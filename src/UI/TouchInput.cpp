#include "UI/TouchInput.h"

namespace UI
{
    void TouchInput::update()
    {
        prevDown = curDown;

        HidTouchScreenState touchState{};
        if (hidGetTouchScreenStates(&touchState, 1) > 0 && touchState.count > 0)
        {
            curX = static_cast<int>(touchState.touches[0].x);
            curY = static_cast<int>(touchState.touches[0].y);
            // Record the start position on the press edge (curDown still holds the previous frame here).
            if (!curDown)
            {
                begX = curX;
                begY = curY;
            }
            curDown = true;
        }
        else
        {
            curDown = false;
        }
    }

    bool TouchInput::dragged() const
    {
        const int deltaX = curX - begX, deltaY = curY - begY;
        return (deltaX * deltaX + deltaY * deltaY) > (20 * 20); // ~20px movement threshold
    }
}
