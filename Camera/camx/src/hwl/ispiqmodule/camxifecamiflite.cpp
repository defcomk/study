////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2018 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxifecamiflite.cpp
/// @brief CAMXIFECAMIF class implementation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


#include "camxdefs.h"
#include "camxtuningdatamanager.h"
#include "parametertuningtypes.h"
#include "camxiqinterface.h"
#include "camxispiqmodule.h"
#include "camxnode.h"
#include "camxifecamiflite.h"

CAMX_NAMESPACE_BEGIN

static const UINT32 IFECAMIFLiteRegLengthDWord = sizeof(IFECAMIFLiteRegCmd) / sizeof(UINT32);

static const UINT32 IFECAMIFLITEEPOCHIRQLine0 = 19;      // Set to 20 line ( first line is counting as 0)
static const UINT32 IFECAMIFLITEEPOCHIRQLine1 = 0x3fff;  // Set to Maximum line since we only need the first irq

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFECAMIFLite::Create
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFECAMIFLite::Create(
    IFEModuleCreateData* pCreateData)
{
    CamxResult result = CamxResultSuccess;

    if (NULL != pCreateData)
    {
        pCreateData->pModule = CAMX_NEW IFECAMIFLite;

        if (NULL == pCreateData->pModule)
        {
            result = CamxResultEFailed;
            CAMX_ASSERT_ALWAYS_MESSAGE("Failed to create IFECAMIFLite object.");
        }
    }
    else
    {
        result = CamxResultEInvalidArg;
        CAMX_ASSERT_ALWAYS_MESSAGE("Null Input Data for IFECAMIFLite Creation");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFECAMIFLite::Execute
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFECAMIFLite::Execute(
    ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;

    CAMX_ASSERT(NULL != pInputData);

    if (TRUE == CheckDependenceChange(pInputData))
    {
        result = RunCalculation(pInputData);

        if (CamxResultSuccess == result)
        {
            result = CreateCmdList(pInputData);
        }
        else
        {
            CAMX_ASSERT_ALWAYS_MESSAGE("CAMIF module calculation Failed.");
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFECAMIFLite::CheckDependenceChange
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL IFECAMIFLite::CheckDependenceChange(
    ISPInputData* pInputData)
{
    BOOL  isChanged = FALSE;

    if (NULL != pInputData)
    {
        if (TRUE  == pInputData->dualPDPipelineData.programCAMIFLite)
        {
            isChanged = TRUE;
        }
    }

    return isChanged;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFECAMIFLite::CreateCmdList
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFECAMIFLite::CreateCmdList(
    ISPInputData* pInputData)
{
    CamxResult result     = CamxResultSuccess;
    CmdBuffer* pCmdBuffer = NULL;

    if (NULL != pInputData->pCmdBuffer)
    {
        pCmdBuffer = pInputData->pCmdBuffer;

        result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                              regIFE_IFE_0_VFE_CAMIF_LITE_CMD,
                                              IFECAMIFLiteRegLengthDWord,
                                              reinterpret_cast<UINT32*>(&m_regCmd));
        if (CamxResultSuccess != result)
        {
            CAMX_ASSERT_ALWAYS_MESSAGE("Failed to write command buffer");
        }
    }
    else
    {
        result = CamxResultEInvalidArg;
        CAMX_ASSERT_ALWAYS_MESSAGE("Invalid Input data or command buffer");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFECAMIFLite::RunCalculation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFECAMIFLite::RunCalculation(
    ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;

    /// @todo (CAMX-677) Set up the regiser value based on the input data
    m_regCmd.statusRegister.u32All                       = 0x0;
    m_regCmd.statusRegister.bitfields.CLEAR_CAMIF_STATUS = 0x1;

    m_regCmd.configRegister.u32All                       = 0x0;

    if (NULL != pInputData->pPDHwConfig)
    {
        m_regCmd.configRegister.bitfields.PD_OUTPUT_EN   = pInputData->pPDHwConfig->enablePDHw;
    }
    else
    {
        m_regCmd.configRegister.bitfields.PD_OUTPUT_EN   = 0;
    }

    m_regCmd.skipPeriodRegister.u32All                         = 0x0;
    m_regCmd.skipPeriodRegister.bitfields.IRQ_SUBSAMPLE_PERIOD = 0;

    m_regCmd.irqSubsamplePattern.bitfields.PATTERN       = 0x1;

    m_regCmd.irqEpochRegister.u32All                     = 0x0;
    m_regCmd.irqEpochRegister.bitfields.EPOCH1_LINE      = IFECAMIFLITEEPOCHIRQLine1;
    m_regCmd.irqEpochRegister.bitfields.EPOCH0_LINE      = IFECAMIFLITEEPOCHIRQLine0;

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFECAMIFLite::IFECAMIFLite
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IFECAMIFLite::IFECAMIFLite()
{
    m_type           = ISPIQModuleType::IFECAMIFLite;

    m_cmdLength      = PacketBuilder::RequiredWriteRegRangeSizeInDwords(IFECAMIFLiteRegLengthDWord);

    m_32bitDMILength = 0;
    m_64bitDMILength = 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IFECAMIFLite::~IFECAMIFLite
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IFECAMIFLite::~IFECAMIFLite()
{
}

CAMX_NAMESPACE_END
