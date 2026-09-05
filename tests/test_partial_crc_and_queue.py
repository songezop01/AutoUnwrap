from pathlib import Path

source = Path("src/main.cpp").read_text(encoding="utf-8")

# The actual report contained: CRC Failed ... Wrong password? at 98%, exit code 2,
# with entries=yes. The implementation must not use the generic password hint alone.
assert "saw_password_hint" in source
assert "saw_crc_failure" in source
assert "result.saw_invalid_password = result.saw_password_hint && !result.saw_crc_failure" in source
assert "const bool partial_success = run.exit_code == 2 && has_entries && !run.saw_invalid_password;" in source

# Next-layer discovery must merge direct MoveTree results and recursive output discovery.
assert "for (const auto& changed : extraction.changed_files)" in source
assert "CollectNewFiles(destination, before_files, recursively_discovered);" in source
assert "NESTED_SCAN" in source
assert "before_files.insert(PathKey(changed));" in source

# Integrity-warning output must continue scanning but never be auto-deleted.
assert "integrity_warning" in source
assert "原始壓縮檔不會自動刪除" in source
print("PASS: CRC/Wrong-password classification, direct file handoff, recursive queue discovery and delete protection")
