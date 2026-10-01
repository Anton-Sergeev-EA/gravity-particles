#include "Json.h"
#include "Test.h"

TEST(json_parses_nested_objects_and_arrays) {
    JsonValue v = parseJson(R"({"a": {"b": "c"}, "list": ["x", "y"], "n": -1.5, "t": true, "z": null})");
    CHECK(v.isObject());
    const JsonValue* a = v.find("a");
    CHECK(a && a->isObject());
    CHECK(a && a->find("b") && a->find("b")->string == "c");
    const JsonValue* list = v.find("list");
    CHECK(list && list->isArray() && list->array.size() == 2);
    CHECK_EQ(v.find("n")->number, -1.5);
    CHECK(v.find("t")->boolean);
    CHECK(v.find("z")->type == JsonValue::Type::Null);
    CHECK(v.find("missing") == nullptr);
}

TEST(json_decodes_escapes_and_unicode) {
    JsonValue v = parseJson(R"({"s": "a\"b\\c\n\u00a0\u0939\ud83d\ude80"})");
    // a"b\c, перевод строки, NBSP, деванагари «ह», эмодзи-ракета (суррогатная пара)
    CHECK_EQ(v.find("s")->string, std::string("a\"b\\c\n\xC2\xA0\xE0\xA4\xB9\xF0\x9F\x9A\x80"));
}

TEST(json_keeps_raw_utf8) {
    JsonValue v = parseJson("{\"t\": \"Привет, 世界, नमस्ते\"}");
    CHECK_EQ(v.find("t")->string, std::string("Привет, 世界, नमस्ते"));
}

TEST(json_skips_bom) {
    JsonValue v = parseJson("\xEF\xBB\xBF{\"k\": \"v\"}");
    CHECK_EQ(v.find("k")->string, std::string("v"));
}

TEST(json_reports_error_position) {
    bool thrown = false;
    try {
        parseJson("{\n  \"a\": \"b\",\n  \"c\" \"d\"\n}");
    } catch (const JsonError& e) {
        thrown = true;
        CHECK_EQ(e.line, 3);
    }
    CHECK(thrown);
}

TEST(json_rejects_invalid_documents) {
    const char* bad[] = {"", "{", "{\"a\":}", "[1,]", "{\"a\": \"\\x\"}", "{\"a\": 1} x",
                         "{\"a\": \"\\ud800\"}", "{\"a\": \"line\nbreak\"}"};
    for (const char* doc : bad) {
        bool thrown = false;
        try {
            parseJson(doc);
        } catch (const JsonError&) {
            thrown = true;
        }
        if (!thrown) test::fail(__FILE__, __LINE__, std::string("accepted invalid JSON: ") + doc);
    }
}
