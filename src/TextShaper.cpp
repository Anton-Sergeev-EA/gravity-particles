#include "TextShaper.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <iterator>

#include <ft2build.h>
#include FT_FREETYPE_H
#include <hb-ot.h>
#include <hb.h>

#include "Utf8.h"

namespace fs = std::filesystem;

struct TextShaper::Library {
    FT_Library ft = nullptr;
    hb_buffer_t* buffer = nullptr;
    Library() {
        FT_Init_FreeType(&ft);
        buffer = hb_buffer_create();
    }
    ~Library() {
        if (buffer) hb_buffer_destroy(buffer);
        if (ft) FT_Done_FreeType(ft);
    }
    Library(const Library&) = delete;
    Library& operator=(const Library&) = delete;
};

struct TextShaper::Face {
    std::vector<unsigned char> data;  // FreeType и HarfBuzz читают шрифт из этой памяти
    FT_Face ft = nullptr;
    hb_blob_t* blob = nullptr;
    hb_face_t* hbFace = nullptr;
    hb_font_t* hbFont = nullptr;
    int upem = 1000;
    int ftPixelSize = 0;

    Face() = default;
    Face(const Face&) = delete;
    Face& operator=(const Face&) = delete;
    ~Face() {
        if (hbFont) hb_font_destroy(hbFont);
        if (hbFace) hb_face_destroy(hbFace);
        if (blob) hb_blob_destroy(blob);
        if (ft) FT_Done_Face(ft);
    }
};

namespace {

bool isInheriting(char32_t cp) {
    if (cp == 0x200C || cp == 0x200D) return true;            // ZWNJ / ZWJ
    if (cp >= 0xFE00 && cp <= 0xFE0F) return true;            // variation selectors
    switch (hb_unicode_general_category(hb_unicode_funcs_get_default(), cp)) {
        case HB_UNICODE_GENERAL_CATEGORY_NON_SPACING_MARK:
        case HB_UNICODE_GENERAL_CATEGORY_SPACING_MARK:
        case HB_UNICODE_GENERAL_CATEGORY_ENCLOSING_MARK:
            return true;
        default:
            return false;
    }
}

// Символы, общие для всех письменностей: их лучше оставить в шрифте текущего
// фрагмента, чтобы не дробить строку на лишние куски.
bool isCommon(char32_t cp) {
    if (cp < 0x80) return !((cp >= 'A' && cp <= 'Z') || (cp >= 'a' && cp <= 'z'));
    return cp == 0x00A0 || (cp >= 0x2000 && cp <= 0x206F);
}

bool isCjk(char32_t cp) {
    return (cp >= 0x4E00 && cp <= 0x9FFF) || (cp >= 0x3400 && cp <= 0x4DBF) ||
           (cp >= 0x3040 && cp <= 0x30FF) || (cp >= 0xAC00 && cp <= 0xD7AF) ||
           (cp >= 0x3000 && cp <= 0x303F) || (cp >= 0xFF00 && cp <= 0xFFEF);
}

// Пунктуация CJK, перед которой нельзя переносить строку.
bool isCjkClosing(char32_t cp) {
    static const char32_t kClosing[] = {0x3001, 0x3002, 0xFF0C, 0xFF0E, 0xFF1A, 0xFF1B,
                                        0xFF01, 0xFF1F, 0xFF09, 0x300D, 0x300F, 0x3011};
    return std::find(std::begin(kClosing), std::end(kClosing), cp) != std::end(kClosing);
}

bool isCjkOpening(char32_t cp) {
    return cp == 0xFF08 || cp == 0x300C || cp == 0x300E || cp == 0x3010;
}

std::string cacheKey(std::string_view text, FontWeight weight, int px) {
    std::string key;
    key.reserve(text.size() + 8);
    key.push_back(static_cast<char>('0' + static_cast<int>(weight)));
    key += std::to_string(px);
    key.push_back('|');
    key.append(text.data(), text.size());
    return key;
}

}  // namespace

TextShaper::TextShaper() : lib_(std::make_unique<Library>()) {}
TextShaper::~TextShaper() {
    faces_.clear();  // начертания освобождаются раньше библиотеки FreeType
}

int TextShaper::addFont(const fs::path& file, FontWeight weight, std::string* error) {
    if (!lib_->ft) {
        if (error) *error = "FreeType initialization failed";
        return -1;
    }
    auto face = std::make_unique<Face>();
    {
        std::ifstream in(file, std::ios::binary);
        if (!in) {
            if (error) *error = "cannot open font " + file.string();
            return -1;
        }
        face->data.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    }
    if (FT_New_Memory_Face(lib_->ft, face->data.data(), static_cast<FT_Long>(face->data.size()), 0,
                           &face->ft) != 0) {
        if (error) *error = "FreeType cannot read font " + file.string();
        return -1;
    }
    face->upem = face->ft->units_per_EM > 0 ? face->ft->units_per_EM : 1000;
    face->blob = hb_blob_create(reinterpret_cast<const char*>(face->data.data()),
                                static_cast<unsigned>(face->data.size()), HB_MEMORY_MODE_READONLY,
                                nullptr, nullptr);
    face->hbFace = hb_face_create(face->blob, 0);
    face->hbFont = hb_font_create(face->hbFace);
    hb_ot_font_set_funcs(face->hbFont);

    faces_.push_back(std::move(face));
    int index = static_cast<int>(faces_.size() - 1);
    chain_[static_cast<int>(weight)].push_back(index);
    clearCache();
    return index;
}

bool TextShaper::loadDefaultFonts(const fs::path& dir, std::string* error) {
    const char* regular[] = {"NotoSans-Regular.ttf", "NotoSansDevanagari-Regular.ttf",
                             "NotoSansSC-Regular.otf"};
    const char* bold[] = {"NotoSans-SemiBold.ttf", "NotoSansDevanagari-SemiBold.ttf",
                          "NotoSansSC-Medium.otf"};
    for (const char* name : regular) {
        if (addFont(dir / name, FontWeight::Regular, error) < 0) return false;
    }
    for (const char* name : bold) {
        if (addFont(dir / name, FontWeight::Bold, error) < 0) return false;
    }
    return true;
}

bool TextShaper::faceHasGlyph(size_t face, char32_t cp) const {
    return face < faces_.size() && FT_Get_Char_Index(faces_[face]->ft, cp) != 0;
}

int TextShaper::faceFor(char32_t cp, FontWeight weight) const {
    for (int idx : chain_[static_cast<int>(weight)]) {
        if (FT_Get_Char_Index(faces_[static_cast<size_t>(idx)]->ft, cp) != 0) return idx;
    }
    return -1;
}

float TextShaper::ascender(int px) const {
    if (faces_.empty()) return static_cast<float>(px) * 0.8f;
    const Face& f = *faces_[static_cast<size_t>(chain_[0].empty() ? 0 : chain_[0][0])];
    return static_cast<float>(f.ft->ascender) * static_cast<float>(px) / static_cast<float>(f.upem);
}

float TextShaper::descender(int px) const {
    if (faces_.empty()) return static_cast<float>(px) * 0.2f;
    const Face& f = *faces_[static_cast<size_t>(chain_[0].empty() ? 0 : chain_[0][0])];
    return static_cast<float>(-f.ft->descender) * static_cast<float>(px) / static_cast<float>(f.upem);
}

float TextShaper::lineHeight(int px) const {
    return ascender(px) + descender(px);
}

void TextShaper::clearCache() {
    cache_.clear();
    wrapCache_.clear();
}

const ShapedLine& TextShaper::shape(std::string_view utf8, FontWeight weight, int px) {
    std::string key = cacheKey(utf8, weight, px);
    auto it = cache_.find(key);
    if (it != cache_.end()) return it->second;
    if (cache_.size() > 4096) cache_.clear();  // строки интерфейса конечны; защита от утечки

    ShapedLine line;
    shapeInto(utf8::decode(utf8), weight, px, line);
    return cache_.emplace(std::move(key), std::move(line)).first->second;
}

void TextShaper::shapeInto(std::u32string_view text, FontWeight weight, int px, ShapedLine& out) {
    out.glyphs.clear();
    out.width = 0.0f;
    const auto& chain = chain_[static_cast<int>(weight)];
    if (text.empty() || chain.empty()) return;

    // 1. Разбиваем строку на фрагменты по начертаниям.
    std::vector<int> faceOf(text.size(), chain[0]);
    int current = -1;
    for (size_t i = 0; i < text.size(); ++i) {
        char32_t cp = text[i];
        int f = -1;
        if (current >= 0 && isInheriting(cp)) {
            f = current;
        } else if (current >= 0 && isCommon(cp) && faceHasGlyph(static_cast<size_t>(current), cp)) {
            f = current;
        } else {
            f = faceFor(cp, weight);
            if (f < 0) f = current >= 0 ? current : chain[0];
        }
        faceOf[i] = f;
        current = f;
    }

    // 2. Шейпим каждый фрагмент своим шрифтом, передавая всю строку как контекст.
    hb_buffer_t* buf = lib_->buffer;
    float pen = 0.0f;
    size_t start = 0;
    while (start < text.size()) {
        size_t end = start + 1;
        while (end < text.size() && faceOf[end] == faceOf[start]) ++end;

        Face& face = *faces_[static_cast<size_t>(faceOf[start])];
        hb_font_set_scale(face.hbFont, px * 64, px * 64);

        hb_buffer_clear_contents(buf);
        hb_buffer_add_utf32(buf, reinterpret_cast<const uint32_t*>(text.data()),
                            static_cast<int>(text.size()), static_cast<unsigned>(start),
                            static_cast<int>(end - start));
        hb_buffer_guess_segment_properties(buf);
        hb_shape(face.hbFont, buf, nullptr, 0);

        unsigned count = 0;
        const hb_glyph_info_t* info = hb_buffer_get_glyph_infos(buf, &count);
        const hb_glyph_position_t* pos = hb_buffer_get_glyph_positions(buf, &count);
        for (unsigned g = 0; g < count; ++g) {
            ShapedGlyph sg;
            sg.face = static_cast<uint16_t>(faceOf[start]);
            sg.glyph = info[g].codepoint;
            sg.x = pen + static_cast<float>(pos[g].x_offset) / 64.0f;
            sg.y = -static_cast<float>(pos[g].y_offset) / 64.0f;  // вниз — положительное направление
            out.glyphs.push_back(sg);
            pen += static_cast<float>(pos[g].x_advance) / 64.0f;
        }
        start = end;
    }
    out.width = pen;
}

std::vector<std::string> TextShaper::wrap(std::string_view utf8text, FontWeight weight, int px,
                                          float maxWidth) {
    std::string key = cacheKey(utf8text, weight, px) + "#" + std::to_string(static_cast<int>(maxWidth));
    auto it = wrapCache_.find(key);
    if (it != wrapCache_.end()) return it->second;

    std::vector<std::string> lines;
    std::u32string text = utf8::decode(utf8text);

    // Токены — неразрывные куски, между которыми допустим перенос.
    std::vector<std::u32string> tokens;
    std::u32string cur;
    auto flush = [&] {
        if (!cur.empty()) tokens.push_back(cur);
        cur.clear();
    };
    for (size_t i = 0; i < text.size(); ++i) {
        char32_t cp = text[i];
        if (cp == '\n') {
            flush();
            tokens.push_back(U"\n");
            continue;
        }
        bool cjk = isCjk(cp) && !isCjkClosing(cp);
        if (cjk && !cur.empty() && !isCjkOpening(cur.back())) flush();
        cur.push_back(cp);
        char32_t next = i + 1 < text.size() ? text[i + 1] : 0;
        if (cp == ' ' && next != ' ') flush();
        else if (cjk && !isCjkOpening(cp) && !(next && isCjkClosing(next))) flush();
    }
    flush();

    auto rtrim = [](std::u32string s) {
        while (!s.empty() && s.back() == ' ') s.pop_back();
        return s;
    };
    auto ltrim = [](std::u32string s) {
        size_t n = 0;
        while (n < s.size() && s[n] == ' ') ++n;
        return s.substr(n);
    };

    std::u32string line;
    ShapedLine tmp;
    for (const auto& tok : tokens) {
        if (tok == U"\n") {
            lines.push_back(utf8::encode(rtrim(line)));
            line.clear();
            continue;
        }
        std::u32string candidate = line + tok;
        shapeInto(rtrim(candidate), weight, px, tmp);
        if (line.empty() || tmp.width <= maxWidth) {
            line = line.empty() ? ltrim(candidate) : candidate;
        } else {
            lines.push_back(utf8::encode(rtrim(line)));
            line = ltrim(tok);
        }
    }
    if (!line.empty() || lines.empty()) lines.push_back(utf8::encode(rtrim(line)));

    if (wrapCache_.size() > 512) wrapCache_.clear();
    wrapCache_[key] = lines;
    return lines;
}

bool TextShaper::rasterize(uint16_t faceIndex, uint32_t glyph, int px, GlyphBitmap& out) {
    if (faceIndex >= faces_.size()) return false;
    Face& face = *faces_[faceIndex];
    if (face.ftPixelSize != px) {
        if (FT_Set_Pixel_Sizes(face.ft, 0, static_cast<FT_UInt>(px)) != 0) return false;
        face.ftPixelSize = px;
    }
    if (FT_Load_Glyph(face.ft, glyph, FT_LOAD_DEFAULT | FT_LOAD_TARGET_LIGHT) != 0) return false;
    if (FT_Render_Glyph(face.ft->glyph, FT_RENDER_MODE_NORMAL) != 0) return false;

    const FT_Bitmap& bm = face.ft->glyph->bitmap;
    out.width = static_cast<int>(bm.width);
    out.height = static_cast<int>(bm.rows);
    out.left = face.ft->glyph->bitmap_left;
    out.top = face.ft->glyph->bitmap_top;
    out.pixels.assign(static_cast<size_t>(out.width) * static_cast<size_t>(out.height), 0);
    for (int y = 0; y < out.height; ++y) {
        const unsigned char* row = bm.buffer + static_cast<ptrdiff_t>(y) * bm.pitch;
        std::memcpy(out.pixels.data() + static_cast<size_t>(y) * static_cast<size_t>(out.width), row,
                    static_cast<size_t>(out.width));
    }
    return true;
}
