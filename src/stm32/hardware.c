#include "board.h"
#include "mcu.h"
#include "strip.h"

#define GPIO_MODE_INPUT  0U
#define GPIO_MODE_OUTPUT 1U
#define GPIO_MODE_ALT    2U
#define GPIO_MODE_ANALOG 3U

#define RCC_GPIOA_EN (1U << 0)
#define RCC_GPIOB_EN (1U << 1)
#define RCC_GPIOC_EN (1U << 2)
#define RCC_GPIOD_EN (1U << 3)
#define RCC_USART1_EN (1U << 14)
#define RCC_SPI1_EN (1U << 12)
#define RCC_ADC_EN (1U << 20)
#define RCC_DMA1_EN (1U << 0)
#define UART_RX_BUFFER_SIZE 64U
#define UART_RX_BUFFER_MASK (UART_RX_BUFFER_SIZE - 1U)

static volatile uint8_t uart_rx_buffer[UART_RX_BUFFER_SIZE];
static volatile uint8_t uart_rx_head;
static volatile uint8_t uart_rx_tail;
static volatile uint16_t uart_rx_overflow_count;
#define LED_RESET_BYTES 80U

/* 80 us low preamble + encoded pixels + 80 us low latch suffix. */
static uint8_t led_dma_frame[LED_RESET_BYTES +
                             STRIP_MAX_PIXELS * 30U +
                             LED_RESET_BYTES]
    __attribute__((aligned(4)));
_Static_assert((UART_RX_BUFFER_SIZE & (UART_RX_BUFFER_SIZE - 1U)) == 0U,
               "UART RX buffer size must be a power of two");

struct pin {
    uint32_t port;
    uint8_t number;
};

static const struct pin button_pins[BUTTON_COUNT] = {
    {GPIOB_BASE, 2},  {GPIOB_BASE, 12}, {GPIOB_BASE, 13}, {GPIOC_BASE, 14},
    {GPIOA_BASE, 2},  {GPIOA_BASE, 4},  {GPIOA_BASE, 8},  {GPIOB_BASE, 9},
    {GPIOB_BASE, 1},  {GPIOA_BASE, 6},  {GPIOB_BASE, 7},  {GPIOD_BASE, 0}
};

static const struct pin led_pins[BUTTON_COUNT] = {
    {GPIOB_BASE, 10}, {GPIOB_BASE, 11}, {GPIOB_BASE, 14}, {GPIOC_BASE, 15},
    {GPIOA_BASE, 3},  {GPIOA_BASE, 5},  {GPIOB_BASE, 15}, {GPIOC_BASE, 13},
    {GPIOB_BASE, 0},  {GPIOA_BASE, 7},  {GPIOB_BASE, 8},  {GPIOD_BASE, 1}
};

static void gpio_set_mode(uint32_t port, uint8_t pin, uint32_t mode)
{
    const uint32_t shift = (uint32_t)pin * 2U;
    GPIO_MODER(port) = (GPIO_MODER(port) & ~(3UL << shift)) | (mode << shift);
}

static void gpio_set_pull_up(uint32_t port, uint8_t pin)
{
    const uint32_t shift = (uint32_t)pin * 2U;
    GPIO_PUPDR(port) = (GPIO_PUPDR(port) & ~(3UL << shift)) | (1UL << shift);
}

void hardware_init(void)
{
    SCB_VTOR = 0x08000000UL;
    RCC_IOPENR |= RCC_GPIOA_EN | RCC_GPIOB_EN | RCC_GPIOC_EN | RCC_GPIOD_EN;
    RCC_AHBENR |= RCC_DMA1_EN;
    RCC_APBENR2 |= RCC_USART1_EN | RCC_SPI1_EN | RCC_ADC_EN;
    (void)RCC_IOPENR;
    (void)RCC_APBENR2;

    for (uint8_t i = 0; i < BUTTON_COUNT; ++i) {
        gpio_set_mode(button_pins[i].port, button_pins[i].number, GPIO_MODE_INPUT);
        gpio_set_pull_up(button_pins[i].port, button_pins[i].number);
        GPIO_BSRR(led_pins[i].port) = (uint32_t)1U << (led_pins[i].number + 16U);
        gpio_set_mode(led_pins[i].port, led_pins[i].number, GPIO_MODE_OUTPUT);
    }

    /* PA0 / ADC_IN0. */
    gpio_set_mode(GPIOA_BASE, 0, GPIO_MODE_ANALOG);
    GPIO_PUPDR(GPIOA_BASE) &= ~(3UL << 0);

    /* PA11 reproduces the stock periodic output, initially driven low. Its
       likely connection to optical-switch LDK is still a hardware inference. */
    GPIO_BSRR(GPIOA_BASE) = (uint32_t)1U << (11U + 16U);
    gpio_set_mode(GPIOA_BASE, 11, GPIO_MODE_OUTPUT);

    /* PA12 / SPI1_MOSI AF0 remains owned by SPI for the entire lifetime of
       the application. Every serialized frame begins and ends with reset-low
       bytes, avoiding glitches from GPIO/alternate-function transitions. */
    GPIO_BSRR(GPIOA_BASE) = (uint32_t)1U << (12U + 16U);
    gpio_set_mode(GPIOA_BASE, 12, GPIO_MODE_ALT);
    GPIO_AFRH(GPIOA_BASE) &= ~(0xFUL << 16);
    GPIO_OTYPER(GPIOA_BASE) |= (1UL << 12);
    GPIO_OSPEEDR(GPIOA_BASE) = (GPIO_OSPEEDR(GPIOA_BASE) & ~(3UL << 24)) |
                              (3UL << 24);

    /* SPI1 produces the verified 8 MHz LED serializer waveform. PB3 is the
       peripheral clock pin but is not part of the one-wire LED connection. */
    GPIO_BSRR(GPIOB_BASE) = (uint32_t)1U << (3U + 16U);
    gpio_set_mode(GPIOB_BASE, 3, GPIO_MODE_ALT);
    GPIO_AFRL(GPIOB_BASE) &= ~(0xFUL << 12);
    GPIO_OTYPER(GPIOB_BASE) |= (1UL << 3);
    GPIO_OSPEEDR(GPIOB_BASE) = (GPIO_OSPEEDR(GPIOB_BASE) & ~(3UL << 6)) |
                              (3UL << 6);

    /* HSI16 / 2 = 8 MHz. Ten SPI bits form each 1.25 us LED symbol. */
    SPI1_CR1 = (1U << 2) | (1U << 8) | (1U << 9) |
               (1U << 14) | (1U << 15);
    SPI1_CR2 = (1U << 1) | (7U << 8) | (1U << 12);
    SPI1_CR1 |= (1U << 6);

    DMA1_CCR1 = 0;
    DMA1_CPAR1 = SPI1_BASE + 0x0CUL;
    DMAMUX1_C0CR = 17U; /* RM0454: SPI1_TX DMA request. */

    /* PA9 USART1_TX and PA10 USART1_RX, alternate function 1. */
    gpio_set_mode(GPIOA_BASE, 9, GPIO_MODE_ALT);
    gpio_set_mode(GPIOA_BASE, 10, GPIO_MODE_ALT);
    GPIO_AFRH(GPIOA_BASE) = (GPIO_AFRH(GPIOA_BASE) & ~((0xFUL << 4) | (0xFUL << 8))) |
                            (1UL << 4) | (1UL << 8);

    /* Default HSI16 clock: 16 MHz / 250000 baud = BRR 64. */
    uart_rx_head = 0;
    uart_rx_tail = 0;
    uart_rx_overflow_count = 0;
    USART1_BRR = 64U;
    USART1_CR1 = (1U << 0) | (1U << 2) | (1U << 3) | (1U << 5);
    NVIC_ISER = (1UL << 27);

    /* ADC synchronous clock from HCLK, regulator startup, self-calibration,
       12-bit single conversion on channel 0. */
    ADC1_CFGR2 = (ADC1_CFGR2 & ~(3UL << 30)) | (1UL << 30);
    ADC1_CR |= (1U << 28);
    for (volatile uint32_t delay = 0; delay < 400U; ++delay) {
    }
    ADC1_CR |= (1UL << 31);
    while ((ADC1_CR & (1UL << 31)) != 0U) {
    }
    ADC1_CFGR1 = 0;
    ADC1_SMPR = 7U;
    ADC1_CHSELR = 1U;
    ADC1_ISR = 1U;
    ADC1_CR |= 1U;
    while ((ADC1_ISR & 1U) == 0U) {
    }

    /* 8 kHz SysTick: 64 PWM steps gives a 125 Hz cycle. */
    SYST_RVR = 1999U;
    SYST_CVR = 0;
    SYST_CSR = 7U;
}

uint16_t hardware_read_buttons(void)
{
    uint16_t result = 0;
    for (uint8_t i = 0; i < BUTTON_COUNT; ++i) {
        if ((GPIO_IDR(button_pins[i].port) &
             ((uint32_t)1U << button_pins[i].number)) == 0U) {
            result |= (uint16_t)1U << i;
        }
    }
    return result;
}

uint16_t hardware_read_adc(void)
{
    ADC1_CR |= (1U << 2);
    while ((ADC1_ISR & (1U << 2)) == 0U) {
    }
    return (uint16_t)ADC1_DR;
}

void hardware_write_button_leds(uint16_t on_mask)
{
    uint32_t set_a = 0, reset_a = 0, set_b = 0, reset_b = 0;
    uint32_t set_c = 0, reset_c = 0, set_d = 0, reset_d = 0;

    for (uint8_t i = 0; i < BUTTON_COUNT; ++i) {
        uint32_t *set = &set_a;
        uint32_t *reset = &reset_a;
        if (led_pins[i].port == GPIOB_BASE) { set = &set_b; reset = &reset_b; }
        else if (led_pins[i].port == GPIOC_BASE) { set = &set_c; reset = &reset_c; }
        else if (led_pins[i].port == GPIOD_BASE) { set = &set_d; reset = &reset_d; }
        if ((on_mask & ((uint16_t)1U << i)) != 0U) {
            *set |= (uint32_t)1U << led_pins[i].number;
        } else {
            *reset |= (uint32_t)1U << led_pins[i].number;
        }
    }
    GPIO_BSRR(GPIOA_BASE) = set_a | (reset_a << 16);
    GPIO_BSRR(GPIOB_BASE) = set_b | (reset_b << 16);
    GPIO_BSRR(GPIOC_BASE) = set_c | (reset_c << 16);
    GPIO_BSRR(GPIOD_BASE) = set_d | (reset_d << 16);
}

void hardware_write_sensor_drive(uint8_t high)
{
    if (high != 0U) {
        GPIO_BSRR(GPIOA_BASE) = (uint32_t)1U << 11;
    } else {
        GPIO_BSRR(GPIOA_BASE) = (uint32_t)1U << (11U + 16U);
    }
}

void hardware_write_addressable(const uint8_t *data, uint16_t length)
{
    uint16_t encoded_length = LED_RESET_BYTES;
    uint8_t accumulator = 0;
    uint8_t accumulated_bits = 0;
    if (length > STRIP_MAX_PIXELS * 3U) return;

    for (uint8_t i = 0; i < LED_RESET_BYTES; ++i) led_dma_frame[i] = 0;

    /* Datasheet-compliant 1.25 us symbols at 8 MHz:
       0 = 1110000000 (0.375 us high, 0.875 us low)
       1 = 1111100000 (0.625 us high, 0.625 us low).
       Pack the 10-bit symbols without gaps before starting DMA. */
    for (uint16_t i = 0; i < length; ++i) {
        uint8_t value = data[i];
        for (uint8_t bit = 0; bit < 8U; ++bit) {
            uint16_t symbol = (value & 0x80U) != 0U ? 0x03E0U : 0x0380U;
            for (uint8_t symbol_bit = 0; symbol_bit < 10U; ++symbol_bit) {
                accumulator = (uint8_t)((accumulator << 1) |
                    ((symbol & 0x0200U) != 0U ? 1U : 0U));
                symbol <<= 1;
                if (++accumulated_bits == 8U) {
                    led_dma_frame[encoded_length++] = accumulator;
                    accumulator = 0;
                    accumulated_bits = 0;
                }
            }
            value <<= 1;
        }
    }

    for (uint8_t i = 0; i < LED_RESET_BYTES; ++i) {
        led_dma_frame[encoded_length++] = 0;
    }

    /* Every source byte contributes 80 bits, so the packed frame ends on an
       exact byte boundary. The reset preamble absorbs DMA startup latency;
       no timing-sensitive peripheral mode transition is required. */
    DMA1_CCR1 &= ~1U;
    DMA1_IFCR = 0x0FU;
    DMA1_CMAR1 = (uint32_t)(uintptr_t)led_dma_frame;
    DMA1_CNDTR1 = encoded_length;
    DMA1_CCR1 = (2U << 12) | (1U << 7) | (1U << 4) | 1U;
    while (DMA1_CNDTR1 != 0U) {
    }
    while ((SPI1_SR & (1U << 7)) != 0U) {
    }
    DMA1_CCR1 &= ~1U;
}

void hardware_request_rom_bootloader(void)
{
    extern void bootloader_enter_system_memory(void);
    irq_disable();
    while ((USART1_ISR & (1U << 6)) == 0U) {
    }
    SYST_CSR = 0;
    NVIC_ICER = 0xFFFFFFFFUL;
    NVIC_ICPR = 0xFFFFFFFFUL;
    /* Return the application peripherals to reset state before the ROM owns
       PA9/PA10. This avoids relying on an SRAM marker surviving reset. */
    RCC_APBRSTR2 |= RCC_USART1_EN | RCC_SPI1_EN | RCC_ADC_EN;
    RCC_APBRSTR2 &= ~(RCC_USART1_EN | RCC_SPI1_EN | RCC_ADC_EN);
    RCC_AHBRSTR |= RCC_DMA1_EN;
    RCC_AHBRSTR &= ~RCC_DMA1_EN;
    bootloader_enter_system_memory();
}

void hardware_uart_write(const uint8_t *data, uint8_t length)
{
    for (uint8_t i = 0; i < length; ++i) {
        while ((USART1_ISR & (1U << 7)) == 0U) {
        }
        USART1_TDR = data[i];
    }
}

void hardware_uart_drain(void)
{
    while ((USART1_ISR & (1U << 6)) == 0U) {
    }
}

int16_t hardware_uart_read(void)
{
    const uint8_t tail = uart_rx_tail;
    if (tail == uart_rx_head) {
        return -1;
    }
    const uint8_t value = uart_rx_buffer[tail];
    uart_rx_tail = (uint8_t)((tail + 1U) & UART_RX_BUFFER_MASK);
    return (int16_t)value;
}

uint16_t hardware_uart_rx_overflows(void)
{
    return uart_rx_overflow_count;
}

void USART1_IRQHandler(void)
{
    const uint32_t status = USART1_ISR;

    if ((status & (1U << 5)) != 0U) {
        const uint8_t value = (uint8_t)USART1_RDR;
        const uint8_t head = uart_rx_head;
        const uint8_t next = (uint8_t)((head + 1U) & UART_RX_BUFFER_MASK);
        if (next == uart_rx_tail) {
            if (uart_rx_overflow_count != UINT16_MAX) {
                ++uart_rx_overflow_count;
            }
        } else {
            uart_rx_buffer[head] = value;
            uart_rx_head = next;
        }
    }

    /* Clear parity, framing, noise, and overrun errors. */
    if ((status & 0x0FU) != 0U) {
        USART1_ICR = 0x0FU;
    }
}
