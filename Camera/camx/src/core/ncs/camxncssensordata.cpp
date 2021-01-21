////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017-2018 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxncssensordata.cpp
/// @brief CamX NCS Sensor Data accessor implementation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "camxincs.h"
#include "camxncssensordata.h"

CAMX_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// NCSSensorData::GetFirst
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID* NCSSensorData::GetFirst()
{
    UINT8* pTemp = NULL;
    pTemp        = m_pBaseAddress;
    return ( pTemp + (m_startIndexOrig * m_bufferStride));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// NCSSensorData::GetNumSamples
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
INT NCSSensorData::GetNumSamples()
{
    return m_numSamples;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// NCSSensorData::GetNext
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID* NCSSensorData::GetNext()
{
    VOID*  pNCSData = NULL;
    UINT8* pTemp    = NULL;

    pTemp = m_pBaseAddress;

    if ((m_currentIndex + 1) >= m_numSamples)
    {
        pNCSData = NULL;
    }
    else
    {
        m_currentIndex = m_currentIndex + 1;
        pNCSData       = pTemp + ((m_totalNumSamples + m_startIndexOrig - m_currentIndex ) % m_totalNumSamples)*m_bufferStride;
        CAMX_LOG_VERBOSE(CamxLogGroupNCS, "GetNext pNCSData %p", pNCSData);
    }

    return pNCSData;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// NCSSensorData::GetCurrent
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID* NCSSensorData::GetCurrent()
{
    VOID*  pNCSData = NULL;
    UINT8* pTemp    = NULL;

    pTemp = m_pBaseAddress;

    pNCSData = pTemp + ((m_totalNumSamples + m_startIndexOrig + m_currentIndex) % m_totalNumSamples)*m_bufferStride;
    CAMX_LOG_VERBOSE(CamxLogGroupNCS, "GetNext pNCSData %p", pNCSData);

    return pNCSData;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// NCSSensorData::SetIndex
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult NCSSensorData::SetIndex(
    INT index)
{
    CamxResult result = CamxResultSuccess;

    if ((index >= m_numSamples) || (index < 0))
    {
        result = CamxResultEInvalidArg;
    }
    else
    {
        m_currentIndex = index;
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// NCSSensorData::SetDataLimits
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult NCSSensorData::SetDataLimits(
    INT start,
    INT end,
    VOID* pBaseAddress,
    INT bufferQLength)
{
    m_pBaseAddress    = reinterpret_cast<UINT8*>(pBaseAddress);
    m_startIndexOrig  = start;
    m_endIndexOrig    = end;
    m_currentIndex    = 0;
    m_totalNumSamples = bufferQLength;
    if (m_startIndexOrig <= m_endIndexOrig)
    {
        m_numSamples = (m_endIndexOrig - m_startIndexOrig + 1);
    }
    else
    {
        m_numSamples = (m_endIndexOrig + 1) + (m_totalNumSamples - m_startIndexOrig);
    }
    CAMX_LOG_VERBOSE(CamxLogGroupNCS, "start %d, end %d, samples %d, total samples %d",
        start, end, m_numSamples, bufferQLength);
    return CamxResultSuccess;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// NCSSensorData::SetHaveBufLocked
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID NCSSensorData::SetHaveBufLocked()
{
    m_haveLockedBuffer = TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// NCSSensorData::SetBufferStride
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID NCSSensorData::SetBufferStride(
    UINT bufferStride)
{
    m_bufferStride = bufferStride;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// NCSSensorData::ClearHaveBufLocked
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID NCSSensorData::ClearHaveBufLocked()
{
    m_haveLockedBuffer = FALSE;
    return;
}

CAMX_NAMESPACE_END
