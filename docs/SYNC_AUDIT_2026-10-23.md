# AutoUnwrap 專案比對與同步紀錄

日期：2026-10-23（UTC+8）

## 比對結論

本次可取得的最新可驗證專案，是 GitHub `main` 的 `a9cab5a9de6c1f54e55e7430d26b2b3a1e892ae1`。其中主程式版本為 `3.0.4-settings-delete-fix`，設定保存、誤貼程式碼修正及 runtime DLL 打包均已存在。故本次保留程式、資源、workflow 與測試，新增交接文檔、同步紀錄及忽略規則，更新過時的 README／BUILD_NOTE。

本次是「現有可取得資料的比對與文件同步」，不是已找回所有歷史附件或補齊不存在的歷史檔案。沒有以舊對話的版本名稱重建或覆蓋較新的正常原始碼。

## 可用資料與限制

| 來源 | 取得內容 | 處理方式 |
| --- | --- | --- |
| GitHub `songezop01/AutoUnwrap` | 完整現行 27 個 tracked files、7 個 commit、`main`、Actions 狀態與成功建置日誌 | 作為可驗證基準；本機 clone 供比對，提交透過已連接的 GitHub。 |
| 舊對話「修正自動刪除BUG」 | 對話 ID `6a9c10de-8704-83e8-a743-b5b15a431db5`；可讀的修正經過、編譯錯誤、DLL 問題及使用者最後正常運作回報 | 作為歷史回報，並以現行 source、commit diff 與 CI 交叉核對。 |
| 舊對話下載連結 | `AutoUnwrap-v3.0.4-source-fixed.zip`、`build.yml` 僅有舊 sandbox 連結文字；實體附件未提供 | 不宣稱已下載或逐位元比對附件，不重新捏造其內容。 |
| 本機 ChatGPT 專案 `sources/` | 目錄為空 | 沒有可補入的同步原始碼；未修改此唯讀參考區。 |
| 舊 README 列出的額外檔案 | 部分檔案不在 GitHub 現行 tree 或本對話附件中 | 更新檔案表，列明缺少項，不以推測回填。 |

目前不存在的舊參考檔包括：`build_mingw.sh`、`analysis.md`、`v21_findings.md`、`dpi_findings.md`、`hang_findings.md`、`hang_repro.md`、`official_findings_split_bandizip.md`、`corefix_findings.md`、`v27_findings.md`、`video_ui_findings.md`、`tests/test_streaming_attempt.py`。`tests/fixture/` 也未保存。若日後取得原始附件，應先比對 hash、版本、commit 及行為，再決定是否補入。

## 原始碼與建置修正的交叉核對

| 舊對話中的問題／結果 | GitHub 可驗證證據 | 同步決策 |
| --- | --- | --- |
| 自動刪除勾選未保存 | `df53583` 新增輸出設定的實際儲存／取消按鈕、X 保存及保存結果驗證 | 保留 v3.0.4 修正。 |
| `PasswordDialogState`／`DisguiseRuleDialogState` 引用不存在的輸出設定欄位，造成編譯失敗 | `7696be3a0399c2a96e1fe5b5b25a0ce98ba0b60a` 移除兩處誤貼 block | 保留此後版本，不套用更早 3.0.4 檔案。 |
| 編譯成功但缺少 `libwinpthread-1.dll` | `a9cab5a` 在 workflow 收集三個 MinGW runtime DLL，封裝整個 `dist` | 保留現有 workflow。 |
| 最後使用者回報正常運作 | 可讀舊對話有此回報；基準 CI 也成功 | 記為歷史使用者回報，不能當成本次 Windows GUI 重測。 |

基準主程式 blob：`02bfe157621a9134f967b524497eb04646d404c9`。

基準 workflow blob：`a6a57673197c00feff0fe5ee06c76e7778d8a32c`。

`src/AutoUnwrap.manifest` 仍有舊 assembly 版本 `2.2.0.0`；本次未變動資源，應以 `APP_VERSION` 辨認目前程式。沒有建立 v3.0.5／v3.1.0 tag 或 Release。

## 本次提交範圍

| 檔案 | 變更 |
| --- | --- |
| `docs/GROK_HANDOFF.zh-TW.md` | 新增可直接交給 Grok 的繁體中文技術交接，包含現行架構、設定、版本、BUG、DLL、建置、驗證與後續建議。 |
| `docs/SYNC_AUDIT_2026-10-23.md` | 本比對紀錄。 |
| `README.md` | 更新目前版本與交接入口、v3.0.4 保存行為、可用建置方法、測試限制；移除不存在檔案的現行檔案表項目。保留歷史版本說明。 |
| `BUILD_NOTE.txt` | 將舊 Linux／缺少腳本說明改為現有 Windows Actions 建置、DLL 與 artifact 說明。 |
| `.gitignore` | 排除產物、DLL／EXE／封裝檔、個人設定、密碼檔、診斷、常見憑證及測試暫存。忽略規則不能取代提交前檢查，也不會移除已追蹤的敏感內容。 |

沒有提交第三方解壓引擎、未知 EXE／DLL、舊 ZIP、個人 AppData、診斷匯出或憑證。已對現行 tracked 文字及新增文件檢查常見 token／私鑰格式；未發現符合這些格式的真實憑證。這是有限格式掃描，不代表已保證所有歷史內容不含敏感資訊。舊 findings 的案例檔名／路徑屬既有內容，本次未複製整份私人診斷到新增文件。

## 驗證結果

### 基準 GitHub Actions

[Build AutoUnwrap #6](https://github.com/songezop01/AutoUnwrap/actions/runs/34020506392) 對基準 `a9cab5a` 成功，時間為 2026-09-06 15:58（UTC+8）。建置、三個 runtime DLL 複製、ZIP 封裝與上傳步驟均成功。該次日誌顯示實際 `g++` 為 15.2.0，路徑為 `C:\mingw64\bin`；雖安裝步驟顯示其他 MinGW 套件版本，不能據此認定實際編譯器已切換。此差異列為後續工具鏈固定的理由。

該次 artifact 名稱為 `AutoUnwrap-Windows-x64`，內含 `AutoUnwrap-Windows-x64.zip`；2026-10-23 查詢顯示未過期。原始碼庫不需補入這些產物。本次沒有下載或執行該 EXE。

### 本次可用現有測試

由 repository 根目錄使用本機 Python 3.13；7-Zip 測試使用已安裝的 7-Zip。結果為 9 pass、1 fail：

| 測試 | 結果／實際涵蓋 |
| --- | --- |
| `test_diagnostic_contract.py` | PASS；來源契約。 |
| `test_dpi_layout.py` | PASS；Python 中的版面算式／模型。 |
| `test_false_positive_and_final_marker.py` | PASS；來源契約。 |
| `test_partial_crc_and_queue.py` | PASS；來源契約。 |
| `test_split_password_retry.py` | PASS；來源契約。 |
| `test_v30_features.py` | PASS；來源契約。 |
| `test_split_archives.py` | PASS；真實 7-Zip 暫存分卷資料測試。 |
| `test_split_nested.py` | PASS；真實 7-Zip 暫存巢狀資料測試。 |
| `test_split_jpg_nested.py` | PASS；真實 7-Zip 暫存偽裝／巢狀資料測試。 |
| `test_delete_contract.py` | FAIL；第 24 行仍期待 `3.0.3-delete-retry`，現行是 `3.0.4-settings-delete-fix`。此測試在過時版本斷言處中止；後續斷言未執行。 |

`create_fixture.py`、`test_fixture.py`、`test_in_place_mixed.py` 依賴硬編碼舊 Linux fixture 路徑，未執行。fixture 建立器還會先遞迴刪除其固定目錄，必須先改成明確、安全的暫存路徑才能使用。

這些測試沒有執行 AutoUnwrap.exe，不能證明 GUI、DPAPI／INI 重啟保存、DeleteFileW、CRC 真實分類、取消或重開機刪除都通過。本機沒有可用的 `g++`／`windres`，本次未本機重編譯。

### 首份同步提交與新 CI 回讀確認

- 提交：[`3052db50ba2be2b41db05639a77589b0e76da467`](https://github.com/songezop01/AutoUnwrap/commit/3052db50ba2be2b41db05639a77589b0e76da467)，以基準 `a9cab5a` 為 parent，使用預期舊 SHA 檢查更新 `main`，沒有強制推送。
- GitHub 回讀確認 `main` 指向該提交，tree 為 `03bfe9d35fcc991841358ea1c253279d62d9fd1b`；交接文檔文字與本機內容完全一致。差異僅本文件列出的五個檔案，25 個原有檔案的 blob 完全不變，沒有移除檔案。
- 新的 [Actions run 39952822192](https://github.com/songezop01/AutoUnwrap/actions/runs/39952822192) 在 2026-10-23 20:24（UTC+8）成功；對應 SHA 正是 `3052db5`。它是本次新的 CI 建置結果，並非沿用 9 月的成功狀態，也不是 GUI／實際來源刪除測試。
- 新產物：[AutoUnwrap-Windows-x64](https://github.com/songezop01/AutoUnwrap/actions/runs/39952822192/artifacts/12501745800)，Actions artifact ID `12501745800`，查詢時未過期；保存期限依 GitHub 設定。未將其 EXE／DLL／ZIP 提交原始碼庫。
- 本段是首份同步結果的後續文件補充，故補充後的新提交 SHA 與 workflow 可另外查詢；本段不聲稱文件能記錄自身 commit SHA。

## Repository 可見性

2026-10-23 已透過 GitHub 設定頁準備 Private 變更；頁面提示將停用 Advanced Security，並可能停用自訂 Dependabot 警示規則。使用者在最後確認前明確選擇「先保持 Public」，因此取消變更。此次文件同步不代表 repository 已改為 Private。

## 接手時的驗收方式

先確認提交差異僅上述五個檔案，`src/main.cpp`、資源、workflow 與 tests 的 blob 與基準完全一致；再確認 GitHub `main` 指向新提交並可讀取交接文檔。提交 SHA 由 GitHub 提交結果提供，不預先寫入本文件造成自我參照。

後續如需改動行為，請參照交接文檔，先用可丟棄的壓縮檔及全新使用者設定建立 Windows 回歸驗證，再調整程式；不要把「CI 綠燈」或「來源字串測試通過」視為真實刪除行為的完整驗證。
