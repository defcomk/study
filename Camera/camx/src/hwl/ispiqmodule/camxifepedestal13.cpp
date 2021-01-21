////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017-2018 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxifepedestal13.cpp
/// @brief CAMXIFEPEDESTAL13 class implementation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "camxdefs.h"
#include "camxifepedestal13.h"
#include "camxiqinterface.h"
#include "camxnode.h"
#include "camxtuningdatamanager.h"
#include "parametertuningtypes.h"
#include "pedestal_1_3_0.h"

CAMX_NAMESPACE_BEGIN

/// @brief Value for select DMI BANK
static const UINT PedestalLGRRBank0 = IFE_IFE_0_VFE_DMI_CFG_DMI_RAM_SEL_PEDESTAL_RAM_L_GR_R_BANK0;
static const UINT PedestalLGRRBank1 = IFE_IFE_0_VFE_DMI_CFG_DMI_RAM_SEL_PEDESTAL_RAM_L_GR_R_BANK1;
static const UINT PedestalLGBBBank0 = IFE_IFE_0_VFE_DMI_CFG_DMI_RAM_SEL_PEDESTAL_RAM_L_GB_B_BANK0;
static const UINT PedestalLGBBBank1 = IFE_IFE_0_VFE_DMI_CFG_DMI_RAM_SEL_PEDESTAL_RAM_L_GB_B_BANK1;

static const UINT32 IFEPedestal13RegLengthDword  = (sizeof(IFEPedestal13RegCmd) / sizeof(UINT32));
static const UINT8  IFEPedestal13NumDMITables    = 2;
static const UINT32 IFEPedestal13DMISetSizeDword = 130;   // DMI LUT table has 130 entries
static const UINT32 IFEPedestal13LUTTableSize    = IFEPedestal13DMISetSizeDword * sizeof(UINT32);
static const UINT32 IFEPedestal13DMILengthDword  = (IFEPedestal13DMISetSizeDword * IFEPedestal13NumDMITables);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEPedestal13::Create
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFEPedestal13::Create(
    IFEModuleCreateData* pCreateData)
{
    CamxResult result = CamxResultSuccess;

    if (NULL != pCreateData)
    {
        IFEPedestal13* pModule = CAMX_NEW IFEPedestal13;

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
            CAMX_ASSERT_ALWAYS_MESSAGE("Failed to create IFEPedestal13 object.");
        }

        pCreateData->pModule = pModule;
    }
    else
    {
        result = CamxResultEInvalidArg;
        CAMX_ASSERT_ALWAYS_MESSAGE("Null Input Data for IFEPedestal13 Creation");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEPedestal13::Initialize
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFEPedestal13::Initialize()
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
// IFEPedestal13::AllocateCommonLibraryData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFEPedestal13::AllocateCommonLibraryData()
{
    CamxResult result = CamxResultSuccess;

    UINT interpolationSize = (sizeof(pedestal_1_3_0::pedestal13_rgn_dataType) * (Pedestal13MaxmiumNonLeafNode + 1));

    if (NULL == m_dependenceData.pInterpolationData)
    {
        // Alloc for pedestal_1_3_0::pedestal13_rgn_dataType
        m_dependenceData.pInterpolationData = CAMX_CALLOC(interpolationSize);
        if (NULL == m_dependenceData.pInterpolationData)
        {
            result = CamxResultENoMemory;
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEPedestal13::Execute
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFEPedestal13::Execute(
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
        }

        if (CamxResultSuccess == result)
        {
            UpdateIFEInternalData(pInputData);
        }
        else
        {
            CAMX_ASSERT_ALWAYS_MESSAGE("Operation failed %d", result);
        }
    }
    else
    {
        result = CamxResultEInvalidArg;
        CAMX_LOG_ERROR(CamxLogGroupISP, "Invalid Input: pInputData %p", pInputData);
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEPedestal13::UpdateIFEInternalData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IFEPedestal13::UpdateIFEInternalData(
    ISPInputData* pInputData)
{
    if (NULL != pInputData->pCalculatedData)
    {
        pInputData->pCalculatedData->moduleEnable.lensProcessingModuleConfig.bitfields.PEDESTAL_EN = m_moduleEnable;

        pInputData->pCalculatedData->blackLevelLock = m_blacklevelLock;
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupISP, "Update Metadata failed");
    }
    if (NULL != pInputData->pStripingInput)
    {
        pInputData->pStripingInput->enableBits.pedestal = m_moduleEnable;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEPedestal13::GetRegCmd
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID* IFEPedestal13::GetRegCmd()
{
    return reinterpret_cast<VOID*>(&m_regCmd);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEPedestal13::CheckDependenceChange
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL IFEPedestal13::CheckDependenceChange(
    ISPInputData* pInputData)
{
    BOOL isChanged = FALSE;

    if ((NULL != pInputData->pAECUpdateData) &&
        (NULL != pInputData->pHwContext)     &&
        (NULL != pInputData->pAWBUpdateData) &&
        (NULL != pInputData->pHALTagsData))
    {
        m_blacklevelLock = pInputData->pHALTagsData->blackLevelLock;
        if (BlackLevelLockOn == m_blacklevelLock)
        {
            isChanged = FALSE;
        }
        else
        {
            if (NULL != pInputData->pOEMIQSetting)
            {
                m_moduleEnable = (static_cast<OEMIFEIQSetting*>(pInputData->pOEMIQSetting))->PedestalEnable;

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

                    m_pChromatix = pTuningManager->GetChromatix()->GetModule_pedestal13_ife(
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
                        m_moduleEnable              = m_pChromatix->enable_section.pedestal_enable;
                        if (TRUE == m_moduleEnable)
                        {
                            isChanged = TRUE;
                        }
                    }
                }
            }

            if (TRUE == m_moduleEnable)
            {
                if ((TRUE ==
                    IQInterface::s_interpolationTable.pedestal13TriggerUpdate(&pInputData->triggerData, &m_dependenceData)) ||
                    (TRUE == pInputData->forceTriggerUpdate))
                {
                    isChanged = TRUE;
                }
            }
        }
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupISP,
                       "Invalid Input: pAECUpdateData %p  pHwContext %p pNewAWBUpdate %p",
                       pInputData->pAECUpdateData,
                       pInputData->pHwContext,
                       pInputData->pAWBUpdateData);
    }

    return isChanged;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEPedestal13::PrepareStripingParameters
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFEPedestal13::PrepareStripingParameters(
    ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;
    /// @todo (CAMX-1308) This is a reduced version of Execute. Revisit when things settle to see if they should be merged.
    if (NULL != pInputData)
    {
        if (TRUE == CheckDependenceChange(pInputData))
        {
            result = RunCalculation(pInputData);

            if (CamxResultSuccess == result)
            {
                UpdateIFEInternalData(pInputData);
            }
            else
            {
                CAMX_ASSERT_ALWAYS_MESSAGE("LSC module calculation Failed.");
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
// IFEPedestal13::CreateCmdList
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFEPedestal13::CreateCmdList(
    const ISPInputData* pInputData)
{
    CamxResult result       = CamxResultSuccess;
    CmdBuffer* pCmdBuffer   = pInputData->pCmdBuffer;
    UINT32     offset       =
        (m_32bitDMIBufferOffsetDword + (pInputData->pStripeConfig->stripeId * m_32bitDMILength)) * sizeof(UINT32);
    CmdBuffer* pDMIBuffer   = pInputData->p32bitDMIBuffer;
    UINT32     lengthInByte = IFEPedestal13DMISetSizeDword * sizeof(UINT32);

    CAMX_ASSERT(NULL != pCmdBuffer);
    CAMX_ASSERT(NULL != pDMIBuffer);

    result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                          regIFE_IFE_0_VFE_PEDESTAL_CFG,
                                          IFEPedestal13RegLengthDword,
                                          reinterpret_cast<UINT32*>(&m_regCmd));

    CAMX_ASSERT(CamxResultSuccess == result);

    result = PacketBuilder::WriteDMI(pCmdBuffer,
                                     regIFE_IFE_0_VFE_DMI_CFG,
                                     m_leftGRRBankSelect,
                                     pDMIBuffer,
                                     offset,
                                     lengthInByte);

    CAMX_ASSERT(CamxResultSuccess == result);

    offset   += lengthInByte;
    result = PacketBuilder::WriteDMI(pCmdBuffer,
                                     regIFE_IFE_0_VFE_DMI_CFG,
                                     m_leftGBBBankSelect ,
                                     pDMIBuffer,
                                     offset,
                                     lengthInByte);
    CAMX_ASSERT(CamxResultSuccess == result);

    m_leftGRRBankSelect = (m_leftGRRBankSelect == PedestalLGRRBank0) ?  PedestalLGRRBank1 : PedestalLGRRBank0;
    m_leftGBBBankSelect = (m_leftGBBBankSelect == PedestalLGBBBank0) ?  PedestalLGBBBank1 : PedestalLGBBBank0;

    m_dependenceData.LUTBankSel ^= 1;

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEPedestal13::RunCalculation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFEPedestal13::RunCalculation(
    const ISPInputData* pInputData)
{
    CamxResult           result           = CamxResultSuccess;
    UINT32*              pPedestalDMIAddr = reinterpret_cast<UINT32*>(pInputData->p32bitDMIBufferAddr +
                                                                      m_32bitDMIBufferOffsetDword +
                                                                      (pInputData->pStripeConfig->stripeId * m_32bitDMILength));
    Pedestal13OutputData outputData;

    outputData.type                  = PipelineType::IFE;
    outputData.regCommand.pRegIFECmd = &m_regCmd;
    outputData.pGRRLUTDMIBuffer      = pPedestalDMIAddr;
    outputData.pGBBLUTDMIBuffer      = reinterpret_cast<UINT32*>((reinterpret_cast<UCHAR*>(outputData.pGRRLUTDMIBuffer) +
                                                                  IFEPedestal13LUTTableSize));

    result = IQInterface::Pedestal13CalculateSetting(&m_dependenceData, pInputData->pOEMIQSetting, &outputData);

    if (CamxResultSuccess != result)
    {
        CAMX_LOG_ERROR(CamxLogGroupISP, "Pedestal Calculation Failed.");
    }
    if (NULL != pInputData->pStripingInput)
    {
        pInputData->pStripingInput->enableBits.pedestal                          = m_moduleEnable;
        pInputData->pStripingInput->stripingInput.pedestalParam.enable           = m_moduleEnable;
        pInputData->pStripingInput->stripingInput.pedestalParam.Bwidth_l         = outputData.pedState.bwidth_l;
        pInputData->pStripingInput->stripingInput.pedestalParam.Bx_d1_l          = outputData.pedState.bx_d1_l;
        pInputData->pStripingInput->stripingInput.pedestalParam.Bx_start_l       = outputData.pedState.bx_start_l;
        pInputData->pStripingInput->stripingInput.pedestalParam.Lx_start_l       = outputData.pedState.lx_start_l;
        pInputData->pStripingInput->stripingInput.pedestalParam.MeshGridBwidth_l = outputData.pedState.meshGridBwidth_l;
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEPedestal13::DeallocateCommonLibraryData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IFEPedestal13::DeallocateCommonLibraryData()
{
    if (NULL != m_dependenceData.pInterpolationData)
    {
        CAMX_FREE(m_dependenceData.pInterpolationData);
        m_dependenceData.pInterpolationData = NULL;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEPedestal13::IFEPedestal13
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IFEPedestal13::IFEPedestal13()
{
    m_type              = ISPIQModuleType::IFEPedestalCorrection;
    m_32bitDMILength    = IFEPedestal13DMILengthDword;
    m_cmdLength         = PacketBuilder::RequiredWriteRegRangeSizeInDwords(IFEPedestal13RegLengthDword) +
                              (PacketBuilder::RequiredWriteDMISizeInDwords() * IFEPedestal13NumDMITables);
    m_moduleEnable      = TRUE;
    m_pChromatix        = NULL;
    m_leftGRRBankSelect = PedestalLGRRBank0; ///< By Default, Select BANK 0
    m_leftGBBBankSelect = PedestalLGBBBank0; ///< By Default, Select BANK 0
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IFEPedestal13::~IFEPedestal13
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IFEPedestal13::~IFEPedestal13()
{
    m_pChromatix = NULL;
    DeallocateCommonLibraryData();
}

CAMX_NAMESPACE_END
