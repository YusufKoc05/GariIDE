#include "core.h"
#include <windows.h>
#include <string>
#include <vector>
#include <cwctype>
#include <sstream>
#include <stdexcept>
#include <memory>
#include <unordered_map>
#include <iomanip>
#include <cstring>
#include <cmath>

// ===================== Input provider =====================
static InputProviderFn gInputProvider = nullptr;

void Core_SetInputProvider(InputProviderFn fn) { gInputProvider = fn; }

static std::wstring ReadLineBlockingFromIDE() {
    if (!gInputProvider) return L"";
    return gInputProvider();
}

// ===================== Small helpers =====================
static std::wstring Widen(const std::string& s) {
    if (s.empty()) return L"";
    int needed = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    if (needed <= 0) return L"";
    std::wstring out((size_t)needed, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), &out[0], needed);
    return out;
}

static std::wstring WidenC(const char* s) {
    return s ? Widen(std::string(s)) : L"";
}

static std::string WToNarrow(const std::wstring& w) {
    if (w.empty()) return {};
    int needed = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    if (needed <= 0) return {};
    std::string out((size_t)needed, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), &out[0], needed, nullptr, nullptr);
    return out;
}

// ===================== Context helpers (error snippet) =====================
static std::wstring GetLineTextAt(const std::wstring& src, int targetLine) {
    if (targetLine < 1) targetLine = 1;
    int line = 1;
    size_t start = 0;

    if (targetLine == 1) start = 0;
    else {
        for (size_t k = 0; k < src.size(); ++k) {
            if (src[k] == L'\n') {
                line++;
                if (line == targetLine) { start = k + 1; break; }
            }
        }
        if (line != targetLine) return L"";
    }

    size_t end = start;
    while (end < src.size() && src[end] != L'\n' && src[end] != L'\r') end++;
    return src.substr(start, end - start);
}
static std::wstring CaretLine(int col) {
    if (col < 1) col = 1;
    std::wstring s;
    s.reserve((size_t)col + 2);
    for (int i = 1; i < col; ++i) s.push_back(L' ');
    s.push_back(L'^');
    return s;
}
static std::wstring JoinExpected(const std::vector<std::wstring>& v) {
    std::wstring out;
    for (size_t i = 0; i < v.size(); ++i) {
        if (i) out += L", ";
        out += v[i];
    }
    return out;
}

// ===================== Errors =====================
struct LexError : std::exception {
    int line = 1;
    int col = 1;
    std::string msg;
    std::wstring got;
    LexError(int ln, int cl, std::string m, std::wstring g = L"")
        : line(ln), col(cl), msg(std::move(m)), got(std::move(g)) {
    }
    const char* what() const noexcept override { return msg.c_str(); }
};

struct ParseError : std::exception {
    int line = 1;
    int col = 1;
    std::string msg;
    std::wstring got;
    std::vector<std::wstring> expected;

    ParseError(int ln, int cl, std::string m,
        std::vector<std::wstring> exp = {},
        std::wstring g = L"")
        : line(ln), col(cl), msg(std::move(m)), got(std::move(g)), expected(std::move(exp)) {
    }

    const char* what() const noexcept override { return msg.c_str(); }
};

struct RuntimeError : std::exception {
    int line = 1;
    int col = 1;
    std::string kind;
    std::string msg;

    RuntimeError(int ln, int cl, std::string k, std::string m)
        : line(ln), col(cl), kind(std::move(k)), msg(std::move(m)) {
    }

    const char* what() const noexcept override { return msg.c_str(); }
};

// ===================== LEXER =====================
enum class TokType {
    End,
    Identifier, Number, String,

    KwConst,

    KwPrint, KwIf, KwElse, KwWhile, KwDoWhile, KwFor,
    KwInt, KwFloat, KwDouble, KwString,

    KwMain, KwReturn,

    LParen, RParen, LBrace, RBrace, Semicolon, Comma,

    Plus, Minus, Star, Slash, Percent,
    Assign,

    PlusAssign, MinusAssign,

    Eq, Ne, Lt, Gt, Le, Ge,

    AndAnd, OrOr,
};

struct Token {
    TokType type{};
    std::wstring lexeme;
    int line = 1;
    int col = 1;
};

static const wchar_t* TokName(TokType t) {
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

    case TokType::LParen: return L"(";
    case TokType::RParen: return L")";
    case TokType::LBrace: return L"{";
    case TokType::RBrace: return L"}";
    case TokType::Semicolon: return L";";
    case TokType::Comma: return L",";

    case TokType::Plus: return L"+";
    case TokType::Minus: return L"-";
    case TokType::Star: return L"*";
    case TokType::Slash: return L"/";
    case TokType::Percent: return L"%";
    case TokType::Assign: return L"=";

    case TokType::PlusAssign: return L"+=";
    case TokType::MinusAssign: return L"-=";

    case TokType::Eq: return L"==";
    case TokType::Ne: return L"!=";
    case TokType::Lt: return L"<";
    case TokType::Gt: return L">";
    case TokType::Le: return L"<=";
    case TokType::Ge: return L">=";

    case TokType::AndAnd: return L"&&";
    case TokType::OrOr:  return L"||";
    default: return L"?";
    }
}

struct Lexer {
    std::wstring src;
    size_t i = 0;
    int line = 1;
    int col = 1;

    explicit Lexer(std::wstring s) : src(std::move(s)) {}

    wchar_t peek()  const { return (i < src.size()) ? src[i] : L'\0'; }
    wchar_t peek2() const { return (i + 1 < src.size()) ? src[i + 1] : L'\0'; }

    wchar_t adv() {
        if (i >= src.size()) return L'\0';
        wchar_t c = src[i++];
        if (c == L'\n') { line++; col = 1; }
        else { col++; }
        return c;
    }

    static bool is_ident_start(wchar_t c) { return std::iswalpha(c) || c == L'_'; }
    static bool is_ident_part(wchar_t c) { return std::iswalnum(c) || c == L'_'; }

    void skip_ws_and_comments() {
        while (true) {
            wchar_t c = peek();
            if (c == L'\0') return;

            if (iswspace(c)) { adv(); continue; }

            if (c == L'/' && peek2() == L'/') {
                while (peek() != L'\0' && peek() != L'\n') adv();
                continue;
            }
            return;
        }
    }

    Token next() {
        skip_ws_and_comments();

        int startLine = line;
        int startCol = col;

        Token t;
        t.line = startLine;
        t.col = startCol;

        wchar_t c = peek();
        if (c == L'\0') { t.type = TokType::End; return t; }

        if (std::iswdigit(c)) {
            std::wstring num;
            while (std::iswdigit(peek())) num.push_back(adv());
            if (peek() == L'.') {
                num.push_back(adv());
                if (!std::iswdigit(peek()))
                    throw LexError(startLine, startCol, "hatali float literal", num);
                while (std::iswdigit(peek())) num.push_back(adv());
            }
            t.type = TokType::Number; t.lexeme = std::move(num);
            return t;
        }

        if (c == L'"') {
            adv();
            std::wstring s;
            while (peek() != L'\0' && peek() != L'"') s.push_back(adv());
            if (peek() == L'"') adv();
            else throw LexError(startLine, startCol, "kapanmamis string", s);
            t.type = TokType::String; t.lexeme = std::move(s);
            return t;
        }

        if (is_ident_start(c)) {
            std::wstring id;
            id.push_back(adv());
            while (is_ident_part(peek())) id.push_back(adv());

            if (id == L"sabit") { t.type = TokType::KwConst; t.lexeme = id; return t; }

            if (id == L"doktur") { t.type = TokType::KwPrint;  t.lexeme = id; return t; }
            if (id == L"sayet") { t.type = TokType::KwIf;     t.lexeme = id; return t; }
            if (id == L"yoksa") { t.type = TokType::KwElse;   t.lexeme = id; return t; }
            if (id == L"oldukca") { t.type = TokType::KwWhile;  t.lexeme = id; return t; }
            if (id == L"bi_yap_hele") { t.type = TokType::KwDoWhile; t.lexeme = id; return t; }
            if (id == L"cevir_gari") { t.type = TokType::KwFor;    t.lexeme = id; return t; }

            if (id == L"tamsayi") { t.type = TokType::KwInt;    t.lexeme = id; return t; }
            if (id == L"hassas_sayi") { t.type = TokType::KwFloat;  t.lexeme = id; return t; }
            if (id == L"cift_hassas_sayi") { t.type = TokType::KwDouble; t.lexeme = id; return t; }
            if (id == L"laf") { t.type = TokType::KwString; t.lexeme = id; return t; }

            if (id == L"basla_gari") { t.type = TokType::KwMain;   t.lexeme = id; return t; }
            if (id == L"dondur_gari") { t.type = TokType::KwReturn; t.lexeme = id; return t; }

            t.type = TokType::Identifier; t.lexeme = std::move(id);
            return t;
        }

        auto one = [&](TokType tp, const wchar_t* lex) {
            Token x; x.type = tp; x.lexeme = lex; x.line = startLine; x.col = startCol;
            adv();
            return x;
            };

        if (c == L'(') return one(TokType::LParen, L"(");
        if (c == L')') return one(TokType::RParen, L")");
        if (c == L'{') return one(TokType::LBrace, L"{");
        if (c == L'}') return one(TokType::RBrace, L"}");
        if (c == L';') return one(TokType::Semicolon, L";");
        if (c == L',') return one(TokType::Comma, L",");

        if (c == L'+') {
            if (peek2() == L'=') { adv(); adv(); return Token{ TokType::PlusAssign, L"+=", startLine, startCol }; }
            return one(TokType::Plus, L"+");
        }
        if (c == L'-') {
            if (peek2() == L'=') { adv(); adv(); return Token{ TokType::MinusAssign, L"-=", startLine, startCol }; }
            return one(TokType::Minus, L"-");
        }

        if (c == L'*') return one(TokType::Star, L"*");
        if (c == L'/') return one(TokType::Slash, L"/");
        if (c == L'%') return one(TokType::Percent, L"%");

        if (c == L'=') {
            if (peek2() == L'=') { adv(); adv(); return Token{ TokType::Eq, L"==", startLine, startCol }; }
            return one(TokType::Assign, L"=");
        }

        if (c == L'!') {
            if (peek2() == L'=') { adv(); adv(); return Token{ TokType::Ne, L"!=", startLine, startCol }; }
            throw LexError(startLine, startCol, "'!' artik yok. '!=' kullan.", L"!");
        }

        if (c == L'<') {
            if (peek2() == L'=') { adv(); adv(); return Token{ TokType::Le, L"<=", startLine, startCol }; }
            return one(TokType::Lt, L"<");
        }
        if (c == L'>') {
            if (peek2() == L'=') { adv(); adv(); return Token{ TokType::Ge, L">=", startLine, startCol }; }
            return one(TokType::Gt, L">");
        }

        auto isAmp = [&](wchar_t x) { return x == L'&' || x == L'＆'; };
        auto isBar = [&](wchar_t x) { return x == L'|' || x == L'｜'; };

        if (isAmp(c) && isAmp(peek2())) { adv(); adv(); return Token{ TokType::AndAnd, L"&&", startLine, startCol }; }
        if (isBar(c) && isBar(peek2())) { adv(); adv(); return Token{ TokType::OrOr,  L"||", startLine, startCol }; }

        if (isAmp(c)) throw LexError(startLine, startCol, "tek '&' yasak. '&&' kullan.", L"&");
        if (isBar(c)) throw LexError(startLine, startCol, "tek '|' yasak. '||' kullan.", L"|");

        {
            std::string msg = "bilinmeyen karakter";
            throw LexError(startLine, startCol, msg, std::wstring(1, c));
        }
    }

    std::vector<Token> tokenize() {
        std::vector<Token> out;
        while (true) {
            Token t = next();
            out.push_back(t);
            if (t.type == TokType::End) break;
        }
        return out;
    }
};

static std::wstring TokensToText(const std::vector<Token>& toks) {
    std::wstringstream ss;
    for (auto& x : toks) {
        ss << L"[satir " << x.line << L", sutun " << x.col << L"] " << TokName(x.type);
        if (!x.lexeme.empty()) ss << L"  {" << x.lexeme << L"}";
        ss << L"\r\n";
    }
    return ss.str();
}

// ===================== Value =====================
struct Value {
    enum class Type { INT, FLOAT, DOUBLE, STRING };

    Type type = Type::INT;
    int i = 0;
    float f = 0.0f;
    double d = 0.0;
    std::wstring s;

    static Value makeInt(int v) { Value x; x.type = Type::INT; x.i = v; return x; }
    static Value makeFloat(float v) { Value x; x.type = Type::FLOAT; x.f = v; return x; }
    static Value makeDouble(double v) { Value x; x.type = Type::DOUBLE; x.d = v; return x; }
    static Value makeString(const std::wstring& v) { Value x; x.type = Type::STRING; x.s = v; return x; }
};

static std::wstring TrimTrailingZeros(std::wstring s) {
    auto pos = s.find(L'.');
    if (pos == std::wstring::npos) return s;
    while (!s.empty() && s.back() == L'0') s.pop_back();
    if (!s.empty() && s.back() == L'.') s.pop_back();
    if (s.empty()) return L"0";
    return s;
}
static std::wstring FormatDouble(double v) {
    std::wostringstream oss;
    oss << std::fixed << std::setprecision(6) << v;
    return TrimTrailingZeros(oss.str());
}
static std::wstring ValueToString(const Value& v) {
    switch (v.type) {
    case Value::Type::INT: return std::to_wstring(v.i);
    case Value::Type::FLOAT: return FormatDouble((double)v.f);
    case Value::Type::DOUBLE: return FormatDouble(v.d);
    case Value::Type::STRING: return v.s;
    default: return L"";
    }
}

static bool IsNumeric(Value::Type t) {
    return t == Value::Type::INT || t == Value::Type::FLOAT || t == Value::Type::DOUBLE;
}
static double ToDouble(const Value& v) {
    if (v.type == Value::Type::DOUBLE) return v.d;
    if (v.type == Value::Type::FLOAT) return (double)v.f;
    if (v.type == Value::Type::INT) return (double)v.i;
    try { return std::stod(v.s); }
    catch (...) { return 0.0; }
}
static float ToFloat(const Value& v) {
    if (v.type == Value::Type::FLOAT) return v.f;
    if (v.type == Value::Type::DOUBLE) return (float)v.d;
    if (v.type == Value::Type::INT) return (float)v.i;
    try { return (float)std::stod(v.s); }
    catch (...) { return 0.0f; }
}
static int ToInt(const Value& v) {
    if (v.type == Value::Type::INT) return v.i;
    if (v.type == Value::Type::FLOAT) return (int)v.f;
    if (v.type == Value::Type::DOUBLE) return (int)v.d;
    try { return std::stoi(v.s); }
    catch (...) { return 0; }
}
static bool IsTrue(const Value& v) {
    if (v.type == Value::Type::STRING) return !v.s.empty();
    return ToInt(v) != 0;
}
static Value Coerce(Value v, Value::Type target) {
    if (v.type == target) return v;
    if (target == Value::Type::STRING) return Value::makeString(ValueToString(v));
    if (target == Value::Type::DOUBLE) return Value::makeDouble(ToDouble(v));
    if (target == Value::Type::FLOAT)  return Value::makeFloat(ToFloat(v));
    if (target == Value::Type::INT)    return Value::makeInt(ToInt(v));
    return Value::makeInt(ToInt(v));
}

static constexpr double GARI_PI = 3.14159265358979323846;
static constexpr double GARI_E = 2.71828182845904523536;

static double DegToRad(double deg) {
    return deg * GARI_PI / 180.0;
}
static bool ValueEquals(const Value& A, const Value& B) {
    if (A.type == Value::Type::STRING || B.type == Value::Type::STRING) return ValueToString(A) == ValueToString(B);
    if (IsNumeric(A.type) && IsNumeric(B.type)) return ToDouble(A) == ToDouble(B);
    return ValueToString(A) == ValueToString(B);
}
static std::string ZeroDivMsg(const char* op, const Value& A, const Value& B) {
    return std::string("Sifira bolme (") + op + "): " +
        WToNarrow(ValueToString(A)) + " " + op + " " + WToNarrow(ValueToString(B));
}

// ===================== Builtin helpers =====================
static void TrimWSLocal(std::wstring& t) {
    while (!t.empty() && iswspace(t.front())) t.erase(t.begin());
    while (!t.empty() && iswspace(t.back()))  t.pop_back();
}

static const wchar_t* const kBuiltinNames[] = {
    L"oku_gari",
    L"oku_tamsayi",
    L"oku_hassas_sayi",
    L"kok",
    L"us",
    L"mutlak",
    L"taban",
    L"tavan",
    L"yuvarla",
    L"min_gari",
    L"max_gari",
    L"sinus",
    L"kosinus",
    L"tanjant",
    L"cotanjant",
    L"ln",
    L"log10",
    L"ustel",
    L"pi_sayisi",
    L"e_sayisi"
};

static std::wstring SuggestBuiltinName(const std::wstring& name) {
    if (name.empty()) return L"";

    int bestScore = -1;
    std::wstring best;

    for (auto cand : kBuiltinNames) {
        std::wstring c = cand;
        int score = 0;

        size_t common = 0;
        while (common < name.size() && common < c.size() && name[common] == c[common]) common++;
        score += (int)common * 4;

        if (c.find(name) != std::wstring::npos || name.find(c) != std::wstring::npos) score += 6;

        if (!name.empty() && !c.empty() && name[0] == c[0]) score += 2;
        if (std::abs((int)name.size() - (int)c.size()) <= 2) score += 1;

        if (score > bestScore) {
            bestScore = score;
            best = c;
        }
    }

    if (bestScore < 4) return L"";
    return best;
}

static std::string UnknownCallMsg(const std::wstring& name) {
    std::wstring hint = SuggestBuiltinName(name);
    std::wstring msg = L"Bilinmeyen cagri: " + name;
    if (!hint.empty()) msg += L". Belki sunu demek istedin: " + hint;
    return WToNarrow(msg);
}

static std::wstring UnknownCallTypeMsg(const std::wstring& name) {
    std::wstring hint = SuggestBuiltinName(name);
    std::wstring msg = L"Bilinmeyen cagri: " + name;
    if (!hint.empty()) msg += L". Belki sunu demek istedin: " + hint;
    return msg;
}

static void ExpectBuiltinArgCount(const std::wstring& sig, size_t got, size_t want, int line, int col) {
    if (got != want) {
        throw RuntimeError(line, col, "ArgError", WToNarrow(sig) + " dogru arguman sayisi ile cagrilmali.");
    }
}

static void ExpectNumericBuiltinArg(const std::wstring& name, const Value& v, int line, int col) {
    if (!IsNumeric(v.type)) {
        throw RuntimeError(line, col, "TypeError", WToNarrow(name) + " sadece sayi alir.");
    }
}

static Value::Type PromoteValueType(Value::Type a, Value::Type b) {
    if (a == Value::Type::DOUBLE || b == Value::Type::DOUBLE) return Value::Type::DOUBLE;
    if (a == Value::Type::FLOAT || b == Value::Type::FLOAT) return Value::Type::FLOAT;
    return Value::Type::INT;
}

static Value MakeNumericByType(Value::Type t, double v) {
    if (t == Value::Type::DOUBLE) return Value::makeDouble(v);
    if (t == Value::Type::FLOAT)  return Value::makeFloat((float)v);
    return Value::makeInt((int)v);
}

static Value EvalBuiltinCall(const std::wstring& name, const std::vector<Value>& args, int line, int col) {
    if (name == L"oku_gari") {
        ExpectBuiltinArgCount(L"oku_gari()", args.size(), 0, line, col);
        return Value::makeString(ReadLineBlockingFromIDE());
    }

    if (name == L"oku_tamsayi") {
        ExpectBuiltinArgCount(L"oku_tamsayi()", args.size(), 0, line, col);
        std::wstring s = ReadLineBlockingFromIDE();
        TrimWSLocal(s);
        if (s.empty()) throw RuntimeError(line, col, "ValueError", "Tamsayi bekleniyordu.");
        try { return Value::makeInt(std::stoi(s)); }
        catch (...) { throw RuntimeError(line, col, "ValueError", "Tamsayi bekleniyordu."); }
    }

    if (name == L"oku_hassas_sayi") {
        ExpectBuiltinArgCount(L"oku_hassas_sayi()", args.size(), 0, line, col);
        std::wstring s = ReadLineBlockingFromIDE();
        TrimWSLocal(s);
        if (s.empty()) throw RuntimeError(line, col, "ValueError", "Hassas sayi bekleniyordu.");
        try { return Value::makeFloat((float)std::stod(s)); }
        catch (...) { throw RuntimeError(line, col, "ValueError", "Hassas sayi bekleniyordu."); }
    }

    if (name == L"pi_sayisi") {
        ExpectBuiltinArgCount(L"pi_sayisi()", args.size(), 0, line, col);
        return Value::makeDouble(GARI_PI);
    }

    if (name == L"e_sayisi") {
        ExpectBuiltinArgCount(L"e_sayisi()", args.size(), 0, line, col);
        return Value::makeDouble(GARI_E);
    }

    if (name == L"kok") {
        ExpectBuiltinArgCount(L"kok(x)", args.size(), 1, line, col);
        ExpectNumericBuiltinArg(name, args[0], line, col);
        double x = ToDouble(args[0]);
        if (x < 0.0) throw RuntimeError(line, col, "MathError", "kok(x) negatif sayida kullanilamaz.");
        return Value::makeDouble(std::sqrt(x));
    }

    if (name == L"us") {
        ExpectBuiltinArgCount(L"us(a, b)", args.size(), 2, line, col);
        ExpectNumericBuiltinArg(name, args[0], line, col);
        ExpectNumericBuiltinArg(name, args[1], line, col);
        return Value::makeDouble(std::pow(ToDouble(args[0]), ToDouble(args[1])));
    }

    if (name == L"mutlak") {
        ExpectBuiltinArgCount(L"mutlak(x)", args.size(), 1, line, col);
        ExpectNumericBuiltinArg(name, args[0], line, col);

        if (args[0].type == Value::Type::INT)    return Value::makeInt(std::abs(ToInt(args[0])));
        if (args[0].type == Value::Type::FLOAT)  return Value::makeFloat((float)std::fabs(ToFloat(args[0])));
        return Value::makeDouble(std::fabs(ToDouble(args[0])));
    }

    if (name == L"taban") {
        ExpectBuiltinArgCount(L"taban(x)", args.size(), 1, line, col);
        ExpectNumericBuiltinArg(name, args[0], line, col);

        if (args[0].type == Value::Type::INT)    return Value::makeInt(ToInt(args[0]));
        if (args[0].type == Value::Type::FLOAT)  return Value::makeFloat((float)std::floor(ToFloat(args[0])));
        return Value::makeDouble(std::floor(ToDouble(args[0])));
    }

    if (name == L"tavan") {
        ExpectBuiltinArgCount(L"tavan(x)", args.size(), 1, line, col);
        ExpectNumericBuiltinArg(name, args[0], line, col);

        if (args[0].type == Value::Type::INT)    return Value::makeInt(ToInt(args[0]));
        if (args[0].type == Value::Type::FLOAT)  return Value::makeFloat((float)std::ceil(ToFloat(args[0])));
        return Value::makeDouble(std::ceil(ToDouble(args[0])));
    }

    if (name == L"yuvarla") {
        ExpectBuiltinArgCount(L"yuvarla(x)", args.size(), 1, line, col);
        ExpectNumericBuiltinArg(name, args[0], line, col);

        if (args[0].type == Value::Type::INT)    return Value::makeInt(ToInt(args[0]));
        if (args[0].type == Value::Type::FLOAT)  return Value::makeFloat((float)std::round(ToFloat(args[0])));
        return Value::makeDouble(std::round(ToDouble(args[0])));
    }

    if (name == L"min_gari") {
        ExpectBuiltinArgCount(L"min_gari(a, b)", args.size(), 2, line, col);
        ExpectNumericBuiltinArg(name, args[0], line, col);
        ExpectNumericBuiltinArg(name, args[1], line, col);

        Value::Type rt = PromoteValueType(args[0].type, args[1].type);
        double x = ToDouble(args[0]);
        double y = ToDouble(args[1]);
        return MakeNumericByType(rt, x < y ? x : y);
    }

    if (name == L"max_gari") {
        ExpectBuiltinArgCount(L"max_gari(a, b)", args.size(), 2, line, col);
        ExpectNumericBuiltinArg(name, args[0], line, col);
        ExpectNumericBuiltinArg(name, args[1], line, col);

        Value::Type rt = PromoteValueType(args[0].type, args[1].type);
        double x = ToDouble(args[0]);
        double y = ToDouble(args[1]);
        return MakeNumericByType(rt, x > y ? x : y);
    }

    if (name == L"sinus") {
        ExpectBuiltinArgCount(L"sinus(x)", args.size(), 1, line, col);
        ExpectNumericBuiltinArg(name, args[0], line, col);
        double deg = ToDouble(args[0]);
        return Value::makeDouble(std::sin(DegToRad(deg)));
    }

    if (name == L"kosinus") {
        ExpectBuiltinArgCount(L"kosinus(x)", args.size(), 1, line, col);
        ExpectNumericBuiltinArg(name, args[0], line, col);
        double deg = ToDouble(args[0]);
        return Value::makeDouble(std::cos(DegToRad(deg)));
    }

    if (name == L"tanjant") {
        ExpectBuiltinArgCount(L"tanjant(x)", args.size(), 1, line, col);
        ExpectNumericBuiltinArg(name, args[0], line, col);
        double deg = ToDouble(args[0]);
        double rad = DegToRad(deg);
        double c = std::cos(rad);
        if (std::fabs(c) < 1e-12)
            throw RuntimeError(line, col, "MathError", "tanjant(x) bu acida tanimsizdir. Ornek: 90, 270.");
        return Value::makeDouble(std::tan(rad));
    }

    if (name == L"cotanjant") {
        ExpectBuiltinArgCount(L"cotanjant(x)", args.size(), 1, line, col);
        ExpectNumericBuiltinArg(name, args[0], line, col);
        double deg = ToDouble(args[0]);
        double rad = DegToRad(deg);
        double s = std::sin(rad);
        if (std::fabs(s) < 1e-12)
            throw RuntimeError(line, col, "MathError", "cotanjant(x) bu acida tanimsizdir. Ornek: 0, 180, 360.");
        return Value::makeDouble(std::cos(rad) / s);
    }

    if (name == L"ln") {
        ExpectBuiltinArgCount(L"ln(x)", args.size(), 1, line, col);
        ExpectNumericBuiltinArg(name, args[0], line, col);
        double x = ToDouble(args[0]);
        if (x <= 0.0) throw RuntimeError(line, col, "MathError", "ln(x) icin x > 0 olmali.");
        return Value::makeDouble(std::log(x));
    }

    if (name == L"log10") {
        ExpectBuiltinArgCount(L"log10(x)", args.size(), 1, line, col);
        ExpectNumericBuiltinArg(name, args[0], line, col);
        double x = ToDouble(args[0]);
        if (x <= 0.0) throw RuntimeError(line, col, "MathError", "log10(x) icin x > 0 olmali.");
        return Value::makeDouble(std::log10(x));
    }

    if (name == L"ustel") {
        ExpectBuiltinArgCount(L"ustel(x)", args.size(), 1, line, col);
        ExpectNumericBuiltinArg(name, args[0], line, col);
        return Value::makeDouble(std::exp(ToDouble(args[0])));
    }

    throw RuntimeError(line, col, "NameError", UnknownCallMsg(name));
}

// ===================== Env =====================
struct Slot { Value v; bool isConst = false; };

struct Env {
    std::vector<std::unordered_map<std::wstring, Slot>> scopes;
    Env() { scopes.push_back({}); }

    void push() { scopes.push_back({}); }
    void pop() { if (scopes.size() > 1) scopes.pop_back(); }

    void declare(const std::wstring& name, const Value& v, bool isConst = false) {
        scopes.back()[name] = Slot{ v, isConst };
    }

    bool assign(const std::wstring& name, const Value& v, int line, int col) {
        for (int i = (int)scopes.size() - 1; i >= 0; --i) {
            auto it = scopes[i].find(name);
            if (it != scopes[i].end()) {
                if (it->second.isConst) {
                    throw RuntimeError(line, col, "ConstError", "sabit degisken degistirilemez: " + WToNarrow(name));
                }
                it->second.v = Coerce(v, it->second.v.type);
                return true;
            }
        }
        return false;
    }

    Value get(const std::wstring& name, int line, int col) const {
        for (int i = (int)scopes.size() - 1; i >= 0; --i) {
            auto it = scopes[i].find(name);
            if (it != scopes[i].end()) return it->second.v;
        }
        throw RuntimeError(line, col, "NameError", "Tanimlanmamis degisken: " + WToNarrow(name));
    }
};

// ===================== AST =====================
static void Indent(std::wstringstream& ss, int n) { for (int i = 0; i < n; i++) ss << L"  "; }

struct Expr {
    int line = 1;
    int col = 1;
    virtual ~Expr() = default;
    virtual Value eval(Env& env) const = 0;
    virtual void dump(std::wstringstream& ss, int indent) const = 0;
};
using ExprPtr = std::unique_ptr<Expr>;

struct Stmt {
    int line = 1;
    int col = 1;
    virtual ~Stmt() = default;
    virtual void dump(std::wstringstream& ss, int indent) const = 0;
};
using StmtPtr = std::unique_ptr<Stmt>;

struct IntExpr : Expr {
    int v; explicit IntExpr(int x) : v(x) {}
    Value eval(Env&) const override { return Value::makeInt(v); }
    void dump(std::wstringstream& ss, int indent) const override { Indent(ss, indent); ss << L"Int(" << v << L")\n"; }
};
struct DoubleExpr : Expr {
    double v; explicit DoubleExpr(double x) : v(x) {}
    Value eval(Env&) const override { return Value::makeDouble(v); }
    void dump(std::wstringstream& ss, int indent) const override { Indent(ss, indent); ss << L"Double(" << v << L")\n"; }
};
struct StrExpr : Expr {
    std::wstring v; explicit StrExpr(std::wstring s) : v(std::move(s)) {}
    Value eval(Env&) const override { return Value::makeString(v); }
    void dump(std::wstringstream& ss, int indent) const override { Indent(ss, indent); ss << L"Str(\"" << v << L"\")\n"; }
};
struct VarExpr : Expr {
    std::wstring name; explicit VarExpr(std::wstring n) : name(std::move(n)) {}
    Value eval(Env& env) const override { return env.get(name, line, col); }
    void dump(std::wstringstream& ss, int indent) const override { Indent(ss, indent); ss << L"Var(" << name << L")\n"; }
};

struct CallExpr : Expr {
    std::wstring name;
    std::vector<ExprPtr> args;
    CallExpr(std::wstring n, std::vector<ExprPtr> a) : name(std::move(n)), args(std::move(a)) {}

    static void TrimWS(std::wstring& t) {
        while (!t.empty() && iswspace(t.front())) t.erase(t.begin());
        while (!t.empty() && iswspace(t.back()))  t.pop_back();
    }

    Value eval(Env& env) const override {
        std::vector<Value> vals;
        vals.reserve(args.size());
        for (auto& a : args) vals.push_back(a->eval(env));
        return EvalBuiltinCall(name, vals, line, col);
    }

    void dump(std::wstringstream& ss, int indent) const override {
        Indent(ss, indent); ss << L"Call(" << name << L")\n";
        for (auto& a : args) a->dump(ss, indent + 1);
    }
};

struct BlockStmt : Stmt {
    std::vector<StmtPtr> stmts;
    void dump(std::wstringstream& ss, int indent) const override {
        Indent(ss, indent); ss << L"Block\n";
        for (auto& s : stmts) s->dump(ss, indent + 1);
    }
};

struct Program {
    std::unique_ptr<BlockStmt> main;
    void dump(std::wstringstream& ss) const {
        ss << L"Program\n";
        ss << L"Main:\n";
        main->dump(ss, 1);
    }
};

enum class BinOp {
    Add, Sub, Mul, Div, Mod,
    Eq, Ne, Lt, Gt, Le, Ge,
    AndAnd, OrOr,
};

struct BinExpr : Expr {
    BinOp op; ExprPtr a, b;
    BinExpr(BinOp o, ExprPtr l, ExprPtr r) : op(o), a(std::move(l)), b(std::move(r)) {}

    Value eval(Env& env) const override {
        if (op == BinOp::AndAnd) {
            Value A = a->eval(env);
            if (!IsTrue(A)) return Value::makeInt(0);
            Value B = b->eval(env);
            return Value::makeInt(IsTrue(B) ? 1 : 0);
        }
        if (op == BinOp::OrOr) {
            Value A = a->eval(env);
            if (IsTrue(A)) return Value::makeInt(1);
            Value B = b->eval(env);
            return Value::makeInt(IsTrue(B) ? 1 : 0);
        }

        Value A = a->eval(env);
        Value B = b->eval(env);

        if (op == BinOp::Eq || op == BinOp::Ne || op == BinOp::Lt || op == BinOp::Gt || op == BinOp::Le || op == BinOp::Ge) {
            bool res = false;
            if (op == BinOp::Eq) res = ValueEquals(A, B);
            else if (op == BinOp::Ne) res = !ValueEquals(A, B);
            else {
                if (A.type == Value::Type::STRING || B.type == Value::Type::STRING) {
                    std::wstring x = ValueToString(A), y = ValueToString(B);
                    if (op == BinOp::Lt) res = (x < y);
                    if (op == BinOp::Gt) res = (x > y);
                    if (op == BinOp::Le) res = (x <= y);
                    if (op == BinOp::Ge) res = (x >= y);
                }
                else {
                    double x = ToDouble(A), y = ToDouble(B);
                    if (op == BinOp::Lt) res = (x < y);
                    if (op == BinOp::Gt) res = (x > y);
                    if (op == BinOp::Le) res = (x <= y);
                    if (op == BinOp::Ge) res = (x >= y);
                }
            }
            return Value::makeInt(res ? 1 : 0);
        }

        if (op == BinOp::Add && (A.type == Value::Type::STRING || B.type == Value::Type::STRING))
            return Value::makeString(ValueToString(A) + ValueToString(B));

        if (op == BinOp::Mod) {
            if (!IsNumeric(A.type) || !IsNumeric(B.type))
                throw RuntimeError(line, col, "TypeError", "'%' sadece sayilarda kullanilir");
        }

        if (A.type == Value::Type::DOUBLE || B.type == Value::Type::DOUBLE) {
            double x = ToDouble(A), y = ToDouble(B);
            if (op == BinOp::Add) return Value::makeDouble(x + y);
            if (op == BinOp::Sub) return Value::makeDouble(x - y);
            if (op == BinOp::Mul) return Value::makeDouble(x * y);
            if (op == BinOp::Div) {
                if (y == 0.0) throw RuntimeError(line, col, "ZeroDivision", ZeroDivMsg("/", A, B));
                return Value::makeDouble(x / y);
            }
            if (op == BinOp::Mod) {
                if (y == 0.0) throw RuntimeError(line, col, "ZeroDivision", ZeroDivMsg("%", A, B));
                return Value::makeDouble(std::fmod(x, y));
            }
            return Value::makeDouble(0.0);
        }
        if (A.type == Value::Type::FLOAT || B.type == Value::Type::FLOAT) {
            float x = ToFloat(A), y = ToFloat(B);
            if (op == BinOp::Add) return Value::makeFloat(x + y);
            if (op == BinOp::Sub) return Value::makeFloat(x - y);
            if (op == BinOp::Mul) return Value::makeFloat(x * y);
            if (op == BinOp::Div) {
                if (y == 0.0f) throw RuntimeError(line, col, "ZeroDivision", ZeroDivMsg("/", A, B));
                return Value::makeFloat(x / y);
            }
            if (op == BinOp::Mod) {
                if (y == 0.0f) throw RuntimeError(line, col, "ZeroDivision", ZeroDivMsg("%", A, B));
                return Value::makeFloat(std::fmod(x, y));
            }
            return Value::makeFloat(0.0f);
        }

        int x = ToInt(A), y = ToInt(B);
        if (op == BinOp::Add) return Value::makeInt(x + y);
        if (op == BinOp::Sub) return Value::makeInt(x - y);
        if (op == BinOp::Mul) return Value::makeInt(x * y);
        if (op == BinOp::Div) {
            if (y == 0) throw RuntimeError(line, col, "ZeroDivision", ZeroDivMsg("/", A, B));
            return Value::makeInt(x / y);
        }
        if (op == BinOp::Mod) {
            if (y == 0) throw RuntimeError(line, col, "ZeroDivision", ZeroDivMsg("%", A, B));
            return Value::makeInt(x % y);
        }
        return Value::makeInt(0);
    }

    void dump(std::wstringstream& ss, int indent) const override {
        Indent(ss, indent); ss << L"Bin(" << (int)op << L")\n";
        a->dump(ss, indent + 1);
        b->dump(ss, indent + 1);
    }
};

// ===================== Statements =====================
static Value::Type TokTypeToValueType(TokType t) {
    switch (t) {
    case TokType::KwInt: return Value::Type::INT;
    case TokType::KwFloat: return Value::Type::FLOAT;
    case TokType::KwDouble: return Value::Type::DOUBLE;
    case TokType::KwString: return Value::Type::STRING;
    default: return Value::Type::INT;
    }
}

struct VarDeclStmt : Stmt {
    std::wstring name;
    Value::Type declaredType = Value::Type::INT;
    ExprPtr init;
    bool isConst = false;

    VarDeclStmt(std::wstring n, Value::Type t, ExprPtr e, bool c)
        : name(std::move(n)), declaredType(t), init(std::move(e)), isConst(c) {
    }

    void dump(std::wstringstream& ss, int indent) const override {
        Indent(ss, indent); ss << L"VarDecl(" << name << L")" << (isConst ? L" [sabit]" : L"") << L"\n";
        if (init) init->dump(ss, indent + 1);
    }
};

struct AssignStmt : Stmt {
    std::wstring name; ExprPtr value;
    AssignStmt(std::wstring n, ExprPtr v) : name(std::move(n)), value(std::move(v)) {}
    void dump(std::wstringstream& ss, int indent) const override {
        Indent(ss, indent); ss << L"Assign(" << name << L")\n";
        value->dump(ss, indent + 1);
    }
};

struct PrintStmt : Stmt {
    ExprPtr expr;
    explicit PrintStmt(ExprPtr e) : expr(std::move(e)) {}
    void dump(std::wstringstream& ss, int indent) const override {
        Indent(ss, indent); ss << L"Print\n";
        expr->dump(ss, indent + 1);
    }
};

struct IfStmt : Stmt {
    ExprPtr cond; StmtPtr thenS; StmtPtr elseS;
    IfStmt(ExprPtr c, StmtPtr t, StmtPtr e) : cond(std::move(c)), thenS(std::move(t)), elseS(std::move(e)) {}
    void dump(std::wstringstream& ss, int indent) const override {
        Indent(ss, indent); ss << L"If\n";
        Indent(ss, indent + 1); ss << L"Cond:\n"; cond->dump(ss, indent + 2);
        Indent(ss, indent + 1); ss << L"Then:\n"; thenS->dump(ss, indent + 2);
        if (elseS) { Indent(ss, indent + 1); ss << L"Else:\n"; elseS->dump(ss, indent + 2); }
    }
};

struct WhileStmt : Stmt {
    ExprPtr cond; StmtPtr body;
    WhileStmt(ExprPtr c, StmtPtr b) : cond(std::move(c)), body(std::move(b)) {}
    void dump(std::wstringstream& ss, int indent) const override {
        Indent(ss, indent); ss << L"While\n";
        Indent(ss, indent + 1); ss << L"Cond:\n"; cond->dump(ss, indent + 2);
        Indent(ss, indent + 1); ss << L"Body:\n"; body->dump(ss, indent + 2);
    }
};

struct DoWhileStmt : Stmt {
    StmtPtr body; ExprPtr cond;
    DoWhileStmt(StmtPtr b, ExprPtr c) : body(std::move(b)), cond(std::move(c)) {}
    void dump(std::wstringstream& ss, int indent) const override {
        Indent(ss, indent); ss << L"DoWhile\n";
        Indent(ss, indent + 1); ss << L"Body:\n"; body->dump(ss, indent + 2);
        Indent(ss, indent + 1); ss << L"Cond:\n"; cond->dump(ss, indent + 2);
    }
};

struct ForStmt : Stmt {
    StmtPtr init; ExprPtr cond; StmtPtr step; StmtPtr body;
    ForStmt(StmtPtr i, ExprPtr c, StmtPtr s, StmtPtr b)
        : init(std::move(i)), cond(std::move(c)), step(std::move(s)), body(std::move(b)) {
    }
    void dump(std::wstringstream& ss, int indent) const override {
        Indent(ss, indent); ss << L"For\n";
        if (init) { Indent(ss, indent + 1); ss << L"Init:\n"; init->dump(ss, indent + 2); }
        if (cond) { Indent(ss, indent + 1); ss << L"Cond:\n"; cond->dump(ss, indent + 2); }
        if (step) { Indent(ss, indent + 1); ss << L"Step:\n"; step->dump(ss, indent + 2); }
        Indent(ss, indent + 1); ss << L"Body:\n"; body->dump(ss, indent + 2);
    }
};

struct ReturnStmt : Stmt {
    ExprPtr value;
    explicit ReturnStmt(ExprPtr v) : value(std::move(v)) {}
    void dump(std::wstringstream& ss, int indent) const override {
        Indent(ss, indent); ss << L"Return\n";
        if (value) value->dump(ss, indent + 1);
    }
};

// ===================== Parser helpers =====================
static std::wstring TokToW(const Token& t) {
    std::wstringstream ss;
    ss << TokName(t.type);
    if (!t.lexeme.empty()) ss << L" {" << t.lexeme << L"}";
    return ss.str();
}
static std::wstring W(const wchar_t* s) { return std::wstring(s ? s : L""); }

// ===================== Parser =====================
struct Parser {
    std::vector<Token> toks; size_t i = 0;
    explicit Parser(std::vector<Token> t) : toks(std::move(t)) {}

    const Token& peek() const { static Token dummy{ TokType::End, L"", 1, 1 }; return (i < toks.size()) ? toks[i] : dummy; }
    const Token& prev() const { static Token dummy{ TokType::End, L"", 1, 1 }; return (i == 0) ? dummy : toks[i - 1]; }
    bool isAtEnd() const { return peek().type == TokType::End; }
    bool check(TokType t) const { return peek().type == t; }
    bool match(TokType t) { if (check(t)) { i++; return true; } return false; }

    const Token& consume(TokType t, const char* msg) {
        if (check(t)) return toks[i++];
        throw ParseError(peek().line, peek().col, msg, { W(TokName(t)) }, TokToW(peek()));
    }

    static bool IsCmpOp(TokType t) { return t == TokType::Lt || t == TokType::Gt || t == TokType::Le || t == TokType::Ge; }
    static std::wstring CmpOpsExpectedList() { return L"<, >, <=, >="; }

    ExprPtr parseExpr() { return parseLogicOr(); }

    ExprPtr parseLogicOr() {
        ExprPtr e = parseLogicAnd();
        while (match(TokType::OrOr)) {
            int opLine = prev().line, opCol = prev().col;
            auto r = parseLogicAnd();
            auto n = std::make_unique<BinExpr>(BinOp::OrOr, std::move(e), std::move(r));
            n->line = opLine; n->col = opCol;
            e = std::move(n);
        }
        return e;
    }
    ExprPtr parseLogicAnd() {
        ExprPtr e = parseEquality();
        while (match(TokType::AndAnd)) {
            int opLine = prev().line, opCol = prev().col;
            auto r = parseEquality();
            auto n = std::make_unique<BinExpr>(BinOp::AndAnd, std::move(e), std::move(r));
            n->line = opLine; n->col = opCol;
            e = std::move(n);
        }
        return e;
    }

    ExprPtr parseEquality() {
        ExprPtr e = parseComparison();
        while (true) {
            if (match(TokType::Eq)) {
                int opLine = prev().line, opCol = prev().col;
                auto r = parseComparison();
                auto n = std::make_unique<BinExpr>(BinOp::Eq, std::move(e), std::move(r));
                n->line = opLine; n->col = opCol; e = std::move(n);
                continue;
            }
            if (match(TokType::Ne)) {
                int opLine = prev().line, opCol = prev().col;
                auto r = parseComparison();
                auto n = std::make_unique<BinExpr>(BinOp::Ne, std::move(e), std::move(r));
                n->line = opLine; n->col = opCol; e = std::move(n);
                continue;
            }
            break;
        }
        return e;
    }

    ExprPtr parseComparison() {
        ExprPtr e = parseTerm();

        if (IsCmpOp(peek().type)) {
            TokType opTok = peek().type;
            int opLine = peek().line, opCol = peek().col;
            i++;
            auto r = parseTerm();

            BinOp bop = BinOp::Lt;
            if (opTok == TokType::Lt) bop = BinOp::Lt;
            else if (opTok == TokType::Gt) bop = BinOp::Gt;
            else if (opTok == TokType::Le) bop = BinOp::Le;
            else if (opTok == TokType::Ge) bop = BinOp::Ge;

            auto n = std::make_unique<BinExpr>(bop, std::move(e), std::move(r));
            n->line = opLine; n->col = opCol;
            e = std::move(n);

            if (IsCmpOp(peek().type)) {
                throw ParseError(
                    peek().line, peek().col,
                    "Zincirli karsilastirma yasak: 'a < b < c' yazamazsin. Iki karsilastirmayi ayri yaz.",
                    { CmpOpsExpectedList() },
                    TokToW(peek())
                );
            }
        }

        return e;
    }

    ExprPtr parseTerm() {
        ExprPtr e = parseFactor();
        while (true) {
            if (match(TokType::Plus)) {
                int opLine = prev().line, opCol = prev().col;
                auto r = parseFactor();
                auto n = std::make_unique<BinExpr>(BinOp::Add, std::move(e), std::move(r));
                n->line = opLine; n->col = opCol; e = std::move(n); continue;
            }
            if (match(TokType::Minus)) {
                int opLine = prev().line, opCol = prev().col;
                auto r = parseFactor();
                auto n = std::make_unique<BinExpr>(BinOp::Sub, std::move(e), std::move(r));
                n->line = opLine; n->col = opCol; e = std::move(n); continue;
            }
            break;
        }
        return e;
    }
    ExprPtr parseFactor() {
        ExprPtr e = parseUnary();
        while (true) {
            if (match(TokType::Star)) {
                int opLine = prev().line, opCol = prev().col;
                auto r = parseUnary();
                auto n = std::make_unique<BinExpr>(BinOp::Mul, std::move(e), std::move(r));
                n->line = opLine; n->col = opCol; e = std::move(n); continue;
            }
            if (match(TokType::Slash)) {
                int opLine = prev().line, opCol = prev().col;
                auto r = parseUnary();
                auto n = std::make_unique<BinExpr>(BinOp::Div, std::move(e), std::move(r));
                n->line = opLine; n->col = opCol; e = std::move(n); continue;
            }
            if (match(TokType::Percent)) {
                int opLine = prev().line, opCol = prev().col;
                auto r = parseUnary();
                auto n = std::make_unique<BinExpr>(BinOp::Mod, std::move(e), std::move(r));
                n->line = opLine; n->col = opCol; e = std::move(n); continue;
            }
            break;
        }
        return e;
    }

    ExprPtr parseUnary() {
        if (match(TokType::Minus)) {
            int ln = prev().line, cl = prev().col;
            auto r = parseUnary();
            auto zero = std::make_unique<IntExpr>(0); zero->line = ln; zero->col = cl;
            auto n = std::make_unique<BinExpr>(BinOp::Sub, std::move(zero), std::move(r));
            n->line = ln; n->col = cl;
            return n;
        }
        return parsePrimary();
    }

    ExprPtr parsePrimary() {
        if (match(TokType::Number)) {
            int ln = prev().line, cl = prev().col;
            std::wstring lex = prev().lexeme;
            if (lex.find(L'.') != std::wstring::npos) {
                double d = 0.0; try { d = std::stod(lex); }
                catch (...) {}
                auto n = std::make_unique<DoubleExpr>(d);
                n->line = ln; n->col = cl;
                return n;
            }
            else {
                int v = 0; try { v = std::stoi(lex); }
                catch (...) {}
                auto n = std::make_unique<IntExpr>(v);
                n->line = ln; n->col = cl;
                return n;
            }
        }

        if (match(TokType::String)) {
            auto n = std::make_unique<StrExpr>(prev().lexeme);
            n->line = prev().line; n->col = prev().col;
            return n;
        }

        if (match(TokType::Identifier)) {
            int ln = prev().line, cl = prev().col;
            std::wstring name = prev().lexeme;

            if (match(TokType::LParen)) {
                std::vector<ExprPtr> args;
                if (!check(TokType::RParen)) {
                    while (true) {
                        args.push_back(parseExpr());
                        if (match(TokType::Comma)) continue;
                        break;
                    }
                }
                consume(TokType::RParen, "')' bekleniyordu");

                auto call = std::make_unique<CallExpr>(std::move(name), std::move(args));
                call->line = ln; call->col = cl;
                return call;
            }

            auto var = std::make_unique<VarExpr>(std::move(name));
            var->line = ln; var->col = cl;
            return var;
        }

        if (match(TokType::LParen)) {
            auto e = parseExpr();
            consume(TokType::RParen, "')' bekleniyordu");
            return e;
        }

        throw ParseError(peek().line, peek().col, "Ifade bekleniyordu", {}, TokToW(peek()));
    }

    StmtPtr parseBlockAfterLBrace() {
        int ln = prev().line, cl = prev().col;
        auto b = std::make_unique<BlockStmt>();
        b->line = ln; b->col = cl;
        while (!check(TokType::RBrace) && !isAtEnd()) b->stmts.push_back(parseStmt());
        consume(TokType::RBrace, "'}' bekleniyordu");
        return b;
    }

    StmtPtr parsePrint() {
        int ln = prev().line, cl = prev().col;
        consume(TokType::LParen, "'(' bekleniyordu");
        auto e = parseExpr();
        consume(TokType::RParen, "')' bekleniyordu");
        consume(TokType::Semicolon, "';' bekleniyordu");
        auto n = std::make_unique<PrintStmt>(std::move(e));
        n->line = ln; n->col = cl;
        return n;
    }

    StmtPtr parseReturn() {
        int ln = prev().line, cl = prev().col;
        ExprPtr v = nullptr;
        if (!check(TokType::Semicolon)) v = parseExpr();
        consume(TokType::Semicolon, "';' bekleniyordu");
        auto n = std::make_unique<ReturnStmt>(std::move(v));
        n->line = ln; n->col = cl;
        return n;
    }

    StmtPtr parseIf() {
        int ln = prev().line, cl = prev().col;
        consume(TokType::LParen, "'(' bekleniyordu");
        auto cond = parseExpr();
        consume(TokType::RParen, "')' bekleniyordu");
        auto thenS = parseStmt();

        StmtPtr elseS = nullptr;
        if (match(TokType::KwElse)) elseS = parseStmt();

        auto n = std::make_unique<IfStmt>(std::move(cond), std::move(thenS), std::move(elseS));
        n->line = ln; n->col = cl;
        return n;
    }

    StmtPtr parseWhile() {
        int ln = prev().line, cl = prev().col;
        consume(TokType::LParen, "'(' bekleniyordu");
        auto cond = parseExpr();
        consume(TokType::RParen, "')' bekleniyordu");
        auto body = parseStmt();
        auto n = std::make_unique<WhileStmt>(std::move(cond), std::move(body));
        n->line = ln; n->col = cl;
        return n;
    }

    StmtPtr parseDoWhile() {
        int ln = prev().line, cl = prev().col;
        auto body = parseStmt();
        consume(TokType::KwWhile, "'oldukca' bekleniyordu");
        consume(TokType::LParen, "'(' bekleniyordu");
        auto cond = parseExpr();
        consume(TokType::RParen, "')' bekleniyordu");
        consume(TokType::Semicolon, "';' bekleniyordu");
        auto n = std::make_unique<DoWhileStmt>(std::move(body), std::move(cond));
        n->line = ln; n->col = cl;
        return n;
    }

    StmtPtr parseVarDecl() {
        bool isConst = false;
        int ln = peek().line, cl = peek().col;

        if (match(TokType::KwConst)) {
            isConst = true;
            ln = peek().line;
            cl = peek().col;
        }

        TokType typeTok = peek().type;
        if (!(typeTok == TokType::KwInt || typeTok == TokType::KwFloat || typeTok == TokType::KwDouble || typeTok == TokType::KwString)) {
            throw ParseError(peek().line, peek().col, "Tip bekleniyordu (tamsayi/laf/...)", { L"(tip)" }, TokToW(peek()));
        }
        i++;
        Value::Type declT = TokTypeToValueType(typeTok);

        Token nameTok = consume(TokType::Identifier, "Degisken ismi bekleniyordu");

        ExprPtr init = nullptr;
        if (match(TokType::Assign)) init = parseExpr();
        else if (isConst) {
            throw ParseError(peek().line, peek().col, "sabit degisken ilk deger almak zorunda", { L"=" }, TokToW(peek()));
        }

        consume(TokType::Semicolon, "';' bekleniyordu");
        auto n = std::make_unique<VarDeclStmt>(nameTok.lexeme, declT, std::move(init), isConst);
        n->line = ln; n->col = cl;
        return n;
    }

    StmtPtr parseSimpleAssignNoSemicolon() {
        int ln = peek().line, cl = peek().col;
        Token nameTok = consume(TokType::Identifier, "degisken adi bekleniyordu");

        if (match(TokType::Assign)) {
            auto val = parseExpr();
            auto n = std::make_unique<AssignStmt>(nameTok.lexeme, std::move(val));
            n->line = ln; n->col = cl;
            return n;
        }
        if (match(TokType::PlusAssign)) {
            int opLine = prev().line, opCol = prev().col;
            auto rhs = parseExpr();
            auto lhs = std::make_unique<VarExpr>(nameTok.lexeme); lhs->line = ln; lhs->col = cl;
            auto sum = std::make_unique<BinExpr>(BinOp::Add, std::move(lhs), std::move(rhs));
            sum->line = opLine; sum->col = opCol;
            auto n = std::make_unique<AssignStmt>(nameTok.lexeme, std::move(sum));
            n->line = ln; n->col = cl;
            return n;
        }
        if (match(TokType::MinusAssign)) {
            int opLine = prev().line, opCol = prev().col;
            auto rhs = parseExpr();
            auto lhs = std::make_unique<VarExpr>(nameTok.lexeme); lhs->line = ln; lhs->col = cl;
            auto sub = std::make_unique<BinExpr>(BinOp::Sub, std::move(lhs), std::move(rhs));
            sub->line = opLine; sub->col = opCol;
            auto n = std::make_unique<AssignStmt>(nameTok.lexeme, std::move(sub));
            n->line = ln; n->col = cl;
            return n;
        }
        throw ParseError(peek().line, peek().col, "Atama operatoru bekleniyordu", { L"= / += / -=" }, TokToW(peek()));
    }

    StmtPtr parseAssign() {
        int ln = peek().line, cl = peek().col;
        Token nameTok = consume(TokType::Identifier, "Degisken ismi bekleniyordu");

        if (match(TokType::Assign)) {
            auto v = parseExpr();
            consume(TokType::Semicolon, "';' bekleniyordu");
            auto n = std::make_unique<AssignStmt>(nameTok.lexeme, std::move(v));
            n->line = ln; n->col = cl;
            return n;
        }

        if (match(TokType::PlusAssign)) {
            int opLine = prev().line, opCol = prev().col;
            auto rhs = parseExpr();
            consume(TokType::Semicolon, "';' bekleniyordu");
            auto lhs = std::make_unique<VarExpr>(nameTok.lexeme); lhs->line = ln; lhs->col = cl;
            auto sum = std::make_unique<BinExpr>(BinOp::Add, std::move(lhs), std::move(rhs));
            sum->line = opLine; sum->col = opCol;
            auto n = std::make_unique<AssignStmt>(nameTok.lexeme, std::move(sum));
            n->line = ln; n->col = cl;
            return n;
        }

        if (match(TokType::MinusAssign)) {
            int opLine = prev().line, opCol = prev().col;
            auto rhs = parseExpr();
            consume(TokType::Semicolon, "';' bekleniyordu");
            auto lhs = std::make_unique<VarExpr>(nameTok.lexeme); lhs->line = ln; lhs->col = cl;
            auto sub = std::make_unique<BinExpr>(BinOp::Sub, std::move(lhs), std::move(rhs));
            sub->line = opLine; sub->col = opCol;
            auto n = std::make_unique<AssignStmt>(nameTok.lexeme, std::move(sub));
            n->line = ln; n->col = cl;
            return n;
        }

        throw ParseError(peek().line, peek().col, "Atama operatoru bekleniyordu", { L"= / += / -=" }, TokToW(peek()));
    }

    StmtPtr parseFor() {
        int ln = prev().line, cl = prev().col;
        consume(TokType::LParen, "'(' bekleniyordu");

        StmtPtr init = nullptr;
        if (check(TokType::KwConst) || check(TokType::KwInt) || check(TokType::KwFloat) || check(TokType::KwDouble) || check(TokType::KwString)) {
            init = parseVarDecl();
        }
        else if (check(TokType::Identifier)) {
            init = parseAssign();
        }
        else {
            consume(TokType::Semicolon, "';' bekleniyordu");
        }

        ExprPtr cond = nullptr;
        if (!check(TokType::Semicolon)) cond = parseExpr();
        consume(TokType::Semicolon, "';' bekleniyordu");

        StmtPtr step = nullptr;
        if (!check(TokType::RParen)) step = parseSimpleAssignNoSemicolon();
        consume(TokType::RParen, "')' bekleniyordu");

        auto body = parseStmt();
        if (!cond) { cond = std::make_unique<IntExpr>(1); cond->line = ln; cond->col = cl; }

        auto n = std::make_unique<ForStmt>(std::move(init), std::move(cond), std::move(step), std::move(body));
        n->line = ln; n->col = cl;
        return n;
    }

    StmtPtr parseStmt() {
        if (match(TokType::KwReturn))   return parseReturn();
        if (match(TokType::KwDoWhile))  return parseDoWhile();
        if (match(TokType::LBrace))     return parseBlockAfterLBrace();
        if (match(TokType::KwIf))       return parseIf();
        if (match(TokType::KwWhile))    return parseWhile();
        if (match(TokType::KwFor))      return parseFor();
        if (match(TokType::KwPrint))    return parsePrint();

        if (check(TokType::KwConst) || check(TokType::KwInt) || check(TokType::KwFloat) || check(TokType::KwDouble) || check(TokType::KwString))
            return parseVarDecl();

        if (check(TokType::Identifier)) return parseAssign();

        throw ParseError(peek().line, peek().col, "Komut/statement bekleniyordu", {}, TokToW(peek()));
    }

    std::unique_ptr<Program> parseProgram() {
        auto p = std::make_unique<Program>();

        consume(TokType::KwMain, "Program basla_gari ile baslamali");
        consume(TokType::LParen, "'(' bekleniyordu");
        consume(TokType::RParen, "')' bekleniyordu");
        consume(TokType::LBrace, "'{' bekleniyordu");

        auto body = std::make_unique<BlockStmt>();
        body->line = prev().line; body->col = prev().col;
        while (!check(TokType::RBrace) && !isAtEnd())
            body->stmts.push_back(parseStmt());
        consume(TokType::RBrace, "'}' bekleniyordu");

        p->main = std::move(body);
        return p;
    }
};

static std::wstring AstToText(const Program& p) { std::wstringstream ss; p.dump(ss); return ss.str(); }

// ===================== STATIC TYPECHECK =====================
enum class SType { INT, FLOAT, DOUBLE, STRING, ANY, INVALID };

static const wchar_t* STypeName(SType t) {
    switch (t) {
    case SType::INT: return L"tamsayi";
    case SType::FLOAT: return L"hassas_sayi";
    case SType::DOUBLE: return L"cift_hassas_sayi";
    case SType::STRING: return L"laf";
    case SType::ANY: return L"any";
    default: return L"invalid";
    }
}

struct TypeError : std::exception {
    int line = 1;
    int col = 1;
    std::wstring msg;
    TypeError(int ln, int cl, std::wstring m) : line(ln), col(cl), msg(std::move(m)) {}
    const char* what() const noexcept override { return "TypeError"; }
};

struct TypeSlot { SType t = SType::INVALID; bool isConst = false; };

struct TypeEnv {
    std::vector<std::unordered_map<std::wstring, TypeSlot>> scopes;
    TypeEnv() { scopes.push_back({}); }
    void push() { scopes.push_back({}); }
    void pop() { if (scopes.size() > 1) scopes.pop_back(); }

    void declare(const std::wstring& n, SType t, bool isConst = false) { scopes.back()[n] = TypeSlot{ t, isConst }; }

    TypeSlot getSlot(const std::wstring& n) const {
        for (int i = (int)scopes.size() - 1; i >= 0; --i) {
            auto it = scopes[i].find(n);
            if (it != scopes[i].end()) return it->second;
        }
        return TypeSlot{ SType::INVALID, false };
    }

    bool assignable(SType to, SType from) const {
        if (to == SType::ANY || from == SType::ANY) return true;
        if (to == from) return true;
        if (to == SType::DOUBLE && (from == SType::FLOAT || from == SType::INT)) return true;
        if (to == SType::FLOAT && (from == SType::INT)) return true;
        if (from == SType::STRING && (to == SType::INT || to == SType::FLOAT || to == SType::DOUBLE)) return true;
        return false;
    }
};

static SType VTypeToSType(Value::Type t) {
    switch (t) {
    case Value::Type::INT: return SType::INT;
    case Value::Type::FLOAT: return SType::FLOAT;
    case Value::Type::DOUBLE: return SType::DOUBLE;
    case Value::Type::STRING: return SType::STRING;
    default: return SType::ANY;
    }
}
static bool IsNumericSType(SType t) { return t == SType::INT || t == SType::FLOAT || t == SType::DOUBLE; }
static SType PromoteNumeric(SType a, SType b) {
    if (a == SType::DOUBLE || b == SType::DOUBLE) return SType::DOUBLE;
    if (a == SType::FLOAT || b == SType::FLOAT) return SType::FLOAT;
    if (a == SType::INT && b == SType::INT) return SType::INT;
    return SType::INVALID;
}
static bool CondAllowed(SType t) { return t == SType::STRING || IsNumericSType(t) || t == SType::ANY; }

static SType TypeOfExpr(const Expr* e, TypeEnv& tenv) {
    if (!e) return SType::ANY;

    if (dynamic_cast<const IntExpr*>(e)) return SType::INT;
    if (dynamic_cast<const DoubleExpr*>(e)) return SType::DOUBLE;
    if (dynamic_cast<const StrExpr*>(e)) return SType::STRING;

    if (auto v = dynamic_cast<const VarExpr*>(e)) {
        TypeSlot ts = tenv.getSlot(v->name);
        if (ts.t == SType::INVALID)
            throw TypeError(e->line, e->col, L"Tanimlanmamis degisken: " + v->name);
        return ts.t;
    }

    if (auto c = dynamic_cast<const CallExpr*>(e)) {
        auto ensureCount = [&](size_t want, const wchar_t* sig) {
            if (c->args.size() != want)
                throw TypeError(e->line, e->col, std::wstring(sig) + L" dogru arguman sayisi ile cagrilmali.");
            };

        auto requireNumeric = [&](size_t idx, const wchar_t* fname) -> SType {
            SType t = TypeOfExpr(c->args[idx].get(), tenv);
            if (!IsNumericSType(t))
                throw TypeError(e->line, e->col, std::wstring(fname) + L" sadece sayi alir.");
            return t;
            };

        if (c->name == L"oku_gari") {
            ensureCount(0, L"oku_gari()");
            return SType::STRING;
        }
        if (c->name == L"oku_tamsayi") {
            ensureCount(0, L"oku_tamsayi()");
            return SType::INT;
        }
        if (c->name == L"oku_hassas_sayi") {
            ensureCount(0, L"oku_hassas_sayi()");
            return SType::FLOAT;
        }
        if (c->name == L"pi_sayisi") {
            ensureCount(0, L"pi_sayisi()");
            return SType::DOUBLE;
        }
        if (c->name == L"e_sayisi") {
            ensureCount(0, L"e_sayisi()");
            return SType::DOUBLE;
        }

        if (c->name == L"kok") {
            ensureCount(1, L"kok(x)");
            (void)requireNumeric(0, L"kok");
            return SType::DOUBLE;
        }
        if (c->name == L"us") {
            ensureCount(2, L"us(a, b)");
            (void)requireNumeric(0, L"us");
            (void)requireNumeric(1, L"us");
            return SType::DOUBLE;
        }
        if (c->name == L"mutlak") {
            ensureCount(1, L"mutlak(x)");
            return requireNumeric(0, L"mutlak");
        }
        if (c->name == L"taban") {
            ensureCount(1, L"taban(x)");
            return requireNumeric(0, L"taban");
        }
        if (c->name == L"tavan") {
            ensureCount(1, L"tavan(x)");
            return requireNumeric(0, L"tavan");
        }
        if (c->name == L"yuvarla") {
            ensureCount(1, L"yuvarla(x)");
            return requireNumeric(0, L"yuvarla");
        }
        if (c->name == L"min_gari") {
            ensureCount(2, L"min_gari(a, b)");
            SType a = requireNumeric(0, L"min_gari");
            SType b = requireNumeric(1, L"min_gari");
            return PromoteNumeric(a, b);
        }
        if (c->name == L"max_gari") {
            ensureCount(2, L"max_gari(a, b)");
            SType a = requireNumeric(0, L"max_gari");
            SType b = requireNumeric(1, L"max_gari");
            return PromoteNumeric(a, b);
        }
        if (c->name == L"sinus") {
            ensureCount(1, L"sinus(x)");
            (void)requireNumeric(0, L"sinus");
            return SType::DOUBLE;
        }
        if (c->name == L"kosinus") {
            ensureCount(1, L"kosinus(x)");
            (void)requireNumeric(0, L"kosinus");
            return SType::DOUBLE;
        }
        if (c->name == L"tanjant") {
            ensureCount(1, L"tanjant(x)");
            (void)requireNumeric(0, L"tanjant");
            return SType::DOUBLE;
        }
        if (c->name == L"cotanjant") {
            ensureCount(1, L"cotanjant(x)");
            (void)requireNumeric(0, L"cotanjant");
            return SType::DOUBLE;
        }
        if (c->name == L"ln") {
            ensureCount(1, L"ln(x)");
            (void)requireNumeric(0, L"ln");
            return SType::DOUBLE;
        }
        if (c->name == L"log10") {
            ensureCount(1, L"log10(x)");
            (void)requireNumeric(0, L"log10");
            return SType::DOUBLE;
        }
        if (c->name == L"ustel") {
            ensureCount(1, L"ustel(x)");
            (void)requireNumeric(0, L"ustel");
            return SType::DOUBLE;
        }

        throw TypeError(e->line, e->col, UnknownCallTypeMsg(c->name));
    }

    if (auto b = dynamic_cast<const BinExpr*>(e)) {
        if (b->op == BinOp::AndAnd || b->op == BinOp::OrOr) {
            SType A = TypeOfExpr(b->a.get(), tenv);
            SType B = TypeOfExpr(b->b.get(), tenv);
            if (!CondAllowed(A) || !CondAllowed(B))
                throw TypeError(e->line, e->col, L"&&/|| icin kosullar sayi veya laf olmali");
            return SType::INT;
        }

        SType A = TypeOfExpr(b->a.get(), tenv);
        SType B = TypeOfExpr(b->b.get(), tenv);

        switch (b->op) {
        case BinOp::Add:
            if (A == SType::STRING || B == SType::STRING) return SType::STRING;
            if (!IsNumericSType(A) || !IsNumericSType(B))
                throw TypeError(e->line, e->col, L"'+' icin sayi veya laf bekleniyordu");
            return PromoteNumeric(A, B);

        case BinOp::Sub:
        case BinOp::Mul:
        case BinOp::Div:
        case BinOp::Mod:
            if (!IsNumericSType(A) || !IsNumericSType(B))
                throw TypeError(e->line, e->col, L"Aritmetik icin sayi bekleniyordu");
            return PromoteNumeric(A, B);

        case BinOp::Eq:
        case BinOp::Ne:
        case BinOp::Lt:
        case BinOp::Gt:
        case BinOp::Le:
        case BinOp::Ge:
            return SType::INT;

        default:
            return SType::ANY;
        }
    }

    return SType::ANY;
}

static void TypeCheckStmt(const Stmt* s, TypeEnv& tenv);

static void TypeCheckBlock(const BlockStmt* b, TypeEnv& tenv) {
    tenv.push();
    for (auto& st : b->stmts) TypeCheckStmt(st.get(), tenv);
    tenv.pop();
}

static void TypeCheckStmt(const Stmt* s, TypeEnv& tenv) {
    if (!s) return;

    if (auto b = dynamic_cast<const BlockStmt*>(s)) { TypeCheckBlock(b, tenv); return; }

    if (auto vd = dynamic_cast<const VarDeclStmt*>(s)) {
        SType to = VTypeToSType(vd->declaredType);
        SType from = vd->init ? TypeOfExpr(vd->init.get(), tenv) : to;
        if (!tenv.assignable(to, from))
            throw TypeError(vd->line, vd->col, std::wstring(L"Degisken tur uyusmazligi: ") + vd->name +
                L" beklenen=" + STypeName(to) + L", gelen=" + STypeName(from));
        tenv.declare(vd->name, to, vd->isConst);
        return;
    }

    if (auto as = dynamic_cast<const AssignStmt*>(s)) {
        TypeSlot ts = tenv.getSlot(as->name);
        if (ts.t == SType::INVALID)
            throw TypeError(as->line, as->col, L"Tanimlanmamis degiskene atama: " + as->name);
        if (ts.isConst)
            throw TypeError(as->line, as->col, L"sabit degisken degistirilemez: " + as->name);

        SType v = TypeOfExpr(as->value.get(), tenv);
        if (!tenv.assignable(ts.t, v))
            throw TypeError(as->line, as->col, std::wstring(L"Atama tur uyusmazligi: ") + as->name +
                L" beklenen=" + STypeName(ts.t) + L", gelen=" + STypeName(v));
        return;
    }

    if (auto pr = dynamic_cast<const PrintStmt*>(s)) { (void)TypeOfExpr(pr->expr.get(), tenv); return; }

    if (auto iff = dynamic_cast<const IfStmt*>(s)) {
        SType c = TypeOfExpr(iff->cond.get(), tenv);
        if (!CondAllowed(c))
            throw TypeError(iff->line, iff->col, L"sayet kosulu sayi veya laf olmali");
        TypeCheckStmt(iff->thenS.get(), tenv);
        if (iff->elseS) TypeCheckStmt(iff->elseS.get(), tenv);
        return;
    }

    if (auto wh = dynamic_cast<const WhileStmt*>(s)) {
        SType c = TypeOfExpr(wh->cond.get(), tenv);
        if (!CondAllowed(c))
            throw TypeError(wh->line, wh->col, L"oldukca kosulu sayi veya laf olmali");
        TypeCheckStmt(wh->body.get(), tenv);
        return;
    }

    if (auto dw = dynamic_cast<const DoWhileStmt*>(s)) {
        SType c = TypeOfExpr(dw->cond.get(), tenv);
        if (!CondAllowed(c))
            throw TypeError(dw->line, dw->col, L"do-while kosulu sayi veya laf olmali");
        TypeCheckStmt(dw->body.get(), tenv);
        return;
    }

    if (auto fr = dynamic_cast<const ForStmt*>(s)) {
        tenv.push();
        if (fr->init) TypeCheckStmt(fr->init.get(), tenv);
        if (fr->cond) {
            SType c = TypeOfExpr(fr->cond.get(), tenv);
            if (!CondAllowed(c))
                throw TypeError(fr->line, fr->col, L"for kosulu sayi veya laf olmali");
        }
        if (fr->step) TypeCheckStmt(fr->step.get(), tenv);
        TypeCheckStmt(fr->body.get(), tenv);
        tenv.pop();
        return;
    }

    if (auto rt = dynamic_cast<const ReturnStmt*>(s)) {
        if (rt->value) (void)TypeOfExpr(rt->value.get(), tenv);
        return;
    }
}

static void TypeCheckProgram(const Program& p, TypeEnv& tenv) { TypeCheckBlock(p.main.get(), tenv); }

// ===================== IR (aynı) =====================
static void EmitExprIR(const Expr* e, std::wstringstream& ir);

static void EmitStmtIR(const Stmt* s, std::wstringstream& ir) {
    if (!s) return;

    if (auto b = dynamic_cast<const BlockStmt*>(s)) {
        ir << L".block\n";
        for (auto& st : b->stmts) EmitStmtIR(st.get(), ir);
        ir << L".endblock\n";
        return;
    }
    if (auto vd = dynamic_cast<const VarDeclStmt*>(s)) {
        ir << L"DECL " << (vd->isConst ? L"CONST " : L"") << vd->name << L"\n";
        if (vd->init) { EmitExprIR(vd->init.get(), ir); ir << L"STORE " << vd->name << L"\n"; }
        return;
    }
    if (auto as = dynamic_cast<const AssignStmt*>(s)) {
        EmitExprIR(as->value.get(), ir);
        ir << L"STORE " << as->name << L"\n";
        return;
    }
    if (auto pr = dynamic_cast<const PrintStmt*>(s)) {
        EmitExprIR(pr->expr.get(), ir);
        ir << L"PRINT\n";
        return;
    }
    if (auto rt = dynamic_cast<const ReturnStmt*>(s)) {
        if (rt->value) EmitExprIR(rt->value.get(), ir);
        else ir << L"PUSH_INT 0\n";
        ir << L"RET\n";
        return;
    }

    ir << L"; (IR: bu statement sonraya) line=" << s->line << L"\n";
}

static void EmitExprIR(const Expr* e, std::wstringstream& ir) {
    if (!e) { ir << L"PUSH_INT 0\n"; return; }

    if (auto x = dynamic_cast<const IntExpr*>(e)) { ir << L"PUSH_INT " << x->v << L"\n"; return; }
    if (auto x = dynamic_cast<const DoubleExpr*>(e)) { ir << L"PUSH_DBL " << x->v << L"\n"; return; }
    if (auto x = dynamic_cast<const StrExpr*>(e)) { ir << L"PUSH_STR \"" << x->v << L"\"\n"; return; }
    if (auto v = dynamic_cast<const VarExpr*>(e)) { ir << L"LOAD " << v->name << L"\n"; return; }
    if (auto c = dynamic_cast<const CallExpr*>(e)) {
        ir << L"CALL " << c->name << L" argc=" << (int)c->args.size() << L"\n";
        return;
    }
    if (auto b = dynamic_cast<const BinExpr*>(e)) {
        EmitExprIR(b->a.get(), ir);
        EmitExprIR(b->b.get(), ir);
        switch (b->op) {
        case BinOp::Add: ir << L"ADD\n"; break;
        case BinOp::Sub: ir << L"SUB\n"; break;
        case BinOp::Mul: ir << L"MUL\n"; break;
        case BinOp::Div: ir << L"DIV\n"; break;
        case BinOp::Mod: ir << L"MOD\n"; break;
        case BinOp::Eq:  ir << L"EQ\n";  break;
        case BinOp::Ne:  ir << L"NE\n";  break;
        case BinOp::Lt:  ir << L"LT\n";  break;
        case BinOp::Gt:  ir << L"GT\n";  break;
        case BinOp::Le:  ir << L"LE\n";  break;
        case BinOp::Ge:  ir << L"GE\n";  break;
        case BinOp::AndAnd: ir << L"AND\n"; break;
        case BinOp::OrOr:  ir << L"OR\n";  break;
        }
        return;
    }

    ir << L"; (IR: expr sonraya) line=" << e->line << L"\n";
}

// ===================== BYTECODE + VM =====================
enum class Op : int {
    NOP = 0,
    PUSH_CONST,
    POP,

    ENTER_SCOPE,
    EXIT_SCOPE,

    LOAD,
    DECLARE,
    STORE,

    TOBOOL,

    ADD, SUB, MUL, DIV, MOD,
    EQ, NE, LT, GT, LE, GE,

    JMP,
    JMPF,
    JMPT,

    PRINT,

    CALL_BUILTIN,
    RETURN,
};

enum class BuiltinId : int {
    OkuGari = 0,
    OkuTamsayi = 1,
    OkuHassasSayi = 2,
    Kok = 3,
    Us = 4,
    Mutlak = 5,
    Taban = 6,
    Tavan = 7,
    Yuvarla = 8,
    MinGari = 9,
    MaxGari = 10,
    Sinus = 11,
    Kosinus = 12,
    Tanjant = 13,
    Cotanjant = 14,
    Ln = 15,
    Log10 = 16,
    Ustel = 17,
    PiSayisi = 18,
    ESayisi = 19,
};

struct Instr {
    Op op = Op::NOP;
    int a = 0;
    int b = 0;
    int line = 1;
    int col = 1;
};

struct Chunk {
    std::vector<Instr> code;
    std::vector<Value> consts;
};

static int AddConst(Chunk& ch, const Value& v) {
    ch.consts.push_back(v);
    return (int)ch.consts.size() - 1;
}

static std::wstring OpName(Op op) {
    switch (op) {
    case Op::NOP: return L"NOP";
    case Op::PUSH_CONST: return L"PUSH_CONST";
    case Op::POP: return L"POP";
    case Op::ENTER_SCOPE: return L"ENTER_SCOPE";
    case Op::EXIT_SCOPE: return L"EXIT_SCOPE";
    case Op::LOAD: return L"LOAD";
    case Op::DECLARE: return L"DECLARE";
    case Op::STORE: return L"STORE";
    case Op::TOBOOL: return L"TOBOOL";
    case Op::ADD: return L"ADD";
    case Op::SUB: return L"SUB";
    case Op::MUL: return L"MUL";
    case Op::DIV: return L"DIV";
    case Op::MOD: return L"MOD";
    case Op::EQ: return L"EQ";
    case Op::NE: return L"NE";
    case Op::LT: return L"LT";
    case Op::GT: return L"GT";
    case Op::LE: return L"LE";
    case Op::GE: return L"GE";
    case Op::JMP: return L"JMP";
    case Op::JMPF: return L"JMPF";
    case Op::JMPT: return L"JMPT";
    case Op::PRINT: return L"PRINT";
    case Op::CALL_BUILTIN: return L"CALL_BUILTIN";
    case Op::RETURN: return L"RETURN";
    default: return L"?";
    }
}

static std::wstring BuiltinName(int id) {
    switch ((BuiltinId)id) {
    case BuiltinId::OkuGari:       return L"oku_gari";
    case BuiltinId::OkuTamsayi:    return L"oku_tamsayi";
    case BuiltinId::OkuHassasSayi: return L"oku_hassas_sayi";
    case BuiltinId::Kok:           return L"kok";
    case BuiltinId::Us:            return L"us";
    case BuiltinId::Mutlak:        return L"mutlak";
    case BuiltinId::Taban:         return L"taban";
    case BuiltinId::Tavan:         return L"tavan";
    case BuiltinId::Yuvarla:       return L"yuvarla";
    case BuiltinId::MinGari:       return L"min_gari";
    case BuiltinId::MaxGari:       return L"max_gari";
    case BuiltinId::Sinus:         return L"sinus";
    case BuiltinId::Kosinus:       return L"kosinus";
    case BuiltinId::Tanjant:       return L"tanjant";
    case BuiltinId::Cotanjant:     return L"cotanjant";
    case BuiltinId::Ln:            return L"ln";
    case BuiltinId::Log10:         return L"log10";
    case BuiltinId::Ustel:         return L"ustel";
    case BuiltinId::PiSayisi:      return L"pi_sayisi";
    case BuiltinId::ESayisi:       return L"e_sayisi";
    default: return L"";
    }
}

static int BuiltinIdFromName(const std::wstring& name) {
    if (name == L"oku_gari")         return (int)BuiltinId::OkuGari;
    if (name == L"oku_tamsayi")      return (int)BuiltinId::OkuTamsayi;
    if (name == L"oku_hassas_sayi")  return (int)BuiltinId::OkuHassasSayi;
    if (name == L"kok")              return (int)BuiltinId::Kok;
    if (name == L"us")               return (int)BuiltinId::Us;
    if (name == L"mutlak")           return (int)BuiltinId::Mutlak;
    if (name == L"taban")            return (int)BuiltinId::Taban;
    if (name == L"tavan")            return (int)BuiltinId::Tavan;
    if (name == L"yuvarla")          return (int)BuiltinId::Yuvarla;
    if (name == L"min_gari")         return (int)BuiltinId::MinGari;
    if (name == L"max_gari")         return (int)BuiltinId::MaxGari;
    if (name == L"sinus")            return (int)BuiltinId::Sinus;
    if (name == L"kosinus")          return (int)BuiltinId::Kosinus;
    if (name == L"tanjant")          return (int)BuiltinId::Tanjant;
    if (name == L"cotanjant")        return (int)BuiltinId::Cotanjant;
    if (name == L"ln")               return (int)BuiltinId::Ln;
    if (name == L"log10")            return (int)BuiltinId::Log10;
    if (name == L"ustel")            return (int)BuiltinId::Ustel;
    if (name == L"pi_sayisi")        return (int)BuiltinId::PiSayisi;
    if (name == L"e_sayisi")         return (int)BuiltinId::ESayisi;
    return -1;
}

static std::wstring ValueTypeName(Value::Type t) {
    switch (t) {
    case Value::Type::INT: return L"tamsayi";
    case Value::Type::FLOAT: return L"float";
    case Value::Type::DOUBLE: return L"double";
    case Value::Type::STRING: return L"laf";
    default: return L"?";
    }
}

// ---------- Compiler ----------
struct Sym {
    int slot = -1;
    bool isConst = false;
    Value::Type declared = Value::Type::INT;
};

struct Compiler {
    Chunk ch;
    std::vector<std::unordered_map<std::wstring, Sym>> scopes;
    std::vector<int> scopeSlotBase;
    int nextSlot = 0;

    Compiler() {
        scopes.push_back({});
        scopeSlotBase.push_back(0);
    }

    void emit(Op op, int a = 0, int b = 0, int line = 1, int col = 1) {
        ch.code.push_back(Instr{ op, a, b, line, col });
    }

    int emitJump(Op op, int line, int col) {
        emit(op, -1, 0, line, col);
        return (int)ch.code.size() - 1;
    }

    void patchJump(int at, int target) {
        if (at < 0 || at >= (int)ch.code.size()) return;
        ch.code[at].a = target;
    }

    void enterScope(int line, int col) {
        scopes.push_back({});
        scopeSlotBase.push_back(nextSlot);
        emit(Op::ENTER_SCOPE, nextSlot, 0, line, col);
    }
    void exitScope(int line, int col) {
        int base = scopeSlotBase.back();
        scopes.pop_back();
        scopeSlotBase.pop_back();
        nextSlot = base;
        emit(Op::EXIT_SCOPE, 0, 0, line, col);
    }

    Sym* resolve(const std::wstring& name) {
        for (int i = (int)scopes.size() - 1; i >= 0; --i) {
            auto it = scopes[i].find(name);
            if (it != scopes[i].end()) return &it->second;
        }
        return nullptr;
    }

    int declareSym(const std::wstring& name, Value::Type t, bool isConst, int line, int col) {
        auto& cur = scopes.back();
        if (cur.find(name) != cur.end()) {
            throw RuntimeError(line, col, "NameError", "Ayni scopeta tekrar tanim: " + WToNarrow(name));
        }
        int slot = nextSlot++;
        cur[name] = Sym{ slot, isConst, t };
        return slot;
    }

    static int PackDecl(Value::Type t, bool isConst) {
        return ((int)t) | (isConst ? 0x100 : 0);
    }

    void compileExpr(const Expr* e) {
        if (!e) { emit(Op::PUSH_CONST, AddConst(ch, Value::makeInt(0)), 0, 1, 1); return; }

        if (auto ie = dynamic_cast<const IntExpr*>(e)) {
            emit(Op::PUSH_CONST, AddConst(ch, Value::makeInt(ie->v)), 0, e->line, e->col);
            return;
        }
        if (auto de = dynamic_cast<const DoubleExpr*>(e)) {
            emit(Op::PUSH_CONST, AddConst(ch, Value::makeDouble(de->v)), 0, e->line, e->col);
            return;
        }
        if (auto se = dynamic_cast<const StrExpr*>(e)) {
            emit(Op::PUSH_CONST, AddConst(ch, Value::makeString(se->v)), 0, e->line, e->col);
            return;
        }
        if (auto ve = dynamic_cast<const VarExpr*>(e)) {
            Sym* s = resolve(ve->name);
            if (!s) throw RuntimeError(e->line, e->col, "NameError", "Tanimlanmamis degisken: " + WToNarrow(ve->name));
            emit(Op::LOAD, s->slot, 0, e->line, e->col);
            return;
        }
        if (auto ce = dynamic_cast<const CallExpr*>(e)) {
            int bid = BuiltinIdFromName(ce->name);
            if (bid < 0)
                throw RuntimeError(e->line, e->col, "NameError", UnknownCallMsg(ce->name));

            for (auto& a : ce->args) compileExpr(a.get());
            emit(Op::CALL_BUILTIN, bid, (int)ce->args.size(), e->line, e->col);
            return;
        }
        if (auto be = dynamic_cast<const BinExpr*>(e)) {
            if (be->op == BinOp::AndAnd) {
                compileExpr(be->a.get());
                int jf = emitJump(Op::JMPF, e->line, e->col);
                compileExpr(be->b.get());
                emit(Op::TOBOOL, 0, 0, e->line, e->col);
                int jend = emitJump(Op::JMP, e->line, e->col);
                int Lfalse = (int)ch.code.size();
                patchJump(jf, Lfalse);
                emit(Op::PUSH_CONST, AddConst(ch, Value::makeInt(0)), 0, e->line, e->col);
                int Lend = (int)ch.code.size();
                patchJump(jend, Lend);
                return;
            }
            if (be->op == BinOp::OrOr) {
                compileExpr(be->a.get());
                int jt = emitJump(Op::JMPT, e->line, e->col);
                compileExpr(be->b.get());
                emit(Op::TOBOOL, 0, 0, e->line, e->col);
                int jend = emitJump(Op::JMP, e->line, e->col);
                int Ltrue = (int)ch.code.size();
                patchJump(jt, Ltrue);
                emit(Op::PUSH_CONST, AddConst(ch, Value::makeInt(1)), 0, e->line, e->col);
                int Lend = (int)ch.code.size();
                patchJump(jend, Lend);
                return;
            }

            compileExpr(be->a.get());
            compileExpr(be->b.get());

            switch (be->op) {
            case BinOp::Add: emit(Op::ADD, 0, 0, e->line, e->col); return;
            case BinOp::Sub: emit(Op::SUB, 0, 0, e->line, e->col); return;
            case BinOp::Mul: emit(Op::MUL, 0, 0, e->line, e->col); return;
            case BinOp::Div: emit(Op::DIV, 0, 0, e->line, e->col); return;
            case BinOp::Mod: emit(Op::MOD, 0, 0, e->line, e->col); return;
            case BinOp::Eq:  emit(Op::EQ, 0, 0, e->line, e->col); return;
            case BinOp::Ne:  emit(Op::NE, 0, 0, e->line, e->col); return;
            case BinOp::Lt:  emit(Op::LT, 0, 0, e->line, e->col); return;
            case BinOp::Gt:  emit(Op::GT, 0, 0, e->line, e->col); return;
            case BinOp::Le:  emit(Op::LE, 0, 0, e->line, e->col); return;
            case BinOp::Ge:  emit(Op::GE, 0, 0, e->line, e->col); return;
            default: break;
            }
        }

        throw RuntimeError(e->line, e->col, "CompileError", "Bilinmeyen expr node");
    }

    void compileStmt(const Stmt* s) {
        if (!s) return;

        if (auto b = dynamic_cast<const BlockStmt*>(s)) {
            enterScope(s->line, s->col);
            for (auto& st : b->stmts) compileStmt(st.get());
            exitScope(s->line, s->col);
            return;
        }

        if (auto vd = dynamic_cast<const VarDeclStmt*>(s)) {
            if (vd->init) compileExpr(vd->init.get());
            else emit(Op::PUSH_CONST, AddConst(ch, Value::makeInt(0)), 0, s->line, s->col);

            int slot = declareSym(vd->name, vd->declaredType, vd->isConst, s->line, s->col);
            emit(Op::DECLARE, slot, PackDecl(vd->declaredType, vd->isConst), s->line, s->col);
            return;
        }

        if (auto as = dynamic_cast<const AssignStmt*>(s)) {
            Sym* sym = resolve(as->name);
            if (!sym) throw RuntimeError(s->line, s->col, "NameError", "Tanimlanmamis degiskene atama: " + WToNarrow(as->name));
            compileExpr(as->value.get());
            emit(Op::STORE, sym->slot, 0, s->line, s->col);
            return;
        }

        if (auto pr = dynamic_cast<const PrintStmt*>(s)) {
            compileExpr(pr->expr.get());
            emit(Op::PRINT, 0, 0, s->line, s->col);
            return;
        }

        if (auto iff = dynamic_cast<const IfStmt*>(s)) {
            compileExpr(iff->cond.get());
            int jf = emitJump(Op::JMPF, s->line, s->col);

            compileStmt(iff->thenS.get());
            int jend = emitJump(Op::JMP, s->line, s->col);

            int Lelse = (int)ch.code.size();
            patchJump(jf, Lelse);

            if (iff->elseS) compileStmt(iff->elseS.get());

            int Lend = (int)ch.code.size();
            patchJump(jend, Lend);
            return;
        }

        if (auto wh = dynamic_cast<const WhileStmt*>(s)) {
            int Lstart = (int)ch.code.size();
            compileExpr(wh->cond.get());
            int jf = emitJump(Op::JMPF, s->line, s->col);
            compileStmt(wh->body.get());
            emit(Op::JMP, Lstart, 0, s->line, s->col);
            int Lend = (int)ch.code.size();
            patchJump(jf, Lend);
            return;
        }

        if (auto dw = dynamic_cast<const DoWhileStmt*>(s)) {
            int Lstart = (int)ch.code.size();
            compileStmt(dw->body.get());
            compileExpr(dw->cond.get());
            emit(Op::JMPT, Lstart, 0, s->line, s->col);
            return;
        }

        if (auto fr = dynamic_cast<const ForStmt*>(s)) {
            enterScope(s->line, s->col);

            if (fr->init) compileStmt(fr->init.get());

            int Lstart = (int)ch.code.size();
            compileExpr(fr->cond.get());
            int jf = emitJump(Op::JMPF, s->line, s->col);

            compileStmt(fr->body.get());
            if (fr->step) compileStmt(fr->step.get());

            emit(Op::JMP, Lstart, 0, s->line, s->col);

            int Lend = (int)ch.code.size();
            patchJump(jf, Lend);

            exitScope(s->line, s->col);
            return;
        }

        if (auto rt = dynamic_cast<const ReturnStmt*>(s)) {
            if (rt->value) {
                compileExpr(rt->value.get());
                emit(Op::RETURN, 1, 0, s->line, s->col);
            }
            else {
                emit(Op::RETURN, 0, 0, s->line, s->col);
            }
            return;
        }

        throw RuntimeError(s->line, s->col, "CompileError", "Bilinmeyen stmt node");
    }

    Chunk compileMain(const Program& p) {
        compileStmt(p.main.get());
        emit(Op::RETURN, 0, 0, 1, 1);
        return std::move(ch);
    }
};

// ---------- Disassembler ----------
static std::wstring DisasmChunk(const Chunk& ch) {
    std::wstringstream ss;
    ss << L"; ===== BYTECODE =====\n";
    ss << L"; consts: " << (int)ch.consts.size() << L"\n\n";
    for (size_t i = 0; i < ch.code.size(); ++i) {
        const auto& in = ch.code[i];
        ss << std::setw(4) << i << L"  " << OpName(in.op);
        if (in.op == Op::PUSH_CONST) ss << L" " << in.a << L" ; " << ValueToString(ch.consts[(size_t)in.a]);
        else if (in.op == Op::LOAD || in.op == Op::STORE) ss << L" slot=" << in.a;
        else if (in.op == Op::DECLARE) {
            int t = (in.b & 0xFF);
            bool isConst = (in.b & 0x100) != 0;
            ss << L" slot=" << in.a << L" type=" << ValueTypeName((Value::Type)t) << (isConst ? L" const" : L"");
        }
        else if (in.op == Op::JMP || in.op == Op::JMPF || in.op == Op::JMPT) ss << L" -> " << in.a;
        else if (in.op == Op::CALL_BUILTIN) ss << L" " << BuiltinName(in.a) << L" argc=" << in.b;
        else if (in.op == Op::RETURN) ss << L" hasValue=" << in.a;
        ss << L"  ; (" << in.line << L":" << in.col << L")\n";
    }
    return ss.str();
}

// ---------- VM ----------
struct VM {
    const Chunk* ch = nullptr;
    std::vector<Value> stack;

    std::vector<Value> locals;
    std::vector<Value::Type> localType;
    std::vector<bool> localConst;
    std::vector<int> scopeBases;

    std::wstringstream out;

    void ensureSlot(int slot) {
        if (slot < 0) return;
        if ((int)locals.size() <= slot) {
            locals.resize((size_t)slot + 1);
            localType.resize((size_t)slot + 1, Value::Type::INT);
            localConst.resize((size_t)slot + 1, false);
        }
    }

    Value pop() {
        if (stack.empty()) return Value::makeInt(0);
        Value v = stack.back();
        stack.pop_back();
        return v;
    }
    void push(const Value& v) { stack.push_back(v); }

    static Value BoolV(bool b) { return Value::makeInt(b ? 1 : 0); }

    void doBinary(Op op, int line, int col) {
        Value B = pop();
        Value A = pop();

        if (op == Op::EQ || op == Op::NE || op == Op::LT || op == Op::GT || op == Op::LE || op == Op::GE) {
            bool res = false;
            if (op == Op::EQ) res = ValueEquals(A, B);
            else if (op == Op::NE) res = !ValueEquals(A, B);
            else {
                if (A.type == Value::Type::STRING || B.type == Value::Type::STRING) {
                    std::wstring x = ValueToString(A), y = ValueToString(B);
                    if (op == Op::LT) res = (x < y);
                    if (op == Op::GT) res = (x > y);
                    if (op == Op::LE) res = (x <= y);
                    if (op == Op::GE) res = (x >= y);
                }
                else {
                    double x = ToDouble(A), y = ToDouble(B);
                    if (op == Op::LT) res = (x < y);
                    if (op == Op::GT) res = (x > y);
                    if (op == Op::LE) res = (x <= y);
                    if (op == Op::GE) res = (x >= y);
                }
            }
            push(BoolV(res));
            return;
        }

        if (op == Op::ADD && (A.type == Value::Type::STRING || B.type == Value::Type::STRING)) {
            push(Value::makeString(ValueToString(A) + ValueToString(B)));
            return;
        }

        if (op == Op::MOD) {
            if (!IsNumeric(A.type) || !IsNumeric(B.type))
                throw RuntimeError(line, col, "TypeError", "'%' sadece sayilarda kullanilir");
        }

        if (A.type == Value::Type::DOUBLE || B.type == Value::Type::DOUBLE) {
            double x = ToDouble(A), y = ToDouble(B);
            if (op == Op::ADD) push(Value::makeDouble(x + y));
            else if (op == Op::SUB) push(Value::makeDouble(x - y));
            else if (op == Op::MUL) push(Value::makeDouble(x * y));
            else if (op == Op::DIV) {
                if (y == 0.0) throw RuntimeError(line, col, "ZeroDivision", ZeroDivMsg("/", A, B));
                push(Value::makeDouble(x / y));
            }
            else if (op == Op::MOD) {
                if (y == 0.0) throw RuntimeError(line, col, "ZeroDivision", ZeroDivMsg("%", A, B));
                push(Value::makeDouble(std::fmod(x, y)));
            }
            else push(Value::makeDouble(0.0));
            return;
        }

        if (A.type == Value::Type::FLOAT || B.type == Value::Type::FLOAT) {
            float x = ToFloat(A), y = ToFloat(B);
            if (op == Op::ADD) push(Value::makeFloat(x + y));
            else if (op == Op::SUB) push(Value::makeFloat(x - y));
            else if (op == Op::MUL) push(Value::makeFloat(x * y));
            else if (op == Op::DIV) {
                if (y == 0.0f) throw RuntimeError(line, col, "ZeroDivision", ZeroDivMsg("/", A, B));
                push(Value::makeFloat(x / y));
            }
            else if (op == Op::MOD) {
                if (y == 0.0f) throw RuntimeError(line, col, "ZeroDivision", ZeroDivMsg("%", A, B));
                push(Value::makeFloat(std::fmod(x, y)));
            }
            else push(Value::makeFloat(0.0f));
            return;
        }

        int x = ToInt(A), y = ToInt(B);
        if (op == Op::ADD) push(Value::makeInt(x + y));
        else if (op == Op::SUB) push(Value::makeInt(x - y));
        else if (op == Op::MUL) push(Value::makeInt(x * y));
        else if (op == Op::DIV) {
            if (y == 0) throw RuntimeError(line, col, "ZeroDivision", ZeroDivMsg("/", A, B));
            push(Value::makeInt(x / y));
        }
        else if (op == Op::MOD) {
            if (y == 0) throw RuntimeError(line, col, "ZeroDivision", ZeroDivMsg("%", A, B));
            push(Value::makeInt(x % y));
        }
        else push(Value::makeInt(0));
    }

    struct Result {
        Value ret;
        bool hasReturn = false;
        std::wstring output;
    };

    Result run(const Chunk& chunk) {
        ch = &chunk;
        stack.clear();
        locals.clear();
        localType.clear();
        localConst.clear();
        scopeBases.clear();
        out.str(L"");
        out.clear();

        scopeBases.push_back(0);

        int ip = 0;
        while (ip >= 0 && ip < (int)ch->code.size()) {
            const Instr& in = ch->code[(size_t)ip];

            switch (in.op) {
            case Op::NOP: ip++; break;

            case Op::PUSH_CONST:
                push(ch->consts[(size_t)in.a]);
                ip++;
                break;

            case Op::POP:
                (void)pop();
                ip++;
                break;

            case Op::ENTER_SCOPE:
                scopeBases.push_back((int)locals.size());
                ip++;
                break;

            case Op::EXIT_SCOPE: {
                int base = scopeBases.back();
                scopeBases.pop_back();
                if ((int)locals.size() > base) {
                    locals.resize((size_t)base);
                    localType.resize((size_t)base);
                    localConst.resize((size_t)base);
                }
                ip++;
                break;
            }

            case Op::LOAD:
                ensureSlot(in.a);
                push(locals[(size_t)in.a]);
                ip++;
                break;

            case Op::DECLARE: {
                ensureSlot(in.a);
                Value v = pop();
                Value::Type declT = (Value::Type)(in.b & 0xFF);
                bool isConst = (in.b & 0x100) != 0;

                v = Coerce(v, declT);
                locals[(size_t)in.a] = v;
                localType[(size_t)in.a] = declT;
                localConst[(size_t)in.a] = isConst;
                ip++;
                break;
            }

            case Op::STORE: {
                ensureSlot(in.a);
                if (localConst[(size_t)in.a]) {
                    throw RuntimeError(in.line, in.col, "ConstError", "sabit degisken degistirilemez");
                }
                Value v = pop();
                v = Coerce(v, localType[(size_t)in.a]);
                locals[(size_t)in.a] = v;
                ip++;
                break;
            }

            case Op::TOBOOL: {
                Value v = pop();
                push(Value::makeInt(IsTrue(v) ? 1 : 0));
                ip++;
                break;
            }

            case Op::ADD: case Op::SUB: case Op::MUL: case Op::DIV: case Op::MOD:
            case Op::EQ: case Op::NE: case Op::LT: case Op::GT: case Op::LE: case Op::GE:
                doBinary(in.op, in.line, in.col);
                ip++;
                break;

            case Op::JMP:
                ip = in.a;
                break;

            case Op::JMPF: {
                Value c = pop();
                if (!IsTrue(c)) ip = in.a;
                else ip++;
                break;
            }

            case Op::JMPT: {
                Value c = pop();
                if (IsTrue(c)) ip = in.a;
                else ip++;
                break;
            }

            case Op::PRINT: {
                Value v = pop();
                out << ValueToString(v) << L"\r\n";
                ip++;
                break;
            }

            case Op::CALL_BUILTIN: {
                int argc = in.b;
                std::vector<Value> args((size_t)argc);
                for (int k = argc - 1; k >= 0; --k) {
                    args[(size_t)k] = pop();
                }

                std::wstring bname = BuiltinName(in.a);
                if (bname.empty())
                    throw RuntimeError(in.line, in.col, "NameError", "Bilinmeyen builtin");

                push(EvalBuiltinCall(bname, args, in.line, in.col));
                ip++;
                break;
            }

            case Op::RETURN: {
                Value r = Value::makeInt(0);
                if (in.a != 0) r = pop();
                Result res;
                res.ret = r;
                res.hasReturn = true;
                res.output = out.str();
                return res;
            }

            default:
                throw RuntimeError(in.line, in.col, "VMError", "Bilinmeyen opcode");
            }
        }

        Result res;
        res.ret = Value::makeInt(0);
        res.hasReturn = false;
        res.output = out.str();
        return res;
    }
};

// BuildIRText now shows the VM bytecode disassembly (real executable code).
static std::wstring BuildIRText(const Program& p) {
    Compiler c;
    Chunk ch = c.compileMain(p);
    return DisasmChunk(ch);
}

// ===================== Exec =====================
struct ExecResult {
    enum class Flow { Normal, Return } flow = Flow::Normal;
    Value returnValue;

    static ExecResult None() { return {}; }
    static ExecResult Ret(const Value& v) { ExecResult r; r.flow = Flow::Return; r.returnValue = v; return r; }
};

static ExecResult ExecStmt(const Stmt* s, Env& env, std::wstringstream& out);

static ExecResult ExecBlockWithOut(const BlockStmt* b, Env& env, std::wstringstream& out) {
    env.push();
    for (auto& st : b->stmts) {
        ExecResult r = ExecStmt(st.get(), env, out);
        if (r.flow != ExecResult::Flow::Normal) { env.pop(); return r; }
    }
    env.pop();
    return ExecResult::None();
}

static ExecResult ExecStmt(const Stmt* s, Env& env, std::wstringstream& out) {
    if (auto b = dynamic_cast<const BlockStmt*>(s)) return ExecBlockWithOut(b, env, out);

    if (auto vd = dynamic_cast<const VarDeclStmt*>(s)) {
        Value v = vd->init ? vd->init->eval(env) : Value::makeInt(0);
        v = Coerce(v, vd->declaredType);
        env.declare(vd->name, v, vd->isConst);
        return ExecResult::None();
    }

    if (auto as = dynamic_cast<const AssignStmt*>(s)) {
        Value v = as->value->eval(env);
        if (!env.assign(as->name, v, as->line, as->col))
            throw RuntimeError(as->line, as->col, "NameError", "Tanimlanmamis degiskene atama: " + WToNarrow(as->name));
        return ExecResult::None();
    }

    if (auto pr = dynamic_cast<const PrintStmt*>(s)) {
        Value v = pr->expr->eval(env);
        out << ValueToString(v) << L"\r\n";
        return ExecResult::None();
    }

    if (auto iff = dynamic_cast<const IfStmt*>(s)) {
        Value c = iff->cond->eval(env);
        if (IsTrue(c)) return ExecStmt(iff->thenS.get(), env, out);
        if (iff->elseS) return ExecStmt(iff->elseS.get(), env, out);
        return ExecResult::None();
    }

    if (auto wh = dynamic_cast<const WhileStmt*>(s)) {
        int guard = 0;
        while (IsTrue(wh->cond->eval(env))) {
            ExecResult r = ExecStmt(wh->body.get(), env, out);
            if (r.flow == ExecResult::Flow::Return) return r;
            if (++guard > 200000) throw RuntimeError(wh->line, wh->col, "LoopError", "Muhtemel sonsuz while");
        }
        return ExecResult::None();
    }

    if (auto dw = dynamic_cast<const DoWhileStmt*>(s)) {
        int guard = 0;
        do {
            ExecResult r = ExecStmt(dw->body.get(), env, out);
            if (r.flow == ExecResult::Flow::Return) return r;
            if (++guard > 200000) throw RuntimeError(dw->line, dw->col, "LoopError", "Muhtemel sonsuz do-while");
        } while (IsTrue(dw->cond->eval(env)));
        return ExecResult::None();
    }

    if (auto fr = dynamic_cast<const ForStmt*>(s)) {
        env.push();
        if (fr->init) {
            ExecResult ir = ExecStmt(fr->init.get(), env, out);
            if (ir.flow != ExecResult::Flow::Normal) { env.pop(); return ir; }
        }

        int guard = 0;
        while (IsTrue(fr->cond->eval(env))) {
            ExecResult r = ExecStmt(fr->body.get(), env, out);
            if (r.flow == ExecResult::Flow::Return) { env.pop(); return r; }

            if (fr->step) {
                ExecResult sr = ExecStmt(fr->step.get(), env, out);
                if (sr.flow == ExecResult::Flow::Return) { env.pop(); return sr; }
            }

            if (++guard > 200000) { env.pop(); throw RuntimeError(fr->line, fr->col, "LoopError", "Muhtemel sonsuz for"); }
        }
        env.pop();
        return ExecResult::None();
    }

    if (auto rt = dynamic_cast<const ReturnStmt*>(s)) {
        Value v = rt->value ? rt->value->eval(env) : Value::makeInt(0);
        return ExecResult::Ret(v);
    }

    return ExecResult::None();
}

// ===================== Public build helpers =====================
std::wstring BuildTokensTextFromSource(const std::wstring& source) {
    try {
        Lexer lx(source);
        auto toks = lx.tokenize();
        return TokensToText(toks);
    }
    catch (const LexError& le) {
        return L"LexError line " + std::to_wstring(le.line) + L" col " + std::to_wstring(le.col) + L": " + WidenC(le.what()) + L"\r\n";
    }
    catch (const std::exception& ex) {
        return L"(HATA) " + WidenC(ex.what()) + L"\r\n";
    }
}

std::wstring BuildAstTextFromSource(const std::wstring& source) {
    try {
        Lexer lx(source);
        auto toks = lx.tokenize();
        Parser ps(std::move(toks));
        auto prg = ps.parseProgram();
        return AstToText(*prg);
    }
    catch (const LexError& le) {
        return L"LexError line " + std::to_wstring(le.line) + L" col " + std::to_wstring(le.col) + L": " + WidenC(le.what()) + L"\r\n";
    }
    catch (const ParseError& pe) {
        return L"ParseError line " + std::to_wstring(pe.line) + L" col " + std::to_wstring(pe.col) + L": " + WidenC(pe.what()) + L"\r\n";
    }
    catch (const std::exception& ex) {
        return L"(HATA) " + WidenC(ex.what()) + L"\r\n";
    }
}

std::wstring BuildIRTextFromSource(const std::wstring& source) {
    try {
        Lexer lx(source);
        auto toks = lx.tokenize();
        Parser ps(std::move(toks));
        auto prg = ps.parseProgram();
        return BuildIRText(*prg);
    }
    catch (const LexError& le) {
        return L"LexError line " + std::to_wstring(le.line) + L" col " + std::to_wstring(le.col) + L": " + WidenC(le.what()) + L"\r\n";
    }
    catch (const ParseError& pe) {
        return L"ParseError line " + std::to_wstring(pe.line) + L" col " + std::to_wstring(pe.col) + L": " + WidenC(pe.what()) + L"\r\n";
    }
    catch (const std::exception& ex) {
        return L"(HATA) " + WidenC(ex.what()) + L"\r\n";
    }
}

// ===================== RunCode_Interpreter (returns output text) =====================
std::wstring RunCode_Interpreter(const std::wstring& source) {
    try {
        Lexer lx(source);
        auto toks = lx.tokenize();
        Parser ps(std::move(toks));
        auto prg = ps.parseProgram();

        try {
            TypeEnv tenv;
            TypeCheckProgram(*prg, tenv);
        }
        catch (const TypeError& te) {
            std::wstring out =
                L"TypeError line " + std::to_wstring(te.line) +
                L" col " + std::to_wstring(te.col) + L": " +
                te.msg + L"\r\n";

            std::wstring lineText = GetLineTextAt(source, te.line);
            if (!lineText.empty()) {
                out += L"\r\n> " + lineText + L"\r\n";
                out += L"  " + CaretLine(te.col) + L"\r\n";
            }
            return out;
        }

        Env env;
        std::wstringstream out;
        ExecResult r = ExecBlockWithOut(prg->main.get(), env, out);

        std::wstring text = out.str();
        if (r.flow == ExecResult::Flow::Return)
            text += L"(return: " + ValueToString(r.returnValue) + L")\r\n";
        return text;
    }
    catch (const LexError& le) {
        std::wstring out =
            L"LexError line " + std::to_wstring(le.line) +
            L" col " + std::to_wstring(le.col) +
            L": " + WidenC(le.what()) + L"\r\n";

        std::wstring lineText = GetLineTextAt(source, le.line);
        if (!lineText.empty()) {
            out += L"\r\n> " + lineText + L"\r\n";
            out += L"  " + CaretLine(le.col) + L"\r\n";
        }
        return out;
    }
    catch (const ParseError& pe) {
        std::wstring out =
            L"ParseError line " + std::to_wstring(pe.line) +
            L" col " + std::to_wstring(pe.col) +
            L": " + WidenC(pe.what()) + L"\r\n";

        if (!pe.expected.empty())
            out += L"Expected: " + JoinExpected(pe.expected) + L"\r\n";
        if (!pe.got.empty())
            out += L"Got: " + pe.got + L"\r\n";

        std::wstring lineText = GetLineTextAt(source, pe.line);
        if (!lineText.empty()) {
            out += L"\r\n> " + lineText + L"\r\n";
            out += L"  " + CaretLine(pe.col) + L"\r\n";
        }
        return out;
    }
    catch (const RuntimeError& re) {
        std::wstring out =
            L"RuntimeError line " + std::to_wstring(re.line) +
            L" col " + std::to_wstring(re.col) +
            L": " + WidenC(re.what()) + L"\r\n";

        std::wstring lineText = GetLineTextAt(source, re.line);
        if (!lineText.empty()) {
            out += L"\r\n> " + lineText + L"\r\n";
            out += L"  " + CaretLine(re.col) + L"\r\n";
        }
        return out;
    }
    catch (const std::exception& ex) {
        return L"(HATA) " + WidenC(ex.what()) + L"\r\n";
    }
}

// ===================== RunCode_VM (returns output text) =====================
std::wstring RunCode_VM(const std::wstring& source) {
    try {
        Lexer lx(source);
        auto toks = lx.tokenize();
        Parser ps(std::move(toks));
        auto prg = ps.parseProgram();

        try {
            TypeEnv tenv;
            TypeCheckProgram(*prg, tenv);
        }
        catch (const TypeError& te) {
            std::wstring out =
                L"TypeError line " + std::to_wstring(te.line) +
                L" col " + std::to_wstring(te.col) + L": " +
                te.msg + L"\r\n";

            std::wstring lineText = GetLineTextAt(source, te.line);
            if (!lineText.empty()) {
                out += L"\r\n> " + lineText + L"\r\n";
                out += L"  " + CaretLine(te.col) + L"\r\n";
            }
            return out;
        }

        Compiler comp;
        Chunk chunk = comp.compileMain(*prg);

        VM vm;
        auto vr = vm.run(chunk);

        std::wstring out = vr.output;
        if (vr.hasReturn)
            out += L"(return: " + ValueToString(vr.ret) + L")\r\n";
        return out;
    }
    catch (const LexError& le) {
        std::wstring out = L"LexError line " + std::to_wstring(le.line) + L" col " + std::to_wstring(le.col) +
            L": " + WidenC(le.what()) + L"\r\n";
        std::wstring lineText = GetLineTextAt(source, le.line);
        if (!lineText.empty()) {
            out += L"\r\n> " + lineText + L"\r\n";
            out += L"  " + CaretLine(le.col) + L"\r\n";
        }
        return out;
    }
    catch (const ParseError& pe) {
        std::wstring out = L"ParseError line " + std::to_wstring(pe.line) + L" col " + std::to_wstring(pe.col) +
            L": " + WidenC(pe.what()) + L"\r\n";
        if (!pe.expected.empty()) out += L"Expected: " + JoinExpected(pe.expected) + L"\r\n";
        if (!pe.got.empty()) out += L"Got: " + pe.got + L"\r\n";

        std::wstring lineText = GetLineTextAt(source, pe.line);
        if (!lineText.empty()) {
            out += L"\r\n> " + lineText + L"\r\n";
            out += L"  " + CaretLine(pe.col) + L"\r\n";
        }
        return out;
    }
    catch (const RuntimeError& re) {
        std::wstring out = L"RuntimeError line " + std::to_wstring(re.line) + L" col " + std::to_wstring(re.col) +
            L": " + WidenC(re.what()) + L"\r\n";
        std::wstring lineText = GetLineTextAt(source, re.line);
        if (!lineText.empty()) {
            out += L"\r\n> " + lineText + L"\r\n";
            out += L"  " + CaretLine(re.col) + L"\r\n";
        }
        return out;
    }
    catch (const std::exception& ex) {
        return L"(HATA) " + WidenC(ex.what()) + L"\r\n";
    }
    catch (...) {
        return L"(HATA) bilinmeyen\r\n";
    }
}