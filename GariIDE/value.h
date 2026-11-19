#pragma once
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <map>

#ifdef VOID
#undef VOID
#endif

struct Value {
    // 🌟 YENİ: ARRAY, VOID, STRUCT ve MAP (sozluk_gari) tipleri eklendi
    enum class Type { INT, FLOAT, DOUBLE, STRING, BOOL, ARRAY, VOID, STRUCT, MAP };

    Type type = Type::INT;
    int iVal = 0;
    float fVal = 0.0f;
    double dVal = 0.0;
    std::wstring sVal;
    bool bVal = false;

    // 🌟 Dizideki elemanları ve eleman tipini shared_ptr ile hafızada güvenle taşımak için
    std::shared_ptr<std::vector<Value>> arrVal = nullptr;
    Type elemType = Type::INT;

    // 🌟 kalip_gari (struct) örnekleri için alan haritası ve kalıp adı
    std::wstring structName;
    std::shared_ptr<std::unordered_map<std::wstring, Value>> fields = nullptr;

    // 🌟 YENİ: sozluk_gari (map/dictionary) örnekleri için sıralı anahtar->değer haritası
    std::shared_ptr<std::map<std::wstring, Value>> mapVal = nullptr;

    static Value makeVoid() { Value x; x.type = Type::VOID; return x; }
    static Value makeInt(int v) { Value x; x.type = Type::INT; x.iVal = v; return x; }
    static Value makeFloat(float v) { Value x; x.type = Type::FLOAT; x.fVal = v; return x; }
    static Value makeDouble(double v) { Value x; x.type = Type::DOUBLE; x.dVal = v; return x; }
    static Value makeString(const std::wstring& v) { Value x; x.type = Type::STRING; x.sVal = v; return x; }
    static Value makeBool(bool v) { Value x; x.type = Type::BOOL; x.bVal = v; return x; }

    static Value makeDefaultForType(Type t) {
        switch (t) {
        case Type::INT: return makeInt(0);
        case Type::FLOAT: return makeFloat(0.0f);
        case Type::DOUBLE: return makeDouble(0.0);
        case Type::STRING: return makeString(L"");
        case Type::BOOL: return makeBool(false);
        case Type::MAP: return makeMap();
        default: return makeInt(0);
        }
    }

    // 🌟 Belirli bir boyutta ve eleman tipinde varsayılan değerlerle dolu dizi üreten yapıcı
    static Value makeArray(size_t size, Type eType = Type::INT) {
        Value x;
        x.type = Type::ARRAY;
        x.elemType = eType;
        x.arrVal = std::make_shared<std::vector<Value>>(size, makeDefaultForType(eType));
        return x;
    }

    static Value makeArrayFromList(std::vector<Value> items, Type eType = Type::INT) {
        Value x;
        x.type = Type::ARRAY;
        x.elemType = eType;
        x.arrVal = std::make_shared<std::vector<Value>>(std::move(items));
        return x;
    }

    static Value makeStruct(const std::wstring& sName) {
        Value x;
        x.type = Type::STRUCT;
        x.structName = sName;
        x.fields = std::make_shared<std::unordered_map<std::wstring, Value>>();
        return x;
    }

    static Value makeMap(Type valT = Type::INT) {
        Value x;
        x.type = Type::MAP;
        x.elemType = valT;
        x.mapVal = std::make_shared<std::map<std::wstring, Value>>();
        return x;
    }
};

bool IsNumeric(Value::Type t);
std::wstring ValueToString(const Value& v);
bool IsTrue(const Value& v);
double ToDouble(const Value& v);
float ToFloat(const Value& v);
int ToInt(const Value& v);
bool ToBool(const Value& v);
bool ValueEquals(const Value& A, const Value& B);
Value Coerce(const Value& v, Value::Type target);