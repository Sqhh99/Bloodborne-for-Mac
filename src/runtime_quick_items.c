// SPDX-License-Identifier: GPL-2.0-or-later
/* Bloodborne 1.09's quick-item selection. Layout independently checked against the native
 * EquipGameData normaliser (0x14cf920); also documented by visy's party/game_state.cpp:
 * GameDataMan +8 -> PlayerGameData; +0x1d0 EquipGameData, six {handle,index} quick slots at
 * EquipGameData+0x218, selected inventory index +0x248. Only selection changes, never inventory
 * contents or the order of equipped items. Requests expire rather than surviving a menu/load. */
#define _GNU_SOURCE
#include "runtime.h"
#include <stdatomic.h>
#include <string.h>
#ifdef __APPLE__
#include <mach/mach.h>
#include <mach/mach_vm.h>
#elif defined(__linux__)
#include <sys/uio.h>
#include <unistd.h>
#endif

enum { QUICK_SLOTS=6, GAME_DATA=0x553b130, WORLD_CHR=0x553e878, NORMALISE=0x14cf920,
       MENU_FLOW=0x5580e38, INPUT_BLOCK=0x5562878 };
static unsigned char *quick_image;
static _Atomic int quick_hooked;
static _Atomic uint64_t previous_request;
static int previous_held; /* pad sampling holds runtime_pad.c's lock */

static int previous_index(const int32_t slots[QUICK_SLOTS],int32_t selected) {
    int current=0;
    for (int i=0;i<QUICK_SLOTS;++i) if (slots[i]==selected && selected>=0) { current=i; break; }
    for (int step=1;step<=QUICK_SLOTS;++step) {
        const int index=slots[(current+QUICK_SLOTS-step)%QUICK_SLOTS];
        if (index>=0) return index;
    }
    return -1;
}

void runtime_quick_items_set_image(unsigned char *image,uint64_t size) {
    static const unsigned char signature[]={0x8b,0x87,0x48,0x02,0,0,0x83,0xf8,0xff};
    quick_image=NULL;
    atomic_store(&quick_hooked,0); atomic_store(&previous_request,0); previous_held=0;
    if (!image || size<MENU_FLOW+sizeof(uintptr_t) ||
        memcmp(image+NORMALISE,signature,sizeof signature)) return;
    quick_image=image;
}
int runtime_quick_items_ready(void) { return quick_image!=NULL; }
void runtime_quick_items_hooked(void) { atomic_store(&quick_hooked,1); }

void runtime_quick_items_input(int held,int allowed) {
    if (!allowed || !atomic_load(&quick_hooked)) {
        previous_held=0; atomic_store(&previous_request,0); return;
    }
    if (held && !previous_held) atomic_store(&previous_request,host_monotonic_ns());
    previous_held=held;
}

static uintptr_t quick_pointer(uintptr_t address) {
    uintptr_t value=0;
    return runtime_read_guest(address,&value,sizeof value) ? value : 0;
}
static int write_selection(uintptr_t address,int32_t index) {
#ifdef __APPLE__
    return mach_vm_write(mach_task_self(),address,(vm_offset_t)&index,sizeof index)==KERN_SUCCESS;
#elif defined(__linux__)
    struct iovec local={&index,sizeof index},remote={(void *)address,sizeof index};
    return process_vm_writev(getpid(),&local,1,&remote,1,0)==(ssize_t)sizeof index;
#else
    (void)address; (void)index; return 0;
#endif
}

void runtime_quick_items_tick(void) {
    const uint64_t event=atomic_exchange(&previous_request,0),now=host_monotonic_ns();
    if (!event || !quick_image || !atomic_load(&quick_hooked) || now<event || now-event>UINT64_C(150000000) ||
        runtime_pad_menu_active()) return;
    const uintptr_t flow=quick_pointer((uintptr_t)quick_image+MENU_FLOW);
    const uintptr_t block=quick_pointer((uintptr_t)quick_image+INPUT_BLOCK);
    uint64_t blocked; uint32_t blocked_also; uint8_t in_game;
    if (!flow || !runtime_read_guest(flow+0xb11,&in_game,1) || !in_game ||
        !runtime_read_guest(flow+0xa78,&blocked,sizeof blocked) || blocked ||
        (block && (!runtime_read_guest(block+0x2e4,&blocked_also,sizeof blocked_also) || blocked_also))) return;
    const uintptr_t manager=quick_pointer((uintptr_t)quick_image+GAME_DATA);
    const uintptr_t record=manager ? quick_pointer(manager+8) : 0;
    const uintptr_t world=quick_pointer((uintptr_t)quick_image+WORLD_CHR);
    if (!record || !world || !quick_pointer(world+0x60)) return; /* no local player during loading */
    int32_t selected,slots[QUICK_SLOTS];
    if (!runtime_read_guest(record+0x418,&selected,sizeof selected)) return;
    for (int i=0;i<QUICK_SLOTS;++i) {
        if (!runtime_read_guest(record+0x3ec+8u*i,&slots[i],sizeof slots[i])) return;
    }
    const int32_t previous=previous_index(slots,selected);
    if (previous<0 || previous==selected || !write_selection(record+0x418,previous)) return;
    /* Keep the original normaliser in charge of validating the selected equipped index. */
    ((void (ABI *)(uintptr_t))(quick_image+NORMALISE))(record+0x1d0);
}
