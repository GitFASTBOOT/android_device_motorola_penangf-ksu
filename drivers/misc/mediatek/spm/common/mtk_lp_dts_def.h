/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2016 MediaTek Inc.
 */

#ifndef __MTK_LP_DTS_DEF_H__
#define __MTK_LP_DTS_DEF_H__

enum MTK_LP_FEATURE_DTS {
	MTK_LP_FEATURE_DTS_SODI = 0,
	MTK_LP_FEATURE_DTS_SODI3,
	MTK_LP_FEATURE_DTS_DP,
	MTK_LP_FEATURE_DTS_SUSPEND,
};

#define MTK_OF_PROPERTY_STATUS_FOUND	(1<<0U)
#define MTK_OF_PROPERTY_VALUE_ENABLE	(1<<1U)
#define MTK_LP_FEATURE_DTS_NAME_SODI	"sodi"
#define MTK_LP_FEATURE_DTS_NAME_SODI3	"sodi3"
#define MTK_LP_FEATURE_DTS_NAME_DP	"dpidle"
#define MTK_LP_FEATURE_DTS_NAME_SUSPEND	"suspend"

#define MTK_LP_FEATURE_DTS_PROPERTY_IDLE_NODE	"idle-states"
#define MTK_LP_SPM_DTS_COMPATIABLE_NODE			"mediatek,sleep"
#endif