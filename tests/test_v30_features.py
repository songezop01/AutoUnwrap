from pathlib import Path

source = Path("src/main.cpp").read_text(encoding="utf-8")

# Responsive main-window layout must react to both resize and DPI changes.
assert "case WM_SIZE" in source
assert "LayoutMainWindow(hwnd, LOWORD(lparam), HIWORD(lparam));" in source
assert "case WM_DPICHANGED" in source
assert "RebuildMainFonts();" in source
assert "compact_threshold" in source
assert "buttons[i]" in source

# Output/delete settings are persisted in a stable INI and loaded at startup.
assert "SettingsIniFile()" in source
assert "WriteSetting(L\"output_mode\"" in source
assert "WriteSetting(L\"output_folder\"" in source
assert "WriteSetting(L\"delete_after\"" in source
assert "ReadSetting(L\"delete_after\"" in source
assert "SaveSettings();" in source

# Custom filename rules are conservative: verify archive content before renaming.
assert "StripConfiguredDisguiseText" in source
assert "StripArchiveSuffixDecoration" in source
assert "NormalizeDisguisedArchiveName" in source
assert "if (DetectArchive(requested) == ArchiveKind::Unknown) return requested;" in source
assert "g_disguise_rules" in source
assert "偽裝檔名規則" in source

# Do not turn arbitrary unknown files into archives.
assert "if (!explicit_archive && !disguised_archive && !filename_archive_hint) return ArchiveKind::Unknown;" in source

print("PASS: v3.0 responsive UI, persistent settings, conservative disguise rules")
