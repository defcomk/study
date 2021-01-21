////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017-2019 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxdebugprint.cpp
/// @brief Debug Print related defines
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "camxdebugprint.h"
#include "camxosutils.h"
#include "camxtrace.h"

CAMX_NAMESPACE_BEGIN

/// @todo (CAMX-944) Set log mask defaults here and in camxsettings.xml to 0x0
struct DebugLogInfo g_logInfo =
{
    {
        0xFFFFFFFF,     ////< CamxLogDebug (Unused)
        0xFFFFFFFF,     ////< CamxLogError (Unused)
        0xFFFFFFFF,     ////< CamxLogWarning
        0xFFFFFFFF,     ////< CamxLogConfig
        0xFFFFFFFF,     ////< CamxLogDump
        0x0,            ////< CamxLogInfo
        0x0,            ////< CamxLogVerbose
        0x0,            ////< CamxLogPerfInfo
        0x0,            ////< CamxLogPerfWarning
        0x0,            ////< CamxLogDRQ
        0x0,            ////< CamxLogMeta
        0x0,            ////< CamxLogEntryExit
        0x0,            ////< CamxLogReqMap
    },
    NULL,               ////< pDebugLogFile
    TRUE,               ////< systemLogEnable
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Log::UpdateLogInfo
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID Log::UpdateLogInfo(
    DebugLogInfo* pNewLogInfo)
{
    // Update the debug log file
    if (NULL != g_logInfo.pDebugLogFile)
    {
        OsUtils::FClose(g_logInfo.pDebugLogFile);
        g_logInfo.pDebugLogFile = NULL;
    }

    g_logInfo.groupsEnable[CamxLogConfig]    = pNewLogInfo->groupsEnable[CamxLogConfig];
    g_logInfo.groupsEnable[CamxLogDump]      = pNewLogInfo->groupsEnable[CamxLogDump];
    g_logInfo.groupsEnable[CamxLogWarning]   = pNewLogInfo->groupsEnable[CamxLogWarning];
    g_logInfo.groupsEnable[CamxLogEntryExit] = pNewLogInfo->groupsEnable[CamxLogEntryExit];
    g_logInfo.groupsEnable[CamxLogInfo]      = pNewLogInfo->groupsEnable[CamxLogInfo];
    g_logInfo.groupsEnable[CamxLogPerfInfo]  = pNewLogInfo->groupsEnable[CamxLogPerfInfo];
    g_logInfo.groupsEnable[CamxLogVerbose]   = pNewLogInfo->groupsEnable[CamxLogVerbose];
    g_logInfo.groupsEnable[CamxLogDRQ]       = pNewLogInfo->groupsEnable[CamxLogDRQ];
    g_logInfo.groupsEnable[CamxLogMeta]      = pNewLogInfo->groupsEnable[CamxLogMeta];
    g_logInfo.groupsEnable[CamxLogReqMap]    = pNewLogInfo->groupsEnable[CamxLogReqMap];
    g_logInfo.pDebugLogFile                  = pNewLogInfo->pDebugLogFile;
    g_logInfo.systemLogEnable                = pNewLogInfo->systemLogEnable;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Log::GetFileName
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const CHAR* Log::GetFileName(
    const CHAR* pFilePath)
{
    return OsUtils::GetFileName(pFilePath);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Log::LogSystem
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID Log::LogSystem(
    CamxLog     level,
    const CHAR* pFormat,
    ...)
{
    if ((TRUE == g_logInfo.systemLogEnable) || (NULL != g_logInfo.pDebugLogFile))
    {
        CHAR logText[MaxLogLength];

        // Generate output string
        va_list args;
        va_start(args, pFormat);
        OsUtils::VSNPrintF(logText, sizeof(logText), pFormat, args);
        va_end(args);

        if (TRUE == g_logInfo.systemLogEnable)
        {
            OsUtils::LogSystem(level, logText);
        }

        if (NULL != g_logInfo.pDebugLogFile)
        {
            CamxDateTime systemDateTime;
            OsUtils::GetDateTime(&systemDateTime);
            OsUtils::FPrintF(g_logInfo.pDebugLogFile, "%02d-%02d %02d:%02d:%02d:%03d %s\n", systemDateTime.month,
                systemDateTime.dayOfMonth, systemDateTime.hours, systemDateTime.minutes, systemDateTime.seconds,
                systemDateTime.microseconds / 1000, logText);
        }
    }

    /// @todo (CAMX-6) Debug print to xlog
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// LogAuto::LogAuto
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
LogAuto::LogAuto(
    CamxLogGroup    group,
    const CHAR*     pFile,
    const CHAR*     pLine,
    const CHAR*     pName,
    UINT64          id,
    BOOL            isScope)
    : m_group(group)
    , m_pName(pName)
    , m_id(id)
{
    if (TRUE == isScope)
    {
        if ('C' == m_pName[0])
        {
            // Skip CamX::SCOPEEvent in string
            m_pName += (sizeof(CHAR) * 16);
        }
        else
        {
            // Skip SCOPEEvent in string
            m_pName += (sizeof(CHAR) * 10);
        }
    }

    m_pFile = OsUtils::GetFileName(pFile);

    // Logging
    if (g_logInfo.groupsEnable[CamxLogEntryExit] & m_group)
    {
        Log::LogSystem(CamxLogEntryExit,
                       "[ENTRY]%s %s:%s Entering %s()",
                       CamX::Log::GroupToString(m_group),
                       m_pFile,
                       pLine,
                       m_pName);
    }

    if (0 == m_id)
    {
        CAMX_TRACE_SYNC_BEGIN(m_group, m_pName);
    }
    else
    {
        CAMX_TRACE_SYNC_BEGIN_F(m_group, "%s : %llu", m_pName, m_id);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// LogAuto::~LogAuto()
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
LogAuto::~LogAuto()
{
    if (g_logInfo.groupsEnable[CamxLogEntryExit] & m_group)
    {
        Log::LogSystem(CamxLogEntryExit,
                       "[ EXIT]%s %s Exiting %s()",
                       CamX::Log::GroupToString(m_group),
                       m_pFile,
                       m_pName);
    }

    CAMX_TRACE_SYNC_END(m_group);
}

CAMX_NAMESPACE_END
