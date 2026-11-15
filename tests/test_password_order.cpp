#include "../src/password_candidates.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

int g_failures = 0;

void Expect(bool condition, const char* message) {
    if (condition) return;
    std::cerr << "FAIL: " << message << "\n";
    ++g_failures;
}

std::wstring UnquoteArg(const std::wstring& quoted) {
    if (quoted.size() < 2 || quoted.front() != L'"' || quoted.back() != L'"') {
        return quoted;
    }
    std::wstring result;
    size_t backslashes = 0;
    for (size_t i = 1; i + 1 < quoted.size(); ++i) {
        const wchar_t ch = quoted[i];
        if (ch == L'\\') {
            ++backslashes;
            continue;
        }
        if (ch == L'"') {
            result.append(backslashes / 2, L'\\');
            if (backslashes % 2 == 0) {
                // A bare quote would end the argument. QuoteArg always escapes quotes.
                return L"<bad-quote>";
            }
            result.push_back(L'"');
            backslashes = 0;
            continue;
        }
        result.append(backslashes, L'\\');
        result.push_back(ch);
        backslashes = 0;
    }
    result.append(backslashes / 2, L'\\');
    return result;
}

void TestCandidateOrder() {
    const std::vector<std::wstring> saved = {L"outer", L"nope", L"inner密碼#1", L"other", L""};
    auto candidates = BuildPasswordCandidates(L"outer", saved);
    Expect(candidates.size() == 5, "duplicate one-time and empty passwords collapse to one each");
    Expect(candidates[0] == L"outer", "one-time password stays first initially");
    Expect(candidates[1] == L"nope", "saved order is preserved");
    Expect(candidates[2] == L"inner密碼#1", "unicode password is kept intact");
    Expect(candidates.back().empty(), "empty password remains available");

    // Layer 1 opens with the outer password. Promotion is a no-op.
    PromotePassword(candidates, L"outer");
    Expect(candidates[0] == L"outer", "already-first password stays first");

    // Layer 2 rejects the outer password and accepts a later saved password.
    PromotePassword(candidates, L"inner密碼#1");
    Expect(candidates[0] == L"inner密碼#1", "successful inner password moves to the front");
    Expect(candidates[1] == L"outer", "previous first password becomes second");
    Expect(candidates.back().empty(), "empty password is not dropped");

    // Layer 3 must try the password that just worked before the outer password.
    Expect(candidates[0] == L"inner密碼#1", "next layer tries the last success first");

    PromotePassword(candidates, L"brand-new");
    Expect(candidates[0] == L"brand-new", "a password supplied mid-job is inserted first");
    Expect(std::find(candidates.begin(), candidates.end(), L"inner密碼#1") != candidates.end(),
           "older successful password remains in the list");
}

void TestQuotingRoundTrip() {
    const std::wstring samples[] = {
        L"",
        L"simple",
        L"has space",
        L"quote\"inside",
        L"trail\\",
        L"trail\\\\",
        L"mix\\\" ",
        L"inner密碼#1",
        L"a&b^c|d",
        L"結束\\",
    };
    for (const auto& sample : samples) {
        const std::wstring quoted = QuoteArg(sample);
        Expect(!quoted.empty() && quoted.front() == L'"' && quoted.back() == L'"', "argument is quoted");
        Expect(UnquoteArg(quoted) == sample, "quoted argument round-trips");
        if (UnquoteArg(quoted) != sample) {
            std::wcerr << L"  sample round-trip mismatch, length=" << sample.size() << L"\n";
        }
    }

    const std::wstring bandizip = QuoteArg(L"-p:" + std::wstring(L"inner密碼#1"));
    Expect(UnquoteArg(bandizip) == L"-p:inner密碼#1", "bandizip password switch keeps unicode");
    const std::wstring seven = QuoteArg(L"-p" + std::wstring(L"a b\"c\\"));
    Expect(UnquoteArg(seven) == L"-pa b\"c\\", "7-zip password switch keeps spaces, quotes, and slashes");
}

}  // namespace

int main() {
    TestCandidateOrder();
    TestQuotingRoundTrip();
    if (g_failures != 0) {
        std::cerr << g_failures << " failure(s)\n";
        return 1;
    }
    std::cout << "PASS: password promotion and argument quoting\n";
    return 0;
}
