// Workaround for a GCC < 13 bug where __attribute__((visibility("default")))
// cannot be parsed after [[deprecated("...")]] in a class declaration.
// This affects the v8-primitive.h header shipped with Electron 43.
//
// Strategy: pre-include v8config.h here to trip its header guard
// (V8CONFIG_H_), then override V8_EXPORT to empty.  When the real inclusion
// chain later tries to include v8config.h again the guard fires, it is
// skipped, and V8_EXPORT stays empty so v8-primitive.h parses without error.
#if defined(__GNUC__) && !defined(__clang__)
#include <v8config.h>
#undef V8_EXPORT
#define V8_EXPORT
#endif
