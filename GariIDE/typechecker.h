#pragma once
#include "ast.h"
#include "value.h"
#include <string>
#include <vector>
#include <unordered_map>

enum class SType { INT, FLOAT, DOUBLE, STRING, BOOL, ARRAY, STRUCT, MAP, VOID, ANY, INVALID };

struct TypeSlot {
    SType t = SType::INVALID;
    bool isConst = false;
    SType elemType = SType::ANY;     // Dizi veya Sözlük ise eleman/değer tipi
    std::wstring structName;         // Kalıp (struct) ise kalıp adı
};

struct FuncSig {
    SType returnType = SType::INT;
    std::vector<TypeSlot> params;
    size_t minArgs = 0;
};

struct TypeEnv {
    std::vector<std::unordered_map<std::wstring, TypeSlot>> scopes;
    std::unordered_map<std::wstring, FuncSig> funcs;
    std::unordered_map<std::wstring, std::unordered_map<std::wstring, SType>> structs;
    SType currentFuncReturn = SType::ANY;

    TypeEnv() { scopes.push_back({}); }
    void push() { scopes.push_back({}); }
    void pop() { if (scopes.size() > 1) scopes.pop_back(); }
    void declare(const std::wstring& name, SType t, bool isConst = false, SType elemT = SType::ANY, const std::wstring& sName = L"") {
        scopes.back()[name] = TypeSlot{ t, isConst, elemT, sName };
    }
    TypeSlot getSlot(const std::wstring& n) const;
    bool assignable(SType to, SType from) const;
};

const wchar_t* STypeName(SType t);
SType TypeOfExpr(const Expr* e, TypeEnv& tenv);
void TypeCheckStmt(const Stmt* s, TypeEnv& tenv);
void TypeCheckBlock(const BlockStmt* b, TypeEnv& tenv);
void TypeCheckProgram(const Program& p, TypeEnv& tenv);