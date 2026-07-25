#ifndef UI_SYSTEM_ICONS_H
#define UI_SYSTEM_ICONS_H

#include <switch.h>

namespace UI {
    // A decoded RGBA image (Switch user avatar or game title icon). The pixel buffer is owned by
    // the session-long cache below and is NEVER freed until cleanup(), so its pointer stays stable
    // for the whole app run — PKSEFramebuffer caches GL images keyed by buffer pointer, so a stable
    // pointer is required to avoid stale-image collisions (same as SpriteManager's sprites).
    struct IconImage {
        unsigned char* data = nullptr;   // RGBA8, cache-owned
        int width  = 0;
        int height = 0;
        bool valid() const { return data != nullptr && width > 0 && height > 0; }
    };

    // Lazily decode + cache system icons for the app session.
    namespace SystemIcons {
        // The account's profile picture (JPEG decoded to RGBA). Cached by user id.
        const IconImage& userIcon(AccountUid uid);
        // A game's icon from its control data (JPEG decoded to RGBA). Cached by title id.
        const IconImage& titleIcon(u64 titleId);
        // Free every cached buffer. Call once at shutdown.
        void cleanup();
    }
}

#endif
