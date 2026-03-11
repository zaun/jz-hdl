#include <stdio.h>
#include <string.h>

#include "../../include/ast.h"

static void print_escaped_string(FILE *out, const char *s) {
    fputc('"', out);
    for (; s && *s; ++s) {
        unsigned char c = (unsigned char)*s;
        switch (c) {
        case '\\': fputs("\\\\", out); break;
        case '"': fputs("\\\"", out); break;
        case '\n': fputs("\\n", out); break;
        case '\r': fputs("\\r", out); break;
        case '\t': fputs("\\t", out); break;
        default:
            if (c < 0x20) {
                fprintf(out, "\\u%04x", c);
            } else {
                fputc(c, out);
            }
            break;
        }
    }
    fputc('"', out);
}

static const char *node_type_name(JZASTNodeType t) {
    switch (t) {
    case JZ_AST_MODULE:                return "Module";
    case JZ_AST_PROJECT:               return "Project";
    case JZ_AST_BLOCK:                 return "Block";
    case JZ_AST_BLACKBOX:              return "Blackbox";
    case JZ_AST_INSTANTIATION:         return "Instantiation";
    case JZ_AST_RAW_TEXT:              return "RawText";
    case JZ_AST_FEATURE_GUARD:         return "FeatureGuard";
    case JZ_AST_CHECK:                 return "Check";

    /* Templates */
    case JZ_AST_TEMPLATE_DEF:          return "TemplateDef";
    case JZ_AST_TEMPLATE_PARAM:        return "TemplateParam";
    case JZ_AST_TEMPLATE_APPLY:        return "TemplateApply";
    case JZ_AST_SCRATCH_DECL:          return "ScratchDecl";

    /* Declarations */
    case JZ_AST_CONST_BLOCK:           return "ConstBlock";
    case JZ_AST_PORT_BLOCK:            return "PortBlock";
    case JZ_AST_WIRE_BLOCK:            return "WireBlock";
    case JZ_AST_REGISTER_BLOCK:        return "RegisterBlock";
    case JZ_AST_LATCH_BLOCK:           return "LatchBlock";
    case JZ_AST_MEM_BLOCK:             return "MemBlock";
    case JZ_AST_MUX_BLOCK:             return "MuxBlock";
    case JZ_AST_BUS_BLOCK:             return "BusBlock";
    case JZ_AST_CONFIG_BLOCK:          return "ConfigBlock";
    case JZ_AST_GLOBAL_BLOCK:          return "GlobalBlock";
    case JZ_AST_CLOCKS_BLOCK:          return "ClocksBlock";
    case JZ_AST_IN_PINS_BLOCK:         return "InPinsBlock";
    case JZ_AST_OUT_PINS_BLOCK:        return "OutPinsBlock";
    case JZ_AST_INOUT_PINS_BLOCK:      return "InoutPinsBlock";
    case JZ_AST_MAP_BLOCK:             return "MapBlock";
    case JZ_AST_CLOCK_GEN_BLOCK:       return "ClockGenBlock";
    case JZ_AST_CLOCK_GEN_UNIT:        return "ClockGenUnit";
    case JZ_AST_CLOCK_GEN_IN:          return "ClockGenIn";
    case JZ_AST_CLOCK_GEN_OUT:         return "ClockGenOut";
    case JZ_AST_CLOCK_GEN_CONFIG:      return "ClockGenConfig";
    case JZ_AST_MODULE_INSTANCE:       return "ModuleInstance";
    case JZ_AST_PROJECT_TOP_INSTANCE:  return "ProjectTopInstance";
    case JZ_AST_CONST_DECL:            return "ConstDecl";
    case JZ_AST_PORT_DECL:             return "PortDecl";
    case JZ_AST_WIRE_DECL:             return "WireDecl";
    case JZ_AST_REGISTER_DECL:         return "RegisterDecl";
    case JZ_AST_LATCH_DECL:            return "LatchDecl";
    case JZ_AST_MEM_DECL:              return "MemDecl";
    case JZ_AST_MEM_PORT:              return "MemPort";
    case JZ_AST_MUX_DECL:              return "MuxDecl";
    case JZ_AST_BUS_DECL:              return "BusDecl";
    case JZ_AST_CDC_DECL:              return "CdcDecl";

    /* Types and widths */
    case JZ_AST_WIDTH_EXPR:            return "WidthExpr";
    case JZ_AST_MEM_DEPTH_EXPR:        return "MemDepthExpr";

    /* Misc */
    case JZ_AST_SYNC_PARAM:            return "SyncParam";

    /* Statements */
    case JZ_AST_STMT_ASSIGN:           return "Assign";
    case JZ_AST_STMT_IF:               return "If";
    case JZ_AST_STMT_ELIF:             return "Elif";
    case JZ_AST_STMT_ELSE:             return "Else";
    case JZ_AST_STMT_SELECT:           return "Select";
    case JZ_AST_STMT_CASE:             return "Case";
    case JZ_AST_STMT_DEFAULT:          return "Default";

    /* Expressions */
    case JZ_AST_EXPR_LITERAL:          return "Literal";
    case JZ_AST_EXPR_IDENTIFIER:       return "Identifier";
    case JZ_AST_EXPR_QUALIFIED_IDENTIFIER: return "QualifiedIdentifier";
    case JZ_AST_EXPR_BUS_ACCESS:       return "BusAccess";
    case JZ_AST_EXPR_UNARY:            return "Unary";
    case JZ_AST_EXPR_BINARY:           return "Binary";
    case JZ_AST_EXPR_TERNARY:          return "Ternary";
    case JZ_AST_EXPR_CONCAT:           return "Concat";
    case JZ_AST_EXPR_SLICE:            return "Slice";
    case JZ_AST_EXPR_BUILTIN_CALL:     return "BuiltinCall";
    case JZ_AST_EXPR_SPECIAL_DRIVER:   return "SpecialDriver";

    /* Testbench */
    case JZ_AST_TESTBENCH:             return "Testbench";
    case JZ_AST_TB_TEST:               return "TbTest";
    case JZ_AST_TB_CLOCK_BLOCK:        return "TbClockBlock";
    case JZ_AST_TB_CLOCK_DECL:         return "TbClockDecl";
    case JZ_AST_TB_WIRE_BLOCK:         return "TbWireBlock";
    case JZ_AST_TB_WIRE_DECL:          return "TbWireDecl";
    case JZ_AST_TB_SETUP:              return "TbSetup";
    case JZ_AST_TB_UPDATE:             return "TbUpdate";
    case JZ_AST_TB_CLOCK_ADV:          return "TbClockAdv";
    case JZ_AST_TB_EXPECT_EQ:          return "TbExpectEqual";
    case JZ_AST_TB_EXPECT_NEQ:         return "TbExpectNotEqual";
    case JZ_AST_TB_EXPECT_TRI:         return "TbExpectTristate";

    /* Simulation */
    case JZ_AST_SIMULATION:            return "Simulation";
    case JZ_AST_SIM_CLOCK_BLOCK:       return "SimClockBlock";
    case JZ_AST_SIM_CLOCK_DECL:        return "SimClockDecl";
    case JZ_AST_SIM_TAP_BLOCK:         return "SimTapBlock";
    case JZ_AST_SIM_TAP_DECL:          return "SimTapDecl";
    case JZ_AST_SIM_RUN:               return "SimRun";
    case JZ_AST_SIM_RUN_UNTIL:         return "SimRunUntil";
    case JZ_AST_SIM_RUN_WHILE:         return "SimRunWhile";
    case JZ_AST_PRINT:                 return "Print";
    case JZ_AST_PRINT_IF:              return "PrintIf";
    case JZ_AST_SIM_TRACE:             return "SimTrace";
    case JZ_AST_SIM_MARK:              return "SimMark";
    case JZ_AST_SIM_ALERT:             return "SimAlert";

    default:
        return "Unknown";
    }
}

static void indent(FILE *out, int level) {
    for (int i = 0; i < level; ++i) fputs("  ", out);
}

static void print_node(FILE *out, const JZASTNode *node, int level) {
    indent(out, level);
    fputc('{', out);

    /* type */
    fputs("\"type\": ", out);
    print_escaped_string(out, node_type_name(node->type));

    /* name */
    if (node->name) {
        fputs(", \"name\": ", out);
        print_escaped_string(out, node->name);
    }

    /* block_kind */
    if (node->block_kind) {
        fputs(", \"block_kind\": ", out);
        print_escaped_string(out, node->block_kind);
    }

    /* text: suppress raw-text blobs from legacy fallback nodes to keep the
     * JSON schema focused on structured AST. Other node kinds may still use
     * small text fields (e.g. CONST/MEM attributes), but we avoid emitting
     * large unstructured RawText payloads here.
     */
    if (node->text && node->type != JZ_AST_RAW_TEXT) {
        fputs(", \"text\": ", out);
        print_escaped_string(out, node->text);
    }

    /* width */
    if (node->width) {
        fputs(", \"width\": ", out);
        print_escaped_string(out, node->width);
    }

    /* location */
    fputs(", \"loc\": {", out);
    fputs("\"line\": ", out);
    fprintf(out, "%d", node->loc.line);
    fputs(", \"column\": ", out);
    fprintf(out, "%d", node->loc.column);
    if (node->loc.filename) {
        fputs(", \"file\": ", out);
        print_escaped_string(out, node->loc.filename);
    }
    fputc('}', out);

    /* children */
    if (node->child_count > 0) {
        fputs(", \"children\": [\n", out);
        for (size_t i = 0; i < node->child_count; ++i) {
            print_node(out, node->children[i], level + 1);
            if (i + 1 < node->child_count) fputc(',', out);
            fputc('\n', out);
        }
        indent(out, level);
        fputc(']', out);
    }

    fputc('}', out);
}

void jz_ast_print_json(FILE *out, const JZASTNode *root) {
    if (!root) {
        fputs("null\n", out);
        return;
    }
    print_node(out, root, 0);
    fputc('\n', out);
}
