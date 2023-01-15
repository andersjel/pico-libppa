#include <cstdio>
#include <pico/stdio.h>
#include <pico/time.h>

int main()
{
    stdio_init_all();

    std::puts("Hello, World!");

    for (;;) {
        sleep_ms(100);
    }
}
