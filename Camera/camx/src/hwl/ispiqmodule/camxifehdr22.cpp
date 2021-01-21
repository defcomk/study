////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017-2018 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxifehdr22.cpp
/// @brief CAMXIFEHDR22 class implementation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "camxdefs.h"
#include "camxifehdr22.h"
#include "camxiqinterface.h"
#include "camxnode.h"
#include "camxtuningdatamanager.h"
#include "parametertuningtypes.h"

CAMX_NAMESPACE_BEGIN

static const UINT32 IFEHDR22RegLengthDWord1 = sizeof(IFEHDR22RegCmd1)  / sizeof(UINT32);
static const UINT32 IFEHDR22RegLengthDWord2 = sizeof(IFEHDR22RegCmd2) / sizeof(UINT32);
static const UINT32 IFEHDR22RegLengthDWord3 = sizeof(IFEHDR22RegCmd3) / sizeof(UINT32);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEHDR22::Create
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFEHDR22::Create(
    IFEModuleCreateData* pCreateData)
{
    CamxResult result = CamxResultSuccess;

    if (NULL != pCreateData)
    {
        IFEHDR22* pModule = CAMX_NEW IFEHDR22;

        if (NULL != pModule)
        {
            result = pModule->Initialize();
            if (CamxResultSuccess != result)
            {
                CAMX_LOG_ERROR(CamxLogGroupISP, "Module initialization failed !!");
                pModule->Destroy();
                pModule = NULL;
            }
        }
        else
        {
            result = CamxResultEFailed;
            CAMX_ASSERT_ALWAYS_MESSAGE("Failed to create IFEHDR22 object.");
        }

        pCreateData->pModule = pModule;
    }
    else
    {
        result = CamxResultEInvalidArg;
        CAMX_ASSERT_ALWAYS_MESSAGE("Null Input Data for IFEHDR22 Creation");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEHDR22::Initialize
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFEHDR22::Initialize()
{
    CamxResult result = CamxResultSuccess;

    result = AllocateCommonLibraryData();
    if (result != CamxResultSuccess)
    {
        CAMX_LOG_ERROR(CamxLogGroupISP, "Unable to initilize common library data, no memory");
    }
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEHDR22::Execute
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFEHDR22::Execute(
    ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;

    if (NULL != pInputData)
    {
        if (TRUE == CheckDependenceChange(pInputData))
        {
            if (CamxResultSuccess == RunCalculation(pInputData))
            {
                result = CreateCmdList(pInputData);
            }
        }

        if (CamxResultSuccess == result)
        {
            UpdateIFEInternalData(pInputData);
        }
    }
    else
    {
        CAMX_ASSERT_ALWAYS_MESSAGE("Null Input Data");
        result = CamxResultEInvalidArg;
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEHDR22::AllocateCommonLibraryData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFEHDR22::AllocateCommonLibraryData()
{
    CamxResult result = CamxResultSuccess;

    UINT interpolationSize = (sizeof(hdr_2_2_0::hdr22_rgn_dataType) * (HDR22MaxmiumNonLeafNode + 1));

    if (NULL == m_dependenceData.pInterpolationData)
    {
        // Alloc for hdr_2_0_0::hdr22_rgn_dataType
        m_dependenceData.pInterpolationData = CAMX_CALLOC(interpolationSize);
        if (NULL == m_dependenceData.pInterpolationData)
        {
            result = CamxResultENoMemory;
        }
    }
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEHDR22::UpdateIFEInternalData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IFEHDR22::UpdateIFEInternalData(
    ISPInputData* pInputData)
{
    CAMX_ASSERT(NULL != pInputData->pCalculatedData);
    pInputData->pCalculatedData->moduleEnable.lensProcessingModuleConfig.bitfields.HDR_MAC_EN   = m_MACEnable;
    pInputData->pCalculatedData->moduleEnable.lensProcessingModuleConfig.bitfields.HDR_RECON_EN = m_RECONEnable;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEHDR22::GetRegCmd
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID* IFEHDR22::GetRegCmd()
{
    return static_cast<VOID*>(&m_regCmd);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEHDR22::CheckDependenceChange
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL IFEHDR22::CheckDependenceChange(
    ISPInputData* pInputData)
{
    BOOL isChanged = FALSE;

    if ((NULL != pInputData->pHwContext)     &&
        (NULL != pInputData->pAECUpdateData) &&
        (NULL != pInputData->pAWBUpdateData))
    {
        if (NULL != pInputData->pOEMIQSetting)
        {
            m_moduleEnable = (static_cast<OEMIFEIQSetting*>(pInputData->pOEMIQSetting))->HDREnable;
            isChanged      = (TRUE == m_moduleEnable);

            if (TRUE == m_moduleEnable)
            {
                m_MACEnable   = TRUE;
                m_RECONEnable = TRUE;
            }
            else
            {
                m_MACEnable   = FALSE;
                m_RECONEnable = FALSE;
            }
        }
        else
        {
            TuningDataManager* pTuningManager = pInputData->pTuningDataManager;
            CAMX_ASSERT(NULL != pTuningManager);

            // Search through the tuning data (tree), only when there
            // are changes to the tuning mode data as an optimization
            if ((TRUE == pInputData->tuningModeChanged)    &&
                (TRUE == pTuningManager->IsValidChromatix()))
            {
                CAMX_ASSERT(NULL != pInputData->pTuningData);

                m_pChromatix = pTuningManager->GetChromatix()->GetModule_hdr22_ife(
                                   reinterpret_cast<TuningMode*>(&pInputData->pTuningData->TuningMode[0]),
                                   pInputData->pTuningData->noOfSelectionParameter);
            }

            CAMX_ASSERT(NULL != m_pChromatix);

            if (NULL != m_pChromatix)
            {
                if ((NULL == m_dependenceData.pChromatix) ||
                    (m_pChromatix->SymbolTableID != m_dependenceData.pChromatix->SymbolTableID))
                {
                    m_dependenceData.pChromatix = m_pChromatix;
                    m_moduleEnable              = m_pChromatix->enable_section.hdr_enable;
                    m_MACEnable                 = FALSE;
                    m_RECONEnable               = FALSE;

                    if (TRUE == m_moduleEnable)
                    {
                        m_MACEnable = m_pChromatix->chromatix_hdr22_reserve.hdr_mac_en;
                        m_RECONEnable = m_pChromatix->chromatix_hdr22_reserve.hdr_recon_en;

                        if ( TRUE == m_MACEnable && TRUE == m_RECONEnable )
                        {
                            isChanged     = TRUE;
                        }
                        else
                        {
                            m_moduleEnable = FALSE;
                        }
                    }
                }
            }
            else
            {
                CAMX_LOG_ERROR(CamxLogGroupISP, "Failed to get Chromatix");
            }
        }

        if (TRUE == m_moduleEnable)
        {
            CamxResult result = CamxResultSuccess;
            isChanged         = TRUE;

            m_dependenceData.blackLevelOffset =
                static_cast<UINT16>(pInputData->pCalculatedData->BLSblackLevelOffset);
            m_dependenceData.ZZHDRFirstRBEXP  =
                static_cast<UINT16>(pInputData->sensorData.ZZHDRFirstExposurePattern);
            m_dependenceData.ZZHDRMode        = 1;
            m_dependenceData.ZZHDRPattern     =
                static_cast<UINT16>(pInputData->sensorData.ZZHDRColorPattern);
            m_dependenceData.RECONFirstField  =
                static_cast<UINT16>(pInputData->sensorData.ZZHDRFirstExposurePattern);

            result = IQInterface::GetPixelFormat(&pInputData->sensorData.format, &(m_dependenceData.bayerPattern));

            if ((TRUE == IQInterface::s_interpolationTable.HDR22TriggerUpdate(&(pInputData->triggerData),
                                                                                &m_dependenceData)) ||
                (TRUE == pInputData->forceTriggerUpdate))
            {
                isChanged = TRUE;
            }
        }
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupISP,
                       "Invalid Input: pNewAECUpdate %x pNewAWBUpdate %x  HwContext %x",
                       pInputData->pAECUpdateData,
                       pInputData->pAWBUpdateData,
                       pInputData->pHwContext);
    }

    return isChanged;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEHDR22::PrepareStripingParameters
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFEHDR22::PrepareStripingParameters(
    ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;

    if (NULL != pInputData)
    {
        if (TRUE == CheckDependenceChange(pInputData))
        {
            HDR22OutputData outputData;
            IFEHDR22RegCmd  regCmd;
            HDR22InputData  inputData;

            outputData.type                = PipelineType::IFE;
            outputData.IFE.pRegCmd = &regCmd;
            Utils::Memcpy(&inputData, &m_dependenceData, sizeof(HDR22InputData));

            result = IQInterface::HDR22CalculateSetting(&inputData, pInputData->pOEMIQSetting, &outputData);

            if (CamxResultSuccess != result)
            {
                CAMX_LOG_ERROR(CamxLogGroupISP, "HDR22 Calculation Failed.");
            }
            else if (NULL != pInputData->pStripingInput)
            {
                pInputData->pStripingInput->enableBits.HDR = m_moduleEnable;
                pInputData->pStripingInput->stripingInput.hdr_enable = static_cast<int16_t>(m_moduleEnable);
            }

            if (CamxResultSuccess != result)
            {
                CAMX_ASSERT_ALWAYS_MESSAGE("HDR module calculation Failed.");
            }
        }
    }
    else
    {
        CAMX_ASSERT_ALWAYS_MESSAGE("Null Input Data");
        result = CamxResultEInvalidArg;
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEHDR22::CreateCmdList
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFEHDR22::CreateCmdList(
    const ISPInputData* pInputData)
{
    CamxResult result     = CamxResultSuccess;
    CmdBuffer* pCmdBuffer = NULL;

    if (NULL != pInputData->pCmdBuffer)
    {
        pCmdBuffer = pInputData->pCmdBuffer;

        result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                              regIFE_IFE_0_VFE_HDR_CFG_0,
                                              IFEHDR22RegLengthDWord1,
                                              reinterpret_cast<UINT32*>(&m_regCmd.m_regCmd1));

        CAMX_ASSERT(CamxResultSuccess == result);

        result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                              regIFE_IFE_0_VFE_HDR_CFG_5,
                                              IFEHDR22RegLengthDWord2,
                                              reinterpret_cast<UINT32*>(&m_regCmd.m_regCmd2));

        CAMX_ASSERT(CamxResultSuccess == result);

        result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                              regIFE_IFE_0_VFE_HDR_RECON_CFG_5,
                                              IFEHDR22RegLengthDWord3,
                                              reinterpret_cast<UINT32*>(&m_regCmd.m_regCmd3));

        CAMX_ASSERT(CamxResultSuccess == result);
    }
    else
    {
        result = CamxResultEInvalidArg;
        CAMX_ASSERT_ALWAYS_MESSAGE("Invalid Input data or command buffer");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEHDR22::RunCalculation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFEHDR22::RunCalculation(
    const ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;

    if (NULL != pInputData)
    {
        HDR22OutputData outputData;

        outputData.type                = PipelineType::IFE;
        outputData.IFE.pRegCmd         = &m_regCmd;

        result = IQInterface::HDR22CalculateSetting(&m_dependenceData, pInputData->pOEMIQSetting, &outputData);

        if (CamxResultSuccess != result)
        {
            CAMX_LOG_ERROR(CamxLogGroupISP, "HDR22 Calculation Failed. result %d", result);
        }
    }
    else
    {
        result = CamxResultEInvalidPointer;
        CAMX_ASSERT_ALWAYS_MESSAGE("Null Input Pointer");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEHDR22::DeallocateCommonLibraryData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IFEHDR22::DeallocateCommonLibraryData()
{
    if (NULL != m_dependenceData.pInterpolationData)
    {
        CAMX_FREE(m_dependenceData.pInterpolationData);
        m_dependenceData.pInterpolationData = NULL;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEHDR22::IFEHDR22
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IFEHDR22::IFEHDR22()
{
    m_type           = ISPIQModuleType::IFEHDR;
    m_32bitDMILength = 0;
    m_64bitDMILength = 0;
    m_cmdLength      = PacketBuilder::RequiredWriteRegRangeSizeInDwords(IFEHDR22RegLengthDWord1)  +
                       PacketBuilder::RequiredWriteRegRangeSizeInDwords(IFEHDR22RegLengthDWord2) +
                       PacketBuilder::RequiredWriteRegRangeSizeInDwords(IFEHDR22RegLengthDWord3);
    m_moduleEnable   = TRUE;
    m_pChromatix     = NULL;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IFEHDR22::~IFEHDR22
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IFEHDR22::~IFEHDR22()
{
    m_pChromatix = NULL;
    DeallocateCommonLibraryData();
}

CAMX_NAMESPACE_END
