////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016-2019 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file camximagesensordata.cpp
/// @brief Implements ImageSensorData methods. This will have sensor library data structure signatures
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "camxincs.h"
#include "camxmem.h"
#include "camxhwcontext.h"
#include "camxhwenvironment.h"
#include "camxsettingsmanager.h"
#include "camximagesensordata.h"
#include "camximagesensormoduledata.h"
#include "camximagesensormoduledatamanager.h"
#include "camximagesensorutils.h"
#include "camxpacketdefs.h"
#include "camxsensorproperty.h"
#include "imagesensormodulesetmanager.h"

CAMX_NAMESPACE_BEGIN

/// @brief Default value for Sensor Combo Mode, first two lanes are assigned for Combo Mode
///        Combo Mode has two sensors connect to the same CSIPHY
static const UINT16 SensorComboModeLaneMask = 0x18;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Instance Methods
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ImageSensorData::ImageSensorData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ImageSensorData::ImageSensorData(
    SensorDriverData*       pData,
    ImageSensorModuleData*  pModule)
{
    m_pSensorData = pData;
    m_pModuleData = pModule;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ImageSensorData::~ImageSensorData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ImageSensorData::~ImageSensorData()
{
    if (NULL != m_phSensorLibHandle)
    {
        OsUtils::LibUnmap(m_phSensorLibHandle);
        m_phSensorLibHandle = NULL;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ImageSensorData::GetInitSettingIndex
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
UINT ImageSensorData::GetInitSettingIndex(
    UINT32 sensorVersion)
{
    UINT selectedIndex = 0;

    if (0 != sensorVersion)
    {
        for (UINT i = 0; i < m_pSensorData->initSettingsCount; i ++)
        {
            if ((0 != m_pSensorData->initSettings[i].sensorVersion) &&
                (sensorVersion == m_pSensorData->initSettings[i].sensorVersion))
            {
                selectedIndex = i;
                break;
            }
        }
    }

    return selectedIndex;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ImageSensorData::GetResSettingIndex
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
UINT ImageSensorData::GetResSettingIndex(
    UINT32 sensorVersion)
{
    UINT selectedIndex = 0;

    if (0 != sensorVersion)
    {
        for (UINT i = 0; i < m_pSensorData->resolutionInfoCount; i ++)
        {
            if ((0 != m_pSensorData->resolutionInfo[i].sensorVersion) &&
                (sensorVersion == m_pSensorData->resolutionInfo[i].sensorVersion))
            {
                selectedIndex = i;
                break;
            }
        }
    }

    return selectedIndex;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ImageSensorData::ConfigureImageSensorData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID ImageSensorData::ConfigureImageSensorData(
    UINT32 sensorVersion)
{
    if (sensorVersion > 0)
    {
        m_initSettingIndex = GetInitSettingIndex(sensorVersion);
        m_resSettingIndex  = GetResSettingIndex(sensorVersion);
    }
    else
    {
        m_initSettingIndex = 0;
        m_resSettingIndex  = 0;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ImageSensorData::GetDigitalGain
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
UINT32 ImageSensorData::GetDigitalGain(
    FLOAT realGain,
    FLOAT realAnalogGain)
{
    UINT32 regData         = 0;
    DOUBLE realDigitalGain = 1.0;

    CAMX_ASSERT(0 != m_pSensorData->exposureControlInfo.realToRegDigitalGainConversionFactor);
    CAMX_ASSERT(0 != m_pSensorData->exposureControlInfo.maxAnalogGain);
    CAMX_ASSERT(0 != m_pSensorData->exposureControlInfo.maxDigitalGain);

    if (realGain > m_pSensorData->exposureControlInfo.maxAnalogGain)
    {
        realDigitalGain = realGain / realAnalogGain;
    }

    if (realDigitalGain > m_pSensorData->exposureControlInfo.maxDigitalGain)
    {
        realDigitalGain = m_pSensorData->exposureControlInfo.maxDigitalGain;
    }

    regData = static_cast<UINT32>(realDigitalGain * m_pSensorData->exposureControlInfo.realToRegDigitalGainConversionFactor);

    return regData;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ImageSensorData::RealToRegGain
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
UINT32 ImageSensorData::RealToRegGain(
    DOUBLE realGain)
{
    UINT32 regGain = 0;

    if (realGain < 1.0)
    {
        realGain = 1.0;
    }
    else if (realGain > m_pSensorData->exposureControlInfo.maxAnalogGain)
    {
        realGain = m_pSensorData->exposureControlInfo.maxAnalogGain;
    }

    /// @todo (CAMX-853) remove hard codings by obtaining real to reg gain from xml equations
    regGain = static_cast<UINT32>(512.0 - (512.0 / realGain));

    return regGain;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ImageSensorData::RegToRealGain
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
FLOAT ImageSensorData::RegToRealGain(
    UINT32 regGain)
{
    /// @todo (CAMX-853) remove hard codings by obtaining real to reg gain from xml equations
    return (512.0f / (512.0f - regGain));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ImageSensorData::UpdateExposureRegAddressInfo
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID ImageSensorData::UpdateExposureRegAddressInfo(
    VOID * pRegisterAddressInfo)
{
    SensorExposureRegInfo* pRegAddressInfo = reinterpret_cast<SensorExposureRegInfo*>(pRegisterAddressInfo);
    UINT32                 index           = 0;

    CAMX_ASSERT(MAX_GROUP_HOLD_REG_SETTINGS >= m_pSensorData->groupHoldOnSettings.regSettingCount);
    CAMX_ASSERT(MAX_GROUP_HOLD_REG_SETTINGS >= m_pSensorData->groupHoldOffSettings.regSettingCount);

    pRegAddressInfo->groupHoldOnSettings.regSettingCount = m_pSensorData->groupHoldOnSettings.regSettingCount;
    for (index = 0; index < m_pSensorData->groupHoldOnSettings.regSettingCount; index++)
    {
        pRegAddressInfo->groupHoldOnSettings.regSetting[index].operation        =
            static_cast<IOOperationType>(m_pSensorData->groupHoldOnSettings.regSetting[index].operation);

        pRegAddressInfo->groupHoldOnSettings.regSetting[index].registerAddr     =
            m_pSensorData->groupHoldOnSettings.regSetting[index].registerAddr;

        if ((OperationType::WRITE_BURST == m_pSensorData->groupHoldOnSettings.regSetting[index].operation) ||
            (OperationType::WRITE_SEQUENTIAL == m_pSensorData->groupHoldOnSettings.regSetting[index].operation))
        {
            pRegAddressInfo->groupHoldOnSettings.regSetting[index].registerDataSeq.count =
                m_pSensorData->groupHoldOnSettings.regSetting[index].registerDataCount;
            pRegAddressInfo->groupHoldOnSettings.regSetting[index].registerDataSeq.data =
                &(m_pSensorData->groupHoldOnSettings.regSetting[index].registerData[0]);
        }
        else
        {
            pRegAddressInfo->groupHoldOnSettings.regSetting[index].registerData  =
                m_pSensorData->groupHoldOnSettings.regSetting[index].registerData[0];
        }

        pRegAddressInfo->groupHoldOnSettings.regSetting[index].regAddrType      =
            static_cast<I2CRegAddressDataType>(m_pSensorData->groupHoldOnSettings.regSetting[index].regAddrType);
        pRegAddressInfo->groupHoldOnSettings.regSetting[index].regDataType      =
            static_cast<I2CRegAddressDataType>(m_pSensorData->groupHoldOnSettings.regSetting[index].regDataType);
        pRegAddressInfo->groupHoldOnSettings.regSetting[index].delayUs          =
            m_pSensorData->groupHoldOnSettings.regSetting[index].delayUs;
    }

    pRegAddressInfo->groupHoldOffSettings.regSettingCount = m_pSensorData->groupHoldOffSettings.regSettingCount;
    for (index = 0; index < m_pSensorData->groupHoldOffSettings.regSettingCount; index++)
    {
        pRegAddressInfo->groupHoldOffSettings.regSetting[index].operation       =
            static_cast<IOOperationType>(m_pSensorData->groupHoldOffSettings.regSetting[index].operation);
        pRegAddressInfo->groupHoldOffSettings.regSetting[index].registerAddr    =
            m_pSensorData->groupHoldOffSettings.regSetting[index].registerAddr;

        if ((OperationType::WRITE_BURST == m_pSensorData->groupHoldOffSettings.regSetting[index].operation) ||
            (OperationType::WRITE_SEQUENTIAL == m_pSensorData->groupHoldOffSettings.regSetting[index].operation))
        {
            pRegAddressInfo->groupHoldOffSettings.regSetting[index].registerDataSeq.count =
                m_pSensorData->groupHoldOffSettings.regSetting[index].registerDataCount;
            pRegAddressInfo->groupHoldOffSettings.regSetting[index].registerDataSeq.data =
                &(m_pSensorData->groupHoldOffSettings.regSetting[index].registerData[0]);
        }
        else
        {
            pRegAddressInfo->groupHoldOffSettings.regSetting[index].registerData    =
                m_pSensorData->groupHoldOffSettings.regSetting[index].registerData[0];
        }
        pRegAddressInfo->groupHoldOffSettings.regSetting[index].regAddrType     =
            static_cast<I2CRegAddressDataType>(m_pSensorData->groupHoldOffSettings.regSetting[index].regAddrType);
        pRegAddressInfo->groupHoldOffSettings.regSetting[index].regDataType     =
            static_cast<I2CRegAddressDataType>(m_pSensorData->groupHoldOffSettings.regSetting[index].regDataType);
        pRegAddressInfo->groupHoldOffSettings.regSetting[index].delayUs         =
            m_pSensorData->groupHoldOffSettings.regSetting[index].delayUs;
    }

    pRegAddressInfo->frameLengthLinesAddr       = m_pSensorData->regAddrInfo.frameLengthLines;
    pRegAddressInfo->coarseIntgTimeAddr         = m_pSensorData->regAddrInfo.coarseIntgTimeAddr;
    pRegAddressInfo->shortCoarseIntgTimeAddr    = m_pSensorData->regAddrInfo.shortCoarseIntgTimeAddr;
    pRegAddressInfo->globalAnalogGainAddr       = m_pSensorData->regAddrInfo.globalGainAddr;
    pRegAddressInfo->shortGlobalAnalogGainAddr  = m_pSensorData->regAddrInfo.shortGlobalGainAddr;
    pRegAddressInfo->GlobalDigitalGainAddr      = m_pSensorData->regAddrInfo.digitalGlobalGainAddr;
    pRegAddressInfo->digitalGainBlueAddr        = m_pSensorData->regAddrInfo.digitalGainBlueAddr;
    pRegAddressInfo->digitalGainGreenBlueAddr   = m_pSensorData->regAddrInfo.digitalGainGreenBlueAddr;
    pRegAddressInfo->digitalGainGreenRedAddr    = m_pSensorData->regAddrInfo.digitalGainGreenRedAddr;
    pRegAddressInfo->digitalGainRedAddr         = m_pSensorData->regAddrInfo.digitalGainRedAddr;

}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ImageSensorData::FillExposureArray2ByteIntegrationTime
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID ImageSensorData::FillExposureArray2ByteIntegrationTime(
    UINT32  analogGain,
    UINT32  digitalGain,
    UINT32  lineCount,
    UINT32  shortRegisterGain,
    UINT32  shortLinecount,
    UINT32  frameLengthLine,
    VOID*   pSetting,
    BOOL    applyShortExposure)
{
    RegSettingsInfo*    pRegSetting     = static_cast<RegSettingsInfo*>(pSetting);
    UINT32              index           = 0;
    UINT16              regCount        = 0;
    UINT32              digitalGainMSB  = (digitalGain & 0xFF00) >> 8;
    UINT32              digitalGainLSB  = digitalGain & 0xFF;

    for (index = 0; index < m_pSensorData->groupHoldOnSettings.regSettingCount; index++)
    {
        pRegSetting->regSetting[regCount].operation =
            static_cast<IOOperationType>(m_pSensorData->groupHoldOnSettings.regSetting[index].operation);
        pRegSetting->regSetting[regCount].registerAddr  =
            m_pSensorData->groupHoldOnSettings.regSetting[index].registerAddr;
        if ((OperationType::WRITE_BURST == m_pSensorData->groupHoldOnSettings.regSetting[index].operation) ||
            (OperationType::WRITE_SEQUENTIAL == m_pSensorData->groupHoldOnSettings.regSetting[index].operation))
        {
            pRegSetting->regSetting[regCount].registerDataSeq.count =
                m_pSensorData->groupHoldOnSettings.regSetting[index].registerDataCount;
            pRegSetting->regSetting[regCount].registerDataSeq.data =
                &(m_pSensorData->groupHoldOnSettings.regSetting[index].registerData[0]);
        }
        else
        {
            pRegSetting->regSetting[regCount].registerData  =
                m_pSensorData->groupHoldOnSettings.regSetting[index].registerData[0];
        }
        pRegSetting->regSetting[regCount].regAddrType   =
            static_cast<I2CRegAddressDataType>(m_pSensorData->groupHoldOnSettings.regSetting[index].regAddrType);
        pRegSetting->regSetting[regCount].regDataType   =
            static_cast<I2CRegAddressDataType>(m_pSensorData->groupHoldOnSettings.regSetting[index].regDataType);
        pRegSetting->regSetting[regCount].delayUs       =
            m_pSensorData->groupHoldOnSettings.regSetting[index].delayUs;
        regCount++;
    }
#if AALLALOU /* AALLALOU - only keep 1 entry to not crash */
    pRegSetting->regSetting[regCount].registerAddr  = m_pSensorData->regAddrInfo.frameLengthLines;
    pRegSetting->regSetting[regCount].registerData  = (frameLengthLine & 0xFF00) >> 8;
    regCount++;

    pRegSetting->regSetting[regCount].registerAddr  = m_pSensorData->regAddrInfo.frameLengthLines + 1;
    pRegSetting->regSetting[regCount].registerData  = (frameLengthLine & 0xFF);
    regCount++;

    pRegSetting->regSetting[regCount].registerAddr  = m_pSensorData->regAddrInfo.coarseIntgTimeAddr;
    pRegSetting->regSetting[regCount].registerData  = (lineCount & 0xFF00) >> 8;
    regCount++;

    pRegSetting->regSetting[regCount].registerAddr  = m_pSensorData->regAddrInfo.coarseIntgTimeAddr + 1;
    pRegSetting->regSetting[regCount].registerData  = (lineCount & 0xFF);
    regCount++;

    pRegSetting->regSetting[regCount].registerAddr  = m_pSensorData->regAddrInfo.globalGainAddr;
    pRegSetting->regSetting[regCount].registerData  = (analogGain & 0xFF00) >> 8;
    regCount++;

    pRegSetting->regSetting[regCount].registerAddr  = m_pSensorData->regAddrInfo.globalGainAddr+ 1;
    pRegSetting->regSetting[regCount].registerData  = (analogGain & 0xFF);
    regCount++;

    pRegSetting->regSetting[regCount].registerAddr  = m_pSensorData->regAddrInfo.digitalGainGreenRedAddr;
    pRegSetting->regSetting[regCount].registerData  = static_cast<UINT16>(digitalGainMSB);
    regCount++;

    pRegSetting->regSetting[regCount].registerAddr  = m_pSensorData->regAddrInfo.digitalGainGreenRedAddr + 1;
    pRegSetting->regSetting[regCount].registerData  = static_cast<UINT16>(digitalGainLSB);
    regCount++;

    pRegSetting->regSetting[regCount].registerAddr  = m_pSensorData->regAddrInfo.digitalGainRedAddr;
    pRegSetting->regSetting[regCount].registerData  = static_cast<UINT16>(digitalGainMSB);
    regCount++;

    pRegSetting->regSetting[regCount].registerAddr  = m_pSensorData->regAddrInfo.digitalGainRedAddr + 1;
    pRegSetting->regSetting[regCount].registerData  = static_cast<UINT16>(digitalGainLSB);
    regCount++;

    pRegSetting->regSetting[regCount].registerAddr  = m_pSensorData->regAddrInfo.digitalGainBlueAddr;
    pRegSetting->regSetting[regCount].registerData  = static_cast<UINT16>(digitalGainMSB);
    regCount++;

    pRegSetting->regSetting[regCount].registerAddr  = m_pSensorData->regAddrInfo.digitalGainBlueAddr + 1;
    pRegSetting->regSetting[regCount].registerData  = static_cast<UINT16>(digitalGainLSB);
    regCount++;

    pRegSetting->regSetting[regCount].registerAddr  = m_pSensorData->regAddrInfo.digitalGainGreenBlueAddr;
    pRegSetting->regSetting[regCount].registerData  = static_cast<UINT16>(digitalGainMSB);
    regCount++;

    pRegSetting->regSetting[regCount].registerAddr  = m_pSensorData->regAddrInfo.digitalGainGreenBlueAddr + 1;
    pRegSetting->regSetting[regCount].registerData  = static_cast<UINT16>(digitalGainLSB);
    regCount++;

    if (TRUE == applyShortExposure)
    {
        pRegSetting->regSetting[regCount].registerAddr  = m_pSensorData->regAddrInfo.shortCoarseIntgTimeAddr;
        pRegSetting->regSetting[regCount].registerData  = (shortLinecount & 0xFF00) >> 8;
        regCount++;

        pRegSetting->regSetting[regCount].registerAddr  = m_pSensorData->regAddrInfo.shortCoarseIntgTimeAddr + 1;
        pRegSetting->regSetting[regCount].registerData  = (shortLinecount & 0xFF);
        regCount++;

        pRegSetting->regSetting[regCount].registerAddr  = m_pSensorData->regAddrInfo.shortGlobalGainAddr;
        pRegSetting->regSetting[regCount].registerData  = (shortRegisterGain & 0xFF00) >> 8;
        regCount++;

        pRegSetting->regSetting[regCount].registerAddr  = m_pSensorData->regAddrInfo.shortGlobalGainAddr + 1;
        pRegSetting->regSetting[regCount].registerData  = (shortRegisterGain & 0xFF);
        regCount++;
    }

    for (index = 0; (m_pSensorData->groupHoldOnSettings.regSettingCount + index) < regCount; index++)
    {
        pRegSetting->regSetting[m_pSensorData->groupHoldOnSettings.regSettingCount + index].regAddrType  =
            I2CRegAddressDataTypeWord;
        pRegSetting->regSetting[m_pSensorData->groupHoldOnSettings.regSettingCount + index].regDataType  =
            I2CRegAddressDataTypeByte;
        pRegSetting->regSetting[m_pSensorData->groupHoldOnSettings.regSettingCount + index].delayUs      =
            0;
    }

    for (index = 0; index < m_pSensorData->groupHoldOffSettings.regSettingCount; index++)
    {
        pRegSetting->regSetting[regCount].operation =
            static_cast<IOOperationType>(m_pSensorData->groupHoldOffSettings.regSetting[index].operation);
        pRegSetting->regSetting[regCount].registerAddr  =
            m_pSensorData->groupHoldOffSettings.regSetting[index].registerAddr;
        if ((OperationType::WRITE_BURST == m_pSensorData->groupHoldOffSettings.regSetting[index].operation) ||
            (OperationType::WRITE_SEQUENTIAL == m_pSensorData->groupHoldOffSettings.regSetting[index].operation))
        {
            pRegSetting->regSetting[regCount].registerDataSeq.count =
                m_pSensorData->groupHoldOffSettings.regSetting[index].registerDataCount;
            pRegSetting->regSetting[regCount].registerDataSeq.data =
                &(m_pSensorData->groupHoldOffSettings.regSetting[index].registerData[0]);
        }
        else
        {
            pRegSetting->regSetting[regCount].registerData  =
                m_pSensorData->groupHoldOffSettings.regSetting[index].registerData[0];
        }
        pRegSetting->regSetting[regCount].regAddrType   =
            static_cast<I2CRegAddressDataType>(m_pSensorData->groupHoldOffSettings.regSetting[index].regAddrType);
        pRegSetting->regSetting[regCount].regDataType   =
            static_cast<I2CRegAddressDataType>(m_pSensorData->groupHoldOffSettings.regSetting[index].regDataType);
        pRegSetting->regSetting[regCount].delayUs       =
            m_pSensorData->groupHoldOffSettings.regSetting[index].delayUs;
        regCount++;
    }
#else
    CAMX_UNREFERENCED_PARAM(analogGain);
    CAMX_UNREFERENCED_PARAM(lineCount);
    CAMX_UNREFERENCED_PARAM(shortRegisterGain);
    CAMX_UNREFERENCED_PARAM(shortLinecount);
    CAMX_UNREFERENCED_PARAM(frameLengthLine);
    CAMX_UNREFERENCED_PARAM(applyShortExposure);
#endif /* AALLALOU */
    CAMX_ASSERT(MAX_REG_SETTINGS > regCount);
    pRegSetting->regSettingCount = regCount;

}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ImageSensorData::GetLineLengthPixelClk
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
UINT16 ImageSensorData::GetLineLengthPixelClk(
    UINT resolutionIndex
    ) const
{
    return static_cast<UINT16>(GetResolutionInfo()->resolutionData[resolutionIndex].lineLengthPixelClock);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ImageSensorData::GetVTPixelClk
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
UINT32 ImageSensorData::GetVTPixelClk(
    UINT resolutionIndex
    ) const
{
    ResolutionData* pResData   = &GetResolutionInfo()->resolutionData[resolutionIndex];

    return static_cast<UINT32>(pResData->lineLengthPixelClock * pResData->frameLengthLines * pResData->frameRate);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ImageSensorData::GetResolutionInfo
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const ResolutionInformation* ImageSensorData::GetResolutionInfo() const
{
    return &m_pSensorData->resolutionInfo[m_resSettingIndex];
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ImageSensorData::Get3ExposureHDRInfo
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
HDR3ExposureInformation ImageSensorData::Get3ExposureHDRInfo(
    UINT resolutionIndex
    ) const
{
    return m_pSensorData->resolutionInfo[m_resSettingIndex].resolutionData[resolutionIndex].HDR3ExposureInfo;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ImageSensorData::GetExposureControlInfo
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const ExposureContorlInformation* ImageSensorData::GetExposureControlInfo() const
{
    return &m_pSensorData->exposureControlInfo;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ImageSensorData::GetActiveArraySize
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const PixelArrayInfo* ImageSensorData::GetActiveArraySize() const
{
    return &m_pSensorData->pixelArrayInfo;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ImageSensorData::GetMaxFrameLengthLines
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
UINT32 ImageSensorData::GetMaxFrameLengthLines(
    UINT resolutionIndex
    ) const
{
    /// @todo (CAMX-555)  get min FPS from 3A chromatix and then remove the MinimumToMaximumFPSRatio
    return static_cast<UINT32>(static_cast<FLOAT>(GetResolutionInfo()->resolutionData[resolutionIndex].frameLengthLines) *
                               MaximumToMinimumFPSRatio);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ImageSensorData::GetFrameLengthLines
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
UINT32 ImageSensorData::GetFrameLengthLines(
    UINT resolutionIndex
    ) const
{
    return GetResolutionInfo()->resolutionData[resolutionIndex].frameLengthLines;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ImageSensorData::GetColorFilterPattern
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ColorFilterArrangement ImageSensorData::GetColorFilterPattern(
    UINT resolutionIndex
    ) const
{
    return (GetResolutionInfo()->resolutionData[resolutionIndex].colorFilterArrangement);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ImageSensorData::GetMaxFPS
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DOUBLE ImageSensorData::GetMaxFPS(
    UINT resolutionIndex
    ) const
{
    return GetResolutionInfo()->resolutionData[resolutionIndex].frameRate;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ImageSensorData::GetMinFPS
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DOUBLE ImageSensorData::GetMinFPS(
    UINT resolutionIndex
    ) const
{
    /// @todo (CAMX-555)  get min FPS from 3A chromatix and then remove the MinimumToMaximumFPSRatio
    return (GetMaxFPS(resolutionIndex) * MinimumToMaximumFPSRatio);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ImageSensorData::GetAnalogRealGain
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
FLOAT ImageSensorData::GetAnalogRealGain(
    VOID* pExposureInfoParam)
{
    SensorExposureInfo* pExposureInfo = reinterpret_cast<SensorExposureInfo*>(pExposureInfoParam);

    return pExposureInfo->analogRealGain;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ImageSensorData::GetDigitalRealGain
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
FLOAT ImageSensorData::GetDigitalRealGain(
    VOID* pExposureInfoParam)
{
    SensorExposureInfo* pExposureInfo = reinterpret_cast<SensorExposureInfo*>(pExposureInfoParam);

    return pExposureInfo->digitalRealGain;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ImageSensorData::GetAnalogRegGain
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
UINT ImageSensorData::GetAnalogRegGain(
    VOID* pExposureInfoParam)
{
    SensorExposureInfo* pExposureInfo = reinterpret_cast<SensorExposureInfo*>(pExposureInfoParam);

    return pExposureInfo->analogRegisterGain;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ImageSensorData::GetDigitalRegGain
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
UINT ImageSensorData::GetDigitalRegGain(
    VOID* pExposureInfoParam)
{
    SensorExposureInfo* pExposureInfo = reinterpret_cast<SensorExposureInfo*>(pExposureInfoParam);

    return pExposureInfo->digitalRegisterGain;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ImageSensorData::GetShortAnalogRegGain
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
UINT ImageSensorData::GetShortAnalogRegGain(
    VOID* pExposureInfoParam)
{
    SensorExposureInfo* pExposureInfo = reinterpret_cast<SensorExposureInfo*>(pExposureInfoParam);

    return pExposureInfo->shortRegisterGain;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ImageSensorData::GetISPDigitalGain
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
FLOAT ImageSensorData::GetISPDigitalGain(
    VOID* pExposureInfoParam)
{
    SensorExposureInfo* pExposureInfo = reinterpret_cast<SensorExposureInfo*>(pExposureInfoParam);

    return pExposureInfo->ISPDigitalGain;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ImageSensorData::CalculateExposure
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID ImageSensorData::CalculateExposure(
    FLOAT realGain,
    UINT  lineCount,
    FLOAT middleRealGain,
    UINT  middleLinecount,
    FLOAT shortRealGain,
    UINT  shortLinecount,
    VOID* pExposureInfoParam)
{
    SensorCalculateExposureData calculateExposureData = {0};
    SensorExposureInfo*         pExposureInfo         = reinterpret_cast<SensorExposureInfo*>(pExposureInfoParam);

    if (NULL != m_sensorLibraryAPI.pCalculateExposure)
    {
        calculateExposureData.realGain        = realGain;
        calculateExposureData.lineCount       = lineCount;
        calculateExposureData.middleRealGain  = middleRealGain;
        calculateExposureData.middleLinecount = middleLinecount;
        calculateExposureData.shortRealGain   = shortRealGain;
        calculateExposureData.shortLinecount  = shortLinecount;

        if (TRUE != m_sensorLibraryAPI.pCalculateExposure(pExposureInfo, &calculateExposureData))
        {
            CAMX_LOG_ERROR(CamxLogGroupSensor, "Calculate exposure failed");
        }
    }
    else
    {
        CAMX_ASSERT(0 != m_pSensorData->exposureControlInfo.realToRegDigitalGainConversionFactor);

        pExposureInfo->analogRegisterGain   = RealToRegGain(realGain);
        pExposureInfo->analogRealGain       = RegToRealGain(pExposureInfo->analogRegisterGain);
        pExposureInfo->digitalRegisterGain  = GetDigitalGain(realGain, pExposureInfo->analogRealGain);
        pExposureInfo->digitalRealGain      = static_cast<FLOAT>(pExposureInfo->digitalRegisterGain /
                                                m_pSensorData->exposureControlInfo.realToRegDigitalGainConversionFactor);
        pExposureInfo->ISPDigitalGain       = realGain /(pExposureInfo->analogRealGain * pExposureInfo->digitalRealGain);
        pExposureInfo->lineCount            = lineCount;
        pExposureInfo->shortRegisterGain    = RealToRegGain(shortRealGain);
        pExposureInfo->shortLinecount       = shortLinecount;
        pExposureInfo->middleRegisterGain   = RealToRegGain(middleRealGain);
        pExposureInfo->middleLinecount      = middleLinecount;
    }
    CAMX_LOG_VERBOSE(CamxLogGroupSensor,
        "Long (TG:%f AG:%f DG:%f ISPG:%f LC:%d) Middle (AG:%f DG:%f LC:%d) Short (AG:%f DG:%f LC:%d)",
        realGain,
        pExposureInfo->analogRealGain,
        pExposureInfo->digitalRealGain,
        pExposureInfo->ISPDigitalGain,
        pExposureInfo->lineCount,
        pExposureInfo->middleAnalogRealGain,
        pExposureInfo->middleDigitalRealGain,
        pExposureInfo->middleLinecount,
        pExposureInfo->shortAnalogRealGain,
        pExposureInfo->shortDigitalRealGain,
        pExposureInfo->shortLinecount);

}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ImageSensorData::FillExposureArray
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID ImageSensorData::FillExposureArray(
    VOID*  pExposureInfoParams,
    UINT   frameLengthLine,
    VOID*  pAdditionalInfo,
    VOID*  pRegAddressInfo,
    VOID*  pRegSettings,
    BOOL   applyShortExposure,
    BOOL   applyMiddleExposure,
    UINT32 sensorResolutionIndex)
{
    SensorFillExposureData  exposureData        = {0};
    SensorExposureInfo*     pExposureInfo       = reinterpret_cast<SensorExposureInfo*>(pExposureInfoParams);
    RegSettingsInfo*        pRegSettingsInfo    = reinterpret_cast<RegSettingsInfo*>(pRegSettings);
    SensorExposureRegInfo*  pRegAddress         = reinterpret_cast<SensorExposureRegInfo*>(pRegAddressInfo);

    if (NULL != m_sensorLibraryAPI.pFillExposureSettings)
    {
        exposureData.pRegInfo                    = pRegAddress;
        exposureData.analogRegisterGain          = pExposureInfo->analogRegisterGain;
        exposureData.digitalRegisterGain         = pExposureInfo->digitalRegisterGain;
        exposureData.lineCount                   = pExposureInfo->lineCount;
        exposureData.shortLineCount              = pExposureInfo->shortLinecount;
        exposureData.middleLineCount             = pExposureInfo->middleLinecount;
        exposureData.shortRegisterGain           = pExposureInfo->shortRegisterGain;
        exposureData.shortDigitalRegisterGain    = pExposureInfo->shortDigitalRegisterGain;
        exposureData.middleRegisterGain          = pExposureInfo->middleRegisterGain;
        exposureData.middleDigitalRegisterGain   = pExposureInfo->middleDigitalRegisterGain;
        exposureData.frameLengthLines            = frameLengthLine;
        exposureData.applyShortExposure          = applyShortExposure;
        exposureData.applyMiddleExposure         = applyMiddleExposure;
        exposureData.sensitivityCorrectionFactor = pExposureInfo->sensitivityCorrectionFactor;
        exposureData.sensorResolutionIndex       = sensorResolutionIndex;


        if (NULL != pAdditionalInfo)
        {
            Utils::Memcpy(exposureData.additionalInfo, pAdditionalInfo, (MAX_REG_CONTROL_EXT_DATA_SIZE * sizeof(INT32)));
        }

        if (TRUE != m_sensorLibraryAPI.pFillExposureSettings(pRegSettingsInfo, &exposureData))
        {
            CAMX_LOG_ERROR(CamxLogGroupSensor, "FillExposureSettings failed");
        }
    }
    else
    {
        FillExposureArray2ByteIntegrationTime(pExposureInfo->analogRegisterGain,
                                              pExposureInfo->digitalRegisterGain,
                                              pExposureInfo->lineCount,
                                              pExposureInfo->shortRegisterGain,
                                              pExposureInfo->shortLinecount,
                                              frameLengthLine,
                                              pRegSettingsInfo,
                                              applyShortExposure);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ImageSensorData::FillPDAFArray
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL ImageSensorData::FillPDAFArray(
    SensorFillPDAFData* pSensorPDAFData,
    VOID*               pRegSettings)
{
    RegSettingsInfo* pRegSettingsInfo = reinterpret_cast<RegSettingsInfo*>(pRegSettings);
    BOOL             result           = FALSE;

    if ((NULL != m_sensorLibraryAPI.pFillPDAFSettings) &&
        (NULL != pRegSettings))
    {
        result = m_sensorLibraryAPI.pFillPDAFSettings(pRegSettingsInfo, pSensorPDAFData);
        if (FALSE == result)
        {
            CAMX_LOG_ERROR(CamxLogGroupSensor, "pFillPDAFSettings failed for SENSOR");
        }
    }
    else
    {
        CAMX_LOG_INFO(CamxLogGroupSensor, "Fill PDAF ARRAY not supported for this Sensor");
    }
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ImageSensorData::FillWBGainArray
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL ImageSensorData::FillWBGainArray(
    SensorFillWBGainData* pSensorWBGainData,
    VOID*                 pRegSettings)
{
    RegSettingsInfo* pRegSettingsInfo = reinterpret_cast<RegSettingsInfo*>(pRegSettings);
    BOOL             result           = FALSE;
    FLOAT            rGain            = pSensorWBGainData->RGain;
    FLOAT            gGain            = pSensorWBGainData->GGain;
    FLOAT            bGain            = pSensorWBGainData->BGain;

    if ((NULL != m_sensorLibraryAPI.pFillAutoWhiteBalanceSettings) &&
        (NULL != pRegSettings))
    {
        result = m_sensorLibraryAPI.pFillAutoWhiteBalanceSettings(pRegSettingsInfo, rGain, gGain, bGain);
        if (FALSE == result)
        {
            CAMX_LOG_ERROR(CamxLogGroupSensor, "FillWBGainSettings failed for SENSOR");
        }
    }
    else
    {
        CAMX_LOG_INFO(CamxLogGroupSensor, "Fill WB gain ARRAY not supported for this Sensor");
    }
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ImageSensorData::GetISO100Gain
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
FLOAT ImageSensorData::GetISO100Gain(
    UINT32 cameraID)
{
    CamxResult          result         = CamxResultSuccess;
    FLOAT               ISO100Gain     = 0.0f;
    TuningDataManager*  pTuningManager = HwEnvironment::GetInstance()->GetTuningDataManager(cameraID);
    TuningMode          selectors[1]   = { { ModeType::Default, { 0 } } };

    result = ImageSensorUtils::GetISO100GainData(pTuningManager, selectors, &ISO100Gain);

    if ((CamxResultSuccess != result) || (0.0f == ISO100Gain))
    {
        CAMX_LOG_WARN(CamxLogGroupSensor, "failed to get ISO100gain/invalid. use default");
        ISO100Gain = 1.0f;
    }

    return ISO100Gain;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ImageSensorData::GainToSensitivity
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
INT32 ImageSensorData::GainToSensitivity(
    FLOAT  gain,
    FLOAT  ISO100Gain,
    VOID*  pAdditionalInfo)
{
    INT32 sensitivity = 0;

    if (0.0f == ISO100Gain)
    {
        ISO100Gain = 1.0f;
    }

    if ((NULL != m_sensorLibraryAPI.pGetSensitivity) &&
        (NULL != pAdditionalInfo))
    {
        sensitivity = m_sensorLibraryAPI.pGetSensitivity((gain / ISO100Gain) * 100,
                                                          static_cast<INT32 *>(pAdditionalInfo));
    }
    else
    {
        sensitivity = Utils::RoundFLOAT((gain / ISO100Gain) * 100);
    }
    return sensitivity;

}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ImageSensorData::GetLineReadoutTime
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DOUBLE ImageSensorData::GetLineReadoutTime(
    UINT    resolutionIndex)
{
    DOUBLE lineLengthPixelClock = static_cast<DOUBLE>(GetLineLengthPixelClk(resolutionIndex));
    DOUBLE VTPixelClock         = static_cast<DOUBLE>(GetVTPixelClk(resolutionIndex));
    DOUBLE lineReadoutTime      = 0;

    CAMX_ASSERT(0 != VTPixelClock);
    CAMX_ASSERT(0 != lineLengthPixelClock);

    if ( 0.0f != VTPixelClock)
    {
        lineReadoutTime = (lineLengthPixelClock * NanoSecondsPerSecond) / VTPixelClock;
    }
    else
    {
        CAMX_LOG_WARN(CamxLogGroupSensor, "VTPixelClock is 0, cannot calculate lineReadoutTime");
    }

    return lineReadoutTime;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ImageSensorData::GetFrameReadoutTime
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
UINT64 ImageSensorData::GetFrameReadoutTime(
    UINT32  currentFrameLengthLines,
    UINT    resolutionIndex)
{
    DOUBLE lineReadoutTime = GetLineReadoutTime(resolutionIndex);

    return static_cast<UINT64>(lineReadoutTime * currentFrameLengthLines);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ImageSensorData::GetRollingShutterSkew
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
UINT64 ImageSensorData::GetRollingShutterSkew(
    UINT32  frameHeight,
    UINT    resolutionIndex)
{
    DOUBLE lineReadoutTime = GetLineReadoutTime(resolutionIndex);

    return static_cast<UINT64>(Utils::RoundDOUBLE(lineReadoutTime * frameHeight));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ImageSensorData::GetExposureStartTime
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
UINT64 ImageSensorData::GetExposureStartTime(
    UINT64  sofTimeStamp,
    UINT64  currentExposure,
    UINT    resolutionIndex)
{
    UINT64 retVal = 0;
    const ResolutionInformation* pResolutionInfo = GetResolutionInfo();
    UINT64 lineReadoutTime = static_cast<UINT64>(Utils::RoundDOUBLE(GetLineReadoutTime(resolutionIndex)));

    if (0 != sofTimeStamp)
    {
        // All the time calculations are in nanoseconds
        // ADCReadoutTime is specified in milliseconds in sensor driver
        retVal = static_cast<UINT64>(
            sofTimeStamp - lineReadoutTime - currentExposure - (m_pSensorData->sensorProperty.ADCReadoutTime * 1000000));
        CAMX_LOG_VERBOSE(CamxLogGroupSensor, "sofTimeStamp %llu LineReadoutTime %llu currentExposure %llu ADCReadoutTime %f",
                                              sofTimeStamp, lineReadoutTime, currentExposure,
                                              (m_pSensorData->sensorProperty.ADCReadoutTime * 1000000));
    }

    return retVal;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ImageSensorData::SensitivityToGain
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
FLOAT ImageSensorData::SensitivityToGain(
    INT32  sensitivity,
    FLOAT  ISO100Gain)
{
    if (0.0f == ISO100Gain)
    {
        ISO100Gain = 1.0f;
    }

    return (static_cast<FLOAT>(sensitivity) / 100) * ISO100Gain;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ImageSensorData::ExposureToLineCount
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
UINT ImageSensorData::ExposureToLineCount(
    UINT64  exposureTimeNS,
    UINT32  resolutionIndex)
{
    DOUBLE lineCount    = static_cast<DOUBLE>(exposureTimeNS) / GetLineReadoutTime(resolutionIndex);
    DOUBLE minLineCount = static_cast<DOUBLE>(GetMinLineCount());
    if (minLineCount > lineCount)
    {
        lineCount = minLineCount;
    }
    return static_cast<UINT>(Utils::RoundDOUBLE(lineCount));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ImageSensorData::LineCountToExposure
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
UINT64 ImageSensorData::LineCountToExposure(
    UINT    lineCount,
    UINT32  resolutionIndex)
{
    return static_cast<UINT64>(Utils::RoundDOUBLE(lineCount * GetLineReadoutTime(resolutionIndex)));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ImageSensorData::GetNoiseProfile
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID ImageSensorData::GetNoiseProfile(
    DOUBLE*                pNoiseProfile,
    ColorFilterArrangement colorFilterArrangement,
    INT32                  sensitivity,
    FLOAT                  ISO100Gain)
{
    switch (colorFilterArrangement)
    {
        case ColorFilterArrangement::BAYER_BGGR:
            pNoiseProfile[0] = GetNoiseModelEntryS(sensitivity, NoiseCoefficientChannel::BAYER_B);
            pNoiseProfile[1] = GetNoiseModelEntryO(sensitivity, ISO100Gain, NoiseCoefficientChannel::BAYER_B);
            pNoiseProfile[2] = GetNoiseModelEntryS(sensitivity, NoiseCoefficientChannel::BAYER_Gb);
            pNoiseProfile[3] = GetNoiseModelEntryO(sensitivity, ISO100Gain, NoiseCoefficientChannel::BAYER_Gb);
            pNoiseProfile[4] = GetNoiseModelEntryS(sensitivity, NoiseCoefficientChannel::BAYER_Gr);
            pNoiseProfile[5] = GetNoiseModelEntryO(sensitivity, ISO100Gain, NoiseCoefficientChannel::BAYER_Gr);
            pNoiseProfile[6] = GetNoiseModelEntryS(sensitivity, NoiseCoefficientChannel::BAYER_R);
            pNoiseProfile[7] = GetNoiseModelEntryO(sensitivity, ISO100Gain, NoiseCoefficientChannel::BAYER_R);
            break;
        case ColorFilterArrangement::BAYER_GBRG:
            pNoiseProfile[0] = GetNoiseModelEntryS(sensitivity, NoiseCoefficientChannel::BAYER_Gb);
            pNoiseProfile[1] = GetNoiseModelEntryO(sensitivity, ISO100Gain, NoiseCoefficientChannel::BAYER_Gb);
            pNoiseProfile[2] = GetNoiseModelEntryS(sensitivity, NoiseCoefficientChannel::BAYER_B);
            pNoiseProfile[3] = GetNoiseModelEntryO(sensitivity, ISO100Gain, NoiseCoefficientChannel::BAYER_B);
            pNoiseProfile[4] = GetNoiseModelEntryS(sensitivity, NoiseCoefficientChannel::BAYER_R);
            pNoiseProfile[5] = GetNoiseModelEntryO(sensitivity, ISO100Gain, NoiseCoefficientChannel::BAYER_R);
            pNoiseProfile[6] = GetNoiseModelEntryS(sensitivity, NoiseCoefficientChannel::BAYER_Gr);
            pNoiseProfile[7] = GetNoiseModelEntryO(sensitivity, ISO100Gain, NoiseCoefficientChannel::BAYER_Gr);
            break;
        case ColorFilterArrangement::BAYER_GRBG:
            pNoiseProfile[0] = GetNoiseModelEntryS(sensitivity, NoiseCoefficientChannel::BAYER_Gr);
            pNoiseProfile[1] = GetNoiseModelEntryO(sensitivity, ISO100Gain, NoiseCoefficientChannel::BAYER_Gr);
            pNoiseProfile[2] = GetNoiseModelEntryS(sensitivity, NoiseCoefficientChannel::BAYER_R);
            pNoiseProfile[3] = GetNoiseModelEntryO(sensitivity, ISO100Gain, NoiseCoefficientChannel::BAYER_R);
            pNoiseProfile[4] = GetNoiseModelEntryS(sensitivity, NoiseCoefficientChannel::BAYER_B);
            pNoiseProfile[5] = GetNoiseModelEntryO(sensitivity, ISO100Gain, NoiseCoefficientChannel::BAYER_B);
            pNoiseProfile[6] = GetNoiseModelEntryS(sensitivity, NoiseCoefficientChannel::BAYER_Gb);
            pNoiseProfile[7] = GetNoiseModelEntryO(sensitivity, ISO100Gain, NoiseCoefficientChannel::BAYER_Gb);
            break;
        case ColorFilterArrangement::BAYER_RGGB:
            pNoiseProfile[0] = GetNoiseModelEntryS(sensitivity, NoiseCoefficientChannel::BAYER_R);
            pNoiseProfile[1] = GetNoiseModelEntryO(sensitivity, ISO100Gain, NoiseCoefficientChannel::BAYER_R);
            pNoiseProfile[2] = GetNoiseModelEntryS(sensitivity, NoiseCoefficientChannel::BAYER_Gr);
            pNoiseProfile[3] = GetNoiseModelEntryO(sensitivity, ISO100Gain, NoiseCoefficientChannel::BAYER_Gr);
            pNoiseProfile[4] = GetNoiseModelEntryS(sensitivity, NoiseCoefficientChannel::BAYER_Gb);
            pNoiseProfile[5] = GetNoiseModelEntryO(sensitivity, ISO100Gain, NoiseCoefficientChannel::BAYER_Gb);
            pNoiseProfile[6] = GetNoiseModelEntryS(sensitivity, NoiseCoefficientChannel::BAYER_B);
            pNoiseProfile[7] = GetNoiseModelEntryO(sensitivity, ISO100Gain, NoiseCoefficientChannel::BAYER_B);
            break;
        default:
            CAMX_LOG_ERROR(CamxLogGroupSensor, "Unsupported colorFilterArrange format %d", colorFilterArrangement);
    }
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ImageSensorData::GetNoiseModelEntryS
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DOUBLE ImageSensorData::GetNoiseModelEntryS(
    INT32                   sensitivity,
    NoiseCoefficientChannel channel)
{
    NoiseCoefficent noiseCoeff = {0.0};

    GetNoiseCoeff(&noiseCoeff, channel);

    CAMX_LOG_VERBOSE(CamxLogGroupSensor,
                     "noise coeff gradient_S:%e,offset_S: %e, gradient_O:%e, offset_O%e",
                     noiseCoeff.gradient_S,
                     noiseCoeff.offset_S,
                     noiseCoeff.gradient_O,
                     noiseCoeff.offset_O);

    DOUBLE noiseModelEntryS = noiseCoeff.gradient_S * sensitivity + noiseCoeff.offset_S;

    return ((noiseModelEntryS < 0.0) ? 0.0 : noiseModelEntryS);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ImageSensorData::GetNoiseModelEntryO
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DOUBLE ImageSensorData::GetNoiseModelEntryO(
    INT32                   sensitivity,
    FLOAT                   ISO100Gain,
    NoiseCoefficientChannel channel)
{
    NoiseCoefficent noiseCoeff = {0.0};

    GetNoiseCoeff(&noiseCoeff, channel);

    DOUBLE maxAnalogSensitvity = m_pSensorData->exposureControlInfo.maxAnalogGain * (100.0f / ISO100Gain);
    DOUBLE digitalGain         = sensitivity / maxAnalogSensitvity;

    digitalGain = (digitalGain < 1.0) ? 1.0 : digitalGain;

    CAMX_LOG_VERBOSE(CamxLogGroupSensor, "sensitivity:%d, ISO100Gain: %f, digitalGain:%lf",
        sensitivity, ISO100Gain, digitalGain);

    DOUBLE noiseModelEntryO = (noiseCoeff.gradient_O * sensitivity * sensitivity) +
                              (noiseCoeff.offset_O * digitalGain * digitalGain);

    return ((noiseModelEntryO < 0.0) ? 0.0 : noiseModelEntryO);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ImageSensorData::GetNoiseCoeff
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID ImageSensorData::GetNoiseCoeff(
    NoiseCoefficent*        pNoiseCoeff,
    NoiseCoefficientChannel channel
    ) const
{
    if (NULL != pNoiseCoeff)
    {
        if (m_pSensorData->noiseCoefficientBayerExists)
        {
            switch (channel)
            {
                case NoiseCoefficientChannel::BAYER_R:
                    *pNoiseCoeff = m_pSensorData->noiseCoefficientBayer.R;
                    break;
                case NoiseCoefficientChannel::BAYER_Gr:
                    *pNoiseCoeff = m_pSensorData->noiseCoefficientBayer.Gr;
                    break;
                case NoiseCoefficientChannel::BAYER_Gb:
                    *pNoiseCoeff = m_pSensorData->noiseCoefficientBayer.Gb;
                    break;
                case NoiseCoefficientChannel::BAYER_B:
                    *pNoiseCoeff = m_pSensorData->noiseCoefficientBayer.B;
                    break;
                default:
                    CAMX_LOG_ERROR(CamxLogGroupSensor, "Unsupported channel format %d", channel);
            }
        }
        /// @todo (CAMX-4356) clean up noiseCoefficient in sensor xsd
        else if (m_pSensorData->noiseCoefficentExists)
        {
            *pNoiseCoeff = (m_pSensorData->noiseCoefficent);
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ImageSensorData::GetSensorStaticCapability
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult ImageSensorData::GetSensorStaticCapability(
    SensorModuleStaticCaps* pCapability,
    UINT32                  cameraID)
{
    CamxResult                   result          = CamxResultSuccess;
    const ResolutionInformation* pResolutionInfo = GetResolutionInfo();
    FLOAT                        ISO100Gain      = GetISO100Gain(cameraID);
    HwCameraInfo                 hwcamerraInfo;

    result = HwEnvironment::GetInstance()->GetCameraInfo(cameraID, &hwcamerraInfo);
    if (CamxResultSuccess == result)
    {
        Utils::Memcpy(&pCapability->OTPData, &hwcamerraInfo.pSensorCaps->OTPData, sizeof(EEPROMOTPData));
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupSensor, "Failed to obtain EEPROM OTP data");
    }

    PopulateLensPoseInformation(pCapability, &pCapability->OTPData);

    CAMX_ASSERT(MaxResolutions >= pResolutionInfo->resolutionDataCount);

    // Punt any modes that we don't want to expose
    UINT resolutionCount = 0;
    for (UINT resolutionIndex = 0; resolutionIndex < pResolutionInfo->resolutionDataCount; resolutionIndex++)
    {
        CAMX_LOG_VERBOSE(CamxLogGroupSensor, "Checking framerate for resolution mode[%d] = %f", resolutionIndex,
            pResolutionInfo->resolutionData[resolutionIndex].frameRate);
        HwEnvironment* pEnv = m_pModuleData->GetDataManager()->GetHwEnvironment();

        if (pEnv->GetSettingsManager()->GetStaticSettings()->forceMaxFPS >=
            pResolutionInfo->resolutionData[resolutionIndex].frameRate)
        {
            if (resolutionCount != resolutionIndex)
            {
                Utils::Memcpy(&pResolutionInfo->resolutionData[resolutionCount],
                    &pResolutionInfo->resolutionData[resolutionIndex],
                    sizeof(ResolutionData));
            }
            resolutionCount++;
        }
        else
        {
            CAMX_LOG_VERBOSE(CamxLogGroupSensor, "Hiding mode[%d]", resolutionIndex);
        }
    }
    m_pSensorData->resolutionInfo[m_resSettingIndex].resolutionDataCount = resolutionCount;
    CAMX_LOG_VERBOSE(CamxLogGroupSensor, "Total number of resolutions exposed to driver %d",
        pCapability->numSensorConfigs);


    /// Any resolution index can be used, so resolution index 0 is taken as reference to find whether its YUV camera or not.
    pCapability->isYUVCamera = ImageSensorUtils::IsYUVCamera(&(pResolutionInfo->resolutionData[0]));
    pCapability->numSensorConfigs = pResolutionInfo->resolutionDataCount;

    for (UINT resolutionIndex = 0; resolutionIndex < pResolutionInfo->resolutionDataCount; resolutionIndex++)
    {
        pCapability->sensorConfigs[resolutionIndex].maxFPS          = GetMaxFPS(resolutionIndex);
        pCapability->sensorConfigs[resolutionIndex].minFPS          =
            Utils::Ceiling(static_cast<FLOAT>(GetMinFPS(resolutionIndex)));
        pCapability->sensorConfigs[resolutionIndex].numStreamConfig =
            pResolutionInfo->resolutionData[resolutionIndex].streamInfo.streamConfigurationCount;

        CAMX_LOG_VERBOSE(CamxLogGroupSensor, "sensor resolutionIndex:%d fps min:max %lf:%lf", resolutionIndex,
             pCapability->sensorConfigs[resolutionIndex].minFPS, pCapability->sensorConfigs[resolutionIndex].maxFPS);

        for (UINT streamConfigIndex = 0;
             streamConfigIndex < pCapability->sensorConfigs[resolutionIndex].numStreamConfig;
             streamConfigIndex++)
        {
            StreamConfiguration* pStreamConfig                  =
                &pResolutionInfo->resolutionData[resolutionIndex].streamInfo.streamConfiguration[streamConfigIndex];
            SensorStreamConfiguration* pStreamConfigCapability  =
                &pCapability->sensorConfigs[resolutionIndex].streamConfigs[streamConfigIndex];

            pStreamConfigCapability->virtualChannel             = pStreamConfig->vc;
            pStreamConfigCapability->dataType                   = pStreamConfig->dt;
            pStreamConfigCapability->dimension.width            = pStreamConfig->frameDimension.width;
            pStreamConfigCapability->dimension.height           = pStreamConfig->frameDimension.height;
            pStreamConfigCapability->bitWidth                   = pStreamConfig->bitWidth;
            pStreamConfigCapability->streamType                 = pStreamConfig->type;
        }
    }

    pCapability->activeArraySize.xMin       = m_pSensorData->pixelArrayInfo.dummyInfo.left;
    pCapability->activeArraySize.yMin       = m_pSensorData->pixelArrayInfo.dummyInfo.top;
    pCapability->activeArraySize.width      = m_pSensorData->pixelArrayInfo.activeDimension.width;
    pCapability->activeArraySize.height     = m_pSensorData->pixelArrayInfo.activeDimension.height;

    pCapability->physicalSensorSize.width   = ImageSensorUtils::CalculatePhysicalSensorWidth(m_pSensorData);
    pCapability->physicalSensorSize.height  = ImageSensorUtils::CalculatePhysicalSensorHeight(m_pSensorData);

    pCapability->pixelArraySize.width       = ImageSensorUtils::CalculateTotalPixelArrayWidth(m_pSensorData);
    pCapability->pixelArraySize.height      = ImageSensorUtils::CalculateTotalPixelArrayHeight(m_pSensorData);

    pCapability->pixelSize                  = static_cast<FLOAT>(m_pSensorData->sensorProperty.pixelSize);

    if (TRUE == HwEnvironment::GetInstance()->GetStaticSettings()->capResolutionForSingleIFE)
    {
        pCapability->preCorrectionActiveArraySize.xMin   = pCapability->activeArraySize.xMin;
        pCapability->preCorrectionActiveArraySize.yMin   = pCapability->activeArraySize.yMin;
        pCapability->preCorrectionActiveArraySize.width  =
            HwEnvironment::GetInstance()->GetStaticSettings()->singleIFESupportedWidth;
        pCapability->preCorrectionActiveArraySize.height =
            HwEnvironment::GetInstance()->GetStaticSettings()->singleIFESupportedHeight;

        // Clamp the preCorrectionActiveArraySize if it is greater than pixelArraySize
        if (pCapability->preCorrectionActiveArraySize.width > pCapability->activeArraySize.width)
        {
            pCapability->preCorrectionActiveArraySize.width  = pCapability->activeArraySize.width;
        }

        if (pCapability->preCorrectionActiveArraySize.height > pCapability->activeArraySize.height)
        {
            pCapability->preCorrectionActiveArraySize.height  = pCapability->activeArraySize.height;
        }
    }
    else
    {
        // Pre corrective array will be same as active array in the abscence of geometric distortion info
        pCapability->preCorrectionActiveArraySize.xMin   = pCapability->activeArraySize.xMin;
        pCapability->preCorrectionActiveArraySize.yMin   = pCapability->activeArraySize.yMin;
        pCapability->preCorrectionActiveArraySize.width  = pCapability->activeArraySize.width;
        pCapability->preCorrectionActiveArraySize.height = pCapability->activeArraySize.height;
    }

    FLOAT ISOFactor = (100.0f / ISO100Gain);

    pCapability->minISOSensitivity    = ImageSensorUtils::CalculateMinimumISOSensitivity(m_pSensorData, ISOFactor);
    pCapability->maxISOSensitivity    = ImageSensorUtils::CalculateMaximumISOSensitivity(m_pSensorData, ISOFactor);
    pCapability->maxAnalogSensitivity = static_cast<INT32>(m_pSensorData->exposureControlInfo.maxAnalogGain * ISOFactor);

    /// If patterns are not specified, default it to solid color mode.
    if (0 == m_pSensorData->testPatternInfo.testPatternDataCount)
    {
        pCapability->numTestPatterns = 1;
        pCapability->testPatterns[0] = static_cast<INT32>(TestPatternMode::SOLID_COLOR);
    }
    else
    {
        pCapability->numTestPatterns = m_pSensorData->testPatternInfo.testPatternDataCount;
        for (UINT testPatternIndex = 0; testPatternIndex < pCapability->numTestPatterns; testPatternIndex++)
        {
            pCapability->testPatterns[testPatternIndex] =
                static_cast<INT32>(m_pSensorData->testPatternInfo.testPatternData[testPatternIndex].mode);
        }
    }

    pCapability->whiteLevel             = m_pSensorData->colorLevelInfo.whiteLevel;
    pCapability->blackLevelPattern[0]   = m_pSensorData->colorLevelInfo.rPedestal;
    pCapability->blackLevelPattern[1]   = m_pSensorData->colorLevelInfo.grPedestal;
    pCapability->blackLevelPattern[2]   = m_pSensorData->colorLevelInfo.gbPedestal;
    pCapability->blackLevelPattern[3]   = m_pSensorData->colorLevelInfo.bPedestal;

    pCapability->numOpticalBlackRegions = m_pSensorData->opticalBlackRegionInfo.dimensionCount;

    CAMX_ASSERT(MaxOpticalBlackRegions >= pCapability->numOpticalBlackRegions);

    for (UINT blackRegionIndex = 0; blackRegionIndex < pCapability->numOpticalBlackRegions; blackRegionIndex++)
    {
        pCapability->opticalBlackRegion[blackRegionIndex].xMin     =
            m_pSensorData->opticalBlackRegionInfo.dimension[blackRegionIndex].xStart;
        pCapability->opticalBlackRegion[blackRegionIndex].yMin     =
            m_pSensorData->opticalBlackRegionInfo.dimension[blackRegionIndex].yStart;
        pCapability->opticalBlackRegion[blackRegionIndex].width    =
            m_pSensorData->opticalBlackRegionInfo.dimension[blackRegionIndex].width;
        pCapability->opticalBlackRegion[blackRegionIndex].height   =
            m_pSensorData->opticalBlackRegionInfo.dimension[blackRegionIndex].height;
    }

    if (FALSE == pCapability->isYUVCamera)
    {
        UINT64 verticalOffset               = m_pSensorData->exposureControlInfo.verticalOffset;

        pCapability->colorFilterArrangement =
            TranslateColorFilterArrangement(pResolutionInfo->resolutionData[0].colorFilterArrangement);
        pCapability->minExposureTime        = 0;
        pCapability->maxExposureTime        = ULLONG_MAX;
        pCapability->maxFrameDuration       = ULLONG_MAX;

        for (UINT resolutionIndex = 0; resolutionIndex < pResolutionInfo->resolutionDataCount; resolutionIndex++)
        {
            INT64   lineTime     = 0;
            UINT64  maxValue     = 0;
            UINT64  maxDuration  = 0;
            INT64   minLineCount = 0;

            minLineCount = Utils::RoundDOUBLE(static_cast<DOUBLE>(GetMinLineCount()));
            lineTime     =
                Utils::RoundDOUBLE(((static_cast<DOUBLE>(GetLineLengthPixelClk(resolutionIndex)) *
                                   NanoSecondsPerSecond) /
                                   static_cast<DOUBLE>(GetVTPixelClk(resolutionIndex))));
            pCapability->minExposureTime    = Utils::MaxUINT64((lineTime * minLineCount), pCapability->minExposureTime);
            maxValue                        = lineTime * m_pSensorData->exposureControlInfo.maxLineCount;
            // note: GetMaxFrameLengthLines is needed for sensors that limit maxExposureTime but not maxFrameDuration
            maxDuration                     = lineTime * Utils::MaxUINT64(
                GetMaxFrameLengthLines(resolutionIndex),
                m_pSensorData->exposureControlInfo.maxLineCount + verticalOffset);
            pCapability->maxExposureTime    = Utils::MinUINT64(maxValue, pCapability->maxExposureTime);
            pCapability->maxFrameDuration   = Utils::MinUINT64(maxDuration, pCapability->maxFrameDuration);
        }
    }

    // check if a Quad CFA sensor
    for (UINT i = 0; i < pResolutionInfo->resolutionDataCount; i++)
    {
        ResolutionData* pResolutionData = &pResolutionInfo->resolutionData[i];
        for (UINT j = 0; j < pResolutionData->capabilityCount; j++)
        {
            if (SensorCapability::QUADCFA == pResolutionData->capability[j])
            {
                StreamConfiguration* pStreamConfiguration = &pResolutionData->streamInfo.streamConfiguration[0];
                pCapability->QuadCFADim.width             = pStreamConfiguration->frameDimension.width;
                pCapability->QuadCFADim.height            = pStreamConfiguration->frameDimension.height;
                pCapability->isQuadCFASensor              = TRUE;

                CAMX_LOG_INFO(CamxLogGroupSensor, "Quad CFA sensor, Quad CFA raw dim:%dx%d",
                    pCapability->QuadCFADim.width,
                    pCapability->QuadCFADim.height);

                break;
            }
        }

        if (TRUE == pCapability->isQuadCFASensor)
        {
            break;
        }
    }

    if (TRUE == HwEnvironment::GetInstance()->GetStaticSettings()->overrideForceFSCapable)
    {
        pCapability->isFSSensor = TRUE;
    }
    else
    {
        // check if it's a Fast shutter sensor
        for (UINT i = 0; i < pResolutionInfo->resolutionDataCount; i++)
        {
            ResolutionData* pResolutionData = &pResolutionInfo->resolutionData[i];
            for (UINT j = 0; j < pResolutionData->capabilityCount; j++)
            {
                if (SensorCapability::FS == pResolutionData->capability[j])
                {
                    pCapability->isFSSensor = TRUE;
                    CAMX_LOG_VERBOSE(CamxLogGroupSensor, "Fast shutter supported sensor");
                    break;
                }
            }

            if (TRUE == pCapability->isFSSensor)
            {
                break;
            }
        }
    }

    // check for zzHDR mode
    for (UINT i = 0; i < pResolutionInfo->resolutionDataCount; i++)
    {
        ResolutionData* pResolutionData = &pResolutionInfo->resolutionData[i];
        for (UINT j = 0; j < pResolutionData->capabilityCount; j++)
        {
            if (SensorCapability::ZZHDR == pResolutionData->capability[j])
            {
                pCapability->isZZHDRSupported = TRUE;
                CAMX_LOG_VERBOSE(CamxLogGroupSensor, "zzHDR is Supported");
                break;
            }
        }
        if (TRUE == pCapability->isZZHDRSupported)
        {
            break;
        }
    }

    // Check for depth sensor
    for (UINT i = 0; i < pResolutionInfo->resolutionDataCount; i++)
    {
        ResolutionData* pResolutionData = &pResolutionInfo->resolutionData[i];
        for (UINT j = 0; j < pResolutionData->capabilityCount; j++)
        {
            if (SensorCapability::DEPTH == pResolutionData->capability[j])
            {
                pCapability->isDepthSensor = TRUE;
                CAMX_LOG_VERBOSE(CamxLogGroupSensor, "Depth is supported");
                break;
            }
        }
        if (TRUE == pCapability->isDepthSensor)
        {
            break;
        }
    }

    // check for IHDR mode
    for (UINT i = 0; i < pResolutionInfo->resolutionDataCount; i++)
    {
        ResolutionData* pResolutionData = &pResolutionInfo->resolutionData[i];
        for (UINT j = 0; j < pResolutionData->capabilityCount; j++)
        {
            if (SensorCapability::IHDR == pResolutionData->capability[j])
            {
                pCapability->isIHDRSupported = TRUE;
                CAMX_LOG_VERBOSE(CamxLogGroupSensor, "IHDR is Supported");
                break;
            }
        }
        if (TRUE == pCapability->isIHDRSupported)
        {
            break;
        }
        CAMX_LOG_VERBOSE(CamxLogGroupSensor, "IHDR Supported");
    }

    // Check for SHDR supported
    for (UINT i = 0; i < pResolutionInfo->resolutionDataCount; i++)
    {
        ResolutionData* pResolutionData = &pResolutionInfo->resolutionData[i];
        for (UINT j = 0; j < pResolutionData->capabilityCount; j++)
        {
            if (SensorCapability::SHDR == pResolutionData->capability[j])
            {
                pCapability->isSHDRSupported = TRUE;
                CAMX_LOG_VERBOSE(CamxLogGroupSensor, "SHDR is supported");
                break;
            }
        }
        if (TRUE == pCapability->isSHDRSupported)
        {
            break;
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ImageSensorData::PopulateLensPoseInformation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID ImageSensorData::PopulateLensPoseInformation(
    SensorModuleStaticCaps* pCapability,
    EEPROMOTPData*          pOTPData)
{
    FLOAT          lensTranslationValue = 0;
    FLOAT          focalLength          = 0;
    UINT           sensorPosition       = 0;
    FLOAT          divisor              = 1000.0;
    FLOAT          pixelPitchInMM       = 1.0;
    CameraPosition cameraPosition;

    if (NULL != pCapability)
    {

        if (0 < m_pSensorData->sensorProperty.pixelSize)
        {
            pixelPitchInMM = (static_cast<FLOAT>(m_pSensorData->sensorProperty.pixelSize)) / divisor;
        }

        const LensInformation* pLensInformation = m_pModuleData->GetLensInfo();
        if (NULL != pLensInformation)
        {
            focalLength = (static_cast<FLOAT>(pLensInformation->focalLength)) / pixelPitchInMM;
        }

        m_pModuleData->GetCameraPosition(&sensorPosition);
        cameraPosition = static_cast<CameraPosition>(sensorPosition);

        if ((NULL != pOTPData) && (TRUE == pOTPData->dualCameraCalibration.isAvailable))
        {
            if ((CameraPosition::REAR_AUX == cameraPosition) || (CameraPosition::FRONT_AUX == cameraPosition))
            {
                lensTranslationValue =
                    (pOTPData->dualCameraCalibration.systemCalibrationData.relativeBaselineDistance/divisor);
            }
        }

        // LENS_POSE_TRANSLATION: 3D vector that describes lens optical centre
        // Default: [0, 0, 0] for Primary camera
        //          [relative_baseline_distance/1000, 0, 0] for Aux camera
        pCapability->lensPoseTranslationDC[0] = lensTranslationValue;
        pCapability->lensPoseTranslationDC[1] = 0;
        pCapability->lensPoseTranslationDC[2] = 0;
        pCapability->lensPoseTranslationCount = LensPoseTranslationSize;

        // LENS_POSE_ROTATION: Four coordinates that describe quaternion rotation
        // from the Android sensor coordinate system to a camera-aligned coordinate system
        // Default is [0, 0, 0, 1]
        pCapability->lensPoseRotationDC[0] = 0;
        pCapability->lensPoseRotationDC[1] = 0;
        pCapability->lensPoseRotationDC[2] = 0;
        pCapability->lensPoseRotationDC[3] = 1.0;
        pCapability->lensPoseRotationCount = LensPoseRotationSize;

        // LENS_INTRINSIC_CALIBRATION: Five calibration parameters to define coordinate translations
        // Default: [FocalLength, FocalLength, 0, 0, 0]
        pCapability->lensIntrinsicCalibrationDC[0] = focalLength;
        pCapability->lensIntrinsicCalibrationDC[1] = focalLength;
        pCapability->lensIntrinsicCalibrationDC[2] = 0;
        pCapability->lensIntrinsicCalibrationDC[3] = 0;
        pCapability->lensIntrinsicCalibrationDC[4] = 0;
        pCapability->lensIntrinsicCalibrationCount = LensIntrinsicCalibrationSize;

        // LENS_POSE_REFERENCE: PRIMARY_CAMERA
        // Default: 0
        pCapability->lensPoseReferenceDC = 0;

        // LENS_DISTORTION: Five correction coefficients that correct radial and tangential lens distortion
        // Default: [0, 0, 0, 0, 0]
        pCapability->lensDistortionDC[0] = 0;
        pCapability->lensDistortionDC[1] = 0;
        pCapability->lensDistortionDC[2] = 0;
        pCapability->lensDistortionDC[3] = 0;
        pCapability->lensDistortionDC[4] = 0;
        pCapability->lensDistortionCount = LensDistortionSize;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ImageSensorData::PopulateSensorModeData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID ImageSensorData::PopulateSensorModeData(
    UsecaseSensorModes* pSensorModeData,
    UINT32              CSIPHYSlotInfo,
    BOOL                isComboMode)
{
    /// @todo (CAMX-555) META/RDI data
    const ResolutionInformation* pResolutionInfo = GetResolutionInfo();

    CAMX_ASSERT(pResolutionInfo->resolutionDataCount <= MAX_RESOLUTION_MODES);

    for (UINT i = 0; i < pResolutionInfo->resolutionDataCount; i++)
    {
        pSensorModeData->allModes[i].outPixelClock           = pResolutionInfo->resolutionData[i].outputPixelClock;
        pSensorModeData->allModes[i].vtPixelClock            = GetVTPixelClk(i);
        pSensorModeData->allModes[i].maxFPS                  = pResolutionInfo->resolutionData[i].frameRate;

        pSensorModeData->allModes[i].capabilityCount         = pResolutionInfo->resolutionData[i].capabilityCount;
        for (UINT index = 0; index < pResolutionInfo->resolutionData[i].capabilityCount; index++)
        {
            pSensorModeData->allModes[i].capability[index]   = pResolutionInfo->resolutionData[i].capability[index];
        }

        pSensorModeData->allModes[i].binningTypeH            = pResolutionInfo->resolutionData[i].horizontalBinning;
        pSensorModeData->allModes[i].binningTypeV            = pResolutionInfo->resolutionData[i].verticalBinning;
        pSensorModeData->allModes[i].numPixelsPerLine        = pResolutionInfo->resolutionData[i].lineLengthPixelClock;
        pSensorModeData->allModes[i].numLinesPerFrame        = pResolutionInfo->resolutionData[i].frameLengthLines;
        pSensorModeData->allModes[i].minHorizontalBlanking   = pResolutionInfo->resolutionData[i].minHorizontalBlanking;
        pSensorModeData->allModes[i].minVerticalBlanking     = pResolutionInfo->resolutionData[i].minVerticalBlanking;

        pSensorModeData->allModes[i].resolution.outputWidth  =
            pResolutionInfo->resolutionData[i].streamInfo.streamConfiguration[0].frameDimension.width;
        pSensorModeData->allModes[i].resolution.outputHeight =
            pResolutionInfo->resolutionData[i].streamInfo.streamConfiguration[0].frameDimension.height;

        pSensorModeData->allModes[i].offset.xStart  =
            pResolutionInfo->resolutionData[i].streamInfo.streamConfiguration[0].frameDimension.xStart;
        pSensorModeData->allModes[i].offset.yStart =
            pResolutionInfo->resolutionData[i].streamInfo.streamConfiguration[0].frameDimension.yStart;

        pSensorModeData->allModes[i].streamConfigCount       =
            pResolutionInfo->resolutionData[i].streamInfo.streamConfigurationCount;
        Utils::Memcpy(pSensorModeData->allModes[i].streamConfig,
                            pResolutionInfo->resolutionData[i].streamInfo.streamConfiguration,
                            (sizeof(StreamConfiguration) *
                                pResolutionInfo->resolutionData[i].streamInfo.streamConfigurationCount));

        pSensorModeData->allModes[i].CSIPHYId                = CSIPHYSlotInfo;
        pSensorModeData->allModes[i].laneCount               = pResolutionInfo->resolutionData[i].laneCount;
        pSensorModeData->allModes[i].is3Phase                = pResolutionInfo->resolutionData[i].is3Phase;

        if (pResolutionInfo->resolutionData[i].downScaleFactor < 1.0)
        {
            pSensorModeData->allModes[i].downScaleFactor     = 1.0;
        }
        else
        {
            pSensorModeData->allModes[i].downScaleFactor     = pResolutionInfo->resolutionData[i].downScaleFactor;
        }

        pSensorModeData->allModes[i].laneCount               = pResolutionInfo->resolutionData[i].laneCount;
        if (FALSE == isComboMode)
        {
            pSensorModeData->allModes[i].laneMask            = (1 << (pResolutionInfo->resolutionData[i].laneCount + 1)) - 1;
        }
        else
        {
            pSensorModeData->allModes[i].laneMask            = SensorComboModeLaneMask;
        }

        pSensorModeData->allModes[i].format                  =
            static_cast<PixelFormat>(pResolutionInfo->resolutionData[i].colorFilterArrangement);
        pSensorModeData->allModes[i].maxLineCount            = m_pSensorData->exposureControlInfo.maxLineCount;
        pSensorModeData->allModes[i].maxGain                 = m_pSensorData->exposureControlInfo.maxDigitalGain *
            m_pSensorData->exposureControlInfo.maxAnalogGain;

        pSensorModeData->allModes[i].cropInfo.firstPixel     =
            pResolutionInfo->resolutionData[i].cropInfo.left;
        pSensorModeData->allModes[i].cropInfo.lastPixel      =
            pSensorModeData->allModes[i].resolution.outputWidth - pResolutionInfo->resolutionData[i].cropInfo.right - 1;
        pSensorModeData->allModes[i].cropInfo.firstLine      =
            pResolutionInfo->resolutionData[i].cropInfo.top;
        pSensorModeData->allModes[i].cropInfo.lastLine       =
            pSensorModeData->allModes[i].resolution.outputHeight - pResolutionInfo->resolutionData[i].cropInfo.bottom - 1;
        if (0 != pResolutionInfo->resolutionData[i].ZZHDRInfoExists)
        {
            pSensorModeData->allModes[i].ZZHDRColorPattern =
                static_cast<UINT>(pResolutionInfo->resolutionData[i].ZZHDRInfo.ZZHDRPattern);
            pSensorModeData->allModes[i].ZZHDRFirstExposurePattern =
                static_cast<UINT>(pResolutionInfo->resolutionData[i].ZZHDRInfo.ZZHDRFirstExposure);
        }

        if (0 != pResolutionInfo->resolutionData[i].HDR3ExposureInfoExists)
        {
            pSensorModeData->allModes[i].HDR3ExposureTypeInfo =
                static_cast<UINT>(pResolutionInfo->resolutionData[i].HDR3ExposureInfo);
        }

        if (0 != pResolutionInfo->resolutionData[i].RemosaicTypeInfoExists)
        {
            pSensorModeData->allModes[i].RemosaicType =
                static_cast<UINT>(pResolutionInfo->resolutionData[i].RemosaicTypeInfo);
        }
    }
    return;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ImageSensorData::LoadSensorLibrary
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult ImageSensorData::LoadSensorLibrary()
{
    CamxResult  result                                      = CamxResultSuccess;
    CHAR        symbolName[]                                = "GetSensorLibraryAPIs";
    UINT16      fileCount                                   = 0;
    CHAR        libFiles[MaxSensorModules][FILENAME_MAX]    = {""};

    fileCount = OsUtils::GetFilesFromPath(SensorModulesPath,
                                          FILENAME_MAX,
                                          &libFiles[0][0],
                                          "*",
                                          "sensor",
                                          m_pSensorData->slaveInfo.sensorName,
                                          SharedLibraryExtension);

    if (0 != fileCount)
    {
        CAMX_ASSERT(1 == fileCount);

        m_phSensorLibHandle = OsUtils::LibMap(&libFiles[0][0]);

        if (NULL != m_phSensorLibHandle)
        {
            typedef VOID(*GetSensorLibraryAPIs)(SensorLibraryAPI* pSensorLibraryAPI);

            GetSensorLibraryAPIs pfnGetSensorLibraryAPI =
                reinterpret_cast<GetSensorLibraryAPIs>(OsUtils::LibGetAddr(m_phSensorLibHandle, symbolName));
            if (NULL != pfnGetSensorLibraryAPI)
            {
                pfnGetSensorLibraryAPI(&m_sensorLibraryAPI);
                CAMX_LOG_INFO(CamxLogGroupSensor,
                              "sensor library loaded for %s, version Major: %d, Minor: %d",
                              m_pSensorData->slaveInfo.sensorName,
                              m_sensorLibraryAPI.majorVersion,
                              m_sensorLibraryAPI.minorVersion);
            }
            else
            {
                result = CamxResultEFailed;
                CAMX_LOG_ERROR(CamxLogGroupSensor, "Couldn't find symbol: %s in %s", symbolName, &libFiles[0][0]);
            }
        }
        else
        {
            result = CamxResultEFailed;
            CAMX_LOG_ERROR(CamxLogGroupSensor, "Unable to open sensor library: %s", &libFiles[0][0]);
        }
    }
    else
    {
        CAMX_LOG_INFO(CamxLogGroupSensor, "sensor library not present for %s", m_pSensorData->slaveInfo.sensorName);
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ImageSensorData::Allocate
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID* ImageSensorData::Allocate(
    SensorAllocateType type)
{
    VOID* pAllocatedPtr = NULL;

    switch (type)
    {
        case SensorAllocateType::I2CRegSetting:
            pAllocatedPtr = static_cast<VOID*>(CAMX_CALLOC(sizeof(RegSettingsInfo)));
            break;

        case SensorAllocateType::ExposureInfo:
            pAllocatedPtr = static_cast<VOID*>(CAMX_CALLOC(sizeof(SensorExposureInfo)));
            break;

        case SensorAllocateType::ExposureRegAddressInfo:
            pAllocatedPtr = static_cast<VOID*>(CAMX_CALLOC(sizeof(SensorExposureRegInfo)));
            break;

        default:
            CAMX_LOG_ERROR(CamxLogGroupSensor, "Unsupported allocate request in Sensor with type: %d", type);
            break;
    }

    return pAllocatedPtr;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ImageSensorData::CreateI2CInfoCmd
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult ImageSensorData::CreateI2CInfoCmd(
    CSLSensorI2CInfo* pI2CInfoCmd)
{
    CamxResult result = CamxResultSuccess;

    result = ImageSensorUtils::CreateI2CInfoCmd(pI2CInfoCmd,
                                                static_cast<UINT16>(m_pSensorData->slaveInfo.slaveAddress),
                                                m_pSensorData->slaveInfo.i2cFrequencyMode);
    return result;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ImageSensorData::CreateProbeCmd
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult ImageSensorData::CreateProbeCmd(
    CSLSensorProbeCmd* pProbeCmd,
    UINT16             cameraId)
{
    CamxResult result = CamxResultSuccess;

    pProbeCmd->dataType      = static_cast<UINT8>(m_pSensorData->slaveInfo.regDataType);
    pProbeCmd->addrType      = static_cast<UINT8>(m_pSensorData->slaveInfo.regAddrType);
    pProbeCmd->opcode        = CSLPacketOpcodesSensorProbe;
    pProbeCmd->cmdType       = CSLSensorCmdTypeProbe;
    pProbeCmd->regAddr       = m_pSensorData->slaveInfo.sensorIdRegAddr;
    pProbeCmd->expectedData  = m_pSensorData->slaveInfo.sensorId;
    pProbeCmd->dataMask      = m_pSensorData->slaveInfo.sensorIdMask;
    pProbeCmd->cameraId      = cameraId;
    pProbeCmd->pipelineDelay = static_cast<UINT16>(m_pSensorData->delayInfo.maxPipeline);

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ImageSensorData::GetPowerSequenceCmdSize
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
UINT ImageSensorData::GetPowerSequenceCmdSize(
    BOOL isPowerUp)
{

    UINT          totalSize         = 0;
    UINT          powerSequenceSize = 0;
    PowerSetting* pSettings         = NULL;

    powerSequenceSize = (TRUE == isPowerUp) ?
                        m_pSensorData->slaveInfo.powerUpSequence.powerSettingCount :
                        m_pSensorData->slaveInfo.powerDownSequence.powerSettingCount;

    pSettings         = (TRUE == isPowerUp) ?
                        m_pSensorData->slaveInfo.powerUpSequence.powerSetting :
                        m_pSensorData->slaveInfo.powerDownSequence.powerSetting;

    totalSize = ImageSensorUtils::GetPowerSequenceCmdSize(powerSequenceSize, pSettings);
    return totalSize;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ImageSensorData::CreatePowerSequenceCmd
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult ImageSensorData::CreatePowerSequenceCmd(
    BOOL    isPowerUp,
    VOID*   pCmdBuffer)
{
    CamxResult    result            = CamxResultSuccess;
    UINT          powerSequenceSize = 0;
    PowerSetting* pSettings         = NULL;

    powerSequenceSize = (TRUE == isPowerUp) ?
                        m_pSensorData->slaveInfo.powerUpSequence.powerSettingCount :
                        m_pSensorData->slaveInfo.powerDownSequence.powerSettingCount;

    pSettings         = (TRUE == isPowerUp) ?
                        m_pSensorData->slaveInfo.powerUpSequence.powerSetting :
                        m_pSensorData->slaveInfo.powerDownSequence.powerSetting;

    result = ImageSensorUtils::CreatePowerSequenceCmd(pCmdBuffer, isPowerUp, powerSequenceSize, pSettings);
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ImageSensorData::GetI2CCmdMaxSize
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
UINT ImageSensorData::GetI2CCmdMaxSize(
    I2CRegSettingType settingType,
    VOID*             pRegSettings)
{
    UINT                driverSize      = 0;
    RegSettingsInfo*    pI2CSettings    = NULL;

    switch (settingType)
    {
        // NOWHINE NC011: Asking for exception, sensor commands are well known as - init, res, start, stop etc
        case I2CRegSettingType::Init:
            driverSize = m_pSensorData->initSettings[m_initSettingIndex].initSetting.regSettingCount;
            break;

        case I2CRegSettingType::Res:
            for (UINT resolutionIndex = 0;
                 resolutionIndex < GetResolutionInfo()->resolutionDataCount;
                 resolutionIndex++)
            {
                ResolutionData* pResData = &GetResolutionInfo()->resolutionData[resolutionIndex];
                driverSize               = Utils::MaxUINT32(driverSize, pResData->resSettings.regSettingCount);
            }
            break;

        case I2CRegSettingType::Start:
            driverSize = m_pSensorData->streamOnSettings.regSettingCount;
            break;

        case I2CRegSettingType::Stop:
            driverSize = m_pSensorData->streamOffSettings.regSettingCount;
            break;

        case I2CRegSettingType::Master:
            driverSize = m_pSensorData->masterSettings.regSettingCount;
            break;

        case I2CRegSettingType::Slave:
            driverSize = m_pSensorData->slaveSettings.regSettingCount;
            break;

        case I2CRegSettingType::IHDR_ON:
            driverSize = m_pSensorData->iHDRSettings.startSetting.regSettingCount;
            break;

        case I2CRegSettingType::IHDR_OFF:
            driverSize = m_pSensorData->iHDRSettings.stopSetting.regSettingCount;
            break;

        case I2CRegSettingType::AEC:
        case I2CRegSettingType::PDAF:
        case I2CRegSettingType::AWB:
        case I2CRegSettingType::Read:
            CAMX_ASSERT(NULL != pRegSettings);
            if (NULL != pRegSettings)
            {
                pI2CSettings    = static_cast<RegSettingsInfo*>(pRegSettings);
                driverSize      = sizeof(pI2CSettings->regSetting) / sizeof(pI2CSettings->regSetting[0]);
            }
            break;

        default:
            CAMX_ASSERT_ALWAYS_MESSAGE("Unknown settingsType 0x%x", settingType);
            break;
    }

    UINT baseSize  = sizeof(CSLSensorI2CRandomWriteCmd) - sizeof(CSLSensorI2CRegValPair);
    UINT totalSize = (sizeof(CSLSensorI2CRegValPair) + baseSize + sizeof(CSLSensorWaitCmd)) * driverSize;
    return totalSize;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ImageSensorData::GetI2CCmdSize
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
UINT ImageSensorData::GetI2CCmdSize(
    I2CRegSettingType   settingType,
    const VOID*         pRegSettings,
    UINT                resolutionIndex)
{
    UINT                           totalSize           = 0;
    const RegSettingsInfo*         pI2CSettings        = NULL;
    const SettingsInfo*            pRegisterSettings   = NULL;
    const ResolutionInformation*   pResolutionInfo     = NULL;

    switch (settingType)
    {
        // NOWHINE NC011: Asking for exception, sensor commands are well known as - init, res, start, stop etc
        case I2CRegSettingType::Init:
            pRegisterSettings = &m_pSensorData->initSettings[m_initSettingIndex].initSetting;
            break;

        case I2CRegSettingType::Res:
            pResolutionInfo = GetResolutionInfo();
            if ((NULL != pResolutionInfo) && (resolutionIndex < pResolutionInfo->resolutionDataCount))
            {
                pRegisterSettings = &(pResolutionInfo->resolutionData[resolutionIndex].resSettings);
            }
            else
            {
                CAMX_LOG_ERROR(CamxLogGroupSensor, "Illegal access : pResolutionInfo=0x%x,"
                    "resolutionIndex=%d", pResolutionInfo, resolutionIndex);
            }
            break;

        case I2CRegSettingType::Start:
            pRegisterSettings = &m_pSensorData->streamOnSettings;
            break;

        case I2CRegSettingType::Stop:
            pRegisterSettings = &m_pSensorData->streamOffSettings;
            break;

        case I2CRegSettingType::Master:
            pRegisterSettings = &m_pSensorData->masterSettings;
            break;

        case I2CRegSettingType::Slave:
            pRegisterSettings = &m_pSensorData->slaveSettings;
            break;

        case I2CRegSettingType::IHDR_ON:
            pRegisterSettings = &m_pSensorData->iHDRSettings.startSetting;
            break;

        case I2CRegSettingType::IHDR_OFF:
            pRegisterSettings = &m_pSensorData->iHDRSettings.stopSetting;
            break;

        case I2CRegSettingType::AEC:
        case I2CRegSettingType::PDAF:
        case I2CRegSettingType::AWB:
        case I2CRegSettingType::Read:
            CAMX_ASSERT(NULL != pRegSettings);
            if (NULL != pRegSettings)
            {
                pI2CSettings = static_cast<const RegSettingsInfo*>(pRegSettings);
            }
            break;

        case I2CRegSettingType::SPC:
        case I2CRegSettingType::AWBOTP:
        case I2CRegSettingType::LSC:
            CAMX_ASSERT(NULL != pRegSettings);
            if (NULL != pRegSettings)
            {
                pRegisterSettings = static_cast<const SettingsInfo*>(pRegSettings);
            }
            break;

        default:
            CAMX_ASSERT_ALWAYS_MESSAGE("Unknown settingsType 0x%x", settingType);
            break;
    }

    // Either pI2CSettings or pRegisterSettings must be a valid pointer, but not both
    CAMX_ASSERT((NULL == pI2CSettings) != (NULL == pRegisterSettings));

    if (NULL != pRegisterSettings)
    {
        totalSize = ImageSensorUtils::GetRegisterSettingsCmdSize(pRegisterSettings->regSettingCount,
                                                                 pRegisterSettings->regSetting);
    }
    else if (NULL != pI2CSettings)
    {
        UINT driverSize = pI2CSettings->regSettingCount;

        IOOperationType       previousOp       = IOOperationTypeMax;
        I2CRegAddressDataType currentDataType  = I2CRegAddressDataTypeMax;
        I2CRegAddressDataType currentAddrType  = I2CRegAddressDataTypeMax;

        for (UINT i = 0; i < driverSize; i++)
        {
            if (IOOperationTypeWrite == pI2CSettings->regSetting[i].operation)
            {
                if ((IOOperationTypeWrite != previousOp) ||
                    (currentDataType != pI2CSettings->regSetting[i].regDataType) ||
                    (currentAddrType != pI2CSettings->regSetting[i].regAddrType) ||
                    (pI2CSettings->regSetting[i].delayUs > 0))
                {
                    previousOp      = IOOperationTypeWrite;
                    currentDataType = pI2CSettings->regSetting[i].regDataType;
                    currentAddrType = pI2CSettings->regSetting[i].regAddrType;

                    // header
                    totalSize += sizeof(CSLSensorI2CRandomWriteCmd) - sizeof(CSLSensorI2CRegValPair);
                }
                totalSize += sizeof(CSLSensorI2CRegValPair);

                if (pI2CSettings->regSetting[i].delayUs > 0)
                {
                    totalSize += sizeof(CSLSensorWaitCmd);
                }
            }
            else if ((IOOperationTypeWriteBurst == pI2CSettings->regSetting[i].operation) ||
                     (IOOperationTypeWriteSequential == pI2CSettings->regSetting[i].operation))
            {
                previousOp = pI2CSettings->regSetting[i].operation;

                // header
                totalSize += sizeof(CSLSensorI2CBurstWriteCmd) - sizeof(CSLSensorI2CVal);
                // burst array
                totalSize += sizeof(CSLSensorI2CVal) * pI2CSettings->regSetting[i].registerDataSeq.count;

                if (pI2CSettings->regSetting[i].delayUs > 0)
                {
                    totalSize += sizeof(CSLSensorWaitCmd);
                }
            }
            else if (IOOperationTypeRead == pI2CSettings->regSetting[i].operation)
            {
                if ((IOOperationTypeRead != previousOp) ||
                    (currentDataType != pI2CSettings->regSetting[i].regDataType) ||
                    (currentAddrType != pI2CSettings->regSetting[i].regAddrType) ||
                    (pI2CSettings->regSetting[i].delayUs > 0))
                {
                    previousOp      = IOOperationTypeRead;
                    currentDataType = pI2CSettings->regSetting[i].regDataType;
                    currentAddrType = pI2CSettings->regSetting[i].regAddrType;

                    // header
                    totalSize += sizeof(CSLSensorI2CContinuousReadCmd);
                }
            }
        }
    }

    return totalSize;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ImageSensorData::CreateI2CCmd
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult ImageSensorData::CreateI2CCmd(
    I2CRegSettingType   settingType,
    VOID*               pCmdBuffer,
    const VOID*         pSuppliedSettings,
    UINT                resolutionIndex)
{
    CamxResult             result            = CamxResultSuccess;
    UINT                   offset            = 0;
    CSLSensorWaitCmd*      pWait             = NULL;
    const RegSettingsInfo* pI2CSettings      = NULL;
    const SettingsInfo*    pRegisterSettings = NULL;

    switch (settingType)
    {
        // NOWHINE NC011: Asking for exception, sensor commands are well known as - init, res, start, stop etc
        case I2CRegSettingType::Init:
            pRegisterSettings = &m_pSensorData->initSettings[m_initSettingIndex].initSetting;
            break;

        case I2CRegSettingType::Res:
            pRegisterSettings = &GetResolutionInfo()->resolutionData[resolutionIndex].resSettings;
            break;

        case I2CRegSettingType::Start:
            pRegisterSettings = &m_pSensorData->streamOnSettings;
            break;

        case I2CRegSettingType::Stop:
            pRegisterSettings = &m_pSensorData->streamOffSettings;
            break;

        case I2CRegSettingType::Master:
            pRegisterSettings = &m_pSensorData->masterSettings;
            break;

        case I2CRegSettingType::Slave:
            pRegisterSettings = &m_pSensorData->slaveSettings;
            break;

        case I2CRegSettingType::IHDR_ON:
            pRegisterSettings = &m_pSensorData->iHDRSettings.startSetting;
            break;

        case I2CRegSettingType::IHDR_OFF:
            pRegisterSettings = &m_pSensorData->iHDRSettings.stopSetting;
            break;

        case I2CRegSettingType::AEC:
        case I2CRegSettingType::PDAF:
        case I2CRegSettingType::AWB:
        case I2CRegSettingType::Read:
            CAMX_ASSERT(NULL != pSuppliedSettings);
            if (NULL != pSuppliedSettings)
            {
                pI2CSettings = static_cast<const RegSettingsInfo*>(pSuppliedSettings);
            }
            break;

        case I2CRegSettingType::SPC:
        case I2CRegSettingType::AWBOTP:
        case I2CRegSettingType::LSC:
            CAMX_ASSERT(NULL != pSuppliedSettings);
            if (NULL != pSuppliedSettings)
            {
                pRegisterSettings = static_cast<const SettingsInfo*>(pSuppliedSettings);
            }
            break;

        default:
            CAMX_ASSERT_ALWAYS_MESSAGE("Unknown settingsType 0x%x", settingType);
            break;
    }

    // Either pI2CSettings or pRegisterSettings must be a valid pointer, but not both
    CAMX_ASSERT((NULL == pI2CSettings) != (NULL == pRegisterSettings));

    if (NULL != pRegisterSettings)
    {
        result = ImageSensorUtils::CreateRegisterSettingsCmd(pCmdBuffer,
                                                             pRegisterSettings->regSettingCount,
                                                             pRegisterSettings->regSetting,
                                                             m_pSensorData->slaveInfo.i2cFrequencyMode);
    }
    else if (NULL != pI2CSettings)
    {
        UINT                        driverSize       = pI2CSettings->regSettingCount;
        I2CRegAddressDataType       currentDataType  = I2CRegAddressDataTypeMax;
        I2CRegAddressDataType       currentAddrType  = I2CRegAddressDataTypeMax;
        IOOperationType             previousOp       = IOOperationTypeMax;

        CSLSensorI2CRandomWriteCmd*    pI2CCmd     = NULL;
        CSLSensorI2CRegValPair*        pRegValPair = NULL;
        CSLSensorI2CContinuousReadCmd* pI2CReadCmd = NULL;

        for (UINT i = 0; i < driverSize; i++)
        {
            if (IOOperationTypeWrite == pI2CSettings->regSetting[i].operation)
            {
                if ((IOOperationTypeWrite != previousOp) ||
                    (currentDataType != pI2CSettings->regSetting[i].regDataType) ||
                    (currentAddrType != pI2CSettings->regSetting[i].regAddrType))
                {
                    previousOp      = IOOperationTypeWrite;
                    currentDataType = pI2CSettings->regSetting[i].regDataType;
                    currentAddrType = pI2CSettings->regSetting[i].regAddrType;

                    pI2CCmd         = reinterpret_cast<CSLSensorI2CRandomWriteCmd*>
                                         (static_cast<BYTE*>(pCmdBuffer) + offset);
                    pRegValPair     = &pI2CCmd->regValPairs[0];

                    pI2CCmd->header.count    = 0;
                    pI2CCmd->header.opcode   = CSLSensorI2COpcodeRandomWrite;
                    pI2CCmd->header.cmdType  = CSLSensorCmdTypeI2CRandomRegWrite;
                    pI2CCmd->header.dataType = static_cast<UINT8>(currentDataType);
                    pI2CCmd->header.addrType = static_cast<UINT8>(currentAddrType);

                    // header
                    offset += sizeof(CSLSensorI2CRandomWriteCmd) - sizeof(CSLSensorI2CRegValPair);
                }
                if ((NULL != pRegValPair) && (NULL != pI2CCmd))
                {
                    pRegValPair[pI2CCmd->header.count].reg = pI2CSettings->regSetting[i].registerAddr;
                    pRegValPair[pI2CCmd->header.count].val = pI2CSettings->regSetting[i].registerData;
                    pI2CCmd->header.count++;
                }
                offset += sizeof(CSLSensorI2CRegValPair);

                if (pI2CSettings->regSetting[i].delayUs > 0)
                {
                    pWait       = reinterpret_cast<CSLSensorWaitCmd*>(static_cast<BYTE*>(pCmdBuffer) + offset);
                    offset     += sizeof(CSLSensorWaitCmd);
                    previousOp  = IOOperationTypeMax;
                    ImageSensorUtils::UpdateWaitCommand(pWait, pI2CSettings->regSetting[i].delayUs);
                }
            }
            else if ((IOOperationTypeWriteBurst == pI2CSettings->regSetting[i].operation) ||
                     (IOOperationTypeWriteSequential == pI2CSettings->regSetting[i].operation))
            {
                CSLSensorI2CBurstWriteCmd* pBurstCmd = reinterpret_cast<CSLSensorI2CBurstWriteCmd*>
                                                           (static_cast<BYTE*>(pCmdBuffer) + offset);

                previousOp = pI2CSettings->regSetting[i].operation;

                if (IOOperationTypeWriteBurst == pI2CSettings->regSetting[i].operation)
                {
                    pBurstCmd->header.opcode = CSLSensorI2COpcodeContinuousWriteBurst;
                }
                else
                {
                    pBurstCmd->header.opcode = CSLSensorI2COpcodeContinuousWriteSequence;
                }

                pBurstCmd->header.cmdType  = CSLSensorCmdTypeI2CContinuousRegWrite;
                pBurstCmd->header.count    = static_cast<UINT16>(pI2CSettings->regSetting[i].registerDataSeq.count);
                pBurstCmd->header.dataType = static_cast<UINT8>(pI2CSettings->regSetting[i].regDataType);
                pBurstCmd->header.addrType = static_cast<UINT8>(pI2CSettings->regSetting[i].regAddrType);
                pBurstCmd->reg             = pI2CSettings->regSetting[i].registerAddr;

                offset = sizeof(CSLSensorI2CBurstWriteCmd) - sizeof(CSLSensorI2CVal);

                CSLSensorI2CVal* pVal = &pBurstCmd->data[0];
                for (UINT j = 0; j < pI2CSettings->regSetting[i].registerDataSeq.count; j++)
                {
                    pVal[j].val = pI2CSettings->regSetting[i].registerDataSeq.data[j];
                    pVal[j].reserved = 0;
                }

                offset += sizeof(CSLSensorI2CVal) * pI2CSettings->regSetting[i].registerDataSeq.count;

                if (pI2CSettings->regSetting[i].delayUs > 0)
                {
                    pWait   = reinterpret_cast<CSLSensorWaitCmd*>(static_cast<BYTE*>(pCmdBuffer) + offset);
                    offset += sizeof(CSLSensorWaitCmd);
                    ImageSensorUtils::UpdateWaitCommand(pWait, pI2CSettings->regSetting[i].delayUs);
                }
            }
            else if (IOOperationTypeRead == pI2CSettings->regSetting[i].operation)
            {
                if ((IOOperationTypeRead != previousOp) ||
                    (currentDataType != pI2CSettings->regSetting[i].regDataType) ||
                    (currentAddrType != pI2CSettings->regSetting[i].regAddrType))
                {
                    previousOp      = IOOperationTypeRead;
                    currentDataType = pI2CSettings->regSetting[i].regDataType;
                    currentAddrType = pI2CSettings->regSetting[i].regAddrType;

                    pI2CReadCmd     = reinterpret_cast<CSLSensorI2CContinuousReadCmd*>
                                          (static_cast<BYTE*>(pCmdBuffer) + offset);

                    pI2CReadCmd->header.count     = static_cast<UINT16>(pI2CSettings->regSetting[i].registerData);
                    pI2CReadCmd->header.opcode    = CSLSensorI2COpcodeContinuousRead;
                    pI2CReadCmd->header.cmdType   = CSLSensorCmdTypeI2CContinuousRegRead;
                    pI2CReadCmd->header.dataType  = static_cast<UINT8>(currentDataType);
                    pI2CReadCmd->header.addrType  = static_cast<UINT8>(currentAddrType);
                    pI2CReadCmd->reg              = pI2CSettings->regSetting[i].registerAddr;

                    offset += sizeof(CSLSensorI2CContinuousReadCmd);

                }
            }
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ImageSensorData::CreateCSIPHYConfig
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult ImageSensorData::CreateCSIPHYConfig(
    CSLSensorCSIPHYInfo*    pCmdCSIPHYConfig,
    UINT16                  laneAssign,
    UINT8                   comboMode)
{
    CamxResult result  = CamxResultSuccess;
    UCHAR resolution = 0;

    pCmdCSIPHYConfig->laneAssign             = laneAssign;
    pCmdCSIPHYConfig->comboMode              = comboMode;

    pCmdCSIPHYConfig->laneCount              =
        static_cast<UINT8>(GetResolutionInfo()->resolutionData[resolution].laneCount);
    pCmdCSIPHYConfig->settleTimeMilliseconds =
        GetResolutionInfo()->resolutionData[resolution].settleTimeNs;
    pCmdCSIPHYConfig->CSIPHY3Phase           =
        static_cast<UINT8>(GetResolutionInfo()->resolutionData[resolution].is3Phase);

    /// @todo (CAMX-555) remove hard coded values after we finalize the XSD definitions
    pCmdCSIPHYConfig->dataRate               =
        GetResolutionInfo()->resolutionData[resolution].outputPixelClock;

    if (0 == pCmdCSIPHYConfig->comboMode)
    {
        pCmdCSIPHYConfig->laneMask = (1 << (pCmdCSIPHYConfig->laneCount + 1)) - 1;
    }
    else
    {
        pCmdCSIPHYConfig->laneMask = SensorComboModeLaneMask;
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ImageSensorData::TranslateColorFilterArrangement
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
SensorInfoColorFilterArrangementValues ImageSensorData::TranslateColorFilterArrangement(
    ColorFilterArrangement sensorColorFiltertype)
{
    SensorInfoColorFilterArrangementValues colorFilter =
        SensorInfoColorFilterArrangementValues::SensorInfoColorFilterArrangementEnd;

    switch (sensorColorFiltertype)
    {
        case ColorFilterArrangement::BAYER_BGGR:
            colorFilter = SensorInfoColorFilterArrangementValues::SensorInfoColorFilterArrangementBggr;
            break;
        case ColorFilterArrangement::BAYER_GBRG:
            colorFilter = SensorInfoColorFilterArrangementValues::SensorInfoColorFilterArrangementGbrg;
            break;
        case ColorFilterArrangement::BAYER_GRBG:
            colorFilter = SensorInfoColorFilterArrangementValues::SensorInfoColorFilterArrangementGrbg;
            break;
        case ColorFilterArrangement::BAYER_RGGB:
            colorFilter = SensorInfoColorFilterArrangementValues::SensorInfoColorFilterArrangementRggb;
            break;
        case ColorFilterArrangement::BAYER_Y:
            colorFilter = SensorInfoColorFilterArrangementValues::SensorInfoColorFilterArrangementY;
            break;
        default:
            CAMX_LOG_ERROR(CamxLogGroupSensor, "Invalid color filter type %d", sensorColorFiltertype);
            break;
    }

    return colorFilter;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ImageSensorData::IsHDRMode
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL ImageSensorData::IsHDRMode(
    UINT32 modeIndex)
{
    BOOL result = FALSE;
    const ResolutionInformation* pResolutionInfo = GetResolutionInfo();
    UINT32 capabilityCount = 0;

    for (capabilityCount = 0; capabilityCount < pResolutionInfo->resolutionData[modeIndex].capabilityCount; capabilityCount++)
    {
        if ((SensorCapability::ZZHDR == pResolutionInfo->resolutionData[modeIndex].capability[capabilityCount]) ||
            (SensorCapability::IHDR == pResolutionInfo->resolutionData[modeIndex].capability[capabilityCount]))
        {
            result = TRUE;
            break;
        }
    }
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ImageSensorData::Is3ExposureHDRMode
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL ImageSensorData::Is3ExposureHDRMode(
    UINT32 modeIndex)
{
    BOOL result = FALSE;
    const ResolutionInformation* pResolutionInfo = GetResolutionInfo();
    UINT32                       capabilityCount = 0;

    for (capabilityCount = 0; capabilityCount < pResolutionInfo->resolutionData[modeIndex].capabilityCount; capabilityCount++)
    {
        if (SensorCapability::IHDR == pResolutionInfo->resolutionData[modeIndex].capability[capabilityCount])
        {
            if (HDR3ExposureInformation::HDR3ExposureTypeUnknown != Get3ExposureHDRInfo(modeIndex))
            {
                result = TRUE;
                break;
            }
        }
    }
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ImageSensorData::IsQuadCFAMode
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL ImageSensorData::IsQuadCFAMode(
    UINT32 modeIndex,
    BOOL* pIsHWRemosaic)
{
    BOOL result = FALSE;
    const ResolutionInformation* pResolutionInfo = GetResolutionInfo();
    UINT32                       capabilityCount = 0;

    *pIsHWRemosaic = FALSE;

    for (capabilityCount = 0; capabilityCount < pResolutionInfo->resolutionData[modeIndex].capabilityCount; capabilityCount++)
    {
        if (SensorCapability::QUADCFA == pResolutionInfo->resolutionData[modeIndex].capability[capabilityCount])
        {
            if (RemosaicType::HWRemosaic == pResolutionInfo->resolutionData[modeIndex].RemosaicTypeInfo)
            {
                *pIsHWRemosaic = TRUE;
            }
            else
            {
                *pIsHWRemosaic = FALSE;
            }
            result = TRUE;
            break;
        }
    }
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ImageSensorData::GetExposureTime
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
UINT64 ImageSensorData::GetExposureTime(
    UINT64 currentExposureTime,
    VOID*  pAdditionalInfo)
{
    UINT64 reportExposureTime = 0;

    if ((NULL != m_sensorLibraryAPI.pGetExposureTime) &&
        (NULL != pAdditionalInfo))
    {
        reportExposureTime = m_sensorLibraryAPI.pGetExposureTime(currentExposureTime, static_cast<INT32 *>(pAdditionalInfo));
    }
    else
    {
        reportExposureTime = currentExposureTime;
    }

    return reportExposureTime;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ImageSensorData::GetStatsParseFuncPtr
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID* ImageSensorData::GetStatsParseFuncPtr() const
{
    BOOL result = TRUE;
    VOID* pStatsParseFuncPtr = reinterpret_cast<VOID *>(m_sensorLibraryAPI.pStatsParse);
    CAMX_LOG_VERBOSE(CamxLogGroupSensor, "pStatsParseFuncPtr=%p", pStatsParseFuncPtr);
    return pStatsParseFuncPtr;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ImageSensorData::GetPixelSize
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
FLOAT ImageSensorData::GetPixelSize()
{
    return (static_cast<FLOAT>(m_pSensorData->sensorProperty.pixelSize));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ImageSensorData::ReadDeviceData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult ImageSensorData::ReadDeviceData(
    INT32         deviceID,
    UINT16        numOfBytes,
    UINT8*        pReadData,
    UINT32        opcode,
    INT32         hCSLDeviceHandle,
    CSLDeviceType device,
    INT32         hCSLSession,
    Packet*       pSensorReadPacket)
{
    CamxResult result = CamxResultSuccess;
    result = ImageSensorUtils::ReadDeviceData(deviceID,
                                            numOfBytes,
                                            pReadData,
                                            opcode,
                                            hCSLDeviceHandle,
                                            device,
                                            hCSLSession,
                                            pSensorReadPacket);
    return result;
}

CAMX_NAMESPACE_END
