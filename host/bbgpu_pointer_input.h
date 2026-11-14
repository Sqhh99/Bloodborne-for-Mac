// SPDX-License-Identifier: GPL-2.0-or-later
/* Two small packets of the shared control block, each with one writer and a sequence word
 * (odd while it is written; a reader tries again, never waits). No pointers cross the processes.
 *
 * BbGpuPointerInput (host window -> game): the mouse as a pointer in the game's own menus.
 * BbGpuPrompts (game -> host renderer): what each PlayStation input is on the device in use,
 * for the key guide icons the renderer draws in place of the game's. */
#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>

typedef struct {
    float x, y;            /* the game's stage: 1920x1080, centred in the frame shown */
    uint32_t inside;       /* the cursor is free and over the window */
    uint32_t left_held;
    uint32_t click_serial; /* left button presses */
    uint32_t back_serial;  /* right button presses */
    uint32_t up_serial, down_serial; /* wheel steps */
    uint64_t click_ns;     /* CLOCK_MONOTONIC at the last left press */
} BbGpuPointerSample;
typedef struct {
    uint32_t sequence, reserved;
    BbGpuPointerSample value;
} BbGpuPointerInput;

/* The inputs a prompt can show, in this order on both sides. */
enum {
    BBGPU_PROMPT_CROSS, BBGPU_PROMPT_CIRCLE, BBGPU_PROMPT_SQUARE, BBGPU_PROMPT_TRIANGLE,
    BBGPU_PROMPT_L1, BBGPU_PROMPT_L2, BBGPU_PROMPT_R1, BBGPU_PROMPT_R2, BBGPU_PROMPT_L3, BBGPU_PROMPT_R3,
    BBGPU_PROMPT_OPTIONS, BBGPU_PROMPT_TOUCH_LEFT, BBGPU_PROMPT_TOUCH_RIGHT,
    BBGPU_PROMPT_UP, BBGPU_PROMPT_DOWN, BBGPU_PROMPT_LEFT, BBGPU_PROMPT_RIGHT,
    BBGPU_PROMPT_MOVE_UP, BBGPU_PROMPT_MOVE_LEFT, BBGPU_PROMPT_MOVE_DOWN, BBGPU_PROMPT_MOVE_RIGHT,
    BBGPU_PROMPT_LOOK_UP, BBGPU_PROMPT_LOOK_LEFT, BBGPU_PROMPT_LOOK_DOWN, BBGPU_PROMPT_LOOK_RIGHT,
    BBGPU_PROMPT_INPUTS
};
#define BBGPU_PROMPT_LABEL 24
/* A prompt's kind: 0 none (the game's own icon stays), 'K' a key, 'M' a mouse button (Left Right
 * Middle X1 X2, Wheel Up, Wheel Down), 'F' a face button (A B X Y), 'S' a shoulder or trigger,
 * 'C' another round button or a stick. Variant 0 prefers a key (key clusters), 1 a mouse button
 * while the mouse has the camera (single icons). */
typedef struct {
    uint32_t sequence;
    uint32_t generation; /* 0: nothing published yet; changes with the device and the bindings */
    int32_t device;      /* 0 a PlayStation pad, 1 another gamepad, 2 the keyboard and mouse */
    uint32_t mouse_look;
    uint8_t kind[2][BBGPU_PROMPT_INPUTS];
    char label[2][BBGPU_PROMPT_INPUTS][BBGPU_PROMPT_LABEL]; /* UTF-8 */
} BbGpuPrompts;

static inline void bbgpu_pointer_publish(BbGpuPointerInput *input, const BbGpuPointerSample *value) {
    __atomic_add_fetch(&input->sequence, 1, __ATOMIC_ACQ_REL);
    memcpy(&input->value, value, sizeof *value);
    __atomic_add_fetch(&input->sequence, 1, __ATOMIC_RELEASE);
}
static inline int bbgpu_pointer_read(const BbGpuPointerInput *input, BbGpuPointerSample *value) {
    for (int attempt = 0; attempt < 4; ++attempt) {
        const uint32_t before = __atomic_load_n(&input->sequence, __ATOMIC_ACQUIRE);
        if (before & 1) continue;
        memcpy(value, &input->value, sizeof *value);
        __atomic_thread_fence(__ATOMIC_ACQUIRE);
        if (before == __atomic_load_n(&input->sequence, __ATOMIC_RELAXED)) return 1;
    }
    return 0;
}
/* `value`: a whole BbGpuPrompts; its sequence is ignored. */
static inline void bbgpu_prompts_publish(BbGpuPrompts *prompts, const BbGpuPrompts *value) {
    __atomic_add_fetch(&prompts->sequence, 1, __ATOMIC_ACQ_REL);
    memcpy(&prompts->generation, &value->generation, sizeof *value - offsetof(BbGpuPrompts, generation));
    __atomic_add_fetch(&prompts->sequence, 1, __ATOMIC_RELEASE);
}
static inline int bbgpu_prompts_read(const BbGpuPrompts *prompts, BbGpuPrompts *value) {
    for (int attempt = 0; attempt < 4; ++attempt) {
        const uint32_t before = __atomic_load_n(&prompts->sequence, __ATOMIC_ACQUIRE);
        if (before & 1) continue;
        memcpy(value, prompts, sizeof *value);
        __atomic_thread_fence(__ATOMIC_ACQUIRE);
        if (before == __atomic_load_n(&prompts->sequence, __ATOMIC_RELAXED)) return 1;
    }
    return 0;
}
