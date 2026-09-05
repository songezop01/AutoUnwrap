# Auto Unwrap v2.8 實機診斷根因分析

## 觀察到的實際流程

使用者的診斷紀錄顯示，第一層 `AA5560.rar` 由 7-Zip 26.00 處理，密碼 1 於 1 分 24 秒內進度達到 98%，暫存輸出活動達到約 5 GB，且 `entries=yes`。工具最後輸出：`ERROR: CRC Failed in encrypted file. Wrong password?`，回傳碼為 2。

舊版同時將 `Wrong password?` 記為 `invalid_password=yes`，因此 `partial_success=no`；`TestAndExtract` 回傳失敗，`ProcessInput` 立刻執行 `if (!extraction.success) continue`，完全不掃描這一層已經產生的檔案，也就不可能自動排入下一層。這是「已看到大量檔案解出，但沒有下一層」的直接根因。

第二個密碼候選在 0 秒內回報 `Cannot open encrypted archive. Wrong password?`，沒有任何輸出；第三個空密碼候選也是相同結果。因此，第一個候選不是單純被即時判定失敗，而是在接近完成時遇到至少一個檔案的 CRC／校驗錯誤。

## v2.8 修正

子程序現在分別保存 `password_hint`、`crc_failure` 及有效的 `invalid_password`。當輸出同時包含 CRC／checksum 錯誤與 `Wrong password?` 時，`invalid_password` 會保持 false；若 exit code 為 2 且暫存目錄已有檔案，會以部分成功處理，移交目前可讀取的檔案並繼續進行下一層候選掃描。該次結果標記為 `integrity_warning`，因此原始壓縮檔不會被自動刪除。

下一層交接現在同時使用 `MoveTree` 回傳的 `changed_files` 及輸出資料夾遞迴掃描結果，並以大小寫不敏感、標準化的絕對路徑鍵值去重。每層會產生 `NESTED_SCAN` 紀錄，包含直接移交檔案數、遞迴發現數及候選總數；若辨識到下一層，會產生 `NESTED_DETECT` 後排入工作佇列。

## 重要限制

CRC 失敗代表至少有檔案內容或校驗不完整，不能保證所有 5.5G 輸出都正確。v2.8 的目標是避免把可讀取部分全部丟棄、讓下一層仍能繼續嘗試，並保留原始壓縮檔供使用者用 7-Zip／Bandizip 手動完整驗證。若正確密碼下仍有 CRC 失敗，仍可能是下載檔損壞、分割卷缺失、磁碟／檔案系統錯誤或壓縮檔本身部分損壞。
