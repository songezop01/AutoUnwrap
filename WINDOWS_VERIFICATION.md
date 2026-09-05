# Windows 實機驗證指南

## 為什麼需要實機驗證

我目前的開發環境是 Linux，無法直接啟動你電腦上的 Bandizip `bz.exe` 做實機 GUI 驗證。這代表我無法直接觀察 Bandizip 在你的 Windows 環境中的實際行為，例如：

- Bandizip `bz.exe` 是否正確接受 `-p:password` 參數
- Bandizip 是否會彈出互動式密碼視窗
- Bandizip 的 exit code 2 在你的環境中代表什麼
- 大檔案解壓時的實際輸出與錯誤訊息

這些問題只能在你的 Windows 電腦上直接測試才能確認。

## 如何進行實機驗證

### 方法一：使用命令列直接測試 Bandizip

1. 開啟 Windows 命令提示字元（CMD）或 PowerShell。
2. 執行以下命令，將 `你的密碼` 替換為實際密碼：

```cmd
"C:\Program Files\Bandizip\bz.exe" x -y -aou -o:"D:\GAMES\H Game\test_output" -p:你的密碼 "D:\GAMES\H Game\AA5560.rar"
```

3. 觀察輸出結果：
   - 如果成功解壓，代表密碼正確且 Bandizip 參數正確。
   - 如果出現 `ERROR: Invalid password`，代表密碼錯誤或 Bandizip 無法接受該密碼。
   - 如果出現其他錯誤，請記錄錯誤訊息。

### 方法二：使用 7-Zip 交叉驗證

1. 下載並安裝 7-Zip（https://www.7-zip.org/）。
2. 執行以下命令：

```cmd
"C:\Program Files\7-Zip\7z.exe" x -y -aou -o"D:\GAMES\H Game\test_output" -p你的密碼 "D:\GAMES\H Game\AA5560.rar"
```

3. 觀察輸出結果：
   - 如果 7-Zip 成功但 Bandizip 失敗，代表 Bandizip 的密碼參數可能有問題。
   - 如果兩者都失敗，代表密碼可能真的錯誤，或檔案本身有問題。

### 方法三：使用 Auto Unwrap 的測試模式

1. 在 Auto Unwrap 中選擇 7-Zip 作為解壓工具。
2. 拖放同一個檔案，觀察是否成功。
3. 如果 7-Zip 成功但 Bandizip 失敗，代表 Bandizip 的密碼參數可能有問題。

## 遠端協作選項

如果你願意，我可以透過以下方式協助你進行實機驗證：

1. **遠端桌面連線**：你可以使用 Windows 遠端桌面或 TeamViewer 等工具，讓我直接操作你的電腦進行測試。
2. **螢幕分享**：你可以使用 Zoom、Google Meet 或 Discord 等工具，分享你的螢幕讓我觀察實際行為。
3. **日誌收集**：你可以執行上述命令列測試，並將完整的輸出結果提供給我，我可以根據輸出結果進一步診斷問題。

## 常見問題排除

### Bandizip 仍然彈出密碼視窗

如果 Bandizip 仍然彈出密碼視窗，請確認：

1. 你選擇的是 `bz.exe` 而不是 `Bandizip.exe`。
2. `bz.exe` 確實存在於 Bandizip 安裝目錄中。
3. 你的 Bandizip 版本支援命令列模式。

### 大檔案解壓到一半失敗

如果大檔案解壓到一半失敗，請確認：

1. 磁碟空間是否足夠。
2. 檔案路徑是否過長（超過 260 字元）。
3. 是否有防毒軟體或檔案鎖定干擾。
4. 使用 7-Zip 交叉驗證是否成功。

### 分割檔解壓後的 JPG 沒有自動處理

如果分割檔解壓後的 JPG 沒有自動處理，請確認：

1. 你使用的是 v2.6 或更新版本。
2. 分割檔的所有卷都在同一個資料夾中。
3. 分割檔的主卷（如 `.001`、`.zip`、`.part1.rar`）存在且可辨識。

## 聯絡方式

如果你需要進一步協助，請提供以下資訊：

1. 完整的錯誤訊息或日誌。
2. Bandizip 和 7-Zip 的版本號。
3. 檔案的大小和格式。
4. 密碼的長度和特殊字元（不需要提供實際密碼）。

我會根據這些資訊進一步診斷問題並提供修正方案。
