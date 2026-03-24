#pragma once

#include <cstddef>
#include <cstdint>

namespace mycsg::renderer::font {

struct UiFontSpan {
    std::uint16_t y = 0;
    std::uint16_t x0 = 0;
    std::uint16_t x1 = 0;
};

struct UiFontGlyph {
    std::uint32_t codepoint = 0;
    std::uint16_t width = 0;
    std::uint16_t height = 0;
    std::int16_t bearingX = 0;
    std::int16_t bearingY = 0;
    std::uint16_t advance = 0;
    std::uint32_t spanOffset = 0;
    std::uint16_t spanCount = 0;
};

extern const int kUiFontPixelSize;
extern const int kUiFontAscent;
extern const int kUiFontDescent;
extern const int kUiFontLineHeight;
extern const UiFontSpan kUiFontSpans[];
extern const UiFontGlyph kUiFontGlyphs[];
extern const std::size_t kUiFontGlyphCount;

}  // namespace mycsg::renderer::font
