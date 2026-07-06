/**
 * @file    test_AMS_Flash_Task.c
 * @brief   Unit tests for AMS_Flash_Task.c's sector-scan / record-validation
 *          / wear-leveling logic (the actual "smart" part of the flash
 *          persistence layer).
 *
 *          Mocks Drivers_Custom/AMS_flash_driver.h (so no real flash is
 *          touched) and Middleware/AMS_DataBroker.h (so this tests the Flash
 *          Task in isolation from the real Broker).
 *
 *          ORDER MATTERS IN THIS FILE — same reason as test_AMS_DataBroker.c:
 *          the Flash Task keeps its scan/write state in a file-scope static
 *          (`s_state`) that persists across test functions in this binary.
 *          The "not yet initialized" test MUST run first.
 *
 *          NOT COVERED: b_Persist_SaveConfig()'s real write path (erase +
 *          write + CRC) is gated behind FEATURE_FLASH_WRITE_ENABLE, which is
 *          currently 0 in AMS_task_config.h — so in the CURRENT build,
 *          SaveConfig always short-circuits to `return true` without
 *          touching flash at all. That's tested here (T02). If/when that
 *          flag flips to 1, the write path (sector-full rollover, CRC
 *          construction, cycle_count increment) will need its own tests.
 */

#include "unity.h"
#include "mock_cmsis_os.h"
#include "mock_AMS_flash_driver.h"
#include "mock_AMS_DataBroker.h"
#include "Middleware/AMS_Flash_Task.h"
#include "AMS_DataStructs.h"

TEST_SOURCE_FILE("Middleware/AMS_Flash_Task.c")

void setUp(void) {}
void tearDown(void) {}

/* Mirrors prv_calc_crc32() in AMS_Flash_Task.c exactly (that function is
 * `static`, so it has no external linkage — this is the only way to build
 * a record with a CRC the code under test will accept as valid). Standard
 * CRC-32/ISO-HDLC: init 0xFFFFFFFF, poly 0xEDB88320 (reflected), final NOT. */
static uint32_t test_crc32(const uint8_t *data, uint32_t len)
{
    uint32_t crc = 0xFFFFFFFFu;
    for (uint32_t i = 0; i < len; i++) {
        crc ^= (uint32_t)data[i];
        for (uint8_t bit = 0; bit < 8u; bit++) {
            if (crc & 1u) { crc = (crc >> 1u) ^ 0xEDB88320u; }
            else           { crc >>= 1u; }
        }
    }
    return ~crc;
}

static AMS_Flash_Record_t build_valid_record(uint16_t soc, uint16_t cycle)
{
    AMS_Flash_Record_t rec = {0};
    rec.magic = AMS_FLASH_RECORD_MAGIC;
    rec.config.soc_percent_x10 = soc;
    rec.config.cycle_count     = cycle;
    rec.crc = test_crc32((const uint8_t *)&rec.config, sizeof(AMS_Persistent_Config_t));
    return rec;
}

/* =========================================================================
 * T00 — b_Persist_SaveConfig() before any Init must fail closed, no mocks
 * touched. MUST run first (see file header comment on static state).
 * =========================================================================
 */
void test_T00_SaveConfig_before_init_returns_false(void)
{
    TEST_ASSERT_FALSE(b_Persist_SaveConfig());
}

/* =========================================================================
 * T01 — Both sectors blank (freshly erased flash): must fall back to
 * defaults (SOC 100.0%, cycle 0) and write them to the Broker.
 * ========================================================================= */
void test_T01_blank_flash_falls_back_to_defaults(void)
{
    b_Flash_IsAddressErased_ExpectAndReturn(FLASH_EEPROM_SECTOR_A_ADDR, true);
    b_Flash_IsAddressErased_ExpectAndReturn(FLASH_EEPROM_SECTOR_B_ADDR, true);

    AMS_Persistent_Config_t expected_defaults = { .soc_percent_x10 = 1000u, .cycle_count = 0u };
    vd_Broker_Set_PersistentConfig_Expect(&expected_defaults);

    /* Boot diagnostic printf inside vd_Persist_Task_Init() reads it back */
    b_Broker_Get_PersistentConfig_ExpectAndReturn(NULL, true);
    b_Broker_Get_PersistentConfig_IgnoreArg_p_out();
    b_Broker_Get_PersistentConfig_ReturnThruPtr_p_out(&expected_defaults);

    vd_Persist_Task_Init();
}

/* =========================================================================
 * T02 — With state initialized (from T01) but FEATURE_FLASH_WRITE_ENABLE==0
 * in the current build, SaveConfig must short-circuit to `true` WITHOUT
 * calling any Broker or flash-driver function.
 * ========================================================================= */
void test_T02_SaveConfig_returns_true_without_touching_flash_when_write_disabled(void)
{
    TEST_ASSERT_TRUE(b_Persist_SaveConfig());
}

/* =========================================================================
 * T03 — Valid record only in sector A; sector B blank. A's data must load.
 * ========================================================================= */
void test_T03_valid_record_in_sector_A_only_loads_A(void)
{
    AMS_Flash_Record_t recA = build_valid_record(955, 5);

    b_Flash_IsAddressErased_ExpectAndReturn(FLASH_EEPROM_SECTOR_A_ADDR, false);
    b_Flash_ReadRecord_ExpectAndReturn(FLASH_EEPROM_SECTOR_A_ADDR, NULL, true);
    b_Flash_ReadRecord_IgnoreArg_p_out();
    b_Flash_ReadRecord_ReturnThruPtr_p_out(&recA);
    b_Flash_IsAddressErased_ExpectAndReturn(FLASH_EEPROM_SECTOR_A_ADDR + FLASH_EEPROM_RECORD_SIZE, true);

    b_Flash_IsAddressErased_ExpectAndReturn(FLASH_EEPROM_SECTOR_B_ADDR, true);

    vd_Broker_Set_PersistentConfig_Expect(&recA.config);

    b_Broker_Get_PersistentConfig_ExpectAndReturn(NULL, true);
    b_Broker_Get_PersistentConfig_IgnoreArg_p_out();
    b_Broker_Get_PersistentConfig_ReturnThruPtr_p_out(&recA.config);

    vd_Persist_Task_Init();
}

/* =========================================================================
 * T04 — Both sectors have a valid record; B has the higher cycle_count, so
 * B's (more recent) data must win, even though A was scanned first.
 * ========================================================================= */
void test_T04_both_valid_higher_cycle_count_wins_sector_B(void)
{
    AMS_Flash_Record_t recA = build_valid_record(900, 3);
    AMS_Flash_Record_t recB = build_valid_record(950, 7);

    b_Flash_IsAddressErased_ExpectAndReturn(FLASH_EEPROM_SECTOR_A_ADDR, false);
    b_Flash_ReadRecord_ExpectAndReturn(FLASH_EEPROM_SECTOR_A_ADDR, NULL, true);
    b_Flash_ReadRecord_IgnoreArg_p_out();
    b_Flash_ReadRecord_ReturnThruPtr_p_out(&recA);
    b_Flash_IsAddressErased_ExpectAndReturn(FLASH_EEPROM_SECTOR_A_ADDR + FLASH_EEPROM_RECORD_SIZE, true);

    b_Flash_IsAddressErased_ExpectAndReturn(FLASH_EEPROM_SECTOR_B_ADDR, false);
    b_Flash_ReadRecord_ExpectAndReturn(FLASH_EEPROM_SECTOR_B_ADDR, NULL, true);
    b_Flash_ReadRecord_IgnoreArg_p_out();
    b_Flash_ReadRecord_ReturnThruPtr_p_out(&recB);
    b_Flash_IsAddressErased_ExpectAndReturn(FLASH_EEPROM_SECTOR_B_ADDR + FLASH_EEPROM_RECORD_SIZE, true);

    vd_Broker_Set_PersistentConfig_Expect(&recB.config);

    b_Broker_Get_PersistentConfig_ExpectAndReturn(NULL, true);
    b_Broker_Get_PersistentConfig_IgnoreArg_p_out();
    b_Broker_Get_PersistentConfig_ReturnThruPtr_p_out(&recB.config);

    vd_Persist_Task_Init();
}

/* =========================================================================
 * T05 — Symmetric to T04: A has the higher cycle_count, A must win.
 * ========================================================================= */
void test_T05_both_valid_higher_cycle_count_wins_sector_A(void)
{
    AMS_Flash_Record_t recA = build_valid_record(980, 12);
    AMS_Flash_Record_t recB = build_valid_record(600, 2);

    b_Flash_IsAddressErased_ExpectAndReturn(FLASH_EEPROM_SECTOR_A_ADDR, false);
    b_Flash_ReadRecord_ExpectAndReturn(FLASH_EEPROM_SECTOR_A_ADDR, NULL, true);
    b_Flash_ReadRecord_IgnoreArg_p_out();
    b_Flash_ReadRecord_ReturnThruPtr_p_out(&recA);
    b_Flash_IsAddressErased_ExpectAndReturn(FLASH_EEPROM_SECTOR_A_ADDR + FLASH_EEPROM_RECORD_SIZE, true);

    b_Flash_IsAddressErased_ExpectAndReturn(FLASH_EEPROM_SECTOR_B_ADDR, false);
    b_Flash_ReadRecord_ExpectAndReturn(FLASH_EEPROM_SECTOR_B_ADDR, NULL, true);
    b_Flash_ReadRecord_IgnoreArg_p_out();
    b_Flash_ReadRecord_ReturnThruPtr_p_out(&recB);
    b_Flash_IsAddressErased_ExpectAndReturn(FLASH_EEPROM_SECTOR_B_ADDR + FLASH_EEPROM_RECORD_SIZE, true);

    vd_Broker_Set_PersistentConfig_Expect(&recA.config);

    b_Broker_Get_PersistentConfig_ExpectAndReturn(NULL, true);
    b_Broker_Get_PersistentConfig_IgnoreArg_p_out();
    b_Broker_Get_PersistentConfig_ReturnThruPtr_p_out(&recA.config);

    vd_Persist_Task_Init();
}

/* =========================================================================
 * T06 — A record with the correct magic word but a corrupted CRC (e.g. a
 * power-loss mid-write) must be treated as invalid — not "the active
 * record", not even "the reason to pick this sector". With B also blank,
 * this must fall back to defaults exactly like T01.
 * ========================================================================= */
void test_T06_corrupt_crc_is_rejected_falls_back_to_defaults(void)
{
    AMS_Flash_Record_t corrupt = build_valid_record(500, 1);
    corrupt.crc ^= 0xFFFFFFFFu; /* flip every bit — guaranteed wrong CRC */

    b_Flash_IsAddressErased_ExpectAndReturn(FLASH_EEPROM_SECTOR_A_ADDR, false);
    b_Flash_ReadRecord_ExpectAndReturn(FLASH_EEPROM_SECTOR_A_ADDR, NULL, true);
    b_Flash_ReadRecord_IgnoreArg_p_out();
    b_Flash_ReadRecord_ReturnThruPtr_p_out(&corrupt);
    b_Flash_IsAddressErased_ExpectAndReturn(FLASH_EEPROM_SECTOR_A_ADDR + FLASH_EEPROM_RECORD_SIZE, true);

    b_Flash_IsAddressErased_ExpectAndReturn(FLASH_EEPROM_SECTOR_B_ADDR, true);

    AMS_Persistent_Config_t expected_defaults = { .soc_percent_x10 = 1000u, .cycle_count = 0u };
    vd_Broker_Set_PersistentConfig_Expect(&expected_defaults);

    b_Broker_Get_PersistentConfig_ExpectAndReturn(NULL, true);
    b_Broker_Get_PersistentConfig_IgnoreArg_p_out();
    b_Broker_Get_PersistentConfig_ReturnThruPtr_p_out(&expected_defaults);

    vd_Persist_Task_Init();
}
