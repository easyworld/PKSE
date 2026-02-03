#include <cstring>
#include <new>  // For std::nothrow

#include "UI/PKSEFramebuffer.h"

namespace UI {
    // Simple 8x8 bitmap font for ASCII characters 32-127
    // Each character is 8 bytes (8 rows of 8 pixels)
    static const uint8_t font8x8[96][8] = {
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // Space
        {0x18, 0x3C, 0x3C, 0x18, 0x18, 0x00, 0x18, 0x00}, // !
        {0x36, 0x36, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // "
        {0x36, 0x36, 0x7F, 0x36, 0x7F, 0x36, 0x36, 0x00}, // #
        {0x0C, 0x3E, 0x03, 0x1E, 0x30, 0x1F, 0x0C, 0x00}, // $
        {0x00, 0x63, 0x33, 0x18, 0x0C, 0x66, 0x63, 0x00}, // %
        {0x1C, 0x36, 0x1C, 0x6E, 0x3B, 0x33, 0x6E, 0x00}, // &
        {0x06, 0x06, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00}, // '
        {0x18, 0x0C, 0x06, 0x06, 0x06, 0x0C, 0x18, 0x00}, // (
        {0x06, 0x0C, 0x18, 0x18, 0x18, 0x0C, 0x06, 0x00}, // )
        {0x00, 0x66, 0x3C, 0xFF, 0x3C, 0x66, 0x00, 0x00}, // *
        {0x00, 0x0C, 0x0C, 0x3F, 0x0C, 0x0C, 0x00, 0x00}, // +
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C, 0x06}, // ,
        {0x00, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x00, 0x00}, // -
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C, 0x00}, // .
        {0x60, 0x30, 0x18, 0x0C, 0x06, 0x03, 0x01, 0x00}, // /
        {0x3E, 0x63, 0x73, 0x7B, 0x6F, 0x67, 0x3E, 0x00}, // 0
        {0x0C, 0x0E, 0x0C, 0x0C, 0x0C, 0x0C, 0x3F, 0x00}, // 1
        {0x1E, 0x33, 0x30, 0x1C, 0x06, 0x33, 0x3F, 0x00}, // 2
        {0x1E, 0x33, 0x30, 0x1C, 0x30, 0x33, 0x1E, 0x00}, // 3
        {0x38, 0x3C, 0x36, 0x33, 0x7F, 0x30, 0x78, 0x00}, // 4
        {0x3F, 0x03, 0x1F, 0x30, 0x30, 0x33, 0x1E, 0x00}, // 5
        {0x1C, 0x06, 0x03, 0x1F, 0x33, 0x33, 0x1E, 0x00}, // 6
        {0x3F, 0x33, 0x30, 0x18, 0x0C, 0x0C, 0x0C, 0x00}, // 7
        {0x1E, 0x33, 0x33, 0x1E, 0x33, 0x33, 0x1E, 0x00}, // 8
        {0x1E, 0x33, 0x33, 0x3E, 0x30, 0x18, 0x0E, 0x00}, // 9
        {0x00, 0x0C, 0x0C, 0x00, 0x00, 0x0C, 0x0C, 0x00}, // :
        {0x00, 0x0C, 0x0C, 0x00, 0x00, 0x0C, 0x0C, 0x06}, // ;
        {0x18, 0x0C, 0x06, 0x03, 0x06, 0x0C, 0x18, 0x00}, // <
        {0x00, 0x00, 0x3F, 0x00, 0x00, 0x3F, 0x00, 0x00}, // =
        {0x06, 0x0C, 0x18, 0x30, 0x18, 0x0C, 0x06, 0x00}, // >
        {0x1E, 0x33, 0x30, 0x18, 0x0C, 0x00, 0x0C, 0x00}, // ?
        {0x3E, 0x63, 0x7B, 0x7B, 0x7B, 0x03, 0x1E, 0x00}, // @
        {0x0C, 0x1E, 0x33, 0x33, 0x3F, 0x33, 0x33, 0x00}, // A
        {0x3F, 0x66, 0x66, 0x3E, 0x66, 0x66, 0x3F, 0x00}, // B
        {0x3C, 0x66, 0x03, 0x03, 0x03, 0x66, 0x3C, 0x00}, // C
        {0x1F, 0x36, 0x66, 0x66, 0x66, 0x36, 0x1F, 0x00}, // D
        {0x7F, 0x46, 0x16, 0x1E, 0x16, 0x46, 0x7F, 0x00}, // E
        {0x7F, 0x46, 0x16, 0x1E, 0x16, 0x06, 0x0F, 0x00}, // F
        {0x3C, 0x66, 0x03, 0x03, 0x73, 0x66, 0x7C, 0x00}, // G
        {0x33, 0x33, 0x33, 0x3F, 0x33, 0x33, 0x33, 0x00}, // H
        {0x1E, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x00}, // I
        {0x78, 0x30, 0x30, 0x30, 0x33, 0x33, 0x1E, 0x00}, // J
        {0x67, 0x66, 0x36, 0x1E, 0x36, 0x66, 0x67, 0x00}, // K
        {0x0F, 0x06, 0x06, 0x06, 0x46, 0x66, 0x7F, 0x00}, // L
        {0x63, 0x77, 0x7F, 0x7F, 0x6B, 0x63, 0x63, 0x00}, // M
        {0x63, 0x67, 0x6F, 0x7B, 0x73, 0x63, 0x63, 0x00}, // N
        {0x1C, 0x36, 0x63, 0x63, 0x63, 0x36, 0x1C, 0x00}, // O
        {0x3F, 0x66, 0x66, 0x3E, 0x06, 0x06, 0x0F, 0x00}, // P
        {0x1E, 0x33, 0x33, 0x33, 0x3B, 0x1E, 0x38, 0x00}, // Q
        {0x3F, 0x66, 0x66, 0x3E, 0x36, 0x66, 0x67, 0x00}, // R
        {0x1E, 0x33, 0x07, 0x0E, 0x38, 0x33, 0x1E, 0x00}, // S
        {0x3F, 0x2D, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x00}, // T
        {0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x3F, 0x00}, // U
        {0x33, 0x33, 0x33, 0x33, 0x33, 0x1E, 0x0C, 0x00}, // V
        {0x63, 0x63, 0x63, 0x6B, 0x7F, 0x77, 0x63, 0x00}, // W
        {0x63, 0x63, 0x36, 0x1C, 0x1C, 0x36, 0x63, 0x00}, // X
        {0x33, 0x33, 0x33, 0x1E, 0x0C, 0x0C, 0x1E, 0x00}, // Y
        {0x7F, 0x63, 0x31, 0x18, 0x4C, 0x66, 0x7F, 0x00}, // Z
        {0x1E, 0x06, 0x06, 0x06, 0x06, 0x06, 0x1E, 0x00}, // [
        {0x03, 0x06, 0x0C, 0x18, 0x30, 0x60, 0x40, 0x00}, // backslash
        {0x1E, 0x18, 0x18, 0x18, 0x18, 0x18, 0x1E, 0x00}, // ]
        {0x08, 0x1C, 0x36, 0x63, 0x00, 0x00, 0x00, 0x00}, // ^
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF}, // _
        {0x0C, 0x0C, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00}, // `
        {0x00, 0x00, 0x1E, 0x30, 0x3E, 0x33, 0x6E, 0x00}, // a
        {0x07, 0x06, 0x06, 0x3E, 0x66, 0x66, 0x3B, 0x00}, // b
        {0x00, 0x00, 0x1E, 0x33, 0x03, 0x33, 0x1E, 0x00}, // c
        {0x38, 0x30, 0x30, 0x3e, 0x33, 0x33, 0x6E, 0x00}, // d
        {0x00, 0x00, 0x1E, 0x33, 0x3f, 0x03, 0x1E, 0x00}, // e
        {0x1C, 0x36, 0x06, 0x0f, 0x06, 0x06, 0x0F, 0x00}, // f
        {0x00, 0x00, 0x6E, 0x33, 0x33, 0x3E, 0x30, 0x1F}, // g
        {0x07, 0x06, 0x36, 0x6E, 0x66, 0x66, 0x67, 0x00}, // h
        {0x0C, 0x00, 0x0E, 0x0C, 0x0C, 0x0C, 0x1E, 0x00}, // i
        {0x30, 0x00, 0x30, 0x30, 0x30, 0x33, 0x33, 0x1E}, // j
        {0x07, 0x06, 0x66, 0x36, 0x1E, 0x36, 0x67, 0x00}, // k
        {0x0E, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x00}, // l
        {0x00, 0x00, 0x33, 0x7F, 0x7F, 0x6B, 0x63, 0x00}, // m
        {0x00, 0x00, 0x1F, 0x33, 0x33, 0x33, 0x33, 0x00}, // n
        {0x00, 0x00, 0x1E, 0x33, 0x33, 0x33, 0x1E, 0x00}, // o
        {0x00, 0x00, 0x3B, 0x66, 0x66, 0x3E, 0x06, 0x0F}, // p
        {0x00, 0x00, 0x6E, 0x33, 0x33, 0x3E, 0x30, 0x78}, // q
        {0x00, 0x00, 0x3B, 0x6E, 0x66, 0x06, 0x0F, 0x00}, // r
        {0x00, 0x00, 0x3E, 0x03, 0x1E, 0x30, 0x1F, 0x00}, // s
        {0x08, 0x0C, 0x3E, 0x0C, 0x0C, 0x2C, 0x18, 0x00}, // t
        {0x00, 0x00, 0x33, 0x33, 0x33, 0x33, 0x6E, 0x00}, // u
        {0x00, 0x00, 0x33, 0x33, 0x33, 0x1E, 0x0C, 0x00}, // v
        {0x00, 0x00, 0x63, 0x6B, 0x7F, 0x7F, 0x36, 0x00}, // w
        {0x00, 0x00, 0x63, 0x36, 0x1C, 0x36, 0x63, 0x00}, // x
        {0x00, 0x00, 0x33, 0x33, 0x33, 0x3E, 0x30, 0x1F}, // y
        {0x00, 0x00, 0x3F, 0x19, 0x0C, 0x26, 0x3F, 0x00}, // z
        {0x38, 0x0C, 0x0C, 0x07, 0x0C, 0x0C, 0x38, 0x00}, // {
        {0x18, 0x18, 0x18, 0x00, 0x18, 0x18, 0x18, 0x00}, // |
        {0x07, 0x0C, 0x0C, 0x38, 0x0C, 0x0C, 0x07, 0x00}, // }
        {0x6E, 0x3B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // ~
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}  // DEL
    };

    // Unicode character glyphs (8x8 bitmap)
    static const uint8_t glyph_star[8] = {       // ★ (U+2605) - Black star
        0x18, 0x18, 0x7E, 0x3C, 0x7E, 0x66, 0x42, 0x00
    };

    static const uint8_t glyph_male[8] = {       // ♂ (U+2642) - Male symbol
        0x70, 0x58, 0x70, 0x20, 0x38, 0x44, 0x44, 0x38
    };

    static const uint8_t glyph_female[8] = {     // ♀ (U+2640) - Female symbol
        0x1C, 0x22, 0x22, 0x1C, 0x08, 0x3E, 0x08, 0x00
    };


    PKSEFramebuffer::PKSEFramebuffer() {
        framebufferCreate(&fb, nwindowGetDefault(), 1280, 720, PIXEL_FORMAT_RGBA_8888, 2);
        framebufferMakeLinear(&fb);
        framebuf = (u32*)framebufferBegin(&fb, &stride);
        width = 1280;
        height = 720;
        
        // Initialize FreeType for font rendering
        freetypeInitialized = false;
        
        // Initialize pl service to get system font
        Result rc = plInitialize(PlServiceType_User);
        if (R_FAILED(rc)) {
            return;  // FreeType won't be available, fallback to ASCII
        }
        
        // Get the Standard shared font (supports CJK characters)
        rc = plGetSharedFontByType(&fontData, PlSharedFontType_ChineseSimplified);
        if (R_FAILED(rc)) {
            plExit();
            return;  // FreeType won't be available, fallback to ASCII
        }
        
        // Initialize FreeType library
        FT_Error ftError = FT_Init_FreeType(&ftLibrary);
        if (ftError) {
            plExit();
            return;  // FreeType won't be available, fallback to ASCII
        }
        
        // Create font face from memory (system font data)
        ftError = FT_New_Memory_Face(
            ftLibrary,
            (const FT_Byte*)fontData.address,  // font data from pl service
            fontData.size,                      // size in bytes
            0,                                  // face index
            &ftFace
        );
        if (ftError) {
            FT_Done_FreeType(ftLibrary);
            plExit();
            return;  // FreeType won't be available, fallback to ASCII
        }
        
        // Set character size (8pt at 96 DPI - matching 8x8 ASCII font)
        ftError = FT_Set_Char_Size(
            ftFace,
            0,        // char_width in 1/64th of points (0 = same as height)
            8*64,     // char_height in 1/64th of points
            96,       // horizontal device resolution
            96        // vertical device resolution
        );
        if (ftError) {
            FT_Done_Face(ftFace);
            FT_Done_FreeType(ftLibrary);
            plExit();
            return;  // FreeType won't be available, fallback to ASCII
        }
        
        freetypeInitialized = true;
    }

    PKSEFramebuffer::~PKSEFramebuffer() {
        if (freetypeInitialized) {
            FT_Done_Face(ftFace);
            FT_Done_FreeType(ftLibrary);
            plExit();
        }
        framebufferClose(&fb);
    }

    void PKSEFramebuffer::clear(Color color) {
        u32 colorValue = color.toRGBA8();
        for (u32 y = 0; y < height; y++) {
            for (u32 x = 0; x < width; x++) {
                framebuf[y * stride / sizeof(u32) + x] = colorValue;
            }
        }
    }

    void PKSEFramebuffer::drawPixel(int x, int y, Color color) {
        if (x < 0 || x >= (int)width || y < 0 || y >= (int)height)
            return;

        framebuf[y * stride / sizeof(u32) + x] = color.toRGBA8();
    }

    void PKSEFramebuffer::drawRect(int x, int y, int w, int h, Color color) {
        // Top and bottom borders
        for (int i = 0; i < w; i++) {
            drawPixel(x + i, y, color);
            drawPixel(x + i, y + h - 1, color);
        }
        // Left and right borders
        for (int i = 0; i < h; i++) {
            drawPixel(x, y + i, color);
            drawPixel(x + w - 1, y + i, color);
        }
    }

    void PKSEFramebuffer::drawFilledRect(int x, int y, int w, int h, Color color) {
        u32 colorValue = color.toRGBA8();
        for (int dy = 0; dy < h; dy++) {
            for (int dx = 0; dx < w; dx++) {
                int px = x + dx;
                int py = y + dy;
                if (px >= 0 && px < (int)width && py >= 0 && py < (int)height) {
                    framebuf[py * stride / sizeof(u32) + px] = colorValue;
                }
            }
        }
    }

    void PKSEFramebuffer::drawText(int x, int y, const char* text, Color color) {
        // If FreeType is available, use it for full Unicode support
        if (freetypeInitialized) {
            u32 tmpx = x;
            u32 tmpy = y;
            FT_Error ftError;
            FT_UInt glyph_index;
            FT_GlyphSlot slot = ftFace->glyph;
            
            size_t i = 0;
            size_t str_size = strlen(text);
            uint32_t tmpchar;
            ssize_t unitcount = 0;
            
            while (i < str_size) {
                // Use libnx's decode_utf8 function
                unitcount = decode_utf8(&tmpchar, (const uint8_t*)&text[i]);
                if (unitcount <= 0) break;
                i += unitcount;
                
                // Handle newline
                if (tmpchar == '\n') {
                    tmpx = x;
                    tmpy += ftFace->size->metrics.height / 64;
                    continue;
                }
                
                // Get glyph index for this character
                glyph_index = FT_Get_Char_Index(ftFace, tmpchar);
                
                // Load the glyph
                ftError = FT_Load_Glyph(ftFace, glyph_index, FT_LOAD_DEFAULT);
                if (ftError) continue;
                
                // Render the glyph
                ftError = FT_Render_Glyph(ftFace->glyph, FT_RENDER_MODE_NORMAL);
                if (ftError) continue;
                
                // Draw the glyph bitmap
                drawGlyph(&slot->bitmap, tmpx + slot->bitmap_left, tmpy - slot->bitmap_top, color);
                
                // Advance to next character position
                tmpx += slot->advance.x >> 6;
                tmpy += slot->advance.y >> 6;
                
                // Wrap to next line if needed
                if (tmpx > width - 32) {
                    tmpx = x;
                    tmpy += ftFace->size->metrics.height / 64;
                }
            }
            return;
        }
        
        // Fallback to 8x8 bitmap font for ASCII only (if FreeType not available)
        int offsetX = x;
        int offsetY = y;

        while (*text) {
            const uint8_t* glyph = nullptr;
            unsigned char c = static_cast<unsigned char>(*text);
            int bytesToSkip = 1;  // Default: skip 1 byte for ASCII

            // Handle newline
            if (c == '\n') {
                offsetY += 10;
                offsetX = x;
                text++;
                continue;
            }

            // Check if this is a multi-byte UTF-8 sequence
            // For CJK characters (U+4E00-U+9FFF) and other multi-byte sequences,
            // we currently don't have font rendering support, so skip them gracefully
            if ((c >= 0xE4 && c <= 0xE9) || c == 0xE3) {
                // This is likely a CJK character (3-byte UTF-8)
                // Skip all 3 bytes and display a placeholder
                if (text[1] && text[2]) {
                    bytesToSkip = 3;
                    c = '?';  // Use placeholder for CJK characters
                }
            }
            // Handle UTF-8 sequences with custom glyphs or mappings
            // 3-byte UTF-8 sequences (U+0800 to U+FFFF)
            else if (c == 0xE2 && text[1] && text[2]) {
                unsigned char b2 = static_cast<unsigned char>(text[1]);
                unsigned char b3 = static_cast<unsigned char>(text[2]);
                bytesToSkip = 3;

                // Special Pokemon symbols with custom glyphs
                // ★ (U+2605): E2 98 85
                if (b2 == 0x98 && b3 == 0x85) {
                    glyph = glyph_star;
                }
                // ♀ (U+2640): E2 99 80
                else if (b2 == 0x99 && b3 == 0x80) {
                    glyph = glyph_female;
                }
                // ♂ (U+2642): E2 99 82
                else if (b2 == 0x99 && b3 == 0x82) {
                    glyph = glyph_male;
                }
                // Unicode normalization: map common Unicode to ASCII equivalents
                // U+2018-201F: Various quotes and apostrophes
                else if (b2 == 0x80) {
                    // ' (U+2018 LEFT SINGLE QUOTATION MARK): E2 80 98
                    // ' (U+2019 RIGHT SINGLE QUOTATION MARK): E2 80 99
                    if (b3 == 0x98 || b3 == 0x99) {
                        c = '\'';  // Map to ASCII apostrophe
                    }
                    // " (U+201C LEFT DOUBLE QUOTATION MARK): E2 80 9C
                    // " (U+201D RIGHT DOUBLE QUOTATION MARK): E2 80 9D
                    else if (b3 == 0x9C || b3 == 0x9D) {
                        c = '"';  // Map to ASCII quote
                    }
                    // – (U+2013 EN DASH): E2 80 93
                    // — (U+2014 EM DASH): E2 80 94
                    else if (b3 == 0x93 || b3 == 0x94) {
                        c = '-';  // Map to ASCII hyphen
                    }
                    // … (U+2026 HORIZONTAL ELLIPSIS): E2 80 A6
                    else if (b3 == 0xA6) {
                        c = '.';  // Map to period
                    }
                }
            }
            // 2-byte UTF-8 sequences (U+0080 to U+07FF)
            else if ((c & 0xE0) == 0xC0 && text[1]) {
                unsigned char b2 = static_cast<unsigned char>(text[1]);
                bytesToSkip = 2;

                // U+00C0-00FF: Latin Extended characters
                // Map common accented characters to their base forms
                unsigned char b1 = c;
                if (b1 == 0xC3) {  // Latin-1 Supplement
                    // À-Å → A, È-Ë → E, Ì-Ï → I, Ò-Ö → O, Ù-Ü → U
                    // à-å → a, è-ë → e, ì-ï → i, ò-ö → o, ù-ü → u
                    if (b2 >= 0x80 && b2 <= 0x85) c = 'A';  // À-Å
                    else if (b2 >= 0xA0 && b2 <= 0xA5) c = 'a';  // à-å
                    else if (b2 >= 0x88 && b2 <= 0x8B) c = 'E';  // È-Ë
                    else if (b2 >= 0xA8 && b2 <= 0xAB) c = 'e';  // è-ë
                    else if (b2 >= 0x8C && b2 <= 0x8F) c = 'I';  // Ì-Ï
                    else if (b2 >= 0xAC && b2 <= 0xAF) c = 'i';  // ì-ï
                    else if (b2 >= 0x92 && b2 <= 0x96) c = 'O';  // Ò-Ö
                    else if (b2 >= 0xB2 && b2 <= 0xB6) c = 'o';  // ò-ö
                    else if (b2 >= 0x99 && b2 <= 0x9C) c = 'U';  // Ù-Ü
                    else if (b2 >= 0xB9 && b2 <= 0xBC) c = 'u';  // ù-ü
                    else if (b2 == 0x87) c = 'C';  // Ç
                    else if (b2 == 0xA7) c = 'c';  // ç
                    else if (b2 == 0x91) c = 'N';  // Ñ
                    else if (b2 == 0xB1) c = 'n';  // ñ
                }
            }

            // If we have a custom glyph, use it; otherwise use ASCII font
            if (glyph == nullptr) {
                // Only render printable ASCII characters (32-127)
                if (c < 32 || c > 127) {
                    c = '?';  // Replace unsupported characters with '?'
                }
                glyph = font8x8[c - 32];
            }

            // Draw the character using 8x8 bitmap font
            for (int row = 0; row < 8; row++) {
                uint8_t rowData = glyph[row];
                for (int col = 0; col < 8; col++) {
                    if (rowData & (1 << col)) {
                        drawPixel(offsetX + col, offsetY + row, color);
                    }
                }
            }

            offsetX += 8;
            text += bytesToSkip;  // Skip the correct number of bytes

            // Wrap to next line if we exceed width
            if (offsetX > (int)width - 8) {
                offsetX = x;
                offsetY += 10;
            }
        }
    }

    void PKSEFramebuffer::drawText(int x, int y, const std::string& text, Color color) {
        drawText(x, y, text.c_str(), color);
    }

    void PKSEFramebuffer::drawImage(int x, int y, int imgWidth, int imgHeight, const unsigned char* imageData, int channels) {
        if (!imageData) return;

        for (int dy = 0; dy < imgHeight; dy++) {
            for (int dx = 0; dx < imgWidth; dx++) {
                int px = x + dx;
                int py = y + dy;

                // Bounds check
                if (px < 0 || px >= (int)width || py < 0 || py >= (int)height)
                    continue;

                // Calculate source pixel offset
                int srcOffset = (dy * imgWidth + dx) * channels;

                // Extract color components based on channel count
                unsigned char r, g, b, a;
                if (channels == 4) {
                    // RGBA
                    r = imageData[srcOffset + 0];
                    g = imageData[srcOffset + 1];
                    b = imageData[srcOffset + 2];
                    a = imageData[srcOffset + 3];
                } else if (channels == 3) {
                    // RGB (fully opaque)
                    r = imageData[srcOffset + 0];
                    g = imageData[srcOffset + 1];
                    b = imageData[srcOffset + 2];
                    a = 255;
                } else if (channels == 2) {
                    // Grayscale + Alpha
                    r = g = b = imageData[srcOffset + 0];
                    a = imageData[srcOffset + 1];
                } else if (channels == 1) {
                    // Grayscale (fully opaque)
                    r = g = b = imageData[srcOffset + 0];
                    a = 255;
                } else {
                    continue;
                }

                // Alpha blending if not fully opaque
                if (a == 255) {
                    // Fully opaque - direct write
                    u32 color = (a << 24) | (b << 16) | (g << 8) | r;
                    framebuf[py * stride / sizeof(u32) + px] = color;
                } else if (a > 0) {
                    // Semi-transparent - blend with background
                    u32 bgColor = framebuf[py * stride / sizeof(u32) + px];
                    unsigned char bgR = bgColor & 0xFF;
                    unsigned char bgG = (bgColor >> 8) & 0xFF;
                    unsigned char bgB = (bgColor >> 16) & 0xFF;

                    // Alpha blend: result = (src * alpha + bg * (1 - alpha))
                    unsigned char finalR = (r * a + bgR * (255 - a)) / 255;
                    unsigned char finalG = (g * a + bgG * (255 - a)) / 255;
                    unsigned char finalB = (b * a + bgB * (255 - a)) / 255;

                    u32 color = (255 << 24) | (finalB << 16) | (finalG << 8) | finalR;
                    framebuf[py * stride / sizeof(u32) + px] = color;
                }
                // If a == 0, skip (fully transparent)
            }
        }
    }

    void PKSEFramebuffer::drawImageScaled(int x, int y, int imgWidth, int imgHeight, int destWidth, int destHeight, const unsigned char* imageData, int channels) {
        if (!imageData || destWidth <= 0 || destHeight <= 0) return;

        // Use nearest-neighbor sampling for scaling
        for (int dy = 0; dy < destHeight; dy++) {
            for (int dx = 0; dx < destWidth; dx++) {
                int px = x + dx;
                int py = y + dy;

                // Bounds check
                if (px < 0 || px >= (int)width || py < 0 || py >= (int)height)
                    continue;

                // Map destination pixel to source pixel (nearest-neighbor)
                int srcX = (dx * imgWidth) / destWidth;
                int srcY = (dy * imgHeight) / destHeight;

                // Calculate source pixel offset
                int srcOffset = (srcY * imgWidth + srcX) * channels;

                // Extract color components based on channel count
                unsigned char r, g, b, a;
                if (channels == 4) {
                    r = imageData[srcOffset + 0];
                    g = imageData[srcOffset + 1];
                    b = imageData[srcOffset + 2];
                    a = imageData[srcOffset + 3];
                } else if (channels == 3) {
                    r = imageData[srcOffset + 0];
                    g = imageData[srcOffset + 1];
                    b = imageData[srcOffset + 2];
                    a = 255;
                } else if (channels == 2) {
                    r = g = b = imageData[srcOffset + 0];
                    a = imageData[srcOffset + 1];
                } else if (channels == 1) {
                    r = g = b = imageData[srcOffset + 0];
                    a = 255;
                } else {
                    continue;
                }

                // Alpha blending
                if (a == 255) {
                    u32 color = (a << 24) | (b << 16) | (g << 8) | r;
                    framebuf[py * stride / sizeof(u32) + px] = color;
                } else if (a > 0) {
                    u32 bgColor = framebuf[py * stride / sizeof(u32) + px];
                    unsigned char bgR = bgColor & 0xFF;
                    unsigned char bgG = (bgColor >> 8) & 0xFF;
                    unsigned char bgB = (bgColor >> 16) & 0xFF;

                    unsigned char finalR = (r * a + bgR * (255 - a)) / 255;
                    unsigned char finalG = (g * a + bgG * (255 - a)) / 255;
                    unsigned char finalB = (b * a + bgB * (255 - a)) / 255;

                    u32 color = (255 << 24) | (finalB << 16) | (finalG << 8) | finalR;
                    framebuf[py * stride / sizeof(u32) + px] = color;
                }
            }
        }
    }

    void PKSEFramebuffer::flush() {
        framebufferEnd(&fb);
        framebuf = (u32*)framebufferBegin(&fb, &stride);
    }

    // Helper method: Draw a single glyph bitmap from FreeType
    void PKSEFramebuffer::drawGlyph(FT_Bitmap* bitmap, u32 x, u32 y, Color color) {
        if (bitmap->pixel_mode != FT_PIXEL_MODE_GRAY) return;
        
        u32 colorValue = color.toRGBA8();
        u8 r = colorValue & 0xFF;
        u8 g = (colorValue >> 8) & 0xFF;
        u8 b = (colorValue >> 16) & 0xFF;
        
        u8* imageptr = bitmap->buffer;
        
        for (u32 row = 0; row < bitmap->rows; row++) {
            for (u32 col = 0; col < bitmap->width; col++) {
                u32 px = x + col;
                u32 py = y + row;
                
                if (px >= width || py >= height) continue;
                
                u8 alpha = imageptr[col];
                if (alpha > 0) {
                    if (alpha == 255) {
                        // Fully opaque
                        framebuf[py * stride / sizeof(u32) + px] = colorValue;
                    } else {
                        // Alpha blending
                        u32 bgColor = framebuf[py * stride / sizeof(u32) + px];
                        u8 bgR = bgColor & 0xFF;
                        u8 bgG = (bgColor >> 8) & 0xFF;
                        u8 bgB = (bgColor >> 16) & 0xFF;
                        
                        u8 finalR = ((r * alpha) + (bgR * (255 - alpha)) + 255) >> 8;
                        u8 finalG = ((g * alpha) + (bgG * (255 - alpha)) + 255) >> 8;
                        u8 finalB = ((b * alpha) + (bgB * (255 - alpha)) + 255) >> 8;
                        
                        u32 blendedColor = (255 << 24) | (finalB << 16) | (finalG << 8) | finalR;
                        framebuf[py * stride / sizeof(u32) + px] = blendedColor;
                    }
                }
            }
            imageptr += bitmap->pitch;
        }
    }
}
