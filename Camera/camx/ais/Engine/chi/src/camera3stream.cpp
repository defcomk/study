//*************************************************************************************************
// Copyright (c) 2016-2017 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
//*************************************************************************************************

#include "camera3stream.h"

#ifndef OS_ANDROID
typedef enum {
    HAL_PIXEL_FORMAT_RGBA_8888 = 1,
    HAL_PIXEL_FORMAT_RGBX_8888 = 2,
    HAL_PIXEL_FORMAT_RGB_888 = 3,
    HAL_PIXEL_FORMAT_RGB_565 = 4,
    HAL_PIXEL_FORMAT_BGRA_8888 = 5,
    HAL_PIXEL_FORMAT_RGBA_1010102 = 43, // 0x2B
    HAL_PIXEL_FORMAT_RGBA_FP16 = 22, // 0x16
    HAL_PIXEL_FORMAT_YV12 = 842094169, // 0x32315659
    HAL_PIXEL_FORMAT_Y8 = 538982489, // 0x20203859
    HAL_PIXEL_FORMAT_Y16 = 540422489, // 0x20363159
    HAL_PIXEL_FORMAT_RAW16 = 32, // 0x20
    HAL_PIXEL_FORMAT_RAW10 = 37, // 0x25
    HAL_PIXEL_FORMAT_RAW12 = 38, // 0x26
    HAL_PIXEL_FORMAT_RAW_OPAQUE = 36, // 0x24
    HAL_PIXEL_FORMAT_BLOB = 33, // 0x21
    HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED = 34, // 0x22
    HAL_PIXEL_FORMAT_YCBCR_420_888 = 35, // 0x23
    HAL_PIXEL_FORMAT_YCBCR_422_888 = 39, // 0x27
    HAL_PIXEL_FORMAT_YCBCR_444_888 = 40, // 0x28
    HAL_PIXEL_FORMAT_FLEX_RGB_888 = 41, // 0x29
    HAL_PIXEL_FORMAT_FLEX_RGBA_8888 = 42, // 0x2A
    HAL_PIXEL_FORMAT_YCBCR_422_SP = 16, // 0x10
    HAL_PIXEL_FORMAT_YCRCB_420_SP = 17, // 0x11
    HAL_PIXEL_FORMAT_YCBCR_422_I = 20, // 0x14
    HAL_PIXEL_FORMAT_JPEG = 256, // 0x100
} android_pixel_format_t;


#define HAL_PIXEL_FORMAT_YCbCr_420_888 35
#endif


    /*
     * Mapping of the different stream usecases with corresponding strings
     */
    std::map<StreamUsecases, std::string> mapStreamStr =
    {
        { PREVIEW,                 "Prv"           },
        { VIDEO,                   "Video"         },
        { REPROCESS,               "RpInput"       },
        { SNAPSHOT,                "Snap"          },
        { PREVIEWCALLBACK,         "PrvCallBack"   },
        { THUMBNAIL,               "Thumbnail"     },
        { IFEOutputPortFull,       "ifefull"       },
        { IFEOutputPortDS4,        "ifeds4"        },
        { IFEOutputPortDS16,       "ifeds16"       },
        { IFEOutputPortFD,         "ifefd"         },
        { IFEOutputPortLSCRaw,     "ifelsc"        },
        { IFEOutputPortCAMIFRaw,   "ifecamif"      },
        { IFEOutputPortGTMRaw,     "ifegtm"        },
        { IFEOutputPortStatsHDRBE, "ifestatshdrbe" },
        { IFEOutputPortStatsAWBBG, "ifestatsawbbg" },
        { IFEOutputPortStatsBF,    "ifestatsbf"    },
        { IFEOutputPortStatsRS,    "ifestatsrs"    },
        { IFEOutputPortStatsCS,    "ifestatscs"    },
        { IFEOutputPortStatsIHIST, "ifestatsihist" },
        { IFEOutputPortRDI0,       "iferdi0"       },
        { IFEOutputPortRDI1,       "iferdi1"       },
        { IFEOutputPortRDI2,       "iferdi2"       },
        { IFEOutputPortPDAF,       "ifepdaf"       },
        { IPEOutputPortDisplay,    "Prv"           },
        { IPEOutputPortVideo,      "Video"         },
        { BPSOutputPortFull,       "bpsfull"       },
        { BPSOutputPortDS4,        "bpsds4"        },
        { BPSOutputPortDS16,       "bpsds16"       },
        { BPSOutputPortDS64,       "bpsds64"       },
        { BPSOutputPortStatsBG,    "bpsBG"         },
        { BPSOutputPortStatsBhist, "bpsHist"       },
        { JPEGInputPort,           "jpegInput"     },
        { JPEGAggregatorOutputPort,"jpegAgg"       },
        { GPUInputPort,            "gpuInput"      },
        { GPUOutputPortFull,       "gpufull"       },
        { GPUOutputPortDS4,        "gpuds4"        },
        { GPUOutputPortDS16,       "gpuds16"       },
        { GPUOutputPortDS64,       "gpuds64"       },
    };

    /**************************************************************************************************
    *   CreateStreams
    *
    *   @brief
    *       Generates camera3_stream_t* based on stream info
    *   @return
    *       valid stream pointer on success or nullptr on failure
    **************************************************************************************************/
    camera3_stream_t* CreateStreams(
        StreamCreateData streamInfo) //[in] info of the streams to be created
    {

        if (streamInfo.num_streams <= 0)
        {
            AIS_LOG_CHI_LOG_ERR("Number of streams should be greater than 0, given: %d", streamInfo.num_streams);
            return nullptr;
        }

        camera3_stream_t* requiredStreams = SAFE_NEW() camera3_stream_t[streamInfo.num_streams];

        for (int streamIndex = 0; streamIndex < streamInfo.num_streams; streamIndex++)
        {
            requiredStreams[streamIndex].format = streamInfo.formats[streamIndex];
            requiredStreams[streamIndex].width  = streamInfo.resolutions[streamIndex].width;
            requiredStreams[streamIndex].height = streamInfo.resolutions[streamIndex].height;
            if(streamInfo.usageFlags == nullptr)
            {
                requiredStreams[streamIndex].grallocUsage = 0;
            }
            else
            {
                requiredStreams[streamIndex].grallocUsage = streamInfo.usageFlags[streamIndex];
            }
#ifdef OS_ANDROID
            if (streamInfo.formats[streamIndex] == (CHISTREAMFORMAT)(HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED) ||
                streamInfo.formats[streamIndex] == (CHISTREAMFORMAT)(PRIVATE_PIXEL_FORMAT_UBWCTP10) ||
                streamInfo.formats[streamIndex] == (CHISTREAMFORMAT)(PRIVATE_PIXEL_FORMAT_P010))
            {
                requiredStreams[streamIndex].grallocUsage |= GRALLOC_USAGE_PRIVATE_0;
            }
#endif
            if(streamInfo.directions[streamIndex] == ChiStreamTypeOutput /*CAMERA3_STREAM_OUTPUT*/)
            {
#ifdef OS_ANDROID
                requiredStreams[streamIndex].grallocUsage |= GraphicBuffer::USAGE_SW_READ_OFTEN  |
                                                      GraphicBuffer::USAGE_SW_WRITE_NEVER |
                                                      GRALLOC_USAGE_HW_CAMERA_WRITE       |
                                                      GraphicBuffer::USAGE_HW_MASK;
#endif
            }
            else if(streamInfo.directions[streamIndex] == ChiStreamTypeInput /*CAMERA3_STREAM_INPUT*/)
            {
#ifdef OS_ANDROID
                requiredStreams[streamIndex].grallocUsage |= GraphicBuffer::USAGE_SW_READ_RARELY |
                                                      GraphicBuffer::USAGE_SW_WRITE_OFTEN |
                                                      GraphicBuffer::USAGE_HW_MASK;
#endif
            }
            else if(streamInfo.directions[streamIndex] ==  ChiStreamTypeBidirectional /*CAMERA3_STREAM_BIDIRECTIONAL*/)
            {
#ifdef OS_ANDROID
                requiredStreams[streamIndex].grallocUsage |= GraphicBuffer::USAGE_SW_READ_RARELY  |
                                                      GraphicBuffer::USAGE_SW_WRITE_RARELY |
                                                      GRALLOC_USAGE_HW_CAMERA_ZSL    |
                                                      GraphicBuffer::USAGE_HW_MASK;
#endif
            }
            else
            {
                AIS_LOG_CHI_LOG_ERR("Invalid stream direction given: %d", streamInfo.directions[streamIndex]);
                return nullptr;
            }

            requiredStreams[streamIndex].streamType = streamInfo.directions[streamIndex];
            requiredStreams[streamIndex].maxNumBuffers = 0;
            requiredStreams[streamIndex].pPrivateInfo = nullptr;
            requiredStreams[streamIndex].rotation =  StreamRotationCCW0; //CAMERA3_STREAM_ROTATION_0

            switch (static_cast<int>(streamInfo.formats[streamIndex]))  // Cast to int to avoid warnings about diff enum types
            {
                case HAL_PIXEL_FORMAT_RAW16:
                case HAL_PIXEL_FORMAT_RAW12:
                case HAL_PIXEL_FORMAT_RAW10:
                case HAL_PIXEL_FORMAT_RAW_OPAQUE:
                    requiredStreams[streamIndex].dataspace =  DataspaceArbitrary; //(CHIDATASPACE)HAL_DATASPACE_ARBITRARY;
                    break;
                case HAL_PIXEL_FORMAT_BLOB:
                    if (streamInfo.isJpeg)
                    {
                        requiredStreams[streamIndex].dataspace =  DataspaceV0JFIF; //(CHIDATASPACE)HAL_DATASPACE_V0_JFIF;
                    }
                    else
                    {
                        AIS_LOG_CHI_LOG_ERR("Non jpeg test, use dataspace HAL_DATASPACE_ARBITRARY");
                        requiredStreams[streamIndex].dataspace = DataspaceArbitrary; //(CHIDATASPACE)HAL_DATASPACE_ARBITRARY;
                    }
                    break;
                case HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED:
                case PRIVATE_PIXEL_FORMAT_UBWCTP10:
                case PRIVATE_PIXEL_FORMAT_P010:
                    requiredStreams[streamIndex].dataspace = DataspaceUnknown; //(CHIDATASPACE)HAL_DATASPACE_UNKNOWN;
                    break;
                case HAL_PIXEL_FORMAT_YCbCr_420_888:
                    requiredStreams[streamIndex].dataspace = DataspaceBT709; //(CHIDATASPACE)HAL_DATASPACE_STANDARD_BT709;
                    break;
                default:
                    AIS_LOG_CHI_LOG_ERR("Invalid stream format: %d", streamInfo.formats[streamIndex]);
                    return nullptr;
            }
        }

        return requiredStreams;
    }

    /*******************************************************************
    * getGrallocSize
    *
    * @brief
    *     gralloc dimensions may be different due to requirement of the format
    * @return
    *     new size object based on the format
    *
    ********************************************************************/
    Size GetGrallocSize(camera3_stream_t* stream)
    {
        //Gralloc implementation maps Raw Opaque to BLOB
        //Support upto 12 bpp

        Size newResolution;

        switch(stream->format)
        {
            case HAL_PIXEL_FORMAT_BLOB:
                if(stream->dataspace == DataspaceV0JFIF /*HAL_DATASPACE_V0_JFIF*/ ||
                   stream->dataspace == DataspaceJFIF   /*HAL_DATASPACE_JFIF*/)
                {
                    newResolution.width  = stream->width * stream->height * 4;
                    newResolution.height = 1;
                }
                else
                {
                    newResolution.width  = stream->width * stream->height * (3.0 / 2);
                    newResolution.height = 1;
                }
                break;
            case HAL_PIXEL_FORMAT_RAW_OPAQUE:
                newResolution.width  = stream->width * stream->height * (3.0 / 2);
                newResolution.height = 1;
                break;
            default:
                newResolution.width  = stream->width;
                newResolution.height = stream->height;
                break;
        }

        return newResolution;
    }

