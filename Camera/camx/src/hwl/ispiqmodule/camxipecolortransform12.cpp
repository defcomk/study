////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017-2018 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxipecolortransform12.cpp
/// @brief IPEColorTransform class implementation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "camxtuningdatamanager.h"
#include "camxiqinterface.h"
#include "camxispiqmodule.h"
#include "camxnode.h"
#include "ipe_data.h"
#include "camxipecolortransform12.h"

CAMX_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPEColorTransform12::Create
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPEColorTransform12::Create(
    IPEModuleCreateData* pCreateData)
{
    CamxResult result = CamxResultSuccess;

    if ((NULL != pCreateData) && (NULL != pCreateData->pNodeIdentifier))
    {
        pCreateData->pModule = CAMX_NEW IPEColorTransform12(pCreateData->pNodeIdentifier);

        if (NULL == pCreateData->pModule)
        {
            result = CamxResultENoMemory;
            CAMX_ASSERT_ALWAYS_MESSAGE("Memory allocation failed");
        }
    }
    else
    {
        result = CamxResultEInvalidArg;
        CAMX_LOG_ERROR(CamxLogGroupPProc, "Null input pointer");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPEColorTransform12::UpdateIPEInternalData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPEColorTransform12::UpdateIPEInternalData(
    const ISPInputData* pInputData
    ) const
{
    CamxResult result = CamxResultSuccess;

    if (NULL != pInputData)
    {
        IpeIQSettings* pIPEIQSettings = reinterpret_cast<IpeIQSettings*>(pInputData->pipelineIPEData.pIPEIQSettings);

        pIPEIQSettings->colorTransformParameters.moduleCfg.EN = m_moduleEnable;
        /// @todo (CAMX-738) Add metadata information

        // Post tuning metadata if setting is enabled
        if (NULL != pInputData->pIPETuningMetadata)
        {
            CAMX_STATIC_ASSERT(sizeof(IPEColorTransformRegCmd) <=
                               sizeof(pInputData->pIPETuningMetadata->IPECSTData.CSTConfig));

            Utils::Memcpy(&pInputData->pIPETuningMetadata->IPECSTData.CSTConfig,
                          &m_regCmd,
                          sizeof(IPEColorTransformRegCmd));
        }
    }
    else
    {
        result = CamxResultEInvalidArg;
        CAMX_LOG_ERROR(CamxLogGroupPProc, "Input Null pointer");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPEColorTransform12::CreateCmdList
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPEColorTransform12::CreateCmdList(
    const ISPInputData* pInputData)
{
    CamxResult result     = CamxResultSuccess;
    CmdBuffer* pCmdBuffer = pInputData->pipelineIPEData.ppIPECmdBuffer[CmdBufferPreLTM];

    CAMX_ASSERT(NULL != pCmdBuffer);
    result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                          regIPE_IPE_0_PPS_CLC_COLOR_XFORM_COLOR_XFORM_CH0_COEFF_CFG_0,
                                          IPEColorTransformRegLength,
                                          reinterpret_cast<UINT32*>(&m_regCmd));
    CAMX_ASSERT(CamxResultSuccess == result);

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPEColorTransform12::ValidateDependenceParams
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPEColorTransform12::ValidateDependenceParams(
    const ISPInputData* pInputData
    ) const
{
    CamxResult result = CamxResultSuccess;

    /// @todo (CAMX-730) validate dependency parameters
    if ((NULL == pInputData)                                                  ||
        (NULL == pInputData->pipelineIPEData.ppIPECmdBuffer[CmdBufferPreLTM]) ||
        (NULL == pInputData->pipelineIPEData.pIPEIQSettings))
    {
        CAMX_LOG_ERROR(CamxLogGroupPProc, "Invalid Input: pInputData %p", pInputData);

        result = CamxResultEInvalidArg;
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPEColorTransform12::GetRegCmd
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID* IPEColorTransform12::GetRegCmd()
{
    return &m_regCmd;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPEColorTransform12::CheckDependenceChange
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL IPEColorTransform12::CheckDependenceChange(
    const ISPInputData* pInputData)
{
    BOOL isChanged = FALSE;

    if (NULL != pInputData)
    {
        if (NULL != pInputData->pOEMIQSetting)
        {
            m_moduleEnable = (static_cast<OEMIPEIQSetting*>(pInputData->pOEMIQSetting))->CSTEnable;

            isChanged = (TRUE == m_moduleEnable);
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

                m_pChromatix = pTuningManager->GetChromatix()->GetModule_cst12_ipe(
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
                    m_moduleEnable              = m_pChromatix->enable_section.cst_enable;

                    isChanged                   = (TRUE == m_moduleEnable);
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
        CAMX_LOG_ERROR(CamxLogGroupISP, "Null Input Pointer");
    }

    return isChanged;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPEColorTransform12::RunCalculation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPEColorTransform12::RunCalculation(
    const ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;

    if (NULL != pInputData)
    {
        CAMX_LOG_VERBOSE(CamxLogGroupPProc, "Lux Index [0x%f] ", pInputData->pAECUpdateData->luxIndex);

        CST12OutputData outputData;

        outputData.type                  = PipelineType::IPE;
        outputData.regCommand.pRegIPECmd = &m_regCmd;

        result = IQInterface::CST12CalculateSetting(&m_dependenceData, pInputData->pOEMIQSetting, &outputData);

        if (CamxResultSuccess != result)
        {
            CAMX_LOG_ERROR(CamxLogGroupISP, "CST12 Calculation Failed. result %d", result);
        }

        if (CamxLogGroupIQMod == (pInputData->dumpRegConfig & CamxLogGroupIQMod))
        {
            DumpRegConfig();
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
// IPEColorTransform12::Execute
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPEColorTransform12::Execute(
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
// IPEColorTransform12::IPEColorTransform12
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IPEColorTransform12::IPEColorTransform12(
    const CHAR* pNodeIdentifier)
{
    m_pNodeIdentifier   = pNodeIdentifier;
    UINT size           = PacketBuilder::RequiredWriteRegRangeSizeInDwords(
                            sizeof(IPEColorTransformRegCmd) / RegisterWidthInBytes);
    m_type              = ISPIQModuleType::IPECST;
    m_moduleEnable      = TRUE;
    m_cmdLength         = size * sizeof(UINT32);
    m_numLUT            = 0;
    m_offsetLUT         = 0;
    m_pChromatix        = NULL;

    CAMX_LOG_VERBOSE(CamxLogGroupPProc, "IPE CST m_cmdLength %d ", m_cmdLength);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IPEColorTransform12::~IPEColorTransform12
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IPEColorTransform12::~IPEColorTransform12()
{
    m_pChromatix = NULL;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPEColorTransform12::DumpRegConfig
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IPEColorTransform12::DumpRegConfig() const
{
    CHAR  dumpFilename[256];
    FILE* pFile = NULL;
    CamX::OsUtils::SNPrintF(dumpFilename, sizeof(dumpFilename), "%s/IPE_regdump.txt", ConfigFileDirectory);
    pFile = CamX::OsUtils::FOpen(dumpFilename, "a+");

    if (NULL != pFile)
    {
        CamX::OsUtils::FPrintF(pFile, "******** IPE Color Transform12 ********\n");
        CamX::OsUtils::FPrintF(pFile, "******** IPE Color Transform12 [HEX] ********\n");
        CamX::OsUtils::FPrintF(pFile, "ColorTransform.Ch0.coefficient0  = %x\n", m_regCmd.configChannel0.coefficient0);
        CamX::OsUtils::FPrintF(pFile, "ColorTransform.Ch0.coefficient1  = %x\n", m_regCmd.configChannel0.coefficient1);
        CamX::OsUtils::FPrintF(pFile, "ColorTransform.Ch0.offset        = %x\n", m_regCmd.configChannel0.offset);
        CamX::OsUtils::FPrintF(pFile, "ColorTransform.Ch0.clamp         = %x\n", m_regCmd.configChannel0.clamp);
        CamX::OsUtils::FPrintF(pFile, "ColorTransform.Ch1.coefficient0  = %x\n", m_regCmd.configChannel1.coefficient0);
        CamX::OsUtils::FPrintF(pFile, "ColorTransform.Ch1.coefficient1  = %x\n", m_regCmd.configChannel1.coefficient1);
        CamX::OsUtils::FPrintF(pFile, "ColorTransform.Ch1.offset        = %x\n", m_regCmd.configChannel1.offset);
        CamX::OsUtils::FPrintF(pFile, "ColorTransform.Ch1.clamp         = %x\n", m_regCmd.configChannel1.clamp);
        CamX::OsUtils::FPrintF(pFile, "ColorTransform.Ch2.coefficient0  = %x\n", m_regCmd.configChannel2.coefficient0);
        CamX::OsUtils::FPrintF(pFile, "ColorTransform.Ch2.coefficient1  = %x\n", m_regCmd.configChannel2.coefficient1);
        CamX::OsUtils::FPrintF(pFile, "ColorTransform.Ch2.offset        = %x\n", m_regCmd.configChannel2.offset);
        CamX::OsUtils::FPrintF(pFile, "ColorTransform.Ch2.clamp         = %x\n", m_regCmd.configChannel2.clamp);
        CamX::OsUtils::FPrintF(pFile, "\n\n");
        CamX::OsUtils::FClose(pFile);
    }
    /// @todo (CAMX-730) dump dependency parameters
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "Ch0 matrix coeff [0x%x] ", m_regCmd.configChannel0.coefficient0.u32All);
}

CAMX_NAMESPACE_END
