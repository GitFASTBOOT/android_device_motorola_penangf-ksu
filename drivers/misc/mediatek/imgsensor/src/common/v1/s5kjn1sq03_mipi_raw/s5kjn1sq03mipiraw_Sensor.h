/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

/*****************************************************************************
 *
 * Filename:
 * ---------
 *	 s5kjn1sq03mipiraw_Sensor.h
 *
 * Project:
 * --------
 *	 ALPS
 *
 * Description:
 * ------------
 *	 CMOS sensor header file
 *
 ****************************************************************************/
#ifndef _S5KJN1SQ03MIPI_SENSOR_H
#define _S5KJN1SQ03MIPI_SENSOR_H

#define S5KJN1SQ03_CUSTOM_ENABLE		1

enum IMGSENSOR_MODE {
	IMGSENSOR_MODE_INIT,
	IMGSENSOR_MODE_PREVIEW,
	IMGSENSOR_MODE_CAPTURE,
	IMGSENSOR_MODE_VIDEO,
	IMGSENSOR_MODE_HIGH_SPEED_VIDEO,
	IMGSENSOR_MODE_SLIM_VIDEO,
#if S5KJN1SQ03_CUSTOM_ENABLE
	IMGSENSOR_MODE_CUSTOM1,
	IMGSENSOR_MODE_CUSTOM2,
#endif
};

struct S5KJN1_eeprom_t {
	kal_uint8 eeprom_table_version[3];
    kal_uint8 mpn[8];
	kal_uint8 actuator_id[1];
	kal_uint8 lens_id[1];
	kal_uint8 manufacturer_id[2];
	kal_uint8 factory_id[2];
	kal_uint8 manufacture_line[1];
	kal_uint8 manufacture_date[3];
	kal_uint8 serial_number[16];
	kal_uint8 manufacture_crc16[2];

	kal_uint8 af_data[24];
	kal_uint8 af_crc16[2];

	kal_uint8 cie_src_1_ev[6];
	kal_uint8 awb_r_g_golden_min_limit[1];
	kal_uint8 awb_r_g_golden_max_limit[1];
	kal_uint8 awb_b_g_golden_min_limit[1];
	kal_uint8 awb_b_g_golden_max_limit[1];
	kal_uint8 awb_src_8401_golden_r1_golden_r[2];
	kal_uint8 awb_src_1_golden_gr[2];
	kal_uint8 awb_src_1_golden_gb[2];
	kal_uint8 awb_src_1_golden_b[2];
	kal_uint8 awb_src_1_golden_rg_ratio[2];
	kal_uint8 awb_src_1_golden_bg_ratio[2];
	kal_uint8 awb_src_1_golden_gr_gb_ratio[2];
	kal_uint8 awb_src_1_r[2];
	kal_uint8 awb_src_1_gr[2];
	kal_uint8 awb_src_1_gb[2];
	kal_uint8 awb_src_1_b[2];
	kal_uint8 awb_src_1_rg_ratio[2];
	kal_uint8 awb_src_1_bg_ratio[2];
	kal_uint8 awb_src_1_gr_gb_ratio[2];
	kal_uint8 awb_reserve[5];
	kal_uint8 awb_crc16[2];

	kal_uint8 oc_data[18];
	kal_uint8 sfr_data[119];
	kal_uint8 qcom_lsc_data[1770];
	kal_uint8 qcom_pdaf_data[999];

	kal_uint8 lsc_data_mtk[1868];
	kal_uint8 lsc_crc16_mtk[2];

	kal_uint8 pdaf_out1_data_mtk[496];
	kal_uint8 pdaf_out2_data_mtk[1004];
	kal_uint8 pdaf_out1_crc16_mtk[2];
	kal_uint8 pdaf_out2_crc16_mtk[2];
	kal_uint8 other_data[8401];
};

//AWB calibration status
 typedef enum
 {
    NO_ERRORS =0,
    CRC_FAILURE,
  } calibration_status_t;

struct imgsensor_mode_struct {
	kal_uint32 pclk;	/* record different mode's pclk */
	kal_uint32 linelength;	/* record different mode's linelength */
	kal_uint32 framelength;	/* record different mode's framelength */

	kal_uint8 startx; /* record different mode's startx of grabwindow */
	kal_uint8 starty; /* record different mode's startx of grabwindow */

	/* record different mode's width of grabwindow */
	kal_uint16 grabwindow_width;

	/* record different mode's height of grabwindow */
	kal_uint16 grabwindow_height;

	/* following for MIPIDataLowPwr2HighSpeedSettleDelayCount
	 * by different scenario
	 */
	kal_uint8 mipi_data_lp2hs_settle_dc;

	/*       following for GetDefaultFramerateByScenario()  */
	kal_uint16 max_framerate;
	kal_uint32 mipi_pixel_rate;

};

/* SENSOR PRIVATE STRUCT FOR VARIABLES*/
struct imgsensor_struct {
	kal_uint8 mirror;	/* mirrorflip information */

	kal_uint8 sensor_mode;	/* record IMGSENSOR_MODE enum value */

	kal_uint32 shutter;	/* current shutter */
	kal_uint16 gain;	/* current gain */

	kal_uint32 pclk;	/* current pclk */

	kal_uint32 frame_length;	/* current framelength */
	kal_uint32 line_length;	/* current linelength */

	/* current min	framelength to max framerate */
	kal_uint32 min_frame_length;
	kal_uint16 dummy_pixel;	/* current dummypixel */
	kal_uint16 dummy_line;	/* current dummline */

	kal_uint16 current_fps;	/* current max fps */
	kal_bool autoflicker_en; /* record autoflicker enable or disable */
	kal_bool test_pattern;	/* record test pattern mode or not */

	/* current scenario id */
	enum MSDK_SCENARIO_ID_ENUM current_scenario_id;

	/* ihdr mode 0: disable, 1: ihdr, 2:mVHDR, 9:zigzag */
	kal_uint8 ihdr_mode;

	kal_uint8 i2c_write_id;	/* record current sensor's i2c write id */
	kal_uint8 pdaf_mode;	/* added 2016.07.08 by dj */
};

/* SENSOR PRIVATE STRUCT FOR CONSTANT*/
struct imgsensor_info_struct {
	kal_uint16 sensor_id;	/* record sensor id defined in Kd_imgsensor.h */
	kal_uint32 checksum_value; /* checksum value for Camera Auto Test */

	/* preview scenario relative information */
	struct imgsensor_mode_struct pre;

	/* capture scenario relative information */
	struct imgsensor_mode_struct cap;

	/* normal video  scenario relative information */
	struct imgsensor_mode_struct normal_video;

	struct imgsensor_mode_struct hs_video;
	struct imgsensor_mode_struct slim_video;
#if S5KJN1SQ03_CUSTOM_ENABLE
	struct imgsensor_mode_struct custom1;
	struct imgsensor_mode_struct custom2;
#endif
	kal_uint8 ae_shut_delay_frame;	/* shutter delay frame for AE cycle */

	/* sensor gain delay frame for AE cycle */
	kal_uint8 ae_sensor_gain_delay_frame;

	/* isp gain delay frame for AE cycle */
	kal_uint8 ae_ispGain_delay_frame;

	kal_uint8 ihdr_support;	/* 1, support; 0,not support */
	kal_uint8 ihdr_le_firstline;	/* 1,le first ; 0, se first */
	kal_uint8 sensor_mode_num;	/* support sensor mode num */

	kal_uint8 cap_delay_frame;	/* enter capture delay frame num */
	kal_uint8 pre_delay_frame;	/* enter preview delay frame num */
	kal_uint8 video_delay_frame;	/* enter video delay frame num */
	kal_uint8 hs_video_delay_frame;
	kal_uint8 slim_video_delay_frame;
#if S5KJN1SQ03_CUSTOM_ENABLE
	kal_uint8 custom1_delay_frame;
	kal_uint8 custom2_delay_frame;
#endif
	kal_uint8  frame_time_delay_frame;

	kal_uint8 margin;	/* sensor framelength & shutter margin */
	kal_uint32 min_shutter;	/* min shutter */

	/* max framelength by sensor register's limitation */
	kal_uint32 max_frame_length;

	kal_uint8 isp_driving_current;	/* mclk driving current */
	kal_uint8 sensor_interface_type;	/* sensor_interface_type */

	kal_uint8 mipi_sensor_type;
	/* 0,MIPI_OPHY_NCSI2; 1,MIPI_OPHY_CSI2,
	 * default is NCSI2, don't modify this para
	 */

	kal_uint8 mipi_settle_delay_mode;
	/* 0, high speed signal auto detect;
	 * 1, use settle delay,unit is ns,
	 * default is auto detect, don't modify this para
	 */

	kal_uint8 sensor_output_dataformat;
	kal_uint8 mclk;	/* mclk value, suggest 24 or 26 for 24Mhz or 26Mhz */

	kal_uint8 mipi_lane_num;	/* mipi lane num */

	/* record sensor support all write id addr,
	 * only supprt 4must end with 0xff
	 */
	kal_uint32 i2c_speed;
	kal_uint8 i2c_addr_table[5];

};

/* SENSOR READ/WRITE ID */
/* #define IMGSENSOR_WRITE_ID_1 (0x6c) */
/* #define IMGSENSOR_READ_ID_1  (0x6d) */
/* #define IMGSENSOR_WRITE_ID_2 (0x20) */
/* #define IMGSENSOR_READ_ID_2  (0x21) */

extern int iReadRegI2C(u8 *a_pSendData, u16 a_sizeSendData,
				u8 *a_pRecvData, u16 a_sizeRecvData, u16 i2cId);

extern int iWriteRegI2C(u8 *a_pSendData, u16 a_sizeSendData, u16 i2cId);

extern int iReadReg(u16 a_u2Addr, u8 *a_puBuff, u16 i2cId);
extern int iWriteReg(u16 a_u2Addr, u32 a_u4Data, u32 a_u4Bytes, u16 i2cId);
extern void kdSetI2CSpeed(u16 i2cSpeed);




extern int iReadRegI2CTiming(u8 *a_pSendData, u16 a_sizeSendData,
				    u8 *a_pRecvData, u16 a_sizeRecvData,
				    u16 i2cId, u16 timing);


extern int iWriteRegI2CTiming(u8 *a_pSendData, u16 a_sizeSendData,
				     u16 i2cId, u16 timing);

extern int iBurstWriteReg_multi(u8 *pData, u32 bytes, u16 i2cId,
					u16 transfer_length, u16 timing);


/*kal_uint16 addr, BYTE *data, kal_uint32 size);*/
#endif
