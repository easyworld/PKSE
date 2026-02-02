#include <string>

#include "UI/Panels/ModeSelectorPanel.h"
#include "UI/PKSEFramebuffer.h"
#include "UI/Common.h"

namespace UI {
namespace Panels {
    void drawModeSelector(PKSEFramebuffer& fb, int selectedMode, int x, int y, int width, int height) {
        fb.drawFilledRect(x, y, width, height, Colors::Panel);
        fb.drawRect(x, y, width, height, Colors::Border);

        fb.drawText(x + 20, y + 20, "查看模式", Colors::Text);
        fb.drawFilledRect(x + 20, y + 45, width - 40, 2, Colors::Border);

        int lineY = y + 60;
        int lineHeight = 28;

        // Draw mode options
        const char* modes[] = {"队伍宝可梦", "盒子宝可梦", "道具"};

        for (int i = 0; i < 3; i++) {
            if (selectedMode == i) {
                // Highlight selected mode
                fb.drawFilledRect(x + 10, lineY, width - 20, lineHeight - 3, Colors::Selected);
            }

            std::string displayText = std::string("> ") + modes[i];
            fb.drawText(x + 30, lineY + 6, displayText, Colors::Text);
            lineY += lineHeight;
        }
    }
}
}
