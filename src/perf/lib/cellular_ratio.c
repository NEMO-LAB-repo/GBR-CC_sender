//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// Cellular Ratio Receiver - receives ratio data from cellular diag logger
//

#include "cellular_ratio.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <time.h>

//
// Exported global variable for BBR to read
// This avoids complex include dependencies between core and perf modules
//
volatile double g_CellularRatioValue = 1.0;
volatile double g_CellularRawRatioValue = 1.0;
volatile uint64_t g_CellularRatioUpdateSeq = 0;
volatile int g_CellularRatioAvailable = 0;
volatile int g_CellularControlEnabled = 0;  // 0 = log only, 1 = log + control
volatile int g_TestModeEnabled = 0;         // 1 = linear rate ramp test mode

//
// EMA smoothing parameters
//
#define CELLULAR_RATIO_EMA_ALPHA 0.6      // Smoothing coefficient (0.6 = 60% new, 40% history)
#define CELLULAR_RATIO_MAX_EFFECTIVE 1.0  // Clamp ratio to this max before smoothing

//
// Global state for cellular ratio receiver
//
static struct {
    int SocketFd;
    pthread_t ReceiverThread;
    volatile bool Running;
    volatile bool Initialized;
    volatile bool DataAvailable;

    // Latest ratio data (protected by mutex)
    pthread_mutex_t Lock;
    double CurrentRatio;         // Raw ratio from external source
    double SmoothedRatio;        // EMA smoothed ratio (this is what BBR uses)

    // Statistics
    uint64_t PacketsReceived;
    uint64_t LastUpdateTimeUs;
    double MinRatio;
    double MaxRatio;
    double SumRatio;
} g_CellularRatio = {
    .SocketFd = -1,
    .Running = false,
    .Initialized = false,
    .DataAvailable = false,
    .CurrentRatio = 1.0,
    .SmoothedRatio = 1.0,
    .PacketsReceived = 0,
    .LastUpdateTimeUs = 0,
    .MinRatio = 100.0,
    .MaxRatio = 0.0,
    .SumRatio = 0.0
};

//
// Get current time in microseconds
//
static uint64_t GetTimeUs(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000 + (uint64_t)ts.tv_nsec / 1000;
}

//
// Receiver thread function
//
static void* ReceiverThreadFunc(void* arg) {
    (void)arg;

    // Packet format: ratio (double, 8 bytes)
    char buffer[sizeof(double)];
    struct sockaddr_un client_addr;
    socklen_t client_len;

    printf("[CellularRatio] Receiver thread started, listening on %s\n", CELLULAR_RATIO_SOCKET_PATH);

    while (g_CellularRatio.Running) {
        client_len = sizeof(client_addr);

        // Receive data (blocking with timeout)
        ssize_t recv_len = recvfrom(g_CellularRatio.SocketFd, buffer, sizeof(buffer), 0,
                                     (struct sockaddr*)&client_addr, &client_len);

        if (recv_len < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // Timeout, check if we should stop
                continue;
            }
            if (g_CellularRatio.Running) {
                // perror("[CellularRatio] recvfrom failed");
            }
            continue;
        }

        if (recv_len >= (ssize_t)sizeof(double)) {
            // Parse ratio.
            double ratio;
            memcpy(&ratio, buffer, sizeof(double));

            // Update global state
            pthread_mutex_lock(&g_CellularRatio.Lock);
            g_CellularRatio.CurrentRatio = ratio;  // Store raw ratio
            g_CellularRatio.DataAvailable = true;
            g_CellularRatio.PacketsReceived++;
            g_CellularRatio.LastUpdateTimeUs = GetTimeUs();

            //
            // Apply EMA smoothing with ratio clamping
            // 1. Clamp high ratio (≥1.0) to prevent excessive speed increase
            // 2. Apply exponential moving average for smooth transitions
            //
            double effectiveRatio = ratio;
            if (effectiveRatio > CELLULAR_RATIO_MAX_EFFECTIVE) {
                effectiveRatio = CELLULAR_RATIO_MAX_EFFECTIVE;
            }

            // EMA: smoothed = alpha * new + (1-alpha) * old
            g_CellularRatio.SmoothedRatio =
                CELLULAR_RATIO_EMA_ALPHA * effectiveRatio +
                (1.0 - CELLULAR_RATIO_EMA_ALPHA) * g_CellularRatio.SmoothedRatio;

            // Update exported global variable for BBR (use smoothed value)
            g_CellularRawRatioValue = ratio;
            g_CellularRatioValue = g_CellularRatio.SmoothedRatio;
            g_CellularRatioUpdateSeq++;
            g_CellularRatioAvailable = 1;

            // Update statistics (using raw ratio for stats)
            if (ratio < g_CellularRatio.MinRatio) {
                g_CellularRatio.MinRatio = ratio;
            }
            if (ratio > g_CellularRatio.MaxRatio) {
                g_CellularRatio.MaxRatio = ratio;
            }
            g_CellularRatio.SumRatio += ratio;
            pthread_mutex_unlock(&g_CellularRatio.Lock);

            // Log every 100th packet (show both raw and smoothed)
            if (g_CellularRatio.PacketsReceived % 100 == 1) {
                printf("[CellularRatio] raw=%.3f -> smoothed=%.3f (clamped to %.1f, alpha=%.1f)\n",
                       ratio, g_CellularRatio.SmoothedRatio,
                       CELLULAR_RATIO_MAX_EFFECTIVE, CELLULAR_RATIO_EMA_ALPHA);
            }
        }
    }

    printf("[CellularRatio] Receiver thread stopped\n");
    return NULL;
}

//
// Initialize the cellular ratio receiver
//
bool CellularRatioInit(void) {
    if (g_CellularRatio.Initialized) {
        printf("[CellularRatio] Already initialized\n");
        return true;
    }

    // Initialize mutex
    if (pthread_mutex_init(&g_CellularRatio.Lock, NULL) != 0) {
        perror("[CellularRatio] Failed to initialize mutex");
        return false;
    }

    // Create Unix domain socket (DGRAM for receiving datagrams)
    g_CellularRatio.SocketFd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (g_CellularRatio.SocketFd < 0) {
        perror("[CellularRatio] Failed to create socket");
        pthread_mutex_destroy(&g_CellularRatio.Lock);
        return false;
    }

    // Remove existing socket file if it exists
    unlink(CELLULAR_RATIO_SOCKET_PATH);

    // Bind to socket path
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, CELLULAR_RATIO_SOCKET_PATH, sizeof(addr.sun_path) - 1);

    if (bind(g_CellularRatio.SocketFd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("[CellularRatio] Failed to bind socket");
        close(g_CellularRatio.SocketFd);
        g_CellularRatio.SocketFd = -1;
        pthread_mutex_destroy(&g_CellularRatio.Lock);
        return false;
    }

    // Set socket timeout (1 second)
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    setsockopt(g_CellularRatio.SocketFd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    // Start receiver thread
    g_CellularRatio.Running = true;
    if (pthread_create(&g_CellularRatio.ReceiverThread, NULL, ReceiverThreadFunc, NULL) != 0) {
        perror("[CellularRatio] Failed to create receiver thread");
        close(g_CellularRatio.SocketFd);
        g_CellularRatio.SocketFd = -1;
        g_CellularRatio.Running = false;
        pthread_mutex_destroy(&g_CellularRatio.Lock);
        return false;
    }

    g_CellularRatio.Initialized = true;
    printf("[CellularRatio] Initialized successfully, socket: %s\n", CELLULAR_RATIO_SOCKET_PATH);

    return true;
}

//
// Cleanup the cellular ratio receiver
//
void CellularRatioCleanup(void) {
    if (!g_CellularRatio.Initialized) {
        return;
    }

    printf("[CellularRatio] Cleaning up...\n");

    // Stop receiver thread
    g_CellularRatio.Running = false;

    // Wait for thread to finish
    pthread_join(g_CellularRatio.ReceiverThread, NULL);

    // Close socket
    if (g_CellularRatio.SocketFd >= 0) {
        close(g_CellularRatio.SocketFd);
        g_CellularRatio.SocketFd = -1;
    }

    // Remove socket file
    unlink(CELLULAR_RATIO_SOCKET_PATH);

    // Destroy mutex
    pthread_mutex_destroy(&g_CellularRatio.Lock);

    // Print final statistics
    if (g_CellularRatio.PacketsReceived > 0) {
        double avgRatio = g_CellularRatio.SumRatio / g_CellularRatio.PacketsReceived;
        printf("[CellularRatio] Final stats: packets=%lu, ratio min=%.3f max=%.3f avg=%.3f\n",
               (unsigned long)g_CellularRatio.PacketsReceived,
               g_CellularRatio.MinRatio,
               g_CellularRatio.MaxRatio,
               avgRatio);
    }

    g_CellularRatio.Initialized = false;
    printf("[CellularRatio] Cleanup complete\n");
}

//
// Get the latest smoothed ratio value (for BBR congestion control)
//
double CellularRatioGet(void) {
    double ratio = 1.0;
    pthread_mutex_lock(&g_CellularRatio.Lock);
    ratio = g_CellularRatio.SmoothedRatio;  // Return smoothed value
    pthread_mutex_unlock(&g_CellularRatio.Lock);
    return ratio;
}

//
// Get the raw (unsmoothed) ratio value (for logging/debugging)
//
double CellularRatioGetRaw(void) {
    double ratio = 1.0;
    pthread_mutex_lock(&g_CellularRatio.Lock);
    ratio = g_CellularRatio.CurrentRatio;
    pthread_mutex_unlock(&g_CellularRatio.Lock);
    return ratio;
}

//
// Check if ratio data is available
//
bool CellularRatioIsAvailable(void) {
    return g_CellularRatio.DataAvailable;
}

//
// Get statistics
//
void CellularRatioGetStats(CELLULAR_RATIO_STATS* Stats) {
    if (Stats == NULL) {
        return;
    }

    pthread_mutex_lock(&g_CellularRatio.Lock);
    Stats->PacketsReceived = g_CellularRatio.PacketsReceived;
    Stats->LastUpdateTimeUs = g_CellularRatio.LastUpdateTimeUs;
    Stats->LastRatio = g_CellularRatio.CurrentRatio;
    Stats->SmoothedRatio = g_CellularRatio.SmoothedRatio;
    Stats->MinRatio = g_CellularRatio.MinRatio;
    Stats->MaxRatio = g_CellularRatio.MaxRatio;
    Stats->AvgRatio = g_CellularRatio.PacketsReceived > 0 ?
                      g_CellularRatio.SumRatio / g_CellularRatio.PacketsReceived : 1.0;
    pthread_mutex_unlock(&g_CellularRatio.Lock);
}
