////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016-2019 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxifenode.cpp
/// @brief IFE Node class implementation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "camxcslifedefs.h"
/// @todo (CAMX-1295) - Need to figure out the proper way to deal with TPG/SensorEmulation + RealDevice/Presil
#include "camxcsljumptable.h"
#include "camxcslresourcedefs.h"
#include "camxthreadmanager.h"
#include "camxhal3module.h"
#include "camxhwcontext.h"
#include "camxtrace.h"
#include "camxvendortags.h"
#include "camxnode.h"
#include "camxpipeline.h"
#include "camxpropertyblob.h"
#include "parametertuningtypes.h"
#include "camxifeabf34.h"
#include "camxifeawbbgstats14.h"
#include "camxifebfstats23.h"
#include "camxifebfstats24.h"
#include "camxifebhiststats14.h"
#include "camxifebls12.h"
#include "camxifebpcbcc50.h"
#include "camxifecamif.h"
#include "camxifecamiflite.h"
#include "camxifecc12.h"
#include "camxifecrop10.h"
#include "camxifecsstats14.h"
#include "camxifecst12.h"
#include "camxifedemosaic36.h"
#include "camxifedemosaic37.h"
#include "camxifedemux13.h"
#include "camxifeds410.h"
#include "camxifedualpd10.h"
#include "camxifegamma16.h"
#include "camxifegtm10.h"
#include "camxifehdr20.h"
#include "camxifehdr23.h"
#include "camxifehdrbestats15.h"
#include "camxifehdrbhiststats13.h"
#include "camxifehvx.h"
#include "camxifeihiststats12.h"
#include "camxifelinearization33.h"
#include "camxifelsc34.h"
#include "camxifemnds16.h"
#include "camxifepdpc11.h"
#include "camxifepedestal13.h"
#include "camxifeprecrop10.h"
#include "camxifer2pd10.h"
#include "camxiferoundclamp11.h"
#include "camxifersstats14.h"
#include "camxispstatsmodule.h"
#include "camxifetintlessbgstats15.h"
#include "camxifewb12.h"
#include "camximagesensormoduledata.h"
#include "camxiqinterface.h"
#include "camxispiqmodule.h"
#include "camxtitan17xcontext.h"
#include "camxtitan17xdefs.h"
#include "chipinforeaderdefs.h"
#include "stripeIFE.h"
#include "camxifenode.h"
#include "camxswtmc11.h"

#define STRIPE_FIELD_PRINT(field)    \
    CAMX_LOG_ERROR(CamxLogGroupISP, "dualIFE %s = %d", #field, (field))

#define STRIPE_FIELD_PRINT_LL(field) \
    CAMX_LOG_ERROR(CamxLogGroupISP, "dualIFE %s = %lld", #field, (field))

#define PRINT_CROP_1D(_in)                          \
do                                                  \
{                                                   \
    STRIPE_FIELD_PRINT(_in.enable);                 \
    STRIPE_FIELD_PRINT(_in.inDim);                  \
    STRIPE_FIELD_PRINT(_in.firstOut);               \
    STRIPE_FIELD_PRINT(_in.lastOut);                \
} while (0, 0)


#define PRINT_MNDS_V16(_in)                         \
do                                                  \
{                                                   \
    STRIPE_FIELD_PRINT(_in.en);                     \
    STRIPE_FIELD_PRINT(_in.input_l);                \
    STRIPE_FIELD_PRINT(_in.output_l);               \
    STRIPE_FIELD_PRINT(_in.pixel_offset);           \
    STRIPE_FIELD_PRINT(_in.cnt_init);               \
    STRIPE_FIELD_PRINT(_in.interp_reso);            \
    STRIPE_FIELD_PRINT(_in.rounding_option_v);      \
    STRIPE_FIELD_PRINT(_in.rounding_option_h);      \
    STRIPE_FIELD_PRINT(_in.right_pad_en);           \
    STRIPE_FIELD_PRINT(_in.input_processed_length); \
    STRIPE_FIELD_PRINT(_in.phase_init);             \
    STRIPE_FIELD_PRINT(_in.phase_step);             \
} while (0, 0)

#define PRINT_WE_STRIPE(_in)                        \
do                                                  \
{                                                   \
    STRIPE_FIELD_PRINT(_in.enable1);                \
    STRIPE_FIELD_PRINT(_in.hInit1);                 \
    STRIPE_FIELD_PRINT(_in.stripeWidth1);           \
    STRIPE_FIELD_PRINT(_in.enable2);                \
    STRIPE_FIELD_PRINT(_in.hInit2);                 \
    STRIPE_FIELD_PRINT(_in.stripeWidth2);           \
} while (0, 0)

CAMX_NAMESPACE_BEGIN

// TPG defaults, need to derive the value based on the dimension and FPS
static const UINT   TPGPIXELCLOCK   = 400000000;
static const FLOAT  TPGFPS          = 30.0f;

// The third column (installed) indicates if the module is available/enabled on a particular chipset e.g. bit 0 corresponds to
// SDM845/SDM710 etc. while bit 1 corresponds to SDM855. Setting the value of 0x0 disables the module.
//
// For example:
//
// ISPHwTitanInvalid (0b00)                               : No installation
// ISPHwTitan170 (0b01)                                   : Enable and applicable SDM845/SDM710 only
// ISPHwTitan175 (0b10)                                   : Enable and applicable SDM855 (SM8150)/SM7150 only
// (ISPHwTitan170 | ISPHwTitan175) (0b11)                 : Enable and applicable both SDM845/SDM710 and SDM855 (SM8150)
// ISPHwTitan150 (0b100)                                  : Enable and applicable SDM640(SM6150) only.
// (ISPHwTitan170 | ISPHwTitan175 | ISPHwTitan150) (0b111): Enable and applicable for SDM845/SDM710/SDM640/SDM855

IFEIQModuleInfo IFEIQModuleItems[] =
{
    {ISPIQModuleType::SWTMC,                    IFEPipelinePath::CommonPath,
    (ISPHwTitan170 | ISPHwTitan175 | ISPHwTitan150),    SWTMC11::CreateIFE},
    {ISPIQModuleType::IFECAMIF,                 IFEPipelinePath::CommonPath,
    (ISPHwTitan170 | ISPHwTitan175 | ISPHwTitan150),    IFECAMIF::Create},
    {ISPIQModuleType::IFECAMIFLite,             IFEPipelinePath::CommonPath,
    ISPHwTitan175,    IFECAMIFLite::Create},
    /// @todo (CAMX-1133) Will enable when it is verified on hardware
    {ISPIQModuleType::IFEPedestalCorrection,    IFEPipelinePath::CommonPath,
    ISPHwTitanInvalid,    IFEPedestal13::Create},
    {ISPIQModuleType::IFELinearization,         IFEPipelinePath::CommonPath,
    (ISPHwTitan170 | ISPHwTitan175 | ISPHwTitan150),    IFELinearization33::Create},
    {ISPIQModuleType::IFEBLS,                   IFEPipelinePath::CommonPath,
    (ISPHwTitan170 | ISPHwTitan175 | ISPHwTitan150),    IFEBLS12::Create },
    {ISPIQModuleType::IFEPDPC,                  IFEPipelinePath::CommonPath,
    (ISPHwTitan170 | ISPHwTitan175 | ISPHwTitan150),    IFEPDPC11::Create},
    {ISPIQModuleType::IFEDUALPD,                IFEPipelinePath::CommonPath,
    ISPHwTitan175,    IFEDUALPD10::Create},
    {ISPIQModuleType::IFEDemux,                 IFEPipelinePath::CommonPath,
    (ISPHwTitan170 | ISPHwTitan175 | ISPHwTitan150),    IFEDemux13::Create},
    /// @todo (CAMX-1133) Will enable when it is verified on hardware
    {ISPIQModuleType::IFEHDR,                   IFEPipelinePath::CommonPath,
    ISPHwTitan170,    IFEHDR20::Create},
    {ISPIQModuleType::IFEHDR,                   IFEPipelinePath::CommonPath,
    ISPHwTitan175,    IFEHDR22::Create},
    {ISPIQModuleType::IFEHDR,                   IFEPipelinePath::CommonPath,
    ISPHwTitan150,    IFEHDR23::Create},
    {ISPIQModuleType::IFEBPCBCC,                IFEPipelinePath::CommonPath,
    (ISPHwTitan170 | ISPHwTitan175 | ISPHwTitan150),    IFEBPCBCC50::Create},
    {ISPIQModuleType::IFEABF,                   IFEPipelinePath::CommonPath,
    (ISPHwTitan170 | ISPHwTitan175 | ISPHwTitan150),    IFEABF34::Create},
    {ISPIQModuleType::IFELSC,                   IFEPipelinePath::CommonPath,
    (ISPHwTitan170 | ISPHwTitan175 | ISPHwTitan150),    IFELSC34::Create},
    {ISPIQModuleType::IFEWB,                    IFEPipelinePath::CommonPath,
    (ISPHwTitan170 | ISPHwTitan175 | ISPHwTitan150),    IFEWB12::Create},
    {ISPIQModuleType::IFEDemosaic,              IFEPipelinePath::CommonPath,
    (ISPHwTitan170 | ISPHwTitan175),    IFEDemosaic36::Create},
    {ISPIQModuleType::IFEDemosaic,              IFEPipelinePath::CommonPath,
    ISPHwTitan150,    IFEDemosaic37::Create},
    {ISPIQModuleType::IFECC,                    IFEPipelinePath::CommonPath,
    (ISPHwTitan170 | ISPHwTitan175 | ISPHwTitan150),    IFECC12::Create},
    {ISPIQModuleType::IFEGTM,                   IFEPipelinePath::CommonPath,
    (ISPHwTitan170 | ISPHwTitan175 | ISPHwTitan150),    IFEGTM10::Create},
    {ISPIQModuleType::IFEGamma,                 IFEPipelinePath::CommonPath,
    (ISPHwTitan170 | ISPHwTitan175 | ISPHwTitan150),    IFEGamma16::Create},
    {ISPIQModuleType::IFECST,                   IFEPipelinePath::CommonPath,
    (ISPHwTitan170 | ISPHwTitan175 | ISPHwTitan150),    IFECST12::Create},
    {ISPIQModuleType::IFEHVX,                   IFEPipelinePath::CommonPath,
    (ISPHwTitan170 | ISPHwTitan175 | ISPHwTitan150),    IFEHVX::Create },
    {ISPIQModuleType::IFEMNDS,                  IFEPipelinePath::FullPath,
    (ISPHwTitan170 | ISPHwTitan175 | ISPHwTitan150),    IFEMNDS16::Create},
    {ISPIQModuleType::IFECrop,                  IFEPipelinePath::FullPath,
    (ISPHwTitan170 | ISPHwTitan175 | ISPHwTitan150),    IFECrop10::Create},
    {ISPIQModuleType::IFERoundClamp,            IFEPipelinePath::FullPath,
    (ISPHwTitan170 | ISPHwTitan175 | ISPHwTitan150),    IFERoundClamp11::Create},
    {ISPIQModuleType::IFEMNDS,                  IFEPipelinePath::FDPath,
    (ISPHwTitan170 | ISPHwTitan175 | ISPHwTitan150),    IFEMNDS16::Create},
    {ISPIQModuleType::IFECrop,                  IFEPipelinePath::FDPath,
    (ISPHwTitan170 | ISPHwTitan175 | ISPHwTitan150),    IFECrop10::Create},
    {ISPIQModuleType::IFERoundClamp,            IFEPipelinePath::FDPath,
    (ISPHwTitan170 | ISPHwTitan175 | ISPHwTitan150),    IFERoundClamp11::Create},
    {ISPIQModuleType::IFEPreCrop,               IFEPipelinePath::DS4Path,
    (ISPHwTitan170 | ISPHwTitan175 | ISPHwTitan150),    IFEPreCrop10::Create},
    {ISPIQModuleType::IFEDS4  ,                 IFEPipelinePath::DS4Path,
    (ISPHwTitan170 | ISPHwTitan175 | ISPHwTitan150),    IFEDS410::Create},
    {ISPIQModuleType::IFECrop,                  IFEPipelinePath::DS4Path,
    (ISPHwTitan170 | ISPHwTitan175 | ISPHwTitan150),    IFECrop10::Create},
    {ISPIQModuleType::IFERoundClamp,            IFEPipelinePath::DS4Path,
    (ISPHwTitan170 | ISPHwTitan175 | ISPHwTitan150),    IFERoundClamp11::Create},
    {ISPIQModuleType::IFER2PD,                  IFEPipelinePath::DS4Path,
    (ISPHwTitan170 | ISPHwTitan175 | ISPHwTitan150),    IFER2PD10::Create},
    {ISPIQModuleType::IFEPreCrop,               IFEPipelinePath::DS16Path,
    (ISPHwTitan170 | ISPHwTitan175 | ISPHwTitan150),    IFEPreCrop10::Create},
    {ISPIQModuleType::IFEDS4  ,                 IFEPipelinePath::DS16Path,
    (ISPHwTitan170 | ISPHwTitan175 | ISPHwTitan150),    IFEDS410::Create},
    {ISPIQModuleType::IFECrop,                  IFEPipelinePath::DS16Path,
    (ISPHwTitan170 | ISPHwTitan175 | ISPHwTitan150),    IFECrop10::Create},
    {ISPIQModuleType::IFERoundClamp,            IFEPipelinePath::DS16Path,
    (ISPHwTitan170 | ISPHwTitan175 | ISPHwTitan150),    IFERoundClamp11::Create},
    {ISPIQModuleType::IFER2PD,                  IFEPipelinePath::DS16Path,
    (ISPHwTitan170 | ISPHwTitan175 | ISPHwTitan150),    IFER2PD10::Create},
    {ISPIQModuleType::IFECrop,                  IFEPipelinePath::PixelRawDumpPath,
    (ISPHwTitan170 | ISPHwTitan175 | ISPHwTitan150),    IFECrop10::Create},
    {ISPIQModuleType::IFEMNDS,                  IFEPipelinePath::DisplayFullPath,
    ISPHwTitan175,    IFEMNDS16::Create},
    {ISPIQModuleType::IFECrop,                  IFEPipelinePath::DisplayFullPath,
    ISPHwTitan175,    IFECrop10::Create},
    {ISPIQModuleType::IFERoundClamp,            IFEPipelinePath::DisplayFullPath,
    ISPHwTitan175,    IFERoundClamp11::Create},
    {ISPIQModuleType::IFEPreCrop,               IFEPipelinePath::DisplayDS4Path,
    ISPHwTitan175,    IFEPreCrop10::Create},
    {ISPIQModuleType::IFEDS4  ,                 IFEPipelinePath::DisplayDS4Path,
    ISPHwTitan175,    IFEDS410::Create},
    {ISPIQModuleType::IFECrop,                  IFEPipelinePath::DisplayDS4Path,
    ISPHwTitan175,    IFECrop10::Create},
    {ISPIQModuleType::IFERoundClamp,            IFEPipelinePath::DisplayDS4Path,
    ISPHwTitan175,    IFERoundClamp11::Create},
    {ISPIQModuleType::IFER2PD,                  IFEPipelinePath::DisplayDS4Path,
    ISPHwTitan175,    IFER2PD10::Create},
    {ISPIQModuleType::IFEPreCrop,               IFEPipelinePath::DisplayDS16Path,
    ISPHwTitan175,    IFEPreCrop10::Create},
    {ISPIQModuleType::IFEDS4  ,                 IFEPipelinePath::DisplayDS16Path,
    ISPHwTitan175,    IFEDS410::Create},
    {ISPIQModuleType::IFECrop,                  IFEPipelinePath::DisplayDS16Path,
    ISPHwTitan175,    IFECrop10::Create},
    {ISPIQModuleType::IFERoundClamp,            IFEPipelinePath::DisplayDS16Path,
    ISPHwTitan175,    IFERoundClamp11::Create},
    {ISPIQModuleType::IFER2PD,                  IFEPipelinePath::DisplayDS16Path,
    ISPHwTitan175,    IFER2PD10::Create},
};

static IFEStatsModuleInfo IFEStatsModuleItems[] =
{
    {ISPStatsModuleType::IFERS,         (ISPHwTitan170 | ISPHwTitan175 | ISPHwTitan150), RSStats14::Create},
    {ISPStatsModuleType::IFECS,         (ISPHwTitan170 | ISPHwTitan175 | ISPHwTitan150), CSStats14::Create},
    {ISPStatsModuleType::IFEIHist,      (ISPHwTitan170 | ISPHwTitan175 | ISPHwTitan150), IHistStats12::Create},
    {ISPStatsModuleType::IFEBHist,      (ISPHwTitan170 | ISPHwTitan175 | ISPHwTitan150), BHistStats14::Create},
    {ISPStatsModuleType::IFEHDRBE,      (ISPHwTitan170 | ISPHwTitan175 | ISPHwTitan150), HDRBEStats15::Create},
    {ISPStatsModuleType::IFEHDRBHist,   (ISPHwTitan170 | ISPHwTitan175 | ISPHwTitan150), HDRBHistStats13::Create},
    {ISPStatsModuleType::IFETintlessBG, (ISPHwTitan170 | ISPHwTitan175 | ISPHwTitan150), TintlessBGStats15::Create},
    {ISPStatsModuleType::IFEBF,         (ISPHwTitan170 | ISPHwTitan175),                 IFEBFStats23::Create},
    {ISPStatsModuleType::IFEBF,         ISPHwTitan150,                                   IFEBFStats24::Create},
    {ISPStatsModuleType::IFEAWBBG,      (ISPHwTitan170 | ISPHwTitan175 | ISPHwTitan150), AWBBGStats14::Create}
};

static UINT64 IFEStatsModuleOutputPorts[] =
{
    IFEOutputPortStatsRS,
    IFEOutputPortStatsCS,
    IFEOutputPortStatsIHIST,
    IFEOutputPortStatsBHIST,
    IFEOutputPortStatsHDRBE,
    IFEOutputPortStatsHDRBHIST,
    IFEOutputPortStatsTLBG,
    IFEOutputPortStatsBF,
    IFEOutputPortStatsBF,
    IFEOutputPortStatsAWBBG
};

/// @brief Command buffer identifiers
enum class IFECmdBufferId: UINT32
{
    Packet = 0,     ///< Packet
    Main,           ///< Main IQ command buffer
    Left,           ///< Left IQ command buffer
    Right,          ///< Right IQ command buffer
    DMI32,          ///< DMI32 command buffer
    DMI64,          ///< DMI64 command buffer
    DualConfig,     ///< Dual IFE config command buffer
    GenericBlob,    ///< Generic command buffer
    NumCmdBuffers   ///< Max number of command buffers
};

static const UINT   IFEKMDCmdBufferMaxSize  = 8 * 1024;                 ///< Reserved KMD Cmd Buffer Size

UINT32 CGCOffCmds[] =
{
    regIFE_IFE_0_VFE_MODULE_LENS_CGC_OVERRIDE,    0x0,
    regIFE_IFE_0_VFE_MODULE_STATS_CGC_OVERRIDE,   0x0,
    regIFE_IFE_0_VFE_MODULE_COLOR_CGC_OVERRIDE,   0x0,
    regIFE_IFE_0_VFE_MODULE_ZOOM_CGC_OVERRIDE,    0x0,
    regIFE_IFE_0_VFE_MODULE_BUS_CGC_OVERRIDE,     0x0,
    regIFE_IFE_0_VFE_MODULE_DUAL_PD_CGC_OVERRIDE, 0x0
};

UINT32 CGCOnCmds[] =
{
    regIFE_IFE_0_VFE_MODULE_LENS_CGC_OVERRIDE,    0xFFFFFFFF,
    regIFE_IFE_0_VFE_MODULE_STATS_CGC_OVERRIDE,   0xFFFFFFFF,
    regIFE_IFE_0_VFE_MODULE_COLOR_CGC_OVERRIDE,   0xFFFFFFFF,
    regIFE_IFE_0_VFE_MODULE_ZOOM_CGC_OVERRIDE,    0xFFFFFFFF,
    regIFE_IFE_0_VFE_MODULE_BUS_CGC_OVERRIDE,     0xFFFFFFFF,
    regIFE_IFE_0_VFE_MODULE_DUAL_PD_CGC_OVERRIDE, 0x1
};

// @brief list of usecase tags published by IFE node
static const UINT32 IFEUsecaseTags[] =
{
    PropertyIDUsecaseIFEPDAFInfo,
};

static const UINT32 IFECGCNumRegs = sizeof(CGCOnCmds) / (2 * sizeof(UINT32));

static const UINT32 IFELeftPartialTileShift     = IFE_IFE_0_BUS_WR_CLIENT_3_TILE_CFG_PARTIAL_TILE_LEFT_SHIFT;
static const UINT32 IFELeftPartialTileMask      = IFE_IFE_0_BUS_WR_CLIENT_3_TILE_CFG_PARTIAL_TILE_LEFT_MASK;
static const UINT32 IFERightPartialTileShift    = IFE_IFE_0_BUS_WR_CLIENT_3_TILE_CFG_PARTIAL_TILE_RIGHT_SHIFT;
static const UINT32 IFERightPartialTileMask     = IFE_IFE_0_BUS_WR_CLIENT_3_TILE_CFG_PARTIAL_TILE_RIGHT_MASK;

// Make this a setting
static const BOOL IFEOverwriteModuleWithStriping = TRUE;    ///< Overwrite IQ modules using striping output

static const UINT32 IFEMaxPdafPixelsPerBlock     = (16 * 16); ///< Maximum Pdaf Pixels Per block

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// TranslateFormatToISPImageFormat
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
UINT32 TranslateFormatToISPImageFormat(
    CamX::Format format,
    UINT8        CSIDecodeBitWidth)
{
    UINT32 ISPFormat = ISPFormatUndefined;

    switch (format)
    {
        case CamX::Format::YUV420NV12:
        case CamX::Format::FDYUV420NV12:
            ISPFormat = ISPFormatNV12;
            break;

        case CamX::Format::YUV420NV21:
            ISPFormat = ISPFormatNV21;
            break;

        case CamX::Format::UBWCTP10:
            ISPFormat = ISPFormatUBWCTP10;
            break;

        case CamX::Format::UBWCNV12:
            ISPFormat = ISPFormatUBWCNV12;
            break;

        case CamX::Format::UBWCNV124R:
            ISPFormat = ISPFormatUBWCNV124R;
            break;
        case CamX::Format::YUV420NV12TP10:
            ISPFormat = ISPFormatTP10;
            break;
        case CamX::Format::RawMIPI8:
            ISPFormat = ISPFormatMIPIRaw8;
            break;
        case CamX::Format::P010:
            ISPFormat = ISPFormatPlain1610;
            break;
        case CamX::Format::RawMIPI:
            switch (CSIDecodeBitWidth)
            {
                case CSIDecode6Bit:
                    ISPFormat = ISPFormatMIPIRaw6;
                    break;
                case CSIDecode8Bit:
                    ISPFormat = ISPFormatMIPIRaw8;
                    break;
                case CSIDecode10Bit:
                    ISPFormat = ISPFormatMIPIRaw10;
                    break;
                case CSIDecode12Bit:
                    ISPFormat = ISPFormatMIPIRaw12;
                    break;
                case CSIDecode14Bit:
                    ISPFormat = ISPFormatMIPIRaw14;
                    break;
                default:
                    ISPFormat = ISPFormatUndefined;
                    CAMX_ASSERT_ALWAYS_MESSAGE("Invalid CSID bit width %d", CSIDecodeBitWidth);
                    break;
            }
            break;

        case CamX::Format::RawPlain16:
            switch (CSIDecodeBitWidth)
            {
                case CSIDecode8Bit:
                    ISPFormat = ISPFormatPlain168;
                    break;
                case CSIDecode10Bit:
                    ISPFormat = ISPFormatPlain1610;
                    break;
                case CSIDecode12Bit:
                    ISPFormat = ISPFormatPlain1612;
                    break;
                case CSIDecode14Bit:
                    ISPFormat = ISPFormatPlain1614;
                    break;
                default:
                    ISPFormat = ISPFormatUndefined;
                    CAMX_ASSERT_ALWAYS_MESSAGE("Invalid CSID bit width %d", CSIDecodeBitWidth);
                    break;
            }
            break;

        case CamX::Format::RawPlain64:
            ISPFormat = ISPFormatPlain64;
            break;

        case CamX::Format::RawYUV8BIT:
            ISPFormat = ISPFormatPlain8;
            break;
        case CamX::Format::Y8:
        case CamX::Format::FDY8:
            ISPFormat = ISPFormatY;
            break;
        case CamX::Format::Jpeg:
        case CamX::Format::YUV422NV16:
        case CamX::Format::Y16:
        case CamX::Format::Blob:
        case CamX::Format::RawPrivate:
        case CamX::Format::RawMeta8BIT:
        default:
            ISPFormat = ISPFormatUndefined;
            CAMX_ASSERT_ALWAYS_MESSAGE("Unsupported format %d", format);
            break;
    }

    return ISPFormat;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::TranslateCSIDataTypeToCSIDecodeFormat
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
UINT8 IFENode::TranslateCSIDataTypeToCSIDecodeFormat(
    const UINT8 CSIDataType)
{
    UINT8 CSIDecodeFormat = 0;

    switch (CSIDataType)
    {
        case CSIDataTypeYUV422_8:
        case CSIDataTypeRaw8:
            CSIDecodeFormat = CSIDecode8Bit;
            break;

        case CSIDataTypeRaw10:
            CSIDecodeFormat = CSIDecode10Bit;
            break;

        case CSIDataTypeRaw12:
            CSIDecodeFormat = CSIDecode12Bit;
            break;

        case CSIDataTypeRaw14:
            CSIDecodeFormat = CSIDecode14Bit;
            break;

        default:
            CSIDecodeFormat = CSIDecode10Bit;
            CAMX_LOG_ERROR(CamxLogGroupISP,
                           "Unable to translate DT = %02x to CSI decode bit format. Assuming 10 bit!",
                           CSIDataType);
            break;
    }

    return CSIDecodeFormat;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::TranslateBitDepthToCSIDecodeFormat
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
UINT8 IFENode::TranslateBitDepthToCSIDecodeFormat(
    const UINT32 bitWidth)
{
    UINT8 CSIDecodeFormat;

    switch (bitWidth)
    {
        case 6:
            CSIDecodeFormat = CSIDecode6Bit;
            break;

        case 8:
            CSIDecodeFormat = CSIDecode8Bit;
            break;

        case 10:
            CSIDecodeFormat = CSIDecode10Bit;
            break;

        case 12:
            CSIDecodeFormat = CSIDecode12Bit;
            break;

        case 14:
            CSIDecodeFormat = CSIDecode14Bit;
            break;

        default:
            CSIDecodeFormat = CSIDecode10Bit;
            CAMX_LOG_ERROR(CamxLogGroupISP,
                           "Unable to translate bit width = %u to CSI decode bit format. Assuming 10 bit!",
                           bitWidth);
            break;
    }

    return CSIDecodeFormat;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::TranslateSensorStreamConfigTypeToPortSourceType
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
UINT IFENode::TranslateSensorStreamConfigTypeToPortSourceType(
    StreamType streamType)
{
    UINT portSourceTypeId = PortSrcTypeUndefined;

    switch (streamType)
    {
        case StreamType::IMAGE:
            portSourceTypeId = PortSrcTypePixel;
            break;

        case StreamType::PDAF:
            portSourceTypeId = PortSrcTypePDAF;
            break;

        case StreamType::HDR:
            portSourceTypeId = PortSrcTypeHDR;
            break;

        case StreamType::META:
            portSourceTypeId = PortSrcTypeMeta;
            break;

        default:
            CAMX_LOG_ERROR(CamxLogGroupISP,
                           "Unable to translate sensor stream type = %u",
                           streamType);
            break;
    }

    return portSourceTypeId;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::TranslatePortSourceTypeToSensorStreamConfigType
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
UINT32 IFENode::TranslatePortSourceTypeToSensorStreamConfigType(
    UINT portSourceTypeId)
{
    StreamType streamType;

    switch (portSourceTypeId)
    {
        case PortSrcTypePixel:
            streamType = StreamType::IMAGE;
            break;

        case PortSrcTypePDAF:
            streamType = StreamType::PDAF;
            break;

        case PortSrcTypeHDR:
            streamType = StreamType::HDR;
            break;

        case PortSrcTypeMeta:
            streamType = StreamType::META;
            break;

        case PortSrcTypeUndefined:
        default:
            streamType = StreamType::BLOB;
            break;
    }

    return static_cast<UINT32>(streamType);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::TranslateColorFilterPatternToISPPattern
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
UINT32 IFENode::TranslateColorFilterPatternToISPPattern(
    const enum SensorInfoColorFilterArrangementValues colorFilterArrangementValue)
{
    UINT32 ISPPattern = 0;

    switch (colorFilterArrangementValue)
    {
        case SensorInfoColorFilterArrangementRggb:
            ISPPattern = ISPPatternBayerRGRGRG;
            break;

        case SensorInfoColorFilterArrangementGrbg:
            ISPPattern = ISPPatternBayerGRGRGR;
            break;

        case SensorInfoColorFilterArrangementGbrg:
            ISPPattern = ISPPatternBayerGBGBGB;
            break;

        case SensorInfoColorFilterArrangementBggr:
            ISPPattern = ISPPatternBayerBGBGBG;
            break;

        case SensorInfoColorFilterArrangementY:
            ISPPattern = ISPPatternBayerRGRGRG;
            break;

        default:
            ISPPattern = ISPPatternBayerRGRGRG;

            CAMX_LOG_ERROR(CamxLogGroupISP,
                           "Unable to translate SensorInfoColorFilterArrangementValue = %d to ISP Bayer pattern."
                           "Assuming ISPPatternBayerRGRGRG!",
                           colorFilterArrangementValue);
            break;
    }

    return ISPPattern;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::FindSensorStreamConfigIndex
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL IFENode::FindSensorStreamConfigIndex(
    StreamType  streamType,
    UINT*       pStreamIndex)
{
    BOOL foundStream = FALSE;

    for (UINT32 index = 0; index < m_pSensorModeData->streamConfigCount; index++)
    {
        if (m_pSensorModeData->streamConfig[index].type == streamType)
        {
            foundStream  = TRUE;
            if (NULL != pStreamIndex)
            {
                *pStreamIndex = index;
            }
            break;
        }
    }

    return foundStream;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::CheckOutputPortIndexIfUnsupported
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL IFENode::CheckOutputPortIndexIfUnsupported(
    UINT outputPortIndex)
{
    BOOL needOutputPortDisabled = FALSE;

    if ((FALSE           == FindSensorStreamConfigIndex(StreamType::PDAF, NULL)) &&
        (PortSrcTypePDAF == GetOutputPortSourceType(outputPortIndex)))
    {
        needOutputPortDisabled = TRUE;
    }
    else if ((FALSE          == FindSensorStreamConfigIndex(StreamType::HDR, NULL)) &&
             (PortSrcTypeHDR == GetOutputPortSourceType(outputPortIndex)))
    {
        needOutputPortDisabled = TRUE;
    }
    else if ((FALSE           == FindSensorStreamConfigIndex(StreamType::META, NULL)) &&
             (PortSrcTypeMeta == GetOutputPortSourceType(outputPortIndex)))
    {
        needOutputPortDisabled = TRUE;
    }

    return needOutputPortDisabled;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::CheckIfPDAFType3Supported
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL IFENode::CheckIfPDAFType3Supported()
{
    BOOL                         enableCAMIFSubsample    = FALSE;
    UINT32                       cameraID                = GetPipeline()->GetCameraId();
    const ImageSensorModuleData* pSensorModuleData       = GetHwContext()->GetImageSensorModuleData(cameraID);
    UINT                         currentMode             = 0;
    BOOL                         isSensorModeSupportPDAF = FALSE;

    static const UINT GetProps[] =
    {
        PropertyIDUsecaseSensorCurrentMode
    };

    static const UINT GetPropsLength          = CAMX_ARRAY_SIZE(GetProps);
    VOID*             pData[GetPropsLength]   = { 0 };
    UINT64            offsets[GetPropsLength] = { 0 };

    GetDataList(GetProps, pData, offsets, GetPropsLength);

    PDLibSensorType sensorCurrentModePDAFType = PDLibSensorInvalid;

    if (NULL != pData[0])
    {
        currentMode = *reinterpret_cast<UINT*>(pData[0]);
        pSensorModuleData->GetPDAFInformation(currentMode,
            &isSensorModeSupportPDAF, reinterpret_cast<PDAFType*>(&sensorCurrentModePDAFType));
    }

    CAMX_LOG_INFO(CamxLogGroupISP, "PDAFType is %d", sensorCurrentModePDAFType);

    // Check if PDAF is disabled with camx settings
    if (FALSE == GetStaticSettings()->disablePDAF)
    {
        if (TRUE == isSensorModeSupportPDAF)
        {
            if (PDLibSensorType3 == sensorCurrentModePDAFType)
            {
                enableCAMIFSubsample = TRUE;
            }
        }
    }

    return enableCAMIFSubsample;
}



////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::IsPixelOutputPortSourceType
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL IFENode::IsPixelOutputPortSourceType(
    UINT outputPortId)
{
    BOOL isPixelPortSourceType = FALSE;

    const UINT portSourceTypeId = GetOutputPortSourceType(OutputPortIndex(outputPortId));

    switch (portSourceTypeId)
    {
        case PortSrcTypeUndefined:
        case PortSrcTypePixel:
            isPixelPortSourceType = TRUE;
            break;

        default:
            break;
    }

    return isPixelPortSourceType;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::SetRDIOutputPortFormat
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFENode::SetRDIOutputPortFormat(
    ISPOutResourceInfo* pOutputResource,
    Format              format,
    UINT                outputPortId,
    UINT                portSourceTypeId)
{
    CamxResult result = CamxResultSuccess;

    switch (portSourceTypeId)
    {
        case PortSrcTypeUndefined:
        case PortSrcTypePixel:
            pOutputResource->format = TranslateFormatToISPImageFormat(format, m_CSIDecodeBitWidth);
            break;

        case PortSrcTypePDAF:
            switch (format)
            {
                case Format::Blob:
                    pOutputResource->format = ISPFormatPlain128;
                    CAMX_LOG_VERBOSE(CamxLogGroupISP,
                                     "For PDAF stream input resource, configure output portId = %u as Plain128",
                                     outputPortId);
                    break;

                case Format::RawPlain16:
                    pOutputResource->format = TranslateFormatToISPImageFormat(format, m_PDAFCSIDecodeFormat);
                    CAMX_LOG_VERBOSE(CamxLogGroupISP,
                                     "For PDAF stream input resource, configure output portId = %u as RawPlain16",
                                     outputPortId);
                    break;

                case Format::RawPlain64:
                    pOutputResource->format = TranslateFormatToISPImageFormat(format, m_PDAFCSIDecodeFormat);
                    CAMX_LOG_VERBOSE(CamxLogGroupISP,
                                     "For PDAF stream input resource, configure output portId = %u as RawPlain16",
                                     outputPortId);
                    break;

                default:
                    CAMX_LOG_ERROR(CamxLogGroupISP,
                                   "Unsupported RDI output format for PDAF stream input resource. Format = %u",
                                   format);
                    result = CamxResultEUnsupported;
                    break;
            }
            break;

        case PortSrcTypeMeta:
            switch (format)
            {
                case Format::Blob:
                    pOutputResource->format = ISPFormatPlain128;
                    CAMX_LOG_VERBOSE(CamxLogGroupISP,
                                     "For META stream input resource, configure output portId = %u as Plain128",
                                     outputPortId);
                    break;

                default:
                    CAMX_LOG_ERROR(CamxLogGroupISP,
                                   "Unsupported RDI output format for META stream input resource. Format = %u",
                                   format);
                    result = CamxResultEUnsupported;
                    break;
            }
            break;

        case PortSrcTypeHDR:
            switch (format)
            {
                case Format::Blob:
                    pOutputResource->format = ISPFormatPlain128;
                    CAMX_LOG_VERBOSE(CamxLogGroupISP,
                                     "For HDR stream input resource, configure output portId = %u as Plain128",
                                     outputPortId);
                    break;

                case Format::RawMIPI:
                    pOutputResource->format = TranslateFormatToISPImageFormat(format, m_HDRCSIDecodeFormat);
                    CAMX_LOG_VERBOSE(CamxLogGroupISP,
                                     "For HDR stream input resource, configure output portId = %u as RawMIPI",
                                     outputPortId);
                    break;

                default:
                    CAMX_LOG_ERROR(CamxLogGroupISP,
                                   "Unsupported RDI output format for HDR stream input resource. Format = %u",
                                   format);
                    result = CamxResultEUnsupported;
                    break;
            }
            break;

        default:
            CAMX_LOG_ERROR(CamxLogGroupISP,
                           "Unsupported port source type = %u for RDI output port = %u",
                           portSourceTypeId,
                           outputPortId);
            result = CamxResultEUnsupported;
            break;
    }

    return result;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::IFENode
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IFENode::IFENode()
    : m_mode(IFEModuleMode::SingleIFENormal)
    , m_useStatsAlgoConfig(TRUE)
    , m_initialConfigPending (TRUE)
    , m_highInitialBWCnt(0)
    , m_pIFEResourceInfo(NULL)
    , m_isIFEResourceAcquried(FALSE)
    , m_enableBusRead(FALSE)
    , m_computeActiveABVotes(FALSE)
{
    m_ISPFrameData.pFrameData   = &m_ISPFramelevelData;
    m_ISPInputSensorData.dGain  = 1.0f;
    m_pNodeName                 = "IFE";
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::ReleaseDevice
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFENode::ReleaseDevice()
{
    CamxResult result = CamxResultSuccess;

    if ((NULL != GetHwContext()) && (0 != m_hDevice))
    {
        result = CSLReleaseDevice(GetCSLSession(), m_hDevice);
        CAMX_LOG_INFO(CamxLogGroupCore, "Releasing - IFEDevice : %s, Result: %d", NodeIdentifierString(), result);
        if (CamxResultSuccess == result)
        {
            SetDeviceAcquired(FALSE);
        }
        else
        {
            CAMX_ASSERT_ALWAYS_MESSAGE("Failed to release device");
        }
        if (NULL != m_pIFEResourceInfo)
        {
            CAMX_FREE(m_pIFEResourceInfo);
            m_pIFEResourceInfo = NULL;
        }
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupISP, "Failed: m_hDevice = %d HwContext = %p", m_hDevice, GetHwContext());
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::~IFENode
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IFENode::~IFENode()
{
    Cleanup();

    if (TRUE == IsDeviceAcquired())
    {
        IQInterface::IQSettingModuleUninitialize(&m_libInitialData);

        ReleaseDevice();
    }
    else
    {
        CAMX_LOG_INFO(CamxLogGroupISP, "No ReleaseDevice, IsDeviceAcquired = %d ", IsDeviceAcquired());
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::Create
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IFENode* IFENode::Create(
    const NodeCreateInputData* pCreateInputData,
    NodeCreateOutputData*      pCreateOutputData)
{
    CAMX_UNREFERENCED_PARAM(pCreateOutputData);

    if ((NULL != pCreateInputData) && (NULL != pCreateInputData->pNodeInfo))
    {
        UINT32           propertyCount   = pCreateInputData->pNodeInfo->nodePropertyCount;
        PerNodeProperty* pNodeProperties = pCreateInputData->pNodeInfo->pNodeProperties;

        IFENode* pNodeObj = CAMX_NEW IFENode;

        if (NULL != pNodeObj)
        {
            CAMX_LOG_VERBOSE(CamxLogGroupISP, "nodePropertyCount %d", propertyCount);

            // There can be multiple IFE instances in a pipeline, each instance can have different properties
            for (UINT32 count = 0; count < propertyCount; count++)
            {
                UINT32 nodePropertyId     = pNodeProperties[count].id;
                VOID*  pNodePropertyValue = pNodeProperties[count].pValue;

                switch (nodePropertyId)
                {
                    case NodePropertyIFECSIDHeight:
                        pNodeObj->m_instanceProperty.IFECSIDHeight = atoi(static_cast<const CHAR*>(pNodePropertyValue));
                        break;
                    case NodePropertyIFECSIDWidth:
                        pNodeObj->m_instanceProperty.IFECSIDWidth = atoi(static_cast<const CHAR*>(pNodePropertyValue));
                        break;
                    case NodePropertyIFECSIDLeft:
                        pNodeObj->m_instanceProperty.IFECSIDLeft = atoi(static_cast<const CHAR*>(pNodePropertyValue));
                        break;
                    case NodePropertyIFECSIDTop:
                        pNodeObj->m_instanceProperty.IFECSIDTop = atoi(static_cast<const CHAR*>(pNodePropertyValue));
                        break;
                    case NodePropertyStatsSkipPattern:
                        // Incase of 60 fps preview, stats should run at 30 fps(tintless at 15),
                        // this property defines the skip pattern.
                        pNodeObj->m_instanceProperty.IFEStatsSkipPattern = (2 * (*static_cast<UINT*>(pNodePropertyValue)));
                        break;
                    case NodePropertyForceSingleIFEOn:
                        pNodeObj->m_instanceProperty.IFESingleOn = atoi(static_cast<CHAR*>(pNodePropertyValue));
                        break;
                    default:
                        CAMX_LOG_WARN(CamxLogGroupPProc, "Unhandled node property Id %d", nodePropertyId);
                        break;
                }
            }

            CAMX_LOG_INFO(CamxLogGroupPProc, "IFE instance CSIDHeight: %d, CSIDWidth: %d, CSIDLeft: %d, CSIDTop: %d, "
                          "SkipPattern: %d, ForceSingleIFEOn: %d",
                          pNodeObj->m_instanceProperty.IFECSIDHeight,
                          pNodeObj->m_instanceProperty.IFECSIDWidth,
                          pNodeObj->m_instanceProperty.IFECSIDLeft,
                          pNodeObj->m_instanceProperty.IFECSIDTop,
                          pNodeObj->m_instanceProperty.IFEStatsSkipPattern,
                          pNodeObj->m_instanceProperty.IFESingleOn);
        }
        else
        {
            CAMX_LOG_ERROR(CamxLogGroupPProc, "Failed to create IFENode, no memory");
        }

        return pNodeObj;
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupISP, "Null input pointer");
        return NULL;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::Destroy
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IFENode::Destroy()
{
    CAMX_DELETE this;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::ProcessingNodeInitialize
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFENode::ProcessingNodeInitialize(
    const NodeCreateInputData* pCreateInputData,
    NodeCreateOutputData*      pCreateOutputData)
{

    CamxResult  result                  = CamxResultSuccess;
    INT32       deviceIndex             = -1;
    UINT        indicesLengthRequired   = 0;
    UINT32      IFETestImageSizeWidth;
    UINT32      IFETestImageSizeHeight;

    CAMX_ASSERT(IFE == Type());

    Titan17xContext* pContext = NULL;

    pContext             = static_cast<Titan17xContext *>(GetHwContext());
    m_OEMIQSettingEnable = pContext->GetTitan17xSettingsManager()->GetTitan17xStaticSettings()->IsOEMIQSettingEnable;

    UINT inputPortId[MaxIFEInputPorts];

    pCreateOutputData->maxOutputPorts = MaxIFEOutputPorts;
    pCreateOutputData->maxInputPorts  = MaxIFEInputPorts;

    GetHwContext()->GetDeviceVersion(CSLDeviceTypeIFE, &m_version);

    m_maxNumOfCSLIFEPortId = CSLIFEPortIdMaxNumPortResourcesOfPlatform(pContext->GetTitanVersion());

    GetAllInputPortIds(&m_totalInputPorts, &inputPortId[0]);
    CAMX_LOG_INFO(CamxLogGroupISP, "m_totalInputPorts = %u", m_totalInputPorts);

    UINT numOutputPorts = 0;
    UINT outputPortId[MaxBufferComposite];

    GetAllOutputPortIds(&numOutputPorts, &outputPortId[0]);

    CAMX_ASSERT_MESSAGE(numOutputPorts > 0, "Failed to get output port.");


    if (TRUE == IsSecureMode())
    {
        for (UINT outputPortIndex = 0; outputPortIndex < numOutputPorts; outputPortIndex++)
        {
            // Except stats,set all ports as secured if node is secure node
            if (TRUE == IsStatsOutputPort(outputPortId[outputPortIndex]))
            {
                ResetSecureMode(outputPortId[outputPortIndex]);
            }
        }
    }

    CAMX_ASSERT(MaxBufferComposite >= numOutputPorts);

    UINT32 groupID = ISPOutputGroupIdMAX;
    for (UINT outputPortIndex = 0; outputPortIndex < numOutputPorts; outputPortIndex++)
    {
        switch (outputPortId[outputPortIndex])
        {

            case IFEOutputPortFull:
            case IFEOutputPortDS4:
            case IFEOutputPortDS16:
                pCreateOutputData->bufferComposite.portGroupID[outputPortId[outputPortIndex]] =
                                                                               ISPOutputGroupId0;
                break;

            case IFEOutputPortDisplayFull:
            case IFEOutputPortDisplayDS4:
            case IFEOutputPortDisplayDS16:
                pCreateOutputData->bufferComposite.portGroupID[outputPortId[outputPortIndex]] =
                                                                               ISPOutputGroupId1;
                break;

            case IFEOutputPortStatsHDRBHIST:
            case IFEOutputPortStatsAWBBG:
            case IFEOutputPortStatsHDRBE:
            case IFEOutputPortStatsIHIST:
            case IFEOutputPortStatsCS:
            case IFEOutputPortStatsRS:
            case IFEOutputPortStatsTLBG:
            case IFEOutputPortStatsBHIST:
                pCreateOutputData->bufferComposite.portGroupID[outputPortId[outputPortIndex]] =
                    ISPOutputGroupId2;
                break;

            case IFEOutputPortStatsBF:
                pCreateOutputData->bufferComposite.portGroupID[IFEOutputPortStatsBF] = ISPOutputGroupId3;
                break;

            case IFEOutputPortFD:
                pCreateOutputData->bufferComposite.portGroupID[IFEOutputPortFD] = ISPOutputGroupId4;
                break;

            case IFEOutputPortCAMIFRaw:
            case IFEOutputPortLSCRaw:
            case IFEOutputPortGTMRaw:
                pCreateOutputData->bufferComposite.portGroupID[outputPortId[outputPortIndex]] =
                    ISPOutputGroupId5;
                break;

            case IFEOutputPortPDAF:
                pCreateOutputData->bufferComposite.portGroupID[IFEOutputPortPDAF] = ISPOutputGroupId6;
                break;

            default:
                pCreateOutputData->bufferComposite.portGroupID[outputPortId[outputPortIndex]] =
                    groupID++;
                break;
        }
    }

    pCreateOutputData->bufferComposite.hasCompositeMask = TRUE;
    Utils::Memcpy(&m_bufferComposite, &pCreateOutputData->bufferComposite, sizeof(BufferGroup));

    IFETestImageSizeWidth   = GetStaticSettings()->IFETestImageSizeWidth;
    IFETestImageSizeHeight  = GetStaticSettings()->IFETestImageSizeHeight;
    m_disableManual3ACCM    = GetStaticSettings()->DisableManual3ACCM;

    /// @todo (CAMX-561) Config the TPG setting properly, right now hardcode
    m_testGenModeData.format                    = PixelFormat::BayerRGGB;
    m_testGenModeData.numPixelsPerLine          = IFETestImageSizeWidth;
    m_testGenModeData.numLinesPerFrame          = IFETestImageSizeHeight;
    m_testGenModeData.resolution.outputWidth    = IFETestImageSizeWidth;
    m_testGenModeData.resolution.outputHeight   = IFETestImageSizeHeight;
    m_testGenModeData.cropInfo.firstPixel       = 0;
    m_testGenModeData.cropInfo.firstLine        = 0;
    m_testGenModeData.cropInfo.lastPixel        = IFETestImageSizeWidth - 1;
    m_testGenModeData.cropInfo.lastLine         = IFETestImageSizeHeight - 1;
    m_testGenModeData.streamConfigCount         = 1;
    m_testGenModeData.streamConfig[0].type      = StreamType::IMAGE;
    m_testGenModeData.outPixelClock             = TPGPIXELCLOCK;
    m_testGenModeData.maxFPS                    = TPGFPS;

    // Add device indices
    result = HwEnvironment::GetInstance()->GetDeviceIndices(CSLDeviceTypeIFE, &deviceIndex, 1, &indicesLengthRequired);

    if (CamxResultSuccess == result)
    {
        CAMX_ASSERT(indicesLengthRequired == 1);
        result = AddDeviceIndex(deviceIndex);
    }

    // If IFE tuning-data enable, initialize debug-data writer
    if ((CamxResultSuccess  == result)                                      &&
        (TRUE               == GetStaticSettings()->enableTuningMetadata)   &&
        (0                  != GetStaticSettings()->tuningDumpDataSizeIFE))
    {
        // We would disable dual IFE when supporting tuning data
        m_pTuningMetadata = static_cast<IFETuningMetadata*>(CAMX_CALLOC(sizeof(IFETuningMetadata)));
        if (NULL == m_pTuningMetadata)
        {
            CAMX_LOG_ERROR(CamxLogGroupISP, "Failed to allocate Tuning metadata.");
            result = CamxResultENoMemory;
        }

        if (CamxResultSuccess == result)
        {
            m_pDebugDataWriter = CAMX_NEW TDDebugDataWriter();
            if (NULL == m_pDebugDataWriter)
            {
                CAMX_LOG_ERROR(CamxLogGroupISP, "Failed to allocate Tuning metadata.");
                result = CamxResultENoMemory;
            }
        }
    }

    if (CamxResultSuccess == result)
    {
        result = CheckForRDIOnly();

        // Check if stats node is connected/disabled
        if ((FALSE == GetPipeline()->HasStatsNode()) || (TRUE == GetStaticSettings()->disableStatsNode))
        {
            m_hasStatsNode = FALSE;
        }
        else
        {
            m_hasStatsNode = TRUE;
        }

        m_OEMStatsConfig = GetStaticSettings()->IsOEMStatSettingEnable;
    }

    if (FALSE == m_RDIOnlyUseCase)
    {
        // Configure IFE Capability
        result = ConfigureIFECapability();
    }

    Utils::Memset(&m_HVXInputData, 0, sizeof(IFEHVXInfo));

    CAMX_LOG_VERBOSE(CamxLogGroupISP, "enableHVXStreaming %d  pHVXAlgoCallbacks %p",
        GetStaticSettings()->enableHVXStreaming, pCreateInputData->pHVXAlgoCallbacks);

    if ((TRUE == GetStaticSettings()->enableHVXStreaming) &&
        (NULL != pCreateInputData->pHVXAlgoCallbacks))
    {
        m_HVXInputData.pHVXAlgoCallbacks = pCreateInputData->pHVXAlgoCallbacks;
        m_HVXInputData.enableHVX        = TRUE;

        result = CreateIFEHVXModules();
    }

    if (CamxResultSuccess == result)
    {
        if (TRUE ==
            HwEnvironment::GetInstance()->IsHWBugWorkaroundEnabled(Titan17xWorkarounds::Titan17xWorkaroundsCDMDMICGCBug))
        {
            // Adding CGC on/off register write requirements
            m_totalIQCmdSizeDWord += PacketBuilder::RequiredWriteInterleavedRegsSizeInDwords(IFECGCNumRegs) * 2;
        }

        // For RDI only use case too we need to allocate cmd buffer for kernel use
        m_totalIQCmdSizeDWord += IFEKMDCmdBufferMaxSize;
    }

    // register to update config done for initial PCR
    pCreateOutputData->createFlags.willNotifyConfigDone = TRUE;

    m_genericBlobCmdBufferSizeBytes =
        (sizeof(IFEResourceHFRConfig) + (sizeof(IFEPortHFRConfig) * (MaxIFEOutputPorts - 1))) +  // HFR configuration
        sizeof(IFEResourceClockConfig) + (sizeof(UINT64) * (RDIMaxNum - 1)) +
        sizeof(IFEResourceBWConfig) + (sizeof(IFEResourceBWVote) * (RDIMaxNum - 1)) +
        sizeof(CSLResourceUBWCConfig) + (sizeof(CSLPortUBWCConfig) * (CSLMaxNumPlanes - 1) * (MaxIFEOutputPorts - 1));

    // The AB bandwidth values only need to be computed for SM7150
    // due to increase in size of the latency buffers in CAMNOC h/w
    if ((TRUE == GetStaticSettings()->enableActiveIFEABVote) &&
        (CSLCameraTitanChipVersion::CSLTitan175V120 ==
                static_cast<Titan17xContext *>(GetHwContext())->GetTitanChipVersion()))
    {
        m_computeActiveABVotes           = TRUE;
        m_genericBlobCmdBufferSizeBytes += (sizeof(IFEResourceBWConfigAB) + (sizeof(UINT64) * (RDIMaxNum - 1)));
        CAMX_LOG_VERBOSE(CamxLogGroupPower, "Compute AB votes explicitly");
    }

    // For RDI only use case too we need to allocate cmd buffer for kernel use
    m_totalIQCmdSizeDWord += IFEKMDCmdBufferMaxSize;

    if (CamxResultSuccess != result)
    {
        if (NULL != m_pTuningMetadata)
        {
            CAMX_FREE(m_pTuningMetadata);
            m_pTuningMetadata = NULL;
        }
        if (NULL != m_pDebugDataWriter)
        {
            CAMX_DELETE m_pDebugDataWriter;
            m_pDebugDataWriter = NULL;
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::ProcessingNodeFinalizeInitialization
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFENode::ProcessingNodeFinalizeInitialization(
    FinalizeInitializationData* pFinalizeInitializationData)
{

    CAMX_UNREFERENCED_PARAM(pFinalizeInitializationData);

    // Save required static metadata
    GetStaticMetadata();

    if ((CSLPresilEnabled == GetCSLMode()) || (CSLPresilRUMIEnabled == GetCSLMode()) || (TRUE == IsTPGMode()))
    {
        m_pSensorModeData = &m_testGenModeData;
        m_pSensorModeRes0Data = &m_testGenModeData;
    }
    else
    {
        GetSensorModeData(&m_pSensorModeData);
        GetSensorModeRes0Data(&m_pSensorModeRes0Data);

        if (NULL != m_pSensorCaps)
        {
            m_pOTPData = &m_pSensorCaps->OTPData;
        }
        else
        {
            CAMX_LOG_ERROR(CamxLogGroupISP, "Sensor static capabilities not available");
        }
    }

    if (NULL != m_pSensorModeData)
    {
        m_IFEinputResolution.horizontalFactor   = 1.0f;
        m_IFEinputResolution.verticalFactor     = 1.0f;
        m_IFEinputResolution.resolution.width   = m_pSensorModeData->cropInfo.lastPixel -
            m_pSensorModeData->cropInfo.firstPixel + 1;
        m_IFEinputResolution.resolution.height  = m_pSensorModeData->cropInfo.lastLine -
            m_pSensorModeData->cropInfo.firstLine + 1;
        m_IFEinputResolution.CAMIFWindow.left   = m_pSensorModeData->cropInfo.firstPixel;
        m_IFEinputResolution.CAMIFWindow.top    = m_pSensorModeData->cropInfo.firstLine;
        m_IFEinputResolution.CAMIFWindow.width  = m_pSensorModeData->cropInfo.lastPixel -
            m_pSensorModeData->cropInfo.firstPixel + 1;
        m_IFEinputResolution.CAMIFWindow.height = m_pSensorModeData->cropInfo.lastLine -
            m_pSensorModeData->cropInfo.firstLine + 1;

        ISPInputData moduleInput;

        moduleInput.HVXData.sensorInput.width = m_pSensorModeData->resolution.outputWidth;
        moduleInput.HVXData.sensorInput.height = m_pSensorModeData->resolution.outputHeight;
        moduleInput.HVXData.format = m_pSensorModeData->format;

        if (TRUE == m_HVXInputData.enableHVX)
        {
            CamxResult result = CamxResultSuccess;

            result = static_cast<IFEHVX*>(m_pIFEHVXModule)->GetHVXInputResolution(&moduleInput);

            if (CamxResultSuccess == result)
            {
                m_HVXInputData.HVXConfiguration = moduleInput.HVXData;
                m_IFEinputResolution.resolution.width = moduleInput.HVXData.HVXOut.width;
                m_IFEinputResolution.resolution.height = moduleInput.HVXData.HVXOut.height;
                m_IFEinputResolution.CAMIFWindow.left = 0;
                m_IFEinputResolution.CAMIFWindow.top = 0;
                m_IFEinputResolution.CAMIFWindow.width = moduleInput.HVXData.HVXOut.width - 1;
                m_IFEinputResolution.CAMIFWindow.height = moduleInput.HVXData.HVXOut.height - 1;
                m_IFEinputResolution.horizontalFactor =
                    static_cast<FLOAT>(moduleInput.HVXData.HVXOut.width) /
                    static_cast<FLOAT>(moduleInput.HVXData.sensorInput.width);
                m_IFEinputResolution.verticalFactor =
                    static_cast<FLOAT>(moduleInput.HVXData.HVXOut.height) /
                    static_cast<FLOAT>(moduleInput.HVXData.sensorInput.height);
            }
            else
            {
                m_HVXInputData.enableHVX = FALSE;
            }

            CAMX_LOG_INFO(CamxLogGroupISP, "[HVX_DBG]: Ds enabled %d",
                m_HVXInputData.HVXConfiguration.DSEnabled);

        }
        PublishIFEInputToUsecasePool(&m_IFEinputResolution);
    }

    m_resourcePolicy = pFinalizeInitializationData->resourcePolicy;

    return CamxResultSuccess;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IFENode::PublishIFEInputToUsecasePool
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IFENode::PublishIFEInputToUsecasePool(
    IFEInputResolution* pIFEResolution)
{
    CAMX_UNREFERENCED_PARAM(pIFEResolution);

    MetadataPool*           pPerUsecasePool = GetPerFramePool(PoolType::PerUsecase);
    MetadataSlot*           pPerUsecaseSlot = pPerUsecasePool->GetSlot(0);
    UsecasePropertyBlob*    pPerUsecaseBlob = NULL;
    CamxResult              result = CamxResultSuccess;

    CAMX_ASSERT(NULL != pIFEResolution);

    const UINT  IFEResolutionTag[] = { PropertyIDUsecaseIFEInputResolution };
    const VOID* pData[1]           = { pIFEResolution };
    UINT        pDataCount[1]      = { sizeof(IFEInputResolution) };

    result = WriteDataList(IFEResolutionTag, pData, pDataCount, 1);

    if (CamxResultSuccess != result)
    {
        CAMX_LOG_ERROR(CamxLogGroupFD, "Falied to publish IFE Downscale ratio uscasepool");
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::InitializeOutputPathImageInfo
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IFENode::InitializeOutputPathImageInfo()
{
    UINT32 totalOutputPorts = 0;
    UINT32 outputPortId[MaxIFEOutputPorts];

    const ImageFormat*  pImageFormat = NULL;

    /// @todo (CAMX-1015) Get this only once in ProcessingNodeInitialize() and save it off in the IFENode class
    GetAllOutputPortIds(&totalOutputPorts, outputPortId);

    /// @todo (CAMX-1221) Change read only type of ISPInput data structure to pointer to avoid memcpy
    for (UINT outputPortIndex = 0; outputPortIndex < totalOutputPorts; outputPortIndex++)
    {
        pImageFormat = GetOutputPortImageFormat(OutputPortIndex(outputPortId[outputPortIndex]));

        CAMX_ASSERT_MESSAGE((NULL != pImageFormat), "Output port is not defined.");

        switch (outputPortId[outputPortIndex])
        {
            case IFEOutputPortFull:
                m_ISPInputHALData.stream[FullOutput].width  = pImageFormat->width;
                m_ISPInputHALData.stream[FullOutput].height = pImageFormat->height;
                m_ISPInputHALData.format[FullOutput]        = pImageFormat->format;
                break;

            case IFEOutputPortFD:
                m_ISPInputHALData.stream[FDOutput].width    = pImageFormat->width;
                m_ISPInputHALData.stream[FDOutput].height   = pImageFormat->height;
                m_ISPInputHALData.format[FDOutput]          = pImageFormat->format;

                break;

            case IFEOutputPortDS4:
                m_ISPInputHALData.stream[DS4Output].width   = pImageFormat->width;
                m_ISPInputHALData.stream[DS4Output].height  = pImageFormat->height;
                m_ISPInputHALData.format[DS4Output]         = pImageFormat->format;

                break;

            case IFEOutputPortDS16:
                m_ISPInputHALData.stream[DS16Output].width  = pImageFormat->width;
                m_ISPInputHALData.stream[DS16Output].height = pImageFormat->height;
                m_ISPInputHALData.format[DS16Output]        = pImageFormat->format;

                break;

            case IFEOutputPortCAMIFRaw:
            case IFEOutputPortLSCRaw:
            case IFEOutputPortGTMRaw:
                m_ISPInputHALData.stream[PixelRawOutput].width  = pImageFormat->width;
                m_ISPInputHALData.stream[PixelRawOutput].height = pImageFormat->height;
                m_ISPInputHALData.format[PixelRawOutput]        = pImageFormat->format;
                break;

            case IFEOutputPortDisplayFull:
                m_ISPInputHALData.stream[DisplayFullOutput].width  = pImageFormat->width;
                m_ISPInputHALData.stream[DisplayFullOutput].height = pImageFormat->height;
                m_ISPInputHALData.format[DisplayFullOutput]        = pImageFormat->format;
                break;

            case IFEOutputPortDisplayDS4:
                m_ISPInputHALData.stream[DisplayDS4Output].width   = pImageFormat->width;
                m_ISPInputHALData.stream[DisplayDS4Output].height  = pImageFormat->height;
                m_ISPInputHALData.format[DisplayDS4Output]         = pImageFormat->format;
                break;

            case IFEOutputPortDisplayDS16:
                m_ISPInputHALData.stream[DisplayDS16Output].width  = pImageFormat->width;
                m_ISPInputHALData.stream[DisplayDS16Output].height = pImageFormat->height;
                m_ISPInputHALData.format[DisplayDS16Output]        = pImageFormat->format;
                break;

            default:
                // For non-image output format, such as stats output, ignore and the configuration.
                break;
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::HardcodeSettings
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void IFENode::HardcodeSettings(
    ISPInputData*     pModuleInput,
    ISPStripeConfig*  pStripeConfig,
    BOOL              initalConfig)
{

    Utils::Memcpy(&pModuleInput->HALData, &m_ISPInputHALData, sizeof(ISPHALConfigureData));
    Utils::Memcpy(&pModuleInput->sensorData, &m_ISPInputSensorData, sizeof(ISPSensorConfigureData));

    // Assume single IFE and set the first (only) stripe's crop to the full crop. This will be overwritten if in dual mode.
    pStripeConfig->CAMIFCrop = pModuleInput->sensorData.CAMIFCrop;

    // CSID crop override: Update CAMIFCrop.
    if (TRUE == EnableCSIDCropOverridingForSingleIFE())
    {
        pStripeConfig->CAMIFCrop.firstPixel = m_instanceProperty.IFECSIDLeft;
        pStripeConfig->CAMIFCrop.lastPixel  = m_instanceProperty.IFECSIDLeft + m_instanceProperty.IFECSIDWidth - 1;
        pStripeConfig->CAMIFCrop.firstLine  = m_instanceProperty.IFECSIDTop;
        pStripeConfig->CAMIFCrop.lastLine   = m_instanceProperty.IFECSIDTop + m_instanceProperty.IFECSIDHeight - 1;
    }

    pStripeConfig->CAMIFSubsampleInfo.enableCAMIFSubsample                   = m_PDAFInfo.enableSubsample;
    pStripeConfig->CAMIFSubsampleInfo.CAMIFSubSamplePattern.pixelSkipPattern = m_PDAFInfo.pixelSkipPattern;
    pStripeConfig->CAMIFSubsampleInfo.CAMIFSubSamplePattern.lineSkipPattern  = m_PDAFInfo.lineSkipPattern;

    if (TRUE == initalConfig)
    {
        if (((pStripeConfig->CAMIFCrop.lastPixel - pStripeConfig->CAMIFCrop.firstPixel + 1) <
            static_cast<UINT32>(pModuleInput->pHALTagsData->HALCrop.left + pModuleInput->pHALTagsData->HALCrop.width)) ||
            ((pStripeConfig->CAMIFCrop.lastLine - pStripeConfig->CAMIFCrop.firstLine + 1) <
            static_cast<UINT32>(pModuleInput->pHALTagsData->HALCrop.top + pModuleInput->pHALTagsData->HALCrop.height)))
        {
            CAMX_LOG_INFO(CamxLogGroupISP, "Overrwriting CROP Window");
            pModuleInput->pHALTagsData->HALCrop.left   = pStripeConfig->CAMIFCrop.firstPixel;
            pModuleInput->pHALTagsData->HALCrop.width  =
                pStripeConfig->CAMIFCrop.lastPixel - pStripeConfig->CAMIFCrop.firstPixel + 1;
            pModuleInput->pHALTagsData->HALCrop.top    = pStripeConfig->CAMIFCrop.firstLine;
            pModuleInput->pHALTagsData->HALCrop.height =
                pStripeConfig->CAMIFCrop.lastLine - pStripeConfig->CAMIFCrop.firstLine + 1;
        }
    }

    pStripeConfig->HALCrop[FDOutput]          = pModuleInput->pHALTagsData->HALCrop;
    pStripeConfig->HALCrop[FullOutput]        = pModuleInput->pHALTagsData->HALCrop;
    pStripeConfig->HALCrop[DS4Output]         = pModuleInput->pHALTagsData->HALCrop;
    pStripeConfig->HALCrop[DS16Output]        = pModuleInput->pHALTagsData->HALCrop;
    pStripeConfig->HALCrop[PixelRawOutput]    = pModuleInput->pHALTagsData->HALCrop;
    pStripeConfig->HALCrop[DisplayFullOutput] = pModuleInput->pHALTagsData->HALCrop;
    pStripeConfig->HALCrop[DisplayDS4Output]  = pModuleInput->pHALTagsData->HALCrop;
    pStripeConfig->HALCrop[DisplayDS16Output] = pModuleInput->pHALTagsData->HALCrop;

    pStripeConfig->stream[FDOutput].width           = pModuleInput->HALData.stream[FDOutput].width;
    pStripeConfig->stream[FDOutput].height          = pModuleInput->HALData.stream[FDOutput].height;
    pStripeConfig->stream[FullOutput].width         = pModuleInput->HALData.stream[FullOutput].width;
    pStripeConfig->stream[FullOutput].height        = pModuleInput->HALData.stream[FullOutput].height;
    pStripeConfig->stream[DS4Output].width          = pModuleInput->HALData.stream[DS4Output].width;
    pStripeConfig->stream[DS4Output].height         = pModuleInput->HALData.stream[DS4Output].height;
    pStripeConfig->stream[DS16Output].width         = pModuleInput->HALData.stream[DS16Output].width;
    pStripeConfig->stream[DS16Output].height        = pModuleInput->HALData.stream[DS16Output].height;
    pStripeConfig->stream[PixelRawOutput].width     = pModuleInput->HALData.stream[PixelRawOutput].width;
    pStripeConfig->stream[PixelRawOutput].height    = pModuleInput->HALData.stream[PixelRawOutput].height;
    pStripeConfig->stream[PDAFRawOutput].width      = m_PDAFInfo.bufferWidth;
    pStripeConfig->stream[PDAFRawOutput].height     = m_PDAFInfo.bufferHeight;
    pStripeConfig->stream[DisplayFullOutput].width  = pModuleInput->HALData.stream[DisplayFullOutput].width;
    pStripeConfig->stream[DisplayFullOutput].height = pModuleInput->HALData.stream[DisplayFullOutput].height;
    pStripeConfig->stream[DisplayDS4Output].width   = pModuleInput->HALData.stream[DisplayDS4Output].width;
    pStripeConfig->stream[DisplayDS4Output].height  = pModuleInput->HALData.stream[DisplayDS4Output].height;
    pStripeConfig->stream[DisplayDS16Output].width  = pModuleInput->HALData.stream[DisplayDS16Output].width;
    pStripeConfig->stream[DisplayDS16Output].height = pModuleInput->HALData.stream[DisplayDS16Output].height;

    UINT32 inputWidth  = m_ISPInputSensorData.CAMIFCrop.lastPixel - m_ISPInputSensorData.CAMIFCrop.firstPixel + 1;
    UINT32 inputHeight = m_ISPInputSensorData.CAMIFCrop.lastLine - m_ISPInputSensorData.CAMIFCrop.firstLine + 1;

    if (TRUE == m_HVXInputData.HVXConfiguration.DSEnabled)
    {
        inputWidth  = m_HVXInputData.HVXConfiguration.HVXOut.width;
        inputHeight = m_HVXInputData.HVXConfiguration.HVXOut.height;
        m_HVXInputData.HVXConfiguration.HVXCrop.firstLine = 0;
        m_HVXInputData.HVXConfiguration.HVXCrop.firstPixel = 0;
        m_HVXInputData.HVXConfiguration.HVXCrop.lastPixel = inputWidth - 1;
        m_HVXInputData.HVXConfiguration.HVXCrop.lastLine = inputHeight - 1;
        m_HVXInputData.HVXConfiguration.origCAMIFCrop = pStripeConfig->CAMIFCrop;
        m_HVXInputData.HVXConfiguration.origHALWindow = pModuleInput->pHALTagsData->HALCrop;

        pStripeConfig->CAMIFCrop = m_HVXInputData.HVXConfiguration.HVXCrop;
        pStripeConfig->HALCrop[FullOutput].left   = 0;
        pStripeConfig->HALCrop[FullOutput].top    = 0;
        pStripeConfig->HALCrop[FullOutput].width  = inputWidth;
        pStripeConfig->HALCrop[FullOutput].height = inputHeight;

        pStripeConfig->HALCrop[FDOutput]   = pStripeConfig->HALCrop[FullOutput];
        pStripeConfig->HALCrop[DS4Output]  = pStripeConfig->HALCrop[FullOutput];
        pStripeConfig->HALCrop[DS16Output] = pStripeConfig->HALCrop[FullOutput];

        pStripeConfig->HALCrop[DisplayFullOutput] = pStripeConfig->HALCrop[FullOutput];
        pStripeConfig->HALCrop[DisplayDS4Output]  = pStripeConfig->HALCrop[FullOutput];
        pStripeConfig->HALCrop[DisplayDS16Output] = pStripeConfig->HALCrop[FullOutput];
    }

    /// @todo (CAMX-3276) Hard code YUV Sensor Crop configure for now.
    if (TRUE == pModuleInput->sensorData.isYUV)
    {
        inputWidth >>= 1;
    }

    // Currently Internal 3A publishes AEC, AWB and AF update, read it from propery pool
    if ((TRUE == m_enableHardcodedConfig) || (TRUE == initalConfig))
    {
        BGBEConfig* pBEConfig = &pModuleInput->pAECStatsUpdateData->statsConfig.BEConfig;

        pBEConfig->channelGainThreshold[ChannelIndexR]  = (1 << IFEPipelineBitWidth) - 1;
        pBEConfig->channelGainThreshold[ChannelIndexGR] = (1 << IFEPipelineBitWidth) - 1;
        pBEConfig->channelGainThreshold[ChannelIndexB]  = (1 << IFEPipelineBitWidth) - 1;
        pBEConfig->channelGainThreshold[ChannelIndexGB] = (1 << IFEPipelineBitWidth) - 1;
        pBEConfig->horizontalNum                        = 64;
        pBEConfig->verticalNum                          = 48;
        pBEConfig->ROI.left                             = 0;
        pBEConfig->ROI.top                              = 0;
        pBEConfig->ROI.width                            = inputWidth - (inputWidth / 10);
        pBEConfig->ROI.height                           = inputHeight - (inputHeight / 10);
        pBEConfig->outputBitDepth                       = 0;
        pBEConfig->outputMode                           = BGBERegular;

        // For AWB BG, store the configuration in pAWBStatsUpdateData.
        BGBEConfig* pBGConfig = &pModuleInput->pAWBStatsUpdateData->statsConfig.BGConfig;

        pBGConfig->channelGainThreshold[ChannelIndexR]  = (1 << IFEPipelineBitWidth) - 1;
        pBGConfig->channelGainThreshold[ChannelIndexGR] = (1 << IFEPipelineBitWidth) - 1;
        pBGConfig->channelGainThreshold[ChannelIndexB]  = (1 << IFEPipelineBitWidth) - 1;
        pBGConfig->channelGainThreshold[ChannelIndexGB] = (1 << IFEPipelineBitWidth) - 1;
        pBGConfig->horizontalNum                        = 64;
        pBGConfig->verticalNum                          = 48;
        pBGConfig->ROI.left                             = 0;
        pBGConfig->ROI.top                              = 0;
        pBGConfig->ROI.width                            = inputWidth - (inputWidth / 10);
        pBGConfig->ROI.height                           = inputHeight - (inputHeight / 10);
        pBGConfig->outputBitDepth                       = 0;
        pBGConfig->outputMode                           = BGBERegular;

        pModuleInput->pAECUpdateData->luxIndex                      = 220.0f;
        pModuleInput->pAECUpdateData->predictiveGain                = 1.0f;
        for (UINT i = 0; i < ExposureIndexCount; i++)
        {
            pModuleInput->pAECUpdateData->exposureInfo[i].exposureTime  = 1;
            pModuleInput->pAECUpdateData->exposureInfo[i].linearGain    = 1.0f;
            pModuleInput->pAECUpdateData->exposureInfo[i].sensitivity   = 1.0f;
        }

        pModuleInput->pAWBUpdateData->colorTemperature = 0;
        pModuleInput->pAWBUpdateData->AWBGains.gGain   = 1.0f;
        pModuleInput->pAWBUpdateData->AWBGains.bGain   = 1.493855f;
        pModuleInput->pAWBUpdateData->AWBGains.rGain   = 2.043310f;

        pModuleInput->pAFUpdateData->exposureCompensationEnable = 0;

        HardcodeSettingsBFStats(pStripeConfig);

        // RS stats configuration
        pModuleInput->pAFDStatsUpdateData->statsConfig.statsHNum                        = RSStats14MaxHorizRegions;
        pModuleInput->pAFDStatsUpdateData->statsConfig.statsVNum                        = RSStats14MaxVertRegions;
        pModuleInput->pAFDStatsUpdateData->statsConfig.statsRSCSColorConversionEnable   = TRUE;

        BHistConfig* pBHistConfig = &pModuleInput->pAECStatsUpdateData->statsConfig.BHistConfig;

        pBHistConfig->ROI.top     = 0;
        pBHistConfig->ROI.left    = 0;
        pBHistConfig->ROI.width   = inputWidth - (inputWidth / 10);
        pBHistConfig->ROI.height  = inputHeight - (inputHeight / 10);
        pBHistConfig->channel     = ColorChannel::ColorChannelY;
        pBHistConfig->uniform     = TRUE;

        // HDR Bhist config
        HDRBHistConfig* pHDRBHistConfig = &pModuleInput->pAECStatsUpdateData->statsConfig.HDRBHistConfig;

        pHDRBHistConfig->ROI.top = 0;
        pHDRBHistConfig->ROI.left = 0;
        pHDRBHistConfig->ROI.width  = inputWidth;
        pHDRBHistConfig->ROI.height = inputHeight;
        pHDRBHistConfig->greenChannelInput = HDRBHistSelectGB;
        pHDRBHistConfig->inputFieldSelect = HDRBHistInputAll;
    }

    if ((FALSE == m_OEMStatsConfig) || (TRUE == m_enableHardcodedConfig) || (TRUE == initalConfig))
    {
        // Currently Internal 3A does not publishes Tintless BG, IHist & CS stats, cofing default values.
        // OEM not using stats node must configure these stats.
        IHistStatsConfig* pIHistConfig = &pModuleInput->pIHistStatsUpdateData->statsConfig;

        pIHistConfig->ROI.top           = 0;
        pIHistConfig->ROI.left          = 0;
        pIHistConfig->ROI.width         = inputWidth - (inputWidth / 10);
        pIHistConfig->ROI.height        = inputHeight - (inputHeight / 10);
        pIHistConfig->channelYCC        = IHistYCCChannelY;
        pIHistConfig->maxPixelSumPerBin = 0;

        // CS stats configuration
        pModuleInput->pCSStatsUpdateData->statsConfig.statsHNum = CSStats14MaxHorizRegions;
        pModuleInput->pCSStatsUpdateData->statsConfig.statsVNum = CSStats14MaxVertRegions;

        // Tintless BG config
        // For Tintless BG, store the configuration in pAECStatsUpdateData.
        BGBEConfig* pBGConfig = &pModuleInput->pAECStatsUpdateData->statsConfig.TintlessBGConfig;

        pBGConfig->channelGainThreshold[ChannelIndexR]  = (1 << IFEPipelineBitWidth) - 1;
        pBGConfig->channelGainThreshold[ChannelIndexGR] = (1 << IFEPipelineBitWidth) - 1;
        pBGConfig->channelGainThreshold[ChannelIndexB]  = (1 << IFEPipelineBitWidth) - 1;
        pBGConfig->channelGainThreshold[ChannelIndexGB] = (1 << IFEPipelineBitWidth) - 1;

        pBGConfig->horizontalNum                        = 32;
        pBGConfig->verticalNum                          = 24;
        pBGConfig->ROI.left                             = 0;
        pBGConfig->ROI.top                              = 0;
        pBGConfig->ROI.width                            = inputWidth;
        pBGConfig->ROI.height                           = inputHeight;
        pBGConfig->outputBitDepth                       = 14;
        pBGConfig->outputMode                           = BGBERegular;
    }
    // restore bankSelect
    pStripeConfig->stateLSC.dependenceData.bankSelect = m_frameConfig.stateLSC.dependenceData.bankSelect;

    // restore Chromatix
    pStripeConfig->stateLSC.dependenceData.pChromatix = m_frameConfig.stateLSC.dependenceData.pChromatix;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::HardcodeTintlessSettings
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void IFENode::HardcodeTintlessSettings(
    ISPInputData*     pModuleInput)
{
    BGBEConfig* pTintlessBGConfig = &pModuleInput->pAECStatsUpdateData->statsConfig.TintlessBGConfig;
    UINT32 CAMIFWidth  = m_ISPInputSensorData.CAMIFCrop.lastPixel - m_ISPInputSensorData.CAMIFCrop.firstPixel + 1;
    UINT32 CAMIFHeight = m_ISPInputSensorData.CAMIFCrop.lastLine - m_ISPInputSensorData.CAMIFCrop.firstLine + 1;

    /// @todo (CAMX-3276) Hard code YUV Sensor Crop configure for now.
    if (TRUE == pModuleInput->sensorData.isYUV)
    {
        CAMIFWidth >>= 1;
    }

    pTintlessBGConfig->channelGainThreshold[ChannelIndexR]  = (1 << IFEPipelineBitWidth) - 1;
    pTintlessBGConfig->channelGainThreshold[ChannelIndexGR] = (1 << IFEPipelineBitWidth) - 1;
    pTintlessBGConfig->channelGainThreshold[ChannelIndexB]  = (1 << IFEPipelineBitWidth) - 1;
    pTintlessBGConfig->channelGainThreshold[ChannelIndexGB] = (1 << IFEPipelineBitWidth) - 1;
    pTintlessBGConfig->horizontalNum  = 32;
    pTintlessBGConfig->verticalNum    = 24;
    pTintlessBGConfig->ROI.left       = 0;
    pTintlessBGConfig->ROI.top        = 0;
    pTintlessBGConfig->ROI.width      = CAMIFWidth;
    pTintlessBGConfig->ROI.height     = CAMIFHeight;
    pTintlessBGConfig->outputBitDepth = 14;
    pTintlessBGConfig->outputMode     = BGBERegular;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::DynamicCAMIFCrop
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void IFENode::DynamicCAMIFCrop(
    ISPStripeConfig*  pStripeConfig)
{
    if (TRUE == EnableCSIDCropOverridingForSingleIFE())
    {
        CropWindow crop;
        crop.left   = m_instanceProperty.IFECSIDLeft;
        crop.top    = m_instanceProperty.IFECSIDTop;
        crop.width  = m_instanceProperty.IFECSIDWidth;
        crop.height = m_instanceProperty.IFECSIDHeight;

        pStripeConfig->HALCrop[FDOutput]          = crop;
        pStripeConfig->HALCrop[FullOutput]        = crop;
        pStripeConfig->HALCrop[DS4Output]         = crop;
        pStripeConfig->HALCrop[DS16Output]        = crop;
        pStripeConfig->HALCrop[PixelRawOutput]    = crop;
        pStripeConfig->HALCrop[DisplayFullOutput] = crop;
        pStripeConfig->HALCrop[DisplayDS4Output]  = crop;
        pStripeConfig->HALCrop[DisplayDS16Output] = crop;

        pStripeConfig->CAMIFCrop.firstPixel = crop.left;
        pStripeConfig->CAMIFCrop.lastPixel  = crop.width + pStripeConfig->CAMIFCrop.firstPixel - 1;
        pStripeConfig->CAMIFCrop.firstLine  = crop.top;
        pStripeConfig->CAMIFCrop.lastLine   = crop.height + pStripeConfig->CAMIFCrop.firstLine - 1;

        CAMX_LOG_INFO(CamxLogGroupISP, "Dynamic Crop Info being used width=%d height=%d left=%d top=%d",
            crop.width,
            crop.height,
            crop.left,
            crop.top);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::CreateCmdBuffers
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFENode::CreateCmdBuffers()
{
    CamxResult result = CamxResultSuccess;

    // Initializate Command Buffer
    result = InitializeCmdBufferManagerList(static_cast<UINT32>(IFECmdBufferId::NumCmdBuffers));

    if (CamxResultSuccess == result)
    {
        ResourceParams resourceIQParams = { 0 };

        resourceIQParams.usageFlags.packet                  = 1;
        // 3 Command Buffers for GenericBlob, Common (and possibly Left/Right IFEs in dual IFE mode)
        resourceIQParams.packetParams.maxNumCmdBuffers      = (IFEModuleMode::DualIFENormal == m_mode) ? 5 : 2;
        // 4 Pixel output and 3 RDI output
        resourceIQParams.packetParams.maxNumIOConfigs       = 24;
        // The max below depends on the number of embeddings of DMI/indirect buffers in the packet. Ideally this
        // value should be calculated based on the design in this node. But an upper bound is fine too.
        resourceIQParams.packetParams.maxNumPatches         = 24;
        resourceIQParams.packetParams.enableAddrPatching    = 1;
        resourceIQParams.resourceSize                       = Packet::CalculatePacketSize(&resourceIQParams.packetParams);

        // Same number as cmd buffers
        resourceIQParams.poolSize                           = m_IFECmdBlobCount * resourceIQParams.resourceSize;
        resourceIQParams.alignment                          = CamxPacketAlignmentInBytes;
        resourceIQParams.pDeviceIndices                     = NULL;
        resourceIQParams.numDevices                         = 0;
        resourceIQParams.memFlags                           = CSLMemFlagKMDAccess | CSLMemFlagUMDAccess;

        result = CreateCmdBufferManager("IQPacketManager", &resourceIQParams, &m_pIQPacketManager);

        if (CamxResultSuccess == result)
        {
            ResourceParams params = { 0 };

            // Need to reserve a buffer in the Cmd Buffer for the KMD to do the patching
            params.resourceSize                     = m_totalIQCmdSizeDWord * sizeof(UINT32);
            params.poolSize                         = m_IFECmdBlobCount * params.resourceSize;
            params.usageFlags.cmdBuffer             = 1;
            params.cmdParams.type                   = CmdType::CDMDirect;
            params.alignment                        = CamxCommandBufferAlignmentInBytes;
            params.cmdParams.enableAddrPatching     = 1;
            // The max below depends on the number of embeddings of DMI/indirect buffers in the command buffer.
            // Ideally this value should be calculated based on the design in this node. But an upper bound is fine too.
            params.cmdParams.maxNumNestedAddrs      = 24;
            /// @todo (CAMX-561) Put in IFE device index (when acquire is implemented)
            params.pDeviceIndices                   = DeviceIndices();
            params.numDevices                       = DeviceIndexCount();
            params.memFlags                         = CSLMemFlagUMDAccess;

            result = CreateCmdBufferManager("IQMainCmdBufferManager", &params, &m_pIQMainCmdBufferManager);

            // Create Generic Blob Buffer
            if ((CamxResultSuccess == result) && (m_genericBlobCmdBufferSizeBytes > 0))
            {
                params = { 0 };

                params.resourceSize                     = m_genericBlobCmdBufferSizeBytes;
                params.poolSize                         = m_IFECmdBlobCount * params.resourceSize;
                params.usageFlags.cmdBuffer             = 1;
                params.cmdParams.type                   = CmdType::Generic;
                params.alignment                        = CamxCommandBufferAlignmentInBytes;
                params.cmdParams.enableAddrPatching     = 0;
                params.memFlags                         = CSLMemFlagKMDAccess | CSLMemFlagUMDAccess;
                params.pDeviceIndices                   = NULL;
                params.numDevices                       = 0;

                result = CreateCmdBufferManager("GenericBlobBufferManager", &params, &m_pGenericBlobBufferManager);
            }

            // Create 64 bit DMI Buffer
            if ((CamxResultSuccess == result) && (m_total64bitDMISizeDWord > 0))
            {
                params = { 0 };

                params.resourceSize                     = m_total64bitDMISizeDWord * sizeof(UINT32);
                params.poolSize                         = m_IFECmdBlobCount * params.resourceSize;
                params.usageFlags.cmdBuffer             = 1;
                params.cmdParams.type                   = CmdType::CDMDMI64;
                params.alignment                        = CamxCommandBufferAlignmentInBytes;
                params.cmdParams.enableAddrPatching     = 0;
                params.memFlags                         = CSLMemFlagUMDAccess;
                params.pDeviceIndices                   = DeviceIndices();
                params.numDevices                       = DeviceIndexCount();

                result = CreateCmdBufferManager("64bitDMIBufferManager", &params, &m_p64bitDMIBufferManager);
            }

            if ((CamxResultSuccess == result) && (m_total32bitDMISizeDWord > 0))
            {
                params = { 0 };

                params.resourceSize                     = m_total32bitDMISizeDWord * sizeof(UINT32);
                params.poolSize                         = m_IFECmdBlobCount * params.resourceSize;
                params.usageFlags.cmdBuffer             = 1;
                params.cmdParams.type                   = CmdType::CDMDMI32;
                params.alignment                        = CamxCommandBufferAlignmentInBytes;
                params.cmdParams.enableAddrPatching     = 0;
                params.memFlags                         = CSLMemFlagUMDAccess;
                params.pDeviceIndices                   = DeviceIndices();
                params.numDevices                       = DeviceIndexCount();

                result = CreateCmdBufferManager("32bitDMIBufferManager", &params, &m_p32bitDMIBufferManager);
            }

            if ((CamxResultSuccess == result) && (IFEModuleMode::DualIFENormal == m_mode))
            {
                params = { 0 };

                // m_totalIQCmdSizeDWord is probably too much but pretty safe.
                params.resourceSize                     = m_totalIQCmdSizeDWord * sizeof(UINT32);
                params.poolSize                         = m_IFECmdBlobCount * params.resourceSize;
                params.usageFlags.cmdBuffer             = 1;
                params.cmdParams.type                   = CmdType::CDMDirect;
                params.alignment                        = CamxCommandBufferAlignmentInBytes;
                params.cmdParams.enableAddrPatching     = 1;
                // The max below depends on the number of embeddings of DMI/indirect buffers in the command buffer.
                // Ideally this value should be calculated based on the design in this node. But an upper bound is fine too.
                params.cmdParams.maxNumNestedAddrs      = 24;
                params.pDeviceIndices                   = DeviceIndices();
                params.numDevices                       = DeviceIndexCount();
                params.memFlags                         = CSLMemFlagUMDAccess;

                result = CreateCmdBufferManager("IQLeftCmdBufferManager", &params, &m_pIQLeftCmdBufferManager);
                if (CamxResultSuccess == result)
                {
                    result = CreateCmdBufferManager("IQRightCmdBufferManager", &params, &m_pIQRightCmdBufferManager);
                }

                if (CamxResultSuccess == result)
                {
                    params.resourceSize                     = DualIFEUtils::GetMaxStripeConfigSize(m_maxNumOfCSLIFEPortId);
                    params.poolSize                         = m_IFECmdBlobCount * params.resourceSize;
                    params.usageFlags.cmdBuffer             = 1;
                    params.cmdParams.type                   = CmdType::Generic;
                    params.alignment                        = CamxCommandBufferAlignmentInBytes;
                    params.cmdParams.enableAddrPatching     = 0;
                    params.pDeviceIndices                   = NULL;
                    params.numDevices                       = 0;
                    params.memFlags                         = CSLMemFlagKMDAccess | CSLMemFlagUMDAccess;

                    result = CreateCmdBufferManager("DualIFEConfigCmdBufferManager", &params,
                                                    &m_pDualIFEConfigCmdBufferManager);
                }
            }
        }
    }

    if (CamxResultSuccess != result)
    {
        CAMX_ASSERT_ALWAYS_MESSAGE("Failed to Create Cmd Buffer Manager or Command Buffer");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::FetchCmdBuffers
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFENode::FetchCmdBuffers(
    UINT64  requestId)
{
    CamxResult result = CamxResultSuccess;

    CAMX_ASSERT((NULL != m_pIQPacketManager)                                              &&
                ((m_totalIQCmdSizeDWord <= 0)           || (NULL != m_pIQMainCmdBufferManager)) &&
                ((m_total64bitDMISizeDWord <= 0)        || (NULL != m_p64bitDMIBufferManager))  &&
                ((m_total32bitDMISizeDWord <= 0)        || (NULL != m_p32bitDMIBufferManager))  &&
                ((m_genericBlobCmdBufferSizeBytes <= 0) || (NULL != m_pGenericBlobBufferManager)));

    // Reset all the previous command buffers
    m_pIQPacket                 = NULL;
    m_pCommonCmdBuffer          = NULL;
    m_p64bitDMIBuffer           = NULL;
    m_p32bitDMIBuffer           = NULL;
    m_pLeftCmdBuffer            = NULL;
    m_pRightCmdBuffer           = NULL;
    m_pDualIFEConfigCmdBuffer   = NULL;
    m_pGenericBlobCmdBuffer     = NULL;

    m_pIQPacket = GetPacketForRequest(requestId, m_pIQPacketManager);
    CAMX_ASSERT(NULL != m_pIQPacket);

    if (m_totalIQCmdSizeDWord > 0)
    {
        m_pCommonCmdBuffer = GetCmdBufferForRequest(requestId, m_pIQMainCmdBufferManager);
        CAMX_ASSERT(NULL != m_pCommonCmdBuffer);
    }

    if (m_genericBlobCmdBufferSizeBytes > 0)
    {
        m_pGenericBlobCmdBuffer = GetCmdBufferForRequest(requestId, m_pGenericBlobBufferManager);
        CAMX_ASSERT(NULL != m_pGenericBlobCmdBuffer);
    }

    if (m_total64bitDMISizeDWord > 0)
    {
        m_p64bitDMIBuffer = GetCmdBufferForRequest(requestId, m_p64bitDMIBufferManager);
        if (NULL != m_p64bitDMIBuffer)
        {
            // Reserve the whole DMI Buffer for this frame
            m_p64bitDMIBufferAddr = static_cast<UINT32*>(m_p64bitDMIBuffer->BeginCommands(m_total64bitDMISizeDWord));
        }
    }

    if (m_total32bitDMISizeDWord > 0)
    {
        m_p32bitDMIBuffer = GetCmdBufferForRequest(requestId, m_p32bitDMIBufferManager);
        if (NULL != m_p32bitDMIBuffer)
        {
            // Reserve the whole DMI Buffer for this frame
            m_p32bitDMIBufferAddr = static_cast<UINT32*>(m_p32bitDMIBuffer->BeginCommands(m_total32bitDMISizeDWord));
        }
    }

    // Error Check
    if ((NULL == m_pIQPacket)                                                ||
        ((m_totalIQCmdSizeDWord    > 0)         && (NULL == m_pCommonCmdBuffer))    ||
        ((m_total64bitDMISizeDWord > 0)         && (NULL == m_p64bitDMIBufferAddr)) ||
        ((m_total32bitDMISizeDWord > 0)         && (NULL == m_p32bitDMIBufferAddr)) ||
        ((m_genericBlobCmdBufferSizeBytes > 0)  && (NULL == m_pGenericBlobCmdBuffer)))
    {
        CAMX_LOG_ERROR(CamxLogGroupISP, "Not able to fetch Cmd buffer for  requestId %llu", requestId);
        result = CamxResultENoMemory;
    }

    if ((CamxResultSuccess            == result)            &&
        (IFEModuleMode::DualIFENormal == m_mode)            &&
        (FALSE                        == m_RDIOnlyUseCase))
    {
        CAMX_ASSERT((NULL != m_pIQLeftCmdBufferManager) && (NULL != m_pIQRightCmdBufferManager));

        m_pLeftCmdBuffer          = GetCmdBufferForRequest(requestId, m_pIQLeftCmdBufferManager);
        m_pRightCmdBuffer         = GetCmdBufferForRequest(requestId, m_pIQRightCmdBufferManager);
        m_pDualIFEConfigCmdBuffer = GetCmdBufferForRequest(requestId, m_pDualIFEConfigCmdBufferManager);

        if ((NULL == m_pLeftCmdBuffer)          ||
            (NULL == m_pRightCmdBuffer)         ||
            (NULL == m_pDualIFEConfigCmdBuffer))
        {
            CAMX_LOG_ERROR(CamxLogGroupISP, "Not able to fetch Dual IFE Cmd buffer for  requestId %llu", requestId);
            result = CamxResultENoMemory;
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::AddCmdBufferReference
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFENode::AddCmdBufferReference(
    UINT64              requestId,
    CSLPacketOpcodesIFE opcode)
{
    CamxResult result         = CamxResultSuccess;
    UINT32     cmdBufferIndex = 0;

    m_pIQPacket->SetRequestId(GetCSLSyncId(requestId));

    /// @todo (CAMX-656) Need to define the Metadata of the packet
    m_pCommonCmdBuffer->SetMetadata(static_cast<UINT32>(CSLIFECmdBufferIdCommon));
    result = m_pIQPacket->AddCmdBufferReference(m_pCommonCmdBuffer, &cmdBufferIndex);

    // We may not need Generic Blob Buffer always, only insert if we have some information in it
    if ((NULL != m_pGenericBlobCmdBuffer) && (m_pGenericBlobCmdBuffer->GetResourceUsedDwords() > 0))
    {
        m_pGenericBlobCmdBuffer->SetMetadata(static_cast<UINT32>(CSLIFECmdBufferIdGenericBlob));
        result = m_pIQPacket->AddCmdBufferReference(m_pGenericBlobCmdBuffer, NULL);
    }

    if ((CamxResultSuccess == result) && (IFEModuleMode::DualIFENormal == m_mode))
    {
        m_pIQPacket->SetOpcode(CSLDeviceType::CSLDeviceTypeIFE, opcode);

        m_pRightCmdBuffer->SetMetadata(static_cast<UINT32>(CSLIFECmdBufferIdRight));
        result = m_pIQPacket->AddCmdBufferReference(m_pRightCmdBuffer, NULL);

        if (CamxResultSuccess == result)
        {
            m_pLeftCmdBuffer->SetMetadata(static_cast<UINT32>(CSLIFECmdBufferIdLeft));
            result = m_pIQPacket->AddCmdBufferReference(m_pLeftCmdBuffer, NULL);
        }

        if (CamxResultSuccess == result)
        {
            m_pDualIFEConfigCmdBuffer->SetMetadata(static_cast<UINT32>(CSLIFECmdBufferIdDualConfig));
            result = m_pIQPacket->AddCmdBufferReference(m_pDualIFEConfigCmdBuffer, NULL);
        }

    }
    else if (CamxResultSuccess == result)
    {
        m_pIQPacket->SetOpcode(CSLDeviceType::CSLDeviceTypeIFE, opcode);
    }

    if (CamxResultSuccess == result)
    {
        result = m_pIQPacket->SetKMDCmdBufferIndex(cmdBufferIndex,
                                                   (m_pCommonCmdBuffer->GetResourceUsedDwords() * sizeof(UINT32)));
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::ConfigBufferIO
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFENode::ConfigBufferIO(
    ExecuteProcessRequestData* pExecuteProcessRequestData)
{
    CAMX_ASSERT(NULL != pExecuteProcessRequestData);

    CamxResult                result                                 = CamxResultSuccess;
    NodeProcessRequestData*   pNodeRequestData                       = pExecuteProcessRequestData->pNodeProcessRequestData;
    PerRequestActivePorts*    pPerRequestPorts                       = pExecuteProcessRequestData->pEnabledPortsInfo;
    BOOL                      isSnapshot                             = FALSE;
    PerRequestOutputPortInfo* pOutputPort                            = NULL;
    BOOL                      groupFenceSignaled[MaxBufferComposite] = { 0 };

    CAMX_ASSERT(NULL != pNodeRequestData);
    CAMX_ASSERT(NULL != pPerRequestPorts);

    /// @todo (CAMX-4167) Base node should have called BindBuffers by now, no need to call again.
    // If LateBinding is enabled, output ImageBuffers may not have backing buffers yet, so lets bind the buffers as we are
    // starting to prepare IO configuration
    result = BindInputOutputBuffers(pExecuteProcessRequestData->pEnabledPortsInfo, TRUE, TRUE);
    if (CamxResultSuccess != result)
    {
        CAMX_LOG_ERROR(CamxLogGroupISP, "Failed in Binding backing buffers to output ImageBuffers, result=%d", result);
    }

    BOOL isFSSnapshot = IsFSSnapshot(pExecuteProcessRequestData, pNodeRequestData->pCaptureRequest->requestId);

    for (UINT i = 0; (i < pPerRequestPorts->numOutputPorts) && (CamxResultSuccess == result); i++)
    {
        UINT numBatchedFrames = pNodeRequestData->pCaptureRequest->numBatchedFrames;
        pOutputPort           = &pPerRequestPorts->pOutputPorts[i];
        if (NULL != pOutputPort)
        {
            for (UINT bufferIndex = 0; bufferIndex < pOutputPort->numOutputBuffers; bufferIndex++)
            {
                FrameSubsampleConfig currentConfig;
                UINT32               channelId      = 0;
                ImageBuffer*         pImageBuffer   = pOutputPort->ppImageBuffer[bufferIndex];
                BOOL                 isPortIOConfig = TRUE;

                /// @todo (CAMX-1015) See if this per request translation can be avoided
                result = MapPortIdToChannelId(pOutputPort->portId, &channelId);

                if (CamxResultSuccess == result)
                {
                    if (NULL != pImageBuffer)
                    {
                        if (TRUE == m_isDisabledOutputPort[OutputPortIndex(pOutputPort->portId)])
                        {
                                // There won't be any HW signal, so signal the fence right away.
                            if ((NULL != pOutputPort) && (CSLInvalidHandle != *pOutputPort->phFence))
                            {
                                CSLFenceSignal(*pOutputPort->phFence, CSLFenceResultSuccess);
                                isPortIOConfig = FALSE;
                            }
                        }
                        else
                        {
                            if (numBatchedFrames > 1)
                            {
                                if (TRUE == pOutputPort->flags.isBatch)
                                {
                                    // Set Pattern to 1 and Period to 0 for no frame dropping
                                    currentConfig.frameDropPattern = 1;
                                    currentConfig.frameDropPeriod  = 0;
                                    currentConfig.subsamplePattern = 1 << (numBatchedFrames - 1);
                                    currentConfig.subsamplePeriod  = (numBatchedFrames - 1);
                                }
                                else
                                {
                                    currentConfig.frameDropPattern = 1;
                                    currentConfig.frameDropPeriod  = (numBatchedFrames - 1);
                                    currentConfig.subsamplePattern = 1;
                                    currentConfig.subsamplePeriod  = 0;
                                }

                                result = m_pIQPacket->AddIOConfig(pImageBuffer,
                                                                  channelId,
                                                                  CSLIODirection::CSLIODirectionOutput,
                                                                  pOutputPort->phFence,
                                                                  1,
                                                                  NULL,
                                                                  &currentConfig,
                                                                  0);
                            }
                            else if (TRUE == m_enableBusRead)
                            {
                                if (TRUE == isFSSnapshot)
                                {
                                    // In FS Snapshot case configure only RDI1
                                    if (IFEOutputPortRDI1 == pOutputPort->portId)
                                    {
                                        CAMX_LOG_INFO(CamxLogGroupISP, "FS: IFE:%d Snapshot - Configure only RDI1");
                                        result = m_pIQPacket->AddIOConfig(pImageBuffer,
                                                                          channelId,
                                                                          CSLIODirection::CSLIODirectionOutput,
                                                                          pOutputPort->phFence,
                                                                          1,
                                                                          NULL,
                                                                          NULL,
                                                                          0);
                                    }
                                    else
                                    {
                                        UINT groupID   = m_bufferComposite.portGroupID[pOutputPort->portId];
                                        isPortIOConfig = FALSE;
                                        /* For rest of the ports signal the fence right away.
                                           Grouped fences need to be signaled only once */
                                        if ((NULL != pOutputPort)                       &&
                                            (CSLInvalidHandle != *pOutputPort->phFence) &&
                                            (FALSE == groupFenceSignaled[groupID]))
                                        {
                                            CSLFenceSignal(*pOutputPort->phFence, CSLFenceResultSuccess);
                                            groupFenceSignaled[groupID] = TRUE;
                                            CAMX_LOG_INFO(CamxLogGroupISP,
                                                          "FS: IFE:%d Snapshot Signal hFence=%d request=%llu groupID=%u",
                                                          InstanceID(),
                                                          *pOutputPort->phFence,
                                                          pNodeRequestData->pCaptureRequest->requestId,
                                                          groupID);
                                        }
                                    }
                                }
                                else
                                {
                                    // FS - Preview
                                    result = m_pIQPacket->AddIOConfig(pImageBuffer,
                                                                      channelId,
                                                                      CSLIODirection::CSLIODirectionOutput,
                                                                      pOutputPort->phFence,
                                                                      1,
                                                                      NULL,
                                                                      NULL,
                                                                      0);

                                    if (IFEOutputPortRDI0 == pOutputPort->portId)
                                    {
                                        // In FS mode RDI0 WM o/p will be BusRead i/p
                                        result = m_pIQPacket->AddIOConfig(pImageBuffer,
                                                                          IFEInputBusRead,
                                                                          CSLIODirection::CSLIODirectionInput,
                                                                          pOutputPort->phFence,
                                                                          1,
                                                                          NULL,
                                                                          NULL,
                                                                          0);

                                        CAMX_LOG_INFO(CamxLogGroupISP,
                                                      "FS: IFE:%d BusRead I/O config, hFence=%d, imgBuf=%p, reqID=%llu rc:%d",
                                                      InstanceID(),
                                                      *pOutputPort->phFence,
                                                      pImageBuffer,
                                                      pNodeRequestData->pCaptureRequest->requestId,
                                                      result);
                                    }
                                }
                            }
                            else
                            {
                                result = m_pIQPacket->AddIOConfig(pImageBuffer,
                                                                  channelId,
                                                                  CSLIODirection::CSLIODirectionOutput,
                                                                  pOutputPort->phFence,
                                                                  1,
                                                                  NULL,
                                                                  NULL,
                                                                  0);
                            }
                        }

                        if ((TRUE == isPortIOConfig) && (NULL != pImageBuffer->GetFormat()))
                        {
                            CAMX_LOG_INFO(CamxLogGroupISP,
                                          "IFE:%d reporting I/O config, portId=%d, hFence=%d, imgBuf=%p WxH=%dx%d request=%llu",
                                          InstanceID(),
                                          pOutputPort->portId,
                                          *pOutputPort->phFence,
                                          pImageBuffer,
                                          pImageBuffer->GetFormat()->width,
                                          pImageBuffer->GetFormat()->height,
                                          pNodeRequestData->pCaptureRequest->requestId);
                        }
                    }
                    else
                    {
                        CAMX_LOG_ERROR(CamxLogGroupISP, "Image Buffer is Null for output buffer %d", bufferIndex);
                        result = CamxResultEInvalidState;
                    }
                }
                else
                {
                    CAMX_LOG_ERROR(CamxLogGroupISP, "Not able to find matching Ouput ID");
                    result = CamxResultEInvalidState;
                }

                if (CamxResultSuccess != result)
                {
                    break;
                }
            }
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::CommitAndSubmitPacket
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFENode::CommitAndSubmitPacket()
{
    CamxResult result = CamxResultSuccess;

    if (NULL != m_p64bitDMIBuffer)
    {
        result = m_p64bitDMIBuffer->CommitCommands();
        CAMX_ASSERT(CamxResultSuccess == result);
    }

    if ((CamxResultSuccess == result) && (NULL != m_p32bitDMIBuffer))
    {
        result = m_p32bitDMIBuffer->CommitCommands();
        CAMX_ASSERT(CamxResultSuccess == result);
    }

    if (CamxResultSuccess == result)
    {
        result = m_pIQPacket->CommitPacket();
        CAMX_ASSERT(CamxResultSuccess == result);
    }

    if (CamxResultSuccess == result)
    {
        result = GetHwContext()->Submit(GetCSLSession(), m_hDevice, m_pIQPacket);
    }

    if (CamxResultSuccess != result)
    {
        CAMX_ASSERT_ALWAYS_MESSAGE("CSL Submit Failed: %s", Utils::CamxResultToString(result));
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::CheckForRDIOnly
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFENode::CheckForRDIOnly()
{
    UINT       outputPortId[MaxIFEOutputPorts];
    UINT       totalOutputPort                  = 0;
    CamxResult result                           = CamxResultSuccess;

    // Set RDI only as default case
    m_RDIOnlyUseCase = TRUE;

    // Get Output Port List
    GetAllOutputPortIds(&totalOutputPort, &outputPortId[0]);

    if (totalOutputPort == 0)
    {
        CAMX_LOG_ERROR(CamxLogGroupISP, "Error: no output port");
        result = CamxResultEInvalidArg;
    }

    for (UINT index = 0; index < totalOutputPort; index++)
    {
        if ((IFEOutputPortRDI0        != outputPortId[index]) &&
            (IFEOutputPortRDI1        != outputPortId[index]) &&
            (IFEOutputPortRDI2        != outputPortId[index]) &&
            (IFEOutputPortRDI3        != outputPortId[index]) &&
            (IFEOutputPortStatsDualPD != outputPortId[index]))
        {
            m_RDIOnlyUseCase = FALSE;
            CAMX_LOG_VERBOSE(CamxLogGroupISP, " m_RDIOnlyUseCase False");
            break;
        }
    }
    CAMX_LOG_VERBOSE(CamxLogGroupISP, " m_RDIOnlyUseCase %d", m_RDIOnlyUseCase);

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::ReadDefaultStatsConfig
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFENode::ReadDefaultStatsConfig(
    ISPInputData* pModuleInput)
{
    CamxResult              result               = CamxResultSuccess;
    static const UINT Properties3A[] =
    {
        PropertyIDUsecaseAECFrameControl,
        PropertyIDUsecaseAWBFrameControl,
        PropertyIDUsecaseAECStatsControl,
        PropertyIDUsecaseAWBStatsControl,
        PropertyIDUsecaseAFFrameControl,
        PropertyIDUsecaseAFStatsControl,
        PropertyIDAFDStatsControl,
        PropertyIDUsecaseHWPDConfig
    };

    VOID* pPropertyData3A[CAMX_ARRAY_SIZE(Properties3A)]       = { 0 };
    UINT64 propertyData3AOffset[CAMX_ARRAY_SIZE(Properties3A)] = { 0 };
    UINT length                    = CAMX_ARRAY_SIZE(Properties3A);

    GetDataList(Properties3A, pPropertyData3A, propertyData3AOffset, length);

    if (NULL != pPropertyData3A[0])
    {
        Utils::Memcpy(pModuleInput->pAECUpdateData, pPropertyData3A[0], sizeof(AECFrameControl));
    }
    if (NULL != pPropertyData3A[1])
    {
        Utils::Memcpy(pModuleInput->pAWBUpdateData, pPropertyData3A[1], sizeof(AWBFrameControl));
    }
    if (NULL != pPropertyData3A[2])
    {
        Utils::Memcpy(pModuleInput->pAECStatsUpdateData, pPropertyData3A[2], sizeof(AECStatsControl));
    }
    if (NULL != pPropertyData3A[3])
    {
        Utils::Memcpy(pModuleInput->pAWBStatsUpdateData, pPropertyData3A[3], sizeof(AWBStatsControl));
    }
    if (NULL != pPropertyData3A[4])
    {
        Utils::Memcpy(pModuleInput->pAFUpdateData, pPropertyData3A[4], sizeof(AFFrameControl));
    }
    if (NULL != pPropertyData3A[5])
    {
        Utils::Memcpy(pModuleInput->pAFStatsUpdateData, pPropertyData3A[5], sizeof(AFStatsControl));
    }
    if (NULL != pPropertyData3A[6])
    {
        Utils::Memcpy(pModuleInput->pAFDStatsUpdateData, pPropertyData3A[6], sizeof(AFDStatsControl));
    }
    if (NULL != pPropertyData3A[7])
    {
        Utils::Memcpy(pModuleInput->pPDHwConfig, pPropertyData3A[7], sizeof(PDHwConfig));
    }
    else
    {
        CAMX_LOG_WARN(CamxLogGroupISP, "PDAF HW Configuration is NULL");
    }
    CAMX_LOG_INFO(CamxLogGroupISP, "Reading from UsecasePool by IFE node");
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::ReleaseResources
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFENode::ReleaseResources(
    CHIDEACTIVATEPIPELINEMODE modeBitmask)
{
    CamxResult  result = CamxResultSuccess;

    if (CHIDeactivateModeSensorStandby & modeBitmask)
    {
        CAMX_LOG_INFO(CamxLogGroupISP,
                      "IFE:%d IFE %p Skipping Resource Release ModeMask 0x%X device handle %d",
                      InstanceID(),
                      this,
                      modeBitmask,
                      m_hDevice);
    }
    else
    {
        result = CSLReleaseHardware(GetCSLSession(),
                                    m_hDevice);
        RecycleAllCmdBuffers();
        if (result == CamxResultSuccess)
        {
            m_isIFEResourceAcquried = FALSE;
            CAMX_LOG_INFO(CamxLogGroupISP,
                      "IFE:%d IFE %p Resource Release ModeMask 0x%X device handle %d",
                      InstanceID(),
                      this,
                      modeBitmask,
                      m_hDevice);
        }
    }

    // Release all the command buffers
    RecycleAllCmdBuffers();
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::UpdateInitIQSettings
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFENode::UpdateInitIQSettings()
{
    CamxResult result = CamxResultSuccess;

    m_IFEInitData.pOTPData                        = m_pOTPData;
    m_IFEInitData.frameNum                        = FirstValidRequestId;
    m_IFEInitData.pHwContext                      = GetHwContext();
    m_IFEInitData.pAECUpdateData                  = &m_frameConfig.AECUpdateData;
    m_IFEInitData.pAECStatsUpdateData             = &m_frameConfig.AECStatsUpdateData;
    m_IFEInitData.pAWBUpdateData                  = &m_frameConfig.AWBUpdateData;
    m_IFEInitData.pAWBStatsUpdateData             = &m_frameConfig.AWBStatsUpdateData;
    m_IFEInitData.pAFUpdateData                   = &m_frameConfig.AFUpdateData;
    m_IFEInitData.pAFStatsUpdateData              = &m_frameConfig.AFStatsUpdateData;
    m_IFEInitData.pAFDStatsUpdateData             = &m_frameConfig.AFDStatsUpdateData;
    m_IFEInitData.pIHistStatsUpdateData           = &m_frameConfig.IHistStatsUpdateData;
    m_IFEInitData.pPDHwConfig                     = &m_frameConfig.pdHwConfig;
    m_IFEInitData.pCSStatsUpdateData              = &m_frameConfig.CSStatsUpdateData;
    m_IFEInitData.pStripeConfig                   = &m_frameConfig;
    m_IFEInitData.pHALTagsData                    = &m_HALTagsData;
    m_IFEInitData.disableManual3ACCM              = m_disableManual3ACCM;
    m_IFEInitData.sensorBitWidth                  = m_pSensorCaps->sensorConfigs[0].streamConfigs[0].bitWidth;
    m_IFEInitData.sensorData.fullResolutionWidth  = m_pSensorModeRes0Data->resolution.outputWidth;
    m_IFEInitData.sensorData.fullResolutionHeight = m_pSensorModeRes0Data->resolution.outputHeight;
    m_IFEInitData.sensorData.sensorBinningFactor  = m_ISPInputSensorData.sensorBinningFactor;
    m_IFEInitData.isDualcamera                    = GetPipeline()->IsMultiCamera();
    m_IFEInitData.useHardcodedRegisterValues      = CheckToUseHardcodedRegValues(m_IFEInitData.pHwContext);
    m_IFEInitData.pTuningData                     = &m_tuningData;
    m_IFEInitData.tuningModeChanged               = ISPIQModule::IsTuningModeDataChanged(m_IFEInitData.pTuningData,
                                                                                         NULL);
    m_IFEInitData.maxOutputWidthFD   = m_maxOutputWidthFD;
    m_IFEInitData.titanVersion       = m_titanVersion;
    m_IFEInitData.forceTriggerUpdate = TRUE;

    HardcodeSettings(&m_IFEInitData, &m_frameConfig, TRUE);

    DynamicCAMIFCrop(&m_frameConfig);

    if (TRUE == m_useStatsAlgoConfig)
    {
        // OEM can not update config as part of config streams, use hardcode values
        if (FALSE == m_OEMStatsConfig)
        {
            // Overwrites with the proper stats configuration information
            result = ReadDefaultStatsConfig(&m_IFEInitData);
            // HardCode Tintless Settings
            HardcodeTintlessSettings(&m_IFEInitData);
        }

        if (CamxResultSuccess != result)
        {
            CAMX_LOG_ERROR(CamxLogGroupISP, "Failed to read the default stream on stats configuration!");
        }
    }

    return result;

}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::UpdateInitSettings
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFENode::UpdateInitSettings()
{
    CamxResult result = CamxResultSuccess;

    Utils::Memset(&m_IFEInitData, 0, sizeof(ISPInputData));

    m_IFEInitData.pIFEOutputPathInfo = m_IFEOutputPathInfo;
    m_IFEInitData.pTuningDataManager = GetTuningDataManager();

    Utils::Memcpy(&m_IFEInitData.sensorData, &m_ISPInputSensorData, sizeof(ISPSensorConfigureData));

    if (TRUE == m_HVXInputData.enableHVX)
    {
        SetupHVXInitialConfig(&m_IFEInitData);
    }

    if ((CamxResultSuccess == result) &&
        (FALSE == m_RDIOnlyUseCase))
    {
        UpdateInitIQSettings();
    }

    // Check for FastShutter (FS) Mode. Enable IFE read/fetch patch
    result = IsFSModeEnabled(&m_enableBusRead);
    if (TRUE == m_enableBusRead)
    {
        m_genericBlobCmdBufferSizeBytes += sizeof(IFEBusReadConfig);
        CAMX_LOG_INFO(CamxLogGroupISP, "FS: IFE Read Path Enabled");
    }

    if (CamxResultSuccess == result)
    {
        m_IFEInitData.minRequiredSingleIFEClock = CalculatePixelClockRate(m_pSensorModeData->resolution.outputWidth);

        IQInterface::IQSetupTriggerData(&m_IFEInitData, this, GetMaximumPipelineDelay(), NULL);

        m_IFEInitData.forceIFEflag = m_instanceProperty.IFESingleOn;

        m_mode = DualIFEUtils::EvaluateDualIFEMode(&m_IFEInitData);
        CAMX_LOG_INFO(CamxLogGroupISP, "IFEMode %d vfeClk %lld sensorWidth %d",
            m_mode, m_IFEInitData.minRequiredSingleIFEClock, m_pSensorModeData->resolution.outputWidth);
        m_IFEInitData.pipelineIFEData.moduleMode = m_mode;
        if (IFEModuleMode::DualIFENormal == m_mode)
        {
            CAMX_ASSERT(NULL == m_pStripingInput);
            CAMX_ASSERT(NULL == m_pPassOut);

            m_stripeConfigs[0].overwriteStripes = IFEOverwriteModuleWithStriping;
            m_stripeConfigs[1].overwriteStripes = IFEOverwriteModuleWithStriping;

            m_pStripingInput = static_cast<IFEStripingInput*>(CAMX_CALLOC(sizeof(IFEStripingInput)));
            m_pPassOut = static_cast<IFEPassOutput*>(CAMX_CALLOC(sizeof(IFEPassOutput)));
            if ((NULL == m_pStripingInput) || (NULL == m_pPassOut))
            {
                result = CamxResultENoMemory;
            }
            if (CamxResultSuccess == result)
            {
                // Store bankSelect for interpolation to store mesh_table_l[bankSelect] and mesh_table_r[bankSelect]
                m_frameConfig.stateLSC.dependenceData.bankSelect =
                    m_stripeConfigs[0].stateLSC.dependenceData.bankSelect;
                m_frameConfig.pFrameLevelData                    = &m_ISPFrameData;
                m_IFEInitData.pStripingInput                     = m_pStripingInput;
                m_IFEInitData.pStripeConfig                      = &m_frameConfig;
                m_IFEInitData.pCalculatedData                    = &m_ISPFramelevelData;

                result = PrepareStripingParameters(&m_IFEInitData);
                if (CamxResultSuccess == result)
                {
                    result = DualIFEUtils::ComputeSplitParams(&m_IFEInitData, &m_dualIFESplitParams);
                }
            }
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::AcquireResources
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFENode::AcquireResources()
{
    CamxResult              result = CamxResultSuccess;

    if (FALSE == m_isIFEResourceAcquried)
    {
        CAMX_LOG_INFO(CamxLogGroupISP,
                      "IFE:%d IFE %p Acquiring Resource device handle %d",
                      InstanceID(),
                      this,
                      m_hDevice);
        // Acquire IFE device resources
        CSLDeviceResource deviceResource;

        deviceResource.deviceResourceParamSize = m_IFEResourceSize;
        deviceResource.pDeviceResourceParam    = m_pIFEResourceInfo;
        deviceResource.resourceID              = ISPResourceIdPort;

        result = CSLAcquireHardware(GetCSLSession(),
                                    m_hDevice,
                                    &deviceResource,
                                    NodeIdentifierString());
        // Create and Submit initial config packet
        if (CamxResultSuccess == result)
        {
            m_isIFEResourceAcquried = TRUE;
            result = FetchCmdBuffers(FirstValidRequestId);
        }
        if ((CamxResultSuccess == result) &&
            (FALSE == m_RDIOnlyUseCase))
        {
            if (IFEModuleMode::DualIFENormal == m_mode)
            {
                result = DualIFEUtils::UpdateDualIFEConfig(&m_IFEInitData,
                    m_PDAFInfo,
                    m_stripeConfigs,
                    &m_dualIFESplitParams,
                    m_pPassOut);
            }
            else
            {
                // Each module has stripe specific configuration that needs to be updated before calling upon module
                // Execute function. In single IFE case, there's only one stripe and hence we will use
                // m_stripeConfigs[0] to hold the input configuration
                UpdateIQStateConfiguration(&m_frameConfig, &m_stripeConfigs[0]);
            }

            if (TRUE == m_libInitialData.isSucceed)
            {
                m_IFEInitData.pLibInitialData = m_libInitialData.pLibData;
            }

            m_IFEInitData.p64bitDMIBuffer     = m_p64bitDMIBuffer;
            m_IFEInitData.p32bitDMIBuffer     = m_p32bitDMIBuffer;
            m_IFEInitData.p32bitDMIBufferAddr = m_p32bitDMIBufferAddr;
            m_IFEInitData.p64bitDMIBufferAddr = m_p64bitDMIBufferAddr;
            m_IFEInitData.pStripeConfig       = &m_frameConfig;
            m_frameConfig.pFrameLevelData     = &m_ISPFrameData;

            // CAMIF needs to be programed during the initialization time
            m_IFEInitData.pipelineIFEData.programCAMIF = TRUE;

            m_IFEInitData.pipelineIFEData.moduleMode = m_mode;

            m_IFEInitData.pipelineIFEData.numBatchedFrames = m_usecaseNumBatchedFrames;

            m_IFEInitData.dualPDPipelineData.programCAMIFLite = TRUE;
            m_IFEInitData.dualPDPipelineData.numBatchedFrames = m_usecaseNumBatchedFrames;

            // Execute IQ module configuration and updated module enable info
            result = ProgramIQConfig(&m_IFEInitData);

            if (CamxResultSuccess == result)
            {
                result = ProgramIQEnable(&m_IFEInitData);
            }
        }

        // Initailiaze the non IQ settings like Batch mode, clock settings and UBWC config
        if (CamxResultSuccess == result)
        {
            result = InitialSetupandConfig();
        }

        // Add all command buffer reference to the packet
        if (CamxResultSuccess == result)
        {
            if (IFEModuleMode::DualIFENormal == m_mode)
            {
                result = AddCmdBufferReference(FirstValidRequestId, CSLPacketOpcodesDualIFEInitialConfig);
            }
            else
            {
                result = AddCmdBufferReference(FirstValidRequestId, CSLPacketOpcodesIFEInitialConfig);
            }
        }

        // Commit DMI command buffers and do a CSL submit of the request
        if (CamxResultSuccess == result)
        {
            result = CommitAndSubmitPacket();
        }
    }
    else
    {
        CAMX_LOG_INFO(CamxLogGroupISP,
                      "IFE:%d IFE %p Skipping Resource Acquire device handle %d",
                      InstanceID(),
                      this,
                      m_hDevice);
    }
    return result;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::NewActiveStreamsSetup
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IFENode::NewActiveStreamsSetup(
    UINT activeStreamIdMask)
{
    BOOL isNodeEnabled         = FALSE;
    UINT fullPortEnableMask    = 0x0;
    UINT displayPortEnableMask = 0x0;
    UINT numOutputPortsEnabled = 0;

    // Mask all DS4/DS16 and Full port mask
    // Such that all ports are enabled if any port is enabled
    for (UINT portIndex = 0; portIndex < GetNumOutputPorts(); portIndex++)
    {
        OutputPort* pOutputPort =  GetOutputPort(portIndex);

        if ((IFEOutputPortDS4 == pOutputPort->portId) ||
            (IFEOutputPortDS16 == pOutputPort->portId) ||
            (IFEOutputPortFull == pOutputPort->portId))
        {
            fullPortEnableMask |= pOutputPort->enabledInStreamMask;
        }
        else if ((IFEOutputPortDisplayDS4 == pOutputPort->portId) ||
                 (IFEOutputPortDisplayDS16 == pOutputPort->portId) ||
                 (IFEOutputPortDisplayFull == pOutputPort->portId))
        {
            displayPortEnableMask |= pOutputPort->enabledInStreamMask;
        }
    }

    for (UINT portIndex = 0; portIndex < GetNumOutputPorts(); portIndex++)
    {
        OutputPort* pOutputPort = GetOutputPort(portIndex);

        UINT mask = 0x0;

        if ((IFEOutputPortDS4 == pOutputPort->portId) ||
            (IFEOutputPortDS16 == pOutputPort->portId) ||
            (IFEOutputPortFull == pOutputPort->portId))
        {
            // Replace EnableInstream mask with the calculated mask
            mask = fullPortEnableMask;
        }
        else if ((IFEOutputPortDisplayDS4 == pOutputPort->portId) ||
                 (IFEOutputPortDisplayDS16 == pOutputPort->portId) ||
                 (IFEOutputPortDisplayFull == pOutputPort->portId))
        {
            mask = displayPortEnableMask;
        }
        else
        {
            mask = pOutputPort->enabledInStreamMask;
        }


        // "enabledInStreamMask" is the list of streams for which the output port needs to be enabled. So if any stream in
        // "activeStreamIdMask" is also set in "enabledInStreamMask", it means we need to enable the output port
        if (0 != (mask & activeStreamIdMask) &&
            pOutputPort->numInputPortsConnected > pOutputPort->numInputPortsDisabled)
        {
            pOutputPort->flags.isEnabled = TRUE;
            numOutputPortsEnabled++;
        }
        else
        {
            pOutputPort->flags.isEnabled = FALSE;
        }
    }


    isNodeEnabled = ((0 == GetNumOutputPorts()) || (0 < numOutputPortsEnabled)) ? TRUE : FALSE;

    SetNodeEnabled(isNodeEnabled);

    UINT                inputPortId[MaxIFEInputPorts];
    UINT                totalInputPort                  = 0;
    // Get Input Port List

    GetAllInputPortIds(&totalInputPort, &inputPortId[0]);

    /// @note It is assumed that if any output port is enabled, all the input ports are active
    ///       i.e. all inputs are required to generate any (and all) outputs
    for (UINT portIndex = 0; portIndex < totalInputPort; portIndex++)
    {
        InputPort* pInputPort       = GetInputPort(portIndex);
        if (FALSE == pInputPort->portDisabled)
        {
            pInputPort->flags.isEnabled = isNodeEnabled;
        }
        else
        {
            pInputPort->flags.isEnabled = FALSE;
        }
    }
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::PostPipelineCreate
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFENode::PostPipelineCreate()
{
    CamxResult              result                     = CamxResultSuccess;
    const StaticSettings*   pSettings                  = HwEnvironment::GetInstance()->GetStaticSettings();
    FLOAT                   sensorAspectRatio          = 0.0f;
    FLOAT                   sensorFullSizeAspectRatio  = 0.0f;


    m_tuningData.noOfSelectionParameter = 1;
    m_tuningData.TuningMode[0].mode     = ChiModeType::Default;

    m_highInitialBWCnt = 0;

    if (FALSE == m_RDIOnlyUseCase)
    {
        // Assemble IFE IQ Modules
        result = CreateIFEIQModules();

        // Assemble IFE Stats Module
        if (CamxResultSuccess == result)
        {
            result = CreateIFEStatsModules();
        }

        if (CamxResultSuccess == result)
        {
            CalculateIQCmdSize();
            // Each member in IFEModuleEnableConfig points to an 32 bit register, and dont have continuous offset
            // Each register needs a header info while writing into pCmdBuffer.
            m_totalIQCmdSizeDWord += (sizeof(IFEModuleEnableConfig) / RegisterWidthInBytes) *
                PacketBuilder::RequiredWriteRegRangeSizeInDwords(RegisterWidthInBytes);
        }

        if (CamxResultSuccess != result)
        {
            Cleanup();
        }
    }

    if (CamxResultSuccess == result)
    {

        if ((CSLPresilEnabled == GetCSLMode()) || (CSLPresilRUMIEnabled == GetCSLMode()) || (TRUE == IsTPGMode()))
        {
            m_pSensorModeData = &m_testGenModeData;
            m_pSensorModeRes0Data = &m_testGenModeData;
        }
        else
        {
            // IFE sink only use case, where FinalizeBufferProperties wont be called
            if (NULL == m_pSensorModeData)
            {
                GetSensorModeData(&m_pSensorModeData);
                GetSensorModeRes0Data(&m_pSensorModeRes0Data);

                if (NULL == m_pSensorModeData)
                {
                    result = CamxResultEInvalidState;
                    CAMX_LOG_ERROR(CamxLogGroupISP, "m_pSensorModeData is NULL.");
                }
                else if ((0 == m_pSensorModeData->resolution.outputWidth) || (0 == m_pSensorModeData->resolution.outputHeight))
                {
                    result = CamxResultEInvalidState;
                    CAMX_LOG_ERROR(CamxLogGroupISP,
                                   "Invalid sensor resolution, W:%d x H:%d!\n",
                                   m_pSensorModeData->resolution.outputWidth,
                                   m_pSensorModeData->resolution.outputHeight);
                }
            }
        }

        if (CamxResultSuccess == result)
        {
            if (NULL == m_pOTPData)
            {
                m_pOTPData = &m_pSensorCaps->OTPData;
            }

            m_currentSensorModeSupportPDAF = FindSensorStreamConfigIndex(StreamType::PDAF, NULL);
            m_currentSensorModeSupportHDR  = FindSensorStreamConfigIndex(StreamType::HDR, NULL);
            m_currentSensorModeSupportMeta = FindSensorStreamConfigIndex(StreamType::META, NULL);

            CAMX_LOG_INFO(CamxLogGroupISP, "m_currentSensorModeSupportPDAF = %u", m_currentSensorModeSupportPDAF);
            CAMX_LOG_INFO(CamxLogGroupISP, "m_currentSensorModeSupportHDR = %u", m_currentSensorModeSupportHDR);
            CAMX_LOG_INFO(CamxLogGroupISP, "m_currentSensorModeSupportMeta = %u", m_currentSensorModeSupportMeta);

            UINT numOutputPorts = 0;
            UINT outputPortId[MaxIFEOutputPorts];
            GetAllOutputPortIds(&numOutputPorts, &outputPortId[0]);
            CAMX_LOG_INFO(CamxLogGroupISP, "totalOutputPorts = %u", numOutputPorts);

            for (UINT outputPortIndex = 0; outputPortIndex < numOutputPorts; outputPortIndex++)
            {
                UINT outputPortNodeIndex = OutputPortIndex(outputPortId[outputPortIndex]);
                m_isDisabledOutputPort[outputPortNodeIndex] = FALSE;
                if (TRUE == CheckOutputPortIndexIfUnsupported(outputPortNodeIndex))
                {
                    m_isDisabledOutputPort[outputPortNodeIndex] = TRUE;
                    m_disabledOutputPorts++;
                }
            }
        }
    }

    if (CamxResultSuccess == result)
    {
        // Read numbers of frames in batch
        const UINT PropUsecase[] = { PropertyIDUsecaseBatch };
        VOID* pData[] = { 0 };
        UINT64 dataOffset[1] = { 0 };
        GetDataList(PropUsecase, pData, dataOffset, 1);
        if (NULL != pData[0])
        {
            m_usecaseNumBatchedFrames = *reinterpret_cast<UINT*>(pData[0]);
        }

        // Read dimension and format from output ports for HAL data.
        CamX::Utils::Memset(&m_ISPInputHALData, 0, sizeof(m_ISPInputHALData));
        InitializeOutputPathImageInfo();

        /// @todo (CAMX-833) Update ISP input data from the port and disable hardcoded flag from settings
        m_enableHardcodedConfig = static_cast<Titan17xContext *>(
            GetHwContext())->GetTitan17xSettingsManager()->GetTitan17xStaticSettings()->IFEEnableHardcodedConfig;


        m_useStatsAlgoConfig = (FALSE == m_enableHardcodedConfig) && (TRUE == m_hasStatsNode);


        CAMX_LOG_VERBOSE(CamxLogGroupISP, "useStatsAlgo %d, Hardcod En %d, Stats Node %d, OEM Stats %d",
                         m_useStatsAlgoConfig,
                         m_enableHardcodedConfig,
                         m_hasStatsNode,
                         m_OEMStatsConfig);

        // Set the sensor data information for ISP input
        m_ISPInputSensorData.CAMIFCrop.firstPixel      = m_pSensorModeData->cropInfo.firstPixel;
        m_ISPInputSensorData.CAMIFCrop.firstLine       = m_pSensorModeData->cropInfo.firstLine;
        m_ISPInputSensorData.CAMIFCrop.lastPixel       = m_pSensorModeData->cropInfo.lastPixel;
        m_ISPInputSensorData.CAMIFCrop.lastLine        = m_pSensorModeData->cropInfo.lastLine;

        // CSID crop override
        if (TRUE == EnableCSIDCropOverridingForSingleIFE())
        {
            m_ISPInputSensorData.CAMIFCrop.firstPixel = m_instanceProperty.IFECSIDLeft;
            m_ISPInputSensorData.CAMIFCrop.lastPixel  = m_instanceProperty.IFECSIDLeft + m_instanceProperty.IFECSIDWidth - 1;
            m_ISPInputSensorData.CAMIFCrop.firstLine  = m_instanceProperty.IFECSIDTop;
            m_ISPInputSensorData.CAMIFCrop.lastLine   = m_instanceProperty.IFECSIDTop + m_instanceProperty.IFECSIDHeight - 1;
        }

        m_ISPInputSensorData.format                    = m_pSensorModeData->format;
        m_ISPInputSensorData.isBayer                   = IsSensorModeFormatBayer(m_ISPInputSensorData.format);
        m_ISPInputSensorData.isYUV                     = IsSensorModeFormatYUV(m_ISPInputSensorData.format);
        m_ISPInputSensorData.isMono                    = IsSensorModeFormatMono(m_ISPInputSensorData.format);
        for (UINT32 i = 0; i < m_pSensorModeData->capabilityCount; i++)
        {
            if (m_pSensorModeData->capability[i] == SensorCapability::IHDR)
            {
                m_ISPInputSensorData.isIHDR        = TRUE;
                CAMX_LOG_VERBOSE(CamxLogGroupISP, "sensor I-HDR mode");
                break;
            }
        }
       // Get full sieze sensor Aspect Ratio
        const UINT sensorInfoTag[] =
        {
            StaticSensorInfoActiveArraySize,
        };
        const UINT length         = CAMX_ARRAY_SIZE(sensorInfoTag);
        VOID*      pDataOut[length]  = { 0 };
        UINT64     offset[length] = { 0 };

        result = GetDataList(sensorInfoTag, pDataOut, offset, length);
        if (CamxResultSuccess == result)
        {
            if (NULL != pDataOut[0])
            {
                Region region      = *static_cast<Region*>(pDataOut[0]);
                if (region.height != 0)
                {
                    sensorFullSizeAspectRatio =
                    static_cast<FLOAT>(region.width) / static_cast<FLOAT>(region.height);
                }
                else
                {
                    CAMX_LOG_ERROR(CamxLogGroupPProc, "Invalid Sensor Dimensions");
                }
            }
        }

        sensorAspectRatio         =
            static_cast<FLOAT>(m_pSensorModeData->resolution.outputWidth) /
            static_cast<FLOAT>(m_pSensorModeData->resolution.outputHeight);
        if (sensorAspectRatio <= (sensorFullSizeAspectRatio + AspectRatioTolerance))
        {
            m_ISPInputSensorData.sensorAspectRatioMode = SensorAspectRatioMode::FULLMODE;
        }
        else
        {
            m_ISPInputSensorData.sensorAspectRatioMode = SensorAspectRatioMode::NON_FULLMODE;
        }
        m_ISPInputSensorData.sensorOut.width           = m_pSensorModeData->resolution.outputWidth;
        m_ISPInputSensorData.sensorOut.height          = m_pSensorModeData->resolution.outputHeight;

        m_ISPInputSensorData.sensorOut.offsetX         = m_pSensorModeData->offset.xStart;
        m_ISPInputSensorData.sensorOut.offsetY         = m_pSensorModeData->offset.yStart;

        m_ISPInputSensorData.dGain                     = 1.0f;
        m_ISPInputSensorData.sensorScalingFactor       = m_pSensorModeData->downScaleFactor;
        m_ISPInputSensorData.sensorBinningFactor       = static_cast<FLOAT>(m_pSensorModeData->binningTypeH);
        m_ISPInputSensorData.ZZHDRColorPattern         = m_pSensorModeData->ZZHDRColorPattern;
        m_ISPInputSensorData.ZZHDRFirstExposurePattern = m_pSensorModeData->ZZHDRFirstExposurePattern;
        m_ISPInputSensorData.fullResolutionWidth       = m_pSensorModeRes0Data->resolution.outputWidth;
        m_ISPInputSensorData.fullResolutionHeight      = m_pSensorModeRes0Data->resolution.outputHeight;

        m_HALTagsData.HALCrop.left   = 0;
        m_HALTagsData.HALCrop.top    = 0;
        m_HALTagsData.HALCrop.width  = m_ISPInputSensorData.CAMIFCrop.lastPixel - m_ISPInputSensorData.CAMIFCrop.firstPixel + 1;
        m_HALTagsData.HALCrop.height = m_ISPInputSensorData.CAMIFCrop.lastLine - m_ISPInputSensorData.CAMIFCrop.firstLine + 1;

        /// @todo (CAMX-3276) Hard code YUV Sensor Crop configure for now.
        if (TRUE == m_ISPInputSensorData.isYUV)
        {
            m_HALTagsData.HALCrop.width >>= 1;
        }

        // Get Sensor PDAF information
        GetPDAFInformation();
    }

    if ((CamxResultSuccess == result) &&
        (FALSE == m_RDIOnlyUseCase))
    {
        UpdateInitSettings();
    }

    // Initialize command buffer managers, and allocate command buffers
    if (CamxResultSuccess == result)
    {
        m_IFECmdBlobCount = GetPipeline()->GetRequestQueueDepth() + 7;  // Number of blob command buffers in circulation
                                                                        // 1 extra is for Initial Configuration packet
                                                                        // 4 more for pipeline delay
                                                                        // 2 extra for meet max request depth
        result = CreateCmdBuffers();
    }

    if (CamxResultSuccess == result)
    {
        IQInterface::IQSettingModuleInitialize(&m_libInitialData);
    }

    // Acquire IFE device resources
    if (CamxResultSuccess == result)
    {
        result = AcquireDevice();
    }
    CAMX_LOG_INFO(CamxLogGroupISP, "m_disabledOutputPorts = %u", m_disabledOutputPorts);
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::InitialSetupandConfig
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFENode::InitialSetupandConfig()
{
    CamxResult result = CamxResultSuccess;
    // Add Initial HFR config information in Generic Blob cmd buffer
    if (CamxResultSuccess == result)
    {
        result = SetupHFRInitialConfig();
    }

    // Add Clock configuration to Initial Packet so that kernel driver can configure IFE clocks
    if (CamxResultSuccess == result)
    {
        result = SetupResourceClockConfig();
    }

    // Add Flush Halt Configuration
    if (CamxResultSuccess == result)
    {
        UINT32 value = 0;
        result = SetupFlushHaltConfig(value);
    }

    // Add Bandwidth configuration to Initial Packet so that kernel driver can configure BW votes
    if (CamxResultSuccess == result)
    {
        result = SetupResourceBWConfig(NULL, CamxInvalidRequestId);
    }

    // Add UBWC configuration to Initial packet so that kernel driver can configure UBWC.
    if (CamxResultSuccess == result)
    {
        result = SetupUBWCInitialConfig();
    }

    if ((CamxResultSuccess == result) && (TRUE == m_enableBusRead))
    {
        result = SetupBusReadInitialConfig();
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::UpdateIQStateConfiguration
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IFENode::UpdateIQStateConfiguration(
    ISPStripeConfig* pFrameConfig,
    ISPStripeConfig* pStripeConfig)
{
    // pStripeConfig points to stripe state that persists from request to request, whereas pFrameConfig points to input state
    // for the current request. In here, the persistent IQ module state is copied first to the input config, then then whole
    // input is copied back to avoid doing a field by field assignment here. This can be later cleaned up by putting all
    // persistent state in its own structure within stripe config.
    pFrameConfig->stateABF  = pStripeConfig->stateABF;
    pFrameConfig->stateBF   = pStripeConfig->stateBF;
    pFrameConfig->stateLSC  = pStripeConfig->stateLSC;
    CamX::Utils::Memcpy(pFrameConfig->stateCrop, pStripeConfig->stateCrop, sizeof(pFrameConfig->stateCrop));
    CamX::Utils::Memcpy(pFrameConfig->stateDS, pStripeConfig->stateDS, sizeof(pFrameConfig->stateDS));
    CamX::Utils::Memcpy(pFrameConfig->stateMNDS, pStripeConfig->stateMNDS, sizeof(pFrameConfig->stateMNDS));
    CamX::Utils::Memcpy(pFrameConfig->stateRoundClamp, pStripeConfig->stateRoundClamp, sizeof(pFrameConfig->stateRoundClamp));

    // Finally update all the input configuration to stripe configuration
    *pStripeConfig = *pFrameConfig;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::GetOEMIQSettings
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFENode::GetOEMIQSettings(
    VOID** ppOEMData)
{
    CamxResult result;
    UINT32     metaTag          = 0;

    CAMX_ASSERT(NULL != ppOEMData);

    result = VendorTagManager::QueryVendorTagLocation("org.quic.camera2.iqsettings", "OEMIFEIQSetting", &metaTag);

    CAMX_ASSERT_MESSAGE((CamxResultSuccess == result), "Fail to query: OEMIFEIQSetting");

    if (CamxResultSuccess == result)
    {
        const UINT OEMProperty[] = { metaTag | InputMetadataSectionMask };
        VOID* OEMData[]          = { 0 };
        UINT64 OEMDataOffset[]   = { 0 };
        GetDataList(OEMProperty, OEMData, OEMDataOffset, 1);

        // Pointers in OEMData[] guaranteed to be non-NULL by GetDataList() for InputMetadataSectionMask
        *ppOEMData = OEMData[0];
    }
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::GetOEMStatsConfig
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFENode::GetOEMStatsConfig(
    ISPStripeConfig* pFrameConfig)
{
    CamxResult result = CamxResultSuccess;

    UINT32 metadataAECFrameControl      = 0;
    UINT32 metadataAWBFrameControl      = 0;
    UINT32 metadataAECStatsControl      = 0;
    UINT32 metadataAWBStatsControl      = 0;
    UINT32 metadataAFStatsControl       = 0;
    UINT32 metadataAFDStatsControl      = 0;
    UINT32 metadataIHistStatsControl    = 0;

    result = VendorTagManager::QueryVendorTagLocation("org.quic.camera2.statsconfigs", "AECFrameControl",
        &metadataAECFrameControl);
    CAMX_ASSERT_MESSAGE((CamxResultSuccess == result), "Fail to query: AECFrameControl");
    result = VendorTagManager::QueryVendorTagLocation("org.quic.camera2.statsconfigs", "AWBFrameControl",
        &metadataAWBFrameControl);
    CAMX_ASSERT_MESSAGE((CamxResultSuccess == result), "Fail to query: AWBFrameControl");
    result = VendorTagManager::QueryVendorTagLocation("org.quic.camera2.statsconfigs", "AECStatsControl",
        &metadataAECStatsControl);
    CAMX_ASSERT_MESSAGE((CamxResultSuccess == result), "Fail to query: AECStatsControl");
    result = VendorTagManager::QueryVendorTagLocation("org.quic.camera2.statsconfigs", "AWBStatsControl",
        &metadataAWBStatsControl);
    CAMX_ASSERT_MESSAGE((CamxResultSuccess == result), "Fail to query: AWBStatsControl");
    result = VendorTagManager::QueryVendorTagLocation("org.quic.camera2.statsconfigs", "AFStatsControl",
        &metadataAFStatsControl);
    CAMX_ASSERT_MESSAGE((CamxResultSuccess == result), "Fail to query: AFStatsControl");
    result = VendorTagManager::QueryVendorTagLocation("org.quic.camera2.statsconfigs", "AFDStatsControl",
        &metadataAFDStatsControl);
    CAMX_ASSERT_MESSAGE((CamxResultSuccess == result), "Fail to query: AFDStatsControl");
    result = VendorTagManager::QueryVendorTagLocation("org.quic.camera2.statsconfigs", "IHistStatsControl",
        &metadataIHistStatsControl);
    CAMX_ASSERT_MESSAGE((CamxResultSuccess == result), "Fail to query: IHistStatsControl");

    static const UINT vendorTagsControl3A[] =
    {
        metadataAECFrameControl     | InputMetadataSectionMask,
        metadataAWBFrameControl     | InputMetadataSectionMask,
        metadataAECStatsControl     | InputMetadataSectionMask,
        metadataAWBStatsControl     | InputMetadataSectionMask,
        metadataAFStatsControl      | InputMetadataSectionMask,
        metadataAFDStatsControl     | InputMetadataSectionMask,
        metadataIHistStatsControl   | InputMetadataSectionMask,
    };
    const SIZE_T numTags                            = CAMX_ARRAY_SIZE(vendorTagsControl3A);
    VOID*        pVendorTagsControl3A[numTags]      = { 0 };
    UINT64       vendorTagsControl3AOffset[numTags] = { 0 };

    GetDataList(vendorTagsControl3A, pVendorTagsControl3A, vendorTagsControl3AOffset, numTags);

    // Pointers in pVendorTagsControl3A[] guaranteed to be non-NULL by GetDataList() for InputMetadataSectionMask
    Utils::Memcpy(&pFrameConfig->AECUpdateData,        pVendorTagsControl3A[0], sizeof(AECFrameControl));
    Utils::Memcpy(&pFrameConfig->AWBUpdateData,        pVendorTagsControl3A[1], sizeof(AWBFrameControl));
    Utils::Memcpy(&pFrameConfig->AECStatsUpdateData,   pVendorTagsControl3A[2], sizeof(AECStatsControl));
    Utils::Memcpy(&pFrameConfig->AWBStatsUpdateData,   pVendorTagsControl3A[3], sizeof(AWBStatsControl));
    Utils::Memcpy(&pFrameConfig->AFStatsUpdateData,    pVendorTagsControl3A[4], sizeof(AFStatsControl));
    Utils::Memcpy(&pFrameConfig->AFDStatsUpdateData,   pVendorTagsControl3A[5], sizeof(AFDStatsControl));
    Utils::Memcpy(&pFrameConfig->IHistStatsUpdateData, pVendorTagsControl3A[6], sizeof(IHistStatsControl));

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::GetTintlessStatus
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IFENode::GetTintlessStatus(
    ISPInputData* pModuleInput)
{
    CamxResult result;
    UINT32     metaTag = 0;

    result = VendorTagManager::QueryVendorTagLocation("org.quic.camera.tintless", "enable", &metaTag);

    if (CamxResultSuccess == result)
    {
        const UINT tintlessProperty[] = { metaTag | InputMetadataSectionMask };
        VOID* tintlessData[]          = { 0 };
        UINT64 tintlessDataOffset[]   = { 0 };
        result = GetDataList(tintlessProperty, tintlessData, tintlessDataOffset, 1);

        if ((CamxResultSuccess == result) && (tintlessData[0] != NULL))
        {
            pModuleInput->tintlessEnable = *(reinterpret_cast<BYTE*>(tintlessData[0]));
        }
        else
        {
            pModuleInput->tintlessEnable = FALSE;
        }
        CAMX_LOG_INFO(CamxLogGroupISP, "Tintless Enable %d", pModuleInput->tintlessEnable);
    }
    else
    {
        pModuleInput->tintlessEnable = FALSE;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::Get3AFrameConfig
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFENode::Get3AFrameConfig(
    ISPInputData*    pModuleInput,
    ISPStripeConfig* pFrameConfig,
    UINT64           requestId)
{
    static const UINT Properties3A[] =
    {
        PropertyIDAECFrameControl,              // 0
        PropertyIDAWBFrameControl,              // 1
        PropertyIDAECStatsControl,              // 2
        PropertyIDAWBStatsControl,              // 3
        PropertyIDAFStatsControl,               // 4
        PropertyIDAFDStatsControl,              // 5
        PropertyIDPostSensorGainId,             // 6
        PropertyIDISPBHistConfig,               // 7
        PropertyIDISPTintlessBGConfig,          // 8
        PropertyIDParsedTintlessBGStatsOutput,  // 9
        PropertyIDParsedBHistStatsOutput,       // 10
        PropertyIDAFFrameControl,               // 11
        PropertyIDSensorNumberOfLEDs,           // 12
        PropertyIDPDHwConfig,                   // 13
    };

    const SIZE_T numTags                 = CAMX_ARRAY_SIZE(Properties3A);
    VOID*  pPropertyData3A[numTags]      = { 0 };

    UINT64 propertyData3AOffset[numTags] = { 0 };

    // Need to read from the frame matching the buffer
    propertyData3AOffset[7] = GetMaximumPipelineDelay();
    propertyData3AOffset[8] = GetMaximumPipelineDelay();

    GetDataList(Properties3A, pPropertyData3A, propertyData3AOffset, numTags);

    if (NULL != pPropertyData3A[0])
    {
        Utils::Memcpy(&pFrameConfig->AECUpdateData,      pPropertyData3A[0], sizeof(pFrameConfig->AECUpdateData));
    }
    if (NULL != pPropertyData3A[1])
    {
        Utils::Memcpy(&pFrameConfig->AWBUpdateData,      pPropertyData3A[1], sizeof(pFrameConfig->AWBUpdateData));
    }
    if (NULL != pPropertyData3A[2])
    {
        Utils::Memcpy(&pFrameConfig->AECStatsUpdateData, pPropertyData3A[2], sizeof(pFrameConfig->AECStatsUpdateData));

        CAMX_LOG_VERBOSE(CamxLogGroupISP, "<HDRBE> req %llu from 3A HNum %d ROI %d %d %d %d",
            requestId,
            pFrameConfig->AECStatsUpdateData.statsConfig.BEConfig.horizontalNum,
            pFrameConfig->AECStatsUpdateData.statsConfig.BEConfig.ROI.left,
            pFrameConfig->AECStatsUpdateData.statsConfig.BEConfig.ROI.top,
            pFrameConfig->AECStatsUpdateData.statsConfig.BEConfig.ROI.width,
            pFrameConfig->AECStatsUpdateData.statsConfig.BEConfig.ROI.height);

        if ((pFrameConfig->AECStatsUpdateData.statsConfig.BEConfig.ROI.width < HDRBEStatsROIMinWidth) ||
            (pFrameConfig->AECStatsUpdateData.statsConfig.BEConfig.ROI.height < HDRBEStatsROIMinHeight))
        {
            pFrameConfig->AECStatsUpdateData.statsConfig.BEConfig.ROI.width = (
                (pFrameConfig->AECStatsUpdateData.statsConfig.BEConfig.ROI.width < HDRBEStatsROIMinWidth) ?
                HDRBEStatsROIMinWidth : pFrameConfig->AECStatsUpdateData.statsConfig.BEConfig.ROI.width);
            pFrameConfig->AECStatsUpdateData.statsConfig.BEConfig.ROI.height = (
                (pFrameConfig->AECStatsUpdateData.statsConfig.BEConfig.ROI.height < HDRBEStatsROIMinHeight) ?
                HDRBEStatsROIMinHeight : pFrameConfig->AECStatsUpdateData.statsConfig.BEConfig.ROI.height);

            CAMX_LOG_VERBOSE(CamxLogGroupISP, "<HDRBE> changed 3A reg %llu HNum %d ROI %d %d %d %d",
                requestId,
                pFrameConfig->AECStatsUpdateData.statsConfig.BEConfig.horizontalNum,
                pFrameConfig->AECStatsUpdateData.statsConfig.BEConfig.ROI.left,
                pFrameConfig->AECStatsUpdateData.statsConfig.BEConfig.ROI.top,
                pFrameConfig->AECStatsUpdateData.statsConfig.BEConfig.ROI.width,
                pFrameConfig->AECStatsUpdateData.statsConfig.BEConfig.ROI.height);
        }

    }
    if (NULL != pPropertyData3A[3])
    {
        Utils::Memcpy(&pFrameConfig->AWBStatsUpdateData, pPropertyData3A[3], sizeof(pFrameConfig->AWBStatsUpdateData));
    }
    if (NULL != pPropertyData3A[4])
    {
        if ((reinterpret_cast<AFStatsControl*>(pPropertyData3A[4]))->statsConfig.
            BFStats.BFStatsROIConfig.numBFStatsROIDimension > 0)
        {
            Utils::Memcpy(&pFrameConfig->AFStatsUpdateData,  pPropertyData3A[4], sizeof(pFrameConfig->AFStatsUpdateData));
            Utils::Memcpy(&m_previousAFstatsControl, &pFrameConfig->AFStatsUpdateData, sizeof(m_previousAFstatsControl));
        }
        else
        {
            if (requestId <= FirstValidRequestId)
            {
                pFrameConfig->CAMIFCrop = m_ISPInputSensorData.CAMIFCrop;
                HardcodeSettingsBFStats(pFrameConfig);
                Utils::Memcpy(&m_previousAFstatsControl, &pFrameConfig->AFStatsUpdateData, sizeof(m_previousAFstatsControl));
                CAMX_LOG_INFO(CamxLogGroupISP, "Invalid BF config use hardcode for requestId %llu",
                    requestId);
            }
            else
            {
                Utils::Memcpy(&pFrameConfig->AFStatsUpdateData, &m_previousAFstatsControl,
                    sizeof(pFrameConfig->AFStatsUpdateData));
                CAMX_LOG_INFO(CamxLogGroupISP, "Invalid BF config use previous config requestId %llu",
                    requestId);
            }
        }
        CAMX_LOG_INFO(CamxLogGroupISP, "ROI %d first vaild %d (%d %d %d %d)",
            pFrameConfig->AFStatsUpdateData.statsConfig.BFStats.BFStatsROIConfig.numBFStatsROIDimension,
            pFrameConfig->AFStatsUpdateData.statsConfig.BFStats.BFStatsROIConfig.BFStatsROIDimension[0].isValid,
            pFrameConfig->AFStatsUpdateData.statsConfig.BFStats.BFStatsROIConfig.BFStatsROIDimension[0].ROI.left,
            pFrameConfig->AFStatsUpdateData.statsConfig.BFStats.BFStatsROIConfig.BFStatsROIDimension[0].ROI.top,
            pFrameConfig->AFStatsUpdateData.statsConfig.BFStats.BFStatsROIConfig.BFStatsROIDimension[0].ROI.width,
            pFrameConfig->AFStatsUpdateData.statsConfig.BFStats.BFStatsROIConfig.BFStatsROIDimension[0].ROI.height);
    }
    if (NULL != pPropertyData3A[5])
    {
        Utils::Memcpy(&pFrameConfig->AFDStatsUpdateData, pPropertyData3A[5], sizeof(pFrameConfig->AFDStatsUpdateData));
    }
    if (NULL != pPropertyData3A[6])
    {
        m_ISPInputSensorData.dGain = *reinterpret_cast<FLOAT*>(pPropertyData3A[6]);

        if (pFrameConfig->AECUpdateData.digitalGainForSimulation >= 1.0f)
        {
            m_ISPInputSensorData.dGain *= pFrameConfig->AECUpdateData.digitalGainForSimulation;
        }
    }

    CAMX_LOG_INFO(CamxLogGroupISP,
                     "Applying Stats Update. AWB Gain(R:%f, G : %f, B : %f) CCT(%u).AEC Gain(%f) ExpTime(%llu)",
                     pFrameConfig->AWBUpdateData.AWBGains.rGain,
                     pFrameConfig->AWBUpdateData.AWBGains.gGain,
                     pFrameConfig->AWBUpdateData.AWBGains.bGain,
                     pFrameConfig->AWBUpdateData.colorTemperature,
                     pFrameConfig->AECUpdateData.exposureInfo[ExposureIndexShort].linearGain,
                     pFrameConfig->AECUpdateData.exposureInfo[ExposureIndexShort].exposureTime);

    CAMX_LOG_INFO(CamxLogGroupISP,
                     "IFE: Processing for ReqId=%llu G:ET=%f : %llu LUX=%f ISPDigitalGain=%f",
                     requestId,
                     pFrameConfig->AECUpdateData.exposureInfo[0].linearGain,
                     pFrameConfig->AECUpdateData.exposureInfo[0].exposureTime,
                     pFrameConfig->AECUpdateData.luxIndex,
                     m_ISPInputSensorData.dGain);

    UINT64 requestIdOffsetFromLastFlush = GetRequestIdOffsetFromLastFlush(requestId);

    if (GetMaximumPipelineDelay() < requestIdOffsetFromLastFlush)
    {
        if (NULL != pPropertyData3A[7])
        {
            Utils::Memcpy(&(pFrameConfig->statsDataForISP.BHistConfig),
                          pPropertyData3A[7],
                          sizeof(pFrameConfig->statsDataForISP.BHistConfig));
        }

        if (NULL != pPropertyData3A[8])
        {
            Utils::Memcpy(&(pFrameConfig->statsDataForISP.tintlessBGConfig),
                          pPropertyData3A[8],
                          sizeof(pFrameConfig->statsDataForISP.tintlessBGConfig));
        }

        if (NULL != pPropertyData3A[9])
        {
            pFrameConfig->statsDataForISP.pParsedTintlessBGStats =
                *(reinterpret_cast<ParsedTintlessBGStatsOutput**>(pPropertyData3A[9]));
            CAMX_LOG_VERBOSE(CamxLogGroupISP, "TLBG Config %d %d | %d TLBG stats %x %x %x ",
                pFrameConfig->statsDataForISP.tintlessBGConfig.tintlessBGConfig.ROI.height,
                pFrameConfig->statsDataForISP.tintlessBGConfig.tintlessBGConfig.ROI.width,
                pFrameConfig->statsDataForISP.pParsedTintlessBGStats->m_numOfRegions,
                pFrameConfig->statsDataForISP.pParsedTintlessBGStats->GetTintlessBGStatsInfo(0),
                pFrameConfig->statsDataForISP.pParsedTintlessBGStats->GetTintlessBGStatsInfo(0),
                pFrameConfig->statsDataForISP.pParsedTintlessBGStats->GetTintlessBGStatsInfo(0));
        }

        if (NULL != pPropertyData3A[10])
        {
            pFrameConfig->statsDataForISP.pParsedBHISTStats =
                *(reinterpret_cast<ParsedBHistStatsOutput**>(pPropertyData3A[10]));
            CAMX_LOG_VERBOSE(CamxLogGroupISP, "BHIST Config %d %d %d | %d %d BHIST stats %x %x %x",
                             pFrameConfig->statsDataForISP.BHistConfig.BHistConfig.channel,
                             pFrameConfig->statsDataForISP.BHistConfig.BHistConfig.uniform,
                             pFrameConfig->statsDataForISP.BHistConfig.numBins,
                             pFrameConfig->statsDataForISP.pParsedBHISTStats->channelType,
                             pFrameConfig->statsDataForISP.pParsedBHISTStats->numBins,
                             pFrameConfig->statsDataForISP.pParsedBHISTStats->BHistogramStats[0],
                             pFrameConfig->statsDataForISP.pParsedBHISTStats->BHistogramStats[1],
                             pFrameConfig->statsDataForISP.pParsedBHISTStats->BHistogramStats[2]);
        }
    }

    if (NULL != pPropertyData3A[11])
    {
        Utils::Memcpy(&pFrameConfig->AFUpdateData, pPropertyData3A[11], sizeof(pFrameConfig->AFUpdateData));
    }

    if (NULL != pPropertyData3A[12])
    {
        pModuleInput->numberOfLED = *reinterpret_cast<UINT16*>(pPropertyData3A[12]);
        CAMX_LOG_VERBOSE(CamxLogGroupISP, "Number of led %d", pModuleInput->numberOfLED);
    }

    if (NULL != pPropertyData3A[13])
    {
        Utils::Memcpy(&pFrameConfig->pdHwConfig, pPropertyData3A[13], sizeof(PDHwConfig));
    }

    CAMX_LOG_INFO(CamxLogGroupISP, "IFE: Processing for ReqId=%llu requestIdOffsetFromLastFlush %llu ", requestId,
        requestIdOffsetFromLastFlush);

    return CamxResultSuccess;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::ExecuteProcessRequest
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFENode::ExecuteProcessRequest(
    ExecuteProcessRequestData* pExecuteProcessRequestData)
{
    CAMX_ASSERT(NULL != pExecuteProcessRequestData);
    CAMX_ASSERT(NULL != pExecuteProcessRequestData->pNodeProcessRequestData);
    CAMX_ASSERT(NULL != pExecuteProcessRequestData->pNodeProcessRequestData->pCaptureRequest);
    CAMX_ASSERT(NULL != pExecuteProcessRequestData->pTuningModeData);

    CamxResult              result                   = CamxResultSuccess;
    BOOL                    hasExplicitDependencies  = TRUE;
    NodeProcessRequestData* pNodeRequestData         = pExecuteProcessRequestData->pNodeProcessRequestData;
    const StaticSettings*   pSettings                = HwEnvironment::GetInstance()->GetStaticSettings();

    INT32 sequenceNumber = pNodeRequestData->processSequenceId;

    // For RDI only output case or for raw output only case, do not use 3A stats dependency check
    if ((FALSE == m_useStatsAlgoConfig) || (TRUE == m_RDIOnlyUseCase) ||
        (TRUE  == m_OEMIQSettingEnable) || (TRUE == m_OEMStatsConfig))
    {
        hasExplicitDependencies = FALSE;
    }

    if (0 == sequenceNumber)
    {
        if (TRUE  == IsFSSnapshot(pExecuteProcessRequestData, pNodeRequestData->pCaptureRequest->requestId))
        {
            // Treat FS snapshot as RDI only usecase i.e. don't set any dependencies
            hasExplicitDependencies = FALSE;
        }

        // If the sequence number is zero then it means we are not called from the DRQ, in which case we need to set our
        // dependencies.
        SetDependencies(pNodeRequestData, hasExplicitDependencies);

        // If no dependency, it should do process directly. Set sequneceNumber to 1 to do process directly
        // Or if no stats node, the first request will not be called.
        if (FALSE == Node::HasAnyDependency(pNodeRequestData->dependencyInfo))
        {
            sequenceNumber = 1;
        }
    }

    // IMPORTANT:
    // Ensure that no stateful (member) data for this node is modified before __ALL__ dependencies are met. Only
    // Member data that is NOT dependent on per-frame controls/request may be modified before dependencies are met.

    // We are able to process if the sequenceID == 1 (all dependencies are met)
    if ((CamxResultSuccess == result) && (1 == sequenceNumber) && (NULL  != m_pSensorModeData))
    {
        ISPInputData        moduleInput;
        UINT32              cameraId                = 0;
        UINT32              numberOfCamerasRunning  = 0;
        UINT32              currentCameraId         = 0;
        BOOL                isMultiCameraUsecase    = TRUE;
        BOOL                isMasterCamera          = TRUE;
        IFETuningMetadata*  pTuningMetadata         = NULL ;

        UINT64 requestIdOffsetFromLastFlush = GetRequestIdOffsetFromLastFlush(pNodeRequestData->pCaptureRequest->requestId);

        Utils::Memset(&moduleInput, 0, sizeof(moduleInput));
        Utils::Memset(&m_frameConfig, 0, sizeof(m_frameConfig));

        cameraId = GetPipeline()->GetCameraId();

        GetMultiCameraInfo(&isMultiCameraUsecase, &numberOfCamerasRunning, &currentCameraId, &isMasterCamera);

        if (TRUE == isMultiCameraUsecase)
        {
            cameraId = currentCameraId;
            CAMX_LOG_VERBOSE(CamxLogGroupISP,
                             "instance=%p, cameraId = %d, currentCameraId = %d, isMasterCamera = %d,"
                             " isMultiCameraUsecase=%d, numberOfCamerasRunning=%u",
                             this, cameraId, currentCameraId, isMasterCamera, isMultiCameraUsecase, numberOfCamerasRunning);
        }

        // Assign Tuning-data pointer only if needed or available.
        pTuningMetadata = (TRUE == isMasterCamera) ? m_pTuningMetadata : NULL;

        moduleInput.pOTPData      = m_pOTPData;

        moduleInput.pIFEOutputPathInfo = m_IFEOutputPathInfo;

        // Since OEM IQ settings come from Inputpool, data always avail, grab unconditionally
        if (TRUE == m_OEMIQSettingEnable)
        {
            moduleInput.pOEMIQSetting = NULL;
            result = GetOEMIQSettings(&moduleInput.pOEMIQSetting);
        }

        if (TRUE == m_OEMStatsConfig && CamxResultSuccess == result)
        {
            result = GetOEMStatsConfig(&m_frameConfig);
        }

        // If we actually have dependencies we can get data when the sequence number is 1
        // meaning we were called from the DRQ
        if ((CamxResultSuccess == result) && (TRUE == hasExplicitDependencies))
        {
            result = Get3AFrameConfig(&moduleInput, &m_frameConfig, pNodeRequestData->pCaptureRequest->requestId);
        }

        if (CamxResultSuccess == result)
        {
            GetTintlessStatus(&moduleInput);
        }

        if (CamxResultSuccess == result)
        {
            result = FetchCmdBuffers(pNodeRequestData->pCaptureRequest->requestId);

            if ((CamxResultSuccess == result) &&
                (FALSE             == m_RDIOnlyUseCase))
            {
                moduleInput.pTuningDataManager  = GetTuningDataManager();

                // Setup the Input data for IQ Parameter
                moduleInput.frameNum                        = pNodeRequestData->pCaptureRequest->requestId;
                moduleInput.pAECUpdateData                  = &m_frameConfig.AECUpdateData;
                moduleInput.pAECStatsUpdateData             = &m_frameConfig.AECStatsUpdateData;
                moduleInput.pAWBUpdateData                  = &m_frameConfig.AWBUpdateData;
                moduleInput.pAWBStatsUpdateData             = &m_frameConfig.AWBStatsUpdateData;
                moduleInput.pAFUpdateData                   = &m_frameConfig.AFUpdateData;
                moduleInput.pAFStatsUpdateData              = &m_frameConfig.AFStatsUpdateData;
                moduleInput.pAFDStatsUpdateData             = &m_frameConfig.AFDStatsUpdateData;
                moduleInput.pIHistStatsUpdateData           = &m_frameConfig.IHistStatsUpdateData;
                moduleInput.pPDHwConfig                     = &m_frameConfig.pdHwConfig;
                moduleInput.pCSStatsUpdateData              = &m_frameConfig.CSStatsUpdateData;
                moduleInput.pStripeConfig                   = &m_frameConfig;
                moduleInput.p32bitDMIBuffer                 = m_p32bitDMIBuffer;
                moduleInput.p64bitDMIBuffer                 = m_p64bitDMIBuffer;
                moduleInput.p32bitDMIBufferAddr             = m_p32bitDMIBufferAddr;
                moduleInput.p64bitDMIBufferAddr             = m_p64bitDMIBufferAddr;
                moduleInput.pHwContext                      = GetHwContext();
                moduleInput.pHALTagsData                    = &m_HALTagsData;
                moduleInput.disableManual3ACCM              = m_disableManual3ACCM;
                moduleInput.sensorID                        = cameraId;
                moduleInput.HVXData                         = m_HVXInputData.HVXConfiguration;
                moduleInput.sensorBitWidth                  = m_pSensorCaps->sensorConfigs[0].streamConfigs[0].bitWidth;
                moduleInput.sensorData.fullResolutionWidth  = m_ISPInputSensorData.fullResolutionWidth;
                moduleInput.sensorData.fullResolutionHeight = m_ISPInputSensorData.fullResolutionHeight;
                moduleInput.useHardcodedRegisterValues      = CheckToUseHardcodedRegValues(moduleInput.pHwContext);
                moduleInput.enableIFEDualStripeLog          = CheckToEnableDualIFEStripeInfo(moduleInput.pHwContext);
                moduleInput.pIFETuningMetadata              = pTuningMetadata;
                moduleInput.pTuningData                     = pExecuteProcessRequestData->pTuningModeData;
                moduleInput.tuningModeChanged               = ISPIQModule::IsTuningModeDataChanged(
                                                                  moduleInput.pTuningData,
                                                                  &m_tuningData);
                moduleInput.dumpRegConfig                   = GetStaticSettings()->logVerboseMask;
                moduleInput.registerBETEn                   = FALSE;
                moduleInput.maxOutputWidthFD                = m_maxOutputWidthFD;
                moduleInput.titanVersion                    = m_titanVersion;
                moduleInput.maximumPipelineDelay            = GetMaximumPipelineDelay();

                if (FirstValidRequestId >= requestIdOffsetFromLastFlush)
                {
                    moduleInput.forceTriggerUpdate = TRUE;
                }

                // Update the Tintless Algo processing requirement
                if (TRUE == moduleInput.tintlessEnable)
                {
                    moduleInput.skipTintlessProcessing = CanSkipAlgoProcessing(&moduleInput);
                }

                // Cache tuning mode selector data for comparison for next frame, to help
                // optimize tuning data (tree) search in the IQ modules
                if (TRUE == moduleInput.tuningModeChanged)
                {
                    Utils::Memcpy(&m_tuningData, moduleInput.pTuningData, sizeof(ChiTuningModeParameter));
                }

                moduleInput.pipelineIFEData.moduleMode = m_mode;
                if (TRUE == m_libInitialData.isSucceed)
                {
                    moduleInput.pLibInitialData = m_libInitialData.pLibData;
                }

                // Disable the CAMIF Program
                if ((TRUE == m_initialConfigPending) && (FALSE == IsPipelineStreamedOn()))
                {
                    moduleInput.pipelineIFEData.programCAMIF     = TRUE;
                    moduleInput.pipelineIFEData.numBatchedFrames = m_usecaseNumBatchedFrames;

                    moduleInput.dualPDPipelineData.programCAMIFLite = TRUE;
                    moduleInput.dualPDPipelineData.numBatchedFrames = m_usecaseNumBatchedFrames;
                }
                else
                {
                    moduleInput.pipelineIFEData.programCAMIF        = FALSE;
                    moduleInput.dualPDPipelineData.programCAMIFLite = FALSE;
                }
                // Get HAL tags
                result = GetMetadataTags(&moduleInput);

                if (CamxResultSuccess == result)
                {
                    BOOL initAll = FALSE;
                    if ((FALSE == m_useStatsAlgoConfig) && (FALSE == m_OEMIQSettingEnable) && (FALSE == m_OEMStatsConfig))
                    {
                        initAll = TRUE;
                    }
                    HardcodeSettings(&moduleInput, &m_frameConfig, initAll);

                    HardcodeTintlessSettings(&moduleInput);

                    // Call IQInterface to Set up the Trigger data
                    IQInterface::IQSetupTriggerData(&moduleInput, this, TRUE , NULL);

                    // Update Dual IFE IQ module config
                    if (IFEModuleMode::DualIFENormal == m_mode)
                    {
                        // Store bankSelect for interpolation to store mesh_table_l[bankSelect] and mesh_table_r[bankSelect]
                        m_frameConfig.stateLSC.dependenceData.bankSelect =
                            m_stripeConfigs[0].stateLSC.dependenceData.bankSelect;
                        m_frameConfig.pFrameLevelData = &m_ISPFrameData;
                        moduleInput.pStripeConfig   = &m_frameConfig;
                        moduleInput.pCalculatedData = &m_ISPFramelevelData;
                        if (FALSE == pSettings->enableDualIFEOverwrite)
                        {
                            moduleInput.pStripingInput  = m_pStripingInput;
                            result = PrepareStripingParameters(&moduleInput);
                        }
                        if (CamxResultSuccess == result)
                        {
                            // Release stripes (if any)
                            // Note this will later be added to striping
                            // lib as a function so that SW doesn't care about internals of that lib!
                            if (IFEModuleMode::DualIFENormal == m_mode)
                            {
                                if ((NULL != m_pPassOut) && (NULL != m_pPassOut->hStripeList.pListHead))
                                {
                                    ListNode* pNode1 = reinterpret_cast<ListNode *>(m_pPassOut->hStripeList.pListHead);
                                    ListNode* pNode2 = pNode1->pNextNode;
                                    CAMX_FREE(pNode1->pData);
                                    CAMX_FREE(pNode1);
                                    CAMX_FREE(pNode2->pData);
                                    CAMX_FREE(pNode2);
                                    m_pPassOut->hStripeList = {NULL};
                                }
                            }
                            DualIFEUtils::UpdateDualIFEConfig(&moduleInput,
                                                              m_PDAFInfo,
                                                              m_stripeConfigs,
                                                              &m_dualIFESplitParams,
                                                              m_pPassOut);
                        }
                    }
                    else
                    {
                        // Each module has stripe specific configuration that needs to be updated before calling upon module
                        // Execute function. In single IFE case, there's only one stripe and hence we will use
                        // m_stripeConfigs[0] to hold the input configuration
                        UpdateIQStateConfiguration(&m_frameConfig, &m_stripeConfigs[0]);
                        moduleInput.pStripeConfig   = &m_frameConfig;
                    }

                    // Execute IQ module configuration and updated module enable info
                    result = ProgramIQConfig(&moduleInput);
                }

                if (CamxResultSuccess == result)
                {
                    result = ProgramIQEnable(&moduleInput);
                }

                // Program output split info for dual IFE stripe
                if ((CamxResultSuccess == result) && (IFEModuleMode::DualIFENormal == m_mode))
                {
                    result = ProgramStripeConfig();
                    DualIFEUtils::ReleaseDualIfePassResult(m_pPassOut);
                }

                // Post metadata from IQ modules to metadata
                if (CamxResultSuccess == result)
                {
                    result = PostMetadata(&moduleInput);
                }

                // Update tuning metadata if setting is enabled
                if ((NULL               != moduleInput.pIFETuningMetadata)  &&
                    (TRUE               == isMasterCamera)                  &&
                    (CamxResultSuccess  == result))
                {
                    // Only use debug data on the master camera
                    DumpTuningMetadata(&moduleInput);
                }
            }
            else if (TRUE == m_RDIOnlyUseCase)
            {
                // For RDI only use-case, some of CTS test case expects tag in metadata.
                result = PostMetadataRaw();
            }
        }

        // Update bandwidth config if there is a change in enabled ports for this request
        if (CamxResultSuccess == result)
        {
            result = SetupResourceBWConfig(pExecuteProcessRequestData, pNodeRequestData->pCaptureRequest->requestId);
        }

        if (CamxResultSuccess == result)
        {
            if (FALSE == IsPipelineStreamedOn())
            {
                if (TRUE == m_initialConfigPending)
                {
                    m_initialConfigPending = FALSE;

                    if (IFEModuleMode::DualIFENormal == m_mode)
                    {
                        result = AddCmdBufferReference(pNodeRequestData->pCaptureRequest->requestId,
                                                       CSLPacketOpcodesDualIFEInitialConfig);
                    }
                    else
                    {
                        result = AddCmdBufferReference(pNodeRequestData->pCaptureRequest->requestId,
                                                       CSLPacketOpcodesIFEInitialConfig);
                    }
                }
                else
                {
                    if (IFEModuleMode::DualIFENormal == m_mode)
                    {
                        result = AddCmdBufferReference(pNodeRequestData->pCaptureRequest->requestId,
                                                       CSLPacketOpcodesDualIFEUpdate);
                    }
                    else
                    {
                        result = AddCmdBufferReference(pNodeRequestData->pCaptureRequest->requestId,
                                                       CSLPacketOpcodesIFEUpdate);
                    }
                }
            }
            else
            {
                m_initialConfigPending = TRUE;
                if (IFEModuleMode::DualIFENormal == m_mode)
                {
                    result = AddCmdBufferReference(pNodeRequestData->pCaptureRequest->requestId, CSLPacketOpcodesDualIFEUpdate);
                }
                else
                {
                    result = AddCmdBufferReference(pNodeRequestData->pCaptureRequest->requestId, CSLPacketOpcodesIFEUpdate);
                }
            }
        }

        // Update the IO buffers to the request
        if (CamxResultSuccess == result)
        {
            result = ConfigBufferIO(pExecuteProcessRequestData);
        }

        // Commit DMI command buffers and do a CSL submit of the request
        if (CamxResultSuccess == result)
        {
            result = CommitAndSubmitPacket();
            if (CamxResultSuccess == result)
            {
                CAMX_LOG_INFO(CamxLogGroupISP, "IFE:%d Submitted packets with requestId = %llu", InstanceID(),
                    pExecuteProcessRequestData->pNodeProcessRequestData->pCaptureRequest->requestId);
            }
            else if (CamxResultECancelledRequest == result)
            {
                CAMX_LOG_INFO(CamxLogGroupISP, "IFE:%d Submit packet canceled for requestId = %llu as session is in"
                    " Flush state", InstanceID(),
                    pExecuteProcessRequestData->pNodeProcessRequestData->pCaptureRequest->requestId);
            }
        }
    }

    // Release stripes (if any)
    // Note this will later be added to striping lib as a function so that SW doesn't care about internals of that lib!
    if (IFEModuleMode::DualIFENormal == m_mode)
    {
        if ((NULL != m_pPassOut) && (NULL != m_pPassOut->hStripeList.pListHead))
        {
            ListNode* pNode1 = reinterpret_cast<ListNode *>(m_pPassOut->hStripeList.pListHead);
            ListNode* pNode2 = pNode1->pNextNode;
            CAMX_FREE(pNode1->pData);
            CAMX_FREE(pNode1);
            CAMX_FREE(pNode2->pData);
            CAMX_FREE(pNode2);
            m_pPassOut->hStripeList = {NULL};
        }
    }
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::SetupDeviceResource
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFENode::SetupDeviceResource()
{
    CamxResult          result                          = CamxResultSuccess;
    SIZE_T              resourceSize                    = 0;
    ISPInResourceInfo*  pInputResource                  = NULL;
    UINT                outputPortId[MaxIFEOutputPorts];
    UINT                totalOutputPort                 = 0;
    UINT                inputPortId[MaxIFEInputPorts];
    UINT                totalInputPort                  = 0;
    UINT                IFEInputResourceSize            = 0;

    m_numResource                                       = 0;

    /// @todo (CAMX-1315)   Add logic for dual IFE and fall back on single IFE if dual fails.

    // Get Input Port List
    /// @todo (CAMX-1015) Get this only once in ProcessingNodeInitialize() and save it off in the IFENode class
    GetAllInputPortIds(&totalInputPort, &inputPortId[0]);

    // Get Output Port List
    GetAllOutputPortIds(&totalOutputPort, &outputPortId[0]);

    if (CamxResultSuccess == result)
    {
        if (totalOutputPort <= 0)
        {
            CAMX_LOG_ERROR(CamxLogGroupISP, "Error: no output port");
            result = CamxResultEInvalidArg;
        }
    }

    result = GetFPS();

    if (CamxResultSuccess == result)
    {
        // Set up pixel channel (including RAW) resource list
        pInputResource = NULL;
        resourceSize   = 0;

        UINT perChannelOutputPortId[MaxIFEOutputPorts];
        UINT perChannelFEOutputPortId[MaxIFEOutputPorts];
        UINT perChannelTotalOutputPort;
        UINT perChannelFETotalOutputPort;

        IFEInputResourceSize = (m_pSensorModeData->streamConfigCount * sizeof(ISPInResourceInfo)) +
                               (totalOutputPort * sizeof(ISPOutResourceInfo));
        // In FS mode, IFE Bus Read is provided as an additional input resource
        if (TRUE == m_enableBusRead)
        {
            IFEInputResourceSize += sizeof(ISPInResourceInfo);
        }
        m_IFEResourceSize    = IFEInputResourceSize + sizeof(ISPAcquireHWInfo);
        m_pIFEResourceInfo   = static_cast<ISPAcquireHWInfo*>(CAMX_CALLOC(m_IFEResourceSize));

        if (NULL != m_pIFEResourceInfo)
        {
            // For each port source type, setup the output ports.
            for (UINT streamIndex = 0; streamIndex < m_pSensorModeData->streamConfigCount; streamIndex++)
            {
                StreamType  streamType                  = m_pSensorModeData->streamConfig[streamIndex].type;
                const UINT  inputResourcePortSourceType = TranslateSensorStreamConfigTypeToPortSourceType(streamType);

                CAMX_LOG_VERBOSE(CamxLogGroupISP, "Port source type = %u streamType=%u",
                    inputResourcePortSourceType, streamType);

                perChannelTotalOutputPort = 0;
                perChannelFETotalOutputPort = 0;

                // Filter the output port that matches with the current input port source type
                for (UINT outputPortIndex = 0; outputPortIndex < totalOutputPort; outputPortIndex++)
                {
                    const UINT currentOutputPortSourceTypeId =
                        GetOutputPortSourceType(OutputPortIndex(outputPortId[outputPortIndex]));

                    // If input resource is of Pixel type, the output port source type can be either Undefined or Pixel.
                    if ((PortSrcTypePixel == inputResourcePortSourceType) &&
                        ((PortSrcTypeUndefined == currentOutputPortSourceTypeId) ||
                         (PortSrcTypePixel     == currentOutputPortSourceTypeId)))
                    {
                        // In FS mode configure only the RDI ports to Image stream
                        // Rest of the output ports should be configured to BusRead Input
                        if (TRUE == m_enableBusRead)
                        {
                            if ((IFEOutputPortRDI0 == outputPortId[outputPortIndex]) ||
                                (IFEOutputPortRDI1 == outputPortId[outputPortIndex]) ||
                                (IFEOutputPortRDI2 == outputPortId[outputPortIndex]) ||
                                (IFEOutputPortRDI3 == outputPortId[outputPortIndex]))
                            {
                                perChannelOutputPortId[perChannelTotalOutputPort] = outputPortId[outputPortIndex];
                                perChannelTotalOutputPort++;
                            }
                            else
                            {
                                perChannelFEOutputPortId[perChannelFETotalOutputPort] = outputPortId[outputPortIndex];
                                perChannelFETotalOutputPort++;
                            }
                        }
                        else
                        {
                            perChannelOutputPortId[perChannelTotalOutputPort] = outputPortId[outputPortIndex];
                            perChannelTotalOutputPort++;
                        }
                    }
                    else if (inputResourcePortSourceType == currentOutputPortSourceTypeId)
                    {
                        perChannelOutputPortId[perChannelTotalOutputPort] = outputPortId[outputPortIndex];
                        perChannelTotalOutputPort++;
                    }
                }

                // Ignore for the sensor stream types that cannot be mapped to any IFE output port.
                if (perChannelTotalOutputPort > 0)
                {
                    pInputResource = reinterpret_cast<ISPInResourceInfo*>
                                    (reinterpret_cast<UINT8*>(&m_pIFEResourceInfo->data) + resourceSize);
                    result = SetupChannelResource(inputPortId[0],
                                                  perChannelTotalOutputPort,
                                                  &perChannelOutputPortId[0],
                                                  inputResourcePortSourceType,
                                                  pInputResource);
                    resourceSize += sizeof(ISPInResourceInfo) + (sizeof(ISPOutResourceInfo) * (perChannelTotalOutputPort - 1));

                    if (CamxResultSuccess == result)
                    {
                        m_numResource++;
                    }
                }

                if ((TRUE == m_enableBusRead)          &&
                    (perChannelFETotalOutputPort > 0) &&
                    (StreamType::IMAGE == streamType))
                {
                    CAMX_LOG_INFO(CamxLogGroupISP, "FS: perChannelTotalOutputPort:%d perChannelFETotalOutputPort:%d",
                                  perChannelTotalOutputPort, perChannelFETotalOutputPort);
                    pInputResource = reinterpret_cast<ISPInResourceInfo*>
                        (reinterpret_cast<UINT8*>(&m_pIFEResourceInfo->data) + resourceSize);
                    pInputResource->resourceType = IFEInputBusRead;
                    result = SetupChannelResource(inputPortId[0],
                                                  perChannelFETotalOutputPort,
                                                  &perChannelFEOutputPortId[0],
                                                  inputResourcePortSourceType,
                                                  pInputResource);
                    resourceSize += sizeof(ISPInResourceInfo) +
                                    (sizeof(ISPOutResourceInfo) * (perChannelFETotalOutputPort - 1));

                    if (CamxResultSuccess == result)
                    {
                        m_numResource++;
                    }
                }
            }

            // Update the IFE resource structure so that KMD will decide which structure to use whicle acquiriing.
            m_pIFEResourceInfo->commonInfoVersion = ISPAcquireCommonVersion0;
            m_pIFEResourceInfo->commonInfoSize    = 0;
            m_pIFEResourceInfo->commonInfoOffset  = 0;
            m_pIFEResourceInfo->numInputs         = m_numResource;
            m_pIFEResourceInfo->inputInfoVersion  = ISPAcquireInputVersion0;
            m_pIFEResourceInfo->inputInfoSize     = IFEInputResourceSize;
            m_pIFEResourceInfo->inputInfoOffset   = 0;
        }
        else
        {
            CAMX_LOG_ERROR(CamxLogGroupISP, "Calloc for m_pIFEResourceInfo failed!")
            result = CamxResultENoMemory;
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::SetupChannelResource
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFENode::SetupChannelResource(
    UINT               inputPortId,
    UINT               totalOutputPorts,
    const UINT*        pPortId,
    UINT               portSourceTypeId,
    ISPInResourceInfo* pInputResource)
{
    CamxResult          result          = CamxResultSuccess;
    ISPOutResourceInfo* pOutputResource = NULL;

    /// @todo (CAMX-1315)   Add logic for dual IFE and fall back on single IFE if dual fails.
    CAMX_ASSERT(NULL != m_pSensorModeData);

    if (NULL == pInputResource)
    {
        CAMX_LOG_ERROR(CamxLogGroupISP, "Failed to allocate ResourceInfo.");
        result = CamxResultENoMemory;
    }

    if (CamxResultSuccess == result)
    {
        pInputResource->numberOutResource = totalOutputPorts;

        CAMX_UNREFERENCED_PARAM(inputPortId);

        // Setup input resource
        /// @todo (CAMX-1295) - Need to figure out the proper way to deal with TPG/SensorEmulation + RealDevice/Presil
        if ((CSLPresilEnabled == GetCSLMode())     ||
            (CSLPresilRUMIEnabled == GetCSLMode()) ||
            (TRUE == IsTPGMode()))
        {
            /// @todo (CAMX-1189) Move all the hardcode value to settings
            pInputResource->resourceType = IFEInputTestGen;
            pInputResource->VC           = 0xa;
            pInputResource->DT           = 0x2B;
            pInputResource->laneNum      = 4;
            pInputResource->laneType     = IFELaneTypeDPHY;
            pInputResource->laneConfig   = 0x3210;

            m_CSIDecodeBitWidth          = TranslateCSIDataTypeToCSIDecodeFormat(static_cast<UINT8>(pInputResource->DT));
            pInputResource->format       = TranslateFormatToISPImageFormat(CamX::Format::RawMIPI, m_CSIDecodeBitWidth);
            pInputResource->testPattern  = ISPPatternBayerRGRGRG;
            pInputResource->usageType    = ISPResourceUsageSingle;

            pInputResource->height       = m_pSensorModeData->resolution.outputHeight;
            pInputResource->lineStart    = 0;
            pInputResource->lineStop     = m_pSensorModeData->resolution.outputHeight - 1;
            pInputResource->batchSize    = 0;
            pInputResource->DSPMode      = m_HVXInputData.DSPMode;
            pInputResource->HBICount     = 0;
        }
        else
        {
            // PHY/Lanes should not be configured for BusRead/Fetch path
            if (IFEInputBusRead != pInputResource->resourceType)
            {
                /// @todo (CAMX-795) Following field needs to from sensor usecase data
                switch (m_pSensorModeData->CSIPHYId)
                {
                    case 0:
                        pInputResource->resourceType = IFEInputPHY0;
                        break;
                    case 1:
                        pInputResource->resourceType = IFEInputPHY1;
                        break;
                    case 2:
                        pInputResource->resourceType = IFEInputPHY2;
                        break;
                    case 3:
                        pInputResource->resourceType = IFEInputPHY3;
                        break;
                    default:
                        CAMX_LOG_VERBOSE(CamxLogGroupISP, "CSIPHY channel out of range value %d default to TestGen",
                                         m_pSensorModeData->CSIPHYId);
                        pInputResource->resourceType = IFEInputTestGen;
                        break;
                }


                pInputResource->laneType     = m_pSensorModeData->is3Phase ? IFELaneTypeCPHY : IFELaneTypeDPHY;
                pInputResource->laneNum      = m_pSensorModeData->laneCount;
                pInputResource->laneConfig   = m_pSensorCaps->CSILaneAssign;
                CAMX_LOG_VERBOSE(CamxLogGroupISP, "is3Phase %d, laneType %d laneNum 0x%x laneAssign 0x%x",
                                    m_pSensorModeData->is3Phase,
                                    pInputResource->laneType,
                                    pInputResource->laneNum,
                                    m_pSensorCaps->CSILaneAssign);
            }
            else
            {
                CAMX_LOG_INFO(CamxLogGroupISP, "FS: ResourceType configured to BusRead");
            }


            // Get VC and DT information from IMAGE stream configuration
            CAMX_LOG_VERBOSE(CamxLogGroupISP,
                             "Current sensor mode resolution output: width = %u, height = %u, streamConfigCount = %u",
                             m_pSensorModeData->resolution.outputWidth,
                             m_pSensorModeData->resolution.outputHeight,
                             m_pSensorModeData->streamConfigCount);
            for (UINT32 index = 0; index < m_pSensorModeData->streamConfigCount; index++)
            {
                CAMX_LOG_VERBOSE(CamxLogGroupISP,
                                 "streamConfig TYPE = %s, vc = 0x%02x, dt = 0x%02x, frameDim = (%d,%d,%d,%d)",
                                 m_pSensorModeData->streamConfig[index].type == StreamType::IMAGE ? "IMAGE" :
                                 m_pSensorModeData->streamConfig[index].type == StreamType::PDAF ? "PDAF" :
                                 m_pSensorModeData->streamConfig[index].type == StreamType::HDR ? "HDR" :
                                 m_pSensorModeData->streamConfig[index].type == StreamType::META ? "META" :
                                 "OTHER",
                                 m_pSensorModeData->streamConfig[index].vc,
                                 m_pSensorModeData->streamConfig[index].dt,
                                 m_pSensorModeData->streamConfig[index].frameDimension.xStart,
                                 m_pSensorModeData->streamConfig[index].frameDimension.yStart,
                                 m_pSensorModeData->streamConfig[index].frameDimension.width,
                                 m_pSensorModeData->streamConfig[index].frameDimension.height);

                if ((StreamType::IMAGE  == m_pSensorModeData->streamConfig[index].type) &&
                    ((PortSrcTypePixel == portSourceTypeId) || (PortSrcTypeUndefined == portSourceTypeId)))
                {
                    pInputResource->VC = m_pSensorModeData->streamConfig[index].vc;
                    pInputResource->DT = m_pSensorModeData->streamConfig[index].dt;

                    m_CSIDecodeBitWidth    = TranslateCSIDataTypeToCSIDecodeFormat(static_cast<UINT8>(pInputResource->DT));
                    pInputResource->format = TranslateFormatToISPImageFormat(CamX::Format::RawMIPI, m_CSIDecodeBitWidth);

                    CAMX_LOG_VERBOSE(CamxLogGroupISP,
                                     "StreamType::IMAGE setting VC / DT, vc = 0x%02x, dt = 0x%02x",
                                     pInputResource->VC,
                                     pInputResource->DT);
                    break;
                }
                if ((StreamType::PDAF   == m_pSensorModeData->streamConfig[index].type) &&
                    (PortSrcTypePDAF    == portSourceTypeId))
                {
                    pInputResource->VC     = m_pSensorModeData->streamConfig[index].vc;
                    pInputResource->DT     = m_pSensorModeData->streamConfig[index].dt;

                    const UINT bitWidth     = m_pSensorModeData->streamConfig[index].bitWidth;
                    m_PDAFCSIDecodeFormat   = TranslateBitDepthToCSIDecodeFormat(bitWidth);
                    pInputResource->format  = TranslateFormatToISPImageFormat(CamX::Format::RawMIPI, m_PDAFCSIDecodeFormat);

                    CAMX_LOG_VERBOSE(CamxLogGroupISP,
                                     "StreamType::PDAF setting VC / DT, vc = 0x%02x, dt = 0x%02x, bit depth= %u",
                                     pInputResource->VC,
                                     pInputResource->DT,
                                     bitWidth);
                    break;
                }
                if ((StreamType::META == m_pSensorModeData->streamConfig[index].type) &&
                    (PortSrcTypeMeta == portSourceTypeId))
                {
                    pInputResource->VC     = m_pSensorModeData->streamConfig[index].vc;
                    pInputResource->DT     = m_pSensorModeData->streamConfig[index].dt;

                    const UINT bitWidth     = m_pSensorModeData->streamConfig[index].bitWidth;
                    m_metaCSIDecodeFormat   = TranslateBitDepthToCSIDecodeFormat(bitWidth);
                    pInputResource->format  = TranslateFormatToISPImageFormat(CamX::Format::RawMIPI, m_metaCSIDecodeFormat);

                    CAMX_LOG_VERBOSE(CamxLogGroupISP,
                                     "StreamType::META setting VC / DT, vc = 0x%02x, dt = 0x%02x, bit depth= %u",
                                     pInputResource->VC,
                                     pInputResource->DT,
                                     bitWidth);
                    break;
                }
                if ((StreamType::HDR == m_pSensorModeData->streamConfig[index].type) &&
                    (PortSrcTypeHDR == portSourceTypeId))
                {
                    pInputResource->VC     = m_pSensorModeData->streamConfig[index].vc;
                    pInputResource->DT     = m_pSensorModeData->streamConfig[index].dt;

                    const UINT bitWidth     = m_pSensorModeData->streamConfig[index].bitWidth;
                    m_HDRCSIDecodeFormat    = TranslateBitDepthToCSIDecodeFormat(bitWidth);
                    pInputResource->format  = TranslateFormatToISPImageFormat(CamX::Format::RawMIPI, m_HDRCSIDecodeFormat);

                    CAMX_LOG_VERBOSE(CamxLogGroupISP,
                                     "StreamType::HDR setting VC / DT, vc = 0x%02x, dt = 0x%02x, bit depth= %u",
                                     pInputResource->VC,
                                     pInputResource->DT,
                                     bitWidth);
                    break;
                }
            }

            if ((PortSrcTypePixel == portSourceTypeId) || (PortSrcTypeUndefined == portSourceTypeId))
            {
                m_CSIDecodeBitWidth    = TranslateCSIDataTypeToCSIDecodeFormat(static_cast<UINT8>(pInputResource->DT));
                pInputResource->format = TranslateFormatToISPImageFormat(CamX::Format::RawMIPI, m_CSIDecodeBitWidth);
            }

            pInputResource->usageType    = ISPResourceUsageSingle;
            pInputResource->height       = m_pSensorModeData->resolution.outputHeight;
            pInputResource->lineStart    = 0;
            pInputResource->lineStop     = m_pSensorModeData->resolution.outputHeight - 1;
            pInputResource->batchSize    = 0;
            pInputResource->DSPMode      = m_HVXInputData.DSPMode;
            pInputResource->HBICount     = 0;
            pInputResource->testPattern  = TranslateColorFilterPatternToISPPattern(m_pSensorCaps->colorFilterArrangement);

            CAMX_LOG_VERBOSE(CamxLogGroupISP,
                             "InputResource: VC = %x, DT = %x, format = %d, testPattern = %d",
                             pInputResource->VC,
                             pInputResource->DT,
                             pInputResource->format,
                             pInputResource->testPattern);
        }

        if (IFEModuleMode::DualIFENormal == m_mode)
        {
            pInputResource->usageType    = ISPResourceUsageDual;
            pInputResource->leftWidth    =
                m_dualIFESplitParams.splitPoint + m_dualIFESplitParams.rightPadding;
            pInputResource->leftStart    = 0;
            pInputResource->leftStop     = pInputResource->leftWidth - 1;
            pInputResource->rightWidth   =
                m_pSensorModeData->resolution.outputWidth -
                m_dualIFESplitParams.splitPoint +
                m_dualIFESplitParams.leftPadding;
            pInputResource->rightStart   =
                m_dualIFESplitParams.splitPoint - m_dualIFESplitParams.leftPadding;
            pInputResource->rightStop    = m_pSensorModeData->resolution.outputWidth - 1;
        }
        else
        {
            // Single IFE configuration case

            // By default, CSID crop setting will use entire sensor output region
            pInputResource->leftStart   = 0;
            pInputResource->leftStop    = m_pSensorModeData->resolution.outputWidth - 1;
            pInputResource->leftWidth   = m_pSensorModeData->resolution.outputWidth;

            // Override CSID crop setting if custom setting values are provided
            if (TRUE == EnableCSIDCropOverridingForSingleIFE())
            {
                // Override horizontal CSID crop setting
                pInputResource->leftStart    = m_instanceProperty.IFECSIDLeft;
                pInputResource->leftStop     = m_instanceProperty.IFECSIDLeft + m_instanceProperty.IFECSIDWidth - 1;
                pInputResource->leftWidth    = m_instanceProperty.IFECSIDWidth;

                // Override vertical CSID crop setting
                pInputResource->height       = m_instanceProperty.IFECSIDHeight;
                pInputResource->lineStart    = m_instanceProperty.IFECSIDTop;
                pInputResource->lineStop     = m_instanceProperty.IFECSIDTop + m_instanceProperty.IFECSIDHeight - 1;

                CAMX_LOG_INFO(CamxLogGroupISP,
                              "CSID crop override: leftStart = %u, leftStop = %u, lineStart = %u, lineStop = %u",
                              pInputResource->leftStart,
                              pInputResource->leftStop,
                              pInputResource->lineStart,
                              pInputResource->lineStop);
            }
        }
    }

    // Setup output resource
    if (CamxResultSuccess == result)
    {
        UINT32  ISPFormat;
        Format  format;

        pOutputResource = reinterpret_cast<ISPOutResourceInfo*>(&(pInputResource->pDataField));

        for (UINT index = 0; index < totalOutputPorts; index++)
        {
            /// @todo (CAMX-1015) Optimize by not calling this function(because it is searching and can be avoided
            UINT currentOutputPortIndex = OutputPortIndex(pPortId[index]);

            // Set width and height from the output port image format
            const ImageFormat* pCurrentOutputPortImageFormat = GetOutputPortImageFormat(currentOutputPortIndex);
            pOutputResource->width                           = pCurrentOutputPortImageFormat->width;
            pOutputResource->height                          = pCurrentOutputPortImageFormat->height;

            if ((CSLCameraTitanVersion::CSLTitan150 == m_titanVersion) &&
                (TRUE == ImageFormatUtils::IsUBWC(static_cast<Format>(pCurrentOutputPortImageFormat->format))))
            {
                CAMX_LOG_ERROR(CamxLogGroupISP, "Node::%s : portID = %u, Image format %d not supported for titan version %x",
                               NodeIdentifierString(), pPortId[index],
                               pCurrentOutputPortImageFormat->format,
                               m_titanVersion);
                result = CamxResultEUnsupported;
                break;
            }

            if (IFEModuleMode::DualIFENormal == m_mode)
            {
                CAMX_ASSERT(0 !=  m_pSensorModeData->resolution.outputWidth);
                FLOAT temp = (static_cast<FLOAT>(pOutputResource->width) / m_pSensorModeData->resolution.outputWidth) *
                                m_dualIFESplitParams.splitPoint;
                pOutputResource->splitPoint = CamX::Utils::RoundFLOAT(temp);
            }

            /// @todo (CAMX-1820) Need to move IFE path enable logic to executeProcessCaptureRequest to support
            /// enable/Disable IFE path per frame, based on conclusion of the above jira.

            CAMX_LOG_VERBOSE(CamxLogGroupISP,
                             "Configure output portId = %u for port source type = %u",
                             pPortId[index],
                             portSourceTypeId);

            pOutputResource->secureMode            = IsOutputPortSecure(pPortId[index]);

            CAMX_LOG_VERBOSE(CamxLogGroupISP, "Output resource type %d, Dimension [%d * %d]",
                             pPortId[index],
                             pOutputResource->width,
                             pOutputResource->height);

            // Update width and height only for Full/FD/DS4/DS16 output ports.
            switch (pPortId[index])
            {
                case IFEOutputPortFD:
                    pOutputResource->resourceType = IFEOutputFD;
                    format                        = GetOutputPortImageFormat(OutputPortIndex(IFEOutputPortFD))->format;
                    ISPFormat                     = TranslateFormatToISPImageFormat(format, m_CSIDecodeBitWidth);
                    pOutputResource->format       = ISPFormat;
                    break;

                case IFEOutputPortFull:
                    pOutputResource->resourceType = IFEOutputFull;
                    format                        = GetOutputPortImageFormat(OutputPortIndex(IFEOutputPortFull))->format;
                    ISPFormat                     = TranslateFormatToISPImageFormat(format, m_CSIDecodeBitWidth);
                    pOutputResource->format       = ISPFormat;
                    break;

                case IFEOutputPortDS4:
                    pOutputResource->resourceType = IFEOutputDS4;
                    pOutputResource->format       = ISPFormatPD10;
                    break;

                case IFEOutputPortDS16:
                    pOutputResource->resourceType = IFEOutputDS16;
                    pOutputResource->format       = ISPFormatPD10;
                    break;

                case IFEOutputPortDisplayFull:
                    pOutputResource->resourceType = IFEOutputDisplayFull;
                    format                        = GetOutputPortImageFormat(OutputPortIndex(IFEOutputPortDisplayFull))->format;
                    ISPFormat                     = TranslateFormatToISPImageFormat(format, m_CSIDecodeBitWidth);
                    pOutputResource->format       = ISPFormat;
                    break;

                case IFEOutputPortDisplayDS4:
                    pOutputResource->resourceType = IFEOutputDisplayDS4;
                    pOutputResource->format       = ISPFormatPD10;
                    break;

                case IFEOutputPortDisplayDS16:
                    pOutputResource->resourceType = IFEOutputDisplayDS16;
                    pOutputResource->format       = ISPFormatPD10;
                    break;

                case IFEOutputPortCAMIFRaw:
                case IFEOutputPortLSCRaw:
                    pOutputResource->resourceType = IFEOutputRaw;
                    pOutputResource->format       = ISPFormatPlain1610;
                    break;

                case IFEOutputPortGTMRaw:
                    pOutputResource->resourceType = IFEOutputRaw;
                    pOutputResource->format       = ISPFormatRaw10Private;
                    break;

                case IFEOutputPortStatsHDRBE:
                    pOutputResource->resourceType   = IFEOutputStatsHDRBE;
                    pOutputResource->format         = ISPFormatPlain64;
                    break;

                case IFEOutputPortStatsHDRBHIST:
                    pOutputResource->resourceType   = IFEOutputStatsHDRBHIST;
                    pOutputResource->format         = ISPFormatPlain64;
                    break;

                case IFEOutputPortStatsBHIST:
                    pOutputResource->resourceType   = IFEOutputStatsBHIST;
                    pOutputResource->format         = ISPFormatPlain64;
                    break;

                case IFEOutputPortStatsAWBBG:
                    pOutputResource->resourceType   = IFEOutputStatsAWBBG;
                    pOutputResource->format         = ISPFormatPlain64;
                    break;

                case IFEOutputPortStatsBF:
                    pOutputResource->resourceType   = IFEOutputStatsBF;
                    pOutputResource->format         = ISPFormatPlain64;
                    break;

                case IFEOutputPortStatsTLBG:
                    pOutputResource->resourceType   = IFEOutputStatsTLBG;
                    pOutputResource->format         = ISPFormatPlain64;
                    break;

                case IFEOutputPortStatsDualPD:
                    pOutputResource->resourceType    = IFEOutputDualPD;
                    result                           = SetRDIOutputPortFormat(pOutputResource,
                                                                              pCurrentOutputPortImageFormat->format,
                                                                              pPortId[index],
                                                                              portSourceTypeId);
                    break;

                case IFEOutputPortRDI0:
                    pOutputResource->resourceType = IFEOutputRDI0;
                    result                        = SetRDIOutputPortFormat(pOutputResource,
                                                                           pCurrentOutputPortImageFormat->format,
                                                                           pPortId[index],
                                                                           portSourceTypeId);
                    break;

                case IFEOutputPortRDI1:
                    pOutputResource->resourceType = IFEOutputRDI1;
                    result                        = SetRDIOutputPortFormat(pOutputResource,
                                                                           pCurrentOutputPortImageFormat->format,
                                                                           pPortId[index],
                                                                           portSourceTypeId);
                    break;

                case IFEOutputPortRDI2:
                    pOutputResource->resourceType = IFEOutputRDI2;
                    result                        = SetRDIOutputPortFormat(pOutputResource,
                                                                           pCurrentOutputPortImageFormat->format,
                                                                           pPortId[index],
                                                                           portSourceTypeId);
                    break;

                case IFEOutputPortRDI3:
                    pOutputResource->resourceType = IFEOutputRDI3;
                    result                        = SetRDIOutputPortFormat(pOutputResource,
                                                                           pCurrentOutputPortImageFormat->format,
                                                                           pPortId[index],
                                                                           portSourceTypeId);
                    break;

                case IFEOutputPortStatsIHIST:
                    pOutputResource->resourceType   = IFEOutputStatsIHIST;
                    pOutputResource->format         = ISPFormatPlain1616;
                    break;

                case IFEOutputPortStatsRS:
                    pOutputResource->resourceType   = IFEOutputStatsRS;
                    pOutputResource->format         = ISPFormatPlain1616;
                    break;

                case IFEOutputPortStatsCS:
                    pOutputResource->resourceType   = IFEOutputStatsCS;
                    pOutputResource->format         = ISPFormatPlain64;
                    break;

                case IFEOutputPortPDAF:
                    pOutputResource->resourceType = IFEOutputPDAF;
                    pOutputResource->format = ISPFormatPlain1610;
                    break;

                default:
                    m_IFEOutputPathInfo[index].path = FALSE;
                    result                        = CamxResultEInvalidArg;
                    CAMX_LOG_ERROR(CamxLogGroupISP, "Invalid Port ID. %d", pPortId[index]);
                    break;
            }

            Titan17xContext* pContext = static_cast<Titan17xContext *>(GetHwContext());
            BOOL enableGrouping = pContext->GetTitan17xSettingsManager()->GetTitan17xStaticSettings()->enableIFEOutputGrouping;

            if ((TRUE == enableGrouping) || (IFEModuleMode::DualIFENormal == m_mode))
            {
                // Group bus clients for composite handling
                switch (pPortId[index])
                {
                    case IFEOutputPortFD:
                        pOutputResource->compositeGroupId   = ISPOutputGroupId4;
                        break;

                    case IFEOutputPortFull:
                    case IFEOutputPortDS4:
                    case IFEOutputPortDS16:
                        pOutputResource->compositeGroupId   = ISPOutputGroupId0;
                        break;

                    case IFEOutputPortStatsHDRBHIST:
                    case IFEOutputPortStatsAWBBG:
                    case IFEOutputPortStatsHDRBE:
                    case IFEOutputPortStatsIHIST:
                    case IFEOutputPortStatsCS:
                    case IFEOutputPortStatsRS:
                    case IFEOutputPortStatsTLBG:
                    case IFEOutputPortStatsBHIST:
                        pOutputResource->compositeGroupId   = ISPOutputGroupId1;
                        break;

                    case IFEOutputPortStatsBF:
                        pOutputResource->compositeGroupId   = ISPOutputGroupId2;
                        break;

                    case IFEOutputPortCAMIFRaw:
                    case IFEOutputPortLSCRaw:
                    case IFEOutputPortGTMRaw:
                        pOutputResource->compositeGroupId   = ISPOutputGroupId5;
                        break;

                    case IFEOutputPortPDAF:
                        pOutputResource->compositeGroupId   = ISPOutputGroupId6;
                        break;

                    case IFEOutputPortDisplayFull:
                    case IFEOutputPortDisplayDS4:
                    case IFEOutputPortDisplayDS16:
                        /// @todo (CAMX-4027) Changes for new IFE display o/p. For now same as FullVidPort
                        pOutputResource->compositeGroupId   = ISPOutputGroupId3;
                        break;

                    default:
                        pOutputResource->compositeGroupId   = ISPOutputGroupIdNONE;
                        break;
                }
            }

            if (CamxResultSuccess == result)
            {
                pOutputResource++;
            }
            else
            {
                break;
            }
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::MapPortIdToChannelId
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFENode::MapPortIdToChannelId(
    UINT    portId,
    UINT32* pChannelId)
{
    /// @todo (CAMX-817) Need to discuss with CSI/Kernel team to avoid this run-time port id conversion
    CamxResult result = CamxResultSuccess;

    *pChannelId = 0;

    switch (portId)
    {
        case IFEOutputPortFull:
            *pChannelId = IFEOutputFull;
            break;

        case IFEOutputPortDS4:
            *pChannelId = IFEOutputDS4;
            break;

        case IFEOutputPortDS16:
            *pChannelId = IFEOutputDS16;
            break;

        case IFEOutputPortDisplayFull:
            *pChannelId = IFEOutputDisplayFull;
            break;

        case IFEOutputPortDisplayDS4:
            *pChannelId = IFEOutputDisplayDS4;
            break;

        case IFEOutputPortDisplayDS16:
            *pChannelId = IFEOutputDisplayDS16;
            break;

        case IFEOutputPortFD:
            *pChannelId = IFEOutputFD;
            break;

        case IFEOutputPortCAMIFRaw:
        case IFEOutputPortLSCRaw:
        case IFEOutputPortGTMRaw:
            *pChannelId = IFEOutputRaw;
            break;

        case IFEOutputPortStatsDualPD:
            *pChannelId = IFEOutputDualPD;
            break;

        case IFEOutputPortRDI0:
            *pChannelId = IFEOutputRDI0;
            break;

        case IFEOutputPortRDI1:
            *pChannelId = IFEOutputRDI1;
            break;

        case IFEOutputPortRDI2:
            *pChannelId = IFEOutputRDI2;
            break;

        case IFEOutputPortRDI3:
            *pChannelId = IFEOutputRDI3;
            break;

        case IFEOutputPortPDAF:
            *pChannelId = IFEOutputPDAF;
            break;

        case IFEOutputPortStatsRS:
            *pChannelId = IFEOutputStatsRS;
            break;

        case IFEOutputPortStatsCS:
            *pChannelId = IFEOutputStatsCS;
            break;

        case IFEOutputPortStatsIHIST:
            *pChannelId = IFEOutputStatsIHIST;
            break;

        case IFEOutputPortStatsBHIST:
            *pChannelId = IFEOutputStatsBHIST;
            break;

        case IFEOutputPortStatsHDRBE:
            *pChannelId = IFEOutputStatsHDRBE;
            break;

        case IFEOutputPortStatsHDRBHIST:
            *pChannelId = IFEOutputStatsHDRBHIST;
            break;

        case IFEOutputPortStatsTLBG:
            *pChannelId = IFEOutputStatsTLBG;
            break;

        case IFEOutputPortStatsAWBBG:
            *pChannelId = IFEOutputStatsAWBBG;
            break;

        case IFEOutputPortStatsBF:
            *pChannelId = IFEOutputStatsBF;
            break;

        default:
            result = CamxResultEInvalidArg;
            break;
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::GetPDAFInformation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IFENode::GetPDAFInformation()
{
    UINT   sensorPDAFInfoTag[1] = { PropertyIDSensorPDAFInfo };
    VOID*  pDataOutput[]        = { 0 };
    UINT64 PDAFdataOffset[1]    = { 0 };

    GetDataList(sensorPDAFInfoTag, pDataOutput, PDAFdataOffset, 1);
    if (NULL != pDataOutput[0])
    {
        Utils::Memcpy(&m_ISPInputSensorData.sensorPDAFInfo, pDataOutput[0], sizeof(SensorPDAFInfo));
    }
    else
    {
        CAMX_LOG_VERBOSE(CamxLogGroupISP, "PDAF not enabled");
    }

}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IFENode::IsPDHwEnabled()
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL IFENode::IsPDHwEnabled()
{
    BOOL isPDHwEnabled = FALSE;
    const StaticSettings*   pSettings = GetStaticSettings();

    if (NULL != pSettings)
    {
        isPDHwEnabled = ((m_hwMask & pSettings->pdafHWEnableMask)      &&
                         (FALSE    == pSettings->disablePDAF)          &&
                         (TRUE     == m_currentSensorModeSupportPDAF));
    }

    return isPDHwEnabled;
}



////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::AcquireDevice
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFENode::AcquireDevice()
{
    /// @todo (CAMX-3118)   Add logic for dual IFE and fall back on single IFE if dual fails.
    CamxResult           result      = CamxResultSuccess;
    CSLDeviceResource    deviceResource;

    CAMX_ASSERT (NULL != m_pSensorModeData);

    result = SetupDeviceResource();

    if (CamxResultSuccess == result)
    {
        deviceResource.pDeviceResourceParam    = NULL;
        deviceResource.deviceResourceParamSize = 0;
        deviceResource.resourceID              = ISPResourceIdPort;

        if (m_numResource <= 0)
        {
            result = CamxResultEInvalidArg;
            CAMX_LOG_ERROR(CamxLogGroupISP, "No Resource Needs to be Set up");
        }
        else
        {
            result = CSLAcquireDevice(GetCSLSession(),
                                      &m_hDevice,
                                      DeviceIndices()[0],
                                      &deviceResource,
                                      0,
                                      NULL,
                                      0,
                                      NodeIdentifierString());
            CAMX_LOG_INFO(CamxLogGroupCore, "Acquiring - IFEDevice : %s, Result: %d", NodeIdentifierString(), result);
            if (CamxResultSuccess == result)
            {
                SetDeviceAcquired(TRUE);
                AddCSLDeviceHandle(m_hDevice);
            }
            else
            {
                CAMX_LOG_ERROR(CamxLogGroupISP, "Acquire IFE Device Failed");
            }
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::UpdateIFECapabilityBasedOnCameraPlatform
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFENode::UpdateIFECapabilityBasedOnCameraPlatform()
{
    CamxResult result = CamxResultSuccess;
    m_titanVersion = static_cast<Titan17xContext *>(GetHwContext())->GetTitanVersion();

    switch(m_titanVersion)
    {
        case CSLCameraTitanVersion::CSLTitan170:
            m_hwMask            = ISPHwTitan170;
            m_maxOutputWidthFD  = IFEMaxOutputWidthFD;
            m_maxOutputHeightFD = IFEMaxOutputHeightFD;
            break;

        case CSLCameraTitanVersion::CSLTitan175:
        case CSLCameraTitanVersion::CSLTitan160:
            // Both CSLTitan175 and CSLTitan160 have the same IFE IQ h/w
            m_hwMask            = ISPHwTitan175;
            m_maxOutputWidthFD  = IFEMaxOutputWidthFD;
            m_maxOutputHeightFD = IFEMaxOutputHeightFD;
            break;

        case CSLCameraTitanVersion::CSLTitan150:
            m_hwMask            = ISPHwTitan150;
            m_maxOutputWidthFD  = IFEMaxOutputWidthFDTalos;
            m_maxOutputHeightFD = IFEMaxOutputHeightFDTalos;
            break;

        default:
            result = CamxResultEUnsupported;
            CAMX_LOG_ERROR(CamxLogGroupISP, "Unsupported Titan Version = 0X%x",
                static_cast<Titan17xContext *>(GetHwContext())->GetTitanVersion());
            break;
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::ConfigureIFECapability
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFENode::ConfigureIFECapability()
{
    CamxResult result = CamxResultSuccess;

    switch (m_version.majorVersion)
    {
        /// @todo (CAMX-652) Finalize the version number definiation with CSL layer, 0 might not be the right number
        case 0:
            m_capability.numIFEIQModule      = sizeof(IFEIQModuleItems) / sizeof(IFEIQModuleInfo);
            m_capability.pIFEIQModuleList    = IFEIQModuleItems;
            m_capability.numIFEStatsModule   = sizeof(IFEStatsModuleItems) / sizeof(IFEStatsModuleInfo);
            m_capability.pIFEStatsModuleList = IFEStatsModuleItems;
            break;

        default:
            result = CamxResultEUnsupported;
            CAMX_ASSERT_ALWAYS_MESSAGE("Unsupported Version Number = %u", m_version.majorVersion);
            break;
    }

    // Configure IFE Capability
    result = UpdateIFECapabilityBasedOnCameraPlatform();

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::HasOutputPortForIQModulePathConfig
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL IFENode::HasOutputPortForIQModulePathConfig(
    IFEPipelinePath pipelinePath
    ) const
{
    // Check if the matching output port for a given pipeline path exist in topology
    BOOL hasOutputPort = FALSE;

    switch (pipelinePath)
    {
        case IFEPipelinePath::FullPath:
            hasOutputPort = m_IFEOutputPathInfo[IFEOutputPortFull].path;
            break;
        case IFEPipelinePath::FDPath:
            hasOutputPort = m_IFEOutputPathInfo[IFEOutputPortFD].path;
            break;
        case IFEPipelinePath::DS4Path:
            hasOutputPort = m_IFEOutputPathInfo[IFEOutputPortDS4].path;
            break;
        case IFEPipelinePath::DS16Path:
            hasOutputPort = m_IFEOutputPathInfo[IFEOutputPortDS16].path;
            break;
        case IFEPipelinePath::DisplayFullPath:
            hasOutputPort = m_IFEOutputPathInfo[IFEOutputPortDisplayFull].path;
            break;
        case IFEPipelinePath::DisplayDS4Path:
            hasOutputPort = m_IFEOutputPathInfo[IFEOutputPortDisplayDS4].path;
            break;
        case IFEPipelinePath::DisplayDS16Path:
            hasOutputPort = m_IFEOutputPathInfo[IFEOutputPortDisplayDS16].path;
            break;
        // If any of pixel raw output port is enabled, IFECrop module for this path needs to be enabled
        case IFEPipelinePath::PixelRawDumpPath:
            hasOutputPort = m_IFEOutputPathInfo[IFEOutputPortCAMIFRaw].path ||
                            m_IFEOutputPathInfo[IFEOutputPortLSCRaw].path   ||
                            m_IFEOutputPathInfo[IFEOutputPortGTMRaw].path;
            break;

        // Common path is regarded as always enabled
        case IFEPipelinePath::CommonPath:
            hasOutputPort = TRUE;
            break;

        default:
            break;
    }

    return hasOutputPort;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::CreateIFEIQModules
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFENode::CreateIFEIQModules()
{
    CamxResult          result          = CamxResultSuccess;
    IFEIQModuleInfo*    pIQModule       = m_capability.pIFEIQModuleList;
    UINT                count           = 0;
    IFEModuleCreateData moduleInputData = {};
    UINT                totalOutputPort = 0;
    UINT                outputPortId[MaxIFEOutputPorts];

    // Get Output Port List
    GetAllOutputPortIds(&totalOutputPort, &outputPortId[0]);

    for (UINT32 index = 0; index < totalOutputPort; index++)
    {
        // Keep the record of which IFE output ports are actually connected/used in this pipeline/topology
        UINT IFEPathIndex = outputPortId[index];

        m_IFEOutputPathInfo[IFEPathIndex].path = TRUE;
    }

    m_numIFEIQModule = 0;
    CamX::Utils::Memset(m_stripeConfigs, 0, sizeof(m_stripeConfigs));
    moduleInputData.initializationData.pStripeConfig = &m_stripeConfigs[0];

    for (count = 0; count < m_capability.numIFEIQModule; count++)
    {
        // Only create IQ modules if the module is installed and the output ports of the current path exists
        if (TRUE == IsIQModuleInstalled(&pIQModule[count]))
        {
            if (TRUE == HasOutputPortForIQModulePathConfig(pIQModule[count].IFEPath))
            {
                moduleInputData.pipelineData.IFEPath = pIQModule[count].IFEPath;

                if ((pIQModule[count].moduleType == ISPIQModuleType::IFEDUALPD) ||
                    (pIQModule[count].moduleType == ISPIQModuleType::IFECAMIFLite))
                {
                    const StaticSettings* pSettings = GetStaticSettings();
                    if (NULL != pSettings)
                    {
                        if ((TRUE == Utils::IsBitMaskSet(m_hwMask, pSettings->pdafHWEnableMask)) &&
                            (TRUE == pSettings->disablePDAF))
                        {
                            CAMX_LOG_INFO(CamxLogGroupStats, " Skipping dualpd hw since it is disabled ");
                            continue;
                        }
                    }
                    else
                    {
                        CAMX_LOG_ERROR(CamxLogGroupStats, "Invalid settings");
                    }
                }

                if (pIQModule[count].moduleType == ISPIQModuleType::IFEHVX)
                {
                    if (NULL != m_pIFEHVXModule)
                    {
                        m_pIFEIQModule[m_numIFEIQModule] = m_pIFEHVXModule;
                        m_numIFEIQModule++;
                    }
                    continue;
                }

                result = pIQModule[count].IQCreate(&moduleInputData);

                if (CamxResultSuccess == result)
                {
                    m_pIFEIQModule[m_numIFEIQModule] = moduleInputData.pModule;

                    m_numIFEIQModule++;
                }
                else
                {
                    CAMX_ASSERT_ALWAYS_MESSAGE("%s: Failed to Create IQ Module, count = %d", __FUNCTION__, count);
                    break;
                }
            }
            else
            {
                // Filter out for the IQ modules that do not have the IFE output ports defined in topology.
                CAMX_LOG_VERBOSE(CamxLogGroupISP,
                                 "IFEIQModuleItems[%u] installed, but skipping since the outport doesn't exist",
                                 count);
            }
        }
    }

    CamX::Utils::Memcpy(&m_stripeConfigs[1], &m_stripeConfigs[0], sizeof(m_stripeConfigs[0]));

    m_stripeConfigs[0].cropType = CropType::CropTypeFromLeft;
    m_stripeConfigs[1].cropType = CropType::CropTypeFromRight;

    m_stripeConfigs[0].stripeId = 0;
    m_stripeConfigs[1].stripeId = 1;

    m_stripeConfigs[0].pFrameLevelData = &m_ISPFrameData;
    m_stripeConfigs[1].pFrameLevelData = &m_ISPFrameData;

    // The clean-up for the error case happens outside this function

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::CreateIFEStatsModules
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFENode::CreateIFEStatsModules()
{
    CamxResult          result       = CamxResultSuccess;
    IFEStatsModuleInfo* pStatsModule = m_capability.pIFEStatsModuleList;
    UINT                count        = 0;
    UINT                i            = 0;
    UINT                outputPortId[MaxIFEOutputPorts];
    UINT                totalOutputPort = 0;
    IFEStatsModuleCreateData moduleInputData;

    m_numIFEStatsModule          = 0;
    moduleInputData.pipelineData = m_pipelineData;

    GetAllOutputPortIds(&totalOutputPort, &outputPortId[0]);
    for (i = 0; i < totalOutputPort; i++)
    {
        if (outputPortId[i] >= IFEOutputPortStatsRS &&
            outputPortId[i] <= IFEOutputPortStatsAWBBG)
        {
            for (count = 0; count < m_capability.numIFEStatsModule; count++)
            {
                if ((outputPortId[i] == IFEStatsModuleOutputPorts[count]) &&
                    ((pStatsModule[count].installed & m_hwMask) > 0))
                {
                    result = pStatsModule[count].StatsCreate(&moduleInputData);

                    if (CamxResultSuccess == result)
                    {
                        m_pIFEStatsModule[m_numIFEStatsModule] = moduleInputData.pModule;
                        m_numIFEStatsModule++;
                    }
                    else
                    {
                        CAMX_ASSERT_ALWAYS_MESSAGE("%s: Failed to create Stats Module.  count %d", __FUNCTION__, count);

                        break;
                    }
                    break;
                }
            }
        }
    }

    // The clean-up for the error case happens outside this function

    return result;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::CreateIFEHVXModules()
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFENode::CreateIFEHVXModules()
{
    IFEIQModuleInfo moduleInfo;
    IFEModuleCreateData moduleInputData = {0};

    Utils::Memset(&moduleInfo, 0, sizeof(moduleInfo));
    moduleInfo = GetIFEIQModulesOfType(ISPIQModuleType::IFEHVX);

    if (TRUE == IsIQModuleInstalled(&moduleInfo))
    {
        moduleInputData.initializationData.pStripeConfig = &m_stripeConfigs[0];
        moduleInputData.pipelineData.IFEPath             = moduleInfo.IFEPath;
        moduleInputData.pHvxInitializeData               = &m_HVXInputData;

        moduleInfo.IQCreate(&moduleInputData);

        m_pIFEHVXModule = moduleInputData.pModule;
    }

    return CamxResultSuccess;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::GetIFEIQModulesOfType()
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IFEIQModuleInfo IFENode::GetIFEIQModulesOfType(
    ISPIQModuleType moduleType)
{
    UINT            numOfIQModule = sizeof(IFEIQModuleItems) / sizeof(IFEIQModuleInfo);
    IFEIQModuleInfo moduleInfo;

    memset(&moduleInfo, 0, sizeof(moduleInfo));
    for (UINT i = 0; i < numOfIQModule; i++)
    {
        if (IFEIQModuleItems[i].moduleType == moduleType)
        {
            moduleInfo = IFEIQModuleItems[i];
            break;
        }
    }
    return moduleInfo;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::GetIFESWTMCModuleInstanceVersion()
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
SWTMCVersion IFENode::GetIFESWTMCModuleInstanceVersion()
{
    // If SWTMC IQ module is not installed, version is 1.0
    SWTMCVersion version = SWTMCVersion::TMC10;

    for (UINT i = 0; i < m_numIFEIQModule; i++)
    {
        if (ISPIQModuleType::SWTMC == m_pIFEIQModule[i]->GetIQType())
        {
            // If several new versions of SWTMC are added later, instead of casting to SWTMC11, may need a polymorphism.
            version = static_cast<SWTMC11*>(m_pIFEIQModule[i])->GetTMCVersion();
            break;
        }
    }

    return version;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::Cleanup()
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFENode::Cleanup()
{
    UINT        count  = 0;
    CamxResult  result = CamxResultSuccess;

    // De-allocate all of the IQ modules

    for (count = 0; count < m_numIFEIQModule; count++)
    {
        if (NULL != m_pIFEIQModule[count])
        {
            m_pIFEIQModule[count]->Destroy();
            m_pIFEIQModule[count] = NULL;
        }
    }

    m_numIFEIQModule = 0;

    for (count = 0; count < m_numIFEStatsModule; count++)
    {
        if (NULL != m_pIFEStatsModule[count])
        {
            m_pIFEStatsModule[count]->Destroy();
            m_pIFEStatsModule[count] = NULL;
        }
    }

    m_numIFEStatsModule = 0;

    if (NULL != m_pStripingInput)
    {
        CAMX_FREE(m_pStripingInput);
        m_pStripingInput = NULL;
    }

    if (NULL != m_pPassOut)
    {
        CAMX_FREE(m_pPassOut);
        m_pPassOut = NULL;
    }

    if (NULL != m_pTuningMetadata)
    {
        CAMX_FREE(m_pTuningMetadata);
        m_pTuningMetadata = NULL;
    }

    if (NULL != m_pDebugDataWriter)
    {
        CAMX_DELETE m_pDebugDataWriter;
        m_pDebugDataWriter = NULL;
    }

    if (NULL != m_pBwResourceConfig)
    {
        CAMX_FREE(m_pBwResourceConfig);
        m_pBwResourceConfig = NULL;
    }

    if (NULL != m_pBwResourceConfigAB)
    {
        CAMX_FREE(m_pBwResourceConfigAB);
        m_pBwResourceConfigAB = NULL;
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::PopulateGeneralTuningMetadata()
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IFENode::PopulateGeneralTuningMetadata(
    ISPInputData* pInputData)
{
    // Populate Enable &  Crop, Round and Clamp information
    IFEModuleEnableConfig* pModuleEnable                    = &pInputData->pCalculatedData->moduleEnable;
    IFETuningModuleEnableConfig* pIFEEnableConfig           = &pInputData->pIFETuningMetadata->IFEEnableConfig;
    pIFEEnableConfig->lensProcessingModuleConfig            = pModuleEnable->lensProcessingModuleConfig.u32All;
    pIFEEnableConfig->colorProcessingModuleConfig           = pModuleEnable->colorProcessingModuleConfig.u32All;
    pIFEEnableConfig->frameProcessingModuleConfig           = pModuleEnable->frameProcessingModuleConfig.u32All;
    pIFEEnableConfig->FDLumaCropRoundClampConfig            = pModuleEnable->FDLumaCropRoundClampConfig.u32All;
    pIFEEnableConfig->FDChromaCropRoundClampConfig          = pModuleEnable->FDChromaCropRoundClampConfig.u32All;
    pIFEEnableConfig->fullLumaCropRoundClampConfig          = pModuleEnable->fullLumaCropRoundClampConfig.u32All;
    pIFEEnableConfig->fullChromaCropRoundClampConfig        = pModuleEnable->fullChromaCropRoundClampConfig.u32All;
    pIFEEnableConfig->DS4LumaCropRoundClampConfig           = pModuleEnable->DS4LumaCropRoundClampConfig.u32All;
    pIFEEnableConfig->DS4ChromaCropRoundClampConfig         = pModuleEnable->DS4ChromaCropRoundClampConfig.u32All;
    pIFEEnableConfig->DS16LumaCropRoundClampConfig          = pModuleEnable->DS16LumaCropRoundClampConfig.u32All;
    pIFEEnableConfig->DS16ChromaCropRoundClampConfig        = pModuleEnable->DS16ChromaCropRoundClampConfig.u32All;
    pIFEEnableConfig->statsEnable                           = pModuleEnable->statsEnable.u32All;
    pIFEEnableConfig->statsConfig                           = pModuleEnable->statsConfig.u32All;
    pIFEEnableConfig->dspConfig                             = pModuleEnable->dspConfig.u32All;
    pIFEEnableConfig->frameProcessingDisplayModuleConfig    = pModuleEnable->frameProcessingDisplayModuleConfig.u32All;
    pIFEEnableConfig->displayFullLumaCropRoundClampConfig   = pModuleEnable->displayFullLumaCropRoundClampConfig.u32All;
    pIFEEnableConfig->displayFullChromaCropRoundClampConfig = pModuleEnable->displayFullChromaCropRoundClampConfig.u32All;
    pIFEEnableConfig->displayDS4LumaCropRoundClampConfig    = pModuleEnable->displayDS4LumaCropRoundClampConfig.u32All;
    pIFEEnableConfig->displayDS4ChromaCropRoundClampConfig  = pModuleEnable->displayDS4ChromaCropRoundClampConfig.u32All;
    pIFEEnableConfig->displayDS16LumaCropRoundClampConfig   = pModuleEnable->displayDS16LumaCropRoundClampConfig.u32All;
    pIFEEnableConfig->displayDS16ChromaCropRoundClampConfig = pModuleEnable->displayDS16ChromaCropRoundClampConfig.u32All;

    // Populate trigger data
    IFETuningTriggerData* pIFETuningTriggers    = &pInputData->pIFETuningMetadata->IFETuningTriggers;
    pIFETuningTriggers->AECexposureGainRatio    = pInputData->triggerData.AECexposureGainRatio;
    pIFETuningTriggers->AECexposureTime         = pInputData->triggerData.AECexposureTime;
    pIFETuningTriggers->AECSensitivity          = pInputData->triggerData.AECSensitivity;
    pIFETuningTriggers->AECGain                 = pInputData->triggerData.AECGain;
    pIFETuningTriggers->AECLuxIndex             = pInputData->triggerData.AECLuxIndex;
    pIFETuningTriggers->AWBleftGGainWB          = pInputData->triggerData.AWBleftGGainWB;
    pIFETuningTriggers->AWBleftBGainWB          = pInputData->triggerData.AWBleftBGainWB;
    pIFETuningTriggers->AWBleftRGainWB          = pInputData->triggerData.AWBleftRGainWB;
    pIFETuningTriggers->AWBColorTemperature     = pInputData->triggerData.AWBColorTemperature;
    pIFETuningTriggers->DRCGain                 = pInputData->triggerData.DRCGain;
    pIFETuningTriggers->DRCGainDark             = pInputData->triggerData.DRCGainDark;
    pIFETuningTriggers->lensPosition            = pInputData->triggerData.lensPosition;
    pIFETuningTriggers->lensZoom                = pInputData->triggerData.lensZoom;
    pIFETuningTriggers->postScaleRatio          = pInputData->triggerData.postScaleRatio;
    pIFETuningTriggers->preScaleRatio           = pInputData->triggerData.preScaleRatio;
    pIFETuningTriggers->sensorImageWidth        = pInputData->triggerData.sensorImageWidth;
    pIFETuningTriggers->sensorImageHeight       = pInputData->triggerData.sensorImageHeight;
    pIFETuningTriggers->CAMIFWidth              = pInputData->triggerData.CAMIFWidth;
    pIFETuningTriggers->CAMIFHeight             = pInputData->triggerData.CAMIFHeight;
    pIFETuningTriggers->numberOfLED             = pInputData->triggerData.numberOfLED;
    pIFETuningTriggers->LEDSensitivity          = static_cast<INT32>(pInputData->triggerData.LEDSensitivity);
    pIFETuningTriggers->bayerPattern            = pInputData->triggerData.bayerPattern;
    pIFETuningTriggers->sensorOffsetX           = pInputData->triggerData.sensorOffsetX;
    pIFETuningTriggers->sensorOffsetY           = pInputData->triggerData.sensorOffsetY;
    pIFETuningTriggers->blackLevelOffset        = pInputData->triggerData.blackLevelOffset;

    // Populate Sensor configuration data
    IFEBPSSensorConfigData*  pIFESensorConfig   = &pInputData->pIFETuningMetadata->IFESensorConfig;
    pIFESensorConfig->isBayer                   = pInputData->sensorData.isBayer;
    pIFESensorConfig->format                    = static_cast<UINT32>(pInputData->sensorData.format);
    pIFESensorConfig->digitalGain               = pInputData->sensorData.dGain;
    pIFESensorConfig->ZZHDRColorPattern         = pInputData->sensorData.ZZHDRColorPattern;
    pIFESensorConfig->ZZHDRFirstExposurePattern = pInputData->sensorData.ZZHDRFirstExposurePattern;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::DumpTuningMetadata()
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IFENode::DumpTuningMetadata(
    ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;
    static const UINT PropertiesTuning[] = { PropertyIDTuningDataIFE };
    VOID* pData[1] = { 0 };
    UINT length = CAMX_ARRAY_SIZE(PropertiesTuning);
    UINT64 propertyDataTuningOffset[1] = { 0 };

    GetDataList(PropertiesTuning, pData, propertyDataTuningOffset, length);

    DebugData* pDebugData = reinterpret_cast<DebugData*>(pData[0]);

    pInputData->pIFETuningMetadata->sensorData.format           = m_pSensorModeData->format;
    pInputData->pIFETuningMetadata->sensorData.cropInfo         = m_pSensorModeData->cropInfo;
    pInputData->pIFETuningMetadata->sensorData.maxLineCount     = m_pSensorModeData->maxLineCount;
    pInputData->pIFETuningMetadata->sensorData.numLinesPerFrame = m_pSensorModeData->numLinesPerFrame;
    pInputData->pIFETuningMetadata->sensorData.numPixelsPerLine = m_pSensorModeData->numPixelsPerLine;
    pInputData->pIFETuningMetadata->sensorData.resolution       = m_pSensorModeData->resolution;
    pInputData->pIFETuningMetadata->sensorData.binningTypeH     = m_pSensorModeData->binningTypeH;
    pInputData->pIFETuningMetadata->sensorData.binningTypeV     = m_pSensorModeData->binningTypeV;

    // Check if debug data buffer is available
    if (NULL == pDebugData || NULL == pDebugData->pData)
    {
        // Debug-data buffer not available is valid use case
        // Normal execution will continue without debug data
        CAMX_LOG_VERBOSE(CamxLogGroupISP, "Debug-data buffer not available");
        return;
    }

    // Populate any metadata obtained direclty from base IFE node
    PopulateGeneralTuningMetadata(pInputData);

    // Update the Debug data pool with the tuning data dump
    // Set the buffer pointer
    m_pDebugDataWriter->SetBufferPointer(static_cast<BYTE*>(pDebugData->pData),
                                         pDebugData->size);

    // Add IFE tuning metadata tags
    m_pDebugDataWriter->AddDataTag(DebugDataTagID::TuningIFEModuleEnableConfig,
                                   DebugDataTagType::TuningIFEEnableConfig,
                                   1,
                                   &pInputData->pIFETuningMetadata->IFEEnableConfig,
                                   sizeof(pInputData->pIFETuningMetadata->IFEEnableConfig));

    m_pDebugDataWriter->AddDataTag(DebugDataTagID::TuningIFETriggerModulesData,
                                   DebugDataTagType::TuningIFETriggerData,
                                   1,
                                   &pInputData->pIFETuningMetadata->IFETuningTriggers,
                                   sizeof(pInputData->pIFETuningMetadata->IFETuningTriggers));

    if (TRUE == pInputData->pCalculatedData->moduleEnable.lensProcessingModuleConfig.bits.ROLLOFF_EN)
    {
        m_pDebugDataWriter->AddDataTag(DebugDataTagID::TuningIFELSCPackedMesh,
                                       DebugDataTagType::TuningLSCMeshLUT,
                                       1,
                                       &pInputData->pIFETuningMetadata->IFEDMIData.packedLUT.LSCMesh,
                                       sizeof(pInputData->pIFETuningMetadata->IFEDMIData.packedLUT.LSCMesh));
    }

    if (TRUE == pInputData->pCalculatedData->moduleEnable.colorProcessingModuleConfig.bits.RGB_LUT_EN)
    {
        m_pDebugDataWriter->AddDataTag(DebugDataTagID::TuningIFEGammaPackedLUT,
                                       DebugDataTagType::TuningGammaCurve,
                                       CAMX_ARRAY_SIZE(pInputData->pIFETuningMetadata->IFEDMIData.packedLUT.gamma),
                                       &pInputData->pIFETuningMetadata->IFEDMIData.packedLUT.gamma,
                                       sizeof(pInputData->pIFETuningMetadata->IFEDMIData.packedLUT.gamma));
    }

    if (TRUE == pInputData->pCalculatedData->moduleEnable.colorProcessingModuleConfig.bits.GTM_EN)
    {
        m_pDebugDataWriter->AddDataTag(DebugDataTagID::TuningIFEGTMPackedLUT,
                                       DebugDataTagType::TuningGTMLUT,
                                       1,
                                       &pInputData->pIFETuningMetadata->IFEDMIData.packedLUT.GTM,
                                       sizeof(pInputData->pIFETuningMetadata->IFEDMIData.packedLUT.GTM));
    }

    if ((TRUE == pInputData->pCalculatedData->moduleEnable.lensProcessingModuleConfig.bitfields.HDR_MAC_EN) ||
        (TRUE == pInputData->pCalculatedData->moduleEnable.lensProcessingModuleConfig.bitfields.HDR_RECON_EN))
    {
        m_pDebugDataWriter->AddDataTag(DebugDataTagID::TuningIFEHDR,
                                       DebugDataTagType::TuningIFEHDR,
                                       1,
                                       &pInputData->pIFETuningMetadata->IFEHDRData,
                                       sizeof(pInputData->pIFETuningMetadata->IFEHDRData));
    }

    // Add Sensor tuning metadata tags
    UINT8 pixelFormat = static_cast<UINT8>(pInputData->pIFETuningMetadata->sensorData.format);
    m_pDebugDataWriter->AddDataTag(DebugDataTagID::TuningSensorPixelFormat,
                                   DebugDataTagType::UInt8,
                                   1,
                                   &pixelFormat,
                                   sizeof(pixelFormat));

    m_pDebugDataWriter->AddDataTag(DebugDataTagID::TuningSensorNumPixelsPerLine,
                                   DebugDataTagType::UInt32,
                                   1,
                                   &pInputData->pIFETuningMetadata->sensorData.numPixelsPerLine,
                                   sizeof(pInputData->pIFETuningMetadata->sensorData.numPixelsPerLine));

    m_pDebugDataWriter->AddDataTag(DebugDataTagID::TuningSensorNumLinesPerFrame,
                                   DebugDataTagType::UInt32,
                                   1,
                                   &pInputData->pIFETuningMetadata->sensorData.numLinesPerFrame,
                                   sizeof(pInputData->pIFETuningMetadata->sensorData.numLinesPerFrame));

    m_pDebugDataWriter->AddDataTag(DebugDataTagID::TuningSensorMaxLineCount,
                                   DebugDataTagType::UInt32,
                                   1,
                                   &pInputData->pIFETuningMetadata->sensorData.maxLineCount,
                                   sizeof(pInputData->pIFETuningMetadata->sensorData.maxLineCount));

    m_pDebugDataWriter->AddDataTag(DebugDataTagID::TuningSensorResoultion,
                                   DebugDataTagType::TuningSensorResolution,
                                   1,
                                   &pInputData->pIFETuningMetadata->sensorData.resolution,
                                   sizeof(pInputData->pIFETuningMetadata->sensorData.resolution));

    m_pDebugDataWriter->AddDataTag(DebugDataTagID::TuningSensorCropInfo,
                                   DebugDataTagType::TuningSensorCropInfo,
                                   1,
                                   &pInputData->pIFETuningMetadata->sensorData.cropInfo,
                                   sizeof(pInputData->pIFETuningMetadata->sensorData.cropInfo));

    m_pDebugDataWriter->AddDataTag(DebugDataTagID::TuningBinningTypeH,
                                   DebugDataTagType::UInt32,
                                   1,
                                   &pInputData->pIFETuningMetadata->sensorData.binningTypeH,
                                   sizeof(pInputData->pIFETuningMetadata->sensorData.binningTypeH));

    m_pDebugDataWriter->AddDataTag(DebugDataTagID::TuningBinningTypeV,
                                   DebugDataTagType::UInt32,
                                   1,
                                   &pInputData->pIFETuningMetadata->sensorData.binningTypeV,
                                   sizeof(pInputData->pIFETuningMetadata->sensorData.binningTypeV));

    m_pDebugDataWriter->AddDataTag(DebugDataTagID::TuningIFESensorConfigData,
                                   DebugDataTagType::TuningIFESensorConfig,
                                   1,
                                   &pInputData->pIFETuningMetadata->IFESensorConfig,
                                   sizeof(pInputData->pIFETuningMetadata->IFESensorConfig));

    // Add 3A tuning metadata tags
    // Copy the CHI AEC data locally to the expected debug data format which is byte-aligned and does not necessarily
    // contain all the fields contained in the CHI structure
    TuningAECFrameControl debugDataAEC;
    CAMX_STATIC_ASSERT(CAMX_ARRAY_SIZE(debugDataAEC.exposureInfo) <= CAMX_ARRAY_SIZE(pInputData->pAECUpdateData->exposureInfo));
    for (UINT32 i = 0; i < CAMX_ARRAY_SIZE(debugDataAEC.exposureInfo); i++)
    {
        debugDataAEC.exposureInfo[i].exposureTime       = pInputData->pAECUpdateData->exposureInfo[i].exposureTime;
        debugDataAEC.exposureInfo[i].linearGain         = pInputData->pAECUpdateData->exposureInfo[i].linearGain;
        debugDataAEC.exposureInfo[i].sensitivity        = pInputData->pAECUpdateData->exposureInfo[i].sensitivity;
        debugDataAEC.exposureInfo[i].deltaEVFromTarget  = pInputData->pAECUpdateData->exposureInfo[i].deltaEVFromTarget;
    }
    debugDataAEC.luxIndex       = pInputData->pAECUpdateData->luxIndex;
    debugDataAEC.flashInfo      = static_cast<UINT8>(pInputData->pAECUpdateData->flashInfo);
    debugDataAEC.preFlashState  = static_cast<UINT8>(pInputData->pAECUpdateData->preFlashState);

    CAMX_STATIC_ASSERT(CAMX_ARRAY_SIZE(debugDataAEC.LEDCurrents) <= CAMX_ARRAY_SIZE(pInputData->pAECUpdateData->LEDCurrents));
    for (UINT32 i = 0; i < CAMX_ARRAY_SIZE(debugDataAEC.LEDCurrents); i++)
    {
        debugDataAEC.LEDCurrents[i] = pInputData->pAECUpdateData->LEDCurrents[i];
    }
    m_pDebugDataWriter->AddDataTag(DebugDataTagID::TuningAECData,
                                   DebugDataTagType::TuningAECFrameControl,
                                   1,
                                   &debugDataAEC,
                                   sizeof(debugDataAEC));


    // Copy the CHI AWB data locally to the expected debug data format which is byte-aligned
    TuningAWBFrameControl debugDataAWB;
    debugDataAWB.AWBGains[0]            = pInputData->pAWBUpdateData->AWBGains.rGain;
    debugDataAWB.AWBGains[1]            = pInputData->pAWBUpdateData->AWBGains.gGain;
    debugDataAWB.AWBGains[2]            = pInputData->pAWBUpdateData->AWBGains.bGain;
    debugDataAWB.colorTemperature       = pInputData->pAWBUpdateData->colorTemperature;
    debugDataAWB.isCCMOverrideEnabled   = (pInputData->pAWBUpdateData->AWBCCM[0].isCCMOverrideEnabled ? 1 : 0);

    CAMX_STATIC_ASSERT(
        CAMX_ARRAY_SIZE(debugDataAWB.CCMOffset) <= CAMX_ARRAY_SIZE(pInputData->pAWBUpdateData->AWBCCM[0].CCMOffset));
    for (UINT32 i = 0; i < CAMX_ARRAY_SIZE(debugDataAWB.CCMOffset); i++)
    {
        debugDataAWB.CCMOffset[i] = pInputData->pAWBUpdateData->AWBCCM[0].CCMOffset[i];
    }

    CAMX_STATIC_ASSERT(CAMX_ARRAY_ROWS(debugDataAWB.CCM) >= AWBNumCCMRows);
    CAMX_STATIC_ASSERT(CAMX_ARRAY_COLS(debugDataAWB.CCM) >= AWBNumCCMCols);
    for (UINT32 row = 0; row < AWBNumCCMRows; row++)
    {
        for (UINT32 col = 0; col < AWBNumCCMCols; col++)
        {
            debugDataAWB.CCM[row][col] = pInputData->pAWBUpdateData->AWBCCM[0].CCM[row][col];
        }
    }

    m_pDebugDataWriter->AddDataTag(DebugDataTagID::TuningAWBData,
                                   DebugDataTagType::TuningAWBFrameControl,
                                   1,
                                   &debugDataAWB,
                                   sizeof(debugDataAWB));

    // Make a copy in main metadata pool
    UINT32 metaTag = 0;
    static const UINT PropertiesDebugData[] = { PropertyIDDebugDataAll };
    VOID* pSrcData[1] = { 0 };
    length = CAMX_ARRAY_SIZE(PropertiesDebugData);
    propertyDataTuningOffset[0] = 0;
    GetDataList(PropertiesDebugData, pSrcData, propertyDataTuningOffset, length);

    result = VendorTagManager::QueryVendorTagLocation("org.quic.camera.debugdata", "DebugDataAll", &metaTag);
    const UINT TuningVendorTag[] = { metaTag };
    const VOID* pDstData[1] = { pSrcData[0]};
    UINT pDataCount[1] = { sizeof(DebugData) };

    WriteDataList(TuningVendorTag, pDstData, pDataCount, 1);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::ProgramIQEnable()
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFENode::ProgramIQEnable(
    ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;


    if (IsSensorModeFormatYUV(m_ISPInputSensorData.format))
    {
        // Clearing out all of IQ blocks for Lens processing
        Utils::Memset(&m_ISPData[0].moduleEnable.lensProcessingModuleConfig.bitfields,
            0,
            sizeof(m_ISPData[0].moduleEnable.lensProcessingModuleConfig.bitfields));
        Utils::Memset(&m_ISPData[1].moduleEnable.lensProcessingModuleConfig.bitfields,
            0,
            sizeof(m_ISPData[1].moduleEnable.lensProcessingModuleConfig.bitfields));
        Utils::Memset(&m_ISPData[2].moduleEnable.lensProcessingModuleConfig.bitfields,
            0,
            sizeof(m_ISPData[2].moduleEnable.lensProcessingModuleConfig.bitfields));

        m_ISPData[0].moduleEnable.lensProcessingModuleConfig.bitfields.CHROMA_UPSAMPLE_EN = 1;
        m_ISPData[1].moduleEnable.lensProcessingModuleConfig.bitfields.CHROMA_UPSAMPLE_EN = 1;
        m_ISPData[2].moduleEnable.lensProcessingModuleConfig.bitfields.CHROMA_UPSAMPLE_EN = 1;
        // DEMUX should be always enable
        m_ISPData[0].moduleEnable.lensProcessingModuleConfig.bitfields.DEMUX_EN = 1;
        m_ISPData[1].moduleEnable.lensProcessingModuleConfig.bitfields.DEMUX_EN = 1;
        m_ISPData[2].moduleEnable.lensProcessingModuleConfig.bitfields.DEMUX_EN = 1;
        // STATS path should be disable except iHIST
        m_IFEOutputPathInfo[IFEOutputPortStatsHDRBE].path = FALSE;
        m_IFEOutputPathInfo[IFEOutputPortStatsHDRBHIST].path = FALSE;
        m_IFEOutputPathInfo[IFEOutputPortStatsBF].path = FALSE;
        m_IFEOutputPathInfo[IFEOutputPortStatsAWBBG].path = FALSE;
        m_IFEOutputPathInfo[IFEOutputPortStatsBHIST].path = FALSE;
        m_IFEOutputPathInfo[IFEOutputPortStatsRS].path = FALSE;
        m_IFEOutputPathInfo[IFEOutputPortStatsCS].path = FALSE;
        m_IFEOutputPathInfo[IFEOutputPortStatsTLBG].path = FALSE;
    }

    if (IFEModuleMode::DualIFENormal == m_mode)
    {
        // In dual IFE mode, each IFE will have its own enable bits with potentially different values, hence writing the enable
        // register to left/right (and not common) command buffers.
        CAMX_LOG_VERBOSE(CamxLogGroupApp, "Right command buffer");
        pInputData->pCmdBuffer      = m_pRightCmdBuffer;
        pInputData->pCalculatedData = &m_ISPData[1];
        result                      = ProgramIQModuleEnableConfig(pInputData);

        if (CamxResultSuccess == result)
        {
            CAMX_LOG_VERBOSE(CamxLogGroupApp, "Left command buffer");
            pInputData->pCmdBuffer      = m_pLeftCmdBuffer;
            pInputData->pCalculatedData = &m_ISPData[0];
            result                      = ProgramIQModuleEnableConfig(pInputData);

        }
    }
    else
    {
        CAMX_LOG_VERBOSE(CamxLogGroupApp, "Main command buffer");
        pInputData->pCmdBuffer      = m_pCommonCmdBuffer;
        pInputData->pCalculatedData = &m_ISPData[2];
        result                      = ProgramIQModuleEnableConfig(pInputData);
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::PrepareHDRBEStatsMetadata
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IFENode::PrepareHDRBEStatsMetadata(
    const ISPInputData*     pInputData,
    PropertyISPHDRBEStats*  pMetadata)
{
    pMetadata->statsConfig  = pInputData->pCalculatedData->metadata.HDRBEStatsConfig;
    pMetadata->dualIFEMode  = FALSE;

    if (IFEModuleMode::DualIFENormal == m_mode)
    {
        pMetadata->dualIFEMode      = TRUE;
        pMetadata->stripeConfig[0]  = m_ISPData[0].metadata.HDRBEStatsConfig;
        pMetadata->stripeConfig[1]  = m_ISPData[1].metadata.HDRBEStatsConfig;

        /// @todo (CAMX-1293) Need to send frame level configuration as well. Manually create for now using two stripes.
        pMetadata->statsConfig.HDRBEConfig  = pInputData->pAECStatsUpdateData->statsConfig.BEConfig;
        pMetadata->statsConfig.isAdjusted   = FALSE;
        pMetadata->statsConfig.regionWidth  = pMetadata->stripeConfig[0].regionWidth;
        pMetadata->statsConfig.regionHeight = pMetadata->stripeConfig[0].regionHeight;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::PrepareBFStatsMetadata
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IFENode::PrepareBFStatsMetadata(
    const ISPInputData* pInputData,
    PropertyISPBFStats* pMetadata)
{
    pMetadata->statsConfig = pInputData->pCalculatedData->metadata.BFStats;
    pMetadata->dualIFEMode = FALSE;

    if (IFEModuleMode::DualIFENormal == m_mode)
    {
        pMetadata->dualIFEMode      = TRUE;
        pMetadata->stripeConfig[0]  = m_ISPData[0].metadata.BFStats;
        pMetadata->stripeConfig[1]  = m_ISPData[1].metadata.BFStats;

        /// @todo (CAMX-1293) Need to send frame level configuration as well. Manually create for now using two stripes.
        pMetadata->statsConfig.BFConfig     = pInputData->pAFStatsUpdateData->statsConfig;
        pMetadata->statsConfig.isAdjusted   = FALSE;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::PrepareAWBBGStatsMetadata
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IFENode::PrepareAWBBGStatsMetadata(
    const ISPInputData*     pInputData,
    PropertyISPAWBBGStats*  pMetadata)
{
    pMetadata->statsConfig = pInputData->pCalculatedData->metadata.AWBBGStatsConfig;
    pMetadata->dualIFEMode = FALSE;

    if (IFEModuleMode::DualIFENormal == m_mode)
    {
        pMetadata->dualIFEMode      = TRUE;
        pMetadata->stripeConfig[0]  = m_ISPData[0].metadata.AWBBGStatsConfig;
        pMetadata->stripeConfig[1]  = m_ISPData[1].metadata.AWBBGStatsConfig;

        /// @todo (CAMX-1293) Need to send frame level configuration as well. Manually create for now using two stripes.
        pMetadata->statsConfig.AWBBGConfig  = pInputData->pAWBStatsUpdateData->statsConfig.BGConfig;
        pMetadata->statsConfig.isAdjusted   = FALSE;
        pMetadata->statsConfig.regionWidth  = pMetadata->stripeConfig[0].regionWidth;
        pMetadata->statsConfig.regionHeight = pMetadata->stripeConfig[0].regionHeight;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::PrepareRSStatsMetadata
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IFENode::PrepareRSStatsMetadata(
    const ISPInputData* pInputData,
    PropertyISPRSStats* pMetadata)
{
    pMetadata->statsConfig = pInputData->pCalculatedData->metadata.RSStats;
    pMetadata->dualIFEMode = FALSE;

    if (IFEModuleMode::DualIFENormal == m_mode)
    {
        pMetadata->dualIFEMode      = TRUE;
        pMetadata->stripeConfig[0]  = m_ISPData[0].metadata.RSStats;
        pMetadata->stripeConfig[1]  = m_ISPData[1].metadata.RSStats;

        /// @todo (CAMX-1293) Need to send frame level configuration as well. Manually create for now using two stripes.
        pMetadata->statsConfig.RSConfig     = pInputData->pAFDStatsUpdateData->statsConfig;
        pMetadata->statsConfig.isAdjusted   = FALSE;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::PrepareCSStatsMetadata
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IFENode::PrepareCSStatsMetadata(
    const ISPInputData* pInputData,
    PropertyISPCSStats* pMetadata)
{
    pMetadata->statsConfig = pInputData->pCalculatedData->metadata.CSStats;
    pMetadata->dualIFEMode = FALSE;

    if (IFEModuleMode::DualIFENormal == m_mode)
    {
        pMetadata->dualIFEMode      = TRUE;
        pMetadata->stripeConfig[0]  = m_ISPData[0].metadata.CSStats;
        pMetadata->stripeConfig[1]  = m_ISPData[1].metadata.CSStats;

        /// @todo (CAMX-1293) Need to send frame level configuration as well. Manually create for now using two stripes.
        pMetadata->statsConfig.CSConfig = pInputData->pCSStatsUpdateData->statsConfig;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::PrepareBHistStatsMetadata
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IFENode::PrepareBHistStatsMetadata(
    const ISPInputData*     pInputData,
    PropertyISPBHistStats*  pMetadata)
{
    pMetadata->statsConfig           = pInputData->pCalculatedData->metadata.BHistStatsConfig;
    pMetadata->dualIFEMode           = FALSE;
    pMetadata->statsConfig.requestID = pInputData->frameNum;

    if (IFEModuleMode::DualIFENormal == m_mode)
    {
        pMetadata->dualIFEMode      = TRUE;
        pMetadata->stripeConfig[0]  = m_ISPData[0].metadata.BHistStatsConfig;
        pMetadata->stripeConfig[1]  = m_ISPData[1].metadata.BHistStatsConfig;

        /// @todo (CAMX-1293) Need to send frame level configuration as well. Manually create for now using two stripes.
        pMetadata->statsConfig.BHistConfig  = pInputData->pAECStatsUpdateData->statsConfig.BHistConfig;
        pMetadata->statsConfig.numBins      = m_ISPData[0].metadata.BHistStatsConfig.numBins;
        pMetadata->statsConfig.isAdjusted   = FALSE;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::PrepareIHistStatsMetadata
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IFENode::PrepareIHistStatsMetadata(
    const ISPInputData*     pInputData,
    PropertyISPIHistStats*  pMetadata)
{
    pMetadata->statsConfig = pInputData->pCalculatedData->metadata.IHistStatsConfig;
    pMetadata->dualIFEMode = FALSE;

    if (IFEModuleMode::DualIFENormal == m_mode)
    {
        pMetadata->dualIFEMode      = TRUE;
        pMetadata->stripeConfig[0]  = m_ISPData[0].metadata.IHistStatsConfig;
        pMetadata->stripeConfig[1]  = m_ISPData[1].metadata.IHistStatsConfig;

        /// @todo (CAMX-1293) Need to send frame level configuration as well. Manually create for now using two stripes.
        pMetadata->statsConfig.IHistConfig  = pInputData->pIHistStatsUpdateData->statsConfig;
        pMetadata->statsConfig.numBins      = m_ISPData[0].metadata.IHistStatsConfig.numBins;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::PrepareHDRBHistStatsMetadata
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IFENode::PrepareHDRBHistStatsMetadata(
    const ISPInputData*         pInputData,
    PropertyISPHDRBHistStats*   pMetadata)
{
    pMetadata->statsConfig = pInputData->pCalculatedData->metadata.HDRBHistStatsConfig;
    pMetadata->dualIFEMode = FALSE;

    if (IFEModuleMode::DualIFENormal == m_mode)
    {
        pMetadata->dualIFEMode      = TRUE;
        pMetadata->stripeConfig[0]  = m_ISPData[0].metadata.HDRBHistStatsConfig;
        pMetadata->stripeConfig[1]  = m_ISPData[1].metadata.HDRBHistStatsConfig;

        /// @todo (CAMX-1293) Need to send frame level configuration as well. Manually create for now using two stripes.
        pMetadata->statsConfig.HDRBHistConfig   = pInputData->pAECStatsUpdateData->statsConfig.HDRBHistConfig;
        pMetadata->statsConfig.numBins          = m_ISPData[0].metadata.HDRBHistStatsConfig.numBins;
        pMetadata->statsConfig.isAdjusted       = FALSE;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::PrepareTintlessBGStatsMetadata
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IFENode::PrepareTintlessBGStatsMetadata(
    const ISPInputData*     pInputData,
    PropertyISPTintlessBG*  pMetadata)
{
    pMetadata->statsConfig = pInputData->pCalculatedData->metadata.tintlessBGStats;
    pMetadata->dualIFEMode = FALSE;

    if (IFEModuleMode::DualIFENormal == m_mode)
    {
        pMetadata->dualIFEMode      = TRUE;
        pMetadata->stripeConfig[0]  = m_ISPData[0].metadata.tintlessBGStats;
        pMetadata->stripeConfig[1]  = m_ISPData[1].metadata.tintlessBGStats;


        /// @todo (CAMX-1293) Need to send frame level configuration as well. Manually create for now using two stripes.
        pMetadata->statsConfig.tintlessBGConfig = pInputData->pAECStatsUpdateData->statsConfig.TintlessBGConfig;
        pMetadata->statsConfig.isAdjusted       = FALSE;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::PostMetadataRaw
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFENode::PostMetadataRaw()
{
    CamxResult  result                                  = CamxResultSuccess;
    UINT        dataIndex                               = 0;
    const VOID* pData[NumIFEMetadataRawOutputTags]      = { 0 };
    UINT        pDataCount[NumIFEMetadataRawOutputTags] = { 0 };

    // Neutral point default value set it to 0.
    Rational    neutralPoint[3]                         = { { 0 } };

    pDataCount[dataIndex] = 3;
    pData[dataIndex]      = &neutralPoint;
    dataIndex++;

    WriteDataList(IFEMetadataRawOutputTags, pData, pDataCount, NumIFEMetadataRawOutputTags);

    return result;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::PostMetadata
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFENode::PostMetadata(
    const ISPInputData* pInputData)
{
    CamxResult   result                             = CamxResultSuccess;
    UINT         dataIndex                          = 0;
    const VOID*  pData[IFETotalMetadataTags]        = { 0 };
    UINT         pDataCount[IFETotalMetadataTags]   = { 0 };
    UINT         pVendorTag[IFETotalMetadataTags]   = { 0 };

    // prepare IFE properties to publish
    PrepareIFEProperties(pInputData, pVendorTag, pData, pDataCount, &dataIndex);

    // prepare HAL metadata tags
    PrepareIFEHALTags(pInputData, pVendorTag, pData, pDataCount, &dataIndex);

    // Prepare IFE Vendor tags
    PrepareIFEVendorTags(pInputData, pVendorTag, pData, pDataCount, &dataIndex);

    // Post metadata tags
    CAMX_ASSERT(IFETotalMetadataTags >= dataIndex);
    if (IFETotalMetadataTags >= dataIndex)
    {
        WriteDataList(pVendorTag, pData, pDataCount, dataIndex);
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupISP, "dataIndex (%d), has exceeded IFETotalMetadataTags (%d)!",
            dataIndex, IFETotalMetadataTags);
        result = CamxResultEOverflow;
    }


    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::PrepareIFEProperties
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IFENode::PrepareIFEProperties(
    const ISPInputData* pInputData,
    UINT*               pVendorTag,
    const VOID**        ppData,
    UINT*               pDataCount,
    UINT*               pDataIndex)
{
    IFECropInfo* pCropInfo;
    IFECropInfo* pAppliedCropInfo;
    UINT         dataIndex = *pDataIndex;

    if (IFEModuleMode::DualIFENormal == m_mode)
    {
        // Write and publish the IFE residual crop info
        pCropInfo        = &m_ISPFramelevelData.metadata.cropInfo;

        // Write and publish the IFE applied crop info
        pAppliedCropInfo = &m_ISPFramelevelData.metadata.appliedCropInfo;
    }
    else
    {
        // Write and publish the IFE residual crop info
        pCropInfo        = &pInputData->pCalculatedData->metadata.cropInfo;

        // Write and publish the IFE applied crop info
        pAppliedCropInfo = &pInputData->pCalculatedData->metadata.appliedCropInfo;
    }

    CAMX_LOG_VERBOSE(CamxLogGroupISP, "IFE:%d <crop> Digit [%d,%d,%d,%d] Applied [%d,%d,%d,%d] req %llu",
        InstanceID(),
        pCropInfo->fullPath.left, pCropInfo->fullPath.top, pCropInfo->fullPath.width, pCropInfo->fullPath.height,
        pAppliedCropInfo->fullPath.left, pAppliedCropInfo->fullPath.top,
        pAppliedCropInfo->fullPath.width, pAppliedCropInfo->fullPath.height,
        pInputData->frameNum);

    pVendorTag[dataIndex]   = PropertyIDIFEDigitalZoom;
    pDataCount[dataIndex]   = sizeof(IFECropInfo);
    ppData[dataIndex++]     = pCropInfo;

    pVendorTag[dataIndex]   = PropertyIDIFEAppliedCrop;
    pDataCount[dataIndex]   = sizeof(IFECropInfo);
    ppData[dataIndex++]     = pAppliedCropInfo;

    // Write and publish internal metadata
    pVendorTag[dataIndex]   = PropertyIDIFEGammaOutput;
    pDataCount[dataIndex]   = sizeof(GammaInfo);
    ppData[dataIndex++]     = &m_ISPData[2].gammaOutput;

    PropertyISPBFStats* pBFStatsProperty = &m_BFStatsMetadata;
    PrepareBFStatsMetadata(pInputData, pBFStatsProperty);
    pVendorTag[dataIndex]   = PropertyIDISPBFConfig;
    pDataCount[dataIndex]   = sizeof(PropertyISPBFStats);
    ppData[dataIndex++]     = pBFStatsProperty;

    PropertyISPIHistStats* pIHistStatsProperty = &m_IHistStatsMetadata;
    PrepareIHistStatsMetadata(pInputData, pIHistStatsProperty);
    pVendorTag[dataIndex]   = PropertyIDISPIHistConfig;
    pDataCount[dataIndex]   = sizeof(PropertyISPIHistStats);
    ppData[dataIndex++]     = pIHistStatsProperty;

    PropertyISPAWBBGStats* pAWBBGStatsProperty = &m_AWBBGStatsMetadata;
    PrepareAWBBGStatsMetadata(pInputData, pAWBBGStatsProperty);
    pVendorTag[dataIndex]   = PropertyIDISPAWBBGConfig;
    pDataCount[dataIndex]   = sizeof(PropertyISPAWBBGStats);
    ppData[dataIndex++]     = pAWBBGStatsProperty;

    PropertyISPHDRBEStats* pHDRBEStatsProperty = &m_HDRBEStatsMetadata;
    PrepareHDRBEStatsMetadata(pInputData, pHDRBEStatsProperty);
    pVendorTag[dataIndex]   = PropertyIDISPHDRBEConfig;
    pDataCount[dataIndex]   = sizeof(PropertyISPHDRBEStats);
    ppData[dataIndex++]     = pHDRBEStatsProperty;

    PropertyISPBHistStats* pBHistStatsProperty = &m_bhistStatsMetadata;
    PrepareBHistStatsMetadata(pInputData, pBHistStatsProperty);
    pVendorTag[dataIndex]   = PropertyIDISPBHistConfig;
    pDataCount[dataIndex]   = sizeof(PropertyISPBHistStats);
    ppData[dataIndex++]     = pBHistStatsProperty;

    PropertyISPHDRBHistStats* pHDRBHistStatsProperty = &m_HDRBHistStatsMetadata;
    PrepareHDRBHistStatsMetadata(pInputData, pHDRBHistStatsProperty);
    pVendorTag[dataIndex]   = PropertyIDISPHDRBHistConfig;
    pDataCount[dataIndex]   = sizeof(PropertyISPHDRBHistStats);
    ppData[dataIndex++]     = pHDRBHistStatsProperty;

    pVendorTag[dataIndex]   = PropertyIDIFEScaleOutput;
    pDataCount[dataIndex]   = sizeof(IFEScalerOutput);
    ppData[dataIndex++]     = &pInputData->pCalculatedData->scalerOutput[1];

    PropertyISPTintlessBG* pTintlessBGStatsProperty = &m_tintlessBGStatsMetadata;
    PrepareTintlessBGStatsMetadata(pInputData, pTintlessBGStatsProperty);
    pVendorTag[dataIndex]   = PropertyIDISPTintlessBGConfig;
    pDataCount[dataIndex]   = sizeof(PropertyISPTintlessBG);
    ppData[dataIndex++]     = pTintlessBGStatsProperty;

    PropertyISPRSStats* pRSStatsProperty = &m_RSStatsMetadata;
    PrepareRSStatsMetadata(pInputData, pRSStatsProperty);
    pVendorTag[dataIndex]   = PropertyIDISPRSConfig;
    pDataCount[dataIndex]   = sizeof(PropertyISPRSStats);
    ppData[dataIndex++]     = pRSStatsProperty;

    PropertyISPCSStats* pCSStatsProperty = &m_CSStatsMetadata;
    PrepareCSStatsMetadata(pInputData, pCSStatsProperty);
    pVendorTag[dataIndex]   = PropertyIDISPCSConfig;
    pDataCount[dataIndex]   = sizeof(PropertyISPCSStats);
    ppData[dataIndex++]     = pCSStatsProperty;

    /**
     * No matter the Single IFE/ Dual IFE, the pCalculatedData will always point to the m_ISPData[2]
     * Always update the percentageOfGTM with the latest update.
     */
    if (NULL != pInputData->triggerData.pADRCData)
    {
        pVendorTag[dataIndex]   = PropertyIDIFEADRCInfoOutput;
        pDataCount[dataIndex]   = sizeof(ADRCData);
        ppData[dataIndex++]     = pInputData->triggerData.pADRCData;
    }
    else
    {
        ADRCData adrcInfo;
        // Avoid blindly memsetting the whole struct since it is quite large
        adrcInfo.version        = 0;
        adrcInfo.enable         = FALSE;
        adrcInfo.gtmEnable      = FALSE;
        adrcInfo.ltmEnable      = FALSE;

        pVendorTag[dataIndex]   = PropertyIDIFEADRCInfoOutput;
        pDataCount[dataIndex]   = sizeof(ADRCData);
        ppData[dataIndex++]     = &adrcInfo;
    }

    pVendorTag[dataIndex]   = PropertyIDIPEGamma15PreCalculation;
    pDataCount[dataIndex]   = sizeof(IPEGammaPreOutput);
    ppData[dataIndex++]     = &pInputData->pCalculatedData->IPEGamma15PreCalculationOutput;

    *pDataIndex = dataIndex;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::PrepareIFEVendorTags
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IFENode::PrepareIFEVendorTags(
    const ISPInputData* pInputData,
    UINT*               pVendorTag,
    const VOID**        ppVendorTagData,
    UINT*               pVendorTagCount,
    UINT*               pDataIndex)
{
    UINT         dataIndex = *pDataIndex;
    IFECropInfo* pCropInfo;
    IFECropInfo* pAppliedCropInfo;
    INT32        sensorWidth;
    INT32        sensorHeight;
    FLOAT        widthRatio;
    FLOAT        heightRatio;

    if (IFEModuleMode::DualIFENormal == m_mode)
    {
        // Write and publish the IFE residual crop info
        pCropInfo        = &m_ISPFramelevelData.metadata.cropInfo;

        // Write and publish the IFE applied crop info
        pAppliedCropInfo = &m_ISPFramelevelData.metadata.appliedCropInfo;
    }
    else
    {
        // Write and publish the IFE residual crop info
        pCropInfo        = &pInputData->pCalculatedData->metadata.cropInfo;

        // Write and publish the IFE applied crop info
        pAppliedCropInfo = &pInputData->pCalculatedData->metadata.appliedCropInfo;
    }

    sensorWidth     = m_pSensorModeData->resolution.outputWidth;
    sensorHeight    = m_pSensorModeData->resolution.outputHeight;

    // CSID crop override
    if (TRUE == EnableCSIDCropOverridingForSingleIFE())
    {
        sensorWidth  = m_instanceProperty.IFECSIDWidth;
        sensorHeight = m_instanceProperty.IFECSIDHeight;
    }

    // sensor width shift for YUV sensor
    if (TRUE == m_ISPInputSensorData.isYUV)
    {
        sensorWidth >>= 1;
    }

    // Prepare SensorIFEAppliedCrop
    CAMX_ASSERT((0 != m_sensorActiveArrayWidth) && (0 != m_sensorActiveArrayHeight));
    CAMX_ASSERT((0 != sensorWidth) && (0 != sensorHeight));

    widthRatio  = static_cast<FLOAT>(m_sensorActiveArrayWidth) / static_cast<FLOAT>(sensorWidth);
    heightRatio = static_cast<FLOAT>(m_sensorActiveArrayHeight) / static_cast<FLOAT>(sensorHeight);

    // Update the crop window for each stream with respect to sensor
    m_modifiedCropWindow.fullPath.left          = Utils::RoundFLOAT(pAppliedCropInfo->fullPath.left   * widthRatio);
    m_modifiedCropWindow.fullPath.top           = Utils::RoundFLOAT(pAppliedCropInfo->fullPath.top    * heightRatio);
    m_modifiedCropWindow.fullPath.width         = Utils::RoundFLOAT(pAppliedCropInfo->fullPath.width  * widthRatio);
    m_modifiedCropWindow.fullPath.height        = Utils::RoundFLOAT(pAppliedCropInfo->fullPath.height * heightRatio);

    m_modifiedCropWindow.FDPath.left            = Utils::RoundFLOAT(pAppliedCropInfo->FDPath.left     * widthRatio);
    m_modifiedCropWindow.FDPath.top             = Utils::RoundFLOAT(pAppliedCropInfo->FDPath.top      * heightRatio);
    m_modifiedCropWindow.FDPath.width           = Utils::RoundFLOAT(pAppliedCropInfo->FDPath.width    * widthRatio);
    m_modifiedCropWindow.FDPath.height          = Utils::RoundFLOAT(pAppliedCropInfo->FDPath.height   * heightRatio);

    m_modifiedCropWindow.DS4Path.left           = Utils::RoundFLOAT(pAppliedCropInfo->DS4Path.left    * widthRatio);
    m_modifiedCropWindow.DS4Path.top            = Utils::RoundFLOAT(pAppliedCropInfo->DS4Path.top     * heightRatio);
    m_modifiedCropWindow.DS4Path.width          = Utils::RoundFLOAT(pAppliedCropInfo->DS4Path.width   * widthRatio);
    m_modifiedCropWindow.DS4Path.height         = Utils::RoundFLOAT(pAppliedCropInfo->DS4Path.height  * heightRatio);

    m_modifiedCropWindow.DS16Path.left          = Utils::RoundFLOAT(pAppliedCropInfo->DS16Path.left   * widthRatio);
    m_modifiedCropWindow.DS16Path.top           = Utils::RoundFLOAT(pAppliedCropInfo->DS16Path.top    * heightRatio);
    m_modifiedCropWindow.DS16Path.width         = Utils::RoundFLOAT(pAppliedCropInfo->DS16Path.width  * widthRatio);
    m_modifiedCropWindow.DS16Path.height        = Utils::RoundFLOAT(pAppliedCropInfo->DS16Path.height * heightRatio);

    m_modifiedCropWindow.displayFullPath.left   = Utils::RoundFLOAT(pAppliedCropInfo->displayFullPath.left   * widthRatio);
    m_modifiedCropWindow.displayFullPath.top    = Utils::RoundFLOAT(pAppliedCropInfo->displayFullPath.top    * heightRatio);
    m_modifiedCropWindow.displayFullPath.width  = Utils::RoundFLOAT(pAppliedCropInfo->displayFullPath.width  * widthRatio);
    m_modifiedCropWindow.displayFullPath.height = Utils::RoundFLOAT(pAppliedCropInfo->displayFullPath.height * heightRatio);

    m_modifiedCropWindow.displayDS4Path.left    = Utils::RoundFLOAT(pAppliedCropInfo->displayDS4Path.left    * widthRatio);
    m_modifiedCropWindow.displayDS4Path.top     = Utils::RoundFLOAT(pAppliedCropInfo->displayDS4Path.top     * heightRatio);
    m_modifiedCropWindow.displayDS4Path.width   = Utils::RoundFLOAT(pAppliedCropInfo->displayDS4Path.width   * widthRatio);
    m_modifiedCropWindow.displayDS4Path.height  = Utils::RoundFLOAT(pAppliedCropInfo->displayDS4Path.height  * heightRatio);

    m_modifiedCropWindow.displayDS16Path.left   = Utils::RoundFLOAT(pAppliedCropInfo->displayDS16Path.left   * widthRatio);
    m_modifiedCropWindow.displayDS16Path.top    = Utils::RoundFLOAT(pAppliedCropInfo->displayDS16Path.top    * heightRatio);
    m_modifiedCropWindow.displayDS16Path.width  = Utils::RoundFLOAT(pAppliedCropInfo->displayDS16Path.width  * widthRatio);
    m_modifiedCropWindow.displayDS16Path.height = Utils::RoundFLOAT(pAppliedCropInfo->displayDS16Path.height * heightRatio);

    // Post SensorIFEAppliedCrop, Index 0 maps to SensorIFEAppliedCrop, to change update IFEOutputVendorTags ordering
    pVendorTag[dataIndex]       = m_vendorTagArray[SensorIFEAppliedCropIndex];
    ppVendorTagData[dataIndex]  = { &m_modifiedCropWindow };
    pVendorTagCount[dataIndex]  = { sizeof(m_modifiedCropWindow) };
    dataIndex++;

    // Post ResidualCrop, Index 1 maps to ResidualCrop, to change update IFEOutputVendorTags ordering
    pVendorTag[dataIndex]       = m_vendorTagArray[ResidualCropIndex];
    ppVendorTagData[dataIndex]  = { pCropInfo };
    pVendorTagCount[dataIndex]  = { sizeof(IFECropInfo) };
    dataIndex++;

    // Post AppliedCrop, Index 2 maps to AppliedCrop, to change update IFEOutputVendorTags ordering
    pVendorTag[dataIndex]       = m_vendorTagArray[AppliedCropIndex];
    ppVendorTagData[dataIndex]  = { pAppliedCropInfo };
    pVendorTagCount[dataIndex]  = { sizeof(IFECropInfo) };
    dataIndex++;

    // Post to Gamma, Index 3 maps to Gamma Info, to change update IFEOutputVendorTags ordering
    pVendorTag[dataIndex]       = m_vendorTagArray[GammaInfoIndex];
    ppVendorTagData[dataIndex]  = { &m_ISPData[2].gammaOutput };
    pVendorTagCount[dataIndex]  = { sizeof(GammaInfo) };
    dataIndex++;

    // check for MultiCameraIdRole vendor tag and then read and post dual camera metadata if available
    if (0 != m_vendorTagArray[MultiCameraIdRoleIndex])
    {
        const UINT multiCamIdTag[]  = { m_vendorTagArray[MultiCameraIdRoleIndex] | InputMetadataSectionMask };
        VOID* pDataMultiCamIdRole[] = { 0 };
        MultiCameraIdRole* pMCCInfo = NULL;
        UINT64 dataOffset[1]        = { 0 };
        if (CDKResultSuccess == GetDataList(multiCamIdTag, pDataMultiCamIdRole, dataOffset, 1))
        {
            // Post MultiCameraIdRole, Index 4 maps to MultiCameraIdRole, to change update IFEOutputVendorTags ordering
            pVendorTag[dataIndex]       = m_vendorTagArray[MultiCameraIdRoleIndex];
            ppVendorTagData[dataIndex]  = { pDataMultiCamIdRole[0] };
            pVendorTagCount[dataIndex]  = { sizeof(MultiCameraIdRole) };
            dataIndex++;
            pMCCInfo = reinterpret_cast<MultiCameraIdRole*>(pDataMultiCamIdRole[0]);
        }

        // Post MasterCamera, Index 5 maps to MasterCamera, to change update IFEOutputVendorTags ordering
        const UINT masterCameraTag[]        = { m_vendorTagArray[MasterCameraIndex] | InputMetadataSectionMask };
        VOID* pMasterCameraGetData[]        = { 0 };
        UINT64 masterCameraDataOffset[1]    = { 0 };
        GetDataList(masterCameraTag, pMasterCameraGetData, masterCameraDataOffset, 1);
        if (NULL != pMasterCameraGetData[0])
        {
            // Post MasterCamera, Index 5 maps to MasterCamera, to change update IFEOutputVendorTags ordering
            pVendorTag[dataIndex]       = m_vendorTagArray[MasterCameraIndex];
            ppVendorTagData[dataIndex]  = { pMasterCameraGetData[0] };
            pVendorTagCount[dataIndex]  = { 1 };
            dataIndex++;
        }
    }

    // Post crop_regions, Index 6 maps to crop_regions, to change update IFEOutputVendorTags ordering
    const UINT tagReadInput[] = { m_vendorTagArray[CropRegionsIndex] | InputMetadataSectionMask };
    VOID* pDataCropRegions[]  = { 0 };
    UINT64 dataOffset[1]      = { 0 };
    if (CDKResultSuccess == GetDataList(tagReadInput , pDataCropRegions, dataOffset, 1))
    {
        // Post crop_regions, Index 6 maps to crop_regions, to change update IFEOutputVendorTags ordering
        pVendorTag[dataIndex]       = m_vendorTagArray[CropRegionsIndex];
        ppVendorTagData[dataIndex]  = { pDataCropRegions[0] };
        pVendorTagCount[dataIndex]  = { sizeof(CaptureRequestCropRegions) };
        dataIndex++;
    }

    *pDataIndex = dataIndex;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::PrepareIFEHALTags
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFENode::PrepareIFEHALTags(
    const ISPInputData* pInputData,
    UINT*               pVendorTag,
    const VOID**        ppData,
    UINT*               pDataCount,
    UINT*               pDataIndex)
{
    CamxResult   result             = CamxResultSuccess;
    UINT         index              = *pDataIndex;
    FLOAT        widthRatio;
    FLOAT        heightRatio;
    INT32        sensorWidth;
    INT32        sensorHeight;

    m_scalerCrop    = pInputData->pHALTagsData->HALCrop;
    sensorWidth     = m_pSensorModeData->resolution.outputWidth;
    sensorHeight    = m_pSensorModeData->resolution.outputHeight;

    // CSID crop override
    if (TRUE == EnableCSIDCropOverridingForSingleIFE())
    {
        sensorWidth  = m_instanceProperty.IFECSIDWidth;
        sensorHeight = m_instanceProperty.IFECSIDHeight;
    }

    // sensor width shift for YUV sensor
    if (TRUE == m_ISPInputSensorData.isYUV)
    {
        sensorWidth >>= 1;
    }

    CAMX_ASSERT((0 != m_sensorActiveArrayWidth) && (0 != m_sensorActiveArrayHeight));
    widthRatio  = static_cast<FLOAT>(m_sensorActiveArrayWidth) / static_cast<FLOAT>(sensorWidth);
    heightRatio = static_cast<FLOAT>(m_sensorActiveArrayHeight) / static_cast<FLOAT>(sensorHeight);

    m_scalerCrop.left   = Utils::RoundFLOAT(m_scalerCrop.left   * widthRatio);
    m_scalerCrop.top    = Utils::RoundFLOAT(m_scalerCrop.top    * heightRatio);
    m_scalerCrop.width  = Utils::RoundFLOAT(m_scalerCrop.width  * widthRatio);
    m_scalerCrop.height = Utils::RoundFLOAT(m_scalerCrop.height * heightRatio);

    pVendorTag[index]   = ScalerCropRegion;
    pDataCount[index]   = 4;
    ppData[index]       = &m_scalerCrop;
    index++;

    if (IFEModuleMode::DualIFENormal == m_mode)
    {
        pVendorTag[index]   = ShadingMode;
        pDataCount[index]   = 1;
        ppData[index]       = &m_ISPData[1].lensShadingInfo.shadingMode;
        index++;

        pVendorTag[index]   = StatisticsLensShadingMapMode;
        pDataCount[index]   = 1;
        ppData[index]       = &m_ISPData[1].lensShadingInfo.lensShadingMapMode;
        index++;

        pVendorTag[index]   = LensInfoShadingMapSize;
        pDataCount[index]   = 2;
        ppData[index]       = &m_ISPData[1].lensShadingInfo.lensShadingMapSize;
        index++;
    }
    else
    {
        pVendorTag[index]   = ShadingMode;
        pDataCount[index]   = 1;
        ppData[index]       = &pInputData->pCalculatedData->lensShadingInfo.shadingMode;
        index++;

        pVendorTag[index]   = StatisticsLensShadingMapMode;
        pDataCount[index]   = 1;
        ppData[index]       = &pInputData->pCalculatedData->lensShadingInfo.lensShadingMapMode;
        index++;

        pVendorTag[index]   = LensInfoShadingMapSize;
        pDataCount[index]   = 2;
        ppData[index]       = &pInputData->pCalculatedData->lensShadingInfo.lensShadingMapSize;
        index++;
    }

    if (StatisticsLensShadingMapModeOn == pInputData->pCalculatedData->lensShadingInfo.lensShadingMapMode)
    {
        pDataCount[index] = (TotalChannels * MESH_ROLLOFF_SIZE);
        pVendorTag[index] = StatisticsLensShadingMap;
        if (IFEModuleMode::DualIFENormal == m_mode)
        {
            ppData[index] = m_ISPData[1].lensShadingInfo.lensShadingMap;
        }
        else
        {
            ppData[index] = pInputData->pCalculatedData->lensShadingInfo.lensShadingMap;
        }
        index++;
    }
    else
    {
        pVendorTag[index]  = StatisticsLensShadingMap;
        pDataCount[index]   = 1;
        ppData[index]       = NULL;
        index++;
    }

    for (UINT32 index = 0; index < ISPChannelMax; index++)
    {
        m_dynamicBlackLevel[index] =
            static_cast<FLOAT>(((m_ISPData[2].BLSblackLevelOffset +
                                 m_ISPData[2].linearizationAppliedBlackLevel[index]) >>
                                (IFEPipelineBitWidth - pInputData->sensorBitWidth)));
    }

    pVendorTag[index]   = SensorDynamicBlackLevel;
    pDataCount[index]   = 4;
    ppData[index]       = m_dynamicBlackLevel;
    index++;

    pVendorTag[index]   = SensorDynamicWhiteLevel;
    pDataCount[index]   = 1;
    ppData[index]       = &m_pSensorCaps->whiteLevel;
    index++;


    pVendorTag[index]   = SensorBlackLevelPattern;
    pDataCount[index]   = 4;
    ppData[index]       = &m_pSensorCaps->blackLevelPattern;
    index++;

    ComputeNeutralPoint(pInputData, &m_neutralPoint[0]);
    pVendorTag[index]   = SensorNeutralColorPoint;
    pDataCount[index]   = sizeof(m_neutralPoint) / sizeof(Rational);
    ppData[index]       = m_neutralPoint;
    index++;

    pVendorTag[index]   = BlackLevelLock;
    pDataCount[index]   = 1;
    ppData[index]       = &m_ISPData[2].blackLevelLock;
    index++;

    pVendorTag[index]   = ColorCorrectionGains;
    pDataCount[index]   = 4;
    ppData[index]       = &(m_ISPData[2].colorCorrectionGains);
    index++;

    pVendorTag[index]   = ControlPostRawSensitivityBoost;
    pDataCount[index]   = 1;
    ppData[index]       = &(m_ISPData[2].controlPostRawSensitivityBoost);
    index++;

    pVendorTag[index]   = HotPixelMode;
    pDataCount[index]   = 1;
    ppData[index]       = &(m_ISPData[2].hotPixelMode);
    index++;

    if (IFEModuleMode::DualIFENormal == m_mode)
    {
        pVendorTag[index]   = NoiseReductionMode;
        pDataCount[index]   = 1;
        ppData[index]       = &m_ISPData[1].noiseReductionMode;
        index++;
    }
    else
    {
        pVendorTag[index]   = NoiseReductionMode;
        pDataCount[index]   = 1;
        ppData[index]       = &pInputData->pCalculatedData->noiseReductionMode;
        index++;
    }

    pVendorTag[index]   = StatisticsHotPixelMapMode;
    pDataCount[index]   = 1;
    ppData[index]       = &pInputData->pHALTagsData->statisticsHotPixelMapMode;
    index++;

    pVendorTag[index]   = TonemapMode;
    pDataCount[index]   = 1;
    ppData[index]       = &pInputData->pHALTagsData->tonemapCurves.tonemapMode;
    index++;

    *pDataIndex = index;

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::ComputeNeutralPoint
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFENode::ComputeNeutralPoint(
    const ISPInputData* pInputData,
    Rational*           pNeutralPoint)
{
    FLOAT inverseWbGain[3]   = { 0 };
    INT32 neutralDenominator = (1 << 10);

    if (pInputData->pAWBUpdateData->AWBGains.rGain != 0 &&
        pInputData->pAWBUpdateData->AWBGains.gGain != 0 &&
        pInputData->pAWBUpdateData->AWBGains.bGain != 0)
    {
        inverseWbGain[0] = pInputData->pAWBUpdateData->AWBGains.gGain / pInputData->pAWBUpdateData->AWBGains.rGain;
        inverseWbGain[1] = pInputData->pAWBUpdateData->AWBGains.gGain;
        inverseWbGain[2] = pInputData->pAWBUpdateData->AWBGains.gGain / pInputData->pAWBUpdateData->AWBGains.bGain;

        pNeutralPoint[0].numerator   = CamX::Utils::FloatToQNumber(inverseWbGain[0], neutralDenominator);
        pNeutralPoint[1].numerator   = CamX::Utils::FloatToQNumber(inverseWbGain[1], neutralDenominator);
        pNeutralPoint[2].numerator   = CamX::Utils::FloatToQNumber(inverseWbGain[2], neutralDenominator);
    }
    else
    {
        pNeutralPoint[0].numerator   = neutralDenominator;
        pNeutralPoint[1].numerator   = neutralDenominator;
        pNeutralPoint[2].numerator   = neutralDenominator;

        CAMX_LOG_ERROR(CamxLogGroupISP, "Input WB gain is 0");
    }

    pNeutralPoint[0].denominator = neutralDenominator;
    pNeutralPoint[1].denominator = neutralDenominator;
    pNeutralPoint[2].denominator = neutralDenominator;

    CAMX_LOG_VERBOSE(CamxLogGroupISP, "WBGains: R:%f G:%f B:%f NP: %d/%d, %d/%d, %d/%d",
        pInputData->pAWBUpdateData->AWBGains.rGain, pInputData->pAWBUpdateData->AWBGains.gGain,
        pInputData->pAWBUpdateData->AWBGains.bGain,
        pNeutralPoint[0].numerator, pNeutralPoint[0].denominator,
        pNeutralPoint[1].numerator, pNeutralPoint[1].denominator,
        pNeutralPoint[2].numerator, pNeutralPoint[2].denominator);

    return CamxResultSuccess;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::ProgramIQModuleEnableConfig()
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFENode::ProgramIQModuleEnableConfig(
    ISPInputData* pInputData)
{
    CamxResult             result        = CamxResultSuccess;
    IFEModuleEnableConfig* pModuleEnable = &pInputData->pCalculatedData->moduleEnable;
    UINT32                 raw_dump = 0;;

    _ife_ife_0_vfe_module_lens_en*                pLensProcessingModuleConfig     =
        &pModuleEnable->lensProcessingModuleConfig.bitfields;
    _ife_ife_0_vfe_module_color_en*               pColorProcessingModuleConfig    =
        &pModuleEnable->colorProcessingModuleConfig.bitfields;
    _ife_ife_0_vfe_module_zoom_en*                pFrameProcessingModuleConfig    =
        &pModuleEnable->frameProcessingModuleConfig.bitfields;
    _ife_ife_0_vfe_fd_out_y_crop_rnd_clamp_cfg*   pFDLumaCropRoundClampConfig     =
        &pModuleEnable->FDLumaCropRoundClampConfig.bitfields;
    _ife_ife_0_vfe_fd_out_c_crop_rnd_clamp_cfg*   pFDChromaCropRoundClampConfig   =
        &pModuleEnable->FDChromaCropRoundClampConfig.bitfields;
    _ife_ife_0_vfe_full_out_y_crop_rnd_clamp_cfg* pFullLumaCropRoundClampConfig   =
        &pModuleEnable->fullLumaCropRoundClampConfig.bitfields;
    _ife_ife_0_vfe_full_out_c_crop_rnd_clamp_cfg* pFullChromaCropRoundClampConfig =
        &pModuleEnable->fullChromaCropRoundClampConfig.bitfields;
    _ife_ife_0_vfe_ds4_y_crop_rnd_clamp_cfg*      pDS4LumaCropRoundClampConfig    =
        &pModuleEnable->DS4LumaCropRoundClampConfig.bitfields;
    _ife_ife_0_vfe_ds4_c_crop_rnd_clamp_cfg*      pDS4ChromaCropRoundClampConfig  =
        &pModuleEnable->DS4ChromaCropRoundClampConfig.bitfields;
    _ife_ife_0_vfe_ds16_y_crop_rnd_clamp_cfg*     pDS16LumaCropRoundClampConfig   =
        &pModuleEnable->DS16LumaCropRoundClampConfig.bitfields;
    _ife_ife_0_vfe_ds16_c_crop_rnd_clamp_cfg*     pDS16ChromaCropRoundClampConfig =
        &pModuleEnable->DS16ChromaCropRoundClampConfig.bitfields;
    _ife_ife_0_vfe_module_stats_en*               pStatsEnable                    = &pModuleEnable->statsEnable.bitfields;
    _ife_ife_0_vfe_stats_cfg*                     pStatsConfig                    = &pModuleEnable->statsConfig.bitfields;

    _ife_ife_0_vfe_module_disp_en*                 pFrameProcessingDisplayModuleConfig    =
        &pModuleEnable->frameProcessingDisplayModuleConfig.bitfields;
    _ife_ife_0_vfe_disp_out_y_crop_rnd_clamp_cfg*  pDisplayFullLumaCropRoundClampConfig   =
        &pModuleEnable->displayFullLumaCropRoundClampConfig.bitfields;
    _ife_ife_0_vfe_disp_out_c_crop_rnd_clamp_cfg*  pDisplayFullChromaCropRoundClampConfig =
        &pModuleEnable->displayFullChromaCropRoundClampConfig.bitfields;
    _ife_ife_0_vfe_disp_ds4_y_crop_rnd_clamp_cfg*  pDisplayDS4LumaCropRoundClampConfig    =
        &pModuleEnable->displayDS4LumaCropRoundClampConfig.bitfields;
    _ife_ife_0_vfe_disp_ds4_c_crop_rnd_clamp_cfg*  pDisplayDS4ChromaCropRoundClampConfig  =
        &pModuleEnable->displayDS4ChromaCropRoundClampConfig.bitfields;
    _ife_ife_0_vfe_disp_ds16_y_crop_rnd_clamp_cfg* pDisplayDS16LumaCropRoundClampConfig   =
        &pModuleEnable->displayDS16LumaCropRoundClampConfig.bitfields;
    _ife_ife_0_vfe_disp_ds16_c_crop_rnd_clamp_cfg* pDisplayDS16ChromaCropRoundClampConfig =
        &pModuleEnable->displayDS16ChromaCropRoundClampConfig.bitfields;

    if (NULL != pInputData && (NULL != pInputData->pCmdBuffer))
    {
        CmdBuffer* pCmdBuffer = pInputData->pCmdBuffer;

        // Anything enabled by common IQ modules should be added to the set. In general, this should be done based on the
        // specific fields and how it's affected by dual IFE.
        pModuleEnable->lensProcessingModuleConfig.u32All    |= m_ISPData[2].moduleEnable.lensProcessingModuleConfig.u32All;
        pModuleEnable->colorProcessingModuleConfig.u32All   |= m_ISPData[2].moduleEnable.colorProcessingModuleConfig.u32All;
        pModuleEnable->frameProcessingModuleConfig.u32All   |= m_ISPData[2].moduleEnable.frameProcessingModuleConfig.u32All;
        pModuleEnable->FDLumaCropRoundClampConfig.u32All    |= m_ISPData[2].moduleEnable.FDLumaCropRoundClampConfig.u32All;
        pModuleEnable->FDChromaCropRoundClampConfig.u32All  |= m_ISPData[2].moduleEnable.FDChromaCropRoundClampConfig.u32All;
        pModuleEnable->fullLumaCropRoundClampConfig.u32All  |= m_ISPData[2].moduleEnable.fullLumaCropRoundClampConfig.u32All;
        pModuleEnable->fullChromaCropRoundClampConfig.u32All
            |= m_ISPData[2].moduleEnable.fullChromaCropRoundClampConfig.u32All;
        pModuleEnable->DS4LumaCropRoundClampConfig.u32All   |= m_ISPData[2].moduleEnable.DS4LumaCropRoundClampConfig.u32All;
        pModuleEnable->DS4ChromaCropRoundClampConfig.u32All |= m_ISPData[2].moduleEnable.DS4ChromaCropRoundClampConfig.u32All;
        pModuleEnable->DS16LumaCropRoundClampConfig.u32All  |= m_ISPData[2].moduleEnable.DS16LumaCropRoundClampConfig.u32All;
        pModuleEnable->DS16ChromaCropRoundClampConfig.u32All
            |= m_ISPData[2].moduleEnable.DS16ChromaCropRoundClampConfig.u32All;
        pModuleEnable->statsEnable.u32All                   |= m_ISPData[2].moduleEnable.statsEnable.u32All;
        pModuleEnable->statsConfig.u32All                   |= m_ISPData[2].moduleEnable.statsConfig.u32All;

        pModuleEnable->frameProcessingDisplayModuleConfig.u32All    |=
            m_ISPData[2].moduleEnable.frameProcessingDisplayModuleConfig.u32All;
        pModuleEnable->displayFullLumaCropRoundClampConfig.u32All   |=
            m_ISPData[2].moduleEnable.displayFullLumaCropRoundClampConfig.u32All;
        pModuleEnable->displayFullChromaCropRoundClampConfig.u32All |=
            m_ISPData[2].moduleEnable.displayFullChromaCropRoundClampConfig.u32All;
        pModuleEnable->displayDS4LumaCropRoundClampConfig.u32All    |=
            m_ISPData[2].moduleEnable.displayDS4LumaCropRoundClampConfig.u32All;
        pModuleEnable->displayDS4ChromaCropRoundClampConfig.u32All  |=
            m_ISPData[2].moduleEnable.displayDS4ChromaCropRoundClampConfig.u32All;
        pModuleEnable->displayDS16LumaCropRoundClampConfig.u32All   |=
            m_ISPData[2].moduleEnable.displayDS16LumaCropRoundClampConfig.u32All;
        pModuleEnable->displayDS16ChromaCropRoundClampConfig.u32All |=
            m_ISPData[2].moduleEnable.displayDS16ChromaCropRoundClampConfig.u32All;

        /// @todo (CAMX-812) Following modules are still disabled
        pLensProcessingModuleConfig->PEDESTAL_EN        = 0;
        // pLensProcessingModuleConfig->CHROMA_UPSAMPLE_EN = 0;
        pLensProcessingModuleConfig->HDR_MAC_EN =
            m_ISPData[2].moduleEnable.lensProcessingModuleConfig.bitfields.HDR_MAC_EN;
        pLensProcessingModuleConfig->HDR_RECON_EN =
            m_ISPData[2].moduleEnable.lensProcessingModuleConfig.bitfields.HDR_RECON_EN;
        pLensProcessingModuleConfig->PDAF_EN            =
            m_ISPData[2].moduleEnable.lensProcessingModuleConfig.bits.PDAF_EN;
        pColorProcessingModuleConfig->GTM_EN            =
            m_ISPData[2].moduleEnable.colorProcessingModuleConfig.bits.GTM_EN;
        pColorProcessingModuleConfig->RGB_LUT_EN        =
            m_ISPData[2].moduleEnable.colorProcessingModuleConfig.bitfields.RGB_LUT_EN;

        pFrameProcessingModuleConfig->SCALE_FD_EN              = m_IFEOutputPathInfo[IFEOutputPortFD].path;
        pFrameProcessingModuleConfig->CROP_FD_EN               = m_IFEOutputPathInfo[IFEOutputPortFD].path;
        pFrameProcessingModuleConfig->SCALE_VID_EN             = m_IFEOutputPathInfo[IFEOutputPortFull].path;
        pFrameProcessingModuleConfig->CROP_VID_EN              = m_IFEOutputPathInfo[IFEOutputPortFull].path;

        // Pre/Post crop modules are enabled/disabled on the basis of single/dual IFE mode
        pFrameProcessingModuleConfig->DS_4TO1_2ND_POST_CROP_EN &= m_IFEOutputPathInfo[IFEOutputPortDS16].path;
        pFrameProcessingModuleConfig->DS_4TO1_1ST_POST_CROP_EN &= m_IFEOutputPathInfo[IFEOutputPortDS4].path;
        pFrameProcessingModuleConfig->DS_4TO1_1ST_PRE_CROP_EN  &= m_IFEOutputPathInfo[IFEOutputPortDS4].path;
        pFrameProcessingModuleConfig->DS_4TO1_2ND_PRE_CROP_EN  &= m_IFEOutputPathInfo[IFEOutputPortDS16].path;

        pFrameProcessingModuleConfig->DS_4TO1_Y_1ST_EN         = m_IFEOutputPathInfo[IFEOutputPortDS4].path;
        pFrameProcessingModuleConfig->DS_4TO1_Y_2ND_EN         = m_IFEOutputPathInfo[IFEOutputPortDS16].path;
        pFrameProcessingModuleConfig->DS_4TO1_C_1ST_EN         = m_IFEOutputPathInfo[IFEOutputPortDS4].path;
        pFrameProcessingModuleConfig->DS_4TO1_C_2ND_EN         = m_IFEOutputPathInfo[IFEOutputPortDS16].path;
        pFrameProcessingModuleConfig->R2PD_1ST_EN              = m_IFEOutputPathInfo[IFEOutputPortDS4].path;
        pFrameProcessingModuleConfig->R2PD_2ND_EN              = m_IFEOutputPathInfo[IFEOutputPortDS16].path;
        pFrameProcessingModuleConfig->PDAF_OUT_EN              = m_IFEOutputPathInfo[IFEOutputPortPDAF].path;
        pFrameProcessingModuleConfig->PIXEL_RAW_DUMP_EN        =
            pModuleEnable->frameProcessingModuleConfig.bitfields.PIXEL_RAW_DUMP_EN;

        pFDLumaCropRoundClampConfig->CROP_EN          = m_IFEOutputPathInfo[IFEOutputPortFD].path;
        pFDLumaCropRoundClampConfig->CH0_ROUND_EN     = m_IFEOutputPathInfo[IFEOutputPortFD].path;
        pFDLumaCropRoundClampConfig->CH0_CLAMP_EN     = m_IFEOutputPathInfo[IFEOutputPortFD].path;

        pFDChromaCropRoundClampConfig->CROP_EN        = m_IFEOutputPathInfo[IFEOutputPortFD].path;
        pFDChromaCropRoundClampConfig->CH0_ROUND_EN   = m_IFEOutputPathInfo[IFEOutputPortFD].path;
        pFDChromaCropRoundClampConfig->CH0_CLAMP_EN   = m_IFEOutputPathInfo[IFEOutputPortFD].path;

        pFullLumaCropRoundClampConfig->CROP_EN        = m_IFEOutputPathInfo[IFEOutputPortFull].path;
        pFullLumaCropRoundClampConfig->CH0_ROUND_EN   = m_IFEOutputPathInfo[IFEOutputPortFull].path;
        pFullLumaCropRoundClampConfig->CH0_CLAMP_EN   = m_IFEOutputPathInfo[IFEOutputPortFull].path;

        pFullChromaCropRoundClampConfig->CROP_EN      = m_IFEOutputPathInfo[IFEOutputPortFull].path;
        pFullChromaCropRoundClampConfig->CH0_ROUND_EN = m_IFEOutputPathInfo[IFEOutputPortFull].path;
        pFullChromaCropRoundClampConfig->CH0_CLAMP_EN = m_IFEOutputPathInfo[IFEOutputPortFull].path;

        pDS4LumaCropRoundClampConfig->CROP_EN         = m_IFEOutputPathInfo[IFEOutputPortDS4].path;
        pDS4LumaCropRoundClampConfig->CH0_ROUND_EN    = m_IFEOutputPathInfo[IFEOutputPortDS4].path;
        pDS4LumaCropRoundClampConfig->CH0_CLAMP_EN    = m_IFEOutputPathInfo[IFEOutputPortDS4].path;

        pDS4ChromaCropRoundClampConfig->CROP_EN       = m_IFEOutputPathInfo[IFEOutputPortDS4].path;
        pDS4ChromaCropRoundClampConfig->CH0_ROUND_EN  = m_IFEOutputPathInfo[IFEOutputPortDS4].path;
        pDS4ChromaCropRoundClampConfig->CH0_CLAMP_EN  = m_IFEOutputPathInfo[IFEOutputPortDS4].path;

        pDS16LumaCropRoundClampConfig->CROP_EN        = m_IFEOutputPathInfo[IFEOutputPortDS16].path;
        pDS16LumaCropRoundClampConfig->CH0_ROUND_EN   = m_IFEOutputPathInfo[IFEOutputPortDS16].path;
        pDS16LumaCropRoundClampConfig->CH0_CLAMP_EN   = m_IFEOutputPathInfo[IFEOutputPortDS16].path;

        pDS16ChromaCropRoundClampConfig->CROP_EN      = m_IFEOutputPathInfo[IFEOutputPortDS16].path;
        pDS16ChromaCropRoundClampConfig->CH0_ROUND_EN = m_IFEOutputPathInfo[IFEOutputPortDS16].path;
        pDS16ChromaCropRoundClampConfig->CH0_CLAMP_EN = m_IFEOutputPathInfo[IFEOutputPortDS16].path;

        pFrameProcessingDisplayModuleConfig->SCALE_VID_EN = m_IFEOutputPathInfo[IFEOutputPortDisplayFull].path;
        pFrameProcessingDisplayModuleConfig->CROP_VID_EN  = m_IFEOutputPathInfo[IFEOutputPortDisplayFull].path;

        // Pre/Post crop modules are enabled/disabled on the basis of single/dual IFE mode
        pFrameProcessingDisplayModuleConfig->DS_4TO1_1ST_POST_CROP_EN &= m_IFEOutputPathInfo[IFEOutputPortDisplayDS4].path;
        pFrameProcessingDisplayModuleConfig->DS_4TO1_2ND_POST_CROP_EN &= m_IFEOutputPathInfo[IFEOutputPortDisplayDS16].path;
        pFrameProcessingDisplayModuleConfig->DS_4TO1_1ST_PRE_CROP_EN  &= m_IFEOutputPathInfo[IFEOutputPortDisplayDS4].path;
        pFrameProcessingDisplayModuleConfig->DS_4TO1_2ND_PRE_CROP_EN  &= m_IFEOutputPathInfo[IFEOutputPortDisplayDS16].path;
        pFrameProcessingDisplayModuleConfig->DS_4TO1_Y_1ST_EN          = m_IFEOutputPathInfo[IFEOutputPortDisplayDS4].path;
        pFrameProcessingDisplayModuleConfig->DS_4TO1_Y_2ND_EN          = m_IFEOutputPathInfo[IFEOutputPortDisplayDS16].path;
        pFrameProcessingDisplayModuleConfig->DS_4TO1_C_1ST_EN          = m_IFEOutputPathInfo[IFEOutputPortDisplayDS4].path;
        pFrameProcessingDisplayModuleConfig->DS_4TO1_C_2ND_EN          = m_IFEOutputPathInfo[IFEOutputPortDisplayDS16].path;
        pFrameProcessingDisplayModuleConfig->R2PD_1ST_EN               = m_IFEOutputPathInfo[IFEOutputPortDisplayDS4].path;
        pFrameProcessingDisplayModuleConfig->R2PD_2ND_EN               = m_IFEOutputPathInfo[IFEOutputPortDisplayDS16].path;

        pDisplayFullLumaCropRoundClampConfig->CROP_EN        = m_IFEOutputPathInfo[IFEOutputPortDisplayFull].path;
        pDisplayFullLumaCropRoundClampConfig->CH0_ROUND_EN   = m_IFEOutputPathInfo[IFEOutputPortDisplayFull].path;
        pDisplayFullLumaCropRoundClampConfig->CH0_CLAMP_EN   = m_IFEOutputPathInfo[IFEOutputPortDisplayFull].path;
        pDisplayFullChromaCropRoundClampConfig->CROP_EN      = m_IFEOutputPathInfo[IFEOutputPortDisplayFull].path;
        pDisplayFullChromaCropRoundClampConfig->CH0_ROUND_EN = m_IFEOutputPathInfo[IFEOutputPortDisplayFull].path;
        pDisplayFullChromaCropRoundClampConfig->CH0_CLAMP_EN = m_IFEOutputPathInfo[IFEOutputPortDisplayFull].path;

        pDisplayDS4LumaCropRoundClampConfig->CROP_EN         = m_IFEOutputPathInfo[IFEOutputPortDisplayDS4].path;
        pDisplayDS4LumaCropRoundClampConfig->CH0_ROUND_EN    = m_IFEOutputPathInfo[IFEOutputPortDisplayDS4].path;
        pDisplayDS4LumaCropRoundClampConfig->CH0_CLAMP_EN    = m_IFEOutputPathInfo[IFEOutputPortDisplayDS4].path;
        pDisplayDS4ChromaCropRoundClampConfig->CROP_EN       = m_IFEOutputPathInfo[IFEOutputPortDisplayDS4].path;
        pDisplayDS4ChromaCropRoundClampConfig->CH0_ROUND_EN  = m_IFEOutputPathInfo[IFEOutputPortDisplayDS4].path;
        pDisplayDS4ChromaCropRoundClampConfig->CH0_CLAMP_EN  = m_IFEOutputPathInfo[IFEOutputPortDisplayDS4].path;

        pDisplayDS16LumaCropRoundClampConfig->CROP_EN        = m_IFEOutputPathInfo[IFEOutputPortDisplayDS16].path;
        pDisplayDS16LumaCropRoundClampConfig->CH0_ROUND_EN   = m_IFEOutputPathInfo[IFEOutputPortDisplayDS16].path;
        pDisplayDS16LumaCropRoundClampConfig->CH0_CLAMP_EN   = m_IFEOutputPathInfo[IFEOutputPortDisplayDS16].path;
        pDisplayDS16ChromaCropRoundClampConfig->CROP_EN      = m_IFEOutputPathInfo[IFEOutputPortDisplayDS16].path;
        pDisplayDS16ChromaCropRoundClampConfig->CH0_ROUND_EN = m_IFEOutputPathInfo[IFEOutputPortDisplayDS16].path;
        pDisplayDS16ChromaCropRoundClampConfig->CH0_CLAMP_EN = m_IFEOutputPathInfo[IFEOutputPortDisplayDS16].path;

        pStatsEnable->HDR_BE_EN     = m_IFEOutputPathInfo[IFEOutputPortStatsHDRBE].path;
        pStatsEnable->HDR_BHIST_EN  = m_IFEOutputPathInfo[IFEOutputPortStatsHDRBHIST].path;
        pStatsEnable->BAF_EN        = m_IFEOutputPathInfo[IFEOutputPortStatsBF].path;
        pStatsEnable->AWB_BG_EN     = m_IFEOutputPathInfo[IFEOutputPortStatsAWBBG].path;
        pStatsEnable->SKIN_BHIST_EN = m_IFEOutputPathInfo[IFEOutputPortStatsBHIST].path;
        pStatsEnable->RS_EN         = m_IFEOutputPathInfo[IFEOutputPortStatsRS].path;
        pStatsEnable->CS_EN         = m_IFEOutputPathInfo[IFEOutputPortStatsCS].path;
        pStatsEnable->IHIST_EN      = m_IFEOutputPathInfo[IFEOutputPortStatsIHIST].path;
        pStatsEnable->AEC_BG_EN     = m_IFEOutputPathInfo[IFEOutputPortStatsTLBG].path;

        /// @todo (CAMX-2396) Remove dual IFE bring-up code
        if (IFEModuleMode::DualIFENormal == m_mode)
        {
            // Only change what's diff from single IFE case
            // pLensProcessingModuleConfig->PEDESTAL_EN        = 0;
            // pLensProcessingModuleConfig->CHROMA_UPSAMPLE_EN = 0;
            // pLensProcessingModuleConfig->HDR_MAC_EN         = 0;
            // pLensProcessingModuleConfig->PDAF_EN            = 0;
            // pColorProcessingModuleConfig->RGB_LUT_EN        = 0;
            // pLensProcessingModuleConfig->BLACK_EN           = 1;
            // pLensProcessingModuleConfig->DEMUX_EN           = 1;
            // pLensProcessingModuleConfig->BPC_EN             = 1;
            // pLensProcessingModuleConfig->ABF_EN             = 1;
            // pLensProcessingModuleConfig->DEMO_EN            = 1;
            // pLensProcessingModuleConfig->BLACK_LEVEL_EN     = 1;
            // pColorProcessingModuleConfig->COLOR_CORRECT_EN  = 1;
            // pColorProcessingModuleConfig->GTM_EN            = 1;
            // pFrameProcessingModuleConfig->CST_EN            = 1;
            // pLensProcessingModuleConfig->ROLLOFF_EN         = 1;

            // Only overwrite what's diff from single IFE
            // pStatsEnable->HDR_BE_EN     = 1;
            // pStatsEnable->HDR_BHIST_EN  = 1;
            // pStatsEnable->BAF_EN        = 1;
            // pStatsEnable->AWB_BG_EN     = 1;
            // pStatsEnable->SKIN_BHIST_EN = 0;
            // pStatsEnable->IHIST_EN      = 0;
            // pStatsEnable->AEC_BG_EN     = 0;
        }

        pStatsConfig->AWB_BG_QUAD_SYNC_EN   = 1;

        if (m_IFEOutputPathInfo[IFEOutputPortCAMIFRaw].path)
        {
            raw_dump = 0;
        }
        else if (m_IFEOutputPathInfo[IFEOutputPortLSCRaw].path)
        {
            raw_dump = 1;
        }
        else if (m_IFEOutputPathInfo[IFEOutputPortGTMRaw].path)
        {
            raw_dump = 2;
        }

        if (NULL != m_pIFEHVXModule)
        {
            pModuleEnable->dspConfig.bitfields.DSP_TO_SEL = (static_cast<IFEHVX*>(m_pIFEHVXModule))->GetHVXTapPoint();
        }

        CAMX_LOG_VERBOSE(CamxLogGroupISP, "Lens processing module enable config                          [0x%x]",
                         pModuleEnable->lensProcessingModuleConfig.u32All);
        CAMX_LOG_VERBOSE(CamxLogGroupISP, "Frame processing module enable config                         [0x%x]",
                         pModuleEnable->frameProcessingModuleConfig.u32All);
        CAMX_LOG_VERBOSE(CamxLogGroupISP, "Color processing module enable config                         [0x%x]",
                         pModuleEnable->colorProcessingModuleConfig.u32All);
        CAMX_LOG_VERBOSE(CamxLogGroupISP, "FD path Round, Clamp and Crop module enable config            [0x%x]",
                         pModuleEnable->FDLumaCropRoundClampConfig.u32All);
        CAMX_LOG_VERBOSE(CamxLogGroupISP, "FD Chroma path Round, Clamp and Crop module enable config     [0x%x]",
                         pModuleEnable->FDChromaCropRoundClampConfig.u32All);
        CAMX_LOG_VERBOSE(CamxLogGroupISP, "Full Luma path Round, Clamp and Crop module enable config     [0x%x]",
                         pModuleEnable->fullLumaCropRoundClampConfig.u32All);
        CAMX_LOG_VERBOSE(CamxLogGroupISP, "Full Chroma path Round, Clamp and Crop module enable config   [0x%x]",
                         pModuleEnable->fullChromaCropRoundClampConfig.u32All);
        CAMX_LOG_VERBOSE(CamxLogGroupISP, "DS4 Luma path Round, Clamp and Crop module enable config      [0x%x]",
                         pModuleEnable->DS4LumaCropRoundClampConfig.u32All);
        CAMX_LOG_VERBOSE(CamxLogGroupISP, "DS4 Chroma path Round, Clamp and Crop module enable config    [0x%x]",
                         pModuleEnable->DS4ChromaCropRoundClampConfig.u32All);
        CAMX_LOG_VERBOSE(CamxLogGroupISP, "DS16 Luma path Round, Clamp and Crop module enable config     [0x%x]",
                         pModuleEnable->DS16LumaCropRoundClampConfig.u32All);
        CAMX_LOG_VERBOSE(CamxLogGroupISP, "DS16 Chroma path Round, Clamp and Crop module enable config   [0x%x]",
                         pModuleEnable->DS16LumaCropRoundClampConfig.u32All);
        CAMX_LOG_VERBOSE(CamxLogGroupISP, "Stats module enable config                                    [0x%x]",
                         pModuleEnable->statsEnable.u32All);
        CAMX_LOG_VERBOSE(CamxLogGroupISP, "Stats general configuration register                          [0x%x]",
                         pModuleEnable->statsConfig.u32All);
        CAMX_LOG_VERBOSE(CamxLogGroupISP, "raw_dump                                                      [0x%x]",
                         raw_dump);
        CAMX_LOG_VERBOSE(CamxLogGroupISP, "Frame Display processing module enable config                 [0x%x]",
                         pModuleEnable->frameProcessingDisplayModuleConfig.u32All);
        CAMX_LOG_VERBOSE(CamxLogGroupISP, "Disp Luma path Round, Clamp and Crop module enable config     [0x%x]",
                         pModuleEnable->displayFullLumaCropRoundClampConfig.u32All);
        CAMX_LOG_VERBOSE(CamxLogGroupISP, "Disp Chroma path Round, Clamp and Crop module enable config   [0x%x]",
                         pModuleEnable->displayFullChromaCropRoundClampConfig.u32All);
        CAMX_LOG_VERBOSE(CamxLogGroupISP, "DS4 Disp Luma path Round, Clamp and Crop module enable cfg    [0x%x]",
                         pModuleEnable->displayDS4LumaCropRoundClampConfig.u32All);
        CAMX_LOG_VERBOSE(CamxLogGroupISP, "DS4 Disp Chroma path Round, Clamp and Crop module enable cfg  [0x%x]",
                         pModuleEnable->displayDS4ChromaCropRoundClampConfig.u32All);
        CAMX_LOG_VERBOSE(CamxLogGroupISP, "DS16 Disp Luma path Round, Clamp and Crop module enable cfg   [0x%x]",
                         pModuleEnable->displayDS16LumaCropRoundClampConfig.u32All);
        CAMX_LOG_VERBOSE(CamxLogGroupISP, "DS16 Disp Chroma path Round, Clamp and Crop module enable cfg [0x%x]",
                         pModuleEnable->displayDS16LumaCropRoundClampConfig.u32All);

        result = PacketBuilder::WriteRegRange(
                     pCmdBuffer,
                     regIFE_IFE_0_VFE_MODULE_LENS_EN,
                     (sizeof(pModuleEnable->lensProcessingModuleConfig) / RegisterWidthInBytes),
                     reinterpret_cast<UINT32*>(&pModuleEnable->lensProcessingModuleConfig));

        result = PacketBuilder::WriteRegRange(
                     pCmdBuffer,
                     regIFE_IFE_0_VFE_MODULE_COLOR_EN,
                     (sizeof(pModuleEnable->colorProcessingModuleConfig) / RegisterWidthInBytes),
                     reinterpret_cast<UINT32*>(&pModuleEnable->colorProcessingModuleConfig));

        result |= PacketBuilder::WriteRegRange(
                     pCmdBuffer,
                     regIFE_IFE_0_VFE_MODULE_ZOOM_EN,
                     (sizeof(pModuleEnable->frameProcessingModuleConfig) / RegisterWidthInBytes),
                     reinterpret_cast<UINT32*>(&pModuleEnable->frameProcessingModuleConfig));

        result |= PacketBuilder::WriteRegRange(
                     pCmdBuffer,
                     regIFE_IFE_0_VFE_FD_OUT_Y_CROP_RND_CLAMP_CFG,
                     (sizeof(pModuleEnable->FDLumaCropRoundClampConfig) / RegisterWidthInBytes),
                     reinterpret_cast<UINT32*>(&pModuleEnable->FDLumaCropRoundClampConfig));

        result |= PacketBuilder::WriteRegRange(
                     pCmdBuffer,
                     regIFE_IFE_0_VFE_FD_OUT_C_CROP_RND_CLAMP_CFG,
                     (sizeof(pModuleEnable->FDChromaCropRoundClampConfig) / RegisterWidthInBytes),
                     reinterpret_cast<UINT32*>(&pModuleEnable->FDChromaCropRoundClampConfig));

        result |= PacketBuilder::WriteRegRange(
                     pCmdBuffer,
                     regIFE_IFE_0_VFE_FULL_OUT_Y_CROP_RND_CLAMP_CFG,
                     (sizeof(pModuleEnable->fullLumaCropRoundClampConfig) / RegisterWidthInBytes),
                     reinterpret_cast<UINT32*>(&pModuleEnable->fullLumaCropRoundClampConfig));

        result |= PacketBuilder::WriteRegRange(
                     pCmdBuffer,
                     regIFE_IFE_0_VFE_FULL_OUT_C_CROP_RND_CLAMP_CFG,
                     (sizeof(pModuleEnable->fullChromaCropRoundClampConfig) / RegisterWidthInBytes),
                     reinterpret_cast<UINT32*>(&pModuleEnable->fullChromaCropRoundClampConfig));

        result |= PacketBuilder::WriteRegRange(
                     pCmdBuffer,
                     regIFE_IFE_0_VFE_DS4_Y_CROP_RND_CLAMP_CFG,
                     (sizeof(pModuleEnable->DS4LumaCropRoundClampConfig) / RegisterWidthInBytes),
                     reinterpret_cast<UINT32*>(&pModuleEnable->DS4LumaCropRoundClampConfig));

        result |= PacketBuilder::WriteRegRange(
                     pCmdBuffer,
                     regIFE_IFE_0_VFE_DS4_C_CROP_RND_CLAMP_CFG,
                     (sizeof(pModuleEnable->DS4ChromaCropRoundClampConfig) / RegisterWidthInBytes),
                     reinterpret_cast<UINT32*>(&pModuleEnable->DS4ChromaCropRoundClampConfig));

        result |= PacketBuilder::WriteRegRange(
                     pCmdBuffer,
                     regIFE_IFE_0_VFE_DS16_Y_CROP_RND_CLAMP_CFG,
                     (sizeof(pModuleEnable->DS16LumaCropRoundClampConfig) / RegisterWidthInBytes),
                     reinterpret_cast<UINT32*>(&pModuleEnable->DS16LumaCropRoundClampConfig));

        result |= PacketBuilder::WriteRegRange(
                     pCmdBuffer,
                     regIFE_IFE_0_VFE_DS16_C_CROP_RND_CLAMP_CFG,
                     (sizeof(pModuleEnable->DS16ChromaCropRoundClampConfig) / RegisterWidthInBytes),
                     reinterpret_cast<UINT32*>(&pModuleEnable->DS16ChromaCropRoundClampConfig));

        result |= PacketBuilder::WriteRegRange(
                     pCmdBuffer,
                     regIFE_IFE_0_VFE_MODULE_STATS_EN,
                     (sizeof(pModuleEnable->statsEnable) / RegisterWidthInBytes),
                     reinterpret_cast<UINT32*>(&pModuleEnable->statsEnable));

        result |= PacketBuilder::WriteRegRange(
                     pCmdBuffer,
                     regIFE_IFE_0_VFE_STATS_CFG,
                     (sizeof(pModuleEnable->statsConfig) / RegisterWidthInBytes),
                     reinterpret_cast<UINT32*>(&pModuleEnable->statsConfig));

        result |= PacketBuilder::WriteRegRange(
                     pCmdBuffer,
                     regIFE_IFE_0_VFE_MODULE_DISP_EN,
                     (sizeof(pModuleEnable->frameProcessingDisplayModuleConfig) / RegisterWidthInBytes),
                     reinterpret_cast<UINT32*>(&pModuleEnable->frameProcessingDisplayModuleConfig));

        result |= PacketBuilder::WriteRegRange(
                     pCmdBuffer,
                     regIFE_IFE_0_VFE_DISP_OUT_Y_CROP_RND_CLAMP_CFG,
                     (sizeof(pModuleEnable->displayFullLumaCropRoundClampConfig) / RegisterWidthInBytes),
                     reinterpret_cast<UINT32*>(&pModuleEnable->displayFullLumaCropRoundClampConfig));

        result |= PacketBuilder::WriteRegRange(
                     pCmdBuffer,
                     regIFE_IFE_0_VFE_DISP_OUT_C_CROP_RND_CLAMP_CFG,
                     (sizeof(pModuleEnable->displayFullChromaCropRoundClampConfig) / RegisterWidthInBytes),
                     reinterpret_cast<UINT32*>(&pModuleEnable->displayFullChromaCropRoundClampConfig));

        result |= PacketBuilder::WriteRegRange(
                     pCmdBuffer,
                     regIFE_IFE_0_VFE_DISP_DS4_Y_CROP_RND_CLAMP_CFG,
                     (sizeof(pModuleEnable->displayDS4LumaCropRoundClampConfig) / RegisterWidthInBytes),
                     reinterpret_cast<UINT32*>(&pModuleEnable->displayDS4LumaCropRoundClampConfig));

        result |= PacketBuilder::WriteRegRange(
                     pCmdBuffer,
                     regIFE_IFE_0_VFE_DISP_DS4_C_CROP_RND_CLAMP_CFG,
                     (sizeof(pModuleEnable->displayDS4ChromaCropRoundClampConfig) / RegisterWidthInBytes),
                     reinterpret_cast<UINT32*>(&pModuleEnable->displayDS4ChromaCropRoundClampConfig));

        result |= PacketBuilder::WriteRegRange(
                     pCmdBuffer,
                     regIFE_IFE_0_VFE_DISP_DS16_Y_CROP_RND_CLAMP_CFG,
                     (sizeof(pModuleEnable->displayDS16LumaCropRoundClampConfig) / RegisterWidthInBytes),
                     reinterpret_cast<UINT32*>(&pModuleEnable->displayDS16LumaCropRoundClampConfig));

        result |= PacketBuilder::WriteRegRange(
                     pCmdBuffer,
                     regIFE_IFE_0_VFE_DISP_DS16_C_CROP_RND_CLAMP_CFG,
                     (sizeof(pModuleEnable->displayDS16ChromaCropRoundClampConfig) / RegisterWidthInBytes),
                     reinterpret_cast<UINT32*>(&pModuleEnable->displayDS16ChromaCropRoundClampConfig));


        if ((m_IFEOutputPathInfo[IFEOutputPortCAMIFRaw].path) ||
            (m_IFEOutputPathInfo[IFEOutputPortLSCRaw].path)   ||
            (m_IFEOutputPathInfo[IFEOutputPortGTMRaw].path))
        {
            result |= PacketBuilder::WriteRegRange(
                         pCmdBuffer,
                         regIFE_IFE_0_VFE_PIXEL_RAW_DUMP_CFG,
                         (sizeof(raw_dump) / RegisterWidthInBytes),
                         reinterpret_cast<UINT32*>(&raw_dump));
        }
        result |= PacketBuilder::WriteRegRange(
                    pCmdBuffer,
                    regIFE_IFE_0_VFE_DSP_TO_SEL,
                    (sizeof(pModuleEnable->dspConfig) / RegisterWidthInBytes),
                    reinterpret_cast<UINT32*>(&pModuleEnable->dspConfig));

        if (CamxResultSuccess != result)
        {
            CAMX_ASSERT_ALWAYS_MESSAGE("Module enable register write failed");
        }
    }
    else
    {
        CAMX_ASSERT_ALWAYS_MESSAGE("Invalid input parameter");
        result = CamxResultEInvalidArg;
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IFENode::PrepareStripingParameters()
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFENode::PrepareStripingParameters(
    ISPInputData* pInputData)
{
    CamxResult          result           = CamxResultSuccess;
    UINT                count            = 0;

    CamX::Utils::Memset(&m_ISPFramelevelData, 0, sizeof(m_ISPFramelevelData));

    for (count = 0; count < m_numIFEIQModule; count++)
    {
        result = m_pIFEIQModule[count]->PrepareStripingParameters(pInputData);
        if (result != CamxResultSuccess)
        {
            break;
        }
    }
    if (CamxResultSuccess == result)
    {
        for (count = 0; count < m_numIFEStatsModule; count++)
        {
            result = m_pIFEStatsModule[count]->PrepareStripingParameters(pInputData);
            if (result != CamxResultSuccess)
            {
                break;
            }
        }
    }
    return result;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::ProgramIQConfig()
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFENode::ProgramIQConfig(
    ISPInputData* pInputData)
{
    CamxResult      result          = CamxResultSuccess;
    UINT            count           = 0;
    BOOL            adrcEnabled     = FALSE;
    FLOAT           percentageOfGTM = 0.0f;

    CamX::Utils::Memset(m_ISPData, 0, sizeof(m_ISPData));

    if (IFEModuleMode::DualIFENormal != m_mode)
    {
        CamX::Utils::Memset(&m_ISPFramelevelData, 0, sizeof(m_ISPFramelevelData));
    }

    // Based on HW team recommendation always keep CGC ON
    if (TRUE == HwEnvironment::GetInstance()->IsHWBugWorkaroundEnabled(Titan17xWorkarounds::Titan17xWorkaroundsCDMDMICGCBug))
    {
        PacketBuilder::WriteInterleavedRegs(m_pCommonCmdBuffer, IFECGCNumRegs, CGCOnCmds);
    }

    // Update sensor bit width from sensor capability structure
    pInputData->sensorBitWidth = m_pSensorCaps->sensorConfigs[0].streamConfigs[0].bitWidth;

    // Get the Default ADRC status.
    IQInterface::GetADRCParams(pInputData, &adrcEnabled, &percentageOfGTM, GetIFESWTMCModuleInstanceVersion());

    if ((AdaptiveGTM == GetStaticSettings()->FDPreprocessing) || (GTM == GetStaticSettings()->FDPreprocessing))
    {
        // Get GTM percentage, DRC gain, DRC dark gain for FD preprocessing
        PropertyISPADRCParams adrcParams          = { 0 };
        UINT                  PropertyTag[]       = { PropertyIDIFEADRCParams };
        const UINT            propNum             = CAMX_ARRAY_SIZE(PropertyTag);
        const VOID*           pData[propNum]      = { &adrcParams };
        UINT                  pDataCount[propNum] = { sizeof(PropertyISPADRCParams) };

        adrcParams.bIsADRCEnabled = adrcEnabled;
        adrcParams.GTMPercentage  = percentageOfGTM;
        adrcParams.DRCGain        = pInputData->triggerData.DRCGain;
        adrcParams.DRCDarkGain    = pInputData->triggerData.DRCGainDark;

        WriteDataList(PropertyTag, pData, pDataCount, propNum);
    }

    if (m_PDAFInfo.enableSubsample)
    {
        m_stripeConfigs[0].CAMIFSubsampleInfo.enableCAMIFSubsample                   = m_PDAFInfo.enableSubsample;
        m_stripeConfigs[0].CAMIFSubsampleInfo.CAMIFSubSamplePattern.pixelSkipPattern = m_PDAFInfo.pixelSkipPattern;
        m_stripeConfigs[0].CAMIFSubsampleInfo.CAMIFSubSamplePattern.lineSkipPattern  = m_PDAFInfo.lineSkipPattern;
        m_stripeConfigs[1].CAMIFSubsampleInfo.enableCAMIFSubsample                   = m_PDAFInfo.enableSubsample;
        m_stripeConfigs[1].CAMIFSubsampleInfo.CAMIFSubSamplePattern.pixelSkipPattern = m_PDAFInfo.pixelSkipPattern;
        m_stripeConfigs[1].CAMIFSubsampleInfo.CAMIFSubSamplePattern.lineSkipPattern  = m_PDAFInfo.lineSkipPattern;
    }

    for (count = 0; count < m_numIFEIQModule; count++)
    {
        IQModuleDualIFEData dualIFEImpact       = { 0 };
        BOOL                dualIFESensitive    = FALSE;

        if (TRUE == adrcEnabled)
        {
            // Update AEC Gain values for ADRC use cases, before GTM(includes) will be triggered by shortGain,
            // betweem GTM & LTM(includes) will be by shortGain*power(DRCGain, gtm_perc) and post LTM will be
            // by shortGain*DRCGain
            IQInterface::UpdateAECGain(m_pIFEIQModule[count]->GetIQType(), pInputData, percentageOfGTM);
        }

        if (IFEModuleMode::DualIFENormal == m_mode)
        {
            m_pIFEIQModule[count]->GetDualIFEData(&dualIFEImpact);
            dualIFESensitive = dualIFEImpact.dualIFESensitive;

            CAMX_LOG_VERBOSE(CamxLogGroupISP, "m_type %d dualIFESensitive %d ",
                m_pIFEIQModule[count]->GetIQType(), dualIFESensitive);
            // If the module is dual-mode-sensitive, generate its command on left/right command buffer.
            if (FALSE == dualIFESensitive)
            {
                // An IQ module that is not sensitive to dual mode should not depend on anything left/right-specific.
                // It can only generate frame-level data that's common between left and right. Data that is needed
                // by left/right but generated by a common IQ is shared using a pointer to m_ISPFrameData. Only a
                // common IQ module may write into this; it can be read by all IQ.
                // If an IQ module depends on data generated by a dual-sensitive module, then it cannot be common.
                // For uniformity, another element is added to m_ISPData to track common data but this may be
                // refactored as more IQ's are enabled and there is more clarity as what exactly is written by them.
                CAMX_LOG_VERBOSE(CamxLogGroupISP, "Common command buffer");
                pInputData->pCalculatedData              = &m_ISPData[2];
                pInputData->pCmdBuffer                   = m_pCommonCmdBuffer;
                pInputData->pStripeConfig                = &m_stripeConfigs[0];
                result                                   = m_pIFEIQModule[count]->Execute(pInputData);
            }
            else
            {
                CAMX_LOG_VERBOSE(CamxLogGroupISP, "Right command buffer");
                pInputData->pCmdBuffer                   = m_pRightCmdBuffer;

                pInputData->pCalculatedData              = &m_ISPData[1];
                pInputData->pStripeConfig                = &m_stripeConfigs[1];
                result                                   = m_pIFEIQModule[count]->Execute(pInputData);
                if (CamxResultSuccess == result)
                {
                    CAMX_LOG_VERBOSE(CamxLogGroupISP, "Left command buffer");
                    pInputData->pCmdBuffer                   = m_pLeftCmdBuffer;
                    pInputData->pCalculatedData              = &m_ISPData[0];
                    pInputData->pStripeConfig                = &m_stripeConfigs[0];
                    result                                   = m_pIFEIQModule[count]->Execute(pInputData);
                }
                else
                {
                    CAMX_ASSERT_ALWAYS_MESSAGE("Failed to Execute IQ Config for Right Stripe, count %d, IQType %d",
                        count, m_pIFEIQModule[count]->GetIQType());
                }
            }

            if (CamxResultSuccess != result)
            {
                CAMX_ASSERT_ALWAYS_MESSAGE("Failed to Execute IQ Config for Left Stripe, count %d, IQType %d",
                    count, m_pIFEIQModule[count]->GetIQType());
            }
        }
        else
        {
            // Program the common command buffer
            CAMX_LOG_VERBOSE(CamxLogGroupISP, "Main command buffer");
            pInputData->pCmdBuffer      = m_pCommonCmdBuffer;
            pInputData->pCalculatedData = &m_ISPData[2];
            pInputData->pStripeConfig   = &m_stripeConfigs[0];
            if (m_pIFEIQModule[count]->GetIQType() == ISPIQModuleType::IFEWB)
            {
                pInputData->pAECUpdateData->exposureInfo[0].sensitivity
                    = pInputData->pAECUpdateData->exposureInfo[0].sensitivity * pInputData->pAECUpdateData->predictiveGain;
                pInputData->pAECUpdateData->exposureInfo[0].linearGain
                    = pInputData->pAECUpdateData->exposureInfo[0].linearGain * pInputData->pAECUpdateData->predictiveGain;

                /// re applying triggerdata based on the above calculation
                Node* pBaseNode = this;
                IQInterface::IQSetupTriggerData(pInputData, pBaseNode, TRUE , NULL);
            }
            result                      = m_pIFEIQModule[count]->Execute(pInputData);
        }

        if (TRUE == adrcEnabled &&
                ISPIQModuleType::IFEGTM == m_pIFEIQModule[count]->GetIQType())
        {
            percentageOfGTM = pInputData->pCalculatedData->percentageOfGTM;
            CAMX_LOG_VERBOSE(CamxLogGroupISP, "update the percentageOfGTM: %f", percentageOfGTM);
        }

        if (CamxResultSuccess != result)
        {
            CAMX_ASSERT_ALWAYS_MESSAGE("Failed to Run IQ Config, count %d", count);
        }
    }

    for (count = 0; count < m_numIFEStatsModule; count++)
    {
        IQModuleDualIFEData dualIFEImpact       = { 0 };
        BOOL                dualIFESensitive    = FALSE;

        if (IFEModuleMode::DualIFENormal == m_mode)
        {
            m_pIFEStatsModule[count]->GetDualIFEData(&dualIFEImpact);
            dualIFESensitive = dualIFEImpact.dualIFESensitive;

            if (FALSE == dualIFESensitive)
            {
                CAMX_LOG_VERBOSE(CamxLogGroupISP, "Common command buffer");
                pInputData->pCalculatedData = &m_ISPData[2];
                pInputData->pCmdBuffer      = m_pCommonCmdBuffer;
                result                      = m_pIFEStatsModule[count]->Execute(pInputData);
            }
            else
            {
                CAMX_LOG_VERBOSE(CamxLogGroupISP, "Right command buffer");
                pInputData->pCmdBuffer      = m_pRightCmdBuffer;
                pInputData->pCalculatedData = &m_ISPData[1];
                pInputData->pStripeConfig   = &m_stripeConfigs[1];
                result                      = m_pIFEStatsModule[count]->Execute(pInputData);

                if (CamxResultSuccess == result)
                {
                    CAMX_LOG_VERBOSE(CamxLogGroupISP, "Left command buffer");
                    pInputData->pCmdBuffer      = m_pLeftCmdBuffer;
                    pInputData->pCalculatedData = &m_ISPData[0];
                    pInputData->pStripeConfig   = &m_stripeConfigs[0];
                    result                      = m_pIFEStatsModule[count]->Execute(pInputData);
                }
                else
                {
                    CAMX_ASSERT_ALWAYS_MESSAGE("Failed to Run Stats Config for Right Stripe, count %d", count);
                }
            }

            if (CamxResultSuccess != result)
            {
                CAMX_ASSERT_ALWAYS_MESSAGE("Failed to Run Stats Config for Left Stripe, count %d", count);
            }
        }
        else
        {
            // Program the left IFE
            CAMX_LOG_VERBOSE(CamxLogGroupISP, "Main command buffer");
            pInputData->pCmdBuffer      = m_pCommonCmdBuffer;
            pInputData->pCalculatedData = &m_ISPData[2];
            pInputData->pStripeConfig   = &m_stripeConfigs[0];
            result                      = m_pIFEStatsModule[count]->Execute(pInputData);
        }

        if (CamxResultSuccess != result)
        {
            CAMX_ASSERT_ALWAYS_MESSAGE("Failed to Run Stats Config, count %d", count);
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::GetStaticMetadata
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IFENode::GetStaticMetadata()
{
    HwCameraInfo    cameraInfo;

    HwEnvironment::GetInstance()->GetCameraInfo(GetPipeline()->GetCameraId(), &cameraInfo);

    // Initialize default metadata
    m_HALTagsData.blackLevelLock                    = BlackLevelLockOff;
    m_HALTagsData.colorCorrectionMode               = ColorCorrectionModeFast;
    m_HALTagsData.controlAEMode                     = ControlAEModeOn;
    m_HALTagsData.controlAWBMode                    = ControlAWBModeAuto;
    m_HALTagsData.controlMode                       = ControlModeAuto;
    m_HALTagsData.controlPostRawSensitivityBoost    = cameraInfo.pPlatformCaps->minPostRawSensitivityBoost;
    m_HALTagsData.noiseReductionMode                = NoiseReductionModeFast;
    m_HALTagsData.shadingMode                       = ShadingModeFast;
    m_HALTagsData.statisticsHotPixelMapMode         = StatisticsHotPixelMapModeOff;
    m_HALTagsData.statisticsLensShadingMapMode      = StatisticsLensShadingMapModeOff;
    m_HALTagsData.tonemapCurves.tonemapMode         = TonemapModeFast;

    // Cache sensor capability information for this camera
    m_pSensorCaps = cameraInfo.pSensorCaps;

    m_sensorActiveArrayWidth  = m_pSensorCaps->activeArraySize.width;
    m_sensorActiveArrayHeight = m_pSensorCaps->activeArraySize.height;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::GetMetadataTags
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFENode::GetMetadataTags(
    ISPInputData* pModuleInput)
{
    VOID*               pTagsData[NumIFEMetadataTags]   = {0};
    CropWindow*         pCrop                           = NULL;
    CropWindow          halCrop                         = {0};
    INT32               sensorWidth                     = m_pSensorModeData->cropInfo.lastPixel -
                                                          m_pSensorModeData->cropInfo.firstPixel + 1;
    INT32               sensorHeight                    = m_pSensorModeData->cropInfo.lastLine -
                                                          m_pSensorModeData->cropInfo.firstLine + 1;
    UINT                dataIndex                       = 0;
    FLOAT               widthRatio ;
    FLOAT               heightRatio;

    // CSID crop override
    if (TRUE == EnableCSIDCropOverridingForSingleIFE())
    {
        sensorWidth  = m_instanceProperty.IFECSIDWidth;
        sensorHeight = m_instanceProperty.IFECSIDHeight;
    }

    pModuleInput->pHALTagsData->noiseReductionMode = NoiseReductionModeFast;

    // sensor width shift for YUV sensor
    if (TRUE == m_ISPInputSensorData.isYUV)
    {
        sensorWidth >>= 1;
    }

    GetDataList(IFEMetadataTags, pTagsData, IFEMetadataTagReqOffset, NumIFEMetadataTags);

    if (NULL != pTagsData[dataIndex])
    {
        pModuleInput->pHALTagsData->blackLevelLock = *(static_cast<UINT8*>(pTagsData[dataIndex++]));
    }
    if (NULL != pTagsData[dataIndex])
    {
        pModuleInput->pHALTagsData->colorCorrectionGains =
            *(static_cast<ColorCorrectionGain*>(pTagsData[dataIndex++]));
    }
    if (NULL != pTagsData[dataIndex])
    {
        pModuleInput->pHALTagsData->colorCorrectionMode = *(static_cast<UINT8*>(pTagsData[dataIndex++]));
    }
    if (NULL != pTagsData[dataIndex])
    {
        pModuleInput->pHALTagsData->colorCorrectionTransform =
            *(static_cast<ISPColorCorrectionTransform*>(pTagsData[dataIndex++]));
    }
    if (NULL != pTagsData[dataIndex])
    {
        pModuleInput->pHALTagsData->controlAEMode = *(static_cast<UINT8*>(pTagsData[dataIndex++]));
    }
    if (NULL != pTagsData[dataIndex])
    {
        pModuleInput->pHALTagsData->controlAWBMode = *(static_cast<UINT8*>(pTagsData[dataIndex++]));
    }
    if (NULL != pTagsData[dataIndex])
    {
        pModuleInput->pHALTagsData->controlMode = *(static_cast<UINT8*>(pTagsData[dataIndex++]));
    }
    if (NULL != pTagsData[dataIndex])
    {
        pModuleInput->pHALTagsData->controlPostRawSensitivityBoost = *(static_cast<INT32*>(pTagsData[dataIndex++]));
    }
    if (NULL != pTagsData[dataIndex])
    {
        pModuleInput->pHALTagsData->hotPixelMode = *(static_cast<HotPixelModeValues*>(pTagsData[dataIndex++]));
    }
    if (NULL != pTagsData[dataIndex])
    {
        pModuleInput->pHALTagsData->noiseReductionMode = *(static_cast<UINT8*>(pTagsData[dataIndex++]));
    }
    if (NULL != pTagsData[dataIndex])
    {
        pModuleInput->pHALTagsData->shadingMode = *(static_cast<UINT8*>(pTagsData[dataIndex++]));
    }
    if (NULL != pTagsData[dataIndex])
    {
        pModuleInput->pHALTagsData->statisticsHotPixelMapMode = *(static_cast<UINT8*>(pTagsData[dataIndex++]));
    }
    if (NULL != pTagsData[dataIndex])
    {
        pModuleInput->pHALTagsData->statisticsLensShadingMapMode = *(static_cast<UINT8*>(pTagsData[dataIndex++]));
    }
    if (NULL != pTagsData[dataIndex])
    {
        pModuleInput->pHALTagsData->tonemapCurves.tonemapMode = *(static_cast<UINT8*>(pTagsData[dataIndex++]));
    }
    if (NULL != pTagsData[dataIndex])
    {
        pModuleInput->pHALTagsData->HALCrop = *(static_cast<CropWindow*>(pTagsData[dataIndex++]));
    }
    if (NULL != pTagsData[dataIndex])
    {
        pModuleInput->pHALTagsData->controlAWBLock = *(static_cast<UINT8*>(pTagsData[dataIndex++]));
    }
    if (NULL != pTagsData[dataIndex])
    {
        pModuleInput->pHALTagsData->controlAECLock = *(static_cast<UINT8*>(pTagsData[dataIndex++]));
    }

    // Map and update application crop window from sensor active pixel array domain to sensor output dimension domain
    halCrop = pModuleInput->pHALTagsData->HALCrop;
    pCrop = &(pModuleInput->pHALTagsData->HALCrop);

    CAMX_ASSERT((0 != m_sensorActiveArrayWidth) && (0 != m_sensorActiveArrayHeight));
    widthRatio  = static_cast<FLOAT>(sensorWidth) / static_cast<FLOAT>(m_sensorActiveArrayWidth);
    heightRatio = static_cast<FLOAT>(sensorHeight) / static_cast<FLOAT>(m_sensorActiveArrayHeight);

    pCrop->left   = Utils::RoundFLOAT(pCrop->left   * widthRatio);
    pCrop->top    = Utils::RoundFLOAT(pCrop->top    * heightRatio);
    pCrop->width  = Utils::RoundFLOAT(pCrop->width  * widthRatio);
    pCrop->height = Utils::RoundFLOAT(pCrop->height * heightRatio);
    CAMX_LOG_VERBOSE(CamxLogGroupISP, "IFE:%d <crop> hal[%d,%d,%d,%d] Sensor:Act[%d,%d] Cur[%d,%d] Crop[%d,%d,%d,%d] req %llu",
        InstanceID(),
        halCrop.left, halCrop.top, halCrop.width, halCrop.height,
        m_sensorActiveArrayWidth, m_sensorActiveArrayHeight,
        sensorWidth, sensorHeight,
        pCrop->left, pCrop->top, pCrop->width, pCrop->height,
        pModuleInput->frameNum);

    GetMetadataContrastLevel(pModuleInput->pHALTagsData);
    GetMetadataTonemapCurve(pModuleInput->pHALTagsData);

    return CamxResultSuccess;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::ProgramStripeConfig
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFENode::ProgramStripeConfig()
{
    /// @todo (CAMX-1308)   The stripe info should come from the striping library. All the NOT_IMPLEMENTED sections below also
    /// fall in that category (other ports/formats).

    UINT32              outputPortId[MaxIFEOutputPorts];
    UINT32              totalOutputPorts    = 0;
    CamxResult          result              = CamxResultSuccess;
    IFEDualConfig*      pDualConfig         = NULL;
    IFEStripeConfig*    pStripes            = NULL;
    UINT32              leftIndex           = 0;
    UINT32              rightIndex          = 0;
    UINT32              numPorts            = 0;

    const UINT32 maxStripeConfigSize = DualIFEUtils::GetMaxStripeConfigSize(m_maxNumOfCSLIFEPortId);

    pDualConfig = reinterpret_cast<IFEDualConfig*>(
                    m_pDualIFEConfigCmdBuffer->BeginCommands(maxStripeConfigSize / sizeof(UINT32)));

    if (NULL != pDualConfig)
    {
        CamX::Utils::Memset(pDualConfig, 0, maxStripeConfigSize);

        // Ideally, we should only send whatever many port we program but for simplicity, just use max HW ports.
        pDualConfig->numPorts   = m_maxNumOfCSLIFEPortId;
        numPorts                = m_maxNumOfCSLIFEPortId;
        pStripes                = &pDualConfig->stripes[0];

        GetAllOutputPortIds(&totalOutputPorts, outputPortId);

        for (UINT outputPortIndex = 0; outputPortIndex < totalOutputPorts; outputPortIndex++)
        {
            const ImageFormat*  pImageFormat = GetOutputPortImageFormat(OutputPortIndex(outputPortId[outputPortIndex]));
            UINT32              portId;
            UINT32              statsPortId;
            UINT32              portIdx;
            UINT32              CSLPortID;

            CAMX_ASSERT_MESSAGE((NULL != pImageFormat), "Output port is not defined.");

            // We need to convert from pixel to bytes here.
            switch (outputPortId[outputPortIndex])
            {
                case IFEOutputPortGTMRaw:
                case IFEOutputPortCAMIFRaw:
                case IFEOutputPortLSCRaw:
                    portId = CSLIFEPortIndex(CSLIFEPortIdRawDump);
                    if (CamX::Format::RawPlain16 == pImageFormat->format)
                    {
                        // First stripe
                        leftIndex = GetStripeConfigIndex(DualIFEStripeIdLeft, numPorts, portId, 0);
                        pStripes[leftIndex].offset = m_stripeConfigs[0].stream[PixelRawOutput].offset;
                        pStripes[leftIndex].width  = m_stripeConfigs[0].stream[PixelRawOutput].width * 2;
                        pStripes[leftIndex].portId = CSLIFEPortIdRawDump;
                        CAMX_LOG_VERBOSE(CamxLogGroupISP,
                                         "Full pixelraw port: left stripe offset: %d, width: %d",
                                         pStripes[leftIndex].offset,
                                         pStripes[leftIndex].width);
                        // Second stripe
                        rightIndex = GetStripeConfigIndex(DualIFEStripeIdRight, numPorts, portId, 0);
                        pStripes[rightIndex].offset = m_stripeConfigs[1].stream[PixelRawOutput].offset * 2;
                        pStripes[rightIndex].width  = m_stripeConfigs[1].stream[PixelRawOutput].width * 2;
                        pStripes[rightIndex].portId = CSLIFEPortIdFull;

                        CAMX_LOG_VERBOSE(CamxLogGroupISP,
                                         "Full pixelraw port: right stripe offset: %d, width: %d",
                                         pStripes[rightIndex].offset,
                                         pStripes[rightIndex].width);
                    }
                    break;
                case IFEOutputPortStatsDualPD:
                case IFEOutputPortRDI0:
                case IFEOutputPortRDI1:
                case IFEOutputPortRDI2:
                    CAMX_LOG_VERBOSE(CamxLogGroupISP, "RDI port %d is not striped; skipping",  outputPortId[outputPortIndex]);
                    break;
                case IFEOutputPortFull:
                case IFEOutputPortDisplayFull:
                    portId    = CSLIFEPortIndex(CSLIFEPortIdFull);
                    CSLPortID = CSLIFEPortIdFull;
                    portIdx   = FullOutput;
                    if (IFEOutputPortDisplayFull == outputPortId[outputPortIndex])
                    {
                        portId    = CSLIFEPortIndex(CSLIFEPortIdDisplayFull);
                        CSLPortID = CSLIFEPortIdDisplayFull;
                        portIdx   = DisplayFullOutput;
                    }
                    if ((CamX::Format::YUV420NV12 == pImageFormat->format) ||
                        (CamX::Format::YUV420NV21 == pImageFormat->format))
                    {
                        // First stripe
                        // For NV12, pixel width == byte width (for both Y and UV plane)
                        // Y plane
                        leftIndex = GetStripeConfigIndex(DualIFEStripeIdLeft, numPorts, portId, 0);
                        pStripes[leftIndex].offset = m_stripeConfigs[0].stream[portIdx].offset;
                        pStripes[leftIndex].width  = m_stripeConfigs[0].stream[portIdx].width;
                        pStripes[leftIndex].portId = CSLPortID;

                        CAMX_LOG_VERBOSE(CamxLogGroupISP,
                                         "Idx:%u Full Y port: left stripe offset: %d, width: %d",
                                         portIdx, pStripes[leftIndex].offset, pStripes[leftIndex].width);

                        // UV plane  (same split point and width)
                        leftIndex = GetStripeConfigIndex(DualIFEStripeIdLeft, numPorts, portId, 1);
                        pStripes[leftIndex].offset = m_stripeConfigs[0].stream[portIdx].offset;
                        pStripes[leftIndex].width  = m_stripeConfigs[0].stream[portIdx].width;
                        pStripes[leftIndex].portId = CSLPortID;

                        CAMX_LOG_VERBOSE(CamxLogGroupISP,
                                         "Idx:%u Full UV port: left stripe offset: %d, width: %d",
                                         portIdx, pStripes[leftIndex].offset, pStripes[leftIndex].width);

                        // Second stripe
                        // Y plane
                        rightIndex = GetStripeConfigIndex(DualIFEStripeIdRight, numPorts, portId, 0);
                        pStripes[rightIndex].offset = m_stripeConfigs[1].stream[portIdx].offset;
                        pStripes[rightIndex].width  = m_stripeConfigs[1].stream[portIdx].width;
                        pStripes[rightIndex].portId = CSLPortID;

                        CAMX_LOG_VERBOSE(CamxLogGroupISP,
                                         "Idx:%u Full Y port: right stripe offset: %d, width: %d",
                                         portIdx, pStripes[rightIndex].offset, pStripes[rightIndex].width);

                        // UV plane  (same split point and width)
                        rightIndex = GetStripeConfigIndex(DualIFEStripeIdRight, numPorts, portId, 1);
                        pStripes[rightIndex].offset = m_stripeConfigs[1].stream[portIdx].offset;
                        pStripes[rightIndex].width  = m_stripeConfigs[1].stream[portIdx].width;
                        pStripes[rightIndex].portId = CSLPortID;

                        CAMX_LOG_VERBOSE(CamxLogGroupISP,
                                         "Idx:%u Full UV port: right stripe offset: %d, width: %d",
                                         portIdx, pStripes[rightIndex].offset, pStripes[rightIndex].width);
                    }
                    else if (TRUE == ImageFormatUtils::IsUBWC(pImageFormat->format))
                    {
                        /// @note: H_INIT is currently supposed to be aligned with the split point; if this changes by striping
                        ///        library, then we need to calculate the start of the tile here and use partial writes to get
                        ///        correct HW behavior. This applies to both left and right stripes.
                        const UBWCTileInfo* pTileInfo = ImageFormatUtils::GetUBWCTileInfo(pImageFormat);

                        if (NULL != pTileInfo)
                        {
                            CAMX_ASSERT(CamxResultSuccess == result);
                            CAMX_ASSERT(0 != pTileInfo->BPPDenominator);
                            CAMX_ASSERT(0 != pTileInfo->widthPixels);

                            // First stripe
                            // For UBWC formats, offset indicates H_INIT value in pixels
                            // Y plane
                            UINT32 leftStripeWidthInPixel               =
                                m_stripeConfigs[DualIFEStripeIdLeft].stream[portIdx].width;
                            UINT32 leftStripeWidthInPixelDenomAligned   =
                                Utils::AlignGeneric32(leftStripeWidthInPixel, pTileInfo->BPPDenominator);
                            UINT32 leftStripeWidthInPixelTileAligned    =
                                Utils::AlignGeneric32(leftStripeWidthInPixel, pTileInfo->widthPixels);

                            UINT32 alignedWidth =
                                (leftStripeWidthInPixelTileAligned * pTileInfo->BPPNumerator) / pTileInfo->BPPDenominator;
                            // H_INIT for left stripe
                            leftIndex = GetStripeConfigIndex(DualIFEStripeIdLeft, numPorts, portId, 0);
                            pStripes[leftIndex].offset  = 0;
                            pStripes[leftIndex].width   = alignedWidth;
                            pStripes[leftIndex].portId  = CSLPortID;

                            // Calculate right partial write for Y
                            UINT32 partialLeft = 0;
                            UINT32 partialRight = ((leftStripeWidthInPixelTileAligned - leftStripeWidthInPixelDenomAligned)
                                                   * pTileInfo->BPPNumerator) / pTileInfo->BPPDenominator;
                            UINT32 leftTileConfig =
                                ((partialRight  << IFERightPartialTileShift)    & IFERightPartialTileMask) |
                                ((partialLeft   << IFELeftPartialTileShift)     & IFELeftPartialTileMask);

                            CAMX_LOG_VERBOSE(CamxLogGroupISP,
                                             "Idx:%u Full Y port (UBWC): left stripe offset: %d, width: %d, partial: %x",
                                             portIdx, pStripes[leftIndex].offset, pStripes[leftIndex].width, leftTileConfig);

                            // UV plane  (same split point and width)
                            // The same as Y plane
                            leftIndex = GetStripeConfigIndex(DualIFEStripeIdLeft, numPorts, portId, 1);
                            pStripes[leftIndex].offset  = 0;
                            pStripes[leftIndex].width   = alignedWidth;
                            pStripes[leftIndex].portId  = CSLPortID;

                            CAMX_LOG_VERBOSE(CamxLogGroupISP,
                                             "Idx:%u Full UV port (UBWC): left stripe offset: %d, width: %d, partial: %x",
                                             portIdx, pStripes[leftIndex].offset, pStripes[leftIndex].width, leftTileConfig);

                            // Second stripe
                            // Y plane
                            UINT32 rightStripeWidthInPixel              =
                                Utils::AlignGeneric32(m_stripeConfigs[1].stream[portIdx].width,
                                                      pTileInfo->BPPDenominator);
                            UINT32 rightStripeDenomAlignedStartInPixel  =
                                Utils::AlignGeneric32(m_stripeConfigs[DualIFEStripeIdRight].stream[portIdx].offset,
                                                      pTileInfo->BPPDenominator);
                            UINT32 rightStripeTileAlignedStartInPixel   =
                                (rightStripeDenomAlignedStartInPixel / pTileInfo->widthPixels) * pTileInfo->widthPixels;
                            UINT32 rightStripeTileAlignedEndInPixel     =
                                Utils::AlignGeneric32(rightStripeDenomAlignedStartInPixel + rightStripeWidthInPixel,
                                                      pTileInfo->widthPixels);

                            alignedWidth =
                                ((rightStripeTileAlignedEndInPixel - rightStripeTileAlignedStartInPixel)
                                 * pTileInfo->BPPNumerator)
                                / pTileInfo->BPPDenominator;

                            // H_INIT for right stripe: find the beginning of the tile where offset is.
                            rightIndex = GetStripeConfigIndex(DualIFEStripeIdRight, numPorts, portId, 0);
                            pStripes[rightIndex].offset    = rightStripeTileAlignedStartInPixel;
                            pStripes[rightIndex].width     = alignedWidth;
                            pStripes[rightIndex].portId    = CSLPortID;

                            // Calculate left and right partial write for Y
                            partialLeft     = pTileInfo->widthBytes - partialRight;
                            partialRight    =
                                ((rightStripeTileAlignedEndInPixel -
                                    (rightStripeDenomAlignedStartInPixel + rightStripeWidthInPixel))
                                * pTileInfo->BPPNumerator) / pTileInfo->BPPDenominator;
                            UINT32 rightTileConfig =
                                ((partialRight  << IFERightPartialTileShift)    & IFERightPartialTileMask) |
                                ((partialLeft   << IFELeftPartialTileShift)     & IFELeftPartialTileMask);

                            CAMX_LOG_VERBOSE(CamxLogGroupISP,
                                             "Idx:%u Full Y port (UBWC): right stripe offset: %d, width: %d, partial: %x",
                                             portIdx,
                                             pStripes[rightIndex].offset,
                                             pStripes[rightIndex].width,
                                             rightTileConfig);

                            // UV plane  (same split point and width)
                            // The same as Y
                            rightIndex = GetStripeConfigIndex(DualIFEStripeIdRight, numPorts, portId, 1);
                            pStripes[rightIndex].offset    = rightStripeTileAlignedStartInPixel;
                            pStripes[rightIndex].width     = alignedWidth;
                            pStripes[rightIndex].portId    = CSLPortID;

                            CAMX_LOG_VERBOSE(CamxLogGroupISP,
                                             "Idx:%u Full UV port (UBWC): right stripe offset: %d, width: %d, partial: %x",
                                             portIdx,
                                             pStripes[rightIndex].offset,
                                             pStripes[rightIndex].width,
                                             rightTileConfig);

                            pStripes[GetStripeConfigIndex(DualIFEStripeIdLeft, numPorts, portId, 0)].tileConfig    =
                                leftTileConfig;
                            pStripes[GetStripeConfigIndex(DualIFEStripeIdLeft, numPorts, portId, 1)].tileConfig    =
                                leftTileConfig;
                            pStripes[GetStripeConfigIndex(DualIFEStripeIdRight, numPorts, portId, 0)].tileConfig   =
                                rightTileConfig;
                            pStripes[GetStripeConfigIndex(DualIFEStripeIdRight, numPorts, portId, 1)].tileConfig   =
                                rightTileConfig;
                        }
                        else
                        {
                            CAMX_LOG_ERROR(CamxLogGroupISP, "pTileInfo is NULL");
                            result = CamxResultEInvalidPointer;
                        }
                    }
                    else if (CamX::Format::P010 == pImageFormat->format)
                    {
                        // First stripe
                        // For P010, pixel width == 2*byte width (for both Y and UV plane)
                        // Y plane
                        leftIndex = GetStripeConfigIndex(DualIFEStripeIdLeft, numPorts, portId, 0);
                        pStripes[leftIndex].offset = m_stripeConfigs[0].stream[portIdx].offset * 2;
                        pStripes[leftIndex].width  = m_stripeConfigs[0].stream[portIdx].width * 2;
                        pStripes[leftIndex].portId = CSLPortID;

                        CAMX_LOG_VERBOSE(CamxLogGroupISP,
                            "Idx:%u Full Y port: left stripe offset: %d, width: %d",
                            portIdx, pStripes[leftIndex].offset, pStripes[leftIndex].width);

                        // UV plane  (same split point and width)
                        leftIndex = GetStripeConfigIndex(DualIFEStripeIdLeft, numPorts, portId, 1);
                        pStripes[leftIndex].offset = m_stripeConfigs[0].stream[portIdx].offset * 2;
                        pStripes[leftIndex].width  = m_stripeConfigs[0].stream[portIdx].width * 2;
                        pStripes[leftIndex].portId = CSLPortID;

                        CAMX_LOG_VERBOSE(CamxLogGroupISP,
                            "Idx:%u Full UV port: left stripe offset: %d, width: %d",
                            portIdx, pStripes[leftIndex].offset, pStripes[leftIndex].width);

                        // Second stripe
                        // Y plane
                        rightIndex = GetStripeConfigIndex(DualIFEStripeIdRight, numPorts, portId, 0);
                        pStripes[rightIndex].offset = m_stripeConfigs[1].stream[portIdx].offset * 2;
                        pStripes[rightIndex].width  = m_stripeConfigs[1].stream[portIdx].width * 2;
                        pStripes[rightIndex].portId = CSLPortID;

                        CAMX_LOG_VERBOSE(CamxLogGroupISP,
                            "Idx:%u Full Y port: right stripe offset: %d, width: %d",
                            portIdx, pStripes[rightIndex].offset, pStripes[rightIndex].width);

                        // UV plane  (same split point and width)
                        rightIndex = GetStripeConfigIndex(DualIFEStripeIdRight, numPorts, portId, 1);
                        pStripes[rightIndex].offset = m_stripeConfigs[1].stream[portIdx].offset * 2;
                        pStripes[rightIndex].width  = m_stripeConfigs[1].stream[portIdx].width * 2;
                        pStripes[rightIndex].portId = CSLPortID;

                        CAMX_LOG_VERBOSE(CamxLogGroupISP,
                            "Idx:%u Full UV port: right stripe offset: %d, width: %d",
                            portIdx, pStripes[rightIndex].offset, pStripes[rightIndex].width);
                    }
                    else if ((CamX::Format::YUV420NV12TP10 == pImageFormat->format) ||
                             (CamX::Format::YUV420NV21TP10 == pImageFormat->format))
                    {
                        // First stripe
                        // For TP10, pixel width == Ceil(width, 3) * 4 / 3(for both Y and UV plane)
                        // Y plane
                        leftIndex                  = GetStripeConfigIndex(DualIFEStripeIdLeft, numPorts, portId, 0);
                        pStripes[leftIndex].offset = Utils::DivideAndCeil(m_stripeConfigs[0].stream[portIdx].offset, 3) * 4;
                        pStripes[leftIndex].width  = Utils::DivideAndCeil(m_stripeConfigs[0].stream[portIdx].width, 3) * 4;
                        pStripes[leftIndex].portId = CSLPortID;

                        CAMX_LOG_VERBOSE(CamxLogGroupISP,
                            "Idx:%u Full Y port: left stripe offset: %d, width: %d",
                            portIdx, pStripes[leftIndex].offset, pStripes[leftIndex].width);

                        // UV plane  (same split point and width)
                        leftIndex                  = GetStripeConfigIndex(DualIFEStripeIdLeft, numPorts, portId, 1);
                        pStripes[leftIndex].offset = Utils::DivideAndCeil(m_stripeConfigs[0].stream[portIdx].offset, 3) * 4;
                        pStripes[leftIndex].width  = Utils::DivideAndCeil(m_stripeConfigs[0].stream[portIdx].width, 3) * 4;
                        pStripes[leftIndex].portId = CSLPortID;

                        CAMX_LOG_VERBOSE(CamxLogGroupISP,
                            "Idx:%u Full UV port: left stripe offset: %d, width: %d",
                            portIdx, pStripes[leftIndex].offset, pStripes[leftIndex].width);

                        // Second stripe
                        // Y plane
                        rightIndex                  = GetStripeConfigIndex(DualIFEStripeIdRight, numPorts, portId, 0);
                        pStripes[rightIndex].offset = Utils::DivideAndCeil(m_stripeConfigs[1].stream[portIdx].offset, 3) * 4;
                        pStripes[rightIndex].width  = Utils::DivideAndCeil(m_stripeConfigs[1].stream[portIdx].width, 3) * 4;
                        pStripes[rightIndex].portId = CSLPortID;

                        CAMX_LOG_VERBOSE(CamxLogGroupISP,
                            "Idx:%u Full Y port: right stripe offset: %d, width: %d",
                            portIdx, pStripes[rightIndex].offset, pStripes[rightIndex].width);

                        // UV plane  (same split point and width)
                        rightIndex                  = GetStripeConfigIndex(DualIFEStripeIdRight, numPorts, portId, 1);
                        pStripes[rightIndex].offset = Utils::DivideAndCeil(m_stripeConfigs[1].stream[portIdx].offset, 3) * 4;
                        pStripes[rightIndex].width  = Utils::DivideAndCeil(m_stripeConfigs[1].stream[portIdx].width, 3) * 4;
                        pStripes[rightIndex].portId = CSLPortID;

                        CAMX_LOG_VERBOSE(CamxLogGroupISP,
                            "Idx:%u Full UV port: right stripe offset: %d, width: %d",
                            portIdx, pStripes[rightIndex].offset, pStripes[rightIndex].width);
                    }
                    else
                    {
                        CAMX_NOT_IMPLEMENTED();
                    }
                    break;

                case IFEOutputPortFD:
                    portId = CSLIFEPortIndex(CSLIFEPortIdFD);
                    if ((CamX::Format::YUV420NV12   == pImageFormat->format)  ||
                        (CamX::Format::YUV420NV21   == pImageFormat->format)  ||
                        (CamX::Format::FDYUV420NV12 == pImageFormat->format)||
                        (CamX::Format::FDY8         == pImageFormat->format))
                    {
                        // First stripe
                        // For NV12, pixel width == byte width (for both Y and UV plane)
                        // Y plane
                        leftIndex = GetStripeConfigIndex(DualIFEStripeIdLeft, numPorts, portId, 0);
                        pStripes[leftIndex].offset = m_stripeConfigs[DualIFEStripeIdLeft].stream[FDOutput].offset;
                        pStripes[leftIndex].width  = m_stripeConfigs[DualIFEStripeIdLeft].stream[FDOutput].width;
                        pStripes[leftIndex].portId = CSLIFEPortIdFD;
                        CAMX_LOG_VERBOSE(CamxLogGroupISP, "left FD width %d offset %d",
                            pStripes[leftIndex].width , pStripes[leftIndex].offset);

                        // UV plane  (same split point and width)
                        leftIndex = GetStripeConfigIndex(DualIFEStripeIdLeft, numPorts, portId, 1);
                        pStripes[leftIndex].offset = m_stripeConfigs[DualIFEStripeIdLeft].stream[FDOutput].offset;
                        pStripes[leftIndex].width  = m_stripeConfigs[DualIFEStripeIdLeft].stream[FDOutput].width;
                        pStripes[leftIndex].portId = CSLIFEPortIdFD;

                        // Second stripe
                        // Y plane
                        rightIndex = GetStripeConfigIndex(DualIFEStripeIdRight, numPorts, portId, 0);
                        pStripes[rightIndex].offset = m_stripeConfigs[DualIFEStripeIdRight].stream[FDOutput].offset;
                        pStripes[rightIndex].width  = m_stripeConfigs[DualIFEStripeIdRight].stream[FDOutput].width;
                        pStripes[rightIndex].portId = CSLIFEPortIdFD;
                        CAMX_LOG_VERBOSE(CamxLogGroupISP, "right FD width %d offset %d",
                            pStripes[rightIndex].width , pStripes[rightIndex].offset);

                        // UV plane  (same split point and width)
                        rightIndex = GetStripeConfigIndex(DualIFEStripeIdRight, numPorts, portId, 1);
                        pStripes[rightIndex].offset = m_stripeConfigs[DualIFEStripeIdRight].stream[FDOutput].offset;
                        pStripes[rightIndex].width  = m_stripeConfigs[DualIFEStripeIdRight].stream[FDOutput].width;
                        pStripes[rightIndex].portId = CSLIFEPortIdFD;
                    }
                    else
                    {
                        CAMX_NOT_IMPLEMENTED();
                    }
                    break;

                case IFEOutputPortDS4:
                case IFEOutputPortDisplayDS4:
                    CAMX_ASSERT(CamX::Format::PD10 == pImageFormat->format);
                    portId    = CSLIFEPortIndex(CSLIFEPortIdDownscaled4);
                    CSLPortID = CSLIFEPortIdDownscaled4;
                    portIdx   = DS4Output;
                    if (IFEOutputPortDisplayDS4 == outputPortId[outputPortIndex])
                    {
                        portId    = CSLIFEPortIndex(CSLIFEPortIdDisplayDownscaled4);
                        CSLPortID = CSLIFEPortIdDisplayDownscaled4;
                        portIdx   = DisplayDS4Output;
                    }
                    // First stripe
                    leftIndex = GetStripeConfigIndex(DualIFEStripeIdLeft, numPorts, portId, 0);
                    pStripes[leftIndex].offset  = m_stripeConfigs[DualIFEStripeIdLeft].stream[portIdx].offset * 4;
                    pStripes[leftIndex].width   =
                        Utils::AlignGeneric32(m_stripeConfigs[DualIFEStripeIdLeft].stream[portIdx].width * 4, 16);
                    pStripes[leftIndex].portId  = CSLPortID;
                    CAMX_LOG_VERBOSE(CamxLogGroupISP, "Idx:%u left DS4 width %d offset %d",
                        portIdx, pStripes[leftIndex].width , pStripes[leftIndex].offset);
                    // Second stripe
                    rightIndex = GetStripeConfigIndex(DualIFEStripeIdRight, numPorts, portId, 0);
                    pStripes[rightIndex].offset = m_stripeConfigs[DualIFEStripeIdRight].stream[portIdx].offset * 4;
                    pStripes[rightIndex].width  =
                        Utils::AlignGeneric32(m_stripeConfigs[DualIFEStripeIdRight].stream[portIdx].width * 4, 16);
                    pStripes[rightIndex].portId = CSLPortID;
                    CAMX_LOG_VERBOSE(CamxLogGroupISP, "Idx:%u right DS4 width %d offset %d",
                        portIdx, pStripes[rightIndex].width, pStripes[rightIndex].offset);
                    break;

                case IFEOutputPortDS16:
                case IFEOutputPortDisplayDS16:
                    CAMX_ASSERT(CamX::Format::PD10 == pImageFormat->format);
                    portId    = CSLIFEPortIndex(CSLIFEPortIdDownscaled16);
                    CSLPortID = CSLIFEPortIdDownscaled16;
                    portIdx   = DS16Output;
                    if (IFEOutputPortDisplayDS16 == outputPortId[outputPortIndex])
                    {
                        portId    = CSLIFEPortIndex(CSLIFEPortIdDisplayDownscaled16);
                        CSLPortID = CSLIFEPortIdDisplayDownscaled16;
                        portIdx   = DisplayDS16Output;
                    }
                    // First stripe
                    leftIndex = GetStripeConfigIndex(DualIFEStripeIdLeft, numPorts, portId, 0);
                    pStripes[leftIndex].offset  = m_stripeConfigs[DualIFEStripeIdLeft].stream[portIdx].offset * 4;
                    pStripes[leftIndex].width   =
                        Utils::AlignGeneric32(m_stripeConfigs[DualIFEStripeIdLeft].stream[portIdx].width * 4, 16);
                    pStripes[leftIndex].portId  = CSLPortID;
                    CAMX_LOG_VERBOSE(CamxLogGroupISP, "Idx:%u left DS16 width %d offset %d",
                        portIdx, pStripes[leftIndex].width, pStripes[leftIndex].offset);

                    // Second stripe
                    rightIndex = GetStripeConfigIndex(DualIFEStripeIdRight, numPorts, portId, 0);
                    pStripes[rightIndex].offset = m_stripeConfigs[DualIFEStripeIdRight].stream[portIdx].offset * 4;
                    pStripes[rightIndex].width  =
                        Utils::AlignGeneric32(m_stripeConfigs[DualIFEStripeIdRight].stream[portIdx].width * 4, 16);
                    pStripes[rightIndex].portId = CSLPortID;
                    CAMX_LOG_VERBOSE(CamxLogGroupISP, "Idx:%u right DS16 width %d offset %d",
                        portIdx, pStripes[rightIndex].width, pStripes[rightIndex].offset);
                    break;

                case IFEOutputPortStatsAWBBG:
                    CAMX_ASSERT(CamX::Format::Blob == pImageFormat->format);
                    portId      = CSLIFEPortIndex(CSLIFEPortIdStatAWBBG);
                    statsPortId = CSLIFEStatsPortIndex(CSLIFEPortIdStatAWBBG);
                    // First stripe
                    leftIndex = GetStripeConfigIndex(DualIFEStripeIdLeft, numPorts, portId, 0);
                    pStripes[leftIndex].offset  = 0;
                    pStripes[leftIndex].width   = m_stripeConfigs[DualIFEStripeIdLeft].stats[statsPortId].width;
                    pStripes[leftIndex].portId  = CSLIFEPortIdStatAWBBG;

                    // Second stripe
                    rightIndex = GetStripeConfigIndex(DualIFEStripeIdRight, numPorts, portId, 0);
                    pStripes[rightIndex].offset = AWBBGStatsMaxWidth;
                    pStripes[rightIndex].width  = m_stripeConfigs[DualIFEStripeIdRight].stats[statsPortId].width;
                    pStripes[rightIndex].portId = CSLIFEPortIdStatAWBBG;
                    break;
                case IFEOutputPortStatsHDRBE:
                    CAMX_ASSERT(CamX::Format::Blob == pImageFormat->format);
                    portId = CSLIFEPortIndex(CSLIFEPortIdStatHDRBE);
                    statsPortId = CSLIFEStatsPortIndex(CSLIFEPortIdStatHDRBE);
                    // First stripe
                    leftIndex = GetStripeConfigIndex(DualIFEStripeIdLeft, numPorts, portId, 0);
                    pStripes[leftIndex].offset  = 0;
                    pStripes[leftIndex].width   = m_stripeConfigs[DualIFEStripeIdLeft].stats[statsPortId].width;
                    pStripes[leftIndex].portId  = CSLIFEPortIdStatHDRBE;

                    // Second stripe
                    rightIndex = GetStripeConfigIndex(DualIFEStripeIdRight, numPorts, portId, 0);
                    pStripes[rightIndex].offset = HDRBEStatsMaxWidth;
                    pStripes[rightIndex].width  = m_stripeConfigs[DualIFEStripeIdRight].stats[statsPortId].width;
                    pStripes[rightIndex].portId = CSLIFEPortIdStatHDRBE;
                    break;
                case IFEOutputPortStatsBHIST:
                    CAMX_ASSERT(CamX::Format::Blob == pImageFormat->format);
                    portId = CSLIFEPortIndex(CSLIFEPortIdStatBHIST);
                    statsPortId = CSLIFEStatsPortIndex(CSLIFEPortIdStatBHIST);
                    // First stripe
                    leftIndex = GetStripeConfigIndex(DualIFEStripeIdLeft, numPorts, portId, 0);
                    pStripes[leftIndex].offset    = 0;
                    pStripes[leftIndex].width     =
                        m_stripeConfigs[DualIFEStripeIdLeft].stats[statsPortId].width;
                    pStripes[leftIndex].portId  = CSLIFEPortIdStatBHIST;

                    // Second stripe
                    rightIndex = GetStripeConfigIndex(DualIFEStripeIdRight, numPorts, portId, 0);
                    pStripes[rightIndex].offset    = BHistStatsWidth;
                    pStripes[rightIndex].width     =
                        m_stripeConfigs[DualIFEStripeIdRight].stats[statsPortId].width;
                    pStripes[rightIndex].portId = CSLIFEPortIdStatBHIST;
                    break;

                case IFEOutputPortStatsHDRBHIST:
                    CAMX_ASSERT(CamX::Format::Blob == pImageFormat->format);
                    portId = CSLIFEPortIndex(CSLIFEPortIdStatHDRBHIST);
                    statsPortId = CSLIFEStatsPortIndex(CSLIFEPortIdStatHDRBHIST);
                    // First stripe
                    leftIndex = GetStripeConfigIndex(DualIFEStripeIdLeft, numPorts, portId, 0);
                    pStripes[leftIndex].offset    = 0;
                    pStripes[leftIndex].width     =
                        m_stripeConfigs[DualIFEStripeIdLeft].stats[statsPortId].width;
                    pStripes[leftIndex].portId  = CSLIFEPortIdStatHDRBHIST;

                    // Second stripe
                    rightIndex = GetStripeConfigIndex(DualIFEStripeIdRight, numPorts, portId, 0);
                    pStripes[rightIndex].offset    = HDRBHistStatsMaxWidth;
                    pStripes[rightIndex].width     =
                        m_stripeConfigs[DualIFEStripeIdRight].stats[statsPortId].width;
                    pStripes[rightIndex].portId = CSLIFEPortIdStatHDRBHIST;
                    break;

                case IFEOutputPortStatsIHIST:
                    CAMX_ASSERT(CamX::Format::Blob == pImageFormat->format);
                    portId = CSLIFEPortIndex(CSLIFEPortIdStatIHIST);
                    statsPortId = CSLIFEStatsPortIndex(CSLIFEPortIdStatIHIST);
                    // First stripe
                    leftIndex = GetStripeConfigIndex(DualIFEStripeIdLeft, numPorts, portId, 0);
                    pStripes[leftIndex].offset  = 0;
                    pStripes[leftIndex].width   = m_stripeConfigs[DualIFEStripeIdLeft].stats[statsPortId].width;
                    pStripes[leftIndex].portId  = CSLIFEPortIdStatIHIST;

                    // Second stripe
                    rightIndex = GetStripeConfigIndex(DualIFEStripeIdRight, numPorts, portId, 0);
                    pStripes[rightIndex].offset = IHistStatsWidth;
                    pStripes[rightIndex].width  = m_stripeConfigs[DualIFEStripeIdRight].stats[statsPortId].width;
                    pStripes[rightIndex].portId = CSLIFEPortIdStatIHIST;
                    break;

                case IFEOutputPortStatsRS:
                    CAMX_ASSERT(CamX::Format::Blob == pImageFormat->format);
                    portId = CSLIFEPortIndex(CSLIFEPortIdStatRS);
                    statsPortId = CSLIFEStatsPortIndex(CSLIFEPortIdStatRS);
                    // First stripe
                    leftIndex = GetStripeConfigIndex(DualIFEStripeIdLeft, numPorts, portId, 0);
                    pStripes[leftIndex].offset  = 0;
                    pStripes[leftIndex].width   = m_stripeConfigs[DualIFEStripeIdLeft].stats[statsPortId].width;
                    pStripes[leftIndex].portId  = CSLIFEPortIdStatRS;

                    // Second stripe
                    rightIndex = GetStripeConfigIndex(DualIFEStripeIdRight, numPorts, portId, 0);
                    pStripes[rightIndex].offset = pStripes[leftIndex].width;
                    pStripes[rightIndex].width  = RSStatsWidth;
                    pStripes[rightIndex].portId = CSLIFEPortIdStatRS;
                    break;

                case IFEOutputPortStatsCS:
                    CAMX_ASSERT(CamX::Format::Blob == pImageFormat->format);
                    portId = CSLIFEPortIndex(CSLIFEPortIdStatCS);
                    statsPortId = CSLIFEStatsPortIndex(CSLIFEPortIdStatCS);
                    // First stripe
                    leftIndex = GetStripeConfigIndex(DualIFEStripeIdLeft, numPorts, portId, 0);
                    pStripes[leftIndex].offset  = 0;
                    pStripes[leftIndex].width   = m_stripeConfigs[DualIFEStripeIdLeft].stats[statsPortId].width;
                    pStripes[leftIndex].portId  = CSLIFEPortIdStatCS;

                    // Second stripe
                    rightIndex = GetStripeConfigIndex(DualIFEStripeIdRight, numPorts, portId, 0);
                    pStripes[rightIndex].offset = CSStatsWidth;
                    pStripes[rightIndex].width  = m_stripeConfigs[DualIFEStripeIdRight].stats[statsPortId].width;
                    pStripes[rightIndex].portId = CSLIFEPortIdStatCS;
                    break;

                case IFEOutputPortStatsBF:
                    CAMX_ASSERT(CamX::Format::Blob == pImageFormat->format);
                    portId = CSLIFEPortIndex(CSLIFEPortIdStatBF);
                    statsPortId = CSLIFEStatsPortIndex(CSLIFEPortIdStatBF);
                    // First stripe
                    leftIndex = GetStripeConfigIndex(DualIFEStripeIdLeft, numPorts, portId, 0);
                    pStripes[leftIndex].offset  = 0;
                    pStripes[leftIndex].width   = m_stripeConfigs[DualIFEStripeIdLeft].stats[statsPortId].width;
                    pStripes[leftIndex].portId  = CSLIFEPortIdStatBF;

                    // Second stripe
                    rightIndex = GetStripeConfigIndex(DualIFEStripeIdRight, numPorts, portId, 0);
                    pStripes[rightIndex].offset     = BFStatsMaxWidth;
                    pStripes[rightIndex].width      = m_stripeConfigs[DualIFEStripeIdRight].stats[statsPortId].width;
                    pStripes[rightIndex].portId     = CSLIFEPortIdStatBF;
                    break;

                case IFEOutputPortStatsTLBG:
                    CAMX_ASSERT(CamX::Format::Blob == pImageFormat->format);
                    portId = CSLIFEPortIndex(CSLIFEPortIdStatTintlessBG);
                    statsPortId = CSLIFEStatsPortIndex(CSLIFEPortIdStatTintlessBG);
                    // First stripe
                    leftIndex = GetStripeConfigIndex(DualIFEStripeIdLeft, numPorts, portId, 0);
                    pStripes[leftIndex].offset = 0;
                    pStripes[leftIndex].width = m_stripeConfigs[DualIFEStripeIdLeft].stats[statsPortId].width;
                    pStripes[leftIndex].portId = CSLIFEPortIdStatTintlessBG;

                    // Second stripe
                    rightIndex = GetStripeConfigIndex(DualIFEStripeIdRight, numPorts, portId, 0);
                    pStripes[rightIndex].offset = TintlessBGStatsWidth;
                    pStripes[rightIndex].width = m_stripeConfigs[DualIFEStripeIdRight].stats[statsPortId].width;
                    pStripes[rightIndex].portId = CSLIFEPortIdStatTintlessBG;
                    break;

                case IFEOutputPortPDAF:
                    portId = CSLIFEPortIndex(CSLIFEPortIdPDAF);
                    if (CamX::Format::RawPlain16 == pImageFormat->format)
                    {
                        // First stripe
                        leftIndex = GetStripeConfigIndex(DualIFEStripeIdLeft, numPorts, portId, 0);
                        pStripes[leftIndex].offset = m_stripeConfigs[0].stream[PDAFRawOutput].offset;
                        pStripes[leftIndex].width  = m_stripeConfigs[0].stream[PDAFRawOutput].width * 2;
                        pStripes[leftIndex].portId = CSLIFEPortIdPDAF;

                        // Second stripe
                        rightIndex = GetStripeConfigIndex(DualIFEStripeIdRight, numPorts, portId, 0);
                        pStripes[rightIndex].offset = m_stripeConfigs[1].stream[PDAFRawOutput].offset * 2;
                        pStripes[rightIndex].width  = m_stripeConfigs[1].stream[PDAFRawOutput].width * 2;
                        pStripes[rightIndex].portId = CSLIFEPortIdPDAF;
                    }
                    break;

                default:
                    CAMX_NOT_IMPLEMENTED();
                    break;
            }
        }
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupISP, "pDualConfig is NULL");
        result = CamxResultEInvalidPointer;
    }

    if (CamxResultSuccess == result)
    {
        result = m_pDualIFEConfigCmdBuffer->CommitCommands();
        CAMX_ASSERT(CamxResultSuccess == result);
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::CalculateIQCmdSize
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IFENode::CalculateIQCmdSize()
{
    UINT count = 0;

    for (count = 0; count < m_numIFEIQModule; count++)
    {
        IQModuleDualIFEData info = { 0 };
        m_pIFEIQModule[count]->GetDualIFEData(&info);

        m_totalIQCmdSizeDWord += m_pIFEIQModule[count]->GetIQCmdLength();

        m_pIFEIQModule[count]->Set32bitDMIBufferOffset(m_total32bitDMISizeDWord);
        m_total32bitDMISizeDWord += m_pIFEIQModule[count]->Get32bitDMILength();
        m_pIFEIQModule[count]->Set64bitDMIBufferOffset(m_total64bitDMISizeDWord);
        m_total64bitDMISizeDWord += m_pIFEIQModule[count]->Get64bitDMILength();

        if (TRUE == info.dualIFEDMI32Sensitive)
        {
            m_total32bitDMISizeDWord += m_pIFEIQModule[count]->Get32bitDMILength();
        }

        if (TRUE == info.dualIFEDMI64Sensitive)
        {
            m_total64bitDMISizeDWord += m_pIFEIQModule[count]->Get64bitDMILength();
        }
    }

    for (count = 0; count < m_numIFEStatsModule; count++)
    {
        IQModuleDualIFEData info = { 0 };
        m_pIFEStatsModule[count]->GetDualIFEData(&info);

        m_totalIQCmdSizeDWord += m_pIFEStatsModule[count]->GetIQCmdLength();

        m_pIFEStatsModule[count]->Set32bitDMIBufferOffset(m_total32bitDMISizeDWord);
        m_total32bitDMISizeDWord += m_pIFEStatsModule[count]->Get32bitDMILength();
        m_pIFEStatsModule[count]->Set64bitDMIBufferOffset(m_total64bitDMISizeDWord);
        m_total64bitDMISizeDWord += m_pIFEStatsModule[count]->Get64bitDMILength();

        if (TRUE == info.dualIFEDMI32Sensitive)
        {
            m_total32bitDMISizeDWord += m_pIFEStatsModule[count]->Get32bitDMILength();
        }

        if (TRUE == info.dualIFEDMI64Sensitive)
        {
            m_total64bitDMISizeDWord += m_pIFEStatsModule[count]->Get64bitDMILength();
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::SetDependencies
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IFENode::SetDependencies(
    NodeProcessRequestData* pNodeRequestData,
    BOOL                    hasExplicitDependencies)
{
    UINT32 count = 0;
    UINT64 requestIdOffsetFromLastFlush = GetRequestIdOffsetFromLastFlush(pNodeRequestData->pCaptureRequest->requestId);

    // Set a dependency on the completion of the previous ExecuteProcessRequest() call
    // so that we can guarantee serialization of all ExecuteProcessRequest() calls for this node.
    // Needed since the ExecuteProcessRequest() implementation is not reentrant.
    // Remove when requirement CAMX-3030 is implemented.

    // Skip setting dependency for first request
    if (FirstValidRequestId < requestIdOffsetFromLastFlush)
    {
        pNodeRequestData->dependencyInfo[0].dependencyFlags.hasPropertyDependency = TRUE;
        pNodeRequestData->dependencyInfo[0].propertyDependency.properties[count] = GetNodeCompleteProperty();
        // Always point to the previous request. Should NOT be tied to the pipeline delay!
        pNodeRequestData->dependencyInfo[0].propertyDependency.offsets[count]     = 1;
        count++;
    }

    if (TRUE == hasExplicitDependencies)
    {
        CAMX_LOG_VERBOSE(CamxLogGroupISP, "IFE: ProcessRequest: Setting IQ dependencies for Req#%llu",
            pNodeRequestData->pCaptureRequest->requestId);

        pNodeRequestData->dependencyInfo[0].dependencyFlags.hasPropertyDependency = TRUE;

        if (TRUE == IsTagPresentInPublishList(PropertyIDAECFrameControl))
        {
            pNodeRequestData->dependencyInfo[0].propertyDependency.properties[count++] = PropertyIDAECFrameControl;
        }
        if (TRUE == IsTagPresentInPublishList(PropertyIDAECStatsControl))
        {
            pNodeRequestData->dependencyInfo[0].propertyDependency.properties[count++] = PropertyIDAECStatsControl;
        }
        if (TRUE == IsTagPresentInPublishList(PropertyIDAWBFrameControl))
        {
            pNodeRequestData->dependencyInfo[0].propertyDependency.properties[count++] = PropertyIDAWBFrameControl;
        }
        if (TRUE == IsTagPresentInPublishList(PropertyIDAWBStatsControl))
        {
            pNodeRequestData->dependencyInfo[0].propertyDependency.properties[count++] = PropertyIDAWBStatsControl;
        }
        if (TRUE == IsTagPresentInPublishList(PropertyIDAFDStatsControl))
        {
            pNodeRequestData->dependencyInfo[0].propertyDependency.properties[count++] = PropertyIDAFDStatsControl;
        }
        if (TRUE == IsTagPresentInPublishList(PropertyIDPostSensorGainId))
        {
            pNodeRequestData->dependencyInfo[0].propertyDependency.properties[count++] = PropertyIDPostSensorGainId;
        }
        if (TRUE == IsTagPresentInPublishList(PropertyIDParsedBHistStatsOutput) &&
            (TRUE == m_IFEOutputPathInfo[IFEOutputPortStatsBHIST].path))
        {
            pNodeRequestData->dependencyInfo[0].propertyDependency.properties[count++] = PropertyIDParsedBHistStatsOutput;
        }

        // AF Stats is not required for Fast AEC,
        // so, skipping dependency check for AF when Fast AEC is enabled
        if (TRUE == IsTagPresentInPublishList(PropertyIDAFStatsControl) &&
            TRUE == IsTagPresentInPublishList(PropertyIDAFFrameControl))
        {
            // Check AF control
            pNodeRequestData->dependencyInfo[0].propertyDependency.properties[count++]  = PropertyIDAFStatsControl;
            pNodeRequestData->dependencyInfo[0].propertyDependency.properties[count++]  = PropertyIDAFFrameControl;

            if (TRUE == IsPDHwEnabled() && (TRUE == IsTagPresentInPublishList(PropertyIDPDHwConfig)))
            {
                pNodeRequestData->dependencyInfo[0].propertyDependency.properties[count++]  = PropertyIDPDHwConfig;
            }

            if (TRUE == IsTagPresentInPublishList(PropertyIDParsedTintlessBGStatsOutput) &&
                (TRUE == m_IFEOutputPathInfo[IFEOutputPortStatsTLBG].path))
            {
                pNodeRequestData->dependencyInfo[0].propertyDependency.properties[count++] =
                    PropertyIDParsedTintlessBGStatsOutput;
            }
        }
        if (TRUE == IsTagPresentInPublishList(PropertyIDSensorNumberOfLEDs))
        {
            pNodeRequestData->dependencyInfo[0].propertyDependency.properties[count++] = PropertyIDSensorNumberOfLEDs;
        }
    }
    else
    {
        CAMX_LOG_VERBOSE(CamxLogGroupISP, "IFE: Skip Setting Dependency for ReqID:%llu",
                      pNodeRequestData->pCaptureRequest->requestId);
    }

    pNodeRequestData->dependencyInfo[0].propertyDependency.count                            = count;
    pNodeRequestData->dependencyInfo[0].dependencyFlags.hasIOBufferAvailabilityDependency   = TRUE;
    pNodeRequestData->dependencyInfo[0].processSequenceId                                   = 1;
    pNodeRequestData->numDependencyLists                                                    = 1;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::ProcessingNodeFinalizeInputRequirement
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFENode::ProcessingNodeFinalizeInputRequirement(
    BufferNegotiationData* pBufferNegotiationData)
{
    CAMX_ASSERT(NULL != pBufferNegotiationData);

    CamxResult      result                          = CamxResultSuccess;
    UINT32          optimalInputWidth               = 0;
    UINT32          optimalInputHeight              = 0;
    UINT32          minInputWidth                   = 0;
    UINT32          minInputHeight                  = 0;
    UINT32          maxInputWidth                   = 0;
    UINT32          maxInputHeight                  = 0;
    UINT32          perOutputPortOptimalWidth       = 0;
    UINT32          perOutputPortOptimalHeight      = 0;
    UINT32          perOutputPortMinWidth           = 0;
    UINT32          perOutputPortMinHeight          = 0;
    UINT32          perOutputPortMaxWidth           = IFEMaxOutputWidthFull * 2;
    UINT32          perOutputPortMaxHeight          = IFEMaxOutputHeight;
    UINT32          totalInputPorts                 = 0;
    AlignmentInfo   alignmentLCM[FormatsMaxPlanes]  = { { 0 } };
    UINT32          inputPortId[MaxIFEInputPorts];

    // Get Input Port List
    GetAllInputPortIds(&totalInputPorts, &inputPortId[0]);

    // The IFE node will have to loop through all the output ports which are connected to a child node or a HAL target.
    // The input buffer requirement will be the super resolution after looping through all the output ports.
    // The super resolution may have different aspect ratio compared to the original output port aspect ratio, but
    // this will be taken care of by the crop hardware associated with the output port.
    for (UINT index = 0; index < pBufferNegotiationData->numOutputPortsNotified; index++)
    {
        OutputPortNegotiationData* pOutputPortNegotiationData = &pBufferNegotiationData->pOutputPortNegotiationData[index];
        UINT                       outputPortIndex            =
            pBufferNegotiationData->pOutputPortNegotiationData[index].outputPortIndex;
        UINT                       outputPortId               = GetOutputPortId(outputPortIndex);

        if ((FALSE == IsStatsOutputPort(outputPortId)) &&
            (TRUE == IsPixelOutputPortSourceType(outputPortId)))
        {
            perOutputPortOptimalWidth  = 0;
            perOutputPortOptimalHeight = 0;
            perOutputPortMinWidth      = 0;
            perOutputPortMinHeight     = 0;
            perOutputPortMaxWidth      = IFEMaxOutputWidthFull * 2 ;
            perOutputPortMaxHeight     = IFEMaxOutputHeight;

            // Get Max FD Output Width values based on the camera platform.
            UpdateIFECapabilityBasedOnCameraPlatform();

            // FD port has a different limit than the full port.
            if (IFEOutputPortFD == outputPortId)
            {
                perOutputPortMaxWidth = m_maxOutputWidthFD * 2;
            }

            if (TRUE == m_RDIOnlyUseCase)
            {
                CAMX_LOG_INFO(CamxLogGroupISP, "ignore Single VFE output limitatation for RDI only usecase");
                perOutputPortMaxWidth  = IFEMAXOutputWidthRDIOnly;
                perOutputPortMaxHeight = IFEMAXOutputHeightRDIOnly;
            }

            Utils::Memset(&pOutputPortNegotiationData->outputBufferRequirementOptions, 0, sizeof(BufferRequirement));

            // Go through the requirements of the destination ports connected to a given output port of IFE
            for (UINT inputIndex = 0; inputIndex < pOutputPortNegotiationData->numInputPortsNotification; inputIndex++)
            {
                BufferRequirement* pInputPortRequirement = &pOutputPortNegotiationData->inputPortRequirement[inputIndex];

                // Optimal width per port is the super resolution of all the connected destination ports' optimal needs.
                perOutputPortOptimalWidth  =
                    Utils::MaxUINT32(pInputPortRequirement->optimalWidth, perOutputPortOptimalWidth);
                perOutputPortOptimalHeight =
                    Utils::MaxUINT32(pInputPortRequirement->optimalHeight, perOutputPortOptimalHeight);
                perOutputPortMinWidth      =
                    Utils::MaxUINT32(pInputPortRequirement->minWidth, perOutputPortMinWidth);
                perOutputPortMinHeight     =
                    Utils::MaxUINT32(pInputPortRequirement->minHeight, perOutputPortMinHeight);
                perOutputPortMaxWidth      =
                   Utils::MinUINT32(pInputPortRequirement->maxWidth, perOutputPortMaxWidth);
                perOutputPortMaxHeight     =
                    Utils::MinUINT32(pInputPortRequirement->maxHeight, perOutputPortMaxHeight);

                for (UINT planeIdx = 0; planeIdx < FormatsMaxPlanes; planeIdx++)
                {
                    alignmentLCM[planeIdx].strideAlignment =
                        Utils::CalculateLCM(
                            static_cast<INT32>(alignmentLCM[planeIdx].strideAlignment),
                            static_cast<INT32>(pInputPortRequirement->planeAlignment[planeIdx].strideAlignment));
                    alignmentLCM[planeIdx].scanlineAlignment =
                        Utils::CalculateLCM(
                            static_cast<INT32>(alignmentLCM[planeIdx].scanlineAlignment),
                            static_cast<INT32>(pInputPortRequirement->planeAlignment[planeIdx].scanlineAlignment));
                }
            }

            // Ensure optimal dimensions are within min and max dimensions. There are chances that the optimal dimension goes
            // over the max. Correct for the same.
            UINT32 originalOptimalWidth = perOutputPortOptimalWidth;
            perOutputPortOptimalWidth  =
                Utils::ClampUINT32(perOutputPortOptimalWidth, perOutputPortMinWidth, perOutputPortMaxWidth);

            // Calculate OptimalHeight if witdh is clamped.
            if (originalOptimalWidth != 0)
            {
                perOutputPortOptimalHeight  =
                    Utils::RoundFLOAT(static_cast<FLOAT>(
                        (perOutputPortOptimalWidth * perOutputPortOptimalHeight) / originalOptimalWidth));
            }
            perOutputPortOptimalHeight =
                Utils::ClampUINT32(perOutputPortOptimalHeight, perOutputPortMinHeight, perOutputPortMaxHeight);

            // Store the buffer requirements for this output port which will be reused to set, during forward walk.
            // The values stored here could be final output dimensions unless it is overridden by forward walk.
            pOutputPortNegotiationData->outputBufferRequirementOptions.optimalWidth  = perOutputPortOptimalWidth;
            pOutputPortNegotiationData->outputBufferRequirementOptions.optimalHeight = perOutputPortOptimalHeight;

            pOutputPortNegotiationData->outputBufferRequirementOptions.minWidth      = perOutputPortMinWidth;
            pOutputPortNegotiationData->outputBufferRequirementOptions.minHeight     = perOutputPortMinHeight;
            pOutputPortNegotiationData->outputBufferRequirementOptions.maxWidth      = perOutputPortMaxWidth;
            pOutputPortNegotiationData->outputBufferRequirementOptions.maxHeight     = perOutputPortMaxHeight;

            Utils::Memcpy(&pOutputPortNegotiationData->outputBufferRequirementOptions.planeAlignment[0],
                          &alignmentLCM[0],
                          sizeof(AlignmentInfo) * FormatsMaxPlanes);

            Utils::Memset(&alignmentLCM[0], 0, sizeof(AlignmentInfo) * FormatsMaxPlanes);

            optimalInputWidth  = Utils::MaxUINT32(perOutputPortOptimalWidth,  optimalInputWidth);
            optimalInputHeight = Utils::MaxUINT32(perOutputPortOptimalHeight, optimalInputHeight);
            minInputWidth      = Utils::MaxUINT32(perOutputPortMinWidth,      minInputWidth);
            minInputHeight     = Utils::MaxUINT32(perOutputPortMinHeight,     minInputHeight);
            maxInputWidth      = Utils::MaxUINT32(perOutputPortMaxWidth,      maxInputWidth);
            maxInputHeight     = Utils::MaxUINT32(perOutputPortMaxHeight,     maxInputHeight);
        }
        else // Stats and DS ports do not take part in buffer negotiation
        {
            const ImageFormat* pImageFormat = GetOutputPortImageFormat(OutputPortIndex(outputPortId));

            pOutputPortNegotiationData->outputBufferRequirementOptions.optimalWidth  = pImageFormat->width;
            pOutputPortNegotiationData->outputBufferRequirementOptions.optimalHeight = pImageFormat->height;
        }
    }

    CheckForRDIOnly();

    if ((FALSE == m_RDIOnlyUseCase) &&
        ((0 == optimalInputWidth) || (0 == optimalInputHeight)))
    {
        result = CamxResultEFailed;

        CAMX_LOG_ERROR(CamxLogGroupISP,
                       "ERROR: Buffer Negotiation Failed, W:%d x H:%d!\n",
                       optimalInputWidth,
                       optimalInputHeight);
    }
    else
    {
        if ((minInputWidth > maxInputWidth) || (minInputHeight > maxInputHeight))
        {
            CAMX_LOG_WARN(CamxLogGroupPProc, "Min > Max, unable to use current format");
            result = CamxResultEFailed;
        }

        // Ensure optimal dimensions are within min and max dimensions. There are chances that the optimal dimension goes
        // over the max. Correct for the same.
        optimalInputWidth  = Utils::ClampUINT32(optimalInputWidth,  minInputWidth,  maxInputWidth);
        optimalInputHeight = Utils::ClampUINT32(optimalInputHeight, minInputHeight, maxInputHeight);

        pBufferNegotiationData->numInputPorts = totalInputPorts;

        for (UINT input = 0; input < totalInputPorts; input++)
        {
            UINT currentInputPortSourceTypeId = GetInputPortSourceType(input);
            if ((PortSrcTypeUndefined != currentInputPortSourceTypeId) &&
                (PortSrcTypePixel     != currentInputPortSourceTypeId))
            {
                CAMX_LOG_INFO(CamxLogGroupISP, "Skip input buffer options for port source type of non pixel.")
                continue;
            }

            pBufferNegotiationData->inputBufferOptions[input].nodeId     = Type();
            pBufferNegotiationData->inputBufferOptions[input].instanceId = InstanceID();
            pBufferNegotiationData->inputBufferOptions[input].portId     = inputPortId[input];

            BufferRequirement* pInputBufferRequirement = &pBufferNegotiationData->inputBufferOptions[input].bufferRequirement;

            pInputBufferRequirement->optimalWidth  = optimalInputWidth;
            pInputBufferRequirement->optimalHeight = optimalInputHeight;
            // If IPE is enabling SIMO and if one of the output is smaller than the other, then the scale capabilities (min,max)
            // needs to be adjusted after accounting for the scaling needed on the smaller output port.
            pInputBufferRequirement->minWidth      = minInputWidth;
            pInputBufferRequirement->minHeight     = minInputHeight;
            pInputBufferRequirement->maxWidth      = maxInputWidth;
            pInputBufferRequirement->maxHeight     = maxInputHeight;

            CAMX_LOG_VERBOSE(CamxLogGroupISP,
                             "Buffer Negotiation dims, Port %d Optimal %d x %d, Min %d x %d, Max %d x %d\n",
                             inputPortId[input],
                             optimalInputWidth,
                             optimalInputHeight,
                             minInputWidth,
                             minInputHeight,
                             maxInputWidth,
                             maxInputHeight);
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::ExtractCAMIFDecimatedPattern
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IFENode::ExtractCAMIFDecimatedPattern(
    UINT32             horizontalOffset,
    UINT32             verticalOffset,
    PDLibBlockPattern* pBlockPattern)
{
    IFECAMIFPixelExtractionData newPdafPixelcoordinates[IFEMaxPdafPixelsPerBlock];
    IFECAMIFPixelExtractionData subPdafPixelcoordinates[IFEMaxPdafPixelsPerBlock];
    UINT32                      pdafPixelCount = 0;
    UINT16                      pixelSkip = 0;
    UINT16                      lineSkip  = 0;
    UINT32                      subBoxWidth = 0;
    UINT32                      subBoxHeight = 0;
    UINT32                      offsetSumX = 0;
    UINT32                      offsetSumY = 0;
    UINT32                      offsetBlockX = 0;
    UINT32                      offsetBlockY = 0;
    UINT32                      offsetExtractX = 0;
    UINT32                      offsetExtractY = 0;
    UINT32                      constX = 0;
    UINT32                      constY = 0;

    pdafPixelCount = m_ISPInputSensorData.sensorPDAFInfo.PDAFPixelCount;

    // CAMIF pixel and line skip operate on 32 pixels and 32 lines at a time
    // Hence 32 x 32 block is considered when subsampiling enabled

    constX = m_ISPInputSensorData.sensorPDAFInfo.PDAFBlockWidth / 16;
    constY = m_ISPInputSensorData.sensorPDAFInfo.PDAFBlockHeight / 16;

    for (UINT32 index = 0; index < pdafPixelCount; index++)
    {
        newPdafPixelcoordinates[index].xCordrinate =
            (m_ISPInputSensorData.sensorPDAFInfo.PDAFPixelCoords[index].PDXCoordinate) %
            m_ISPInputSensorData.sensorPDAFInfo.PDAFBlockWidth;
        newPdafPixelcoordinates[index].yCordrinate =
            (m_ISPInputSensorData.sensorPDAFInfo.PDAFPixelCoords[index].PDYCoordinate) %
            m_ISPInputSensorData.sensorPDAFInfo.PDAFBlockHeight;

        // Index of ecah 32x32 block
        newPdafPixelcoordinates[index].blockX = newPdafPixelcoordinates[index].xCordrinate / 16;
        newPdafPixelcoordinates[index].blockY = newPdafPixelcoordinates[index].yCordrinate / 16;

        // Mod by 32 to fit CAMIF subsampling pattern
        newPdafPixelcoordinates[index].xCordrinate = newPdafPixelcoordinates[index].xCordrinate % 16;
        newPdafPixelcoordinates[index].yCordrinate = newPdafPixelcoordinates[index].yCordrinate % 16;

        lineSkip |= ((static_cast<UINT16>(1)) << (15 - newPdafPixelcoordinates[index].yCordrinate));
        pixelSkip |= ((static_cast<UINT16>(1)) << (15 - newPdafPixelcoordinates[index].xCordrinate));
    }

    /* Sub Sampled Block Size*/
    subBoxWidth  = GetPixelsInSkipPattern(pixelSkip);
    subBoxHeight = GetPixelsInSkipPattern(lineSkip);

    for (UINT32 index = 0; index < pdafPixelCount; index++)
    {
        subPdafPixelcoordinates[index].xCordrinate =
            GetPixelsInSkipPattern(pixelSkip >> (15 - newPdafPixelcoordinates[index].xCordrinate)) - 1;
        subPdafPixelcoordinates[index].xCordrinate +=
            newPdafPixelcoordinates[index].blockX * subBoxWidth;
        subPdafPixelcoordinates[index].yCordrinate =
            GetPixelsInSkipPattern(lineSkip >> (15 - newPdafPixelcoordinates[index].yCordrinate)) - 1;
        subPdafPixelcoordinates[index].yCordrinate +=
            newPdafPixelcoordinates[index].blockY * subBoxHeight;
    }

    offsetBlockX = (horizontalOffset / 16) * subBoxWidth;
    offsetBlockY = (verticalOffset / 16) * subBoxHeight;

    if ((horizontalOffset % 16) > 0)
    {
        offsetExtractX = GetPixelsInSkipPattern(pixelSkip >> (16 - (horizontalOffset % 16)));
    }

    if ((verticalOffset % 16) > 0)
    {
        offsetExtractY = GetPixelsInSkipPattern(lineSkip >> (16 - (verticalOffset % 16)));
    }

    offsetSumX = offsetBlockX + offsetExtractX;
    offsetSumY = offsetBlockY + offsetExtractY;

    pBlockPattern->horizontalPDOffset = offsetSumX;
    pBlockPattern->verticalPDOffset   = offsetSumY;
    pBlockPattern->blockDimension.width = subBoxWidth * constX;
    pBlockPattern->blockDimension.height = subBoxHeight * constY;
    pBlockPattern->pixelCount = pdafPixelCount;

    for (UINT index = 0; index < pdafPixelCount; index++)
    {
        pBlockPattern->pixelCoordinate[index].x =
            (subPdafPixelcoordinates[index].xCordrinate - offsetSumX + (1024 * subBoxWidth * constX)) %
            (subBoxWidth * constX);
        pBlockPattern->pixelCoordinate[index].y =
            (subPdafPixelcoordinates[index].yCordrinate - offsetSumY + (1024 * subBoxHeight * constY)) %
            (subBoxHeight * constY);
    }

}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::PreparePDAFInformation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IFENode::PreparePDAFInformation()
{
    UINT16 pixelSkipPattern = 0;
    UINT16 lineSkipPattern = 0;
    UINT32 numberOfPixels = 0;
    UINT32 numberOfLines = 0;
    UINT32 pdafPixelCount = m_ISPInputSensorData.sensorPDAFInfo.PDAFPixelCount;
    PDLibDataBufferInfo PDAFBufferInfo;

    Utils::Memset(&PDAFBufferInfo, 0, sizeof(PDLibDataBufferInfo));

    PDAFBufferInfo.imageOverlap = 0;
    PDAFBufferInfo.bufferFormat = PDLibBufferFormat::PDLibBufferFormatUnpacked16;
    PDAFBufferInfo.sensorType   = PDLibSensorType::PDLibSensorType3;
    PDAFBufferInfo.isp1BufferWidth = m_PDAFInfo.bufferWidth;
    PDAFBufferInfo.isp1BufferStride = m_PDAFInfo.bufferWidth * 2;
    PDAFBufferInfo.ispBufferHeight = m_PDAFInfo.bufferHeight;

    ExtractCAMIFDecimatedPattern(m_ISPInputSensorData.sensorPDAFInfo.PDAFGlobaloffsetX,
                                 m_ISPInputSensorData.sensorPDAFInfo.PDAFGlobaloffsetY,
                                 &PDAFBufferInfo.isp1BlockPattern);
    // Publish IFE PDAF Buffer Information
    const UINT  outputTags[] = { PropertyIDUsecaseIFEPDAFInfo };
    const VOID* pOutputData[1] = { 0 };
    UINT        pDataCount[1] = { 0 };
    pDataCount[0] = sizeof(PDLibDataBufferInfo);
    pOutputData[0] = &PDAFBufferInfo;
    WriteDataList(outputTags, pOutputData, pDataCount, 1);
};




////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::FinalizeBufferProperties
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IFENode::FinalizeBufferProperties(
    BufferNegotiationData* pBufferNegotiationData)
{
    UINT32 finalSelectedOutputWidth    = 0;
    UINT32 finalSelectedOutputHeight   = 0;

    UINT32 fullPortOutputWidth         = 0;
    UINT32 fullPortOutputHeight        = 0;
    UINT32 displayFullPortOutputWidth  = 0;
    UINT32 displayFullPortOutputHeight = 0;

    UINT16 pixelSkipPattern            = 0;
    UINT16 lineSkipPattern             = 0;
    UINT32 numberOfPixels              = 0;
    UINT32 numberOfLines               = 0;
    UINT32 pdafPixelCount              = 0;
    UINT32 residualWidth               = 0;
    UINT32 residualHeight              = 0;
    UINT16 residualWidthPattern        = 0;
    UINT16 residualHeightPattern       = 0;
    UINT16 offsetX                     = 0;
    UINT16 offsetY                     = 0;
    FLOAT  perOutputPortAspectRatio    = 0.0f;
    FLOAT  inputSensorAspectRatio      = 0.0f;
    FLOAT  curStreamAspectRatio        = 0.0f;
    UINT32 perOutputPortOptimalWidth   = 0;
    UINT32 perOutputPortOptimalHeight  = 0;
    UINT32 perOutputPortMinWidth       = 0;
    UINT32 perOutputPortMinHeight      = 0;
    UINT32 perOutputPortMaxWidth       = IFEMaxOutputWidthFull * 2;  // Since for DUAL IFE , Max width port width * 2
    UINT32 perOutputPortMaxHeight      = IFEMaxOutputHeight;

    const StaticSettings* pStaticSettings = HwEnvironment::GetInstance()->GetStaticSettings();

    /// @todo (CAMX-561) Handle sensor mode changes per request. May need to keep an array of m_pSensorModeData[..]
    /// @todo (CAMX-1295) - Need to figure out the proper way to deal with TPG/SensorEmulation + RealDevice/Presil
    if ((CSLPresilEnabled == GetCSLMode()) || (CSLPresilRUMIEnabled == GetCSLMode()) || (TRUE == IsTPGMode()))
    {
        m_pSensorModeData = &m_testGenModeData;
        m_pSensorModeRes0Data = &m_testGenModeData;
    }
    else
    {
        if (NULL == m_pSensorModeData)
        {
            GetSensorModeData(&m_pSensorModeData);
        }
    }
    if (NULL == m_pOTPData)
    {
        if (!((CSLPresilEnabled == GetCSLMode()) || (CSLPresilRUMIEnabled == GetCSLMode())))
        {
            if (NULL != m_pSensorCaps)
            {
                m_pOTPData = &m_pSensorCaps->OTPData;
            }
            else
            {
                CAMX_LOG_ERROR(CamxLogGroupISP, "Sensor static capabilities not available");
            }
        }
    }

    // Gets the PDAF Information from Sensor
    GetPDAFInformation();


    // The input can be from the sensor or a test pattern generator (CAMIF TPG or CSID TPG)
    UINT32 finalSelectedInputWidth  = m_pSensorModeData->resolution.outputWidth;
    UINT32 finalSelectedInputHeight = m_pSensorModeData->resolution.outputHeight;

    if (0 != finalSelectedInputHeight)
    {
        inputSensorAspectRatio = static_cast<FLOAT>(finalSelectedInputWidth) /
            static_cast<FLOAT>(finalSelectedInputHeight);
    }

    // CSID crop override
    if (TRUE == EnableCSIDCropOverridingForSingleIFE())
    {
        finalSelectedInputWidth  = m_instanceProperty.IFECSIDWidth;
        if (TRUE == IsSensorModeFormatYUV(m_pSensorModeData->format))
        {
            finalSelectedInputWidth >>= 1;
        }
        finalSelectedInputHeight = m_instanceProperty.IFECSIDHeight;

        CAMX_LOG_INFO(CamxLogGroupISP, "CSID crop override: finalSelectedInputWidth = %d, finalSelectedInputHeight = %d",
                      finalSelectedInputWidth, finalSelectedInputHeight);
    }

    CAMX_LOG_INFO(CamxLogGroupISP, "finalSelectedInputWidth = %d, finalSelectedInputHeight = %d",
                  finalSelectedInputWidth, finalSelectedInputHeight);

    for (UINT index = 0; index < pBufferNegotiationData->numOutputPortsNotified; index++)
    {
        OutputPortNegotiationData* pOutputPortNegotiationData = &pBufferNegotiationData->pOutputPortNegotiationData[index];
        UINT                       outputPortIndex =
            pBufferNegotiationData->pOutputPortNegotiationData[index].outputPortIndex;
        UINT                       outputPortId = GetOutputPortId(outputPortIndex);
        FLOAT newOutputPortAspectRatio = 0.0f;

        if ((FALSE == IsStatsOutputPort(outputPortId)) &&
             (TRUE == IsPixelOutputPortSourceType(outputPortId)))
        {
            perOutputPortOptimalWidth  = 0;
            perOutputPortOptimalHeight = 0;
            perOutputPortMinWidth      = 0;
            perOutputPortMinHeight     = 0;
            perOutputPortMaxWidth      = IFEMaxOutputWidthFull * 2;
            perOutputPortMaxHeight     = IFEMaxOutputHeight;
            perOutputPortAspectRatio   = 0.0f;


            // FD port has a different limit than the full port.
            if (IFEOutputPortFD == outputPortId)
            {
                perOutputPortMaxWidth = IFEMaxOutputWidthFD * 2;
            }

            if (TRUE == m_RDIOnlyUseCase)
            {
                CAMX_LOG_INFO(CamxLogGroupISP, "ignore Single IFE output limitatation for RDI only usecase");
                perOutputPortMaxWidth  = IFEMAXOutputWidthRDIOnly;
                perOutputPortMaxHeight = IFEMAXOutputHeightRDIOnly;
            }

            // Go through the requirements of the destination ports connected to a given output port of IFE
            for (UINT inputIndex = 0; inputIndex < pOutputPortNegotiationData->numInputPortsNotification; inputIndex++)
            {
                BufferRequirement* pInputPortRequirement = &pOutputPortNegotiationData->inputPortRequirement[inputIndex];

                // Optimal width per port is the super resolution of all the connected destination ports' optimal needs.
                perOutputPortOptimalWidth =
                    Utils::MaxUINT32(pInputPortRequirement->optimalWidth, perOutputPortOptimalWidth);
                perOutputPortOptimalHeight =
                    Utils::MaxUINT32(pInputPortRequirement->optimalHeight, perOutputPortOptimalHeight);

                if (FALSE == Utils::FEqual(pInputPortRequirement->optimalHeight, 0.0f))
                {
                    curStreamAspectRatio = static_cast<FLOAT>(pInputPortRequirement->optimalWidth) /
                        static_cast<FLOAT>(pInputPortRequirement->optimalHeight);
                }
                else
                {
                    CAMX_LOG_INFO(CamxLogGroupISP, "pInputPortRequirement->optimalHeightis 0");
                }
                // Get new OutputPortAspectRatio because perOutputPortOptimalWidth/Height may be modified,
                // this may not be standard ratio, just for further checking
                if (FALSE == Utils::FEqual(perOutputPortOptimalHeight, 0.0f))
                {
                    newOutputPortAspectRatio = static_cast<FLOAT>(perOutputPortOptimalWidth) /
                         static_cast<FLOAT>(perOutputPortOptimalHeight);
                }
                else
                {
                    CAMX_LOG_INFO(CamxLogGroupISP, "perOutputPortOptimalHeight is 0");
                }

                if ((perOutputPortAspectRatio != 0) &&
                    (perOutputPortAspectRatio != curStreamAspectRatio || perOutputPortAspectRatio != newOutputPortAspectRatio))
                {
                    FLOAT adjustAspectRatio = perOutputPortAspectRatio > curStreamAspectRatio ?
                        curStreamAspectRatio : perOutputPortAspectRatio;
                    adjustAspectRatio = inputSensorAspectRatio > adjustAspectRatio ?
                         inputSensorAspectRatio : adjustAspectRatio;

                    if (TRUE == Utils::FEqualCoarse(adjustAspectRatio, newOutputPortAspectRatio))
                    {
                        // The dimensions are fine. Do nothing
                    }
                    else if (adjustAspectRatio > newOutputPortAspectRatio)
                    {
                        perOutputPortOptimalWidth =
                            Utils::EvenFloorUINT32(static_cast<UINT32>(perOutputPortOptimalHeight * adjustAspectRatio));
                        CAMX_LOG_INFO(CamxLogGroupISP, "NonConformant AspectRatio:%f Change Width %d using AR:%f",
                            newOutputPortAspectRatio, perOutputPortOptimalWidth, adjustAspectRatio);
                    }
                    else
                    {
                        perOutputPortOptimalHeight = Utils::EvenFloorUINT32(
                            static_cast<UINT32>(perOutputPortOptimalWidth / adjustAspectRatio));
                        CAMX_LOG_INFO(CamxLogGroupISP, "NonConformant AspectRatio:%f Change Height %d using AR:%f",
                            newOutputPortAspectRatio, perOutputPortOptimalHeight, adjustAspectRatio);
                    }
                }

                if (FALSE == Utils::FEqual(perOutputPortOptimalHeight, 0.0f))
                {
                    perOutputPortAspectRatio = static_cast<FLOAT>(perOutputPortOptimalWidth) /
                        static_cast<FLOAT>(perOutputPortOptimalHeight);
                }

                perOutputPortMinWidth  =
                    Utils::MaxUINT32(pInputPortRequirement->minWidth, perOutputPortMinWidth);
                perOutputPortMinHeight =
                    Utils::MaxUINT32(pInputPortRequirement->minHeight, perOutputPortMinHeight);
                perOutputPortMaxWidth  =
                    Utils::MinUINT32(pInputPortRequirement->maxWidth, perOutputPortMaxWidth);
                perOutputPortMaxHeight =
                    Utils::MinUINT32(pInputPortRequirement->maxHeight, perOutputPortMaxHeight);
            }

            // Ensure optimal dimensions are within min and max dimensions. There are chances that the optimal dimension goes
            // over the max. Correct for the same.
            perOutputPortOptimalWidth =
                Utils::ClampUINT32(perOutputPortOptimalWidth, perOutputPortMinWidth, perOutputPortMaxWidth);
            perOutputPortOptimalHeight =
                Utils::ClampUINT32(perOutputPortOptimalHeight, perOutputPortMinHeight, perOutputPortMaxHeight);

            // Store the buffer requirements for this output port which will be reused to set, during forward walk.
            // The values stored here could be final output dimensions unless it is overridden by forward walk.
            pOutputPortNegotiationData->outputBufferRequirementOptions.optimalWidth = perOutputPortOptimalWidth;
            pOutputPortNegotiationData->outputBufferRequirementOptions.optimalHeight = perOutputPortOptimalHeight;
        }
    }

    // Find Full port dimensions, for DS port dimension determination
    for (UINT index = 0; index < pBufferNegotiationData->numOutputPortsNotified; index++)
    {
        OutputPortNegotiationData* pOutputPortData = &pBufferNegotiationData->pOutputPortNegotiationData[index];
        UINT                       outputPortId    = GetOutputPortId(pOutputPortData->outputPortIndex);

        if (IFEOutputPortFull == outputPortId)
        {
            fullPortOutputWidth  = pOutputPortData->outputBufferRequirementOptions.optimalWidth;
            fullPortOutputHeight = pOutputPortData->outputBufferRequirementOptions.optimalHeight;

            if (TRUE == pStaticSettings->capResolutionForSingleIFE)
            {
                if (fullPortOutputWidth > (IFEMaxOutputWidthFull))
                {
                    fullPortOutputWidth = IFEMaxOutputWidthFull;
                }
            }

            CAMX_LOG_VERBOSE(CamxLogGroupISP, " Found full port on index %d dims %dx%d",
                                 index, fullPortOutputWidth, fullPortOutputHeight);

            /// Clip if output greater than input since IFE cannot do upscale.
            if (fullPortOutputWidth > finalSelectedInputWidth)
            {
                fullPortOutputWidth = finalSelectedInputWidth;

                CAMX_LOG_INFO(CamxLogGroupISP, "outputPortId = %d, Output buffer clipped width = %d",
                    outputPortId, fullPortOutputWidth);
            }

            /// Clip if output greater than input, clamp since IFE cannot do upscale.
            if (fullPortOutputHeight > finalSelectedInputHeight)
            {
                fullPortOutputHeight = finalSelectedInputHeight;

                CAMX_LOG_INFO(CamxLogGroupISP, "outputPortId = %d, Output buffer clipped height = %d",
                    outputPortId, fullPortOutputHeight);
            }

            /// If downscale ratio is beyond ife limits, cap the output dimension.
            if ((finalSelectedInputWidth / fullPortOutputWidth) > IFEMaxDownscaleLimt)
            {
                fullPortOutputWidth = static_cast<UINT32>(finalSelectedInputWidth * IFEMaxDownscaleLimt);
                CAMX_LOG_ERROR(CamxLogGroupISP, "Scaleratio beyond limit, input height = %u, output height = %u",
                    finalSelectedInputWidth, fullPortOutputWidth);
            }
            if (finalSelectedInputHeight / fullPortOutputHeight > IFEMaxDownscaleLimt)
            {
                fullPortOutputHeight = static_cast<UINT32>(finalSelectedInputHeight * IFEMaxDownscaleLimt);
                CAMX_LOG_ERROR(CamxLogGroupISP, "Scaleratio beyond limit, input height = %u, output height = %u",
                    finalSelectedInputHeight, fullPortOutputHeight);
            }

            CAMX_LOG_VERBOSE(CamxLogGroupISP, "Found full port on index %d adj dims %dx%d",
                                 index, fullPortOutputWidth, fullPortOutputHeight);
        }

        if (IFEOutputPortDisplayFull == outputPortId)
        {
            displayFullPortOutputWidth  = pOutputPortData->outputBufferRequirementOptions.optimalWidth;
            displayFullPortOutputHeight = pOutputPortData->outputBufferRequirementOptions.optimalHeight;

            if (TRUE == pStaticSettings->capResolutionForSingleIFE)
            {
                if (displayFullPortOutputWidth > (IFEMaxOutputWidthFull))
                {
                    displayFullPortOutputWidth = IFEMaxOutputWidthFull;
                }
            }

            CAMX_LOG_VERBOSE(CamxLogGroupISP, "Found display full port on index %d dims %dx%d",
                                 index, displayFullPortOutputWidth, displayFullPortOutputHeight);

            /// Clip if output greater than input since IFE cannot do upscale.
            if (displayFullPortOutputWidth > finalSelectedInputWidth)
            {
                displayFullPortOutputWidth = finalSelectedInputWidth;

                CAMX_LOG_INFO(CamxLogGroupISP, "outputPortId = %d, Output buffer clipped width = %d",
                    outputPortId, displayFullPortOutputWidth);
            }

            /// Clip if output greater than input, clamp since IFE cannot do upscale.
            if (displayFullPortOutputHeight > finalSelectedInputHeight)
            {
                displayFullPortOutputHeight = finalSelectedInputHeight;

                CAMX_LOG_INFO(CamxLogGroupISP, "outputPortId = %d, Output buffer clipped height = %d",
                    outputPortId, displayFullPortOutputHeight);
            }

            /// If downscale ratio is beyond ife limits, cap the output dimension.
            if ((finalSelectedInputWidth / displayFullPortOutputWidth) > IFEMaxDownscaleLimt)
            {
                displayFullPortOutputWidth = static_cast<UINT32>(finalSelectedInputWidth * IFEMaxDownscaleLimt);
                CAMX_LOG_ERROR(CamxLogGroupISP, "Scaleratio beyond limit, input height = %u, output height = %u",
                    finalSelectedInputWidth, displayFullPortOutputWidth);
            }
            if (finalSelectedInputHeight / displayFullPortOutputHeight > IFEMaxDownscaleLimt)
            {
                displayFullPortOutputHeight = static_cast<UINT32>(finalSelectedInputHeight * IFEMaxDownscaleLimt);
                CAMX_LOG_ERROR(CamxLogGroupISP, "Scaleratio beyond limit, input height = %u, output height = %u",
                    finalSelectedInputHeight, displayFullPortOutputHeight);
            }

            CAMX_LOG_VERBOSE(CamxLogGroupISP, "Found display full port on index %d adj dims %dx%d",
                                 index, displayFullPortOutputWidth, displayFullPortOutputHeight);
        }
    }

    for (UINT index = 0; index < pBufferNegotiationData->numOutputPortsNotified; index++)
    {
        OutputPortNegotiationData* pOutputPortNegotiationData = &pBufferNegotiationData->pOutputPortNegotiationData[index];
        BufferProperties*          pOutputBufferProperties    = pOutputPortNegotiationData->pFinalOutputBufferProperties;
        UINT                       outputPortId               = GetOutputPortId(pOutputPortNegotiationData->outputPortIndex);

        // Assume only singleIFE, cap output buffer to IFE limitation
        if (TRUE == pStaticSettings->capResolutionForSingleIFE)
        {
            if (pOutputPortNegotiationData->outputBufferRequirementOptions.optimalWidth > (IFEMaxOutputWidthFull))
            {
                pOutputPortNegotiationData->outputBufferRequirementOptions.optimalWidth = IFEMaxOutputWidthFull;
            }
        }
        if ((IFEOutputPortFD == outputPortId) &&
            (pOutputPortNegotiationData->outputBufferRequirementOptions.optimalWidth > (m_maxOutputWidthFD)))
        {
            pOutputPortNegotiationData->outputBufferRequirementOptions.optimalWidth = m_maxOutputWidthFD;
        }
        if ((IFEOutputPortFD == outputPortId) &&
            (pOutputPortNegotiationData->outputBufferRequirementOptions.optimalHeight > (m_maxOutputHeightFD)))
        {
            pOutputPortNegotiationData->outputBufferRequirementOptions.optimalHeight  = m_maxOutputHeightFD;
        }
        finalSelectedOutputWidth  = pOutputPortNegotiationData->outputBufferRequirementOptions.optimalWidth;
        finalSelectedOutputHeight = pOutputPortNegotiationData->outputBufferRequirementOptions.optimalHeight;

        CAMX_LOG_INFO(CamxLogGroupISP, "outputPortId = %d, Output buffer requirement options: optimalWidth = %d",
            outputPortId, finalSelectedOutputWidth);
        CAMX_LOG_INFO(CamxLogGroupISP, "outputPortId = %d, Output buffer requirement options: optimalHeight = %d",
            outputPortId, finalSelectedOutputHeight);

        // If clipping needed, apply only for non-stats type output ports.
        if ((FALSE == IsStatsOutputPort(outputPortId)) &&
            (TRUE  == IsPixelOutputPortSourceType(outputPortId)))
        {
            /// Clip if output greater than input since IFE cannot do upscale.
            if (finalSelectedOutputWidth > finalSelectedInputWidth)
            {
                finalSelectedOutputWidth = finalSelectedInputWidth;

                CAMX_LOG_INFO(CamxLogGroupISP, "outputPortId = %d, Output buffer clipped width = %d",
                    outputPortId, finalSelectedOutputWidth);
            }

            /// Clip if output greater than input, clamp since IFE cannot do upscale.
            if (finalSelectedOutputHeight > finalSelectedInputHeight)
            {
                finalSelectedOutputHeight = finalSelectedInputHeight;

                CAMX_LOG_INFO(CamxLogGroupISP, "outputPortId = %d, Output buffer clipped height = %d",
                    outputPortId, finalSelectedOutputHeight);
            }
        }

        // Dont apply scale limits for nonstats ports
        if ((FALSE == IsStatsOutputPort(outputPortId)) &&
            (FALSE == IsDSOutputPort(outputPortId)) &&
            (TRUE  == IsPixelOutputPortSourceType(outputPortId)))
        {
            /// If downscale ratio is beyond ife limits, cap the output dimension.
            if (0 != finalSelectedOutputWidth)
            {
                if ((finalSelectedInputWidth / finalSelectedOutputWidth) > IFEMaxDownscaleLimt)
                {
                    finalSelectedOutputWidth = static_cast<UINT32>(finalSelectedInputWidth * IFEMaxDownscaleLimt);
                    CAMX_LOG_WARN(CamxLogGroupISP, "Scaleratio beyond limit, inp height = %d, out height = %d",
                        finalSelectedInputWidth, finalSelectedOutputWidth);
                }
            }
            if (0 != finalSelectedOutputHeight)
            {
                if (finalSelectedInputHeight / finalSelectedOutputHeight > IFEMaxDownscaleLimt)
                {
                    finalSelectedOutputHeight = static_cast<UINT32>(finalSelectedInputHeight * IFEMaxDownscaleLimt);
                    CAMX_LOG_WARN(CamxLogGroupISP, "Scaleratio beyond limit, inp height = %d, out height = %d",
                        finalSelectedInputHeight, finalSelectedOutputHeight);
                }
            }
        }
        UINT outputPortSourceTypeId = GetOutputPortSourceType(pOutputPortNegotiationData->outputPortIndex);
        BOOL isSinkPortWithBuffer = IsSinkPortWithBuffer(pOutputPortNegotiationData->outputPortIndex);

        if (FALSE == isSinkPortWithBuffer)
        {
            switch (outputPortId)
            {
                case IFEOutputPortFull:
                case IFEOutputPortDisplayFull:
                case IFEOutputPortFD:
                    pOutputBufferProperties->imageFormat.width  = finalSelectedOutputWidth;
                    pOutputBufferProperties->imageFormat.height = finalSelectedOutputHeight;

                    Utils::Memcpy(&pOutputBufferProperties->imageFormat.planeAlignment[0],
                                  &pOutputPortNegotiationData->outputBufferRequirementOptions.planeAlignment[0],
                                  sizeof(AlignmentInfo) * FormatsMaxPlanes);
                    break;

                case IFEOutputPortDS4:
                    pOutputBufferProperties->imageFormat.width =
                        Utils::EvenCeilingUINT32(
                            Utils::AlignGeneric32(fullPortOutputWidth, DS4Factor) / DS4Factor);
                    pOutputBufferProperties->imageFormat.height =
                        Utils::EvenCeilingUINT32(
                            Utils::AlignGeneric32(fullPortOutputHeight, DS4Factor) / DS4Factor);
                    break;

                case IFEOutputPortDS16:
                    pOutputBufferProperties->imageFormat.width =
                        Utils::EvenCeilingUINT32(
                            Utils::AlignGeneric32(fullPortOutputWidth, DS16Factor) / DS16Factor);
                    pOutputBufferProperties->imageFormat.height =
                        Utils::EvenCeilingUINT32(
                            Utils::AlignGeneric32(fullPortOutputHeight, DS16Factor) / DS16Factor);
                    break;

                case IFEOutputPortDisplayDS4:
                    pOutputBufferProperties->imageFormat.width =
                        Utils::EvenCeilingUINT32(
                            Utils::AlignGeneric32(displayFullPortOutputWidth, DS4Factor) / DS4Factor);
                    pOutputBufferProperties->imageFormat.height =
                        Utils::EvenCeilingUINT32(
                            Utils::AlignGeneric32(displayFullPortOutputHeight, DS4Factor) / DS4Factor);
                    break;

                case IFEOutputPortDisplayDS16:
                    pOutputBufferProperties->imageFormat.width =
                        Utils::EvenCeilingUINT32(
                            Utils::AlignGeneric32(displayFullPortOutputWidth, DS16Factor) / DS16Factor);
                    pOutputBufferProperties->imageFormat.height =
                        Utils::EvenCeilingUINT32(
                            Utils::AlignGeneric32(displayFullPortOutputHeight, DS16Factor) / DS16Factor);
                    break;

                /// @note * 2 below for stats is for handling dual IFE (conservatively). This can be optimized.
                case IFEOutputPortStatsIHIST:
                    pOutputBufferProperties->imageFormat.width  = IHistStatsWidth * 2;
                    pOutputBufferProperties->imageFormat.height = IHistStatsHeight;
                    break;

                case IFEOutputPortStatsHDRBE:
                    pOutputBufferProperties->imageFormat.width  = HDRBEStatsMaxWidth * 2;
                    pOutputBufferProperties->imageFormat.height = HDRBEStatsMaxHeight;
                    break;

                case IFEOutputPortStatsHDRBHIST:
                    pOutputBufferProperties->imageFormat.width  = HDRBHistStatsMaxWidth * 2;
                    pOutputBufferProperties->imageFormat.height = HDRBHistStatsMaxHeight;
                    break;

                case IFEOutputPortStatsAWBBG:
                    pOutputBufferProperties->imageFormat.width  = AWBBGStatsMaxWidth * 2;
                    pOutputBufferProperties->imageFormat.height = AWBBGStatsMaxHeight;
                    break;

                case IFEOutputPortStatsTLBG:
                    pOutputBufferProperties->imageFormat.width  = TintlessBGStatsWidth * 2;
                    pOutputBufferProperties->imageFormat.height = TintlessBGStatsHeight;
                    break;

                case IFEOutputPortStatsBHIST:
                    pOutputBufferProperties->imageFormat.width  = BHistStatsWidth * 2;
                    pOutputBufferProperties->imageFormat.height = 1;
                    break;

                case IFEOutputPortStatsRS:
                    pOutputBufferProperties->imageFormat.width  = RSStatsWidth * 2;
                    pOutputBufferProperties->imageFormat.height = RSStatsHeight;
                    break;

                case IFEOutputPortStatsCS:
                    pOutputBufferProperties->imageFormat.width  = CSStatsWidth * 2;
                    pOutputBufferProperties->imageFormat.height = CSStatsHeight;
                    break;

                case IFEOutputPortStatsBF:
                    pOutputBufferProperties->imageFormat.width  = BFStatsMaxWidth * 2;
                    pOutputBufferProperties->imageFormat.height = BFStatsMaxHeight;
                    break;

                // For RDI output port cases, set the output buffer dimension to the IFE input buffer dimension.
                case IFEOutputPortRDI0:
                case IFEOutputPortRDI1:
                case IFEOutputPortRDI2:
                case IFEOutputPortRDI3:
                case IFEOutputPortStatsDualPD:
                    pOutputBufferProperties->imageFormat.width  = finalSelectedInputWidth;
                    pOutputBufferProperties->imageFormat.height = finalSelectedInputHeight;

                    // Overwrite if the RDI port is associated with PDAF port source type.
                    if (PortSrcTypePDAF == outputPortSourceTypeId)
                    {
                        UINT streamIndex;
                        if (TRUE == FindSensorStreamConfigIndex(StreamType::PDAF, &streamIndex))
                        {
                            UINT32 PDAFWidth  = m_pSensorModeData->streamConfig[streamIndex].frameDimension.width;
                            UINT32 PDAFHeight = m_pSensorModeData->streamConfig[streamIndex].frameDimension.height;

                            // For type PDAF Type2 or 2PD SW-based type (i.e. dual PD) over RDI port case
                            if (Format::RawPlain16 == pOutputBufferProperties->imageFormat.format)
                            {

                                // Read PDAFBufferFormat from sensor PDAF Info
                                const PDBufferFormat sensorPDBufferFormat =
                                    m_ISPInputSensorData.sensorPDAFInfo.PDAFBufferFormat;

                                // Check NativeBufferFormat if Sensor is PDAF Type2/DualPD
                                if (PDLibSensorType2 ==
                                    static_cast<PDLibSensorType>(m_ISPInputSensorData.sensorPDAFInfo.PDAFSensorType) ||
                                    PDLibSensorDualPD ==
                                    static_cast<PDLibSensorType>(m_ISPInputSensorData.sensorPDAFInfo.PDAFSensorType))
                                {
                                    // Read PDAFNativeBufferFormat from sensor PDAF Info
                                    const PDBufferFormat sensorPDNativeBufferFormat =
                                        m_ISPInputSensorData.sensorPDAFInfo.PDAFNativeBufferFormat;

                                    switch (sensorPDNativeBufferFormat)
                                    {
                                        case PDBufferFormat::UNPACKED16:
                                            // Need to handle the padding case for the default blob sensor
                                            pOutputBufferProperties->imageFormat.width = PDAFWidth * PDAFHeight * 2;
                                            pOutputBufferProperties->imageFormat.height = 1;
                                            pOutputBufferProperties->imageFormat.format = Format::Blob;
                                            break;

                                        case PDBufferFormat::MIPI10:

                                            switch (sensorPDBufferFormat)
                                            {
                                                case PDBufferFormat::UNPACKED16:
                                                    pOutputBufferProperties->imageFormat.width = PDAFWidth;
                                                    pOutputBufferProperties->imageFormat.height = PDAFHeight;
                                                    pOutputBufferProperties->imageFormat.format = Format::RawPlain16;
                                                    break;
                                                default:
                                                    pOutputBufferProperties->imageFormat.width  = PDAFWidth;
                                                    pOutputBufferProperties->imageFormat.height = PDAFHeight;
                                                    break;
                                            }
                                            break;

                                        default:
                                            CAMX_LOG_ERROR(CamxLogGroupISP, "Unsupported PDNativeBufferFormat = %d",
                                                           sensorPDNativeBufferFormat);
                                            break;
                                    }
                                    CAMX_LOG_VERBOSE(CamxLogGroupISP, "PDNativeBufferFormat = %d", sensorPDNativeBufferFormat);
                                }
                                else
                                {
                                    switch (sensorPDBufferFormat)
                                    {
                                        case PDBufferFormat::UNPACKED16:
                                            // Need to handle the padding case for the default blob sensor
                                            pOutputBufferProperties->imageFormat.width = PDAFWidth * PDAFHeight * 2;
                                            pOutputBufferProperties->imageFormat.height = 1;
                                            pOutputBufferProperties->imageFormat.format = Format::Blob;
                                            break;

                                        case PDBufferFormat::MIPI10:
                                            pOutputBufferProperties->imageFormat.width  = PDAFWidth;
                                            pOutputBufferProperties->imageFormat.height = PDAFHeight;
                                            break;

                                        default:
                                            CAMX_LOG_ERROR(CamxLogGroupISP, "Unsupported PDBufferFormat = %d",
                                                           sensorPDBufferFormat);
                                            break;
                                    }
                                }
                                CAMX_LOG_VERBOSE(CamxLogGroupISP,
                                                 "PDAF buffer format = RawPlain16, PDBufferFormat = %d, "
                                                 "width = %u, height = %u",
                                                 sensorPDBufferFormat,
                                                 pOutputBufferProperties->imageFormat.width,
                                                 pOutputBufferProperties->imageFormat.height);
                            }
                            else if (Format::RawPlain64 == pOutputBufferProperties->imageFormat.format)
                            {
                                pOutputBufferProperties->imageFormat.width  = PDAFWidth;
                                pOutputBufferProperties->imageFormat.height = PDAFHeight;

                                CAMX_LOG_VERBOSE(CamxLogGroupISP,
                                                 "PDAF buffer format = RawPlain16, width = %u, height = %u",
                                                 pOutputBufferProperties->imageFormat.width,
                                                 pOutputBufferProperties->imageFormat.height);
                            }
                            else
                            {
                                // Unsupported PDAF format
                                pOutputBufferProperties->imageFormat.width  = 0;
                                pOutputBufferProperties->imageFormat.height = 0;

                                CAMX_LOG_ERROR(CamxLogGroupISP,
                                               "Unspported PDAF buffer format = %u",
                                               pOutputBufferProperties->imageFormat.format);
                            }
                        }
                        else
                        {
                            pOutputBufferProperties->imageFormat.width          = 0;
                            pOutputBufferProperties->imageFormat.height         = 0;
                            pOutputBufferProperties->immediateAllocImageBuffers = 0;
                            CAMX_LOG_INFO(CamxLogGroupISP, "PDAF stream not configured by sensor");

                        }
                    }
                    else if (PortSrcTypeMeta == outputPortSourceTypeId)
                    {
                        UINT streamIndex;
                        if (TRUE == FindSensorStreamConfigIndex(StreamType::META, &streamIndex))
                        {
                            UINT32 metaWidth  = m_pSensorModeData->streamConfig[streamIndex].frameDimension.width;
                            UINT32 metaHeight = m_pSensorModeData->streamConfig[streamIndex].frameDimension.height;

                            if (Format::Blob == pOutputBufferProperties->imageFormat.format)
                            {
                                // The default blob sensor
                                pOutputBufferProperties->imageFormat.width  = metaWidth * metaHeight;
                                pOutputBufferProperties->imageFormat.height = 1;

                                CAMX_LOG_VERBOSE(CamxLogGroupISP,
                                                 "Meta buffer format = Blob, width = %u, height = %u",
                                                 pOutputBufferProperties->imageFormat.width,
                                                 pOutputBufferProperties->imageFormat.height);
                            }
                            else
                            {
                                // Unsupported Meta buffer format
                                pOutputBufferProperties->imageFormat.width  = 0;
                                pOutputBufferProperties->imageFormat.height = 0;

                                CAMX_LOG_ERROR(CamxLogGroupISP,
                                               "Unspported meta buffer format = %u",
                                               pOutputBufferProperties->imageFormat.format);
                            }
                        }
                    }
                    else if (PortSrcTypeHDR == outputPortSourceTypeId)
                    {
                        UINT streamIndex;
                        if (TRUE == FindSensorStreamConfigIndex(StreamType::HDR, &streamIndex))
                        {
                            UINT32 HDRWidth  = m_pSensorModeData->streamConfig[streamIndex].frameDimension.width;
                            UINT32 HDRHeight = m_pSensorModeData->streamConfig[streamIndex].frameDimension.height;

                            if (Format::Blob == pOutputBufferProperties->imageFormat.format)
                            {
                                // The default blob sensor
                                pOutputBufferProperties->imageFormat.width  = HDRWidth * HDRHeight;
                                pOutputBufferProperties->imageFormat.height = 1;

                                CAMX_LOG_VERBOSE(CamxLogGroupISP,
                                                 "HDR buffer format = Blob, width = %u, height = %u",
                                                 pOutputBufferProperties->imageFormat.width,
                                                 pOutputBufferProperties->imageFormat.height);
                            }
                            else if (Format::RawMIPI == pOutputBufferProperties->imageFormat.format)
                            {
                                pOutputBufferProperties->imageFormat.width  = HDRWidth;
                                pOutputBufferProperties->imageFormat.height = HDRHeight;

                                CAMX_LOG_VERBOSE(CamxLogGroupISP,
                                    "HDR buffer format = RawMIPI, width = %u, height = %u",
                                    pOutputBufferProperties->imageFormat.width,
                                    pOutputBufferProperties->imageFormat.height);
                            }
                            else
                            {
                                // Unsupported Meta buffer format
                                pOutputBufferProperties->imageFormat.width  = 0;
                                pOutputBufferProperties->imageFormat.height = 0;

                                CAMX_LOG_ERROR(CamxLogGroupISP,
                                               "Unspported HDR buffer format = %u",
                                               pOutputBufferProperties->imageFormat.format);
                            }
                        }
                    }
                    break;

                case IFEOutputPortCAMIFRaw:
                case IFEOutputPortLSCRaw:
                    pOutputBufferProperties->imageFormat.width  = finalSelectedInputWidth;
                    pOutputBufferProperties->imageFormat.height = finalSelectedInputHeight;
                    break;

                case IFEOutputPortPDAF:
                    pdafPixelCount   = m_ISPInputSensorData.sensorPDAFInfo.PDAFPixelCount;


                    for (UINT coordinate = 0;
                         (coordinate < pdafPixelCount) && (coordinate < IFEMaxPdafPixelsPerBlock);
                         coordinate++)
                    {
                        pixelSkipPattern |=
                            (1 << (m_ISPInputSensorData.sensorPDAFInfo.PDAFPixelCoords[coordinate].PDXCoordinate % 16));
                        lineSkipPattern |=
                            (1 << (m_ISPInputSensorData.sensorPDAFInfo.PDAFPixelCoords[coordinate].PDYCoordinate % 16));
                    }

                    // Find number of pixels
                    numberOfPixels = GetPixelsInSkipPattern(pixelSkipPattern);
                    numberOfLines = GetPixelsInSkipPattern(lineSkipPattern);

                    offsetX = m_ISPInputSensorData.CAMIFCrop.firstPixel % 16;
                    offsetY = m_ISPInputSensorData.CAMIFCrop.firstLine % 16;

                    pixelSkipPattern = ((pixelSkipPattern << offsetX) | (pixelSkipPattern >> (16 - offsetX)));
                    lineSkipPattern  = ((lineSkipPattern << offsetY) | (lineSkipPattern >> (16 - offsetY)));

                    m_PDAFInfo.bufferWidth  = (m_pSensorModeData->resolution.outputWidth / 16) * numberOfPixels;
                    m_PDAFInfo.bufferHeight = (m_pSensorModeData->resolution.outputHeight / 16) * numberOfLines;

                    residualWidth  = m_pSensorModeData->resolution.outputWidth % 16;
                    residualHeight = m_pSensorModeData->resolution.outputHeight % 16;

                    if (0 != residualWidth)
                    {
                        residualWidthPattern = ((static_cast<UINT16>(~0)) >> (16 - residualWidth));
                    }

                    if (0 != residualHeight)
                    {
                        residualHeightPattern = ((static_cast<UINT16>(~0)) >> (16 - residualHeight));
                    }

                    m_PDAFInfo.bufferWidth  += GetPixelsInSkipPattern(pixelSkipPattern & residualWidthPattern);
                    m_PDAFInfo.bufferHeight += GetPixelsInSkipPattern(lineSkipPattern & residualHeightPattern);
                    m_PDAFInfo.pixelSkipPattern = pixelSkipPattern;
                    m_PDAFInfo.lineSkipPattern  = lineSkipPattern;
                    m_PDAFInfo.enableSubsample  = CheckIfPDAFType3Supported();
                    pOutputBufferProperties->imageFormat.width = m_PDAFInfo.bufferWidth;
                    pOutputBufferProperties->imageFormat.height = m_PDAFInfo.bufferHeight;

                    PreparePDAFInformation();

                    CAMX_LOG_VERBOSE(CamxLogGroupISP, "PDAF Buffer Width %d Height %d",
                        m_PDAFInfo.bufferWidth, m_PDAFInfo.bufferHeight);
                    break;

                default:
                    CAMX_LOG_ERROR(CamxLogGroupISP, "Unhandled output portID");
                    break;
            }
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::IsSensorModeFormatBayer
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL IFENode::IsSensorModeFormatBayer(
    PixelFormat format
    ) const
{
    BOOL isBayer = FALSE;

    if ((PixelFormat::BayerBGGR == format) ||
        (PixelFormat::BayerGBRG == format) ||
        (PixelFormat::BayerGRBG == format) ||
        (PixelFormat::BayerRGGB == format))
    {
        isBayer = TRUE;
    }

    return isBayer;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::IsSensorModeFormatMono
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL IFENode::IsSensorModeFormatMono(
    PixelFormat format
    ) const
{
    BOOL isMono = FALSE;

    if (PixelFormat::Monochrome == format ||
        PixelFormat::YUVFormatY == format)
    {
        isMono = TRUE;
    }

    return isMono;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IFENode::IsSensorModeFormatYUV
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL IFENode::IsSensorModeFormatYUV(
    PixelFormat format
    ) const
{
    BOOL isYUV = FALSE;

    if ((PixelFormat::YUVFormatUYVY == format) ||
        (PixelFormat::YUVFormatYUYV == format))
    {
        isYUV = TRUE;
    }

    return isYUV;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IFENode::IsTPGMode
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL IFENode::IsTPGMode()
{
    BOOL isTPG                        = FALSE;
    const StaticSettings*   pSettings = GetStaticSettings();
    if (NULL != pSettings)
    {
        isTPG = pSettings->enableTPG;
    }

    return isTPG;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// DualIFEUtils::EvaluateDualIFEMode
IFEModuleMode DualIFEUtils::EvaluateDualIFEMode(
    ISPInputData* pISPInputdata)
{
    CAMX_UNREFERENCED_PARAM(pISPInputdata);

    const StaticSettings* pSettings         = HwEnvironment::GetInstance()->GetStaticSettings();
    IFEModuleMode         mode              = IFEModuleMode::SingleIFENormal;
    const PlatformStaticCaps*   pStaticCaps = HwEnvironment::GetInstance()->GetPlatformStaticCaps();

    if (TRUE == pSettings->enableDualIFE)
    {
        if (TRUE == pSettings->forceDualIFEOn)
        {
            mode = IFEModuleMode::DualIFENormal;
        }
        else
        {
            UINT64 IFEDualClockThreshod = static_cast<Titan17xContext*>(pISPInputdata->pHwContext)->
                GetTitan17xSettingsManager()->GetTitan17xStaticSettings()->IFEDualClockThreshold;
            if (0xFFFFFFFF == IFEDualClockThreshod)
            {
                IFEDualClockThreshod = pStaticCaps->maxIFESVSClock;
            }
            if (pISPInputdata->isDualcamera)
            {
                if (pISPInputdata->minRequiredSingleIFEClock <= pStaticCaps->maxIFETURBOClock)
                {
                    mode = IFEModuleMode::SingleIFENormal;
                }
                else
                {
                    mode = IFEModuleMode::DualIFENormal;
                }
            }
            else if ((pISPInputdata->forceIFEflag == TRUE) &&
                (pISPInputdata->minRequiredSingleIFEClock <= pStaticCaps->maxIFETURBOClock))
            {
                mode = IFEModuleMode::SingleIFENormal;
            }
            else if (pISPInputdata->minRequiredSingleIFEClock > IFEDualClockThreshod)
            {
                mode = IFEModuleMode::DualIFENormal;
            }
            else if ((pISPInputdata->HALData.stream[FDOutput].width > pISPInputdata->maxOutputWidthFD) ||
                     (pISPInputdata->HALData.stream[FullOutput].width > IFEMaxOutputWidthFull)         ||
                     (pISPInputdata->HALData.stream[DisplayFullOutput].width > IFEMaxOutputWidthFull))
            {
                CAMX_LOG_INFO(CamxLogGroupISP, "forcing dual IFE due to width of FD port %d Full port %d displayFull port %d",
                    pISPInputdata->HALData.stream[FDOutput].width,
                    pISPInputdata->HALData.stream[FullOutput].width,
                    pISPInputdata->HALData.stream[DisplayFullOutput].width);
                mode = IFEModuleMode::DualIFENormal;
            }
        }
    }
    return mode;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// DualIFEUtils::FillCfgFromOneStripe
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void DualIFEUtils::FillCfgFromOneStripe(
    ISPInputData*       pISPInputdata,
    IFEStripeOutput*    pStripeOut,
    ISPStripeConfig*    pStripeConfig)
{
    CAMX_ASSERT(NULL != pISPInputdata);
    CAMX_ASSERT(NULL != pStripeOut);
    CAMX_ASSERT(NULL != pStripeConfig);

    if (1 == pStripeOut->edgeStripeLT)
    {
        CAMX_LOG_INFO(CamxLogGroupISP, "<DualIFE>/<scaler> left stripe");
    }
    else
    {
        CAMX_LOG_INFO(CamxLogGroupISP, "<DualIFE>/<scaler> right stripe");
    }

    CAMX_LOG_INFO(CamxLogGroupISP, "<scaler> full %d FD %d DS4 %d DS16 %d",
        pStripeOut->outRange_full[1]  - pStripeOut->outRange_full[0]  + 1,
        pStripeOut->outRange_fd[1]    - pStripeOut->outRange_fd[0]    + 1,
        pStripeOut->outRange_1to4[1]  - pStripeOut->outRange_1to4[0]  + 1,
        pStripeOut->outRange_1to16[1] - pStripeOut->outRange_1to16[0] + 1);
    CAMX_LOG_INFO(CamxLogGroupISP, "<scaler> disp %d DS4 %d DS16 %d",
        pStripeOut->outRange_disp[1]  - pStripeOut->outRange_disp[0]  + 1,
        pStripeOut->outRange_1to4_disp[1]  - pStripeOut->outRange_1to4_disp[0]  + 1,
        pStripeOut->outRange_1to16_disp[1] - pStripeOut->outRange_1to16_disp[0] + 1);
    CAMX_LOG_INFO(CamxLogGroupISP, "<scaler> post crop (enable, input, firstpixel, last pixel)"
        "full (%d %d %d %d) FD (%d %d %d %d) DS4 (%d %d %d %d) DS16 (%d %d %d %d)",
        pStripeOut->outCrop_full_Y.enable, pStripeOut->outCrop_full_Y.inDim,
        pStripeOut->outCrop_full_Y.firstOut, pStripeOut->outCrop_full_Y.lastOut,
        pStripeOut->outCrop_fd_Y.enable, pStripeOut->outCrop_fd_Y.inDim,
        pStripeOut->outCrop_fd_Y.firstOut, pStripeOut->outCrop_fd_Y.lastOut,
        pStripeOut->outCrop_1to4_Y.enable, pStripeOut->outCrop_1to4_Y.inDim,
        pStripeOut->outCrop_1to4_Y.firstOut, pStripeOut->outCrop_1to4_Y.lastOut,
        pStripeOut->outCrop_1to16_Y.enable, pStripeOut->outCrop_1to16_Y.inDim,
        pStripeOut->outCrop_1to16_Y.firstOut, pStripeOut->outCrop_1to16_Y.lastOut);
    CAMX_LOG_INFO(CamxLogGroupISP, "<scaler> Disp post crop (enable, input, firstpixel, last pixel)"
        "disp (%d %d %d %d) DS4 (%d %d %d %d) DS16 (%d %d %d %d)",
        pStripeOut->outCrop_Y_disp.enable, pStripeOut->outCrop_Y_disp.inDim,
        pStripeOut->outCrop_Y_disp.firstOut, pStripeOut->outCrop_Y_disp.lastOut,
        pStripeOut->outCrop_1to4_Y_disp.enable, pStripeOut->outCrop_1to4_Y_disp.inDim,
        pStripeOut->outCrop_1to4_Y_disp.firstOut, pStripeOut->outCrop_1to4_Y_disp.lastOut,
        pStripeOut->outCrop_1to16_Y_disp.enable, pStripeOut->outCrop_1to16_Y_disp.inDim,
        pStripeOut->outCrop_1to16_Y_disp.firstOut, pStripeOut->outCrop_1to16_Y_disp.lastOut);
    CAMX_LOG_INFO(CamxLogGroupISP, "<scaler>pre crop (enable, input, firstpixel, last pixel)"
        "DS4 (%d %d %d %d) DS16 (%d %d %d %d)",
        pStripeOut->preDS4Crop_1to4_Y.enable, pStripeOut->preDS4Crop_1to4_Y.inDim,
        pStripeOut->preDS4Crop_1to4_Y.firstOut, pStripeOut->preDS4Crop_1to4_Y.lastOut,
        pStripeOut->preDS4Crop_1to16_Y.enable, pStripeOut->preDS4Crop_1to16_Y.inDim,
        pStripeOut->preDS4Crop_1to16_Y.firstOut, pStripeOut->preDS4Crop_1to16_Y.lastOut);
    CAMX_LOG_INFO(CamxLogGroupISP, "<scaler> Disp pre crop (enable, input, firstpixel, last pixel)"
        "DS4 (%d %d %d %d) DS16 (%d %d %d %d)",
        pStripeOut->preDS4Crop_1to4_Y_disp.enable, pStripeOut->preDS4Crop_1to4_Y_disp.inDim,
        pStripeOut->preDS4Crop_1to4_Y_disp.firstOut, pStripeOut->preDS4Crop_1to4_Y_disp.lastOut,
        pStripeOut->preDS4Crop_1to16_Y_disp.enable, pStripeOut->preDS4Crop_1to16_Y_disp.inDim,
        pStripeOut->preDS4Crop_1to16_Y_disp.firstOut, pStripeOut->preDS4Crop_1to16_Y_disp.lastOut);
    CAMX_LOG_INFO(CamxLogGroupISP, "<scaler> mnds full in %d out %d offset %d cnt %d mn %d phase %d",
        pStripeOut->mndsConfig_y_full.input_l, pStripeOut->mndsConfig_y_full.output_l,
        pStripeOut->mndsConfig_y_full.pixel_offset,
        pStripeOut->mndsConfig_y_full.cnt_init,
        pStripeOut->mndsConfig_y_full.phase_init,
        pStripeOut->mndsConfig_y_full.phase_step);
    CAMX_LOG_INFO(CamxLogGroupISP, "<scaler> mnds disp in %d out %d offset %d cnt %d mn %d phase %d",
        pStripeOut->mndsConfig_y_disp.input_l, pStripeOut->mndsConfig_y_disp.output_l,
        pStripeOut->mndsConfig_y_disp.pixel_offset,
        pStripeOut->mndsConfig_y_disp.cnt_init,
        pStripeOut->mndsConfig_y_disp.phase_init,
        pStripeOut->mndsConfig_y_disp.phase_step);

    // LSC
    if (TRUE == pISPInputdata->pStripingInput->enableBits.rolloff)
    {
        pStripeConfig->stateLSC.dependenceData.stripeOut.lx_start_l =
            pStripeOut->rolloffOutStripe.grid_index;
        pStripeConfig->stateLSC.dependenceData.stripeOut.bx_start_l =
            pStripeOut->rolloffOutStripe.subgrid_index;
        pStripeConfig->stateLSC.dependenceData.stripeOut.bx_d1_l    =
            pStripeOut->rolloffOutStripe.pixel_index_within_subgrid;
        CAMX_LOG_INFO(CamxLogGroupISP,
            "Fetching: lx_start_l = %d, bx_d1_l = %d, bx_start_l = %d\n",
            pStripeOut->rolloffOutStripe.grid_index,
            pStripeOut->rolloffOutStripe.pixel_index_within_subgrid,
            pStripeOut->rolloffOutStripe.subgrid_index);
    }

    // AWB BG
    if (TRUE == pStripeOut->bg_awb_out.bg_enabled)
    {
        pStripeConfig->AWBStatsUpdateData.statsConfig                           =
            pISPInputdata->pAWBStatsUpdateData->statsConfig;
        pStripeConfig->AWBStatsUpdateData.statsConfig.BGConfig.horizontalNum    =
            pStripeOut->bg_awb_out.bg_rgn_h_num_stripe + 1;
        pStripeConfig->AWBStatsUpdateData.statsConfig.BGConfig.ROI.left         =
            pStripeOut->bg_awb_out.bg_roi_h_offset;
        pStripeConfig->AWBStatsUpdateData.statsConfig.BGConfig.ROI.width        =
            (pStripeOut->bg_awb_out.bg_rgn_h_num_stripe + 1) *
            (pISPInputdata->pStripingInput->stripingInput.bg_awb_input.bg_rgn_width + 1);
    }

    // BG Tintless
    if (TRUE == pStripeOut->bg_tintless_out.bg_enabled)
    {
        pStripeConfig->AECStatsUpdateData.statsConfig.TintlessBGConfig          =
            pISPInputdata->pAECStatsUpdateData->statsConfig.TintlessBGConfig;
        pStripeConfig->AECStatsUpdateData.statsConfig.TintlessBGConfig.horizontalNum    =
            pStripeOut->bg_tintless_out.bg_rgn_h_num_stripe + 1;
        pStripeConfig->AECStatsUpdateData.statsConfig.TintlessBGConfig.ROI.left         =
            pStripeOut->bg_tintless_out.bg_roi_h_offset;
        pStripeConfig->AECStatsUpdateData.statsConfig.TintlessBGConfig.ROI.width        =
            (pStripeOut->bg_tintless_out.bg_rgn_h_num_stripe + 1) *
            (pISPInputdata->pStripingInput->stripingInput.bg_tintless_input.bg_rgn_width + 1);
    }

    // HDR BE
    if (TRUE == pStripeOut->be_out.be_enable)
    {
        pStripeConfig->AECStatsUpdateData.statsConfig.BEConfig                      =
            pISPInputdata->pAECStatsUpdateData->statsConfig.BEConfig;
        pStripeConfig->AECStatsUpdateData.statsConfig.BEConfig.horizontalNum        =
            pStripeOut->be_out.be_rgn_h_num + 1;
        pStripeConfig->AECStatsUpdateData.statsConfig.BEConfig.ROI.left             =
            pStripeOut->be_out.be_roi_h_offset;
        pStripeConfig->AECStatsUpdateData.statsConfig.BEConfig.ROI.width            =
            (pStripeOut->be_out.be_rgn_h_num + 1) *
            (pISPInputdata->pStripingInput->stripingInput.be_input.be_rgn_width + 1);
        pStripeConfig->AECStatsUpdateData.statsConfig.BEConfig.isStripeValid        = TRUE;
    }

    // BAF
    pStripeConfig->AFStatsUpdateData.statsConfig = pISPInputdata->pAFStatsUpdateData->statsConfig;
    BFStatsROIDimensionParams* pBFROIDimension =
        pStripeConfig->AFStatsUpdateData.statsConfig.BFStats.BFStatsROIConfig.BFStatsROIDimension;
    pStripeConfig->AFStatsUpdateData.statsConfig.BFStats.BFStatsROIConfig.numBFStatsROIDimension = 0;
    Utils::Memset(pBFROIDimension, 0, sizeof(BFStatsROIDimensionParams));

    CAMX_LOG_INFO(CamxLogGroupISP, "<DualIFE> CropType %d enable %d",
            pISPInputdata->pStripeConfig->cropType,
            pStripeOut->baf_out.enable);

    if (TRUE == pStripeOut->baf_out.enable)
    {
        // Index 0 is start tag added for DMI transfer. This needs to be ignored.
        // This needs to be handled later in proper way.
        for (UINT32 index = 1; index <= BFMaxROIRegions; index++)
        {
            UINT64 bfROI = pStripeOut->baf_out.baf_roi_indexLUT[index];

            // We don't get how many windows have been selected in each windows. So if bfROI is 0,
            // we break and use index as number of ROIs configured.
            if (0 == bfROI)
            {
                pStripeConfig->AFStatsUpdateData.statsConfig.BFStats.BFStatsROIConfig.numBFStatsROIDimension = index - 1;
                CAMX_LOG_VERBOSE(CamxLogGroupISP, "Reaching the end of BF ROI index, index = %d", index);
                break;
            }

            pBFROIDimension[index - 1].region      = static_cast<BFStatsRegionType>(bfROI >> DMISelShift);
            pBFROIDimension[index - 1].regionNum   = (bfROI >> DMIIndexShift)  & 0x1FF;
            pBFROIDimension[index - 1].ROI.left    = (bfROI >> DMILeftShift)   & DMILeftBits;
            pBFROIDimension[index - 1].ROI.top     = (bfROI >> DMITopShift)    & DMITopBits;
            pBFROIDimension[index - 1].ROI.width   = (bfROI >> DMIWidthShift)  & DMIWidthBits;
            pBFROIDimension[index - 1].ROI.height  = (bfROI >> DMIHeightShift) & DMIHeightBits;
            CAMX_LOG_INFO(CamxLogGroupISP, "<DualIFE> ROI[%d] region %d num %d left, top, width, height (%d, %d, %d, %d)",
                index - 1, pBFROIDimension[index - 1].region,
                pBFROIDimension[index - 1].regionNum,
                pBFROIDimension[index - 1].ROI.left,
                pBFROIDimension[index - 1].ROI.top,
                pBFROIDimension[index - 1].ROI.width,
                pBFROIDimension[index - 1].ROI.height);

            pStripeConfig->AFStatsUpdateData.statsConfig.BFStats.BFStatsROIConfig.numBFStatsROIDimension++;
        }

        if (TRUE == pStripeOut->baf_out.mndsParam.en)
        {
            pStripeConfig->AFStatsUpdateData.statsConfig.BFStats.BFScaleConfig.isValid = TRUE;
            pStripeConfig->AFStatsUpdateData.statsConfig.BFStats.BFScaleConfig.BFScaleEnable =
                pStripeOut->baf_out.mndsParam.en;
            pStripeConfig->AFStatsUpdateData.statsConfig.BFStats.BFScaleConfig.interpolationResolution =
                pStripeOut->baf_out.mndsParam.interp_reso;
            pStripeConfig->AFStatsUpdateData.statsConfig.BFStats.BFScaleConfig.scaleM =
                pStripeOut->baf_out.mndsParam.output_l;
            pStripeConfig->AFStatsUpdateData.statsConfig.BFStats.BFScaleConfig.scaleN =
                pStripeOut->baf_out.mndsParam.input_l;
            pStripeConfig->AFStatsUpdateData.statsConfig.BFStats.BFScaleConfig.inputImageWidth =
                pStripeOut->baf_out.mndsParam.input_processed_length;
            pStripeConfig->AFStatsUpdateData.statsConfig.BFStats.BFScaleConfig.mnInit =
                pStripeOut->baf_out.mndsParam.cnt_init;
            pStripeConfig->AFStatsUpdateData.statsConfig.BFStats.BFScaleConfig.phaseInit =
                pStripeOut->baf_out.mndsParam.phase_init;
            pStripeConfig->AFStatsUpdateData.statsConfig.BFStats.BFScaleConfig.phaseStep =
                pStripeOut->baf_out.mndsParam.phase_step;
            pStripeConfig->AFStatsUpdateData.statsConfig.BFStats.BFScaleConfig.pixelOffset =
                pStripeOut->baf_out.mndsParam.pixel_offset;
        }
    }

    // check if BF stats enable for Talos
    if (TRUE == pStripeOut->baf_outv24.enable)
    {
        // Index 0 is start tag added for DMI transfer. This needs to be ignored.
        // This needs to be handled later in proper way.
        for (UINT32 index = 1; index <= BFMaxROIRegions; index++)
        {
            UINT64 bfROI = pStripeOut->baf_outv24.baf_roi_indexLUT[index];

            // We don't get how many windows have been selected in each windows. So if bfROI is 0,
            // we break and use index as number of ROIs configured.
            if (0 == bfROI)
            {
                pStripeConfig->AFStatsUpdateData.statsConfig.BFStats.BFStatsROIConfig.numBFStatsROIDimension = index - 1;
                CAMX_LOG_VERBOSE(CamxLogGroupISP, "Reaching the end of BF ROI index, index = %d", index);
                break;
            }

            pBFROIDimension[index - 1].region = static_cast<BFStatsRegionType>(bfROI >> DMISelShift);
            pBFROIDimension[index - 1].regionNum = (bfROI >> DMIIndexShift) & 0x1FF;
            pBFROIDimension[index - 1].ROI.left = (bfROI >> DMILeftShift)   & DMILeftBits;
            pBFROIDimension[index - 1].ROI.top = (bfROI >> DMITopShift)    & DMITopBits;
            pBFROIDimension[index - 1].ROI.width = (bfROI >> DMIWidthShift)  & DMIWidthBits;
            pBFROIDimension[index - 1].ROI.height = (bfROI >> DMIHeightShift) & DMIHeightBits;
            CAMX_LOG_INFO(CamxLogGroupISP, "<DualIFE> ROI[%d] region %d num %d left, top, width, height (%d, %d, %d, %d)",
                index - 1, pBFROIDimension[index - 1].region,
                pBFROIDimension[index - 1].regionNum,
                pBFROIDimension[index - 1].ROI.left,
                pBFROIDimension[index - 1].ROI.top,
                pBFROIDimension[index - 1].ROI.width,
                pBFROIDimension[index - 1].ROI.height);

            pStripeConfig->AFStatsUpdateData.statsConfig.BFStats.BFStatsROIConfig.numBFStatsROIDimension++;
        }

        if (TRUE == pStripeOut->baf_outv24.mndsParam.en)
        {
            pStripeConfig->AFStatsUpdateData.statsConfig.BFStats.BFScaleConfig.isValid = TRUE;
            pStripeConfig->AFStatsUpdateData.statsConfig.BFStats.BFScaleConfig.BFScaleEnable =
                pStripeOut->baf_outv24.mndsParam.en;
            pStripeConfig->AFStatsUpdateData.statsConfig.BFStats.BFScaleConfig.interpolationResolution =
                pStripeOut->baf_outv24.mndsParam.interp_reso;
            pStripeConfig->AFStatsUpdateData.statsConfig.BFStats.BFScaleConfig.scaleM =
                pStripeOut->baf_outv24.mndsParam.output_l;
            pStripeConfig->AFStatsUpdateData.statsConfig.BFStats.BFScaleConfig.scaleN =
                pStripeOut->baf_outv24.mndsParam.input_l;
            pStripeConfig->AFStatsUpdateData.statsConfig.BFStats.BFScaleConfig.inputImageWidth =
                pStripeOut->baf_outv24.mndsParam.input_processed_length;
            pStripeConfig->AFStatsUpdateData.statsConfig.BFStats.BFScaleConfig.mnInit =
                pStripeOut->baf_outv24.mndsParam.cnt_init;
            pStripeConfig->AFStatsUpdateData.statsConfig.BFStats.BFScaleConfig.phaseInit =
                pStripeOut->baf_outv24.mndsParam.phase_init;
            pStripeConfig->AFStatsUpdateData.statsConfig.BFStats.BFScaleConfig.phaseStep =
                pStripeOut->baf_outv24.mndsParam.phase_step;
            pStripeConfig->AFStatsUpdateData.statsConfig.BFStats.BFScaleConfig.pixelOffset =
                pStripeOut->baf_outv24.mndsParam.pixel_offset;
        }
    }
    // IHist
    if (TRUE == pStripeOut->iHist_out.enable)
    {
        pStripeConfig->IHistStatsUpdateData.statsConfig             =
            pISPInputdata->pIHistStatsUpdateData->statsConfig;
        pStripeConfig->IHistStatsUpdateData.statsConfig.ROI.left    = pStripeOut->iHist_out.hist_rgn_h_offset;
        pStripeConfig->IHistStatsUpdateData.statsConfig.ROI.width   =
            (pStripeOut->iHist_out.hist_rgn_h_num + 1) * IHistStats12RegionWidth;
    }

    // HDR BHist
    if (TRUE == pStripeOut->hdrBhist_out.bihist_enabled)
    {
        pStripeConfig->AECStatsUpdateData.statsConfig.HDRBHistConfig            =
            pISPInputdata->pAECStatsUpdateData->statsConfig.HDRBHistConfig;
        pStripeConfig->AECStatsUpdateData.statsConfig.HDRBHistConfig.ROI.left   =
            pStripeOut->hdrBhist_out.bihist_roi_h_offset;
        pStripeConfig->AECStatsUpdateData.statsConfig.HDRBHistConfig.ROI.width  =
            (pStripeOut->hdrBhist_out.bihist_rgn_h_num + 1) * HDRBHistStats13RegionWidth;
    }

    // RSCS
    if (TRUE == pStripeOut->rscs_out.rs_enable)
    {
        pStripeConfig->AFDStatsUpdateData.statsConfig               = pISPInputdata->pAFDStatsUpdateData->statsConfig;
        pStripeConfig->AFDStatsUpdateData.statsConfig.statsHOffset  = pStripeOut->rscs_out.rs_rgn_h_offset;
        pStripeConfig->AFDStatsUpdateData.statsConfig.statsHNum     = pStripeOut->rscs_out.rs_rgn_h_num + 1;
    }

    if (TRUE == pStripeOut->rscs_out.cs_enable)
    {
        pStripeConfig->CSStatsUpdateData.statsConfig                = pISPInputdata->pCSStatsUpdateData->statsConfig;
        pStripeConfig->CSStatsUpdateData.statsConfig.statsHOffset   = pStripeOut->rscs_out.cs_rgn_h_offset;
        pStripeConfig->CSStatsUpdateData.statsConfig.statsHNum      = pStripeOut->rscs_out.cs_rgn_h_num + 1;
    }

    pStripeConfig->CAMIFCrop.firstPixel = pStripeOut->fetchFirstPixel;
    pStripeConfig->CAMIFCrop.lastPixel  = pStripeOut->fetchLastPixel;

    // BHist
    if (TRUE == pStripeOut->bHist_out.bihist_enabled)
    {
        pStripeConfig->AECStatsUpdateData.statsConfig.BHistConfig           =
            pISPInputdata->pAECStatsUpdateData->statsConfig.BHistConfig;
        pStripeConfig->AECStatsUpdateData.statsConfig.BHistConfig.ROI.left  =
            pStripeOut->bHist_out.bihist_roi_h_offset;
        pStripeConfig->AECStatsUpdateData.statsConfig.BHistConfig.ROI.width =
            (pStripeOut->bHist_out.bihist_rgn_h_num + 1) * BHistStats14RegionWidth;
    }

    UINT32  fullWidth;
    UINT32  FDWidth;
    UINT32  DS4Width;
    UINT32  DS16Width;

    // Display Full/DS4/DS16 ports support
    UINT32  dispWidth;
    UINT32  DS4DisplayWidth;
    UINT32  DS16DisplayWidth;

    // Extract output ranges
    fullWidth   = pStripeOut->outRange_full[1] - pStripeOut->outRange_full[0] + 1;
    FDWidth     = pStripeOut->outRange_fd[1] - pStripeOut->outRange_fd[0] + 1;
    DS4Width    = pStripeOut->outRange_1to4[1] - pStripeOut->outRange_1to4[0] + 1;
    DS16Width   = pStripeOut->outRange_1to16[1] - pStripeOut->outRange_1to16[0] + 1;

    // Display Full/DS4/DS16 ports support
    dispWidth        = pStripeOut->outRange_disp[1]       - pStripeOut->outRange_disp[0] + 1;
    DS4DisplayWidth  = pStripeOut->outRange_1to4_disp[1]  - pStripeOut->outRange_1to4_disp[0] + 1;
    DS16DisplayWidth = pStripeOut->outRange_1to16_disp[1] - pStripeOut->outRange_1to16_disp[0] + 1;

    pStripeConfig->stream[FDOutput].width     = FDWidth;
    pStripeConfig->stream[FDOutput].height    =
        Utils::EvenCeilingUINT32(pISPInputdata->HALData.stream[FDOutput].height);
    pStripeConfig->stream[FDOutput].offset    = pStripeOut->outRange_fd[0];
    pStripeConfig->stream[FullOutput].width   = fullWidth;
    pStripeConfig->stream[FullOutput].height  =
        Utils::EvenCeilingUINT32(pISPInputdata->HALData.stream[FullOutput].height);
    pStripeConfig->stream[FullOutput].offset  = pStripeOut->outRange_full[0];
    pStripeConfig->stream[DS4Output].width    = DS4Width;
    pStripeConfig->stream[DS4Output].height   =
        Utils::EvenCeilingUINT32(pISPInputdata->HALData.stream[DS4Output].height);
    pStripeConfig->stream[DS4Output].offset   = pStripeOut->outRange_1to4[0];
    pStripeConfig->stream[DS16Output].width   = DS16Width;
    pStripeConfig->stream[DS16Output].height  =
        Utils::EvenCeilingUINT32(pISPInputdata->HALData.stream[DS16Output].height);
    pStripeConfig->stream[DS16Output].offset  = pStripeOut->outRange_1to16[0];

    // Display Full/DS4/DS16 ports support
    pStripeConfig->stream[DisplayFullOutput].width   = dispWidth;
    pStripeConfig->stream[DisplayFullOutput].height  =
        Utils::EvenCeilingUINT32(pISPInputdata->HALData.stream[DisplayFullOutput].height);
    pStripeConfig->stream[DisplayFullOutput].offset  = pStripeOut->outRange_disp[0];

    pStripeConfig->stream[DisplayDS4Output].width    = DS4DisplayWidth;
    pStripeConfig->stream[DisplayDS4Output].height   =
        Utils::EvenCeilingUINT32(pISPInputdata->HALData.stream[DisplayDS4Output].height);
    pStripeConfig->stream[DisplayDS4Output].offset   = pStripeOut->outRange_1to4_disp[0];

    pStripeConfig->stream[DisplayDS16Output].width   = DS16DisplayWidth;
    pStripeConfig->stream[DisplayDS16Output].height  =
        Utils::EvenCeilingUINT32(pISPInputdata->HALData.stream[DisplayDS16Output].height);
    pStripeConfig->stream[DisplayDS16Output].offset  = pStripeOut->outRange_1to16_disp[0];
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// DualIFEUtils::FetchCfgWithStripeOutput
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void DualIFEUtils::FetchCfgWithStripeOutput(
    ISPInputData*       pISPInputdata,
    IFEPassOutput*      pPassOut,
    ISPStripeConfig*    pStripeConfigs)
{
    uint16_t stripeNum = 0;

    CAMX_UNREFERENCED_PARAM(pISPInputdata);

    CAMX_ASSERT((NULL != pPassOut) && (NULL != pPassOut->hStripeList.pListHead));

    IFEStripeOutput* pStripe1 =
        static_cast<IFEStripeOutput*>(pPassOut->hStripeList.pListHead->pData);

    CAMX_ASSERT(NULL != pStripe1);

    if (1 == pStripe1->edgeStripeLT) // left stripe
    {
        stripeNum = 0;
    }
    else if (1 == pStripe1->edgeStripeRB)// right stripe
    {
        stripeNum = 1;
    }
    pStripeConfigs[stripeNum].pStripeOutput = pStripe1;
    pStripeConfigs[stripeNum].stripeId = stripeNum;
    /* Reset Left & Right Stripe config for BE stats ROI */
    pStripeConfigs[0].AECStatsUpdateData.statsConfig.BEConfig.isStripeValid = FALSE;
    pStripeConfigs[1].AECStatsUpdateData.statsConfig.BEConfig.isStripeValid = FALSE;
    FillCfgFromOneStripe(pISPInputdata, pStripe1, &pStripeConfigs[stripeNum]);

    IFEStripeOutput* pStripe2 =
        static_cast<IFEStripeOutput*>(pPassOut->hStripeList.pListHead->pNextNode->pData);

    CAMX_ASSERT(NULL != pStripe2);

    if (1 == pStripe2->edgeStripeLT) // left stripe
    {
        stripeNum = 0;
    }
    else if (1 == pStripe2->edgeStripeRB)// right stripe
    {
        stripeNum = 1;
    }
    pStripeConfigs[stripeNum].pStripeOutput = pStripe2;
    pStripeConfigs[stripeNum].stripeId = stripeNum;
    FillCfgFromOneStripe(pISPInputdata, pStripe2, &pStripeConfigs[stripeNum]);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// DualIFEUtils::ComputeSplitParams
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult DualIFEUtils::ComputeSplitParams(
    ISPInputData*       pISPInputdata,
    DualIFESplitParams* pSplitParams)
{
    CamxResult            result    = CamxResultSuccess;
    const StaticSettings* pSettings = HwEnvironment::GetInstance()->GetStaticSettings();

    CAMX_ASSERT(NULL != pISPInputdata);
    CAMX_ASSERT(NULL != pSplitParams);
    CAMX_ASSERT(TRUE == pSettings->enableDualIFE);

    if (TRUE == pSettings->enableDualIFEOverwrite)
    {
        pSplitParams->splitPoint    =
            (pISPInputdata->sensorData.sensorOut.width / 2) + pSettings->dualIFESplitPointOffset;
        pSplitParams->leftPadding   = pSettings->dualIFELeftPadding;
        pSplitParams->rightPadding  = pSettings->dualIFERightPadding;
    }
    else
    {
        result = UpdateStripingInput(pISPInputdata);
        stripingInput_titanIFE* pInput = &pISPInputdata->pStripingInput->stripingInput;
        if (CamxResultSuccess == result)
        {
            result = deriveFetchRange_titanIFE(pInput);
        }
        if (CamxResultSuccess == result)
        {
            // calculate spliting point and left/right padding
            pSplitParams->splitPoint = pISPInputdata->sensorData.sensorOut.width / 2;

            if ((pSplitParams->splitPoint > static_cast<UINT32>(pInput->fetchLeftStripeEnd)) ||
                (pSplitParams->splitPoint < static_cast<UINT32>(pInput->fetchRightStripeStart)))
            {
                pSplitParams->splitPoint = (pInput->fetchRightStripeStart + pInput->fetchLeftStripeEnd + 1) / 2;
            }

            pSplitParams->leftPadding  = pSplitParams->splitPoint - pInput->fetchRightStripeStart;
            pSplitParams->rightPadding = pInput->fetchLeftStripeEnd - pSplitParams->splitPoint + 1;
            CAMX_LOG_INFO(CamxLogGroupISP,
                             "<DualIFE> leftStripeEnd = %d, rightStripeStart = %d",
                             pInput->fetchLeftStripeEnd,
                             pInput->fetchRightStripeStart);

            CAMX_LOG_INFO(CamxLogGroupISP, "<DualIFE> splitPoint = %d, leftPadding = %d, rightPadding = %d",
                pSplitParams->splitPoint, pSplitParams->leftPadding, pSplitParams->rightPadding);
        }
    }
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// DualIFEUtils::GetDefaultDualIFEStatsConfig
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID DualIFEUtils::GetDefaultDualIFEStatsConfig(
    ISPInputData*       pISPInputdata,
    ISPStripeConfig*    pStripeConfig,
    DualIFESplitParams* pSplitParams)
{
    // Handle BE stats splitting
    pStripeConfig[0].AECStatsUpdateData.statsConfig.BEConfig            =
        pISPInputdata->pAECStatsUpdateData->statsConfig.BEConfig;
    pStripeConfig[0].AECStatsUpdateData.statsConfig.BEConfig.ROI.left   =
        pISPInputdata->pAECStatsUpdateData->statsConfig.BEConfig.ROI.left;
    pStripeConfig[0].AECStatsUpdateData.statsConfig.BEConfig.ROI.width  =
        pSplitParams->splitPoint - pISPInputdata->pAECStatsUpdateData->statsConfig.BEConfig.ROI.left;

    FLOAT splitRatioBE = static_cast<FLOAT>(pStripeConfig[0].AECStatsUpdateData.statsConfig.BEConfig.ROI.width) /
        pISPInputdata->pAECStatsUpdateData->statsConfig.BEConfig.ROI.width;

    pStripeConfig[0].AECStatsUpdateData.statsConfig.BEConfig.horizontalNum =
        static_cast<UINT32>(splitRatioBE * pISPInputdata->pAECStatsUpdateData->statsConfig.BEConfig.horizontalNum);

    pStripeConfig[1].AECStatsUpdateData.statsConfig.BEConfig                =
        pISPInputdata->pAECStatsUpdateData->statsConfig.BEConfig;
    pStripeConfig[1].AECStatsUpdateData.statsConfig.BEConfig.ROI.left       =
        pSplitParams->splitPoint - pStripeConfig[1].CAMIFCrop.firstPixel;
    pStripeConfig[1].AECStatsUpdateData.statsConfig.BEConfig.ROI.width      =
        pISPInputdata->pAECStatsUpdateData->statsConfig.BEConfig.ROI.width -
        pStripeConfig[0].AECStatsUpdateData.statsConfig.BEConfig.ROI.width;
    pStripeConfig[1].AECStatsUpdateData.statsConfig.BEConfig.horizontalNum  =
        pISPInputdata->pAECStatsUpdateData->statsConfig.BEConfig.horizontalNum -
        pStripeConfig[0].AECStatsUpdateData.statsConfig.BEConfig.horizontalNum;

    // AWB BG splitting
    pStripeConfig[0].AWBStatsUpdateData.statsConfig.BGConfig            =
        pISPInputdata->pAWBStatsUpdateData->statsConfig.BGConfig;
    pStripeConfig[0].AWBStatsUpdateData.statsConfig.BGConfig.ROI.left   =
        pISPInputdata->pAWBStatsUpdateData->statsConfig.BGConfig.ROI.left;
    pStripeConfig[0].AWBStatsUpdateData.statsConfig.BGConfig.ROI.width  =
        pSplitParams->splitPoint - pISPInputdata->pAWBStatsUpdateData->statsConfig.BGConfig.ROI.left;

    FLOAT splitRatioBG = static_cast<FLOAT>(pStripeConfig[0].AWBStatsUpdateData.statsConfig.BGConfig.ROI.width) /
        pISPInputdata->pAWBStatsUpdateData->statsConfig.BGConfig.ROI.width;

    pStripeConfig[0].AWBStatsUpdateData.statsConfig.BGConfig.horizontalNum =
        static_cast<UINT32>(splitRatioBG * pISPInputdata->pAWBStatsUpdateData->statsConfig.BGConfig.horizontalNum);

    pStripeConfig[1].AWBStatsUpdateData.statsConfig.BGConfig                =
        pISPInputdata->pAWBStatsUpdateData->statsConfig.BGConfig;
    pStripeConfig[1].AWBStatsUpdateData.statsConfig.BGConfig.ROI.left       =
        pSplitParams->splitPoint - pStripeConfig[1].CAMIFCrop.firstPixel;
    pStripeConfig[1].AWBStatsUpdateData.statsConfig.BGConfig.ROI.width      =
        pISPInputdata->pAWBStatsUpdateData->statsConfig.BGConfig.ROI.width -
        pStripeConfig[0].AWBStatsUpdateData.statsConfig.BGConfig.ROI.width;
    pStripeConfig[1].AWBStatsUpdateData.statsConfig.BGConfig.horizontalNum  =
        pISPInputdata->pAWBStatsUpdateData->statsConfig.BGConfig.horizontalNum -
        pStripeConfig[0].AWBStatsUpdateData.statsConfig.BGConfig.horizontalNum;

    // Handle BG stats splitting
    pStripeConfig[0].AECStatsUpdateData.statsConfig.BGConfig            =
        pISPInputdata->pAECStatsUpdateData->statsConfig.BGConfig;
    pStripeConfig[0].AECStatsUpdateData.statsConfig.BGConfig.ROI.left   =
        pISPInputdata->pAECStatsUpdateData->statsConfig.BGConfig.ROI.left;
    pStripeConfig[0].AECStatsUpdateData.statsConfig.BGConfig.ROI.width  =
        pSplitParams->splitPoint - pISPInputdata->pAECStatsUpdateData->statsConfig.BGConfig.ROI.left;

    splitRatioBG = static_cast<FLOAT>(pStripeConfig[0].AECStatsUpdateData.statsConfig.BGConfig.ROI.width) /
        pISPInputdata->pAECStatsUpdateData->statsConfig.BGConfig.ROI.width;

    pStripeConfig[0].AECStatsUpdateData.statsConfig.BGConfig.horizontalNum =
        static_cast<UINT32>(splitRatioBG * pISPInputdata->pAECStatsUpdateData->statsConfig.BGConfig.horizontalNum);

    pStripeConfig[1].AECStatsUpdateData.statsConfig.BGConfig                =
        pISPInputdata->pAECStatsUpdateData->statsConfig.BGConfig;
    pStripeConfig[1].AECStatsUpdateData.statsConfig.BGConfig.ROI.left       =
        pSplitParams->splitPoint - pStripeConfig[1].CAMIFCrop.firstPixel;
    pStripeConfig[1].AECStatsUpdateData.statsConfig.BGConfig.ROI.width      =
        pISPInputdata->pAECStatsUpdateData->statsConfig.BGConfig.ROI.width -
        pStripeConfig[0].AECStatsUpdateData.statsConfig.BGConfig.ROI.width;
    pStripeConfig[1].AECStatsUpdateData.statsConfig.BGConfig.horizontalNum  =
        pISPInputdata->pAECStatsUpdateData->statsConfig.BGConfig.horizontalNum -
        pStripeConfig[0].AECStatsUpdateData.statsConfig.BGConfig.horizontalNum;


    // BHist
    pStripeConfig[0].AECStatsUpdateData.statsConfig.BHistConfig             =
        pISPInputdata->pAECStatsUpdateData->statsConfig.BHistConfig;
    pStripeConfig[0].AECStatsUpdateData.statsConfig.BHistConfig.ROI.left    =
        pISPInputdata->pAECStatsUpdateData->statsConfig.BHistConfig.ROI.left;
    pStripeConfig[0].AECStatsUpdateData.statsConfig.BHistConfig.ROI.width   =
        pSplitParams->splitPoint - pISPInputdata->pAECStatsUpdateData->statsConfig.BHistConfig.ROI.left;

    pStripeConfig[1].AECStatsUpdateData.statsConfig.BHistConfig                 =
        pISPInputdata->pAECStatsUpdateData->statsConfig.BHistConfig;
    pStripeConfig[1].AECStatsUpdateData.statsConfig.BHistConfig.ROI.left        =
        pSplitParams->splitPoint - pStripeConfig[1].CAMIFCrop.firstPixel;
    pStripeConfig[1].AECStatsUpdateData.statsConfig.BHistConfig.ROI.width       =
        pISPInputdata->pAECStatsUpdateData->statsConfig.BHistConfig.ROI.width -
        pStripeConfig[0].AECStatsUpdateData.statsConfig.BHistConfig.ROI.width;

    // HDRBHist
    pStripeConfig[0].AECStatsUpdateData.statsConfig.HDRBHistConfig                  =
        pISPInputdata->pAECStatsUpdateData->statsConfig.HDRBHistConfig;
    pStripeConfig[0].AECStatsUpdateData.statsConfig.HDRBHistConfig.ROI.width        =
        pSplitParams->splitPoint - pISPInputdata->pAECStatsUpdateData->statsConfig.HDRBHistConfig.ROI.left;
    pStripeConfig[1].AECStatsUpdateData.statsConfig.HDRBHistConfig                  =
        pISPInputdata->pAECStatsUpdateData->statsConfig.HDRBHistConfig;
    pStripeConfig[1].AECStatsUpdateData.statsConfig.HDRBHistConfig.ROI.left         =
        pSplitParams->splitPoint - pStripeConfig[1].CAMIFCrop.firstPixel;
    pStripeConfig[1].AECStatsUpdateData.statsConfig.HDRBHistConfig.ROI.width        =
        pISPInputdata->pAECStatsUpdateData->statsConfig.HDRBHistConfig.ROI.width -
        pStripeConfig[0].AECStatsUpdateData.statsConfig.HDRBHistConfig.ROI.width;

    // IHist
    pStripeConfig[0].IHistStatsUpdateData.statsConfig                   =
        pISPInputdata->pIHistStatsUpdateData->statsConfig;
    pStripeConfig[0].IHistStatsUpdateData.statsConfig.ROI.width         =
        pSplitParams->splitPoint - pISPInputdata->pIHistStatsUpdateData->statsConfig.ROI.left;
    pStripeConfig[1].IHistStatsUpdateData.statsConfig                   =
        pISPInputdata->pIHistStatsUpdateData->statsConfig;
    pStripeConfig[1].IHistStatsUpdateData.statsConfig.ROI.left          =
        pSplitParams->splitPoint - pStripeConfig[1].CAMIFCrop.firstPixel;
    pStripeConfig[1].IHistStatsUpdateData.statsConfig.ROI.width         =
        pISPInputdata->pIHistStatsUpdateData->statsConfig.ROI.width -
        pStripeConfig[0].IHistStatsUpdateData.statsConfig.ROI.width;

    // RS stats
    pStripeConfig[0].AFDStatsUpdateData.statsConfig                 = pISPInputdata->pAFDStatsUpdateData->statsConfig;
    pStripeConfig[0].AFDStatsUpdateData.statsConfig.statsRgnWidth   =
        pSplitParams->splitPoint - pISPInputdata->pAFDStatsUpdateData->statsConfig.statsHOffset;

    FLOAT splitRatioRS = static_cast<FLOAT>(pStripeConfig[0].AFDStatsUpdateData.statsConfig.statsRgnWidth) /
        pISPInputdata->pAFDStatsUpdateData->statsConfig.statsRgnWidth;

    pStripeConfig[0].AFDStatsUpdateData.statsConfig.statsHNum       =
        static_cast<UINT32>(splitRatioRS * pISPInputdata->pAFDStatsUpdateData->statsConfig.statsHNum);

    pStripeConfig[1].AFDStatsUpdateData.statsConfig                 = pISPInputdata->pAFDStatsUpdateData->statsConfig;
    pStripeConfig[1].AFDStatsUpdateData.statsConfig.statsHOffset    =
        pSplitParams->splitPoint - pStripeConfig[1].CAMIFCrop.firstPixel;
    pStripeConfig[1].AFDStatsUpdateData.statsConfig.statsRgnWidth   =
        pISPInputdata->pAFDStatsUpdateData->statsConfig.statsRgnWidth -
        pStripeConfig[0].AFDStatsUpdateData.statsConfig.statsRgnWidth;
    pStripeConfig[1].AFDStatsUpdateData.statsConfig.statsHNum       =
        pISPInputdata->pAFDStatsUpdateData->statsConfig.statsHNum -
        pStripeConfig[0].AFDStatsUpdateData.statsConfig.statsHNum;


    // CS stats
    pStripeConfig[0].CSStatsUpdateData.statsConfig                  = pISPInputdata->pCSStatsUpdateData->statsConfig;
    pStripeConfig[0].CSStatsUpdateData.statsConfig.statsRgnWidth    =
        pSplitParams->splitPoint - pISPInputdata->pCSStatsUpdateData->statsConfig.statsHOffset;

    FLOAT splitRatioCS = static_cast<FLOAT>(pStripeConfig[0].CSStatsUpdateData.statsConfig.statsRgnWidth) /
        pISPInputdata->pCSStatsUpdateData->statsConfig.statsRgnWidth;

    pStripeConfig[0].CSStatsUpdateData.statsConfig.statsHNum        =
        static_cast<UINT32>(splitRatioCS * pISPInputdata->pCSStatsUpdateData->statsConfig.statsHNum);

    pStripeConfig[1].CSStatsUpdateData.statsConfig = pISPInputdata->pCSStatsUpdateData->statsConfig;
    pStripeConfig[1].CSStatsUpdateData.statsConfig.statsHOffset     =
        pSplitParams->splitPoint - pStripeConfig[1].CAMIFCrop.firstPixel;
    pStripeConfig[1].CSStatsUpdateData.statsConfig.statsRgnWidth    =
        pISPInputdata->pCSStatsUpdateData->statsConfig.statsRgnWidth -
        pStripeConfig[0].CSStatsUpdateData.statsConfig.statsRgnWidth;
    pStripeConfig[1].CSStatsUpdateData.statsConfig.statsHNum        =
        pISPInputdata->pCSStatsUpdateData->statsConfig.statsHNum -
        pStripeConfig[0].CSStatsUpdateData.statsConfig.statsHNum;

    // Tintless BG splitting
    pStripeConfig[0].AECStatsUpdateData.statsConfig.TintlessBGConfig =
        pISPInputdata->pAECStatsUpdateData->statsConfig.TintlessBGConfig;
    pStripeConfig[0].AECStatsUpdateData.statsConfig.TintlessBGConfig.ROI.left =
        pISPInputdata->pAECStatsUpdateData->statsConfig.TintlessBGConfig.ROI.left;
    pStripeConfig[0].AECStatsUpdateData.statsConfig.TintlessBGConfig.ROI.width =
        pSplitParams->splitPoint - pISPInputdata->pAECStatsUpdateData->statsConfig.TintlessBGConfig.ROI.left;

    splitRatioBG = static_cast<FLOAT>(pStripeConfig[0].AECStatsUpdateData.statsConfig.TintlessBGConfig.ROI.width) /
        pISPInputdata->pAECStatsUpdateData->statsConfig.TintlessBGConfig.ROI.width;

    pStripeConfig[0].AECStatsUpdateData.statsConfig.TintlessBGConfig.horizontalNum =
        static_cast<UINT32>(splitRatioBG * pISPInputdata->pAECStatsUpdateData->statsConfig.TintlessBGConfig.horizontalNum);

    pStripeConfig[1].AECStatsUpdateData.statsConfig.TintlessBGConfig =
        pISPInputdata->pAECStatsUpdateData->statsConfig.TintlessBGConfig;
    pStripeConfig[1].AECStatsUpdateData.statsConfig.TintlessBGConfig.ROI.left =
        pSplitParams->splitPoint - pStripeConfig[1].CAMIFCrop.firstPixel;
    pStripeConfig[1].AECStatsUpdateData.statsConfig.TintlessBGConfig.ROI.width =
        pISPInputdata->pAECStatsUpdateData->statsConfig.TintlessBGConfig.ROI.width -
        pStripeConfig[0].AECStatsUpdateData.statsConfig.TintlessBGConfig.ROI.width;
    pStripeConfig[1].AECStatsUpdateData.statsConfig.TintlessBGConfig.horizontalNum =
        pISPInputdata->pAECStatsUpdateData->statsConfig.TintlessBGConfig.horizontalNum -
        pStripeConfig[0].AECStatsUpdateData.statsConfig.TintlessBGConfig.horizontalNum;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// DualIFEUtils::ReleaseDualIfePassResult
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID DualIFEUtils::ReleaseDualIfePassResult(
    IFEPassOutput* pPassOut)
{
    ListNode_T* pNode = NULL;

    if (pPassOut == NULL)
    {
        return;
    }

    pPassOut->numStripes = 0;

    while (pPassOut->hStripeList.pListHead != NULL)
    {
        pNode = List_RemoveHead(&pPassOut->hStripeList);

        CAMX_FREE(pNode->pData);
        CAMX_FREE(pNode);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// DualIFEUtils::PrintDualIfeInput
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID DualIFEUtils::PrintDualIfeInput(
    const stripingInput_titanIFE* pStripingInput)
{
    STRIPE_FIELD_PRINT(pStripingInput->tiering);
    STRIPE_FIELD_PRINT(pStripingInput->striping);
    STRIPE_FIELD_PRINT(pStripingInput->inputFormat);
    STRIPE_FIELD_PRINT(pStripingInput->inputWidth);
    STRIPE_FIELD_PRINT(pStripingInput->inputHeight);
    STRIPE_FIELD_PRINT(pStripingInput->use_zoom_setting_from_external);
    STRIPE_FIELD_PRINT(pStripingInput->roi_mnds_out_full.start_x);
    STRIPE_FIELD_PRINT(pStripingInput->roi_mnds_out_full.end_x);
    STRIPE_FIELD_PRINT(pStripingInput->roi_mnds_out_fd.start_x);
    STRIPE_FIELD_PRINT(pStripingInput->roi_mnds_out_fd.end_x);
    STRIPE_FIELD_PRINT(pStripingInput->roi_mnds_out_disp.start_x);
    STRIPE_FIELD_PRINT(pStripingInput->roi_mnds_out_disp.end_x);
    STRIPE_FIELD_PRINT(pStripingInput->zoom_window.zoom_enable);
    STRIPE_FIELD_PRINT(pStripingInput->zoom_window.start_x);
    STRIPE_FIELD_PRINT(pStripingInput->zoom_window.start_y);
    STRIPE_FIELD_PRINT(pStripingInput->zoom_window.width);
    STRIPE_FIELD_PRINT(pStripingInput->zoom_window.height);
    STRIPE_FIELD_PRINT(pStripingInput->outFormat_full);
    STRIPE_FIELD_PRINT(pStripingInput->outWidth_full);
    STRIPE_FIELD_PRINT(pStripingInput->outHeight_full);
    STRIPE_FIELD_PRINT(pStripingInput->outFormat_1to4);
    STRIPE_FIELD_PRINT(pStripingInput->outFormat_1to16);
    STRIPE_FIELD_PRINT(pStripingInput->outFormat_fd);
    STRIPE_FIELD_PRINT(pStripingInput->outWidth_fd);
    STRIPE_FIELD_PRINT(pStripingInput->outHeight_fd);
    STRIPE_FIELD_PRINT(pStripingInput->outFormat_disp);
    STRIPE_FIELD_PRINT(pStripingInput->outWidth_disp);
    STRIPE_FIELD_PRINT(pStripingInput->outHeight_disp);
    STRIPE_FIELD_PRINT(pStripingInput->outFormat_1to4_disp);
    STRIPE_FIELD_PRINT(pStripingInput->outFormat_1to16_disp);
    STRIPE_FIELD_PRINT(pStripingInput->outFormat_raw);
    STRIPE_FIELD_PRINT(pStripingInput->outFormat_pdaf);
    STRIPE_FIELD_PRINT(pStripingInput->pedestal_enable);
    STRIPE_FIELD_PRINT(pStripingInput->rolloff_enable);
    STRIPE_FIELD_PRINT(pStripingInput->baf_enable);
    STRIPE_FIELD_PRINT(pStripingInput->bg_tintless_enable);
    STRIPE_FIELD_PRINT(pStripingInput->bg_awb_enable);
    STRIPE_FIELD_PRINT(pStripingInput->be_enable);
    STRIPE_FIELD_PRINT(pStripingInput->pdaf_enable);
    STRIPE_FIELD_PRINT(pStripingInput->hdr_enable);
    STRIPE_FIELD_PRINT(pStripingInput->bpc_enable);
    STRIPE_FIELD_PRINT(pStripingInput->abf_enable);
    STRIPE_FIELD_PRINT(pStripingInput->tappingPoint_be);
    STRIPE_FIELD_PRINT(pStripingInput->tapoffPoint_hvx);
    STRIPE_FIELD_PRINT(pStripingInput->kernelSize_hvx);
    STRIPE_FIELD_PRINT(pStripingInput->fetchLeftStripeEnd);
    STRIPE_FIELD_PRINT(pStripingInput->fetchRightStripeStart);
    STRIPE_FIELD_PRINT(pStripingInput->hdr_in.hdr_zrec_first_rb_exp);
    STRIPE_FIELD_PRINT(pStripingInput->mnds_in_y_fd.input_l);
    STRIPE_FIELD_PRINT(pStripingInput->mnds_in_y_fd.output_l);
    STRIPE_FIELD_PRINT(pStripingInput->mnds_in_y_fd.rounding_option_h);
    STRIPE_FIELD_PRINT(pStripingInput->mnds_in_y_fd.rounding_option_v);
    STRIPE_FIELD_PRINT(pStripingInput->mnds_in_c_fd.input_l);
    STRIPE_FIELD_PRINT(pStripingInput->mnds_in_c_fd.output_l);
    STRIPE_FIELD_PRINT(pStripingInput->mnds_in_c_fd.rounding_option_h);
    STRIPE_FIELD_PRINT(pStripingInput->mnds_in_c_fd.rounding_option_v);
    STRIPE_FIELD_PRINT(pStripingInput->mnds_in_y_full.input_l);
    STRIPE_FIELD_PRINT(pStripingInput->mnds_in_y_full.output_l);
    STRIPE_FIELD_PRINT(pStripingInput->mnds_in_y_full.rounding_option_h);
    STRIPE_FIELD_PRINT(pStripingInput->mnds_in_y_full.rounding_option_v);
    STRIPE_FIELD_PRINT(pStripingInput->mnds_in_c_full.input_l);
    STRIPE_FIELD_PRINT(pStripingInput->mnds_in_c_full.output_l);
    STRIPE_FIELD_PRINT(pStripingInput->mnds_in_c_full.rounding_option_h);
    STRIPE_FIELD_PRINT(pStripingInput->mnds_in_c_full.rounding_option_v);
    STRIPE_FIELD_PRINT(pStripingInput->mnds_in_y_disp.input_l);
    STRIPE_FIELD_PRINT(pStripingInput->mnds_in_y_disp.output_l);
    STRIPE_FIELD_PRINT(pStripingInput->mnds_in_y_disp.rounding_option_h);
    STRIPE_FIELD_PRINT(pStripingInput->mnds_in_y_disp.rounding_option_v);
    STRIPE_FIELD_PRINT(pStripingInput->mnds_in_c_disp.input_l);
    STRIPE_FIELD_PRINT(pStripingInput->mnds_in_c_disp.output_l);
    STRIPE_FIELD_PRINT(pStripingInput->mnds_in_c_disp.rounding_option_h);
    STRIPE_FIELD_PRINT(pStripingInput->mnds_in_c_disp.rounding_option_v);
    STRIPE_FIELD_PRINT(pStripingInput->pdpc_in.pdaf_global_offset_x);
    STRIPE_FIELD_PRINT(pStripingInput->pdpc_in.pdaf_x_end);
    STRIPE_FIELD_PRINT(pStripingInput->pdpc_in.pdaf_zzHDR_first_rb_exp);
    for (UINT32 idx = 0; idx < 64; idx++ )
    {
        STRIPE_FIELD_PRINT(pStripingInput->pdpc_in.PDAF_PD_Mask[idx]);
    }
    STRIPE_FIELD_PRINT(pStripingInput->pedestalParam.enable);
    STRIPE_FIELD_PRINT(pStripingInput->pedestalParam.Bwidth_l);
    STRIPE_FIELD_PRINT(pStripingInput->pedestalParam.MeshGridBwidth_l);
    STRIPE_FIELD_PRINT(pStripingInput->pedestalParam.Lx_start_l);
    STRIPE_FIELD_PRINT(pStripingInput->pedestalParam.Bx_start_l);
    STRIPE_FIELD_PRINT(pStripingInput->pedestalParam.Bx_d1_l);
    STRIPE_FIELD_PRINT(pStripingInput->rollOffParam.enable);
    STRIPE_FIELD_PRINT(pStripingInput->rollOffParam.Bwidth_l);
    STRIPE_FIELD_PRINT(pStripingInput->rollOffParam.MeshGridBwidth_l);
    STRIPE_FIELD_PRINT(pStripingInput->rollOffParam.Lx_start_l);
    STRIPE_FIELD_PRINT(pStripingInput->rollOffParam.Bx_start_l);
    STRIPE_FIELD_PRINT(pStripingInput->rollOffParam.Bx_d1_l);
    STRIPE_FIELD_PRINT(pStripingInput->rollOffParam.num_meshgain_h);
    STRIPE_FIELD_PRINT(pStripingInput->hdrBhist_input.bihist_enabled);
    STRIPE_FIELD_PRINT(pStripingInput->hdrBhist_input.bihist_roi_h_offset);
    STRIPE_FIELD_PRINT(pStripingInput->hdrBhist_input.bihist_rgn_h_num);
    STRIPE_FIELD_PRINT(pStripingInput->bHist_input.bihist_enabled);
    STRIPE_FIELD_PRINT(pStripingInput->bHist_input.bihist_roi_h_offset);
    STRIPE_FIELD_PRINT(pStripingInput->bHist_input.bihist_rgn_h_num);
    STRIPE_FIELD_PRINT(pStripingInput->iHist_input.enable);
    STRIPE_FIELD_PRINT(pStripingInput->iHist_input.hist_rgn_h_offset);
    STRIPE_FIELD_PRINT(pStripingInput->iHist_input.hist_rgn_h_num);
    STRIPE_FIELD_PRINT(pStripingInput->bg_tintless_input.bg_enabled);
    STRIPE_FIELD_PRINT(pStripingInput->bg_tintless_input.bg_rgn_v_num);
    STRIPE_FIELD_PRINT(pStripingInput->bg_tintless_input.bg_rgn_h_num);
    STRIPE_FIELD_PRINT(pStripingInput->bg_tintless_input.bg_region_sampling);
    STRIPE_FIELD_PRINT(pStripingInput->bg_tintless_input.bg_rgn_width);
    STRIPE_FIELD_PRINT(pStripingInput->bg_tintless_input.bg_roi_h_offset);
    STRIPE_FIELD_PRINT(pStripingInput->bg_tintless_input.bg_sat_output_enable);
    STRIPE_FIELD_PRINT(pStripingInput->bg_tintless_input.bg_Y_output_enable);
    STRIPE_FIELD_PRINT(pStripingInput->bg_awb_input.bg_enabled);
    STRIPE_FIELD_PRINT(pStripingInput->bg_awb_input.bg_rgn_v_num);
    STRIPE_FIELD_PRINT(pStripingInput->bg_awb_input.bg_rgn_h_num);
    STRIPE_FIELD_PRINT(pStripingInput->bg_awb_input.bg_region_sampling);
    STRIPE_FIELD_PRINT(pStripingInput->bg_awb_input.bg_rgn_width);
    STRIPE_FIELD_PRINT(pStripingInput->bg_awb_input.bg_roi_h_offset);
    STRIPE_FIELD_PRINT(pStripingInput->bg_awb_input.bg_sat_output_enable);
    STRIPE_FIELD_PRINT(pStripingInput->bg_awb_input.bg_Y_output_enable);
    STRIPE_FIELD_PRINT(pStripingInput->be_input.be_enable);
    STRIPE_FIELD_PRINT(pStripingInput->be_input.be_zHDR_first_rb_exp);
    STRIPE_FIELD_PRINT(pStripingInput->be_input.be_roi_h_offset);
    STRIPE_FIELD_PRINT(pStripingInput->be_input.be_rgn_width);
    STRIPE_FIELD_PRINT(pStripingInput->be_input.be_rgn_h_num);
    STRIPE_FIELD_PRINT(pStripingInput->be_input.be_rgn_v_num);
    STRIPE_FIELD_PRINT(pStripingInput->be_input.be_sat_output_enable);
    STRIPE_FIELD_PRINT(pStripingInput->be_input.be_Y_output_enable);
    for (UINT32 idx = 0; idx < BF_STATS_RGNCOUNT_V23; idx++ )
    {
        STRIPE_FIELD_PRINT_LL(pStripingInput->baf_input.baf_roi_indexLUT[idx]);
    }
    STRIPE_FIELD_PRINT(pStripingInput->baf_input.baf_h_scaler_en);
    STRIPE_FIELD_PRINT(pStripingInput->baf_input.baf_fir_h1_en);
    STRIPE_FIELD_PRINT(pStripingInput->baf_input.baf_iir_h1_en);
    STRIPE_FIELD_PRINT(pStripingInput->baf_input.mndsParam.rounding_option_h);
    STRIPE_FIELD_PRINT(pStripingInput->baf_input.mndsParam.rounding_option_v);
    STRIPE_FIELD_PRINT(pStripingInput->rscs_input.rs_enable);
    STRIPE_FIELD_PRINT(pStripingInput->rscs_input.cs_enable);
    STRIPE_FIELD_PRINT(pStripingInput->rscs_input.rs_rgn_h_offset);
    STRIPE_FIELD_PRINT(pStripingInput->rscs_input.rs_rgn_width);
    STRIPE_FIELD_PRINT(pStripingInput->rscs_input.rs_rgn_h_num);
    STRIPE_FIELD_PRINT(pStripingInput->rscs_input.rs_rgn_v_num);
    STRIPE_FIELD_PRINT(pStripingInput->rscs_input.cs_rgn_h_offset);
    STRIPE_FIELD_PRINT(pStripingInput->rscs_input.cs_rgn_width);
    STRIPE_FIELD_PRINT(pStripingInput->rscs_input.cs_rgn_h_num);
    STRIPE_FIELD_PRINT(pStripingInput->rscs_input.cs_rgn_v_num);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// DualIFEUtils::PrintDualIfeOutput
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID DualIFEUtils::PrintDualIfeOutput(
    const IFEStripeOutput* pStripingOutput)
{
    STRIPE_FIELD_PRINT(pStripingOutput->edgeStripeLT);
    STRIPE_FIELD_PRINT(pStripingOutput->edgeStripeRB);
    STRIPE_FIELD_PRINT(pStripingOutput->fetchFirstPixel);
    STRIPE_FIELD_PRINT(pStripingOutput->fetchLastPixel);
    STRIPE_FIELD_PRINT(pStripingOutput->hvxTapoffFirstPixel);
    STRIPE_FIELD_PRINT(pStripingOutput->hvxTapoffLastPixel);
    STRIPE_FIELD_PRINT(pStripingOutput->outRange_fd[0]);
    STRIPE_FIELD_PRINT(pStripingOutput->outRange_fd[1]);
    STRIPE_FIELD_PRINT(pStripingOutput->outRange_full[0]);
    STRIPE_FIELD_PRINT(pStripingOutput->outRange_full[1]);
    STRIPE_FIELD_PRINT(pStripingOutput->outRange_1to4[0]);
    STRIPE_FIELD_PRINT(pStripingOutput->outRange_1to4[1]);
    STRIPE_FIELD_PRINT(pStripingOutput->outRange_1to16[0]);
    STRIPE_FIELD_PRINT(pStripingOutput->outRange_1to16[1]);
    STRIPE_FIELD_PRINT(pStripingOutput->outRange_disp[0]);
    STRIPE_FIELD_PRINT(pStripingOutput->outRange_disp[1]);
    STRIPE_FIELD_PRINT(pStripingOutput->outRange_1to4_disp[0]);
    STRIPE_FIELD_PRINT(pStripingOutput->outRange_1to4_disp[1]);
    STRIPE_FIELD_PRINT(pStripingOutput->outRange_1to16_disp[0]);
    STRIPE_FIELD_PRINT(pStripingOutput->outRange_1to16_disp[1]);
    STRIPE_FIELD_PRINT(pStripingOutput->outRange_raw[0]);
    STRIPE_FIELD_PRINT(pStripingOutput->outRange_raw[1]);
    STRIPE_FIELD_PRINT(pStripingOutput->outRange_pdaf[0]);
    STRIPE_FIELD_PRINT(pStripingOutput->outRange_pdaf[1]);

    // crop amounts
    PRINT_CROP_1D(pStripingOutput->outCrop_fd_Y);
    PRINT_CROP_1D(pStripingOutput->outCrop_fd_C);
    PRINT_CROP_1D(pStripingOutput->outCrop_full_Y);
    PRINT_CROP_1D(pStripingOutput->outCrop_full_C);
    PRINT_CROP_1D(pStripingOutput->outCrop_1to4_Y);
    PRINT_CROP_1D(pStripingOutput->outCrop_1to4_C);
    PRINT_CROP_1D(pStripingOutput->outCrop_1to16_Y);
    PRINT_CROP_1D(pStripingOutput->outCrop_1to16_C);
    PRINT_CROP_1D(pStripingOutput->outCrop_raw);
    PRINT_CROP_1D(pStripingOutput->preDS4Crop_1to4_Y);
    PRINT_CROP_1D(pStripingOutput->preDS4Crop_1to4_C);
    PRINT_CROP_1D(pStripingOutput->preDS4Crop_1to16_Y);
    PRINT_CROP_1D(pStripingOutput->preDS4Crop_1to16_C);
    PRINT_CROP_1D(pStripingOutput->outCrop_Y_disp);
    PRINT_CROP_1D(pStripingOutput->outCrop_C_disp);
    PRINT_CROP_1D(pStripingOutput->outCrop_1to4_Y_disp);
    PRINT_CROP_1D(pStripingOutput->outCrop_1to4_C_disp);
    PRINT_CROP_1D(pStripingOutput->outCrop_1to16_Y_disp);
    PRINT_CROP_1D(pStripingOutput->outCrop_1to16_C_disp);
    PRINT_CROP_1D(pStripingOutput->preDS4Crop_1to4_Y_disp);
    PRINT_CROP_1D(pStripingOutput->preDS4Crop_1to4_C_disp);
    PRINT_CROP_1D(pStripingOutput->preDS4Crop_1to16_Y_disp);
    PRINT_CROP_1D(pStripingOutput->preDS4Crop_1to16_C_disp);

    //// scalers
    PRINT_MNDS_V16(pStripingOutput->mndsConfig_y_fd);
    PRINT_MNDS_V16(pStripingOutput->mndsConfig_c_fd);
    PRINT_MNDS_V16(pStripingOutput->mndsConfig_y_full);
    PRINT_MNDS_V16(pStripingOutput->mndsConfig_c_full);
    PRINT_MNDS_V16(pStripingOutput->bafDownscaler);
    PRINT_MNDS_V16(pStripingOutput->mndsConfig_y_disp);
    PRINT_MNDS_V16(pStripingOutput->mndsConfig_c_disp);

    STRIPE_FIELD_PRINT(pStripingOutput->rolloffOutStripe.grid_index);
    STRIPE_FIELD_PRINT(pStripingOutput->rolloffOutStripe.subgrid_index);
    STRIPE_FIELD_PRINT(pStripingOutput->rolloffOutStripe.pixel_index_within_subgrid);
    STRIPE_FIELD_PRINT(pStripingOutput->pedestalOutStripe.grid_index);
    STRIPE_FIELD_PRINT(pStripingOutput->pedestalOutStripe.subgrid_index);
    STRIPE_FIELD_PRINT(pStripingOutput->pedestalOutStripe.pixel_index_within_subgrid);

    PRINT_WE_STRIPE(pStripingOutput->bg_tintlessWriteEngineStripeParam);
    PRINT_WE_STRIPE(pStripingOutput->bg_awbWriteEngineStripeParam);
    PRINT_WE_STRIPE(pStripingOutput->beWriteEngineStripeParam);
    PRINT_WE_STRIPE(pStripingOutput->bafWriteEngineStripeParam);
    PRINT_WE_STRIPE(pStripingOutput->rowSumWriteEngineStripeParam);
    PRINT_WE_STRIPE(pStripingOutput->colSumWriteEngineStripeParam);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// DualIFEUtils::PrintDualIfeFrame
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID DualIFEUtils::PrintDualIfeFrame(
    const IFEPassOutput* pIfePassOutput)
{
    IFEStripeOutput* pStripingOutputStripes[2] = {0, };

    ListNode* pNode = List_GetTail(&pIfePassOutput->hStripeList);
    if (NULL != pNode)
    {
        pStripingOutputStripes[0] = static_cast<IFEStripeOutput*>(pNode->pData);
        pNode = List_GetPrevNode(pNode);
        if (NULL != pNode)
        {
            pStripingOutputStripes[1] = static_cast<IFEStripeOutput*>(pNode->pData);

            for (UINT i = 0; i < 2; i++)
            {
                CAMX_LOG_INFO(CamxLogGroupISP, "stripe %d output start", i);
                PrintDualIfeOutput(pStripingOutputStripes[i]);
            }
        }
        else
        {
            CAMX_LOG_ERROR(CamxLogGroupISP, "Invalid stripe list previous node");
        }
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupISP, "Invalid stripe list tail node");
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// DualIFEUtils::UpdateDualIFEConfig
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult DualIFEUtils::UpdateDualIFEConfig(
    ISPInputData*       pISPInputdata,
    IFEPDAFInfo         PDAFInfo,
    ISPStripeConfig*    pStripeConfig,
    DualIFESplitParams* pSplitParams,
    IFEPassOutput*      pPassOut)
{
    CamxResult            result                           = CamxResultSuccess;
    const StaticSettings* pSettings                        = HwEnvironment::GetInstance()->GetStaticSettings();
    UINT32                splitPoint                       = pSplitParams->splitPoint;
    UINT32                rightPadding                     = pSplitParams->rightPadding;
    UINT32                leftPadding                      = pSplitParams->leftPadding;
    FLOAT                 scaleratio                       = 1.0f;
    FLOAT                 splitRatio[IFEMaxNonCommonPaths] = { 0.0f };
    UINT32                fullAlign                        = 2;
    BOOL                  midNextAlignemnt                 = TRUE;
    UINT32                rightStripeFirstPixel            = 0;

    UINT32 leftWidth    = splitPoint + rightPadding;
    UINT32 rightWidth   = pISPInputdata->sensorData.sensorOut.width -
                          splitPoint + leftPadding;
    FLOAT inputSplitRatio = static_cast<FLOAT>(pSplitParams->splitPoint) / pISPInputdata->sensorData.sensorOut.width;

    // Update stats module output data
    pStripeConfig[0].AECUpdateData = *pISPInputdata->pAECUpdateData;
    pStripeConfig[1].AECUpdateData = *pISPInputdata->pAECUpdateData;
    pStripeConfig[0].AWBUpdateData = *pISPInputdata->pAWBUpdateData;
    pStripeConfig[1].AWBUpdateData = *pISPInputdata->pAWBUpdateData;

    if (TRUE == pISPInputdata->enableIFEDualStripeLog)
    {
        STRIPE_FIELD_PRINT(pISPInputdata->pStripingInput->stripingInput.tiering);
    }

    if (TRUE == pSettings->enableDualIFEOverwrite)
    {
        GetDefaultDualIFEStatsConfig(pISPInputdata, pStripeConfig, pSplitParams);
    }
    else
    {
        result = UpdateStripingInput(pISPInputdata);
        if (CamxResultSuccess == result)
        {
            if (TRUE == pISPInputdata->enableIFEDualStripeLog)
            {
                PrintDualIfeInput(&pISPInputdata->pStripingInput->stripingInput);
            }

            stripingInput_titanIFE* pInput = &pISPInputdata->pStripingInput->stripingInput;
            // Use striping library to get the per stripe configuration
            result = deriveStriping_titanIFE(pInput, pPassOut);

            if (TRUE == pISPInputdata->enableIFEDualStripeLog)
            {
                PrintDualIfeFrame(pPassOut);
            }

            if (CamxResultSuccess == result)
            {
                DualIFEUtils::FetchCfgWithStripeOutput(pISPInputdata, pPassOut, pStripeConfig);
                if (pISPInputdata->HALData.stream[PixelRawOutput].width > 0)
                {
                    pStripeConfig[0].stream[PixelRawOutput].width     = pSplitParams->splitPoint;
                    pStripeConfig[0].stream[PixelRawOutput].height    =
                        Utils::EvenCeilingUINT32(pISPInputdata->HALData.stream[PixelRawOutput].height);
                    pStripeConfig[0].stream[PixelRawOutput].offset    = 0;

                    pStripeConfig[1].stream[PixelRawOutput].width     = pSplitParams->splitPoint;
                    pStripeConfig[1].stream[PixelRawOutput].height    =
                        Utils::EvenCeilingUINT32(pISPInputdata->HALData.stream[PixelRawOutput].height);
                    pStripeConfig[1].stream[PixelRawOutput].offset    = pISPInputdata->sensorData.sensorOut.width -
                        pSplitParams->splitPoint;

                }

                if (PDAFInfo.enableSubsample)
                {
                    UINT32 leftSplitWidth   = ((pSplitParams->splitPoint - 1) | 0xF) + 1;
                    UINT32 rightStripeStart = splitPoint - leftPadding;

                    pStripeConfig[0].CAMIFSubsampleInfo.PDAFCAMIFCrop.firstPixel = 0;
                    pStripeConfig[0].CAMIFSubsampleInfo.PDAFCAMIFCrop.lastPixel  = leftSplitWidth - 1;
                    pStripeConfig[0].CAMIFSubsampleInfo.PDAFCAMIFCrop.firstLine  = 0;
                    pStripeConfig[0].CAMIFSubsampleInfo.PDAFCAMIFCrop.lastLine   =
                        pISPInputdata->sensorData.sensorOut.height - 1;
                    pStripeConfig[0].stream[PDAFRawOutput].offset                = 0;
                    pStripeConfig[0].stream[PDAFRawOutput].width                 =
                        (leftSplitWidth / 16) * GetPixelsInSkipPattern(PDAFInfo.pixelSkipPattern);
                    pStripeConfig[0].stream[PDAFRawOutput].height                =
                        PDAFInfo.bufferHeight;

                    pStripeConfig[1].CAMIFSubsampleInfo.PDAFCAMIFCrop.firstPixel = leftSplitWidth - rightStripeStart;
                    pStripeConfig[1].CAMIFSubsampleInfo.PDAFCAMIFCrop.lastPixel  = rightWidth - 1;
                    pStripeConfig[1].CAMIFSubsampleInfo.PDAFCAMIFCrop.firstLine  = 0;
                    pStripeConfig[1].CAMIFSubsampleInfo.PDAFCAMIFCrop.lastLine   =
                        pISPInputdata->sensorData.sensorOut.height - 1;
                    pStripeConfig[1].stream[PDAFRawOutput].offset                =
                        pStripeConfig[0].stream[PDAFRawOutput].width;
                    pStripeConfig[1].stream[PDAFRawOutput].width                 =
                        PDAFInfo.bufferWidth - pStripeConfig[0].stream[PDAFRawOutput].width;
                    pStripeConfig[1].stream[PDAFRawOutput].height                =
                        PDAFInfo.bufferHeight;
                }
            }
        }
    }
    if (CamxResultSuccess == result)
    {
        pStripeConfig[0].CAMIFCrop  = {0, 0, (leftWidth - 1), (pISPInputdata->sensorData.sensorOut.height - 1) };
        pStripeConfig[1].CAMIFCrop  =
        {
            pISPInputdata->sensorData.sensorOut.width - rightWidth,
            0,
            (pISPInputdata->sensorData.sensorOut.width - 1),
            (pISPInputdata->sensorData.sensorOut.height - 1)
        };
        pStripeConfig[0].stateHVX.overlap = rightPadding - leftPadding + 1;
        pStripeConfig[1].stateHVX.overlap = rightPadding - leftPadding + 1;
        pStripeConfig[0].stateHVX.rightImageOffset = rightPadding;
        pStripeConfig[1].stateHVX.rightImageOffset = rightPadding;


        if (TRUE == pISPInputdata->HVXData.DSEnabled)
        {
            scaleratio = static_cast<FLOAT>(pISPInputdata->HVXData.HVXOut.width) /
                static_cast<FLOAT>(pISPInputdata->sensorData.sensorOut.width);
        }
        splitPoint   = static_cast<UINT32>(scaleratio * pSplitParams->splitPoint);
        rightPadding = static_cast<UINT32>(scaleratio * pSplitParams->rightPadding);
        leftPadding  = static_cast<UINT32>(scaleratio * pSplitParams->leftPadding);
        pStripeConfig[0].stateHVX.inputWidth = leftWidth;
        pStripeConfig[1].stateHVX.inputWidth = rightWidth;
        pStripeConfig[0].stateHVX.inputHeight = pISPInputdata->sensorData.sensorOut.height;
        pStripeConfig[1].stateHVX.inputHeight = pISPInputdata->sensorData.sensorOut.height;

        pStripeConfig[0].stateHVX.hvxOutDimension.width = static_cast<UINT32>(scaleratio * leftWidth);
        pStripeConfig[1].stateHVX.hvxOutDimension.width = static_cast<UINT32>(scaleratio * rightWidth);
        pStripeConfig[0].stateHVX.hvxOutDimension.height = pISPInputdata->HVXData.HVXOut.height;
        pStripeConfig[1].stateHVX.hvxOutDimension.height = pISPInputdata->HVXData.HVXOut.height;


        pStripeConfig[0].stateHVX.cropWindow =
        {
            0,
            0,
            (pStripeConfig[0].stateHVX.hvxOutDimension.width - 1),
            (pISPInputdata->HVXData.HVXOut.height - 1)
        };
        pStripeConfig[1].stateHVX.cropWindow =
        {
            pStripeConfig[0].stateHVX.hvxOutDimension.width - pStripeConfig[1].stateHVX.hvxOutDimension.width,
            0,
            (pISPInputdata->HVXData.HVXOut.width - 1),
            (pISPInputdata->HVXData.HVXOut.height - 1)
        };

        if (TRUE == pISPInputdata->HVXData.DSEnabled)
        {
            rightStripeFirstPixel = pStripeConfig[1].stateHVX.cropWindow.firstPixel;
        }
        else
        {
            rightStripeFirstPixel = pStripeConfig[1].CAMIFCrop.firstPixel;
        }

        CAMX_LOG_VERBOSE(CamxLogGroupISP,
                            "Stripinglib stripe (LEFT) output dim (offset, w, h):"
                            "full(%d,%d,%d), fd(%d,%d,%d), ds4(%d,%d,%d), ds16(%d,%d,%d) pixelRaw(%d,%d,%d)"
                            "disp(%d,%d,%d), ds4(%d,%d,%d), ds16(%d,%d,%d)",
                            pStripeConfig[0].stream[FullOutput].offset,
                            pStripeConfig[0].stream[FullOutput].width,
                            pStripeConfig[0].stream[FullOutput].height,
                            pStripeConfig[0].stream[FDOutput].offset,
                            pStripeConfig[0].stream[FDOutput].width,
                            pStripeConfig[0].stream[FDOutput].height,
                            pStripeConfig[0].stream[DS4Output].offset,
                            pStripeConfig[0].stream[DS4Output].width,
                            pStripeConfig[0].stream[DS4Output].height,
                            pStripeConfig[0].stream[DS16Output].offset,
                            pStripeConfig[0].stream[DS16Output].width,
                            pStripeConfig[0].stream[DS16Output].height,
                            pStripeConfig[0].stream[PixelRawOutput].offset,
                            pStripeConfig[0].stream[PixelRawOutput].width,
                            pStripeConfig[0].stream[PixelRawOutput].height,
                            pStripeConfig[0].stream[DisplayFullOutput].offset,
                            pStripeConfig[0].stream[DisplayFullOutput].width,
                            pStripeConfig[0].stream[DisplayFullOutput].height,
                            pStripeConfig[0].stream[DisplayDS4Output].offset,
                            pStripeConfig[0].stream[DisplayDS4Output].width,
                            pStripeConfig[0].stream[DisplayDS4Output].height,
                            pStripeConfig[0].stream[DisplayDS16Output].offset,
                            pStripeConfig[0].stream[DisplayDS16Output].width,
                            pStripeConfig[0].stream[DisplayDS16Output].height);

        CAMX_LOG_VERBOSE(CamxLogGroupISP,
                            "Stripinglib stripe (RIGHT) output dim (offset, w, h):"
                            "full(%d,%d,%d), fd(%d,%d,%d), ds4(%d,%d,%d), ds16(%d,%d,%d) pixelRaw(%d,%d,%d)"
                            "disp(%d,%d,%d), ds4(%d,%d,%d), ds16(%d,%d,%d)",
                            pStripeConfig[1].stream[FullOutput].offset,
                            pStripeConfig[1].stream[FullOutput].width,
                            pStripeConfig[1].stream[FullOutput].height,
                            pStripeConfig[1].stream[FDOutput].offset,
                            pStripeConfig[1].stream[FDOutput].width,
                            pStripeConfig[1].stream[FDOutput].height,
                            pStripeConfig[1].stream[DS4Output].offset,
                            pStripeConfig[1].stream[DS4Output].width,
                            pStripeConfig[1].stream[DS4Output].height,
                            pStripeConfig[1].stream[DS16Output].offset,
                            pStripeConfig[1].stream[DS16Output].width,
                            pStripeConfig[1].stream[DS16Output].height,
                            pStripeConfig[1].stream[PixelRawOutput].offset,
                            pStripeConfig[1].stream[PixelRawOutput].width,
                            pStripeConfig[1].stream[PixelRawOutput].height,
                            pStripeConfig[1].stream[DisplayFullOutput].offset,
                            pStripeConfig[1].stream[DisplayFullOutput].width,
                            pStripeConfig[1].stream[DisplayFullOutput].height,
                            pStripeConfig[1].stream[DisplayDS4Output].offset,
                            pStripeConfig[1].stream[DisplayDS4Output].width,
                            pStripeConfig[1].stream[DisplayDS4Output].height,
                            pStripeConfig[1].stream[DisplayDS16Output].offset,
                            pStripeConfig[1].stream[DisplayDS16Output].width,
                            pStripeConfig[1].stream[DisplayDS16Output].height);

        CAMX_ASSERT(0 != pISPInputdata->sensorData.sensorOut.width);
        splitRatio[FullOutput] = (static_cast<FLOAT>(pSplitParams->splitPoint) / pISPInputdata->sensorData.sensorOut.width);
        CropWindow halCrop = pISPInputdata->pHALTagsData->HALCrop;

        splitRatio[FullOutput]                      = static_cast<FLOAT>(pStripeConfig[0].stream[FullOutput].width) /
            pISPInputdata->HALData.stream[FullOutput].width;
        pStripeConfig[0].HALCrop[FullOutput].height = halCrop.height;
        pStripeConfig[0].HALCrop[FullOutput].top    = halCrop.top;
        pStripeConfig[0].HALCrop[FullOutput].left   = halCrop.left;
        pStripeConfig[0].HALCrop[FullOutput].width  = static_cast<UINT32>(inputSplitRatio * halCrop.width);

        splitRatio[FDOutput]                        = static_cast<FLOAT>(pStripeConfig[0].stream[FDOutput].width) /
            pISPInputdata->HALData.stream[FDOutput].width;
        pStripeConfig[0].HALCrop[FDOutput].height = halCrop.height;
        pStripeConfig[0].HALCrop[FDOutput].top    = halCrop.top;
        pStripeConfig[0].HALCrop[FDOutput].left   = halCrop.left;
        pStripeConfig[0].HALCrop[FDOutput].width  = static_cast<UINT32>(inputSplitRatio * halCrop.width);

        splitRatio[DS4Output]                       = static_cast<FLOAT>(pStripeConfig[0].stream[DS4Output].width) /
            pISPInputdata->HALData.stream[DS4Output].width;
        pStripeConfig[0].HALCrop[DS4Output].height = halCrop.height;
        pStripeConfig[0].HALCrop[DS4Output].top    = halCrop.top;
        pStripeConfig[0].HALCrop[DS4Output].left   = halCrop.left;
        pStripeConfig[0].HALCrop[DS4Output].width  = static_cast<UINT32>(inputSplitRatio * halCrop.width);

        splitRatio[DS16Output]                      = static_cast<FLOAT>(pStripeConfig[0].stream[DS16Output].width) /
            pISPInputdata->HALData.stream[DS16Output].width;
        pStripeConfig[0].HALCrop[DS16Output].height = halCrop.height;
        pStripeConfig[0].HALCrop[DS16Output].top    = halCrop.top;
        pStripeConfig[0].HALCrop[DS16Output].left   = halCrop.left;
        pStripeConfig[0].HALCrop[DS16Output].width  = static_cast<UINT32>(inputSplitRatio * halCrop.width);

        if (pISPInputdata->HALData.stream[PixelRawOutput].width > 0)
        {
            pStripeConfig[0].HALCrop[PixelRawOutput].height = pISPInputdata->sensorData.sensorOut.height;
            pStripeConfig[0].HALCrop[PixelRawOutput].top    = 0;
            pStripeConfig[0].HALCrop[PixelRawOutput].left   = 0;
            pStripeConfig[0].HALCrop[PixelRawOutput].width  = pStripeConfig[0].stream[PixelRawOutput].width;
        }

        splitRatio[DisplayFullOutput]                      =
            static_cast<FLOAT>(pStripeConfig[0].stream[DisplayFullOutput].width) /
            pISPInputdata->HALData.stream[DisplayFullOutput].width;
        pStripeConfig[0].HALCrop[DisplayFullOutput].height = halCrop.height;
        pStripeConfig[0].HALCrop[DisplayFullOutput].top    = halCrop.top;
        pStripeConfig[0].HALCrop[DisplayFullOutput].left   = halCrop.left;
        pStripeConfig[0].HALCrop[DisplayFullOutput].width  = static_cast<UINT32>(inputSplitRatio * halCrop.width);

        splitRatio[DisplayDS4Output]                      =
            static_cast<FLOAT>(pStripeConfig[0].stream[DisplayDS4Output].width) /
            pISPInputdata->HALData.stream[DisplayDS4Output].width;
        pStripeConfig[0].HALCrop[DisplayDS4Output].height = halCrop.height;
        pStripeConfig[0].HALCrop[DisplayDS4Output].top    = halCrop.top;
        pStripeConfig[0].HALCrop[DisplayDS4Output].left   = halCrop.left;
        pStripeConfig[0].HALCrop[DisplayDS4Output].width  = static_cast<UINT32>(inputSplitRatio * halCrop.width);

        splitRatio[DisplayDS16Output]                      =
            static_cast<FLOAT>(pStripeConfig[0].stream[DisplayDS16Output].width) /
            pISPInputdata->HALData.stream[DisplayDS16Output].width;
        pStripeConfig[0].HALCrop[DisplayDS16Output].height = halCrop.height;
        pStripeConfig[0].HALCrop[DisplayDS16Output].top    = halCrop.top;
        pStripeConfig[0].HALCrop[DisplayDS16Output].left   = halCrop.left;
        pStripeConfig[0].HALCrop[DisplayDS16Output].width  = static_cast<UINT32>(inputSplitRatio * halCrop.width);

        // HAL crop should be releative to CAMIF crop
        pStripeConfig[1].HALCrop[FDOutput].height   = halCrop.height;
        pStripeConfig[1].HALCrop[FDOutput].top      = halCrop.top;
        pStripeConfig[1].HALCrop[FDOutput].left     = pSplitParams->leftPadding;
        pStripeConfig[1].HALCrop[FDOutput].width    = halCrop.width - pStripeConfig[0].HALCrop[FDOutput].width;

        pStripeConfig[1].HALCrop[FullOutput].height = halCrop.height;
        pStripeConfig[1].HALCrop[FullOutput].top    = halCrop.top;
        pStripeConfig[1].HALCrop[FullOutput].left   = pSplitParams->leftPadding;
        pStripeConfig[1].HALCrop[FullOutput].width  = halCrop.width - pStripeConfig[0].HALCrop[FullOutput].width;

        pStripeConfig[1].HALCrop[DS4Output].height  = halCrop.height;
        pStripeConfig[1].HALCrop[DS4Output].top     = halCrop.top;
        pStripeConfig[1].HALCrop[DS4Output].left    = pSplitParams->leftPadding;
        pStripeConfig[1].HALCrop[DS4Output].width   = halCrop.width - pStripeConfig[0].HALCrop[DS4Output].width;

        pStripeConfig[1].HALCrop[DS16Output].height = halCrop.height;
        pStripeConfig[1].HALCrop[DS16Output].top    = halCrop.top;
        pStripeConfig[1].HALCrop[DS16Output].left   = pSplitParams->leftPadding;
        pStripeConfig[1].HALCrop[DS16Output].width  = halCrop.width - pStripeConfig[0].HALCrop[DS16Output].width;

        if (pISPInputdata->HALData.stream[PixelRawOutput].width > 0)
        {
            pStripeConfig[1].HALCrop[PixelRawOutput].height = pISPInputdata->sensorData.sensorOut.height;
            pStripeConfig[1].HALCrop[PixelRawOutput].top    = 0;
            pStripeConfig[1].HALCrop[PixelRawOutput].left   = pSplitParams->leftPadding;
            pStripeConfig[1].HALCrop[PixelRawOutput].width  = pStripeConfig[1].stream[PixelRawOutput].width;
        }

        pStripeConfig[1].HALCrop[DisplayFullOutput].height = halCrop.height;
        pStripeConfig[1].HALCrop[DisplayFullOutput].top    = halCrop.top;
        pStripeConfig[1].HALCrop[DisplayFullOutput].left   = pSplitParams->leftPadding;
        pStripeConfig[1].HALCrop[DisplayFullOutput].width  = halCrop.width - pStripeConfig[0].HALCrop[DisplayFullOutput].width;

        pStripeConfig[1].HALCrop[DisplayDS4Output].height  = halCrop.height;
        pStripeConfig[1].HALCrop[DisplayDS4Output].top     = halCrop.top;
        pStripeConfig[1].HALCrop[DisplayDS4Output].left    = pSplitParams->leftPadding;
        pStripeConfig[1].HALCrop[DisplayDS4Output].width   = halCrop.width - pStripeConfig[0].HALCrop[DisplayDS4Output].width;

        pStripeConfig[1].HALCrop[DisplayDS16Output].height = halCrop.height;
        pStripeConfig[1].HALCrop[DisplayDS16Output].top    = halCrop.top;
        pStripeConfig[1].HALCrop[DisplayDS16Output].left   = pSplitParams->leftPadding;
        pStripeConfig[1].HALCrop[DisplayDS16Output].width  = halCrop.width - pStripeConfig[0].HALCrop[DisplayDS16Output].width;

        CAMX_LOG_INFO(CamxLogGroupISP,
                            "<dualIFE> Overwritten stripe (LEFT) output dim (offset, w, h):"
                            "full(%d,%d,%d), fd(%d,%d,%d), ds4(%d,%d,%d), ds16(%d,%d,%d)"
                            "disp(%d,%d,%d), ds4(%d,%d,%d), ds16(%d,%d,%d)",
                            pStripeConfig[0].stream[FullOutput].offset,
                            pStripeConfig[0].stream[FullOutput].width,
                            pStripeConfig[0].stream[FullOutput].height,
                            pStripeConfig[0].stream[FDOutput].offset,
                            pStripeConfig[0].stream[FDOutput].width,
                            pStripeConfig[0].stream[FDOutput].height,
                            pStripeConfig[0].stream[DS4Output].offset,
                            pStripeConfig[0].stream[DS4Output].width,
                            pStripeConfig[0].stream[DS4Output].height,
                            pStripeConfig[0].stream[DS16Output].offset,
                            pStripeConfig[0].stream[DS16Output].width,
                            pStripeConfig[0].stream[DS16Output].height,
                            pStripeConfig[0].stream[DisplayFullOutput].offset,
                            pStripeConfig[0].stream[DisplayFullOutput].width,
                            pStripeConfig[0].stream[DisplayFullOutput].height,
                            pStripeConfig[0].stream[DisplayDS4Output].offset,
                            pStripeConfig[0].stream[DisplayDS4Output].width,
                            pStripeConfig[0].stream[DisplayDS4Output].height,
                            pStripeConfig[0].stream[DisplayDS16Output].offset,
                            pStripeConfig[0].stream[DisplayDS16Output].width,
                            pStripeConfig[0].stream[DisplayDS16Output].height);

        CAMX_LOG_INFO(CamxLogGroupISP,
                            "Overwritten stripe (RIGHT) output dim (offset, w, h):"
                            "full(%d,%d,%d), fd(%d,%d,%d), ds4(%d,%d,%d), ds16(%d,%d,%d)"
                            "disp(%d,%d,%d), ds4(%d,%d,%d), ds16(%d,%d,%d)",
                            pStripeConfig[1].stream[FullOutput].offset,
                            pStripeConfig[1].stream[FullOutput].width,
                            pStripeConfig[1].stream[FullOutput].height,
                            pStripeConfig[1].stream[FDOutput].offset,
                            pStripeConfig[1].stream[FDOutput].width,
                            pStripeConfig[1].stream[FDOutput].height,
                            pStripeConfig[1].stream[DS4Output].offset,
                            pStripeConfig[1].stream[DS4Output].width,
                            pStripeConfig[1].stream[DS4Output].height,
                            pStripeConfig[1].stream[DS16Output].offset,
                            pStripeConfig[1].stream[DS16Output].width,
                            pStripeConfig[1].stream[DS16Output].height,
                            pStripeConfig[1].stream[DisplayFullOutput].offset,
                            pStripeConfig[1].stream[DisplayFullOutput].width,
                            pStripeConfig[1].stream[DisplayFullOutput].height,
                            pStripeConfig[1].stream[DisplayDS4Output].offset,
                            pStripeConfig[1].stream[DisplayDS4Output].width,
                            pStripeConfig[1].stream[DisplayDS4Output].height,
                            pStripeConfig[1].stream[DisplayDS16Output].offset,
                            pStripeConfig[1].stream[DisplayDS16Output].width,
                            pStripeConfig[1].stream[DisplayDS16Output].height);

        // Update left and right stripe width for each of the stats module
        pStripeConfig[0].stats[CSLIFEStatsPortIndex(CSLIFEPortIdStatHDRBE)].width       =
            pStripeConfig[0].AECStatsUpdateData.statsConfig.BEConfig.horizontalNum *
            pISPInputdata->pAECStatsUpdateData->statsConfig.BEConfig.verticalNum * HDRBEStatsOutputSizePerRegion;
        pStripeConfig[1].stats[CSLIFEStatsPortIndex(CSLIFEPortIdStatHDRBE)].width       =
            pStripeConfig[1].AECStatsUpdateData.statsConfig.BEConfig.horizontalNum *
            pISPInputdata->pAECStatsUpdateData->statsConfig.BEConfig.verticalNum * HDRBEStatsOutputSizePerRegion;

        pStripeConfig[0].stats[CSLIFEStatsPortIndex(CSLIFEPortIdStatTintlessBG)].width  =
            pStripeConfig[0].AECStatsUpdateData.statsConfig.TintlessBGConfig.horizontalNum *
            pISPInputdata->pAECStatsUpdateData->statsConfig.TintlessBGConfig.verticalNum * TintlessStatsOutputSizePerRegion;
        pStripeConfig[1].stats[CSLIFEStatsPortIndex(CSLIFEPortIdStatTintlessBG)].width  =
            pStripeConfig[1].AECStatsUpdateData.statsConfig.TintlessBGConfig.horizontalNum *
            pISPInputdata->pAECStatsUpdateData->statsConfig.TintlessBGConfig.verticalNum * TintlessStatsOutputSizePerRegion;

        pStripeConfig[0].stats[CSLIFEStatsPortIndex(CSLIFEPortIdStatAWBBG)].width       =
            pStripeConfig[0].AWBStatsUpdateData.statsConfig.BGConfig.horizontalNum *
            pISPInputdata->pAWBStatsUpdateData->statsConfig.BGConfig.verticalNum * AWBBGStatsOutputSizePerRegion;
        pStripeConfig[1].stats[CSLIFEStatsPortIndex(CSLIFEPortIdStatAWBBG)].width       =
            pStripeConfig[1].AWBStatsUpdateData.statsConfig.BGConfig.horizontalNum *
            pISPInputdata->pAWBStatsUpdateData->statsConfig.BGConfig.verticalNum * AWBBGStatsOutputSizePerRegion;

        if (0 == pStripeConfig[0].AFStatsUpdateData.statsConfig.BFStats.BFStatsROIConfig.numBFStatsROIDimension)
        {
            pStripeConfig[0].stats[CSLIFEStatsPortIndex(CSLIFEPortIdStatBF)].width =
                (1 + BFStatsNumOfFramgTagEntry) * BFStatsOutputSizePerRegion;
        }
        else
        {
            pStripeConfig[0].stats[CSLIFEStatsPortIndex(CSLIFEPortIdStatBF)].width =
                (pStripeConfig[0].AFStatsUpdateData.statsConfig.BFStats.BFStatsROIConfig.numBFStatsROIDimension
                    + BFStatsNumOfFramgTagEntry) * BFStatsOutputSizePerRegion;
        }

        if (0 == pStripeConfig[1].AFStatsUpdateData.statsConfig.BFStats.BFStatsROIConfig.numBFStatsROIDimension)
        {
            pStripeConfig[1].stats[CSLIFEStatsPortIndex(CSLIFEPortIdStatBF)].width =
                (1 + BFStatsNumOfFramgTagEntry) * BFStatsOutputSizePerRegion;
        }
        else
        {
            pStripeConfig[1].stats[CSLIFEStatsPortIndex(CSLIFEPortIdStatBF)].width =
                (pStripeConfig[1].AFStatsUpdateData.statsConfig.BFStats.BFStatsROIConfig.numBFStatsROIDimension
                    + BFStatsNumOfFramgTagEntry) * BFStatsOutputSizePerRegion;
        }


        CAMX_LOG_INFO(CamxLogGroupISP,
                            "<dualIFE> Overwritten stripe (LEFT) ouput dim (offset, w, h):"
                            "full(%d,%d,%d), fd(%d,%d,%d), ds4(%d,%d,%d), ds16(%d,%d,%d) BF(%d %d %d noROI %d)",
                            pStripeConfig[0].stream[FullOutput].offset,
                            pStripeConfig[0].stream[FullOutput].width,
                            pStripeConfig[0].stream[FullOutput].height,
                            pStripeConfig[0].stream[FDOutput].offset,
                            pStripeConfig[0].stream[FDOutput].width,
                            pStripeConfig[0].stream[FDOutput].height,
                            pStripeConfig[0].stream[DS4Output].offset,
                            pStripeConfig[0].stream[DS4Output].width,
                            pStripeConfig[0].stream[DS4Output].height,
                            pStripeConfig[0].stream[DS16Output].offset,
                            pStripeConfig[0].stream[DS16Output].width,
                            pStripeConfig[0].stream[DS16Output].height,
                            pStripeConfig[0].stats[CSLIFEStatsPortIndex(CSLIFEPortIdStatBF)].width,
                            pStripeConfig[0].stats[CSLIFEStatsPortIndex(CSLIFEPortIdStatBF)].height,
                            pStripeConfig[0].stats[CSLIFEStatsPortIndex(CSLIFEPortIdStatBF)].offset,
                            pStripeConfig[0].AFStatsUpdateData.statsConfig.BFStats.BFStatsROIConfig.numBFStatsROIDimension);

        CAMX_LOG_INFO(CamxLogGroupISP,
                            "Overwritten stripe (RIGHT) ouput dim (offset, w, h):"
                            "full(%d,%d,%d), fd(%d,%d,%d), ds4(%d,%d,%d), ds16(%d,%d,%d) BF(%d %d %d noROI %d)",
                            pStripeConfig[1].stream[FullOutput].offset,
                            pStripeConfig[1].stream[FullOutput].width,
                            pStripeConfig[1].stream[FullOutput].height,
                            pStripeConfig[1].stream[FDOutput].offset,
                            pStripeConfig[1].stream[FDOutput].width,
                            pStripeConfig[1].stream[FDOutput].height,
                            pStripeConfig[1].stream[DS4Output].offset,
                            pStripeConfig[1].stream[DS4Output].width,
                            pStripeConfig[1].stream[DS4Output].height,
                            pStripeConfig[1].stream[DS16Output].offset,
                            pStripeConfig[1].stream[DS16Output].width,
                            pStripeConfig[1].stream[DS16Output].height,
                            pStripeConfig[1].stats[CSLIFEStatsPortIndex(CSLIFEPortIdStatBF)].width,
                            pStripeConfig[1].stats[CSLIFEStatsPortIndex(CSLIFEPortIdStatBF)].height,
                            pStripeConfig[1].stats[CSLIFEStatsPortIndex(CSLIFEPortIdStatBF)].offset,
                            pStripeConfig[1].AFStatsUpdateData.statsConfig.BFStats.BFStatsROIConfig.numBFStatsROIDimension);

        pStripeConfig[0].stats[CSLIFEStatsPortIndex(CSLIFEPortIdStatBHIST)].width       = BHistStatsWidth;
        pStripeConfig[1].stats[CSLIFEStatsPortIndex(CSLIFEPortIdStatBHIST)].width       = BHistStatsWidth;

        pStripeConfig[0].stats[CSLIFEStatsPortIndex(CSLIFEPortIdStatHDRBHIST)].width    = HDRBHistStatsMaxWidth;
        pStripeConfig[1].stats[CSLIFEStatsPortIndex(CSLIFEPortIdStatHDRBHIST)].width    = HDRBHistStatsMaxWidth;

        pStripeConfig[0].stats[CSLIFEStatsPortIndex(CSLIFEPortIdStatIHIST)].width       = IHistStatsWidth;
        pStripeConfig[1].stats[CSLIFEStatsPortIndex(CSLIFEPortIdStatIHIST)].width       = IHistStatsWidth;

        pStripeConfig[0].stats[CSLIFEStatsPortIndex(CSLIFEPortIdStatRS)].width = RSStatsWidth;
        pStripeConfig[1].stats[CSLIFEStatsPortIndex(CSLIFEPortIdStatRS)].width = RSStatsWidth;

        pStripeConfig[0].stats[CSLIFEStatsPortIndex(CSLIFEPortIdStatCS)].width = CSStatsWidth;
        pStripeConfig[1].stats[CSLIFEStatsPortIndex(CSLIFEPortIdStatCS)].width = CSStatsWidth;

        pStripeConfig[0].stats[CSLIFEStatsPortIndex(CSLIFEPortIdStatTintlessBG)].width = TintlessBGStatsWidth;
        pStripeConfig[1].stats[CSLIFEStatsPortIndex(CSLIFEPortIdStatTintlessBG)].width = TintlessBGStatsWidth;

    }
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// DualIFEUtils::UpdateStripingInput
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult DualIFEUtils::UpdateStripingInput(
    ISPInputData* pISPInputdata)
{
    stripingInput_titanIFE* pInput = &pISPInputdata->pStripingInput->stripingInput;
    CamxResult              result = CamxResultSuccess;

    switch (pISPInputdata->titanVersion)
    {
        case CSLCameraTitanVersion::CSLTitan150:
            pInput->tiering = TITAN_150_V1;
            break;
        case CSLCameraTitanVersion::CSLTitan170:
            pInput->tiering = TITAN_170_V1;
            break;
        case CSLCameraTitanVersion::CSLTitan175:
        case CSLCameraTitanVersion::CSLTitan160:
            // Both CSLTitan175 and CSLTitan160 have the same IFE IQ h/w
            pInput->tiering = TITAN_175_V1;
            break;
        default:
            CAMX_LOG_ERROR(CamxLogGroupISP,
                           "Invalid Camera Titan Version",
                           pISPInputdata->titanVersion);
            result = CamxResultEUnsupported;
            break;
    }

    pInput->striping            = 1;
    pInput->inputFormat         = TranslateInputFormatToStripingLib(pISPInputdata->sensorData.format,
        pISPInputdata->sensorBitWidth);
    pInput->inputWidth          = static_cast<int16_t>(pISPInputdata->pStripeConfig->CAMIFCrop.lastPixel -
        pISPInputdata->pStripeConfig->CAMIFCrop.firstPixel + 1);
    pInput->inputHeight         = static_cast<int16_t>(pISPInputdata->pStripeConfig->CAMIFCrop.lastLine -
        pISPInputdata->pStripeConfig->CAMIFCrop.firstLine + 1);

#if DUALIFE_STRIPINGLIB_ZOOMS
    pInput->use_zoom_setting_from_external = FALSE;
    pInput->zoom_window.start_x = static_cast<int16_t>(pISPInputdata->pHALTagsData->HALCrop.left);
    pInput->zoom_window.start_y = static_cast<int16_t>(pISPInputdata->pHALTagsData->HALCrop.top);
    pInput->zoom_window.width   = static_cast<int16_t>(pISPInputdata->pHALTagsData->HALCrop.width);
    pInput->zoom_window.height  = static_cast<int16_t>(pISPInputdata->pHALTagsData->HALCrop.height);
#else
    pInput->use_zoom_setting_from_external  = TRUE;
    if (pISPInputdata->pCalculatedData->fullOutCrop.lastPixel == 0)
    {
        pISPInputdata->pCalculatedData->fullOutCrop.firstPixel                   = 0;
        pISPInputdata->pCalculatedData->fullOutCrop.lastPixel                    = pInput->inputWidth - 1;
        pISPInputdata->HALData.format[FullOutput]                                = Format::YUV420NV12;
        pISPInputdata->HALData.stream[FullOutput].width                          = pInput->inputWidth;
        pISPInputdata->HALData.stream[FullOutput].height                         = pInput->inputHeight;
        pISPInputdata->HALData.format[DS4Output ]                                = Format::PD10;
        pISPInputdata->HALData.format[DS16Output ]                               = Format::PD10;
        pISPInputdata->pCalculatedData->scalerOutput[FullOutput].dimension.width = pInput->inputWidth;
        pISPInputdata->pCalculatedData->scalerOutput[FullOutput].input.width     = pInput->inputWidth;
        pInput->outFormat_full                                                   = static_cast<int16_t>(Format::YUV420NV12);
        pInput->outWidth_full                                                    = pInput->inputWidth;
        pInput->outHeight_full                                                   = pInput->inputHeight;
        CAMX_LOG_INFO(CamxLogGroupISP, "<dualIFE> full out not enabled: override with default");
    }
    pInput->roi_mnds_out_full.start_x       = static_cast<uint16_t>(pISPInputdata->pCalculatedData->fullOutCrop.firstPixel);
    pInput->roi_mnds_out_full.end_x         = static_cast<uint16_t>(pISPInputdata->pCalculatedData->fullOutCrop.lastPixel);
    pInput->roi_mnds_out_fd.start_x         = static_cast<uint16_t>(pISPInputdata->pCalculatedData->fdOutCrop.firstPixel);
    pInput->roi_mnds_out_fd.end_x           = static_cast<uint16_t>(pISPInputdata->pCalculatedData->fdOutCrop.lastPixel);
    pInput->roi_mnds_out_disp.start_x       = static_cast<uint16_t>(pISPInputdata->pCalculatedData->dispOutCrop.firstPixel);
    pInput->roi_mnds_out_disp.end_x         = static_cast<uint16_t>(pISPInputdata->pCalculatedData->dispOutCrop.lastPixel);
    CAMX_LOG_INFO(CamxLogGroupISP, "<Scaler> full start_x %d end_x %d fd start_x %d end_x %d disp start_x %d end_x %d",
        pISPInputdata->pCalculatedData->fullOutCrop.firstPixel, pISPInputdata->pCalculatedData->fullOutCrop.lastPixel,
        pISPInputdata->pCalculatedData->fdOutCrop.firstPixel, pISPInputdata->pCalculatedData->fdOutCrop.lastPixel,
        pISPInputdata->pCalculatedData->dispOutCrop.firstPixel, pISPInputdata->pCalculatedData->dispOutCrop.lastPixel);

#endif // DUALIFE_STRIPINGLIB_ZOOMS

    if (TRUE == pISPInputdata->pIFEOutputPathInfo[IFEOutputPortFull].path)
    {
        pInput->outFormat_full = TranslateOutputFormatToStripingLib(pISPInputdata->HALData.format[FullOutput]);
        pInput->outWidth_full  = static_cast<int16_t>(pISPInputdata->HALData.stream[FullOutput].width);
        pInput->outHeight_full = static_cast<int16_t>(pISPInputdata->HALData.stream[FullOutput].height);
    }

    if (TRUE == pISPInputdata->pIFEOutputPathInfo[IFEOutputPortDS4].path)
    {
        pInput->outFormat_1to4 = TranslateOutputFormatToStripingLib(pISPInputdata->HALData.format[DS4Output]);
    }

    if (TRUE == pISPInputdata->pIFEOutputPathInfo[IFEOutputPortDS16].path)
    {
        pInput->outFormat_1to16 = TranslateOutputFormatToStripingLib(pISPInputdata->HALData.format[DS16Output]);
    }

    // Display Full/DS4/DS16 ports support
    if (TRUE == pISPInputdata->pIFEOutputPathInfo[IFEOutputPortDisplayFull].path)
    {
        pInput->outFormat_disp = TranslateOutputFormatToStripingLib(pISPInputdata->HALData.format[DisplayFullOutput]);
        pInput->outWidth_disp  = static_cast<int16_t>(pISPInputdata->HALData.stream[DisplayFullOutput].width);
        pInput->outHeight_disp = static_cast<int16_t>(pISPInputdata->HALData.stream[DisplayFullOutput].height);
    }

    if (TRUE == pISPInputdata->pIFEOutputPathInfo[IFEOutputPortDisplayDS4].path)
    {
        pInput->outFormat_1to4_disp = TranslateOutputFormatToStripingLib(pISPInputdata->HALData.format[DisplayDS4Output]);
    }

    if (TRUE == pISPInputdata->pIFEOutputPathInfo[IFEOutputPortDisplayDS16].path)
    {
        pInput->outFormat_1to16_disp = TranslateOutputFormatToStripingLib(pISPInputdata->HALData.format[DisplayDS16Output]);
    }

    pInput->outFormat_fd      = TranslateOutputFormatToStripingLib(pISPInputdata->HALData.format[FDOutput]);
    pInput->outWidth_fd       = static_cast<int16_t>(pISPInputdata->HALData.stream[FDOutput].width);
    pInput->outHeight_fd      = static_cast<int16_t>(pISPInputdata->HALData.stream[FDOutput].height);
    pInput->hdr_in            = pISPInputdata->pStripingInput->stripingInput.hdr_in;

    pInput->mnds_in_y_fd.rounding_option_h = 0;
    pInput->mnds_in_y_fd.rounding_option_h = 0;
    pInput->mnds_in_c_fd.rounding_option_h = 0;
    pInput->mnds_in_c_fd.rounding_option_v = 0;

    pInput->mnds_in_y_fd.output_l =
        static_cast<uint16_t>(pISPInputdata->pCalculatedData->scalerOutput[FDOutput].dimension.width);
    pInput->mnds_in_y_fd.input_l  =
        static_cast<uint16_t>(pISPInputdata->pCalculatedData->scalerOutput[FDOutput].input.width);
    pInput->mnds_in_c_fd.output_l = pInput->mnds_in_y_fd.output_l / 2;
    pInput->mnds_in_c_fd.input_l  = pInput->mnds_in_y_fd.input_l;

    pInput->mnds_in_y_full.rounding_option_h    = 0;
    pInput->mnds_in_y_full.rounding_option_v    = 0;
    pInput->mnds_in_c_full.rounding_option_h    = 0;
    pInput->mnds_in_c_full.rounding_option_v    = 0;

    pInput->mnds_in_y_full.output_l =
        static_cast<uint16_t>(pISPInputdata->pCalculatedData->scalerOutput[FullOutput].dimension.width);
    pInput->mnds_in_y_full.input_l  =
        static_cast<uint16_t>(pISPInputdata->pCalculatedData->scalerOutput[FullOutput].input.width);
    pInput->mnds_in_c_full.output_l = pInput->mnds_in_y_full.output_l / 2;
    pInput->mnds_in_c_full.input_l  = pInput->mnds_in_y_full.input_l;

    // Display Full/DS4/DS16 ports support
    pInput->mnds_in_y_disp.rounding_option_h    = 0;
    pInput->mnds_in_y_disp.rounding_option_v    = 0;
    pInput->mnds_in_c_disp.rounding_option_h    = 0;
    pInput->mnds_in_c_disp.rounding_option_v    = 0;

    pInput->mnds_in_y_disp.output_l =
        static_cast<uint16_t>(pISPInputdata->pCalculatedData->scalerOutput[DisplayFullOutput].dimension.width);
    pInput->mnds_in_y_disp.input_l  =
        static_cast<uint16_t>(pISPInputdata->pCalculatedData->scalerOutput[DisplayFullOutput].input.width);
    pInput->mnds_in_c_disp.output_l = pInput->mnds_in_y_disp.output_l / 2;
    pInput->mnds_in_c_disp.input_l  = pInput->mnds_in_y_disp.input_l;

    pInput->pdpc_in             = pISPInputdata->pStripingInput->stripingInput.pdpc_in;
    pInput->pedestal_enable     = static_cast<int16_t>(pISPInputdata->pStripingInput->enableBits.pedestal);

    pInput->rolloff_enable      = static_cast<int16_t>(pISPInputdata->pStripingInput->enableBits.rolloff);
    pInput->baf_enable          = static_cast<int16_t>(pISPInputdata->pStripingInput->enableBits.BAF);
    pInput->bg_tintless_enable  = static_cast<int16_t>(pISPInputdata->pStripingInput->enableBits.BGTintless);
    pInput->bg_awb_enable       = static_cast<int16_t>(pISPInputdata->pStripingInput->enableBits.BGAWB);
    pInput->be_enable           = static_cast<int16_t>(pISPInputdata->pStripingInput->enableBits.BE);
    pInput->pdaf_enable         = static_cast<int16_t>(pISPInputdata->pStripingInput->enableBits.PDAF);
    pInput->hdr_enable          = static_cast<int16_t>(pISPInputdata->pStripingInput->enableBits.HDR);
    pInput->bpc_enable          = static_cast<int16_t>(pISPInputdata->pStripingInput->enableBits.BPC);
    pInput->abf_enable          = static_cast<int16_t>(pISPInputdata->pStripingInput->enableBits.ABF);
    pInput->tappingPoint_be     = pISPInputdata->pStripingInput->stripingInput.tappingPoint_be;
    pInput->tapoffPoint_hvx     = pISPInputdata->pStripingInput->stripingInput.tapoffPoint_hvx;
    pInput->kernelSize_hvx      = pISPInputdata->pStripingInput->stripingInput.kernelSize_hvx;
    pInput->pedestalParam       = pISPInputdata->pStripingInput->stripingInput.pedestalParam;
    pInput->rollOffParam        = pISPInputdata->pStripingInput->stripingInput.rollOffParam;
    pInput->hdrBhist_input      = pISPInputdata->pStripingInput->stripingInput.hdrBhist_input;
    pInput->iHist_input         = pISPInputdata->pStripingInput->stripingInput.iHist_input;
    pInput->bHist_input         = pISPInputdata->pStripingInput->stripingInput.bHist_input;
    pInput->bg_tintless_input   = pISPInputdata->pStripingInput->stripingInput.bg_tintless_input;
    pInput->bg_awb_input        = pISPInputdata->pStripingInput->stripingInput.bg_awb_input;
    pInput->be_input            = pISPInputdata->pStripingInput->stripingInput.be_input;
    pInput->rscs_input          = pISPInputdata->pStripingInput->stripingInput.rscs_input;

    if (CSLCameraTitanVersion::CSLTitan150 == pISPInputdata->titanVersion)
    {
        Utils::Memcpy(&pInput->baf_inputv24, &pISPInputdata->pStripingInput->stripingInput.baf_inputv24,
            sizeof(BayerFocusv24InputParam));
    }
    else
    {
        Utils::Memcpy(&pInput->baf_input, &pISPInputdata->pStripingInput->stripingInput.baf_input,
            sizeof(BayerFocusv23InputParam));
    }
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// DualIFEUtils::TranslateOutputFormatToStripingLib
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int16_t DualIFEUtils::TranslateOutputFormatToStripingLib(
    Format srcFormat)
{
    int16_t format = IMAGE_FORMAT_INVALID;
    switch (srcFormat)
    {
        case Format::UBWCTP10:
            format = IMAGE_FORMAT_UBWC_TP_10;
            break;
        case Format::PD10:
            format = IMAGE_FORMAT_PD_10;
            break;
        case Format::YUV420NV12:
        case Format::YUV420NV21:
        case Format::FDYUV420NV12:
            /* configure FDY8 as NV12 and if needed ignore chroma */
        case Format::FDY8:
            format = IMAGE_FORMAT_LINEAR_NV12;
            break;
        case Format::UBWCNV12:
            format = IMAGE_FORMAT_UBWC_NV_12;
            break;
        case Format::UBWCNV124R:
            format = IMAGE_FORMAT_UBWC_NV12_4R;
            break;
        case Format::P010:
            format = IMAGE_FORMAT_LINEAR_P010;
            break;
        case Format::YUV420NV12TP10:
        case Format::YUV420NV21TP10:
            format = IMAGE_FORMAT_LINEAR_TP_10;
            break;
        default:
            CAMX_ASSERT_ALWAYS_MESSAGE("Missing format translation to striping lib format: %d!", srcFormat);
            break;
    }
    return format;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// DualIFEUtils::TranslateInputFormatToStripingLib
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int16_t DualIFEUtils::TranslateInputFormatToStripingLib(
    PixelFormat srcFormat,
    UINT32 CSIDBitWidth)
{
    int16_t format = IMAGE_FORMAT_INVALID;
    switch (srcFormat)
    {
        case PixelFormat::BayerBGGR:
        case PixelFormat::BayerGBRG:
        case PixelFormat::BayerGRBG:
        case PixelFormat::BayerRGGB:
            switch (CSIDBitWidth)
            {
                case 14:
                    format = IMAGE_FORMAT_MIPI_14;
                    break;
                case 12:
                    format = IMAGE_FORMAT_MIPI_12;
                    break;
                case 10:
                    format = IMAGE_FORMAT_MIPI_10;
                    break;
                case 8:
                    format = IMAGE_FORMAT_MIPI_8;
                    break;
                default:
                    CAMX_ASSERT_ALWAYS_MESSAGE("Invalid CSID format bitwidth; defaulting to 10 bit: %d!", CSIDBitWidth);
                    format = IMAGE_FORMAT_MIPI_10;
                    break;
            }
            break;
        case PixelFormat::YUVFormatUYVY:
        case PixelFormat::YUVFormatYUYV:
            switch (CSIDBitWidth)
            {
                case 10:
                    format = IMAGE_FORMAT_YUV422_10;
                    break;
                case 8:
                    format = IMAGE_FORMAT_YUV422_8;
                    break;
                default:
                    CAMX_ASSERT_ALWAYS_MESSAGE("Invalid CSID format bitwidth; defaulting to 10 bit: %d!", CSIDBitWidth);
                    format = IMAGE_FORMAT_YUV422_10;
                    break;
            }
            break;
        default:
            CAMX_ASSERT_ALWAYS_MESSAGE("Missing format translation to striping lib format: %d!", srcFormat);
            break;
    }
    return format;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::HardcodeSettingsSetDefaultBFFilterInputConfig
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IFENode::HardcodeSettingsSetDefaultBFFilterInputConfig(
    AFStatsControl* pAFConfigOutput)
{
    AFConfigParams* pStatsConfig = NULL;

    pStatsConfig = &pAFConfigOutput->statsConfig;

    // 1. H1 Config
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeHorizontal1].horizontalScaleEnable = FALSE;
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeHorizontal1].isValid               = TRUE;
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeHorizontal1].shiftBits             = -3;

    // 1a. H1 FIR
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeHorizontal1].BFFIRFilterConfig.enable               = TRUE;
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeHorizontal1].BFFIRFilterConfig.numOfFIRCoefficients = 13;

    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeHorizontal1].BFFIRFilterConfig.FIRFilterCoefficients[0]  = -1;
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeHorizontal1].BFFIRFilterConfig.FIRFilterCoefficients[1]  = -2;
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeHorizontal1].BFFIRFilterConfig.FIRFilterCoefficients[2]  = -1;
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeHorizontal1].BFFIRFilterConfig.FIRFilterCoefficients[3]  = 1;
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeHorizontal1].BFFIRFilterConfig.FIRFilterCoefficients[4]  = 5;
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeHorizontal1].BFFIRFilterConfig.FIRFilterCoefficients[5]  = 8;
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeHorizontal1].BFFIRFilterConfig.FIRFilterCoefficients[6]  = 10;
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeHorizontal1].BFFIRFilterConfig.FIRFilterCoefficients[7]  = 8;
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeHorizontal1].BFFIRFilterConfig.FIRFilterCoefficients[8]  = 5;
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeHorizontal1].BFFIRFilterConfig.FIRFilterCoefficients[9]  = 1;
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeHorizontal1].BFFIRFilterConfig.FIRFilterCoefficients[10] = -1;
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeHorizontal1].BFFIRFilterConfig.FIRFilterCoefficients[11] = -2;
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeHorizontal1].BFFIRFilterConfig.FIRFilterCoefficients[12] = -1;

    // 1b. H1 IIR
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeHorizontal1].BFIIRFilterConfig.enable = TRUE;

    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeHorizontal1].BFIIRFilterConfig.b10 = 0.092346f;
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeHorizontal1].BFIIRFilterConfig.b11 = 0.000000f;
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeHorizontal1].BFIIRFilterConfig.b12 = -0.092346f;
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeHorizontal1].BFIIRFilterConfig.a11 = 1.712158f;
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeHorizontal1].BFIIRFilterConfig.a12 = -0.815308f;
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeHorizontal1].BFIIRFilterConfig.b20 = 0.112976f;
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeHorizontal1].BFIIRFilterConfig.b21 = 0.000000f;
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeHorizontal1].BFIIRFilterConfig.b22 = -0.112976f;
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeHorizontal1].BFIIRFilterConfig.a21 = 1.869690f;
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeHorizontal1].BFIIRFilterConfig.a22 = -0.898743f;

    // 1c. H1 Coring
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeHorizontal1].BFFilterCoringConfig.threshold = (1 << 16);
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeHorizontal1].BFFilterCoringConfig.gain      = 16;

    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeHorizontal1].BFFilterCoringConfig.core[0]  = 0;
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeHorizontal1].BFFilterCoringConfig.core[1]  = 16;
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeHorizontal1].BFFilterCoringConfig.core[2]  = 16;
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeHorizontal1].BFFilterCoringConfig.core[3]  = 16;
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeHorizontal1].BFFilterCoringConfig.core[4]  = 16;
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeHorizontal1].BFFilterCoringConfig.core[5]  = 16;
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeHorizontal1].BFFilterCoringConfig.core[6]  = 16;
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeHorizontal1].BFFilterCoringConfig.core[7]  = 16;
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeHorizontal1].BFFilterCoringConfig.core[8]  = 16;
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeHorizontal1].BFFilterCoringConfig.core[9]  = 16;
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeHorizontal1].BFFilterCoringConfig.core[10] = 16;
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeHorizontal1].BFFilterCoringConfig.core[11] = 16;
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeHorizontal1].BFFilterCoringConfig.core[12] = 16;
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeHorizontal1].BFFilterCoringConfig.core[13] = 16;
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeHorizontal1].BFFilterCoringConfig.core[14] = 16;
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeHorizontal1].BFFilterCoringConfig.core[15] = 16;
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeHorizontal1].BFFilterCoringConfig.core[16] = 16;

    // 2. H2 Config
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeHorizontal2].horizontalScaleEnable = TRUE;
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeHorizontal2].isValid               = TRUE;
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeHorizontal2].shiftBits             = 3;

    // 2a. H2 FIR
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeHorizontal2].BFFIRFilterConfig.enable = FALSE;

    // 2b. H2 IIR
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeHorizontal2].BFIIRFilterConfig.enable = TRUE;

    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeHorizontal2].BFIIRFilterConfig.b10 = 0.078064f;
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeHorizontal2].BFIIRFilterConfig.b11 = 0.000000f;
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeHorizontal2].BFIIRFilterConfig.b12 = -0.078064f;
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeHorizontal2].BFIIRFilterConfig.a11 = 1.735413f;
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeHorizontal2].BFIIRFilterConfig.a12 = -0.843811f;
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeHorizontal2].BFIIRFilterConfig.b20 = 0.257202f;
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeHorizontal2].BFIIRFilterConfig.b21 = 0.000000f;
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeHorizontal2].BFIIRFilterConfig.b22 = -0.257202f;
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeHorizontal2].BFIIRFilterConfig.a21 = 1.477051f;
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeHorizontal2].BFIIRFilterConfig.a22 = -0.760071f;

    // 2c. H2 Coring
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeHorizontal2].BFFilterCoringConfig.threshold = (1 << 16);
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeHorizontal2].BFFilterCoringConfig.gain      = 16;
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeHorizontal2].BFFilterCoringConfig.core[0]   = 0;
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeHorizontal2].BFFilterCoringConfig.core[1]   = 16;
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeHorizontal2].BFFilterCoringConfig.core[2]   = 16;
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeHorizontal2].BFFilterCoringConfig.core[3]   = 16;
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeHorizontal2].BFFilterCoringConfig.core[4]   = 16;
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeHorizontal2].BFFilterCoringConfig.core[5]   = 16;
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeHorizontal2].BFFilterCoringConfig.core[6]   = 16;
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeHorizontal2].BFFilterCoringConfig.core[7]   = 16;
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeHorizontal2].BFFilterCoringConfig.core[8]   = 16;
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeHorizontal2].BFFilterCoringConfig.core[9]   = 16;
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeHorizontal2].BFFilterCoringConfig.core[10]  = 16;
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeHorizontal2].BFFilterCoringConfig.core[11]  = 16;
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeHorizontal2].BFFilterCoringConfig.core[12]  = 16;
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeHorizontal2].BFFilterCoringConfig.core[13]  = 16;
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeHorizontal2].BFFilterCoringConfig.core[14]  = 16;
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeHorizontal2].BFFilterCoringConfig.core[15]  = 16;
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeHorizontal2].BFFilterCoringConfig.core[16]  = 16;

    // 3. Vertical Config
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeVertical].isValid   = TRUE;
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeVertical].shiftBits = 0;

    // 3a. Vertical FIR
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeVertical].BFFIRFilterConfig.enable               = TRUE;
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeVertical].BFFIRFilterConfig.numOfFIRCoefficients = 5;

    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeVertical].BFFIRFilterConfig.FIRFilterCoefficients[0] = 1;
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeVertical].BFFIRFilterConfig.FIRFilterCoefficients[1] = 1;
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeVertical].BFFIRFilterConfig.FIRFilterCoefficients[2] = 1;
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeVertical].BFFIRFilterConfig.FIRFilterCoefficients[3] = 1;
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeVertical].BFFIRFilterConfig.FIRFilterCoefficients[4] = 1;

    // 3b. H1 IIR
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeVertical].BFIIRFilterConfig.enable = TRUE;
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeVertical].BFIIRFilterConfig.b10    = 0.894897f;
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeVertical].BFIIRFilterConfig.b11    = -1.789673f;
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeVertical].BFIIRFilterConfig.b12    = 0.894897f;
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeVertical].BFIIRFilterConfig.a11    = 1.778625f;
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeVertical].BFIIRFilterConfig.a12    = -0.800781f;
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeVertical].BFIIRFilterConfig.b20    = 0.112976f;
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeVertical].BFIIRFilterConfig.b21    = 0.000000f;
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeVertical].BFIIRFilterConfig.b22    = -0.112976f;
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeVertical].BFIIRFilterConfig.a21    = 1.869690f;
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeVertical].BFIIRFilterConfig.a22    = -0.898743f;

    // 3c. H1 Coring
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeVertical].BFFilterCoringConfig.threshold = (1 << 16);
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeVertical].BFFilterCoringConfig.gain      = 16;

    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeVertical].BFFilterCoringConfig.core[0]  = 0;
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeVertical].BFFilterCoringConfig.core[1]  = 16;
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeVertical].BFFilterCoringConfig.core[2]  = 16;
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeVertical].BFFilterCoringConfig.core[3]  = 16;
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeVertical].BFFilterCoringConfig.core[4]  = 16;
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeVertical].BFFilterCoringConfig.core[5]  = 16;
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeVertical].BFFilterCoringConfig.core[6]  = 16;
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeVertical].BFFilterCoringConfig.core[7]  = 16;
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeVertical].BFFilterCoringConfig.core[8]  = 16;
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeVertical].BFFilterCoringConfig.core[9]  = 16;
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeVertical].BFFilterCoringConfig.core[10] = 16;
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeVertical].BFFilterCoringConfig.core[11] = 16;
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeVertical].BFFilterCoringConfig.core[12] = 16;
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeVertical].BFFilterCoringConfig.core[13] = 16;
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeVertical].BFFilterCoringConfig.core[14] = 16;
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeVertical].BFFilterCoringConfig.core[15] = 16;
    pStatsConfig->BFStats.BFFilterConfig[BFFilterTypeVertical].BFFilterCoringConfig.core[16] = 16;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::HardcodeSettingsSetDefaultBFROIInputConfig
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IFENode::HardcodeSettingsSetDefaultBFROIInputConfig(
    AFStatsControl* pAFConfigOutput,
    UINT32          CAMIFWidth,
    UINT32          CAMIFHeight)
{
    UINT32                     horizontalOffset = 0;
    UINT32                     verticalOffset   = 0;
    UINT32                     ROIWidth         = 0;
    UINT32                     ROIHeight        = 0;
    UINT32                     horizontalNum    = 0;
    UINT32                     verticalNum      = 0;
    UINT32                     indexX           = 0;
    UINT32                     indexY           = 0;
    BFStatsROIDimensionParams* pROI             = NULL;
    BFStatsROIConfigType*      pROIConfig       = NULL;

    pROIConfig = &pAFConfigOutput->statsConfig.BFStats.BFStatsROIConfig;

    Utils::Memset(pROIConfig, 0, sizeof(BFStatsROIConfigType));
    pROIConfig->numBFStatsROIDimension = 0;

    // Configure ROI in 5 x 5 grids
    horizontalNum = 5;
    verticalNum   = 5;

    ROIWidth  = Utils::FloorUINT32(2, (((25 * CAMIFWidth) / horizontalNum) / 100));
    ROIHeight = Utils::FloorUINT32(2, (((25 * CAMIFHeight) / verticalNum) / 100));

    horizontalOffset = (CAMIFWidth - (horizontalNum * ROIWidth)) >> 1;
    verticalOffset   = (CAMIFHeight - (verticalNum * ROIHeight)) >> 1;

    for (indexY = 0; indexY < verticalNum; indexY++)
    {
        for (indexX = 0; indexX < horizontalNum; indexX++)
        {
            pROI = &pROIConfig->BFStatsROIDimension[pROIConfig->numBFStatsROIDimension];

            pROI->region     = BFStatsPrimaryRegion;
            pROI->ROI.left   = Utils::FloorUINT32(2, (horizontalOffset + (indexX * ROIWidth)));
            pROI->ROI.top    = Utils::FloorUINT32(2, (verticalOffset + (indexY * ROIHeight)));
            pROI->ROI.width  = ROIWidth - 1;
            pROI->ROI.height = ROIHeight - 1;
            pROI->regionNum  = pROIConfig->numBFStatsROIDimension++;
            pROI->isValid    = TRUE;
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::HardcodeSettingsBFStats
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void IFENode::HardcodeSettingsBFStats(
    ISPStripeConfig* pStripeConfig)
{
    const UINT32    CAMIFWidth      = pStripeConfig->CAMIFCrop.lastPixel - pStripeConfig->CAMIFCrop.firstPixel + 1;
    const UINT32    CAMIFHeight     = pStripeConfig->CAMIFCrop.lastLine - pStripeConfig->CAMIFCrop.firstLine + 1;
    AFStatsControl* pAFUpdateData   = &pStripeConfig->AFStatsUpdateData;

    pAFUpdateData->statsConfig.mask = BFStats;

    HardcodeSettingsSetDefaultBFFilterInputConfig(pAFUpdateData);
    HardcodeSettingsSetDefaultBFROIInputConfig(pAFUpdateData, CAMIFWidth, CAMIFHeight);

    pAFUpdateData->statsConfig.BFStats.BFStatsROIConfig.BFROIType     = BFStatsCustomROI;
    pAFUpdateData->statsConfig.BFStats.BFScaleConfig.BFScaleEnable    = FALSE;
    pAFUpdateData->statsConfig.BFStats.BFScaleConfig.isValid          = FALSE;
    pAFUpdateData->statsConfig.BFStats.BFScaleConfig.scaleM           = 1;
    pAFUpdateData->statsConfig.BFStats.BFScaleConfig.scaleN           = 2;
    pAFUpdateData->statsConfig.BFStats.BFInputConfig.BFChannelSel     = BFChannelSelectG;
    pAFUpdateData->statsConfig.BFStats.BFInputConfig.BFInputGSel      = BFInputSelectGr;
    pAFUpdateData->statsConfig.BFStats.BFInputConfig.isValid          = TRUE;
    pAFUpdateData->statsConfig.BFStats.BFInputConfig.YAConfig[0]      = 0;
    pAFUpdateData->statsConfig.BFStats.BFInputConfig.YAConfig[1]      = 0;
    pAFUpdateData->statsConfig.BFStats.BFInputConfig.YAConfig[2]      = 0;
    pAFUpdateData->statsConfig.BFStats.BFGammaLUTConfig.isValid       = FALSE;
    pAFUpdateData->statsConfig.BFStats.BFGammaLUTConfig.numGammaLUT   = BF_GAMMA_ENTRIES;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::SetupHFRInitialConfig
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFENode::SetupHFRInitialConfig()
{
    CamxResult              result                  = CamxResultSuccess;
    UINT                    totalOutputPorts        = 0;
    SIZE_T                  resourceHFRConfigSize   = 0;
    IFEResourceHFRConfig*   pResourceHFRConfig      = NULL;
    UINT                    outputPortId[MaxIFEOutputPorts];
    UINT32                  channelId;

    // Get Output Port List
    GetAllOutputPortIds(&totalOutputPorts, &outputPortId[0]);
    if ((totalOutputPorts == 0) || (totalOutputPorts > MaxIFEOutputPorts))
    {
        CAMX_LOG_ERROR(CamxLogGroupISP, "Error: invalid number of output ports %d", totalOutputPorts);
        result = CamxResultEInvalidArg;
    }

    if (CamxResultSuccess == result)
    {
        resourceHFRConfigSize   = sizeof(IFEResourceHFRConfig) +
                                  (sizeof(IFEPortHFRConfig) * (totalOutputPorts - 1 - m_disabledOutputPorts));
        pResourceHFRConfig      = static_cast<IFEResourceHFRConfig*>(CAMX_CALLOC(resourceHFRConfigSize));

        if (NULL == pResourceHFRConfig)
        {
            CAMX_LOG_ERROR(CamxLogGroupISP, "Failed to allocate ResourceHFRInfo.");
            result = CamxResultENoMemory;
        }
    }

    if (CamxResultSuccess == result)
    {
        pResourceHFRConfig->numPorts = totalOutputPorts - m_disabledOutputPorts;

        CAMX_LOG_VERBOSE(CamxLogGroupISP, "totalOutputPorts=%d, m_usecaseNumBatchedFrames=%d, m_disabledOutputPorts = %d",
                         totalOutputPorts, m_usecaseNumBatchedFrames, m_disabledOutputPorts);

        UINT portHFRConfigIndex = 0;
        for (UINT portIndex = 0; portIndex < totalOutputPorts; portIndex++)
        {
            UINT currentInternalOutputPortIndex = OutputPortIndex(outputPortId[portIndex]);
            if (TRUE == m_isDisabledOutputPort[currentInternalOutputPortIndex])
            {
                CAMX_LOG_INFO(CamxLogGroupISP,
                              "Skipping for output portId = %u because the sensor does not support port source type = %u",
                              outputPortId[portIndex],
                              GetOutputPortSourceType(currentInternalOutputPortIndex));
                continue;
            }

            result = MapPortIdToChannelId(outputPortId[portIndex], &channelId);

            if (CamxResultSuccess == result)
            {
                IFEPortHFRConfig* pPortHFRConfig = &pResourceHFRConfig->portHFRConfig[portHFRConfigIndex];

                pPortHFRConfig->portResourceId = channelId;

                if (m_usecaseNumBatchedFrames > 1)
                {
                    if (TRUE == IsBatchEnabledPort(outputPortId[portIndex]))
                    {
                        pPortHFRConfig->framedropPattern  = 1;
                        pPortHFRConfig->framedropPeriod   = 0;
                        pPortHFRConfig->subsamplePattern  = 1 << (m_usecaseNumBatchedFrames - 1);
                        pPortHFRConfig->subsamplePeriod   = (m_usecaseNumBatchedFrames - 1);
                    }
                    else
                    {
                        pPortHFRConfig->framedropPattern  = 1;
                        pPortHFRConfig->framedropPeriod   = (m_usecaseNumBatchedFrames - 1);
                        pPortHFRConfig->subsamplePattern  = 1;
                        pPortHFRConfig->subsamplePeriod   = 0;
                    }
                }
                else
                {
                    pPortHFRConfig->framedropPattern  = 1;
                    pPortHFRConfig->framedropPeriod   = 0;
                    pPortHFRConfig->subsamplePattern  = 1;
                    pPortHFRConfig->subsamplePeriod   = 0;
                }
            }
            else
            {
                CAMX_LOG_ERROR(CamxLogGroupISP, "Error: invalid Port Id %d", outputPortId[portIndex]);
                break;
            }
            portHFRConfigIndex++;
        }

        if (CamxResultSuccess == result)
        {
            result = PacketBuilder::WriteGenericBlobData(m_pGenericBlobCmdBuffer,
                                                         IFEGenericBlobTypeHFRConfig,
                                                         static_cast<UINT32>(resourceHFRConfigSize),
                                                         reinterpret_cast<BYTE*>(pResourceHFRConfig));

            if (CamxResultSuccess != result)
            {
                CAMX_LOG_ERROR(CamxLogGroupISP, "Error: Writing Blob data size=%d, result=%d",
                               static_cast<UINT32>(resourceHFRConfigSize), result);
            }
        }
    }

    if (NULL != pResourceHFRConfig)
    {
        CAMX_FREE(pResourceHFRConfig);
        pResourceHFRConfig = NULL;
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::SetupUBWCInitialConfig
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFENode::SetupUBWCInitialConfig()
{
    CamxResult              result                 = CamxResultSuccess;
    UINT                    totalOutputPorts       = 0;
    SIZE_T                  resourceUBWCConfigSize = 0;
    CSLResourceUBWCConfig*  pResourceUBWCConfig    = NULL;
    UINT                    outputPortId[MaxIFEOutputPorts];
    UINT32                  channelId;
    UINT                    totalUBWCOutputPorts   = 0;
    UINT                    outputUBWCPortId[MaxIFEOutputPorts];

    // Get Output Port List
    GetAllOutputPortIds(&totalOutputPorts, &outputPortId[0]);
    if ((totalOutputPorts == 0) || (totalOutputPorts > MaxIFEOutputPorts))
    {
        CAMX_LOG_ERROR(CamxLogGroupISP, "Error: invalid number of output ports %d", totalOutputPorts);
        result = CamxResultEInvalidArg;
    }

    if (CamxResultSuccess == result)
    {
        for (UINT portIndex = 0; portIndex < totalOutputPorts; portIndex++)
        {
            UINT currentOutputPortIndex = OutputPortIndex(outputPortId[portIndex]);

            if (TRUE == m_isDisabledOutputPort[portIndex])
            {
                CAMX_LOG_VERBOSE(CamxLogGroupISP,
                                 "Skipping for output portId = %u because the sensor does not support port source type = %u",
                                 GetOutputPortId(portIndex),
                                 GetOutputPortSourceType(portIndex));
                continue;
            }
            const ImageFormat* pCurrentOutputPortImageFormat = GetOutputPortImageFormat(currentOutputPortIndex);
            if (TRUE == ImageFormatUtils::IsUBWC(pCurrentOutputPortImageFormat->format))
            {
                outputUBWCPortId[totalUBWCOutputPorts] = outputPortId[portIndex];
                totalUBWCOutputPorts++;
            }
        }

        CAMX_LOG_VERBOSE(CamxLogGroupISP, "number of UBWC Supported output ports %d", totalUBWCOutputPorts);

        if ((CamxResultSuccess == result) && (totalUBWCOutputPorts > 0))
        {
            resourceUBWCConfigSize = sizeof(CSLResourceUBWCConfig) +
                (sizeof(CSLPortUBWCConfig) * (CSLMaxNumPlanes - 1) * (totalUBWCOutputPorts - 1));
            pResourceUBWCConfig = static_cast<CSLResourceUBWCConfig*>(CAMX_CALLOC(resourceUBWCConfigSize));

            if (NULL == pResourceUBWCConfig)
            {
                CAMX_LOG_ERROR(CamxLogGroupISP, "Failed to allocate ResourceUBWCInfo.");
                result = CamxResultENoMemory;
            }
        }

        if ((CamxResultSuccess == result) && (totalUBWCOutputPorts > 0))
        {
            pResourceUBWCConfig->numPorts       = totalUBWCOutputPorts;
            pResourceUBWCConfig->UBWCAPIVersion = IFESupportedUBWCVersions2And3; ///< which supports both UBWC 2.0 & 3.0
            CAMX_LOG_VERBOSE(CamxLogGroupISP, "totalOutputPorts=%d, m_usecaseNumBatchedFrames=%d, m_disabledOutputPorts = %d",
                             totalOutputPorts, m_usecaseNumBatchedFrames, m_disabledOutputPorts);

            UINT portUBWCConfigIndex = 0;
            for (UINT portIndex = 0; portIndex < totalUBWCOutputPorts; portIndex++)
            {
                if (TRUE == m_isDisabledOutputPort[portIndex])
                {
                    CAMX_LOG_INFO(CamxLogGroupISP,
                                  "Skipping for output portId = %u because the sensor does not support port source type = %u",
                                  GetOutputPortId(portIndex),
                                  GetOutputPortSourceType(portIndex));
                    continue;
                }

                UINT currentOutputPortIndex = OutputPortIndex(outputUBWCPortId[portIndex]);
                // Set image format from the output port index
                const ImageFormat* pCurrentOutputPortImageFormat = GetOutputPortImageFormat(currentOutputPortIndex);

                if (TRUE == ImageFormatUtils::IsUBWC(pCurrentOutputPortImageFormat->format))
                {
                    result = MapPortIdToChannelId(outputUBWCPortId[portIndex], &channelId);

                    if (CamxResultSuccess == result)
                    {
                        UINT numPlanes = ImageFormatUtils::GetNumberOfPlanes(pCurrentOutputPortImageFormat);
                        for (UINT planeIndex = 0; planeIndex < numPlanes; planeIndex++)
                        {
                            UBWCPartialTileInfo   UBWCPartialTileParam;
                            CSLPortUBWCConfig*    pPortUBWCConfig =
                                &pResourceUBWCConfig->portUBWCConfig[portUBWCConfigIndex][planeIndex];

                            pPortUBWCConfig->portResourceId = channelId;
                            pPortUBWCConfig->metadataStride =
                                pCurrentOutputPortImageFormat->formatParams.yuvFormat[planeIndex].metadataStride;
                            pPortUBWCConfig->metadataSize   =
                                pCurrentOutputPortImageFormat->formatParams.yuvFormat[planeIndex].metadataSize;

                            switch (pCurrentOutputPortImageFormat->format)
                            {
                                case Format::UBWCTP10:
                                    pPortUBWCConfig->packerConfig = 0xb;
                                    break;
                                case Format::UBWCNV12:
                                    pPortUBWCConfig->packerConfig = 0xe;
                                    break;
                                case Format::UBWCNV124R:
                                    pPortUBWCConfig->packerConfig = 0xe;
                                    break;
                                default:
                                    CAMX_LOG_ERROR(CamxLogGroupISP, "Invalid UBWC format: %d",
                                                   pCurrentOutputPortImageFormat->format);
                                    result = CamxResultEFailed;
                                    break;
                            }

                            const struct UBWCTileInfo* pTileInfo =
                                ImageFormatUtils::GetUBWCTileInfo(pCurrentOutputPortImageFormat);

                            if (NULL != pTileInfo)
                            {
                                // partial tile information calculated assuming this full frame processing
                                // dual ife this will be overwritten by ife node.
                                // hInit & vInit are 0, as we are programming entire frame
                                ImageFormatUtils::GetUBWCPartialTileInfo(pTileInfo,
                                                                         &UBWCPartialTileParam,
                                                                         0,
                                                                         pCurrentOutputPortImageFormat->width);

                                pPortUBWCConfig->tileConfig |= (UBWCPartialTileParam.partialTileBytesLeft & 0x3F) << 16;
                                pPortUBWCConfig->tileConfig |= (UBWCPartialTileParam.partialTileBytesRight & 0x3F) << 23;

                                pPortUBWCConfig->modeConfig =
                                    ImageFormatUtils::GetUBWCModeConfig(pCurrentOutputPortImageFormat, planeIndex);
                                pPortUBWCConfig->modeConfig1 = ImageFormatUtils::GetUBWCModeConfig1();

                                // (Cuurently, Only Moorea supports IFE UBWC lossy out) Check if resolution >= 4K,
                                // make IFE output lossy.
                                if (TRUE == IsUBWCLossyNeeded(pCurrentOutputPortImageFormat))
                                {
                                    pPortUBWCConfig->modeConfig1 |= (1 << 4);    // Enable UBWC Lossy output
                                    CAMX_LOG_VERBOSE(CamxLogGroupISP, "Set Outputport %d to lossless, format %d",
                                    outputUBWCPortId[portIndex], pCurrentOutputPortImageFormat->format);
                                }
                                pPortUBWCConfig->hInitialVal = UBWCPartialTileParam.horizontalInit;
                                pPortUBWCConfig->vInitialVal = UBWCPartialTileParam.verticalInit;
                                // no need to program this as of now
                                pPortUBWCConfig->metadataOffset = 0;
                            }
                            else
                            {
                                CAMX_LOG_ERROR(CamxLogGroupUtils, "pTileInfo is NULL");
                                result = CamxResultEFailed;
                                break;
                            }
                        }
                    }
                    else
                    {
                        CAMX_LOG_ERROR(CamxLogGroupISP, "Error: invalid Port Id %d", outputUBWCPortId[portIndex]);
                        break;
                    }
                    portUBWCConfigIndex++;
                }
            }

            if (CamxResultSuccess == result)
            {
                result = PacketBuilder::WriteGenericBlobData(m_pGenericBlobCmdBuffer,
                                                             IFEGenericBlobTypeUBWCConfig,
                                                             static_cast<UINT32>(resourceUBWCConfigSize),
                                                             reinterpret_cast<BYTE*>(pResourceUBWCConfig));
                if (CamxResultSuccess != result)
                {
                    CAMX_LOG_ERROR(CamxLogGroupISP, "Error: Writing Blob data size=%d, result=%d",
                                   static_cast<UINT32>(resourceUBWCConfigSize), result);
                }
            }
        }
    }

    if (NULL != pResourceUBWCConfig)
    {
        CAMX_FREE(pResourceUBWCConfig);
        pResourceUBWCConfig = NULL;
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::SetupBusReadInitialConfig
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFENode::SetupBusReadInitialConfig()
{
    CamxResult            result            = CamxResultSuccess;
    SIZE_T                busReadConfigSize = 0;
    IFEBusReadConfig*     pBusReadConfig    = NULL;

    busReadConfigSize = sizeof(IFEBusReadConfig);
    pBusReadConfig    = static_cast<IFEBusReadConfig*>(CAMX_CALLOC(busReadConfigSize));

    if (NULL == pBusReadConfig)
    {
        CAMX_LOG_ERROR(CamxLogGroupISP, "Failed to allocate BusReadConfig.");
        result = CamxResultENoMemory;
    }

    if (CamxResultSuccess == result)
    {
        pBusReadConfig->HBICount = m_pSensorModeData->minHorizontalBlanking;
        if (BusReadMinimumHBI > pBusReadConfig->HBICount)
        {
            CAMX_LOG_WARN(CamxLogGroupISP, "Invalid BusRead HBI. Clamp to minimum required");
            pBusReadConfig->HBICount = BusReadMinimumHBI;
        }

        // A value of VBI will be counted to VBI*8 number of lines in h/w after fetching each frame
        pBusReadConfig->VBICount = (m_pSensorModeData->minVerticalBlanking >> 3);
        if (BusReadMinimumVBI > pBusReadConfig->VBICount)
        {
            CAMX_LOG_WARN(CamxLogGroupISP, "Invalid BusRead VBI. Clamp to minimum required");
            pBusReadConfig->VBICount = BusReadMinimumVBI;
        }

        CAMX_LOG_VERBOSE(CamxLogGroupISP, "BusRead minHBI=%u minVBI=%u",
            pBusReadConfig->HBICount, pBusReadConfig->VBICount);

        pBusReadConfig->lineSyncEnable = 0;
        pBusReadConfig->FSMode         = 1;
        pBusReadConfig->syncEnable     = 0;
        pBusReadConfig->goCmdSelect    = 1;
        pBusReadConfig->clientEnable   = 1;
        pBusReadConfig->unpackMode     = TranslateFormatToISPImageFormat(CamX::Format::RawMIPI, m_CSIDecodeBitWidth);;
        pBusReadConfig->latencyBufSize = 0x1000;

        result = PacketBuilder::WriteGenericBlobData(m_pGenericBlobCmdBuffer,
                                                     IFEGenericBlobTypeBusReadConfig,
                                                     static_cast<UINT32>(busReadConfigSize),
                                                     reinterpret_cast<BYTE*>(pBusReadConfig));
        if (CamxResultSuccess != result)
        {
            CAMX_LOG_ERROR(CamxLogGroupISP, "Error: Writing BusRead Blob data size=%d, result=%d",
                static_cast<UINT32>(busReadConfigSize), result);
        }
    }

    if (NULL != pBusReadConfig)
    {
        CAMX_FREE(pBusReadConfig);
        pBusReadConfig = NULL;
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::GetIFEInputWidth
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
UINT32 IFENode::GetIFEInputWidth(
    IFESplitID IFEIndex)
{
    CAMX_ASSERT(IFESplitID::Right >= IFEIndex);

    UINT32 width = 0;

    if (IFEModuleMode::DualIFENormal == m_mode)
    {
        if (IFESplitID::Left == IFEIndex)
        {
            width = m_dualIFESplitParams.splitPoint + m_dualIFESplitParams.rightPadding;
        }
        else
        {
            width = m_pSensorModeData->resolution.outputWidth - m_dualIFESplitParams.splitPoint +
                    m_dualIFESplitParams.leftPadding;
        }
    }
    else
    {
        width = m_pSensorModeData->resolution.outputWidth;
    }

    return width;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::CalculatePixelClockRate
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
UINT64 IFENode::CalculatePixelClockRate(
    UINT32  inputWidth)
{
    UINT64  pixClockHz = 0;

    CAMX_ASSERT((0 != m_pSensorModeData->resolution.outputWidth) &&
                (0 != m_pSensorModeData->maxFPS) &&
                (0 != m_pSensorModeData->numLinesPerFrame));

    CheckForRDIOnly();

    if (FALSE == m_RDIOnlyUseCase)
    {
        const UINT minIFEhbi = 64;

        pixClockHz = m_pSensorModeData->outPixelClock;

        CAMX_LOG_VERBOSE(CamxLogGroupPower,
                         "inputWidth=%u minHorizontalBlanking=%u outPixelClock=%llu, sensorOutputWidth=%u",
                         inputWidth, m_pSensorModeData->minHorizontalBlanking, m_pSensorModeData->outPixelClock,
                         m_pSensorModeData->resolution.outputWidth);

        if (0 != m_pSensorModeData->minHorizontalBlanking)
        {
            pixClockHz = (Utils::ByteAlign32((inputWidth + minIFEhbi), 32) * m_pSensorModeData->outPixelClock) /
                         (m_pSensorModeData->resolution.outputWidth + m_pSensorModeData->minHorizontalBlanking);
        }
        else
        {
            // When minHorizontalBlanking is not populated, fallback to provided frameLineLength
            UINT64 theoreticalLineLength = static_cast<UINT64>(m_pSensorModeData->outPixelClock /
                (m_pSensorModeData->maxFPS * m_pSensorModeData->numLinesPerFrame));

            CAMX_LOG_VERBOSE(CamxLogGroupPower,
                             "theoreticalLineLength=%llu maxFPS=%lf numPixelsPerLine=%u numLinesPerFrame=%u",
                             theoreticalLineLength, m_pSensorModeData->maxFPS,
                             m_pSensorModeData->numPixelsPerLine, m_pSensorModeData->numLinesPerFrame);

            pixClockHz = (Utils::ByteAlign32((inputWidth + minIFEhbi), 32) * m_pSensorModeData->outPixelClock) /
                         (Utils::MinUINT64(theoreticalLineLength, Utils::MaxUINT32(m_pSensorModeData->resolution.outputWidth,
                                                                                   m_pSensorModeData->numPixelsPerLine)));
        }

        if ((0 != m_pSensorModeData->minVerticalBlanking) &&
            (m_pSensorModeData->minVerticalBlanking < IFEMinVBI))
        {
            CAMX_LOG_VERBOSE(CamxLogGroupPower, "minVerticalBlanking=%u", m_pSensorModeData->minVerticalBlanking);
            pixClockHz = (pixClockHz * IFEMinVBI) / m_pSensorModeData->minVerticalBlanking;
        }
    }

    CAMX_LOG_VERBOSE(CamxLogGroupPower, "pixClockHz=%llu", pixClockHz);

    return pixClockHz;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::GetPixelClockRate
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
UINT64 IFENode::GetPixelClockRate(
    IFESplitID IFEIndex)
{
    CAMX_ASSERT(IFESplitID::Right >= IFEIndex);

    UINT64 pixClockHz = 0;

    INT overridePixClockMHz = static_cast<INT>(GetStaticSettings()->ifeClockFrequencyMHz);
    if (0 > overridePixClockMHz)
    {
        CAMX_LOG_INFO(CamxLogGroupPower, "Clock setting disabled by override %d", overridePixClockMHz);
    }
    else
    {
        if (0 == m_ifeClockRate[static_cast<UINT>(IFEIndex)])
        {
            if (0 != overridePixClockMHz)
            {
                pixClockHz = overridePixClockMHz * 1000000; // Convert to Hz
                CAMX_LOG_INFO(CamxLogGroupPower, "Using override clock rate %llu Hz", pixClockHz);
            }
            else
            {
                UINT32 inputWidth = GetIFEInputWidth(IFEIndex);

                // Force a minimum clock rate to meet HW constraints on 845
                pixClockHz = Utils::MaxUINT64(HwEnvironment::GetInstance()->GetPlatformStaticCaps()->minIFEHWClkRate,
                                              CalculatePixelClockRate(inputWidth));

                CAMX_LOG_INFO(CamxLogGroupPower, "Using clock rate %llu Hz for inputWidth: %d", pixClockHz, inputWidth);
            }

            m_ifeClockRate[static_cast<UINT>(IFEIndex)] = pixClockHz;
        }
        else
        {
            pixClockHz = m_ifeClockRate[static_cast<UINT>(IFEIndex)];
        }
    }

    return pixClockHz;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::SetupHVXInitialConfig
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFENode::SetupHVXInitialConfig(
    ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;

    if (NULL != m_pIFEHVXModule)
    {
        result = (static_cast<IFEHVX*>(m_pIFEHVXModule))->InitConfiguration(pInputData);
        if (CamxResultSuccess == result)
        {
            m_HVXInputData.DSPMode  = DSPModeRoundTrip;
            pInputData->HVXData     = m_HVXInputData.HVXConfiguration;
        }
        else if (CamxResultEUnsupported == result)
        {
            CAMX_LOG_ERROR(CamxLogGroupISP, "DSP operaion is not supported");
            m_HVXInputData.enableHVX = FALSE;
            result                   = CamxResultSuccess;
        }
        else
        {
            m_HVXInputData.enableHVX    = FALSE;
            CAMX_LOG_ERROR(CamxLogGroupISP, "result %d ", result);
        }
    }

    CAMX_LOG_INFO(CamxLogGroupISP, "DSenable  %d ", m_HVXInputData.HVXConfiguration.DSEnabled);

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::PrepareStreamOn
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFENode::PrepareStreamOn()
{
    CamxResult result = CamxResultSuccess;

    if ((NULL != m_pIFEHVXModule) && (TRUE == m_HVXInputData.enableHVX))
    {
        result = (static_cast<IFEHVX*>(m_pIFEHVXModule))->PrepareStreamOn();
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::OnStreamOff
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFENode::OnStreamOff(
    CHIDEACTIVATEPIPELINEMODE modeBitmask)
{
    CamxResult result = CamxResultSuccess;

    CAMX_UNREFERENCED_PARAM(modeBitmask);

    CAMX_LOG_VERBOSE(CamxLogGroupISP, "Prepare stream off ");

    if ((NULL != m_pIFEHVXModule) && (TRUE == m_HVXInputData.enableHVX))
    {
        result = (static_cast<IFEHVX*>(m_pIFEHVXModule))->OnStreamOff(modeBitmask);
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::GetFPS()
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFENode::GetFPS()
{
    CamxResult        result                               = CamxResultSuccess;
    static const UINT UsecasePropertiesIFE[]               = { PropertyIDUsecaseFPS };
    const UINT        length                               = CAMX_ARRAY_SIZE(UsecasePropertiesIFE);
    VOID*             pData[length]                        = { 0 };
    UINT64            usecasePropertyDataIFEOffset[length] = { 0 };

    GetDataList(UsecasePropertiesIFE, pData, usecasePropertyDataIFEOffset, length);

    if (NULL != pData[0])
    {
        m_FPS = *reinterpret_cast<UINT*>(pData[0]);
    }
    else
    {
        m_FPS = 30;
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::CalculateRDIClockRate
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
UINT64 IFENode::CalculateRDIClockRate()
{
    UINT64 clockRate = CalculatePixelClockRate(m_pSensorModeData->resolution.outputWidth);

    // We'll write out at 2 bytes per pixel
    clockRate = clockRate >> 1;

    // Force minimum clock rate (dependent on HW version)
    clockRate = Utils::MaxUINT64(HwEnvironment::GetInstance()->GetPlatformStaticCaps()->minIFEHWClkRate, clockRate);

    CAMX_LOG_VERBOSE(CamxLogGroupPower, "RDI clock=%llu", clockRate);
    return clockRate;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::CalculateSensorLineDuration
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
FLOAT IFENode::CalculateSensorLineDuration(
    UINT64 ifeClockRate)
{
    FLOAT lineDuration = 0;
    FLOAT lineLength   = 0.0f;
    if (0 < ifeClockRate)
    {
        if (0 != m_pSensorModeData->minHorizontalBlanking)
        {
            lineLength = static_cast<FLOAT>(m_pSensorModeData->resolution.outputWidth) +
                         static_cast<FLOAT>( m_pSensorModeData->minHorizontalBlanking);
        }
        else
        {
            lineLength = static_cast<FLOAT>(
                m_pSensorModeData->outPixelClock / (m_pSensorModeData->maxFPS * m_pSensorModeData->numLinesPerFrame));
        }

        lineDuration = lineLength * 1000000.0f / ifeClockRate;
    }

    CAMX_LOG_VERBOSE(CamxLogGroupPower, "lineDuration=%f clockRate=%llu lineLen=%f",
                     lineDuration, ifeClockRate, lineLength);

    return lineDuration;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::GetOverrideBandwidth
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IFENode::GetOverrideBandwidth(
    IFEResourceBWConfig* pBwResourceConfig,
    INT                  overrideCamnocMBytes,
    INT                  overrideExternalMBytes)
{
    CAMX_ASSERT(NULL != pBwResourceConfig);

    UINT       totalOutputPorts                 = 0;
    UINT32     outputPortId[MaxIFEOutputPorts];

    GetAllOutputPortIds(&totalOutputPorts, outputPortId);

    if (0 < overrideCamnocMBytes)
    {
        overrideCamnocMBytes = overrideExternalMBytes;
    }
    else if (0 < overrideExternalMBytes)
    {
        overrideExternalMBytes = overrideCamnocMBytes;
    }

    CheckForRDIOnly();
    UINT   numActivePorts = 0;
    if (FALSE == m_RDIOnlyUseCase)
    {
        numActivePorts++;
        if (IFEModuleMode::DualIFENormal == m_mode)
        {
            numActivePorts++;
        }
    }

    for (UINT outputPortIndex = 0; outputPortIndex < totalOutputPorts; outputPortIndex++)
    {
        switch (outputPortId[outputPortIndex])
        {
            case IFEOutputPortRDI0:
            case IFEOutputPortRDI1:
            case IFEOutputPortRDI2:
            case IFEOutputPortRDI3:
                numActivePorts++;
                break;

            default:
                break;
        }
    }

    CAMX_LOG_VERBOSE(CamxLogGroupPower, "totalOutputPorts=%d numActivePorts=%d", totalOutputPorts, numActivePorts);

    UINT64 camnocSplitBWbytes   = static_cast<UINT64>(overrideCamnocMBytes) * 1000000 / numActivePorts;
    UINT64 externalSplitBWbytes = static_cast<UINT64>(overrideExternalMBytes) * 1000000 / numActivePorts;

    if (FALSE == m_RDIOnlyUseCase)
    {
        pBwResourceConfig->leftPixelVote.camnocBWbytes    = camnocSplitBWbytes;
        pBwResourceConfig->leftPixelVote.externalBWbytes  = externalSplitBWbytes;
        CAMX_LOG_INFO(CamxLogGroupPower, "BW leftPixelVote.camnocBWbytes=%llu", camnocSplitBWbytes);
        if (IFEModuleMode::DualIFENormal == m_mode)
        {
            pBwResourceConfig->rightPixelVote.camnocBWbytes   = camnocSplitBWbytes;
            pBwResourceConfig->rightPixelVote.externalBWbytes = externalSplitBWbytes;
            CAMX_LOG_INFO(CamxLogGroupPower, "BW rightPixelVote.camnocBWbytes=%llu", camnocSplitBWbytes);
        }
    }

    for (UINT outputPortIndex = 0; outputPortIndex < totalOutputPorts; outputPortIndex++)
    {
        switch (outputPortId[outputPortIndex])
        {
            case IFEOutputPortRDI0:
            case IFEOutputPortRDI1:
            case IFEOutputPortRDI2:
            case IFEOutputPortRDI3:
                {
                    UINT32 idx = outputPortId[outputPortIndex] - IFEOutputPortRDI0;
                    pBwResourceConfig->rdiVote[idx].camnocBWbytes   = camnocSplitBWbytes;
                    pBwResourceConfig->rdiVote[idx].externalBWbytes = externalSplitBWbytes;
                    CAMX_LOG_INFO(CamxLogGroupPower, "BW RDI idx=%d camnocBWbytes=%llu", idx, camnocSplitBWbytes);
                }
                break;

            default:
                break;
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::CalculatePixelPortLineBandwidth
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
UINT64 IFENode::CalculatePixelPortLineBandwidth(
    UINT32 inputWidthInPixels,
    UINT32 outputWidthInBytes,
    UINT   IFEIndex)
{
    FLOAT  lineDuration  = static_cast<FLOAT>((inputWidthInPixels * 1000000.0f) /
                                              GetPixelClockRate(static_cast<IFESplitID>(IFEIndex)));

    UINT64 camnocBWbytes = static_cast<UINT64>( (outputWidthInBytes * 1000000.f) / lineDuration );

    return camnocBWbytes;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::CalculateBandwidth
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFENode::CalculateBandwidth(
    ExecuteProcessRequestData* pExecuteProcessRequestData,
    IFEResourceBWConfig*       pBwResourceConfig,
    UINT64                     requestId)
{
    CAMX_ASSERT(NULL != pBwResourceConfig);
    CAMX_UNREFERENCED_PARAM(requestId);

    CamxResult result                 = CamxResultSuccess;
    INT        overrideCamnocMBytes   = static_cast<INT>(GetStaticSettings()->ifeCamnocBandwidthMBytes);
    INT        overrideExternalMBytes = static_cast<INT>(GetStaticSettings()->ifeExternalBandwidthMBytes);

    if ((0 > overrideCamnocMBytes) || (0 > overrideExternalMBytes))
    {
        CAMX_LOG_INFO(CamxLogGroupPower, "BW setting disabled by override camnoc=%d external=%d",
                      overrideCamnocMBytes, overrideExternalMBytes);
        result = CamxResultEDisabled;
    }
    else if ((0 < overrideCamnocMBytes) || (0 < overrideExternalMBytes))
    {
        GetOverrideBandwidth(pBwResourceConfig, overrideCamnocMBytes, overrideExternalMBytes);
    }
    else
    {
        // Overrides are disabled. Calculate based on HW characteristics.

        UINT       ISPDataIndexBase                 = 2; // Default to single IFE base
        UINT       totalOutputPorts                 = 0;
        UINT32     outputPortId[MaxIFEOutputPorts];
        UINT       numIFEs                          = 1;

        GetAllOutputPortIds(&totalOutputPorts, outputPortId);

        if (IFEModuleMode::DualIFENormal == m_mode)
        {
            numIFEs          = 2;
            ISPDataIndexBase = 0;
        }

        PerRequestActivePorts*  pPerRequestPorts;
        UINT                    numOutputPorts   = 0;
        if (NULL == pExecuteProcessRequestData)
        {
            // Calculating for all output ports
            numOutputPorts = totalOutputPorts;
        }
        else
        {
            // Calculating for output ports that are enabled for this request
            pPerRequestPorts = pExecuteProcessRequestData->pEnabledPortsInfo;

            CAMX_ASSERT(NULL != pPerRequestPorts);

            numOutputPorts = pPerRequestPorts->numOutputPorts;
        }

        for (UINT IFEIndex = 0; IFEIndex < numIFEs; IFEIndex++)
        {
            UINT64 camnocBWbytes          = 0;
            UINT64 externalBWbytes        = 0;
            DOUBLE camnocLineBW           = 0;
            DOUBLE externalLineBW         = 0;
            UINT32 maxPixelPathInputWidth = 0;

            CAMX_ASSERT(((IFEModuleMode::DualIFENormal == m_mode) && (0 == ISPDataIndexBase)) ||
                        (2 == ISPDataIndexBase));

            ISPInternalData* pISPData = &m_ISPData[ISPDataIndexBase + IFEIndex];

            for (UINT index = 0; index < numOutputPorts; index++)
            {
                // Calculating for all output ports or for output ports that are enabled for this request
                UINT32 portId = (NULL == pExecuteProcessRequestData) ?
                                    outputPortId[index] : pPerRequestPorts->pOutputPorts[index].portId;
                UINT                outputPortIndex = OutputPortIndex(portId);

                if (TRUE == m_isDisabledOutputPort[outputPortIndex])
                {
                    continue;
                }

                const  ImageFormat* pImageFormat    = GetOutputPortImageFormat(outputPortIndex);
                UINT32              outputLineWidthInPixels;
                UINT32              portIdx;
                FLOAT               inputLineDuration = 0;
                UINT32              croppedSensorInputWidth;
                UINT32              outputWidthInBytes;

                switch (portId)
                {
                    case IFEOutputPortFull:
                    case IFEOutputPortDisplayFull:
                        {
                            if (IFEOutputPortDisplayFull == portId)
                            {
                                portIdx                 = DisplayFullOutput;
                                croppedSensorInputWidth = pISPData->metadata.appliedCropInfo.displayFullPath.width;
                            }
                            else
                            {
                                portIdx                 = FullOutput;
                                croppedSensorInputWidth = pISPData->metadata.appliedCropInfo.fullPath.width;
                            }

                            outputLineWidthInPixels   = m_stripeConfigs[IFEIndex].stream[portIdx].width;

                            UINT32 inputWidthInPixels = Utils::MaxUINT32(outputLineWidthInPixels, croppedSensorInputWidth);
                            maxPixelPathInputWidth    = Utils::MaxUINT32(maxPixelPathInputWidth, inputWidthInPixels);

                            FLOAT  bytesPerPix        = ImageFormatUtils::GetBytesPerPixel(pImageFormat);
                            UINT32 uncompressedOutputWidthInBytes;

                            if (TRUE == ImageFormatUtils::IsUBWC(pImageFormat->format))
                            {
                                FLOAT compressionRatio = (Format::UBWCTP10 == pImageFormat->format)?
                                                               IFEUBWCTP10CompressionRatio:
                                                               IFEUBWCNV12CompressionRatio;

                                // (Cuurently, Only Moorea supports IFE UBWC lossy out) Check if resolution >= 4K,
                                // make IFE output lossy.
                                if (TRUE == IsUBWCLossyNeeded(pImageFormat))
                                {
                                    if (((m_mode == IFEModuleMode::DualIFENormal)       &&
                                        (inputWidthInPixels >= UHDResolutionWidth / 2)) ||
                                        ((m_mode == IFEModuleMode::SingleIFENormal)     &&
                                        (inputWidthInPixels >= UHDResolutionWidth)))
                                    {
                                        compressionRatio = (Format::UBWCTP10 == pImageFormat->format) ?
                                            IFEUBWCTP10CompressionRatioUHD:
                                            IFEUBWCNV12CompressionRatioUHD;
                                    }
                                }

                                outputWidthInBytes = static_cast<UINT32>(outputLineWidthInPixels * bytesPerPix);
                                externalLineBW    += outputWidthInBytes / compressionRatio;

                                FLOAT uncompressedBytesPerPix = (ImageFormatUtils::Is10BitFormat(pImageFormat->format)) ?
                                                                 2.0f : 1.5f;
                                uncompressedOutputWidthInBytes = static_cast<UINT32>(outputLineWidthInPixels *
                                                                                     uncompressedBytesPerPix);
                            }
                            else
                            {
                                outputWidthInBytes  = static_cast<UINT32>(outputLineWidthInPixels * bytesPerPix);
                                externalLineBW     += outputWidthInBytes;
                                uncompressedOutputWidthInBytes = outputWidthInBytes;
                            }

                            camnocBWbytes += CalculatePixelPortLineBandwidth(inputWidthInPixels,
                                                                             uncompressedOutputWidthInBytes, IFEIndex);

                            CAMX_LOG_VERBOSE(CamxLogGroupPower,
                                             "ReqId=%llu IFEOutputPortFull Idx=%u inputWidthInPixels=%u "
                                             "outputLineWidthInPixels=%u outputWidthInBytes=%u croppedSensorInputWidth=%u "
                                             "uncompressedOutputWidthInBytes=%u format=%u externalLineBW=%lf "
                                             "camnocBWbytes=%llu",
                                             requestId, portIdx, inputWidthInPixels, outputLineWidthInPixels,
                                             outputWidthInBytes, croppedSensorInputWidth, uncompressedOutputWidthInBytes,
                                             pImageFormat->format, externalLineBW, camnocBWbytes);
                        }
                        break;

                    case IFEOutputPortDS4:
                    case IFEOutputPortDisplayDS4:
                        {
                            if (IFEOutputPortDisplayDS4 == portId)
                            {
                                portIdx = DisplayDS4Output;
                                croppedSensorInputWidth = pISPData->metadata.appliedCropInfo.displayDS4Path.width;
                            }
                            else
                            {
                                portIdx = DS4Output;
                                croppedSensorInputWidth = pISPData->metadata.appliedCropInfo.DS4Path.width;
                            }
                            outputLineWidthInPixels   = m_stripeConfigs[IFEIndex].stream[portIdx].width;

                            UINT32 inputWidthInPixels = Utils::MaxUINT32(outputLineWidthInPixels, croppedSensorInputWidth);
                            maxPixelPathInputWidth    = Utils::MaxUINT32(maxPixelPathInputWidth, inputWidthInPixels);

                            FLOAT  bytesPerPix        = ImageFormatUtils::GetBytesPerPixel(pImageFormat);

                            // 2: 2x2 tile in 8 bytes per 8 lines
                            outputWidthInBytes = static_cast<UINT32>(outputLineWidthInPixels * bytesPerPix); //  / 8.0f;
                            externalLineBW    += outputWidthInBytes;
                            camnocBWbytes     += CalculatePixelPortLineBandwidth(inputWidthInPixels,
                                                                                 outputWidthInBytes, IFEIndex);
                            CAMX_LOG_VERBOSE(CamxLogGroupPower,
                                             "ReqId=%llu IFEOutputPortDS4  Idx=%u inputWidthInPixels=%u "
                                             "outputLineWidthInPixels=%u outputWidthInBytes=%u croppedSensorInputWidth=%u "
                                             "format=%u externalLineBW=%lf camnocBWbytes=%llu",
                                             requestId, portIdx, inputWidthInPixels, outputLineWidthInPixels,
                                             outputWidthInBytes, croppedSensorInputWidth,
                                             pImageFormat->format, externalLineBW, camnocBWbytes);
                        }
                        break;

                    case IFEOutputPortDS16:
                    case IFEOutputPortDisplayDS16:
                        {
                            if (IFEOutputPortDisplayDS16 == portId)
                            {
                                portIdx = DisplayDS16Output;
                                croppedSensorInputWidth = pISPData->metadata.appliedCropInfo.displayDS16Path.width;
                            }
                            else
                            {
                                portIdx = DS16Output;
                                croppedSensorInputWidth = pISPData->metadata.appliedCropInfo.DS16Path.width;
                            }
                            outputLineWidthInPixels   = m_stripeConfigs[IFEIndex].stream[portIdx].width;
                            UINT32 inputWidthInPixels = Utils::MaxUINT32(outputLineWidthInPixels, croppedSensorInputWidth);
                            maxPixelPathInputWidth    = Utils::MaxUINT32(maxPixelPathInputWidth, inputWidthInPixels);

                            FLOAT  bytesPerPix        = ImageFormatUtils::GetBytesPerPixel(pImageFormat);

                            // 2: 2x2 tile in 8 bytes per 32 lines
                            outputWidthInBytes  = static_cast<UINT32>(outputLineWidthInPixels * bytesPerPix); // / 32.0f;
                            externalLineBW      += outputWidthInBytes;
                            camnocBWbytes       += CalculatePixelPortLineBandwidth(inputWidthInPixels,
                                                                                   outputWidthInBytes, IFEIndex);
                            CAMX_LOG_VERBOSE(CamxLogGroupPower,
                                             "ReqId=%llu IFEOutputPortDS16 Idx=%u inputWidthInPixels=%u "
                                             "outputLineWidthInPixels=%u outputWidthInBytes=%u croppedSensorInputWidth=%u "
                                             "format=%u externalLineBW=%lf camnocBWbytes=%llu",
                                             requestId, portIdx, inputWidthInPixels, outputLineWidthInPixels,
                                             outputWidthInBytes, croppedSensorInputWidth,
                                             pImageFormat->format, externalLineBW, camnocBWbytes);
                        }
                        break;

                    case IFEOutputPortFD:
                        {
                            croppedSensorInputWidth   = pISPData->metadata.appliedCropInfo.FDPath.width;
                            outputLineWidthInPixels   = m_stripeConfigs[IFEIndex].stream[FDOutput].width;
                            UINT32 inputWidthInPixels = Utils::MaxUINT32(outputLineWidthInPixels, croppedSensorInputWidth);
                            maxPixelPathInputWidth    = Utils::MaxUINT32(maxPixelPathInputWidth, inputWidthInPixels);
                            FLOAT  bytesPerPix        = static_cast<FLOAT>(ImageFormatUtils::GetBytesPerPixel(pImageFormat));
                            outputWidthInBytes        = static_cast<UINT32>(outputLineWidthInPixels * bytesPerPix);
                            externalLineBW           += outputWidthInBytes;
                            camnocBWbytes            += CalculatePixelPortLineBandwidth(inputWidthInPixels,
                                                                                        outputWidthInBytes, IFEIndex);
                            CAMX_LOG_VERBOSE(CamxLogGroupPower,
                                             "ReqId=%llu IFEOutputPortFD   inputWidthInPixels=%u "
                                             "outputLineWidthInPixels=%u outputWidthInBytes=%u croppedSensorInputWidth=%u "
                                             "format=%u externalLineBW=%lf camnocBWbytes=%llu",
                                             requestId, inputWidthInPixels, outputLineWidthInPixels,
                                             outputWidthInBytes, croppedSensorInputWidth,
                                             pImageFormat->format, externalLineBW, camnocBWbytes);
                        }
                        break;

                    case IFEOutputPortPDAF:
                    case IFEOutputPortCAMIFRaw:
                    case IFEOutputPortLSCRaw:
                    case IFEOutputPortGTMRaw:
                    case IFEOutputPortStatsDualPD:
                        CalculateRAWBandwidth(IFEIndex, portId, &camnocLineBW, &externalLineBW, requestId);
                        break;

                    case IFEOutputPortRDI0:
                    case IFEOutputPortRDI1:
                    case IFEOutputPortRDI2:
                    case IFEOutputPortRDI3:
                        if (static_cast<UINT>(IFESplitID::Left) == IFEIndex) // KMD only uses IFE0 blob for RDI votes
                        {
                            FLOAT bytesPerPix = 0.0f;

                            result = CalculateRDIBandwidth(IFEIndex,
                                                           pBwResourceConfig,
                                                           pImageFormat,
                                                           portId,
                                                           outputPortIndex,
                                                           &bytesPerPix,
                                                           requestId);
                        }
                        break;

                    default:
                        break;
                }
            }

            FLOAT overheadCamnoc = 1.0f; // This needs to be tuned
            FLOAT overheadExt    = 1.0f; // This needs to be tuned

            UINT64 rawPortsCamnocBWbytes;
            if (0 != m_pSensorModeData->minHorizontalBlanking)
            {
                // We need to write out camnocLineBW bytes within one line time or IFE will overflow. Since
                // the IFE has a one-line buffer and we can use the entire line width including Horizontal blanking
                // duration to write out the data, we amortize (spread) the BW over the blanking period as well.
                // For Dual-IFE, the blanking will include the time remaining after CAMIF crop - so we have more
                // time to write out the same data. However, the net BW will be similar to single IFE since the
                // line is split over 2 IFEs.
                rawPortsCamnocBWbytes =
                    static_cast<UINT64>((overheadCamnoc * 1000000.0f * camnocLineBW) / m_sensorLineTimeMSecs[IFEIndex]);
                externalBWbytes +=
                    static_cast<UINT64>((overheadExt * 1000000.0f * externalLineBW) / m_sensorLineTimeMSecs[IFEIndex]);
            }
            else
            {
                // Be more conservative when we don't have accurate blanking data
                // DOUBLE sensorLineTime = 1 / m_pSensorModeData->maxFPS / m_pSensorModeData->numLinesPerFrame;
                // totalBW = (lineBW / sensorLineTime);
                // Reordered to avoid loss of precision and avoid divisions:

                rawPortsCamnocBWbytes = static_cast<UINT64>(overheadCamnoc * camnocLineBW *
                                                       m_pSensorModeData->resolution.outputHeight * m_pSensorModeData->maxFPS);
                externalBWbytes      += static_cast<UINT64>(overheadExt * externalLineBW *
                                                       m_pSensorModeData->resolution.outputHeight * m_pSensorModeData->maxFPS);
            }
            camnocBWbytes += rawPortsCamnocBWbytes;

            // Account for zoom
            if (0 < maxPixelPathInputWidth)
            {
                externalBWbytes = externalBWbytes * m_pSensorModeData->resolution.outputWidth / maxPixelPathInputWidth;
            }

            // Add fixed 90MB BW for stats outputs based on modelling recommendation
            const UINT32 StatsBW = 90000000 / numIFEs;
            camnocBWbytes       += StatsBW;
            externalBWbytes     += StatsBW;

            // Workaround for overflow on use case transitions
            if (m_highInitialBWCnt < GetStaticSettings()->IFENumFramesHighBW)
            {
                camnocBWbytes   *= 5;
                externalBWbytes *= 5;
            }

            CAMX_LOG_VERBOSE(CamxLogGroupPower,
                             "ReqId=%llu ------- PixelPathTotal IFE[%d]: InstanceID=%u "
                             "HalCrop.width=%u maxPixelPathInputWidth=%u camnocBWbytes=%llu externalBWbytes=%llu",
                             requestId, IFEIndex, InstanceID(), m_HALTagsData.HALCrop.width, maxPixelPathInputWidth,
                             camnocBWbytes, externalBWbytes);

            if (static_cast<UINT>(IFESplitID::Left) == IFEIndex)
            {
                pBwResourceConfig->leftPixelVote.camnocBWbytes   = camnocBWbytes;
                pBwResourceConfig->leftPixelVote.externalBWbytes = externalBWbytes;
            }
            else
            {
                pBwResourceConfig->rightPixelVote.camnocBWbytes   = camnocBWbytes;
                pBwResourceConfig->rightPixelVote.externalBWbytes = externalBWbytes;
            }

        }
        m_highInitialBWCnt++;
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::CalculateActiveBandwidth
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFENode::CalculateActiveBandwidth(
    ExecuteProcessRequestData* pExecuteProcessRequestData,
    IFEResourceBWConfig*       pBwResourceConfig,
    IFEResourceBWConfigAB*     pBwResourceConfigAB,
    UINT64                     requestId)
{
    CAMX_ASSERT(NULL != pBwResourceConfig);
    CAMX_ASSERT(NULL != pBwResourceConfigAB);
    CAMX_UNREFERENCED_PARAM(requestId);

    CamxResult result                 = CamxResultSuccess;
    INT        overrideCamnocMBytes   = static_cast<INT>(GetStaticSettings()->ifeCamnocBandwidthMBytes);
    INT        overrideExternalMBytes = static_cast<INT>(GetStaticSettings()->ifeExternalBandwidthMBytes);

    if ((0 > overrideCamnocMBytes) || (0 > overrideExternalMBytes))
    {
        CAMX_LOG_INFO(CamxLogGroupPower, "BW setting disabled by override camnoc=%d external=%d",
                      overrideCamnocMBytes, overrideExternalMBytes);
        result = CamxResultEDisabled;
    }
    else if ((0 < overrideCamnocMBytes) || (0 < overrideExternalMBytes))
    {
        GetOverrideBandwidth(pBwResourceConfig, overrideCamnocMBytes, overrideExternalMBytes);
        // In case bandwidth is specified using override settings avoid setting AB and use AB = IB
        m_computeActiveABVotes = FALSE;
    }
    else
    {
        // Overrides are disabled. Calculate based on HW characteristics.

        UINT       ISPDataIndexBase                 = 2; // Default to single IFE base
        UINT       totalOutputPorts                 = 0;
        UINT       numIFEs                          = 1;
        UINT32     outputPortId[MaxIFEOutputPorts];

        GetAllOutputPortIds(&totalOutputPorts, outputPortId);

        if (IFEModuleMode::DualIFENormal == m_mode)
        {
            numIFEs          = 2;
            ISPDataIndexBase = 0;
        }

        PerRequestActivePorts*  pPerRequestPorts;
        UINT                    numOutputPorts   = 0;
        if (NULL == pExecuteProcessRequestData)
        {
            // Calculating for all output ports
            numOutputPorts = totalOutputPorts;
        }
        else
        {
            // Calculating for output ports that are enabled for this request
            pPerRequestPorts = pExecuteProcessRequestData->pEnabledPortsInfo;

            CAMX_ASSERT(NULL != pPerRequestPorts);

            numOutputPorts = pPerRequestPorts->numOutputPorts;
        }

        UINT32 sensorVBI              = m_pSensorModeData->numLinesPerFrame - m_pSensorModeData->resolution.outputHeight;
        FLOAT  bandwidthMultiplierAB  = (m_pSensorModeData->resolution.outputHeight) /
                                         static_cast<FLOAT>(m_pSensorModeData->resolution.outputHeight + sensorVBI);
        // For HFR use cases the IFE display and FD ports should be configured to run at lower FPS
        UINT32 previewFPS             = m_FPS / m_usecaseNumBatchedFrames;
        CAMX_LOG_VERBOSE(CamxLogGroupPower, "sensorVBI=%u Sensor FLL:%u Height:%u bwMultiplier:%f m_FPS:%u PreviewFPS:%u "
                         "batchSize=%u",
                         sensorVBI, m_pSensorModeData->numLinesPerFrame,
                         m_pSensorModeData->resolution.outputHeight, bandwidthMultiplierAB,
                         m_FPS, previewFPS, m_usecaseNumBatchedFrames);

        for (UINT IFEIndex = 0; IFEIndex < numIFEs; IFEIndex++)
        {
            UINT64 camnocBWbytes          = 0;
            UINT64 externalBWbytes        = 0;
            UINT64 externalBWbytesAB      = 0;
            DOUBLE camnocLineBW           = 0;
            DOUBLE PDAFLineBW             = 0;
            DOUBLE externalLineBW         = 0;
            UINT32 maxPixelPathInputWidth = 0;

            CAMX_ASSERT(((IFEModuleMode::DualIFENormal == m_mode) && (0 == ISPDataIndexBase)) ||
                        (2 == ISPDataIndexBase));

            ISPInternalData* pISPData = &m_ISPData[ISPDataIndexBase + IFEIndex];

            for (UINT index = 0; index < numOutputPorts; index++)
            {
                // Calculating for all output ports or for output ports that are enabled for this request
                UINT32 portId = (NULL == pExecuteProcessRequestData) ?
                                    outputPortId[index] : pPerRequestPorts->pOutputPorts[index].portId;
                UINT                outputPortIndex = OutputPortIndex(portId);

                if (TRUE == m_isDisabledOutputPort[outputPortIndex])
                {
                    continue;
                }

                const  ImageFormat* pImageFormat    = GetOutputPortImageFormat(outputPortIndex);
                UINT32              outputLineWidthInPixels;
                UINT32              portIdx;
                FLOAT               inputLineDuration = 0;
                UINT32              croppedSensorInputWidth;
                UINT32              outputWidthInBytes;
                UINT64              outputWidthInBytesAB = 0;

                switch (portId)
                {
                    case IFEOutputPortFull:
                    case IFEOutputPortDisplayFull:
                        {
                            if (IFEOutputPortDisplayFull == portId)
                            {
                                portIdx                 = DisplayFullOutput;
                                croppedSensorInputWidth = pISPData->metadata.appliedCropInfo.displayFullPath.width;
                            }
                            else
                            {
                                portIdx                 = FullOutput;
                                croppedSensorInputWidth = pISPData->metadata.appliedCropInfo.fullPath.width;
                            }

                            outputLineWidthInPixels   = m_stripeConfigs[IFEIndex].stream[portIdx].width;

                            UINT32 inputWidthInPixels = Utils::MaxUINT32(outputLineWidthInPixels, croppedSensorInputWidth);
                            maxPixelPathInputWidth    = Utils::MaxUINT32(maxPixelPathInputWidth, inputWidthInPixels);

                            FLOAT  bytesPerPix        = ImageFormatUtils::GetBytesPerPixel(pImageFormat);
                            UINT32 uncompressedOutputWidthInBytes;

                            if (TRUE == ImageFormatUtils::IsUBWC(pImageFormat->format))
                            {
                                FLOAT compressionRatio = (Format::UBWCTP10 == pImageFormat->format)?
                                                               IFEUBWCTP10CompressionRatio:
                                                               IFEUBWCNV12CompressionRatio;

                                if (TRUE == IsUBWCLossyNeeded(pImageFormat))
                                {
                                    if (((m_mode == IFEModuleMode::DualIFENormal)       &&
                                        (inputWidthInPixels >= UHDResolutionWidth / 2)) ||
                                        ((m_mode == IFEModuleMode::SingleIFENormal)     &&
                                        (inputWidthInPixels >= UHDResolutionWidth)))
                                    {
                                        compressionRatio = (Format::UBWCTP10 == pImageFormat->format) ?
                                            IFEUBWCTP10CompressionRatioUHD:
                                            IFEUBWCNV12CompressionRatioUHD;
                                    }
                                }

                                outputWidthInBytes = static_cast<UINT32>(outputLineWidthInPixels * bytesPerPix);
                                externalLineBW    += outputWidthInBytes / compressionRatio;

                                FLOAT uncompressedBytesPerPix = (ImageFormatUtils::Is10BitFormat(pImageFormat->format)) ?
                                                                 2.0f : 1.5f;
                                uncompressedOutputWidthInBytes = static_cast<UINT32>(outputLineWidthInPixels *
                                                                                     uncompressedBytesPerPix);

                                outputWidthInBytesAB = m_stripeConfigs[IFEIndex].stream[portIdx].width *
                                                       m_stripeConfigs[IFEIndex].stream[portIdx].height *
                                                       bytesPerPix /compressionRatio;
                            }
                            else
                            {
                                outputWidthInBytes  = static_cast<UINT32>(outputLineWidthInPixels * bytesPerPix);
                                externalLineBW     += outputWidthInBytes;
                                uncompressedOutputWidthInBytes = outputWidthInBytes;

                                outputWidthInBytesAB = m_stripeConfigs[IFEIndex].stream[portIdx].width *
                                                       m_stripeConfigs[IFEIndex].stream[portIdx].height *
                                                       bytesPerPix;
                            }

                            camnocBWbytes += CalculatePixelPortLineBandwidth(inputWidthInPixels,
                                                                             uncompressedOutputWidthInBytes, IFEIndex);

                            CAMX_LOG_VERBOSE(CamxLogGroupPower,
                                             "ReqId=%llu IFEOutputPortFull Idx=%u inputWidthInPixels=%u "
                                             "outputLineWidthInPixels=%u outputWidthInBytes=%u croppedSensorInputWidth=%u "
                                             "uncompressedOutputWidthInBytes=%u format=%u externalLineBW=%lf "
                                             "camnocBWbytes=%llu",
                                             requestId, portIdx, inputWidthInPixels, outputLineWidthInPixels,
                                             outputWidthInBytes, croppedSensorInputWidth, uncompressedOutputWidthInBytes,
                                             pImageFormat->format, externalLineBW, camnocBWbytes);

                            if (IFEOutputPortFull == portId)
                            {
                                outputWidthInBytesAB *= m_FPS;
                            }
                            else
                            {
                                outputWidthInBytesAB *= previewFPS;
                            }
                            externalBWbytesAB  += outputWidthInBytesAB;
                            CAMX_LOG_VERBOSE(CamxLogGroupPower,
                                             "ReqId=%llu IFEOutputPortFull Idx=%u bytsPerPix:%f Dims:[%ux%u] "
                                             "outputWidthInBytesAB=%llu externalBWbytesAB=%llu",
                                             requestId, portIdx, bytesPerPix, m_stripeConfigs[IFEIndex].stream[portIdx].width,
                                             m_stripeConfigs[IFEIndex].stream[portIdx].height, outputWidthInBytesAB,
                                             externalBWbytesAB);
                        }
                        break;

                    case IFEOutputPortDS4:
                    case IFEOutputPortDisplayDS4:
                        {
                            if (IFEOutputPortDisplayDS4 == portId)
                            {
                                portIdx = DisplayDS4Output;
                                croppedSensorInputWidth = pISPData->metadata.appliedCropInfo.displayDS4Path.width;
                            }
                            else
                            {
                                portIdx = DS4Output;
                                croppedSensorInputWidth = pISPData->metadata.appliedCropInfo.DS4Path.width;
                            }
                            outputLineWidthInPixels   = m_stripeConfigs[IFEIndex].stream[portIdx].width;

                            UINT32 inputWidthInPixels = Utils::MaxUINT32(outputLineWidthInPixels, croppedSensorInputWidth);
                            maxPixelPathInputWidth    = Utils::MaxUINT32(maxPixelPathInputWidth, inputWidthInPixels);

                            FLOAT  bytesPerPix        = ImageFormatUtils::GetBytesPerPixel(pImageFormat);

                            // 2: 2x2 tile in 8 bytes per 8 lines
                            outputWidthInBytes = static_cast<UINT32>(outputLineWidthInPixels * bytesPerPix); //  / 8.0f;
                            externalLineBW    += outputWidthInBytes;
                            camnocBWbytes     += CalculatePixelPortLineBandwidth(inputWidthInPixels,
                                                                                 outputWidthInBytes, IFEIndex);
                            CAMX_LOG_VERBOSE(CamxLogGroupPower,
                                             "ReqId=%llu IFEOutputPortDS4  Idx=%u inputWidthInPixels=%u "
                                             "outputLineWidthInPixels=%u outputWidthInBytes=%u croppedSensorInputWidth=%u "
                                             "format=%u externalLineBW=%lf camnocBWbytes=%llu",
                                             requestId, portIdx, inputWidthInPixels, outputLineWidthInPixels,
                                             outputWidthInBytes, croppedSensorInputWidth,
                                             pImageFormat->format, externalLineBW, camnocBWbytes);

                            outputWidthInBytesAB = m_stripeConfigs[IFEIndex].stream[portIdx].width *
                                                   m_stripeConfigs[IFEIndex].stream[portIdx].height *
                                                   bytesPerPix;
                            if (IFEOutputPortDS4 == portId)
                            {
                                outputWidthInBytesAB *= m_FPS;
                            }
                            else
                            {
                                outputWidthInBytesAB *= previewFPS;
                            }
                            externalBWbytesAB  += outputWidthInBytesAB;
                            CAMX_LOG_VERBOSE(CamxLogGroupPower,
                                             "ReqId=%llu IFEOutputPortDS4  Idx=%u bytsPerPix:%f Dims:[%ux%u] "
                                             "outputWidthInBytesAB=%llu externalBWbytesAB=%llu",
                                             requestId, portIdx, bytesPerPix, m_stripeConfigs[IFEIndex].stream[portIdx].width,
                                             m_stripeConfigs[IFEIndex].stream[portIdx].height, outputWidthInBytesAB,
                                             externalBWbytesAB);
                        }
                        break;

                    case IFEOutputPortDS16:
                    case IFEOutputPortDisplayDS16:
                        {
                            if (IFEOutputPortDisplayDS16 == portId)
                            {
                                portIdx = DisplayDS16Output;
                                croppedSensorInputWidth = pISPData->metadata.appliedCropInfo.displayDS16Path.width;
                            }
                            else
                            {
                                portIdx = DS16Output;
                                croppedSensorInputWidth = pISPData->metadata.appliedCropInfo.DS16Path.width;
                            }
                            outputLineWidthInPixels   = m_stripeConfigs[IFEIndex].stream[portIdx].width;
                            UINT32 inputWidthInPixels = Utils::MaxUINT32(outputLineWidthInPixels, croppedSensorInputWidth);
                            maxPixelPathInputWidth    = Utils::MaxUINT32(maxPixelPathInputWidth, inputWidthInPixels);

                            FLOAT  bytesPerPix        = ImageFormatUtils::GetBytesPerPixel(pImageFormat);

                            // 2: 2x2 tile in 8 bytes per 32 lines
                            outputWidthInBytes  = static_cast<UINT32>(outputLineWidthInPixels * bytesPerPix); // / 32.0f;
                            externalLineBW      += outputWidthInBytes;
                            camnocBWbytes       += CalculatePixelPortLineBandwidth(inputWidthInPixels,
                                                                                   outputWidthInBytes, IFEIndex);
                            CAMX_LOG_VERBOSE(CamxLogGroupPower,
                                             "ReqId=%llu IFEOutputPortDS16 Idx=%u inputWidthInPixels=%u "
                                             "outputLineWidthInPixels=%u outputWidthInBytes=%u croppedSensorInputWidth=%u "
                                             "format=%u externalLineBW=%lf camnocBWbytes=%llu",
                                             requestId, portIdx, inputWidthInPixels, outputLineWidthInPixels,
                                             outputWidthInBytes, croppedSensorInputWidth,
                                             pImageFormat->format, externalLineBW, camnocBWbytes);

                            outputWidthInBytesAB = m_stripeConfigs[IFEIndex].stream[portIdx].width *
                                                   m_stripeConfigs[IFEIndex].stream[portIdx].height *
                                                   bytesPerPix;
                            if (IFEOutputPortDS16 == portId)
                            {
                                outputWidthInBytesAB *= m_FPS;
                            }
                            else
                            {
                                outputWidthInBytesAB *= previewFPS;
                            }
                            externalBWbytesAB  += outputWidthInBytesAB;
                            CAMX_LOG_VERBOSE(CamxLogGroupPower,
                                             "ReqId=%llu IFEOutputPortDS16 Idx=%u bytsPerPix:%f Dims:[%ux%u] "
                                             "outputWidthInBytesAB=%llu externalBWbytesAB=%llu",
                                             requestId, portIdx, bytesPerPix, m_stripeConfigs[IFEIndex].stream[portIdx].width,
                                             m_stripeConfigs[IFEIndex].stream[portIdx].height, outputWidthInBytesAB,
                                             externalBWbytesAB);
                        }
                        break;

                    case IFEOutputPortFD:
                        {
                            croppedSensorInputWidth   = pISPData->metadata.appliedCropInfo.FDPath.width;
                            outputLineWidthInPixels   = m_stripeConfigs[IFEIndex].stream[FDOutput].width;
                            UINT32 inputWidthInPixels = Utils::MaxUINT32(outputLineWidthInPixels, croppedSensorInputWidth);
                            maxPixelPathInputWidth    = Utils::MaxUINT32(maxPixelPathInputWidth, inputWidthInPixels);
                            FLOAT  bytesPerPix        = static_cast<FLOAT>(ImageFormatUtils::GetBytesPerPixel(pImageFormat));
                            outputWidthInBytes        = static_cast<UINT32>(outputLineWidthInPixels * bytesPerPix);
                            externalLineBW           += outputWidthInBytes;
                            camnocBWbytes            += CalculatePixelPortLineBandwidth(inputWidthInPixels,
                                                                                        outputWidthInBytes, IFEIndex);
                            CAMX_LOG_VERBOSE(CamxLogGroupPower,
                                             "ReqId=%llu IFEOutputPortFD   inputWidthInPixels=%u "
                                             "outputLineWidthInPixels=%u outputWidthInBytes=%u croppedSensorInputWidth=%u "
                                             "format=%u externalLineBW=%lf camnocBWbytes=%llu",
                                             requestId, inputWidthInPixels, outputLineWidthInPixels,
                                             outputWidthInBytes, croppedSensorInputWidth,
                                             pImageFormat->format, externalLineBW, camnocBWbytes);

                            outputWidthInBytesAB = m_stripeConfigs[IFEIndex].stream[FDOutput].width *
                                                   m_stripeConfigs[IFEIndex].stream[FDOutput].height *
                                                   bytesPerPix * previewFPS;
                            externalBWbytesAB   += outputWidthInBytesAB;
                            CAMX_LOG_VERBOSE(CamxLogGroupPower,
                                             "ReqId=%llu IFEOutputPortFD   bytsPerPix:%f Dims:[%ux%u] "
                                             "outputWidthInBytesAB=%llu externalBWbytesAB=%llu",
                                             requestId, bytesPerPix, m_stripeConfigs[IFEIndex].stream[FDOutput].width,
                                             m_stripeConfigs[IFEIndex].stream[FDOutput].height, outputWidthInBytesAB,
                                             externalBWbytesAB);
                        }
                        break;

                    case IFEOutputPortPDAF:
                        {
                            CalculateRAWBandwidth(IFEIndex, portId, &camnocLineBW, &externalLineBW, requestId);

                            PDAFLineBW           = m_stripeConfigs[IFEIndex].stream[PDAFRawOutput].width * 2.0; // Plain16
                            FLOAT  bytesPerPix   = static_cast<FLOAT>(ImageFormatUtils::GetBytesPerPixel(pImageFormat));
                            outputWidthInBytesAB = m_stripeConfigs[IFEIndex].stream[PDAFRawOutput].width  *
                                                   m_stripeConfigs[IFEIndex].stream[PDAFRawOutput].height *
                                                   bytesPerPix * m_FPS;
                            externalBWbytesAB   += outputWidthInBytesAB;
                            CAMX_LOG_VERBOSE(CamxLogGroupPower,
                                             "ReqId=%llu IFEOutputPortPDAF bytsPerPix:%f Dims:[%ux%u] "
                                             "outputWidthInBytesAB=%llu externalBWbytesAB=%llu",
                                             requestId, bytesPerPix, m_stripeConfigs[IFEIndex].stream[PDAFRawOutput].width,
                                             m_stripeConfigs[IFEIndex].stream[PDAFRawOutput].height, outputWidthInBytesAB,
                                             externalBWbytesAB);
                        }
                        break;

                    case IFEOutputPortCAMIFRaw:
                    case IFEOutputPortLSCRaw:
                    case IFEOutputPortGTMRaw:
                    case IFEOutputPortStatsDualPD:
                        CalculateRAWBandwidth(IFEIndex, portId, &camnocLineBW, &externalLineBW, requestId);
                        break;

                    case IFEOutputPortRDI0:
                    case IFEOutputPortRDI1:
                    case IFEOutputPortRDI2:
                    case IFEOutputPortRDI3:
                        if (static_cast<UINT>(IFESplitID::Left) == IFEIndex) // KMD only uses IFE0 blob for RDI votes
                        {
                            FLOAT bytesPerPix = 0.0f;

                            result = CalculateRDIBandwidth(IFEIndex,
                                                           pBwResourceConfig,
                                                           pImageFormat,
                                                           portId,
                                                           outputPortIndex,
                                                           &bytesPerPix,
                                                           requestId);
                            if (CamxResultSuccess == result)
                            {
                                UINT32 idx         = portId - IFEOutputPortRDI0;
                                DOUBLE bandwidthAB = m_pSensorModeData->resolution.outputWidth  *
                                                        m_pSensorModeData->resolution.outputHeight *
                                                        bytesPerPix * m_FPS * bandwidthMultiplierAB;
                                pBwResourceConfigAB->rdiVoteAB[idx] = static_cast<UINT64>(bandwidthAB);
                                CAMX_LOG_VERBOSE(CamxLogGroupPower,
                                    "ReqId=%llu IFEOutputPortRDIx[%d] bytesPerPix=%f Dims:[%ux%u] "
                                    "externalBWbytesAB=%llu", requestId, idx, bytesPerPix,
                                    m_pSensorModeData->resolution.outputWidth, m_pSensorModeData->resolution.outputHeight,
                                    pBwResourceConfigAB->rdiVoteAB[idx]);
                            }
                        }
                        break;

                    default:
                        break;
                }
            }

            FLOAT overheadCamnoc = 1.0f; // This needs to be tuned
            FLOAT overheadExt    = 1.0f; // This needs to be tuned

            UINT64 rawPortsCamnocBWbytes;
            if (0 != m_pSensorModeData->minHorizontalBlanking)
            {
                // We need to write out camnocLineBW bytes within one line time or IFE will overflow. Since
                // the IFE has a one-line buffer and we can use the entire line width including Horizontal blanking
                // duration to write out the data, we amortize (spread) the BW over the blanking period as well.
                // For Dual-IFE, the blanking will include the time remaining after CAMIF crop - so we have more
                // time to write out the same data. However, the net BW will be similar to single IFE since the
                // line is split over 2 IFEs.
                rawPortsCamnocBWbytes =
                    static_cast<UINT64>((overheadCamnoc * 1000000.0f * camnocLineBW) / m_sensorLineTimeMSecs[IFEIndex]);
                externalBWbytes +=
                    static_cast<UINT64>((overheadExt * 1000000.0f * externalLineBW) / m_sensorLineTimeMSecs[IFEIndex]);
            }
            else
            {
                // Be more conservative when we don't have accurate blanking data
                // DOUBLE sensorLineTime = 1 / m_pSensorModeData->maxFPS / m_pSensorModeData->numLinesPerFrame;
                // totalBW = (lineBW / sensorLineTime);
                // Reordered to avoid loss of precision and avoid divisions:

                rawPortsCamnocBWbytes = static_cast<UINT64>(overheadCamnoc * camnocLineBW *
                                                       m_pSensorModeData->resolution.outputHeight * m_pSensorModeData->maxFPS);
                externalBWbytes += static_cast<UINT64>(overheadExt * externalLineBW *
                                                       m_pSensorModeData->resolution.outputHeight * m_pSensorModeData->maxFPS);
            }
            camnocBWbytes += rawPortsCamnocBWbytes;

            // Subtract the PDAF bandwidth since it is already accounted in external AB bandwidth
            UINT64 rawPortsBWbytesAB   = static_cast<UINT64>((camnocLineBW - PDAFLineBW)*
                                                       m_pSensorModeData->resolution.outputHeight * m_FPS);
            externalBWbytesAB += rawPortsBWbytesAB;
            CAMX_LOG_VERBOSE(CamxLogGroupPower, "ReqId=%llu IFEOutputPortsRAW rawPortsBWbytesAB=%llu",
                          requestId, rawPortsBWbytesAB);

            // Account for zoom
            if (0 < maxPixelPathInputWidth)
            {
                externalBWbytes = externalBWbytes * m_pSensorModeData->resolution.outputWidth / maxPixelPathInputWidth;
            }

            // Add fixed 90MB BW for stats outputs based on modelling recommendation
            const UINT32 StatsBW = 90000000 / numIFEs;
            camnocBWbytes       += StatsBW;
            externalBWbytes     += StatsBW;
            externalBWbytesAB   += StatsBW;

            // take into account sensor vertical blanking
            externalBWbytesAB *= bandwidthMultiplierAB;

            // Workaround for overflow on use case transitions
            if (m_highInitialBWCnt < GetStaticSettings()->IFENumFramesHighBW)
            {
                camnocBWbytes   *= 5;
                externalBWbytes *= 5;
            }

            CAMX_LOG_VERBOSE(CamxLogGroupPower,
                             "ReqId=%llu ------- PixelPathTotal IFE[%d]: InstanceID=%u "
                             "HalCrop.width=%u maxPixelPathInputWidth=%u camnocBWbytes=%llu externalBWbytes=%llu",
                             requestId, IFEIndex, InstanceID(), m_HALTagsData.HALCrop.width, maxPixelPathInputWidth,
                             camnocBWbytes, externalBWbytes);
            CAMX_LOG_VERBOSE(CamxLogGroupPower,
                             "ReqId=%llu ------- PixelPathTotal IFE[%d]: InstanceID=%u externalBWbytesAB=%llu",
                             requestId, IFEIndex, InstanceID(), externalBWbytesAB);

            if (static_cast<UINT>(IFESplitID::Left) == IFEIndex)
            {
                pBwResourceConfig->leftPixelVote.camnocBWbytes   = camnocBWbytes;
                pBwResourceConfig->leftPixelVote.externalBWbytes = externalBWbytes;
                pBwResourceConfigAB->leftPixelVoteAB             = externalBWbytesAB;
            }
            else
            {
                pBwResourceConfig->rightPixelVote.camnocBWbytes   = camnocBWbytes;
                pBwResourceConfig->rightPixelVote.externalBWbytes = externalBWbytes;
                pBwResourceConfigAB->rightPixelVoteAB             = externalBWbytesAB;
            }

        }
        m_highInitialBWCnt++;
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::CalculateRAWBandwidth
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IFENode::CalculateRAWBandwidth(
    UINT    IFEIndex,
    UINT32  portId,
    DOUBLE* pCamnocLineBW,
    DOUBLE* pExternalLineBW,
    UINT64  requestId)
{
    UINT32 outputLineWidthInPixels = 0;

    switch (portId)
    {
        case IFEOutputPortPDAF:
            // hard-coded system modelling recommendation - may need to refine with actual PD ROI
            // outputLineWidthInPixels = m_pSensorModeData->resolution.outputWidth * 0.6;
            outputLineWidthInPixels = m_stripeConfigs[IFEIndex].stream[PDAFRawOutput].width;
            *pExternalLineBW       += (outputLineWidthInPixels * 2.0); // Plain16;
            *pCamnocLineBW         += (outputLineWidthInPixels * 2.0); // Plain16;
            CAMX_LOG_VERBOSE(CamxLogGroupPower,
                             "ReqId=%llu IFEOutputPortPDAF outputLineWidthInPixels=%u "
                             "camnocLineBW=externalLineBW=%lf",
                             requestId, outputLineWidthInPixels, *pExternalLineBW);
            break;

        case IFEOutputPortCAMIFRaw:
        case IFEOutputPortLSCRaw:
            // Need to account for Crop block at PIXEL_RAW_DUMP_OUT
            outputLineWidthInPixels = m_pSensorModeData->resolution.outputWidth;
            *pExternalLineBW += (outputLineWidthInPixels * 2); // Bayer14 in 16 bit words
            *pCamnocLineBW   += (outputLineWidthInPixels * 2);
            CAMX_LOG_VERBOSE(CamxLogGroupPower,
                             "ReqId=%llu CAMIF/LSC Raw Port=%u outputLineWidthInPixels=%d externalLineBW=%lf",
                             requestId, portId, outputLineWidthInPixels, *pExternalLineBW);
            break;

        case IFEOutputPortGTMRaw:
            // Need to account for Crop block at PIXEL_RAW_DUMP_OUT
            outputLineWidthInPixels = m_pSensorModeData->resolution.outputWidth;
            // ARGB Plain16 packed, 16bits for each of the 4 channels
            *pExternalLineBW += (outputLineWidthInPixels * 4 * 2);
            *pCamnocLineBW   += (outputLineWidthInPixels * 4 * 2);
            CAMX_LOG_VERBOSE(CamxLogGroupPower,
                             "ReqId=%llu IFEOutputPortGTMRaw outputLineWidthInPixels=%d externalLineBW=%lf",
                             requestId, outputLineWidthInPixels, *pExternalLineBW);
            break;

        case IFEOutputPortStatsDualPD:
            // Vote For Single IFE as the DUALPD output is from RDI currently
            if (0 == IFEIndex)
            {
                UINT PDAFstreamIndex;

                if (TRUE == FindSensorStreamConfigIndex(StreamType::PDAF, &PDAFstreamIndex))
                {
                    UINT32 PDAFWidth = m_pSensorModeData->streamConfig[PDAFstreamIndex].frameDimension.width;
                    CAMX_LOG_VERBOSE(CamxLogGroupISP, "PDAF RDI_%d w=%d", portId, PDAFWidth);
                    *pExternalLineBW += (PDAFWidth * 2); // PLAIN16 Format
                    *pCamnocLineBW   += (PDAFWidth * 2); // PLAIN16 Format
                }
            }
            break;

        default:
            // Not a RAW port. Nothing to do
            break;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::CalculateRDIBandwidth
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFENode::CalculateRDIBandwidth(
    UINT                 IFEIndex,
    IFEResourceBWConfig* pBwResourceConfig,
    const ImageFormat*   pImageFormat,
    UINT32               portId,
    UINT                 outputPortIndex,
    FLOAT*               pBytesPerPixel,
    UINT64               requestId)
{
    CamxResult result = CamxResultSuccess;
    UINT32     idx    = portId - IFEOutputPortRDI0;
    if (RDIMaxNum > idx)
    {
        FLOAT              bytesPerPix        = 0.0f;
        const RawFormat*   pRawFormat         = &(pImageFormat->formatParams.rawFormat);

        if (CamX::Format::Blob == pImageFormat->format)
        {
            UINT32 streamType =
                TranslatePortSourceTypeToSensorStreamConfigType(
                    GetOutputPortSourceType(outputPortIndex));

            UINT streamIndex;
            bytesPerPix = 0.0f;
            if (TRUE == FindSensorStreamConfigIndex(
                static_cast<StreamType>(streamType), &streamIndex))
            {
                bytesPerPix = m_pSensorModeData->streamConfig[streamIndex].bitWidth / 8.0f;
            }
            else
            {
                CAMX_LOG_ERROR(CamxLogGroupPower, "Invalid streamType=%u", streamType);
            }
        }
        else
        {
            bytesPerPix = ImageFormatUtils::GetBytesPerPixel(pImageFormat);
        }

        if (0.0f < bytesPerPix)
        {
            // UINT32 stride = pRawFormat->stride;
            UINT32 stride = m_pSensorModeData->resolution.outputWidth;

            DOUBLE bandwidth = (stride * 1000000.0f * bytesPerPix) / m_sensorLineTimeMSecs[IFEIndex];

            // Double bandwidth request in FS mode since there will be an additional read
            /// @todo (CAMX-4450) Update Bandwidth and Clock voting for FastShutter Usecase
            if ((TRUE == m_enableBusRead) &&
                (IFEOutputPortRDI0 == portId))
            {
                DOUBLE bw = bandwidth;
                bandwidth *= 2;
                CAMX_LOG_INFO(CamxLogGroupISP, "FS: Increase bandwidth %lf to %lf",
                    bw, bandwidth);
            }

            if (TRUE == m_RDIOnlyUseCase)
            {
                // Compensate for overly aggressive DCD in RDI-only case - needs to be tuned
                bandwidth *= 5.0f;
            }
            else
            {
                // Overwrite if the RDI port is associated with PDAF port source type.
                if (PortSrcTypePDAF == GetOutputPortSourceType(outputPortIndex))
                {
                    UINT PDAFstreamIndex;

                    if (TRUE == FindSensorStreamConfigIndex(StreamType::PDAF, &PDAFstreamIndex))
                    {
                        UINT32 PDAFHeight =
                            m_pSensorModeData->streamConfig[PDAFstreamIndex].frameDimension.height;

                        CAMX_LOG_VERBOSE(CamxLogGroupPower, "PDAF RDI_%d h=%d full h=%d",
                            portId, PDAFHeight, m_pSensorModeData->resolution.outputHeight);

                        bandwidth *= (static_cast<DOUBLE>(PDAFHeight) /
                            static_cast<DOUBLE>(m_pSensorModeData->resolution.outputHeight));
                    }
                }
            }

            if (m_highInitialBWCnt < GetStaticSettings()->IFENumFramesHighBW)
            {
                bandwidth *= 5.0f;
            }

            pBwResourceConfig->rdiVote[idx].camnocBWbytes   = static_cast<UINT64>(bandwidth);
            pBwResourceConfig->rdiVote[idx].externalBWbytes = static_cast<UINT64>(bandwidth);

            *pBytesPerPixel = bytesPerPix;

            CAMX_LOG_VERBOSE(CamxLogGroupPower,
                "ReqId=%llu IFEOutputPortRDIx[%d] format=%u stride=%u bytesPerPix=%f "
                "camnocBWbytes==externalBWbytes=%llu",
                requestId, idx, pImageFormat->format, stride, bytesPerPix,
                pBwResourceConfig->rdiVote[idx].camnocBWbytes);
        }
        else
        {
            CAMX_LOG_ERROR(CamxLogGroupPower, "Unsupported format: %u", pImageFormat->format);
            result = CamxResultEInvalidArg;
        }
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupISP, "Invalid RDI Port Idx:%d PortId:%d", idx, portId);
        result = CamxResultEInvalidArg;
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::SetupResourceClockConfig
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFENode::SetupResourceClockConfig()
{
    CamxResult              result                  = CamxResultSuccess;
    SIZE_T                  resourceClockConfigSize = 0;
    UINT64                  maxCalculatedClock      = 0;
    IFEResourceClockConfig* pClockResourceConfig;

    resourceClockConfigSize      = sizeof(IFEResourceClockConfig) + (sizeof(UINT64) * (RDIMaxNum - 1));
    pClockResourceConfig         = static_cast<IFEResourceClockConfig*>(CAMX_CALLOC(resourceClockConfigSize));

    if (NULL != pClockResourceConfig)
    {
        if (IFEModuleMode::DualIFENormal == m_mode)
        {
            pClockResourceConfig->usageType = ISPResourceUsageDual;
        }
        else
        {
            pClockResourceConfig->usageType         = ISPResourceUsageSingle;
            pClockResourceConfig->rightPixelClockHz = 0;
        }
    }
    else
    {
        result = CamxResultENoMemory;
    }

    if (CamxResultSuccess == result)
    {
        m_sensorLineTimeMSecs[0] = 0;
        m_sensorLineTimeMSecs[1] = 0;

        result = CamxResultEDisabled;
        pClockResourceConfig->leftPixelClockHz = GetPixelClockRate(IFESplitID::Left);
        maxCalculatedClock                     = pClockResourceConfig->leftPixelClockHz;
        if (0 < pClockResourceConfig->leftPixelClockHz)
        {
            result = CamxResultSuccess;

            m_sensorLineTimeMSecs[0] = CalculateSensorLineDuration(pClockResourceConfig->leftPixelClockHz);
            CAMX_ASSERT(0 < m_sensorLineTimeMSecs[0]);

            // In FS mode increase Clock by 33% since for a 120FPS sensor mode writing @appr. 8.2ms/frame,
            // IFE only has a reduced time of ~24ms to process the frame
            /// @todo (CAMX-4450) Update Bandwidth and Clock voting for FastShutter Usecase
            if (TRUE == m_enableBusRead)
            {
                FLOAT clockFreq = m_sensorLineTimeMSecs[0];
                m_sensorLineTimeMSecs[0] *= (4.0/3.0);
                CAMX_LOG_INFO(CamxLogGroupISP, "FS: Increase Clock from %f to %f",
                    clockFreq, m_sensorLineTimeMSecs[0]);
            }

            if (IFEModuleMode::DualIFENormal == m_mode)
            {
                pClockResourceConfig->rightPixelClockHz = GetPixelClockRate(IFESplitID::Right);
                if (0 < pClockResourceConfig->rightPixelClockHz)
                {
                    m_sensorLineTimeMSecs[1] = CalculateSensorLineDuration(pClockResourceConfig->rightPixelClockHz);
                    CAMX_ASSERT(0 < m_sensorLineTimeMSecs[1]);
                }
                if (maxCalculatedClock < pClockResourceConfig->rightPixelClockHz)
                {
                    maxCalculatedClock = pClockResourceConfig->rightPixelClockHz;
                }
            }
        }
    }

    if (CamxResultSuccess == result)
    {
        pClockResourceConfig->rdiClockHz[0] = CalculateRDIClockRate();
        pClockResourceConfig->numRdi = RDIMaxNum;

        // In DUAL IFE DULA PD Case, both IFEs are consuming the PDDATA but voting for only IFE. This is a temp
        // workaround to solve the IFE violations due to less clock.
        if (IFEModuleMode::DualIFENormal == m_mode)
        {
            if (maxCalculatedClock < pClockResourceConfig->rdiClockHz[0])
            {
                maxCalculatedClock = pClockResourceConfig->rdiClockHz[0];
            }
            pClockResourceConfig->rightPixelClockHz = maxCalculatedClock;
            pClockResourceConfig->leftPixelClockHz  = maxCalculatedClock;
        }
        CAMX_LOG_INFO(CamxLogGroupPower, "IFE:%d leftIfePixClk=%llu Hz, rightIfePixClk=%llu Hz",
                      InstanceID(),
                      pClockResourceConfig->leftPixelClockHz, pClockResourceConfig->rightPixelClockHz);

        result = PacketBuilder::WriteGenericBlobData(m_pGenericBlobCmdBuffer,
                                                     IFEGenericBlobTypeResourceClockConfig,
                                                     static_cast<UINT32>(resourceClockConfigSize),
                                                     reinterpret_cast<BYTE*>(pClockResourceConfig));

        if (CamxResultSuccess != result)
        {
            CAMX_LOG_ERROR(CamxLogGroupPower, "Error: Writing Clock Blob data result=%d", result);
        }
    }
    else if (CamxResultEDisabled == result)
    {
        // Reset state since there is no real error
        result = CamxResultSuccess;
    }


    if (NULL != pClockResourceConfig)
    {
        CAMX_FREE(pClockResourceConfig);
        pClockResourceConfig = NULL;
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::SetupFlushHaltConfig
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFENode::SetupFlushHaltConfig(
    UINT32 value)
{
    CamxResult                               result       = CamxResultSuccess;
    IFE_IFE_0_VFE_ISP_MODULE_FLUSH_HALT_CFG  flushHaltCfg;

    /* Need to update based on calculation */
    value = 0b1010101010101010;
    flushHaltCfg.bitfields.FLUSH_HALT_GEN_EN  = 1;
    flushHaltCfg.bitfields.FLUSH_HALT_PATTERN = value;


    result = PacketBuilder::WriteRegRange(m_pCommonCmdBuffer,
                                          regIFE_IFE_0_VFE_ISP_MODULE_FLUSH_HALT_CFG,
                                          sizeof(flushHaltCfg) / RegisterWidthInBytes,
                                          reinterpret_cast<UINT32*>(&flushHaltCfg));

    return result;

}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::SetupResourceBWConfig
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFENode::SetupResourceBWConfig(
    ExecuteProcessRequestData* pExecuteProcessRequestData,
    UINT64                     requestId)
{
    CamxResult result = CamxResultEFailed;

    if (NULL == m_pBwResourceConfig)
    {
        m_resourceBwConfigSize  = sizeof(IFEResourceBWConfig) + (sizeof(IFEResourceBWVote) * (RDIMaxNum - 1));
        m_pBwResourceConfig     = static_cast<IFEResourceBWConfig*>(CAMX_CALLOC(m_resourceBwConfigSize));
    }

    if (NULL != m_pBwResourceConfig)
    {
        Utils::Memset(m_pBwResourceConfig, 0, m_resourceBwConfigSize);

        m_pBwResourceConfig->usageType = (IFEModuleMode::DualIFENormal == m_mode)? ISPResourceUsageDual: ISPResourceUsageSingle;
        m_pBwResourceConfig->numRdi    = RDIMaxNum;

        if (TRUE == m_computeActiveABVotes)
        {
            // Compute AB and IB votes explicitly i.e AB != IB
            if (NULL == m_pBwResourceConfigAB)
            {
                m_resourceBWConfigSizeAB = sizeof(IFEResourceBWConfigAB) + (sizeof(UINT64) * (RDIMaxNum - 1));
                m_pBwResourceConfigAB    = static_cast<IFEResourceBWConfigAB*>(CAMX_CALLOC(m_resourceBWConfigSizeAB));
            }

            if (NULL != m_pBwResourceConfigAB)
            {
                Utils::Memset(m_pBwResourceConfigAB, 0, m_resourceBWConfigSizeAB);

                m_pBwResourceConfigAB->usageType = (IFEModuleMode::DualIFENormal == m_mode)?
                                                        ISPResourceUsageDual: ISPResourceUsageSingle;
                m_pBwResourceConfigAB->numRdi    = RDIMaxNum;

                // AB and IB values are different
                result = CalculateActiveBandwidth(pExecuteProcessRequestData,
                                                  m_pBwResourceConfig,
                                                  m_pBwResourceConfigAB,
                                                  requestId);
            }
        }
        else
        {
            // AB = IB
            result = CalculateBandwidth(pExecuteProcessRequestData, m_pBwResourceConfig, requestId);
        }
    }

    if (CamxResultSuccess == result)
    {
        CAMX_LOG_VERBOSE(CamxLogGroupPower,
                         "IFE:%d "
                         "Left  camnocBWbytes=%llu externalBWbytes=%llu "
                         "Right camnocBWbytes=%llu externalBWbytes=%llu "
                         "RDI_0 camnocBWbytes=%llu externalBWbytes=%llu "
                         "RDI_1 camnocBWbytes=%llu externalBWbytes=%llu "
                         "RDI_2 camnocBWbytes=%llu externalBWbytes=%llu "
                         "RDI_3 camnocBWbytes=%llu externalBWbytes=%llu ",
                         InstanceID(),
                         m_pBwResourceConfig->leftPixelVote.camnocBWbytes,
                         m_pBwResourceConfig->leftPixelVote.externalBWbytes,
                         m_pBwResourceConfig->rightPixelVote.camnocBWbytes,
                         m_pBwResourceConfig->rightPixelVote.externalBWbytes,
                         m_pBwResourceConfig->rdiVote[0].camnocBWbytes,
                         m_pBwResourceConfig->rdiVote[0].externalBWbytes,
                         m_pBwResourceConfig->rdiVote[1].camnocBWbytes,
                         m_pBwResourceConfig->rdiVote[1].externalBWbytes,
                         m_pBwResourceConfig->rdiVote[2].camnocBWbytes,
                         m_pBwResourceConfig->rdiVote[2].externalBWbytes,
                         m_pBwResourceConfig->rdiVote[3].camnocBWbytes,
                         m_pBwResourceConfig->rdiVote[3].externalBWbytes);

        result = PacketBuilder::WriteGenericBlobData(m_pGenericBlobCmdBuffer,
                                                     IFEGenericBlobTypeResourceBWConfig,
                                                     static_cast<UINT32>(m_resourceBwConfigSize),
                                                     reinterpret_cast<BYTE*>(m_pBwResourceConfig));

        if (CamxResultSuccess != result)
        {
            CAMX_LOG_ERROR(CamxLogGroupPower, "Error: Writing BW Blob data result=%d", result);
        }

        if ((TRUE == m_computeActiveABVotes) && (CamxResultSuccess == result))
        {
            // The AB bandwidth values need to be provided separately to KMD
            CAMX_LOG_VERBOSE(CamxLogGroupPower,
                             "IFE:%d AB Votes "
                             "Left  externalBWbytesAB=%llu "
                             "Right externalBWbytesAB=%llu "
                             "RDI_0 externalBWbytesAB=%llu "
                             "RDI_1 externalBWbytesAB=%llu "
                             "RDI_2 externalBWbytesAB=%llu "
                             "RDI_3 externalBWbytesAB=%llu ",
                             InstanceID(),
                             m_pBwResourceConfigAB->leftPixelVoteAB,
                             m_pBwResourceConfigAB->rightPixelVoteAB,
                             m_pBwResourceConfigAB->rdiVoteAB[0],
                             m_pBwResourceConfigAB->rdiVoteAB[1],
                             m_pBwResourceConfigAB->rdiVoteAB[2],
                             m_pBwResourceConfigAB->rdiVoteAB[3]);

            result = PacketBuilder::WriteGenericBlobData(m_pGenericBlobCmdBuffer,
                                                         IFEGenericBlobTypeResourceBWConfigAB,
                                                         static_cast<UINT32>(m_resourceBWConfigSizeAB),
                                                         reinterpret_cast<BYTE*>(m_pBwResourceConfigAB));
            if (CamxResultSuccess != result)
            {
                CAMX_LOG_ERROR(CamxLogGroupPower, "Error: Writing AB BW Blob data result=%d", result);
            }
        }
    }
    else if (CamxResultEDisabled == result)
    {
        // Reset state since there is no real error
        result = CamxResultSuccess;
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::CanSkipAlgoProcessing
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL IFENode::CanSkipAlgoProcessing(
    ISPInputData* pInputData
    ) const
{
    UINT skipFactor   = m_instanceProperty.IFEStatsSkipPattern;
    BOOL skipRequired = FALSE;

    if (0 != skipFactor)
    {
        UINT64 requestIdOffsetFromLastFlush = GetRequestIdOffsetFromLastFlush(pInputData->frameNum);

        skipFactor = (requestIdOffsetFromLastFlush <= FirstValidRequestId + GetMaximumPipelineDelay()) ? 1
                                                                                                                   : skipFactor;
        skipRequired = ((requestIdOffsetFromLastFlush % skipFactor) == 0) ? FALSE : TRUE;
    }

    // This check is required to identify if this is flash snapshot.
    // For flash snapshot we should not skip tintless/LSC programming
    if ((0 < pInputData->numberOfLED) && (1.0f < pInputData->pAECUpdateData->LEDInfluenceRatio))
    {
        skipRequired = FALSE;
    }

    CAMX_LOG_INFO(CamxLogGroupISP, "skipRequired %u requestId %llu skipFactor %u",
                  skipRequired, pInputData->frameNum, skipFactor);

    return skipRequired;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IFENode::QueryMetadataPublishList
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFENode::QueryMetadataPublishList(
    NodeMetadataList* pPublistTagList)
{
    CamxResult result             = CamxResultSuccess;
    UINT32     tagCount           = 0;
    UINT32     numMetadataTags    = CAMX_ARRAY_SIZE(IFEMetadataOutputTags);
    UINT32     numRawMetadataTags = CAMX_ARRAY_SIZE(IFEMetadataRawOutputTags);
    UINT32     numVendorTags      = CAMX_ARRAY_SIZE(IFEOutputVendorTags);
    UINT32     tagID;

    if (numMetadataTags + numRawMetadataTags + numVendorTags < MaxTagsPublished)
    {
        for (UINT32 tagIndex = 0; tagIndex < numMetadataTags; ++tagIndex)
        {
            pPublistTagList->tagArray[tagCount++] = IFEMetadataOutputTags[tagIndex];
        }

        for (UINT32 tagIndex = 0; tagIndex < numRawMetadataTags; ++tagIndex)
        {
            pPublistTagList->tagArray[tagCount++] = IFEMetadataRawOutputTags[tagIndex];
        }

        for (UINT32 tagIndex = 0; tagIndex < numVendorTags; ++tagIndex)
        {
            result = VendorTagManager::QueryVendorTagLocation(
                IFEOutputVendorTags[tagIndex].pSectionName,
                IFEOutputVendorTags[tagIndex].pTagName,
                &tagID);

            if (CamxResultSuccess != result)
            {
                CAMX_LOG_ERROR(CamxLogGroupMeta, "Cannot query vendor tag %s %s",
                    IFEOutputVendorTags[tagIndex].pSectionName,
                    IFEOutputVendorTags[tagIndex].pTagName);
                break;
            }
            pPublistTagList->tagArray[tagCount++] = tagID;

            m_vendorTagArray[tagIndex] = tagID;
        }
    }
    else
    {
        result = CamxResultEOutOfBounds;
        CAMX_LOG_ERROR(CamxLogGroupMeta, "ERROR More space needed to add publish tags (%d %d %d)",
            numMetadataTags, numRawMetadataTags, numVendorTags);
    }

    if (CamxResultSuccess == result)
    {
        pPublistTagList->tagCount = tagCount;
        CAMX_LOG_VERBOSE(CamxLogGroupMeta, "%d tags will be published", tagCount);
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::IsFSModeEnabled
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFENode::IsFSModeEnabled(
    BOOL* pIsFSModeEnabled)
{
    CamxResult  result            = CamxResultSuccess;
    UINT32      metaTag           = 0;
    UINT8       isFSModeEnabled   = FALSE;

    result = VendorTagManager::QueryVendorTagLocation(
        "org.quic.camera.SensorModeFS", "SensorModeFS", &metaTag);

    if (CamxResultSuccess == result)
    {
        UINT   props[]                         = { metaTag | UsecaseMetadataSectionMask };
        VOID*  pData[CAMX_ARRAY_SIZE(props)]   = { 0 };
        UINT64 offsets[CAMX_ARRAY_SIZE(props)] = { 0 };
        GetDataList(props, pData, offsets, CAMX_ARRAY_SIZE(props));
        if (NULL != pData[0])
        {
            isFSModeEnabled = *reinterpret_cast<UINT8*>(pData[0]);
        }
        else
        {
            isFSModeEnabled = FALSE;
        }
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupCore, "Failed to read SensorModeFS tag %d", result);
        isFSModeEnabled = FALSE;
    }

    *pIsFSModeEnabled = isFSModeEnabled;
    CAMX_LOG_VERBOSE(CamxLogGroupCore, "Fast shutter mode enabled - %d",
        *pIsFSModeEnabled);

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::IsFSSnapshot
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL IFENode::IsFSSnapshot(
    ExecuteProcessRequestData* pExecuteProcessRequestData,
    UINT64                     requestID)
{
    // FS snapshot is similar to an RDI only usecase with RDI1 being used for snapshot
    BOOL isFSSnapshot                       = FALSE;
    PerRequestActivePorts* pPerRequestPorts = pExecuteProcessRequestData->pEnabledPortsInfo;

    if ((TRUE == m_enableBusRead) && (NULL != pPerRequestPorts))
    {
        PerRequestOutputPortInfo* pOutputPort = NULL;
        // Loop through all the ports to check if RDI1 i.e. snapshot request is made
        for (UINT i = 0; i < pPerRequestPorts->numOutputPorts; i++)
        {
            pOutputPort = &pPerRequestPorts->pOutputPorts[i];
            if ((NULL != pOutputPort) && (IFEOutputPortRDI1 == pOutputPort->portId))
            {
                isFSSnapshot = TRUE;
                CAMX_LOG_VERBOSE(CamxLogGroupISP, "FS: IFE:%d Snapshot Requested for ReqID:%llu",
                                 InstanceID(), requestID);
                break;
            }
        }
    }

    return isFSSnapshot;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::GetMetadataContrastLevel
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IFENode::GetMetadataContrastLevel(
    ISPHALTagsData* pHALTagsData)
{
    UINT32     metaTagContrast = 0;
    CamxResult resultContrast  = VendorTagManager::QueryVendorTagLocation("org.codeaurora.qcamera3.contrast",
                                                                            "level",
                                                                            &metaTagContrast);
    if (CamxResultSuccess == resultContrast)
    {
        static const UINT VendorTagContrast[] =
        {
            metaTagContrast | InputMetadataSectionMask,
        };

        const static UINT lengthContrast                    = CAMX_ARRAY_SIZE(VendorTagContrast);
        VOID* pDataContrast[lengthContrast]                 = { 0 };
        UINT64 vendorTagsContrastIFEOffset[lengthContrast]  = { 0 };

        GetDataList(VendorTagContrast, pDataContrast, vendorTagsContrastIFEOffset, lengthContrast);
        if (NULL != pDataContrast[0])
        {
            UINT8 appLevel = *(static_cast<UINT8*>(pDataContrast[0]));
            if (appLevel > 0)
            {
                pHALTagsData->contrastLevel = appLevel - 1;
            }
            else
            {
                pHALTagsData->contrastLevel = 5;
            }
            CAMX_LOG_VERBOSE(CamxLogGroupPProc, "Manual Contrast Level = %d", pHALTagsData->contrastLevel);
        }
        else
        {
            CAMX_LOG_ERROR(CamxLogGroupPProc, "Cannot obtain Contrast Level. Set default to 5");
            pHALTagsData->contrastLevel = 5;
        }
    }
    else
    {
        CAMX_LOG_WARN(CamxLogGroupPProc, "No Contrast Level available. Set default to 5");
        pHALTagsData->contrastLevel = 5; // normal without contrast change
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFENode::GetMetadataTonemapCurve
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IFENode::GetMetadataTonemapCurve(
    ISPHALTagsData* pHALTagsData)
{
    // Deep copy tone map curves, only when the tone map is contrast curve
    if (TonemapModeContrastCurve == pHALTagsData->tonemapCurves.tonemapMode)
    {
        ISPTonemapPoint* pBlueTonemapCurve  = NULL;
        ISPTonemapPoint* pGreenTonemapCurve = NULL;
        ISPTonemapPoint* pRedTonemapCurve   = NULL;

        static const UINT VendorTagsIFE[] =
        {
            InputTonemapCurveBlue,
            InputTonemapCurveGreen,
            InputTonemapCurveRed,
        };

        const static UINT length                    = CAMX_ARRAY_SIZE(VendorTagsIFE);
        VOID* pData[length]                         = { 0 };
        UINT64 vendorTagsTonemapIFEOffset[length]   = { 0 };

        GetDataList(VendorTagsIFE, pData, vendorTagsTonemapIFEOffset, length);

        if (NULL != pData[0])
        {
            pBlueTonemapCurve = static_cast<ISPTonemapPoint*>(pData[0]);
        }

        if (NULL != pData[1])
        {
            pGreenTonemapCurve = static_cast<ISPTonemapPoint*>(pData[1]);
        }

        if (NULL != pData[2])
        {
            pRedTonemapCurve = static_cast<ISPTonemapPoint*>(pData[2]);
        }

        CAMX_ASSERT(NULL != pBlueTonemapCurve);
        CAMX_ASSERT(NULL != pGreenTonemapCurve);
        CAMX_ASSERT(NULL != pRedTonemapCurve);

        pHALTagsData->tonemapCurves.curvePoints = static_cast<INT32>(
            GetDataCountFromPipeline(InputTonemapCurveBlue, 0, GetPipeline()->GetPipelineId(), TRUE));

        if ((pHALTagsData->tonemapCurves.curvePoints > 0) && (NULL != pBlueTonemapCurve))
        {
            // Blue tone map curve
            Utils::Memcpy(pHALTagsData->tonemapCurves.tonemapCurveBlue,
                          pBlueTonemapCurve,
                          (sizeof(ISPTonemapPoint) * pHALTagsData->tonemapCurves.curvePoints));
        }

        pHALTagsData->tonemapCurves.curvePoints = static_cast<INT32>(
            GetDataCountFromPipeline(InputTonemapCurveGreen, 0, GetPipeline()->GetPipelineId(), TRUE));

        if ((pHALTagsData->tonemapCurves.curvePoints > 0) && (NULL != pGreenTonemapCurve))
        {
            // Green tone map curve
            Utils::Memcpy(pHALTagsData->tonemapCurves.tonemapCurveGreen,
                          pGreenTonemapCurve,
                          (sizeof(ISPTonemapPoint) * pHALTagsData->tonemapCurves.curvePoints));
        }

        pHALTagsData->tonemapCurves.curvePoints = static_cast<INT32>(
            GetDataCountFromPipeline(InputTonemapCurveRed, 0, GetPipeline()->GetPipelineId(), TRUE));

        if ((pHALTagsData->tonemapCurves.curvePoints > 0) && (NULL != pRedTonemapCurve))
        {
            // Red tone map curve
            Utils::Memcpy(pHALTagsData->tonemapCurves.tonemapCurveRed,
                          pRedTonemapCurve,
                          (sizeof(ISPTonemapPoint) * pHALTagsData->tonemapCurves.curvePoints));
        }
    }
}

CAMX_NAMESPACE_END
