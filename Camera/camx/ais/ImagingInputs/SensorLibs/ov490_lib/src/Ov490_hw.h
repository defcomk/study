/**
 * @file
 *          Ov490_hw.h
 *
 * @brief
 *
 * @par Author (last changes):
 *          - Xin Huang
 *          - Phone +86 (0)21 6080 3824
 *          - xin.huang@continental-corporation.com
 *
 * @par Project Leader:
 *          - Pei Yu
 *          - Phone +86 (0)21 6080 3469
 *          - Yu.2.Pei@continental-corporation.com
 *
 * @par Responsible Architect:
 *          - Xin Huang
 *          - Phone +86 (0)21 6080 3824
 *          - xin.huang@continental-corporation.com
 *
 * @par Project:
 *          DiDi
 *
 * @par SW-Component:
 *          ov490 chip header file
 *
 * @par SW-Package:
 *          libais_ov490.so
 *
 * @par SW-Module:
 *          AIS Camera HAL
 *
 * @par Description:
 *          Bridge driver  for DiDi board ov490 ISP  in AIS 
 * @note
 *
 * @par Module-History:
 *
 * @verbatim
 *
 *  Date            Author                  Reason
 *  18.03.2019      Xin Huang             Initial version.


 *
 * @endverbatim
 *
 * @par Copyright Notice:
 *
 * Copyright (c) Continental AG and subsidiaries 2015
 * Continental Automotive Holding (Shanghai)
 * Alle Rechte vorbehalten. All Rights Reserved.

 * The reproduction, transmission or use of this document or its contents is
 * not permitted without express written authority.
 * Offenders will be liable for damages. All rights, including rights created
 * by patent grant or registration of a utility model or design, are reserved.
 */

#ifndef __OV490_HW_H__
#define __OV490_HW_H__

/*SCCBID0pin =0 and SCCVID1pin = 0*/
#define OV490_SCCB_SLAVE_ID_SCCBID00    (0x48)

/*SCCBID0pin =1 and SCCVID1pin = 0*/
#define OV490_SCCB_SLAVE_ID_SCCBID10    (0x46)

/*SCCBID0pin =0 and SCCVID1pin = 1*/
#define OV490_SCCB_SLAVE_ID_SCCBID01    (0x4A)

/*SCCBID0pin =1 and SCCVID1pin = 1*/     
#define OV490_SCCB_SLAVE_ID_SCCBID11    (0x4C)

/*FSIN0pin = 0 and FSIN1pin = 0*/
#define OV490_BOOTMODE_SCCBSLAVE        (0)

/*FSIN0pin = 1 and FSIN1pin = 0*/
#define OV490_BOOTMODE_SCCBMASTER       (1)

/*FSIN0pin = 0 and FSIN1pin = 1*/
#define OV490_BOOTMODE_SPIMASTER        (2)

/*FSIN0pin = 1 and FSIN1pin = 1*/
#define OV490_BOOTMODE_SPISLAVE         (3)


/*Boot flag */
#define SCCB_BOOT_RDY                  (0xa1)
#define SCCB_BOOT_JUMPBOOT_START       (0xc2)
#define SCCB_BOOT_JUMPBOOT_START_CACHE (0xc5)
#define SCCB_BOOT_JUMPBOOT_OK          (0xc3)
#define SCCB_BOOT_JUMPBOOT_ERR         (0xc4)
#define FW_RUNNING_STAGE_INIT          (0x01)
#define FW_RUNNING_STAGE_SNR_INIT_DONE (0x11)
#define FW_RUNNING_STAGE_FINAL_DONE    (0x02)
#define FW_RUNNING_STAGE_SNR_ID_ERR    (0xEE)
#define FW_RUNNING_STAGE_SLAVEID_NACK  (0xFF)
#define HOSTCMD_RETURN_NO_ERROR        (0x99)


/*Address*/
#define SRAM_FLAG_ADDR    (0x8019ffc8)
#define SRAM_FW_ADDR_BOOT (0x8019ffc4)
#define SRAM_FW_START     (0x801900f8)
#define ADDR_FW_BOOT      (0x00190000)
#define SCCB_BURST_ADDR   (0x802a6000)
#define FW_RUNNING_STAGE_ADDR (0x80800120)



#define OV490_REGADDR_CHIPID_H (0x300a)
#define OV490_REGADDR_CHIPID_L (0x300b)
/*chip id high*/ 
#define CHIP_ID_H (0x04)
/*chip id low*/  
#define CHIP_ID_L (0x90)

#define OV490_REGADDR_SCCB_BURST_ADDR (0x6000)
#define OV490_REGADDR_RUNNING_STAGE_ADDR (0x0120)
#define OV490_REGADDR_FW_BOOT (0x0000)
#define OV490_REGADDR_SRAM_FLAG_ADDR (0xffc8)
#define OV490_REGADDR_SRAM_FW_ADDR_BOOT (0xffc4)
#define OV490_REGADDR_SRAM_FW_START (0x00f8)
#define OV490_REGADDR_HOSTCMD_RETURN (0x5ffc)
#define OV490_REGADDR_ISP_TICK_HIGH   (0x0104)
#define OV490_REGADDR_ISP_TICK_LOW    (0x0105)
#define OV490_REGADDR_FW_VERSION_HIGH (0x0102)
#define OV490_REGADDR_FW_VERSION_LOW  (0x0103)
#define OV490_REGADDR_COLOR_FORMAT  (0x6010)  //head addr 8029

#endif

