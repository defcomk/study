////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017-2019 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxbpsgtm10.cpp
/// @brief BPSGTM10 class implementation
///        Global tone mapping to normalize intensities of the image content
///
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "bps_data.h"
#include "camxbpsgtm10.h"
#include "camxiqinterface.h"
#include "camxnode.h"
#include "camxtuningdatamanager.h"
#include "parametertuningtypes.h"
#include "gtm10setting.h"

CAMX_NAMESPACE_BEGIN

static const UINT32 GTMLUT    = 0x1;
static const UINT32 MaxGTMLUT = 1;

static const UINT32 BPSGTM10LUTTableSize     = 64; // DMI table size
static const UINT32 BPSGTM10DMILengthDword   = (BPSGTM10LUTTableSize * sizeof(UINT64)) / sizeof(UINT32);
static const UINT32 BPSGTM10DMILengthInBytes = (BPSGTM10DMILengthDword * sizeof(UINT32));

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSGTM10::Create
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSGTM10::Create(
    BPSModuleCreateData* pCreateData)
{
    CamxResult result = CamxResultSuccess;

    if ((NULL != pCreateData) && (NULL != pCreateData->pNodeIdentifier))
    {
        BPSGTM10* pModule = CAMX_NEW BPSGTM10(pCreateData->pNodeIdentifier);

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
        CAMX_LOG_ERROR(CamxLogGroupISP, "Null Input Pointer");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSGTM10::Initialize
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSGTM10::Initialize()
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
// BPSGTM10::FillCmdBufferManagerParams
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSGTM10::FillCmdBufferManagerParams(
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
                                  (NULL != m_pNodeIdentifier) ? m_pNodeIdentifier : " ", "BPSGTM10");
                pResourceParams->resourceSize                 = BPSGTM10DMILengthInBytes;
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
// BPSGTM10::AllocateCommonLibraryData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSGTM10::AllocateCommonLibraryData()
{
    CamxResult result = CamxResultSuccess;

    UINT interpolationSize = (sizeof(gtm_1_0_0::gtm10_rgn_dataType) * (GTM10MaxmiumNonLeafNode + 1));

    if (NULL == m_dependenceData.pInterpolationData)
    {
        m_dependenceData.pInterpolationData = CAMX_CALLOC(interpolationSize);
        if (NULL == m_dependenceData.pInterpolationData)
        {
            result = CamxResultENoMemory;
        }
    }
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSGTM10::CheckDependenceChange
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL BPSGTM10::CheckDependenceChange(
    ISPInputData* pInputData)
{
    BOOL    isChanged   = FALSE;
    BOOL    manualMode  = FALSE;

    if ((NULL != pInputData)           &&
        (NULL != pInputData->pHwContext))
    {
        if (NULL != pInputData->pOEMIQSetting)
        {
            m_moduleEnable = (static_cast<OEMBPSIQSetting*>(pInputData->pOEMIQSetting))->GTMEnable;

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

                m_pChromatix    = pTuningManager->GetChromatix()->GetModule_gtm10_bps(
                                      reinterpret_cast<TuningMode*>(&pInputData->pTuningData->TuningMode[0]),
                                      pInputData->pTuningData->noOfSelectionParameter);
                m_pTMCChromatix = pTuningManager->GetChromatix()->GetModule_tmc10_sw(
                                      reinterpret_cast<TuningMode*>(&pInputData->pTuningData->TuningMode[0]),
                                      pInputData->pTuningData->noOfSelectionParameter);
            }

            CAMX_ASSERT(NULL != m_pChromatix);
            CAMX_ASSERT(NULL != m_pTMCChromatix);

            if (NULL != m_pChromatix)
            {
                if (m_pChromatix != m_dependenceData.pChromatix)
                {
                    m_dependenceData.pChromatix = m_pChromatix;
                    m_pTMCInput.pChromatix      = m_pTMCChromatix;
                    m_moduleEnable              = m_pChromatix->enable_section.gtm_enable;

                    isChanged                   = (TRUE == m_moduleEnable);
                }

                // Check for manual control
                if ((TonemapModeContrastCurve   == pInputData->pHALTagsData->tonemapCurves.tonemapMode) &&
                    (0                          != pInputData->pHALTagsData->tonemapCurves.curvePoints))
                {
                    m_moduleEnable  = FALSE;
                    isChanged       = FALSE;
                    manualMode      = TRUE;
                }

                if (TRUE == m_moduleEnable)
                {
                    if (TRUE == IQInterface::s_interpolationTable.GTM10TriggerUpdate(&pInputData->triggerData,
                                                                                     &m_dependenceData))
                    {
                        isChanged = TRUE;
                    }
                }

                if (NULL != m_pTMCChromatix)
                {
                    m_pTMCInput.adrcGTMEnable = FALSE;
                    if ((TRUE == m_pTMCChromatix->enable_section.adrc_isp_enable) &&
                        (TRUE == m_pTMCChromatix->chromatix_tmc10_reserve.use_gtm) &&
                        (FALSE == pInputData->triggerData.isTMC11Enabled) &&
                        (FALSE == manualMode))
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
                                isChanged = FALSE;
                                CAMX_LOG_ERROR(CamxLogGroupISP, "Memory allocation failed for ADRC Specific data");
                            }
                        }

                        pInputData->triggerData.pADRCData = m_pADRCData;
                        CAMX_LOG_VERBOSE(CamxLogGroupISP, "BPS GTM : using TMC 1.0");
                    }
                    else
                    {
                        if (NULL != pInputData->triggerData.pADRCData)
                        {
                            m_pTMCInput.adrcGTMEnable = pInputData->triggerData.pADRCData->gtmEnable;
                            CAMX_LOG_VERBOSE(CamxLogGroupISP, "BPS GTM : using TMC 1.1");
                        }
                    }

                    m_pTMCInput.pAdrcOutputData = pInputData->triggerData.pADRCData;
                }
            }
            else
            {
                CAMX_LOG_ERROR(CamxLogGroupISP, "Failed to get Chromatix");
            }
        }
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupISP,
                       "Invalid Input: pInputData %p", pInputData);
    }

    return isChanged;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSGTM10::FetchDMIBuffer
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSGTM10::FetchDMIBuffer()
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
        CAMX_LOG_ERROR(CamxLogGroupISP, "LUT Command Buffer Manager is NULL");
    }

    if (CamxResultSuccess == result)
    {
        m_pLUTDMICmdBuffer = static_cast<CmdBuffer*>(pPacketResource);
    }
    else
    {
        m_pLUTDMICmdBuffer = NULL;
        CAMX_ASSERT_ALWAYS_MESSAGE("Failed to fetch DMI Buffer");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSGTM10::RunCalculation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSGTM10::RunCalculation(
    const ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;

    if (NULL != pInputData)
    {
        result = FetchDMIBuffer();

        if (CamxResultSuccess == result)
        {
            GTM10OutputData outputData;

            outputData.regCmd.BPS.pDMIDataPtr =
                reinterpret_cast<UINT64*>(m_pLUTDMICmdBuffer->BeginCommands(BPSGTM10DMILengthDword));
            CAMX_ASSERT(NULL != outputData.regCmd.BPS.pDMIDataPtr);

            outputData.type = PipelineType::BPS;
            outputData.registerBETEn = pInputData->registerBETEn;

            result =
                IQInterface::GTM10CalculateSetting(&m_dependenceData, pInputData->pOEMIQSetting, &outputData, &m_pTMCInput);

            if (CamxResultSuccess == result)
            {
                m_pLUTDMICmdBuffer->CommitCommands();
                if (NULL != pInputData->pBPSTuningMetadata)
                {
                    m_pGTMLUTPtr = outputData.regCmd.BPS.pDMIDataPtr;
                }
                if (CamxLogGroupIQMod == (pInputData->dumpRegConfig & CamxLogGroupIQMod))
                {
                    DumpRegConfig(pInputData);
                }
            }
            else
            {
                CAMX_LOG_ERROR(CamxLogGroupISP, "GTM10 Calculation Failed. result %d", result);
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
// BPSGTM10::WriteLUTtoDMI
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSGTM10::WriteLUTtoDMI(
    ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;

    // CDM pack the DMI buffer and patch the Red and Blue LUT DMI buffer into CDM pack
    if ((NULL != pInputData) && (NULL != pInputData->pDMICmdBuffer))
    {
        result = PacketBuilder::WriteDMI(pInputData->pDMICmdBuffer,
                                         regBPS_BPS_0_CLC_GTM_DMI_CFG,
                                         GTMLUT,
                                         m_pLUTDMICmdBuffer,
                                         0,
                                         BPSGTM10DMILengthInBytes);

        if (CamxResultSuccess != result)
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
// BPSGTM10::UpdateBPSInternalData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID BPSGTM10::UpdateBPSInternalData(
    const ISPInputData* pInputData
    ) const
{
    BpsIQSettings*  pBPSIQSettings = reinterpret_cast<BpsIQSettings*>(pInputData->pipelineBPSData.pIQSettings);
    CAMX_ASSERT(NULL != pBPSIQSettings);

    pBPSIQSettings->gtmParameters.moduleCfg.EN = m_moduleEnable;
    pInputData->pCalculatedData->percentageOfGTM =
        (NULL == m_pTMCInput.pAdrcOutputData) ? 0 : m_pTMCInput.pAdrcOutputData->gtmPercentage;

    // Post tuning metadata if setting is enabled
    if (NULL != pInputData->pBPSTuningMetadata)
    {
        CAMX_STATIC_ASSERT((BPSGTM10DMILengthDword * sizeof(UINT32)) <=
                          sizeof(pInputData->pBPSTuningMetadata->BPSDMIData.packedLUT.GTM.LUT));
        if (NULL != m_pGTMLUTPtr)
        {
            Utils::Memcpy(pInputData->pBPSTuningMetadata->BPSDMIData.packedLUT.GTM.LUT,
                          m_pGTMLUTPtr,
                          BPSGTM10DMILengthDword * sizeof(UINT32));
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSGTM10::Execute
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSGTM10::Execute(
    ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;

    if (NULL != pInputData)
    {
        /// @todo (CAMX-1768) Add Setting to perform interpolation regardless of dependence change
        if (TRUE == CheckDependenceChange(pInputData))
        {
            result = RunCalculation(pInputData);
        }

        if ((CamxResultSuccess == result) && (TRUE == m_moduleEnable))
        {
            result = WriteLUTtoDMI(pInputData);
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
        result = CamxResultEInvalidArg;
        CAMX_ASSERT_ALWAYS_MESSAGE("Null Input Pointer");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSGTM10::DeallocateCommonLibraryData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID BPSGTM10::DeallocateCommonLibraryData()
{
    if (NULL != m_dependenceData.pInterpolationData)
    {
        CAMX_FREE(m_dependenceData.pInterpolationData);
        m_dependenceData.pInterpolationData = NULL;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSGTM10::BPSGTM10
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BPSGTM10::BPSGTM10(
    const CHAR* pNodeIdentifier)
{
    m_pNodeIdentifier   = pNodeIdentifier;
    m_type          = ISPIQModuleType::BPSGTM;
    m_moduleEnable  = FALSE;
    m_numLUT        = MaxGTMLUT;
    m_cmdLength     = 0;
    m_pGTMLUTPtr    = NULL;
    m_pChromatix    = NULL;
    m_pTMCChromatix = NULL;
    m_pADRCData     = NULL;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSGTM10::~BPSGTM10
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BPSGTM10::~BPSGTM10()
{
    DeallocateCommonLibraryData();

    CAMX_FREE(m_pADRCData);

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

    m_pChromatix    = NULL;
    m_pTMCChromatix = NULL;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSGTM10::DumpRegConfig
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID BPSGTM10::DumpRegConfig(
    const ISPInputData* pInputData
    ) const
{
    CHAR     dumpFilename[256];
    FILE*    pFile = NULL;
    UINT32*  pDMIData;
    CamX::OsUtils::SNPrintF(dumpFilename, sizeof(dumpFilename), "%s/BPS_regdump.txt", ConfigFileDirectory);
    pFile = CamX::OsUtils::FOpen(dumpFilename, "a+");

    if (NULL != pFile)
    {
        CamX::OsUtils::FPrintF(pFile, "******** BPS GTM10 ******** \n");
        if (TRUE == m_moduleEnable)
        {
            pDMIData = static_cast<UINT32*>(pInputData->pBetDMIAddr);

            CamX::OsUtils::FPrintF(pFile, "* gtm10Data.yratio_base[2][64] = \n");
            for (UINT32 index = 0; index < GTM10LUTSize * 2; index++)
            {
                if (index % 2 == 0)
                {
                    CamX::OsUtils::FPrintF(pFile, "           %d",
                        static_cast<UINT64>(*(pDMIData + index)) & Utils::AllOnes64(18));
                }
            }
            CamX::OsUtils::FPrintF(pFile, "* gtm10Data.yratio_slope[2][64] = \n");
            for (UINT32 index = 0; index < GTM10LUTSize * 2; index++)
            {
                if (index % 2 != 0)
                {
                    CamX::OsUtils::FPrintF(pFile, "           %d", static_cast<UINT64>(*(pDMIData + index)));
                }
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
