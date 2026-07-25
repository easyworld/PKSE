/* NanoVG GL3 backend implementation unit.
 * glad (the GL 4.3 loader) MUST be included before nanovg_gl.h so the GL entry points
 * are declared. This is the single translation unit that defines the GL backend. */
#include <glad/glad.h>
#include "nanovg.h"
#define NANOVG_GL3_IMPLEMENTATION
#include "nanovg_gl.h"
