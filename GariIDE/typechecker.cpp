#include "typechecker.h"
#include "core.h"
#include <cmath>

const wchar_t* STypeName(SType t) {
    switch (t) {
    case SType::INT: return L"tamsayi";
    case SType::FLOAT: return L"hassas_sayi";
    case SType::DOUBLE: return L"cift_hassas_sayi";
    case SType::STRING: return L"laf";
    case SType::BOOL: return L"mantik";
    case SType::ARRAY: return L"dizi_gari";
    case SType::STRUCT: return L"kalip_gari";
    case SType::MAP: return L"sozluk_gari";
    case SType::VOID: return L"bos_gari";
    case SType::ANY: return L"any";
    default: return L"invalid";
    }
}

TypeSlot TypeEnv::getSlot(const std::wstring& n) const {
    for (int i = (int)scopes.size() - 1; i >= 0; --i) {
        auto it = scopes[i].find(n); if (it != scopes[i].end()) return it->second;
    }
    return TypeSlot{ SType::INVALID, false };
}

bool TypeEnv::assignable(SType to, SType from) const {
    if (to == SType::VOID || from == SType::VOID) return to == from;
    if (to == SType::ANY || from == SType::ANY) return true;
    if (to == from) return true;
    if (to == SType::DOUBLE && (from == SType::FLOAT || from == SType::INT)) return true;
    if (to == SType::FLOAT && (from == SType::INT)) return true;
    if (from == SType::STRING && (to == SType::INT || to == SType::FLOAT || to == SType::DOUBLE)) return true;
    return false;
}

static SType VTypeToSType(Value::Type t) {
    switch (t) {
    case Value::Type::INT: return SType::INT;
    case Value::Type::FLOAT: return SType::FLOAT;
    case Value::Type::DOUBLE: return SType::DOUBLE;
    case Value::Type::STRING: return SType::STRING;
    case Value::Type::BOOL: return SType::BOOL;
    case Value::Type::ARRAY: return SType::ARRAY;
    case Value::Type::STRUCT: return SType::STRUCT;
    case Value::Type::MAP: return SType::MAP;
    case Value::Type::VOID: return SType::VOID;
    default: return SType::ANY;
    }
}

static bool IsOriginalNumeric(SType t) {
    return t == SType::INT || t == SType::FLOAT || t == SType::DOUBLE || t == SType::BOOL || t == SType::ANY;
}

static SType PromoteNumeric(SType a, SType b) {
    if (a == SType::ANY || b == SType::ANY) return SType::ANY;
    if (a == SType::DOUBLE || b == SType::DOUBLE) return SType::DOUBLE;
    if (a == SType::FLOAT || b == SType::FLOAT) return SType::FLOAT;
    if (a == SType::BOOL && b == SType::BOOL) return SType::BOOL;
    if (a == SType::INT && b == SType::INT) return SType::INT;
    if ((a == SType::INT || a == SType::BOOL) && (b == SType::INT || b == SType::BOOL)) return SType::INT;
    return SType::INVALID;
}

static bool CondAllowed(SType t) {
    return t == SType::STRING || IsOriginalNumeric(t) || t == SType::BOOL || t == SType::ARRAY || t == SType::MAP || t == SType::ANY;
}

SType TypeOfExpr(const Expr* e, TypeEnv& tenv) {
    if (!e) return SType::ANY;
    if (dynamic_cast<const IntExpr*>(e)) return SType::INT;
    if (dynamic_cast<const DoubleExpr*>(e)) return SType::DOUBLE;
    if (dynamic_cast<const StrExpr*>(e)) return SType::STRING;
    if (dynamic_cast<const BoolExpr*>(e)) return SType::BOOL;

    if (auto te = dynamic_cast<const TernaryExpr*>(e)) {
        SType c = TypeOfExpr(te->cond.get(), tenv);
        if (!CondAllowed(c)) throw Core_TypeError(e->line, e->col, L"'?:' uclu kosul operatorunde kosul sayi, laf veya mantik olmali.");
        SType t1 = TypeOfExpr(te->thenE.get(), tenv);
        SType t2 = TypeOfExpr(te->elseE.get(), tenv);
        if (t1 == t2) return t1;
        if (IsOriginalNumeric(t1) && IsOriginalNumeric(t2)) return PromoteNumeric(t1, t2);
        return SType::ANY;
    }

    if (auto aa = dynamic_cast<const ArrayAccessExpr*>(e)) {
        TypeSlot ts = tenv.getSlot(aa->name);
        if (ts.t == SType::INVALID) throw Core_TypeError(e->line, e->col, L"Tanimlanmamis dizi veya sozluk: " + aa->name);
        if (ts.t == SType::MAP) {
            (void)TypeOfExpr(aa->index.get(), tenv);
            return ts.elemType;
        }
        if (ts.t == SType::STRING) {
            SType idxT = TypeOfExpr(aa->index.get(), tenv);
            if (!IsOriginalNumeric(idxT)) throw Core_TypeError(e->line, e->col, L"Laf indeksi tamsayi olmalidir.");
            return SType::STRING;
        }
        if (ts.t != SType::ARRAY && ts.t != SType::ANY) throw Core_TypeError(e->line, e->col, aa->name + L" bir dizi_gari veya sozluk_gari degil.");
        SType idxT = TypeOfExpr(aa->index.get(), tenv);
        if (!IsOriginalNumeric(idxT)) throw Core_TypeError(e->line, e->col, L"Dizi indeksi tamsayi olmalidir.");
        return ts.elemType;
    }

    if (auto fa = dynamic_cast<const FieldAccessExpr*>(e)) {
        TypeSlot ts = tenv.getSlot(fa->objName);
        if (ts.t == SType::INVALID) throw Core_TypeError(e->line, e->col, L"Tanimlanmamis nesne: " + fa->objName);
        if (ts.t != SType::STRUCT && ts.t != SType::ANY) throw Core_TypeError(e->line, e->col, fa->objName + L" bir kalip_gari nesnesi degil.");
        auto sit = tenv.structs.find(ts.structName);
        if (sit != tenv.structs.end()) {
            auto fit = sit->second.find(fa->fieldName);
            if (fit == sit->second.end()) {
                throw Core_TypeError(e->line, e->col, ts.structName + L" kalibinda '" + fa->fieldName + L"' diye bir alan yok.");
            }
            return fit->second;
        }
        return SType::ANY;
    }

    if (auto n = dynamic_cast<const NotExpr*>(e)) {
        SType t = TypeOfExpr(n->expr.get(), tenv);
        if (!CondAllowed(t)) throw Core_TypeError(e->line, e->col, L"'!' / 'degil_gari' operatoru sadece sayi, laf veya mantik tipinde kullanilabilir.");
        return SType::BOOL;
    }
    if (auto v = dynamic_cast<const VarExpr*>(e)) {
        TypeSlot ts = tenv.getSlot(v->name);
        if (ts.t == SType::INVALID) throw Core_TypeError(e->line, e->col, L"Tanimlanmamis degisken: " + v->name);
        return ts.t;
    }
    if (auto c = dynamic_cast<const CallExpr*>(e)) {
        auto ensureCount = [&](size_t want, const wchar_t* sig) {
            if (c->args.size() != want) throw Core_TypeError(e->line, e->col, std::wstring(sig) + L" dogru arguman sayisi ile cagrilmali.");
            };
        auto requireNumeric = [&](size_t idx, const wchar_t* fname) -> SType {
            SType t = TypeOfExpr(c->args[idx].get(), tenv);
            if (!IsOriginalNumeric(t)) throw Core_TypeError(e->line, e->col, std::wstring(fname) + L" sadece sayi alir.");
            return t;
            };

        if (c->name == L"oku_gari") { ensureCount(0, L"oku_gari()"); return SType::STRING; }
        if (c->name == L"oku_tamsayi") { ensureCount(0, L"oku_tamsayi()"); return SType::INT; }
        if (c->name == L"oku_hassas_sayi") { ensureCount(0, L"oku_hassas_sayi()"); return SType::FLOAT; }
        if (c->name == L"pi_sayisi") { ensureCount(0, L"pi_sayisi()"); return SType::DOUBLE; }
        if (c->name == L"e_sayisi") { ensureCount(0, L"e_sayisi()"); return SType::DOUBLE; }
        if (c->name == L"kok") { ensureCount(1, L"kok(x)"); requireNumeric(0, L"kok"); return SType::DOUBLE; }
        if (c->name == L"us") { ensureCount(2, L"us(a, b)"); requireNumeric(0, L"us"); requireNumeric(1, L"us"); return SType::DOUBLE; }
        if (c->name == L"mutlak") { ensureCount(1, L"mutlak(x)"); return requireNumeric(0, L"mutlak"); }
        if (c->name == L"taban") { ensureCount(1, L"taban(x)"); return requireNumeric(0, L"taban"); }
        if (c->name == L"tavan") { ensureCount(1, L"tavan(x)"); return requireNumeric(0, L"tavan"); }
        if (c->name == L"yuvarla") { ensureCount(1, L"yuvarla(x)"); return requireNumeric(0, L"yuvarla"); }
        if (c->name == L"min_gari") { ensureCount(2, L"min_gari(a, b)"); SType a_t = requireNumeric(0, L"min_gari"), b_t = requireNumeric(1, L"min_gari"); return PromoteNumeric(a_t, b_t); }
        if (c->name == L"max_gari") { ensureCount(2, L"max_gari(a, b)"); SType a_t = requireNumeric(0, L"max_gari"), b_t = requireNumeric(1, L"max_gari"); return PromoteNumeric(a_t, b_t); }
        if (c->name == L"sinus") { ensureCount(1, L"sinus(x)"); requireNumeric(0, L"sinus"); return SType::DOUBLE; }
        if (c->name == L"kosinus") { ensureCount(1, L"kosinus(x)"); requireNumeric(0, L"kosinus"); return SType::DOUBLE; }
        if (c->name == L"tanjant") { ensureCount(1, L"tanjant(x)"); requireNumeric(0, L"tanjant"); return SType::DOUBLE; }
        if (c->name == L"cotanjant") { ensureCount(1, L"cotanjant(x)"); requireNumeric(0, L"cotanjant"); return SType::DOUBLE; }
        if (c->name == L"ln") { ensureCount(1, L"ln(x)"); requireNumeric(0, L"ln"); return SType::DOUBLE; }
        if (c->name == L"log10") { ensureCount(1, L"log10(x)"); requireNumeric(0, L"log10"); return SType::DOUBLE; }
        if (c->name == L"ustel") { ensureCount(1, L"ustel(x)"); requireNumeric(0, L"ustel"); return SType::DOUBLE; }

        // 🌟 YENİ: Yeni Ege ağzı yerleşik fonksiyonların tip denetimi
        if (c->name == L"boyut_gari") {
            ensureCount(1, L"boyut_gari(x)");
            SType t = TypeOfExpr(c->args[0].get(), tenv);
            if (t != SType::ARRAY && t != SType::STRING && t != SType::MAP && t != SType::ANY)
                throw Core_TypeError(e->line, e->col, L"boyut_gari(x) sadece dizi_gari, sozluk_gari veya laf alir.");
            return SType::INT;
        }
        if (c->name == L"ekle_gari") {
            ensureCount(2, L"ekle_gari(dizi, eleman)");
            SType t = TypeOfExpr(c->args[0].get(), tenv);
            if (t != SType::ARRAY && t != SType::ANY)
                throw Core_TypeError(e->line, e->col, L"ekle_gari ilk arguman olarak dizi_gari bekler.");
            (void)TypeOfExpr(c->args[1].get(), tenv);
            return SType::INT;
        }
        if (c->name == L"cikar_gari") {
            ensureCount(1, L"cikar_gari(dizi)");
            SType t = TypeOfExpr(c->args[0].get(), tenv);
            if (t != SType::ARRAY && t != SType::ANY)
                throw Core_TypeError(e->line, e->col, L"cikar_gari arguman olarak dizi_gari bekler.");
            return SType::ANY;
        }
        if (c->name == L"rastgele_gari") {
            ensureCount(2, L"rastgele_gari(min, max)");
            requireNumeric(0, L"rastgele_gari");
            requireNumeric(1, L"rastgele_gari");
            return SType::INT;
        }
        if (c->name == L"lafa_cevir") { ensureCount(1, L"lafa_cevir(x)"); (void)TypeOfExpr(c->args[0].get(), tenv); return SType::STRING; }
        if (c->name == L"tamsayiya_cevir") { ensureCount(1, L"tamsayiya_cevir(x)"); (void)TypeOfExpr(c->args[0].get(), tenv); return SType::INT; }
        if (c->name == L"hassasa_cevir") { ensureCount(1, L"hassasa_cevir(x)"); (void)TypeOfExpr(c->args[0].get(), tenv); return SType::DOUBLE; }
        if (c->name == L"buyut_gari") { ensureCount(1, L"buyut_gari(laf)"); (void)TypeOfExpr(c->args[0].get(), tenv); return SType::STRING; }
        if (c->name == L"kucult_gari") { ensureCount(1, L"kucult_gari(laf)"); (void)TypeOfExpr(c->args[0].get(), tenv); return SType::STRING; }
        if (c->name == L"parcala_gari") {
            ensureCount(3, L"parcala_gari(laf, baslangic, uzunluk)");
            (void)TypeOfExpr(c->args[0].get(), tenv);
            requireNumeric(1, L"parcala_gari");
            requireNumeric(2, L"parcala_gari");
            return SType::STRING;
        }
        if (c->name == L"icinde_var_mi") {
            ensureCount(2, L"icinde_var_mi(kaynak, aranan)");
            (void)TypeOfExpr(c->args[0].get(), tenv);
            (void)TypeOfExpr(c->args[1].get(), tenv);
            return SType::BOOL;
        }
        if (c->name == L"sirala_gari") {
            ensureCount(1, L"sirala_gari(dizi)");
            SType t = TypeOfExpr(c->args[0].get(), tenv);
            if (t != SType::ARRAY && t != SType::ANY) throw Core_TypeError(e->line, e->col, L"sirala_gari sadece dizi_gari alir.");
            return SType::ARRAY;
        }
        if (c->name == L"ters_cevir_gari") {
            ensureCount(1, L"ters_cevir_gari(x)");
            SType t = TypeOfExpr(c->args[0].get(), tenv);
            if (t != SType::ARRAY && t != SType::STRING && t != SType::ANY) throw Core_TypeError(e->line, e->col, L"ters_cevir_gari sadece dizi_gari veya laf alir.");
            return t;
        }
        if (c->name == L"degistir_gari") {
            ensureCount(3, L"degistir_gari(laf, eski, yeni)");
            (void)TypeOfExpr(c->args[0].get(), tenv);
            (void)TypeOfExpr(c->args[1].get(), tenv);
            (void)TypeOfExpr(c->args[2].get(), tenv);
            return SType::STRING;
        }
        if (c->name == L"kirp_gari") {
            ensureCount(1, L"kirp_gari(laf)");
            (void)TypeOfExpr(c->args[0].get(), tenv);
            return SType::STRING;
        }
        if (c->name == L"dosya_yaz_gari") {
            ensureCount(2, L"dosya_yaz_gari(yol, icerik)");
            (void)TypeOfExpr(c->args[0].get(), tenv);
            (void)TypeOfExpr(c->args[1].get(), tenv);
            return SType::BOOL;
        }
        if (c->name == L"dosya_oku_gari") {
            ensureCount(1, L"dosya_oku_gari(yol)");
            (void)TypeOfExpr(c->args[0].get(), tenv);
            return SType::STRING;
        }
        if (c->name == L"dosya_var_mi") {
            ensureCount(1, L"dosya_var_mi(yol)");
            (void)TypeOfExpr(c->args[0].get(), tenv);
            return SType::BOOL;
        }
        if (c->name == L"bekle_gari") {
            ensureCount(1, L"bekle_gari(ms)");
            requireNumeric(0, L"bekle_gari");
            return SType::INT;
        }
        if (c->name == L"vakit_gari") {
            ensureCount(0, L"vakit_gari()");
            return SType::INT;
        }
        if (c->name == L"anahtarlar_gari") {
            ensureCount(1, L"anahtarlar_gari(sozluk)");
            SType t = TypeOfExpr(c->args[0].get(), tenv);
            if (t != SType::MAP && t != SType::ANY) throw Core_TypeError(e->line, e->col, L"anahtarlar_gari sadece sozluk_gari alir.");
            return SType::ARRAY;
        }
        if (c->name == L"sil_gari") {
            ensureCount(2, L"sil_gari(sozluk, anahtar)");
            SType t = TypeOfExpr(c->args[0].get(), tenv);
            if (t != SType::MAP && t != SType::ANY) throw Core_TypeError(e->line, e->col, L"sil_gari ilk arguman olarak sozluk_gari bekler.");
            (void)TypeOfExpr(c->args[1].get(), tenv);
            return SType::BOOL;
        }

        // Kullanıcı tanımlı fonksiyon kontrolü
        auto fit = tenv.funcs.find(c->name);
        if (fit != tenv.funcs.end()) {
            const FuncSig& sig = fit->second;
            if (c->args.size() < sig.minArgs || c->args.size() > sig.params.size()) {
                throw Core_TypeError(e->line, e->col, c->name + L" fonksiyonu " + std::to_wstring(sig.minArgs) + L" ile " + std::to_wstring(sig.params.size()) + L" arasi arguman bekliyor, " + std::to_wstring(c->args.size()) + L" verildi.");
            }
            for (size_t i = 0; i < c->args.size(); ++i) {
                SType argT = TypeOfExpr(c->args[i].get(), tenv);
                if (!tenv.assignable(sig.params[i].t, argT)) {
                    throw Core_TypeError(e->line, e->col, c->name + L" fonksiyonunun " + std::to_wstring(i + 1) + L". argumaninda tur uyusmazligi.");
                }
            }
            return sig.returnType;
        }

        throw Core_TypeError(e->line, e->col, L"Tanimlanmamis fonksiyon cagrisi: " + c->name);
    }
    if (auto be = dynamic_cast<const BinExpr*>(e)) {
        if (be->op == BinOp::AndAnd || be->op == BinOp::OrOr) {
            SType A = TypeOfExpr(be->a.get(), tenv), B = TypeOfExpr(be->b.get(), tenv);
            if (!CondAllowed(A) || !CondAllowed(B)) throw Core_TypeError(e->line, e->col, L"&&/|| (hem_de/ya_da) icin kosullar sayi, laf veya mantik olmali");
            return SType::BOOL;
        }
        SType A = TypeOfExpr(be->a.get(), tenv), B = TypeOfExpr(be->b.get(), tenv);
        switch (be->op) {
        case BinOp::Add:
            if (A == SType::STRING || B == SType::STRING) return SType::STRING;
            if (!IsOriginalNumeric(A) || !IsOriginalNumeric(B)) throw Core_TypeError(e->line, e->col, L"'+' icin sayi veya laf bekleniyordu");
            return PromoteNumeric(A, B);
        case BinOp::Sub: case BinOp::Mul: case BinOp::Div: case BinOp::Mod:
            if (!IsOriginalNumeric(A) || !IsOriginalNumeric(B)) throw Core_TypeError(e->line, e->col, L"Aritmetik icin sayi bekleniyordu");
            return PromoteNumeric(A, B);
        case BinOp::Eq: case BinOp::Ne: case BinOp::Lt: case BinOp::Gt: case BinOp::Le: case BinOp::Ge:
            return SType::BOOL;
        default: return SType::ANY;
        }
    }
    return SType::ANY;
}

void TypeCheckBlock(const BlockStmt* b, TypeEnv& tenv) {
    tenv.push(); for (auto& st : b->stmts) TypeCheckStmt(st.get(), tenv); tenv.pop();
}

void TypeCheckStmt(const Stmt* s, TypeEnv& tenv) {
    if (!s) return;
    if (auto b = dynamic_cast<const BlockStmt*>(s)) { TypeCheckBlock(b, tenv); return; }
    if (dynamic_cast<const BreakStmt*>(s)) return;
    if (dynamic_cast<const ContinueStmt*>(s)) return;
    if (dynamic_cast<const StructDeclStmt*>(s)) return;

    if (auto svd = dynamic_cast<const StructVarDeclStmt*>(s)) {
        if (tenv.structs.find(svd->structName) == tenv.structs.end()) {
            throw Core_TypeError(svd->line, svd->col, L"Bilinmeyen kalip_gari tipi: " + svd->structName);
        }
        tenv.declare(svd->varName, SType::STRUCT, false, SType::ANY, svd->structName);
        return;
    }

    if (auto fa = dynamic_cast<const FieldAssignStmt*>(s)) {
        TypeSlot ts = tenv.getSlot(fa->objName);
        if (ts.t == SType::INVALID) throw Core_TypeError(fa->line, fa->col, L"Tanimlanmamis nesne: " + fa->objName);
        if (ts.t != SType::STRUCT && ts.t != SType::ANY) throw Core_TypeError(fa->line, fa->col, fa->objName + L" bir kalip_gari nesnesi degil.");
        SType valT = TypeOfExpr(fa->value.get(), tenv);
        auto sit = tenv.structs.find(ts.structName);
        if (sit != tenv.structs.end()) {
            auto fit = sit->second.find(fa->fieldName);
            if (fit == sit->second.end()) {
                throw Core_TypeError(fa->line, fa->col, ts.structName + L" kalibinda '" + fa->fieldName + L"' diye bir alan yok.");
            }
            if (!tenv.assignable(fit->second, valT)) {
                throw Core_TypeError(fa->line, fa->col, L"Kalip alani atamasinda tur uyusmazligi: " + fa->objName + L"." + fa->fieldName);
            }
        }
        return;
    }

    if (auto es = dynamic_cast<const ExprStmt*>(s)) {
        (void)TypeOfExpr(es->expr.get(), tenv);
        return;
    }

    if (auto vd = dynamic_cast<const VarDeclStmt*>(s)) {
        SType to = VTypeToSType(vd->declaredType);
        SType from = vd->init ? TypeOfExpr(vd->init.get(), tenv) : to;
        if (!tenv.assignable(to, from))
            throw Core_TypeError(vd->line, vd->col, std::wstring(L"Degisken tur uyusmazligi: ") + vd->name + L" beklenen=" + STypeName(to) + L", gelen=" + STypeName(from));
        tenv.declare(vd->name, to, vd->isConst); return;
    }
    if (auto ad = dynamic_cast<const ArrayDeclStmt*>(s)) {
        SType elemT = VTypeToSType(ad->elementType);
        if (ad->sizeExpr) {
            SType szT = TypeOfExpr(ad->sizeExpr.get(), tenv);
            if (!IsOriginalNumeric(szT)) throw Core_TypeError(ad->line, ad->col, L"Dizi boyutu sayi olmalidir.");
        }
        for (auto& ie : ad->initElements) {
            SType it = TypeOfExpr(ie.get(), tenv);
            if (!tenv.assignable(elemT, it)) {
                throw Core_TypeError(ad->line, ad->col, L"Dizi baslangic elemaninda tur uyusmazligi: " + ad->name);
            }
        }
        tenv.declare(ad->name, SType::ARRAY, false, elemT); return;
    }
    if (auto md = dynamic_cast<const MapDeclStmt*>(s)) {
        SType valT = VTypeToSType(md->valueType);
        for (auto& kv : md->initEntries) {
            (void)TypeOfExpr(kv.first.get(), tenv);
            SType vt = TypeOfExpr(kv.second.get(), tenv);
            if (!tenv.assignable(valT, vt)) {
                throw Core_TypeError(md->line, md->col, L"Sozluk baslangic degerinde tur uyusmazligi: " + md->name);
            }
        }
        tenv.declare(md->name, SType::MAP, false, valT);
        return;
    }
    if (auto aa = dynamic_cast<const ArrayAssignStmt*>(s)) {
        TypeSlot ts = tenv.getSlot(aa->name);
        if (ts.t == SType::INVALID) throw Core_TypeError(aa->line, aa->col, L"Tanimlanmamis dizi veya sozluge atama: " + aa->name);
        if (ts.t == SType::MAP) {
            (void)TypeOfExpr(aa->index.get(), tenv);
            SType valT = TypeOfExpr(aa->value.get(), tenv);
            if (!tenv.assignable(ts.elemType, valT)) {
                throw Core_TypeError(aa->line, aa->col, L"Sozluk deger atamasinda tur uyusmazligi: " + aa->name);
            }
            return;
        }
        if (ts.t != SType::ARRAY && ts.t != SType::ANY) throw Core_TypeError(aa->line, aa->col, aa->name + L" bir dizi_gari veya sozluk_gari degil.");
        SType idxT = TypeOfExpr(aa->index.get(), tenv);
        if (!IsOriginalNumeric(idxT)) throw Core_TypeError(aa->line, aa->col, L"Dizi indeksi sayi olmalidir.");
        SType valT = TypeOfExpr(aa->value.get(), tenv);
        if (!tenv.assignable(ts.elemType, valT)) {
            throw Core_TypeError(aa->line, aa->col, L"Dizi eleman atamasinda tur uyusmazligi: " + aa->name + L" beklenen=" + STypeName(ts.elemType) + L", gelen=" + STypeName(valT));
        }
        return;
    }

    // 🌟 Kullanıcı Tanımlı Fonksiyon Bildirimi Tip Denetimi
    if (auto fd = dynamic_cast<const FuncDeclStmt*>(s)) {
        SType prevRet = tenv.currentFuncReturn;
        tenv.currentFuncReturn = VTypeToSType(fd->returnType);
        tenv.push(); // Fonksiyonun parametreleri için yeni bir yerel kapsam açılır
        for (auto& p : fd->params) {
            tenv.declare(p.name, VTypeToSType(p.type), false, VTypeToSType(p.elemType));
        }
        if (fd->body) TypeCheckBlock(fd->body.get(), tenv);
        tenv.pop(); // Kapsam güvenle kapatılır
        tenv.currentFuncReturn = prevRet;
        return;
    }

    if (auto as = dynamic_cast<const AssignStmt*>(s)) {
        TypeSlot ts = tenv.getSlot(as->name);
        if (ts.t == SType::INVALID) throw Core_TypeError(as->line, as->col, L"Tanimlanmamis degiskene atama: " + as->name);
        if (ts.isConst) throw Core_TypeError(as->line, as->col, L"sabit degisken degistirilemez: " + as->name);
        SType v = TypeOfExpr(as->value.get(), tenv);
        if (!tenv.assignable(ts.t, v))
            throw Core_TypeError(as->line, as->col, std::wstring(L"Atama tur uyusmazligi: ") + as->name + L" beklenen=" + STypeName(ts.t) + L", gelen=" + STypeName(v));
        return;
    }
    if (auto pr = dynamic_cast<const PrintStmt*>(s)) {
        SType t = TypeOfExpr(pr->expr.get(), tenv);
        if (t == SType::VOID) throw Core_TypeError(pr->line, pr->col, L"bos_gari donen bir fonksiyon doktur() icinde yazdirilamaz.");
        return;
    }
    if (auto iff = dynamic_cast<const IfStmt*>(s)) {
        SType c = TypeOfExpr(iff->cond.get(), tenv);
        if (!CondAllowed(c)) throw Core_TypeError(iff->line, iff->col, L"sayet kosulu sayi, laf veya mantik olmali");
        TypeCheckStmt(iff->thenS.get(), tenv); if (iff->elseS) TypeCheckStmt(iff->elseS.get(), tenv); return;
    }
    if (auto sw = dynamic_cast<const SwitchStmt*>(s)) {
        SType t = TypeOfExpr(sw->target.get(), tenv);
        if (t == SType::VOID) throw Core_TypeError(sw->line, sw->col, L"bak_hele hedefi bos_gari olamaz");
        for (auto& br : sw->branches) {
            if (br.matchValue) (void)TypeOfExpr(br.matchValue.get(), tenv);
            tenv.push();
            for (auto& st : br.stmts) TypeCheckStmt(st.get(), tenv);
            tenv.pop();
        }
        return;
    }
    if (auto wh = dynamic_cast<const WhiteStmt*>(s)) {
        SType c = TypeOfExpr(wh->cond.get(), tenv);
        if (!CondAllowed(c)) throw Core_TypeError(wh->line, wh->col, L"oldukca kosulu sayi, laf veya mantik olmali");
        TypeCheckStmt(wh->body.get(), tenv); return;
    }
    if (auto dw = dynamic_cast<const DoWhileStmt*>(s)) {
        SType c = TypeOfExpr(dw->cond.get(), tenv);
        if (!CondAllowed(c)) throw Core_TypeError(dw->line, dw->col, L"bi_yap_hele kosulu sayi, laf veya mantik olmali");
        TypeCheckStmt(dw->body.get(), tenv); return;
    }
    if (auto fr = dynamic_cast<const ForStmt*>(s)) {
        tenv.push(); if (fr->init) TypeCheckStmt(fr->init.get(), tenv);
        if (fr->cond) { SType c = TypeOfExpr(fr->cond.get(), tenv); if (!CondAllowed(c)) throw Core_TypeError(fr->line, fr->col, L"cevir_gari kosulu sayi, laf veya mantik olmali"); }
        if (fr->step) TypeCheckStmt(fr->step.get(), tenv); TypeCheckStmt(fr->body.get(), tenv); tenv.pop(); return;
    }
    if (auto fe = dynamic_cast<const ForeachStmt*>(s)) {
        SType itT = TypeOfExpr(fe->iterable.get(), tenv);
        if (itT != SType::ARRAY && itT != SType::STRING && itT != SType::MAP && itT != SType::ANY) {
            throw Core_TypeError(fe->line, fe->col, L"gez_gari sadece dizi_gari, sozluk_gari veya laf uzerinde gezebilir.");
        }
        tenv.push();
        tenv.declare(fe->varName, VTypeToSType(fe->varType), false);
        TypeCheckStmt(fe->body.get(), tenv);
        tenv.pop();
        return;
    }
    if (auto tc = dynamic_cast<const TryCatchStmt*>(s)) {
        TypeCheckStmt(tc->tryBody.get(), tenv);
        tenv.push();
        tenv.declare(tc->errVarName, SType::STRING, false);
        TypeCheckStmt(tc->catchBody.get(), tenv);
        tenv.pop();
        return;
    }
    if (auto th = dynamic_cast<const ThrowStmt*>(s)) {
        if (th->expr) (void)TypeOfExpr(th->expr.get(), tenv);
        return;
    }
    if (auto rt = dynamic_cast<const ReturnStmt*>(s)) {
        if (rt->value) {
            SType v = TypeOfExpr(rt->value.get(), tenv);
            if (tenv.currentFuncReturn == SType::VOID) {
                throw Core_TypeError(rt->line, rt->col, L"bos_gari fonksiyon deger donduremez.");
            }
            if (tenv.currentFuncReturn != SType::ANY && !tenv.assignable(tenv.currentFuncReturn, v)) {
                throw Core_TypeError(rt->line, rt->col, L"dondur_gari tur uyusmazligi: beklenen=" + std::wstring(STypeName(tenv.currentFuncReturn)) + L", gelen=" + STypeName(v));
            }
        }
        return;
    }
}

void TypeCheckProgram(const Program& p, TypeEnv& tenv) {
    // 1. Önce tüm kalip_gari (struct) tanımlarını kaydet
    for (auto& sd : p.structs) {
        std::unordered_map<std::wstring, SType> fmap;
        for (auto& f : sd->fields) {
            fmap[f.name] = VTypeToSType(f.type);
        }
        tenv.structs[sd->name] = std::move(fmap);
    }

    // 2. Küresel değişkenleri ve sabitleri global kapsama ekle
    for (auto& g : p.globals) {
        TypeCheckStmt(g.get(), tenv);
    }

    // 3. Tüm fonksiyon imzalarını kaydet (böylece fonksiyonlar birbirini ve kendilerini/recursion çağırabilir)
    for (auto& f : p.functions) {
        FuncSig sig;
        sig.returnType = VTypeToSType(f->returnType);
        sig.minArgs = 0;
        for (auto& prm : f->params) {
            SType pt = VTypeToSType(prm.type);
            if (prm.defaultVal) {
                SType defT = TypeOfExpr(prm.defaultVal.get(), tenv);
                if (!tenv.assignable(pt, defT)) {
                    throw Core_TypeError(f->line, f->col, f->name + L" fonksiyonunun '" + prm.name + L"' parametresindeki varsayilan degerde tur uyusmazligi.");
                }
            } else {
                sig.minArgs++;
            }
            sig.params.push_back(TypeSlot{ pt, false, VTypeToSType(prm.elemType) });
        }
        tenv.funcs[f->name] = std::move(sig);
    }

    // 4. Fonksiyon gövdelerini ve basla_gari bloğunu denetle
    for (auto& f : p.functions) {
        TypeCheckStmt(f.get(), tenv);
    }
    tenv.currentFuncReturn = SType::INT;
    TypeCheckBlock(p.main.get(), tenv);
}