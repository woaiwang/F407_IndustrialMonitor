#include "boot_application.h"

#include "boot_config.h"
#include "usart.h"

typedef void (*Boot_ApplicationEntry_t)(void);

/*
 * Kept outside the old bootloader stack so the branch target remains available
 * after __set_MSP() switches to the application stack.
 */
static volatile Boot_ApplicationEntry_t bootApplicationEntry;

static uint32_t Boot_ReadWord(uint32_t address)
{
    return *((volatile const uint32_t *)address);
}

bool Boot_ApplicationIsValid(void)
{
    uint32_t initialMsp = Boot_ReadWord(BOOT_APP_ADDRESS);
    uint32_t resetHandler = Boot_ReadWord(BOOT_APP_ADDRESS + sizeof(uint32_t));
    uint32_t resetHandlerAddress = resetHandler & ~1UL;

    if ((initialMsp < (BOOT_SRAM_START + BOOT_MINIMUM_STACK_MARGIN)) ||
        (initialMsp > BOOT_SRAM_END))
    {
        return false;
    }

    if ((initialMsp & 0x7UL) != 0U)
    {
        return false;
    }

    if ((resetHandler & 0x1UL) == 0U)
    {
        return false;
    }

    if ((resetHandlerAddress < BOOT_APP_ADDRESS) || (resetHandlerAddress >= BOOT_APP_END))
    {
        return false;
    }

    return true;
}

void Boot_JumpToApplication(void)
{
    uint32_t initialMsp;
    uint32_t resetHandler;
    uint32_t registerIndex;

    if ((__get_IPSR() != 0U) ||
        ((__get_CONTROL() & CONTROL_nPRIV_Msk) != 0U) ||
        !Boot_ApplicationIsValid())
    {
        return;
    }

    initialMsp = Boot_ReadWord(BOOT_APP_ADDRESS);
    resetHandler = Boot_ReadWord(BOOT_APP_ADDRESS + sizeof(uint32_t));
    bootApplicationEntry = (Boot_ApplicationEntry_t)resetHandler;

    __disable_irq();

    SysTick->CTRL = 0U;
    SysTick->LOAD = 0U;
    SysTick->VAL = 0U;
    SCB->ICSR = SCB_ICSR_PENDSTCLR_Msk | SCB_ICSR_PENDSVCLR_Msk;

    (void)HAL_UART_DeInit(&huart1);
    (void)HAL_DeInit();

    for (registerIndex = 0U; registerIndex < BOOT_NVIC_REGISTER_COUNT; registerIndex++)
    {
        NVIC->ICER[registerIndex] = 0xFFFFFFFFUL;
        NVIC->ICPR[registerIndex] = 0xFFFFFFFFUL;
    }

    SCB->VTOR = BOOT_APP_ADDRESS;
    __DSB();
    __ISB();

    __set_CONTROL(0U);
    __set_MSP(initialMsp);
    __set_PSP(0U);
    __set_BASEPRI(0U);
    __set_FAULTMASK(0U);
    __DSB();
    __ISB();

    __set_PRIMASK(0U);
    bootApplicationEntry();

    __disable_irq();
    while (1)
    {
    }
}
