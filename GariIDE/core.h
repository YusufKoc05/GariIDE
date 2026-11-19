#pragma once
#include "value.h"
#include <string>
#include <vector>
#include <memory>
#include <sstream>
#include <iostream>

// Çalýþma zamaný hatalarý için özel istisnalar
struct Core_LexError : public std::exception {
    int line, col; std::string msg, lex;
    Core_LexError(int l, int c, std::string m, std::wstring lx) : line(l), col(c), msg(std::move(m)) {
        for (wchar_t wc : lx) lex.push_back(static_cast<char>(wc));
    }
    const char* what() const noexcept override { return msg.c_str(); }
};

struct Core_ParseError : public std::exception {
    int line, col; std::string msg; std::vector<std::wstring> expected; std::wstring got;
    Core_ParseError(int l, int c, std::string m, std::vector<std::wstring> exp, std::wstring g) : line(l), col(c), msg(m), expected(exp), got(g) {}
    const char* what() const noexcept override { return msg.c_str(); }
};

struct Core_RuntimeError : public std::exception {
    int line, col; std::string type, msg;
    Core_RuntimeError(int l, int c, std::string t, std::string m) : line(l), col(c), type(t), msg(m) {}
    const char* what() const noexcept override { return msg.c_str(); }
};

struct Core_TypeError : public std::exception {
    int line, col; std::wstring msg;
    Core_TypeError(int l, int c, std::wstring m) : line(l), col(c), msg(m) {}
    Core_TypeError(std::wstring builtinName) : line(1), col(1), msg(L"Yerlesik fonksiyon cagri hatasi: " + builtinName) {}
};

// Yerleþik fonksiyon yönetim imzalarý
using InputProviderFn = std::wstring(*)();
void Core_SetInputProvider(InputProviderFn fn);
InputProviderFn Core_GetInputProvider();
std::wstring Core_ReadLineBlockingFromIDE();

std::wstring Core_Widen(const std::string& s);
std::wstring Core_WidenC(const char* s);
std::string Core_WToNarrow(const std::wstring& w);

std::wstring GetLineTextAt(const std::wstring& src, int targetLine);
std::wstring CaretLine(int col);
std::wstring JoinExpected(const std::vector<std::wstring>& v);

// IDE arayüzü ile konuþan çekirdek derleme fonksiyonlarý
std::wstring BuildTokensTextFromSource(const std::wstring& source);
std::wstring BuildAstTextFromSource(const std::wstring& source);
std::wstring BuildIRTextFromSource(const std::wstring& source);
std::wstring RunCode_Interpreter(const std::wstring& source);
std::wstring RunCode_VM(const std::wstring& source);

// Matematiksel ve sistem fonksiyonlarý baðlayýcý deklarasyonu
struct Program; // Ýleri bildirim
Value EvalBuiltinCall(const std::wstring& name, const std::vector<Value>& args, int line, int col);
std::string ZeroDivMsg(const std::string& op, const Value& a, const Value& b);
std::string UnknownCallMsg(const std::wstring& name);