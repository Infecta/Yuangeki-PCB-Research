#include <stdint.h>

extern uint32_t _estack;
extern uint32_t _sidata;
extern uint32_t _sdata;
extern uint32_t _edata;
extern uint32_t _sbss;
extern uint32_t _ebss;

int main(void);
void Reset_Handler(void);
void SysTick_Handler(void);
void USART1_IRQHandler(void);

void bootloader_enter_system_memory(void) __attribute__((naked, noreturn));
void bootloader_enter_system_memory(void)
{
    __asm volatile (
        "movs r3, #0\n"
        "msr control, r3\n"
        "ldr r0, =0x1fff0000\n"
        "ldr r1, [r0]\n"
        "ldr r2, [r0, #4]\n"
        "ldr r3, =0xe000ed08\n"
        "str r0, [r3]\n"
        "dsb\n"
        "msr msp, r1\n"
        "cpsie i\n"
        "isb\n"
        "bx r2\n");
}

static void Default_Handler(void)
{
    for (;;) {
    }
}

void NMI_Handler(void) __attribute__((weak, alias("Default_Handler")));
void HardFault_Handler(void) __attribute__((weak, alias("Default_Handler")));
void SVC_Handler(void) __attribute__((weak, alias("Default_Handler")));
void PendSV_Handler(void) __attribute__((weak, alias("Default_Handler")));

typedef void (*vector_fn)(void);

__attribute__((section(".isr_vector"), used))
const vector_fn vector_table[] = {
    (vector_fn)&_estack,
    Reset_Handler,
    NMI_Handler,
    HardFault_Handler,
    0, 0, 0, 0, 0, 0, 0,
    SVC_Handler,
    0, 0,
    PendSV_Handler,
    SysTick_Handler,
    Default_Handler, 0, Default_Handler, Default_Handler,
    Default_Handler, Default_Handler, Default_Handler, Default_Handler,
    0, Default_Handler, Default_Handler, Default_Handler,
    Default_Handler, Default_Handler, Default_Handler, 0,
    Default_Handler, 0, 0, Default_Handler,
    0, Default_Handler, Default_Handler, Default_Handler,
    Default_Handler, Default_Handler, Default_Handler, USART1_IRQHandler,
    Default_Handler
};

void Reset_Handler(void)
{
    uint32_t *source = &_sidata;
    uint32_t *destination = &_sdata;

    while (destination < &_edata) {
        *destination++ = *source++;
    }

    destination = &_sbss;
    while (destination < &_ebss) {
        *destination++ = 0;
    }

    (void)main();
    for (;;) {
    }
}
