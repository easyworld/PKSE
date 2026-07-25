#ifndef UI_SCREEN_H
#define UI_SCREEN_H

#include <switch.h>

namespace UI {
    class PKSEFramebuffer;
    class TouchInput;

    class UIScreen {
    public:
        virtual ~UIScreen() = default;
        virtual void update(const PadState& pad, const TouchInput& touch) = 0;
        virtual void draw(PKSEFramebuffer& fb) = 0;
        virtual bool shouldExit() const { return false; }
    };
}

#endif
