#pragma once

#include <algorithm>
#include <string>
#include <vector>

// Password candidate order for nested extraction.
// The first entry is tried first. Callers pass the one-time password, then the
// saved list, and this helper always keeps a single empty password at the end
// so an unprotected inner archive can still be opened. Duplicates are dropped.
inline std::vector<std::wstring> BuildPasswordCandidates(const std::wstring& one_time,
                                                         const std::vector<std::wstring>& saved) {
    std::vector<std::wstring> candidates;
    auto add = [&candidates](const std::wstring& value) {
        if (std::find(candidates.begin(), candidates.end(), value) == candidates.end()) {
            candidates.push_back(value);
        }
    };
    if (!one_time.empty()) add(one_time);
    for (const auto& password : saved) add(password);
    add(L"");
    return candidates;
}

// After a layer accepts a password, later layers must try that password first.
// Nested archives often use one outer password and a different inner password
// that then stays the same. Leaving the original order in place makes every
// deeper layer spend its first attempts on passwords that already failed, and
// those attempts are reported as a wrong password.
inline void PromotePassword(std::vector<std::wstring>& passwords, const std::wstring& succeeded) {
    const auto it = std::find(passwords.begin(), passwords.end(), succeeded);
    if (it == passwords.begin()) return;
    if (it != passwords.end()) passwords.erase(it);
    passwords.insert(passwords.begin(), succeeded);
}

// Windows CommandLineToArgvW quoting. CreateProcessW parses this back into the
// original argument, including spaces, quotes, trailing backslashes, and
// non-ASCII characters. The whole argument is always quoted.
inline std::wstring QuoteArg(const std::wstring& arg) {
    std::wstring result = L"\"";
    size_t backslashes = 0;
    for (wchar_t ch : arg) {
        if (ch == L'\\') {
            ++backslashes;
        } else if (ch == L'"') {
            result.append(backslashes * 2 + 1, L'\\');
            result.push_back(L'"');
            backslashes = 0;
        } else {
            result.append(backslashes, L'\\');
            result.push_back(ch);
            backslashes = 0;
        }
    }
    result.append(backslashes * 2, L'\\');
    result.push_back(L'"');
    return result;
}
