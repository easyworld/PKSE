#ifndef UI_COMMON_H
#define UI_COMMON_H

#include <cstdint>

namespace UI {
    struct Color {
        uint8_t r, g, b, a;

        constexpr Color(uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha = 255)
            : r(red), g(green), b(blue), a(alpha) {}

        uint32_t toRGBA8() const {
            return (r << 0) | (g << 8) | (b << 16) | (a << 24);
        }
    };

    namespace Colors {
        // --- Fixed literal colors (theme-independent, always constant) ---
        constexpr Color Black(0, 0, 0);
        constexpr Color White(255, 255, 255);
        constexpr Color Gray(128, 128, 128);
        constexpr Color LightGray(192, 192, 192);
        constexpr Color DarkGray(64, 64, 64);
        constexpr Color Red(255, 0, 0);
        constexpr Color Green(0, 255, 0);
        constexpr Color Blue(0, 0, 255);
        constexpr Color Yellow(255, 255, 0);
        constexpr Color Cyan(0, 255, 255);
        constexpr Color Magenta(255, 0, 255);
        constexpr Color Orange(255, 165, 0);

        // --- Semantic theme colors (runtime-swappable by applyTheme()) ---
        // These are mutable so a dark/light toggle can restyle the whole UI without
        // changing the 15+ screens that reference Colors::Text etc. Defaults = dark theme.
        inline Color Background   = Color(28, 27, 38);    // deep indigo-tinted charcoal
        inline Color Panel        = Color(40, 39, 54);    // elevated card surface
        inline Color PanelAlt     = Color(48, 47, 64);    // secondary surface / hover
        inline Color Selected     = Color(78, 70, 130);   // selection highlight (indigo)
        inline Color Border       = Color(60, 58, 78);
        inline Color Text         = Color(234, 233, 242);
        inline Color TextDim      = Color(150, 148, 168);
        inline Color Accent       = Color(139, 122, 255); // indigo/violet accent
        inline Color AccentDim    = Color(96, 84, 178);   // muted accent (bars/underlines)
        // Warm secondary (HOME's "warm-on-cool" pop): selected pill / primary action / Save.
        // Indigo stays the hero; amber is used sparingly for the single primary/selected element.
        inline Color Primary      = Color(255, 184, 77);  // amber
        inline Color PrimaryText  = Color(43, 32, 10);    // dark text drawn on top of amber
        // Attention accent for warning dialog titles ("未保存的更改", "删除备份？"). Theme-aware
        // because a bright amber that reads on the dark UI is nearly invisible on light-mode white.
        inline Color Warning      = Color(255, 199, 66);
        // Shiny marker (star / "是"), HOME-style red rather than yellow. Theme-aware: yellow washed
        // out on light-mode white exactly like Warning did, and red reads on both sprites and panels.
        inline Color ShinyStar    = Color(255, 96, 86);
        // Storage cursor-mode colors, one per CursorMode — a red/blue/green scheme across the three
        // pointer arrows (Menu / Move / Multi). Theme-aware: the light variants are deepened so
        // the arrow, the selection wash and the carried-block backing still read against a white panel.
        inline Color CursorMenu   = Color(232, 92, 92);
        inline Color CursorMove   = Color(86, 148, 244);
        inline Color CursorMulti  = Color(96, 205, 128);

        // Let's Go specific colors (matching in-game UI) — theme-independent
        constexpr Color PartnerHeart(255, 105, 180);    // Hot pink heart for Partner Pokemon
        constexpr Color PartyNumber(255, 200, 50);      // Yellow/amber for party position numbers
        // Party-position badge: a gold disc with a dark digit, drawn behind the number so it stays
        // legible on any sprite in either theme (a bare amber digit washed out on light-mode tiles).
        constexpr Color PartyBadge(255, 193, 68);
        constexpr Color PartyBadgeText(38, 28, 8);
    }

    // Height of the arrowhead on the storage/box grid cursor, in framebuffer pixels. Absolute
    // rather than a fraction of the disc, because the two grids size their discs differently (the
    // Boxes view has more room per cell than the bank's paired panes) -- scaling off the disc gave
    // a visibly bigger cursor in one view than the other.
    constexpr int kGridCursorH = 32;

    // Minimum comfortable touch-target size in framebuffer pixels. The UI renders at 1280x720 on
    // the ~6" handheld screen, so tappable controls (menu rows, dialog buttons, tab/nav hit areas)
    // should be at least this tall/wide for a fingertip. Grid slots are already larger than this.
    constexpr int TouchTargetMin = 56;

    enum class ThemeMode { Dark, Light };

    inline ThemeMode g_themeMode = ThemeMode::Dark;

    // Swap the semantic palette. Screens keep using Colors::Text/Panel/... and pick up
    // the change automatically on the next frame (everything redraws each frame).
    inline void applyTheme(ThemeMode mode) {
        using namespace Colors;
        g_themeMode = mode;
        if (mode == ThemeMode::Dark) {
            Background = Color(28, 27, 38);
            Panel      = Color(40, 39, 54);
            PanelAlt   = Color(48, 47, 64);
            Selected   = Color(78, 70, 130);
            Border     = Color(60, 58, 78);
            Text       = Color(234, 233, 242);
            TextDim    = Color(150, 148, 168);
            Accent     = Color(139, 122, 255);
            AccentDim  = Color(96, 84, 178);
            Primary    = Color(255, 184, 77);
            PrimaryText= Color(43, 32, 10);
            Warning    = Color(255, 199, 66);   // bright gold, reads on the dark UI
            ShinyStar  = Color(255, 96, 86);    // warm coral-red on the dark UI
            CursorMenu = Color(232, 92, 92);
            CursorMove = Color(86, 148, 244);
            CursorMulti= Color(96, 205, 128);
        } else { // Light
            Background = Color(241, 241, 246);
            Panel      = Color(255, 255, 255);
            PanelAlt   = Color(232, 231, 242);
            Selected   = Color(223, 218, 250);
            Border     = Color(214, 214, 224);
            Text       = Color(28, 28, 38);
            TextDim    = Color(110, 110, 128);
            Accent     = Color(109, 90, 230);
            AccentDim  = Color(150, 134, 240);
            Primary    = Color(245, 166, 45);
            PrimaryText= Color(43, 32, 10);
            Warning    = Color(176, 98, 0);     // deep amber, reads on light-mode white
            ShinyStar  = Color(202, 44, 38);    // deep red, reads on light-mode white
            CursorMenu = Color(206, 58, 58);
            CursorMove = Color(38, 106, 214);
            CursorMulti= Color(38, 150, 84);
        }
    }
}

#endif
