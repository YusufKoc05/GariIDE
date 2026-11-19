#pragma once
#include "value.h"
#include "ast.h"
#include <vector>
#include <unordered_map>
#include <string>
#include <sstream>

struct Slot { Value v; bool isConst = false; };
struct Env {
    std::vector<std::unordered_map<std::wstring, Slot>> scopes;
    const Program* prog = nullptr;        // 🌟 YENİ: Kullanıcı fonksiyonları ve kalıplarına erişim
    std::wstringstream* outStream = nullptr; // 🌟 YENİ: Fonksiyon içi doktur() çıktıları için
    int callDepth = 0;                    // 🌟 YENİ: Sonsuz özyineleme (stack overflow) koruması

    Env() { scopes.push_back({}); }
    void push() { scopes.push_back({}); }
    void pop() { if (scopes.size() > 1) scopes.pop_back(); }
    void declare(const std::wstring& name, const Value& v, bool isConst = false) { scopes.back()[name] = Slot{ v, isConst }; }
    bool assign(const std::wstring& name, const Value& v, int line, int col);
    Value get(const std::wstring& name, int line, int col) const;
    Slot* findSlot(const std::wstring& name);
};

enum class Op : int {
    NOP = 0, PUSH_CONST, POP, ENTER_SCOPE, EXIT_SCOPE, LOAD, DECLARE, STORE,
    TOBOOL, ADD, SUB, MUL, DIV, MOD, EQ, NE, LT, GT, LE, GE, JMP, JMPF, JMPT, PRINT, CALL_BUILTIN, RETURN,
    NOT, BREAK, CONTINUE,
    NEW_ARRAY, NEW_ARRAY_LIST, LOAD_ELEMENT, STORE_ELEMENT,
    NEW_STRUCT, LOAD_FIELD, STORE_FIELD,
    NEW_MAP,
    TRY_BEGIN, TRY_END, THROW_ERR
};

struct Instr { Op op = Op::NOP; int a = 0; int b = 0; int line = 1; int col = 1; };
struct Chunk { std::vector<Instr> code; std::vector<Value> consts; };

int AddConst(Chunk& ch, const Value& v);
std::wstring OpName(Op op);

int BuiltinIdFromName(const std::wstring& name);
std::wstring ValueTypeName(Value::Type t);

std::wstring DisasmChunk(const Chunk& ch);

struct Sym { int slot = -1; bool isConst = false; Value::Type declared = Value::Type::INT; };

struct LoopCompileState { std::vector<int> breakJumps; std::vector<int> continueJumps; };

struct Compiler {
    Chunk ch; std::vector<std::unordered_map<std::wstring, Sym>> scopes;
    std::vector<int> scopeSlotBase; int nextSlot = 0;
    std::vector<LoopCompileState> loopStack;
    Compiler() { scopes.push_back({}); scopeSlotBase.push_back(0); }

    void emit(Op op, int a = 0, int b = 0, int line = 1, int col = 1) { ch.code.push_back(Instr{ op, a, b, line, col }); }
    int emitJump(Op op, int line, int col);
    void patchJump(int at, int target);
    void enterScope(int line, int col);
    void exitScope(int line, int col);
    Sym* resolve(const std::wstring& name);
    int declareSym(const std::wstring& name, Value::Type t, bool isConst, int line, int col);

    void compileExpr(const Expr* e);
    void compileStmt(const Stmt* s);
    Chunk compileMain(const Program& p);
};

struct VM {
    const Chunk* ch = nullptr;
    const Program* prog = nullptr; // Kullanıcı tanımlı fonksiyonlara erişim için
    std::vector<Value> stack;
    std::vector<Value> locals;
    std::vector<Value::Type> localType;
    std::vector<bool> localConst;
    std::vector<int> scopeBases;
    std::wstringstream out;

    struct TryHandler {
        int catchIp = 0;
        size_t stackDepth = 0;
        size_t scopeDepth = 0;
    };
    std::vector<TryHandler> tryStack;

    void ensureSlot(int slot);
    Value pop(); void push(const Value& v);
    void doBinary(Op op, int line, int col);

    struct Result { Value ret; bool hasReturn = false; std::wstring output; };
    Result run(const Chunk& chunk, const Program* program); // İmza güncellendi
};

struct ExecResult {
    enum class Flow { Normal, Return, Break, Continue } flow = Flow::Normal;
    Value returnValue;
    static ExecResult None() { return {}; }
    static ExecResult Ret(const Value& v) { ExecResult r; r.flow = Flow::Return; r.returnValue = v; return r; }
    static ExecResult Brk() { ExecResult r; r.flow = Flow::Break; return r; }
    static ExecResult Cnt() { ExecResult r; r.flow = Flow::Continue; return r; }
};

ExecResult ExecStmt(const Stmt* s, Env& env, std::wstringstream& out);
ExecResult ExecBlockWithOut(const BlockStmt* b, Env& env, std::wstringstream& out);

inline std::wstring DisasmProgram(const Program& p) { Compiler c; Chunk ch = c.compileMain(p); return DisasmChunk(ch); }