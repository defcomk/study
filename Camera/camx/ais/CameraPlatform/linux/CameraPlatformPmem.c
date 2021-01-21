/**
 * @file CameraPlatformPmem.c
 *
 * @brief Implementation of Camera PMEM methods
 *
 * Copyright (c) 2009-2012, 2014-2017 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 */

/* ===========================================================================
                        INCLUDE FILES FOR MODULE
=========================================================================== */
#include "CameraPlatform.h"

/* ===========================================================================
                DEFINITIONS AND DECLARATIONS FOR MODULE
=========================================================================== */

/* --------------------------------------------------------------------------
** Constant / Define Declarations
** ----------------------------------------------------------------------- */
/* --------------------------------------------------------------------------
** Type Declarations
** ----------------------------------------------------------------------- */

/* --------------------------------------------------------------------------
** Local Object Definitions
** ----------------------------------------------------------------------- */

/* --------------------------------------------------------------------------
** Global Object Definitions
** ----------------------------------------------------------------------- */

/* --------------------------------------------------------------------------
** Forward Declarations
** ----------------------------------------------------------------------- */

/* ===========================================================================
**                          Macro Definitions
** ======================================================================== */

/* ===========================================================================
**                          Internal Definitions
** ======================================================================== */

/* ===========================================================================
**                           Definitions
** ======================================================================== */
/**
 * This function initializates CameraPMEM API and should be called
 * before any other CameraPMEM API call is made.
 *
 * @return TRUE - if successful
 *         FALSE - if failed
 */
boolean CameraPMEMInit(void)
{
    return TRUE;
}

/**
 * This function deinitializes CameraPMEM API and should only be called
 * after all CameraPMEM have been freed.
 *
 * @return TRUE - if successful
 *         FALSE - if failed
 */
boolean CameraPMEMDeinit(void)
{
    return TRUE;
}


/**
 * This function allocates a block of size bytes from the heap that matches
 * the requested flags/attributes.
 *
 * @param[in] numBytes The size of the buffer to be allocated in bytes
 *
 * @return A pointer to the newly allocated block, or NULL if the block
 *         could not be allocated. The pointer is 1k aligned.
 */
void* CameraPMEMMalloc (uint32 numBytes)
{
    return CameraAllocate(0, numBytes);
}

/**
 * This function deallocates a block of memory and returns it to the
 * correct heap.
 *
 * @param[in] buffer The pointer to the memory to be freed
 *
 * @return None
 */
void CameraPMEMFree (void* buffer)
{
    CameraFree(0, buffer);
}

/**
 * This function allocates a block of size bytes from reserved heap that matches
 * the requested flags/attributes.
 *
 *@param [in] size  Size of memory chunk to be allocated, Will be rounded up to 4K size
 *@param [in] flags flags on the type of allocation.

 * @return A pointer to the newly allocated block, or NULL if the block
 *         could not be allocated.
 */
void* CameraPMEMMallocExt(uint32 numBytes, uint32 flags, uint32 vmidMask,
    void** pMemHandle, void* rsv)
{
    return CameraPMEMMalloc(numBytes);
}

/**
 * This function deallocates a block of memory and returns it to the
 * reserved heap.
 *
 * @param[in] buffer The pointer to the memory to be freed
 *
 * @return None
 */
void CameraPMEMFreeExt(void* buffer)
{
    CameraPMEMFree(buffer);
}

/**
 * This function translates the virtual address of the PMEM-allocated buffer
 * into a physical address.
 *
 * @param[in] buffer The virtual address to be translated
 *
 * @return The physical address corresponding to the virtual address
 *         passed in.
 */
void* CameraPMEMGetPhysicalAddress (void* buffer)
{
    return buffer;
}

