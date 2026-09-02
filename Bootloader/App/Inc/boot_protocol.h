#ifndef BOOT_PROTOCOL_H
#define BOOT_PROTOCOL_H

#include <stdint.h>

typedef enum
{
    BOOT_PROTOCOL_DEBUG_STATUS_IDLE = 0U,
    BOOT_PROTOCOL_DEBUG_STATUS_ENTERED,
    BOOT_PROTOCOL_DEBUG_STATUS_RX_TIMEOUT,
    BOOT_PROTOCOL_DEBUG_STATUS_FRAME_READY,
    BOOT_PROTOCOL_DEBUG_STATUS_CRC_ERROR,
    BOOT_PROTOCOL_DEBUG_STATUS_PARSE_ERROR,
    BOOT_PROTOCOL_DEBUG_STATUS_ACK_SENT,
    BOOT_PROTOCOL_DEBUG_STATUS_ACK_TX_ERROR,
    BOOT_PROTOCOL_DEBUG_STATUS_NACK_SENT,
    BOOT_PROTOCOL_DEBUG_STATUS_NACK_TX_ERROR
} Boot_ProtocolDebugStatus_t;

typedef struct
{
    uint32_t protocolLoopCount;
    uint32_t rxFrameCount;
    uint32_t helloCount;
    uint32_t ackCount;
    uint32_t nackCount;
    uint32_t lastCommand;
    uint32_t lastStatus;
} Boot_ProtocolDebug_t;

extern volatile Boot_ProtocolDebug_t g_bootDebug;

void Boot_Protocol_Run(void);

#endif /* BOOT_PROTOCOL_H */
