#ifndef _AIS_MODULE_H_
#define _AIS_MODULE_H_
/*
 * @file ais_module.h
 *
 * @brief defines all modules id.
 *
 * Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#ifdef __cplusplus
extern "C" {
#endif


#define AIS_MOD_ID_RSVR                     0            //reserved



#define AIS_MOD_ID_CLIENT                   10            //ais client
#define AIS_MOD_ID_SERVER                   11            //ais server

#define AIS_MOD_ID_CONN_HYPERVISOR          12            //hypervisor mmhab
#define AIS_MOD_ID_CONN_INTEGRITY           13            //integrity connection
#define AIS_MOD_ID_CONN_LINUX               14            //linux socket
#define AIS_MOD_ID_CONN_QNX                 15            //qnx socket

#define AIS_MOD_ID_EVENT_QUEUE              16            //event queue


#define AIS_MOD_ID_ENGINE                   20            //ais engine



#define AIS_MOD_ID_CCI_DRV                  30            //cci driver
#define AIS_MOD_ID_IFE_DRV                  31
#define AIS_MOD_ID_CSID_DRV                 32
#define AIS_MOD_ID_CSIPHY_DRV               33
#define AIS_MOD_ID_ICP_DRV                  34


#define AIS_MOD_ID_ADV7481_DRV              40            //adv7481 driver
#define AIS_MOD_ID_ADV7481_LIB              41            //adv7481 lib
#define AIS_MOD_ID_TI960_LIB                42            //ti960 lib
//STATS
#define AIS_MOD_ID_AEC                      50
#define AIS_MOD_ID_AWB                      51
#define AIS_MOD_ID_AF                       52
#define AIS_MOD_ID_Q3A                      53
#define AIS_MOD_ID_ASD                      54
#define AIS_MOD_ID_AFD                      55

#define AIS_MOD_ID_QCARCAM_TEST             100          //qcarcam test


#define AIS_MOD_ID_DIAG_SERVER              120          // diagnostic server
#define AIS_MOD_ID_DIAG_CLIENT              121          // diagnostic client

#define AIS_MOD_CHI_MOD                     130          // CHI module(camx)
#define AIS_MOD_CHI_PIPELINE                131          // CHI Pipeline(camx)
#define AIS_MOD_CHI_SESSION                 132          // CHI Session(camx)

#define AIS_MOD_CHI_LOG                     133          // CHI generic log(camx)

#define AIS_MOD_ID_MAX_NUM                  256          //maximum number of module id

#ifdef __cplusplus
}
#endif

#endif //_AIS_MODULE_H_
