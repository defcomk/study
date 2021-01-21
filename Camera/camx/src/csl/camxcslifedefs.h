////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017-2019 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxcslifedefs.h
/// @brief IFE Hardware Interface Definition
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef CAMXCSLIFEDEFS_H
#define CAMXCSLIFEDEFS_H
#include "camxutils.h"

CAMX_NAMESPACE_BEGIN

static const UINT32 IFEOutputFull          = 0x3000;
static const UINT32 IFEOutputDS4           = 0x3001;
static const UINT32 IFEOutputDS16          = 0x3002;
static const UINT32 IFEOutputRaw           = 0x3003;
static const UINT32 IFEOutputFD            = 0x3004;
static const UINT32 IFEOutputPDAF          = 0x3005;
static const UINT32 IFEOutputRDI0          = 0x3006;
static const UINT32 IFEOutputRDI1          = 0x3007;
static const UINT32 IFEOutputRDI2          = 0x3008;
static const UINT32 IFEOutputRDI3          = 0x3009;
static const UINT32 IFEOutputStatsHDRBE    = 0x300A;
static const UINT32 IFEOutputStatsHDRBHIST = 0x300B;
static const UINT32 IFEOutputStatsTLBG     = 0x300C;
static const UINT32 IFEOutputStatsBF       = 0x300D;
static const UINT32 IFEOutputStatsAWBBG    = 0x300E;
static const UINT32 IFEOutputStatsBHIST    = 0x300F;
static const UINT32 IFEOutputStatsRS       = 0x3010;
static const UINT32 IFEOutputStatsCS       = 0x3011;
static const UINT32 IFEOutputStatsIHIST    = 0x3012;
static const UINT32 IFEOutputDisplayFull   = 0x3013;
static const UINT32 IFEOutputDisplayDS4    = 0x3014;
static const UINT32 IFEOutputDisplayDS16   = 0x3015;
static const UINT32 IFEOutputDualPD        = 0x3016;

// IFE input resource type
static const UINT32 IFEInputTestGen        = 0x4000;
static const UINT32 IFEInputPHY0           = 0x4001;
static const UINT32 IFEInputPHY1           = 0x4002;
static const UINT32 IFEInputPHY2           = 0x4003;
static const UINT32 IFEInputPHY3           = 0x4004;

// IFE fetch engine resource type
static const UINT32 IFEInputBusRead        = 0x4005;

// IFE input resource Lane Type
static const UINT32 IFELaneTypeDPHY        = 0;
static const UINT32 IFELaneTypeCPHY        = 1;

// IFE Generic Blob types
static const UINT32 IFEGenericBlobTypeHFRConfig             = 0;
static const UINT32 IFEGenericBlobTypeResourceClockConfig   = 1;
static const UINT32 IFEGenericBlobTypeResourceBWConfig      = 2;
static const UINT32 IFEGenericBlobTypeUBWCConfig            = 3;
static const UINT32 IFEGenericBlobTypeBusReadConfig         = 5;
static const UINT32 IFEGenericBlobTypeResourceBWConfigAB    = 6;

CAMX_NAMESPACE_END
#endif // CAMXCSLIFEDEFS_H
