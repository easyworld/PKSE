/**
 * Self-contained, no external dependencies. Added for Brilliant Diamond /
 * Shining Pearl (BDSP) save writing: BDSP validates the save with a whole-file
 * MD5 hash and silently rejects a save whose stored digest does not match, so
 * this must be byte-exact.
 *
 * Known-answer vectors (RFC 1321 Appendix A.5):
 *   md5("")    = d41d8cd98f00b204e9800998ecf8427e
 *   md5("abc") = 900150983cd24fb0d6963f7d28e17f72
 */

#ifndef UTILS_MD5_H
#define UTILS_MD5_H

#include <cstdint>
#include <cstddef>

namespace Utils {
    void md5(const uint8_t* data, size_t length, uint8_t out[16]);
}

#endif
