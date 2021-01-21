/**
 * @file adv7481.c
 *
 * @brief implementation of ADV7481 CVBS driver
 *
 * Copyright (c) 2017-2019 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 */

#include "AEEstd.h"
#include "CameraTypes.h"
#include "CameraPlatform.h"
#include "CameraSensorDeviceInterface.h"
#include "SensorDebug.h"

#include "adv7481_hw.h"
#include "adv7481.h"


#define AIS_LOG_ADV(level, fmt...) AIS_LOG(AIS_MOD_ID_ADV7481_DRV, level, fmt)

#define AIS_LOG_ADV_ERR(fmt...) AIS_LOG(AIS_MOD_ID_ADV7481_DRV, AIS_LOG_LVL_ERR, fmt)
#define AIS_LOG_ADV_WARN(fmt...) AIS_LOG(AIS_MOD_ID_ADV7481_DRV, AIS_LOG_LVL_WARN, fmt)
#define AIS_LOG_ADV_HIGH(fmt...) AIS_LOG(AIS_MOD_ID_ADV7481_DRV, AIS_LOG_LVL_HIGH, fmt)
#define AIS_LOG_ADV_MED(fmt...) AIS_LOG(AIS_MOD_ID_ADV7481_DRV, AIS_LOG_LVL_MED, fmt)
#define AIS_LOG_ADV_LOW(fmt...) AIS_LOG(AIS_MOD_ID_ADV7481_DRV, AIS_LOG_LVL_LOW, fmt)
#define AIS_LOG_ADV_DBG(fmt...) AIS_LOG(AIS_MOD_ID_ADV7481_DRV, AIS_LOG_LVL_DBG, fmt)
#define AIS_LOG_ADV_DBG1(fmt...) AIS_LOG(AIS_MOD_ID_ADV7481_DRV, AIS_LOG_LVL_DBG1, fmt)



/**
 * adv7481 i2c read/write wrapper
 */
#define ADV7481_I2C_REG_READ(p_drv, slave_addr, p_setting) \
        (((p_drv)->sp_func_tbl.sensor_i2c_slave_read)((p_drv)->p_sp_ctx, (slave_addr), (p_setting)) ? -1 : 0)
#define ADV7481_I2C_REG_WRITE(p_drv, slave_addr, p_setting) \
        (((p_drv)->sp_func_tbl.sensor_i2c_slave_write_array)((p_drv)->p_sp_ctx, (slave_addr), (p_setting)) ? -1 : 0)

/**
 * Forward declarations
 */
int adv7481_port_notify(s_adv7481_drv *p_drv, s_adv7481_port *p_port, boolean is_lock);
int adv7481_port_proc_sdp_lock(s_adv7481_drv *p_drv, s_adv7481_port *p_port,e_adv7481_sdp_lock_type type, boolean is_lock);
int adv7481_sdp_isr(s_adv7481_drv *p_drv);

/**
 * reset the chip
 *
 * @param p_drv points to the driver management structure
 * @return 0: success, otherwise fail
 *
 */
int adv7481_reset(s_adv7481_drv *p_drv)
{
    int rc;
    struct camera_i2c_reg_array reg;
    struct camera_i2c_reg_setting setting;

    AIS_LOG_ADV_MED("E %s 0x%p", __func__, p_drv);

    if (p_drv == NULL)
    {
        rc = -1;
        goto EXIT_FLAG;
    }

    setting.addr_type = CAMERA_I2C_BYTE_ADDR;
    setting.data_type = CAMERA_I2C_BYTE_DATA;
    setting.reg_array = &reg;
    setting.size = 1;

    //reset device
    reg.reg_addr = ADV7481_IO_MAP_IO_REG_FF_ADDR;
    reg.reg_data = 0xFF;
    reg.delay = 5000; //i2c hardware delay 5000us
    setting.delay = 5; //wait 5ms until reset finishes

    rc = ADV7481_I2C_REG_WRITE(p_drv, ADV7481_IO_MAP_SLAVE_ADDR, &setting);
    rc = (rc == 0) ? 0 : -2;

EXIT_FLAG:

    AIS_LOG_ADV(rc == 0 ? AIS_LOG_LVL_MED : AIS_LOG_LVL_ERR,
                "X %s 0x%p %d", __func__, p_drv, rc);

    return rc;
}

/**
 * check if the chip is supported by verifying chip id
 *
 * @param p_drv points to the driver management structure
 * @return 0: success, otherwise fail
 *
 */
int adv7481_check_chip(s_adv7481_drv *p_drv)
{
    int rc;
    uint32 chip_id = 0;
    struct camera_i2c_reg_array reg;
    struct camera_i2c_reg_setting setting;

    AIS_LOG_ADV_MED("E %s 0x%p", __func__, p_drv);

    if (p_drv == NULL)
    {
        rc = -1;
        goto EXIT_FLAG;
    }

    setting.addr_type = CAMERA_I2C_BYTE_ADDR;
    setting.data_type = CAMERA_I2C_BYTE_DATA;
    setting.reg_array = &reg;
    setting.size = 1;

    //read chip revision
    reg.reg_addr = ADV7481_IO_MAP_CHIP_REV_ID_1_ADDR;
    reg.reg_data = 0;
    reg.delay = 0;
    setting.delay = 0;
    rc = ADV7481_I2C_REG_READ(p_drv, ADV7481_IO_MAP_SLAVE_ADDR, &setting);
    if (rc != 0)
    {
        rc = -2;
        goto EXIT_FLAG;
    }
    chip_id = reg.reg_data << 8;

    reg.reg_addr = ADV7481_IO_MAP_CHIP_REV_ID_2_ADDR;
    rc = ADV7481_I2C_REG_READ(p_drv, ADV7481_IO_MAP_SLAVE_ADDR, &setting);
    if (rc != 0)
    {
        rc = -3;
        goto EXIT_FLAG;
    }
    chip_id |= reg.reg_data;

    AIS_LOG_ADV_LOW("%s:%d 0x%p 0x%x", __func__, __LINE__, p_drv, chip_id);

    //check if revision is expected
    if (chip_id != ADV7481_IO_MAP_CHIP_REV_ES1
        && chip_id != ADV7481_IO_MAP_CHIP_REV_ES2
        && chip_id != ADV7481_IO_MAP_CHIP_REV_ES3
        && chip_id != ADV7481_IO_MAP_CHIP_REV_ES3_1)
    {
        rc = -4;
        goto EXIT_FLAG;
    }

EXIT_FLAG:

    AIS_LOG_ADV(rc == 0 ? AIS_LOG_LVL_MED : AIS_LOG_LVL_ERR,
                "X %s 0x%p %d", __func__, p_drv, rc);

    return rc;
}

/**
 * config the chip and map sub tables
 *
 * @param p_drv points to the driver management structure
 * @return 0: success, otherwise fail
 *
 */
int adv7481_config(s_adv7481_drv *p_drv)
{
    int rc;
    uint8 idx = 0;
    struct camera_i2c_reg_array reg[6];
    struct camera_i2c_reg_setting setting;

    AIS_LOG_ADV_MED("E %s 0x%p", __func__, p_drv);

    if (p_drv == NULL)
    {
        rc = -1;
        goto EXIT_FLAG;
    }

    setting.addr_type = CAMERA_I2C_BYTE_ADDR;
    setting.data_type = CAMERA_I2C_BYTE_DATA;
    setting.reg_array = reg;

    idx = 0;
    //ADI required write
    reg[idx].reg_addr = ADV7481_IO_MAP_IO_REG_O1_ADDR;
    reg[idx].reg_data = 0x76;
    reg[idx].delay = 0;
    idx++;

    //disable chip powerdown - powerdown Rx
    reg[idx].reg_addr = ADV7481_IO_MAP_PWR_DOWN_CTRL_ADDR;
    reg[idx].reg_data = 0x30;
    reg[idx].delay = 0;
    idx++;

    //enable i2c read auto-increment address
    reg[idx].reg_addr = ADV7481_IO_MAP_IO_REG_F2_ADDR;
    reg[idx].reg_data = 0x01;
    reg[idx].delay = 0;
    idx++;

    //configure i2c slave address mapping for sub table
    reg[idx].reg_addr = ADV7481_IO_MAP_I2C_SLAVE_ADDR_9_ADDR;
    reg[idx].reg_data = ADV7481_SDP_MAP_SLAVE_ADDR;
    reg[idx].delay = 0;
    idx++;

    reg[idx].reg_addr = ADV7481_IO_MAP_I2C_SLAVE_ADDR_10_ADDR;
    reg[idx].reg_data = ADV7481_CSI_TXB_MAP_SLAVE_ADDR;
    reg[idx].delay = 0;
    idx++;

    reg[idx].reg_addr = ADV7481_IO_MAP_I2C_SLAVE_ADDR_11_ADDR;
    reg[idx].reg_data = ADV7481_CSI_TXA_MAP_SLAVE_ADDR;
    reg[idx].delay = 0;
    idx++;

    setting.size = idx;
    setting.delay = 0;

    rc = ADV7481_I2C_REG_WRITE(p_drv, ADV7481_IO_MAP_SLAVE_ADDR, &setting);
    rc = (rc == 0) ? 0 : -2;

EXIT_FLAG:

    AIS_LOG_ADV(rc == 0 ? AIS_LOG_LVL_MED : AIS_LOG_LVL_ERR,
                "X %s 0x%p %d", __func__, p_drv, rc);

    return rc;
}

/**
 * configure the chip to enable interrupt to host
 *
 * @param p_drv points to the driver management structure
 * @return 0: success, otherwise fail
 *
 */
int adv7481_intr_enable(s_adv7481_drv *p_drv)
{
    int rc;

    uint8 idx = 0;
    struct camera_i2c_reg_array reg[5];
    struct camera_i2c_reg_setting setting;

    AIS_LOG_ADV_LOW("E %s 0x%p", __func__, p_drv);

    if (p_drv == NULL)
    {
        rc = -1;
        goto EXIT_FLAG;
    }

    setting.addr_type = CAMERA_I2C_BYTE_ADDR;
    setting.data_type = CAMERA_I2C_BYTE_DATA;
    setting.reg_array = reg;
    setting.size = 1;
    setting.delay = 0;

    //power up INT1, which should be up by default.
    reg[idx].reg_addr = ADV7481_IO_MAP_PAD_CTRL_1_ADDR;
    reg[idx].reg_data = 0;
    reg[idx].delay = 0;
    rc = ADV7481_I2C_REG_READ(p_drv, ADV7481_IO_MAP_SLAVE_ADDR, &setting);
    if (rc != 0)
    {
        rc = -2;
        goto EXIT_FLAG;
    }

    if (reg[idx].reg_data & ADV7481_IO_MAP_PAD_CTRL_1_PDN_INT1_BMSK)
    {
        //power up INT1 if not up by default
        reg[idx].reg_data &= ~ADV7481_IO_MAP_PAD_CTRL_1_PDN_INT1_BMSK;
        rc = ADV7481_I2C_REG_WRITE(p_drv, ADV7481_IO_MAP_SLAVE_ADDR, &setting);
        if (rc != 0)
        {
            rc = -3;
            goto EXIT_FLAG;
        }
    }

    //configure INT1 interrupt
    reg[idx].reg_addr = ADV7481_IO_MAP_INT1_CONFIG_ADDR;
    reg[idx].reg_data = (0x3 << ADV7481_IO_MAP_INT1_CONFIG_INTRQ_DUR_SEL_SHFT)
                        | (0x1 << ADV7481_IO_MAP_INT1_CONFIG_INTRQ_OP_SEL_SHFT);
    reg[idx].delay = 0;

    rc = ADV7481_I2C_REG_WRITE(p_drv, ADV7481_IO_MAP_SLAVE_ADDR, &setting);
    if (rc != 0)
    {
        rc = -4;
        goto EXIT_FLAG;
    }

    //unmask INT_SD_ST
    reg[idx].reg_addr = ADV7481_IO_MAP_DATAPATH_INT_MASKB_ADDR;
    reg[idx].reg_data = 0x1 << ADV7481_IO_MAP_DATAPATH_INT_MASKB_INT_SD_MB1_SHFT;
    reg[idx].delay = 0;

    rc = ADV7481_I2C_REG_WRITE(p_drv, ADV7481_IO_MAP_SLAVE_ADDR, &setting);
    if (rc != 0)
    {
        rc = -5;
        goto EXIT_FLAG;
    }

    //CVBS interrupt:
    //set CVBS lock/unlock interrupts
    //map to SDP MAP 1
    idx = 0;
    reg[idx].reg_addr = ADV7481_SDP_MAIN_MAP_USER_MAP_RW_REG_0E_ADDR;
    reg[idx].reg_data = ADV7481_SDP_MAP_SEL_SDP_MAP_1;
    reg[idx].delay = 0;
    idx++;

    //unmask SD_LOCK_MSKB/SD_UNLOCK_MSKB
    reg[idx].reg_addr = ADV7481_SDP_MAP_1_USER_SUB_MAP_1_RW_REG_44_ADDR;
    reg[idx].reg_data = ADV7481_SDP_MAP_1_USER_SUB_MAP_1_RW_REG_44_SD_UNLOCK_MSKB_BMSK
                        | ADV7481_SDP_MAP_1_USER_SUB_MAP_1_RW_REG_44_SD_LOCK_MSKB_BMSK;
    reg[idx].delay = 0;
    idx++;

    //unmask sd_h_lock_chng_mask/sd_v_lock_chng_mask
    reg[idx].reg_addr = ADV7481_SDP_MAP_1_USER_SUB_MAP_1_RW_REG_4C_ADDR;
    reg[idx].reg_data = ADV7481_SDP_MAP_1_USER_SUB_MAP_1_RW_REG_4C_SD_H_LOCK_CHNG_MSKB_BMSK
                        | ADV7481_SDP_MAP_1_USER_SUB_MAP_1_RW_REG_4C_SD_V_LOCK_CHNG_MSKB_BMSK;
    reg[idx].delay = 0;
    idx++;

    //map to SDP MAIN MAP
    reg[idx].reg_addr = ADV7481_SDP_MAIN_MAP_USER_MAP_RW_REG_0E_ADDR;
    reg[idx].reg_data = ADV7481_SDP_MAP_SEL_SDP_MAIN_MAP;
    reg[idx].delay = 0;
    idx++;

    //enable fsc_lock
    reg[idx].reg_addr = ADV7481_SDP_MAIN_MAP_USER_MAP_RW_REG_51_ADDR;
    reg[idx].reg_data = 0xb6;
    reg[idx].delay = 0;
    idx++;

    setting.size = idx;
    setting.delay = 0;

    rc = ADV7481_I2C_REG_WRITE(p_drv, ADV7481_SDP_MAP_SLAVE_ADDR, &setting);
    rc = (rc == 0) ? 0 : -6;

EXIT_FLAG:

    AIS_LOG_ADV(rc == 0 ? AIS_LOG_LVL_LOW : AIS_LOG_LVL_ERR,
                "X %s 0x%p %d", __func__, p_drv, rc);

    return rc;
}

/**
 * configure the chip to disable interrupt to host
 *
 * @param p_drv points to the driver management structure
 * @return 0: success, otherwise fail
 *
 */
int adv7481_intr_disable(s_adv7481_drv *p_drv)
{
    int rc;

    struct camera_i2c_reg_array reg;
    struct camera_i2c_reg_setting setting;

    AIS_LOG_ADV_LOW("E %s 0x%p", __func__, p_drv);

    if (p_drv == NULL)
    {
        rc = -1;
        goto EXIT_FLAG;
    }

    setting.addr_type = CAMERA_I2C_BYTE_ADDR;
    setting.data_type = CAMERA_I2C_BYTE_DATA;
    setting.reg_array = &reg;
    setting.size = 1;
    setting.delay = 0;

    reg.reg_addr = ADV7481_IO_MAP_PAD_CTRL_1_ADDR;
    reg.reg_data = 0;
    reg.delay = 0;
    rc = ADV7481_I2C_REG_READ(p_drv, ADV7481_IO_MAP_SLAVE_ADDR, &setting);
    if (rc != 0)
    {
        rc = -2;
        goto EXIT_FLAG;
    }

    if ((reg.reg_data & ADV7481_IO_MAP_PAD_CTRL_1_PDN_INT1_BMSK) == 0)
    {
        //power down INT1
        reg.reg_data |= ADV7481_IO_MAP_PAD_CTRL_1_PDN_INT1_BMSK;
        rc = ADV7481_I2C_REG_WRITE(p_drv, ADV7481_IO_MAP_SLAVE_ADDR, &setting);
        rc = (rc == 0) ? 0 : -3;
    }

EXIT_FLAG:

    AIS_LOG_ADV(rc == 0 ? AIS_LOG_LVL_LOW : AIS_LOG_LVL_ERR,
                "X %s 0x%p %d", __func__, p_drv, rc);

    return rc;
}

/**
 * start video on the specified port
 *
 * @param p_drv points to the driver management structure
 * @param p_port points to the port management structure
 * @return 0: success, otherwise fail
 *
 */
int adv7481_port_start(s_adv7481_drv *p_drv, s_adv7481_port *p_port)
{
    int rc;
    uint32 lane = 0;
    uint32 slave_addr = 0;

    uint8 idx = 0;
    struct camera_i2c_reg_array reg[14];
    struct camera_i2c_reg_setting setting;

    AIS_LOG_ADV_LOW("E %s 0x%p 0x%p", __func__, p_drv, p_port);

    if (p_drv == NULL || p_port == NULL)
    {
        rc = -1;
        goto EXIT_FLAG;
    }

    setting.addr_type = CAMERA_I2C_BYTE_ADDR;
    setting.data_type = CAMERA_I2C_BYTE_DATA;
    setting.reg_array = reg;
    setting.size = 1;
    setting.delay = 0;


    //TODO: check if port is locked

    //map to sdp ro main map
    reg[idx].reg_addr = ADV7481_SDP_MAP_SEL_ADDR;
    reg[idx].reg_data = ADV7481_SDP_MAP_SEL_SDP_RO_MAIN_MAP;
    reg[idx].delay = 0;

    rc = ADV7481_I2C_REG_WRITE(p_drv, ADV7481_SDP_MAP_SLAVE_ADDR, &setting);
    if (rc != 0)
    {
        rc = 2;
        goto EXIT_FLAG;
    }

    reg[idx].reg_addr = ADV7481_SDP_RO_MAIN_MAP_USER_MAP_R_REG_13_ADDR;
    rc = ADV7481_I2C_REG_READ(p_drv, ADV7481_SDP_MAP_SLAVE_ADDR, &setting);
    if (rc != 0)
    {
        rc = -3;
        goto EXIT_FLAG;
    }

    AIS_LOG_ADV_DBG("%s:%d 0x%x", __func__, __LINE__, reg[idx].reg_data);

    idx = 0;
    setting.delay = 0;

    lane = ADV7481_CSI_TX_LANE(p_port->out);
    if (p_port->out & ADV7481_CSI_TXA_BMSK)
    {
        slave_addr = ADV7481_CSI_TXA_MAP_SLAVE_ADDR;
    }
    else if (p_port->out & ADV7481_CSI_TXB_BMSK)
    {
        slave_addr = ADV7481_CSI_TXB_MAP_SLAVE_ADDR;
    }

    //CSI-TXA/B Lane
    reg[idx].reg_addr = 0x0;
    reg[idx].reg_data = 0x80 | lane;
    reg[idx].delay = 0;
    idx++;

    //CSI-TXA/B set auto DPHY timing
    reg[idx].reg_addr = 0x0;
    reg[idx].reg_data = 0xA0 | lane;
    reg[idx].delay = 0;
    idx++;

    //power up MIPI DPHY
    reg[idx].reg_addr = 0xDB;
    reg[idx].reg_data = 0x10;
    reg[idx].delay = 0;
    idx++;

    //ADI required write
    reg[idx].reg_addr = 0xD6;
    reg[idx].reg_data = 0x07;
    reg[idx].delay = 0;
    idx++;

    reg[idx].reg_addr = 0xC4;
    reg[idx].reg_data = 0x0A;
    reg[idx].delay = 0;
    idx++;

    reg[idx].reg_addr = 0x71;
    reg[idx].reg_data = 0x33;
    reg[idx].delay = 0;
    idx++;

    reg[idx].reg_addr = 0x72;
    reg[idx].reg_data = 0x11;
    reg[idx].delay = 0;
    idx++;

    //i2c_dphy_pwdn - 1'b0
    reg[idx].reg_addr = 0xF0;
    reg[idx].reg_data = 0x0;
    reg[idx].delay = 0;
    idx++;

    //ADI required write
    reg[idx].reg_addr = 0x31;
    reg[idx].reg_data = 0x82;
    reg[idx].delay = 0;
    idx++;

    reg[idx].reg_addr = 0x1E;
    reg[idx].reg_data = 0x40;
    reg[idx].delay = 0;
    idx++;

    //i2c_mipi_pll_en - 1'b1
    reg[idx].reg_addr = 0xDA;
    reg[idx].reg_data = 0x01;
    reg[idx].delay = 2000;
    idx++;

    //power up CSI-TXA/B
    reg[idx].reg_addr = 0x0;
    reg[idx].reg_data = 0x20 | lane;
    reg[idx].delay = 1000;
    idx++;

    //ADI required write
    reg[idx].reg_addr = 0xC1;
    reg[idx].reg_data = 0x2B;
    reg[idx].delay = 1000;
    idx++;

    reg[idx].reg_addr = 0x31;
    reg[idx].reg_data = 0x80;
    reg[idx].delay = 0;
    idx++;

    setting.size = idx;

    rc = ADV7481_I2C_REG_WRITE(p_drv, slave_addr, &setting);
    rc = (rc == 0) ? 0 : -4;

EXIT_FLAG:

    AIS_LOG_ADV(rc == 0 ? AIS_LOG_LVL_LOW : AIS_LOG_LVL_ERR,
                "X %s 0x%p 0x%p %d", __func__, p_drv, p_port, rc);

    return rc;
}

/**
 * stop video on the specified port
 *
 * @param p_drv points to the driver management structure
 * @param p_port points to the port management structure
 * @return 0: success, otherwise fail
 *
 */
int adv7481_port_stop(s_adv7481_drv *p_drv, s_adv7481_port *p_port)
{
    int rc;
    uint32 slave_addr = 0;

    uint8 idx = 0;
    struct camera_i2c_reg_array reg[5];
    struct camera_i2c_reg_setting setting;

    AIS_LOG_ADV_LOW("E %s 0x%p 0x%p", __func__, p_drv, p_port);

    if (p_drv == NULL || p_port == NULL)
    {
        rc = -1;
        goto EXIT_FLAG;
    }

    setting.addr_type = CAMERA_I2C_BYTE_ADDR;
    setting.data_type = CAMERA_I2C_BYTE_DATA;
    setting.reg_array = reg;
    setting.delay = 0;

    if (p_port->out & ADV7481_CSI_TXA_BMSK)
    {
        slave_addr = ADV7481_CSI_TXA_MAP_SLAVE_ADDR;
    }
    else if (p_port->out & ADV7481_CSI_TXB_BMSK)
    {
        slave_addr = ADV7481_CSI_TXB_MAP_SLAVE_ADDR;
    }

    reg[idx].reg_addr = 0x31;
    reg[idx].reg_data = 0x82;
    reg[idx].delay = 0;
    idx++;

    reg[idx].reg_addr = 0x1E;
    reg[idx].reg_data = 0x0;
    reg[idx].delay = 0;
    idx++;

    //power down CSI_TXA
    reg[idx].reg_addr = 0x0;
    reg[idx].reg_data = 0x81;
    reg[idx].delay = 0;
    idx++;

    //disable mipi_pll_en control
    reg[idx].reg_addr = 0xDA;
    reg[idx].reg_data = 0x0;
    reg[idx].delay = 0;
    idx++;

    reg[idx].reg_addr = 0xC1;
    reg[idx].reg_data = 0x3B;
    reg[idx].delay = 0;
    idx++;

    setting.size = idx;

    rc = ADV7481_I2C_REG_WRITE(p_drv, slave_addr, &setting);
    rc = (rc == 0) ? 0 : -2;

EXIT_FLAG:

    AIS_LOG_ADV(rc == 0 ? AIS_LOG_LVL_LOW : AIS_LOG_LVL_ERR,
                "X %s 0x%p 0x%p %d", __func__, p_drv, p_port, rc);

    return rc;
}

/**
 * configure the specified port
 *
 * @param p_drv points to the driver management structure
 * @param p_port points to the port management structure
 * @return 0: success, otherwise fail
 *
 */
int adv7481_port_config(s_adv7481_drv *p_drv, s_adv7481_port *p_port)
{
    int rc;

    uint8 idx = 0;
    struct camera_i2c_reg_array reg[16];
    struct camera_i2c_reg_setting setting;

    AIS_LOG_ADV_MED("E %s 0x%p 0x%p", __func__, p_drv, p_port);

    if (p_drv == NULL || p_port == NULL)
    {
        rc = -1;
        goto EXIT_FLAG;
    }

    CameraResetSignal(p_port->signal);

    CameraAcquireSpinLock(p_drv->spinlock);
    p_port->flag = 1;
    CameraReleaseSpinLock(p_drv->spinlock);

    setting.addr_type = CAMERA_I2C_BYTE_ADDR;
    setting.data_type = CAMERA_I2C_BYTE_DATA;
    setting.reg_array = reg;
    setting.delay = 0;

    idx = 0;
    //disable chip powerdown - powerdown Rx
    reg[idx].reg_addr = ADV7481_IO_MAP_PWR_DOWN_CTRL_ADDR;
    reg[idx].reg_data = 0x30;
    reg[idx].delay = 0;
    idx++;

    //LLC/PIX/AUD/SPI PINS TRISTATED
    reg[idx].reg_addr = 0x0E;
    reg[idx].reg_data = 0xFF;
    reg[idx].delay = 0;
    idx++;

    setting.size = idx;

    rc = ADV7481_I2C_REG_WRITE(p_drv, ADV7481_IO_MAP_SLAVE_ADDR, &setting);
    if (rc != 0)
    {
        rc = -2;
        goto EXIT_FLAG;
    }

    idx = 0;

    //map to sdp main map
    reg[idx].reg_addr = ADV7481_SDP_MAP_SEL_ADDR;
    reg[idx].reg_data = ADV7481_SDP_MAP_SEL_SDP_MAIN_MAP;
    reg[idx].delay = 0;
    idx++;

    //sdp power up
    reg[idx].reg_addr = ADV7481_SDP_MAIN_MAP_USER_MAP_RW_REG_0F_ADDR;
    reg[idx].reg_data = 0x0;
    reg[idx].delay = 0;
    idx++;

    //ADI required write
    reg[idx].reg_addr = 0x52;
    reg[idx].reg_data = 0xCD;
    reg[idx].delay = 0;
    idx++;

    //sdp input pin
    reg[idx].reg_addr = AD7V481_SDP_MAIN_MAP_USER_MAP_RW_REG_00_ADDR;
    reg[idx].reg_data = ADV7481_CSI_TX_LANE(p_port->in);
    reg[idx].delay = 0;
    idx++;

    //ADI required write
    //secret sdp sub map
    reg[idx].reg_addr = ADV7481_SDP_MAP_SEL_ADDR;
    reg[idx].reg_data = 0x80;
    reg[idx].delay = 0;
    idx++;

    reg[idx].reg_addr = 0x9C;
    reg[idx].reg_data = 0x0;
    reg[idx].delay = 0;
    idx++;

    reg[idx].reg_addr = 0x9C;
    reg[idx].reg_data = 0xFF;
    reg[idx].delay = 0;
    idx++;

    //map to sdp main map
    reg[idx].reg_addr = ADV7481_SDP_MAP_SEL_ADDR;
    reg[idx].reg_data = ADV7481_SDP_MAP_SEL_SDP_MAIN_MAP;
    reg[idx].delay = 0;
    idx++;

    reg[idx].reg_addr = 0x80;
    reg[idx].reg_data = 0x51;
    reg[idx].delay = 0;
    idx++;

    reg[idx].reg_addr = 0x81;
    reg[idx].reg_data = 0x51;
    reg[idx].delay = 0;
    idx++;

    reg[idx].reg_addr = 0x82;
    reg[idx].reg_data = 0x68;
    reg[idx].delay = 0;
    idx++;

    //Tri-S Output Drivers, PwrDwn 656 pads
    reg[idx].reg_addr = 0x03;
    reg[idx].reg_data = 0x42;
    reg[idx].delay = 0;
    idx++;

    //power up INTRQ pad, enable SFL
    reg[idx].reg_addr = 0x04;
    reg[idx].reg_data = 0x07;
    reg[idx].delay = 0;
    idx++;

    //ADI required write
    reg[idx].reg_addr = 0x13;
    reg[idx].reg_data = 0x0;
    reg[idx].delay = 0;
    idx++;

    //select SH1
    reg[idx].reg_addr = 0x17;
    reg[idx].reg_data = 0x41;
    reg[idx].delay = 0;
    idx++;

    //ADI required write
    reg[idx].reg_addr = 0x31;
    reg[idx].reg_data = 0x12;
    reg[idx].delay = 0;
    idx++;

    setting.size = idx;
    rc = ADV7481_I2C_REG_WRITE(p_drv, ADV7481_SDP_MAP_SLAVE_ADDR, &setting);
    if (rc != 0)
    {
        rc = -3;
        goto EXIT_FLAG;
    }

    idx = 0;
    //enable 4-lane CSI Tx & Pixel output Port
    reg[idx].reg_addr = 0x10;
    reg[idx].reg_data = 0xA8;
    reg[idx].delay = 0;
    idx++;

    setting.size = idx;
    rc = ADV7481_I2C_REG_WRITE(p_drv, ADV7481_IO_MAP_SLAVE_ADDR, &setting);
    if (rc != 0)
    {
        rc = -4;
        goto EXIT_FLAG;
    }

EXIT_FLAG:

    AIS_LOG_ADV(rc == 0 ? AIS_LOG_LVL_MED : AIS_LOG_LVL_ERR,
                "X %s 0x%p 0x%p %d", __func__, p_drv, p_port, rc);

    return rc;
}

int adv7481_port_check_sdp_state(s_adv7481_drv *p_drv, s_adv7481_port *p_port, uint32 *p_state)
{
    int rc = 0;
    uint32 state = 0;
    struct camera_i2c_reg_array reg;
    struct camera_i2c_reg_setting setting;

    uint8 sd_lock_sts = 0;
    uint8 sd_h_v_lock_sts = 0;

    setting.addr_type = CAMERA_I2C_BYTE_ADDR;
    setting.data_type = CAMERA_I2C_BYTE_DATA;
    setting.reg_array = &reg;
    setting.delay = 0;
    setting.size = 1;

    AIS_LOG_ADV_DBG("E %s 0x%p 0x%p 0x%p", __func__, p_drv, p_port, p_state);

    //check SDP lock/unlock status

    //map to SDP_RO_MAP_1
    reg.reg_addr = ADV7481_SDP_MAP_SEL_ADDR;
    reg.reg_data = ADV7481_SDP_MAP_SEL_SDP_RO_MAP_1;
    reg.delay = 0;

    rc = ADV7481_I2C_REG_WRITE(p_drv, ADV7481_SDP_MAP_SLAVE_ADDR, &setting);
    if (rc != 0)
    {
        rc = -1;
        goto EXIT_FLAG;
    }

    reg.reg_addr = ADV7481_SDP_RO_MAP_1_USER_SUB_MAP_1_R_REG_49_ADDR;
    rc = ADV7481_I2C_REG_READ(p_drv, ADV7481_SDP_MAP_SLAVE_ADDR, &setting);
    if (rc != 0)
    {
        rc = -2;
        goto EXIT_FLAG;
    }

    sd_h_v_lock_sts = reg.reg_data;

    //map to SDP_RO_MAIN
    reg.reg_addr = ADV7481_SDP_MAP_SEL_ADDR;
    reg.reg_data = ADV7481_SDP_MAP_SEL_SDP_RO_MAIN_MAP;

    rc = ADV7481_I2C_REG_WRITE(p_drv, ADV7481_SDP_MAP_SLAVE_ADDR, &setting);
    if (rc != 0)
    {
        rc = -3;
        goto EXIT_FLAG;
    }

    reg.reg_addr = ADV7481_SDP_RO_MAIN_MAP_USER_MAP_R_REG_10_ADDR;
    rc = ADV7481_I2C_REG_READ(p_drv, ADV7481_SDP_MAP_SLAVE_ADDR, &setting);
    if (rc != 0)
    {
        rc = -4;
        goto EXIT_FLAG;
    }

    sd_lock_sts = reg.reg_data;

    AIS_LOG_ADV_DBG("%s:%d 0x%p 0x%p | 0x%x 0x%x", __func__, __LINE__,
        p_drv, p_port, sd_lock_sts, sd_h_v_lock_sts);

    if (sd_lock_sts & ADV7481_SDP_RO_MAIN_MAP_USER_MAP_R_REG_10_IN_LOCK_BMSK)
    {
        state |= ADV7481_SDP_LOCK_TYPE_BF(ADV7481_SDP_LOCK_TYPE_SD);
    }

    if (sd_h_v_lock_sts & ADV7481_SDP_RO_MAP_1_USER_SUB_MAP_1_R_REG_49_SD_H_LOCK_BMSK)
    {
        state |= ADV7481_SDP_LOCK_TYPE_BF(ADV7481_SDP_LOCK_TYPE_H);
    }

    if (sd_h_v_lock_sts & ADV7481_SDP_RO_MAP_1_USER_SUB_MAP_1_R_REG_49_SD_V_LOCK_BMSK)
    {
        state |= ADV7481_SDP_LOCK_TYPE_BF(ADV7481_SDP_LOCK_TYPE_V);
    }

    *p_state = state;

EXIT_FLAG:

    AIS_LOG_ADV(rc == 0 ? AIS_LOG_LVL_DBG : AIS_LOG_LVL_ERR,
                "X %s 0x%p 0x%p 0x%p %d", __func__, p_drv, p_port, p_state, rc);

    return rc;
}


/**
 * wait until the specified port is connected and locked
 *
 * @param p_drv points to the driver management structure
 * @param p_port points to the port management structure
 * @return 0: success, otherwise fail
 *
 */
int adv7481_port_wait_ready(s_adv7481_drv *p_drv, s_adv7481_port *p_port)
{
    int rc = -1;

    AIS_LOG_ADV_MED("E %s 0x%p 0x%p", __func__, p_drv, p_port);

    if (p_drv == NULL || p_port == NULL)
    {
        rc = -1;
        goto EXIT_FLAG;
    }

#ifdef ADV7481_INTR_USE_POLL
    int cnt = 0;
    while (cnt++ < 20)
    {
        uint32 state = 0;
        adv7481_port_check_sdp_state(p_drv, p_port, &state);
        if (ADV7481_SDP_IS_LOCKED(state))
        {
            rc = 0;
            break;
        }
        CameraSleep(100);
    }
#else
    rc = CameraWaitOnSignal(p_port->signal, ADV7481_PORT_TIMEOUT);
    rc = (rc == CAMERA_SUCCESS) ? 0 : -1;
#endif


    if (rc != 0)
    {
        CameraAcquireSpinLock(p_drv->spinlock);
        p_port->flag = 0;
        CameraReleaseSpinLock(p_drv->spinlock);
    }

EXIT_FLAG:

    AIS_LOG_ADV(rc == 0 ? AIS_LOG_LVL_MED : AIS_LOG_LVL_ERR,
                "X %s 0x%p 0x%p %d", __func__, p_drv, p_port, rc);

    return rc;
}

/**
 * read the field type on the specified port
 *
 * @param p_drv points to the driver management structure
 * @param p_port points to the port management structure
 * @param p_is_event points to the boolean variable indicating field type
 * @return 0: success, otherwise fail
 *
 */
int adv7481_port_read_field_type(s_adv7481_drv *p_drv, s_adv7481_port *p_port,         boolean *p_is_even)
{
    int rc = 0;
    struct camera_i2c_reg_array reg;
    struct camera_i2c_reg_setting setting;

    AIS_LOG_ADV_DBG("E %s 0x%p 0x%p 0x%p\n", __func__, p_drv, p_port, p_is_even);

    if (p_drv == NULL || p_port == NULL || p_is_even == NULL)
    {
        rc = -1;
        goto EXIT_FLAG;
    }

    setting.addr_type = CAMERA_I2C_BYTE_ADDR;
    setting.data_type = CAMERA_I2C_BYTE_DATA;
    setting.reg_array = &reg;
    setting.delay = 0;
    setting.size = 1;

    //map to SDP_RO_MAP_1
    reg.reg_addr = ADV7481_SDP_MAP_SEL_ADDR;
    reg.reg_data = ADV7481_SDP_MAP_SEL_SDP_RO_MAP_1;
    rc = ADV7481_I2C_REG_WRITE(p_drv, ADV7481_SDP_MAP_SLAVE_ADDR, &setting);
    if( rc != 0)
    {
        rc = -2;
        goto EXIT_FLAG;
    }

    reg.reg_addr = ADV7481_SDP_RO_MAP_1_USER_SUB_MAP_1_R_REG_45_ADDR;
    rc = ADV7481_I2C_REG_READ(p_drv, ADV7481_SDP_MAP_SLAVE_ADDR, &setting);
    if( rc != 0)
    {
        rc = -3;
        goto EXIT_FLAG;
    }

    *p_is_even = ((reg.reg_data & ADV7481_SDP_RO_MAP_1_USER_SUB_MAP_1_R_REG_45_EVEN_FIELD_BMSK)
                >> ADV7481_SDP_RO_MAP_1_USER_SUB_MAP_1_R_REG_45_EVEN_FIELD_SHFT);

    AIS_LOG_ADV_DBG("%s:%d %d\n", __func__, __LINE__, *p_is_even);

EXIT_FLAG:

    AIS_LOG_ADV(rc == 0 ? AIS_LOG_LVL_DBG : AIS_LOG_LVL_ERR,
                "X %s 0x%p 0x%p 0x%p %d\n", __func__, p_drv, p_port, p_is_even, rc);

    return rc;
}


/**
 * notify users the the specified port is locked or unlocked
 *
 * @param p_drv points to the driver management structure
 * @param p_port points to the port management structure
 * @param is_lock indicates if the port is locked or unlocked
 * @return 0: success, otherwise fail
 *
 */
int adv7481_port_notify(s_adv7481_drv *p_drv, s_adv7481_port *p_port, boolean is_lock)
{
    int rc = -1;
    f_callback pf;
    void *p_info;

    AIS_LOG_ADV_LOW("E %s 0x%p 0x%p %d", __func__, p_drv, p_port, is_lock);

#ifndef ADV7481_INTR_USE_POLL

    int flag = 0;

    CameraAcquireSpinLock(p_drv->spinlock);
    flag = p_port->flag;
    CameraReleaseSpinLock(p_drv->spinlock);

    if (flag && is_lock)
    {
        CameraSetSignal(p_port->signal);
    }

#endif

    CameraAcquireSpinLock(p_drv->spinlock);
    p_port->flag = 0;

    pf = p_port->cb;
    p_info = p_port->p_client;
    CameraReleaseSpinLock(p_drv->spinlock);

    if (pf != NULL)
    {
        s_adv7481_msg msg = {is_lock, p_port};
        rc = pf(&msg, p_info);
    }

    AIS_LOG_ADV(rc == 0 ? AIS_LOG_LVL_LOW : AIS_LOG_LVL_ERR,
                "X %s 0x%p 0x%p %d %d", __func__, p_drv, p_port, is_lock, rc);

    return rc;
}

/**
 * handle SDP lock/unlock events of different types
 *
 * @param p_drv points to the driver management structure
 * @param p_port points to the port management structure
 * @param type indicates different type which is processed currently
 * @param is_lock indicates if the port is locked or unlocked
 * @return 0: success, otherwise fail
 *
 */
int adv7481_port_proc_sdp_lock(s_adv7481_drv *p_drv, s_adv7481_port *p_port,
                                e_adv7481_sdp_lock_type type, boolean is_lock)
{
    int rc = 0;

    uint32 prev_status = p_port->signal_status;

    AIS_LOG_ADV_DBG("E %s 0x%p 0x%p %d %d", __func__, p_drv, p_port, type, is_lock);

    if (is_lock)
    {
        p_port->signal_status |= ADV7481_SDP_LOCK_TYPE_BF(type);
    }
    else
    {
        p_port->signal_status &= ~ADV7481_SDP_LOCK_TYPE_BF(type);
    }

    if (ADV7481_SDP_IS_LOCKED(prev_status)
        != ADV7481_SDP_IS_LOCKED(p_port->signal_status))
    {
        rc = adv7481_port_notify(p_drv, p_port, ADV7481_SDP_IS_LOCKED(p_port->signal_status));
    }

    AIS_LOG_ADV(rc == 0 ? AIS_LOG_LVL_DBG : AIS_LOG_LVL_ERR,
                "X %s 0x%p 0x%p %d %d %d", __func__, p_drv, p_port, type, is_lock, rc);

    return rc;
}

#ifdef ADV7481_INTR_USE_POLL

int adv7481_sdp_isr(s_adv7481_drv *p_drv)
{
    int rc;
    uint32 state = 0;
    s_adv7481_port *p_port;

    AIS_LOG_ADV_DBG("E %s 0x%p", __func__, p_drv);

    p_port = p_drv->port;

    rc = adv7481_port_check_sdp_state(p_drv, p_port, &state);
    if (rc != 0)
    {
        goto EXIT_FLAG;
    }

    rc = adv7481_port_proc_sdp_lock(p_drv, p_port, ADV7481_SDP_LOCK_TYPE_SD,
                                    state & ADV7481_SDP_LOCK_TYPE_BF_SD);

    rc = adv7481_port_proc_sdp_lock(p_drv, p_port, ADV7481_SDP_LOCK_TYPE_H,
                                    state & ADV7481_SDP_LOCK_TYPE_BF_H);

    rc = adv7481_port_proc_sdp_lock(p_drv, p_port, ADV7481_SDP_LOCK_TYPE_V,
                                    state & ADV7481_SDP_LOCK_TYPE_BF_V);

EXIT_FLAG:

    AIS_LOG_ADV(rc == 0 ? AIS_LOG_LVL_DBG : AIS_LOG_LVL_ERR,
                "X %s 0x%p %d", __func__, p_drv, rc);

    return rc;
}


void adv7481_isr(void *p_arg)
{
    int rc;
    s_adv7481_drv *p_drv = (s_adv7481_drv *)p_arg;

    AIS_LOG_ADV_DBG("E %s 0x%p", __func__, p_drv);

    rc = adv7481_sdp_isr(p_drv);

   AIS_LOG_ADV(rc == 0 ? AIS_LOG_LVL_DBG : AIS_LOG_LVL_ERR,
                "X %s 0x%p %d", __func__, p_drv, rc);

    return ;
}

#else

/**
 * handle SDP interrupts
 *
 * @param p_drv points to the driver management structure
 * @return 0: success, otherwise fail
 *
 */
int adv7481_sdp_isr(s_adv7481_drv *p_drv)
{
    int rc = 0;
    struct camera_i2c_reg_array reg;
    struct camera_i2c_reg_setting setting;

    s_adv7481_port *p_port;
    uint8 sd_lock_q_info = 0;
    uint8 sd_lock_sts = 0;
    uint8 sd_h_v_lock_q_info = 0;
    uint8 sd_h_v_lock_sts = 0;

    setting.addr_type = CAMERA_I2C_BYTE_ADDR;
    setting.data_type = CAMERA_I2C_BYTE_DATA;
    setting.reg_array = &reg;
    setting.delay = 0;
    setting.size = 1;

    AIS_LOG_ADV_DBG("E %s 0x%p", __func__, p_drv);

    p_port = p_drv->port;

    //check SDP lock/unlock interrupt

    //map to SDP_RO_MAP_1
    reg.reg_addr = ADV7481_SDP_MAP_SEL_ADDR;
    reg.reg_data = ADV7481_SDP_MAP_SEL_SDP_RO_MAP_1;
    reg.delay = 0;

    rc = ADV7481_I2C_REG_WRITE(p_drv, ADV7481_SDP_MAP_SLAVE_ADDR, &setting);
    if (rc != 0)
    {
        rc = -1;
        goto EXIT_FLAG;
    }

    reg.reg_addr = ADV7481_SDP_RO_MAP_1_USER_SUB_MAP_1_R_REG_42_ADDR;
    rc = ADV7481_I2C_REG_READ(p_drv, ADV7481_SDP_MAP_SLAVE_ADDR, &setting);
    if (rc != 0)
    {
        rc = -2;
        goto EXIT_FLAG;
    }

    sd_lock_q_info = reg.reg_data;

    reg.reg_addr = ADV7481_SDP_RO_MAP_1_USER_SUB_MAP_1_R_REG_4A_ADDR;
    rc = ADV7481_I2C_REG_READ(p_drv, ADV7481_SDP_MAP_SLAVE_ADDR, &setting);
    if (rc != 0)
    {
        rc = -3;
        goto EXIT_FLAG;
    }

    sd_h_v_lock_q_info = reg.reg_data;

    reg.reg_addr = ADV7481_SDP_RO_MAP_1_USER_SUB_MAP_1_R_REG_49_ADDR;
    rc = ADV7481_I2C_REG_READ(p_drv, ADV7481_SDP_MAP_SLAVE_ADDR, &setting);
    if (rc != 0)
    {
        rc = -4;
        goto EXIT_FLAG;
    }

    sd_h_v_lock_sts = reg.reg_data;


    //map to SDP_RO_MAIN
    reg.reg_addr = ADV7481_SDP_MAP_SEL_ADDR;
    reg.reg_data = ADV7481_SDP_MAP_SEL_SDP_RO_MAIN_MAP;

    rc = ADV7481_I2C_REG_WRITE(p_drv, ADV7481_SDP_MAP_SLAVE_ADDR, &setting);
    if (rc != 0)
    {
        rc = -5;
        goto EXIT_FLAG;
    }

    reg.reg_addr = ADV7481_SDP_RO_MAIN_MAP_USER_MAP_R_REG_10_ADDR;
    rc = ADV7481_I2C_REG_READ(p_drv, ADV7481_SDP_MAP_SLAVE_ADDR, &setting);
    if (rc != 0)
    {
        rc = -6;
        goto EXIT_FLAG;
    }

    sd_lock_sts = reg.reg_data;


    AIS_LOG_ADV_DBG("%s:%d 0x%p 0x%p | 0x%x 0x%x 0x%x 0x%x", __func__, __LINE__,
        p_drv, p_port, sd_lock_q_info, sd_lock_sts, sd_h_v_lock_q_info, sd_h_v_lock_sts);

    if (sd_lock_q_info
        & (ADV7481_SDP_RO_MAP_1_USER_SUB_MAP_1_R_REG_42_SD_UNLOCK_Q_BMSK
            | ADV7481_SDP_RO_MAP_1_USER_SUB_MAP_1_R_REG_42_SD_LOCK_Q_BMSK))
    {
        if (sd_lock_sts & ADV7481_SDP_RO_MAIN_MAP_USER_MAP_R_REG_10_IN_LOCK_BMSK)
        {
            if (sd_lock_q_info & ADV7481_SDP_RO_MAP_1_USER_SUB_MAP_1_R_REG_42_SD_LOCK_Q_BMSK)
            {
                rc = adv7481_port_proc_sdp_lock(p_drv, p_port, ADV7481_SDP_LOCK_TYPE_SD, TRUE);
            }
        }
        else
        {
            if (sd_lock_q_info & ADV7481_SDP_RO_MAP_1_USER_SUB_MAP_1_R_REG_42_SD_UNLOCK_Q_BMSK)
            {
                rc = adv7481_port_proc_sdp_lock(p_drv, p_port, ADV7481_SDP_LOCK_TYPE_SD, FALSE);
            }
        }

        //map to SDP_MAP_1
        reg.reg_addr = ADV7481_SDP_MAP_SEL_ADDR;
        reg.reg_data = ADV7481_SDP_MAP_SEL_SDP_MAP_1;
        rc = ADV7481_I2C_REG_WRITE(p_drv, ADV7481_SDP_MAP_SLAVE_ADDR, &setting);
        if (rc != 0)
        {
            goto EXIT_FLAG;
        }

        //clear interrupt
        reg.reg_addr = ADV7481_SDP_MAP_1_USER_SUB_MAP_1_RW_REG_43_ADDR;
        reg.reg_data = ADV7481_SDP_MAP_1_USER_SUB_MAP_1_RW_REG_43_SD_UNLOCK_CLR_BMSK
                        | ADV7481_SDP_MAP_1_USER_SUB_MAP_1_RW_REG_43_SD_LOCK_CLR_BMSK;
        rc = ADV7481_I2C_REG_WRITE(p_drv, ADV7481_SDP_MAP_SLAVE_ADDR, &setting);
    }


    if (sd_h_v_lock_q_info
        & (ADV7481_SDP_RO_MAP_1_USER_SUB_MAP_1_R_REG_4A_SD_H_LOCK_CHNG_Q_BMSK
            | ADV7481_SDP_RO_MAP_1_USER_SUB_MAP_1_R_REG_4A_SD_V_LOCK_CHNG_Q_BMSK))
    {
        if (sd_h_v_lock_q_info & ADV7481_SDP_RO_MAP_1_USER_SUB_MAP_1_R_REG_4A_SD_H_LOCK_CHNG_Q_BMSK)
        {
            rc = adv7481_port_proc_sdp_lock(p_drv, p_port, ADV7481_SDP_LOCK_TYPE_H,
                                            sd_h_v_lock_sts & ADV7481_SDP_RO_MAP_1_USER_SUB_MAP_1_R_REG_49_SD_H_LOCK_BMSK);
        }

        if (sd_h_v_lock_q_info & ADV7481_SDP_RO_MAP_1_USER_SUB_MAP_1_R_REG_4A_SD_V_LOCK_CHNG_Q_BMSK)
        {
            rc = adv7481_port_proc_sdp_lock(p_drv, p_port, ADV7481_SDP_LOCK_TYPE_V,
                                            sd_h_v_lock_sts & ADV7481_SDP_RO_MAP_1_USER_SUB_MAP_1_R_REG_49_SD_V_LOCK_BMSK);
        }

        //map to SDP_MAP_1
        reg.reg_addr = ADV7481_SDP_MAP_SEL_ADDR;
        reg.reg_data = ADV7481_SDP_MAP_SEL_SDP_MAP_1;
        rc = ADV7481_I2C_REG_WRITE(p_drv, ADV7481_SDP_MAP_SLAVE_ADDR, &setting);
        if (rc != 0)
        {
            goto EXIT_FLAG;
        }

        //clear interrupt
        reg.reg_addr = ADV7481_SDP_MAP_1_USER_SUB_MAP_1_RW_REG_4B_ADDR;
        reg.reg_data = ADV7481_SDP_MAP_1_USER_SUB_MAP_1_RW_REG_4B_SD_H_LOCK_CHNG_CLR_BMSK
                        | ADV7481_SDP_MAP_1_USER_SUB_MAP_1_RW_REG_4B_SD_V_LOCK_CHNG_CLR_BMSK;
        rc = ADV7481_I2C_REG_WRITE(p_drv, ADV7481_SDP_MAP_SLAVE_ADDR, &setting);

    }

EXIT_FLAG:

    AIS_LOG_ADV(rc == 0 ? AIS_LOG_LVL_DBG : AIS_LOG_LVL_ERR,
                "X %s 0x%p %d", __func__, p_drv, rc);

    return rc;
}

/**
 * handle all interrupts
 *
 * @param p_arg points to the driver management structure
 *
 */
void adv7481_isr(void *p_arg)
{
    int rc;
    s_adv7481_drv *p_drv = (s_adv7481_drv *)p_arg;

    struct camera_i2c_reg_array reg;
    struct camera_i2c_reg_setting setting;

    AIS_LOG_ADV_DBG("E %s 0x%p", __func__, p_drv);

    setting.addr_type = CAMERA_I2C_BYTE_ADDR;
    setting.data_type = CAMERA_I2C_BYTE_DATA;
    setting.reg_array = &reg;
    setting.delay = 0;
    setting.size = 1;

    //check intrq1 status
    reg.reg_addr = ADV7481_IO_MAP_INT_RAW_STATUS_ADDR;
    reg.reg_data = 0;
    reg.delay = 0;

    rc = ADV7481_I2C_REG_READ(p_drv, ADV7481_IO_MAP_SLAVE_ADDR, &setting);
    if (rc != 0)
    {
        rc = -1;
        goto EXIT_FLAG;
    }

    if (reg.reg_data & ADV7481_IO_MAP_INT_RAW_STATUS_INTRQ_RAW_BMSK)
    {
        //check datapath sd interrupt
        reg.reg_addr = ADV7481_IO_MAP_DATAPATH_RAW_STATUS_ADDR;
        rc = ADV7481_I2C_REG_READ(p_drv, ADV7481_IO_MAP_SLAVE_ADDR, &setting);
        if (rc != 0)
        {
            rc = -2;
            goto EXIT_FLAG;
        }

        if (reg.reg_data & ADV7481_IO_MAP_DATAPATH_RAW_STATUS_INT_SD_RAW_BMSK)
        {
            rc = adv7481_sdp_isr(p_drv);
        }

        //check datapath interrupt
        reg.reg_addr = ADV7481_IO_MAP_DATAPATH_INT_STATUS_ADDR;
        rc = ADV7481_I2C_REG_READ(p_drv, ADV7481_IO_MAP_SLAVE_ADDR, &setting);
        if (rc != 0)
        {
            rc = -4;
            goto EXIT_FLAG;
        }

        //clear datapath interrupt
        reg.reg_addr = ADV7481_IO_MAP_DATAPATH_INT_CLR_ADDR;
        reg.reg_data = ADV7481_IO_MAP_DATAPATH_INT_CLR_INT_SD_CLR_BMSK;
        rc = ADV7481_I2C_REG_WRITE(p_drv, ADV7481_IO_MAP_SLAVE_ADDR, &setting);
    }

EXIT_FLAG:

    AIS_LOG_ADV(rc == 0 ? AIS_LOG_LVL_DBG : AIS_LOG_LVL_ERR,
                "X %s 0x%p %d", __func__, p_drv, rc);

    return ;
}

#endif

/**
 * register a callback function to get notifications for interrupt events
 *
 * @param p_drv points to the driver management structure
 * @param cb points to a callback function
 * @param p_client points to the info for callback function
 * @return 0: success, otherwise fail
 *
 */
int adv7481_register_callback(s_adv7481_drv *p_drv, f_callback cb, void *p_client)
{
    int rc = 0;
    int i;

    AIS_LOG_ADV_MED("E %s 0x%p 0x%p 0x%p", __func__, p_drv, cb, p_client);

    if (p_drv == NULL || cb == NULL)
    {
        rc = -1;
        goto EXIT_FLAG;
    }

    CameraAcquireSpinLock(p_drv->spinlock);

    for (i = 0; i < ADV7481_PORT_NUM; i++)
    {
        p_drv->port[i].cb = cb;
        p_drv->port[i].p_client = p_client;
    }

    CameraReleaseSpinLock(p_drv->spinlock);

EXIT_FLAG:

    AIS_LOG_ADV(rc == 0 ? AIS_LOG_LVL_MED : AIS_LOG_LVL_ERR,
                "X %s 0x%p 0x%p 0x%p %d", __func__, p_drv, cb, p_client, rc);

    return rc;
}

/**
 * register a platform function table. i2c function would be used
 *
 * @param p_drv points to the driver management structure
 * @param p_func_tbl points to a function table
 * @param p_ctx points to the info the functions in the function table would be used
 * @return 0: success, otherwise fail
 *
 */
int adv7481_register_ext_if(s_adv7481_drv *p_drv, sensor_platform_func_table_t *p_func_tbl, void *p_ctx)
{
    int rc = 0;

    AIS_LOG_ADV_LOW("E %s 0x%p 0x%p 0x%p", __func__, p_drv, p_func_tbl, p_ctx);

    if (p_drv == NULL || p_func_tbl == NULL)
    {
        rc = -1;
        goto EXIT_FLAG;
    }

    p_drv->sp_func_tbl = *p_func_tbl;
    p_drv->p_sp_ctx = p_ctx;

EXIT_FLAG:

    AIS_LOG_ADV(rc == 0 ? AIS_LOG_LVL_LOW : AIS_LOG_LVL_ERR,
                "X %s 0x%p 0x%p 0x%p %d", __func__, p_drv, p_func_tbl, p_ctx, rc);

    return rc;
}

/**
 * initialize the chip
 *
 * @param p_drv points to the driver management structure
 * @return 0: success, otherwise fail
 *
 */
int adv7481_init(s_adv7481_drv *p_drv)
{
    int rc = 0;

    AIS_LOG_ADV_HIGH("E %s 0x%p", __func__, p_drv);

    if (p_drv == NULL)
    {
        rc = -1;
        goto EXIT_FLAG;
    }

    //reset
    rc = adv7481_reset(p_drv);
    if (rc != 0)
    {
        rc = -2;
        goto EXIT_FLAG;
    }

    //interrupt
//    rc = adv7481_intr_enable(p_drv);
//    if (rc != 0)
//    {
//        rc = -3;
//        goto EXIT_FLAG;
//    }

EXIT_FLAG:

    AIS_LOG_ADV(rc == 0 ? AIS_LOG_LVL_HIGH : AIS_LOG_LVL_ERR,
                "X %s 0x%p %d", __func__, p_drv, rc);

    return rc;
}

/**
 * deinitialize the chip
 *
 * @param p_drv points to the driver management structure
 * @return 0: success, otherwise fail
 *
 */
int adv7481_deinit(s_adv7481_drv *p_drv)
{
    int rc = 0;

    AIS_LOG_ADV_HIGH("E %s 0x%p", __func__, p_drv);

    if (p_drv == NULL)
    {
        rc = -1;
        goto EXIT_FLAG;
    }

//    rc = adv7481_intr_disable(p_drv);

EXIT_FLAG:

    AIS_LOG_ADV(rc == 0 ? AIS_LOG_LVL_HIGH : AIS_LOG_LVL_ERR,
                "X %s 0x%p %d", __func__, p_drv, rc);

    return rc;
}

/**
 * create the management structure of the chip and initialize some HW resource
 *
 * @param pp_drv points to the driver management structure which would be returned
 * @return 0: success, otherwise fail
 *
 */
int adv7481_create(s_adv7481_drv **pp_drv)
{
    int rc = 0;
    int i;

    s_adv7481_drv *p_drv = NULL;

    AIS_LOG_ADV_HIGH("E %s 0x%p", __func__, pp_drv);

    if (pp_drv == NULL)
    {
        rc = -1;
        goto EXIT_FLAG;
    }

    p_drv = (s_adv7481_drv *)CameraAllocate(CAMERA_ALLOCATE_ID_SENSOR_DRIVER_CTXT, sizeof(s_adv7481_drv));
    if (p_drv == NULL)
    {
        rc = -2;
        goto EXIT_FLAG;
    }

    //initialize the whole sturcture
    std_memset(p_drv, 0, sizeof(s_adv7481_drv));

    CameraCreateSpinLock(&p_drv->spinlock);

    for (i = 0; i < ADV7481_PORT_NUM; i++)
    {
        CameraCreateSignal(&p_drv->port[i].signal);
    }

    //configure port resource
    p_drv->port[0].in = ADV7481_ANALOG_RX_CVBS_AIN1;
    p_drv->port[0].out = ADV7481_CSI_TXA_LANE_1;

EXIT_FLAG:

    if (rc == 0)
    {
        *pp_drv = p_drv;
    }

    AIS_LOG_ADV(rc == 0 ? AIS_LOG_LVL_HIGH : AIS_LOG_LVL_ERR,
                "X %s 0x%p %d", __func__, pp_drv, rc);

    return rc;
}


/**
 * destroy the management structure of the chip and free all HW resource
 *
 * @param p_drv points to the driver management structure
 * @return 0: success, otherwise fail
 *
 */
int adv7481_destroy(s_adv7481_drv *p_drv)
{
    int rc = 0;
    int i;

    AIS_LOG_ADV_HIGH("E %s 0x%p", __func__, p_drv);

    if (p_drv == NULL)
    {
        rc = -1;
        goto EXIT_FLAG;
    }

    for (i = 0; i < ADV7481_PORT_NUM; i++)
    {
        CameraDestroySignal(p_drv->port[i].signal);
        p_drv->port[i].signal = NULL;
    }

    CameraDestroySpinLock(&p_drv->spinlock);

    CameraFree(CAMERA_ALLOCATE_ID_SENSOR_DRIVER_CTXT, p_drv);

EXIT_FLAG:

    AIS_LOG_ADV(rc == 0 ? AIS_LOG_LVL_HIGH : AIS_LOG_LVL_ERR,
                "X %s 0x%p %d", __func__, p_drv, rc);

    return rc;
}

