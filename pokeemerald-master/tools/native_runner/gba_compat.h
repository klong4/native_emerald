#ifndef GBA_COMPAT_H
#define GBA_COMPAT_H

// Compatibility shims for GBA code compiled with MSVC (native runner).
// The original GBA codebase uses GCC-specific attributes (__attribute__).
// MSVC does not support __attribute__ syntax, so we provide empty or equivalent macros.

#ifdef _MSC_VER

// Define __attribute__ to nothing for MSVC (GCC-specific extension)
#ifndef __attribute__
#define __attribute__(x)
#endif

// ALIGNED macro for MSVC (will conflict with gba/defines.h, so undefine it there if needed)
// We'll let gba/defines.h win by checking if ALIGNED is already defined.
#ifdef ALIGNED
#undef ALIGNED
#endif
#define ALIGNED(n) __declspec(align(n))

// Struct packing for MSVC
#define PACKED_STRUCT_BEGIN __pragma(pack(push, 1))
#define PACKED_STRUCT_END   __pragma(pack(pop))

// Provide a minimal struct packing shim.
// Many structs use __attribute__((packed)); for MSVC we'll wrap with pragma pack.
// The easiest shim is to ignore it globally or handle via #pragma pack in specific files.
// For now we define these empty since __attribute__ is already neutered above.

#else // GCC/Clang

// GCC/Clang: keep __attribute__ as-is
#ifndef ALIGNED
#define ALIGNED(n) __attribute__((aligned(n)))
#endif

#define PACKED_STRUCT_BEGIN
#define PACKED_STRUCT_END

#endif // _MSC_VER

#endif // GBA_COMPAT_H
