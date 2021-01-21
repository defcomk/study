////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017-2018 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxifebpcbcc50.cpp
/// @brief CAMXIFEBPCBCC50 class implementation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "camxdefs.h"
#include "camxtuningdatamanager.h"
#include "parametertuningtypes.h"
#include "camxiqinterface.h"
#include "camxnode.h"
#include "camxifebpcbcc50.h"

CAMX_NAMESPACE_BEGIN

static const UINT32 IFEBPCBCC50RegLengthDWord = sizeof(IFEBPCBCC50RegCmd) / sizeof(UINT32);

CAMX_STATIC_ASSERT((8 * 4) == sizeof(IFEBPCBCC50RegCmd));

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEBPCBCC50::Create
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFEBPCBCC50::Create(
    IFEModuleCreateData* pCreateData)
{
    CamxResult result = CamxResultSuccess;

    if (NULL != pCreateData)
    {
        IFEBPCBCC50* pModule = CAMX_NEW IFEBPCBCC50;

        if (NULL != pModule)
        {
            result = pModule->Initialize();
            if (CamxResultSuccess != result)
            {
                CAMX_LOG_ERROR(CamxLogGroupPProc, "Module initialization failed");
                pModule->Destroy();
                pModule = NULL;
            }
        }
        else
        {
            result = CamxResultEFailed;
            CAMX_ASSERT_ALWAYS_MESSAGE("Failed to create IFEBPCBCC50 object.");
        }

        pCreateData->pModule = pModule;
    }
    else
    {
        result = CamxResultEInvalidArg;
        CAMX_ASSERT_ALWAYS_MESSAGE("Null Input Data for IFEBPCBCC50 Creation");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEBPCBCC50::Initialize
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFEBPCBCC50::Initialize()
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
// IFEBPCBCC50::Execute
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFEBPCBCC50::Execute(
    ISPInputData* pInputData)
{
    CamxResult result             = CamxResultSuccess;

    if (NULL != pInputData)
    {
        if (TRUE == CheckDependenceChange(pInputData))
        {
            result = RunCalculation(pInputData);
            if (CamxResultSuccess == result)
            {
                result = CreateCmdList(pInputData);
            }
            else
            {
                CAMX_ASSERT_ALWAYS_MESSAGE("BPC module calculation Failed.");
            }
        }
        if (CamxResultSuccess == result)
        {
            UpdateIFEInternalData(pInputData);
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEBPCBCC50::AllocateCommonLibraryData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFEBPCBCC50::AllocateCommonLibraryData()
{
    CamxResult result = CamxResultSuccess;

    UINT interpolationSize = (sizeof(bpcbcc_5_0_0::bpcbcc50_rgn_dataType) * (BPCBCC50MaxmiumNonLeafNode + 1));

    if (NULL == m_dependenceData.pInterpolationData)
    {
        // Alloc for bpcbcc_5_0_0::bpcbcc50_rgn_dataType
        m_dependenceData.pInterpolationData = CAMX_CALLOC(interpolationSize);
        if (NULL == m_dependenceData.pInterpolationData)
        {
            result = CamxResultENoMemory;
        }
    }
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEBPCBCC50::UpdateIFEInternalData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IFEBPCBCC50::UpdateIFEInternalData(
    ISPInputData* pInputData)
{
    pInputData->pCalculatedData->moduleEnable.lensProcessingModuleConfig.bits.BPC_EN = m_moduleEnable;

    pInputData->pCalculatedData->hotPixelMode = m_hotPixelMode;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEBPCBCC50::GetRegCmd
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID* IFEBPCBCC50::GetRegCmd()
{
    return static_cast<VOID*>(&m_regCmd);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEBPCBCC50::CheckDependenceChange
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL IFEBPCBCC50::CheckDependenceChange(
    ISPInputData* pInputData)
{
    BOOL isChanged = FALSE;

    if ((NULL != pInputData)                 &&
        (NULL != pInputData->pHwContext)     &&
        (NULL != pInputData->pAECUpdateData) &&
        (NULL != pInputData->pAWBUpdateData) &&
        (NULL != pInputData->pHALTagsData))
    {

        if (NULL != pInputData->pOEMIQSetting)
        {
            m_moduleEnable = (static_cast<OEMIFEIQSetting*>(pInputData->pOEMIQSetting))->BPCBCCEnable;

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

                m_pChromatix = pTuningManager->GetChromatix()->GetModule_bpcbcc50_ife(
                                   reinterpret_cast<TuningMode*>(&pInputData->pTuningData->TuningMode[0]),
                                   pInputData->pTuningData->noOfSelectionParameter);
            }

            CAMX_ASSERT(NULL != m_pChromatix);
            if (NULL != m_pChromatix)
            {
                if ((NULL == m_dependenceData.pChromatix)                                       ||
                    (m_pChromatix->SymbolTableID != m_dependenceData.pChromatix->SymbolTableID) ||
                    (m_moduleEnable              != m_pChromatix->enable_section.bpcbcc_enable))
                {
                    m_moduleEnable                   = m_pChromatix->enable_section.bpcbcc_enable;
                    m_dependenceData.pChromatix      = m_pChromatix;
                    m_dependenceData.symbolIDChanged = TRUE;
                    if (TRUE == m_moduleEnable)
                    {
                        isChanged = TRUE;
                    }
                }

                m_hotPixelMode = pInputData->pHALTagsData->hotPixelMode;
                if ((TRUE == m_moduleEnable) &&
                    (HotPixelModeOff == pInputData->pHALTagsData->hotPixelMode))
                {
                    m_moduleEnable = FALSE;
                    isChanged      = FALSE;
                }
            }
        }

        // Check for trigger update status
        if ((TRUE == m_moduleEnable) &&
            ((TRUE == IQInterface::s_interpolationTable.IFEBPCBCC50TriggerUpdate(&pInputData->triggerData,
                                                                                 &m_dependenceData))       ||
            (TRUE == pInputData->forceTriggerUpdate)))
        {
            if (NULL == pInputData->pOEMIQSetting)
            {
                // Check for module dynamic enable trigger hysterisis
                m_moduleEnable = IQSettingUtils::GetDynamicEnableFlag(
                    m_dependenceData.pChromatix->dynamic_enable_triggers.bpcbcc_enable.enable,
                    m_dependenceData.pChromatix->dynamic_enable_triggers.bpcbcc_enable.hyst_control_var,
                    m_dependenceData.pChromatix->dynamic_enable_triggers.bpcbcc_enable.hyst_mode,
                    &(m_dependenceData.pChromatix->dynamic_enable_triggers.bpcbcc_enable.hyst_trigger),
                    static_cast<VOID*>(&pInputData->triggerData),
                    &m_dependenceData.moduleEnable);

                // Set status to isChanged to avoid calling RunCalculation if the module is disabled
                isChanged = (TRUE == m_moduleEnable);
            }

            m_dependenceData.nonHdrMultFactor = 1.0f;
        }
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupISP, "pInputData is NULL ");
    }

    return isChanged;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEBPCBCC50::PrepareStripingParameters
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFEBPCBCC50::PrepareStripingParameters(
    ISPInputData* pInputData)
{
    CamxResult                            result               = CamxResultSuccess;
    TuningDataManager*                    pTuningManager       = NULL;
    bpcbcc_5_0_0::chromatix_bpcbcc50Type* pBPCBCCChromatixData = NULL;

    if (NULL != pInputData)
    {
        if (NULL != pInputData->pOEMIQSetting)
        {
            m_moduleEnable = (static_cast<OEMIFEIQSetting*>(pInputData->pOEMIQSetting))->BPCBCCEnable;
        }
        else
        {
            pTuningManager  = pInputData->pTuningDataManager;

            CAMX_ASSERT(NULL != pTuningManager);

            if (TRUE == pTuningManager->IsValidChromatix())
            {
                pBPCBCCChromatixData = pTuningManager->GetChromatix()->GetModule_bpcbcc50_ife(
                    reinterpret_cast<TuningMode*>(&pInputData->pTuningData->TuningMode[0]),
                    pInputData->pTuningData->noOfSelectionParameter);
            }


            if (NULL != pBPCBCCChromatixData)
            {
                if ((NULL == m_dependenceData.pChromatix) ||
                    (m_dependenceData.pChromatix->SymbolTableID != pBPCBCCChromatixData->SymbolTableID))
                {
                    m_moduleEnable = pBPCBCCChromatixData->enable_section.bpcbcc_enable;
                }
            }
        }
        if (NULL != pInputData->pStripingInput)
        {
            pInputData->pStripingInput->enableBits.BPC = m_moduleEnable;
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
// IFEBPCBCC50::CreateCmdList
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFEBPCBCC50::CreateCmdList(
    ISPInputData* pInputData)
{
    CamxResult result     = CamxResultSuccess;
    CmdBuffer* pCmdBuffer = NULL;

    if (NULL != pInputData->pCmdBuffer)
    {
        pCmdBuffer = pInputData->pCmdBuffer;
        result     = PacketBuilder::WriteRegRange(pCmdBuffer,
                                                  regIFE_IFE_0_VFE_BPC_CFG_0,
                                                  IFEBPCBCC50RegLengthDWord,
                                                  reinterpret_cast<UINT32*>(&m_regCmd));
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
/// IFEBPCBCC50::DumpRegConfig
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IFEBPCBCC50::DumpRegConfig() const
{
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "configRegister0 [0x%x]", m_regCmd.configRegister0.u32All);
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "configRegister1 [0x%x]", m_regCmd.configRegister1.u32All);
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "configRegister2 [0x%x]", m_regCmd.configRegister2.u32All);
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "configRegister3 [0x%x]", m_regCmd.configRegister3.u32All);
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "configRegister4 [0x%x]", m_regCmd.configRegister4.u32All);
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "configRegister5 [0x%x]", m_regCmd.configRegister5.u32All);
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "configRegister6 [0x%x]", m_regCmd.configRegister6.u32All);
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "statsRegister   [0x%x]", m_regCmd.statsRegister.u32All);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEBPCBCC50::RunCalculation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFEBPCBCC50::RunCalculation(
    const ISPInputData* pInputData)
{
    CamxResult         result     = CamxResultSuccess;
    BPCBCC50OutputData outputData;

    outputData.pRegCmd = &m_regCmd;

    result = IQInterface::IFEBPCBCC50CalculateSetting(&m_dependenceData, pInputData->pOEMIQSetting, &outputData);

    if (CamxResultSuccess != result)
    {
        CAMX_LOG_ERROR(CamxLogGroupISP, "BPCBCC Calculation Failed.");
    }

    if (CamxLogGroupIQMod == (pInputData->dumpRegConfig & CamxLogGroupIQMod))
    {
        DumpRegConfig();
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEBPCBCC50::DeallocateCommonLibraryData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IFEBPCBCC50::DeallocateCommonLibraryData()
{
    if (NULL != m_dependenceData.pInterpolationData)
    {
        CAMX_FREE(m_dependenceData.pInterpolationData);
        m_dependenceData.pInterpolationData = NULL;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEBPCBCC50::IFEBPCBCC50
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IFEBPCBCC50::IFEBPCBCC50()
{
    m_type       = ISPIQModuleType::IFEBPCBCC;
    m_cmdLength  = PacketBuilder::RequiredWriteRegRangeSizeInDwords(IFEBPCBCC50RegLengthDWord);
    m_pChromatix = NULL;

    m_dependenceData.moduleEnable = FALSE; ///< First frame is always FALSE
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IFEBPCBCC50::~IFEBPCBCC50
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IFEBPCBCC50::~IFEBPCBCC50()
{
    m_pChromatix = NULL;
    DeallocateCommonLibraryData();
}

CAMX_NAMESPACE_END
