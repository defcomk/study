////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxmetabuffer.cpp
///
/// @brief Meta Buffer implementation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <queue>
#include "camxatomic.h"
#include "camxhal3metadatautil.h"
#include "camxhwenvironment.h"
#include "camxmetabuffer.h"
#include "camxutils.h"

CAMX_NAMESPACE_BEGIN

// NOWHINE FILE CP006: used standard libraries for performance improvements
// NOWHINE FILE CP010: used references for the parameters which can never be NULL
// NOWHINE FILE PR007b: Whiner incorrectly concludes as non-library files

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespaces
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constant Definitions
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static const UINT32 NumDefaultMemoryRegions     = 30;          ///< Default number of regions to reserve
static const UINT32 NumDefaultMetaBufferLinks   = 10;          ///< Default number of metadata dependency links
static const UINT32 NumDefaultMetaBufferClients = 30;          ///< Default number of clients
static const INT32  MetadataUtilOK              = 0;           ///< Success status of android metadata APIs
static const INT32  MetadataUtilError           = 1;           ///< Error status of android metadata APIs
static const UINT32 NumDefaultTagCount          = 30;          ///< Default number of tags to reserve
static const UINT32 ReservedIndex               = 0xFFFFFFFD;  ///< Index to indicate whether the region is reserved
static const UINT32 InvalidClient               = 0xFFFFFFFF;  ///< Invalid client identifier
static const UINT32 InvalidTag                  = 0xFFFFFFFF;  ///< Invalid tag index
static const UINT32 AlignmentFactor             = 8;           ///< Memory Alignment factor for the tags
static const UINT32 MetaBufferClientMask        = 0x80000000;  ///< Mask to indicate the client is another metabuffer
static const UINT32 InPlaceMemoryRegion         = 0x8000000D;  ///< Index to indicate whether the region is reserved
static const UINT32 MaxTagSize                  = 0xFFFF;      ///< Maximum Tag size
static const UINT32 MetaBufferUsageMask         = 0xD0000000;  ///< Mask to identify the client
static const UINT32 InvalidCameraId             = 0xFFFFFFFF;  ///< Invalid camera identifier
static const UINT32 MaxGraphDepth               = 10;          ///< Maximum depth of metadata graph


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Static Definitions
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
UINT32 MetaBuffer::s_metaBufferIdentifier = 0x28913080; ///< Unique identified for detecting invalid metadata handle

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MetaBuffer::MemoryRegion Methods
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MetaBuffer::MemoryRegion::MemoryRegion
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
MetaBuffer::MemoryRegion::MemoryRegion()
    : m_pVaddr(NULL)
    , m_size(0)
    , m_fd(-1)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MetaBuffer::MemoryRegion::Release
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID MetaBuffer::MemoryRegion::Release()
{
    if (NULL != m_pVaddr)
    {
        CAMX_ASSERT(0 < m_size);
        CAMX_DELETE[] m_pVaddr;
        m_pVaddr = NULL;
        m_size   = 0;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MetaBuffer::MemoryRegion::Allocate
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult MetaBuffer::MemoryRegion::Allocate(
    UINT32 regionSize)
{
    CamxResult result = CamxResultSuccess;

    CAMX_ASSERT(0 < regionSize);
    CAMX_ASSERT(NULL == m_pVaddr);

    if (0 < regionSize)
    {
        m_pVaddr = CAMX_NEW BYTE[regionSize];
        if (NULL != m_pVaddr)
        {
            m_size = regionSize;
        }
        else
        {
            result = CamxResultENoMemory;
        }
    }
    else
    {
        result = CamxResultEInvalidArg;
    }

    if (CamxResultSuccess != result)
    {
        CAMX_LOG_ERROR(CamxLogGroupMeta, "Allocation failed %p or invalid size %u", m_pVaddr, regionSize);
    }
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MetaBuffer::Content Methods
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MetaBuffer::Content::Content
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
MetaBuffer::Content::Content()
    : m_pVaddr(NULL)
    , m_count(0)
    , m_regionIndex(InvalidIndex)
    , m_offset(0)
    , m_pParentMetaBuffer(NULL)
    , m_tag(InvalidTag)
    , m_tagIndex(InvalidIndex)
    , m_maxSize(MaxTagSize)
    , m_cameraId(InvalidCameraId)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MetaBuffer::Content::Content
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
MetaBuffer::Content::Content(
    UINT32 argRegionIndex,
    UINT32 argOffset,
    UINT32 tag,
    UINT32 tagIndex)
    : m_pVaddr(0)
    , m_count(0)
    , m_regionIndex(argRegionIndex)
    , m_offset(argOffset)
    , m_size(0)
    , m_pParentMetaBuffer(NULL)
    , m_tag(tag)
    , m_tagIndex(tagIndex)
    , m_maxSize(MaxTagSize)
    , m_cameraId(InvalidCameraId)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MetaBuffer::Content::Content
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
MetaBuffer::Content::Content(
    BYTE*       pVaddr,
    UINT32      count,
    UINT32      regionIndex,
    UINT32      offset,
    UINT32      size,
    MetaBuffer* pMetaBuffer,
    UINT32      tag,
    UINT32      tagIndex,
    UINT32      maxSize)
    : m_pVaddr(pVaddr)
    , m_count(count)
    , m_regionIndex(regionIndex)
    , m_offset(offset)
    , m_size(size)
    , m_pParentMetaBuffer(pMetaBuffer)
    , m_tag(tag)
    , m_tagIndex(tagIndex)
    , m_maxSize(maxSize)
    , m_cameraId(InvalidCameraId)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MetaBuffer::Content::Reset
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID MetaBuffer::Content::Reset()
{
    m_pParentMetaBuffer = NULL;
    m_pVaddr            = NULL;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MetaBuffer::Content::Assign
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID MetaBuffer::Content::Assign(
    Content& srcContent)
{
    if (MaxInplaceTagSize >= srcContent.m_maxSize)
    {
        Utils::Memcpy(m_data, srcContent.m_data, sizeof(m_data));

        m_pVaddr = m_data;
    }
    else
    {
        m_pVaddr = srcContent.m_pVaddr;
    }

    m_count    = srcContent.m_count;
    m_size     = srcContent.m_size;
    m_tag      = srcContent.m_tag;
    m_tagIndex = srcContent.m_tagIndex;
    m_maxSize  = srcContent.m_maxSize;
    m_cameraId = srcContent.m_cameraId;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MetaBuffer::Content::Copy
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID MetaBuffer::Content::Copy(
    const Content& srcContent)
{
    if (MaxInplaceTagSize >= srcContent.m_maxSize)
    {
        Utils::Memcpy(m_data, srcContent.m_data, sizeof(m_data));

        m_pVaddr      = m_data;
        m_regionIndex = InPlaceMemoryRegion;
    }
    else
    {
        Utils::Memcpy(m_pVaddr, srcContent.m_pVaddr, srcContent.m_size);
    }

    m_size     = srcContent.m_size;
    m_count    = srcContent.m_count;
    m_tag      = srcContent.m_tag;
    m_tagIndex = srcContent.m_tagIndex;
    m_maxSize  = srcContent.m_maxSize;
    m_cameraId = srcContent.m_cameraId;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MetaBuffer::Link Methods
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MetaBuffer::Link::Link
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
MetaBuffer::Link::Link()
    : m_pMetaBuffer(NULL)
    , m_clientID(0)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MetaBuffer::Link::Link
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
MetaBuffer::Link::Link(
    MetaBuffer* pMetaBuffer)
    : m_pMetaBuffer(pMetaBuffer)
    , m_clientID(0)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MetaBuffer::Link::Link
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
MetaBuffer::Link::Link(
    MetaBuffer* pMetaBuffer,
    UINT32 clientID)
    : m_pMetaBuffer(pMetaBuffer)
    , m_clientID(clientID)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MetaBuffer::LinearMap Methods
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MetaBuffer::LinearMap::Create
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
MetaBuffer::LinearMap* MetaBuffer::LinearMap::Create(
    UINT32 maxTags)
{
    LinearMap* pMap = CAMX_NEW MetaBuffer::LinearMap;
    if (NULL != pMap)
    {
        pMap->m_maxMetadataTags      = maxTags;
        pMap->m_pMetadataOffsetTable = CAMX_NEW Content[maxTags];

        if (NULL != pMap->m_pMetadataOffsetTable)
        {
            for (UINT32 index = 0; index < maxTags; ++index)
            {
                const MetadataInfo* pInfo = HAL3MetadataUtil::GetMetadataInfoByIndex(index);

                if (NULL != pInfo)
                {
                    Content& content   = pMap->m_pMetadataOffsetTable[index];
                    content.m_maxSize  = pInfo->size;
                    content.m_tagIndex = index;

                    if (MaxInplaceTagSize >= content.m_maxSize)
                    {
                        content.m_regionIndex = InPlaceMemoryRegion;
                    }
                }
                else
                {
                    CAMX_DELETE pMap;
                    pMap = NULL;
                    break;
                }
            }
        }
        else
        {
            CAMX_DELETE pMap;
            pMap = NULL;
        }
    }
    return pMap;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MetaBuffer::LinearMap::LinearMap
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
MetaBuffer::LinearMap::LinearMap()
    : m_pMetadataOffsetTable(NULL)
    , m_maxMetadataTags(0)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MetaBuffer::LinearMap::~LinearMap
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
MetaBuffer::LinearMap::~LinearMap()
{
    if (NULL != m_pMetadataOffsetTable)
    {
        CAMX_DELETE[] m_pMetadataOffsetTable;
        m_pMetadataOffsetTable = NULL;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MetaBuffer::LinearMap::Find
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
MetaBuffer::Content* MetaBuffer::LinearMap::Find(
    UINT32 tag)
{
    Content* pContent   = NULL;
    UINT32 tagIndex     = HAL3MetadataUtil::GetUniqueIndexByTag(tag);
    CAMX_ASSERT(m_maxMetadataTags > tagIndex);
    return &m_pMetadataOffsetTable[tagIndex];
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MetaBuffer::LinearMap::Reset
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID MetaBuffer::LinearMap::Reset()
{
    for (UINT32 tagIndex = 0; tagIndex < m_maxMetadataTags; ++tagIndex)
    {
        m_pMetadataOffsetTable[tagIndex].Reset();
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MetaBuffer::LinearMap::Insert
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID MetaBuffer::LinearMap::Insert(
    UINT32 tag,
    UINT32 regionIndex,
    UINT32 offset,
    UINT32 size,
    BYTE*  pAddress,
    UINT32 count,
    UINT32 tagIndex,
    UINT32 maxSize)
{
    CAMX_UNREFERENCED_PARAM(maxSize);

    CAMX_ASSERT(m_maxMetadataTags > tagIndex);

    Content& content = m_pMetadataOffsetTable[tagIndex];

    content.m_regionIndex = regionIndex;
    content.m_offset      = offset;
    content.m_pVaddr      = pAddress;
    content.m_count       = count;
    content.m_size        = size;
    content.m_tag         = tag;
    content.m_maxSize     = maxSize;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MetaBuffer::LinearMap::Count
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
UINT32 MetaBuffer::LinearMap::Count()
{
    UINT32 count = 0;
    for (UINT32 tagIndex = 0; tagIndex < m_maxMetadataTags; ++tagIndex)
    {
        if (m_pMetadataOffsetTable[tagIndex].IsValid())
        {
            count++;
        }
    }
    return count;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MetaBuffer::LinearMap::CopyValidAndReserveUnfilledTags
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID MetaBuffer::LinearMap::CopyValidAndReserveUnfilledTags(
    Map*                        pSrcMap,
    std::vector<MemoryRegion>&  memoryRegions,
    UINT32&                     totalSize,
    BOOL                        disjoint)
{
    LinearMap* pSrcLinearMap = static_cast<LinearMap*>(pSrcMap);

    CAMX_ASSERT(m_maxMetadataTags == pSrcLinearMap->m_maxMetadataTags);

    totalSize = 0;

    for (UINT32 tagIndex = 0; tagIndex < m_maxMetadataTags; ++tagIndex)
    {
        Content& dstContent = m_pMetadataOffsetTable[tagIndex];
        Content& srcContent = pSrcLinearMap->m_pMetadataOffsetTable[tagIndex];

        if ((TRUE == srcContent.IsValid()) &&
            ((FALSE == disjoint) || (FALSE == dstContent.IsValid())))
        {
            dstContent.m_count             = srcContent.m_count;
            dstContent.m_size              = srcContent.m_size;
            dstContent.m_tag               = srcContent.m_tag;
            dstContent.m_pParentMetaBuffer = NULL;

            if (MaxInplaceTagSize >= srcContent.m_maxSize)
            {
                Utils::Memcpy(dstContent.m_data, srcContent.m_data, sizeof(dstContent.m_data));

                dstContent.m_pVaddr = dstContent.m_data;
            }
            else if (dstContent.m_regionIndex < memoryRegions.size())
            {
                dstContent.m_pVaddr = memoryRegions[dstContent.m_regionIndex].m_pVaddr +
                    dstContent.m_offset;

                Utils::Memcpy(dstContent.m_pVaddr, srcContent.m_pVaddr, dstContent.m_size);
            }
            else
            {
                dstContent.m_offset = totalSize;

                // mark the regions
                dstContent.m_regionIndex       = ReservedIndex;
                dstContent.m_pVaddr            = srcContent.m_pVaddr;
                dstContent.m_pParentMetaBuffer = NULL;

                totalSize += CAMX_ALIGN_TO(srcContent.m_maxSize, AlignmentFactor);
            }
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MetaBuffer::LinearMap::SetUnfilledTagRegion
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID MetaBuffer::LinearMap::SetUnfilledTagRegion(
    BYTE*   pDstBaseAddress,
    UINT32  regionIndex)
{
    for (UINT32 tagIndex = 0; tagIndex < m_maxMetadataTags; ++tagIndex)
    {
        Content& content = m_pMetadataOffsetTable[tagIndex];

        if (ReservedIndex == content.m_regionIndex)
        {
            BYTE* pSrcAddress = content.m_pVaddr;
            BYTE* pDstAddress = pDstBaseAddress + content.m_offset;

            content.m_regionIndex = regionIndex;
            content.m_pVaddr      = pDstAddress;

            Utils::Memcpy(pDstAddress, pSrcAddress, content.m_size);
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MetaBuffer::LinearMap::Copy
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID MetaBuffer::LinearMap::Copy(
    Map*        pSrcMap,
    MetaBuffer* pSrcMetaBuffer)
{
    CAMX_UNREFERENCED_PARAM(pSrcMetaBuffer);

    LinearMap* pSrcLinearMap = static_cast<LinearMap*>(pSrcMap);

    for (UINT32 tagIndex = 0; tagIndex < m_maxMetadataTags; ++tagIndex)
    {
        Content& srcContent = pSrcLinearMap->m_pMetadataOffsetTable[tagIndex];
        Content& dstContent = m_pMetadataOffsetTable[tagIndex];

        if (TRUE == srcContent.IsValid())
        {
            dstContent.Copy(srcContent);

            dstContent.m_pParentMetaBuffer = NULL;
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MetaBuffer::LinearMap::Merge
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID MetaBuffer::LinearMap::Merge(
    Map*        pSrcMap,
    MetaBuffer* pSrcMetaBuffer)
{
    LinearMap* pSrcLinearMap = static_cast<LinearMap*>(pSrcMap);

    for (UINT32 tagIndex = 0; tagIndex < m_maxMetadataTags; ++tagIndex)
    {
        Content& srcContent = pSrcLinearMap->m_pMetadataOffsetTable[tagIndex];
        Content& dstContent = m_pMetadataOffsetTable[tagIndex];

        if (TRUE == srcContent.IsValid())
        {
            dstContent.Assign(srcContent);

            dstContent.m_pParentMetaBuffer = pSrcMetaBuffer;
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MetaBuffer::LinearMap::MergeDisjoint
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID MetaBuffer::LinearMap::MergeDisjoint(
    Map*        pSrcMap,
    MetaBuffer* pSrcMetaBuffer)
{
    LinearMap* pSrcLinearMap = static_cast<LinearMap*>(pSrcMap);

    for (UINT32 tagIndex = 0; tagIndex < m_maxMetadataTags; ++tagIndex)
    {
        Content& srcContent = pSrcLinearMap->m_pMetadataOffsetTable[tagIndex];
        Content& dstContent = m_pMetadataOffsetTable[tagIndex];

        if ((TRUE == srcContent.IsValid()) && (FALSE == dstContent.IsValid()))
        {
            dstContent.Assign(srcContent);

            dstContent.m_pParentMetaBuffer = pSrcMetaBuffer;
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MetaBuffer::LinearMap::SwitchAndMerge
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID MetaBuffer::LinearMap::SwitchAndMerge(
    Map*        pSrcMap,
    MetaBuffer* pSrcMetaBuffer,
    Map*        pMasterMap,
    UINT32      oldCameraId)
{
    LinearMap* pSrcLinearMap    = static_cast<LinearMap*>(pSrcMap);
    LinearMap* pMasterLinearMap = static_cast<LinearMap*>(pMasterMap);

    for (UINT32 tagIndex = 0; tagIndex < m_maxMetadataTags; ++tagIndex)
    {
        Content& srcContent       = pSrcLinearMap->m_pMetadataOffsetTable[tagIndex];
        Content& dstContent       = m_pMetadataOffsetTable[tagIndex];
        Content& masterSrcContent = pMasterLinearMap->m_pMetadataOffsetTable[tagIndex];

        if ((TRUE == srcContent.IsValid()) && (FALSE == dstContent.IsValid()))
        {
            if ((oldCameraId == srcContent.m_cameraId) && (TRUE == masterSrcContent.IsValid()))
            {
                dstContent.Assign(masterSrcContent);
            }

            dstContent.m_pParentMetaBuffer = pSrcMetaBuffer;
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MetaBuffer::LinearMap::GetAndroidMeta
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult MetaBuffer::LinearMap::GetAndroidMeta(
    camera_metadata_t*      pAndroidMeta,
    UINT32                  propertyBlobId,
    BOOL                    frameworkTagsOnly,
    BOOL                    filterProperties,
    unordered_set<UINT32>&  filterSet)
{
    CamxResult result   = CamxResultSuccess;
    UINT32     tagCount = m_maxMetadataTags - HAL3MetadataUtil::GetPropertyCount();

    for (UINT32 tagIndex = 0; tagIndex < tagCount; ++tagIndex)
    {
        Content& content = m_pMetadataOffsetTable[tagIndex];

        if (TRUE == content.IsValid())
        {
            const MetadataInfo* pInfo       = HAL3MetadataUtil::GetMetadataInfoByIndex(tagIndex);
            CamxResult          resultLocal = CamxResultSuccess;

            BOOL isTagPresent = (filterSet.find(content.m_tag) != filterSet.end());

            if ((FALSE  == isTagPresent)      &&
                ((FALSE == frameworkTagsOnly) ||
                 (0     != (TagSectionVisibility::TagSectionVisibleToFramework & pInfo->visibility))))
            {
                resultLocal = HAL3MetadataUtil::UpdateMetadata(
                    pAndroidMeta,
                    content.m_tag,
                    content.m_pVaddr,
                    content.m_count,
                    TRUE);
            }

            if (CamxResultSuccess != resultLocal)
            {
                result = resultLocal;

                CAMX_LOG_ERROR(CamxLogGroupMeta, "Update tag %x failed %d name %s addr %p count %u",
                               content.m_tag, result, pInfo->tagName,
                               content.m_pVaddr, content.m_count);
            }
        }
    }

    if (FALSE == filterProperties)
    {
        PropertyPackingInfo packingInfo = {};

        UINT32 propertyStartIndex = HAL3MetadataUtil::GetUniqueIndexByTag(PropertyIDPerFrameResultBegin);
        UINT32 propertyEndIndex   = HAL3MetadataUtil::GetUniqueIndexByTag(PropertyIDNodeComplete0);

        for (UINT32 tagIndex = propertyStartIndex; tagIndex < propertyEndIndex; ++tagIndex)
        {
            const MetadataInfo* pInfo   = HAL3MetadataUtil::GetMetadataInfoByIndex(tagIndex);
            Content&            content = m_pMetadataOffsetTable[tagIndex];

            if (TRUE == content.IsValid())
            {
                result = HAL3MetadataUtil::AppendPropertyPackingInfo(
                    content.m_tag,
                    content.m_pVaddr,
                    &packingInfo);
            }
        }


        if ((CamxResultSuccess == result) && (0 != packingInfo.count))
        {
            BYTE* pPropertyBlob = CAMX_NEW BYTE[PropertyBlobSize];
            if (NULL != pPropertyBlob)
            {
                HAL3MetadataUtil::PackPropertyInfoToBlob(&packingInfo, pPropertyBlob);

                CamxResult resultLocal = HAL3MetadataUtil::UpdateMetadata(
                    pAndroidMeta,
                    propertyBlobId,
                    pPropertyBlob,
                    PropertyBlobSize,
                    TRUE);

                if (CamxResultSuccess == resultLocal)
                {
                    CAMX_LOG_INFO(CamxLogGroupMeta, "Added %d properties to vendor tag", packingInfo.count);
                }

                CAMX_DELETE[] pPropertyBlob;
            }
            else
            {
                CAMX_LOG_ERROR(CamxLogGroupMeta, "Failed to allocate memory for pPropertyBlob");
                result = CamxResultENoMemory;
            }
        }
    }

    if (CamxResultSuccess != result)
    {
        Print(TRUE);

        result = CamxResultSuccess;
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MetaBuffer::LinearMap::Print
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID MetaBuffer::LinearMap::Print(
    BOOL    validTagsOnly)
{
    for (UINT32 tagIndex = 0; tagIndex < m_maxMetadataTags; ++tagIndex)
    {
        Content& content = m_pMetadataOffsetTable[tagIndex];

        if ((FALSE == validTagsOnly) || (TRUE == content.IsValid()))
        {
            const MetadataInfo* pInfo = HAL3MetadataUtil::GetMetadataInfoByIndex(tagIndex);

            CAMX_LOG_INFO(CamxLogGroupMeta, "Tag %x size %u offset %u addr %p region %u count %u parent %p tagName %s",
                content.m_tag,
                content.m_size,
                content.m_offset,
                content.m_pVaddr,
                content.m_regionIndex,
                content.m_count,
                content.m_pParentMetaBuffer,
                pInfo ? pInfo->tagName : "NULL");
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MetaBuffer::LinearMap::UpdateCameraId
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID MetaBuffer::LinearMap::UpdateCameraId(
    UINT32  cameraId)
{
    for (UINT32 tagIndex = 0; tagIndex < m_maxMetadataTags; ++tagIndex)
    {
        Content& content = m_pMetadataOffsetTable[tagIndex];

        if (TRUE == content.IsValid())
        {
            content.m_cameraId = cameraId;
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MetaBuffer::LinearMap::Dump
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID MetaBuffer::LinearMap::Dump(
    FILE*   pFile,
    BOOL    validTagsOnly)
{
    for (UINT32 tagIndex = 0; tagIndex < m_maxMetadataTags; ++tagIndex)
    {
        if ((FALSE == validTagsOnly) || (TRUE == m_pMetadataOffsetTable[tagIndex].IsValid()))
        {
            Content& content = m_pMetadataOffsetTable[tagIndex];

            const MetadataInfo* pInfo = HAL3MetadataUtil::GetMetadataInfoByIndex(tagIndex);

            OsUtils::FPrintF(pFile, "Tag %x size %u max_size %u offset %u addr %p region %u count %u parent %p cameraId %x"
                             " tagName %s\n",
                             content.m_tag,
                             content.m_size,
                             content.m_maxSize,
                             content.m_offset,
                             content.m_pVaddr,
                             content.m_regionIndex,
                             content.m_count,
                             content.m_pParentMetaBuffer,
                             content.m_cameraId,
                             pInfo->tagName);

            if (TRUE == content.IsValid())
            {
                HAL3MetadataUtil::DumpTag(pFile,
                                          content.m_tag,
                                          content.m_pVaddr,
                                          content.m_count,
                                          pInfo);
            }
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MetaBuffer::LinearMap::LinearIterator::LinearIterator
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
MetaBuffer::LinearMap::LinearIterator::LinearIterator(
    LinearMap &map)
    : m_mapIndex(0)
    , m_map(map)
{
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MetaBuffer::LinearMap::LinearIterator::Begin
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult MetaBuffer::LinearMap::LinearIterator::Begin()
{
    CamxResult result = CamxResultSuccess;

    for (m_mapIndex = 0; m_mapIndex < m_map.m_maxMetadataTags; ++m_mapIndex)
    {
        if (TRUE == m_map.m_pMetadataOffsetTable[m_mapIndex].IsValid())
        {
            break;
        }
    }

    if (TRUE == HasDone())
    {
        result = CamxResultENoMore;
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MetaBuffer::LinearMap::LinearIterator::HasDone
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL MetaBuffer::LinearMap::LinearIterator::HasDone()
{
    BOOL hasDone;

    if (m_mapIndex < m_map.m_maxMetadataTags)
    {
        hasDone = FALSE;
    }
    else
    {
        hasDone = TRUE;
    }

    return hasDone;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MetaBuffer::LinearMap::LinearIterator::Next
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult MetaBuffer::LinearMap::LinearIterator::Next()
{
    CamxResult result = CamxResultSuccess;

    if (FALSE == HasDone())
    {
        // increment the iterator first
        ++m_mapIndex;

        for (; m_mapIndex < m_map.m_maxMetadataTags; ++m_mapIndex)
        {
            if (TRUE == m_map.m_pMetadataOffsetTable[m_mapIndex].IsValid())
            {
                break;
            }
        }
    }

    if (TRUE == HasDone())
    {
        result = CamxResultENoMore;
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MetaBuffer::LinearMap::LinearIterator::GetEntry
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult MetaBuffer::LinearMap::LinearIterator::GetEntry(
    Entry& entry)
{
    CamxResult result = CamxResultSuccess;

    if (FALSE == HasDone())
    {
        Content& content = m_map.m_pMetadataOffsetTable[m_mapIndex];

        entry.tagID    = content.m_tag;
        entry.pTagData = content.m_pVaddr;
        entry.size     = content.m_size;
        entry.count    = content.m_count;
    }
    else
    {
        result = CamxResultEInvalidArg;
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MetaBuffer::HashMap::Create
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
MetaBuffer::HashMap* MetaBuffer::HashMap::Create(
    UINT32 maxNumTags)
{
    CAMX_UNREFERENCED_PARAM(maxNumTags);

    HashMap* pMap = CAMX_NEW MetaBuffer::HashMap();

    return pMap;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MetaBuffer::HashMap::Find
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
MetaBuffer::Content* MetaBuffer::HashMap::Find(
    UINT32 tag)
{
    Content* pContent = NULL;

    std::unordered_map<UINT32, Content>::iterator contentIterator = m_pMetadataOffsetMap.find(tag);

    if (contentIterator != m_pMetadataOffsetMap.end())
    {
        pContent = &contentIterator->second;
    }

    return pContent;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MetaBuffer::HashMap::Reset
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID MetaBuffer::HashMap::Reset()
{
    unordered_map<UINT32, Content>::iterator pContentIterator;

    for (pContentIterator = m_pMetadataOffsetMap.begin();
         pContentIterator != m_pMetadataOffsetMap.end();
         ++pContentIterator)
    {
        pContentIterator->second.Reset();
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MetaBuffer::HashMap::Insert
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID MetaBuffer::HashMap::Insert(
    UINT32 tag,
    UINT32 regionIndex,
    UINT32 offset,
    UINT32 size,
    BYTE*  pAddress,
    UINT32 count,
    UINT32 tagIndex,
    UINT32 maxSize)
{
    m_pMetadataOffsetMap.insert({ tag,
                                  Content(pAddress, count, regionIndex, offset, size, NULL, tag, tagIndex, maxSize)
                                });
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MetaBuffer::HashMap::Count
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
UINT32 MetaBuffer::HashMap::Count()
{
    UINT32 count = 0;
    unordered_map<UINT32, Content>::iterator pContentIterator;
    for (pContentIterator = m_pMetadataOffsetMap.begin();
         pContentIterator != m_pMetadataOffsetMap.end();
         ++pContentIterator)
    {
        if (pContentIterator->second.IsValid())
        {
            count++;
        }
    }
    return count;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MetaBuffer::HashMap::Merge
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID MetaBuffer::HashMap::Merge(
    Map*        pSrcMap,
    MetaBuffer* pSrcMetaBuffer)
{
    HashMap* pMap = static_cast<HashMap*>(pSrcMap);

    unordered_map<UINT32, Content>::iterator pSrcIterator;

    for (pSrcIterator = pMap->m_pMetadataOffsetMap.begin(); pSrcIterator != pMap->m_pMetadataOffsetMap.end(); ++pSrcIterator)
    {
        Content& srcContent = pSrcIterator->second;

        if (TRUE == srcContent.IsValid())
        {
            unordered_map<UINT32, Content>::iterator pDstIterator = m_pMetadataOffsetMap.find(pSrcIterator->first);

            if (pDstIterator == m_pMetadataOffsetMap.end())
            {
                m_pMetadataOffsetMap.insert({ pSrcIterator->first,
                                            Content(
                                                NULL,
                                                pSrcIterator->second.m_count,
                                                InvalidIndex,
                                                0,
                                                pSrcIterator->second.m_size,
                                                pSrcMetaBuffer,
                                                pSrcIterator->first,
                                                pSrcIterator->second.m_tagIndex,
                                                pSrcIterator->second.m_maxSize) });

                pDstIterator = m_pMetadataOffsetMap.find(pSrcIterator->first);

                CAMX_ASSERT(pDstIterator != m_pMetadataOffsetMap.end());
            }

            // update
            Content& dstContent = pDstIterator->second;

            dstContent.Assign(pSrcIterator->second);
            dstContent.m_pParentMetaBuffer = pSrcMetaBuffer;
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MetaBuffer::HashMap::MergeDisjoint
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID MetaBuffer::HashMap::MergeDisjoint(
    Map*        pSrcMap,
    MetaBuffer* pSrcMetaBuffer)
{
    HashMap* pMap = static_cast<HashMap*>(pSrcMap);

    unordered_map<UINT32, Content>::iterator pSrcIterator;
    for (pSrcIterator = pMap->m_pMetadataOffsetMap.begin(); pSrcIterator != pMap->m_pMetadataOffsetMap.end(); ++pSrcIterator)
    {
        Content& srcContent = pSrcIterator->second;

        if (TRUE == srcContent.IsValid())
        {
            unordered_map<UINT32, Content>::iterator pDstIterator = m_pMetadataOffsetMap.find(pSrcIterator->first);

            if ((pDstIterator == m_pMetadataOffsetMap.end()) || (FALSE == pDstIterator->second.IsValid()))
            {
                if (pDstIterator == m_pMetadataOffsetMap.end())
                {
                    m_pMetadataOffsetMap.insert({ pSrcIterator->first,
                                                Content(
                                                    NULL,
                                                    pSrcIterator->second.m_count,
                                                    InvalidIndex,
                                                    0,
                                                    pSrcIterator->second.m_size,
                                                    pSrcMetaBuffer,
                                                    pSrcIterator->first,
                                                    pSrcIterator->second.m_tagIndex,
                                                    pSrcIterator->second.m_maxSize) });

                    pDstIterator = m_pMetadataOffsetMap.find(pSrcIterator->first);

                    CAMX_ASSERT(pDstIterator != m_pMetadataOffsetMap.end());
                }

                // update
                Content& dstContent = pDstIterator->second;

                dstContent.Assign(pSrcIterator->second);
                dstContent.m_pParentMetaBuffer = pSrcMetaBuffer;
            }
        }
    }
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MetaBuffer::HashMap::SwitchAndMerge
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID MetaBuffer::HashMap::SwitchAndMerge(
    Map*        pSrcMap,
    MetaBuffer* pSrcMetaBuffer,
    Map*        pSrcMasterMap,
    UINT32      oldCameraId)
{
    HashMap* pMap       = static_cast<HashMap*>(pSrcMap);
    HashMap* pMasterMap = static_cast<HashMap*>(pSrcMasterMap);

    unordered_map<UINT32, Content>::iterator pSrcIterator;

    for (pSrcIterator = pMap->m_pMetadataOffsetMap.begin(); pSrcIterator != pMap->m_pMetadataOffsetMap.end(); ++pSrcIterator)
    {
        Content& srcContent = pSrcIterator->second;

        if (TRUE == srcContent.IsValid())
        {
            unordered_map<UINT32, Content>::iterator pDstIterator = m_pMetadataOffsetMap.find(pSrcIterator->first);

            if ((pDstIterator == m_pMetadataOffsetMap.end()) || (FALSE == pDstIterator->second.IsValid()))
            {
                if (pDstIterator == m_pMetadataOffsetMap.end())
                {
                    m_pMetadataOffsetMap.insert({ pSrcIterator->first,
                                                Content(
                                                    NULL,
                                                    pSrcIterator->second.m_count,
                                                    InvalidIndex,
                                                    0,
                                                    pSrcIterator->second.m_size,
                                                    pSrcMetaBuffer,
                                                    pSrcIterator->first,
                                                    pSrcIterator->second.m_tagIndex,
                                                    pSrcIterator->second.m_maxSize) });

                    pDstIterator = m_pMetadataOffsetMap.find(pSrcIterator->first);

                    CAMX_ASSERT(pDstIterator != m_pMetadataOffsetMap.end());
                }

                // update
                Content& dstContent       = pDstIterator->second;

                unordered_map<UINT32, Content>::iterator pMasterIterator =
                    pMasterMap->m_pMetadataOffsetMap.find(pSrcIterator->first);

                if ((oldCameraId     == srcContent.m_cameraId) &&
                    (pMasterIterator != pMasterMap->m_pMetadataOffsetMap.end()) &&
                    (TRUE == pMasterIterator->second.IsValid()))
                {
                    dstContent.Assign(pMasterIterator->second);
                }

                dstContent.m_pParentMetaBuffer = pSrcMetaBuffer;
            }
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MetaBuffer::HashMap::Copy
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID MetaBuffer::HashMap::Copy(
    Map*        pSrcMap,
    MetaBuffer* pSrcMetaBuffer)
{
    CAMX_UNREFERENCED_PARAM(pSrcMetaBuffer);

    HashMap* pMap = static_cast<HashMap*>(pSrcMap);

    unordered_map<UINT32, Content>::iterator pSrcIterator;

    for (pSrcIterator = pMap->m_pMetadataOffsetMap.begin(); pSrcIterator != pMap->m_pMetadataOffsetMap.end(); ++pSrcIterator)
    {
        if (TRUE == pSrcIterator->second.IsValid())
        {
            unordered_map<UINT32, Content>::iterator pDstIterator = m_pMetadataOffsetMap.find(pSrcIterator->first);

            // Contents must be allocated before Copy()
            CAMX_ASSERT(pDstIterator != m_pMetadataOffsetMap.end());

            pDstIterator->second.Copy(pSrcIterator->second);

            pDstIterator->second.m_pParentMetaBuffer = NULL;
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MetaBuffer::HashMap::CopyValidAndReserveUnfilledTags
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID MetaBuffer::HashMap::CopyValidAndReserveUnfilledTags(
    Map*                        pSrcMap,
    std::vector<MemoryRegion>&  memoryRegions,
    UINT32&                     totalSize,
    BOOL                        disjoint)
{
    HashMap* pSrcHashMap    = static_cast<HashMap*>(pSrcMap);
    BOOL     needAllocation = FALSE;

    totalSize = 0;

    unordered_map<UINT32, Content>::iterator pSrcIterator;

    for (pSrcIterator = pSrcHashMap->m_pMetadataOffsetMap.begin();
         pSrcIterator != pSrcHashMap->m_pMetadataOffsetMap.end();
         ++pSrcIterator)
    {
        Content& srcContent = pSrcIterator->second;

        if (TRUE == srcContent.IsValid())
        {
            unordered_map<UINT32, Content>::iterator pDstIterator = m_pMetadataOffsetMap.find(pSrcIterator->first);

            if ((FALSE == disjoint) ||
                ((pDstIterator != m_pMetadataOffsetMap.end()) && (FALSE == pDstIterator->second.IsValid())))
            {
                if (pDstIterator == m_pMetadataOffsetMap.end())
                {
                    m_pMetadataOffsetMap.insert({ pSrcIterator->first,
                        Content(
                            NULL,
                            pSrcIterator->second.m_count,
                            InvalidIndex,
                            0,
                            pSrcIterator->second.m_size,
                            NULL,
                            pSrcIterator->first,
                            pSrcIterator->second.m_tagIndex,
                            pSrcIterator->second.m_maxSize) });

                    pDstIterator = m_pMetadataOffsetMap.find(pSrcIterator->first);

                    CAMX_ASSERT(pDstIterator != m_pMetadataOffsetMap.end());
                }

                Content& dstContent = pDstIterator->second;

                if (dstContent.m_regionIndex < memoryRegions.size())
                {
                    dstContent.m_pVaddr = memoryRegions[dstContent.m_regionIndex].m_pVaddr + dstContent.m_offset;

                    dstContent.Copy(srcContent);
                }
                else if (MaxInplaceTagSize >= dstContent.m_maxSize)
                {
                    dstContent.Copy(srcContent);
                }
                else
                {
                    dstContent.m_size  = srcContent.m_size;
                    dstContent.m_count = srcContent.m_count;

                    // mark the data
                    dstContent.m_pVaddr            = srcContent.m_pVaddr;
                    dstContent.m_offset            = totalSize;
                    dstContent.m_regionIndex       = ReservedIndex;
                    dstContent.m_pParentMetaBuffer = NULL;
                    dstContent.m_offset            = totalSize;

                    totalSize += srcContent.m_size;
                }
            }
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MetaBuffer::HashMap::SetUnfilledTagRegion
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID MetaBuffer::HashMap::SetUnfilledTagRegion(
    BYTE*   pDstBaseAddress,
    UINT32  regionIndex)
{
    unordered_map<UINT32, Content>::iterator pIterator;

    for (pIterator = m_pMetadataOffsetMap.begin(); pIterator != m_pMetadataOffsetMap.end(); ++pIterator)
    {
        Content& content = pIterator->second;

        if (ReservedIndex == content.m_regionIndex)
        {
            BYTE* pSrcAddress = content.m_pVaddr;
            BYTE* pDstAddress = pDstBaseAddress + content.m_offset;

            content.m_regionIndex = regionIndex;
            content.m_pVaddr      = pDstAddress;

            Utils::Memcpy(pDstAddress, pSrcAddress, content.m_size);
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MetaBuffer::HashMap::GetAndroidMeta
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult MetaBuffer::HashMap::GetAndroidMeta(
    camera_metadata_t*      pAndroidMeta,
    UINT32                  propertyBlobId,
    BOOL                    frameworkTagsOnly,
    BOOL                    filterProperties,
    unordered_set<UINT32>&  filterSet)
{
    CamxResult          result      = CamxResultSuccess;
    PropertyPackingInfo packingInfo = {};

    unordered_map<UINT32, Content>::iterator pIterator;

    for (pIterator = m_pMetadataOffsetMap.begin(); pIterator != m_pMetadataOffsetMap.end(); ++pIterator)
    {
        if (pIterator->second.IsValid())
        {
            BOOL isTagPresent = (filterSet.find(pIterator->second.m_tag) != filterSet.end());

            const MetadataInfo* pInfo = HAL3MetadataUtil::GetMetadataInfoByIndex(pIterator->second.m_tagIndex);

            CAMX_ASSERT(NULL != pInfo);

            if ((HAL3MetadataUtil::IsProperty(pIterator->first)))
            {
                if (FALSE == filterProperties)
                {
                    result = HAL3MetadataUtil::AppendPropertyPackingInfo(
                        pIterator->first,
                        pIterator->second.m_pVaddr,
                        &packingInfo);
                }
            }
            else if ((FALSE  == isTagPresent)      &&
                     ((FALSE == frameworkTagsOnly) ||
                      (0     != (TagSectionVisibility::TagSectionVisibleToFramework & pInfo->visibility))))
            {
                result = HAL3MetadataUtil::UpdateMetadata(
                    pAndroidMeta,
                    pIterator->first,
                    pIterator->second.m_pVaddr,
                    pIterator->second.m_count,
                    TRUE);
            }

            if (CamxResultSuccess != result)
            {
                CAMX_LOG_ERROR(CamxLogGroupMeta, "Update tag %x failed %d", pIterator->first, result);
            }
        }
    }

    if (CamxResultSuccess == result)
    {
        if (0 != packingInfo.count)
        {
            INT size = PropertyBlobSize;
            BYTE* pPropertyBlob = CAMX_NEW BYTE[size];
            if (NULL != pPropertyBlob)
            {
                HAL3MetadataUtil::PackPropertyInfoToBlob(&packingInfo, pPropertyBlob);
                result = HAL3MetadataUtil::UpdateMetadata(
                    pAndroidMeta,
                    propertyBlobId,
                    pPropertyBlob,
                    size,
                    TRUE);
                if (CamxResultSuccess == result)
                {
                    CAMX_LOG_INFO(CamxLogGroupMeta, "Added %d properties to vendor tag", packingInfo.count);
                }
                CAMX_DELETE[] pPropertyBlob;
            }
            else
            {
                CAMX_LOG_ERROR(CamxLogGroupMeta, "Failed to allocate memory for pPropertyBlob");
                result = CamxResultENoMemory;
            }

        }
    }


    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MetaBuffer::HashMap::Dump
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID MetaBuffer::HashMap::Dump(
    FILE*   pFile,
    BOOL    validTagsOnly)
{
    unordered_map<UINT32, Content>::iterator pIterator;
    for (pIterator = m_pMetadataOffsetMap.begin(); pIterator != m_pMetadataOffsetMap.end(); ++pIterator)
    {
        if (!validTagsOnly || pIterator->second.IsValid())
        {
            const MetadataInfo* pInfo = HAL3MetadataUtil::GetMetadataInfoByTag(pIterator->first);

            OsUtils::FPrintF(pFile, "Tag %x size %u offset %u addr %p region %u count %u parent %p cameraId %u tagName %s\n",
                             pIterator->second.m_tag,
                             pIterator->second.m_size,
                             pIterator->second.m_offset,
                             pIterator->second.m_pVaddr,
                             pIterator->second.m_regionIndex,
                             pIterator->second.m_count,
                             pIterator->second.m_pParentMetaBuffer,
                             pIterator->second.m_cameraId,
                             pInfo->tagName);

            if (pIterator->second.IsValid() && !HAL3MetadataUtil::IsProperty(pIterator->first))
            {
                HAL3MetadataUtil::DumpTag(pFile,
                                          pIterator->second.m_tag,
                                          pIterator->second.m_pVaddr,
                                          pIterator->second.m_count,
                                          pInfo);
            }
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MetaBuffer::HashMap::Print
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID MetaBuffer::HashMap::Print(
    BOOL    validTagsOnly)
{
    unordered_map<UINT32, Content>::iterator pIterator;
    for (pIterator = m_pMetadataOffsetMap.begin(); pIterator != m_pMetadataOffsetMap.end(); ++pIterator)
    {
        if (!validTagsOnly || pIterator->second.IsValid())
        {
            const MetadataInfo* pInfo = HAL3MetadataUtil::GetMetadataInfoByTag(pIterator->first);

            CAMX_LOG_INFO(CamxLogGroupMeta, "Tag %x size %u offset %u addr %p region %u count %u parent %p tagName %s",
                pIterator->second.m_tag,
                pIterator->second.m_size,
                pIterator->second.m_offset,
                pIterator->second.m_pVaddr,
                pIterator->second.m_regionIndex,
                pIterator->second.m_count,
                pIterator->second.m_pParentMetaBuffer,
                pInfo ? pInfo->tagName : "NULL");
        }
    }
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MetaBuffer::HashMap::UpdateCameraId
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID MetaBuffer::HashMap::UpdateCameraId(
    UINT32  cameraId)
{
    unordered_map<UINT32, Content>::iterator pIterator;

    for (pIterator = m_pMetadataOffsetMap.begin(); pIterator != m_pMetadataOffsetMap.end(); ++pIterator)
    {
        if (TRUE == pIterator->second.IsValid())
        {
            pIterator->second.m_cameraId = cameraId;
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MetaBuffer::HashMap::HashIterator::HashIterator
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
MetaBuffer::HashMap::HashIterator::HashIterator(
    HashMap &map)
    : m_map(map)
    , m_iterator(map.m_pMetadataOffsetMap.begin())
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MetaBuffer::HashMap::HashIterator::Begin
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult MetaBuffer::HashMap::HashIterator::Begin()
{
    CamxResult result = CamxResultSuccess;

    for (m_iterator = m_map.m_pMetadataOffsetMap.begin(); m_iterator != m_map.m_pMetadataOffsetMap.end(); ++m_iterator)
    {
        if (m_iterator->second.IsValid())
        {
            break;
        }
    }

    if (TRUE == HasDone())
    {
        result = CamxResultENoMore;
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MetaBuffer::HashMap::HashIterator::HasDone
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL MetaBuffer::HashMap::HashIterator::HasDone()
{
    BOOL hasDone;

    if (m_iterator != m_map.m_pMetadataOffsetMap.end())
    {
        hasDone = FALSE;
    }
    else
    {
        hasDone = TRUE;
    }

    return hasDone;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MetaBuffer::HashMap::HashIterator::Next
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult MetaBuffer::HashMap::HashIterator::Next()
{
    CamxResult result = CamxResultSuccess;

    if (FALSE == HasDone())
    {
        // increment the iterator first
        ++m_iterator;

        for (; m_iterator != m_map.m_pMetadataOffsetMap.end(); ++m_iterator)
        {
            if (m_iterator->second.IsValid())
            {
                break;
            }
        }
    }

    if (TRUE == HasDone())
    {
        result = CamxResultENoMore;
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MetaBuffer::HashMap::HashIterator::GetEntry
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult MetaBuffer::HashMap::HashIterator::GetEntry(
    Entry& entry)
{
    CamxResult result = CamxResultSuccess;

    if (FALSE == HasDone())
    {
        entry.tagID    = m_iterator->first;
        entry.pTagData = m_iterator->second.m_pVaddr;
        entry.size     = m_iterator->second.m_size;
        entry.count    = m_iterator->second.m_count;
    }
    else
    {
        result = CamxResultEInvalidArg;
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// CreateUniqueID
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
UINT32 CreateUniqueID()
{
    static atomic<UINT32> s_metabufferCount;
    return MetaBufferClientMask | ++s_metabufferCount;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// MetaBuffer::Create
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
MetaBuffer* MetaBuffer::Create(
    VOID* phPrivateUserHandle)
{
    CamxResult result;

    MetaBuffer* pMetaBuffer = CAMX_NEW MetaBuffer;
    CAMX_ASSERT(NULL != pMetaBuffer);

    if (NULL != pMetaBuffer)
    {
        BOOL enableLinearMetaLUT = HwEnvironment::GetInstance()->GetStaticSettings()->enableLinearMetaLUT;

        result = pMetaBuffer->Initialize(enableLinearMetaLUT);

        CAMX_LOG_VERBOSE(CamxLogGroupMeta, "MetaBuffer created linearLUT %d", enableLinearMetaLUT)

        if (CamxResultSuccess == result)
        {
            pMetaBuffer->m_phPrivateUserHandle = phPrivateUserHandle;
        }
        else
        {
            CAMX_DELETE pMetaBuffer;
            pMetaBuffer = NULL;
            CAMX_LOG_ERROR(CamxLogGroupMeta, "Initialize failed failed");
        }
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupMeta, "Create failed");
    }
    return pMetaBuffer;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// MetaBuffer::Initialize
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult MetaBuffer::Initialize(
    BOOL useLookupTable)
{
    CamxResult result = CamxResultSuccess;

    CAMX_ASSERT(0 < HAL3MetadataUtil::GetTotalTagCount());
    m_pMemoryRegionLock = Mutex::Create("MetaBufferMemoryLock");

    if (NULL != m_pMemoryRegionLock)
    {
        m_metaBufferDependentLinks.resize(NumDefaultMetaBufferLinks);
        m_maxMetadataTags = HAL3MetadataUtil::GetTotalTagCount();

        if (0 < m_maxMetadataTags)
        {
            m_memoryRegions.reserve(m_maxMetadataTags);
            if (TRUE == useLookupTable)
            {
                m_pMap    = LinearMap::Create(m_maxMetadataTags);
                m_mapType = MapType::Linear;
            }
            else
            {
                m_pMap    = HashMap::Create(m_maxMetadataTags);
                m_mapType = MapType::Hash;
            }

            if (NULL == m_pMap)
            {
                result = CamxResultENoMemory;
                CAMX_LOG_ERROR(CamxLogGroupMeta, "Allocation failed while creating map type %d", useLookupTable);
            }
            else
            {
                m_pClientLock = Mutex::Create("MetaBufferClientLock");
                if (NULL == m_pClientLock)
                {
                    result = CamxResultENoMemory;
                    CAMX_LOG_ERROR(CamxLogGroupMeta, "Initialize failed while creating client mutex");
                }
            }

            if (CamxResultSuccess == result)
            {
                m_pRWLock = ReadWriteLock::Create("MetaBufferMapLock");
                if (NULL == m_pRWLock)
                {
                    result = CamxResultEFailed;
                }
            }

            if (CamxResultSuccess == result)
            {
                m_uniqueId    = CreateUniqueID();
                m_metaBufferClients.resize(NumDefaultMetaBufferClients);

                ClientInfo clientInfo = { InvalidClient, 0, NULL };
                std::fill(m_metaBufferClients.begin(), m_metaBufferClients.end(), clientInfo);

                result = VendorTagManager::QueryVendorTagLocation("org.codeaurora.qcamera3.internal_private",
                                                                  "private_property",
                                                                  &m_propertyBlobId);

            }
        }
        else
        {
            result = CamxResultEInvalidState;
            CAMX_LOG_ERROR(CamxLogGroupMeta, "Initialize failed metadata tags %u", m_maxMetadataTags);
        }
    }
    else
    {
        result = CamxResultENoMemory;
        CAMX_LOG_ERROR(CamxLogGroupMeta, "Initialize failed while creating memory mutex");
    }
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// MetaBuffer::Destroy
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult MetaBuffer::Destroy(
    BOOL force)
{
    CamxResult result;
    if ((TRUE == force) || (0 == ReferenceCount()))
    {
        CAMX_LOG_INFO(CamxLogGroupMeta, "Destroy %p %x %u size %u", this, m_uniqueId, ReferenceCount(), Capacity());
        m_metaBufferIdentifier = 0xdeadbeef;
        CAMX_DELETE this;
        result = CamxResultSuccess;
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupMeta, "Cannot destroy %u", ReferenceCount());
        result = CamxResultEBusy;
    }
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// MetaBuffer::MetaBuffer
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
MetaBuffer::MetaBuffer()
    : m_metaBufferIdentifier(s_metaBufferIdentifier)
    , m_pMemoryRegionLock(NULL)
    , m_externalRefCount(0)
    , m_pClientLock(NULL)
    , m_pMap(NULL)
    , m_internalRefCount(0)
    , m_mergeRefCount(0)
    , m_pRWLock(NULL)
    , m_cameraId(InvalidCameraId)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// MetaBuffer::~MetaBuffer
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
MetaBuffer::~MetaBuffer()
{
    for (vector<Link>::iterator pLink = m_metaBufferDependentLinks.begin();
        pLink != m_metaBufferDependentLinks.end(); ++pLink)
    {
        pLink->m_pMetaBuffer = NULL;
    }

    for (vector<MemoryRegion>::iterator pRegion = m_memoryRegions.begin();
         pRegion != m_memoryRegions.end(); ++pRegion)
    {
        pRegion->Release();
    }

    if (NULL != m_pMap)
    {
        CAMX_DELETE m_pMap;
        m_pMap = NULL;
    }

    if (NULL != m_pMemoryRegionLock)
    {
        m_pMemoryRegionLock->Destroy();
        m_pMemoryRegionLock = NULL;
    }

    if (NULL != m_pClientLock)
    {
        m_pClientLock->Destroy();
        m_pClientLock = NULL;
    }

    if (NULL != m_pRWLock)
    {
        m_pRWLock->Destroy();
        m_pRWLock = NULL;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// MetaBuffer::Reset
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult MetaBuffer::Reset()
{
    // remove the usage of buffer
    for (vector<Link>::iterator pLink = m_metaBufferDependentLinks.begin();
         pLink != m_metaBufferDependentLinks.end(); ++pLink)
    {
        if (NULL != pLink->m_pMetaBuffer)
        {
            pLink->m_pMetaBuffer->ReleaseReference(this);

            pLink->m_pMetaBuffer = NULL;
        }
    }

    m_pMap->Reset();

    m_pCameraIdSubTree  = NULL;
    m_invalidatePending = FALSE;
    m_cameraIdMap.clear();

    return CamxResultSuccess;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// MetaBuffer::ReserveRegionAndAllocate
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult MetaBuffer::ReserveRegionAndAllocate(
    UINT32  totalSize,
    UINT32& regionIndex)
{
    CamxResult result = CamxResultSuccess;
    if (0 < totalSize)
    {
        m_pMemoryRegionLock->Lock();

        UINT32 regionSize = static_cast<UINT32>(m_memoryRegions.size());
        for (regionIndex = 0; regionIndex < m_memoryRegions.size(); ++regionIndex)
        {
            if (m_memoryRegions[regionIndex].IsFree())
            {
                // early marking to reserve the region
                m_memoryRegions[regionIndex].m_size = totalSize;
                break;
            }
        }

        if (m_memoryRegions.size() == regionIndex)
        {
            if (regionIndex < m_memoryRegions.capacity())
            {
                // need allocation
                MemoryRegion newRegion;
                newRegion.m_size = totalSize;
                // reserve the index to facilitate parallel allocation
                m_memoryRegions.push_back(newRegion);
                CAMX_LOG_VERBOSE(CamxLogGroupMeta, "Allocation Growing the region index %d size %d totalSize %u",
                    regionIndex, m_memoryRegions.size(), totalSize);
            }
            else
            {
                result = CamxResultENoMemory;
                CAMX_LOG_ERROR(CamxLogGroupMeta, "Allocation exceeds capacity region %d count %d",
                               regionIndex, m_memoryRegions.capacity());
            }
        }
        m_pMemoryRegionLock->Unlock();

        // allocate now
        if (CamxResultSuccess == result)
        {
            result = m_memoryRegions[regionIndex].Allocate(totalSize);

            if (NULL == m_memoryRegions[regionIndex].m_pVaddr)
            {
                result = CamxResultENoMemory;
                CAMX_LOG_ERROR(CamxLogGroupMeta, "Allocation failed region %d count %d", regionIndex, totalSize);
            }
        }
        CAMX_LOG_VERBOSE(CamxLogGroupMeta, "Allocation region %d count %d", regionIndex, totalSize);
    }
    else
    {
        result = CamxResultEInvalidArg;
    }
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// MetaBuffer::AllocateBuffer
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult MetaBuffer::AllocateBuffer(
    const UINT32* pMetadataTags,
    UINT32        metadataTagCount)
{
    CamxResult result = CamxResultSuccess;

    CAMX_ASSERT(NULL != pMetadataTags);
    CAMX_ASSERT(0 < metadataTagCount);

    if ((NULL != pMetadataTags) && (0 < metadataTagCount))
    {
        // calculate size and offsets
        UINT32          totalSize        = 0;
        UINT32          alignedTotalSize = 0;

        vector<UINT32>  tagSize(metadataTagCount);
        UINT32          regionIndex;

        for (UINT32 tagIndex = 0; tagIndex < metadataTagCount; ++tagIndex)
        {
            UINT32              tagId = pMetadataTags[tagIndex];
            const MetadataInfo* pInfo = HAL3MetadataUtil::GetMetadataInfoByTag(tagId);

            CAMX_ASSERT(NULL != pInfo);

            Content* pContent = m_pMap->Find(tagId);

            if (NULL == pContent)
            {
                regionIndex = (MaxInplaceTagSize >= pInfo->size) ? InPlaceMemoryRegion : InvalidIndex;

                m_pMap->Insert(tagId,
                               regionIndex,
                               0,
                               pInfo->size,
                               NULL,
                               0,
                               pInfo->index,
                               pInfo->size);
            }

            if (((NULL != pContent) && (InvalidIndex == pContent->m_regionIndex)) ||
                ((NULL == pContent) && (MaxInplaceTagSize < pInfo->size)))
            {
                tagSize[tagIndex] = pInfo->size;

                totalSize         += tagSize[tagIndex];
                alignedTotalSize  += CAMX_ALIGN_TO(tagSize[tagIndex], AlignmentFactor);
            }
        }

        if (0 < alignedTotalSize)
        {
            result = ReserveRegionAndAllocate(alignedTotalSize, regionIndex);

            if (CamxResultSuccess == result)
            {
                UINT32 absOffset = 0;

                for (UINT32 index = 0; index < metadataTagCount; ++index)
                {
                    UINT32              tagId = pMetadataTags[index];
                    const MetadataInfo* pInfo = HAL3MetadataUtil::GetMetadataInfoByTag(tagId);

                    Content* pContent = m_pMap->Find(tagId);

                    if (InvalidIndex == pContent->m_regionIndex)
                    {
                        pContent->m_regionIndex = regionIndex;
                        pContent->m_offset      = absOffset;
                        pContent->m_tag         = tagId;
                        pContent->m_size        = pInfo->size;
                    }
                    else if (InPlaceMemoryRegion == pContent->m_regionIndex)
                    {
                        pContent->m_offset      = 0;
                        pContent->m_tag         = tagId;
                        pContent->m_size        = pInfo->size;
                    }

                    absOffset += CAMX_ALIGN_TO(tagSize[index], AlignmentFactor);
                }
            }
        }

        CAMX_LOG_VERBOSE(CamxLogGroupMeta, "Allocation success tagCount %d size %u", metadataTagCount, totalSize);
    }
    else
    {
        result = CamxResultEInvalidArg;
    }

    if (CamxResultSuccess != result)
    {
        CAMX_LOG_ERROR(CamxLogGroupMeta, "Allocation failed result %d tagCount %d", result, metadataTagCount);
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// MetaBuffer::AllocateBuffer
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult MetaBuffer::AllocateBuffer(
    const std::unordered_set<UINT32>& metadataTags)
{
    CamxResult result = CamxResultSuccess;

    if (0 < metadataTags.size())
    {
        vector<UINT32> tagVector(metadataTags.begin(), metadataTags.end());

        result = AllocateBuffer(tagVector.data(), static_cast<UINT32>(tagVector.size()));
    }
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// MetaBuffer::AllocateBuffer
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult MetaBuffer::AllocateBuffer(
    const camera_metadata_t* pMetadata)
{
    CamxResult result = CamxResultSuccess;
    UINT32 totalEntries = static_cast<UINT32>(get_camera_metadata_entry_count(pMetadata));

    if (0 < totalEntries)
    {
        // calculate size and offsets
        vector<UINT32> tagVector(totalEntries);

        for (UINT32 index = 0; index < totalEntries; ++index)
        {
            camera_metadata_ro_entry_t entry;

            INT32 metadataStatus = static_cast<UINT32>(get_camera_metadata_ro_entry(pMetadata, index, &entry));

            if (MetadataUtilOK == metadataStatus)
            {
                tagVector[index] = entry.tag;
            }
            else
            {
                CAMX_LOG_WARN(CamxLogGroupMeta, "Invalid tag %x status %d skipping..",
                    entry.tag, metadataStatus);
            }
        }

        result = AllocateBuffer(tagVector.data(), static_cast<UINT32>(tagVector.size()));
    }
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// MetaBuffer::AddToFreeLink
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult MetaBuffer::AddToFreeLink(
    MetaBuffer* pDependee)
{
    CamxResult result = CamxResultENoMore;
    vector<Link>::iterator pLink;
    m_pClientLock->Lock();
    for (pLink = m_metaBufferDependentLinks.begin(); pLink != m_metaBufferDependentLinks.end(); ++pLink)
    {
        if (NULL == pLink->m_pMetaBuffer)
        {
            pLink->m_pMetaBuffer = pDependee;
            result               = CamxResultSuccess;
            break;
        }
    }

    if (CamxResultSuccess != result)
    {
        CAMX_LOG_ERROR(CamxLogGroupMeta, "Max links reached %d. Grow size, new = %u",
            result,
            m_metaBufferDependentLinks.size());
        m_metaBufferDependentLinks.push_back(Link(pDependee));
        result = CamxResultSuccess;
    }
    m_pClientLock->Unlock();
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// MetaBuffer::RemoveFromLink
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult MetaBuffer::RemoveFromLink(
    MetaBuffer* pDependee)
{
    CamxResult result = CamxResultENoSuch;
    vector<Link>::iterator pLink;
    m_pClientLock->Lock();
    for (pLink = m_metaBufferDependentLinks.begin(); pLink != m_metaBufferDependentLinks.end(); ++pLink)
    {
        if (pDependee == pLink->m_pMetaBuffer)
        {
            pLink->m_pMetaBuffer = NULL;
            result               = CamxResultSuccess;
            break;
        }
    }
    m_pClientLock->Unlock();
    if (CamxResultSuccess != result)
    {
        CAMX_LOG_ERROR(CamxLogGroupMeta, "Invalid metabuffer to remove %u %u", m_uniqueId, pDependee->GetUniqueID());
    }
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// MetaBuffer::Merge
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult MetaBuffer::Merge(
    MetaBuffer* pSrcMetaBuffer,
    BOOL        disjoint,
    BOOL        needLock)
{
    CamxResult result = CamxResultSuccess;

    CAMX_ASSERT(NULL != pSrcMetaBuffer);

    if (NULL == pSrcMetaBuffer)
    {
        result = CamxResultEInvalidArg;
        CAMX_LOG_ERROR(CamxLogGroupMeta, "Invalid metadata disjoint %d", disjoint);
    }
    else if (pSrcMetaBuffer == this)
    {
        result = CamxResultEInvalidArg;
        CAMX_LOG_ERROR(CamxLogGroupMeta, "Error Merge to itself");
    }
    else if (m_mapType != pSrcMetaBuffer->m_mapType)
    {
        result = CamxResultENotImplemented;
        CAMX_LOG_ERROR(CamxLogGroupMeta, "Maptype mismatch %d %d", m_mapType, pSrcMetaBuffer->m_mapType);
    }
    else
    {
        CheckAndIssueWritelock(needLock);

        if (TRUE == disjoint)
        {
            m_pMap->MergeDisjoint(pSrcMetaBuffer->m_pMap, pSrcMetaBuffer);
        }
        else
        {
            m_pMap->Merge(pSrcMetaBuffer->m_pMap, pSrcMetaBuffer);
        }

        CheckAndIssueUnlock(needLock);

        result = AddToFreeLink(pSrcMetaBuffer);

        if (CamxResultSuccess == result)
        {
            pSrcMetaBuffer->AddReference(this);
        }
    }
    return result;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// MetaBuffer::Merge
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult MetaBuffer::Merge(
    MetaBuffer* pSrcMetaBuffer,
    UINT32      oldMasterCameraId,
    UINT32      newMasterCameraId,
    BOOL        needLock)
{
    CamxResult result = CamxResultSuccess;

    CAMX_ASSERT(NULL != pSrcMetaBuffer);

    if (NULL == pSrcMetaBuffer)
    {
        result = CamxResultEInvalidArg;
        CAMX_LOG_ERROR(CamxLogGroupMeta, "Invalid metadata");
    }
    else if (pSrcMetaBuffer == this)
    {
        result = CamxResultEInvalidArg;
        CAMX_LOG_ERROR(CamxLogGroupMeta, "Error Merge to itself");
    }
    else if (m_mapType != pSrcMetaBuffer->m_mapType)
    {
        result = CamxResultENotImplemented;
        CAMX_LOG_ERROR(CamxLogGroupMeta, "Maptype mismatch %d %d", m_mapType, pSrcMetaBuffer->m_mapType);
    }
    else
    {
        CheckAndIssueWritelock(needLock);

        // find the node. Do only once to improve performance of subsequent calls
        if ((0 == pSrcMetaBuffer->m_cameraIdMap.size()) && (NULL == pSrcMetaBuffer->m_pCameraIdSubTree))
        {
            FindSubtreeContainingCameraId();
        }

        if (NULL != pSrcMetaBuffer->m_pCameraIdSubTree)
        {
            map<UINT32, MetaBuffer*>::iterator cameraIdIterator =
                pSrcMetaBuffer->m_pCameraIdSubTree->m_cameraIdMap.find(newMasterCameraId);

            if ((pSrcMetaBuffer->m_pCameraIdSubTree->m_cameraIdMap.end() != cameraIdIterator) &&
                (NULL != cameraIdIterator->second))
            {
                MetaBuffer* pMasterMeta = cameraIdIterator->second;

                m_pMap->SwitchAndMerge(pSrcMetaBuffer->m_pMap, pSrcMetaBuffer, pMasterMeta->m_pMap, oldMasterCameraId);
            }
            else
            {
                result = CamxResultEInvalidArg;
                CAMX_LOG_ERROR(CamxLogGroupMeta, "Metadata doesnt contain cameraId %d tags %p", newMasterCameraId, this);
            }
        }
        else
        {
            m_pMap->Merge(pSrcMetaBuffer->m_pMap, pSrcMetaBuffer);
            CAMX_LOG_INFO(CamxLogGroupMeta, "Metadata doesnt contain multi-camera info,"
                           " subtree containing cameraId not found, switch to regular merge %p", this);
        }

        CheckAndIssueUnlock(needLock);

        if (CamxResultSuccess == result)
        {
            result = AddToFreeLink(pSrcMetaBuffer);
        }

        if (CamxResultSuccess == result)
        {
            pSrcMetaBuffer->AddReference(this);
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// MetaBuffer::Copy
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult MetaBuffer::Copy(
    MetaBuffer* pSrcMetaBuffer,
    BOOL        disjoint,
    BOOL        needLock)
{
    CamxResult result = CamxResultSuccess;

    CAMX_ASSERT(NULL != pSrcMetaBuffer);

    if ((NULL != pSrcMetaBuffer) && (m_mapType == pSrcMetaBuffer->m_mapType))
    {
        UINT32         totalSize;
        UINT32         regionIndex;

        CheckAndIssueWritelock(needLock);

        m_pMap->CopyValidAndReserveUnfilledTags(pSrcMetaBuffer->m_pMap, m_memoryRegions, totalSize, disjoint);

        if (0 < totalSize)
        {
            result = ReserveRegionAndAllocate(totalSize, regionIndex);
            if (CamxResultSuccess == result)
            {
                // assign the indices
                m_pMap->SetUnfilledTagRegion(m_memoryRegions[regionIndex].m_pVaddr, regionIndex);
            }
            else
            {
                CAMX_LOG_ERROR(CamxLogGroupMeta, "Cannot copy the buffers status %d", result);
            }
        }

        CheckAndIssueUnlock(needLock);
    }
    else if (NULL == pSrcMetaBuffer)
    {
        result = CamxResultEInvalidArg;

        CAMX_LOG_ERROR(CamxLogGroupMeta, "Invalid metadata disjoint %d", disjoint);
    }
    else
    {
        result = CamxResultENotImplemented;

        CAMX_LOG_ERROR(CamxLogGroupMeta, "Maptype mismatch %d %d", m_mapType, pSrcMetaBuffer->m_mapType);
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// MetaBuffer::Invalidate
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult MetaBuffer::Invalidate(
    BOOL force)
{
    CamxResult result = CamxResultEInvalidState;
    if (TRUE == force)
    {
        result = Reset();
        CAMX_LOG_INFO(CamxLogGroupMeta, "Force Invalidate %p %x", this, m_uniqueId);
        // Keep this different from Reset() so that in future we can optimize by doing deferred reset
    }
    else
    {
        if (0 == ReferenceCount())
        {
            result = Reset();
            CAMX_LOG_INFO(CamxLogGroupMeta, "Invalidate %p %x", this, m_uniqueId);
        }
        else
        {
            m_invalidatePending = TRUE;
            CAMX_LOG_INFO(CamxLogGroupMeta, "Invalidate Pending cur %p %x refCount %u",
                           this, m_uniqueId, ReferenceCount());
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// MetaBuffer::GetTag
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID* MetaBuffer::GetTag(
    UINT32 metadataTag,
    BOOL   needLock)
{
    CheckAndIssueReadlock(needLock);

    Content* pContent = m_pMap->Find(metadataTag);
    VOID*    pTagData = pContent ? pContent->m_pVaddr : NULL;

    CheckAndIssueUnlock(needLock);

    return pTagData;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// MetaBuffer::FindSubtreeContainingCameraId
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult MetaBuffer::FindSubtreeContainingCameraId()
{
    queue<MetaBuffer*> bfsQueue;
    CamxResult         result    = CamxResultENoSuch;
    UINT32             depth     = 0;

    bfsQueue.push(this);

    while (!bfsQueue.empty() && (MaxGraphDepth > depth++))
    {
        MetaBuffer* pCurrentMeta = bfsQueue.front();
        bfsQueue.pop();

        if (0 != pCurrentMeta->m_cameraIdMap.size())
        {
            m_pCameraIdSubTree = pCurrentMeta;
            result             = CamxResultSuccess;
            break;
        }

        vector<Link>::iterator pLink;
        for (pLink = pCurrentMeta->m_metaBufferDependentLinks.begin();
             pLink != pCurrentMeta->m_metaBufferDependentLinks.end(); ++pLink)
        {
            if (NULL != pLink->m_pMetaBuffer)
            {
                bfsQueue.push(pLink->m_pMetaBuffer);
            }
        }
    }

    if (MaxGraphDepth < depth)
    {
        result = CamxResultEFailed;
        CAMX_LOG_ERROR(CamxLogGroupMeta, "ERROR: Max depth reached %d mb %p", depth, this);
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// MetaBuffer::GetTagByCameraId
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID* MetaBuffer::GetTagByCameraId(
    UINT32 metadataTag,
    UINT32 cameraId,
    BOOL   needLock)
{
    VOID*    pTagData = NULL;
    Content* pContent;

    CheckAndIssueReadlock(needLock);

    // find the content
    pContent = m_pMap->Find(metadataTag);

    // If the cameraId is not set, tag is agnostic to the cameraId.
    if (InvalidCameraId == pContent->m_cameraId)
    {
        pTagData = pContent->m_pVaddr;

        CheckAndIssueUnlock(needLock);
    }
    else
    {
        CheckAndIssueUnlock(needLock);

        // find the node. Do only once to improve performance of subsequent calls
        if ((0 == m_cameraIdMap.size()) && (NULL == m_pCameraIdSubTree))
        {
            FindSubtreeContainingCameraId();
        }

        if (NULL != m_pCameraIdSubTree)
        {
            map<UINT32, MetaBuffer*>::iterator cameraIdIterator = m_pCameraIdSubTree->m_cameraIdMap.find(cameraId);

            if ((m_pCameraIdSubTree->m_cameraIdMap.end() != cameraIdIterator) && (NULL != cameraIdIterator->second))
            {
                pTagData = cameraIdIterator->second->GetTag(metadataTag, needLock);
            }
        }
    }

    return pTagData;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// MetaBuffer::GetTag
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult MetaBuffer::GetTag(
    UINT32 metadataTag,
    Entry& entry,
    BOOL   needLock)
{
    CamxResult result   = CamxResultSuccess;

    CheckAndIssueReadlock(needLock);

    Content* pContent = m_pMap->Find(metadataTag);

    if ((NULL != pContent) && (NULL != pContent->m_pVaddr))
    {
        const MetadataInfo* pInfo = HAL3MetadataUtil::GetMetadataInfoByIndex(pContent->m_tagIndex);
        if (NULL != pInfo)
        {
            entry.count     = pContent->m_count;
            entry.size      = pContent->m_size;
            entry.pTagData  = pContent->m_pVaddr;
            entry.tagID     = metadataTag;
            entry.type      = static_cast<Entry::EntryType>(pInfo->type);
        }
        else
        {
            result = CamxResultEOutOfBounds;
            CAMX_LOG_ERROR(CamxLogGroupMeta, "Tag Info not found %x index %d max tags %d",
                metadataTag, pContent->m_tagIndex, HAL3MetadataUtil::GetTotalTagCount());
        }
    }
    else
    {
        result = CamxResultENoSuch;
    }

    CheckAndIssueUnlock(needLock);

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// MetaBuffer::GetTag
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID* MetaBuffer::GetTag(
    UINT32  metadataTag,
    UINT32& count,
    BOOL    needLock)
{
    CheckAndIssueReadlock(needLock);

    Content*    pContent = m_pMap->Find(metadataTag);
    VOID*       pPayload = NULL;
    if ((NULL != pContent) && (NULL != pContent->m_pVaddr))
    {
        count       = pContent->m_count;
        pPayload    = pContent->m_pVaddr;
    }

    CheckAndIssueUnlock(needLock);

    return pPayload;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// MetaBuffer::RemoveTag
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult MetaBuffer::RemoveTag(
    UINT32 metadataTag,
    BOOL   needLock)
{
    CamxResult  result      = CamxResultSuccess;

    CheckAndIssueReadlock(needLock);

    Content*    pContent    = m_pMap->Find(metadataTag);
    if (NULL != pContent)
    {
        pContent->m_pVaddr = NULL;
    }
    else
    {
        result = CamxResultENoSuch;
        CAMX_LOG_ERROR(CamxLogGroupMeta, "Tag not found %x", metadataTag);
    }

    CheckAndIssueUnlock(needLock);

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// MetaBuffer::SetTag
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult MetaBuffer::SetTag(
    const camera_metadata_t* pMetadata,
    BOOL                     needLock)
{
    CamxResult result = CamxResultSuccess;

    CAMX_ASSERT(NULL != pMetadata);

    if (NULL != pMetadata)
    {
        // try to preallocate missing tags first
        AllocateBuffer(pMetadata);

        UINT32 totalEntries = static_cast<UINT32>(get_camera_metadata_entry_count(pMetadata));
        UINT32 totalSize    = 0;

        PropertyPackingInfo packingInfo;

        for (UINT32 index = 0; index < totalEntries; ++index)
        {
            camera_metadata_ro_entry_t entry;

            INT32 metadataStatus = get_camera_metadata_ro_entry(pMetadata, index, &entry);

            CheckAndIssueWritelock(needLock);

            if (MetadataUtilOK == metadataStatus)
            {
                if (m_propertyBlobId != entry.tag)
                {
                    UINT32 maxSize        = static_cast<UINT32>(HAL3MetadataUtil::GetMaxSizeByTag(entry.tag));
                    UINT32 entrySize      = static_cast<UINT32>(HAL3MetadataUtil::GetSizeByType(entry.type) * entry.count);
                    UINT32 entryCount     = static_cast<UINT32>(entry.count);
                    BOOL   needAllocation = FALSE;

                    if ((0 < entrySize) && (maxSize >= entrySize))
                    {

                        Content* pContent = m_pMap->Find(entry.tag);

                        if (NULL != pContent)
                        {
                            if (MaxInplaceTagSize >= pContent->m_maxSize)
                            {
                                pContent->m_pVaddr = pContent->m_data;
                                pContent->m_count  = entryCount;
                                pContent->m_size   = maxSize;
                                pContent->m_tag    = entry.tag;

                                Utils::Memcpy(pContent->m_pVaddr, entry.data.u8, entrySize);
                            }
                            else if (m_memoryRegions.size() > pContent->m_regionIndex)
                            {
                                MemoryRegion& memRegion = m_memoryRegions[pContent->m_regionIndex];

                                if ((NULL != memRegion.m_pVaddr) &&
                                    (pContent->m_offset + entrySize <= m_memoryRegions[pContent->m_regionIndex].m_size))
                                {
                                    pContent->m_pVaddr = memRegion.m_pVaddr + pContent->m_offset;
                                    pContent->m_count  = entryCount;
                                    pContent->m_size   = maxSize;
                                    pContent->m_tag    = entry.tag;

                                    Utils::Memcpy(pContent->m_pVaddr, entry.data.u8, entrySize);
                                }
                                else
                                {
                                    result = CamxResultEFailed;

                                    CAMX_LOG_ERROR(CamxLogGroupMeta, "Cannot set tag %x size %u max %u size %d addr %p",
                                        entry.tag, entrySize, maxSize, memRegion.m_size, memRegion.m_pVaddr);
                                }
                            }
                            else
                            {
                                result = CamxResultEFailed;

                                CAMX_LOG_ERROR(CamxLogGroupMeta, "Cannot set tag %x size %u max %u invalid region %u",
                                    entry.tag, entrySize, maxSize, pContent->m_regionIndex);
                            }
                        }
                        else
                        {
                            result = CamxResultEFailed;

                            CAMX_LOG_ERROR(CamxLogGroupMeta, "Cannot set %x size %u max %u cannot find content",
                                entry.tag, entrySize, maxSize);
                        }
                    }
                    else
                    {
                        CAMX_LOG_ERROR(CamxLogGroupMeta, "Cannot set %x size %u max %u",
                            entry.tag, entrySize, maxSize);
                    }
                }
                else
                {
                    // NOWHINE CP036a: Google API requires const type
                    HAL3MetadataUtil::UnPackBlobToPropertyInfo(&packingInfo, const_cast<UINT8*>(entry.data.u8));

                    if (0 != packingInfo.count)
                    {
                        CAMX_LOG_VERBOSE(CamxLogGroupMeta, "Received %d number of properties in blob", packingInfo.count);

                        for (UINT tagIndex = 0; tagIndex < packingInfo.count; tagIndex++)
                        {
                            const MetadataInfo* pInfo = HAL3MetadataUtil::GetMetadataInfoByTag(packingInfo.tagId[tagIndex]);

                            SetTag(packingInfo.tagId[tagIndex], packingInfo.pAddress[tagIndex], 1, pInfo->size);
                        }
                    }
                }
            }
            else
            {
                result = CamxResultEInvalidArg;
                CAMX_LOG_ERROR(CamxLogGroupMeta, "Cannot get metadata status %d", metadataStatus);
            }

            CheckAndIssueUnlock(needLock);
        }
    }
    else
    {
        result = CamxResultEInvalidArg;
        CAMX_LOG_ERROR(CamxLogGroupMeta, "NULL metadata");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// MetaBuffer::SetTag
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult MetaBuffer::SetTag(
    UINT32              metadataTag,
    const VOID*         pTagPayload,
    UINT32              tagCount,
    UINT32              tagSize,
    BOOL                needLock,
    const MetadataInfo* pMetadataInfo)
{
    CamxResult result         = CamxResultSuccess;
    BOOL       needAllocation = FALSE;

    // get metadata info if the caller doesnt pass the same
    if (NULL == pMetadataInfo)
    {
        pMetadataInfo = HAL3MetadataUtil::GetMetadataInfoByTag(metadataTag);
    }

    if ((0                   <  tagSize)        &&
        (pMetadataInfo->size >= tagSize)        &&
        (NULL                != pTagPayload)    &&
        (0                   <  tagCount))
    {
        CheckAndIssueWritelock(needLock);

        Content* pContent = m_pMap->Find(metadataTag);

        if (NULL == pContent)
        {
            UINT32 regionIndex = (MaxInplaceTagSize >= pMetadataInfo->size) ? InPlaceMemoryRegion : InvalidIndex;

            m_pMap->Insert(metadataTag,
                           regionIndex,
                           0,
                           tagSize,
                           NULL,
                           tagCount,
                           pMetadataInfo->index,
                           pMetadataInfo->size);

            pContent = m_pMap->Find(metadataTag);

            CAMX_ASSERT(NULL != pContent);
        }

        if (MaxInplaceTagSize >= pContent->m_maxSize)
        {
            if (tagSize <= pContent->m_maxSize)
            {
                pContent->m_pVaddr      = pContent->m_data;
                pContent->m_regionIndex = InPlaceMemoryRegion;
                pContent->m_tag         = metadataTag;
                pContent->m_count       = tagCount;
                pContent->m_size        = tagSize;
                pContent->m_cameraId    = m_cameraId;

                Utils::Memcpy(pContent->m_pVaddr, pTagPayload, tagSize);
            }
            else
            {
                result = CamxResultEFailed;
                CAMX_LOG_ERROR(CamxLogGroupMeta,
                               "Cannot set tag %x size %u count %u Maxsize %u",
                               metadataTag, tagSize, tagCount, pContent->m_maxSize);
            }
        }
        else
        {
            if (InvalidIndex == pContent->m_regionIndex)
            {
                UINT32 regionIndex;

                result = ReserveRegionAndAllocate(pContent->m_maxSize, regionIndex);

                if (CamxResultSuccess == result)
                {
                    pContent->m_regionIndex = regionIndex;
                    pContent->m_offset      = 0;
                    pContent->m_tag         = metadataTag;
                    pContent->m_cameraId    = m_cameraId;
                }
            }

            if (pContent->m_regionIndex < static_cast<UINT32>(m_memoryRegions.size()))
            {
                MemoryRegion& memRegion = m_memoryRegions[pContent->m_regionIndex];

                if ((NULL != memRegion.m_pVaddr) && (pContent->m_offset + tagSize <= memRegion.m_size))
                {
                    pContent->m_pVaddr      = memRegion.m_pVaddr + pContent->m_offset;
                    pContent->m_count       = tagCount;
                    pContent->m_size        = tagSize;
                    pContent->m_cameraId    = m_cameraId;

                    Utils::Memcpy(pContent->m_pVaddr, pTagPayload, tagSize);
                }
                else
                {
                    result = CamxResultEFailed;

                    CAMX_LOG_ERROR(CamxLogGroupMeta,
                                   "Cannot set tag %x size %u count %u size %d addr %p index %d offset %d",
                                   metadataTag, tagSize, tagCount, memRegion.m_size,
                                   memRegion.m_pVaddr, pContent->m_regionIndex,
                                   pContent->m_offset);
                }
            }
            else
            {
                result = CamxResultEFailed;

                CAMX_LOG_ERROR(CamxLogGroupMeta, "Cannot set tag %x size %u count region index %d",
                               metadataTag, tagSize, tagCount, pContent->m_regionIndex);
            }
        }

        CheckAndIssueUnlock(needLock);
    }
    else
    {
        result = CamxResultEFailed;
        CAMX_LOG_ERROR(CamxLogGroupMeta, "Invalid tag %x size %u maxSize %u",
                       metadataTag, tagSize, pMetadataInfo->size);
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// MetaBuffer::AddReference
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
UINT32 MetaBuffer::AddReference(
    UINT32      clientID,
    BOOL        isExternal)
{
    m_pClientLock->Lock();

    UINT32& refCount = (TRUE == isExternal) ? m_externalRefCount : m_internalRefCount;

    refCount++;

    UINT32 index;
    UINT32 totalRefCount = m_mergeRefCount + m_internalRefCount + m_externalRefCount;

    for (index = 0; index < m_metaBufferClients.size(); ++index)
    {
        if ((InvalidClient == m_metaBufferClients[index].clientID) &&
            (NULL == m_metaBufferClients[index].pMetaBuffer))
        {
            break;
        }
    }

    if (index == m_metaBufferClients.size())
    {
        CAMX_LOG_ERROR(CamxLogGroupMeta, "Exceeds the max number of clients %d", clientID);
    }
    else
    {
        m_metaBufferClients[index].clientID    = clientID;
        m_metaBufferClients[index].pMetaBuffer = NULL;
    }

    CAMX_LOG_INFO(CamxLogGroupMeta, "Add Reference Count merge %d int %d ext %d mb cur %p",
                   m_mergeRefCount, m_internalRefCount, m_externalRefCount, this);

    m_pClientLock->Unlock();

    return totalRefCount;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// MetaBuffer::AddReference
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
UINT32 MetaBuffer::AddReference(
    MetaBuffer* pMetaBuffer)
{
    m_pClientLock->Lock();
    m_mergeRefCount++;

    UINT32 index;
    UINT32 totalRefCount = m_mergeRefCount + m_externalRefCount + m_internalRefCount;

    for (index = 0; index < m_metaBufferClients.size(); ++index)
    {
        if ((InvalidClient == m_metaBufferClients[index].clientID) &&
            (NULL == m_metaBufferClients[index].pMetaBuffer))
        {
            break;
        }
    }

    if (index == m_metaBufferClients.size())
    {
        CAMX_LOG_ERROR(CamxLogGroupMeta, "Exceeds the max number of clients");
    }
    else
    {
        m_metaBufferClients[index].clientID    = InvalidClient;
        m_metaBufferClients[index].pMetaBuffer = pMetaBuffer;
    }

    CAMX_LOG_INFO(CamxLogGroupMeta, "Add Reference Count merge %d int %d ext %d mb dep %p cur %p",
                   m_mergeRefCount, m_internalRefCount, m_externalRefCount, pMetaBuffer, this);

    m_pClientLock->Unlock();

    return totalRefCount;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// MetaBuffer::ReleaseReference
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
UINT32 MetaBuffer::ReleaseReference(
    UINT32      clientID,
    BOOL        isExternal)
{
    UINT32 totalRefCount = 0;

    m_pClientLock->Lock();

    totalRefCount = m_mergeRefCount + m_internalRefCount + m_externalRefCount;

    UINT32& refCount =  (TRUE == isExternal) ? m_externalRefCount : m_internalRefCount;

    if (0 < refCount)
    {
        refCount--;

        totalRefCount = m_mergeRefCount + m_internalRefCount + m_externalRefCount;

        UINT32 index;

        for (index = 0; index < m_metaBufferClients.size(); ++index)
        {
            if (clientID == m_metaBufferClients[index].clientID)
            {
                break;
            }
        }

        if (index == m_metaBufferClients.size())
        {
            CAMX_LOG_ERROR(CamxLogGroupMeta, "Cannot find client %d refCnt %d %p", clientID, refCount, this);
        }
        else
        {
            m_metaBufferClients[index].clientID    = InvalidClient;
            m_metaBufferClients[index].pMetaBuffer = NULL;
        }

        if ((0 == totalRefCount) && (TRUE == m_invalidatePending))
        {
            Invalidate();
        }

        CAMX_LOG_INFO(CamxLogGroupMeta, "Reference Count merge %d int %d ext %d mb cur %p",
                      m_mergeRefCount,
                      m_internalRefCount,
                      m_externalRefCount,
                      this);
    }
    else
    {
        CAMX_LOG_INFO(CamxLogGroupMeta, "ERROR Reference Count merge %d int %d ext %d mb cur %p",
                      m_mergeRefCount,
                      m_internalRefCount,
                      m_externalRefCount,
                      this);
    }

    m_pClientLock->Unlock();

    return totalRefCount;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// MetaBuffer::ReleaseReference
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
UINT32 MetaBuffer::ReleaseReference(
    MetaBuffer* pMetaBuffer)
{
    UINT32 totalRefCount = 0;

    m_pClientLock->Lock();

    if (0 < m_mergeRefCount)
    {
        m_mergeRefCount--;

        totalRefCount = m_mergeRefCount + m_internalRefCount + m_externalRefCount;

        UINT32 index;

        for (index = 0; index < m_metaBufferClients.size(); ++index)
        {
            if (pMetaBuffer == m_metaBufferClients[index].pMetaBuffer)
            {
                break;
            }
        }

        if (index == m_metaBufferClients.size())
        {
            CAMX_LOG_ERROR(CamxLogGroupMeta, "Cannot find metabuffer %p refCnt %d cur %p",
                           pMetaBuffer, totalRefCount, this);
        }
        else
        {
            m_metaBufferClients[index].clientID = InvalidClient;
            m_metaBufferClients[index].pMetaBuffer = NULL;
        }

        if ((0 == totalRefCount) && (TRUE == m_invalidatePending))
        {
            Invalidate();
        }

        CAMX_LOG_INFO(CamxLogGroupMeta, "Reference Count merge %d int %d ext %d mb dep %p cur %p",
                      m_mergeRefCount,
                      m_internalRefCount,
                      m_externalRefCount,
                      pMetaBuffer,
                      this);
    }
    else
    {
        CAMX_LOG_INFO(CamxLogGroupMeta, "ERROR Reference Count merge %d int %d ext %d mb dep %p cur %p",
                      m_mergeRefCount,
                      m_internalRefCount,
                      m_externalRefCount,
                      pMetaBuffer,
                      this);
    }

    m_pClientLock->Unlock();

    return totalRefCount;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// MetaBuffer::ReleaseAllReferences
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID MetaBuffer::ReleaseAllReferences(
    BOOL bExternalAndInternalReferences)
{
    m_pClientLock->Lock();

    if (TRUE == bExternalAndInternalReferences)
    {
        m_mergeRefCount     = 0;
        m_internalRefCount  = 0;
        m_externalRefCount  = 0;

        for (UINT32 index = 0; index < m_metaBufferClients.size(); ++index)
        {
            ClientInfo& clientInfo = m_metaBufferClients[index];

            clientInfo.clientID    = InvalidClient;
            clientInfo.pMetaBuffer = NULL;
        }

        // remove the usage of buffer
        for (vector<Link>::iterator pLink = m_metaBufferDependentLinks.begin();
             pLink != m_metaBufferDependentLinks.end(); ++pLink)
        {
            pLink->m_pMetaBuffer = NULL;
        }

        Invalidate();
    }
    else
    {
        m_externalRefCount  = 0;

        UINT32 totalRefCount = m_mergeRefCount + m_internalRefCount + m_externalRefCount;

        for (UINT32 index = 0; index < m_metaBufferClients.size(); ++index)
        {
            ClientInfo& clientInfo = m_metaBufferClients[index];

            if ((NULL == clientInfo.pMetaBuffer) &&
                (0 == (clientInfo.clientID & MetaBufferUsageMask)))
            {
                clientInfo.clientID    = InvalidClient;
            }
        }

        if ((0 == totalRefCount) && (TRUE == m_invalidatePending))
        {
            Invalidate();
        }
    }

    m_pClientLock->Unlock();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// MetaBuffer::Capacity
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
UINT32 MetaBuffer::Capacity() const
{
    UINT32 totalSize = 0;
    for (vector<MemoryRegion>::const_iterator regionIterator = m_memoryRegions.cbegin();
         regionIterator != m_memoryRegions.cend();
         ++regionIterator)
    {
        totalSize += regionIterator->m_size;
    }

    return totalSize;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// MetaBuffer::Count
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
UINT32 MetaBuffer::Count()
{
    return m_pMap->Count();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// MetaBuffer::Clone
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
MetaBuffer* MetaBuffer::Clone()
{
    MetaBuffer* pDstMetaBuffer = MetaBuffer::Create(NULL);

    if (NULL != pDstMetaBuffer)
    {
        CamxResult  result = pDstMetaBuffer->Copy(this, FALSE);
        if (CamxResultSuccess != result)
        {
            CAMX_LOG_ERROR(CamxLogGroupMeta, "Cannot copy %d", result);
            pDstMetaBuffer->Destroy();
            pDstMetaBuffer = NULL;
        }
    }
    return pDstMetaBuffer;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// MetaBuffer::GetAndroidMeta
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult MetaBuffer::GetAndroidMeta(
    camera_metadata_t* pAndroidMeta,
    BOOL               externalTagsOnly,
    BOOL               filterProperTies,
    UINT32             filterTagCount,
    UINT32*            pFilterTagArray)
{
    CamxResult result = CamxResultSuccess;
    CAMX_ASSERT(NULL != pAndroidMeta);

    if (NULL != pAndroidMeta)
    {
        unordered_set<UINT32> filterTagSet;

        for (UINT32 tagIndex = 0; tagIndex < filterTagCount; ++tagIndex)
        {
            filterTagSet.insert(pFilterTagArray[tagIndex]);
        }

        result = m_pMap->GetAndroidMeta(pAndroidMeta,
                                        m_propertyBlobId,
                                        externalTagsOnly,
                                        filterProperTies,
                                        filterTagSet);
    }
    else
    {
        result = CamxResultEInvalidArg;
        CAMX_LOG_ERROR(CamxLogGroupMeta, "NULL metadata");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// MetaBuffer::CombineMultiCameraMetadata
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult MetaBuffer::CombineMultiCameraMetadata(
    UINT32       metabufferCount,
    UINT32*      pCameraIdArray,
    MetaBuffer** ppMetaBufferArray,
    UINT32       primaryCameraId)
{
    MetaBuffer* pPrimaryMeta = NULL;
    CamxResult  result       = CamxResultSuccess;

    if (0 == m_cameraIdMap.size())
    {
        for (UINT32 index = 0; index < metabufferCount; ++index)
        {
            MetaBuffer* pSrcMetaBuffer = ppMetaBufferArray[index];
            UINT32&     cameraId       = pCameraIdArray[index];

            if ((NULL != pSrcMetaBuffer) &&
                (InvalidCameraId != cameraId) &&
                (m_cameraIdMap.end() == m_cameraIdMap.find(cameraId)))
            {
                result = AddToFreeLink(pSrcMetaBuffer);

                if (CamxResultSuccess == result)
                {
                    pSrcMetaBuffer->AddReference(this);
                    pSrcMetaBuffer->m_cameraId = cameraId;
                    m_cameraIdMap.insert({cameraId, pSrcMetaBuffer});

                    // update the cameraId for the tags
                    pSrcMetaBuffer->m_pMap->UpdateCameraId(cameraId);

                    if (primaryCameraId == cameraId)
                    {
                        pPrimaryMeta = pSrcMetaBuffer;
                    }
                }
            }
            else
            {
                result = CamxResultEInvalidArg;
                CAMX_LOG_ERROR(CamxLogGroupMeta, "Invalid cameraId or metadata provided %p id %d",
                               pSrcMetaBuffer, cameraId);
                break;
            }
        }

        if (NULL != pPrimaryMeta)
        {
            m_pMap->MergeDisjoint(pPrimaryMeta->m_pMap, pPrimaryMeta);
            m_pCameraIdSubTree = this;
        }
        else
        {
            result = CamxResultEInvalidArg;
            CAMX_LOG_ERROR(CamxLogGroupMeta, "Primary CameraId %d not found for %p",
                           primaryCameraId, this);
        }
    }
    else
    {
        result = CamxResultEInvalidState;
        CAMX_LOG_ERROR(CamxLogGroupMeta, "Duplicate Combine request %p", this);
    }
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// MetaBuffer::PrintDetails
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID MetaBuffer::PrintDetails()
{
    queue<MetaBuffer*> bfsQueue;
    CamxResult         result    = CamxResultENoSuch;
    UINT32             depth     = 0;

    Log(NULL);
    bfsQueue.push(this);

    while (!bfsQueue.empty() && (MaxGraphDepth > depth++))
    {
        MetaBuffer* pCurrentMeta = bfsQueue.front();
        bfsQueue.pop();

        vector<Link>::iterator pLink;
        for (pLink = pCurrentMeta->m_metaBufferDependentLinks.begin();
             pLink != pCurrentMeta->m_metaBufferDependentLinks.end(); ++pLink)
        {
            if (NULL != pLink->m_pMetaBuffer)
            {
                pLink->m_pMetaBuffer->Log(pCurrentMeta);
                bfsQueue.push(pLink->m_pMetaBuffer);
            }
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// MetaBuffer::DumpDetailsToFile
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID MetaBuffer::DumpDetailsToFile(
    const CHAR* pFilename)
{
    UINT32 dependeeCount = 0;
    CHAR  dumpFilename[256];

    if (NULL == pFilename)
    {
        OsUtils::SNPrintF(dumpFilename,
            sizeof(dumpFilename),
            "%s/metadata/metadata_%x.txt",
            ConfigFileDirectory,
            GetUniqueID());
    }
    else
    {
        OsUtils::SNPrintF(dumpFilename,
            sizeof(dumpFilename),
            "%s/metadata/%s",
            ConfigFileDirectory,
            pFilename);
    }


    FILE* pFile = OsUtils::FOpen(dumpFilename, "wb");
    if (NULL != pFile)
    {
        CAMX_LOG_VERBOSE(CamxLogGroupMeta, "Metadata %p file name %s", this, dumpFilename);

        // Update number of regions
        OsUtils::FPrintF(pFile, "MetaBuffer %x mapType %d\n", m_uniqueId, m_mapType);
        OsUtils::FPrintF(pFile, "Memory regions %u \n", m_memoryRegions.size());
        for (UINT32 regionIndex = 0; regionIndex < m_memoryRegions.size(); ++regionIndex)
        {
            OsUtils::FPrintF(pFile, "  Memory region[%d] addr %p size %u\n", regionIndex,
                m_memoryRegions[regionIndex].m_pVaddr,
                m_memoryRegions[regionIndex].m_size);
        }

        OsUtils::FPrintF(pFile, "Reference Count %d\n", ReferenceCount());
        OsUtils::FPrintF(pFile, "Client Count %u\n", m_metaBufferClients.size());
        for (UINT32 clientIndex = 0; clientIndex < m_metaBufferClients.size(); ++clientIndex)
        {
            if (InvalidClient != m_metaBufferClients[clientIndex].clientID)
            {
                OsUtils::FPrintF(pFile, "  Client[%d] cliendID %x numReferences %u\n", clientIndex,
                    m_metaBufferClients[clientIndex].clientID,
                    m_metaBufferClients[clientIndex].numOfReferences);
            }
        }

        OsUtils::FPrintF(pFile, "Dependent MaxCount %u\n", m_metaBufferDependentLinks.size());
        for (UINT32 depIndex = 0; depIndex < m_metaBufferDependentLinks.size(); ++depIndex)
        {
            if (m_metaBufferDependentLinks[depIndex].m_pMetaBuffer)
            {
                OsUtils::FPrintF(pFile, "  Dependee[%d] Dependee %x numReferences %u\n", dependeeCount++,
                    m_metaBufferDependentLinks[depIndex].m_pMetaBuffer->GetUniqueID(),
                    m_metaBufferDependentLinks[depIndex].m_pMetaBuffer->ReferenceCount());
            }
        }

        m_pMap->Dump(pFile, false);

        OsUtils::FClose(pFile);
    }
    else
    {
        CAMX_LOG_INFO(CamxLogGroupMeta, "Cannot dump Metadata... file name %s", dumpFilename);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MetaBuffer::CreateIterator
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
MetaBuffer::Iterator* MetaBuffer::CreateIterator()
{
    Iterator* pIterator = CAMX_NEW MetaBuffer::Iterator(*this);

    if (NULL != pIterator)
    {
        if (MapType::Hash == m_mapType)
        {
            HashMap* pMap               = static_cast<HashMap*>(m_pMap);
            pIterator->m_pMapIterator   = CAMX_NEW HashMap::HashIterator(*pMap);
        }
        else
        {
            LinearMap* pMap             = static_cast<LinearMap*>(m_pMap);
            pIterator->m_pMapIterator   = CAMX_NEW LinearMap::LinearIterator(*pMap);
        }
    }

    return pIterator;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MetaBuffer::Iterator::Iterator
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
MetaBuffer::Iterator::Iterator(
    MetaBuffer &metaBuffer)
    : m_metaBuffer(metaBuffer)
    , m_pMapIterator(NULL)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MetaBuffer::Iterator::~Iterator
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
MetaBuffer::Iterator::~Iterator()
{
    if (NULL != m_pMapIterator)
    {
        CAMX_DELETE m_pMapIterator;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MetaBuffer::Iterator::Begin
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult MetaBuffer::Iterator::Begin()
{
    return m_pMapIterator->Begin();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MetaBuffer::Iterator::HasDone
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL MetaBuffer::Iterator::HasDone()
{
    return m_pMapIterator->HasDone();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MetaBuffer::Iterator::Next
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult MetaBuffer::Iterator::Next()
{
    return m_pMapIterator->Next();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//  MetaBuffer::Iterator::GetEntry
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult MetaBuffer::Iterator::GetEntry(
    Entry& entry)
{
    return m_pMapIterator->GetEntry(entry);
}

CAMX_NAMESPACE_END
