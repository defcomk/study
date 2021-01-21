/**
 * @file ar0231_ext_isp.c
 *
 * Copyright (c) 2019 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#include "ar0231_ext_isp.h"

#define SENSOR_WIDTH    1920
// The sensor embeds extra 4 lines of metadata with ISP parameters, this is the default behaviour
// Adding this by default, if possible to query, will add code to this later
#define SENSOR_HEIGHT   1024

#define CAM_SENSOR_DEFAULT_ADDR         0xE0

/*CONFIGURATION OPTIONS*/

#define _ar0231_ext_isp_delay_ 0
#define MAX9295_LINK_RESET_DELAY 100000

/**
 * For UYVY 8bpp set to QCARCAM_FMT_UYVY_8 / CSI_DT_RAW8
 * For UYVY 10bpp set to QCARCAM_FMT_UYVY_10 / CSI_DT_RAW10
 */
#define FMT_9296_LINK_A   QCARCAM_FMT_UYVY_8
#define FMT_9296_LINK_B   QCARCAM_FMT_UYVY_8
#define DT_9296_LINK_A    CSI_DT_RAW8
#define DT_9296_LINK_B    CSI_DT_RAW8

#define CID_VC0        0
#define CID_VC1        4


/*
Reg 1B0 - 1B9
Use ‘crossbar’ in the serializer move bits from 10 to 8, moving last two LSB to MSB

9 .... 0 -> 1098765432 in serialzer (cross bar)
10 | 98765432 (8 bits) in deser (truncate)

Deseralizer is programmed to truncate from 10 to 8 (in MSB). Effective, you get 9 - 2, truncating just the 2 LSB bits
*/
#define CAM_SER_INIT_A_8BIT \
{ \
    { 0x0002, 0x03, _ar0231_ext_isp_delay_ }, \
    { 0x0100, 0x60, _ar0231_ext_isp_delay_ }, \
    { 0x0101, 0x0A, _ar0231_ext_isp_delay_ }, \
    { 0x01B0, 0x02, _ar0231_ext_isp_delay_ }, \
    { 0x01B1, 0x03, _ar0231_ext_isp_delay_ }, \
    { 0x01B2, 0x04, _ar0231_ext_isp_delay_ }, \
    { 0x01B3, 0x05, _ar0231_ext_isp_delay_ }, \
    { 0x01B4, 0x06, _ar0231_ext_isp_delay_ }, \
    { 0x01B5, 0x07, _ar0231_ext_isp_delay_ }, \
    { 0x01B6, 0x08, _ar0231_ext_isp_delay_ }, \
    { 0x01B7, 0x09, _ar0231_ext_isp_delay_ }, \
    { 0x01B8, 0x00, _ar0231_ext_isp_delay_ }, \
    { 0x01B9, 0x01, _ar0231_ext_isp_delay_ }, \
    { 0x0053, 0x00, _ar0231_ext_isp_delay_ }, \
    { 0x0102, 0x0E, _ar0231_ext_isp_delay_ }, \
    { 0x0311, 0x10, _ar0231_ext_isp_delay_ }, \
}

#define CAM_SER_INIT_B_8BIT \
{ \
    { 0x0002, 0x03, _ar0231_ext_isp_delay_ }, \
    { 0x0100, 0x60, _ar0231_ext_isp_delay_ }, \
    { 0x0101, 0x0A, _ar0231_ext_isp_delay_ }, \
    { 0x01B0, 0x02, _ar0231_ext_isp_delay_ }, \
    { 0x01B1, 0x03, _ar0231_ext_isp_delay_ }, \
    { 0x01B2, 0x04, _ar0231_ext_isp_delay_ }, \
    { 0x01B3, 0x05, _ar0231_ext_isp_delay_ }, \
    { 0x01B4, 0x06, _ar0231_ext_isp_delay_ }, \
    { 0x01B5, 0x07, _ar0231_ext_isp_delay_ }, \
    { 0x01B6, 0x08, _ar0231_ext_isp_delay_ }, \
    { 0x01B7, 0x09, _ar0231_ext_isp_delay_ }, \
    { 0x01B8, 0x00, _ar0231_ext_isp_delay_ }, \
    { 0x01B9, 0x01, _ar0231_ext_isp_delay_ }, \
    { 0x0053, 0x01, _ar0231_ext_isp_delay_ }, \
    { 0x0102, 0x0E, _ar0231_ext_isp_delay_ }, \
    { 0x0311, 0x10, _ar0231_ext_isp_delay_ }, \
}


#define CAM_SER_INIT_A_10BIT \
{ \
    { 0x0002, 0x03, _ar0231_ext_isp_delay_ }, \
    { 0x0100, 0x60, _ar0231_ext_isp_delay_ }, \
    { 0x0101, 0x0A, _ar0231_ext_isp_delay_ }, \
    { 0x0053, 0x00, _ar0231_ext_isp_delay_ }, \
    { 0x0102, 0x0E, _ar0231_ext_isp_delay_ }, \
    { 0x0311, 0x10, _ar0231_ext_isp_delay_ }, \
}

#define CAM_SER_INIT_B_10BIT \
{ \
    { 0x0002, 0x03, _ar0231_ext_isp_delay_ }, \
    { 0x0100, 0x60, _ar0231_ext_isp_delay_ }, \
    { 0x0101, 0x0A, _ar0231_ext_isp_delay_ }, \
    { 0x0053, 0x01, _ar0231_ext_isp_delay_ }, \
    { 0x0102, 0x0E, _ar0231_ext_isp_delay_ }, \
    { 0x0311, 0x10, _ar0231_ext_isp_delay_ }, \
}

#define CAM_SER_START \
{ \
    { 0x0007, 0x37, _ar0231_ext_isp_delay_ }, \
    { 0x0002, 0x13, 20000 }, \
}

#define CAM_SER_STOP \
{ \
    { 0x0002, 0x03, 20000 }, \
}

#define CAM_SER_ADDR_CHNG_A \
{ \
    { 0x006B, 0x10, _max9296_delay_ }, \
    { 0x0073, 0x11, _max9296_delay_ }, \
    { 0x007B, 0x30, _max9296_delay_ }, \
    { 0x0083, 0x30, _max9296_delay_ }, \
    { 0x0093, 0x30, _max9296_delay_ }, \
    { 0x009B, 0x30, _max9296_delay_ }, \
    { 0x00A3, 0x30, _max9296_delay_ }, \
    { 0x00AB, 0x30, _max9296_delay_ }, \
    { 0x008B, 0x30, _max9296_delay_ }, \
}

#define CAM_SER_ADDR_CHNG_B \
{ \
    { 0x006B, 0x12, _max9296_delay_ }, \
    { 0x0073, 0x13, _max9296_delay_ }, \
    { 0x007B, 0x32, _max9296_delay_ }, \
    { 0x0083, 0x32, _max9296_delay_ }, \
    { 0x0093, 0x32, _max9296_delay_ }, \
    { 0x009B, 0x32, _max9296_delay_ }, \
    { 0x00A3, 0x32, _max9296_delay_ }, \
    { 0x00AB, 0x32, _max9296_delay_ }, \
    { 0x008B, 0x32, _max9296_delay_ }, \
}


static int ar0231_ext_isp_detect(max9296_context_t* ctxt, uint32 link);
static int ar0231_ext_isp_get_link_cfg(max9296_context_t* ctxt, uint32 link, max9296_link_cfg_t* p_cfg);
static int ar0231_ext_isp_init_link(max9296_context_t* ctxt, uint32 link);
static int ar0231_ext_isp_start_link(max9296_context_t* ctxt, uint32 link);
static int ar0231_ext_isp_stop_link(max9296_context_t* ctxt, uint32 link);

max9296_sensor_t ar0231_ext_isp_info = {
    .id = MAXIM_SENSOR_ID_AR0231_EXT_ISP,
    .detect = ar0231_ext_isp_detect,
    .get_link_cfg = ar0231_ext_isp_get_link_cfg,

    .init_link = ar0231_ext_isp_init_link,
    .start_link = ar0231_ext_isp_start_link,
    .stop_link = ar0231_ext_isp_stop_link,
};


static struct camera_i2c_reg_setting cam_ser_reg_setting =
{
    .reg_array = NULL,
    .size = 0,
    .addr_type = CAMERA_I2C_WORD_ADDR,
    .data_type = CAMERA_I2C_BYTE_DATA,
};


static struct camera_i2c_reg_array max9295_gmsl_0[] = CAM_SER_ADDR_CHNG_A;
static struct camera_i2c_reg_array max9295_gmsl_1[] = CAM_SER_ADDR_CHNG_B;
static struct camera_i2c_reg_array max9295_init_8bit_regs_0[] = CAM_SER_INIT_A_8BIT;
static struct camera_i2c_reg_array max9295_init_8bit_regs_1[] = CAM_SER_INIT_B_8BIT;
static struct camera_i2c_reg_array max9295_init_10bit_regs_0[] = CAM_SER_INIT_A_10BIT;
static struct camera_i2c_reg_array max9295_init_10bit_regs_1[] = CAM_SER_INIT_B_10BIT;
static struct camera_i2c_reg_array max9295_start_reg[] = CAM_SER_START;
static struct camera_i2c_reg_array max9295_stop_reg[] = CAM_SER_STOP;

// List of serializer addresses we support
static uint16 supported_ser_addr[] = {0xC4, 0x88, 0x80};

static maxim_pipeline_t ar0231_ext_isp_pipelines[MAXIM_LINK_MAX] =
{
    {
        .id = MAXIM_PIPELINE_X,
        .mode =
        {
            .fmt = FMT_9296_LINK_A,
            .res = {.width = SENSOR_WIDTH, .height = SENSOR_HEIGHT, .fps = 30.0f},
            .channel_info = {.vc = 0, .dt = DT_9296_LINK_A, .cid = CID_VC0},
        },
    },
    {
        .id = MAXIM_PIPELINE_X,
        .mode =
        {
            .fmt = FMT_9296_LINK_B,
            .res = {.width = SENSOR_WIDTH, .height = SENSOR_HEIGHT, .fps = 30.0f},
            .channel_info = {.vc = 1, .dt = DT_9296_LINK_B, .cid = CID_VC1},
        },
    }
};

static int ar0231_ext_isp_detect(max9296_context_t* ctxt, uint32 link)
{
    max9296_context_t* pCtxt = (max9296_context_t*)ctxt;
    int rc = 0;
    int i = 0;
    int num_addr = STD_ARRAY_SIZE(supported_ser_addr);
    struct camera_i2c_reg_array read_reg[] = {{0, 0, 0}};
    max9296_sensor_info_t* pSensor = &pCtxt->max9296_sensors[link];
    sensor_platform_func_table_t* sensor_fcn_tbl = &pCtxt->platform_fcn_tbl;

    cam_ser_reg_setting.reg_array = read_reg;
    cam_ser_reg_setting.size = STD_ARRAY_SIZE(read_reg);

    /* Detect far end serializer */
    for (i = 0; i < num_addr; i++)
    {
        cam_ser_reg_setting.reg_array[0].reg_addr = MSM_SER_CHIP_ID_REG_ADDR;
        rc = sensor_fcn_tbl->i2c_slave_read(pCtxt->ctrl, supported_ser_addr[i], &cam_ser_reg_setting);
        if (!rc)
        {
            pSensor->serializer_addr = supported_ser_addr[i];
            break;
        }
    }

    if (i == num_addr)
    {
        SENSOR_WARN("No Camera connected to Link %d of MAX9296 0x%x", link, pCtxt->slave_addr);
    }
    else if (pSensor->serializer_alias == pSensor->serializer_addr)
    {
        SENSOR_WARN("LINK %d already re-mapped", link);
        rc = 0;
    }
    else
    {
        struct camera_i2c_reg_array remap_ser[] = {
            {0x0, pSensor->serializer_alias, _ar0231_ext_isp_delay_}
        };

        //link reset, remap cam, create broadcast addr,
        struct camera_i2c_reg_array remap_ser_2[] = {
            {0x0010, 0x31, MAX9295_LINK_RESET_DELAY },
            {0x0042, pSensor->sensor_alias, _ar0231_ext_isp_delay_},
            {0x0043, CAM_SENSOR_DEFAULT_ADDR, _ar0231_ext_isp_delay_},
            {0x0044, CAM_SER_BROADCAST_ADDR, _ar0231_ext_isp_delay_},
            {0x0045, pSensor->serializer_alias, _ar0231_ext_isp_delay_}
        };

        SENSOR_WARN("Detected Camera connected to Link %d, Ser ID[0x%x]: 0x%x",
            link, MSM_SER_CHIP_ID_REG_ADDR, cam_ser_reg_setting.reg_array[0].reg_data);

        cam_ser_reg_setting.reg_array = remap_ser;
        cam_ser_reg_setting.size = STD_ARRAY_SIZE(remap_ser);
        rc = sensor_fcn_tbl->i2c_slave_write_array(pCtxt->ctrl,pSensor->serializer_addr, &cam_ser_reg_setting);
        if (rc)
        {
            SERR("Failed to change serializer address (0x%x) of MAX9296 0x%x Link %d",
                pSensor->serializer_addr, pCtxt->slave_addr, link);
            return -1;
        }

        cam_ser_reg_setting.reg_array = remap_ser_2;
        cam_ser_reg_setting.size = STD_ARRAY_SIZE(remap_ser_2);
        rc = sensor_fcn_tbl->i2c_slave_write_array(pCtxt->ctrl, pSensor->serializer_alias, &cam_ser_reg_setting);
        if (rc)
        {
            SERR("Failed to reset link %d and remap cam on serializer(0x%x)", link, pSensor->serializer_alias);
            return -1;
        }

        cam_ser_reg_setting.reg_array = link ? max9295_gmsl_1 : max9295_gmsl_0;
        cam_ser_reg_setting.size = link ? STD_ARRAY_SIZE(max9295_gmsl_1) : STD_ARRAY_SIZE(max9295_gmsl_0);
        rc = sensor_fcn_tbl->i2c_slave_write_array(pCtxt->ctrl, pSensor->serializer_alias, &cam_ser_reg_setting);
        if (rc)
        {
            SERR("Failed to reset link %d and remap cam on serializer(0x%x)", link, pSensor->serializer_alias);
            return -1;
        }

        // Read mapped SER to double check if successful
        cam_ser_reg_setting.reg_array = read_reg;
        cam_ser_reg_setting.size = STD_ARRAY_SIZE(read_reg);
        cam_ser_reg_setting.reg_array[0].reg_addr = 0x0000;
        rc = sensor_fcn_tbl->i2c_slave_read(pCtxt->ctrl, pSensor->serializer_alias, &cam_ser_reg_setting);
        if (rc)
        {
            SERR("Failed to read serializer(0x%x) after remap", pSensor->serializer_alias);
            return -1;
        }

        if (pSensor->serializer_alias != cam_ser_reg_setting.reg_array[0].reg_data)
        {
            SENSOR_WARN("Remote SER address remap failed: 0x%x, should be 0x%x",
                cam_ser_reg_setting.reg_array[0].reg_data, pSensor->serializer_alias);
        }
    }

    return rc;
}

static int ar0231_ext_isp_get_link_cfg(max9296_context_t* ctxt, uint32 link, max9296_link_cfg_t* p_cfg)
{
    (void)ctxt;

    p_cfg->num_pipelines = 1;
    p_cfg->pipelines[0] = ar0231_ext_isp_pipelines[link];
    return 0;
}

static int ar0231_ext_isp_init_link(max9296_context_t* ctxt, uint32 link)
{
    max9296_context_t* pCtxt = ctxt;
    max9296_sensor_info_t* pSensor = &pCtxt->max9296_sensors[link];
    int rc;

    if (SENSOR_STATE_DETECTED == pSensor->state)
    {
        switch (ar0231_ext_isp_pipelines[link].mode.channel_info.dt)
        {
        case CSI_DT_RAW8:
            cam_ser_reg_setting.reg_array = link ? max9295_init_8bit_regs_1 : max9295_init_8bit_regs_0;
            cam_ser_reg_setting.size = link ? STD_ARRAY_SIZE(max9295_init_8bit_regs_1) : STD_ARRAY_SIZE(max9295_init_8bit_regs_0);
            break;
        case CSI_DT_RAW10:
            cam_ser_reg_setting.reg_array = link ? max9295_init_10bit_regs_1 : max9295_init_10bit_regs_0;
            cam_ser_reg_setting.size = link ? STD_ARRAY_SIZE(max9295_init_10bit_regs_1) : STD_ARRAY_SIZE(max9295_init_10bit_regs_0);
            break;
        default:
            SENSOR_WARN("link %d unknown DT: 0x%x", link,
                ar0231_ext_isp_pipelines[link].mode.channel_info.dt);
            return -1;
        }

        rc = pCtxt->platform_fcn_tbl.i2c_slave_write_array(
                    pCtxt->ctrl,
                    pSensor->serializer_alias, &cam_ser_reg_setting);
        if (rc)
        {
            SERR("Failed to init camera serializer(0x%x)", pSensor->serializer_alias);
            return -1;
        }

        pSensor->state = SENSOR_STATE_INITIALIZED;
    }
    else
    {
        SERR("ar0231 %d init in wrong state %d", link, pSensor->state);
        rc = -1;
    }

    return rc;
}

static int ar0231_ext_isp_start_link(max9296_context_t* ctxt, uint32 link)
{
    max9296_context_t* pCtxt = ctxt;
    max9296_sensor_info_t* pSensor = &pCtxt->max9296_sensors[link];
    int rc;

    if (SENSOR_STATE_INITIALIZED == pSensor->state)
    {
        SHIGH("starting serializer");
        cam_ser_reg_setting.reg_array = max9295_start_reg;
        cam_ser_reg_setting.size = STD_ARRAY_SIZE(max9295_start_reg);
        rc = pCtxt->platform_fcn_tbl.i2c_slave_write_array(
            pCtxt->ctrl,
            pSensor->serializer_alias,
            &cam_ser_reg_setting);
        if (rc)
        {
            SERR("serializer 0x%x failed to start", pSensor->serializer_alias);
        }
        else
        {
            pSensor->state = SENSOR_STATE_STREAMING;
        }
    }
    else
    {
        SERR("ar0231 %d start in wrong state %d", link, pSensor->state);
        rc = -1;
    }

    return rc;
}

static int ar0231_ext_isp_stop_link(max9296_context_t* ctxt, uint32 link)
{
    max9296_context_t* pCtxt = ctxt;
    max9296_sensor_info_t* pSensor = &pCtxt->max9296_sensors[link];
    int rc;

    if (SENSOR_STATE_STREAMING == pSensor->state)
    {
        cam_ser_reg_setting.reg_array = max9295_stop_reg;
        cam_ser_reg_setting.size = STD_ARRAY_SIZE(max9295_stop_reg);
        if ((rc = pCtxt->platform_fcn_tbl.i2c_slave_write_array(
            pCtxt->ctrl,
            pSensor->serializer_alias,
            &cam_ser_reg_setting)))
        {
            SERR("Failed to stop serializer(0x%x)", pSensor->serializer_alias);
        }

        pSensor->state = SENSOR_STATE_INITIALIZED;
    }
    else
    {
        SERR("ar0231 %d stop in wrong state %d", link, pSensor->state);
        rc = -1;
    }

    return rc;
}
