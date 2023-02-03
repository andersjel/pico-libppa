#pragma once

#include <hardware/sync.h>
#include <stdint.h>

struct cond_var {
    uint8_t waiters;
};

void cond_var_wait(spin_lock_t* lock, struct cond_var* cond_var);
void cond_var_wake(struct cond_var* cond_var);
