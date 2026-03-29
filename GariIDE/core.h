#pragma once
#include <string>

// IDE -> Core: input salayc (oku_gari / oku_tamsayi iin)
using InputProviderFn = std::wstring(*)();
void Core_SetInputProvider(InputProviderFn fn);

// Sa panel ierikleri
std::wstring BuildTokensTextFromSource(const std::wstring& source);
std::wstring BuildAstTextFromSource(const std::wstring& source);
std::wstring BuildIRTextFromSource(const std::wstring& source);

// altrma (stdout metni dner)
std::wstring RunCode_Interpreter(const std::wstring& source);
std::wstring RunCode_VM(const std::wstring& source);
// Back-compat: defaults to VM
inline std::wstring RunCode(const std::wstring& source) { return RunCode_VM(source); }
