/**
 * @file    AMS_bms_driver.c
 * @brief   See AMS_bms_driver.h.
 *
 *          Calls into Algorithms/AMS_bms_safety_algorithms.c for the NTC
 *          voltage->temperature conversion (f_BmsSafety_NtcVoltageToTempC).
 *          Not a layering violation: that function is pure (no HAL/RTOS
 *          deps of its own), and the dependency only runs one direction
 *          (Driver -> Algorithms, never back) — reusing it here avoids
 *          duplicating the per-channel flatten loop between this file and
 *          the task.
 */
#include "Drivers_Custom/AMS_bms_driver.h"
#include "Algorithms/AMS_bms_safety_algorithms.h"
#include "LTC6813.h"
#include "LTC681x.h"
#include "bms_hardware.h"
#include "dwt_delay.h"
#include <string.h>

/* -------------------------------------------------------------------------
 * SPI / CS / delay glue — ported from Test_4_09_2025's bms_hardware.c.
 * Private to this file; the vendor library calls these by name (weak
 * coupling via function name, not a function-pointer table — that's how
 * the vendor library itself is written, see LTC681x.c's calls to
 * spi_write_array()/spi_write_read()/wakeup_sleep()/wakeup_idle()).
 * ---------------------------------------------------------------------- */

static SPI_HandleTypeDef *s_phspi = NULL;

void cs_low(uint8_t pin) {
    (void)pin;
    HAL_GPIO_WritePin(BMS_SPI_CS_GPIO_Port, BMS_SPI_CS_Pin, GPIO_PIN_RESET);
}

void cs_high(uint8_t pin) {
    (void)pin;
    HAL_GPIO_WritePin(BMS_SPI_CS_GPIO_Port, BMS_SPI_CS_Pin, GPIO_PIN_SET);
}

void delay_u(uint16_t micro) {
    DWT_Delay(micro);
}

void delay_m(uint16_t milli) {
    HAL_Delay(milli);
}

void spi_write_array(uint8_t len, uint8_t data[]) {
    HAL_SPI_Transmit(s_phspi, data, len, 100);
}

void spi_write_read(uint8_t tx_Data[], uint8_t tx_len, uint8_t *rx_data, uint8_t rx_len) {
    HAL_SPI_Transmit(s_phspi, tx_Data, tx_len, 100);
    HAL_SPI_Receive(s_phspi, rx_data, rx_len, 100);
}

uint8_t spi_read_byte(uint8_t tx_dat) {
    (void)tx_dat;
    uint8_t data = 0;
    HAL_SPI_Receive(s_phspi, &data, 1, 100);
    return data;
}

/* -------------------------------------------------------------------------
 * LTC6813 configuration — same register values as Test_4_09_2025's
 * spi_stm32f4.c bms_init()/performMeasurements(), just without the printf
 * calls that were interleaved with them there.
 * ---------------------------------------------------------------------- */

static const uint8_t ADC_CONVERSION_MODE = MD_7KHZ_3KHZ;

static bool REFON = true;
static bool ADCOPT = true;
static bool s_gpiobits_a[5] = {true, true, true, true, true};
static bool s_gpiobits_b[4] = {true, true, true, true};
static uint16_t s_uv_threshold = 28000;  /* ADC code, LTC6813 comparator — separate from
                                           * BMS_CELL_UNDERVOLTAGE_mV in the Algorithms layer */
static uint16_t s_ov_threshold = 42000;
static bool s_dccbits_a[12] = {0};
static bool s_dccbits_b[7]  = {0};
static bool s_dctobits[4]   = {true, false, true, false};
static bool s_fdrf  = false;
static bool s_dtmen = true;
static bool s_psbits[2] = {false, false};

static cell_asic s_bms_ic[BMS_TOTAL_IC];

bool b_BMS_Driver_Init(SPI_HandleTypeDef *phspi)
{
    if (phspi == NULL) {
        return false;
    }
    s_phspi = phspi;

    DWT_Init();

    LTC6813_init_cfg(BMS_TOTAL_IC, s_bms_ic);
    LTC6813_init_cfgb(BMS_TOTAL_IC, s_bms_ic);

    for (uint8_t ic = 0; ic < BMS_TOTAL_IC; ic++) {
        LTC6813_set_cfgr(ic, s_bms_ic, REFON, ADCOPT, s_gpiobits_a, s_dccbits_a, s_dctobits, s_uv_threshold, s_ov_threshold);
        LTC6813_set_cfgrb(ic, s_bms_ic, s_fdrf, s_dtmen, s_psbits, s_gpiobits_b, s_dccbits_b);
    }

    LTC6813_reset_crc_count(BMS_TOTAL_IC, s_bms_ic);
    LTC6813_init_reg_limits(BMS_TOTAL_IC, s_bms_ic);

    wakeup_sleep(BMS_TOTAL_IC);
    LTC681x_wrcfg(BMS_TOTAL_IC, s_bms_ic);
    LTC681x_wrcfgb(BMS_TOTAL_IC, s_bms_ic);

    wakeup_idle(BMS_TOTAL_IC);
    int8_t err = LTC681x_rdcfg(BMS_TOTAL_IC, s_bms_ic);
    if (err != 0) {
        return false;
    }
    err = LTC681x_rdcfgb(BMS_TOTAL_IC, s_bms_ic);
    if (err != 0) {
        return false;
    }

    return true;
}

bool b_BMS_Driver_Measure(AMS_BMS_Data_t *p_out)
{
    if ((p_out == NULL) || (s_phspi == NULL)) {
        return false;
    }

    wakeup_sleep(BMS_TOTAL_IC);

    wakeup_idle(BMS_TOTAL_IC);
    LTC6813_wrcfg(BMS_TOTAL_IC, s_bms_ic);
    LTC6813_wrcfgb(BMS_TOTAL_IC, s_bms_ic);

    wakeup_idle(BMS_TOTAL_IC);
    LTC6813_adcv(ADC_CONVERSION_MODE, DCP_DISABLED, CELL_CH_ALL);
    LTC6813_pollAdc();
    wakeup_idle(BMS_TOTAL_IC);
    uint8_t pec_err = LTC6813_rdcv(0, BMS_TOTAL_IC, s_bms_ic);

    wakeup_idle(BMS_TOTAL_IC);
    LTC6813_adax(ADC_CONVERSION_MODE, AUX_CH_ALL);
    LTC6813_pollAdc();
    wakeup_idle(BMS_TOTAL_IC);
    pec_err |= LTC6813_rdaux(0, BMS_TOTAL_IC, s_bms_ic);

    if (pec_err != 0) {
        return false;
    }

    /* --- Flatten per-IC cell_asic arrays into the flat Broker-shaped arrays --- */
    uint32_t pack_mV_sum = 0;
    uint16_t min_mV = 0xFFFFu, max_mV = 0;
    uint8_t  min_id = 0, max_id = 0;
    int16_t  max_temp_cC = INT16_MIN;

    for (uint8_t ic = 0; ic < BMS_TOTAL_IC; ic++) {
        for (uint8_t c = 0; c < BMS_CELLS_PER_IC; c++) {
            uint16_t cell_mV = (uint16_t)(s_bms_ic[ic].cells.c_codes[c] / 10u); /* 100uV code -> mV */
            uint16_t flat_idx = (uint16_t)(ic * BMS_CELLS_PER_IC + c);
            p_out->ui16_cell_mV[flat_idx] = cell_mV;

            pack_mV_sum += cell_mV;
            if (cell_mV < min_mV) { min_mV = cell_mV; min_id = flat_idx; }
            if (cell_mV > max_mV) { max_mV = cell_mV; max_id = flat_idx; }
        }

        uint8_t temp_ch_out = 0;
        for (uint8_t gpio = 0; gpio < 9; gpio++) {
            if (gpio == 5) continue;  /* channel 5 is Vref2, not an NTC */
            float voltage = s_bms_ic[ic].aux.a_codes[gpio] * 0.0001f;
            float temp_C  = f_BmsSafety_NtcVoltageToTempC(voltage);
            int16_t temp_cC = (int16_t)(temp_C * 100.0f);

            uint16_t flat_idx = (uint16_t)(ic * BMS_TEMP_CH_PER_IC + temp_ch_out);
            p_out->i16_cell_temp_cC[flat_idx] = temp_cC;
            if (temp_cC > max_temp_cC) { max_temp_cC = temp_cC; }
            temp_ch_out++;
        }
    }

    p_out->ui32_pack_voltage_mV = pack_mV_sum;
    p_out->ui16_min_cell_mV = min_mV;
    p_out->ui16_max_cell_mV = max_mV;
    p_out->ui8_min_cell_id  = min_id;
    p_out->ui8_max_cell_id  = max_id;
    p_out->i16_max_cell_temp_cC = max_temp_cC;

    return true;
}
