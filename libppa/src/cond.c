#include <hardware/sync.h>
#include <pico/platform.h>
#include <ppa/cond.h>
#include <stdint.h>

void cond_var_wait(spin_lock_t* lock, struct cond_var* cond_var)
{
    // Mark us as waiting.
    cond_var->waiters |= 1u << get_core_num();

    spin_unlock_unsafe(lock);

    // Enable interrupts.
    // clang-format off
    asm volatile("cpsie i" ::: "memory");
    // clang-format on

    // Go to sleep if the event flag is not set and clear the event flag.
    __wfe();

    // Disable interrupts again.
    // clang-format off
    asm volatile("cpsid i" ::: "memory");
    // clang-format on

    spin_lock_unsafe_blocking(lock);
}

void cond_var_wake(struct cond_var* cond_var)
{
    // If anybody is waiting, wake them up.
    //
    // We do not need to wake up this core, since the only way this could be
    // called while sleeping is in an interrupt handler, and returning from an
    // interrupt sets the event flag or wakes up from WFE.
    if (cond_var->waiters && ~get_core_num()) {
        // Sets the event flag on the other core or wake it up from WFE.
        __sev();
    }
}
