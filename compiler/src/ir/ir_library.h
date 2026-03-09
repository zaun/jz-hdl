/**
 * @file ir_library.h
 * @brief CDC library module construction and CDC-to-instance lowering.
 *
 * After the main IR construction pass, this module scans for CDC crossings
 * and creates real IR_Module entries for each required CDC primitive
 * (BIT, BUS, FIFO). Each IR_CDC entry is then lowered into a standard
 * IR_Instance so the Verilog backend can emit them without special-casing.
 */

#ifndef JZ_HDL_IR_LIBRARY_H
#define JZ_HDL_IR_LIBRARY_H

#include "ir.h"
#include "arena.h"

/**
 * @brief Build library modules for CDC primitives and lower CDC entries.
 *
 * Scans all modules in the design for CDC crossings, creates one IR_Module
 * per unique CDC variant, appends them to the design's module array, and
 * replaces each IR_CDC entry with an IR_Instance pointing to the appropriate
 * library module.
 *
 * @param design IR design to modify (modules array may be reallocated).
 * @param arena  Arena for all IR allocations.
 * @return 0 on success, non-zero on failure.
 */
int ir_build_library_modules(IR_Design *design, JZArena *arena);

/**
 * @brief Lower memory writes into clock domain statement trees.
 *
 * For each module's clock domain, finds memory write ports that belong to
 * that clock domain and creates STMT_IF / STMT_MEM_WRITE statements that
 * are appended to the clock domain's statement tree. When the clock domain
 * has an async reset, the memory writes are wrapped in a NOT-reset guard.
 *
 * @param design IR design to modify.
 * @param arena  Arena for all IR allocations.
 * @return 0 on success, non-zero on failure.
 */
int ir_lower_memory_writes(IR_Design *design, JZArena *arena);

#endif /* JZ_HDL_IR_LIBRARY_H */
