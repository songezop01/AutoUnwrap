#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif

#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <commctrl.h>
#include <commdlg.h>
#include <wincrypt.h>
#include <ole2.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <queue>
#include <set>
#include <string>
#include <thread>
#include <vector>

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "ole32.lib")

namespace fs = std::filesystem;

static constexpr UINT WM_APP_LOG = WM_APP + 1;
static constexpr UINT WM_APP_DONE = WM_APP + 2;
static constexpr UINT WM_APP_FIRST_RUN = WM_APP + 3;
static constexpr UINT WM_APP_STATUS = WM_APP + 4;
static constexpr UINT WM_APP_REQUEST_PASSWORD = WM_APP + 5;
static constexpr int MAX_LAYERS = 5;
static constexpr wchar_t APP_VERSION[] = L"3.0.4-settings-delete-fix";

static constexpr int ID_LOG = 1001;
static constexpr int ID_PASSWORDS = 1002;
static constexpr int ID_ENGINE = 1003;
static constexpr int ID_OUTPUT_SETTINGS = 1004;
static constexpr int ID_OPEN_OUTPUT = 1005;
static constexpr int ID_STATUS = 1006;
static constexpr int ID_ENGINE_LABEL = 1007;
static constexpr int ID_OUTPUT_LABEL = 1008;
static constexpr int ID_PROGRESS = 1009;
static constexpr int ID_DIAGNOSTICS = 1010;
static constexpr int ID_DISGUISE_RULES = 1011;

static constexpr int PD_EDIT = 2001;
static constexpr int PD_LIST = 2002;
static constexpr int PD_ADD = 2003;
static constexpr int PD_REMOVE = 2004;
static constexpr int PD_OK = 2005;
static constexpr int PD_CANCEL = 2006;
static constexpr int PD_SELECTED = 2007;

static constexpr int OD_SOURCE = 3001;
static constexpr int OD_FIXED = 3002;
static constexpr int OD_ASK = 3003;
static constexpr int OD_EDIT = 3004;
static constexpr int OD_BROWSE = 3005;
static constexpr int OD_DELETE = 3006;
static constexpr int OD_OK = 3007;
static constexpr int OD_CANCEL = 3008;

static constexpr int RD_EDIT = 4001;
static constexpr int RD_LIST = 4002;
static constexpr int RD_ADD = 4003;
static constexpr int RD_REMOVE = 4004;
static constexpr int RD_OK = 4005;
static constexpr int RD_CANCEL = 4006;

static HINSTANCE g_instance = nullptr;
static HWND g_main = nullptr;
static HWND g_title = nullptr;
static HWND g_subtitle = nullptr;
static HWND g_password_button = nullptr;
static HWND g_engine = nullptr;
static HWND g_output_settings = nullptr;
static HWND g_rules = nullptr;
static HWND g_hint = nullptr;
static HWND g_diagnostics = nullptr;
static HWND g_log = nullptr;
static HWND g_status = nullptr;
static HWND g_engine_label = nullptr;
static HWND g_output_label = nullptr;
static HWND g_open_output = nullptr;
static HWND g_progress = nullptr;
static std::vector<HWND> g_main_controls;
static HFONT g_font = nullptr;
static HFONT g_title_font = nullptr;
static UINT g_dpi = 96;
static double g_ui_scale = 1.0;
static std::atomic_bool g_busy(false);
static std::atomic_bool g_cancel_requested(false);
static std::vector<std::wstring> g_passwords;
static std::mutex g_password_mutex;
static std::wstring g_engine_path;
static fs::path g_output_folder;
static fs::path g_last_output;
static std::mutex g_diagnostic_mutex;
static bool g_diagnostics_initialized = false;
static std::vector<std::wstring> g_disguise_rules;

enum class OutputMode { SourceFolder = 0, FixedFolder = 1, AskEachTime = 2 };
static OutputMode g_output_mode = OutputMode::SourceFolder;
static bool g_delete_after = false;

struct PasswordDialogState {
    std::vector<std::wstring> passwords;
    std::wstring one_time;
    bool accepted = false;
    HWND edit = nullptr;
    HWND list = nullptr;
    HWND selected_label = nullptr;
    int selected_index = -1;
};

struct DisguiseRuleDialogState {
    std::vector<std::wstring> rules;
    bool accepted = false;
    HWND edit = nullptr;
    HWND list = nullptr;
};

struct OutputDialogState {
    OutputMode mode = OutputMode::SourceFolder;
    bool delete_after = false;
    bool accepted = false;
    std::wstring folder;
    HWND fixed_edit = nullptr;
    HWND browse = nullptr;
};

static void ApplyDialogFonts(HWND dialog);
static void LayoutMainWindow(HWND hwnd, int client_width, int client_height);
static void RebuildMainFonts();

static int DpiScale(int value, UINT dpi) {
    return MulDiv(value, static_cast<int>(dpi), 96);
}

static int S(int value) {
    const double scaled = static_cast<double>(value) * static_cast<double>(g_dpi) / 96.0 * g_ui_scale;
    return std::max(1, static_cast<int>(scaled + 0.5));
}

static void RecalculateUiScale() {
    RECT work{};
    if (!SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0)) return;
    const int desired_height = DpiScale(530, g_dpi);
    const int available_height = std::max(420, static_cast<int>(work.bottom - work.top) - 60);
    g_ui_scale = desired_height > available_height
        ? std::max(0.82, static_cast<double>(available_height) / static_cast<double>(desired_height))
        : 1.0;
}

static UINT WindowDpi(HWND hwnd) {
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    using GetDpiForWindowFn = UINT (WINAPI*)(HWND);
    auto get_dpi = reinterpret_cast<GetDpiForWindowFn>(GetProcAddress(user32, "GetDpiForWindow"));
    if (get_dpi && hwnd) return get_dpi(hwnd);
    HDC dc = GetDC(hwnd);
    const int dpi = dc ? GetDeviceCaps(dc, LOGPIXELSX) : 96;
    if (dc) ReleaseDC(hwnd, dc);
    return dpi > 0 ? static_cast<UINT>(dpi) : 96;
}

static void EnableDpiAwareness() {
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    using SetDpiAwarenessContextFn = BOOL (WINAPI*)(HANDLE);
    auto set_context = reinterpret_cast<SetDpiAwarenessContextFn>(GetProcAddress(user32, "SetProcessDpiAwarenessContext"));
    if (set_context && set_context(reinterpret_cast<HANDLE>(-4))) return;
    using SetDpiAwareFn = BOOL (WINAPI*)();
    auto set_aware = reinterpret_cast<SetDpiAwareFn>(GetProcAddress(user32, "SetProcessDPIAware"));
    if (set_aware) set_aware();
}

static std::wstring Lower(std::wstring value) {
    std::transform(value.begin(), value.end(), value.begin(), [](wchar_t c) {
        return static_cast<wchar_t>(towlower(c));
    });
    return value;
}

static bool EndsWithInsensitive(const std::wstring& value, const std::wstring& suffix);
static fs::path UniqueTargetPath(const fs::path& original);

static std::wstring AppDataDir() {
    wchar_t buffer[MAX_PATH] = {};
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, buffer))) {
        return std::wstring(buffer) + L"\\AutoUnwrap";
    }
    wchar_t* env = _wgetenv(L"APPDATA");
    if (env) return std::wstring(env) + L"\\AutoUnwrap";
    return L".\\AutoUnwrap";
}

static void EnsureAppDataDir();
static std::wstring FormatBytes(uintmax_t bytes);
static fs::path DiagnosticFile() { return fs::path(AppDataDir()) / L"diagnostic.log"; }
static fs::path ProcessOutputDirectory() { return fs::path(AppDataDir()) / L"process-output"; }

static fs::path NewProcessOutputFile() {
    EnsureAppDataDir();
    std::error_code ec;
    fs::create_directories(ProcessOutputDirectory(), ec);
    return ProcessOutputDirectory() / (L"process-" + std::to_wstring(GetCurrentProcessId()) + L"-" +
                                       std::to_wstring(GetTickCount64()) + L".log");
}

static std::string WideToUtf8(const std::wstring& value) {
    if (value.empty()) return {};
    const int size = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (size <= 0) return {};
    std::string result(static_cast<size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), size, nullptr, nullptr);
    return result;
}

static std::wstring DiagnosticTimestamp() {
    SYSTEMTIME time{};
    GetLocalTime(&time);
    wchar_t buffer[64] = {};
    swprintf_s(buffer, L"%04u-%02u-%02u %02u:%02u:%02u.%03u", time.wYear, time.wMonth, time.wDay,
               time.wHour, time.wMinute, time.wSecond, time.wMilliseconds);
    return buffer;
}

static std::wstring DiagnosticSafe(const std::wstring& value) {
    std::wstring result = value;
    for (auto& ch : result) if (ch == L'\r' || ch == L'\n') ch = L' ';
    return result;
}

static void WriteDiagnostic(const std::wstring& category, const std::wstring& message) {
    EnsureAppDataDir();
    const std::wstring line = DiagnosticTimestamp() + L" [" + category + L"] [pid=" +
                              std::to_wstring(GetCurrentProcessId()) + L" tid=" +
                              std::to_wstring(GetCurrentThreadId()) + L"] " + DiagnosticSafe(message) + L"\r\n";
    std::lock_guard<std::mutex> lock(g_diagnostic_mutex);
    std::ofstream out(DiagnosticFile(), std::ios::binary | std::ios::app);
    if (!out) return;
    const std::string utf8 = WideToUtf8(line);
    out.write(utf8.data(), static_cast<std::streamsize>(utf8.size()));
}

static std::wstring SystemSnapshot(const fs::path& location = {}) {
    SYSTEM_INFO system{};
    GetNativeSystemInfo(&system);
    MEMORYSTATUSEX memory{sizeof(memory)};
    GlobalMemoryStatusEx(&memory);
    OSVERSIONINFOW os{sizeof(os)};
    GetVersionExW(&os);
    wchar_t windows_dir[MAX_PATH] = {};
    GetWindowsDirectoryW(windows_dir, MAX_PATH);
    ULARGE_INTEGER free_bytes{}, total_bytes{}, total_free{};
    const std::wstring disk_path = location.empty() ? std::wstring(windows_dir) : location.wstring();
    const BOOL disk_ok = GetDiskFreeSpaceExW(disk_path.c_str(), &free_bytes, &total_bytes, &total_free);
    const UINT dpi = g_dpi;
    std::wstring snapshot = L"app_version=" + std::wstring(APP_VERSION) +
        L" os=" + std::to_wstring(os.dwMajorVersion) + L"." + std::to_wstring(os.dwMinorVersion) +
        L" build=" + std::to_wstring(os.dwBuildNumber) + L" arch=" +
        (system.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64 ? L"x64" : L"other") +
        L" cpu=" + std::to_wstring(system.dwNumberOfProcessors) +
        L" ram_total=" + FormatBytes(memory.ullTotalPhys) +
        L" ram_available=" + FormatBytes(memory.ullAvailPhys) +
        L" dpi=" + std::to_wstring(dpi) +
        L" screen=" + std::to_wstring(GetSystemMetrics(SM_CXSCREEN)) + L"x" + std::to_wstring(GetSystemMetrics(SM_CYSCREEN));
    if (disk_ok) snapshot += L" disk_free=" + FormatBytes(free_bytes.QuadPart) + L" disk_total=" + FormatBytes(total_bytes.QuadPart);
    if (!location.empty()) snapshot += L" location=" + location.wstring();
    return snapshot;
}

static void InitializeDiagnostics() {
    if (g_diagnostics_initialized) return;
    g_diagnostics_initialized = true;
    WriteDiagnostic(L"LIFECYCLE", L"Auto Unwrap 啟動；密碼內容不會寫入診斷紀錄");
    WriteDiagnostic(L"SYSTEM", SystemSnapshot());
}

static fs::path PasswordFile() { return fs::path(AppDataDir()) / L"passwords.dat"; }
static fs::path EngineFile() { return fs::path(AppDataDir()) / L"engine.dat"; }
static fs::path OutputModeFile() { return fs::path(AppDataDir()) / L"output_mode.dat"; }
static fs::path OutputFolderFile() { return fs::path(AppDataDir()) / L"output_folder.dat"; }
static fs::path DeleteAfterFile() { return fs::path(AppDataDir()) / L"delete_after.dat"; }
static fs::path SettingsIniFile() { return fs::path(AppDataDir()) / L"settings.ini"; }
static fs::path DisguiseRulesFile() { return fs::path(AppDataDir()) / L"disguise_rules.dat"; }

static void EnsureAppDataDir() {
    std::error_code ec;
    fs::create_directories(fs::path(AppDataDir()), ec);
}

static bool WriteBytes(const fs::path& path, const std::vector<BYTE>& bytes) {
    EnsureAppDataDir();
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    if (!bytes.empty()) out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    return static_cast<bool>(out);
}

static std::vector<BYTE> ReadBytes(const fs::path& path) {
    std::vector<BYTE> result;
    std::ifstream in(path, std::ios::binary);
    if (!in) return result;
    in.seekg(0, std::ios::end);
    const auto size = in.tellg();
    if (size <= 0) return result;
    in.seekg(0, std::ios::beg);
    result.resize(static_cast<size_t>(size));
    in.read(reinterpret_cast<char*>(result.data()), static_cast<std::streamsize>(result.size()));
    if (!in) result.clear();
    return result;
}

static bool SaveWideText(const fs::path& path, const std::wstring& value) {
    std::vector<BYTE> bytes(reinterpret_cast<const BYTE*>(value.data()),
                            reinterpret_cast<const BYTE*>(value.data()) + value.size() * sizeof(wchar_t));
    return WriteBytes(path, bytes);
}

static std::wstring LoadWideText(const fs::path& path) {
    auto bytes = ReadBytes(path);
    if (bytes.empty() || bytes.size() % sizeof(wchar_t) != 0) return L"";
    return std::wstring(reinterpret_cast<wchar_t*>(bytes.data()), bytes.size() / sizeof(wchar_t));
}

static bool SavePasswords(const std::vector<std::wstring>& passwords) {
    EnsureAppDataDir();
    if (passwords.empty()) {
        DeleteFileW(PasswordFile().wstring().c_str());
        return true;
    }
    std::wstring serialized;
    for (const auto& password : passwords) {
        serialized += password;
        serialized.push_back(L'\n');
    }
    DATA_BLOB input{};
    input.cbData = static_cast<DWORD>(serialized.size() * sizeof(wchar_t));
    input.pbData = reinterpret_cast<BYTE*>(serialized.data());
    DATA_BLOB protected_blob{};
    if (!CryptProtectData(&input, L"AutoUnwrap passwords", nullptr, nullptr, nullptr,
                          CRYPTPROTECT_UI_FORBIDDEN, &protected_blob)) {
        return false;
    }
    std::vector<BYTE> bytes(protected_blob.pbData, protected_blob.pbData + protected_blob.cbData);
    LocalFree(protected_blob.pbData);
    return WriteBytes(PasswordFile(), bytes);
}

static std::vector<std::wstring> LoadPasswords() {
    std::vector<std::wstring> result;
    auto bytes = ReadBytes(PasswordFile());
    if (bytes.empty()) return result;
    DATA_BLOB input{static_cast<DWORD>(bytes.size()), bytes.data()};
    DATA_BLOB unprotected{};
    if (!CryptUnprotectData(&input, nullptr, nullptr, nullptr, nullptr,
                            CRYPTPROTECT_UI_FORBIDDEN, &unprotected)) {
        return result;
    }
    if (unprotected.cbData % sizeof(wchar_t) == 0) {
        std::wstring serialized(reinterpret_cast<wchar_t*>(unprotected.pbData),
                                unprotected.cbData / sizeof(wchar_t));
        size_t start = 0;
        while (start <= serialized.size()) {
            const size_t end = serialized.find(L'\n', start);
            const std::wstring item = serialized.substr(start, end == std::wstring::npos ? end : end - start);
            if (!item.empty() && std::find(result.begin(), result.end(), item) == result.end()) result.push_back(item);
            if (end == std::wstring::npos) break;
            start = end + 1;
        }
    }
    LocalFree(unprotected.pbData);
    return result;
}

static std::wstring ReadSetting(const wchar_t* key, const std::wstring& fallback) {
    wchar_t buffer[32768] = {};
    GetPrivateProfileStringW(L"AutoUnwrap", key, fallback.c_str(), buffer,
                             static_cast<DWORD>(std::size(buffer)), SettingsIniFile().wstring().c_str());
    return buffer;
}

static bool WriteSetting(const wchar_t* key, const std::wstring& value) {
    return WritePrivateProfileStringW(L"AutoUnwrap", key, value.c_str(), SettingsIniFile().wstring().c_str()) != 0;
}

static void LoadSettings() {
    g_engine_path = LoadWideText(EngineFile());
    const std::wstring mode = LoadWideText(OutputModeFile());
    if (mode == L"1") g_output_mode = OutputMode::FixedFolder;
    else if (mode == L"2") g_output_mode = OutputMode::AskEachTime;
    else g_output_mode = OutputMode::SourceFolder;
    g_output_folder = LoadWideText(OutputFolderFile());
    g_delete_after = LoadWideText(DeleteAfterFile()) == L"1";

    // INI is the authoritative copy for settings written by v3.0; legacy files remain as fallback.
    const std::wstring ini_engine = ReadSetting(L"engine", g_engine_path);
    const std::wstring ini_mode = ReadSetting(L"output_mode", std::to_wstring(static_cast<int>(g_output_mode)));
    const std::wstring ini_folder = ReadSetting(L"output_folder", g_output_folder.wstring());
    const std::wstring ini_delete = ReadSetting(L"delete_after", g_delete_after ? L"1" : L"0");
    g_engine_path = ini_engine;
    if (ini_mode == L"1") g_output_mode = OutputMode::FixedFolder;
    else if (ini_mode == L"2") g_output_mode = OutputMode::AskEachTime;
    else g_output_mode = OutputMode::SourceFolder;
    g_output_folder = ini_folder;
    g_delete_after = ini_delete == L"1";
}

static bool SaveSettings() {
    EnsureAppDataDir();
    const std::wstring mode = std::to_wstring(static_cast<int>(g_output_mode));
    const std::wstring folder = g_output_folder.wstring();
    const std::wstring delete_after = g_delete_after ? L"1" : L"0";

    const bool legacy_ok =
        SaveWideText(EngineFile(), g_engine_path) &&
        SaveWideText(OutputModeFile(), mode) &&
        SaveWideText(OutputFolderFile(), folder) &&
        SaveWideText(DeleteAfterFile(), delete_after);

    const bool ini_ok =
        WriteSetting(L"engine", g_engine_path) &&
        WriteSetting(L"output_mode", mode) &&
        WriteSetting(L"output_folder", folder) &&
        WriteSetting(L"delete_after", delete_after);

    const bool ok = legacy_ok && ini_ok;
    WriteDiagnostic(L"SETTINGS_SAVE",
        L"result=" + std::wstring(ok ? L"success" : L"failure") +
        L"；dir=" + fs::path(AppDataDir()).wstring() +
        L"；output_mode=" + mode +
        L"；delete_after=" + delete_after);
    return ok;
}

static void LoadDisguiseRules() {
    g_disguise_rules.clear();
    const std::wstring serialized = LoadWideText(DisguiseRulesFile());
    size_t start = 0;
    while (start <= serialized.size()) {
        const size_t end = serialized.find(L'\n', start);
        std::wstring item = serialized.substr(start, end == std::wstring::npos ? end : end - start);
        while (!item.empty() && (item.back() == L'\r' || item.back() == L' ' || item.back() == L'\t')) item.pop_back();
        if (!item.empty() && std::find(g_disguise_rules.begin(), g_disguise_rules.end(), item) == g_disguise_rules.end()) {
            g_disguise_rules.push_back(item);
        }
        if (end == std::wstring::npos) break;
        start = end + 1;
    }
}

static void SaveDisguiseRules() {
    std::wstring serialized;
    for (const auto& rule : g_disguise_rules) {
        if (!rule.empty()) serialized += rule + L"\n";
    }
    SaveWideText(DisguiseRulesFile(), serialized);
}

static void AppendLog(const std::wstring& text) {
    WriteDiagnostic(L"LOG", text);
    if (!g_main) return;
    PostMessageW(g_main, WM_APP_LOG, 0, reinterpret_cast<LPARAM>(new std::wstring(text)));
}

static void SetStatus(const std::wstring& text) {
    WriteDiagnostic(L"STATUS", text);
    if (g_status) SetWindowTextW(g_status, text.c_str());
}

static std::wstring DiagnosticReportHeader() {
    std::wstring header = L"Auto Unwrap 診斷報告\r\n版本：" + std::wstring(APP_VERSION) + L"\r\n";
    header += L"此報告不包含任何密碼內容；密碼只以候選數量及長度摘要表示。\r\n";
    header += L"產生時間：" + DiagnosticTimestamp() + L"\r\n";
    header += L"系統狀態：" + SystemSnapshot() + L"\r\n";
    header += L"完整工具輸出紀錄資料夾：" + ProcessOutputDirectory().wstring() + L"\r\n";
    header += L"目前解壓工具：" + (g_engine_path.empty() ? L"未設定" : g_engine_path) + L"\r\n";
    header += L"輸出模式：" + std::to_wstring(static_cast<int>(g_output_mode)) +
              L"；完成後刪除：" + (g_delete_after ? L"是" : L"否") + L"\r\n";
    {
        std::lock_guard<std::mutex> lock(g_password_mutex);
        header += L"已保存密碼數量：" + std::to_wstring(g_passwords.size()) + L"（不包含密碼內容）\r\n";
    }
    header += L"\r\n================ 事件紀錄 ================\r\n";
    return header;
}

static bool ExportDiagnosticReport(HWND owner) {
    WriteDiagnostic(L"USER_ACTION", L"使用者要求匯出診斷報告");
    SYSTEMTIME now{};
    GetLocalTime(&now);
    wchar_t default_name[128] = {};
    swprintf_s(default_name, L"AutoUnwrap-diagnostic-%04u%02u%02u-%02u%02u%02u.txt",
               now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond);
    std::vector<wchar_t> filename(32768, L'\0');
    wcsncpy_s(filename.data(), filename.size(), default_name, _TRUNCATE);
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.lpstrFilter = L"診斷報告 (*.txt)\\0*.txt\\0所有檔案 (*.*)\\0*.*\\0\\0";
    ofn.lpstrFile = filename.data();
    ofn.nMaxFile = static_cast<DWORD>(filename.size());
    ofn.lpstrDefExt = L"txt";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    if (!GetSaveFileNameW(&ofn)) {
        WriteDiagnostic(L"USER_ACTION", L"使用者取消匯出診斷報告");
        return false;
    }
    std::ofstream out(fs::path(filename.data()), std::ios::binary | std::ios::trunc);
    if (!out) {
        WriteDiagnostic(L"ERROR", L"無法建立診斷報告檔：" + std::wstring(filename.data()));
        MessageBoxW(owner, L"無法建立診斷報告檔，請選擇可寫入的資料夾。", L"診斷報告", MB_OK | MB_ICONERROR);
        return false;
    }
    const std::string header = WideToUtf8(DiagnosticReportHeader());
    out.write(header.data(), static_cast<std::streamsize>(header.size()));
    const std::vector<BYTE> raw = ReadBytes(DiagnosticFile());
    if (!raw.empty()) out.write(reinterpret_cast<const char*>(raw.data()), static_cast<std::streamsize>(raw.size()));
    out.close();
    WriteDiagnostic(L"USER_ACTION", L"診斷報告已匯出：" + std::wstring(filename.data()));
    MessageBoxW(owner, L"診斷報告已匯出。請將該 TXT 檔案附加到錯誤回報；報告不包含密碼內容。", L"診斷報告", MB_OK | MB_ICONINFORMATION);
    return true;
}

static void FlashWorkWindow() {
    if (!g_main || IsWindowVisible(g_main) == FALSE) return;
    FLASHWINFO flash{sizeof(flash), g_main, FLASHW_TRAY | FLASHW_TIMERNOFG, 2, 0};
    FlashWindowEx(&flash);
}

static void PostStatus(const std::wstring& text) {
    WriteDiagnostic(L"STATUS", text);
    if (g_main) PostMessageW(g_main, WM_APP_STATUS, 0, reinterpret_cast<LPARAM>(new std::wstring(text)));
}

static void UpdatePasswordSelection(PasswordDialogState* state) {
    if (!state || !state->list) return;
    const LRESULT index = SendMessageW(state->list, LB_GETCURSEL, 0, 0);
    state->selected_index = index == LB_ERR ? -1 : static_cast<int>(index);
    if (!state->selected_label) return;
    if (state->selected_index >= 0 && static_cast<size_t>(state->selected_index) < state->passwords.size()) {
        const std::wstring text = L"目前選擇（按確定後優先使用）：" + state->passwords[state->selected_index];
        SetWindowTextW(state->selected_label, text.c_str());
        SetWindowTextW(state->edit, state->passwords[state->selected_index].c_str());
    } else {
        SetWindowTextW(state->selected_label, L"目前未選擇清單密碼；可使用上方輸入框的一次性密碼");
    }
    InvalidateRect(state->list, nullptr, TRUE);
}

static void RefreshPasswordList(PasswordDialogState* state) {
    if (!state || !state->list) return;
    SendMessageW(state->list, LB_RESETCONTENT, 0, 0);
    for (const auto& password : state->passwords) {
        SendMessageW(state->list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(password.c_str()));
    }
    if (state->selected_index >= 0 && static_cast<size_t>(state->selected_index) < state->passwords.size()) {
        SendMessageW(state->list, LB_SETCURSEL, state->selected_index, 0);
    }
    UpdatePasswordSelection(state);
}

static LRESULT CALLBACK PasswordDialogProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    auto* state = reinterpret_cast<PasswordDialogState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    switch (message) {
        case WM_NCCREATE: {
            auto* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
            return TRUE;
        }
        case WM_CREATE: {
            state = reinterpret_cast<PasswordDialogState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
            const HINSTANCE instance = g_instance;
            CreateWindowW(L"STATIC", L"可直接輸入密碼，或點選清單密碼；按「確定並使用」會使用輸入框目前顯示的文字。",
                          WS_CHILD | WS_VISIBLE | SS_ENDELLIPSIS, S(18), S(15), S(520), S(25), hwnd, nullptr, instance, nullptr);
            state->edit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                          WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                          S(18), S(50), S(360), S(30), hwnd, reinterpret_cast<HMENU>(PD_EDIT), instance, nullptr);
            CreateWindowW(L"BUTTON", L"加入常用",
                          WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, S(390), S(50), S(110), S(30),
                          hwnd, reinterpret_cast<HMENU>(PD_ADD), instance, nullptr);
            CreateWindowW(L"STATIC", L"已保存的密碼（明碼顯示）：",
                          WS_CHILD | WS_VISIBLE, S(18), S(93), S(350), S(22), hwnd, nullptr, instance, nullptr);
            state->list = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", L"",
                          WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOTIFY | LBS_OWNERDRAWFIXED | LBS_HASSTRINGS | LBS_NOINTEGRALHEIGHT,
                          S(18), S(118), S(520), S(135), hwnd, reinterpret_cast<HMENU>(PD_LIST), instance, nullptr);
            state->selected_label = CreateWindowW(L"STATIC", L"目前未選擇清單密碼；可使用上方輸入框的一次性密碼",
                          WS_CHILD | WS_VISIBLE | SS_ENDELLIPSIS, S(18), S(258), S(520), S(24),
                          hwnd, reinterpret_cast<HMENU>(PD_SELECTED), instance, nullptr);
            CreateWindowW(L"BUTTON", L"移除選取",
                          WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, S(18), S(292), S(110), S(30),
                          hwnd, reinterpret_cast<HMENU>(PD_REMOVE), instance, nullptr);
            CreateWindowW(L"BUTTON", L"確定並使用",
                          WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, S(310), S(292), S(100), S(30),
                          hwnd, reinterpret_cast<HMENU>(PD_OK), instance, nullptr);
            CreateWindowW(L"BUTTON", L"取消",
                          WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, S(420), S(292), S(100), S(30),
                          hwnd, reinterpret_cast<HMENU>(PD_CANCEL), instance, nullptr);
            RefreshPasswordList(state);
            SendMessageW(state->list, LB_SETITEMHEIGHT, 0, S(28));
            ApplyDialogFonts(hwnd);
            return 0;
        }
        case WM_DRAWITEM: {
            if (wparam == PD_LIST) {
                const auto* draw = reinterpret_cast<LPDRAWITEMSTRUCT>(lparam);
                if (draw->itemID == static_cast<UINT>(-1)) return TRUE;
                wchar_t text[4096] = {};
                SendMessageW(state->list, LB_GETTEXT, draw->itemID, reinterpret_cast<LPARAM>(text));
                const bool selected = (draw->itemState & ODS_SELECTED) != 0;
                const COLORREF background = selected ? RGB(0, 120, 215) : RGB(255, 255, 255);
                const COLORREF foreground = selected ? RGB(255, 255, 255) : RGB(30, 30, 30);
                HBRUSH brush = CreateSolidBrush(background);
                FillRect(draw->hDC, &draw->rcItem, brush);
                DeleteObject(brush);
                SetBkMode(draw->hDC, TRANSPARENT);
                SetTextColor(draw->hDC, foreground);
                RECT text_rect = draw->rcItem;
                text_rect.left += 12;
                DrawTextW(draw->hDC, text, -1, &text_rect, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
                if (selected) {
                    RECT mark = draw->rcItem;
                    mark.right = mark.left + 4;
                    HBRUSH mark_brush = CreateSolidBrush(RGB(255, 210, 70));
                    FillRect(draw->hDC, &mark, mark_brush);
                    DeleteObject(mark_brush);
                }
                return TRUE;
            }
            break;
        }
        case WM_MEASUREITEM:
            if (wparam == PD_LIST) {
                auto* measure = reinterpret_cast<LPMEASUREITEMSTRUCT>(lparam);
                measure->itemHeight = 28;
                return TRUE;
            }
            break;
        case WM_COMMAND: {
            if (!state) return 0;
            const int id = LOWORD(wparam);
            if (id == PD_LIST && HIWORD(wparam) == LBN_SELCHANGE) {
                UpdatePasswordSelection(state);
            } else if (id == PD_ADD) {
                wchar_t buffer[4096] = {};
                GetWindowTextW(state->edit, buffer, 4096);
                std::wstring value(buffer);
                if (!value.empty() && std::find(state->passwords.begin(), state->passwords.end(), value) == state->passwords.end()) {
                    state->passwords.push_back(value);
                    state->selected_index = static_cast<int>(state->passwords.size() - 1);
                    RefreshPasswordList(state);
                }
            } else if (id == PD_REMOVE) {
                const LRESULT index = SendMessageW(state->list, LB_GETCURSEL, 0, 0);
                if (index != LB_ERR && static_cast<size_t>(index) < state->passwords.size()) {
                    state->passwords.erase(state->passwords.begin() + index);
                    state->selected_index = -1;
                    RefreshPasswordList(state);
                }
            } else if (id == PD_OK) {
                wchar_t buffer[4096] = {};
                GetWindowTextW(state->edit, buffer, 4096);
                state->one_time = buffer;
                state->accepted = true;
                SavePasswords(state->passwords);
                DestroyWindow(hwnd);
            } else if (id == PD_CANCEL) {
                DestroyWindow(hwnd);
            }
            return 0;
        }
        case WM_CLOSE:
            // Treat the window's X button as "儲存設定並關閉". The previous
            // build had OK/CANCEL handlers but accidentally created neither
            // button, so closing the settings window discarded the checkbox
            // state and g_delete_after stayed false for the next job.
            if (state) {
                wchar_t buffer[MAX_PATH * 4] = {};
                GetWindowTextW(state->fixed_edit, buffer, MAX_PATH * 4);
                state->folder = buffer;
                state->delete_after = SendMessageW(GetDlgItem(hwnd, OD_DELETE), BM_GETCHECK, 0, 0) == BST_CHECKED;
                state->accepted = true;
            }
            DestroyWindow(hwnd);
            return 0;
    }
    return DefWindowProcW(hwnd, message, wparam, lparam);
}

static bool ShowPasswordDialog(HWND owner, std::wstring& one_time) {
    WriteDiagnostic(L"USER_ACTION", L"開啟密碼管理視窗");
    PasswordDialogState state;
    {
        std::lock_guard<std::mutex> lock(g_password_mutex);
        state.passwords = g_passwords;
    }
    static bool registered = false;
    if (!registered) {
        WNDCLASSW wc{};
        wc.lpfnWndProc = PasswordDialogProc;
        wc.hInstance = g_instance;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        wc.lpszClassName = L"AutoUnwrapPasswordDialog";
        RegisterClassW(&wc);
        registered = true;
    }
    g_dpi = WindowDpi(owner);
    RECT owner_rect{};
    GetWindowRect(owner, &owner_rect);
    const int width = S(560), height = S(360);
    const int x = owner_rect.left + ((owner_rect.right - owner_rect.left) - width) / 2;
    const int y = owner_rect.top + ((owner_rect.bottom - owner_rect.top) - height) / 2;
    HWND dialog = CreateWindowExW(WS_EX_DLGMODALFRAME, L"AutoUnwrapPasswordDialog", L"密碼管理",
                                  WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
                                  x, y, width, height, owner, nullptr, g_instance, &state);
    if (!dialog) return false;
    EnableWindow(owner, FALSE);
    SetForegroundWindow(dialog);
    MSG msg{};
    while (IsWindow(dialog) && GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (!IsDialogMessageW(dialog, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    EnableWindow(owner, TRUE);
    SetForegroundWindow(owner);
    if (state.accepted) {
        std::lock_guard<std::mutex> lock(g_password_mutex);
        g_passwords = state.passwords;
        one_time = state.one_time;
        WriteDiagnostic(L"USER_ACTION", L"密碼設定已確認；一次性密碼長度=" + std::to_wstring(one_time.size()) +
                        L"；保存候選數量=" + std::to_wstring(g_passwords.size()) + L"；未記錄密碼內容");
        return true;
    }
    WriteDiagnostic(L"USER_ACTION", L"使用者取消密碼設定");
    return false;
}

static void RefreshRuleList(DisguiseRuleDialogState* state) {
    if (!state || !state->list) return;
    SendMessageW(state->list, LB_RESETCONTENT, 0, 0);
    for (const auto& rule : state->rules) {
        SendMessageW(state->list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(rule.c_str()));
    }
}

static LRESULT CALLBACK DisguiseRuleDialogProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    auto* state = reinterpret_cast<DisguiseRuleDialogState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    switch (message) {
        case WM_NCCREATE: {
            auto* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
            return TRUE;
        }
        case WM_CREATE: {
            state = reinterpret_cast<DisguiseRuleDialogState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
            const HINSTANCE instance = g_instance;
            CreateWindowW(L"STATIC", L"輸入要從偽裝檔名移除的固定文字；程式仍會先檢查檔案內容確實是壓縮檔。",
                          WS_CHILD | WS_VISIBLE, S(18), S(15), S(520), S(24), hwnd, nullptr, instance, nullptr);
            CreateWindowW(L"STATIC", L"例：XXXXX.rar請刪除文字後解壓 → 規則輸入：請刪除文字後解壓",
                          WS_CHILD | WS_VISIBLE, S(18), S(42), S(520), S(24), hwnd, nullptr, instance, nullptr);
            state->edit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                          WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                          S(18), S(76), S(370), S(30), hwnd, reinterpret_cast<HMENU>(RD_EDIT), instance, nullptr);
            CreateWindowW(L"BUTTON", L"加入規則",
                          WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, S(400), S(76), S(140), S(30),
                          hwnd, reinterpret_cast<HMENU>(RD_ADD), instance, nullptr);
            CreateWindowW(L"STATIC", L"已保存的偽裝檔名規則：",
                          WS_CHILD | WS_VISIBLE, S(18), S(119), S(300), S(22), hwnd, nullptr, instance, nullptr);
            state->list = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", L"",
                          WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOTIFY,
                          S(18), S(145), S(522), S(125), hwnd, reinterpret_cast<HMENU>(RD_LIST), instance, nullptr);
            CreateWindowW(L"BUTTON", L"移除選取規則",
                          WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, S(18), S(286), S(140), S(30),
                          hwnd, reinterpret_cast<HMENU>(RD_REMOVE), instance, nullptr);
            CreateWindowW(L"BUTTON", L"確定並使用",
                          WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, S(340), S(286), S(100), S(30),
                          hwnd, reinterpret_cast<HMENU>(RD_OK), instance, nullptr);
            CreateWindowW(L"BUTTON", L"取消",
                          WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, S(440), S(286), S(100), S(30),
                          hwnd, reinterpret_cast<HMENU>(RD_CANCEL), instance, nullptr);
            RefreshRuleList(state);
            ApplyDialogFonts(hwnd);
            return 0;
        }
        case WM_COMMAND: {
            if (!state) return 0;
            const int id = LOWORD(wparam);
            if (id == RD_ADD) {
                wchar_t buffer[4096] = {};
                GetWindowTextW(state->edit, buffer, 4096);
                std::wstring value(buffer);
                if (!value.empty() && std::find(state->rules.begin(), state->rules.end(), value) == state->rules.end()) {
                    state->rules.push_back(value);
                    SetWindowTextW(state->edit, L"");
                    RefreshRuleList(state);
                }
            } else if (id == RD_REMOVE) {
                const LRESULT index = SendMessageW(state->list, LB_GETCURSEL, 0, 0);
                if (index != LB_ERR && static_cast<size_t>(index) < state->rules.size()) {
                    state->rules.erase(state->rules.begin() + index);
                    RefreshRuleList(state);
                }
            } else if (id == RD_OK) {
                state->accepted = true;
                DestroyWindow(hwnd);
            } else if (id == RD_CANCEL) {
                DestroyWindow(hwnd);
            }
            return 0;
        }
        case WM_CLOSE:
            // Treat the window's X button as "儲存設定並關閉". The previous
            // build had OK/CANCEL handlers but accidentally created neither
            // button, so closing the settings window discarded the checkbox
            // state and g_delete_after stayed false for the next job.
            if (state) {
                wchar_t buffer[MAX_PATH * 4] = {};
                GetWindowTextW(state->fixed_edit, buffer, MAX_PATH * 4);
                state->folder = buffer;
                state->delete_after = SendMessageW(GetDlgItem(hwnd, OD_DELETE), BM_GETCHECK, 0, 0) == BST_CHECKED;
                state->accepted = true;
            }
            DestroyWindow(hwnd);
            return 0;
    }
    return DefWindowProcW(hwnd, message, wparam, lparam);
}

static bool ShowDisguiseRulesDialog(HWND owner) {
    WriteDiagnostic(L"USER_ACTION", L"開啟偽裝檔名規則視窗");
    DisguiseRuleDialogState state;
    state.rules = g_disguise_rules;
    static bool registered = false;
    if (!registered) {
        WNDCLASSW wc{};
        wc.lpfnWndProc = DisguiseRuleDialogProc;
        wc.hInstance = g_instance;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        wc.lpszClassName = L"AutoUnwrapDisguiseRuleDialog";
        RegisterClassW(&wc);
        registered = true;
    }
    g_dpi = WindowDpi(owner);
    RECT owner_rect{};
    GetWindowRect(owner, &owner_rect);
    const int width = S(575), height = S(360);
    const int x = owner_rect.left + ((owner_rect.right - owner_rect.left) - width) / 2;
    const int y = owner_rect.top + ((owner_rect.bottom - owner_rect.top) - height) / 2;
    HWND dialog = CreateWindowExW(WS_EX_DLGMODALFRAME, L"AutoUnwrapDisguiseRuleDialog", L"偽裝檔名規則",
                                  WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
                                  x, y, width, height, owner, nullptr, g_instance, &state);
    if (!dialog) return false;
    EnableWindow(owner, FALSE);
    SetForegroundWindow(dialog);
    MSG msg{};
    while (IsWindow(dialog) && GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (!IsDialogMessageW(dialog, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    EnableWindow(owner, TRUE);
    SetForegroundWindow(owner);
    if (!state.accepted) {
        WriteDiagnostic(L"USER_ACTION", L"使用者取消偽裝檔名規則設定");
        return false;
    }
    g_disguise_rules = state.rules;
    SaveDisguiseRules();
    WriteDiagnostic(L"USER_ACTION", L"偽裝檔名規則已保存；rule_count=" + std::to_wstring(g_disguise_rules.size()));
    return true;
}

struct PasswordRequest {
    std::mutex mutex;
    std::condition_variable condition;
    bool done = false;
    bool accepted = false;
    std::wstring password;
};

static bool RequestAdditionalPassword(const fs::path& archive, int depth, std::wstring& password) {
    if (!g_main) return false;
    PasswordRequest request;
    WriteDiagnostic(L"PASSWORD_REQUEST", L"depth=" + std::to_wstring(depth + 1) + L"；archive=" + archive.wstring());
    if (!PostMessageW(g_main, WM_APP_REQUEST_PASSWORD, 0, reinterpret_cast<LPARAM>(&request))) return false;
    std::unique_lock<std::mutex> lock(request.mutex);
    if (!request.condition.wait_for(lock, std::chrono::minutes(10), [&request] { return request.done; })) {
        WriteDiagnostic(L"PASSWORD_REQUEST", L"結果=timeout");
        return false;
    }
    if (!request.accepted || request.password.empty()) {
        WriteDiagnostic(L"PASSWORD_REQUEST", L"結果=cancelled_or_empty");
        return false;
    }
    password = request.password;
    WriteDiagnostic(L"PASSWORD_REQUEST", L"結果=accepted；password_length=" + std::to_wstring(password.size()) + L"；password_value=REDACTED");
    return true;
}

static bool ChooseFolder(HWND owner, const fs::path& initial, fs::path& result) {
    IFileDialog* dialog = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                                   IID_PPV_ARGS(&dialog));
    if (FAILED(hr) || !dialog) return false;
    DWORD options = 0;
    dialog->GetOptions(&options);
    dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
    dialog->SetTitle(L"選擇固定輸出資料夾");
    if (!initial.empty() && fs::exists(initial)) {
        IShellItem* initial_item = nullptr;
        if (SUCCEEDED(SHCreateItemFromParsingName(initial.wstring().c_str(), nullptr, IID_PPV_ARGS(&initial_item)))) {
            dialog->SetFolder(initial_item);
            initial_item->Release();
        }
    }
    hr = dialog->Show(owner);
    if (FAILED(hr)) {
        dialog->Release();
        return false;
    }
    IShellItem* item = nullptr;
    hr = dialog->GetResult(&item);
    if (FAILED(hr) || !item) {
        dialog->Release();
        return false;
    }
    PWSTR path = nullptr;
    hr = item->GetDisplayName(SIGDN_FILESYSPATH, &path);
    if (SUCCEEDED(hr) && path) {
        result = fs::path(path);
        CoTaskMemFree(path);
    }
    item->Release();
    dialog->Release();
    return !result.empty();
}

static void UpdateOutputDialogState(OutputDialogState* state) {
    if (!state) return;
    EnableWindow(state->fixed_edit, state->mode == OutputMode::FixedFolder);
    EnableWindow(state->browse, state->mode == OutputMode::FixedFolder);
}

static LRESULT CALLBACK OutputDialogProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    auto* state = reinterpret_cast<OutputDialogState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    switch (message) {
        case WM_NCCREATE: {
            auto* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
            return TRUE;
        }
        case WM_CREATE: {
            state = reinterpret_cast<OutputDialogState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
            const HINSTANCE instance = g_instance;
            CreateWindowW(L"STATIC", L"輸出位置：",
                          WS_CHILD | WS_VISIBLE, S(18), S(15), S(120), S(22), hwnd, nullptr, instance, nullptr);
            HWND source = CreateWindowW(L"BUTTON", L"原始檔案所在資料夾",
                          WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON | WS_GROUP, S(18), S(45), S(220), S(26),
                          hwnd, reinterpret_cast<HMENU>(OD_SOURCE), instance, nullptr);
            HWND fixed = CreateWindowW(L"BUTTON", L"固定輸出資料夾",
                          WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON, S(18), S(75), S(220), S(26),
                          hwnd, reinterpret_cast<HMENU>(OD_FIXED), instance, nullptr);
            HWND ask = CreateWindowW(L"BUTTON", L"每次處理時詢問",
                          WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON, S(18), S(105), S(220), S(26),
                          hwnd, reinterpret_cast<HMENU>(OD_ASK), instance, nullptr);
            state->fixed_edit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", state->folder.c_str(),
                          WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_READONLY,
                          S(245), S(75), S(300), S(28), hwnd, reinterpret_cast<HMENU>(OD_EDIT), instance, nullptr);
            state->browse = CreateWindowW(L"BUTTON", L"瀏覽...",
                          WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, S(445), S(75), S(100), S(28),
                          hwnd, reinterpret_cast<HMENU>(OD_BROWSE), instance, nullptr);
            CreateWindowW(L"BUTTON", L"完成後自動刪除已成功解壓的壓縮檔",
                          WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                          S(18), S(150), S(520), S(26), hwnd, reinterpret_cast<HMENU>(OD_DELETE), instance, nullptr);
            CreateWindowW(L"BUTTON", L"儲存設定並關閉",
                          WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                          S(330), S(188), S(145), S(30), hwnd, reinterpret_cast<HMENU>(OD_OK), instance, nullptr);
            CreateWindowW(L"BUTTON", L"取消",
                          WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                          S(485), S(188), S(60), S(30), hwnd, reinterpret_cast<HMENU>(OD_CANCEL), instance, nullptr);
            SendMessageW(source, BM_SETCHECK, state->mode == OutputMode::SourceFolder ? BST_CHECKED : BST_UNCHECKED, 0);
            SendMessageW(fixed, BM_SETCHECK, state->mode == OutputMode::FixedFolder ? BST_CHECKED : BST_UNCHECKED, 0);
            SendMessageW(ask, BM_SETCHECK, state->mode == OutputMode::AskEachTime ? BST_CHECKED : BST_UNCHECKED, 0);
            SendMessageW(GetDlgItem(hwnd, OD_DELETE), BM_SETCHECK, state->delete_after ? BST_CHECKED : BST_UNCHECKED, 0);
            UpdateOutputDialogState(state);
            ApplyDialogFonts(hwnd);
            return 0;
        }
        case WM_COMMAND: {
            if (!state) return 0;
            const int id = LOWORD(wparam);
            if (id == OD_SOURCE || id == OD_FIXED || id == OD_ASK) {
                if (id == OD_SOURCE) state->mode = OutputMode::SourceFolder;
                else if (id == OD_FIXED) {
                    state->mode = OutputMode::FixedFolder;
                    fs::path selected;
                    if (ChooseFolder(hwnd, state->folder, selected)) {
                        state->folder = selected.wstring();
                        SetWindowTextW(state->fixed_edit, state->folder.c_str());
                    }
                } else state->mode = OutputMode::AskEachTime;
                SendMessageW(GetDlgItem(hwnd, OD_SOURCE), BM_SETCHECK, id == OD_SOURCE ? BST_CHECKED : BST_UNCHECKED, 0);
                SendMessageW(GetDlgItem(hwnd, OD_FIXED), BM_SETCHECK, id == OD_FIXED ? BST_CHECKED : BST_UNCHECKED, 0);
                SendMessageW(GetDlgItem(hwnd, OD_ASK), BM_SETCHECK, id == OD_ASK ? BST_CHECKED : BST_UNCHECKED, 0);
                UpdateOutputDialogState(state);
            } else if (id == OD_DELETE && HIWORD(wparam) == BN_CLICKED) {
                state->delete_after = SendMessageW(GetDlgItem(hwnd, OD_DELETE), BM_GETCHECK, 0, 0) == BST_CHECKED;
                WriteDiagnostic(L"USER_ACTION", L"輸出設定中切換完成後自動刪除：" + std::wstring(state->delete_after ? L"是" : L"否"));
            } else if (id == OD_BROWSE) {
                fs::path selected;
                if (ChooseFolder(hwnd, state->folder, selected)) {
                    state->folder = selected.wstring();
                    SetWindowTextW(state->fixed_edit, state->folder.c_str());
                }
            } else if (id == OD_OK) {
                wchar_t buffer[MAX_PATH * 4] = {};
                GetWindowTextW(state->fixed_edit, buffer, MAX_PATH * 4);
                state->folder = buffer;
                if (state->mode == OutputMode::FixedFolder && state->folder.empty()) {
                    MessageBoxW(hwnd, L"請先選擇固定輸出資料夾。", L"輸出設定", MB_OK | MB_ICONWARNING);
                    return 0;
                }
                state->delete_after = SendMessageW(GetDlgItem(hwnd, OD_DELETE), BM_GETCHECK, 0, 0) == BST_CHECKED;
                state->accepted = true;
                DestroyWindow(hwnd);
            } else if (id == OD_CANCEL) {
                DestroyWindow(hwnd);
            }
            return 0;
        }
        case WM_CLOSE:
            // Treat the window's X button as "儲存設定並關閉". The previous
            // build had OK/CANCEL handlers but accidentally created neither
            // button, so closing the settings window discarded the checkbox
            // state and g_delete_after stayed false for the next job.
            if (state) {
                wchar_t buffer[MAX_PATH * 4] = {};
                GetWindowTextW(state->fixed_edit, buffer, MAX_PATH * 4);
                state->folder = buffer;
                state->delete_after = SendMessageW(GetDlgItem(hwnd, OD_DELETE), BM_GETCHECK, 0, 0) == BST_CHECKED;
                state->accepted = true;
            }
            DestroyWindow(hwnd);
            return 0;
    }
    return DefWindowProcW(hwnd, message, wparam, lparam);
}

static bool ShowOutputDialog(HWND owner) {
    OutputDialogState state;
    state.mode = g_output_mode;
    state.folder = g_output_folder.wstring();
    state.delete_after = g_delete_after;
    static bool registered = false;
    if (!registered) {
        WNDCLASSW wc{};
        wc.lpfnWndProc = OutputDialogProc;
        wc.hInstance = g_instance;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        wc.lpszClassName = L"AutoUnwrapOutputDialog";
        RegisterClassW(&wc);
        registered = true;
    }
    g_dpi = WindowDpi(owner);
    RECT owner_rect{};
    GetWindowRect(owner, &owner_rect);
    const int width = S(590), height = S(245);
    const int x = owner_rect.left + ((owner_rect.right - owner_rect.left) - width) / 2;
    const int y = owner_rect.top + ((owner_rect.bottom - owner_rect.top) - height) / 2;
    HWND dialog = CreateWindowExW(WS_EX_DLGMODALFRAME, L"AutoUnwrapOutputDialog", L"輸出與刪除設定",
                                  WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
                                  x, y, width, height, owner, nullptr, g_instance, &state);
    if (!dialog) return false;
    EnableWindow(owner, FALSE);
    SetForegroundWindow(dialog);
    MSG msg{};
    while (IsWindow(dialog) && GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (!IsDialogMessageW(dialog, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    EnableWindow(owner, TRUE);
    SetForegroundWindow(owner);
    if (state.accepted) {
        g_output_mode = state.mode;
        g_output_folder = state.folder;
        g_delete_after = state.delete_after;
        const bool save_ok = SaveSettings();
        const bool saved_delete_after = ReadSetting(L"delete_after", L"0") == L"1";
        WriteDiagnostic(L"USER_ACTION", L"輸出設定已確認；模式=" + std::to_wstring(static_cast<int>(g_output_mode)) +
                        L"；固定資料夾=" + g_output_folder.wstring() + L"；完成後刪除=" + (g_delete_after ? L"是" : L"否") +
                        L"；保存驗證=" + (save_ok && saved_delete_after == g_delete_after ? L"成功" : L"失敗"));
        if (!save_ok || saved_delete_after != g_delete_after) {
            MessageBoxW(owner, L"設定無法寫入 Windows 使用者設定資料夾。請確認目前帳號有權限寫入 AppData\\Roaming\\AutoUnwrap。",
                        L"Auto Unwrap 設定", MB_OK | MB_ICONERROR);
            return false;
        }
        return true;
    }
    return false;
}

static std::wstring QuoteArg(const std::wstring& arg) {
    std::wstring result = L"\"";
    size_t backslashes = 0;
    for (wchar_t ch : arg) {
        if (ch == L'\\') ++backslashes;
        else if (ch == L'\"') {
            result.append(backslashes * 2 + 1, L'\\');
            result.push_back(L'\"');
            backslashes = 0;
        } else {
            result.append(backslashes, L'\\');
            result.push_back(ch);
            backslashes = 0;
        }
    }
    result.append(backslashes * 2, L'\\');
    result.push_back(L'\"');
    return result;
}

static bool IsBandizip(const fs::path& engine) {
    const std::wstring name = Lower(engine.filename().wstring());
    return name == L"bz.exe" || name == L"bandizip.exe" || name.find(L"bandizip") != std::wstring::npos;
}

static fs::path PreferredBandizipCli(const fs::path& selected) {
    if (!IsBandizip(selected)) return selected;
    const std::wstring filename = Lower(selected.filename().wstring());
    if (filename == L"bz.exe") return selected;
    const fs::path sibling = selected.parent_path() / L"bz.exe";
    if (fs::exists(sibling)) return sibling;
    return selected;
}

static fs::path NormalizeEnginePath(const fs::path& selected) {
    if (selected.empty()) return selected;
    return PreferredBandizipCli(selected);
}

static bool IsBandizipConsole(const fs::path& engine) {
    return IsBandizip(engine) && Lower(engine.filename().wstring()) == L"bz.exe";
}

static std::wstring FindEngine() {
    if (!g_engine_path.empty() && fs::exists(g_engine_path)) {
        const fs::path preferred = NormalizeEnginePath(fs::path(g_engine_path));
        if (IsBandizip(fs::path(g_engine_path)) && !IsBandizipConsole(preferred)) {
            AppendLog(L"設定的 Bandizip.exe 不是主控台工具且找不到同目錄 bz.exe，將改找 7-Zip 備援。");
        } else {
            if (preferred.wstring() != g_engine_path) {
                g_engine_path = preferred.wstring();
                SaveSettings();
                AppendLog(L"已將 Bandizip GUI 入口改為同目錄的主控台工具：" + g_engine_path);
            }
            return g_engine_path;
        }
    }
    const std::vector<std::wstring> candidates = {
        L"C:\\Program Files\\Bandizip\\bz.exe",
        L"C:\\Program Files\\Bandizip\\Bandizip.exe",
        L"C:\\Program Files (x86)\\Bandizip\\bz.exe",
        L"C:\\Program Files (x86)\\Bandizip\\Bandizip.exe",
        L"C:\\Program Files\\7-Zip\\7z.exe",
        L"C:\\Program Files\\7-Zip\\7zz.exe",
        L"C:\\Program Files (x86)\\7-Zip\\7z.exe"
    };
    for (const auto& candidate : candidates) if (fs::exists(candidate)) {
        const fs::path preferred = NormalizeEnginePath(fs::path(candidate));
        if (IsBandizip(fs::path(candidate)) && !IsBandizipConsole(preferred)) continue;
        return preferred.wstring();
    }
    wchar_t buffer[32768] = {};
    if (SearchPathW(nullptr, L"bz.exe", nullptr, 32768, buffer, nullptr)) return buffer;
    if (SearchPathW(nullptr, L"Bandizip.exe", nullptr, 32768, buffer, nullptr)) {
        const fs::path preferred = PreferredBandizipCli(fs::path(buffer));
        if (IsBandizipConsole(preferred)) return preferred.wstring();
    }
    if (SearchPathW(nullptr, L"7z.exe", nullptr, 32768, buffer, nullptr)) return buffer;
    if (SearchPathW(nullptr, L"7zz.exe", nullptr, 32768, buffer, nullptr)) return buffer;
    return L"";
}

struct ProcessRunResult {
    bool launched = false;
    bool timed_out = false;
    bool cancelled = false;
    bool saw_invalid_password = false;
    bool saw_password_hint = false;
    bool saw_crc_failure = false;
    bool saw_explicit_error = false;
    DWORD exit_code = ERROR_PROCESS_ABORTED;
    uintmax_t output_bytes = 0;
    ULONGLONG elapsed_ms = 0;
    std::wstring output_tail;
};

static bool ContainsInsensitiveAscii(const std::string& value, const char* needle) {
    std::string lower = value;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    std::string wanted = needle ? needle : "";
    std::transform(wanted.begin(), wanted.end(), wanted.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return !wanted.empty() && lower.find(wanted) != std::string::npos;
}

static uintmax_t DirectoryActivityBytes(const fs::path& root, size_t& file_count) {
    file_count = 0;
    uintmax_t total = 0;
    std::error_code ec;
    if (!fs::exists(root, ec)) return 0;
    for (fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec), end; it != end; it.increment(ec)) {
        if (ec) continue;
        if (it->is_regular_file(ec)) {
            ++file_count;
            const uintmax_t size = it->file_size(ec);
            if (!ec) total += size;
        }
    }
    return total;
}

static std::wstring ProcessOutputToWide(const std::string& bytes) {
    if (bytes.empty()) return L"";
    int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, bytes.data(), static_cast<int>(bytes.size()), nullptr, 0);
    UINT code_page = CP_UTF8;
    if (length <= 0) {
        code_page = CP_ACP;
        length = MultiByteToWideChar(code_page, 0, bytes.data(), static_cast<int>(bytes.size()), nullptr, 0);
    }
    if (length <= 0) return L"";
    std::wstring result(static_cast<size_t>(length), L'\0');
    MultiByteToWideChar(code_page, 0, bytes.data(), static_cast<int>(bytes.size()), result.data(), length);
    for (auto& ch : result) if (ch == L'\r') ch = L'\n';
    return result;
}

static std::wstring LastOutputLines(const std::string& bytes) {
    std::wstring text = ProcessOutputToWide(bytes);
    if (text.size() > 6000) text.erase(0, text.size() - 6000);
    const size_t first = text.find_last_of(L'\n', text.size() > 1 ? text.size() - 2 : 0);
    if (first != std::wstring::npos && text.size() - first > 2400) text.erase(0, text.size() - 2400);
    return text;
}

static std::wstring FormatElapsed(ULONGLONG milliseconds) {
    const ULONGLONG seconds = milliseconds / 1000;
    return std::to_wstring(seconds / 60) + L"分" + std::to_wstring(seconds % 60) + L"秒";
}

static std::wstring LatestPercent(const std::string& bytes) {
    const size_t percent = bytes.find_last_of('%');
    if (percent == std::string::npos || percent == 0) return L"";
    size_t begin = percent;
    while (begin > 0 && bytes[begin - 1] >= '0' && bytes[begin - 1] <= '9') --begin;
    if (begin == percent) return L"";
    return ProcessOutputToWide(bytes.substr(begin, percent - begin + 1));
}

static std::wstring FormatBytes(uintmax_t bytes) {
    const wchar_t* units[] = {L"B", L"KB", L"MB", L"GB", L"TB"};
    double value = static_cast<double>(bytes);
    size_t unit = 0;
    while (value >= 1024.0 && unit < 4) {
        value /= 1024.0;
        ++unit;
    }
    return std::to_wstring(static_cast<unsigned long long>(value + 0.5)) + L" " + units[unit];
}

static ProcessRunResult RunProcess(const std::wstring& command, const fs::path& activity_directory,
                                   const std::wstring& status_label) {
    constexpr ULONGLONG IDLE_WARNING_MS = 10ULL * 60ULL * 1000ULL;
    constexpr ULONGLONG IDLE_TIMEOUT_MS = 30ULL * 60ULL * 1000ULL;
    constexpr ULONGLONG HARD_TIMEOUT_MS = 24ULL * 60ULL * 60ULL * 1000ULL;
    ProcessRunResult result;
    SECURITY_ATTRIBUTES security{};
    security.nLength = sizeof(security);
    security.bInheritHandle = TRUE;
    HANDLE read_pipe = nullptr, write_pipe = nullptr;
    HANDLE null_input = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                    &security, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (null_input == INVALID_HANDLE_VALUE) null_input = nullptr;
    if (!CreatePipe(&read_pipe, &write_pipe, &security, 0)) {
        if (null_input) CloseHandle(null_input);
        return result;
    }
    SetHandleInformation(read_pipe, HANDLE_FLAG_INHERIT, 0);
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    startup.wShowWindow = SW_HIDE;
    startup.hStdInput = null_input;
    startup.hStdOutput = write_pipe;
    startup.hStdError = write_pipe;
    PROCESS_INFORMATION process{};
    std::vector<wchar_t> command_line(command.begin(), command.end());
    command_line.push_back(L'\0');
    const BOOL created = CreateProcessW(nullptr, command_line.data(), nullptr, nullptr, TRUE,
                                        CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process);
    if (null_input) CloseHandle(null_input);
    CloseHandle(write_pipe);
    if (!created) {
        CloseHandle(read_pipe);
        return result;
    }
    result.launched = true;
    const ULONGLONG started = GetTickCount64();
    ULONGLONG last_activity = started;
    ULONGLONG last_scan = started;
    ULONGLONG last_report = started;
    size_t last_file_count = 0;
    uintmax_t last_file_bytes = 0;
    std::string output;
    const fs::path raw_output_path = NewProcessOutputFile();
    std::ofstream raw_output(raw_output_path, std::ios::binary | std::ios::trunc);
    WriteDiagnostic(L"PROCESS_START", status_label + L"；raw_output=" + raw_output_path.wstring());
    auto consume_output = [&](const char* data, DWORD amount) {
        if (!data || amount == 0) return;
        const std::string chunk(data, data + amount);
        if (raw_output) raw_output.write(chunk.data(), static_cast<std::streamsize>(chunk.size()));
        result.output_bytes += amount;
        output.append(chunk);
        result.saw_password_hint = result.saw_password_hint ||
            ContainsInsensitiveAscii(chunk, "invalid password") || ContainsInsensitiveAscii(chunk, "wrong password");
        result.saw_crc_failure = result.saw_crc_failure ||
            ContainsInsensitiveAscii(chunk, "crc failed") || ContainsInsensitiveAscii(chunk, "checksum failed") ||
            ContainsInsensitiveAscii(chunk, "crc error");
        result.saw_explicit_error = result.saw_explicit_error ||
            ContainsInsensitiveAscii(chunk, "error:") || ContainsInsensitiveAscii(chunk, "can't open") ||
            ContainsInsensitiveAscii(chunk, "cannot open") || ContainsInsensitiveAscii(chunk, "not an archive");
        if (output.size() > 12000) output.erase(0, output.size() - 12000);
    };
    char buffer[8192];
    bool finished = false;
    bool idle_warning_reported = false;
    while (!finished) {
        const ULONGLONG now = GetTickCount64();
        DWORD available = 0;
        while (PeekNamedPipe(read_pipe, nullptr, 0, nullptr, &available, nullptr) && available > 0) {
            DWORD read = 0;
            const DWORD amount = std::min<DWORD>(available, sizeof(buffer));
            if (!ReadFile(read_pipe, buffer, amount, &read, nullptr) || read == 0) break;
            consume_output(buffer, read);
            last_activity = now;
        }
        if (now - last_scan >= 15000) {
            size_t file_count = 0;
            const uintmax_t file_bytes = DirectoryActivityBytes(activity_directory, file_count);
            if (file_count != last_file_count || file_bytes != last_file_bytes) last_activity = now;
            last_file_count = file_count;
            last_file_bytes = file_bytes;
            last_scan = now;
        }
                if (now - last_report >= 1000) {
            const std::wstring progress = LatestPercent(output);
            const bool idle_warning = now - last_activity >= IDLE_WARNING_MS;
            PostStatus(status_label + L"｜已執行 " + FormatElapsed(now - started) +
                       (progress.empty() ? L"" : L"｜工具進度 " + progress) +
                       L"｜輸出活動 " + FormatBytes(last_file_bytes) +
                       (idle_warning ? L"｜最近 10 分鐘無可見活動，仍在等待；可關閉視窗取消" : L"｜可關閉視窗取消"));
            last_report = now;
        }

        if (now - last_activity >= IDLE_WARNING_MS && !idle_warning_reported) {
            idle_warning_reported = true;
            AppendLog(status_label + L"：已超過 10 分鐘沒有工具輸出或檔案大小變化；不會立即終止，若要停止請關閉視窗。");
        }

        if (g_cancel_requested.load()) {
            result.cancelled = true;
            TerminateProcess(process.hProcess, ERROR_CANCELLED);
            WaitForSingleObject(process.hProcess, 5000);
            break;
        }
        if (now - started >= HARD_TIMEOUT_MS || now - last_activity >= IDLE_TIMEOUT_MS) {
            result.timed_out = true;
            TerminateProcess(process.hProcess, WAIT_TIMEOUT);
            WaitForSingleObject(process.hProcess, 5000);
            break;
        }
        const DWORD wait = WaitForSingleObject(process.hProcess, 100);
        if (wait == WAIT_OBJECT_0) {
            finished = true;
            while (PeekNamedPipe(read_pipe, nullptr, 0, nullptr, &available, nullptr) && available > 0) {
                DWORD read = 0;
                const DWORD amount = std::min<DWORD>(available, sizeof(buffer));
                if (!ReadFile(read_pipe, buffer, amount, &read, nullptr) || read == 0) break;
                consume_output(buffer, read);
            }
        }
    }
    if (raw_output) raw_output.close();
    result.elapsed_ms = GetTickCount64() - started;
    if (!result.cancelled && !result.timed_out) GetExitCodeProcess(process.hProcess, &result.exit_code);
    result.output_tail = LastOutputLines(output);
    // 7-Zip 常在 CRC 錯誤後附帶「Wrong password?」提示；只要已發生 CRC，不能把它當成密碼錯誤。
    result.saw_invalid_password = result.saw_password_hint && !result.saw_crc_failure;
    WriteDiagnostic(L"PROCESS_END", status_label + L"；raw_output=" + raw_output_path.wstring() +
                    L"；exit_code=" + std::to_wstring(result.exit_code) + L"；bytes=" + FormatBytes(result.output_bytes));
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    CloseHandle(read_pipe);
    return result;
}

enum class ArchiveKind { Unknown, Zip, SevenZip, Rar, Lz4 };

static bool IsExplicitArchiveExtension(const std::wstring& ext) {
    return ext == L".7z" || ext == L".zip" || ext == L".rar" || ext == L".lz4";
}

static bool IsDisguisedArchiveExtension(const std::wstring& ext) {
    return ext.empty() || ext == L".jpg" || ext == L".jpeg" || ext == L".png" || ext == L".gif" ||
           ext == L".bmp" || ext == L".webp" || ext == L".lz4" || ext == L".dat" || ext == L".img" ||
           ext == L".tmp";
}

static bool ShouldDeepScanArchive(const fs::path& path) {
    return IsDisguisedArchiveExtension(Lower(path.extension().wstring()));
}

static std::wstring LowerCopy(const std::wstring& value) {
    return Lower(value);
}

static bool HasSupportedArchiveSuffix(const std::wstring& value) {
    const std::wstring lower = LowerCopy(value);
    const auto has_suffix = [&lower](const std::wstring& suffix) {
        return lower.size() >= suffix.size() &&
               lower.compare(lower.size() - suffix.size(), suffix.size(), suffix) == 0;
    };
    return has_suffix(L".rar") || has_suffix(L".zip") || has_suffix(L".7z") || has_suffix(L".lz4");
}

static std::wstring StripConfiguredDisguiseText(const std::wstring& name, std::wstring& matched_rule) {
    std::wstring cleaned = name;
    for (const auto& rule : g_disguise_rules) {
        if (rule.empty()) continue;
        const std::wstring::size_type position = cleaned.find(rule);
        if (position != std::wstring::npos) {
            cleaned.erase(position, rule.size());
            matched_rule = rule;
            return cleaned;
        }
    }
    return cleaned;
}

static std::wstring StripArchiveSuffixDecoration(const std::wstring& name, std::wstring& matched_suffix) {
    const std::wstring lower = LowerCopy(name);
    const std::vector<std::wstring> suffixes = {L".rar", L".zip", L".7z", L".lz4"};
    size_t best = std::wstring::npos;
    std::wstring best_suffix;
    for (const auto& suffix : suffixes) {
        const size_t position = lower.find(suffix);
        if (position != std::wstring::npos && position > 0 &&
            position + suffix.size() < name.size() && (best == std::wstring::npos || position > best)) {
            best = position;
            best_suffix = suffix;
        }
    }
    if (best == std::wstring::npos) return name;
    const wchar_t first_extra = name[best + best_suffix.size()];
    // 只移除自然語言／說明文字，不碰正式分割卷的 .001、.z01 等結構。
    if (first_extra == L'.' || first_extra == L'_' || first_extra == L'-' ||
        (first_extra >= L'0' && first_extra <= L'9')) return name;
    matched_suffix = best_suffix;
    return name.substr(0, best + best_suffix.size());
}

static ArchiveKind DetectArchive(const fs::path& path) {
    std::error_code ec;
    if (!fs::is_regular_file(path, ec)) return ArchiveKind::Unknown;
    std::ifstream input(path, std::ios::binary);
    if (!input) return ArchiveKind::Unknown;
    std::wstring decorated_rule;
    const std::wstring decorated_name = StripConfiguredDisguiseText(path.filename().wstring(), decorated_rule);
    const std::wstring suffix_name = StripArchiveSuffixDecoration(path.filename().wstring(), decorated_rule);
    const bool filename_archive_hint = HasSupportedArchiveSuffix(decorated_name) || HasSupportedArchiveSuffix(suffix_name);
    const size_t scan_limit = (ShouldDeepScanArchive(path) || filename_archive_hint) ? 1024 * 1024 : 64 * 1024;
    std::vector<unsigned char> bytes(scan_limit);
    input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    const size_t n = static_cast<size_t>(input.gcount());
    auto match_at = [&, n](size_t offset, std::initializer_list<unsigned char> signature) {
        if (offset + signature.size() > n) return false;
        size_t index = 0;
        for (const unsigned char value : signature) if (bytes[offset + index++] != value) return false;
        return true;
    };
    // 真實壓縮檔通常由標頭開始；只有可疑偽裝副檔名才允許在前綴區尋找自解壓／非標準標頭。
    const std::wstring ext = Lower(path.extension().wstring());
    const bool explicit_archive = IsExplicitArchiveExtension(ext);
    const bool disguised_archive = IsDisguisedArchiveExtension(ext);
    if (!explicit_archive && !disguised_archive && !filename_archive_hint) return ArchiveKind::Unknown;
    const auto detect_at = [&](size_t offset) -> ArchiveKind {
        if (match_at(offset, {0x37, 0x7A, 0xBC, 0xAF, 0x27, 0x1C})) return ArchiveKind::SevenZip;
        if (match_at(offset, {'R', 'a', 'r', '!', 0x1A, 0x07, 0x00}) ||
            match_at(offset, {'R', 'a', 'r', '!', 0x1A, 0x07, 0x01})) return ArchiveKind::Rar;
        if (match_at(offset, {0x04, 0x22, 0x4D, 0x18})) return ArchiveKind::Lz4;
        if (match_at(offset, {'P', 'K', 0x03, 0x04}) || match_at(offset, {'P', 'K', 0x05, 0x06}) ||
            match_at(offset, {'P', 'K', 0x07, 0x08})) return ArchiveKind::Zip;
        return ArchiveKind::Unknown;
    };
    const ArchiveKind at_start = detect_at(0);
    if (at_start != ArchiveKind::Unknown) return at_start;
    if (!ShouldDeepScanArchive(path) && !filename_archive_hint) return ArchiveKind::Unknown;
    for (size_t offset = 1; offset < n; ++offset) {
        const ArchiveKind found = detect_at(offset);
        if (found != ArchiveKind::Unknown) return found;
    }
    return ArchiveKind::Unknown;
}

static std::wstring ExtensionFor(ArchiveKind kind) {
    switch (kind) {
        case ArchiveKind::Zip: return L".zip";
        case ArchiveKind::SevenZip: return L".7z";
        case ArchiveKind::Rar: return L".rar";
        case ArchiveKind::Lz4: return L".lz4";
        default: return L"";
    }
}

static std::wstring KindName(ArchiveKind kind) {
    switch (kind) {
        case ArchiveKind::Zip: return L"ZIP";
        case ArchiveKind::SevenZip: return L"7Z";
        case ArchiveKind::Rar: return L"RAR";
        case ArchiveKind::Lz4: return L"LZ4";
        default: return L"未知";
    }
}

static bool EndsWithInsensitive(const std::wstring& value, const std::wstring& suffix) {
    const std::wstring lower_value = Lower(value);
    const std::wstring lower_suffix = Lower(suffix);
    return lower_value.size() >= lower_suffix.size() &&
           lower_value.compare(lower_value.size() - lower_suffix.size(), lower_suffix.size(), lower_suffix) == 0;
}

static bool IsDigits(const std::wstring& value) {
    if (value.empty()) return false;
    return std::all_of(value.begin(), value.end(), [](wchar_t c) { return c >= L'0' && c <= L'9'; });
}

struct ArchiveGroup {
    fs::path primary;
    ArchiveKind kind = ArchiveKind::Unknown;
    std::vector<fs::path> parts;
    bool segmented = false;
};

static fs::path NormalizeDisguisedArchiveName(const fs::path& requested) {
    if (!fs::is_regular_file(requested)) return requested;
    std::wstring matched;
    std::wstring candidate_name = StripConfiguredDisguiseText(requested.filename().wstring(), matched);
    if (candidate_name == requested.filename().wstring()) {
        candidate_name = StripArchiveSuffixDecoration(requested.filename().wstring(), matched);
    }
    if (candidate_name == requested.filename().wstring() || !HasSupportedArchiveSuffix(candidate_name)) return requested;
    if (DetectArchive(requested) == ArchiveKind::Unknown) return requested;
    fs::path target = requested.parent_path() / candidate_name;
    if (fs::exists(target)) target = UniqueTargetPath(target);
    std::error_code ec;
    fs::rename(requested, target, ec);
    if (!ec) {
        AppendLog(L"已清理偽裝檔名：" + requested.filename().wstring() + L" → " + target.filename().wstring());
        WriteDiagnostic(L"FILENAME_RULE", L"original=" + requested.filename().wstring() +
                        L"；cleaned=" + target.filename().wstring() + L"；rule=" + matched);
        return target;
    }
    AppendLog(L"偽裝檔名規則匹配但無法改名，保留原檔案：" + requested.filename().wstring());
    return requested;
}

static fs::path RepairLegacyRenamedFirstVolume(const fs::path& requested) {
    const std::wstring name = requested.filename().wstring();
    const std::wstring lower_name = Lower(name);
    const std::vector<std::wstring> suspicious = {L".7z.7z", L".rar.rar", L".zip.zip"};
    for (const auto& suffix : suspicious) {
        if (!EndsWithInsensitive(name, suffix)) continue;
        const std::wstring prefix = name.substr(0, name.size() - 4);
        const fs::path candidate = requested.parent_path() / (prefix + L".001");
        const fs::path second = requested.parent_path() / (prefix + L".002");
        if (!fs::exists(candidate) && fs::exists(second) && DetectArchive(requested) != ArchiveKind::Unknown) {
            std::error_code ec;
            fs::rename(requested, candidate, ec);
            if (!ec) {
                AppendLog(L"已修復舊版誤改的分割檔首卷：" + name + L" → " + candidate.filename().wstring());
                return candidate;
            }
        }
    }
    return requested;
}

static void AddExistingPart(std::vector<fs::path>& parts, const fs::path& path) {
    if (fs::exists(path) && std::find(parts.begin(), parts.end(), path) == parts.end()) parts.push_back(path);
}

static ArchiveGroup ResolveArchiveGroup(const fs::path& requested) {
    ArchiveGroup group;
    const fs::path cleaned = NormalizeDisguisedArchiveName(requested);
    const fs::path repaired = RepairLegacyRenamedFirstVolume(cleaned);
    const std::wstring name = repaired.filename().wstring();
    const std::wstring lower_name = Lower(name);
    group.primary = repaired;
    group.kind = DetectArchive(repaired);
    group.parts.push_back(repaired);

    const size_t dot = lower_name.find_last_of(L'.');
    if (dot != std::wstring::npos) {
        const std::wstring extension = lower_name.substr(dot);
        const std::wstring base = name.substr(0, dot);
        if (extension.size() >= 4 && extension[0] == L'.' && IsDigits(extension.substr(1))) {
            const std::wstring volume_number = extension.substr(1);
            const int index = std::stoi(volume_number);
            if (index >= 1) {
                const int width = static_cast<int>(volume_number.size());
                const std::wstring first_number = std::wstring(width > 1 ? width - 1 : 0, L'0') + L"1";
                const fs::path first = repaired.parent_path() / (base + L"." + first_number);
                const bool looks_like_known_volume = EndsWithInsensitive(base, L".7z") ||
                                                     EndsWithInsensitive(base, L".zip") ||
                                                     EndsWithInsensitive(base, L".rar") ||
                                                     group.kind != ArchiveKind::Unknown;
                if (looks_like_known_volume && fs::exists(first)) {
                    group.primary = first;
                    group.kind = DetectArchive(first);
                    group.segmented = true;
                    group.parts.clear();
                    for (int part = 1; part < 100000; ++part) {
                        const std::wstring part_text = std::to_wstring(part);
                        const std::wstring padded = part_text.size() < static_cast<size_t>(width)
                            ? std::wstring(static_cast<size_t>(width) - part_text.size(), L'0') + part_text
                            : part_text;
                        const fs::path candidate = repaired.parent_path() / (base + L"." + padded);
                        if (!fs::exists(candidate)) break;
                        group.parts.push_back(candidate);
                    }
                    if (group.kind != ArchiveKind::Unknown) return group;
                    if (EndsWithInsensitive(base, L".7z")) { group.kind = ArchiveKind::SevenZip; return group; }
                    if (EndsWithInsensitive(base, L".zip")) { group.kind = ArchiveKind::Zip; return group; }
                    if (EndsWithInsensitive(base, L".rar")) { group.kind = ArchiveKind::Rar; return group; }
                }
            }
        }
        if (extension.size() >= 3 && extension[0] == L'.' && extension[1] == L'z' &&
            IsDigits(extension.substr(2))) {
            const fs::path primary = repaired.parent_path() / (base + L".zip");
            if (fs::exists(primary)) {
                group.primary = primary;
                group.kind = DetectArchive(primary);
                group.segmented = true;
                group.parts.clear();
                for (int part = 1; part < 100000; ++part) {
                    const fs::path candidate = repaired.parent_path() / (base + L".z" +
                        (part < 10 ? L"0" : L"") + std::to_wstring(part));
                    if (!fs::exists(candidate)) break;
                    group.parts.push_back(candidate);
                }
                AddExistingPart(group.parts, primary);
                if (group.kind != ArchiveKind::Unknown) return group;
                group.kind = ArchiveKind::Zip;
                return group;
            }
        }
        if (extension == L".rar") {
            const size_t part_marker = lower_name.find(L".part");
            if (part_marker != std::wstring::npos) {
                const std::wstring number = lower_name.substr(part_marker + 5, lower_name.size() - part_marker - 9);
                if (IsDigits(number)) {
                    const std::wstring prefix = name.substr(0, part_marker);
                    const int width = static_cast<int>(number.size());
                    const std::wstring first_number = width > 1 ? std::wstring(width - 1, L'0') + L"1" : L"1";
                    const fs::path first = repaired.parent_path() / (prefix + L".part" + first_number + L".rar");
                    if (fs::exists(first)) {
                        group.primary = first;
                        group.kind = ArchiveKind::Rar;
                        group.segmented = true;
                        group.parts.clear();
                        for (int part = 1; part < 100000; ++part) {
                            const std::wstring part_number = width > 1
                                ? std::wstring(width - std::to_wstring(part).size(), L'0') + std::to_wstring(part)
                                : std::to_wstring(part);
                            const fs::path candidate = repaired.parent_path() / (prefix + L".part" + part_number + L".rar");
                            if (!fs::exists(candidate)) break;
                            group.parts.push_back(candidate);
                        }
                        return group;
                    }
                }
            }
        }
    }

    if (lower_name.size() >= 4 && EndsWithInsensitive(lower_name, L".rar")) {
        group.kind = group.kind == ArchiveKind::Unknown ? ArchiveKind::Rar : group.kind;
        const std::wstring prefix = name.substr(0, name.size() - 4);
        const fs::path first_legacy = repaired.parent_path() / (prefix + L".rar");
        bool has_legacy_parts = false;
        for (int part = 0; part < 100000; ++part) {
            const std::wstring suffix = part == 0 ? L".rar" :
                (std::wstring(L".r") + (part < 10 ? L"0" : L"") + std::to_wstring(part - 1));
            const fs::path candidate = repaired.parent_path() / (prefix + suffix);
            if (!fs::exists(candidate)) {
                if (part == 0) break;
                break;
            }
            if (part > 0) has_legacy_parts = true;
            AddExistingPart(group.parts, candidate);
        }
        if (has_legacy_parts) {
            group.primary = first_legacy;
            group.segmented = true;
            group.kind = ArchiveKind::Rar;
            return group;
        }
    }

    if (Lower(repaired.extension().wstring()) == L".r00") {
        const fs::path primary = repaired.parent_path() / (name.substr(0, name.size() - 4) + L".rar");
        if (fs::exists(primary)) {
            group.primary = primary;
            group.kind = ArchiveKind::Rar;
            group.segmented = true;
            group.parts.clear();
            AddExistingPart(group.parts, primary);
            for (int part = 0; part < 100000; ++part) {
                const fs::path candidate = repaired.parent_path() / (name.substr(0, name.size() - 4) + L".r" +
                    (part < 10 ? L"0" : L"") + std::to_wstring(part));
                if (!fs::exists(candidate)) break;
                group.parts.push_back(candidate);
            }
            return group;
        }
    }

    return group;
}

static fs::path RenameWithCorrectExtension(const fs::path& original, ArchiveKind kind, bool segmented = false) {
    const std::wstring extension = ExtensionFor(kind);
    if (segmented || extension.empty() || Lower(original.extension().wstring()) == extension) return original;
    fs::path target = original.parent_path() / (original.stem().wstring() + extension);
    int counter = 2;
    while (fs::exists(target)) target = original.parent_path() / (original.stem().wstring() + L"_" + std::to_wstring(counter++) + extension);
    std::error_code ec;
    fs::rename(original, target, ec);
    if (ec) {
        AppendLog(L"無法直接修改副檔名，將保留原檔名嘗試解壓：" + original.filename().wstring());
        return original;
    }
    AppendLog(L"已直接修改副檔名：" + original.filename().wstring() + L" → " + target.filename().wstring());
    return target;
}

static bool IsExecutableFile(const fs::path& path) {
    return Lower(path.extension().wstring()) == L".exe" && fs::is_regular_file(path);
}

static bool HasFinalGameMarker(const std::vector<fs::path>& changed_files, std::wstring& marker) {
    for (const auto& file : changed_files) {
        const fs::path name_path = file.filename();
        const std::wstring lower_name = Lower(name_path.wstring());
        if (lower_name == L"game" || lower_name == L"renpy") {
            marker = name_path.wstring();
            return true;
        }
        if (IsExecutableFile(file)) {
            marker = file.filename().wstring();
            return true;
        }
        for (const auto& parent : {file.parent_path(), file.parent_path().parent_path()}) {
            const std::wstring parent_name = Lower(parent.filename().wstring());
            if (parent_name == L"game" || parent_name == L"renpy") {
                marker = parent.filename().wstring();
                return true;
            }
        }
    }
    return false;
}

static bool HasAnyEntry(const fs::path& directory) {
    std::error_code ec;
    return fs::directory_iterator(directory, fs::directory_options::skip_permission_denied, ec) != fs::directory_iterator();
}

static std::wstring PathKey(const fs::path& path) {
    std::error_code ec;
    fs::path absolute = fs::absolute(path, ec);
    if (ec) absolute = path;
    return Lower(absolute.lexically_normal().wstring());
}

static fs::path UniqueTargetPath(const fs::path& original) {
    if (!fs::exists(original)) return original;
    const std::wstring stem = original.stem().wstring();
    const std::wstring extension = original.extension().wstring();
    for (int i = 2; i < 100000; ++i) {
        const fs::path candidate = original.parent_path() / (stem + L"_" + std::to_wstring(i) + extension);
        if (!fs::exists(candidate)) return candidate;
    }
    return original.parent_path() / (stem + L"_new" + extension);
}

static void CollectRegularFiles(const fs::path& root, std::vector<fs::path>& files) {
    std::error_code ec;
    if (!fs::exists(root, ec)) return;
    for (fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec), end; it != end; it.increment(ec)) {
        if (ec) continue;
        if (it->is_regular_file(ec)) files.push_back(it->path());
    }
}

static void CollectNewFiles(const fs::path& root, const std::set<std::wstring>& before, std::vector<fs::path>& files) {
    std::error_code ec;
    if (!fs::exists(root, ec)) return;
    for (fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec), end; it != end; it.increment(ec)) {
        if (ec) continue;
        const std::wstring key = PathKey(it->path());
        if (before.count(key) == 0 && it->is_regular_file(ec)) files.push_back(it->path());
    }
}

static bool MoveTree(const fs::path& source, const fs::path& destination, std::vector<fs::path>& moved_files) {
    std::error_code ec;
    fs::create_directories(destination, ec);
    if (ec) return false;
    for (fs::directory_iterator it(source, fs::directory_options::skip_permission_denied, ec), end; it != end; it.increment(ec)) {
        if (ec) return false;
        const fs::path entry = it->path();
        fs::path target = destination / entry.filename();
        if (it->is_directory(ec)) {
            if (fs::exists(target, ec) && fs::is_directory(target, ec)) {
                if (!MoveTree(entry, target, moved_files)) return false;
                fs::remove(entry, ec);
            } else {
                std::vector<fs::path> subtree;
                CollectRegularFiles(entry, subtree);
                fs::rename(entry, target, ec);
                if (ec) {
                    ec.clear();
                    fs::copy(entry, target, fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);
                    if (!ec) fs::remove_all(entry, ec);
                }
                if (ec) return false;
                for (const auto& old_file : subtree) {
                    const fs::path relative = old_file.lexically_relative(entry);
                    moved_files.push_back(target / relative);
                }
            }
        } else if (it->is_regular_file(ec)) {
            target = UniqueTargetPath(target);
            fs::rename(entry, target, ec);
            if (ec) {
                ec.clear();
                fs::copy_file(entry, target, fs::copy_options::overwrite_existing, ec);
                if (!ec) fs::remove(entry, ec);
            }
            if (ec) return false;
            moved_files.push_back(target);
        }
    }
    return true;
}

static fs::path CreateAttemptDirectory(const fs::path& destination, int depth, size_t attempt) {
    std::error_code ec;
    fs::path candidate = destination / (L".autounwrap_tmp_" + std::to_wstring(GetCurrentProcessId()) + L"_" +
                                      std::to_wstring(GetTickCount64()) + L"_" + std::to_wstring(depth) + L"_" + std::to_wstring(attempt));
    while (fs::exists(candidate, ec)) candidate += L"_x";
    fs::create_directories(candidate, ec);
    if (!ec) SetFileAttributesW(candidate.wstring().c_str(), FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_TEMPORARY);
    return candidate;
}

static std::wstring BuildCommand(const fs::path& engine, const wchar_t* action,
                                 const fs::path& archive, const fs::path& destination,
                                 const std::wstring& password) {
    const bool bandizip = IsBandizip(engine);
    std::wstring command = QuoteArg(engine.wstring()) + L" " + action + L" -y ";
    if (action[0] == L'x') command += L"-aou ";
    if (!bandizip) command += L"-bsp1 ";
    if (bandizip) {
        if (action[0] == L'x') command += QuoteArg(L"-o:" + destination.wstring()) + L" ";
        command += QuoteArg(L"-p:" + password) + L" ";
        command += QuoteArg(archive.wstring());
    } else {
        command += QuoteArg(archive.wstring()) + L" ";
        if (action[0] == L'x') command += QuoteArg(L"-o" + destination.wstring()) + L" ";
        command += QuoteArg(password.empty() ? L"-p" : L"-p" + password);
    }
    return command;
}

static std::vector<std::wstring> PasswordCandidates(const std::wstring& one_time) {
    std::vector<std::wstring> candidates;
    auto add = [&candidates](const std::wstring& value) {
        if (std::find(candidates.begin(), candidates.end(), value) == candidates.end()) candidates.push_back(value);
    };
    if (!one_time.empty()) add(one_time);
    std::lock_guard<std::mutex> lock(g_password_mutex);
    for (const auto& password : g_passwords) add(password);
    add(L"");
    return candidates;
}

struct ExtractionResult {
    bool success = false;
    bool integrity_warning = false;
    std::vector<fs::path> changed_files;
    std::wstring password;
};

static ExtractionResult TestAndExtract(const ArchiveGroup& group, const fs::path& destination,
                                       const std::vector<std::wstring>& passwords, const fs::path& engine, int depth) {
    ExtractionResult result;
    const fs::path archive = group.primary;
    for (size_t i = 0; i < passwords.size(); ++i) {
        if (g_cancel_requested.load()) break;
        const fs::path attempt = CreateAttemptDirectory(destination, depth, i + 1);
        const std::wstring label = L"第 " + std::to_wstring(depth + 1) + L" 層／密碼 " + std::to_wstring(i + 1);
        const std::wstring extract_command = BuildCommand(engine, L"x", archive, attempt, passwords[i]);
        WriteDiagnostic(L"EXTRACT_ATTEMPT", label + L"；engine=" + engine.wstring() + L"；archive=" + archive.wstring() +
                        L"；password_length=" + std::to_wstring(passwords[i].size()) +
                        L"；password_value=REDACTED；temporary=" + attempt.wstring());
        AppendLog(label + L"：開始直接解壓至隱藏暫存區。");
        FlashWorkWindow();
        PostStatus(label + L"：解壓工具執行中");
        const ProcessRunResult run = RunProcess(extract_command, attempt, label);
        const bool has_entries = HasAnyEntry(attempt);
        const bool warning_only = run.exit_code == 1 && has_entries && !run.saw_explicit_error;
        const bool partial_success = run.exit_code == 2 && has_entries && !run.saw_invalid_password;
        const bool extraction_ok = run.launched && !run.timed_out && !run.cancelled &&
                                   (run.exit_code == 0 || warning_only || partial_success) && has_entries;
        WriteDiagnostic(L"PROCESS_RESULT", label + L"；exit_code=" + std::to_wstring(run.exit_code) +
                        L"；elapsed=" + FormatElapsed(run.elapsed_ms) + L"；output_bytes=" + FormatBytes(run.output_bytes) +
                        L"；entries=" + (has_entries ? L"yes" : L"no") +
                        L"；password_hint=" + (run.saw_password_hint ? L"yes" : L"no") +
                        L"；invalid_password=" + (run.saw_invalid_password ? L"yes" : L"no") +
                        L"；crc_failure=" + (run.saw_crc_failure ? L"yes" : L"no") +
                        L"；explicit_error=" + (run.saw_explicit_error ? L"yes" : L"no") +
                        L"；partial_success=" + (partial_success ? L"yes" : L"no"));
        if (extraction_ok) {
            if (!MoveTree(attempt, destination, result.changed_files)) {
                AppendLog(L"解壓成功但無法將結果移入輸出資料夾：" + archive.filename().wstring());
                std::error_code cleanup_ec;
                SetFileAttributesW(attempt.wstring().c_str(), FILE_ATTRIBUTE_NORMAL);
                fs::remove_all(attempt, cleanup_ec);
                return result;
            }
            result.success = true;
            result.integrity_warning = partial_success || run.saw_crc_failure || run.saw_explicit_error;
            result.password = passwords[i];
            SetFileAttributesW(attempt.wstring().c_str(), FILE_ATTRIBUTE_NORMAL);
            std::error_code cleanup_ec;
            fs::remove_all(attempt, cleanup_ec);
            AppendLog(L"解壓成功：" + archive.filename().wstring() + L"；回傳碼 " +
                      std::to_wstring(run.exit_code) + L"；耗時 " + FormatElapsed(run.elapsed_ms) +
                      (warning_only ? L"（工具回報警告，但結果已產生）" : L"") +
                      (partial_success ? (run.saw_crc_failure
                          ? L"（工具回報 CRC／校驗錯誤；密碼未判定為錯誤，已移交可讀取的部分，原始壓縮檔不會自動刪除）"
                          : L"（部分檔案成功，但整體回傳碼為 2；原始壓縮檔不會自動刪除）") : L"") + L"。");
            PostStatus(L"第 " + std::to_wstring(depth + 1) + L" 層完成，已產生輸出");
            return result;
        }
        if (!run.launched) {
            AppendLog(L"無法啟動解壓工具：" + engine.wstring());
        } else if (run.cancelled) {
            AppendLog(label + L"：已取消。");
        } else if (run.timed_out) {
            AppendLog(label + L"：超過 30 分鐘沒有工具或檔案活動，已終止子程序。請檢查密碼、磁碟、檔案鎖定及解壓工具版本。");
        } else {
            AppendLog(label + L"：失敗，回傳碼 " + std::to_wstring(run.exit_code) +
                      L"，耗時 " + FormatElapsed(run.elapsed_ms) + L"。");
        }
        if (!run.output_tail.empty()) AppendLog(L"解壓工具最後輸出：\r\n" + run.output_tail);
        if (run.saw_invalid_password) AppendLog(L"診斷：解壓工具明確回報密碼錯誤。");
        else if (run.saw_crc_failure) AppendLog(L"診斷：解壓工具回報 CRC／校驗錯誤；密碼可能已通過，但檔案或分割卷可能不完整。");
        std::error_code cleanup_ec;
        SetFileAttributesW(attempt.wstring().c_str(), FILE_ATTRIBUTE_NORMAL);
        fs::remove_all(attempt, cleanup_ec);
    }
    if (g_cancel_requested.load()) AppendLog(L"處理已取消，未刪除任何未完成的壓縮檔。");
    else AppendLog(L"本層所有密碼均失敗，檔案保留供重新處理：" + archive.filename().wstring());
    return result;
}

static bool IsTransientDeleteError(DWORD error) {
    return error == ERROR_ACCESS_DENIED ||
           error == ERROR_SHARING_VIOLATION ||
           error == ERROR_LOCK_VIOLATION ||
           error == ERROR_USER_MAPPED_FILE;
}

static bool DeleteFileRobust(const fs::path& path, DWORD& final_error, int& attempts, bool& scheduled) {
    final_error = ERROR_SUCCESS;
    attempts = 0;
    scheduled = false;

    // Do not rely on fs::exists(): an ACL/race can make exists() report false even
    // though DeleteFileW would give us a useful Win32 error.
    for (int attempt = 1; attempt <= 8; ++attempt) {
        attempts = attempt;
        SetFileAttributesW(path.wstring().c_str(), FILE_ATTRIBUTE_NORMAL);

        if (DeleteFileW(path.wstring().c_str())) {
            return true;
        }

        const DWORD error = GetLastError();
        final_error = error;
        if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND) {
            return true; // Already gone; treat as successful cleanup.
        }
        if (!IsTransientDeleteError(error)) break;

        // Newly-created archives can briefly be held by Explorer, antivirus,
        // indexers, or thumbnail/preview handlers. Give them several chances
        // before falling back to Windows' reboot-time delete mechanism.
        Sleep(static_cast<DWORD>(150 * attempt));
    }

    // If Windows is still holding the file, ask Windows to remove it at the
    // next reboot. This is deliberately a fallback only: immediate deletion
    // is always attempted first.
    if (IsTransientDeleteError(final_error)) {
        if (MoveFileExW(path.wstring().c_str(), nullptr, MOVEFILE_DELAY_UNTIL_REBOOT)) {
            scheduled = true;
            return true;
        }
        final_error = GetLastError();
    }
    return false;
}

static void DeleteSuccessfulArchiveGroups(const std::vector<ArchiveGroup>& groups) {
    std::set<std::wstring> deleted_keys;
    size_t requested = 0;
    size_t deleted = 0;
    size_t scheduled = 0;
    size_t failed = 0;
    for (const auto& group : groups) {
        for (const auto& part : group.parts) {
            const std::wstring key = PathKey(part);
            if (!deleted_keys.insert(key).second) continue;
            ++requested;

            DWORD error = ERROR_SUCCESS;
            int attempts = 0;
            bool reboot_scheduled = false;
            if (DeleteFileRobust(part, error, attempts, reboot_scheduled)) {
                if (reboot_scheduled) {
                    ++scheduled;
                    AppendLog(L"檔案目前被 Windows 暫時鎖定，已安排重新啟動後自動刪除：" +
                              part.filename().wstring());
                    WriteDiagnostic(L"DELETE_RESULT",
                        L"path=" + part.wstring() + L"；result=scheduled_on_reboot；attempts=" +
                        std::to_wstring(attempts) + L"；win32_error=" + std::to_wstring(error));
                } else {
                    ++deleted;
                    AppendLog(L"已自動刪除成功處理的壓縮檔：" + part.filename().wstring());
                    WriteDiagnostic(L"DELETE_RESULT",
                        L"path=" + part.wstring() + L"；result=deleted；attempts=" +
                        std::to_wstring(attempts));
                }
            } else {
                ++failed;
                AppendLog(L"無法自動刪除成功處理的壓縮檔：" + part.filename().wstring() +
                          L"；Windows 錯誤碼 " + std::to_wstring(error) +
                          L"；已嘗試 " + std::to_wstring(attempts) + L" 次");
                WriteDiagnostic(L"DELETE_RESULT",
                    L"path=" + part.wstring() + L"；result=failed；attempts=" +
                    std::to_wstring(attempts) + L"；win32_error=" + std::to_wstring(error));
            }
        }
    }
    WriteDiagnostic(L"DELETE_SUMMARY", L"requested=" + std::to_wstring(requested) +
                    L"；deleted=" + std::to_wstring(deleted) +
                    L"；scheduled_on_reboot=" + std::to_wstring(scheduled) +
                    L"；failed=" + std::to_wstring(failed));
}

struct ArchiveJob {
    ArchiveGroup group;
    int depth = 0;
};

static void ProcessInput(const fs::path& input, const fs::path& destination, const std::wstring& one_time,
                          bool delete_after_for_job) {
    WriteDiagnostic(L"JOB_START", L"input=" + input.wstring() + L"；destination=" + destination.wstring() +
                    L"；one_time_password_length=" + std::to_wstring(one_time.size()) +
                    L"；delete_after_for_job=" + std::wstring(delete_after_for_job ? L"yes" : L"no") +
                    L"；password_value=REDACTED");
    FlashWorkWindow();
    PostStatus(L"正在分析第一層檔案內容與準備解壓工具...");
    const std::wstring engine_name = FindEngine();
    if (engine_name.empty() || !fs::exists(engine_name)) {
        AppendLog(L"尚未設定有效的解壓工具，請按右上角「設定解壓工具」。");
        return;
    }
    const fs::path engine(engine_name);
    const ArchiveGroup initial_group = ResolveArchiveGroup(input);
    WriteDiagnostic(L"DETECT", L"input=" + input.wstring() + L"；kind=" + KindName(initial_group.kind) +
                    L"；primary=" + initial_group.primary.wstring() + L"；segmented=" +
                    (initial_group.segmented ? L"yes" : L"no") + L"；parts=" + std::to_wstring(initial_group.parts.size()));
    if (initial_group.kind == ArchiveKind::Unknown || initial_group.primary.empty() || !fs::exists(initial_group.primary)) {
        WriteDiagnostic(L"ERROR", L"無法辨識輸入檔案內容或分割檔群組：" + input.wstring());
        AppendLog(L"無法由檔案內容或分割檔命名辨識 ZIP、7Z、RAR 或 LZ4：" + input.filename().wstring());
        return;
    }
    std::error_code ec;
    fs::create_directories(destination, ec);
    if (ec) {
        WriteDiagnostic(L"ERROR", L"無法建立或使用輸出資料夾：" + destination.wstring() + L"；錯誤碼=" + std::to_wstring(ec.value()));
        AppendLog(L"無法建立或使用輸出資料夾：" + destination.wstring());
        return;
    }
    WriteDiagnostic(L"SYSTEM", SystemSnapshot(destination));
    const fs::path initial_archive = RenameWithCorrectExtension(initial_group.primary, initial_group.kind, initial_group.segmented);
    ArchiveGroup normalized_initial = initial_group;
    normalized_initial.primary = initial_archive;
    if (!normalized_initial.segmented) {
        normalized_initial.parts.clear();
        normalized_initial.parts.push_back(initial_archive);
    }
    AppendLog(L"目前解壓工具：" + engine.wstring());
    AppendLog(L"初始格式：" + KindName(normalized_initial.kind) +
              (normalized_initial.segmented ? L"；已辨識完整分割檔群組，共 " + std::to_wstring(normalized_initial.parts.size()) + L" 卷" : L"") +
              L"；輸出位置：" + destination.wstring());
    std::vector<std::wstring> passwords = PasswordCandidates(one_time);
    std::queue<ArchiveJob> jobs;
    jobs.push({normalized_initial, 0});
    std::set<std::wstring> processed;
    std::vector<ArchiveGroup> successful_archives;
    int extracted_layers = 0;

    std::set<std::wstring> before_files;
    {
        std::vector<fs::path> existing;
        CollectRegularFiles(destination, existing);
        for (const auto& path : existing) before_files.insert(PathKey(path));
    }

    while (!jobs.empty()) {
        if (g_cancel_requested.load()) break;
        const ArchiveJob job = jobs.front();
        jobs.pop();
        if (job.depth >= MAX_LAYERS) {
            AppendLog(L"已達到最多 5 層限制，停止掃描更深的壓縮檔。");
            continue;
        }
        const std::wstring key = PathKey(job.group.primary);
        if (processed.count(key)) continue;
        processed.insert(key);
        ExtractionResult extraction = TestAndExtract(job.group, destination, passwords, engine, job.depth);
        if (!extraction.success && !g_cancel_requested.load()) {
            AppendLog(L"第 " + std::to_wstring(job.depth + 1) + L" 層現有密碼均未成功；可輸入該層專用密碼再試一次。");
            std::wstring additional_password;
            if (RequestAdditionalPassword(job.group.primary, job.depth, additional_password)) {
                std::vector<std::wstring> additional{additional_password};
                ExtractionResult retry = TestAndExtract(job.group, destination, additional, engine, job.depth);
                if (retry.success) {
                    passwords.push_back(additional_password);
                    extraction = std::move(retry);
                    AppendLog(L"第 " + std::to_wstring(job.depth + 1) + L" 層使用補充密碼成功；已加入後續層候選。");
                }
            }
        }
        if (!extraction.success) continue;
        ++extracted_layers;
        if (!extraction.integrity_warning) {
            successful_archives.push_back(job.group);
            WriteDiagnostic(L"DELETE_QUEUE", L"archive=" + job.group.primary.wstring() + L"；parts=" +
                            std::to_wstring(job.group.parts.size()) + L"；eligible=yes");
        } else {
            AppendLog(L"第 " + std::to_wstring(job.depth + 1) + L" 層含完整性警告；成功產生的檔案會繼續掃描，但原始壓縮檔不會自動刪除。");
        }
        std::wstring final_marker;
        if (HasFinalGameMarker(extraction.changed_files, final_marker)) {
            WriteDiagnostic(L"FINAL_GAME_MARKER", L"depth=" + std::to_wstring(job.depth + 1) +
                            L"；marker=" + final_marker + L"；action=stop_nested_scan");
            AppendLog(L"偵測到最終遊戲目錄特徵（" + final_marker + L"），停止此遊戲樹的後續壓縮檔掃描。");
            continue;
        }
        std::vector<fs::path> new_files;
        std::set<std::wstring> candidate_keys;
        for (const auto& changed : extraction.changed_files) {
            if (fs::is_regular_file(changed, ec) && candidate_keys.insert(PathKey(changed)).second) new_files.push_back(changed);
        }
        std::vector<fs::path> recursively_discovered;
        // MoveTree 已逐一回傳所有移交的 regular file；只有這份清單異常為空時才啟用昂貴的遞迴回掃。
        if (extraction.changed_files.empty()) {
            CollectNewFiles(destination, before_files, recursively_discovered);
            for (const auto& discovered : recursively_discovered) {
                if (candidate_keys.insert(PathKey(discovered)).second) new_files.push_back(discovered);
            }
        }
        WriteDiagnostic(L"NESTED_SCAN", L"depth=" + std::to_wstring(job.depth + 1) +
                        L"；direct_changed=" + std::to_wstring(extraction.changed_files.size()) +
                        L"；recursive_new=" + std::to_wstring(recursively_discovered.size()) +
                        L"；candidates=" + std::to_wstring(new_files.size()));
        for (const auto& changed : new_files) {
            before_files.insert(PathKey(changed));
            const ArchiveGroup nested_group = ResolveArchiveGroup(changed);
            if (nested_group.kind == ArchiveKind::Unknown || nested_group.primary.empty()) continue;
            const fs::path normalized = RenameWithCorrectExtension(nested_group.primary, nested_group.kind, nested_group.segmented);
            ArchiveGroup normalized_nested = nested_group;
            normalized_nested.primary = normalized;
            if (!normalized_nested.segmented) {
                normalized_nested.parts.clear();
                normalized_nested.parts.push_back(normalized);
            }
            const std::wstring nested_key = PathKey(normalized);
            if (processed.count(nested_key)) continue;
            WriteDiagnostic(L"NESTED_DETECT", L"depth=" + std::to_wstring(job.depth + 2) + L"；kind=" + KindName(normalized_nested.kind) +
                            L"；primary=" + normalized.wstring() + L"；segmented=" +
                            (normalized_nested.segmented ? L"yes" : L"no") + L"；parts=" + std::to_wstring(normalized_nested.parts.size()));
            AppendLog(L"發現第 " + std::to_wstring(job.depth + 2) + L" 層 " + KindName(normalized_nested.kind) +
                      (normalized_nested.segmented ? L" 分割群組，共 " + std::to_wstring(normalized_nested.parts.size()) + L" 卷" : L"") +
                      L"：" + normalized.filename().wstring());
            jobs.push({normalized_nested, job.depth + 1});
        }
    }

    if (g_cancel_requested.load()) {
        PostStatus(L"處理已取消；未完成的壓縮檔已保留");
        AppendLog(L"處理已取消；未完成的壓縮檔已保留。");
        return;
    }
    WriteDiagnostic(L"DELETE_SETTINGS", std::wstring(L"delete_after_for_job=") + (delete_after_for_job ? L"yes" : L"no") +
                    L"；current_setting=" + (g_delete_after ? L"yes" : L"no") +
                    L"；eligible_groups=" + std::to_wstring(successful_archives.size()));
    if (delete_after_for_job) {
        DeleteSuccessfulArchiveGroups(successful_archives);
    } else {
        AppendLog(L"自動刪除未啟用；成功處理的壓縮檔會保留。可在「輸出與刪除設定」中勾選後按「確定」。");
    }
    g_last_output = destination;
    WriteDiagnostic(L"JOB_DONE", L"成功解壓層數=" + std::to_wstring(extracted_layers) +
                    L"；delete_after_for_job=" + (delete_after_for_job ? L"yes" : L"no") +
                    L"；cancelled=" + (g_cancel_requested.load() ? L"yes" : L"no"));
    PostStatus(L"處理完成：成功解壓 " + std::to_wstring(extracted_layers) + L" 層");
    AppendLog(L"處理完成：成功解壓 " + std::to_wstring(extracted_layers) + L" 層。未成功的壓縮檔會保留，不會被刪除。");
}

static void ApplyFont(HWND hwnd) {
    if (g_font && hwnd) SendMessageW(hwnd, WM_SETFONT, reinterpret_cast<WPARAM>(g_font), TRUE);
}

static void ApplyDialogFonts(HWND dialog) {
    EnumChildWindows(dialog, [](HWND child, LPARAM) -> BOOL {
        ApplyFont(child);
        return TRUE;
    }, 0);
}

static void RebuildMainFonts() {
    if (g_font) {
        DeleteObject(g_font);
        g_font = nullptr;
    }
    if (g_title_font) {
        DeleteObject(g_title_font);
        g_title_font = nullptr;
    }
    const int body_font_height = -std::max(8, static_cast<int>(DpiScale(9, g_dpi) * g_ui_scale));
    const int title_font_height = -std::max(12, static_cast<int>(DpiScale(15, g_dpi) * g_ui_scale));
    g_font = CreateFontW(body_font_height, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                         DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                         CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    g_title_font = CreateFontW(title_font_height, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                               DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                               CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    if (g_main) EnumChildWindows(g_main, [](HWND child, LPARAM) -> BOOL {
        ApplyFont(child);
        return TRUE;
    }, 0);
    if (g_title && g_title_font) SendMessageW(g_title, WM_SETFONT, reinterpret_cast<WPARAM>(g_title_font), TRUE);
}

static void LayoutMainWindow(HWND hwnd, int client_width, int client_height) {
    const int margin = S(18);
    const int gap = S(8);
    const int button_height = S(30);
    const int label_height = S(22);
    const int min_content_width = S(360);
    const int available = std::max(min_content_width, client_width - margin * 2);
    const int top_y = S(58);
    const int min_button_width = S(108);
    const int button_count = 6;
    const int compact_threshold = S(820);
    HWND buttons[] = {g_password_button, g_engine, g_output_settings, g_open_output, g_diagnostics, g_rules};
    int label_y = top_y + button_height + gap;
    if (available < compact_threshold) {
        const int columns = 3;
        const int button_width = std::max(min_button_width, (available - gap * (columns - 1)) / columns);
        for (int i = 0; i < button_count; ++i) {
            if (buttons[i]) {
                const int row = i / columns;
                const int column = i % columns;
                MoveWindow(buttons[i], margin + column * (button_width + gap),
                           top_y + row * (button_height + gap), button_width, button_height, TRUE);
            }
        }
        label_y = top_y + 2 * (button_height + gap);
    } else {
        const int total_gaps = gap * (button_count - 1);
        const int button_width = std::max(min_button_width, (available - total_gaps) / button_count);
        for (int i = 0; i < button_count; ++i) {
            if (buttons[i]) MoveWindow(buttons[i], margin + i * (button_width + gap), top_y, button_width, button_height, TRUE);
        }
    }
    if (g_engine_label) MoveWindow(g_engine_label, margin, S(10), std::max(S(240), available / 2 - gap), label_height, TRUE);
    if (g_output_label) MoveWindow(g_output_label, margin + available / 2 + gap, S(10), std::max(S(180), available / 2 - gap), label_height, TRUE);
    if (g_hint) MoveWindow(g_hint, margin, label_y, available, label_height, TRUE);
    const int log_y = label_y + label_height + gap;
    const int bottom_reserved = S(58);
    const int log_height = std::max(S(160), client_height - log_y - bottom_reserved);
    if (g_log) MoveWindow(g_log, margin, log_y, available, log_height, TRUE);
    const int progress_y = log_y + log_height + gap;
    if (g_progress) MoveWindow(g_progress, margin, progress_y, available, S(14), TRUE);
    if (g_status) MoveWindow(g_status, margin, progress_y + S(20), available, label_height, TRUE);
}

static void AppendLogToControl(const std::wstring& text) {
    if (!g_log) return;
    const std::wstring line = text + L"\r\n";
    const int length = GetWindowTextLengthW(g_log);
    SendMessageW(g_log, EM_SETSEL, length, length);
    SendMessageW(g_log, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(line.c_str()));
    SendMessageW(g_log, EM_SCROLLCARET, 0, 0);
}

static void RefreshHeader() {
    if (g_engine_label) {
        const std::wstring value = g_engine_path.empty() ? L"目前解壓工具：尚未設定" : L"目前解壓工具：" + g_engine_path;
        SetWindowTextW(g_engine_label, value.c_str());
    }
    if (g_output_label) {
        std::wstring mode;
        if (g_output_mode == OutputMode::SourceFolder) mode = L"原始檔案所在資料夾";
        else if (g_output_mode == OutputMode::FixedFolder) mode = L"固定：" + g_output_folder.wstring();
        else mode = L"每次處理時詢問";
        SetWindowTextW(g_output_label, (L"輸出模式：" + mode).c_str());
    }
}

static void ChooseEngine() {
    std::vector<wchar_t> filename(32768, L'\0');
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_main;
    ofn.lpstrFilter = L"解壓工具 (*.exe)\0*.exe\0所有檔案 (*.*)\0*.*\0\0";
    ofn.lpstrFile = filename.data();
    ofn.nMaxFile = static_cast<DWORD>(filename.size());
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (GetOpenFileNameW(&ofn)) {
        const fs::path selected(filename.data());
        const fs::path preferred = NormalizeEnginePath(selected);
        if (IsBandizip(selected) && Lower(preferred.filename().wstring()) != L"bz.exe") {
            MessageBoxW(g_main, L"請選擇 Bandizip 安裝目錄內的 bz.exe。Bandizip.exe 是圖形介面入口，可能彈出互動式密碼視窗；本程式只使用 bz.exe 進行自動解壓。", L"需要 Bandizip 主控台工具", MB_OK | MB_ICONWARNING);
            return;
        }
        g_engine_path = preferred.wstring();
        SaveSettings();
        RefreshHeader();
        WriteDiagnostic(L"USER_ACTION", L"使用者選擇解壓工具：" + g_engine_path);
        AppendLog(L"已選擇解壓工具：" + g_engine_path);
    }
}

static bool ResolveOutputFolder(HWND owner, const fs::path& input, fs::path& destination) {
    if (g_output_mode == OutputMode::SourceFolder) destination = input.parent_path();
    else if (g_output_mode == OutputMode::FixedFolder) destination = g_output_folder;
    else if (!ChooseFolder(owner, input.parent_path(), destination)) {
        WriteDiagnostic(L"USER_ACTION", L"使用者取消輸出資料夾選擇");
        return false;
    }
    if (destination.empty()) destination = input.parent_path();
    std::error_code ec;
    fs::create_directories(destination, ec);
    return !ec;
}

static LRESULT CALLBACK MainProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
        case WM_CREATE: {
            g_main = hwnd;
            DragAcceptFiles(hwnd, TRUE);
            g_dpi = WindowDpi(hwnd);
            InitializeDiagnostics();
            WriteDiagnostic(L"LIFECYCLE", L"主視窗建立；dpi=" + std::to_wstring(g_dpi));
            RecalculateUiScale();
            g_title = CreateWindowW(L"STATIC", L"Auto Unwrap",
                                       WS_CHILD | WS_VISIBLE, S(18), S(10), S(220), S(24), hwnd, nullptr, g_instance, nullptr);
            g_subtitle = CreateWindowW(L"STATIC", L"多層偽裝壓縮檔｜直接拖放即可開始",
                                       WS_CHILD | WS_VISIBLE, S(18), S(34), S(330), S(20), hwnd, nullptr, g_instance, nullptr);
            g_engine_label = CreateWindowExW(0, L"STATIC", L"",
                                       WS_CHILD | WS_VISIBLE | SS_ENDELLIPSIS, S(355), S(12), S(650), S(22),
                                       hwnd, reinterpret_cast<HMENU>(ID_ENGINE_LABEL), g_instance, nullptr);
            ApplyFont(g_engine_label);
            g_password_button = CreateWindowW(L"BUTTON", L"密碼管理",
                                                                                      WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, S(18), S(58), S(112), S(30), hwnd, reinterpret_cast<HMENU>(ID_PASSWORDS), g_instance, nullptr);
            g_engine = CreateWindowW(L"BUTTON", L"設定解壓工具",

                                        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, S(140), S(58), S(130), S(30),
                                        hwnd, reinterpret_cast<HMENU>(ID_ENGINE), g_instance, nullptr);
            g_output_settings = CreateWindowW(L"BUTTON", L"輸出與刪除設定",
                                        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, S(280), S(58), S(145), S(30),
                                        hwnd, reinterpret_cast<HMENU>(ID_OUTPUT_SETTINGS), g_instance, nullptr);
            g_open_output = CreateWindowW(L"BUTTON", L"開啟輸出資料夾",
                                          WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, S(435), S(58), S(145), S(30),
                                          hwnd, reinterpret_cast<HMENU>(ID_OPEN_OUTPUT), g_instance, nullptr);
            g_diagnostics = CreateWindowW(L"BUTTON", L"診斷／匯出報告",
                                          WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, S(590), S(58), S(145), S(30),
                                          hwnd, reinterpret_cast<HMENU>(ID_DIAGNOSTICS), g_instance, nullptr);
            g_rules = CreateWindowW(L"BUTTON", L"偽裝檔名規則",
                                    WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, S(750), S(58), S(145), S(30),
                                    hwnd, reinterpret_cast<HMENU>(ID_DISGUISE_RULES), g_instance, nullptr);
            ApplyFont(g_password_button); ApplyFont(g_engine); ApplyFont(g_output_settings); ApplyFont(g_open_output); ApplyFont(g_diagnostics); ApplyFont(g_rules);
            g_output_label = CreateWindowExW(0, L"STATIC", L"",
                                       WS_CHILD | WS_VISIBLE | SS_ENDELLIPSIS, S(750), S(61), S(255), S(22),
                                       hwnd, reinterpret_cast<HMENU>(ID_OUTPUT_LABEL), g_instance, nullptr);
            ApplyFont(g_output_label);
            g_log = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"首次使用請先選擇解壓工具；之後將檔案拖放到本視窗。\r\n",
                                    WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
                                    S(18), S(96), S(987), S(335), hwnd, reinterpret_cast<HMENU>(ID_LOG), g_instance, nullptr);
            ApplyFont(g_log);
            g_hint = CreateWindowW(L"STATIC", L"拖放區：來源檔案會直接改成真實副檔名；支援 RAR／ZIP／7Z 混合巢狀，最多 5 層。",
                                      WS_CHILD | WS_VISIBLE, S(18), S(440), S(850), S(22), hwnd, nullptr, g_instance, nullptr);
            ApplyFont(g_hint);
            g_progress = CreateWindowExW(0, PROGRESS_CLASSW, L"",
                                     WS_CHILD | WS_VISIBLE | PBS_MARQUEE, S(18), S(468), S(987), S(14),
                                     hwnd, reinterpret_cast<HMENU>(ID_PROGRESS), g_instance, nullptr);
            SendMessageW(g_progress, PBM_SETMARQUEE, FALSE, 0);
            g_status = CreateWindowW(L"STATIC", L"狀態：等待拖放",
                                     WS_CHILD | WS_VISIBLE, S(18), S(493), S(987), S(22), hwnd, reinterpret_cast<HMENU>(ID_STATUS), g_instance, nullptr);
            ApplyFont(g_status);
            g_passwords = LoadPasswords();
            LoadSettings();
            LoadDisguiseRules();
            WriteDiagnostic(L"SETTINGS", L"設定載入完成；engine=" + (g_engine_path.empty() ? L"未設定" : g_engine_path) +
                            L"；output_mode=" + std::to_wstring(static_cast<int>(g_output_mode)) +
                            L"；saved_password_count=" + std::to_wstring(g_passwords.size()) + L"；password_values=REDACTED");
            RefreshHeader();
            RebuildMainFonts();
            RECT client{};
            GetClientRect(hwnd, &client);
            LayoutMainWindow(hwnd, client.right - client.left, client.bottom - client.top);
            PostMessageW(hwnd, WM_APP_FIRST_RUN, 0, 0);
            return 0;
        }
        case WM_APP_FIRST_RUN:
            if (g_engine_path.empty() || !fs::exists(g_engine_path)) {
                MessageBoxW(hwnd, L"首次使用需要選擇解壓縮軟件。若使用 Bandizip，請選擇安裝目錄內的 bz.exe；不要選 Bandizip.exe GUI 入口。也可以選擇 7z.exe 或 7zz.exe。", L"Auto Unwrap 首次設定", MB_OK | MB_ICONINFORMATION);
                ChooseEngine();
                if (g_engine_path.empty()) SetStatus(L"狀態：尚未設定解壓工具");
            }
            return 0;
        case WM_DPICHANGED: {
            g_dpi = HIWORD(wparam);
            RecalculateUiScale();
            const RECT* suggested = reinterpret_cast<const RECT*>(lparam);
            if (suggested) {
                SetWindowPos(hwnd, nullptr, suggested->left, suggested->top,
                             suggested->right - suggested->left, suggested->bottom - suggested->top,
                             SWP_NOZORDER | SWP_NOACTIVATE);
            }
            RebuildMainFonts();
            RECT client{};
            GetClientRect(hwnd, &client);
            LayoutMainWindow(hwnd, client.right - client.left, client.bottom - client.top);
            return 0;
        }
        case WM_SIZE: {
            LayoutMainWindow(hwnd, LOWORD(lparam), HIWORD(lparam));
            return 0;
        }
        case WM_COMMAND: {
            const int id = LOWORD(wparam);
            if (id == ID_PASSWORDS && HIWORD(wparam) == BN_CLICKED) {
                std::wstring ignored;
                ShowPasswordDialog(hwnd, ignored);
            } else if (id == ID_ENGINE && HIWORD(wparam) == BN_CLICKED) {
                ChooseEngine();
            } else if (id == ID_OUTPUT_SETTINGS && HIWORD(wparam) == BN_CLICKED) {
                ShowOutputDialog(hwnd);
                RefreshHeader();
            } else if (id == ID_OPEN_OUTPUT && HIWORD(wparam) == BN_CLICKED) {
                WriteDiagnostic(L"USER_ACTION", L"使用者按下開啟輸出資料夾");
                if (!g_last_output.empty() && fs::exists(g_last_output)) ShellExecuteW(hwnd, L"open", g_last_output.wstring().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
                else MessageBoxW(hwnd, L"目前還沒有成功的輸出資料夾。", L"Auto Unwrap", MB_OK | MB_ICONINFORMATION);
            } else if (id == ID_DIAGNOSTICS && HIWORD(wparam) == BN_CLICKED) {
                ExportDiagnosticReport(hwnd);
            } else if (id == ID_DISGUISE_RULES && HIWORD(wparam) == BN_CLICKED) {
                ShowDisguiseRulesDialog(hwnd);
            }
            return 0;
        }
        case WM_DROPFILES: {
            HDROP drop = reinterpret_cast<HDROP>(wparam);
            std::vector<wchar_t> filename(32768, L'\0');
            if (DragQueryFileW(drop, 0, filename.data(), static_cast<UINT>(filename.size())) > 0) {
                const fs::path input(filename.data());
                WriteDiagnostic(L"USER_ACTION", L"使用者拖放檔案：" + input.wstring());
                if (g_busy.exchange(true)) {
                    WriteDiagnostic(L"USER_ACTION", L"拖放被拒絕：目前已有工作執行中");
                    AppendLogToControl(L"目前已有工作執行中，請等待完成。");
                } else {
                    std::wstring one_time;
                    if (ShowPasswordDialog(hwnd, one_time)) {
                        fs::path destination;
                        if (ResolveOutputFolder(hwnd, input, destination)) {
                            AppendLogToControl(L"開始處理：" + input.filename().wstring());
                            g_cancel_requested.store(false);
                            const bool delete_after_for_job = g_delete_after;
                            WriteDiagnostic(L"DELETE_JOB_MODE", std::wstring(L"本次工作啟用自動刪除：") +
                                            (delete_after_for_job ? L"是" : L"否"));
                            AppendLogToControl(delete_after_for_job
                                ? L"本次工作已啟用：成功解壓後自動刪除完整壓縮檔群組。"
                                : L"本次工作未啟用自動刪除；若要刪除，請在開始拖放前勾選並按「確定」。");
                            SetStatus(L"狀態：處理中；大型檔案會直接解壓，請勿關閉視窗...");
                            if (g_progress) SendMessageW(g_progress, PBM_SETMARQUEE, TRUE, 50);
                            std::thread([input, destination, one_time, delete_after_for_job]() {
                                ProcessInput(input, destination, one_time, delete_after_for_job);
                                if (g_main) PostMessageW(g_main, WM_APP_DONE, 0, 0);
                            }).detach();
                        } else {
                            g_busy = false;
                            AppendLogToControl(L"使用者取消輸出資料夾選擇。");
                        }
                    } else {
                        g_busy = false;
                        WriteDiagnostic(L"USER_ACTION", L"使用者取消密碼設定");
                        AppendLogToControl(L"使用者取消密碼設定。");
                    }
                }
            }
            DragFinish(drop);
            return 0;
        }
        case WM_APP_LOG: {
            auto* text = reinterpret_cast<std::wstring*>(lparam);
            if (text) {
                AppendLogToControl(*text);
                delete text;
            }
            return 0;
        }
        case WM_APP_STATUS: {
            auto* text = reinterpret_cast<std::wstring*>(lparam);
            if (text) {
                SetStatus(L"狀態：" + *text);
                delete text;
            }
            return 0;
        }
        case WM_APP_REQUEST_PASSWORD: {
            auto* request = reinterpret_cast<PasswordRequest*>(lparam);
            if (!request) return 0;
            std::wstring password;
            const bool accepted = ShowPasswordDialog(hwnd, password);
            {
                std::lock_guard<std::mutex> lock(request->mutex);
                request->accepted = accepted && !password.empty();
                request->password = password;
                request->done = true;
            }
            request->condition.notify_one();
            return 0;
        }
        case WM_APP_DONE:
            WriteDiagnostic(L"LIFECYCLE", L"背景工作執行緒回報完成");
            g_busy = false;
            g_cancel_requested.store(false);
            if (g_progress) SendMessageW(g_progress, PBM_SETMARQUEE, FALSE, 0);
            SetStatus(L"狀態：完成；可拖放下一個檔案");
            return 0;
        case WM_CLOSE:
            WriteDiagnostic(L"USER_ACTION", std::wstring(L"使用者要求關閉主視窗；busy=") + (g_busy.load() ? L"yes" : L"no"));
            if (g_busy) {
                const int answer = MessageBoxW(hwnd, L"目前仍在解壓，確定要取消並關閉程式嗎？", L"Auto Unwrap", MB_YESNO | MB_ICONWARNING);
                if (answer != IDYES) return 0;
                g_cancel_requested.store(true);
                WriteDiagnostic(L"USER_ACTION", L"使用者確認取消背景解壓");
                SetStatus(L"狀態：正在取消解壓工具，請稍候...");
            }
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            if (g_font) DeleteObject(g_font);
            if (g_title_font) DeleteObject(g_title_font);
            g_font = nullptr;
            g_title_font = nullptr;
            g_main = nullptr;
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hwnd, message, wparam, lparam);
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show_command) {
    g_instance = instance;
    EnableDpiAwareness();
    HDC screen_dc = GetDC(nullptr);
    if (screen_dc) {
        const int screen_dpi = GetDeviceCaps(screen_dc, LOGPIXELSX);
        if (screen_dpi > 0) g_dpi = static_cast<UINT>(screen_dpi);
        ReleaseDC(nullptr, screen_dc);
    }
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
                INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_STANDARD_CLASSES | ICC_PROGRESS_CLASS};

    InitCommonControlsEx(&controls);
    WNDCLASSW wc{};
    wc.lpfnWndProc = MainProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszClassName = L"AutoUnwrapMainWindow";
    wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    RegisterClassW(&wc);
    HWND window = CreateWindowExW(0, wc.lpszClassName, L"Auto Unwrap",
                                  WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, S(960), S(530),
                                  nullptr, nullptr, instance, nullptr);
    if (!window) {
        CoUninitialize();
        return 1;
    }
    ShowWindow(window, show_command);
    UpdateWindow(window);
    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    CoUninitialize();
    return static_cast<int>(message.wParam);
}
