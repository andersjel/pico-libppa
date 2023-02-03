#include <math.h>
#include <mp3dec.h>
#include <pico/stdio.h>
#include <ppa/ppa.h>
#include <stdalign.h>
#include <stdint.h>
#include <stdio.h>

#define BUFFER_SIZE  10240
#define BUFFER_COUNT 2

struct buffer {
    struct ppa_desc desc;
    uint16_t        data[BUFFER_SIZE];
};

static struct buffer buffers[BUFFER_COUNT];

extern uint8_t _binary_strauss_mp3_start[];
extern uint8_t _binary_strauss_mp3_end;

int main()
{
    stdio_init_all();
    puts("Running");

    uint8_t*       input      = _binary_strauss_mp3_start;
    const uint8_t* file_end   = &_binary_strauss_mp3_end;
    int            bytes_left = file_end - input;

    printf("input      = 0x%08x\n", (unsigned)input);
    printf("file_end   = 0x%08x\n", (unsigned)file_end);
    printf("bytes_left = %d\n", bytes_left);

    puts("Initializing mp3decoder");
    HMP3Decoder decoder = MP3InitDecoder();

    puts("Initializing PPA");
    ppa_init(true);

    puts("Preparing buffers.");
    for (size_t i = 0; i < BUFFER_COUNT; i++) {
        buffers[i].desc.stereo    = false;
        buffers[i].desc.data_mono = buffers[i].data;
        ppa_put_back(&buffers[i].desc);
    }

    while (input + 8 <= file_end) {
        struct ppa_desc* desc = ppa_poll_blocking();
        short*           data = (short*)desc->data_mono;
        desc->size            = 0;

        while (desc->size < BUFFER_SIZE - 1000) {
            printf("Header: %02x%02x%02x%02x\n", input[0], input[1], input[2], input[3]);
            int err = MP3Decode(decoder, &input, &bytes_left, data + desc->size, 0);
            if (err) {
                panic("err: %d", err);
            }

            MP3FrameInfo info = {};
            MP3GetLastFrameInfo(decoder, &info);

            desc->size += info.outputSamps;
        }

        for (size_t i = 0; i < desc->size; i++) {
            data[i] = (data[i] >> 5) + 512;
            if (data[i] < 0)
                data[i] = 0;
            if (data[i] > 1023)
                data[i] = 1023;
        }

        ppa_queue(desc);
    }

    panic("done");
}
