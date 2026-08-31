#include "protocol.h"

#include <stddef.h>

typedef enum
{
    PROTOCOL_STATE_WAIT_HEADER_0 = 0U,
    PROTOCOL_STATE_WAIT_HEADER_1,
    PROTOCOL_STATE_WAIT_LENGTH,
    PROTOCOL_STATE_WAIT_COMMAND,
    PROTOCOL_STATE_WAIT_PAYLOAD,
    PROTOCOL_STATE_WAIT_CRC_LOW,
    PROTOCOL_STATE_WAIT_CRC_HIGH
} ProtocolParserState_t;

static uint16_t Protocol_UpdateCrc16(uint16_t crc, uint8_t data);
static void Protocol_ResetAfterError(ProtocolParser_t *parser, uint8_t data);

void Protocol_Init(ProtocolParser_t *parser)
{
    if (parser == NULL)
    {
        return;
    }

    parser->state = (uint8_t)PROTOCOL_STATE_WAIT_HEADER_0;
    parser->length = 0U;
    parser->command = 0U;
    parser->payloadIndex = 0U;
    parser->calculatedCrc = 0xFFFFU;
    parser->receivedCrc = 0U;
}

uint16_t Protocol_CalculateCrc16(const uint8_t *data, uint16_t length)
{
    uint16_t crc = 0xFFFFU;
    uint16_t index;

    if ((data == NULL) && (length != 0U))
    {
        return 0U;
    }

    for (index = 0U; index < length; index++)
    {
        crc = Protocol_UpdateCrc16(crc, data[index]);
    }

    return crc;
}

bool Protocol_Parse(RingBuffer_t *ringBuffer,
                    ProtocolParser_t *parser,
                    ProtocolFrame_t *frame)
{
    uint8_t data;
    uint8_t index;

    if ((ringBuffer == NULL) || (parser == NULL) || (frame == NULL))
    {
        return false;
    }

    while (RingBuffer_Get(ringBuffer, &data))
    {
        switch ((ProtocolParserState_t)parser->state)
        {
            case PROTOCOL_STATE_WAIT_HEADER_0:
                if (data == PROTOCOL_HEADER_BYTE_0)
                {
                    parser->state = (uint8_t)PROTOCOL_STATE_WAIT_HEADER_1;
                }
                break;

            case PROTOCOL_STATE_WAIT_HEADER_1:
                if (data == PROTOCOL_HEADER_BYTE_1)
                {
                    parser->state = (uint8_t)PROTOCOL_STATE_WAIT_LENGTH;
                }
                else if (data != PROTOCOL_HEADER_BYTE_0)
                {
                    parser->state = (uint8_t)PROTOCOL_STATE_WAIT_HEADER_0;
                }
                break;

            case PROTOCOL_STATE_WAIT_LENGTH:
                if (data > PROTOCOL_MAX_PAYLOAD_LENGTH)
                {
                    Protocol_ResetAfterError(parser, data);
                }
                else
                {
                    parser->length = data;
                    parser->payloadIndex = 0U;
                    parser->calculatedCrc = Protocol_UpdateCrc16(0xFFFFU, data);
                    parser->state = (uint8_t)PROTOCOL_STATE_WAIT_COMMAND;
                }
                break;

            case PROTOCOL_STATE_WAIT_COMMAND:
                parser->command = data;
                parser->calculatedCrc = Protocol_UpdateCrc16(parser->calculatedCrc,
                                                              data);

                if (parser->length == 0U)
                {
                    parser->state = (uint8_t)PROTOCOL_STATE_WAIT_CRC_LOW;
                }
                else
                {
                    parser->state = (uint8_t)PROTOCOL_STATE_WAIT_PAYLOAD;
                }
                break;

            case PROTOCOL_STATE_WAIT_PAYLOAD:
                parser->payload[parser->payloadIndex] = data;
                parser->payloadIndex++;
                parser->calculatedCrc = Protocol_UpdateCrc16(parser->calculatedCrc,
                                                              data);

                if (parser->payloadIndex >= parser->length)
                {
                    parser->state = (uint8_t)PROTOCOL_STATE_WAIT_CRC_LOW;
                }
                break;

            case PROTOCOL_STATE_WAIT_CRC_LOW:
                parser->receivedCrc = data;
                parser->state = (uint8_t)PROTOCOL_STATE_WAIT_CRC_HIGH;
                break;

            case PROTOCOL_STATE_WAIT_CRC_HIGH:
                parser->receivedCrc |= (uint16_t)data << 8U;

                if (parser->receivedCrc == parser->calculatedCrc)
                {
                    frame->command = parser->command;
                    frame->length = parser->length;

                    for (index = 0U; index < frame->length; index++)
                    {
                        frame->payload[index] = parser->payload[index];
                    }

                    Protocol_Init(parser);
                    return true;
                }

                Protocol_ResetAfterError(parser, data);
                break;

            default:
                Protocol_Init(parser);
                break;
        }
    }

    return false;
}

uint16_t Protocol_BuildFrame(uint8_t command,
                             const uint8_t *payload,
                             uint8_t length,
                             uint8_t *frame,
                             uint16_t frameSize)
{
    uint16_t crc;
    uint8_t index;
    uint16_t requiredLength;

    if ((frame == NULL) || (length > PROTOCOL_MAX_PAYLOAD_LENGTH) ||
        ((payload == NULL) && (length != 0U)))
    {
        return 0U;
    }

    requiredLength = (uint16_t)(length + 6U);

    if (frameSize < requiredLength)
    {
        return 0U;
    }

    frame[0] = PROTOCOL_HEADER_BYTE_0;
    frame[1] = PROTOCOL_HEADER_BYTE_1;
    frame[2] = length;
    frame[3] = command;

    for (index = 0U; index < length; index++)
    {
        frame[4U + index] = payload[index];
    }

    crc = Protocol_CalculateCrc16(&frame[2], (uint16_t)(length + 2U));
    frame[4U + length] = (uint8_t)(crc & 0x00FFU);
    frame[5U + length] = (uint8_t)(crc >> 8U);

    return requiredLength;
}

static uint16_t Protocol_UpdateCrc16(uint16_t crc, uint8_t data)
{
    uint8_t bit;

    crc ^= data;

    for (bit = 0U; bit < 8U; bit++)
    {
        if ((crc & 0x0001U) != 0U)
        {
            crc = (crc >> 1U) ^ 0xA001U;
        }
        else
        {
            crc >>= 1U;
        }
    }

    return crc;
}

static void Protocol_ResetAfterError(ProtocolParser_t *parser, uint8_t data)
{
    Protocol_Init(parser);

    if (data == PROTOCOL_HEADER_BYTE_0)
    {
        parser->state = (uint8_t)PROTOCOL_STATE_WAIT_HEADER_1;
    }
}
