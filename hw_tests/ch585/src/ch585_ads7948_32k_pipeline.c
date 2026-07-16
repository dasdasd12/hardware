#include "ch585_common.h"

#include "CH585SFR.h"
#include "CH58x_gpio.h"
#include "CH58x_spi.h"
#include "CH58x_sys.h"
#include "core_riscv.h"

#define ACQ32_TARGET_HZ 32000U
#define ACQ32_SYSCLK_HZ 78000000U
#define ACQ32_PERIOD_BASE (ACQ32_SYSCLK_HZ / ACQ32_TARGET_HZ)
#define ACQ32_PERIOD_REMAINDER (ACQ32_SYSCLK_HZ % ACQ32_TARGET_HZ)
#define ACQ32_PERIOD_CEIL ((ACQ32_SYSCLK_HZ + ACQ32_TARGET_HZ - 1U) / ACQ32_TARGET_HZ)
#define ACQ32_WINDOW_FRAMES 4096U
#define ACQ32_LANE_STRIDE 16U
#define ACQ32_RAW_COUNT 64U

#define ACQ32_SPI_SCK_PA0 GPIO_Pin_0
#define ACQ32_SPI_MOSI_PA1 GPIO_Pin_1
#define ACQ32_SPI_MISO_PA2 GPIO_Pin_2
#define ACQ32_MUX_SEL_MASK (GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_3)
#define ACQ32_CH_SEL GPIO_Pin_18
#define ACQ32_PDEN GPIO_Pin_19

#if defined(CH585_ADC_MUX_HALF_RIGHT)
#define ACQ32_HALF_NAME "right"
#define ACQ32_ADC0_CS GPIO_Pin_15
#define ACQ32_ADC1_CS GPIO_Pin_14
#define ACQ32_CH0_ADC0_COUNT 10U
#define ACQ32_CH0_ADC1_COUNT 10U
#define ACQ32_CH1_ADC0_COUNT 10U
#define ACQ32_CH1_ADC1_COUNT 11U
#else
#define ACQ32_HALF_NAME "left"
#define ACQ32_ADC0_CS GPIO_Pin_14
#define ACQ32_ADC1_CS GPIO_Pin_15
#define ACQ32_CH0_ADC0_COUNT 9U
#define ACQ32_CH0_ADC1_COUNT 9U
#define ACQ32_CH1_ADC0_COUNT 9U
#define ACQ32_CH1_ADC1_COUNT 9U
#endif

static uint16_t s_acq32_raw[ACQ32_RAW_COUNT];
static uint32_t s_acq32_adc_reads;

static uint32_t acq32_counter(void)
{
    return SysTick->CNTL;
}

static void acq32_counter_init(void)
{
    SysTick->CTLR = 0U;
    SysTick->CNTL = 0U;
    SysTick->CMPL = 0xFFFFFFFFU;
    SysTick->CTLR = SysTick_CTLR_STRE | SysTick_CTLR_STCLK | SysTick_CTLR_STE;
}

static uint32_t acq32_next_period(uint32_t *remainder)
{
    uint32_t period = ACQ32_PERIOD_BASE;

    *remainder += ACQ32_PERIOD_REMAINDER;
    if(*remainder >= ACQ32_TARGET_HZ)
    {
        *remainder -= ACQ32_TARGET_HZ;
        period++;
    }
    return period;
}

static void acq32_set_mux(uint8_t mux)
{
    uint32_t output = R32_PB_OUT & ~(uint32_t)ACQ32_MUX_SEL_MASK;
    output |= (uint32_t)(mux & 0x0FU);
    R32_PB_OUT = output;
}

static void acq32_set_channel(uint8_t channel)
{
    if(channel != 0U)
    {
        R32_PB_OUT |= (uint32_t)ACQ32_CH_SEL;
    }
    else
    {
        R32_PB_OUT &= ~(uint32_t)ACQ32_CH_SEL;
    }
}

static uint8_t acq32_spi_read_byte(void)
{
    R8_SPI1_BUFFER = 0xFFU;
    while((R8_SPI1_INT_FLAG & RB_SPI_FREE) == 0U)
    {
    }
    return R8_SPI1_BUFFER;
}

static uint16_t acq32_read_adc(uint32_t cs_pin)
{
    uint8_t hi;
    uint8_t lo;

    R32_PB_OUT &= ~cs_pin;
    hi = acq32_spi_read_byte();
    lo = acq32_spi_read_byte();
    R32_PB_OUT |= cs_pin;
    s_acq32_adc_reads++;
    return (uint16_t)((((uint16_t)hi << 8U) | lo) >> 6U) & 0x03FFU;
}

static void acq32_scan_channel(uint8_t channel,
                               uint8_t adc0_count,
                               uint8_t adc1_count)
{
    uint8_t mux;
    uint8_t max_count = (adc0_count > adc1_count) ? adc0_count : adc1_count;
    uint8_t adc0_previous_valid = 0U;
    uint8_t adc1_previous_valid = 0U;
    uint8_t adc0_previous_mux = 0U;
    uint8_t adc1_previous_mux = 0U;
    uint8_t adc0_lane = channel;
    uint8_t adc1_lane = (uint8_t)(2U + channel);

    acq32_set_channel(channel);
    for(mux = 0U; mux < max_count; mux++)
    {
        acq32_set_mux(mux);

        if(mux < adc0_count)
        {
            uint16_t code = acq32_read_adc(ACQ32_ADC0_CS);
            if(adc0_previous_valid != 0U)
            {
                s_acq32_raw[(uint16_t)adc0_lane * ACQ32_LANE_STRIDE +
                            adc0_previous_mux] = code;
            }
            adc0_previous_valid = 1U;
            adc0_previous_mux = mux;
        }

        if(mux < adc1_count)
        {
            uint16_t code = acq32_read_adc(ACQ32_ADC1_CS);
            if(adc1_previous_valid != 0U)
            {
                s_acq32_raw[(uint16_t)adc1_lane * ACQ32_LANE_STRIDE +
                            adc1_previous_mux] = code;
            }
            adc1_previous_valid = 1U;
            adc1_previous_mux = mux;
        }
    }

    if(adc0_previous_valid != 0U)
    {
        s_acq32_raw[(uint16_t)adc0_lane * ACQ32_LANE_STRIDE +
                    adc0_previous_mux] = acq32_read_adc(ACQ32_ADC0_CS);
    }
    if(adc1_previous_valid != 0U)
    {
        s_acq32_raw[(uint16_t)adc1_lane * ACQ32_LANE_STRIDE +
                    adc1_previous_mux] = acq32_read_adc(ACQ32_ADC1_CS);
    }
}

static void acq32_scan_frame(void)
{
    acq32_scan_channel(0U, ACQ32_CH0_ADC0_COUNT, ACQ32_CH0_ADC1_COUNT);
    acq32_scan_channel(1U, ACQ32_CH1_ADC0_COUNT, ACQ32_CH1_ADC1_COUNT);
}

static uint32_t acq32_checksum(void)
{
    uint32_t value = 2166136261UL;
    uint16_t i;

    for(i = 0U; i < ACQ32_RAW_COUNT; i++)
    {
        value ^= s_acq32_raw[i];
        value *= 16777619UL;
    }
    return value;
}

static void acq32_io_init(void)
{
    GPIOADigitalCfg(ENABLE,
                    ACQ32_SPI_SCK_PA0 | ACQ32_SPI_MOSI_PA1 |
                    ACQ32_SPI_MISO_PA2);
    GPIOBDigitalCfg(ENABLE,
                    ACQ32_MUX_SEL_MASK | ACQ32_ADC0_CS | ACQ32_ADC1_CS |
                    ACQ32_CH_SEL | ACQ32_PDEN);

    R32_PB_OUT |= (uint32_t)(ACQ32_ADC0_CS | ACQ32_ADC1_CS);
    R32_PB_OUT &= ~(uint32_t)(ACQ32_MUX_SEL_MASK | ACQ32_CH_SEL |
                              ACQ32_PDEN);
    GPIOB_ModeCfg(ACQ32_MUX_SEL_MASK | ACQ32_ADC0_CS | ACQ32_ADC1_CS |
                      ACQ32_CH_SEL | ACQ32_PDEN,
                  GPIO_ModeOut_PP_5mA);
    GPIOA_ModeCfg(ACQ32_SPI_SCK_PA0, GPIO_ModeOut_PP_20mA);
    GPIOA_ModeCfg(ACQ32_SPI_MISO_PA2, GPIO_ModeIN_PU);

    SPI1_MasterDefInit();
    SPI1_DataMode(Mode0_HighBitINFront);
    SPI1_CLKCfg(2U);
    R8_SPI1_CTRL_MOD &= (uint8_t)(~RB_SPI_MOSI_OE);
    R8_SPI1_CTRL_MOD |= RB_SPI_FIFO_DIR;
}

static void acq32_log_result(uint32_t min_cycles,
                             uint32_t max_cycles,
                             uint32_t total_cycles,
                             uint32_t overruns,
                             uint32_t elapsed_cycles,
                             uint32_t adc_reads)
{
    uint32_t average_cycles = total_cycles / ACQ32_WINDOW_FRAMES;
    uint32_t elapsed_us = elapsed_cycles / (ACQ32_SYSCLK_HZ / 1000000U);
    uint32_t effective_hz = elapsed_us != 0U ?
        (ACQ32_WINDOW_FRAMES * 1000000U) / elapsed_us : 0U;

    ch585_log_str("ACQ32_RESULT half=");
    ch585_log_str(ACQ32_HALF_NAME);
    ch585_log_str(" scheme=fixed_pipeline frames=");
    ch585_log_u32_dec(ACQ32_WINDOW_FRAMES);
    ch585_log_str(" target_hz=");
    ch585_log_u32_dec(ACQ32_TARGET_HZ);
    ch585_log_str(" budget_cycles=");
    ch585_log_u32_dec(ACQ32_PERIOD_CEIL);
    ch585_log_str(" min_cycles=");
    ch585_log_u32_dec(min_cycles);
    ch585_log_str(" max_cycles=");
    ch585_log_u32_dec(max_cycles);
    ch585_log_str(" avg_cycles=");
    ch585_log_u32_dec(average_cycles);
    ch585_log_str(" overruns=");
    ch585_log_u32_dec(overruns);
    ch585_log_str(" effective_hz=");
    ch585_log_u32_dec(effective_hz);
    ch585_log_str(" adc_reads=");
    ch585_log_u32_dec(adc_reads);
    ch585_log_str(" checksum=0x");
    ch585_log_u32_hex(acq32_checksum(), 8U);
    ch585_log_str("\r\n");
}

void ch585_ads7948_32k_pipeline_run(void)
{
    uint32_t remainder = 0U;
    uint32_t deadline;

    acq32_io_init();
    acq32_counter_init();
    acq32_scan_frame();
    acq32_scan_frame();

    ch585_log_str("DATA acq32 half=");
    ch585_log_str(ACQ32_HALF_NAME);
    ch585_log_str(" scheme=fixed_pipeline sysclk=");
    ch585_log_u32_dec(GetSysClock());
    ch585_log_str(" target_hz=");
    ch585_log_u32_dec(ACQ32_TARGET_HZ);
    ch585_log_str(" period_base=");
    ch585_log_u32_dec(ACQ32_PERIOD_BASE);
    ch585_log_str(" period_ceil=");
    ch585_log_u32_dec(ACQ32_PERIOD_CEIL);
    ch585_log_str(" spi_hz=39000000\r\n");

    deadline = acq32_counter();
    while(1)
    {
        uint32_t frame;
        uint32_t min_cycles = 0xFFFFFFFFU;
        uint32_t max_cycles = 0U;
        uint32_t total_cycles = 0U;
        uint32_t overruns = 0U;
        uint32_t reads_before = s_acq32_adc_reads;
        uint32_t window_start = acq32_counter();

        for(frame = 0U; frame < ACQ32_WINDOW_FRAMES; frame++)
        {
            uint32_t period = acq32_next_period(&remainder);
            uint32_t start = acq32_counter();
            uint32_t end;
            uint32_t used;

            deadline += period;
            acq32_scan_frame();
            end = acq32_counter();
            used = end - start;
            if(used < min_cycles)
            {
                min_cycles = used;
            }
            if(used > max_cycles)
            {
                max_cycles = used;
            }
            total_cycles += used;

            if((int32_t)(end - deadline) >= 0)
            {
                overruns++;
                deadline = end;
            }
            else
            {
                while((int32_t)(acq32_counter() - deadline) < 0)
                {
                }
            }
        }

        acq32_log_result(min_cycles,
                         max_cycles,
                         total_cycles,
                         overruns,
                         acq32_counter() - window_start,
                         s_acq32_adc_reads - reads_before);
        deadline = acq32_counter();
    }
}
