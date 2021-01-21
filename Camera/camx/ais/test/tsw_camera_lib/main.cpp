/* ************************************************************************
* Copyright (c) Continental AG and subsidiaries
* All rights reserved
*
* The reproduction, transmission or use of this document or its contents is
* not permitted without express written authority.
* Offenders will be liable for damages. All rights, including rights created
* by patent grant or registration of a utility model or design, are reserved.
* ************************************************************************/

/**
@file     ChimeService.cpp
@brief    This file is a template for a header file including Doxygen tags for inline documentation
@par      Project: DIDI CHJ MY20
@par      SW Component: ChimeService
@par      SW Package: service
@author   My Name
@par      Configuration Management:
@todo     list of things to do
*/

#define LOG_TAG "Cameradv"
/**** INCLUDES ***********************************************************/
#include <android/log.h>
#include <android/hardware/automotive/vehicle/2.0/IVehicle.h>
#include <hidl/HidlTransportSupport.h>
#include <iostream>

#include <iostream>
#include "CameraTypeDef.h"
// #include <utils/SystemClock.h>
#include <stdio.h>
// #include <utils/SystemClock.h>
#include "QCarCamLogic.h"

using namespace android;
using namespace android::hardware;
// using namespace device::conti::d01::chime::V1_0;

int main(int /* argc */, char* /* argv */ []) {






    while(1)
    {
        QCarCamLogic carcam;

        carcam.init();

        carcam.openCamera(QCARCAM_INPUT_TYPE_EXT_REAR);

        carcam.startCamera();

        sleep(5);

        carcam.stopCamera();

        carcam.closeCamera();

        carcam.deinit();

    }
#if 0
    auto service = std::make_unique<VehicleSignalReceiver>();

    ALOGI("camera dv starting");
    //subscribe chime property from vehiclehal
    std::vector<SubscribeOptions> subCameraOptions = {
         /*S level*/
        { .propId = toInt(VehicleProperty::CONTI_DV_TEST_COMMAND),
              .sampleRate = 0.5f,
              .flags = SubscribeFlags::EVENTS_FROM_CAR },
    };

    service->subscribeCameraPropertys(subCameraOptions);
    while(service->isRun()) {
         std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
#endif

#if 0
     // sleep(2);
    CameraControl* pCameraControl = new CameraControl();
    pCameraControl->initCamera();

    for(int i=0 ; i<10; i++)
    {

        // sleep(1);

        // joinRpcThreadpool();
        pCameraControl->startCamera();
        sleep(5);
        pCameraControl->closeCamera();
        // sleep(2);



    }
    pCameraControl->deinitCamera();

    delete pCameraControl;

#endif
    ALOGI("Exit");
    printf("exit\n");
    return 1;
}
