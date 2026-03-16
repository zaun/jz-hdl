/**
 * @file version.h
 * @brief Compiler version string.
 *
 * NOTE: During a CMake build, a generated version.h in the build directory
 * takes precedence over this file (it includes the git commit hash).
 * This fallback exists for non-CMake builds or IDE indexing.
 */

#ifndef JZ_HDL_VERSION_H
#define JZ_HDL_VERSION_H

#define JZ_HDL_VERSION_MAJOR 0
#define JZ_HDL_VERSION_MINOR 1
#define JZ_HDL_VERSION_PATCH 4

#define JZ_HDL_VERSION_STRING "Version 0.1.4 (unknown)"

#endif /* JZ_HDL_VERSION_H */
