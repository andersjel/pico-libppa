#include <math.h>
#include <pico/stdio.h>
#include <ppa/ppa.h>
#include <stdalign.h>
#include <stdint.h>
#include <stdio.h>

#define BUFFER_SIZE 22050

struct buffer {
    struct ppa_desc desc;
    uint16_t        data[BUFFER_SIZE];
};

static struct buffer buffers[2];

int main()
{
    stdio_init_all();

    puts("Initializing PPA");
    ppa_init(true);

    for (unsigned i = 0; i < 2; i++) {
        for (unsigned j = 0; j < BUFFER_SIZE; j++) {
            float x            = j * M_PI / (BUFFER_SIZE - 1u) * 50;
            buffers[i].data[j] = roundf((sinf(x) + 1) / 2 * 1023);
        }
        buffers[i].desc.size      = BUFFER_SIZE;
        buffers[i].desc.data_mono = buffers[i].data;
    }

    ppa_queue(&buffers[0].desc);
    ppa_queue(&buffers[1].desc);
    puts("Started");

    for (;;) {
        struct ppa_desc* desc = ppa_poll_blocking();
        printf("Resubmitting %x\n", (unsigned)desc);
        ppa_queue(desc);
    }
}
