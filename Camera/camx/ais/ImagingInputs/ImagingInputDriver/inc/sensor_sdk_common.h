/**
 * @file sensor_sdk_common.h
 *
 * Copyright (c) 2015-2019 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */
#ifndef __SENSOR_SDK_COMMON_H__
#define __SENSOR_SDK_COMMON_H__

#include "sensor_types.h"

#define NAME_SIZE_MAX           64
#define I2C_REG_SET_MAX         30
#define MAX_POWER_CONFIG        12
#define U_I2C_SEQ_REG_DATA_MAX  1024

#define GPIO_OUT_LOW            (0 << 1)
#define GPIO_OUT_HIGH           (1 << 1)
#define MAX_PDAF_WIN        200

enum sensor_camera_id {
    /// This index identifies min camera index
    CAMSENSOR_ID_MIN = 0,
    CAMSENSOR_ID_0 = CAMSENSOR_ID_MIN,
    CAMSENSOR_ID_1,
    CAMSENSOR_ID_2,
    CAMSENSOR_ID_3,
    CAMSENSOR_ID_4,
    CAMSENSOR_ID_5,
    /// This index identifies max camera index
    CAMSENSOR_ID_MAX,
};

enum camera_i2c_freq_mode {
    SENSOR_I2C_MODE_STANDARD,
    SENSOR_I2C_MODE_FAST,
    SENSOR_I2C_MODE_CUSTOM,
    SENSOR_I2C_MODE_FAST_PLUS,
    SENSOR_I2C_MODE_MAX
};

enum camera_i2c_reg_addr_type {
    CAMERA_I2C_BYTE_ADDR = 1,
    CAMERA_I2C_WORD_ADDR,
    CAMERA_I2C_3B_ADDR,
    CAMERA_I2C_ADDR_TYPE_MAX
};

enum camera_i2c_data_type {
    CAMERA_I2C_BYTE_DATA = 1,
    CAMERA_I2C_WORD_DATA,
    CAMERA_I2C_DWORD_DATA,
    CAMERA_I2C_SET_BYTE_MASK,
    CAMERA_I2C_UNSET_BYTE_MASK,
    CAMERA_I2C_SET_WORD_MASK,
    CAMERA_I2C_UNSET_WORD_MASK,
    CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA,
    CAMERA_I2C_DATA_TYPE_MAX
};

enum camera_power_seq_type {
    CAMERA_POW_SEQ_CLK,
    CAMERA_POW_SEQ_GPIO,
    CAMERA_POW_SEQ_VREG,
    CAMERA_POW_SEQ_I2C_MUX,
    CAMERA_POW_SEQ_I2C
};

enum camera_gpio_type {
    CAMERA_GPIO_INVALID,
    CAMERA_GPIO_VANA,
    CAMERA_GPIO_VDIG,
    CAMERA_GPIO_VIO,
    CAMERA_GPIO_VAF,
    CAMERA_GPIO_VAF_PWDM,
    CAMERA_GPIO_CUSTOM_REG1,
    CAMERA_GPIO_CUSTOM_REG2,
    CAMERA_GPIO_RESET,
    CAMERA_GPIO_STANDBY,
    CAMERA_GPIO_CUSTOM1,
    CAMERA_GPIO_CUSTOM2,
    CAMERA_GPIO_INTR,
    CAMERA_GPIO_PASS_PIN,
    CAMERA_GPIO_LOCK_PIN,
    CAMERA_GPIO_MAX
};

enum camera_gpio_config_type {
    CAMERA_GPIO_CONFIG_LOW = GPIO_OUT_LOW,
    CAMERA_GPIO_CONFIG_HIGH = GPIO_OUT_HIGH,
};

enum camera_gpio_trigger_type {
    CAMERA_GPIO_TRIGGER_NONE,
    CAMERA_GPIO_TRIGGER_RISING,
    CAMERA_GPIO_TRIGGER_FALLING,
    CAMERA_GPIO_TRIGGER_EDGE,
    CAMERA_GPIO_TRIGGER_LEVEL_LOW,
    CAMERA_GPIO_TRIGGER_LEVEL_HIGH
};

enum camera_gpio_intr_type
{
    CAMERA_GPIO_INTR_NONE, /**< Do not config interrupt at all */
    CAMERA_GPIO_INTR_POLL, /**< Use polling */
    CAMERA_GPIO_INTR_TLMM, /**< configure with TLMM Interrupt */
    CAMERA_GPIO_INTR_PMIC, /**< configure with PMIC Interrupt */
};

enum camera_clk_type {
    CAMERA_MCLK,
    CAMERA_CLK,
    CAMERA_CLK_MAX
};

enum camera_vreg_name {
    CAMERA_VANA,
    CAMERA_VDIG,
    CAMERA_VIO,
    CAMERA_VAF,
    CAMERA_VAF_PWDM,
    CAMERA_V_CUSTOM1,
    CAMERA_V_CUSTOM2,
    CAMERA_VREG_MAX
};

struct camera_i2c_reg_array {
    unsigned int reg_addr;
    unsigned int reg_data;
    unsigned int delay;
};

enum camera_i2c_operation {
    CAMERA_I2C_OP_WRITE = 0,
    CAMERA_I2C_OP_POLL,
    CAMERA_I2C_OP_READ,
};

struct camera_reg_settings_t {
    unsigned short reg_addr;
    enum camera_i2c_reg_addr_type addr_type;
    unsigned short reg_data;
    enum camera_i2c_data_type data_type;
    enum camera_i2c_operation i2c_operation;
    unsigned int delay;
};

struct camera_i2c_reg_setting_array {
    struct camera_i2c_reg_array reg_array[I2C_REG_SET_MAX];
    unsigned short size;
    enum camera_i2c_reg_addr_type addr_type;
    enum camera_i2c_data_type data_type;
    unsigned short delay;
};

struct camera_power_setting {
    enum camera_power_seq_type seq_type;
    unsigned short seq_val;
    long config_val;
    unsigned short delay;
};

struct camera_power_setting_array {
    struct camera_power_setting  power_up_setting_a[MAX_POWER_CONFIG];
    unsigned short size_up;
    struct camera_power_setting  power_down_setting_a[MAX_POWER_CONFIG];
    unsigned short size_down;
};

struct camera_i2c_seq_reg_array {
    unsigned short reg_addr;
    unsigned char reg_data[U_I2C_SEQ_REG_DATA_MAX];
    unsigned short reg_data_size;
};

struct camera_gpio_cfg_t
{
    uint32 dir;
    uint32 pull_dir;
    uint32 strength;
    uint32 fcn;
};

struct camera_gpio_intr_cfg_t
{
    enum camera_gpio_type gpio_id;     /**< gpio identifier that input driver will refer to */
    enum camera_gpio_intr_type intr_type; /**< how the interrupt based pin to be configured */
    uint32 pin_id;                     /**< gpio number */
    enum camera_gpio_trigger_type trigger; /**< trigger type specific to module */
    struct camera_gpio_cfg_t gpio_cfg;     /**< applicable only if module is TLMM */
};

typedef enum
{
   INPUT_SIGNAL_VALID_EVENT = 0,
   INPUT_SIGNAL_LOST_EVENT
} input_event_t;

struct camera_input_status_t
{
    uint32 src;         /** source number within a particular device **/
    input_event_t event;
    uint32 data[16]; /** optional array containg all events**/
};

#endif /* __SENSOR_SDK_COMMON_H__ */

