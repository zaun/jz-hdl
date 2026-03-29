cmake_minimum_required(VERSION 3.16)

project(jz_hdl VERSION 0.1.6 LANGUAGES C)

# Default to Debug if no build type is specified (for single-config generators)
if(NOT CMAKE_CONFIGURATION_TYPES)
    if(NOT CMAKE_BUILD_TYPE)
        set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
    endif()
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release RelWithDebInfo)
endif()

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# ---- SQLite (for JZW waveform format) ----
include(FetchContent)
FetchContent_Declare(
    sqlite3
    URL https://www.sqlite.org/2025/sqlite-amalgamation-3490100.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(sqlite3)

add_library(sqlite3_lib STATIC ${sqlite3_SOURCE_DIR}/sqlite3.c)
target_include_directories(sqlite3_lib PUBLIC ${sqlite3_SOURCE_DIR})
# SQLite needs C extensions and BSD types; silence warnings in third-party code
set_target_properties(sqlite3_lib PROPERTIES C_EXTENSIONS ON C_STANDARD 99)
target_compile_options(sqlite3_lib PRIVATE -w)
target_compile_definitions(sqlite3_lib PRIVATE _BSD_SOURCE)
# SQLite needs pthreads on POSIX
find_package(Threads)
if(Threads_FOUND)
    target_link_libraries(sqlite3_lib PUBLIC Threads::Threads)
endif()
# SQLite needs dlopen on some platforms
include(CheckLibraryExists)
check_library_exists(dl dlopen "" HAVE_LIBDL)
if(HAVE_LIBDL)
    target_link_libraries(sqlite3_lib PUBLIC dl)
endif()

# Expose POSIX and default libc APIs (realpath, strdup, strtok_r, lstat, etc.)
# Applied to jz_hdl_lib only (not third-party code like SQLite).

# Strict warnings for supported compilers
if(CMAKE_C_COMPILER_ID MATCHES "Clang" OR CMAKE_C_COMPILER_ID MATCHES "GNU")
    add_compile_options(-Wall -Wextra -Wpedantic -Wunused-function -Wunused-variable -Wunused-but-set-variable)
    # GCC's -Wformat-truncation assumes every %s could be 255 bytes and warns
    # even though snprintf safely truncates.  Clang does not have this warning.
    if(CMAKE_C_COMPILER_ID MATCHES "GNU")
        add_compile_options(-Wno-format-truncation)
    endif()
elseif(MSVC)
    add_compile_options(/W4)
endif()

# Simulation performance tracking
option(TRACK_PERF "Enable simulation performance tracking" OFF)
if(TRACK_PERF)
    add_compile_definitions(TRACK_PERF)
endif()

# CI-only Werror toggle
option(JZ_HDL_WERROR "Treat warnings as errors" OFF)
if(JZ_HDL_WERROR)
    if(CMAKE_C_COMPILER_ID MATCHES "Clang" OR CMAKE_C_COMPILER_ID MATCHES "GNU")
        add_compile_options(-Werror)
    elseif(MSVC)
        add_compile_options(/WX)
    endif()
endif()

# Testing option using CTest and tests/*.jz
include(CTest)
if(BUILD_TESTING)
    add_test(NAME const_eval       COMMAND jz-const-eval-test)
    add_test(NAME type_semantics   COMMAND jz-type-test)
    add_test(NAME literal_semantics COMMAND jz-literal-test)
    add_test(
        NAME lint_validation
        COMMAND ${CMAKE_COMMAND} -E env JZ_HDL_BIN=$<TARGET_FILE:jz-hdl>
                bash ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_validation.sh
    )
endif()

add_library(jz_hdl_lib
    src/ast/ast.c
    src/ast/ast_json.c
    src/lexer.c
    src/util.c
    src/path_security.c
    src/chip_data.c
    src/compiler.c
    src/arena.c
    src/diagnostic.c
    src/ir/ir_build_clock.c
    src/ir/ir_build_design.c
    src/ir/ir_build_expr.c
    src/ir/ir_build_instance.c
    src/ir/ir_build_memory.c
    src/ir/ir_build_signal.c
    src/ir/ir_build_stmt.c
    src/ir/ir_library.c
    src/ir/ir_serialize.c
    src/ir/ir_div_guard.c
    src/ir/ir_tristate_transform.c
    src/parser/parser_blocks.c
    src/parser/parser_cdc.c
    src/parser/parser_core.c
    src/parser/parser_mem.c
    src/parser/parser_mux.c
    src/parser/parser_port.c
    src/parser/parser_project_blocks.c
    src/parser/parser_register.c
    src/parser/parser_wire.c
    src/parser/parser_expressions.c
    src/parser/parser_import.c
    src/parser/parser_instance.c
    src/parser/parser_module.c
    src/parser/parser_project.c
    src/parser/parser_statements.c
    src/parser/parser_template.c
    src/parser/parser_testbench.c
    src/parser/parser_simulation.c
    src/sem/const_eval.c
    src/sem/type.c
    src/sem/literal.c
    src/sem/driver.c
    src/sem/driver_literal.c
    src/sem/driver_operators.c
    src/sem/driver_flow.c
    src/sem/driver_control.c
    src/sem/driver_assign.c
    src/sem/driver_width.c
    src/sem/driver_instance.c
    src/sem/driver_expr.c
    src/sem/driver_net.c
    src/sem/driver_tristate.c
    src/sem/driver_comb.c
    src/sem/driver_clocks.c
    src/sem/driver_mem.c
    src/sem/driver_project.c
    src/sem/driver_project_hw.c
    src/repeat_expand.c
    src/sem/template_expand.c
    src/sem/driver_testbench.c
    src/report/alias/alias_report.c
    src/report/memory_report.c
    src/report/chip_report.c
    src/report/tristate/tristate_report.c
    src/rules.c
    src/backend/verilog-2005/alias.c
    src/backend/verilog-2005/common.c
    src/backend/verilog-2005/constraints.c
    src/backend/verilog-2005/emit_blocks.c
    src/backend/verilog-2005/emit_decl.c
    src/backend/verilog-2005/emit_expr.c
    src/backend/verilog-2005/emit_instances.c
    src/backend/verilog-2005/emit_stmt.c
    src/backend/verilog-2005/emit_wrapper.c
    src/backend/verilog-2005/verilog_main.c
    src/backend/rtlil/common.c
    src/backend/rtlil/emit_cells.c
    src/backend/rtlil/emit_instances.c
    src/backend/rtlil/emit_memory.c
    src/backend/rtlil/emit_module.c
    src/backend/rtlil/emit_process.c
    src/backend/rtlil/emit_wrapper.c
    src/backend/rtlil/rtlil_main.c
    src/sim/sim_value.c
    src/sim/sim_state.c
    src/sim/sim_eval.c
    src/sim/sim_exec.c
    src/sim/sim_engine.c
    src/sim/sim_vcd.c
    src/sim/sim_fst.c
    src/sim/sim_jzw.c
    src/sim/sim_waveform.c
    src/sim/sim_perf.c
    src/lsp/lsp_io.c
    src/lsp/lsp_json.c
    src/lsp/lsp_project_discovery.c
    src/lsp/lsp_server.c
)

# Embed JSON data files as headers
file(GLOB JSON_DATA_FILES "${CMAKE_CURRENT_SOURCE_DIR}/data/*.json")

# Create output directory for generated headers
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated/data")

set(GENERATED_JSON_HEADERS "")

foreach(JSON_FILE ${JSON_DATA_FILES})
    get_filename_component(JSON_NAME ${JSON_FILE} NAME_WE)
    set(HEADER_FILE "${CMAKE_CURRENT_BINARY_DIR}/generated/data/${JSON_NAME}.h")
    
    add_custom_command(
        OUTPUT ${HEADER_FILE}
        COMMAND ${CMAKE_COMMAND}
            -D INPUT_FILE=${JSON_FILE}
            -D OUTPUT_FILE=${HEADER_FILE}
            -D VAR_NAME=${JSON_NAME}_json
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/EmbedJson.cmake
        DEPENDS ${JSON_FILE} ${CMAKE_CURRENT_SOURCE_DIR}/cmake/EmbedJson.cmake
        COMMENT "Generating ${JSON_NAME}.h from ${JSON_NAME}.json"
    )
    
    list(APPEND GENERATED_JSON_HEADERS ${HEADER_FILE})
endforeach()

# Create custom target for all generated headers
add_custom_target(generate_json_headers ALL DEPENDS ${GENERATED_JSON_HEADERS})

# Generate version.h at build time (includes git commit hash)
set(VERSION_HEADER "${CMAKE_CURRENT_BINARY_DIR}/generated/version.h")
add_custom_target(generate_version ALL
    COMMAND ${CMAKE_COMMAND}
        -D VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
        -D VERSION_MINOR=${PROJECT_VERSION_MINOR}
        -D VERSION_PATCH=${PROJECT_VERSION_PATCH}
        -D OUTPUT_FILE=${VERSION_HEADER}
        -D SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/..
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/GenerateVersion.cmake
    COMMENT "Generating version.h"
)

# Make library depend on headers being generated first
add_dependencies(jz_hdl_lib generate_json_headers generate_version)

# Add generated headers to include path
target_include_directories(jz_hdl_lib
    PUBLIC
        ${CMAKE_CURRENT_BINARY_DIR}/generated
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(jz_hdl_lib PUBLIC _DEFAULT_SOURCE _POSIX_C_SOURCE=200809L)
target_link_libraries(jz_hdl_lib PUBLIC sqlite3_lib)

add_executable(jz-hdl
    src/main.c
)

target_link_libraries(jz-hdl PRIVATE jz_hdl_lib m)

add_executable(jz-const-eval-test
    src/sem/const_eval_test.c
)

target_link_libraries(jz-const-eval-test PRIVATE jz_hdl_lib)

add_executable(jz-type-test
    src/sem/type_test.c
)

target_link_libraries(jz-type-test PRIVATE jz_hdl_lib)

add_executable(jz-literal-test
    src/sem/literal_test.c
)

target_link_libraries(jz-literal-test PRIVATE jz_hdl_lib)

# Documentation target (requires pandoc and xelatex)
# LaTeX packages needed (install via: sudo tlmgr install <pkg>):
#   framed, titling, collection-fontsrecommended
# Or install most common packages at once:
#   sudo tlmgr install collection-latexrecommended
find_program(PANDOC_EXECUTABLE pandoc)
if(PANDOC_EXECUTABLE)
    # 1. Define your list of files (Filename without extension)
    set(DOC_FILES
        "jz-hdl-specification"
        "simulation-specification"
        "testbench-specification"
        "chip-info-specification"
        "jzw-specification"
    )

    set(PDF_OUTPUTS "")

    foreach(DOC_NAME ${DOC_FILES})
        set(SRC_MD "${CMAKE_CURRENT_SOURCE_DIR}/../specification/${DOC_NAME}.md")
        set(OUT_PDF "${CMAKE_CURRENT_BINARY_DIR}/${DOC_NAME}.pdf")

        # 2. Create an individual custom command for each file
        add_custom_command(
            OUTPUT "${OUT_PDF}"
            COMMAND ${PANDOC_EXECUTABLE}
                "${SRC_MD}"
                -o "${OUT_PDF}"
                --pdf-engine=xelatex
                -V geometry:margin=1in
                -V fontsize=11pt
                --syntax-highlighting=tango
                -V colorlinks=true
            DEPENDS "${SRC_MD}"
            COMMENT "Building PDF for ${DOC_NAME}"
        )
        
        list(APPEND PDF_OUTPUTS "${OUT_PDF}")
    endforeach()

    # 3. Copy built PDFs to the docs site public directory
    set(DOCS_PUBLIC_PDF_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../docs/public/pdf")
    set(PDF_COPY_COMMANDS "")
    foreach(DOC_NAME ${DOC_FILES})
        list(APPEND PDF_COPY_COMMANDS
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_CURRENT_BINARY_DIR}/${DOC_NAME}.pdf"
                "${DOCS_PUBLIC_PDF_DIR}/${DOC_NAME}.pdf"
        )
    endforeach()

    # 4. Create one top-level target to build all docs and copy to site
    add_custom_target(docs ALL
        DEPENDS ${PDF_OUTPUTS}
        ${PDF_COPY_COMMANDS}
        COMMENT "Copying PDFs to docs/public/pdf/"
    )

else()
    message(STATUS "pandoc not found - 'docs' target will not be available")
endif()

# Install rules
include(GNUInstallDirs)
install(TARGETS jz-hdl RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
