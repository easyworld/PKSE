#include "UI/PKSEFramebuffer.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

#include <glad/glad.h>
#include <SDL2/SDL.h>

#include "nanovg.h"
#define NANOVG_GL3
#include "nanovg_gl.h"

#include "UI/SpriteManager.h"   // eviction hook: textures are keyed on sprite buffer addresses
#include "Utils/Logger.h"

using namespace Utils;

namespace UI {

    static constexpr const char* kFontPath = "romfs:/fonts/Nunito.ttf";
    static constexpr const char* kCJKPath  = "romfs:/fonts/NotoSansSC.ttf";
    static constexpr const char* kSymPath  = "romfs:/fonts/NotoSansSymbols.ttf";
    static constexpr const char* kSym2Path = "romfs:/fonts/NotoSansSymbols2.ttf";

    // Nunito point size per TextStyle (Body matches the old single-size text).
    static constexpr float kFontSizes[static_cast<int>(TextStyle::Count)] = {
        15.0f,  // Caption
        19.0f,  // Body
        26.0f,  // Heading
        34.0f,  // Title
    };

    static inline NVGcolor toNVG(Color c) { return nvgRGBA(c.r, c.g, c.b, c.a); }

    // The framebuffer SpriteManager's eviction callback should reach. Only one exists at a time
    // (UIManager owns it), and it is cleared on destruction so a late eviction is a no-op.
    static PKSEFramebuffer* s_activeFramebuffer = nullptr;

    // ---------------------------------------------------------------------------------------------

    PKSEFramebuffer::PKSEFramebuffer()
        : window(nullptr), glContext(nullptr), vg(nullptr),
          fontSans(-1), fontCJK(-1), fontSym(-1), fontSym2(-1), width(1280), height(720) {

        // Request an OpenGL 4.3 core context with a stencil buffer — NanoVG needs stencil for its
        // fills / stencil strokes. This is the proven Switch (mesa/nouveau) config.
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
        SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
        SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
        SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
        SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

        window = SDL_CreateWindow("PKSE", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                  width, height, SDL_WINDOW_OPENGL);
        if (!window) { logErrorToFile("SDL_CreateWindow(OPENGL) failed"); logErrorToFile(SDL_GetError()); return; }

        glContext = SDL_GL_CreateContext(window);
        if (!glContext) { logErrorToFile("SDL_GL_CreateContext failed"); logErrorToFile(SDL_GetError()); return; }
        SDL_GL_MakeCurrent(window, glContext);
        SDL_GL_SetSwapInterval(1);   // vsync

        if (!gladLoadGL()) { logErrorToFile("gladLoadGL failed"); return; }
        glGetError();                // clear any benign startup GL error

        const char* glver = reinterpret_cast<const char*>(glGetString(GL_VERSION));
        if (glver) logInfoToFile("GL_VERSION:", glver);   // should be "OpenGL 4.x", not "OpenGL ES"

        vg = nvgCreateGL3(NVG_ANTIALIAS | NVG_STENCIL_STROKES);
        if (!vg) { logErrorToFile("nvgCreateGL3 failed"); return; }

        // Fonts from romfs (fontstash reads via fopen; romfs is mounted). Add Simplified Chinese
        // and symbol fallbacks so nvgText() automatically resolves every localized UI glyph.
        fontSans = nvgCreateFont(vg, "sans", kFontPath);
        if (fontSans < 0) logErrorToFile("nvgCreateFont Nunito failed");
        fontCJK  = nvgCreateFont(vg, "cjk",  kCJKPath);
        fontSym  = nvgCreateFont(vg, "sym",  kSymPath);
        fontSym2 = nvgCreateFont(vg, "sym2", kSym2Path);
        if (fontSans >= 0 && fontCJK  >= 0) nvgAddFallbackFontId(vg, fontSans, fontCJK);
        if (fontSans >= 0 && fontSym  >= 0) nvgAddFallbackFontId(vg, fontSans, fontSym);
        if (fontSans >= 0 && fontSym2 >= 0) nvgAddFallbackFontId(vg, fontSans, fontSym2);

        s_activeFramebuffer = this;
        SpriteManager::setEvictCallback(&PKSEFramebuffer::onSpriteEvicted);
    }

    void PKSEFramebuffer::onSpriteEvicted(const unsigned char* data) {
        if (s_activeFramebuffer) s_activeFramebuffer->invalidateImage(data);
    }

    void PKSEFramebuffer::invalidateImage(const unsigned char* data) {
        if (!vg || !data) return;
        auto it = imageCache.find(data);
        if (it == imageCache.end()) return;
        if (it->second >= 0) nvgDeleteImage(vg, it->second);
        imageCache.erase(it);
    }

    PKSEFramebuffer::~PKSEFramebuffer() {
        // Stop eviction reaching a half-destroyed object; the loop below frees every texture anyway.
        SpriteManager::setEvictCallback(nullptr);
        if (s_activeFramebuffer == this) s_activeFramebuffer = nullptr;
        if (vg) {
            for (auto& kv : imageCache) if (kv.second >= 0) nvgDeleteImage(vg, kv.second);
            imageCache.clear();
            nvgDeleteGL3(vg);
        }
        if (glContext) SDL_GL_DeleteContext(glContext);
        if (window) SDL_DestroyWindow(window);
    }

    // Begin a NanoVG frame if one isn't active yet (safety for draws before clear()).
    // Returns true if vg is valid and a frame is ready.
    bool PKSEFramebuffer::ensureFrame() {
        if (!vg) return false;
        if (!frameActive) {
            nvgBeginFrame(vg, static_cast<float>(width), static_cast<float>(height), 1.0f);
            frameActive = true;
        }
        return true;
    }

    void PKSEFramebuffer::clear(Color color) {
        if (!vg) return;
        if (frameActive) { nvgEndFrame(vg); frameActive = false; }   // close a stray previous frame
        glViewport(0, 0, width, height);
        glClearColor(color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, color.a / 255.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
        nvgBeginFrame(vg, static_cast<float>(width), static_cast<float>(height), 1.0f);
        frameActive = true;
    }

    void PKSEFramebuffer::flush() {
        if (vg && frameActive) { nvgEndFrame(vg); frameActive = false; }
        if (window) SDL_GL_SwapWindow(window);

        uint64_t now = SDL_GetPerformanceCounter();
        if (lastCounter != 0) {
            double freq = static_cast<double>(SDL_GetPerformanceFrequency());
            deltaSeconds = static_cast<float>(static_cast<double>(now - lastCounter) / freq);
            if (deltaSeconds > 0.1f) deltaSeconds = 0.1f;   // guard against huge dt after a pause
        }
        lastCounter = now;
        totalSeconds += deltaSeconds;
    }

    // ---- Basic primitives (all anti-aliased by NanoVG) ----

    void PKSEFramebuffer::drawPixel(int x, int y, Color color) {
        if (!ensureFrame()) return;
        nvgBeginPath(vg); nvgRect(vg, (float)x, (float)y, 1.0f, 1.0f);
        nvgFillColor(vg, toNVG(color)); nvgFill(vg);
    }

    void PKSEFramebuffer::drawFilledRect(int x, int y, int w, int h, Color color) {
        if (!ensureFrame()) return;
        nvgBeginPath(vg); nvgRect(vg, (float)x, (float)y, (float)w, (float)h);
        nvgFillColor(vg, toNVG(color)); nvgFill(vg);
    }

    // Clip subsequent drawing to a rect (nvgScissor) so a scrolled region's rows don't spill over
    // neighbouring chrome. clearClip() lifts the restriction; both are no-ops without an active frame.
    void PKSEFramebuffer::setClipRect(int x, int y, int w, int h) {
        if (!ensureFrame()) return;
        nvgScissor(vg, (float)x, (float)y, (float)w, (float)h);
    }
    void PKSEFramebuffer::clearClip() {
        if (!ensureFrame()) return;
        nvgResetScissor(vg);
    }

    void PKSEFramebuffer::drawRect(int x, int y, int w, int h, Color color) {
        if (!ensureFrame()) return;
        nvgBeginPath(vg); nvgRect(vg, x + 0.5f, y + 0.5f, w - 1.0f, h - 1.0f);
        nvgStrokeColor(vg, toNVG(color)); nvgStrokeWidth(vg, 1.0f); nvgStroke(vg);
    }

    void PKSEFramebuffer::drawFilledRoundedRect(int x, int y, int w, int h, int r, Color color) {
        if (w <= 0 || h <= 0 || !ensureFrame()) return;
        int rmax = std::min(w, h) / 2; if (r > rmax) r = rmax; if (r < 0) r = 0;
        nvgBeginPath(vg); nvgRoundedRect(vg, (float)x, (float)y, (float)w, (float)h, (float)r);
        nvgFillColor(vg, toNVG(color)); nvgFill(vg);
    }

    void PKSEFramebuffer::drawRoundedRect(int x, int y, int w, int h, int r, Color color, int thickness) {
        if (w <= 0 || h <= 0 || thickness <= 0 || !ensureFrame()) return;
        int rmax = std::min(w, h) / 2; if (r > rmax) r = rmax; if (r < 0) r = 0;
        float t = (float)thickness;
        nvgBeginPath(vg);
        nvgRoundedRect(vg, x + t * 0.5f, y + t * 0.5f, w - t, h - t, std::max(0.0f, r - t * 0.5f));
        nvgStrokeColor(vg, toNVG(color)); nvgStrokeWidth(vg, t); nvgStroke(vg);
    }

    void PKSEFramebuffer::drawFilledCircle(int cx, int cy, int r, Color color) {
        if (r <= 0 || !ensureFrame()) return;
        nvgBeginPath(vg); nvgCircle(vg, (float)cx, (float)cy, (float)r);
        nvgFillColor(vg, toNVG(color)); nvgFill(vg);
    }

    void PKSEFramebuffer::drawCircle(int cx, int cy, int r, Color color, int thickness) {
        if (r <= 0 || !ensureFrame()) return;
        float t = (float)thickness;
        nvgBeginPath(vg); nvgCircle(vg, (float)cx, (float)cy, r - t * 0.5f);
        nvgStrokeColor(vg, toNVG(color)); nvgStrokeWidth(vg, t); nvgStroke(vg);
    }

    void PKSEFramebuffer::drawEgg(int cx, int cy, int size) {
        // Classic Pokemon egg: a cream oval (taller than wide) with a few teal spots.
        const int rx = std::max(4, static_cast<int>(size * 0.30f));
        const int ry = std::max(5, static_cast<int>(size * 0.40f));
        drawFilledEllipse(cx, cy, rx + 2, ry + 2, Color(206, 196, 168, 255));  // subtle edge ring
        drawFilledEllipse(cx, cy, rx, ry, Color(238, 232, 210, 255));          // cream shell
        const Color spot(120, 198, 176, 255);                                  // teal spots
        drawFilledCircle(cx - rx / 3, cy - ry / 3, std::max(2, size / 13), spot);
        drawFilledCircle(cx + rx / 3, cy,          std::max(2, size / 16), spot);
        drawFilledCircle(cx - rx / 5, cy + ry / 3, std::max(2, size / 18), spot);
    }

    void PKSEFramebuffer::drawShinyMark(int x, int y, int size, Color color) {
        // Pokemon HOME's shiny mark: a four-pointed sparkle. The four tips sit on the cardinal
        // directions; the edges between them curve INWARD (a quad curve pulled toward a control
        // point near the center), so it reads as a pinched "twinkle" rather than a plain star.
        // Anchored top-left in a size×size box to match how drawSymbol placed the old ★ glyph.
        if (size <= 1 || !ensureFrame()) return;
        const float R  = size * 0.5f;          // tip radius (half the box)
        const float c  = R * 0.34f;            // control radius -> depth of the concave pinch
        const float ck = c * 0.70710678f;      // c * cos45, the diagonal control offset
        const float cx = x + R, cy = y + R;
        nvgBeginPath(vg);
        nvgMoveTo(vg, cx,      cy - R);                         // top tip
        nvgQuadTo(vg, cx + ck, cy - ck, cx + R, cy);           // -> right tip
        nvgQuadTo(vg, cx + ck, cy + ck, cx,     cy + R);       // -> bottom tip
        nvgQuadTo(vg, cx - ck, cy + ck, cx - R, cy);           // -> left tip
        nvgQuadTo(vg, cx - ck, cy - ck, cx,     cy - R);       // -> back to top tip
        nvgClosePath(vg);
        nvgFillColor(vg, toNVG(color)); nvgFill(vg);
    }

    void PKSEFramebuffer::drawPill(int x, int y, int w, int h, Color color) {
        drawFilledRoundedRect(x, y, w, h, h / 2, color);
    }
    void PKSEFramebuffer::drawPillBorder(int x, int y, int w, int h, Color color, int thickness) {
        drawRoundedRect(x, y, w, h, h / 2, color, thickness);
    }

    void PKSEFramebuffer::drawFilledEllipse(int cx, int cy, int rx, int ry, Color color) {
        if (rx <= 0 || ry <= 0 || !ensureFrame()) return;
        nvgBeginPath(vg); nvgEllipse(vg, (float)cx, (float)cy, (float)rx, (float)ry);
        nvgFillColor(vg, toNVG(color)); nvgFill(vg);
    }

    void PKSEFramebuffer::drawSoftShadow(int x, int y, int w, int h, int r) {
        if (w <= 0 || h <= 0 || !ensureFrame()) return;
        // Real feathered drop shadow: box gradient in the ring around the element (hole in the middle).
        NVGpaint sh = nvgBoxGradient(vg, (float)x, y + 3.0f, (float)w, (float)h, r + 2.0f, 12.0f,
                                     nvgRGBA(0, 0, 0, 110), nvgRGBA(0, 0, 0, 0));
        nvgBeginPath(vg);
        nvgRect(vg, x - 14.0f, y - 14.0f, w + 28.0f, h + 28.0f);
        nvgRoundedRect(vg, (float)x, (float)y, (float)w, (float)h, (float)r);
        nvgPathWinding(vg, NVG_HOLE);
        nvgFillPaint(vg, sh); nvgFill(vg);
    }

    void PKSEFramebuffer::drawVerticalGradient(int x, int y, int w, int h, Color top, Color bottom) {
        if (w <= 0 || h <= 0 || !ensureFrame()) return;
        NVGpaint p = nvgLinearGradient(vg, (float)x, (float)y, (float)x, (float)(y + h), toNVG(top), toNVG(bottom));
        nvgBeginPath(vg); nvgRect(vg, (float)x, (float)y, (float)w, (float)h);
        nvgFillPaint(vg, p); nvgFill(vg);
    }

    void PKSEFramebuffer::drawStatHexagon(int cx, int cy, int R, const float* values, int count,
                                          float maxValue, Color fill, Color web, Color outline) {
        if (R <= 0 || count <= 0 || !values || !ensureFrame()) return;
        static const double kAngles[6] = { 90.0, 30.0, -30.0, 270.0, 210.0, 150.0 };
        const double kPi = 3.14159265358979323846;
        int n = count < 6 ? count : 6;
        auto vx = [&](double deg, double rad) { return cx + (float)(std::cos(deg * kPi / 180.0) * rad); };
        auto vy = [&](double deg, double rad) { return cy - (float)(std::sin(deg * kPi / 180.0) * rad); };

        // Web rings.
        for (int ring = 1; ring <= 4; ++ring) {
            double rr = R * (ring / 4.0);
            nvgBeginPath(vg);
            for (int i = 0; i < n; ++i) {
                float px = vx(kAngles[i], rr), py = vy(kAngles[i], rr);
                if (i == 0) nvgMoveTo(vg, px, py); else nvgLineTo(vg, px, py);
            }
            nvgClosePath(vg);
            nvgStrokeColor(vg, toNVG(web)); nvgStrokeWidth(vg, 1.0f); nvgStroke(vg);
        }
        // Spokes.
        nvgBeginPath(vg);
        for (int i = 0; i < n; ++i) { nvgMoveTo(vg, (float)cx, (float)cy); nvgLineTo(vg, vx(kAngles[i], R), vy(kAngles[i], R)); }
        nvgStrokeColor(vg, toNVG(web)); nvgStrokeWidth(vg, 1.0f); nvgStroke(vg);
        // Data polygon.
        nvgBeginPath(vg);
        for (int i = 0; i < n; ++i) {
            double v = maxValue > 0.0f ? values[i] / maxValue : 0.0;
            v = std::clamp(v, 0.06, 1.0);
            float px = vx(kAngles[i], R * v), py = vy(kAngles[i], R * v);
            if (i == 0) nvgMoveTo(vg, px, py); else nvgLineTo(vg, px, py);
        }
        nvgClosePath(vg);
        nvgFillColor(vg, toNVG(fill)); nvgFill(vg);
        nvgStrokeColor(vg, toNVG(outline)); nvgStrokeWidth(vg, 2.0f); nvgStroke(vg);
    }

    // ---- Text ----

    void PKSEFramebuffer::applyTextStyle(TextStyle style) const {
        nvgFontFaceId(vg, fontSans);
        nvgFontSize(vg, kFontSizes[static_cast<int>(style)]);
    }

    void PKSEFramebuffer::drawText(int x, int y, const char* text, Color color, TextStyle style) {
        if (!text || !*text) return;
        drawText(x, y, std::string(text), color, style);
    }

    void PKSEFramebuffer::drawText(int x, int y, const std::string& text, Color color, TextStyle style) {
        if (text.empty() || !ensureFrame()) return;
        applyTextStyle(style);
        nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
        nvgFillColor(vg, toNVG(color));
        nvgText(vg, (float)x, (float)y, text.c_str(), nullptr);
        // Faux-bold for the display styles (Nunito is loaded regular; re-stroke slightly offset).
        if (style == TextStyle::Heading || style == TextStyle::Title)
            nvgText(vg, x + 0.6f, (float)y, text.c_str(), nullptr);
    }

    void PKSEFramebuffer::drawSymbol(int x, int y, const std::string& symbol, Color color, TextStyle style) {
        // Symbol fonts are fallbacks on "sans", so nvgText resolves ♂/♀/★/♥/◀/▶ automatically.
        if (symbol.empty() || !ensureFrame()) return;
        applyTextStyle(style);
        nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
        nvgFillColor(vg, toNVG(color));
        nvgText(vg, (float)x, (float)y, symbol.c_str(), nullptr);
    }

    void PKSEFramebuffer::measureText(const std::string& text, int& outWidth, int& outHeight, TextStyle style) {
        outWidth = 0; outHeight = 0;
        if (!vg) return;
        applyTextStyle(style);
        nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
        float adv = nvgTextBounds(vg, 0, 0, text.c_str(), nullptr, nullptr);
        outWidth = (int)std::ceil(adv);
        outHeight = lineHeight(style);
    }

    int PKSEFramebuffer::lineHeight(TextStyle style) const {
        if (!vg) return 0;
        applyTextStyle(style);
        float asc = 0, desc = 0, lh = 0;
        nvgTextMetrics(vg, &asc, &desc, &lh);
        return (int)std::ceil(lh);
    }

    // ---- Images / sprites ----

    int PKSEFramebuffer::nvgImageFor(const unsigned char* data, int w, int h, int channels) {
        if (!vg || !data || w <= 0 || h <= 0) return -1;
        auto it = imageCache.find(data);
        if (it != imageCache.end()) return it->second;

        // Mipmaps so downscaled sprites (box grid / party / menu) stay crisp instead of shimmering.
        const int flags = NVG_IMAGE_GENERATE_MIPMAPS;
        int img = -1;
        if (channels == 4) {
            img = nvgCreateImageRGBA(vg, w, h, flags, data);
        } else if (channels == 3) {
            std::vector<unsigned char> rgba(static_cast<size_t>(w) * h * 4);
            for (int i = 0; i < w * h; ++i) {
                rgba[i * 4 + 0] = data[i * 3 + 0];
                rgba[i * 4 + 1] = data[i * 3 + 1];
                rgba[i * 4 + 2] = data[i * 3 + 2];
                rgba[i * 4 + 3] = 255;
            }
            img = nvgCreateImageRGBA(vg, w, h, flags, rgba.data());
        } else {
            return -1;
        }
        imageCache[data] = img;
        return img;
    }

    void PKSEFramebuffer::drawImage(int x, int y, int w, int h, const unsigned char* data, int channels) {
        if (!ensureFrame()) return;
        int img = nvgImageFor(data, w, h, channels);
        if (img < 0) return;
        NVGpaint p = nvgImagePattern(vg, (float)x, (float)y, (float)w, (float)h, 0.0f, img, 1.0f);
        nvgBeginPath(vg); nvgRect(vg, (float)x, (float)y, (float)w, (float)h);
        nvgFillPaint(vg, p); nvgFill(vg);
    }

    void PKSEFramebuffer::drawImageScaled(int x, int y, int w, int h, int destW, int destH,
                                          const unsigned char* data, int channels) {
        if (!ensureFrame()) return;
        int img = nvgImageFor(data, w, h, channels);
        if (img < 0) return;
        NVGpaint p = nvgImagePattern(vg, (float)x, (float)y, (float)destW, (float)destH, 0.0f, img, 1.0f);
        nvgBeginPath(vg); nvgRect(vg, (float)x, (float)y, (float)destW, (float)destH);
        nvgFillPaint(vg, p); nvgFill(vg);
    }

    void PKSEFramebuffer::drawSpriteIdle(int x, int y, int boxW, int boxH, int srcW, int srcH,
                                         const unsigned char* data, int channels, float phase) {
        if (!data || boxW <= 0 || boxH <= 0) return;
        double s = std::sin(totalSeconds * 2.7 + phase);
        int bob = (int)std::lround(s * 3.0);
        int dw = (int)std::lround(boxW * (1.0 - s * 0.03));
        int dh = (int)std::lround(boxH * (1.0 + s * 0.03));
        int baseY = y + boxH;
        int dx = x + (boxW - dw) / 2;
        int dy = baseY - dh - bob;
        int shRx = (int)std::lround(boxW * 0.30 - s * 2.0);
        int shRy = (int)std::lround(5.0 - s * 1.0);
        drawFilledEllipse(x + boxW / 2, baseY - 1, shRx, shRy, Color(0, 0, 0, 80));
        drawImageScaled(dx, dy, srcW, srcH, dw, dh, data, channels);
    }

    // ---- Themed helpers (compose the primitives above) ----

    void PKSEFramebuffer::drawCard(int x, int y, int w, int h) {
        drawFilledRoundedRect(x, y, w, h, 14, Colors::Panel);
        drawRoundedRect(x, y, w, h, 14, Colors::Border, 1);
    }

    void PKSEFramebuffer::drawSelectionHighlight(int x, int y, int w, int h) {
        drawFilledRoundedRect(x, y, w, h, 10, Colors::Selected);
        drawRoundedRect(x, y, w, h, 10, Colors::Accent, 2);
    }

    void PKSEFramebuffer::drawHDivider(int x, int y, int w) {
        drawFilledRect(x, y, w, 2, Colors::Border);
    }

    int PKSEFramebuffer::drawTypeBadge(int x, int y, const std::string& typeName, Color typeColor) {
        const int h = 24;
        int tw, th; measureText(typeName, tw, th, TextStyle::Caption);
        const int padX = 12, total = tw + padX * 2;
        drawFilledRoundedRect(x, y, total, h, h / 2, typeColor);
        int lum = (typeColor.r * 299 + typeColor.g * 587 + typeColor.b * 114) / 1000;
        Color txt = lum > 150 ? Color(30, 30, 36) : Color(245, 245, 250);
        drawText(x + padX, y + (h - th) / 2, typeName, txt, TextStyle::Caption);
        return total;
    }

    void PKSEFramebuffer::drawNameCursorLabel(int cx, int topY, const std::string& text,
                                              Color fill, Color textColor) {
        int tw, th; measureText(text, tw, th, TextStyle::Caption);
        const int padX = 12, padY = 6, tri = 7;
        const int w = tw + padX * 2, h = th + padY * 2;
        int x = cx - w / 2, y = topY - tri - h;
        drawSoftShadow(x, y, w, h, 8);
        drawFilledRoundedRect(x, y, w, h, 8, fill);
        drawText(x + padX, y + padY, text, textColor, TextStyle::Caption);
        // Downward triangle tail.
        if (ensureFrame()) {
            int ty = y + h;
            nvgBeginPath(vg);
            nvgMoveTo(vg, (float)(cx - tri), (float)ty);
            nvgLineTo(vg, (float)(cx + tri), (float)ty);
            nvgLineTo(vg, (float)cx, (float)(ty + tri));
            nvgClosePath(vg);
            nvgFillColor(vg, toNVG(fill)); nvgFill(vg);
        }
    }

    // ---- Screen fade ----

    void PKSEFramebuffer::startFade() { fadeStart = totalSeconds; }

    void PKSEFramebuffer::drawFadeOverlay() {
        constexpr double kDur = 0.22;
        double t = (totalSeconds - fadeStart) / kDur;
        if (t < 0.0 || t >= 1.0) return;
        Uint8 a = static_cast<Uint8>((1.0 - t) * 255.0);
        drawFilledRect(0, 0, width, height, Color(Colors::Background.r, Colors::Background.g, Colors::Background.b, a));
    }
}
