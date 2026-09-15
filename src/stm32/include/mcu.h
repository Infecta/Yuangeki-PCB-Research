#ifndef YTG_MCU_H
#define YTG_MCU_H

#include <stdint.h>

#define REG32(address) (*(volatile uint32_t *)(address))

#define RCC_BASE       0x40021000UL
#define FLASH_BASE_REG 0x40022000UL
#define GPIOA_BASE     0x50000000UL
#define GPIOB_BASE     0x50000400UL
#define GPIOC_BASE     0x50000800UL
#define GPIOD_BASE     0x50000C00UL
#define USART1_BASE    0x40013800UL
#define SPI1_BASE      0x40013000UL
#define ADC1_BASE      0x40012400UL
#define DMA1_BASE      0x40020000UL
#define DMAMUX1_BASE   0x40020800UL
#define ADC_COMMON     0x40012708UL

#define RCC_IOPENR     REG32(RCC_BASE + 0x34UL)
#define RCC_AHBRSTR    REG32(RCC_BASE + 0x28UL)
#define RCC_APBRSTR1   REG32(RCC_BASE + 0x2CUL)
#define RCC_APBRSTR2   REG32(RCC_BASE + 0x30UL)
#define RCC_APBENR1    REG32(RCC_BASE + 0x3CUL)
#define RCC_AHBENR     REG32(RCC_BASE + 0x38UL)
#define RCC_APBENR2    REG32(RCC_BASE + 0x40UL)
#define FLASH_KEYR     REG32(FLASH_BASE_REG + 0x08UL)
#define FLASH_SR       REG32(FLASH_BASE_REG + 0x10UL)
#define FLASH_CR       REG32(FLASH_BASE_REG + 0x14UL)

#define GPIO_MODER(base)   REG32((base) + 0x00UL)
#define GPIO_OTYPER(base)  REG32((base) + 0x04UL)
#define GPIO_PUPDR(base)   REG32((base) + 0x0CUL)
#define GPIO_IDR(base)     REG32((base) + 0x10UL)
#define GPIO_BSRR(base)    REG32((base) + 0x18UL)
#define GPIO_AFRL(base)    REG32((base) + 0x20UL)
#define GPIO_AFRH(base)    REG32((base) + 0x24UL)
#define GPIO_OSPEEDR(base) REG32((base) + 0x08UL)

#define USART1_CR1     REG32(USART1_BASE + 0x00UL)
#define USART1_BRR     REG32(USART1_BASE + 0x0CUL)
#define USART1_ISR     REG32(USART1_BASE + 0x1CUL)
#define USART1_ICR     REG32(USART1_BASE + 0x20UL)
#define USART1_RDR     REG32(USART1_BASE + 0x24UL)
#define USART1_TDR     REG32(USART1_BASE + 0x28UL)

#define SPI1_CR1       REG32(SPI1_BASE + 0x00UL)
#define SPI1_CR2       REG32(SPI1_BASE + 0x04UL)
#define SPI1_SR        REG32(SPI1_BASE + 0x08UL)
#define SPI1_DR8       (*(volatile uint8_t *)(SPI1_BASE + 0x0CUL))

#define DMA1_ISR       REG32(DMA1_BASE + 0x00UL)
#define DMA1_IFCR      REG32(DMA1_BASE + 0x04UL)
#define DMA1_CCR1      REG32(DMA1_BASE + 0x08UL)
#define DMA1_CNDTR1    REG32(DMA1_BASE + 0x0CUL)
#define DMA1_CPAR1     REG32(DMA1_BASE + 0x10UL)
#define DMA1_CMAR1     REG32(DMA1_BASE + 0x14UL)
#define DMAMUX1_C0CR   REG32(DMAMUX1_BASE + 0x00UL)


#define ADC1_ISR       REG32(ADC1_BASE + 0x00UL)
#define ADC1_CR        REG32(ADC1_BASE + 0x08UL)
#define ADC1_CFGR1     REG32(ADC1_BASE + 0x0CUL)
#define ADC1_CFGR2     REG32(ADC1_BASE + 0x10UL)
#define ADC1_SMPR      REG32(ADC1_BASE + 0x14UL)
#define ADC1_CHSELR    REG32(ADC1_BASE + 0x28UL)
#define ADC1_DR        REG32(ADC1_BASE + 0x40UL)
#define ADC_CCR        REG32(ADC_COMMON)

#define SYST_CSR       REG32(0xE000E010UL)
#define SYST_RVR       REG32(0xE000E014UL)
#define SYST_CVR       REG32(0xE000E018UL)
#define NVIC_ISER      REG32(0xE000E100UL)
#define NVIC_ICER      REG32(0xE000E180UL)
#define NVIC_ICPR      REG32(0xE000E280UL)
#define SCB_VTOR       REG32(0xE000ED08UL)
#define SCB_AIRCR      REG32(0xE000ED0CUL)

static inline void irq_disable(void)
{
    __asm volatile ("cpsid i" ::: "memory");
}

static inline void irq_enable(void)
{
    __asm volatile ("cpsie i" ::: "memory");
}

static inline uint32_t irq_save_disable(void)
{
    uint32_t state;
    __asm volatile ("mrs %0, primask\n"
                    "cpsid i" : "=r" (state) :: "memory");
    return state;
}

static inline void irq_restore(uint32_t state)
{
    __asm volatile ("msr primask, %0" :: "r" (state) : "memory");
}

static inline void wait_for_interrupt(void)
{
    __asm volatile ("wfi");
}

static inline void instruction_sync_barrier(void)
{
    __asm volatile ("isb" ::: "memory");
}

#endif
