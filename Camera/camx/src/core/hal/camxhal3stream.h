////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016, 2018 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxhal3stream.h
/// @brief Declarations for the HAL3Stream class.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef CAMXHAL3STREAM_H
#define CAMXHAL3STREAM_H

#include "camxdefs.h"
#include "camxhal3types.h"
#include "camxincs.h"

CAMX_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Forward declarations
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
enum class Format;

/// Max entries for tracking in which frames the stream is enabled
static const UINT MaxFrameEnabledEntries = 64;

/// @brief Information about in which frames is the stream enabled
struct FrameEnabledInfo
{
    UINT32 enabledInFrame[MaxFrameEnabledEntries];      ///< Framenumber in which the stream is enabled
    UINT32 head;                                        ///< The next frame number in which the stream is enabled
    UINT32 tail;                                        ///< Next index in the array to start adding more entries
};

/// @brief HDR mode for the stream
enum StreamHDRMode
{
    HDRModeNone        = 0,   ///< None HDR mode
    HDRModeHLG,               ///< HLG mode
    HDRModeHDR10,             ///< HDR 10 bit mode
    HDRModeHDR10Plus,         ///< HDR10+ mode
    HDRModePQ,                ///< PQ mode
    HDRModeMax                ///< Invalid Max value
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief The HAL3Stream class is a helper class to HAL3Device which abstracts the stream properties and configurations
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class HAL3Stream
{
public:
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Public Methods
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// HAL3Stream
    ///
    /// @brief  Constructor for HAL3Stream object.
    ///
    /// @param  pStream         Native stream coming from request
    /// @param  streamIndex     The original index of pStream in the Camera3StreamConfig::ppStreams array passed in to
    ///                         configure_streams()
    /// @param  bufferFormat    Buffer format
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    HAL3Stream(
        Camera3Stream*  pStream,
        const UINT32    streamIndex,
        Format          bufferFormat);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// ~HAL3Stream
    ///
    /// @brief  Destructor for HAL3Stream object.
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    ~HAL3Stream();

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// GetNativeStream
    ///
    /// @brief  Retrieve the native Camera3Stream
    ///
    /// @return Pointer to Camera3Stream if successful
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CAMX_INLINE Camera3Stream* GetNativeStream() const
    {
        return m_pStream;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// GetStreamIndex
    ///
    /// @brief  Returns the index of this stream which corresponds to the original index in the Camera3StreamConfig::ppStreams
    ///         array passed in to configure_streams().
    ///
    /// @return The index of this stream
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CAMX_INLINE UINT32 GetStreamIndex() const
    {
        return m_streamIndex;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// SetStreamIndex
    ///
    /// @brief  Set the index of this stream. The index corresponds to the original index in the Camera3StreamConfig::ppStreams
    ///         array passed in to configure_streams().
    ///
    /// @param  streamIndex The index which corresponds to the original index in the Camera3StreamConfig::ppStreams
    ///                     array passed in to configure_streams().
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CAMX_INLINE VOID SetStreamIndex(
        const UINT32 streamIndex)
    {
        m_streamIndex = streamIndex;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// GetGrallocUsage
    ///
    /// @brief  Gets the gralloc usage flags in the native stream wrapped by this HAL3Stream object.
    ///
    /// @return gralloc usage flags
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    GrallocUsage64 GetGrallocUsage();

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// SetNativeProducerGrallocUsage
    ///
    /// @brief  Sets the gralloc usage flags in the native stream wrapped by this HAL3Stream object.
    ///
    /// @param  producerUsage The producer usage flags required for the native stream wrapped by this HAL3Stream object.
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CAMX_INLINE VOID SetNativeProducerGrallocUsage(
        const GrallocProducerUsage producerUsage)
    {
        if (m_pStream->pHalStream)
        {
            m_pStream->pHalStream->producerUsage = producerUsage;
        }
    }

     ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
     /// SetNativeGrallocUsage
     ///
     /// @brief  Sets the gralloc usage flags in the native stream wrapped by this HAL3Stream object.
     ///
     /// @param  usage The producer usage flags required for the native stream wrapped by this HAL3Stream object.
     ///
     /// @return None
     ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CAMX_INLINE VOID SetNativeGrallocUsage(
        const GrallocUsage usage)
    {
        m_pStream->grallocUsage = usage;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// SetNativeConsumerGrallocUsage
    ///
    /// @brief  Sets the gralloc usage flags in the native stream wrapped by this HAL3Stream object.
    ///
    /// @param  consumerUsage The consumer usage flags required for the native stream wrapped by this HAL3Stream object.
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CAMX_INLINE VOID SetNativeConsumerGrallocUsage(
        const GrallocConsumerUsage consumerUsage)
    {
        if (m_pStream->pHalStream)
        {
            m_pStream->pHalStream->consumerUsage = consumerUsage;
        }
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// SetNativeOverrideFormat
    ///
    /// @brief  Sets the override format in the native stream wrapped by this HAL3Stream object.
    ///
    /// @param  overrideFormat override format for gralloc pixel format
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CAMX_INLINE VOID SetNativeOverrideFormat(
        const HALPixelFormat overrideFormat)
    {
        if (m_pStream->pHalStream)
        {
            m_pStream->pHalStream->overrideFormat = overrideFormat;
        }
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// GetNativeOverrideFormat
    ///
    /// @brief  Gets the override format in the native stream wrapped by this HAL3Stream object.
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CAMX_INLINE HALPixelFormat GetNativeOverrideFormat()
    {
        if (m_pStream->pHalStream)
        {
            return m_pStream->pHalStream->overrideFormat;
        }
        else
        {
            return m_pStream->format;
        }
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// SetNativeMaxNumBuffers
    ///
    /// @brief  Sets the maximum number of in-flight buffers required for the native stream wrapped by this HAL3Stream object.
    ///
    /// @param  maxNumBuffers The maximum number of in-flight buffers required for the native stream wrapped by this HAL3Stream
    ///                       object.
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CAMX_INLINE VOID SetNativeMaxNumBuffers(
        const UINT32 maxNumBuffers)
    {
        m_pStream->maxNumBuffers = maxNumBuffers;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// IsStreamReused
    ///
    /// @brief  Indicates whether this stream is being reused
    ///
    /// @return TRUE if this stream is being reused, FALSE otherwise
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CAMX_INLINE BOOL IsStreamReused() const
    {
        return m_streamReused;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// SetStreamReused
    ///
    /// @brief  Sets the flags indicating whether this stream is being reused.
    ///
    /// @param  streamReused A flag indicating whether this stream is being reused.
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CAMX_INLINE VOID SetStreamReused(
        const BOOL streamReused)
    {
        m_streamReused = streamReused;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// IsStreamConfigMatch
    ///
    /// @brief  Indicates whether the configuration of the incoming stream match the one wrapped by this object.
    ///
    /// @param  pStream Pointer to the stream configuration to check for a match with the one wrapped by this object.
    ///
    /// @return TRUE if the configuration of the incoming stream matches the one wrapped by this object, FALSE otherwise.
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    BOOL IsStreamConfigMatch(
        const Camera3Stream* pStream
        ) const;

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// GetInternalFormat
    ///
    /// @brief  Gets the format selected for the stream
    ///
    /// @return Format
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CAMX_INLINE Format GetInternalFormat() const
    {
        return m_selectedFormat;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// SetInternalFormat
    ///
    /// @brief  Sets the format selected for the stream
    ///
    /// @param  format  Format
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CAMX_INLINE VOID SetInternalFormat(
        Format format)
    {
        m_selectedFormat = format;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// AddEnabledInFrame
    ///
    /// @brief  Add the frame in which the stream got enabled
    ///
    /// @param  frameNum    Frame number in which it got enabled
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CAMX_INLINE VOID AddEnabledInFrame(
        UINT32 frameNum)
    {
        m_frameEnabledInfo.enabledInFrame[m_frameEnabledInfo.tail] = frameNum;
        m_frameEnabledInfo.tail = ((m_frameEnabledInfo.tail + 1) % MaxFrameEnabledEntries);
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// IsNextExpectedResultFrame
    ///
    /// @brief  Determines if the passed in frame number is the next frame number whose results are expected
    ///
    /// @param  frameNum    Frame number being queried
    ///
    /// @return TRUE if the passed in frame number is the next expected result frame
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CAMX_INLINE BOOL IsNextExpectedResultFrame(
        UINT32 frameNum)
    {
        return ((frameNum == m_frameEnabledInfo.enabledInFrame[m_frameEnabledInfo.head]) ? TRUE : FALSE);
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// MoveToNextExpectedResultFrame
    ///
    /// @brief  Function to move to the next expected result frame number
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CAMX_INLINE VOID MoveToNextExpectedResultFrame()
    {
        m_frameEnabledInfo.head = ((m_frameEnabledInfo.head + 1) % MaxFrameEnabledEntries);
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// ResetEnabledFrameInfo
    ///
    /// @brief  Reset EnabledFrameInfo
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CAMX_INLINE VOID ResetEnabledFrameInfo()
    {
        Utils::Memset(&m_frameEnabledInfo, 0, sizeof(m_frameEnabledInfo));
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// SetHDRMode
    ///
    /// @brief  Update HDR mode for the stream
    ///
    /// @param  hdrMode  New hdr mode
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CAMX_INLINE VOID SetHDRMode(
        UINT8 hdrMode)
    {
        m_HDRMode = hdrMode;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// GetHDRMode
    ///
    /// @brief  Get HDR mode of stream
    ///
    /// @return The current HDR mode of this stream
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CAMX_INLINE UINT8 GetHDRMode() const
    {
        return m_HDRMode;
    }


    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// SetPortId
    ///
    /// @brief  Update port Id for the stream
    ///
    /// @param  portId  port id associated with stream
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CAMX_INLINE VOID SetPortId(
        UINT32 portId)
    {
        m_portId = portId;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// GetPortId
    ///
    /// @brief  Get port Id of stream
    ///
    /// @return The port Id of node to which this stream is input
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CAMX_INLINE UINT32 GetPortId() const
    {
        return m_portId;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// IsPreviewStream
    ///
    /// @brief  Checks if stream is for preview
    ///
    /// @return TRUE if stream is for preview else FALSE
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CAMX_INLINE BOOL IsPreviewStream()
    {
        BOOL result = FALSE;
        if (( (StreamTypeBidirectional == m_pStream->streamType) ||
            (StreamTypeOutput == m_pStream->streamType)) &&
            (GrallocUsageHwTexture == (GrallocUsageHwTexture & m_pStream->pHalStream->producerUsage)))
        {
            result = TRUE;
        }

        return result;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// IsVideoStream
    ///
    /// @brief  Checks if stream is for video
    ///
    /// @return TRUE if stream is for video else FALSE
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CAMX_INLINE BOOL IsVideoStream()
    {
        BOOL result = FALSE;

        if (0 != (GrallocUsageHwVideoEncoder & GetGrallocUsage()))
        {
            result = TRUE;
        }

        return result;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// IsSnapshotStream
    ///
    /// @brief  Checks if stream is for snapshot
    ///
    /// @return TRUE if stream is for preview else FALSE
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CAMX_INLINE BOOL IsSnapshotStream()
    {
        BOOL result = FALSE;

        if ((StreamTypeOutput == m_pStream->streamType) &&
            (GrallocUsageSwReadOften == (GrallocUsageSwReadOften & GetGrallocUsage())) &&
            (HALDataspaceDepth != m_pStream->dataspace))
        {
            result = TRUE;
        }

        return result;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// IsDepthStream
    ///
    /// @brief  Checks if stream is for depth
    ///
    /// @return TRUE if stream is for depth else FALSE
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CAMX_INLINE BOOL IsDepthStream()
    {
        BOOL result = FALSE;

        if (HALDataspaceDepth == m_pStream->dataspace)
        {
            result = TRUE;
        }

        return result;
    }

private:
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Private Methods
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    // Do not implement the copy constructor or assignment operator
    HAL3Stream(const HAL3Stream& rHAL3Stream) = delete;
    HAL3Stream& operator= (const HAL3Stream& rHAL3Stream) = delete;

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Private Data
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    Camera3Stream*  m_pStream;          ///< Placeholder for native stream
    Camera3HalStream m_hidlStream;   ///< Placeholder for hidl hal stream
    UINT32          m_streamIndex;      ///< The index of this stream which corresponds to the original index in the
                                        ///  Camera3StreamConfig::ppStreams array passed in to configure_streams().
    BOOL            m_streamReused;     ///< Flag to indicate whether this stream is being reused
    Format          m_selectedFormat;   ///< Selected format for this stream based on the HAL format in the native stream
    UINT8           m_HDRMode;          ///< Current HDR Mode.
    UINT32          m_portId;           ///< Port id of node to which stream wrapper is input

    /// @todo (CAMX-1797) Probably should start using this to determine if a stream is enabled in a particular frame or not
    ///                   Added this for CHI but should start using it in general
    FrameEnabledInfo m_frameEnabledInfo;    ///< Frame enabled info
};

CAMX_NAMESPACE_END

#endif // CAMXHAL3STREAM_H
