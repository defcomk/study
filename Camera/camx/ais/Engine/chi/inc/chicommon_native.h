/*
 * Copyright (c) 2016-2018 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 * Not a Contribution.
 * Apache license notifications and license are retained
 * for attribution purposes only.
 */

/*
 * Copyright (C) 2013 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __CHI_COMMON__
#define __CHI_COMMON__

#include "camera3stream.h"
#include <chi.h>
#include <map>

#define EXPECT_CHI_API_MAJOR_VERSION   1
#define EXPECT_CHI_API_MINOR_VERSION   0

static const GrallocUsage GrallocUsageTP10 = 0x08000000;   ///< UBWC TP10 private usage flag.
static const GrallocUsage GrallocUsageP010 = 0x40000000;   ///< UBWC P010 private usage flag.
static const int METADATA_ENTRIES = 200;
const int MaxPipelinesPerSessionlocal = 3;

/// @brief Create session data
struct SessionCreateData
{
    CHIPIPELINEINFO pPipelineInfos[MaxPipelinesPerSessionlocal];   ///< Chi Pipeline infos
    uint32_t numPipelines;                             ///< Number of pipelines in this session
    CHICALLBACKS* pCallbacks;                               ///< Callbacks
    void* pPrivateCallbackData;                     ///< Private callbacks
};

/// @brief Pipeline create data
struct PipelineCreateData
{
    CHIPIPELINECREATEDESCRIPTOR* pPipelineCreateDescriptor;         ///< Pipeline create descriptor
    uint32_t numOutputs;                        ///< Number of output buffers of this pipeline
    CHIPORTBUFFERDESCRIPTOR* pOutputDescriptors;                ///< Output buffer descriptors
    uint32_t numInputs;                         ///< Number of inputs
    CHIPORTBUFFERDESCRIPTOR* pInputDescriptors;                 ///< Input buffer descriptors
    CHIPIPELINEINPUTOPTIONS* pInputBufferRequirements;          ///< Input buffer requirements for this pipeline

};

/// @brief PipelineType
enum class PipelineType
{
    RealtimeIFE = 0,
    RealTimeIPE,
    RealtimeIFETestGen,
    RealtimeIFEStatsTestGen,
    RealtimeIFEStats,
    RealtimeIFERDI0,
    RealtimeIFERDI1,
    RealtimeIFERDI2,
    RealtimeIFERDI3,
    RealtimeIFERawCamif,
    RealtimeIFERawLsc,
    OfflineBPS,
    OfflineIPEDisp,
    OfflineIPEVideo,
    OfflineIPEDispVideo,
    OfflineBPSIPE,
    OfflineBPSStats,
    OfflineBPSStatsWithIPE,
    OfflineBPSBGStatsWithIPE,
    OfflineBPSHistStatsWithIPE,
    RealtimePrevRDI,
    PrevRDIwithTestGen,
    RealtimePrevRDIWithStats,
    RealtimePrevWithCallbackAndRDIWithStats,
    RealtimePrevWithCallbackWithStats,
    RealtimePrevAndVideoWithStats,
    RealtimePrevWithMemcpy,
    RealtimePrevFromMemcpy,
    OfflineZSlSnap,
    OfflineZSlSnapWithDSports,
    OfflineZSlSnapAndThumbnailWithDSports,
    VendorTagWrite,
    OfflineJpegSnapshot,
    OfflineBPSGPUpipeline
};


#endif  // __CHI_COMMON__
