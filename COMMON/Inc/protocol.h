#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdbool.h>
#include <stdint.h>

#include "ring_buffer.h"

#define PROTOCOL_HEADER_BYTE_0       0xAAU
#define PROTOCOL_HEADER_BYTE_1       0x55U
#define PROTOCOL_MAX_PAYLOAD_LENGTH  64U
#define PROTOCOL_MAX_FRAME_LENGTH    (2U + 1U + 1U + PROTOCOL_MAX_PAYLOAD_LENGTH + 2U)

typedef enum
{
    CMD_GET_STATUS  = 0x01,
    CMD_GET_SENSOR  = 0x02,
    CMD_GET_VERSION = 0x03,
    CMD_REBOOT      = 0x04
} ProtocolCommand_t;

typedef struct
{
    uint8_t command;
    uint8_t length;
    uint8_t payload[PROTOCOL_MAX_PAYLOAD_LENGTH];
} ProtocolFrame_t;

typedef struct
{
    uint8_t state;
    uint8_t length;
    uint8_t command;
    uint8_t payloadIndex;
    uint8_t payload[PROTOCOL_MAX_PAYLOAD_LENGTH];
    uint16_t calculatedCrc;
    uint16_t receivedCrc;
} ProtocolParser_t;

void Protocol_Init(ProtocolParser_t *parser);
uint16_t Protocol_CalculateCrc16(const uint8_t *data, uint16_t length);
bool Protocol_Parse(RingBuffer_t *ringBuffer,
                    ProtocolParser_t *parser,
                    ProtocolFrame_t *frame);
uint16_t Protocol_BuildFrame(uint8_t command,
                             const uint8_t *payload,
                             uint8_t length,
                             uint8_t *frame,
                             uint16_t frameSize);

#endif /* PROTOCOL_H */
