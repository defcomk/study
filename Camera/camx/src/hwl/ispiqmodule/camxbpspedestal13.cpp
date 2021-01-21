////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017-2019 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxbpspedestal13.cpp
/// @brief CAMXBPSPEDESTAL13 class implementation
///        Corrects spatially non-uniform black levels from the pixel data by programmable meshes and optimally scale
///        the pedestal corrected pixel data to maximum dynamic range.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "camxbpspedestal13.h"
#include "camxiqinterface.h"
#include "camxnode.h"
#include "camxtuningdatamanager.h"
#include "parametertuningtypes.h"
#include "pedestal13setting.h"

CAMX_NAMESPACE_BEGIN

static const UINT32 RedLUT                       = 0x1;
static const UINT32 BlueLUT                      = 0x2;
static const UINT32 MaxPedestalLUT               = 2;

static const UINT8  BPSPedestal13NumDMITables    = 2;
static const UINT32 BPSPedestal13DMISetSizeDword = 130;   // DMI LUT table has 130 entries
static const UINT32 BPSPedestal13LUTTableSize    = BPSPedestal13DMISetSizeDword * sizeof(UINT32);
static const UINT32 BPSPedestal13DMILengthDword  = BPSPedestal13DMISetSizeDword * BPSPedestal13NumDMITables;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSPedestal13::Create
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSPedestal13::Create(
    BPSModuleCreateData* pCreateData)
{
    CamxResult result = CamxResultSuccess;

    if ((NULL != pCreateData) && (NULL != pCreateData->pNodeIdentifier))
    {
        BPSPedestal13* pModule = CAMX_NEW BPSPedestal13(pCreateData->pNodeIdentifier);

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
// BPSPedestal13::Initialize
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSPedestal13::Initialize()
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
// BPSPedestal13::FillCmdBufferManagerParams
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSPedestal13::FillCmdBufferManagerParams(
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
                                  (NULL != m_pNodeIdentifier) ? m_pNodeIdentifier : " ", "BPSPedestal13");
                pResourceParams->resourceSize                 = (BPSPedestal13DMILengthDword * sizeof(UINT32));
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
// BPSPedestal13::AllocateCommonLibraryData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSPedestal13::AllocateCommonLibraryData()
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
// BPSPedestal13::CheckDependenceChange
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL BPSPedestal13::CheckDependenceChange(
    ISPInputData* pInputData)
{
    BOOL isChanged = FALSE;

    if ((NULL != pInputData)               &&
        (NULL != pInputData->pHwContext)   &&
        (NULL != pInputData->pHALTagsData))
    {
        if (NULL != pInputData->pOEMIQSetting)
        {
            m_moduleEnable = (static_cast<OEMBPSIQSetting*>(pInputData->pOEMIQSetting))->PedestalEnable;
            isChanged      = (TRUE == m_moduleEnable);
        }
        else
        {
            m_blacklevelLock = pInputData->pHALTagsData->blackLevelLock;
            if (BlackLevelLockOn == m_blacklevelLock)
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

                    m_pChromatix = pTuningManager->GetChromatix()->GetModule_pedestal13_bps(
                                       reinterpret_cast<TuningMode*>(&pInputData->pTuningData->TuningMode[0]),
                                       pInputData->pTuningData->noOfSelectionParameter);
                }

                CAMX_ASSERT(NULL != m_pChromatix);
                if (NULL != m_pChromatix)
                {
                    if (m_pChromatix != m_dependenceData.pChromatix)
                    {
                        m_dependenceData.pChromatix      = m_pChromatix;
                        m_moduleEnable                   = m_pChromatix->enable_section.pedestal_enable;
                        m_dependenceData.symbolIDChanged = TRUE;
                        isChanged                        = (TRUE == m_moduleEnable);
                    }
                    else
                    {
                        CAMX_LOG_ERROR(CamxLogGroupISP, "Failed to get Chromatix");
                    }
                }
            }
        }

        CamxResult result = IQInterface::GetPixelFormat(&pInputData->sensorData.format, &m_dependenceData.bayerPattern);

        if ((TRUE             == m_moduleEnable) &&
            (BlackLevelLockOn != m_blacklevelLock))
        {
            if (TRUE == IQInterface::s_interpolationTable.pedestal13TriggerUpdate(&(pInputData->triggerData),
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
                           "Invalid Input: pHwContext:%p",
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
// BPSPedestal13::WriteLUTtoDMI
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSPedestal13::WriteLUTtoDMI(
    const ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;

    // CDM pack the DMI buffer and patch the Red and Blue LUT DMI buffer into CDM pack
    if ((NULL != pInputData) && (NULL != pInputData->pDMICmdBuffer))
    {
        UINT32 LUTOffset = 0;
        result = PacketBuilder::WriteDMI(pInputData->pDMICmdBuffer,
                                         regBPS_BPS_0_CLC_PEDESTAL_DMI_CFG,
                                         RedLUT,
                                         m_pLUTDMICmdBuffer,
                                         LUTOffset,
                                         BPSPedestal13DMISetSizeDword * sizeof(UINT32));
        if (CamxResultSuccess == result)
        {
            LUTOffset = (BPSPedestal13DMISetSizeDword * sizeof(UINT32));
            result = PacketBuilder::WriteDMI(pInputData->pDMICmdBuffer,
                                             regBPS_BPS_0_CLC_PEDESTAL_DMI_CFG,
                                             BlueLUT,
                                             m_pLUTDMICmdBuffer,
                                             LUTOffset,
                                             BPSPedestal13DMISetSizeDword * sizeof(UINT32));
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
// BPSPedestal13::CreateCmdList
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSPedestal13::CreateCmdList(
    const ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;

    result = WriteLUTtoDMI(pInputData);

    if ((CamxResultSuccess == result) && (NULL != pInputData->pCmdBuffer))
    {
        CmdBuffer* pCmdBuffer = pInputData->pCmdBuffer;

        result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                              regBPS_BPS_0_CLC_PEDESTAL_MODULE_1_CFG,
                                              (sizeof(BPSPedestal13RegConfig) / RegisterWidthInBytes),
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
// BPSPedestal13::FetchDMIBuffer
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSPedestal13::FetchDMIBuffer()
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
// BPSPedestal13::RunCalculation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSPedestal13::RunCalculation(
    ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;

    if (NULL != pInputData)
    {
        result = FetchDMIBuffer();

        if (CamxResultSuccess == result)
        {
            BpsIQSettings*       pBPSIQSettings = reinterpret_cast<BpsIQSettings*>(pInputData->pipelineBPSData.pIQSettings);
            Pedestal13OutputData outputData;

            outputData.type                               = PipelineType::BPS;
            outputData.regCommand.BPS.pRegBPSCmd          = &m_regCmd;
            outputData.regCommand.BPS.pPedestalParameters = &pBPSIQSettings->pedestalParameters;

            outputData.pGRRLUTDMIBuffer =
                reinterpret_cast<UINT32*>(m_pLUTDMICmdBuffer->BeginCommands(BPSPedestal13DMILengthDword));
            CAMX_ASSERT(NULL != outputData.pGRRLUTDMIBuffer);
            // BET ONLY - InputData is different per module tested
            pInputData->pBetDMIAddr = static_cast<VOID*>(outputData.pGRRLUTDMIBuffer);

            outputData.pGBBLUTDMIBuffer =
                reinterpret_cast<UINT32*>(reinterpret_cast<UCHAR*>(outputData.pGRRLUTDMIBuffer) + BPSPedestal13LUTTableSize);
            CAMX_ASSERT(NULL != outputData.pGBBLUTDMIBuffer);

            result = IQInterface::Pedestal13CalculateSetting(&m_dependenceData, pInputData->pOEMIQSetting, &outputData);
            if (CamxResultSuccess == result)
            {
                m_pLUTDMICmdBuffer->CommitCommands();

                if (NULL != pInputData->pBPSTuningMetadata)
                {
                    m_pGRRLUTDMIBuffer  = outputData.pGRRLUTDMIBuffer;
                    m_pGBBLUTDMIBuffer  = outputData.pGBBLUTDMIBuffer;
                }
            }
            else
            {
                CAMX_LOG_ERROR(CamxLogGroupISP, "Pedestal Calculation Failed %d", result);
            }
        }
        if (CamxLogGroupIQMod == (pInputData->dumpRegConfig & CamxLogGroupIQMod))
        {
            DumpRegConfig(pInputData);
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
// BPSPedestal13::UpdateBPSInternalData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID BPSPedestal13::UpdateBPSInternalData(
    const ISPInputData* pInputData
    ) const
{
    BpsIQSettings* pBPSIQSettings = reinterpret_cast<BpsIQSettings*>(pInputData->pipelineBPSData.pIQSettings);

    pBPSIQSettings->pedestalParameters.moduleCfg.EN = m_moduleEnable;


    pInputData->pCalculatedData->blackLevelLock = m_blacklevelLock;

    // Post tuning metadata if setting is enabled
    if (NULL != pInputData->pBPSTuningMetadata)
    {
        CAMX_STATIC_ASSERT(BPSPedestal13LUTTableSize <=
                          sizeof(pInputData->pBPSTuningMetadata->BPSDMIData.packedLUT.pedestalLUT.GRRLUT));
        CAMX_STATIC_ASSERT(sizeof(BPSPedestal13RegConfig) <= sizeof(pInputData->pBPSTuningMetadata->BPSPedestalData));

        if (NULL != m_pGRRLUTDMIBuffer)
        {
            Utils::Memcpy(&pInputData->pBPSTuningMetadata->BPSDMIData.packedLUT.pedestalLUT.GRRLUT[0],
                          m_pGRRLUTDMIBuffer,
                          BPSPedestal13LUTTableSize);
        }
        if (NULL != m_pGBBLUTDMIBuffer)
        {
            Utils::Memcpy(&pInputData->pBPSTuningMetadata->BPSDMIData.packedLUT.pedestalLUT.GBBLUT[0],
                          m_pGBBLUTDMIBuffer,
                          BPSPedestal13LUTTableSize);
        }
        Utils::Memcpy(&pInputData->pBPSTuningMetadata->BPSPedestalData.pedestalConfig,
                      &m_regCmd,
                      sizeof(BPSPedestal13RegConfig));
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSPedestal13::Execute
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSPedestal13::Execute(
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
// BPSPedestal13::DeallocateCommonLibraryData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID BPSPedestal13::DeallocateCommonLibraryData()
{
    if (NULL != m_dependenceData.pInterpolationData)
    {
        CAMX_FREE(m_dependenceData.pInterpolationData);
        m_dependenceData.pInterpolationData = NULL;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSPedestal13::BPSPedestal13
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BPSPedestal13::BPSPedestal13(
    const CHAR* pNodeIdentifier)
{
    m_pNodeIdentifier     = pNodeIdentifier;
    m_type                = ISPIQModuleType::BPSPedestalCorrection;
    m_moduleEnable        = FALSE;
    m_numLUT              = MaxPedestalLUT;

    m_cmdLength = PacketBuilder::RequiredWriteRegRangeSizeInDwords(sizeof(BPSPedestal13RegConfig) / RegisterWidthInBytes);

    m_pGRRLUTDMIBuffer = NULL;
    m_pGBBLUTDMIBuffer = NULL;
    m_pChromatix       = NULL;

    m_blacklevelLock = BlackLevelLockOff;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSPedestal13::~BPSPedestal13
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BPSPedestal13::~BPSPedestal13()
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
    DeallocateCommonLibraryData();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSPedestal13::DumpRegConfig
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID BPSPedestal13::DumpRegConfig(
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
        CamX::OsUtils::FPrintF(pFile, "******** BPS Pedestal13 ********\n");
        CamX::OsUtils::FPrintF(pFile, "* Pedestal13 CFG [HEX] \n");
        CamX::OsUtils::FPrintF(pFile, "module 1 Config = %x\n", m_regCmd.module1Config);
        CamX::OsUtils::FPrintF(pFile, "module 2 Config = %x\n", m_regCmd.module2Config);
        CamX::OsUtils::FPrintF(pFile, "module 3 Config = %x\n", m_regCmd.module3Config);
        CamX::OsUtils::FPrintF(pFile, "module 4 Config = %x\n", m_regCmd.module4Config);
        CamX::OsUtils::FPrintF(pFile, "module 5 Config = %x\n", m_regCmd.module5Config);

        if (TRUE == m_moduleEnable)
        {
            UINT32 DMICnt = 0;
            pDMIData = static_cast<UINT32*>(pInputData->pBetDMIAddr);

            CamX::OsUtils::FPrintF(pFile, "* channel[0] = \n");
            CamX::OsUtils::FPrintF(pFile, "    ** channel[0][0] = \n");
            for (UINT32 verMeshNum = 0; verMeshNum < PED_MESH_PT_V_V13; verMeshNum++)
            {
                for (UINT32 horMeshNum = 0; horMeshNum < PED_MESH_PT_H_V13; horMeshNum++)
                {
                    CamX::OsUtils::FPrintF(pFile, "           %d",
                        ((*(pDMIData + DMICnt)) & Utils::AllOnes32(12)));
                    DMICnt++;
                }
                CamX::OsUtils::FPrintF(pFile, "\n");
            }
            DMICnt = 0;
            CamX::OsUtils::FPrintF(pFile, "    ** channel[0][1] = \n");
            for (UINT32 verMeshNum = 0; verMeshNum < PED_MESH_PT_V_V13; verMeshNum++)
            {
                for (UINT32 horMeshNum = 0; horMeshNum < PED_MESH_PT_H_V13; horMeshNum++)
                {
                    CamX::OsUtils::FPrintF(pFile, "           %d",
                        ((*(pDMIData + DMICnt)) >> 12) & Utils::AllOnes32(12));
                    DMICnt++;
                }
                CamX::OsUtils::FPrintF(pFile, "\n");
            }
            DMICnt = 0;
            CamX::OsUtils::FPrintF(pFile, "    ** channel[0][2] = \n");
            for (UINT32 verMeshNum = 0; verMeshNum < PED_MESH_PT_V_V13; verMeshNum++)
            {
                for (UINT32 horMeshNum = 0; horMeshNum < PED_MESH_PT_H_V13; horMeshNum++)
                {
                    CamX::OsUtils::FPrintF(pFile, "           %d",
                        (((*(pDMIData + PED_LUT_LENGTH + DMICnt)) >> 12) & Utils::AllOnes32(12)));
                    DMICnt++;
                }
                CamX::OsUtils::FPrintF(pFile, "\n");
            }
            DMICnt = 0;
            CamX::OsUtils::FPrintF(pFile, "    ** channel[0][3] = \n");
            for (UINT32 verMeshNum = 0; verMeshNum < PED_MESH_PT_V_V13; verMeshNum++)
            {
                for (UINT32 horMeshNum = 0; horMeshNum < PED_MESH_PT_H_V13; horMeshNum++)
                {
                    CamX::OsUtils::FPrintF(pFile, "           %d",
                        ((*(pDMIData + PED_LUT_LENGTH + DMICnt)) & Utils::AllOnes32(12)));
                    DMICnt++;
                }
                CamX::OsUtils::FPrintF(pFile, "\n");
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
