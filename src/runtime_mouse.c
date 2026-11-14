// SPDX-License-Identifier: GPL-2.0-or-later
/* Bloodborne 1.09 free-camera mouse input. The site and input-activity flags are documented by
 * bbport 0.6 and bloodborne-mac's bbport_game_hooks.cpp. Inject displacement after the game's
 * stick-speed calculation, before its angle wrapping / pitch clamp. Gamepad input is kept.
 * Unlike stick emulation, equal mouse distances always give equal turns, at any swipe speed. */
#define _GNU_SOURCE
#include "runtime.h"
#include <math.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#ifdef __APPLE__
#include "host/bbgpu_shared.h"
#endif

#define MOUSE_LOOK_OFFSET 0x143cde0u
#define MOUSE_LOOK_STALE_NS UINT64_C(100000000)
#define MOUSE_RADIANS_PER_POINT 0.0015
static const unsigned char mouse_look_code[]={
    0xc5,0xa8,0x59,0xc9, 0xc5,0xf8,0x28,0x95,0x10,0xfc,0xff,0xff, 0xc5,0xe8,0x59,0xc9
};
static void *(*mouse_shared)(void);
static _Atomic int mouse_installed;
static _Atomic uint64_t mouse_update_ns;
void runtime_mouse_set_shared(void *(*shared)(void)) { mouse_shared=shared; }

const char *runtime_mouse_patch_conflict(uint64_t offset,const unsigned char *data,size_t size) {
    const char *off=getenv("BB_NATIVE_MOUSE");
    if ((off && off[0]=='0') || size>UINT64_MAX-offset || offset>=MOUSE_LOOK_OFFSET+sizeof mouse_look_code ||
        offset+size<=MOUSE_LOOK_OFFSET) return NULL;
    const uint64_t start=offset>MOUSE_LOOK_OFFSET ? offset : MOUSE_LOOK_OFFSET;
    const uint64_t end=offset+size<MOUSE_LOOK_OFFSET+sizeof mouse_look_code ? offset+size : MOUSE_LOOK_OFFSET+sizeof mouse_look_code;
    return memcmp(data+(start-offset),mouse_look_code+(start-MOUSE_LOOK_OFFSET),(size_t)(end-start)) ? "native mouse camera" : NULL;
}

int runtime_mouse_direct_active(void) {
#ifdef __APPLE__
    const BbGpuShared *s=mouse_shared ? mouse_shared() : NULL;
    const uint64_t last=atomic_load_explicit(&mouse_update_ns,memory_order_relaxed),now=host_monotonic_ns();
    return atomic_load_explicit(&mouse_installed,memory_order_acquire) && s && last &&
        now>=last && now-last<MOUSE_LOOK_STALE_NS;
#else
    return 0;
#endif
}

#if defined(__x86_64__) && defined(__APPLE__)
static BbGpuMouseSample mouse_previous;
static int mouse_primed;
void *bb_mouse_look_return;

/* Called only by the normal free-camera update; lock-on / cinematics do not reach this site.
 * Consume and discard blocked/stale input too, so reopening a menu or changing camera mode
 * never releases a delayed turn. The host is the only writer of the seqlock packet. */
void bb_mouse_look_apply(float turn[2],uintptr_t frame,uintptr_t camera) {
    const BbGpuShared *s=mouse_shared ? mouse_shared() : NULL;
    BbGpuMouseSample input;
    if (!s || !bbgpu_mouse_read(&s->mouse_input,&input)) return;
    const uint64_t now=host_monotonic_ns();
    const uint64_t last=atomic_exchange_explicit(&mouse_update_ns,now,memory_order_relaxed);
    const int epoch_changed=!mouse_primed || input.epoch!=mouse_previous.epoch;
    const double dx=(double)input.x-(epoch_changed ? 0.0 : (double)mouse_previous.x);
    const double dy=(double)input.y-(epoch_changed ? 0.0 : (double)mouse_previous.y);
    mouse_previous=input; mouse_primed=1;
    if (!input.captured || __atomic_load_n(&s->overlay_captures,__ATOMIC_ACQUIRE) || runtime_pad_menu_active() ||
        !input.motion_ns || now<input.motion_ns || now-input.motion_ns>MOUSE_LOOK_STALE_NS ||
        (last && now-last>MOUSE_LOOK_STALE_NS) || (dx==0.0 && dy==0.0)) return;
    const float pitch=(float)(dy/BBGPU_MOUSE_POINT_SCALE*MOUSE_RADIANS_PER_POINT);
    const float yaw=(float)(dx/BBGPU_MOUSE_POINT_SCALE*MOUSE_RADIANS_PER_POINT);
    if (!isfinite(pitch) || !isfinite(yaw)) return;
    turn[0]+=pitch; turn[1]+=yaw;
    /* Mark real camera input, preventing the game's automatic recenter from fighting it. */
    const uint32_t one=1,zero=0;
    memcpy((void *)(frame-0x3c4),&one,4);
    *(unsigned char *)(camera+0x15c)=1;
    memcpy((void *)(camera+0x154),&zero,4);
}

/* A mid-function detour: preserve flags, the red zone, volatile GPRs and all 16 full YMM
 * registers. The displaced instructions run first, so only xmm1's pitch/yaw are changed. */
__asm__(
    ".text\n.p2align 4\n_bb_mouse_look_trampoline:\n"
    "leaq -128(%rsp),%rsp\n"
    "pushfq\npushq %rax\npushq %rcx\npushq %rdx\npushq %rsi\npushq %rdi\n"
    "pushq %r8\npushq %r9\npushq %r10\npushq %r11\npushq %rbp\n"
    "movq %rsp,%r11\nandq $-32,%rsp\nsubq $544,%rsp\nmovq %r11,512(%rsp)\n"
    "vmovdqu %ymm0,0(%rsp)\nvmovdqu %ymm1,32(%rsp)\nvmovdqu %ymm2,64(%rsp)\nvmovdqu %ymm3,96(%rsp)\n"
    "vmovdqu %ymm4,128(%rsp)\nvmovdqu %ymm5,160(%rsp)\nvmovdqu %ymm6,192(%rsp)\nvmovdqu %ymm7,224(%rsp)\n"
    "vmovdqu %ymm8,256(%rsp)\nvmovdqu %ymm9,288(%rsp)\nvmovdqu %ymm10,320(%rsp)\nvmovdqu %ymm11,352(%rsp)\n"
    "vmovdqu %ymm12,384(%rsp)\nvmovdqu %ymm13,416(%rsp)\nvmovdqu %ymm14,448(%rsp)\nvmovdqu %ymm15,480(%rsp)\n"
    "leaq 32(%rsp),%rdi\nmovq (%r11),%rsi\nmovq %r13,%rdx\ncallq _bb_mouse_look_apply\n"
    "vmovdqu 0(%rsp),%ymm0\nvmovdqu 32(%rsp),%ymm1\nvmovdqu 64(%rsp),%ymm2\nvmovdqu 96(%rsp),%ymm3\n"
    "vmovdqu 128(%rsp),%ymm4\nvmovdqu 160(%rsp),%ymm5\nvmovdqu 192(%rsp),%ymm6\nvmovdqu 224(%rsp),%ymm7\n"
    "vmovdqu 256(%rsp),%ymm8\nvmovdqu 288(%rsp),%ymm9\nvmovdqu 320(%rsp),%ymm10\nvmovdqu 352(%rsp),%ymm11\n"
    "vmovdqu 384(%rsp),%ymm12\nvmovdqu 416(%rsp),%ymm13\nvmovdqu 448(%rsp),%ymm14\nvmovdqu 480(%rsp),%ymm15\n"
    "movq 512(%rsp),%rsp\npopq %rbp\npopq %r11\npopq %r10\npopq %r9\npopq %r8\n"
    "popq %rdi\npopq %rsi\npopq %rdx\npopq %rcx\npopq %rax\npopfq\n"
    "leaq 128(%rsp),%rsp\njmpq *_bb_mouse_look_return(%rip)\n"
);
extern char bb_mouse_look_trampoline[];
static void mouse_jump(unsigned char *at,uintptr_t destination) {
    const unsigned char op[]={0xff,0x25,0,0,0,0};
    memcpy(at,op,sizeof op); memcpy(at+sizeof op,&destination,8);
}
int runtime_install_mouse_look(unsigned char *image,uint64_t size) {
    const char *off=getenv("BB_NATIVE_MOUSE");
    if (off && off[0]=='0') { puts("Mouse: native camera disabled; stick fallback"); return 0; }
    if (!image || size<MOUSE_LOOK_OFFSET+sizeof mouse_look_code ||
        memcmp(image+MOUSE_LOOK_OFFSET,mouse_look_code,sizeof mouse_look_code)) {
        puts("Mouse: unsupported camera code; stick fallback"); return 0;
    }
    unsigned char *stub=mmap(NULL,4096,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    if (stub==MAP_FAILED) return 0;
    memcpy(stub,mouse_look_code,sizeof mouse_look_code);
    mouse_jump(stub+sizeof mouse_look_code,(uintptr_t)bb_mouse_look_trampoline);
    if (mprotect(stub,4096,PROT_READ|PROT_EXEC)) { munmap(stub,4096); return 0; }
    bb_mouse_look_return=image+MOUSE_LOOK_OFFSET+sizeof mouse_look_code;
    mouse_jump(image+MOUSE_LOOK_OFFSET,(uintptr_t)stub);
    memset(image+MOUSE_LOOK_OFFSET+14,0x90,sizeof mouse_look_code-14);
    atomic_store_explicit(&mouse_installed,1,memory_order_release);
    puts("Mouse: native displacement camera enabled (no stick acceleration or turn-speed cap)");
    return 1;
}
#else
int runtime_install_mouse_look(unsigned char *image,uint64_t size) { (void)image; (void)size; return 0; }
#endif
