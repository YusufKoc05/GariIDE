#include "value.h"
#include "core.h"
#include <sstream>
#include <iomanip>
#include <cmath>
#include <stdexcept>
#include <algorithm>
#include <random>
#include <cwctype>
#include <fstream>
#include <chrono>
#include <thread>
#include <windows.h>
#ifdef VOID
#undef VOID
#endif

#ifndef GARI_PI
#define GARI_PI 3.14159265358979323846
#endif
#ifndef GARI_E
#define GARI_E  2.71828182845904523536
#endif

std::wstring TrimTrailingZeros(std::wstring s) {
    auto pos = s.find(L'.');
    if (pos == std::wstring::npos) return s;
    while (!s.empty() && s.back() == L'0') s.pop_back();
    if (!s.empty() && s.back() == L'.') s.pop_back();
    if (s.empty()) return L"0";
    return s;
}

std::wstring FormatDouble(double v) {
    std::wostringstream oss;
    oss << std::fixed << std::setprecision(6) << v;
    return TrimTrailingZeros(oss.str());
}

std::wstring ValueToString(const Value& v) {
    switch (v.type) {
    case Value::Type::VOID:   return L"bos_gari";
    case Value::Type::INT:    return std::to_wstring(v.iVal);
    case Value::Type::FLOAT:  return FormatDouble((double)v.fVal);
    case Value::Type::DOUBLE: return FormatDouble(v.dVal);
    case Value::Type::STRING: return v.sVal;
    case Value::Type::BOOL:   return v.bVal ? L"hakli" : L"haksiz";
    case Value::Type::ARRAY: {
        if (!v.arrVal) return L"[]";
        std::wostringstream oss;
        oss << L"[";
        for (size_t i = 0; i < v.arrVal->size(); ++i) {
            if (i > 0) oss << L", ";
            const Value& item = (*v.arrVal)[i];
            if (item.type == Value::Type::STRING) oss << L"\"" << item.sVal << L"\"";
            else oss << ValueToString(item);
        }
        oss << L"]";
        return oss.str();
    }
    case Value::Type::STRUCT: {
        std::wostringstream oss;
        oss << v.structName << L"{";
        if (v.fields) {
            bool first = true;
            for (const auto& kv : *v.fields) {
                if (!first) oss << L", ";
                first = false;
                oss << kv.first << L": " << ValueToString(kv.second);
            }
        }
        oss << L"}";
        return oss.str();
    }
    case Value::Type::MAP: {
        std::wostringstream oss;
        oss << L"[";
        if (v.mapVal) {
            bool first = true;
            for (const auto& kv : *v.mapVal) {
                if (!first) oss << L", ";
                first = false;
                oss << L"\"" << kv.first << L"\": ";
                if (kv.second.type == Value::Type::STRING) oss << L"\"" << kv.second.sVal << L"\"";
                else oss << ValueToString(kv.second);
            }
        }
        oss << L"]";
        return oss.str();
    }
    default: return L"";
    }
}

bool IsNumeric(Value::Type t) {
    return t == Value::Type::INT || t == Value::Type::FLOAT || t == Value::Type::DOUBLE || t == Value::Type::BOOL;
}

double ToDouble(const Value& v) {
    if (v.type == Value::Type::DOUBLE) return v.dVal;
    if (v.type == Value::Type::FLOAT)  return (double)v.fVal;
    if (v.type == Value::Type::INT)    return (double)v.iVal;
    if (v.type == Value::Type::BOOL)   return v.bVal ? 1.0 : 0.0;
    try { return std::stod(v.sVal); }
    catch (...) { return 0.0; }
}

float ToFloat(const Value& v) {
    if (v.type == Value::Type::FLOAT)  return v.fVal;
    if (v.type == Value::Type::DOUBLE) return (float)v.dVal;
    if (v.type == Value::Type::INT)    return (float)v.iVal;
    if (v.type == Value::Type::BOOL)   return v.bVal ? 1.0f : 0.0f;
    try { return (float)std::stod(v.sVal); }
    catch (...) { return 0.0f; }
}

int ToInt(const Value& v) {
    if (v.type == Value::Type::INT)    return v.iVal;
    if (v.type == Value::Type::FLOAT)  return (int)v.fVal;
    if (v.type == Value::Type::DOUBLE) return (int)v.dVal;
    if (v.type == Value::Type::BOOL)   return v.bVal ? 1 : 0;
    try { return std::stoi(v.sVal); }
    catch (...) { return 0; }
}

bool ToBool(const Value& v) {
    return IsTrue(v);
}

bool IsTrue(const Value& v) {
    if (v.type == Value::Type::VOID)   return false;
    if (v.type == Value::Type::STRING) return !v.sVal.empty();
    if (v.type == Value::Type::BOOL)   return v.bVal;
    if (v.type == Value::Type::ARRAY)  return v.arrVal != nullptr && !v.arrVal->empty();
    if (v.type == Value::Type::STRUCT) return v.fields != nullptr;
    if (v.type == Value::Type::MAP)    return v.mapVal != nullptr && !v.mapVal->empty();
    return ToInt(v) != 0;
}

Value Coerce(const Value& v, Value::Type target) {
    if (v.type == target) return v;
    if (target == Value::Type::VOID)   return Value::makeVoid();
    if (target == Value::Type::ARRAY || target == Value::Type::STRUCT || target == Value::Type::MAP) return v;
    if (target == Value::Type::STRING) return Value::makeString(ValueToString(v));
    if (target == Value::Type::DOUBLE) return Value::makeDouble(ToDouble(v));
    if (target == Value::Type::FLOAT)  return Value::makeFloat(ToFloat(v));
    if (target == Value::Type::INT)    return Value::makeInt(ToInt(v));
    if (target == Value::Type::BOOL)   return Value::makeBool(IsTrue(v));
    return v;
}

double DegToRad(double deg) { return deg * GARI_PI / 180.0; }

bool ValueEquals(const Value& A, const Value& B) {
    if (A.type == Value::Type::STRING || B.type == Value::Type::STRING) return ValueToString(A) == ValueToString(B);
    if (A.type == Value::Type::BOOL || B.type == Value::Type::BOOL) return IsTrue(A) == IsTrue(B);
    if (A.type == Value::Type::ARRAY || B.type == Value::Type::ARRAY) return A.arrVal == B.arrVal;
    if (A.type == Value::Type::MAP || B.type == Value::Type::MAP) return A.mapVal == B.mapVal;
    if (IsNumeric(A.type) && IsNumeric(B.type)) return ToDouble(A) == ToDouble(B);
    return ValueToString(A) == ValueToString(B);
}

std::string ZeroDivMsg(const std::string& op, const Value& A, const Value& B) {
    return "Sifira bolme hatasi (" + op + "): " + Core_WToNarrow(ValueToString(A)) + " " + op + " " + Core_WToNarrow(ValueToString(B));
}

void TrimWSLocal(std::wstring& t) {
    while (!t.empty() && iswspace(t.front())) t.erase(t.begin());
    while (!t.empty() && iswspace(t.back()))  t.pop_back();
}

static const wchar_t* const kBuiltinNames[] = {
    L"oku_gari", L"oku_tamsayi", L"oku_hassas_sayi", L"kok", L"us", L"mutlak",
    L"taban", L"tavan", L"yuvarla", L"min_gari", L"max_gari", L"sinus",
    L"kosinus", L"tanjant", L"cotanjant", L"ln", L"log10", L"ustel", L"pi_sayisi", L"e_sayisi",
    L"boyut_gari", L"ekle_gari", L"cikar_gari", L"rastgele_gari",
    L"lafa_cevir", L"tamsayiya_cevir", L"hassasa_cevir",
    L"buyut_gari", L"kucult_gari", L"parcala_gari", L"icinde_var_mi",
    L"sirala_gari", L"ters_cevir_gari", L"degistir_gari", L"kirp_gari",
    L"dosya_yaz_gari", L"dosya_oku_gari", L"dosya_var_mi",
    L"bekle_gari", L"vakit_gari",
    L"anahtarlar_gari", L"sil_gari"
};

std::wstring SuggestBuiltinName(const std::wstring& name) {
    if (name.empty()) return L"";
    int bestScore = -1; std::wstring best;
    for (auto cand : kBuiltinNames) {
        std::wstring c = cand; int score = 0; size_t common = 0;
        while (common < name.size() && common < c.size() && name[common] == c[common]) common++;
        score += (int)common * 4;
        if (c.find(name) != std::wstring::npos || name.find(c) != std::wstring::npos) score += 6;
        if (!name.empty() && !c.empty() && name[0] == c[0]) score += 2;
        if (std::abs((int)name.size() - (int)c.size()) <= 2) score += 1;
        if (score > bestScore) { bestScore = score; best = c; }
    }
    if (bestScore < 4) return L""; return best;
}

std::string UnknownCallMsg(const std::wstring& name) {
    std::wstring hint = SuggestBuiltinName(name); std::wstring msg = L"Bilinmeyen cagri: " + name;
    if (!hint.empty()) msg += L". Belki sunu demek istedin: " + hint;
    return Core_WToNarrow(msg);
}

std::wstring UnknownCallTypeMsg(const std::wstring& name) {
    std::wstring hint = SuggestBuiltinName(name); std::wstring msg = L"Bilinmeyen cagri: " + name;
    if (!hint.empty()) msg += L". Belki sunu demek istedin: " + hint;
    return msg;
}

void ExpectBuiltinArgCount(const std::wstring& sig, size_t got, size_t want, int line, int col) {
    if (got != want) throw Core_RuntimeError(line, col, "ArgError", Core_WToNarrow(sig) + " dogru arguman sayisi ile cagrilmali.");
}

void ExpectNumericBuiltinArg(const std::wstring& name, const Value& v, int line, int col) {
    if (!IsNumeric(v.type)) throw Core_RuntimeError(line, col, "TypeError", Core_WToNarrow(name) + " sadece sayi alir.");
}

Value::Type PromoteValueType(Value::Type a, Value::Type b) {
    if (a == Value::Type::DOUBLE || b == Value::Type::DOUBLE) return Value::Type::DOUBLE;
    if (a == Value::Type::FLOAT || b == Value::Type::FLOAT) return Value::Type::FLOAT;
    return Value::Type::INT;
}

Value MakeNumericByType(Value::Type t, double v) {
    if (t == Value::Type::DOUBLE) return Value::makeDouble(v);
    if (t == Value::Type::FLOAT)  return Value::makeFloat((float)v);
    return Value::makeInt((int)v);
}

Value EvalBuiltinCall(const std::wstring& name, const std::vector<Value>& args, int line, int col) {
    if (name == L"oku_gari") { ExpectBuiltinArgCount(L"oku_gari()", args.size(), 0, line, col); return Value::makeString(Core_ReadLineBlockingFromIDE()); }
    if (name == L"oku_tamsayi") {
        ExpectBuiltinArgCount(L"oku_tamsayi()", args.size(), 0, line, col); std::wstring s = Core_ReadLineBlockingFromIDE(); TrimWSLocal(s);
        if (s.empty()) throw Core_RuntimeError(line, col, "ValueError", "Tamsayi bekleniyordu.");
        try { return Value::makeInt(std::stoi(s)); }
        catch (...) { throw Core_RuntimeError(line, col, "ValueError", "Tamsayi bekleniyordu."); }
    }
    if (name == L"oku_hassas_sayi") {
        ExpectBuiltinArgCount(L"oku_hassas_sayi()", args.size(), 0, line, col); std::wstring s = Core_ReadLineBlockingFromIDE(); TrimWSLocal(s);
        if (s.empty()) throw Core_RuntimeError(line, col, "ValueError", "Hassas sayi bekleniyordu.");
        try { return Value::makeFloat((float)std::stod(s)); }
        catch (...) { throw Core_RuntimeError(line, col, "ValueError", "Hassas sayi bekleniyordu."); }
    }
    if (name == L"pi_sayisi") { ExpectBuiltinArgCount(L"pi_sayisi()", args.size(), 0, line, col); return Value::makeDouble(GARI_PI); }
    if (name == L"e_sayisi") { ExpectBuiltinArgCount(L"e_sayisi()", args.size(), 0, line, col); return Value::makeDouble(GARI_E); }
    if (name == L"kok") {
        ExpectBuiltinArgCount(L"kok(x)", args.size(), 1, line, col); ExpectNumericBuiltinArg(name, args[0], line, col);
        double x = ToDouble(args[0]); if (x < 0.0) throw Core_RuntimeError(line, col, "MathError", "kok(x) negatif sayida kullanilamaz.");
        return Value::makeDouble(std::sqrt(x));
    }
    if (name == L"us") { ExpectBuiltinArgCount(L"us(a, b)", args.size(), 2, line, col); ExpectNumericBuiltinArg(name, args[0], line, col); ExpectNumericBuiltinArg(name, args[1], line, col); return Value::makeDouble(std::pow(ToDouble(args[0]), ToDouble(args[1]))); }
    if (name == L"mutlak") {
        ExpectBuiltinArgCount(L"mutlak(x)", args.size(), 1, line, col); ExpectNumericBuiltinArg(name, args[0], line, col);
        if (args[0].type == Value::Type::INT)    return Value::makeInt(std::abs(ToInt(args[0])));
        if (args[0].type == Value::Type::FLOAT)  return Value::makeFloat((float)std::fabs(ToFloat(args[0])));
        return Value::makeDouble(std::fabs(ToDouble(args[0])));
    }
    if (name == L"taban") {
        ExpectBuiltinArgCount(L"taban(x)", args.size(), 1, line, col); ExpectNumericBuiltinArg(name, args[0], line, col);
        if (args[0].type == Value::Type::INT)    return Value::makeInt(ToInt(args[0]));
        if (args[0].type == Value::Type::FLOAT)  return Value::makeFloat((float)std::floor(ToFloat(args[0])));
        return Value::makeDouble(std::floor(ToDouble(args[0])));
    }
    if (name == L"tavan") {
        ExpectBuiltinArgCount(L"tavan(x)", args.size(), 1, line, col); ExpectNumericBuiltinArg(name, args[0], line, col);
        if (args[0].type == Value::Type::INT)    return Value::makeInt(ToInt(args[0]));
        if (args[0].type == Value::Type::FLOAT)  return Value::makeFloat((float)std::ceil(ToFloat(args[0])));
        return Value::makeDouble(std::ceil(ToDouble(args[0])));
    }
    if (name == L"yuvarla") {
        ExpectBuiltinArgCount(L"yuvarla(x)", args.size(), 1, line, col); ExpectNumericBuiltinArg(name, args[0], line, col);
        if (args[0].type == Value::Type::INT)    return Value::makeInt(ToInt(args[0]));
        if (args[0].type == Value::Type::FLOAT)  return Value::makeFloat((float)std::round(ToFloat(args[0])));
        return Value::makeDouble(std::round(ToDouble(args[0])));
    }
    if (name == L"min_gari") { ExpectBuiltinArgCount(L"min_gari(a, b)", args.size(), 2, line, col); ExpectNumericBuiltinArg(name, args[0], line, col); ExpectNumericBuiltinArg(name, args[1], line, col); Value::Type rt = PromoteValueType(args[0].type, args[1].type); double x = ToDouble(args[0]), y = ToDouble(args[1]); return MakeNumericByType(rt, x < y ? x : y); }
    if (name == L"max_gari") { ExpectBuiltinArgCount(L"max_gari(a, b)", args.size(), 2, line, col); ExpectNumericBuiltinArg(name, args[0], line, col); ExpectNumericBuiltinArg(name, args[1], line, col); Value::Type rt = PromoteValueType(args[0].type, args[1].type); double x = ToDouble(args[0]), y = ToDouble(args[1]); return MakeNumericByType(rt, x > y ? x : y); }
    if (name == L"sinus") { ExpectBuiltinArgCount(L"sinus(x)", args.size(), 1, line, col); ExpectNumericBuiltinArg(name, args[0], line, col); return Value::makeDouble(std::sin(DegToRad(ToDouble(args[0])))); }
    if (name == L"kosinus") { ExpectBuiltinArgCount(L"kosinus(x)", args.size(), 1, line, col); ExpectNumericBuiltinArg(name, args[0], line, col); return Value::makeDouble(std::cos(DegToRad(ToDouble(args[0])))); }
    if (name == L"tanjant") { ExpectBuiltinArgCount(L"tanjant(x)", args.size(), 1, line, col); ExpectNumericBuiltinArg(name, args[0], line, col); double rad = DegToRad(ToDouble(args[0])), c = std::cos(rad); if (std::fabs(c) < 1e-12) throw Core_RuntimeError(line, col, "MathError", "tanjant(x) bu acida tanimsizdir."); return Value::makeDouble(std::tan(rad)); }
    if (name == L"cotanjant") { ExpectBuiltinArgCount(L"cotanjant(x)", args.size(), 1, line, col); ExpectNumericBuiltinArg(name, args[0], line, col); double rad = DegToRad(ToDouble(args[0])), s = std::sin(rad); if (std::fabs(s) < 1e-12) throw Core_RuntimeError(line, col, "MathError", "cotanjant(x) bu acida tanimsizdir."); return Value::makeDouble(std::cos(rad) / s); }
    if (name == L"ln") { ExpectBuiltinArgCount(L"ln(x)", args.size(), 1, line, col); ExpectNumericBuiltinArg(name, args[0], line, col); double x = ToDouble(args[0]); if (x <= 0.0) throw Core_RuntimeError(line, col, "MathError", "ln(x) icin x > 0 olmali."); return Value::makeDouble(std::log(x)); }
    if (name == L"log10") { ExpectBuiltinArgCount(L"log10(x)", args.size(), 1, line, col); ExpectNumericBuiltinArg(name, args[0], line, col); double x = ToDouble(args[0]); if (x <= 0.0) throw Core_RuntimeError(line, col, "MathError", "log10(x) icin x > 0 olmali."); return Value::makeDouble(std::log10(x)); }
    if (name == L"ustel") { ExpectBuiltinArgCount(L"ustel(x)", args.size(), 1, line, col); ExpectNumericBuiltinArg(name, args[0], line, col); return Value::makeDouble(std::exp(ToDouble(args[0]))); }

    // 🌟 YENİ: Dizi ve Laf Yardımcı Yerleşik Fonksiyonlar
    if (name == L"boyut_gari") {
        ExpectBuiltinArgCount(L"boyut_gari(x)", args.size(), 1, line, col);
        if (args[0].type == Value::Type::ARRAY) return Value::makeInt(args[0].arrVal ? (int)args[0].arrVal->size() : 0);
        if (args[0].type == Value::Type::STRING) return Value::makeInt((int)args[0].sVal.size());
        if (args[0].type == Value::Type::MAP) return Value::makeInt(args[0].mapVal ? (int)args[0].mapVal->size() : 0);
        throw Core_RuntimeError(line, col, "TypeError", "boyut_gari(x) sadece dizi_gari, sozluk_gari veya laf tipinde kullanilabilir.");
    }
    if (name == L"ekle_gari") {
        ExpectBuiltinArgCount(L"ekle_gari(dizi, eleman)", args.size(), 2, line, col);
        if (args[0].type != Value::Type::ARRAY || !args[0].arrVal) {
            throw Core_RuntimeError(line, col, "TypeError", "ekle_gari ilk arguman olarak dizi_gari bekler.");
        }
        Value elem = Coerce(args[1], args[0].elemType);
        args[0].arrVal->push_back(elem);
        return Value::makeInt((int)args[0].arrVal->size());
    }
    if (name == L"cikar_gari") {
        ExpectBuiltinArgCount(L"cikar_gari(dizi)", args.size(), 1, line, col);
        if (args[0].type != Value::Type::ARRAY || !args[0].arrVal) {
            throw Core_RuntimeError(line, col, "TypeError", "cikar_gari arguman olarak dizi_gari bekler.");
        }
        if (args[0].arrVal->empty()) {
            throw Core_RuntimeError(line, col, "IndexError", "Bos diziden eleman cikarilamaz (cikar_gari).");
        }
        Value last = args[0].arrVal->back();
        args[0].arrVal->pop_back();
        return last;
    }
    if (name == L"rastgele_gari") {
        ExpectBuiltinArgCount(L"rastgele_gari(min, max)", args.size(), 2, line, col);
        ExpectNumericBuiltinArg(name, args[0], line, col);
        ExpectNumericBuiltinArg(name, args[1], line, col);
        int lo = ToInt(args[0]), hi = ToInt(args[1]);
        if (lo > hi) std::swap(lo, hi);
        static thread_local std::mt19937 rng{ std::random_device{}() };
        std::uniform_int_distribution<int> dist(lo, hi);
        return Value::makeInt(dist(rng));
    }
    if (name == L"lafa_cevir") {
        ExpectBuiltinArgCount(L"lafa_cevir(x)", args.size(), 1, line, col);
        return Value::makeString(ValueToString(args[0]));
    }
    if (name == L"tamsayiya_cevir") {
        ExpectBuiltinArgCount(L"tamsayiya_cevir(x)", args.size(), 1, line, col);
        return Value::makeInt(ToInt(args[0]));
    }
    if (name == L"hassasa_cevir") {
        ExpectBuiltinArgCount(L"hassasa_cevir(x)", args.size(), 1, line, col);
        return Value::makeDouble(ToDouble(args[0]));
    }
    if (name == L"buyut_gari") {
        ExpectBuiltinArgCount(L"buyut_gari(laf)", args.size(), 1, line, col);
        std::wstring s = ValueToString(args[0]);
        for (auto& ch : s) ch = (wchar_t)std::towupper(ch);
        return Value::makeString(s);
    }
    if (name == L"kucult_gari") {
        ExpectBuiltinArgCount(L"kucult_gari(laf)", args.size(), 1, line, col);
        std::wstring s = ValueToString(args[0]);
        for (auto& ch : s) ch = (wchar_t)std::towlower(ch);
        return Value::makeString(s);
    }
    if (name == L"parcala_gari") {
        ExpectBuiltinArgCount(L"parcala_gari(laf, baslangic, uzunluk)", args.size(), 3, line, col);
        std::wstring s = ValueToString(args[0]);
        int start = ToInt(args[1]);
        int len = ToInt(args[2]);
        if (start < 0) start = 0;
        if (start >= (int)s.size() || len <= 0) return Value::makeString(L"");
        return Value::makeString(s.substr((size_t)start, (size_t)len));
    }
    if (name == L"icinde_var_mi") {
        ExpectBuiltinArgCount(L"icinde_var_mi(kaynak, aranan)", args.size(), 2, line, col);
        if (args[0].type == Value::Type::MAP && args[0].mapVal) {
            std::wstring key = ValueToString(args[1]);
            return Value::makeBool(args[0].mapVal->find(key) != args[0].mapVal->end());
        }
        if (args[0].type == Value::Type::ARRAY && args[0].arrVal) {
            for (const auto& item : *args[0].arrVal) {
                if (ValueEquals(item, args[1])) return Value::makeBool(true);
            }
            return Value::makeBool(false);
        }
        std::wstring s = ValueToString(args[0]);
        std::wstring needle = ValueToString(args[1]);
        return Value::makeBool(s.find(needle) != std::wstring::npos);
    }
    if (name == L"anahtarlar_gari") {
        ExpectBuiltinArgCount(L"anahtarlar_gari(sozluk)", args.size(), 1, line, col);
        if (args[0].type != Value::Type::MAP || !args[0].mapVal) {
            throw Core_RuntimeError(line, col, "TypeError", "anahtarlar_gari(sozluk) sadece sozluk_gari alir.");
        }
        std::vector<Value> keys;
        keys.reserve(args[0].mapVal->size());
        for (const auto& kv : *args[0].mapVal) {
            keys.push_back(Value::makeString(kv.first));
        }
        return Value::makeArrayFromList(std::move(keys), Value::Type::STRING);
    }
    if (name == L"sil_gari") {
        ExpectBuiltinArgCount(L"sil_gari(sozluk, anahtar)", args.size(), 2, line, col);
        if (args[0].type != Value::Type::MAP || !args[0].mapVal) {
            throw Core_RuntimeError(line, col, "TypeError", "sil_gari ilk arguman olarak sozluk_gari bekler.");
        }
        std::wstring key = ValueToString(args[1]);
        bool removed = args[0].mapVal->erase(key) > 0;
        return Value::makeBool(removed);
    }
    if (name == L"sirala_gari") {
        ExpectBuiltinArgCount(L"sirala_gari(dizi)", args.size(), 1, line, col);
        if (args[0].type != Value::Type::ARRAY || !args[0].arrVal) {
            throw Core_RuntimeError(line, col, "TypeError", "sirala_gari(dizi) sadece dizi_gari alir.");
        }
        auto& vec = *args[0].arrVal;
        std::sort(vec.begin(), vec.end(), [](const Value& a, const Value& b) {
            if (a.type == Value::Type::STRING || b.type == Value::Type::STRING) {
                return ValueToString(a) < ValueToString(b);
            }
            return ToDouble(a) < ToDouble(b);
        });
        return args[0];
    }
    if (name == L"ters_cevir_gari") {
        ExpectBuiltinArgCount(L"ters_cevir_gari(x)", args.size(), 1, line, col);
        if (args[0].type == Value::Type::ARRAY && args[0].arrVal) {
            std::reverse(args[0].arrVal->begin(), args[0].arrVal->end());
            return args[0];
        }
        std::wstring s = ValueToString(args[0]);
        std::reverse(s.begin(), s.end());
        return Value::makeString(s);
    }
    if (name == L"degistir_gari") {
        ExpectBuiltinArgCount(L"degistir_gari(laf, eski, yeni)", args.size(), 3, line, col);
        std::wstring s = ValueToString(args[0]);
        std::wstring from = ValueToString(args[1]);
        std::wstring to = ValueToString(args[2]);
        if (from.empty()) return Value::makeString(s);
        size_t start_pos = 0;
        while ((start_pos = s.find(from, start_pos)) != std::wstring::npos) {
            s.replace(start_pos, from.length(), to);
            start_pos += to.length();
        }
        return Value::makeString(s);
    }
    if (name == L"kirp_gari") {
        ExpectBuiltinArgCount(L"kirp_gari(laf)", args.size(), 1, line, col);
        std::wstring s = ValueToString(args[0]);
        TrimWSLocal(s);
        return Value::makeString(s);
    }
    if (name == L"dosya_yaz_gari") {
        ExpectBuiltinArgCount(L"dosya_yaz_gari(yol, icerik)", args.size(), 2, line, col);
        std::wstring path = ValueToString(args[0]);
        std::wstring content = ValueToString(args[1]);
        std::string utf8 = Core_WToNarrow(content);
        std::ofstream ofs(path, std::ios::binary | std::ios::trunc);
        if (!ofs) throw Core_RuntimeError(line, col, "IOError", "Dosya yazilamadi: " + Core_WToNarrow(path));
        ofs.write(utf8.data(), (std::streamsize)utf8.size());
        return Value::makeBool(true);
    }
    if (name == L"dosya_oku_gari") {
        ExpectBuiltinArgCount(L"dosya_oku_gari(yol)", args.size(), 1, line, col);
        std::wstring path = ValueToString(args[0]);
        std::ifstream ifs(path, std::ios::binary);
        if (!ifs) throw Core_RuntimeError(line, col, "IOError", "Dosya okunamadi: " + Core_WToNarrow(path));
        std::string utf8((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
        return Value::makeString(Core_Widen(utf8));
    }
    if (name == L"dosya_var_mi") {
        ExpectBuiltinArgCount(L"dosya_var_mi(yol)", args.size(), 1, line, col);
        std::wstring path = ValueToString(args[0]);
        DWORD attr = GetFileAttributesW(path.c_str());
        bool exists = (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY));
        return Value::makeBool(exists);
    }
    if (name == L"bekle_gari") {
        ExpectBuiltinArgCount(L"bekle_gari(ms)", args.size(), 1, line, col);
        ExpectNumericBuiltinArg(name, args[0], line, col);
        int ms = ToInt(args[0]);
        if (ms > 0) {
            if (ms > 30000) ms = 30000; // Maksimum 30 saniye güvenlik sınırı
            std::this_thread::sleep_for(std::chrono::milliseconds(ms));
        }
        return Value::makeInt(ms);
    }
    if (name == L"vakit_gari") {
        ExpectBuiltinArgCount(L"vakit_gari()", args.size(), 0, line, col);
        static const auto t0 = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        int ms = (int)std::chrono::duration_cast<std::chrono::milliseconds>(now - t0).count();
        return Value::makeInt(ms);
    }

    throw Core_RuntimeError(line, col, "NameError", UnknownCallMsg(name));
}