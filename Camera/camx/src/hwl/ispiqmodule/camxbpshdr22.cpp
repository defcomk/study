////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017-2019 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxbpshdr22.cpp
/// @brief CAMXBPSHDR22 class implementation
/// HDR_Recon:  Reconstructs interlaced or zig-zag T1 (long exposure)/T2 (short exposure) HDR fields
///                    to full T1 and T2 images
/// HDR_MAC:    Combines full T1 and T2 images from HDR_Recon according to motion in the image contents
///                    to avoid motion artifacts in output HDR image
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "bps_data.h"
#include "camxbpshdr22.h"
#include "camxiqinterface.h"
#include "camxnode.h"
#include "camxtuningdatamanager.h"
#include "parametertuningtypes.h"

CAMX_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSHDR22::Create
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSHDR22::Create(
    BPSModuleCreateData* pCreateData)
{
    CamxResult result = CamxResultSuccess;

    if ((NULL != pCreateData) && (NULL != pCreateData->pNodeIdentifier))
    {
        BPSHDR22* pModule = CAMX_NEW BPSHDR22(pCreateData->pNodeIdentifier);

        if (NULL != pModule)
        {
            result = pModule->Initialize();
            if (CamxResultSuccess != result)
            {
                CAMX_LOG_ERROR(CamxLogGroupPProc, "Module initialization failed");
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
// BPSHDR22::Initialize
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSHDR22::Initialize()
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
// BPSHDR22::AllocateCommonLibraryData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSHDR22::AllocateCommonLibraryData()
{
    CamxResult result = CamxResultSuccess;

    UINT interpolationSize = (sizeof(hdr_2_2_0::hdr22_rgn_dataType) * (HDR22MaxmiumNonLeafNode + 1));

    if (NULL == m_dependenceData.pInterpolationData)
    {
        // Alloc for hdr_2_2_0::hdr22_rgn_dataType
        m_dependenceData.pInterpolationData = CAMX_CALLOC(interpolationSize);
        if (NULL == m_dependenceData.pInterpolationData)
        {
            result = CamxResultENoMemory;
        }
    }
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSHDR22::CheckDependenceChange
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL BPSHDR22::CheckDependenceChange(
    ISPInputData* pInputData)
{
    BOOL isChanged = FALSE;

    if ((NULL != pInputData)           &&
        (NULL != pInputData->pHwContext))
    {
        if (NULL != pInputData->pOEMIQSetting)
        {
            m_moduleEnable = (static_cast<OEMBPSIQSetting*>(pInputData->pOEMIQSetting))->HDREnable;
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

                m_pChromatix = pTuningManager->GetChromatix()->GetModule_hdr22_bps(
                                   reinterpret_cast<TuningMode*>(&pInputData->pTuningData->TuningMode[0]),
                                   pInputData->pTuningData->noOfSelectionParameter);
            }

            CAMX_ASSERT(NULL != m_pChromatix);
            if (NULL != m_pChromatix)
            {
                if (m_pChromatix != m_dependenceData.pChromatix)
                {
                    m_dependenceData.pChromatix = m_pChromatix;
                    m_moduleEnable              = m_pChromatix->enable_section.hdr_enable;

                    if (TRUE == m_moduleEnable)
                    {
                        m_MACEnable   = m_pChromatix->chromatix_hdr22_reserve.hdr_mac_en;
                        m_RECONEnable = m_pChromatix->chromatix_hdr22_reserve.hdr_recon_en;
                    }

                    m_moduleEnable = ((TRUE == m_MACEnable) && (TRUE == m_RECONEnable));

                    if (FALSE == m_moduleEnable)
                    {
                        m_MACEnable   = FALSE;
                        m_RECONEnable = FALSE;
                    }

                    isChanged = (TRUE == m_moduleEnable);
                }
            }
            else
            {
                CAMX_LOG_ERROR(CamxLogGroupISP, "Failed to get Chromatix");
            }
        }

        if (TRUE == m_moduleEnable)
        {
            CamxResult result = CamxResultSuccess;

            m_dependenceData.blackLevelOffset =
                static_cast<UINT16>(pInputData->pCalculatedData->BLSblackLevelOffset);
            m_dependenceData.ZZHDRFirstRBEXP  =
                static_cast<UINT16>(pInputData->sensorData.ZZHDRFirstExposurePattern);
            m_dependenceData.ZZHDRMode        = 1;
            m_dependenceData.ZZHDRPattern     =
                static_cast<UINT16>(pInputData->sensorData.ZZHDRColorPattern);
            m_dependenceData.RECONFirstField  =
                static_cast<UINT16>(pInputData->sensorData.ZZHDRFirstExposurePattern);

            result = IQInterface::GetPixelFormat(&pInputData->sensorData.format, &(m_dependenceData.bayerPattern));
            CAMX_ASSERT(CamxResultSuccess == result);

            // Check for trigger update status
            if (TRUE == IQInterface::s_interpolationTable.HDR22TriggerUpdate(&(pInputData->triggerData), &m_dependenceData))
            {
                if (NULL == pInputData->pOEMIQSetting)
                {
                    // Check for HDR MAC dynamic enable trigger hysterisis
                    m_MACEnable = IQSettingUtils::GetDynamicEnableFlag(
                        m_dependenceData.pChromatix->dynamic_enable_triggers.hdr_mac_en.enable,
                        m_dependenceData.pChromatix->dynamic_enable_triggers.hdr_mac_en.hyst_control_var,
                        m_dependenceData.pChromatix->dynamic_enable_triggers.hdr_mac_en.hyst_mode,
                        &(m_dependenceData.pChromatix->dynamic_enable_triggers.hdr_mac_en.hyst_trigger),
                        static_cast<VOID*>(&(pInputData->triggerData)),
                        &m_dependenceData.moduleMacEnable);

                    // Check for HDR RECON dynamic enable trigger hysterisis
                    m_RECONEnable = IQSettingUtils::GetDynamicEnableFlag(
                        m_dependenceData.pChromatix->dynamic_enable_triggers.hdr_recon_en.enable,
                        m_dependenceData.pChromatix->dynamic_enable_triggers.hdr_recon_en.hyst_control_var,
                        m_dependenceData.pChromatix->dynamic_enable_triggers.hdr_recon_en.hyst_mode,
                        &(m_dependenceData.pChromatix->dynamic_enable_triggers.hdr_recon_en.hyst_trigger),
                        static_cast<VOID*>(&pInputData->triggerData),
                        &m_dependenceData.moduleReconEnable);

                    m_moduleEnable = ((TRUE == m_MACEnable) && (TRUE == m_RECONEnable));

                    if (FALSE == m_moduleEnable)
                    {
                        m_MACEnable   = FALSE;
                        m_RECONEnable = FALSE;
                    }

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
// BPSHDR22::CreateCmdList
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSHDR22::CreateCmdList(
    const ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;

    if (NULL != pInputData->pCmdBuffer)
    {
        CmdBuffer* pCmdBuffer = pInputData->pCmdBuffer;

        result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                              regBPS_BPS_0_CLC_HDR_RECON_HDR_0_CFG,
                                              (sizeof(BPSHDRReconstructConfig) / RegisterWidthInBytes),
                                              reinterpret_cast<UINT32*>(&m_regCmd.reconstructConfig));
        if (CamxResultSuccess == result)
        {
            result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                                  regBPS_BPS_0_CLC_HDR_MAC_HDR_MAC_0_CFG,
                                                  (sizeof(BPSHDRMACConfig) / RegisterWidthInBytes),
                                                  reinterpret_cast<UINT32*>(&m_regCmd.MACConfig));
        }
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
// BPSHDR22::RunCalculation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSHDR22::RunCalculation(
    const ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;

    if (NULL != pInputData)
    {
        HDR22OutputData outputData;

        outputData.type                    = PipelineType::BPS;
        outputData.BPS.pRegCmd             = &m_regCmd;
        outputData.BPS.pHdrMacParameters   = &m_hdrMacParameters;
        outputData.BPS.pHdrReconParameters = &m_hdrReconParameters;

        result = IQInterface::HDR22CalculateSetting(&m_dependenceData, NULL, &outputData);

        if (CamxResultSuccess != result)
        {
            CAMX_LOG_ERROR(CamxLogGroupISP, "HDR22 Calculation Failed. result %d", result);
        }
        if (CamxLogGroupIQMod == (pInputData->dumpRegConfig & CamxLogGroupIQMod))
        {
            DumpRegConfig(outputData.BPS.pHdrMacParameters, outputData.BPS.pHdrReconParameters);
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
// BPSHDR22::GetRegCmd
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID* BPSHDR22::GetRegCmd()
{
    return static_cast<VOID*>(&m_regCmd);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSHDR22::UpdateBPSInternalData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID BPSHDR22::UpdateBPSInternalData(
    ISPInputData* pInputData
    ) const
{
    BpsIQSettings* pBPSIQSettings = reinterpret_cast<BpsIQSettings*>(pInputData->pipelineBPSData.pIQSettings);
    CAMX_ASSERT(NULL != pBPSIQSettings);

    pBPSIQSettings->hdrMacParameters        = m_hdrMacParameters;
    pBPSIQSettings->hdrReconParameters      = m_hdrReconParameters;

    // Store HDR enable state for PDPC
    pInputData->triggerData.zzHDRModeEnable = m_moduleEnable;

    // Post tuning metadata if setting is enabled
    if (NULL != pInputData->pBPSTuningMetadata)
    {
        CAMX_STATIC_ASSERT(sizeof(BPSHDRReconstructConfig) <=
                          sizeof(pInputData->pBPSTuningMetadata->BPSHDRData.HDRReconConfig));
        CAMX_STATIC_ASSERT(sizeof(BPSHDRMACConfig) <=
                          sizeof(pInputData->pBPSTuningMetadata->BPSHDRData.HDRMACConfig));

        Utils::Memcpy(&pInputData->pBPSTuningMetadata->BPSHDRData.HDRReconConfig,
                      &m_regCmd.reconstructConfig,
                      sizeof(BPSHDRReconstructConfig));
        Utils::Memcpy(&pInputData->pBPSTuningMetadata->BPSHDRData.HDRMACConfig,
                      &m_regCmd.MACConfig,
                      sizeof(BPSHDRMACConfig));
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSHDR22::Execute
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSHDR22::Execute(
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
            CAMX_ASSERT_ALWAYS_MESSAGE("BPSHDR22 module calculation Failed.");
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
// BPSHDR22::DeallocateCommonLibraryData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID BPSHDR22::DeallocateCommonLibraryData()
{
    if (NULL != m_dependenceData.pInterpolationData)
    {
        CAMX_FREE(m_dependenceData.pInterpolationData);
        m_dependenceData.pInterpolationData = NULL;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSHDR22::BPSHDR22
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BPSHDR22::BPSHDR22(
    const CHAR* pNodeIdentifier)
{
    m_pNodeIdentifier     = pNodeIdentifier;
    m_type                = ISPIQModuleType::BPSHDR;
    m_moduleEnable        = FALSE;
    m_pChromatix          = NULL;

    m_cmdLength    = PacketBuilder::RequiredWriteRegRangeSizeInDwords(sizeof(BPSHDRReconstructConfig) / RegisterWidthInBytes) +
                     PacketBuilder::RequiredWriteRegRangeSizeInDwords(sizeof(BPSHDRMACConfig) / RegisterWidthInBytes);

    m_dependenceData.moduleMacEnable   = FALSE; ///< First frame is always FALSE
    m_dependenceData.moduleReconEnable = FALSE; ///< First frame is always FALSE
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSHDR22::~BPSHDR22
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BPSHDR22::~BPSHDR22()
{
    m_pChromatix = NULL;
    DeallocateCommonLibraryData();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSHDR22::DumpRegConfig
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID BPSHDR22::DumpRegConfig(
    const HDRMacParameters* pMacParametersConfig,
    const HDRReconParameters* pReconParametersConfig
    ) const
{
    CHAR  dumpFilename[256];
    FILE* pFile = NULL;
    CamX::OsUtils::SNPrintF(dumpFilename, sizeof(dumpFilename), "%s/BPS_regdump.txt", ConfigFileDirectory);
    pFile = CamX::OsUtils::FOpen(dumpFilename, "a+");

    if (NULL != pFile)
    {
        CamX::OsUtils::FPrintF(pFile, "******** BPS HDR22 ******** \n");
        CamX::OsUtils::FPrintF(pFile, "* HDR22 module CFG [HEX] \n");
        CamX::OsUtils::FPrintF(pFile, "hdrMacParameters.moduleCfg.EN        = %x\n", pMacParametersConfig->moduleCfg.EN);
        CamX::OsUtils::FPrintF(pFile, "hdrMacParameters.dilation            = %x\n", pMacParametersConfig->dilation);
        CamX::OsUtils::FPrintF(pFile, "hdrMacParameters.linearMode          = %x\n", pMacParametersConfig->linearMode);
        CamX::OsUtils::FPrintF(pFile, "hdrMacParameters.smoothFilterEnable  = %x\n", pMacParametersConfig->smoothFilterEnable);
        CamX::OsUtils::FPrintF(pFile, "hdrReconParameters.moduleCfg.EN      = %x\n", pReconParametersConfig->moduleCfg.EN);
        CamX::OsUtils::FPrintF(pFile, "hdrReconParameters.hdrExpRatio       = %x\n", pReconParametersConfig->hdrExpRatio);
        CamX::OsUtils::FPrintF(pFile, "hdrReconParameters.hdrMode           = %x\n", pReconParametersConfig->hdrMode);
        CamX::OsUtils::FPrintF(pFile, "hdrReconParameters.linearMode        = %x\n", pReconParametersConfig->linearMode);
        CamX::OsUtils::FPrintF(pFile, "hdrReconParameters.zzHdrPattern      = %x\n", pReconParametersConfig->zzHdrPattern);
        CamX::OsUtils::FPrintF(pFile, "hdrReconParameters.zzHdrPrefilterTap = %x\n", pReconParametersConfig->zzHdrPrefilterTap);
        CamX::OsUtils::FPrintF(pFile, "* HDR22 CFG [HEX] \n");
        CamX::OsUtils::FPrintF(pFile, "reconstructConfig.config0            = %x \n", m_regCmd.reconstructConfig.config0);
        CamX::OsUtils::FPrintF(pFile, "reconstructConfig.config1            = %x \n", m_regCmd.reconstructConfig.config1);
        CamX::OsUtils::FPrintF(pFile, "reconstructConfig.config2            = %x \n", m_regCmd.reconstructConfig.config2);
        CamX::OsUtils::FPrintF(pFile, "reconstructConfig.config3            = %x \n", m_regCmd.reconstructConfig.config3);
        CamX::OsUtils::FPrintF(pFile, "reconstructConfig.interlacedConfig0  = %x \n",
            m_regCmd.reconstructConfig.interlacedConfig0);
        CamX::OsUtils::FPrintF(pFile, "reconstructConfig.interlacedConfig1  = %x \n",
            m_regCmd.reconstructConfig.interlacedConfig1);
        CamX::OsUtils::FPrintF(pFile, "reconstructConfig.interlacedConfig2  = %x \n",
            m_regCmd.reconstructConfig.interlacedConfig2);
        CamX::OsUtils::FPrintF(pFile, "reconstructConfig.interlacedConfig0  = %x \n",
            m_regCmd.reconstructConfig.zigzagConfig0);
        CamX::OsUtils::FPrintF(pFile, "reconstructConfig.interlacedConfig1  = %x \n",
            m_regCmd.reconstructConfig.zigzagConfig1);
        CamX::OsUtils::FPrintF(pFile, "MACConfig.config0                    = %x \n", m_regCmd.MACConfig.config0);
        CamX::OsUtils::FPrintF(pFile, "MACConfig.config1                    = %x \n", m_regCmd.MACConfig.config1);
        CamX::OsUtils::FPrintF(pFile, "MACConfig.config2                    = %x \n", m_regCmd.MACConfig.config2);
        CamX::OsUtils::FPrintF(pFile, "MACConfig.config3                    = %x \n", m_regCmd.MACConfig.config3);
        CamX::OsUtils::FPrintF(pFile, "MACConfig.config4                    = %x \n", m_regCmd.MACConfig.config4);
        CamX::OsUtils::FPrintF(pFile, "MACConfig.config5                    = %x \n", m_regCmd.MACConfig.config5);
        CamX::OsUtils::FPrintF(pFile, "MACConfig.config6                    = %x \n", m_regCmd.MACConfig.config6);
        CamX::OsUtils::FPrintF(pFile, "\n\n");
        CamX::OsUtils::FClose(pFile);
    }
}

CAMX_NAMESPACE_END
