# Tasks Index

This is the one place to look to answer "what tasks exist, what does each do,
is it on right now, and who owns what Broker data." Every row below was read
directly out of `Core/Src/main.c` (the `osThreadAttr_t` blocks and the
`osThreadNew()` calls) and `Core/Inc/AMS_task_config.h` (the enable flags) —
if this table and the code ever disagree, the code wins and this table is
stale; please fix it rather than trust it blindly next time you're in here.

For the big picture of *why* the system is shaped this way (layers, the
Broker, the one-writer-per-struct rule, freshness flags), read
[`../Architecture_Overview.md`](../Architecture_Overview.md) first. This page
is the reference table; that page is the explanation.

## The gated-thread pattern

Six of the eight rows below follow the same shape: a compile-time
`TASK_*_ENABLE` flag in `AMS_task_config.h` decides whether `main()` even
creates the FreeRTOS thread. If the flag is `0`, the thread is never created
at all (no TCB, no stack allocated) — this used to not be true (all six
threads were always created and a disabled one just called `osThreadExit()`
on its first tick, wasting RAM), but `main.c` now wraps each `osThreadNew()`
call in `#if (TASK_*_ENABLE == 1)`. Each of those tasks has its own detail
README linked below.

**Two rows below don't fit this pattern** — `AMS_Algorithms_Task` and
`AMS_Led_Task` — and are included anyway rather than omitted, because a
newcomer grepping for "where does the LED state machine run" or "why is
there no `TASK_ALGORITHMS_ENABLE` flag" should find the answer here, not
conclude the task doesn't exist.

## Index

| Task | Source file | Enable flag | Thread name (`main.c`) | Priority | Stack size | Init function | Loop function | Broker struct(s) owned (sole writer) | Detail README |
|---|---|---|---|---|---|---|---|---|---|
| `AMS_ADC_Task` | `Core/Src/Middleware/AMS_ADC_Task.c` | `TASK_ADC_ENABLE` (currently **1**) | `Task_ADC` | `osPriorityNormal` | 1024 B (256 words × 4) | `vd_ADC_Task_Init(&hadc1, &hadc2, &htim2, &htim3)` | `vd_ADC_Manager_TaskProcess()` | `Vehicle_Data_t`, `AMS_ADC_Data_t` | [README](AMS_ADC_Task/README.md) |
| `AMS_CAN_Task` | `Core/Src/Middleware/AMS_CAN_Task.c` | `TASK_CAN_ENABLE` (currently **0**) | `Task_CAN` | `osPriorityNormal` | 1024 B | `vd_CAN_Task_Init(&hcan2)` | `vd_CAN_Manager_TaskProcess()` | `AMS_Powertrain_Data_t` | [README](AMS_CAN_Task/README.md) |
| `AMS_Flash_Task` | `Core/Src/Middleware/AMS_Flash_Task.c` | `TASK_FLASH_MEMO_ENABLE` (currently **0**) | `Task_Flash_Memo` | `osPriorityLow` | 1024 B | `vd_Flash_Task_Init(void)` — **not called from the thread body.** Runs once from `main()`, *before* `osKernelStart()`, gated by the same `TASK_FLASH_MEMO_ENABLE` flag — see the note below. | `vd_Flash_Manager_TaskProcess()` | `AMS_Persistent_Config_t` | [README](AMS_Flash_Task/README.md) |
| `AMS_GPS_Task` | `Core/Src/Middleware/AMS_GPS_Task.c` | `TASK_GPS_ENABLE` (currently **0**) | `GPS_Task` | `osPriorityNormal` | 1024 B | `vd_GPS_Task_Init(&huart3)` (bench wiring — production is `&huart6`, see the comment at the call site in `main.c`'s `GPS_Start_Task()`) | `vd_GPS_Manager_TaskProcess()` | `GPS_Data_t` | [README](AMS_GPS_Task/README.md) |
| `AMS_BMS_Task` | `Core/Src/Middleware/AMS_BMS_Task.c` | `TASK_BMS_ENABLE` (currently **0**, needs bench-testing first) | `Task_BMS` | `osPriorityNormal` | 1024 B | `vd_BMS_Task_Init(&hspi2)` | `vd_BMS_Manager_TaskProcess()` | `AMS_BMS_Data_t` | [README](AMS_BMS_Task/README.md) |
| `AMS_Logger_Task` | `Core/Src/Middleware/AMS_Logger_Task.c` | `TASK_SD_CARD_ENABLE` (currently **0**) **OR** `FEATURE_LOGGER_PRINT_ENABLE` (currently **1**) — the thread is created if *either* is set, since it hosts both the SD-card logger and the terminal dashboard printer as independent sub-features | `Task_SD_Card` | `osPriorityNormal` | 2048 B (512 words × 4 — double the others, because `printf`-heavy dashboard formatting needs more stack) | `vd_Logger_Task_Init(void)` | `vd_Logger_Manager_TaskProcess()` | *(none — read-only consumer of every domain, writes to SD/terminal, never to the Broker)* | [README](AMS_Logger_Task/README.md) |
| `AMS_Algorithms_Task` — **doesn't fit the gated-thread pattern**, see below | `Core/Src/Middleware/AMS_Algorithms_Task.c` | *(none — always on)* | `defaultTask` | `osPriorityNormal` | 1024 B | `vd_Algorithms_Task_Init(void)` (currently an empty function body — nothing to initialize yet) | `vd_Algorithms_Manager_TaskProcess()` | `AMS_Telemetry_Data_t`, `AMS_BatteryStats_Data_t` | [README](AMS_Algorithms_Task/README.md) |
| `AMS_Led_Task` — **not its own FreeRTOS thread at all**, see below | `Core/Src/Middleware/AMS_Led_Task.c` | *(none — not gated, not a thread)* | *(none — no thread name; runs inside `defaultTask`)* | *(none)* | *(none — no dedicated stack)* | `vd_LED_Manager_Init(void)` — called once from `main()` before `osKernelStart()` | `vd_LED_Manager_Process()` — ticked once per loop iteration (~every 10 ms) from inside `AMS_Algorithms_Task`'s `vd_Algorithms_Manager_TaskProcess()`, as its 5th and final section | *(none — purely local LED state, no Broker domain)* | [README](AMS_Led_Task/README.md) |

### Notes on the two rows that don't fit the pattern

**`AMS_Algorithms_Task`** is created unconditionally as `defaultTask` — there
is no `TASK_ALGORITHMS_ENABLE` flag, because it does not touch any
peripheral (no SPI/UART/CAN/ADC/SD dependency) and running it costs nothing
even with every acquisition task disabled. It is the single home for every
*derived* value in the system (current, charge/SOC, telemetry, thermal —
five sections total, in a fixed order — see its own README) and, as its 5th
section, drives the LED state machine.

**`AMS_Led_Task`** has no `osThreadNew()` call anywhere in `main.c` and
therefore no thread name, priority, or stack of its own to report — asking
"what priority does the LED task run at" is a category error, the same way
asking that about a function call would be. Its `vd_LED_Manager_Process()`
is a plain function, ticked from inside `AMS_Algorithms_Task`'s loop.
`vd_LED_Manager_Init()` (a *different* function — one-time GPIO/state setup,
not the periodic tick) is called once from `main()` before the scheduler
starts, alongside `b_Broker_Init()`, because without it all four Discovery
board LEDs stay lit from boot (active-low logic + CubeMX leaves the pins in
the RESET state).

### On `AMS_Flash_Task`'s split init

Unlike every other task, `AMS_Flash_Task`'s init function does **not** run
from inside its own thread body (`Flash_Memory_Start()` in `main.c` only
calls `vd_Flash_Manager_TaskProcess()`). `vd_Flash_Task_Init()` instead runs
directly from `main()`, before `osKernelStart()`, still gated by
`TASK_FLASH_MEMO_ENABLE`. Why: every other task reads
`AMS_Persistent_Config_t` from the Broker as soon as it starts, with no
ordering guarantee over which task's first loop iteration runs first (they
all share the same priority) — so the flash record has to be loaded into the
Broker *before* the scheduler starts handing out CPU time at all, not just
"early." See `docs/Next_Steps.md`'s "What's done" section for the bug this
fixed.

## Broker ownership at a glance

Every Broker struct above has exactly **one** writer task — see
[`Architecture_Overview.md` §4](../Architecture_Overview.md#4-data-ownership--safety-tiering)
for the full ownership table (readers, max-age, whether ownership is
enforced by code or just convention, and who actually checks the freshness
flags before trusting stale data).
