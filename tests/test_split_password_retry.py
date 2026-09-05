from pathlib import Path

source = Path("src/main.cpp").read_text(encoding="utf-8")

# Bandizip and 7-Zip have different password option syntaxes.
assert 'QuoteArg(L"-p:" + password)' in source
assert 'QuoteArg(password.empty() ? L"-p" : L"-p" + password)' in source

# A manually dropped inner archive is depth 0, so retry must not be gated on depth > 0.
assert 'if (!extraction.success && !g_cancel_requested.load())' in source
assert 'RequestAdditionalPassword(job.group.primary, job.depth, additional_password)' in source
assert 'passwords.push_back(additional_password)' in source
assert 'WM_APP_REQUEST_PASSWORD' in source
assert 'PASSWORD_REQUEST' in source

print("PASS: split-password retry and engine-specific CLI password contract")
