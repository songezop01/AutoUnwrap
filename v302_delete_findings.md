# Auto Unwrap v3.0.2 自動刪除診斷

## 實機報告證據

使用者提供的 `AutoUnwrap-diagnostic-20260905-202704.txt` 顯示報告版本為 `3.0.0-feature`，不是 v3.0.1。報告標頭記錄「完成後刪除：否」。該次工作在 20:25:58 開始，`JOB_START`、兩層 `DELETE_QUEUE`、20:26:12 的 `DELETE_SETTINGS` 及 `JOB_DONE` 都顯示 `delete_after=no`。使用者直到 20:26:23 才在工作完成後切換勾選，因此當次工作未啟用自動刪除，設定不會回溯套用。

## v3.0.2 修正

版本識別更新為 `3.0.2-delete-fix`，避免新版與舊版 EXE 混淆。拖放開始且密碼／輸出資料夾確認後，程式會立即把 `g_delete_after` 複製成 `delete_after_for_job` 傳入背景工作，之後該工作只使用這個固定值。畫面日誌會先顯示「本次工作已啟用」或「本次工作未啟用」，診斷事件會記錄 `DELETE_JOB_MODE` 與 `JOB_START`。

因此，開始處理前必須先勾選自動刪除並按下「確定」。處理途中或處理完成後才勾選，只會影響下一個工作，不會回溯刪除已完成的壓縮檔。

## 既有刪除安全機制

完整成功的單檔與分割群組會進入 `DELETE_QUEUE`，完成後透過 `DeleteFileW` 清除唯讀／隱藏／暫存屬性並刪除；CRC／checksum 部分成功不會加入刪除清單。`DELETE_RESULT` 與 `DELETE_SUMMARY` 會記錄每個 Windows 刪除結果及錯誤碼。
