#pragma once
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

// Минимальный JSON-парсер без внешних зависимостей. Нужен для файлов
// локализации и манифеста языков: поддерживает объекты, массивы, строки
// (включая \uXXXX и суррогатные пары), числа, true/false/null.
class JsonValue {
public:
    enum class Type { Null, Bool, Number, String, Array, Object };

    Type type = Type::Null;
    bool boolean = false;
    double number = 0.0;
    std::string string;
    std::vector<JsonValue> array;
    std::vector<std::pair<std::string, JsonValue>> object;  // порядок ключей сохраняется

    bool isObject() const { return type == Type::Object; }
    bool isArray() const { return type == Type::Array; }
    bool isString() const { return type == Type::String; }

    // nullptr, если ключа нет или значение не объект.
    const JsonValue* find(const std::string& key) const;
};

class JsonError : public std::runtime_error {
public:
    JsonError(const std::string& message, int line, int column);
    int line;
    int column;
};

JsonValue parseJson(const std::string& text);
