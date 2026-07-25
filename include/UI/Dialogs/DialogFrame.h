#ifndef UI_DIALOGS_DIALOG_FRAME_H
#define UI_DIALOGS_DIALOG_FRAME_H

#include <string>

#include "UI/PKSEFramebuffer.h"
#include "UI/Common.h"
#include "UI/ScreenChrome.h"

namespace UI {
namespace Dialogs {
    // Radius shared by every dialog surface. Matches PickerDialog, which was already rounded, so
    // all modal cards now read as one family instead of some square and some not.
    constexpr int kDialogRadius = 16;

    // Draw a centered modal dialog frame: a dim scrim over the screen, a lifted card, a heading
    // title, and an accent underline. Returns the Y at which content should start.
    inline int drawDialogFrame(PKSEFramebuffer& fb, int x, int y, int w, int h,
                               const std::string& title, Color titleColor) {
        fb.drawFilledRect(0, 0, fb.getWidth(), fb.getHeight(), Color(0, 0, 0, 150));  // scrim
        fb.drawSoftShadow(x, y, w, h, kDialogRadius);
        fb.drawFilledRoundedRect(x, y, w, h, kDialogRadius, Colors::Panel);
        fb.drawRoundedRect(x, y, w, h, kDialogRadius, Colors::Border, 1);
        fb.drawText(x + 24, y + 16, title, titleColor, TextStyle::Heading);
        // Inset so the rule stops short of the rounded corners instead of running into the curve.
        fb.drawFilledRect(x + kDialogRadius, y + 56, w - kDialogRadius * 2, 2, Colors::Accent);
        return y + 78;
    }

    // Draw the control-hint strip at the bottom of a dialog, using the same controller badges as
    // the screen nav bar. Takes the same "Btn: Label  |  Btn: Label" hint format.
    inline void drawDialogFooter(PKSEFramebuffer& fb, int x, int y, int w, int h, const std::string& hint) {
        constexpr int fh = 42;
        const int fy = y + h - fh;
        // Round the bottom corners to follow the card, then square off the top against the content.
        fb.drawFilledRoundedRect(x, fy, w, fh, kDialogRadius, Colors::PanelAlt);
        fb.drawFilledRect(x, fy, w, fh / 2, Colors::PanelAlt);
        drawNavHints(fb, x, w, fy + fh / 2, hint);
    }
}
}

#endif
