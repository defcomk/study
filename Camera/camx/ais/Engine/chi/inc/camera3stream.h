//*************************************************************************************************
// Copyright (c) 2017 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
//*************************************************************************************************
#ifndef __CAMERA3_STREAM__
#define __CAMERA3_STREAM__

#include "chi.h"
#include <map>

#include <fcntl.h>
#include <memory>
#include <cinttypes>
#include <sstream>
#include <mutex>

#ifdef OS_ANDROID
#include <dlfcn.h>
#include <system/graphics.h>
#include <ui/GraphicBuffer.h>
using namespace android;
#endif

#define SAFE_NEW() new

#include "ais_log.h"

#define AIS_LOG_CHI_LOG(level, fmt...) AIS_LOG(AIS_MOD_CHI_LOG, level, fmt)
#define AIS_LOG_CHI_LOG_ERR(fmt...)    AIS_LOG(AIS_MOD_CHI_LOG, AIS_LOG_LVL_ERR, fmt)
#define AIS_LOG_CHI_LOG_HIGH(fmt...)   AIS_LOG(AIS_MOD_CHI_LOG, AIS_LOG_LVL_HIGH, fmt)
#define AIS_LOG_CHI_LOG_WARN(fmt...)   AIS_LOG(AIS_MOD_CHI_LOG, AIS_LOG_LVL_WARN, fmt)
#define AIS_LOG_CHI_LOG_HIGH(fmt...)   AIS_LOG(AIS_MOD_CHI_LOG, AIS_LOG_LVL_HIGH, fmt)
#define AIS_LOG_CHI_LOG_MED(fmt...)    AIS_LOG(AIS_MOD_CHI_LOG, AIS_LOG_LVL_MED, fmt)
#define AIS_LOG_CHI_LOG_LOW(fmt...)    AIS_LOG(AIS_MOD_CHI_LOG, AIS_LOG_LVL_LOW, fmt)
#define AIS_LOG_CHI_LOG_DBG(fmt...)    AIS_LOG(AIS_MOD_CHI_LOG, AIS_LOG_LVL_DBG, fmt)
#define AIS_LOG_CHI_LOG_DBG1(fmt...)   AIS_LOG(AIS_MOD_CHI_LOG, AIS_LOG_LVL_DBG1, fmt)

#define NATIVETEST_UNUSED_PARAM(expr) (void)(expr)

typedef CHISTREAMTYPE   camera3_stream_type_t;
typedef CHISTREAM       camera3_stream_t;
typedef CHISTREAMBUFFER camera3_stream_buffer_t;

#ifdef OS_ANDROID
static const std::string ImageRootPath = "/data/misc/camera/nativechitest/";
#else
static const std::string ImageRootPath = "./nativechitest/";
static const std::string OverrideSettings = "camxoverridesettings.txt";
#endif

typedef enum ChiStreamPrivateFormat
{
    PRIVATE_PIXEL_FORMAT_P010     = 0x7FA30C0A,
    PRIVATE_PIXEL_FORMAT_UBWCTP10 = 0x7FA30C09
} CHISTREAMPRIVATEFORMAT;

struct Size
{
    uint32_t width;
    uint32_t height;
    Size(uint32_t w = 0, uint32_t h = 0) : width(w), height(h) {}

    inline Size operator / (int ds) const
            {
        return Size(width / ds, height / ds);
            }
};

const int FPS_30 = 30;
const int FPS_60 = 60;

///Buffer timeout (in s) is obtained by multiplying this by TIMEOUT_RETRIES
///Timeout waiting for buffer on presil
const uint64_t BUFFER_TIMEOUT_PRESIL = 10;
///Timeout waiting for buffer on device
const uint64_t BUFFER_TIMEOUT_DEVICE = 1;

#ifdef OS_ANDROID
const int buftimeOut = BUFFER_TIMEOUT_DEVICE;
#else
const int buftimeOut = BUFFER_TIMEOUT_PRESIL;
#endif // OS_ANDROID
const int TIMEOUT_RETRIES = 10;

///Resolution definitions
const Size RAW_BACK_8996 = Size(5344, 4016);
const Size RAW_FRONT_8996 = Size(4208, 3120);
const Size RDI_MAX_NAPALI = Size(4608, 2592);
const Size FULLHD = Size(1920, 1080);
const Size FULLHD_ALT = Size(1920, 1088);
const Size HD4K = Size(4096, 2160);
const Size UHD = Size(3840, 2160);
const Size HD = Size(1280, 720);
const Size VGA = Size(640, 480);
const Size QVGA = Size(320, 240);
const Size MP21 = Size(5344, 4008);
const Size Size160_120 = Size(160, 120);
const Size Size40_30 = Size(40, 30);

/*
 * Enums defining various names for the streams. Instead of hardcoding stream index
 * we can use these enums to identify the streams
 */
enum StreamUsecases
{
    PREVIEW = 0,
    SNAPSHOT,
    VIDEO,
    REPROCESS,
    PREVIEWCALLBACK,
    THUMBNAIL,
    IFEOutputPortFull,
    IFEOutputPortDS4,
    IFEOutputPortDS16,
    IFEOutputPortCAMIFRaw,
    IFEOutputPortLSCRaw,
    IFEOutputPortGTMRaw,
    IFEOutputPortFD,
    IFEOutputPortPDAF,
    IFEOutputPortRDI0,
    IFEOutputPortRDI1,
    IFEOutputPortRDI2,
    IFEOutputPortRDI3,
    IFEOutputPortStatsRS,
    IFEOutputPortStatsCS,
    IFEOutputPortStatsIHIST,
    IFEOutputPortStatsBHIST,
    IFEOutputPortStatsHDRBE,
    IFEOutputPortStatsHDRBHIST,
    IFEOutputPortStatsTLBG,
    IFEOutputPortStatsBF,
    IFEOutputPortStatsAWBBG,
    BPSInputPort,
    BPSOutputPortFull,
    BPSOutputPortDS4,
    BPSOutputPortDS16,
    BPSOutputPortDS64,
    BPSOutputPortStatsBG,
    BPSOutputPortStatsBhist,
    BPSOutputPortREG1,
    BPSOutputPortREG2,
    IPEInputPortFull,
    IPEInputPortDS4,
    IPEInputPortDS16,
    IPEInputPortDS64,
    IPEInputPortFullRef,
    IPEInputPortDS4Ref,
    IPEInputPortDS16Ref,
    IPEInputPortDS64Ref,
    IPEOutputPortDisplay,
    IPEOutputPortVideo,
    IPEOutputPortFullRef,
    IPEOutputPortDS4Ref,
    IPEOutputPortDS16Ref,
    IPEOutputPortDS64Ref,
    JPEGInputPort,
    JPEGOutputPort,
    JPEGAggregatorInputPort,
    JPEGAggregatorOutputPort,
    GPUInputPort,
    GPUOutputPortFull,
    GPUOutputPortDS4,
    GPUOutputPortDS16,
    GPUOutputPortDS64,
    InvalidPort
};

struct StreamCreateData
{
    StreamCreateData() : num_streams(0), usageFlags(nullptr), filename(nullptr) {}
    int                        num_streams;   ///< number of streams
    CHISTREAMFORMAT*    formats;       ///< format of streams
    camera3_stream_type_t*     directions;    ///< direction of streams
    Size*                      resolutions;   ///< resolution of streams
    StreamUsecases*            streamIds;     ///< names for streams
    uint32_t*                  usageFlags;    ///< usage flags for streams
    const char*                filename;      ///< filename to be read for input stream
    bool                       isJpeg;        ///< toggle between creating jpeg or blob buffer
    bool*                      isRealtime;
};

camera3_stream_t* CreateStreams(StreamCreateData streamInfo);

Size GetGrallocSize(camera3_stream_t* stream);

//map linking ports to strings
extern std::map<StreamUsecases, std::string> mapStreamStr;

#endif  // __CAMERA3_STREAM__
