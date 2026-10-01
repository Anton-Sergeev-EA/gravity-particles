#include <filesystem>
#include <memory>

#include "I18n.h"
#include "Test.h"
#include "TextShaper.h"
#include "Utf8.h"

namespace fs = std::filesystem;

namespace {

TextShaper& shaper() {
    static std::unique_ptr<TextShaper> s;
    if (!s) {
        s = std::make_unique<TextShaper>();
        std::string error;
        if (!s->loadDefaultFonts(fs::path(GP_SOURCE_DIR) / "assets" / "fonts", &error)) {
            test::fail(__FILE__, __LINE__, "fonts: " + error);
        }
    }
    return *s;
}

bool isFormatChar(char32_t cp) {
    return cp < 0x20 || cp == 0x200C || cp == 0x200D;
}

}  // namespace

// Каждый символ каждой строки каждого перевода есть во встроенных шрифтах
// обоих начертаний — иначе на экране появятся «квадратики» (tofu).
TEST(text_fonts_cover_all_translations) {
    I18n i18n;
    CHECK(i18n.load(fs::path(GP_SOURCE_DIR) / "locales"));
    TextShaper& sh = shaper();
    for (const auto& lang : i18n.languages()) {
        for (const auto& key : i18n.keys(lang.code)) {
            const std::string* s = i18n.raw(lang.code, key);
            for (char32_t cp : utf8::decode(*s)) {
                if (isFormatChar(cp)) continue;
                for (FontWeight w : {FontWeight::Regular, FontWeight::Bold}) {
                    if (sh.faceFor(cp, w) < 0) {
                        test::fail(__FILE__, __LINE__,
                                   lang.code + "/" + key + ": no glyph for U+" + std::to_string(cp));
                    }
                }
            }
        }
    }
}

// Деванагари: огласовка «ि» пишется после согласной, но рисуется перед ней.
// Без шейпинга (как в Dear ImGui и большинстве игровых UI) порядок неверный.
TEST(text_devanagari_reorders_i_matra) {
    TextShaper& sh = shaper();
    const ShapedLine& line = sh.shape("कि", FontWeight::Regular, 32);
    CHECK_EQ(line.glyphs.size(), size_t(2));
    const int deva = sh.faceFor(U'क', FontWeight::Regular);
    CHECK(deva >= 0);
    if (line.glyphs.size() == 2) {
        CHECK_EQ(static_cast<int>(line.glyphs[0].face), deva);
        // Первым (левее) должен идти глиф огласовки, а не согласной «क».
        CHECK(line.glyphs[0].x < line.glyphs[1].x);
        const ShapedLine& ka = sh.shape("क", FontWeight::Regular, 32);
        CHECK(!ka.glyphs.empty() && line.glyphs[0].glyph != ka.glyphs[0].glyph);
        CHECK(line.glyphs[1].glyph == ka.glyphs[0].glyph);
    }
}

TEST(text_devanagari_forms_conjuncts) {
    TextShaper& sh = shaper();
    // हिन्दी = ह ि न ् द ी (6 кодовых точек); «न्द» собирается в лигатуру/полуформу.
    const std::u32string cps = utf8::decode("हिन्दी");
    CHECK_EQ(cps.size(), size_t(6));
    const ShapedLine& line = sh.shape("हिन्दी", FontWeight::Regular, 32);
    CHECK(line.glyphs.size() < cps.size());
    CHECK(line.width > 0.0f);
}

TEST(text_chinese_uses_cjk_face) {
    TextShaper& sh = shaper();
    const int cjk = sh.faceFor(U'引', FontWeight::Regular);
    CHECK(cjk >= 0);
    CHECK(cjk != sh.faceFor(U'A', FontWeight::Regular));
    const ShapedLine& line = sh.shape("引力粒子", FontWeight::Regular, 20);
    CHECK_EQ(line.glyphs.size(), size_t(4));
    for (const auto& g : line.glyphs) CHECK_EQ(static_cast<int>(g.face), cjk);
    // Иероглифы — квадратные: ширина строки ≈ 4 × кегль.
    CHECK(line.width > 4 * 20 * 0.9f && line.width < 4 * 20 * 1.1f);
}

TEST(text_mixed_scripts_split_into_runs) {
    TextShaper& sh = shaper();
    const ShapedLine& line = sh.shape("F1 — सहायता · Tab — 设置", FontWeight::Regular, 16);
    std::vector<int> faces;
    for (const auto& g : line.glyphs) {
        if (faces.empty() || faces.back() != g.face) faces.push_back(g.face);
    }
    CHECK(faces.size() >= 3);
    for (size_t i = 1; i < line.glyphs.size(); ++i) CHECK(line.glyphs[i].x >= line.glyphs[i - 1].x - 20.0f);
}

TEST(text_cyrillic_and_latin_in_primary_face) {
    TextShaper& sh = shaper();
    const int latin = sh.faceFor(U'A', FontWeight::Regular);
    CHECK_EQ(sh.faceFor(U'Ж', FontWeight::Regular), latin);
    CHECK_EQ(sh.faceFor(U'ß', FontWeight::Regular), latin);
    CHECK_EQ(sh.faceFor(U'É', FontWeight::Regular), latin);
    CHECK(sh.faceFor(U'Ж', FontWeight::Bold) != latin);  // жирное начертание — отдельный файл
}

TEST(text_wrap_respects_width) {
    TextShaper& sh = shaper();
    const char* text = "Click anywhere to create a gravity source and watch particles orbit it";
    auto lines = sh.wrap(text, FontWeight::Regular, 16, 160.0f);
    CHECK(lines.size() > 2);
    for (const auto& l : lines) {
        CHECK(sh.measure(l, FontWeight::Regular, 16) <= 160.0f);
        CHECK(!l.empty() && l.front() != ' ' && l.back() != ' ');
    }
}

TEST(text_wrap_breaks_between_ideographs) {
    TextShaper& sh = shaper();
    auto lines = sh.wrap("点击任意位置即可创建引力源，点击任意位置即可创建引力源。", FontWeight::Regular, 16, 100.0f);
    CHECK(lines.size() >= 3);
    for (const auto& l : lines) {
        const std::u32string cps = utf8::decode(l);
        CHECK(!cps.empty());
        // Строка не может начинаться с закрывающей пунктуации.
        CHECK(cps.front() != U'，' && cps.front() != U'。');
    }
}

TEST(text_rasterizes_glyphs) {
    TextShaper& sh = shaper();
    const ShapedLine& line = sh.shape("Ж", FontWeight::Regular, 24);
    CHECK_EQ(line.glyphs.size(), size_t(1));
    GlyphBitmap bm;
    CHECK(sh.rasterize(line.glyphs[0].face, line.glyphs[0].glyph, 24, bm));
    CHECK(bm.width > 5 && bm.height > 5);
    int ink = 0;
    for (auto p : bm.pixels) ink += p > 128 ? 1 : 0;
    CHECK(ink > 20);
}

TEST(text_shape_cache_is_stable) {
    TextShaper& sh = shaper();
    const ShapedLine* a = &sh.shape("Настройки", FontWeight::Bold, 18);
    const ShapedLine* b = &sh.shape("Настройки", FontWeight::Bold, 18);
    CHECK(a == b);
    CHECK(sh.measure("Настройки", FontWeight::Bold, 18) > sh.measure("Настройки", FontWeight::Bold, 12));
}

TEST(text_utf8_roundtrip_and_invalid_input) {
    const std::string s = "Ж中हि🚀";
    CHECK_EQ(utf8::encode(utf8::decode(s)), s);
    const std::u32string bad = utf8::decode("\xFF\xC3");
    CHECK_EQ(bad.size(), size_t(2));
    CHECK(bad[0] == 0xFFFD && bad[1] == 0xFFFD);
    CHECK_EQ(utf8::decode("\xC0\xAF").front(), char32_t(0xFFFD));  // overlong '/'
}
