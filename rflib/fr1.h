/*
 * Copyright 2022-2025 NXP
 */

/*
 * NXP Proprietary. This software is owned or controlled by NXP and may only
 * be used strictly in accordance with the applicable license terms. By expressly accepting
 * such terms or by downloading, installing, activating and/or otherwise using
 * the software, you are agreeing that you have read, and that you agree to
 * comply with and are bound by, such license terms. If you do not agree to
 * be bound by the applicable license terms, then you may not retain,
 * install, activate or otherwise use the software.
 */

#ifndef _FR1_DIO_H_
#define _FR1_DIO_H_

#include <mt3812_drv.h>
#include <mt3812_types.h>
#include <diora_osal.h>
#include <diora_phal.h>

#include <rflib_common.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

#define FR1_TX_MAX_GAIN_IDX 31
#define FR1_TX_MIN_GAIN_IDX 0
#define FR1_TX_MAX_GAIN_DB 62

#define FR1_RX_MAX_RF_GAIN_IDX 7
#define FR1_RX_MIN_RF_GAIN_IDX 0
#define FR1_RX_MAX_BB_GAIN_IDX 15
#define FR1_RX_MIN_BB_GAIN_IDX 0
#define FR1_RX_MAX_GAIN_DB 42

#define MT_DEV_BASE_SIZE 128 // Keeping it equal to size of MT rf handle
#define MT_DEV_BUF_SIZE 64 // Some additional buffer aligned to 32 bits
#define MT_DEV_SIZE MT_DEV_BASE_SIZE+MT_DEV_BUF_SIZE
#define MAX_FREQS 1 // For current release, Number of LO freqs is one.
#define MAX_DEVS 2

//#define NX_SCRATCH_BUF_PHY_ADDR 0x92400000 //Scratch Buffer Base Address
//#define NX_SCRATCH_BUF_OFFSET 0x26391c0 //Offset is added from LA9310_MEM_REGION_STD_FW
//#define NX_SCRATCH_BUF_OFFSET 0x263a000 //Offset is added from LA9310_MEM_REGION_STD_FW
//#define NX_MT_MDATA_BASE_ADDR NX_SCRATCH_BUF_PHY_ADDR+NX_SCRATCH_BUF_OFFSET

#define NX_MT_RF_SZ 0x30000

#define MMAP_MDATA(size, offset) ({ \
	long page_size = sysconf(_SC_PAGESIZE); \
	FR1_LOG_ERR("Page Size = %ldbytes\n", page_size); \
	void *addr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, offset); \
	if(addr == MAP_FAILED) { \
		FR1_LOG_ERR("mmap of scratch buff to mdata failed, size %ldbytes offset 0x%x\n", size, offset); \
		perror("mmap"); \
		NULL; \
	} \
	FR1_LOG_ERR("mmap of scratch buff to mdata succeeded, addr %p size %ldbytes offset 0x%x\n", addr, size, offset); \
	addr; \
})

#define MT_DRV_OPEN 1<<0
#define MT_DRV_LOAD 1<<1 //By Default after Load, we will be in STANDBY state
#define MT_SS_BBLOOP 1<<2
#define MT_SS_TRXPLLON 1<<3
#define MT_SS_PREPARED 1<<4
#define MT_SS_CALPLLON 1<<5
#define MT_SS_RFLOOP 1<<6
#define MT_SS_ACTIVE 1<<7
#define MT_CAL_DONE 1<<8

typedef struct{
    char rf_path[22];
}RF_PATH;

typedef struct RF_FR1_CONFIG{
    int lo_freq_KHz;
    char *rf_cal_file;
    uint8_t path_tx;
    uint8_t path_rx1;
    uint8_t path_rx2;
    uint8_t tx_bw;
    uint8_t rx_bw;
    int tx_gain_db;
    int rx_gain_db;
} RF_FR1_CONFIG;

typedef enum {
    FR1_1T1R = 1,
    FR1_1T2R = 2
} FR1DevType;

typedef enum rf_sw_cmd {
    FR1_SW_CMD_INVALID = 0,
    FR1_SW_CMD_API_INIT = 1,
    FR1_SW_CMD_API_DEINIT = 2,
    FR1_SW_CMD_ADJUST_PLL_FREQ = 3,
    FR1_SW_CMD_SET_TX_GAIN = 4,
    FR1_SW_CMD_SET_RX_GAIN = 5,
    FR1_SW_CMD_SET_TX_GAIN_IDX = 6,
    FR1_SW_CMD_SET_RX_GAIN_IDX = 7,
    FR1_SW_CMD_GET_TX_GAIN = 8,
    FR1_SW_CMD_GET_RX_GAIN = 9,
    FR1_SW_CMD_GET_SYSSTATUS = 10,
    RF_SW_CMD_MAX_COUNT
} rf_sw_cmd_t;

typedef struct mt3812_priv_handle{
    mt3812_handle_t mt3812_handle;
    struct mt3812_init_params init_params;
    int32_t rf_lo_freq_khz;
    int32_t tx_rf_gain;
    int32_t tx_bb_gain;
    int32_t rx_rf_gain;
    int32_t rx_bb_gain;
    int32_t tx_gain_db;
    int32_t rx_gain_db;
    int32_t trx_path;
    int8_t tx_bw;
    int8_t rx_bw;
    volatile uint64_t cmd_latencies[RF_SW_CMD_MAX_COUNT];
    FR1DevType dev_type;
} RFDevice_t;

typedef RFDevice_t * RficAPIHandle_t;

struct mt_mdata {
	u32 mt_magic_word;			/**< Set to MAGIC_WORD if the driver is initialized */
	u32 mt_init_rtc;
	sys_state_t mt_sys_state;	/**< System state 	*/
	u32 mt_rst_gpio;			/**< RESETN GPIO ID and Pin Number			*/
	u32 mt_rst_pinnum;
	u32 mt_txrx_sw_gpio;		/**< TXRX_SW1 GPIO ID and Pin Number		*/
	u32 mt_txrx_sw_pinnum;
	u32 mt_txrx_sw2_gpio;		/**< TXRX_SW2 GPIO ID and Pin Number		*/
	u32 mt_txrx_sw2_pinnum;
	u32 mt_pa_polarity;
	u32 mt_tx1pa_gpio_id;		/**< TX1_PA_ENABLE GPIO ID and Pin Number	*/
	u32 mt_tx1pa_gpio_pinnum;
	u32 mt_tx2pa_gpio_id;		/**< TX2_PA_ENABLE GPIO ID and Pin Number	*/
	u32 mt_tx2pa_gpio_pinnum;
	u32 mt_rx1lna_gpio_id;		/**< RX1_LN1_BYPASS GPIO ID and Pin Number	*/
	u32 mt_rx1lna_gpio_pinnum;
	u32 mt_rx2lna_gpio_id;		/**< RX2_LNA_BYPASS GPIO ID and Pin Number	*/
	u32 mt_rx2lna_gpio_pinnum;
	u32 mt_rx1dpd_gpio_id;		/**< RX1_DPD_SWITCH GPIO ID and Pin Number	*/
	u32 mt_rx1dpd_gpio_pinnum;
	u32 mt_rx2dpd_gpio_id;		/**< RX2_DPD_SWITCH GPIO ID and Pin Number	*/
	u32 mt_rx2dpd_gpio_pinnum;
	void* mt_cmd_rw_device_h;	/**< Peripheral bank to use if command interface peripheral is to be initialized	*/
	u32 mt_cmd_rw_chan;		/**< Selected channel (CS), if command interface peripheral has channels 			*/
	void* mt_dcshandle;
	char *mt_drv_date;	    /**< Date 			*/
	char *mt_drv_vers;	    /**< Driver version */
};

struct rf_dev_mdata {
        u32 magic_word;
        u32 flags_state;
        RFDevice_t nx_rf_dev;
        struct mt_mdata mt_mdata;
        u32 log_level;
};

extern struct rf_dev_mdata * rf_mdata[];

#define MAX_RTC 12

#ifndef RFAPI
static char return_code[13][23] =
{ "RTC_OK",
  "RTC_CMD_UNKNOWN",       "RTC_CMD_WRONG_FLAGS", "RTC_TOO_FEW_DATAWORDS",  "RTC_TOO_MANY_DATAWORDS",
  "RTC_INVALID_PARAMETER", "RTC_WRONG_SYS_STATE", "RTC_WRONG_AUXADC_STATE", "RTC_WRONG_TXRX_STATE",
  "RTC_INVALID_MODE",      "RTC_PAENV_ERROR",     "RTC_TIMEOUT",            "RTC_NO_RESULT" };

static char error_code[8][16] =
{ "MT3812_NOERR", "MT3812_ERROR", "MT3812_ENOTSUP", "MT3812_ECMDPROC",
  "MT3812_ERSPPROC", "MT3812_EINVTR", "MT3812_EINVPR", "MT3812_ENOMEM" };

static char sys_state[9][12] =
{ "SS_IDLE", "SS_STANDBY", "SS_BBLOOP", "SS_TRXPLLON", "SS_PREPARED",
  "SS_CALPLLON", "SS_RFLOOP", "SS-ACTIVE", "SS_UNKNOWN" };

static char txbw[1][10] =
{"TXBW_50M"};

static char rxbw[5][10] =
{ "RXBW_20M", "RXBW_25M", "RXBW_30M", "RXBW_40M",  "RXBW_50M"};

static char band[4][10] = 
{ "BAND_AUTO", "BAND_HB", "BAND_MB", "BAND_LB" };

static char dpd_mode[6][12] =
{ "DPD_NONE", "DPD_RX1_RX", "DPD_RX1_RO", "DPD_RX2_RX", "DPD_RX2_RO", "DPD_RX1_RX2" };

static char rx_mode[4][8] =
{ "RX_NONE", "RX_1", "RX_2", "RX_1_2" };

static char act_mode[4][11] =
{ "ACT_OFF", "ACT_TDD", "ACT_FDD_T", "ACT_FDD_RX" };

static char mode[2][4] =
{ "OFF", "ON" };

static char channel[2][6] =
{ "CH_TX", "CH_RX" };

static char rate[2][11] =
{ "7.68 MSps", "15.36 MSps" };

static char rf_interface[2][5] =
{ "SPI", "LLCP" };

static char trxpath [16][21] =
{ "PATH_NONE", "PATH_RX1", "PATH_RX2", "PATH_RX1_RX2", "PATH_TX1", "PATH_RX1_TX1",
  "PATH_RX2_TX1", "PATH_RX1_RX2_TX1", "PATH_TX2", "PATH_RX1_TX2", "PATH_RX2_TX2",
  "PATH_RX1_RX2_TX2", "PATH_TX1_TX2", "PATX_RX1_TX1_TX2", "PATH_RX2_TX1_TX2", "PATH_RX1_RX2_TX1_TX2" };

#define RTC_MESSAGE(Func, rtc) \
{ \
	if (rtc <= MAX_RTC) \
		printf( "\r%s FAILED. Return code: %s\n", __func__, return_code[rtc] ); \
	else if (rtc == 255) \
		printf( "\r%s FAILED. Return code: RTC_INTERNAL_ERROR\n", __func__ ); \
	else \
		printf( "\r%s FAILED. Return code not supported: %d\n", __func__, rtc ); \
}
#endif

void get_cmd_handle ();

RficAPIHandle_t diora_drv_open_reentry (FR1DevType dev_type);

RficAPIHandle_t diora_drv_open (FR1DevType dev_type);
int32_t diora_drv_close (void);
int32_t diora_drv_load (void);
int32_t diora_drv_version (void);
int32_t diora_drv_read (uint32_t addr);
int32_t diora_drv_write (uint32_t addr, const uint16_t val);

int32_t rf_adjust_pll_freq (int32_t *pll_freq_args, uint32_t pll_freq_args_size);
int32_t rf_get_trx_pll (void);
int32_t rf_set_txbw (int32_t *txbw_args, int32_t txbw_args_size);
int32_t rf_get_txbw (void);
int32_t rf_set_rxbw (int32_t *rxbw_args, int32_t rxbw_args_size);
int32_t rf_get_rxbw (void);
int32_t rf_set_active (int32_t active_mode, int32_t rx_path);
int32_t rf_tx_gain_control_relative_diff (int32_t gain_db);
int32_t rf_rx_gain_control_relative_diff (int32_t gain_db);
int32_t rf_set_gain_idx (int32_t rf_path, uint8_t chan, uint32_t bbgain, uint32_t rfgain);
int32_t rf_get_gain_idx	(void);
int32_t rf_set_path (int32_t rf_path, int32_t freq_band, int32_t rssi, int32_t dpd, int32_t rx_bw, int32_t tx_bw);
int32_t rf_get_path (void);
int32_t rf_set_bbloop (int32_t rx_path, int32_t tx_bw, int32_t rx_bw);
int32_t rf_set_cal_pll (int32_t *cal_pll_args, uint32_t cal_pll_args_size);
int32_t rf_get_cal_pll(void);
int32_t rf_set_rfloop (int32_t rx_path, int32_t freq_band, int32_t rf_loop);

int32_t rf_app_get_sysstatus (void);
int32_t rf_app_get_temperature (void);
int32_t rf_app_cal_resistor (int32_t *cal_resistor_args, int32_t cal_resistor_args_size);
int32_t rf_app_cal_regulator (void);

int32_t diora_gen_get_version (void);
int32_t diora_gen_set_register (int32_t *set_register_args);
int32_t diora_gen_get_register (uint32_t addr, uint32_t len);
int32_t diora_gen_set_register_masked (uint32_t addr, uint32_t val, uint32_t mask);
int32_t diora_gen_set_property (int32_t *set_property_args);
int32_t diora_gen_get_property (uint32_t property, uint32_t len);

int32_t diora_calib_bw (int32_t chan, int32_t freq_band, int32_t freq, int32_t rx_bw, int32_t tx_bw);
int32_t diora_calib_rxdc (int32_t *calib_rxdc_args, uint32_t calib_rxdc_args_size);

int32_t fr1_rf_init (RF_FR1_CONFIG *config);
int32_t rf_get_rf_status (void);
#endif
