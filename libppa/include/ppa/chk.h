#pragma once

// Evaluates to expr, but goes to fail_label if the result is equal to -1.
#define CHK_NEG1(expr, fail_label)                                                                 \
    ({                                                                                             \
        __auto_type _result = (expr);                                                              \
        if (_result == -1)                                                                         \
            goto fail_label;                                                                       \
        _result;                                                                                   \
    })
