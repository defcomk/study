////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017-2019 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxbpsgic30.cpp
/// @brief BPSGIC30 class implementation
///        Corrects localized Gb/Gr imbalance artifacts in image
///
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "bps_data.h"
#include "camxbpsgic30.h"
#include "camxiqinterface.h"
#include "camxnode.h"
#include "camxtuningdatamanager.h"
#include "parametertuningtypes.h"
#include "bpsgic30setting.h"

CAMX_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSGIC30::Create
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSGIC30::Create(
    BPSModuleCreateData* pCreateData)
{
    CamxResult result = CamxResultSuccess;

    if ((NULL != pCreateData) && (NULL != pCreateData->pNodeIdentifier))
    {
        BPSGIC30* pModule = CAMX_NEW BPSGIC30(pCreateData->pNodeIdentifier);

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
            result = CamxResultENoMemory;
            CAMX_ASSERT_ALWAYS_MESSAGE("Memory allocation failed");
        }

        pCreateData->pModule = pModule;
    }
    else
    {
        result = CamxResultEInvalidArg;
        CAMX_LOG_ERROR(CamxLogGroupISP, "Null input pointer");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSGIC30::Initialize
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSGIC30::Initialize()
{
    CamxResult     result          = CamxResultSuccess;

    result = AllocateCommonLibraryData();
    if (result != CamxResultSuccess)
    {
        CAMX_LOG_ERROR(CamxLogGroupPProc, "Unable to initilizee common library data, no memory");
    }
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSGIC30::FillCmdBufferManagerParams
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSGIC30::FillCmdBufferManagerParams(
    const ISPInputData*     pInputData,
    IQModuleCmdBufferParam* pParam)
{
    CamxResult result                  = CamxResultSuccess;
    ResourceParams* pResourceParams    = NULL;
    CHAR*           pBufferManagerName = NULL;

    if ((NULL != pParam) && (NULL != pParam->pCmdBufManagerParam) && (NULL != pInputData))
    {
        // The Resource Params and Buffer Manager Name will be freed by caller Node
        pResourceParams = static_cast<ResourceParams*>(CAMX_CALLOC(sizeof(ResourceParams)));
        if (NULL != pResourceParams)
        {
            pBufferManagerName = static_cast<CHAR*>(CAMX_CALLOC((sizeof(CHAR) * MaxStringLength256)));
            if (NULL != pBufferManagerName)
            {
                OsUtils::SNPrintF(pBufferManagerName, (sizeof(CHAR) * MaxStringLength256), "CBM_%s_%s",
                                  (NULL != m_pNodeIdentifier) ? m_pNodeIdentifier : " ", "BPSGIC30");
                pResourceParams->resourceSize                 = BPSGICNoiseLUTBufferSize;
                pResourceParams->poolSize                     = (pInputData->requestQueueDepth * pResourceParams->resourceSize);
                pResourceParams->usageFlags.cmdBuffer         = 1;
                pResourceParams->cmdParams.type               = CmdType::CDMDMI;
                pResourceParams->alignment                    = CamxCommandBufferAlignmentInBytes;
                pResourceParams->cmdParams.enableAddrPatching = 0;
                pResourceParams->cmdParams.maxNumNestedAddrs  = 0;
                pResourceParams->memFlags                     = CSLMemFlagUMDAccess;
                pResourceParams->pDeviceIndices               = pInputData->pipelineBPSData.pDeviceIndex;
                pResourceParams->numDevices                   = 1;

                pParam->pCmdBufManagerParam[pParam->numberOfCmdBufManagers].pBufferManagerName = pBufferManagerName;
                pParam->pCmdBufManagerParam[pParam->numberOfCmdBufManagers].pParams            = pResourceParams;
                pParam->pCmdBufManagerParam[pParam->numberOfCmdBufManagers].ppCmdBufferManager = &m_pLUTCmdBufferManager;
                pParam->numberOfCmdBufManagers++;

            }
            else
            {
                result = CamxResultENoMemory;
                CAMX_FREE(pResourceParams);
                pResourceParams = NULL;
                CAMX_LOG_ERROR(CamxLogGroupISP, "Out Of Memory");
            }
        }
        else
        {
            result = CamxResultENoMemory;
            CAMX_LOG_ERROR(CamxLogGroupISP, "Out Of Memory");
        }
    }
    else
    {
        result = CamxResultEInvalidArg;
        CAMX_LOG_ERROR(CamxLogGroupISP, "Input Error %p", pParam);
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSGIC30::AllocateCommonLibraryData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSGIC30::AllocateCommonLibraryData()
{
    CamxResult result = CamxResultSuccess;

    UINT interpolationSize = (sizeof(gic_3_0_0::gic30_rgn_dataType) * (GIC30MaxmiumNonLeafNode + 1));

    if (NULL == m_dependenceData.pInterpolationData)
    {
        // Allocate memory for gic_3_0_0::gic30_rgn_dataType
        m_dependenceData.pInterpolationData = CAMX_CALLOC(interpolationSize);
        if (NULL == m_dependenceData.pInterpolationData)
        {
            result = CamxResultENoMemory;
        }
    }
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSGIC30::CheckDependenceChange
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL BPSGIC30::CheckDependenceChange(
    ISPInputData* pInputData)
{
    BOOL isChanged = FALSE;

    if ((NULL != pInputData)           &&
        (NULL != pInputData->pHwContext))
    {
        if (NULL != pInputData->pOEMIQSetting)
        {
            m_moduleEnable = (static_cast<OEMBPSIQSetting*>(pInputData->pOEMIQSetting))->GICEnable;
            isChanged      = (TRUE == m_moduleEnable);
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

                m_pChromatix = pTuningManager->GetChromatix()->GetModule_gic30_bps(
                    reinterpret_cast<TuningMode*>(&pInputData->pTuningData->TuningMode[0]),
                    pInputData->pTuningData->noOfSelectionParameter);
            }

            CAMX_ASSERT(NULL != m_pChromatix);
            if (NULL != m_pChromatix)
            {
                if (m_pChromatix != m_dependenceData.pChromatix)
                {
                    m_dependenceData.pChromatix = m_pChromatix;
                    m_moduleEnable              = m_pChromatix->enable_section.gic_global_enable;
                    isChanged                   = (TRUE == m_moduleEnable);
                }
            }
            else
            {
                CAMX_LOG_ERROR(CamxLogGroupISP, "Failed to get Chromatix");
            }
        }

        if (TRUE == m_moduleEnable)
        {
            // Check for trigger update status
            if (TRUE == IQInterface::s_interpolationTable.BPSGIC30TriggerUpdate(&(pInputData->triggerData), &m_dependenceData))
            {
                if (NULL == pInputData->pOEMIQSetting)
                {
                    // Check for module dynamic enable trigger hysterisis
                    m_moduleEnable = IQSettingUtils::GetDynamicEnableFlag(
                        m_dependenceData.pChromatix->dynamic_enable_triggers.gic_global_enable.enable,
                        m_dependenceData.pChromatix->dynamic_enable_triggers.gic_global_enable.hyst_control_var,
                        m_dependenceData.pChromatix->dynamic_enable_triggers.gic_global_enable.hyst_mode,
                        &(m_dependenceData.pChromatix->dynamic_enable_triggers.gic_global_enable.hyst_trigger),
                        static_cast<VOID*>(&(pInputData->triggerData)),
                        &m_dependenceData.moduleEnable);

                    // Set status to isChanged to avoid calling RunCalculation if the module is disabled
                    isChanged = (TRUE == m_moduleEnable);
                }
            }
        }
    }
    else
    {
        if (NULL != pInputData)
        {
            CAMX_LOG_ERROR(CamxLogGroupISP,
                           "Invalid Input: pHwContext %p",
                           pInputData->pHwContext);
        }
        else
        {
            CAMX_LOG_ERROR(CamxLogGroupISP, "Null Input Pointer");
        }
    }

    return isChanged;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSGIC30::WriteLUTtoDMI
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSGIC30::WriteLUTtoDMI(
    const ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;

    // CDM pack the DMI buffer and patch the Red and Blue LUT DMI buffer into CDM pack
    if ((NULL != pInputData) && (NULL != pInputData->pDMICmdBuffer))
    {
        result = PacketBuilder::WriteDMI(pInputData->pDMICmdBuffer,
                                         regBPS_BPS_0_CLC_GIC_DMI_CFG,
                                         GICNoiseLUT,
                                         m_pLUTDMICmdBuffer,
                                         0,
                                         BPSGICNoiseLUTBufferSize);

        if (CamxResultSuccess == result)
        {
            ///< Switch LUT Bank select immediately after writing
            m_dependenceData.LUTBankSel = (m_dependenceData.LUTBankSel == GICLUTBank0) ? GICLUTBank1 : GICLUTBank0;
        }
        else
        {
            CAMX_ASSERT_ALWAYS_MESSAGE("Failed to write DMI data");
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
// BPSGIC30::CreateCmdList
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSGIC30::CreateCmdList(
    const ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;

    result = WriteLUTtoDMI(pInputData);

    if ((CamxResultSuccess == result) && (NULL != pInputData->pCmdBuffer))
    {
        CmdBuffer* pCmdBuffer = pInputData->pCmdBuffer;

        result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                              regBPS_BPS_0_CLC_GIC_GIC_FILTER_CFG,
                                              (sizeof(BPSGIC30RegConfig) / RegisterWidthInBytes),
                                              reinterpret_cast<UINT32*>(&m_regCmd));

        if (CamxResultSuccess != result)
        {
            CAMX_ASSERT_ALWAYS_MESSAGE("Failed to write command buffer");
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
// BPSGIC30::FetchDMIBuffer
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSGIC30::FetchDMIBuffer()
{
    CamxResult      result          = CamxResultSuccess;
    PacketResource* pPacketResource = NULL;

    if (NULL != m_pLUTCmdBufferManager)
    {
        // Recycle the last updated LUT DMI cmd buffer
        if (NULL != m_pLUTDMICmdBuffer)
        {
            m_pLUTCmdBufferManager->Recycle(m_pLUTDMICmdBuffer);
        }

        // fetch a fresh LUT DMI cmd buffer
        result = m_pLUTCmdBufferManager->GetBuffer(&pPacketResource);
    }
    else
    {
        result = CamxResultEFailed;
        CAMX_LOG_ERROR(CamxLogGroupISP, "LUT Command Buffer is NULL");
    }

    if (CamxResultSuccess == result)
    {
        m_pLUTDMICmdBuffer = static_cast<CmdBuffer*>(pPacketResource);
    }
    else
    {
        m_pLUTDMICmdBuffer = NULL;
        result             = CamxResultEUnableToLoad;
        CAMX_ASSERT_ALWAYS_MESSAGE("Failed to fetch DMI Buffer");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSGIC30::RunCalculation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSGIC30::RunCalculation(
    ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;

    if (NULL != pInputData)
    {
        result = FetchDMIBuffer();

        if (CamxResultSuccess == result)
        {
            GIC30OutputData outputData;

            outputData.pDMIData     = reinterpret_cast<UINT32*>(m_pLUTDMICmdBuffer->BeginCommands(BPSGIC30DMILengthDword));
            // BET ONLY - InputData is different per module tested
            pInputData->pBetDMIAddr = static_cast<VOID*>(outputData.pDMIData);
            CAMX_ASSERT(NULL != outputData.pDMIData);

            outputData.pRegCmd       = &m_regCmd;
            outputData.pModuleConfig = &m_moduleConfig;

            result = IQInterface::BPSGIC30CalculateSetting(&m_dependenceData, pInputData->pOEMIQSetting, &outputData);
            if (CamxResultSuccess == result)
            {
                m_pLUTDMICmdBuffer->CommitCommands();
                if (NULL != pInputData->pBPSTuningMetadata)
                {
                    m_pGICNoiseLUT = outputData.pDMIData;
                }
                if (CamxLogGroupIQMod == (pInputData->dumpRegConfig & CamxLogGroupIQMod))
                {
                    DumpRegConfig(pInputData, outputData.pModuleConfig);
                }
            }
            else
            {
                CAMX_LOG_ERROR(CamxLogGroupISP, "GIC Calculation Failed. result %d", result);
            }
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
// BPSGIC30::ValidateDependenceParams
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSGIC30::ValidateDependenceParams(
    const ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;

    if (NULL != pInputData)
    {

        if ((NULL == pInputData->pHwContext) || (NULL == pInputData->pAECUpdateData))
        {
            CAMX_LOG_ERROR(CamxLogGroupISP,
                "Invalid Input: pInputData %p pNewAECUpdate %p  HwContext %p",
                pInputData,
                pInputData->pAECUpdateData,
                pInputData->pHwContext);
            result = CamxResultEFailed;
        }
    }
    else
    {
        result = CamxResultEFailed;
        CAMX_LOG_ERROR(CamxLogGroupISP, "Null Input Pointer");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSGIC30::GetRegCmd
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID* BPSGIC30::GetRegCmd()
{
    return static_cast<VOID*>(&m_regCmd);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSGIC30::UpdateBPSInternalData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID BPSGIC30::UpdateBPSInternalData(
    const ISPInputData* pInputData
    ) const
{
    BpsIQSettings*  pBPSIQSettings = reinterpret_cast<BpsIQSettings*>(pInputData->pipelineBPSData.pIQSettings);
    CAMX_ASSERT(NULL != pBPSIQSettings);

    pBPSIQSettings->gicParameters.moduleCfg.EN     = m_moduleEnable;
    pBPSIQSettings->gicParameters.moduleCfg.PNR_EN = m_moduleConfig.moduleConfig.bitfields.PNR_EN;
    pBPSIQSettings->gicParameters.moduleCfg.GIC_EN = m_moduleConfig.moduleConfig.bitfields.GIC_EN;

    // Post tuning metadata if setting is enabled
    if (NULL != pInputData->pBPSTuningMetadata)
    {
        CAMX_STATIC_ASSERT(BPSGICNoiseLUTBufferSize <=
                          sizeof(pInputData->pBPSTuningMetadata->BPSDMIData.packedLUT.GICLUT));
        CAMX_STATIC_ASSERT(sizeof(BPSGIC30RegConfig) <= sizeof(pInputData->pBPSTuningMetadata->BPSGICData));

        if (NULL != m_pGICNoiseLUT)
        {
            Utils::Memcpy(&pInputData->pBPSTuningMetadata->BPSDMIData.packedLUT.GICLUT,
                          m_pGICNoiseLUT,
                          BPSGICNoiseLUTBufferSize);
        }
        Utils::Memcpy(&pInputData->pBPSTuningMetadata->BPSGICData,
                      &m_regCmd,
                      sizeof(BPSGIC30RegConfig));
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSGIC30::Execute
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSGIC30::Execute(
    ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;

    if (NULL != pInputData)
    {
        // Check if dependent is valid and been updated compared to last request
        result = ValidateDependenceParams(pInputData);

        if (CamxResultSuccess == result)
        {
            if (TRUE == CheckDependenceChange(pInputData))
            {
                result = RunCalculation(pInputData);
            }

            if ((CamxResultSuccess == result) && (TRUE == m_moduleEnable))
            {
                result = CreateCmdList(pInputData);
            }

            if (CamxResultSuccess == result)
            {
                UpdateBPSInternalData(pInputData);
            }
            else
            {
                CAMX_ASSERT_ALWAYS_MESSAGE("Operation failed %d", result);
            }
        }
        else
        {
            CAMX_LOG_ERROR(CamxLogGroupISP, "Parameter dependencies failed");
        }
    }
    else
    {
        result = CamxResultEInvalidArg;
        CAMX_ASSERT_ALWAYS_MESSAGE("Null Input Pointer");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSGIC30::DeallocateCommonLibraryData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID BPSGIC30::DeallocateCommonLibraryData()
{
    if (NULL != m_dependenceData.pInterpolationData)
    {
        CAMX_FREE(m_dependenceData.pInterpolationData);
        m_dependenceData.pInterpolationData = NULL;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSGIC30::BPSGIC30
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BPSGIC30::BPSGIC30(
    const CHAR* pNodeIdentifier)
{
    m_pNodeIdentifier     = pNodeIdentifier;
    m_type                = ISPIQModuleType::BPSGIC;
    m_moduleEnable        = FALSE;
    m_numLUT              = MaxGICNoiseLUT;

    m_cmdLength    = PacketBuilder::RequiredWriteRegRangeSizeInDwords(sizeof(BPSGIC30RegConfig) / RegisterWidthInBytes);

    m_pGICNoiseLUT = NULL;
    m_pChromatix   = NULL;

    m_dependenceData.moduleEnable = FALSE; ///< First frame is always FALSE
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSGIC30::~BPSGIC30
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BPSGIC30::~BPSGIC30()
{
    DeallocateCommonLibraryData();
    if (NULL != m_pLUTCmdBufferManager)
    {
        if (NULL != m_pLUTDMICmdBuffer)
        {
            m_pLUTCmdBufferManager->Recycle(m_pLUTDMICmdBuffer);
            m_pLUTDMICmdBuffer = NULL;
        }

        m_pLUTCmdBufferManager->Uninitialize();
        CAMX_DELETE m_pLUTCmdBufferManager;
        m_pLUTCmdBufferManager = NULL;
    }

    m_pChromatix = NULL;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSGIC30::DumpRegConfig
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID BPSGIC30::DumpRegConfig(
    const ISPInputData* pInputData,
    const BPSGIC30ModuleConfig* pModuleConfig
    ) const
{
    CHAR     dumpFilename[256];
    FILE*    pFile = NULL;
    UINT32*  pDMIData;
    CamX::OsUtils::SNPrintF(dumpFilename, sizeof(dumpFilename), "%s/BPS_regdump.txt", ConfigFileDirectory);
    pFile = CamX::OsUtils::FOpen(dumpFilename, "a+");

    if (NULL != pFile)
    {
        CamX::OsUtils::FPrintF(pFile, "********  BPS GIC30 ********  \n");
        CamX::OsUtils::FPrintF(pFile, "* GIC30 module CFG [HEX]\n");
        CamX::OsUtils::FPrintF(pFile, "GIC30 module CFG                  = %x\n", pModuleConfig->moduleConfig);
        CamX::OsUtils::FPrintF(pFile, "* GIC30 CFG [HEX] \n");
        CamX::OsUtils::FPrintF(pFile, "filterConfig                      = %x\n", m_regCmd.filterConfig);
        CamX::OsUtils::FPrintF(pFile, "lineNoiseOffset                   = %x\n", m_regCmd.lineNoiseOffset);
        CamX::OsUtils::FPrintF(pFile, "anchorBaseSetting0                = %x\n", m_regCmd.anchorBaseSetting0);
        CamX::OsUtils::FPrintF(pFile, "anchorBaseSetting1                = %x\n", m_regCmd.anchorBaseSetting1);
        CamX::OsUtils::FPrintF(pFile, "anchorBaseSetting2                = %x\n", m_regCmd.anchorBaseSetting2);
        CamX::OsUtils::FPrintF(pFile, "anchorBaseSetting3                = %x\n", m_regCmd.anchorBaseSetting3);
        CamX::OsUtils::FPrintF(pFile, "anchorBaseSetting4                = %x\n", m_regCmd.anchorBaseSetting4);
        CamX::OsUtils::FPrintF(pFile, "anchorBaseSetting5                = %x\n", m_regCmd.anchorBaseSetting5);
        CamX::OsUtils::FPrintF(pFile, "slopeShiftSetting0                = %x\n", m_regCmd.slopeShiftSetting0);
        CamX::OsUtils::FPrintF(pFile, "slopeShiftSetting1                = %x\n", m_regCmd.slopeShiftSetting1);
        CamX::OsUtils::FPrintF(pFile, "slopeShiftSetting2                = %x\n", m_regCmd.slopeShiftSetting2);
        CamX::OsUtils::FPrintF(pFile, "slopeShiftSetting3                = %x\n", m_regCmd.slopeShiftSetting3);
        CamX::OsUtils::FPrintF(pFile, "slopeShiftSetting4                = %x\n", m_regCmd.slopeShiftSetting4);
        CamX::OsUtils::FPrintF(pFile, "slopeShiftSetting5                = %x\n", m_regCmd.slopeShiftSetting5);
        CamX::OsUtils::FPrintF(pFile, "horizontalVerticalOffset          = %x\n", m_regCmd.horizontalVerticalOffset);
        CamX::OsUtils::FPrintF(pFile, "squareInit                        = %x\n", m_regCmd.squareInit);
        CamX::OsUtils::FPrintF(pFile, "scaleShift                        = %x\n", m_regCmd.scaleShift);
        CamX::OsUtils::FPrintF(pFile, "noiseRatioScale0                  = %x\n", m_regCmd.noiseRatioScale0);
        CamX::OsUtils::FPrintF(pFile, "noiseRatioScale1                  = %x\n", m_regCmd.noiseRatioScale1);
        CamX::OsUtils::FPrintF(pFile, "noiseRatioScale2                  = %x\n", m_regCmd.noiseRatioScale2);
        CamX::OsUtils::FPrintF(pFile, "noiseRatioScale3                  = %x\n", m_regCmd.noiseRatioScale3);
        CamX::OsUtils::FPrintF(pFile, "noiseRatioFilterStrength          = %x\n", m_regCmd.noiseRatioFilterStrength);

        if (TRUE == m_moduleEnable)
        {
            pDMIData = static_cast<UINT32*>(pInputData->pBetDMIAddr);

            CamX::OsUtils::FPrintF(pFile, "* gic30Data.noise_std_lut_level0[2][64] = \n");
            for (UINT32 index = 0; index < DMIRAM_GIC_NOISESTD_LENGTH_V30; index++)
            {
                CamX::OsUtils::FPrintF(pFile, "           %d", (*(pDMIData + index)));
            }
        }
        else
        {
            CamX::OsUtils::FPrintF(pFile, "NOT ENABLED");
        }

        CamX::OsUtils::FPrintF(pFile, "\n\n");
        CamX::OsUtils::FClose(pFile);
    }
}

CAMX_NAMESPACE_END
