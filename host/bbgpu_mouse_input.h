// SPDX-License-Identifier: GPL-2.0-or-later
/* One window-thread writer, camera/pad readers. Cumulative fixed-point motion preserves a
 * fast swipe even when it arrives between game frames; readers never consume the same delta
 * twice. The epoch separates cursor captures. No pointers cross the process boundary. */
#pragma once
#include <stdint.h>

#define BBGPU_MOUSE_POINT_SCALE 65536.0
typedef struct {
    uint32_t epoch, captured;
    int64_t x, y;
    uint64_t motion_ns;
    uint32_t wheel_serial;
    int32_t wheel_direction; /* +1 up, -1 down */
    uint64_t wheel_ns;
} BbGpuMouseSample;
typedef struct {
    uint32_t sequence, reserved;
    BbGpuMouseSample value;
} BbGpuMouseInput;

static inline void bbgpu_mouse_publish(BbGpuMouseInput *input,const BbGpuMouseSample *value) {
    __atomic_add_fetch(&input->sequence,1,__ATOMIC_ACQ_REL);
#define BB_MOUSE_STORE(field) __atomic_store_n(&input->value.field,value->field,__ATOMIC_RELAXED)
    BB_MOUSE_STORE(epoch); BB_MOUSE_STORE(captured); BB_MOUSE_STORE(x); BB_MOUSE_STORE(y);
    BB_MOUSE_STORE(motion_ns); BB_MOUSE_STORE(wheel_serial); BB_MOUSE_STORE(wheel_direction); BB_MOUSE_STORE(wheel_ns);
#undef BB_MOUSE_STORE
    __atomic_add_fetch(&input->sequence,1,__ATOMIC_RELEASE);
}
static inline int bbgpu_mouse_read(const BbGpuMouseInput *input,BbGpuMouseSample *value) {
    for (int attempt=0;attempt<4;++attempt) {
        const uint32_t before=__atomic_load_n(&input->sequence,__ATOMIC_ACQUIRE);
        if (before&1) continue;
#define BB_MOUSE_LOAD(field) value->field=__atomic_load_n(&input->value.field,__ATOMIC_RELAXED)
        BB_MOUSE_LOAD(epoch); BB_MOUSE_LOAD(captured); BB_MOUSE_LOAD(x); BB_MOUSE_LOAD(y);
        BB_MOUSE_LOAD(motion_ns); BB_MOUSE_LOAD(wheel_serial); BB_MOUSE_LOAD(wheel_direction); BB_MOUSE_LOAD(wheel_ns);
#undef BB_MOUSE_LOAD
        __atomic_thread_fence(__ATOMIC_ACQUIRE);
        if (before==__atomic_load_n(&input->sequence,__ATOMIC_RELAXED)) return 1;
    }
    return 0; /* try next frame, never wait on the input thread */
}
