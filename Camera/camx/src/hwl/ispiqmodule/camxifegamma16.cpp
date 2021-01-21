////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017-2018 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxifegamma16.cpp
/// @brief CAMXIFEGamma16 class implementation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "camxdefs.h"
#include "camxtuningdatamanager.h"
#include "parametertuningtypes.h"
#include "camxiqinterface.h"
#include "camxispiqmodule.h"
#include "camxnode.h"
#include "camxifegamma16.h"

CAMX_NAMESPACE_BEGIN

static const UINT GammaGLUTBank0 = IFE_IFE_0_VFE_DMI_CFG_DMI_RAM_SEL_RGBLUT_RAM_CH0_BANK0;
static const UINT GammaGLUTBank1 = IFE_IFE_0_VFE_DMI_CFG_DMI_RAM_SEL_RGBLUT_RAM_CH0_BANK1;
static const UINT GammaBLUTBank0 = IFE_IFE_0_VFE_DMI_CFG_DMI_RAM_SEL_RGBLUT_RAM_CH1_BANK0;
static const UINT GammaBLUTBank1 = IFE_IFE_0_VFE_DMI_CFG_DMI_RAM_SEL_RGBLUT_RAM_CH1_BANK1;
static const UINT GammaRLUTBank0 = IFE_IFE_0_VFE_DMI_CFG_DMI_RAM_SEL_RGBLUT_RAM_CH2_BANK0;
static const UINT GammaRLUTBank1 = IFE_IFE_0_VFE_DMI_CFG_DMI_RAM_SEL_RGBLUT_RAM_CH2_BANK1;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEGamma16::Create
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFEGamma16::Create(
    IFEModuleCreateData* pCreateData)
{
    CamxResult result = CamxResultSuccess;

    if (NULL != pCreateData)
    {
        pCreateData->pModule = CAMX_NEW IFEGamma16;

        if (NULL == pCreateData->pModule)
        {
            result = CamxResultEFailed;
            CAMX_ASSERT_ALWAYS_MESSAGE("Failed to create IFEGamma16 object.");
        }
    }
    else
    {
        result = CamxResultEInvalidArg;
        CAMX_ASSERT_ALWAYS_MESSAGE("Null Input Data for IFEGamma16 Creation");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEGamma16::Execute
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFEGamma16::Execute(
    ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;

    if (NULL != pInputData)
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
                CAMX_ASSERT_ALWAYS_MESSAGE("Gamma16 module calculation Failed.");
            }
        }

        if (CamxResultSuccess == result)
        {
            UpdateIFEInternalData(pInputData);
        }
    }
    else
    {
        result = CamxResultEInvalidArg;
        CAMX_ASSERT_ALWAYS_MESSAGE("Invalid Input pointer");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEGamma16::UpdateIFEInternalData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IFEGamma16::UpdateIFEInternalData(
    ISPInputData* pInputData)
{
    // Post module enable info
    pInputData->pCalculatedData->moduleEnable.colorProcessingModuleConfig.bits.RGB_LUT_EN = m_moduleEnable;

    // post tuning metadata if setting is enabled
    if (NULL != pInputData->pIFETuningMetadata)
    {
        if (NULL != m_pGammaG[GammaLUTChannelG])
        {
            Utils::Memcpy(&pInputData->pIFETuningMetadata->IFEDMIData.packedLUT.gamma[GammaLUTChannelG],
                          m_pGammaG[GammaLUTChannelG],
                          Gamma16NumberOfEntriesPerLUT * sizeof(UINT32));
        }

        if (NULL != m_pGammaG[GammaLUTChannelB])
        {
            Utils::Memcpy(&pInputData->pIFETuningMetadata->IFEDMIData.packedLUT.gamma[GammaLUTChannelB],
                          m_pGammaG[GammaLUTChannelB],
                          Gamma16NumberOfEntriesPerLUT * sizeof(UINT32));
        }

        if (NULL != m_pGammaG[GammaLUTChannelR])
        {
            Utils::Memcpy(&pInputData->pIFETuningMetadata->IFEDMIData.packedLUT.gamma[GammaLUTChannelR],
                          m_pGammaG[GammaLUTChannelR],
                          Gamma16NumberOfEntriesPerLUT * sizeof(UINT32));
        }
    }

    if (NULL != m_pGammaG[GammaLUTChannelG])
    {
        for (UINT i = 0; i < Gamma16NumberOfEntriesPerLUT; i++)
        {
            // mask off any bit beyond 10th bit to remove delta in output gamma table
            pInputData->pCalculatedData->gammaOutput.gammaG[i] = m_pGammaG[GammaLUTChannelG][i] & GammaMask;
        }
        pInputData->pCalculatedData->gammaOutput.isGammaValid = TRUE;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEGamma16::GetRegCmd
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID* IFEGamma16::GetRegCmd()
{
    return static_cast<VOID*>(&m_regCmd);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEGamma16::CheckDependenceChange
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL IFEGamma16::CheckDependenceChange(
    ISPInputData* pInputData)
{
    BOOL                                isChanged      = FALSE;
    gamma_1_6_0::chromatix_gamma16Type* pChromatix     = NULL;
    TuningDataManager*                  pTuningManager = NULL;
    ISPHALTagsData*                     pHALTagsData   = pInputData->pHALTagsData;

    if ((NULL != pHALTagsData)               &&
        (NULL != pInputData->pHwContext)     &&
        (NULL != pInputData->pAECUpdateData) &&
        (NULL != pInputData->pAWBUpdateData))
    {
        pTuningManager = pInputData->pTuningDataManager;
        CAMX_ASSERT(NULL != pTuningManager);

        if (NULL != pInputData->pOEMIQSetting)
        {
            m_moduleEnable = (static_cast<OEMIFEIQSetting*>(pInputData->pOEMIQSetting))->GammaEnable;

            if (TRUE == m_moduleEnable)
            {
                isChanged = TRUE;
            }
        }
        else
        {
            CAMX_ASSERT(NULL != pInputData->pTuningData);

            if (TRUE == pTuningManager->IsValidChromatix())
            {
                pChromatix = pTuningManager->GetChromatix()->GetModule_gamma16_ife(
                    reinterpret_cast<TuningMode*>(&pInputData->pTuningData->TuningMode[0]),
                    pInputData->pTuningData->noOfSelectionParameter);
            }

            CAMX_ASSERT(NULL != pChromatix);

            if (NULL != pChromatix)
            {
                if ((NULL == m_dependenceData.pChromatix) ||
                    (m_dependenceData.pChromatix->SymbolTableID != pChromatix->SymbolTableID) ||
                    (pChromatix->enable_section.gamma_enable != m_moduleEnable))
                {
                    m_dependenceData.pChromatix = pChromatix;
                    m_moduleEnable              = pChromatix->enable_section.gamma_enable;
                    if (TRUE == m_moduleEnable)
                    {
                        isChanged = TRUE;
                    }
                }
            }

            // Check for manual control
            if (TonemapModeContrastCurve == pInputData->pHALTagsData->tonemapCurves.tonemapMode)
            {
                m_moduleEnable = FALSE;
                isChanged = FALSE;
            }

            if (TRUE == m_moduleEnable)
            {
                if ((TRUE ==
                    IQInterface::s_interpolationTable.gamma16TriggerUpdate(&pInputData->triggerData, &m_dependenceData)) ||
                    (TRUE == pInputData->forceTriggerUpdate))
                {
                    isChanged = TRUE;
                }
            }
        }

    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupISP,
                       "Invalid Input: pNewAECUpdate %x  pNewAWBupdate %x HwContext %x",
                       pInputData->pAECUpdateData,
                       pInputData->pAWBUpdateData,
                       pInputData->pHwContext);
    }

    return isChanged;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEGamma16::CreateCmdList
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFEGamma16::CreateCmdList(
    const ISPInputData* pInputData)
{
    CamxResult result       = CamxResultSuccess;
    CmdBuffer* pCmdBuffer   = pInputData->pCmdBuffer;
    UINT32     offset       = m_32bitDMIBufferOffsetDword * sizeof(UINT32);
    CmdBuffer* pDMIBuffer   = pInputData->p32bitDMIBuffer;
    UINT32     lengthInByte = Gamma16NumberOfEntriesPerLUT * sizeof(UINT32);

    CAMX_ASSERT(NULL != pCmdBuffer);
    CAMX_ASSERT(NULL != pDMIBuffer);

    // Can't pass &m_regCmd.rgb_lut_cfg directly since taking the
    // address of a packed struct member can cause alignment errors
    IFE_IFE_0_VFE_RGB_LUT_CFG rgb_lut_cfg = m_regCmd.rgb_lut_cfg;
    result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                          regIFE_IFE_0_VFE_RGB_LUT_CFG,
                                          IFEGamma16RegLengthDword,
                                          reinterpret_cast<UINT32*>(&rgb_lut_cfg));
    m_regCmd.rgb_lut_cfg = rgb_lut_cfg;

    result = PacketBuilder::WriteDMI(pCmdBuffer,
                                     regIFE_IFE_0_VFE_DMI_CFG,
                                     m_channelGLUTBankSelect,
                                     pDMIBuffer,
                                     offset,
                                     lengthInByte);
    CAMX_ASSERT(CamxResultSuccess == result);

    offset   += lengthInByte;
    result = PacketBuilder::WriteDMI(pCmdBuffer,
                                     regIFE_IFE_0_VFE_DMI_CFG,
                                     m_channelBLUTBankSelect,
                                     pDMIBuffer,
                                     offset,
                                     lengthInByte);
    CAMX_ASSERT(CamxResultSuccess == result);

    offset   += lengthInByte;
    result = PacketBuilder::WriteDMI(pCmdBuffer,
                                     regIFE_IFE_0_VFE_DMI_CFG,
                                     m_channelRLUTBankSelect,
                                     pDMIBuffer,
                                     offset,
                                     lengthInByte);
    CAMX_ASSERT(CamxResultSuccess == result);

    m_channelGLUTBankSelect = (m_channelGLUTBankSelect == GammaGLUTBank0) ? GammaGLUTBank1 : GammaGLUTBank0;
    m_channelBLUTBankSelect = (m_channelBLUTBankSelect == GammaBLUTBank0) ? GammaBLUTBank1 : GammaBLUTBank0;
    m_channelRLUTBankSelect = (m_channelRLUTBankSelect == GammaRLUTBank0) ? GammaRLUTBank1 : GammaRLUTBank0;

    // Although the DMI offsets in the h/w are different, each of the R,G,B Gamma LUTs will use the same LUT index (0/1)
    m_dependenceData.LUTBankSel ^= 1;

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEGamma16::RunCalculation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFEGamma16::RunCalculation(
    const ISPInputData* pInputData)
{
    CamxResult        result        = CamxResultSuccess;
    UINT32*           pGammaDMIAddr = reinterpret_cast<UINT32*>(pInputData->p32bitDMIBufferAddr + m_32bitDMIBufferOffsetDword);
    Gamma16OutputData outputData;

    outputData.type         = PipelineType::IFE;
    outputData.IFE.pRegCmd  = &m_regCmd;
    outputData.pGDMIDataPtr = reinterpret_cast<UINT32*>(pGammaDMIAddr);
    outputData.pBDMIDataPtr = reinterpret_cast<UINT32*>(reinterpret_cast<UCHAR*>(outputData.pGDMIDataPtr) +
                                                                                 IFEGamma16LutTableSize);
    outputData.pRDMIDataPtr = reinterpret_cast<UINT32*>(reinterpret_cast<UCHAR*>(outputData.pBDMIDataPtr) +
                                                                                 IFEGamma16LutTableSize);

    // @todo (CAMX-1829) Dual IFE changes for Gamma
    result = IQInterface::Gamma16CalculateSetting(&m_dependenceData, pInputData->pOEMIQSetting, &outputData, NULL);

    if (CamxResultSuccess == result)
    {
        m_pGammaG[GammaLUTChannelG] = outputData.pGDMIDataPtr;
        m_pGammaG[GammaLUTChannelB] = outputData.pBDMIDataPtr;
        m_pGammaG[GammaLUTChannelR] = outputData.pRDMIDataPtr;
    }
    else
    {
        m_pGammaG[GammaLUTChannelG] = NULL;
        m_pGammaG[GammaLUTChannelB] = NULL;
        m_pGammaG[GammaLUTChannelR] = NULL;
        CAMX_LOG_ERROR(CamxLogGroupISP, "Gamma Calculation Failed")
    }
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEGamma16::IFEGamma16
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IFEGamma16::IFEGamma16()
{
    m_type            = ISPIQModuleType::IFEGamma;
    m_cmdLength       = PacketBuilder::RequiredWriteRegRangeSizeInDwords(IFEGamma16RegLengthDword) +
                        (PacketBuilder::RequiredWriteDMISizeInDwords() * IFEGamma16NumDMITables);
    m_32bitDMILength  = IFEGamma16DMILengthDword;
    m_64bitDMILength  = 0;
    m_moduleEnable    = TRUE;

    // By default, Bank Selection set to 0
    m_channelGLUTBankSelect = GammaGLUTBank0;
    m_channelBLUTBankSelect = GammaBLUTBank0;
    m_channelRLUTBankSelect = GammaRLUTBank0;

    m_pGammaG[GammaLUTChannelG] = NULL;
    m_pGammaG[GammaLUTChannelB] = NULL;
    m_pGammaG[GammaLUTChannelR] = NULL;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IFEGamma16::~IFEGamma16
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IFEGamma16::~IFEGamma16()
{
}
CAMX_NAMESPACE_END
