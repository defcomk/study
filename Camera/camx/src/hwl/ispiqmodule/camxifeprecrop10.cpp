////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017-2018 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxifeprecrop10.cpp
/// @brief CAMXIFEPRECROP10 class implementation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "camxifeprecrop10.h"

CAMX_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEPreCrop10::Create
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFEPreCrop10::Create(
    IFEModuleCreateData* pCreateData)
{
    CamxResult result = CamxResultSuccess;

    if (NULL != pCreateData)
    {
        pCreateData->pModule = CAMX_NEW IFEPreCrop10(pCreateData);
        if (NULL == pCreateData->pModule)
        {
            result = CamxResultENoMemory;
            CAMX_ASSERT_ALWAYS_MESSAGE("Memory allocation failed");
        }
    }
    else
    {
        result = CamxResultEInvalidArg;
        CAMX_LOG_ERROR(CamxLogGroupISP, "Input Null pointer");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEPreCrop10::GetRegCmd
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID* IFEPreCrop10::GetRegCmd()
{
    return &m_regCmd;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEPreCrop10::CreateCmdList
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFEPreCrop10::CreateCmdList(
    ISPInputData* pInputData)
{
    CamxResult result           = CamxResultSuccess;
    UINT32     register1        = 0;
    UINT32     numberOfValues1  = 0;
    UINT32*    pValues1         = NULL;
    UINT32     register2        = 0;
    UINT32     numberOfValues2  = 0;
    UINT32*    pValues2         = NULL;

    if (NULL != pInputData->pCmdBuffer)
    {
        CmdBuffer* pCmdBuffer = pInputData->pCmdBuffer;

        switch (m_modulePath)
        {
            case IFEPipelinePath::DS4Path:
                register1   = regIFE_IFE_0_VFE_DS4_Y_PRE_CROP_LINE_CFG;
                register2   = regIFE_IFE_0_VFE_DS4_C_PRE_CROP_LINE_CFG;

                numberOfValues1 = sizeof(IFEPreCrop10DS4LumaReg) / RegisterWidthInBytes;
                pValues1        = reinterpret_cast<UINT32*>(&m_regCmd.DS4Luma);
                numberOfValues2 = sizeof(IFEPreCrop10DS4ChromaReg) / RegisterWidthInBytes;
                pValues2        = reinterpret_cast<UINT32*>(&m_regCmd.DS4Chroma);
                break;

            case IFEPipelinePath::DS16Path:
                register1   = regIFE_IFE_0_VFE_DS16_Y_PRE_CROP_LINE_CFG;
                register2   = regIFE_IFE_0_VFE_DS16_C_PRE_CROP_LINE_CFG;

                // Note, the register types for both DS4/DS16 are identical.
                numberOfValues1 = sizeof(IFEPreCrop10DS4LumaReg) / RegisterWidthInBytes;
                pValues1        = reinterpret_cast<UINT32*>(&m_regCmd.DS16Luma);
                numberOfValues2 = sizeof(IFEPreCrop10DS4ChromaReg) / RegisterWidthInBytes;
                pValues2        = reinterpret_cast<UINT32*>(&m_regCmd.DS16Chroma);
                break;

            case IFEPipelinePath::DisplayDS4Path:
                register1   = regIFE_IFE_0_VFE_DISP_DS4_Y_PRE_CROP_LINE_CFG;
                register2   = regIFE_IFE_0_VFE_DISP_DS4_C_PRE_CROP_LINE_CFG;

                numberOfValues1 = sizeof(IFEPreCrop10DisplayDS4LumaReg) / RegisterWidthInBytes;
                pValues1        = reinterpret_cast<UINT32*>(&m_regCmd.displayDS4Luma);
                numberOfValues2 = sizeof(IFEPreCrop10DisplayDS4ChromaReg) / RegisterWidthInBytes;
                pValues2        = reinterpret_cast<UINT32*>(&m_regCmd.displayDS4Chroma);
                break;

            case IFEPipelinePath::DisplayDS16Path:
                register1   = regIFE_IFE_0_VFE_DISP_DS16_Y_PRE_CROP_LINE_CFG;
                register2   = regIFE_IFE_0_VFE_DISP_DS16_C_PRE_CROP_LINE_CFG;

                // Note, the register types for both DS4/DS16 are identical.
                numberOfValues1 = sizeof(IFEPreCrop10DisplayDS4LumaReg) / RegisterWidthInBytes;
                pValues1        = reinterpret_cast<UINT32*>(&m_regCmd.displayDS16Luma);
                numberOfValues2 = sizeof(IFEPreCrop10DisplayDS4ChromaReg) / RegisterWidthInBytes;
                pValues2        = reinterpret_cast<UINT32*>(&m_regCmd.displayDS16Chroma);
                break;

            default:
                CAMX_LOG_ERROR(CamxLogGroupISP, "Invalid module path for pre-crop module");
                result = CamxResultEInvalidState;
        }

        if (CamxResultSuccess == result)
        {
            result = PacketBuilder::WriteRegRange(pCmdBuffer, register1, numberOfValues1, pValues1);

            if (CamxResultSuccess == result)
            {
                result = PacketBuilder::WriteRegRange(pCmdBuffer, register2, numberOfValues2, pValues2);
            }

            if (CamxResultSuccess != result)
            {
                CAMX_ASSERT_ALWAYS_MESSAGE("Failed to configure PreCrop Register");
            }
        }
    }
    else
    {
        result = CamxResultEInvalidPointer;
        CAMX_ASSERT_ALWAYS_MESSAGE("Invalid Input pointer");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEPreCrop10::ValidateDependenceParams
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFEPreCrop10::ValidateDependenceParams(
    ISPInputData* pInputData)
{
    CamxResult       result                  = CamxResultSuccess;

    if (((IFEPipelinePath::DS4Path == m_modulePath)                         &&
         ((0 == pInputData->pCalculatedData->preCropInfo.YCrop.lastPixel)   ||
          (0 == pInputData->pCalculatedData->preCropInfo.YCrop.lastLine)    ||
          (0 == pInputData->pCalculatedData->preCropInfo.CrCrop.lastPixel)  ||
          (0 == pInputData->pCalculatedData->preCropInfo.CrCrop.lastLine))) ||
        ((IFEPipelinePath::DS16Path == m_modulePath)                        &&
         ((0 == pInputData->pCalculatedData->preCropInfoDS16.YCrop.lastPixel)   ||
          (0 == pInputData->pCalculatedData->preCropInfoDS16.YCrop.lastLine)    ||
          (0 == pInputData->pCalculatedData->preCropInfoDS16.CrCrop.lastPixel)  ||
          (0 == pInputData->pCalculatedData->preCropInfoDS16.CrCrop.lastLine))))
    {
        result = CamxResultEInvalidArg;
        CAMX_LOG_ERROR(CamxLogGroupISP, "Invalid PreCrop Input for DS4/DS16 path");
    }

    if (((IFEPipelinePath::DisplayDS4Path == m_modulePath)                    &&
        ((0 == pInputData->pCalculatedData->dispPreCropInfo.YCrop.lastPixel)   ||
         (0 == pInputData->pCalculatedData->dispPreCropInfo.YCrop.lastLine)    ||
         (0 == pInputData->pCalculatedData->dispPreCropInfo.CrCrop.lastPixel)  ||
         (0 == pInputData->pCalculatedData->dispPreCropInfo.CrCrop.lastLine))) ||
        ((IFEPipelinePath::DisplayDS16Path == m_modulePath)                        &&
         ((0 == pInputData->pCalculatedData->dispPreCropInfoDS16.YCrop.lastPixel)   ||
          (0 == pInputData->pCalculatedData->dispPreCropInfoDS16.YCrop.lastLine)    ||
          (0 == pInputData->pCalculatedData->dispPreCropInfoDS16.CrCrop.lastPixel)  ||
          (0 == pInputData->pCalculatedData->dispPreCropInfoDS16.CrCrop.lastLine))))
    {
        result = CamxResultEInvalidArg;
        CAMX_LOG_ERROR(CamxLogGroupISP, "Invalid PreCrop Input for Display DS4/DS16 path");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEPreCrop10::DumpRegConfig
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IFEPreCrop10::DumpRegConfig()
{
    if (IFEPipelinePath::DS4Path == m_modulePath)
    {
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "DS4 Luma pre-Crop Config[%d, %d, %d, %d]",
                          m_regCmd.DS4Luma.pixelConfig.bitfields.FIRST_PIXEL,
                          m_regCmd.DS4Luma.lineConfig.bitfields.FIRST_LINE,
                          m_regCmd.DS4Luma.pixelConfig.bitfields.LAST_PIXEL,
                          m_regCmd.DS4Luma.lineConfig.bitfields.LAST_LINE);

        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "DS4 Chroma pre-Crop Config[%d, %d, %d, %d]",
                         m_regCmd.DS4Chroma.pixelConfig.bitfields.FIRST_PIXEL,
                         m_regCmd.DS4Chroma.lineConfig.bitfields.FIRST_LINE,
                         m_regCmd.DS4Chroma.pixelConfig.bitfields.LAST_PIXEL,
                         m_regCmd.DS4Chroma.lineConfig.bitfields.LAST_LINE);
    }
    else if (IFEPipelinePath::DS16Path == m_modulePath)
    {
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "DS16 Luma pre-Crop Config[%d, %d, %d, %d]",
                         m_regCmd.DS16Luma.pixelConfig.bitfields.FIRST_PIXEL,
                         m_regCmd.DS16Luma.lineConfig.bitfields.FIRST_LINE,
                         m_regCmd.DS16Luma.pixelConfig.bitfields.LAST_PIXEL,
                         m_regCmd.DS16Luma.lineConfig.bitfields.LAST_LINE);

        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "DS16 Chroma pre-Crop Config[%d, %d, %d, %d]",
                         m_regCmd.DS16Chroma.pixelConfig.bitfields.FIRST_PIXEL,
                         m_regCmd.DS16Chroma.lineConfig.bitfields.FIRST_LINE,
                         m_regCmd.DS16Chroma.pixelConfig.bitfields.LAST_PIXEL,
                         m_regCmd.DS16Chroma.lineConfig.bitfields.LAST_LINE);
    }
    else if (IFEPipelinePath::DisplayDS4Path == m_modulePath)
    {
        CAMX_LOG_VERBOSE(CamxLogGroupISP, "DS4 Display Luma pre-Crop Config[%d, %d, %d, %d]",
            m_regCmd.displayDS4Luma.pixelConfig.bitfields.FIRST_PIXEL,
            m_regCmd.displayDS4Luma.lineConfig.bitfields.FIRST_LINE,
            m_regCmd.displayDS4Luma.pixelConfig.bitfields.LAST_PIXEL,
            m_regCmd.displayDS4Luma.lineConfig.bitfields.LAST_LINE);

        CAMX_LOG_VERBOSE(CamxLogGroupISP, "DS4 Display Chroma pre-Crop Config[%d, %d, %d, %d]",
            m_regCmd.displayDS4Chroma.pixelConfig.bitfields.FIRST_PIXEL,
            m_regCmd.displayDS4Chroma.lineConfig.bitfields.FIRST_LINE,
            m_regCmd.displayDS4Chroma.pixelConfig.bitfields.LAST_PIXEL,
            m_regCmd.displayDS4Chroma.lineConfig.bitfields.LAST_LINE);
    }
    else
    {
        // IFEPipelinePath::DisplayDS16Path == m_modulePath
        CAMX_LOG_VERBOSE(CamxLogGroupISP, "DS16 Display Luma pre-Crop Config[%d, %d, %d, %d]",
            m_regCmd.displayDS16Luma.pixelConfig.bitfields.FIRST_PIXEL,
            m_regCmd.displayDS16Luma.lineConfig.bitfields.FIRST_LINE,
            m_regCmd.displayDS16Luma.pixelConfig.bitfields.LAST_PIXEL,
            m_regCmd.displayDS16Luma.lineConfig.bitfields.LAST_LINE);

        CAMX_LOG_VERBOSE(CamxLogGroupISP, "DS16 Display Chroma pre-Crop Config[%d, %d, %d, %d]",
            m_regCmd.displayDS16Chroma.pixelConfig.bitfields.FIRST_PIXEL,
            m_regCmd.displayDS16Chroma.lineConfig.bitfields.FIRST_LINE,
            m_regCmd.displayDS16Chroma.pixelConfig.bitfields.LAST_PIXEL,
            m_regCmd.displayDS16Chroma.lineConfig.bitfields.LAST_LINE);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEPreCrop10::CheckDependenceChange
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL IFEPreCrop10::CheckDependenceChange(
    ISPInputData* pInputData)
{
    BOOL result = FALSE;

    m_moduleEnable = FALSE;
    // For now enable the module only for single VFE case
    if (CropTypeCentered == pInputData->pStripeConfig->cropType)
    {
        if ((IFEPipelinePath::DS4Path == m_modulePath) || (IFEPipelinePath::DisplayDS4Path == m_modulePath))
        {
            m_moduleEnable = TRUE;
            result         = TRUE;
        }
    }
    else
    {
        if (TRUE == pInputData->pStripeConfig->overwriteStripes)
        {
            if ((IFEPipelinePath::DS4Path      == m_modulePath) ||
                (IFEPipelinePath::DS16Path     == m_modulePath) ||
                (IFEPipelinePath::DisplayDS4Path  == m_modulePath) ||
                (IFEPipelinePath::DisplayDS16Path == m_modulePath))
            {
                m_moduleEnable = TRUE;
                result         = TRUE;
            }
        }
    }

    if ((TRUE == m_moduleEnable) && (TRUE == pInputData->forceTriggerUpdate))
    {
        result = TRUE;
    }
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEPreCrop10::RunCalculation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFEPreCrop10::RunCalculation(
    ISPInputData* pInputData)
{
    CamxResult result      = CamxResultSuccess;

    if (IFEPipelinePath::DS4Path == m_modulePath)
    {
        DS4PreCropInfo* pPreCropInfo = &pInputData->pCalculatedData->preCropInfo;

        m_regCmd.DS4Luma.lineConfig.bitfields.FIRST_LINE   = pPreCropInfo->YCrop.firstLine;
        m_regCmd.DS4Luma.lineConfig.bitfields.LAST_LINE    = pPreCropInfo->YCrop.lastLine;
        m_regCmd.DS4Chroma.lineConfig.bitfields.FIRST_LINE = pPreCropInfo->CrCrop.firstLine;
        m_regCmd.DS4Chroma.lineConfig.bitfields.LAST_LINE  = pPreCropInfo->CrCrop.lastLine;

        // Copy the values computed in the main scalar
        m_regCmd.DS4Luma.pixelConfig.bitfields.FIRST_PIXEL = pPreCropInfo->YCrop.firstPixel;
        m_regCmd.DS4Luma.pixelConfig.bitfields.LAST_PIXEL  = pPreCropInfo->YCrop.lastPixel;

        m_regCmd.DS4Chroma.pixelConfig.bitfields.FIRST_PIXEL = pPreCropInfo->CrCrop.firstPixel;
        m_regCmd.DS4Chroma.pixelConfig.bitfields.LAST_PIXEL  = pPreCropInfo->CrCrop.lastPixel;
    }
    else if (IFEPipelinePath::DS16Path == m_modulePath)
    {
        DS4PreCropInfo* pPreCropInfoDS16 = &pInputData->pCalculatedData->preCropInfoDS16;

        m_regCmd.DS16Luma.lineConfig.bitfields.FIRST_LINE   = pPreCropInfoDS16->YCrop.firstLine;
        m_regCmd.DS16Luma.lineConfig.bitfields.LAST_LINE    = pPreCropInfoDS16->YCrop.lastLine;
        m_regCmd.DS16Chroma.lineConfig.bitfields.FIRST_LINE = pPreCropInfoDS16->CrCrop.firstLine;
        m_regCmd.DS16Chroma.lineConfig.bitfields.LAST_LINE  = pPreCropInfoDS16->CrCrop.lastLine;

        // Copy the values computed in the main scalar
        m_regCmd.DS16Luma.pixelConfig.bitfields.FIRST_PIXEL = pPreCropInfoDS16->YCrop.firstPixel;
        m_regCmd.DS16Luma.pixelConfig.bitfields.LAST_PIXEL  = pPreCropInfoDS16->YCrop.lastPixel;

        m_regCmd.DS16Chroma.pixelConfig.bitfields.FIRST_PIXEL = pPreCropInfoDS16->CrCrop.firstPixel;
        m_regCmd.DS16Chroma.pixelConfig.bitfields.LAST_PIXEL  = pPreCropInfoDS16->CrCrop.lastPixel;
    }
    else if (IFEPipelinePath::DisplayDS4Path == m_modulePath)
    {
        DS4PreCropInfo* pPreCropInfo = &pInputData->pCalculatedData->dispPreCropInfo;

        m_regCmd.displayDS4Luma.lineConfig.bitfields.FIRST_LINE   = pPreCropInfo->YCrop.firstLine;
        m_regCmd.displayDS4Luma.lineConfig.bitfields.LAST_LINE    = pPreCropInfo->YCrop.lastLine;
        m_regCmd.displayDS4Chroma.lineConfig.bitfields.FIRST_LINE = pPreCropInfo->CrCrop.firstLine;
        m_regCmd.displayDS4Chroma.lineConfig.bitfields.LAST_LINE  = pPreCropInfo->CrCrop.lastLine;

        // Copy the values computed in the main scalar
        m_regCmd.displayDS4Luma.pixelConfig.bitfields.FIRST_PIXEL = pPreCropInfo->YCrop.firstPixel;
        m_regCmd.displayDS4Luma.pixelConfig.bitfields.LAST_PIXEL  = pPreCropInfo->YCrop.lastPixel;

        m_regCmd.displayDS4Chroma.pixelConfig.bitfields.FIRST_PIXEL = pPreCropInfo->CrCrop.firstPixel;
        m_regCmd.displayDS4Chroma.pixelConfig.bitfields.LAST_PIXEL  = pPreCropInfo->CrCrop.lastPixel;
    }
    else if (IFEPipelinePath::DisplayDS16Path == m_modulePath)
    {
        DS4PreCropInfo* pPreCropInfoDS16 =  &pInputData->pCalculatedData->dispPreCropInfoDS16;

        m_regCmd.displayDS16Luma.lineConfig.bitfields.FIRST_LINE   = pPreCropInfoDS16->YCrop.firstLine;
        m_regCmd.displayDS16Luma.lineConfig.bitfields.LAST_LINE    = pPreCropInfoDS16->YCrop.lastLine;
        m_regCmd.displayDS16Chroma.lineConfig.bitfields.FIRST_LINE = pPreCropInfoDS16->CrCrop.firstLine;
        m_regCmd.displayDS16Chroma.lineConfig.bitfields.LAST_LINE  = pPreCropInfoDS16->CrCrop.lastLine;

        // Copy the values computed in the main scalar
        m_regCmd.displayDS16Luma.pixelConfig.bitfields.FIRST_PIXEL = pPreCropInfoDS16->YCrop.firstPixel;
        m_regCmd.displayDS16Luma.pixelConfig.bitfields.LAST_PIXEL  = pPreCropInfoDS16->YCrop.lastPixel;

        m_regCmd.displayDS16Chroma.pixelConfig.bitfields.FIRST_PIXEL = pPreCropInfoDS16->CrCrop.firstPixel;
        m_regCmd.displayDS16Chroma.pixelConfig.bitfields.LAST_PIXEL  = pPreCropInfoDS16->CrCrop.lastPixel;
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupISP, "Invalid IFE Path");
    }

    if (CamxLogGroupIQMod == (pInputData->dumpRegConfig & CamxLogGroupIQMod))
    {
        DumpRegConfig();
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEPreCrop10::UpdateIFEInternalData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IFEPreCrop10::UpdateIFEInternalData(
    ISPInputData* pInputData)
{
    IFE_IFE_0_VFE_MODULE_ZOOM_EN* pModuleEnable  = &pInputData->pCalculatedData->moduleEnable.frameProcessingModuleConfig;
    IFE_IFE_0_VFE_MODULE_DISP_EN* pDisplayEnable =
        &pInputData->pCalculatedData->moduleEnable.frameProcessingDisplayModuleConfig;

    if (NULL != pModuleEnable)
    {
        if (IFEPipelinePath::DS4Path == m_modulePath)
        {
            pModuleEnable->bitfields.DS_4TO1_1ST_PRE_CROP_EN  = (TRUE == m_moduleEnable) ? 1 : 0;

            if (FALSE == pInputData->pStripeConfig->overwriteStripes)
            {
                if ((TRUE == m_moduleEnable) && (CropTypeCentered == pInputData->pStripeConfig->cropType))
                {
                    // In single IFE mode disable the postcrop modules and enable only the precrop in the DS4 path.
                    // DS4/DS16 postcrop modules may output frames with slight drift in pixels which can cause
                    // artifacts that will be prominent when zoom is applied
                    pModuleEnable->bitfields.DS_4TO1_2ND_POST_CROP_EN = 0;
                    pModuleEnable->bitfields.DS_4TO1_1ST_POST_CROP_EN = 0;
                }
            }
        }
        else if (IFEPipelinePath::DS16Path == m_modulePath)
        {
            pModuleEnable->bitfields.DS_4TO1_2ND_PRE_CROP_EN = (TRUE == m_moduleEnable) ? 1 : 0;
        }
    }

    if (NULL != pDisplayEnable)
    {
        if (IFEPipelinePath::DisplayDS4Path == m_modulePath)
        {
            pDisplayEnable->bitfields.DS_4TO1_1ST_PRE_CROP_EN  = (TRUE == m_moduleEnable) ? 1 : 0;

            if (FALSE == pInputData->pStripeConfig->overwriteStripes)
            {
                if ((TRUE == m_moduleEnable) && (CropTypeCentered == pInputData->pStripeConfig->cropType))
                {
                    // In single IFE mode disable the postcrop modules and enable only the precrop in the DS4 path.
                    // DS4/DS16 postcrop modules may output frames with slight drift in pixels which can cause
                    // artifacts that will be prominent when zoom is applied
                    pDisplayEnable->bitfields.DS_4TO1_2ND_POST_CROP_EN = 0;
                    pDisplayEnable->bitfields.DS_4TO1_1ST_POST_CROP_EN = 0;
                }
            }
        }
        else if (IFEPipelinePath::DisplayDS16Path == m_modulePath)
        {
            pDisplayEnable->bitfields.DS_4TO1_2ND_PRE_CROP_EN = (TRUE == m_moduleEnable) ? 1 : 0;
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEPreCrop10::Execute
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFEPreCrop10::Execute(
    ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;

    CAMX_ASSERT(static_cast<UINT>(m_modulePath) < IFEMaxNonCommonPaths);

    m_pInputData = pInputData;

    if (NULL != pInputData)
    {
        // Check if dependent is valid and been updated compared to last request
        result = ValidateDependenceParams(pInputData);
        if (CamxResultSuccess == result)
        {
            if (TRUE == CheckDependenceChange(pInputData))
            {
                result = RunCalculation(pInputData);

                if (CamxResultSuccess == result)
                {
                    result = CreateCmdList(pInputData);
                }
            }
            if (CamxResultSuccess == result)
            {
                UpdateIFEInternalData(pInputData);
            }
        }
    }
    else
    {
        result = CamxResultEInvalidArg;
        CAMX_ASSERT_ALWAYS_MESSAGE("Invalid Input pointer");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEPreCrop10::IFEPreCrop10
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IFEPreCrop10::IFEPreCrop10(
    IFEModuleCreateData* pCreateData)
{
    m_type              = ISPIQModuleType::IFEPreCrop;
    m_moduleEnable      = FALSE;
    m_32bitDMILength    = 0;
    m_64bitDMILength    = 0;

    m_cmdLength = PacketBuilder::RequiredWriteRegRangeSizeInDwords(sizeof(IFEPreCrop10DS4LumaReg) / RegisterWidthInBytes) +
                  PacketBuilder::RequiredWriteRegRangeSizeInDwords(sizeof(IFEPreCrop10DS4ChromaReg) / RegisterWidthInBytes);

    m_modulePath = pCreateData->pipelineData.IFEPath;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IFEPreCrop10::~IFEPreCrop10
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IFEPreCrop10::~IFEPreCrop10()
{
}

CAMX_NAMESPACE_END
