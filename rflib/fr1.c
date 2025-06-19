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

#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include <fr1.h>
#include <types.h>
#include <mt3812_api.h>
#include <mt3812_cmd.h>
#include <fw.h>

#include <sys/ioctl.h>
#include <la9310_modinfo.h>
#include <la9310_host_if.h>

int fd;

extern char* date_defn;
extern char* vers_defn;
extern rflib_ll_t rflib_fr1_log_level;

struct rf_dev_mdata * rf_mdata[MAX_DEVS];

RF_PATH path_append(bool rx1_on_off, bool rx2_on_off, bool tx_on_off){
    RF_PATH path_trx;
    strcpy(path_trx.rf_path, "PATH");
    if (rx1_on_off==1)
        strcat(path_trx.rf_path, "_RX1");
    if (rx2_on_off==1)
        strcat(path_trx.rf_path, "_RX2");
    if (tx_on_off==1)
        strcat(path_trx.rf_path,"_TX1_TX2");
    if (!rx1_on_off && !rx2_on_off && !tx_on_off)
        strcat(path_trx.rf_path,"_NONE");

    return path_trx;
}

path_t path_to_enum_path (char* str){
    for (int i =0; i<16; i++){
        if(strcmp(trxpath[i], str) == 0){
            return (path_t)i;
        }
    }
    return -1;
}

modinfo_t mi = {0};

static int get_mod_info()
{
	int ret;
	char dev_name[32];
	sprintf(dev_name, "/dev/%s%d", LA9310_DEV_NAME_PREFIX, 0);

	fd = open(dev_name, O_RDWR);
	if (fd < 0) {
		FR1_LOG_ERR("File %s open error\n", dev_name);
		return -1;
	}

	ret = ioctl(fd, IOCTL_LA93XX_MODINFO_GET, &mi);
	if (ret < 0) {
		FR1_LOGMSG_ERR("IOCTL_LA9310_MODINFO_GET failed.\n");
		close(fd);
		return -1;
	}
	close(fd);

	return 0;
}

void get_cmd_handle ()
{
	int ret;

	ret=get_mod_info();
	if(ret != 0) {
		perror("Fail to get modem_info \r\n");
		exit(EXIT_FAILURE);
	}

	fd = open("/dev/mem", O_RDWR|O_SYNC);
	if(fd==-1)
		FR1_LOG_ERR("File descriptor open failed for /dev/mem with fd=%d\n",fd);
	FR1_LOG_INFO("File descriptor open success with fd=%d\n",fd);

	long page_size = sysconf(_SC_PAGESIZE);
	FR1_LOG_INFO("Page Size = %ldbytes\n", page_size);

	off_t offset = (mi.rfcal.host_phy_addr & ~(page_size-1)) + page_size;
	long size = NX_MT_RF_SZ;

	FR1_LOG_INFO("RFCAL host_phy_addr addr 0x%lx, Final addr after aligning with pagesize 0x%lx, size 0x%lx\n", mi.rfcal.host_phy_addr, offset, size);
	rf_mdata[0] = (struct rf_dev_mdata *) mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, offset);
	if(rf_mdata[0] == (struct rf_dev_mdata *)MAP_FAILED) {
		FR1_LOG_ERR("mmap of scratch buff to mdata failed, size 0x%x offsetted phy_addr 0x%lx\n", mi.rfcal.size, offset);
		perror("mmap");
		return;
	}
	FR1_LOG_INFO("mmap of scratch buff to mdata succeeded, virt_addr %p size 0x%lx phy_addr 0x%lx\n", rf_mdata[0], size, offset);

	rf_mdata[0]->nx_rf_dev.mt3812_handle = (mt3812_handle_t) &(rf_mdata[0]->mt_mdata);
	rf_mdata[1] = NULL;

	/* Restore the log level */
	rflib_fr1_log_level = rf_mdata[0]->log_level;
	FR1_LOG_INFO("log level restored to - %d\n", rflib_fr1_log_level);

	FR1_LOG_INFO("updating the re entry pointers for device type - %d\n ", FR1_1T2R);
	diora_drv_open_reentry(FR1_1T2R);

	return;
}

RficAPIHandle_t diora_drv_open (FR1DevType devtype)
{
	rf_mdata[0]->magic_word = 0xBAADD00D;
	rf_mdata[0]->flags_state = 0;
	rf_mdata[0]->nx_rf_dev.mt3812_handle = NULL;
	FR1_LOGMSG_INFO( "MT3812_HANDLE is set to NULL \n");

	FR1_LOG_INFO("Init_params address %p\n",&(rf_mdata[0]->nx_rf_dev.init_params));
	FR1_LOG_INFO("Size of init_params 0x%lx\n", sizeof(struct mt3812_init_params));
	memset((uint32_t *)&(rf_mdata[0]->nx_rf_dev.init_params), 0, (sizeof(struct mt3812_init_params))/4);

	FR1_LOGMSG_INFO("JUST before mt3812_drv_open \n");
	FR1_LOG_INFO("MT3812_HANDLE is pointing at %p just before drv_open, size 0x%lx \n",(rf_mdata[0]->nx_rf_dev.mt3812_handle),sizeof(mt3812_handle_t));

	rf_mdata[0]->nx_rf_dev.mt3812_handle = mt3812_drv_open(&(rf_mdata[0]->nx_rf_dev.init_params));

	if (rf_mdata[0]->nx_rf_dev.mt3812_handle) {
		rf_mdata[0]->nx_rf_dev.dev_type = devtype;
		FR1_LOG_INFO( "diora-open mt3812_handle set  -- default: %p \n", rf_mdata[0]->nx_rf_dev.mt3812_handle);
		FR1_LOGMSG_DBG( "diora-open SUCCEED for mt3812 RF card\n");
	}
	else {
		FR1_LOGMSG_ERR( "diora-open FAILED for mt3812 RF card\n" );
		return NULL;
	}

	return &rf_mdata[0]->nx_rf_dev;
}

RficAPIHandle_t diora_drv_open_reentry (FR1DevType devtype)
{
	struct mt_mdata *pdev = (struct mt_mdata *) rf_mdata[0]->nx_rf_dev.mt3812_handle;

	/* LLCP or SPI bus initialization */
	pdev->mt_cmd_rw_device_h = diora_phal_rw_init(0); //params->cmd_periph_rw_id = 0
	if(pdev->mt_cmd_rw_device_h == NULL)
		goto err;

	/* Setup date and version */
	pdev->mt_drv_date = (char*) date_defn;
	pdev->mt_drv_vers = (char*) vers_defn;

	return &rf_mdata[0]->nx_rf_dev;

err:
	diora_osal_free(pdev);
	return NULL;
}



int32_t diora_drv_close (void)
{
	if (rf_mdata[0]) {
        // diora-close handle_local
	    if (!mt3812_drv_close(rf_mdata[0]->nx_rf_dev.mt3812_handle)) {
		    FR1_LOGMSG_DBG( "diora-close SUCCEED for mt3812 RF card1\n" );
		    rf_mdata[0]->nx_rf_dev.mt3812_handle = NULL;
	    }
        else {
		    FR1_LOGMSG_ERR( "diora-close FAILED for mt3812 RF card1\n" );
			munmap((void *)rf_mdata[0], NX_MT_RF_SZ );
			close(fd);
			FR1_LOGMSG_INFO("memory unmap done\n");
            return MT3812_ERROR;
        }
		munmap((void *)rf_mdata[0], NX_MT_RF_SZ );
		FR1_LOGMSG_INFO("memory unmap done\n");
		close(fd);
    }

	return 0;
}

int32_t diora_drv_load (void)
{
	FR1_LOG_DBG("Just before %s call. \n", __func__);

	// diora-load handle_local
	if (mt3812_firmware_load(rf_mdata[0]->nx_rf_dev.mt3812_handle, 
			fw_prog_mem_size_R2_7_001, &fw_prog_mem_R2_7_001[0], 
			fw_init_mem_size_R2_7_001, &fw_init_mem_R2_7_001[0])) {
		FR1_LOGMSG_ERR("diora-load FAILED\n");
        return MT3812_ERROR;
    }
	else {
		FR1_LOGMSG_DBG("diora-load SUCCEED\n");
    }
	return 0;
}

int32_t diora_drv_version (void)
{
	FR1_LOG_DBG("Just before %s call. \n", __func__);

	char* vstr = mt3812_drv_vers(rf_mdata[0]->nx_rf_dev.mt3812_handle);
	char* dstr = mt3812_drv_date(rf_mdata[0]->nx_rf_dev.mt3812_handle);

	// diora-drv handle_local
	if (vstr == NULL || dstr == NULL ) {
		FR1_LOGMSG_ERR("diora-version FAILED. Make sure the driver has been initalized properly\n");
	}
	else {
		FR1_LOG_INFO("\tDriver SDK Version: %s\n\tDriver Build Date: %s\n", vstr, dstr);
	}
	return 0;
}

int32_t diora_drv_read (uint32_t addr)
{
    int32_t iAddr;
    uint16_t usData = 0;

    iAddr = addr;

	FR1_LOG_DBG("Just before %s call. \n", __func__);

    // diora-read handle_local
    if( mt3812_drv_read( rf_mdata[0]->nx_rf_dev.mt3812_handle, iAddr, &usData )) {
    	FR1_LOGMSG_ERR("diora-read FAILED\n");
    }
    else {
    	FR1_LOG_INFO("diora-read addr[0x%04x] = 0x%04x\n",
    		(unsigned int)iAddr, (unsigned int)usData);
    }
    return 0;
    
}

int32_t diora_drv_write (uint32_t addr, const uint16_t val)
{
    int32_t iAddr, iVal;
        
    iAddr = addr;
    iVal = val;

    if (( 0 > iVal ) || ( 0xFFFF < iVal )) {
    	FR1_LOGMSG_ERR("\tEINVAL: Invalid <value> parameter\n");
        return MT3812_EINVPR;
    }

	FR1_LOG_DBG("Just before %s call. \n", __func__);

    // diora-write handle_local
    if( mt3812_drv_write( rf_mdata[0]->nx_rf_dev.mt3812_handle, iAddr, iVal )) {
    	FR1_LOGMSG_ERR("diora-write FAILED\n");
    }
    else {
    	FR1_LOG_INFO("diora-write addr[0x%04x] = 0x%04x\n",
    		(unsigned int)iAddr, (unsigned int)iVal );
    }
    return 0;
}

int32_t rf_adjust_pll_freq (int32_t *pll_freq_args, uint32_t pll_freq_args_size)
{
	struct in_api_param in_p;
	struct out_api_param out_p;
	int32_t freq;
	bool_t cal = 1;
	u16 rtc;
    bool_t precal;

    memset(&in_p, 0, sizeof(struct in_api_param));
    memset(&out_p, 0, sizeof(struct out_api_param));

	in_p.trx_settrxpll.mode  = ((uint8_t *)pll_freq_args)[0];
	freq = (((int32_t *)pll_freq_args)[1])*2;
	in_p.trx_settrxpll.freq_msw =  (u16)(freq >> 16);
	in_p.trx_settrxpll.freq_lsw =  (u16)(freq & 0x0000FFFF);
	// Manage optional parameters
	if (pll_freq_args_size > 5) {
		in_p.trx_settrxpll.vco_sel = ((uint16_t *)pll_freq_args)[2];
        FR1_LOGMSG_DBG(" vco_sel value set \n");
		if (pll_freq_args_size > 6) {
			in_p.trx_settrxpll.cal_cap = ((uint16_t *)pll_freq_args)[3];
            FR1_LOGMSG_DBG(" cal_cap value set \n");
			if (pll_freq_args_size > 7) {
				in_p.trx_settrxpll.cal_current = ((uint16_t *)pll_freq_args)[4];
                FR1_LOGMSG_DBG(" cal_current value set \n");
			} else {
				in_p.trx_settrxpll.cal_current = 0;
				cal = 0; // word3 will not be attached
			}
		} else {
			in_p.trx_settrxpll.cal_cap     = 0;
			in_p.trx_settrxpll.cal_current = 0;
			cal = 0; // word3 will not be attached
		}
	} else {
		in_p.trx_settrxpll.vco_sel     = 7;	// VCO is selected automatically
		in_p.trx_settrxpll.cal_cap     = 0;
		in_p.trx_settrxpll.cal_current = 0;
		cal = 0; // word3 will not be attached
	}

	FR1_LOG_DBG("Just before %s call. Cal = %d \n", __func__, cal);
    // diora-settrxpll handle_local
	if (cal){  // word3 will be attached
	    FR1_LOGMSG_DBG("Just before precal function call. \n");
	    rtc = mt3812_api_TRX_SetTrxPllCal(rf_mdata[0]->nx_rf_dev.mt3812_handle, &in_p, &out_p);}
	else{  // word3 will not be attached
		rtc = mt3812_api_TRX_SetTrxPll(rf_mdata[0]->nx_rf_dev.mt3812_handle, &in_p, &out_p);}
	if (rtc) {
		RTC_MESSAGE( "diora-settrxpll", rtc )
		return rtc;
	}
	FR1_LOG_DBG("Just after %s call \n", __func__);
	
	freq = (out_p.trx_settrxpll.freq_msw << 16) | out_p.trx_settrxpll.freq_lsw;
	FR1_LOG_INFO("\tmode        %d - %s\n", out_p.trx_settrxpll.mode, mode[out_p.trx_settrxpll.mode] );
	FR1_LOG_INFO("\tvco_sel     %d\n", out_p.trx_settrxpll.vco_sel );
	FR1_LOG_INFO("\tfreq_msw    %d\tfreq_lsw    %d\tfreq    %d\n", 
			out_p.trx_settrxpll.freq_msw, out_p.trx_settrxpll.freq_lsw, freq );
	FR1_LOG_INFO("\tcal_cap     %d\n", out_p.trx_settrxpll.cal_cap );
	FR1_LOG_INFO("\tcal_current %d\n", out_p.trx_settrxpll.cal_current );
    
    rf_mdata[0]->nx_rf_dev.rf_lo_freq_khz = (freq)/2;

	return rtc;
}

int32_t rf_get_trx_pll (void)
{
	struct in_api_param in_p;
	struct out_api_param out_p;
	int32_t freq;
	u16 rtc;

    memset(&in_p, 0, sizeof(struct in_api_param));
    memset(&out_p, 0, sizeof(struct out_api_param));

	FR1_LOG_DBG("Just before %s call. \n", __func__);

	// diora-gettrxpll handle_local
	rtc = mt3812_api_TRX_GetTrxPll(rf_mdata[0]->nx_rf_dev.mt3812_handle, &in_p, &out_p);
	if (rtc)
		RTC_MESSAGE( "diora-gettrxpll", rtc )
	else {
		freq = (out_p.trx_gettrxpll.freq_msw << 16) | out_p.trx_gettrxpll.freq_lsw;
		FR1_LOG_INFO("\tmode        %d - %s\n", out_p.trx_gettrxpll.mode, mode[out_p.trx_gettrxpll.mode] ); 
		FR1_LOG_INFO("\tvco_sel     %d\n", out_p.trx_gettrxpll.vco_sel );
		FR1_LOG_INFO("\tfreq_msw    %d\tfreq_lsw    %d\tfreq    %d\n", 
			out_p.trx_gettrxpll.freq_msw, out_p.trx_gettrxpll.freq_lsw, freq );
		FR1_LOG_INFO("\tcal_cap     %d\n", out_p.trx_gettrxpll.cal_cap );
		FR1_LOG_INFO("\tcal_current %d\n", out_p.trx_gettrxpll.cal_current );
	}
	return rtc;
}

int32_t rf_set_txbw (int32_t *txbw_args, int32_t txbw_args_size)
{
	struct in_api_param in_p;
	struct out_api_param out_p;
	bool_t ftune = 1;
	u16 rtc;

    memset(&in_p, 0, sizeof(struct in_api_param));
    memset(&out_p, 0, sizeof(struct out_api_param));

	in_p.trx_settxbw.bw = ((uint8_t *)txbw_args)[0];
	if (txbw_args_size>3) {
		in_p.trx_settxbw.bw_ftune = ((uint8_t *)txbw_args)[1];
		if (txbw_args_size>4)
			in_p.trx_settxbw.vcm_in_low = ((uint8_t *)txbw_args)[2];
		else
			in_p.trx_settxbw.vcm_in_low = 1; /* default active high */
	} else
		ftune = 0;
	if ((in_p.trx_settxbw.bw < TXBW_50M) || (in_p.trx_settxbw.bw > TXBW_50M)) {
		FR1_LOGMSG_ERR( "\tEINVAL: Invalid <bw> parameter\n" );
		FR1_LOGMSG_ERR( "\tbw      [0 .. 0]\n" );
		return MT3812_EINVPR;
	}
	
	FR1_LOG_DBG("Just before %s call. \n", __func__);

	// diora-settxbw handle_local
	if (ftune){
	    FR1_LOGMSG_DBG("Just before precal function call. \n");
		rtc = mt3812_api_TRX_SetTxBwFtune(rf_mdata[0]->nx_rf_dev.mt3812_handle, &in_p, &out_p);}
	else
		rtc = mt3812_api_TRX_SetTxBw(rf_mdata[0]->nx_rf_dev.mt3812_handle, &in_p, &out_p);
	if (rtc)
		RTC_MESSAGE( "diora-settxbw", rtc )
	else{
		FR1_LOG_INFO("\tbw         %d - %s\n", out_p.trx_settxbw.bw, txbw[out_p.trx_settxbw.bw] ); 
		FR1_LOG_INFO("\tbw_ftune   %d\n", out_p.trx_settxbw.bw_ftune );
		FR1_LOG_INFO("\tvcm_in_low %d\n", out_p.trx_settxbw.vcm_in_low );
    }

    rf_mdata[0]->nx_rf_dev.tx_bw = out_p.trx_settxbw.bw;

	return rtc;
}

int32_t rf_get_txbw (void)
{
	struct in_api_param in_p;
	struct out_api_param out_p;
	u16 rtc;

    memset(&in_p, 0, sizeof(struct in_api_param));
    memset(&out_p, 0, sizeof(struct out_api_param));

	FR1_LOG_DBG("Just before %s call. \n", __func__);

	// diora-gettxbw handle_local
	rtc = mt3812_api_TRX_GetTxBw(rf_mdata[0]->nx_rf_dev.mt3812_handle, &in_p, &out_p);
	if (rtc)
		RTC_MESSAGE( "diora-gettxbw", rtc )
	else {
		FR1_LOG_INFO("\tbw         %d - %s\n", out_p.trx_gettxbw.bw, txbw[out_p.trx_gettxbw.bw] ); 
		FR1_LOG_INFO("\tbw_ftune   %d\n", out_p.trx_gettxbw.bw_ftune );
		FR1_LOG_INFO("\tvcm_in_low %d\n", out_p.trx_gettxbw.vcm_in_low );
	}
	return rtc;
}

int32_t rf_set_rxbw (int32_t *rxbw_args, int32_t rxbw_args_size)
{
	struct in_api_param in_p;
	struct out_api_param out_p;
	bool_t ftune = 1;
	u16 rtc;

    memset(&in_p, 0, sizeof(struct in_api_param));
    memset(&out_p, 0, sizeof(struct out_api_param));

	in_p.trx_setrxbw.bw = ((uint8_t *)rxbw_args)[0];
	if (rxbw_args_size>3) {
		in_p.trx_setrxbw.bw_ftune = ((uint8_t *)rxbw_args)[1];
		if (rxbw_args_size>4)
			in_p.trx_setrxbw.bw_coarse = ((uint8_t *)rxbw_args)[2];
		else {
			FR1_LOGMSG_ERR( "\tMissing coarse calibration parameter\n" );
			return MT3812_EINVPR;
		}
	}
	else
		ftune = 0;
	if ((in_p.trx_setrxbw.bw < RXBW_20M) || (in_p.trx_setrxbw.bw > RXBW_50M)) {
		FR1_LOGMSG_ERR( "\tEINVAL: Invalid <bw> parameter\n" );
		FR1_LOGMSG_ERR( "\tbw      [0 .. 4]\n" );
		return MT3812_EINVPR;
	}
		
	FR1_LOG_DBG("Just before %s call. \n", __func__);

	// diora-setrxbw handle_local
	if (ftune){
	    FR1_LOGMSG_DBG("Just before precal function call. \n");
		rtc = mt3812_api_TRX_SetRxBwFtune(rf_mdata[0]->nx_rf_dev.mt3812_handle, &in_p, &out_p);}
	else
		rtc = mt3812_api_TRX_SetRxBw(rf_mdata[0]->nx_rf_dev.mt3812_handle, &in_p, &out_p);
	if (rtc)
		RTC_MESSAGE( "diora-setrxbw", rtc )
	else{
		FR1_LOG_INFO("\tbw         %d - %s\n", out_p.trx_setrxbw.bw, rxbw[out_p.trx_setrxbw.bw] ); 
		FR1_LOG_INFO("\tbw_ftune   %d\n", out_p.trx_setrxbw.bw_ftune );
		FR1_LOG_INFO("\tbw_coarse  %d\n", out_p.trx_setrxbw.bw_coarse );
    }

    rf_mdata[0]->nx_rf_dev.rx_bw = out_p.trx_setrxbw.bw;

	return rtc;
}

int32_t rf_get_rxbw (void)
{
	struct in_api_param in_p;
	struct out_api_param out_p;
	u16 rtc;

    memset(&in_p, 0, sizeof(struct in_api_param));
    memset(&out_p, 0, sizeof(struct out_api_param));

	FR1_LOG_DBG("Just before %s call. \n", __func__);

	// diora-getrxbw handle_local
	rtc = mt3812_api_TRX_GetRxBw(rf_mdata[0]->nx_rf_dev.mt3812_handle, &in_p, &out_p);
	if (rtc)
		RTC_MESSAGE( "diora-getrxbw", rtc )
	else {		
		FR1_LOG_INFO("\tbw         %d - %s\n", out_p.trx_getrxbw.bw, rxbw[out_p.trx_getrxbw.bw] ); 
		FR1_LOG_INFO("\tbw_ftune   %d\n", out_p.trx_getrxbw.bw_ftune );
		FR1_LOG_INFO("\tbw_coarse  %d\n", out_p.trx_getrxbw.bw_coarse);
	}
	return rtc;
}

int32_t rf_set_active (int32_t active_mode, int32_t rx_path)
{
	struct in_api_param in_p;
	struct out_api_param out_p;
	u16 rtc;

	memset(&in_p, 0, sizeof(struct in_api_param));
	memset(&out_p, 0, sizeof(struct out_api_param));

	in_p.trx_setactive.mode = active_mode;
	in_p.trx_setactive.dpd_rx = rx_path;
	if ((in_p.trx_setactive.mode   < ACT_OFF) || (in_p.trx_setactive.mode   > ACT_FDD_RX) ||
		(in_p.trx_setactive.dpd_rx < RX_NONE) || (in_p.trx_setactive.dpd_rx > RX_1_2)) {
		FR1_LOGMSG_ERR( "\tEINVAL: Invalid parameter(s)\n" );
		FR1_LOGMSG_ERR( "\tmode    [0 .. 3]\n" );
		FR1_LOGMSG_ERR( "\tdpd_rx  [0 .. 3]\n" );
		return MT3812_EINVPR;
	}
		
	FR1_LOG_DBG("Just before %s call. \n", __func__);

	// diora-setactive handle_local
	rtc = mt3812_api_TRX_SetActive(rf_mdata[0]->nx_rf_dev.mt3812_handle, &in_p, &out_p);
	if (rtc)
		RTC_MESSAGE( "diora-setactive", rtc )
	else {
		FR1_LOG_INFO("\tmode     %d - %s\n", out_p.trx_setactive.mode,   act_mode[out_p.trx_setactive.mode] );
		FR1_LOG_INFO("\tdpd_rx   %d - %s\n", out_p.trx_setactive.dpd_rx, rx_mode[out_p.trx_setactive.dpd_rx] );
	}
	
	return rtc;
}

int32_t rf_tx_gain_control_relative_diff (int32_t gain_db)
{
    int32_t tx_gain_idx = floor(gain_db/2);

	int32_t curr_tx_gain_db = rf_mdata[0]->nx_rf_dev.tx_gain_db;
	int32_t curr_tx_rf_gain_idx = rf_mdata[0]->nx_rf_dev.tx_rf_gain;

    if(!(curr_tx_gain_db+gain_db >= 0 && curr_tx_gain_db+gain_db <= FR1_TX_MAX_GAIN_DB)) {
		FR1_LOGMSG_ERR( "\tEINVAL: Invalid parameter\n" );
		FR1_LOG_ERR( "\tgain_db    [%d .. %d]\n", -(curr_tx_gain_db), FR1_TX_MAX_GAIN_DB-(curr_tx_gain_db));
        return MT3812_EINVPR;
    }

    if (((curr_tx_rf_gain_idx+tx_gain_idx)>=FR1_TX_MIN_GAIN_IDX) && ((curr_tx_rf_gain_idx+tx_gain_idx)<=FR1_TX_MAX_GAIN_IDX)) {
        rf_set_gain_idx(12, 0, 0, curr_tx_rf_gain_idx+tx_gain_idx);
    }
    else {
		FR1_LOGMSG_ERR( "\tEINVAL: Invalid parameter\n" );
		FR1_LOG_ERR( "\tgain_db    [%d .. %d]\n", -(curr_tx_gain_db), FR1_TX_MAX_GAIN_DB-(curr_tx_gain_db));
    }

    return 0;
}

int32_t rf_rx_gain_control_relative_diff (int32_t gain_db)
{
    int32_t rx_rf_gain_idx;
    int32_t rx_bb_gain_idx;

	int32_t curr_rx_gain_db = rf_mdata[0]->nx_rf_dev.rx_gain_db;

    if(!(curr_rx_gain_db+gain_db >= 0 && curr_rx_gain_db+gain_db <= FR1_RX_MAX_GAIN_DB)) {
		FR1_LOGMSG_ERR( "\tEINVAL: Invalid parameter\n" );
		FR1_LOG_ERR( "\tgain_db    [%d .. %d]\n", -(curr_rx_gain_db), FR1_RX_MAX_GAIN_DB-(curr_rx_gain_db));
        return MT3812_EINVPR;
    }

    if(gain_db >= 0) {
        rx_rf_gain_idx = floor((curr_rx_gain_db+gain_db)/6);
        rx_bb_gain_idx = floor(((curr_rx_gain_db+gain_db)%6)/2);
    }
    else {
        rx_rf_gain_idx = floor((curr_rx_gain_db+gain_db+1)/6);
        rx_bb_gain_idx = floor(((curr_rx_gain_db+gain_db+1)%6)/2);
    }

    if ((rx_rf_gain_idx>=FR1_RX_MIN_RF_GAIN_IDX) && (rx_rf_gain_idx <= FR1_RX_MAX_RF_GAIN_IDX) &&
        (rx_bb_gain_idx>=FR1_RX_MIN_BB_GAIN_IDX) && (rx_bb_gain_idx <= FR1_RX_MAX_BB_GAIN_IDX)) {
        rf_set_gain_idx(3, 1, rx_bb_gain_idx, rx_rf_gain_idx);
    }
    else {
		FR1_LOGMSG_ERR( "\tEINVAL: Invalid parameter\n" );
		FR1_LOG_ERR( "\tgain_db    [%d .. %d]\n", -(curr_rx_gain_db), FR1_RX_MAX_GAIN_DB-(curr_rx_gain_db));
        return MT3812_EINVPR;
    }

    return 0;
}

int32_t rf_set_gain_idx (int32_t rf_path, uint8_t chan, uint32_t bbgain, uint32_t rfgain)
{
	struct in_api_param in_p;
	struct out_api_param out_p;
	u16 rtc;
    char trx_path[25];
    int32_t gain_idx;

    memset(&in_p, 0, sizeof(struct in_api_param));
    memset(&out_p, 0, sizeof(struct out_api_param));

    strcpy(trx_path,trxpath[rf_path]);
	FR1_LOG_DBG("RF Path = %s\n",trx_path);
    if((strstr(trx_path, "RX1")||strstr(trx_path, "RX2")) && !(strstr(trx_path, "TX1")||strstr(trx_path, "TX2"))) {
	    if ((bbgain < 0) || (bbgain > 15) || (rfgain < 0) || (rfgain > 7)) {
		    FR1_LOGMSG_ERR( "\tEINVAL: Invalid parameter(s)\n" );
		    FR1_LOGMSG_ERR( "\tbbgain      [0 .. 15]\n" );
		    FR1_LOGMSG_ERR( "\trfgain      [0 .. 7]\n" );
		    return MT3812_EINVPR;
	    }
        gain_idx = (rfgain<<4 | bbgain);
    }
    else if(!(strstr(trx_path, "RX1")||strstr(trx_path, "RX2")) && (strstr(trx_path, "TX1")||strstr(trx_path, "TX2"))) {
	    if ((bbgain < 0) || (bbgain > 7) || (rfgain < 0) || (rfgain > 31)) {
		    FR1_LOGMSG_ERR( "\tEINVAL: Invalid parameter(s)\n" );
		    FR1_LOGMSG_ERR( "\tbbgain      [0 .. 7]\n" );
		    FR1_LOGMSG_ERR( "\trfgain      [0 .. 31]\n" );
		    return MT3812_EINVPR;
	    }
        gain_idx = rfgain;
    }
    else {
        FR1_LOGMSG_ERR("RF Path Should be either Tx or Rx but not both\n");
        return MT3812_EINVPR;
    }

    in_p.trx_setgain.path = rf_path;
	in_p.trx_setgain.manual = 1; //hard-coded
	in_p.trx_setgain.channel = chan;
	in_p.trx_setgain.hash_mode = 0; //hard-coded
	in_p.trx_setgain.gain = gain_idx;

	if ((in_p.trx_setgain.path      < PATH_NONE) || (in_p.trx_setgain.path > PATH_RX1_RX2_TX1_TX2) ||
		(in_p.trx_setgain.channel   > CH_RX)     ||
		(in_p.trx_setgain.manual    > ON)        ||
		(in_p.trx_setgain.hash_mode > ON))
	{
		FR1_LOGMSG_ERR( "\tEINVAL: Invalid parameter(s)\n");
		FR1_LOGMSG_ERR( "\tpath			[0 .. 15]\n" );	
		FR1_LOGMSG_ERR( "\tchannel  	[0 ..  1]\n" );
		FR1_LOGMSG_ERR( "\tmanual  		[0 ..  1]\n" );
		FR1_LOGMSG_ERR( "\thash_mode	[0 ..  1]\n" );
		return MT3812_EINVPR;
	}

	FR1_LOG_DBG("Just before %s call. \n", __func__);

	// diora-setgain handle_local
	rtc = mt3812_api_TRX_SetGain(rf_mdata[0]->nx_rf_dev.mt3812_handle, &in_p, &out_p);
	if (rtc) {
		RTC_MESSAGE( "diora-setgain", rtc )
		return rtc;
	}

	FR1_LOG_DBG("Just after %s call. \n", __func__);

    FR1_LOG_INFO("\tchannel       %d - %s\n", 
		out_p.trx_setgain.channel, channel[out_p.trx_setgain.channel] );
	FR1_LOG_INFO("\trx1_manual    %d - %s\trx1_hash_mode %d - %s\n",	
		out_p.trx_setgain.rx1_manual,    mode[out_p.trx_setgain.rx1_manual],
		out_p.trx_setgain.rx1_hash_mode, mode[out_p.trx_setgain.rx1_hash_mode] );
	FR1_LOG_INFO( "\trx1_gain      %d\trx1_gain_raw  %d\n", 
		out_p.trx_setgain.rx1_gain, out_p.trx_setgain.rx1_gain_raw);
	FR1_LOG_INFO( "\ttx1_gain      %d\ttx1_gain_raw  %d\n",
		out_p.trx_setgain.tx1_gain, out_p.trx_setgain.tx1_gain_raw );
	FR1_LOG_INFO("\trx2_manual    %d - %s\trx2_hash_mode %d - %s\n", 
		out_p.trx_setgain.rx2_manual,   mode[out_p.trx_setgain.rx2_manual],
		out_p.trx_setgain.rx2_hash_mode, mode[out_p.trx_setgain.rx2_hash_mode] );
	FR1_LOG_INFO( "\trx2_gain      %d\trx2_gain_raw  %d\n", 
		out_p.trx_setgain.rx2_gain, out_p.trx_setgain.rx2_gain_raw);
	FR1_LOG_INFO( "\ttx2_gain      %d\ttx2_gain_raw  %d\n",
		out_p.trx_setgain.tx2_gain, out_p.trx_setgain.tx2_gain_raw );

    if(!(strstr(trx_path, "RX1")||strstr(trx_path, "RX2")) && (strstr(trx_path, "TX1")||strstr(trx_path, "TX2"))) { //For TX
        rf_mdata[0]->nx_rf_dev.tx_rf_gain = out_p.trx_setgain.tx1_gain;
	    FR1_LOG_DBG("Tx Gain = %d Expected = %d BBgain = %d RFgain = %d\n", out_p.trx_setgain.tx1_gain, gain_idx, bbgain, rfgain);
		rf_mdata[0]->nx_rf_dev.tx_gain_db = (rf_mdata[0]->nx_rf_dev.tx_rf_gain)*2;
    }
    else { //For RX
        rf_mdata[0]->nx_rf_dev.rx_rf_gain = rfgain;
        rf_mdata[0]->nx_rf_dev.rx_bb_gain = bbgain;
	    FR1_LOG_DBG("Rx Gain = %d Expected = %d BBgain = %d RFgain = %d\n", out_p.trx_setgain.rx1_gain, gain_idx, bbgain, rfgain);
		rf_mdata[0]->nx_rf_dev.rx_gain_db = (rf_mdata[0]->nx_rf_dev.rx_bb_gain)*2+(rf_mdata[0]->nx_rf_dev.rx_rf_gain)*6;
    }

	return rtc;
}

int32_t rf_get_gain_idx (void)
{
	struct in_api_param in_p;
	struct out_api_param out_p;
	u16 rtc;

    memset(&in_p, 0, sizeof(struct in_api_param));
    memset(&out_p, 0, sizeof(struct out_api_param));

	FR1_LOG_DBG("Just before %s call. \n", __func__);

	// diora-getgain handle_local
	rtc = mt3812_api_TRX_GetGain(rf_mdata[0]->nx_rf_dev.mt3812_handle, &in_p, &out_p);
	if (rtc) {
		RTC_MESSAGE( "diora-getgain", rtc )
		return rtc;
	}
	FR1_LOG_INFO("\tchannel       %d - %s\n", 
			out_p.trx_getgain.channel, channel[out_p.trx_getgain.channel] );
	FR1_LOG_INFO("\trx1_manual    %d - %s\trx1_hash_mode %d - %s\n", 
			out_p.trx_getgain.rx1_manual,    mode[out_p.trx_getgain.rx1_manual],
			out_p.trx_getgain.rx1_hash_mode, mode[out_p.trx_getgain.rx1_hash_mode] );
	FR1_LOG_INFO( "\trx1_gain      %d\trx1_gain_raw  %d\n", 
			out_p.trx_getgain.rx1_gain, out_p.trx_getgain.rx1_gain_raw);
	FR1_LOG_INFO( "\ttx1_gain      %d\ttx1_gain_raw  %d\n",
			out_p.trx_getgain.tx1_gain, out_p.trx_getgain.tx1_gain_raw );
	FR1_LOG_INFO("\trx2_manual    %d - %s\trx2_hash_mode %d - %s\n", 
			out_p.trx_getgain.rx2_manual,   mode[out_p.trx_getgain.rx2_manual],
			out_p.trx_getgain.rx2_hash_mode, mode[out_p.trx_getgain.rx2_hash_mode] );
	FR1_LOG_INFO( "\trx2_gain      %d\trx2_gain_raw  %d\n", 
			out_p.trx_getgain.rx2_gain, out_p.trx_getgain.rx2_gain_raw);
	FR1_LOG_INFO( "\ttx2_gain      %d\ttx2_gain_raw  %d\n",
			out_p.trx_getgain.tx2_gain, out_p.trx_getgain.tx2_gain_raw );


	return rtc;
}

int32_t rf_set_path (int32_t rf_path, int32_t freq_band, int32_t rssi, int32_t dpd, int32_t rx_bw, int32_t tx_bw)
{
	struct in_api_param in_p;
	struct out_api_param out_p;
	u16 rtc;

    memset(&in_p, 0, sizeof(struct in_api_param));
    memset(&out_p, 0, sizeof(struct out_api_param));

	FR1_LOG_DBG("Just before %s call. \n", __func__);

	in_p.trx_setpath.path = rf_path;
	in_p.trx_setpath.band = freq_band;
	in_p.trx_setpath.rssi_mode = rssi;
	in_p.trx_setpath.dpd_mode = dpd;
	in_p.trx_setpath.rxbw = rx_bw;
	in_p.trx_setpath.txbw = tx_bw;
	
	if ((in_p.trx_setpath.path      < PATH_NONE) || (in_p.trx_setpath.path      > PATH_RX1_RX2_TX1_TX2) ||
		(in_p.trx_setpath.band      < BAND_AUTO) || (in_p.trx_setpath.band      > BAND_LB)              ||
		(in_p.trx_setpath.rssi_mode < RX_NONE)   || (in_p.trx_setpath.rssi_mode > RX_1_2)               ||
		(in_p.trx_setpath.dpd_mode  < DPD_NONE)  || (in_p.trx_setpath.dpd_mode  > DPD_RX1_RX2)          ||
		(in_p.trx_setpath.rxbw      < RXBW_20M)  || (in_p.trx_setpath.rxbw      > RXBW_50M)            ||
		(in_p.trx_setpath.txbw      < TXBW_50M)  || (in_p.trx_setpath.txbw      > TXBW_50M)) {

		FR1_LOGMSG_ERR("\tEINVAL: Invalid parameter(s)\n");
		FR1_LOGMSG_ERR("\tpath		[0 .. 15]\n" );
		FR1_LOGMSG_ERR("\tband  	[0 ..  3]\n" );
		FR1_LOGMSG_ERR("\trssi_mode	[0 ..  3]\n" );
		FR1_LOGMSG_ERR("\tdpd_mode	[0 ..  5]\n" );
		FR1_LOGMSG_ERR("\trx    	[0 ..  4]\n" );
		FR1_LOGMSG_ERR("\ttx	    [0 ..  0]\n" );
		return MT3812_EINVPR;
	}

	// diora-setpath handle_local
	rtc = mt3812_api_TRX_SetPath(rf_mdata[0]->nx_rf_dev.mt3812_handle, &in_p, &out_p);
	if (rtc) {
		RTC_MESSAGE( "diora-setpath", rtc )
		return rtc;
	}

	FR1_LOG_INFO("\tpath      %d\tband     %d - %s\n", 
		out_p.trx_setpath.path, out_p.trx_setpath.band, band[out_p.trx_setpath.band] ); 
	FR1_LOG_INFO("\trssi_mode %d - %s\tdpd_mode  %d - %s\n", 
		out_p.trx_setpath.rssi_mode, rx_mode[out_p.trx_setpath.rssi_mode], 
		out_p.trx_setpath.dpd_mode, dpd_mode[out_p.trx_setpath.dpd_mode] );
	FR1_LOG_INFO("\trxbw      %d - %s\ttxbw      %d - %s\n", 
		out_p.trx_setpath.rxbw, rxbw[out_p.trx_setpath.rxbw], 
		out_p.trx_setpath.txbw, txbw[out_p.trx_setpath.txbw] );

	if(rf_path)
		rf_mdata[0]->nx_rf_dev.trx_path = out_p.trx_setpath.path;

	return rtc;
}

int32_t rf_get_path (void)
{
	struct in_api_param in_p;
	struct out_api_param out_p;
	u16 rtc;

    memset(&in_p, 0, sizeof(struct in_api_param));
    memset(&out_p, 0, sizeof(struct out_api_param));

	FR1_LOG_DBG("Just before %s call. \n", __func__);

	// diora-getpath handle_local
	rtc = mt3812_api_TRX_GetPath(rf_mdata[0]->nx_rf_dev.mt3812_handle, &in_p, &out_p);
	if (rtc) {
		RTC_MESSAGE( "diora-getpath", rtc )
		return rtc;
	}
			
	FR1_LOG_INFO("\tpath      %d\t\tband      %d - %s\n", 
		out_p.trx_getpath.path, out_p.trx_getpath.band, band[out_p.trx_getpath.band] ); 
	FR1_LOG_INFO("\trssi_mode %d - %s\tdpd_mode  %d - %s\n", 
		out_p.trx_getpath.rssi_mode, rx_mode[out_p.trx_getpath.rssi_mode], 
		out_p.trx_getpath.dpd_mode, dpd_mode[out_p.trx_getpath.dpd_mode] );
	FR1_LOG_INFO("\trxbw      %d - %s\ttxbw      %d - %s\n", 
		out_p.trx_getpath.rxbw, rxbw[out_p.trx_getpath.rxbw], 
		out_p.trx_getpath.txbw, txbw[out_p.trx_getpath.txbw] );
	return rtc;
}

int32_t rf_set_bbloop (int32_t rx_path, int32_t tx_bw, int32_t rx_bw)
{
	struct in_api_param in_p;
	struct out_api_param out_p;
	u16 rtc;

    memset(&in_p, 0, sizeof(struct in_api_param));
    memset(&out_p, 0, sizeof(struct out_api_param));

	in_p.trx_setbbloopback.receiver = rx_path;
	in_p.trx_setbbloopback.txbw = tx_bw;
	in_p.trx_setbbloopback.rxbw = rx_bw;
	if ((in_p.trx_setbbloopback.receiver < RX_NONE) || (in_p.trx_setbbloopback.receiver > RX_1_2) ||
		(in_p.trx_setbbloopback.txbw < TXBW_50M)    || (in_p.trx_setbbloopback.txbw > TXBW_50M)  ||
		(in_p.trx_setbbloopback.rxbw < RXBW_20M)    || (in_p.trx_setbbloopback.rxbw > RXBW_50M)) {
		FR1_LOGMSG_ERR( "\tEINVAL: Invalid parameter(s)\n" );
		FR1_LOGMSG_ERR( "\treceiver\t[0 ..  3]\n" );
		FR1_LOGMSG_ERR( "\ttxbw\t\t[0 ..  0]\n" );
		FR1_LOGMSG_ERR( "\trxbw\t\t[0 ..  4]\n" );
		return MT3812_EINVPR;
	}

	FR1_LOG_DBG("Just before %s call. \n", __func__);

	// diora-setbbloop handle_local
	rtc = mt3812_api_TRX_SetBbLoopBack(rf_mdata[0]->nx_rf_dev.mt3812_handle, &in_p, &out_p);
	if (rtc)
		RTC_MESSAGE( "diora-setbbloop", rtc )
	else
		FR1_LOGMSG_DBG("diora-setbbloop SUCCEED\n");
	return rtc;
}

int32_t rf_set_cal_pll (int32_t *cal_pll_args, uint32_t cal_pll_args_size)
{
	struct in_api_param in_p;
	struct out_api_param out_p;
	int32_t freq;
	bool_t band = 1;
	u16 rtc;

    memset(&in_p, 0, sizeof(struct in_api_param));
    memset(&out_p, 0, sizeof(struct out_api_param));

	in_p.trx_setcalpll.mode = ((uint8_t *)cal_pll_args)[0];
	freq = (((int32_t *)cal_pll_args)[1])*2;
	in_p.trx_setcalpll.freq_msw =  (u16)(freq >> 16);
	in_p.trx_setcalpll.freq_lsw =  (u16)(freq & 0x0000FFFF);
	if (cal_pll_args_size>5)
		in_p.trx_setcalpll.cal_band = ((uint8_t *)cal_pll_args)[2];
	else {
		band = 0; // word 3 will not be attached
		in_p.trx_setcalpll.cal_band = BAND_AUTO;
	}

	if ((in_p.trx_setcalpll.mode < OFF) || (in_p.trx_setcalpll.mode > ON) ||
		(in_p.trx_setcalpll.cal_band > 15)) {
		FR1_LOGMSG_ERR( "\tEINVAL: Invalid parameter(s)\n" );
		FR1_LOGMSG_ERR( "\tmode    [0 ..  1]\n" );
		FR1_LOGMSG_ERR( "\tband    [0 .. 15]\n" );
		return MT3812_EINVPR;
	}
	
    FR1_LOG_DBG("Just before %s call. \n", __func__);

	// diora-Setcalpll handle_local
	if (band) {
	    FR1_LOGMSG_DBG("Just before precal function call. \n");
	    rtc = mt3812_api_TRX_SetCalPllBand(rf_mdata[0]->nx_rf_dev.mt3812_handle, &in_p, &out_p); // word 3 will be attached
	} else {
		rtc = mt3812_api_TRX_SetCalPll(rf_mdata[0]->nx_rf_dev.mt3812_handle, &in_p, &out_p); // word 3 will not be attached
	}
	if (rtc) {
		RTC_MESSAGE( "diora-setcalpll", rtc )
		return rtc;
	}

	freq = (out_p.trx_setcalpll.freq_msw << 16) | out_p.trx_setcalpll.freq_lsw;
	FR1_LOG_INFO("\tmode     %d - %s\n", out_p.trx_setcalpll.mode, mode[out_p.trx_setcalpll.mode] );
	FR1_LOG_INFO("\tfreq_msw %d\tfreq_lsw %d\tfreq    %d\n",
		out_p.trx_setcalpll.freq_msw, out_p.trx_setcalpll.freq_lsw, freq );
	FR1_LOG_INFO("\tcal_band %d\n", out_p.trx_setcalpll.cal_band );
	return rtc;
}

int32_t rf_get_cal_pll(void)
{
	struct in_api_param in_p;
	struct out_api_param out_p;
	int32_t freq;
	u16 rtc;

    memset(&in_p, 0, sizeof(struct in_api_param));
    memset(&out_p, 0, sizeof(struct out_api_param));

	FR1_LOG_DBG("Just before %s call. \n", __func__);

	// diora-getcalpll handle_local
	rtc = mt3812_api_TRX_GetCalPll(rf_mdata[0]->nx_rf_dev.mt3812_handle, &in_p, &out_p);
	if (rtc) {
		RTC_MESSAGE( "diora-getcalpll", rtc )
		return rtc;
	}

	freq = (out_p.trx_getcalpll.freq_msw << 16) | out_p.trx_getcalpll.freq_lsw;
	FR1_LOG_INFO("\tmode     %d - %s\n", out_p.trx_getcalpll.mode, mode[out_p.trx_getcalpll.mode] );
	FR1_LOG_INFO("\tfreq_msw %d\tfreq_lsw %d\tfreq    %d\n",
		out_p.trx_getcalpll.freq_msw, out_p.trx_getcalpll.freq_lsw, freq );
	FR1_LOG_INFO("\tcal_band %d\n", out_p.trx_getcalpll.cal_band);
	return rtc;
}

int32_t rf_set_rfloop (int32_t rx_path, int32_t freq_band, int32_t rf_loop)
{
	struct in_api_param in_p;
	struct out_api_param out_p;
	u16 rtc;

    memset(&in_p, 0, sizeof(struct in_api_param));
    memset(&out_p, 0, sizeof(struct out_api_param));

	in_p.trx_setrfloopback.receiver  = rx_path;
	in_p.trx_setrfloopback.band      = freq_band;
	in_p.trx_setrfloopback.loop_mode = rf_loop;
	if ((in_p.trx_setrfloopback.receiver  < RX_NONE)        || (in_p.trx_setrfloopback.receiver > RX_1_2)  ||
		(in_p.trx_setrfloopback.band      < BAND_AUTO)      || (in_p.trx_setrfloopback.band     > BAND_LB) ||
		(in_p.trx_setrfloopback.loop_mode < LOOP_TXBB2RXRF) || (in_p.trx_setrfloopback.loop_mode > LOOP_TXRF2RXBB) ) {
		FR1_LOGMSG_ERR( "\tEINVAL: Invalid parameter(s)\n" );
		FR1_LOGMSG_ERR( "\treceiver\t[0 .. 3]\n" );
		FR1_LOGMSG_ERR( "\tband\t\t[0 .. 3]\n" );
		FR1_LOGMSG_ERR( "\trf_loop\t\t[0 .. 1]\n" );
		return MT3812_EINVPR;
	}
	
    FR1_LOG_DBG("Just before %s call. \n", __func__);

	// diora-setrfloop handle_local
	rtc = mt3812_api_TRX_SetRfLoopBack(rf_mdata[0]->nx_rf_dev.mt3812_handle, &in_p, &out_p);
	if (rtc)
		RTC_MESSAGE( "diora-setrfloop", rtc )
	else
		FR1_LOGMSG_DBG("diora-setrfloop SUCCEED\n");
	return rtc;
}

int32_t rf_app_get_sysstatus (void)
{
	struct in_api_param in_p;
	struct out_api_param out_p;
	u16 rtc;

	memset(&in_p, 0, sizeof(struct in_api_param));
	memset(&out_p, 0, sizeof(struct out_api_param));

	FR1_LOG_DBG("Address of handle is %p\n",rf_mdata[0]->nx_rf_dev.mt3812_handle);

	// diora-getsysstatus handle_local
	rtc = mt3812_api_APP_GetSysStatus(rf_mdata[0]->nx_rf_dev.mt3812_handle, &in_p, &out_p);
	if (rtc)
		RTC_MESSAGE( "diora-getsysstatus", rtc )
	else {
		FR1_LOG_INFO("\tsys_state           %d - %s\n", out_p.app_getsysstatus.sys_state, sys_state[out_p.app_getsysstatus.sys_state] ); 
		FR1_LOG_INFO("\tcapll_unlock        %d\n", out_p.app_getsysstatus.calpll_unlock ); 
		FR1_LOG_INFO("\tcapll_vtune_det_hi  %d\tcapll_vtune_det_lo  %d\n", 
			out_p.app_getsysstatus.calpll_vtune_det_hi, out_p.app_getsysstatus.calpll_vtune_det_lo ); 
		FR1_LOG_INFO("\ttrxpll_unlock       %d\n", out_p.app_getsysstatus.trxpll_unlock );   
		FR1_LOG_INFO("\ttrxpll_vtune_det_hi %d\ttrxpll_vtune_det_lo %d\n", 
			out_p.app_getsysstatus.trxpll_vtune_det_hi, out_p.app_getsysstatus.trxpll_vtune_det_lo );
		FR1_LOG_INFO("\ttrxpll_vco_det_hi   %d\ttrxpll_vco_det_lo   %d\n", 
			out_p.app_getsysstatus.trxpll_vco_det_hi, out_p.app_getsysstatus.trxpll_vco_det_lo );
	}
	return rtc;
}

int32_t rf_app_get_temperature (void)
{
	struct in_api_param in_p;
	struct out_api_param out_p;
	u16 rtc;

    memset(&in_p, 0, sizeof(struct in_api_param));
    memset(&out_p, 0, sizeof(struct out_api_param));

	FR1_LOG_DBG("Just before %s call. \n", __func__);

	// diora-gettemperature handle_local
	rtc = mt3812_api_APP_GetTemperature(rf_mdata[0]->nx_rf_dev.mt3812_handle, &in_p, &out_p);
	if (rtc)
		RTC_MESSAGE( "diora-gettemperature", rtc )
	else {
		// Negative values management - Two's complement format is used for negative values
		// values are coded on 16 bits (0xFFFF). Offset to apply to negative values = 0x10000
		s32 temperature = out_p.app_gettemperature.temperature;
		s32 value_raw = out_p.app_gettemperature.value_raw;
		if (temperature > 0x8000)
			temperature -= 0x10000;
		if (value_raw > 0x8000)
			value_raw -= 0x10000;
		FR1_LOG_INFO("\ttemperature %d\n", temperature );
		FR1_LOG_INFO("\tvalue_raw   %d\n", value_raw );
	}
	return rtc;
}

int32_t rf_app_cal_resistor (int32_t *cal_resistor_args, int32_t cal_resistor_args_size)
{
	struct in_api_param in_p;
	struct out_api_param out_p;
	u16 rtc;

    memset(&in_p, 0, sizeof(struct in_api_param));
    memset(&out_p, 0, sizeof(struct out_api_param));

	FR1_LOG_DBG("Just before %s call. \n", __func__);

	if (cal_resistor_args_size) {
	    in_p.app_calresistor.rcal_value = ((uint32_t *)cal_resistor_args)[0];
	    // If an input value is given, this value is used and no calibration is performed
	    // diora-calresistor handle_local
	    FR1_LOGMSG_DBG("Just before precal function call. \n");
	    rtc = mt3812_api_APP_CalResistor(rf_mdata[0]->nx_rf_dev.mt3812_handle, &in_p, &out_p);
	} else {
		// If no input value is given, the calibration is performed
		// diora-calresistor handle_local
		rtc = mt3812_api_APP_CalResistorCalib(rf_mdata[0]->nx_rf_dev.mt3812_handle, &in_p, &out_p);
	}

	if (rtc)
		RTC_MESSAGE( "diora-calresistor", rtc )
	else
		FR1_LOG_INFO("\trcal_value %d\n", out_p.app_calresistor.rcal_value );
	return rtc;
}

int32_t rf_app_cal_regulator (void)
{
	struct in_api_param in_p;
	struct out_api_param out_p;
	u16 rtc;

    memset(&in_p, 0, sizeof(struct in_api_param));
    memset(&out_p, 0, sizeof(struct out_api_param));

	FR1_LOG_DBG("Just before %s call. \n", __func__);

	// diora-calregulator handle_local
	rtc = mt3812_api_APP_CalRegulator(rf_mdata[0]->nx_rf_dev.mt3812_handle, &in_p, &out_p);
	if (rtc)
		RTC_MESSAGE( "diora-calregulator", rtc )
	else
		FR1_LOG_INFO("\tvcal_value %d\n", out_p.app_calregulator.vcal_value);
	return rtc;
}

int32_t diora_gen_get_version (void)
{
	struct in_api_param in_p;
	struct out_api_param out_p;
	u16 rtc;

    memset(&in_p, 0, sizeof(struct in_api_param));
    memset(&out_p, 0, sizeof(struct out_api_param));

	FR1_LOG_DBG("Just before %s call. \n", __func__);

	// diora-getversion handle_local
	rtc = mt3812_api_GEN_GetVersion(rf_mdata[0]->nx_rf_dev.mt3812_handle, &in_p, &out_p);
	if (rtc) {
		RTC_MESSAGE( "diora-getversion", rtc )
		return rtc;
	}
	
	FR1_LOG_INFO("\thw_version 0x%04x\n", out_p.gen_getversion.hw_version ); 
	FR1_LOG_INFO("\tminor_ver  %d\tmajor_ver  %d\ttarget_ver  %d\n", 
			out_p.gen_getversion.minor_ver, out_p.gen_getversion.major_ver, out_p.gen_getversion.target_ver ); 
	FR1_LOG_INFO("\tbuild_nr   %d\ttest_nr   %d\n", 
			out_p.gen_getversion.build_nr, out_p.gen_getversion.test_nr );
	FR1_LOG_INFO("\tdebug_flag %d\n", out_p.gen_getversion.debug_flag );
	return rtc;
}

int32_t diora_gen_set_register (int32_t *set_register_args)
{
	int32_t ii;
	struct in_api_param in_p;
	struct out_api_param out_p;
	u16 rtc;

    memset(&in_p, 0, sizeof(struct in_api_param));
    memset(&out_p, 0, sizeof(struct out_api_param));

	in_p.gen_setregister.address = ((uint32_t *)set_register_args)[0];
	
	for (ii = 1; ii < GEN_SET_REGISTER_CMD_WORDS ; ii++) {
		in_p.gen_setregister.value[ii-1] = ((uint32_t *)set_register_args)[ii];
	}
	
	FR1_LOG_DBG("Just before %s call. \n", __func__);

	// diora-setregister handle_local	        
	rtc = mt3812_api_GEN_SetRegister(rf_mdata[0]->nx_rf_dev.mt3812_handle, &in_p, &out_p);
	if (rtc)
		RTC_MESSAGE( "diora-setregister", rtc )
	else
		FR1_LOGMSG_DBG("diora-setregister SUCCEED\n");
	return rtc;
}

int32_t diora_gen_get_register (uint32_t addr, uint32_t len)
{
	struct in_api_param in_p;
	struct out_api_param out_p;
	int ii;
	u16 rtc;

    memset(&in_p, 0, sizeof(struct in_api_param));
    memset(&out_p, 0, sizeof(struct out_api_param));

	in_p.gen_getregister.address = addr; 
	in_p.gen_getregister.length  = len;

	if (in_p.gen_getregister.length <= 0 || in_p.gen_getregister.length >= 9) {
		FR1_LOGMSG_ERR( "\tEINVAL: Invalid <length> parameter\n" );
	    FR1_LOGMSG_ERR( "\tlength  [1 .. 8]\n" );
	    return MT3812_EINVPR;
	}
	
	FR1_LOG_DBG("Just before %s call. \n", __func__);
    FR1_LOG_INFO( "mt3812_handle set  -- default: %p \n", rf_mdata[0]->nx_rf_dev.mt3812_handle);

	// diora-getregister handle_local
	rtc = mt3812_api_GEN_GetRegister(rf_mdata[0]->nx_rf_dev.mt3812_handle, &in_p, &out_p);
	if (rtc) {
		RTC_MESSAGE( "diora-getregister", rtc )
		return rtc;
	}
	     
	FR1_LOG_INFO( "diora-getregister Reg Addr[0x%04x] \n", in_p.gen_getregister.address );
	for (ii = 0; ii < in_p.gen_getregister.length; ii++)
			FR1_LOG_INFO( "0x%04x \n", out_p.gen_getregister.value[ii] );
	FR1_LOGMSG_INFO( "\n" );
	return rtc;
}

int32_t diora_gen_set_register_masked (uint32_t addr, uint32_t val, uint32_t mask)
{
	struct in_api_param in_p;
	struct out_api_param out_p;
	u16 rtc;

    memset(&in_p, 0, sizeof(struct in_api_param));
    memset(&out_p, 0, sizeof(struct out_api_param));

	in_p.gen_setregistermasked.address = addr;
	in_p.gen_setregistermasked.value = val;
	in_p.gen_setregistermasked.mask = mask;
		
	FR1_LOG_DBG("Just before %s call. \n", __func__);

	// diora-setregistermasked handle_local
	rtc = mt3812_api_GEN_SetRegisterMasked(rf_mdata[0]->nx_rf_dev.mt3812_handle, &in_p, &out_p);
	if (rtc)
		RTC_MESSAGE( "diora-setregistermasked", rtc )
	else
		FR1_LOGMSG_DBG("diora-setregistermasked SUCCEED\n");
	return rtc;	
}

int32_t diora_gen_set_property (int32_t *set_property_args)
{
	struct in_api_param in_p;
	struct out_api_param out_p;
	s32 property_value;
	u16 rtc;

    memset(&in_p, 0, sizeof(struct in_api_param));
    memset(&out_p, 0, sizeof(struct out_api_param));

	in_p.gen_setproperty.property_id = ((uint32_t *)set_property_args)[0];
	property_value = ((uint32_t *)set_property_args)[1];
	// Negative values management - Two's complement format is used for negative values
	// property values are  coded on 16 bits (0xFFFF). Offset to apply to negative values = 0x10000
	if (property_value < 0)
		property_value += 0x10000;
	in_p.gen_setproperty.value[0] = property_value;

	FR1_LOG_DBG("Just before %s call. \n", __func__);

	// diora-setproperty handle_local
	rtc = mt3812_api_GEN_SetProperty(rf_mdata[0]->nx_rf_dev.mt3812_handle, &in_p, &out_p);
	if (rtc)
		RTC_MESSAGE( "diora-setproperty", rtc )
	else
		FR1_LOGMSG_DBG("diora-setproperty SUCCEED\n");
	return rtc;
}

int32_t diora_gen_get_property (uint32_t property, uint32_t len)
{
	struct in_api_param in_p;
	struct out_api_param out_p;
	int ii;
	u16 rtc;

    memset(&in_p, 0, sizeof(struct in_api_param));
    memset(&out_p, 0, sizeof(struct out_api_param));

	in_p.gen_getproperty.property_id = property;
	in_p.gen_getproperty.length  = len;

	if (in_p.gen_getproperty.length <= 0 || in_p.gen_getproperty.length >= 9) {
		FR1_LOGMSG_ERR( "\tEINVAL: Invalid <length> parameter\n" );
		FR1_LOGMSG_ERR( "\tlength  [1 .. 8]\n" );
		return MT3812_EINVPR;
	}
		
	FR1_LOG_DBG("Just before %s call. \n", __func__);

	 // diora-getproperty handle_local
	rtc = mt3812_api_GEN_GetProperty(rf_mdata[0]->nx_rf_dev.mt3812_handle, &in_p, &out_p);
	if (rtc)
		RTC_MESSAGE( "diora-getproperty", rtc )
	else {
		s32 property_value;
		FR1_LOG_INFO( "diora-getproperty id [%d] ", in_p.gen_getproperty.property_id );
		for (ii = 0; ii < in_p.gen_getproperty.length; ii++) {
			property_value = out_p.gen_getproperty.value[ii];
			// Negative values management - Two's complement format is used for negative values
			// values are coded on 16 bits (0xFFFF). Offset to apply to negative values = 0x10000
			if (property_value > 0x8000)
				property_value -= 0x10000;
			FR1_LOG_INFO( "%d ",  property_value);
		}
		FR1_LOGMSG_INFO( "\n" );
	}
	return rtc;
}

//#ifndef MT3812_BPACK
#if 1
int32_t diora_calib_bw (int32_t chan, int32_t freq_band, int32_t freq, int32_t rx_bw, int32_t tx_bw)
{
	FR1_LOGMSG_ERR("\tEINVAL: diora_calib_bw not supported\n");
	return 0;
}
int32_t diora_calib_rxdc (int32_t *calib_rxdc_args, uint32_t calib_rxdc_args_size)
{
	FR1_LOGMSG_ERR("\tEINVAL: diora_calib_rxdc not supported\n");
	return 0;
}
#else
#define BUF_SZ 16384	// 16K
c16   txbuf[BUF_SZ] __attribute__((section(".hram")));
c16  txbuf2[BUF_SZ] __attribute__((section(".hram")));
c16   rxbuf[BUF_SZ] __attribute__((section(".hram")));
float res_r_re[BUF_SZ]  __attribute__((section(".hram")));
float res_r_im[BUF_SZ]  __attribute__((section(".hram")));
float res_i_re[BUF_SZ]  __attribute__((section(".hram")));
float res_i_im[BUF_SZ]  __attribute__((section(".hram")));
float t_buf[BUF_SZ]     __attribute__((section(".hram")));

#define CHAN1 0 
#define CHAN2 1

int32_t diora_calib_bw (int32_t chan, int32_t freq_band, int32_t freq, int32_t rx_bw, int32_t tx_bw)
{
	struct bw_params_output out_p;
	band_t band = BAND_MB;
	u16 rtc;
	struct calib_bw_config in_p;

	if ((chan < CHAN1) || (chan > CHAN2)) {
		FR1_LOGMSG_ERR("\tEINVAL: Invalid <channel> parameter\n");
		FR1_LOGMSG_ERR("\tchannel      [0 .. 1]\n");
		return MT3812_EINVPR;
	}
	band = freq_band;
	if ((band < BAND_AUTO) || (band > BAND_LB)) {
		FR1_LOGMSG_ERR("\tEINVAL: Invalid <band> parameter\n");
		FR1_LOGMSG_ERR("\tband    [0 .. 3]\n");
		return MT3812_EINVPR;
	}
	if (rx_bw > RXBW_50M) {
		FR1_LOGMSG_ERR("\tEINVAL: Invalid <rxbw> parameter\n");
		FR1_LOGMSG_ERR("\trxbw    [0 .. 4]\n");
		return MT3812_EINVPR;
	}
	if (tx_bw > TXBW_50M) {
		FR1_LOGMSG_ERR("\tEINVAL: Invalid <txbw> parameter\n");
		FR1_LOGMSG_ERR("\ttxbw    [0 .. 0]\n");
		return MT3812_EINVPR;
	}

	memset(&in_p, 0, sizeof(struct calib_bw_config));

	// diora-calib_bw handle_local
	// input parameters
	in_p.dac_rate = SAMPLERATE_491;
	in_p.adc_rate = SAMPLERATE_245;
	in_p.txbuf  = &txbuf[0];
	in_p.txbuf2 = &txbuf2[0];
	in_p.rxbuf  = &rxbuf[0];
	in_p.rxbw   = rx_bw;
	in_p.txbw   = tx_bw;
	in_p.select_way = 0;  // TX & RX
	

	// output parameters
	memset(&out_p, 0, sizeof(struct bw_params_output));

	FR1_LOG_DBG("Just before %s call. \n", __func__);

	rtc = mt3812_calib_bw(rf_mdata[0]->nx_rf_dev.mt3812_handle, freq*2, chan, freq_band, &in_p, &out_p);
	if (rtc) {
		RTC_MESSAGE("diora-calibbw", rtc)
		return rtc;
	}
	
	FR1_LOGMSG_INFO("BW ---------------------------------------------------------------------------\n");
	if (channel == CHAN1) {
		FR1_LOG_INFO("\tTX_1 - SetTxBw [bw, bw_ftune, vcm_in_low] [%d, %d, %d]\n", 
				tx_bw, out_p.txbw_ftune, out_p.tx_vcm_in_low);
		FR1_LOG_INFO("\tRX_1 - SetRxBw [bw, bw_ftune, coarse]     [%d, %d, %d]\n", 
				rx_bw, out_p.rxbw_ftune, out_p.rxbw_coarse);
	} else { // channel = CHAN2)
		FR1_LOG_INFO("\tTX_2 - SetTxBw [bw, bw_ftune, vcm_in_low] [%d, %d, %d]\n", 
				tx_bw, out_p.txbw_ftune, out_p.tx_vcm_in_low);
		FR1_LOG_INFO("\tRX_2 - SetRxBw [bw, bw_ftune, coarse]     [%d, %d, %d]\n", 
				rx_bw, out_p.rxbw_ftune, out_p.rxbw_coarse);
	}
	
	return rtc;
}

int32_t diora_calib_rxdc (int32_t *calib_rxdc_args, uint32_t calib_rxdc_args_size)
{
	struct rxdc_params_output out_p[2];
	u32 freq = 7000000;
	receiver_t rx = RX_1;
	band_t band = BAND_MB;
	u16 rtc;
	struct calib_rxdc_config in_p;
	u32 ii, jj;

	memset(&in_p, 0, sizeof(struct calib_rxdc_config));

	rx = ((uint8_t *)calib_rxdc_args)[0];
	if ((rx < RX_1) || (rx > RX_1_2)) {
		FR1_LOGMSG_ERR("\tEINVAL: Invalid <rx> parameter\n");
		FR1_LOGMSG_ERR("\trx      [1 .. 3]\n");
		return MT3812_EINVPR;
	}
	band = ((uint8_t *)calib_rxdc_args)[1];
	if ((band < BAND_AUTO) || (band > BAND_LB)) {
		FR1_LOGMSG_ERR("\tEINVAL: Invalid <band> parameter\n");
		FR1_LOGMSG_ERR("\tband    [0 .. 3]\n");
		return MT3812_EINVPR;
	}
	freq = (((uint32_t *)calib_rxdc_args)[2])*2;
	
	in_p.rxbw = RXBW_50M;
	in_p.txbw = TXBW_50M;

	if (calib_rxdc_args_size > 3) {
		in_p.rxbw = ((uint8_t *)calib_rxdc_args)[3];
		if (calib_rxdc_args_size > 4)
			in_p.txbw = ((uint8_t *)calib_rxdc_args)[4];
	}
		
	if ((in_p.txbw > TXBW_50M) || (in_p.rxbw > RXBW_50M)) {
		FR1_LOGMSG_ERR( "\tEINVAL: Invalid parameter(s)\n" );
		FR1_LOGMSG_ERR( "\trx_bw      [0 .. 4]\n" );
		FR1_LOGMSG_ERR( "\ttx_bw      [0 .. 0]\n" );
		return MT3812_EINVPR;
	}
	
	in_p.bbgain   = 6;
	if (calib_rxdc_args_size > 5)
		in_p.bbgain = ((uint8_t *)calib_rxdc_args)[5];
	
	if (in_p.bbgain > 15) {
		FR1_LOGMSG_ERR( "\tEINVAL: Invalid parameter(s)\n" );
		FR1_LOGMSG_ERR( "\tbbgain     [0 .. 15]\n" );
		return MT3812_EINVPR;
	}

	// diora-calib_rxdc handle_local
	// input parameters
	in_p.dac_rate = SAMPLERATE_491;
	in_p.adc_rate = SAMPLERATE_245;
	in_p.txbuf  = &txbuf[0];
	in_p.rxbuf  = &rxbuf[0];
	in_p.res_re  = &res_r_re[0];
	in_p.res_im  = &res_r_im[0];
	in_p.t_buf   = &t_buf[0];

	// output parameters
	memset(out_p, 0, 2 * sizeof(struct rxdc_params_output));

	FR1_LOG_DBG("Just before %s call. \n", __func__);

	rtc = mt3812_calib_rxdc(rf_mdata[0]->nx_rf_dev.mt3812_handle, freq, rx, band, &in_p, &out_p[0]);
	if (rtc) {
		RTC_MESSAGE("diora-calibrxdc", rtc)
		return rtc;
	}

	FR1_LOGMSG_INFO("RXDC -------------------------------------------------------------------------\n");
	for (jj = 0; jj < 2; jj++) {
		FR1_LOG_INFO("\tRX_%d [Ifine, Icoarse]\t[%d, %d]", jj + 1,
			out_p[jj].Ifine[0], out_p[jj].Icoarse[0]);
		for (ii = 1; ii < 8; ii++)
			FR1_LOG_INFO("  [%d, %d]", out_p[jj].Ifine[ii],	out_p[jj].Icoarse[ii]);
		FR1_LOGMSG_INFO("\n");

		FR1_LOG_INFO("\tRX_%d [Qfine, Qcoarse]\t[%d, %d]", jj + 1,
			out_p[jj].Qfine[0], out_p[jj].Qcoarse[0]);
		for (ii = 1; ii < 8; ii++)
			FR1_LOG_INFO("  [%d, %d]", out_p[jj].Qfine[ii], out_p[jj].Qcoarse[ii]);
		FR1_LOGMSG_INFO("\n");
		
		FR1_LOG_INFO("\tCal RX_%d [Ifine, Icoarse, Qfine, Qcoarse]\t[%d, %d, %d, %d]", jj + 1,
			out_p[jj].cal_Ifine, out_p[jj].cal_Icoarse, out_p[jj].cal_Qfine, out_p[jj].cal_Qcoarse);
		FR1_LOGMSG_INFO("\n");
		FR1_LOGMSG_INFO("DC offset\n");
		FR1_LOG_INFO("\t[ I, Q] Q15 [%d, %d]\n", out_p[jj].DC_offset_I, out_p[jj].DC_offset_Q );   
		FR1_LOG_INFO("\t[-I,-Q]     [%.6f, %.6f]\n",(double) -out_p[jj].DC_offset_I/32768, (double) -out_p[jj].DC_offset_Q/32768);
		FR1_LOGMSG_INFO("\n");
	}
	
	return rtc;
}
#endif

int32_t fr1_rf_init (RF_FR1_CONFIG* config)
{
    int ret;
    FR1_LOG_DBG("LO_Freq = %d Tx_Path = %d Rx1_Path = %d Rx2_Path = %d Tx_BW = %d Rx_BW = %d \nTx_Gain_dB = %d Rx_Gain_dB = %d\n", 
            config->lo_freq_KHz, config->path_tx, config->path_rx1, config->path_rx2, config->tx_bw, config->rx_bw, config->tx_gain_db, config->rx_gain_db);
    FR1_LOG_DBG("rf calibration file path = %s \n", config->rf_cal_file);
    RF_PATH trx_path = path_append(config->path_rx1, config->path_rx2, config->path_tx);
    FR1_LOG_DBG("RF Path = %s \n", trx_path.rf_path);
    path_t rf_trx_path = path_to_enum_path(trx_path.rf_path);
    FR1_LOG_DBG("Path = %d\n", rf_trx_path);

    diora_drv_open(2);
    if(rf_mdata[0]->nx_rf_dev.mt3812_handle == NULL)
    {
        FR1_LOGMSG_ERR("drv_open failed\n");
        return MT3812_ERROR;
    }
    FR1_LOGMSG_DBG("drv_open is success\n");

    ret = diora_drv_load();
    if(ret != 0)
    {
        FR1_LOG_ERR("drv_firmware_load failed with return_code=%d \n",ret);
        return MT3812_ERROR;
    }
    FR1_LOG_DBG("drv_firmware_load is success with return_code=%d\n",ret);
    ret = diora_drv_version();
    ret = diora_gen_get_version();
    if(ret != 0)
    {
        FR1_LOG_ERR("GEN_get_version api failed with return_code=%d \n",ret);
        return MT3812_ERROR;
    }
    FR1_LOG_DBG("GEN_get_version is success with return_code=%d\n",ret);

    int32_t cal_resistor_args[] = {};
    ret = rf_app_cal_resistor(cal_resistor_args, 0);
    if(ret != 0)
    {
        FR1_LOG_ERR("APP_cal_resistor api failed with return_code=%d \n",ret);
        return MT3812_ERROR;
    }
    FR1_LOG_DBG("APP_cal_resistor is success with return_code=%d\n",ret);
    ret = rf_app_cal_regulator();
    if(ret != 0)
    {
        FR1_LOG_ERR("APP_cal_regulator api failed with return_code=%d \n",ret);
        return MT3812_ERROR;
    }
    FR1_LOG_DBG("APP_cal_regulator is success with return_code=%d\n",ret);
    int32_t pll_freq_args[] = {1, (config->lo_freq_KHz),0,0,0}; 
    ret = rf_adjust_pll_freq(pll_freq_args, 4);
    if(ret != 0)
    {
        FR1_LOG_ERR("trx_pll api failed with return_code=%d \n",ret);
        return MT3812_ERROR;
    }
    FR1_LOG_DBG("trx_pll is success with return_code=%d\n",ret);
    ret = rf_set_path(rf_trx_path, 2, 0, 0, config->rx_bw, config->tx_bw);
    if(ret != 0)
    {
        FR1_LOG_ERR("set_path api failed with return_code=%d \n",ret);
        return MT3812_ERROR;
    }
    FR1_LOG_DBG("set_path is success with return_code=%d\n",ret);
    int32_t txbw_args[] = {config->tx_bw,0,0};
    ret = rf_set_txbw(txbw_args, 2);
    if(ret != 0)
    {
        FR1_LOG_ERR("set_txbw api failed with return_code=%d \n",ret);
        return MT3812_ERROR;
    }
    FR1_LOG_DBG("set_txbw is success with return_code=%d\n",ret);
    int32_t rxbw_args[] = {config->rx_bw,0,0};
    ret = rf_set_rxbw(rxbw_args, 2);
    if(ret != 0)
    {
        FR1_LOG_ERR("set_rxbw api failed with return_code=%d \n",ret);
        return MT3812_ERROR;
    }
    FR1_LOG_DBG("set_rxbw is success with return_code=%d\n",ret);
    ret = rf_tx_gain_control_relative_diff(config->tx_gain_db);
    if(ret != 0)
    {
        FR1_LOG_ERR("set_tx_rel_diff api failed with return_code=%d \n",ret);
        return MT3812_ERROR;
    }
    FR1_LOG_DBG("set_tx_rel_diff is success with return_code=%d\n",ret);
    ret = rf_rx_gain_control_relative_diff(config->rx_gain_db);
    if(ret != 0)
    {
        FR1_LOG_ERR("set_rx_rel_diff api failed with return_code=%d \n",ret);
        return MT3812_ERROR;
    }
    FR1_LOG_DBG("set_rx_rel_diff is success with return_code=%d\n",ret);
    ret = rf_set_active(1,0);
    if(ret != 0)
    {
        FR1_LOG_ERR("set_active api failed with return_code=%d \n",ret);
        return MT3812_ERROR;
    }
    FR1_LOG_DBG("set_active is success with return_code=%d\n",ret);
    ret = rf_app_get_sysstatus();
    if(ret != 0)
    {
        FR1_LOG_ERR("APP_get_sysstatus api failed with return_code=%d \n",ret);
        return MT3812_ERROR;
    }
    FR1_LOG_DBG("APP_get_sysstatus is success with return_code=%d\n",ret);
    return 0;

}

int32_t rf_get_rf_status (void)
{
    printf("RF_CURRENT_STATUS\n");
    printf("---------------------------------------------------------------\n");
    printf("LO Freq in KHz   %d\n",rf_mdata[0]->nx_rf_dev.rf_lo_freq_khz);
    printf("Tx Gain in dB    %d\n",rf_mdata[0]->nx_rf_dev.tx_gain_db);
    printf("Rx Gain in dB    %d\n",rf_mdata[0]->nx_rf_dev.rx_gain_db);
    printf("Tx RF Gain Idx   %d\n",rf_mdata[0]->nx_rf_dev.tx_rf_gain);
    printf("Tx BB Gain Idx   %d\n",rf_mdata[0]->nx_rf_dev.tx_bb_gain);
    printf("Rx RF Gain Idx   %d\n",rf_mdata[0]->nx_rf_dev.rx_rf_gain);
    printf("Rx BB Gain Idx   %d\n",rf_mdata[0]->nx_rf_dev.rx_bb_gain);
    printf("Tx Bw            %d - %s\n",rf_mdata[0]->nx_rf_dev.tx_bw, txbw[rf_mdata[0]->nx_rf_dev.tx_bw]);
    printf("Rx Bw            %d - %s\n",rf_mdata[0]->nx_rf_dev.rx_bw, rxbw[rf_mdata[0]->nx_rf_dev.rx_bw]);
    printf("TRX Path         %d - %s\n",rf_mdata[0]->nx_rf_dev.trx_path, trxpath[rf_mdata[0]->nx_rf_dev.trx_path]);
    printf("---------------------------------------------------------------\n");
    return 0;
}
