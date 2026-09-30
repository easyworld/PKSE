# memecrypto (vendored)

**Third-party code. Do not edit.** Re-sync from upstream instead:
<https://github.com/FlagBrew/memecrypto> (SciresM). This is the copy PKSM ships as
`core/memecrypto/`, unmodified.

## Why PKSE needs it

Sun/Moon and Ultra Sun/Ultra Moon sign their save's checksum table: a SHA-256 of that table is
RSA-signed and stored inside block 36 (`TurtleSalmonSave`) at `blockOffset + 0x100`. **Any** edit
changes the checksum table, so a save written without re-signing is rejected by the game. No other
generation PKSE supports needs this — see `Trainer67::signGen7`.

## Contents and licensing

| File              | Origin                                                            | Licence                   |
| ----------------- | ----------------------------------------------------------------- | ------------------------- |
| `memecrypto.c/.h` | SciresM                                                           | **GPLv3** (see `LICENSE`) |
| `aes.c/.h`        | [tiny-AES128-C](https://github.com/kokke/tiny-AES128-C)           | public domain             |
| `sha1.c/.h`       | [libsha1](https://github.com/dottedmag/libsha1), Dr Brian Gladman | BSD-style                 |
| `rsa.c/.h`        | SciresM, on mini-gmp                                              | GPLv3                     |
| `mini-gmp.c/.h`   | GNU GMP                                                           | LGPLv3 / GPLv2 dual       |

PKSE is **AGPLv3**, which GPLv3 and LGPLv3 are compatible with, so this combines cleanly. `LICENSE`
here is the upstream GPLv3 text and must ship with the repository.

## Build

`memecrypto` is listed in the Makefile's `SOURCES`, so its `.c` files are compiled with the project.
It is plain C99 with no platform dependencies beyond `string.h`/`stdlib.h`; the headers already carry
`extern "C"`.
