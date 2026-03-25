# Test Plan: 10.4 Template Forbidden Content
**Specification Reference:** Section 10.4 of jz-hdl-specification.md

## 1. Objective
Verify forbidden content inside templates: WIRE, REGISTER, PORT, CONST, MEM, MUX declarations, SYNCHRONOUS/ASYNCHRONOUS block headers, structural directives (@new, @module, @project, @feature, CDC), and nested @template definitions.

## 2. Test Scenarios

### 2.1 Happy Path
1. Template with only assignments and control flow — valid
2. Template with @scratch (allowed alternative to WIRE) — valid

### 2.2 Error Cases
1. WIRE declaration in template — Error (TEMPLATE_FORBIDDEN_DECL)
2. REGISTER declaration in template — Error (TEMPLATE_FORBIDDEN_DECL)
3. PORT declaration in template — Error (TEMPLATE_FORBIDDEN_DECL)
4. SYNCHRONOUS block header in template — Error (TEMPLATE_FORBIDDEN_BLOCK_HEADER)
5. ASYNCHRONOUS block header in template — Error (TEMPLATE_FORBIDDEN_BLOCK_HEADER)
6. @new directive in template — Error (TEMPLATE_FORBIDDEN_DIRECTIVE)
7. @module directive in template — Error (TEMPLATE_FORBIDDEN_DIRECTIVE)
8. @feature directive in template — Error (TEMPLATE_FORBIDDEN_DIRECTIVE)
9. Nested @template definition — Error (TEMPLATE_NESTED_DEF)

### 2.3 Edge Cases
1. MEM declaration in template — Error (TEMPLATE_FORBIDDEN_DECL)
2. MUX declaration in template — Error (TEMPLATE_FORBIDDEN_DECL)
3. CONST declaration in template — Error (TEMPLATE_FORBIDDEN_DECL)

## 3. Input/Output Matrix
| # | Input | Expected Output | Rule ID | Notes |
|---|-------|----------------|---------|-------|
| 1 | Declaration block in template | Error | TEMPLATE_FORBIDDEN_DECL | S10.4 |
| 2 | SYNC/ASYNC header in template | Error | TEMPLATE_FORBIDDEN_BLOCK_HEADER | S10.4 |
| 3 | Structural directive in template | Error | TEMPLATE_FORBIDDEN_DIRECTIVE | S10.4 |
| 4 | Nested @template | Error | TEMPLATE_NESTED_DEF | S10.4 |

## 4. Existing Validation Tests
| Test File | Rule ID | Description |
|-----------|---------|-------------|
| `10_4_HAPPY_PATH-template_forbidden_content_ok.jz` | — | Happy-path: template with only allowed content (assignments, control flow, @scratch) |
| `10_4_TEMPLATE_FORBIDDEN_DECL-declaration_blocks_in_template.jz` | TEMPLATE_FORBIDDEN_DECL | WIRE/REGISTER/etc. inside template body |
| `10_4_TEMPLATE_FORBIDDEN_BLOCK_HEADER-async_file_scoped.jz` | TEMPLATE_FORBIDDEN_BLOCK_HEADER | ASYNCHRONOUS header in file-scoped template |
| `10_4_TEMPLATE_FORBIDDEN_BLOCK_HEADER-async_mod_scoped.jz` | TEMPLATE_FORBIDDEN_BLOCK_HEADER | ASYNCHRONOUS header in module-scoped template |
| `10_4_TEMPLATE_FORBIDDEN_BLOCK_HEADER-cdc_file_scoped.jz` | TEMPLATE_FORBIDDEN_BLOCK_HEADER | CDC header in file-scoped template |
| `10_4_TEMPLATE_FORBIDDEN_BLOCK_HEADER-cdc_mod_scoped.jz` | TEMPLATE_FORBIDDEN_BLOCK_HEADER | CDC header in module-scoped template |
| `10_4_TEMPLATE_FORBIDDEN_BLOCK_HEADER-sync_file_scoped.jz` | TEMPLATE_FORBIDDEN_BLOCK_HEADER | SYNCHRONOUS header in file-scoped template |
| `10_4_TEMPLATE_FORBIDDEN_BLOCK_HEADER-sync_mod_scoped.jz` | TEMPLATE_FORBIDDEN_BLOCK_HEADER | SYNCHRONOUS header in module-scoped template |
| `10_4_TEMPLATE_FORBIDDEN_DIRECTIVE-feature_in_mod_template.jz` | TEMPLATE_FORBIDDEN_DIRECTIVE | @feature in module-scoped template |
| `10_4_TEMPLATE_FORBIDDEN_DIRECTIVE-feature_in_template.jz` | TEMPLATE_FORBIDDEN_DIRECTIVE | @feature in file-scoped template |
| `10_4_TEMPLATE_FORBIDDEN_DIRECTIVE-module_in_mod_template.jz` | TEMPLATE_FORBIDDEN_DIRECTIVE | @module in module-scoped template |
| `10_4_TEMPLATE_FORBIDDEN_DIRECTIVE-module_in_template.jz` | TEMPLATE_FORBIDDEN_DIRECTIVE | @module in file-scoped template |
| `10_4_TEMPLATE_FORBIDDEN_DIRECTIVE-new_in_mod_template.jz` | TEMPLATE_FORBIDDEN_DIRECTIVE | @new in module-scoped template |
| `10_4_TEMPLATE_FORBIDDEN_DIRECTIVE-new_in_template.jz` | TEMPLATE_FORBIDDEN_DIRECTIVE | @new in file-scoped template |
| `10_4_TEMPLATE_FORBIDDEN_DIRECTIVE-project_in_mod_template.jz` | TEMPLATE_FORBIDDEN_DIRECTIVE | @project in module-scoped template |
| `10_4_TEMPLATE_FORBIDDEN_DIRECTIVE-project_in_template.jz` | TEMPLATE_FORBIDDEN_DIRECTIVE | @project in file-scoped template |
| `10_4_TEMPLATE_NESTED_DEF-nested_template_definition.jz` | TEMPLATE_NESTED_DEF | @template defined inside another @template |

## 5. Rules Matrix

### 5.1 Rules Tested
| Rule ID | Description | Test Case(s) |
|---------|-------------|-------------|
| TEMPLATE_FORBIDDEN_DECL | Declaration blocks not allowed inside template body | `10_4_TEMPLATE_FORBIDDEN_DECL-declaration_blocks_in_template.jz` |
| TEMPLATE_FORBIDDEN_BLOCK_HEADER | SYNC/ASYNC block headers not allowed inside template body | `10_4_TEMPLATE_FORBIDDEN_BLOCK_HEADER-async_file_scoped.jz`, `10_4_TEMPLATE_FORBIDDEN_BLOCK_HEADER-async_mod_scoped.jz`, `10_4_TEMPLATE_FORBIDDEN_BLOCK_HEADER-cdc_file_scoped.jz`, `10_4_TEMPLATE_FORBIDDEN_BLOCK_HEADER-cdc_mod_scoped.jz`, `10_4_TEMPLATE_FORBIDDEN_BLOCK_HEADER-sync_file_scoped.jz`, `10_4_TEMPLATE_FORBIDDEN_BLOCK_HEADER-sync_mod_scoped.jz` |
| TEMPLATE_FORBIDDEN_DIRECTIVE | Structural directives not allowed inside template body | `10_4_TEMPLATE_FORBIDDEN_DIRECTIVE-feature_in_mod_template.jz`, `10_4_TEMPLATE_FORBIDDEN_DIRECTIVE-feature_in_template.jz`, `10_4_TEMPLATE_FORBIDDEN_DIRECTIVE-module_in_mod_template.jz`, `10_4_TEMPLATE_FORBIDDEN_DIRECTIVE-module_in_template.jz`, `10_4_TEMPLATE_FORBIDDEN_DIRECTIVE-new_in_mod_template.jz`, `10_4_TEMPLATE_FORBIDDEN_DIRECTIVE-new_in_template.jz`, `10_4_TEMPLATE_FORBIDDEN_DIRECTIVE-project_in_mod_template.jz`, `10_4_TEMPLATE_FORBIDDEN_DIRECTIVE-project_in_template.jz` |
| TEMPLATE_NESTED_DEF | Nested @template definitions not allowed | `10_4_TEMPLATE_NESTED_DEF-nested_template_definition.jz` |

### 5.2 Rules Not Tested
| Expected Rule | Spec Reference | Gap Description |
|--------------|---------------|-----------------|
| — | — | All S10.4 rules covered |
