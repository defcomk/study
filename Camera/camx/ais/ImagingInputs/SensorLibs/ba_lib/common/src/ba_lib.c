/**
 * @file ba_lib.c
 *
 * Copyright (c) 2017-2019 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "CameraSensorDeviceInterface.h"

#include "ba_lib.h"
#include "SensorDebug.h"

#include "avin_ioctl.h"
#include "ioctlClient.h"
#include "avin_types.h"
#include "avin_utils.h"

#define BA_DEVICE_NAME "avinput/ba"
#define BA_DEVICE_IOCTL_BUFF_SIZE                 (256)
#define BA_DEVICE_CVBS_PORT_ID                    (0)
#define BA_DEVICE_HDMI_PORT_ID                    (1)

typedef struct
{
    /*must be first element*/
    sensor_lib_t sensor_lib;

    sensor_platform_func_table_t sensor_platform_fcn_tbl;

    unsigned int num_supported_sensors;
    unsigned int num_connected_sensors;

    uint32 ba_num_ports;

    uint32 ba_port_id;

    ioctl_session_t* ba_hdl;
    uint8 ba_msg_buffer[BA_DEVICE_IOCTL_BUFF_SIZE];

    boolean is_cvbs;

    boolean is_connected;

    void* ctrl;
} ba_context_t;

static sensor_lib_t ba_lib_ptr =
{
  .sensor_slave_info =
  {
      .sensor_name = SENSOR_MODEL,
      .addr_type = CAMERA_I2C_BYTE_ADDR,
      .data_type = CAMERA_I2C_BYTE_DATA,
      .i2c_freq_mode = SENSOR_I2C_MODE_CUSTOM,
  },
  .num_channels = 1,
  .channels =
  {
    {
      .output_mode =
      {
        .fmt = QCARCAM_FMT_UYVY_8,
        .res = {.width = 720, .height = 507, .fps = 30.0f},
        .channel_info = {.vc = 0, .dt = DT_AV7481, .cid = CID_VC0,},
        .interlaced = TRUE,
      },
      .num_subchannels = 1,
      .subchan_layout =
      {
        {
          .src_id = 0,
          .x_offset = 0,
          .y_offset = 0,
          .stride = 1440,
        },
      },
    },
  },
  .num_subchannels = 1,
  .subchannels =
  {
    {
      .src_id = 0,
      .modes =
      {
        {
          .fmt = QCARCAM_FMT_UYVY_8,
          .res = {.width = 720, .height = 507, .fps = 30.0f},
          .channel_info = {.vc = 0, .dt = DT_AV7481, .cid = CID_VC0,},
          .interlaced = TRUE,
        },
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
    .lane_cnt = 1,
    .settle_cnt = BA_SETTLE_COUNT,
    .lane_mask = 0x1F,
    .combo_mode = 0,
    .is_csi_3phase = 0,
  },
  .sensor_close_lib = &ba_sensor_close_lib,
  .sensor_capability = (1 << SENSOR_CAPABILITY_COLOR_CONFIG),
  .sensor_custom_func =
  {
    .sensor_detect_device = &ba_sensor_detect_device,
    .sensor_detect_device_channels = &ba_sensor_detect_device_channels,
    .sensor_init_setting = &ba_sensor_init_setting,
    .sensor_set_channel_mode = &ba_sensor_set_channel_mode,
    .sensor_power_suspend = &ba_sensor_power_suspend,
    .sensor_power_resume = &ba_sensor_power_resume,
    .sensor_start_stream = &ba_sensor_start_stream,
    .sensor_stop_stream = &ba_sensor_stop_stream,
    .sensor_config_resolution = &ba_sensor_config_resolution,
    .sensor_query_field = &ba_sensor_field_info_query,
    .sensor_set_platform_func_table = &ba_sensor_set_platform_func_table,
    .sensor_s_param = &ba_sensor_s_param,
  },
  .use_sensor_custom_func = TRUE,
};


static int ba_lib_evt_handler
(
    uint8* msg,
    uint32 length,
    void* cd
)
{
   avin_drv_msg_type*   cb_msg = (avin_drv_msg_type*) msg;
   ba_context_t*        pCtxt  =  (ba_context_t*) cd;

   if ( !cb_msg || !length || !pCtxt )
   {
      SENSOR_ERROR("Null pointer of msg 0x%p or length %d", msg, length );
      return 0;
   }

   struct camera_input_status_t sensor_status;
   memset( &sensor_status, 0, sizeof(sensor_status) );

   if ( AVIN_EVT_PORT_CONNECTED == cb_msg->event )
   {
      uint32 port_id = cb_msg->info.port_id;

      /* currently cvbs/hdmi has separate device with only one port associated with pCtxt */
      sensor_status.src = 0;
      sensor_status.event = INPUT_SIGNAL_VALID_EVENT;

      SENSOR_HIGH("Return INPUT_SIGNAL_VALID_EVENT event to client pCtxt 0x%p ctrl 0x%p port %d", pCtxt, pCtxt->ctrl, port_id);
      pCtxt->sensor_platform_fcn_tbl.input_status_cb(pCtxt->ctrl, &sensor_status);
   }
   else if ( AVIN_EVT_PORT_DISCONNECTED == cb_msg->event )
   {
      uint32 port_id = cb_msg->info.port_id;

      /* currently cvbs/hdmi has separate device with only one port associated with pCtxt */
      sensor_status.src = 0;
      sensor_status.event = INPUT_SIGNAL_LOST_EVENT;

      SENSOR_HIGH("Return INPUT_SIGNAL_LOST_EVENT event to client pCtxt pCtxt 0x%p ctrl 0x%p port %d", pCtxt, pCtxt->ctrl, port_id);
      pCtxt->sensor_platform_fcn_tbl.input_status_cb(pCtxt->ctrl, &sensor_status);
   }
   else if ( AVIN_EVT_PORT_SETTINGS_CHANGED == cb_msg->event )
   {
      uint32 port_id = cb_msg->info.port_id;
      SENSOR_HIGH("Recevied AVIN_EVT_PORT_SETTINGS_CHANGED event pCtxt pCtxt 0x%p port %d", pCtxt, port_id);
   }
   else
   {
      SENSOR_ERROR("Recevied unsupported event 0x%x", cb_msg->event );
   }

   return 0;
}

/**
 * FUNCTION: ba_sensor_open_lib
 *
 * DESCRIPTION: Open sensor library and returns data pointer
 **/
static void* ba_sensor_open_lib(void* ctrl, void* arg)
{
   SENSOR_HIGH("open BA lib called ctrl %p", ctrl);
   sensor_open_lib_t* device_info = (sensor_open_lib_t*)arg;

   ba_context_t* pCtxt = calloc(1, sizeof(ba_context_t));
   if (pCtxt)
   {
       memcpy(&pCtxt->sensor_lib, &ba_lib_ptr, sizeof(pCtxt->sensor_lib));

       pCtxt->ctrl = ctrl;
       pCtxt->sensor_lib.sensor_slave_info.camera_id = device_info->camera_id;
       pCtxt->num_supported_sensors = 2;

       //Default is cvbs. Update info if HDMI.
       pCtxt->is_cvbs = !device_info->subdev_id;
       if (!pCtxt->is_cvbs)
       {
           pCtxt->sensor_lib.channels[0].output_mode.res.width = 1920;
           pCtxt->sensor_lib.channels[0].output_mode.res.height = 1080;
           pCtxt->sensor_lib.channels[0].subchan_layout[0].stride = 3840;
           pCtxt->sensor_lib.subchannels[0].modes[0].res.width = 1920;
           pCtxt->sensor_lib.subchannels[0].modes[0].res.height = 1080;
           pCtxt->sensor_lib.subchannels[0].modes[0].interlaced = FALSE;
           pCtxt->sensor_lib.csi_params.lane_cnt = 4;
           pCtxt->ba_port_id = BA_DEVICE_HDMI_PORT_ID;
       }
       else
       {
           pCtxt->sensor_lib.subchannels[0].modes[0].interlaced = TRUE;
           pCtxt->ba_port_id = BA_DEVICE_CVBS_PORT_ID;
       }
       SENSOR_HIGH("opened BA successfully pCtxt 0x%p ctrl 0x%p is_cvbs? %d", pCtxt, pCtxt->ctrl, pCtxt->is_cvbs);
   }

   return pCtxt;
}

static int ba_sensor_close_lib(void* ctrl)
{
   SENSOR_HIGH("close BA lib ctrl 0x%p called", ctrl);

   if ( ctrl )
   {
      free(ctrl);
   }
   else
   {
      SENSOR_ERROR("Null pointer ctrl");
   }

   return 0;
}

/**
 * FUNCTION: ba_sensor_init_setting
 *
 * DESCRIPTION: init setting of ba driver
 **/
static int ba_sensor_init_setting (void* ctxt)
{
   SENSOR_HIGH("ba_sensor_init_setting ctxt 0x%p called", ctxt);

   return 0;
}


/**
 * FUNCTION: ba_sensor_detect_device
 *
 * DESCRIPTION: detect and initialize BA driver
 **/
static int ba_sensor_detect_device (void* ctxt)
{
   SENSOR_HIGH("ba_sensor_detect_device ctxt 0x%p called", ctxt);

   return 0;
}

/**
 * FUNCTION: ba_sensor_detect_device_channels
 *
 * DESCRIPTION: open BA instance
 **/
static int ba_sensor_detect_device_channels (void* ctxt)
{

   SENSOR_HIGH("ba_sensor_detect_device_channels ctxt 0x%p called", ctxt);

   ba_context_t* pCtxt = (ba_context_t*)ctxt;

   if ( !pCtxt )
   {
      SENSOR_ERROR("Null pointer ctxt");
      return -1;
   }

   pCtxt->sensor_lib.src_id_enable_mask = 1;

   return 0;
}

/**
 * FUNCTION: ba_sensor_set_channel_mode
 *
 * DESCRIPTION: connect ba input port
 **/
static int ba_sensor_set_channel_mode (void* ctxt, unsigned int src_id_mask, unsigned int mode)
{
   (void) mode;
   (void) src_id_mask;
   ba_context_t* pCtxt = (ba_context_t*)ctxt;
   avin_status_type rc = AVIN_SUCCESS;

   if ( !pCtxt )
   {
      SENSOR_ERROR("Null pointer ctxt");
      return -1;
   }

   SENSOR_HIGH("BA set_channel_mode pCtxt 0x%p called", pCtxt);

   uint32* port_id;
   uint32 msg_size;

   port_id = (uint32*) pCtxt->ba_msg_buffer;
   *port_id = pCtxt->ba_port_id;
   msg_size = sizeof(uint32);

   SENSOR_HIGH("ba 0x%p start connecting port %d ...", pCtxt, pCtxt->ba_port_id );

   rc = (avin_status_type)device_ioctl(pCtxt->ba_hdl, AVIN_IOCTL_CONNECT,
                                      pCtxt->ba_msg_buffer, msg_size,
                                      NULL, 0);

   if ( AVIN_SUCCESS == rc )
   {
       // connected already
       SENSOR_HIGH("BA connected to port %d already ", pCtxt->ba_port_id );
       pCtxt->is_connected = TRUE;
   }
   else if ( AVIN_PENDING == rc )
   {
      /* currently can't support if cable is not connected */
      SENSOR_HIGH("BA pending for connection to port %d", pCtxt->ba_port_id );
   }
   else
   {
       SENSOR_ERROR("BA connection to port %d failed", pCtxt->ba_port_id);
   }

   return ( AVIN_IS_SUCCESS(rc)? 0 : -1 );
}

/**
 * FUNCTION: ba_sensor_power_resume
 *
 * DESCRIPTION: BA open and connect channel
 **/
static int ba_sensor_power_resume( void* ctxt )
{
   avin_status_type rc = AVIN_SUCCESS;
   ba_context_t* pCtxt = (ba_context_t*)ctxt;

   if ( !pCtxt )
   {
      SENSOR_ERROR("Null pointer ctxt");
      return -1;
   }

   SENSOR_HIGH("ba_sensor_power_resume pCtxt 0x%p called", pCtxt );

   ioctl_callback_t callback;

   callback.handler = ba_lib_evt_handler;
   callback.data    = pCtxt;

   pCtxt->ba_hdl = device_open( BA_DEVICE_NAME, &callback );

   if ( pCtxt->ba_hdl )
   {
      SENSOR_ERROR("device_open %s succeed handle 0x%p", BA_DEVICE_NAME, pCtxt->ba_hdl );

      avin_property_hdr_type *prop       = (avin_property_hdr_type *)pCtxt->ba_msg_buffer;
      uint32                 msg_size    = sizeof(avin_property_hdr_type) + sizeof(uint32);


      prop->prop_id = AVIN_PROP_NUMBER_PORTS;
      prop->size = sizeof(uint32);

      rc = (avin_status_type)device_ioctl(pCtxt->ba_hdl, AVIN_IOCTL_GET_PROPERTY,
                                          pCtxt->ba_msg_buffer, msg_size,
                                          (uint8*)&pCtxt->ba_num_ports, sizeof(uint32) );
   }
   else
   {
      SENSOR_ERROR("device_open %s failed", BA_DEVICE_NAME );
      rc = AVIN_ERR_FAIL;
   }

   return ( AVIN_IS_SUCCESS(rc)? 0 : -1 );

}

/**
 * FUNCTION: ba_sensor_power_suspend
 *
 * DESCRIPTION: BA disconnect and close
 **/
static int ba_sensor_power_suspend( void* ctxt )
{
   avin_status_type rc = AVIN_SUCCESS;
   ba_context_t* pCtxt = (ba_context_t*)ctxt;

   if ( !pCtxt )
   {
      SENSOR_ERROR("Null pointer ctxt");
      return -1;
   }


   SENSOR_HIGH("ba_sensor_power_suspend pCtxt 0x%p called", pCtxt);

   if ( pCtxt->ba_hdl )
   {
      if ( pCtxt->is_connected )
      {
         rc = (avin_status_type)device_ioctl( pCtxt->ba_hdl, AVIN_IOCTL_DISCONNECT,
                                              NULL, 0, NULL, 0);

         if ( AVIN_IS_SUCCESS(rc) )
         {
            SENSOR_HIGH("Disconnect ba handle 0x%p success", pCtxt->ba_hdl);
         }
         else
         {
            SENSOR_ERROR("Disconnect ba handle 0x%p failed", pCtxt->ba_hdl);
         }

         pCtxt->is_connected = FALSE;
      }

      rc = (avin_status_type)device_close( pCtxt->ba_hdl );

      if( AVIN_IS_SUCCESS(rc) )
      {
         SENSOR_HIGH("Close ba handle 0x%p success", pCtxt->ba_hdl);
      }
      else
      {
         SENSOR_ERROR("Close ba handle 0x%p failed", pCtxt->ba_hdl);
      }

      pCtxt->ba_hdl = NULL;
   }

   return 0;
}

/**
 * FUNCTION: ba_sensor_start_stream
 *
 * DESCRIPTION: BA start stream
 **/
static int ba_sensor_start_stream(void* ctxt, unsigned int src_id_mask)
{
    (void)src_id_mask;
    avin_status_type rc = AVIN_SUCCESS;
    ba_context_t* pCtxt = (ba_context_t*)ctxt;

    if ( !pCtxt )
    {
       SENSOR_ERROR("Null pointer ctxt");
       return -1;
    }

    SENSOR_HIGH("ba_sensor_start_stream pCtxt 0x%p called", pCtxt);

    rc = (avin_status_type)device_ioctl(pCtxt->ba_hdl, AVIN_IOCTL_START,
                                        NULL, 0, NULL, 0 );

    if ( AVIN_IS_SUCCESS(rc) )
    {
       SENSOR_HIGH("BA stream started ...");
    }
    else
    {
       SENSOR_ERROR("BA starts stream failed");
    }

    return (AVIN_IS_SUCCESS(rc)? 0 : -1);
}

/**
 * FUNCTION: ba_sensor_stop_stream
 *
 * DESCRIPTION: BA stop stream
 **/
static int ba_sensor_stop_stream(void* ctxt, unsigned int src_id_mask)
{
    (void)src_id_mask;

    avin_status_type rc;
    ba_context_t* pCtxt = (ba_context_t*)ctxt;

    if ( !pCtxt )
    {
       SENSOR_ERROR("Null pointer ctxt");
       return -1;
    }

    SENSOR_HIGH("ba_sensor_stop_stream pCtxt 0x%p called", pCtxt);

    rc = (avin_status_type)device_ioctl(pCtxt->ba_hdl, AVIN_IOCTL_STOP,
                                        NULL, 0, NULL, 0 );

    if ( AVIN_IS_SUCCESS(rc) )
    {
       SENSOR_HIGH("BA stream stopped ...");
    }
    else
    {
       SENSOR_ERROR("BA stops stream failed");
    }

    return (AVIN_IS_SUCCESS(rc)? 0 : -1);
}


/**
 * FUNCTION: ba_sensor_field_info_query
 *
 * DESCRIPTION: get the current field info
 **/
static int ba_sensor_field_info_query (void* ctxt, boolean *even_field, uint64_t *field_ts)
{
   avin_status_type rc;
   ba_context_t* pCtxt = (ba_context_t*)ctxt;
   avin_property_port_field_info_type fld_info;

   fld_info.even_field = 0;
   fld_info.time = 0;

   if ( !pCtxt )
   {
      SENSOR_ERROR("Null pointer ctxt");
      return -1;
   }

   avin_property_hdr_type *prop       = (avin_property_hdr_type *)pCtxt->ba_msg_buffer;
   uint32                 msg_size    = sizeof(avin_property_hdr_type) +
                                          sizeof(avin_property_port_field_info_type);

   prop->prop_id = AVIN_PROP_PORT_FIELD_INFO;
   prop->port = pCtxt->ba_port_id;
   prop->size = sizeof(avin_property_port_field_info_type);

   rc = (avin_status_type)device_ioctl( pCtxt->ba_hdl, AVIN_IOCTL_GET_PROPERTY,
                          pCtxt->ba_msg_buffer, msg_size,
                          (uint8*)&fld_info, sizeof(avin_property_port_field_info_type) );

   if ( AVIN_IS_SUCCESS(rc) )
   {
      *even_field = fld_info.even_field;
      *field_ts = fld_info.time * 1000000; // nsec
      SENSOR_HIGH("even_field %d time %lld", *even_field, *field_ts );
   }
   else
   {
      SENSOR_ERROR("ba get field info failed with status %d", rc );
   }

   return ( AVIN_IS_SUCCESS(rc)? 0 : -1 );
}

/**
 * FUNCTION: ba_sensor_config_resolution
 *
 * DESCRIPTION: configure ba resolution
 **/
static int ba_sensor_config_resolution( void* ctxt, int32 width, int32 height )
{
   ba_context_t* pCtxt = (ba_context_t*)ctxt;

   if ( !pCtxt )
   {
      SENSOR_ERROR("Null pointer ctxt");
      return -1;
   }

   SENSOR_HIGH("ba_sensor_config_resolution pCtxt 0x%p called", pCtxt);

   if ( width != pCtxt->sensor_lib.channels[0].output_mode.res.width ||
        height!= pCtxt->sensor_lib.channels[0].output_mode.res.height )
   {
      SENSOR_ERROR("ba_sensor_config_resolution %d x %d failed", width, height );
      return -1;
   }

   return 0;
}

/**
 * FUNCTION: ba_sensor_set_i2c_func_table
 *
 * DESCRIPTION: set i2c function table
 **/
static int ba_sensor_set_platform_func_table (void* ctxt, sensor_platform_func_table_t* table)
{
    ba_context_t* pCtxt = (ba_context_t*)ctxt;

    if ( !pCtxt || !table )
    {
       SENSOR_ERROR("ba_sensor_set_platform_func_table NULL pointer");
       return -1;
    }

    pCtxt->sensor_platform_fcn_tbl = *table;
    SENSOR_HIGH("ba_sensor_set_i2c_func_table 0x%p", table);

    return 0;
}

/**
 * FUNCTION: ba_sensor_s_param
 *
 * DESCRIPTION: set color parameters
 **/
static int ba_sensor_s_param(void* ctxt, qcarcam_param_t param, unsigned int src_id, void* p_param)
{
   avin_status_type rc = AVIN_SUCCESS;
   ba_context_t* pCtxt = (ba_context_t*)ctxt;
   avin_drv_property_type *property = NULL;
   avin_property_hdr_type *prop     = NULL;
   uint32                 msg_size;

   if ( !pCtxt )
   {
      SERR("Null pointer ctxt");
      return -1;
   }

   if ( p_param == NULL )
   {
      SERR("ba_sensor_s_param Invalid params");
      return -1;
   }

   property = (avin_drv_property_type *)pCtxt->ba_msg_buffer;
   prop     = (avin_property_hdr_type *)pCtxt->ba_msg_buffer;

   switch(param)
   {
      case QCARCAM_PARAM_SATURATION:
      {
         avin_property_port_sat_str_type sat;
         msg_size    = sizeof(avin_property_hdr_type) +
                       sizeof(avin_property_port_sat_str_type);

         prop->prop_id = AVIN_PROP_PORT_SATURATION;
         prop->port = pCtxt->ba_port_id;
         prop->size = sizeof(avin_property_port_sat_str_type);

         sat.type = AVIN_COLOR_SATURATION_U;
         sat.str.strength = *((float*) p_param);
         avin_memcpy( &property->prop_data, &sat, sizeof(sat) );

         rc = (avin_status_type)device_ioctl( pCtxt->ba_hdl, AVIN_IOCTL_SET_PROPERTY,
                          pCtxt->ba_msg_buffer, msg_size,
                          NULL, 0 );

         if( !AVIN_IS_SUCCESS(rc) )
         {
            SERR("set AVIN_COLOR_SATURATION_U strengh %f failed", sat.str.strength );
         }
         else
         {
            sat.type = AVIN_COLOR_SATURATION_V;
            sat.str.strength = *((float*) p_param);
            avin_memcpy( &property->prop_data, &sat, sizeof(sat) );

            rc = (avin_status_type)device_ioctl( pCtxt->ba_hdl, AVIN_IOCTL_SET_PROPERTY,
                             pCtxt->ba_msg_buffer, msg_size,
                             NULL, 0 );
         }

         if( !AVIN_IS_SUCCESS(rc) )
         {
            SERR("set AVIN_COLOR_SATURATION_V strengh %f failed", sat.str.strength );
         }
         break;
      }
      case QCARCAM_PARAM_HUE:
      {
         msg_size    = sizeof(avin_property_hdr_type) + sizeof(avin_strength_type);
         avin_strength_type hue;

         prop->prop_id = AVIN_PROP_PORT_HUE;
         prop->port = pCtxt->ba_port_id;
         prop->size = sizeof(avin_strength_type);

         hue.strength = *((float*) p_param);
         avin_memcpy( &property->prop_data, &hue, sizeof(hue) );

         rc = (avin_status_type)device_ioctl( pCtxt->ba_hdl, AVIN_IOCTL_SET_PROPERTY,
                          pCtxt->ba_msg_buffer, msg_size,
                          NULL, 0 );

         if( !AVIN_IS_SUCCESS(rc) )
         {
            SERR("set AVIN_PROP_PORT_HUE strengh %f failed", hue.strength );
         }
         break;
      }
      default:
      {
         SERR("ba_sensor_s_param. Param not supported = %d", param);
         rc = AVIN_ERR_BAD_PARAMETER;
         break;
      }
   }

   return ( AVIN_IS_SUCCESS(rc)? 0 : -1 );
}

/**
 * FUNCTION: CameraSensorDevice_Open_ba
 *
 * DESCRIPTION: Entry function for device driver framework
 **/
CAM_API CameraResult CameraSensorDevice_Open_ba(CameraDeviceHandleType** ppNewHandle,
        CameraDeviceIDType deviceId)
{
    sensor_lib_interface_t sensor_lib_interface = {
            .sensor_open_lib = ba_sensor_open_lib,
    };

    return CameraSensorDevice_Open(ppNewHandle, deviceId, &sensor_lib_interface);
}
