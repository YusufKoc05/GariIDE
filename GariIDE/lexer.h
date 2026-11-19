#pragma once
#include <string>
#include <vector>
#include <cwctype>

enum class TokType {
    End, Identifier, Number, String,
    KwConst, KwPrint, KwIf, KwElse, KwWhile, KwDoWhile, KwFor,
    KwInt, KwFloat, KwDouble, KwString, KwMain, KwReturn,
    KwBreak, KwContinue,
    KwBool, KwTrue, KwFalse,
    KwArray,
    KwFunction,              // 🌟 "fonksiyon_gari"
    KwVoid,                  // 🌟 "bos_gari"
    KwSwitch,                // 🌟 "bak_hele"
    KwCase,                  // 🌟 "olursa"
    KwDefault,               // 🌟 "hicbiri"
    KwStruct,                // 🌟 "kalip_gari"
    KwTry,                   // 🌟 "dene_gari"
    KwCatch,                 // 🌟 "yakala_gari"
    KwThrow,                 // 🌟 "firlat_gari"
    KwForeach,               // 🌟 "gez_gari"
    KwMap,                   // 🌟 YENİ: "sozluk_gari"
    LParen, RParen, LBrace, RBrace,
    LBracket, RBracket,
    Semicolon, Comma, Colon, Dot, Question,
    Plus, Minus, Star, Slash, Percent, Assign,
    PlusAssign, MinusAssign, StarAssign, SlashAssign, PercentAssign,
    PlusPlus, MinusMinus,
    Eq, Ne, Lt, Gt, Le, Ge, AndAnd, OrOr,
    Bang
};

struct Token {
    TokType type{};
    std::wstring lexeme;
    int line = 1;
    int col = 1;
};

const wchar_t* TokName(TokType t);

struct Lexer {
    std::wstring src;
    size_t i = 0;
    int line = 1;
    int col = 1;

    explicit Lexer(std::wstring s) : src(std::move(s)) {}

    wchar_t peek()  const { return (i < src.size()) ? src[i] : L'\0'; }
    wchar_t peek2() const { return (i + 1 < src.size()) ? src[i + 1] : L'\0'; }
    wchar_t adv();

    static bool is_ident_start(wchar_t c) { return std::iswalpha(c) || c == L'_'; }
    static bool is_ident_part(wchar_t c) { return std::iswalnum(c) || c == L'_'; }

    void skip_ws_and_comments();
    Token next();
    std::vector<Token> tokenize();
};

std::wstring TokensToText(const std::vector<Token>& toks);