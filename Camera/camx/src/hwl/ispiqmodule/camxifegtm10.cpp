////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017-2018 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxifegtm10.cpp
/// @brief CAMXIFEGTM10 class implementation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "camxdefs.h"
#include "camxifegtm10.h"
#include "camxiqinterface.h"
#include "camxnode.h"
#include "camxtuningdatamanager.h"
#include "parametertuningtypes.h"

CAMX_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEGTM10::Create
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFEGTM10::Create(
    IFEModuleCreateData* pCreateData)
{
    CamxResult result = CamxResultSuccess;

    if (NULL != pCreateData)
    {
        IFEGTM10* pModule = CAMX_NEW IFEGTM10;

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
            CAMX_ASSERT_ALWAYS_MESSAGE("Failed to create IFEGTM10 object.");
        }

        pCreateData->pModule = pModule;
    }
    else
    {
        result = CamxResultEInvalidArg;
        CAMX_ASSERT_ALWAYS_MESSAGE("Null Input Data for IFEGTM10 Creation");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEGTM10::Initialize
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFEGTM10::Initialize()
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
// IFEGTM10::AllocateCommonLibraryData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFEGTM10::AllocateCommonLibraryData()
{
    CamxResult result = CamxResultSuccess;

    UINT interpolationSize = (sizeof(gtm_1_0_0::gtm10_rgn_dataType) * (GTM10MaxmiumNonLeafNode + 1));

    if (NULL == m_dependenceData.pInterpolationData)
    {
        // Alloc for gtm_1_0_0::gtm10_rgn_dataType
        m_dependenceData.pInterpolationData = CAMX_CALLOC(interpolationSize);
        if (NULL == m_dependenceData.pInterpolationData)
        {
            result = CamxResultENoMemory;
        }
    }
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEGTM10::Execute
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFEGTM10::Execute(
    ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;

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
                CAMX_ASSERT_ALWAYS_MESSAGE("GTM10 module calculation Failed.");
            }
        }

        if (CamxResultSuccess == result)
        {
            UpdateIFEInternalData(pInputData);
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
// IFEGTM10::UpdateIFEInternalData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IFEGTM10::UpdateIFEInternalData(
    ISPInputData* pInputData)
{
    pInputData->pCalculatedData->moduleEnable.colorProcessingModuleConfig.bits.GTM_EN = m_moduleEnable;
    pInputData->pCalculatedData->percentageOfGTM =
        (NULL == m_pTMCInput.pAdrcOutputData) ? 0 : m_pTMCInput.pAdrcOutputData->gtmPercentage;

    // Post tuning metadata if setting is enabled
    if (NULL != pInputData->pIFETuningMetadata)
    {
        if (NULL != m_pGTMLUTPtr)
        {
            Utils::Memcpy(pInputData->pIFETuningMetadata->IFEDMIData.packedLUT.GTM.LUT,
                          m_pGTMLUTPtr,
                          IFEGTM10DMILengthDword * sizeof(UINT32));
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEGTM10::GetRegCmd
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID* IFEGTM10::GetRegCmd()
{
    return reinterpret_cast<VOID*>(&m_regCmd);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEGTM10::CheckDependenceChange
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL IFEGTM10::CheckDependenceChange(
    ISPInputData* pInputData)
{
    BOOL                            isChanged     = FALSE;
    tmc_1_0_0::chromatix_tmc10Type* ptmcChromatix = NULL;

    if ((NULL != pInputData->pHwContext)     &&
        (NULL != pInputData->pAECUpdateData) &&
        (NULL != pInputData->pAWBUpdateData))
    {
        if (NULL != pInputData->pOEMIQSetting)
        {
            m_moduleEnable = (static_cast<OEMIFEIQSetting*>(pInputData->pOEMIQSetting))->GTMEnable;

            if (TRUE == m_moduleEnable)
            {
                isChanged = TRUE;
            }
        }
        else
        {
            TuningDataManager* pTuningManager = pInputData->pTuningDataManager;
            CAMX_ASSERT(NULL != pTuningManager);

            if (TRUE == pTuningManager->IsValidChromatix())
            {
                CAMX_ASSERT(NULL != pInputData->pTuningData);

                // Search through the tuning data (tree), only when there
                // are changes to the tuning mode data as an optimization
                if (TRUE == pInputData->tuningModeChanged)
                {
                    m_pChromatix  = pTuningManager->GetChromatix()->GetModule_gtm10_ife(
                                        reinterpret_cast<TuningMode*>(&pInputData->pTuningData->TuningMode[0]),
                                        pInputData->pTuningData->noOfSelectionParameter);
                }
                ptmcChromatix = pTuningManager->GetChromatix()->GetModule_tmc10_sw(
                                    reinterpret_cast<TuningMode*>(&pInputData->pTuningData->TuningMode[0]),
                                    pInputData->pTuningData->noOfSelectionParameter);
            }

            CAMX_ASSERT(NULL != ptmcChromatix);
            CAMX_ASSERT(NULL != m_pChromatix);
            if (NULL != m_pChromatix)
            {
                if ((NULL == m_dependenceData.pChromatix) ||
                    (m_pChromatix->SymbolTableID != m_dependenceData.pChromatix->SymbolTableID))
                {
                    m_pTMCInput.pChromatix      = ptmcChromatix;
                    m_dependenceData.pChromatix = m_pChromatix;
                    m_moduleEnable              = m_pChromatix->enable_section.gtm_enable;
                    if (TRUE == m_moduleEnable)
                    {
                        isChanged = TRUE;
                    }
                }
            }
        }

        // Check for manual control
        if ((TonemapModeContrastCurve == pInputData->pHALTagsData->tonemapCurves.tonemapMode) &&
           (0 != pInputData->pHALTagsData->tonemapCurves.curvePoints))
        {
            m_moduleEnable = FALSE;
            isChanged      = FALSE;
        }

        if (TRUE == m_moduleEnable)
        {
            if ((TRUE ==
                IQInterface::s_interpolationTable.GTM10TriggerUpdate(&pInputData->triggerData, &m_dependenceData)) ||
                (TRUE == pInputData->forceTriggerUpdate))
            {
                isChanged = TRUE;
            }

            CAMX_ASSERT(NULL != ptmcChromatix);

            m_pTMCInput.adrcGTMEnable = FALSE;
            if ((NULL  != ptmcChromatix)                                  &&
                (TRUE  == ptmcChromatix->enable_section.adrc_isp_enable)  &&
                (FALSE == pInputData->triggerData.isTMC11Enabled)         &&
                (TRUE  == ptmcChromatix->chromatix_tmc10_reserve.use_gtm))
            {
                isChanged = TRUE;
                m_pTMCInput.adrcGTMEnable = TRUE;
                IQInterface::s_interpolationTable.TMC10TriggerUpdate(&pInputData->triggerData, &m_pTMCInput);

                // Assign memory for parsing ADRC Specific data.
                if (NULL == m_pADRCData)
                {
                    m_pADRCData = CAMX_NEW ADRCData;
                    if (NULL != m_pADRCData)
                    {
                        Utils::Memset(m_pADRCData, 0, sizeof(ADRCData));
                    }
                    else
                    {
                        isChanged                 = FALSE;
                        CAMX_LOG_ERROR(CamxLogGroupISP, "Memory allocation failed for ADRC Specific data");
                    }
                }
                pInputData->triggerData.pADRCData = m_pADRCData;
                CAMX_LOG_VERBOSE(CamxLogGroupISP, "IFE GTM : using TMC 1.0");
            }
            else
            {
                if (NULL != pInputData->triggerData.pADRCData)
                {
                    m_pTMCInput.adrcGTMEnable = pInputData->triggerData.pADRCData->gtmEnable;
                    CAMX_LOG_VERBOSE(CamxLogGroupISP, "IFE GTM : using TMC 1.1");
                }
            }

            m_pTMCInput.pAdrcOutputData = pInputData->triggerData.pADRCData;
        }

        if ((ControlAWBLockOn == pInputData->pHALTagsData->controlAWBLock) &&
            (ControlAELockOn  == pInputData->pHALTagsData->controlAECLock))
        {
            isChanged = FALSE;
        }

    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupISP, "Null Input Pointer");
    }

    return isChanged;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEGTM10::CreateCmdList
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFEGTM10::CreateCmdList(
    const ISPInputData* pInputData)
{
    CamxResult result       = CamxResultSuccess;
    CmdBuffer* pCmdBuffer   = pInputData->pCmdBuffer;
    UINT32     offset       = m_64bitDMIBufferOffsetDword * sizeof(UINT32);
    CmdBuffer* pDMIBuffer   = pInputData->p64bitDMIBuffer;
    UINT32     lengthInByte = IFEGTM10DMILengthDword * sizeof(UINT32);

    CAMX_ASSERT(NULL != pCmdBuffer);
    CAMX_ASSERT(NULL != pDMIBuffer);

    result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                          regIFE_IFE_0_VFE_GTM_CFG,
                                          IFEGTM10RegLengthDword,
                                          reinterpret_cast<UINT32*>(&m_regCmd));

    CAMX_ASSERT(CamxResultSuccess == result);

    result = PacketBuilder::WriteDMI(pCmdBuffer,
                                     regIFE_IFE_0_VFE_DMI_CFG,
                                     m_bankSelect,
                                     pDMIBuffer,
                                     offset,
                                     lengthInByte);

    CAMX_ASSERT(CamxResultSuccess == result);

    // Switch LUT Bank select immediately after writing
    m_bankSelect = (m_bankSelect == GTMLUTRawBank0) ? GTMLUTRawBank1 : GTMLUTRawBank0;

    m_regCmd.configRegister.bitfields.LUT_BANK_SEL = (m_bankSelect == GTMLUTRawBank0) ? 0x0 : 0x1;

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEGTM10::RunCalculation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFEGTM10::RunCalculation(
    const ISPInputData* pInputData)
{
    CamxResult      result      = CamxResultSuccess;
    UINT32*         pDMIAddr    = NULL;
    GTM10OutputData outputData;

    if (NULL != pInputData->p64bitDMIBufferAddr)
    {
        pDMIAddr = pInputData->p64bitDMIBufferAddr + m_64bitDMIBufferOffsetDword;

        CAMX_ASSERT(NULL != pDMIAddr);
        /// @todo (CAMX-1791) Revisit this function to see if it needs to act differently in the Dual IFE case

        outputData.type                   = PipelineType::IFE;
        outputData.regCmd.IFE.pRegCmd     = &m_regCmd;
        outputData.regCmd.IFE.pDMIDataPtr = reinterpret_cast<UINT64*>(pDMIAddr);
        outputData.registerBETEn = pInputData->registerBETEn;
        m_dependenceData.LUTBankSel       = m_regCmd.configRegister.bitfields.LUT_BANK_SEL;

        result = IQInterface::GTM10CalculateSetting(&m_dependenceData, pInputData->pOEMIQSetting, &outputData, &m_pTMCInput);

        if (CamxResultSuccess == result)
        {
            if (NULL != pInputData->pIFETuningMetadata)
            {
                m_pGTMLUTPtr = outputData.regCmd.IFE.pDMIDataPtr;
            }
        }
        else
        {
            m_pGTMLUTPtr = NULL;
            CAMX_LOG_ERROR(CamxLogGroupISP, "GTM10 Calculation Failed. result %d", result);
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
// IFEGTM10::DeallocateCommonLibraryData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IFEGTM10::DeallocateCommonLibraryData()
{
    if (NULL != m_dependenceData.pInterpolationData)
    {
        CAMX_FREE(m_dependenceData.pInterpolationData);
        m_dependenceData.pInterpolationData = NULL;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEGTM10::IFEGTM10
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IFEGTM10::IFEGTM10()
{
    m_type           = ISPIQModuleType::IFEGTM;
    m_32bitDMILength = 0;
    m_64bitDMILength = IFEGTM10DMILengthDword;
    m_cmdLength      = PacketBuilder::RequiredWriteRegRangeSizeInDwords(IFEGTM10RegLengthDword) +
                       PacketBuilder::RequiredWriteDMISizeInDwords();
    m_moduleEnable   = TRUE;
    m_bankSelect     = GTMLUTRawBank0; ///< By default set to Bank 0
    m_pGTMLUTPtr     = NULL;
    m_pChromatix     = NULL;
    m_pADRCData      = NULL;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IFEGTM10::~IFEGTM10
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IFEGTM10::~IFEGTM10()
{
    CAMX_FREE(m_pADRCData);

    m_pChromatix = NULL;
    DeallocateCommonLibraryData();
}

CAMX_NAMESPACE_END
