#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <sys/queue.h>

// The following macros configure the library when building it.
//
// * PPA_GPIO_A:    The GPIO to use for channel A (the left channel in stereo).
// * PPA_GPIO_B:    The GPIO to use for channel B (the right channel in stereo).
// * PPA_SAMPLE_HZ: The sample rate (default: 22050).
// * PPA_BITS:      Audio resolution in bits (default: 10).
// * PPA_DMA_IRQ_N: Controls what dma irq to use (0 or 1, default: 0).
//
// Either PPA_GPIO_A or PPA_GPIO_B are required.
//
// PPA_GPIO_A and PPA_GPIO_B must correspond to the A and B channels
// respectively of the same PWM slice.

struct ppa_desc {
    STAILQ_ENTRY(ppa_desc)
    entry;

    uint32_t size;
    bool     stereo;
    union {
        uint16_t* data_mono;
        uint32_t* data_stereo;
    };
};

bool             ppa_init(bool required);
void             ppa_queue(struct ppa_desc* buffer);
struct ppa_desc* ppa_poll();
struct ppa_desc* ppa_poll_blocking();
