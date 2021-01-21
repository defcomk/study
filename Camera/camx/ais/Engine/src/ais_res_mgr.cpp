/*!
 * Copyright (c) 2016-2018 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#include <string.h>
#include <errno.h>
#include "ais_res_mgr.h"
#include "ais_engine.h"
#include "ais_input_configurer.h"
#include "ais_ife_configurer.h"
#include "ais_buffer_manager.h"

typedef struct
{
    boolean in_use;

    IfeOutputPathType output;

    AisUsrCtxt* user;
} ResMgrIfeIntfType;

typedef struct
{
    boolean in_use;

    CsiphyCoreType csi_phy;

    ResMgrIfeIntfType interface[IFE_INTF_MAX];

} ResMgrIfeResourceType;

class AisResourceManagerPrivate : public AisResourceManager
{
public:
    AisResourceManagerPrivate();

    virtual CameraResult Init(void);
    virtual CameraResult Deinit(void);

    virtual CameraResult Reserve(AisUsrCtxt* pUsrCtxt);
    virtual CameraResult Release(AisUsrCtxt* pUsrCtxt);

private:
    void ResetResourceTable();
    IfeOutputPathType GetIfeOutputFromInterface(IfeInterfaceType ifeInterf);

    ResMgrIfeResourceType m_resources[IFE_CORE_MAX];
};


///@brief AisEngine singleton
AisResourceManager* AisResourceManager::m_pResourceManagerInstance = nullptr;

AisResourceManager* AisResourceManager::GetInstance()
{
    if(m_pResourceManagerInstance == nullptr)
    {
        m_pResourceManagerInstance = new AisResourceManagerPrivate();
    }

    return m_pResourceManagerInstance;
}

void AisResourceManager::DestroyInstance()
{
    if(m_pResourceManagerInstance != nullptr)
    {
        delete m_pResourceManagerInstance;
        m_pResourceManagerInstance = nullptr;
    }
}

AisResourceManagerPrivate::AisResourceManagerPrivate()
{
    ResetResourceTable();
}

void AisResourceManagerPrivate::ResetResourceTable()
{
    uint32 i;

    memset(m_resources, 0x0, sizeof(m_resources));

    for(i=0; i<IFE_CORE_MAX;i++)
    {
        uint32 j;

        //TODO: for now it is a 1 to 1 mapping csiphy to ife
        m_resources[i].csi_phy = (CsiphyCoreType)i;

        for (j=0; j<IFE_INTF_MAX; j++)
        {
            m_resources[i].interface[j].output = GetIfeOutputFromInterface((IfeInterfaceType)j);
        }
    }
}

CameraResult AisResourceManagerPrivate::Init()
{
    ResetResourceTable();

    return CAMERA_SUCCESS;
}

CameraResult AisResourceManagerPrivate::Deinit()
{
    DestroyInstance();

    return CAMERA_SUCCESS;
}

CameraResult AisResourceManagerPrivate::Reserve(AisUsrCtxt* pUsrCtxt)
{
    CameraResult rc = CAMERA_SUCCESS;
    ais_input_interface_t input_interface;
    uint32 ife_interface;
    uint32 ife_core;
    boolean b_found = FALSE;

    //TODO: mutex protect?

    input_interface.input_id = pUsrCtxt->m_inputId;
    rc = AisInputConfigurer::GetInstance()->GetInterface(&input_interface);
    if (CAMERA_SUCCESS != rc)
    {
        CAM_MSG(ERROR, "input %d not supported", pUsrCtxt->m_inputId);
        return CAMERA_ERESOURCENOTFOUND;
    }

    for(ife_core = 0; ife_core < IFE_CORE_MAX; ife_core++)
    {
        if (m_resources[ife_core].csi_phy != input_interface.csiphy)
        {
            continue;
        }

        for (ife_interface = 0; ife_interface < IFE_INTF_MAX; ife_interface++)
        {
            if (!m_resources[ife_core].interface[ife_interface].in_use &&
                m_resources[ife_core].interface[ife_interface].output != IFE_OUTPUT_PATH_MAX)
            {
                m_resources[ife_core].interface[ife_interface].in_use = TRUE;
                m_resources[ife_core].interface[ife_interface].user = pUsrCtxt;
                b_found = TRUE;
                break;
            }
        }

        if (b_found)
        {
            break;
        }
    }

    if (b_found)
    {
        uint32 buffer_list_idx;
        uint32 ife_path_index = pUsrCtxt->m_resources.n_user_paths;
        ais_ife_user_path_info* ife_path_info = &pUsrCtxt->m_resources.ife_user_path_info[ife_path_index];

        buffer_list_idx = AIS_USER_OUTPUT_IDX;

        ife_path_info->ife_core = (IfeCoreType)ife_core;
        ife_path_info->ife_interf = (IfeInterfaceType)ife_interface;
        ife_path_info->ife_output = m_resources[ife_core].interface[ife_interface].output;
        ife_path_info->buffer_list_idx = buffer_list_idx;

        pUsrCtxt->m_resources.cid = input_interface.cid;
        pUsrCtxt->m_resources.csid = input_interface.csid;
        pUsrCtxt->m_resources.csiphy = input_interface.csiphy;
        pUsrCtxt->m_resources.n_user_paths++;
        CAM_MSG(HIGH,  "ife core %d interf %d out %d - dev %d src %d - cid %d csi 0x%x",
            ife_path_info->ife_core, ife_path_info->ife_interf, ife_path_info->ife_output,
            pUsrCtxt->m_inputCfg.devId, pUsrCtxt->m_inputCfg.srcId,
            pUsrCtxt->m_resources.cid, pUsrCtxt->m_resources.csid);
    }
    else
    {
        CAM_MSG(ERROR, "No IFE resource available");
        rc = CAMERA_ERESOURCENOTFOUND;
    }


    return rc;
}

CameraResult AisResourceManagerPrivate::Release(AisUsrCtxt* pUsrCtxt)
{
    CameraResult ret = CAMERA_SUCCESS;
    uint32 ife_interface;
    uint32 ife_core;
    boolean b_found = FALSE;

    //@todo: mutex protect?

    for(ife_core = 0; ife_core < IFE_CORE_MAX && !b_found; ife_core++)
    {
        for (ife_interface = 0; ife_interface < IFE_INTF_MAX; ife_interface++)
        {
            if (m_resources[ife_core].interface[ife_interface].in_use &&
                m_resources[ife_core].interface[ife_interface].user == pUsrCtxt)
            {
                m_resources[ife_core].interface[ife_interface].user = NULL;
                m_resources[ife_core].interface[ife_interface].in_use = FALSE;
                b_found = TRUE;
                break;
            }
        }
    }

    if (b_found)
    {
        uint32 i;

        for (i = 0; i < pUsrCtxt->m_resources.n_user_paths; i++)
        {
            /*cleanup user context resources*/
            if (pUsrCtxt->m_resources.ife_user_path_info[i].buffer_list_idx < AIS_BUFFER_LIST_INTERNAL_START)
            {
                continue;
            }

            AisBufferManager::FreeBufferList(pUsrCtxt, pUsrCtxt->m_resources.ife_user_path_info[i].buffer_list_idx);
        }

        memset(&pUsrCtxt->m_resources, 0x0, sizeof(pUsrCtxt->m_resources));
    }
    else
    {
        CAM_MSG(ERROR, "Did not find input resource to free");
        ret = CAMERA_ERESOURCENOTFOUND;
    }

    return ret;
}

IfeOutputPathType AisResourceManagerPrivate::GetIfeOutputFromInterface(IfeInterfaceType ifeInterf)
{
    IfeOutputPathType eOutput = IFE_OUTPUT_PATH_MAX;

    switch(ifeInterf)
    {
    case IFE_INTF_RDI0:
        eOutput = IFE_OUTPUT_PATH_RDI0;
        break;
    case IFE_INTF_RDI1:
        eOutput = IFE_OUTPUT_PATH_RDI1;
        break;
    case IFE_INTF_RDI2:
        eOutput = IFE_OUTPUT_PATH_RDI2;
        break;
    case IFE_INTF_RDI3:
        eOutput = IFE_OUTPUT_PATH_RDI3;
        break;
    default:
        eOutput = IFE_OUTPUT_PATH_MAX;
        break;
    }

    return eOutput;
}
