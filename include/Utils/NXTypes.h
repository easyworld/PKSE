#ifndef UTILS_NXTYPES_H
#define UTILS_NXTYPES_H

/**
 * libnx's scalar typedefs, available off-console.
 *
 * The save layer (Trainer / Pokemon / Encryption / Names) is pure byte manipulation and has no
 * business needing a Switch SDK — but a handful of its headers pulled in <switch.h> purely for
 * `u8`..`s64`, which was enough to make the whole layer un-compilable anywhere else. That blocked
 * the read -> write -> re-read validation harness (#50), which is the only automated check this
 * project has against save corruption.
 *
 * On the Switch this is still <switch.h> verbatim, so nothing about the real build changes. Off it,
 * these few typedefs stand in and the save layer compiles for the host.
 *
 * Anything that genuinely needs the SDK (title lookups, filesystem, accounts) stays behind
 * `#ifdef __SWITCH__` in its own file rather than being shimmed here — a stub that silently returns
 * nothing would be worse than a link error.
 */
#ifdef __SWITCH__
#include <switch.h>
#else
#include <cstdint>

using u8  = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using s8  = std::int8_t;
using s16 = std::int16_t;
using s32 = std::int32_t;
using s64 = std::int64_t;

/// Layout-compatible stand-in for libnx's account id. Host builds only pass it around.
struct AccountUid { u64 uid[2]; };
#endif

#endif
