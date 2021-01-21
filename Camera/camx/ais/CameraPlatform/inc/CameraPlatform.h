#ifndef __CAMERAPLATFORM_H_
#define __CAMERAPLATFORM_H_

/**
 * @file CameraPlatform.h
 *
 * @brief Header file for CameraPlatform interface.
 *
 * Copyright (c) 2007-2019 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

/* ===========================================================================
                        INCLUDE FILES
=========================================================================== */
#include "CameraResult.h"
#include "CameraOSServices.h"
#include "sensor_sdk_common.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

/* ===========================================================================
                        DATA DECLARATIONS
=========================================================================== */
/* ---------------------------------------------------------------------------
** Constant / Define Declarations
** ------------------------------------------------------------------------ */
#define CAMERAPLATFORM_MAX_HWBLOCKS 32

/* ---------------------------------------------------------------------------
** Type Declarations
** ------------------------------------------------------------------------ */
typedef enum sensor_camera_id CameraSensorIndex;

typedef enum
{
    CAMSENSOR_I2C_MIN       = 100,
    CAMSENSOR_I2C_100KHZ    = CAMSENSOR_I2C_MIN,
    CAMSENSOR_I2C_400KHZ    = 400,
    CAMSENSOR_I2C_1000KHZ   = 1000,
    CAMSENSOR_I2C_3400KHZ   = 3400,
    CAMSENSOR_I2C_MAX,
} CameraSensorI2C_SpeedType;


typedef enum camera_gpio_type CameraSensorGPIO_SignalType;
typedef enum camera_gpio_trigger_type CameraSensorGPIO_EventType;
typedef enum camera_gpio_config_type CameraSensorGPIO_ValueType;
typedef enum camera_gpio_intr_type CameraSensorGPIO_IntrCfgType;
typedef struct camera_gpio_intr_cfg_t CameraSensorGPIO_IntrPinType;

/**
 * This enumerates the possible I2C options. These are used in the options
 * word in the I/O command structure.
 */
typedef enum
{
    /// Use default address for I/O
    CAMSENSOR_I2C_SIMPLE_DEV = 0x0001,
    /// memory device (16 bit addresses)
    CAMSENSOR_I2C_MEM_DEV = 0x0002,
    /// 8-bit register based device
    CAMSENSOR_I2C_REG_DEV = 0x0004,
    /// NACK last byte read from slave
    CAMSENSOR_I2C_NACK_LAST_BYTE = 0x0008,
    /// Let slave release SDA after last byte is read from it.
    CAMSENSOR_I2C_NOP_LAST_BYTE = 0x0010,
    /// During a read transaction, gen. a repeated START after writing the slave addr(just before read)
    CAMSENSOR_I2C_START_BEFORE_READ = 0x0020,
    /// During a read transaction, gen. a STOP & START after writing the slave addr (just before read)
    CAMSENSOR_I2C_STOP_START_BEFORE_READ = 0x0040,
    /// Generate a clock and a START before every I/O operation.
    CAMSENSOR_I2C_CLK_START_BEFORE_RW = 0x0080,
    /// No stop condition after writing the address
    CAMSENSOR_I2C_NO_STOP_AFTER_WRITE = 0x0100,
    /// Immediatly read the data
    CAMSENSOR_I2C_IMMEDIATE_READ = 0x0200,
    /// Delay periods for for HW CTRL
    CAMSENSOR_I2C_USE_DELAY_PERIODS   = 0x0400,
    /// Write Multiple I2C Sequentially
    CAMSENSOR_I2C_SEQ_WR = 0x0800,
    /// Collapses singles i2c to burst mode if possible
    CAMSENSOR_I2C_COLLAPSIBLE_SEQ_WR = 0x1000,
    /// Overrides existing burst mode setting and enable burst mode
    CAMSENSOR_I2C_FORCED_BURST_MODE = 0x2000,
} CameraSensorI2COptionsType;

typedef enum
{
    CAMSENSOR_I2C_MASTER_MIN,
    CAMSENSOR_I2C_MASTER_0 = CAMSENSOR_I2C_MASTER_MIN,
    CAMSENSOR_I2C_MASTER_1,
    CAMSENSOR_I2C_MASTER_MAX,
} CameraSensorI2CMasterType;

/**
 * This struct defines a read/write command type for I2C slave I/O operation.
 */
typedef enum
{
    CAMSENSOR_I2C_CMD_TYPE_WRITE         = 0x0001, /* write command access type */
    CAMSENSOR_I2C_CMD_TYPE_READ          = 0x0002  /* read command access type */
} CameraSensorI2CCmdAccessType;

typedef enum
{
    CAMSENSOR_EXP_CALLBACK = 1,
    CAMSENSOR_FCS_CALLBACK,
    CAMSENSOR_FLSH_CALLBACK,
    CAMSENSOR_ASYNC_I2C_CALLBACK,
    CAMSENSOR_GPIO_IN_CALLBACK,
    CAMSENSOR_MX_CALLBACK
} CameraSensorCallbackType;

typedef enum
{
    CHIP_ID_INVALID = 0,
    CHIP_ID_SA8155 = 362,
    CHIP_ID_SA8155P = 367,
    CHIP_ID_SA6155 = 377,
    CHIP_ID_SA8195P = 405
} CameraPlatformChipIdType;

typedef enum
{
    HARDWARE_PLATFORM_INVALID = 0,
    HARDWARE_PLATFORM_ADP_STAR,
    HARDWARE_PLATFORM_ADP_AIR,
    HARDWARE_PLATFORM_ADP_ALCOR,
    HARDWARE_PLATFORM_TDD
} CameraPlatformIdType;

/// @brief Camera Titan family Information
typedef enum
{
    CameraTitan175 = 0x00010705,
    CameraTitan170 = 0x00010700,
    CameraTitan160 = 0x00010600,
    CameraTitan150 = 0x00010500
} CameraPlatformTitanVersion;

typedef uint32 CameraPlatformTitanHWVersion;

typedef enum
{
    CCI_GPIO_OUT_MIN = 0,
    CCI_GPIO_OUT_0 = CCI_GPIO_OUT_MIN,
    CCI_GPIO_OUT_1,
    CCI_GPIO_OUT_2,
    CCI_GPIO_OUT_3,
    CCI_GPIO_OUT_4,
    CCI_GPIO_OUT_MAX
} CameraControlGPIOOutputIDType;

typedef enum
{
    CCI_GPIO_IN_MIN = 0,
    CCI_GPIO_IN_0 = CCI_GPIO_IN_MIN,
    CCI_GPIO_IN_1,
    CCI_GPIO_IN_2,
    CCI_GPIO_IN_MAX
} CameraControlGPIOInputIDType;

typedef enum
{
    GPIO_TRIG_MIN = 0,
    GPIO_TRIG_POS_EDG = GPIO_TRIG_MIN,
    GPIO_TRIG_NEG_EDG,
    GPIO_TRIG_VAL_1,
    GPIO_TRIG_VAL_0,
    GPIO_TRIG_MAX,
} CameraControlGPIOInputTrigType;
typedef enum
{
    TRIG_MIN = 0,
    TRIG_GPIO_BASED = TRIG_MIN,
    TRIG_CSI_BASED,
    TRIG_MAX,
} CameraControlTriggerType;
/***
* This struct defines the orienation of both front and back sensor in terms of mirror and flip
***/
typedef struct
{
   uint32 cameraSensorFlip;
   uint32 cameraSensorMirror;
}CameraPlatformSensorOrientationType;


/**
 * CCI I2C support three types of setting for operating clocks:
 * 100KHz, 400KHz, 1000Khz. clients can modify those settings for customized clocks
 */
typedef enum _cam_i2c_clk_mode
{
    CAM_I2C_CLK_MODE_STANDARD = 0,  /**< default clock: 100KHZ */
    CAM_I2C_CLK_MODE_FAST,          /**< default clock: 400KHZ */
    CAM_I2C_CLK_MODE_FAST_PLUS,     /**< default clock: 1000KHZ */

    CAM_I2C_CLK_MODE_MAX_NUM
} cam_i2c_clk_mode;



/**
 * data type for i2c data/address, which indicates the length of data/address
 */
typedef enum _cam_i2c_data_type
{
    CAM_I2C_DATA_TYPE_1BYTE = 1,
    CAM_I2C_DATA_TYPE_2BYTE = 2,
    CAM_I2C_DATA_TYPE_3BYTE = 3,
    CAM_I2C_DATA_TYPE_4BYTE = 4,
} cam_i2c_data_type;

/**
 * storage type for i2c data
 */
typedef enum _cam_i2c_storage_type
{
    CAM_I2C_STORAGE_TYPE_BYTE = 1, /**< uint8 or equivalent */
    CAM_I2C_STORAGE_TYPE_WORD = 2, /**< uint16 or equivalent */
    CAM_I2C_STORAGE_TYPE_DWORD = 4 /**< uint32 or equivalent */
} cam_i2c_storage_type;


/**
 * i2c data's endianness
 */
typedef enum _cam_i2c_endian
{
    CAM_I2C_ENDIAN_LITTLE = 0,
    CAM_I2C_ENDIAN_BIG = 1
} cam_i2c_endian;


/**
 * compose or decompose data type, storage type and endian
 * data_type: means how long the data is.
 *            1: 1 byte, 2: 2 bytes, 3: 3 bytes, 4: 4 bytes
 * stor_type: means how long the storage for data is.
              1: uint8, 2: uint16, 4: uint32
 * endian: 0: little endian: 1: big endian
 * data_type may not be greater than stor_type
 */
#define CAM_I2C_CMD_PARAM_OPTION_MAKE(data_type, stor_type, endian) \
    ((((endian) & 0x1) << 8) | (((stor_type) & 0xF) << 4) | ((data_type) & 0xF))

#define CAM_I2C_CMD_PARAM_OPTION_DATA_TYPE(x) ((x) & 0xF)
#define CAM_I2C_CMD_PARAM_OPTION_STOR_TYPE(x) (((x) >> 4)& 0xF)
#define CAM_I2C_CMD_PARAM_OPTION_ENDIAN(x) (((x) >> 8) & 0x1)



/**
 * I2C client information, which are used for I2C bus
 */
typedef struct _cam_i2c_client
{
    cam_i2c_clk_mode clk_mode;  /**< operating clock speed */
    uint8 slave_id;             /**< i2c slave device address */

    uint8 retry_cnt;            /**< default value: 0, may not be more than 3 */
    uint8 id_map;               /**< default value: 0 */

    uint8 master_id;            /**< 0 or 1 */
    uint8 queue_id;             /**< 0 or 1,  >1 means it is selected by driver */

} cam_i2c_client;


/**
 * I2C buffer for read/write operations of scattered registers
 */
typedef struct _cam_i2c_multi
{
    uint32 addr_option; /**< indicates address info, including address type,
                             address storage type, and endian */
    uint32 data_option; /**< indicates data info, including data type,
                                data storage type, and endian */

    struct camera_i2c_reg_array *p_buf; /**< array of multiple registers */
    uint16 size;                        /**< size of array */

} cam_i2c_multi;

/**
 * I2C buffer for read/write operations of consecutive registers
 */
typedef struct _cam_i2c_seq
{
    uint32 addr_option; /**< indicates address info, including address type,
                             address storage type, and endian */
    uint32 data_option; /**< indicates data info, including data type,
                                data storage type, and endian */

    uint32 addr;  /**< starting address */
    void *p_data; /**< data buffer, whose storage type is indicated in data_option */
    uint16 size;  /**< size of original data type pointed by p_data */
} cam_i2c_seq;

/**
 * I2C buffer for read/write operations of registers
 */
typedef union _cam_i2c_buf
{
    cam_i2c_multi multi;
    cam_i2c_seq seq;
} cam_i2c_buf;


/**
 * I2C command for I2C slave I/O operation.
 */
typedef struct
{
    cam_i2c_client client; /**< I2C client information */

    uint8       op_type;   /**< defines operation type, 0: multi, 1: seq */
    cam_i2c_buf buf;       /**< I2C operation buffer */

} cam_i2c_cmd;

/**
 * This enumerates the camera hw blocks
 */
typedef enum
{
    CAMERA_HWBLOCK_INVALID = 0,
    CAMERA_HWBLOCK_TOP,
    CAMERA_HWBLOCK_CPAS,
    CAMERA_HWBLOCK_IFE,
    CAMERA_HWBLOCK_IFE_CSID,
    CAMERA_HWBLOCK_CSIPHY,
    CAMERA_HWBLOCK_CCI,
    CAMERA_HWBLOCK_CAMNOC,
    CAMERA_HWBLOCK_CDM,
    CAMERA_HWBLOCK_ICP,
    CAMERA_HWBLOCK_BPS,
    CAMERA_HWBLOCK_IPE,
    CAMERA_HWBLOCK_MAX
} CameraHwBlockType;

typedef struct
{
    char     name[32];
    CameraHwBlockType hwblock;
    uint8    id;
    uint32   offset;
    uint32   size;
    uint32   irq;
} CameraHWBlockDefinitionType;

typedef struct
{
    char     name[32];
    uint32   id;
    uint32   version;
    uint32   offset;
    uint32   size;
    const CameraHWBlockDefinitionType* hwblocks;
} CameraHWDefinitionType;

typedef struct
{
    void*    PhysicalAddress;
    void*    VirtualAddress;
    int      Size;
} CameraBuffer;

#define CAMERA_BUFFER_FLAG_CACHED           0x00000001
#define CAMERA_BUFFER_FLAG_SECURE           0x00000002
#define CAMERA_BUFFER_FLAG_FIXEDMAP         0x00000004
#define CAMERA_BUFFER_FLAG_CONTIGOUS        0x00000008
#define CAMERA_BUFFER_FLAG_READ             0x00000010
#define CAMERA_BUFFER_FLAG_WRITE            0x00000020
#define CAMERA_BUFFER_FLAG_PRIV_READ_WRITE  0x00000040

#define CAMERA_BUFFER_FLAG_V2               0x10000000 /*using pmem handle*/

//
// This structure should be identical to CameraChannelInfoType in CameraConfig.h.
// It describes a channel configuration and identifies with a unique <devId, srcId>.
//
typedef struct
{
    /**
     * unique inputId that maps to <devId, srcId>
     */
    uint32 aisInputId;

    /**
     * unique <devId, srcId> of input id
     */
    uint32 devId;
    uint32 srcId;
} CameraChannelDataType;

/* ===========================================================================
                        EXTERNAL API DECLARATIONS
=========================================================================== */
/**
 * This function initializes the camera platform and should be called before all
 * other functions.
 */
boolean CameraPlatformInit(void);

/**
 * This function deinits the camera platform. No other functions than
 * CameraPlatformInit should be called after this call.
 */
void CameraPlatformDeinit(void);

/**
 * This function is used to enable camera clocks
 *
 * @return CAMERA_SUCCESS - if successful
 */
CameraResult CameraPlatformClockEnable(void);

/**
 * This function is used to disable camera clocks
 *
 * @return CAMERA_SUCCESS - if successful
 */
CameraResult CameraPlatformClockDisable(void);

/**
 * This function is used to enable camera clocks
 *
 * @return CAMERA_SUCCESS - if successful
 */
CameraResult CameraPlatformClockEnableCCI(void);

/**
 * This function is used to disable camera clocks
 *
 * @return CAMERA_SUCCESS - if successful
 */
CameraResult CameraPlatformClockDisableCCI(void);

/**
 * This function registers the camera with the sleep module.
 */
void CameraRegisterWithSleep(void);

/**
 * This function de-registers the camera with the sleep module.
 */
void CameraDeregisterWithSleep(void);

/**
 * This function indicates to the sleep module that it is ok to sleep.
 */
void CameraPermitSleep(void);

/**
 * This function indicates to the sleep module that it is not ok to sleep.
 */
void CameraProhibitSleep(void);

/**
 * This function initializates CameraPMEM API and should be called
 * before any other CameraPMEM API call is made.
 *
 * @return TRUE - if successful
 *         FALSE - if failed
 */
boolean CameraPMEMInit(void);

/**
 * This function allocates a block of size bytes from the heap that matches
 * the requested flags/attributes.
 *
 * @param[in] numBytes The size of the buffer to be allocated in bytes
 *
 * @return A pointer to the newly allocated block, or NULL if the block
 *         could not be allocated. The pointer is 1k aligned.
 */
void* CameraPMEMMalloc(uint32 numBytes);

/**
 * This function deallocates a block of memory and returns it to the
 * correct heap.
 *
 * @param[in] buffer The pointer to the memory to be freed
 *
 * @return None
 */
void CameraPMEMFree(void* buffer);

/**
 * This function allocates physical memory with size rounded up to the nearest
 *alignment specified.The allocated physical memory can be contigous or non-congituous
 *depending on the flags passed.
 *
 * @param [in] numBytes  Size of memory chunk to be allocated, Will be rounded up to 4K size
 * @param [in] flags flags on the type of allocation
 * @param [in] vmid_mask vmid_mask of the GVM(s) for which this allocation is being made
 * @param [out] mem_handle  The output opaque pmem handle associated with this allocation
 * @param [rsv] rsv Reserved for future expansion.
 *
 * @return A pointer to the newly allocated block, or NULL if the block
 *         could not be allocated.
 */
void* CameraPMEMMallocExt(uint32 numBytes, uint32 flags, uint32 vmidMask,
                              void** pMemHandle, void* rsv);

/**
 * This function deallocates a block of memory and returns it to the
 * reserved heap.
 *
 * @param[in] buffer The pointer to the memory to be freed
 *
 * @return None
 */
void CameraPMEMFreeExt(void* buffer);

/**
 * This function translates the virtual address of the PMEM-allocated buffer
 * into a physical address.
 *
 * @param[in] buffer The virtual address to be translated
 *
 * @return The physical address corresponding to the virtual address
 *         passed in.
 */
void* CameraPMEMGetPhysicalAddress(void* buffer);

/**
 * This function deinitializes CameraPMEM API and should only be called
 * after all CameraPMEM have been freed.
 *
 * @return TRUE - if successful
 *         FALSE - if failed
 */
boolean CameraPMEMDeinit(void);

/**
 * This function initializes SMMU objects
 *
 * @param[in] None
 *
 * @return 0 if successful
 */
CameraResult CameraSMMUInit(void);

/**
 * This function closes the SMMU handles
 *
 * @param[in] None
 *
 * @return None
 */
void CameraSMMUDeinit(void);

/**
* This function maps the output buffer
* into the camera SMMU context.
*
* @param[in] pMemHandle The buffer to be mapped
* @param[in] numBytes   The size of the buffer to be allocated in bytes
* @param[in] devAddr    Dev address if fixed mapping flag is set
* @param[in] flags      Bit mask of flags for allocation (cont, cached, secure, read/write, etc...)
* @param[in] pHwBlocks  List of hw blocks to map to
* @param[in] nHwBlocks  Number of hw blocks in pHwBlocks

* @return CameraResult
*/
CameraResult CameraSMMUMapCameraBuffer(void* pMemHandle,
       uint32 numBytes,
       uint64_t devAddr,
       uint32 flags,
       const CameraHwBlockType* pHwBlocks,
       uint32 nHwBlocks);

/**
* This function unmaps the output buffer
* into the camera SMMU context.
*
* @param[in] out_buf The buffer to be unmapped
*/
void CameraSMMUUnmapCameraBuffer(void* pMemHandle);

/**
* This function looks up the device address that is mapped to a specific HW block
*
* @param[in] pMemHandle The memory handle that was mapped
* @param[in] hwBlock The HW block for which the DA is needed
*
* @return The device address corresponding to the virtual address
*         passed in.
*/
uint64_t CameraSMMUGetDeviceAddress(void* pMemHandle, CameraHwBlockType hwBlock);

/**
 * This function is used to initialize camera sensor clock
 * block.
 *
 *
 * @return TRUE - if successful
 *         FALSE - if failed
 */

boolean CameraSensorCLK_Init(void);

/**
 * This function is used to deinitialize camera sensor clock
 * block.
 *
 *
 * @return TRUE - if successful
 *         FALSE - if failed
 */

boolean CameraSensorCLK_Deinit(void);

/**
 * This function is used to configure the required master clock for
 * the camera sensor.
 *
 * @param[in] idx The camera index.
 * @param[in] nCameraSensorMCLKHz The desired clock frequency in Hz.
 *
 * @return TRUE - if successful
 *         FALSE - if failed
 */
boolean CameraSensorCLKConfigMclk(CameraSensorIndex idx, uint32 nCameraSensorMCLKHz);

/**
 * This function is used to unconfigure the master clock.
 *
 * @param[in] idx The camera index.
 *
 * @return TRUE - if successful
 *         FALSE - if failed
 */
boolean CameraSensorCLKUnConfigMclk(CameraSensorIndex idx);

/**
 * This function is used to wait for the specified number of microseconds.
 *
 * @param[in] nMicroSecToWait The number of microseconds to wait.
 */
CAM_API void CameraPlatform_WaitMicroSeconds(uint32 nMicroSecToWait);

/**
 * This function is used to Get DDR Frequency for specified sensor Index
 *
 * @param[in] idx The camera index , pDDRFrequency.
 *
 * @return TRUE - if successful
 *         FALSE - if failed
 */
boolean CameraSensor_GetDDRFrequency(const CameraSensorIndex sensorIndex, uint32* pDDRFrequency);

/**
 * This function is used to initialize GPIO Block.
 *
 * @return TRUE - if successful
 *         FALSE - if failed
 */
boolean CameraSensorGPIO_Init(void);

/**
 * This function is used to deinitialize GPIO Block.
 *
 * @return TRUE - if successful
 *         FALSE - if failed
 */
boolean CameraSensorGPIO_DeInit(void);


/**
 * This function is used to configure GPIO pins for camera.
 *
 * @param[in] idx The camera index.
 *
 * @return TRUE - if successful
 *         FALSE - if failed
 */
boolean CameraSensorGPIOConfigCamera(CameraSensorIndex idx);

boolean CameraSensorGPIO_ConfigCameraCCI(void);

boolean CameraSensorGPIO_UnConfigCameraCCI(void);

/**
 * This function is used to unconfigure GPIO pins for MIPI camera.
 *
 * @param[in] idx The camera index.
 *
 * @return TRUE - if successful
 *         FALSE - if failed
 */
boolean CameraSensorGPIOUnConfigCamera(CameraSensorIndex idx);

/**
 * This function is used to set the logic level of a GPIO pin.
 *
 * @param[in] GPIOSignal The GPIO pin to set
 * @param[out] GPIOValue logic level to set
 *
 * @return TRUE - if successful
 *         FALSE - if failed
 */
boolean CameraSensorGPIO_Out(
        CameraSensorIndex idx,
        CameraSensorGPIO_SignalType GPIOSignal,
        CameraSensorGPIO_ValueType GPIOValue);

/**
 * This function is used to read the logic level of a GPIO pin.
 *
 * @param[in] GPIOSignal The GPIO pin to read
 * @param[out] GPIOValue logic level to read
 *
 * @return TRUE - if successful
 *         FALSE - if failed
 */
CAM_API boolean CameraSensorGPIO_Read(
        CameraSensorIndex idx,
        CameraSensorGPIO_SignalType GPIOSignal,
        CameraSensorGPIO_ValueType *GPIOValue);

/**
 * This function obtains pins info from camera config.
 *
 * @return TRUE if operation was successful; FALSE otherwise.
 */
boolean CameraSensorGPIO_GetIntrPinInfo(CameraSensorIndex idx,
        CameraSensorGPIO_SignalType pin,
        CameraSensorGPIO_IntrPinType *pPinInfo);

/**
 * This function set the given gpio pin cfg with mask.
 *
 * @return TRUE if operation was successful; FALSE otherwise.
 */
boolean CameraSensorGPIO_SetConfig(uint32 pin, uint32 mask, uint32 cfg);

/**
 * This function set the given GPIO pin to low/high.
 *
 * @return TRUE if operation was successful; FALSE otherwise.
 */
boolean CameraSensorGPIO_SetPin(uint32 pin, CameraSensorGPIO_ValueType pinVal);

/**
 * This function get the given GPIO pin state
 *
 * @return TRUE if operation was successful; FALSE otherwise.
 */
boolean CameraSensorGPIO_GetPin(uint32 pin, CameraSensorGPIO_ValueType* pPinVal);


/**
 * This function is used to intialize I2C communication.
 *
 * @return TRUE - if successful
 *         FALSE - if failed
 */
boolean CameraSensorI2C_Init(void);

/**
 * This function is used to de-intialize I2C communication.
 *
 * @return TRUE - if successful
 *         FALSE - if failed
 */
boolean CameraSensorI2C_DeInit(void);

/**
 * This function is used to power up I2C communication.
 *
 * @return TRUE - if successful
 *         FALSE - if failed
 */
boolean CameraSensorI2C_PowerUp(void);

/**
 * This function is used to power down I2C communication.
 *
 * @return TRUE - if successful
 *         FALSE - if failed
 */
boolean CameraSensorI2C_PowerDown(void);

/**
 * This function is used to set the I2C clock frequency of the bus.
 *
 * @return void
 *
 */
void CameraSensorI2C_SetClockFrequency(CameraSensorI2C_SpeedType speed);

/**
 * This function is used to write multiple data packets over the I2C bus.
 *
 * @param[in] cmd A pointer to the CameraSensorI2CCmdType data structure
 * @param[in] size The size of CameraSensorI2CCmdType data structure
 *
 * @return TRUE - if successful
 *         FALSE - if failed
 */
CAM_API boolean CameraSensorI2C_Write_MultiCommands(void* cmd, uint16 size);

/**
 * This function is used to write data over the I2C bus.
 *
 * @param[in] cmd A pointer to the CameraSensorI2CCmdType data structure
 *
 * @return TRUE - if successful
 *         FALSE - if failed
 */
CAM_API boolean CameraSensorI2C_Write(void* cmd);

/**
 * This function is used to read data from the I2C bus.
 *
 * @param[out] cmd A pointer to the CameraSensorI2CCmdType data structure
 *
 * @return TRUE - if successful
 *         FALSE - if failed
 */
CAM_API boolean CameraSensorI2C_Read(void* cmd);

/**
 * This function is used to get the i2c master associated with
 * the specified sensor..
 *
 * @param[out] CameraSensorIndex front or back sensor
 *
 * @return uint32 masterIdx for specified sensor
 *  returns default of 0 if sensor is not valid
 *  or boards do not specify a i2c master index
 *  associated with the sensor.
 */
CAM_API uint32 CameraSensorGetI2cMaster(CameraSensorIndex idx);

/**
 * This function is used to identify the chip the software is running on.
 * @return void
 */
CAM_API CameraPlatformChipIdType CameraPlatform_GetChipId(void);

/**
 * This function is used to identify the chip version the software is running on.
 * @return void
 */
uint32 CameraPlatform_GetChipVersion(void);

/**
 * This function is used to identify the chip the software is running on.
 * @return void
 */
CAM_API CameraPlatformIdType CameraPlatform_GetPlatformId(void);

/**
 * This function is used to identify the I2C bus the sensors are configured on.
 *param[in] UINT32 value that chooses back or front camera
 *
 * @return CameraPlatformSensorOrientationType *
 */
 CameraPlatformSensorOrientationType* CameraPlatform_GetOrientation(CameraSensorIndex idx);

 /**
 * This function is used to identify the I2C bus Master Index sensors are configured on.
 *param[in] UINT32 value that chooses back or front camera
 *
 * @return uint16 *
 */
 CameraSensorI2CMasterType CameraPlatform_GetMasterIndex(CameraSensorIndex idx);

/**
 * This returns the virtual base address of requested hw block registers.
 *
 * @return The virtual base address of hw block Registers.
 *         This function will return NULL if hw block is not supported.
 *         The caller should check for NULL return value.
 */
void* CameraPlatformGetVirtualAddress(CameraHwBlockType hwBlockType, uint8 id);

/**
 * This method retrieve a copy of the channel config info from
 * camera config.  Caller shall allocate necessary memory in
 * pChannelData and indicates its size as number of elements in
 * pNumChannels.  Upon successful return, pNumChannels will be
 * updated to reflect the actual number of channel data copied
 * into pChannelData.
 *
 * @return 0 if successful
 *         non-0 otherwise
 */
int CameraPlatformGetChannelData(CameraChannelDataType * const pChannelData,
                                 int * const pNumChannels);


typedef struct
{
    uint32 csid;
    uint32 csiphy;
}CameraInputDeviceInterfaceType;

/**
 * This method retrieve a input device physical interface information
 * such as csi core, number of lanes, etc...
 * @return CAMERA_SUCCESS if successful
 */
CameraResult CameraPlatformGetInputDeviceInterface(uint32 CameraDeviceId,
        CameraInputDeviceInterfaceType* pInterf);

/**
 * This method retrieve CameraSensorIndex from CameraDeviceId
 * @return CAMERA_SUCCESS if successful
 */
CAM_API CameraResult CameraPlatformGetInputDeviceIndex(uint32 CameraDeviceId, CameraSensorIndex* pIndex, uint32* pSubdevIndex);

/**
 * This function reads values from VFE0 and VFE1 registers and prints
 * them into a XML file.
 */
void CameraPrintVFERegisters(void);

/**
 * This function reads values from CSI PHY registers and prints
 * them into a XML file.
 */
void CameraPrintCSIPHYRegisters(void);

/**
 * This function reads values from CSID registers and prints
 * them into a XML file.
 */
void CameraPrintCSIDRegisters(void);

/**
 * This function reads values from ISPIF registers and prints
 * them into a XML file.
 */
void CameraPrintISPIFRegisters(void);


#ifdef __cplusplus
} // extern "C"
#endif // __cplusplus

#endif // __CAMERAPLATFORM_H_
