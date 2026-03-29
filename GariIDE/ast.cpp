#include "ast.h"
#include "value.h"
#include "env.h"
#include "runtime.h"

// ---- Expr eval/dump impl ----
Value IntExpr::eval(Env&) const { return Value::makeInt(v); }
void IntExpr::dump(std::wstringstream& ss, int indent) const {
    Indent(ss, indent); ss << L"Int(" << v << L")\n";
}

Value DoubleExpr::eval(Env&) const { return Value::makeDouble(v); }
void DoubleExpr::dump(std::wstringstream& ss, int indent) const {
    Indent(ss, indent); ss << L"Double(" << v << L")\n";
}

Value StrExpr::eval(Env&) const { return Value::makeString(v); }
void StrExpr::dump(std::wstringstream& ss, int indent) const {
    Indent(ss, indent); ss << L"Str(\"" << v << L"\")\n";
}

Value BoolExpr::eval(Env&) const { return Value::makeBool(v); }
void BoolExpr::dump(std::wstringstream& ss, int indent) const {
    Indent(ss, indent); ss << L"Bool(" << (v ? L"dogru" : L"yanlis") << L")\n";
}

Value NullExpr::eval(Env&) const { return Value::makeNull(); }
void NullExpr::dump(std::wstringstream& ss, int indent) const {
    Indent(ss, indent); ss << L"Null\n";
}

Value VarExpr::eval(Env& env) const { return env.get(name, line, col); }
void VarExpr::dump(std::wstringstream& ss, int indent) const {
    Indent(ss, indent); ss << L"Var(" << name << L")\n";
}

Value ArrayExpr::eval(Env& env) const {
    std::vector<Value> v;
    v.reserve(elems.size());
    for (auto& e : elems) v.push_back(e->eval(env));
    return Value::makeArray(std::move(v));
}
void ArrayExpr::dump(std::wstringstream& ss, int indent) const {
    Indent(ss, indent); ss << L"Array\n";
    for (auto& e : elems) e->dump(ss, indent + 1);
}

Value IndexExpr::eval(Env& env) const {
    Value b = base->eval(env);
    int i = ToInt(index->eval(env));
    if (b.type != Value::Type::ARRAY)
        throw RuntimeError(line, col, "TypeError", "Index islemi sadece dizide kullanilir");
    if (i < 0 || (size_t)i >= b.arr.size())
        throw RuntimeError(line, col, "IndexError", "Dizi indeksi disarida");
    return b.arr[(size_t)i];
}
void IndexExpr::dump(std::wstringstream& ss, int indent) const {
    Indent(ss, indent); ss << L"Index\n";
    base->dump(ss, indent + 1);
    index->dump(ss, indent + 1);
}

Value LenExpr::eval(Env& env) const {
    Value v = target->eval(env);
    if (v.type == Value::Type::ARRAY) return Value::makeInt((int)v.arr.size());
    if (v.type == Value::Type::STRING) return Value::makeInt((int)v.s.size());
    return Value::makeInt(0);
}
void LenExpr::dump(std::wstringstream& ss, int indent) const {
    Indent(ss, indent); ss << L"Len\n";
    target->dump(ss, indent + 1);
}

// ---- Block dump ----
void BlockStmt::dump(std::wstringstream& ss, int indent) const {
    Indent(ss, indent); ss << L"Block\n";
    for (auto& s : stmts) s->dump(ss, indent + 1);
}

// ---- Program dump (BURASI AST.H’DEN TAÞINDI) ----
void Program::dump(std::wstringstream& ss) const {
    ss << L"Program\n";
    ss << L"Functions:\n";
    for (auto& f : funcs) {
        ss << L"  Func " << f->name << L"(";
        for (size_t i = 0; i < f->params.size(); ++i) {
            if (i) ss << L", ";
            ss << f->params[i].name;
        }
        ss << L")\n";
        f->body->dump(ss, 2);
    }
    ss << L"Main:\n";
    main->dump(ss, 1);
}