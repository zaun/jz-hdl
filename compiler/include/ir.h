/**
 * @file ir.h
 * @brief Intermediate Representation (IR) types for JZ-HDL.
 *
 * Defines the full IR type hierarchy: signals, expressions, statements,
 * clock domains, memories, module instances, CDC crossings, modules,
 * projects, and the top-level design root. The IR is constructed from a
 * verified AST by jz_ir_build_design() and consumed by backends.
 *
 * @par ID conventions
 *   - IR_Module.id: Index into IR_Design.modules[], unique within the design.
 *   - IR_Signal.id: Stable symbol ID from the semantic layer, unique within
 *     the owning module. The pair (owner_module_id, id) is globally unique.
 *   - IR_Memory.id, IR_ClockDomain.id, IR_Instance.id, IR_CDC.id: Indices
 *     into their owning IR_Module's arrays; unique only within that module.
 *   - IR_Pin.id: Index into IR_Project.pins[], unique within the project.
 *   - Other integer IDs (e.g., IR_Assignment.id) are local to the containing
 *     statement tree and intended for diagnostics/debugging only.
 *
 * @par String ownership
 *   All char* fields in IR structs are owned by the IR allocator (typically
 *   the IR arena attached to JZCompiler). Callers must treat these pointers
 *   as read-only and must not free them individually; they remain valid for
 *   the lifetime of the IR_Design.
 */

#ifndef JZ_HDL_IR_H
#define JZ_HDL_IR_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @enum IR_SignalKind
 * @brief Unified signal kind: ports, nets, registers, latches.
 */
typedef enum IR_SignalKind {
    SIG_PORT,      /**< Port signal (input, output, or bidirectional). */
    SIG_NET,       /**< Combinational net (wire). */
    SIG_REGISTER,  /**< Clocked register. */
    SIG_LATCH      /**< Level-sensitive latch. */
} IR_SignalKind;

/**
 * @enum IR_PortDirection
 * @brief Port directions for SIG_PORT signals.
 */
typedef enum IR_PortDirection {
    PORT_IN,    /**< Input port. */
    PORT_OUT,   /**< Output port. */
    PORT_INOUT  /**< Bidirectional port. */
} IR_PortDirection;

/** Number of 64-bit words in an IR_Literal (supports up to 256 bits). */
#define IR_LIT_WORDS 4

/**
 * @struct IR_Literal
 * @brief Simple integer literal with explicit width.
 */
typedef struct IR_Literal {
    uint64_t words[IR_LIT_WORDS];  /**< Literal value words (word 0 = LSB). */
    int      width;                /**< Bit width of the value. */
    int      is_z;                 /**< Non-zero if this is an all-z (high-impedance) literal. */
} IR_Literal;

/**
 * @struct IR_Signal
 * @brief Unified signal type used by all backends.
 */
typedef struct IR_Signal {
    int            id;               /**< Unique within owner module; see file header. */
    char          *name;             /**< Signal name (owned by IR allocator). */
    IR_SignalKind  kind;             /**< Signal kind (port, net, register, latch). */
    int            width;            /**< Ground-truth bit-width. */
    int            owner_module_id;  /**< Module containing this signal. */
    int            source_line;      /**< Originating source line. */

    bool           can_be_z;         /**< True if any driver can assign 'z'. */
    bool           iob;              /**< True if this latch should be placed in the IOB. */

    union {
        struct {
            IR_PortDirection direction;   /**< Port direction (IN, OUT, INOUT). */
        } port;

        struct {
            int _unused; /**< Placeholder to avoid empty-struct warnings. */
        } net;

        struct {
            IR_Literal  reset_value;          /**< Concrete reset literal. */
            const char *reset_value_gnd_vcc;  /**< "GND" or "VCC" for polymorphic reset. */
            int         home_clock_domain_id; /**< Owning clock domain. */
        } reg;
    } u; /**< Kind-specific data (discriminated by IR_Signal.kind). */
} IR_Signal;

/* Forward declarations for recursive IR node types. */
typedef struct IR_Expr IR_Expr;
typedef struct IR_Stmt IR_Stmt;

/**
 * @enum IR_ExprKind
 * @brief Expression kinds (semantic, not syntactic).
 */
typedef enum IR_ExprKind {
    /* Literals and references */
    EXPR_LITERAL,          /**< Integer literal. */
    EXPR_SIGNAL_REF,       /**< Reference to a signal by ID. */

    /* Unary operators */
    EXPR_UNARY_NOT,        /**< Bitwise NOT (~). */
    EXPR_UNARY_NEG,        /**< Arithmetic negation (-). */
    EXPR_LOGICAL_NOT,      /**< Logical NOT (!). */

    /* Binary arithmetic */
    EXPR_BINARY_ADD,       /**< Addition (+). */
    EXPR_BINARY_SUB,       /**< Subtraction (-). */
    EXPR_BINARY_MUL,       /**< Multiplication (*). */
    EXPR_BINARY_DIV,       /**< Division (/). */
    EXPR_BINARY_MOD,       /**< Modulo (%). */

    /* Bitwise */
    EXPR_BINARY_AND,       /**< Bitwise AND (&). */
    EXPR_BINARY_OR,        /**< Bitwise OR (|). */
    EXPR_BINARY_XOR,       /**< Bitwise XOR (^). */

    /* Shifts */
    EXPR_BINARY_SHL,       /**< Shift left (<<). */
    EXPR_BINARY_SHR,       /**< Logical shift right (>>). */
    EXPR_BINARY_ASHR,      /**< Arithmetic shift right (>>>). */

    /* Comparisons */
    EXPR_BINARY_EQ,        /**< Equality (==). */
    EXPR_BINARY_NEQ,       /**< Inequality (!=). */
    EXPR_BINARY_LT,        /**< Less than (<). */
    EXPR_BINARY_GT,        /**< Greater than (>). */
    EXPR_BINARY_LTE,       /**< Less than or equal (<=). */
    EXPR_BINARY_GTE,       /**< Greater than or equal (>=). */

    /* Logical */
    EXPR_LOGICAL_AND,      /**< Logical AND (&&). */
    EXPR_LOGICAL_OR,       /**< Logical OR (||). */

    /* Ternary and concatenation */
    EXPR_TERNARY,          /**< Ternary conditional (? :). */
    EXPR_CONCAT,           /**< Concatenation. */
    EXPR_SLICE,            /**< Bit slice [msb:lsb]. */

    /* Intrinsics */
    EXPR_INTRINSIC_UADD,   /**< Unsigned widening add. */
    EXPR_INTRINSIC_SADD,   /**< Signed widening add. */
    EXPR_INTRINSIC_UMUL,   /**< Unsigned widening multiply. */
    EXPR_INTRINSIC_SMUL,   /**< Signed widening multiply. */
    EXPR_INTRINSIC_GBIT,   /**< Get single bit. */
    EXPR_INTRINSIC_SBIT,   /**< Set single bit. */
    EXPR_INTRINSIC_GSLICE, /**< Get bit slice. */
    EXPR_INTRINSIC_SSLICE, /**< Set bit slice. */
    EXPR_INTRINSIC_OH2B,   /**< One-hot to binary encoder. */
    EXPR_INTRINSIC_B2OH,   /**< Binary to one-hot decoder. */
    EXPR_INTRINSIC_PRIENC, /**< Priority encoder (MSB-first). */
    EXPR_INTRINSIC_LZC,    /**< Leading zero count. */
    EXPR_INTRINSIC_USUB,   /**< Unsigned widening subtract. */
    EXPR_INTRINSIC_SSUB,   /**< Signed widening subtract. */
    EXPR_INTRINSIC_ABS,    /**< Signed absolute value. */
    EXPR_INTRINSIC_UMIN,   /**< Unsigned minimum. */
    EXPR_INTRINSIC_UMAX,   /**< Unsigned maximum. */
    EXPR_INTRINSIC_SMIN,   /**< Signed minimum. */
    EXPR_INTRINSIC_SMAX,   /**< Signed maximum. */
    EXPR_INTRINSIC_POPCOUNT, /**< Population count. */
    EXPR_INTRINSIC_REVERSE,  /**< Bit reversal. */
    EXPR_INTRINSIC_BSWAP,    /**< Byte swap. */
    EXPR_INTRINSIC_REDUCE_AND, /**< AND reduction. */
    EXPR_INTRINSIC_REDUCE_OR,  /**< OR reduction. */
    EXPR_INTRINSIC_REDUCE_XOR, /**< XOR reduction. */

    /* Memory read */
    EXPR_MEM_READ          /**< Memory read expression. */
} IR_ExprKind;

/**
 * @struct IR_Expr
 * @brief Expression node with width metadata.
 */
struct IR_Expr {
    IR_ExprKind kind;         /**< Expression kind discriminator. */
    int         width;        /**< Result width (ground truth). */
    int         source_line;  /**< Originating source line. */
    const char *const_name;   /**< Non-NULL when lowered from CONST/GLOBAL identifier. */

    union {
        struct {
            IR_Literal literal; /**< Literal value and width. */
        } literal;

        struct {
            int signal_id;      /**< Reference to IR_Signal by ID. */
        } signal_ref;

        struct {
            IR_Expr *operand;   /**< Single operand. */
        } unary;

        struct {
            IR_Expr *left;      /**< Left operand. */
            IR_Expr *right;     /**< Right operand. */
        } binary;

        struct {
            IR_Expr *condition; /**< Condition (width-1). */
            IR_Expr *true_val;  /**< True-branch value. */
            IR_Expr *false_val; /**< False-branch value. */
        } ternary;

        struct {
            IR_Expr **operands;    /**< Array of concatenated expressions. */
            int       num_operands; /**< Number of operands. */
        } concat;

        struct {
            int signal_id;      /**< Base signal ID (used when base_expr is NULL). */
            IR_Expr *base_expr; /**< Base expression (non-NULL for expr slices like (a+b)[7:4]). */
            int msb;            /**< Most significant bit index. */
            int lsb;            /**< Least significant bit index. */
        } slice;

        struct {
            IR_Expr *source;        /**< Source expression (gbit/gslice/sbit/sslice). */
            IR_Expr *index;         /**< Index expression. */
            IR_Expr *value;         /**< Value expression (sbit/sslice only; may be NULL). */
            int      element_width; /**< Element width (gslice/sslice). */
        } intrinsic;

        struct {
            char    *memory_name;  /**< Name of the IR_Memory (e.g., "rom"). */
            char    *port_name;    /**< Name of the IR_MemoryPort (e.g., "read"). */
            IR_Expr *address;      /**< Address expression. */
        } mem_read;
    } u; /**< Kind-specific payload (discriminated by IR_Expr.kind). */
};

/**
 * @enum IR_AssignmentKind
 * @brief Assignment operator kinds preserving syntactic intent.
 */
typedef enum IR_AssignmentKind {
    ASSIGN_ALIAS,         /**< Direct alias (=). */
    ASSIGN_ALIAS_ZEXT,    /**< Zero-extending alias (=z). */
    ASSIGN_ALIAS_SEXT,    /**< Sign-extending alias (=s). */

    ASSIGN_DRIVE,         /**< Drive (=>). */
    ASSIGN_DRIVE_ZEXT,    /**< Zero-extending drive (=>z). */
    ASSIGN_DRIVE_SEXT,    /**< Sign-extending drive (=>s). */

    ASSIGN_RECEIVE,       /**< Receive (<=). */
    ASSIGN_RECEIVE_ZEXT,  /**< Zero-extending receive (<=z). */
    ASSIGN_RECEIVE_SEXT   /**< Sign-extending receive (<=s). */
} IR_AssignmentKind;

/**
 * @struct IR_Assignment
 * @brief Single assignment site.
 */
typedef struct IR_Assignment {
    int               id;             /**< Local assignment ID (debugging). */
    int               lhs_signal_id;  /**< Signal being assigned to. */
    IR_Expr          *rhs;            /**< Right-hand side expression. */
    IR_AssignmentKind kind;           /**< Operator kind. */
    bool              is_sliced;      /**< True if LHS is a part-select/concat. */
    int               lhs_msb;        /**< LHS MSB (valid when is_sliced). */
    int               lhs_lsb;        /**< LHS LSB (valid when is_sliced). */
    int               source_line;    /**< Originating source line. */
} IR_Assignment;

/**
 * @enum IR_StmtKind
 * @brief Statement kinds preserving control-flow structure.
 */
typedef enum IR_StmtKind {
    STMT_ASSIGNMENT, /**< Assignment statement. */
    STMT_IF,         /**< If/elif/else statement. */
    STMT_SELECT,     /**< Select/case statement. */
    STMT_BLOCK,      /**< Block of statements. */
    STMT_MEM_WRITE   /**< Memory write operation. */
} IR_StmtKind;

/**
 * @struct IR_MemWriteStmt
 * @brief Memory write statement data.
 */
typedef struct IR_MemWriteStmt {
    char    *memory_name;  /**< Target memory name (e.g., "ram"). */
    char    *port_name;    /**< Target port name (e.g., "write"). */
    IR_Expr *address;      /**< Address expression. */
    IR_Expr *data;         /**< Data expression. */
} IR_MemWriteStmt;

/**
 * @struct IR_IfStmt
 * @brief If statement with optional elif chain and else block.
 */
typedef struct IR_IfStmt {
    IR_Expr *condition;   /**< Condition expression (width-1). */
    IR_Stmt *then_block;  /**< Then-branch body. */
    IR_Stmt *elif_chain;  /**< Linked list of elif IFs, or NULL. */
    IR_Stmt *else_block;  /**< Else-branch body, or NULL. */
} IR_IfStmt;

/**
 * @struct IR_SelectCase
 * @brief A single case within a SELECT statement.
 */
typedef struct IR_SelectCase {
    IR_Expr *case_value;  /**< Case value expression (NULL for DEFAULT). */
    IR_Stmt *body;        /**< Case body statements. */
} IR_SelectCase;

/**
 * @struct IR_SelectStmt
 * @brief SELECT statement with selector and case list.
 */
typedef struct IR_SelectStmt {
    IR_Expr       *selector;   /**< Selector expression. */
    IR_SelectCase *cases;      /**< Array of cases. */
    int            num_cases;  /**< Number of cases. */
} IR_SelectStmt;

/**
 * @struct IR_BlockStmt
 * @brief A block (sequence) of statements.
 */
typedef struct IR_BlockStmt {
    IR_Stmt *stmts;   /**< Array of statements. */
    int      count;   /**< Number of statements. */
} IR_BlockStmt;

/**
 * @struct IR_Stmt
 * @brief Statement node.
 */
struct IR_Stmt {
    IR_StmtKind kind;        /**< Statement kind discriminator. */
    int         source_line; /**< Originating source line. */

    union {
        IR_Assignment   assign;      /**< Assignment data. */
        IR_IfStmt       if_stmt;     /**< If statement data. */
        IR_SelectStmt   select_stmt; /**< Select statement data. */
        IR_BlockStmt    block;       /**< Block data. */
        IR_MemWriteStmt mem_write;   /**< Memory write data. */
    } u; /**< Kind-specific payload. */
};

/**
 * @enum IR_ClockEdge
 * @brief Clock edge sensitivity.
 */
typedef enum IR_ClockEdge {
    EDGE_RISING,  /**< Rising (positive) edge. */
    EDGE_FALLING, /**< Falling (negative) edge. */
    EDGE_BOTH     /**< Both edges. */
} IR_ClockEdge;

/**
 * @enum IR_ResetPolarity
 * @brief Reset signal active polarity.
 */
typedef enum IR_ResetPolarity {
    RESET_ACTIVE_HIGH, /**< Active-high reset. */
    RESET_ACTIVE_LOW   /**< Active-low reset. */
} IR_ResetPolarity;

/**
 * @enum IR_ResetType
 * @brief Reset implementation type.
 */
typedef enum IR_ResetType {
    RESET_IMMEDIATE, /**< Asynchronous (immediate) reset. */
    RESET_CLOCKED    /**< Synchronous (clocked) reset. */
} IR_ResetType;

/**
 * @struct IR_SensitivityEntry
 * @brief Sensitivity list entry for clock domain always blocks.
 */
typedef struct IR_SensitivityEntry {
    int          signal_id; /**< Signal in the sensitivity list. */
    IR_ClockEdge edge;      /**< Edge type (posedge/negedge). */
} IR_SensitivityEntry;

/**
 * @struct IR_ClockDomain
 * @brief A clock domain with its registers, reset, and synchronous logic.
 */
typedef struct IR_ClockDomain {
    int          id;                  /**< Unique within owning module. */
    int          clock_signal_id;     /**< Clock port signal (SIG_PORT, width-1). */
    IR_ClockEdge edge;                /**< Active clock edge. */

    int         *register_ids;        /**< Array of SIG_REGISTER IDs in this domain. */
    int          num_registers;       /**< Number of registers. */

    int              reset_signal_id;      /**< Reset signal ID (-1 if none). */
    int              reset_sync_signal_id; /**< Synchronized reset signal ID (-1 if none). */
    IR_ResetPolarity reset_active;         /**< Reset active polarity. */
    IR_ResetType     reset_type;           /**< Reset implementation type. */

    IR_SensitivityEntry *sensitivity_list; /**< always @(...) entries. */
    int                  num_sensitivity;  /**< Number of sensitivity entries. */

    IR_Stmt     *statements;          /**< Body of SYNCHRONOUS block. */
} IR_ClockDomain;

/**
 * @enum IR_MemPortKind
 * @brief Memory port access type.
 */
typedef enum IR_MemPortKind {
    MEM_PORT_READ_ASYNC, /**< Asynchronous read port. */
    MEM_PORT_READ_SYNC,  /**< Synchronous read port. */
    MEM_PORT_WRITE,      /**< Write port. */
    MEM_PORT_INOUT       /**< Shared read/write port (INOUT). */
} IR_MemPortKind;

/**
 * @enum IR_MemWriteMode
 * @brief Memory write-port read-during-write behavior.
 */
typedef enum IR_MemWriteMode {
    WRITE_MODE_FIRST,      /**< Write-first mode. */
    WRITE_MODE_READ_FIRST, /**< Read-first mode. */
    WRITE_MODE_NO_CHANGE   /**< No-change mode. */
} IR_MemWriteMode;

/**
 * @enum IR_MemoryKind
 * @brief Memory implementation kind.
 */
typedef enum IR_MemoryKind {
    MEM_KIND_DISTRIBUTED, /**< Distributed (LUT-based) memory. */
    MEM_KIND_BLOCK        /**< Block RAM memory. */
} IR_MemoryKind;

/**
 * @struct IR_MemoryPort
 * @brief A single port on a memory, with optional signal bindings.
 *
 * Binding fields (addr_signal_id, data_in_signal_id, etc.) are populated
 * by jz_ir_bind_memory_ports(). All binding IDs are initialized to -1
 * and remain -1 when no binding could be inferred.
 */
typedef struct IR_MemoryPort {
    char          *name;             /**< Port name. */
    IR_MemPortKind kind;             /**< Port access type. */
    int            address_width;    /**< Address width: clog2(depth). */
    IR_MemWriteMode write_mode;     /**< Write-port read-during-write behavior. */

    int            addr_signal_id;    /**< Address signal ID (-1 when unbound). */
    int            data_in_signal_id; /**< Write data signal ID (-1 when unbound; WRITE only). */
    int            data_out_signal_id;/**< Read data signal ID (-1 when unbound; READ only). */
    int            enable_signal_id;  /**< Write-enable signal ID (-1 when unbound; WRITE only). */
    int            wdata_signal_id;   /**< .wdata signal ID for INOUT ports (-1 otherwise). */

    int            output_signal_id;  /**< Legacy: sync read output signal (-1 otherwise). */

    int            addr_reg_signal_id;   /**< Synthetic address register signal ID (-1 when not created). */
    int            sync_clock_domain_id; /**< Clock domain for SYNC read address capture (-1 when unbound). */
} IR_MemoryPort;

/**
 * @struct IR_Memory
 * @brief A memory block with dimensions, initialization, and ports.
 */
typedef struct IR_Memory {
    int   id;              /**< Unique within owning module. */
    char *name;            /**< Memory name. */

    IR_MemoryKind kind;    /**< Implementation kind (distributed/block). */

    int   word_width;      /**< Bits per word. */
    int   depth;           /**< Number of entries. */
    int   address_width;   /**< Address width: clog2(depth). */

    union {
        IR_Literal literal;    /**< Constant initialization value. */
        char      *file_path;  /**< \@file("...") initialization path. */
    } init; /**< Initialization data. */
    bool init_is_file;         /**< True if init.file_path is active. */

    IR_MemoryPort *ports;      /**< Array of memory ports. */
    int            num_ports;  /**< Number of ports. */
} IR_Memory;

/**
 * @struct IR_InstanceConnection
 * @brief A single port binding on a module instance.
 */
typedef struct IR_InstanceConnection {
    int   parent_signal_id; /**< Signal in parent module. */
    int   child_port_id;    /**< Port signal ID in child module. */
    int   parent_msb;       /**< Inclusive MSB of parent slice, or -1 for full. */
    int   parent_lsb;       /**< Inclusive LSB of parent slice, or -1 for full. */
    char *const_expr;       /**< Literal expression when bound to a constant. */
} IR_InstanceConnection;

/**
 * @struct IR_Instance
 * @brief A module instantiation within a parent module.
 */
/**
 * @struct IR_InstanceParam
 * @brief A parameter override for a blackbox instance.
 */
typedef struct IR_InstanceParam {
    char *name;          /**< Parameter name (e.g., "CLK_FRE"). */
    long long value;     /**< Evaluated integer value. */
    char *string_value;  /**< String value, or NULL for integer params. */
} IR_InstanceParam;

typedef struct IR_Instance {
    int                   id;               /**< Unique within owning module. */
    char                 *name;             /**< Instance name. */
    int                   child_module_id;  /**< Specialized child module ID. */
    IR_InstanceConnection *connections;     /**< Port binding array. */
    int                   num_connections;  /**< Number of port bindings. */
    IR_InstanceParam     *params;           /**< Blackbox parameter overrides. */
    int                   num_params;       /**< Number of parameter overrides. */
} IR_Instance;

/**
 * @enum IR_CDCType
 * @brief Clock domain crossing type.
 */
typedef enum IR_CDCType {
    CDC_BIT,        /**< Single-bit CDC synchronizer. */
    CDC_BUS,        /**< Multi-bit bus CDC. */
    CDC_FIFO,       /**< FIFO-based CDC. */
    CDC_HANDSHAKE,  /**< Req/ack handshake protocol for multi-bit transfers. */
    CDC_PULSE,      /**< Toggle-based pulse synchronizer (width-1 only). */
    CDC_MCP,        /**< Multi-cycle path with synchronized enable. */
    CDC_RAW         /**< Direct unsynchronized view (wire alias, no crossing logic). */
} IR_CDCType;

/**
 * @struct IR_CDC
 * @brief A clock domain crossing declaration.
 */
typedef struct IR_CDC {
    int       id;               /**< Unique within owning module. */
    int       source_reg_id;    /**< Source register signal ID. */
    int       source_msb;       /**< Bit-select MSB (-1 = whole register). */
    int       source_lsb;       /**< Bit-select LSB (-1 = whole register). */
    int       source_clock_id;  /**< Source clock domain ID. */
    char     *dest_alias_name;  /**< Name in destination domain. */
    int       dest_clock_id;    /**< Destination clock domain ID. */
    IR_CDCType type;            /**< CDC crossing type. */
} IR_CDC;

/**
 * @struct IR_PortAliasGroup
 * @brief A group of PORT signals that are aliased together in a module.
 *
 * When a module's ASYNCHRONOUS block contains aliases like:
 *   src.DATA = data;
 *   tgt[0].DATA = data;
 *   tgt[1].DATA = data;
 * then all the PORT signals involved form an alias group.  The tristate
 * transform uses these groups to merge shared nets across bus interconnects.
 */
typedef struct IR_PortAliasGroup {
    int *port_ids;  /**< Array of PORT signal IDs in this group. */
    int  count;     /**< Number of port signals in the group. */
} IR_PortAliasGroup;

/**
 * @struct IR_Module
 * @brief A hardware module with signals, clock domains, instances, and memories.
 */
typedef struct IR_Module {
    int   id;                /**< Unique within design. */
    char *name;              /**< Module name (may include specialization suffix). */
    int   base_module_id;    /**< Base module ID (-1 for generic modules). */

    IR_Signal      *signals;          /**< Array of signals. */
    int             num_signals;      /**< Number of signals. */

    IR_ClockDomain *clock_domains;    /**< Array of clock domains. */
    int             num_clock_domains;/**< Number of clock domains. */

    IR_Instance    *instances;        /**< Array of module instances. */
    int             num_instances;    /**< Number of instances. */

    IR_Memory      *memories;         /**< Array of memories. */
    int             num_memories;     /**< Number of memories. */

    IR_CDC         *cdc_crossings;    /**< Array of CDC crossings. */
    int             num_cdc_crossings;/**< Number of CDC crossings. */

    IR_Stmt        *async_block;      /**< ASYNCHRONOUS block root (may be NULL). */

    IR_PortAliasGroup *port_alias_groups;    /**< Groups of aliased PORT signals (may be NULL). */
    int                num_port_alias_groups; /**< Number of port alias groups. */

    int             source_file_id;   /**< Index into IR_Design.source_files (-1 if unknown). */
    int             source_line;      /**< Module declaration line. */
    bool            eliminated;       /**< True if unreachable from @top (dead module). */
    bool            is_blackbox;      /**< True if module originated from @blackbox declaration. */
} IR_Module;

/**
 * @struct IR_Clock
 * @brief Project-level clock definition.
 */
typedef struct IR_Clock {
    char        *name;        /**< Clock name. */
    double       period_ns;   /**< Clock period in nanoseconds (may be fractional). */
    double       phase_deg;   /**< Phase offset in degrees (0 for external/non-phase clocks). */
    IR_ClockEdge edge;        /**< Active edge. */
    bool         is_generated;/**< True for PLL/DLL output clocks (use get_nets in SDC). */
} IR_Clock;

/**
 * @struct IR_ClockGenOutput
 * @brief A single output of a clock generator unit.
 */
typedef struct IR_ClockGenOutput {
    char *selector;    /**< PLL output name (e.g., "BASE", "PHASE", "DIV"). */
    char *clock_name;  /**< Output signal name. */
    int   is_clock;    /**< 1 if clock output (OUT), 0 if wire output (WIRE). */
} IR_ClockGenOutput;

/**
 * @struct IR_ClockGenConfig
 * @brief A configuration parameter for a clock generator.
 */
typedef struct IR_ClockGenConfig {
    char *param_name;  /**< Parameter name. */
    char *param_value; /**< Parameter value. */
} IR_ClockGenConfig;

/**
 * @struct IR_ClockGenInput
 * @brief A named input of a clock generator unit.
 */
typedef struct IR_ClockGenInput {
    char *selector;     /**< Input name (e.g., "REF_CLK", "CE"). */
    char *signal_name;  /**< Connected signal name, or NULL if using default. */
} IR_ClockGenInput;

/**
 * @struct IR_ClockGenUnit
 * @brief A single PLL or DLL clock generator unit.
 */
typedef struct IR_ClockGenUnit {
    char              *type;         /**< Generator type (lowercase, e.g. "pll", "clkdiv2"). */
    IR_ClockGenInput  *inputs;       /**< Array of named inputs. */
    int                num_inputs;   /**< Number of inputs. */
    IR_ClockGenOutput *outputs;      /**< Array of outputs. */
    int                num_outputs;  /**< Number of outputs. */
    IR_ClockGenConfig *configs;      /**< Array of configuration parameters. */
    int                num_configs;  /**< Number of configuration parameters. */
} IR_ClockGenUnit;

/**
 * @struct IR_ClockGen
 * @brief Clock generation block (uses project-level chip ID).
 */
typedef struct IR_ClockGen {
    IR_ClockGenUnit *units;     /**< Array of generator units. */
    int              num_units; /**< Number of generator units. */
} IR_ClockGen;

/**
 * @enum IR_PinKind
 * @brief Physical pin direction.
 */
typedef enum IR_PinKind {
    PIN_IN,    /**< Input pin. */
    PIN_OUT,   /**< Output pin. */
    PIN_INOUT  /**< Bidirectional pin. */
} IR_PinKind;

/**
 * @enum IR_PinMode
 * @brief Pin signaling mode (single-ended or differential).
 */
typedef enum IR_PinMode {
    PIN_MODE_SINGLE,       /**< Single-ended signaling (default). */
    PIN_MODE_DIFFERENTIAL  /**< Differential signaling (requires P/N pair). */
} IR_PinMode;

/**
 * @enum IR_PullMode
 * @brief Pull resistor configuration.
 */
typedef enum IR_PullMode {
    PULL_NONE,  /**< No pull resistor (default). */
    PULL_UP,    /**< Pull-up resistor. */
    PULL_DOWN   /**< Pull-down resistor. */
} IR_PullMode;

/**
 * @struct IR_Pin
 * @brief A physical pin declaration.
 */
typedef struct IR_Pin {
    int        id;        /**< Unique within project. */
    char      *name;      /**< Pin name. */
    IR_PinKind kind;      /**< Pin direction. */
    int        width;     /**< Pin width in bits. */
    char      *standard;  /**< I/O standard (e.g., "LVCMOS33"). */
    int        drive_ma;  /**< Drive strength in mA (-1 if N/A). */
    double     drive;     /**< Drive strength (fractional, e.g. 3.5 for LVDS). */
    IR_PinMode mode;      /**< Signaling mode (SINGLE or DIFFERENTIAL). */
    int        term;      /**< Termination: 0=OFF, 1=ON. */
    IR_PullMode pull;     /**< Pull resistor mode. */
    char      *fclk_name; /**< Fast (serializer) clock name; NULL if none. */
    char      *pclk_name; /**< Parallel data clock name; NULL if none. */
    char      *reset_name; /**< Reset signal name for serializer; NULL if none. */
} IR_Pin;

/**
 * @struct IR_PinMapping
 * @brief Logical-to-physical pin mapping.
 */
typedef struct IR_PinMapping {
    char *logical_pin_name; /**< Logical pin name. */
    int   bit_index;        /**< Bit index within bus (-1 if scalar). */
    char *board_pin_id;     /**< Board/package pin identifier (P pin for differential). */
    char *board_pin_n_id;   /**< N pin identifier for differential; NULL for single-ended. */
} IR_PinMapping;

/**
 * @struct IR_TopBinding
 * @brief Top-level binding between a top-module port and a physical pin bit.
 *
 * For bus ports and bus pins, bindings are expanded per-bit so that tools
 * can unambiguously relate individual top-port bits to individual board pins.
 */
typedef struct IR_TopBinding {
    int   top_port_signal_id; /**< Signal ID in top module (SIG_PORT). */
    int   top_bit_index;      /**< Bit index within top port, or -1 for scalar. */
    int   pin_id;             /**< Index into IR_Project.pins[], or -1 for no-connect. */
    int   pin_bit_index;      /**< Bit index within pin bus, or -1 for scalar. */
    int   const_value;        /**< Constant value (0 or 1) when pin_id < 0. */
    int   inverted;           /**< Non-zero for inverted binding (e.g., ~btns[0]). */
    char *clock_name;         /**< Internal clock wire name when pin_id < 0 (may be NULL). */
} IR_TopBinding;

/**
 * @struct IR_Project
 * @brief Project-level IR containing clocks, pins, mappings, and top binding.
 */
typedef struct IR_Project {
    char *name;                  /**< Project name. */
    char *chip_id;               /**< Project-level CHIP identifier (e.g., "GW1NR-9-QN88-C6-I5"). */

    IR_Clock      *clocks;       /**< Array of clock definitions. */
    int            num_clocks;   /**< Number of clocks. */

    IR_ClockGen   *clock_gens;   /**< Array of clock generators. */
    int            num_clock_gens; /**< Number of clock generators. */

    IR_Pin        *pins;         /**< Array of pin declarations. */
    int            num_pins;     /**< Number of pins. */

    IR_PinMapping *mappings;     /**< Array of pin mappings. */
    int            num_mappings; /**< Number of pin mappings. */

    int            top_module_id;    /**< Index into design modules array. */

    IR_TopBinding *top_bindings;     /**< Top port to pin/clock bindings. */
    int            num_top_bindings; /**< Number of top bindings. */
} IR_Project;

/**
 * @struct IR_SourceFile
 * @brief Metadata for a source file referenced by the design.
 */
typedef struct IR_SourceFile {
    char *path;       /**< Source file path. */
    int   line_count; /**< Number of lines in the file. */
} IR_SourceFile;

/**
 * @enum IR_TristateDefault
 * @brief Tri-state default replacement mode for FPGA targets.
 */
typedef enum IR_TristateDefault {
    TRISTATE_DEFAULT_NONE = 0,  /**< Allow z (ASIC/sim mode). */
    TRISTATE_DEFAULT_GND,       /**< Replace z with all-0. */
    TRISTATE_DEFAULT_VCC        /**< Replace z with all-1. */
} IR_TristateDefault;

/**
 * @struct IR_Design
 * @brief Top-level design root containing all modules and project data.
 */
typedef struct IR_Design {
    char *name;                    /**< Project name (may be NULL). */

    IR_Module    *modules;         /**< Array of all modules (generic + specializations). */
    int           num_modules;     /**< Number of modules. */

    IR_Project   *project;         /**< Project data (NULL if no \@project block). */

    IR_SourceFile *source_files;   /**< Array of source file metadata. */
    int            num_source_files; /**< Number of source files. */

    IR_TristateDefault tristate_default; /**< Tri-state default mode (set by CLI). */
} IR_Design;

#endif /* JZ_HDL_IR_H */
