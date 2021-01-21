////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017-2019 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxbpsgamma16.cpp
/// @brief CAMXBPSGAMMA16 class implementation
///        Gamm LUT to compensate intensity nonlinearity of display
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "bps_data.h"
#include "camxbpsgamma16.h"
#include "camxiqinterface.h"
#include "camxnode.h"
#include "camxtuningdatamanager.h"
#include "gamma16setting.h"

CAMX_NAMESPACE_BEGIN

static const UINT32 Channel0LUT            = BPS_BPS_0_CLC_GLUT_DMI_LUT_CFG_LUT_SEL_GLUT_CH0_LUT;
static const UINT32 Channel1LUT            = BPS_BPS_0_CLC_GLUT_DMI_LUT_CFG_LUT_SEL_GLUT_CH1_LUT;
static const UINT32 Channel2LUT            = BPS_BPS_0_CLC_GLUT_DMI_LUT_CFG_LUT_SEL_GLUT_CH2_LUT;

static const UINT32 GammaSizeLUTInBytes    = NumberOfEntriesPerLUT * sizeof(UINT32);
static const UINT32 BPSGammalLUTBufferSize = GammaSizeLUTInBytes * GammaLUTMax;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSGamma16::Create
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSGamma16::Create(
    BPSModuleCreateData* pCreateData)
{
    CamxResult result = CamxResultSuccess;

    if ((NULL != pCreateData) && (NULL != pCreateData->pNodeIdentifier))
    {
        BPSGamma16* pModule = CAMX_NEW BPSGamma16(pCreateData->pNodeIdentifier);
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
// BPSGamma16::FillCmdBufferManagerParams
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSGamma16::FillCmdBufferManagerParams(
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
                                  (NULL != m_pNodeIdentifier) ? m_pNodeIdentifier : " ", "BPSGamma16");
                pResourceParams->resourceSize                 = BPSGammalLUTBufferSize;
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
// BPSGamma16::CheckDependenceChange
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL BPSGamma16::CheckDependenceChange(
    ISPInputData* pInputData)
{
    BOOL isChanged = FALSE;

    if ((NULL != pInputData)                &&
        (NULL != pInputData->pHALTagsData)  &&
        (NULL != pInputData->pHwContext))
    {
        if (NULL != pInputData->pOEMIQSetting)
        {
            m_moduleEnable = (static_cast<OEMBPSIQSetting*>(pInputData->pOEMIQSetting))->GammaEnable;

            isChanged      = (TRUE == m_moduleEnable);
        }
        else
        {
            TuningDataManager* pTuningManager = pInputData->pTuningDataManager;
            CAMX_ASSERT(NULL != pTuningManager);

            m_dependenceData.pLibData = pInputData->pLibInitialData;

            // Search through the tuning data (tree), only when there
            // are changes to the tuning mode data as an optimization
            if ((TRUE == pInputData->tuningModeChanged)    &&
                (TRUE == pTuningManager->IsValidChromatix()))
            {
                CAMX_ASSERT(NULL != pInputData->pTuningData);

                m_pChromatix = pTuningManager->GetChromatix()->GetModule_gamma16_bps(
                                   reinterpret_cast<TuningMode*>(&pInputData->pTuningData->TuningMode[0]),
                                   pInputData->pTuningData->noOfSelectionParameter);
            }

            CAMX_ASSERT(NULL != m_pChromatix);
            if (NULL != m_pChromatix)
            {
                if ((m_pChromatix != m_dependenceData.pChromatix) ||
                    (m_pChromatix->SymbolTableID != m_dependenceData.pChromatix->SymbolTableID) ||
                    (m_pChromatix->enable_section.gamma_enable != m_moduleEnable))
                {
                    m_dependenceData.pChromatix = m_pChromatix;
                    m_moduleEnable              = m_pChromatix->enable_section.gamma_enable;

                    isChanged                   = (TRUE == m_moduleEnable);
                }

                // Check for manual control
                if (TonemapModeContrastCurve == pInputData->pHALTagsData->tonemapCurves.tonemapMode)
                {
                    m_moduleEnable = FALSE;
                    isChanged      = FALSE;
                }
            }
            else
            {
                CAMX_LOG_ERROR(CamxLogGroupISP, "Failed to get Chromatix");
            }

            if (TRUE == m_moduleEnable)
            {
                if (TRUE ==
                    IQInterface::s_interpolationTable.gamma16TriggerUpdate(&pInputData->triggerData, &m_dependenceData))
                {
                    isChanged = TRUE;
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
// BPSGamma16::WriteLUTtoDMI
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSGamma16::WriteLUTtoDMI(
    const ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;

    // CDM pack the DMI buffer and patch the Red and Blue LUT DMI buffer into CDM pack
    if ((NULL != pInputData) && (NULL != pInputData->pDMICmdBuffer))
    {
        UINT32 LUTOffset = 0;
        result = PacketBuilder::WriteDMI(pInputData->pDMICmdBuffer,
                                         regBPS_BPS_0_CLC_GLUT_DMI_CFG,
                                         Channel0LUT,
                                         m_pLUTDMICmdBuffer,
                                         LUTOffset,
                                         GammaSizeLUTInBytes);

        if (CamxResultSuccess == result)
        {
            LUTOffset += GammaSizeLUTInBytes;
            result = PacketBuilder::WriteDMI(pInputData->pDMICmdBuffer,
                                             regBPS_BPS_0_CLC_GLUT_DMI_CFG,
                                             Channel1LUT,
                                             m_pLUTDMICmdBuffer,
                                             LUTOffset,
                                             GammaSizeLUTInBytes);
        }

        if (CamxResultSuccess == result)
        {
            LUTOffset += GammaSizeLUTInBytes;
            result = PacketBuilder::WriteDMI(pInputData->pDMICmdBuffer,
                                             regBPS_BPS_0_CLC_GLUT_DMI_CFG,
                                             Channel2LUT,
                                             m_pLUTDMICmdBuffer,
                                             LUTOffset,
                                             GammaSizeLUTInBytes);
        }

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
// BPSGamma16::FetchDMIBuffer
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSGamma16::FetchDMIBuffer()
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
        result             = CamxResultEUnableToLoad;
        CAMX_ASSERT_ALWAYS_MESSAGE("Failed to fetch DMI Buffer");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSGamma16::RunCalculation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSGamma16::RunCalculation(
    ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;

    if (NULL != pInputData)
    {
        result = FetchDMIBuffer();

        if (CamxResultSuccess == result)
        {
            Gamma16OutputData outputData;

            outputData.type = PipelineType::BPS;

            // Update the LUT DMI buffer with Channel G LUT data
            outputData.pGDMIDataPtr =
                reinterpret_cast<UINT32*>(m_pLUTDMICmdBuffer->BeginCommands(BPSGammalLUTBufferSize / sizeof(UINT32)));
            // BET ONLY - InputData is different per module tested
            pInputData->pBetDMIAddr = static_cast<VOID*>(outputData.pGDMIDataPtr);
            CAMX_ASSERT(NULL != outputData.pGDMIDataPtr);

            // Update the LUT DMI buffer with Channel B LUT data
            outputData.pBDMIDataPtr =
                reinterpret_cast<UINT32*>(GammaSizeLUTInBytes + reinterpret_cast<UCHAR*>(outputData.pGDMIDataPtr));
            CAMX_ASSERT(NULL != outputData.pBDMIDataPtr);

            // Update the LUT DMI buffer with Channel R LUT data
            outputData.pRDMIDataPtr =
                reinterpret_cast<UINT32*>(GammaSizeLUTInBytes + reinterpret_cast<UCHAR*>(outputData.pBDMIDataPtr));
            CAMX_ASSERT(NULL != outputData.pRDMIDataPtr);

            result = IQInterface::Gamma16CalculateSetting(&m_dependenceData,
                                                          pInputData->pOEMIQSetting,
                                                          &outputData,
                                                          NULL);

            if (CamxResultSuccess == result)
            {
                m_pGammaG = outputData.pGDMIDataPtr;
                m_pLUTDMICmdBuffer->CommitCommands();

                if (NULL != pInputData->pBPSTuningMetadata)
                {
                    m_pGammaTuning[GammaLUTChannelR] = outputData.pRDMIDataPtr;
                    m_pGammaTuning[GammaLUTChannelG] = outputData.pGDMIDataPtr;
                    m_pGammaTuning[GammaLUTChannelB] = outputData.pBDMIDataPtr;
                }
                if (CamxLogGroupIQMod == (pInputData->dumpRegConfig & CamxLogGroupIQMod))
                {
                    DumpRegConfig(pInputData);
                }
            }
            else
            {
                m_pGammaG = NULL;
                CAMX_LOG_ERROR(CamxLogGroupISP, "Gamma Calculation Failed");
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
// BPSGamma16::UpdateBPSInternalData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID BPSGamma16::UpdateBPSInternalData(
    const ISPInputData* pInputData
    ) const
{
    BpsIQSettings*  pBPSIQSettings = reinterpret_cast<BpsIQSettings*>(pInputData->pipelineBPSData.pIQSettings);
    CAMX_ASSERT(NULL != pBPSIQSettings);
    pBPSIQSettings->glutParameters.moduleCfg.EN = m_moduleEnable;

    ISPInternalData* pData = pInputData->pCalculatedData;
    if (NULL != m_pGammaG && NULL != pData)
    {
        for (UINT i = 0; i < NumberOfEntriesPerLUT; i++)
        {
            pData->gammaOutput.gammaG[i] = m_pGammaG[i] & GammaMask;
        }
        pData->gammaOutput.isGammaValid = TRUE;
    }

    // Post tuning metadata if setting is enabled
    if (NULL != pInputData->pBPSTuningMetadata)
    {
        CAMX_STATIC_ASSERT(BPSGammalLUTBufferSize <=
                          sizeof(pInputData->pBPSTuningMetadata->BPSDMIData.packedLUT.gamma));

        if (NULL != m_pGammaTuning[GammaLUTChannelR])
        {
            Utils::Memcpy(&pInputData->pBPSTuningMetadata->BPSDMIData.packedLUT.gamma[GammaLUTChannelR],
                          m_pGammaTuning[GammaLUTChannelR],
                          GammaSizeLUTInBytes);
        }

        if (NULL != m_pGammaTuning[GammaLUTChannelG])
        {
            Utils::Memcpy(&pInputData->pBPSTuningMetadata->BPSDMIData.packedLUT.gamma[GammaLUTChannelG],
                          m_pGammaTuning[GammaLUTChannelG],
                          GammaSizeLUTInBytes);
        }

        if (NULL != m_pGammaTuning[GammaLUTChannelB])
        {
            Utils::Memcpy(&pInputData->pBPSTuningMetadata->BPSDMIData.packedLUT.gamma[GammaLUTChannelB],
                          m_pGammaTuning[GammaLUTChannelB],
                          GammaSizeLUTInBytes);
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSGamma16::Execute
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSGamma16::Execute(
    ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;

    if (NULL != pInputData)
    {
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
// BPSGamma16::BPSGamma16
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BPSGamma16::BPSGamma16(
    const CHAR* pNodeIdentifier)
{
    m_pNodeIdentifier   = pNodeIdentifier;
    m_type              = ISPIQModuleType::BPSGamma;
    m_numLUT            = GammaLUTMax;
    m_cmdLength         = 0;
    m_moduleEnable      = FALSE;
    m_pGammaG           = NULL;
    m_pChromatix        = NULL;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSGamma16::~BPSGamma16
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BPSGamma16::~BPSGamma16()
{
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
// BPSGamma16::DumpRegConfig
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID BPSGamma16::DumpRegConfig(
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
        CamX::OsUtils::FPrintF(pFile, "********  BPS Gamma16 ********  \n");

        if (TRUE == m_moduleEnable)
        {
            pDMIData = static_cast<UINT32*>(pInputData->pBetDMIAddr);

            CamX::OsUtils::FPrintF(pFile, "* glut16Data.g_lut_in_cfg.gamma_table[2][64] = \n");
            for (UINT32 index = 0; index < DMIRAM_RGB_CH0_LENGTH_RGBLUT_V16; index++)
            {
                CamX::OsUtils::FPrintF(pFile, "           %d", static_cast<UINT32>(*(pDMIData + index)));
            }
            pDMIData += DMIRAM_RGB_CH0_LENGTH_RGBLUT_V16;
            CamX::OsUtils::FPrintF(pFile, "* glut16Data.b_lut_in_cfg.gamma_table[2][64] = \n");
            for (UINT32 index = 0; index < DMIRAM_RGB_CH0_LENGTH_RGBLUT_V16; index++)
            {
                CamX::OsUtils::FPrintF(pFile, "           %d", static_cast<UINT32>(*(pDMIData + index)));
            }
            pDMIData += DMIRAM_RGB_CH0_LENGTH_RGBLUT_V16;
            CamX::OsUtils::FPrintF(pFile, "* glut16Data.r_lut_in_cfg.gamma_table[2][64] = \n");
            for (UINT32 index = 0; index < DMIRAM_RGB_CH0_LENGTH_RGBLUT_V16; index++)
            {
                CamX::OsUtils::FPrintF(pFile, "           %d", static_cast<UINT32>(*(pDMIData + index)));
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
