#pragma once
#include "value.h"
#include "lexer.h"
#include <memory>
#include <vector>
#include <sstream>

struct Env;
struct Program;

struct Expr {
    int line = 1; int col = 1;
    virtual ~Expr() = default;
    virtual Value eval(Env& env) const = 0;
    virtual void dump(std::wstringstream& ss, int indent) const = 0;
};
using ExprPtr = std::unique_ptr<Expr>;

struct Stmt {
    int line = 1; int col = 1;
    virtual ~Stmt() = default;
    virtual Value eval(Env& env) const = 0;
    virtual void dump(std::wstringstream& ss, int indent) const = 0;
};
using StmtPtr = std::unique_ptr<Stmt>;

void Indent(std::wstringstream& ss, int n);

struct IntExpr : Expr {
    int v; explicit IntExpr(int x) : v(x) {}
    Value eval(Env&) const override;
    void dump(std::wstringstream& ss, int indent) const override;
};

struct DoubleExpr : Expr {
    double v; explicit DoubleExpr(double x) : v(x) {}
    Value eval(Env&) const override;
    void dump(std::wstringstream& ss, int indent) const override;
};

struct StrExpr : Expr {
    std::wstring v; explicit StrExpr(std::wstring s) : v(std::move(s)) {}
    Value eval(Env&) const override;
    void dump(std::wstringstream& ss, int indent) const override;
};

struct BoolExpr : Expr {
    bool v; explicit BoolExpr(bool x) : v(x) {}
    Value eval(Env&) const override;
    void dump(std::wstringstream& ss, int indent) const override;
};

struct ArrayAccessExpr : Expr {
    std::wstring name;
    ExprPtr index;
    ArrayAccessExpr(std::wstring n, ExprPtr idx) : name(std::move(n)), index(std::move(idx)) {}
    Value eval(Env& env) const override;
    void dump(std::wstringstream& ss, int indent) const override;
};

// 🌟 YENİ: kalip_gari (struct) alan erişimi: nesne.alan
struct FieldAccessExpr : Expr {
    std::wstring objName;
    std::wstring fieldName;
    FieldAccessExpr(std::wstring o, std::wstring f) : objName(std::move(o)), fieldName(std::move(f)) {}
    Value eval(Env& env) const override;
    void dump(std::wstringstream& ss, int indent) const override;
};

struct VarExpr : Expr {
    std::wstring name; explicit VarExpr(std::wstring n) : name(std::move(n)) {}
    Value eval(Env& env) const override;
    void dump(std::wstringstream& ss, int indent) const override;
};

struct CallExpr : Expr {
    std::wstring name; std::vector<ExprPtr> args;
    CallExpr(std::wstring n, std::vector<ExprPtr> a) : name(std::move(n)), args(std::move(a)) {}
    Value eval(Env& env) const override;
    void dump(std::wstringstream& ss, int indent) const override;
};

struct NotExpr : Expr {
    ExprPtr expr;
    explicit NotExpr(ExprPtr e) : expr(std::move(e)) {}
    Value eval(Env& env) const override;
    void dump(std::wstringstream& ss, int indent) const override;
};

struct BlockStmt : Stmt {
    std::vector<StmtPtr> stmts;
    Value eval(Env& env) const override;
    void dump(std::wstringstream& ss, int indent) const override;
};

// 🌟 YENİ: kalip_gari (struct) tanımlama düğümü
struct StructFieldDef {
    std::wstring name;
    Value::Type type = Value::Type::INT;
};

struct StructDeclStmt : Stmt {
    std::wstring name;
    std::vector<StructFieldDef> fields;
    StructDeclStmt(std::wstring n, std::vector<StructFieldDef> f)
        : name(std::move(n)), fields(std::move(f)) {}
    Value eval(Env& env) const override;
    void dump(std::wstringstream& ss, int indent) const override;
};

// 🌟 YENİ: kalip_gari (struct) değişken tanımlama düğümü (örn: Zeybek efe;)
struct StructVarDeclStmt : Stmt {
    std::wstring structName;
    std::wstring varName;
    StructVarDeclStmt(std::wstring sName, std::wstring vName)
        : structName(std::move(sName)), varName(std::move(vName)) {}
    Value eval(Env& env) const override;
    void dump(std::wstringstream& ss, int indent) const override;
};

// 🌟 YENİ: kalip_gari (struct) alanına atama düğümü (örn: efe.isim = "Yusuf";)
struct FieldAssignStmt : Stmt {
    std::wstring objName;
    std::wstring fieldName;
    ExprPtr value;
    FieldAssignStmt(std::wstring o, std::wstring f, ExprPtr v)
        : objName(std::move(o)), fieldName(std::move(f)), value(std::move(v)) {}
    Value eval(Env& env) const override;
    void dump(std::wstringstream& ss, int indent) const override;
};

// 🌟 Kullanıcı Tanımlı Fonksiyon Tanımlama Komutu Düğümü
struct FuncParam {
    std::wstring name;
    Value::Type type = Value::Type::INT;
    Value::Type elemType = Value::Type::INT; // Dizi veya sözlük parametresi ise eleman tipi
    ExprPtr defaultVal = nullptr;            // 🌟 YENİ: Varsayılan parametre değeri (örn: tamsayi derece = 2)
};

// 🌟 YENİ: Üçlü Koşul Operatörü (kosul ? dogru_ifade : yanlis_ifade)
struct TernaryExpr : Expr {
    ExprPtr cond;
    ExprPtr thenE;
    ExprPtr elseE;
    TernaryExpr(ExprPtr c, ExprPtr t, ExprPtr e)
        : cond(std::move(c)), thenE(std::move(t)), elseE(std::move(e)) {}
    Value eval(Env& env) const override;
    void dump(std::wstringstream& ss, int indent) const override;
};

struct FuncDeclStmt : Stmt {
    std::wstring name;
    Value::Type returnType = Value::Type::INT;
    std::vector<FuncParam> params;
    std::unique_ptr<BlockStmt> body;

    FuncDeclStmt(std::wstring n, Value::Type retT,
        std::vector<FuncParam> p,
        std::unique_ptr<BlockStmt> b)
        : name(std::move(n)), returnType(retT), params(std::move(p)), body(std::move(b)) {
    }

    Value eval(Env& env) const override;
    void dump(std::wstringstream& ss, int indent) const override;
};

struct VarDeclStmt : Stmt {
    std::wstring name; Value::Type declaredType = Value::Type::INT;
    ExprPtr init; bool isConst = false;
    VarDeclStmt(std::wstring n, Value::Type t, ExprPtr e, bool c) : name(std::move(n)), declaredType(t), init(std::move(e)), isConst(c) {}
    Value eval(Env& env) const override;
    void dump(std::wstringstream& ss, int indent) const override;
};

struct Program {
    std::vector<std::unique_ptr<StructDeclStmt>> structs; // Küresel kalip_gari tanımları
    std::vector<std::unique_ptr<VarDeclStmt>> globals;    // 🌟 YENİ: Küresel değişkenler ve sabitler
    std::vector<std::unique_ptr<FuncDeclStmt>> functions; // Küresel tanımlanan fonksiyonlar listesi
    std::unique_ptr<BlockStmt> main;
    void dump(std::wstringstream& ss) const;
};

enum class BinOp { Add, Sub, Mul, Div, Mod, Eq, Ne, Lt, Gt, Le, Ge, AndAnd, OrOr };

struct BinExpr : Expr {
    BinOp op; ExprPtr a, b;
    BinExpr(BinOp o, ExprPtr l, ExprPtr r) : op(o), a(std::move(l)), b(std::move(r)) {}
    Value eval(Env& env) const override;
    void dump(std::wstringstream& ss, int indent) const override;
};

struct ArrayDeclStmt : Stmt {
    std::wstring name;
    Value::Type elementType = Value::Type::INT;
    ExprPtr sizeExpr;                  // dizi_gari tamsayi a[5];
    std::vector<ExprPtr> initElements; // dizi_gari tamsayi a = [10, 20, 30];
    ArrayDeclStmt(std::wstring n, Value::Type t, ExprPtr sizeE, std::vector<ExprPtr> inits = {})
        : name(std::move(n)), elementType(t), sizeExpr(std::move(sizeE)), initElements(std::move(inits)) {
    }
    Value eval(Env& env) const override;
    void dump(std::wstringstream& ss, int indent) const override;
};

// 🌟 YENİ: sozluk_gari (Map / Dictionary) tanımlama komutu
// Örn: sozluk_gari tamsayi plakalar = ["Aydin": 9, "Izmir": 35];
struct MapDeclStmt : Stmt {
    std::wstring name;
    Value::Type valueType = Value::Type::INT;
    std::vector<std::pair<ExprPtr, ExprPtr>> initEntries;
    MapDeclStmt(std::wstring n, Value::Type vt, std::vector<std::pair<ExprPtr, ExprPtr>> entries = {})
        : name(std::move(n)), valueType(vt), initEntries(std::move(entries)) {}
    Value eval(Env& env) const override;
    void dump(std::wstringstream& ss, int indent) const override;
};

struct AssignStmt : Stmt {
    std::wstring name; ExprPtr value;
    AssignStmt(std::wstring n, ExprPtr v) : name(std::move(n)), value(std::move(v)) {}
    Value eval(Env& env) const override;
    void dump(std::wstringstream& ss, int indent) const override;
};

struct ArrayAssignStmt : Stmt {
    std::wstring name;
    ExprPtr index;
    ExprPtr value;
    ArrayAssignStmt(std::wstring n, ExprPtr idx, ExprPtr val)
        : name(std::move(n)), index(std::move(idx)), value(std::move(val)) {
    }
    Value eval(Env& env) const override;
    void dump(std::wstringstream& ss, int indent) const override;
};

// 🌟 YENİ: Tek başına ifade / fonksiyon çağrısı komutu (örn: selamla_gari(); veya ekle_gari(d, 5);)
struct ExprStmt : Stmt {
    ExprPtr expr;
    explicit ExprStmt(ExprPtr e) : expr(std::move(e)) {}
    Value eval(Env& env) const override;
    void dump(std::wstringstream& ss, int indent) const override;
};

struct PrintStmt : Stmt {
    ExprPtr expr; explicit PrintStmt(ExprPtr e) : expr(std::move(e)) {}
    Value eval(Env& env) const override;
    void dump(std::wstringstream& ss, int indent) const override;
};

struct IfStmt : Stmt {
    ExprPtr cond; StmtPtr thenS; StmtPtr elseS;
    IfStmt(ExprPtr c, StmtPtr t, StmtPtr e) : cond(std::move(c)), thenS(std::move(t)), elseS(std::move(e)) {}
    Value eval(Env& env) const override;
    void dump(std::wstringstream& ss, int indent) const override;
};

// 🌟 YENİ: bak_hele / olursa / hicbiri (Switch-Case) komutu
struct SwitchCaseBranch {
    ExprPtr matchValue; // nullptr ise "hicbiri:" (default)
    std::vector<StmtPtr> stmts;
};

struct SwitchStmt : Stmt {
    ExprPtr target;
    std::vector<SwitchCaseBranch> branches;
    SwitchStmt(ExprPtr t, std::vector<SwitchCaseBranch> b)
        : target(std::move(t)), branches(std::move(b)) {}
    Value eval(Env& env) const override;
    void dump(std::wstringstream& ss, int indent) const override;
};

struct WhiteStmt : Stmt {
    ExprPtr cond; StmtPtr body;
    WhiteStmt(ExprPtr c, StmtPtr b) : cond(std::move(c)), body(std::move(b)) {}
    Value eval(Env& env) const override;
    void dump(std::wstringstream& ss, int indent) const override;
};

struct DoWhileStmt : Stmt {
    StmtPtr body; ExprPtr cond;
    DoWhileStmt(StmtPtr b, ExprPtr c) : body(std::move(b)), cond(std::move(c)) {}
    Value eval(Env& env) const override;
    void dump(std::wstringstream& ss, int indent) const override;
};

struct ForStmt : Stmt {
    StmtPtr init; ExprPtr cond; StmtPtr step; StmtPtr body;
    ForStmt(StmtPtr i, ExprPtr c, StmtPtr s, StmtPtr b) : init(std::move(i)), cond(std::move(c)), step(std::move(s)), body(std::move(b)) {}
    Value eval(Env& env) const override;
    void dump(std::wstringstream& ss, int indent) const override;
};

// 🌟 YENİ: gez_gari (tamsayi x : sayilar) { ... } (Foreach döngüsü)
struct ForeachStmt : Stmt {
    std::wstring varName;
    Value::Type varType = Value::Type::INT;
    ExprPtr iterable;
    StmtPtr body;
    ForeachStmt(std::wstring vn, Value::Type vt, ExprPtr it, StmtPtr b)
        : varName(std::move(vn)), varType(vt), iterable(std::move(it)), body(std::move(b)) {}
    Value eval(Env& env) const override;
    void dump(std::wstringstream& ss, int indent) const override;
};

// 🌟 YENİ: dene_gari { ... } yakala_gari (laf hata) { ... } (Try-Catch)
struct TryCatchStmt : Stmt {
    StmtPtr tryBody;
    std::wstring errVarName;
    StmtPtr catchBody;
    TryCatchStmt(StmtPtr tb, std::wstring ev, StmtPtr cb)
        : tryBody(std::move(tb)), errVarName(std::move(ev)), catchBody(std::move(cb)) {}
    Value eval(Env& env) const override;
    void dump(std::wstringstream& ss, int indent) const override;
};

// 🌟 YENİ: firlat_gari("Mesaj"); (Throw)
struct ThrowStmt : Stmt {
    ExprPtr expr;
    explicit ThrowStmt(ExprPtr e) : expr(std::move(e)) {}
    Value eval(Env& env) const override;
    void dump(std::wstringstream& ss, int indent) const override;
};

struct ReturnStmt : Stmt {
    ExprPtr value; explicit ReturnStmt(ExprPtr v) : value(std::move(v)) {}
    Value eval(Env& env) const override;
    void dump(std::wstringstream& ss, int indent) const override;
};

struct BreakStmt : Stmt {
    Value eval(Env& env) const override;
    void dump(std::wstringstream& ss, int indent) const override;
};

struct ContinueStmt : Stmt {
    Value eval(Env& env) const override;
    void dump(std::wstringstream& ss, int indent) const override;
};

struct Parser {
    std::vector<Token> toks; size_t i = 0;
    std::vector<std::wstring> knownStructs; // Tanımlanan kalip_gari isimleri
    explicit Parser(std::vector<Token> t) : toks(std::move(t)) {}

    const Token& peek() const { static Token dummy{ TokType::End, L"", 1, 1 }; return (i < toks.size()) ? toks[i] : dummy; }
    const Token& prev() const { static Token dummy{ TokType::End, L"", 1, 1 }; return (i == 0) ? dummy : toks[i - 1]; }
    bool isAtEnd() const { return peek().type == TokType::End; }
    bool check(TokType t) const { return peek().type == t; }
    bool match(TokType t) { if (check(t)) { i++; return true; } return false; }
    const Token& consume(TokType t, const char* msg);
    bool isKnownStruct(const std::wstring& name) const;

    ExprPtr parseExpr(); ExprPtr parseTernary(); ExprPtr parseLogicOr(); ExprPtr parseLogicAnd();
    ExprPtr parseEquality(); ExprPtr parseRelational(); ExprPtr parseAdditive();
    ExprPtr parseTerm(); ExprPtr parseFactor(); ExprPtr parseUnary(); ExprPtr parsePrimary();

    StmtPtr parseBlockAfterLBrace(); StmtPtr parsePrint(); StmtPtr parseReturn();
    StmtPtr parseIf(); StmtPtr parseSwitch(); StmtPtr parseWhile(); StmtPtr parseDoWhile(); StmtPtr parseVarDecl();
    StmtPtr parseArrayDecl(); StmtPtr parseMapDecl();
    StmtPtr parseIdentifierLedStmt(const Token& nameTok, bool requireSemicolon = true);
    StmtPtr parseSimpleAssignNoSemicolon(); StmtPtr parseAssign(); StmtPtr parseFor();
    StmtPtr parseForeach(); StmtPtr parseTryCatch(); StmtPtr parseThrow();
    StmtPtr parseStmt();
    std::unique_ptr<StructDeclStmt> parseStructDecl();
    std::unique_ptr<FuncDeclStmt> parseFuncDecl();
    std::unique_ptr<Program> parseProgram();
};

std::wstring AstToText(const Program& p);