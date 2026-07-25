#include "UI/TouchInput.h"

namespace UI {
    void TouchInput::update() {
        prevDown = curDown;

        HidTouchScreenState st{};
        if (hidGetTouchScreenStates(&st, 1) > 0 && st.count > 0) {
            curX = static_cast<int>(st.touches[0].x);
            curY = static_cast<int>(st.touches[0].y);
            // Record the start position on the press edge (curDown still holds the previous frame here).
            if (!curDown) { begX = curX; begY = curY; }
            curDown = true;
        } else {
            curDown = false;
        }
    }

    bool TouchInput::dragged() const {
        const int dx = curX - begX, dy = curY - begY;
        return (dx * dx + dy * dy) > (20 * 20);  // ~20px movement threshold
    }
}
