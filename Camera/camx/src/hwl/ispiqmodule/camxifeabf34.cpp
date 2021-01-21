////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016-2018 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxifeabf34.cpp
/// @brief IFEABF34 class implementation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "camxdefs.h"
#include "camxtuningdatamanager.h"
#include "parametertuningtypes.h"
#include "camxiqinterface.h"
#include "camxispiqmodule.h"
#include "camxnode.h"
#include "camxifeabf34.h"


CAMX_NAMESPACE_BEGIN

static const UINT8 ABFBank0 = IFE_IFE_0_VFE_DMI_CFG_DMI_RAM_SEL_ABF_STD2_L0_BANK0;
static const UINT8 ABFBank1 = IFE_IFE_0_VFE_DMI_CFG_DMI_RAM_SEL_ABF_STD2_L0_BANK1;

static const UINT32 IFEABF34RegLength1DWord = sizeof(IFEABF34RegCmd1) / sizeof(UINT32);
static const UINT32 IFEABF34RegLength2DWord = sizeof(IFEABF34RegCmd2) / sizeof(UINT32);


static const UINT32 IFEABF34LUTLengthDWord = DMIRAM_ABF34_NOISESTD_LENGTH;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEABF34::Create
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFEABF34::Create(
    IFEModuleCreateData* pCreateData)
{
    CamxResult result = CamxResultSuccess;

    if (NULL != pCreateData)
    {
        IFEABF34* pModule = CAMX_NEW IFEABF34;

        if (NULL != pModule)
        {
            CamX::Utils::Memset(&pCreateData->initializationData.pStripeConfig->stateABF,
                                0,
                                sizeof(pCreateData->initializationData.pStripeConfig->stateABF));

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
            CAMX_ASSERT_ALWAYS_MESSAGE("Failed to create IFEABF34 object.");
        }

        pCreateData->pModule = pModule;
    }
    else
    {
        result = CamxResultEInvalidArg;
        CAMX_ASSERT_ALWAYS_MESSAGE("Null Input Data for IFEABF34 Creation");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEABF34::Initialize
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFEABF34::Initialize()
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
// IFEABF34::Execute
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFEABF34::Execute(
    ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;

    if ((NULL != pInputData)                    &&
        (NULL != pInputData->pCmdBuffer)        &&
        (NULL != pInputData->p32bitDMIBuffer)   &&
        (NULL != pInputData->p32bitDMIBufferAddr))
    {
        m_pState = &pInputData->pStripeConfig->stateABF;

        if (TRUE == CheckDependenceChange(pInputData))
        {
            result = RunCalculation(pInputData);

            if (CamxResultSuccess == result)
            {
                result = CreateCmdList(pInputData);
            }
            else
            {
                CAMX_ASSERT_ALWAYS_MESSAGE("ABF module calculation Failed.");
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
// IFEABF34::AllocateCommonLibraryData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFEABF34::AllocateCommonLibraryData()
{
    CamxResult result = CamxResultSuccess;

    UINT interpolationSize = (sizeof(abf_3_4_0::abf34_rgn_dataType) * (ABF34MaxmiumNonLeafNode + 1));

    if (NULL == m_pInterpolationData)
    {
        // Alloc for abf_3_4_0::abf34_rgn_dataType
        m_pInterpolationData = CAMX_CALLOC(interpolationSize);
        if (NULL == m_pInterpolationData)
        {
            result = CamxResultENoMemory;
        }
    }
    return result;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEABF34::UpdateIFEInternalData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IFEABF34::UpdateIFEInternalData(
    ISPInputData* pInputData)
{
    pInputData->pCalculatedData->moduleEnable.lensProcessingModuleConfig.bits.ABF_EN = m_moduleEnable;
    pInputData->pCalculatedData->noiseReductionMode = m_noiseReductionMode;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEABF34::GetRegCmd
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID* IFEABF34::GetRegCmd()
{
    return reinterpret_cast<VOID*>(&m_regCmd1);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEABF34::CheckDependenceChange
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL IFEABF34::CheckDependenceChange(
    ISPInputData* pInputData)
{
    BOOL isChanged        = FALSE;

    if ((NULL != pInputData->pHwContext)     &&
        (NULL != pInputData->pAECUpdateData) &&
        (NULL != pInputData->pAWBUpdateData) &&
        (NULL != pInputData->pHALTagsData))
    {
        if (NULL != pInputData->pOEMIQSetting)
        {
            m_moduleEnable = (static_cast<OEMIFEIQSetting*>(pInputData->pOEMIQSetting))->ABFEnable;

            if (TRUE == m_moduleEnable)
            {
                isChanged = TRUE;
            }
            m_noiseReductionMode = NoiseReductionModeFast;
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

                m_pChromatix = pTuningManager->GetChromatix()->GetModule_abf34_ife(
                                   reinterpret_cast<TuningMode*>(&pInputData->pTuningData->TuningMode[0]),
                                   pInputData->pTuningData->noOfSelectionParameter);
            }

            CAMX_ASSERT(NULL != m_pChromatix);
            if (NULL != m_pChromatix)
            {
                if ((NULL == m_pState->dependenceData.pChromatix)                                       ||
                    (m_pChromatix->SymbolTableID != m_pState->dependenceData.pChromatix->SymbolTableID))
                {
                    m_pState->dependenceData.pChromatix         = m_pChromatix;
                    m_pState->dependenceData.pInterpolationData = m_pInterpolationData;
                    m_moduleSBPCEnable                          = m_pChromatix->enable_section.sbpc_enable;
                    m_moduleEnable                              = m_pChromatix->enable_section.abf_enable;
                }
            }
        }

        m_noiseReductionMode = pInputData->pHALTagsData->noiseReductionMode;

        if (m_noiseReductionMode == NoiseReductionModeOff)
        {
            m_moduleEnable = FALSE;
            isChanged      = FALSE;
        }

        // Check for trigger update status
        if ((TRUE == m_moduleEnable || TRUE == m_moduleSBPCEnable)               &&
            (TRUE == IQInterface::s_interpolationTable.IFEABF34TriggerUpdate(
                     &pInputData->triggerData, &m_pState->dependenceData)    ||
            (TRUE == pInputData->forceTriggerUpdate)))
        {
            if (NULL == pInputData->pOEMIQSetting)
            {
                // Check for module dynamic enable trigger hysterisis
                m_moduleEnable = IQSettingUtils::GetDynamicEnableFlag(
                    m_pState->dependenceData.pChromatix->dynamic_enable_triggers.abf_enable.enable,
                    m_pState->dependenceData.pChromatix->dynamic_enable_triggers.abf_enable.hyst_control_var,
                    m_pState->dependenceData.pChromatix->dynamic_enable_triggers.abf_enable.hyst_mode,
                    &(m_pState->dependenceData.pChromatix->dynamic_enable_triggers.abf_enable.hyst_trigger),
                    static_cast<VOID*>(&pInputData->triggerData),
                    &m_pState->dependenceData.moduleEnable);

                // Set status to isChanged to avoid calling RunCalculation if the module is disabled
                isChanged = (TRUE == m_moduleEnable) || (TRUE == m_moduleSBPCEnable);
            }
        }
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupISP, "Null Input Pointer");
    }

    return isChanged;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEABF34::CreateCmdList
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFEABF34::CreateCmdList(
    const ISPInputData* pInputData)
{
    CamxResult result       = CamxResultSuccess;
    CmdBuffer* pCmdBuffer   = pInputData->pCmdBuffer;
    UINT32     offset       =
        (m_32bitDMIBufferOffsetDword + (pInputData->pStripeConfig->stripeId * m_32bitDMILength)) * sizeof(UINT32);
    CmdBuffer* pDMIBuffer   = pInputData->p32bitDMIBuffer;
    UINT32     lengthInByte = IFEABF34LUTLengthDWord * sizeof(UINT32);
    UINT32     abfBankSelect = (m_regCmd1.configReg.bitfields.LUT_BANK_SEL == 0) ? ABFBank0 : ABFBank1;

    CAMX_ASSERT(NULL != pCmdBuffer);
    CAMX_ASSERT(NULL != pDMIBuffer);

    result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                          regIFE_IFE_0_VFE_ABF_CFG,
                                          IFEABF34RegLength1DWord,
                                          reinterpret_cast<UINT32*>(&m_regCmd1));

    CAMX_ASSERT(CamxResultSuccess == result);
    result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                          regIFE_IFE_0_VFE_ABF_GR_CFG,
                                          IFEABF34RegLength2DWord,
                                          reinterpret_cast<UINT32*>(&m_regCmd2));

    CAMX_ASSERT(CamxResultSuccess == result);

    result = PacketBuilder::WriteDMI(pCmdBuffer,
                                     regIFE_IFE_0_VFE_DMI_CFG,
                                     static_cast<UINT8>(abfBankSelect),
                                     pDMIBuffer,
                                     offset,
                                     lengthInByte);

    CAMX_ASSERT(CamxResultSuccess == result);

    // Switch LUT Bank select immediately after writing
    m_pState->dependenceData.LUTBankSel ^= 1;

    CAMX_LOG_VERBOSE(CamxLogGroupISP, "m_pState: %p abfBankSelect: %d, LUT_BANK_SEL: %d, LUTBankSel: %d",
        m_pState,
        abfBankSelect,
        m_regCmd1.configReg.bitfields.LUT_BANK_SEL,
        m_pState->dependenceData.LUTBankSel);

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEABF34::PrepareStripingParameters
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFEABF34::PrepareStripingParameters(
    ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;

    if (NULL != pInputData)
    {
        if (NULL != pInputData->pOEMIQSetting)
        {
            m_moduleEnable = (static_cast<OEMIFEIQSetting*>(pInputData->pOEMIQSetting))->ABFEnable;
        }
        else
        {
            TuningDataManager* pTuningManager = pInputData->pTuningDataManager;
            CAMX_ASSERT(pTuningManager != NULL);

            // Search through the tuning data (tree), only when there
            // are changes to the tuning mode data as an optimization
            if ((TRUE == pInputData->tuningModeChanged)    &&
                (TRUE == pTuningManager->IsValidChromatix()))
            {
                CAMX_ASSERT(NULL != pInputData->pTuningData);

                m_pChromatix = pTuningManager->GetChromatix()->GetModule_abf34_ife(
                    reinterpret_cast<TuningMode*>(&pInputData->pTuningData->TuningMode[0]),
                    pInputData->pTuningData->noOfSelectionParameter);
            }

            CAMX_ASSERT(NULL != m_pChromatix);
            if (NULL != m_pChromatix)
            {
                m_moduleEnable = m_pChromatix->enable_section.abf_enable ||
                                 m_pChromatix->enable_section.sbpc_enable;
            }
        }

        if (NULL != pInputData->pStripingInput)
        {
            pInputData->pStripingInput->enableBits.ABF = m_moduleEnable;
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
// IFEABF34::RunCalculation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFEABF34::RunCalculation(
    const ISPInputData* pInputData)
{
    CamxResult      result   = CamxResultSuccess;
    UINT32*         pDMIAddr = pInputData->p32bitDMIBufferAddr;
    ABF34OutputData outputData;

    pDMIAddr += m_32bitDMIBufferOffsetDword + (pInputData->pStripeConfig->stripeId * m_32bitDMILength);

    CAMX_ASSERT(NULL != pDMIAddr);

    outputData.pRegCmd1 = &m_regCmd1;
    outputData.pRegCmd2 = &m_regCmd2;
    outputData.pDMIData = pDMIAddr;

    result = IQInterface::IFEABF34CalculateSetting(&m_pState->dependenceData, pInputData->pOEMIQSetting, &outputData);

    if (CamxResultSuccess != result)
    {
        CAMX_LOG_ERROR(CamxLogGroupISP, " ABF Calculation Failed.");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEABF34::DeallocateCommonLibraryData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IFEABF34::DeallocateCommonLibraryData()
{
    if (NULL != m_pInterpolationData)
    {
        CAMX_FREE(m_pInterpolationData);
        m_pInterpolationData = NULL;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEABF34::IFEABF34
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IFEABF34::IFEABF34()
{
    m_type             = ISPIQModuleType::IFEABF;
    m_cmdLength        = PacketBuilder::RequiredWriteRegRangeSizeInDwords(IFEABF34RegLength1DWord) +
                         PacketBuilder::RequiredWriteRegRangeSizeInDwords(IFEABF34RegLength2DWord) +
                         PacketBuilder::RequiredWriteDMISizeInDwords();

    m_32bitDMILength   = IFEABF34LUTLengthDWord;
    m_64bitDMILength   = 0;
    m_moduleEnable     = TRUE;
    m_moduleSBPCEnable = TRUE;

    m_ABFBankSelect                             = ABFBank0;   ///< By Default, Select BANK 0
    m_regCmd1.configReg.bitfields.LUT_BANK_SEL  = 0;

    m_noiseReductionMode = NoiseReductionModeFast;
    m_noiseReductionMode = 0;
    m_pState             = NULL;
    m_pChromatix         = NULL;
    m_pInterpolationData = NULL;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IFEABF34::~IFEABF34
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IFEABF34::~IFEABF34()
{
    m_pChromatix = NULL;
    DeallocateCommonLibraryData();
}

CAMX_NAMESPACE_END
