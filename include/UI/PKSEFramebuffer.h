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
        
        // Helper method for UTF-8 decoding
        uint32_t utf8ToUnicode(const char*& text);
    };
}

#endif
