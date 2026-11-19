#include "lexer.h"
#include "core.h"
#include <cwctype>
#include <sstream>

const wchar_t* TokName(TokType t) {
    switch (t) {
    case TokType::End: return L"End";
    case TokType::Identifier: return L"Identifier";
    case TokType::Number: return L"Number";
    case TokType::String: return L"String";
    case TokType::KwConst: return L"KwConst(sabit)";
    case TokType::KwPrint: return L"KwPrint(doktur)";
    case TokType::KwIf: return L"KwIf(sayet)";
    case TokType::KwElse: return L"KwElse(yoksa)";
    case TokType::KwWhile: return L"KwWhile(oldukca)";
    case TokType::KwDoWhile: return L"KwDoWhile(bi_yap_hele)";
    case TokType::KwFor: return L"KwFor(cevir_gari)";
    case TokType::KwInt: return L"KwInt(tamsayi)";
    case TokType::KwFloat: return L"KwFloat(hassas_sayi)";
    case TokType::KwDouble: return L"KwDouble(cift_hassas_sayi)";
    case TokType::KwString: return L"KwString(laf)";
    case TokType::KwMain: return L"KwMain(basla_gari)";
    case TokType::KwReturn: return L"KwReturn(dondur_gari)";
    case TokType::KwBreak: return L"KwBreak(kir_gari)";
    case TokType::KwContinue: return L"KwContinue(pas_gec)";
    case TokType::KwBool: return L"KwBool(mantik)";
    case TokType::KwTrue: return L"KwTrue(hakli)";
    case TokType::KwFalse: return L"KwFalse(haksiz)";
    case TokType::KwArray: return L"KwArray(dizi_gari)";
    case TokType::KwFunction: return L"KwFunction(fonksiyon_gari)";
    case TokType::KwVoid: return L"KwVoid(bos_gari)";
    case TokType::KwSwitch: return L"KwSwitch(bak_hele)";
    case TokType::KwCase: return L"KwCase(olursa)";
    case TokType::KwDefault: return L"KwDefault(hicbiri)";
    case TokType::KwStruct: return L"KwStruct(kalip_gari)";
    case TokType::KwTry: return L"KwTry(dene_gari)";
    case TokType::KwCatch: return L"KwCatch(yakala_gari)";
    case TokType::KwThrow: return L"KwThrow(firlat_gari)";
    case TokType::KwForeach: return L"KwForeach(gez_gari)";
    case TokType::KwMap: return L"KwMap(sozluk_gari)";
    case TokType::LParen: return L"("; case TokType::RParen: return L")";
    case TokType::LBrace: return L"{"; case TokType::RBrace: return L"}";
    case TokType::LBracket: return L"["; case TokType::RBracket: return L"]";
    case TokType::Semicolon: return L";"; case TokType::Comma: return L",";
    case TokType::Colon: return L":"; case TokType::Dot: return L".";
    case TokType::Question: return L"?";
    case TokType::Plus: return L"+"; case TokType::Minus: return L"-";
    case TokType::Star: return L"*"; case TokType::Slash: return L"/";
    case TokType::Percent: return L"%"; case TokType::Assign: return L"=";
    case TokType::PlusAssign: return L"+="; case TokType::MinusAssign: return L"-=";
    case TokType::StarAssign: return L"*="; case TokType::SlashAssign: return L"/=";
    case TokType::PercentAssign: return L"%=";
    case TokType::PlusPlus: return L"++"; case TokType::MinusMinus: return L"--";
    case TokType::Eq: return L"=="; case TokType::Ne: return L"!=";
    case TokType::Lt: return L"<"; case TokType::Gt: return L">";
    case TokType::Le: return L"<="; case TokType::Ge: return L">=";
    case TokType::AndAnd: return L"&&"; case TokType::OrOr:  return L"||";
    case TokType::Bang: return L"!";
    default: return L"?";
    }
}

wchar_t Lexer::adv() {
    if (i >= src.size()) return L'\0';
    wchar_t c = src[i++];
    if (c == L'\n') { line++; col = 1; }
    else { col++; }
    return c;
}

void Lexer::skip_ws_and_comments() {
    while (true) {
        wchar_t c = peek();
        if (c == L'\0') return;
        if (iswspace(c)) { adv(); continue; }
        if (c == L'/' && peek2() == L'/') {
            while (peek() != L'\0' && peek() != L'\n') adv();
            continue;
        }
        if (c == L'/' && peek2() == L'*') {
            int cLine = line, cCol = col;
            adv(); adv();
            while (peek() != L'\0' && !(peek() == L'*' && peek2() == L'/')) {
                adv();
            }
            if (peek() == L'*' && peek2() == L'/') {
                adv(); adv();
            } else {
                throw Core_LexError(cLine, cCol, "kapanmamis cok satirli yorum (/* ... */)", L"/*");
            }
            continue;
        }
        return;
    }
}

Token Lexer::next() {
    skip_ws_and_comments();
    int startLine = line; int startCol = col;
    Token t; t.line = startLine; t.col = startCol;
    wchar_t c = peek();
    if (c == L'\0') { t.type = TokType::End; return t; }

    if (std::iswdigit(c)) {
        std::wstring num;
        while (std::iswdigit(peek())) num.push_back(adv());
        if (peek() == L'.') {
            num.push_back(adv());
            if (!std::iswdigit(peek())) throw Core_LexError(startLine, startCol, "hatali float literal", num);
            while (std::iswdigit(peek())) num.push_back(adv());
        }
        t.type = TokType::Number; t.lexeme = std::move(num); return t;
    }

    if (c == L'"') {
        adv(); std::wstring s;
        while (peek() != L'\0' && peek() != L'"') {
            wchar_t ch = adv();
            if (ch == L'\\') {
                wchar_t esc = peek();
                if (esc == L'n') { adv(); s.push_back(L'\n'); }
                else if (esc == L'r') { adv(); s.push_back(L'\r'); }
                else if (esc == L't') { adv(); s.push_back(L'\t'); }
                else if (esc == L'"') { adv(); s.push_back(L'"'); }
                else if (esc == L'\\') { adv(); s.push_back(L'\\'); }
                else { s.push_back(ch); }
            } else {
                s.push_back(ch);
            }
        }
        if (peek() == L'"') adv();
        else throw Core_LexError(startLine, startCol, "kapanmamis string", s);
        t.type = TokType::String; t.lexeme = std::move(s); return t;
    }

    if (is_ident_start(c)) {
        std::wstring id; id.push_back(adv());
        while (is_ident_part(peek())) id.push_back(adv());

        if (id == L"sabit") { t.type = TokType::KwConst; t.lexeme = id; return t; }
        if (id == L"doktur") { t.type = TokType::KwPrint;  t.lexeme = id; return t; }
        if (id == L"sayet") { t.type = TokType::KwIf;     t.lexeme = id; return t; }
        if (id == L"yoksa") { t.type = TokType::KwElse;   t.lexeme = id; return t; }
        if (id == L"oldukca") { t.type = TokType::KwWhile;  t.lexeme = id; return t; }
        if (id == L"bi_yap_hele") { t.type = TokType::KwDoWhile; t.lexeme = id; return t; }
        if (id == L"cevir_gari") { t.type = TokType::KwFor;     t.lexeme = id; return t; }
        if (id == L"tamsayi") { t.type = TokType::KwInt;    t.lexeme = id; return t; }
        if (id == L"hassas_sayi") { t.type = TokType::KwFloat;  t.lexeme = id; return t; }
        if (id == L"cift_hassas_sayi") { t.type = TokType::KwDouble; t.lexeme = id; return t; }
        if (id == L"laf") { t.type = TokType::KwString; t.lexeme = id; return t; }
        if (id == L"basla_gari") { t.type = TokType::KwMain;   t.lexeme = id; return t; }
        if (id == L"dondur_gari") { t.type = TokType::KwReturn; t.lexeme = id; return t; }
        if (id == L"kir_gari") { t.type = TokType::KwBreak; t.lexeme = id; return t; }
        if (id == L"pas_gec") { t.type = TokType::KwContinue; t.lexeme = id; return t; }

        if (id == L"mantik") { t.type = TokType::KwBool; t.lexeme = id; return t; }
        if (id == L"hakli") { t.type = TokType::KwTrue; t.lexeme = id; return t; }
        if (id == L"haksiz") { t.type = TokType::KwFalse; t.lexeme = id; return t; }

        if (id == L"dizi_gari") { t.type = TokType::KwArray; t.lexeme = id; return t; }
        if (id == L"fonksiyon_gari") { t.type = TokType::KwFunction; t.lexeme = id; return t; }
        if (id == L"bos_gari") { t.type = TokType::KwVoid; t.lexeme = id; return t; }
        if (id == L"bak_hele") { t.type = TokType::KwSwitch; t.lexeme = id; return t; }
        if (id == L"olursa") { t.type = TokType::KwCase; t.lexeme = id; return t; }
        if (id == L"hicbiri") { t.type = TokType::KwDefault; t.lexeme = id; return t; }
        if (id == L"kalip_gari") { t.type = TokType::KwStruct; t.lexeme = id; return t; }
        if (id == L"dene_gari") { t.type = TokType::KwTry; t.lexeme = id; return t; }
        if (id == L"yakala_gari") { t.type = TokType::KwCatch; t.lexeme = id; return t; }
        if (id == L"firlat_gari") { t.type = TokType::KwThrow; t.lexeme = id; return t; }
        if (id == L"gez_gari") { t.type = TokType::KwForeach; t.lexeme = id; return t; }
        if (id == L"sozluk_gari") { t.type = TokType::KwMap; t.lexeme = id; return t; }

        // Ege ağzı mantıksal operatör alternatifleri
        if (id == L"hem_de") { t.type = TokType::AndAnd; t.lexeme = id; return t; }
        if (id == L"ya_da") { t.type = TokType::OrOr; t.lexeme = id; return t; }
        if (id == L"degil_gari") { t.type = TokType::Bang; t.lexeme = id; return t; }

        t.type = TokType::Identifier; t.lexeme = std::move(id); return t;
    }

    auto one = [&](TokType tp, const wchar_t* lex) {
        Token x; x.type = tp; x.lexeme = lex; x.line = startLine; x.col = startCol; adv(); return x;
        };

    if (c == L'(') return one(TokType::LParen, L"(");
    if (c == L')') return one(TokType::RParen, L")");
    if (c == L'{') return one(TokType::LBrace, L"{");
    if (c == L'}') return one(TokType::RBrace, L"}");
    if (c == L'[') return one(TokType::LBracket, L"[");
    if (c == L']') return one(TokType::RBracket, L"]");
    if (c == L';') return one(TokType::Semicolon, L";");
    if (c == L',') return one(TokType::Comma, L",");
    if (c == L':') return one(TokType::Colon, L":");
    if (c == L'.') return one(TokType::Dot, L".");
    if (c == L'?') return one(TokType::Question, L"?");

    if (c == L'+') {
        if (peek2() == L'=') { adv(); adv(); return Token{ TokType::PlusAssign, L"+=", startLine, startCol }; }
        if (peek2() == L'+') { adv(); adv(); return Token{ TokType::PlusPlus, L"++", startLine, startCol }; }
        return one(TokType::Plus, L"+");
    }
    if (c == L'-') {
        if (peek2() == L'=') { adv(); adv(); return Token{ TokType::MinusAssign, L"-=", startLine, startCol }; }
        if (peek2() == L'-') { adv(); adv(); return Token{ TokType::MinusMinus, L"--", startLine, startCol }; }
        return one(TokType::Minus, L"-");
    }
    if (c == L'*') {
        if (peek2() == L'=') { adv(); adv(); return Token{ TokType::StarAssign, L"*=", startLine, startCol }; }
        return one(TokType::Star, L"*");
    }
    if (c == L'/') {
        if (peek2() == L'=') { adv(); adv(); return Token{ TokType::SlashAssign, L"/=", startLine, startCol }; }
        return one(TokType::Slash, L"/");
    }
    if (c == L'%') {
        if (peek2() == L'=') { adv(); adv(); return Token{ TokType::PercentAssign, L"%=", startLine, startCol }; }
        return one(TokType::Percent, L"%");
    }
    if (c == L'=') {
        if (peek2() == L'=') { adv(); adv(); return Token{ TokType::Eq, L"==", startLine, startCol }; }
        return one(TokType::Assign, L"=");
    }
    if (c == L'!') {
        if (peek2() == L'=') { adv(); adv(); return Token{ TokType::Ne, L"!=", startLine, startCol }; }
        return one(TokType::Bang, L"!");
    }
    if (c == L'<') {
        if (peek2() == L'=') { adv(); adv(); return Token{ TokType::Le, L"<=", startLine, startCol }; }
        return one(TokType::Lt, L"<");
    }
    if (c == L'>') {
        if (peek2() == L'=') { adv(); adv(); return Token{ TokType::Ge, L">=", startLine, startCol }; }
        return one(TokType::Gt, L">");
    }
    if (c == L'&' && peek2() == L'&') { adv(); adv(); return Token{ TokType::AndAnd, L"&&", startLine, startCol }; }
    if (c == L'|' && peek2() == L'|') { adv(); adv(); return Token{ TokType::OrOr, L"||", startLine, startCol }; }

    throw Core_LexError(startLine, startCol, "Bilinmeyen karakter", std::wstring(1, c));
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;
    while (true) {
        Token t = next(); tokens.push_back(t);
        if (t.type == TokType::End) break;
    }
    return tokens;
}

std::wstring TokensToText(const std::vector<Token>& toks) {
    std::wstringstream ss;
    for (auto& x : toks) {
        ss << L"[satir " << x.line << L", sutun " << x.col << L"] " << TokName(x.type);
        if (!x.lexeme.empty()) ss << L"  {" << x.lexeme << L"}";
        ss << L"\r\n";
    }
    return ss.str();
}