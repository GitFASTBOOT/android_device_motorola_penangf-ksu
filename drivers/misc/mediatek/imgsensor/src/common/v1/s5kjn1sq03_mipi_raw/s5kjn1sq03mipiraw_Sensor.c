// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

/*****************************************************************************
 *
 * Filename:
 * ---------
 *	 s5kjn1sq03mipiraw_Sensor.c
 *
 * Project:
 * --------
 *	 ALPS
 *
 * Description:
 * ------------
 *	 Source code of Sensor driver
 *
 *
 *------------------------------------------------------------------------------
 * Upper this line, this part is controlled by CC/CQ. DO NOT MODIFY!!
 *============================================================================
 ****************************************************************************/

#define PFX "S5KJN1SQ03_camera_sensor"
#define pr_fmt(fmt) PFX "[%s] " fmt, __func__

#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/atomic.h>

#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"
#include "kd_camera_typedef.h"

#include "s5kjn1sq03mipiraw_Sensor.h"

#define MULTI_WRITE 1
#define EEPROM_READY 1
#define ENABLE_PDAF
#define LOG_INF(format, args...)    \
    pr_err(PFX "[%s] " format, __func__, ##args)

#define LOG_ERR(format, args...)    \
    pr_err(PFX "[%s] " format, __func__, ##args)

#define S5KJN1_SERIAL_NUM_SIZE 16
#define MAIN_SERIAL_NUM_DATA_PATH "/data/vendor/camera_dump/serial_number_main.bin"

#if MULTI_WRITE
#define I2C_BUFFER_LEN 1020 /*trans# max is 255, each 4 bytes*/
#else
#define I2C_BUFFER_LEN 4
#endif

/*
 * #define LOG_INF(format, args...) pr_debug(
 * PFX "[%s] " format, __func__, ##args)
 */
static DEFINE_SPINLOCK(imgsensor_drv_lock);
static bool bNeedSetNormalMode = KAL_FALSE;

static struct imgsensor_info_struct imgsensor_info = {
	.sensor_id = S5KJN1SQ03_SENSOR_ID,
	.checksum_value = 0x67b95889,

	.pre = {
		.pclk = 560000000,
		.linelength = 5888,
		.framelength = 3168,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4080,
		.grabwindow_height = 3072,
		//grabwindow_height should be 16's N times
		.mipi_data_lp2hs_settle_dc = 88,
		.max_framerate = 300,
		.mipi_pixel_rate = 513600000,
	},
	.cap = {
		.pclk = 560000000,
		.linelength = 5888,
		.framelength = 3168,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4080,
		.grabwindow_height = 3072,
		//grabwindow_height should be 16's N times
		.mipi_data_lp2hs_settle_dc = 0x22,
		.max_framerate = 300,
		.mipi_pixel_rate = 513600000,
	},
	.normal_video = {
		.pclk = 560000000,
		.linelength = 5888,
		.framelength = 3168,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4080,
		.grabwindow_height = 3072,
		//grabwindow_height should be 16's N times
		.mipi_data_lp2hs_settle_dc = 0x22,
		.max_framerate = 300,
		.mipi_pixel_rate = 513600000,
	},
	.hs_video = {
		.pclk = 560000000,
		.linelength = 4256,
		.framelength = 1174,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1920,
		.grabwindow_height = 1080,
		//grabwindow_height should be 16's N times
		.mipi_data_lp2hs_settle_dc = 0x22,
		.max_framerate = 1200,
		.mipi_pixel_rate = 640000000,
	},
	.slim_video = {
		.pclk = 560000000,
		.linelength = 5888,
		.framelength = 3168,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4080,
		.grabwindow_height = 3072,
		//grabwindow_height should be 16's N times
		.mipi_data_lp2hs_settle_dc = 0x22,
		.max_framerate = 300,
		.mipi_pixel_rate = 513600000,
	},
#if S5KJN1SQ03_CUSTOM_ENABLE
	.custom1 = {
		.pclk = 560000000,
		.linelength = 5910,
		.framelength = 3156,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4080,
		.grabwindow_height = 2296,
		//grabwindow_height should be 16's N times
		.mipi_data_lp2hs_settle_dc = 0x22,
		.max_framerate = 300,
		.mipi_pixel_rate = 504000000,
	},
	.custom2 = {
		.pclk = 560000000,
		.linelength = 8688,
		.framelength = 6400,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 8160,
		.grabwindow_height = 6144,
		//grabwindow_height should be 16's N times
		.mipi_data_lp2hs_settle_dc = 0x22,
		.mipi_pixel_rate = 556800000,
		.max_framerate = 100,
	},
#endif
	.margin = 10,		/* sensor framelength & shutter margin */
	.min_shutter = 4,	/* min shutter */

	/* max framelength by sensor register's limitation */
	.max_frame_length = 0xffff,

	/* shutter delay frame for AE cycle,
	 * 2 frame with ispGain_delay-shut_delay=2-0=2
	 */
	.ae_shut_delay_frame = 0,

	/* sensor gain delay frame for AE cycle,
	 * 2 frame with ispGain_delay-sensor_gain_delay=2-0=2
	 */
	.ae_sensor_gain_delay_frame = 0,

	.ae_ispGain_delay_frame = 2,	/* isp gain delay frame for AE cycle */
	.ihdr_support = 0,	/* 1, support; 0,not support */
	.ihdr_le_firstline = 0,	/* 1,le first ; 0, se first */
	.sensor_mode_num = 7,	/* support sensor mode num */

	.cap_delay_frame = 2,	/* enter capture delay frame num */
	.pre_delay_frame = 2,	/* enter preview delay frame num */
	.video_delay_frame = 2,	/* enter video delay frame num */
	.hs_video_delay_frame = 2,
	.slim_video_delay_frame = 2,
#if S5KJN1SQ03_CUSTOM_ENABLE
	.custom1_delay_frame = 2,
	.custom2_delay_frame = 2,
#endif
	.frame_time_delay_frame = 1,

	.isp_driving_current = ISP_DRIVING_6MA,	/* mclk driving current */

	/* sensor_interface_type */
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,

	/* 0,MIPI_OPHY_NCSI2;  1,MIPI_OPHY_CSI2 */
	.mipi_sensor_type = MIPI_OPHY_NCSI2,

	/* 0,MIPI_SETTLEDELAY_AUTO; 1,MIPI_SETTLEDELAY_MANNUAL */
	.mipi_settle_delay_mode = 0,
	//.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_Gb,
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_Gb,
	.mclk = 24,	/* mclk value, suggest 24 or 26 for 24Mhz or 26Mhz */
	.mipi_lane_num = SENSOR_MIPI_4_LANE,
	//.i2c_speed = 1000, /*support 1MHz write*/
	.i2c_speed = 1000, /*support 1MHz write*/
	/* record sensor support all write id addr,
	 * only supprt 4 must end with 0xff
	 */
	.i2c_addr_table = {0xac, 0xff},
};


static struct imgsensor_struct imgsensor = {
	.mirror = IMAGE_HV_MIRROR,	/* mirrorflip information */

	/* IMGSENSOR_MODE enum value,record current sensor mode,such as:
	 * INIT, Preview, Capture, Video,High Speed Video, Slim Video
	 */
	.sensor_mode = IMGSENSOR_MODE_INIT,

	.shutter = 0x3D0,	/* current shutter */
	.gain = 0x100,		/* current gain */
	.dummy_pixel = 0,	/* current dummypixel */
	.dummy_line = 0,	/* current dummyline */
	.current_fps = 0,	/* full size current fps : 24fps for PIP,
				 * 30fps for Normal or ZSD
				 */

	/* auto flicker enable: KAL_FALSE for disable auto flicker,
	 * KAL_TRUE for enable auto flicker
	 */
	.autoflicker_en = KAL_FALSE,

		/* test pattern mode or not.
		 * KAL_FALSE for in test pattern mode,
		 * KAL_TRUE for normal output
		 */
	.test_pattern = KAL_FALSE,

	/* current scenario id */
	.current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,

	/* sensor need support LE, SE with HDR feature */
	.ihdr_mode = KAL_FALSE,
	.i2c_write_id = 0xac,	/* record current sensor's i2c write id */

};

#ifdef ENABLE_PDAF
static struct SENSOR_VC_INFO_STRUCT SENSOR_VC_INFO[1]=
{
	{0x02, 0x0A,   0x00,   0x08, 0x40, 0x00,
	 0x00, 0x2B, 0x0FF0, 0x0C00, 0x00, 0x00, 0x0000, 0x0000,
	 0x01, 0x30, 0x027C, 0x0BF0, 0x03, 0x00, 0x0000, 0x0000
	},
};

static struct SET_PD_BLOCK_INFO_T imgsensor_pd_info = {
	.i4OffsetX = 8,
	.i4OffsetY = 8,
	.i4PitchX  = 8,
	.i4PitchY  = 8,
	.i4PairNum  =4,
	.i4SubBlkW  =8,
	.i4SubBlkH  =2,
	.i4PosL = {{11, 8},{9, 11},{13, 12},{15, 15}},
	.i4PosR = {{10, 8},{8, 11},{12, 12},{14, 15}},
	.iMirrorFlip = 0,
	.i4BlockNumX = 508,
	.i4BlockNumY = 382,
	.i4Crop = { {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}},
};
#endif

/* Sensor output window information */
#if S5KJN1SQ03_CUSTOM_ENABLE
static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[7] = {
	{ 8160,	6144,	  0,	  0,	8160,	6144,	4080,	3072,	0,	0,	4080,	3072,	0,	0,	4080,	3072}, // Preview
	{ 8160,	6144,	  0,	  0,	8160,	6144,	4080,	3072,	0,	0,	4080,	3072,	0,	0,	4080,	3072}, // capture
	{ 8160,	6144,	  0,	  0,	8160,	6144,	4080,	3072,	0,	0,	4080,	3072,	0,	0,	4080,	3072}, // video
	{ 8160,	6144,	  0,    776,	8160,	4592,	2040,	1148,	60,	34,	1920,	1080,	0,	0,	1920,	1080}, // hs
	{ 8160,	6144,	  0,	  0,	8160,	8160,	4080,	3072,	0,	0,	4080,	3072,	0,	0,	4080,	3072}, // slim
	{ 8160,	6144,	  0,    776,	8160,	4592,	4080,	2296,	0,	0,	4080,	2296,	0,	0,	4080,	2296}, // custom1
	{ 8160,	6144,	  0,	  0,	8160,	6144,	8160,	6144,	0,	0,	8160,	6144,	0,	0,	8160,	6144}, // custom2
};
#else
static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[5] = {
	{ 8160,	6144,	  0,	  0,	8160,	6144,	4080,	3072,	0,	0,	4080,	3072,	0,	0,	4080,	3072}, // Preview
	{ 8160,	6144,	  0,	  0,	8160,	6144,	4080,	3072,	0,	0,	4080,	3072,	0,	0,	4080,	3072}, // capture
	{ 8160,	6144,	  0,	  0,	8160,	6144,	4080,	3072,	0,	0,	4080,	3072,	0,	0,	4080,	3072}, // video
	{ 8160,	6144,	  0,    776,	8160,	4592,	2040,	1148,	60,	34,	1920,	1080,	0,	0,	1920,	1080}, // hs
	{ 8160,	6144,	  0,	  0,	8160,	8160,	4080,	3072,	0,	0,	4080,	3072,	0,	0,	4080,	3072}, // slim
};
#endif

static void write_cmos_sensor(kal_uint16 addr, kal_uint16 para)
{
	char pusendcmd[4] = {
		(char)(addr >> 8), (char)(addr & 0xFF),
		(char)(para >> 8), (char)(para & 0xFF) };
	iWriteRegI2C(pusendcmd, 4, imgsensor.i2c_write_id);
}

static kal_uint16 read_cmos_sensor_8(kal_uint16 addr)
{
	kal_uint16 get_byte = 0;
	char pusendcmd[2] = { (char)(addr >> 8), (char)(addr & 0xFF) };

	iReadRegI2C(pusendcmd, 2, (u8 *) &get_byte, 1, imgsensor.i2c_write_id);
	return get_byte;
}

static void write_cmos_sensor_8(kal_uint16 addr, kal_uint8 para)
{
	char pusendcmd[3] = {
		(char)(addr >> 8), (char)(addr & 0xFF), (char)(para & 0xFF) };

	iWriteRegI2C(pusendcmd, 3, imgsensor.i2c_write_id);
}

static void set_dummy(void)
{
	pr_debug("dummyline = %d, dummypixels = %d\n",
		imgsensor.dummy_line, imgsensor.dummy_pixel);

	/* return; //for test */
	write_cmos_sensor(0x0340, imgsensor.frame_length);
	write_cmos_sensor(0x0342, imgsensor.line_length);
}				/*      set_dummy  */


static void set_max_framerate(UINT16 framerate, kal_bool min_framelength_en)
{

	kal_uint32 frame_length = imgsensor.frame_length;

	//printk("---------------------------------------set_max_framerate---------------------------------------\n");

	pr_debug("framerate = %d, min framelength should enable %d\n",
		framerate, min_framelength_en);

	frame_length = imgsensor.pclk / framerate * 10 / imgsensor.line_length;
	spin_lock(&imgsensor_drv_lock);
	if (frame_length >= imgsensor.min_frame_length)
		imgsensor.frame_length = frame_length;
	else
		imgsensor.frame_length = imgsensor.min_frame_length;

	imgsensor.dummy_line =
		imgsensor.frame_length - imgsensor.min_frame_length;

	if (imgsensor.frame_length > imgsensor_info.max_frame_length) {
		imgsensor.frame_length = imgsensor_info.max_frame_length;

		imgsensor.dummy_line =
			imgsensor.frame_length - imgsensor.min_frame_length;
	}
	if (min_framelength_en)
		imgsensor.min_frame_length = imgsensor.frame_length;
	spin_unlock(&imgsensor_drv_lock);
	set_dummy();
}				/*      set_max_framerate  */

#if 0
static void write_shutter(kal_uint16 shutter)
{

	kal_uint16 realtime_fps = 0;

	spin_lock(&imgsensor_drv_lock);
	if (shutter > imgsensor.min_frame_length - imgsensor_info.margin)
		imgsensor.frame_length = shutter + imgsensor_info.margin;
	else
		imgsensor.frame_length = imgsensor.min_frame_length;
	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
		imgsensor.frame_length = imgsensor_info.max_frame_length;
	spin_unlock(&imgsensor_drv_lock);
	if (shutter < imgsensor_info.min_shutter)
		shutter = imgsensor_info.min_shutter;

	if (imgsensor.autoflicker_en) {
		realtime_fps = imgsensor.pclk
			/ imgsensor.line_length * 10 / imgsensor.frame_length;

		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
		else {
			/* Extend frame length */
			write_cmos_sensor(0x0340, imgsensor.frame_length);

		}
	} else {
		/* Extend frame length */
		write_cmos_sensor(0x0340, imgsensor.frame_length);
		pr_debug("(else)imgsensor.frame_length = %d\n",
			imgsensor.frame_length);

	}
	/* Update Shutter */
	write_cmos_sensor(0x0202, shutter);
	pr_debug("shutter =%d, framelength =%d\n",
		shutter, imgsensor.frame_length);

}				/*      write_shutter  */
#endif


/*************************************************************************
 * FUNCTION
 *	set_shutter
 *
 * DESCRIPTION
 *	This function set e-shutter of sensor to change exposure time.
 *
 * PARAMETERS
 *	iShutter : exposured lines
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static void set_shutter(kal_uint32 shutter)
{
	unsigned long flags;
	kal_uint16 realtime_fps = 0;
	kal_uint32 CintR = 0;
	kal_uint32 Time_Frame = 0;

	//printk("---------------------------------------set_shutter---------------------------------------\n");

	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

	spin_lock(&imgsensor_drv_lock);
	if (shutter > imgsensor.min_frame_length - imgsensor_info.margin) {
		imgsensor.frame_length = shutter + imgsensor_info.margin;
	} else {
		imgsensor.frame_length = imgsensor.min_frame_length;
	}
	if (imgsensor.frame_length > imgsensor_info.max_frame_length) {
		imgsensor.frame_length = imgsensor_info.max_frame_length;
	}
	spin_unlock(&imgsensor_drv_lock);
	if (shutter < imgsensor_info.min_shutter)
		shutter = imgsensor_info.min_shutter;

	if (imgsensor.autoflicker_en) {
		realtime_fps = imgsensor.pclk / imgsensor.line_length * 10 / imgsensor.frame_length;
		if (realtime_fps >= 297 && realtime_fps <= 305) {
			set_max_framerate(296, 0);
		} else if (realtime_fps >= 147 && realtime_fps <= 150) {
			set_max_framerate(146, 0);
		} else {
			// Extend frame length
			write_cmos_sensor(0x0340, imgsensor.frame_length);
		}
	} else {
		// Extend frame length
		write_cmos_sensor(0x0340, imgsensor.frame_length);
	}

	if (shutter > 0xFFF0) {

		bNeedSetNormalMode = KAL_TRUE;
		if(shutter >= 3448275){
			shutter = 3448275;
		}
		CintR = ((unsigned long long)shutter) / 128;
		Time_Frame = CintR + 0x0002;
		printk("CintR = %d\n", CintR);
		write_cmos_sensor(0x0340, Time_Frame & 0xFFFF);
		write_cmos_sensor(0x0202, CintR & 0xFFFF);
		write_cmos_sensor(0x0702, 0x0700);
		write_cmos_sensor(0x0704, 0x0700);

		printk("download long shutter setting shutter = %d\n", shutter);
	} else {
		if (bNeedSetNormalMode == KAL_TRUE) {
			bNeedSetNormalMode = KAL_FALSE;
			write_cmos_sensor(0x0702, 0x0000);
			write_cmos_sensor(0x0704, 0x0000);

			printk("return to normal shutter =%d, framelength =%d\n", shutter, imgsensor.frame_length);

		}
		write_cmos_sensor(0x0340, imgsensor.frame_length);
		write_cmos_sensor(0x0202, imgsensor.shutter);
	}
}

static void set_shutter_frame_length(kal_uint32 shutter, kal_uint32 frame_length)
{
	unsigned long flags;
	kal_uint16 realtime_fps = 0;
	kal_int32 dummy_line = 0;
	//kal_uint32 CintR = 0;
	//kal_uint32 Time_Frame = 0;

	//printk("---------------------------------------set_shutter_frame_length---------------------------------------\n");

	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

	spin_lock(&imgsensor_drv_lock);
	/* Change frame time */
	if (frame_length > 1)
		dummy_line = frame_length - imgsensor.frame_length;
	imgsensor.frame_length = imgsensor.frame_length + dummy_line;

	if (shutter > imgsensor.frame_length - imgsensor_info.margin)
		imgsensor.frame_length = shutter + imgsensor_info.margin;

	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
		imgsensor.frame_length = imgsensor_info.max_frame_length;

	spin_unlock(&imgsensor_drv_lock);
	shutter = (shutter < imgsensor_info.min_shutter) ? imgsensor_info.min_shutter : shutter;
	shutter = (shutter > (imgsensor_info.max_frame_length - imgsensor_info.margin))
		? (imgsensor_info.max_frame_length - imgsensor_info.margin) : shutter;

	if (imgsensor.autoflicker_en) {
		realtime_fps = imgsensor.pclk / imgsensor.line_length * 10 / imgsensor.frame_length;
		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
		else {
			/* Extend frame length */
			write_cmos_sensor(0x0340, imgsensor.frame_length & 0xFFFF);
		}
	} else {
		/* Extend frame length */
		write_cmos_sensor(0x0340, imgsensor.frame_length & 0xFFFF);
	}

	write_cmos_sensor(0x0202, shutter & 0xffff);
}

static kal_uint16 gain2reg(const kal_uint16 gain)
{
	kal_uint16 reg_gain = 0x0;

	reg_gain = gain / 2;
	return (kal_uint16) reg_gain;
}

/*************************************************************************
 * FUNCTION
 *	set_gain
 *
 * DESCRIPTION
 *	This function is to set global gain to sensor.
 *
 * PARAMETERS
 *	iGain : sensor global gain(base: 0x40)
 *
 * RETURNS
 *	the actually gain set to sensor.
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint16 set_gain(kal_uint16 gain)
{
	kal_uint16 reg_gain, max_gain;

	/* gain=1024;//for test */
	/* return; //for test */

	//printk("---------------------------------------set_gain---------------------------------------\n");

	switch (imgsensor.current_scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		if(imgsensor_info.pre.grabwindow_width <= 4080)
			max_gain = 64;
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		if(imgsensor_info.cap.grabwindow_width <= 4080)
			max_gain = 64;
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		if(imgsensor_info.normal_video.grabwindow_width <= 4080)
			max_gain = 64;
		break;
#if  S5KJN1SQ03_CUSTOM_ENABLE
	case MSDK_SCENARIO_ID_CUSTOM1:
		if(imgsensor_info.custom1.grabwindow_width <= 4080)
			max_gain = 64;
		break;
	case MSDK_SCENARIO_ID_CUSTOM2:
		if(imgsensor_info.custom2.grabwindow_width <= 8160)
			max_gain = 64;
		break;
#endif
	default:
		max_gain = 16;
		break;
	}

	if (gain < BASEGAIN || gain > max_gain * BASEGAIN) {
		printk("Error gain setting");

		if (gain < BASEGAIN)
			gain = BASEGAIN;
		else if (gain > max_gain * BASEGAIN)
			gain = max_gain * BASEGAIN;
	}

	reg_gain = gain2reg(gain);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.gain = reg_gain;
	spin_unlock(&imgsensor_drv_lock);
	pr_debug("gain = %d , reg_gain = 0x%x\n ", gain, reg_gain);

	write_cmos_sensor(0x0204, reg_gain);
	/* write_cmos_sensor_8(0x0204,(reg_gain>>8)); */
	/* write_cmos_sensor_8(0x0205,(reg_gain&0xff)); */

	return gain;
}				/*      set_gain  */

static void set_mirror_flip(kal_uint8 image_mirror)
{

	kal_uint8 itemp;

	printk("---------------------------------------set_mirror_flip---------------------------------------\n");

	printk("image_mirror = %d\n", image_mirror);
	itemp = read_cmos_sensor_8(0x0101);
	itemp &= ~0x03;

	switch (image_mirror) {

	case IMAGE_NORMAL:
		write_cmos_sensor_8(0x0101, itemp);
		break;

	case IMAGE_V_MIRROR:
		write_cmos_sensor_8(0x0101, itemp | 0x02);
		break;

	case IMAGE_H_MIRROR:
		write_cmos_sensor_8(0x0101, itemp | 0x01);
		break;

	case IMAGE_HV_MIRROR:
		write_cmos_sensor_8(0x0101, itemp | 0x03);
		break;
	}
}

/*************************************************************************
 * FUNCTION
 *	check_stremoff
 *
 * DESCRIPTION
 *	waiting function until sensor streaming finish.
 *
 * PARAMETERS
 *	None
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/

static void check_streamoff(void)
{
	unsigned int i = 0;
	int timeout = (10000 / imgsensor.current_fps) + 1;

	printk("---------------------------------------check_streamoff---------------------------------------\n");

	mdelay(3);
	for (i = 0; i < timeout; i++) {
		if (read_cmos_sensor_8(0x0005) != 0xFF)
			mdelay(1);
		else
			break;
	}
	printk("%s exit! %d\n", __func__, i);
}

static kal_uint32 streaming_control(kal_bool enable)
{
	printk("streaming_enable(0=Sw Standby,1=streaming): %d\n", enable);

	printk("---------------------------------------streaming_control---------------------------------------\n");

	if (enable) {
		write_cmos_sensor_8(0x0100, 0x01);
	} else {
		write_cmos_sensor_8(0x0100, 0x00);
		check_streamoff();
	}
	return ERROR_NONE;
}

static kal_uint16 table_write_cmos_sensor(kal_uint16 *para, kal_uint32 len)
{
	char puSendCmd[I2C_BUFFER_LEN];
	kal_uint32 tosend, IDX;
	kal_uint16 addr = 0, addr_last = 0, data;

	tosend = 0;
	IDX = 0;

	printk("---------------------------------------table_write_cmos_sensor---------------------------------------\n");

	while (len > IDX) {
		addr = para[IDX];
		{
			puSendCmd[tosend++] = (char)(addr >> 8);
			puSendCmd[tosend++] = (char)(addr & 0xFF);
			data = para[IDX + 1];
			puSendCmd[tosend++] = (char)(data >> 8);
			puSendCmd[tosend++] = (char)(data & 0xFF);
			IDX += 2;
			addr_last = addr;

		}

#if MULTI_WRITE
	if ((I2C_BUFFER_LEN - tosend) < 4 || IDX == len || addr != addr_last) {
		iBurstWriteReg_multi(puSendCmd, tosend,
			imgsensor.i2c_write_id, 4, imgsensor_info.i2c_speed);
			tosend = 0;
}
#else
		iWriteRegI2CTiming(puSendCmd, 4,
			imgsensor.i2c_write_id, imgsensor_info.i2c_speed);

		tosend = 0;
#endif

	}
	return 0;
}

static kal_uint16 addr_data_pair_init_jn1sq[] = {
	/*S5KJN1SQ03_EVT0_ReferenceSetfile_v0.3_20210707*/
	/*Global*/
	0x6028, 0x2400,
	0x602A, 0x1354,
	0x6F12, 0x0100,
	0x6F12, 0x7017,
	0x602A, 0x13B2,
	0x6F12, 0x0000,
	0x602A, 0x1236,
	0x6F12, 0x0000,
	0x602A, 0x1A0A,
	0x6F12, 0x4C0A,
	0x602A, 0x2210,
	0x6F12, 0x3401,
	0x602A, 0x2176,
	0x6F12, 0x6400,
	0x602A, 0x222E,
	0x6F12, 0x0001,
	0x602A, 0x06B6,
	0x6F12, 0x0A00,
	0x602A, 0x06BC,
	0x6F12, 0x1001,
	0x602A, 0x2140,
	0x6F12, 0x0101,
	0x602A, 0x1A0E,
	0x6F12, 0x9600,
	0x6028, 0x4000,
	0xF44E, 0x0011,
	0xF44C, 0x0B0B,
	0xF44A, 0x0006,
	0x0118, 0x0002,
	0x011A, 0x0001
};

static void sensor_init(void)
{
	/* initial sequence */

	printk("---------------------------------------sensor_init---------------------------------------\n");

	pr_debug("sensor_init\n");

	write_cmos_sensor(0x6028, 0x4000);
	write_cmos_sensor(0x0000, 0x0003);
	write_cmos_sensor(0x0000, 0x38E1);
	write_cmos_sensor(0x001E, 0x0007);
	write_cmos_sensor(0x6028, 0x4000);
	write_cmos_sensor(0x6010, 0x0001);
	mdelay(5);
	write_cmos_sensor(0x6226, 0x0001);
	mdelay(10);

	table_write_cmos_sensor(addr_data_pair_init_jn1sq,
		sizeof(addr_data_pair_init_jn1sq) / sizeof(kal_uint16));
}				/*      sensor_init  */

static kal_uint16 addr_data_pair_pre_jn1sq[] = {
	0x6028, 0x2400,
	0x602A, 0x1A28,
	0x6F12, 0x4C00,
	0x602A, 0x065A,
	0x6F12, 0x0000,
	0x602A, 0x139E,
	0x6F12, 0x0100,
	0x602A, 0x139C,
	0x6F12, 0x0000,
	0x602A, 0x13A0,
	0x6F12, 0x0A00,
	0x6F12, 0x0120,
	0x602A, 0x2072,
	0x6F12, 0x0000,
	0x602A, 0x1A64,
	0x6F12, 0x0301,
	0x6F12, 0xFF00,
	0x602A, 0x19E6,
	0x6F12, 0x0200,
	0x602A, 0x1A30,
	0x6F12, 0x3401,
	0x602A, 0x19FC,
	0x6F12, 0x0B00,
	0x602A, 0x19F4,
	0x6F12, 0x0606,
	0x602A, 0x19F8,
	0x6F12, 0x1010,
	0x602A, 0x1B26,
	0x6F12, 0x6F80,
	0x6F12, 0xA060,
	0x602A, 0x1A3C,
	0x6F12, 0x6207,
	0x602A, 0x1A48,
	0x6F12, 0x6207,
	0x602A, 0x1444,
	0x6F12, 0x2000,
	0x6F12, 0x2000,
	0x602A, 0x144C,
	0x6F12, 0x3F00,
	0x6F12, 0x3F00,
	0x602A, 0x7F6C,
	0x6F12, 0x0100,
	0x6F12, 0x2F00,
	0x6F12, 0xFA00,
	0x6F12, 0x2400,
	0x6F12, 0xE500,
	0x602A, 0x0650,
	0x6F12, 0x0600,
	0x602A, 0x0654,
	0x6F12, 0x0000,
	0x602A, 0x1A46,
	0x6F12, 0x8A00,
	0x602A, 0x1A52,
	0x6F12, 0xBF00,
	0x602A, 0x0674,
	0x6F12, 0x0500,
	0x6F12, 0x0500,
	0x6F12, 0x0500,
	0x6F12, 0x0500,
	0x602A, 0x0668,
	0x6F12, 0x0800,
	0x6F12, 0x0800,
	0x6F12, 0x0800,
	0x6F12, 0x0800,
	0x602A, 0x0684,
	0x6F12, 0x4001,
	0x602A, 0x0688,
	0x6F12, 0x4001,
	0x602A, 0x147C,
	0x6F12, 0x1000,
	0x602A, 0x1480,
	0x6F12, 0x1000,
	0x602A, 0x19F6,
	0x6F12, 0x0904,
	0x602A, 0x0812,
	0x6F12, 0x0000,
	0x602A, 0x1A02,
	0x6F12, 0x1800,
	0x602A, 0x2148,
	0x6F12, 0x0100,
	0x602A, 0x2042,
	0x6F12, 0x1A00,
	0x602A, 0x0874,
	0x6F12, 0x0100,
	0x602A, 0x09C0,
	0x6F12, 0x2008,
	0x602A, 0x09C4,
	0x6F12, 0x2000,
	0x602A, 0x19FE,
	0x6F12, 0x0E1C,
	0x602A, 0x4D92,
	0x6F12, 0x0100,
	0x602A, 0x84C8,
	0x6F12, 0x0100,
	0x602A, 0x4D94,
	0x6F12, 0x0005,
	0x6F12, 0x000A,
	0x6F12, 0x0010,
	0x6F12, 0x0810,
	0x6F12, 0x000A,
	0x6F12, 0x0040,
	0x6F12, 0x0810,
	0x6F12, 0x0810,
	0x6F12, 0x8002,
	0x6F12, 0xFD03,
	0x6F12, 0x0010,
	0x6F12, 0x1510,
	0x602A, 0x3570,
	0x6F12, 0x0000,
	0x602A, 0x3574,
	0x6F12, 0x1201,
	0x602A, 0x21E4,
	0x6F12, 0x0400,
	0x602A, 0x21EC,
	0x6F12, 0x1F04,
	0x602A, 0x2080,
	0x6F12, 0x0101,
	0x6F12, 0xFF00,
	0x6F12, 0x7F01,
	0x6F12, 0x0001,
	0x6F12, 0x8001,
	0x6F12, 0xD244,
	0x6F12, 0xD244,
	0x6F12, 0x14F4,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x602A, 0x20BA,
	0x6F12, 0x121C,
	0x6F12, 0x111C,
	0x6F12, 0x54F4,
	0x602A, 0x120E,
	0x6F12, 0x1000,
	0x602A, 0x212E,
	0x6F12, 0x0200,
	0x602A, 0x13AE,
	0x6F12, 0x0101,
	0x602A, 0x0718,
	0x6F12, 0x0001,
	0x602A, 0x0710,
	0x6F12, 0x0002,
	0x6F12, 0x0804,
	0x6F12, 0x0100,
	0x602A, 0x1B5C,
	0x6F12, 0x0000,
	0x602A, 0x0786,
	0x6F12, 0x7701,
	0x602A, 0x2022,
	0x6F12, 0x0500,
	0x6F12, 0x0500,
	0x602A, 0x1360,
	0x6F12, 0x0100,
	0x602A, 0x1376,
	0x6F12, 0x0100,
	0x6F12, 0x6038,
	0x6F12, 0x7038,
	0x6F12, 0x8038,
	0x602A, 0x1386,
	0x6F12, 0x0B00,
	0x602A, 0x06FA,
	0x6F12, 0x0000,
	0x602A, 0x4A94,
	0x6F12, 0x0900,
	0x6F12, 0x0000,
	0x6F12, 0x0300,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0300,
	0x6F12, 0x0000,
	0x6F12, 0x0900,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x602A, 0x0A76,
	0x6F12, 0x1000,
	0x602A, 0x0AEE,
	0x6F12, 0x1000,
	0x602A, 0x0B66,
	0x6F12, 0x1000,
	0x602A, 0x0BDE,
	0x6F12, 0x1000,
	0x602A, 0x0BE8,
	0x6F12, 0x3000,
	0x6F12, 0x3000,
	0x602A, 0x0C56,
	0x6F12, 0x1000,
	0x602A, 0x0C60,
	0x6F12, 0x3000,
	0x6F12, 0x3000,
	0x602A, 0x0CB6,
	0x6F12, 0x0100,
	0x602A, 0x0CF2,
	0x6F12, 0x0001,
	0x602A, 0x0CF0,
	0x6F12, 0x0101,
	0x602A, 0x11B8,
	0x6F12, 0x0100,
	0x602A, 0x11F6,
	0x6F12, 0x0020,
	0x602A, 0x4A74,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0xD8FF,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0xD8FF,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x602A, 0x218E,
	0x6F12, 0x0000,
	0x602A, 0x2268,
	0x6F12, 0xF279,
	0x602A, 0x5006,
	0x6F12, 0x0000,
	0x602A, 0x500E,
	0x6F12, 0x0100,
	0x602A, 0x4E70,
	0x6F12, 0x2062,
	0x6F12, 0x5501,
	0x602A, 0x06DC,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6028, 0x4000,
	0xF46A, 0xAE80,
	0x0344, 0x0000,
	0x0346, 0x0000,
	0x0348, 0x1FFF,
	0x034A, 0x181F,
	0x034C, 0x0FF0,
	0x034E, 0x0C00,
	0x0350, 0x0008,
	0x0352, 0x0008,
	0x0900, 0x0122,
	0x0380, 0x0002,
	0x0382, 0x0002,
	0x0384, 0x0002,
	0x0386, 0x0002,
	0x0110, 0x1002,
	0x0114, 0x0301,
	0x0116, 0x3000,
	0x0136, 0x1800,
	0x013E, 0x0000,
	0x0300, 0x0006,
	0x0302, 0x0001,
	0x0304, 0x0004,
	0x0306, 0x008C,
	0x0308, 0x0008,
	0x030A, 0x0001,
	0x030C, 0x0000,
	0x030E, 0x0004,
	0x0310, 0x006B,
	0x0312, 0x0000,
	0x080E, 0x0000,
	0x0340, 0x0C60,
	0x0342, 0x1700,
	0x0702, 0x0000,
	0x0202, 0x0100,
	0x0200, 0x0100,
	0x0D00, 0x0101,
	0x0D02, 0x0101,
	0x0D04, 0x0102,
	0x6226, 0x0000
};

static void preview_setting(void)
{
	/* Convert from : "S5KJN1SQ03_EVT0_ReferenceSetfile_v0.3_20210707 -- (Set)Shine"*/

	/*
	ExtClk :	24	MHz
	Vt_pix_clk :	560 	MHz
	MIPI_output_speed : 1284.0	Mbps/lane
	Crop_Width :	8192	px
	Crop_Height :	6176	px
	Output_Width :	4080	px
	Output_Height : 3072	px
	Frame rate :	30.02	fps
	Output format : Raw10	
	H-size :	5888	px
	H-blank :	1808	px
	V-size :	3168	line
	V-blank :	96	line
	V-blank time :	1.01	ms
	Tail X :	508 
	Tail Y :	3056	
	Lane :	4	lane
	First Pixel :	Gr	First
	*/

	pr_debug("preview_setting\n");

	printk("---------------------------------------preview_setting---------------------------------------\n");

	table_write_cmos_sensor(addr_data_pair_pre_jn1sq,
			sizeof(addr_data_pair_pre_jn1sq) / sizeof(kal_uint16));

}				/*      preview_setting  */

static kal_uint16 addr_data_pair_video_jn1sq[] = {
	0x6028, 0x2400,
	0x602A, 0x1A28,
	0x6F12, 0x4C00,
	0x602A, 0x065A,
	0x6F12, 0x0000,
	0x602A, 0x139E,
	0x6F12, 0x0100,
	0x602A, 0x139C,
	0x6F12, 0x0000,
	0x602A, 0x13A0,
	0x6F12, 0x0A00,
	0x6F12, 0x0120,
	0x602A, 0x2072,
	0x6F12, 0x0000,
	0x602A, 0x1A64,
	0x6F12, 0x0301,
	0x6F12, 0xFF00,
	0x602A, 0x19E6,
	0x6F12, 0x0200,
	0x602A, 0x1A30,
	0x6F12, 0x3401,
	0x602A, 0x19FC,
	0x6F12, 0x0B00,
	0x602A, 0x19F4,
	0x6F12, 0x0606,
	0x602A, 0x19F8,
	0x6F12, 0x1010,
	0x602A, 0x1B26,
	0x6F12, 0x6F80,
	0x6F12, 0xA060,
	0x602A, 0x1A3C,
	0x6F12, 0x6207,
	0x602A, 0x1A48,
	0x6F12, 0x6207,
	0x602A, 0x1444,
	0x6F12, 0x2000,
	0x6F12, 0x2000,
	0x602A, 0x144C,
	0x6F12, 0x3F00,
	0x6F12, 0x3F00,
	0x602A, 0x7F6C,
	0x6F12, 0x0100,
	0x6F12, 0x2F00,
	0x6F12, 0xFA00,
	0x6F12, 0x2400,
	0x6F12, 0xE500,
	0x602A, 0x0650,
	0x6F12, 0x0600,
	0x602A, 0x0654,
	0x6F12, 0x0000,
	0x602A, 0x1A46,
	0x6F12, 0x8A00,
	0x602A, 0x1A52,
	0x6F12, 0xBF00,
	0x602A, 0x0674,
	0x6F12, 0x0500,
	0x6F12, 0x0500,
	0x6F12, 0x0500,
	0x6F12, 0x0500,
	0x602A, 0x0668,
	0x6F12, 0x0800,
	0x6F12, 0x0800,
	0x6F12, 0x0800,
	0x6F12, 0x0800,
	0x602A, 0x0684,
	0x6F12, 0x4001,
	0x602A, 0x0688,
	0x6F12, 0x4001,
	0x602A, 0x147C,
	0x6F12, 0x1000,
	0x602A, 0x1480,
	0x6F12, 0x1000,
	0x602A, 0x19F6,
	0x6F12, 0x0904,
	0x602A, 0x0812,
	0x6F12, 0x0000,
	0x602A, 0x1A02,
	0x6F12, 0x1800,
	0x602A, 0x2148,
	0x6F12, 0x0100,
	0x602A, 0x2042,
	0x6F12, 0x1A00,
	0x602A, 0x0874,
	0x6F12, 0x0100,
	0x602A, 0x09C0,
	0x6F12, 0x2008,
	0x602A, 0x09C4,
	0x6F12, 0x2000,
	0x602A, 0x19FE,
	0x6F12, 0x0E1C,
	0x602A, 0x4D92,
	0x6F12, 0x0100,
	0x602A, 0x84C8,
	0x6F12, 0x0100,
	0x602A, 0x4D94,
	0x6F12, 0x0005,
	0x6F12, 0x000A,
	0x6F12, 0x0010,
	0x6F12, 0x0810,
	0x6F12, 0x000A,
	0x6F12, 0x0040,
	0x6F12, 0x0810,
	0x6F12, 0x0810,
	0x6F12, 0x8002,
	0x6F12, 0xFD03,
	0x6F12, 0x0010,
	0x6F12, 0x1510,
	0x602A, 0x3570,
	0x6F12, 0x0000,
	0x602A, 0x3574,
	0x6F12, 0x1201,
	0x602A, 0x21E4,
	0x6F12, 0x0400,
	0x602A, 0x21EC,
	0x6F12, 0x1F04,
	0x602A, 0x2080,
	0x6F12, 0x0101,
	0x6F12, 0xFF00,
	0x6F12, 0x7F01,
	0x6F12, 0x0001,
	0x6F12, 0x8001,
	0x6F12, 0xD244,
	0x6F12, 0xD244,
	0x6F12, 0x14F4,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x602A, 0x20BA,
	0x6F12, 0x121C,
	0x6F12, 0x111C,
	0x6F12, 0x54F4,
	0x602A, 0x120E,
	0x6F12, 0x1000,
	0x602A, 0x212E,
	0x6F12, 0x0200,
	0x602A, 0x13AE,
	0x6F12, 0x0101,
	0x602A, 0x0718,
	0x6F12, 0x0001,
	0x602A, 0x0710,
	0x6F12, 0x0002,
	0x6F12, 0x0804,
	0x6F12, 0x0100,
	0x602A, 0x1B5C,
	0x6F12, 0x0000,
	0x602A, 0x0786,
	0x6F12, 0x7701,
	0x602A, 0x2022,
	0x6F12, 0x0500,
	0x6F12, 0x0500,
	0x602A, 0x1360,
	0x6F12, 0x0100,
	0x602A, 0x1376,
	0x6F12, 0x0100,
	0x6F12, 0x6038,
	0x6F12, 0x7038,
	0x6F12, 0x8038,
	0x602A, 0x1386,
	0x6F12, 0x0B00,
	0x602A, 0x06FA,
	0x6F12, 0x0000,
	0x602A, 0x4A94,
	0x6F12, 0x0900,
	0x6F12, 0x0000,
	0x6F12, 0x0300,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0300,
	0x6F12, 0x0000,
	0x6F12, 0x0900,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x602A, 0x0A76,
	0x6F12, 0x1000,
	0x602A, 0x0AEE,
	0x6F12, 0x1000,
	0x602A, 0x0B66,
	0x6F12, 0x1000,
	0x602A, 0x0BDE,
	0x6F12, 0x1000,
	0x602A, 0x0BE8,
	0x6F12, 0x3000,
	0x6F12, 0x3000,
	0x602A, 0x0C56,
	0x6F12, 0x1000,
	0x602A, 0x0C60,
	0x6F12, 0x3000,
	0x6F12, 0x3000,
	0x602A, 0x0CB6,
	0x6F12, 0x0100,
	0x602A, 0x0CF2,
	0x6F12, 0x0001,
	0x602A, 0x0CF0,
	0x6F12, 0x0101,
	0x602A, 0x11B8,
	0x6F12, 0x0100,
	0x602A, 0x11F6,
	0x6F12, 0x0020,
	0x602A, 0x4A74,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0xD8FF,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0xD8FF,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x602A, 0x218E,
	0x6F12, 0x0000,
	0x602A, 0x2268,
	0x6F12, 0xF279,
	0x602A, 0x5006,
	0x6F12, 0x0000,
	0x602A, 0x500E,
	0x6F12, 0x0100,
	0x602A, 0x4E70,
	0x6F12, 0x2062,
	0x6F12, 0x5501,
	0x602A, 0x06DC,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6028, 0x4000,
	0xF46A, 0xAE80,
	0x0344, 0x0000,
	0x0346, 0x0000,
	0x0348, 0x1FFF,
	0x034A, 0x181F,
	0x034C, 0x0FF0,
	0x034E, 0x0C00,
	0x0350, 0x0008,
	0x0352, 0x0008,
	0x0900, 0x0122,
	0x0380, 0x0002,
	0x0382, 0x0002,
	0x0384, 0x0002,
	0x0386, 0x0002,
	0x0110, 0x1002,
	0x0114, 0x0301,
	0x0116, 0x3000,
	0x0136, 0x1800,
	0x013E, 0x0000,
	0x0300, 0x0006,
	0x0302, 0x0001,
	0x0304, 0x0004,
	0x0306, 0x008C,
	0x0308, 0x0008,
	0x030A, 0x0001,
	0x030C, 0x0000,
	0x030E, 0x0004,
	0x0310, 0x006B,
	0x0312, 0x0000,
	0x080E, 0x0000,
	0x0340, 0x0C60,
	0x0342, 0x1700,
	0x0702, 0x0000,
	0x0202, 0x0100,
	0x0200, 0x0100,
	0x0D00, 0x0101,
	0x0D02, 0x0101,
	0x0D04, 0x0102,
	0x6226, 0x0000
};

static void normal_video_setting(kal_uint16 currefps)
{
	/* Convert from : "S5KJN1SQ03_EVT0_ReferenceSetfile_v0.3_20210707 -- (Set)Shine"*/

	/*
	ExtClk :	24	MHz
	Vt_pix_clk :	560 	MHz
	MIPI_output_speed : 1284.0	Mbps/lane
	Crop_Width :	8192	px
	Crop_Height :	6176	px
	Output_Width :	4080	px
	Output_Height : 3072	px
	Frame rate :	30.02	fps
	Output format : Raw10	
	H-size :	5888	px
	H-blank :	1808	px
	V-size :	3168	line
	V-blank :	96	line
	V-blank time :	1.01	ms
	Tail X :	508
	Tail Y :	3056
	Lane :	4	lane
	First Pixel :	Gr	First
	*/

	pr_debug("normal_video_setting\n");

	printk("---------------------------------------normal_video_setting---------------------------------------\n");

	table_write_cmos_sensor(addr_data_pair_video_jn1sq,
		sizeof(addr_data_pair_video_jn1sq) / sizeof(kal_uint16));
}


static kal_uint16 addr_data_pair_hs_video_jn1sq[] = {
	0x6028, 0x2400,
	0x602A, 0x1A28,
	0x6F12, 0x4C00,
	0x602A, 0x065A,
	0x6F12, 0x0000,
	0x602A, 0x139E,
	0x6F12, 0x0300,
	0x602A, 0x139C,
	0x6F12, 0x0000,
	0x602A, 0x13A0,
	0x6F12, 0x0A00,
	0x6F12, 0x0020,
	0x602A, 0x2072,
	0x6F12, 0x0000,
	0x602A, 0x1A64,
	0x6F12, 0x0301,
	0x6F12, 0x3F00,
	0x602A, 0x19E6,
	0x6F12, 0x0201,
	0x602A, 0x1A30,
	0x6F12, 0x3401,
	0x602A, 0x19FC,
	0x6F12, 0x0B00,
	0x602A, 0x19F4,
	0x6F12, 0x0606,
	0x602A, 0x19F8,
	0x6F12, 0x1010,
	0x602A, 0x1B26,
	0x6F12, 0x6F80,
	0x6F12, 0xA020,
	0x602A, 0x1A3C,
	0x6F12, 0x5207,
	0x602A, 0x1A48,
	0x6F12, 0x5207,
	0x602A, 0x1444,
	0x6F12, 0x2100,
	0x6F12, 0x2100,
	0x602A, 0x144C,
	0x6F12, 0x4200,
	0x6F12, 0x4200,
	0x602A, 0x7F6C,
	0x6F12, 0x0100,
	0x6F12, 0x3100,
	0x6F12, 0xF700,
	0x6F12, 0x2600,
	0x6F12, 0xE100,
	0x602A, 0x0650,
	0x6F12, 0x0600,
	0x602A, 0x0654,
	0x6F12, 0x0000,
	0x602A, 0x1A46,
	0x6F12, 0x8600,
	0x602A, 0x1A52,
	0x6F12, 0xBF00,
	0x602A, 0x0674,
	0x6F12, 0x0500,
	0x6F12, 0x0500,
	0x6F12, 0x0500,
	0x6F12, 0x0500,
	0x602A, 0x0668,
	0x6F12, 0x0800,
	0x6F12, 0x0800,
	0x6F12, 0x0800,
	0x6F12, 0x0800,
	0x602A, 0x0684,
	0x6F12, 0x4001,
	0x602A, 0x0688,
	0x6F12, 0x4001,
	0x602A, 0x147C,
	0x6F12, 0x1000,
	0x602A, 0x1480,
	0x6F12, 0x1000,
	0x602A, 0x19F6,
	0x6F12, 0x0904,
	0x602A, 0x0812,
	0x6F12, 0x0000,
	0x602A, 0x1A02,
	0x6F12, 0x0800,
	0x602A, 0x2148,
	0x6F12, 0x0100,
	0x602A, 0x2042,
	0x6F12, 0x1A00,
	0x602A, 0x0874,
	0x6F12, 0x1100,
	0x602A, 0x09C0,
	0x6F12, 0x9800,
	0x602A, 0x09C4,
	0x6F12, 0x9800,
	0x602A, 0x19FE,
	0x6F12, 0x0E1C,
	0x602A, 0x4D92,
	0x6F12, 0x0100,
	0x602A, 0x84C8,
	0x6F12, 0x0100,
	0x602A, 0x4D94,
	0x6F12, 0x4001,
	0x6F12, 0x0004,
	0x6F12, 0x0010,
	0x6F12, 0x0810,
	0x6F12, 0x0004,
	0x6F12, 0x0010,
	0x6F12, 0x0810,
	0x6F12, 0x0810,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0010,
	0x6F12, 0x0010,
	0x602A, 0x3570,
	0x6F12, 0x0000,
	0x602A, 0x3574,
	0x6F12, 0x9400,
	0x602A, 0x21E4,
	0x6F12, 0x0400,
	0x602A, 0x21EC,
	0x6F12, 0x4F01,
	0x602A, 0x2080,
	0x6F12, 0x0100,
	0x6F12, 0x7F00,
	0x602A, 0x2086,
	0x6F12, 0x8000,
	0x6F12, 0x0002,
	0x6F12, 0xC244,
	0x6F12, 0xD244,
	0x6F12, 0x14F4,
	0x6F12, 0x141C,
	0x602A, 0x208E,
	0x6F12, 0x14F4,
	0x602A, 0x208A,
	0x6F12, 0xC244,
	0x6F12, 0xD244,
	0x6F12, 0x0000,
	0x602A, 0x120E,
	0x6F12, 0x1000,
	0x602A, 0x212E,
	0x6F12, 0x0A00,
	0x602A, 0x13AE,
	0x6F12, 0x0102,
	0x602A, 0x0718,
	0x6F12, 0x0005,
	0x602A, 0x0710,
	0x6F12, 0x0004,
	0x6F12, 0x0401,
	0x6F12, 0x0100,
	0x602A, 0x1B5C,
	0x6F12, 0x0300,
	0x602A, 0x0786,
	0x6F12, 0x7701,
	0x602A, 0x2022,
	0x6F12, 0x0101,
	0x6F12, 0x0101,
	0x602A, 0x1360,
	0x6F12, 0x0000,
	0x602A, 0x1376,
	0x6F12, 0x0200,
	0x6F12, 0x6038,
	0x6F12, 0x7038,
	0x6F12, 0x8038,
	0x602A, 0x1386,
	0x6F12, 0x0B00,
	0x602A, 0x06FA,
	0x6F12, 0x1000,
	0x602A, 0x4A94,
	0x6F12, 0x0C00,
	0x6F12, 0x0000,
	0x6F12, 0x0600,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0600,
	0x6F12, 0x0000,
	0x6F12, 0x0C00,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x602A, 0x0A76,
	0x6F12, 0x1000,
	0x602A, 0x0AEE,
	0x6F12, 0x1000,
	0x602A, 0x0B66,
	0x6F12, 0x1000,
	0x602A, 0x0BDE,
	0x6F12, 0x1000,
	0x602A, 0x0BE8,
	0x6F12, 0x3000,
	0x6F12, 0x3000,
	0x602A, 0x0C56,
	0x6F12, 0x1000,
	0x602A, 0x0C60,
	0x6F12, 0x3000,
	0x6F12, 0x3000,
	0x602A, 0x0CB6,
	0x6F12, 0x0000,
	0x602A, 0x0CF2,
	0x6F12, 0x0001,
	0x602A, 0x0CF0,
	0x6F12, 0x0101,
	0x602A, 0x11B8,
	0x6F12, 0x0000,
	0x602A, 0x11F6,
	0x6F12, 0x0010,
	0x602A, 0x4A74,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0xD8FF,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0xD8FF,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x602A, 0x218E,
	0x6F12, 0x0000,
	0x602A, 0x2268,
	0x6F12, 0xF279,
	0x602A, 0x5006,
	0x6F12, 0x0000,
	0x602A, 0x500E,
	0x6F12, 0x0100,
	0x602A, 0x4E70,
	0x6F12, 0x2062,
	0x6F12, 0x5501,
	0x602A, 0x06DC,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6028, 0x4000,
	0xF46A, 0xAE80,
	0x0344, 0x00F0,
	0x0346, 0x0390,
	0x0348, 0x1F0F,
	0x034A, 0x148F,
	0x034C, 0x0780,
	0x034E, 0x0438,
	0x0350, 0x0004,
	0x0352, 0x0004,
	0x0900, 0x0144,
	0x0380, 0x0002,
	0x0382, 0x0006,
	0x0384, 0x0002,
	0x0386, 0x0006,
	0x0110, 0x1002,
	0x0114, 0x0300,
	0x0116, 0x3000,
	0x0136, 0x1800,
	0x013E, 0x0000,
	0x0300, 0x0006,
	0x0302, 0x0001,
	0x0304, 0x0004,
	0x0306, 0x0096,
	0x0308, 0x0008,
	0x030A, 0x0001,
	0x030C, 0x0000,
	0x030E, 0x0003,
	0x0310, 0x0064,
	0x0312, 0x0000,
	0x080E, 0x0000,
	0x0340, 0x0496,
	0x0342, 0x10A0,
	0x0702, 0x0000,
	0x0202, 0x0100,
	0x0200, 0x0100,
	0x0D00, 0x0101,
	0x0D02, 0x0001,
	0x0D04, 0x0002,
	0x6226, 0x0000
};

static void hs_video_setting(kal_uint16 currefps)
{
	/* Convert from : "S5KJN1SQ03_EVT0_ReferenceSetfile_v0.3_20210707 -- (Set)Ocean"*/

	/*
	ExtClk :	24	MHz
	Vt_pix_clk :	600 	MHz
	MIPI_output_speed : 1600.0	Mbps/lane
	Crop_Width :	5152	px
	Crop_Height :	2912	px
	Output_Width :	1280	px
	Output_Height : 720 px
	Frame rate :	120.08	fps
	Output format : Raw10	
	H-size :	4256	px
	H-blank :	2336 px
	V-size :	2384	line
	V-blank :	94	line
	V-blank time :	5.81	ms
	Tail X :	480
	Tail Y :	536
	Lane :	4	lane
	First Pixel :	Gr	First
	*/

	pr_debug("hs_video_setting\n");

	printk("---------------------------------------hs_video_setting---------------------------------------\n");

	table_write_cmos_sensor(addr_data_pair_hs_video_jn1sq,
		sizeof(addr_data_pair_hs_video_jn1sq) / sizeof(kal_uint16));
}

static void slim_video_setting(kal_uint16 currefps)
{
	/* Convert from : "S5KJN1SQ03_EVT0_ReferenceSetfile_v0.3_20210707 -- (Set)Shine"*/

	/*
	ExtClk :	24	MHz
	Vt_pix_clk :	560 	MHz
	MIPI_output_speed : 1284.0	Mbps/lane
	Crop_Width :	8192	px
	Crop_Height :	6176	px
	Output_Width :	4080	px
	Output_Height : 3072	px
	Frame rate :	30.02	fps
	Output format : Raw10	
	H-size :	5888	px
	H-blank :	1808	px
	V-size :	3168	line
	V-blank :	96	line
	V-blank time :	1.01	ms
	Tail X :	508
	Tail Y :	3056
	Lane :	4	lane
	First Pixel :	Gr	First
	*/

	pr_debug("slim_video_setting\n");

	printk("---------------------------------------slim_video_setting---------------------------------------\n");

	table_write_cmos_sensor(addr_data_pair_video_jn1sq,
		sizeof(addr_data_pair_video_jn1sq) / sizeof(kal_uint16));
}

#if S5KJN1SQ03_CUSTOM_ENABLE
static kal_uint16 addr_data_pair_custom1[] = {
	0x6028, 0x2400,
	0x602A, 0x1A28,
	0x6F12, 0x4C00,
	0x602A, 0x065A,
	0x6F12, 0x0000,
	0x602A, 0x139E,
	0x6F12, 0x0100,
	0x602A, 0x139C,
	0x6F12, 0x0000,
	0x602A, 0x13A0,
	0x6F12, 0x0A00,
	0x6F12, 0x0120,
	0x602A, 0x2072,
	0x6F12, 0x0000,
	0x602A, 0x1A64,
	0x6F12, 0x0301,
	0x6F12, 0xFF00,
	0x602A, 0x19E6,
	0x6F12, 0x0200,
	0x602A, 0x1A30,
	0x6F12, 0x3401,
	0x602A, 0x19FC,
	0x6F12, 0x0B00,
	0x602A, 0x19F4,
	0x6F12, 0x0606,
	0x602A, 0x19F8,
	0x6F12, 0x1010,
	0x602A, 0x1B26,
	0x6F12, 0x6F80,
	0x6F12, 0xA060,
	0x602A, 0x1A3C,
	0x6F12, 0x6207,
	0x602A, 0x1A48,
	0x6F12, 0x6207,
	0x602A, 0x1444,
	0x6F12, 0x2000,
	0x6F12, 0x2000,
	0x602A, 0x144C,
	0x6F12, 0x3F00,
	0x6F12, 0x3F00,
	0x602A, 0x7F6C,
	0x6F12, 0x0100,
	0x6F12, 0x2F00,
	0x6F12, 0xFA00,
	0x6F12, 0x2400,
	0x6F12, 0xE500,
	0x602A, 0x0650,
	0x6F12, 0x0600,
	0x602A, 0x0654,
	0x6F12, 0x0000,
	0x602A, 0x1A46,
	0x6F12, 0x8A00,
	0x602A, 0x1A52,
	0x6F12, 0xBF00,
	0x602A, 0x0674,
	0x6F12, 0x0500,
	0x6F12, 0x0500,
	0x6F12, 0x0500,
	0x6F12, 0x0500,
	0x602A, 0x0668,
	0x6F12, 0x0800,
	0x6F12, 0x0800,
	0x6F12, 0x0800,
	0x6F12, 0x0800,
	0x602A, 0x0684,
	0x6F12, 0x4001,
	0x602A, 0x0688,
	0x6F12, 0x4001,
	0x602A, 0x147C,
	0x6F12, 0x1000,
	0x602A, 0x1480,
	0x6F12, 0x1000,
	0x602A, 0x19F6,
	0x6F12, 0x0904,
	0x602A, 0x0812,
	0x6F12, 0x0000,
	0x602A, 0x1A02,
	0x6F12, 0x1800,
	0x602A, 0x2148,
	0x6F12, 0x0100,
	0x602A, 0x2042,
	0x6F12, 0x1A00,
	0x602A, 0x0874,
	0x6F12, 0x0100,
	0x602A, 0x09C0,
	0x6F12, 0x2008,
	0x602A, 0x09C4,
	0x6F12, 0x2000,
	0x602A, 0x19FE,
	0x6F12, 0x0E1C,
	0x602A, 0x4D92,
	0x6F12, 0x0100,
	0x602A, 0x84C8,
	0x6F12, 0x0100,
	0x602A, 0x4D94,
	0x6F12, 0x0005,
	0x6F12, 0x000A,
	0x6F12, 0x0010,
	0x6F12, 0x0810,
	0x6F12, 0x000A,
	0x6F12, 0x0040,
	0x6F12, 0x0810,
	0x6F12, 0x0810,
	0x6F12, 0x8002,
	0x6F12, 0xFD03,
	0x6F12, 0x0010,
	0x6F12, 0x1510,
	0x602A, 0x3570,
	0x6F12, 0x0000,
	0x602A, 0x3574,
	0x6F12, 0x1201,
	0x602A, 0x21E4,
	0x6F12, 0x0400,
	0x602A, 0x21EC,
	0x6F12, 0x1F04,
	0x602A, 0x2080,
	0x6F12, 0x0101,
	0x6F12, 0xFF00,
	0x6F12, 0x7F01,
	0x6F12, 0x0001,
	0x6F12, 0x8001,
	0x6F12, 0xD244,
	0x6F12, 0xD244,
	0x6F12, 0x14F4,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x602A, 0x20BA,
	0x6F12, 0x121C,
	0x6F12, 0x111C,
	0x6F12, 0x54F4,
	0x602A, 0x120E,
	0x6F12, 0x1000,
	0x602A, 0x212E,
	0x6F12, 0x0200,
	0x602A, 0x13AE,
	0x6F12, 0x0101,
	0x602A, 0x0718,
	0x6F12, 0x0001,
	0x602A, 0x0710,
	0x6F12, 0x0002,
	0x6F12, 0x0804,
	0x6F12, 0x0100,
	0x602A, 0x1B5C,
	0x6F12, 0x0000,
	0x602A, 0x0786,
	0x6F12, 0x7701,
	0x602A, 0x2022,
	0x6F12, 0x0500,
	0x6F12, 0x0500,
	0x602A, 0x1360,
	0x6F12, 0x0100,
	0x602A, 0x1376,
	0x6F12, 0x0100,
	0x6F12, 0x6038,
	0x6F12, 0x7038,
	0x6F12, 0x8038,
	0x602A, 0x1386,
	0x6F12, 0x0B00,
	0x602A, 0x06FA,
	0x6F12, 0x0000,
	0x602A, 0x4A94,
	0x6F12, 0x0900,
	0x6F12, 0x0000,
	0x6F12, 0x0300,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0300,
	0x6F12, 0x0000,
	0x6F12, 0x0900,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x602A, 0x0A76,
	0x6F12, 0x1000,
	0x602A, 0x0AEE,
	0x6F12, 0x1000,
	0x602A, 0x0B66,
	0x6F12, 0x1000,
	0x602A, 0x0BDE,
	0x6F12, 0x1000,
	0x602A, 0x0BE8,
	0x6F12, 0x3000,
	0x6F12, 0x3000,
	0x602A, 0x0C56,
	0x6F12, 0x1000,
	0x602A, 0x0C60,
	0x6F12, 0x3000,
	0x6F12, 0x3000,
	0x602A, 0x0CB6,
	0x6F12, 0x0100,
	0x602A, 0x0CF2,
	0x6F12, 0x0001,
	0x602A, 0x0CF0,
	0x6F12, 0x0101,
	0x602A, 0x11B8,
	0x6F12, 0x0100,
	0x602A, 0x11F6,
	0x6F12, 0x0020,
	0x602A, 0x4A74,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0xD8FF,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0xD8FF,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x602A, 0x218E,
	0x6F12, 0x0000,
	0x602A, 0x2268,
	0x6F12, 0xF279,
	0x602A, 0x5006,
	0x6F12, 0x0000,
	0x602A, 0x500E,
	0x6F12, 0x0100,
	0x602A, 0x4E70,
	0x6F12, 0x2062,
	0x6F12, 0x5501,
	0x602A, 0x06DC,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6028, 0x4000,
	0xF46A, 0xAE80,
	0x0344, 0x0000,
	0x0346, 0x0308,
	0x0348, 0x1FFF,
	0x034A, 0x1517,
	0x034C, 0x0FF0,
	0x034E, 0x08F8,
	0x0350, 0x0008,
	0x0352, 0x0008,
	0x0900, 0x0122,
	0x0380, 0x0002,
	0x0382, 0x0002,
	0x0384, 0x0002,
	0x0386, 0x0002,
	0x0110, 0x1002,
	0x0114, 0x0301,
	0x0116, 0x3000,
	0x0136, 0x1800,
	0x013E, 0x0000,
	0x0300, 0x0006,
	0x0302, 0x0001,
	0x0304, 0x0004,
	0x0306, 0x008C,
	0x0308, 0x0008,
	0x030A, 0x0001,
	0x030C, 0x0000,
	0x030E, 0x0004,
	0x0310, 0x0069,
	0x0312, 0x0000,
	0x080E, 0x0000,
	0x0340, 0x0C54,
	0x0342, 0x1716,
	0x0702, 0x0000,
	0x0202, 0x0100,
	0x0200, 0x0100,
	0x0D00, 0x0101,
	0x0D02, 0x0101,
	0x0D04, 0x0102,
	0x6226, 0x0000
};

static void custom1_setting(void)
{
	/* Convert from : "S5KJN1SQ03_EVT0_ReferenceSetfile_v0.3_20210707 -- (Set)Lake"*/

	/*
	ExtClk :	24	MHz
	Vt_pix_clk :	560 	MHz
	MIPI_output_speed : 1260.0	Mbps/lane
	Crop_Width :	7328	px
	Crop_Height :	5504	px
	Output_Width :	4080	px
	Output_Height : 2296	px
	Frame rate :	30.02	fps
	Output format : Raw10	
	H-size :	5910	px
	H-blank :	1830	px
	V-size :	3156	line
	V-blank :	860 line
	V-blank time :	9.08ms
	Tail X :	508
	Tail Y :	2228
	Lane :	4	lane
	First Pixel :	Gr	First
	*/

	printk("---------------------------------------custom1_setting---------------------------------------\n");

	pr_debug("custom1_setting\n");
	table_write_cmos_sensor(addr_data_pair_custom1,
				sizeof(addr_data_pair_custom1) / sizeof(kal_uint16));
}

static kal_uint16 addr_data_pair_custom2[] = {
	0x6028, 0x2400,
	0x602A, 0x1A28,
	0x6F12, 0x4C00,
	0x602A, 0x065A,
	0x6F12, 0x0000,
	0x602A, 0x139E,
	0x6F12, 0x0400,
	0x602A, 0x139C,
	0x6F12, 0x0100,
	0x602A, 0x13A0,
	0x6F12, 0x0500,
	0x6F12, 0x0120,
	0x602A, 0x2072,
	0x6F12, 0x0101,
	0x602A, 0x1A64,
	0x6F12, 0x0001,
	0x6F12, 0x0000,
	0x602A, 0x19E6,
	0x6F12, 0x0200,
	0x602A, 0x1A30,
	0x6F12, 0x3403,
	0x602A, 0x19FC,
	0x6F12, 0x0700,
	0x602A, 0x19F4,
	0x6F12, 0x0707,
	0x602A, 0x19F8,
	0x6F12, 0x0B0B,
	0x602A, 0x1B26,
	0x6F12, 0x6F80,
	0x6F12, 0xA060,
	0x602A, 0x1A3C,
	0x6F12, 0x8207,
	0x602A, 0x1A48,
	0x6F12, 0x8207,
	0x602A, 0x1444,
	0x6F12, 0x2000,
	0x6F12, 0x2000,
	0x602A, 0x144C,
	0x6F12, 0x3F00,
	0x6F12, 0x3F00,
	0x602A, 0x7F6C,
	0x6F12, 0x0100,
	0x6F12, 0x2F00,
	0x6F12, 0xFA00,
	0x6F12, 0x2400,
	0x6F12, 0xE500,
	0x602A, 0x0650,
	0x6F12, 0x0600,
	0x602A, 0x0654,
	0x6F12, 0x0000,
	0x602A, 0x1A46,
	0x6F12, 0x8500,
	0x602A, 0x1A52,
	0x6F12, 0x9800,
	0x602A, 0x0674,
	0x6F12, 0x0500,
	0x6F12, 0x0500,
	0x6F12, 0x0500,
	0x6F12, 0x0500,
	0x602A, 0x0668,
	0x6F12, 0x0800,
	0x6F12, 0x0800,
	0x6F12, 0x0800,
	0x6F12, 0x0800,
	0x602A, 0x0684,
	0x6F12, 0x4001,
	0x602A, 0x0688,
	0x6F12, 0x4001,
	0x602A, 0x147C,
	0x6F12, 0x0400,
	0x602A, 0x1480,
	0x6F12, 0x0400,
	0x602A, 0x19F6,
	0x6F12, 0x0404,
	0x602A, 0x0812,
	0x6F12, 0x0000,
	0x602A, 0x1A02,
	0x6F12, 0x1800,
	0x602A, 0x2148,
	0x6F12, 0x0100,
	0x602A, 0x2042,
	0x6F12, 0x1A00,
	0x602A, 0x0874,
	0x6F12, 0x0106,
	0x602A, 0x09C0,
	0x6F12, 0x4000,
	0x602A, 0x09C4,
	0x6F12, 0x4000,
	0x602A, 0x19FE,
	0x6F12, 0x0C1C,
	0x602A, 0x4D92,
	0x6F12, 0x0000,
	0x602A, 0x84C8,
	0x6F12, 0x0000,
	0x602A, 0x4D94,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x602A, 0x3570,
	0x6F12, 0x0000,
	0x602A, 0x3574,
	0x6F12, 0x7306,
	0x602A, 0x21E4,
	0x6F12, 0x0400,
	0x602A, 0x21EC,
	0x6F12, 0x6902,
	0x602A, 0x2080,
	0x6F12, 0x0100,
	0x6F12, 0xFF00,
	0x6F12, 0x0002,
	0x6F12, 0x0001,
	0x6F12, 0x0002,
	0x6F12, 0xD244,
	0x6F12, 0xD244,
	0x6F12, 0x14F4,
	0x6F12, 0x101C,
	0x6F12, 0x0D1C,
	0x6F12, 0x54F4,
	0x602A, 0x20BA,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x602A, 0x120E,
	0x6F12, 0x1000,
	0x602A, 0x212E,
	0x6F12, 0x0200,
	0x602A, 0x13AE,
	0x6F12, 0x0100,
	0x602A, 0x0718,
	0x6F12, 0x0000,
	0x602A, 0x0710,
	0x6F12, 0x0010,
	0x6F12, 0x0201,
	0x6F12, 0x0800,
	0x602A, 0x1B5C,
	0x6F12, 0x0000,
	0x602A, 0x0786,
	0x6F12, 0x1401,
	0x602A, 0x2022,
	0x6F12, 0x0500,
	0x6F12, 0x0500,
	0x602A, 0x1360,
	0x6F12, 0x0000,
	0x602A, 0x1376,
	0x6F12, 0x0000,
	0x6F12, 0x6038,
	0x6F12, 0x7038,
	0x6F12, 0x8038,
	0x602A, 0x1386,
	0x6F12, 0x0B00,
	0x602A, 0x06FA,
	0x6F12, 0x1000,
	0x602A, 0x4A94,
	0x6F12, 0x0400,
	0x6F12, 0x0400,
	0x6F12, 0x0400,
	0x6F12, 0x0400,
	0x6F12, 0x0800,
	0x6F12, 0x0800,
	0x6F12, 0x0800,
	0x6F12, 0x0800,
	0x6F12, 0x0400,
	0x6F12, 0x0400,
	0x6F12, 0x0400,
	0x6F12, 0x0400,
	0x6F12, 0x0800,
	0x6F12, 0x0800,
	0x6F12, 0x0800,
	0x6F12, 0x0800,
	0x602A, 0x0A76,
	0x6F12, 0x1000,
	0x602A, 0x0AEE,
	0x6F12, 0x1000,
	0x602A, 0x0B66,
	0x6F12, 0x1000,
	0x602A, 0x0BDE,
	0x6F12, 0x1000,
	0x602A, 0x0BE8,
	0x6F12, 0x5000,
	0x6F12, 0x5000,
	0x602A, 0x0C56,
	0x6F12, 0x1000,
	0x602A, 0x0C60,
	0x6F12, 0x5000,
	0x6F12, 0x5000,
	0x602A, 0x0CB6,
	0x6F12, 0x0000,
	0x602A, 0x0CF2,
	0x6F12, 0x0001,
	0x602A, 0x0CF0,
	0x6F12, 0x0101,
	0x602A, 0x11B8,
	0x6F12, 0x0000,
	0x602A, 0x11F6,
	0x6F12, 0x0010,
	0x602A, 0x4A74,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x602A, 0x218E,
	0x6F12, 0x0000,
	0x602A, 0x2268,
	0x6F12, 0xF279,
	0x602A, 0x5006,
	0x6F12, 0x0000,
	0x602A, 0x500E,
	0x6F12, 0x0100,
	0x602A, 0x4E70,
	0x6F12, 0x2062,
	0x6F12, 0x5501,
	0x602A, 0x06DC,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6028, 0x4000,
	0xF46A, 0xAE80,
	0x0344, 0x0000,
	0x0346, 0x0000,
	0x0348, 0x1FFF,
	0x034A, 0x181F,
	0x034C, 0x1FE0,
	0x034E, 0x1800,
	0x0350, 0x0010,
	0x0352, 0x0010,
	0x0900, 0x0111,
	0x0380, 0x0001,
	0x0382, 0x0001,
	0x0384, 0x0001,
	0x0386, 0x0001,
	0x0110, 0x1002,
	0x0114, 0x0300,
	0x0116, 0x3000,
	0x0136, 0x1800,
	0x013E, 0x0000,
	0x0300, 0x0006,
	0x0302, 0x0001,
	0x0304, 0x0004,
	0x0306, 0x008C,
	0x0308, 0x0008,
	0x030A, 0x0001,
	0x030C, 0x0000,
	0x030E, 0x0004,
	0x0310, 0x0074,
	0x0312, 0x0000,
	0x080E, 0x0000,
	0x0340, 0x1900,
	0x0342, 0x21F0,
	0x0702, 0x0000,
	0x0202, 0x0100,
	0x0200, 0x0100,
	0x0D00, 0x0100,
	0x0D02, 0x0001,
	0x0D04, 0x0002,
	0x6226, 0x0000
};

static void custom2_setting(void)
{
	/* Convert from : "S5KJN1SQ03_EVT0_ReferenceSetfile_v0.3_20210707 -- (Set)Shine"*/

	/*
	ExtClk :	24	MHz
	Vt_pix_clk :	560 	MHz
	MIPI_output_speed : 1236.0	Mbps/lane
	Crop_Width :	8192	px
	Crop_Height :	6176	px
	Output_Width :	8192	px
	Output_Height : 6144	px
	Frame rate :	10.07	fps
	Output format : Raw10	
	H-size :	8688	px
	H-blank :	528	px
	V-size :	6400	line
	V-blank :	256	line
	V-blank time :	3.97	ms
	Tail X :	0
	Tail Y :	0
	Lane :	4	lane
	First Pixel :	Gr	First
	*/
	
	printk("---------------------------------------custom2_setting---------------------------------------\n");

	pr_debug("custom2_setting\n");
	table_write_cmos_sensor(addr_data_pair_custom2,
				sizeof(addr_data_pair_custom2) / sizeof(kal_uint16));
}
#endif

#if EEPROM_READY //cfp-210812

#define S5KJN1_EEPROM_SLAVE_ADDR 0xA0
#define S5KJN1_SENSOR_IIC_SLAVE_ADDR 0xAC
#define S5KJN1_EEPROM_SIZE  0x39c7
#define EEPROM_DATA_PATH "/data/vendor/camera_dump/s5kjn1_eeprom_data.bin"
#define S5KJN1_EEPROM_CRC_AF_CAL_SIZE 24
#define S5KJN1_EEPROM_CRC_AWB_CAL_SIZE 43
#define S5KJN1_EEPROM_CRC_LSC_SIZE 1868
#define S5KJN1_EEPROM_CRC_PDAF_OUTPUT1_SIZE 496
#define S5KJN1_EEPROM_CRC_PDAF_OUTPUT2_SIZE 1004
#define S5KJN1_EEPROM_CRC_MANUFACTURING_SIZE 37
#define S5KJN1_MANUFACTURE_PART_NUMBER "28D14866"
#define S5KJN1_MPN_LENGTH 8
#define S5KJN1_EEPROM_GGC_SIZE 346
#define S5KJN1_EEPROM_GGC_START_ADDR 0x386B
#define S5KJN1_EEPROM_GGC_END_ADDR 0x39C4
/*Penang Code for EKPENAN4GU-2504 by chenxiaoyong at 20240311 start*/
extern kal_uint8 otp_status_main;//otp_state
extern char module_info_main[32];//module_id
/*Penang Code for EKPENAN4GU-2504 by chenxiaoyong at 20240311 end*/
//static kal_uint16 addr_data_pair_ggc_jn1sq[S5KJN1_EEPROM_GGC_SIZE+4] = {0};
static uint8_t S5KJN1_eeprom[S5KJN1_EEPROM_SIZE] = {0};
static calibration_status_t mnf_status = CRC_FAILURE;
static calibration_status_t af_status = CRC_FAILURE;
static calibration_status_t awb_status = CRC_FAILURE;
static calibration_status_t lsc_status = CRC_FAILURE;
static calibration_status_t pdaf_status = CRC_FAILURE;
static calibration_status_t dualStatus = CRC_FAILURE;
static calibration_status_t oisStatus = CRC_FAILURE;
static calibration_status_t stereoCalStatus = CRC_FAILURE;

static uint8_t crc_reverse_byte(uint32_t data)
{
	return ((data * 0x0802LU & 0x22110LU) |
		(data * 0x8020LU & 0x88440LU)) * 0x10101LU >> 16;
}

static uint32_t convert_crc(uint8_t *crc_ptr)
{
	return (crc_ptr[0] << 8) | (crc_ptr[1]);
}
/*
static uint16_t to_uint16_swap(uint8_t *data)
{
	uint16_t converted;
	memcpy(&converted, data, sizeof(uint16_t));
	return ntohs(converted);
}
*/
static void S5KJN1_read_data_from_eeprom(kal_uint8 slave, kal_uint32 start_add, uint32_t size)
{
	int i = 0;
	spin_lock(&imgsensor_drv_lock);
	imgsensor.i2c_write_id = slave;
	spin_unlock(&imgsensor_drv_lock);
    LOG_INF("s5kjn1_enter_read_data_from_eeprom\n");
	//read eeprom data
	for (i = 0; i < size; i ++) {
		S5KJN1_eeprom[i] = read_cmos_sensor_8(start_add);
		start_add ++;
	}

	spin_lock(&imgsensor_drv_lock);
	imgsensor.i2c_write_id = S5KJN1_SENSOR_IIC_SLAVE_ADDR;
	spin_unlock(&imgsensor_drv_lock);
}
/*
static void S5KJN1_eeprom_dump_bin(const char *file_name, uint32_t size, const void *data)
{
	struct file *fp = NULL;
	mm_segment_t old_fs;
	int ret = 0;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	fp = filp_open(file_name, O_WRONLY | O_CREAT | O_TRUNC | O_SYNC, 0666);
	if (IS_ERR_OR_NULL(fp)) {
            ret = PTR_ERR(fp);
		LOG_INF("open file error(%s), error(%d)\n",  file_name, ret);
		goto p_err;
	}

	ret = vfs_write(fp, (const char *)data, size, &fp->f_pos);
	if (ret < 0) {
		LOG_INF("file write fail(%s) to EEPROM data(%d)", file_name, ret);
		goto p_err;
	}

	LOG_INF("wirte to file(%s)\n", file_name);
p_err:
	if (!IS_ERR_OR_NULL(fp))
		filp_close(fp, NULL);

	set_fs(old_fs);
	LOG_INF(" end writing file");
}
*/
static int32_t eeprom_util_check_crc16(uint8_t *data, uint32_t size, uint32_t ref_crc)
{
	int32_t crc_match = 0;
	uint16_t crc = 0x0000;
	uint16_t crc_reverse = 0x0000;
	uint32_t i, j;

	uint32_t tmp;
	uint32_t tmp_reverse;

	/* Calculate both methods of CRC since integrators differ on
	* how CRC should be calculated. */
	for (i = 0; i < size; i++) {
		tmp_reverse = crc_reverse_byte(data[i]);
		tmp = data[i] & 0xff;
		for (j = 0; j < 8; j++) {
			if (((crc & 0x8000) >> 8) ^ (tmp & 0x80))
				crc = (crc << 1) ^ 0x8005;
			else
				crc = crc << 1;
			tmp <<= 1;

			if (((crc_reverse & 0x8000) >> 8) ^ (tmp_reverse & 0x80))
				crc_reverse = (crc_reverse << 1) ^ 0x8005;
			else
				crc_reverse = crc_reverse << 1;

			tmp_reverse <<= 1;
		}
	}

	crc_reverse = (crc_reverse_byte(crc_reverse) << 8) |
		crc_reverse_byte(crc_reverse >> 8);

	if (crc == ref_crc || crc_reverse == ref_crc)
		crc_match = 1;

	LOG_INF("REF_CRC 0x%x CALC CRC 0x%x CALC Reverse CRC 0x%x matches? %d\n",
		ref_crc, crc, crc_reverse, crc_match);

	return crc_match;
}

/*
static uint8_t eeprom_util_check_awb_limits(awb_t unit, awb_t golden)
{
	uint8_t result = 0;

	if (unit.r < AWB_R_MIN || unit.r > AWB_R_MAX) {
		LOG_INF("unit r out of range! MIN: %d, r: %d, MAX: %d",
			AWB_R_MIN, unit.r, AWB_R_MAX);
		result = 1;
	}
	if (unit.gr < AWB_GR_MIN || unit.gr > AWB_GR_MAX) {
		LOG_INF("unit gr out of range! MIN: %d, gr: %d, MAX: %d",
			AWB_GR_MIN, unit.gr, AWB_GR_MAX);
		result = 1;
	}
	if (unit.gb < AWB_GB_MIN || unit.gb > AWB_GB_MAX) {
		LOG_INF("unit gb out of range! MIN: %d, gb: %d, MAX: %d",
			AWB_GB_MIN, unit.gb, AWB_GB_MAX);
		result = 1;
	}
	if (unit.b < AWB_B_MIN || unit.b > AWB_B_MAX) {
		LOG_INF("unit b out of range! MIN: %d, b: %d, MAX: %d",
			AWB_B_MIN, unit.b, AWB_B_MAX);
		result = 1;
	}

	if (golden.r < AWB_R_MIN || golden.r > AWB_R_MAX) {
		LOG_INF("golden r out of range! MIN: %d, r: %d, MAX: %d",
			AWB_R_MIN, golden.r, AWB_R_MAX);
		result = 1;
	}
	if (golden.gr < AWB_GR_MIN || golden.gr > AWB_GR_MAX) {
		LOG_INF("golden gr out of range! MIN: %d, gr: %d, MAX: %d",
			AWB_GR_MIN, golden.gr, AWB_GR_MAX);
		result = 1;
	}
	if (golden.gb < AWB_GB_MIN || golden.gb > AWB_GB_MAX) {
		LOG_INF("golden gb out of range! MIN: %d, gb: %d, MAX: %d",
			AWB_GB_MIN, golden.gb, AWB_GB_MAX);
		result = 1;
	}
	if (golden.b < AWB_B_MIN || golden.b > AWB_B_MAX) {
		LOG_INF("golden b out of range! MIN: %d, b: %d, MAX: %d",
			AWB_B_MIN, golden.b, AWB_B_MAX);
		result = 1;
	}

	return result;
}
*/
/*
static uint8_t eeprom_util_calculate_awb_factors_limit(awb_t unit, awb_t golden,
		awb_limit_t limit)
{
	uint32_t r_g;
	uint32_t b_g;
	uint32_t golden_rg, golden_bg;
	uint32_t r_g_golden_min;
	uint32_t r_g_golden_max;
	uint32_t b_g_golden_min;
	uint32_t b_g_golden_max;

	r_g = unit.r_g * 1000;
	b_g = unit.b_g*1000;

	golden_rg = golden.r_g* 1000;
	golden_bg = golden.b_g* 1000;

	r_g_golden_min = limit.r_g_golden_min*16384;
	r_g_golden_max = limit.r_g_golden_max*16384;
	b_g_golden_min = limit.b_g_golden_min*16384;
	b_g_golden_max = limit.b_g_golden_max*16384;
	LOG_INF("rg = %d, bg=%d,rgmin=%d,bgmax =%d\n",r_g,b_g,r_g_golden_min,r_g_golden_max);
	LOG_INF("grg = %d, gbg=%d,bgmin=%d,bgmax =%d\n",golden_rg,golden_bg,b_g_golden_min,b_g_golden_max);
	if (r_g < (golden_rg - r_g_golden_min) || r_g > (golden_rg + r_g_golden_max)) {
		LOG_INF("Final RG calibration factors out of range!");
		return 1;
	}

	if (b_g < (golden_bg - b_g_golden_min) || b_g > (golden_bg + b_g_golden_max)) {
		LOG_INF("Final BG calibration factors out of range!");
		return 1;
	}
	return 0;
}
*/

#if EEPROM_READY //stan
#define FOUR_CELL_SIZE 8006
static char Four_Cell_Size_Array[2];
static void read_4cell_from_eeprom(char *data)
{
	Four_Cell_Size_Array[0] = (FOUR_CELL_SIZE & 0xff);/*Low*/
	Four_Cell_Size_Array[1] = ((FOUR_CELL_SIZE >> 8) & 0xff);/*High*/

	if (data != NULL) {
		pr_err("jn1 memcpy  xtc to data\n");
		memcpy(data, Four_Cell_Size_Array, 2);	//adress
		memcpy(data+2, &S5KJN1_eeprom[6429], 2612);	//XTC
		memcpy(data+2614, &S5KJN1_eeprom[9043], 768);	//sensor XTC
		memcpy(data+3382, &S5KJN1_eeprom[9813], 4000);//PDXTC
		memcpy(data+7382, &S5KJN1_eeprom[13815], 626);//SW GGC
	}
}
#endif

static calibration_status_t S5KJN1_check_awb_data(void *data)
{
	struct S5KJN1_eeprom_t *eeprom = (struct S5KJN1_eeprom_t*)data;
	//awb_t unit;
	//awb_t golden;
	//awb_limit_t golden_limit;

	if(!eeprom_util_check_crc16(eeprom->cie_src_1_ev,
		S5KJN1_EEPROM_CRC_AWB_CAL_SIZE,
		convert_crc(eeprom->awb_crc16))) {
		LOG_INF("--AWB CRC Fails!");
		return CRC_FAILURE;
	}
/*
	unit.r = to_uint16_swap(eeprom->awb_src_1_r)/64;
	unit.gr = to_uint16_swap(eeprom->awb_src_1_gr)/64;
	unit.gb = to_uint16_swap(eeprom->awb_src_1_gb)/64;
	unit.b = to_uint16_swap(eeprom->awb_src_1_b)/64;
	unit.r_g = to_uint16_swap(eeprom->awb_src_1_rg_ratio);
	unit.b_g = to_uint16_swap(eeprom->awb_src_1_bg_ratio);
	unit.gr_gb = to_uint16_swap(eeprom->awb_src_1_gr_gb_ratio);

	golden.r = to_uint16_swap(eeprom->awb_src_1_golden_r)/64;
	golden.gr = to_uint16_swap(eeprom->awb_src_1_golden_gr)/64;
	golden.gb = to_uint16_swap(eeprom->awb_src_1_golden_gb)/64;
	golden.b = to_uint16_swap(eeprom->awb_src_1_golden_b)/64;
	golden.r_g = to_uint16_swap(eeprom->awb_src_1_golden_rg_ratio);
	golden.b_g = to_uint16_swap(eeprom->awb_src_1_golden_bg_ratio);
	golden.gr_gb = to_uint16_swap(eeprom->awb_src_1_golden_gr_gb_ratio);
	if (eeprom_util_check_awb_limits(unit, golden)) {
		LOG_INF("AWB CRC limit Fails!");
		return LIMIT_FAILURE;
	}

	golden_limit.r_g_golden_min = eeprom->awb_r_g_golden_min_limit[0];
	golden_limit.r_g_golden_max = eeprom->awb_r_g_golden_max_limit[0];
	golden_limit.b_g_golden_min = eeprom->awb_b_g_golden_min_limit[0];
	golden_limit.b_g_golden_max = eeprom->awb_b_g_golden_max_limit[0];

	if (eeprom_util_calculate_awb_factors_limit(unit, golden,golden_limit)) {
		LOG_INF("AWB CRC factor limit Fails!");
		return LIMIT_FAILURE;
	}
	*/
	LOG_INF("AWB CRC Pass");
	return NO_ERRORS;
}

static calibration_status_t S5KJN1_check_lsc_data_mtk(void *data)
{
	struct S5KJN1_eeprom_t *eeprom = (struct S5KJN1_eeprom_t*)data;

	if (!eeprom_util_check_crc16(eeprom->lsc_data_mtk, S5KJN1_EEPROM_CRC_LSC_SIZE,
		convert_crc(eeprom->lsc_crc16_mtk))) {
		LOG_INF("LSC CRC Fails!");
		return CRC_FAILURE;
	}
	LOG_INF("LSC CRC Pass");
	return NO_ERRORS;
}
/*Penang Code for EKPENAN4GU-2504 by chenxiaoyong at 20240311 start*/
static void S5KJN1_eeprom_get_serial_number(void *data){
	int i = 0;
	struct S5KJN1_eeprom_t *eeprom = (struct S5KJN1_eeprom_t*)data;

	for(i=0;i<S5KJN1_SERIAL_NUM_SIZE;i++){
			LOG_INF("serial_number[%d]=%x",i,eeprom->serial_number[i]);
	}

	//S5KJN1_eeprom_dump_bin(MAIN_SERIAL_NUM_DATA_PATH,  S5KJN1_SERIAL_NUM_SIZE,  eeprom->serial_number);
}
/*Penang Code for EKPENAN4GU-2504 by chenxiaoyong at 20240311 end*/

/*
static void S5KJN1_eeprom_get_mnf_data(void *data,
		calibration_mnf_t *mnf)
{
	int ret;
	struct S5KJN1_eeprom_t *eeprom = (struct S5KJN1_eeprom_t*)data;

	ret = snprintf(mnf->table_revision, MAX_CALIBRATION_STRING, "0x%x",
		eeprom->eeprom_table_version[0]);

	if (ret < 0 || ret >= MAX_CALIBRATION_STRING) {
		LOG_INF("snprintf of mnf->table_revision failed");
		mnf->table_revision[0] = 0;
	}

	ret = snprintf(mnf->part_number, MAX_CALIBRATION_STRING, "%c%c%c%c%c%c%c%c",
		eeprom->mpn[0], eeprom->mpn[1], eeprom->mpn[2], eeprom->mpn[3],
		eeprom->mpn[4], eeprom->mpn[5], eeprom->mpn[6], eeprom->mpn[7]);

	if (ret < 0 || ret >= MAX_CALIBRATION_STRING) {
		LOG_INF("snprintf of mnf->part_number failed");
		mnf->part_number[0] = 0;
	}

	ret = snprintf(mnf->actuator_id, MAX_CALIBRATION_STRING, "0x%x", eeprom->actuator_id[0]);

	if (ret < 0 || ret >= MAX_CALIBRATION_STRING) {
		LOG_INF("snprintf of mnf->actuator_id failed");
		mnf->actuator_id[0] = 0;
	}

	ret = snprintf(mnf->lens_id, MAX_CALIBRATION_STRING, "0x%x", eeprom->lens_id[0]);

	if (ret < 0 || ret >= MAX_CALIBRATION_STRING) {
		LOG_INF("snprintf of mnf->lens_id failed");
		mnf->lens_id[0] = 0;
	}

	if (eeprom->manufacturer_id[0] == 'S' && eeprom->manufacturer_id[1] == 'U') {
		ret = snprintf(mnf->integrator, MAX_CALIBRATION_STRING, "Sunny");
	} else if (eeprom->manufacturer_id[0] == 'O' && eeprom->manufacturer_id[1] == 'F') {
		ret = snprintf(mnf->integrator, MAX_CALIBRATION_STRING, "OFilm");
	} else if (eeprom->manufacturer_id[0] == 'Q' && eeprom->manufacturer_id[1] == 'T') {
		ret = snprintf(mnf->integrator, MAX_CALIBRATION_STRING, "Qtech");
	} else {
		ret = snprintf(mnf->integrator, MAX_CALIBRATION_STRING, "Unknown");
		LOG_INF("unknown manufacturer_id");
	}

	if (ret < 0 || ret >= MAX_CALIBRATION_STRING) {
		LOG_INF("snprintf of mnf->integrator failed");
		mnf->integrator[0] = 0;
	}

	ret = snprintf(mnf->factory_id, MAX_CALIBRATION_STRING, "%c%c",
		eeprom->factory_id[0], eeprom->factory_id[1]);

	if (ret < 0 || ret >= MAX_CALIBRATION_STRING) {
		LOG_INF("snprintf of mnf->factory_id failed");
		mnf->factory_id[0] = 0;
	}

	ret = snprintf(mnf->manufacture_line, MAX_CALIBRATION_STRING, "%u",
		eeprom->manufacture_line[0]);

	if (ret < 0 || ret >= MAX_CALIBRATION_STRING) {
		LOG_INF("snprintf of mnf->manufacture_line failed");
		mnf->manufacture_line[0] = 0;
	}

	ret = snprintf(mnf->manufacture_date, MAX_CALIBRATION_STRING, "20%u/%u/%u",
		eeprom->manufacture_date[0], eeprom->manufacture_date[1], eeprom->manufacture_date[2]);

	if (ret < 0 || ret >= MAX_CALIBRATION_STRING) {
		LOG_INF("snprintf of mnf->manufacture_date failed");
		mnf->manufacture_date[0] = 0;
	}

	ret = snprintf(mnf->serial_number, MAX_CALIBRATION_STRING, "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
		eeprom->serial_number[0], eeprom->serial_number[1],
		eeprom->serial_number[2], eeprom->serial_number[3],
		eeprom->serial_number[4], eeprom->serial_number[5],
		eeprom->serial_number[6], eeprom->serial_number[7],
		eeprom->serial_number[8], eeprom->serial_number[9],
		eeprom->serial_number[10], eeprom->serial_number[11],
		eeprom->serial_number[12], eeprom->serial_number[13],
		eeprom->serial_number[14], eeprom->serial_number[15]);
	S5KJN1_eeprom_dump_bin(MAIN_SERIAL_NUM_DATA_PATH,  S5KJN1_SERIAL_NUM_SIZE,  eeprom->serial_number);
	if (ret < 0 || ret >= MAX_CALIBRATION_STRING) {
		LOG_INF("snprintf of mnf->serial_number failed");
		mnf->serial_number[0] = 0;
	}
}
*/
static calibration_status_t S5KJN1_check_af_data(void *data)
{
	struct S5KJN1_eeprom_t *eeprom = (struct S5KJN1_eeprom_t*)data;

	if (!eeprom_util_check_crc16(eeprom->af_data, S5KJN1_EEPROM_CRC_AF_CAL_SIZE,
		convert_crc(eeprom->af_crc16))) {
		LOG_INF("Autofocus CRC Fails!");
		return CRC_FAILURE;
	}
	LOG_INF("-Autofocus CRC Pass");
	return NO_ERRORS;
}

static calibration_status_t S5KJN1_check_pdaf_data(void *data)
{
	struct S5KJN1_eeprom_t *eeprom = (struct S5KJN1_eeprom_t*)data;

	if (!eeprom_util_check_crc16(eeprom->pdaf_out1_data_mtk, S5KJN1_EEPROM_CRC_PDAF_OUTPUT1_SIZE,
		convert_crc(eeprom->pdaf_out1_crc16_mtk))) {
		LOG_INF("PDAF OUTPUT1 CRC Fails!");
		return CRC_FAILURE;
	}

	if (!eeprom_util_check_crc16(eeprom->pdaf_out2_data_mtk, S5KJN1_EEPROM_CRC_PDAF_OUTPUT2_SIZE,
		convert_crc(eeprom->pdaf_out2_crc16_mtk))) {
		LOG_INF("PDAF OUTPUT2 CRC Fails!");
		return CRC_FAILURE;
	}

	LOG_INF("PDAF CRC Pass");
	return NO_ERRORS;
}

static calibration_status_t S5KJN1_check_manufacturing_data(void *data)
{
	struct S5KJN1_eeprom_t *eeprom = (struct S5KJN1_eeprom_t*)data;
#if 0
	if(strncmp(eeprom->mpn, S5KJN1_MANUFACTURE_PART_NUMBER, S5KJN1_MPN_LENGTH) != 0) {
		LOG_INF("Manufacturing part number (%s) check Fails!", eeprom->mpn);
		return CRC_FAILURE;
	}
#endif
	if (!eeprom_util_check_crc16(data, S5KJN1_EEPROM_CRC_MANUFACTURING_SIZE,
		convert_crc(eeprom->manufacture_crc16))) {
		LOG_INF("Manufacturing CRC Fails!");
		return CRC_FAILURE;
	}
	LOG_INF("Manufacturing CRC Pass");
	return NO_ERRORS;
}

static void S5KJN1_eeprom_format_calibration_data(void *data)
{
	if (NULL == data) {
		LOG_INF("data is NULL");
		return;
	}

	mnf_status = S5KJN1_check_manufacturing_data(data);
	af_status = S5KJN1_check_af_data(data);
	awb_status = S5KJN1_check_awb_data(data);
	lsc_status = S5KJN1_check_lsc_data_mtk(data);
	pdaf_status = S5KJN1_check_pdaf_data(data);
	dualStatus = NO_ERRORS;
    oisStatus = NO_ERRORS;
    stereoCalStatus = NO_ERRORS;
/*Penang Code for EKPENAN4GU-2504 by chenxiaoyong at 20240311 start*/
	otp_status_main = mnf_status | awb_status << 1 | lsc_status << 2 | af_status << 3 | pdaf_status << 4 | dualStatus << 5 | oisStatus << 6 | stereoCalStatus << 7;
/*Penang Code for EKPENAN4GU-2504 by chenxiaoyong at 20240311 end*/

	LOG_INF("status mnf:%d, af:%d, awb:%d, lsc:%d, pdaf:%d, dual:%d",
		mnf_status, af_status, awb_status, lsc_status, pdaf_status, dualStatus);
}

/*Penang Code for EKPENAN4GU-2504 by chenxiaoyong at 20240311 start*/
static void S5KJN1_eeprom_get_module_info(void *data)
{
	struct S5KJN1_eeprom_t *eeprom = (struct S5KJN1_eeprom_t*)data;
	if( (eeprom->manufacturer_id[0] ==  'Q') && (eeprom->manufacturer_id[1] ==  'T')){//qtech
		sprintf(module_info_main, "%s", "s5kjn1sq03_qtech_mipi_raw");
	} else if( (eeprom->manufacturer_id[0] ==  'S') && (eeprom->manufacturer_id[1] ==  'U')){//sunny
		sprintf(module_info_main, "%s", "s5kjn1sq03_sunny_mipi_raw");
	}
	LOG_INF("eeprom->manufacturer_id[0]:%s, eeprom->manufacturer_id[1]:%s", eeprom->manufacturer_id[0], eeprom->manufacturer_id[1]);

}
/*Penang Code for EKPENAN4GU-2504 by chenxiaoyong at 20240311 end*/

/*
static void S5KJN1_eeprom_format_ggc_data(void)
{
	kal_uint16 gcc_crc16 = S5KJN1_eeprom[S5KJN1_EEPROM_GGC_END_ADDR+1]<<8|S5KJN1_eeprom[S5KJN1_EEPROM_GGC_END_ADDR+2];
	kal_uint16 i = 0;

	if (eeprom_util_check_crc16((S5KJN1_eeprom + S5KJN1_EEPROM_GGC_START_ADDR),
		S5KJN1_EEPROM_GGC_SIZE, gcc_crc16)) {
		pr_debug("HW GCC CRC success!");

		addr_data_pair_ggc_jn1sq[0] = 0x6028;
		addr_data_pair_ggc_jn1sq[1] = 0x2400;
		addr_data_pair_ggc_jn1sq[2] = 0x602A;
		addr_data_pair_ggc_jn1sq[3] = 0x0CFC;
		for(i = 0; i < S5KJN1_EEPROM_GGC_SIZE; i += 2){
			addr_data_pair_ggc_jn1sq[i+4] = 0x6F12;
			addr_data_pair_ggc_jn1sq[i+5] = S5KJN1_eeprom[i + S5KJN1_EEPROM_GGC_START_ADDR]<<8 | S5KJN1_eeprom[i + S5KJN1_EEPROM_GGC_START_ADDR+1];
		}

	}
}
*/
#endif   //cfp-210812
/*************************************************************************
 * FUNCTION
 *	get_imgsensor_id
 *
 * DESCRIPTION
 *	This function get the sensor ID
 *
 * PARAMETERS
 *	*sensorID : return the sensor ID
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
#if 1
static kal_uint32 get_imgsensor_id(UINT32 *sensor_id)
{
	kal_uint8 i = 0;
	kal_uint8 retry = 2;
//	kal_uint16 sp8spFlag = 0;

	/* sensor have two i2c address 0x6c 0x6d & 0x21 0x20,
	 *we should detect the module used i2c address
	 */
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			*sensor_id = (
		(read_cmos_sensor_8(0x0000) << 8) | read_cmos_sensor_8(0x0001));

			if (*sensor_id == imgsensor_info.sensor_id) {
#if EEPROM_READY //stan
				S5KJN1_read_data_from_eeprom(S5KJN1_EEPROM_SLAVE_ADDR, 0x0000, S5KJN1_EEPROM_SIZE);
				//S5KJN1_eeprom_dump_bin(EEPROM_DATA_PATH, S5KJN1_EEPROM_SIZE, (void *)S5KJN1_eeprom);
				S5KJN1_eeprom_get_serial_number((void *)S5KJN1_eeprom);
				S5KJN1_eeprom_format_calibration_data((void *)S5KJN1_eeprom);
				S5KJN1_eeprom_get_module_info((void *)S5KJN1_eeprom);
				//S5KJN1_eeprom_format_ggc_data();
				pr_debug("i2c write id: 0x%x, sensor id: 0x%x\n",
					imgsensor.i2c_write_id, *sensor_id);
				/* preload 4cell data */
				//read_4cell_from_eeprom(NULL);
#endif
				printk("##############0000################### i2c write id: 0x%x, sensor id: 0x%x\n",imgsensor.i2c_write_id, *sensor_id);

				return ERROR_NONE;

		/* 4Cell version check, s5kjn1sq03 And s5kjn1sq03's checking is differet
		 *	sp8spFlag = (((read_cmos_sensor(0x000C) & 0xFF) << 8)
		 *		|((read_cmos_sensor(0x000E) >> 8) & 0xFF));
		 *	pr_debug(
		 *	"sp8Flag(0x%x),0x5003 used by s5kjn1sq03\n", sp8spFlag);
		 *
		 *	if (sp8spFlag == 0x5003) {
		 *		pr_debug("it is s5kjn1sq03\n");
		 *		return ERROR_NONE;
		 *	}
		 *
		 *		pr_debug(
		 *	"s5kjn1sq03 type is 0x(%x),0x000C(0x%x),0x000E(0x%x)\n",
		 *		sp8spFlag,
		 *		read_cmos_sensor(0x000C),
		 *		read_cmos_sensor(0x000E));
		 *
		 *		*sensor_id = 0xFFFFFFFF;
		 *	return ERROR_SENSOR_CONNECT_FAIL;
		 */
			}
			pr_debug("Read sensor id fail, id: 0x%x\n",
				imgsensor.i2c_write_id);
			retry--;
		} while (retry > 0);
		i++;
		retry = 2;
	}
	if (*sensor_id != imgsensor_info.sensor_id) {
	/* if Sensor ID is not correct, Must set *sensor_id to 0xFFFFFFFF */
		*sensor_id = 0xFFFFFFFF;
		return ERROR_SENSOR_CONNECT_FAIL;
	}
	return ERROR_NONE;
}
#endif

/*************************************************************************
 * FUNCTION
 *	open
 *
 * DESCRIPTION
 *	This function initialize the registers of CMOS sensor
 *
 * PARAMETERS
 *	None
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint32 open(void)
{
	kal_uint8 i = 0;
	kal_uint8 retry = 2;
	kal_uint16 sensor_id = 0;

	printk("---------------------------------------open---------------------------------------\n");

	printk("##################000############## %s", __func__);

	/* sensor have two i2c address 0x6c 0x6d & 0x21 0x20,
	 * we should detect the module used i2c address
	 */
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			sensor_id = (
		(read_cmos_sensor_8(0x0000) << 8) | read_cmos_sensor_8(0x0001));

			if (sensor_id == imgsensor_info.sensor_id) {
				printk("##################111############## i2c write id: 0x%x, sensor id: 0x%x\n",
					imgsensor.i2c_write_id, sensor_id);
				break;
			}

			printk("##################222############## Read sensor id fail, id: 0x%x\n",
				imgsensor.i2c_write_id);
			retry--;
		} while (retry > 0);
		i++;
		if (sensor_id == imgsensor_info.sensor_id)
			break;
		retry = 2;
	}
	if (imgsensor_info.sensor_id != sensor_id)
		return ERROR_SENSOR_CONNECT_FAIL;

	/* initail sequence write in  */
	sensor_init();

#if 0  //cfp-210812
	pr_debug("wirte gcc date to sensor reg");
	table_write_cmos_sensor(addr_data_pair_ggc_jn1sq,
		sizeof(addr_data_pair_ggc_jn1sq) / sizeof(kal_uint16));
#endif

	spin_lock(&imgsensor_drv_lock);

	imgsensor.autoflicker_en = KAL_FALSE;
	imgsensor.sensor_mode = IMGSENSOR_MODE_INIT;
	imgsensor.shutter = 0x3D0;
	imgsensor.gain = 0x100;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.dummy_pixel = 0;
	imgsensor.dummy_line = 0;
	imgsensor.ihdr_mode = 0;
	imgsensor.test_pattern = KAL_FALSE;
	imgsensor.current_fps = imgsensor_info.pre.max_framerate;
	spin_unlock(&imgsensor_drv_lock);

	printk("##################333############## %s", __func__);

	return ERROR_NONE;
}				/*      open  */



/*************************************************************************
 * FUNCTION
 *	close
 *
 * DESCRIPTION
 *
 *
 * PARAMETERS
 *	None
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint32 close(void)
{
	pr_debug("E\n");

	printk("---------------------------------------close---------------------------------------\n");

	/*No Need to implement this function */

	return ERROR_NONE;
}				/*      close  */


/*************************************************************************
 * FUNCTION
 * preview
 *
 * DESCRIPTION
 *	This function start the sensor preview.
 *
 * PARAMETERS
 *	*image_window : address pointer of pixel numbers in one period of HSYNC
 *  *sensor_config_data : address pointer of line numbers in one period of VSYNC
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint32 preview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	printk("---------------------------------------preview---------------------------------------\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_PREVIEW;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	preview_setting();
	set_mirror_flip(imgsensor.mirror);

	return ERROR_NONE;
}				/*      preview   */

/*************************************************************************
 * FUNCTION
 *	capture
 *
 * DESCRIPTION
 *	This function setup the CMOS sensor in capture MY_OUTPUT mode
 *
 * PARAMETERS
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint32 capture(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	printk("---------------------------------------capture---------------------------------------\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CAPTURE;

	if (imgsensor.current_fps != imgsensor_info.cap.max_framerate) {
		pr_debug("Warning: current_fps %d fps is not support, so use cap's setting: %d fps!\n",
			imgsensor.current_fps,
			imgsensor_info.cap.max_framerate / 10);
	}

	imgsensor.pclk = imgsensor_info.cap.pclk;
	imgsensor.line_length = imgsensor_info.cap.linelength;
	imgsensor.frame_length = imgsensor_info.cap.framelength;
	imgsensor.min_frame_length = imgsensor_info.cap.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	//capture_setting(imgsensor.current_fps);
	preview_setting();
	set_mirror_flip(imgsensor.mirror);

	return ERROR_NONE;
}				/* capture() */

static kal_uint32 normal_video(
	MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{

	printk("---------------------------------------normal_video---------------------------------------\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_VIDEO;
	imgsensor.pclk = imgsensor_info.normal_video.pclk;
	imgsensor.line_length = imgsensor_info.normal_video.linelength;
	imgsensor.frame_length = imgsensor_info.normal_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.normal_video.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	normal_video_setting(imgsensor.current_fps);
	set_mirror_flip(imgsensor.mirror);

	return ERROR_NONE;
}				/*      normal_video   */

static kal_uint32 hs_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	printk("---------------------------------------hs_video---------------------------------------\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_HIGH_SPEED_VIDEO;
	imgsensor.pclk = imgsensor_info.hs_video.pclk;
	/* imgsensor.video_mode = KAL_TRUE; */
	imgsensor.line_length = imgsensor_info.hs_video.linelength;
	imgsensor.frame_length = imgsensor_info.hs_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.hs_video.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	hs_video_setting(imgsensor.current_fps);
	set_mirror_flip(imgsensor.mirror);
	return ERROR_NONE;
}
static kal_uint32 slim_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	printk("---------------------------------------slim_video---------------------------------------\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_SLIM_VIDEO;
	imgsensor.pclk = imgsensor_info.slim_video.pclk;
	imgsensor.line_length = imgsensor_info.slim_video.linelength;
	imgsensor.frame_length = imgsensor_info.slim_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.slim_video.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	slim_video_setting(imgsensor.current_fps);
	set_mirror_flip(imgsensor.mirror);
	return ERROR_NONE;
}

#if  S5KJN1SQ03_CUSTOM_ENABLE
static kal_uint32 custom1(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT
			  *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{

	printk("---------------------------------------custom1---------------------------------------\n");

	pr_debug("enter custom1 cur fps: %d\n", imgsensor.current_fps);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM1;
	imgsensor.pclk = imgsensor_info.custom1.pclk;
	imgsensor.line_length = imgsensor_info.custom1.linelength;
	imgsensor.frame_length = imgsensor_info.custom1.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom1.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	custom1_setting();
	set_mirror_flip(imgsensor.mirror);
	//mdelay(10);

	return ERROR_NONE;
}   /*  custom1   */

static kal_uint32 custom2(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *
		image_window, MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");
	printk("---------------------------------------custom2---------------------------------------\n");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM2;
	imgsensor.pclk = imgsensor_info.custom2.pclk;

	imgsensor.line_length = imgsensor_info.custom2.linelength;
	imgsensor.frame_length = imgsensor_info.custom2.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom2.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	custom2_setting();
	set_mirror_flip(imgsensor.mirror);
	return ERROR_NONE;
}
#endif

static kal_uint32 get_resolution(
	MSDK_SENSOR_RESOLUTION_INFO_STRUCT(*sensor_resolution))
{
	printk("---------------------------------------get_resolution---------------------------------------\n");
	pr_debug("get resolution E\n");
	sensor_resolution->SensorFullWidth =
		imgsensor_info.cap.grabwindow_width;
	sensor_resolution->SensorFullHeight =
		imgsensor_info.cap.grabwindow_height;

	sensor_resolution->SensorPreviewWidth =
		imgsensor_info.pre.grabwindow_width;
	sensor_resolution->SensorPreviewHeight =
		imgsensor_info.pre.grabwindow_height;

	sensor_resolution->SensorVideoWidth =
		imgsensor_info.normal_video.grabwindow_width;
	sensor_resolution->SensorVideoHeight =
		imgsensor_info.normal_video.grabwindow_height;
	sensor_resolution->SensorHighSpeedVideoWidth =
		imgsensor_info.hs_video.grabwindow_width;
	sensor_resolution->SensorHighSpeedVideoHeight =
		imgsensor_info.hs_video.grabwindow_height;
	sensor_resolution->SensorSlimVideoWidth =
		imgsensor_info.slim_video.grabwindow_width;
	sensor_resolution->SensorSlimVideoHeight =
		imgsensor_info.slim_video.grabwindow_height;
#if S5KJN1SQ03_CUSTOM_ENABLE
	sensor_resolution->SensorCustom1Width  =
		imgsensor_info.custom1.grabwindow_width;
	sensor_resolution->SensorCustom1Height  =
		imgsensor_info.custom1.grabwindow_height;
	sensor_resolution->SensorCustom2Width =
		imgsensor_info.custom2.grabwindow_width;
	sensor_resolution->SensorCustom2Height =
		imgsensor_info.custom2.grabwindow_height;
#endif
	return ERROR_NONE;
}				/*      get_resolution  */

static kal_uint32 get_info(enum MSDK_SCENARIO_ID_ENUM scenario_id,
			   MSDK_SENSOR_INFO_STRUCT *sensor_info,
			   MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	/*pr_debug("get_info -> scenario_id = %d\n", scenario_id);*/

	printk("---------------------------------------get_info---------------------------------------\n");

	sensor_info->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;

	/* not use */
	sensor_info->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW;

	/* inverse with datasheet */
	sensor_info->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;

	sensor_info->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorInterruptDelayLines = 4;	/* not use */
	sensor_info->SensorResetActiveHigh = FALSE;	/* not use */
	sensor_info->SensorResetDelayCount = 5;	/* not use */

	sensor_info->SensroInterfaceType = imgsensor_info.sensor_interface_type;
	sensor_info->MIPIsensorType = imgsensor_info.mipi_sensor_type;
	sensor_info->SettleDelayMode = imgsensor_info.mipi_settle_delay_mode;

	sensor_info->SensorOutputDataFormat =
		imgsensor_info.sensor_output_dataformat;

	sensor_info->FrameTimeDelayFrame = imgsensor_info.frame_time_delay_frame;
	sensor_info->CaptureDelayFrame = imgsensor_info.cap_delay_frame;
	sensor_info->PreviewDelayFrame = imgsensor_info.pre_delay_frame;
	sensor_info->VideoDelayFrame = imgsensor_info.video_delay_frame;
	sensor_info->HighSpeedVideoDelayFrame = imgsensor_info.hs_video_delay_frame;
	sensor_info->SlimVideoDelayFrame = imgsensor_info.slim_video_delay_frame;
#if S5KJN1SQ03_CUSTOM_ENABLE
	sensor_info->Custom1DelayFrame = imgsensor_info.custom1_delay_frame;
	sensor_info->Custom2DelayFrame = imgsensor_info.custom2_delay_frame;
#endif
	sensor_info->SensorMasterClockSwitch = 0;	/* not use */
	sensor_info->SensorDrivingCurrent = imgsensor_info.isp_driving_current;
	sensor_info->FrameTimeDelayFrame = imgsensor_info.frame_time_delay_frame;

	/* The frame of setting shutter default 0 for TG int */
	sensor_info->AEShutDelayFrame = imgsensor_info.ae_shut_delay_frame;

	/* The frame of setting sensor gain*/
	sensor_info->AESensorGainDelayFrame =
				imgsensor_info.ae_sensor_gain_delay_frame;

	sensor_info->AEISPGainDelayFrame =
				imgsensor_info.ae_ispGain_delay_frame;

	sensor_info->IHDR_Support = imgsensor_info.ihdr_support;
	sensor_info->IHDR_LE_FirstLine = imgsensor_info.ihdr_le_firstline;
	sensor_info->SensorModeNum = imgsensor_info.sensor_mode_num;

	/* change pdaf support mode to pdaf VC mode */
	sensor_info->PDAF_Support = 2;
	sensor_info->SensorMIPILaneNumber = imgsensor_info.mipi_lane_num;
	sensor_info->SensorClockFreq = imgsensor_info.mclk;
	sensor_info->SensorClockDividCount = 3;	/* not use */
	sensor_info->SensorClockRisingCount = 0;
	sensor_info->SensorClockFallingCount = 2;	/* not use */
	sensor_info->SensorPixelClockCount = 3;	/* not use */
	sensor_info->SensorDataLatchCount = 2;	/* not use */

	sensor_info->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
	sensor_info->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
	sensor_info->SensorWidthSampling = 0;	/* 0 is default 1x */
	sensor_info->SensorHightSampling = 0;	/* 0 is default 1x */
	sensor_info->SensorPacketECCOrder = 1;
#if 0 //cfp-210812
	sensor_info->calibration_status.mnf = mnf_status;
	sensor_info->calibration_status.af = af_status;
	sensor_info->calibration_status.awb = awb_status;
	sensor_info->calibration_status.lsc = lsc_status;
	sensor_info->calibration_status.pdaf = pdaf_status;
	sensor_info->calibration_status.dual = dual_status;
	S5KJN1_eeprom_get_mnf_data((void *) S5KJN1_eeprom, &sensor_info->mnf_calibration);
#endif
	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		sensor_info->SensorGrabStartX = imgsensor_info.pre.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.pre.mipi_data_lp2hs_settle_dc;

		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		sensor_info->SensorGrabStartX = imgsensor_info.cap.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.cap.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.cap.mipi_data_lp2hs_settle_dc;

		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:

		sensor_info->SensorGrabStartX =
			imgsensor_info.normal_video.startx;
		sensor_info->SensorGrabStartY =
			imgsensor_info.normal_video.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.normal_video.mipi_data_lp2hs_settle_dc;

		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		sensor_info->SensorGrabStartX = imgsensor_info.hs_video.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.hs_video.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.hs_video.mipi_data_lp2hs_settle_dc;
		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		sensor_info->SensorGrabStartX = imgsensor_info.slim_video.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.slim_video.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.slim_video.mipi_data_lp2hs_settle_dc;
		break;
#if S5KJN1SQ03_CUSTOM_ENABLE
	case MSDK_SCENARIO_ID_CUSTOM1:
		sensor_info->SensorGrabStartX = imgsensor_info.custom1.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.custom1.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.custom1.mipi_data_lp2hs_settle_dc;

		break;
	case MSDK_SCENARIO_ID_CUSTOM2:
		sensor_info->SensorGrabStartX = imgsensor_info.custom2.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.custom2.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.custom2.mipi_data_lp2hs_settle_dc;
		break;
#endif
	default:
		sensor_info->SensorGrabStartX = imgsensor_info.pre.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.pre.mipi_data_lp2hs_settle_dc;
		break;
	}

	return ERROR_NONE;
}				/*      get_info  */


static kal_uint32 control(enum MSDK_SCENARIO_ID_ENUM scenario_id,
			  MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	printk("---------------------------------------control---------------------------------------\n");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.current_scenario_id = scenario_id;
	spin_unlock(&imgsensor_drv_lock);
	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		preview(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		capture(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		normal_video(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		hs_video(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		slim_video(image_window, sensor_config_data);
		break;
#if S5KJN1SQ03_CUSTOM_ENABLE
	case MSDK_SCENARIO_ID_CUSTOM1:
		custom1(image_window, sensor_config_data); // custom1
		break;
	case MSDK_SCENARIO_ID_CUSTOM2:
		custom2(image_window, sensor_config_data);
		break;
#endif
	default:
		pr_debug("Error ScenarioId setting");
		preview(image_window, sensor_config_data);
		return ERROR_INVALID_SCENARIO_ID;
	}
	return ERROR_NONE;
}				/* control() */



static kal_uint32 set_video_mode(UINT16 framerate)
{
	/* //pr_debug("framerate = %d\n ", framerate); */
	/* SetVideoMode Function should fix framerate */
	printk("---------------------------------------set_video_mode---------------------------------------\n");
	if (framerate == 0)
		/* Dynamic frame rate */
		return ERROR_NONE;
	spin_lock(&imgsensor_drv_lock);
	if ((framerate == 300) && (imgsensor.autoflicker_en == KAL_TRUE))
		imgsensor.current_fps = 296;
	else if ((framerate == 150) && (imgsensor.autoflicker_en == KAL_TRUE))
		imgsensor.current_fps = 146;
	else
		imgsensor.current_fps = framerate;
	spin_unlock(&imgsensor_drv_lock);
	set_max_framerate(imgsensor.current_fps, 1);

	return ERROR_NONE;
}

static kal_uint32 set_auto_flicker_mode(
	kal_bool enable, UINT16 framerate)
{
	printk("---------------------------------------set_auto_flicker_mode---------------------------------------\n");
	pr_debug("enable = %d, framerate = %d\n", enable, framerate);
	spin_lock(&imgsensor_drv_lock);
	if (enable)		/* enable auto flicker */
		imgsensor.autoflicker_en = KAL_TRUE;
	else			/* Cancel Auto flick */
		imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}


static kal_uint32 set_max_framerate_by_scenario(
	enum MSDK_SCENARIO_ID_ENUM scenario_id,	MUINT32 framerate)
{
	kal_uint32 frame_length;

	printk("---------------------------------------set_max_framerate_by_scenario---------------------------------------\n");

	pr_debug("scenario_id = %d, framerate = %d\n", scenario_id, framerate);

	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		frame_length = imgsensor_info.pre.pclk
			/ framerate * 10 / imgsensor_info.pre.linelength;
		spin_lock(&imgsensor_drv_lock);

		imgsensor.dummy_line =
			(frame_length > imgsensor_info.pre.framelength)
			? (frame_length - imgsensor_info.pre.framelength) : 0;

		imgsensor.frame_length =
			imgsensor_info.pre.framelength + imgsensor.dummy_line;

		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		if (framerate == 0)
			return ERROR_NONE;
		frame_length = imgsensor_info.normal_video.pclk
		    / framerate * 10 / imgsensor_info.normal_video.linelength;

		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
	    (frame_length > imgsensor_info.normal_video.framelength)
	  ? (frame_length - imgsensor_info.normal_video.framelength) : 0;

		imgsensor.frame_length =
		 imgsensor_info.normal_video.framelength + imgsensor.dummy_line;

		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:

	frame_length = imgsensor_info.cap.pclk
		/ framerate * 10 / imgsensor_info.cap.linelength;
	spin_lock(&imgsensor_drv_lock);
	imgsensor.dummy_line =
		(frame_length > imgsensor_info.cap.framelength)
		? (frame_length - imgsensor_info.cap.framelength) : 0;
	imgsensor.frame_length =
		imgsensor_info.cap.framelength + imgsensor.dummy_line;
	imgsensor.min_frame_length = imgsensor.frame_length;
	spin_unlock(&imgsensor_drv_lock);

	if (imgsensor.frame_length > imgsensor.shutter)
		set_dummy();
	break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		frame_length = imgsensor_info.hs_video.pclk / framerate * 10 / imgsensor_info.hs_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.hs_video.framelength) ?
			(frame_length - imgsensor_info.hs_video.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.hs_video.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		frame_length = imgsensor_info.slim_video.pclk / framerate * 10 / imgsensor_info.slim_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.slim_video.framelength) ?
			(frame_length - imgsensor_info.slim_video.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.slim_video.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
#if S5KJN1SQ03_CUSTOM_ENABLE
	case MSDK_SCENARIO_ID_CUSTOM1:
		frame_length = imgsensor_info.custom1.pclk
			       / framerate * 10 / imgsensor_info.custom1.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length > imgsensor_info.custom1.framelength)
			? (frame_length - imgsensor_info.custom1.framelength) : 0;
		if (imgsensor.dummy_line < 0)
			imgsensor.dummy_line = 0;
		imgsensor.frame_length = imgsensor_info.custom1.framelength +
					 imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_CUSTOM2:
		frame_length = imgsensor_info.custom2.pclk
			       / framerate * 10 / imgsensor_info.custom2.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length > imgsensor_info.custom2.framelength)
			? (frame_length - imgsensor_info.custom2.framelength) : 0;
		if (imgsensor.dummy_line < 0)
			imgsensor.dummy_line = 0;
		imgsensor.frame_length = imgsensor_info.custom2.framelength +
					 imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
#endif
	default:		/* coding with  preview scenario by default */
		frame_length = imgsensor_info.pre.pclk
			/ framerate * 10 / imgsensor_info.pre.linelength;

		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length > imgsensor_info.pre.framelength)
			? (frame_length - imgsensor_info.pre.framelength) : 0;

		imgsensor.frame_length =
			imgsensor_info.pre.framelength + imgsensor.dummy_line;

		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		pr_debug("error scenario_id = %d, we use preview scenario\n",
		scenario_id);
		break;
	}
	return ERROR_NONE;
}


static kal_uint32 get_default_framerate_by_scenario(
	enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 *framerate)
{
	/*pr_debug("scenario_id = %d\n", scenario_id);*/

	printk("---------------------------------------get_default_framerate_by_scenario---------------------------------------\n");

	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		*framerate = imgsensor_info.pre.max_framerate;
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		*framerate = imgsensor_info.normal_video.max_framerate;
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		*framerate = imgsensor_info.cap.max_framerate;
		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		*framerate = imgsensor_info.hs_video.max_framerate;
		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		*framerate = imgsensor_info.slim_video.max_framerate;
		break;
#if S5KJN1SQ03_CUSTOM_ENABLE
	case MSDK_SCENARIO_ID_CUSTOM1:
		*framerate = imgsensor_info.custom1.max_framerate;
		break;
	case MSDK_SCENARIO_ID_CUSTOM2:
		*framerate = imgsensor_info.custom2.max_framerate;
		break;
#endif
	default:
		break;
	}

	return ERROR_NONE;
}
#if 0
static kal_uint32 set_test_pattern_mode(kal_bool enable)
{
	pr_debug("enable: %d\n", enable);

	printk("---------------------------------------set_test_pattern_mode---------------------------------------\n");

	if (enable) {
/* 0 : Normal, 1 : Solid Color, 2 : Color Bar, 3 : Shade Color Bar, 4 : PN9 */
		write_cmos_sensor(0x0600, 0x0002);
	} else {
		write_cmos_sensor(0x0600, 0x0000);
	}
	spin_lock(&imgsensor_drv_lock);
	imgsensor.test_pattern = enable;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}
#endif
static kal_uint32 set_test_pattern_mode(kal_uint32 modes,
	struct SET_SENSOR_PATTERN_SOLID_COLOR *pdata)
{
	kal_uint16 Color_R, Color_Gr, Color_Gb, Color_B;

	pr_debug("set_test_pattern enum: %d\n", modes);

	if (modes) {
		//write_cmos_sensor_8(0x0600, modes>>4);
		write_cmos_sensor(0x0600, modes);
		if (modes == 1 && (pdata != NULL)) { //Solid Color
			pr_debug("R=0x%x,Gr=0x%x,B=0x%x,Gb=0x%x",
				pdata->COLOR_R, pdata->COLOR_Gr, pdata->COLOR_B, pdata->COLOR_Gb);
			Color_R = (pdata->COLOR_R >> 22) & 0x3FF; //10bits depth color
			Color_Gr = (pdata->COLOR_Gr >> 22) & 0x3FF;
			Color_B = (pdata->COLOR_B >> 22) & 0x3FF;
			Color_Gb = (pdata->COLOR_Gb >> 22) & 0x3FF;
			write_cmos_sensor_8(0x0602, (Color_Gr >> 8) & 0x3);
			write_cmos_sensor_8(0x0603, Color_Gr & 0xFF);
			write_cmos_sensor_8(0x0604, (Color_R >> 8) & 0x3);
			write_cmos_sensor_8(0x0605, Color_R & 0xFF);
			write_cmos_sensor_8(0x0606, (Color_B >> 8) & 0x3);
			write_cmos_sensor_8(0x0607, Color_B & 0xFF);
			write_cmos_sensor_8(0x0608, (Color_Gb >> 8) & 0x3);
			write_cmos_sensor_8(0x0609, Color_Gb & 0xFF);
		}
	} else{
		/*Penang Code for EKPENAN4GU-4138 by chenxiaoyong at 20240522 start*/
		write_cmos_sensor(0x0600, 0x0000); /*No pattern*/
		/*Penang Code for EKPENAN4GU-4138 by chenxiaoyong at 20240522 end*/
	}
	    spin_lock(&imgsensor_drv_lock);
		imgsensor.test_pattern = modes;
		spin_unlock(&imgsensor_drv_lock);
		return ERROR_NONE;
}


static kal_uint32 feature_control(MSDK_SENSOR_FEATURE_ENUM feature_id,
				  UINT8 *feature_para, UINT32 *feature_para_len)
{
	UINT16 *feature_return_para_16 = (UINT16 *) feature_para;
	UINT16 *feature_data_16 = (UINT16 *) feature_para;
	UINT32 *feature_return_para_32 = (UINT32 *) feature_para;
	UINT32 *feature_data_32 = (UINT32 *) feature_para;
	//INT32 *feature_return_para_i32 = (INT32 *) feature_para;
	unsigned long long *feature_data = (unsigned long long *)feature_para;

#ifdef ENABLE_PDAF
	struct SET_PD_BLOCK_INFO_T *PDAFinfo;
	struct SENSOR_VC_INFO_STRUCT *pvcinfo;
#endif
	struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;

	MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data =
		(MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;

	//printk("---------------------------------------feature_control---------------------------------------\n");

	//pr_err("feature_id = %d\n", feature_id);
	switch (feature_id) {
	case SENSOR_FEATURE_GET_PERIOD:
		*feature_return_para_16++ = imgsensor.line_length;
		*feature_return_para_16 = imgsensor.frame_length;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
		*feature_return_para_32 = imgsensor.pclk;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_ESHUTTER:
		set_shutter(*feature_data);
		break;
	case SENSOR_FEATURE_SET_NIGHTMODE:
	/* night_mode((BOOL) *feature_data); no need to implement this mode */
		break;
	case SENSOR_FEATURE_SET_GAIN:
		set_gain((UINT16) *feature_data);
		break;
	case SENSOR_FEATURE_SET_FLASHLIGHT:
		break;
	case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
		break;

	case SENSOR_FEATURE_SET_REGISTER:
		write_cmos_sensor_8(sensor_reg_data->RegAddr,
			sensor_reg_data->RegData);
		break;

	case SENSOR_FEATURE_GET_REGISTER:
		sensor_reg_data->RegData =
			read_cmos_sensor_8(sensor_reg_data->RegAddr);
		break;

	case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
		/* get the lens driver ID from EEPROM or
		 * just return LENS_DRIVER_ID_DO_NOT_CARE
		 */
		/* if EEPROM does not exist in camera module. */
		*feature_return_para_32 = LENS_DRIVER_ID_DO_NOT_CARE;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_VIDEO_MODE:
		set_video_mode(*feature_data);
		break;
	case SENSOR_FEATURE_CHECK_SENSOR_ID:
		get_imgsensor_id(feature_return_para_32);
		break;
	case SENSOR_FEATURE_SET_AUTO_FLICKER_MODE:
		set_auto_flicker_mode((BOOL) (*feature_data_16),
					*(feature_data_16 + 1));
		break;
	case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
		set_max_framerate_by_scenario(
	    (enum MSDK_SCENARIO_ID_ENUM) *feature_data, *(feature_data + 1));
		break;

	case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
		get_default_framerate_by_scenario(
			(enum MSDK_SCENARIO_ID_ENUM) *(feature_data),
			  (MUINT32 *) (uintptr_t) (*(feature_data + 1)));
		break;
	case SENSOR_FEATURE_SET_TEST_PATTERN:
		set_test_pattern_mode((UINT32)*feature_data,
		(struct SET_SENSOR_PATTERN_SOLID_COLOR *)(uintptr_t)(*(feature_data + 1)));
		//set_test_pattern_mode((BOOL) (*feature_data));
		break;

	/* for factory mode auto testing */
	case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:
		*feature_return_para_32 = imgsensor_info.checksum_value;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_FRAMERATE:
		pr_debug("current fps :%d\n", *feature_data_32);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.current_fps = (UINT16)*feature_data_32;
		spin_unlock(&imgsensor_drv_lock);
		break;
	case SENSOR_FEATURE_SET_HDR:
		pr_debug("ihdr enable :%d\n", *feature_data_32);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.ihdr_mode = (UINT8)*feature_data_32;
		spin_unlock(&imgsensor_drv_lock);
		break;

	case SENSOR_FEATURE_GET_CROP_INFO:
		/* pr_debug("SENSOR_FEATURE_GET_CROP_INFO scenarioId:%d\n",
		 *	(UINT32) *feature_data);
		 */

		wininfo =
	(struct SENSOR_WINSIZE_INFO_STRUCT *) (uintptr_t) (*(feature_data + 1));

		switch (*feature_data_32) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			memcpy((void *)wininfo,
				(void *)&imgsensor_winsize_info[1],
			       sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			memcpy((void *)wininfo,
				(void *)&imgsensor_winsize_info[2],
			       sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[3], sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[4], sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
#if S5KJN1SQ03_CUSTOM_ENABLE
		case MSDK_SCENARIO_ID_CUSTOM1:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[5], sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CUSTOM2:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[6], sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
#endif
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			memcpy((void *)wininfo,
				(void *)&imgsensor_winsize_info[0],
			       sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		}
		break;
	case SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN:
		pr_debug("SENSOR_SET_SENSOR_IHDR LE=%d, SE=%d, Gain=%d\n",
			(UINT16) *feature_data,
			(UINT16) *(feature_data + 1),
			(UINT16) *(feature_data + 2));

/* ihdr_write_shutter_gain((UINT16)*feature_data,(UINT16)*(feature_data+1),
 * (UINT16)*(feature_data+2));
 */
		break;
	case SENSOR_FEATURE_SET_AWB_GAIN:
		break;
	case SENSOR_FEATURE_SET_HDR_SHUTTER:
		pr_debug("SENSOR_FEATURE_SET_HDR_SHUTTER LE=%d, SE=%d\n",
			(UINT16) *feature_data,
			(UINT16) *(feature_data + 1));
/* ihdr_write_shutter((UINT16)*feature_data,(UINT16)*(feature_data+1)); */
		break;

#if EEPROM_READY //stan
	case SENSOR_FEATURE_GET_4CELL_DATA:/*get 4 cell data from eeprom*/
	{
		int type = (kal_uint16)(*feature_data);
		char *data = (char *)(uintptr_t)(*(feature_data+1));
		//memset(data, 0, FOUR_CELL_SIZE + 2);
		pr_err("SENSOR_FEATURE_GET_4CELL_DATA type:%d",type);

		if (type == FOUR_CELL_CAL_TYPE_XTALK_CAL) {
			int buf_size = (kal_uint16)(*(feature_data +2));
			pr_err("zhoubiao::buf_size:%d",buf_size);
			if(buf_size < 8008){
				break;
			}
			read_4cell_from_eeprom(data);
			pr_err("Read Cross Talk = %02x %02x %02x %02x %02x %02x\n",
				(UINT16)data[0], (UINT16)data[1],
				(UINT16)data[2], (UINT16)data[3],
				(UINT16)data[4], (UINT16)data[5]);
		}
		break;
	}
#endif

#ifdef ENABLE_PDAF
	/******************** PDAF START >>> *********/
	case SENSOR_FEATURE_GET_PDAF_INFO:
		pr_debug("SENSOR_FEATURE_GET_PDAF_INFO scenarioId:%d\n", (UINT16)*feature_data);
		PDAFinfo = (struct SET_PD_BLOCK_INFO_T *)(uintptr_t)(*(feature_data+1));
		switch (*feature_data) {
			case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
				memcpy((void *)PDAFinfo, (void *)&imgsensor_pd_info, sizeof(struct SET_PD_BLOCK_INFO_T));
				break;
			case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			case MSDK_SCENARIO_ID_SLIM_VIDEO:
			default:
				break;
		}
		break;
	case SENSOR_FEATURE_GET_VC_INFO:
		pr_debug("SENSOR_FEATURE_GET_VC_INFO %d\n", (UINT16)*feature_data);
		pvcinfo = (struct SENSOR_VC_INFO_STRUCT *)(uintptr_t)(*(feature_data+1));
		switch (*feature_data_32) {
			case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			default:
				memcpy((void *)pvcinfo,(void *)&SENSOR_VC_INFO[0], sizeof(struct SENSOR_VC_INFO_STRUCT));
				break;
		}
		break;
	case SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY:
		pr_debug("SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY scenarioId:%d\n", (UINT16)*feature_data);
		//PDAF capacity enable or not
		switch (*feature_data) {
			case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
				*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
				break;
			case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
				*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
				// video & capture use same setting
				break;
			case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
				*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
				break;
			case MSDK_SCENARIO_ID_SLIM_VIDEO:
				//need to check
				*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
				break;
			case MSDK_SCENARIO_ID_CUSTOM1:
				//need to check
				*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
				break;
			case MSDK_SCENARIO_ID_CUSTOM2:
				//need to check
				*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
				break;
			case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
				*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
				break;
			default:
				*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
				break;
		}
		break;
	case SENSOR_FEATURE_SET_PDAF:
		pr_debug("PDAF mode :%d\n", *feature_data_16);
		imgsensor.pdaf_mode= *feature_data_16;
		break;
	/******************** PDAF END   <<< *********/
#endif
	case SENSOR_FEATURE_SET_SHUTTER_FRAME_TIME:
		pr_debug("SENSOR_FEATURE_SET_SHUTTER_FRAME_TIME\n");
		set_shutter_frame_length((UINT16) *feature_data, (UINT16) *(feature_data+1));
		break;
	case SENSOR_FEATURE_SET_STREAMING_SUSPEND:
		pr_debug("SENSOR_FEATURE_SET_STREAMING_SUSPEND\n");
		streaming_control(KAL_FALSE);
		break;
	case SENSOR_FEATURE_SET_STREAMING_RESUME:
		pr_debug("SENSOR_FEATURE_SET_STREAMING_RESUME, shutter:%llu\n",
			*feature_data);
		if (*feature_data != 0)
			set_shutter(*feature_data);
		streaming_control(KAL_TRUE);
		break;
	case SENSOR_FEATURE_GET_PIXEL_RATE:
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
			(imgsensor_info.cap.pclk /
			(imgsensor_info.cap.linelength - 80))*
			imgsensor_info.cap.grabwindow_width;

			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
			(imgsensor_info.normal_video.pclk /
			(imgsensor_info.normal_video.linelength - 80))*
			imgsensor_info.normal_video.grabwindow_width;

			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				(imgsensor_info.hs_video.pclk /
				 (imgsensor_info.hs_video.linelength - 80)) *
				imgsensor_info.hs_video.grabwindow_width;

			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				(imgsensor_info.slim_video.pclk /
				 (imgsensor_info.slim_video.linelength - 80)) *
				imgsensor_info.slim_video.grabwindow_width;

			break;
#if S5KJN1SQ03_CUSTOM_ENABLE
		case MSDK_SCENARIO_ID_CUSTOM1:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				(imgsensor_info.custom1.pclk /
				 (imgsensor_info.custom1.linelength - 80)) *
				imgsensor_info.custom1.grabwindow_width;

			break;
		case MSDK_SCENARIO_ID_CUSTOM2:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				(imgsensor_info.custom2.pclk /
				 (imgsensor_info.custom2.linelength - 80)) *
				imgsensor_info.custom2.grabwindow_width;

			break;
#endif
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
			(imgsensor_info.pre.pclk /
			(imgsensor_info.pre.linelength - 80))*
			imgsensor_info.pre.grabwindow_width;
			break;
		}
		break;

	case SENSOR_FEATURE_GET_MIPI_PIXEL_RATE:

		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.cap.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.normal_video.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.hs_video.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.slim_video.mipi_pixel_rate;
			break;
#if S5KJN1SQ03_CUSTOM_ENABLE
		case MSDK_SCENARIO_ID_CUSTOM1:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.custom1.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_CUSTOM2:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.custom2.mipi_pixel_rate;
			break;
#endif
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.pre.mipi_pixel_rate;
			break;
		}
		break;
	default:
		break;
	}

	return ERROR_NONE;
}				/*      feature_control()  */

static struct SENSOR_FUNCTION_STRUCT sensor_func = {
	open,
	get_info,
	get_resolution,
	feature_control,
	control,
	close
};

UINT32 S5KJN1SQ03_MIPI_RAW_SensorInit(
	struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
	/* To Do : Check Sensor status here */
	if (pfFunc != NULL)
		*pfFunc = &sensor_func;
	return ERROR_NONE;
}
