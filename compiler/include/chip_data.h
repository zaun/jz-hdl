/**
 * @file chip_data.h
 * @brief Chip-specific data loading and querying.
 *
 * Provides functions to load and query chip-specific configuration data
 * (memory configurations, clock generator templates) from embedded JSON
 * files or external overrides. Used by the backend to emit chip-aware
 * Verilog and constraints.
 */

#ifndef JZ_HDL_CHIP_DATA_H
#define JZ_HDL_CHIP_DATA_H

#include <stdio.h>
#include "util.h"

/**
 * @enum JZChipMemType
 * @brief Memory implementation type for a chip.
 */
typedef enum JZChipMemType {
    JZ_CHIP_MEM_UNKNOWN = 0, /**< Unknown memory type. */
    JZ_CHIP_MEM_DISTRIBUTED, /**< Distributed (LUT-based) memory. */
    JZ_CHIP_MEM_BLOCK,       /**< Block RAM. */
    JZ_CHIP_MEM_SDRAM,       /**< On-chip SDRAM. */
    JZ_CHIP_MEM_FLASH,       /**< On-chip Flash. */
    JZ_CHIP_MEM_SPRAM        /**< Single-Port RAM (iCE40). */
} JZChipMemType;

/**
 * @struct JZChipMemConfig
 * @brief A single memory configuration supported by the chip.
 */
typedef struct JZChipMemConfig {
    JZChipMemType type;    /**< Memory type. */
    unsigned      r_ports; /**< Number of read ports. */
    unsigned      w_ports; /**< Number of write ports. */
    unsigned      width;   /**< Maximum word width. */
    unsigned      depth;   /**< Maximum depth (entries). */
} JZChipMemConfig;

/**
 * @struct JZChipClockGenMap
 * @brief Backend-specific template for clock generator instantiation.
 */
typedef struct JZChipClockGenMap {
    char *backend;       /**< Backend name (e.g., "verilog-2005"). */
    char *template_text; /**< Template with placeholders for instantiation. */
} JZChipClockGenMap;

/**
 * @struct JZChipClockGenDerived
 * @brief A derived parameter for clock generator configuration.
 */
typedef struct JZChipClockGenDerived {
    char *name; /**< Derived parameter name (e.g., "PSDA_SEL"). */
    char *expr; /**< Expression to compute the value (e.g., "toString(PHASESEL, BIN, 4)"). */
    int has_min;  /**< Non-zero if min constraint exists. */
    double min;   /**< Minimum valid value for this derived parameter. */
    int has_max;  /**< Non-zero if max constraint exists. */
    double max;   /**< Maximum valid value for this derived parameter. */
} JZChipClockGenDerived;

/**
 * @struct JZChipClockGenParam
 * @brief A clock generator parameter with its default value.
 */
typedef struct JZChipClockGenParam {
    char *name;          /**< Parameter name (e.g., "IDIV"). */
    char *default_value; /**< Default value as a string (e.g., "1"). */
    int has_min;         /**< Non-zero if min constraint exists. */
    long min;            /**< Minimum valid value for this parameter. */
    int has_max;         /**< Non-zero if max constraint exists. */
    long max;            /**< Maximum valid value for this parameter. */
    char **valid_values; /**< Array of valid string values (NULL if using min/max). */
    size_t valid_count;  /**< Number of entries in valid_values. */
} JZChipClockGenParam;

/**
 * @struct JZChipClockGenOutput
 * @brief An output of a clock generator with its frequency expression.
 */
typedef struct JZChipClockGenOutput {
    char *selector;       /**< Output selector (e.g., "BASE", "PHASE", "DIV", "DIV3"). */
    char *frequency_expr; /**< Expression for frequency in MHz (e.g., "FVCO / ODIV"). */
    char *phase_deg_expr; /**< Expression for phase offset in degrees (e.g., "PHASESEL * 45"), or NULL. */
} JZChipClockGenOutput;

/**
 * @struct JZChipClockGenInput
 * @brief A named input of a clock generator (e.g., REF_CLK, CE).
 */
typedef struct JZChipClockGenInput {
    char   *name;            /**< Input name (e.g., "REF_CLK", "CE"). */
    char   *default_value;   /**< Default value (e.g., "1'b1"), or NULL if required. */
    int     required;        /**< Non-zero if this input must be provided. */
    int     requires_period; /**< Non-zero if the connected clock must have a period. */
    int     has_min_mhz;     /**< Non-zero if min frequency constraint exists. */
    double  min_mhz;         /**< Minimum frequency in MHz. */
    int     has_max_mhz;     /**< Non-zero if max frequency constraint exists. */
    double  max_mhz;         /**< Maximum frequency in MHz. */
} JZChipClockGenInput;

/**
 * @struct JZChipClockGen
 * @brief Clock generator definition for a chip (PLL or DLL).
 */
typedef struct JZChipClockGen {
    char     *type;     /**< Generator type ("pll", "dll", or "clkdiv"). */
    char     *mode;     /**< Optional mode variant (e.g., "local", "global"). NULL if not specified. */
    JZBuffer  maps;     /**< Array of JZChipClockGenMap entries. */
    JZBuffer  deriveds; /**< Array of JZChipClockGenDerived entries. */
    JZBuffer  params;   /**< Array of JZChipClockGenParam entries. */
    JZBuffer  outputs;  /**< Array of JZChipClockGenOutput entries. */
    JZBuffer  inputs;   /**< Array of JZChipClockGenInput entries. */
    int       has_refclk_range; /**< Non-zero if FCLKIN min/max are defined. */
    double    refclk_min_mhz;   /**< Minimum reference clock frequency in MHz. */
    double    refclk_max_mhz;   /**< Maximum reference clock frequency in MHz. */
    int       count;            /**< Number of this generator type available on the chip. */
    int       has_chaining;     /**< Non-zero if chaining field was specified. */
    int       chaining;         /**< Non-zero if generators can be chained. */
    JZBuffer  constraints;      /**< Array of char* constraint rule strings. */
} JZChipClockGen;

/**
 * @struct JZChipDiffMap
 * @brief Backend-specific template for a differential I/O primitive.
 */
typedef struct JZChipDiffMap {
    char *backend;       /**< Backend name (e.g., "verilog-2005"). */
    char *template_text; /**< Template with placeholders. */
} JZChipDiffMap;

/**
 * @struct JZChipDiffPrimitive
 * @brief A differential I/O primitive (buffer or serializer).
 */
typedef struct JZChipDiffPrimitive {
    int       ratio;     /**< Serializer ratio (e.g., 10 for OSER10); 0 for buffers. */
    JZBuffer  maps;      /**< Array of JZChipDiffMap entries. */
} JZChipDiffPrimitive;

/**
 * @struct JZChipDifferential
 * @brief Differential I/O support data for a chip.
 */
typedef struct JZChipDifferential {
    JZChipDiffPrimitive output_buffer;     /**< Differential output buffer (e.g., ELVDS_OBUF). */
    JZChipDiffPrimitive output_serializer; /**< Output serializer (e.g., OSER10). */
    JZChipDiffPrimitive input_buffer;        /**< Differential input buffer (e.g., ELVDS_IBUF). */
    JZChipDiffPrimitive input_deserializer; /**< Input deserializer (e.g., IDES10). */
    int has_output_buffer;       /**< Non-zero if output buffer is defined. */
    int has_output_serializer;   /**< Non-zero if output serializer is defined. */
    int has_input_buffer;        /**< Non-zero if input buffer is defined. */
    int has_input_deserializer;  /**< Non-zero if input deserializer is defined. */
    char *io_type;               /**< CST IO_TYPE for differential pins (e.g., "LVDS25", "LVCMOS33D"). */
    char *diff_type;             /**< Differential type ("true" or "emulated"). */
} JZChipDifferential;

/**
 * @struct JZChipLatchSupport
 * @brief Latch type support per resource block.
 */
typedef struct JZChipLatchSupport {
    int cfu_d;   /**< CFU supports D latches. */
    int cfu_sr;  /**< CFU supports SR latches. */
    int iob_d;   /**< IOB supports D latches. */
    int iob_sr;  /**< IOB supports SR latches. */
} JZChipLatchSupport;

/**
 * @struct JZChipMemResource
 * @brief Aggregate memory resource limits for a chip.
 */
typedef struct JZChipMemResource {
    JZChipMemType type;          /**< Memory type (BLOCK, DISTRIBUTED, etc.). */
    unsigned      quantity;      /**< Number of physical blocks (BLOCK only). */
    unsigned      bits_per_block;/**< Bits per block (BLOCK only). */
    unsigned      total_bits;    /**< Total bits available for this type. */
} JZChipMemResource;

/**
 * @struct JZChipData
 * @brief Loaded chip-specific data for a target device.
 */
typedef struct JZChipData {
    char              *chip_id;       /**< Normalized chip ID string. */
    JZBuffer           mem_configs;   /**< Array of JZChipMemConfig entries. */
    JZBuffer           mem_resources; /**< Array of JZChipMemResource entries. */
    JZBuffer           clock_gens;    /**< Array of JZChipClockGen entries. */
    JZChipDifferential differential;  /**< Differential I/O data. */
    JZChipLatchSupport latches;       /**< Latch type support data. */
    int                has_latches;   /**< Non-zero if latch data was loaded. */
} JZChipData;

/**
 * @enum JZChipLoadStatus
 * @brief Result status from jz_chip_data_load().
 */
typedef enum JZChipLoadStatus {
    JZ_CHIP_LOAD_OK = 0,    /**< Chip data loaded successfully. */
    JZ_CHIP_LOAD_GENERIC,   /**< Chip is GENERIC; no data loaded. */
    JZ_CHIP_LOAD_NOT_FOUND, /**< Chip data file not found. */
    JZ_CHIP_LOAD_JSON_ERROR /**< JSON parse error in chip data file. */
} JZChipLoadStatus;

/**
 * @brief Load chip data for a given chip ID.
 *
 * If chip_id is NULL or resolves to "GENERIC" (case-insensitive),
 * returns JZ_CHIP_LOAD_GENERIC and leaves out empty. Otherwise,
 * searches for <chip_id>.json near project_filename or in built-in data.
 *
 * @param chip_id          Target chip identifier (may be NULL).
 * @param project_filename Project file path for relative JSON lookup (may be NULL).
 * @param out              Receives loaded chip data on success.
 * @return Load status code.
 */
JZChipLoadStatus jz_chip_data_load(const char *chip_id,
                                   const char *project_filename,
                                   JZChipData *out);

/**
 * @brief Free all resources owned by a JZChipData structure.
 * @param data Pointer to the chip data to free. Must not be NULL.
 */
void jz_chip_data_free(JZChipData *data);

/**
 * @brief Get the display name for a chip memory type.
 * @param type Memory type enum value.
 * @return Static string name (e.g., "BLOCK", "DISTRIBUTED").
 */
const char *jz_chip_mem_type_name(JZChipMemType type);

/**
 * @brief Get the number of built-in chip data entries.
 * @return Count of embedded chip definitions.
 */
size_t jz_chip_builtin_count(void);

/**
 * @brief Get the chip ID of a built-in chip data entry by index.
 * @param index Index into the built-in chip table (0-based).
 * @return Static chip ID string.
 */
const char *jz_chip_builtin_id(size_t index);

/**
 * @brief Print chip information to an output stream.
 * @param chip_id Target chip identifier.
 * @param out     Output stream (e.g., stdout).
 * @return 0 on success, non-zero if chip not found.
 */
int jz_chip_print_info(const char *chip_id, FILE *out);

/**
 * @brief Get the clock generator template for a given type and backend.
 * @param data    Loaded chip data.
 * @param type    Generator type (e.g., "pll").
 * @param backend Backend name (e.g., "verilog-2005").
 * @return Template string, or NULL if not found.
 */
const char *jz_chip_clock_gen_map(const JZChipData *data,
                                   const char *type,
                                   const char *backend);

/**
 * @brief Get a derived expression for a clock generator type and parameter.
 * @param data         Loaded chip data.
 * @param type         Generator type (e.g., "pll").
 * @param derived_name Derived parameter name (e.g., "PSDA_SEL").
 * @return Expression string, or NULL if not found.
 */
const char *jz_chip_clock_gen_derived_expr(const JZChipData *data,
                                            const char *type,
                                            const char *derived_name);

/**
 * @brief Get the default value for a clock generator parameter.
 * @param data       Loaded chip data.
 * @param type       Generator type (e.g., "pll").
 * @param param_name Parameter name (e.g., "IDIV").
 * @return Default value string, or NULL if not found.
 */
const char *jz_chip_clock_gen_param_default(const JZChipData *data,
                                             const char *type,
                                             const char *param_name);

/**
 * @brief Get the min/max range for a clock generator parameter.
 * @param data       Loaded chip data.
 * @param type       Generator type (e.g., "pll").
 * @param param_name Parameter name (e.g., "IDIV").
 * @param out_min    Receives minimum value if exists.
 * @param out_max    Receives maximum value if exists.
 * @return 1 if both min and max exist, 0 otherwise.
 */
int jz_chip_clock_gen_param_range(const JZChipData *data,
                                   const char *type,
                                   const char *param_name,
                                   long *out_min, long *out_max);

/**
 * @brief Get the min/max range for a clock generator derived value.
 * @param data         Loaded chip data.
 * @param type         Generator type (e.g., "pll").
 * @param derived_name Derived parameter name (e.g., "VCO").
 * @param out_min      Receives minimum value if exists.
 * @param out_max      Receives maximum value if exists.
 * @return 1 if both min and max exist, 0 otherwise.
 */
int jz_chip_clock_gen_derived_range(const JZChipData *data,
                                     const char *type,
                                     const char *derived_name,
                                     double *out_min, double *out_max);

/**
 * @brief Get the count of derived entries for a clock generator type.
 * @param data Loaded chip data.
 * @param type Generator type (e.g., "pll").
 * @return Number of derived entries, or 0 if not found.
 */
size_t jz_chip_clock_gen_derived_count(const JZChipData *data, const char *type);

/**
 * @brief Get the i-th derived entry for a clock generator type.
 * @param data  Loaded chip data.
 * @param type  Generator type (e.g., "pll").
 * @param index 0-based index.
 * @return Pointer to the derived entry, or NULL if out of bounds.
 */
const JZChipClockGenDerived *jz_chip_clock_gen_derived_at(
    const JZChipData *data, const char *type, size_t index);

/**
 * @brief Get the count of parameter entries for a clock generator type.
 * @param data Loaded chip data.
 * @param type Generator type (e.g., "pll").
 * @return Number of parameter entries, or 0 if not found.
 */
size_t jz_chip_clock_gen_param_count(const JZChipData *data, const char *type);

/**
 * @brief Get the i-th parameter entry for a clock generator type.
 * @param data  Loaded chip data.
 * @param type  Generator type (e.g., "pll").
 * @param index 0-based index.
 * @return Pointer to the parameter entry, or NULL if out of bounds.
 */
const JZChipClockGenParam *jz_chip_clock_gen_param_at(
    const JZChipData *data, const char *type, size_t index);

/**
 * @brief Get the frequency expression for a clock generator output.
 * @param data     Loaded chip data.
 * @param type     Generator type (e.g., "pll").
 * @param selector Output selector (e.g., "BASE", "PHASE", "DIV", "DIV3").
 * @return Frequency expression string (in MHz), or NULL if not found.
 */
const char *jz_chip_clock_gen_output_freq_expr(const JZChipData *data,
                                                const char *type,
                                                const char *selector);

/**
 * @brief Get the phase offset expression for a clock generator output.
 * @return Phase expression in degrees (e.g., "PHASESEL * 45"), or NULL.
 */
const char *jz_chip_clock_gen_output_phase_expr(const JZChipData *data,
                                                 const char *type,
                                                 const char *selector);

/**
 * @brief Get the FCLKIN frequency range for a clock generator type.
 * @param data    Loaded chip data.
 * @param type    Generator type (e.g., "pll").
 * @param out_min Receives minimum frequency in MHz.
 * @param out_max Receives maximum frequency in MHz.
 * @return 1 if range is defined, 0 otherwise.
 */
int jz_chip_clock_gen_refclk_range(const JZChipData *data,
                                    const char *type,
                                    double *out_min, double *out_max);

/**
 * @brief Get the default value for a clock generator input.
 * @param data       Loaded chip data.
 * @param type       Generator type (e.g., "pll").
 * @param input_name Input name (e.g., "REF_CLK", "CE").
 * @return Default value string, or NULL if required or not found.
 */
const char *jz_chip_clock_gen_input_default(const JZChipData *data,
                                             const char *type,
                                             const char *input_name);

/**
 * @brief Get a clock generator input definition.
 * @param data       Loaded chip data.
 * @param type       Generator type (e.g., "pll").
 * @param input_name Input name (e.g., "REF_CLK", "CE").
 * @return Pointer to input definition, or NULL if not found.
 */
const JZChipClockGenInput *jz_chip_clock_gen_input(const JZChipData *data,
                                                     const char *type,
                                                     const char *input_name);

/**
 * @brief Get the count of input entries for a clock generator type.
 * @param data Loaded chip data.
 * @param type Generator type (e.g., "pll").
 * @return Number of input entries, or 0 if not found.
 */
size_t jz_chip_clock_gen_input_count(const JZChipData *data, const char *type);

/**
 * @brief Get the i-th input entry for a clock generator type.
 * @param data  Loaded chip data.
 * @param type  Generator type (e.g., "pll").
 * @param index 0-based index.
 * @return Pointer to the input entry, or NULL if out of bounds.
 */
const JZChipClockGenInput *jz_chip_clock_gen_input_at(
    const JZChipData *data, const char *type, size_t index);

/**
 * @brief Get the differential output buffer template for a backend.
 * @param data    Loaded chip data.
 * @param backend Backend name (e.g., "verilog-2005").
 * @return Template string, or NULL if not available.
 */
const char *jz_chip_diff_output_buffer_map(const JZChipData *data,
                                            const char *backend);

/**
 * @brief Get the differential output serializer template for a backend.
 * @param data    Loaded chip data.
 * @param backend Backend name (e.g., "verilog-2005").
 * @return Template string, or NULL if not available.
 */
const char *jz_chip_diff_output_serializer_map(const JZChipData *data,
                                                const char *backend);

/**
 * @brief Get the differential input buffer template for a backend.
 * @param data    Loaded chip data.
 * @param backend Backend name (e.g., "verilog-2005").
 * @return Template string, or NULL if not available.
 */
const char *jz_chip_diff_input_buffer_map(const JZChipData *data,
                                           const char *backend);

/**
 * @brief Get the serializer ratio for differential output.
 * @param data Loaded chip data.
 * @return Serializer ratio (e.g., 10), or 0 if no serializer defined.
 */
int jz_chip_diff_serializer_ratio(const JZChipData *data);

/**
 * @brief Get the differential input deserializer template for a backend.
 * @param data    Loaded chip data.
 * @param backend Backend name (e.g., "verilog-2005").
 * @return Template string, or NULL if not available.
 */
const char *jz_chip_diff_input_deserializer_map(const JZChipData *data,
                                                  const char *backend);

/**
 * @brief Get the deserializer ratio for differential input.
 * @param data Loaded chip data.
 * @return Deserializer ratio (e.g., 10), or 0 if no deserializer defined.
 */
int jz_chip_diff_deserializer_ratio(const JZChipData *data);

/**
 * @brief Get the CST IO_TYPE override for differential pins.
 *
 * Returns the chip-specific IO_TYPE to use in CST constraints for
 * differential pins (e.g., "LVDS25" for true LVDS, "LVCMOS33D" for
 * emulated LVDS). When non-NULL, this overrides the user-specified
 * standard in the project file for CST emission.
 *
 * @param data Loaded chip data.
 * @return IO_TYPE string, or NULL if not specified.
 */
const char *jz_chip_diff_io_type(const JZChipData *data);

/**
 * @brief Get the differential I/O type classification.
 *
 * Returns "true" for native LVDS or "emulated" for LVCMOS-based
 * differential signalling.
 *
 * @param data Loaded chip data.
 * @return Type string, or NULL if not specified.
 */
const char *jz_chip_diff_type(const JZChipData *data);

/**
 * @brief Get the count of a clock generator type available on the chip.
 * @param data Loaded chip data.
 * @param type Generator type (e.g., "pll").
 * @return Number available, or 0 if not found.
 */
int jz_chip_clock_gen_count(const JZChipData *data, const char *type);

/**
 * @brief Check if clock generator chaining is supported.
 * @param data Loaded chip data.
 * @param type Generator type (e.g., "pll").
 * @param out_chaining Receives chaining support flag.
 * @return 1 if chaining field was specified, 0 otherwise.
 */
int jz_chip_clock_gen_chaining(const JZChipData *data, const char *type,
                                int *out_chaining);

/**
 * @brief Get the number of constraint rules for a clock generator type.
 * @param data Loaded chip data.
 * @param type Generator type (e.g., "pll").
 * @return Number of constraint rules, or 0 if not found.
 */
size_t jz_chip_clock_gen_constraint_count(const JZChipData *data, const char *type);

/**
 * @brief Get the i-th constraint rule string for a clock generator type.
 * @param data  Loaded chip data.
 * @param type  Generator type (e.g., "pll").
 * @param index 0-based index.
 * @return Constraint rule string, or NULL if out of bounds.
 */
const char *jz_chip_clock_gen_constraint_at(const JZChipData *data,
                                             const char *type, size_t index);

/**
 * @brief Get the quantity of physical memory blocks for a given type.
 * @param data Loaded chip data.
 * @param type Memory type (e.g., JZ_CHIP_MEM_BLOCK).
 * @return Number of blocks, or 0 if not found.
 */
unsigned jz_chip_mem_quantity(const JZChipData *data, JZChipMemType type);

/**
 * @brief Get the total bits available for a given memory type.
 * @param data Loaded chip data.
 * @param type Memory type (e.g., JZ_CHIP_MEM_BLOCK, JZ_CHIP_MEM_DISTRIBUTED).
 * @return Total bits, or 0 if not found.
 */
unsigned jz_chip_mem_total_bits(const JZChipData *data, JZChipMemType type);

#endif /* JZ_HDL_CHIP_DATA_H */
