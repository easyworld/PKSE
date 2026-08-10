#ifndef UI_PKSE_FRAMEBUFFER_H
#define UI_PKSE_FRAMEBUFFER_H

#include <cstdint>
#include <map>
#include <string>

#include "UI/Common.h"

// Forward-declare types so we don't pull heavy SDL/GL/NanoVG headers into the 15+ UI files that
// include this header. The concrete includes live only in PKSEFramebuffer.cpp.
struct SDL_Window;
typedef void* SDL_GLContext;
struct NVGcontext;

namespace UI {
    // Typographic scale (Nunito). Body is the default and matches the old single-size
    // text, so existing drawText(...) calls are unchanged. Heading/Title are faux-bold.
    enum class TextStyle { Caption, Body, Heading, Title, Count };

    // NanoVG (OpenGL) 2D surface — GPU anti-aliased vector graphics + text. The public API is
    // deliberately identical to the earlier SDL_Renderer version so every screen/panel/dialog
    // keeps working unchanged; SDL provides only the window, the GL context, and input.
    class PKSEFramebuffer {
    public:
        PKSEFramebuffer();
        ~PKSEFramebuffer();

        void clear(Color color);
        void drawPixel(int x, int y, Color color);
        void drawRect(int x, int y, int width, int height, Color color);
        void drawFilledRect(int x, int y, int width, int height, Color color);
        void drawText(int x, int y, const char* text, Color color, TextStyle style = TextStyle::Body);
        void drawText(int x, int y, const std::string& text, Color color, TextStyle style = TextStyle::Body);

        // Draw a symbol (gender ♂/♀, star ★, heart ♥, ◀ ▶, …) via the fallback symbol font,
        // since the primary UI font (Nunito) doesn't include these glyphs. `style` picks the size,
        // so a symbol placed inside a small badge can be measured and drawn at the same (Caption) size.
        void drawSymbol(int x, int y, const std::string& symbol, Color color, TextStyle style = TextStyle::Body);
        void drawImage(int x, int y, int imgWidth, int imgHeight, const unsigned char* imageData, int channels);
        void drawImageScaled(int x, int y, int imgWidth, int imgHeight, int destWidth, int destHeight, const unsigned char* imageData, int channels);
        void flush();

        // Textures are cached under the ADDRESS of the pixel buffer they were built from, so when
        // the sprite cache frees a buffer its texture has to go too -- otherwise a later sprite
        // allocated at that same address would be drawn with the freed one's image. Hooked up to
        // SpriteManager's eviction callback; onSpriteEvicted forwards to the live framebuffer.
        void invalidateImage(const unsigned char* data);
        static void onSpriteEvicted(const unsigned char* data);

        // Clip subsequent drawing to a rectangle (for a scrollable region); clearClip() removes it.
        // Both need an active frame; primitives outside the clip rect are not rendered.
        void setClipRect(int x, int y, int w, int h);
        void clearClip();

        // --- Higher-level themed helpers ---
        // Encapsulate the current look so panels stay simple and stay consistent when the
        // theme changes. All pull from the active Colors palette.
        void drawCard(int x, int y, int w, int h);                // surface fill + border outline
        void drawSelectionHighlight(int x, int y, int w, int h);  // selection fill + accent left edge
        void drawHDivider(int x, int y, int w);                   // subtle horizontal rule
        void drawFilledEllipse(int cx, int cy, int rx, int ry, Color color);  // e.g. soft shadows

        // --- Rounded / vector primitives (HOME look, Phase 4) ---
        // Anti-aliased on the horizontal edges of rounded corners (fractional coverage);
        // straight edges are exact. `r` is the corner radius (clamped to min(w,h)/2).
        void drawFilledRoundedRect(int x, int y, int w, int h, int r, Color color);
        void drawRoundedRect(int x, int y, int w, int h, int r, Color color, int thickness = 1);
        void drawFilledCircle(int cx, int cy, int r, Color color);
        void drawCircle(int cx, int cy, int r, Color color, int thickness = 1);
        // Draws a Pokemon egg (cream oval + teal spots) centered at (cx,cy), ~size px tall.
        // Used in the box/storage grids in place of the species sprite when a mon is an egg.
        void drawEgg(int cx, int cy, int size);
        // Draws HOME's shiny mark -- a four-pointed sparkle (concave-sided star) --
        // filling a size×size box whose top-left is (x,y), so it drops into existing marker
        // rows the same way drawSymbol did. Replaces the old ★ glyph everywhere shiny is shown.
        void drawShinyMark(int x, int y, int size, Color color);
        // Storage-grid cursor: a wide arrowhead pointing straight down, no shaft. Its POINT lands on
        // (tipX, tipY) with the head above, symmetric about that x. `headHeight` sizes the head --
        // the visible arrow; the mitred outline runs on below it to the point at tipY, adding ~27%
        // more. Drawn in the active cursor mode's colour.
        void drawPointerCursor(int tipX, int tipY, int headHeight, Color color);
        // Stadium (fully-rounded) pill = rounded rect with r = h/2. HOME's headers/badges/buttons.
        void drawPill(int x, int y, int w, int h, Color color);
        void drawPillBorder(int x, int y, int w, int h, Color color, int thickness = 1);
        // Soft drop shadow / elevation under a card, pill, or bar (layered translucent rounded rects).
        void drawSoftShadow(int x, int y, int w, int h, int r);
        // Vertical two-stop gradient (screen backdrops, header/pill fills).
        void drawVerticalGradient(int x, int y, int w, int h, Color top, Color bottom);
        // The signature stat radar. `values` must be in HOME vertex order
        // [HP, Attack, Defense, Speed, Sp.Def, Sp.Atk] (HP at top, clockwise). Draws the
        // web + spokes, a translucent filled polygon, and the outline. Labels are the caller's job.
        void drawStatHexagon(int cx, int cy, int R, const float* values, int count,
                             float maxValue, Color fill, Color web, Color outline);
        // Two-tone rounded type badge (HOME style): colored icon chip + name. Returns total width drawn.
        int  drawTypeBadge(int x, int y, const std::string& typeName, Color typeColor);
        // Draw a sprite with a gentle idle animation (bob + breathe) and a soft ground
        // shadow, driven by the frame tick. (x,y,boxW,boxH) is the nominal placement box;
        // (srcW,srcH) the sprite's native size; `phase` offsets the timing per sprite.
        void drawSpriteIdle(int x, int y, int boxW, int boxH, int srcW, int srcH,
                            const unsigned char* data, int channels, float phase);

        int getWidth() const { return width; }
        int getHeight() const { return height; }

        // Screen transition fade (Phase 4.6). startFade() marks "now" as the fade start; call it when
        // a screen begins. drawFadeOverlay() draws a full-screen Background-colored veil that fades
        // from opaque to clear over ~0.22s (so the content materializes) — call it each frame just
        // before flush(). A no-op once the fade has elapsed.
        void startFade();
        void drawFadeOverlay();

        // Frame timing (updated each flush()). Use these to drive animation without
        // every screen having to read a clock. deltaSeconds = time since last frame.
        float  getDeltaSeconds() const { return deltaSeconds; }
        double getTimeSeconds()  const { return totalSeconds; }

        // Exposed so callers can do their own layout math (centering, right-align, wrapping).
        // Returns the pixel size of `text` in the given style's font.
        void measureText(const std::string& text, int& outWidth, int& outHeight, TextStyle style = TextStyle::Body);
        // Line height (font ascent+descent) for a style — for consistent vertical spacing.
        int  lineHeight(TextStyle style = TextStyle::Body) const;

    private:
        // Lazily build + cache a NanoVG image from a raw sprite pixel buffer, keyed by the buffer
        // pointer (SpriteManager caches sprites for the session, so pointers are stable). Converts
        // RGB→RGBA if needed (NanoVG images are RGBA). Returns the image handle (>=0), or -1.
        int  nvgImageFor(const unsigned char* data, int w, int h, int channels);
        // Set NanoVG's font face + size for a text style (Heading/Title get a heavier faux-bold).
        void applyTextStyle(TextStyle style) const;
        // Begin the NanoVG frame lazily if one isn't already active. Returns true if vg is valid
        // and a frame is ready to draw into (so callers do `if (!ensureFrame()) return;`).
        bool ensureFrame();

        SDL_Window*   window;
        SDL_GLContext glContext;
        NVGcontext*   vg;
        int fontSans;                // Nunito (primary); CJK and symbol fonts are fallbacks
        int fontCJK;                 // Noto Sans SC (Simplified Chinese)
        int fontSym;                 // Noto Sans Symbols (♂ ♀ ★ …)
        int fontSym2;                // Noto Sans Symbols 2 (card suits ♥ …)
        bool frameActive = false;    // true between the frame's first draw/clear and flush()
        int width;
        int height;

        // Frame timing (see getDeltaSeconds/getTimeSeconds).
        uint64_t lastCounter = 0;
        float    deltaSeconds = 0.0f;
        double   totalSeconds = 0.0;
        double   fadeStart = -100.0;   // start time of the current screen fade (see startFade)

        // NanoVG image handles keyed by the source pixel-buffer pointer.
        std::map<const unsigned char*, int> imageCache;
    };
}

#endif
