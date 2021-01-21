/**
 * @file ccidbgr.c
 *
 * This file implements the cci application that can R/W cci devices.
 *
 * Copyright (c) 2013, 2016-2019 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>

#include <media/cam_req_mgr.h>
#include <media/ais_sensor.h>

struct menu_controls {
    const char *name;
    void (*fcn)(void);
};

#define IOCTL(n, r, p) ioctl_name(n, r, p, #r)

#define CCI_DEFAULT_FREQ_MODE 2 //SENSOR_I2C_MODE_CUSTOM

static int g_fd_cci = -1;
static int verbose = 0;
static uint32_t g_slave_addr = 0x0;
static uint32_t g_i2c_freq = CCI_DEFAULT_FREQ_MODE;
static uint32_t g_addr_type = 0x0;
static uint32_t g_data_type = 0x0;


static int ioctl_name(int fd, unsigned long int request, void *parm, const char *name)
{
    int retval = ioctl(fd, request, parm);

    if (retval < 0)
        printf("%s: failed: %s\n", name, strerror(errno));
    else if (verbose)
        printf("%s: ok\n", name);

    return retval;
}

static void sensor_query_cap(int fd, struct cam_sensor_query_cap* sensor_cap)
{
    int rc = 0;
    struct cam_control cam_cmd;

    memset(&cam_cmd, 0x0, sizeof(cam_cmd));

    cam_cmd.op_code     = CAM_QUERY_CAP;
    cam_cmd.size        = sizeof(*sensor_cap);
    cam_cmd.handle_type = CAM_HANDLE_USER_POINTER;
    cam_cmd.reserved    = 0;
    cam_cmd.handle      = (uint64_t)sensor_cap;

    rc = IOCTL(fd, VIDIOC_CAM_CONTROL, &cam_cmd);

    return;
}

static void cci_power_up(void)
{
    int rc = 0;
    struct cam_control cci_cmd;

    memset(&cci_cmd, 0x0, sizeof(cci_cmd));

    cci_cmd.op_code     = AIS_SENSOR_I2C_POWER_UP;
    cci_cmd.size        = 0;
    cci_cmd.handle_type = CAM_HANDLE_USER_POINTER;

    rc = IOCTL(g_fd_cci, VIDIOC_CAM_CONTROL, &cci_cmd);

    printf("cci power up [%s]\n", (rc ? "FAIL" : "ok"));

    return;
}

static void cci_read(void)
{
    int rc = 0;
    struct cam_control cci_cmd;
    struct ais_sensor_cmd_i2c_read i2c_read;
    uint32_t reg_addr;

    memset(&cci_cmd, 0x0, sizeof(cci_cmd));
    memset(&i2c_read, 0x0, sizeof(i2c_read));

    cci_cmd.op_code     = AIS_SENSOR_I2C_READ;
    cci_cmd.size        = sizeof(i2c_read);
    cci_cmd.handle_type = CAM_HANDLE_USER_POINTER;
    cci_cmd.reserved    = 0;
    cci_cmd.handle      = (uint64_t)&i2c_read;

    printf("<addr>\n");
    scanf("%x", &reg_addr);
    i2c_read.reg_addr = reg_addr;
    i2c_read.addr_type = g_addr_type;
    i2c_read.data_type = g_data_type;
    i2c_read.i2c_config.slave_addr = g_slave_addr;
    i2c_read.i2c_config.i2c_freq_mode = g_i2c_freq;

    rc = IOCTL(g_fd_cci, VIDIOC_CAM_CONTROL, &cci_cmd);

    printf("\t 0x%x [%d %d] <- 0x%x [%s]\n", i2c_read.reg_addr,
            i2c_read.addr_type, i2c_read.data_type,
            i2c_read.reg_data, (rc ? "FAIL" : "ok"));

    return;
}

static void cci_write(void)
{
    int rc = 0;
    struct cam_control cci_cmd;
    struct ais_sensor_cmd_i2c_wr i2c_write;
    uint32_t reg_addr, reg_data;

    memset(&cci_cmd, 0x0, sizeof(cci_cmd));
    memset(&i2c_write, 0x0, sizeof(i2c_write));

    cci_cmd.op_code     = AIS_SENSOR_I2C_WRITE;
    cci_cmd.size        = sizeof(i2c_write);
    cci_cmd.handle_type = CAM_HANDLE_USER_POINTER;
    cci_cmd.reserved    = 0;
    cci_cmd.handle      = (uint64_t)&i2c_write;

    printf("<addr> <data>\n");
    scanf("%x %x", &reg_addr, &reg_data);
    i2c_write.wr_payload.reg_addr = reg_addr;
    i2c_write.wr_payload.reg_data = reg_data;
    i2c_write.addr_type = g_addr_type;
    i2c_write.data_type = g_data_type;
    i2c_write.i2c_config.slave_addr = g_slave_addr;
    i2c_write.i2c_config.i2c_freq_mode = g_i2c_freq;

    rc = IOCTL(g_fd_cci, VIDIOC_CAM_CONTROL, &cci_cmd);

    printf("\t 0x%x [%d %d] <- 0x%x [%s]\n", i2c_write.wr_payload.reg_addr,
            i2c_write.addr_type, i2c_write.data_type,
            i2c_write.wr_payload.reg_data, (rc ? "FAIL" : "ok"));

    return;
}

static void cci_update(void)
{
    printf("<slave addr>, <addr_type>, <data type>\n");
    scanf("%x %d %d",
            &g_slave_addr,
            &g_addr_type,
            &g_data_type);
}

static struct menu_controls cci_menu_ctrls[] = {
    {"cci_read", cci_read},
    {"cci_write", cci_write},
    {"cci_update", cci_update},
};

static void cci_menu(void)
{
    int i = 0;
    int max = sizeof(cci_menu_ctrls)/sizeof(cci_menu_ctrls[0]);

    while (1)
    {
        while (1) {
            printf("MAIN MENU\n");
            for (i=0; i<max; i++) {
                printf("\t[ %d ] - %s\n", i, cci_menu_ctrls[i].name);
            }
            printf("\t[ %d ] - exit\n", i);

            do {
                scanf("%d",&i);
            } while (i > max);

            if (i == max) {
                printf("exiting...\n");
                return;
            }
            else
                cci_menu_ctrls[i].fcn();
        }
    }
}


int main(int argc, char **argv)
{
    struct media_device_info mdev_info;
    int num_media_devices = 0;
    char dev_name[64];
    int rc = 0, dev_fd = 0;
    int num_entities = 1;
    int found_cci = 0;
    int i = 0;
    /*default to dev=0*/
    unsigned int slot_id = 0;

    printf("%s: E\n", __func__);

    for (i = 1; i < argc; i++)
    {
        const char *tok;

        if (!strncmp(argv[i], "-dev=", strlen("-dev=")))
        {
            tok = argv[i] + strlen("-dev=");
            slot_id = atoi(tok);
        }
    }


    while (!found_cci)
    {
        struct media_entity_desc entity;
        memset(&entity, 0, sizeof(entity));

        snprintf(dev_name, sizeof(dev_name), "/dev/media%d", num_media_devices);
        dev_fd = open(dev_name, O_RDWR | O_NONBLOCK);
        if (dev_fd < 0) {
            printf("Done discovering media devices\n");
            break;
        }

        num_media_devices++;
        rc = ioctl(dev_fd, MEDIA_IOC_DEVICE_INFO, &mdev_info);
        if (rc < 0) {
            printf("Error: ioctl media_dev failed: %s\n", strerror(errno));
            close(dev_fd);
            return rc;
        }

        if (strncmp(mdev_info.model, CAM_REQ_MGR_VNODE_NAME, sizeof(mdev_info.model)) != 0) {
            close(dev_fd);
            continue;
        }

        while (!found_cci)
        {
            entity.id = num_entities;
            int rc = ioctl(dev_fd, MEDIA_IOC_ENUM_ENTITIES, &entity);
            if (rc < 0) {
                printf("Done enumerating media entities\n");
                close(dev_fd);
                rc = 0;
                break;
            }

            num_entities = entity.id | MEDIA_ENT_ID_FLAG_NEXT;

            printf("entity name %s type 0x%x group id %d\n",
                    entity.name, entity.type, entity.group_id);

            switch (entity.type)
            {
            case CAM_SENSOR_DEVICE_TYPE:
            {
                struct cam_sensor_query_cap sensor_cap;

                snprintf(dev_name, sizeof(dev_name), "/dev/%s", entity.name);
                g_fd_cci = open(dev_name, O_RDWR | O_NONBLOCK);
                if (g_fd_cci < 0) {
                    printf("FAILED TO OPEN SENSOR\n");
                    exit(0);
                }

                memset(&sensor_cap, 0x0, sizeof(sensor_cap));
                sensor_query_cap(g_fd_cci, &sensor_cap);

                if (sensor_cap.slot_info == slot_id)
                {
                    found_cci = 1;
                }
            }
                break;
            default:
                break;
            }
        }

        close(dev_fd);
    }

    cci_power_up();

    cci_menu();

    close(g_fd_cci);

    exit(0);
}
