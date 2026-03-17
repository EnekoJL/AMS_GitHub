/**
 * @file    AMS_Led_Task.c
 * @brief   LED Manager - independent per-LED state machine.
 * @date    17 de Marzo de 2026
 *
 * Each LED has its own channel with its own mode, toggle timer and blink deadline.
 * All LEDs run independently: blue can TOGGLE while green is ON, etc.
 */

#include "Middleware/AMS_Led_Task.h"
#include "Drivers_Custom/AMS_led_driver.h"
#include "AMS_DataTypes.h"
#include "main.h"

/* -----------------------------------------------------------------------
 * Configuration
 * ----------------------------------------------------------------------- */
#define TOGGLE_PERIOD_ms  500u

/* -----------------------------------------------------------------------
 * Per-LED channel state
 * ----------------------------------------------------------------------- */
typedef struct {
    AMS_LED_PinMode_t mode;
    uint32_t          toggle_tick;   /* last toggle timestamp for TOGGLE mode */
    uint32_t          blink_end;     /* deadline tick for BLINK mode          */
} LedChannel_t;

static LedChannel_t s_channels[LED_COLOR_COUNT];

/* -----------------------------------------------------------------------
 * Driver function tables indexed by AMS_LED_Color_t
 * (avoids repetitive switch statements in the processing loop)
 * ----------------------------------------------------------------------- */
typedef void (*LedSetFn_t)(bool);
typedef void (*LedToggleFn_t)(void);

static const LedSetFn_t    led_set_fn[LED_COLOR_COUNT]    = {
    [LED_COLOR_GREEN]  = vd_LED_Driver_SetGreen,
    [LED_COLOR_RED]    = vd_LED_Driver_SetRed,
    [LED_COLOR_BLUE]   = vd_LED_Driver_SetBlue,
    [LED_COLOR_ORANGE] = vd_LED_Driver_SetOrange,
};

static const LedToggleFn_t led_toggle_fn[LED_COLOR_COUNT] = {
    [LED_COLOR_GREEN]  = vd_LED_Driver_ToggleGreen,
    [LED_COLOR_RED]    = vd_LED_Driver_ToggleRed,
    [LED_COLOR_BLUE]   = vd_LED_Driver_ToggleBlue,
    [LED_COLOR_ORANGE] = vd_LED_Driver_ToggleOrange,
};

static const uint32_t led_blink_duration_ms[LED_COLOR_COUNT] = {
    [LED_COLOR_GREEN]  = LED_BLINK_DURATION_GREEN_ms,
    [LED_COLOR_RED]    = LED_BLINK_DURATION_RED_ms,
    [LED_COLOR_BLUE]   = LED_BLINK_DURATION_BLUE_ms,
    [LED_COLOR_ORANGE] = LED_BLINK_DURATION_ORANGE_ms,
};

/* -----------------------------------------------------------------------
 * Process one LED channel (called from vd_LED_Manager_Process for each color)
 * ----------------------------------------------------------------------- */
static void led_channel_process(AMS_LED_Color_t color)
{
    LedChannel_t *ch = &s_channels[color];

    switch (ch->mode)
    {
        case LED_PIN_OFF:
            /* Nothing to do — LED was turned off in SetMode */
            break;

        case LED_PIN_ON:
            /* Nothing to do — LED was turned on in SetMode */
            break;

        case LED_PIN_TOGGLE:
            if (HAL_GetTick() - ch->toggle_tick >= TOGGLE_PERIOD_ms) {
                ch->toggle_tick = HAL_GetTick();
                led_toggle_fn[color]();
            }
            break;

        case LED_PIN_BLINK:
            if (HAL_GetTick() >= ch->blink_end) {
                /* Blink window expired: turn this LED off */
                led_set_fn[color](false);
                ch->mode = LED_PIN_OFF;
            }
            break;
    }
}

/* -----------------------------------------------------------------------
 * Public API
 * ----------------------------------------------------------------------- */
void vd_LED_Manager_Init(void)
{
    vd_LED_Driver_Init();

    for (int i = 0; i < LED_COLOR_COUNT; i++) {
        s_channels[i].mode        = LED_PIN_OFF;
        s_channels[i].toggle_tick = 0u;
        s_channels[i].blink_end   = 0u;
    }
}

void vd_LED_Manager_SetMode(AMS_LED_Color_t color, AMS_LED_PinMode_t mode)
{
    if (color >= LED_COLOR_COUNT) { return; }

    LedChannel_t *ch = &s_channels[color];
    ch->mode = mode;

    switch (mode)
    {
        case LED_PIN_OFF:
            led_set_fn[color](false);
            break;

        case LED_PIN_ON:
            led_set_fn[color](true);
            break;

        case LED_PIN_TOGGLE:
            ch->toggle_tick = HAL_GetTick();  /* start timer fresh */
            break;

        case LED_PIN_BLINK:
            led_set_fn[color](true);
            ch->blink_end = HAL_GetTick() + led_blink_duration_ms[color];
            break;
    }
}

void vd_LED_Manager_Process(void)
{
    for (AMS_LED_Color_t color = 0; color < LED_COLOR_COUNT; color++) {
        led_channel_process(color);
    }
}
