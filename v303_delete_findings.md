# Auto Unwrap v3.0.3 自動刪除修正

## 根因

v3.0.2 的刪除邏輯只對每個成功群組的檔案呼叫一次 `DeleteFileW`. 即使解壓程序已結束，Windows Explorer 預覽/縮圖、Windows Defender、搜尋索引或其他短暫檔案鎖仍可能讓一次刪除回傳 sharing/access denied 錯誤，因此使用者會看到「已完成解壓」但原始壓縮檔仍留下。

## 修正

- 新增 `DeleteFileRobust`。
- 刪除前清除 `FILE_ATTRIBUTE_NORMAL`。
- 對常見暫時性鎖定錯誤最多重試 8 次，等待時間逐步增加。
- `ERROR_FILE_NOT_FOUND` / `ERROR_PATH_NOT_FOUND` 視為檔案已經消失，清理視為成功。
- 多次重試仍被 Windows 鎖定時，使用 `MoveFileExW(..., MOVEFILE_DELAY_UNTIL_REBOOT)` 登記重新啟動後刪除。
- 每個檔案記錄 `attempts`、結果與 Win32 錯誤；摘要增加 `scheduled_on_reboot`。
- 原有安全條件不變：只有完整成功且沒有 CRC/checksum 警告的 ArchiveGroup 才會進入刪除佇列。

## 限制

本環境沒有 MinGW-w64 Windows 交叉編譯器，因此無法在這裡重新產生 `dist/AutoUnwrap.exe`。更新後的 `src/main.cpp`、測試及說明文件已放入交付壓縮檔；Windows x64 EXE 需在有 MinGW-w64 的環境執行 `build_mingw.sh` 建置。
