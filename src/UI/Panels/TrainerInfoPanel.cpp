#include <string>

#include "UI/Panels/TrainerInfoPanel.h"
#include "UI/PKSEFramebuffer.h"
#include "UI/Common.h"
#include "Trainer/Trainer.h"

using namespace Trainer;

namespace UI {
namespace Panels {
    void drawTrainerInfo(PKSEFramebuffer& fb, Trainer::Trainer& trainer, int x, int y, int width, int height) {
        fb.drawFilledRect(x, y, width, height, Colors::Panel);
        fb.drawRect(x, y, width, height, Colors::Border);

        fb.drawText(x + 20, y + 20, "训练家信息", Colors::Text);
        fb.drawFilledRect(x + 20, y + 45, width - 40, 2, Colors::Border);

        int lineY = y + 60;
        int lineHeight = 18;

        std::string nameText = "姓名: " + trainer.trainerName;
        fb.drawText(x + 20, lineY, nameText, Colors::Text);
        lineY += lineHeight;

        std::string idText = "ID: " + std::to_string(trainer.TID16) + "/" + std::to_string(trainer.SID16);
        fb.drawText(x + 20, lineY, idText, Colors::Text);
        lineY += lineHeight;

        std::string tidText = "训练家ID: " + std::to_string(trainer.TID);
        fb.drawText(x + 20, lineY, tidText, Colors::Text);
        lineY += lineHeight;

        std::string sidText = "秘密ID: " + std::to_string(trainer.SID);
        fb.drawText(x + 20, lineY, sidText, Colors::Text);
        lineY += lineHeight;

        std::string moneyText = "金钱: " + std::to_string(trainer.money);
        fb.drawText(x + 20, lineY, moneyText, Colors::Text);
    }
}
}
