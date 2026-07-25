#ifndef UI_TOUCH_INPUT_H
#define UI_TOUCH_INPUT_H

#include <switch.h>

namespace UI {
    /**
     * Reads the Switch touchscreen once per frame and exposes an edge-triggered view of it
     * (tap / hold / release / drag), mirroring the way padGetButtonsDown gives edge-triggered
     * buttons. Coordinates are in framebuffer pixels: the UI renders at 1280x720, which matches the
     * touch panel resolution, so touch points map 1:1 to pixels with no scaling.
     */
    class TouchInput {
    public:
        /// Poll the touchscreen; call once per frame after hidInitializeTouchScreen().
        void update();

        bool isDown() const { return curDown; }                     // a finger is currently on the screen
        bool justPressed() const { return curDown && !prevDown; }   // touch began this frame
        bool justReleased() const { return !curDown && prevDown; }  // touch ended this frame (use start pos)
        int x() const { return curX; }                              // current (or last) touch position
        int y() const { return curY; }
        int startX() const { return begX; }                         // where the current/last touch began
        int startY() const { return begY; }
        bool dragged() const;                                       // moved past a small threshold since press

    private:
        bool curDown = false, prevDown = false;
        int curX = 0, curY = 0;   // latest touch position
        int begX = 0, begY = 0;   // position where the current touch started (recorded on the press edge)
    };
}

#endif
