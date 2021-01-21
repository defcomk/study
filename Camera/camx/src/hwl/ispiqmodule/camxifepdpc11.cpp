////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017-2018 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxifepdpc11.cpp
/// @brief CAMXIFEPDPC11 class implementation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "camxdefs.h"
#include "camxifepdpc11.h"
#include "camxiqinterface.h"
#include "camxnode.h"
#include "camxtuningdatamanager.h"
#include "ifepdpc11setting.h"
#include "parametertuningtypes.h"
#include "pdpc_1_1_0.h"


CAMX_NAMESPACE_BEGIN

static const UINT32 PDAFLUT                 = IFE_IFE_0_VFE_DMI_CFG_DMI_RAM_SEL_PDAF_LUT;
static const UINT32 IFEPDPC11RegLengthDword = sizeof(IFEPDPC11RegCmd) / sizeof(UINT32);
static const UINT32 IFEPDPC11DMILengthDword = DMIRAM_PDAF_LUT_LENGTH;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEPDPC11::Create
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFEPDPC11::Create(
    IFEModuleCreateData* pCreateData)
{
    CamxResult result = CamxResultSuccess;

    if (NULL != pCreateData)
    {
        IFEPDPC11* pModule = CAMX_NEW IFEPDPC11;

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
            CAMX_ASSERT_ALWAYS_MESSAGE("Failed to create IFEPDPC11 object.");
        }

        pCreateData->pModule = pModule;
    }
    else
    {
        result = CamxResultEInvalidArg;
        CAMX_ASSERT_ALWAYS_MESSAGE("Null Input Data for IFEPDPC11 Creation");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEPDPC11::Initialize
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFEPDPC11::Initialize()
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
// IFEPDPC11::Execute
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFEPDPC11::Execute(
    ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;

    if ((NULL != pInputData)                  &&
        (NULL != pInputData->pCmdBuffer)      &&
        (NULL != pInputData->p32bitDMIBuffer) &&
        (NULL != pInputData->p32bitDMIBufferAddr))
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
// IFEPDPC11::AllocateCommonLibraryData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFEPDPC11::AllocateCommonLibraryData()
{
    CamxResult result = CamxResultSuccess;

    UINT interpolationSize = (sizeof(pdpc_1_1_0::pdpc11_rgn_dataType) * (CC12MaxmiumNonLeafNode + 1));

    if (NULL == m_dependenceData.pInterpolationData)
    {
        // Alloc for pdpc_1_1_0::pdpc11_rgn_dataType
        m_dependenceData.pInterpolationData = CAMX_CALLOC(interpolationSize);
        if (NULL == m_dependenceData.pInterpolationData)
        {
            result = CamxResultENoMemory;
        }
    }
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEPDPC11::PrepareStripingParameters
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFEPDPC11::PrepareStripingParameters(
    ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;

    if (NULL != pInputData)
    {
        if (TRUE == CheckDependenceChange(pInputData))
        {
            result = RunCalculation(pInputData);
            if (CamxResultSuccess != result)
            {
                CAMX_ASSERT_ALWAYS_MESSAGE("Linearization module calculation Failed.");
            }
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
// IFEPDPC11::UpdateIFEInternalData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IFEPDPC11::UpdateIFEInternalData(
    ISPInputData* pInputData)
{
    pInputData->pCalculatedData->moduleEnable.lensProcessingModuleConfig.bits.PDAF_EN =  m_moduleEnable;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEPDPC11::GetRegCmd
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID* IFEPDPC11::GetRegCmd()
{
    return reinterpret_cast<VOID*>(&m_regCmd);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEPDPC11::CheckDependenceChange
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL IFEPDPC11::CheckDependenceChange(
    ISPInputData* pInputData)
{
    BOOL isChanged = FALSE;

    if ((NULL != pInputData)                 &&
        (NULL != pInputData->pHwContext)     &&
        (NULL != pInputData->pAECUpdateData))
    {
        if (NULL != pInputData->pOEMIQSetting)
        {
            m_moduleEnable = (static_cast<OEMIFEIQSetting*>(pInputData->pOEMIQSetting))->PDPCEnable;

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

                m_pChromatix = pTuningManager->GetChromatix()->GetModule_pdpc11_ife(
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
                    m_moduleEnable              = ((TRUE == m_pChromatix->enable_section.pdpc_enable) &&
                                                   (pInputData->sensorData.sensorPDAFInfo.PDAFPixelCount > 0));
                    if (TRUE == m_moduleEnable)
                    {
                        isChanged = TRUE;
                    }
                }
            }
        }

        // Dependence data still needs to be updated even in the OEM setting case
        if (TRUE == m_moduleEnable)
        {
            /// @todo (CAMX-1207) PDAF datas need to send from the sensor. This will send at the start.
            /// Will update TRUE if the PDAF data available.
            BOOL PDAFDataAvailable = FALSE;

            if ((TRUE ==
                IQInterface::s_interpolationTable.IFEPDPC11TriggerUpdate(&pInputData->triggerData, &m_dependenceData)) ||
                (TRUE == pInputData->forceTriggerUpdate))
            {
                isChanged = TRUE;
            }

            if (NULL != m_pChromatix)
            {
                PDAFDataAvailable = ((TRUE == m_pChromatix->enable_section.pdpc_enable) ||
                                     (TRUE == m_pChromatix->enable_section.dsbpc_enable));
            }

            // PDAF data is fixed, and expected to comes only during the stream start
            if ((TRUE == m_moduleEnable) && (TRUE == PDAFDataAvailable))
            {
                CamxResult result      = CamxResultSuccess;
                UINT32     inputWidth  = 0;
                UINT32     inputHeight = 0;

                /// @todo (CAMX-1207) Sensor need to send PDAF dimensions info.
                /// remove this hardcoding after that.
                if (TRUE == pInputData->HVXData.DSEnabled)
                {
                    inputWidth  = pInputData->HVXData.HVXOut.width;
                    inputHeight = pInputData->HVXData.HVXOut.height;
                }
                else
                {
                    inputWidth  = pInputData->sensorData.sensorOut.width;
                    inputHeight = pInputData->sensorData.sensorOut.height;
                }
                result = IQInterface::IFEGetSensorMode(&pInputData->sensorData.format, &m_dependenceData.sensorType);
                if (CamxResultSuccess == result)
                {
                    m_dependenceData.imageWidth        = inputWidth;
                    m_dependenceData.imageHeight       = inputHeight;
                    m_dependenceData.PDAFBlockWidth    = pInputData->sensorData.sensorPDAFInfo.PDAFBlockWidth;
                    m_dependenceData.PDAFBlockHeight   = pInputData->sensorData.sensorPDAFInfo.PDAFBlockHeight;
                    m_dependenceData.PDAFGlobaloffsetX =
                        static_cast<UINT16>(pInputData->sensorData.sensorPDAFInfo.PDAFGlobaloffsetX);
                    m_dependenceData.PDAFGlobaloffsetY =
                        static_cast<UINT16>(pInputData->sensorData.sensorPDAFInfo.PDAFGlobaloffsetY);
                    m_dependenceData.PDAFPixelCount    = pInputData->sensorData.sensorPDAFInfo.PDAFPixelCount;

                    Utils::Memcpy(m_dependenceData.PDAFPixelCoords,
                                  &pInputData->sensorData.sensorPDAFInfo.PDAFPixelCoords,
                                  sizeof(PDPixelCoordinates) * m_dependenceData.PDAFPixelCount);

                    isChanged         = TRUE;
                    PDAFDataAvailable = FALSE;
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
// IFEPDPC11::CreateCmdList
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFEPDPC11::CreateCmdList(
    const ISPInputData* pInputData)
{
    CamxResult result       = CamxResultSuccess;
    CmdBuffer* pCmdBuffer   = pInputData->pCmdBuffer;
    UINT32     offset       =
        (m_32bitDMIBufferOffsetDword + (pInputData->pStripeConfig->stripeId * m_32bitDMILength)) * sizeof(UINT32);
    CmdBuffer* pDMIBuffer   = pInputData->p32bitDMIBuffer;
    UINT32     lengthInByte = IFEPDPC11DMILengthDword * sizeof(UINT32);

    CAMX_ASSERT(NULL != pCmdBuffer);
    CAMX_ASSERT(NULL != pDMIBuffer);

    result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                          regIFE_IFE_0_VFE_PDAF_CFG,
                                          IFEPDPC11RegLengthDword,
                                          reinterpret_cast<UINT32*>(&m_regCmd));

    CAMX_ASSERT(CamxResultSuccess == result);

    if (FALSE == m_isLUTLoaded)
    {
        result = PacketBuilder::WriteDMI(pCmdBuffer,
                                         regIFE_IFE_0_VFE_DMI_CFG,
                                         PDAFLUT,
                                         pDMIBuffer,
                                         offset,
                                         lengthInByte);

        if (CamxResultSuccess == result)
        {
            m_isLUTLoaded = TRUE;
        }
        else
        {
            CAMX_ASSERT_ALWAYS_MESSAGE("Failed to program DMI buffer");
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEPDPC11::RunCalculation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFEPDPC11::RunCalculation(
    const ISPInputData* pInputData)
{
    CamxResult       result     = CamxResultSuccess;
    UINT32*          pDMIAddr   = pInputData->p32bitDMIBufferAddr;
    PDPC11OutputData outputData;

    CAMX_ASSERT(NULL != pDMIAddr);

    pDMIAddr           += m_32bitDMIBufferOffsetDword + (pInputData->pStripeConfig->stripeId * m_32bitDMILength);
    outputData.pRegCmd  = &m_regCmd;
    outputData.pDMIData = pDMIAddr;

    result = IQInterface::IFEPDPC11CalculateSetting(&m_dependenceData, pInputData->pOEMIQSetting, &outputData);

    if (CamxResultSuccess != result)
    {
        CAMX_LOG_ERROR(CamxLogGroupISP, "PDPC Calculation Failed.");
    }

    if (NULL != pInputData->pStripingInput)
    {
        pInputData->pStripingInput->enableBits.PDAF                               = m_moduleEnable;
        pInputData->pStripingInput->stripingInput.pdaf_enable                     = static_cast<int16_t>(m_moduleEnable);
        pInputData->pStripingInput->stripingInput.pdpc_in.pdaf_global_offset_x    = outputData.pdpcState.pdaf_global_offset_x;
        pInputData->pStripingInput->stripingInput.pdpc_in.pdaf_x_end              = outputData.pdpcState.pdaf_x_end;
        pInputData->pStripingInput->stripingInput.pdpc_in.pdaf_zzHDR_first_rb_exp =
            outputData.pdpcState.pdaf_zzHDR_first_rb_exp;

        Utils::Memcpy(pInputData->pStripingInput->stripingInput.pdpc_in.PDAF_PD_Mask,
            outputData.pdpcState.PDAF_PD_Mask,
            sizeof(outputData.pdpcState.PDAF_PD_Mask));
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEPDPC11::DeallocateCommonLibraryData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IFEPDPC11::DeallocateCommonLibraryData()
{
    if (NULL != m_dependenceData.pInterpolationData)
    {
        CAMX_FREE(m_dependenceData.pInterpolationData);
        m_dependenceData.pInterpolationData = NULL;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEPDPC11::IFEPDPC11
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IFEPDPC11::IFEPDPC11()
{
    m_type           = ISPIQModuleType::IFEPDPC;
    m_32bitDMILength = IFEPDPC11DMILengthDword;
    m_cmdLength      = PacketBuilder::RequiredWriteRegRangeSizeInDwords(IFEPDPC11RegLengthDword);
    m_moduleEnable   = TRUE;
    m_isLUTLoaded    = FALSE;
    m_pChromatix     = NULL;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IFEPDPC11::~IFEPDPC11
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IFEPDPC11::~IFEPDPC11()
{
    m_pChromatix = NULL;
    DeallocateCommonLibraryData();
}

CAMX_NAMESPACE_END
