/* ===========================================================================
 * Copyright (c) 2017-2019 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
=========================================================================== */
#ifdef USE_STANDALONE_AIS
#define PUBLIC_API

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_util.h"

#include "ais.h"

qcarcam_ret_t test_util_standalone_init(test_util_ctxt_t *ctxt)
{
    ais_initialize(NULL);

    return QCARCAM_RET_OK;
}

qcarcam_ret_t test_util_standalone_deinit(test_util_ctxt_t *ctxt)
{
    ais_uninitialize();
    return QCARCAM_RET_OK;
}

static qcarcam_ret_t camera_to_qcarcam_result(CameraResult result)
{
    switch (result)
    {
    case CAMERA_SUCCESS             : return QCARCAM_RET_OK         ; break;
    case CAMERA_EFAILED             : return QCARCAM_RET_FAILED     ; break;
    case CAMERA_ENOMEMORY           : return QCARCAM_RET_NOMEM      ; break;
    case CAMERA_ECLASSNOTSUPPORT    : return QCARCAM_RET_FAILED     ; break;
    case CAMERA_EVERSIONNOTSUPPORT  : return QCARCAM_RET_UNSUPPORTED; break;
    case CAMERA_EALREADYLOADED      : return QCARCAM_RET_BADSTATE   ; break;
    case CAMERA_EUNABLETOLOAD       : return QCARCAM_RET_FAILED     ; break;
    case CAMERA_EUNABLETOUNLOAD     : return QCARCAM_RET_FAILED     ; break;
    case CAMERA_EALARMPENDING       : return QCARCAM_RET_BADSTATE   ; break;
    case CAMERA_EINVALIDTIME        : return QCARCAM_RET_BADPARAM   ; break;
    case CAMERA_EBADCLASS           : return QCARCAM_RET_BADSTATE   ; break;
    case CAMERA_EBADMETRIC          : return QCARCAM_RET_BADPARAM   ; break;
    case CAMERA_EEXPIRED            : return QCARCAM_RET_TIMEOUT    ; break;
    case CAMERA_EBADSTATE           : return QCARCAM_RET_BADSTATE   ; break;
    case CAMERA_EBADPARM            : return QCARCAM_RET_BADPARAM   ; break;
    case CAMERA_ESCHEMENOTSUPPORTED : return QCARCAM_RET_UNSUPPORTED; break;
    case CAMERA_EBADITEM            : return QCARCAM_RET_BADPARAM   ; break;
    case CAMERA_EINVALIDFORMAT      : return QCARCAM_RET_BADPARAM   ; break;
    case CAMERA_EINCOMPLETEITEM     : return QCARCAM_RET_BADPARAM   ; break;
    case CAMERA_ENOPERSISTMEMORY    : return QCARCAM_RET_NOMEM      ; break;
    case CAMERA_EUNSUPPORTED        : return QCARCAM_RET_UNSUPPORTED; break;
    case CAMERA_EPRIVLEVEL          : return QCARCAM_RET_BADSTATE   ; break;
    case CAMERA_ERESOURCENOTFOUND   : return QCARCAM_RET_FAILED     ; break;
    case CAMERA_EREENTERED          : return QCARCAM_RET_FAILED     ; break;
    case CAMERA_EBADTASK            : return QCARCAM_RET_BADPARAM   ; break;
    case CAMERA_EALLOCATED          : return QCARCAM_RET_BADSTATE   ; break;
    case CAMERA_EALREADY            : return QCARCAM_RET_BADSTATE   ; break;
    case CAMERA_EADSAUTHBAD         : return QCARCAM_RET_FAILED     ; break;
    case CAMERA_ENEEDSERVICEPROG    : return QCARCAM_RET_FAILED     ; break;
    case CAMERA_EMEMPTR             : return QCARCAM_RET_BADPARAM   ; break;
    case CAMERA_EHEAP               : return QCARCAM_RET_BADSTATE   ; break;
    case CAMERA_EIDLE               : return QCARCAM_RET_BADSTATE   ; break;
    case CAMERA_EITEMBUSY           : return QCARCAM_RET_BADSTATE   ; break;
    case CAMERA_EBADSID             : return QCARCAM_RET_BADPARAM   ; break;
    case CAMERA_ENOTYPE             : return QCARCAM_RET_FAILED     ; break;
    case CAMERA_ENEEDMORE           : return QCARCAM_RET_FAILED     ; break;
    case CAMERA_EADSCAPS            : return QCARCAM_RET_FAILED     ; break;
    case CAMERA_EBADSHUTDOWN        : return QCARCAM_RET_FAILED     ; break;
    case CAMERA_EBUFFERTOOSMALL     : return QCARCAM_RET_BADPARAM   ; break;
    case CAMERA_ENOSUCH             : return QCARCAM_RET_FAILED     ; break;
    case CAMERA_EACKPENDING         : return QCARCAM_RET_BADSTATE   ; break;
    case CAMERA_ENOTOWNER           : return QCARCAM_RET_FAILED     ; break;
    case CAMERA_EINVALIDITEM        : return QCARCAM_RET_BADSTATE   ; break;
    case CAMERA_ENOTALLOWED         : return QCARCAM_RET_FAILED     ; break;
    case CAMERA_EBADHANDLE          : return QCARCAM_RET_BADPARAM   ; break;
    case CAMERA_EOUTOFHANDLES       : return QCARCAM_RET_NOMEM      ; break;
    case CAMERA_EINTERRUPTED        : return QCARCAM_RET_FAILED     ; break;
    case CAMERA_ENOMORE             : return QCARCAM_RET_NOMEM      ; break;
    case CAMERA_ECPUEXCEPTION       : return QCARCAM_RET_FAILED     ; break;
    case CAMERA_EREADONLY           : return QCARCAM_RET_FAILED     ; break;
    case CAMERA_EWOULDBLOCK         : return QCARCAM_RET_FAILED     ; break;
    case CAMERA_EOUTOFBOUND         : return QCARCAM_RET_FAILED     ; break;
    case CAMERA_EDUALISPCONFIGFAILED: return QCARCAM_RET_FAILED     ; break;
    default                         : return QCARCAM_RET_FAILED     ; break;
    }
}

PUBLIC_API qcarcam_ret_t qcarcam_query_inputs(qcarcam_input_t* p_inputs, unsigned int size, unsigned int* ret_size)
{
    return camera_to_qcarcam_result(ais_query_inputs(p_inputs, size, ret_size));
}

PUBLIC_API qcarcam_hndl_t qcarcam_open(qcarcam_input_desc_t desc)
{
    return ais_open(desc);
}

PUBLIC_API qcarcam_ret_t qcarcam_close(qcarcam_hndl_t hndl)
{
    return camera_to_qcarcam_result(ais_close(hndl));
}

PUBLIC_API qcarcam_ret_t qcarcam_g_param(qcarcam_hndl_t hndl, qcarcam_param_t param, qcarcam_param_value_t* p_value)
{
    return camera_to_qcarcam_result(ais_g_param(hndl, param, p_value));
}

PUBLIC_API qcarcam_ret_t qcarcam_s_param(qcarcam_hndl_t hndl, qcarcam_param_t param, const qcarcam_param_value_t* p_value)
{
    return camera_to_qcarcam_result(ais_s_param(hndl, param, p_value));
}

PUBLIC_API qcarcam_ret_t qcarcam_s_buffers(qcarcam_hndl_t hndl, qcarcam_buffers_t* p_buffers)
{
    return camera_to_qcarcam_result(ais_s_buffers(hndl, p_buffers));
}

PUBLIC_API qcarcam_ret_t qcarcam_start(qcarcam_hndl_t hndl)
{
    return camera_to_qcarcam_result(ais_start(hndl));
}

PUBLIC_API qcarcam_ret_t qcarcam_stop(qcarcam_hndl_t hndl)
{
    return camera_to_qcarcam_result(ais_stop(hndl));
}

PUBLIC_API qcarcam_ret_t qcarcam_pause(qcarcam_hndl_t hndl)
{
    return camera_to_qcarcam_result(ais_pause(hndl));
}

PUBLIC_API qcarcam_ret_t qcarcam_resume(qcarcam_hndl_t hndl)
{
    return camera_to_qcarcam_result(ais_resume(hndl));
}

PUBLIC_API qcarcam_ret_t qcarcam_get_frame(qcarcam_hndl_t hndl, qcarcam_frame_info_t* p_frame_info,
        unsigned long long int timeout, unsigned int flags)
{
    return camera_to_qcarcam_result(ais_get_frame(hndl, p_frame_info, timeout, flags));
}

PUBLIC_API qcarcam_ret_t qcarcam_release_frame(qcarcam_hndl_t hndl, unsigned int idx)
{
    return camera_to_qcarcam_result(ais_release_frame(hndl, idx));
}
#endif
