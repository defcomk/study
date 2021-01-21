////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017-2018 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxipechromaenhancement12.cpp
/// @brief IPEChromaEnhancement class implementation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "camxipechromaenhancement12.h"
#include "camxiqinterface.h"
#include "camxnode.h"
#include "camxtuningdatamanager.h"
#include "ipe_data.h"
#include "parametertuningtypes.h"

CAMX_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPEChromaEnhancement12::Create
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPEChromaEnhancement12::Create(
    IPEModuleCreateData* pCreateData)
{
    CamxResult result = CamxResultSuccess;

    if ((NULL != pCreateData) && (NULL != pCreateData->pNodeIdentifier))
    {
        IPEChromaEnhancement12* pModule = CAMX_NEW IPEChromaEnhancement12(pCreateData->pNodeIdentifier);

        if (NULL != pModule)
        {
            result = pModule->Initialize();
            if (result != CamxResultSuccess)
            {
                CAMX_LOG_ERROR(CamxLogGroupPProc, "Unable to initilize LUTCmdBufferManager, no memory");
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
        CAMX_LOG_ERROR(CamxLogGroupPProc, "Null input pointer");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPEChromaEnhancement12::Initialize
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPEChromaEnhancement12::Initialize()
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
// IPEChromaEnhancement12::AllocateCommonLibraryData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPEChromaEnhancement12::AllocateCommonLibraryData()
{
    CamxResult result = CamxResultSuccess;

    UINT interpolationSize = (sizeof(cv_1_2_0::cv12_rgn_dataType) * (CV12MaxNonLeafNode + 1));

    if (NULL == m_dependenceData.pInterpolationData)
    {
        // Alloc for cv_1_2_0::cv12_rgn_dataType
        m_dependenceData.pInterpolationData = CAMX_CALLOC(interpolationSize);
        if (NULL == m_dependenceData.pInterpolationData)
        {
            result = CamxResultENoMemory;
        }
    }
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPEChromaEnhancement12::UpdateIPEInternalData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPEChromaEnhancement12::UpdateIPEInternalData(
    const ISPInputData* pInputData
    ) const
{
    CamxResult      result          = CamxResultSuccess;
    IpeIQSettings*  pIPEIQSettings  = reinterpret_cast<IpeIQSettings*>(pInputData->pipelineIPEData.pIPEIQSettings);

    pIPEIQSettings->chromaEnhancementParameters.moduleCfg.EN = m_moduleEnable;
    /// @todo (CAMX-738) Add metadata information

    // Post tuning metadata if setting is enabled
    if (NULL != pInputData->pIPETuningMetadata)
    {
        CAMX_STATIC_ASSERT(sizeof(IPEChromaEnhancementRegCmd) <=
                           sizeof(pInputData->pIPETuningMetadata->IPEChromaEnhancementData.CEConfig));

        Utils::Memcpy(&pInputData->pIPETuningMetadata->IPEChromaEnhancementData.CEConfig,
                      &m_regCmd,
                      sizeof(IPEChromaEnhancementRegCmd));
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPEChromaEnhancement12::CreateCmdList
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPEChromaEnhancement12::CreateCmdList(
    const ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;

    if (NULL != pInputData)
    {
        CmdBuffer* pCmdBuffer = pInputData->pipelineIPEData.ppIPECmdBuffer[CmdBufferPostLTM];

        if (NULL != pCmdBuffer)
        {
            result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                                  regIPE_IPE_0_PPS_CLC_CHROMA_ENHAN_CHROMA_ENHAN_LUMA_CFG_0,
                                                  IPEChromaEnhancementRegLength,
                                                  reinterpret_cast<UINT32*>(&m_regCmd));
            CAMX_ASSERT(CamxResultSuccess == result);

            if (CamxResultSuccess != result)
            {
                CAMX_ASSERT_ALWAYS_MESSAGE("Failed to write Chroma Enhancement config. data");
            }
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPEChromaEnhancement12::ValidateDependenceParams
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPEChromaEnhancement12::ValidateDependenceParams(
    const ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;

    /// @todo (CAMX-730) validate dependency parameters
    if ((NULL == pInputData)                                                   ||
        (NULL == pInputData->pipelineIPEData.ppIPECmdBuffer[CmdBufferPostLTM]) ||
        (NULL == pInputData->pipelineIPEData.pIPEIQSettings))
    {
        result = CamxResultEInvalidArg;
        CAMX_ASSERT_ALWAYS_MESSAGE("Invalid Input data or command buffer");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPEChromaEnhancement12::GetRegCmd
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID* IPEChromaEnhancement12::GetRegCmd()
{
    return &m_regCmd;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPEChromaEnhancement12::CheckDependenceChange
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL IPEChromaEnhancement12::CheckDependenceChange(
    ISPInputData* pInputData)
{
    BOOL isChanged = FALSE;

    if ((NULL != pInputData)                  &&
        (NULL != pInputData->pAECUpdateData)  &&
        (NULL != pInputData->pAWBUpdateData)  &&
        (NULL != pInputData->pHwContext))
    {
        /// @todo (CAMX-730) Check module dependency
        CAMX_LOG_VERBOSE(CamxLogGroupPProc, "Lux Index [0x%f] ", pInputData->pAECUpdateData->luxIndex);

        if (NULL != pInputData->pOEMIQSetting)
        {
            m_moduleEnable = (static_cast<OEMIPEIQSetting*>(pInputData->pOEMIQSetting))->ChromaEnhancementEnable;

            isChanged      = (TRUE == m_moduleEnable);
        }
        else
        {
            if (NULL != pInputData->pTuningData)
            {
                TuningDataManager* pTuningManager = pInputData->pTuningDataManager;
                CAMX_ASSERT(NULL != pTuningManager);

                // Search through the tuning data (tree), only when there
                // are changes to the tuning mode data as an optimization
                if ((TRUE == pInputData->tuningModeChanged)    &&
                    (TRUE == pTuningManager->IsValidChromatix()))
                {
                    m_pChromatix = pTuningManager->GetChromatix()->GetModule_cv12_ipe(
                                       reinterpret_cast<TuningMode*>(&pInputData->pTuningData->TuningMode[0]),
                                       pInputData->pTuningData->noOfSelectionParameter);
                }

                CAMX_ASSERT(NULL != m_pChromatix);
                if (NULL != m_pChromatix)
                {
                    if (m_pChromatix != m_dependenceData.pChromatix)
                    {
                        CAMX_LOG_VERBOSE(CamxLogGroupPProc, "updating chromatix pointer");
                        m_dependenceData.pChromatix = m_pChromatix;
                        m_moduleEnable              = m_pChromatix->enable_section.cv_enable;

                        isChanged                   = (TRUE == m_moduleEnable);
                    }
                }
                else
                {
                    CAMX_LOG_ERROR(CamxLogGroupPProc, "Failed to get Chromatix");
                }
            }
            else
            {
                CAMX_LOG_ERROR(CamxLogGroupPProc, "Failed to get Tuning Pointer");
            }
        }

        if (TRUE == m_moduleEnable)
        {
            if (TRUE ==
                IQInterface::s_interpolationTable.CV12TriggerUpdate(&pInputData->triggerData, &m_dependenceData))
            {
                isChanged = TRUE;
            }
        }
    }
    else
    {
        if (NULL != pInputData)
        {
            CAMX_LOG_ERROR(CamxLogGroupPProc,
                           "Invalid Input: pNewAECUpdate %p pNewAWBUpdate %p pHwContext %p",
                           pInputData->pAECUpdateData,
                           pInputData->pAWBUpdateData,
                           pInputData->pHwContext);
        }
        else
        {
            CAMX_LOG_ERROR(CamxLogGroupPProc, "Null Input Pointer");
        }
    }

    return isChanged;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPEChromaEnhancement12::RunCalculation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPEChromaEnhancement12::RunCalculation(
    const ISPInputData* pInputData)
{
    CamxResult     result = CamxResultSuccess;
    CV12OutputData outputData = { 0 };

    CAMX_LOG_VERBOSE(CamxLogGroupPProc, "Lux Index [0x%f] ", pInputData->pAECUpdateData->luxIndex);

    outputData.pRegCmd = &m_regCmd;
    result             = IQInterface::IPECV12CalculateSetting(&m_dependenceData, pInputData->pOEMIQSetting, &outputData);

    if (CamxResultSuccess != result)
    {
        CAMX_LOG_ERROR(CamxLogGroupPProc, "IPE CV12 Calculation Failed. result %d", result);
    }

    if (CamxLogGroupIQMod == (pInputData->dumpRegConfig & CamxLogGroupIQMod))
    {
        DumpRegConfig();
    }

    return result;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPEChromaEnhancement12::Execute
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPEChromaEnhancement12::Execute(
    ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;

    if (NULL != pInputData)
    {
        // Check if dependency is published and valid
        result = ValidateDependenceParams(pInputData);

        if ((CamxResultSuccess == result) && (TRUE == m_moduleEnable))
        {
            if (TRUE == CheckDependenceChange(pInputData))
            {
                result = RunCalculation(pInputData);
            }

            // Regardless of any update in dependency parameters, command buffers and IQSettings/Metadata shall be updated.
            if ((CamxResultSuccess == result) && (TRUE == m_moduleEnable))
            {
                result = CreateCmdList(pInputData);
            }

            if (CamxResultSuccess == result)
            {
                result = UpdateIPEInternalData(pInputData);
            }

            if (CamxResultSuccess != result)
            {
                CAMX_ASSERT_ALWAYS_MESSAGE("Operation failed %d", result);
            }
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
// IPEChromaEnhancement12::DeallocateCommonLibraryData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IPEChromaEnhancement12::DeallocateCommonLibraryData()
{
    if (NULL != m_dependenceData.pInterpolationData)
    {
        CAMX_FREE(m_dependenceData.pInterpolationData);
        m_dependenceData.pInterpolationData = NULL;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPEChromaEnhancement12::IPEChromaEnhancement12
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IPEChromaEnhancement12::IPEChromaEnhancement12(
    const CHAR* pNodeIdentifier)
{
    UINT cmdSize = PacketBuilder::RequiredWriteRegRangeSizeInDwords(sizeof(IPEChromaEnhancementRegCmd) / RegisterWidthInBytes);

    m_pNodeIdentifier   = pNodeIdentifier;
    m_type              = ISPIQModuleType::IPEChromaEnhancement;
    m_moduleEnable      = TRUE;
    m_cmdLength         = cmdSize * sizeof(UINT32);
    m_numLUT            = 0;
    m_offsetLUT         = 0;
    m_pChromatix        = NULL;

    CAMX_LOG_VERBOSE(CamxLogGroupPProc, "IPE Chroma Enhancement m_cmdLength %d ", m_cmdLength);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IPEChromaEnhancement12::~IPEChromaEnhancement12
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IPEChromaEnhancement12::~IPEChromaEnhancement12()
{
    m_pChromatix = NULL;
    DeallocateCommonLibraryData();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPEChromaEnhancement12::DumpRegConfig
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IPEChromaEnhancement12::DumpRegConfig() const
{
    CHAR  dumpFilename[256];
    FILE* pFile = NULL;
    CamX::OsUtils::SNPrintF(dumpFilename, sizeof(dumpFilename), "%s/IPE_regdump.txt", ConfigFileDirectory);
    pFile = CamX::OsUtils::FOpen(dumpFilename, "a+");

    if (NULL != pFile)
    {
        CamX::OsUtils::FPrintF(pFile, "******** IPE Chroma Enhancement 12 ********\n");
        CamX::OsUtils::FPrintF(pFile, "* Enhancement 12 CFG [HEX] \n");
        CamX::OsUtils::FPrintF(pFile, "ChromaEnhancement.V0  = %x\n", m_regCmd.lumaConfig0.bitfields.V0);
        CamX::OsUtils::FPrintF(pFile, "ChromaEnhancement.V1  = %x\n", m_regCmd.lumaConfig0.bitfields.V1);
        CamX::OsUtils::FPrintF(pFile, "ChromaEnhancement.V2  = %x\n", m_regCmd.lumaConfig1.bitfields.V2);
        CamX::OsUtils::FPrintF(pFile, "ChromaEnhancement.K   = %x\n", m_regCmd.lumaConfig1.bitfields.K);

        CamX::OsUtils::FPrintF(pFile, "ChromaEnhancement.AM  = %x\n", m_regCmd.aConfig.bitfields.AM);
        CamX::OsUtils::FPrintF(pFile, "ChromaEnhancement.AP  = %x\n", m_regCmd.aConfig.bitfields.AP);
        CamX::OsUtils::FPrintF(pFile, "ChromaEnhancement.BM  = %x\n", m_regCmd.bConfig.bitfields.BM);
        CamX::OsUtils::FPrintF(pFile, "ChromaEnhancement.BP  = %x\n", m_regCmd.bConfig.bitfields.BP);
        CamX::OsUtils::FPrintF(pFile, "ChromaEnhancement.CM  = %x\n", m_regCmd.cConfig.bitfields.CM);
        CamX::OsUtils::FPrintF(pFile, "ChromaEnhancement.CP  = %x\n", m_regCmd.cConfig.bitfields.CP);
        CamX::OsUtils::FPrintF(pFile, "ChromaEnhancement.DM  = %x\n", m_regCmd.dConfig.bitfields.DM);
        CamX::OsUtils::FPrintF(pFile, "ChromaEnhancement.DP  = %x\n", m_regCmd.dConfig.bitfields.DP);
        CamX::OsUtils::FPrintF(pFile, "ChromaEnhancement.KCB = %x\n", m_regCmd.cbConfig.bitfields.KCB);
        CamX::OsUtils::FPrintF(pFile, "ChromaEnhancement.KCR = %x\n", m_regCmd.crConfig.bitfields.KCR);
        CamX::OsUtils::FPrintF(pFile, "\n\n");
        CamX::OsUtils::FClose(pFile);
    }

    return;
}

CAMX_NAMESPACE_END
