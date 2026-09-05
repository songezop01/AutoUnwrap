from pathlib import Path

source = Path("src/main.cpp").read_text(encoding="utf-8")

# Normal game resources must not be deep-scanned by extension alone.
assert "IsExplicitArchiveExtension" in source
assert "IsDisguisedArchiveExtension" in source
assert "ext == L\".jpg\"" in source
assert "ext == L\".tmp\"" in source
assert "ext == L\".apk\"" not in source.split("static bool IsDisguisedArchiveExtension", 1)[1].split("static bool ShouldDeepScanArchive", 1)[0]
assert "ext == L\".pyc\"" not in source.split("static bool IsDisguisedArchiveExtension", 1)[1].split("static bool ShouldDeepScanArchive", 1)[0]
assert "ext == L\".save\"" not in source.split("static bool IsDisguisedArchiveExtension", 1)[1].split("static bool ShouldDeepScanArchive", 1)[0]

# Final game markers must stop recursive archive processing.
assert "HasFinalGameMarker" in source
assert "lower_name == L\"game\" || lower_name == L\"renpy\"" in source
assert "Lower(path.extension().wstring()) == L\".exe\"" in source
assert "FINAL_GAME_MARKER" in source
assert "action=stop_nested_scan" in source

# The expensive fallback scan is only used when direct handoff is empty.
assert "if (extraction.changed_files.empty())" in source
assert "last_scan >= 15000" in source
print("PASS: false-positive extension filter, final game marker stop, and fast direct handoff contract")
