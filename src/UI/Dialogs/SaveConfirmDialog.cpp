#include "UI/Dialogs/SaveConfirmDialog.h"
#include "UI/TrainerViewScreen.h"
#include "UI/Common.h"
#include "UI/PKSEFramebuffer.h"

namespace UI {
namespace Dialogs {
    void drawSaveConfirmDialog(TrainerViewScreen& screen, PKSEFramebuffer& fb) {
        // Draw dialog
        int dialogWidth = 500;
        int dialogHeight = 180;
        int dialogX = (fb.getWidth() - dialogWidth) / 2;
        int dialogY = (fb.getHeight() - dialogHeight) / 2;

        // Draw dialog background
        fb.drawFilledRect(dialogX, dialogY, dialogWidth, dialogHeight, Colors::Panel);
        fb.drawRect(dialogX, dialogY, dialogWidth, dialogHeight, Colors::Border);

        // Draw title and message
        if (screen.exitingWithUnsavedChanges) {
            fb.drawText(dialogX + 20, dialogY + 20, "未保存的更改", Colors::Yellow);
            fb.drawFilledRect(dialogX + 20, dialogY + 45, dialogWidth - 40, 2, Colors::Border);
            fb.drawText(dialogX + 20, dialogY + 70, "您有未保存的更改。", Colors::Text);
            fb.drawText(dialogX + 20, dialogY + 95, "如果继续，更改将会丢失。", Colors::Text);
            fb.drawText(dialogX + 20, dialogY + 135, "A: 放弃并退出  |  B: 取消", Colors::TextDim);
        } else {
            // Regular save dialog (triggered by X button)
            fb.drawText(dialogX + 20, dialogY + 20, "保存更改", Colors::Text);
            fb.drawFilledRect(dialogX + 20, dialogY + 45, dialogWidth - 40, 2, Colors::Border);
            if (screen.hasUnsavedChanges) {
                fb.drawText(dialogX + 20, dialogY + 70, "您有未保存的更改。", Colors::Yellow);
                fb.drawText(dialogX + 20, dialogY + 95, "您要保存它们吗？", Colors::Text);
            } else {
                fb.drawText(dialogX + 20, dialogY + 70, "没有进行任何更改。", Colors::TextDim);
                fb.drawText(dialogX + 20, dialogY + 95, "仍然保存吗？", Colors::Text);
            }
            fb.drawText(dialogX + 20, dialogY + 135, "A: 保存  |  B: 取消", Colors::TextDim);
        }
    }
}
}