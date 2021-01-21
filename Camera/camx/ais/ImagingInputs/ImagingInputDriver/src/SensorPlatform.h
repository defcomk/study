#ifndef __SENSORPLATFORM_H_
#define __SENSORPLATFORM_H_

/**
 * @file SensorPlatform.h
 *
 * @brief Declaration of SensorPlatform camera sensor driver
 *
 * Copyright (c) 2011-2017 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 */

/*============================================================================
                        INCLUDE FILES
=========================================================================== */
#include "AEEstd.h"
#include "sensor_lib.h"
#include "CameraPlatform.h"
#include "CameraOSServices.h"

/* ===========================================================================
                     PUBLIC FUNCTION DECLARATIONS
=========================================================================== */
#define ConvertMilliToMicroSeconds(_ms_) (_ms_ * 1000)

class SensorPlatform
{
protected:
    virtual ~SensorPlatform(){};

public:
    /* ---------------------------------------------------------------------------
     *    FUNCTION        SensorPlatformInit
     *    DESCRIPTION     Initialize sensor platform with dev id
     *    DEPENDENCIES    None
     *    PARAMETERS
     *    RETURN VALUE    sensor platform ctxt ptr
     *    SIDE EFFECTS    None
     * ------------------------------------------------------------------------ */
    static SensorPlatform* SensorPlatformInit(sensor_lib_t *p_sensor_lib);

    /* ---------------------------------------------------------------------------
     *    FUNCTION        SensorPlatformDeinit
     *    DESCRIPTION     Deinitialize sensor platform with dev id
     *    DEPENDENCIES    None
     *    PARAMETERS
     *    RETURN VALUE    sensor platform ctxt ptr
     *    SIDE EFFECTS    None
     * ------------------------------------------------------------------------ */
    static void SensorPlatformDeinit(SensorPlatform*);

    /* ---------------------------------------------------------------------------
     * FUNCTION    - sensor_power_up -
     *
     * DESCRIPTION: Executes power up sequence defined in sensor library
     * ------------------------------------------------------------------------ */
    virtual CameraResult SensorPowerUp() = 0;

    /* ---------------------------------------------------------------------------
     * FUNCTION    - sensor_power_down -
     *
     * DESCRIPTION: Executes power down sequence defined in sensor library
     * ------------------------------------------------------------------------ */
    virtual CameraResult SensorPowerDown() = 0;

    /* ---------------------------------------------------------------------------
     * FUNCTION    - SensorWriteI2cSettingArray -
     *
     * DESCRIPTION: Write I2C setting array
     * ------------------------------------------------------------------------ */
    virtual CameraResult SensorWriteI2cSettingArray(struct camera_i2c_reg_setting_array *settings) = 0;

    /* ---------------------------------------------------------------------------
     * FUNCTION    - SensorSlaveWriteI2cSetting -
     *
     * DESCRIPTION: Slave Write I2C setting array
     * ------------------------------------------------------------------------ */
    virtual CameraResult SensorSlaveWriteI2cSetting(unsigned short slave_addr,
            struct camera_i2c_reg_setting *setting) = 0;

    /* ---------------------------------------------------------------------------
     * FUNCTION    - SensorSlaveI2cRead -
     *
     * DESCRIPTION: Slave read I2C reg
     * ------------------------------------------------------------------------ */
    virtual CameraResult SensorSlaveI2cRead(unsigned short slave_addr,
            struct camera_reg_settings_t *setting) = 0;

    /* ---------------------------------------------------------------------------
     * FUNCTION    - SensorWriteI2cSetting -
     *
     * DESCRIPTION: Write I2C setting
     * ------------------------------------------------------------------------ */
    virtual CameraResult SensorWriteI2cSetting(struct camera_i2c_reg_setting *setting) = 0;

    /* ---------------------------------------------------------------------------
     * FUNCTION    - SensorExecutePowerSetting -
     *
     * DESCRIPTION: Execute Sensor power config sequence
     * ------------------------------------------------------------------------ */
    virtual CameraResult SensorExecutePowerSetting(struct camera_power_setting *power_settings, unsigned short nSize) = 0;

    /* ---------------------------------------------------------------------------
     * FUNCTION    - SensorSetupGpioInterrupt -
     *
     * DESCRIPTION: Setup gpio id as interrupt
     * ------------------------------------------------------------------------ */
    virtual CameraResult SensorSetupGpioInterrupt(enum camera_gpio_type gpio_id,
            sensor_intr_callback_t cb, void *data) = 0;

    /* ---------------------------------------------------------------------------
     * FUNCTION    - SensorPowerResume -
     *
     * DESCRIPTION: Executes power resume sequence defined in sensor library
     * ------------------------------------------------------------------------ */
    virtual CameraResult SensorPowerResume() = 0;

    /* ---------------------------------------------------------------------------
     * FUNCTION    - SensorPowerSuspend -
     *
     * DESCRIPTION: Executes power suspend sequence defined in sensor library
     * ------------------------------------------------------------------------ */
    virtual CameraResult SensorPowerSuspend() = 0;
};

#endif /* __SENSORPLATFORM_H_ */
