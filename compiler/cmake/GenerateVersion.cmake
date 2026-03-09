# GenerateVersion.cmake
# Build-time script to generate version.h with git commit hash.
#
# Expected variables (passed via -D):
#   VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH - semantic version components
#   OUTPUT_FILE - path to write the generated header
#   SOURCE_DIR  - top-level source directory (for git)

find_package(Git QUIET)

set(GIT_COMMIT "unknown")
if(GIT_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
        WORKING_DIRECTORY "${SOURCE_DIR}"
        OUTPUT_VARIABLE GIT_COMMIT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE GIT_RESULT
    )
    if(NOT GIT_RESULT EQUAL 0)
        set(GIT_COMMIT "unknown")
    endif()
endif()

set(VERSION_NUMBER "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}")
set(VERSION_STRING "Version ${VERSION_NUMBER} (${GIT_COMMIT})")

file(WRITE "${OUTPUT_FILE}"
"/**
 * @file version.h
 * @brief Compiler version string (auto-generated, do not edit).
 */

#ifndef JZ_HDL_VERSION_H
#define JZ_HDL_VERSION_H

#define JZ_HDL_VERSION_MAJOR ${VERSION_MAJOR}
#define JZ_HDL_VERSION_MINOR ${VERSION_MINOR}
#define JZ_HDL_VERSION_PATCH ${VERSION_PATCH}

/** @brief Version string for the jz-hdl compiler. */
#define JZ_HDL_VERSION_STRING \"${VERSION_STRING}\"

#endif /* JZ_HDL_VERSION_H */
")
