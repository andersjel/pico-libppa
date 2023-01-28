#include <ppa/ppa.h>

#include <assert.h>
#include <hardware/address_mapped.h>
#include <hardware/clocks.h>
#include <hardware/dma.h>
#include <hardware/gpio.h>
#include <hardware/irq.h>
#include <hardware/pwm.h>
#include <hardware/structs/dma.h>
#include <hardware/structs/pwm.h>
#include <hardware/sync.h>
#include <pico/binary_info.h>
#include <ppa/chk.h>
#include <ppa/cond.h>
#include <sys/queue.h>

#ifndef PPA_SAMPLE_HZ
#define PPA_SAMPLE_HZ 22050
#endif

#ifndef PPA_BITS
#define PPA_BITS 10
#endif

#ifndef PPA_DMA_IRQ_N
#define PPA_DMA_IRQ_N 0
#endif

#if !defined(PPA_GPIO_A) && !defined(PPA_GPIO_B)
#error "PPA_GPIO_A or PPA_GPIO_B must be defined"
#endif

#ifdef PPA_GPIO_A
static_assert((PPA_GPIO_A & 1u) == PWM_CHAN_A);
#endif

#ifdef PPA_GPIO_B
static_assert((PPA_GPIO_B & 1u) == PWM_CHAN_B);
#endif

#if defined(PPA_GPIO_A) && defined(PPA_GPIO_B)
static_assert(((PPA_GPIO_A >> 1u) & 7u) == ((PPA_GPIO_B >> 1u) & 7u));
#endif

#ifdef PPA_GPIO_A
#define PPA_PWM_SLICE (pwm_gpio_to_slice_num(PPA_GPIO_A))
#else
#define PPA_PWM_SLICE (pwm_gpio_to_slice_num(PPA_GPIO_B))
#endif

STAILQ_HEAD(desc_list, ppa_desc);

// Configuration.
static uint8_t g_timer;
static uint8_t g_dma[2];

// State
static spin_lock_t*     g_lock;
static struct cond_var  g_cond_var;
static struct desc_list g_queued;
static struct desc_list g_done;
static struct ppa_desc* g_running;
static struct ppa_desc* g_pending;
// An index into g_dma[], i.e. 0 or 1.
static uint8_t          g_running_index;

static uint8_t running_chan()
{
    return g_dma[g_running_index];
}

static uint8_t pending_chan()
{
    return g_dma[!g_running_index];
}

static uint32_t channel_bits()
{
    return (1u << g_dma[0]) | (1u << g_dma[1]);
}

static struct ppa_desc* pop_queued()
{
    struct ppa_desc* rval = STAILQ_FIRST(&g_queued);
    if (rval) {
        STAILQ_REMOVE_HEAD(&g_queued, entry);
    }
    return rval;
}

static void set_channel_ctrl_register(
    uint8_t chan, uint8_t chain_to, struct ppa_desc* desc, bool trigger)
{
    dma_channel_config cfg = dma_channel_get_default_config(chan);
    channel_config_set_dreq(&cfg, dma_get_timer_dreq(g_timer));
    channel_config_set_transfer_data_size(&cfg, desc->stereo ? DMA_SIZE_32 : DMA_SIZE_16);
    channel_config_set_chain_to(&cfg, chain_to);
    dma_channel_set_config(chan, &cfg, trigger);
}

static void prepare_transaction(uint8_t chan, struct ppa_desc* desc, bool trigger)
{
    void* addr = desc->stereo ? (void*)desc->data_stereo : (void*)desc->data_mono;
    dma_channel_set_read_addr(chan, addr, false);
    dma_channel_set_trans_count(chan, desc->size, false);
    set_channel_ctrl_register(chan, chan, desc, trigger);
}

static void handle_completed_transaction()
{
    // Transfer running to done and pending to running.
    STAILQ_INSERT_TAIL(&g_done, g_running, entry);
    g_running       = g_pending;
    g_pending       = NULL;
    g_running_index = !g_running_index;

    // Make sure that the dma is running, if it was not already
    // triggered by chain_to.
    if (g_running) {
        dma_start_channel_mask(1 << running_chan());
    }

    cond_var_wake(&g_cond_var);
}

static void ppa_update()
{
    for (;;) {
        // Make sure we have a running transaction.
        if (!g_running) {
            g_running = pop_queued();
            if (!g_running)
                return;
            prepare_transaction(running_chan(), g_running, true);
        }

        // Check if we can set up a pending transaction.
        if (!g_pending) {
            g_pending = pop_queued();
            if (g_pending) {
                prepare_transaction(pending_chan(), g_pending, false);
                set_channel_ctrl_register(running_chan(), pending_chan(), g_pending, false);
            }
        }

        // If the channel is now busy with the current dma, then we are done.
        // Otherwise we need to handle the completed dma and flush again.
        if (dma_channel_is_busy(running_chan())) {
            return;
        } else {
            handle_completed_transaction();
        }
    }
}

static void ppa_irq()
{
    // Acknowledge interrupts.
    hw_set_bits(PPA_DMA_IRQ_N ? &dma_hw->ints1 : &dma_hw->ints0, channel_bits());

    spin_lock_unsafe_blocking(g_lock);
    ppa_update();
    spin_unlock_unsafe(g_lock);
}

static void initialize_pwm()
{
    pwm_config pwm_cfg = pwm_get_default_config();
    pwm_config_set_wrap(&pwm_cfg, (1u << PPA_BITS) - 2u);
    pwm_init(PPA_PWM_SLICE, &pwm_cfg, true);
    pwm_set_both_levels(PPA_PWM_SLICE, 0x7fu, 0x7fu);
#ifdef PPA_GPIO_A
    gpio_set_function(PPA_GPIO_A, GPIO_FUNC_PWM);
#endif
#ifdef PPA_GPIO_B
    gpio_set_function(PPA_GPIO_B, GPIO_FUNC_PWM);
#endif
}

static void initialize_channels()
{
    // Set the write address of both channels to target the cc register of the
    // PWM slice. The register has two 16-bit fields that control the level of
    // each channel.
    //
    // Note, if a narrow write is done to a hardware register, the value is
    // replicated across. Therefore, to write both channel levels we can just do
    // a 16-bit write to the register.
    uintptr_t write_addr = &pwm_hw->slice[PPA_PWM_SLICE].cc;
    dma_channel_set_write_addr(g_dma[1], write_addr, false);
    dma_channel_set_write_addr(g_dma[0], write_addr, false);
}

static void initialize_timer()
{
    // We need to configure a fractional timer (x/y)*clk_sys = sample_rate with
    // x and y being 16 bit. Since the fraction is less than 1, y will be larger
    // that x. We will probably get more precision if we make both x and y
    // large, so lets try and fix y at 0xffffu and see what x we get.
    uint32_t sys = clock_get_hz(clk_sys);
    uint16_t y   = 0xffffu;
    uint16_t x   = (uint64_t)y * PPA_SAMPLE_HZ / sys;

    // Then, let us see what y we get from this.
    y = (uint64_t)x * sys / PPA_SAMPLE_HZ;

    // As an example, if sys is 133000000 and sample_rate is 22050, we get:
    // x = 10, y = 60317, x/y = 22050.17.

    dma_timer_set_fraction(g_timer, x, y);
}

static void initialize_interrupt_handler()
{
    uint irq = PPA_DMA_IRQ_N ? DMA_IRQ_1 : DMA_IRQ_0;
    irq_add_shared_handler(irq, ppa_irq, PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY);
    irq_set_enabled(irq, true);
    dma_irqn_set_channel_mask_enabled(PPA_DMA_IRQ_N, channel_bits(), true);
}

bool ppa_init(bool required)
{
#if defined(PPA_GPIO_A) && defined(PPA_GPIO_B)
    bi_decl_if_func_used(bi_1pin_with_name(PPA_GPIO_A, "AUDIO_OUT_LEFT"));
    bi_decl_if_func_used(bi_1pin_with_func(PPA_GPIO_A, GPIO_FUNC_PWM));
    bi_decl_if_func_used(bi_1pin_with_name(PPA_GPIO_B, "AUDIO_OUT_RIGHT"));
    bi_decl_if_func_used(bi_1pin_with_func(PPA_GPIO_B, GPIO_FUNC_PWM));
#elif defined(PPA_GPIO_A)
    bi_decl_if_func_used(bi_1pin_with_name(PPA_GPIO_A, "AUDIO_OUT"));
    bi_decl_if_func_used(bi_1pin_with_func(PPA_GPIO_A, GPIO_FUNC_PWM));
#elif defined(PPA_GPIO_B)
    bi_decl_if_func_used(bi_1pin_with_name(PPA_GPIO_B, "AUDIO_OUT"));
    bi_decl_if_func_used(bi_1pin_with_func(PPA_GPIO_B, GPIO_FUNC_PWM));
#endif

    // Claim resources.
    g_timer      = CHK_NEG1(dma_claim_unused_timer(required), fail);
    g_dma[0]     = CHK_NEG1(dma_claim_unused_channel(required), fail_timer);
    g_dma[1]     = CHK_NEG1(dma_claim_unused_channel(required), fail_chan_0);
    int lock_num = CHK_NEG1(spin_lock_claim_unused(required), fail_chan_1);
    g_lock       = spin_lock_init(lock_num);

    STAILQ_INIT(&g_queued);
    STAILQ_INIT(&g_done);

    initialize_pwm();
    initialize_channels();
    initialize_timer();
    initialize_interrupt_handler();

    return true;

fail_chan_1:
    dma_channel_unclaim(g_dma[1]);
fail_chan_0:
    dma_channel_unclaim(g_dma[0]);
fail_timer:
    dma_timer_unclaim(g_timer);
fail:
    return false;
}

void ppa_queue(struct ppa_desc* desc)
{
    uint32_t save = spin_lock_blocking(g_lock);
    STAILQ_INSERT_TAIL(&g_queued, desc, entry);
    if (!g_running || !g_pending) {
        ppa_update();
    }
    spin_unlock(g_lock, save);
}

static struct ppa_desc* do_ppa_poll()
{
    struct ppa_desc* rval = STAILQ_FIRST(&g_done);
    if (rval) {
        STAILQ_REMOVE_HEAD(&g_done, entry);
    }
    return rval;
}

struct ppa_desc* ppa_poll()
{
    uint32_t         save = spin_lock_blocking(g_lock);
    struct ppa_desc* rval = do_ppa_poll();
    spin_unlock(g_lock, save);
    return rval;
}

struct ppa_desc* ppa_poll_blocking()
{
    uint32_t         save = spin_lock_blocking(g_lock);
    struct ppa_desc* rval = do_ppa_poll();
    while (!rval) {
        cond_var_wait(g_lock, &g_cond_var);
        rval = do_ppa_poll();
    }
    spin_unlock(g_lock, save);
    return rval;
}
