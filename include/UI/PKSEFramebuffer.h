#ifndef UI_PKSE_FRAMEBUFFER_H
#define UI_PKSE_FRAMEBUFFER_H

#include <string>

#include <switch.h>

#include "UI/Common.h"

namespace UI {
    class PKSEFramebuffer {
    public:
        PKSEFramebuffer();
        ~PKSEFramebuffer();

        void clear(Color color);
        void drawPixel(int x, int y, Color color);
        void drawRect(int x, int y, int width, int height, Color color);
        void drawFilledRect(int x, int y, int width, int height, Color color);
        void drawText(int x, int y, const char* text, Color color);
        void drawText(int x, int y, const std::string& text, Color color);
        void drawImage(int x, int y, int imgWidth, int imgHeight, const unsigned char* imageData, int channels);
        void drawImageScaled(int x, int y, int imgWidth, int imgHeight, int destWidth, int destHeight, const unsigned char* imageData, int channels);
        void flush();

        int getWidth() const { return width; }
        int getHeight() const { return height; }

    private:
        Framebuffer fb;  // libnx framebuffer struct
        u32* framebuf;
        u32 width;
        u32 height;
        u32 stride;
        
        // Shared font support for Unicode/Chinese text
        bool plServiceInitialized;  // true if plSharedFont service successfully initialized
        PlFontData standardFont;     // Standard font data (supports Latin, Kana, CJK)
        
        // Helper methods for Unicode text rendering
        void drawTextWithSharedFont(int x, int y, const char* text, Color color, int& outWidth);
        uint32_t utf8ToUnicode(const char*& text);
        void drawGlyphFromSharedFont(int x, int y, uint32_t codepoint, Color color, int& glyphWidth);
    };
}

#endif
