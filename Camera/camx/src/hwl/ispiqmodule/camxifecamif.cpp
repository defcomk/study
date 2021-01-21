////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017-2018 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxifecamif.cpp
/// @brief CAMXIFECAMIF class implementation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "camxdefs.h"
#include "camxifecamif.h"
#include "camxiqinterface.h"
#include "camxispiqmodule.h"
#include "camxtuningdatamanager.h"

CAMX_NAMESPACE_BEGIN

static const UINT32 IFECAMIFRegLengthDWord1 = sizeof(IFECAMIFRegCmd1) / sizeof(UINT32);
CAMX_STATIC_ASSERT((2 * 4) == sizeof(IFECAMIFRegCmd1));

static const UINT32 IFECAMIFRegLengthDWord2 = sizeof(IFECAMIFRegCmd2) / sizeof(UINT32);
CAMX_STATIC_ASSERT((3 * 4) == sizeof(IFECAMIFRegCmd2));

static const UINT32 IFECAMIFRegLengthDWord3 = sizeof(IFECAMIFRegCmd3) / sizeof(UINT32);
CAMX_STATIC_ASSERT((1 * 4) == sizeof(IFECAMIFRegCmd3));

static const UINT32 IFECAMIFRegLengthDWord4 = sizeof(IFECAMIFRegCmd4) / sizeof(UINT32);
CAMX_STATIC_ASSERT((2 * 4) == sizeof(IFECAMIFRegCmd4));

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFECAMIF::Create
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFECAMIF::Create(
    IFEModuleCreateData* pCreateData)
{
    CamxResult result = CamxResultSuccess;

    if (NULL != pCreateData)
    {
        pCreateData->pModule = CAMX_NEW IFECAMIF;

        if (NULL == pCreateData->pModule)
        {
            result = CamxResultEFailed;
            CAMX_ASSERT_ALWAYS_MESSAGE("Failed to create IFECAMIF object.");
        }
    }
    else
    {
        result = CamxResultEInvalidArg;
        CAMX_ASSERT_ALWAYS_MESSAGE("Null Input Data for IFECAMIF Creation");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFECAMIF::Execute
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFECAMIF::Execute(
    ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;

    CAMX_ASSERT(NULL != pInputData);

    if (TRUE == CheckDependenceChange(pInputData))
    {
        result = RunCalculation(pInputData);

        if (CamxResultSuccess == result)
        {
            result = CreateCmdList(pInputData);
        }
        else
        {
            CAMX_ASSERT_ALWAYS_MESSAGE("CAMIF module calculation Failed.");
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFECAMIF::GetRegCmd
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID* IFECAMIF::GetRegCmd()
{
    return reinterpret_cast<VOID*>(&m_regCmd1);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFECAMIF::CheckDependenceChange
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL IFECAMIF::CheckDependenceChange(
    ISPInputData* pInputData)
{
    BOOL  isChanged = FALSE;

    if (NULL != pInputData)
    {
        if (TRUE == pInputData->pipelineIFEData.programCAMIF)
        {
            isChanged = TRUE;
        }
    }

    return isChanged;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFECAMIF::CreateCmdList
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFECAMIF::CreateCmdList(
    ISPInputData* pInputData)
{
    CamxResult result     = CamxResultSuccess;
    CmdBuffer* pCmdBuffer = NULL;

    if (NULL != pInputData->pCmdBuffer)
    {
        pCmdBuffer = pInputData->pCmdBuffer;
        result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                              regIFE_IFE_0_VFE_CAMIF_CMD,
                                              IFECAMIFRegLengthDWord1,
                                              reinterpret_cast<UINT32*>(&m_regCmd1));

        if (CamxResultSuccess == result)
        {
            result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                                  regIFE_IFE_0_VFE_CAMIF_LINE_SKIP_PATTERN,
                                                  IFECAMIFRegLengthDWord2,
                                                  reinterpret_cast<UINT32*>(&m_regCmd2));
        }

        if (CamxResultSuccess == result)
        {
            result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                                  regIFE_IFE_0_VFE_CAMIF_IRQ_SUBSAMPLE_PATTERN,
                                                  IFECAMIFRegLengthDWord3,
                                                  reinterpret_cast<UINT32*>(&m_regCmd3));
        }

        if (CamxResultSuccess == result)
        {
            result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                                  regIFE_IFE_0_VFE_CAMIF_RAW_CROP_WIDTH_CFG,
                                                  IFECAMIFRegLengthDWord4,
                                                  reinterpret_cast<UINT32*>(&m_regCmd4));
        }

        if (CamxResultSuccess != result)
        {
            CAMX_ASSERT_ALWAYS_MESSAGE("Failed to write command buffer");
        }
    }
    else
    {
        result = CamxResultEInvalidArg;
        CAMX_ASSERT_ALWAYS_MESSAGE("Invalid Input data or command buffer");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFECAMIF::RunCalculation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFECAMIF::RunCalculation(
    ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;

    /// @todo (CAMX-677) Set up the regiser value based on the input data
    m_regCmd1.statusRegister.u32All                       = 0x0;
    m_regCmd1.statusRegister.bitfields.CLEAR_CAMIF_STATUS = 0x1;

    m_regCmd1.configRegister.u32All                       = 0x0;
    m_regCmd1.configRegister.bitfields.VFE_OUTPUT_EN      = 0x1;
    m_regCmd1.configRegister.bitfields.BUS_OUTPUT_EN      = 0x1;
    m_regCmd1.configRegister.bitfields.RAW_CROP_EN        = 0x1;
    m_regCmd1.configRegister.bitfields.BUS_SUBSAMPLE_EN   =
        pInputData->pStripeConfig->CAMIFSubsampleInfo.enableCAMIFSubsample;

    CAMX_LOG_VERBOSE(CamxLogGroupISP, "PDAF pixelSkip 0x%X line SKip 0x%X",
        pInputData->pStripeConfig->CAMIFSubsampleInfo.CAMIFSubSamplePattern.pixelSkipPattern,
        pInputData->pStripeConfig->CAMIFSubsampleInfo.CAMIFSubSamplePattern.lineSkipPattern);
    m_regCmd2.lineSkipPatternRegister.u32All =
        pInputData->pStripeConfig->CAMIFSubsampleInfo.CAMIFSubSamplePattern.lineSkipPattern;
    m_regCmd2.pixelSkipPatternRegister.u32All =
        pInputData->pStripeConfig->CAMIFSubsampleInfo.CAMIFSubSamplePattern.pixelSkipPattern;

    CAMX_ASSERT(0 != pInputData->pipelineIFEData.numBatchedFrames);

    m_regCmd2.skipPeriodRegister.u32All                   = 0x0;
    m_regCmd3.irqSubsamplePattern.u32All                  = 1;

    m_regCmd2.skipPeriodRegister.bitfields.PIXEL_SKIP_PERIOD = 0xF;
    m_regCmd2.skipPeriodRegister.bitfields.LINE_SKIP_PERIOD  = 0xF;

    if (1 < pInputData->pipelineIFEData.numBatchedFrames)
    {
        m_regCmd2.skipPeriodRegister.bitfields.IRQ_SUBSAMPLE_PERIOD = pInputData->pipelineIFEData.numBatchedFrames - 1;
    }

    if (TRUE == pInputData->HVXData.DSEnabled)
    {
        m_regCmd4.rawCropWidthConfig.bitfields.LAST_PIXEL = pInputData->HVXData.origCAMIFCrop.lastPixel;
        m_regCmd4.rawCropWidthConfig.bitfields.FIRST_PIXEL = pInputData->HVXData.origCAMIFCrop.firstPixel;

        m_regCmd4.rawCropHeightConfig.bitfields.LAST_LINE = pInputData->HVXData.origCAMIFCrop.lastLine;
        m_regCmd4.rawCropHeightConfig.bitfields.FIRST_LINE = pInputData->HVXData.origCAMIFCrop.firstLine;
    }
    else
    {
        m_regCmd4.rawCropWidthConfig.bitfields.LAST_PIXEL  =
            pInputData->pStripeConfig->CAMIFSubsampleInfo.PDAFCAMIFCrop.lastPixel;
        m_regCmd4.rawCropWidthConfig.bitfields.FIRST_PIXEL =
            pInputData->pStripeConfig->CAMIFSubsampleInfo.PDAFCAMIFCrop.firstPixel;
        m_regCmd4.rawCropHeightConfig.bitfields.LAST_LINE  =
            pInputData->pStripeConfig->CAMIFSubsampleInfo.PDAFCAMIFCrop.lastLine;
        m_regCmd4.rawCropHeightConfig.bitfields.FIRST_LINE =
            pInputData->pStripeConfig->CAMIFSubsampleInfo.PDAFCAMIFCrop.firstLine;
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFECAMIF::IFECAMIF
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IFECAMIF::IFECAMIF()
{
    m_type           = ISPIQModuleType::IFECAMIF;
    m_cmdLength      = PacketBuilder::RequiredWriteRegRangeSizeInDwords(IFECAMIFRegLengthDWord1) +
                       PacketBuilder::RequiredWriteRegRangeSizeInDwords(IFECAMIFRegLengthDWord2) +
                       PacketBuilder::RequiredWriteRegRangeSizeInDwords(IFECAMIFRegLengthDWord3) +
                       PacketBuilder::RequiredWriteRegRangeSizeInDwords(IFECAMIFRegLengthDWord4);
    m_32bitDMILength = 0;
    m_64bitDMILength = 0;

    /// @todo (CAMX-677) This hardcode value is to support initial presil test. Will replace with memset to 0
    m_regCmd1.statusRegister.u32All           = 0x0;
    m_regCmd1.configRegister.u32All           = 0x0;
    m_regCmd2.lineSkipPatternRegister.u32All  = 0x0;
    m_regCmd2.pixelSkipPatternRegister.u32All = 0x0;
    m_regCmd2.skipPeriodRegister.u32All       = 0x0;
    m_regCmd3.irqSubsamplePattern.u32All      = 0x0;
    m_regCmd4.rawCropWidthConfig.u32All       = 0x0;
    m_regCmd4.rawCropHeightConfig.u32All      = 0x0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IFECAMIF::~IFECAMIF
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IFECAMIF::~IFECAMIF()
{
}

CAMX_NAMESPACE_END
