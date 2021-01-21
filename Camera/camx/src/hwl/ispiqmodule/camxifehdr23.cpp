////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2018 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxifehdr23.cpp
/// @brief CAMXIFEHDR23 class implementation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "camxdefs.h"
#include "camxifehdr23.h"
#include "camxiqinterface.h"
#include "camxnode.h"
#include "camxtuningdatamanager.h"
#include "parametertuningtypes.h"

CAMX_NAMESPACE_BEGIN

static const UINT32 IFEHDR23RegLengthDWord1 = sizeof(IFEHDR23RegCmd1) / sizeof(UINT32);
static const UINT32 IFEHDR23RegLengthDWord2 = sizeof(IFEHDR23RegCmd2) / sizeof(UINT32);
static const UINT32 IFEHDR23RegLengthDWord3 = sizeof(IFEHDR23RegCmd3) / sizeof(UINT32);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEHDR23::Create
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFEHDR23::Create(
    IFEModuleCreateData* pCreateData)
{
    CamxResult result = CamxResultSuccess;

    if (NULL != pCreateData)
    {
        IFEHDR23* pModule = CAMX_NEW IFEHDR23;

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
            CAMX_ASSERT_ALWAYS_MESSAGE("Failed to create IFEHDR23 object.");
        }

        pCreateData->pModule = pModule;
    }
    else
    {
        result = CamxResultEInvalidArg;
        CAMX_ASSERT_ALWAYS_MESSAGE("Null Input Data for IFEHDR23 Creation");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEHDR23::Initialize
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFEHDR23::Initialize()
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
// IFEHDR23::Execute
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFEHDR23::Execute(
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
// IFEHDR23::AllocateCommonLibraryData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFEHDR23::AllocateCommonLibraryData()
{
    CamxResult result = CamxResultSuccess;

    UINT interpolationSize = (sizeof(hdr_2_3_0::hdr23_rgn_dataType) * (HDR23MaxmiumNonLeafNode + 1));

    if (NULL == m_dependenceData.pInterpolationData)
    {
        // Alloc for hdr_2_0_0::hdr23_rgn_dataType
        m_dependenceData.pInterpolationData = CAMX_CALLOC(interpolationSize);
        if (NULL == m_dependenceData.pInterpolationData)
        {
            result = CamxResultENoMemory;
        }
    }
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEHDR23::UpdateIFEInternalData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IFEHDR23::UpdateIFEInternalData(
    ISPInputData* pInputData)
{
    CAMX_ASSERT(NULL != pInputData->pCalculatedData);
    pInputData->pCalculatedData->moduleEnable.lensProcessingModuleConfig.bitfields.HDR_MAC_EN   = m_MACEnable;
    pInputData->pCalculatedData->moduleEnable.lensProcessingModuleConfig.bitfields.HDR_RECON_EN = m_RECONEnable;

    // Post tuning metadata if setting is enabled
    if (NULL != pInputData->pIFETuningMetadata)
    {
        CAMX_STATIC_ASSERT(sizeof(IFEHDR23RegCmd1) ==
                           sizeof(pInputData->pIFETuningMetadata->IFEHDRData.HDRMACReconConfig));
        CAMX_STATIC_ASSERT(sizeof(IFEHDR23RegCmd2) ==
                           sizeof(pInputData->pIFETuningMetadata->IFEHDRData.HDRBLK));
        CAMX_STATIC_ASSERT(sizeof(IFEHDR23RegCmd3) ==
                           sizeof(pInputData->pIFETuningMetadata->IFEHDRData.HDRReconConfig));

        Utils::Memcpy(&pInputData->pIFETuningMetadata->IFEHDRData.HDRMACReconConfig[0],
                      &m_regCmd.m_regCmd1,
                      sizeof(IFEHDR23RegCmd1));
        Utils::Memcpy(&pInputData->pIFETuningMetadata->IFEHDRData.HDRBLK[0],
                      &m_regCmd.m_regCmd2,
                      sizeof(IFEHDR23RegCmd2));
        Utils::Memcpy(&pInputData->pIFETuningMetadata->IFEHDRData.HDRReconConfig[0],
                      &m_regCmd.m_regCmd3,
                      sizeof(IFEHDR23RegCmd3));

    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEHDR23::GetRegCmd
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID* IFEHDR23::GetRegCmd()
{
    return static_cast<VOID*>(&m_regCmd);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEHDR23::CheckDependenceChange
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL IFEHDR23::CheckDependenceChange(
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

            if (TRUE == m_moduleEnable)
            {
                isChanged = TRUE;
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

                m_pChromatix = pTuningManager->GetChromatix()->GetModule_hdr23_ife(
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

                    if (TRUE == m_moduleEnable)
                    {
                        m_MACEnable   = m_pChromatix->chromatix_hdr23_reserve.hdr_mac_en;
                        m_RECONEnable = m_pChromatix->chromatix_hdr23_reserve.hdr_recon_en;
                    }

                    m_moduleEnable = ((TRUE == m_MACEnable) && (TRUE == m_RECONEnable));

                    if (FALSE == m_moduleEnable)
                    {
                        m_MACEnable   = FALSE;
                        m_RECONEnable = FALSE;
                    }

                    isChanged = (TRUE == m_moduleEnable);
                }
            }
            else
            {
                CAMX_LOG_ERROR(CamxLogGroupISP, "Failed to get Chromatix");
            }
        }

        if (TRUE == m_moduleEnable)
        {
            m_dependenceData.ZZHDRFirstRBEXP   =
                static_cast<UINT16>(pInputData->sensorData.ZZHDRFirstExposurePattern);
            m_dependenceData.ZZHDRMode         = 1;
            m_dependenceData.ZZHDRPattern      =
                static_cast<UINT16>(pInputData->sensorData.ZZHDRColorPattern);
            m_dependenceData.RECONFirstField   =
                static_cast<UINT16>(pInputData->sensorData.ZZHDRFirstExposurePattern);
            m_dependenceData.HDRZigzagReconSel = 1;

            // Check for trigger update status
            if ((TRUE == IQInterface::s_interpolationTable.IFEHDR23TriggerUpdate(&pInputData->triggerData,
                                                                                 &m_dependenceData))       ||
                (TRUE == pInputData->forceTriggerUpdate))
            {
                if (NULL == pInputData->pOEMIQSetting)
                {
                    // Check for HDR MAC dynamic enable trigger hysterisis
                    m_MACEnable = IQSettingUtils::GetDynamicEnableFlag(
                        m_dependenceData.pChromatix->dynamic_enable_triggers.hdr_mac_en.enable,
                        m_dependenceData.pChromatix->dynamic_enable_triggers.hdr_mac_en.hyst_control_var,
                        m_dependenceData.pChromatix->dynamic_enable_triggers.hdr_mac_en.hyst_mode,
                        &(m_dependenceData.pChromatix->dynamic_enable_triggers.hdr_mac_en.hyst_trigger),
                        static_cast<VOID*>(&pInputData->triggerData),
                        &m_dependenceData.moduleMacEnable);

                    // Check for HDR RECON dynamic enable trigger hysterisis
                    m_RECONEnable = IQSettingUtils::GetDynamicEnableFlag(
                        m_dependenceData.pChromatix->dynamic_enable_triggers.hdr_recon_en.enable,
                        m_dependenceData.pChromatix->dynamic_enable_triggers.hdr_recon_en.hyst_control_var,
                        m_dependenceData.pChromatix->dynamic_enable_triggers.hdr_recon_en.hyst_mode,
                        &(m_dependenceData.pChromatix->dynamic_enable_triggers.hdr_recon_en.hyst_trigger),
                        static_cast<VOID*>(&pInputData->triggerData),
                        &m_dependenceData.moduleReconEnable);

                    m_moduleEnable = ((TRUE == m_MACEnable) && (TRUE == m_RECONEnable));

                    if (FALSE == m_moduleEnable)
                    {
                        m_MACEnable   = FALSE;
                        m_RECONEnable = FALSE;
                    }

                    // Set status to isChanged to avoid calling RunCalculation if the module is disabled
                    isChanged = (TRUE == m_moduleEnable);
                }
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
// IFEHDR23::PrepareStripingParameters
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFEHDR23::PrepareStripingParameters(
    ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;

    if (NULL != pInputData)
    {
        if (TRUE == CheckDependenceChange(pInputData))
        {
            HDR23OutputData outputData;
            IFEHDR23RegCmd  regCmd;
            HDR23InputData  inputData;

            outputData.pRegCmd = &regCmd;
            Utils::Memcpy(&inputData, &m_dependenceData, sizeof(HDR23InputData));

            result = IQInterface::HDR23CalculateSetting(&inputData, pInputData->pOEMIQSetting, &outputData);

            if (CamxResultSuccess != result)
            {
                CAMX_LOG_ERROR(CamxLogGroupISP, "HDR23 Calculation Failed.");
            }
            else if (NULL != pInputData->pStripingInput)
            {
                pInputData->pStripingInput->enableBits.HDR           = m_moduleEnable;
                pInputData->pStripingInput->stripingInput.hdr_enable = static_cast<int16_t>(m_moduleEnable);
                pInputData->pStripingInput->stripingInput.hdr_in.hdr_zrec_first_rb_exp =
                    outputData.hdr23State.hdr_zrec_first_rb_exp;
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
// IFEHDR23::CreateCmdList
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFEHDR23::CreateCmdList(
    const ISPInputData* pInputData)
{
    CamxResult result     = CamxResultSuccess;
    CmdBuffer* pCmdBuffer = NULL;

    if (NULL != pInputData->pCmdBuffer)
    {
        pCmdBuffer = pInputData->pCmdBuffer;

        result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                              regIFE_IFE_0_VFE_HDR_CFG_0,
                                              IFEHDR23RegLengthDWord1,
                                              reinterpret_cast<UINT32*>(&m_regCmd.m_regCmd1));

        if (CamxResultSuccess == result)
        {
            result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                                  regIFE_IFE_0_VFE_HDR_CFG_5,
                                                  IFEHDR23RegLengthDWord2,
                                                  reinterpret_cast<UINT32*>(&m_regCmd.m_regCmd2));
        }

        if (CamxResultSuccess == result)
        {
            result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                                  regIFE_IFE_0_VFE_HDR_RECON_CFG_5,
                                                  IFEHDR23RegLengthDWord3,
                                                  reinterpret_cast<UINT32*>(&m_regCmd.m_regCmd3));
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
// IFEHDR23::RunCalculation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFEHDR23::RunCalculation(
    const ISPInputData* pInputData)
{
    CamxResult        result = CamxResultEFailed;
    HDR23OutputData   outputData;

    outputData.pRegCmd = &m_regCmd;
    result = IQInterface::HDR23CalculateSetting(&m_dependenceData, pInputData->pOEMIQSetting, &outputData);

    if (CamxResultSuccess != result)
    {
        CAMX_LOG_ERROR(CamxLogGroupISP, "HDR23 Calculation Failed.");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEHDR23::DeallocateCommonLibraryData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IFEHDR23::DeallocateCommonLibraryData()
{
    if (NULL != m_dependenceData.pInterpolationData)
    {
        CAMX_FREE(m_dependenceData.pInterpolationData);
        m_dependenceData.pInterpolationData = NULL;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEHDR23::IFEHDR23
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IFEHDR23::IFEHDR23()
{
    m_type           = ISPIQModuleType::IFEHDR;
    m_32bitDMILength = 0;
    m_64bitDMILength = 0;
    m_cmdLength      = PacketBuilder::RequiredWriteRegRangeSizeInDwords(IFEHDR23RegLengthDWord1) +
                       PacketBuilder::RequiredWriteRegRangeSizeInDwords(IFEHDR23RegLengthDWord2) +
                       PacketBuilder::RequiredWriteRegRangeSizeInDwords(IFEHDR23RegLengthDWord3);
    m_pChromatix     = NULL;

    m_dependenceData.moduleMacEnable   = FALSE; ///< First frame is always FALSE
    m_dependenceData.moduleReconEnable = FALSE; ///< First frame is always FALSE
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IFEHDR23::~IFEHDR23
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IFEHDR23::~IFEHDR23()
{
    m_pChromatix = NULL;
    DeallocateCommonLibraryData();
}

CAMX_NAMESPACE_END
