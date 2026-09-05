from pathlib import Path

source = Path("src/main.cpp").read_text(encoding="utf-8")

assert "WriteSetting(L\"delete_after\"" in source
assert "ReadSetting(L\"delete_after\"" in source
assert "saved_delete_after" in source
assert "DELETE_SETTINGS" in source
assert "DELETE_QUEUE" in source
assert "DeleteSuccessfulArchiveGroups" in source
assert "DeleteFileW" in source
assert "DeleteFileRobust" in source
assert "ERROR_SHARING_VIOLATION" in source
assert "ERROR_ACCESS_DENIED" in source
assert "MOVEFILE_DELAY_UNTIL_REBOOT" in source
assert "scheduled_on_reboot" in source
assert "attempts=" in source
assert "SetFileAttributesW(path.wstring().c_str(), FILE_ATTRIBUTE_NORMAL)" in source
assert "DELETE_RESULT" in source
assert "DELETE_SUMMARY" in source
assert "if (!extraction.integrity_warning)" in source
assert "successful_archives.push_back(job.group)" in source
assert "g_delete_after" in source
assert 'APP_VERSION[] = L"3.0.3-delete-retry"' in source
assert "delete_after_for_job" in source
assert "DELETE_JOB_MODE" in source
assert "本次工作未啟用自動刪除" in source
assert "eligible_groups=" in source

# Incomplete/CRC-warning archives must remain protected from automatic deletion.
warning_block = source[source.index("if (!extraction.integrity_warning)"):source.index("std::wstring final_marker", source.index("if (!extraction.integrity_warning)"))]
assert "successful_archives.push_back(job.group)" in warning_block
assert "else" in warning_block
assert "原始壓縮檔不會自動刪除" in warning_block

print("PASS: delete setting persistence, complete-group queue, split cleanup, and safety guard")
