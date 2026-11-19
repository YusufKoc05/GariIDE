#include "ast.h"
#include "vm.h" 
#include "core.h"
#include <cmath>
#include <algorithm>

void Indent(std::wstringstream& ss, int n) { for (int i = 0; i < n; i++) ss << L"  "; }

Value IntExpr::eval(Env&) const { return Value::makeInt(v); }
void IntExpr::dump(std::wstringstream& ss, int indent) const { Indent(ss, indent); ss << L"Int(" << v << L")\n"; }

Value DoubleExpr::eval(Env&) const { return Value::makeDouble(v); }
void DoubleExpr::dump(std::wstringstream& ss, int indent) const { Indent(ss, indent); ss << L"Double(" << v << L")\n"; }

Value StrExpr::eval(Env&) const { return Value::makeString(v); }
void StrExpr::dump(std::wstringstream& ss, int indent) const { Indent(ss, indent); ss << L"Str(\"" << v << L"\")\n"; }

Value BoolExpr::eval(Env&) const { return Value::makeBool(v); }
void BoolExpr::dump(std::wstringstream& ss, int indent) const { Indent(ss, indent); ss << L"Bool(" << (v ? L"hakli" : L"haksiz") << L")\n"; }

Value ArrayAccessExpr::eval(Env& env) const {
    Value arr = env.get(name, line, col);
    if (arr.type == Value::Type::MAP && arr.mapVal) {
        std::wstring key = ValueToString(index->eval(env));
        auto it = arr.mapVal->find(key);
        if (it == arr.mapVal->end()) {
            throw Core_RuntimeError(line, col, "KeyError", Core_WToNarrow(name) + " sozlugunde '" + Core_WToNarrow(key) + "' anahtari bulunamadi.");
        }
        return it->second;
    }
    if (arr.type == Value::Type::STRING) {
        int idx = ToInt(index->eval(env));
        if (idx < 0 || idx >= (int)arr.sVal.size()) {
            throw Core_RuntimeError(line, col, "IndexError", "Laf sinirlari disinda indeks: " + std::to_string(idx));
        }
        return Value::makeString(std::wstring(1, arr.sVal[(size_t)idx]));
    }
    if (arr.type != Value::Type::ARRAY || !arr.arrVal) {
        throw Core_RuntimeError(line, col, "TypeError", Core_WToNarrow(name) + " bir dizi_gari veya sozluk_gari degil.");
    }
    int idx = ToInt(index->eval(env));
    if (idx < 0 || idx >= (int)arr.arrVal->size()) {
        throw Core_RuntimeError(line, col, "IndexError", "Dizi sinirlari disinda indeks: " + std::to_string(idx));
    }
    return (*arr.arrVal)[(size_t)idx];
}
void ArrayAccessExpr::dump(std::wstringstream& ss, int indent) const {
    Indent(ss, indent); ss << L"ArrayAccess(" << name << L")\n";
    index->dump(ss, indent + 1);
}

Value FieldAccessExpr::eval(Env& env) const {
    Value obj = env.get(objName, line, col);
    if (obj.type != Value::Type::STRUCT || !obj.fields) {
        throw Core_RuntimeError(line, col, "TypeError", Core_WToNarrow(objName) + " bir kalip_gari nesnesi degil.");
    }
    auto it = obj.fields->find(fieldName);
    if (it == obj.fields->end()) {
        throw Core_RuntimeError(line, col, "NameError", Core_WToNarrow(obj.structName) + " kalibinda '" + Core_WToNarrow(fieldName) + "' alani yok.");
    }
    return it->second;
}
void FieldAccessExpr::dump(std::wstringstream& ss, int indent) const {
    Indent(ss, indent); ss << L"FieldAccess(" << objName << L"." << fieldName << L")\n";
}

Value VarExpr::eval(Env& env) const { return env.get(name, line, col); }
void VarExpr::dump(std::wstringstream& ss, int indent) const { Indent(ss, indent); ss << L"Var(" << name << L")\n"; }

Value CallExpr::eval(Env& env) const {
    std::vector<Value> vals; vals.reserve(args.size());
    for (auto& a : args) vals.push_back(a->eval(env));

    // Önce kullanıcı tanımlı fonksiyonlarda ara (iç içe çağrı ve recursion desteği)
    if (env.prog) {
        const FuncDeclStmt* targetFunc = nullptr;
        for (auto& f : env.prog->functions) {
            if (f->name == name) { targetFunc = f.get(); break; }
        }
        if (targetFunc) {
            if (env.callDepth > 500) {
                throw Core_RuntimeError(line, col, "RecursionError", "Cok derin fonksiyon cagrisi (sonsuz ozyineleme): " + Core_WToNarrow(name));
            }
            size_t minArgs = 0;
            for (const auto& prm : targetFunc->params) {
                if (!prm.defaultVal) minArgs++;
            }
            if (vals.size() < minArgs || vals.size() > targetFunc->params.size()) {
                throw Core_RuntimeError(line, col, "TypeError", "Yanlis arguman sayisi: " + Core_WToNarrow(name));
            }
            Env callEnv;
            callEnv.prog = env.prog;
            callEnv.outStream = env.outStream;
            callEnv.callDepth = env.callDepth + 1;
            if (!env.scopes.empty()) {
                callEnv.scopes[0] = env.scopes[0]; // Küresel değişkenleri paylaş
            }
            callEnv.push();

            for (size_t pi = 0; pi < targetFunc->params.size(); ++pi) {
                Value rawVal = (pi < vals.size())
                    ? vals[pi]
                    : (targetFunc->params[pi].defaultVal ? targetFunc->params[pi].defaultVal->eval(callEnv) : Value::makeDefaultForType(targetFunc->params[pi].type));
                Value coerced = Coerce(rawVal, targetFunc->params[pi].type);
                callEnv.declare(targetFunc->params[pi].name, coerced, false);
            }

            std::wstringstream localOut;
            std::wstringstream* outPtr = env.outStream ? env.outStream : &localOut;
            ExecResult er = ExecStmt(targetFunc->body.get(), callEnv, *outPtr);
            if (!env.scopes.empty() && !callEnv.scopes.empty()) {
                for (auto& kv : env.scopes[0]) {
                    auto it = callEnv.scopes[0].find(kv.first);
                    if (it != callEnv.scopes[0].end()) kv.second.v = it->second.v;
                }
            }
            if (er.flow == ExecResult::Flow::Return) {
                if (targetFunc->returnType == Value::Type::VOID) return Value::makeVoid();
                return Coerce(er.returnValue, targetFunc->returnType);
            }
            if (targetFunc->returnType == Value::Type::VOID) return Value::makeVoid();
            return Value::makeDefaultForType(targetFunc->returnType);
        }
    }

    return EvalBuiltinCall(name, vals, line, col);
}
void CallExpr::dump(std::wstringstream& ss, int indent) const {
    Indent(ss, indent); ss << L"Call(" << name << L")\n";
    for (auto& a : args) a->dump(ss, indent + 1);
}

Value NotExpr::eval(Env& env) const {
    Value val = expr->eval(env);
    return Value::makeBool(!IsTrue(val));
}
void NotExpr::dump(std::wstringstream& ss, int indent) const {
    Indent(ss, indent); ss << L"Not(!)\n"; expr->dump(ss, indent + 1);
}

Value BlockStmt::eval(Env& env) const {
    for (auto& s : stmts) {
        s->eval(env);
    }
    return Value::makeInt(0);
}
void BlockStmt::dump(std::wstringstream& ss, int indent) const {
    Indent(ss, indent); ss << L"Block\n";
    for (auto& s : stmts) s->dump(ss, indent + 1);
}

Value StructDeclStmt::eval(Env&) const {
    return Value::makeInt(0);
}
void StructDeclStmt::dump(std::wstringstream& ss, int indent) const {
    Indent(ss, indent); ss << L"StructDecl(" << name << L")\n";
    for (auto& f : fields) {
        Indent(ss, indent + 1); ss << f.name << L" : " << ValueTypeName(f.type) << L"\n";
    }
}

Value StructVarDeclStmt::eval(Env& env) const {
    Value sVal = Value::makeStruct(structName);
    if (env.prog) {
        for (auto& sd : env.prog->structs) {
            if (sd->name == structName) {
                for (auto& f : sd->fields) {
                    (*sVal.fields)[f.name] = Value::makeDefaultForType(f.type);
                }
                break;
            }
        }
    }
    env.declare(varName, sVal, false);
    return sVal;
}
void StructVarDeclStmt::dump(std::wstringstream& ss, int indent) const {
    Indent(ss, indent); ss << L"StructVarDecl(" << structName << L" " << varName << L")\n";
}

Value FieldAssignStmt::eval(Env& env) const {
    Value obj = env.get(objName, line, col);
    if (obj.type != Value::Type::STRUCT || !obj.fields) {
        throw Core_RuntimeError(line, col, "TypeError", Core_WToNarrow(objName) + " bir kalip_gari nesnesi degil.");
    }
    auto it = obj.fields->find(fieldName);
    if (it == obj.fields->end()) {
        throw Core_RuntimeError(line, col, "NameError", Core_WToNarrow(obj.structName) + " kalibinda '" + Core_WToNarrow(fieldName) + "' alani yok.");
    }
    Value val = value->eval(env);
    it->second = Coerce(val, it->second.type);
    return it->second;
}
void FieldAssignStmt::dump(std::wstringstream& ss, int indent) const {
    Indent(ss, indent); ss << L"FieldAssign(" << objName << L"." << fieldName << L")\n";
    value->dump(ss, indent + 1);
}

Value FuncDeclStmt::eval(Env&) const {
    return Value::makeInt(0);
}
void FuncDeclStmt::dump(std::wstringstream& ss, int indent) const {
    Indent(ss, indent); ss << L"FuncDecl(" << name << L") -> " << ValueTypeName(returnType) << L"\n";
    Indent(ss, indent + 1); ss << L"Params:\n";
    for (auto& p : params) {
        Indent(ss, indent + 2); ss << p.name << L" (" << ValueTypeName(p.type) << L")\n";
    }
    if (body) body->dump(ss, indent + 1);
}

void Program::dump(std::wstringstream& ss) const {
    ss << L"Program\n";
    if (!structs.empty()) {
        ss << L"Structs:\n";
        for (auto& s : structs) s->dump(ss, 1);
    }
    if (!globals.empty()) {
        ss << L"Globals:\n";
        for (auto& g : globals) g->dump(ss, 1);
    }
    if (!functions.empty()) {
        ss << L"Functions:\n";
        for (auto& f : functions) f->dump(ss, 1);
    }
    ss << L"Main:\n"; main->dump(ss, 1);
}

Value ForeachStmt::eval(Env&) const { return Value::makeInt(0); }
void ForeachStmt::dump(std::wstringstream& ss, int indent) const {
    Indent(ss, indent); ss << L"Foreach(gez_gari " << varName << L" : " << ValueTypeName(varType) << L")\n";
    if (iterable) iterable->dump(ss, indent + 1);
    if (body) body->dump(ss, indent + 1);
}

Value TryCatchStmt::eval(Env&) const { return Value::makeInt(0); }
void TryCatchStmt::dump(std::wstringstream& ss, int indent) const {
    Indent(ss, indent); ss << L"TryCatch(dene_gari / yakala_gari " << errVarName << L")\n";
    if (tryBody) tryBody->dump(ss, indent + 1);
    if (catchBody) catchBody->dump(ss, indent + 1);
}

Value ThrowStmt::eval(Env& env) const {
    Value v = expr ? expr->eval(env) : Value::makeString(L"Bilinmeyen hata");
    throw Core_RuntimeError(line, col, "UserError", Core_WToNarrow(ValueToString(v)));
}
void ThrowStmt::dump(std::wstringstream& ss, int indent) const {
    Indent(ss, indent); ss << L"Throw(firlat_gari)\n";
    if (expr) expr->dump(ss, indent + 1);
}

Value BreakStmt::eval(Env&) const { return Value::makeInt(0); }
void BreakStmt::dump(std::wstringstream& ss, int indent) const { Indent(ss, indent); ss << L"Break\n"; }

Value ContinueStmt::eval(Env&) const { return Value::makeInt(0); }
void ContinueStmt::dump(std::wstringstream& ss, int indent) const { Indent(ss, indent); ss << L"Continue\n"; }

Value ExprStmt::eval(Env& env) const { return expr ? expr->eval(env) : Value::makeInt(0); }
void ExprStmt::dump(std::wstringstream& ss, int indent) const {
    Indent(ss, indent); ss << L"ExprStmt\n";
    if (expr) expr->dump(ss, indent + 1);
}

Value SwitchStmt::eval(Env&) const { return Value::makeInt(0); }
void SwitchStmt::dump(std::wstringstream& ss, int indent) const {
    Indent(ss, indent); ss << L"Switch(bak_hele)\n";
    Indent(ss, indent + 1); ss << L"Target:\n";
    if (target) target->dump(ss, indent + 2);
    for (auto& br : branches) {
        Indent(ss, indent + 1);
        if (br.matchValue) {
            ss << L"Case(olursa):\n";
            br.matchValue->dump(ss, indent + 2);
        } else {
            ss << L"Default(hicbiri):\n";
        }
        for (auto& st : br.stmts) st->dump(ss, indent + 2);
    }
}

Value BinExpr::eval(Env& env) const {
    if (op == BinOp::AndAnd) {
        Value A = a->eval(env); if (!IsTrue(A)) return Value::makeBool(false);
        Value B = b->eval(env); return Value::makeBool(IsTrue(B));
    }
    if (op == BinOp::OrOr) {
        Value A = a->eval(env); if (IsTrue(A)) return Value::makeBool(true);
        Value B = b->eval(env); return Value::makeBool(IsTrue(B));
    }
    Value A = a->eval(env); Value B = b->eval(env);
    if (op == BinOp::Eq || op == BinOp::Ne || op == BinOp::Lt || op == BinOp::Gt || op == BinOp::Le || op == BinOp::Ge) {
        bool res = false;
        if (op == BinOp::Eq) res = ValueEquals(A, B);
        else if (op == BinOp::Ne) res = !ValueEquals(A, B);
        else {
            if (A.type == Value::Type::STRING || B.type == Value::Type::STRING) {
                std::wstring x = ValueToString(A), y = ValueToString(B);
                if (op == BinOp::Lt) res = (x < y);   if (op == BinOp::Gt) res = (x > y);
                if (op == BinOp::Le) res = (x <= y);  if (op == BinOp::Ge) res = (x >= y);
            }
            else {
                double x = ToDouble(A), y = ToDouble(B);
                if (op == BinOp::Lt) res = (x < y);   if (op == BinOp::Gt) res = (x > y);
                if (op == BinOp::Le) res = (x <= y);  if (op == BinOp::Ge) res = (x >= y);
            }
        }
        return Value::makeBool(res);
    }
    if (op == BinOp::Add && (A.type == Value::Type::STRING || B.type == Value::Type::STRING))
        return Value::makeString(ValueToString(A) + ValueToString(B));
    if (op == BinOp::Mod && (!IsNumeric(A.type) || !IsNumeric(B.type)))
        throw Core_RuntimeError(line, col, "TypeError", "'%' sadece sayilarda kullanilir");

    if (A.type == Value::Type::DOUBLE || B.type == Value::Type::DOUBLE) {
        double x = ToDouble(A), y = ToDouble(B);
        if (op == BinOp::Add) return Value::makeDouble(x + y); if (op == BinOp::Sub) return Value::makeDouble(x - y);
        if (op == BinOp::Mul) return Value::makeDouble(x * y);
        if (op == BinOp::Div) { if (y == 0.0) throw Core_RuntimeError(line, col, "ZeroDivision", ZeroDivMsg("/", A, B)); return Value::makeDouble(x / y); }
        if (op == BinOp::Mod) { if (y == 0.0) throw Core_RuntimeError(line, col, "ZeroDivision", ZeroDivMsg("%", A, B)); return Value::makeDouble(std::fmod(x, y)); }
        return Value::makeDouble(0.0);
    }
    if (A.type == Value::Type::FLOAT || B.type == Value::Type::FLOAT) {
        float x = ToFloat(A), y = ToFloat(B);
        if (op == BinOp::Add) return Value::makeFloat(x + y); if (op == BinOp::Sub) return Value::makeFloat(x - y);
        if (op == BinOp::Mul) return Value::makeFloat(x * y);
        if (op == BinOp::Div) { if (y == 0.0f) throw Core_RuntimeError(line, col, "ZeroDivision", ZeroDivMsg("/", A, B)); return Value::makeFloat(x / y); }
        if (op == BinOp::Mod) { if (y == 0.0f) throw Core_RuntimeError(line, col, "ZeroDivision", ZeroDivMsg("%", A, B)); return Value::makeFloat(std::fmod(x, y)); }
        return Value::makeFloat(0.0f);
    }
    int x = ToInt(A), y = ToInt(B);
    if (op == BinOp::Add) return Value::makeInt(x + y); if (op == BinOp::Sub) return Value::makeInt(x - y);
    if (op == BinOp::Mul) return Value::makeInt(x * y);
    if (op == BinOp::Div) { if (y == 0) throw Core_RuntimeError(line, col, "ZeroDivision", ZeroDivMsg("/", A, B)); return Value::makeInt(x / y); }
    if (op == BinOp::Mod) { if (y == 0) throw Core_RuntimeError(line, col, "ZeroDivision", ZeroDivMsg("%", A, B)); return Value::makeInt(x % y); }
    return Value::makeInt(0);
}
void BinExpr::dump(std::wstringstream& ss, int indent) const {
    Indent(ss, indent); ss << L"Bin(" << (int)op << L")\n"; a->dump(ss, indent + 1); b->dump(ss, indent + 1);
}

Value VarDeclStmt::eval(Env& env) const {
    Value v = init ? init->eval(env) : Value::makeInt(0);
    v = Coerce(v, declaredType);
    env.declare(name, v, isConst);
    return v;
}
void VarDeclStmt::dump(std::wstringstream& ss, int indent) const {
    Indent(ss, indent); ss << L"VarDecl(" << name << L")" << (isConst ? L" [sabit]" : L"") << L"\n"; if (init) init->dump(ss, indent + 1);
}

Value ArrayDeclStmt::eval(Env& env) const {
    if (!initElements.empty() || !sizeExpr) {
        std::vector<Value> items;
        items.reserve(initElements.size());
        for (auto& e : initElements) {
            items.push_back(Coerce(e->eval(env), elementType));
        }
        Value arr = Value::makeArrayFromList(std::move(items), elementType);
        env.declare(name, arr, false);
        return arr;
    }
    int size = ToInt(sizeExpr->eval(env));
    if (size <= 0) {
        throw Core_RuntimeError(line, col, "ValueError", "Dizi boyutu pozitif bir tamsayi olmalidir.");
    }
    Value arr = Value::makeArray((size_t)size, elementType);
    env.declare(name, arr, false);
    return arr;
}
void ArrayDeclStmt::dump(std::wstringstream& ss, int indent) const {
    Indent(ss, indent); ss << L"ArrayDecl(" << name << L" : " << ValueTypeName(elementType) << L")\n";
    if (sizeExpr) sizeExpr->dump(ss, indent + 1);
    for (auto& e : initElements) e->dump(ss, indent + 1);
}

Value MapDeclStmt::eval(Env& env) const {
    Value mp = Value::makeMap(valueType);
    for (auto& kv : initEntries) {
        std::wstring k = ValueToString(kv.first->eval(env));
        Value v = Coerce(kv.second->eval(env), valueType);
        (*mp.mapVal)[k] = v;
    }
    env.declare(name, mp, false);
    return mp;
}
void MapDeclStmt::dump(std::wstringstream& ss, int indent) const {
    Indent(ss, indent); ss << L"MapDecl(sozluk_gari " << name << L" : " << ValueTypeName(valueType) << L")\n";
    for (auto& kv : initEntries) {
        kv.first->dump(ss, indent + 1);
        kv.second->dump(ss, indent + 1);
    }
}

Value TernaryExpr::eval(Env& env) const {
    Value c = cond->eval(env);
    return IsTrue(c) ? thenE->eval(env) : elseE->eval(env);
}
void TernaryExpr::dump(std::wstringstream& ss, int indent) const {
    Indent(ss, indent); ss << L"Ternary(?:)\n";
    if (cond) cond->dump(ss, indent + 1);
    if (thenE) thenE->dump(ss, indent + 1);
    if (elseE) elseE->dump(ss, indent + 1);
}

Value AssignStmt::eval(Env& env) const {
    Value v = value->eval(env);
    env.assign(name, v, line, col);
    return v;
}
void AssignStmt::dump(std::wstringstream& ss, int indent) const { Indent(ss, indent); ss << L"Assign(" << name << L")\n"; value->dump(ss, indent + 1); }

Value ArrayAssignStmt::eval(Env& env) const {
    Value arr = env.get(name, line, col);
    if (arr.type == Value::Type::MAP && arr.mapVal) {
        std::wstring key = ValueToString(index->eval(env));
        Value val = Coerce(value->eval(env), arr.elemType);
        (*arr.mapVal)[key] = val;
        return val;
    }
    if (arr.type != Value::Type::ARRAY || !arr.arrVal) {
        throw Core_RuntimeError(line, col, "TypeError", Core_WToNarrow(name) + " bir dizi_gari veya sozluk_gari degil.");
    }
    int idx = ToInt(index->eval(env));
    if (idx < 0 || idx >= (int)arr.arrVal->size()) {
        throw Core_RuntimeError(line, col, "IndexError", "Dizi sinirlari disinda indeks: " + std::to_string(idx));
    }
    Value val = Coerce(value->eval(env), arr.elemType);
    (*arr.arrVal)[(size_t)idx] = val;
    return val;
}
void ArrayAssignStmt::dump(std::wstringstream& ss, int indent) const {
    Indent(ss, indent); ss << L"ArrayAssign(" << name << L")\n";
    index->dump(ss, indent + 1);
    value->dump(ss, indent + 1);
}

Value PrintStmt::eval(Env&) const { return Value::makeInt(0); }
void PrintStmt::dump(std::wstringstream& ss, int indent) const { Indent(ss, indent); ss << L"Print\n"; expr->dump(ss, indent + 1); }

Value IfStmt::eval(Env&) const { return Value::makeInt(0); }
void IfStmt::dump(std::wstringstream& ss, int indent) const {
    Indent(ss, indent); ss << L"If\n"; Indent(ss, indent + 1); ss << L"Cond:\n"; cond->dump(ss, indent + 2);
    Indent(ss, indent + 1); ss << L"Then:\n"; thenS->dump(ss, indent + 2); if (elseS) { Indent(ss, indent + 1); ss << L"Else:\n"; elseS->dump(ss, indent + 2); }
}

Value WhiteStmt::eval(Env&) const { return Value::makeInt(0); }
void WhiteStmt::dump(std::wstringstream& ss, int indent) const {
    Indent(ss, indent); ss << L"While\n"; Indent(ss, indent + 1); ss << L"Cond:\n"; cond->dump(ss, indent + 2); Indent(ss, indent + 1); ss << L"Body:\n"; body->dump(ss, indent + 2);
}

Value DoWhileStmt::eval(Env&) const { return Value::makeInt(0); }
void DoWhileStmt::dump(std::wstringstream& ss, int indent) const {
    Indent(ss, indent); ss << L"DoWhile\n"; Indent(ss, indent + 1); ss << L"Body:\n"; body->dump(ss, indent + 2); Indent(ss, indent + 1); ss << L"Cond:\n"; cond->dump(ss, indent + 2);
}

Value ForStmt::eval(Env&) const { return Value::makeInt(0); }
void ForStmt::dump(std::wstringstream& ss, int indent) const {
    Indent(ss, indent); ss << L"For\n"; if (init) { Indent(ss, indent + 1); ss << L"Init:\n"; init->dump(ss, indent + 2); }
    if (cond) { Indent(ss, indent + 1); ss << L"Cond:\n"; cond->dump(ss, indent + 2); } if (step) { Indent(ss, indent + 1); ss << L"Step:\n"; step->dump(ss, indent + 2); }
    Indent(ss, indent + 1); ss << L"Body:\n"; body->dump(ss, indent + 2);
}

Value ReturnStmt::eval(Env&) const { return Value::makeInt(0); }
void ReturnStmt::dump(std::wstringstream& ss, int indent) const { Indent(ss, indent); ss << L"Return\n"; if (value) value->dump(ss, indent + 1); }

const Token& Parser::consume(TokType t, const char* msg) {
    if (check(t)) return toks[i++];
    throw Core_ParseError(peek().line, peek().col, msg, { std::wstring(TokName(t)) }, peek().lexeme);
}

bool Parser::isKnownStruct(const std::wstring& name) const {
    return std::find(knownStructs.begin(), knownStructs.end(), name) != knownStructs.end();
}

static bool IsPrimitiveTypeTok(TokType t) {
    return t == TokType::KwInt || t == TokType::KwFloat || t == TokType::KwDouble || t == TokType::KwString || t == TokType::KwBool;
}

static Value::Type TokTypeToValueType(TokType t) {
    switch (t) {
    case TokType::KwInt:    return Value::Type::INT;
    case TokType::KwFloat:  return Value::Type::FLOAT;
    case TokType::KwDouble: return Value::Type::DOUBLE;
    case TokType::KwString: return Value::Type::STRING;
    case TokType::KwBool:   return Value::Type::BOOL;
    case TokType::KwVoid:   return Value::Type::VOID;
    default: return Value::Type::INT;
    }
}

ExprPtr Parser::parseExpr() { return parseTernary(); }
ExprPtr Parser::parseTernary() {
    ExprPtr e = parseLogicOr();
    if (match(TokType::Question)) {
        int qLine = prev().line, qCol = prev().col;
        ExprPtr thenE = parseExpr();
        consume(TokType::Colon, "':' bekleniyordu (? operatoru icin)");
        ExprPtr elseE = parseTernary();
        auto n = std::make_unique<TernaryExpr>(std::move(e), std::move(thenE), std::move(elseE));
        n->line = qLine; n->col = qCol;
        return n;
    }
    return e;
}
ExprPtr Parser::parseLogicOr() {
    ExprPtr e = parseLogicAnd();
    while (match(TokType::OrOr)) {
        int opLine = prev().line, opCol = prev().col; auto r = parseLogicAnd();
        auto n = std::make_unique<BinExpr>(BinOp::OrOr, std::move(e), std::move(r)); n->line = opLine; n->col = opCol; e = std::move(n);
    }
    return e;
}
ExprPtr Parser::parseLogicAnd() {
    ExprPtr e = parseEquality();
    while (match(TokType::AndAnd)) {
        int opLine = prev().line, opCol = prev().col; auto r = parseEquality();
        auto n = std::make_unique<BinExpr>(BinOp::AndAnd, std::move(e), std::move(r)); n->line = opLine; n->col = opCol; e = std::move(n);
    }
    return e;
}
ExprPtr Parser::parseEquality() {
    ExprPtr e = parseRelational();
    while (match(TokType::Eq) || match(TokType::Ne)) {
        Token op = prev(); auto r = parseRelational();
        BinOp bo = (op.type == TokType::Eq) ? BinOp::Eq : BinOp::Ne;
        auto n = std::make_unique<BinExpr>(bo, std::move(e), std::move(r)); n->line = op.line; n->col = op.col; e = std::move(n);
    }
    return e;
}
ExprPtr Parser::parseRelational() {
    ExprPtr e = parseAdditive();
    while (peek().type == TokType::Lt || peek().type == TokType::Gt || peek().type == TokType::Le || peek().type == TokType::Ge) {
        Token op = peek(); match(op.type); auto r = parseAdditive();
        BinOp bo = BinOp::Lt;
        if (op.type == TokType::Lt) bo = BinOp::Lt; else if (op.type == TokType::Gt) bo = BinOp::Gt;
        else if (op.type == TokType::Le) bo = BinOp::Le; else if (op.type == TokType::Ge) bo = BinOp::Ge;
        auto n = std::make_unique<BinExpr>(bo, std::move(e), std::move(r)); n->line = op.line; n->col = op.col; e = std::move(n);
    }
    return e;
}
ExprPtr Parser::parseAdditive() {
    ExprPtr e = parseTerm();
    while (match(TokType::Plus) || match(TokType::Minus)) {
        Token op = prev(); auto r = parseTerm(); BinOp bo = (op.type == TokType::Plus) ? BinOp::Add : BinOp::Sub;
        auto n = std::make_unique<BinExpr>(bo, std::move(e), std::move(r)); n->line = op.line; n->col = op.col; e = std::move(n);
    }
    return e;
}
ExprPtr Parser::parseTerm() {
    ExprPtr e = parseFactor();
    while (true) {
        if (match(TokType::Plus)) { int opLine = prev().line, opCol = prev().col; auto r = parseFactor(); auto n = std::make_unique<BinExpr>(BinOp::Add, std::move(e), std::move(r)); n->line = opLine; n->col = opCol; e = std::move(n); continue; }
        if (match(TokType::Minus)) { int opLine = prev().line, opCol = prev().col; auto r = parseFactor(); auto n = std::make_unique<BinExpr>(BinOp::Sub, std::move(e), std::move(r)); n->line = opLine; n->col = opCol; e = std::move(n); continue; }
        break;
    }
    return e;
}
ExprPtr Parser::parseFactor() {
    ExprPtr e = parseUnary();
    while (true) {
        if (match(TokType::Star)) { int opLine = prev().line, opCol = prev().col; auto r = parseUnary(); auto n = std::make_unique<BinExpr>(BinOp::Mul, std::move(e), std::move(r)); n->line = opLine; n->col = opCol; e = std::move(n); continue; }
        if (match(TokType::Slash)) { int opLine = prev().line, opCol = prev().col; auto r = parseUnary(); auto n = std::make_unique<BinExpr>(BinOp::Div, std::move(e), std::move(r)); n->line = opLine; n->col = opCol; e = std::move(n); continue; }
        if (match(TokType::Percent)) { int opLine = prev().line, opCol = prev().col; auto r = parseUnary(); auto n = std::make_unique<BinExpr>(BinOp::Mod, std::move(e), std::move(r)); n->line = opLine; n->col = opCol; e = std::move(n); continue; }
        break;
    }
    return e;
}
ExprPtr Parser::parseUnary() {
    if (match(TokType::Minus)) {
        int ln = prev().line, cl = prev().col; auto r = parseUnary(); auto zero = std::make_unique<IntExpr>(0); zero->line = ln; zero->col = cl;
        auto n = std::make_unique<BinExpr>(BinOp::Sub, std::move(zero), std::move(r)); n->line = ln; n->col = cl; return n;
    }
    if (match(TokType::Bang)) {
        int ln = prev().line, cl = prev().col; auto r = parseUnary();
        auto n = std::make_unique<NotExpr>(std::move(r)); n->line = ln; n->col = cl; return n;
    }
    return parsePrimary();
}
ExprPtr Parser::parsePrimary() {
    if (match(TokType::Number)) {
        int ln = prev().line, cl = prev().col; std::wstring lex = prev().lexeme;
        if (lex.find(L'.') != std::wstring::npos) {
            double d = 0.0; try { d = std::stod(lex); }
            catch (...) {}
            auto n = std::make_unique<DoubleExpr>(d); n->line = ln; n->col = cl; return n;
        }
        else {
            int v = 0; try { v = std::stoi(lex); }
            catch (...) {}
            auto n = std::make_unique<IntExpr>(v); n->line = ln; n->col = cl; return n;
        }
    }
    if (match(TokType::String)) { auto n = std::make_unique<StrExpr>(prev().lexeme); n->line = prev().line; n->col = prev().col; return n; }

    if (match(TokType::KwTrue)) {
        auto n = std::make_unique<BoolExpr>(true);
        n->line = prev().line; n->col = prev().col; return n;
    }
    if (match(TokType::KwFalse)) {
        auto n = std::make_unique<BoolExpr>(false);
        n->line = prev().line; n->col = prev().col; return n;
    }

    if (match(TokType::Identifier)) {
        int ln = prev().line, cl = prev().col; std::wstring name = prev().lexeme;
        if (match(TokType::LParen)) {
            std::vector<ExprPtr> args;
            if (!check(TokType::RParen)) {
                while (true) { args.push_back(parseExpr()); if (match(TokType::Comma)) continue; break; }
            }
            consume(TokType::RParen, "')' bekleniyordu");
            auto call = std::make_unique<CallExpr>(std::move(name), std::move(args)); call->line = ln; call->col = cl; return call;
        }
        if (match(TokType::LBracket)) {
            auto idx = parseExpr();
            consume(TokType::RBracket, "']' bekleniyordu");
            auto acc = std::make_unique<ArrayAccessExpr>(name, std::move(idx));
            acc->line = ln; acc->col = cl; return acc;
        }
        if (match(TokType::Dot)) {
            Token fieldTok = consume(TokType::Identifier, "Alan (field) ismi bekleniyordu");
            auto fa = std::make_unique<FieldAccessExpr>(name, fieldTok.lexeme);
            fa->line = ln; fa->col = cl; return fa;
        }
        auto var = std::make_unique<VarExpr>(std::move(name)); var->line = ln; var->col = cl; return var;
    }
    if (match(TokType::LParen)) { auto e = parseExpr(); consume(TokType::RParen, "')' bekleniyordu"); return e; }
    throw Core_ParseError(peek().line, peek().col, "Ifade bekleniyordu", {}, peek().lexeme);
}

StmtPtr Parser::parseBlockAfterLBrace() {
    int ln = prev().line, cl = prev().col; auto b = std::make_unique<BlockStmt>(); b->line = ln; b->col = cl;
    while (!check(TokType::RBrace) && !isAtEnd()) b->stmts.push_back(parseStmt());
    consume(TokType::RBrace, "'}' bekleniyordu"); return b;
}
StmtPtr Parser::parsePrint() {
    int ln = prev().line, cl = prev().col; consume(TokType::LParen, "'(' bekleniyordu"); auto e = parseExpr();
    consume(TokType::RParen, "')' bekleniyordu"); consume(TokType::Semicolon, "';' bekleniyordu");
    auto n = std::make_unique<PrintStmt>(std::move(e)); n->line = ln; n->col = cl; return n;
}
StmtPtr Parser::parseReturn() {
    int ln = prev().line, cl = prev().col; ExprPtr v = nullptr; if (!check(TokType::Semicolon)) v = parseExpr();
    consume(TokType::Semicolon, "';' bekleniyordu"); auto n = std::make_unique<ReturnStmt>(std::move(v)); n->line = ln; n->col = cl; return n;
}
StmtPtr Parser::parseIf() {
    int ln = prev().line, cl = prev().col; consume(TokType::LParen, "'(' bekleniyordu"); auto cond = parseExpr();
    consume(TokType::RParen, "')' bekleniyordu"); auto thenS = parseStmt(); StmtPtr elseS = nullptr; if (match(TokType::KwElse)) elseS = parseStmt();
    auto n = std::make_unique<IfStmt>(std::move(cond), std::move(thenS), std::move(elseS)); n->line = ln; n->col = cl; return n;
}

StmtPtr Parser::parseSwitch() {
    int ln = prev().line, cl = prev().col;
    consume(TokType::LParen, "'(' bekleniyordu");
    auto target = parseExpr();
    consume(TokType::RParen, "')' bekleniyordu");
    consume(TokType::LBrace, "'{' bekleniyordu");

    std::vector<SwitchCaseBranch> branches;
    while (!check(TokType::RBrace) && !isAtEnd()) {
        if (match(TokType::KwCase)) {
            SwitchCaseBranch br;
            br.matchValue = parseExpr();
            consume(TokType::Colon, "':' bekleniyordu");
            while (!check(TokType::KwCase) && !check(TokType::KwDefault) && !check(TokType::RBrace) && !isAtEnd()) {
                if (match(TokType::KwBreak)) {
                    consume(TokType::Semicolon, "';' bekleniyordu");
                    break;
                }
                br.stmts.push_back(parseStmt());
            }
            branches.push_back(std::move(br));
        } else if (match(TokType::KwDefault)) {
            SwitchCaseBranch br;
            br.matchValue = nullptr;
            consume(TokType::Colon, "':' bekleniyordu");
            while (!check(TokType::KwCase) && !check(TokType::KwDefault) && !check(TokType::RBrace) && !isAtEnd()) {
                if (match(TokType::KwBreak)) {
                    consume(TokType::Semicolon, "';' bekleniyordu");
                    break;
                }
                br.stmts.push_back(parseStmt());
            }
            branches.push_back(std::move(br));
        } else {
            throw Core_ParseError(peek().line, peek().col, "'olursa' veya 'hicbiri' bekleniyordu", { L"olursa", L"hicbiri" }, peek().lexeme);
        }
    }
    consume(TokType::RBrace, "'}' bekleniyordu");
    auto n = std::make_unique<SwitchStmt>(std::move(target), std::move(branches));
    n->line = ln; n->col = cl;
    return n;
}

StmtPtr Parser::parseWhile() {
    int ln = prev().line, cl = prev().col; consume(TokType::LParen, "'(' bekleniyordu"); auto cond = parseExpr();
    consume(TokType::RParen, "')' bekleniyordu"); auto body = parseStmt(); auto n = std::make_unique<WhiteStmt>(std::move(cond), std::move(body)); n->line = ln; n->col = cl; return n;
}
StmtPtr Parser::parseDoWhile() {
    int ln = prev().line, cl = prev().col; auto body = parseStmt(); consume(TokType::KwWhile, "'oldukca' bekleniyordu");
    consume(TokType::LParen, "'(' bekleniyordu"); auto cond = parseExpr(); consume(TokType::RParen, "')' bekleniyordu"); consume(TokType::Semicolon, "';' bekleniyordu");
    auto n = std::make_unique<DoWhileStmt>(std::move(body), std::move(cond)); n->line = ln; n->col = cl; return n;
}
StmtPtr Parser::parseVarDecl() {
    bool isConst = false; int ln = peek().line, cl = peek().col; if (match(TokType::KwConst)) { isConst = true; ln = peek().line; cl = peek().col; }
    TokType typeTok = peek().type;
    if (!IsPrimitiveTypeTok(typeTok)) {
        throw Core_ParseError(peek().line, peek().col, "Tip bekleniyordu (tamsayi/laf/mantik/...)", { L"(tip)" }, peek().lexeme);
    }
    Value::Type declT = TokTypeToValueType(typeTok); i++; Token nameTok = consume(TokType::Identifier, "Degisken ismi bekleniyordu");
    if (match(TokType::Assign)) {
        auto init = parseExpr();
        consume(TokType::Semicolon, "';' bekleniyordu");
        auto n = std::make_unique<VarDeclStmt>(nameTok.lexeme, declT, std::move(init), isConst);
        n->line = ln; n->col = cl; return n;
    }
    if (isConst) throw Core_ParseError(peek().line, peek().col, "sabit degisken ilk deger almak zorunda", { L"=" }, peek().lexeme);
    consume(TokType::Semicolon, "';' bekleniyordu");
    auto n = std::make_unique<VarDeclStmt>(nameTok.lexeme, declT, nullptr, isConst);
    n->line = ln; n->col = cl; return n;
}

StmtPtr Parser::parseArrayDecl() {
    int ln = prev().line, cl = prev().col;
    TokType typeTok = peek().type;
    if (!IsPrimitiveTypeTok(typeTok)) {
        throw Core_ParseError(peek().line, peek().col, "Dizi eleman tipi bekleniyordu (tamsayi/laf/...)", { L"(tip)" }, peek().lexeme);
    }
    Value::Type elemT = TokTypeToValueType(typeTok); i++;
    Token nameTok = consume(TokType::Identifier, "Dizi ismi bekleniyordu");

    // 1. durum: dizi_gari tamsayi sayilar = [10, 20, 30];
    if (match(TokType::Assign)) {
        consume(TokType::LBracket, "'[' bekleniyordu");
        std::vector<ExprPtr> inits;
        if (!check(TokType::RBracket)) {
            while (true) {
                inits.push_back(parseExpr());
                if (match(TokType::Comma)) continue;
                break;
            }
        }
        consume(TokType::RBracket, "']' bekleniyordu");
        consume(TokType::Semicolon, "';' bekleniyordu");
        auto n = std::make_unique<ArrayDeclStmt>(nameTok.lexeme, elemT, nullptr, std::move(inits));
        n->line = ln; n->col = cl; return n;
    }

    // 2. durum: dizi_gari tamsayi sayilar[5]; veya dizi_gari tamsayi sayilar[3] = [1, 2, 3];
    consume(TokType::LBracket, "'[' veya '=' bekleniyordu");
    ExprPtr sizeE = nullptr;
    if (!check(TokType::RBracket)) {
        sizeE = parseExpr();
    }
    consume(TokType::RBracket, "']' bekleniyordu");

    std::vector<ExprPtr> inits;
    if (match(TokType::Assign)) {
        consume(TokType::LBracket, "'[' bekleniyordu");
        if (!check(TokType::RBracket)) {
            while (true) {
                inits.push_back(parseExpr());
                if (match(TokType::Comma)) continue;
                break;
            }
        }
        consume(TokType::RBracket, "']' bekleniyordu");
    }

    consume(TokType::Semicolon, "';' bekleniyordu");
    auto n = std::make_unique<ArrayDeclStmt>(nameTok.lexeme, elemT, std::move(sizeE), std::move(inits));
    n->line = ln; n->col = cl; return n;
}

StmtPtr Parser::parseMapDecl() {
    int ln = prev().line, cl = prev().col;
    TokType typeTok = peek().type;
    if (!IsPrimitiveTypeTok(typeTok)) {
        throw Core_ParseError(peek().line, peek().col, "Sozluk deger tipi bekleniyordu (tamsayi/laf/mantik/...)", { L"(tip)" }, peek().lexeme);
    }
    Value::Type valT = TokTypeToValueType(typeTok); i++;
    Token nameTok = consume(TokType::Identifier, "Sozluk ismi bekleniyordu");

    std::vector<std::pair<ExprPtr, ExprPtr>> entries;
    if (match(TokType::Assign)) {
        consume(TokType::LBracket, "'[' bekleniyordu");
        if (!check(TokType::RBracket)) {
            while (true) {
                auto keyE = parseExpr();
                consume(TokType::Colon, "':' bekleniyordu (anahtar : deger)");
                auto valE = parseExpr();
                entries.push_back({ std::move(keyE), std::move(valE) });
                if (match(TokType::Comma)) continue;
                break;
            }
        }
        consume(TokType::RBracket, "']' bekleniyordu");
    }
    consume(TokType::Semicolon, "';' bekleniyordu");
    auto n = std::make_unique<MapDeclStmt>(nameTok.lexeme, valT, std::move(entries));
    n->line = ln; n->col = cl;
    return n;
}

// Identifier ile başlayan tüm ifadeleri (atama, +=, -=, *=, /=, %=, ++, --, dizi ataması, kalıp alanı ataması, kalıp değişkeni tanımı, fonksiyon çağrısı) ayrıştırır
StmtPtr Parser::parseIdentifierLedStmt(const Token& nameTok, bool requireSemicolon) {
    int ln = nameTok.line, cl = nameTok.col;
    auto maybeSemi = [&]() { if (requireSemicolon) consume(TokType::Semicolon, "';' bekleniyordu"); };

    // 1. Eğer nameTok bilinen bir kalip_gari (struct) tipiyse ve arkasından Identifier geliyorsa: Zeybek efe;
    if (isKnownStruct(nameTok.lexeme) && check(TokType::Identifier)) {
        Token varTok = consume(TokType::Identifier, "Nesne degisken ismi bekleniyordu");
        maybeSemi();
        auto n = std::make_unique<StructVarDeclStmt>(nameTok.lexeme, varTok.lexeme);
        n->line = ln; n->col = cl; return n;
    }

    // 2. Fonksiyon çağrısı komutu: selamla_gari(x);
    if (match(TokType::LParen)) {
        std::vector<ExprPtr> args;
        if (!check(TokType::RParen)) {
            while (true) { args.push_back(parseExpr()); if (match(TokType::Comma)) continue; break; }
        }
        consume(TokType::RParen, "')' bekleniyordu");
        maybeSemi();
        auto call = std::make_unique<CallExpr>(nameTok.lexeme, std::move(args));
        call->line = ln; call->col = cl;
        auto st = std::make_unique<ExprStmt>(std::move(call));
        st->line = ln; st->col = cl; return st;
    }

    // 3. Dizi elemanı ataması: dizi[i] = val; veya dizi[i] += val; veya dizi[i]++;
    if (match(TokType::LBracket)) {
        size_t idxStart = i;
        auto idx = parseExpr();
        size_t idxEnd = i;
        consume(TokType::RBracket, "']' bekleniyordu");

        auto cloneIdx = [&]() -> ExprPtr {
            size_t saved = i;
            i = idxStart;
            ExprPtr copy = parseExpr();
            i = saved;
            return copy;
        };

        if (match(TokType::Assign)) {
            auto val = parseExpr();
            maybeSemi();
            auto n = std::make_unique<ArrayAssignStmt>(nameTok.lexeme, std::move(idx), std::move(val));
            n->line = ln; n->col = cl; return n;
        }
        if (match(TokType::PlusPlus) || match(TokType::MinusMinus)) {
            BinOp bo = (prev().type == TokType::PlusPlus) ? BinOp::Add : BinOp::Sub;
            maybeSemi();
            auto lhs = std::make_unique<ArrayAccessExpr>(nameTok.lexeme, cloneIdx()); lhs->line = ln; lhs->col = cl;
            auto one = std::make_unique<IntExpr>(1); one->line = ln; one->col = cl;
            auto expr = std::make_unique<BinExpr>(bo, std::move(lhs), std::move(one)); expr->line = ln; expr->col = cl;
            auto n = std::make_unique<ArrayAssignStmt>(nameTok.lexeme, std::move(idx), std::move(expr));
            n->line = ln; n->col = cl; return n;
        }
        if (match(TokType::PlusAssign) || match(TokType::MinusAssign) || match(TokType::StarAssign) || match(TokType::SlashAssign) || match(TokType::PercentAssign)) {
            TokType opT = prev().type;
            BinOp bo = BinOp::Add;
            if (opT == TokType::MinusAssign) bo = BinOp::Sub;
            else if (opT == TokType::StarAssign) bo = BinOp::Mul;
            else if (opT == TokType::SlashAssign) bo = BinOp::Div;
            else if (opT == TokType::PercentAssign) bo = BinOp::Mod;
            auto rhs = parseExpr();
            maybeSemi();
            auto lhs = std::make_unique<ArrayAccessExpr>(nameTok.lexeme, cloneIdx()); lhs->line = ln; lhs->col = cl;
            auto expr = std::make_unique<BinExpr>(bo, std::move(lhs), std::move(rhs)); expr->line = ln; expr->col = cl;
            auto n = std::make_unique<ArrayAssignStmt>(nameTok.lexeme, std::move(idx), std::move(expr));
            n->line = ln; n->col = cl; return n;
        }
        (void)idxEnd;
        throw Core_ParseError(peek().line, peek().col, "Dizi elemanina atama icin '=' bekleniyordu", { L"=" }, peek().lexeme);
    }

    // 4. Kalıp (struct) alanı ataması: efe.yas = 25; veya efe.yas += 1; veya efe.yas++;
    if (match(TokType::Dot)) {
        Token fieldTok = consume(TokType::Identifier, "Alan ismi bekleniyordu");
        if (match(TokType::Assign)) {
            auto val = parseExpr();
            maybeSemi();
            auto n = std::make_unique<FieldAssignStmt>(nameTok.lexeme, fieldTok.lexeme, std::move(val));
            n->line = ln; n->col = cl; return n;
        }
        if (match(TokType::PlusPlus) || match(TokType::MinusMinus)) {
            BinOp bo = (prev().type == TokType::PlusPlus) ? BinOp::Add : BinOp::Sub;
            maybeSemi();
            auto lhs = std::make_unique<FieldAccessExpr>(nameTok.lexeme, fieldTok.lexeme); lhs->line = ln; lhs->col = cl;
            auto one = std::make_unique<IntExpr>(1); one->line = ln; one->col = cl;
            auto expr = std::make_unique<BinExpr>(bo, std::move(lhs), std::move(one)); expr->line = ln; expr->col = cl;
            auto n = std::make_unique<FieldAssignStmt>(nameTok.lexeme, fieldTok.lexeme, std::move(expr));
            n->line = ln; n->col = cl; return n;
        }
        if (match(TokType::PlusAssign) || match(TokType::MinusAssign) || match(TokType::StarAssign) || match(TokType::SlashAssign) || match(TokType::PercentAssign)) {
            TokType opT = prev().type;
            BinOp bo = BinOp::Add;
            if (opT == TokType::MinusAssign) bo = BinOp::Sub;
            else if (opT == TokType::StarAssign) bo = BinOp::Mul;
            else if (opT == TokType::SlashAssign) bo = BinOp::Div;
            else if (opT == TokType::PercentAssign) bo = BinOp::Mod;
            auto rhs = parseExpr();
            maybeSemi();
            auto lhs = std::make_unique<FieldAccessExpr>(nameTok.lexeme, fieldTok.lexeme); lhs->line = ln; lhs->col = cl;
            auto expr = std::make_unique<BinExpr>(bo, std::move(lhs), std::move(rhs)); expr->line = ln; expr->col = cl;
            auto n = std::make_unique<FieldAssignStmt>(nameTok.lexeme, fieldTok.lexeme, std::move(expr));
            n->line = ln; n->col = cl; return n;
        }
        throw Core_ParseError(peek().line, peek().col, "Alan atamasi icin '=' bekleniyordu", { L"=" }, peek().lexeme);
    }

    // 5. Normal değişken atamaları: =, +=, -=, *=, /=, %=, ++, --
    if (match(TokType::Assign)) {
        auto v = parseExpr();
        maybeSemi();
        auto n = std::make_unique<AssignStmt>(nameTok.lexeme, std::move(v));
        n->line = ln; n->col = cl; return n;
    }
    if (match(TokType::PlusPlus) || match(TokType::MinusMinus)) {
        int opLine = prev().line, opCol = prev().col;
        BinOp bo = (prev().type == TokType::PlusPlus) ? BinOp::Add : BinOp::Sub;
        maybeSemi();
        auto lhs = std::make_unique<VarExpr>(nameTok.lexeme); lhs->line = ln; lhs->col = cl;
        auto one = std::make_unique<IntExpr>(1); one->line = opLine; one->col = opCol;
        auto expr = std::make_unique<BinExpr>(bo, std::move(lhs), std::move(one)); expr->line = opLine; expr->col = opCol;
        auto n = std::make_unique<AssignStmt>(nameTok.lexeme, std::move(expr));
        n->line = ln; n->col = cl; return n;
    }
    if (match(TokType::PlusAssign) || match(TokType::MinusAssign) || match(TokType::StarAssign) || match(TokType::SlashAssign) || match(TokType::PercentAssign)) {
        int opLine = prev().line, opCol = prev().col;
        TokType opT = prev().type;
        BinOp bo = BinOp::Add;
        if (opT == TokType::MinusAssign) bo = BinOp::Sub;
        else if (opT == TokType::StarAssign) bo = BinOp::Mul;
        else if (opT == TokType::SlashAssign) bo = BinOp::Div;
        else if (opT == TokType::PercentAssign) bo = BinOp::Mod;
        auto rhs = parseExpr();
        maybeSemi();
        auto lhs = std::make_unique<VarExpr>(nameTok.lexeme); lhs->line = ln; lhs->col = cl;
        auto expr = std::make_unique<BinExpr>(bo, std::move(lhs), std::move(rhs)); expr->line = opLine; expr->col = opCol;
        auto n = std::make_unique<AssignStmt>(nameTok.lexeme, std::move(expr));
        n->line = ln; n->col = cl; return n;
    }

    throw Core_ParseError(peek().line, peek().col, "Atama operatoru veya fonksiyon cagrisi bekleniyordu", { L"=", L"+=", L"-=", L"++", L"--", L"(" }, peek().lexeme);
}

StmtPtr Parser::parseSimpleAssignNoSemicolon() {
    Token nameTok = consume(TokType::Identifier, "degisken adi bekleniyordu");
    return parseIdentifierLedStmt(nameTok, false);
}

StmtPtr Parser::parseAssign() {
    Token nameTok = consume(TokType::Identifier, "Degisken ismi bekleniyordu");
    return parseIdentifierLedStmt(nameTok, true);
}

StmtPtr Parser::parseFor() {
    int ln = prev().line, cl = prev().col; consume(TokType::LParen, "'(' bekleniyordu");
    StmtPtr init = nullptr;
    if (check(TokType::KwConst) || IsPrimitiveTypeTok(peek().type)) init = parseVarDecl();
    else if (check(TokType::Identifier)) {
        Token nameTok = consume(TokType::Identifier, "");
        init = parseIdentifierLedStmt(nameTok, true);
    }
    else consume(TokType::Semicolon, "';' bekleniyordu");
    ExprPtr cond = nullptr; if (!check(TokType::Semicolon)) cond = parseExpr(); consume(TokType::Semicolon, "';' bekleniyordu");
    StmtPtr step = nullptr; if (!check(TokType::RParen)) step = parseSimpleAssignNoSemicolon(); consume(TokType::RParen, "')' bekleniyordu");
    auto body = parseStmt(); if (!cond) { cond = std::make_unique<IntExpr>(1); cond->line = ln; cond->col = cl; }
    auto n = std::make_unique<ForStmt>(std::move(init), std::move(cond), std::move(step), std::move(body)); n->line = ln; n->col = cl; return n;
}

StmtPtr Parser::parseForeach() {
    int ln = prev().line, cl = prev().col;
    consume(TokType::LParen, "'(' bekleniyordu");
    TokType typeTok = peek().type;
    if (!IsPrimitiveTypeTok(typeTok)) {
        throw Core_ParseError(peek().line, peek().col, "gez_gari degisken tipi bekleniyordu (tamsayi/laf/...)", { L"(tip)" }, peek().lexeme);
    }
    Value::Type vt = TokTypeToValueType(typeTok); i++;
    Token varTok = consume(TokType::Identifier, "gez_gari degisken ismi bekleniyordu");
    consume(TokType::Colon, "':' bekleniyordu");
    auto iterable = parseExpr();
    consume(TokType::RParen, "')' bekleniyordu");
    auto body = parseStmt();
    auto n = std::make_unique<ForeachStmt>(varTok.lexeme, vt, std::move(iterable), std::move(body));
    n->line = ln; n->col = cl;
    return n;
}

StmtPtr Parser::parseTryCatch() {
    int ln = prev().line, cl = prev().col;
    auto tryBody = parseStmt();
    consume(TokType::KwCatch, "'yakala_gari' bekleniyordu");
    consume(TokType::LParen, "'(' bekleniyordu");
    if (check(TokType::KwString)) i++; // Opsiyonel 'laf' tipi: yakala_gari (laf hata) veya yakala_gari (hata)
    Token errVarTok = consume(TokType::Identifier, "Hata degiskeni ismi bekleniyordu");
    consume(TokType::RParen, "')' bekleniyordu");
    auto catchBody = parseStmt();
    auto n = std::make_unique<TryCatchStmt>(std::move(tryBody), errVarTok.lexeme, std::move(catchBody));
    n->line = ln; n->col = cl;
    return n;
}

StmtPtr Parser::parseThrow() {
    int ln = prev().line, cl = prev().col;
    bool hasParen = match(TokType::LParen);
    auto e = parseExpr();
    if (hasParen) consume(TokType::RParen, "')' bekleniyordu");
    consume(TokType::Semicolon, "';' bekleniyordu");
    auto n = std::make_unique<ThrowStmt>(std::move(e));
    n->line = ln; n->col = cl;
    return n;
}

StmtPtr Parser::parseStmt() {
    if (match(TokType::KwReturn))    return parseReturn();
    if (match(TokType::KwDoWhile))   return parseDoWhile();
    if (match(TokType::LBrace))      return parseBlockAfterLBrace();
    if (match(TokType::KwIf))        return parseIf();
    if (match(TokType::KwSwitch))    return parseSwitch();
    if (match(TokType::KwWhile))     return parseWhile();
    if (match(TokType::KwFor))       return parseFor();
    if (match(TokType::KwForeach))   return parseForeach();
    if (match(TokType::KwTry))       return parseTryCatch();
    if (match(TokType::KwThrow))     return parseThrow();
    if (match(TokType::KwPrint))     return parsePrint();
    if (match(TokType::KwArray))     return parseArrayDecl();
    if (match(TokType::KwMap))       return parseMapDecl();
    if (match(TokType::KwBreak)) {
        int ln = prev().line, cl = prev().col; consume(TokType::Semicolon, "';' bekleniyordu");
        auto n = std::make_unique<BreakStmt>(); n->line = ln; n->col = cl; return n;
    }
    if (match(TokType::KwContinue)) {
        int ln = prev().line, cl = prev().col; consume(TokType::Semicolon, "';' bekleniyordu");
        auto n = std::make_unique<ContinueStmt>(); n->line = ln; n->col = cl; return n;
    }
    if (check(TokType::KwConst) || IsPrimitiveTypeTok(peek().type)) return parseVarDecl();
    if (check(TokType::Identifier)) {
        Token nameTok = consume(TokType::Identifier, "");
        return parseIdentifierLedStmt(nameTok, true);
    }
    throw Core_ParseError(peek().line, peek().col, "Komut/statement bekleniyordu", {}, peek().lexeme);
}

std::unique_ptr<StructDeclStmt> Parser::parseStructDecl() {
    int ln = prev().line, cl = prev().col;
    Token nameTok = consume(TokType::Identifier, "Kalip (struct) ismi bekleniyordu");
    knownStructs.push_back(nameTok.lexeme);
    consume(TokType::LBrace, "'{' bekleniyordu");

    std::vector<StructFieldDef> fields;
    while (!check(TokType::RBrace) && !isAtEnd()) {
        TokType fTypeTok = peek().type;
        if (!IsPrimitiveTypeTok(fTypeTok)) {
            throw Core_ParseError(peek().line, peek().col, "Kalip alani tipi bekleniyordu (tamsayi/laf/mantik/...)", { L"(tip)" }, peek().lexeme);
        }
        Value::Type fType = TokTypeToValueType(fTypeTok); i++;
        Token fNameTok = consume(TokType::Identifier, "Kalip alani ismi bekleniyordu");
        consume(TokType::Semicolon, "';' bekleniyordu");
        fields.push_back(StructFieldDef{ fNameTok.lexeme, fType });
    }
    consume(TokType::RBrace, "'}' bekleniyordu");
    match(TokType::Semicolon); // Opsiyonel ';'

    auto n = std::make_unique<StructDeclStmt>(nameTok.lexeme, std::move(fields));
    n->line = ln; n->col = cl;
    return n;
}

std::unique_ptr<FuncDeclStmt> Parser::parseFuncDecl() {
    int ln = prev().line, cl = prev().col;

    TokType typeTok = peek().type;
    if (!(IsPrimitiveTypeTok(typeTok) || typeTok == TokType::KwVoid)) {
        throw Core_ParseError(peek().line, peek().col, "Fonksiyon geri donus tipi bekleniyordu (tamsayi/laf/bos_gari/...)", { L"(tip)", L"bos_gari" }, peek().lexeme);
    }
    Value::Type retType = TokTypeToValueType(typeTok); i++;

    Token nameTok = consume(TokType::Identifier, "Fonksiyon ismi bekleniyordu");
    consume(TokType::LParen, "'(' bekleniyordu");

    std::vector<FuncParam> params;
    bool seenDefault = false;
    if (!check(TokType::RParen)) {
        while (true) {
            if (match(TokType::KwArray)) {
                TokType elemTok = peek().type;
                if (!IsPrimitiveTypeTok(elemTok)) {
                    throw Core_ParseError(peek().line, peek().col, "Dizi parametresi eleman tipi bekleniyordu", { L"(tip)" }, peek().lexeme);
                }
                Value::Type elemT = TokTypeToValueType(elemTok); i++;
                Token pNameTok = consume(TokType::Identifier, "Parametre ismi bekleniyordu");
                if (seenDefault) throw Core_ParseError(pNameTok.line, pNameTok.col, "Varsayilan degerli parametreden sonra normal parametre gelemez", {}, pNameTok.lexeme);
                params.push_back(FuncParam{ pNameTok.lexeme, Value::Type::ARRAY, elemT, nullptr });
            } else if (match(TokType::KwMap)) {
                TokType elemTok = peek().type;
                if (!IsPrimitiveTypeTok(elemTok)) {
                    throw Core_ParseError(peek().line, peek().col, "Sozluk parametresi deger tipi bekleniyordu", { L"(tip)" }, peek().lexeme);
                }
                Value::Type elemT = TokTypeToValueType(elemTok); i++;
                Token pNameTok = consume(TokType::Identifier, "Parametre ismi bekleniyordu");
                if (seenDefault) throw Core_ParseError(pNameTok.line, pNameTok.col, "Varsayilan degerli parametreden sonra normal parametre gelemez", {}, pNameTok.lexeme);
                params.push_back(FuncParam{ pNameTok.lexeme, Value::Type::MAP, elemT, nullptr });
            } else {
                TokType pTypeTok = peek().type;
                if (!IsPrimitiveTypeTok(pTypeTok)) {
                    throw Core_ParseError(peek().line, peek().col, "Parametre tipi bekleniyordu", { L"(tip)" }, peek().lexeme);
                }
                Value::Type pType = TokTypeToValueType(pTypeTok); i++;
                Token pNameTok = consume(TokType::Identifier, "Parametre ismi bekleniyordu");
                ExprPtr defExpr = nullptr;
                if (match(TokType::Assign)) {
                    defExpr = parseExpr();
                    seenDefault = true;
                } else if (seenDefault) {
                    throw Core_ParseError(pNameTok.line, pNameTok.col, "Varsayilan degerli parametreden sonra varsayilan degersiz parametre gelemez", { L"=" }, pNameTok.lexeme);
                }
                params.push_back(FuncParam{ pNameTok.lexeme, pType, pType, std::move(defExpr) });
            }

            if (match(TokType::Comma)) continue;
            break;
        }
    }
    consume(TokType::RParen, "')' bekleniyordu");
    consume(TokType::LBrace, "'{' bekleniyordu");

    auto bodyStmt = parseBlockAfterLBrace();
    auto* rawBlock = dynamic_cast<BlockStmt*>(bodyStmt.get());
    std::unique_ptr<BlockStmt> bodyBlock(nullptr);
    if (rawBlock) {
        bodyStmt.release();
        bodyBlock = std::unique_ptr<BlockStmt>(rawBlock);
    }

    auto n = std::make_unique<FuncDeclStmt>(nameTok.lexeme, retType, std::move(params), std::move(bodyBlock));
    n->line = ln; n->col = cl;
    return n;
}

std::unique_ptr<Program> Parser::parseProgram() {
    auto p = std::make_unique<Program>();

    // basla_gari kelimesini görene kadar kalip_gari, küresel değişkenler ve fonksiyon_gari tanımlarını yakalıyoruz
    while (true) {
        if (match(TokType::KwStruct)) {
            p->structs.push_back(parseStructDecl());
        } else if (match(TokType::KwFunction)) {
            p->functions.push_back(parseFuncDecl());
        } else if (check(TokType::KwConst) || IsPrimitiveTypeTok(peek().type)) {
            auto vdStmt = parseVarDecl();
            auto* rawVd = dynamic_cast<VarDeclStmt*>(vdStmt.get());
            if (rawVd) {
                vdStmt.release();
                p->globals.push_back(std::unique_ptr<VarDeclStmt>(rawVd));
            }
        } else {
            break;
        }
    }

    consume(TokType::KwMain, "Program basla_gari ile baslamali");
    consume(TokType::LParen, "'(' bekleniyordu");
    consume(TokType::RParen, "')' bekleniyordu");
    consume(TokType::LBrace, "'{' bekleniyordu");

    auto body = std::make_unique<BlockStmt>();
    body->line = prev().line;
    body->col = prev().col;

    while (!check(TokType::RBrace) && !isAtEnd()) {
        body->stmts.push_back(parseStmt());
    }

    consume(TokType::RBrace, "'}' bekleniyordu");
    p->main = std::move(body);
    return p;
}