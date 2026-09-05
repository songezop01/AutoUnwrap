# Auto Unwrap v3.0.1 自動刪除分析

## 根因與風險

v3.0 原本在工作結束時只使用 `std::filesystem::remove`，而且只在記憶體中的 `g_delete_after` 為 true 且成功群組沒有完整性警告時嘗試清理。這使得設定沒有被真正保存、檔案具有唯讀／隱藏屬性、分割卷路徑不同或 Windows 刪除失敗時，都很難從畫面判斷原因。

## v3.0.1 修正

輸出設定按下「確定」時會保存 legacy dat 與 `%APPDATA%\\AutoUnwrap\\settings.ini`，並立即回讀 `delete_after` 驗證保存結果。啟動時同時載入這兩種格式，INI 是 v3.0 之後的權威副本。

每個完整成功的 `ArchiveGroup` 會加入 `DELETE_QUEUE`。具 CRC／checksum 警告的部分成功結果不加入清單，因此不會因為誤判完整成功而刪除原始資料。工作結束時，`DeleteSuccessfulArchiveGroups` 會以標準化絕對路徑去重，清除 Windows 檔案屬性，再用 `DeleteFileW` 刪除單檔或整組分割卷。

診斷事件包括 `DELETE_SETTINGS`、`DELETE_QUEUE`、`DELETE_RESULT` 與 `DELETE_SUMMARY`。如果使用者仍看到原始壓縮檔，應檢查 `DELETE_SETTINGS` 是否為 `delete_after=yes`、`DELETE_QUEUE` 是否包含該壓縮檔，以及 `DELETE_RESULT` 的 Windows 錯誤碼。

## 驗證

已完成 Windows x64 PE 編譯，並通過自動刪除契約、單檔多層、7Z／ZIP 分割卷、CRC 完整性保護、診斷及 v3.0 功能回歸測試。由於建置環境不是 Windows，無法直接在使用者桌面確認檔案鎖定或防毒軟體攔截；v3.0.1 會以 Windows 錯誤碼保存這類實機資訊。
