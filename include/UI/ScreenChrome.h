#ifndef UI_SCREEN_CHROME_H
#define UI_SCREEN_CHROME_H

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include "UI/PKSEFramebuffer.h"
#include "UI/Common.h"
#include "UI/TouchInput.h"

namespace UI {
    // ---------------------------------------------------------------------------------------------
    // HOME-style screen chrome.
    //
    // Both bars are rounded SHEETS whose far edge runs off-screen, so only the edge facing the
    // content is curved: the header curves along its bottom, the nav bar along its top. A soft
    // shadow does the separating, which is why neither carries a hard divider rule any more.
    // ---------------------------------------------------------------------------------------------

    constexpr int kChromeRadius = 18;   // curve on the content-facing edge of both bars
    constexpr int kHeaderH      = 64;
    constexpr int kNavBarH      = 46;

    // --- Controller-button badges -----------------------------------------------------------------

    // Draw (or, with measureOnly, just measure) one controller badge, shaped like the real button:
    // face buttons are round, shoulders are rounded rectangles, +/- are round with a drawn bar, and
    // the d-pad is a cross with the unused axis dimmed. `cy` is the badge's vertical CENTRE.
    // Returns the width consumed, or 0 if the token isn't a button PKSE knows how to draw.
    inline int buttonGlyph(PKSEFramebuffer& fb, int x, int cy, const std::string& btn, bool measureOnly) {
        // Mid-grey badge with the bar colour as ink. Both are theme colours, so the pair inverts
        // itself: dark letter on light grey in the dark theme, white letter on grey in the light one.
        const Color fill = Colors::TextDim;
        const Color ink  = Colors::Panel;
        constexpr int kR = 12;              // face-button radius

        auto centred = [&](const std::string& s, int bx, int bw) {
            int tw, th; fb.measureText(s, tw, th, TextStyle::Caption);
            fb.drawText(bx + (bw - tw) / 2, cy - th / 2, s, ink, TextStyle::Caption);
        };

        // Face buttons.
        if (btn.size() == 1 && (btn[0] == 'A' || btn[0] == 'B' || btn[0] == 'X' || btn[0] == 'Y')) {
            if (!measureOnly) { fb.drawFilledCircle(x + kR, cy, kR, fill); centred(btn, x, kR * 2); }
            return kR * 2;
        }

        // Plus / Minus. The bars are drawn rather than typed -- Nunito's '+' and '-' are far too
        // thin to read at badge size, and '-' sits at x-height instead of centred.
        if (btn == "+" || btn == "加号键" || btn == "-" || btn == "减号键") {
            if (!measureOnly) {
                fb.drawFilledCircle(x + kR, cy, kR, fill);
                fb.drawFilledRoundedRect(x + kR - 6, cy - 1, 13, 3, 1, ink);
                if (btn == "+" || btn == "加号键")
                    fb.drawFilledRoundedRect(x + kR - 1, cy - 6, 3, 13, 1, ink);
            }
            return kR * 2;
        }

        // Shoulders, and the slash pairs the hints use ("L/R", "ZL/ZR"): rounded rects sized to text.
        if (btn == "L" || btn == "R" || btn == "ZL" || btn == "ZR" || btn == "L/R" || btn == "ZL/ZR") {
            int tw, th; fb.measureText(btn, tw, th, TextStyle::Caption);
            const int w = tw + 14, h = 22;
            if (!measureOnly) { fb.drawFilledRoundedRect(x, cy - h / 2, w, h, 7, fill); centred(btn, x, w); }
            return w;
        }

        // D-pad. "上／下" and "左／右" dim the axis they don't use, so the badge itself says
        // which way the stick moves rather than relying on the label to explain it.
        if (btn == "方向键" || btn == "上／下" || btn == "左／右") {
            constexpr int s = 24, a = 9;    // overall size, arm thickness
            if (!measureOnly) {
                const Color dim(fill.r, fill.g, fill.b, 70);
                const bool vOnly = (btn == "上／下"), hOnly = (btn == "左／右");
                const int vx = x + (s - a) / 2, vy = cy - s / 2;
                const int hx = x,               hy = cy - a / 2;
                // Dim arm first, so the solid one wins where they overlap in the middle.
                if (vOnly) {
                    fb.drawFilledRoundedRect(hx, hy, s, a, 3, dim);
                    fb.drawFilledRoundedRect(vx, vy, a, s, 3, fill);
                } else if (hOnly) {
                    fb.drawFilledRoundedRect(vx, vy, a, s, 3, dim);
                    fb.drawFilledRoundedRect(hx, hy, s, a, 3, fill);
                } else {
                    fb.drawFilledRoundedRect(vx, vy, a, s, 3, fill);
                    fb.drawFilledRoundedRect(hx, hy, s, a, 3, fill);
                }
            }
            return s;
        }

        // A single d-pad direction: a rounded-square badge with a geometric triangle arrow. Used by
        // the edit dialogs' -1 / +1 steps (the shoulders take the +/-10 and +/-100 steps).
        if (btn == "左" || btn == "右" || btn == "上" || btn == "下") {
            constexpr int s = 22;
            if (!measureOnly) {
                fb.drawFilledRoundedRect(x, cy - s / 2, s, s, 6, fill);
                const std::string tri = btn == "左"  ? "\xE2\x97\x80"    // ◀
                                      : btn == "右" ? "\xE2\x96\xB6"    // ▶
                                      : btn == "上"    ? "\xE2\x96\xB2"    // ▲
                                      :                  "\xE2\x96\xBC";   // ▼
                // Measure AND draw the arrow at Caption size -- drawSymbol otherwise defaults to Body,
                // which overflowed this 22px badge and mis-centred the glyph (the ◀ was clipping away).
                int tw, th; fb.measureText(tri, tw, th, TextStyle::Caption);
                fb.drawSymbol(x + (s - tw) / 2, cy - th / 2, tri, ink, TextStyle::Caption);
            }
            return s;
        }
        return 0;   // not a button we have a badge for
    }

    inline int buttonGlyphWidth(PKSEFramebuffer& fb, const std::string& btn) {
        return buttonGlyph(fb, 0, 0, btn, true);
    }

    // A pressable button that carries its controller badge ON the button (glyph + label, centred),
    // instead of relying on a separate "A：确认" guide line below it. `fill` lets destructive
    // actions stay red; `textColor` keeps the label legible on that fill. Screen-independent (does
    // NOT register a touch target) so any screen can use it and wire its own hit region --
    // drawEditChoiceButton wraps this for the TrainerViewScreen dialogs.
    inline void drawGlyphButton(PKSEFramebuffer& fb, int bx, int by, int bw, int bh,
                                const std::string& glyph, const std::string& label,
                                Color fill = Colors::PanelAlt, Color textColor = Colors::Text) {
        fb.drawFilledRoundedRect(bx, by, bw, bh, 8, fill);
        fb.drawRoundedRect(bx, by, bw, bh, 8, Colors::Border, 1);
        const int gw = buttonGlyphWidth(fb, glyph);
        int lw, lh; fb.measureText(label, lw, lh);
        const int gx = bx + (bw - (gw + 10 + lw)) / 2;
        buttonGlyph(fb, gx, by + bh / 2, glyph, false);
        fb.drawText(gx + gw + 10, by + (bh - lh) / 2, label, textColor);
    }

    // --- Tappable badges --------------------------------------------------------------------
    //
    // Every screen already publishes a contextual hint string, so making the badges tappable gives
    // HOME-style on-screen action buttons everywhere at once. A tap resolves to that button's press
    // and the screen ORs it into padGetButtonsDown, so no existing handler changes -- there is one
    // input path, not two, and a tap can never diverge from what the physical button does.
    //
    // Only badges standing for exactly ONE button get a hit region. "L/R", "ZL/ZR" and the d-pad stay
    // informational: a single badge can't say whether you meant L or R, and splitting one in half
    // would leave each half far below a usable touch target.
    struct NavHit { int x, y, w, h; uint64_t button; };
    inline std::vector<NavHit> g_navHits;

    inline uint64_t navButtonFor(const std::string& btn) {
        if (btn == "A") return HidNpadButton_A;
        if (btn == "B") return HidNpadButton_B;
        if (btn == "X") return HidNpadButton_X;
        if (btn == "Y") return HidNpadButton_Y;
        if (btn == "+"  || btn == "加号键")  return HidNpadButton_Plus;
        if (btn == "-"  || btn == "减号键") return HidNpadButton_Minus;
        if (btn == "L")  return HidNpadButton_L;
        if (btn == "R")  return HidNpadButton_R;
        if (btn == "ZL") return HidNpadButton_ZL;
        if (btn == "ZR") return HidNpadButton_ZR;
        return 0;   // multi-button or directional badge: informational only
    }

    // Hit-test a fresh tap against the badges captured during the PREVIOUS frame's draw (same
    // one-frame-late contract as TrainerViewScreen::touchedButtonId). Returns a mask to fold into
    // kDown, or 0. Edge-triggered, so it behaves exactly like padGetButtonsDown.
    inline uint64_t navTouchButton(const TouchInput& touch) {
        if (!touch.justPressed()) return 0;
        for (const NavHit& h : g_navHits) {
            if (touch.x() >= h.x && touch.x() < h.x + h.w &&
                touch.y() >= h.y && touch.y() < h.y + h.h)
                return h.button;
        }
        return 0;
    }

    // --- Hint layout ------------------------------------------------------------------------------

    // Lay out a "Btn: Label  |  Btn: Label" hint as badge+label pairs, centred within [x, x+w] on
    // `cy`. A segment with no colon (e.g. "HOLDING") is a state marker and renders as accent text.
    // Shared by the screen nav bar and the dialog footer so the two always match.
    inline void drawNavHints(PKSEFramebuffer& fb, int x, int w, int cy, const std::string& hint) {
        struct Seg { std::string btn, label; int glyphW, labelW; };

        // Whoever draws last owns the taps, so an open modal's footer replaces the nav bar behind it
        // rather than leaving the background live. Clearing here (not in drawNavBar) also means the
        // list can't grow across frames if a screen ever draws a footer without a nav bar.
        g_navHits.clear();

        auto trim = [](const std::string& s) {
            const size_t a = s.find_first_not_of(" \t");
            if (a == std::string::npos) return std::string();
            return s.substr(a, s.find_last_not_of(" \t") - a + 1);
        };

        std::vector<Seg> segs;
        for (size_t i = 0; i <= hint.size(); ) {
            const size_t bar = hint.find('|', i);
            const std::string tok =
                trim(hint.substr(i, bar == std::string::npos ? std::string::npos : bar - i));
            if (!tok.empty()) {
                Seg s{};
                size_t colon = tok.find(':');
                size_t colonLen = 1;
                if (colon == std::string::npos) {
                    colon = tok.find("：");
                    colonLen = std::string("：").size();
                }
                if (colon == std::string::npos) {
                    s.label = tok;
                } else {
                    s.btn   = trim(tok.substr(0, colon));
                    s.label = trim(tok.substr(colon + colonLen));
                }
                s.glyphW = s.btn.empty() ? 0 : buttonGlyphWidth(fb, s.btn);
                // A button we have no badge for still has to be readable: fall back to plain text
                // rather than silently dropping the button name and leaving a bare verb.
                if (s.glyphW == 0 && !s.btn.empty()) s.label = s.btn + ": " + s.label;
                int th; fb.measureText(s.label, s.labelW, th, TextStyle::Caption);
                segs.push_back(s);
            }
            if (bar == std::string::npos) break;
            i = bar + 1;
        }
        if (segs.empty()) return;

        constexpr int kGapGlyph = 8, kGapItemMax = 26, kGapItemMin = 10, kPadX = 20;
        int fixed = 0;
        for (const Seg& s : segs) fixed += s.glyphW + (s.glyphW ? kGapGlyph : 0) + s.labelW;

        const int n = static_cast<int>(segs.size());
        int gap = kGapItemMax;
        if (n > 1) gap = std::clamp((w - kPadX * 2 - fixed) / (n - 1), kGapItemMin, kGapItemMax);

        int cx = x + std::max(kPadX, (w - (fixed + gap * (n - 1))) / 2);
        for (const Seg& s : segs) {
            const int segX = cx;
            if (s.glyphW) { buttonGlyph(fb, cx, cy, s.btn, false); cx += s.glyphW + kGapGlyph; }
            int tw, th; fb.measureText(s.label, tw, th, TextStyle::Caption);
            fb.drawText(cx, cy - th / 2, s.label, s.glyphW ? Colors::Text : Colors::Accent,
                        TextStyle::Caption);
            cx += s.labelW;
            // The badge AND its label are one tap target -- aiming at a 24px circle is unreasonable,
            // and the label is the part that says what will happen. Height is TouchTargetMin rather
            // than the bar height (46), so the target stays fingertip-sized.
            const uint64_t button = s.glyphW ? navButtonFor(s.btn) : 0;
            if (button) g_navHits.push_back({segX, cy - TouchTargetMin / 2, cx - segX, TouchTargetMin, button});
            cx += gap;
        }
    }

    // --- Bars -------------------------------------------------------------------------------------

    // Top title bar: "PKSE" + a subtitle on a sheet that curves along its bottom edge.
    inline void drawTitleBar(PKSEFramebuffer& fb, const std::string& subtitle) {
        fb.drawSoftShadow(0, -40, fb.getWidth(), kHeaderH + 40, kChromeRadius);
        fb.drawFilledRoundedRect(0, -kChromeRadius, fb.getWidth(), kHeaderH + kChromeRadius,
                                 kChromeRadius, Colors::Panel);
        fb.drawText(20, 8, "PKSE", Colors::Accent, TextStyle::Title);
        int bw, bh; fb.measureText("PKSE", bw, bh, TextStyle::Title);
        // Short accent underline beneath the wordmark. This replaces the old full-width accent rule,
        // which can't work against a curved sheet -- a straight line would cut across the corners.
        fb.drawFilledRoundedRect(20, 52, bw, 3, 2, Colors::Accent);
        if (!subtitle.empty()) fb.drawText(20 + bw + 16, 24, subtitle, Colors::TextDim, TextStyle::Body);
    }

    // Bottom nav bar: a sheet that curves along its top edge, carrying the controller badges.
    inline void drawNavBar(PKSEFramebuffer& fb, const std::string& hint) {
        const int W = fb.getWidth(), barY = fb.getHeight() - kNavBarH;
        fb.drawSoftShadow(0, barY, W, kNavBarH + 40, kChromeRadius);
        fb.drawFilledRoundedRect(0, barY, W, kNavBarH + kChromeRadius, kChromeRadius, Colors::Panel);
        drawNavHints(fb, 0, W, barY + kNavBarH / 2, hint);
    }

    // A HOME-style selectable list tile: rounded (stadium), soft shadow, amber when
    // selected. `accent` marks a special/primary row (indigo-tinted when not selected, e.g. the
    // "新建备份" action); `enabled` false dims it (e.g. a "没有存档" placeholder).
    inline void drawHomeTile(PKSEFramebuffer& fb, int x, int y, int w, int h,
                             const std::string& label, bool selected, bool accent = false, bool enabled = true) {
        fb.drawSoftShadow(x, y, w, h, h / 2);
        const Color fill = selected ? Colors::Primary : Colors::PanelAlt;
        fb.drawPill(x, y, w, h, fill);
        // An accent (primary-action) row is a normal tile with accent text + a thin accent outline —
        // NOT a filled highlight, which would read as a false selection next to the real (amber) one.
        if (accent && !selected) fb.drawPillBorder(x, y, w, h, Colors::Accent, 2);
        const Color txt = !enabled ? Colors::TextDim
                        : selected  ? Colors::PrimaryText
                        : accent    ? Colors::Accent
                        :             Colors::Text;
        int lx = x + 28;
        if (selected) { fb.drawSymbol(x + 20, y + h / 2 - 12, "\xE2\x96\xB6", Colors::PrimaryText); lx = x + 48; }
        int lw, lh; fb.measureText(label, lw, lh, TextStyle::Body);
        fb.drawText(lx, y + (h - lh) / 2, label, txt, TextStyle::Body);
    }

    // A thin scrollbar thumb on a scrolling viewport's right edge, drawn ONLY when the content
    // overflows. All pixels: `x` is the thumb's left edge, [trackY, trackY + trackH] the viewport,
    // contentH the full content height, scroll the current offset. Item lists pass contentH =
    // totalItems * rowH and scroll = firstItem * rowH. One helper so every scrolling surface in the
    // app gets the same thumb the details editor uses.
    inline void drawScrollbar(PKSEFramebuffer& fb, int x, int trackY, int trackH, int contentH, int scroll) {
        if (contentH <= trackH || trackH <= 0) return;
        const int maxS = contentH - trackH;
        int s = scroll; if (s < 0) s = 0; if (s > maxS) s = maxS;
        const int thumbH = std::max(24, trackH * trackH / contentH);
        const int thumbY = trackY + (trackH - thumbH) * s / maxS;
        fb.drawFilledRoundedRect(x, thumbY, 3, thumbH, 2, Colors::Border);
    }
}

#endif
