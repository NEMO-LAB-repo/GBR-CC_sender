//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// Cellular Ratio Receiver - receives ratio data from cellular diag logger
//

#ifndef _CELLULAR_RATIO_H_
#define _CELLULAR_RATIO_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//
// Socket path for receiving ratio data from Python cellular logger
//
#define CELLULAR_RATIO_SOCKET_PATH "/tmp/msquic_cellular_ratio.sock"

//
// Control mode: 0 = log only, 1 = log + control
//
extern volatile int g_CellularControlEnabled;
extern volatile double g_CellularRawRatioValue;
extern volatile uint64_t g_CellularRatioUpdateSeq;

//
// Initialize the cellular ratio receiver
// Creates a Unix domain socket and starts a receiver thread
//
bool CellularRatioInit(void);

//
// Cleanup the cellular ratio receiver
// Stops the receiver thread and closes the socket
//
void CellularRatioCleanup(void);

//
// Get the latest smoothed ratio value (for BBR congestion control)
// Returns EMA-smoothed ratio with high values clamped to 1.0
// Returns 1.0 if no ratio has been received yet
//
double CellularRatioGet(void);

//
// Get the raw (unsmoothed) ratio value (for logging/debugging)
// Returns the most recent raw ratio received from the cellular logger
//
double CellularRatioGetRaw(void);

//
// Check if ratio data is available (received at least once)
//
bool CellularRatioIsAvailable(void);

//
// Get statistics about received ratio data
//
typedef struct CELLULAR_RATIO_STATS {
    uint64_t PacketsReceived;
    uint64_t LastUpdateTimeUs;
    double LastRatio;           // Raw ratio (unsmoothed)
    double SmoothedRatio;       // EMA smoothed ratio (what BBR uses)
    double MinRatio;
    double MaxRatio;
    double AvgRatio;
} CELLULAR_RATIO_STATS;

void CellularRatioGetStats(CELLULAR_RATIO_STATS* Stats);

#ifdef __cplusplus
}
#endif

#endif // _CELLULAR_RATIO_H_
