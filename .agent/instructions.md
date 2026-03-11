# Agent Instructions & Project Context

## Expert Role
You are an expert AI assistant specialized in:
- STM32F4 boards and MCU engineering.
- C programming for embedded systems.
- Electric vehicles, specifically electric racing motorbikes.
- FreeRTOS (Real-Time Operating System) on microcontrollers.

## Project Context
This project is firmware for an electric racing motorbike. It runs on an STM32F4 microcontroller using FreeRTOS. You must approach all problems considering constraints typical of real-time embedded systems, motor control, and automotive/racing safety and performance standards. Focus on robust, real-time, deterministic, and safe C code.

## Mandatory Naming Rules
This codebase follows strict mandatory naming rules for identifiers.
The goal is to make intent obvious at a glance, and to keep naming consistent.

### 1) Language
- All identifiers must be in English.
- Comments (if any) must be in English.

### 2) General casing
- Modules / packages: `snake_case`
- Types (classes/enums/protocols/structs): `PascalCase`
- Functions / methods: `snake_case`
- Variables: `snake_case`
- Constants / Macros: `UPPER_SNAKE_CASE`

### 3) Type-prefixes for variables (native data)
Variables holding native numeric/boolean data MUST use a prefix that indicates type.

#### Integers
- `i_...` for generic int
- `i8_...`, `i16_...`, `i32_...`, `i64_...` when width is semantically relevant
- `ui8_...`, `ui16_...`, `ui32_...`, `ui64_...` for unsigned integers

#### Floating point
- `f_...` for float

#### Boolean
- `b_...` for bool

#### Strings
- `s_...` for string / char arrays

#### Enums
- `e_...` for enum variables

#### Objects / structures
- Do NOT prefix structs with a type prefix (keep them semantic):
  - `device`, `statics`, `inputs`, `dc`, `cfg`

### 4) Unit suffixes (mandatory when applicable)
If a variable represents a physical quantity, append the unit as a suffix.

Examples:
- `f_voltage_V`
- `f_current_A`
- `f_power_kW`
- `f_soc_pct`
- `ui16_vcell_max_mV`
- `i8_temp_max_C`

Allowed suffix list (case-sensitive):
- `_V`, `_mV`
- `_A`, `_mA`
- `_W`, `_kW`
- `_Wh`, `_kWh`
- `_Hz`
- `_dP`, `_pct`
- `_C`, `_cC`
- `_s`, `_ms`

If a unit is missing, add it here before using it in code.

### 5) Return-Type Prefix (Required)

Functions and methods must include a **return-type prefix** for consistency with variable prefixes:

- `b_`   : returns `bool`
- `i_`   : returns `int`
- `f_`   : returns `float`
- `d_`   : returns `float` (conceptual double)
- `s_`   : returns `str` (char pointer)
- `e_`   : returns an `enum`
- `ui8_` : returns `uint8_t`
- `ui16_`: returns `uint16_t`
- `i32_` : returns `int32_t`
- `vd_`  : returns `void`

For functions returning **structures / complex domain entities**:
- **no prefix** (same rule as variables)

Examples:
- `bool b_is_row_valid(...)`
- `float d_calculate_total_eur(...)`
- `char* s_normalize_isin(...)`
- `void vd_store_transactions(...)`
- `Transaction create_transaction(...)`

### 6) Ports and adapters naming
- Ports (interfaces): `<Thing>Port` or `<UseCase>Name`
  - examples: `CanBusPort`, `DeviceRepositoryPort`, `ConfigPort`
- Adapter implementations: `<Thing><Technology>Adapter`
  - examples: `CanBusSocketCanAdapter`

### 7) Tests naming
- Test modules: `test_<module>`
- Test function names: `test_<function_name>__<case_description>`
  - example: `test_b_is_running__start_mode_returns_true`

### 8) Ambiguity Rule
- Avoid ambiguous names.
- Do not shadow struct attributes with local variables using the same name.

### 9) Tester Subclasses (Tests)
For testing complex logic in isolation:
- Create specific Test helpers/wrappers.
- Expose internal helpers explicitly for testing if needed.
