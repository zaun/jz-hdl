# Test Plan: 6.9 Top-Level Module Instantiation (@top)

**Specification Reference:** Section 6.9 of jz-hdl-specification.md

## 1. Objective

Verify @top directive: module/blackbox reference resolution, port-to-pin binding, no-connect (`_`) with explicit width, width/direction matching between ports and pins, literal binding prohibition on OUT ports, and logical-to-physical expansion for differential pins.

## 2. Test Scenarios

### 2.1 Happy Path

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | All ports bound to pins | @top with every port connected to a declared pin |
| 2 | No-connect on output port | `OUT [8] debug = _;` -- intentional non-connection with width |
| 3 | No-connect on input port | `IN [8] reserved = _;` -- unused input with width |
| 4 | Differential pin binding | `OUT [1] tmds_c = TMDS_CLK;` where TMDS_CLK is diff mode |
| 5 | Bitwise pin expression | `IN [2] buttons = { btn1, ~btn1 };` |
| 6 | INOUT port bound to INOUT_PINS | Bidirectional port to bidirectional pin |

### 2.2 Error Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Undefined module in @top | @top references module that does not exist |
| 2 | Missing @top in project | No @top directive in @project |
| 3 | Port omitted from @top | Module port not listed in @top block |
| 4 | Port width mismatch | @top port width differs from module port width |
| 5 | Pin declaration missing | Connected port has no IN_PINS/OUT_PINS/INOUT_PINS/CLOCKS entry |
| 6 | Direction mismatch | Module IN port bound to OUT_PINS |
| 7 | Literal on OUT port | OUT port bound to literal value |
| 8 | No-connect without width | Port bound to `_` but missing explicit width |

### 2.3 Edge Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Top module with only IN ports | All ports are inputs |
| 2 | Top module with INOUT ports | Bidirectional ports bound to INOUT_PINS |
| 3 | @top referencing a @blackbox | Top-level module is a blackbox |
| 4 | IN port bound to INOUT_PINS | Module IN connected to bidirectional pin (valid subset) |
| 5 | OUT port bound to INOUT_PINS | Module OUT connected to bidirectional pin (valid subset) |

## 3. Input/Output Matrix

| # | Scenario | Triggering Construct | Expected Rule ID | Severity |
|---|----------|---------------------|------------------|----------|
| 1 | Undefined module in @top | `@top nonexistent { ... }` | INSTANCE_UNDEFINED_MODULE | error |
| 2 | No @top in project | @project with no @top directive | PROJECT_MISSING_TOP_MODULE | error |
| 3 | Port omitted from @top | Module has `IN [1] clk` but @top omits it | TOP_PORT_NOT_LISTED | error |
| 4 | Port width mismatch | `@top m { IN [4] data = pin; }` but module says `[8]` | TOP_PORT_WIDTH_MISMATCH | error |
| 5 | No pin declaration for port | `IN [1] sig = undeclared_pin` | TOP_PORT_PIN_DECL_MISSING | error |
| 6 | Direction mismatch | `IN [1] clk = out_pin` where out_pin is OUT_PINS | TOP_PORT_PIN_DIRECTION_MISMATCH | error |
| 7 | Literal on OUT port | `OUT [8] data = 8'hFF;` | TOP_OUT_LITERAL_BINDING | error |
| 8 | No-connect without width | `OUT data = _;` missing width bracket | TOP_NO_CONNECT_WITHOUT_WIDTH | error |

## 4. Existing Validation Tests

| Test File | Rule ID | Description |
|-----------|---------|-------------|
| 6_9_HAPPY_PATH-top_module_ok.jz | -- | Valid @top binding with all ports connected (clean compile) |
| 6_9_INSTANCE_UNDEFINED_MODULE-top_undefined_module.jz | INSTANCE_UNDEFINED_MODULE | @top references non-existent module |
| 6_9_PROJECT_MISSING_TOP_MODULE-no_top_directive.jz | PROJECT_MISSING_TOP_MODULE | Project has no @top directive |
| 6_9_TOP_NO_CONNECT_WITHOUT_WIDTH-missing_width.jz | TOP_NO_CONNECT_WITHOUT_WIDTH | No-connect port missing explicit width |
| 6_9_TOP_OUT_LITERAL_BINDING-literal_on_out.jz | TOP_OUT_LITERAL_BINDING | OUT port bound to literal value |
| 6_9_TOP_PORT_NOT_LISTED-port_omitted.jz | TOP_PORT_NOT_LISTED | Module port omitted from @top block |
| 6_9_TOP_PORT_PIN_DECL_MISSING-no_pin_declaration.jz | TOP_PORT_PIN_DECL_MISSING | Connected port has no pin declaration |
| 6_9_TOP_PORT_PIN_DIRECTION_MISMATCH-wrong_direction.jz | TOP_PORT_PIN_DIRECTION_MISMATCH | Port direction incompatible with pin category |
| 6_9_TOP_PORT_WIDTH_MISMATCH-width_differs.jz | TOP_PORT_WIDTH_MISMATCH | Port width does not match module definition |

## 5. Rules Matrix

### 5.1 Rules Tested

| Rule ID | Severity | Description | Test Case(s) |
|---------|----------|-------------|--------------|
| INSTANCE_UNDEFINED_MODULE | error | S4.13/S6.9 Instantiation references non-existent module | 6_9_INSTANCE_UNDEFINED_MODULE-top_undefined_module.jz |
| PROJECT_MISSING_TOP_MODULE | error | S6.9 Project does not declare a top-level @top module binding | 6_9_PROJECT_MISSING_TOP_MODULE-no_top_directive.jz |
| TOP_PORT_NOT_LISTED | error | S6.9/S6.10 Top module port omitted from project-level @top block | 6_9_TOP_PORT_NOT_LISTED-port_omitted.jz |
| TOP_PORT_WIDTH_MISMATCH | error | S6.9/S6.10 Instantiated top port width does not match module port width | 6_9_TOP_PORT_WIDTH_MISMATCH-width_differs.jz |
| TOP_PORT_PIN_DECL_MISSING | error | S6.9/S6.10 Connected top port has no corresponding pin/clock declaration | 6_9_TOP_PORT_PIN_DECL_MISSING-no_pin_declaration.jz |
| TOP_PORT_PIN_DIRECTION_MISMATCH | error | S6.9/S6.10 Module IN/OUT/INOUT direction incompatible with pin category | 6_9_TOP_PORT_PIN_DIRECTION_MISMATCH-wrong_direction.jz |
| TOP_OUT_LITERAL_BINDING | error | S6.9 OUT ports may not be bound to literal values in project-level @top | 6_9_TOP_OUT_LITERAL_BINDING-literal_on_out.jz |
| TOP_NO_CONNECT_WITHOUT_WIDTH | error | S6.9 Port bound to `_` but missing explicit width in top-level @top list | 6_9_TOP_NO_CONNECT_WITHOUT_WIDTH-missing_width.jz |

### 5.2 Rules Not Tested

| Rule ID | Severity | Reason |
|---------|----------|--------|
| -- | -- | All rules for this section have dedicated validation tests |
