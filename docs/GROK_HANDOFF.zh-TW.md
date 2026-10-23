# AutoUnwrap 技術交接文檔（供 Grok AI 接手）

編製日期：2026-10-23（臺灣時間）  
Repository：[songezop01/AutoUnwrap](https://github.com/songezop01/AutoUnwrap)  
本次比對的既有基準 commit：`a9cab5a9de6c1f54e55e7430d26b2b3a1e892ae1`  
程式內版本：`3.0.4-settings-delete-fix`

這份文檔交接目前可取得的原始碼、編譯方式、已修正問題及尚未驗證的風險。它不是新版本發行公告。本次同步沒有將程式升級為 v3.0.5 或 v3.1，也沒有重新實作解壓／刪除功能。

可見性決定：2026-10-23，使用者在可見性確認時明確答覆「先保持 Public」。本次依此保留 Public，不變更為 Private；後續接手者不要依最初請求擅自再變更。

## 1. 接手時先讀這一節

1. 先確認 GitHub 的最新預設分支與 commit，再讀本檔、README、`.github/workflows/build.yml`、`src/main.cpp`；若最新版本已有後續修正，以實際 diff 為準，不要把本檔基準當成永遠最新。
2. 保存目前已正常運作的 3.0.4 原始碼。不要用歷史 ZIP、舊 README 標題或版本號較舊的附件，直接覆蓋 GitHub 上較新的修正。
3. 遇到自動刪除問題，先確認執行中 EXE 的 `APP_VERSION`、本次 `DELETE_JOB_MODE`／`JOB_START`、是否有 `DELETE_QUEUE`、以及 `DELETE_RESULT`。不要先假設刪除 API 本身失效。
4. 在修改前說明具體重現方式及預期行為；用測試副本驗證。會改名、解壓或刪除來源的測試，不應指向使用者真實下載檔。
5. 不要索取或提交 GitHub Token、真實壓縮密碼、DPAPI 密碼檔、完整私人診斷紀錄。不要把第三方解壓工具或無關 EXE／DLL 當成原始碼上傳。
6. 本次使用者最新指示為「先保持 Public」，不進行可見性變更。不建立新版本標籤／Release、不重寫 Git 歷史。可見性仍是 GitHub 即時設定；未來有新指示時，先查詢當下狀態再處理。

### 已核對的版本身分

| 項目 | 本次基準 |
|---|---|
| 既有 HEAD | `a9cab5a9de6c1f54e55e7430d26b2b3a1e892ae1` |
| `src/main.cpp` Git blob | `02bfe157621a9134f967b524497eb04646d404c9` |
| `.github/workflows/build.yml` Git blob | `a6a57673197c00feff0fe5ee06c76e7778d8a32c` |
| 程式內版本 | `APP_VERSION = L"3.0.4-settings-delete-fix"` |
| 既有成功 CI | [Actions run 34020506392](https://github.com/songezop01/AutoUnwrap/actions/runs/34020506392)，2026-09-06 15:58（臺灣時間） |

上述 blob 是 Git 物件識別值，不是 SHA-256 發行檔校驗碼。同步文檔的後續 commit 應另外核對，不應冒充原有程式建置的 commit。

## 2. 可取得資料及證據界線

本次工作所在的 ChatGPT 專案鏡像沒有可供比對的新附件內容；`sources/` 沒有可用專案資料，且同步專案來源本來就只能讀取。舊對話的快取提到 ZIP、`build_mingw.sh`、程式可運作及 DLL 問題，但舊附件／舊完整壓縮包無法從該快取實際下載。因此，本次可驗證的完整原始碼基準是 GitHub 現有 repository，不能宣稱已找回或逐檔比對所有歷史附件。

應分開理解下列證據：

- **目前原始碼**：可以確認函式、設定流向、刪除條件及工作流程。
- **Git commit diff**：可以確認 3.0.4 UI 修正與後續編譯錯誤修復的先後。
- **既有 Actions 成功結果**：證明當時 workflow 成功完成，不等於本次在使用者桌面重測 GUI、DLL 載入、檔案鎖或所有解壓案例。
- **歷史使用者回報**：使用者在前次對話說「程序完美運作」。這是先前正常運作的回報，本次沒有重新取得同一組檔案的實機測試證據。
- **`v*_findings.md`、舊 README、`WINDOWS_VERIFICATION.md`**：是歷史分析及操作參考。當中的 Linux 開發環境、報告版本、範例路徑、測試成功敘述，不能直接套用到本次環境。

## 3. 產品用途與架構

AutoUnwrap 是 Windows x64 拖放式工具，處理副檔名偽裝及混合巢狀壓縮檔。它讀取檔案簽名來辨識 ZIP、7Z、RAR4／RAR5、LZ4；按需修正來源檔名，呼叫使用者安裝的解壓工具，再掃描新解出的內容，最多處理五層。

程式為 C++17，核心集中於 `src/main.cpp`，GUI 直接使用 Win32，沒有 Qt／Electron／.NET 框架。壓縮演算法由外部 `bz.exe`、`7z.exe` 或 `7zz.exe` 執行；程式不內嵌壓縮引擎，也不自行下載引擎。

### 3.1 主要檔案

| 路徑 | 用途 |
|---|---|
| `src/main.cpp` | GUI、設定、DPAPI、格式辨識、分割卷、程序執行、掃描佇列、清理與診斷 |
| `src/AutoUnwrap.manifest` | Windows Common Controls 與 DPI 宣告 |
| `src/resource.rc` | 將 manifest 編入 EXE |
| `.github/workflows/build.yml` | Windows MinGW-w64 建置、DLL 蒐集、ZIP 及 Actions artifact |
| `tests/` | 原始碼契約／版面計算檢查，以及部分直接呼叫 7-Zip 的測試 |
| `v28_findings.md` 至 `v303_delete_findings.md` | 歷史問題及當時修正說明 |
| `WINDOWS_VERIFICATION.md` | 歷史實機驗證指南；使用前應改成自己的測試副本與路徑 |
| `docs/GROK_HANDOFF.zh-TW.md` | 本交接文檔 |

注意：本次基準沒有 `build_mingw.sh`。舊文件即使提過這個檔名，也不能據此宣稱已存在。manifest 的 assembly identity 仍為 `2.2.0.0`，不是目前 `APP_VERSION`；後續正式發版應統一版本資訊。

### 3.2 函式導覽

以下行號指本檔基準版本；後續修改後請以函式名稱搜尋。

| 函式／結構 | 約略行號 | 職責 |
|---|---:|---|
| `wWinMain` | 2605 | 初始化 DPI、COM、Common Controls 與主視窗 |
| `MainProc` | 2388 | GUI 事件、設定視窗、拖放、背景工作與訊息接收 |
| `LayoutMainWindow` | 2284 | 隨 `WM_SIZE` 重排主視窗控制項 |
| `AppDataDir` | 197 | 使用者設定目錄 |
| `SavePasswords`／`LoadPasswords` | 337／361 | Windows DPAPI 密碼保存／讀取 |
| `LoadSettings`／`SaveSettings` | 398／420 | legacy `.dat` 與 INI 設定 |
| `OutputDialogState` | 134 | 輸出／刪除設定視窗的暫存狀態 |
| `OutputDialogProc`／`ShowOutputDialog` | 938／1043 | 輸出設定 UI 與確認後保存 |
| `FindEngine` | 1142 | 正規化 Bandizip CLI，尋找備援引擎 |
| `RunProcess` | 1271 | 隱藏子程序、stdin／stdout／stderr、進度及逾時 |
| `DetectArchive` | 1472 | 以副檔名限制與檔案簽名辨識格式 |
| `ArchiveGroup`／`ResolveArchiveGroup` | 1547／1602 | 主卷、卷清單與分割群組解析 |
| `RenameWithCorrectExtension` | 1742 | 原地修正單檔副檔名；分割卷保留必要命名 |
| `HasFinalGameMarker` | 1762 | 遊戲目錄／EXE 停止掃描判斷 |
| `MoveTree` | 1827 | 暫存結果移交及新檔清單 |
| `BuildCommand`／`PasswordCandidates` | 1879／1898 | 引擎參數及密碼候選 |
| `TestAndExtract` | 1917 | 逐密碼直接解壓、錯誤分類與結果移交 |
| `DeleteFileRobust` | 1998 | Windows 單檔刪除、重試及重啟刪除備援 |
| `DeleteSuccessfulArchiveGroups` | 2039 | 去重後清理成功群組的每一卷 |
| `ProcessInput` | 2091 | 整體巢狀工作佇列與最後清理 |

### 3.3 一次工作的資料流

1. `WM_DROPFILES` 只取第一個拖入檔案，透過 `g_busy` 防止同時開啟第二份工作。
2. 顯示密碼視窗，依輸出模式取得目的地；在開始前捕捉 `delete_after_for_job`，以值傳入背景執行緒。
3. `ProcessInput` 取得引擎、解析初始 `ArchiveGroup`，必要時將偽裝來源檔直接改名。這個改名會影響使用者檔案，並非只改暫存副本。
4. `PasswordCandidates` 依序放入一次性輸入、已保存候選、空密碼，並去重。
5. 每次嘗試在目的地建立隱藏 `.autounwrap_tmp_*` 目錄，直接執行解壓。雖名為 `TestAndExtract`，現行流程沒有先對每個候選完整執行一次 `t` 測試再 `x`，以免重複讀取大檔案。
6. `MoveTree` 將可交付結果移入目的地；同名一般檔以不同檔名避免直接覆蓋。成功回傳的檔案清單作為下一層候選，僅在清單為空時啟用較昂貴的輸出目錄回掃。
7. 既有候選全失敗時，透過 `WM_APP_REQUEST_PASSWORD` 要求補充密碼，只重試新輸入；成功後加入後續層候選。
8. 成功且沒有完整性警告的群組加入最後刪除佇列。完成佇列後，依本次固定設定決定是否刪除。

遇到新產生的 `.exe`、`game` 或 `renpy` 特徵會停止這次解壓結果的下一層掃描；尚在佇列中的其他工作仍可繼續。程式不執行新解出的 EXE。

### 3.4 外部引擎與程序

- Bandizip 必須使用 `bz.exe`。選取 `Bandizip.exe` 時，程式嘗試改用同目錄 CLI；找不到有效 CLI 才尋找 7-Zip 等備援。
- Bandizip 密碼參數為 `-p:...`，7-Zip 為 `-p...`；`QuoteArg` 處理 Windows 參數引號。不可把兩種語法混用，也不應根據舊文件某句話移除現行參數引用。
- 使用 `CreateProcessW`，不經過命令殼層執行；stdin 接到 `NUL`，stdout／stderr 接管道，並完整寫入本機 `process-output`。
- 每約 15 秒掃描一次暫存輸出活動；10 分鐘沒有可見活動時提醒，30 分鐘沒有活動或總耗時達 24 小時則終止直接啟動的程序。
- 大檔案解壓與移交是否成功，仍依磁碟空間、檔案系統、引擎支援及來源完整性而定。LZ4 簽名可辨識，不代表所有所選引擎都能解壓。

## 4. 設定保存與密碼

### 4.1 目錄與檔案

`AppDataDir()` 優先由 Windows 取得 Roaming AppData，通常是 `%APPDATA%\AutoUnwrap`；若失敗再讀 `%APPDATA%`，最後才退回相對路徑 `.\AutoUnwrap`。因此搬動 EXE 通常不會搬動設定，也不能把 EXE 旁邊沒有設定檔誤認為未保存。

| 檔案 | 內容／格式 |
|---|---|
| `settings.ini` | `[AutoUnwrap]` 下的 `engine`、`output_mode`、`output_folder`、`delete_after` |
| `engine.dat` | 引擎路徑，legacy UTF-16 寬字串內容 |
| `output_mode.dat` | legacy 模式值 |
| `output_folder.dat` | legacy 輸出路徑 |
| `delete_after.dat` | legacy 刪除旗標，`1` 啟用 |
| `passwords.dat` | Windows DPAPI 保護的密碼清單 |
| `disguise_rules.dat` | 偽裝檔名規則，UTF-16 文字 |
| `diagnostic.log` | UTF-8 累積診斷 |
| `process-output\process-*.log` | 引擎完整 stdout／stderr |

`OutputMode` 的值為 `0 = SourceFolder`、`1 = FixedFolder`、`2 = AskEachTime`。

### 4.2 載入與保存

`LoadSettings` 先讀四個 legacy `.dat` 當 fallback，再讀 INI；有 INI 值時以 INI 為準。這是相容舊設定，不是兩份資料依修改時間自動合併。

`SaveSettings` 同時嘗試保存 legacy 與 INI，回傳布林結果並記錄 `SETTINGS_SAVE`。`ShowOutputDialog` 將已接受的視窗狀態回寫 globals，保存後再回讀 INI 的 `delete_after`；失敗會顯示 AppData 無法寫入的錯誤。部分其他呼叫點未處理 `SaveSettings` 回傳值。

密碼清單以 `CryptProtectData`／`CryptUnprotectData` 保存，使用者可存多個候選。不要假設將 `passwords.dat` 複製到另一部電腦／帳號就能解密，也不要將它提交 repository。密碼視窗的「確定」會更新一次性輸入並保存清單；取消或 X 不接受該次修改。一次性輸入本身不會因為拿來解壓，就自動變成永久保存的候選。

DPAPI 是本機資料保護；實際密碼仍會在記憶體及解壓工具的程序命令列出現，並不是無法由同機有權限的程序讀取。

## 5. 版本沿革與自動刪除 BUG 修正鏈

### 5.1 功能沿革：歷史文件所述

Git 初次匯入前沒有完整逐版 commit 可供還原；下表主要整理 README／findings，不能視為每一版均有可重現發行檔。

| 版本 | 歷史記錄的重點 |
|---|---|
| 2.2 | 高 DPI、Per-Monitor DPI、字型與縮放 |
| 2.3 | 子程序輸出、活動監測、逾時與取消 |
| 2.4 | 改用 Bandizip CLI；保留分割卷命名 |
| 2.5–2.6 | 引擎密碼語法、分割檔入口、偽裝 JPG 下一層及部分輸出處理 |
| 2.7 | 持久化診斷、完整工具輸出、系統資訊與報告匯出 |
| 2.8 | 將 CRC 附帶的 `Wrong password?` 與明確密碼錯誤分開；部分成功繼續掃描但保留原檔 |
| 2.9 | 減少回掃 I/O、限制深度簽名掃描、防止 APK／SAVE／PYC 等誤判、遊戲目錄停止條件 |
| 2.9.1 | 任何層級候選失敗都能補充密碼，包含手動拖入第二層 |
| 3.0 | 主視窗響應式排版、INI 權威設定、自訂偽裝檔名規則 |
| 3.0.1 | 刪除群組及 Windows 刪除錯誤診斷；保存／回讀刪除設定 |
| 3.0.2 | 本次工作固定捕捉 `delete_after_for_job`，畫面清楚顯示啟用狀態 |
| 3.0.3 | 暫時檔案鎖重試、已不存在視為完成、重啟刪除備援 |
| 3.0.4 | 修正輸出設定未建立保存按鈕及關閉丟棄勾選的 UI 問題；補足保存錯誤處理 |

`v302_delete_findings.md` 記錄當時使用者報告其實來自 `3.0.0-feature`，本次工作 `delete_after=no`，且使用者在工作完成後才切換勾選。這是那次歷史個案的證據，不應拿來斷言所有後續問題都是使用者操作造成。

### 5.2 Git 可直接證實的沿革

| Commit | 內容與意義 |
|---|---|
| [`2f323e1`](https://github.com/songezop01/AutoUnwrap/commit/2f323e1d07cd302c5142d58451e87696453c2f18) | 初次匯入的主程式已是 `3.0.3-delete-retry`。更早沿革主要保存在文檔中。 |
| [`df53583`](https://github.com/songezop01/AutoUnwrap/commit/df535833c0815d6b495de0d3b010513d605622aa) | 改為 3.0.4，建立輸出設定保存／取消按鈕、保存回傳值及錯誤 UI；但誤把輸出視窗專用的關閉處理貼入另兩種 state，造成無效欄位引用。 |
| [`409d076`](https://github.com/songezop01/AutoUnwrap/commit/409d076314abc69a29a19ccea95fa217bddfcd67) | 調整 workflow。 |
| [`7696be3`](https://github.com/songezop01/AutoUnwrap/commit/7696be3a0399c2a96e1fe5b5b25a0ce98ba0b60a) | 移除密碼與規則對話框誤貼的兩段輸出設定程式碼，共 22 行；保留真正 `OutputDialogProc` 的修正。這是必須保留的較新主程式。 |
| [`a9cab5a`](https://github.com/songezop01/AutoUnwrap/commit/a9cab5a9de6c1f54e55e7430d26b2b3a1e892ae1) | 更新編譯及 DLL 打包 workflow；主程式沒有再改動。 |

### 5.3 3.0.4 的具體設定 BUG

舊 `OutputDialogProc` 已有 `OD_OK`／`OD_CANCEL` handler，卻未實際建立兩個按鈕。勾選框只更新對話框暫存狀態，使用者只能按 X 關閉；舊關閉處理未接受修改，`ShowOutputDialog` 因 `accepted == false` 不會更新 `g_delete_after` 或保存。使用者雖看見勾選，下一份工作仍可能得到 `false`。

現行 `OutputDialogState` 明確包含 `mode`、`delete_after`、`accepted`、`folder` 及控制項 handle。修正後：

- 視窗建立「儲存設定並關閉」及「取消」。
- 點勾選框更新暫存 `delete_after`。
- 按保存會重新讀取勾選值、設 `accepted = true` 再關閉；固定輸出模式且路徑空白時拒絕保存。
- 按 X 的 `WM_CLOSE` 也讀取目前值並設 `accepted = true`，視同保存。
- 按「取消」直接關閉，保留原設定。密碼／偽裝規則視窗的 X 則維持取消語意，不能將輸出視窗的程式碼再貼進去。
- `ShowOutputDialog` 只在 accepted 時回寫 globals、呼叫 `SaveSettings` 並驗證。

按 X 保存是此版明確行為，並非一般使用者可能預期的「關閉等於取消」。後續如要改動，應連同文字與回歸案例一起規劃。

## 6. 自動刪除的安全條件及實際限制

### 6.1 哪些檔案符合刪除資格

本次設定必須在拖放並確認密碼／輸出位置後、背景工作啟動前已啟用。`delete_after_for_job` 以值固定，處理中或完成後切換只影響下一份工作，不會回溯刪除。

`TestAndExtract` 成功、結果已由 `MoveTree` 移交，且 `integrity_warning == false` 的 `ArchiveGroup` 才加入 `successful_archives`。原始碼會把部分成功、CRC／checksum、已辨識的一般工具錯誤排除在刪除資格外。工作被取消時，在最後刪除階段前直接返回。

這是**逐群組判斷**，不是整個巢狀專案必須全數成功。外層成功而下一層密碼失敗時，已成功外層仍可在最後被刪，失敗內層會保留。若產品需求是「任何內層失敗都保留全部外層」，那是後續行為變更，現版沒有這項保證。

### 6.2 如何刪除

`DeleteSuccessfulArchiveGroups` 將每組 `parts` 逐一處理，以正規化、大小寫不敏感的絕對路徑鍵去重。單檔的卷清單會在改名後更新為實際路徑；分割卷按解析到的整組路徑清理。

`DeleteFileRobust` 每次先設 `FILE_ATTRIBUTE_NORMAL`，再呼叫 `DeleteFileW`。最多八次；對 access denied、sharing violation、lock violation、user mapped file 等錯誤逐步等待。若全部都是暫時錯誤，累計等待約 5.4 秒／檔案，再嘗試 `MoveFileExW(..., MOVEFILE_DELAY_UNTIL_REBOOT)`。排程是否成功由 Windows 回傳結果決定，不能把「重試後一定成功」當成保證。

檔案／路徑已不存在視為清理成功。這是永久刪除 API，沒有移入資源回收筒。若修改屬性後仍刪除失敗，程式未回復原屬性。

### 6.3 排查順序

| 診斷事件 | 要確認的內容 |
|---|---|
| `SETTINGS_SAVE` | 設定目錄、保存成功與否、`delete_after` |
| `DELETE_JOB_MODE`／`JOB_START` | 本次工作的固定刪除旗標 |
| `PROCESS_RESULT` | exit code、entries、CRC、密碼提示與部分成功分類 |
| `DELETE_QUEUE` | 該群組是否真的有資格刪除 |
| `DELETE_SETTINGS` | 本次設定、當下設定及合格群組數 |
| `DELETE_RESULT` | 立即完成、`scheduled_on_reboot` 或失敗，及 Windows 錯誤碼／次數 |
| `DELETE_SUMMARY` | requested／deleted／scheduled／failed 統計 |

不要只根據「處理完成」或成功解壓層數推論所有原檔都應刪除。完成狀態也可能包含被保留的失敗／部分成功層。

## 7. 編譯流程、DLL 及發佈內容

### 7.1 現行 GitHub Actions

`Build AutoUnwrap` 在 `main` 的 push 或 `workflow_dispatch` 觸發，使用 `windows-latest`，依序：

1. `actions/checkout@v4` 取原始碼。
2. Chocolatey 安裝 `mingw`，顯示 gcc／g++／windres 版本。
3. 建立 `dist`，編譯 Windows resource，再建置 EXE；檢查退出碼及檔案是否存在。
4. 從實際 `g++.exe` 所在目錄蒐集可取得的 runtime DLL。
5. 將 `dist\*` 壓成 `AutoUnwrap-Windows-x64.zip`。
6. 用 `actions/upload-artifact@v4` 上傳名為 `AutoUnwrap-Windows-x64` 的 artifact。

workflow 沒有執行 `tests/`，沒有建立 GitHub Release，也沒有驗證在乾淨使用者 Windows 環境能啟動。歷史 Actions artifact 可能受保留期限限制，不能只因舊 run 成功就保證今天仍可下載該 ZIP。

基準成功 run 的建置紀錄顯示：Chocolatey 安裝的 `mingw` 套件版本為 `16.1.0`，但 PATH 實際執行的是 `C:\mingw64\bin` 的 `g++ 15.2.0`；該 run 有複製三個 runtime DLL。套件版本、實際編譯器版本與 PATH 命中位置應分別記錄，不能僅以安裝步驟推論有效編譯器版本。

### 7.2 與現行 workflow 一致的核心命令

在 MinGW-w64 的 `g++`、`windres` 已加入 PATH 的 Windows PowerShell，從 repository 根目錄執行：

```powershell
New-Item -ItemType Directory -Force dist | Out-Null
windres src/resource.rc -O coff -o resource.o
g++ -std=c++17 -O2 -mwindows -municode -DUNICODE -D_UNICODE src/main.cpp resource.o -o "dist/AutoUnwrap.exe" -static-libgcc -static-libstdc++ -lshell32 -lcomdlg32 -lcrypt32 -lcomctl32 -lole32 -luuid -ladvapi32 -lgdi32 -luser32
```

這裡呈現實際編譯參數；workflow 另有命令失敗與輸出存在檢查。本次沒有新增或聲稱存在 Linux 的 `build_mingw.sh`。

### 7.3 DLL 包裝與歷史問題

現行編譯只明確指定 `-static-libgcc`、`-static-libstdc++`，**沒有** `-static` 或靜態 pthread 參數。因此不能宣稱 EXE 已完全自足；MinGW 的執行緒 runtime 仍可能需要 `libwinpthread-1.dll`。

workflow 會檢查並複製同一 MinGW 工具鏈目錄的下列 DLL，缺少時輸出訊息後略過：

- `libwinpthread-1.dll`
- `libgcc_s_seh-1.dll`
- `libstdc++-6.dll`

後兩者即使列入蒐集，也不代表 EXE 在目前靜態 runtime 參數下一定動態依賴它們；打包清單和實際 import table 是兩件事。若要刪除或增補 DLL，先檢查 EXE 及 DLL 的實際相依性，並在未安裝 MinGW 的乾淨 Windows 環境啟動測試。

使用者應解壓並保留整包 ZIP 內容，讓需要的 DLL 與 `AutoUnwrap.exe` 放在同一資料夾；不可只取出 EXE，再假設 DLL 問題已解決。不要從隨機 DLL 下載站補檔，應使用建置該 EXE 的相同架構／工具鏈 runtime。

`shell32`、`comdlg32`、`crypt32`、`comctl32`、`ole32`、`uuid`、`advapi32`、`gdi32`、`user32` 是 Windows 連結相關庫，不是應從系統目錄複製進 Git 的第三方程式包。Bandizip／7-Zip 引擎由使用者自行安裝，與 MinGW runtime DLL 分開處理。

## 8. 本次驗證及測試侷限

本次同步檢查記錄為 **9 項通過、1 項失敗**；失敗是舊測試硬編碼版本字串，不能隱瞞，也不應為了讓測試通過而倒退正常程式版本。

| 測試 | 本次結果 | 實際證明範圍 |
|---|---|---|
| `test_diagnostic_contract.py` | 通過 | 原始碼中診斷／遮罩相關契約 |
| `test_dpi_layout.py` | 通過 | Python 版面計算案例，不是 GUI 截圖測試 |
| `test_false_positive_and_final_marker.py` | 通過 | 偽陽性限制與停止條件的原始碼契約 |
| `test_partial_crc_and_queue.py` | 通過 | CRC 分類、掃描及保留條件的原始碼契約 |
| `test_split_password_retry.py` | 通過 | 補充密碼與兩種引擎參數的原始碼契約 |
| `test_v30_features.py` | 通過 | 版面、INI、規則等原始碼契約 |
| `test_split_archives.py` | 通過 | 測試暫存區中實際 7-Zip 分割卷建立／解壓 |
| `test_split_jpg_nested.py` | 通過 | 7-Zip 分割外層與偽裝 ZIP 內層測試 |
| `test_split_nested.py` | 通過 | 7-Zip 巢狀分割卷測試 |
| `test_delete_contract.py` | 失敗 | 仍要求 `3.0.3-delete-retry`，實際程式為 3.0.4；尚未修正此測試 |

後三個通過的測試直接呼叫 7-Zip，沒有執行編譯後的 AutoUnwrap GUI。原始碼契約檢查大量使用字串存在與區塊檢查，只能發現部分結構回歸，不能證明 Windows API 呼叫成功、密碼不會從所有路徑外洩，或真實檔案鎖一定可解除。

本次沒有盲目執行 `create_fixture.py`、`test_fixture.py`、`test_in_place_mixed.py`：它們依賴舊 Linux 絕對 fixture 路徑，fixture 未隨本次可見來源完整提供；建立 fixture 的腳本也不應未經檢查就在現有資料目錄重建。應先改成安全、可設定的獨立暫存測試流程，再於後續工作執行。

本次沒有重新建置 EXE、沒有重新執行 Windows GUI，也沒有把既有成功 CI 當成新測試結果。較新的文檔 commit 可能觸發 workflow；是否成功應另查該次 run，並核對其對應 commit。

## 9. 已知風險與優先處理順序

以下是根據目前原始碼的待改善事項，除另有說明外，未在本次同步中修改，也不應表述成已發生資料損失的實證。

### 優先 1：保護既有行為並補齊關鍵驗證

- 將 `test_delete_contract.py` 的過期版本斷言改成可維護的版本契約，再補具體的保存／取消／X／重啟／本次快照案例，避免只有字串存在檢查。
- 用測試副本實測來源輸出、固定輸出、每次詢問；勾選後保存與重啟；取消設定；單檔／分割卷；部分成功／CRC；檔案鎖；關閉取消；DLL 完整包在乾淨 Windows 啟動。
- 對 `RunProcess` 輸出分類增加測試：目前逐管道 chunk 尋找英文詞，語系不同或關鍵字跨 chunk 邊界可能漏判。
- 現行 exit code 1 且有輸出、未辨識到一般錯誤時可視為 `warning_only`；若沒有 CRC／一般錯誤旗標，仍可能進刪除佇列。因此「任何警告都保留原檔」不是現版保證。應先確認不同引擎警告碼語意，再決定是否收緊。

### 優先 2：程序生命週期及設定一致性

- 背景工作使用 detached `std::thread`。主視窗取消後立即銷毀，沒有 join 等候清理；直接啟動的 CLI 雖有終止邏輯，程序結束前是否來得及執行不保證。也未用 Job Object 管理整個子程序樹，應補明確的取消、等待與結束程序流程。
- 部分全域設定與 `g_disguise_rules` 可在工作期間由 GUI 修改，背景工作亦會讀取；未完全使用鎖或不可變工作快照。應將所有本次參數封裝到工作設定，或在執行時限制修改。
- `SaveSettings` 是多檔雙寫且以 `&&` 串接，某一步失敗會略過同串後續寫入，沒有交易／原子替換。`ShowOutputDialog` 先更新記憶體，失敗後不回復；可出現本次生效但重啟恢復舊設定的差異。
- 輸出設定 X 的保存路徑未套用保存按鈕的「固定資料夾不可空白」驗證，需統一驗證邏輯。密碼／規則保存失敗的回饋也不完整。

### 優先 3：輸入與輸出邊界

- `ResolveArchiveGroup` 以 `std::stoi` 解析數字卷名，極端長數字可拋例外；RAR 補零計算亦需檢查編號位數邊界。背景執行緒沒有頂層例外處理。
- 程式沒有獨立的解壓資源配額、可用空間預留或完整輸出路徑 containment／reparse point 防護，應先審查外部引擎行為再補強，不可僅因使用暫存目錄就宣稱有完整沙箱。
- 檔案移交失敗可能留下部分已交付結果，再刪除剩餘暫存內容；原壓縮檔會保留，但重試時可能產生另名重複檔。需有一致的失敗狀態及可復原紀錄。
- 檔名修正是原地操作；深度掃描及 `game`／`renpy`／任意 EXE 停止策略是啟發式規則，可能提早略過仍需處理的其他壓縮檔。
- 刪除資格保存的是路徑，完成到刪除之間未重新驗證檔案身分或內容；外部程序若改動同一路徑，應評估是否需要額外保護。

### 優先 4：建置可重現性、版本與維護

- `windows-latest` 與 Chocolatey `mingw` 版本未固定，且基準 run 存在套件安裝版本與 PATH 實際工具鏈不同的情況；後續更新可能改變編譯器及 DLL 相依性。建議明確選定工具鏈目錄、記錄／固定版本，加入 import 檢查及打包後啟動測試。
- workflow 缺少測試階段；應依平台及測試類型分開執行，避免把舊 Linux fixture 路徑搬到 Windows 直接跑。
- 目前 `APP_VERSION`、manifest identity、歷史文件版本資訊不同步，尚未有統一版本來源與可信的發行檔校驗／簽章流程。
- 核心集中單檔，可逐步拆成設定、引擎程序、辨識與群組、工作流程、GUI、診斷；拆分應配合行為測試，避免只為重構大改現有正常版本。

## 10. 敏感資料與協作注意

本次對 tracked files 的常見 live credential／私鑰格式掃描未發現相符內容；這不是對任意格式密鑰的完整保證。測試密碼是 fixture 用字，與使用者真實密碼分開。

`diagnostic.log` 會保存檔名、來源／輸出路徑、引擎路徑與系統資訊；歷史 findings 已含個案檔名，Windows 指南亦有歷史磁碟路徑範例。外傳前應檢查與遮罩，不要把原始私人資料再加進交接文檔。

程式自行組成的密碼診斷欄位只記錄候選數量、長度及 `REDACTED`，但外部工具的 raw stdout／stderr 沒有完整敏感內容遮罩；因此不能保證所有 raw output 永遠不含密碼或私人內容。目前沒有日誌輪替／保留上限，長期使用也應處理容量問題。

本次依使用者於 2026-10-23 的最新指示保留 Public，沒有執行可見性變更。提交結果及後續即時可見性仍以 GitHub 與本次同步紀錄為準。

## 11. Grok 接手後的第一份回覆應包含

請先回報你讀到的預設分支／HEAD、`APP_VERSION`、最新 workflow 結果、實際可取得的檔案及尚缺附件。列出這次要解決的具體問題、預計修改的檔案與驗證方式，再進行範圍內的工作。

若只需要維護文檔或同步資料，就保留現行正常原始碼及 workflow。若需要修功能，請從已含 `7696be3` 修正的版本出發，使用獨立分支或可審查的 commit；完成時回報 commit、差異摘要、真實測試結果與尚未驗證的項目，不要把推測或歷史回報寫成新實機測試。
