#include "vm.h"
#include "core.h"
#include <iomanip>
#include <cmath>

Slot* Env::findSlot(const std::wstring& name) {
    for (int i = (int)scopes.size() - 1; i >= 0; --i) {
        auto it = scopes[i].find(name);
        if (it != scopes[i].end()) return &it->second;
    }
    return nullptr;
}

bool Env::assign(const std::wstring& name, const Value& v, int line, int col) {
    for (int i = (int)scopes.size() - 1; i >= 0; --i) {
        auto it = scopes[i].find(name);
        if (it != scopes[i].end()) {
            if (it->second.isConst) throw Core_RuntimeError(line, col, "ConstError", "sabit degisken degistirilemez: " + Core_WToNarrow(name));
            it->second.v = Coerce(v, it->second.v.type); return true;
        }
    }
    return false;
}
Value Env::get(const std::wstring& name, int line, int col) const {
    for (int i = (int)scopes.size() - 1; i >= 0; --i) {
        auto it = scopes[i].find(name); if (it != scopes[i].end()) return it->second.v;
    }
    throw Core_RuntimeError(line, col, "NameError", "Tanimlanmamis degisken: " + Core_WToNarrow(name));
}

int AddConst(Chunk& ch, const Value& v) { ch.consts.push_back(v); return (int)ch.consts.size() - 1; }

std::wstring OpName(Op op) {
    switch (op) {
    case Op::NOP: return L"NOP"; case Op::PUSH_CONST: return L"PUSH_CONST"; case Op::POP: return L"POP";
    case Op::ENTER_SCOPE: return L"ENTER_SCOPE"; case Op::EXIT_SCOPE: return L"EXIT_SCOPE";
    case Op::LOAD: return L"LOAD"; case Op::DECLARE: return L"DECLARE"; case Op::STORE: return L"STORE";
    case Op::TOBOOL: return L"TOBOOL"; case Op::ADD: return L"ADD"; case Op::SUB: return L"SUB";
    case Op::MUL: return L"MUL"; case Op::DIV: return L"DIV"; case Op::MOD: return L"MOD";
    case Op::EQ: return L"EQ"; case Op::NE: return L"NE"; case Op::LT: return L"LT";
    case Op::GT: return L"GT"; case Op::LE: return L"LE"; case Op::GE: return L"GE";
    case Op::JMP: return L"JMP"; case Op::JMPF: return L"JMPF"; case Op::JMPT: return L"JMPT";
    case Op::PRINT: return L"PRINT"; case Op::CALL_BUILTIN: return L"CALL_BUILTIN"; case Op::RETURN: return L"RETURN";
    case Op::NOT: return L"NOT"; case Op::BREAK: return L"BREAK"; case Op::CONTINUE: return L"CONTINUE";
    case Op::NEW_ARRAY: return L"NEW_ARRAY";
    case Op::NEW_ARRAY_LIST: return L"NEW_ARRAY_LIST";
    case Op::LOAD_ELEMENT: return L"LOAD_ELEMENT";
    case Op::STORE_ELEMENT: return L"STORE_ELEMENT";
    case Op::NEW_STRUCT: return L"NEW_STRUCT";
    case Op::LOAD_FIELD: return L"LOAD_FIELD";
    case Op::STORE_FIELD: return L"STORE_FIELD";
    case Op::NEW_MAP: return L"NEW_MAP";
    case Op::TRY_BEGIN: return L"TRY_BEGIN";
    case Op::TRY_END: return L"TRY_END";
    case Op::THROW_ERR: return L"THROW_ERR";
    default: return L"?";
    }
}

enum class BuiltinId : int {
    OkuGari, OkuTamsayi, OkuHassasSayi, Kok, Us, Mutlak, Taban, Tavan, Yuvarla,
    MinGari, MaxGari, Sinus, Kosinus, Tanjant, Cotanjant, Ln, Log10, Ustel, PiSayisi, ESayisi,
    BoyutGari, EkleGari, CikarGari, RastgeleGari, LafaCevir, TamsayiyaCevir, HassasaCevir,
    BuyutGari, KucultGari, ParcalaGari, IcindeVarMi,
    SiralaGari, TersCevirGari, DegistirGari, KirpGari,
    DosyaYazGari, DosyaOkuGari, DosyaVarMi, BekleGari, VakitGari,
    AnahtarlarGari, SilGari
};

std::wstring BuiltinName(int id) {
    switch ((BuiltinId)id) {
    case BuiltinId::OkuGari: return L"oku_gari"; case BuiltinId::OkuTamsayi: return L"oku_tamsayi";
    case BuiltinId::OkuHassasSayi: return L"oku_hassas_sayi"; case BuiltinId::Kok: return L"kok";
    case BuiltinId::Us: return L"us"; case BuiltinId::Mutlak: return L"mutlak";
    case BuiltinId::Taban: return L"taban"; case BuiltinId::Tavan: return L"tavan";
    case BuiltinId::Yuvarla: return L"yuvarla"; case BuiltinId::MinGari: return L"min_gari";
    case BuiltinId::MaxGari: return L"max_gari"; case BuiltinId::Sinus: return L"sinus";
    case BuiltinId::Kosinus: return L"kosinus"; case BuiltinId::Tanjant: return L"tanjant";
    case BuiltinId::Cotanjant: return L"cotanjant"; case BuiltinId::Ln: return L"ln";
    case BuiltinId::Log10: return L"log10"; case BuiltinId::Ustel: return L"ustel";
    case BuiltinId::PiSayisi: return L"pi_sayisi"; case BuiltinId::ESayisi: return L"e_sayisi";
    case BuiltinId::BoyutGari: return L"boyut_gari"; case BuiltinId::EkleGari: return L"ekle_gari";
    case BuiltinId::CikarGari: return L"cikar_gari"; case BuiltinId::RastgeleGari: return L"rastgele_gari";
    case BuiltinId::LafaCevir: return L"lafa_cevir"; case BuiltinId::TamsayiyaCevir: return L"tamsayiya_cevir";
    case BuiltinId::HassasaCevir: return L"hassasa_cevir"; case BuiltinId::BuyutGari: return L"buyut_gari";
    case BuiltinId::KucultGari: return L"kucult_gari"; case BuiltinId::ParcalaGari: return L"parcala_gari";
    case BuiltinId::IcindeVarMi: return L"icinde_var_mi";
    case BuiltinId::SiralaGari: return L"sirala_gari"; case BuiltinId::TersCevirGari: return L"ters_cevir_gari";
    case BuiltinId::DegistirGari: return L"degistir_gari"; case BuiltinId::KirpGari: return L"kirp_gari";
    case BuiltinId::DosyaYazGari: return L"dosya_yaz_gari"; case BuiltinId::DosyaOkuGari: return L"dosya_oku_gari";
    case BuiltinId::DosyaVarMi: return L"dosya_var_mi"; case BuiltinId::BekleGari: return L"bekle_gari";
    case BuiltinId::VakitGari: return L"vakit_gari";
    case BuiltinId::AnahtarlarGari: return L"anahtarlar_gari"; case BuiltinId::SilGari: return L"sil_gari";
    default: return L"";
    }
}

int BuiltinIdFromName(const std::wstring& name) {
    if (name == L"oku_gari") return 0;        if (name == L"oku_tamsayi") return 1;
    if (name == L"oku_hassas_sayi") return 2; if (name == L"kok") return 3;
    if (name == L"us") return 4;              if (name == L"mutlak") return 5;
    if (name == L"taban") return 6;           if (name == L"tavan") return 7;
    if (name == L"yuvarla") return 8;         if (name == L"min_gari") return 9;
    if (name == L"max_gari") return 10;       if (name == L"sinus") return 11;
    if (name == L"kosinus") return 12;        if (name == L"tanjant") return 13;
    if (name == L"cotanjant") return 14;      if (name == L"ln") return 15;
    if (name == L"log10") return 16;          if (name == L"ustel") return 17;
    if (name == L"pi_sayisi") return 18;      if (name == L"e_sayisi") return 19;
    if (name == L"boyut_gari") return 20;     if (name == L"ekle_gari") return 21;
    if (name == L"cikar_gari") return 22;     if (name == L"rastgele_gari") return 23;
    if (name == L"lafa_cevir") return 24;     if (name == L"tamsayiya_cevir") return 25;
    if (name == L"hassasa_cevir") return 26;  if (name == L"buyut_gari") return 27;
    if (name == L"kucult_gari") return 28;    if (name == L"parcala_gari") return 29;
    if (name == L"icinde_var_mi") return 30;
    if (name == L"sirala_gari") return 31;    if (name == L"ters_cevir_gari") return 32;
    if (name == L"degistir_gari") return 33;  if (name == L"kirp_gari") return 34;
    if (name == L"dosya_yaz_gari") return 35; if (name == L"dosya_oku_gari") return 36;
    if (name == L"dosya_var_mi") return 37;   if (name == L"bekle_gari") return 38;
    if (name == L"vakit_gari") return 39;
    if (name == L"anahtarlar_gari") return 40; if (name == L"sil_gari") return 41;
    return -1;
}

std::wstring ValueTypeName(Value::Type t) {
    switch (t) {
    case Value::Type::VOID: return L"bos_gari";
    case Value::Type::INT: return L"tamsayi";
    case Value::Type::FLOAT: return L"hassas_sayi";
    case Value::Type::DOUBLE: return L"cift_hassas_sayi";
    case Value::Type::STRING: return L"laf";
    case Value::Type::BOOL: return L"mantik";
    case Value::Type::ARRAY: return L"dizi_gari";
    case Value::Type::STRUCT: return L"kalip_gari";
    case Value::Type::MAP: return L"sozluk_gari";
    default: return L"?";
    }
}

int Compiler::emitJump(Op op, int line, int col) { emit(op, -1, 0, line, col); return (int)ch.code.size() - 1; }
void Compiler::patchJump(int at, int target) { if (at < 0 || at >= (int)ch.code.size()) return; ch.code[at].a = target; }
void Compiler::enterScope(int line, int col) { scopes.push_back({}); scopeSlotBase.push_back(nextSlot); emit(Op::ENTER_SCOPE, nextSlot, 0, line, col); }
void Compiler::exitScope(int line, int col) { int base = scopeSlotBase.back(); scopes.pop_back(); scopeSlotBase.pop_back(); nextSlot = base; emit(Op::EXIT_SCOPE, 0, 0, line, col); }
Sym* Compiler::resolve(const std::wstring& name) {
    for (int i = (int)scopes.size() - 1; i >= 0; --i) { auto it = scopes[i].find(name); if (it != scopes[i].end()) return &it->second; }
    return nullptr;
}
int Compiler::declareSym(const std::wstring& name, Value::Type t, bool isConst, int line, int col) {
    auto& cur = scopes.back(); if (cur.find(name) != cur.end()) throw Core_RuntimeError(line, col, "NameError", "Ayni scopeta tekrar tanim: " + Core_WToNarrow(name));
    int slot = nextSlot++; cur[name] = Sym{ slot, isConst, t }; return slot;
}

void Compiler::compileExpr(const Expr* e) {
    if (!e) { emit(Op::PUSH_CONST, AddConst(ch, Value::makeInt(0)), 0, 1, 1); return; }
    if (auto ie = dynamic_cast<const IntExpr*>(e)) { emit(Op::PUSH_CONST, AddConst(ch, Value::makeInt(ie->v)), 0, e->line, e->col); return; }
    if (auto de = dynamic_cast<const DoubleExpr*>(e)) { emit(Op::PUSH_CONST, AddConst(ch, Value::makeDouble(de->v)), 0, e->line, e->col); return; }
    if (auto se = dynamic_cast<const StrExpr*>(e)) { emit(Op::PUSH_CONST, AddConst(ch, Value::makeString(se->v)), 0, e->line, e->col); return; }
    if (auto be = dynamic_cast<const BoolExpr*>(e)) { emit(Op::PUSH_CONST, AddConst(ch, Value::makeBool(be->v)), 0, e->line, e->col); return; }

    if (auto te = dynamic_cast<const TernaryExpr*>(e)) {
        compileExpr(te->cond.get());
        int jf = emitJump(Op::JMPF, e->line, e->col);
        compileExpr(te->thenE.get());
        int jend = emitJump(Op::JMP, e->line, e->col);
        patchJump(jf, (int)ch.code.size());
        compileExpr(te->elseE.get());
        patchJump(jend, (int)ch.code.size());
        return;
    }

    if (auto ae = dynamic_cast<const ArrayAccessExpr*>(e)) {
        Sym* s = resolve(ae->name);
        if (!s) throw Core_RuntimeError(e->line, e->col, "NameError", "Tanimlanmamis dizi: " + Core_WToNarrow(ae->name));
        compileExpr(ae->index.get());
        emit(Op::LOAD_ELEMENT, s->slot, 0, e->line, e->col);
        return;
    }

    if (auto fa = dynamic_cast<const FieldAccessExpr*>(e)) {
        Sym* s = resolve(fa->objName);
        if (!s) throw Core_RuntimeError(e->line, e->col, "NameError", "Tanimlanmamis nesne: " + Core_WToNarrow(fa->objName));
        int fNameConst = AddConst(ch, Value::makeString(fa->fieldName));
        emit(Op::LOAD_FIELD, s->slot, fNameConst, e->line, e->col);
        return;
    }

    if (auto n = dynamic_cast<const NotExpr*>(e)) { compileExpr(n->expr.get()); emit(Op::NOT, 0, 0, e->line, e->col); return; }
    if (auto ve = dynamic_cast<const VarExpr*>(e)) {
        Sym* s = resolve(ve->name); if (!s) throw Core_RuntimeError(e->line, e->col, "NameError", "Tanimlanmamis degisken: " + Core_WToNarrow(ve->name));
        emit(Op::LOAD, s->slot, 0, e->line, e->col); return;
    }

    if (auto ce = dynamic_cast<const CallExpr*>(e)) {
        int bid = BuiltinIdFromName(ce->name);
        if (bid >= 0) {
            for (auto& a : ce->args) compileExpr(a.get());
            emit(Op::CALL_BUILTIN, bid, (int)ce->args.size(), e->line, e->col);
            return;
        }
        // Kullanıcı tanımlı fonksiyon: ismini const havuzuna ekleyip negatif indeksle işaretle
        int nameSlot = AddConst(ch, Value::makeString(ce->name));
        for (auto& a : ce->args) compileExpr(a.get());
        emit(Op::CALL_BUILTIN, -nameSlot - 1, (int)ce->args.size(), e->line, e->col);
        return;
    }
    if (auto be = dynamic_cast<const BinExpr*>(e)) {
        if (be->op == BinOp::AndAnd) {
            compileExpr(be->a.get()); int jf = emitJump(Op::JMPF, e->line, e->col); compileExpr(be->b.get()); emit(Op::TOBOOL, 0, 0, e->line, e->col);
            int jend = emitJump(Op::JMP, e->line, e->col); patchJump(jf, (int)ch.code.size()); emit(Op::PUSH_CONST, AddConst(ch, Value::makeBool(false)), 0, e->line, e->col); patchJump(jend, (int)ch.code.size()); return;
        }
        if (be->op == BinOp::OrOr) {
            compileExpr(be->a.get()); int jt = emitJump(Op::JMPT, e->line, e->col); compileExpr(be->b.get()); emit(Op::TOBOOL, 0, 0, e->line, e->col);
            int jend = emitJump(Op::JMP, e->line, e->col); patchJump(jt, (int)ch.code.size()); emit(Op::PUSH_CONST, AddConst(ch, Value::makeBool(true)), 0, e->line, e->col); patchJump(jend, (int)ch.code.size()); return;
        }
        compileExpr(be->a.get()); compileExpr(be->b.get());

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
}

void Compiler::compileStmt(const Stmt* s) {
    if (!s) return;
    if (auto b = dynamic_cast<const BlockStmt*>(s)) { enterScope(s->line, s->col); for (auto& st : b->stmts) compileStmt(st.get()); exitScope(s->line, s->col); return; }
    if (dynamic_cast<const StructDeclStmt*>(s)) { return; }

    if (auto svd = dynamic_cast<const StructVarDeclStmt*>(s)) {
        int slot = declareSym(svd->varName, Value::Type::STRUCT, false, s->line, s->col);
        int sNameConst = AddConst(ch, Value::makeString(svd->structName));
        emit(Op::NEW_STRUCT, slot, sNameConst, s->line, s->col);
        return;
    }

    if (auto fa = dynamic_cast<const FieldAssignStmt*>(s)) {
        Sym* sym = resolve(fa->objName);
        if (!sym) throw Core_RuntimeError(s->line, s->col, "NameError", "Tanimlanmamis nesne: " + Core_WToNarrow(fa->objName));
        compileExpr(fa->value.get());
        int fNameConst = AddConst(ch, Value::makeString(fa->fieldName));
        emit(Op::STORE_FIELD, sym->slot, fNameConst, s->line, s->col);
        return;
    }

    if (auto es = dynamic_cast<const ExprStmt*>(s)) {
        compileExpr(es->expr.get());
        emit(Op::POP, 0, 0, s->line, s->col);
        return;
    }

    if (auto fd = dynamic_cast<const FuncDeclStmt*>(s)) {
        int jmpFuncOver = emitJump(Op::JMP, s->line, s->col);
        enterScope(s->line, s->col);
        for (auto& p : fd->params) {
            declareSym(p.name, p.type, false, s->line, s->col);
        }
        if (fd->body) compileStmt(fd->body.get());
        exitScope(s->line, s->col);
        emit(Op::RETURN, 0, 0, s->line, s->col);
        patchJump(jmpFuncOver, (int)ch.code.size());
        return;
    }

    if (auto ad = dynamic_cast<const ArrayDeclStmt*>(s)) {
        int slot = declareSym(ad->name, Value::Type::ARRAY, false, s->line, s->col);
        if (!ad->initElements.empty() || !ad->sizeExpr) {
            for (auto& ie : ad->initElements) compileExpr(ie.get());
            int countAndType = ((int)ad->initElements.size() << 8) | ((int)ad->elementType & 0xFF);
            emit(Op::NEW_ARRAY_LIST, slot, countAndType, s->line, s->col);
        } else {
            compileExpr(ad->sizeExpr.get());
            emit(Op::NEW_ARRAY, slot, (int)ad->elementType, s->line, s->col);
        }
        return;
    }
    if (auto md = dynamic_cast<const MapDeclStmt*>(s)) {
        int slot = declareSym(md->name, Value::Type::MAP, false, s->line, s->col);
        for (auto& kv : md->initEntries) {
            compileExpr(kv.first.get());
            compileExpr(kv.second.get());
        }
        int countAndType = ((int)md->initEntries.size() << 8) | ((int)md->valueType & 0xFF);
        emit(Op::NEW_MAP, slot, countAndType, s->line, s->col);
        return;
    }
    if (auto aa = dynamic_cast<const ArrayAssignStmt*>(s)) {
        Sym* sym = resolve(aa->name);
        if (!sym) throw Core_RuntimeError(s->line, s->col, "NameError", "Tanimlanmamis diziye atama: " + Core_WToNarrow(aa->name));
        compileExpr(aa->index.get());
        compileExpr(aa->value.get());
        emit(Op::STORE_ELEMENT, sym->slot, 0, s->line, s->col);
        return;
    }

    if (auto vd = dynamic_cast<const VarDeclStmt*>(s)) {
        if (vd->init) compileExpr(vd->init.get()); else emit(Op::PUSH_CONST, AddConst(ch, Value::makeDefaultForType(vd->declaredType)), 0, s->line, s->col);
        int slot = declareSym(vd->name, vd->declaredType, vd->isConst, s->line, s->col); emit(Op::DECLARE, slot, ((int)vd->declaredType) | (vd->isConst ? 0x100 : 0), s->line, s->col); return;
    }
    if (auto as = dynamic_cast<const AssignStmt*>(s)) {
        Sym* sym = resolve(as->name); if (!sym) throw Core_RuntimeError(s->line, s->col, "NameError", "Tanimlanmamis degiskene atama: " + Core_WToNarrow(as->name));
        compileExpr(as->value.get()); emit(Op::STORE, sym->slot, 0, s->line, s->col); return;
    }
    if (auto pr = dynamic_cast<const PrintStmt*>(s)) { compileExpr(pr->expr.get()); emit(Op::PRINT, 0, 0, s->line, s->col); return; }
    if (dynamic_cast<const BreakStmt*>(s)) {
        if (loopStack.empty()) throw Core_RuntimeError(s->line, s->col, "SyntaxError", "kir_gari sadece dongu veya bak_hele icinde kullanilabilir.");
        int j = emitJump(Op::JMP, s->line, s->col); loopStack.back().breakJumps.push_back(j); return;
    }
    if (dynamic_cast<const ContinueStmt*>(s)) {
        if (loopStack.empty()) throw Core_RuntimeError(s->line, s->col, "SyntaxError", "pas_gec sadece dongu icinde kullanilabilir.");
        int j = emitJump(Op::JMP, s->line, s->col); loopStack.back().continueJumps.push_back(j); return;
    }
    if (auto iff = dynamic_cast<const IfStmt*>(s)) {
        compileExpr(iff->cond.get()); int jf = emitJump(Op::JMPF, s->line, s->col); compileStmt(iff->thenS.get()); int jend = emitJump(Op::JMP, s->line, s->col);
        patchJump(jf, (int)ch.code.size()); if (iff->elseS) compileStmt(iff->elseS.get()); patchJump(jend, (int)ch.code.size()); return;
    }
    if (auto sw = dynamic_cast<const SwitchStmt*>(s)) {
        enterScope(s->line, s->col);
        compileExpr(sw->target.get());
        int targetSlot = declareSym(L"$switch_target_" + std::to_wstring(ch.code.size()), Value::Type::INT, false, s->line, s->col);
        // targetSlot'a herhangi bir tip dönüşümü yapmadan saklamak için STORE
        emit(Op::STORE, targetSlot, -1, s->line, s->col); // b = -1: raw store (tip zorlamasız)

        std::vector<int> endJumps;
        for (auto& br : sw->branches) {
            int nextBranchJmp = -1;
            if (br.matchValue) {
                emit(Op::LOAD, targetSlot, 0, s->line, s->col);
                compileExpr(br.matchValue.get());
                emit(Op::EQ, 0, 0, s->line, s->col);
                nextBranchJmp = emitJump(Op::JMPF, s->line, s->col);
            }
            enterScope(s->line, s->col);
            for (auto& st : br.stmts) compileStmt(st.get());
            exitScope(s->line, s->col);
            endJumps.push_back(emitJump(Op::JMP, s->line, s->col));
            if (nextBranchJmp >= 0) {
                patchJump(nextBranchJmp, (int)ch.code.size());
            }
        }
        int switchEnd = (int)ch.code.size();
        for (int j : endJumps) patchJump(j, switchEnd);
        exitScope(s->line, s->col);
        return;
    }
    if (auto wh = dynamic_cast<const WhiteStmt*>(s)) {
        loopStack.push_back({}); int Lstart = (int)ch.code.size(); compileExpr(wh->cond.get()); int jf = emitJump(Op::JMPF, s->line, s->col);
        compileStmt(wh->body.get()); emit(Op::JMP, Lstart, 0, s->line, s->col); int Lend = (int)ch.code.size(); patchJump(jf, Lend);
        for (int j : loopStack.back().breakJumps) patchJump(j, Lend);
        for (int j : loopStack.back().continueJumps) patchJump(j, Lstart);
        loopStack.pop_back(); return;
    }
    if (auto dw = dynamic_cast<const DoWhileStmt*>(s)) {
        loopStack.push_back({}); int Lstart = (int)ch.code.size(); compileStmt(dw->body.get()); int Lcond = (int)ch.code.size();
        compileExpr(dw->cond.get()); emit(Op::JMPT, Lstart, 0, s->line, s->col); int Lend = (int)ch.code.size();
        for (int j : loopStack.back().breakJumps) patchJump(j, Lend);
        for (int j : loopStack.back().continueJumps) patchJump(j, Lcond);
        loopStack.pop_back(); return;
    }
    if (auto fr = dynamic_cast<const ForStmt*>(s)) {
        loopStack.push_back({}); enterScope(s->line, s->col); if (fr->init) compileStmt(fr->init.get()); int Lstart = (int)ch.code.size(); compileExpr(fr->cond.get()); int jf = emitJump(Op::JMPF, s->line, s->col);
        compileStmt(fr->body.get()); int Lstep = (int)ch.code.size(); if (fr->step) compileStmt(fr->step.get()); emit(Op::JMP, Lstart, 0, s->line, s->col); int Lend = (int)ch.code.size(); patchJump(jf, Lend);
        for (int j : loopStack.back().breakJumps) patchJump(j, Lend);
        for (int j : loopStack.back().continueJumps) patchJump(j, Lstep);
        exitScope(s->line, s->col); loopStack.pop_back(); return;
    }
    if (auto fe = dynamic_cast<const ForeachStmt*>(s)) {
        loopStack.push_back({});
        enterScope(s->line, s->col);
        size_t uid = ch.code.size();

        // 1. Koleksiyonu (dizi_gari veya laf) sakla
        compileExpr(fe->iterable.get());
        int colSlot = declareSym(L"$fe_col_" + std::to_wstring(uid), Value::Type::ARRAY, false, s->line, s->col);
        emit(Op::STORE, colSlot, -1, s->line, s->col);

        // 2. İndeks = 0
        emit(Op::PUSH_CONST, AddConst(ch, Value::makeInt(0)), 0, s->line, s->col);
        int idxSlot = declareSym(L"$fe_idx_" + std::to_wstring(uid), Value::Type::INT, false, s->line, s->col);
        emit(Op::DECLARE, idxSlot, (int)Value::Type::INT, s->line, s->col);

        // 3. Eleman değişkenini tanımla
        emit(Op::PUSH_CONST, AddConst(ch, Value::makeDefaultForType(fe->varType)), 0, s->line, s->col);
        int varSlot = declareSym(fe->varName, fe->varType, false, s->line, s->col);
        emit(Op::DECLARE, varSlot, (int)fe->varType, s->line, s->col);

        // 4. Döngü koşulu: idx < boyut_gari(col)
        int Lstart = (int)ch.code.size();
        emit(Op::LOAD, idxSlot, 0, s->line, s->col);
        emit(Op::LOAD, colSlot, 0, s->line, s->col);
        emit(Op::CALL_BUILTIN, BuiltinIdFromName(L"boyut_gari"), 1, s->line, s->col);
        emit(Op::LT, 0, 0, s->line, s->col);
        int jf = emitJump(Op::JMPF, s->line, s->col);

        // 5. Elemanı yükle ve değişkene ata
        emit(Op::LOAD, idxSlot, 0, s->line, s->col);
        emit(Op::LOAD_ELEMENT, colSlot, 0, s->line, s->col);
        emit(Op::STORE, varSlot, 0, s->line, s->col);

        // 6. Döngü gövdesi
        compileStmt(fe->body.get());

        // 7. İndeksi 1 artır
        int Lstep = (int)ch.code.size();
        emit(Op::LOAD, idxSlot, 0, s->line, s->col);
        emit(Op::PUSH_CONST, AddConst(ch, Value::makeInt(1)), 0, s->line, s->col);
        emit(Op::ADD, 0, 0, s->line, s->col);
        emit(Op::STORE, idxSlot, 0, s->line, s->col);
        emit(Op::JMP, Lstart, 0, s->line, s->col);

        int Lend = (int)ch.code.size();
        patchJump(jf, Lend);
        for (int j : loopStack.back().breakJumps) patchJump(j, Lend);
        for (int j : loopStack.back().continueJumps) patchJump(j, Lstep);
        exitScope(s->line, s->col);
        loopStack.pop_back();
        return;
    }
    if (auto tc = dynamic_cast<const TryCatchStmt*>(s)) {
        int tryBeginAt = emitJump(Op::TRY_BEGIN, s->line, s->col);
        compileStmt(tc->tryBody.get());
        emit(Op::TRY_END, 0, 0, s->line, s->col);
        int jmpOverCatch = emitJump(Op::JMP, s->line, s->col);

        int catchStart = (int)ch.code.size();
        patchJump(tryBeginAt, catchStart);

        enterScope(s->line, s->col);
        int errSlot = declareSym(tc->errVarName, Value::Type::STRING, false, s->line, s->col);
        // Hata mesajı stack'in en üstündedir
        emit(Op::DECLARE, errSlot, (int)Value::Type::STRING, s->line, s->col);
        compileStmt(tc->catchBody.get());
        exitScope(s->line, s->col);

        patchJump(jmpOverCatch, (int)ch.code.size());
        return;
    }
    if (auto th = dynamic_cast<const ThrowStmt*>(s)) {
        if (th->expr) compileExpr(th->expr.get());
        else emit(Op::PUSH_CONST, AddConst(ch, Value::makeString(L"Bilinmeyen hata")), 0, s->line, s->col);
        emit(Op::THROW_ERR, 0, 0, s->line, s->col);
        return;
    }
    if (auto rt = dynamic_cast<const ReturnStmt*>(s)) { if (rt->value) { compileExpr(rt->value.get()); emit(Op::RETURN, 1, 0, s->line, s->col); } else { emit(Op::RETURN, 0, 0, s->line, s->col); } return; }
}

Chunk Compiler::compileMain(const Program& p) {
    for (auto& g : p.globals) {
        compileStmt(g.get());
    }
    for (auto& f : p.functions) {
        compileStmt(f.get());
    }
    compileStmt(p.main.get());
    emit(Op::RETURN, 0, 0, 1, 1);
    return std::move(ch);
}

std::wstring DisasmChunk(const Chunk& ch) {
    std::wstringstream ss; ss << L"; ===== BYTECODE =====\n; consts: " << (int)ch.consts.size() << L"\n\n";
    for (size_t i = 0; i < ch.code.size(); ++i) {
        const auto& in = ch.code[i]; ss << std::setw(4) << i << L"  " << OpName(in.op);
        if (in.op == Op::PUSH_CONST) ss << L" " << in.a << L" ; " << ValueToString(ch.consts[(size_t)in.a]);
        else if (in.op == Op::LOAD || in.op == Op::STORE) ss << L" slot=" << in.a;
        else if (in.op == Op::DECLARE) { int t = (in.b & 0xFF); bool isConst = (in.b & 0x100) != 0; ss << L" slot=" << in.a << L" type=" << ValueTypeName((Value::Type)t) << (isConst ? L" const" : L""); }
        else if (in.op == Op::JMP || in.op == Op::JMPF || in.op == Op::JMPT) ss << L" -> " << in.a;
        else if (in.op == Op::RETURN) ss << L" hasValue=" << in.a;
        ss << L"  ; (" << in.line << L":" << in.col << L")\n";
    }
    return ss.str();
}

void VM::ensureSlot(int slot) {
    if (slot < 0) return;
    if ((int)locals.size() <= slot) { locals.resize((size_t)slot + 1); localType.resize((size_t)slot + 1, Value::Type::INT); localConst.resize((size_t)slot + 1, false); }
}
Value VM::pop() { if (stack.empty()) return Value::makeInt(0); Value v = stack.back(); stack.pop_back(); return v; }
void VM::push(const Value& v) { stack.push_back(v); }
void VM::doBinary(Op op, int line, int col) {
    Value B = pop(); Value A = pop();
    if (op == Op::EQ || op == Op::NE || op == Op::LT || op == Op::GT || op == Op::LE || op == Op::GE) {
        bool res = false;
        if (op == Op::EQ) res = ValueEquals(A, B); else if (op == Op::NE) res = !ValueEquals(A, B);
        else {
            if (A.type == Value::Type::STRING || B.type == Value::Type::STRING) {
                std::wstring x = ValueToString(A), y = ValueToString(B);
                if (op == Op::LT) res = (x < y); if (op == Op::GT) res = (x > y); if (op == Op::LE) res = (x <= y); if (op == Op::GE) res = (x >= y);
            }
            else {
                double x = ToDouble(A), y = ToDouble(B);
                if (op == Op::LT) res = (x < y); if (op == Op::GT) res = (x > y); if (op == Op::LE) res = (x <= y); if (op == Op::GE) res = (x >= y);
            }
        }
        push(Value::makeBool(res)); return;
    }
    if (op == Op::ADD && (A.type == Value::Type::STRING || B.type == Value::Type::STRING)) { push(Value::makeString(ValueToString(A) + ValueToString(B))); return; }
    if (op == Op::MOD && (!IsNumeric(A.type) || !IsNumeric(B.type))) throw Core_RuntimeError(line, col, "TypeError", "'%' sadece sayilarda kullanilir");

    if (A.type == Value::Type::DOUBLE || B.type == Value::Type::DOUBLE) {
        double x = ToDouble(A), y = ToDouble(B);
        if (op == Op::ADD) push(Value::makeDouble(x + y)); else if (op == Op::SUB) push(Value::makeDouble(x - y));
        else if (op == Op::MUL) push(Value::makeDouble(x * y));
        else if (op == Op::DIV) { if (y == 0.0) throw Core_RuntimeError(line, col, "ZeroDivision", ZeroDivMsg("/", A, B)); push(Value::makeDouble(x / y)); }
        else if (op == Op::MOD) { if (y == 0.0) throw Core_RuntimeError(line, col, "ZeroDivision", ZeroDivMsg("%", A, B)); push(Value::makeDouble(std::fmod(x, y))); }
        else push(Value::makeDouble(0.0)); return;
    }
    if (A.type == Value::Type::FLOAT || B.type == Value::Type::FLOAT) {
        float x = ToFloat(A), y = ToFloat(B);
        if (op == Op::ADD) push(Value::makeFloat(x + y)); else if (op == Op::SUB) push(Value::makeFloat(x - y));
        else if (op == Op::MUL) push(Value::makeFloat(x * y));
        else if (op == Op::DIV) { if (y == 0.0f) throw Core_RuntimeError(line, col, "ZeroDivision", ZeroDivMsg("/", A, B)); push(Value::makeFloat(x / y)); }
        else if (op == Op::MOD) { if (y == 0.0f) throw Core_RuntimeError(line, col, "ZeroDivision", ZeroDivMsg("%", A, B)); push(Value::makeFloat(std::fmod(x, y))); }
        else push(Value::makeFloat(0.0f)); return;
    }
    int x = ToInt(A), y = ToInt(B);
    if (op == Op::ADD) push(Value::makeInt(x + y)); else if (op == Op::SUB) push(Value::makeInt(x - y));
    else if (op == Op::MUL) push(Value::makeInt(x * y));
    else if (op == Op::DIV) { if (y == 0) throw Core_RuntimeError(line, col, "ZeroDivision", ZeroDivMsg("/", A, B)); push(Value::makeInt(x / y)); }
    else if (op == Op::MOD) { if (y == 0) throw Core_RuntimeError(line, col, "ZeroDivision", ZeroDivMsg("%", A, B)); push(Value::makeInt(x % y)); }
    else push(Value::makeInt(0));
}

VM::Result VM::run(const Chunk& chunk, const Program* program) {
    ch = &chunk; prog = program;
    stack.clear(); locals.clear(); localType.clear(); localConst.clear(); scopeBases.clear(); tryStack.clear(); out.str(L""); out.clear(); scopeBases.push_back(0);
    int ip = 0;
    while (ip >= 0 && ip < (int)ch->code.size()) {
        const Instr& in = ch->code[(size_t)ip];
        try {
        switch (in.op) {
        case Op::NOP: ip++; break;
        case Op::PUSH_CONST: push(ch->consts[(size_t)in.a]); ip++; break;
        case Op::POP: (void)pop(); ip++; break;
        case Op::ENTER_SCOPE: scopeBases.push_back((int)locals.size()); ip++; break;
        case Op::EXIT_SCOPE: { int base = scopeBases.back(); scopeBases.pop_back(); if ((int)locals.size() > base) { locals.resize((size_t)base); localType.resize((size_t)base); localConst.resize((size_t)base); } ip++; break; }
        case Op::LOAD: ensureSlot(in.a); push(locals[(size_t)in.a]); ip++; break;
        case Op::DECLARE: { ensureSlot(in.a); Value v = pop(); Value::Type declT = (Value::Type)(in.b & 0xFF); bool isConst = (in.b & 0x100) != 0; v = Coerce(v, declT); locals[(size_t)in.a] = v; localType[(size_t)in.a] = declT; localConst[(size_t)in.a] = isConst; ip++; break; }
        case Op::STORE: {
            ensureSlot(in.a);
            if (localConst[(size_t)in.a]) throw Core_RuntimeError(in.line, in.col, "ConstError", "sabit degisken degistirilemez");
            Value v = pop();
            if (in.b != -1) v = Coerce(v, localType[(size_t)in.a]);
            else localType[(size_t)in.a] = v.type;
            locals[(size_t)in.a] = v;
            ip++;
            break;
        }
        case Op::TOBOOL: { Value v = pop(); push(Value::makeBool(IsTrue(v))); ip++; break; }
        case Op::NOT: { Value v = pop(); push(Value::makeBool(!IsTrue(v))); ip++; break; }
        case Op::ADD: case Op::SUB: case Op::MUL: case Op::DIV: case Op::MOD: case Op::EQ: case Op::NE: case Op::LT: case Op::GT: case Op::LE: case Op::GE: doBinary(in.op, in.line, in.col); ip++; break;
        case Op::JMP: ip = in.a; break;
        case Op::JMPF: { Value c = pop(); if (!IsTrue(c)) ip = in.a; else ip++; break; }
        case Op::JMPT: { Value c = pop(); if (IsTrue(c)) ip = in.a; else ip++; break; }
        case Op::PRINT: {
            Value v = pop();
            out << ValueToString(v).c_str() << L"\r\n";
            ip++;
            break;
        }
        case Op::CALL_BUILTIN: {
            int argc = in.b;
            std::vector<Value> args((size_t)argc);
            for (int k = argc - 1; k >= 0; --k) args[(size_t)k] = pop();

            if (in.a < 0) {
                // Kullanıcı tanımlı fonksiyon çağrısı
                int nameSlot = -in.a - 1;
                std::wstring funcName = ValueToString(ch->consts[(size_t)nameSlot]);

                // prog üzerinden fonksiyon tanımını bul
                const FuncDeclStmt* targetFunc = nullptr;
                if (prog) {
                    for (auto& f : prog->functions) {
                        if (f->name == funcName) { targetFunc = f.get(); break; }
                    }
                }

                if (!targetFunc)
                    throw Core_RuntimeError(in.line, in.col, "NameError", "Fonksiyon tanimli degil: " + Core_WToNarrow(funcName));

                // Fonksiyon için yeni bir ortam oluştur, küresel değişkenleri ve parametreleri bağla
                std::wstringstream callOut;
                Env callEnv;
                callEnv.prog = prog;
                callEnv.outStream = &callOut;
                callEnv.callDepth = 1;

                if (prog) {
                    for (size_t gi = 0; gi < prog->globals.size() && gi < locals.size(); ++gi) {
                        callEnv.declare(prog->globals[gi]->name, locals[gi], prog->globals[gi]->isConst);
                    }
                }

                size_t minArgs = 0;
                for (const auto& prm : targetFunc->params) {
                    if (!prm.defaultVal) minArgs++;
                }
                if (args.size() < minArgs || args.size() > targetFunc->params.size())
                    throw Core_RuntimeError(in.line, in.col, "TypeError", "Yanlis arguman sayisi: " + Core_WToNarrow(funcName));

                callEnv.push();
                for (size_t pi = 0; pi < targetFunc->params.size(); ++pi) {
                    Value rawVal = (pi < args.size())
                        ? args[pi]
                        : (targetFunc->params[pi].defaultVal ? targetFunc->params[pi].defaultVal->eval(callEnv) : Value::makeDefaultForType(targetFunc->params[pi].type));
                    Value coerced = Coerce(rawVal, targetFunc->params[pi].type);
                    callEnv.declare(targetFunc->params[pi].name, coerced, false);
                }

                // Fonksiyon gövdesini AST yorumlayıcısıyla çalıştır (iç içe fonksiyonlar ve recursion destekli)
                ExecResult er = ExecStmt(targetFunc->body.get(), callEnv, callOut);
                out << callOut.str(); // Fonksiyon içindeki print çıktılarını ana akışa yaz

                // Küresel değişkenlerdeki güncellemeleri geri senkronize et
                if (prog) {
                    for (size_t gi = 0; gi < prog->globals.size() && gi < locals.size(); ++gi) {
                        if (!prog->globals[gi]->isConst) {
                            auto it = callEnv.scopes[0].find(prog->globals[gi]->name);
                            if (it != callEnv.scopes[0].end()) locals[gi] = it->second.v;
                        }
                    }
                }

                if (er.flow == ExecResult::Flow::Return) {
                    if (targetFunc->returnType == Value::Type::VOID) push(Value::makeVoid());
                    else push(Coerce(er.returnValue, targetFunc->returnType));
                }
                else {
                    if (targetFunc->returnType == Value::Type::VOID) push(Value::makeVoid());
                    else push(Value::makeDefaultForType(targetFunc->returnType));
                }

                ip++;
                break;
            }

            // Yerleşik (builtin) fonksiyon çağrısı
            std::wstring bname = BuiltinName(in.a);
            if (bname.empty()) throw Core_RuntimeError(in.line, in.col, "NameError", "Bilinmeyen builtin");
            push(EvalBuiltinCall(bname, args, in.line, in.col)); ip++; break;
        }

        case Op::NEW_ARRAY: {
            ensureSlot(in.a);
            Value sizeVal = pop();
            int size = ToInt(sizeVal);
            if (size <= 0) throw Core_RuntimeError(in.line, in.col, "ValueError", "Dizi boyutu pozitif olmalidir.");
            Value::Type elemT = (Value::Type)(in.b & 0xFF);
            locals[(size_t)in.a] = Value::makeArray((size_t)size, elemT);
            localType[(size_t)in.a] = Value::Type::ARRAY;
            ip++;
            break;
        }
        case Op::NEW_ARRAY_LIST: {
            ensureSlot(in.a);
            int count = (in.b >> 8);
            Value::Type elemT = (Value::Type)(in.b & 0xFF);
            std::vector<Value> items((size_t)count);
            for (int k = count - 1; k >= 0; --k) {
                items[(size_t)k] = Coerce(pop(), elemT);
            }
            locals[(size_t)in.a] = Value::makeArrayFromList(std::move(items), elemT);
            localType[(size_t)in.a] = Value::Type::ARRAY;
            ip++;
            break;
        }
        case Op::NEW_MAP: {
            ensureSlot(in.a);
            int count = (in.b >> 8);
            Value::Type valT = (Value::Type)(in.b & 0xFF);
            Value mp = Value::makeMap(valT);
            std::vector<std::pair<std::wstring, Value>> pairs((size_t)count);
            for (int k = count - 1; k >= 0; --k) {
                Value v = Coerce(pop(), valT);
                std::wstring key = ValueToString(pop());
                pairs[(size_t)k] = { std::move(key), std::move(v) };
            }
            for (auto& kv : pairs) {
                (*mp.mapVal)[kv.first] = kv.second;
            }
            locals[(size_t)in.a] = mp;
            localType[(size_t)in.a] = Value::Type::MAP;
            ip++;
            break;
        }
        case Op::LOAD_ELEMENT: {
            ensureSlot(in.a);
            Value arr = locals[(size_t)in.a];
            Value idxVal = pop();
            if (arr.type == Value::Type::MAP && arr.mapVal) {
                int idx = ToInt(idxVal);
                std::wstring key = ValueToString(idxVal);
                auto it = arr.mapVal->find(key);
                if (it != arr.mapVal->end()) {
                    push(it->second);
                    ip++;
                    break;
                }
                // gez_gari sırasında tamsayı indeksle sözlüğün i. anahtarını da desteklemek için:
                if (idxVal.type == Value::Type::INT && idx >= 0 && idx < (int)arr.mapVal->size()) {
                    auto mit = arr.mapVal->begin();
                    std::advance(mit, idx);
                    push(Value::makeString(mit->first));
                    ip++;
                    break;
                }
                throw Core_RuntimeError(in.line, in.col, "KeyError", "Sozlukte '" + Core_WToNarrow(key) + "' anahtari bulunamadi.");
            }
            int idx = ToInt(idxVal);
            if (arr.type == Value::Type::STRING) {
                if (idx < 0 || idx >= (int)arr.sVal.size()) {
                    throw Core_RuntimeError(in.line, in.col, "IndexError", "Laf (metin) sinirlari disinda indeks: " + std::to_string(idx));
                }
                push(Value::makeString(std::wstring(1, arr.sVal[(size_t)idx])));
                ip++;
                break;
            }
            if (!arr.arrVal || idx < 0 || idx >= (int)arr.arrVal->size()) {
                throw Core_RuntimeError(in.line, in.col, "IndexError", "Dizi sinirlari disinda indeks: " + std::to_string(idx));
            }
            push((*arr.arrVal)[(size_t)idx]);
            ip++;
            break;
        }
        case Op::STORE_ELEMENT: {
            ensureSlot(in.a);
            Value arr = locals[(size_t)in.a];
            Value val = pop();
            Value idxVal = pop();
            if (arr.type == Value::Type::MAP && arr.mapVal) {
                std::wstring key = ValueToString(idxVal);
                (*arr.mapVal)[key] = Coerce(val, arr.elemType);
                ip++;
                break;
            }
            int idx = ToInt(idxVal);
            if (!arr.arrVal || idx < 0 || idx >= (int)arr.arrVal->size()) {
                throw Core_RuntimeError(in.line, in.col, "IndexError", "Dizi sinirlari disinda indeks: " + std::to_string(idx));
            }
            (*arr.arrVal)[(size_t)idx] = Coerce(val, arr.elemType);
            ip++;
            break;
        }

        case Op::NEW_STRUCT: {
            ensureSlot(in.a);
            std::wstring sName = ValueToString(ch->consts[(size_t)in.b]);
            Value sVal = Value::makeStruct(sName);
            if (prog) {
                for (auto& sd : prog->structs) {
                    if (sd->name == sName) {
                        for (auto& f : sd->fields) {
                            (*sVal.fields)[f.name] = Value::makeDefaultForType(f.type);
                        }
                        break;
                    }
                }
            }
            locals[(size_t)in.a] = sVal;
            localType[(size_t)in.a] = Value::Type::STRUCT;
            ip++;
            break;
        }
        case Op::LOAD_FIELD: {
            ensureSlot(in.a);
            Value obj = locals[(size_t)in.a];
            std::wstring fName = ValueToString(ch->consts[(size_t)in.b]);
            if (obj.type != Value::Type::STRUCT || !obj.fields) {
                throw Core_RuntimeError(in.line, in.col, "TypeError", "Nesne bir kalip_gari degil.");
            }
            auto it = obj.fields->find(fName);
            if (it == obj.fields->end()) {
                throw Core_RuntimeError(in.line, in.col, "NameError", Core_WToNarrow(obj.structName) + " kalibinda '" + Core_WToNarrow(fName) + "' alani yok.");
            }
            push(it->second);
            ip++;
            break;
        }
        case Op::STORE_FIELD: {
            ensureSlot(in.a);
            Value obj = locals[(size_t)in.a];
            Value val = pop();
            std::wstring fName = ValueToString(ch->consts[(size_t)in.b]);
            if (obj.type != Value::Type::STRUCT || !obj.fields) {
                throw Core_RuntimeError(in.line, in.col, "TypeError", "Nesne bir kalip_gari degil.");
            }
            auto it = obj.fields->find(fName);
            if (it == obj.fields->end()) {
                throw Core_RuntimeError(in.line, in.col, "NameError", Core_WToNarrow(obj.structName) + " kalibinda '" + Core_WToNarrow(fName) + "' alani yok.");
            }
            it->second = Coerce(val, it->second.type);
            ip++;
            break;
        }

        case Op::TRY_BEGIN: {
            tryStack.push_back(TryHandler{ in.a, stack.size(), scopeBases.size() });
            ip++;
            break;
        }
        case Op::TRY_END: {
            if (!tryStack.empty()) tryStack.pop_back();
            ip++;
            break;
        }
        case Op::THROW_ERR: {
            Value msgVal = pop();
            throw Core_RuntimeError(in.line, in.col, "UserError", Core_WToNarrow(ValueToString(msgVal)));
        }

        case Op::RETURN: { Value r = Value::makeInt(0); if (in.a != 0) r = pop(); Result res; res.ret = r; res.hasReturn = true; res.output = out.str(); return res; }
        default: throw Core_RuntimeError(in.line, in.col, "VMError", "Bilinmeyen opcode");
        }
        } catch (const Core_RuntimeError& re) {
            if (!tryStack.empty()) {
                TryHandler th = tryStack.back();
                tryStack.pop_back();
                if (stack.size() > th.stackDepth) stack.resize(th.stackDepth);
                while (scopeBases.size() > th.scopeDepth) {
                    int base = scopeBases.back();
                    scopeBases.pop_back();
                    if ((int)locals.size() > base) {
                        locals.resize((size_t)base);
                        localType.resize((size_t)base);
                        localConst.resize((size_t)base);
                    }
                }
                push(Value::makeString(Core_WidenC(re.what())));
                ip = th.catchIp;
                continue;
            }
            throw;
        }
    }
    Result res; res.ret = Value::makeInt(0); res.hasReturn = false; res.output = out.str(); return res;
}

ExecResult ExecStmt(const Stmt* s, Env& env, std::wstringstream& out) {
    if (!s) return ExecResult::None();
    if (!env.outStream) env.outStream = &out;
    if (auto b = dynamic_cast<const BlockStmt*>(s)) return ExecBlockWithOut(b, env, out);
    if (dynamic_cast<const BreakStmt*>(s)) return ExecResult::Brk();
    if (dynamic_cast<const ContinueStmt*>(s)) return ExecResult::Cnt();
    if (dynamic_cast<const StructDeclStmt*>(s)) return ExecResult::None();
    if (auto svd = dynamic_cast<const StructVarDeclStmt*>(s)) { svd->eval(env); return ExecResult::None(); }
    if (auto fa = dynamic_cast<const FieldAssignStmt*>(s)) { fa->eval(env); return ExecResult::None(); }
    if (auto es = dynamic_cast<const ExprStmt*>(s)) { es->eval(env); return ExecResult::None(); }
    if (auto vd = dynamic_cast<const VarDeclStmt*>(s)) { Value v = vd->init ? vd->init->eval(env) : Value::makeDefaultForType(vd->declaredType); v = Coerce(v, vd->declaredType); env.declare(vd->name, v, vd->isConst); return ExecResult::None(); }
    if (auto ad = dynamic_cast<const ArrayDeclStmt*>(s)) { ad->eval(env); return ExecResult::None(); }
    if (auto md = dynamic_cast<const MapDeclStmt*>(s)) { md->eval(env); return ExecResult::None(); }
    if (auto aa = dynamic_cast<const ArrayAssignStmt*>(s)) { aa->eval(env); return ExecResult::None(); }

    if (dynamic_cast<const FuncDeclStmt*>(s)) { return ExecResult::None(); }

    if (auto as = dynamic_cast<const AssignStmt*>(s)) { Value v = as->value->eval(env); if (!env.assign(as->name, v, as->line, as->col)) throw Core_RuntimeError(as->line, as->col, "NameError", "Tanimlanmamis degiskene atama: " + Core_WToNarrow(as->name)); return ExecResult::None(); }
    if (auto pr = dynamic_cast<const PrintStmt*>(s)) { Value v = pr->expr->eval(env); out << ValueToString(v).c_str() << L"\r\n"; return ExecResult::None(); }
    if (auto iff = dynamic_cast<const IfStmt*>(s)) { Value c = iff->cond->eval(env); if (IsTrue(c)) return ExecStmt(iff->thenS.get(), env, out); if (iff->elseS) return ExecStmt(iff->elseS.get(), env, out); return ExecResult::None(); }
    if (auto sw = dynamic_cast<const SwitchStmt*>(s)) {
        Value targetVal = sw->target->eval(env);
        const SwitchCaseBranch* defaultBranch = nullptr;
        const SwitchCaseBranch* matchedBranch = nullptr;
        for (const auto& br : sw->branches) {
            if (!br.matchValue) {
                if (!defaultBranch) defaultBranch = &br;
            } else {
                Value caseVal = br.matchValue->eval(env);
                if (ValueEquals(targetVal, caseVal)) {
                    matchedBranch = &br;
                    break;
                }
            }
        }
        const SwitchCaseBranch* chosen = matchedBranch ? matchedBranch : defaultBranch;
        if (chosen) {
            env.push();
            for (const auto& st : chosen->stmts) {
                ExecResult r = ExecStmt(st.get(), env, out);
                if (r.flow == ExecResult::Flow::Break) { env.pop(); return ExecResult::None(); }
                if (r.flow != ExecResult::Flow::Normal) { env.pop(); return r; }
            }
            env.pop();
        }
        return ExecResult::None();
    }
    if (auto wh = dynamic_cast<const WhiteStmt*>(s)) {
        int guard = 0; while (IsTrue(wh->cond->eval(env))) { ExecResult r = ExecStmt(wh->body.get(), env, out); if (r.flow == ExecResult::Flow::Return) return r; if (r.flow == ExecResult::Flow::Break) break; if (r.flow == ExecResult::Flow::Continue) { if (++guard > 200000) throw Core_RuntimeError(wh->line, wh->col, "LoopError", "Muhtemel sonsuz while"); continue; } if (++guard > 200000) throw Core_RuntimeError(wh->line, wh->col, "LoopError", "Muhtemel sonsuz while"); } return ExecResult::None();
    }
    if (auto dw = dynamic_cast<const DoWhileStmt*>(s)) {
        int guard = 0; do { ExecResult r = ExecStmt(dw->body.get(), env, out); if (r.flow == ExecResult::Flow::Return) return r; if (r.flow == ExecResult::Flow::Break) break; if (r.flow == ExecResult::Flow::Continue) { if (++guard > 200000) throw Core_RuntimeError(dw->line, dw->col, "LoopError", "Muhtemel sonsuz do-while"); continue; } if (++guard > 200000) throw Core_RuntimeError(dw->line, dw->col, "LoopError", "Muhtemel sonsuz do-while"); } while (IsTrue(dw->cond->eval(env))); return ExecResult::None();
    }
    if (auto fr = dynamic_cast<const ForStmt*>(s)) {
        env.push(); if (fr->init) { ExecResult ir = ExecStmt(fr->init.get(), env, out); if (ir.flow != ExecResult::Flow::Normal) { env.pop(); return ir; } }
        int guard = 0;
        while (IsTrue(fr->cond->eval(env))) {
            ExecResult r = ExecStmt(fr->body.get(), env, out); if (r.flow == ExecResult::Flow::Return) { env.pop(); return r; } if (r.flow == ExecResult::Flow::Break) break;
            if (fr->step) { ExecResult sr = ExecStmt(fr->step.get(), env, out); if (sr.flow == ExecResult::Flow::Return) { env.pop(); return sr; } }
            if (++guard > 200000) { env.pop(); throw Core_RuntimeError(fr->line, fr->col, "LoopError", "Muhtemel sonsuz for"); }
        }
        env.pop(); return ExecResult::None();
    }
    if (auto fe = dynamic_cast<const ForeachStmt*>(s)) {
        Value col = fe->iterable->eval(env);
        std::vector<Value> items;
        if (col.type == Value::Type::ARRAY && col.arrVal) {
            items = *col.arrVal;
        } else if (col.type == Value::Type::STRING) {
            for (wchar_t ch : col.sVal) items.push_back(Value::makeString(std::wstring(1, ch)));
        } else if (col.type == Value::Type::MAP && col.mapVal) {
            for (const auto& kv : *col.mapVal) items.push_back(Value::makeString(kv.first));
        } else {
            throw Core_RuntimeError(fe->line, fe->col, "TypeError", "gez_gari sadece dizi_gari, sozluk_gari veya laf uzerinde calisir.");
        }
        env.push();
        env.declare(fe->varName, Value::makeDefaultForType(fe->varType), false);
        for (const auto& item : items) {
            env.assign(fe->varName, Coerce(item, fe->varType), fe->line, fe->col);
            ExecResult r = ExecStmt(fe->body.get(), env, out);
            if (r.flow == ExecResult::Flow::Return) { env.pop(); return r; }
            if (r.flow == ExecResult::Flow::Break) break;
        }
        env.pop();
        return ExecResult::None();
    }
    if (auto tc = dynamic_cast<const TryCatchStmt*>(s)) {
        size_t savedScopes = env.scopes.size();
        try {
            return ExecStmt(tc->tryBody.get(), env, out);
        } catch (const Core_RuntimeError& re) {
            while (env.scopes.size() > savedScopes) env.pop();
            env.push();
            env.declare(tc->errVarName, Value::makeString(Core_WidenC(re.what())), false);
            ExecResult cr = ExecStmt(tc->catchBody.get(), env, out);
            env.pop();
            return cr;
        }
    }
    if (auto th = dynamic_cast<const ThrowStmt*>(s)) {
        th->eval(env);
        return ExecResult::None();
    }
    if (auto rt = dynamic_cast<const ReturnStmt*>(s)) { Value v = rt->value ? rt->value->eval(env) : Value::makeInt(0); return ExecResult::Ret(v); }
    return ExecResult::None();
}

ExecResult ExecBlockWithOut(const BlockStmt* b, Env& env, std::wstringstream& out) {
    env.push();
    for (auto& st : b->stmts) { ExecResult r = ExecStmt(st.get(), env, out); if (r.flow != ExecResult::Flow::Normal) { env.pop(); return r; } }
    env.pop(); return ExecResult::None();
}