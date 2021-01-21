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

#ifndef EVSAISCAMERA_H
#define EVSAISCAMERA_H

#include <android/hardware/automotive/evs/1.0/types.h>
#include <android/hardware/automotive/evs/1.0/IEvsCamera.h>
#include <ui/GraphicBuffer.h>

#include <thread>
#include <functional>

#include <list>
#include <atomic>
#include <thread>

#include "video_capture.h"
#include "qcarcam.h"
#include "qcarcam_types.h"

#define MAX_AIS_CAMERAS 12
#define MIN_AIS_BUF_CNT 3
#define AIS_FRAME_WIDTH 1280
#define AIS_FRAME_HEIGHT 720
#define NOT_IN_USE 0

typedef struct
{
    void* ptr;
    int size;
}test_util_buffer_t;

struct QEventDesc {
    qcarcam_event_t event_id;
    qcarcam_event_payload_t *p_payload;

    QEventDesc(qcarcam_event_t event, qcarcam_event_payload_t *payload) {event_id = event; p_payload = payload;}
};

typedef struct QcarcamCamera {
    qcarcam_hndl_t qcarcam_context;
    std::mutex gEventQLock;
    std::condition_variable gCV;
    std::list<QEventDesc> sQcarcamList;
}gQcarCamClientData;

static gQcarCamClientData sQcarcamCameraArray[MAX_AIS_CAMERAS];
static std::mutex mMultiCamQLock;

namespace android {
namespace hardware {
namespace automotive {
namespace evs {
namespace V1_0 {
namespace implementation {


// From EvsEnumerator.h
class EvsEnumerator;


class EvsAISCamera : public IEvsCamera {
public:
    // Methods from ::android::hardware::automotive::evs::V1_0::IEvsCamera follow.
    Return<void> getCameraInfo(getCameraInfo_cb _hidl_cb)  override;
    Return <EvsResult> setMaxFramesInFlight(uint32_t bufferCount) override;
    Return <EvsResult> startVideoStream(const ::android::sp<IEvsCameraStream>& stream) override;
    Return<void> doneWithFrame(const BufferDesc& buffer) override;
    Return<void> stopVideoStream() override;
    Return <int32_t> getExtendedInfo(uint32_t opaqueIdentifier) override;
    Return <EvsResult> setExtendedInfo(uint32_t opaqueIdentifier, int32_t opaqueValue) override;

    // Implementation details
    EvsAISCamera(const char *deviceName);
    virtual ~EvsAISCamera() override;
    void shutdown();

    const CameraDesc& getDesc() { return mDescription; };

private:
    // These three functions are expected to be called while mAccessLock is held
    bool setAvailableFrames_Locked(unsigned bufferCount);
    unsigned increaseAvailableFrames_Locked(unsigned numToAdd);
    unsigned decreaseAvailableFrames_Locked(unsigned numToRemove);

    sp <IEvsCameraStream> mStream = nullptr;  // The callback used to deliver each frame

    CameraDesc mDescription = {};   // The properties of this camera
    uint32_t mFormat = 0;           // Values from android_pixel_format_t
    uint32_t mUsage  = 0;           // Values from from Gralloc.h
    uint32_t mStride = 0;           // Pixels per row (may be greater than image width)

    struct BufferRecord {
        buffer_handle_t handle;
        bool inUse;

        explicit BufferRecord(buffer_handle_t h) : handle(h), inUse(false) {};
    };

    std::vector <BufferRecord> mBuffers;    // Graphics buffers to transfer images
    unsigned mFramesAllowed;                // How many buffers are we currently using
    unsigned mFramesInUse;                  // How many buffers are currently outstanding

    // Synchronization necessary to deconflict the capture thread from the main service thread
    // Note that the service interface remains single threaded (ie: not reentrant)
    std::mutex mAccessLock;
    qcarcam_hndl_t qcarcam_context;

    std::thread mCaptureThread;             // The thread we'll use to dispatch frames
    std::atomic<int> mRunMode;              // Used to signal the frame loop (see RunModes below)
    // Careful changing these -- we're using bit-wise ops to manipulate these
    enum RunModes {
        STOPPED     = 0,
        RUN         = 1,
        STOPPING    = 2,
    };
    void collectFrames();
    void dumpqcarcamFrame(qcarcam_frame_info_t *frame_info);
    void sendFramesToApp(qcarcam_frame_info_t *frame_info);
    void fillTargetBuffer(uint8_t* tgt, void* imgData, unsigned imgStride);
#ifdef ENABLE_RGBA_CONVERSION
    float clamp(float v, float min, float max);
    uint32_t yuvToRgbx(const unsigned char Y, const unsigned char Uin, const unsigned char Vin);
    void fillRGBAFromUYVY(const BufferDesc& tgtBuff, uint8_t* tgt, void* imgData, unsigned imgStride);
#endif

    static gQcarCamClientData* findEventQByHndl(const qcarcam_hndl_t qcarcam_hndl);

    sp<GraphicBuffer> *gfx_bufs;
    qcarcam_buffers_t p_buffers_output;
    test_util_buffer_t *qcarcam_mmap_buffer;
    qcarcam_frame_info_t frame_info;

    static void evs_ais_event_cb(qcarcam_hndl_t hndl, qcarcam_event_t event_id, qcarcam_event_payload_t *p_payload);
    gQcarCamClientData *pCurrentRecord;
    int getStrideMultiplayer(uint32_t mFormat);

};

} // namespace implementation
} // namespace V1_0
} // namespace evs
} // namespace automotive
} // namespace hardware
} // namespace android

#endif  // EVSAISCAMERA_H
