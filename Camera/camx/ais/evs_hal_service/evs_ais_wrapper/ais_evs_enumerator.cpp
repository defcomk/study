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

#include "ais_evs_enumerator.h"
#include "ais_evs_camera.h"
#include "ais_evs_gldisplay.h"
#include "qcarcam.h"
#include "qcarcam_types.h"

#include <dirent.h>


namespace android {
namespace hardware {
namespace automotive {
namespace evs {
namespace V1_0 {
namespace implementation {


// NOTE:  All members values are static so that all clients operate on the same state
//        That is to say, this is effectively a singleton despite the fact that HIDL
//        constructs a new instance for each client.
std::list<EvsAISEnumerator::CameraRecord>   EvsAISEnumerator::sCameraList;
wp<EvsGlDisplay>                           EvsAISEnumerator::sActiveDisplay;


EvsAISEnumerator::EvsAISEnumerator() {
    ALOGD("EvsAISEnumerator created");

    unsigned captureCount = 0;

    /*query qcarcam*/
    qcarcam_input_t *pInputs;
    char camera_name[20];
    unsigned int queryNumInputs = 0, queryFilled = 0;
    int i, ret = 0;

    qcarcam_init_t qcarcam_init;
    qcarcam_init.version = QCARCAM_VERSION;

    // Initialize qcarcam
    ret = qcarcam_initialize(&qcarcam_init);
    if (ret == QCARCAM_RET_OK) {
        mQcarcaInitilized = true;
        ret = qcarcam_query_inputs(NULL, 0, &queryNumInputs);
        if (QCARCAM_RET_OK != ret || queryNumInputs == 0) {
            ALOGE("Failed qcarcam_query_inputs number of inputs with ret %d", ret);
        } else {
            pInputs = (qcarcam_input_t *)calloc(queryNumInputs, sizeof(*pInputs));
            if (pInputs) {

                ret = qcarcam_query_inputs(pInputs, queryNumInputs, &queryFilled);
                if (QCARCAM_RET_OK != ret || queryFilled != queryNumInputs) {
                    ALOGE("Failed qcarcam_query_inputs with ret %d", ret);
                }

                ALOGE("--- QCarCam Queried Inputs ----");
                for (i = 0; i < (int)queryFilled; i++)
                {
                    ALOGE("%d: input_id=%d, res=%dx%d fmt=0x%08x", i, pInputs[i].desc,
                            pInputs[i].res[0].width, pInputs[i].res[0].height,
                            pInputs[i].color_fmt[0]);
                    sprintf(camera_name, "%d",pInputs[i].desc);
                    sCameraList.emplace_back(camera_name);
                    captureCount++;
                }
            }
        }
        ALOGE("Found %d qualified video capture devices found\n", captureCount);
    } else {
        ALOGE("qcarcam_initialize failed %d", ret);
        mQcarcaInitilized = false;
    }
}


// Methods from ::android::hardware::automotive::evs::V1_0::IEvsEnumerator follow.
Return<void> EvsAISEnumerator::getCameraList(getCameraList_cb _hidl_cb)  {
    ALOGD("getCameraList");

    const unsigned numCameras = sCameraList.size();

    // Build up a packed array of CameraDesc for return
    hidl_vec<CameraDesc> hidlCameras;
    hidlCameras.resize(numCameras);
    unsigned i = 0;
    for (const auto& cam : sCameraList) {
        hidlCameras[i++] = cam.desc;
    }

    // Send back the results
    ALOGD("reporting %zu cameras available", hidlCameras.size());
    _hidl_cb(hidlCameras);

    // HIDL convention says we return Void if we sent our result back via callback
    return Void();
}


Return<sp<IEvsCamera>> EvsAISEnumerator::openCamera(const hidl_string& cameraId) {
    ALOGD("openCamera");

    // Is this a recognized camera id?
    CameraRecord *pRecord = findCameraById(cameraId);
    if (!pRecord) {
        ALOGE("Requested camera %s not found", cameraId.c_str());
        return nullptr;
    }

    // Has this camera already been instantiated by another caller?
    sp<EvsAISCamera> pActiveCamera = pRecord->activeInstance.promote();
    if (pActiveCamera != nullptr) {
        ALOGW("Killing previous camera because of new caller");
        closeCamera(pActiveCamera);
    }

    // Construct a camera instance for the caller
    pActiveCamera = new EvsAISCamera(cameraId.c_str());
    pRecord->activeInstance = pActiveCamera;
    if (pActiveCamera == nullptr) {
        ALOGE("Failed to allocate new EvsAISCamera object for %s\n", cameraId.c_str());
    }

    return pActiveCamera;
}


Return<void> EvsAISEnumerator::closeCamera(const ::android::sp<IEvsCamera>& pCamera) {
    ALOGD("closeCamera");

    if (pCamera == nullptr) {
        ALOGE("Ignoring call to closeCamera with null camera ptr");
        return Void();
    }

    // Get the camera id so we can find it in our list
    std::string cameraId;
    pCamera->getCameraInfo([&cameraId](CameraDesc desc) {
                               cameraId = desc.cameraId;
                           }
    );

    // Find the named camera
    CameraRecord *pRecord = findCameraById(cameraId);

    // Is the display being destroyed actually the one we think is active?
    if (!pRecord) {
        ALOGE("Asked to close a camera whose name isn't recognized");
    } else {
        sp<EvsAISCamera> pActiveCamera = pRecord->activeInstance.promote();

        if (pActiveCamera == nullptr) {
            ALOGE("Somehow a camera is being destroyed when the enumerator didn't know one existed");
        } else if (pActiveCamera != pCamera) {
            // This can happen if the camera was aggressively reopened, orphaning this previous instance
            ALOGW("Ignoring close of previously orphaned camera - why did a client steal?");
        } else {
            // Drop the active camera
            pActiveCamera->shutdown();
            pRecord->activeInstance = nullptr;
        }
    }

    return Void();
}


Return<sp<IEvsDisplay>> EvsAISEnumerator::openDisplay() {
    ALOGD("openDisplay");

    // If we already have a display active, then we need to shut it down so we can
    // give exclusive access to the new caller.
    sp<EvsGlDisplay> pActiveDisplay = sActiveDisplay.promote();
    if (pActiveDisplay != nullptr) {
        ALOGW("Killing previous display because of new caller");
        closeDisplay(pActiveDisplay);
    }

    // Create a new display interface and return it
    pActiveDisplay = new EvsGlDisplay();
    sActiveDisplay = pActiveDisplay;

    ALOGD("Returning new EvsGlDisplay object %p", pActiveDisplay.get());
    return pActiveDisplay;
}


Return<void> EvsAISEnumerator::closeDisplay(const ::android::sp<IEvsDisplay>& pDisplay) {
    ALOGD("closeDisplay");

    // Do we still have a display object we think should be active?
    sp<EvsGlDisplay> pActiveDisplay = sActiveDisplay.promote();
    if (pActiveDisplay == nullptr) {
        ALOGE("Somehow a display is being destroyed when the enumerator didn't know one existed");
    } else if (sActiveDisplay != pDisplay) {
        ALOGW("Ignoring close of previously orphaned display - why did a client steal?");
    } else {
        // Drop the active display
        pActiveDisplay->forceShutdown();
        sActiveDisplay = nullptr;
    }

    return Void();
}


Return<DisplayState> EvsAISEnumerator::getDisplayState()  {
    ALOGD("getDisplayState");

    // Do we still have a display object we think should be active?
    sp<IEvsDisplay> pActiveDisplay = sActiveDisplay.promote();
    if (pActiveDisplay != nullptr) {
        return pActiveDisplay->getDisplayState();
    } else {
        return DisplayState::NOT_OPEN;
    }
}

EvsAISEnumerator::CameraRecord* EvsAISEnumerator::findCameraById(const std::string& cameraId) {
    // Find the named camera
    for (auto &&cam : sCameraList) {
        if (cam.desc.cameraId == cameraId) {
            // Found a match!
            return &cam;
        }
        ALOGE("EVS::cam.desc.cameraId.c_str() = %s\n",cam.desc.cameraId.c_str());
    }

    // We didn't find a match
    return nullptr;
}

EvsAISEnumerator::~EvsAISEnumerator() {
    ALOGD("EvsAISEnumerator destructor");
    qcarcam_uninitialize();
    mQcarcaInitilized = false;
}


} // namespace implementation
} // namespace V1_0
} // namespace evs
} // namespace automotive
} // namespace hardware
} // namespace android
