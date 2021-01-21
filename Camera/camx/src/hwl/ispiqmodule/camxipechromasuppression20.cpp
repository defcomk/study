////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017-2018 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxipechromasuppression20.cpp
/// @brief IPEChromaSuppression class implementation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "camxipechromasuppression20.h"
#include "camxiqinterface.h"
#include "camxnode.h"
#include "camxtuningdatamanager.h"
#include "ipe_data.h"
#include "parametertuningtypes.h"

CAMX_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPEChromaSuppression20::Create
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPEChromaSuppression20::Create(
    IPEModuleCreateData* pCreateData)
{
    CamxResult result = CamxResultSuccess;

    if ((NULL != pCreateData) && (NULL != pCreateData->pNodeIdentifier))
    {
        IPEChromaSuppression20* pModule = CAMX_NEW IPEChromaSuppression20(pCreateData->pNodeIdentifier);

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
        CAMX_LOG_ERROR(CamxLogGroupPProc, "Null input pointer");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPEChromaSuppression20::Initialize
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPEChromaSuppression20::Initialize()
{
    CamxResult result = CamxResultSuccess;

    result = AllocateCommonLibraryData();
    if (result != CamxResultSuccess)
    {
        CAMX_LOG_ERROR(CamxLogGroupPProc, "Unable to initilize common library data, no memory");
    }
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPEChromaSuppression20::AllocateCommonLibraryData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPEChromaSuppression20::AllocateCommonLibraryData()
{
    CamxResult result = CamxResultSuccess;

    UINT interpolationSize = (sizeof(cs_2_0_0::cs20_rgn_dataType) * (CS20MaxmiumNonLeafNode + 1));

    if (NULL == m_dependenceData.pInterpolationData)
    {
        // Alloc for cs_2_0_0::cs20_rgn_dataType
        m_dependenceData.pInterpolationData = CAMX_CALLOC(interpolationSize);
        if (NULL == m_dependenceData.pInterpolationData)
        {
            result = CamxResultENoMemory;
        }
    }
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPEChromaSuppression20::UpdateIPEInternalData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPEChromaSuppression20::UpdateIPEInternalData(
    const ISPInputData* pInputData
    ) const
{
    CamxResult     result         = CamxResultSuccess;
    IpeIQSettings* pIPEIQSettings = reinterpret_cast<IpeIQSettings*>(pInputData->pipelineIPEData.pIPEIQSettings);

    pIPEIQSettings->chromaSupressionParameters.moduleCfg.EN = m_moduleEnable;

    // Post tuning metadata if setting is enabled
    if (NULL != pInputData->pIPETuningMetadata)
    {
        CAMX_STATIC_ASSERT(sizeof(IPEChromaSuppressionRegCmd) <=
                           sizeof(pInputData->pIPETuningMetadata->IPEChromaSuppressionData.CSConfig));

        Utils::Memcpy(&pInputData->pIPETuningMetadata->IPEChromaSuppressionData.CSConfig,
                      &m_regCmd,
                      sizeof(IPEChromaSuppressionRegCmd));
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPEChromaSuppression20::CreateCmdList
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPEChromaSuppression20::CreateCmdList(
    const ISPInputData* pInputData)
{
    CamxResult  result      = CamxResultSuccess;
    CmdBuffer*  pCmdBuffer  = pInputData->pipelineIPEData.ppIPECmdBuffer[CmdBufferPostLTM];

    CAMX_ASSERT(NULL != pCmdBuffer);

    result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                          regIPE_IPE_0_PPS_CLC_CHROMA_SUP_CS_Y_KNEE_PT_LUT_CFG_0,
                                          IPEChromaSuppressionRegLength,
                                          reinterpret_cast<UINT32*>(&m_regCmd));
    CAMX_ASSERT(CamxResultSuccess == result);

    return result;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPEChromaSuppression20::GetRegCmd
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID* IPEChromaSuppression20::GetRegCmd()
{
    return reinterpret_cast<VOID*>(&m_regCmd);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPEChromaSuppression20::CheckDependenceChange
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL IPEChromaSuppression20::CheckDependenceChange(
    ISPInputData* pInputData)
{
    BOOL isChanged = FALSE;

    if ((NULL != pInputData)                  &&
        (NULL != pInputData->pAECUpdateData)  &&
        (NULL != pInputData->pAWBUpdateData)  &&
        (NULL != pInputData->pHwContext)      &&
        (NULL != pInputData->pHALTagsData))
    {
        if (NULL != pInputData->pOEMIQSetting)
        {
            m_moduleEnable = (static_cast<OEMIPEIQSetting*>(pInputData->pOEMIQSetting))->ChromaSuppressionEnable;

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
                    m_pChromatix = pTuningManager->GetChromatix()->GetModule_cs20_ipe(
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
                        m_moduleEnable              = m_pChromatix->enable_section.chroma_suppression_enable;

                        if (TRUE == m_moduleEnable)
                        {
                            isChanged = TRUE;
                        }
                    }
                }
                else
                {
                    CAMX_LOG_ERROR(CamxLogGroupPProc, "Failed to get Chromatix");
                }
            }
            else
            {
                CAMX_LOG_ERROR(CamxLogGroupPProc, "Tuning Pointer is NULL");
            }
        }

        // Check for manual tone map mode, if yes disable chroma supression
        if ((TRUE == m_moduleEnable) &&
            (TonemapModeContrastCurve == pInputData->pHALTagsData->tonemapCurves.tonemapMode))
        {
            m_moduleEnable = FALSE;
            isChanged      = FALSE;
        }

        if (TRUE == m_moduleEnable)
        {
            if (TRUE ==
                IQInterface::s_interpolationTable.IPECS20TriggerUpdate(&pInputData->triggerData, &m_dependenceData))
            {
                isChanged = TRUE;
            }
        }
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupPProc, "Null Input Pointer");
    }

    return isChanged;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPEChromaSuppression20::RunCalculation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPEChromaSuppression20::RunCalculation(
    const ISPInputData* pInputData)
{
    CamxResult      result      = CamxResultSuccess;
    CS20OutputData  outputData  = { 0 };

    if (NULL != pInputData)
    {

        outputData.pRegCmd = &m_regCmd;

        result = IQInterface::IPECS20CalculateSetting(&m_dependenceData, pInputData->pOEMIQSetting, &outputData);

        if (CamxResultSuccess != result)
        {
            CAMX_LOG_ERROR(CamxLogGroupISP, "Chroma Suppression Calculation Failed. result %d", result);
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
// IPEChromaSuppression20::Execute
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPEChromaSuppression20::Execute(
    ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;

    if (NULL != pInputData)
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
    }
    else
    {
        result = CamxResultEInvalidArg;
        CAMX_ASSERT_ALWAYS_MESSAGE("Null Input Pointer");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPEChromaSuppression20::DeallocateCommonLibraryData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IPEChromaSuppression20::DeallocateCommonLibraryData()
{
    if (NULL != m_dependenceData.pInterpolationData)
    {
        CAMX_FREE(m_dependenceData.pInterpolationData);
        m_dependenceData.pInterpolationData = NULL;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPEChromaSuppression20::IPEChromaSuppression20
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IPEChromaSuppression20::IPEChromaSuppression20(
    const CHAR* pNodeIdentifier)
{
    UINT size      = PacketBuilder::RequiredWriteRegRangeSizeInDwords(sizeof(IPEChromaSuppressionRegCmd) / 4);

    m_pNodeIdentifier   = pNodeIdentifier;
    m_type              = ISPIQModuleType::IPEChromaSuppression;
    m_moduleEnable      = TRUE;
    m_cmdLength         = size * sizeof(UINT32);
    m_numLUT            = 0;
    m_offsetLUT         = 0;
    m_pChromatix        = NULL;

    /// @todo (CAMX-729) Consume 3A updates and call common IQ library
    m_regCmd.knee.cfg0.u32All = 0x00100000;
    m_regCmd.knee.cfg1.u32All = 0x00300020;
    m_regCmd.knee.cfg2.u32All = 0x00500040;
    m_regCmd.knee.cfg3.u32All = 0x01e000f0;
    m_regCmd.knee.cfg4.u32All = 0x02e002b0;
    m_regCmd.knee.cfg5.u32All = 0x03400310;
    m_regCmd.knee.cfg6.u32All = 0x03a00370;
    m_regCmd.knee.cfg7.u32All = 0x03fc03d0;

    m_regCmd.weight.cfg0.u32All = 0x04000400;
    m_regCmd.weight.cfg1.u32All = 0x04000400;
    m_regCmd.weight.cfg2.u32All = 0x04000400;
    m_regCmd.weight.cfg3.u32All = 0x04000400;
    m_regCmd.weight.cfg4.u32All = 0x04000400;
    m_regCmd.weight.cfg5.u32All = 0x04000400;
    m_regCmd.weight.cfg6.u32All = 0x04000400;
    m_regCmd.weight.cfg7.u32All = 0x04000400;

    m_regCmd.threshold.cfg0.u32All = 0x00000000;
    m_regCmd.threshold.cfg1.u32All = 0x00000000;
    m_regCmd.threshold.cfg2.u32All = 0x00000000;
    m_regCmd.threshold.cfg3.u32All = 0x00000000;
    m_regCmd.threshold.cfg4.u32All = 0x00000000;
    m_regCmd.threshold.cfg5.u32All = 0x00000000;
    m_regCmd.threshold.cfg6.u32All = 0x00000000;
    m_regCmd.threshold.cfg7.u32All = 0x00000000;

    m_regCmd.slope.cfg0.u32All = 0x001d001d;
    m_regCmd.slope.cfg1.u32All = 0x001d001d;
    m_regCmd.slope.cfg2.u32All = 0x001d001d;
    m_regCmd.slope.cfg3.u32All = 0x001d001d;
    m_regCmd.slope.cfg4.u32All = 0x001d001d;
    m_regCmd.slope.cfg5.u32All = 0x001d001d;
    m_regCmd.slope.cfg6.u32All = 0x001d001d;
    m_regCmd.slope.cfg7.u32All = 0x001d001d;

    m_regCmd.inverse.cfg0.u32All = 0x02000200;
    m_regCmd.inverse.cfg1.u32All = 0x02000200;
    m_regCmd.inverse.cfg2.u32All = 0x00330200;
    m_regCmd.inverse.cfg3.u32All = 0x00270022;
    m_regCmd.inverse.cfg4.u32All = 0x00aa00aa;
    m_regCmd.inverse.cfg5.u32All = 0x00aa00aa;
    m_regCmd.inverse.cfg6.u32All = 0x00aa00aa;
    m_regCmd.inverse.cfg7.u32All = 0x000100ba;

    m_regCmd.qRes.luma.u32All   = 0x00000000;
    m_regCmd.qRes.chroma.u32All = 0x00000000;

    CAMX_LOG_VERBOSE(CamxLogGroupPProc, "IPE Chroma Suppression m_cmdLength %d ", m_cmdLength);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IPEChromaSuppression20::~IPEChromaSuppression20
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IPEChromaSuppression20::~IPEChromaSuppression20()
{
    m_pChromatix = NULL;
    DeallocateCommonLibraryData();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPEChromaSuppression20::DumpRegConfig
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IPEChromaSuppression20::DumpRegConfig() const
{
    CHAR   dumpFilename[256];
    FILE*  pFile = NULL;
    CamX::OsUtils::SNPrintF(dumpFilename, sizeof(dumpFilename), "%s/IPE_regdump.txt", ConfigFileDirectory);
    pFile = CamX::OsUtils::FOpen(dumpFilename, "a+");

    if (NULL != pFile)
    {
        CamX::OsUtils::FPrintF(pFile, "******** IPE Chroma Suppression 20 ********\n");
        CamX::OsUtils::FPrintF(pFile, "* Chroma Suppression 20 CFG [HEX] \n");
        CamX::OsUtils::FPrintF(pFile, "m_regCmd.knee.cfg0     = %x\n", m_regCmd.knee.cfg0);
        CamX::OsUtils::FPrintF(pFile, "m_regCmd.knee.cfg1       = %x\n", m_regCmd.knee.cfg1);
        CamX::OsUtils::FPrintF(pFile, "m_regCmd.knee.cfg2       = %x\n", m_regCmd.knee.cfg2);
        CamX::OsUtils::FPrintF(pFile, "m_regCmd.knee.cfg3       = %x\n", m_regCmd.knee.cfg3);
        CamX::OsUtils::FPrintF(pFile, "m_regCmd.knee.cfg4       = %x\n", m_regCmd.knee.cfg4);
        CamX::OsUtils::FPrintF(pFile, "m_regCmd.knee.cfg5       = %x\n", m_regCmd.knee.cfg5);
        CamX::OsUtils::FPrintF(pFile, "m_regCmd.knee.cfg6       = %x\n", m_regCmd.knee.cfg6);
        CamX::OsUtils::FPrintF(pFile, "m_regCmd.knee.cfg7       = %x\n", m_regCmd.knee.cfg7);
        CamX::OsUtils::FPrintF(pFile, "m_regCmd.knee.cfg7       = %x\n", m_regCmd.knee.cfg7);
        CamX::OsUtils::FPrintF(pFile, "m_regCmd.weight.cfg0      = %x\n", m_regCmd.weight.cfg0);
        CamX::OsUtils::FPrintF(pFile, "m_regCmd.weight.cfg1      = %x\n", m_regCmd.weight.cfg1);
        CamX::OsUtils::FPrintF(pFile, "m_regCmd.weight.cfg2      = %x\n", m_regCmd.weight.cfg2);
        CamX::OsUtils::FPrintF(pFile, "m_regCmd.weight.cfg3      = %x\n", m_regCmd.weight.cfg3);
        CamX::OsUtils::FPrintF(pFile, "m_regCmd.weight.cfg4      = %x\n", m_regCmd.weight.cfg4);
        CamX::OsUtils::FPrintF(pFile, "m_regCmd.weight.cfg5      = %x\n", m_regCmd.weight.cfg5);
        CamX::OsUtils::FPrintF(pFile, "m_regCmd.weight.cfg6      = %x\n", m_regCmd.weight.cfg6);
        CamX::OsUtils::FPrintF(pFile, "m_regCmd.weight.cfg7      = %x\n", m_regCmd.weight.cfg7);
        CamX::OsUtils::FPrintF(pFile, "m_regCmd.weight.cfg7      = %x\n", m_regCmd.weight.cfg7);
        CamX::OsUtils::FPrintF(pFile, "m_regCmd.threshold.cfg0  = %x\n", m_regCmd.threshold.cfg0);
        CamX::OsUtils::FPrintF(pFile, "m_regCmd.threshold.cfg1  = %x\n", m_regCmd.threshold.cfg1);
        CamX::OsUtils::FPrintF(pFile, "m_regCmd.threshold.cfg2  = %x\n", m_regCmd.threshold.cfg2);
        CamX::OsUtils::FPrintF(pFile, "m_regCmd.threshold.cfg3  = %x\n", m_regCmd.threshold.cfg3);
        CamX::OsUtils::FPrintF(pFile, "m_regCmd.threshold.cfg4  = %x\n", m_regCmd.threshold.cfg4);
        CamX::OsUtils::FPrintF(pFile, "m_regCmd.threshold.cfg5  = %x\n", m_regCmd.threshold.cfg5);
        CamX::OsUtils::FPrintF(pFile, "m_regCmd.threshold.cfg6  = %x\n", m_regCmd.threshold.cfg6);
        CamX::OsUtils::FPrintF(pFile, "m_regCmd.threshold.cfg7  = %x\n", m_regCmd.threshold.cfg7);
        CamX::OsUtils::FPrintF(pFile, "m_regCmd.threshold.cfg7  = %x\n", m_regCmd.threshold.cfg7);
        CamX::OsUtils::FPrintF(pFile, "m_regCmd.slope.cfg0      = %x\n", m_regCmd.slope.cfg0);
        CamX::OsUtils::FPrintF(pFile, "m_regCmd.slope.cfg1      = %x\n", m_regCmd.slope.cfg1);
        CamX::OsUtils::FPrintF(pFile, "m_regCmd.slope.cfg2      = %x\n", m_regCmd.slope.cfg2);
        CamX::OsUtils::FPrintF(pFile, "m_regCmd.slope.cfg3      = %x\n", m_regCmd.slope.cfg3);
        CamX::OsUtils::FPrintF(pFile, "m_regCmd.slope.cfg4      = %x\n", m_regCmd.slope.cfg4);
        CamX::OsUtils::FPrintF(pFile, "m_regCmd.slope.cfg5      = %x\n", m_regCmd.slope.cfg5);
        CamX::OsUtils::FPrintF(pFile, "m_regCmd.slope.cfg6      = %x\n", m_regCmd.slope.cfg6);
        CamX::OsUtils::FPrintF(pFile, "m_regCmd.slope.cfg7      = %x\n", m_regCmd.slope.cfg7);
        CamX::OsUtils::FPrintF(pFile, "m_regCmd.slope.cfg7      = %x\n", m_regCmd.slope.cfg7);
        CamX::OsUtils::FPrintF(pFile, "m_regCmd.inverse.cfg0    = %x\n", m_regCmd.inverse.cfg0);
        CamX::OsUtils::FPrintF(pFile, "m_regCmd.inverse.cfg1    = %x\n", m_regCmd.inverse.cfg1);
        CamX::OsUtils::FPrintF(pFile, "m_regCmd.inverse.cfg2    = %x\n", m_regCmd.inverse.cfg2);
        CamX::OsUtils::FPrintF(pFile, "m_regCmd.inverse.cfg3    = %x\n", m_regCmd.inverse.cfg3);
        CamX::OsUtils::FPrintF(pFile, "m_regCmd.inverse.cfg4    = %x\n", m_regCmd.inverse.cfg4);
        CamX::OsUtils::FPrintF(pFile, "m_regCmd.inverse.cfg5    = %x\n", m_regCmd.inverse.cfg5);
        CamX::OsUtils::FPrintF(pFile, "m_regCmd.inverse.cfg6    = %x\n", m_regCmd.inverse.cfg6);
        CamX::OsUtils::FPrintF(pFile, "m_regCmd.inverse.cfg7    = %x\n", m_regCmd.inverse.cfg7);
        CamX::OsUtils::FPrintF(pFile, "m_regCmd.inverse.cfg7    = %x\n", m_regCmd.threshold.cfg7);
        CamX::OsUtils::FPrintF(pFile, "m_regCmd.qRes.luma       = %x\n", m_regCmd.qRes.luma);
        CamX::OsUtils::FPrintF(pFile, "m_regCmd.qRes.chroma     = %x\n", m_regCmd.qRes.chroma);
        CamX::OsUtils::FPrintF(pFile, "\n\n");
        CamX::OsUtils::FClose(pFile);
    }
}

CAMX_NAMESPACE_END
