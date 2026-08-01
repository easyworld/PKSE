#include <cstdint>
#include <string>

#include "UI/Dialogs/PickerDialog.h"
#include "UI/TrainerViewScreen.h"
#include "UI/Common.h"
#include "UI/ScreenChrome.h"     // drawScrollbar
#include "UI/PKSEFramebuffer.h"
#include "Trainer/Trainer.h"     // getNatureName / getAbilityName
#include "Names/MoveNames.h"     // getMoveName / getMoveCount
#include "Names/ItemNames.h"     // getItemNameG3 (Gen 3 has its own item id space)
#include "Enums/Ball.h"          // getBallName
#include "Enums/LanguageID.h"    // getLanguageName
#include "Enums/GameVersion.h"   // getGameVersionName (Origin picker)
#include "Names/LocationNames.h" // getMetLocationName (Met Location picker)
#include "Names/FormNames.h"     // getFormName (Form picker)

using namespace Trainer;

namespace UI {
namespace Dialogs {

    static const char* const GENDER_LABELS[3] = { "雄性", "雌性", "无性别" };

    int pickerOptionCount(PickerKind kind) {
        switch (kind) {
            case PickerKind::Nature: return 25;
            case PickerKind::Gender: return 3;
            case PickerKind::Move:   return static_cast<int>(Names::getMoveCount());
            case PickerKind::Item:   return static_cast<int>(getItemCount());
            case PickerKind::ItemG3: return static_cast<int>(Names::getItemCountG3());
            case PickerKind::Level:  return 100;
            case PickerKind::Friendship: return 256;
            case PickerKind::Ball:       return 38;   // 0=(none) .. 37=Origin Ball
            case PickerKind::Ability:    return 311;  // AbilityNames covers ids 0-310 (through Gen 9)
            case PickerKind::Language:   return 11;   // 0-10 (6 is unused, shown as "-")
            case PickerKind::Origin:     return 53;   // version ids 0-52 (getGameVersionName)
            case PickerKind::MetLevel:   return 100;  // met level 1-100
            case PickerKind::MetLocation: return 0;   // ids supplied via pickerOrder (caller sets count)
            case PickerKind::Form:        return 0;   // form count is species-dependent (caller sets count)
            case PickerKind::StatNature:  return 25;  // same 25 natures as the real nature
            case PickerKind::Species:    return 1026; // ids 0-1025 (0 = None)
            // Pouch lists are supplied through pickerOrder, so the caller sets pickerCount.
            case PickerKind::PouchItem:
            case PickerKind::PouchItemG3: return 0;
        }
        return 0;
    }

    const char* pickerOptionLabel(PickerKind kind, int index) {
        switch (kind) {
            case PickerKind::Nature: return getNatureName(static_cast<uint8_t>(index));
            case PickerKind::Gender: return (index >= 0 && index < 3) ? GENDER_LABELS[index] : "?";
            case PickerKind::Move:   return Names::getMoveName(static_cast<uint16_t>(index));
            case PickerKind::Item:   return getItemName(static_cast<uint16_t>(index));
            case PickerKind::ItemG3: return Names::getItemNameG3(static_cast<uint16_t>(index));
            case PickerKind::Level:  { static std::string s; s = std::to_string(index + 1); return s.c_str(); }
            case PickerKind::Friendship: { static std::string s; s = std::to_string(index); return s.c_str(); }
            case PickerKind::Ball:       return Enums::getBallName(static_cast<uint8_t>(index));
            case PickerKind::Ability:    return getAbilityName(static_cast<uint16_t>(index));
            case PickerKind::Language:   return Enums::getLanguageName(static_cast<uint8_t>(index));
            case PickerKind::Origin:     { static std::string s; s = Enums::getGameVersionName(static_cast<Enums::GameVersion>(index)); return s.c_str(); }
            case PickerKind::MetLevel:   { static std::string s; s = std::to_string(index + 1); return s.c_str(); }
            case PickerKind::MetLocation: return "";  // labelled in drawPickerDialog (needs the origin version)
            case PickerKind::Form:        return "";  // labelled in drawPickerDialog (needs the species)
            case PickerKind::StatNature:  return getNatureName(static_cast<uint8_t>(index));
            case PickerKind::Species:    return getSpeciesName(static_cast<uint16_t>(index));
            case PickerKind::PouchItem:   return getItemName(static_cast<uint16_t>(index));
            case PickerKind::PouchItemG3: return Names::getItemNameG3(static_cast<uint16_t>(index));
        }
        return "?";
    }

    const char* pickerTitle(PickerKind kind) {
        switch (kind) {
            case PickerKind::Nature: return "选择性格";
            case PickerKind::Gender: return "选择性别";
            case PickerKind::Move:   return "选择招式";
            case PickerKind::Item:
            case PickerKind::ItemG3: return "选择携带道具";
            case PickerKind::Level:  return "选择等级";
            case PickerKind::Friendship: return "选择亲密度";
            case PickerKind::Ball:       return "选择精灵球";
            case PickerKind::Ability:    return "选择特性";
            case PickerKind::Language:   return "选择语言";
            case PickerKind::Origin:     return "选择初训家游戏";
            case PickerKind::MetLevel:   return "选择相遇等级";
            case PickerKind::MetLocation: return "选择相遇地点";
            case PickerKind::Form:        return "选择形态";
            case PickerKind::StatNature:  return "选择能力性格（薄荷）";
            case PickerKind::Species:    return "选择种类";
            case PickerKind::PouchItem:
            case PickerKind::PouchItemG3: return "向口袋添加道具";
        }
        return "选择";
    }

    void drawPickerDialog(TrainerViewScreen& screen, PKSEFramebuffer& fb) {
        const int W = fb.getWidth(), H = fb.getHeight();
        const PickerKind kind = screen.pickerKind;
        const int count = screen.pickerCount > 0 ? screen.pickerCount : 1;
        int sel = screen.pickerSel;
        if (sel < 0) sel = 0;
        if (sel >= count) sel = count - 1;

        // Dim behind + centered panel.
        fb.drawFilledRect(0, 0, W, H, Color(0, 0, 0, 150));
        const int pw = 560, ph = H - 120;
        const int px = (W - pw) / 2, py = 60;
        fb.drawFilledRoundedRect(px, py, pw, ph, 16, Colors::Panel);
        fb.drawRoundedRect(px, py, pw, ph, 16, Colors::Accent, 2);

        // Title + position caption. A pouch picker opened to change an existing item's type says so
        // rather than "向口袋添加道具".
        const char* title = (screen.itemPickerReplace &&
                             (kind == PickerKind::PouchItem || kind == PickerKind::PouchItemG3))
                          ? "更换道具" : pickerTitle(kind);
        fb.drawText(px + 20, py + 16, title, Colors::Text, TextStyle::Heading);
        {
            std::string pos = std::to_string(sel + 1) + " / " + std::to_string(count);
            int pwi, phi; fb.measureText(pos, pwi, phi, TextStyle::Caption);
            fb.drawText(px + pw - 20 - pwi, py + 22, pos, Colors::TextDim, TextStyle::Caption);
        }
        fb.drawHDivider(px + 20, py + 52, pw - 40);

        // Scrollable list window centered on the selection.
        const int rowH = 40;
        const int listTop = py + 64, listBottom = py + ph - 48;
        int visible = (listBottom - listTop) / rowH;
        if (visible < 1) visible = 1;
        int first = sel - visible / 2;
        if (first > count - visible) first = count - visible;
        if (first < 0) first = 0;

        screen.touchButtons.clear();
        // the Ability picker reorders its options (legal abilities first) via screen.pickerOrder,
        // and the legal prefix renders green. Every other kind stays identity-indexed (row == value).
        const bool reorder = (kind == PickerKind::Ability || kind == PickerKind::Species
                           || kind == PickerKind::Move    || kind == PickerKind::PouchItem
                           || kind == PickerKind::PouchItemG3 || kind == PickerKind::MetLocation
                           || kind == PickerKind::Ball)
                           && !screen.pickerOrder.empty();
        for (int i = 0; i < visible && (first + i) < count; ++i) {
            const int idx = first + i;
            const int val = (reorder && idx < static_cast<int>(screen.pickerOrder.size())) ? screen.pickerOrder[idx] : idx;
            const int ry = listTop + i * rowH;
            const bool s = (idx == sel);
            if (s) {
                fb.drawFilledRoundedRect(px + 12, ry, pw - 24, rowH - 4, 8, Colors::Selected);
                fb.drawRoundedRect(px + 12, ry, pw - 24, rowH - 4, 8, Colors::Accent, 2);
            }
            const bool legal = reorder && idx < screen.pickerLegalCount;
            const Color col = legal ? Color(120, 210, 130) : (s ? Colors::Text : Colors::TextDim);
            // Met Location / Form resolve their names through mon-specific context (origin version /
            // species) that the free pickerOptionLabel() can't see; everything else is context-free.
            const char* label;
            static std::string formLbl;
            if (kind == PickerKind::MetLocation) {
                label = Names::getMetLocationName(screen.pickerMetVersion, static_cast<uint16_t>(val));
            } else if (kind == PickerKind::Form) {
                const char* fn = Names::getFormName(screen.pickerFormSpecies, static_cast<uint8_t>(val));
                formLbl = (fn[0] != '\0') ? std::string(fn)
                                          : (val == 0 ? std::string("普通形态") : ("形态 " + std::to_string(val)));
                label = formLbl.c_str();
            } else {
                label = pickerOptionLabel(kind, val);
            }
            fb.drawText(px + 28, ry + 8, label, col);
            screen.touchButtons.push_back({ idx, px + 12, ry, pw - 24, rowH - 4 });  // id = option row
        }

        // Scrollbar on the panel's right edge (same thumb as everywhere else) when the list overflows.
        drawScrollbar(fb, px + pw - 14, listTop, visible * rowH, count * rowH, first * rowH);

        fb.drawText(px + 20, py + ph - 34, "A：选择    B：取消    L/R：翻页", Colors::TextDim, TextStyle::Caption);
    }
}
}
