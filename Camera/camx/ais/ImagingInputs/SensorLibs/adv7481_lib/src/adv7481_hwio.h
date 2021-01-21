#ifndef _ADV7481_HWIO_H_
#define _ADV7481_HWIO_H_

/**
 * @file adv7481_hwio.h
 * @brief defines registers of adv7481
 *        please check the user guide of adv7481 for details
 *
 * Copyright (c) 2017 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

/** IO Map */
#define ADV7481_IO_MAP_PWR_DOWN_CTRL_ADDR                                   0x00
#define ADV7481_IO_MAP_IO_REG_O1_ADDR                                       0x01
#define ADV7481_IO_MAP_CHIP_REV_ID_1_ADDR                                   0xDF
#define ADV7481_IO_MAP_CHIP_REV_ID_2_ADDR                                   0xE0
#define ADV7481_IO_MAP_I2C_SLAVE_ADDR_9_ADDR                                0xFB
#define ADV7481_IO_MAP_I2C_SLAVE_ADDR_10_ADDR                               0xFC
#define ADV7481_IO_MAP_I2C_SLAVE_ADDR_11_ADDR                               0xFD
#define ADV7481_IO_MAP_IO_REG_F2_ADDR                                       0xF2
#define ADV7481_IO_MAP_IO_REG_FF_ADDR                                       0xFF

#define ADV7481_IO_MAP_PAD_CTRL_1_ADDR                                      0x1D
#define ADV7481_IO_MAP_PAD_CTRL_1_PDN_INT1_BMSK                             0x80
#define ADV7481_IO_MAP_PAD_CTRL_1_PDN_INT1_SHFT                             7

#define ADV7481_IO_MAP_INT_RAW_STATUS_ADDR                                  0x3F
#define ADV7481_IO_MAP_INT_RAW_STATUS_INTRQ_RAW_BMSK                        0x01
#define ADV7481_IO_MAP_INT_RAW_STATUS_INTRQ_RAW_SHFT                        0

#define ADV7481_IO_MAP_INT1_CONFIG_ADDR                                     0x40
#define ADV7481_IO_MAP_INT1_CONFIG_INTRQ_DUR_SEL_BMSK                       0xC0
#define ADV7481_IO_MAP_INT1_CONFIG_INTRQ_DUR_SEL_SHFT                       6
#define ADV7481_IO_MAP_INT1_CONFIG_INTRQ_OP_SEL_BMSK                        0x03
#define ADV7481_IO_MAP_INT1_CONFIG_INTRQ_OP_SEL_SHFT                        0


#define ADV7481_IO_MAP_DATAPATH_RAW_STATUS_ADDR                             0x43
#define ADV7481_IO_MAP_DATAPATH_RAW_STATUS_INT_SD_RAW_BMSK                  0x01
#define ADV7481_IO_MAP_DATAPATH_RAW_STATUS_INT_SD_RAW_SHFT                  0

#define ADV7481_IO_MAP_DATAPATH_INT_STATUS_ADDR                             0x44
#define ADV7481_IO_MAP_DATAPATH_INT_STATUS_INT_SD_ST_BMSK                   0x01
#define ADV7481_IO_MAP_DATAPATH_INT_STATUS_INT_SD_ST_SHFT                   0

#define ADV7481_IO_MAP_DATAPATH_INT_CLR_ADDR                                0x45
#define ADV7481_IO_MAP_DATAPATH_INT_CLR_INT_SD_CLR_BMSK                     0x01
#define ADV7481_IO_MAP_DATAPATH_INT_CLR_INT_SD_CLR_SHFT                     0

#define ADV7481_IO_MAP_DATAPATH_INT_MASKB_ADDR                              0x47
#define ADV7481_IO_MAP_DATAPATH_INT_MASKB_INT_SD_MB1_BMSK                   0x01
#define ADV7481_IO_MAP_DATAPATH_INT_MASKB_INT_SD_MB1_SHFT                   0


/** SDP Map */
#define ADV7481_SDP_MAP_SEL_ADDR                                            0x0E


#define AD7V481_SDP_MAIN_MAP_USER_MAP_RW_REG_00_ADDR                        0x00
#define ADV7481_SDP_MAIN_MAP_USER_MAP_RW_REG_0E_ADDR                        0x0E
#define ADV7481_SDP_MAIN_MAP_USER_MAP_RW_REG_0F_ADDR                        0x0F
#define ADV7481_SDP_MAIN_MAP_USER_MAP_RW_REG_51_ADDR                        0x51


#define ADV7481_SDP_RO_MAIN_MAP_USER_MAP_R_REG_10_ADDR                      0x10
#define ADV7481_SDP_RO_MAIN_MAP_USER_MAP_R_REG_10_IN_LOCK_BMSK              0x01
#define ADV7481_SDP_RO_MAIN_MAP_USER_MAP_R_REG_10_IN_LOCK_SHFT              0

#define ADV7481_SDP_RO_MAIN_MAP_USER_MAP_R_REG_13_ADDR                      0x13


#define ADV7481_SDP_MAP_1_USER_SUB_MAP_1_RW_REG_43_ADDR                     0x43
#define ADV7481_SDP_MAP_1_USER_SUB_MAP_1_RW_REG_43_SD_UNLOCK_CLR_BMSK       0x02
#define ADV7481_SDP_MAP_1_USER_SUB_MAP_1_RW_REG_43_SD_UNLOCK_CLR_SHFT       1
#define ADV7481_SDP_MAP_1_USER_SUB_MAP_1_RW_REG_43_SD_LOCK_CLR_BMSK         0x01
#define ADV7481_SDP_MAP_1_USER_SUB_MAP_1_RW_REG_43_SD_LOCK_CLR_SHFT         0

#define ADV7481_SDP_MAP_1_USER_SUB_MAP_1_RW_REG_44_ADDR                     0x44
#define ADV7481_SDP_MAP_1_USER_SUB_MAP_1_RW_REG_44_SD_UNLOCK_MSKB_BMSK      0x02
#define ADV7481_SDP_MAP_1_USER_SUB_MAP_1_RW_REG_44_SD_UNLOCK_MSKB_SHFT      1
#define ADV7481_SDP_MAP_1_USER_SUB_MAP_1_RW_REG_44_SD_LOCK_MSKB_BMSK        0x01
#define ADV7481_SDP_MAP_1_USER_SUB_MAP_1_RW_REG_44_SD_LOCK_MSKB_SHFT        0

#define ADV7481_SDP_MAP_1_USER_SUB_MAP_1_RW_REG_4B_ADDR                     0x4B
#define ADV7481_SDP_MAP_1_USER_SUB_MAP_1_RW_REG_4B_SD_H_LOCK_CHNG_CLR_BMSK  0x04
#define ADV7481_SDP_MAP_1_USER_SUB_MAP_1_RW_REG_4B_SD_H_LOCK_CHNG_CLR_SHFT  2
#define ADV7481_SDP_MAP_1_USER_SUB_MAP_1_RW_REG_4B_SD_V_LOCK_CHNG_CLR_BMSK  0x02
#define ADV7481_SDP_MAP_1_USER_SUB_MAP_1_RW_REG_4B_SD_V_LOCK_CHNG_CLR_SHFT  1

#define ADV7481_SDP_MAP_1_USER_SUB_MAP_1_RW_REG_4C_ADDR                     0x4C
#define ADV7481_SDP_MAP_1_USER_SUB_MAP_1_RW_REG_4C_SD_H_LOCK_CHNG_MSKB_BMSK 0x04
#define ADV7481_SDP_MAP_1_USER_SUB_MAP_1_RW_REG_4C_SD_H_LOCK_CHNG_MSKB_SHFT 2
#define ADV7481_SDP_MAP_1_USER_SUB_MAP_1_RW_REG_4C_SD_V_LOCK_CHNG_MSKB_BMSK 0x02
#define ADV7481_SDP_MAP_1_USER_SUB_MAP_1_RW_REG_4C_SD_V_LOCK_CHNG_MSKB_SHFT 1


#define ADV7481_SDP_RO_MAP_1_USER_SUB_MAP_1_R_REG_42_ADDR                   0x42
#define ADV7481_SDP_RO_MAP_1_USER_SUB_MAP_1_R_REG_42_SD_UNLOCK_Q_BMSK       0x02
#define ADV7481_SDP_RO_MAP_1_USER_SUB_MAP_1_R_REG_42_SD_UNLOCK_Q_SHFT       1
#define ADV7481_SDP_RO_MAP_1_USER_SUB_MAP_1_R_REG_42_SD_LOCK_Q_BMSK         0x01
#define ADV7481_SDP_RO_MAP_1_USER_SUB_MAP_1_R_REG_42_SD_LOCK_Q_SHFT         0

#define ADV7481_SDP_RO_MAP_1_USER_SUB_MAP_1_R_REG_49_ADDR                   0x49
#define ADV7481_SDP_RO_MAP_1_USER_SUB_MAP_1_R_REG_49_SD_H_LOCK_BMSK         0x04
#define ADV7481_SDP_RO_MAP_1_USER_SUB_MAP_1_R_REG_49_SD_H_LOCK_SHFT         2
#define ADV7481_SDP_RO_MAP_1_USER_SUB_MAP_1_R_REG_49_SD_V_LOCK_BMSK         0x02
#define ADV7481_SDP_RO_MAP_1_USER_SUB_MAP_1_R_REG_49_SD_V_LOCK_SHFT         1

#define ADV7481_SDP_RO_MAP_1_USER_SUB_MAP_1_R_REG_4A_ADDR                   0x4A
#define ADV7481_SDP_RO_MAP_1_USER_SUB_MAP_1_R_REG_4A_SD_H_LOCK_CHNG_Q_BMSK  0x04
#define ADV7481_SDP_RO_MAP_1_USER_SUB_MAP_1_R_REG_4A_SD_H_LOCK_CHNG_Q_SHFT  2
#define ADV7481_SDP_RO_MAP_1_USER_SUB_MAP_1_R_REG_4A_SD_V_LOCK_CHNG_Q_BMSK  0x02
#define ADV7481_SDP_RO_MAP_1_USER_SUB_MAP_1_R_REG_4A_SD_V_LOCK_CHNG_Q_SHFT  1

#define ADV7481_SDP_RO_MAP_1_USER_SUB_MAP_1_R_REG_45_ADDR                   0x45
#define ADV7481_SDP_RO_MAP_1_USER_SUB_MAP_1_R_REG_45_EVEN_FIELD_BMSK        0x10
#define ADV7481_SDP_RO_MAP_1_USER_SUB_MAP_1_R_REG_45_EVEN_FIELD_SHFT        4
#endif //_ADV7481_HWIO_H_
