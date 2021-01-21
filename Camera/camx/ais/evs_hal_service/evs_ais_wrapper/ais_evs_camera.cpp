/*
 * Copyright (c) 2019 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 * Not a contribution.
 *
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "ais_evs_camera.h"
#include "ais_evs_enumerator.h"
#include "buffer_copy.h"

#include <ui/GraphicBufferAllocator.h>
#include <ui/GraphicBufferMapper.h>
#include <gralloc_priv.h>
#include <stdlib.h>
#include <sys/mman.h>


namespace android {
namespace hardware {
namespace automotive {
namespace evs {
namespace V1_0 {
namespace implementation {


// Arbitrary limit on number of graphics buffers allowed to be allocated
// Safeguards against unreasonable resource consumption and provides a testable limit
static const unsigned MAX_BUFFERS_IN_FLIGHT = 100;


EvsAISCamera::EvsAISCamera(const char *deviceName) :
        mFramesAllowed(0),
        mFramesInUse(0) {
    ALOGE("EvsAISCamera instantiated");

    mDescription.cameraId = deviceName;

    // Initialize qcarcam camera
    int camera_id;
    sscanf(deviceName, "%d", &camera_id);
    qcarcam_context = qcarcam_open((qcarcam_input_desc_t)camera_id);
    if (qcarcam_context == 0)
    {
        ALOGE("qcarcam_open() failed");
    }
    else
    {
        ALOGE("qcarcam_open() success for stream %d qcarcam_context = %p", camera_id, qcarcam_context);
        mMultiCamQLock.lock();
        sQcarcamCameraArray[camera_id].qcarcam_context = qcarcam_context;
        pCurrentRecord = &sQcarcamCameraArray[camera_id];
        mMultiCamQLock.unlock();
    }
    mRunMode = STOPPED;

    // NOTE:  Our current spec says only support NV21 -- can we stick to that with software
    // conversion?  Will this work with the hardware texture units?
    // TODO:  Settle on the one official format that works on all platforms
    // TODO:  Get NV21 working?  It is scrambled somewhere along the way right now.
//    mFormat = HAL_PIXEL_FORMAT_YCRCB_420_SP;    // 420SP == NV21
    //mFormat = HAL_PIXEL_FORMAT_YCBCR_422_I;
#ifdef ENABLE_RGBA_CONVERSION
    mFormat = HAL_PIXEL_FORMAT_RGBA_8888;
#else
    mFormat = HAL_PIXEL_FORMAT_CbYCrY_422_I;
#endif

    // How we expect to use the gralloc buffers we'll exchange with our client
    mUsage  = GRALLOC_USAGE_HW_TEXTURE     |
              GRALLOC_USAGE_SW_READ_RARELY |
              GRALLOC_USAGE_SW_WRITE_OFTEN;
}


EvsAISCamera::~EvsAISCamera() {
    qcarcam_ret_t ret;
    ALOGE("EvsAISCamera being destroyed");
    // Close AIS camera stream
    ret = qcarcam_close(this->qcarcam_context);
    if (ret != QCARCAM_RET_OK)
    {
        ALOGE("qcarcam_close() failed");
    }
    shutdown();
    mMultiCamQLock.lock();
    ALOGE("Removing qcarcam_context = %p from Q",this->qcarcam_context);
    gQcarCamClientData *pRecord = findEventQByHndl(this->qcarcam_context);
    pRecord->qcarcam_context = NOT_IN_USE;
    mMultiCamQLock.unlock();
    this->qcarcam_context = NOT_IN_USE;
    pCurrentRecord = nullptr;
}


//
// This gets called if another caller "steals" ownership of the camera
//
void EvsAISCamera::shutdown()
{
    ALOGE("EvsAISCamera shutdown");

    // Make sure our output stream is cleaned up
    // (It really should be already)
    stopVideoStream();

    // Drop all the graphics buffers we've been using
    if (mBuffers.size() > 0) {
        GraphicBufferAllocator& alloc(GraphicBufferAllocator::get());
        for (auto&& rec : mBuffers) {
            if (rec.inUse) {
                ALOGE("Error - releasing buffer despite remote ownership");
            }
            alloc.free(rec.handle);
            rec.handle = nullptr;
        }
        mBuffers.clear();
    }
}

int EvsAISCamera::getStrideMultiplayer(uint32_t mFormat)
{
    int stride_val = 1;
    switch(mFormat)
    {
        case HAL_PIXEL_FORMAT_RGBA_8888:
            stride_val = 4;
            break;
        case HAL_PIXEL_FORMAT_CbYCrY_422_I:
            stride_val = 2;
            break;
        default:
            stride_val = 1;
    }
    return stride_val;
}

// Methods from ::android::hardware::automotive::evs::V1_0::IEvsCamera follow.
Return<void> EvsAISCamera::getCameraInfo(getCameraInfo_cb _hidl_cb) {
    ALOGE("getCameraInfo");

    // Send back our self description
    _hidl_cb(mDescription);
    return Void();
}


Return<EvsResult> EvsAISCamera::setMaxFramesInFlight(uint32_t bufferCount) {
    ALOGE("setMaxFramesInFlight bufferCount = %d",bufferCount);
    std::lock_guard<std::mutex> lock(mAccessLock);

    // We cannot function without at least one video buffer to send data
    if (bufferCount < 1) {
        ALOGE("Ignoring setMaxFramesInFlight with less than one buffer requested");
        return EvsResult::INVALID_ARG;
    }

    // Update our internal state
    if (!setAvailableFrames_Locked(bufferCount)) {
        return EvsResult::BUFFER_NOT_AVAILABLE;
    }

    int ret = 0, i;
    if (bufferCount >= MIN_AIS_BUF_CNT)
        p_buffers_output.n_buffers = bufferCount;
    else
        p_buffers_output.n_buffers = MIN_AIS_BUF_CNT;
    p_buffers_output.color_fmt = (qcarcam_color_fmt_t)101187587;
    p_buffers_output.buffers = (qcarcam_buffer_t *)calloc(p_buffers_output.n_buffers, sizeof(*p_buffers_output.buffers));
    gfx_bufs = (sp<GraphicBuffer>*)calloc(p_buffers_output.n_buffers, sizeof(sp<GraphicBuffer>));
    qcarcam_mmap_buffer = (test_util_buffer_t*)calloc(p_buffers_output.n_buffers, sizeof(test_util_buffer_t));

    if (p_buffers_output.buffers == 0)
    {
        ALOGE("alloc qcarcam_buffer failed");
        return EvsResult::BUFFER_NOT_AVAILABLE;
    }
    const int graphicsUsage  = android::GraphicBuffer::USAGE_HW_TEXTURE | android::GraphicBuffer::USAGE_SW_WRITE_OFTEN;
    /* Auto camera supports only UYVY so hardcode here */
    const int nativeBufferFormat = HAL_PIXEL_FORMAT_CbYCrY_422_I;
    for (i = 0; i < (int)p_buffers_output.n_buffers; ++i)
    {
        p_buffers_output.buffers[i].n_planes = 1;
        p_buffers_output.buffers[i].planes[0].width = AIS_FRAME_WIDTH;
        p_buffers_output.buffers[i].planes[0].height = AIS_FRAME_HEIGHT;

        /* Auto camera supports only UYVY so hardcode stride here */
        p_buffers_output.buffers[i].planes[0].stride = AIS_FRAME_WIDTH*2;
        p_buffers_output.buffers[i].planes[0].size = p_buffers_output.buffers[i].planes[0].height * p_buffers_output.buffers[i].planes[0].stride;

        ALOGE("%dx%d %d %d", p_buffers_output.buffers[i].planes[0].width, p_buffers_output.buffers[i].planes[0].height, p_buffers_output.buffers[i].planes[0].stride, p_buffers_output.buffers[i].planes[0].size);

        gfx_bufs[i] = NULL;
        gfx_bufs[i] = new GraphicBuffer(p_buffers_output.buffers[i].planes[0].width,
                p_buffers_output.buffers[i].planes[0].height,
                nativeBufferFormat,
                graphicsUsage);

        struct private_handle_t * private_hndl = (struct private_handle_t *)(gfx_bufs[i]->getNativeBuffer()->handle);
        ALOGE("%d %p\n",i, private_hndl);
        p_buffers_output.buffers[i].planes[0].p_buf = (void*)(uintptr_t)(private_hndl->fd);

        qcarcam_mmap_buffer[i].size = p_buffers_output.buffers[i].planes[0].size;

        qcarcam_mmap_buffer[i].ptr = mmap(NULL, qcarcam_mmap_buffer[i].size,
                        PROT_READ | PROT_WRITE, MAP_SHARED,
                        private_hndl->fd, 0);
        if (qcarcam_mmap_buffer[i].ptr)
            ALOGE("mmap success\n");
    }
    ALOGE(" calling qcarcam_s_buffers");
    p_buffers_output.flags = 0;
    ret = qcarcam_s_buffers(qcarcam_context, &p_buffers_output);
    if (ret != QCARCAM_RET_OK)
    {
        ALOGE("qcarcam_s_buffers failed!");
        return EvsResult::BUFFER_NOT_AVAILABLE;
    }
    else
    {
        ALOGE("qcarcam_s_buffers success");
    }
    return EvsResult::OK;
}


Return<EvsResult> EvsAISCamera::startVideoStream(const ::android::sp<IEvsCameraStream>& stream)  {
    ALOGE("startVideoStream");

    int ret = 0;
    mStream = stream;
    qcarcam_param_value_t param;
    param.ptr_value = (void *)&EvsAISCamera::evs_ais_event_cb;
    ret = qcarcam_s_param(this->qcarcam_context, QCARCAM_PARAM_EVENT_CB, &param);

    param.uint_value = QCARCAM_EVENT_FRAME_READY | QCARCAM_EVENT_INPUT_SIGNAL | QCARCAM_EVENT_ERROR;
    ret = qcarcam_s_param(this->qcarcam_context, QCARCAM_PARAM_EVENT_MASK, &param);

    ret = qcarcam_start(this->qcarcam_context);
    if (ret != QCARCAM_RET_OK)
    {
        ALOGE("qcarcam_start() failed");
        return EvsResult::UNDERLYING_SERVICE_ERROR;
    }
    else
    {
        ALOGE("qcarcam_start() success");
    }

    // Set the state of our background thread
    int prevRunMode = mRunMode.fetch_or(RUN);
    if (prevRunMode & RUN) {
        // The background thread is already running, so we can't start a new stream
        ALOGE("Already in RUN state, so we can't start a new streaming thread");
        return EvsResult::OK;
    }

    // Fire up a thread to receive and dispatch the qcarcam frames
    mCaptureThread = std::thread([this](){ collectFrames(); });

    return EvsResult::OK;
}


Return<void> EvsAISCamera::doneWithFrame(const BufferDesc& buffer)  {
    //ALOGE("doneWithFrame");
    std::lock_guard <std::mutex> lock(mAccessLock);

    if (buffer.memHandle == nullptr) {
        ALOGE("ignoring doneWithFrame called with null handle");
    } else if (buffer.bufferId >= mBuffers.size()) {
        ALOGE("ignoring doneWithFrame called with invalid bufferId %d (max is %zu)",
                buffer.bufferId, mBuffers.size()-1);
    } else if (!mBuffers[buffer.bufferId].inUse) {
        ALOGE("ignoring doneWithFrame called on frame %d which is already free",
                buffer.bufferId);
    } else {
        // Mark the frame as available
        mBuffers[buffer.bufferId].inUse = false;
        mFramesInUse--;

        // If this frame's index is high in the array, try to move it down
        // to improve locality after mFramesAllowed has been reduced.
        if (buffer.bufferId >= mFramesAllowed) {
            // Find an empty slot lower in the array (which should always exist in this case)
            for (auto&& rec : mBuffers) {
                if (rec.handle == nullptr) {
                    rec.handle = mBuffers[buffer.bufferId].handle;
                    mBuffers[buffer.bufferId].handle = nullptr;
                    break;
                }
            }
        }
    }

    return Void();
}


Return<void> EvsAISCamera::stopVideoStream()  {
    ALOGE("stopVideoStream");

    qcarcam_ret_t ret = QCARCAM_RET_OK;
    // Stop AIS stream
    ret = qcarcam_stop(this->qcarcam_context);
    if (ret != QCARCAM_RET_OK)
    {
        ALOGE("qcarcam_stop() failed");
    }

    // Tell the background thread to stop
    int prevRunMode = mRunMode.fetch_or(STOPPING);
    if (prevRunMode == STOPPED) {
        // The background thread wasn't running, so set the flag back to STOPPED
        mRunMode = STOPPED;
    } else if (prevRunMode & STOPPING) {
        ALOGE("stopStream called while stream is already stopping.  Reentrancy is not supported!");
        return Void();
    } else {
        pCurrentRecord->gCV.notify_one();
        // Block until the background thread is stopped
        if (mCaptureThread.joinable()) {
            mCaptureThread.join();
        }
        ALOGI("Capture thread stopped.");
    }

    if (mStream != nullptr) {
        std::unique_lock <std::mutex> lock(mAccessLock);

        // Send one last NULL frame to signal the actual end of stream
        BufferDesc nullBuff = {};
        auto result = mStream->deliverFrame(nullBuff);
        if (!result.isOk()) {
            ALOGE("Error delivering end of stream marker");
        }

        // Drop our reference to the client's stream receiver
        mStream = nullptr;
    }

    return Void();
}


Return<int32_t> EvsAISCamera::getExtendedInfo(uint32_t /*opaqueIdentifier*/)  {
    ALOGE("getExtendedInfo");
    // Return zero by default as required by the spec
    return 0;
}


Return<EvsResult> EvsAISCamera::setExtendedInfo(uint32_t /*opaqueIdentifier*/,
                                                int32_t /*opaqueValue*/)  {
    ALOGE("setExtendedInfo");
    // Return zero by default as required by the spec
    return EvsResult::OK;
}


bool EvsAISCamera::setAvailableFrames_Locked(unsigned bufferCount) {
    if (bufferCount < 1) {
        ALOGE("Ignoring request to set buffer count to zero");
        return false;
    }
    if (bufferCount > MAX_BUFFERS_IN_FLIGHT) {
        ALOGE("Rejecting buffer request in excess of internal limit");
        return false;
    }

    // Is an increase required?
    if (mFramesAllowed < bufferCount) {
        // An increase is required
        unsigned needed = bufferCount - mFramesAllowed;
        ALOGE("Allocating %d buffers for camera frames", needed);

        unsigned added = increaseAvailableFrames_Locked(needed);
        if (added != needed) {
            // If we didn't add all the frames we needed, then roll back to the previous state
            ALOGE("Rolling back to previous frame queue size");
            decreaseAvailableFrames_Locked(added);
            return false;
        }
    } else if (mFramesAllowed > bufferCount) {
        // A decrease is required
        unsigned framesToRelease = mFramesAllowed - bufferCount;
        ALOGE("Returning %d camera frame buffers", framesToRelease);

        unsigned released = decreaseAvailableFrames_Locked(framesToRelease);
        if (released != framesToRelease) {
            // This shouldn't happen with a properly behaving client because the client
            // should only make this call after returning sufficient outstanding buffers
            // to allow a clean resize.
            ALOGE("Buffer queue shrink failed -- too many buffers currently in use?");
        }
    }

    return true;
}


unsigned EvsAISCamera::increaseAvailableFrames_Locked(unsigned numToAdd) {
    // Acquire the graphics buffer allocator
    GraphicBufferAllocator &alloc(GraphicBufferAllocator::get());

    unsigned added = 0;

    while (added < numToAdd) {
        unsigned pixelsPerLine;
        buffer_handle_t memHandle = nullptr;

        status_t result = alloc.allocate(AIS_FRAME_WIDTH*getStrideMultiplayer(mFormat),
                                         AIS_FRAME_HEIGHT,
                                         mFormat, 1,
                                         mUsage,
                                         &memHandle, &pixelsPerLine, 0, "EvsAISCamera");
        if (result != NO_ERROR) {
            ALOGE("Error %d allocating %d x %d graphics buffer",
                  result,
                  AIS_FRAME_WIDTH,
                  AIS_FRAME_HEIGHT);
            break;
        }
        if (!memHandle) {
            ALOGE("We didn't get a buffer handle back from the allocator");
            break;
        }
        if (mStride) {
            if (mStride != pixelsPerLine) {
                ALOGE("We did not expect to get buffers with different strides!");
            }
        } else {
            // Gralloc defines stride in terms of pixels per line
            mStride = pixelsPerLine;
            ALOGE("mstride = %d\n",mStride);
        }
        // Find a place to store the new buffer
        bool stored = false;
        for (auto&& rec : mBuffers) {
            if (rec.handle == nullptr) {
                // Use this existing entry
                rec.handle = memHandle;
                rec.inUse = false;
                stored = true;
                break;
            }
        }
        if (!stored) {
            // Add a BufferRecord wrapping this handle to our set of available buffers
            mBuffers.emplace_back(memHandle);
        }

        mFramesAllowed++;
        added++;
    }

    return added;
}


unsigned EvsAISCamera::decreaseAvailableFrames_Locked(unsigned numToRemove) {
    // Acquire the graphics buffer allocator
    GraphicBufferAllocator &alloc(GraphicBufferAllocator::get());

    unsigned removed = 0;

    for (auto&& rec : mBuffers) {
        // Is this record not in use, but holding a buffer that we can free?
        if ((rec.inUse == false) && (rec.handle != nullptr)) {
            // Release buffer and update the record so we can recognize it as "empty"
            alloc.free(rec.handle);
            rec.handle = nullptr;

            mFramesAllowed--;
            removed++;

            if (removed == numToRemove) {
                break;
            }
        }
    }

    return removed;
}

/**
 * Qcarcam event callback function
 * @param hndl
 * @param event_id
 * @param p_payload
 */
void EvsAISCamera::evs_ais_event_cb(qcarcam_hndl_t hndl, qcarcam_event_t event_id, qcarcam_event_payload_t *p_payload)
{
    int q_size = 0;
    switch (event_id)
    {
        case QCARCAM_EVENT_FRAME_READY:
            //ALOGI("received QCARCAM_EVENT_FRAME_READY");
            break;
        default:
            ALOGE("received unsupported event %d", event_id);
            break;
    }

    //TODO::Protect with semaphore
    // This stream should be already in Queue
    mMultiCamQLock.lock();
    gQcarCamClientData *pRecord = findEventQByHndl(hndl);
    if (!pRecord) {
            ALOGE("ERROR::Requested qcarcam_hndl %p not found", hndl);
            mMultiCamQLock.unlock();
            return;
    }
    else {
        //ALOGI("Found qcarcam_hndl in the list");
    }
    mMultiCamQLock.unlock();

    // Push the event into queue
    if (pRecord)
    {
        std::unique_lock<std::mutex> lk(pRecord->gEventQLock);
        q_size = pRecord->sQcarcamList.size();
        if (q_size < 3)
        {
            pRecord->sQcarcamList.emplace_back(event_id, p_payload);
            //ALOGI("Pushed event id %d to Event Queue", (int)event_id);
        }
        else {
            ALOGE("Max events are Queued in event Q");
        }
        lk.unlock();
        pRecord->gCV.notify_one();
    }
}

gQcarCamClientData* EvsAISCamera::findEventQByHndl(const qcarcam_hndl_t qcarcam_hndl) {
    // Find the named camera
    int i = 0;
    for (i=0; i<MAX_AIS_CAMERAS; i++) {
        if (sQcarcamCameraArray[i].qcarcam_context == qcarcam_hndl) {
            // Found a match!
            return &sQcarcamCameraArray[i];
        }
    }
    ALOGE("ERROR qcarcam_context = %p did not match!!\n",qcarcam_hndl);

    // We didn't find a match
    return nullptr;
}

// This runs on a background thread to receive and dispatch qcarcam frames
void EvsAISCamera::collectFrames() {
    int q_size = 0;
    qcarcam_ret_t ret;
    ALOGE("collectFrames thread running");

    if (!this->pCurrentRecord) {
        ALOGE("ERROR::Requested qcarcam_hndl not found in the list");
        ALOGI("VideoCapture thread ending");
        mRunMode = STOPPED;
        return;
    }
    // Run until our atomic signal is cleared
    while (mRunMode == RUN) {
        // Wait for a buffer to be ready
        std::unique_lock<std::mutex> lk(this->pCurrentRecord->gEventQLock);
        this->pCurrentRecord->gCV.wait(lk);
        // fetch events from event Q
        q_size = this->pCurrentRecord->sQcarcamList.size();
        if (q_size) {
            //ALOGI("Found %d events in the Event Queue", q_size);
            this->pCurrentRecord->sQcarcamList.pop_front();
            lk.unlock();

            ret = qcarcam_get_frame(this->qcarcam_context, &frame_info, 500000000, 0);

            if (QCARCAM_RET_OK != ret)
            {
                ALOGE("qcarcam_get_frame failed 0x%p ret %d\n", this->qcarcam_context, ret);
            }
            else
            {
                //ALOGI("Fetched new frame from AIS");
                //dumpqcarcamFrame(&frame_info);
                sendFramesToApp(&frame_info);
                ret = qcarcam_release_frame(this->qcarcam_context, frame_info.idx);
                if (QCARCAM_RET_OK != ret)
                {
                    ALOGE("qcarcam_release_frame() %d failed", frame_info.idx);
                }
            }
        }
        else {
            //ALOGI("Event Q is empty");
            lk.unlock();
        }
    }

    // Mark ourselves stopped
    ALOGI("VideoCapture thread ending");
    mRunMode = STOPPED;
}

void EvsAISCamera::dumpqcarcamFrame(qcarcam_frame_info_t *frame_info)
{
    FILE *fp;
    size_t numBytesWritten = 0;
    size_t numByteToWrite = 0;
    unsigned char *pBuf = 0;
    static int frame_cnt = 0;
    char filename[128];

    if (frame_cnt > 5)
        return;

    sprintf(filename, "%s_%d","/sdcard/Pictures/evs_frame",frame_cnt);

    if (NULL == frame_info)
        ALOGE("Enpty frame info");

    if (!qcarcam_mmap_buffer[frame_info->idx].ptr)
    {
        ALOGE("buffer is not mapped");
        return;
    }

    fp = fopen(filename, "w+");

    ALOGE("dumping qcarcam frame %s numbytes %d", filename,
        qcarcam_mmap_buffer[frame_info->idx].size);

    if (0 != fp)
    {
        pBuf = (unsigned char *)qcarcam_mmap_buffer[frame_info->idx].ptr;
        numByteToWrite = qcarcam_mmap_buffer[frame_info->idx].size;

        numBytesWritten = fwrite(pBuf, 1, numByteToWrite, fp);

        if (numBytesWritten != numByteToWrite)
        {
            ALOGE("error no data written to file");
        }
        fclose(fp);
    }
    else
    {
        ALOGE("failed to open file");
        return;
    }
    frame_cnt++;
}

void EvsAISCamera::sendFramesToApp(qcarcam_frame_info_t *frame_info)
{
    bool readyForFrame = false;
    size_t idx = 0;

    void *pData = NULL;

    if (!qcarcam_mmap_buffer[frame_info->idx].ptr)
    {
        ALOGE("buffer is not mapped");
        return;
    }

    pData = (void *)qcarcam_mmap_buffer[frame_info->idx].ptr;

    // Lock scope for updating shared state
    {
        std::lock_guard<std::mutex> lock(mAccessLock);

        // Are we allowed to issue another buffer?
        if (mFramesInUse >= mFramesAllowed) {
            // Can't do anything right now -- skip this frame
            ALOGE("Skipped a frame because too many are in flight mFramesInUse = %d, mFramesAllowed = %d\n", mFramesInUse, mFramesAllowed);
        } else {
            // Identify an available buffer to fill
            for (idx = 0; idx < mBuffers.size(); idx++) {
                if (!mBuffers[idx].inUse) {
                    if (mBuffers[idx].handle != nullptr) {
                        // Found an available record, so stop looking
                        break;
                    }
                }
            }
            if (idx >= mBuffers.size()) {
                // This shouldn't happen since we already checked mFramesInUse vs mFramesAllowed
                ALOGE("Failed to find an available buffer slot\n");
            } else {
                // We're going to make the frame busy
                mBuffers[idx].inUse = true;
                mFramesInUse++;
                readyForFrame = true;
            }
        }
    }

    if (!readyForFrame) {
        // We need to return the vide buffer so it can capture a new frame
        //mVideo.markFrameConsumed();
        ALOGI("Frame is not ready");
    } else {
        // Assemble the buffer description we'll transmit below
        BufferDesc buff = {};
        buff.width      = p_buffers_output.buffers[frame_info->idx].planes[0].width;
        buff.height     = p_buffers_output.buffers[frame_info->idx].planes[0].height;
        buff.stride     = p_buffers_output.buffers[frame_info->idx].planes[0].width*getStrideMultiplayer(mFormat);
        buff.format     = mFormat;
        buff.usage      = mUsage;
        buff.bufferId   = idx;
        buff.memHandle  = mBuffers[idx].handle;

        // Lock our output buffer for writing
        void *targetPixels = nullptr;
        GraphicBufferMapper &mapper = GraphicBufferMapper::get();
        mapper.lock(buff.memHandle,
                GRALLOC_USAGE_SW_WRITE_OFTEN | GRALLOC_USAGE_SW_READ_RARELY,
                android::Rect(buff.width, buff.height),
                (void **) &targetPixels);

        // If we failed to lock the pixel buffer, we're about to crash, but log it first
        if (!targetPixels) {
            ALOGE("Camera failed to gain access to image buffer for writing");
        }

        // Transfer the video image into the output buffer, making any needed
        // format conversion along the way
#ifdef ENABLE_RGBA_CONVERSION
        fillRGBAFromUYVY(buff, (uint8_t*)targetPixels, pData, buff.stride);
#else
        fillTargetBuffer((uint8_t*)targetPixels, pData, qcarcam_mmap_buffer[frame_info->idx].size);
#endif

        //ALOGI("Target buffer is ready");
        // Unlock the output buffer
        mapper.unlock(buff.memHandle);

        //ALOGI("buff.stride = %u buff.mUsage = %u, buff.idx = %d",buff.stride,  buff.usage, buff.bufferId);
        //ALOGI("Sending %p as id %d", buff.memHandle.getNativeHandle(), buff.bufferId);

        // Issue the (asynchronous) callback to the client -- can't be holding the lock
        auto result = mStream->deliverFrame(buff);
        if (!result.isOk()) {
            // This can happen if the client dies and is likely unrecoverable.
            // To avoid consuming resources generating failing calls, we stop sending
            // frames.  Note, however, that the stream remains in the "STREAMING" state
            // until cleaned up on the main thread.
            ALOGE("Frame delivery call failed in the transport layer.");

            // Since we didn't actually deliver it, mark the frame as available
            std::lock_guard<std::mutex> lock(mAccessLock);
            mBuffers[idx].inUse = false;
            mFramesInUse--;
        }
    }
}

void EvsAISCamera::fillTargetBuffer(uint8_t* tgt, void* imgData, unsigned size) {
    uint32_t* src = (uint32_t*)imgData;
    uint32_t* dst = (uint32_t*)tgt;

    memcpy(dst, src, size);
}

#ifdef ENABLE_RGBA_CONVERSION
// Limit the given value to the provided range.  :)
float EvsAISCamera::clamp(float v, float min, float max) {
    if (v < min) return min;
    if (v > max) return max;
    return v;
}


uint32_t EvsAISCamera::yuvToRgbx(const unsigned char Y, const unsigned char Uin, const unsigned char Vin) {
    // Don't use this if you want to see the best performance.  :)
    // Better to do this in a pixel shader if we really have to, but on actual
    // embedded hardware we expect to be able to texture directly from the YUV data
    float U = Uin - 128.0f;
    float V = Vin - 128.0f;

    float Rf = Y + 1.140f*V;
    float Gf = Y - 0.395f*U - 0.581f*V;
    float Bf = Y + 2.032f*U;
    unsigned char R = (unsigned char)clamp(Rf, 0.0f, 255.0f);
    unsigned char G = (unsigned char)clamp(Gf, 0.0f, 255.0f);
    unsigned char B = (unsigned char)clamp(Bf, 0.0f, 255.0f);

    return ((R & 0xFF))       |
           ((G & 0xFF) << 8)  |
           ((B & 0xFF) << 16) |
           0xFF000000;  // Fill the alpha channel with ones
}

void EvsAISCamera::fillRGBAFromUYVY(const BufferDesc& tgtBuff, uint8_t* tgt, void* imgData, unsigned imgStride) {
    unsigned width = tgtBuff.width;
    unsigned height = tgtBuff.height;
    uint32_t* src = (uint32_t*)imgData;
    uint32_t* dst = (uint32_t*)tgt;
    unsigned srcStridePixels = imgStride / 4;
    unsigned dstStridePixels = tgtBuff.stride;

    const int srcRowPadding32 = srcStridePixels/2 - width/2;  // 2 bytes per pixel, 4 bytes per word
    const int dstRowPadding32 = dstStridePixels   - width;    // 4 bytes per pixel, 4 bytes per word

    for (unsigned r=0; r<height; r++) {
        for (unsigned c=0; c<width/2; c++) {
            // Note:  we're walking two pixels at a time here (even/odd)
            uint32_t srcPixel = *src++;

            uint8_t U  = (srcPixel)       & 0xFF;
            uint8_t Y1 = (srcPixel >> 8)  & 0xFF;
            uint8_t V  = (srcPixel >> 16) & 0xFF;
            uint8_t Y2 = (srcPixel >> 24) & 0xFF;

            // On the RGB output, we're writing one pixel at a time
            *(dst+0) = yuvToRgbx(Y1, U, V);
            *(dst+1) = yuvToRgbx(Y2, U, V);
            dst += 2;
        }

        // Skip over any extra data or end of row alignment padding
        src += srcRowPadding32;
        dst += dstRowPadding32;
    }
}
#endif /* ENABLE_RGBA_CONVERSION */


} // namespace implementation
} // namespace V1_0
} // namespace evs
} // namespace automotive
} // namespace hardware
} // namespace android
