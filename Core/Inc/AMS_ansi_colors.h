/**
 * @file    AMS_ansi_colors.h
 * @brief   ANSI escape-code colour definitions for debug terminal output.
 *
 *          Include this header in any module that prints coloured text via
 *          printf / SWO.  All macros expand to string literals so they
 *          concatenate with adjacent string constants at compile time with
 *          zero runtime overhead.
 *
 *          Usage:
 *            printf(ANSI_FG_GREEN "OK" ANSI_RESET "\r\n");
 *            printf(COL_WARN "Error: bad value\r\n" ANSI_RESET);
 *
 * @note    Disable all colour output by defining AMS_ANSI_COLORS_DISABLE
 *          before including this header (e.g. in a unit-test host build).
 *
 * @author  Eneko Juanena
 */

#ifndef AMS_ANSI_COLORS_H_
#define AMS_ANSI_COLORS_H_

/* =========================================================================
 * Colour kill-switch for host/unit-test builds
 * ========================================================================= */
#ifdef AMS_ANSI_COLORS_DISABLE

#define ANSI_RESET       ""
#define ANSI_BOLD        ""
#define ANSI_DIM         ""

#define ANSI_FG_WHITE    ""
#define ANSI_FG_CYAN     ""
#define ANSI_FG_GREEN    ""
#define ANSI_FG_YELLOW   ""
#define ANSI_FG_MAGENTA  ""
#define ANSI_FG_BLUE     ""
#define ANSI_FG_RED      ""
#define ANSI_FG_ORANGE   ""

#define ANSI_BG_DARK     ""

#else /* normal embedded build */

/* -------------------------------------------------------------------------
 * Base attributes
 * ---------------------------------------------------------------------- */
#define ANSI_RESET       "\033[0m"   /**< Reset all attributes              */
#define ANSI_BOLD        "\033[1m"   /**< Bold / bright                     */
#define ANSI_DIM         "\033[2m"   /**< Dim / faint                       */

/* -------------------------------------------------------------------------
 * Foreground colours (bright variants for dark backgrounds)
 * ---------------------------------------------------------------------- */
#define ANSI_FG_WHITE    "\033[97m"
#define ANSI_FG_CYAN     "\033[96m"
#define ANSI_FG_GREEN    "\033[92m"
#define ANSI_FG_YELLOW   "\033[93m"
#define ANSI_FG_MAGENTA  "\033[95m"
#define ANSI_FG_BLUE     "\033[94m"
#define ANSI_FG_RED      "\033[91m"
#define ANSI_FG_ORANGE   "\033[38;5;214m"  /**< 256-colour orange           */

/* -------------------------------------------------------------------------
 * Background colours
 * ---------------------------------------------------------------------- */
#define ANSI_BG_DARK     "\033[40m"  /**< Black background                  */

#endif /* AMS_ANSI_COLORS_DISABLE */

/* =========================================================================
 * Semantic colour aliases  (project-level conventions)
 *
 * These map a *meaning* to a colour so that modules never hard-code a raw
 * ANSI code.  Change the colour here and every module updates automatically.
 * ========================================================================= */

/** Section header banner (bold white on dark background) */
#define COL_HEADER      ANSI_BOLD ANSI_FG_WHITE  ANSI_BG_DARK

/** Per-section accent colours (DataBroker printer) */
#define COL_ADC         ANSI_BOLD ANSI_FG_CYAN
#define COL_VEHICLE     ANSI_BOLD ANSI_FG_GREEN
#define COL_GPS         ANSI_BOLD ANSI_FG_YELLOW
#define COL_TELEMETRY   ANSI_BOLD ANSI_FG_MAGENTA
#define COL_PERSISTENT  ANSI_BOLD ANSI_FG_BLUE
#define COL_BMS         ANSI_BOLD ANSI_FG_ORANGE
#define COL_BATTSTATS   ANSI_BOLD ANSI_FG_RED

/** Generic UI roles */
#define COL_LABEL       ANSI_DIM  ANSI_FG_WHITE  /**< Field name (dimmed)   */
#define COL_VALUE       ANSI_FG_WHITE             /**< Field value           */
#define COL_WARN        ANSI_BOLD ANSI_FG_RED     /**< Warning / error       */
#define COL_OK          ANSI_FG_GREEN             /**< Success / valid       */
#define COL_NA          ANSI_DIM  ANSI_FG_RED     /**< Not available / none  */

#endif /* AMS_ANSI_COLORS_H_ */
