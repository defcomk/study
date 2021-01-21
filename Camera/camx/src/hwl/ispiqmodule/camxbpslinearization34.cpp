////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017-2019 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxbpslinearization34.cpp
/// @brief BPSLinearization34 class implementation
///        Corrects nonlinearity of sensor response and also subtracts black level
///
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "bps_data.h"
#include "camxbpslinearization34.h"
#include "camxdefs.h"
#include "camxiqinterface.h"
#include "camxnode.h"
#include "camxtuningdatamanager.h"
#include "linearization_3_4_0.h"
#include "parametertuningtypes.h"

CAMX_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSLinearization34::Create
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSLinearization34::Create(
    BPSModuleCreateData* pCreateData)
{
    CamxResult result = CamxResultSuccess;

    if ((NULL != pCreateData) && (NULL != pCreateData->pNodeIdentifier))
    {
        BPSLinearization34* pModule = CAMX_NEW BPSLinearization34(pCreateData->pNodeIdentifier);

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
// BPSLinearization34::Initialize
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSLinearization34::Initialize()
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
// BPSLinearization34::FillCmdBufferManagerParams
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSLinearization34::FillCmdBufferManagerParams(
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
                                  (NULL != m_pNodeIdentifier) ? m_pNodeIdentifier : " ", "BPSLinearization34");
                pResourceParams->resourceSize                 = (BPSLinearization34LUTLengthDword * sizeof(UINT32));
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
// BPSLinearization34::AllocateCommonLibraryData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSLinearization34::AllocateCommonLibraryData()
{
    CamxResult result = CamxResultSuccess;

    UINT interpolationSize = (sizeof(linearization_3_4_0::linearization34_rgn_dataType) *
                              (Linearization34MaxmiumNonLeafNode + 1));

    if (NULL == m_dependenceData.pInterpolationData)
    {
        // Alloc for linearization_3_4_0::linearization34_rgn_dataType
        m_dependenceData.pInterpolationData = CAMX_CALLOC(interpolationSize);
        if (NULL == m_dependenceData.pInterpolationData)
        {
            result = CamxResultENoMemory;
        }
    }
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSLinearization34::CheckDependenceChange
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL BPSLinearization34::CheckDependenceChange(
    ISPInputData* pInputData)
{
    BOOL isChanged               = FALSE;

    if ((NULL != pInputData)                            &&
        (NULL != pInputData->pHwContext)                &&
        (NULL != pInputData->pHALTagsData)              &&
        (NULL != pInputData->pipelineBPSData.pIQSettings))
    {
        if (NULL != pInputData->pOEMIQSetting)
        {
            m_moduleEnable = (static_cast<OEMBPSIQSetting*>(pInputData->pOEMIQSetting))->LinearizationEnable;
            isChanged      = (TRUE == m_moduleEnable);
        }
        else
        {
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

                    m_pChromatix = pTuningManager->GetChromatix()->GetModule_linearization34_bps(
                                       reinterpret_cast<TuningMode*>(&pInputData->pTuningData->TuningMode[0]),
                                       pInputData->pTuningData->noOfSelectionParameter);
                }

                CAMX_ASSERT(NULL != m_pChromatix);
                if (NULL != m_pChromatix)
                {
                    if (m_pChromatix != m_dependenceData.pChromatix)
                    {
                        m_dependenceData.pChromatix      = m_pChromatix;
                        m_moduleEnable                   = m_pChromatix->enable_section.linearization_enable;
                        m_dependenceData.symbolIDChanged = TRUE;
                        isChanged                        = (TRUE == m_moduleEnable);
                    }
                }
                else
                {
                    CAMX_LOG_ERROR(CamxLogGroupISP, "Failed to get Chromatix");
                }
            }
        }

        if ((TRUE == m_moduleEnable)             &&
            (BlackLevelLockOn != m_blacklevelLock))
        {
            BpsIQSettings* pBPSIQSettings = reinterpret_cast<BpsIQSettings*>(pInputData->pipelineBPSData.pIQSettings);
            CamxResult     result         = IQInterface::GetPixelFormat(&pInputData->sensorData.format,
                                                                        &m_dependenceData.bayerPattern);

            m_dependenceData.pedestalEnable = pBPSIQSettings->pedestalParameters.moduleCfg.EN;

            if (TRUE == IQInterface::s_interpolationTable.BPSLinearization34TriggerUpdate(&(pInputData->triggerData),
                                                                                          &m_dependenceData))
            {
                isChanged = TRUE;
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
// BPSLinearization34::WriteLUTtoDMI
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSLinearization34::WriteLUTtoDMI(
    const ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;

    // CDM pack the DMI buffer and patch the Red and Blue LUT DMI buffer into CDM pack
    if ((NULL != pInputData) && (NULL != pInputData->pDMICmdBuffer))
    {
        UINT32 lengthInByte = BPSLinearization34LUTLengthDword * sizeof(UINT32);

        result = PacketBuilder::WriteDMI(pInputData->pDMICmdBuffer,
                                         regBPS_BPS_0_CLC_LINEARIZATION_DMI_CFG,
                                         LinearizationLUT,
                                         m_pLUTDMICmdBuffer,
                                         0,
                                         lengthInByte);

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
// BPSLinearization34::CreateCmdList
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSLinearization34::CreateCmdList(
    const ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;

    result = WriteLUTtoDMI(pInputData);

    if ((CamxResultSuccess == result) && (NULL != pInputData->pCmdBuffer))
    {
        CmdBuffer* pCmdBuffer = pInputData->pCmdBuffer;

        result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                              regBPS_BPS_0_CLC_LINEARIZATION_KNEEPOINT_R_0_CFG,
                                              (sizeof(BPSLinearization34RegConfig) / RegisterWidthInBytes),
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
// BPSLinearization34::FetchDMIBuffer
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSLinearization34::FetchDMIBuffer(
    const ISPInputData* pInputData)
{
    CamxResult      result          = CamxResultSuccess;
    PacketResource* pPacketResource = NULL;

    CAMX_UNREFERENCED_PARAM(pInputData);

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
// BPSLinearization34::RunCalculation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSLinearization34::RunCalculation(
    ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;

    if (NULL != pInputData)
    {
        result = FetchDMIBuffer(pInputData);

        if (CamxResultSuccess == result)
        {
            Linearization34OutputData outputData;

            outputData.pDMIDataPtr = reinterpret_cast<UINT32*>(
                                         m_pLUTDMICmdBuffer->BeginCommands(BPSLinearization34LUTLengthDword));
            // BET ONLY - InputData is different per module tested
            pInputData->pBetDMIAddr = static_cast<VOID*>(outputData.pDMIDataPtr);
            CAMX_ASSERT(NULL != outputData.pDMIDataPtr);

            /// @todo (CAMX-1791) Revisit this function to see if it needs to act differently in the Dual IFE case
            outputData.pRegCmd          = &m_regCmd;

            result = IQInterface::BPSLinearization34CalculateSetting(&m_dependenceData,
                                                                     pInputData->pOEMIQSetting,
                                                                     &outputData);
            pInputData->pCalculatedData->stretchGainBlue      = outputData.stretchGainBlue;
            pInputData->pCalculatedData->stretchGainRed       = outputData.stretchGainRed;
            pInputData->pCalculatedData->stretchGainGreenEven = outputData.stretchGainGreenEven;
            pInputData->pCalculatedData->stretchGainGreenOdd  = outputData.stretchGainGreenOdd;

            m_dynamicBlackLevel[0] = outputData.pRegCmd->kneepointR0Config.bitfields.P0;
            m_dynamicBlackLevel[1] = outputData.pRegCmd->kneepointGR0Config.bitfields.P0;
            m_dynamicBlackLevel[2] = outputData.pRegCmd->kneepointGB0Config.bitfields.P0;
            m_dynamicBlackLevel[3] = outputData.pRegCmd->kneepointB0Config.bitfields.P0;

            if (CamxResultSuccess == result)
            {
                m_pLUTDMICmdBuffer->CommitCommands();
                if (NULL != pInputData->pBPSTuningMetadata)
                {
                    m_pLUTDMIBuffer = outputData.pDMIDataPtr;
                }
                if (CamxLogGroupIQMod == (pInputData->dumpRegConfig & CamxLogGroupIQMod))
                {
                    DumpRegConfig(&outputData);
                }
            }
            else
            {
                CAMX_LOG_ERROR(CamxLogGroupISP, "Linearization Calculation Failed. result %d", result);
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
// BPSLinearization34::UpdateBPSInternalData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID BPSLinearization34::UpdateBPSInternalData(
    const ISPInputData* pInputData
    ) const
{
    BpsIQSettings* pBPSIQSettings = reinterpret_cast<BpsIQSettings*>(pInputData->pipelineBPSData.pIQSettings);
    CAMX_ASSERT(NULL != pBPSIQSettings);

    pBPSIQSettings->linearizationParameters.moduleCfg.EN = m_moduleEnable;

    pInputData->pCalculatedData->blackLevelLock = m_blacklevelLock;

    pInputData->pCalculatedData->linearizationAppliedBlackLevel[0] = m_dynamicBlackLevel[0];
    pInputData->pCalculatedData->linearizationAppliedBlackLevel[1] = m_dynamicBlackLevel[1];
    pInputData->pCalculatedData->linearizationAppliedBlackLevel[2] = m_dynamicBlackLevel[2];
    pInputData->pCalculatedData->linearizationAppliedBlackLevel[3] = m_dynamicBlackLevel[3];

    // Post tuning metadata if setting is enabled
    if (NULL != pInputData->pBPSTuningMetadata)
    {
        CAMX_STATIC_ASSERT(BPSLinearization34LUTLengthBytes <=
                          sizeof(pInputData->pBPSTuningMetadata->BPSDMIData.packedLUT.linearizationLUT));
        CAMX_STATIC_ASSERT(sizeof(BPSLinearization34RegConfig) <=
                          sizeof(pInputData->pBPSTuningMetadata->BPSLinearizationData));

        if (NULL != m_pLUTDMIBuffer)
        {
            Utils::Memcpy(&pInputData->pBPSTuningMetadata->BPSDMIData.packedLUT.linearizationLUT,
                          m_pLUTDMIBuffer,
                          BPSLinearization34LUTLengthBytes);
        }
        Utils::Memcpy(&pInputData->pBPSTuningMetadata->BPSLinearizationData,
                      &m_regCmd,
                      sizeof(BPSLinearization34RegConfig));
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSLinearization34::Execute
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSLinearization34::Execute(
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
        result = CamxResultEInvalidArg;
        CAMX_ASSERT_ALWAYS_MESSAGE("Null Input Pointer");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSLinearization34::DeallocateCommonLibraryData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID BPSLinearization34::DeallocateCommonLibraryData()
{
    if (NULL != m_dependenceData.pInterpolationData)
    {
        CAMX_FREE(m_dependenceData.pInterpolationData);
        m_dependenceData.pInterpolationData = NULL;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSLinearization34::BPSLinearization34
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BPSLinearization34::BPSLinearization34(
    const CHAR* pNodeIdentifier)
{
    m_pNodeIdentifier = pNodeIdentifier;
    m_type            = ISPIQModuleType::BPSLinearization;
    m_moduleEnable    = FALSE;
    m_cmdLength       = PacketBuilder::RequiredWriteRegRangeSizeInDwords(BPSLinearization34RegLengthDWord);
    m_numLUT          = LinearizationMaxLUT;
    m_pLUTDMIBuffer   = NULL;
    m_pChromatix      = NULL;
    m_AWBLock         = ControlAWBLockOff;
    m_blacklevelLock  = BlackLevelLockOff;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSLinearization34::~BPSLinearization34
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BPSLinearization34::~BPSLinearization34()
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
// BPSLinearization34::DumpRegConfig
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID BPSLinearization34::DumpRegConfig(
    const Linearization34OutputData* pOutputData
    ) const
{
    CHAR                      dumpFilename[256];
    FILE*                     pFile = NULL;
    UINT32                    DMICnt = 0;
    UINT                      maxSlope      = 9;
    CamX::OsUtils::SNPrintF(dumpFilename, sizeof(dumpFilename), "%s/BPS_regdump.txt", ConfigFileDirectory);
    pFile = CamX::OsUtils::FOpen(dumpFilename, "a+");

    if (NULL != pFile)
    {
        CamX::OsUtils::FPrintF(pFile, "******** BPS Linearization34 ********\n");
        CamX::OsUtils::FPrintF(pFile, "* Linearization34 CFG [HEX] \n");
        CamX::OsUtils::FPrintF(pFile, "Linearizatioin34.kneepointR0Config  = %x\n", m_regCmd.kneepointR0Config);
        CamX::OsUtils::FPrintF(pFile, "Linearizatioin34.kneepointR10Config = %x\n", m_regCmd.kneepointR1Config);
        CamX::OsUtils::FPrintF(pFile, "Linearizatioin34.kneepointR2Config  = %x\n", m_regCmd.kneepointR2Config);
        CamX::OsUtils::FPrintF(pFile, "Linearizatioin34.kneepointR3Config  = %x\n", m_regCmd.kneepointR3Config);
        CamX::OsUtils::FPrintF(pFile, "Linearizatioin34.kneepointGR0Config = %x\n", m_regCmd.kneepointGR0Config);
        CamX::OsUtils::FPrintF(pFile, "Linearizatioin34.kneepointGR1Config = %x\n", m_regCmd.kneepointGR1Config);
        CamX::OsUtils::FPrintF(pFile, "Linearizatioin34.kneepointGR2Config = %x\n", m_regCmd.kneepointGR2Config);
        CamX::OsUtils::FPrintF(pFile, "Linearizatioin34.kneepointGR3Config = %x\n", m_regCmd.kneepointGR3Config);
        CamX::OsUtils::FPrintF(pFile, "Linearizatioin34.kneepointB0Config  = %x\n", m_regCmd.kneepointB0Config);
        CamX::OsUtils::FPrintF(pFile, "Linearizatioin34.kneepointB1Config  = %x\n", m_regCmd.kneepointB1Config);
        CamX::OsUtils::FPrintF(pFile, "Linearizatioin34.kneepointB2Config  = %x\n", m_regCmd.kneepointB2Config);
        CamX::OsUtils::FPrintF(pFile, "Linearizatioin34.kneepointB3Config  = %x\n", m_regCmd.kneepointB3Config);
        CamX::OsUtils::FPrintF(pFile, "Linearizatioin34.kneepointGB0Config = %x\n", m_regCmd.kneepointGB0Config);
        CamX::OsUtils::FPrintF(pFile, "Linearizatioin34.kneepointGB1Config = %x\n", m_regCmd.kneepointGB1Config);
        CamX::OsUtils::FPrintF(pFile, "Linearizatioin34.kneepointGB2Config = %x\n", m_regCmd.kneepointGB2Config);
        CamX::OsUtils::FPrintF(pFile, "Linearizatioin34.kneepointGB3Config = %x\n", m_regCmd.kneepointGB3Config);

        if (0 != m_moduleEnable)
        {
            CamX::OsUtils::FPrintF(pFile, "* lin34Data.r_lut_base_l[2][9] = \n");
            for (UINT32 index = 0; index < maxSlope; index++, DMICnt += 4)
            {
                CamX::OsUtils::FPrintF(pFile, "           %d",
                    static_cast<UINT16>(*(pOutputData->pDMIDataPtr + DMICnt) & Utils::AllOnes32(14)));
            }
            CamX::OsUtils::FPrintF(pFile, "\n* lin34Data.r_lut_delta_l[2][9] = \n");
            for (UINT32 index = 0; index < maxSlope; index++, DMICnt += 4)
            {
                CamX::OsUtils::FPrintF(pFile, "           %d",
                    static_cast<UINT16>((*(pOutputData->pDMIDataPtr + DMICnt) >> 14) & Utils::AllOnes32(14)));
            }
            CamX::OsUtils::FPrintF(pFile, "\n* lin34Data.gr_lut_base_l[2][9] = \n");
            for (UINT32 index = 0; index < maxSlope; index++, DMICnt += 4)
            {
                CamX::OsUtils::FPrintF(pFile, "           %d",
                    static_cast<UINT16>(*(pOutputData->pDMIDataPtr + (DMICnt + 1)) & Utils::AllOnes32(14)));
            }
            CamX::OsUtils::FPrintF(pFile, "\n* lin34Data.gr_lut_delta_l[2][9] = \n");
            for (UINT32 index = 0; index < maxSlope; index++, DMICnt += 4)
            {
                CamX::OsUtils::FPrintF(pFile, "           %d",
                    static_cast<UINT16>((*(pOutputData->pDMIDataPtr + (DMICnt + 1)) >> 14) & Utils::AllOnes32(14)));
            }
            CamX::OsUtils::FPrintF(pFile, "\n* lin34Data.gb_lut_base_l[2][9] = \n");
            for (UINT32 index = 0; index < maxSlope; index++, DMICnt += 4)
            {
                CamX::OsUtils::FPrintF(pFile, "           %d",
                    static_cast<UINT16>(*(pOutputData->pDMIDataPtr + (DMICnt + 2)) & Utils::AllOnes32(14)));
            }
            CamX::OsUtils::FPrintF(pFile, "\n* lin34Data.gb_lut_delta_l[2][9] = \n");
            for (UINT32 index = 0; index < maxSlope; index++, DMICnt += 4)
            {
                CamX::OsUtils::FPrintF(pFile, "           %d",
                    static_cast<UINT16>((*(pOutputData->pDMIDataPtr + (DMICnt + 2)) >> 14) & Utils::AllOnes32(14)));
            }
            CamX::OsUtils::FPrintF(pFile, "\n* lin34Data.b_lut_base_l[2][9] = \n");
            for (UINT32 index = 0; index < maxSlope; index++, DMICnt += 4)
            {
                CamX::OsUtils::FPrintF(pFile, "           %d",
                    static_cast<UINT16>(*(pOutputData->pDMIDataPtr + (DMICnt + 3)) & Utils::AllOnes32(14)));
            }
            CamX::OsUtils::FPrintF(pFile, "\n* lin34Data.b_lut_delta_l[2][9] = \n");
            for (UINT32 index = 0; index < maxSlope; index++, DMICnt += 4)
            {
                CamX::OsUtils::FPrintF(pFile, "           %d",
                    static_cast<UINT16>((*(pOutputData->pDMIDataPtr + (DMICnt + 3)) >> 14) & Utils::AllOnes32(14)));
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
