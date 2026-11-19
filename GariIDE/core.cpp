#include "core.h"
#include "lexer.h"
#include "ast.h"
#include "typechecker.h"
#include "vm.h"
#include <windows.h>
#include <cmath>

static InputProviderFn gInputProvider = nullptr;
void Core_SetInputProvider(InputProviderFn fn) { gInputProvider = fn; }
InputProviderFn Core_GetInputProvider() { return gInputProvider; }
std::wstring Core_ReadLineBlockingFromIDE() { if (!gInputProvider) return L""; return gInputProvider(); }

std::wstring Core_Widen(const std::string& s) {
    if (s.empty()) return L"";
    int needed = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0); if (needed <= 0) return L"";
    std::wstring out((size_t)needed, L'\0'); MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), &out[0], needed); return out;
}
std::wstring Core_WidenC(const char* s) { return s ? Core_Widen(std::string(s)) : L""; }
std::string Core_WToNarrow(const std::wstring& w) {
    if (w.empty()) return {};
    int needed = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr); if (needed <= 0) return {};
    std::string out((size_t)needed, '\0'); WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), &out[0], needed, nullptr, nullptr); return out;
}

std::wstring GetLineTextAt(const std::wstring& src, int targetLine) {
    if (targetLine < 1) targetLine = 1; int line = 1; size_t start = 0;
    if (targetLine == 1) start = 0;
    else {
        for (size_t k = 0; k < src.size(); ++k) { if (src[k] == L'\n') { line++; if (line == targetLine) { start = k + 1; break; } } }
        if (line != targetLine) return L"";
    }
    size_t end = start; while (end < src.size() && src[end] != L'\n' && src[end] != L'\r') end++; return src.substr(start, end - start);
}

std::wstring CaretLine(int col) {
    if (col < 1) col = 1; std::wstring s; s.reserve((size_t)col + 2); for (int i = 1; i < col; ++i) s.push_back(L' '); s.push_back(L'^'); return s;
}

std::wstring JoinExpected(const std::vector<std::wstring>& v) {
    std::wstring out; for (size_t i = 0; i < v.size(); ++i) { if (i) out += L", "; out += v[i]; } return out;
}

std::wstring BuildTokensTextFromSource(const std::wstring& source) {
    try { Lexer lx(source); auto toks = lx.tokenize(); return TokensToText(toks); }
    catch (const Core_LexError& le) { return L"LexError line " + std::to_wstring(le.line) + L" col " + std::to_wstring(le.col) + L": " + Core_WidenC(le.what()) + L"\r\n"; }
    catch (const std::exception& ex) { return L"(HATA) " + Core_WidenC(ex.what()) + L"\r\n"; }
}

static std::wstring NormalizeCRLF(const std::wstring& s) {
    std::wstring out;
    out.reserve(s.size() + 64);
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == L'\n' && (i == 0 || s[i - 1] != L'\r')) {
            out.push_back(L'\r');
        }
        out.push_back(s[i]);
    }
    return out;
}

std::wstring BuildAstTextFromSource(const std::wstring& source) {
    try { Lexer lx(source); auto toks = lx.tokenize(); Parser ps(std::move(toks)); auto prg = ps.parseProgram(); return NormalizeCRLF(AstToText(*prg)); }
    catch (const Core_LexError& le) { return L"LexError line " + std::to_wstring(le.line) + L" col " + std::to_wstring(le.col) + L": " + Core_WidenC(le.what()) + L"\r\n"; }
    catch (const Core_ParseError& pe) { return L"ParseError line " + std::to_wstring(pe.line) + L" col " + std::to_wstring(pe.col) + L": " + Core_WidenC(pe.what()) + L"\r\n"; }
    catch (const std::exception& ex) { return L"(HATA) " + Core_WidenC(ex.what()) + L"\r\n"; }
}

std::wstring BuildIRTextFromSource(const std::wstring& source) {
    try { Lexer lx(source); auto toks = lx.tokenize(); Parser ps(std::move(toks)); auto prg = ps.parseProgram(); return NormalizeCRLF(DisasmProgram(*prg)); }
    catch (const Core_LexError& le) { return L"LexError line " + std::to_wstring(le.line) + L" col " + std::to_wstring(le.col) + L": " + Core_WidenC(le.what()) + L"\r\n"; }
    catch (const Core_ParseError& pe) { return L"ParseError line " + std::to_wstring(pe.line) + L" col " + std::to_wstring(pe.col) + L": " + Core_WidenC(pe.what()) + L"\r\n"; }
    catch (const std::exception& ex) { return L"(HATA) " + Core_WidenC(ex.what()) + L"\r\n"; }
}

std::wstring RunCode_Interpreter(const std::wstring& source) {
    try {
        Lexer lx(source); auto toks = lx.tokenize(); Parser ps(std::move(toks)); auto prg = ps.parseProgram();
        try { TypeEnv tenv; TypeCheckProgram(*prg, tenv); }
        catch (const Core_TypeError& te) {
            std::wstring out = L"TypeError line " + std::to_wstring(te.line) + L" col " + std::to_wstring(te.col) + L": " + te.msg + L"\r\n";
            std::wstring txt = GetLineTextAt(source, te.line); if (!txt.empty()) { out += L"\r\n> " + txt + L"\r\n  " + CaretLine(te.col) + L"\r\n"; } return out;
        }
        Env env; std::wstringstream out;
        env.prog = prg.get();
        env.outStream = &out;
        for (auto& g : prg->globals) {
            ExecStmt(g.get(), env, out);
        }
        ExecResult r = ExecStmt(prg->main.get(), env, out); std::wstring text = out.str();
        if (r.flow == ExecResult::Flow::Return) text += L"(return: " + ValueToString(r.returnValue) + L")\r\n"; return text;
    }
    catch (const Core_LexError& le) { std::wstring out = L"LexError line " + std::to_wstring(le.line) + L" col " + std::to_wstring(le.col) + L": " + Core_WidenC(le.what()) + L"\r\n"; std::wstring txt = GetLineTextAt(source, le.line); if (!txt.empty()) out += L"\r\n> " + txt + L"\r\n  " + CaretLine(le.col) + L"\r\n"; return out; }
    catch (const Core_ParseError& pe) {
        std::wstring out = L"ParseError line " + std::to_wstring(pe.line) + L" col " + std::to_wstring(pe.col) + L": " + Core_WidenC(pe.what()) + L"\r\n";
        if (!pe.expected.empty()) out += L"Expected: " + JoinExpected(pe.expected) + L"\r\n"; if (!pe.got.empty()) out += L"Got: " + pe.got + L"\r\n";
        std::wstring txt = GetLineTextAt(source, pe.line); if (!txt.empty()) out += L"\r\n> " + txt + L"\r\n  " + CaretLine(pe.col) + L"\r\n"; return out;
    }
    catch (const Core_RuntimeError& re) { std::wstring out = L"RuntimeError line " + std::to_wstring(re.line) + L" col " + std::to_wstring(re.col) + L": " + Core_WidenC(re.what()) + L"\r\n"; std::wstring txt = GetLineTextAt(source, re.line); if (!txt.empty()) out += L"\r\n> " + txt + L"\r\n  " + CaretLine(re.col) + L"\r\n"; return out; }
    catch (const std::exception& ex) { return L"(HATA) " + Core_WidenC(ex.what()) + L"\r\n"; }
}

std::wstring RunCode_VM(const std::wstring& source) {
    try {
        Lexer lx(source); auto toks = lx.tokenize(); Parser ps(std::move(toks)); auto prg = ps.parseProgram();
        try { TypeEnv tenv; TypeCheckProgram(*prg, tenv); }
        catch (const Core_TypeError& te) {
            std::wstring out = L"TypeError line " + std::to_wstring(te.line) + L" col " + std::to_wstring(te.col) + L": " + te.msg + L"\r\n";
            std::wstring txt = GetLineTextAt(source, te.line); if (!txt.empty()) { out += L"\r\n> " + txt + L"\r\n  " + CaretLine(te.col) + L"\r\n"; } return out;
        }
        Compiler comp; Chunk chunk = comp.compileMain(*prg);
        VM vm; auto vr = vm.run(chunk, prg.get()); // ← DÜZELTME: prg.get() eklendi
        std::wstring out = vr.output;
        if (vr.hasReturn) out += L"(return: " + ValueToString(vr.ret) + L")\r\n"; return out;
    }
    catch (const Core_LexError& le) { std::wstring out = L"LexError line " + std::to_wstring(le.line) + L" col " + std::to_wstring(le.col) + L": " + Core_WidenC(le.what()) + L"\r\n"; std::wstring txt = GetLineTextAt(source, le.line); if (!txt.empty()) out += L"\r\n> " + txt + L"\r\n  " + CaretLine(le.col) + L"\r\n"; return out; }
    catch (const Core_ParseError& pe) {
        std::wstring out = L"ParseError line " + std::to_wstring(pe.line) + L" col " + std::to_wstring(pe.col) + L": " + Core_WidenC(pe.what()) + L"\r\n";
        if (!pe.expected.empty()) out += L"Expected: " + JoinExpected(pe.expected) + L"\r\n"; if (!pe.got.empty()) out += L"Got: " + pe.got + L"\r\n";
        std::wstring txt = GetLineTextAt(source, pe.line); if (!txt.empty()) out += L"\r\n> " + txt + L"\r\n  " + CaretLine(pe.col) + L"\r\n"; return out;
    }
    catch (const Core_RuntimeError& re) { std::wstring out = L"RuntimeError line " + std::to_wstring(re.line) + L" col " + std::to_wstring(re.col) + L": " + Core_WidenC(re.what()) + L"\r\n"; std::wstring txt = GetLineTextAt(source, re.line); if (!txt.empty()) out += L"\r\n> " + txt + L"\r\n  " + CaretLine(re.col) + L"\r\n"; return out; }
    catch (const std::exception& ex) { return L"(HATA) " + Core_WidenC(ex.what()) + L"\r\n"; }
    catch (...) { return L"(HATA) bilinmeyen\r\n"; }
}// LNK2001 bağlantı hatasını çözen asıl fonksiyon gövdesi:
std::wstring AstToText(const Program& p) {
    std::wstringstream ss;
    p.dump(ss);
    return ss.str();
}