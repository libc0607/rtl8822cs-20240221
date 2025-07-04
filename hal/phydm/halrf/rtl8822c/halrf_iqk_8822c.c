/******************************************************************************
 *
 * Copyright(c) 2007 - 2017  Realtek Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * The full GNU General Public License is included in this distribution in the
 * file called LICENSE.
 *
 * Contact Information:
 * wlanfae <wlanfae@realtek.com>
 * Realtek Corporation, No. 2, Innovation Road II, Hsinchu Science Park,
 * Hsinchu 300, Taiwan.
 *
 * Larry Finger <Larry.Finger@lwfinger.net>
 *
 *****************************************************************************/

#include "mp_precomp.h"
#if (DM_ODM_SUPPORT_TYPE == ODM_WIN)
#if RT_PLATFORM == PLATFORM_MACOSX
#include "phydm_precomp.h"
#else
#include "../phydm_precomp.h"
#endif
#else
#include "../../phydm_precomp.h"
#endif

#if (RTL8822C_SUPPORT == 1)

#if 1
boolean
_iqk_check_cal_8822c(
	struct dm_struct *dm,
	u8 path,
	u8 cmd)
{
	boolean notready = true, fail = true;
	u32 delay_count = 0x0;

	while (notready) {
		if (odm_read_1byte(dm, 0x2d9c) == 0x55) {
			if (cmd == 0x0) /*LOK*/
				fail = false;
			else
				fail = (boolean)odm_get_bb_reg(dm, R_0x1b08, BIT(26));
			notready = false;
		} else {
			ODM_delay_us(10);
			delay_count++;
		}

		if (delay_count >= 30000) {
			fail = true;
			RF_DBG(dm, DBG_RF_IQK, "[IQK]IQK timeout!!!\n");
			break;
		}
	}
	odm_write_1byte(dm, 0x1b10, 0x0);
	// disable slef-mixer for rx mode
	//odm_set_rf_reg(dm, (enum rf_path)path, 0x0, 0xf0000, 0x1);
	if(dm->cut_version == ODM_CUT_E)
		odm_set_rf_reg(dm, (enum rf_path)path, 0x8f, BIT(14), 0x0);
	halrf_delay_10us(1);
	odm_write_4byte(dm, 0x1b00, 0x8 | path << 1);
	if(!fail)
		odm_set_bb_reg(dm, R_0x1b20, BIT(26) | BIT(25), 0x2);	
	else
		odm_set_bb_reg(dm, R_0x1b20, BIT(26) | BIT(25), 0x0);
	//RF_DBG(dm, DBG_RF_IQK, "[IQK]delay count = 0x%x!!!\n", delay_count);
	//return fail;	
	return false;
}

void _iqk_idft_8822c(struct dm_struct *dm)
{

	odm_write_4byte(dm, 0x1b00, 0x8);
	odm_write_4byte(dm, 0x1bd8, 0xe0000001);
	odm_write_4byte(dm, 0x1b00, 0x00000e18);
	odm_write_4byte(dm, 0x1b00, 0x00000e19);
	_iqk_check_cal_8822c(dm, RF_PATH_A, 0x0);
	odm_write_4byte(dm, 0x1b00, 0xa);
	odm_write_4byte(dm, 0x1bd8, 0xe0000001);
	odm_write_4byte(dm, 0x1b00, 0x00000e2a);
	odm_write_4byte(dm, 0x1b00, 0x00000e2b);
	_iqk_check_cal_8822c(dm, RF_PATH_B, 0x0);

	odm_write_4byte(dm, 0x1b00, 0x8);
	odm_set_bb_reg(dm, R_0x1b20, BIT(31) | BIT(30), 0x0);	
	odm_write_4byte(dm, 0x1b00, 0xa);
	odm_set_bb_reg(dm, R_0x1b20, BIT(31) | BIT(30), 0x0);	
}

void _iqk_rx_cfir_check_8822c(struct dm_struct *dm, u8 t)
{
	struct dm_iqk_info *iqk_info = &dm->IQK_info;
	u8 j;

	for (j = 0; j <= 16; j++) {
		if (iqk_info->rx_cfir_real[0][0][j] != iqk_info->rx_cfir_real[1][0][j] ||
		iqk_info->rx_cfir_imag[0][0][j] != iqk_info->rx_cfir_imag[1][0][j]) {
			if (t == 0) {
				RF_DBG(dm, DBG_RF_IQK, "[ABC]bypass pathA RXCFIR\n");
				odm_set_bb_reg(dm, 0x180c, BIT(31), 0x0);
			} else {
				RF_DBG(dm, DBG_RF_IQK, "[ABC]pathA RX CFIR is changed\n");
			}
			break;
			
		}

		if (iqk_info->rx_cfir_real[0][1][j] != iqk_info->rx_cfir_real[1][1][j] ||
		iqk_info->rx_cfir_imag[0][1][j] != iqk_info->rx_cfir_imag[1][1][j]) {
			if (t ==0) {
				RF_DBG(dm, DBG_RF_IQK, "[ABC]bypass pathB RXCFIR\n");
				odm_set_bb_reg(dm, 0x410c, BIT(31), 0x0);
			} else {
				RF_DBG(dm, DBG_RF_IQK, "[ABC]pathB RX CFIR is changed\n");
			}
			break;
		}
	}
}


void _iqk_get_rxcfir_8822c(void *dm_void, u8 path, u8 t)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct dm_iqk_info *iqk_info = &dm->IQK_info;

	u8 i;
	u32 tmp;
	u32 bit_mask_20_16 = BIT(20) | BIT(19) | BIT(18) | BIT(17) | BIT(16);


	odm_write_4byte(dm, 0x1b00, 0x8 | path << 1);

//	for (i = 0; i <  0x100/4; i++)
//		RF_DBG(dm, DBG_RF_DPK, "[CC] (1) 1b%x = 0x%x\n",
//		       i*4, odm_read_4byte(dm, (0x1b00 + i*4)));

	odm_set_bb_reg(dm, R_0x1b20, BIT(31) | BIT(30), 0x1);		

	odm_set_bb_reg(dm, R_0x1bd4, BIT(21), 0x1);
	odm_set_bb_reg(dm, R_0x1bd4, bit_mask_20_16, 0x10);
	for (i = 0; i <= 16; i++) {
		odm_set_bb_reg(dm, R_0x1bd8, MASKDWORD, 0xe0000001 | i << 2);
		tmp = odm_get_bb_reg(dm, R_0x1bfc, MASKDWORD);
		iqk_info->rx_cfir_real[t][path][i] = (tmp & 0x0fff0000) >> 16;
		iqk_info->rx_cfir_imag[t][path][i] = tmp & 0x0fff;		
	}
//	for (i = 0; i <= 16; i++)
//		RF_DBG(dm, DBG_RF_IQK, "[CC](7) rx_cfir_real[%d][%d][%x] = %2x\n", t, path, i, iqk_info->rx_cfir_real[t][path][i]);		
//	for (i = 0; i <= 16; i++)
//		RF_DBG(dm, DBG_RF_IQK, "[CC](7) rx_cfir_imag[%d][%d][%x] = %2x\n", t, path, i, iqk_info->rx_cfir_imag[t][path][i]); 
	odm_set_bb_reg(dm, R_0x1b20, BIT(31) | BIT(30), 0x0);
//	odm_set_bb_reg(dm, R_0x1bd8, MASKDWORD, 0x0);
}

void _iqk_reload_rxcfir_8822c(struct dm_struct *dm, u8 path)
{
	struct dm_iqk_info *iqk_info = &dm->IQK_info;
#if 1
	u8 i;
	u32 tmp = 0x0, tmp1 = 0x0, tmp2 =0x0;

	odm_set_bb_reg(dm, 0x1b00, MASKDWORD, 0x8 | path << 1);
//	odm_set_bb_reg(dm, R_0x1b2c, MASKDWORD, 0x7);
//	odm_set_bb_reg(dm, R_0x1b38, MASKDWORD, 0x40000000);
//	odm_set_bb_reg(dm, R_0x1b3c, MASKDWORD, 0x40000000);
//	odm_write_1byte(dm, 0x1bcc, 0x0);
	odm_set_bb_reg(dm, 0x1b20, BIT(31) | BIT(30), 0x1);
	tmp1 = 0x60000001;
//	odm_set_bb_reg(dm, 0x1bd8, MASKDWORD, 0x60012303);
//	odm_set_bb_reg(dm, 0x1bd8, MASKDWORD, 0x60045601);
	halrf_delay_10us(100);
#if 1
	for (i = 0; i <= 16; i++) {
		tmp2 = tmp1 | iqk_info->rx_cfir_real[0][path][i]<< 8;
		tmp2 = (tmp2 | i << 2) + 2;
		odm_set_bb_reg(dm, 0x1bd8, MASKDWORD, tmp2);
		halrf_delay_10us(100);
		odm_set_bb_reg(dm, 0x1bd8, BIT(30), 0x0);
//		odm_set_bb_reg(dm, 0x1bd8, MASKDWORD, 0xe0000001);
	}
	for (i = 0; i <= 16; i++) {
		tmp2 = tmp1 | iqk_info->rx_cfir_imag[0][path][i]<< 8;
		tmp2 = (tmp2 | i << 2);
		odm_set_bb_reg(dm, 0x1bd8, MASKDWORD, tmp2);		
		halrf_delay_10us(100);
		odm_set_bb_reg(dm, 0x1bd8, BIT(30), 0x0);
//		odm_set_bb_reg(dm, 0x1bd8, MASKDWORD, 0xe0000001);
	}
#endif	
	// end for write CFIR SRAM
	odm_set_bb_reg(dm, 0x1bd8, MASKDWORD, 0xe0000001);
	halrf_delay_10us(100);
//	odm_set_bb_reg(dm, 0x1bd8, MASKDWORD, 0xe0000000);
//	ODM_delay_ms(10);
	odm_set_bb_reg(dm, 0x1b20, BIT(31) | BIT(30), 0x0);
//	ODM_delay_ms(10);
//	odm_set_bb_reg(dm, 0x1bd8, MASKDWORD, 0x0);
#endif
}

void _iqk_rx_cfir_8822c(struct dm_struct *dm, u8 path)
{

	u32 xym_tmp[6], cfir_tmp[3];
	u32 i;

	odm_set_bb_reg(dm, R_0x1b00, MASKDWORD, 0x8 | path << 1);
	odm_set_bb_reg(dm, 0x1b20, 0xff000000, 0x41);
	odm_set_bb_reg(dm, 0x1bd8, 0xffffffff, 0xe0000001);
	odm_set_bb_reg(dm, 0x1bd4, 0xffffffff, 0x00300001);

	odm_set_bb_reg(dm, 0x1bd8, 0xff, 0x01);
	cfir_tmp[0] = odm_get_bb_reg(dm, 0x1bfc, 0xffffffff);
	odm_set_bb_reg(dm, 0x1bd8, 0xff, 0x05);
	cfir_tmp[1] = odm_get_bb_reg(dm, 0x1bfc, 0xffffffff);
	odm_set_bb_reg(dm, 0x1bd8, 0xff, 0x09);
	cfir_tmp[2] = odm_get_bb_reg(dm, 0x1bfc, 0xffffffff);

	odm_set_bb_reg(dm, 0x1b20, 0xff000000, 0x05);
//	odm_set_bb_reg(dm, 0x1bd8, 0xff, 0x00);

	RF_DBG(dm, DBG_RF_IQK, "[CC] S%d RX CFIR = 0x%x, 0x%x, 0x%x\n",
		path, cfir_tmp[0], cfir_tmp[1], cfir_tmp[2]);
}

void _iqk_tx_cfir_8822c(struct dm_struct *dm, u8 path)
{
	u32 xym_tmp[6], cfir_tmp[3];
	u32 i;

	odm_set_bb_reg(dm, R_0x1b00, MASKDWORD, 0x8 | path << 1);
	odm_set_bb_reg(dm, 0x1b20, 0xff000000, 0xc1);
	odm_set_bb_reg(dm, 0x1bd8, 0xffffffff, 0xe0000001);
	odm_set_bb_reg(dm, 0x1bd4, 0xffffffff, 0x00300001);

	odm_set_bb_reg(dm, 0x1bd8, 0xff, 0x01);
	cfir_tmp[0] = odm_get_bb_reg(dm, 0x1bfc, 0xffffffff);
	odm_set_bb_reg(dm, 0x1bd8, 0xff, 0x05);
	cfir_tmp[1] = odm_get_bb_reg(dm, 0x1bfc, 0xffffffff);
	odm_set_bb_reg(dm, 0x1bd8, 0xff, 0x09);
	cfir_tmp[2] = odm_get_bb_reg(dm, 0x1bfc, 0xffffffff);

	odm_set_bb_reg(dm, 0x1b20, 0xff000000, 0x05);
//	odm_set_bb_reg(dm, 0x1bd8, 0xff, 0x00);

	RF_DBG(dm, DBG_RF_IQK, "[CC] TX CFIR = 0x%x, 0x%x, 0x%x\n",
		cfir_tmp[0], cfir_tmp[1], cfir_tmp[2]);
}

#endif

u8 _iqk_get_efuse_thermal_8822c(
	struct dm_struct *dm,
	u8 path)
{
	u32 thermal_tmp;
	u8 eeprom_thermal;

	if (path == RF_PATH_A) /*path s0*/
		odm_efuse_logical_map_read(dm, 1, 0xd0, &thermal_tmp);
	else /*path s1*/
		odm_efuse_logical_map_read(dm, 1, 0xd1, &thermal_tmp);
	eeprom_thermal = (u8)thermal_tmp;

	return eeprom_thermal;
}


/*---------------------------Define Local Constant---------------------------*/
void phydm_get_read_counter_8822c(struct dm_struct *dm)
{
	u32 counter = 0x0;

	while (1) {
		if ((odm_get_rf_reg(dm, RF_PATH_A, RF_0x8, RFREGOFFSETMASK) == 0xabcde) || counter > 300)
			break;
		counter++;
		ODM_delay_us(10);
	};
	odm_set_rf_reg(dm, RF_PATH_A, RF_0x8, RFREGOFFSETMASK, 0x0);
	RF_DBG(dm, DBG_RF_IQK, "[IQK]counter = %d\n", counter);
}

#if !(DM_ODM_SUPPORT_TYPE & ODM_AP)
void do_iqk_8822c(
	void *dm_void,
	u8 delta_thermal_index,
	u8 thermal_value,
	u8 threshold)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct dm_iqk_info *iqk_info = &dm->IQK_info;

	dm->rf_calibrate_info.thermal_value_iqk = thermal_value;
	halrf_segment_iqk_trigger(dm, true, false);
}
#else
/*Originally config->do_iqk is hooked phy_iq_calibrate_8822C, but do_iqk_8822C and phy_iq_calibrate_8822C have different arguments*/
void do_iqk_8822c(
	void *dm_void,
	u8 delta_thermal_index,
	u8 thermal_value,
	u8 threshold)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct dm_iqk_info *iqk_info = &dm->IQK_info;
	boolean is_recovery = (boolean)delta_thermal_index;

	halrf_segment_iqk_trigger(dm, true, false);
}
#endif

void iqk_power_save_8822c(
	void *dm_void,
	boolean is_power_save)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct dm_iqk_info *iqk_info = &dm->IQK_info;
	u8 path  = 0;

	for(path = 0; path < SS_8822C; path++) {
		odm_set_bb_reg(dm, R_0x1b00, BIT(2)| BIT(1), path);
		if (is_power_save)
			odm_set_bb_reg(dm, R_0x1b08, BIT(7), 0x0);
		else
			odm_set_bb_reg(dm, R_0x1b08, BIT(7), 0x1);
		}
}

void iqk_info_rsvd_page_8822c(
	void *dm_void,
	u8 *buf,
	u32 *buf_size)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct dm_iqk_info *iqk = &dm->IQK_info;
	u32 i = 0;
	
	if (buf) {
		odm_move_memory(dm, buf, iqk->iqk_channel,
				sizeof(iqk->iqk_channel));
		i += sizeof(iqk->iqk_channel);
		odm_move_memory(dm, buf + i, &iqk->iqk_cfir_real[0][0],
				sizeof(iqk->iqk_cfir_real[0][0]));
		i += sizeof(iqk->iqk_cfir_real[0][0]);
		odm_move_memory(dm, buf + i, &iqk->iqk_cfir_real[0][1],
				sizeof(iqk->iqk_cfir_real[0][1]));
		i += sizeof(iqk->iqk_cfir_real[0][1]);
		odm_move_memory(dm, buf + i, &iqk->iqk_cfir_real[1][0],
				sizeof(iqk->iqk_cfir_real[1][0]));
		i += sizeof(iqk->iqk_cfir_real[1][0]);
		odm_move_memory(dm, buf + i, &iqk->iqk_cfir_real[1][1],
				sizeof(iqk->iqk_cfir_real[1][1]));
		i += sizeof(iqk->iqk_cfir_real[1][1]);
		odm_move_memory(dm, buf + i, &iqk->iqk_cfir_imag[0][0],
				sizeof(iqk->iqk_cfir_imag[0][0]));
		i += sizeof(iqk->iqk_cfir_imag[0][0]);
		odm_move_memory(dm, buf + i, &iqk->iqk_cfir_imag[0][1],
				sizeof(iqk->iqk_cfir_imag[0][1]));
		i += sizeof(iqk->iqk_cfir_imag[0][1]);
		odm_move_memory(dm, buf + i, &iqk->iqk_cfir_imag[1][0],
				sizeof(iqk->iqk_cfir_imag[1][0]));
		i += sizeof(iqk->iqk_cfir_imag[1][0]);
		odm_move_memory(dm, buf + i, &iqk->iqk_cfir_imag[1][1],
				sizeof(iqk->iqk_cfir_imag[1][1]));
		i += sizeof(iqk->iqk_cfir_imag[1][1]);
		odm_move_memory(dm, buf + i, &iqk->lok_idac[0][0],
				sizeof(iqk->lok_idac[0][0]));
		i += sizeof(iqk->lok_idac[0][0]);
		odm_move_memory(dm, buf + i, &iqk->lok_idac[0][1],
				sizeof(iqk->lok_idac[0][1]));
		i += sizeof(iqk->lok_idac[0][1]);
		odm_move_memory(dm, buf + i, &iqk->lok_idac[1][0],
				sizeof(iqk->lok_idac[1][0]));
		i += sizeof(iqk->lok_idac[1][0]);
		odm_move_memory(dm, buf + i, &iqk->lok_idac[1][1],
				sizeof(iqk->lok_idac[1][1]));
		i += sizeof(iqk->lok_idac[1][1]);
	}

	if (buf_size)
		*buf_size = IQK_INFO_RSVD_LEN_8822C;
}

void _iqk_information_8822c(
	struct dm_struct *dm)
{
	struct dm_iqk_info *iqk_info = &dm->IQK_info;

	u32  reg_rf18;

	if (odm_get_bb_reg(dm, R_0x1e7c, BIT(30)))
		iqk_info->is_tssi_mode = true;
	else
		iqk_info->is_tssi_mode = false;

	reg_rf18 = odm_get_rf_reg(dm, RF_PATH_A, RF_0x18, RFREG_MASK);
	iqk_info->iqk_band = (u8)((reg_rf18 & BIT(16)) >> 16); /*0/1:G/A*/
	iqk_info->iqk_ch = (u8)reg_rf18 & 0xff;
	iqk_info->iqk_bw = (u8)((reg_rf18 & 0x3000) >> 12); /*3/2/1:20/40/80*/

	RF_DBG(dm, DBG_RF_DPK, "[IQK] TSSI/ Band/ CH/ BW = %d / %s / %d / %s\n",
	       iqk_info->is_tssi_mode, iqk_info->iqk_band == 0 ? "2G" : "5G",
	       iqk_info->iqk_ch,
	       iqk_info->iqk_bw == 3 ? "20M" : (iqk_info->iqk_bw == 2 ? "40M" : "80M"));
}


boolean _iqk_xym_read_8822c(struct dm_struct *dm, u8 path)
{
	u32 i = 0x0;
	u32 xym = 0x0;
	boolean kfail = false;
	u32 xvalue = 0x0;
	u32 yvalue = 0x0;
	u8 x_thr = 100, y_thr = 100;

	odm_write_4byte(dm, 0x1b00, 0x8 | path << 1);	
	odm_set_bb_reg(dm, 0x1b1c, BIT(1) | BIT(0), 0x2);

	for (i = 0x0; i < 24; i++ ) {
		odm_set_bb_reg(dm, 0x1b14, MASKDWORD, 0x000000e0 + i);
		odm_set_bb_reg(dm, 0x1b14, MASKDWORD, 0x0);
		xym = odm_get_bb_reg(dm, 0x1b38, MASKDWORD);
		xvalue = odm_get_bb_reg(dm, 0x1b38, 0xfff00000);		
		yvalue = odm_get_bb_reg(dm, 0x1b38, 0x000fff00);		

		if (xvalue < 0x400) {// "- vale
			if ((0x400 - xvalue) > x_thr)
				kfail = true;				
		} else { //"+" vale
			if ((xvalue - 0x400) > x_thr)
				kfail = true;			
		}

		if (yvalue > 0x800) { // "- vale
			if ((0xfff - yvalue) > y_thr)
				kfail = true;			
		} else { // "+" vale
			if (yvalue > y_thr)
				kfail = true;
		}
		
		if (kfail == true) {
			RF_DBG(dm, DBG_RF_IQK, "[IQK]S%d XYM > thr happen\n", path);
			break;
		}
		
	}
	odm_set_bb_reg(dm, 0x1b38, MASKDWORD, 0x40000000);
	return kfail;
}


static u32
_iqk_btc_wait_indirect_reg_ready_8822c(struct dm_struct *dm)
{
	u32 delay_count = 0;
	
	/* wait for ready bit before access 0x1700 */
	while (1) {
		if ((odm_read_1byte(dm, 0x1703) & BIT(5)) == 0) {
			delay_ms(10);
			if (++delay_count >= 10)
			break;
		} else {
			break;
		}
	}
	
	return delay_count;
}

static u32
_iqk_btc_read_indirect_reg_8822c(struct dm_struct *dm, u16 reg_addr)
{
	u32 delay_count = 0;

	/* wait for ready bit before access 0x1700 */
	_iqk_btc_wait_indirect_reg_ready_8822c(dm);

	odm_write_4byte(dm, 0x1700, 0x800F0000 | reg_addr);

	return odm_read_4byte(dm, 0x1708); /* get read data */
}

static void
_iqk_btc_write_indirect_reg_8822c(struct dm_struct *dm, u16 reg_addr,
		       u32 bit_mask, u32 reg_value)
{
	u32 val, i = 0, bitpos = 0, delay_count = 0;

	if (bit_mask == 0x0)
		return;

	if (bit_mask == 0xffffffff) {
	/* wait for ready bit before access 0x1700 */
	_iqk_btc_wait_indirect_reg_ready_8822c(dm);

	/* put write data */
	odm_write_4byte(dm, 0x1704, reg_value);

	odm_write_4byte(dm, 0x1700, 0xc00F0000 | reg_addr);
	} else {
		for (i = 0; i <= 31; i++) {
			if (((bit_mask >> i) & 0x1) == 0x1) {
				bitpos = i;
				break;
			}
		}

		/* read back register value before write */
		val = _iqk_btc_read_indirect_reg_8822c(dm, reg_addr);
		val = (val & (~bit_mask)) | (reg_value << bitpos);

		/* wait for ready bit before access 0x1700 */
		_iqk_btc_wait_indirect_reg_ready_8822c(dm);

		odm_write_4byte(dm, 0x1704, val); /* put write data */
		odm_write_4byte(dm, 0x1700, 0xc00F0000 | reg_addr);
	}
}

void _iqk_set_gnt_wl_high_8822c(struct dm_struct *dm)
{
	u32 val = 0;
	u8 state = 0x1;

	/*GNT_WL = 1*/
	val = (state << 1) | 0x1;
	_iqk_btc_write_indirect_reg_8822c(dm, 0x38, 0xff00, 0x77); /*0x38[13:12]*/
	//_iqk_btc_write_indirect_reg_8822c(dm, 0x38, 0x0300, val); /*0x38[9:8]*/
}

void _iqk_set_gnt_bt_low_8822c(struct dm_struct *dm)
{
#if 0
	u32 val = 0;
	u8 state = 0x0, sw_control = 0x1;

	/*GNT_BT = 0*/
	val = (sw_control) ? ((state << 1) | 0x1) : 0;
	//_iqk_btc_write_indirect_reg_8822c(dm, 0x38, 0xc000, val); /*0x38[15:14]*/
	//_iqk_btc_write_indirect_reg_8822c(dm, 0x38, 0x0c00, val); /*0x38[11:10]*/
#endif
	return;
}

void _iqk_set_gnt_wl_gnt_bt_8822c(struct dm_struct *dm, boolean beforeK)
{
	struct dm_iqk_info *iqk_info = &dm->IQK_info;

	if (beforeK) {
		_iqk_set_gnt_wl_high_8822c(dm);
		//_iqk_set_gnt_bt_low_8822c(dm);
	} else {
		_iqk_btc_write_indirect_reg_8822c(dm, 0x38, MASKDWORD, iqk_info->tmp_gntwl);
	}
}



void _iqk_nctl_8822c(struct dm_struct *dm)
{
	RF_DBG(dm, DBG_RF_IQK, "[IQK]==========IQK NCTL!!!!!========\n");
	//odm_write_4byte(dm 0x1CD0, 0x7 [31:28]);	
	odm_set_bb_reg(dm, 0x1cd0, 0xf0000000, 0x7);
	//====== Subpage 0 Init_Setting ======================= //
	odm_write_4byte(dm, 0x1b00, 0x00000008);
	odm_write_4byte(dm, 0x1b00, 0x00D70008);
	odm_write_4byte(dm, 0x1b00, 0x00150008);
	odm_write_4byte(dm, 0x1b00, 0x00000008);

	odm_write_4byte(dm, 0x1b04, 0xE2462952);
	odm_write_4byte(dm, 0x1b08, 0x00000080);
	odm_write_4byte(dm, 0x1b0c, 0x00000000);
	odm_write_4byte(dm, 0x1b10, 0x00011800);
	odm_write_4byte(dm, 0x1b14, 0x00000000);
	odm_write_4byte(dm, 0x1b18, 0x00292903);
	odm_write_4byte(dm, 0x1b1c, 0xA2193C32);
	odm_write_4byte(dm, 0x1b20, 0x03040008);
	odm_write_4byte(dm, 0x1b24, 0x00060008);
	odm_write_4byte(dm, 0x1b28, 0x00060300);
	odm_write_4byte(dm, 0x1b2C, 0x00180018);
	odm_write_4byte(dm, 0x1b30, 0x40000000);
	odm_write_4byte(dm, 0x1b34, 0x00000800);
	odm_write_4byte(dm, 0x1b38, 0x40000000 );
	odm_write_4byte(dm, 0x1b3C, 0x40000000 );
	// TxGainGapK
	odm_write_4byte(dm, 0x1b98, 0x00000000);
	odm_write_4byte(dm, 0x1b9c, 0x00000000);
	odm_write_4byte(dm, 0x1bc0, 0x01000000);
	odm_write_4byte(dm, 0x1bcc, 0x00000000);
	//odm_write_4byte(dm, 0x1be4, 0x0 [1:0]);	
	odm_set_bb_reg(dm, 0x1be4, BIT(1) | BIT(0), 0x0);
	odm_write_4byte(dm, 0x1bec, 0x40000000);
	// ---------------------------------- DPD -------------------------- //
	//DPD associated settings
	odm_write_4byte(dm, 0x1b40, 0x40000000);
	odm_write_4byte(dm, 0x1b44, 0x20001064);
	odm_write_4byte(dm, 0x1b48, 0x0005002D);
	odm_write_4byte(dm, 0x1b4c, 0x00000000);
	odm_write_4byte(dm, 0x1b54, 0x00009802);
	odm_write_4byte(dm, 0x1b60, 0x1F150000);
	odm_write_4byte(dm, 0x1b64, 0x19140000);
	odm_write_4byte(dm, 0x1b58, 0x00008F00);
	odm_write_4byte(dm, 0x1b5C, 0x00000000);
	//0dB amp
	odm_write_4byte(dm, 0x1b4c, 0x00000000);
	odm_write_4byte(dm, 0x1b4c, 0x008a0000);
	odm_write_4byte(dm, 0x1b50, 0x000003BE);
	odm_write_4byte(dm, 0x1b4c, 0x018a0000);
	odm_write_4byte(dm, 0x1b50, 0x0000057A);
	odm_write_4byte(dm, 0x1b4c, 0x028a0000);
	odm_write_4byte(dm, 0x1b50, 0x000006C8);
	odm_write_4byte(dm, 0x1b4c, 0x038a0000);
	odm_write_4byte(dm, 0x1b50, 0x000007E0);
	odm_write_4byte(dm, 0x1b4c, 0x048a0000);
	odm_write_4byte(dm, 0x1b50, 0x000008D5);
	odm_write_4byte(dm, 0x1b4c, 0x058a0000);
	odm_write_4byte(dm, 0x1b50, 0x000009B2);
	odm_write_4byte(dm, 0x1b4c, 0x068a0000);
	odm_write_4byte(dm, 0x1b50, 0x00000A7D);
	odm_write_4byte(dm, 0x1b4c, 0x078a0000);
	odm_write_4byte(dm, 0x1b50, 0x00000B3A);
	odm_write_4byte(dm, 0x1b4c, 0x088a0000);
	odm_write_4byte(dm, 0x1b50, 0x00000BEB);
	odm_write_4byte(dm, 0x1b4c, 0x098a0000);
	odm_write_4byte(dm, 0x1b50, 0x00000C92);
	odm_write_4byte(dm, 0x1b4c, 0x0A8a0000);
	odm_write_4byte(dm, 0x1b50, 0x00000D31);
	odm_write_4byte(dm, 0x1b4c, 0x0B8a0000);
	odm_write_4byte(dm, 0x1b50, 0x00000DC9);
	odm_write_4byte(dm, 0x1b4c, 0x0C8a0000);
	odm_write_4byte(dm, 0x1b50, 0x00000E5A);
	odm_write_4byte(dm, 0x1b4c, 0x0D8a0000);
	odm_write_4byte(dm, 0x1b50, 0x00000EE6);
	odm_write_4byte(dm, 0x1b4c, 0x0E8a0000);
	odm_write_4byte(dm, 0x1b50, 0x00000F6D);
	odm_write_4byte(dm, 0x1b4c, 0x0F8a0000);
	odm_write_4byte(dm, 0x1b50, 0x00000FF0);
	odm_write_4byte(dm, 0x1b4c, 0x108a0000);
	odm_write_4byte(dm, 0x1b50, 0x0000106F);
	odm_write_4byte(dm, 0x1b4c, 0x118a0000);
	odm_write_4byte(dm, 0x1b50, 0x000010E9);
	odm_write_4byte(dm, 0x1b4c, 0x128a0000);
	odm_write_4byte(dm, 0x1b50, 0x00001161);
	odm_write_4byte(dm, 0x1b4c, 0x138a0000);
	odm_write_4byte(dm, 0x1b50, 0x000011D5);
	odm_write_4byte(dm, 0x1b4c, 0x148a0000);
	odm_write_4byte(dm, 0x1b50, 0x00001247);
	odm_write_4byte(dm, 0x1b4c, 0x158a0000);
	odm_write_4byte(dm, 0x1b50, 0x000012B5);
	odm_write_4byte(dm, 0x1b4c, 0x168a0000);
	odm_write_4byte(dm, 0x1b50, 0x00001322);
	odm_write_4byte(dm, 0x1b4c, 0x178a0000);
	odm_write_4byte(dm, 0x1b50, 0x0000138B);
	odm_write_4byte(dm, 0x1b4c, 0x188a0000);
	odm_write_4byte(dm, 0x1b50, 0x000013F3);
	odm_write_4byte(dm, 0x1b4c, 0x198a0000);
	odm_write_4byte(dm, 0x1b50, 0x00001459);
	odm_write_4byte(dm, 0x1b4c, 0x1A8a0000);
	odm_write_4byte(dm, 0x1b50, 0x000014BD);
	odm_write_4byte(dm, 0x1b4c, 0x1B8a0000);
	odm_write_4byte(dm, 0x1b50, 0x0000151E);
	odm_write_4byte(dm, 0x1b4c, 0x1C8a0000);
	odm_write_4byte(dm, 0x1b50, 0x0000157F);
	odm_write_4byte(dm, 0x1b4c, 0x1D8a0000);
	odm_write_4byte(dm, 0x1b50, 0x000015DD);
	odm_write_4byte(dm, 0x1b4c, 0x1E8a0000);
	odm_write_4byte(dm, 0x1b50, 0x0000163A);
	odm_write_4byte(dm, 0x1b4c, 0x1F8a0000);
	odm_write_4byte(dm, 0x1b50, 0x00001695);
	odm_write_4byte(dm, 0x1b4c, 0x208a0000);
	odm_write_4byte(dm, 0x1b50, 0x000016EF);
	odm_write_4byte(dm, 0x1b4c, 0x218a0000);
	odm_write_4byte(dm, 0x1b50, 0x00001748);
	odm_write_4byte(dm, 0x1b4c, 0x228a0000);
	odm_write_4byte(dm, 0x1b50, 0x0000179F);
	odm_write_4byte(dm, 0x1b4c, 0x238a0000);
	odm_write_4byte(dm, 0x1b50, 0x000017F5);
	odm_write_4byte(dm, 0x1b4c, 0x248a0000);
	odm_write_4byte(dm, 0x1b50, 0x0000184A);
	odm_write_4byte(dm, 0x1b4c, 0x258a0000);
	odm_write_4byte(dm, 0x1b50, 0x0000189E);
	odm_write_4byte(dm, 0x1b4c, 0x268a0000);
	odm_write_4byte(dm, 0x1b50, 0x000018F1);
	odm_write_4byte(dm, 0x1b4c, 0x278a0000);
	odm_write_4byte(dm, 0x1b50, 0x00001942);
	odm_write_4byte(dm, 0x1b4c, 0x288a0000);
	odm_write_4byte(dm, 0x1b50, 0x00001993);
	odm_write_4byte(dm, 0x1b4c, 0x298a0000);
	odm_write_4byte(dm, 0x1b50, 0x000019E2);
	odm_write_4byte(dm, 0x1b4c, 0x2A8a0000);
	odm_write_4byte(dm, 0x1b50, 0x00001A31);
	odm_write_4byte(dm, 0x1b4c, 0x2B8a0000);
	odm_write_4byte(dm, 0x1b50, 0x00001A7F);
	odm_write_4byte(dm, 0x1b4c, 0x2C8a0000);
	odm_write_4byte(dm, 0x1b50, 0x00001ACC);
	odm_write_4byte(dm, 0x1b4c, 0x2D8a0000);
	odm_write_4byte(dm, 0x1b50, 0x00001B18);
	odm_write_4byte(dm, 0x1b4c, 0x2E8a0000);
	odm_write_4byte(dm, 0x1b50, 0x00001B63);
	odm_write_4byte(dm, 0x1b4c, 0x2F8a0000);
	odm_write_4byte(dm, 0x1b50, 0x00001BAD);
	odm_write_4byte(dm, 0x1b4c, 0x308a0000);
	odm_write_4byte(dm, 0x1b50, 0x00001BF7);
	odm_write_4byte(dm, 0x1b4c, 0x318a0000);
	odm_write_4byte(dm, 0x1b50, 0x00001C40);
	odm_write_4byte(dm, 0x1b4c, 0x328a0000);
	odm_write_4byte(dm, 0x1b50, 0x00001C88);
	odm_write_4byte(dm, 0x1b4c, 0x338a0000);
	odm_write_4byte(dm, 0x1b50, 0x00001CCF);
	odm_write_4byte(dm, 0x1b4c, 0x348a0000);
	odm_write_4byte(dm, 0x1b50, 0x00001D16);
	odm_write_4byte(dm, 0x1b4c, 0x358a0000);
	odm_write_4byte(dm, 0x1b50, 0x00001D5C);
	odm_write_4byte(dm, 0x1b4c, 0x368a0000);
	odm_write_4byte(dm, 0x1b50, 0x00001DA2);
	odm_write_4byte(dm, 0x1b4c, 0x378a0000);
	odm_write_4byte(dm, 0x1b50, 0x00001DE6);
	odm_write_4byte(dm, 0x1b4c, 0x388a0000);
	odm_write_4byte(dm, 0x1b50, 0x00001E2B);
	odm_write_4byte(dm, 0x1b4c, 0x398a0000);
	odm_write_4byte(dm, 0x1b50, 0x00001E6E);
	odm_write_4byte(dm, 0x1b4c, 0x3A8a0000);
	odm_write_4byte(dm, 0x1b50, 0x00001EB1);
	odm_write_4byte(dm, 0x1b4c, 0x3B8a0000);
	odm_write_4byte(dm, 0x1b50, 0x00001EF4);
	odm_write_4byte(dm, 0x1b4c, 0x3C8a0000);
	odm_write_4byte(dm, 0x1b50, 0x00001F35);
	odm_write_4byte(dm, 0x1b4c, 0x3D8a0000);
	odm_write_4byte(dm, 0x1b50, 0x00001F77);
	odm_write_4byte(dm, 0x1b4c, 0x3E8a0000);
	odm_write_4byte(dm, 0x1b50, 0x00001FB8);
	odm_write_4byte(dm, 0x1b4c, 0x3F8a0000);
	odm_write_4byte(dm, 0x1b50, 0x00001FF8);
	odm_write_4byte(dm, 0x1b4c, 0x00000000);
	odm_write_4byte(dm, 0x1b50, 0x00000000);
	// write pwsf table 
	odm_write_4byte(dm, 0x1b58, 0x00890000);
	odm_write_4byte(dm, 0x1b5C, 0x3C6B3FFF);
	odm_write_4byte(dm, 0x1b58, 0x02890000);
	odm_write_4byte(dm, 0x1b5C, 0x35D9390A);
	odm_write_4byte(dm, 0x1b58, 0x04890000);
	odm_write_4byte(dm, 0x1b5C, 0x2FFE32D6);
	odm_write_4byte(dm, 0x1b58, 0x06890000);
	odm_write_4byte(dm, 0x1b5C, 0x2AC62D4F);
	odm_write_4byte(dm, 0x1b58, 0x08890000);
	odm_write_4byte(dm, 0x1b5C, 0x261F2862);
	odm_write_4byte(dm, 0x1b58, 0x0A890000);
	odm_write_4byte(dm, 0x1b5C, 0x21FA23FD);
	odm_write_4byte(dm, 0x1b58, 0x0C890000);
	odm_write_4byte(dm, 0x1b5C, 0x1E482013);
	odm_write_4byte(dm, 0x1b58, 0x0E890000);
	odm_write_4byte(dm, 0x1b5C, 0x1AFD1C96);
	odm_write_4byte(dm, 0x1b58, 0x10890000);
	odm_write_4byte(dm, 0x1b5C, 0x180E197B);
	odm_write_4byte(dm, 0x1b58, 0x12890000);
	odm_write_4byte(dm, 0x1b5C, 0x157016B5);
	odm_write_4byte(dm, 0x1b58, 0x14890000);
	odm_write_4byte(dm, 0x1b5C, 0x131B143D);
	odm_write_4byte(dm, 0x1b58, 0x16890000);
	odm_write_4byte(dm, 0x1b5C, 0x1107120A);
	odm_write_4byte(dm, 0x1b58, 0x18890000);
	odm_write_4byte(dm, 0x1b5C, 0x0F2D1013);
	odm_write_4byte(dm, 0x1b58, 0x1A890000);
	odm_write_4byte(dm, 0x1b5C, 0x0D870E54);
	odm_write_4byte(dm, 0x1b58, 0x1C890000);
	odm_write_4byte(dm, 0x1b5C, 0x0C0E0CC5);
	odm_write_4byte(dm, 0x1b58, 0x1E890000);
	odm_write_4byte(dm, 0x1b5C, 0x0ABF0B62);
	odm_write_4byte(dm, 0x1b58, 0x20890000);
	odm_write_4byte(dm, 0x1b5C, 0x09930A25);
	odm_write_4byte(dm, 0x1b58, 0x22890000);
	odm_write_4byte(dm, 0x1b5C, 0x0889090A);
	odm_write_4byte(dm, 0x1b58, 0x24890000);
	odm_write_4byte(dm, 0x1b5C, 0x079B080F);
	odm_write_4byte(dm, 0x1b58, 0x26890000);
	odm_write_4byte(dm, 0x1b5C, 0x06C7072E);
	odm_write_4byte(dm, 0x1b58, 0x28890000);
	odm_write_4byte(dm, 0x1b5C, 0x060B0666);
	odm_write_4byte(dm, 0x1b58, 0x2A890000);
	odm_write_4byte(dm, 0x1b5C, 0x056305B4);
	odm_write_4byte(dm, 0x1b58, 0x2C890000);
	odm_write_4byte(dm, 0x1b5C, 0x04CD0515);
	odm_write_4byte(dm, 0x1b58, 0x2E890000);
	odm_write_4byte(dm, 0x1b5C, 0x04470488);
	odm_write_4byte(dm, 0x1b58, 0x30890000);
	odm_write_4byte(dm, 0x1b5C, 0x03D0040A);
	odm_write_4byte(dm, 0x1b58, 0x32890000);
	odm_write_4byte(dm, 0x1b5C, 0x03660399);
	odm_write_4byte(dm, 0x1b58, 0x34890000);
	odm_write_4byte(dm, 0x1b5C, 0x03070335);
	odm_write_4byte(dm, 0x1b58, 0x36890000);
	odm_write_4byte(dm, 0x1b5C, 0x02B302DC);
	odm_write_4byte(dm, 0x1b58, 0x38890000);
	odm_write_4byte(dm, 0x1b5C, 0x0268028C);
	odm_write_4byte(dm, 0x1b58, 0x3A890000);
	odm_write_4byte(dm, 0x1b5C, 0x02250245);
	odm_write_4byte(dm, 0x1b58, 0x3C890000);
	odm_write_4byte(dm, 0x1b5C, 0x01E90206);
	odm_write_4byte(dm, 0x1b58, 0x3E890000);
	odm_write_4byte(dm, 0x1b5C, 0x01B401CE);
	odm_write_4byte(dm, 0x1b58, 0x40890000);
	odm_write_4byte(dm, 0x1b5C, 0x0185019C);
	odm_write_4byte(dm, 0x1b58, 0x42890000);
	odm_write_4byte(dm, 0x1b5C, 0x015A016F);
	odm_write_4byte(dm, 0x1b58, 0x44890000);
	odm_write_4byte(dm, 0x1b5C, 0x01350147);
	odm_write_4byte(dm, 0x1b58, 0x46890000);
	odm_write_4byte(dm, 0x1b5C, 0x01130123);
	odm_write_4byte(dm, 0x1b58, 0x48890000);
	odm_write_4byte(dm, 0x1b5C, 0x00F50104);
	odm_write_4byte(dm, 0x1b58, 0x4A890000);
	odm_write_4byte(dm, 0x1b5C, 0x00DA00E7);
	odm_write_4byte(dm, 0x1b58, 0x4C890000);
	odm_write_4byte(dm, 0x1b5C, 0x00C300CE);
	odm_write_4byte(dm, 0x1b58, 0x4E890000);
	odm_write_4byte(dm, 0x1b5C, 0x00AE00B8);
	odm_write_4byte(dm, 0x1b58, 0x50890000);
	odm_write_4byte(dm, 0x1b5C, 0x009B00A4);
	odm_write_4byte(dm, 0x1b58, 0x52890000);
	odm_write_4byte(dm, 0x1b5C, 0x008A0092);
	odm_write_4byte(dm, 0x1b58, 0x54890000);
	odm_write_4byte(dm, 0x1b5C, 0x007B0082);
	odm_write_4byte(dm, 0x1b58, 0x56890000);
	odm_write_4byte(dm, 0x1b5C, 0x006E0074);
	odm_write_4byte(dm, 0x1b58, 0x58890000);
	odm_write_4byte(dm, 0x1b5C, 0x00620067);
	odm_write_4byte(dm, 0x1b58, 0x5A890000);
	odm_write_4byte(dm, 0x1b5C, 0x0057005C);
	odm_write_4byte(dm, 0x1b58, 0x5C890000);
	odm_write_4byte(dm, 0x1b5C, 0x004E0052);
	odm_write_4byte(dm, 0x1b58, 0x5E890000);
	odm_write_4byte(dm, 0x1b5C, 0x00450049);
	odm_write_4byte(dm, 0x1b58, 0x60890000);
	odm_write_4byte(dm, 0x1b5C, 0x003E0041);
	odm_write_4byte(dm, 0x1b58, 0x62890000);
	odm_write_4byte(dm, 0x1b5C, 0x0037003A);
	odm_write_4byte(dm, 0x1b58, 0x62010000);
	// ======================= Subpage 1 Init_Setting ===============
	odm_write_4byte(dm, 0x1b00, 0x0000000A);
	// --------------------------- WIQK --------------------------- //
	odm_write_4byte(dm, 0x1b00, 0x00D7000A);
	odm_write_4byte(dm, 0x1b00, 0x0015000A);
	odm_write_4byte(dm, 0x1b00, 0x0000000A);
	odm_write_4byte(dm, 0x1b04, 0xE2462952);
	odm_write_4byte(dm, 0x1b08, 0x00000080);
	odm_write_4byte(dm, 0x1b0c, 0x00000000);
	odm_write_4byte(dm, 0x1b10, 0x00011800);
	odm_write_4byte(dm, 0x1b14, 0x00000000);
	odm_write_4byte(dm, 0x1b18, 0x00292903);
	odm_write_4byte(dm, 0x1b1c, 0xA2193C32);
	odm_write_4byte(dm, 0x1b20, 0x03040008);
	odm_write_4byte(dm, 0x1b24, 0x00060008);
	odm_write_4byte(dm, 0x1b28, 0x00060300);
	odm_write_4byte(dm, 0x1b2C, 0x00180018);
	odm_write_4byte(dm, 0x1b30, 0x40000000);
	odm_write_4byte(dm, 0x1b34, 0x00000800);
	odm_write_4byte(dm, 0x1b38, 0x40000000 );
	odm_write_4byte(dm, 0x1b3C, 0x40000000 );
	// TxGainGapK
	odm_write_4byte(dm, 0x1b98, 0x00000000);
	odm_write_4byte(dm, 0x1b9c, 0x00000000);
	odm_write_4byte(dm, 0x1bc0, 0x01000000);
	odm_write_4byte(dm, 0x1bcc, 0x00000000);
	//odm_write_4byte(dm, 0x1be4, 0x0 [1:0]);
	odm_set_bb_reg(dm, 0x1be4, BIT(1) | BIT(0), 0x0);
	odm_write_4byte(dm, 0x1bec, 0x40000000);
	// --------------------------- DPD --------------------------- //
	//DPD associated settings
	odm_write_4byte(dm, 0x1b54, 0x00009802 );
	odm_write_4byte(dm, 0x1b60, 0x1F150000);
	odm_write_4byte(dm, 0x1b64, 0x19140000);
	odm_write_4byte(dm, 0x1b58, 0x00008F00);
	odm_write_4byte(dm, 0x1b5C, 0x00000000);
	// write pwsf table );
	odm_write_4byte(dm, 0x1b58, 0x00890000);
	odm_write_4byte(dm, 0x1b5C, 0x3C6B3FFF);
	odm_write_4byte(dm, 0x1b58, 0x02890000);
	odm_write_4byte(dm, 0x1b5C, 0x35D9390A);
	odm_write_4byte(dm, 0x1b58, 0x04890000);
	odm_write_4byte(dm, 0x1b5C, 0x2FFE32D6);
	odm_write_4byte(dm, 0x1b58, 0x06890000);
	odm_write_4byte(dm, 0x1b5C, 0x2AC62D4F);
	odm_write_4byte(dm, 0x1b58, 0x08890000);
	odm_write_4byte(dm, 0x1b5C, 0x261F2862);
	odm_write_4byte(dm, 0x1b58, 0x0A890000);
	odm_write_4byte(dm, 0x1b5C, 0x21FA23FD);
	odm_write_4byte(dm, 0x1b58, 0x0C890000);
	odm_write_4byte(dm, 0x1b5C, 0x1E482013);
	odm_write_4byte(dm, 0x1b58, 0x0E890000);
	odm_write_4byte(dm, 0x1b5C, 0x1AFD1C96);
	odm_write_4byte(dm, 0x1b58, 0x10890000);
	odm_write_4byte(dm, 0x1b5C, 0x180E197B);
	odm_write_4byte(dm, 0x1b58, 0x12890000);
	odm_write_4byte(dm, 0x1b5C, 0x157016B5);
	odm_write_4byte(dm, 0x1b58, 0x14890000);
	odm_write_4byte(dm, 0x1b5C, 0x131B143D);
	odm_write_4byte(dm, 0x1b58, 0x16890000);
	odm_write_4byte(dm, 0x1b5C, 0x1107120A);
	odm_write_4byte(dm, 0x1b58, 0x18890000);
	odm_write_4byte(dm, 0x1b5C, 0x0F2D1013);
	odm_write_4byte(dm, 0x1b58, 0x1A890000);
	odm_write_4byte(dm, 0x1b5C, 0x0D870E54);
	odm_write_4byte(dm, 0x1b58, 0x1C890000);
	odm_write_4byte(dm, 0x1b5C, 0x0C0E0CC5);
	odm_write_4byte(dm, 0x1b58, 0x1E890000);
	odm_write_4byte(dm, 0x1b5C, 0x0ABF0B62);
	odm_write_4byte(dm, 0x1b58, 0x20890000);
	odm_write_4byte(dm, 0x1b5C, 0x09930A25);
	odm_write_4byte(dm, 0x1b58, 0x22890000);
	odm_write_4byte(dm, 0x1b5C, 0x0889090A);
	odm_write_4byte(dm, 0x1b58, 0x24890000);
	odm_write_4byte(dm, 0x1b5C, 0x079B080F);
	odm_write_4byte(dm, 0x1b58, 0x26890000);
	odm_write_4byte(dm, 0x1b5C, 0x06C7072E);
	odm_write_4byte(dm, 0x1b58, 0x28890000);
	odm_write_4byte(dm, 0x1b5C, 0x060B0666);
	odm_write_4byte(dm, 0x1b58, 0x2A890000);
	odm_write_4byte(dm, 0x1b5C, 0x056305B4);
	odm_write_4byte(dm, 0x1b58, 0x2C890000);
	odm_write_4byte(dm, 0x1b5C, 0x04CD0515);
	odm_write_4byte(dm, 0x1b58, 0x2E890000);
	odm_write_4byte(dm, 0x1b5C, 0x04470488);
	odm_write_4byte(dm, 0x1b58, 0x30890000);
	odm_write_4byte(dm, 0x1b5C, 0x03D0040A);
	odm_write_4byte(dm, 0x1b58, 0x32890000);
	odm_write_4byte(dm, 0x1b5C, 0x03660399);
	odm_write_4byte(dm, 0x1b58, 0x34890000);
	odm_write_4byte(dm, 0x1b5C, 0x03070335);
	odm_write_4byte(dm, 0x1b58, 0x36890000);
	odm_write_4byte(dm, 0x1b5C, 0x02B302DC);
	odm_write_4byte(dm, 0x1b58, 0x38890000);
	odm_write_4byte(dm, 0x1b5C, 0x0268028C);
	odm_write_4byte(dm, 0x1b58, 0x3A890000);
	odm_write_4byte(dm, 0x1b5C, 0x02250245);
	odm_write_4byte(dm, 0x1b58, 0x3C890000);
	odm_write_4byte(dm, 0x1b5C, 0x01E90206);
	odm_write_4byte(dm, 0x1b58, 0x3E890000);
	odm_write_4byte(dm, 0x1b5C, 0x01B401CE);
	odm_write_4byte(dm, 0x1b58, 0x40890000);
	odm_write_4byte(dm, 0x1b5C, 0x0185019C);
	odm_write_4byte(dm, 0x1b58, 0x42890000);
	odm_write_4byte(dm, 0x1b5C, 0x015A016F);
	odm_write_4byte(dm, 0x1b58, 0x44890000);
	odm_write_4byte(dm, 0x1b5C, 0x01350147);
	odm_write_4byte(dm, 0x1b58, 0x46890000);
	odm_write_4byte(dm, 0x1b5C, 0x01130123);
	odm_write_4byte(dm, 0x1b58, 0x48890000);
	odm_write_4byte(dm, 0x1b5C, 0x00F50104);
	odm_write_4byte(dm, 0x1b58, 0x4A890000);
	odm_write_4byte(dm, 0x1b5C, 0x00DA00E7);
	odm_write_4byte(dm, 0x1b58, 0x4C890000);
	odm_write_4byte(dm, 0x1b5C, 0x00C300CE);
	odm_write_4byte(dm, 0x1b58, 0x4E890000);
	odm_write_4byte(dm, 0x1b5C, 0x00AE00B8);
	odm_write_4byte(dm, 0x1b58, 0x50890000);
	odm_write_4byte(dm, 0x1b5C, 0x009B00A4);
	odm_write_4byte(dm, 0x1b58, 0x52890000);
	odm_write_4byte(dm, 0x1b5C, 0x008A0092);
	odm_write_4byte(dm, 0x1b58, 0x54890000);
	odm_write_4byte(dm, 0x1b5C, 0x007B0082);
	odm_write_4byte(dm, 0x1b58, 0x56890000);
	odm_write_4byte(dm, 0x1b5C, 0x006E0074);
	odm_write_4byte(dm, 0x1b58, 0x58890000);
	odm_write_4byte(dm, 0x1b5C, 0x00620067);
	odm_write_4byte(dm, 0x1b58, 0x5A890000);
	odm_write_4byte(dm, 0x1b5C, 0x0057005C);
	odm_write_4byte(dm, 0x1b58, 0x5C890000);
	odm_write_4byte(dm, 0x1b5C, 0x004E0052);
	odm_write_4byte(dm, 0x1b58, 0x5E890000);
	odm_write_4byte(dm, 0x1b5C, 0x00450049);
	odm_write_4byte(dm, 0x1b58, 0x60890000);
	odm_write_4byte(dm, 0x1b5C, 0x003E0041);
	odm_write_4byte(dm, 0x1b58, 0x62890000);
	odm_write_4byte(dm, 0x1b5C, 0x0037003A);
	odm_write_4byte(dm, 0x1b58, 0x62010000);
	// ============== Subpage 2 Init_Setting =========== //);
	odm_write_4byte(dm, 0x1b00, 0x0000000C);
	// set LMS parameters 
	odm_write_4byte(dm, 0x1bB8, 0x20202020);
	odm_write_4byte(dm, 0x1bBC, 0x20202020);
	odm_write_4byte(dm, 0x1bC0, 0x20202020);
	odm_write_4byte(dm, 0x1bC4, 0x20202020);
	odm_write_4byte(dm, 0x1bC8, 0x20202020);
	odm_write_4byte(dm, 0x1bCC, 0x20202020);
	odm_write_4byte(dm, 0x1bD0, 0x20202020);
	odm_write_4byte(dm, 0x1bD8, 0x20202020);
	odm_write_4byte(dm, 0x1bDC, 0x20202020);
	odm_write_4byte(dm, 0x1bE0, 0x20202020);
	odm_write_4byte(dm, 0x1bE4, 0x09050301);
	odm_write_4byte(dm, 0x1bE8, 0x130F0D0B);
	odm_write_4byte(dm, 0x1bEC, 0x00000000);
	odm_write_4byte(dm, 0x1bF0, 0x00000000);
	// MDPK init reg settings
	odm_write_4byte(dm, 0x1b04, 0x30000080);
	odm_write_4byte(dm, 0x1b08, 0x00004000);
	odm_write_4byte(dm, 0x1b5C, 0x30000080);
	odm_write_4byte(dm, 0x1b60, 0x00004000);
	odm_write_4byte(dm, 0x1bb4, 0x20000000);
	// ================= NCTL ============ //
	// nctl_8822C_20180626_wi_iqk_dpk_v1_driver
	odm_write_4byte(dm, 0x1b00, 0x00000008);
	odm_write_4byte(dm, 0x1b80, 0x00000007);
	odm_write_4byte(dm, 0x1b80, 0x00080005);
	odm_write_4byte(dm, 0x1b80, 0x00080007);
	odm_write_4byte(dm, 0x1b80, 0x80000015);
	odm_write_4byte(dm, 0x1b80, 0x80000017);
	odm_write_4byte(dm, 0x1b80, 0x09080025);
	odm_write_4byte(dm, 0x1b80, 0x09080027);
	odm_write_4byte(dm, 0x1b80, 0x0f020035);
	odm_write_4byte(dm, 0x1b80, 0x0f020037);
	odm_write_4byte(dm, 0x1b80, 0x00220045);
	odm_write_4byte(dm, 0x1b80, 0x00220047);
	odm_write_4byte(dm, 0x1b80, 0x00040055);
	odm_write_4byte(dm, 0x1b80, 0x00040057);
	odm_write_4byte(dm, 0x1b80, 0x05c00065);
	odm_write_4byte(dm, 0x1b80, 0x05c00067);
	odm_write_4byte(dm, 0x1b80, 0x00070075);
	odm_write_4byte(dm, 0x1b80, 0x00070077);
	odm_write_4byte(dm, 0x1b80, 0x64020085);
	odm_write_4byte(dm, 0x1b80, 0x64020087);
	odm_write_4byte(dm, 0x1b80, 0x00020095);
	odm_write_4byte(dm, 0x1b80, 0x00020097);
	odm_write_4byte(dm, 0x1b80, 0x000400a5);
	odm_write_4byte(dm, 0x1b80, 0x000400a7);
	odm_write_4byte(dm, 0x1b80, 0x4a0000b5);
	odm_write_4byte(dm, 0x1b80, 0x4a0000b7);
	odm_write_4byte(dm, 0x1b80, 0x4b0400c5);
	odm_write_4byte(dm, 0x1b80, 0x4b0400c7);
	odm_write_4byte(dm, 0x1b80, 0x860300d5);
	odm_write_4byte(dm, 0x1b80, 0x860300d7);
	odm_write_4byte(dm, 0x1b80, 0x400900e5);
	odm_write_4byte(dm, 0x1b80, 0x400900e7);
	odm_write_4byte(dm, 0x1b80, 0xe02700f5);
	odm_write_4byte(dm, 0x1b80, 0xe02700f7);
	odm_write_4byte(dm, 0x1b80, 0x4b050105);
	odm_write_4byte(dm, 0x1b80, 0x4b050107);
	odm_write_4byte(dm, 0x1b80, 0x87030115);
	odm_write_4byte(dm, 0x1b80, 0x87030117);
	odm_write_4byte(dm, 0x1b80, 0x400b0125);
	odm_write_4byte(dm, 0x1b80, 0x400b0127);
	odm_write_4byte(dm, 0x1b80, 0xe0270135);
	odm_write_4byte(dm, 0x1b80, 0xe0270137);
	odm_write_4byte(dm, 0x1b80, 0x4b060145);
	odm_write_4byte(dm, 0x1b80, 0x4b060147);
	odm_write_4byte(dm, 0x1b80, 0x88030155);
	odm_write_4byte(dm, 0x1b80, 0x88030157);
	odm_write_4byte(dm, 0x1b80, 0x400d0165);
	odm_write_4byte(dm, 0x1b80, 0x400d0167);
	odm_write_4byte(dm, 0x1b80, 0xe0270175);
	odm_write_4byte(dm, 0x1b80, 0xe0270177);
	odm_write_4byte(dm, 0x1b80, 0x4b000185);
	odm_write_4byte(dm, 0x1b80, 0x4b000187);
	odm_write_4byte(dm, 0x1b80, 0x00070195);
	odm_write_4byte(dm, 0x1b80, 0x00070197);
	odm_write_4byte(dm, 0x1b80, 0x4c0001a5);
	odm_write_4byte(dm, 0x1b80, 0x4c0001a7);
	odm_write_4byte(dm, 0x1b80, 0x000401b5);
	odm_write_4byte(dm, 0x1b80, 0x000401b7);
	odm_write_4byte(dm, 0x1b80, 0x400801c5);
	odm_write_4byte(dm, 0x1b80, 0x400801c7);
	odm_write_4byte(dm, 0x1b80, 0x505501d5);
	odm_write_4byte(dm, 0x1b80, 0x505501d7);
	odm_write_4byte(dm, 0x1b80, 0x090a01e5);
	odm_write_4byte(dm, 0x1b80, 0x090a01e7);
	odm_write_4byte(dm, 0x1b80, 0x0ffe01f5);
	odm_write_4byte(dm, 0x1b80, 0x0ffe01f7);
	odm_write_4byte(dm, 0x1b80, 0x00220205);
	odm_write_4byte(dm, 0x1b80, 0x00220207);
	odm_write_4byte(dm, 0x1b80, 0x00040215);
	odm_write_4byte(dm, 0x1b80, 0x00040217);
	odm_write_4byte(dm, 0x1b80, 0x05c00225);
	odm_write_4byte(dm, 0x1b80, 0x05c00227);
	odm_write_4byte(dm, 0x1b80, 0x00070235);
	odm_write_4byte(dm, 0x1b80, 0x00070237);
	odm_write_4byte(dm, 0x1b80, 0x64000245);
	odm_write_4byte(dm, 0x1b80, 0x64000247);
	odm_write_4byte(dm, 0x1b80, 0x00020255);
	odm_write_4byte(dm, 0x1b80, 0x00020257);
	odm_write_4byte(dm, 0x1b80, 0x30000265);
	odm_write_4byte(dm, 0x1b80, 0x30000267);
	odm_write_4byte(dm, 0x1b80, 0xa50d0275);
	odm_write_4byte(dm, 0x1b80, 0xa50d0277);
	odm_write_4byte(dm, 0x1b80, 0xe2a60285);
	odm_write_4byte(dm, 0x1b80, 0xe2a60287);
	odm_write_4byte(dm, 0x1b80, 0xf0180295);
	odm_write_4byte(dm, 0x1b80, 0xf0180297);
	odm_write_4byte(dm, 0x1b80, 0xf11802a5);
	odm_write_4byte(dm, 0x1b80, 0xf11802a7);
	odm_write_4byte(dm, 0x1b80, 0xf21802b5);
	odm_write_4byte(dm, 0x1b80, 0xf21802b7);
	odm_write_4byte(dm, 0x1b80, 0xf31802c5);
	odm_write_4byte(dm, 0x1b80, 0xf31802c7);
	odm_write_4byte(dm, 0x1b80, 0xf41802d5);
	odm_write_4byte(dm, 0x1b80, 0xf41802d7);
	odm_write_4byte(dm, 0x1b80, 0xf51802e5);
	odm_write_4byte(dm, 0x1b80, 0xf51802e7);
	odm_write_4byte(dm, 0x1b80, 0xf61802f5);
	odm_write_4byte(dm, 0x1b80, 0xf61802f7);
	odm_write_4byte(dm, 0x1b80, 0xf7180305);
	odm_write_4byte(dm, 0x1b80, 0xf7180307);
	odm_write_4byte(dm, 0x1b80, 0xf8180315);
	odm_write_4byte(dm, 0x1b80, 0xf8180317);
	odm_write_4byte(dm, 0x1b80, 0xf9180325);
	odm_write_4byte(dm, 0x1b80, 0xf9180327);
	odm_write_4byte(dm, 0x1b80, 0xfa180335);
	odm_write_4byte(dm, 0x1b80, 0xfa180337);
	odm_write_4byte(dm, 0x1b80, 0xf2180345);
	odm_write_4byte(dm, 0x1b80, 0xf2180347);
	odm_write_4byte(dm, 0x1b80, 0xf3180355);
	odm_write_4byte(dm, 0x1b80, 0xf3180357);
	odm_write_4byte(dm, 0x1b80, 0xf6180365);
	odm_write_4byte(dm, 0x1b80, 0xf6180367);
	odm_write_4byte(dm, 0x1b80, 0xf7180375);
	odm_write_4byte(dm, 0x1b80, 0xf7180377);
	odm_write_4byte(dm, 0x1b80, 0xf8180385);
	odm_write_4byte(dm, 0x1b80, 0xf8180387);
	odm_write_4byte(dm, 0x1b80, 0xf9180395);
	odm_write_4byte(dm, 0x1b80, 0xf9180397);
	odm_write_4byte(dm, 0x1b80, 0xfa1803a5);
	odm_write_4byte(dm, 0x1b80, 0xfa1803a7);
	odm_write_4byte(dm, 0x1b80, 0xfb1803b5);
	odm_write_4byte(dm, 0x1b80, 0xfb1803b7);
	odm_write_4byte(dm, 0x1b80, 0xfc1803c5);
	odm_write_4byte(dm, 0x1b80, 0xfc1803c7);
	odm_write_4byte(dm, 0x1b80, 0xfd1803d5);
	odm_write_4byte(dm, 0x1b80, 0xfd1803d7);
	odm_write_4byte(dm, 0x1b80, 0xfe1803e5);
	odm_write_4byte(dm, 0x1b80, 0xfe1803e7);
	odm_write_4byte(dm, 0x1b80, 0xff1803f5);
	odm_write_4byte(dm, 0x1b80, 0xff1803f7);
	odm_write_4byte(dm, 0x1b80, 0x00010405);
	odm_write_4byte(dm, 0x1b80, 0x00010407);
	odm_write_4byte(dm, 0x1b80, 0x30610415);
	odm_write_4byte(dm, 0x1b80, 0x30610417);
	odm_write_4byte(dm, 0x1b80, 0x30790425);
	odm_write_4byte(dm, 0x1b80, 0x30790427);
	odm_write_4byte(dm, 0x1b80, 0x30e20435);
	odm_write_4byte(dm, 0x1b80, 0x30e20437);
	odm_write_4byte(dm, 0x1b80, 0x307b0445);
	odm_write_4byte(dm, 0x1b80, 0x307b0447);
	odm_write_4byte(dm, 0x1b80, 0x30860455);
	odm_write_4byte(dm, 0x1b80, 0x30860457);
	odm_write_4byte(dm, 0x1b80, 0x30910465);
	odm_write_4byte(dm, 0x1b80, 0x30910467);
	odm_write_4byte(dm, 0x1b80, 0x30e60475);
	odm_write_4byte(dm, 0x1b80, 0x30e60477);
	odm_write_4byte(dm, 0x1b80, 0x30f10485);
	odm_write_4byte(dm, 0x1b80, 0x30f10487);
	odm_write_4byte(dm, 0x1b80, 0x30fc0495);
	odm_write_4byte(dm, 0x1b80, 0x30fc0497);
	odm_write_4byte(dm, 0x1b80, 0x316104a5);
	odm_write_4byte(dm, 0x1b80, 0x316104a7);
	odm_write_4byte(dm, 0x1b80, 0x305804b5);
	odm_write_4byte(dm, 0x1b80, 0x305804b7);
	odm_write_4byte(dm, 0x1b80, 0x307904c5);
	odm_write_4byte(dm, 0x1b80, 0x307904c7);
	odm_write_4byte(dm, 0x1b80, 0x30e004d5);
	odm_write_4byte(dm, 0x1b80, 0x30e004d7);
	odm_write_4byte(dm, 0x1b80, 0x317e04e5);
	odm_write_4byte(dm, 0x1b80, 0x317e04e7);
	odm_write_4byte(dm, 0x1b80, 0x318504f5);
	odm_write_4byte(dm, 0x1b80, 0x318504f7);
	odm_write_4byte(dm, 0x1b80, 0x318c0505);
	odm_write_4byte(dm, 0x1b80, 0x318c0507);
	odm_write_4byte(dm, 0x1b80, 0x31930515);
	odm_write_4byte(dm, 0x1b80, 0x31930517);
	odm_write_4byte(dm, 0x1b80, 0x319a0525);
	odm_write_4byte(dm, 0x1b80, 0x319a0527);
	odm_write_4byte(dm, 0x1b80, 0x31a30535);
	odm_write_4byte(dm, 0x1b80, 0x31a30537);
	odm_write_4byte(dm, 0x1b80, 0x31ac0545);
	odm_write_4byte(dm, 0x1b80, 0x31ac0547);
	odm_write_4byte(dm, 0x1b80, 0x31b20555);
	odm_write_4byte(dm, 0x1b80, 0x31b20557);
	odm_write_4byte(dm, 0x1b80, 0x31b80565);
	odm_write_4byte(dm, 0x1b80, 0x31b80567);
	odm_write_4byte(dm, 0x1b80, 0x31be0575);
	odm_write_4byte(dm, 0x1b80, 0x31be0577);
	odm_write_4byte(dm, 0x1b80, 0x4d040585);
	odm_write_4byte(dm, 0x1b80, 0x4d040587);
	odm_write_4byte(dm, 0x1b80, 0x20810595);
	odm_write_4byte(dm, 0x1b80, 0x20810597);
	odm_write_4byte(dm, 0x1b80, 0x234505a5);
	odm_write_4byte(dm, 0x1b80, 0x234505a7);
	odm_write_4byte(dm, 0x1b80, 0x200405b5);
	odm_write_4byte(dm, 0x1b80, 0x200405b7);
	odm_write_4byte(dm, 0x1b80, 0x001705c5);
	odm_write_4byte(dm, 0x1b80, 0x001705c7);
	odm_write_4byte(dm, 0x1b80, 0x234605d5);
	odm_write_4byte(dm, 0x1b80, 0x234605d7);
	odm_write_4byte(dm, 0x1b80, 0x789a05e5);
	odm_write_4byte(dm, 0x1b80, 0x789a05e7);
	odm_write_4byte(dm, 0x1b80, 0x4d0005f5);
	odm_write_4byte(dm, 0x1b80, 0x4d0005f7);
	odm_write_4byte(dm, 0x1b80, 0x00010605);
	odm_write_4byte(dm, 0x1b80, 0x00010607);
	odm_write_4byte(dm, 0x1b80, 0xe23f0615);
	odm_write_4byte(dm, 0x1b80, 0xe23f0617);
	odm_write_4byte(dm, 0x1b80, 0x4d040625);
	odm_write_4byte(dm, 0x1b80, 0x4d040627);
	odm_write_4byte(dm, 0x1b80, 0x20800635);
	odm_write_4byte(dm, 0x1b80, 0x20800637);
	odm_write_4byte(dm, 0x1b80, 0x00000645);
	odm_write_4byte(dm, 0x1b80, 0x00000647);
	odm_write_4byte(dm, 0x1b80, 0x4d000655);
	odm_write_4byte(dm, 0x1b80, 0x4d000657);
	odm_write_4byte(dm, 0x1b80, 0x55070665);
	odm_write_4byte(dm, 0x1b80, 0x55070667);
	odm_write_4byte(dm, 0x1b80, 0xe2370675);
	odm_write_4byte(dm, 0x1b80, 0xe2370677);
	odm_write_4byte(dm, 0x1b80, 0xe2370685);
	odm_write_4byte(dm, 0x1b80, 0xe2370687);
	odm_write_4byte(dm, 0x1b80, 0x4d040695);
	odm_write_4byte(dm, 0x1b80, 0x4d040697);
	odm_write_4byte(dm, 0x1b80, 0x208806a5);
	odm_write_4byte(dm, 0x1b80, 0x208806a7);
	odm_write_4byte(dm, 0x1b80, 0x020006b5);
	odm_write_4byte(dm, 0x1b80, 0x020006b7);
	odm_write_4byte(dm, 0x1b80, 0x4d0006c5);
	odm_write_4byte(dm, 0x1b80, 0x4d0006c7);
	odm_write_4byte(dm, 0x1b80, 0x550f06d5);
	odm_write_4byte(dm, 0x1b80, 0x550f06d7);
	odm_write_4byte(dm, 0x1b80, 0xe23706e5);
	odm_write_4byte(dm, 0x1b80, 0xe23706e7);
	odm_write_4byte(dm, 0x1b80, 0x4f0206f5);
	odm_write_4byte(dm, 0x1b80, 0x4f0206f7);
	odm_write_4byte(dm, 0x1b80, 0x4e000705);
	odm_write_4byte(dm, 0x1b80, 0x4e000707);
	odm_write_4byte(dm, 0x1b80, 0x53020715);
	odm_write_4byte(dm, 0x1b80, 0x53020717);
	odm_write_4byte(dm, 0x1b80, 0x52010725);
	odm_write_4byte(dm, 0x1b80, 0x52010727);
	odm_write_4byte(dm, 0x1b80, 0xe23b0735);
	odm_write_4byte(dm, 0x1b80, 0xe23b0737);
	odm_write_4byte(dm, 0x1b80, 0x4d080745);
	odm_write_4byte(dm, 0x1b80, 0x4d080747);
	odm_write_4byte(dm, 0x1b80, 0x57100755);
	odm_write_4byte(dm, 0x1b80, 0x57100757);
	odm_write_4byte(dm, 0x1b80, 0x57000765);
	odm_write_4byte(dm, 0x1b80, 0x57000767);
	odm_write_4byte(dm, 0x1b80, 0x4d000775);
	odm_write_4byte(dm, 0x1b80, 0x4d000777);
	odm_write_4byte(dm, 0x1b80, 0x00010785);
	odm_write_4byte(dm, 0x1b80, 0x00010787);
	odm_write_4byte(dm, 0x1b80, 0xe23f0795);
	odm_write_4byte(dm, 0x1b80, 0xe23f0797);
	odm_write_4byte(dm, 0x1b80, 0x000107a5);
	odm_write_4byte(dm, 0x1b80, 0x000107a7);
	odm_write_4byte(dm, 0x1b80, 0x30a607b5);
	odm_write_4byte(dm, 0x1b80, 0x30a607b7);
	odm_write_4byte(dm, 0x1b80, 0x002607c5);
	odm_write_4byte(dm, 0x1b80, 0x002607c7);
	odm_write_4byte(dm, 0x1b80, 0xe29907d5);
	odm_write_4byte(dm, 0x1b80, 0xe29907d7);
	odm_write_4byte(dm, 0x1b80, 0x000207e5);
	odm_write_4byte(dm, 0x1b80, 0x000207e7);
	odm_write_4byte(dm, 0x1b80, 0x54ec07f5);
	odm_write_4byte(dm, 0x1b80, 0x54ec07f7);
	odm_write_4byte(dm, 0x1b80, 0x0ba60805);
	odm_write_4byte(dm, 0x1b80, 0x0ba60807);
	odm_write_4byte(dm, 0x1b80, 0x00260815);
	odm_write_4byte(dm, 0x1b80, 0x00260817);
	odm_write_4byte(dm, 0x1b80, 0xe2990825);
	odm_write_4byte(dm, 0x1b80, 0xe2990827);
	odm_write_4byte(dm, 0x1b80, 0x00020835);
	odm_write_4byte(dm, 0x1b80, 0x00020837);
	odm_write_4byte(dm, 0x1b80, 0x63c30845);
	odm_write_4byte(dm, 0x1b80, 0x63c30847);
	odm_write_4byte(dm, 0x1b80, 0x30d00855);
	odm_write_4byte(dm, 0x1b80, 0x30d00857);
	odm_write_4byte(dm, 0x1b80, 0x309e0865);
	odm_write_4byte(dm, 0x1b80, 0x309e0867);
	odm_write_4byte(dm, 0x1b80, 0x00240875);
	odm_write_4byte(dm, 0x1b80, 0x00240877);
	odm_write_4byte(dm, 0x1b80, 0xe2990885);
	odm_write_4byte(dm, 0x1b80, 0xe2990887);
	odm_write_4byte(dm, 0x1b80, 0x00020895);
	odm_write_4byte(dm, 0x1b80, 0x00020897);
	odm_write_4byte(dm, 0x1b80, 0x54ea08a5);
	odm_write_4byte(dm, 0x1b80, 0x54ea08a7);
	odm_write_4byte(dm, 0x1b80, 0x0ba608b5);
	odm_write_4byte(dm, 0x1b80, 0x0ba608b7);
	odm_write_4byte(dm, 0x1b80, 0x002408c5);
	odm_write_4byte(dm, 0x1b80, 0x002408c7);
	odm_write_4byte(dm, 0x1b80, 0xe29908d5);
	odm_write_4byte(dm, 0x1b80, 0xe29908d7);
	odm_write_4byte(dm, 0x1b80, 0x000208e5);
	odm_write_4byte(dm, 0x1b80, 0x000208e7);
	odm_write_4byte(dm, 0x1b80, 0x63c308f5);
	odm_write_4byte(dm, 0x1b80, 0x63c308f7);
	odm_write_4byte(dm, 0x1b80, 0x30d00905);
	odm_write_4byte(dm, 0x1b80, 0x30d00907);
	odm_write_4byte(dm, 0x1b80, 0x6c100915);
	odm_write_4byte(dm, 0x1b80, 0x6c100917);
	odm_write_4byte(dm, 0x1b80, 0x6d0f0925);
	odm_write_4byte(dm, 0x1b80, 0x6d0f0927);
	odm_write_4byte(dm, 0x1b80, 0xe23f0935);
	odm_write_4byte(dm, 0x1b80, 0xe23f0937);
	odm_write_4byte(dm, 0x1b80, 0xe2990945);
	odm_write_4byte(dm, 0x1b80, 0xe2990947);
	odm_write_4byte(dm, 0x1b80, 0x6c240955);
	odm_write_4byte(dm, 0x1b80, 0x6c240957);
	odm_write_4byte(dm, 0x1b80, 0xe23f0965);
	odm_write_4byte(dm, 0x1b80, 0xe23f0967);
	odm_write_4byte(dm, 0x1b80, 0xe2990975);
	odm_write_4byte(dm, 0x1b80, 0xe2990977);
	odm_write_4byte(dm, 0x1b80, 0x6c440985);
	odm_write_4byte(dm, 0x1b80, 0x6c440987);
	odm_write_4byte(dm, 0x1b80, 0xe23f0995);
	odm_write_4byte(dm, 0x1b80, 0xe23f0997);
	odm_write_4byte(dm, 0x1b80, 0xe29909a5);
	odm_write_4byte(dm, 0x1b80, 0xe29909a7);
	odm_write_4byte(dm, 0x1b80, 0x6c6409b5);
	odm_write_4byte(dm, 0x1b80, 0x6c6409b7);
	odm_write_4byte(dm, 0x1b80, 0xe23f09c5);
	odm_write_4byte(dm, 0x1b80, 0xe23f09c7);
	odm_write_4byte(dm, 0x1b80, 0xe29909d5);
	odm_write_4byte(dm, 0x1b80, 0xe29909d7);
	odm_write_4byte(dm, 0x1b80, 0x0baa09e5);
	odm_write_4byte(dm, 0x1b80, 0x0baa09e7);
	odm_write_4byte(dm, 0x1b80, 0x6c8409f5);
	odm_write_4byte(dm, 0x1b80, 0x6c8409f7);
	odm_write_4byte(dm, 0x1b80, 0x6d0f0a05);
	odm_write_4byte(dm, 0x1b80, 0x6d0f0a07);
	odm_write_4byte(dm, 0x1b80, 0xe23f0a15);
	odm_write_4byte(dm, 0x1b80, 0xe23f0a17);
	odm_write_4byte(dm, 0x1b80, 0xe2990a25);
	odm_write_4byte(dm, 0x1b80, 0xe2990a27);
	odm_write_4byte(dm, 0x1b80, 0x6ca40a35);
	odm_write_4byte(dm, 0x1b80, 0x6ca40a37);
	odm_write_4byte(dm, 0x1b80, 0xe23f0a45);
	odm_write_4byte(dm, 0x1b80, 0xe23f0a47);
	odm_write_4byte(dm, 0x1b80, 0xe2990a55);
	odm_write_4byte(dm, 0x1b80, 0xe2990a57);
	odm_write_4byte(dm, 0x1b80, 0x0bac0a65);
	odm_write_4byte(dm, 0x1b80, 0x0bac0a67);
	odm_write_4byte(dm, 0x1b80, 0x6cc40a75);
	odm_write_4byte(dm, 0x1b80, 0x6cc40a77);
	odm_write_4byte(dm, 0x1b80, 0x6d0f0a85);
	odm_write_4byte(dm, 0x1b80, 0x6d0f0a87);
	odm_write_4byte(dm, 0x1b80, 0xe23f0a95);
	odm_write_4byte(dm, 0x1b80, 0xe23f0a97);
	odm_write_4byte(dm, 0x1b80, 0xe2990aa5);
	odm_write_4byte(dm, 0x1b80, 0xe2990aa7);
	odm_write_4byte(dm, 0x1b80, 0x6ce40ab5);
	odm_write_4byte(dm, 0x1b80, 0x6ce40ab7);
	odm_write_4byte(dm, 0x1b80, 0xe23f0ac5);
	odm_write_4byte(dm, 0x1b80, 0xe23f0ac7);
	odm_write_4byte(dm, 0x1b80, 0xe2990ad5);
	odm_write_4byte(dm, 0x1b80, 0xe2990ad7);
	odm_write_4byte(dm, 0x1b80, 0x6cf40ae5);
	odm_write_4byte(dm, 0x1b80, 0x6cf40ae7);
	odm_write_4byte(dm, 0x1b80, 0xe23f0af5);
	odm_write_4byte(dm, 0x1b80, 0xe23f0af7);
	odm_write_4byte(dm, 0x1b80, 0xe2990b05);
	odm_write_4byte(dm, 0x1b80, 0xe2990b07);
	odm_write_4byte(dm, 0x1b80, 0x6c0c0b15);
	odm_write_4byte(dm, 0x1b80, 0x6c0c0b17);
	odm_write_4byte(dm, 0x1b80, 0x6d000b25);
	odm_write_4byte(dm, 0x1b80, 0x6d000b27);
	odm_write_4byte(dm, 0x1b80, 0xe23f0b35);
	odm_write_4byte(dm, 0x1b80, 0xe23f0b37);
	odm_write_4byte(dm, 0x1b80, 0xe2990b45);
	odm_write_4byte(dm, 0x1b80, 0xe2990b47);
	odm_write_4byte(dm, 0x1b80, 0x6c1c0b55);
	odm_write_4byte(dm, 0x1b80, 0x6c1c0b57);
	odm_write_4byte(dm, 0x1b80, 0xe23f0b65);
	odm_write_4byte(dm, 0x1b80, 0xe23f0b67);
	odm_write_4byte(dm, 0x1b80, 0xe2990b75);
	odm_write_4byte(dm, 0x1b80, 0xe2990b77);
	odm_write_4byte(dm, 0x1b80, 0x6c3c0b85);
	odm_write_4byte(dm, 0x1b80, 0x6c3c0b87);
	odm_write_4byte(dm, 0x1b80, 0xe23f0b95);
	odm_write_4byte(dm, 0x1b80, 0xe23f0b97);
	odm_write_4byte(dm, 0x1b80, 0xe2990ba5);
	odm_write_4byte(dm, 0x1b80, 0xe2990ba7);
	odm_write_4byte(dm, 0x1b80, 0xf3c10bb5);
	odm_write_4byte(dm, 0x1b80, 0xf3c10bb7);
	odm_write_4byte(dm, 0x1b80, 0x6c5c0bc5);
	odm_write_4byte(dm, 0x1b80, 0x6c5c0bc7);
	odm_write_4byte(dm, 0x1b80, 0xe23f0bd5);
	odm_write_4byte(dm, 0x1b80, 0xe23f0bd7);
	odm_write_4byte(dm, 0x1b80, 0xe2990be5);
	odm_write_4byte(dm, 0x1b80, 0xe2990be7);
	odm_write_4byte(dm, 0x1b80, 0x6c7c0bf5);
	odm_write_4byte(dm, 0x1b80, 0x6c7c0bf7);
	odm_write_4byte(dm, 0x1b80, 0xe23f0c05);
	odm_write_4byte(dm, 0x1b80, 0xe23f0c07);
	odm_write_4byte(dm, 0x1b80, 0xe2990c15);
	odm_write_4byte(dm, 0x1b80, 0xe2990c17);
	odm_write_4byte(dm, 0x1b80, 0xf4c50c25);
	odm_write_4byte(dm, 0x1b80, 0xf4c50c27);
	odm_write_4byte(dm, 0x1b80, 0x6c9c0c35);
	odm_write_4byte(dm, 0x1b80, 0x6c9c0c37);
	odm_write_4byte(dm, 0x1b80, 0xe23f0c45);
	odm_write_4byte(dm, 0x1b80, 0xe23f0c47);
	odm_write_4byte(dm, 0x1b80, 0xe2990c55);
	odm_write_4byte(dm, 0x1b80, 0xe2990c57);
	odm_write_4byte(dm, 0x1b80, 0x6cbc0c65);
	odm_write_4byte(dm, 0x1b80, 0x6cbc0c67);
	odm_write_4byte(dm, 0x1b80, 0xe23f0c75);
	odm_write_4byte(dm, 0x1b80, 0xe23f0c77);
	odm_write_4byte(dm, 0x1b80, 0xe2990c85);
	odm_write_4byte(dm, 0x1b80, 0xe2990c87);
	odm_write_4byte(dm, 0x1b80, 0x6cdc0c95);
	odm_write_4byte(dm, 0x1b80, 0x6cdc0c97);
	odm_write_4byte(dm, 0x1b80, 0xe23f0ca5);
	odm_write_4byte(dm, 0x1b80, 0xe23f0ca7);
	odm_write_4byte(dm, 0x1b80, 0xe2990cb5);
	odm_write_4byte(dm, 0x1b80, 0xe2990cb7);
	odm_write_4byte(dm, 0x1b80, 0x6cf00cc5);
	odm_write_4byte(dm, 0x1b80, 0x6cf00cc7);
	odm_write_4byte(dm, 0x1b80, 0xe23f0cd5);
	odm_write_4byte(dm, 0x1b80, 0xe23f0cd7);
	odm_write_4byte(dm, 0x1b80, 0xe2990ce5);
	odm_write_4byte(dm, 0x1b80, 0xe2990ce7);
	odm_write_4byte(dm, 0x1b80, 0x63c30cf5);
	odm_write_4byte(dm, 0x1b80, 0x63c30cf7);
	odm_write_4byte(dm, 0x1b80, 0x55010d05);
	odm_write_4byte(dm, 0x1b80, 0x55010d07);
	odm_write_4byte(dm, 0x1b80, 0x57040d15);
	odm_write_4byte(dm, 0x1b80, 0x57040d17);
	odm_write_4byte(dm, 0x1b80, 0x57000d25);
	odm_write_4byte(dm, 0x1b80, 0x57000d27);
	odm_write_4byte(dm, 0x1b80, 0x96000d35);
	odm_write_4byte(dm, 0x1b80, 0x96000d37);
	odm_write_4byte(dm, 0x1b80, 0x57080d45);
	odm_write_4byte(dm, 0x1b80, 0x57080d47);
	odm_write_4byte(dm, 0x1b80, 0x57000d55);
	odm_write_4byte(dm, 0x1b80, 0x57000d57);
	odm_write_4byte(dm, 0x1b80, 0x95000d65);
	odm_write_4byte(dm, 0x1b80, 0x95000d67);
	odm_write_4byte(dm, 0x1b80, 0x4d000d75);
	odm_write_4byte(dm, 0x1b80, 0x4d000d77);
	odm_write_4byte(dm, 0x1b80, 0x63070d85);
	odm_write_4byte(dm, 0x1b80, 0x63070d87);
	odm_write_4byte(dm, 0x1b80, 0x7b400d95);
	odm_write_4byte(dm, 0x1b80, 0x7b400d97);
	odm_write_4byte(dm, 0x1b80, 0x7a000da5);
	odm_write_4byte(dm, 0x1b80, 0x7a000da7);
	odm_write_4byte(dm, 0x1b80, 0x79000db5);
	odm_write_4byte(dm, 0x1b80, 0x79000db7);
	odm_write_4byte(dm, 0x1b80, 0x7f400dc5);
	odm_write_4byte(dm, 0x1b80, 0x7f400dc7);
	odm_write_4byte(dm, 0x1b80, 0x7e000dd5);
	odm_write_4byte(dm, 0x1b80, 0x7e000dd7);
	odm_write_4byte(dm, 0x1b80, 0x7d000de5);
	odm_write_4byte(dm, 0x1b80, 0x7d000de7);
	odm_write_4byte(dm, 0x1b80, 0x00010df5);
	odm_write_4byte(dm, 0x1b80, 0x00010df7);
	odm_write_4byte(dm, 0x1b80, 0xe26b0e05);
	odm_write_4byte(dm, 0x1b80, 0xe26b0e07);
	odm_write_4byte(dm, 0x1b80, 0x00010e15);
	odm_write_4byte(dm, 0x1b80, 0x00010e17);
	odm_write_4byte(dm, 0x1b80, 0x5c320e25);
	odm_write_4byte(dm, 0x1b80, 0x5c320e27);
	odm_write_4byte(dm, 0x1b80, 0xe2950e35);
	odm_write_4byte(dm, 0x1b80, 0xe2950e37);
	odm_write_4byte(dm, 0x1b80, 0xe26b0e45);
	odm_write_4byte(dm, 0x1b80, 0xe26b0e47);
	odm_write_4byte(dm, 0x1b80, 0x00010e55);
	odm_write_4byte(dm, 0x1b80, 0x00010e57);
	odm_write_4byte(dm, 0x1b80, 0x311d0e65);
	odm_write_4byte(dm, 0x1b80, 0x311d0e67);
	odm_write_4byte(dm, 0x1b80, 0x00260e75);
	odm_write_4byte(dm, 0x1b80, 0x00260e77);
	odm_write_4byte(dm, 0x1b80, 0xe29e0e85);
	odm_write_4byte(dm, 0x1b80, 0xe29e0e87);
	odm_write_4byte(dm, 0x1b80, 0x00020e95);
	odm_write_4byte(dm, 0x1b80, 0x00020e97);
	odm_write_4byte(dm, 0x1b80, 0x54ec0ea5);
	odm_write_4byte(dm, 0x1b80, 0x54ec0ea7);
	odm_write_4byte(dm, 0x1b80, 0x0ba60eb5);
	odm_write_4byte(dm, 0x1b80, 0x0ba60eb7);
	odm_write_4byte(dm, 0x1b80, 0x00260ec5);
	odm_write_4byte(dm, 0x1b80, 0x00260ec7);
	odm_write_4byte(dm, 0x1b80, 0xe29e0ed5);
	odm_write_4byte(dm, 0x1b80, 0xe29e0ed7);
	odm_write_4byte(dm, 0x1b80, 0x00020ee5);
	odm_write_4byte(dm, 0x1b80, 0x00020ee7);
	odm_write_4byte(dm, 0x1b80, 0x63830ef5);
	odm_write_4byte(dm, 0x1b80, 0x63830ef7);
	odm_write_4byte(dm, 0x1b80, 0x30d00f05);
	odm_write_4byte(dm, 0x1b80, 0x30d00f07);
	odm_write_4byte(dm, 0x1b80, 0x31110f15);
	odm_write_4byte(dm, 0x1b80, 0x31110f17);
	odm_write_4byte(dm, 0x1b80, 0x00240f25);
	odm_write_4byte(dm, 0x1b80, 0x00240f27);
	odm_write_4byte(dm, 0x1b80, 0xe29e0f35);
	odm_write_4byte(dm, 0x1b80, 0xe29e0f37);
	odm_write_4byte(dm, 0x1b80, 0x00020f45);
	odm_write_4byte(dm, 0x1b80, 0x00020f47);
	odm_write_4byte(dm, 0x1b80, 0x54ea0f55);
	odm_write_4byte(dm, 0x1b80, 0x54ea0f57);
	odm_write_4byte(dm, 0x1b80, 0x0ba60f65);
	odm_write_4byte(dm, 0x1b80, 0x0ba60f67);
	odm_write_4byte(dm, 0x1b80, 0x00240f75);
	odm_write_4byte(dm, 0x1b80, 0x00240f77);
	odm_write_4byte(dm, 0x1b80, 0xe29e0f85);
	odm_write_4byte(dm, 0x1b80, 0xe29e0f87);
	odm_write_4byte(dm, 0x1b80, 0x00020f95);
	odm_write_4byte(dm, 0x1b80, 0x00020f97);
	odm_write_4byte(dm, 0x1b80, 0x63830fa5);
	odm_write_4byte(dm, 0x1b80, 0x63830fa7);
	odm_write_4byte(dm, 0x1b80, 0x30d00fb5);
	odm_write_4byte(dm, 0x1b80, 0x30d00fb7);
	odm_write_4byte(dm, 0x1b80, 0x5c320fc5);
	odm_write_4byte(dm, 0x1b80, 0x5c320fc7);
	odm_write_4byte(dm, 0x1b80, 0x54e60fd5);
	odm_write_4byte(dm, 0x1b80, 0x54e60fd7);
	odm_write_4byte(dm, 0x1b80, 0x6e100fe5);
	odm_write_4byte(dm, 0x1b80, 0x6e100fe7);
	odm_write_4byte(dm, 0x1b80, 0x6f0f0ff5);
	odm_write_4byte(dm, 0x1b80, 0x6f0f0ff7);
	odm_write_4byte(dm, 0x1b80, 0xe26b1005);
	odm_write_4byte(dm, 0x1b80, 0xe26b1007);
	odm_write_4byte(dm, 0x1b80, 0xe29e1015);
	odm_write_4byte(dm, 0x1b80, 0xe29e1017);
	odm_write_4byte(dm, 0x1b80, 0x5c321025);
	odm_write_4byte(dm, 0x1b80, 0x5c321027);
	odm_write_4byte(dm, 0x1b80, 0x54e71035);
	odm_write_4byte(dm, 0x1b80, 0x54e71037);
	odm_write_4byte(dm, 0x1b80, 0x6e241045);
	odm_write_4byte(dm, 0x1b80, 0x6e241047);
	odm_write_4byte(dm, 0x1b80, 0xe26b1055);
	odm_write_4byte(dm, 0x1b80, 0xe26b1057);
	odm_write_4byte(dm, 0x1b80, 0xe29e1065);
	odm_write_4byte(dm, 0x1b80, 0xe29e1067);
	odm_write_4byte(dm, 0x1b80, 0x5c321075);
	odm_write_4byte(dm, 0x1b80, 0x5c321077);
	odm_write_4byte(dm, 0x1b80, 0x54e81085);
	odm_write_4byte(dm, 0x1b80, 0x54e81087);
	odm_write_4byte(dm, 0x1b80, 0x6e441095);
	odm_write_4byte(dm, 0x1b80, 0x6e441097);
	odm_write_4byte(dm, 0x1b80, 0xe26b10a5);
	odm_write_4byte(dm, 0x1b80, 0xe26b10a7);
	odm_write_4byte(dm, 0x1b80, 0xe29e10b5);
	odm_write_4byte(dm, 0x1b80, 0xe29e10b7);
	odm_write_4byte(dm, 0x1b80, 0x5c3210c5);
	odm_write_4byte(dm, 0x1b80, 0x5c3210c7);
	odm_write_4byte(dm, 0x1b80, 0x54e910d5);
	odm_write_4byte(dm, 0x1b80, 0x54e910d7);
	odm_write_4byte(dm, 0x1b80, 0x6e6410e5);
	odm_write_4byte(dm, 0x1b80, 0x6e6410e7);
	odm_write_4byte(dm, 0x1b80, 0xe26b10f5);
	odm_write_4byte(dm, 0x1b80, 0xe26b10f7);
	odm_write_4byte(dm, 0x1b80, 0xe29e1105);
	odm_write_4byte(dm, 0x1b80, 0xe29e1107);
	odm_write_4byte(dm, 0x1b80, 0x5c321115);
	odm_write_4byte(dm, 0x1b80, 0x5c321117);
	odm_write_4byte(dm, 0x1b80, 0x54ea1125);
	odm_write_4byte(dm, 0x1b80, 0x54ea1127);
	odm_write_4byte(dm, 0x1b80, 0x0baa1135);
	odm_write_4byte(dm, 0x1b80, 0x0baa1137);
	odm_write_4byte(dm, 0x1b80, 0x6e841145);
	odm_write_4byte(dm, 0x1b80, 0x6e841147);
	odm_write_4byte(dm, 0x1b80, 0x6f0f1155);
	odm_write_4byte(dm, 0x1b80, 0x6f0f1157);
	odm_write_4byte(dm, 0x1b80, 0xe26b1165);
	odm_write_4byte(dm, 0x1b80, 0xe26b1167);
	odm_write_4byte(dm, 0x1b80, 0xe29e1175);
	odm_write_4byte(dm, 0x1b80, 0xe29e1177);
	odm_write_4byte(dm, 0x1b80, 0x5c321185);
	odm_write_4byte(dm, 0x1b80, 0x5c321187);
	odm_write_4byte(dm, 0x1b80, 0x54eb1195);
	odm_write_4byte(dm, 0x1b80, 0x54eb1197);
	odm_write_4byte(dm, 0x1b80, 0x6ea411a5);
	odm_write_4byte(dm, 0x1b80, 0x6ea411a7);
	odm_write_4byte(dm, 0x1b80, 0xe26b11b5);
	odm_write_4byte(dm, 0x1b80, 0xe26b11b7);
	odm_write_4byte(dm, 0x1b80, 0xe29e11c5);
	odm_write_4byte(dm, 0x1b80, 0xe29e11c7);
	odm_write_4byte(dm, 0x1b80, 0x5c3211d5);
	odm_write_4byte(dm, 0x1b80, 0x5c3211d7);
	odm_write_4byte(dm, 0x1b80, 0x54ec11e5);
	odm_write_4byte(dm, 0x1b80, 0x54ec11e7);
	odm_write_4byte(dm, 0x1b80, 0x0bac11f5);
	odm_write_4byte(dm, 0x1b80, 0x0bac11f7);
	odm_write_4byte(dm, 0x1b80, 0x6ec41205);
	odm_write_4byte(dm, 0x1b80, 0x6ec41207);
	odm_write_4byte(dm, 0x1b80, 0x6f0f1215);
	odm_write_4byte(dm, 0x1b80, 0x6f0f1217);
	odm_write_4byte(dm, 0x1b80, 0xe26b1225);
	odm_write_4byte(dm, 0x1b80, 0xe26b1227);
	odm_write_4byte(dm, 0x1b80, 0xe29e1235);
	odm_write_4byte(dm, 0x1b80, 0xe29e1237);
	odm_write_4byte(dm, 0x1b80, 0x5c321245);
	odm_write_4byte(dm, 0x1b80, 0x5c321247);
	odm_write_4byte(dm, 0x1b80, 0x54ed1255);
	odm_write_4byte(dm, 0x1b80, 0x54ed1257);
	odm_write_4byte(dm, 0x1b80, 0x6ee41265);
	odm_write_4byte(dm, 0x1b80, 0x6ee41267);
	odm_write_4byte(dm, 0x1b80, 0xe26b1275);
	odm_write_4byte(dm, 0x1b80, 0xe26b1277);
	odm_write_4byte(dm, 0x1b80, 0xe29e1285);
	odm_write_4byte(dm, 0x1b80, 0xe29e1287);
	odm_write_4byte(dm, 0x1b80, 0x5c321295);
	odm_write_4byte(dm, 0x1b80, 0x5c321297);
	odm_write_4byte(dm, 0x1b80, 0x54ee12a5);
	odm_write_4byte(dm, 0x1b80, 0x54ee12a7);
	odm_write_4byte(dm, 0x1b80, 0x6ef412b5);
	odm_write_4byte(dm, 0x1b80, 0x6ef412b7);
	odm_write_4byte(dm, 0x1b80, 0xe26b12c5);
	odm_write_4byte(dm, 0x1b80, 0xe26b12c7);
	odm_write_4byte(dm, 0x1b80, 0xe29e12d5);
	odm_write_4byte(dm, 0x1b80, 0xe29e12d7);
	odm_write_4byte(dm, 0x1b80, 0x5c3212e5);
	odm_write_4byte(dm, 0x1b80, 0x5c3212e7);
	odm_write_4byte(dm, 0x1b80, 0x54ef12f5);
	odm_write_4byte(dm, 0x1b80, 0x54ef12f7);
	odm_write_4byte(dm, 0x1b80, 0x6e0c1305);
	odm_write_4byte(dm, 0x1b80, 0x6e0c1307);
	odm_write_4byte(dm, 0x1b80, 0x6f001315);
	odm_write_4byte(dm, 0x1b80, 0x6f001317);
	odm_write_4byte(dm, 0x1b80, 0xe26b1325);
	odm_write_4byte(dm, 0x1b80, 0xe26b1327);
	odm_write_4byte(dm, 0x1b80, 0xe29e1335);
	odm_write_4byte(dm, 0x1b80, 0xe29e1337);
	odm_write_4byte(dm, 0x1b80, 0x5c321345);
	odm_write_4byte(dm, 0x1b80, 0x5c321347);
	odm_write_4byte(dm, 0x1b80, 0x54f01355);
	odm_write_4byte(dm, 0x1b80, 0x54f01357);
	odm_write_4byte(dm, 0x1b80, 0x6e1c1365);
	odm_write_4byte(dm, 0x1b80, 0x6e1c1367);
	odm_write_4byte(dm, 0x1b80, 0xe26b1375);
	odm_write_4byte(dm, 0x1b80, 0xe26b1377);
	odm_write_4byte(dm, 0x1b80, 0xe29e1385);
	odm_write_4byte(dm, 0x1b80, 0xe29e1387);
	odm_write_4byte(dm, 0x1b80, 0x5c321395);
	odm_write_4byte(dm, 0x1b80, 0x5c321397);
	odm_write_4byte(dm, 0x1b80, 0x54f113a5);
	odm_write_4byte(dm, 0x1b80, 0x54f113a7);
	odm_write_4byte(dm, 0x1b80, 0x6e3c13b5);
	odm_write_4byte(dm, 0x1b80, 0x6e3c13b7);
	odm_write_4byte(dm, 0x1b80, 0xe26b13c5);
	odm_write_4byte(dm, 0x1b80, 0xe26b13c7);
	odm_write_4byte(dm, 0x1b80, 0xe29e13d5);
	odm_write_4byte(dm, 0x1b80, 0xe29e13d7);
	odm_write_4byte(dm, 0x1b80, 0xf6a913e5);
	odm_write_4byte(dm, 0x1b80, 0xf6a913e7);
	odm_write_4byte(dm, 0x1b80, 0x5c3213f5);
	odm_write_4byte(dm, 0x1b80, 0x5c3213f7);
	odm_write_4byte(dm, 0x1b80, 0x54f21405);
	odm_write_4byte(dm, 0x1b80, 0x54f21407);
	odm_write_4byte(dm, 0x1b80, 0x6e5c1415);
	odm_write_4byte(dm, 0x1b80, 0x6e5c1417);
	odm_write_4byte(dm, 0x1b80, 0xe26b1425);
	odm_write_4byte(dm, 0x1b80, 0xe26b1427);
	odm_write_4byte(dm, 0x1b80, 0xe29e1435);
	odm_write_4byte(dm, 0x1b80, 0xe29e1437);
	odm_write_4byte(dm, 0x1b80, 0x5c321445);
	odm_write_4byte(dm, 0x1b80, 0x5c321447);
	odm_write_4byte(dm, 0x1b80, 0x54f31455);
	odm_write_4byte(dm, 0x1b80, 0x54f31457);
	odm_write_4byte(dm, 0x1b80, 0x6e7c1465);
	odm_write_4byte(dm, 0x1b80, 0x6e7c1467);
	odm_write_4byte(dm, 0x1b80, 0xe26b1475);
	odm_write_4byte(dm, 0x1b80, 0xe26b1477);
	odm_write_4byte(dm, 0x1b80, 0xe29e1485);
	odm_write_4byte(dm, 0x1b80, 0xe29e1487);
	odm_write_4byte(dm, 0x1b80, 0xf7a91495);
	odm_write_4byte(dm, 0x1b80, 0xf7a91497);
	odm_write_4byte(dm, 0x1b80, 0x5c3214a5);
	odm_write_4byte(dm, 0x1b80, 0x5c3214a7);
	odm_write_4byte(dm, 0x1b80, 0x54f414b5);
	odm_write_4byte(dm, 0x1b80, 0x54f414b7);
	odm_write_4byte(dm, 0x1b80, 0x6e9c14c5);
	odm_write_4byte(dm, 0x1b80, 0x6e9c14c7);
	odm_write_4byte(dm, 0x1b80, 0xe26b14d5);
	odm_write_4byte(dm, 0x1b80, 0xe26b14d7);
	odm_write_4byte(dm, 0x1b80, 0xe29e14e5);
	odm_write_4byte(dm, 0x1b80, 0xe29e14e7);
	odm_write_4byte(dm, 0x1b80, 0x5c3214f5);
	odm_write_4byte(dm, 0x1b80, 0x5c3214f7);
	odm_write_4byte(dm, 0x1b80, 0x54f51505);
	odm_write_4byte(dm, 0x1b80, 0x54f51507);
	odm_write_4byte(dm, 0x1b80, 0x6ebc1515);
	odm_write_4byte(dm, 0x1b80, 0x6ebc1517);
	odm_write_4byte(dm, 0x1b80, 0xe26b1525);
	odm_write_4byte(dm, 0x1b80, 0xe26b1527);
	odm_write_4byte(dm, 0x1b80, 0xe29e1535);
	odm_write_4byte(dm, 0x1b80, 0xe29e1537);
	odm_write_4byte(dm, 0x1b80, 0x5c321545);
	odm_write_4byte(dm, 0x1b80, 0x5c321547);
	odm_write_4byte(dm, 0x1b80, 0x54f61555);
	odm_write_4byte(dm, 0x1b80, 0x54f61557);
	odm_write_4byte(dm, 0x1b80, 0x6edc1565);
	odm_write_4byte(dm, 0x1b80, 0x6edc1567);
	odm_write_4byte(dm, 0x1b80, 0xe26b1575);
	odm_write_4byte(dm, 0x1b80, 0xe26b1577);
	odm_write_4byte(dm, 0x1b80, 0xe29e1585);
	odm_write_4byte(dm, 0x1b80, 0xe29e1587);
	odm_write_4byte(dm, 0x1b80, 0x5c321595);
	odm_write_4byte(dm, 0x1b80, 0x5c321597);
	odm_write_4byte(dm, 0x1b80, 0x54f715a5);
	odm_write_4byte(dm, 0x1b80, 0x54f715a7);
	odm_write_4byte(dm, 0x1b80, 0x6ef015b5);
	odm_write_4byte(dm, 0x1b80, 0x6ef015b7);
	odm_write_4byte(dm, 0x1b80, 0xe26b15c5);
	odm_write_4byte(dm, 0x1b80, 0xe26b15c7);
	odm_write_4byte(dm, 0x1b80, 0xe29e15d5);
	odm_write_4byte(dm, 0x1b80, 0xe29e15d7);
	odm_write_4byte(dm, 0x1b80, 0x638315e5);
	odm_write_4byte(dm, 0x1b80, 0x638315e7);
	odm_write_4byte(dm, 0x1b80, 0x30d015f5);
	odm_write_4byte(dm, 0x1b80, 0x30d015f7);
	odm_write_4byte(dm, 0x1b80, 0x00011605);
	odm_write_4byte(dm, 0x1b80, 0x00011607);
	odm_write_4byte(dm, 0x1b80, 0x00041615);
	odm_write_4byte(dm, 0x1b80, 0x00041617);
	odm_write_4byte(dm, 0x1b80, 0x55011625);
	odm_write_4byte(dm, 0x1b80, 0x55011627);
	odm_write_4byte(dm, 0x1b80, 0x5c311635);
	odm_write_4byte(dm, 0x1b80, 0x5c311637);
	odm_write_4byte(dm, 0x1b80, 0x5f821645);
	odm_write_4byte(dm, 0x1b80, 0x5f821647);
	odm_write_4byte(dm, 0x1b80, 0x66051655);
	odm_write_4byte(dm, 0x1b80, 0x66051657);
	odm_write_4byte(dm, 0x1b80, 0x00061665);
	odm_write_4byte(dm, 0x1b80, 0x00061667);
	odm_write_4byte(dm, 0x1b80, 0x5d801675);
	odm_write_4byte(dm, 0x1b80, 0x5d801677);
	odm_write_4byte(dm, 0x1b80, 0x09001685);
	odm_write_4byte(dm, 0x1b80, 0x09001687);
	odm_write_4byte(dm, 0x1b80, 0x0a011695);
	odm_write_4byte(dm, 0x1b80, 0x0a011697);
	odm_write_4byte(dm, 0x1b80, 0x0b4016a5);
	odm_write_4byte(dm, 0x1b80, 0x0b4016a7);
	odm_write_4byte(dm, 0x1b80, 0x0d0016b5);
	odm_write_4byte(dm, 0x1b80, 0x0d0016b7);
	odm_write_4byte(dm, 0x1b80, 0x0f0116c5);
	odm_write_4byte(dm, 0x1b80, 0x0f0116c7);
	odm_write_4byte(dm, 0x1b80, 0x002a16d5);
	odm_write_4byte(dm, 0x1b80, 0x002a16d7);
	odm_write_4byte(dm, 0x1b80, 0x055a16e5);
	odm_write_4byte(dm, 0x1b80, 0x055a16e7);
	odm_write_4byte(dm, 0x1b80, 0x05db16f5);
	odm_write_4byte(dm, 0x1b80, 0x05db16f7);
	odm_write_4byte(dm, 0x1b80, 0xe2891705);
	odm_write_4byte(dm, 0x1b80, 0xe2891707);
	odm_write_4byte(dm, 0x1b80, 0xe2371715);
	odm_write_4byte(dm, 0x1b80, 0xe2371717);
	odm_write_4byte(dm, 0x1b80, 0x00061725);
	odm_write_4byte(dm, 0x1b80, 0x00061727);
	odm_write_4byte(dm, 0x1b80, 0x06da1735);
	odm_write_4byte(dm, 0x1b80, 0x06da1737);
	odm_write_4byte(dm, 0x1b80, 0x07db1745);
	odm_write_4byte(dm, 0x1b80, 0x07db1747);
	odm_write_4byte(dm, 0x1b80, 0xe2891755);
	odm_write_4byte(dm, 0x1b80, 0xe2891757);
	odm_write_4byte(dm, 0x1b80, 0xe2371765);
	odm_write_4byte(dm, 0x1b80, 0xe2371767);
	odm_write_4byte(dm, 0x1b80, 0xe2801775);
	odm_write_4byte(dm, 0x1b80, 0xe2801777);
	odm_write_4byte(dm, 0x1b80, 0x00021785);
	odm_write_4byte(dm, 0x1b80, 0x00021787);
	odm_write_4byte(dm, 0x1b80, 0xe2851795);
	odm_write_4byte(dm, 0x1b80, 0xe2851797);
	odm_write_4byte(dm, 0x1b80, 0x5d0017a5);
	odm_write_4byte(dm, 0x1b80, 0x5d0017a7);
	odm_write_4byte(dm, 0x1b80, 0x000417b5);
	odm_write_4byte(dm, 0x1b80, 0x000417b7);
	odm_write_4byte(dm, 0x1b80, 0x5fa217c5);
	odm_write_4byte(dm, 0x1b80, 0x5fa217c7);
	odm_write_4byte(dm, 0x1b80, 0x000117d5);
	odm_write_4byte(dm, 0x1b80, 0x000117d7);
	odm_write_4byte(dm, 0x1b80, 0xe1c417e5);
	odm_write_4byte(dm, 0x1b80, 0xe1c417e7);
	odm_write_4byte(dm, 0x1b80, 0x740817f5);
	odm_write_4byte(dm, 0x1b80, 0x740817f7);
	odm_write_4byte(dm, 0x1b80, 0xe2021805);
	odm_write_4byte(dm, 0x1b80, 0xe2021807);
	odm_write_4byte(dm, 0x1b80, 0xe1e41815);
	odm_write_4byte(dm, 0x1b80, 0xe1e41817);
	odm_write_4byte(dm, 0x1b80, 0xe2161825);
	odm_write_4byte(dm, 0x1b80, 0xe2161827);
	odm_write_4byte(dm, 0x1b80, 0xe2221835);
	odm_write_4byte(dm, 0x1b80, 0xe2221837);
	odm_write_4byte(dm, 0x1b80, 0x00011845);
	odm_write_4byte(dm, 0x1b80, 0x00011847);
	odm_write_4byte(dm, 0x1b80, 0xe1c41855);
	odm_write_4byte(dm, 0x1b80, 0xe1c41857);
	odm_write_4byte(dm, 0x1b80, 0x74081865);
	odm_write_4byte(dm, 0x1b80, 0x74081867);
	odm_write_4byte(dm, 0x1b80, 0xe20c1875);
	odm_write_4byte(dm, 0x1b80, 0xe20c1877);
	odm_write_4byte(dm, 0x1b80, 0xe1e41885);
	odm_write_4byte(dm, 0x1b80, 0xe1e41887);
	odm_write_4byte(dm, 0x1b80, 0xe21c1895);
	odm_write_4byte(dm, 0x1b80, 0xe21c1897);
	odm_write_4byte(dm, 0x1b80, 0xe22218a5);
	odm_write_4byte(dm, 0x1b80, 0xe22218a7);
	odm_write_4byte(dm, 0x1b80, 0x000118b5);
	odm_write_4byte(dm, 0x1b80, 0x000118b7);
	odm_write_4byte(dm, 0x1b80, 0xe1d418c5);
	odm_write_4byte(dm, 0x1b80, 0xe1d418c7);
	odm_write_4byte(dm, 0x1b80, 0x740018d5);
	odm_write_4byte(dm, 0x1b80, 0x740018d7);
	odm_write_4byte(dm, 0x1b80, 0xe20218e5);
	odm_write_4byte(dm, 0x1b80, 0xe20218e7);
	odm_write_4byte(dm, 0x1b80, 0xe1f318f5);
	odm_write_4byte(dm, 0x1b80, 0xe1f318f7);
	odm_write_4byte(dm, 0x1b80, 0xe2161905);
	odm_write_4byte(dm, 0x1b80, 0xe2161907);
	odm_write_4byte(dm, 0x1b80, 0xe2221915);
	odm_write_4byte(dm, 0x1b80, 0xe2221917);
	odm_write_4byte(dm, 0x1b80, 0x00011925);
	odm_write_4byte(dm, 0x1b80, 0x00011927);
	odm_write_4byte(dm, 0x1b80, 0xe1d41935);
	odm_write_4byte(dm, 0x1b80, 0xe1d41937);
	odm_write_4byte(dm, 0x1b80, 0x74001945);
	odm_write_4byte(dm, 0x1b80, 0x74001947);
	odm_write_4byte(dm, 0x1b80, 0xe20c1955);
	odm_write_4byte(dm, 0x1b80, 0xe20c1957);
	odm_write_4byte(dm, 0x1b80, 0xe1f31965);
	odm_write_4byte(dm, 0x1b80, 0xe1f31967);
	odm_write_4byte(dm, 0x1b80, 0xe21c1975);
	odm_write_4byte(dm, 0x1b80, 0xe21c1977);
	odm_write_4byte(dm, 0x1b80, 0xe2221985);
	odm_write_4byte(dm, 0x1b80, 0xe2221987);
	odm_write_4byte(dm, 0x1b80, 0x00011995);
	odm_write_4byte(dm, 0x1b80, 0x00011997);
	odm_write_4byte(dm, 0x1b80, 0x000419a5);
	odm_write_4byte(dm, 0x1b80, 0x000419a7);
	odm_write_4byte(dm, 0x1b80, 0x445b19b5);
	odm_write_4byte(dm, 0x1b80, 0x445b19b7);
	odm_write_4byte(dm, 0x1b80, 0x470019c5);
	odm_write_4byte(dm, 0x1b80, 0x470019c7);
	odm_write_4byte(dm, 0x1b80, 0x000619d5);
	odm_write_4byte(dm, 0x1b80, 0x000619d7);
	odm_write_4byte(dm, 0x1b80, 0x772819e5);
	odm_write_4byte(dm, 0x1b80, 0x772819e7);
	odm_write_4byte(dm, 0x1b80, 0x000419f5);
	odm_write_4byte(dm, 0x1b80, 0x000419f7);
	odm_write_4byte(dm, 0x1b80, 0x4b801a05);
	odm_write_4byte(dm, 0x1b80, 0x4b801a07);
	odm_write_4byte(dm, 0x1b80, 0x40081a15);
	odm_write_4byte(dm, 0x1b80, 0x40081a17);
	odm_write_4byte(dm, 0x1b80, 0x00011a25);
	odm_write_4byte(dm, 0x1b80, 0x00011a27);
	odm_write_4byte(dm, 0x1b80, 0x00051a35);
	odm_write_4byte(dm, 0x1b80, 0x00051a37);
	odm_write_4byte(dm, 0x1b80, 0x5c5b1a45);
	odm_write_4byte(dm, 0x1b80, 0x5c5b1a47);
	odm_write_4byte(dm, 0x1b80, 0x5f001a55);
	odm_write_4byte(dm, 0x1b80, 0x5f001a57);
	odm_write_4byte(dm, 0x1b80, 0x00061a65);
	odm_write_4byte(dm, 0x1b80, 0x00061a67);
	odm_write_4byte(dm, 0x1b80, 0x77291a75);
	odm_write_4byte(dm, 0x1b80, 0x77291a77);
	odm_write_4byte(dm, 0x1b80, 0x00041a85);
	odm_write_4byte(dm, 0x1b80, 0x00041a87);
	odm_write_4byte(dm, 0x1b80, 0x63801a95);
	odm_write_4byte(dm, 0x1b80, 0x63801a97);
	odm_write_4byte(dm, 0x1b80, 0x40081aa5);
	odm_write_4byte(dm, 0x1b80, 0x40081aa7);
	odm_write_4byte(dm, 0x1b80, 0x00011ab5);
	odm_write_4byte(dm, 0x1b80, 0x00011ab7);
	odm_write_4byte(dm, 0x1b80, 0xe1c41ac5);
	odm_write_4byte(dm, 0x1b80, 0xe1c41ac7);
	odm_write_4byte(dm, 0x1b80, 0x74081ad5);
	odm_write_4byte(dm, 0x1b80, 0x74081ad7);
	odm_write_4byte(dm, 0x1b80, 0xe2021ae5);
	odm_write_4byte(dm, 0x1b80, 0xe2021ae7);
	odm_write_4byte(dm, 0x1b80, 0x00041af5);
	odm_write_4byte(dm, 0x1b80, 0x00041af7);
	odm_write_4byte(dm, 0x1b80, 0x40081b05);
	odm_write_4byte(dm, 0x1b80, 0x40081b07);
	odm_write_4byte(dm, 0x1b80, 0x00011b15);
	odm_write_4byte(dm, 0x1b80, 0x00011b17);
	odm_write_4byte(dm, 0x1b80, 0xe1c41b25);
	odm_write_4byte(dm, 0x1b80, 0xe1c41b27);
	odm_write_4byte(dm, 0x1b80, 0x74081b35);
	odm_write_4byte(dm, 0x1b80, 0x74081b37);
	odm_write_4byte(dm, 0x1b80, 0xe20c1b45);
	odm_write_4byte(dm, 0x1b80, 0xe20c1b47);
	odm_write_4byte(dm, 0x1b80, 0x00041b55);
	odm_write_4byte(dm, 0x1b80, 0x00041b57);
	odm_write_4byte(dm, 0x1b80, 0x40081b65);
	odm_write_4byte(dm, 0x1b80, 0x40081b67);
	odm_write_4byte(dm, 0x1b80, 0x00011b75);
	odm_write_4byte(dm, 0x1b80, 0x00011b77);
	odm_write_4byte(dm, 0x1b80, 0xe1d41b85);
	odm_write_4byte(dm, 0x1b80, 0xe1d41b87);
	odm_write_4byte(dm, 0x1b80, 0x74001b95);
	odm_write_4byte(dm, 0x1b80, 0x74001b97);
	odm_write_4byte(dm, 0x1b80, 0xe2021ba5);
	odm_write_4byte(dm, 0x1b80, 0xe2021ba7);
	odm_write_4byte(dm, 0x1b80, 0x00041bb5);
	odm_write_4byte(dm, 0x1b80, 0x00041bb7);
	odm_write_4byte(dm, 0x1b80, 0x40081bc5);
	odm_write_4byte(dm, 0x1b80, 0x40081bc7);
	odm_write_4byte(dm, 0x1b80, 0x00011bd5);
	odm_write_4byte(dm, 0x1b80, 0x00011bd7);
	odm_write_4byte(dm, 0x1b80, 0xe1d41be5);
	odm_write_4byte(dm, 0x1b80, 0xe1d41be7);
	odm_write_4byte(dm, 0x1b80, 0x74001bf5);
	odm_write_4byte(dm, 0x1b80, 0x74001bf7);
	odm_write_4byte(dm, 0x1b80, 0xe20c1c05);
	odm_write_4byte(dm, 0x1b80, 0xe20c1c07);
	odm_write_4byte(dm, 0x1b80, 0x00041c15);
	odm_write_4byte(dm, 0x1b80, 0x00041c17);
	odm_write_4byte(dm, 0x1b80, 0x40081c25);
	odm_write_4byte(dm, 0x1b80, 0x40081c27);
	odm_write_4byte(dm, 0x1b80, 0x00011c35);
	odm_write_4byte(dm, 0x1b80, 0x00011c37);
	odm_write_4byte(dm, 0x1b80, 0x00071c45);
	odm_write_4byte(dm, 0x1b80, 0x00071c47);
	odm_write_4byte(dm, 0x1b80, 0x780c1c55);
	odm_write_4byte(dm, 0x1b80, 0x780c1c57);
	odm_write_4byte(dm, 0x1b80, 0x79191c65);
	odm_write_4byte(dm, 0x1b80, 0x79191c67);
	odm_write_4byte(dm, 0x1b80, 0x7a001c75);
	odm_write_4byte(dm, 0x1b80, 0x7a001c77);
	odm_write_4byte(dm, 0x1b80, 0x7b821c85);
	odm_write_4byte(dm, 0x1b80, 0x7b821c87);
	odm_write_4byte(dm, 0x1b80, 0x7b021c95);
	odm_write_4byte(dm, 0x1b80, 0x7b021c97);
	odm_write_4byte(dm, 0x1b80, 0x78141ca5);
	odm_write_4byte(dm, 0x1b80, 0x78141ca7);
	odm_write_4byte(dm, 0x1b80, 0x79ee1cb5);
	odm_write_4byte(dm, 0x1b80, 0x79ee1cb7);
	odm_write_4byte(dm, 0x1b80, 0x7a011cc5);
	odm_write_4byte(dm, 0x1b80, 0x7a011cc7);
	odm_write_4byte(dm, 0x1b80, 0x7b831cd5);
	odm_write_4byte(dm, 0x1b80, 0x7b831cd7);
	odm_write_4byte(dm, 0x1b80, 0x7b031ce5);
	odm_write_4byte(dm, 0x1b80, 0x7b031ce7);
	odm_write_4byte(dm, 0x1b80, 0x780f1cf5);
	odm_write_4byte(dm, 0x1b80, 0x780f1cf7);
	odm_write_4byte(dm, 0x1b80, 0x79b41d05);
	odm_write_4byte(dm, 0x1b80, 0x79b41d07);
	odm_write_4byte(dm, 0x1b80, 0x7a001d15);
	odm_write_4byte(dm, 0x1b80, 0x7a001d17);
	odm_write_4byte(dm, 0x1b80, 0x7b001d25);
	odm_write_4byte(dm, 0x1b80, 0x7b001d27);
	odm_write_4byte(dm, 0x1b80, 0x00011d35);
	odm_write_4byte(dm, 0x1b80, 0x00011d37);
	odm_write_4byte(dm, 0x1b80, 0x00071d45);
	odm_write_4byte(dm, 0x1b80, 0x00071d47);
	odm_write_4byte(dm, 0x1b80, 0x78101d55);
	odm_write_4byte(dm, 0x1b80, 0x78101d57);
	odm_write_4byte(dm, 0x1b80, 0x79131d65);
	odm_write_4byte(dm, 0x1b80, 0x79131d67);
	odm_write_4byte(dm, 0x1b80, 0x7a001d75);
	odm_write_4byte(dm, 0x1b80, 0x7a001d77);
	odm_write_4byte(dm, 0x1b80, 0x7b801d85);
	odm_write_4byte(dm, 0x1b80, 0x7b801d87);
	odm_write_4byte(dm, 0x1b80, 0x7b001d95);
	odm_write_4byte(dm, 0x1b80, 0x7b001d97);
	odm_write_4byte(dm, 0x1b80, 0x78db1da5);
	odm_write_4byte(dm, 0x1b80, 0x78db1da7);
	odm_write_4byte(dm, 0x1b80, 0x79001db5);
	odm_write_4byte(dm, 0x1b80, 0x79001db7);
	odm_write_4byte(dm, 0x1b80, 0x7a001dc5);
	odm_write_4byte(dm, 0x1b80, 0x7a001dc7);
	odm_write_4byte(dm, 0x1b80, 0x7b811dd5);
	odm_write_4byte(dm, 0x1b80, 0x7b811dd7);
	odm_write_4byte(dm, 0x1b80, 0x7b011de5);
	odm_write_4byte(dm, 0x1b80, 0x7b011de7);
	odm_write_4byte(dm, 0x1b80, 0x780f1df5);
	odm_write_4byte(dm, 0x1b80, 0x780f1df7);
	odm_write_4byte(dm, 0x1b80, 0x79b41e05);
	odm_write_4byte(dm, 0x1b80, 0x79b41e07);
	odm_write_4byte(dm, 0x1b80, 0x7a001e15);
	odm_write_4byte(dm, 0x1b80, 0x7a001e17);
	odm_write_4byte(dm, 0x1b80, 0x7b001e25);
	odm_write_4byte(dm, 0x1b80, 0x7b001e27);
	odm_write_4byte(dm, 0x1b80, 0x00011e35);
	odm_write_4byte(dm, 0x1b80, 0x00011e37);
	odm_write_4byte(dm, 0x1b80, 0x00071e45);
	odm_write_4byte(dm, 0x1b80, 0x00071e47);
	odm_write_4byte(dm, 0x1b80, 0x783e1e55);
	odm_write_4byte(dm, 0x1b80, 0x783e1e57);
	odm_write_4byte(dm, 0x1b80, 0x79f91e65);
	odm_write_4byte(dm, 0x1b80, 0x79f91e67);
	odm_write_4byte(dm, 0x1b80, 0x7a011e75);
	odm_write_4byte(dm, 0x1b80, 0x7a011e77);
	odm_write_4byte(dm, 0x1b80, 0x7b821e85);
	odm_write_4byte(dm, 0x1b80, 0x7b821e87);
	odm_write_4byte(dm, 0x1b80, 0x7b021e95);
	odm_write_4byte(dm, 0x1b80, 0x7b021e97);
	odm_write_4byte(dm, 0x1b80, 0x78a91ea5);
	odm_write_4byte(dm, 0x1b80, 0x78a91ea7);
	odm_write_4byte(dm, 0x1b80, 0x79ed1eb5);
	odm_write_4byte(dm, 0x1b80, 0x79ed1eb7);
	odm_write_4byte(dm, 0x1b80, 0x7b831ec5);
	odm_write_4byte(dm, 0x1b80, 0x7b831ec7);
	odm_write_4byte(dm, 0x1b80, 0x7b031ed5);
	odm_write_4byte(dm, 0x1b80, 0x7b031ed7);
	odm_write_4byte(dm, 0x1b80, 0x780f1ee5);
	odm_write_4byte(dm, 0x1b80, 0x780f1ee7);
	odm_write_4byte(dm, 0x1b80, 0x79b41ef5);
	odm_write_4byte(dm, 0x1b80, 0x79b41ef7);
	odm_write_4byte(dm, 0x1b80, 0x7a001f05);
	odm_write_4byte(dm, 0x1b80, 0x7a001f07);
	odm_write_4byte(dm, 0x1b80, 0x7b001f15);
	odm_write_4byte(dm, 0x1b80, 0x7b001f17);
	odm_write_4byte(dm, 0x1b80, 0x00011f25);
	odm_write_4byte(dm, 0x1b80, 0x00011f27);
	odm_write_4byte(dm, 0x1b80, 0x00071f35);
	odm_write_4byte(dm, 0x1b80, 0x00071f37);
	odm_write_4byte(dm, 0x1b80, 0x78ae1f45);
	odm_write_4byte(dm, 0x1b80, 0x78ae1f47);
	odm_write_4byte(dm, 0x1b80, 0x79fa1f55);
	odm_write_4byte(dm, 0x1b80, 0x79fa1f57);
	odm_write_4byte(dm, 0x1b80, 0x7a011f65);
	odm_write_4byte(dm, 0x1b80, 0x7a011f67);
	odm_write_4byte(dm, 0x1b80, 0x7b801f75);
	odm_write_4byte(dm, 0x1b80, 0x7b801f77);
	odm_write_4byte(dm, 0x1b80, 0x7b001f85);
	odm_write_4byte(dm, 0x1b80, 0x7b001f87);
	odm_write_4byte(dm, 0x1b80, 0x787a1f95);
	odm_write_4byte(dm, 0x1b80, 0x787a1f97);
	odm_write_4byte(dm, 0x1b80, 0x79f11fa5);
	odm_write_4byte(dm, 0x1b80, 0x79f11fa7);
	odm_write_4byte(dm, 0x1b80, 0x7b811fb5);
	odm_write_4byte(dm, 0x1b80, 0x7b811fb7);
	odm_write_4byte(dm, 0x1b80, 0x7b011fc5);
	odm_write_4byte(dm, 0x1b80, 0x7b011fc7);
	odm_write_4byte(dm, 0x1b80, 0x780f1fd5);
	odm_write_4byte(dm, 0x1b80, 0x780f1fd7);
	odm_write_4byte(dm, 0x1b80, 0x79b41fe5);
	odm_write_4byte(dm, 0x1b80, 0x79b41fe7);
	odm_write_4byte(dm, 0x1b80, 0x7a001ff5);
	odm_write_4byte(dm, 0x1b80, 0x7a001ff7);
	odm_write_4byte(dm, 0x1b80, 0x7b002005);
	odm_write_4byte(dm, 0x1b80, 0x7b002007);
	odm_write_4byte(dm, 0x1b80, 0x00012015);
	odm_write_4byte(dm, 0x1b80, 0x00012017);
	odm_write_4byte(dm, 0x1b80, 0x77102025);
	odm_write_4byte(dm, 0x1b80, 0x77102027);
	odm_write_4byte(dm, 0x1b80, 0x00062035);
	odm_write_4byte(dm, 0x1b80, 0x00062037);
	odm_write_4byte(dm, 0x1b80, 0x74002045);
	odm_write_4byte(dm, 0x1b80, 0x74002047);
	odm_write_4byte(dm, 0x1b80, 0x76002055);
	odm_write_4byte(dm, 0x1b80, 0x76002057);
	odm_write_4byte(dm, 0x1b80, 0x77002065);
	odm_write_4byte(dm, 0x1b80, 0x77002067);
	odm_write_4byte(dm, 0x1b80, 0x75102075);
	odm_write_4byte(dm, 0x1b80, 0x75102077);
	odm_write_4byte(dm, 0x1b80, 0x75002085);
	odm_write_4byte(dm, 0x1b80, 0x75002087);
	odm_write_4byte(dm, 0x1b80, 0xb3002095);
	odm_write_4byte(dm, 0x1b80, 0xb3002097);
	odm_write_4byte(dm, 0x1b80, 0x930020a5);
	odm_write_4byte(dm, 0x1b80, 0x930020a7);
	odm_write_4byte(dm, 0x1b80, 0x000120b5);
	odm_write_4byte(dm, 0x1b80, 0x000120b7);
	odm_write_4byte(dm, 0x1b80, 0x772020c5);
	odm_write_4byte(dm, 0x1b80, 0x772020c7);
	odm_write_4byte(dm, 0x1b80, 0x000620d5);
	odm_write_4byte(dm, 0x1b80, 0x000620d7);
	odm_write_4byte(dm, 0x1b80, 0x740020e5);
	odm_write_4byte(dm, 0x1b80, 0x740020e7);
	odm_write_4byte(dm, 0x1b80, 0x760020f5);
	odm_write_4byte(dm, 0x1b80, 0x760020f7);
	odm_write_4byte(dm, 0x1b80, 0x77012105);
	odm_write_4byte(dm, 0x1b80, 0x77012107);
	odm_write_4byte(dm, 0x1b80, 0x75102115);
	odm_write_4byte(dm, 0x1b80, 0x75102117);
	odm_write_4byte(dm, 0x1b80, 0x75002125);
	odm_write_4byte(dm, 0x1b80, 0x75002127);
	odm_write_4byte(dm, 0x1b80, 0xb3002135);
	odm_write_4byte(dm, 0x1b80, 0xb3002137);
	odm_write_4byte(dm, 0x1b80, 0x93002145);
	odm_write_4byte(dm, 0x1b80, 0x93002147);
	odm_write_4byte(dm, 0x1b80, 0x00012155);
	odm_write_4byte(dm, 0x1b80, 0x00012157);
	odm_write_4byte(dm, 0x1b80, 0x00042165);
	odm_write_4byte(dm, 0x1b80, 0x00042167);
	odm_write_4byte(dm, 0x1b80, 0x44802175);
	odm_write_4byte(dm, 0x1b80, 0x44802177);
	odm_write_4byte(dm, 0x1b80, 0x47302185);
	odm_write_4byte(dm, 0x1b80, 0x47302187);
	odm_write_4byte(dm, 0x1b80, 0x00062195);
	odm_write_4byte(dm, 0x1b80, 0x00062197);
	odm_write_4byte(dm, 0x1b80, 0x776c21a5);
	odm_write_4byte(dm, 0x1b80, 0x776c21a7);
	odm_write_4byte(dm, 0x1b80, 0x000121b5);
	odm_write_4byte(dm, 0x1b80, 0x000121b7);
	odm_write_4byte(dm, 0x1b80, 0x000521c5);
	odm_write_4byte(dm, 0x1b80, 0x000521c7);
	odm_write_4byte(dm, 0x1b80, 0x5c8021d5);
	odm_write_4byte(dm, 0x1b80, 0x5c8021d7);
	odm_write_4byte(dm, 0x1b80, 0x5f3021e5);
	odm_write_4byte(dm, 0x1b80, 0x5f3021e7);
	odm_write_4byte(dm, 0x1b80, 0x000621f5);
	odm_write_4byte(dm, 0x1b80, 0x000621f7);
	odm_write_4byte(dm, 0x1b80, 0x776d2205);
	odm_write_4byte(dm, 0x1b80, 0x776d2207);
	odm_write_4byte(dm, 0x1b80, 0x00012215);
	odm_write_4byte(dm, 0x1b80, 0x00012217);
	odm_write_4byte(dm, 0x1b80, 0xb9002225);
	odm_write_4byte(dm, 0x1b80, 0xb9002227);
	odm_write_4byte(dm, 0x1b80, 0x99002235);
	odm_write_4byte(dm, 0x1b80, 0x99002237);
	odm_write_4byte(dm, 0x1b80, 0x77202245);
	odm_write_4byte(dm, 0x1b80, 0x77202247);
	odm_write_4byte(dm, 0x1b80, 0x00042255);
	odm_write_4byte(dm, 0x1b80, 0x00042257);
	odm_write_4byte(dm, 0x1b80, 0x40082265);
	odm_write_4byte(dm, 0x1b80, 0x40082267);
	odm_write_4byte(dm, 0x1b80, 0x98032275);
	odm_write_4byte(dm, 0x1b80, 0x98032277);
	odm_write_4byte(dm, 0x1b80, 0x4a022285);
	odm_write_4byte(dm, 0x1b80, 0x4a022287);
	odm_write_4byte(dm, 0x1b80, 0x30192295);
	odm_write_4byte(dm, 0x1b80, 0x30192297);
	odm_write_4byte(dm, 0x1b80, 0x000122a5);
	odm_write_4byte(dm, 0x1b80, 0x000122a7);
	odm_write_4byte(dm, 0x1b80, 0x7b4822b5);
	odm_write_4byte(dm, 0x1b80, 0x7b4822b7);
	odm_write_4byte(dm, 0x1b80, 0x7a9022c5);
	odm_write_4byte(dm, 0x1b80, 0x7a9022c7);
	odm_write_4byte(dm, 0x1b80, 0x790022d5);
	odm_write_4byte(dm, 0x1b80, 0x790022d7);
	odm_write_4byte(dm, 0x1b80, 0x550322e5);
	odm_write_4byte(dm, 0x1b80, 0x550322e7);
	odm_write_4byte(dm, 0x1b80, 0x323722f5);
	odm_write_4byte(dm, 0x1b80, 0x323722f7);
	odm_write_4byte(dm, 0x1b80, 0x7b382305);
	odm_write_4byte(dm, 0x1b80, 0x7b382307);
	odm_write_4byte(dm, 0x1b80, 0x7a802315);
	odm_write_4byte(dm, 0x1b80, 0x7a802317);
	odm_write_4byte(dm, 0x1b80, 0x550b2325);
	odm_write_4byte(dm, 0x1b80, 0x550b2327);
	odm_write_4byte(dm, 0x1b80, 0x32372335);
	odm_write_4byte(dm, 0x1b80, 0x32372337);
	odm_write_4byte(dm, 0x1b80, 0x7b402345);
	odm_write_4byte(dm, 0x1b80, 0x7b402347);
	odm_write_4byte(dm, 0x1b80, 0x7a002355);
	odm_write_4byte(dm, 0x1b80, 0x7a002357);
	odm_write_4byte(dm, 0x1b80, 0x55132365);
	odm_write_4byte(dm, 0x1b80, 0x55132367);
	odm_write_4byte(dm, 0x1b80, 0x74012375);
	odm_write_4byte(dm, 0x1b80, 0x74012377);
	odm_write_4byte(dm, 0x1b80, 0x74002385);
	odm_write_4byte(dm, 0x1b80, 0x74002387);
	odm_write_4byte(dm, 0x1b80, 0x8e002395);
	odm_write_4byte(dm, 0x1b80, 0x8e002397);
	odm_write_4byte(dm, 0x1b80, 0x000123a5);
	odm_write_4byte(dm, 0x1b80, 0x000123a7);
	odm_write_4byte(dm, 0x1b80, 0x570223b5);
	odm_write_4byte(dm, 0x1b80, 0x570223b7);
	odm_write_4byte(dm, 0x1b80, 0x570023c5);
	odm_write_4byte(dm, 0x1b80, 0x570023c7);
	odm_write_4byte(dm, 0x1b80, 0x970023d5);
	odm_write_4byte(dm, 0x1b80, 0x970023d7);
	odm_write_4byte(dm, 0x1b80, 0x000123e5);
	odm_write_4byte(dm, 0x1b80, 0x000123e7);
	odm_write_4byte(dm, 0x1b80, 0x4f7823f5);
	odm_write_4byte(dm, 0x1b80, 0x4f7823f7);
	odm_write_4byte(dm, 0x1b80, 0x53882405);
	odm_write_4byte(dm, 0x1b80, 0x53882407);
	odm_write_4byte(dm, 0x1b80, 0xe24b2415);
	odm_write_4byte(dm, 0x1b80, 0xe24b2417);
	odm_write_4byte(dm, 0x1b80, 0x54802425);
	odm_write_4byte(dm, 0x1b80, 0x54802427);
	odm_write_4byte(dm, 0x1b80, 0x54002435);
	odm_write_4byte(dm, 0x1b80, 0x54002437);
	odm_write_4byte(dm, 0x1b80, 0x54812445);
	odm_write_4byte(dm, 0x1b80, 0x54812447);
	odm_write_4byte(dm, 0x1b80, 0x54002455);
	odm_write_4byte(dm, 0x1b80, 0x54002457);
	odm_write_4byte(dm, 0x1b80, 0x54822465);
	odm_write_4byte(dm, 0x1b80, 0x54822467);
	odm_write_4byte(dm, 0x1b80, 0x54002475);
	odm_write_4byte(dm, 0x1b80, 0x54002477);
	odm_write_4byte(dm, 0x1b80, 0xe2562485);
	odm_write_4byte(dm, 0x1b80, 0xe2562487);
	odm_write_4byte(dm, 0x1b80, 0xbf1d2495);
	odm_write_4byte(dm, 0x1b80, 0xbf1d2497);
	odm_write_4byte(dm, 0x1b80, 0x301924a5);
	odm_write_4byte(dm, 0x1b80, 0x301924a7);
	odm_write_4byte(dm, 0x1b80, 0xe22b24b5);
	odm_write_4byte(dm, 0x1b80, 0xe22b24b7);
	odm_write_4byte(dm, 0x1b80, 0xe23024c5);
	odm_write_4byte(dm, 0x1b80, 0xe23024c7);
	odm_write_4byte(dm, 0x1b80, 0xe23424d5);
	odm_write_4byte(dm, 0x1b80, 0xe23424d7);
	odm_write_4byte(dm, 0x1b80, 0xe23b24e5);
	odm_write_4byte(dm, 0x1b80, 0xe23b24e7);
	odm_write_4byte(dm, 0x1b80, 0xe29524f5);
	odm_write_4byte(dm, 0x1b80, 0xe29524f7);
	odm_write_4byte(dm, 0x1b80, 0x55132505);
	odm_write_4byte(dm, 0x1b80, 0x55132507);
	odm_write_4byte(dm, 0x1b80, 0xe2372515);
	odm_write_4byte(dm, 0x1b80, 0xe2372517);
	odm_write_4byte(dm, 0x1b80, 0x55152525);
	odm_write_4byte(dm, 0x1b80, 0x55152527);
	odm_write_4byte(dm, 0x1b80, 0xe23b2535);
	odm_write_4byte(dm, 0x1b80, 0xe23b2537);
	odm_write_4byte(dm, 0x1b80, 0xe2952545);
	odm_write_4byte(dm, 0x1b80, 0xe2952547);
	odm_write_4byte(dm, 0x1b80, 0x00012555);
	odm_write_4byte(dm, 0x1b80, 0x00012557);
	odm_write_4byte(dm, 0x1b80, 0x54bf2565);
	odm_write_4byte(dm, 0x1b80, 0x54bf2567);
	odm_write_4byte(dm, 0x1b80, 0x54c02575);
	odm_write_4byte(dm, 0x1b80, 0x54c02577);
	odm_write_4byte(dm, 0x1b80, 0x54a32585);
	odm_write_4byte(dm, 0x1b80, 0x54a32587);
	odm_write_4byte(dm, 0x1b80, 0x54c12595);
	odm_write_4byte(dm, 0x1b80, 0x54c12597);
	odm_write_4byte(dm, 0x1b80, 0x54a425a5);
	odm_write_4byte(dm, 0x1b80, 0x54a425a7);
	odm_write_4byte(dm, 0x1b80, 0x4c1825b5);
	odm_write_4byte(dm, 0x1b80, 0x4c1825b7);
	odm_write_4byte(dm, 0x1b80, 0xbf0725c5);
	odm_write_4byte(dm, 0x1b80, 0xbf0725c7);
	odm_write_4byte(dm, 0x1b80, 0x54c225d5);
	odm_write_4byte(dm, 0x1b80, 0x54c225d7);
	odm_write_4byte(dm, 0x1b80, 0x54a425e5);
	odm_write_4byte(dm, 0x1b80, 0x54a425e7);
	odm_write_4byte(dm, 0x1b80, 0xbf0425f5);
	odm_write_4byte(dm, 0x1b80, 0xbf0425f7);
	odm_write_4byte(dm, 0x1b80, 0x54c12605);
	odm_write_4byte(dm, 0x1b80, 0x54c12607);
	odm_write_4byte(dm, 0x1b80, 0x54a32615);
	odm_write_4byte(dm, 0x1b80, 0x54a32617);
	odm_write_4byte(dm, 0x1b80, 0xbf012625);
	odm_write_4byte(dm, 0x1b80, 0xbf012627);
	odm_write_4byte(dm, 0x1b80, 0xe2a32635);
	odm_write_4byte(dm, 0x1b80, 0xe2a32637);
	odm_write_4byte(dm, 0x1b80, 0x54df2645);
	odm_write_4byte(dm, 0x1b80, 0x54df2647);
	odm_write_4byte(dm, 0x1b80, 0x00012655);
	odm_write_4byte(dm, 0x1b80, 0x00012657);
	odm_write_4byte(dm, 0x1b80, 0x54bf2665);
	odm_write_4byte(dm, 0x1b80, 0x54bf2667);
	odm_write_4byte(dm, 0x1b80, 0x54e52675);
	odm_write_4byte(dm, 0x1b80, 0x54e52677);
	odm_write_4byte(dm, 0x1b80, 0x050a2685);
	odm_write_4byte(dm, 0x1b80, 0x050a2687);
	odm_write_4byte(dm, 0x1b80, 0x54df2695);
	odm_write_4byte(dm, 0x1b80, 0x54df2697);
	odm_write_4byte(dm, 0x1b80, 0x000126a5);
	odm_write_4byte(dm, 0x1b80, 0x000126a7);
	odm_write_4byte(dm, 0x1b80, 0x7f4026b5);
	odm_write_4byte(dm, 0x1b80, 0x7f4026b7);
	odm_write_4byte(dm, 0x1b80, 0x7e0026c5);
	odm_write_4byte(dm, 0x1b80, 0x7e0026c7);
	odm_write_4byte(dm, 0x1b80, 0x7d0026d5);
	odm_write_4byte(dm, 0x1b80, 0x7d0026d7);
	odm_write_4byte(dm, 0x1b80, 0x550126e5);
	odm_write_4byte(dm, 0x1b80, 0x550126e7);
	odm_write_4byte(dm, 0x1b80, 0x5c3126f5);
	odm_write_4byte(dm, 0x1b80, 0x5c3126f7);
	odm_write_4byte(dm, 0x1b80, 0xe2372705);
	odm_write_4byte(dm, 0x1b80, 0xe2372707);
	odm_write_4byte(dm, 0x1b80, 0xe23b2715);
	odm_write_4byte(dm, 0x1b80, 0xe23b2717);
	odm_write_4byte(dm, 0x1b80, 0x54802725);
	odm_write_4byte(dm, 0x1b80, 0x54802727);
	odm_write_4byte(dm, 0x1b80, 0x54002735);
	odm_write_4byte(dm, 0x1b80, 0x54002737);
	odm_write_4byte(dm, 0x1b80, 0x54812745);
	odm_write_4byte(dm, 0x1b80, 0x54812747);
	odm_write_4byte(dm, 0x1b80, 0x54002755);
	odm_write_4byte(dm, 0x1b80, 0x54002757);
	odm_write_4byte(dm, 0x1b80, 0x54822765);
	odm_write_4byte(dm, 0x1b80, 0x54822767);
	odm_write_4byte(dm, 0x1b80, 0x54002775);
	odm_write_4byte(dm, 0x1b80, 0x54002777);
	odm_write_4byte(dm, 0x1b80, 0xe2562785);
	odm_write_4byte(dm, 0x1b80, 0xe2562787);
	odm_write_4byte(dm, 0x1b80, 0xbfed2795);
	odm_write_4byte(dm, 0x1b80, 0xbfed2797);
	odm_write_4byte(dm, 0x1b80, 0x301927a5);
	odm_write_4byte(dm, 0x1b80, 0x301927a7);
	odm_write_4byte(dm, 0x1b80, 0x740227b5);
	odm_write_4byte(dm, 0x1b80, 0x740227b7);
	odm_write_4byte(dm, 0x1b80, 0x003f27c5);
	odm_write_4byte(dm, 0x1b80, 0x003f27c7);
	odm_write_4byte(dm, 0x1b80, 0x740027d5);
	odm_write_4byte(dm, 0x1b80, 0x740027d7);
	odm_write_4byte(dm, 0x1b80, 0x000227e5);
	odm_write_4byte(dm, 0x1b80, 0x000227e7);
	odm_write_4byte(dm, 0x1b80, 0x000127f5);
	odm_write_4byte(dm, 0x1b80, 0x000127f7);
	odm_write_4byte(dm, 0x1b80, 0x00062805);
	odm_write_4byte(dm, 0x1b80, 0x00062807);
	odm_write_4byte(dm, 0x1b80, 0x5a802815);
	odm_write_4byte(dm, 0x1b80, 0x5a802817);
	odm_write_4byte(dm, 0x1b80, 0x5a002825);
	odm_write_4byte(dm, 0x1b80, 0x5a002827);
	odm_write_4byte(dm, 0x1b80, 0x92002835);
	odm_write_4byte(dm, 0x1b80, 0x92002837);
	odm_write_4byte(dm, 0x1b80, 0x00012845);
	odm_write_4byte(dm, 0x1b80, 0x00012847);
	odm_write_4byte(dm, 0x1b80, 0x5b8f2855);
	odm_write_4byte(dm, 0x1b80, 0x5b8f2857);
	odm_write_4byte(dm, 0x1b80, 0x5b0f2865);
	odm_write_4byte(dm, 0x1b80, 0x5b0f2867);
	odm_write_4byte(dm, 0x1b80, 0x91002875);
	odm_write_4byte(dm, 0x1b80, 0x91002877);
	odm_write_4byte(dm, 0x1b80, 0x00012885);
	odm_write_4byte(dm, 0x1b80, 0x00012887);
	odm_write_4byte(dm, 0x1b80, 0x00062895);
	odm_write_4byte(dm, 0x1b80, 0x00062897);
	odm_write_4byte(dm, 0x1b80, 0x5d8028a5);
	odm_write_4byte(dm, 0x1b80, 0x5d8028a7);
	odm_write_4byte(dm, 0x1b80, 0x5e5628b5);
	odm_write_4byte(dm, 0x1b80, 0x5e5628b7);
	odm_write_4byte(dm, 0x1b80, 0x000428c5);
	odm_write_4byte(dm, 0x1b80, 0x000428c7);
	odm_write_4byte(dm, 0x1b80, 0x4d0828d5);
	odm_write_4byte(dm, 0x1b80, 0x4d0828d7);
	odm_write_4byte(dm, 0x1b80, 0x571028e5);
	odm_write_4byte(dm, 0x1b80, 0x571028e7);
	odm_write_4byte(dm, 0x1b80, 0x570028f5);
	odm_write_4byte(dm, 0x1b80, 0x570028f7);
	odm_write_4byte(dm, 0x1b80, 0x4d002905);
	odm_write_4byte(dm, 0x1b80, 0x4d002907);
	odm_write_4byte(dm, 0x1b80, 0x00062915);
	odm_write_4byte(dm, 0x1b80, 0x00062917);
	odm_write_4byte(dm, 0x1b80, 0x5d002925);
	odm_write_4byte(dm, 0x1b80, 0x5d002927);
	odm_write_4byte(dm, 0x1b80, 0x00042935);
	odm_write_4byte(dm, 0x1b80, 0x00042937);
	odm_write_4byte(dm, 0x1b80, 0x00012945);
	odm_write_4byte(dm, 0x1b80, 0x00012947);
	odm_write_4byte(dm, 0x1b80, 0x549f2955);
	odm_write_4byte(dm, 0x1b80, 0x549f2957);
	odm_write_4byte(dm, 0x1b80, 0x54ff2965);
	odm_write_4byte(dm, 0x1b80, 0x54ff2967);
	odm_write_4byte(dm, 0x1b80, 0x54002975);
	odm_write_4byte(dm, 0x1b80, 0x54002977);
	odm_write_4byte(dm, 0x1b80, 0x00012985);
	odm_write_4byte(dm, 0x1b80, 0x00012987);
	odm_write_4byte(dm, 0x1b80, 0x5c312995);
	odm_write_4byte(dm, 0x1b80, 0x5c312997);
	odm_write_4byte(dm, 0x1b80, 0x071429a5);
	odm_write_4byte(dm, 0x1b80, 0x071429a7);
	odm_write_4byte(dm, 0x1b80, 0x540029b5);
	odm_write_4byte(dm, 0x1b80, 0x540029b7);
	odm_write_4byte(dm, 0x1b80, 0x5c3229c5);
	odm_write_4byte(dm, 0x1b80, 0x5c3229c7);
	odm_write_4byte(dm, 0x1b80, 0x000129d5);
	odm_write_4byte(dm, 0x1b80, 0x000129d7);
	odm_write_4byte(dm, 0x1b80, 0x5c3229e5);
	odm_write_4byte(dm, 0x1b80, 0x5c3229e7);
	odm_write_4byte(dm, 0x1b80, 0x071429f5);
	odm_write_4byte(dm, 0x1b80, 0x071429f7);
	odm_write_4byte(dm, 0x1b80, 0x54002a05);
	odm_write_4byte(dm, 0x1b80, 0x54002a07);
	odm_write_4byte(dm, 0x1b80, 0x5c312a15);
	odm_write_4byte(dm, 0x1b80, 0x5c312a17);
	odm_write_4byte(dm, 0x1b80, 0x00012a25);
	odm_write_4byte(dm, 0x1b80, 0x00012a27);
	odm_write_4byte(dm, 0x1b80, 0x4c982a35);
	odm_write_4byte(dm, 0x1b80, 0x4c982a37);
	odm_write_4byte(dm, 0x1b80, 0x4c182a45);
	odm_write_4byte(dm, 0x1b80, 0x4c182a47);
	odm_write_4byte(dm, 0x1b80, 0x00012a55);
	odm_write_4byte(dm, 0x1b80, 0x00012a57);
	odm_write_4byte(dm, 0x1b80, 0x5c322a65);
	odm_write_4byte(dm, 0x1b80, 0x5c322a67);
	odm_write_4byte(dm, 0x1b80, 0x62042a75);
	odm_write_4byte(dm, 0x1b80, 0x62042a77);
	odm_write_4byte(dm, 0x1b80, 0x63032a85);
	odm_write_4byte(dm, 0x1b80, 0x63032a87);
	odm_write_4byte(dm, 0x1b80, 0x66072a95);
	odm_write_4byte(dm, 0x1b80, 0x66072a97);
	odm_write_4byte(dm, 0x1b80, 0x7b402aa5);
	odm_write_4byte(dm, 0x1b80, 0x7b402aa7);
	odm_write_4byte(dm, 0x1b80, 0x7a002ab5);
	odm_write_4byte(dm, 0x1b80, 0x7a002ab7);
	odm_write_4byte(dm, 0x1b80, 0x79002ac5);
	odm_write_4byte(dm, 0x1b80, 0x79002ac7);
	odm_write_4byte(dm, 0x1b80, 0x7f402ad5);
	odm_write_4byte(dm, 0x1b80, 0x7f402ad7);
	odm_write_4byte(dm, 0x1b80, 0x7e002ae5);
	odm_write_4byte(dm, 0x1b80, 0x7e002ae7);
	odm_write_4byte(dm, 0x1b80, 0x7d002af5);
	odm_write_4byte(dm, 0x1b80, 0x7d002af7);
	odm_write_4byte(dm, 0x1b80, 0x09012b05);
	odm_write_4byte(dm, 0x1b80, 0x09012b07);
	odm_write_4byte(dm, 0x1b80, 0x0c012b15);
	odm_write_4byte(dm, 0x1b80, 0x0c012b17);
	odm_write_4byte(dm, 0x1b80, 0x0ba62b25);
	odm_write_4byte(dm, 0x1b80, 0x0ba62b27);
	odm_write_4byte(dm, 0x1b80, 0x00012b35);
	odm_write_4byte(dm, 0x1b80, 0x00012b37);
	odm_write_4byte(dm, 0x1b80, 0x00000006);
	odm_write_4byte(dm, 0x1b80, 0x00000002);

}


void _iqk_cal_path_off_8822c(struct dm_struct *dm)
{
	u8 path;

	odm_set_bb_reg(dm, 0x1bb8, BIT(20), 0x0);
	for(path = 0; path < SS_8822C; path++) {
		//odm_set_rf_reg(dm, (enum rf_path)path, 0x0, 0xfffff, 0x10000);		
		//odm_set_bb_reg(dm, R_0x1b00, MASKDWORD, 0x8 | path << 1);
		odm_set_bb_reg(dm, R_0x1b00, BIT(2)| BIT(1), path);
		odm_set_bb_reg(dm, 0x1bcc, 0x3f, 0x3f);
	}
}

void _iqk_con_tx_8822c(
	struct dm_struct *dm,
	boolean is_contx)
{
	if (is_contx) {
		odm_set_bb_reg(dm, 0x180c, 0x3, 0x0);
		odm_set_bb_reg(dm, 0x410c, 0x3, 0x0);
		//odm_set_bb_reg(dm, 0x520c, 0x3, 0x0);
		//odm_set_bb_reg(dm, 0x530c, 0x3, 0x0);
		odm_set_bb_reg(dm, 0x1d08, BIT(0), 0x1);
		odm_set_bb_reg(dm, 0x1ca4, BIT(0), 0x1);
		odm_set_bb_reg(dm, 0x1e70, BIT(1), 0x1);
		odm_set_bb_reg(dm, 0x1e70, BIT(1), 0x0);
		odm_set_bb_reg(dm, 0x1e70, BIT(2), 0x0);
		odm_set_bb_reg(dm, 0x1e70, BIT(2), 0x1);
	} else {
		odm_set_bb_reg(dm, 0x1d08, BIT(0), 0x0);
		odm_set_bb_reg(dm, 0x1ca4, BIT(0), 0x0);
	}
}

void _iqk_rf_set_check_8822c(
	struct dm_struct *dm,
	u8 path,
	u16 add,
	u32 data)
{
	u32 i;

	odm_set_rf_reg(dm, (enum rf_path)path, add, RFREGOFFSETMASK, data);

	for (i = 0; i < 100; i++) {
		if (odm_get_rf_reg(dm, (enum rf_path)path, add, RFREGOFFSETMASK) == data)
			break;
		else {
			ODM_delay_us(10);
			odm_set_rf_reg(dm, (enum rf_path)path, add, RFREGOFFSETMASK, data);
		}
	}
}

void _iqk_rf0xb0_workaround_8822c(
	struct dm_struct *dm)
{
	/*add 0xb8 control for the bad phase noise after switching channel*/
	odm_set_rf_reg(dm, (enum rf_path)0x0, RF_0xb8, RFREGOFFSETMASK, 0x00a00);
	odm_set_rf_reg(dm, (enum rf_path)0x0, RF_0xb8, RFREGOFFSETMASK, 0x80a00);
}

void _iqk_fill_iqk_report_8822c(
	void *dm_void,
	u8 ch)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct dm_iqk_info *iqk = &dm->IQK_info;
	u32 tmp1 = 0x0, tmp2 = 0x0, tmp3 = 0x0, data;
	u8 i;

	for (i = 0; i < SS_8822C; i++) {
		tmp1 += ((iqk->iqk_fail_report[ch][i][TX_IQK] & 1) << i);
		tmp2 += ((iqk->iqk_fail_report[ch][i][RX_IQK] & 1) << (i + 4));
		tmp3 += ((iqk->rxiqk_fail_code[ch][i] & 0x3) << (i * 2 + 8));
		data = iqk->rxiqk_agc[ch][i];

		odm_write_4byte(dm, R_0x1b00, IQK_CMD_8822C | i << 1 );
		odm_set_bb_reg(dm, R_0x1bf0, 0x0000ffff, tmp1 | tmp2 | tmp3);
		odm_write_4byte(dm, R_0x1be8, data);
		RF_DBG(dm, DBG_RF_IQK, "[IQK]S%d 0x1bf0 =0x%x,0x1be8=0x%x\n", i,
		       odm_read_4byte(dm, 0x1bf0), odm_read_4byte(dm, 0x1be8));
	}

	
}

void _iqk_fail_count_8822c(
	void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct dm_iqk_info *iqk_info = &dm->IQK_info;
	u8 i;

	dm->n_iqk_cnt++;
	if (odm_get_rf_reg(dm, RF_PATH_A, RF_0x1bf0, BIT(16)) == 1)
		iqk_info->is_reload = true;
	else
		iqk_info->is_reload = false;

	if (!iqk_info->is_reload) {
		for (i = 0; i < 8; i++) {
			if (odm_get_bb_reg(dm, R_0x1bf0, BIT(i)) == 1)
				dm->n_iqk_fail_cnt++;
		}
	}
	RF_DBG(dm, DBG_RF_IQK, "[IQK]All/Fail = %d %d\n", dm->n_iqk_cnt, dm->n_iqk_fail_cnt);
}

void _iqk_iqk_fail_report_8822c(
	struct dm_struct *dm)
{
	u32 tmp1bf0 = 0x0;
	u8 i;

	tmp1bf0 = odm_read_4byte(dm, 0x1bf0);

	for (i = 0; i < 4; i++) {
		if (tmp1bf0 & (0x1 << i))
#if !(DM_ODM_SUPPORT_TYPE & ODM_AP)
			RF_DBG(dm, DBG_RF_IQK, "[IQK] please check S%d TXIQK\n", i);
#else
			panic_printk("[IQK] please check S%d TXIQK\n", i);
#endif
		if (tmp1bf0 & (0x1 << (i + 12)))
#if !(DM_ODM_SUPPORT_TYPE & ODM_AP)
			RF_DBG(dm, DBG_RF_IQK, "[IQK] please check S%d RXIQK\n", i);
#else
			panic_printk("[IQK] please check S%d RXIQK\n", i);
#endif
	}
}

void _iqk_backup_mac_bb_8822c(
	struct dm_struct *dm,
	u32 *MAC_backup,
	u32 *BB_backup,
	u32 *backup_mac_reg,
	u32 *backup_bb_reg)
{
	u32 i;
	for (i = 0; i < MAC_REG_NUM_8822C; i++){
		MAC_backup[i] = odm_read_4byte(dm, backup_mac_reg[i]);
		//RF_DBG(dm, DBG_RF_IQK, "[IQK]Backup mac addr = %x, value =% x\n", backup_mac_reg[i], MAC_backup[i]);
	}
	for (i = 0; i < BB_REG_NUM_8822C; i++){
		BB_backup[i] = odm_read_4byte(dm, backup_bb_reg[i]);		
		//RF_DBG(dm, DBG_RF_IQK, "[IQK]Backup bbaddr = %x, value =% x\n", backup_bb_reg[i], BB_backup[i]);
	}
	RF_DBG(dm, DBG_RF_IQK, "[IQK]BackupMacBB Success!!!!\n"); 
}

void _iqk_backup_rf_8822c(
	struct dm_struct *dm,
	u32 RF_backup[][SS_8822C],
	u32 *backup_rf_reg)
{
	u32 i;

	for (i = 0; i < RF_REG_NUM_8822C; i++) {
		RF_backup[i][RF_PATH_A] = odm_get_rf_reg(dm, RF_PATH_A, backup_rf_reg[i], RFREGOFFSETMASK);
		RF_backup[i][RF_PATH_B] = odm_get_rf_reg(dm, RF_PATH_B, backup_rf_reg[i], RFREGOFFSETMASK);
		//RF_backup[i][RF_PATH_C] = odm_get_rf_reg(dm, RF_PATH_C, backup_rf_reg[i], RFREGOFFSETMASK);
		//RF_backup[i][RF_PATH_D] = odm_get_rf_reg(dm, RF_PATH_D, backup_rf_reg[i], RFREGOFFSETMASK);
	}
	RF_DBG(dm, DBG_RF_IQK, "[IQK]BackupRF Success!!!!\n"); 
}

void _iqk_agc_bnd_int_8822c(
	struct dm_struct *dm)
{
	return;
#if 0
	/*initialize RX AGC bnd, it must do after bbreset*/
	odm_write_4byte(dm, 0x1b00, 0x8);
	odm_write_4byte(dm, 0x1b00, 0x00A70008);
	odm_write_4byte(dm, 0x1b00, 0x00150008);
	odm_write_4byte(dm, 0x1b00, 0x8);
	RF_DBG(dm, DBG_RF_IQK, "[IQK]init. rx agc bnd\n");
#endif
}

void _iqk_bb_reset_8822c(
	struct dm_struct *dm)
{
	boolean cca_ing = false;
	u32 count = 0;

	odm_set_rf_reg(dm, RF_PATH_A, RF_0x0, RFREGOFFSETMASK, 0x10000);
	odm_set_rf_reg(dm, RF_PATH_B, RF_0x0, RFREGOFFSETMASK, 0x10000);
	/*reset BB report*/
	odm_set_bb_reg(dm, R_0x8f8, 0x0ff00000, 0x0);

	while (1) {
		odm_write_4byte(dm, 0x8fc, 0x0);
		odm_set_bb_reg(dm, R_0x198c, 0x7, 0x7);
		cca_ing = (boolean)odm_get_bb_reg(dm, R_0xfa0, BIT(3));

		if (count > 30)
			cca_ing = false;

		if (cca_ing) {
			ODM_delay_us(10);
			count++;
		} else {
			odm_write_1byte(dm, 0x808, 0x0); /*RX ant off*/
			odm_set_bb_reg(dm, R_0xa04, BIT(27) | BIT(26) | BIT(25) | BIT(24), 0x0); /*CCK RX path off*/

			/*BBreset*/
			odm_set_bb_reg(dm, R_0x0, BIT(16), 0x0);
			odm_set_bb_reg(dm, R_0x0, BIT(16), 0x1);

			if (odm_get_bb_reg(dm, R_0x660, BIT(16)))
				odm_write_4byte(dm, 0x6b4, 0x89000006);
			/*RF_DBG(dm, DBG_RF_IQK, "[IQK]BBreset!!!!\n");*/
			break;
		}
	}
}
void _iqk_bb_for_dpk_setting_8822c(struct dm_struct *dm)
{
	odm_set_bb_reg(dm, R_0x1e24, BIT(17), 0x1);
	odm_set_bb_reg(dm, R_0x1cd0, BIT(28), 0x1);
	odm_set_bb_reg(dm, R_0x1cd0, BIT(29), 0x1);
	odm_set_bb_reg(dm, R_0x1cd0, BIT(30), 0x1);
	odm_set_bb_reg(dm, R_0x1cd0, BIT(31), 0x0);
	//odm_set_bb_reg(dm, R_0x1c68, 0x0f000000, 0xf);	
	odm_set_bb_reg(dm, 0x1d58, 0xff8, 0x1ff);
	odm_set_bb_reg(dm, 0x1864, BIT(31), 0x1);
	odm_set_bb_reg(dm, 0x4164, BIT(31), 0x1);
	odm_set_bb_reg(dm, R_0x180c, BIT(27), 0x1);
	odm_set_bb_reg(dm, R_0x410c, BIT(27), 0x1);
	odm_set_bb_reg(dm, R_0x186c, BIT(7), 0x1);
	odm_set_bb_reg(dm, 0x416c, BIT(7), 0x1);
	odm_set_bb_reg(dm, R_0x180c, 0x3, 0x0); //S0 -3 wire
	odm_set_bb_reg(dm, R_0x410c, 0x3, 0x0); //S1 -3wire
	odm_set_bb_reg(dm, 0x1a00, BIT(1) | BIT(0), 0x2);
	RF_DBG(dm, DBG_RF_IQK, "[IQK]_iqk_bb_for_dpk_setting_8822c!!!!\n");
}

void _iqk_rf_setting_8822c(struct dm_struct *dm)
{	
	odm_set_bb_reg(dm, 0x1bb8, BIT(20), 0x0);
	/*TxIQK mode S0,RF0x00[19:16]=0x4*/
	odm_set_rf_reg(dm, RF_PATH_A, 0xef, 0xfffff, 0x80000);
	odm_set_rf_reg(dm, RF_PATH_A, 0x33, 0x0000f, 0x4);
	odm_set_rf_reg(dm, RF_PATH_A, 0x3e, 0xfffff, 0x00003);
	odm_set_rf_reg(dm, RF_PATH_A, 0x3f, 0xfffff, 0xF60FF);//3F[15]=0, iPA off 
	odm_set_rf_reg(dm, RF_PATH_A, 0xef, 0xfffff, 0x00000);

	/*TxIQK mode S1,RF0x00[19:16]=0x4*/
	odm_set_rf_reg(dm, RF_PATH_B, 0xef, 0xfffff, 0x80000);
	odm_set_rf_reg(dm, RF_PATH_B, 0x33, 0x0000f, 0x4);
	odm_set_rf_reg(dm, RF_PATH_B, 0x3f, 0xfffff, 0xFD83F);//3F[15]=0, iPA off 
	odm_set_rf_reg(dm, RF_PATH_B, 0xef, 0xfffff, 0x00000);

	// RxIQK1 mode S0, RF0x00[19:16]=0x6
	odm_set_rf_reg(dm, RF_PATH_A, 0xef, 0xfffff, 0x80000); //[19]: WE_LUT_RFMODE
	odm_set_rf_reg(dm, RF_PATH_A, 0x33, 0x0000f, 0x6); //RFMODE
	odm_set_rf_reg(dm, RF_PATH_A, 0x3e, 0xfffff, 0x00003);
	odm_set_rf_reg(dm, RF_PATH_A, 0x3f, 0xfffff, 0x760FF);//3F[15]=0, iPA off , 3F[19]=0, POW_TXBB off
	odm_set_rf_reg(dm, RF_PATH_A, 0xef, 0xfffff, 0x00000);

	// RxIQK1 mode S1, RF0x00[19:16]=0x6
	odm_set_rf_reg(dm, RF_PATH_B, 0xef, 0xfffff, 0x80000);
	odm_set_rf_reg(dm, RF_PATH_B, 0x33, 0x0000f, 0x6);
	odm_set_rf_reg(dm, RF_PATH_B, 0x3f, 0xfffff, 0xDD83F);//3F[15]=0, iPA off 
	odm_set_rf_reg(dm, RF_PATH_B, 0xef, 0xfffff, 0x00000);

	// RxIQK2 mode S0, RF0x00[19:16]=0x7	
	odm_set_rf_reg(dm, RF_PATH_A, 0xef, 0xfffff, 0x80000); //[19]: WE_LUT_RFMODE
	odm_set_rf_reg(dm, RF_PATH_A, 0x33, 0x0000f, 0x7); //RFMODE
	odm_set_rf_reg(dm, RF_PATH_A, 0x3e, 0xfffff, 0x00003);
	odm_set_rf_reg(dm, RF_PATH_A, 0x3f, 0xfffff, 0x7DEFF);//3F[15]=1, iPA on ,3F[19]=0, POW_TXBB off
	odm_set_rf_reg(dm, RF_PATH_A, 0xef, 0xfffff, 0x00000);

	// RxIQK2 mode S1, RF0x00[19:16]=0x7
	odm_set_rf_reg(dm, RF_PATH_B, 0xef, 0xfffff, 0x80000); //[19]: WE_LUT_RFMODE
	odm_set_rf_reg(dm, RF_PATH_B, 0x33, 0x0000f, 0x7); //RFMODE
	odm_set_rf_reg(dm, RF_PATH_B, 0x3f, 0xfffff, 0xDF7BF);//3F[13]=1, iPA on ,3F[17]=0, POW_TXBB off
	odm_set_rf_reg(dm, RF_PATH_B, 0xef, 0xfffff, 0x00000);

	odm_set_rf_reg(dm, RF_PATH_A, RF_0x19, RFREG_MASK, 0x0);
	odm_set_rf_reg(dm, RF_PATH_B, RF_0x19, RFREG_MASK, 0x0);

	
	RF_DBG(dm, DBG_RF_IQK, "[IQK]_iqk_rf_setting_8822c RF01!!!!\n");
}

void _iqk_set_afe_8822c(struct dm_struct *dm)
{
	odm_set_bb_reg(dm, 0x1830, BIT(30), 0x0);
	odm_set_bb_reg(dm, 0x1860, 0xfffff000, 0xf0001);
	odm_set_bb_reg(dm, 0x4130, BIT(30), 0x0);
	odm_set_bb_reg(dm, 0x4160, 0xfffff000, 0xf0001);
	/*ADDA FIFO reset*/
	odm_write_4byte(dm, 0x1c38, 0x0);
	ODM_delay_us(10);
	odm_write_4byte(dm, 0x1c38, 0xffffffff);	
	RF_DBG(dm, DBG_RF_IQK, "[IQK]AFE setting for IQK mode!!!!\n");
}


void _iqk_afe_setting_8822c(
	struct dm_struct *dm,
	boolean do_iqk)
{
	u8 i;
	struct dm_iqk_info *iqk_info = &dm->IQK_info;

	if (do_iqk) {
		/*03_8822C_AFE_for_DPK.txt*/
		// AFE on Settings
		odm_write_4byte(dm, 0x1830, 0x700f0001);
		odm_write_4byte(dm, 0x1830, 0x700f0001);
		odm_write_4byte(dm, 0x1830, 0x701f0001);
		odm_write_4byte(dm, 0x1830, 0x702f0001);
		odm_write_4byte(dm, 0x1830, 0x703f0001);
		odm_write_4byte(dm, 0x1830, 0x704f0001);
		odm_write_4byte(dm, 0x1830, 0x705f0001);
		odm_write_4byte(dm, 0x1830, 0x706f0001);
		odm_write_4byte(dm, 0x1830, 0x707f0001);
		odm_write_4byte(dm, 0x1830, 0x708f0001);
		odm_write_4byte(dm, 0x1830, 0x709f0001);
		odm_write_4byte(dm, 0x1830, 0x70af0001);
		odm_write_4byte(dm, 0x1830, 0x70bf0001);
		odm_write_4byte(dm, 0x1830, 0x70cf0001);
		odm_write_4byte(dm, 0x1830, 0x70df0001);
		odm_write_4byte(dm, 0x1830, 0x70ef0001);
		odm_write_4byte(dm, 0x1830, 0x70ff0001);
		odm_write_4byte(dm, 0x1830, 0x70ff0001);
		odm_write_4byte(dm, 0x4130, 0x700f0001);
		odm_write_4byte(dm, 0x4130, 0x700f0001);
		odm_write_4byte(dm, 0x4130, 0x701f0001);
		odm_write_4byte(dm, 0x4130, 0x702f0001);
		odm_write_4byte(dm, 0x4130, 0x703f0001);
		odm_write_4byte(dm, 0x4130, 0x704f0001);
		odm_write_4byte(dm, 0x4130, 0x705f0001);
		odm_write_4byte(dm, 0x4130, 0x706f0001);
		odm_write_4byte(dm, 0x4130, 0x707f0001);
		odm_write_4byte(dm, 0x4130, 0x708f0001);
		odm_write_4byte(dm, 0x4130, 0x709f0001);
		odm_write_4byte(dm, 0x4130, 0x70af0001);
		odm_write_4byte(dm, 0x4130, 0x70bf0001);
		odm_write_4byte(dm, 0x4130, 0x70cf0001);
		odm_write_4byte(dm, 0x4130, 0x70df0001);
		odm_write_4byte(dm, 0x4130, 0x70ef0001);
		odm_write_4byte(dm, 0x4130, 0x70ff0001);
		odm_write_4byte(dm, 0x4130, 0x70ff0001);		
		/*ADDA FIFO reset*/
		odm_write_4byte(dm, 0x1c38, 0x0);
		ODM_delay_us(10);
		odm_write_4byte(dm, 0x1c38, 0xffffffff);
		RF_DBG(dm, DBG_RF_IQK, "[IQK]AFE setting for IQK mode!!!!\n");
	} else {
		if (iqk_info->is_tssi_mode) {
			odm_set_bb_reg(dm, R_0x1c38, MASKDWORD, 0xf7d5005e);
			odm_set_bb_reg(dm, R_0x1860, 0x00007000, 0x4 >> iqk_info->iqk_band);
			odm_set_bb_reg(dm, R_0x1830, MASKDWORD, 0x700b8041);
			odm_set_bb_reg(dm, R_0x1830, MASKDWORD, 0x701f0040 | (0x4 >> iqk_info->iqk_band));
			odm_set_bb_reg(dm, R_0x1830, MASKDWORD, 0x702f0040 | (0x4 >> iqk_info->iqk_band));
			odm_set_bb_reg(dm, R_0x1830, MASKDWORD, 0x703f0040 | (0x4 >> iqk_info->iqk_band));
			odm_set_bb_reg(dm, R_0x1830, MASKDWORD, 0x704f0040 | (0x4 >> iqk_info->iqk_band));
			odm_set_bb_reg(dm, R_0x1830, MASKDWORD, 0x705b8041);
			odm_set_bb_reg(dm, R_0x1830, MASKDWORD, 0x706f0040 | (0x4 >> iqk_info->iqk_band));

			odm_set_bb_reg(dm, R_0x4130, MASKDWORD, 0x700b8041);
			odm_set_bb_reg(dm, R_0x4130, MASKDWORD, 0x701f0040 | (0x4 >> iqk_info->iqk_band));
			odm_set_bb_reg(dm, R_0x4130, MASKDWORD, 0x702f0040 | (0x4 >> iqk_info->iqk_band));
			odm_set_bb_reg(dm, R_0x4130, MASKDWORD, 0x703f0040 | (0x4 >> iqk_info->iqk_band));
			odm_set_bb_reg(dm, R_0x4130, MASKDWORD, 0x704f0040 | (0x4 >> iqk_info->iqk_band));
			odm_set_bb_reg(dm, R_0x4130, MASKDWORD, 0x705b8041);
			odm_set_bb_reg(dm, R_0x4130, MASKDWORD, 0x706f0040 | (0x4 >> iqk_info->iqk_band));

			RF_DBG(dm, DBG_RF_IQK, "[IQK]AFE for TSSI mode\n");

		} else {
			// AFE Restore Settings
			odm_set_bb_reg(dm, R_0x1830, MASKDWORD, 0x700b8041);
			odm_set_bb_reg(dm, R_0x1830, MASKDWORD, 0x70144041);
			odm_set_bb_reg(dm, R_0x1830, MASKDWORD, 0x70244041);
			odm_set_bb_reg(dm, R_0x1830, MASKDWORD, 0x70344041);
			odm_set_bb_reg(dm, R_0x1830, MASKDWORD, 0x70444041);
			odm_set_bb_reg(dm, R_0x1830, MASKDWORD, 0x705b8041);
			odm_set_bb_reg(dm, R_0x1830, MASKDWORD, 0x70644041);
			odm_set_bb_reg(dm, R_0x1830, MASKDWORD, 0x707b8041);
			odm_set_bb_reg(dm, R_0x1830, MASKDWORD, 0x708b8041);
			odm_set_bb_reg(dm, R_0x1830, MASKDWORD, 0x709b8041);
			odm_set_bb_reg(dm, R_0x1830, MASKDWORD, 0x70ab8041);
			odm_set_bb_reg(dm, R_0x1830, MASKDWORD, 0x70bb8041);
			odm_set_bb_reg(dm, R_0x1830, MASKDWORD, 0x70cb8041);
			odm_set_bb_reg(dm, R_0x1830, MASKDWORD, 0x70db8041);
			odm_set_bb_reg(dm, R_0x1830, MASKDWORD, 0x70eb8041);
			odm_set_bb_reg(dm, R_0x1830, MASKDWORD, 0x70fb8041);
			
			odm_set_bb_reg(dm, R_0x4130, MASKDWORD, 0x700b8041);
			odm_set_bb_reg(dm, R_0x4130, MASKDWORD, 0x70144041);
			odm_set_bb_reg(dm, R_0x4130, MASKDWORD, 0x70244041);
			odm_set_bb_reg(dm, R_0x4130, MASKDWORD, 0x70344041);
			odm_set_bb_reg(dm, R_0x4130, MASKDWORD, 0x70444041);
			odm_set_bb_reg(dm, R_0x4130, MASKDWORD, 0x705b8041);
			odm_set_bb_reg(dm, R_0x4130, MASKDWORD, 0x70644041);
			odm_set_bb_reg(dm, R_0x4130, MASKDWORD, 0x707b8041);
			odm_set_bb_reg(dm, R_0x4130, MASKDWORD, 0x708b8041);
			odm_set_bb_reg(dm, R_0x4130, MASKDWORD, 0x709b8041);
			odm_set_bb_reg(dm, R_0x4130, MASKDWORD, 0x70ab8041);
			odm_set_bb_reg(dm, R_0x4130, MASKDWORD, 0x70bb8041);
			odm_set_bb_reg(dm, R_0x4130, MASKDWORD, 0x70cb8041);
			odm_set_bb_reg(dm, R_0x4130, MASKDWORD, 0x70db8041);
			odm_set_bb_reg(dm, R_0x4130, MASKDWORD, 0x70eb8041);
			odm_set_bb_reg(dm, R_0x4130, MASKDWORD, 0x70fb8041);
			RF_DBG(dm, DBG_RF_IQK, "[IQK]AFE for non-TSSI mode\n");
		}
#if 0
		/*11_8822C_BB_for_DPK_restore*/
		odm_set_bb_reg(dm, 0x1d0c, BIT(16), 0x1);
		odm_set_bb_reg(dm, 0x1d0c, BIT(16), 0x0);
		odm_set_bb_reg(dm, 0x1d0c, BIT(16), 0x1);
#endif
		odm_set_bb_reg(dm, 0x1bb8, BIT(20), 0x0);
		odm_set_bb_reg(dm, 0x1bcc, 0x000000ff, 0x0);

		// BB Restore Settings
		//odm_set_bb_reg(dm, 0x1c68, 0x0f000000, 0x0);
		odm_set_bb_reg(dm, 0x1d58, 0xff8, 0x0);
		//odm_set_bb_reg(dm, 0x1c3c, BIT(0), 0x1);
		//odm_set_bb_reg(dm, 0x1c3c, BIT(1), 0x1);
		odm_set_bb_reg(dm, 0x1864, BIT(31), 0x0);
		odm_set_bb_reg(dm, 0x4164, BIT(31), 0x0);
		odm_set_bb_reg(dm, 0x180c, BIT(27), 0x0);
		odm_set_bb_reg(dm, 0x410c, BIT(27), 0x0);
		odm_set_bb_reg(dm, 0x186c, BIT(7), 0x0);
		odm_set_bb_reg(dm, 0x416c, BIT(7), 0x0);
		odm_set_bb_reg(dm, 0x180c, BIT(1) | BIT(0), 0x3);
		odm_set_bb_reg(dm, 0x410c, BIT(1) | BIT(0), 0x3);
		odm_set_bb_reg(dm, 0x1a00, BIT(1) | BIT(0), 0x0);


		RF_DBG(dm, DBG_RF_IQK, "[IQK]AFE setting for Normal mode!!!!\n");
	}
}

void _iqk_restore_mac_bb_8822c(
	struct dm_struct *dm,
	u32 *MAC_backup,
	u32 *BB_backup,
	u32 *backup_mac_reg,
	u32 *backup_bb_reg)
{
	struct dm_iqk_info *iqk_info = &dm->IQK_info;
	u32 i;	

	/*toggle IGI*/
	odm_write_4byte(dm, 0x1d70, 0x50505050);

	for (i = 0; i < MAC_REG_NUM_8822C; i++){
		odm_write_4byte(dm, backup_mac_reg[i], MAC_backup[i]);
		//RF_DBG(dm, DBG_RF_IQK, "[IQK]restore mac = %x, value = %x\n",backup_mac_reg[i],MAC_backup[i]);
		}
	for (i = 0; i < BB_REG_NUM_8822C; i++){
		odm_write_4byte(dm, backup_bb_reg[i], BB_backup[i]);		
		//RF_DBG(dm, DBG_RF_IQK, "[IQK]restore bb = %x, value = %x\n",backup_bb_reg[i],BB_backup[i]);
		}
	/*rx go throughput IQK*/
#if 0
	odm_set_bb_reg(dm, 0x180c, BIT(31), 0x1);
	odm_set_bb_reg(dm, 0x410c, BIT(31), 0x1);
#else
	if (iqk_info->iqk_fail_report[0][0][RXIQK] == true) 
		odm_set_bb_reg(dm, 0x180c, BIT(31), 0x0);
	else
		odm_set_bb_reg(dm, 0x180c, BIT(31), 0x1);

	if (iqk_info->iqk_fail_report[0][1][RXIQK] == true) 
		odm_set_bb_reg(dm, 0x410c, BIT(31), 0x0);
	else
		odm_set_bb_reg(dm, 0x410c, BIT(31), 0x1);
#endif
	//odm_set_bb_reg(dm, 0x520c, BIT(31), 0x1);
	//odm_set_bb_reg(dm, 0x530c, BIT(31), 0x1);
	/*	RF_DBG(dm, DBG_RF_IQK, "[IQK]RestoreMacBB Success!!!!\n"); */
}

void _iqk_restore_rf_8822c(
	struct dm_struct *dm,
	u32 *rf_reg,
	u32 temp[][SS_8822C])
{
	u32 i;
	
	odm_set_rf_reg(dm, RF_PATH_A, RF_0xef, 0xfffff, 0x0);
	odm_set_rf_reg(dm, RF_PATH_B, RF_0xef, 0xfffff, 0x0);
	/*0xdf[4]=0*/
	//_iqk_rf_set_check_8822c(dm, RF_PATH_A, 0xdf, temp[0][RF_PATH_A] & (~BIT(4)));
	//_iqk_rf_set_check_8822c(dm, RF_PATH_B, 0xdf, temp[0][RF_PATH_B] & (~BIT(4)));

	for (i = 0; i < RF_REG_NUM_8822C; i++) {
		odm_set_rf_reg(dm, RF_PATH_A, rf_reg[i],
			       0xfffff, temp[i][RF_PATH_A]);
		odm_set_rf_reg(dm, RF_PATH_B, rf_reg[i],
			       0xfffff, temp[i][RF_PATH_B]);
	}
	
	odm_set_rf_reg(dm, RF_PATH_A, RF_0xde, BIT(16), 0x0);
	odm_set_rf_reg(dm, RF_PATH_B, RF_0xde, BIT(16), 0x0);
	RF_DBG(dm, DBG_RF_IQK, "[IQK]RestoreRF Success!!!!\n"); 
}

void _iqk_backup_iqk_8822c(
	struct dm_struct *dm,
	u8 step,
	u8 path)
{
	struct dm_iqk_info *iqk_info = &dm->IQK_info;
	u8 i, j, k;

	switch (step) {
	case 0:
		iqk_info->iqk_channel[1] = iqk_info->iqk_channel[0];
		//RF_DBG(dm, DBG_RF_IQK, "[IQK](0)iqk_info->iqk_channel[1] = %2x\n",iqk_info->iqk_channel[1]);
		for (i = 0; i < SS_8822C; i++) {
			iqk_info->lok_idac[1][i] = iqk_info->lok_idac[0][i];
			iqk_info->rxiqk_agc[1][i] = iqk_info->rxiqk_agc[0][i];
			iqk_info->bypass_iqk[1][i] = iqk_info->bypass_iqk[0][i];
			iqk_info->rxiqk_fail_code[1][i] = iqk_info->rxiqk_fail_code[0][i];
			for (j = 0; j < 2; j++) {
				iqk_info->iqk_fail_report[1][i][j] = iqk_info->iqk_fail_report[0][i][j];			
				//RF_DBG(dm, DBG_RF_IQK, "[IQK](2)iqk_info->iqk_fail_report[0][%x][%x] = %2x\n",i,j,iqk_info->iqk_fail_report[1][i][j] );
				for (k = 0; k <= 16; k++) {
					iqk_info->iqk_cfir_real[1][i][j][k] = iqk_info->iqk_cfir_real[0][i][j][k];
					iqk_info->iqk_cfir_imag[1][i][j][k] = iqk_info->iqk_cfir_imag[0][i][j][k];
				}
			}
		}

		for (i = 0; i < SS_8822C; i++) {
			iqk_info->rxiqk_fail_code[0][i] = 0x0;
			iqk_info->rxiqk_agc[0][i] = 0x0;
			for (j = 0; j < 2; j++) {
				iqk_info->iqk_fail_report[0][i][j] = true;
				iqk_info->gs_retry_count[0][i][j] = 0x0;
			}
			for (j = 0; j < 3; j++)
				iqk_info->retry_count[0][i][j] = 0x0;
		}
		/*backup channel*/
		iqk_info->iqk_channel[0] = iqk_info->rf_reg18;
		break;
	case 1: /*LOK backup*/
		iqk_info->lok_idac[0][path] = odm_get_rf_reg(dm, (enum rf_path)path, RF_0x58, RFREGOFFSETMASK);	
		//RF_DBG(dm, DBG_RF_IQK, "[IQK](4) iqk_info->lok_idac[0][%d]= %2x\n", path, iqk_info->lok_idac[0][path]);
		break;
	case 2: /*TXIQK backup*/
		iqk_get_cfir_8822c(dm, TX_IQK, path, false);
		break;		
	case 3: /*RXIQK backup*/		
		iqk_get_cfir_8822c(dm, RX_IQK, path, false);
		break;
	}
}

void _iqk_reload_iqk_setting_8822c(
	struct dm_struct *dm,
	u8 ch,
	u8 reload_idx /*1: reload TX, 2: reload LO, TX, RX*/
	)
{
	struct dm_iqk_info *iqk_info = &dm->IQK_info;
#if 1
	u8 i, path, idx;
	u16 iqk_apply[2] = {0x180c, 0x410c};
	u32 tmp = 0x0, tmp1 = 0x0, tmp2 = 0x0;
	boolean is_NB_IQK = false;
	
	if ((*dm->band_width == CHANNEL_WIDTH_5) ||(*dm->band_width == CHANNEL_WIDTH_10))
		is_NB_IQK = true;

	for (path = 0; path < SS_8822C; path++) {
		if (reload_idx == 2) {
			/*odm_set_rf_reg(dm, (enum rf_path)path, RF_0xdf, BIT(4), 0x1);*/
			tmp = odm_get_rf_reg(dm, (enum rf_path)path, RF_0xdf, RFREGOFFSETMASK) | BIT(4);
			_iqk_rf_set_check_8822c(dm, (enum rf_path)path, 0xdf, tmp);
			odm_set_rf_reg(dm, (enum rf_path)path, RF_0x58, RFREGOFFSETMASK, iqk_info->lok_idac[ch][path]);			
		}

		for (idx = 0; idx < reload_idx; idx++) {
			odm_set_bb_reg(dm, R_0x1b00, MASKDWORD, 0x8 | path << 1);			
			odm_write_1byte(dm, 0x1bcc, 0x0);
			if (is_NB_IQK) {
				odm_set_bb_reg(dm, R_0x1b20, BIT(26), 0x0);
				odm_set_bb_reg(dm, 0x1b38, MASKDWORD, iqk_info->nbtxk_1b38[path]);
				odm_set_bb_reg(dm, 0x1b3c, MASKDWORD, iqk_info->nbrxk_1b3c[path]);
			} else {
				odm_set_bb_reg(dm, R_0x1b20, BIT(26), 0x1);
				odm_set_bb_reg(dm, R_0x1b38, MASKDWORD, 0x40000000);
				odm_set_bb_reg(dm, R_0x1b3c, MASKDWORD, 0x40000000);
			}
			if (idx == TX_IQK) {//TXCFIR
				odm_set_bb_reg(dm, R_0x1b20, BIT(31) | BIT(30), 0x3);		
				tmp1 = 0xc0000001;
			} else {//RXCFIR
				odm_set_bb_reg(dm, R_0x1b20, BIT(31) | BIT(30), 0x1);
				tmp1 = 0x60000001;
			}
			for (i = 0; i <= 16; i++) {
				tmp2 = tmp1 | iqk_info->iqk_cfir_real[ch][path][idx][i] << 8;
				tmp2 = (tmp2 | i << 2) + 2;
				odm_set_bb_reg(dm, R_0x1bd8, MASKDWORD, tmp2);
			}
			for (i = 0; i <= 16; i++) {
				tmp2 = tmp1 | iqk_info->iqk_cfir_imag[ch][path][idx][i] << 8;
				tmp2 = (tmp2 | i << 2);
				odm_set_bb_reg(dm, R_0x1bd8, MASKDWORD, tmp2);		
			}
			if (idx == RX_IQK) {
				odm_set_bb_reg(dm, R_0x1b20, BIT(31) | BIT(30), 0x1);
				odm_set_bb_reg(dm, R_0x1bd8, MASKDWORD, 0xe0000001);		
				odm_set_bb_reg(dm, R_0x1b20, BIT(31) | BIT(30), 0x0);
				//odm_set_bb_reg(dm, R_0x1bd8, MASKDWORD, 0x0);
			}
		}
		// end for write CFIR SRAM
//		odm_set_bb_reg(dm, R_0x1bd8, MASKDWORD, 0xe0000000);
		odm_set_bb_reg(dm, R_0x1b20, BIT(31) | BIT(30), 0x0);
//		odm_set_bb_reg(dm, R_0x1bd8, MASKDWORD, 0x0);
	}
#endif
}

boolean
_iqk_reload_iqk_8822c(
	struct dm_struct *dm,
	boolean reset)
{
	struct dm_iqk_info *iqk_info = &dm->IQK_info;
	u8 i;
	iqk_info->is_reload = false;

	if (reset) {
		for (i = 0; i < 2; i++)
			iqk_info->iqk_channel[i] = 0x0;
	} else {
		iqk_info->rf_reg18 = odm_get_rf_reg(dm, RF_PATH_A, RF_0x18, RFREGOFFSETMASK);

		for (i = 0; i < 2; i++) {
			if (iqk_info->rf_reg18 == iqk_info->iqk_channel[i]) {
				_iqk_reload_iqk_setting_8822c(dm, i, 2);
				_iqk_fill_iqk_report_8822c(dm, i);
				RF_DBG(dm, DBG_RF_IQK, "[IQK]reload IQK result before!!!!\n");
				iqk_info->is_reload = true;
			}
		}
	}
	/*report*/
	odm_set_bb_reg(dm, R_0x1bf0, BIT(16), (u8)iqk_info->is_reload);
	odm_set_rf_reg(dm, RF_PATH_A, RF_0xdf, BIT(4), 0x0);
	odm_set_rf_reg(dm, RF_PATH_B, RF_0xdf, BIT(4), 0x0);
	return iqk_info->is_reload;
}

void _iqk_rfe_setting_8822c(
	struct dm_struct *dm,
	boolean ext_pa_on)
{
	/*TBD*/
	return;
#if 0
	if (ext_pa_on) {
		/*RFE setting*/
		odm_write_4byte(dm, 0xcb0, 0x77777777);
		odm_write_4byte(dm, 0xcb4, 0x00007777);
		odm_write_4byte(dm, 0xcbc, 0x0000083B);
		odm_write_4byte(dm, 0xeb0, 0x77777777);
		odm_write_4byte(dm, 0xeb4, 0x00007777);
		odm_write_4byte(dm, 0xebc, 0x0000083B);
		/*odm_write_4byte(dm, 0x1990, 0x00000c30);*/
		RF_DBG(dm, DBG_RF_IQK, "[IQK]external PA on!!!!\n");
	} else {
		/*RFE setting*/
		odm_write_4byte(dm, 0xcb0, 0x77777777);
		odm_write_4byte(dm, 0xcb4, 0x00007777);
		odm_write_4byte(dm, 0xcbc, 0x00000100);
		odm_write_4byte(dm, 0xeb0, 0x77777777);
		odm_write_4byte(dm, 0xeb4, 0x00007777);
		odm_write_4byte(dm, 0xebc, 0x00000100);
		/*odm_write_4byte(dm, 0x1990, 0x00000c30);*/
		/*RF_DBG(dm, DBG_RF_IQK, "[IQK]external PA off!!!!\n");*/
	}
#endif
}

void _iqk_setrf_bypath_8822c(
	struct dm_struct *dm)
{
	u8 path;
	u32 tmp;

	/*TBD*/
}
void _iqk_rf_direct_access_8822c(
	struct dm_struct *dm,
	u8 path,
	boolean direct_access)
{
	if(!direct_access) {//PI	
		if ((enum rf_path)path == RF_PATH_A)
			odm_set_bb_reg(dm, 0x1c, BIT(31) | BIT(30), 0x0);
		else if((enum rf_path)path == RF_PATH_B)
			odm_set_bb_reg(dm, 0xec, BIT(31) | BIT(30), 0x0);
		//odm_set_bb_reg(dm, 0x1c, BIT(31) | BIT(30), 0x0);	
		//odm_set_bb_reg(dm, 0xec, BIT(31) | BIT(30), 0x0);
	} else {//direct access
		if ((enum rf_path)path == RF_PATH_A)
			odm_set_bb_reg(dm, 0x1c, BIT(31) | BIT(30), 0x2);
		else if((enum rf_path)path == RF_PATH_B)
			odm_set_bb_reg(dm, 0xec, BIT(31) | BIT(30), 0x2);
		//odm_set_bb_reg(dm, 0x1c, BIT(31) | BIT(30), 0x2);
		//odm_set_bb_reg(dm, 0xec, BIT(31) | BIT(30), 0x2);
	}
	/*
	RF_DBG(dm, DBG_RF_IQK, "[IQK]0x1c = 0x%x, 0xec = 0x%x\n",
	       odm_read_4byte(dm, 0x1c), odm_read_4byte(dm, 0xec));
	*/
}

void _iqk_bbtx_path_8822c(
	struct dm_struct *dm,
	u8 path)
{
	u32 temp1 = 0, temp2 = 0;

	switch (path) {
	case RF_PATH_A:
		temp1 = 0x11111111;
		temp2 = 0x1;
		break;
	case RF_PATH_B:
		temp1 = 0x22222222;
		temp2 = 0x2;
		break;
	}
	odm_write_4byte(dm, 0x820, temp1);
	odm_set_bb_reg(dm, 0x824, 0xf0000, temp2);
}

void _iqk_iqk_mode_8822c(
	struct dm_struct *dm,
	boolean is_iqkmode)
{
	u32 temp1, temp2;
	/*RF can't be write in iqk mode*/
	/*page 1b can't */
	if (is_iqkmode)
		odm_set_bb_reg(dm, 0x1cd0, BIT(31), 0x1);
	else
		odm_set_bb_reg(dm, 0x1cd0, BIT(31), 0x0);	
}

void _iqk_macbb_8822c(
	struct dm_struct *dm)
{
	struct dm_iqk_info *iqk_info = &dm->IQK_info;

	if (iqk_info->is_tssi_mode) {
		odm_set_bb_reg(dm, R_0x1e7c, BIT(30), 0x0);
		odm_set_bb_reg(dm, R_0x18a4, BIT(28), 0x0);
		odm_set_bb_reg(dm, R_0x41a4, BIT(28), 0x0);
	}

	/*MACBB register setting*/
	odm_write_1byte(dm, REG_TXPAUSE, 0xff);
	//0x73[2] = 1 (PTA control path is at WLAN)
	odm_set_bb_reg(dm, 0x70, 0xff000000, 0x06);
	/*BB CCA off*/
	//odm_set_bb_reg(dm, 0x1c68, BIT(27) | BIT(26) | BIT(25) | BIT(24), 0xf);
	//odm_set_bb_reg(dm, 0x1d58, 0xff8, 0x1ff);
	//odm_set_bb_reg(dm, 0x1c68, 0xff8, 0x1ff);
	/*tx go throughput IQK*/
	odm_set_bb_reg(dm, 0x1e24, BIT(17), 0x1);
	/*enable IQK block*/
	odm_set_bb_reg(dm, 0x1cd0, BIT(30) | BIT(29) | BIT(28), 0x7);	
	/*enable IQK loop back in BB*/
	odm_set_bb_reg(dm, 0x1d60, BIT(31), 0x1);
	/*ADDA FIFO reset*/
	odm_write_4byte(dm, 0x1c38, 0xffffffff);
	/*CCK off*/
	//odm_set_bb_reg(dm, 0x1c3c, BIT(0), 0x0);
	//odm_set_bb_reg(dm, 0x1c3c, BIT(1), 0x0);
	odm_set_bb_reg(dm, R_0x1a14, 0x300, 0x3);

	/*r_iqk_dpk_clock_src*/
	//odm_set_bb_reg(dm, R_0x1cd0, 0xf0000000, 0x7);

	/*rx path on*/
	odm_set_bb_reg(dm, 0x824, 0x30000, 0x3);

	RF_DBG(dm, DBG_RF_IQK, "[IQK]_iqk_macbb_8822c!!!!\n");
}

void _iqk_lok_setting_8822c(
	struct dm_struct *dm,
	u8 path,
	u8 idac_bs)
{
	struct dm_iqk_info *iqk_info = &dm->IQK_info;

	boolean is_NB_IQK = false;
	u32 temp;

	if ((*dm->band_width == CHANNEL_WIDTH_5) ||(*dm->band_width == CHANNEL_WIDTH_10))
		is_NB_IQK = true;

	_iqk_cal_path_off_8822c(dm);
	//_iqk_bbtx_path_8822c(dm, path);
	odm_write_4byte(dm, 0x1b00, 0x8 | path << 1);	
	odm_set_bb_reg(dm, 0x1b20, BIT(31) | BIT(30), 0x0);
	odm_set_bb_reg(dm, 0x1b20, 0x3e0, 0x12);// 12dB
	
	odm_set_rf_reg(dm, (enum rf_path)path, RF_0xdf, BIT(4), 0x0);
	// Disable bypass TXBB @ RF0x0[19:16]=0x6 and 0x7
	odm_set_rf_reg(dm, (enum rf_path)path, 0x9e, BIT(5), 0x0);		
	odm_set_rf_reg(dm, (enum rf_path)path, 0x9e, BIT(10), 0x0);

	
	//LOK_RES Table
	if (*dm->band_type == ODM_BAND_2_4G) {
		odm_set_rf_reg(dm, (enum rf_path)path, RF_0xde, BIT(16), 0x1);
		odm_set_rf_reg(dm, (enum rf_path)path, RF_0x56, 0xfff, 0x887);
		odm_set_rf_reg(dm, (enum rf_path)path, RF_0xef, BIT(2), 0x1);
		//odm_set_rf_reg(dm, (enum rf_path)path, RF_0x18, BIT(16), 0x0);
		//odm_set_rf_reg(dm, (enum rf_path)path, RF_0x33, BIT(0), 0x0);		
		odm_set_rf_reg(dm, (enum rf_path)path, RF_0x08, 0x70, idac_bs);		
		odm_set_rf_reg(dm, (enum rf_path)path, RF_0xef, BIT(2), 0x0);		
	} else {	
		odm_set_rf_reg(dm, (enum rf_path)path, RF_0xde, BIT(16), 0x1);
		odm_set_rf_reg(dm, (enum rf_path)path, RF_0x56, 0xfff, 0x868);
		odm_set_rf_reg(dm, (enum rf_path)path, RF_0xef, BIT(2), 0x1);
		//odm_set_rf_reg(dm, (enum rf_path)path, RF_0x18, BIT(16), 0x1);
		odm_set_rf_reg(dm, (enum rf_path)path, RF_0x33, BIT(0), 0x0);
		odm_set_rf_reg(dm, (enum rf_path)path, RF_0x08, 0x70, idac_bs);		
		odm_set_rf_reg(dm, (enum rf_path)path, RF_0xef, BIT(2), 0x0);
	}	
	odm_set_rf_reg(dm, (enum rf_path)path, 0x57, BIT(0), 0x0);

//TX_LOK
	odm_set_rf_reg(dm, (enum rf_path)path, RF_0xef, BIT(4), 0x1);
	if (*dm->band_type == ODM_BAND_2_4G) {
		odm_set_rf_reg(dm, (enum rf_path)path, RF_0x33, 0x7f, 0x00);
		odm_write_1byte(dm, 0x1bcc, 0x09);		
	} else {
		odm_set_rf_reg(dm, (enum rf_path)path, RF_0x33, 0x7f, 0x20);
		odm_write_1byte(dm, 0x1bcc, 0x09);
	}
	odm_write_1byte(dm, 0x1b10, 0x0);

	if(is_NB_IQK)
		odm_set_bb_reg(dm, 0x1b2c, 0xfff, 0x08);
	else	
		odm_set_bb_reg(dm, 0x1b2c, 0xfff, 0x38);
}

void _iqk_reload_lok_setting_8822c(
	struct dm_struct *dm,
	u8 path)
{
#if 1
	struct dm_iqk_info *iqk_info = &dm->IQK_info;
	u32 tmp;
	u8 idac_i, idac_q;
	u8 i;

	idac_i = (u8)((iqk_info->rf_reg58 & 0xfc000) >> 14);
	idac_q = (u8)((iqk_info->rf_reg58 & 0x3f00) >> 8);
	odm_set_rf_reg(dm, (enum rf_path)path, RF_0xdf, BIT(4), 0x0);//W LOK table
	odm_set_rf_reg(dm, (enum rf_path)path, RF_0xef, BIT(4), 0x1);

	if (*dm->band_type == ODM_BAND_2_4G)
		odm_set_rf_reg(dm, (enum rf_path)path, RF_0x33, 0x7f, 0x00);
	else
		odm_set_rf_reg(dm, (enum rf_path)path, RF_0x33, 0x7f, 0x20);

	odm_set_rf_reg(dm, (enum rf_path)path, RF_0x08, 0xfc000, idac_i);
	odm_set_rf_reg(dm, (enum rf_path)path, RF_0x08, 0x003f0, idac_q);
	odm_set_rf_reg(dm, (enum rf_path)path, RF_0xef, BIT(4), 0x0);// stop write
	
	tmp = odm_get_rf_reg(dm, (enum rf_path)path, RF_0x58, 0xfffff);
	RF_DBG(dm, DBG_RF_IQK, "[IQK]S%d,reload 0x58 = 0x%x\n", path, tmp);
#endif
}

void _iqk_txk_setting_8822c(
	struct dm_struct *dm,
	u8 path)
{
	u32 rf_reg64 = 0x0;
	u32 curr_thermal = 0x0, ee_thermal = 0x0;
	u32 rf_0x56 = 0x0;
	boolean flag = false;
	u8 threshold = 0x10;
	boolean is_NB_IQK = false;

	if ((*dm->band_width == CHANNEL_WIDTH_5) ||(*dm->band_width == CHANNEL_WIDTH_10))
		is_NB_IQK = true;

	odm_write_4byte(dm, 0x1b00, 0x8 | path << 1);	
	odm_set_bb_reg(dm, 0x1bb8, BIT(20), 0x0);
	odm_write_4byte(dm, 0x1b20, 0x00040008);

	path = (enum rf_path)path;
	if (*dm->band_type == ODM_BAND_2_4G) {
		rf_0x56 = 0x887;
		odm_set_rf_reg(dm, (enum rf_path)path, RF_0x56, 0xfff, rf_0x56);
		odm_write_1byte(dm, 0x1bcc, 0x09);
	} else {
		rf_0x56 = 0x8c6;
#if 1
		//TANK 
		rf_reg64 = odm_get_rf_reg(dm, path, RF_0x64, MASK20BITS);
		rf_reg64 = (rf_reg64 & 0xfff0f) | 0x010;
		odm_set_rf_reg(dm, path, RF_0xdf, BIT(6), 0x1);
		odm_set_rf_reg(dm, path, RF_0x64, MASK20BITS, rf_reg64);
#endif
#if 1
		/*get thermal meter*/
		ee_thermal = _iqk_get_efuse_thermal_8822c(dm, path);
		odm_set_rf_reg(dm, path, 0x42, BIT(17) | BIT(16), 0x3);
		halrf_delay_10us(20);
		curr_thermal = (u8)odm_get_rf_reg(dm, path, 0x42, 0xfc00);
		if (ee_thermal > curr_thermal)
			flag = ee_thermal - curr_thermal > threshold ? true : false;
		if (flag)
			rf_0x56 = 0x886;
		odm_set_rf_reg(dm, path, RF_0x56, 0xfff, rf_0x56);
#endif
		odm_write_1byte(dm, 0x1bcc, 0x09);
	}
	
	if(is_NB_IQK)
		odm_set_bb_reg(dm, 0x1b2c, 0xfff, 0x08);
	else	
		odm_set_bb_reg(dm, 0x1b2c, 0xfff, 0x38);
}

void _iqk_lok_for_rxk_setting_8822c(
	struct dm_struct *dm,
	u8 path)
{
	struct dm_iqk_info *iqk_info = &dm->IQK_info;

	boolean is_NB_IQK = false;

	if ((*dm->band_width == CHANNEL_WIDTH_5) ||(*dm->band_width == CHANNEL_WIDTH_10))
		is_NB_IQK = true;

	_iqk_cal_path_off_8822c(dm);
	odm_write_4byte(dm, 0x1b00, 0x8 | path << 1);
	odm_set_bb_reg(dm, 0x1bb8, BIT(20), 0x0);	
	odm_set_bb_reg(dm, 0x1b20, BIT(31) | BIT(30), 0x0);
	
	//force 0x53[0]=1, force PA on
	odm_set_rf_reg(dm, (enum rf_path)path, 0x53, BIT(0), 0x1);

	//LOK_RES Table
	if (*dm->band_type == ODM_BAND_2_4G) {
		odm_set_rf_reg(dm, (enum rf_path)path, 0x00, 0xf0000, 0x7);
		odm_set_rf_reg(dm, (enum rf_path)path, 0x9e, BIT(5), 0x1);		
		odm_set_rf_reg(dm, (enum rf_path)path, 0x9e, BIT(10), 0x1);
		odm_set_bb_reg(dm, 0x1b20, 0x3e0, 0x12);// 12dB
		odm_set_rf_reg(dm, (enum rf_path)path, RF_0xde, BIT(16), 0x1);
		odm_set_rf_reg(dm, (enum rf_path)path, RF_0x56, 0xfff, 020);
		odm_set_rf_reg(dm, (enum rf_path)path, RF_0xef, BIT(2), 0x1);
		//odm_set_rf_reg(dm, (enum rf_path)path, RF_0x18, BIT(16), 0x0);
		odm_set_rf_reg(dm, (enum rf_path)path, RF_0x33, BIT(0), 0x0);		
		odm_set_rf_reg(dm, (enum rf_path)path, RF_0x08, 0x70, 0x4);		
		odm_set_rf_reg(dm, (enum rf_path)path, RF_0xef, BIT(2), 0x0);		
	} else {
		odm_set_rf_reg(dm, (enum rf_path)path, 0x00, 0xf0000, 0x7);
		odm_set_rf_reg(dm, (enum rf_path)path, 0x9e, BIT(5), 0x1);
		odm_set_rf_reg(dm, (enum rf_path)path, 0x9e, BIT(10), 0x1);
		odm_set_rf_reg(dm, (enum rf_path)path, RF_0xde, BIT(16), 0x1);
		odm_set_rf_reg(dm, (enum rf_path)path, RF_0x56, 0xfff, 0x000);
		odm_set_rf_reg(dm, (enum rf_path)path, RF_0xef, BIT(2), 0x1);
		//odm_set_rf_reg(dm, (enum rf_path)path, RF_0x18, BIT(16), 0x1);
		odm_set_rf_reg(dm, (enum rf_path)path, RF_0x33, BIT(0), 0x1);
		odm_set_rf_reg(dm, (enum rf_path)path, RF_0x08, 0x70, 0x4);		
		odm_set_rf_reg(dm, (enum rf_path)path, RF_0xef, BIT(2), 0x0);
	}	
		odm_set_rf_reg(dm, (enum rf_path)path, 0x57, BIT(0), 0x0);
	
	//TX_LOK
	odm_set_rf_reg(dm, (enum rf_path)path, RF_0xef, BIT(4), 0x1);
	if (*dm->band_type == ODM_BAND_2_4G) {
		odm_set_rf_reg(dm, (enum rf_path)path, RF_0x33, 0x7f, 0x00);
		odm_write_1byte(dm, 0x1bcc, 0x09);		
	} else {
		odm_set_rf_reg(dm, (enum rf_path)path, RF_0x33, 0x7f, 0x20);
		odm_write_1byte(dm, 0x1bcc, 0x09);
	}
	odm_set_rf_reg(dm, (enum rf_path)path, RF_0xef, BIT(4), 0x1); //LOK _Write_en
	odm_write_1byte(dm, 0x1b10, 0x0);		
	odm_write_1byte(dm, 0x1bcc, 0x12);
	if(is_NB_IQK)
		odm_set_bb_reg(dm, 0x1b2c, 0xfff, 0x008);
	else
		odm_set_bb_reg(dm, 0x1b2c, 0xfff, 0x038);
}

//static u8 wlg_lna[5] = {0x0, 0x1, 0x2, 0x3, 0x5};
//static u8 wla_lna[5] = {0x0, 0x1, 0x3, 0x4, 0x5};
void _iqk_rxk1_setting_8822c(
	struct dm_struct *dm,
	u8 path)
{
	struct dm_iqk_info *iqk = &dm->IQK_info;
	boolean is_NB_IQK = false;

	if ((*dm->band_width == CHANNEL_WIDTH_5) ||(*dm->band_width == CHANNEL_WIDTH_10))
		is_NB_IQK = true;

	_iqk_cal_path_off_8822c(dm);
	odm_write_4byte(dm, 0x1b00, 0x8 | path << 1);
	odm_set_bb_reg(dm, 0x1bb8, BIT(20), 0x0);
	odm_set_bb_reg(dm, 0x1b20, BIT(31) | BIT(30), 0x0);
	//odm_write_4byte(dm, 0x1bd8, 0x0);

	odm_set_bb_reg(dm, 0x1b20, 0x3e0, 0x12); // 12dB
	if (*dm->band_type == ODM_BAND_2_4G) {
		odm_set_rf_reg(dm, (enum rf_path)path, RF_0xde, BIT(16), 0x1);
		odm_set_rf_reg(dm, (enum rf_path)path, RF_0x56, 0xfff, 0x020);
		odm_write_1byte(dm, 0x1bcc, 0x12);
	} else {	
		odm_set_rf_reg(dm, (enum rf_path)path, RF_0xde, BIT(16), 0x1);
		odm_set_rf_reg(dm, (enum rf_path)path, RF_0x56, 0xfff, 0x000);
		odm_write_1byte(dm, 0x1bcc, 0x12);
	}
	
	if(is_NB_IQK)
		odm_set_bb_reg(dm, 0x1b2c, 0xfff, 0x008);
	else
		odm_set_bb_reg(dm, 0x1b2c, 0xfff, 0x038);
}

void _iqk_rxk2_setting_8822c(
	struct dm_struct *dm,
	u8 path,
	boolean is_gs)
{
	struct dm_iqk_info *iqk = &dm->IQK_info;
	boolean is_NB_IQK = false;

	if ((*dm->band_width == CHANNEL_WIDTH_5) ||(*dm->band_width == CHANNEL_WIDTH_10))
		is_NB_IQK = true;
	odm_write_4byte(dm, 0x1b00, 0x8 | path << 1);
	odm_set_bb_reg(dm, 0x1b20, BIT(31) | BIT(30), 0x0);
	//odm_write_4byte(dm, 0x1bd8, 0x0);

	if (*dm->band_type == ODM_BAND_2_4G) {
		if (is_gs) {
			iqk->tmp1bcc = 0x12;
		}		
		odm_write_1byte(dm, 0x1bcc, iqk->tmp1bcc);
		odm_set_rf_reg(dm, (enum rf_path)path, RF_0xde, BIT(16), 0x1);
		odm_set_rf_reg(dm, (enum rf_path)path, RF_0x56, 0xfff, 0x020);
		odm_set_bb_reg(dm, 0x1b18, BIT(1), 0x1);
		odm_write_4byte(dm, 0x1b24, 0x00071808); //LNA=0110, RXBB=00000
		odm_write_1byte(dm, 0x1b10, 0x0);		
	} else {
	
		if (is_gs) {
			iqk->tmp1bcc = 0x12;
		}
		odm_set_rf_reg(dm, (enum rf_path)path, RF_0xde, BIT(16), 0x1);
		odm_set_rf_reg(dm, (enum rf_path)path, RF_0x56, 0xfff, 0x000);	
		odm_write_1byte(dm, 0x1bcc, iqk->tmp1bcc);
		odm_set_bb_reg(dm, 0x1b18, BIT(1), 0x1);
		odm_write_4byte(dm, 0x1b24, 0x00070c08); //LNA=011	
		odm_write_1byte(dm, 0x1b10, 0x0);
	}
	
	if(is_NB_IQK)
		odm_set_bb_reg(dm, 0x1b2c, 0xfff, 0x008);
	else
		odm_set_bb_reg(dm, 0x1b2c, 0xfff, 0x038);
}



void
_iqk_set_lok_lut_8822c(
	struct dm_struct *dm,
	u8 path)
{
#if 0
	u32 temp;
	u8 idac_i, idac_q;
	u8 i;

	temp = odm_get_rf_reg(dm, (enum rf_path)path, RF_0x58, 0xfffff);
	RF_DBG(dm, DBG_RF_IQK, "[IQK]setlut_0x58 = 0x%x\n", temp);
	idac_i = (u8)((temp & 0xfc000) >> 14);
	idac_q = (u8)((temp & 0x3f0) >> 4);
	temp =  (idac_i << 6) | idac_q;
	odm_set_rf_reg(dm, (enum rf_path)path, RF_0xdf, BIT(4), 0x0);
	odm_set_rf_reg(dm, (enum rf_path)path, RF_0xef, BIT(4), 0x1);
	for (i = 0; i < 8; i++) {
		temp = (i << 14) | (temp & 0xfff);
		if (*dm->band_type == ODM_BAND_2_4G)
			odm_set_rf_reg(dm, (enum rf_path)path, 0x33, 0xfffff, temp);
		else
			odm_set_rf_reg(dm, (enum rf_path)path, 0x33, 0xfffff, 0x20 | temp);
		RF_DBG(dm, DBG_RF_IQK, "[IQK]path =%d,0x33  = 0x%x!!!\n", path, temp);
	}
	odm_set_rf_reg(dm, (enum rf_path)path, RF_0xef, BIT(4), 0x0);
#endif
}

boolean
_iqk_rx_iqk_gain_search_fail_8822c(
	struct dm_struct *dm,
	u8 path,
	u8 step)
{


	struct dm_iqk_info *iqk = &dm->IQK_info;
	boolean fail = true, k2fail = true;
	u32 IQK_CMD = 0x0, rf_reg0 = 0x0, tmp = 0x0, bb_idx = 0x0;
	u8 IQMUX[5] = {0x9, 0x12, 0x1b, 0x24, 0x24};
	u8 idx;

	RF_DBG(dm, DBG_RF_IQK, "[IQK]============ S%d RXIQK GainSearch ============\n", path);

	if (step == RXIQK1) {		
		IQK_CMD = (0x208 | (1 << (path + 4)) | (path << 1));
		RF_DBG(dm, DBG_RF_IQK, "[IQK]S%d GS%d_Trigger = 0x%x\n", path,
	       	       step, IQK_CMD);
		
		if(dm->cut_version == ODM_CUT_E) {
			odm_set_rf_reg(dm, (enum rf_path)path, 0x0, 0xf0000, 0x4);
			odm_set_rf_reg(dm, (enum rf_path)path, 0x8f, BIT(14), 0x1);
		}
		halrf_delay_10us(1);
		odm_write_4byte(dm, 0x1b00, IQK_CMD);
		odm_write_4byte(dm, 0x1b00, IQK_CMD + 0x1);
		fail = _iqk_check_cal_8822c(dm, path, 0x1);
	} else if (step == RXIQK2) {
		for (idx = 0; idx < 4; idx++) {
			if (iqk->tmp1bcc == IQMUX[idx])
				break;
		}
		if (idx == 4)
			RF_DBG(dm, DBG_RF_IQK, "[IQK] rx_gs overflow\n");

		odm_write_4byte(dm, 0x1b00, 0x8 | path << 1);	
		odm_write_4byte(dm, 0x1bcc, iqk->tmp1bcc);

		IQK_CMD = (0x308 | (1 << (path + 4)) | (path << 1));
		RF_DBG(dm, DBG_RF_IQK, "[IQK]S%d GS%d_Trigger = 0x%x\n", path,
		       step, IQK_CMD);
		
		if(dm->cut_version == ODM_CUT_E) {
			odm_set_rf_reg(dm, (enum rf_path)path, 0x0, 0xf0000, 0x7);
			odm_set_rf_reg(dm, (enum rf_path)path, 0x8f, BIT(14), 0x0);
		}
		halrf_delay_10us(1);
		odm_write_4byte(dm, 0x1b00, IQK_CMD);
		odm_write_4byte(dm, 0x1b00, IQK_CMD + 0x1);
		halrf_delay_10us(2);
		rf_reg0 = odm_get_rf_reg(dm, (enum rf_path)path,
					 RF_0x0, MASK20BITS);

		k2fail = _iqk_check_cal_8822c(dm, path, 0x1);

		if (k2fail == true) {
			iqk->tmp1bcc = IQMUX[idx++];
			return true;
		}
		odm_write_4byte(dm, 0x1b00, 0x00000008 | path << 1);
		
		tmp = (rf_reg0 & 0x1fe0) >> 5;
		iqk->lna_idx = tmp >> 5; // lna value
		bb_idx = tmp & 0x1f;
		if (bb_idx <= 0x1) {
			if (idx != 3)
				idx++;
			else
				iqk->isbnd = true;
			fail = true;
		} else if (bb_idx >= 0xa) {
			if (idx != 0)
				idx--;
			else
				iqk->isbnd = true;
			fail = true;
		} else {
			fail = false;
			iqk->isbnd = false;
		}
		
		if (iqk->isbnd)
			fail = false;
		if(idx < 5)
			iqk->tmp1bcc = IQMUX[idx];

		if (fail == false){
			tmp = (iqk->tmp1bcc << 8) |  bb_idx ;
			odm_write_4byte(dm, 0x1be8, tmp);
			RF_DBG(dm, DBG_RF_IQK, "[IQK]S%d 0x1be8 = %x\n",path, tmp);
		}
}
	
return fail;

	
}

boolean
_lok_check_8822c(void *dm_void, u8 path)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct dm_iqk_info *iqk_info = &dm->IQK_info;
	struct _hal_rf_ *rf = &dm->rf_table;
	u32 temp;
	u8 idac_i, idac_q;
	u8 i;

	_iqk_cal_path_off_8822c(dm);
	odm_write_4byte(dm, 0x1b00, 0x8 | path << 1);	

	temp = odm_get_rf_reg(dm, (enum rf_path)path, RF_0x58, 0xfffff);
	//RF_DBG(dm, DBG_RF_IQK, "[IQK](1)setlut_0x58 = 0x%x\n", temp);
	idac_i = (u8)((temp & 0xfc000) >> 14);
	idac_q = (u8)((temp & 0x03f00) >> 8);

	if (idac_i <= 0x3 || idac_i >= 0x3c || idac_q <= 0x3 || idac_q >= 0x3c)
		return false;
	else
		return true;

}


boolean
_lok_one_shot_8822c(
	void *dm_void,
	u8 path,
	boolean for_rxk)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct dm_iqk_info *iqk_info = &dm->IQK_info;
	struct _hal_rf_ *rf = &dm->rf_table;

	u8 delay_count = 0;
	boolean LOK_notready = false;
	u32 temp = 0;
	u32 IQK_CMD = 0x0;
	u8 idac_i, idac_q;

	_iqk_set_gnt_wl_gnt_bt_8822c(dm, true);
	if (for_rxk) {
		RF_DBG(dm, DBG_RF_IQK,
			"[IQK]======S%d LOK for RXK======\n", path);
		IQK_CMD = 0x8 | (1 << (4 + path)) | (path << 1);
		if(dm->cut_version == ODM_CUT_E) {
			odm_set_rf_reg(dm, (enum rf_path)path, 0x0, 0xf0000, 0x6);
			odm_set_rf_reg(dm, (enum rf_path)path, 0x8f, BIT(14), 0x1);
			RF_DBG(dm, DBG_RF_IQK, "[IQK]0x00 =%x, 0x8f = 0x%x\n", odm_get_rf_reg(dm, path, 0x0, 0xfffff), odm_get_rf_reg(dm, path, 0x8f, 0xfffff));
			RF_DBG(dm, DBG_RF_IQK, "[IQK]0x38 =%x, 0x8f = 0x%x\n", _iqk_btc_read_indirect_reg_8822c(dm, 0x38), odm_get_bb_reg(dm, 0x70, 0xff000000));

		}
		halrf_delay_10us(1);
	} else { 
		RF_DBG(dm, DBG_RF_IQK,
			"[IQK]======S%d LOK======\n", path);
		IQK_CMD = 0x8 | (1 << (4 + path)) | (path << 1);
		
		if(dm->cut_version == ODM_CUT_E) {
			odm_set_rf_reg(dm, (enum rf_path)path, 0x0, 0xf0000, 0x4);
			odm_set_rf_reg(dm, (enum rf_path)path, 0x8f, BIT(14), 0x1);
			RF_DBG(dm, DBG_RF_IQK, "[IQK]0x00 =%x, 0x8f = 0x%x\n", odm_get_rf_reg(dm, path, 0x0, 0xfffff), odm_get_rf_reg(dm, path, 0x8f, 0xfffff));
			RF_DBG(dm, DBG_RF_IQK, "[IQK]0x38 =%x, 0x8f = 0x%x\n", _iqk_btc_read_indirect_reg_8822c(dm, 0x38), odm_get_bb_reg(dm, 0x70, 0xff000000));

	}
		halrf_delay_10us(1);
	}
	RF_DBG(dm, DBG_RF_IQK, "[IQK]LOK_Trigger = 0x%x\n", IQK_CMD);

	RF_DBG(dm, DBG_RF_IQK, "[IQK]0x00 =%x, 0x8f = 0x%x\n", odm_get_rf_reg(dm, path, 0x0, 0xfffff), odm_get_rf_reg(dm, path, 0x8f, 0xfffff));
	RF_DBG(dm, DBG_RF_IQK, "[IQK]0x38 =%x, 0x8f = 0x%x\n", _iqk_btc_read_indirect_reg_8822c(dm, 0x38), odm_get_bb_reg(dm, 0x70, 0xff000000));

	
	_iqk_rf_direct_access_8822c(dm, (enum rf_path)path, false);
	odm_write_4byte(dm, 0x1b00, IQK_CMD);
	odm_write_4byte(dm, 0x1b00, IQK_CMD + 1);	
	halrf_delay_10us(200);
	_iqk_rf_direct_access_8822c(dm, (enum rf_path)path, true);
	/*LOK: CMD ID = 0	{0xf8000018, 0xf8000028}*/
	/*LOK: CMD ID = 0	{0xf8000019, 0xf8000029}*/

	// idx of LOK LUT table, EF[4]:WE_LUT_TX_LOK
	odm_set_rf_reg(dm, (enum rf_path)path, RF_0xef, BIT(4), 0x0);

	LOK_notready = _iqk_check_cal_8822c(dm, path, 0x0);
#if 1
	if (path == RF_PATH_B)
		odm_set_rf_reg(dm, (enum rf_path)path, RF_0x00, 0xf0000, 0x1);
#endif

	_iqk_set_gnt_wl_gnt_bt_8822c(dm, false);
	if(!for_rxk)
		iqk_info->rf_reg58 = odm_get_rf_reg(dm, (enum rf_path)path, RF_0x58, 0xfffff);

	if (!LOK_notready) {
		RF_DBG(dm, DBG_RF_IQK, "[IQK]0x58 = 0x%x\n",
		       odm_get_rf_reg(dm, (enum rf_path)path, RF_0x58, 0xfffff));
		_iqk_backup_iqk_8822c(dm, 0x1, path);
	} else
		RF_DBG(dm, DBG_RF_IQK, "[IQK]==>S%d LOK Fail!!!\n", path);
	iqk_info->lok_fail[path] = LOK_notready;
	return LOK_notready;
}

boolean
_iqk_one_shot_8822c(
	void *dm_void,
	u8 path,
	u8 idx)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct dm_iqk_info *iqk_info = &dm->IQK_info;
	struct _hal_rf_ *rf = &dm->rf_table;
	boolean notready = true, fail = true, is_NB_IQK = false;
	u32 iqk_cmd = 0x0 , temp = 0x0;

	if (idx == TXIQK)
		RF_DBG(dm, DBG_RF_IQK, "[IQK]============ S%d WBTXIQK ============\n", path);
	else if (idx == RXIQK1)
		RF_DBG(dm, DBG_RF_IQK, "[IQK]============ S%d WBRXIQK STEP1============\n", path);
	else
		RF_DBG(dm, DBG_RF_IQK, "[IQK]============ S%d WBRXIQK STEP2============\n", path);

	if ((*dm->band_width == CHANNEL_WIDTH_5) ||(*dm->band_width == CHANNEL_WIDTH_10))
		is_NB_IQK = true;

	_iqk_set_gnt_wl_gnt_bt_8822c(dm, true);

	if (idx == TXIQK) {
		if (is_NB_IQK)
			temp = (0x1 << 8) | (1 << (path + 4)) | (path << 1);
		else
			temp = ((*dm->band_width + 4) << 8) | (1 << (path + 4)) | (path << 1);
		iqk_cmd = 0x8 | temp;
		RF_DBG(dm, DBG_RF_IQK, "[IQK]TXK_Trigger = 0x%x\n", iqk_cmd);
		
		if(dm->cut_version == ODM_CUT_E){
			odm_set_rf_reg(dm, (enum rf_path)path, 0x0, 0xf0000, 0x4);
			odm_set_rf_reg(dm, (enum rf_path)path, 0x8f, BIT(14), 0x1);
		}
		halrf_delay_10us(1);
		/*{0xf8000118, 0xf800012a} ==> NB TXK   (CMD = 1)*/
		/*{0xf8000418, 0xf800042a} ==> 20 WBTXK (CMD = 3)*/
		/*{0xf8000518, 0xf800052a} ==> 40 WBTXK (CMD = 4)*/
		/*{0xf8000618, 0xf800062a} ==> 80 WBTXK (CMD = 5)*/
	} else if (idx == RXIQK1) {
		if (is_NB_IQK)
			temp = (0x2 << 8) | (1 << (path + 4)) | (path << 1);
		else
			temp = ((*dm->band_width + 7) << 8) | (1 << (path + 4)) | (path << 1);
		iqk_cmd = 0x8 | temp;
		RF_DBG(dm, DBG_RF_IQK, "[IQK]RXK1_Trigger = 0x%x\n", iqk_cmd);
		
		if(dm->cut_version == ODM_CUT_E){
			odm_set_rf_reg(dm, (enum rf_path)path, 0x0, 0xf0000, 0x6);
			odm_set_rf_reg(dm, (enum rf_path)path, 0x8f, BIT(14), 0x1);
		}
		halrf_delay_10us(1);
		/*{0xf8000218, 0xf800021a} ==> NB RXK1   (CMD = 1)*/
		/*{0xf8000718, 0xf800071a} ==> 20 WBRXK1 (CMD = 7)*/
		/*{0xf8000718, 0xf800081a} ==> 40 WBRXK1 (CMD = 8)*/
		/*{0xf8000818, 0xf800091a} ==> 80 WBRXK1 (CMD = 9)*/
	} else if (idx == RXIQK2) {
		if (is_NB_IQK)			
			temp = (0x3 << 8) | (1 << (path + 4)) | (path << 1);
		else
			temp = ((*dm->band_width + 0xa) << 8) | (1 << (path + 4)) | (path << 1);
		iqk_cmd = 0x8 | temp;
		RF_DBG(dm, DBG_RF_IQK, "[IQK]RXK2_Trigger = 0x%x\n", iqk_cmd);
		
		if(dm->cut_version == ODM_CUT_E) {
			odm_set_rf_reg(dm, (enum rf_path)path, 0x0, 0xf0000, 0x7);
			odm_set_rf_reg(dm, (enum rf_path)path, 0x8f, BIT(14), 0x0);
		}
		halrf_delay_10us(1);
		/*{0xf8000318, 0xf800031a} ==> NB RXK2   (CMD = 3)*/
		/*{0xf8000918, 0xf8000a1a} ==> 20 WBRXK2 (CMD = a)*/
		/*{0xf8000a18, 0xf8000b1a} ==> 40 WBRXK2 (CMD = b)*/
		/*{0xf8000b18, 0xf8000c1a} ==> 80 WBRXK2 (CMD = c)*/
	}

	RF_DBG(dm, DBG_RF_IQK, "[IQK]0x0 =%x, 0x8f = 0x%x\n", odm_get_rf_reg(dm, path, 0x0, 0xfffff), odm_get_rf_reg(dm, path, 0x8f, 0xfffff));
	RF_DBG(dm, DBG_RF_IQK, "[IQK]0x38 =%x, 0x73 = 0x%x\n", _iqk_btc_read_indirect_reg_8822c(dm, 0x38), odm_get_bb_reg(dm, 0x70, 0xff000000));

	if (rf->rf_dbg_comp & DBG_RF_IQK) {
		if (idx != TXIQK) {
			odm_write_4byte(dm, 0x1b00, 0x8 | path << 1);
			RF_DBG(dm, DBG_RF_IQK, "[IQK]0x1bcc =0x%x\n", odm_read_1byte(dm, 0x1bcc));
		}
	}
	//_iqk_set_gnt_wl_gnt_bt_8822c(dm, true);
	odm_write_4byte(dm, 0x1b00, iqk_cmd);
	odm_write_4byte(dm, 0x1b00, iqk_cmd + 0x1);
	fail = _iqk_check_cal_8822c(dm, path, 0x1);
	
#if 1
	if (path == RF_PATH_B)
		odm_set_rf_reg(dm, (enum rf_path)path, RF_0x00, 0xf0000, 0x1);
#endif
	_iqk_set_gnt_wl_gnt_bt_8822c(dm, false);

	if (idx == TXIQK) {		
		odm_write_4byte(dm, 0x1b00, 0x8 | path << 1);
		iqk_info->iqk_fail_report[0][path][TXIQK] = fail;
		if (!fail){
			if (is_NB_IQK)
				iqk_info->nbtxk_1b38[path] = odm_read_4byte(dm, 0x1b38);				
			else
				_iqk_backup_iqk_8822c(dm, 0x2, path);
		}
	}
	if (idx == RXIQK2) {		
		odm_write_4byte(dm, 0x1b00, 0x8 | path << 1);
		temp = odm_get_rf_reg(dm,(enum rf_path)path, RF_0x0, MASK20BITS) >> 5;
		temp = temp & 0xff;
		temp = temp | (iqk_info->tmp1bcc << 8);
		iqk_info->rxiqk_agc[0][path] = (u16)temp;
		iqk_info->iqk_fail_report[0][path][RXIQK] = fail;
		if (!fail) {
			if (is_NB_IQK)
				iqk_info->nbrxk_1b3c[path] = odm_read_4byte(dm, 0x1b3c);			
			else
				_iqk_backup_iqk_8822c(dm, 0x3, path);
		}
	}
	return fail;
}

boolean
_iqk_rx_iqk_by_path_8822c(
	void *dm_void,
	u8 path)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct dm_iqk_info *iqk_info = &dm->IQK_info;
	boolean KFAIL = false, gonext;
	u32 tmp;

//RF_DBG(dm, DBG_RF_IQK, "[IQK]rx_iqk_step = 0x%x\n", iqk_info->rxiqk_step);

#if 1
	switch (iqk_info->rxiqk_step) {
	case 0: //LOK for RXK 
#if 1
		_iqk_lok_for_rxk_setting_8822c(dm, path);
		_lok_one_shot_8822c(dm, path, true);
#endif
		iqk_info->rxiqk_step++;
		break;
	case 1: /*gain search_RXK1*/
#if 0
		_iqk_rxk1_setting_8822c(dm, path);
		gonext = false;
		while (1) {
			KFAIL = _iqk_rx_iqk_gain_search_fail_8822c(dm, path, RXIQK1);
			if (KFAIL && iqk_info->gs_retry_count[0][path][0] < 2)
				iqk_info->gs_retry_count[0][path][0]++;
			else if (KFAIL) {
				iqk_info->rxiqk_fail_code[0][path] = 0;
				iqk_info->rxiqk_step = RXK_STEP_8822C;
				gonext = true;
			} else {
				iqk_info->rxiqk_step++;
				gonext = true;
			}
			if (gonext)
				break;
		}
		//halrf_iqk_xym_read(dm, path, 0x2);
#else
		iqk_info->rxiqk_step++;
#endif
		break;
	case 2: /*RXK1*/
#if 1
		_iqk_rxk1_setting_8822c(dm, path);
		gonext = false;
		while (1) {
			KFAIL = _iqk_one_shot_8822c(dm, path, RXIQK1);			
			if (KFAIL && iqk_info->retry_count[0][path][RXIQK1] < 2)
				iqk_info->retry_count[0][path][RXIQK1]++;
			else if (KFAIL) {
				iqk_info->rxiqk_fail_code[0][path] = 1;
				iqk_info->rxiqk_step = RXK_STEP_8822C;
				gonext = true;
			} else {
				iqk_info->rxiqk_step++;
				gonext = true;
			}
			if (gonext)
				break;
		}
#else
		iqk_info->rxiqk_step++;
#endif
			break;

	case 3: /*gain search_RXK2*/
#if 1
		_iqk_rxk2_setting_8822c(dm, path, true);
		iqk_info->isbnd = false;
		while (1) {
			RF_DBG(dm, DBG_RF_IQK, "[IQK]gs2_retry = %d\n", iqk_info->gs_retry_count[0][path][1]);
			KFAIL = _iqk_rx_iqk_gain_search_fail_8822c(dm, path, RXIQK2);
			if (KFAIL && (iqk_info->gs_retry_count[0][path][1] < rxiqk_gs_limit))
				iqk_info->gs_retry_count[0][path][1]++;
			else {
				iqk_info->rxiqk_step++;
				break;
			}
		}
		//halrf_iqk_xym_read(dm, path, 0x3);
#else
		iqk_info->rxiqk_step++;
#endif
		break;
	case 4: /*RXK2*/
#if 1
		_iqk_rxk2_setting_8822c(dm, path, false);
		gonext = false;
		while (1) {
			KFAIL = _iqk_one_shot_8822c(dm, path, RXIQK2);			
			if (KFAIL && iqk_info->retry_count[0][path][RXIQK2] < 2)
				iqk_info->retry_count[0][path][RXIQK2]++;
			else if (KFAIL) {
				iqk_info->rxiqk_fail_code[0][path] = 2;
				iqk_info->rxiqk_step = RXK_STEP_8822C;
				gonext = true;
			} else {
				iqk_info->rxiqk_step++;
				gonext = true;
			}
			if (gonext)
				break;
		}
#else
	iqk_info->rxiqk_step++;
#endif
		break;
	case 5: /*check RX XYM*/
#if 0
		RF_DBG(dm, DBG_RF_IQK, "[IQK] check RX XYM step =%d\n", iqk_info->rxiqk_step);
		KFAIL = _iqk_xym_read_8822c(dm, path);
		if (KFAIL)
			iqk_info->rxiqk_step = 0x0;
		else
			iqk_info->rxiqk_step++;	

		iqk_info->iqk_fail_report[0][path][RXIQK] = KFAIL;
#else
		iqk_info->rxiqk_step++;
#endif
		break;

	}
	return KFAIL;
#endif
}

void _iqk_lok_tune_8822c(void *dm_void, u8 path)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	u8 idac_bs = 0x4;

	while (1) {
		_iqk_lok_setting_8822c(dm, path, idac_bs);
		_lok_one_shot_8822c(dm, path, false);	
		//RF_DBG(dm, DBG_RF_IQK, "[IQK]ibs = %d\n", idac_bs);
		if(!_lok_check_8822c(dm, path)) {	
			if(idac_bs == 0x6)
				break;
			else
				idac_bs++;
		} else {
			break;
		}
	}
}

boolean
_lok_load_default_8822c(void *dm_void, u8 path)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct dm_iqk_info *iqk_info = &dm->IQK_info;
	struct _hal_rf_ *rf = &dm->rf_table;
	u32 temp;
	u8 idac_i, idac_q;
	u8 i;

	_iqk_cal_path_off_8822c(dm);
	odm_write_4byte(dm, 0x1b00, 0x8 | path << 1);	

	temp = odm_get_rf_reg(dm, (enum rf_path)path, RF_0x58, 0xfffff);
	//RF_DBG(dm, DBG_RF_IQK, "[IQK](1)setlut_0x58 = 0x%x\n", temp);
	idac_i = (u8)((temp & 0xfc000) >> 14);
	idac_q = (u8)((temp & 0x3f00) >> 8);

	if (!(idac_i == 0x0 || idac_i == 0x3f || idac_q == 0x0 || idac_q == 0x3f)) {		
		RF_DBG(dm, DBG_RF_IQK, "[IQK]LOK 0x58 = 0x%x\n", temp);
		return false;
	}

	idac_i = 0x20;
	idac_q = 0x20;

	odm_set_rf_reg(dm, (enum rf_path)path, 0x57, BIT(0), 0x0);	
	odm_set_rf_reg(dm, (enum rf_path)path, RF_0xef, BIT(4), 0x1);

	if (*dm->band_type == ODM_BAND_2_4G)
		odm_set_rf_reg(dm, (enum rf_path)path, RF_0x33, 0x7f, 0x0);
	else
		odm_set_rf_reg(dm, (enum rf_path)path, RF_0x33, 0x7f, 0x20);

	//_iqk_rf_direct_access_8822c(dm, (enum rf_path)path, false);

	odm_set_rf_reg(dm, (enum rf_path)path, 0x08, 0x003f0, idac_i);
	odm_set_rf_reg(dm, (enum rf_path)path, 0x08, 0xfc000, idac_q);
	
	temp = odm_get_rf_reg(dm, (enum rf_path)path, RF_0x08, 0xfffff);
	RF_DBG(dm, DBG_RF_IQK, "[IQK](2)setlut_0x08 = 0x%x\n", temp);
	
	temp = odm_get_rf_reg(dm, (enum rf_path)path, RF_0x58, 0xfffff);
	RF_DBG(dm, DBG_RF_IQK, "[IQK](2)setlut_0x58 = 0x%x\n", temp);

	//_iqk_rf_direct_access_8822c(dm, (enum rf_path)path, true);
	
	odm_set_rf_reg(dm, (enum rf_path)path, RF_0xef, BIT(4), 0x0);

	return true;

}

void _iqk_iqk_by_path_8822c(
	void *dm_void,
	boolean segment_iqk)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct dm_iqk_info *iqk_info = &dm->IQK_info;
	boolean KFAIL = true, is_NB_IQK = false;
	u8 i= 0x0, kcount_limit, path;
	u32 counter = 0x0;

	if ((*dm->band_width == CHANNEL_WIDTH_5) ||(*dm->band_width == CHANNEL_WIDTH_10))
		is_NB_IQK = true;

#if 1
	switch (iqk_info->iqk_step) {
	case 0: /*S0 RXIQK*/
#if 1
		counter = 0x0;
		while (1) {
			counter++;
			KFAIL = _iqk_rx_iqk_by_path_8822c(dm, RF_PATH_A);
			//RF_DBG(dm, DBG_RF_IQK, "[IQK]S0RXK KFail = 0x%x\n", KFAIL);
			if ((!KFAIL) && (iqk_info->rxiqk_step == RXK_STEP_8822C)){ //do lok
				iqk_info->iqk_step++;
				iqk_info->rxiqk_step = 0;
				if (KFAIL)
					RF_DBG(dm, DBG_RF_IQK, "[IQK]S0RXK fail code: %d!!!\n", iqk_info->rxiqk_fail_code[0][RF_PATH_A]);
				break;
			}

			if ((counter > 60) && (iqk_info->rxiqk_step == 0x0)) { // do lok
				iqk_info->iqk_step++;				
 				RF_DBG(dm, DBG_RF_IQK, "[IQK] counter > 10\n");
				break;
			} 
		}
		//_iqk_get_rxcfir_8822c(dm, RF_PATH_A, 0);
		//_iqk_rx_cfir_8822c(dm, RF_PATH_A);
		iqk_info->kcount++;		
#else
		iqk_info->iqk_step++;
#endif
					break;

	case 1: /*S0 LOK*/
#if 1
		_iqk_lok_tune_8822c(dm, RF_PATH_A);
		//if(_lok_load_default_8822c(dm, RF_PATH_A))
		//	RF_DBG(dm, DBG_RF_IQK, "[IQK]S1 Load LOK to default\n");

#endif
		iqk_info->iqk_step++;
		break;
	case 2: /*S0 TXIQK*/
#if 1
		_iqk_txk_setting_8822c(dm, RF_PATH_A);
		KFAIL = _iqk_one_shot_8822c(dm, RF_PATH_A, TXIQK);
		odm_set_rf_reg(dm, RF_PATH_A, RF_0xef, BIT(4), 0x0);
		iqk_info->kcount++;
		RF_DBG(dm, DBG_RF_IQK, "[IQK]S0TXK KFail = 0x%x\n", KFAIL);
		iqk_info->iqk_step++;

		//RF_DBG(dm, DBG_RF_IQK, "[CC]CFIR after S0 TXIQK\n");
		//_iqk_rx_cfir_8822c(dm, RF_PATH_A);

#else
			iqk_info->iqk_step++;
#endif
		break;
	case 3: /*S1 RXIQK*/
#if 1
		counter = 0x0;
		while (1) {
			counter++;
			KFAIL = _iqk_rx_iqk_by_path_8822c(dm, RF_PATH_B);
			//_DBG(dm, DBG_RF_IQK, "[IQK]S1RXK KFail = 0x%x\n", KFAIL);

			if ((!KFAIL) && (iqk_info->rxiqk_step == RXK_STEP_8822C)){ //do lok
				iqk_info->iqk_step++;
				iqk_info->rxiqk_step = 0;
				if (KFAIL)
					RF_DBG(dm, DBG_RF_IQK, "[IQK]S1RXK fail code: %d!!!\n", iqk_info->rxiqk_fail_code[0][RF_PATH_B]);
				break;
			}

			if ((counter > 60) && (iqk_info->rxiqk_step == 0x0)) { // do lok
				iqk_info->iqk_step++;				
 				RF_DBG(dm, DBG_RF_IQK, "[IQK] counter > 10\n");
				break;
			} 
		}
		iqk_info->kcount++;
		//RF_DBG(dm, DBG_RF_IQK, "[CC]CFIR after S1 RXIQK\n");
		//_iqk_get_rxcfir_8822c(dm, RF_PATH_B, 0);
		//_iqk_rx_cfir_8822c(dm,RF_PATH_B);
#else
		iqk_info->iqk_step++;
#endif
		break;

	case 4: /*S1 LOK*/
#if 1
		_iqk_lok_tune_8822c(dm, RF_PATH_B);
		//if(_lok_load_default_8822c(dm, RF_PATH_B))
		//	RF_DBG(dm, DBG_RF_IQK, "[IQK]S1 Load LOK to default\n");

#endif
		iqk_info->iqk_step++;
		break;
	
	case 5: /*S1 TXIQK*/
#if 1
		_iqk_txk_setting_8822c(dm, RF_PATH_B);
		KFAIL = _iqk_one_shot_8822c(dm, RF_PATH_B, TXIQK);
		odm_set_rf_reg(dm, RF_PATH_B, RF_0xef, BIT(4), 0x0);
		iqk_info->kcount++;
		RF_DBG(dm, DBG_RF_IQK, "[IQK]S1TXK KFail = 0x%x\n", KFAIL);
		if (KFAIL && iqk_info->retry_count[0][RF_PATH_B][TXIQK] < 3)
			iqk_info->retry_count[0][RF_PATH_B][TXIQK]++;
		else
			iqk_info->iqk_step++;
		//RF_DBG(dm, DBG_RF_IQK, "[CC]CFIR after S1 TXIQK\n");
		//_iqk_rx_cfir_8822c(dm, RF_PATH_B);
#else
				iqk_info->iqk_step++;
#endif
		break;
	case 6: /*IDFT*/
#if 0
		RF_DBG(dm, DBG_RF_IQK, "[CC]IDFT\n");
		_iqk_idft_8822c(dm);
		iqk_info->iqk_step++;
#else
				iqk_info->iqk_step++;
#endif
		break;
	}

	if (iqk_info->iqk_step == IQK_STEP_8822C) {
		RF_DBG(dm, DBG_RF_IQK, "[IQK]========LOK summary =========\n");
		RF_DBG(dm, DBG_RF_IQK, "[IQK]S0_LOK_fail= %d, S1_LOK_fail= %d\n",
		       iqk_info->lok_fail[RF_PATH_A],
		       iqk_info->lok_fail[RF_PATH_B]);
		RF_DBG(dm, DBG_RF_IQK, "[IQK]========IQK summary ==========\n");
		RF_DBG(dm, DBG_RF_IQK, "[IQK]S0_TXIQK_fail = %d, S1_TXIQK_fail = %d\n"
		       ,iqk_info->iqk_fail_report[0][RF_PATH_A][TXIQK],
		       iqk_info->iqk_fail_report[0][RF_PATH_B][TXIQK]);
		RF_DBG(dm, DBG_RF_IQK, "[IQK]S0_RXIQK_fail= %d, S1_RXIQK_fail= %d\n"
		       ,iqk_info->iqk_fail_report[0][RF_PATH_A][RXIQK],
		       iqk_info->iqk_fail_report[0][RF_PATH_B][RXIQK]);
		RF_DBG(dm, DBG_RF_IQK, "[IQK]S0_TK_retry = %d, S1_TXIQK_retry = %d\n"
		       ,iqk_info->retry_count[0][RF_PATH_A][TXIQK],
		       iqk_info->retry_count[0][RF_PATH_B][TXIQK]);
		RF_DBG(dm, DBG_RF_IQK, "[IQK]S0_RXK1_retry = %d, S0_RXK2_retry = %d\n"
		       ,iqk_info->retry_count[0][RF_PATH_A][RXIQK1], 
		       iqk_info->retry_count[0][RF_PATH_A][RXIQK2]);
		RF_DBG(dm, DBG_RF_IQK, "[IQK]S2_RXK1_retry = %d, S2_RXK2_retry = %d\n"
		       ,iqk_info->retry_count[0][RF_PATH_B][RXIQK1],
		       iqk_info->retry_count[0][RF_PATH_B][RXIQK2]);
		RF_DBG(dm, DBG_RF_IQK, "[IQK]S0_GS1_retry = %d, S0_GS2_retry = %d, S1_GS1_retry = %d, S1_GS2_retry = %d\n"
		       ,iqk_info->gs_retry_count[0][RF_PATH_A][0],
		       iqk_info->gs_retry_count[0][RF_PATH_A][1],
		       iqk_info->gs_retry_count[0][RF_PATH_B][0],
		       iqk_info->gs_retry_count[0][RF_PATH_B][1]);

		for (path = 0; path < SS_8822C; path++) {
			odm_set_bb_reg(dm, 0x1b00, bMaskDWord, 0x8 | path << 1);
			odm_set_bb_reg(dm, 0x1bb8, BIT(20), 0x0);
			odm_set_bb_reg(dm, 0x1bcc, MASKBYTE0, 0x0);
			if (is_NB_IQK) {
				odm_set_bb_reg(dm, R_0x1b20, BIT(26), 0x0);
				odm_set_bb_reg(dm, 0x1b38, MASKDWORD, iqk_info->nbtxk_1b38[path]);
				odm_set_bb_reg(dm, 0x1b3c, MASKDWORD, iqk_info->nbrxk_1b3c[path]);
			} else {
				odm_set_bb_reg(dm, R_0x1b20, BIT(26), 0x1);
				odm_set_bb_reg(dm, 0x1b38, MASKDWORD, 0x40000000);
				odm_set_bb_reg(dm, 0x1b3c, MASKDWORD, 0x40000000);			
			}			
			// force return to rx mode
			odm_set_rf_reg(dm, path, RF_0x0, 0xf0000, 0x3);

			RF_DBG(dm, DBG_RF_IQK, "[IQK]1b38= 0x%x, 1b3c= 0x%x\n",
			       iqk_info->nbtxk_1b38[path],
			       iqk_info->nbrxk_1b3c[path]);
		}
		

	}
#endif

}

void _iqk_dpd_in_sel_8822c(
	struct dm_struct *dm,
	u8 input)
{
	u8 path;
	/*input =1: DPD input = single tone, 0: DPD input = OFDM*/
	for (path = 0; path < SS_8822C; path++) {
		odm_write_4byte(dm, 0x1b00, IQK_CMD_8822C | (path << 1));
		/*dpd_in_sel*/
		odm_set_bb_reg(dm, 0x1bcc, BIT(13), input);
	}

}

void _iqk_start_iqk_8822c(
	struct dm_struct *dm,
	boolean segment_iqk)
{
	struct dm_iqk_info *iqk_info = &dm->IQK_info;
	u8 i = 0;
	u8 kcount_limit;
	
	if (*dm->band_width == 2)
		kcount_limit = kcount_limit_80m;
	else
		kcount_limit = kcount_limit_others;

	while (i <  100) {
		_iqk_iqk_by_path_8822c(dm, segment_iqk);
		
		if (iqk_info->iqk_step == IQK_STEP_8822C)
			break;
		if (segment_iqk && (iqk_info->kcount == kcount_limit))
			break;
		i++;
	}
}

void _iq_calibrate_8822c_init(
	struct dm_struct *dm)
{
	struct dm_iqk_info *iqk_info = &dm->IQK_info;
	u8 i, j, k, m;
	static boolean firstrun = true;

	if (firstrun) {
		firstrun = false;
		RF_DBG(dm, DBG_RF_IQK, "[IQK]=====>PHY_IQCalibrate_8822c_Init\n");

		for (i = 0; i < SS_8822C; i++) {
			for (j = 0; j < 2; j++) {
				iqk_info->lok_fail[i] = true;
				iqk_info->iqk_fail[j][i] = true;
				iqk_info->iqc_matrix[j][i] = 0x20000000;
			}
		}

		for (i = 0; i < 2; i++) {
			iqk_info->iqk_channel[i] = 0x0;

			for (j = 0; j < SS_8822C; j++) {
				iqk_info->lok_idac[i][j] = 0x0;
				iqk_info->rxiqk_agc[i][j] = 0x0;
				iqk_info->bypass_iqk[i][j] = 0x0;

				for (k = 0; k < 2; k++) {
					iqk_info->iqk_fail_report[i][j][k] = true;
					for (m = 0; m <= 16; m++) {
						iqk_info->iqk_cfir_real[i][j][k][m] = 0x0;
						iqk_info->iqk_cfir_imag[i][j][k][m] = 0x0;
					}
				}

				for (k = 0; k < 3; k++)
					iqk_info->retry_count[i][j][k] = 0x0;
			}
		}
	}

}

boolean
_iqk_rximr_rxk1_test_8822c(
	struct dm_struct *dm,
	u8 path,
	u32 tone_index)
{
	struct dm_iqk_info *iqk_info = &dm->IQK_info;
	boolean fail = true;
	u32 IQK_CMD;

	odm_write_4byte(dm, 0x1b00, 0x8 | path << 1);
	odm_write_4byte(dm, 0x1b20, (odm_read_4byte(dm, 0x1b20) & 0x000fffff) | ((tone_index & 0xfff) << 20));
	odm_write_4byte(dm, 0x1b24, (odm_read_4byte(dm, 0x1b24) & 0x000fffff) | ((tone_index & 0xfff) << 20));

	IQK_CMD = 0xf8000208 | (1 << (path + 4));
	odm_write_4byte(dm, 0x1b00, IQK_CMD);
	odm_write_4byte(dm, 0x1b00, IQK_CMD + 0x1);

	fail = _iqk_check_cal_8822c(dm, path, 0x1);
	return fail;
}

u32 _iqk_tximr_selfcheck_8822c(
	void *dm_void,
	u8 tone_index,
	u8 path)
{
	u32 tx_ini_power_H[2], tx_ini_power_L[2];
	u32 tmp1, tmp2, tmp3, tmp4, tmp5;
	u32 IQK_CMD;
	u32 tximr = 0x0;
	u8 i;

	struct dm_struct *dm = (struct dm_struct *)dm_void;
	/*backup*/
	odm_write_4byte(dm, 0x1b00, 0x8 | path << 1);
	odm_write_4byte(dm, 0x1bc8, 0x80000000);
	odm_write_4byte(dm, 0x8f8, 0x41400080);
	tmp1 = odm_read_4byte(dm, 0x1b0c);
	tmp2 = odm_read_4byte(dm, 0x1b14);
	tmp3 = odm_read_4byte(dm, 0x1b1c);
	tmp4 = odm_read_4byte(dm, 0x1b20);
	tmp5 = odm_read_4byte(dm, 0x1b24);
	/*setup*/
	odm_write_4byte(dm, 0x1b0c, 0x00003000);
	odm_write_4byte(dm, 0x1b1c, 0xA2193C32);
	odm_write_1byte(dm, 0x1b15, 0x00);
	odm_write_4byte(dm, 0x1b20, (u32)(tone_index << 20 | 0x00040008));
	odm_write_4byte(dm, 0x1b24, (u32)(tone_index << 20 | 0x00060008));
	odm_write_4byte(dm, 0x1b2c, 0x07);
	odm_write_4byte(dm, 0x1b38, 0x40000000);
	odm_write_4byte(dm, 0x1b3c, 0x40000000);
	/* ======derive pwr1========*/
	for (i = 0; i < 2; i++) {
		odm_write_4byte(dm, 0x1b00, 0x8 | path << 1);
		if (i == 0)
			odm_write_1byte(dm, 0x1bcc, 0x0f);
		else
			odm_write_1byte(dm, 0x1bcc, 0x09);
		/* One Shot*/
		IQK_CMD = 0x00000800;
		odm_write_4byte(dm, 0x1b34, IQK_CMD + 1);
		odm_write_4byte(dm, 0x1b34, IQK_CMD);
		halrf_delay_10us(100);
		odm_write_4byte(dm, 0x1bd4, 0x00040001);
		tx_ini_power_H[i] = odm_read_4byte(dm, 0x1bfc);
		odm_write_4byte(dm, 0x1bd4, 0x000C0001);
		tx_ini_power_L[i] = odm_read_4byte(dm, 0x1bfc);
	}
	/*restore*/
	odm_write_4byte(dm, 0x1b00, 0x8 | path << 1);
	odm_write_4byte(dm, 0x1b0c, tmp1);
	odm_write_4byte(dm, 0x1b14, tmp2);
	odm_write_4byte(dm, 0x1b1c, tmp3);
	odm_write_4byte(dm, 0x1b20, tmp4);
	odm_write_4byte(dm, 0x1b24, tmp5);

	if (tx_ini_power_H[1] == tx_ini_power_H[0])
		tximr = (3 * (halrf_psd_log2base(tx_ini_power_L[0] << 2) - halrf_psd_log2base(tx_ini_power_L[1]))) / 100;
	else
		tximr = 0;
	return tximr;
}

void _iqk_start_tximr_test_8822c(
	struct dm_struct *dm,
	u8 imr_limit)
{
	boolean KFAIL;
	u8 path, i, tone_index;
	u32 imr_result;

	for (path = 0; path < SS_8822C; path++) {
		_iqk_txk_setting_8822c(dm, path);
		KFAIL = _iqk_one_shot_8822c(dm, path, TXIQK);
		for (i = 0x0; i < imr_limit; i++) {
			tone_index = (u8)(0x08 | i << 4);
			imr_result = _iqk_tximr_selfcheck_8822c(dm, tone_index, path);
			RF_DBG(dm, DBG_RF_IQK, "[IQK]path=%x, toneindex = %x, TXIMR = %d\n", path, tone_index, imr_result);
		}
		RF_DBG(dm, DBG_RF_IQK, "\n");
	}
}

u32 _iqk_rximr_selfcheck_8822c(
	void *dm_void,
	u32 tone_index,
	u8 path,
	u32 tmp1b38)
{
	u32 rx_ini_power_H[2], rx_ini_power_L[2]; /*[0]: psd tone; [1]: image tone*/
	u32 tmp1, tmp2, tmp3, tmp4, tmp5;
	u32 IQK_CMD, tmp1bcc;
	u8 i, num_k1, rximr_step, count = 0x0;
	u32 rximr = 0x0;
	boolean KFAIL = true;

	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct dm_iqk_info *iqk_info = &dm->IQK_info;

	/*backup*/
	odm_write_4byte(dm, 0x1b00, 0x8 | path << 1);
	tmp1 = odm_read_4byte(dm, 0x1b0c);
	tmp2 = odm_read_4byte(dm, 0x1b14);
	tmp3 = odm_read_4byte(dm, 0x1b1c);
	tmp4 = odm_read_4byte(dm, 0x1b20);
	tmp5 = odm_read_4byte(dm, 0x1b24);

	odm_write_4byte(dm, 0x1b0c, 0x00001000);
	odm_write_1byte(dm, 0x1b15, 0x00);
	odm_write_4byte(dm, 0x1b1c, 0x82193d31);
	odm_write_4byte(dm, 0x1b20, (u32)(tone_index << 20 | 0x00040008));
	odm_write_4byte(dm, 0x1b24, (u32)(tone_index << 20 | 0x00060048));
	odm_write_4byte(dm, 0x1b2c, 0x07);
	odm_write_4byte(dm, 0x1b38, tmp1b38);
	odm_write_4byte(dm, 0x1b3c, 0x40000000);

	for (i = 0; i < 2; i++) {
		if (i == 0)
			odm_write_4byte(dm, 0x1b1c, 0x82193d31);
		else
			odm_write_4byte(dm, 0x1b1c, 0xa2193d31);
		IQK_CMD = 0x00000800;
		odm_write_4byte(dm, 0x1b34, IQK_CMD + 1);
		odm_write_4byte(dm, 0x1b34, IQK_CMD);
		halrf_delay_10us(100);
		odm_write_1byte(dm, 0x1bd6, 0xb);
		while (count < 100) {
			count++;
			if (odm_get_bb_reg(dm, R_0x1bfc, BIT(1)) == 1)
				break;
			else
				halrf_delay_10us(100);
		}
		if (1) {
			odm_write_1byte(dm, 0x1bd6, 0x5);
			rx_ini_power_H[i] = odm_read_4byte(dm, 0x1bfc);
			odm_write_1byte(dm, 0x1bd6, 0xe);
			rx_ini_power_L[i] = odm_read_4byte(dm, 0x1bfc);
		} else {
			rx_ini_power_H[i] = 0x0;
			rx_ini_power_L[i] = 0x0;
		}
	}
	/*restore*/
	odm_write_4byte(dm, 0x1b0c, tmp1);
	odm_write_4byte(dm, 0x1b14, tmp2);
	odm_write_4byte(dm, 0x1b1c, tmp3);
	odm_write_4byte(dm, 0x1b20, tmp4);
	odm_write_4byte(dm, 0x1b24, tmp5);
	for (i = 0; i < 2; i++)
		rx_ini_power_H[i] = (rx_ini_power_H[i] & 0xf8000000) >> 27;

	if (rx_ini_power_H[0] != rx_ini_power_H[1])
		switch (rx_ini_power_H[0]) {
		case 1:
			rx_ini_power_L[0] = (u32)((rx_ini_power_L[0] >> 1) | 0x80000000);
			rx_ini_power_L[1] = (u32)rx_ini_power_L[1] >> 1;
			break;
		case 2:
			rx_ini_power_L[0] = (u32)((rx_ini_power_L[0] >> 2) | 0x80000000);
			rx_ini_power_L[1] = (u32)rx_ini_power_L[1] >> 2;
			break;
		case 3:
			rx_ini_power_L[0] = (u32)((rx_ini_power_L[0] >> 2) | 0xc0000000);
			rx_ini_power_L[1] = (u32)rx_ini_power_L[1] >> 2;
			break;
		case 4:
			rx_ini_power_L[0] = (u32)((rx_ini_power_L[0] >> 3) | 0x80000000);
			rx_ini_power_L[1] = (u32)rx_ini_power_L[1] >> 3;
			break;
		case 5:
			rx_ini_power_L[0] = (u32)((rx_ini_power_L[0] >> 3) | 0xa0000000);
			rx_ini_power_L[1] = (u32)rx_ini_power_L[1] >> 3;
			break;
		case 6:
			rx_ini_power_L[0] = (u32)((rx_ini_power_L[0] >> 3) | 0xc0000000);
			rx_ini_power_L[1] = (u32)rx_ini_power_L[1] >> 3;
			break;
		case 7:
			rx_ini_power_L[0] = (u32)((rx_ini_power_L[0] >> 3) | 0xe0000000);
			rx_ini_power_L[1] = (u32)rx_ini_power_L[1] >> 3;
			break;
		default:
			break;
		}
	rximr = (u32)(3 * ((halrf_psd_log2base(rx_ini_power_L[0] / 100) - halrf_psd_log2base(rx_ini_power_L[1] / 100))) / 100);
	/*
		RF_DBG(dm, DBG_RF_IQK, "%-20s: 0x%x, 0x%x, 0x%x, 0x%x,0x%x, tone_index=%x, rximr= %d\n",
		(path == 0) ? "PATH A RXIMR ": "PATH B RXIMR",
		rx_ini_power_H[0], rx_ini_power_L[0], rx_ini_power_H[1], rx_ini_power_L[1], tmp1bcc, tone_index, rximr);
*/
	return rximr;
}

void _iqk_rximr_test_8822c(
	struct dm_struct *dm,
	u8 path,
	u8 imr_limit)
{
	struct dm_iqk_info *iqk_info = &dm->IQK_info;
	boolean kfail;
	u8 i, step, count, side;
	u32 imr_result = 0, tone_index;
	u32 temp = 0, temp1b38[2][15] = {{0}};
	char *freq[15] = {"1.25MHz", "3.75MHz", "6.25MHz", "8.75MHz", "11.25MHz",
			  "13.75MHz", "16.25MHz", "18.75MHz", "21.25MHz", "23.75MHz",
			  "26.25MHz", "28.75MHz", "31.25MHz", "33.75MHz", "36.25MHz"};

	for (step = 1; step < 5; step++) {
		count = 0;
		switch (step) {
		case 1: /*gain search_RXK1*/
			_iqk_rxk1_setting_8822c(dm, path);
			while (count < 3) {
				kfail = _iqk_rx_iqk_gain_search_fail_8822c(dm, path, RXIQK1);
				RF_DBG(dm, DBG_RF_IQK, "[IQK]path = %x, kfail = %x\n", path, kfail);
				if (kfail) {
					count++;
					if (count == 3)
						step = 5;
				} else {
					break;
				}
			}
			break;
		case 2: /*gain search_RXK2*/
			_iqk_rxk2_setting_8822c(dm, path, true);
			iqk_info->isbnd = false;
			while (count < 8) {
				kfail = _iqk_rx_iqk_gain_search_fail_8822c(dm, path, RXIQK2);
				RF_DBG(dm, DBG_RF_IQK, "[IQK]path = %x, kfail = %x\n", path, kfail);
				if (kfail) {
					count++;
					if (count == 8)
						step = 5;
				} else {
					break;
				}
			}
			break;
		case 3: /*get RXK1 IQC*/
			odm_write_4byte(dm, 0x1b00, 0x8 | path << 1);
			temp = odm_read_4byte(dm, 0x1b1c);
			for (side = 0; side < 2; side++) {
				for (i = 0; i < imr_limit; i++) {
					if (side == 0)
						tone_index = 0xff8 - (i << 4);
					else
						tone_index = 0x08 | (i << 4);
					while (count < 3) {
						_iqk_rxk1_setting_8822c(dm, path);
						kfail = _iqk_rximr_rxk1_test_8822c(dm, path, tone_index);
						RF_DBG(dm, DBG_RF_IQK, "[IQK]path = %x, kfail = %x\n", path, kfail);
						if (kfail) {
							count++;
							if (count == 3) {
								step = 5;
								temp1b38[side][i] = 0x20000000;
								RF_DBG(dm, DBG_RF_IQK, "[IQK]path = %x, toneindex = %x rxk1 fail\n", path, tone_index);
							}
						} else {
							odm_write_4byte(dm, 0x1b00, 0x8 | path << 1);
							odm_write_4byte(dm, 0x1b1c, 0xa2193c32);
							odm_write_4byte(dm, 0x1b14, 0xe5);
							odm_write_4byte(dm, 0x1b14, 0x0);
							temp1b38[side][i] = odm_read_4byte(dm, 0x1b38);
							RF_DBG(dm, DBG_RF_IQK, "[IQK]path = 0x%x, tone_idx = 0x%x, tmp1b38 = 0x%x\n", path, tone_index, temp1b38[side][i]);
							break;
						}
					}
				}
			}
			break;
		case 4: /*get RX IMR*/
			for (side = 0; side < 2; side++) {
				for (i = 0x0; i < imr_limit; i++) {
					if (side == 0)
						tone_index = 0xff8 - (i << 4);
					else
						tone_index = 0x08 | (i << 4);
					_iqk_rxk2_setting_8822c(dm, path, false);
					imr_result = _iqk_rximr_selfcheck_8822c(dm, tone_index, path, temp1b38[side][i]);
					RF_DBG(dm, DBG_RF_IQK, "[IQK]tone_idx = 0x%5x, freq = %s%10s, RXIMR = %5d dB\n", tone_index, (side == 0) ? "-" : " ", freq[i], imr_result);
				}
				odm_write_4byte(dm, 0x1b00, 0x8 | path << 1);
				odm_write_4byte(dm, 0x1b1c, temp);
				odm_write_4byte(dm, 0x1b38, 0x20000000);
			}
			break;
		}
	}
}

void _iqk_start_rximr_test_8822c(
	struct dm_struct *dm,
	u8 imr_limit)
{
	u8 path;

	for (path = 0; path < SS_8822C; path++)
		_iqk_rximr_test_8822c(dm, path, imr_limit);
}

void _iqk_start_imr_test_8822c(
	void *dm_void)
{
	u8 imr_limit;

	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct dm_iqk_info *iqk_info = &dm->IQK_info;

	if (*dm->band_width == 2)
		imr_limit = 0xf;
	else if (*dm->band_width == 1)
		imr_limit = 0x8;
	else
		imr_limit = 0x4;
	//	_iqk_start_tximr_test_8822c(dm, imr_limit);
	_iqk_start_rximr_test_8822c(dm, imr_limit);
}



void _phy_iq_calibrate_8822c(
	struct dm_struct *dm,
	boolean reset,
	boolean segment_iqk)
{
	u32 MAC_backup[MAC_REG_NUM_8822C] = {0};
	u32 BB_backup[BB_REG_NUM_8822C] = {0};
	u32 RF_backup[RF_REG_NUM_8822C][SS_8822C] = {{0}};
	u32 backup_mac_reg[MAC_REG_NUM_8822C] = {0x520, 0x1c, 0x70};
	u32 backup_bb_reg[BB_REG_NUM_8822C] = {
		0x0820, 0x0824, 0x1c38, 0x1c68,
		0x1d60, 0x180c, 0x410c, 0x1c3c,
		0x1a14, 0x1d58, 0x1d70, R_0x1864,
		R_0x4164, R_0x186c, R_0x416c, R_0x1a14,
		R_0x1e70, R_0x80c, R_0x1e7c, R_0x18a4, 
		R_0x41a4};
	u32 backup_rf_reg[RF_REG_NUM_8822C] = {0x19, 0xdf, 0x9e};
	boolean is_mp = false;
	u8 i = 0;

	struct dm_iqk_info *iqk_info = &dm->IQK_info;

	if (*dm->mp_mode)
		is_mp = true;
	else
		is_mp = false;
#if 0
	if (!is_mp)
		if (_iqk_reload_iqk_8822c(dm, reset))
			return;
#endif
	iqk_info->rf_reg18 = odm_get_rf_reg(dm, RF_PATH_A, RF_0x18, RFREGOFFSETMASK);

	RF_DBG(dm, DBG_RF_IQK, "[IQK]==========IQK strat!!!!!==========\n");
	RF_DBG(dm, DBG_RF_IQK, "[IQK]band_type = %s, band_width = %d, ExtPA2G = %d, ext_pa_5g = %d\n", (*dm->band_type == ODM_BAND_5G) ? "5G" : "2G", *dm->band_width, dm->ext_pa, dm->ext_pa_5g);
	RF_DBG(dm, DBG_RF_IQK, "[IQK]Interface = %d, Cv = %x\n", dm->support_interface, dm->cut_version);
	RF_DBG(dm, DBG_RF_IQK, "[IQK] Test V15 \n");
	iqk_info->iqk_times++;
	iqk_info->kcount = 0;
	iqk_info->iqk_step = 0;
	iqk_info->rxiqk_step = 0;
	iqk_info->tmp_gntwl = _iqk_btc_read_indirect_reg_8822c(dm, 0x38);

	_iqk_information_8822c(dm);
	_iqk_backup_iqk_8822c(dm, 0x0, 0x0);
	_iqk_backup_mac_bb_8822c(dm, MAC_backup, BB_backup, backup_mac_reg, backup_bb_reg);
	_iqk_backup_rf_8822c(dm, RF_backup, backup_rf_reg);

	while (i < 3) {
		i++;
		_iqk_macbb_8822c(dm);		
		_iqk_bb_for_dpk_setting_8822c(dm);
		_iqk_afe_setting_8822c(dm, true);
		_iqk_agc_bnd_int_8822c(dm);
		_iqk_start_iqk_8822c(dm, segment_iqk);
		_iqk_afe_setting_8822c(dm, false);
		_iqk_restore_rf_8822c(dm, backup_rf_reg, RF_backup);
		_iqk_restore_mac_bb_8822c(dm, MAC_backup, BB_backup, backup_mac_reg, backup_bb_reg);
		if (iqk_info->iqk_step == IQK_STEP_8822C)
			break;
		iqk_info->kcount = 0;
		RF_DBG(dm, DBG_RF_IQK, "[IQK]delay 50ms!!!\n");
		halrf_delay_10us(500);
	};

	_iqk_fill_iqk_report_8822c(dm, 0);
#if 0
	/*check cfir value*/
	_iqk_get_rxcfir_8822c(dm, RF_PATH_A , 1);
	_iqk_get_rxcfir_8822c(dm, RF_PATH_B , 1);
	_iqk_rx_cfir_check_8822c(dm, 1);

	_iqk_rx_cfir_8822c(dm, RF_PATH_A);
	_iqk_rx_cfir_8822c(dm, RF_PATH_B);
#endif
	RF_DBG(dm, DBG_RF_IQK, "[IQK]==========IQK end!!!!!==========\n");
}

void _check_fwiqk_done_8822c(struct dm_struct *dm)
{
	u32 counter = 0x0;
#if 1
	while (1) {
		if (odm_read_1byte(dm, 0x2d9c) == 0xaa  || counter > 300)
			break;
		counter++;
		halrf_delay_10us(100);
	};
	odm_write_1byte(dm, 0x1b10, 0x0);
	RF_DBG(dm, DBG_RF_IQK, "[IQK]counter = %d\n", counter);
#else
	ODM_delay_ms(50);
	RF_DBG(dm, DBG_RF_IQK, "[IQK] delay 50ms\n");

#endif
}


void _phy_iq_calibrate_by_fw_8822c(
	void *dm_void,
	u8 clear,
	u8 segment_iqk)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct dm_iqk_info *iqk_info = &dm->IQK_info;
	enum hal_status status = HAL_STATUS_FAILURE;

	if (*dm->mp_mode)
		clear = 0x1;
	//	else if (dm->is_linked)
	//		segment_iqk = 0x1;

	iqk_info->iqk_times++;
	status = odm_iq_calibrate_by_fw(dm, clear, segment_iqk);

	if (status == HAL_STATUS_SUCCESS)
		RF_DBG(dm, DBG_RF_IQK, "[IQK]FWIQK OK!!!\n");
	else
		RF_DBG(dm, DBG_RF_IQK, "[IQK]FWIQK fail!!!\n");
}

/*IQK_version:0x8, NCTL:0x5*/
/*1.max tx pause while IQK*/
/*2.CCK off while IQK*/
void phy_iq_calibrate_8822c(
	void *dm_void,
	boolean clear,
	boolean segment_iqk)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct _hal_rf_ *rf = &dm->rf_table;

	if (!(rf->rf_supportability & HAL_RF_IQK))
		return;

	//if (!(*dm->mp_mode))
	//	_iqk_check_coex_status(dm, true);

	dm->rf_calibrate_info.is_iqk_in_progress = true;
	/*FW IQK*/
	if (dm->fw_offload_ability & PHYDM_RF_IQK_OFFLOAD) {
		_phy_iq_calibrate_by_fw_8822c(dm, clear, (u8)(segment_iqk));
		_check_fwiqk_done_8822c(dm);
		_iqk_check_if_reload(dm);
		RF_DBG(dm, DBG_RF_IQK, "!!!!!  FW IQK   !!!!!\n");
	} else {
		_iq_calibrate_8822c_init(dm);
		_phy_iq_calibrate_8822c(dm, clear, segment_iqk);
	}
	_iqk_fail_count_8822c(dm);
#if (DM_ODM_SUPPORT_TYPE & ODM_AP)
	_iqk_iqk_fail_report_8822c(dm);
#endif
	halrf_iqk_dbg(dm);

	dm->rf_calibrate_info.is_iqk_in_progress = false;

}

void iqk_reload_iqk_8822c(void *dm_void, boolean reset)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct dm_iqk_info *iqk_info = &dm->IQK_info;

	_iqk_reload_iqk_8822c(dm, reset);

}

void iqk_get_cfir_8822c(void *dm_void, u8 idx, u8 path, boolean debug)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct dm_iqk_info *iqk_info = &dm->IQK_info;

	u8 i, ch;
	u32 tmp;
	u32 bit_mask_20_16 = BIT(20) | BIT(19) | BIT(18) | BIT(17) | BIT(16);

	if (debug)
		ch = 2;
	else
		ch = 0;

	odm_write_4byte(dm, 0x1b00, 0x8 | path << 1);
#if 0
	for (i = 0; i <  0x100/4; i++)
		RF_DBG(dm, DBG_RF_DPK, "[IQK] (1) 1b%x = 0x%x\n",
		       i*4, odm_read_4byte(dm, (0x1b00 + i*4)));
#endif
	if (idx == TX_IQK) {//TXCFIR
		odm_set_bb_reg(dm, R_0x1b20, BIT(31) | BIT(30), 0x3);
	} else {//RXCFIR
		odm_set_bb_reg(dm, R_0x1b20, BIT(31) | BIT(30), 0x1);		
	}
	odm_set_bb_reg(dm, R_0x1bd4, BIT(21), 0x1);
	odm_set_bb_reg(dm, R_0x1bd4, bit_mask_20_16, 0x10);
	for (i = 0; i <= 16; i++) {
		odm_set_bb_reg(dm, R_0x1bd8, MASKDWORD, 0xe0000001 | i << 2);
		tmp = odm_get_bb_reg(dm, R_0x1bfc, MASKDWORD);
		iqk_info->iqk_cfir_real[ch][path][idx][i] =
						(u16)((tmp & 0x0fff0000) >> 16);
		iqk_info->iqk_cfir_imag[ch][path][idx][i] = (u16)(tmp & 0x0fff);		
	}
#if 0
	for (i = 0; i <= 16; i++)
		RF_DBG(dm, DBG_RF_IQK, "[IQK](7) cfir_real[0][%d][%d][%x] = %2x\n", path, idx, i, iqk_info->iqk_cfir_real[0][path][idx][i]);		
	for (i = 0; i <= 16; i++)
		RF_DBG(dm, DBG_RF_IQK, "[IQK](7) cfir_imag[0][%d][%d][%x] = %2x\n", path, idx, i, iqk_info->iqk_cfir_imag[0][path][idx][i]); 
#endif
	odm_set_bb_reg(dm, R_0x1b20, BIT(31) | BIT(30), 0x0);
	//odm_set_bb_reg(dm, R_0x1bd8, MASKDWORD, 0x0);
}

void iqk_set_cfir_8822c(void *dm_void, u8 idx, u8 path, boolean debug)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct dm_iqk_info *iqk_info = &dm->IQK_info;

	u8 i = 0x0, ch = 0x0;
	u32 tmp1 = 0x0, tmp2 = 0x0;
	//u32 bit_mask_20_16 = BIT(20) | BIT(19) | BIT(18) | BIT(17) | BIT(16);

	if (debug)
		ch = 2;
	else
		ch = 0;

	odm_write_4byte(dm, 0x1b00, 0x8 | path << 1);

	if (idx == TX_IQK) {//TXCFIR
		odm_set_bb_reg(dm, R_0x1b20, BIT(31) | BIT(30), 0x3);		
		tmp1 = 0xc0000001;
	} else {//RXCFIR
		odm_set_bb_reg(dm, R_0x1b20, BIT(31) | BIT(30), 0x1);
		tmp1 = 0x60000001;
	}
	
	for (i = 0; i <= 16; i++) {
		tmp2 = tmp1 | iqk_info->iqk_cfir_real[ch][path][idx][i]<< 8;
		tmp2 = (tmp2 | i << 2) + 2;
		odm_set_bb_reg(dm, R_0x1bd8, MASKDWORD, tmp2);
		//RF_DBG(dm, DBG_RF_IQK, "[IQK]iqk_cfir_real = 0x%x\n", tmp2);
	}
	for (i = 0; i <= 16; i++) {
		tmp2 = tmp1 | iqk_info->iqk_cfir_imag[ch][path][idx][i]<< 8;
		tmp2 = (tmp2 | i << 2);
		odm_set_bb_reg(dm, R_0x1bd8, MASKDWORD, tmp2);		
		//RF_DBG(dm, DBG_RF_IQK, "[IQK]iqk_cfir_imag = 0x%x\n", tmp2);
	}		
		
	// end for write CFIR SRAM
	//odm_set_bb_reg(dm, R_0x1bd8, MASKDWORD, 0xe0000001);
	odm_set_bb_reg(dm, R_0x1b20, BIT(31) | BIT(30), 0x0);
	//odm_set_bb_reg(dm, R_0x1bd8, MASKDWORD, 0x0);
}


void iqk_clean_cfir_8822c(void *dm_void, u8 mode, u8 path)
{	
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct dm_iqk_info *iqk_info = &dm->IQK_info;	
	u32 bit_mask_6_2 = 0x7c;	
	u32 bit_mask_19_8 = 0xfff00;
	u8 i = 0x0;
	u32 tmp = 0x0;
	
	odm_write_4byte(dm, 0x1b00, 0x8 | path << 1);
//TX_IQK
	odm_set_bb_reg(dm, R_0x1b20, BIT(31) | BIT(30), 0x3);		
	// clear real part
	tmp = 0xc0000003;
	for (i =0x0; i<= 16; i++)		
		odm_set_bb_reg(dm, R_0x1bd8, MASKDWORD, tmp | i << 2);
	//clear img part
	tmp = 0xc0000001;
	for (i =0x0; i<= 16; i++)		
		odm_set_bb_reg(dm, R_0x1bd8, MASKDWORD, tmp | i << 2);
//RX_IQK
	odm_set_bb_reg(dm, R_0x1b20, BIT(31) | BIT(30), 0x1);
	// clear real part
	tmp = 0x60000003;
	for (i =0x0; i<= 16; i++)		
		odm_set_bb_reg(dm, R_0x1bd8, MASKDWORD, tmp | i << 2);
	//clear img part
	tmp = 0x60000001;
	for (i =0x0; i<= 16; i++)		
		odm_set_bb_reg(dm, R_0x1bd8, MASKDWORD, tmp | i << 2);

	// end for write CFIR SRAM
	odm_set_bb_reg(dm, R_0x1bd8, MASKDWORD, 0xe0000001);
		
}
void phy_get_iqk_cfir_8822c(void *dm_void, u8 idx, u8 path, boolean debug)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct dm_iqk_info *iqk_info = &dm->IQK_info;

	iqk_get_cfir_8822c(dm, idx, path, debug);
}


void phy_iqk_dbg_cfir_backup_8822c(void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct dm_iqk_info *iqk_info = &dm->IQK_info;
	u8 path, idx, i;

	RF_DBG(dm, DBG_RF_IQK, "[IQK]%-20s\n", "backup TX/RX CFIR");

	for (path = 0; path < SS_8822C; path++)
		for (idx = 0; idx < 2; idx++)
			phydm_get_iqk_cfir(dm, idx, path, true);

	for (path = 0; path < SS_8822C; path++) {
		for (idx = 0; idx < 2; idx++) {
			for (i = 0; i <= 16; i++) {
				RF_DBG(dm, DBG_RF_IQK,
				       "[IQK]%-7s %-3s CFIR_real: %-2d: 0x%x\n",
				       (path == 0) ? "PATH A" : "PATH B",
				       (idx == 0) ? "TX" : "RX", i,
				       iqk_info->iqk_cfir_real[2][path][idx][i])
				       ;
			}
			for (i = 0; i <= 16; i++) {
				RF_DBG(dm, DBG_RF_IQK,
				       "[IQK]%-7s %-3s CFIR_img:%-2d: 0x%x\n",
				       (path == 0) ? "PATH A" : "PATH B",
				       (idx == 0) ? "TX" : "RX", i,
				       iqk_info->iqk_cfir_imag[2][path][idx][i])
				       ;
			}
		}
	}
}

void phy_iqk_dbg_cfir_backup_update_8822c(void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct dm_iqk_info *iqk = &dm->IQK_info;
	u8 i, path, idx;
	u32 bmask13_12 = BIT(13) | BIT(12);
	u32 bmask20_16 = BIT(20) | BIT(19) | BIT(18) | BIT(17) | BIT(16);
	u32 data;

	if (iqk->iqk_cfir_real[2][0][0][0] == 0) {
		RF_DBG(dm, DBG_RF_IQK, "[IQK]%-20s\n", "CFIR is invalid");
		return;
	}
	for (path = 0; path < SS_8822C; path++) {
		for (idx = 0; idx < 2; idx++) {
			odm_set_bb_reg(dm, R_0x1b00, MASKDWORD, 0x8 | path << 1);
			odm_set_bb_reg(dm, R_0x1b2c, MASKDWORD, 0x7);
			odm_set_bb_reg(dm, R_0x1b38, MASKDWORD, 0x40000000);
			odm_set_bb_reg(dm, R_0x1b3c, MASKDWORD, 0x40000000);
			odm_set_bb_reg(dm, R_0x1bcc, MASKDWORD, 0x00000000);
			iqk_get_cfir_8822c(dm, idx, path, false);
		}
	}
	RF_DBG(dm, DBG_RF_IQK, "[IQK]%-20s\n", "update new CFIR");
}

void phy_iqk_dbg_cfir_reload_8822c(void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct dm_iqk_info *iqk = &dm->IQK_info;
	u8 i, path, idx;
	u32 bmask13_12 = BIT(13) | BIT(12);
	u32 bmask20_16 = BIT(20) | BIT(19) | BIT(18) | BIT(17) | BIT(16);
	u32 data;

	if (iqk->iqk_cfir_real[0][0][0][0] == 0) {
		RF_DBG(dm, DBG_RF_IQK, "[IQK]%-20s\n", "CFIR is invalid");
		return;
	}
	for (path = 0; path < SS_8822C; path++) {
		for (idx = 0; idx < 2; idx++) {
			odm_set_bb_reg(dm, R_0x1b00, MASKDWORD, 0x8 | path << 1);
			odm_set_bb_reg(dm, R_0x1b2c, MASKDWORD, 0x7);
			odm_set_bb_reg(dm, R_0x1b38, MASKDWORD, 0x40000000);
			odm_set_bb_reg(dm, R_0x1b3c, MASKDWORD, 0x40000000);
			odm_set_bb_reg(dm, R_0x1bcc, MASKDWORD, 0x00000000);			
			iqk_set_cfir_8822c(dm, idx, path, false);
		}
	}
	RF_DBG(dm, DBG_RF_IQK, "[IQK]%-20s\n", "write CFIR with default value");
}

void phy_iqk_dbg_cfir_write_8822c(void *dm_void, u8 type, u32 path, u32 idx,
			      u32 i, u32 data)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct dm_iqk_info *iqk_info = &dm->IQK_info;

	if (type == 0)
		iqk_info->iqk_cfir_real[2][path][idx][i] = (u16)data;
	else
		iqk_info->iqk_cfir_imag[2][path][idx][i] = (u16)data;
}

void phy_iqk_dbg_cfir_backup_show_8822c(void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct dm_iqk_info *iqk_info = &dm->IQK_info;
	u8 path, idx, i;

	RF_DBG(dm, DBG_RF_IQK, "[IQK]%-20s\n", "backup TX/RX CFIR");

	for (path = 0; path < SS_8822C; path++) {
		for (idx = 0; idx < 2; idx++) {
			for (i = 0; i <= 16; i++) {
				RF_DBG(dm, DBG_RF_IQK,
				       "[IQK]%-10s %-3s CFIR_real:%-2d: 0x%x\n",
				       (path == 0) ? "PATH A" : "PATH B",
				       (idx == 0) ? "TX" : "RX", i,
				       iqk_info->iqk_cfir_real[2][path][idx][i])
				       ;
			}
			for (i = 0; i <= 16; i++) {
				RF_DBG(dm, DBG_RF_IQK,
				       "[IQK]%-10s %-3s CFIR_img:%-2d: 0x%x\n",
				       (path == 0) ? "PATH A" : "PATH B",
				       (idx == 0) ? "TX" : "RX", i,
				       iqk_info->iqk_cfir_imag[2][path][idx][i])
				       ;
			}
		}
	}
}

#endif
