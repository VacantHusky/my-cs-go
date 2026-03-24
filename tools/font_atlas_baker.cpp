#include <ft2build.h>
#include FT_FREETYPE_H

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

struct Span {
    std::uint16_t y = 0;
    std::uint16_t x0 = 0;
    std::uint16_t x1 = 0;
};

struct Glyph {
    std::uint32_t codepoint = 0;
    std::uint16_t width = 0;
    std::uint16_t height = 0;
    std::int16_t bearingX = 0;
    std::int16_t bearingY = 0;
    std::uint16_t advance = 0;
    std::uint32_t spanOffset = 0;
    std::uint16_t spanCount = 0;
};

std::string readTextFile(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        return {};
    }
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

bool writeTextFile(const std::filesystem::path& path, const std::string& content) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary);
    if (!stream) {
        return false;
    }
    stream.write(content.data(), static_cast<std::streamsize>(content.size()));
    return static_cast<bool>(stream);
}

std::vector<std::uint32_t> decodeUtf8(std::string_view text) {
    std::vector<std::uint32_t> result;
    for (std::size_t index = 0; index < text.size();) {
        const unsigned char lead = static_cast<unsigned char>(text[index]);
        std::uint32_t codepoint = 0;
        std::size_t count = 0;
        if ((lead & 0x80u) == 0) {
            codepoint = lead;
            count = 1;
        } else if ((lead & 0xE0u) == 0xC0u && index + 1 < text.size()) {
            codepoint = static_cast<std::uint32_t>(lead & 0x1Fu) << 6;
            codepoint |= static_cast<std::uint32_t>(static_cast<unsigned char>(text[index + 1]) & 0x3Fu);
            count = 2;
        } else if ((lead & 0xF0u) == 0xE0u && index + 2 < text.size()) {
            codepoint = static_cast<std::uint32_t>(lead & 0x0Fu) << 12;
            codepoint |= static_cast<std::uint32_t>(static_cast<unsigned char>(text[index + 1]) & 0x3Fu) << 6;
            codepoint |= static_cast<std::uint32_t>(static_cast<unsigned char>(text[index + 2]) & 0x3Fu);
            count = 3;
        } else if ((lead & 0xF8u) == 0xF0u && index + 3 < text.size()) {
            codepoint = static_cast<std::uint32_t>(lead & 0x07u) << 18;
            codepoint |= static_cast<std::uint32_t>(static_cast<unsigned char>(text[index + 1]) & 0x3Fu) << 12;
            codepoint |= static_cast<std::uint32_t>(static_cast<unsigned char>(text[index + 2]) & 0x3Fu) << 6;
            codepoint |= static_cast<std::uint32_t>(static_cast<unsigned char>(text[index + 3]) & 0x3Fu);
            count = 4;
        } else {
            codepoint = '?';
            count = 1;
        }

        result.push_back(codepoint);
        index += count;
    }
    return result;
}

std::string narrowOrEmpty(const char* value) {
    return value != nullptr ? value : "";
}

FT_Face loadPreferredFace(FT_Library library, const std::filesystem::path& fontPath) {
    FT_Face chosen = nullptr;
    FT_Long faceIndex = 0;
    while (true) {
        FT_Face face = nullptr;
        const FT_Error error = FT_New_Face(library, fontPath.string().c_str(), faceIndex, &face);
        if (error != 0) {
            break;
        }

        const std::string family = narrowOrEmpty(face->family_name);
        const std::string style = narrowOrEmpty(face->style_name);
        const std::string joined = family + " " + style;
        if (joined.find("SC") != std::string::npos || joined.find("CN") != std::string::npos || joined.find("Simplified") != std::string::npos) {
            if (chosen != nullptr) {
                FT_Done_Face(chosen);
            }
            chosen = face;
            break;
        }

        if (chosen == nullptr) {
            chosen = face;
        } else {
            FT_Done_Face(face);
        }

        ++faceIndex;
    }

    return chosen;
}

std::string emitHeader() {
    return
        "#pragma once\n"
        "\n"
        "#include <cstddef>\n"
        "#include <cstdint>\n"
        "\n"
        "namespace mycsg::renderer::font {\n"
        "\n"
        "struct UiFontSpan {\n"
        "    std::uint16_t y = 0;\n"
        "    std::uint16_t x0 = 0;\n"
        "    std::uint16_t x1 = 0;\n"
        "};\n"
        "\n"
        "struct UiFontGlyph {\n"
        "    std::uint32_t codepoint = 0;\n"
        "    std::uint16_t width = 0;\n"
        "    std::uint16_t height = 0;\n"
        "    std::int16_t bearingX = 0;\n"
        "    std::int16_t bearingY = 0;\n"
        "    std::uint16_t advance = 0;\n"
        "    std::uint32_t spanOffset = 0;\n"
        "    std::uint16_t spanCount = 0;\n"
        "};\n"
        "\n"
        "extern const int kUiFontPixelSize;\n"
        "extern const int kUiFontAscent;\n"
        "extern const int kUiFontDescent;\n"
        "extern const int kUiFontLineHeight;\n"
        "extern const UiFontSpan kUiFontSpans[];\n"
        "extern const UiFontGlyph kUiFontGlyphs[];\n"
        "extern const std::size_t kUiFontGlyphCount;\n"
        "\n"
        "}  // namespace mycsg::renderer::font\n";
}

std::string emitSource(const int pixelSize,
                       const int ascent,
                       const int descent,
                       const int lineHeight,
                       const std::vector<Span>& spans,
                       const std::vector<Glyph>& glyphs) {
    std::ostringstream out;
    out << "#include \"renderer/font/GeneratedUiFontData.h\"\n\n";
    out << "namespace mycsg::renderer::font {\n\n";
    out << "const int kUiFontPixelSize = " << pixelSize << ";\n";
    out << "const int kUiFontAscent = " << ascent << ";\n";
    out << "const int kUiFontDescent = " << descent << ";\n";
    out << "const int kUiFontLineHeight = " << lineHeight << ";\n\n";
    out << "const UiFontSpan kUiFontSpans[] = {\n";
    for (const auto& span : spans) {
        out << "    {" << span.y << ", " << span.x0 << ", " << span.x1 << "},\n";
    }
    out << "};\n\n";
    out << "const UiFontGlyph kUiFontGlyphs[] = {\n";
    for (const auto& glyph : glyphs) {
        out << "    {0x" << std::hex << glyph.codepoint << std::dec
            << "u, " << glyph.width
            << ", " << glyph.height
            << ", " << glyph.bearingX
            << ", " << glyph.bearingY
            << ", " << glyph.advance
            << ", " << glyph.spanOffset
            << ", " << glyph.spanCount
            << "},\n";
    }
    out << "};\n\n";
    out << "const std::size_t kUiFontGlyphCount = sizeof(kUiFontGlyphs) / sizeof(kUiFontGlyphs[0]);\n\n";
    out << "}  // namespace mycsg::renderer::font\n";
    return out.str();
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 5) {
        std::cerr << "usage: font_atlas_baker <font> <charset> <out-header> <out-source>\n";
        return 1;
    }

    const std::filesystem::path fontPath = argv[1];
    const std::filesystem::path charsetPath = argv[2];
    const std::filesystem::path headerPath = argv[3];
    const std::filesystem::path sourcePath = argv[4];

    const std::string charsetText = readTextFile(charsetPath);
    if (charsetText.empty()) {
        std::cerr << "charset file is empty: " << charsetPath << '\n';
        return 1;
    }

    std::set<std::uint32_t> uniqueCodepoints;
    for (const std::uint32_t codepoint : decodeUtf8(charsetText)) {
        if (codepoint >= 32u) {
            uniqueCodepoints.insert(codepoint);
        }
    }
    for (std::uint32_t codepoint = 32; codepoint <= 126; ++codepoint) {
        uniqueCodepoints.insert(codepoint);
    }
    uniqueCodepoints.insert('?');

    FT_Library library = nullptr;
    if (FT_Init_FreeType(&library) != 0) {
        std::cerr << "failed to initialize freetype\n";
        return 1;
    }

    FT_Face face = loadPreferredFace(library, fontPath);
    if (face == nullptr) {
        std::cerr << "failed to load font face from " << fontPath << '\n';
        FT_Done_FreeType(library);
        return 1;
    }

    constexpr int pixelSize = 32;
    FT_Set_Pixel_Sizes(face, 0, pixelSize);

    const int ascent = static_cast<int>(face->size->metrics.ascender >> 6);
    const int descent = static_cast<int>(-(face->size->metrics.descender >> 6));
    const int lineHeight = static_cast<int>(face->size->metrics.height >> 6);

    std::vector<Span> spans;
    std::vector<Glyph> glyphs;

    for (const std::uint32_t codepoint : uniqueCodepoints) {
        if (FT_Load_Char(face, codepoint, FT_LOAD_RENDER | FT_LOAD_TARGET_NORMAL) != 0) {
            continue;
        }

        const FT_GlyphSlot slot = face->glyph;
        const FT_Bitmap& bitmap = slot->bitmap;
        Glyph glyph{};
        glyph.codepoint = codepoint;
        glyph.width = static_cast<std::uint16_t>(bitmap.width);
        glyph.height = static_cast<std::uint16_t>(bitmap.rows);
        glyph.bearingX = static_cast<std::int16_t>(slot->bitmap_left);
        glyph.bearingY = static_cast<std::int16_t>(slot->bitmap_top);
        glyph.advance = static_cast<std::uint16_t>(slot->advance.x >> 6);
        glyph.spanOffset = static_cast<std::uint32_t>(spans.size());

        for (unsigned int y = 0; y < bitmap.rows; ++y) {
            const unsigned char* row = bitmap.buffer + y * bitmap.pitch;
            unsigned int x = 0;
            while (x < bitmap.width) {
                while (x < bitmap.width && row[x] <= 64u) {
                    ++x;
                }
                const unsigned int runStart = x;
                while (x < bitmap.width && row[x] > 64u) {
                    ++x;
                }
                if (runStart < x) {
                    spans.push_back(Span{
                        static_cast<std::uint16_t>(y),
                        static_cast<std::uint16_t>(runStart),
                        static_cast<std::uint16_t>(x),
                    });
                    ++glyph.spanCount;
                }
            }
        }

        glyphs.push_back(glyph);
    }

    FT_Done_Face(face);
    FT_Done_FreeType(library);

    if (!writeTextFile(headerPath, emitHeader())) {
        std::cerr << "failed to write header: " << headerPath << '\n';
        return 1;
    }
    if (!writeTextFile(sourcePath, emitSource(pixelSize, ascent, descent, lineHeight, spans, glyphs))) {
        std::cerr << "failed to write source: " << sourcePath << '\n';
        return 1;
    }

    std::cout << "Baked " << glyphs.size() << " glyphs and " << spans.size() << " spans.\n";
    return 0;
}
