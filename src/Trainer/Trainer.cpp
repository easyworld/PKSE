#include <cstdint>
#include <cstddef>
#include <vector>
#include <cstring>
#include <span>
#include <algorithm>

#include "Utils/Logger.h"
#include "Trainer/Trainer.h"
#include "Encryption/Encryption.h"

// Forward declarations for Names namespace functions
namespace Names
{
    extern const char *getItemName(uint16_t itemId);
    extern const char *getNatureName(uint8_t natureId);
    extern const char *getSpeciesName(uint16_t speciesId);
    extern const char *getAbilityName(uint16_t abilityId);
    extern size_t getItemCount();
}

// Wrapper functions in Trainer namespace that forward to Names namespace
namespace Trainer
{
    const char *getItemName(uint16_t itemId)
    {
        return Names::getItemName(itemId);
    }

    const char *getNatureName(uint8_t natureId)
    {
        return Names::getNatureName(natureId);
    }

    const char *getSpeciesName(uint16_t speciesId)
    {
        return Names::getSpeciesName(speciesId);
    }

    const char *getAbilityName(uint16_t abilityId)
    {
        return Names::getAbilityName(abilityId);
    }

    size_t getItemCount()
    {
        return Names::getItemCount();
    }
}