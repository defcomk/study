////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017-2018 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxifelinearization33.cpp
/// @brief CAMXIFELinearization33 class implementation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "camxdefs.h"
#include "camxifelinearization33.h"
#include "camxiqinterface.h"
#include "camxnode.h"
#include "camxtuningdatamanager.h"
#include "linearization_3_3_0.h"
#include "parametertuningtypes.h"

CAMX_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFELinearization33::Create
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFELinearization33::Create(
    IFEModuleCreateData* pCreateData)
{
    CamxResult result = CamxResultSuccess;

    if (NULL != pCreateData)
    {
        IFELinearization33* pModule = CAMX_NEW IFELinearization33;

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
            CAMX_ASSERT_ALWAYS_MESSAGE("Failed to create IFELinearization33 object.");
        }

        pCreateData->pModule = pModule;
    }
    else
    {
        result = CamxResultEInvalidArg;
        CAMX_ASSERT_ALWAYS_MESSAGE("Null Input Data for IFELinearization33 Creation");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFELinearization33::Initialize
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFELinearization33::Initialize()
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
// IFELinearization33::Execute
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFELinearization33::Execute(
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
                CAMX_ASSERT_ALWAYS_MESSAGE("Linearization module calculation Failed.");
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
        result = CamxResultEFailed;
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFELinearization33::UpdateIFEInternalData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IFELinearization33::UpdateIFEInternalData(
    ISPInputData* pInputData)
{
    // Update metdata
    pInputData->pCalculatedData->blackLevelLock                    = m_blacklevelLock;
    pInputData->pCalculatedData->linearizationAppliedBlackLevel[0] = m_dynamicBlackLevel[0];
    pInputData->pCalculatedData->linearizationAppliedBlackLevel[1] = m_dynamicBlackLevel[1];
    pInputData->pCalculatedData->linearizationAppliedBlackLevel[2] = m_dynamicBlackLevel[2];
    pInputData->pCalculatedData->linearizationAppliedBlackLevel[3] = m_dynamicBlackLevel[3];

    // Update module enable info
    pInputData->pCalculatedData->moduleEnable.lensProcessingModuleConfig.bits.BLACK_EN  = m_moduleEnable;

    // Update stretch Gains
    pInputData->pCalculatedData->stretchGainBlue      = m_stretchGainBlue;
    pInputData->pCalculatedData->stretchGainGreenEven = m_stretchGainGreenEven;
    pInputData->pCalculatedData->stretchGainGreenOdd  = m_stretchGainGreenOdd;
    pInputData->pCalculatedData->stretchGainRed       = m_stretchGainRed;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFELinearization33::AllocateCommonLibraryData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFELinearization33::AllocateCommonLibraryData()
{
    CamxResult result = CamxResultSuccess;

    UINT interpolationSize = (sizeof(linearization_3_3_0::linearization33_rgn_dataType) *
                             (Linearization33MaxmiumNonLeafNode + 1));

    if (NULL == m_dependenceData.pInterpolationData)
    {
        // Alloc for linearization_3_3_0::linearization33_rgn_dataType
        m_dependenceData.pInterpolationData = CAMX_CALLOC(interpolationSize);
        if (NULL == m_dependenceData.pInterpolationData)
        {
            result = CamxResultENoMemory;
        }
    }
    return result;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFELinearization33::GetRegCmd
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID* IFELinearization33::GetRegCmd()
{
    return static_cast<VOID*>(&m_regCmd);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFELinearization33::CheckDependenceChange
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL IFELinearization33::CheckDependenceChange(
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
            m_moduleEnable = (static_cast<OEMIFEIQSetting*>(pInputData->pOEMIQSetting))->LinearizationEnable;
            m_dependenceData.pedestalEnable = (static_cast<OEMIFEIQSetting*>(pInputData->pOEMIQSetting))->PedestalEnable;

            if (TRUE == m_moduleEnable)
            {
                isChanged = TRUE;
            }
        }
        else
        {
            m_blacklevelLock = pInputData->pHALTagsData->blackLevelLock;
            m_AWBLock        = pInputData->pHALTagsData->controlAWBLock;

            if (((pInputData->pHALTagsData->blackLevelLock == m_blacklevelLock) &&
                 (pInputData->pHALTagsData->blackLevelLock == BlackLevelLockOn)) ||
                ((pInputData->pHALTagsData->controlAWBLock == m_AWBLock) &&
                 (pInputData->pHALTagsData->controlAWBLock == ControlAWBLockOn)))
            {
                isChanged = FALSE;
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

                    m_pChromatix = pTuningManager->GetChromatix()->GetModule_linearization33_ife(
                        reinterpret_cast<TuningMode*>(&pInputData->pTuningData->TuningMode[0]),
                        pInputData->pTuningData->noOfSelectionParameter);
                }

                CAMX_ASSERT(NULL != m_pChromatix);
                if (NULL != m_pChromatix)
                {
                    if ((NULL == m_dependenceData.pChromatix) ||
                        (m_pChromatix->SymbolTableID != m_dependenceData.pChromatix->SymbolTableID))
                    {
                        m_dependenceData.pChromatix      = m_pChromatix;
                        m_moduleEnable                   = m_pChromatix->enable_section.linearization_enable;
                        m_dependenceData.pedestalEnable  =
                            pInputData->pCalculatedData->moduleEnable.lensProcessingModuleConfig.bitfields.PEDESTAL_EN;
                        m_dependenceData.symbolIDChanged = TRUE;

                        if (TRUE == m_moduleEnable)
                        {
                            isChanged = TRUE;
                        }
                    }
                }
                if (TRUE == m_moduleEnable)
                {
                    m_dependenceData.pedestalEnable =
                       pInputData->pCalculatedData->moduleEnable.lensProcessingModuleConfig.bits.PEDESTAL_EN;

                    if ((TRUE == IQInterface::s_interpolationTable.IFELinearization33TriggerUpdate(&pInputData->triggerData,
                                                                                                  &m_dependenceData)) ||
                        (TRUE == pInputData->forceTriggerUpdate))
                    {
                        isChanged = TRUE;
                    }
                }
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
// IFELinearization33::CreateCmdList
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFELinearization33::CreateCmdList(
    ISPInputData* pInputData)
{
    CamxResult result        = CamxResultSuccess;
    CmdBuffer* pCmdBuffer    = NULL;
    UINT32     offset        = m_64bitDMIBufferOffsetDword * sizeof(UINT32);
    CmdBuffer* pDMIBuffer    = NULL;
    UINT32     lengthInByte  = IFELinearization33DMILengthDWord * sizeof(UINT32);
    UINT8      bankSelect    = 0;

    if (NULL != pInputData                  &&
        NULL != pInputData->pCmdBuffer      &&
        NULL != pInputData->p64bitDMIBuffer &&
        NULL != pInputData->p64bitDMIBufferAddr)
    {
        pCmdBuffer = pInputData->pCmdBuffer;
        pDMIBuffer = pInputData->p64bitDMIBuffer;

        result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                              regIFE_IFE_0_VFE_BLACK_CFG,
                                              IFELinearization33RegLengthDWord,
                                              reinterpret_cast<UINT32*>(&m_regCmd));

        CAMX_ASSERT(CamxResultSuccess == result);

        bankSelect = (m_regCmd.configRegister.bitfields.LUT_BANK_SEL == 0) ? BlackLUTBank0 : BlackLUTBank1;

        result = PacketBuilder::WriteDMI(pCmdBuffer,
                                         regIFE_IFE_0_VFE_DMI_CFG,
                                         bankSelect,
                                         pDMIBuffer,
                                         offset,
                                         lengthInByte);

        CAMX_ASSERT(CamxResultSuccess == result);

        m_bankSelect ^= 1;
    }
    else
    {
        CAMX_ASSERT_ALWAYS_MESSAGE("Null Input Data");
        result = CamxResultEFailed;
    }
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFELinearization33::RunCalculation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFELinearization33::RunCalculation(
    const ISPInputData* pInputData)
{
    CamxResult                result     = CamxResultSuccess;
    UINT32*                   pDMIAddr   = NULL;
    Linearization33OutputData outputData = {NULL, NULL, 0.f, 0.f, 0.f, 0.f};

    if (NULL != pInputData && pInputData->p64bitDMIBufferAddr)
    {
        pDMIAddr = pInputData->p64bitDMIBufferAddr + m_64bitDMIBufferOffsetDword;

        CAMX_ASSERT(NULL != pDMIAddr);
        /// @todo (CAMX-1791) Revisit this function to see if it needs to act differently in the Dual IFE case

        outputData.pRegCmd          = &m_regCmd;
        outputData.pDMIDataPtr      = reinterpret_cast<UINT64*>(pDMIAddr);
        outputData.registerBETEn    = pInputData->registerBETEn;
        m_dependenceData.lutBankSel = m_bankSelect;

        result = IQInterface::IFELinearization33CalculateSetting(&m_dependenceData, pInputData->pOEMIQSetting, &outputData);

        m_stretchGainBlue      = outputData.stretchGainBlue;
        m_stretchGainRed       = outputData.stretchGainRed;
        m_stretchGainGreenEven = outputData.stretchGainGreenEven;
        m_stretchGainGreenOdd  = outputData.stretchGainGreenOdd;


        m_dynamicBlackLevel[0] = outputData.pRegCmd->interpolationR0Register.bitfields.LUT_P0 ;
        m_dynamicBlackLevel[1] = outputData.pRegCmd->interpolationGR0Register.bitfields.LUT_P0;
        m_dynamicBlackLevel[2] = outputData.pRegCmd->interpolationGB0Register.bitfields.LUT_P0;
        m_dynamicBlackLevel[3] = outputData.pRegCmd->interpolationB0Register.bitfields.LUT_P0;

        if (CamxResultSuccess != result)
        {
            CAMX_LOG_ERROR(CamxLogGroupISP, "Linearization Calculation Failed. result %d", result);
        }
    }
    else
    {
        CAMX_ASSERT_ALWAYS_MESSAGE("Null Input Data");
        result = CamxResultEFailed;
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFELinearization33::DeallocateCommonLibraryData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IFELinearization33::DeallocateCommonLibraryData()
{
    if (NULL != m_dependenceData.pInterpolationData)
    {
        CAMX_FREE(m_dependenceData.pInterpolationData);
        m_dependenceData.pInterpolationData = NULL;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFELinearization33::IFELinearization33
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IFELinearization33::IFELinearization33()
{
    m_type           = ISPIQModuleType::IFELinearization;
    m_64bitDMILength = IFELinearization33DMILengthDWord;
    m_cmdLength      = PacketBuilder::RequiredWriteRegRangeSizeInDwords(IFELinearization33RegLengthDWord) +
                       PacketBuilder::RequiredWriteDMISizeInDwords();
    m_moduleEnable   = TRUE;
    m_bankSelect     = 0;
    m_pChromatix     = NULL;
    m_AWBLock        = ControlAWBLockOff;
    m_blacklevelLock = BlackLevelLockOff;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IFELinearization33::~IFELinearization33
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IFELinearization33::~IFELinearization33()
{
    m_pChromatix = NULL;
    DeallocateCommonLibraryData();
}

CAMX_NAMESPACE_END
