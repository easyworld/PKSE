#include "UI/SystemIcons.h"

#include <Libs/stb_image.h>   // implementation lives in SpriteManager.cpp; we only need the decls

#include <cstdlib>
#include <map>
#include <vector>

#include "Utils/Logger.h"

using namespace Utils;

namespace UI {
    namespace {
        // AccountUid is a 128-bit value (u64[2]); make it usable as a map key.
        struct UidKey {
            u64 a, b;
            bool operator<(const UidKey& o) const { return a != o.a ? a < o.a : b < o.b; }
        };

        std::map<UidKey, IconImage> s_userCache;
        std::map<u64, IconImage>    s_titleCache;

        // Decode a JPEG blob to a session-owned RGBA IconImage (invalid on failure).
        IconImage decodeToRGBA(const unsigned char* jpg, int len) {
            IconImage img;
            int w = 0, h = 0, comp = 0;
            unsigned char* rgba = stbi_load_from_memory(jpg, len, &w, &h, &comp, 4);
            if (!rgba) return img;
            img.data = rgba;
            img.width = w;
            img.height = h;
            return img;
        }
    }

    const IconImage& SystemIcons::userIcon(AccountUid uid) {
        UidKey key{uid.uid[0], uid.uid[1]};
        auto it = s_userCache.find(key);
        if (it != s_userCache.end()) return it->second;

        IconImage img;
        AccountProfile profile;
        if (R_SUCCEEDED(accountGetProfile(&profile, uid))) {
            u32 imgSize = 0;
            if (R_SUCCEEDED(accountProfileGetImageSize(&profile, &imgSize)) && imgSize > 0) {
                std::vector<unsigned char> jpg(imgSize);
                u32 realSize = 0;
                if (R_SUCCEEDED(accountProfileLoadImage(&profile, jpg.data(), imgSize, &realSize)) && realSize > 0) {
                    img = decodeToRGBA(jpg.data(), static_cast<int>(realSize));
                }
            }
            accountProfileClose(&profile);
        }
        if (!img.valid()) logErrorToFile("SystemIcons: failed to load user avatar");
        return s_userCache.emplace(key, img).first->second;
    }

    const IconImage& SystemIcons::titleIcon(u64 titleId) {
        auto it = s_titleCache.find(titleId);
        if (it != s_titleCache.end()) return it->second;

        IconImage img;
        // NsApplicationControlData is large (~0x24000); heap-allocate it.
        NsApplicationControlData* ctl = static_cast<NsApplicationControlData*>(malloc(sizeof(NsApplicationControlData)));
        if (ctl) {
            u64 outSize = 0;
            Result rc = nsGetApplicationControlData(NsApplicationControlSource_Storage, titleId,
                                                    ctl, sizeof(NsApplicationControlData), &outSize);
            if (R_SUCCEEDED(rc) && outSize > sizeof(ctl->nacp)) {
                int iconLen = static_cast<int>(outSize - sizeof(ctl->nacp));
                img = decodeToRGBA(ctl->icon, iconLen);
            }
            free(ctl);
        }
        if (!img.valid()) logErrorToFile("SystemIcons: failed to load title icon");
        return s_titleCache.emplace(titleId, img).first->second;
    }

    void SystemIcons::cleanup() {
        for (auto& kv : s_userCache)  if (kv.second.data) stbi_image_free(kv.second.data);
        for (auto& kv : s_titleCache) if (kv.second.data) stbi_image_free(kv.second.data);
        s_userCache.clear();
        s_titleCache.clear();
    }
}
