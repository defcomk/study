////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016-2019 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxcslhwtypes.cpp
/// @brief CamxCSL hw types UMD/KMD comparison header file Version 1.0
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#if ANDROID

#include <media/cam_defs.h>

#include "camxcslhwinternal.h"
#include "camxincs.h"
#include "camxpacketdefs.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// CSL packet definitions comparison with KMD cam packet
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CAMX_STATIC_ASSERT(sizeof(CSLPacketHeader)                  == sizeof(struct cam_packet_header));
CAMX_STATIC_ASSERT(sizeof(CSLPacket)                        == sizeof(struct cam_packet));
CAMX_STATIC_ASSERT(offsetof(CSLPacketHeader, opcode)        == offsetof(struct cam_packet_header, op_code));
CAMX_STATIC_ASSERT(offsetof(CSLPacketHeader, size)          == offsetof(struct cam_packet_header, size));
CAMX_STATIC_ASSERT(offsetof(CSLPacketHeader, requestId)     == offsetof(struct cam_packet_header, request_id));
CAMX_STATIC_ASSERT(offsetof(CSLPacketHeader, flags)         == offsetof(struct cam_packet_header, flags));
CAMX_STATIC_ASSERT(offsetof(CSLPacketHeader, padding)       == offsetof(struct cam_packet_header, padding));
CAMX_STATIC_ASSERT(offsetof(CSLPacket, header)              == offsetof(struct cam_packet, header));
CAMX_STATIC_ASSERT(offsetof(CSLPacket, cmdBuffersOffset)    == offsetof(struct cam_packet, cmd_buf_offset));
CAMX_STATIC_ASSERT(offsetof(CSLPacket, numCmdBuffers)       == offsetof(struct cam_packet, num_cmd_buf));
CAMX_STATIC_ASSERT(offsetof(CSLPacket, ioConfigsOffset)     == offsetof(struct cam_packet, io_configs_offset));
CAMX_STATIC_ASSERT(offsetof(CSLPacket, numBufferIOConfigs)  == offsetof(struct cam_packet, num_io_configs));
CAMX_STATIC_ASSERT(offsetof(CSLPacket, patchsetOffset)      == offsetof(struct cam_packet, patch_offset));
CAMX_STATIC_ASSERT(offsetof(CSLPacket, numPatches)          == offsetof(struct cam_packet, num_patches));
CAMX_STATIC_ASSERT(offsetof(CSLPacket, kmdCmdBufferIndex)   == offsetof(struct cam_packet, kmd_cmd_buf_index));
CAMX_STATIC_ASSERT(offsetof(CSLPacket, kmdCmdBufferOffset)  == offsetof(struct cam_packet, kmd_cmd_buf_offset));
CAMX_STATIC_ASSERT(offsetof(CSLPacket, data)                == offsetof(struct cam_packet, payload));

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CSLAddrPatch definitions comparison with KMD definitions
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CAMX_STATIC_ASSERT(sizeof(CSLAddrPatch)                 == sizeof(struct cam_patch_desc));
CAMX_STATIC_ASSERT(offsetof(CSLAddrPatch, hDstBuffer)   == offsetof(struct cam_patch_desc, dst_buf_hdl));
CAMX_STATIC_ASSERT(offsetof(CSLAddrPatch, dstOffset)    == offsetof(struct cam_patch_desc, dst_offset));
CAMX_STATIC_ASSERT(offsetof(CSLAddrPatch, hSrcBuffer)   == offsetof(struct cam_patch_desc, src_buf_hdl));
CAMX_STATIC_ASSERT(offsetof(CSLAddrPatch, srcOffset)    == offsetof(struct cam_patch_desc, src_offset));

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CSLCmdMemDesc definitions comparison with KMD definitions
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CAMX_STATIC_ASSERT(sizeof(CSLCmdMemDesc)                == sizeof(struct cam_cmd_buf_desc));
CAMX_STATIC_ASSERT(offsetof(CSLCmdMemDesc, hMem)        == offsetof(struct cam_cmd_buf_desc, mem_handle));
CAMX_STATIC_ASSERT(offsetof(CSLCmdMemDesc, offset)      == offsetof(struct cam_cmd_buf_desc, offset));
CAMX_STATIC_ASSERT(offsetof(CSLCmdMemDesc, size)        == offsetof(struct cam_cmd_buf_desc, size));
CAMX_STATIC_ASSERT(offsetof(CSLCmdMemDesc, length)      == offsetof(struct cam_cmd_buf_desc, length));
CAMX_STATIC_ASSERT(offsetof(CSLCmdMemDesc, type)        == offsetof(struct cam_cmd_buf_desc, type));
CAMX_STATIC_ASSERT(offsetof(CSLCmdMemDesc, metadata)    == offsetof(struct cam_cmd_buf_desc, meta_data));

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CSLBufferIOConfig definitions comparison with KMD definitions
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CAMX_STATIC_ASSERT(CSLMaxNumPlanes == CAM_PACKET_MAX_PLANES);
CAMX_STATIC_ASSERT(sizeof(CSLBufferIOConfig)                        == sizeof(struct cam_buf_io_cfg));
CAMX_STATIC_ASSERT(offsetof(CSLBufferIOConfig, hMems)               == offsetof(struct cam_buf_io_cfg, mem_handle));
CAMX_STATIC_ASSERT(offsetof(CSLBufferIOConfig, offsets)             == offsetof(struct cam_buf_io_cfg, offsets));
CAMX_STATIC_ASSERT(offsetof(CSLBufferIOConfig, planes)              == offsetof(struct cam_buf_io_cfg, planes));
CAMX_STATIC_ASSERT(offsetof(CSLBufferIOConfig, format)              == offsetof(struct cam_buf_io_cfg, format));
CAMX_STATIC_ASSERT(offsetof(CSLBufferIOConfig, colorSpace)          == offsetof(struct cam_buf_io_cfg, color_space));
CAMX_STATIC_ASSERT(offsetof(CSLBufferIOConfig, colorFilterPattern)  == offsetof(struct cam_buf_io_cfg, color_pattern));
CAMX_STATIC_ASSERT(offsetof(CSLBufferIOConfig, bitsPerPixel)        == offsetof(struct cam_buf_io_cfg, bpp));
CAMX_STATIC_ASSERT(offsetof(CSLBufferIOConfig, rotation)            == offsetof(struct cam_buf_io_cfg, rotation));
CAMX_STATIC_ASSERT(offsetof(CSLBufferIOConfig, portResourceId)      == offsetof(struct cam_buf_io_cfg, resource_type));
CAMX_STATIC_ASSERT(offsetof(CSLBufferIOConfig, hSync)               == offsetof(struct cam_buf_io_cfg, fence));
CAMX_STATIC_ASSERT(offsetof(CSLBufferIOConfig, hEarlySync)          == offsetof(struct cam_buf_io_cfg, early_fence));
CAMX_STATIC_ASSERT(offsetof(CSLBufferIOConfig, auxCmdBuffer)        == offsetof(struct cam_buf_io_cfg, aux_cmd_buf));
CAMX_STATIC_ASSERT(offsetof(CSLBufferIOConfig, direction)           == offsetof(struct cam_buf_io_cfg, direction));
CAMX_STATIC_ASSERT(offsetof(CSLBufferIOConfig, batchSize)           == offsetof(struct cam_buf_io_cfg, batch_size));
CAMX_STATIC_ASSERT(offsetof(CSLBufferIOConfig, subsamplePattern)    == offsetof(struct cam_buf_io_cfg, subsample_pattern));
CAMX_STATIC_ASSERT(offsetof(CSLBufferIOConfig, subsamplePeriod)     == offsetof(struct cam_buf_io_cfg, subsample_period));
CAMX_STATIC_ASSERT(offsetof(CSLBufferIOConfig, framedropPattern)    == offsetof(struct cam_buf_io_cfg, framedrop_pattern));
CAMX_STATIC_ASSERT(offsetof(CSLBufferIOConfig, framedropPeriod)     == offsetof(struct cam_buf_io_cfg, framedrop_period));
CAMX_STATIC_ASSERT(offsetof(CSLBufferIOConfig, flags)               == offsetof(struct cam_buf_io_cfg, flag));
CAMX_STATIC_ASSERT(offsetof(CSLBufferIOConfig, padding)             == offsetof(struct cam_buf_io_cfg, padding));

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CSLPlane definitions comparison with KMD definitions
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CAMX_STATIC_ASSERT(sizeof(CSLPlane)                    == sizeof(struct cam_plane_cfg));
CAMX_STATIC_ASSERT(offsetof(CSLPlane, width)           == offsetof(struct cam_plane_cfg, width));
CAMX_STATIC_ASSERT(offsetof(CSLPlane, height)          == offsetof(struct cam_plane_cfg, height));
CAMX_STATIC_ASSERT(offsetof(CSLPlane, planeStride)     == offsetof(struct cam_plane_cfg, plane_stride));
CAMX_STATIC_ASSERT(offsetof(CSLPlane, sliceHeight)     == offsetof(struct cam_plane_cfg, slice_height));
CAMX_STATIC_ASSERT(offsetof(CSLPlane, metadataStride)  == offsetof(struct cam_plane_cfg, meta_stride));
CAMX_STATIC_ASSERT(offsetof(CSLPlane, metadataSize)    == offsetof(struct cam_plane_cfg, meta_size));
CAMX_STATIC_ASSERT(offsetof(CSLPlane, metadataOffset)  == offsetof(struct cam_plane_cfg, meta_offset));
CAMX_STATIC_ASSERT(offsetof(CSLPlane, packerConfig)    == offsetof(struct cam_plane_cfg, packer_config));
CAMX_STATIC_ASSERT(offsetof(CSLPlane, modeConfig)      == offsetof(struct cam_plane_cfg, mode_config));
CAMX_STATIC_ASSERT(offsetof(CSLPlane, tileConfig)      == offsetof(struct cam_plane_cfg, tile_config));
CAMX_STATIC_ASSERT(offsetof(CSLPlane, hInit)           == offsetof(struct cam_plane_cfg, h_init));
CAMX_STATIC_ASSERT(offsetof(CSLPlane, vInit)           == offsetof(struct cam_plane_cfg, v_init));

#if CAM_UBWC_CFG_VERSION_1

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CSLPortUBWCConfig definitions comparison with KMD definitions
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CAMX_STATIC_ASSERT(sizeof(CSLPortUBWCConfig)                   == sizeof(struct cam_ubwc_plane_cfg_v1));
CAMX_STATIC_ASSERT(offsetof(CSLPortUBWCConfig, portResourceId) == offsetof(struct cam_ubwc_plane_cfg_v1, port_type));
CAMX_STATIC_ASSERT(offsetof(CSLPortUBWCConfig, metadataStride) == offsetof(struct cam_ubwc_plane_cfg_v1, meta_stride));
CAMX_STATIC_ASSERT(offsetof(CSLPortUBWCConfig, metadataSize)   == offsetof(struct cam_ubwc_plane_cfg_v1, meta_size));
CAMX_STATIC_ASSERT(offsetof(CSLPortUBWCConfig, metadataOffset) == offsetof(struct cam_ubwc_plane_cfg_v1, meta_offset));
CAMX_STATIC_ASSERT(offsetof(CSLPortUBWCConfig, packerConfig)   == offsetof(struct cam_ubwc_plane_cfg_v1, packer_config));
CAMX_STATIC_ASSERT(offsetof(CSLPortUBWCConfig, modeConfig)     == offsetof(struct cam_ubwc_plane_cfg_v1, mode_config_0));
CAMX_STATIC_ASSERT(offsetof(CSLPortUBWCConfig, modeConfig1)    == offsetof(struct cam_ubwc_plane_cfg_v1, mode_config_1));
CAMX_STATIC_ASSERT(offsetof(CSLPortUBWCConfig, tileConfig)     == offsetof(struct cam_ubwc_plane_cfg_v1, tile_config));
CAMX_STATIC_ASSERT(offsetof(CSLPortUBWCConfig, hInitialVal)    == offsetof(struct cam_ubwc_plane_cfg_v1, h_init));
CAMX_STATIC_ASSERT(offsetof(CSLPortUBWCConfig, vInitialVal)    == offsetof(struct cam_ubwc_plane_cfg_v1, v_init));

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CSLResourceUBWCConfig definitions comparison with KMD definitions
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CAMX_STATIC_ASSERT(sizeof(CSLResourceUBWCConfig)                   == sizeof(struct cam_ubwc_config));
CAMX_STATIC_ASSERT(offsetof(CSLResourceUBWCConfig, UBWCAPIVersion) == offsetof(struct cam_ubwc_config, api_version));
CAMX_STATIC_ASSERT(offsetof(CSLResourceUBWCConfig, numPorts)       == offsetof(struct cam_ubwc_config, num_ports));
CAMX_STATIC_ASSERT(offsetof(CSLResourceUBWCConfig, portUBWCConfig) == offsetof(struct cam_ubwc_config, ubwc_plane_cfg));

#endif // CAM_UBWC_CFG_VERSION_1

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CSLRotation definitions comparison with KMD definitions
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CAMX_STATIC_ASSERT(CSLRotationCW0Degrees    == CAM_ROTATE_CW_0_DEGREE);
CAMX_STATIC_ASSERT(CSLRotationCW90Degrees   == CAM_ROTATE_CW_90_DEGREE);
CAMX_STATIC_ASSERT(CSLRotationCW180Degrees  == CAM_RORATE_CW_180_DEGREE);
CAMX_STATIC_ASSERT(CSLRotationCW270Degrees  == CAM_ROTATE_CW_270_DEGREE);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CSLColorSpace definitions comparison with KMD definitions
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CAMX_STATIC_ASSERT(CSLColorSpaceUnknown     == CAM_COLOR_SPACE_BASE);
CAMX_STATIC_ASSERT(CSLColorSpaceBT601Full   == CAM_COLOR_SPACE_BT601_FULL);
CAMX_STATIC_ASSERT(CSLColorSpaceBT601625    == CAM_COLOR_SPACE_BT601625);
CAMX_STATIC_ASSERT(CSLColorSpaceBT601525    == CAM_COLOR_SPACE_BT601525);
CAMX_STATIC_ASSERT(CSLColorSpaceBT709       == CAM_COLOR_SPACE_BT709);
CAMX_STATIC_ASSERT(CSLColorSpaceDepth       == CAM_COLOR_SPACE_DEPTH);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CSLIODirection definitions comparison with KMD definitions
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CAMX_STATIC_ASSERT(CSLIODirectionInput      == CAM_BUF_INPUT);
CAMX_STATIC_ASSERT(CSLIODirectionOutput     == CAM_BUF_OUTPUT);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CSLVersion definitions comparison with KMD definitions
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CAMX_STATIC_ASSERT(sizeof(CSLVersion)                      == sizeof(struct cam_hw_version));
CAMX_STATIC_ASSERT(offsetof(CSLVersion, majorVersion)      == offsetof(struct cam_hw_version, major));
CAMX_STATIC_ASSERT(offsetof(CSLVersion, minorVersion)      == offsetof(struct cam_hw_version, minor));
CAMX_STATIC_ASSERT(offsetof(CSLVersion, revVersion)        == offsetof(struct cam_hw_version, incr));


#endif // ANDROID
