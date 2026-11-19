// SPDX-License-Identifier: GPL-2.0-or-later
/* One window-thread writer, pad readers. Keyboard and mouse buttons belong to the same SDL
 * event pump: a reader must not combine a fresh click with an older W/Shift state. */
#pragma once
#include <stdint.h>

#define BBGPU_INPUT_KEYS 512
typedef struct {
    uint8_t keys[BBGPU_INPUT_KEYS]; /* SDL scancodes, 0 or 1 */
    uint32_t mouse_buttons, mouse_captured;
} BbGpuButtonSample;
typedef struct {
    uint32_t sequence, reserved; /* 0: no sample yet; odd: the writer is updating */
    BbGpuButtonSample value;
} BbGpuButtonInput;

static inline void bbgpu_buttons_publish(BbGpuButtonInput *input,const BbGpuButtonSample *value) {
    __atomic_add_fetch(&input->sequence,1,__ATOMIC_ACQ_REL);
    for (unsigned i=0;i<BBGPU_INPUT_KEYS;++i)
        __atomic_store_n(&input->value.keys[i],value->keys[i],__ATOMIC_RELAXED);
    __atomic_store_n(&input->value.mouse_buttons,value->mouse_buttons,__ATOMIC_RELAXED);
    __atomic_store_n(&input->value.mouse_captured,value->mouse_captured,__ATOMIC_RELAXED);
    __atomic_add_fetch(&input->sequence,1,__ATOMIC_RELEASE);
}

/* Bounded and non-consuming: a busy writer leaves the caller's last complete sample intact.
 * In particular, never insert a neutral stick between two reads of a held movement key. */
static inline int bbgpu_buttons_read(const BbGpuButtonInput *input,BbGpuButtonSample *value) {
    for (int attempt=0;attempt<4;++attempt) {
        const uint32_t before=__atomic_load_n(&input->sequence,__ATOMIC_ACQUIRE);
        if (!before || (before&1)) continue;
        BbGpuButtonSample next;
        for (unsigned i=0;i<BBGPU_INPUT_KEYS;++i)
            next.keys[i]=__atomic_load_n(&input->value.keys[i],__ATOMIC_RELAXED);
        next.mouse_buttons=__atomic_load_n(&input->value.mouse_buttons,__ATOMIC_RELAXED);
        next.mouse_captured=__atomic_load_n(&input->value.mouse_captured,__ATOMIC_RELAXED);
        __atomic_thread_fence(__ATOMIC_ACQUIRE);
        if (before==__atomic_load_n(&input->sequence,__ATOMIC_RELAXED)) { *value=next; return 1; }
    }
    return 0;
}
