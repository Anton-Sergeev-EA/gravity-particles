#pragma once
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

enum class FontWeight { Regular = 0, Bold = 1 };

struct ShapedGlyph {
    uint16_t face;   // индекс начертания в TextShaper
    uint32_t glyph;  // индекс глифа в шрифте (не код символа!)
    float x, y;      // позиция относительно начала строки на базовой линии, px
};

struct ShapedLine {
    std::vector<ShapedGlyph> glyphs;
    float width = 0.0f;
};

struct GlyphBitmap {
    int width = 0, height = 0;
    int left = 0, top = 0;  // смещение от точки пера до левого верхнего угла
    std::vector<uint8_t> pixels;
};

// Шейпинг и растеризация текста на любом из поддерживаемых языков.
//
// HarfBuzz превращает строку в последовательность глифов с учётом правил
// письменности: перестановка огласовок и лигатуры деванагари (हिन्दी),
// кернинг латиницы и кириллицы, полноширинная пунктуация CJK.
// FreeType растеризует глифы. Для каждого символа выбирается первое
// начертание из цепочки, которое его содержит (Noto Sans → Noto Sans
// Devanagari → Noto Sans SC); комбинирующие знаки остаются в шрифте
// базового символа.
//
// Класс не зависит от OpenGL, поэтому покрывается модульными тестами.
class TextShaper {
public:
    TextShaper();
    ~TextShaper();
    TextShaper(const TextShaper&) = delete;
    TextShaper& operator=(const TextShaper&) = delete;

    // Загружает стандартный набор шрифтов из каталога assets/fonts.
    bool loadDefaultFonts(const std::filesystem::path& fontsDir, std::string* error = nullptr);
    // Добавляет шрифт в цепочку начертания weight. Возвращает индекс или -1.
    int addFont(const std::filesystem::path& file, FontWeight weight, std::string* error = nullptr);

    // Результат кэшируется; ссылка действительна до следующего clearCache().
    const ShapedLine& shape(std::string_view utf8, FontWeight weight, int pixelSize);
    float measure(std::string_view utf8, FontWeight weight, int pixelSize) {
        return shape(utf8, weight, pixelSize).width;
    }

    // Перенос по словам с учётом CJK (перенос возможен между иероглифами).
    std::vector<std::string> wrap(std::string_view utf8, FontWeight weight, int pixelSize,
                                  float maxWidth);

    bool rasterize(uint16_t face, uint32_t glyph, int pixelSize, GlyphBitmap& out);

    float ascender(int pixelSize) const;
    float descender(int pixelSize) const;  // положительное число
    float lineHeight(int pixelSize) const;

    // Индекс начертания, содержащего символ, или -1.
    int faceFor(char32_t cp, FontWeight weight) const;
    size_t faceCount() const { return faces_.size(); }
    bool faceHasGlyph(size_t face, char32_t cp) const;

    void clearCache();

private:
    struct Face;
    struct Library;
    std::unique_ptr<Library> lib_;
    std::vector<std::unique_ptr<Face>> faces_;
    std::vector<int> chain_[2];  // цепочки начертаний по весу
    std::unordered_map<std::string, ShapedLine> cache_;
    std::unordered_map<std::string, std::vector<std::string>> wrapCache_;

    void shapeInto(std::u32string_view text, FontWeight weight, int pixelSize, ShapedLine& out);
};
