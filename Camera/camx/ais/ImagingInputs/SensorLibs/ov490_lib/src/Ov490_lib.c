/**
 * @file
 *          ov490_lib.c
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
 *          bridge driver body file
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>
#include <ctype.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <sys/stat.h> 


#include "Ov490_lib.h"
#include "Ov490_hw.h"
#include "Ov490_utility.h"
#include "SensorDebug.h"
#include "CameraSensorDeviceInterface.h"
#include "Ov490_I2C_config.h"


/*ov490 states*/
typedef enum
{
    OV490_STATE_INVALID = 0,
	OV490_STATE_DETECTED,
	OV490_STATE_POWERUP_SCCB_READY,
	OV490_STATE_BOOTING,
	OV490_STATE_BOOTING2,
	OV490_STATE_STREAM_ON,
	OV490_STATE_STREAM_OFF,
	OV490_STATE_STANDBY,
	OV490_STATE_POWERDOWN,
	OV490_STATE_ERROR_TIMEOUT,
}ov490_state_t;

/*ov9284 states*/
typedef enum
{
	DEVICE_STATE_INVALID = 0,
	DEVICE_STATE_STREAM_ON,
	DEVICE_STATE_INITED,
	DEVICE_STATE_LINK_LOCKED,
	DEVICE_STATE_LINK_UNLOCKED,
	DEVICE_STATE_LINK_PASSED,
	DEVICE_STATE_LINK_UNPASSED,
	DEVICE_STATE_ERROR,
}external_devices_state_t;

/*record of external devices state*/
typedef struct
{
	external_devices_state_t  ov9284_state;
	external_devices_state_t  ti936_state;
	volatile unsigned int lock_pin_status;
	volatile unsigned int ov490_isp_ticks;
}external_devices_info_t;

typedef struct
{
    /*must be first element*/
	sensor_lib_t sensor_lib;
	CameraMutex mutex;

	unsigned int num_supported_sensors;
	unsigned int num_connected_sensors;
	
	sensor_platform_func_table_t  sensor_platform_fcn_tbl;
	bool sensor_i2c_tbl_initialized;

    struct camera_i2c_reg_setting local_reg_setting;
	struct camera_i2c_reg_setting remote_reg_setting;
	struct camera_i2c_reg_setting dummy_reg[1];

	/*firmware download*/
	FILE* fp;
	struct camera_i2c_reg_array *firmware;
	struct camera_i2c_reg_setting firmware_setting;
	unsigned int firmware_size;  //in bytes
	bool firmware_loaded;

	
	external_devices_info_t bridge_sensors;
	ov490_state_t   ov490_state;
	unsigned short  ov490_slave_addr;
	
	/*dtc */
    void        *linkdetect_handler_tid;
    volatile bool linkdetect_handler_exit;

	void* ctrl;
}bridge_context_t;


/*
bridge interfaces for lib definitions
*/
static int ov490_sensor_close_lib(void* ctxt);
static int ov490_sensor_power_suspend(void* ctxt);
static int ov490_sensor_power_resume(void* ctxt);
static int ov490_sensor_detect_device(void* ctxt);
static int ov490_sensor_detect_device_channels(void* ctxt);
static int ov490_sensor_init_setting(void* ctxt); 
static int ov490_sensor_set_channel_mode(void* ctxt, unsigned int src_id_mask, unsigned int mode);
static int ov490_sensor_start_stream(void* ctxt, unsigned int src_id_mask);
static int ov490_sensor_stop_stream(void* ctxt, unsigned int src_id_mask);
static int ov490_sensor_set_platform_func_table(void* ctxt, sensor_platform_func_table_t* table);
//static int ov490_sensor_power_on(void* ctxt);
//static int ov490_sensor_power_off(void* ctxt);

/*bridge interfaces for ext*/
#ifdef OV490_SCCB_SLAVE_BOOTUP_ENABLE
static int ov490_read_firmware(bridge_context_t* pCtxt);
static bool ov490_firmwaresize_check(unsigned int size);
static int ov490_firmware_fill_settings(bridge_context_t* pCtxt);
static int ov490_firmware_boot(bridge_context_t* pCtxt);
static int ov490_set_yuyv_format(bridge_context_t* pCtxt);
#endif
static int ov490_detect_device(bridge_context_t* pCtxt);
static int ov490_start_stream(bridge_context_t* pCtxt);
static int ov490_stop_stream(bridge_context_t* pCtxt);
static int ov490_power_up();
static int ti936_power_up();

#ifdef OV490_HOTPLUG_FEATURE_ENABLE
static bool ti936_lock_status();
static int ti936_enable_lockpin(bridge_context_t* pCtxt,bool enable);
typedef int (*ti936_lock_event_thread_t)(void *arg);
static int ti936_lock_poll_thread(void *arg);
static int start_lock_poll(bridge_context_t* pCtxt);
static int stop_lock_poll(bridge_context_t* pCtxt);
static int ov9284_init(bridge_context_t* pCtxt);
static bool ov490_isp_status(bridge_context_t* pCtxt);  //benny20190416_isp_health
static int ov490_isp_status_dtc_report(bridge_context_t* pCtxt); //benny20190416_isp_health
#endif

static sensor_lib_t sensor_lib_ptr = 
{
    .sensor_slave_info =
    {
        .sensor_name = SENSOR_MODEL,
		.slave_addr = OV490_SCCB_SLAVE_ID_SCCBID00,
		.i2c_freq_mode = SENSOR_I2C_MODE_CUSTOM,
		.addr_type = CAMERA_I2C_WORD_ADDR,
		.data_type = CAMERA_I2C_BYTE_DATA,
		.sensor_id_info =
        {
            .sensor_id_reg_addr = 0x00,
			.sensor_id = OV490_SCCB_SLAVE_ID_SCCBID00,
			.sensor_id_mask = 0xff00,
        },
		//.power_setting_array = 
		//{
		//	
		//},
		.is_init_params_valid = 1,
    },
    .num_channels = NUM_SUPPORT_SENSORS,
    .channels = 
	{
	    {
			.output_mode = 
			{
			    .fmt = FMT_OV490,
				.res = {.width = SENSOR_WIDTH, .height = SENSOR_HEIGHT, .fps = 30.0f},
				.channel_info = { .vc = VC0, .dt = DT_OV490, .cid = CID_VC0,},
			},
			.num_subchannels = 1,
			.subchan_layout =
			{
			    {
					.src_id = 0,
					.x_offset = 0,
					.y_offset = 0,
					.stride = SENSOR_WIDTH*2   //need to confirm 
			    },
			},
		},
	},
	.num_subchannels = NUM_SUPPORT_SENSORS,
	.subchannels =
	{
	    {
			.src_id = 0,
			.modes = 
			{
				{
				    .fmt = FMT_OV490,
					.res = {.width = SENSOR_WIDTH, .height = SENSOR_HEIGHT, .fps = 30.0f},
					.channel_info = { .vc = VC0, .dt = DT_OV490, .cid = CID_VC0,},
				}
			},
			.num_modes = 1,
		},
	},
	.sensor_output = 
    {
        .output_format = SENSOR_YCBCR,
		.connection_mode = SENSOR_MIPI_CSI,
		.raw_output = SENSOR_8_BIT_DIRECT,
		.filter_arrangement = SENSOR_UYVY,
    },
    .csi_params =
    {
        .lane_cnt = 4,
        .settle_cnt = OV490_SC,
        .lane_mask = 0x1F,
        .combo_mode = 0,
        .is_csi_3phase = 0,
    },
    .sensor_close_lib = ov490_sensor_close_lib,
    .sensor_custom_func =
    {
        .sensor_set_platform_func_table = &ov490_sensor_set_platform_func_table,
        .sensor_power_suspend = &ov490_sensor_power_suspend,
        .sensor_power_resume = &ov490_sensor_power_resume,
        .sensor_detect_device = &ov490_sensor_detect_device,
        //.sensor_detect_device_channels = &ov490_sensor_detect_device_channels,
        .sensor_init_setting = &ov490_sensor_init_setting,
        .sensor_set_channel_mode = &ov490_sensor_set_channel_mode,
        .sensor_start_stream = &ov490_sensor_start_stream,
        .sensor_stop_stream = &ov490_sensor_stop_stream,
        //.sensor_power_on = &ov490_sensor_power_on,
        //.sensor_power_off = &ov490_sensor_power_off,
    },
	.use_sensor_custom_func = TRUE,
};


static int ov490_sensor_close_lib(void* ctxt)
{
	if(ctxt == NULL)
		return -1;
	
	bridge_context_t* pCtxt = (bridge_context_t*)ctxt;
	if((pCtxt->firmware_loaded == true) && (pCtxt->firmware != NULL))
		free(pCtxt->firmware);
	if(pCtxt->fp != NULL)
		fclose(pCtxt->fp);

	stop_lock_poll(pCtxt);

	CameraDestroyMutex(pCtxt->mutex);
	free(pCtxt);
	return 0;
}

static int ov490_sensor_power_suspend(void* ctxt)
{	
    if(ctxt == NULL)
		return -1;
	bridge_context_t* pCtxt = (bridge_context_t*)ctxt;
	pCtxt->ov490_state = OV490_STATE_POWERDOWN;
	SHIGH("ov490_sensor_power_suspend()");
	return 0;
}

static int ov490_sensor_power_resume(void* ctxt)
{
    if(ctxt == NULL)
		return -1;	
	bridge_context_t* pCtxt = (bridge_context_t*)ctxt;
	pCtxt->ov490_state = OV490_STATE_STREAM_OFF;
	SHIGH("ov490_sensor_power_resume()");
	return 0;
}


static int ov490_sensor_set_channel_mode(void* ctxt, unsigned int src_id_mask, unsigned int mode)
{
    (void)ctxt;
    (void)mode;
    (void)src_id_mask;

    SHIGH("ov490_sensor_set_channel_mode()");

    return 0;
}

static int ov490_sensor_start_stream(void* ctxt, unsigned int src_id_mask)
{
	int ret;
	(void)src_id_mask;
	if(ctxt == NULL)
		return -1;	
	bridge_context_t* pCtxt = (bridge_context_t*)ctxt;

	ret = ov490_start_stream(pCtxt);
	return ret;
}

static int ov490_sensor_stop_stream(void* ctxt, unsigned int src_id_mask)
{
	int ret;	
	(void)src_id_mask;
    if(ctxt == NULL)
		return -1;
	bridge_context_t* pCtxt = (bridge_context_t*)ctxt;

	ret = ov490_stop_stream(pCtxt);
	return ret;
}

static int ov490_sensor_set_platform_func_table(void* ctxt, sensor_platform_func_table_t* table)
{
   bridge_context_t* pCtxt = (bridge_context_t*)ctxt;

   if (!pCtxt->sensor_i2c_tbl_initialized)
    {
        if (!table)
        {
            SERR("Invalid i2c func table param");
            return -1;
        }
        pCtxt->sensor_platform_fcn_tbl = *table;
        pCtxt->sensor_i2c_tbl_initialized = 1;
        SLOW("i2c func table set");
    }
    return 0;
}

static int ov490_sensor_detect_device(void* ctxt)
{
   int ret = 0;
   bridge_context_t* pCtxt = (bridge_context_t*)ctxt;
   //power up devices 
   ti936_power_up();
   ov490_power_up();

   //detect ov490
   ret = ov490_detect_device(pCtxt); 
   if((ret == 0)&&(pCtxt->ov490_state >= OV490_STATE_DETECTED))
   {
       SHIGH("Ov490 chip detected");
	   pCtxt->sensor_lib.src_id_enable_mask |= (1 << 0);

	   //prepare firmware
	   unsigned long startslot=0;
       unsigned long endslot=0;
	   test_get_time(&startslot);
	   ret = ov490_read_firmware(pCtxt);
	   test_get_time(&endslot);

	   if(ret == 0)
	   {
	       SHIGH("Ov490 firmware read time span %lu",endslot-startslot);
	   }
	   else
	   {
	       ret = -1;
	   }
   }
   else 
   {
       ret = -1;
   }
   return ret;
}


static void* ov490_sensor_open_lib(void* ctrl, void* arg)  
{
	if((ctrl == NULL)||(arg == NULL))
		return NULL;
	
	sensor_open_lib_t* device_info = (sensor_open_lib_t*)arg;
	bridge_context_t* pCtxt = calloc(1,sizeof(bridge_context_t));

	SHIGH("ov490_sensor_open_lib()");

#ifdef OV490_SCCB_SLAVE_BOOTUP_ENABLE
	ov490_read_firmware(pCtxt);
#endif 

    //initialize bridge contxt
	if(pCtxt)
	{
		memcpy(&pCtxt->sensor_lib, &sensor_lib_ptr, sizeof(pCtxt->sensor_lib));

		pCtxt->ctrl = ctrl;
		pCtxt->sensor_lib.sensor_slave_info.camera_id = device_info->camera_id;
		
		pCtxt->num_supported_sensors = NUM_SUPPORT_SENSORS;
		pCtxt->num_connected_sensors = 0;
		pCtxt->ov490_slave_addr = OV490_SCCB_SLAVE_ID_SCCBID00;
		pCtxt->sensor_lib.sensor_slave_info.sensor_id_info.sensor_id = pCtxt->ov490_slave_addr;

		pCtxt->ov490_state = OV490_STATE_INVALID;
		pCtxt->bridge_sensors.ti936_state = DEVICE_STATE_INVALID;
		pCtxt->bridge_sensors.ov9284_state = DEVICE_STATE_INVALID;
		pCtxt->bridge_sensors.lock_pin_status = TI936_LOCK_PIN_LOST;
		pCtxt->bridge_sensors.ov490_isp_ticks = 0;    //benny20190416_isp_health

		pCtxt->fp = NULL;
		pCtxt->firmware_loaded = false;
		pCtxt->firmware_size = 0;
		pCtxt->firmware = NULL;
		
		pCtxt->linkdetect_handler_exit = false;
		pCtxt->linkdetect_handler_tid = NULL;
		
		CameraCreateMutex(&pCtxt->mutex);

	}
	
	return pCtxt;
}

static int ov490_sensor_init_setting(void* ctxt)
{
	if(ctxt == NULL)
		return -1;	
	int ret= 0;
	bridge_context_t* pCtxt = (bridge_context_t*)ctxt;
	
	//boot device
	do{	
		unsigned long startslot=0;
		unsigned long endslot=0;
		test_get_time(&startslot);
    	ret = ov490_firmware_boot(pCtxt);
		test_get_time(&endslot);
		SHIGH("%s Ov490 firmware bootup time span %lu",__FUNCTION__,endslot-startslot);
	}while(0);
	//local init
	//omit s

	//detect remote
	if((ret == 0)&&(OV490_STATE_STREAM_OFF == pCtxt->ov490_state))
	{
	    ti936_enable_lockpin(pCtxt,true);
		start_lock_poll(pCtxt);
		unsigned int i=0;
		do{
			if(pCtxt->bridge_sensors.lock_pin_status == TI936_LOCK_PIN_LOCKED)
			{	
			    SHIGH("hotplug %s ov490 remote locked",__FUNCTION__);
				break;
			}
			else	
			    CameraSleep(10);
		}while(++i <= 500); //this shall be refined 
	}

	if(pCtxt->bridge_sensors.lock_pin_status != TI936_LOCK_PIN_LOCKED)
	{
		SERR("hotplug %s ov490 remote loss during ais_server init. we dont commit to cover this error in current design",__FUNCTION__);
	    //ret = -2;
	}
	return ret;
}

#define READ_OV490_CMD(context,reg,out,ret) \
{\
    read_setting.reg_array[0].reg_addr = reg;\
    ret = context->sensor_platform_fcn_tbl.i2c_slave_read(\
		context->ctrl,\
		context->ov490_slave_addr,\
		&read_setting);\
	out = read_setting.reg_array[0].reg_data;\
}

#define WAITFOR_OV490_STATUS(context,reg,state,timeout,ret) \
{ \
    unsigned int cnt=0;\
    read_setting.reg_array[0].reg_addr = reg;\
    do{\
    	CameraSleep(10);\
		ret = context->sensor_platform_fcn_tbl.i2c_slave_read(\
		context->ctrl,\
		context->ov490_slave_addr,\
		&read_setting);\
    }while((++cnt<timeout)&&(ret == 0)&&(read_setting.reg_array[0].reg_data != state));\
    if((cnt >= timeout)||(ret!=0))\
		ret = -1;\
	else\
		ret = 0;\
}

#define WRITE_OV490_CMD(context,cmd,ret) \
{ \
    ret = context->sensor_platform_fcn_tbl.i2c_slave_write_array( context->ctrl,\
		  context->ov490_slave_addr,\
		  &cmd);\
}

//ov490 chip driver related 
//#ifdef OV490_SCCB_SLAVE_BOOTUP_ENABLE

static int ov490_read_firmware(bridge_context_t* pCtxt)
{

    int error = 0;
    const char* filename = FIRMWARE_BIN_FREERUN_PATH;
	

	if(!pCtxt->firmware_loaded)
	{
		pCtxt->fp = fopen(filename, "r");
		if (NULL == pCtxt->fp)
		{
	    	SERR("Couldn't open %s\n", filename);
			error = -1;
		}
		else 
		{
		 	struct stat statbuf;  
    		stat(filename,&statbuf);
			pCtxt->firmware_size = statbuf.st_size;
			if(ov490_firmwaresize_check(pCtxt->firmware_size))
			{
				SHIGH("checking firmware size %d pass",pCtxt->firmware_size);
				//allocate register settings
				pCtxt->firmware = calloc(pCtxt->firmware_size,sizeof(struct camera_i2c_reg_array));
				if(!pCtxt->firmware)
				{
				    SERR("Couldn't alloc firmware buff\n");
					error = -1;
				}
				else
				{	 
					ov490_firmware_fill_settings(pCtxt);
				}
			}
		}
		if(error!= 0)
		{
			pCtxt->firmware_size = 0;
			pCtxt->firmware_loaded = false;
		}
		else
		{	
			pCtxt->firmware_loaded = true;
			fclose(pCtxt->fp);
		}
	}
	
	return error;
}

static bool ov490_firmwaresize_check(unsigned int size)
{
	return ((size != 0)?(SRAM_FW_START + size < 0x8019A000):false);
}

static int ov490_firmware_fill_settings(bridge_context_t* pCtxt)
{
	int ret = 0;
	unsigned int readbytes = 0;
	unsigned int leftbytes = 0;
	unsigned char buff[FIRMWARE_READ_BUFF];

	unsigned int reg_addr_start = (SRAM_FW_START&0x0000ffff);
	unsigned int reg_offset = 0;
	unsigned int buff_offset = 0;

	if((pCtxt == NULL)||(pCtxt->firmware == NULL) || (pCtxt->fp == NULL))
	{
		SERR("ov490_firmware_fill_settings Param Error\n");
		return -1;
	}
	leftbytes = pCtxt->firmware_size;
	fseek(pCtxt->fp, 0, SEEK_SET);

	while(leftbytes > 0)
	{
		if(leftbytes >= FIRMWARE_READ_BUFF)		
		{
			readbytes = fread(&buff[0],sizeof(unsigned char),FIRMWARE_READ_BUFF,pCtxt->fp);
		}
		else if(leftbytes < FIRMWARE_READ_BUFF)
		{
			readbytes = fread(&buff[0],sizeof(unsigned char),leftbytes,pCtxt->fp);
		}
		leftbytes = leftbytes - readbytes;

		//fill in
		buff_offset = 0;
		while(readbytes > 0)
		{
			ADD_I2C_REG_ARRAY(pCtxt->firmware,reg_offset,reg_addr_start,buff[buff_offset],0);

			// do { \
			//     pCtxt->firmware[reg_offset].reg_addr = reg_addr_start; \
			//     pCtxt->firmware[reg_offset].reg_data = _val_; \
			//     pCtxt->firmware[reg_offset].delay = _delay_; \
			//     reg_offset++; \
			// } while(0)


			reg_addr_start ++;
			buff_offset++;
			readbytes--;
		}
		SHIGH("ov490_firmware_fill_settings leftbytes %d items filled in reg array\n",leftbytes);
	}

	// pCtxt->firmware_setting.reg_array = pCtxt->firmware;
	// pCtxt->firmware_setting.size = pCtxt->firmware_size;
	pCtxt->firmware_setting.addr_type = CAMERA_I2C_WORD_ADDR;
	pCtxt->firmware_setting.data_type = CAMERA_I2C_BYTE_DATA;
	return ret;
}

static int ov490_write_firmware_context(bridge_context_t* pCtxt);
static int ov490_write_firmware_context(bridge_context_t* pCtxt)
{
    int ret = -1;
	if(!pCtxt->firmware_loaded)
	{
	    SERR("no firmware laoded");
		ret = -99;
	}
	else
	{
	    WRITE_OV490_CMD(pCtxt,ov490_status_prefix_8019,ret);
		do
		{
        	unsigned int leftbytes = pCtxt->firmware_size;
            unsigned int offset = 0;
            while(leftbytes > 0)
            {
                if(leftbytes >= FIRMWARE_LOAD_BUFF)
        	    {
                    pCtxt->firmware_setting.reg_array = &(pCtxt->firmware[offset]);
                    pCtxt->firmware_setting.size = FIRMWARE_LOAD_BUFF;
                    ret = pCtxt->sensor_platform_fcn_tbl.i2c_slave_write_array(pCtxt->ctrl,
		  				pCtxt->ov490_slave_addr,
		  				&(pCtxt->firmware_setting));
                    if(ret == 0)
                    {
                	    leftbytes = leftbytes - FIRMWARE_LOAD_BUFF;
                	    offset = offset + FIRMWARE_LOAD_BUFF; 
                    }
                    else 
                    {
                	    break;
                    }
        	    }
        	    else if(leftbytes < FIRMWARE_LOAD_BUFF)
        	    {
                    pCtxt->firmware_setting.reg_array = &(pCtxt->firmware[offset]);
                    pCtxt->firmware_setting.size = leftbytes;
                    ret = pCtxt->sensor_platform_fcn_tbl.i2c_slave_write_array(pCtxt->ctrl,
		  				pCtxt->ov490_slave_addr,
		  				&(pCtxt->firmware_setting));
                    if(ret == 0)
                    {
                	   offset = offset + leftbytes; 
                	   leftbytes = 0;
                	   SHIGH("%s I2C function ov490 load firmware successfully %d loaded, %d wanted",__FUNCTION__,offset,pCtxt->firmware_size);
                    }
                    else
                    {
		    		   break;
                    }
        	    }
            }

		    if(0!=ret)
		    {
		        SERR("%s I2C function ov490 load firmware error %d loaded, %d wanted",__FUNCTION__,offset,pCtxt->firmware_size);
		        break;
	        }
	    }while(0);	
	}
	return ret;
}

static int ov490_check_firmware_version(bridge_context_t* pCtxt);
static int ov490_check_firmware_version(bridge_context_t* pCtxt)
{
    int ret =-1;
	//check FW version
    read_setting.reg_array[0].reg_addr = OV490_REGADDR_FW_VERSION_HIGH;
    ret = pCtxt->sensor_platform_fcn_tbl.i2c_slave_read( pCtxt->ctrl, pCtxt->ov490_slave_addr,
          &read_setting);
    if (0!=ret)
    {
        SERR("Unable to read from OV490 I2C device address (0x%x) reg address 0x80800102", pCtxt->ov490_slave_addr);
    }
    else
    {
        SHIGH("Read FW version reg 0x80800102 data 0x%x",read_setting.reg_array[0].reg_data);
    }

    read_setting.reg_array[0].reg_addr = OV490_REGADDR_FW_VERSION_LOW;
    ret = pCtxt->sensor_platform_fcn_tbl.i2c_slave_read( pCtxt->ctrl, pCtxt->ov490_slave_addr,
          &read_setting);
    if (0!=ret)
    {
        SERR("Unable to read from OV490 I2C device address (0x%x) reg address 0x80800103", pCtxt->ov490_slave_addr);
    }
    else
    {
        SHIGH("Read FW version reg 0x80800103 data 0x%x",read_setting.reg_array[0].reg_data);
    } 
	return ret;
}

static int ov490_firmware_boot(bridge_context_t* pCtxt)
{
    int ret = 0;
	unsigned char boottime = 0;
	SHIGH("ov490_firmware_boot state:%d s",pCtxt->ov490_state);
	
	//if (pCtxt->ov490_state != OV490_STATE_DETECTED)
	//{
	//	SHIGH("ov490 is not power up");
	//	return -1;
	//}

	//loop SCCB_BOOT_RDY
    WRITE_OV490_CMD(pCtxt,ov490_status_prefix_8019,ret);
	WAITFOR_OV490_STATUS(pCtxt,OV490_REGADDR_SRAM_FLAG_ADDR,SCCB_BOOT_RDY,10,ret);
	if(ret != 0)
	{
		SERR("%s I2C function WAITFOR_OV490_STATUS SCCB_BOOT_RDY timeout",__FUNCTION__);
	}
	else
	{
		pCtxt->ov490_state = OV490_STATE_POWERUP_SCCB_READY;
		SHIGH("%s: ov490 enter OV490_STATE_POWERUP_SCCB_READY",__FUNCTION__);

		do
		{
		    // enable burst
		    WRITE_OV490_CMD(pCtxt,ov490_enable_burst,ret);
		    // enable clock
		    WRITE_OV490_CMD(pCtxt,ov490_enable_clock,ret);

		    // load firmware
		    ret = ov490_write_firmware_context(pCtxt);
		    if(ret == 0)
		    {
		        pCtxt->ov490_state = OV490_STATE_BOOTING;
				SHIGH("%s: ov490 enter OV490_STATE_BOOTING",__FUNCTION__);
				//boot
				WRITE_OV490_CMD(pCtxt,ov490_enable_boot,ret);
				//enable crc
				WRITE_OV490_CMD(pCtxt,ov490_enable_download_CRCcheck,ret);

				//check boot flag
				WRITE_OV490_CMD(pCtxt,ov490_status_prefix_8019,ret);
				WAITFOR_OV490_STATUS(pCtxt,OV490_REGADDR_SRAM_FLAG_ADDR,SCCB_BOOT_JUMPBOOT_OK,300,ret);
				if(ret != 0)
				{
				    pCtxt->ov490_state = OV490_STATE_ERROR_TIMEOUT;
					continue;
				}
				else
				{
				    pCtxt->ov490_state = OV490_STATE_BOOTING2;
					SHIGH("%s: ov490 enter OV490_STATE_BOOTING2",__FUNCTION__);
					break;
				}
		    }
			else
			{
			    pCtxt->ov490_state = OV490_STATE_ERROR_TIMEOUT;
				break;
			}
	    }while((++boottime < FIRMWARE_BOOT_RETRY_TIME)&&
	    (pCtxt->ov490_state == OV490_STATE_ERROR_TIMEOUT));
	}

    if(pCtxt->ov490_state == OV490_STATE_BOOTING2)
    {
        //check running stage flag
        WRITE_OV490_CMD(pCtxt,ov490_status_prefix_8080,ret);
		WAITFOR_OV490_STATUS(pCtxt,OV490_REGADDR_RUNNING_STAGE_ADDR,FW_RUNNING_STAGE_FINAL_DONE,300,ret);
		if(ret == 0)
		{
		    pCtxt->ov490_state = OV490_STATE_STREAM_OFF;
			SHIGH("%s: ov490 enter OV490_STATE_STREAM_OFF",__FUNCTION__);
		}
		else
		{
		    pCtxt->ov490_state = OV490_STATE_ERROR_TIMEOUT;
			SERR("%s: OV490_REGADDR_RUNNING_STAGE_ADDR: error",__FUNCTION__);
		}
    }
	
    (void)ov490_check_firmware_version(pCtxt);
	SHIGH("ov490_firmware_boot state:%d e",pCtxt->ov490_state);

    ret = ov490_set_yuyv_format(pCtxt);

	return ret;
}

static int ov490_set_yuyv_format(bridge_context_t* pCtxt)
{
    int ret = 0;
    unsigned char formatReg =0;
    WRITE_OV490_CMD(pCtxt,ov490_status_prefix_8029,ret);
    READ_OV490_CMD(pCtxt,OV490_REGADDR_COLOR_FORMAT,formatReg,ret); 
    SHIGH("ov490_firmware_boot color format:0x%x",formatReg);
    formatReg = formatReg|OV490_SET_YUYV_FORMAT_BIT;
    ov490_set_format_YUYV_reg_array[0].reg_data = formatReg ;
    //set color format to YUV2(YUYV)
    WRITE_OV490_CMD(pCtxt,ov490_set_format_YUYV,ret);
    if(0 != ret)
    {
        SERR("%s: set pixel format failed return:",__FUNCTION__,ret);
    }

    return ret;
}

//#endif

static int ov490_detect_device(bridge_context_t* pCtxt)
{
    int ret = -1;

    SHIGH("%s detecting ov490 0x%x",__FUNCTION__,pCtxt->ov490_slave_addr);

	WRITE_OV490_CMD(pCtxt,ov490_detect_chipid,ret);

	if(ret == 0)
	{
        WAITFOR_OV490_STATUS(pCtxt,OV490_REGADDR_CHIPID_H,CHIP_ID_H,5,ret);
		if(ret != 0)
		{
		   SERR("%s Unable to detect chip id high (0x%x), want(0x%x) ",__FUNCTION__,read_setting.reg_array[0].reg_data,CHIP_ID_H );
		}

		WAITFOR_OV490_STATUS(pCtxt,OV490_REGADDR_CHIPID_L,CHIP_ID_L,5,ret);
		if(ret != 0)
		{
		   SERR("%s Unable to detect chip id low (0x%x), want(0x%x) ",__FUNCTION__,read_setting.reg_array[0].reg_data,CHIP_ID_L );
		}
	}
	if((ret == 0)&&(pCtxt->ov490_state == OV490_STATE_INVALID))
	{			
		pCtxt->ov490_state = OV490_STATE_DETECTED;
	}
	return ret;
}

static int ov490_start_stream(bridge_context_t* pCtxt)
{
	int ret = -1;
	if (pCtxt->ov490_state != OV490_STATE_STREAM_OFF)
    {
        SHIGH("%s: invalid state %d",__FUNCTION__,pCtxt->ov490_state);
    }
	else
	{

		CameraLockMutex(pCtxt->mutex);
		if( TI936_LOCK_PIN_LOCKED != pCtxt->bridge_sensors.lock_pin_status)
		{
			CameraUnlockMutex(pCtxt->mutex);
		    SERR("%s camera sensor unlock",__FUNCTION__);
			return -1;
		}
		CameraUnlockMutex(pCtxt->mutex);

		unsigned long startslot=0;
		unsigned long endslot=0;
		unsigned long val = 0;
		
	    WRITE_OV490_CMD(pCtxt,ov490_start_stream_cmd,ret);
		if(ret == 0)
		{   
		    test_get_time(&startslot);
			WRITE_OV490_CMD(pCtxt,ov490_status_prefix_8019,ret);
			WAITFOR_OV490_STATUS(pCtxt,OV490_REGADDR_HOSTCMD_RETURN,HOSTCMD_RETURN_NO_ERROR,100,ret);
			test_get_time(&endslot);
			val = endslot-startslot;
			if(ret != 0)
        	{
	        	SERR("%s I2C function WAITFOR_OV490_STATUS HOSTCMD_RETURN_NO_ERROR timeout %lu",__FUNCTION__,val);

				WRITE_OV490_CMD(pCtxt,ov490_status_prefix_8080,ret);

				read_setting.reg_array[0].reg_addr = 0x0120;
                (void)pCtxt->sensor_platform_fcn_tbl.i2c_slave_read( pCtxt->ctrl, pCtxt->ov490_slave_addr,&read_setting);
				
				if(read_setting.reg_array[0].reg_data == 0xEE)
				{
				    SHIGH("%s I2C function OV490_REGADDR_RUNNING_STAGE_ADDR sensor error ",__FUNCTION__);
				}
				else
				{
				    SHIGH("%s I2C function OV490_REGADDR_RUNNING_STAGE_ADDR 0x%x",__FUNCTION__,read_setting.reg_array[0].reg_data);
				}
	    	}
		    else
			{
			    pCtxt->ov490_state = OV490_STATE_STREAM_ON;
			    SHIGH("%s: ov490 enter OV490_STATE_STREAM_ON %d",__FUNCTION__,val);
			}
		}
		else
		{
		    SERR("%s I2C function failure to write start stream cmd",__FUNCTION__);
		}
	}
	return ret;
}

static int ov490_stop_stream(bridge_context_t* pCtxt)
{
	int ret = -1;

	if (pCtxt->ov490_state != OV490_STATE_STREAM_ON)
    {
        SHIGH("%s: invalid state %d",__FUNCTION__,pCtxt->ov490_state);
    }
	else
	{
		CameraLockMutex(pCtxt->mutex);
		if( TI936_LOCK_PIN_LOCKED != pCtxt->bridge_sensors.lock_pin_status)
		{
			CameraUnlockMutex(pCtxt->mutex);
			pCtxt->ov490_state = OV490_STATE_STREAM_OFF;
			SERR("%s camera sensor unlock",__FUNCTION__);
			return -1;
		}
		CameraUnlockMutex(pCtxt->mutex);

	    WRITE_OV490_CMD(pCtxt,ov490_stop_stream_cmd,ret);
	    if(ret == 0)
	    {
		    SHIGH("%s: ov490 enter OV490_STATE_STREAM_OFF",__FUNCTION__);
	    }
		pCtxt->ov490_state = OV490_STATE_STREAM_OFF;
	}
	return ret;
}

static int ov490_power_up()
{
	gpio_set_value(OV490_RESET_PIN,0);
    CameraSleep(1);//1ms
    gpio_set_value(OV490_RESET_PIN,1);
	CameraSleep(5);//1ms
	return 0;
}

static int ti936_power_up()
{
	gpio_set_value(TI936_POWER_PIN,0);
	CameraSleep(1);//1ms
	gpio_set_value(TI936_POWER_PIN,1);
	CameraSleep(2);
	//hard reset
	gpio_set_value(TI936_POWER_PIN,0);
	CameraSleep(5);
	gpio_set_value(TI936_POWER_PIN,1);
	return 0;
}


//#ifdef OV490_HOTPLUG_FEATURE_ENABLE
static bool ti936_lock_status()
{ 
	unsigned char ntime = 0;
	unsigned int pinValue = 0;
	unsigned char dtime1 =0;
	unsigned char dtime2 =0;
	bool status = true;
	bool steady = false;
	for(ntime=0;ntime<LOCK_SIGNAL_SCAN_TIMES;ntime++)
	{
		gpio_get_value(TI936_LOCK_PIN,&pinValue);
		if(pinValue == TI936_LOCK_PIN_LOST)
		{	
		    ++dtime1;
			dtime2 = 0;	
		}
		else
		{
			++dtime2;
			dtime1 = 0;
		}

		if(dtime1 >= LOCK_SIGNAL_DEBOUNCE_TIMES)
		{
			status = false;
			steady = true;
		    break;
		}
		else if(dtime2 >= LOCK_SIGNAL_DEBOUNCE_TIMES)
		{
		    status = true;
			steady = true;
			break;
		}
		CameraSleep(1);
	}


	if((ntime >= LOCK_SIGNAL_SCAN_TIMES)&&(!steady))
	{
	    SHIGH("debouce time out");
		status = false;
	}
	return status;
}

static int ti936_enable_lockpin(bridge_context_t* pCtxt,bool enable)
{
    int ret = 0;
	if(enable)
	{
		WRITE_OV490_CMD(pCtxt,ov490_enable_lock_signal,ret);
		SHIGH("hotplug %s enable lock ret: %d",__FUNCTION__,ret);
	}
	else
	{
		WRITE_OV490_CMD(pCtxt,ov490_disable_lock_signal,ret);
		SHIGH("hotplug %s disable lock ret: %d",__FUNCTION__,ret);
	}
	return ret;
}

static int ti936_lock_poll_thread(void *arg)
{
   //pthread_detach(pthread_self());
   int ret = 0;
   bridge_context_t* pLinkContext = (bridge_context_t*)arg;
   if(pLinkContext != NULL)
   {
       SHIGH("%s instance run...",__FUNCTION__);
	   while(!pLinkContext->linkdetect_handler_exit)
	   {
	       //read status of link
	       bool status;
	       status  = ti936_lock_status();
		   if(status)//lock
		   {
		       //current status
		       if(pLinkContext->bridge_sensors.lock_pin_status == TI936_LOCK_PIN_LOST)
		       {
		          //change from lost to locked
				  CameraLockMutex(pLinkContext->mutex);
				  pLinkContext->bridge_sensors.lock_pin_status = TI936_LOCK_PIN_LOCKED;
				  CameraUnlockMutex(pLinkContext->mutex);
				  
				  //notify user
				  struct camera_input_status_t sensor_status;
                  sensor_status.src = 0;
                  sensor_status.event = INPUT_SIGNAL_VALID_EVENT;
                  pLinkContext->sensor_platform_fcn_tbl.input_status_cb(pLinkContext->ctrl, &sensor_status);
				  SHIGH("%s hotplug push INPUT_SIGNAL_VALID_EVENT",__FUNCTION__);
				  
		       }
			   //else//keep connected
			   //{
			   //}
		   }
		   else
		   {
		       if(pLinkContext->bridge_sensors.lock_pin_status == TI936_LOCK_PIN_LOCKED)
		       {
		          //change from locked to lost	
		          CameraLockMutex(pLinkContext->mutex);
				  pLinkContext->bridge_sensors.lock_pin_status = TI936_LOCK_PIN_LOST;
				  CameraUnlockMutex(pLinkContext->mutex);

				  //notify user
				  struct camera_input_status_t sensor_status;
                  sensor_status.src = 0;
                  sensor_status.event = INPUT_SIGNAL_LOST_EVENT;
                  pLinkContext->sensor_platform_fcn_tbl.input_status_cb(pLinkContext->ctrl, &sensor_status);
				  SHIGH("%s hotplug push INPUT_SIGNAL_LOST_EVENT",__FUNCTION__);
		       }
			   //else// keep lost
			   //{
			   //}
		   }
		   
#ifdef OV490_ISP_HEALTH_FEATURE_ENABLE
           if((pLinkContext->ov490_state != OV490_STATE_POWERDOWN)&&
		   	(pLinkContext->ov490_state != OV490_STATE_ERROR_TIMEOUT))
		        (void)ov490_isp_status_dtc_report(pLinkContext);
#endif
		   CameraSleep(200);
	   }
   }
   SHIGH("%s instance exit...",__FUNCTION__);
   return ret;
}

static int start_lock_poll(bridge_context_t* pCtxt)
{
    if(pCtxt == NULL)
		return -1;
	ti936_lock_event_thread_t pEventThreadFcn = NULL;
	pEventThreadFcn = ti936_lock_poll_thread;
	(void)CameraCreateThread(CAMERA_THREAD_PRIO_DEFAULT, 0,
                    pEventThreadFcn,
                    (void*)(pCtxt),
                    0,
                    "lockevent_poll",
                    &pCtxt->linkdetect_handler_tid);
    (void)CameraSetThreadPriority(pCtxt->linkdetect_handler_tid,
                    CAMERA_THREAD_PRIO_BACKEND);
	return 0;
}

static int stop_lock_poll(bridge_context_t* pCtxt)
{
	pCtxt->linkdetect_handler_exit = FALSE;
	CameraReleaseThread(pCtxt->linkdetect_handler_tid);
	return 0;
}

static int ov9284_init(bridge_context_t* pCtxt)
{
    int ret= -1;

    WRITE_OV490_CMD(pCtxt,ov490_request_init_sensor,ret);

	if(ret == 0)
	{
	    WAITFOR_OV490_STATUS(pCtxt,OV490_REGADDR_HOSTCMD_RETURN,HOSTCMD_RETURN_NO_ERROR,100,ret);
		if(ret != 0)
		{
		    SERR("%s I2C function HOSTCMD_RETURN_NO_ERROR timeout",__FUNCTION__);
			int tmp;
			WAITFOR_OV490_STATUS(pCtxt,OV490_REGADDR_RUNNING_STAGE_ADDR,FW_RUNNING_STAGE_SNR_ID_ERR,5,tmp);
			SHIGH("%s I2C function sensor error ",__FUNCTION__);
			//DTC if needed
		}
	}
	return ret;
}


static bool ov490_isp_status(bridge_context_t* pCtxt) //benny20190416_isp_health
{
   int ret =-1;
   bool status = true;
   unsigned char reg1 =0;
   unsigned char reg2 =0;
   unsigned int tmp_cnt = 0;
   static unsigned char halt_cnt = 0;
   
   CameraLockMutex(pCtxt->mutex);
   WRITE_OV490_CMD(pCtxt,ov490_status_prefix_8080,ret);
   READ_OV490_CMD(pCtxt,OV490_REGADDR_ISP_TICK_HIGH,reg1,ret);
   READ_OV490_CMD(pCtxt,OV490_REGADDR_ISP_TICK_LOW,reg2,ret); 
   CameraUnlockMutex(pCtxt->mutex);

   tmp_cnt = (reg1<<8)|reg2;
   //SERR("%s hotplug:  hbeat tick 0x%x, H:0x%x, L:0x%x ",__FUNCTION__,tmp_cnt,reg1,reg2);
   if(tmp_cnt != pCtxt->bridge_sensors.ov490_isp_ticks)
   {    
       pCtxt->bridge_sensors.ov490_isp_ticks = tmp_cnt;
	   halt_cnt = 0;
   }
   else
   {
   	   pCtxt->bridge_sensors.ov490_isp_ticks = tmp_cnt;  
       if(halt_cnt< OV490_ISP_SETTLE_CNT)halt_cnt++;
	   if(halt_cnt >= OV490_ISP_SETTLE_CNT)
	   {
	       //DTC out
	       SERR("%s hotplug: DTC isp halted",__FUNCTION__);
		   status = false;
	   }
   }
   return status;
}

static int ov490_isp_status_dtc_report(bridge_context_t* pCtxt) //benny20190416_isp_health
{
	int ret = -1;
    static bool dtc_fault = false;
	bool status = false;
	status = ov490_isp_status(pCtxt);
	if(status && dtc_fault)
	{
	    //recover 
	    SHIGH("%s hotplug: current ov490 state %d",__FUNCTION__,pCtxt->ov490_state);
	    SHIGH("%s hotplug: DTC isp recover",__FUNCTION__);
		dtc_fault = false;
	}
	else if(status && !dtc_fault)
	{
	    //always run
	    dtc_fault = false;
	}
	else if(!status && dtc_fault)
	{
	    //need report fault
	    SHIGH("%s hotplug: current ov490 state %d",__FUNCTION__,pCtxt->ov490_state);
	    SHIGH("%s hotplug: DTC isp fault",__FUNCTION__);
		dtc_fault = true;
	}
	else
	{ 
	    //always halt
	    dtc_fault = true;
	}
	ret++;
	ret--;
	return ret;
}

//#endif





CAM_API CameraResult CameraSensorDevice_Open_ov490(CameraDeviceHandleType** ppNewHandle,
        CameraDeviceIDType deviceId)
{
    sensor_lib_interface_t sensor_lib_interface = {
            .sensor_open_lib = ov490_sensor_open_lib,
    };

    return CameraSensorDevice_Open(ppNewHandle, deviceId, &sensor_lib_interface);
}
