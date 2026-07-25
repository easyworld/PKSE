#ifndef UI_DIALOGS_EDIT_CONTROLS_H
#define UI_DIALOGS_EDIT_CONTROLS_H

#include "UI/PKSEFramebuffer.h"
#include "UI/Common.h"
#include "UI/ScreenChrome.h"        // buttonGlyph — the same controller badges the bottom guides use
#include "UI/TrainerViewScreen.h"   // TrainerViewScreen::touchButtons

namespace UI {
namespace Dialogs {
    // Shared controls for the value editors (Edit Stat, Edit Amount). Kept identical between the two
    // so they feel the same: a row of controller-badged +/- step buttons, plus A/B-labelled
    // Cancel/Confirm. Touch ids: 34=-100(ZL) 30=-10(L) 31=-1(<) 32=+1(>) 33=+10(R) 35=+100(ZR),
    // 0=Cancel(B) 1=Confirm(A) -- the dialogs' input handlers map these back to the same buttons.
    struct EditStep { const char* glyph; const char* delta; int id; };

    inline const EditStep kEditSteps[6] = {
        {"ZL", "-100", 34}, {"L", "-10", 30}, {"左", "-1", 31},
        {"右", "+1", 32}, {"R", "+10", 33}, {"ZR", "+100", 35},
    };

    inline void drawEditStepButton(TrainerViewScreen& screen, PKSEFramebuffer& fb,
                                   int bx, int by, int bw, int bh, const EditStep& s) {
        fb.drawFilledRoundedRect(bx, by, bw, bh, 8, Colors::PanelAlt);
        fb.drawRoundedRect(bx, by, bw, bh, 8, Colors::Border, 1);
        const int gw = buttonGlyphWidth(fb, s.glyph);
        buttonGlyph(fb, bx + (bw - gw) / 2, by + 18, s.glyph, false);   // badge above
        int tw, th; fb.measureText(s.delta, tw, th, TextStyle::Caption);
        fb.drawText(bx + (bw - tw) / 2, by + bh - th - 6, s.delta, Colors::Text, TextStyle::Caption);  // delta below
        screen.touchButtons.push_back({s.id, bx, by, bw, bh});
    }

    // Lay the six step buttons across [x, x+w], centred, at rowY (height rowH).
    inline void drawEditStepRow(TrainerViewScreen& screen, PKSEFramebuffer& fb, int x, int w, int rowY, int rowH) {
        constexpr int n = 6, gap = 8;
        const int bw = (w - 32 - (n - 1) * gap) / n;
        int bx = x + (w - (n * bw + (n - 1) * gap)) / 2;
        for (const EditStep& s : kEditSteps) { drawEditStepButton(screen, fb, bx, rowY, bw, rowH, s); bx += bw + gap; }
    }

    // A Cancel/Confirm button labelled with its face button. Deliberately NOT accent-filled: colouring
    // "确认" as if selected implied a cursor that isn't there -- both are always available.
    inline void drawEditChoiceButton(TrainerViewScreen& screen, PKSEFramebuffer& fb, int bx, int by,
                                     int bw, int bh, const char* glyph, const char* label, int id,
                                     Color fill = Colors::PanelAlt, Color textColor = Colors::Text) {
        drawGlyphButton(fb, bx, by, bw, bh, glyph, label, fill, textColor);
        screen.touchButtons.push_back({id, bx, by, bw, bh});
    }
}
}

#endif
