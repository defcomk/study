/**
 * @file common_types.h
 *
 * Copyright (c) 2010-2019 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#ifndef _CAM_TYPES_H
#define _CAM_TYPES_H

#define MMU_HANDLE_INVALID -1

/// @brief Handle to a device within the CAMSS that is exposed to the UMD
typedef int32 CamDeviceHandle;

/// @brief Type to hold addresses mapped to the camera HW.
typedef uint32 CSLDeviceAddress;
typedef int32 CSLMemHandle;
typedef int32 CSLFence;


/// @brief CSL buffer descriptor.
typedef struct
{
    CSLMemHandle        hHandle;        ///< Handle to the buffer managed by CSL.
    void*               pVirtualAddr;   ///< Virtual address mapped in the UMD for the buffer. If the buffer is not mapped,
                                        ///  pVa will be set to NULL.
    CSLDeviceAddress    deviceAddr;     ///< The mapped HW device address for the buffer. If the buffer is not mapped to HW,
                                        ///  or the KMD does not expose the device address to the UMD, it will be set to 0.
    int                 fd;             ///< The file descriptor of the buffer. If no valid file descriptor is available for
                                        ///  the buffer the fd will be set to 0.
    size_t              offset;         ///< The offset of the buffer in the file. This offset is only valid relative to fd.
    size_t              size;           ///< The size in bytes of the buffer.
    uint32              flags;          ///< Any usage flags set for this buffer.
    /// @todo (CAMX-428) Remove the bufferImported member in CSLBufferInfo.
    uint32              bufferImported; ///< Set if this buffer is imported and not allocated by CSL.
    bool                mappedToKMD;         //Indicates if the buffer has been mapped into kernel
} CSLBufferInfo;

static void* VoidPtrInc(
    void*    pVoid,
    size_t   numBytes)
{
    return (static_cast<char*>(pVoid) + numBytes);
}

static uint64 VoidPtrToUINT64(
    void* pVoid)
{
    return static_cast<uint64>(reinterpret_cast<size_t>(pVoid));
}

#endif
