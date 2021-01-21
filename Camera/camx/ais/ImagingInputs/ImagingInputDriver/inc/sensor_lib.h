/**
 * @file sensor_lib.h
 *
 * Copyright (c) 2012-2019 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#ifndef __SENSOR_LIB_H_
#define __SENSOR_LIB_H_

#include "sensor_sdk_common.h"

/*
 * Sensor driver version is given by:
 * <Major version>.<Minor version>.<Patch version>
 */
#define SENSOR_DRIVER_VERSION "2.2.0"
#define SENSOR_SDK_CAPABILITIES "Automotive"

#define MAX_CID 16

#define CSI_DT_EMBED_DATA        0x12
#define CSI_DT_RESERVED_DATA_0   0x13
#define CSI_DT_YUV422_8          0x1E
#define CSI_DT_YUV422_10         0x1F
#define CSI_DT_RGB888            0x24
#define CSI_DT_RAW8              0x2A
#define CSI_DT_RAW10             0x2B
#define CSI_DT_RAW12             0x2C
#define CSI_DT_RAW14             0x2D
#define CSI_DT_RAW16             0x2E
#define CSI_DT_RAW20             0x2F

#define CSI_DECODE_6BIT         0
#define CSI_DECODE_8BIT         1
#define CSI_DECODE_10BIT        2
#define CSI_DECODE_12BIT        3
#define CSI_DECODE_DPCM_10_8_10 5

/* Non HFR mode for normal camera, camcorder usecases */
#define SENSOR_DEFAULT_MODE (1 << 0)
/* HFR mode used to capture slow motion video */
#define SENSOR_HFR_MODE (1 << 1)
/* HDR mode used to High Dynamic Range imaging */
#define SENSOR_HDR_MODE (1 << 2)
/* RAW HDR mode used to stream raw HDR */
#define SENSOR_RAW_HDR_MODE (1 << 3)
/* Macro for using proper chromatix library. */
#define SENSOR_LOAD_CHROMATIX(name, mode) \
  "libchromatix_" name "_" #mode ".so"
/* MOUNT ANGLE > = to this value is considered invalid in sensor lib */
#define SENSOR_MOUNTANGLE_360 360
/* Sensor mount angle. */
#define SENSOR_MOUNTANGLE_0 0
#define SENSOR_MOUNTANGLE_90 90
#define SENSOR_MOUNTANGLE_180 180
#define SENSOR_MOUNTANGLE_270 270

/* OEM's can extend these modes */
#define MAX_RESOLUTION_MODES 13
#define MAX_META_DATA_SIZE 3
#define MAX_SENSOR_STREAM 5
#define MAX_SENSOR_DATA_TYPE 5
#define MAX_SENSOR_EFFECT 3

#define SENSOR_VIDEO_HDR_FLAG ((1 << 8) | 0)
#define SENSOR_SNAPSHOT_HDR_FLAG ((1 << 15) | 0)
#define SENSOR_AWB_UPDATE ((1 << 8) | 1)
#define MAX_SENSOR_SETTING_I2C_REG 1024

#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRUE
#define TRUE 1
#endif

enum camsensor_mode {
  CAMSENSOR_MODE_2D =      (1 << 0),
  CAMSENSOR_MODE_3D =      (1 << 1),
  CAMSENSOR_MODE_INVALID = (1 << 2),
};

typedef enum {
  SENSOR_DELAY_EXPOSURE,            /* delay for exposure*/
  SENSOR_DELAY_ANALOG_SENSOR_GAIN,  /* delay for sensor analog gain*/
  SENSOR_DELAY_DIGITAL_SENSOR_GAIN, /* delay for sensor digital gain*/
  SENSOR_DELAY_ISP_GAIN,            /* delay for sensor ISP (error) gain*/
  SENSOR_DELAY_MAX,
} sensor_delay_type_t;

typedef enum {
  SENSOR_BAYER,
  SENSOR_YCBCR,
  SENSOR_META,
} sensor_output_format_t;

typedef enum {
  SENSOR_PARALLEL,
  SENSOR_MIPI_CSI,
  SENSOR_MIPI_CSI_1,
  SENSOR_MIPI_CSI_2,
} sensor_connection_mode_t;

typedef enum {
  SENSOR_8_BIT_DIRECT,
  SENSOR_10_BIT_DIRECT,
  SENSOR_12_BIT_DIRECT
} sensor_raw_output_t;

typedef enum {
  SENSOR_BGGR,
  SENSOR_GBRG,
  SENSOR_GRBG,
  SENSOR_RGGB,

  SENSOR_UYVY,
  SENSOR_YUYV,
  SENSOR_RGB,
} sensor_filter_arrangement;

typedef enum {
  SENSOR_SMETHOD_NOT_DEFINED = 1,
  SENSOR_SMETHOD_ONE_CHIP_COLOR_AREA_SENSOR=2,
  SENSOR_SMETHOD_TWO_CHIP_COLOR_AREA_SENSOR=3,
  SENSOR_SMETHOD_THREE_CHIP_COLOR_AREA_SENSOR=4,
  SENSOR_SMETHOD_COLOR_SEQ_AREA_SENSOR=5,
  SENSOR_SMETHOD_TRILINEAR_SENSOR=7,
  SENSOR_SMETHOD_COLOR_SEQ_LINEAR_SENSOR=8
}sensor_sensing_method_type_t;

typedef enum {
  SENSOR_TEST_PATTERN_OFF,
  SENSOR_TEST_PATTERN_SOLID_COLOR,
  SENSOR_TEST_PATTERN_COLOR_BARS,
  SENSOR_TEST_PATTERN_COLOR_BARS_FADE_TO_GRAY,
  SENSOR_TEST_PATTERN_PN9,
  SENSOR_TEST_PATTERN_CUSTOM1,
  SENSOR_TEST_PATTERN_MAX,
} sensor_test_pattern_t;

typedef enum {
  SENSOR_EFFECT_OFF,
  SENSOR_EFFECT_SET_SATURATION,
  SENSOR_EFFECT_SET_CONTRAST,
  SENSOR_EFFECT_SET_SHARPNESS,
  SENSOR_EFFECT_SET_ISO,
  SENSOR_EFFECT_SET_EXPOSURE_COMPENSATION,
  SENSOR_EFFECT_SET_ANTIBANDING,
  SENSOR_EFFECT_SET_WHITEBALANCE,
  SENSOR_EFFECT_SET_BEST_SHOT,
  SENSOR_EFFECT_SET_EFFECT,
  SENSOR_EFFECT_MAX,
} sensor_effect_t;

struct sensor_id_info_t {
  unsigned short sensor_id_reg_addr;
  unsigned short sensor_id;
  unsigned short sensor_id_mask;
};

struct camera_sensor_slave_info {
  char sensor_name[32];
  enum sensor_camera_id camera_id;
  unsigned short slave_addr;
  enum camera_i2c_freq_mode i2c_freq_mode;
  enum camera_i2c_reg_addr_type addr_type;
  enum camera_i2c_data_type data_type;
  struct sensor_id_info_t sensor_id_info;
  struct camera_power_setting_array power_setting_array;
  unsigned char is_init_params_valid;
};

struct sensor_test_mode_addr_t {
  unsigned short r_addr;
  unsigned short gr_addr;
  unsigned short gb_addr;
  unsigned short b_addr;
};

typedef struct {
  sensor_test_pattern_t mode;
  struct camera_i2c_reg_setting_array settings;
} sensor_test_pattern_settings;

typedef struct {
  sensor_test_pattern_settings test_pattern_settings[SENSOR_TEST_PATTERN_MAX];
  unsigned char size;
  struct sensor_test_mode_addr_t solid_mode_addr;
} sensor_test_info;

struct sensor_effect_settings{
  sensor_effect_t mode;
  struct camera_i2c_reg_setting_array settings;
};

struct sensor_effect_info{
  struct sensor_effect_settings effect_settings[MAX_SENSOR_EFFECT];
  unsigned char size;
};

struct sensor_crop_parms_t {
  unsigned short top_crop;
  unsigned short bottom_crop;
  unsigned short left_crop;
  unsigned short right_crop;
};

typedef struct {
  sensor_output_format_t output_format;
  sensor_connection_mode_t connection_mode;
  sensor_raw_output_t raw_output;
  sensor_filter_arrangement filter_arrangement;
} sensor_output_t;

typedef struct {
  float min_gain;
  float max_gain;
  float max_analog_gain;
  unsigned int max_linecount;
} sensor_aec_data_t;

typedef struct {
  float pix_size;
  sensor_sensing_method_type_t sensing_method;
  float crop_factor; //depends on sensor physical dimentions
} sensor_property_t;

typedef struct {
  int width;
  int height;
} sensor_dimension_t;

typedef struct {
  sensor_dimension_t active_array_size;
  unsigned short left_dummy;
  unsigned short right_dummy;
  unsigned short top_dummy;
  unsigned short bottom_dummy;
} sensor_imaging_pixel_array_size;

typedef struct {
  unsigned short white_level;
  unsigned short r_pedestal;
  unsigned short gr_pedestal;
  unsigned short gb_pedestal;
  unsigned short b_pedestal;
} sensor_color_level_info;

/* sensor_lib_out_info_t: store information about different resolution
 * supported by sensor
 *
 * x_output: sensor output width (pixels)
 * y_output: sensor output height (pixels)
 * line_length_pclk: number of pixels per line
 * frame_length_lines: number of lines per frame
 * vt_pixel_clk: sensor scanning rate (cycles / sec)
 * op_pixel_clk: actual sensor output rate (cycles / sec)
 * binning_factor: 1 for no binning, 2 for V 1/2 H 1/2 binning and so on
 * binning_method: 0 for average, 1 for addition (summed)
 * min_fps: minumim fps that can be supported for this resolution
 * max_fps: maximum fps that can be supported for this sensor
 * mode: mode / modes for which this resolution can be used
 *       SENSOR_DEFAULT_MODE / SENSOR_HFR_MODE*/
struct sensor_lib_out_info_t {
  unsigned short x_output;
  unsigned short y_output;
  unsigned short line_length_pclk;
  unsigned short frame_length_lines;
  unsigned int vt_pixel_clk;
  unsigned int op_pixel_clk;
  unsigned short binning_factor;
  unsigned short binning_method;
  float    min_fps;
  float    max_fps;
  unsigned int mode;
  unsigned int offset_x;
  unsigned int offset_y;
  float    scale_factor;
  unsigned int is_pdaf_supported;
};

typedef struct {
  unsigned int full_size_width;
  unsigned int full_size_height;
  unsigned int full_size_left_crop;
  unsigned int full_size_top_crop;
} sensor_full_size_info_t;

typedef struct {
    unsigned char enable;
    sensor_full_size_info_t full_size_info;
} sensor_rolloff_config;

struct sensor_lib_out_info_array {
  /* sensor output for each resolutions */
  struct sensor_lib_out_info_t out_info[MAX_RESOLUTION_MODES];

  /* Number of valid entries in out_info array */
  unsigned short size;
};

struct camera_i2c_reg_setting {
  struct camera_i2c_reg_array *reg_array;
  unsigned short size;
  enum camera_i2c_reg_addr_type addr_type;
  enum camera_i2c_data_type data_type;
  unsigned short delay;
};

struct sensor_i2c_reg_setting_array {
  struct camera_i2c_reg_array reg_array[MAX_SENSOR_SETTING_I2C_REG];
  unsigned short size;
  enum camera_i2c_reg_addr_type addr_type;
  enum camera_i2c_data_type data_type;
  unsigned short delay;
};

struct sensor_lib_reg_settings_array {
  struct camera_i2c_reg_setting reg_settings[MAX_RESOLUTION_MODES];
  unsigned int                          size;
};

struct sensor_csi_params {
  unsigned char  lane_cnt;
  unsigned char  settle_cnt;
  unsigned short lane_mask;
  unsigned char  combo_mode;
  unsigned char  is_csi_3phase;
};

struct sensor_csid_vc_cfg {
  unsigned char cid;
  unsigned char dt;
  unsigned char decode_format;
};

struct sensor_csid_lut_params {
  unsigned char num_cid;
  struct sensor_csid_vc_cfg vc_cfg_a[MAX_CID];
};

struct sensor_csid_lut_params_array {
  struct sensor_csid_lut_params lut_params[MAX_RESOLUTION_MODES];
  unsigned short size;
};

struct sensor_csid_testmode_parms {
  unsigned int num_bytes_per_line;
  unsigned int num_lines;
  unsigned int h_blanking_count;
  unsigned int v_blanking_count;
  unsigned int payload_mode;
};

struct sensor_lib_crop_params_array{
  struct sensor_crop_parms_t crop_params[MAX_RESOLUTION_MODES];
  unsigned short size;
};

typedef struct {
    float real_gain;
    float exposure_time;
    unsigned int line_count;

    float exp_pclk_count;
    unsigned int reg_gain;
    float sensor_analog_gain;
    float sensor_digital_gain;
    qcarcam_exposure_mode_t exposure_mode_type;
} sensor_exposure_info_t;

typedef struct _sensor_stream_info_t {
  unsigned short vc_cfg_size;
  struct sensor_csid_vc_cfg vc_cfg[MAX_SENSOR_DATA_TYPE];
  sensor_output_format_t pix_data_fmt[MAX_SENSOR_DATA_TYPE];
} sensor_stream_info_t;

typedef struct _sensor_stream_info_array_t {
  sensor_stream_info_t sensor_stream_info[MAX_SENSOR_STREAM];
  unsigned short size;
} sensor_stream_info_array_t;

struct sensor_output_reg_addr_t {
  unsigned short x_output;
  unsigned short y_output;
  unsigned short line_length_pclk;
  unsigned short frame_length_lines;
};

struct sensor_exp_gain_info_t {
  unsigned short coarse_int_time_addr;
  unsigned short global_gain_addr;
  unsigned short vert_offset;
};

typedef enum sensor_stats_types {
  HDR_STATS,
  PD_STATS,
} sensor_stats_type_t;

struct sensor_meta_data_out_info_t {
  unsigned int width;
  unsigned int height;
  unsigned int stats_type;
  unsigned char  dt;
};

struct sensor_lib_meta_data_info_array {
  /* meta data output */
  struct sensor_meta_data_out_info_t meta_data_out_info[MAX_META_DATA_SIZE];

  /* Number of valid entries in meta data array */
  unsigned short size;
};

struct sensor_noise_coefficient_t {
  double gradient_S;
  double offset_S;
  double gradient_O;
  double offset_O;
};

struct camera_i2c_seq_reg_setting {
  struct camera_i2c_seq_reg_array *reg_setting;
  unsigned short size;
  enum camera_i2c_reg_addr_type addr_type;
  unsigned short delay;
};


typedef void (*sensor_intr_callback_t)(void* data);

/** Platform Function table
 *
 * function table for platform functions such as i2c operations
 *
 * return value
 *   0 for success
 *   negative value for failure **/
typedef struct {
    /* i2c read single reg */
    int (*i2c_read)(void* ctxt,
            struct camera_i2c_reg_setting *reg_setting);
    /* i2c write array */
    int (*i2c_write_array)(void* ctxt,
            struct camera_i2c_reg_setting *reg_setting);
    /* i2c read single reg from slave_addr */
    int (*i2c_slave_read)(void* ctxt,
            unsigned short slave_addr,
            struct camera_i2c_reg_setting *reg_setting);
    /* i2c write array to slave_addr */
    int (*i2c_slave_write_array)(void* ctxt,
            unsigned short slave_addr,
            struct camera_i2c_reg_setting *reg_setting);
    /* execute power sequence */
    int (*execute_power_setting)(void *ctxt,
            struct camera_power_setting *power_setting,
            unsigned short size);
    /* setup gpio interrupt */
    int (*setup_gpio_interrupt)(void *ctrl,
            enum camera_gpio_type gpio_id,
            sensor_intr_callback_t cb, void *data);
    /* input status callback */
    int (*input_status_cb)(void *ctrl,
            struct camera_input_status_t *status);
}sensor_platform_func_table_t;

/** I2C Function table
 * return value
 *   0 for success
 *   negative value for failure **/
typedef struct {
    /** Function to set i2c function table pointers */
    int (*sensor_set_platform_func_table)(void* ctxt, sensor_platform_func_table_t* table);

    /** Function to power on and off sensor */
    int (*sensor_power_on)(void* ctxt);
    int (*sensor_power_off)(void* ctxt);
    int (*sensor_power_suspend)(void* ctxt);
    int (*sensor_power_resume)(void* ctxt);

    /** Functions to custom detect device and its subdevices */
    int (*sensor_detect_device)(void* ctxt);
    int (*sensor_detect_device_channels)(void* ctxt);

    int (*sensor_init_setting)(void* ctxt);

    /** Function to set channel mode */
    int (*sensor_set_channel_mode)(void* ctxt, unsigned int src_id_mask, unsigned int mode);

    /** Function to start/stop channel streams */
    int (*sensor_start_stream)(void* ctxt, unsigned int src_id_mask);
    int (*sensor_stop_stream)(void* ctxt, unsigned int src_id_mask);

    int (*sensor_s_param)(void* ctxt, qcarcam_param_t id, unsigned int src_id, void* param);
    int (*sensor_g_param)(void* ctxt, qcarcam_param_t id, unsigned int src_id, void* param);

    int (*sensor_config_resolution)(void* ctxt, int32 width, int32 height);
    /** Function to get field info, output even_field and field_ts with ns unit */
    int (*sensor_query_field)(void* ctxt, boolean *even_field, uint64_t *field_ts);

} sensor_custom_func_table_t;

typedef struct {
  /** Function to calculate exposure based on real gain and linecount or exposure in ms
   * 1st param - real gain
   * 2nd param - linecount
   * 3rd param - exposure (ms)
   * 4th param - exposure info output, return status -
   * return value - 0 for success and negative value for
   * failure **/
  int (*sensor_calculate_exposure) (void* ctxt, unsigned int src_id, sensor_exposure_info_t *);

  /** Function to set exposure param on specific sensor */
  int (*sensor_exposure_config)(void* ctxt, unsigned int src_id, sensor_exposure_info_t *);

  int (*sensor_run_aec)(void* ctxt, void *, void *);
} sensor_exposure_table_t;

typedef struct {
  /** Function to create register table from awb settings
   * input param1 - r_gain
   * input param2 - b_gain
   * output param3 - register settings
   *
   * return value - 0 for success and negative value for
   * failure **/
  int (*sensor_fill_awb_array)(float, float,
          struct camera_i2c_reg_setting *);
  /* sensor awb table size */
  unsigned short awb_table_size;
} sensor_awb_table_t;

typedef enum {
  SENSOR_STATS_CUSTOM = 0,
  SENSOR_STATS_RAW10_8B_CONF_10B_PD, /*8 bits confidence, 10 bits PD*/
} sensor_stats_format_t;

typedef struct {
  int (*parse_VHDR_stats)(unsigned int *, void *);
  int (*parse_PDAF_stats)(unsigned int *, void *);
  sensor_stats_format_t pd_data_format;
} sensor_RDI_parser_stats_t;

int sensor_calculate_exposure(void* ctxt, float real_gain,
  unsigned int line_count, sensor_exposure_info_t *exp_info, float s_real_gain);

int sensor_fill_exposure_array(unsigned int gain,
   unsigned int digital_gain, unsigned int line,
  unsigned int fl_lines, int luma_avg, unsigned int hdr_param,
  struct camera_i2c_reg_setting* reg_setting,
  unsigned int s_reg_gain, int s_linecount, int is_hdr_enabled);

int sensor_fill_awb_array(float awb_gain_r,
  float awb_gain_b,  struct camera_i2c_seq_reg_setting* reg_setting);

int parse_VHDR_stats(unsigned int *destLumaBuff,
  void *rawBuff);

int parse_PDAF_stats(unsigned int *destLumaBuff,
  void *rawBuff);

typedef struct {
  /* wrapper to call external lib*/
  char calcdefocus[NAME_SIZE_MAX];
  /* external lib and API */
  char libname[NAME_SIZE_MAX];
  char pdaf_deinit_API[NAME_SIZE_MAX];
  char pdaf_init_API[NAME_SIZE_MAX];
  char pdaf_get_defocus_API[NAME_SIZE_MAX];
  char pdaf_get_version_API[NAME_SIZE_MAX];
} sensorlib_pdaf_apis_t;


/* Automotive Extension API to support multi-sensor bridge chips */
#define MAX_IMAGE_SOURCES MAX_CID

typedef struct {
    unsigned int vc;
    unsigned int dt;
    unsigned int cid;
}img_src_channel_info_t;

typedef struct {
    qcarcam_color_fmt_t fmt;
    qcarcam_res_t res;
    img_src_channel_info_t channel_info;
    boolean interlaced;
}img_src_mode_t;

typedef struct {
    unsigned int src_id;
    img_src_mode_t modes[MAX_RESOLUTION_MODES];
    unsigned int num_modes;
}img_src_subchannel_t;


/* layout description */
typedef enum {
    SENSOR_INTERLEAVE_FRAME = 0,
    SENSOR_INTERLEAVE_LINE,
    SENSOR_INTERLEAVE_MAX
}img_src_interleave_t;

typedef struct {
    unsigned int src_id;
    unsigned int x_offset;
    unsigned int y_offset;
    unsigned int stride;
    img_src_interleave_t interleave;
}img_src_subchannel_layout_t;

typedef struct {
    img_src_mode_t output_mode;
    img_src_subchannel_layout_t subchan_layout[MAX_IMAGE_SOURCES];
    unsigned int num_subchannels;
}img_src_channel_t;

typedef enum {
    SENSOR_CAPABILITY_EXPOSURE_CONFIG = 0,
    SENSOR_CAPABILITY_COLOR_CONFIG,
    SENSOR_CAPABILITY_MAX = 0xFFFFFFFF,
}sensor_capability_t;

typedef struct
{
    /* private context - must be first element */
    void* priv_ctxt;

    /* close lib function ptr */
    int (*sensor_close_lib)(void* ctxt);

    /* channels */
    img_src_channel_t channels[MAX_IMAGE_SOURCES];
    unsigned int num_channels;

    /* subchannels */
    img_src_subchannel_t subchannels[MAX_IMAGE_SOURCES];
    unsigned int num_subchannels;

    /* sensor slave info */
    struct camera_sensor_slave_info sensor_slave_info;

    /* sensor output settings */
    sensor_output_t sensor_output;

    /* sensor output register address */
    struct sensor_output_reg_addr_t output_reg_addr;

    /* sensor exposure gain register address */
    struct sensor_exp_gain_info_t exp_gain_info;

    /* sensor aec info */
    sensor_aec_data_t aec_info;

    /* number of frames to skip after start stream info */
    unsigned short sensor_num_frame_skip;

    /* number of frames to skip after start HDR stream info */
    unsigned short sensor_num_HDR_frame_skip;

    /* sensor pipeline delay */
    unsigned int sensor_max_pipeline_frame_delay;

    /* sensor lens info */
    sensor_property_t sensor_property;

    /* imaging pixel array size info */
    sensor_imaging_pixel_array_size pixel_array_size_info;

    /* Sensor color level information */
    sensor_color_level_info color_level_info;

    /* sensor port info that consists of cid mask and fourcc mapaping */
    sensor_stream_info_array_t sensor_stream_info_array;

    /* Sensor Settings */
    struct camera_i2c_reg_setting start_settings;
    struct camera_i2c_reg_setting stop_settings;
    struct camera_i2c_reg_setting groupon_settings;
    struct camera_i2c_reg_setting groupoff_settings;
    struct camera_i2c_reg_setting embedded_data_enable_settings;
    struct camera_i2c_reg_setting embedded_data_disable_settings;
    struct camera_i2c_reg_setting aec_enable_settings;
    struct camera_i2c_reg_setting aec_disable_settings;
    /* sensor test pattern info */
    sensor_test_info test_pattern_info;
    /* sensor effects info */
    struct sensor_effect_info effect_info;

    /* Sensor Settings Array */
    struct sensor_lib_reg_settings_array init_settings_array;
    struct sensor_lib_reg_settings_array res_settings_array;

    struct sensor_lib_out_info_array     out_info_array;
    struct sensor_csi_params             csi_params;
    struct sensor_csid_lut_params_array  csid_lut_params_array;
    struct sensor_lib_crop_params_array  crop_params_array;

    /* Exposure Info */
    sensor_exposure_table_t exposure_func_table;

    /* video_hdr mode info*/
    struct sensor_lib_meta_data_info_array meta_data_out_info_array;

    /* sensor_capability */
    /* bit mask setting for sensor capability */
    /* bit positions are provided in sensor_capability_t */
    unsigned int sensor_capability;

    /* sensor_awb_table_t */
    sensor_awb_table_t awb_func_table;

    /* Parse RDI stats callback function */
    sensor_RDI_parser_stats_t parse_RDI_stats;

    /* full size info */
    sensor_rolloff_config rolloff_config;

    /* analog-digital conversion time */
    long long adc_readout_time;

    /* number of frames to skip for fast AEC use case */
    unsigned short sensor_num_fast_aec_frame_skip;

    /* add soft delay for sensor settings like exposure, gain ...*/
    unsigned char app_delay[SENSOR_DELAY_MAX];

    /* for noise profile calculation
     Tuning team must update with proper values. */
    struct sensor_noise_coefficient_t noise_coeff;

    /* Flag to be set if any external library are to be loaded */
    unsigned char external_library;

    /* custom func table ptr */
    sensor_custom_func_table_t sensor_custom_func;
    boolean use_sensor_custom_func;
    uint32 src_id_enable_mask;

} sensor_lib_t;

typedef struct
{
    enum sensor_camera_id camera_id;
    unsigned int subdev_id;
} sensor_open_lib_t;

// Function to get the sensor lib pointer
void* sensor_open_lib(void* ctrl, void* arg);

#endif /* __SENSOR_LIB_H_ */
