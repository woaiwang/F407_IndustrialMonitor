#ifndef CLI_H
#define CLI_H

#include <stdint.h>

#define CLI_COMMAND_BUFFER_SIZE 64U

typedef struct
{
    char commandBuffer[CLI_COMMAND_BUFFER_SIZE];
    uint8_t length;
    uint8_t overflow;
} CliContext_t;

void CLI_Init(CliContext_t *context);
void CLI_ProcessByte(CliContext_t *context, uint8_t data);

#endif /* CLI_H */
