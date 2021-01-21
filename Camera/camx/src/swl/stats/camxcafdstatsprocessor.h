////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016-2019 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxcafdstatsprocessor.h
/// @brief The class that implements IStatsProcessor for AFD.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef CAMXCAFDSTATSPROCESSOR_H
#define CAMXCAFDSTATSPROCESSOR_H

#include "chiafdinterface.h"
#include "chistatsproperty.h"
#include "camxcafdalgorithmhandler.h"
#include "camxcafdiohandler.h"
#include "camxistatsprocessor.h"
#include "camxstatsparser.h"
#include "camxutils.h"

static const UINT DefaultVNumConfig          = 1024; ///< Define default config for VNum when AFD disabled
static const UINT DefaultHNumConfig          = 8;    ///< Define default config for HNum when AFD disabled

CAMX_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief The class that implements IStatsProcessor for AFD.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CAFDStatsProcessor final : public IStatsProcessor
{
public:
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Create
    ///
    /// @brief  Create the object for CAFDStatsProcessor.
    ///
    /// @param  ppAFDStatsProcessor Pointer to CAFDStatsProcessor
    ///
    /// @return CamxResultSuccess if successful
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static CamxResult Create(
        IStatsProcessor** ppAFDStatsProcessor);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Initialize
    ///
    /// @brief  Used to initialize the class.
    ///
    /// @param  pInitializeData Pointer to inital settings
    ///
    /// @return CamxResultSuccess if successful
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual CamxResult Initialize(
        const StatsInitializeData* pInitializeData);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// ExecuteProcessRequest
    ///
    /// @brief  Executes the algoritm for a request.
    ///
    /// @param  pStatsProcessRequestDataInfo Pointer to process request information
    ///
    /// @return CamxResultSuccess if successful
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual CamxResult ExecuteProcessRequest(
        const StatsProcessRequestData* pStatsProcessRequestDataInfo);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// GetDependencies
    ///
    /// @brief  Get the the list of dependencies from all stats processors.
    ///
    /// @param  pStatsProcessRequestDataInfo    Pointer to process request information
    /// @param  pStatsDependency                Pointer to stats depedencies
    ///
    /// @return CamxResultSuccess if successful
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual CamxResult GetDependencies(
        const StatsProcessRequestData* pStatsProcessRequestDataInfo,
        StatsDependency*               pStatsDependency);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// GetPublishList
    ///
    /// @brief  Get the the list of tags published by all stats processors.
    ///
    /// @param  maxTagArraySize  Maximum size of pTagArray
    /// @param  pTagArray        Array of tags that are published by the stats processor.
    /// @param  pTagCount        Number of tags published by the stats processor
    /// @param  pPartialTagCount Number of Partialtags published by the stats processor
    ///
    /// @return CamxResultSuccess if successful, return failure if the number of tags to be published exceeds maxTagArraySize
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual CamxResult GetPublishList(
        const UINT32    maxTagArraySize,
        UINT32*         pTagArray,
        UINT32*         pTagCount,
        UINT32*         pPartialTagCount);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// IsDependenciesSatisfied
    ///
    /// @brief  Checks if the depedencies is satisfied for a family.
    ///
    /// @param  pStatsProcessRequestDataInfo    Pointer to process request information
    /// @param  pIsSatisfied                    Pointer to Is depedencies satisfied
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual CamxResult IsDependenciesSatisfied(
        const StatsProcessRequestData* pStatsProcessRequestDataInfo,
        BOOL*                          pIsSatisfied);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// SetAFDAlgorithmHandler
    ///
    /// @brief  Set AFDAlgoHandler Object.
    ///
    /// @param  pAlgoHandler    Pointer to AFDAlgoHandler  Object
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID SetAFDAlgorithmHandler(
        CAFDAlgorithmHandler* pAlgoHandler);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// SetAFDIOHandler
    ///
    /// @brief  Set AFD Input/Output Object.
    ///
    /// @param  pAFDIOHandler    Pointer to Input/Output Object
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID SetAFDIOHandler(
        CAFDIOHandler* pAFDIOHandler);

protected:
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// ~CAFDStatsProcessor
    ///
    /// @brief  destructor
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual ~CAFDStatsProcessor();

private:
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// CAFDStatsProcessor
    ///
    /// @brief  Constructor.
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CAFDStatsProcessor();

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// PrepareInputParams
    ///
    /// @brief  Gets the required inputs for the algorithm
    ///
    /// @param  pRequestId      Pointer to Request Id
    /// @param  pInput          Pointer to the input data
    ///
    /// @return Success or failure
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult PrepareInputParams(
        UINT64*           pRequestId,
        AFDAlgoInputList* pInput);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// ReadHALAFDParam
    ///
    /// @brief  Read the HAL's input for AFD
    ///
    /// @param  pHALParam The pointer AECEngineHALParam to save the HAL parameters
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID ReadHALAFDParam(
        AFDHALParam* pHALParam
    ) const;

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// ProcessSetParams
    ///
    /// @brief  Sends the required set parameters for the core algorithm
    ///
    /// @param  pInputTuningModeData Pointer to Chi tuning data
    ///
    /// @return Success or failure
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult ProcessSetParams(
        ChiTuningModeParameter* pInputTuningModeData);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// SetAlgoChromatix
    ///
    /// @brief  Sets the required inputs parameters for the core algorithm
    ///
    /// @param  pInputTuningModeData Pointer to Chi Tuning mode
    /// @param  pTuningData          Pointer to o/p data
    ///
    /// @return Success or failure
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult SetAlgoChromatix(
        ChiTuningModeParameter* pInputTuningModeData,
        StatsTuningData*        pTuningData);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// GetPrevConfigFromAlgo
    ///
    /// @brief  Gets Configuration from Algo
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID GetPrevConfigFromAlgo();

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// UpdateInputParam
    ///
    /// @brief  Function to set input into algorithm input handle
    ///
    /// @param  pInputList  List of input parameter
    /// @param  inputType   input parameter type
    /// @param  inputSize   size of input parameter
    /// @param  pValue      payload of the input parameter
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CAMX_INLINE VOID UpdateInputParam(
        AFDAlgoInputList* pInputList,
        AFDAlgoInputType  inputType,
        UINT32            inputSize,
        VOID*             pValue);

    CAFDStatsProcessor(const CAFDStatsProcessor&)               = delete;   ///< Do not implement copy constructor
    CAFDStatsProcessor& operator=(const CAFDStatsProcessor&)    = delete;   ///< Do not implement assignment operator

    CAFDAlgorithmHandler* m_pAFDAlgorithmHandler;                 ///< Algorithm Handler
    CAFDIOHandler*        m_pAFDIOHandler;                        ///< Property Pool Input/output handler
    AFDAlgoInput          m_inputArray[AFDInputTypeLastIndex];    ///< Structure to the input parameter for the
                                                                       /// interface
    CREATEAFD             m_pfnCreate;                            ///< Function Pointer to create Algorithm instance
    AFDHALParam           m_HALParam;                             ///< HAL params coming to AFD
    VOID*                 m_pTuningDataManager;                   ///< Tuning data manager
    const StaticSettings* m_pStaticSettings;                      ///< Pointer to camera settings
    Node*                 m_pNode;                                ///< Pointer to owning StatsProcessor node
    CamX::OSLIBRARYHANDLE m_hHandle;                              ///< Handle for algo lib.
};

CAMX_NAMESPACE_END

#endif // CAMXCAFDSTATSPROCESSOR_H
