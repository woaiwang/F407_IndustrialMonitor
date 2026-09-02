#include "boot_protocol.h"

#include <stdbool.h>
#include <stdint.h>

#include "boot_application.h"
#include "boot_config.h"
#include "boot_crc32.h"
#include "boot_flash.h"
#include "usart.h"

#define BOOT_PROTOCOL_HEADER_0           0xA5U
#define BOOT_PROTOCOL_HEADER_1           0x5AU
#define BOOT_PROTOCOL_MAX_WRITE_DATA_SIZE 256U
#define BOOT_PROTOCOL_MAX_PAYLOAD_SIZE   (4U + BOOT_PROTOCOL_MAX_WRITE_DATA_SIZE)
#define BOOT_PROTOCOL_MAX_FRAME_SIZE     (2U + 1U + 1U + 2U + \
                                          BOOT_PROTOCOL_MAX_PAYLOAD_SIZE + 4U)

#define BOOT_PROTOCOL_CMD_HELLO          0x01U
#define BOOT_PROTOCOL_CMD_BEGIN_UPDATE   0x02U
#define BOOT_PROTOCOL_CMD_ERASE_APP      0x03U
#define BOOT_PROTOCOL_CMD_WRITE_DATA     0x04U
#define BOOT_PROTOCOL_CMD_VERIFY         0x05U
#define BOOT_PROTOCOL_CMD_RUN_APP        0x06U

#define BOOT_PROTOCOL_CMD_ACK            0x79U
#define BOOT_PROTOCOL_CMD_NACK           0x1FU

#define BOOT_PROTOCOL_RX_TIMEOUT_MS      1000U

typedef enum
{
    BOOT_PROTOCOL_STATE_WAIT_HEADER_0 = 0U,
    BOOT_PROTOCOL_STATE_WAIT_HEADER_1,
    BOOT_PROTOCOL_STATE_WAIT_COMMAND,
    BOOT_PROTOCOL_STATE_WAIT_SEQUENCE,
    BOOT_PROTOCOL_STATE_WAIT_LENGTH_LOW,
    BOOT_PROTOCOL_STATE_WAIT_LENGTH_HIGH,
    BOOT_PROTOCOL_STATE_WAIT_PAYLOAD,
    BOOT_PROTOCOL_STATE_WAIT_CRC_0,
    BOOT_PROTOCOL_STATE_WAIT_CRC_1,
    BOOT_PROTOCOL_STATE_WAIT_CRC_2,
    BOOT_PROTOCOL_STATE_WAIT_CRC_3
} Boot_ProtocolParserState_t;

typedef enum
{
    BOOT_PROTOCOL_PARSE_IN_PROGRESS = 0U,
    BOOT_PROTOCOL_PARSE_FRAME_READY,
    BOOT_PROTOCOL_PARSE_CRC_ERROR,
    BOOT_PROTOCOL_PARSE_ERROR
} Boot_ProtocolParseResult_t;

typedef enum
{
    BOOT_PROTOCOL_UPDATE_IDLE = 0U,
    BOOT_PROTOCOL_UPDATE_PREPARED,
    BOOT_PROTOCOL_UPDATE_ERASED,
    BOOT_PROTOCOL_UPDATE_VERIFIED
} Boot_ProtocolUpdateState_t;

typedef enum
{
    BOOT_PROTOCOL_NACK_BAD_CRC = 0x01U,
    BOOT_PROTOCOL_NACK_UNKNOWN_COMMAND = 0x02U,
    BOOT_PROTOCOL_NACK_INVALID_PAYLOAD = 0x03U,
    BOOT_PROTOCOL_NACK_INVALID_STATE = 0x04U,
    BOOT_PROTOCOL_NACK_INVALID_OFFSET = 0x05U,
    BOOT_PROTOCOL_NACK_FLASH_ERROR = 0x06U,
    BOOT_PROTOCOL_NACK_VERIFY_FAILED = 0x07U,
    BOOT_PROTOCOL_NACK_APP_INVALID = 0x08U
} Boot_ProtocolNackReason_t;

typedef struct
{
    Boot_ProtocolParserState_t state;
    uint8_t command;
    uint8_t sequence;
    uint16_t length;
    uint16_t payloadIndex;
    uint8_t payload[BOOT_PROTOCOL_MAX_PAYLOAD_SIZE];
    uint32_t calculatedCrc32;
    uint32_t receivedCrc32;
} Boot_ProtocolParser_t;

typedef struct
{
    uint8_t command;
    uint8_t sequence;
    uint16_t length;
    uint8_t payload[BOOT_PROTOCOL_MAX_PAYLOAD_SIZE];
    uint32_t crc32;
} Boot_ProtocolFrame_t;

typedef struct
{
    uint32_t expectedFirmwareCrc32;
    uint32_t lastAcknowledgedCrc32;
    uint8_t lastAcknowledgedCommand;
    uint8_t lastAcknowledgedSequence;
    Boot_ProtocolUpdateState_t updateState;
    bool lastAcknowledgementValid;
} Boot_ProtocolSession_t;

static void Boot_ProtocolParserInit(Boot_ProtocolParser_t *parser);
static Boot_ProtocolParseResult_t Boot_ProtocolProcessByte(
    Boot_ProtocolParser_t *parser,
    uint8_t data,
    Boot_ProtocolFrame_t *frame);
static void Boot_ProtocolSendAck(const Boot_ProtocolFrame_t *frame);
static void Boot_ProtocolSendNack(uint8_t sequence,
                                  Boot_ProtocolNackReason_t reason);
static HAL_StatusTypeDef Boot_ProtocolSendFrame(uint8_t command,
                                                 uint8_t sequence,
                                                 const uint8_t *payload,
                                                 uint16_t length);
static void Boot_ProtocolHandleFrame(const Boot_ProtocolFrame_t *frame,
                                     Boot_ProtocolSession_t *session);
static bool Boot_ProtocolIsDuplicate(const Boot_ProtocolFrame_t *frame,
                                     const Boot_ProtocolSession_t *session);
static void Boot_ProtocolRememberAcknowledgement(
    const Boot_ProtocolFrame_t *frame,
    Boot_ProtocolSession_t *session);
static uint32_t Boot_ProtocolReadUint32(const uint8_t *data);

static Boot_ProtocolParser_t bootProtocolParser;
static Boot_ProtocolFrame_t bootProtocolFrame;
static Boot_ProtocolSession_t bootProtocolSession;
volatile Boot_ProtocolDebug_t g_bootDebug;

void Boot_Protocol_Run(void)
{
    uint8_t receivedByte;
    HAL_StatusTypeDef receiveStatus;
    Boot_ProtocolParseResult_t parseResult;

    Boot_ProtocolParserInit(&bootProtocolParser);
    bootProtocolSession = (Boot_ProtocolSession_t){0};
    bootProtocolSession.updateState = BOOT_PROTOCOL_UPDATE_IDLE;
    g_bootDebug.lastStatus = BOOT_PROTOCOL_DEBUG_STATUS_ENTERED;

    while (1)
    {
        g_bootDebug.protocolLoopCount++;
        receiveStatus = HAL_UART_Receive(&huart1,
                                         &receivedByte,
                                         1U,
                                         BOOT_PROTOCOL_RX_TIMEOUT_MS);
        if (receiveStatus != HAL_OK)
        {
            Boot_ProtocolParserInit(&bootProtocolParser);
            g_bootDebug.lastStatus = BOOT_PROTOCOL_DEBUG_STATUS_RX_TIMEOUT;
            continue;
        }

        parseResult = Boot_ProtocolProcessByte(&bootProtocolParser,
                                               receivedByte,
                                               &bootProtocolFrame);

        if (parseResult == BOOT_PROTOCOL_PARSE_FRAME_READY)
        {
            g_bootDebug.rxFrameCount++;
            g_bootDebug.lastCommand = bootProtocolFrame.command;
            g_bootDebug.lastStatus = BOOT_PROTOCOL_DEBUG_STATUS_FRAME_READY;
            Boot_ProtocolHandleFrame(&bootProtocolFrame, &bootProtocolSession);
        }
        else if (parseResult == BOOT_PROTOCOL_PARSE_CRC_ERROR)
        {
            g_bootDebug.lastCommand = bootProtocolFrame.command;
            g_bootDebug.lastStatus = BOOT_PROTOCOL_DEBUG_STATUS_CRC_ERROR;
            Boot_ProtocolSendNack(bootProtocolFrame.sequence,
                                  BOOT_PROTOCOL_NACK_BAD_CRC);
        }
        else if (parseResult == BOOT_PROTOCOL_PARSE_ERROR)
        {
            g_bootDebug.lastStatus = BOOT_PROTOCOL_DEBUG_STATUS_PARSE_ERROR;
        }
    }
}

static void Boot_ProtocolParserInit(Boot_ProtocolParser_t *parser)
{
    parser->state = BOOT_PROTOCOL_STATE_WAIT_HEADER_0;
    parser->command = 0U;
    parser->sequence = 0U;
    parser->length = 0U;
    parser->payloadIndex = 0U;
    parser->calculatedCrc32 = Boot_Crc32_Begin();
    parser->receivedCrc32 = 0U;
}

static Boot_ProtocolParseResult_t Boot_ProtocolProcessByte(
    Boot_ProtocolParser_t *parser,
    uint8_t data,
    Boot_ProtocolFrame_t *frame)
{
    uint8_t crcByteIndex;

    switch (parser->state)
    {
        case BOOT_PROTOCOL_STATE_WAIT_HEADER_0:
            if (data == BOOT_PROTOCOL_HEADER_0)
            {
                parser->state = BOOT_PROTOCOL_STATE_WAIT_HEADER_1;
            }
            break;

        case BOOT_PROTOCOL_STATE_WAIT_HEADER_1:
            if (data == BOOT_PROTOCOL_HEADER_1)
            {
                parser->state = BOOT_PROTOCOL_STATE_WAIT_COMMAND;
            }
            else if (data != BOOT_PROTOCOL_HEADER_0)
            {
                Boot_ProtocolParserInit(parser);
            }
            break;

        case BOOT_PROTOCOL_STATE_WAIT_COMMAND:
            parser->command = data;
            parser->calculatedCrc32 = Boot_Crc32_Begin();
            parser->calculatedCrc32 = Boot_Crc32_Update(parser->calculatedCrc32,
                                                        &data,
                                                        1U);
            parser->state = BOOT_PROTOCOL_STATE_WAIT_SEQUENCE;
            break;

        case BOOT_PROTOCOL_STATE_WAIT_SEQUENCE:
            parser->sequence = data;
            parser->calculatedCrc32 = Boot_Crc32_Update(parser->calculatedCrc32,
                                                        &data,
                                                        1U);
            parser->state = BOOT_PROTOCOL_STATE_WAIT_LENGTH_LOW;
            break;

        case BOOT_PROTOCOL_STATE_WAIT_LENGTH_LOW:
            parser->length = data;
            parser->calculatedCrc32 = Boot_Crc32_Update(parser->calculatedCrc32,
                                                        &data,
                                                        1U);
            parser->state = BOOT_PROTOCOL_STATE_WAIT_LENGTH_HIGH;
            break;

        case BOOT_PROTOCOL_STATE_WAIT_LENGTH_HIGH:
            parser->length |= (uint16_t)data << 8U;
            parser->calculatedCrc32 = Boot_Crc32_Update(parser->calculatedCrc32,
                                                        &data,
                                                        1U);

            if (parser->length > BOOT_PROTOCOL_MAX_PAYLOAD_SIZE)
            {
                Boot_ProtocolParserInit(parser);
                return BOOT_PROTOCOL_PARSE_ERROR;
            }

            parser->payloadIndex = 0U;
            parser->state = (parser->length == 0U) ?
                            BOOT_PROTOCOL_STATE_WAIT_CRC_0 :
                            BOOT_PROTOCOL_STATE_WAIT_PAYLOAD;
            break;

        case BOOT_PROTOCOL_STATE_WAIT_PAYLOAD:
            parser->payload[parser->payloadIndex] = data;
            parser->payloadIndex++;
            parser->calculatedCrc32 = Boot_Crc32_Update(parser->calculatedCrc32,
                                                        &data,
                                                        1U);

            if (parser->payloadIndex == parser->length)
            {
                parser->state = BOOT_PROTOCOL_STATE_WAIT_CRC_0;
            }
            break;

        case BOOT_PROTOCOL_STATE_WAIT_CRC_0:
        case BOOT_PROTOCOL_STATE_WAIT_CRC_1:
        case BOOT_PROTOCOL_STATE_WAIT_CRC_2:
            crcByteIndex = (uint8_t)(parser->state -
                                     BOOT_PROTOCOL_STATE_WAIT_CRC_0);
            parser->receivedCrc32 |= (uint32_t)data << (crcByteIndex * 8U);
            parser->state = (Boot_ProtocolParserState_t)(parser->state + 1U);
            break;

        case BOOT_PROTOCOL_STATE_WAIT_CRC_3:
            parser->receivedCrc32 |= (uint32_t)data << 24U;
            frame->command = parser->command;
            frame->sequence = parser->sequence;
            frame->length = parser->length;
            frame->crc32 = parser->receivedCrc32;

            if (Boot_Crc32_Finish(parser->calculatedCrc32) ==
                parser->receivedCrc32)
            {
                uint16_t index;

                for (index = 0U; index < parser->length; index++)
                {
                    frame->payload[index] = parser->payload[index];
                }

                Boot_ProtocolParserInit(parser);
                return BOOT_PROTOCOL_PARSE_FRAME_READY;
            }

            Boot_ProtocolParserInit(parser);
            return BOOT_PROTOCOL_PARSE_CRC_ERROR;

        default:
            Boot_ProtocolParserInit(parser);
            return BOOT_PROTOCOL_PARSE_ERROR;
    }

    return BOOT_PROTOCOL_PARSE_IN_PROGRESS;
}

static void Boot_ProtocolSendAck(const Boot_ProtocolFrame_t *frame)
{
    if (Boot_ProtocolSendFrame(BOOT_PROTOCOL_CMD_ACK,
                               frame->sequence,
                               NULL,
                               0U) == HAL_OK)
    {
        g_bootDebug.ackCount++;
        g_bootDebug.lastStatus = BOOT_PROTOCOL_DEBUG_STATUS_ACK_SENT;
    }
    else
    {
        g_bootDebug.lastStatus = BOOT_PROTOCOL_DEBUG_STATUS_ACK_TX_ERROR;
    }
}

static void Boot_ProtocolSendNack(uint8_t sequence,
                                  Boot_ProtocolNackReason_t reason)
{
    uint8_t payload = (uint8_t)reason;

    if (Boot_ProtocolSendFrame(BOOT_PROTOCOL_CMD_NACK,
                               sequence,
                               &payload,
                               1U) == HAL_OK)
    {
        g_bootDebug.nackCount++;
        g_bootDebug.lastStatus = BOOT_PROTOCOL_DEBUG_STATUS_NACK_SENT;
    }
    else
    {
        g_bootDebug.lastStatus = BOOT_PROTOCOL_DEBUG_STATUS_NACK_TX_ERROR;
    }
}

static HAL_StatusTypeDef Boot_ProtocolSendFrame(uint8_t command,
                                                 uint8_t sequence,
                                                 const uint8_t *payload,
                                                 uint16_t length)
{
    uint8_t frame[BOOT_PROTOCOL_MAX_FRAME_SIZE];
    uint16_t index;
    uint16_t frameLength;
    uint32_t crc32;

    frame[0] = BOOT_PROTOCOL_HEADER_0;
    frame[1] = BOOT_PROTOCOL_HEADER_1;
    frame[2] = command;
    frame[3] = sequence;
    frame[4] = (uint8_t)(length & 0x00FFU);
    frame[5] = (uint8_t)(length >> 8U);

    for (index = 0U; index < length; index++)
    {
        frame[6U + index] = payload[index];
    }

    crc32 = Boot_Crc32_Calculate(&frame[2], (uint32_t)(4U + length));
    frame[6U + length] = (uint8_t)(crc32 & 0x000000FFUL);
    frame[7U + length] = (uint8_t)((crc32 >> 8U) & 0x000000FFUL);
    frame[8U + length] = (uint8_t)((crc32 >> 16U) & 0x000000FFUL);
    frame[9U + length] = (uint8_t)((crc32 >> 24U) & 0x000000FFUL);
    frameLength = (uint16_t)(10U + length);

    return HAL_UART_Transmit(&huart1,
                             frame,
                             frameLength,
                             BOOT_UART_TX_TIMEOUT_MS);
}

static void Boot_ProtocolHandleFrame(const Boot_ProtocolFrame_t *frame,
                                     Boot_ProtocolSession_t *session)
{
    uint32_t firmwareSize;
    uint32_t firmwareCrc32;
    uint32_t offset;

    if (Boot_ProtocolIsDuplicate(frame, session))
    {
        Boot_ProtocolSendAck(frame);
        return;
    }

    switch (frame->command)
    {
        case BOOT_PROTOCOL_CMD_HELLO:
            g_bootDebug.helloCount++;

            if (frame->length != 0U)
            {
                Boot_ProtocolSendNack(frame->sequence,
                                      BOOT_PROTOCOL_NACK_INVALID_PAYLOAD);
                return;
            }
            break;

        case BOOT_PROTOCOL_CMD_BEGIN_UPDATE:
            if (frame->length != 8U)
            {
                Boot_ProtocolSendNack(frame->sequence,
                                      BOOT_PROTOCOL_NACK_INVALID_PAYLOAD);
                return;
            }

            firmwareSize = Boot_ProtocolReadUint32(&frame->payload[0]);
            firmwareCrc32 = Boot_ProtocolReadUint32(&frame->payload[4]);

            if (!Boot_FlashPrepareUpdate(firmwareSize))
            {
                Boot_ProtocolSendNack(frame->sequence,
                                      BOOT_PROTOCOL_NACK_INVALID_PAYLOAD);
                return;
            }

            session->expectedFirmwareCrc32 = firmwareCrc32;
            session->updateState = BOOT_PROTOCOL_UPDATE_PREPARED;
            break;

        case BOOT_PROTOCOL_CMD_ERASE_APP:
            if (frame->length != 0U)
            {
                Boot_ProtocolSendNack(frame->sequence,
                                      BOOT_PROTOCOL_NACK_INVALID_PAYLOAD);
                return;
            }

            if (session->updateState != BOOT_PROTOCOL_UPDATE_PREPARED)
            {
                Boot_ProtocolSendNack(frame->sequence,
                                      BOOT_PROTOCOL_NACK_INVALID_STATE);
                return;
            }

            if (!Boot_FlashEraseApplication())
            {
                Boot_ProtocolSendNack(frame->sequence,
                                      BOOT_PROTOCOL_NACK_FLASH_ERROR);
                return;
            }

            session->updateState = BOOT_PROTOCOL_UPDATE_ERASED;
            break;

        case BOOT_PROTOCOL_CMD_WRITE_DATA:
            if ((frame->length < 5U) ||
                (frame->length > BOOT_PROTOCOL_MAX_PAYLOAD_SIZE))
            {
                Boot_ProtocolSendNack(frame->sequence,
                                      BOOT_PROTOCOL_NACK_INVALID_PAYLOAD);
                return;
            }

            if (session->updateState != BOOT_PROTOCOL_UPDATE_ERASED)
            {
                Boot_ProtocolSendNack(frame->sequence,
                                      BOOT_PROTOCOL_NACK_INVALID_STATE);
                return;
            }

            offset = Boot_ProtocolReadUint32(&frame->payload[0]);

            if (offset != Boot_FlashGetReceivedBytes())
            {
                Boot_ProtocolSendNack(frame->sequence,
                                      BOOT_PROTOCOL_NACK_INVALID_OFFSET);
                return;
            }

            if (!Boot_FlashWriteData(&frame->payload[4],
                                     (uint16_t)(frame->length - 4U)))
            {
                Boot_ProtocolSendNack(frame->sequence,
                                      BOOT_PROTOCOL_NACK_FLASH_ERROR);
                return;
            }
            break;

        case BOOT_PROTOCOL_CMD_VERIFY:
            if (frame->length != 0U)
            {
                Boot_ProtocolSendNack(frame->sequence,
                                      BOOT_PROTOCOL_NACK_INVALID_PAYLOAD);
                return;
            }

            if (session->updateState != BOOT_PROTOCOL_UPDATE_ERASED)
            {
                Boot_ProtocolSendNack(frame->sequence,
                                      BOOT_PROTOCOL_NACK_INVALID_STATE);
                return;
            }

            if (!Boot_FlashVerify(session->expectedFirmwareCrc32))
            {
                Boot_ProtocolSendNack(frame->sequence,
                                      BOOT_PROTOCOL_NACK_VERIFY_FAILED);
                return;
            }

            if (!Boot_ApplicationIsValid())
            {
                Boot_ProtocolSendNack(frame->sequence,
                                      BOOT_PROTOCOL_NACK_APP_INVALID);
                return;
            }

            session->updateState = BOOT_PROTOCOL_UPDATE_VERIFIED;
            break;

        case BOOT_PROTOCOL_CMD_RUN_APP:
            if (frame->length != 0U)
            {
                Boot_ProtocolSendNack(frame->sequence,
                                      BOOT_PROTOCOL_NACK_INVALID_PAYLOAD);
                return;
            }

            if ((session->updateState != BOOT_PROTOCOL_UPDATE_VERIFIED) ||
                !Boot_ApplicationIsValid())
            {
                Boot_ProtocolSendNack(frame->sequence,
                                      BOOT_PROTOCOL_NACK_APP_INVALID);
                return;
            }
            break;

        default:
            Boot_ProtocolSendNack(frame->sequence,
                                  BOOT_PROTOCOL_NACK_UNKNOWN_COMMAND);
            return;
    }

    Boot_ProtocolRememberAcknowledgement(frame, session);
    Boot_ProtocolSendAck(frame);

    if (frame->command == BOOT_PROTOCOL_CMD_RUN_APP)
    {
        Boot_JumpToApplication();
    }
}

static bool Boot_ProtocolIsDuplicate(const Boot_ProtocolFrame_t *frame,
                                     const Boot_ProtocolSession_t *session)
{
    return session->lastAcknowledgementValid &&
           (frame->command == session->lastAcknowledgedCommand) &&
           (frame->sequence == session->lastAcknowledgedSequence) &&
           (frame->crc32 == session->lastAcknowledgedCrc32);
}

static void Boot_ProtocolRememberAcknowledgement(
    const Boot_ProtocolFrame_t *frame,
    Boot_ProtocolSession_t *session)
{
    session->lastAcknowledgedCommand = frame->command;
    session->lastAcknowledgedSequence = frame->sequence;
    session->lastAcknowledgedCrc32 = frame->crc32;
    session->lastAcknowledgementValid = true;
}

static uint32_t Boot_ProtocolReadUint32(const uint8_t *data)
{
    return (uint32_t)data[0] |
           ((uint32_t)data[1] << 8U) |
           ((uint32_t)data[2] << 16U) |
           ((uint32_t)data[3] << 24U);
}
