#include "spi1_bus_arbiter.h"

#include "aik_spi1_bus_arbiter.h"
#include "ch32h417_gpio.h"
#include "ch32h417_hsem.h"
#include "ch32h417_rcc.h"
#include "ch32h417_spi.h"
#include "ch585_link.h"

#define V3F_SPI1_QUIESCE_POLLS 500000UL

typedef enum
{
    V3F_SPI1_ARB_FATAL = 0,
    V3F_SPI1_ARB_ACTIVE,
    V3F_SPI1_ARB_PAUSED,
} v3f_spi1_arb_local_state_t;

static v3f_spi1_arb_local_state_t s_state;
static uint32_t s_epoch;
static uint32_t s_transitions;

static uint8_t request_header_valid(
    const volatile aik_spi1_arb_request_t *request)
{
    return (uint8_t)((request->magic == AIK_SPI1_ARB_MAGIC) &&
                     (request->version == AIK_SPI1_ARB_VERSION) &&
                     (request->slot_bytes == AIK_SPI1_ARB_SLOT_BYTES));
}

static uint8_t v3f_owns_hsem(void)
{
    aik_spi1_arb_fence();
    return (uint8_t)(HSEM->RX[AIK_SPI1_ARB_HSEM_ID] ==
                     AIK_SPI1_ARB_HSEM_V3F_OWNER);
}

static void publish_response(uint32_t epoch,
                             uint32_t state,
                             uint32_t error,
                             uint32_t flags)
{
    volatile aik_spi1_arb_response_t *response =
        &AIK_SPI1_ARB_MAILBOX->response;

    response->state = AIK_SPI1_ARB_STATE_RESET;
    aik_spi1_arb_fence();
    response->magic = AIK_SPI1_ARB_MAGIC;
    response->version = AIK_SPI1_ARB_VERSION;
    response->slot_bytes = AIK_SPI1_ARB_SLOT_BYTES;
    response->epoch = epoch;
    response->error = error;
    response->hsem_snapshot = HSEM->RX[AIK_SPI1_ARB_HSEM_ID];
    response->transition_count = s_transitions;
    response->flags = flags;
    aik_spi1_arb_fence();
    /* State is the commit word and is always written last. */
    response->state = state;
    aik_spi1_arb_fence();
}

static void initialize_mailbox(void)
{
    volatile uint32_t *words =
        (volatile uint32_t *)(uintptr_t)AIK_SPI1_ARB_MAILBOX_ADDR;
    uint32_t i;

    for(i = 0U; i < (AIK_SPI1_ARB_MAILBOX_BYTES / sizeof(uint32_t)); i++)
    {
        words[i] = 0U;
    }
    AIK_SPI1_ARB_MAILBOX->request.magic = AIK_SPI1_ARB_MAGIC;
    AIK_SPI1_ARB_MAILBOX->request.version = AIK_SPI1_ARB_VERSION;
    AIK_SPI1_ARB_MAILBOX->request.slot_bytes = AIK_SPI1_ARB_SLOT_BYTES;
    aik_spi1_arb_fence();
}

static uint8_t quiesce_spi1(void)
{
    GPIO_InitTypeDef gpio = {0};
    uint32_t polls = V3F_SPI1_QUIESCE_POLLS;
    uint8_t drain = 16U;

    RCC_HB2PeriphClockCmd(RCC_HB2Periph_AFIO |
                          RCC_HB2Periph_GPIOB |
                          RCC_HB2Periph_GPIOD |
                          RCC_HB2Periph_GPIOF |
                          RCC_HB2Periph_SPI1, ENABLE);

    /* Both CH585s must be deselected before the clock/data pins can move. */
    GPIO_SetBits(GPIOF, GPIO_Pin_2);
    GPIO_SetBits(GPIOD, GPIO_Pin_9);
    gpio.GPIO_Pin = GPIO_Pin_2;
    gpio.GPIO_Speed = GPIO_Speed_Low;
    gpio.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(GPIOF, &gpio);
    gpio.GPIO_Pin = GPIO_Pin_9;
    GPIO_Init(GPIOD, &gpio);

    while(SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_BSY) != RESET)
    {
        if(polls-- == 0U)
        {
            return 0U;
        }
    }

    /* V3F uses polling today, but clear every producer before granting V5F. */
    SPI1->CTLR2 &= (uint16_t)~(SPI_CTLR2_RXDMAEN |
                               SPI_CTLR2_TXDMAEN |
                               SPI_CTLR2_ERRIE |
                               SPI_CTLR2_RXNEIE |
                               SPI_CTLR2_TXEIE);
    SPI_Cmd(SPI1, DISABLE);
    while((drain-- != 0U) &&
          (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_RXNE) != RESET))
    {
        (void)SPI_I2S_ReceiveData(SPI1);
    }
    (void)SPI1->STATR;
    SPI_I2S_DeInit(SPI1);

    /*
     * V5F owns PF6..PF9; V3F must electrically and logically disappear from
     * PB3..PB5.  CH32H417 gives SPI1_MISO candidates a fixed priority:
     * PA6 > PB4 > PF3 > PF9.  Therefore GPIO_Mode_AIN alone is insufficient:
     * while PB4's AF selector remains AF5, it still wins over the Flash MISO
     * on PF9.  Move all three CH585 pins away from SPI1 AF5 as part of the
     * grant, then disconnect their digital paths.  resume_spi1() performs the
     * full CH585 init and restores AF5 after V5F releases the lease.
     */
    GPIO_PinAFConfig(GPIOB, GPIO_PinSource3, GPIO_AF15);
    GPIO_PinAFConfig(GPIOB, GPIO_PinSource4, GPIO_AF15);
    GPIO_PinAFConfig(GPIOB, GPIO_PinSource5, GPIO_AF15);
    gpio.GPIO_Pin = GPIO_Pin_3 | GPIO_Pin_4 | GPIO_Pin_5;
    gpio.GPIO_Speed = GPIO_Speed_Low;
    gpio.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(GPIOB, &gpio);
    aik_spi1_arb_fence();
    return 1U;
}

static uint8_t resume_spi1(void)
{
    if(HSEM_Take(HSEM_ID31, AIK_SPI1_ARB_HSEM_V3F_PID) != READY)
    {
        return 0U;
    }
    if(v3f_owns_hsem() == 0U)
    {
        return 0U;
    }

    v3f_ch585_link_restore_idle();
    aik_spi1_arb_fence();
    return 1U;
}

uint8_t v3f_spi1_bus_arbiter_init(void)
{
    initialize_mailbox();
    s_epoch = 0U;
    s_transitions = 0U;
    s_state = V3F_SPI1_ARB_FATAL;

    if(HSEM_Take(HSEM_ID31, AIK_SPI1_ARB_HSEM_V3F_PID) != READY)
    {
        publish_response(0U, AIK_SPI1_ARB_STATE_FATAL,
                         AIK_SPI1_ARB_ERROR_HSEM_BOOT_BUSY, 0U);
        return 0U;
    }

    s_state = V3F_SPI1_ARB_ACTIVE;
    publish_response(0U, AIK_SPI1_ARB_STATE_V3_ACTIVE,
                     AIK_SPI1_ARB_ERROR_NONE, 0U);
    return 1U;
}

uint8_t v3f_spi1_bus_arbiter_service(void)
{
    volatile aik_spi1_arb_request_t *request =
        &AIK_SPI1_ARB_MAILBOX->request;
    uint32_t epoch;
    uint32_t command;

    if(s_state == V3F_SPI1_ARB_FATAL)
    {
        return 0U;
    }

    if((s_state == V3F_SPI1_ARB_ACTIVE) && (v3f_owns_hsem() == 0U))
    {
        /* Ownership loss is never repaired by touching SPI1.  Stop all CH585
         * traffic and expose a fail-closed state to V5F. */
        s_state = V3F_SPI1_ARB_FATAL;
        s_transitions++;
        publish_response(s_epoch, AIK_SPI1_ARB_STATE_FATAL,
                         AIK_SPI1_ARB_ERROR_HSEM_RETAKE, 0U);
        return 0U;
    }

    aik_spi1_arb_fence();
    if(request_header_valid(request) == 0U)
    {
        /* Invalid metadata never causes V3F to release or retake HSEM31. */
        return (uint8_t)(s_state == V3F_SPI1_ARB_ACTIVE);
    }
    epoch = request->epoch;
    command = request->command;
    aik_spi1_arb_fence();

    if(s_state == V3F_SPI1_ARB_ACTIVE)
    {
        /* A request may be withdrawn before V3F reaches the grant boundary.
         * Acknowledge the same epoch without disturbing SPI1/HSEM ownership. */
        if((command == AIK_SPI1_ARB_CMD_CANCEL) && (epoch != 0U))
        {
            const volatile aik_spi1_arb_response_t *response =
                &AIK_SPI1_ARB_MAILBOX->response;

            /* Idempotent, including CANCEL after a rejected ACQUIRE that
             * already recorded the same epoch. */
            if((s_epoch != epoch) || (response->epoch != epoch) ||
               (response->state != AIK_SPI1_ARB_STATE_V3_ACTIVE) ||
               ((response->flags & AIK_SPI1_ARB_RESPONSE_RESTORED) == 0U))
            {
                s_epoch = epoch;
                s_transitions++;
                publish_response(epoch, AIK_SPI1_ARB_STATE_V3_ACTIVE,
                                 AIK_SPI1_ARB_ERROR_NONE,
                                 AIK_SPI1_ARB_RESPONSE_RESTORED);
            }
            return 1U;
        }
        if((command == AIK_SPI1_ARB_CMD_ACQUIRE) &&
           (epoch != 0U) && (epoch != s_epoch))
        {
            if((v3f_owns_hsem() == 0U) || (quiesce_spi1() == 0U))
            {
                s_epoch = epoch;
                s_transitions++;
                publish_response(epoch, AIK_SPI1_ARB_STATE_REJECTED,
                                 AIK_SPI1_ARB_ERROR_SPI_BUSY_TIMEOUT, 0U);
                return 1U;
            }

            s_epoch = epoch;
            s_transitions++;
            HSEM_ReleaseOneSem(HSEM_ID31, AIK_SPI1_ARB_HSEM_V3F_PID);
            aik_spi1_arb_fence();
            s_state = V3F_SPI1_ARB_PAUSED;
            publish_response(epoch, AIK_SPI1_ARB_STATE_QUIESCED,
                             AIK_SPI1_ARB_ERROR_NONE, 0U);
            return 0U;
        }
        return 1U;
    }

    /* While paused, only the matching V5F transaction may release/cancel. */
    if((epoch == s_epoch) &&
       ((command == AIK_SPI1_ARB_CMD_RELEASE) ||
        (command == AIK_SPI1_ARB_CMD_CANCEL)))
    {
        if(resume_spi1() != 0U)
        {
            s_state = V3F_SPI1_ARB_ACTIVE;
            s_transitions++;
            publish_response(epoch, AIK_SPI1_ARB_STATE_V3_ACTIVE,
                             AIK_SPI1_ARB_ERROR_NONE,
                             AIK_SPI1_ARB_RESPONSE_RESTORED);
            return 1U;
        }
        /* V5F may still own HSEM31.  Never steal/reset the peripheral. */
    }
    return 0U;
}

uint8_t v3f_spi1_bus_arbiter_transfer_allowed(void)
{
    return (uint8_t)((s_state == V3F_SPI1_ARB_ACTIVE) &&
                     (v3f_owns_hsem() != 0U));
}
