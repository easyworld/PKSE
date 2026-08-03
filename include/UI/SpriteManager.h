#ifndef UI_SPRITE_MANAGER_H
#define UI_SPRITE_MANAGER_H

#include <cstddef>
#include <cstdint>
#include <list>
#include <map>
#include <string>

namespace UI {
    struct Sprite {
        unsigned char* data;  // Image pixel data (RGBA)
        int width;
        int height;
        int channels;         // Number of color channels (1-4)

        Sprite() : data(nullptr), width(0), height(0), channels(0) {}
        ~Sprite();
    };

    // Sprite manager for loading and caching Pokemon sprites
    class SpriteManager {
    public:
        static void init();

        // Cleanup and free all cached sprites
        static void cleanup();

        // Get sprite for a Pokemon (normal size)
        // Returns nullptr if sprite not found
        static Sprite* getSprite(uint16_t speciesId, bool isShiny = false);

        // Get sprite for a Pokemon with specific form
        // Returns nullptr if sprite not found
        static Sprite* getSprite(uint16_t speciesId, uint8_t formId, bool isShiny);

        // Get small icon sprite for box display (40x40)
        // Returns nullptr if sprite not found
        static Sprite* getIconSprite(uint16_t speciesId, bool isShiny = false);

        // Get icon sprite for a Pokemon with specific form
        static Sprite* getIconSprite(uint16_t speciesId, uint8_t formId, bool isShiny);

        static bool spriteExists(uint16_t speciesId, bool isShiny = false);

        // Get type sprite by type ID (0-17)
        // Returns nullptr if sprite not found
        static Sprite* getTypeSprite(uint8_t typeId);

        static bool typeSpriteExists(uint8_t typeId);

        // Called just before a sprite's pixel buffer is freed. The renderer caches the texture it
        // built from that buffer under the buffer's ADDRESS, so it has to drop it in step: a later
        // sprite allocated at the same address would otherwise be drawn with the evicted one's
        // image. Registered by the renderer; cleared when the renderer goes away.
        using EvictFn = void (*)(const unsigned char* data);
        static void setEvictCallback(EvictFn fn);

        // Bytes of decoded pixel data currently held. Exposed for diagnostics/logging.
        static size_t cachedBytes();

    private:
        // Load a sprite from ROMFS
        static Sprite* loadSprite(const std::string& path);

        // Pokemon sprites are the only cache that grows with use -- one 256px HD sprite decodes to
        // 256 KB, and a full save has hundreds -- so it is capped and evicts least-recently-used.
        // The 18 type icons are naturally bounded and stay a simple map.
        struct SpriteEntry {
            Sprite* sprite;                      // may be null: a failed load is cached too
            std::list<uint32_t>::iterator lru;   // position in spriteLru
        };

        // Sprite cache: key = species ID | (isShiny << 16) | (isIcon << 17) | (form << 18)
        static std::map<uint32_t, SpriteEntry> spriteCache;
        static std::list<uint32_t> spriteLru;    // front = most recently used
        static size_t spriteBytes;
        static EvictFn evictCallback;

        // Drop least-recently-used sprites until the cache is back inside its budget.
        static void trimToBudget();
        static void releaseSprite(Sprite* sprite);

        // Type sprite cache: key = type ID
        static std::map<uint8_t, Sprite*> typeSpriteCache;

        static bool initialized;

        // Generate cache key for Pokemon sprites (supports form ID up to 255)
        static uint32_t makeCacheKey(uint16_t speciesId, uint8_t formId, bool isShiny, bool isIcon) {
            // Format: bits 0-15: speciesId, bit 16: isShiny, bit 17: isIcon, bits 18-25: formId
            return speciesId | (isShiny ? (1 << 16) : 0) | (isIcon ? (1 << 17) : 0) | (static_cast<uint32_t>(formId) << 18);
        }

        // Legacy cache key for base forms
        static uint32_t makeCacheKey(uint16_t speciesId, bool isShiny, bool isIcon) {
            return makeCacheKey(speciesId, 0, isShiny, isIcon);
        }
    };
}

#endif
