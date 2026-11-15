// SPDX-License-Identifier: GPL-2.0-or-later
/* Reverse selection changes only the selected equipped index, on the main-thread tick. */
#define _GNU_SOURCE
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include "../src/runtime_quick_items.c"

static int menu_open;
int runtime_pad_menu_active(void) { return menu_open; }
int runtime_read_guest(uintptr_t address,void *out,size_t size) {
#ifdef __APPLE__
    mach_vm_size_t got=0;
    return mach_vm_read_overwrite(mach_task_self(),address,size,(mach_vm_address_t)out,&got)==KERN_SUCCESS && got==size;
#else
    struct iovec local={out,size},remote={(void *)address,size};
    return process_vm_readv(getpid(),&local,1,&remote,1,0)==(ssize_t)size;
#endif
}
static void set_index(unsigned char *record,int slot,int32_t value) { memcpy(record+0x3ec+8*slot,&value,4); }
static int32_t selected(unsigned char *record) { int32_t value; memcpy(&value,record+0x418,4); return value; }
static void request(void) { runtime_quick_items_input(0,1); runtime_quick_items_input(1,1); }

int main(void) {
    const int32_t gaps[]={10,-1,30,-1,50,-1},none[]={-1,-1,-1,-1,-1,-1},one[]={-1,-1,30,-1,-1,-1};
    assert(previous_index(gaps,30)==10 && previous_index(gaps,10)==50 && previous_index(gaps,50)==30);
    assert(previous_index(gaps,-1)==50 && previous_index(gaps,999)==50);
    assert(previous_index(none,10)==-1 && previous_index(one,30)==30);

    const size_t size=0x5590000;
    unsigned char *image=mmap(NULL,size,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    assert(image!=MAP_FAILED);
    unsigned char record[0x500]={0},world[0x70]={0},flow[0xb12]={0},block[0x2e8]={0};
    uintptr_t manager[2]={0,(uintptr_t)record},player=1;
    memcpy(image+GAME_DATA,&(uintptr_t){(uintptr_t)manager},sizeof(uintptr_t));
    memcpy(image+WORLD_CHR,&(uintptr_t){(uintptr_t)world},sizeof(uintptr_t));
    memcpy(world+0x60,&player,sizeof player);
    memcpy(image+MENU_FLOW,&(uintptr_t){(uintptr_t)flow},sizeof(uintptr_t));
    memcpy(image+INPUT_BLOCK,&(uintptr_t){(uintptr_t)block},sizeof(uintptr_t));
    flow[0xb11]=1;
    runtime_quick_items_set_image(image,size); assert(!runtime_quick_items_ready());
    /* An executable normaliser fixture with the verified entry instructions; it reads the
     * selected index and returns. This exercises dispatch, not the game's own normalisation. */
    const unsigned char code[]={0x8b,0x87,0x48,0x02,0,0,0x83,0xf8,0xff,0xc3};
    memcpy(image+NORMALISE,code,sizeof code);
    runtime_quick_items_set_image(image,WORLD_CHR); assert(!runtime_quick_items_ready());
    runtime_quick_items_set_image(image,size); assert(runtime_quick_items_ready());
    assert(!mprotect(image+(NORMALISE&~0x3fffu),0x4000,PROT_READ|PROT_EXEC));
    for (int i=0;i<QUICK_SLOTS;++i) set_index(record,i,gaps[i]);
    memcpy(record+0x418,&(int32_t){10},4);
    request(); runtime_quick_items_tick(); assert(selected(record)==10); /* hook not installed */
    runtime_quick_items_hooked(); request();
    assert(selected(record)==10); /* a pad sample never mutates game state */
    runtime_quick_items_tick(); assert(selected(record)==50);
    runtime_quick_items_input(1,1); runtime_quick_items_tick(); assert(selected(record)==50); /* held */
    request(); runtime_quick_items_tick(); assert(selected(record)==30);
    request(); runtime_quick_items_input(0,0); runtime_quick_items_tick(); assert(selected(record)==30);
    request(); menu_open=1; runtime_quick_items_tick(); menu_open=0; assert(selected(record)==30);
    flow[0xb11]=0; request(); runtime_quick_items_tick(); assert(selected(record)==30);
    flow[0xb11]=1; flow[0xa78]=1; request(); runtime_quick_items_tick(); assert(selected(record)==30);
    flow[0xa78]=0; block[0x2e4]=1; request(); runtime_quick_items_tick(); assert(selected(record)==30);
    block[0x2e4]=0;
    request(); atomic_store(&previous_request,host_monotonic_ns()-UINT64_C(200000000));
    runtime_quick_items_tick(); assert(selected(record)==30); /* stale input is dropped */
    memset(world+0x60,0,sizeof(uintptr_t)); request(); runtime_quick_items_tick(); assert(selected(record)==30);
    memcpy(world+0x60,&player,sizeof player);
    for (int i=0;i<QUICK_SLOTS;++i) { int32_t v; memcpy(&v,record+0x3ec+8*i,4); assert(v==gaps[i]); }
    munmap(image,size);
    puts("PASS: reverse quick items, wraparound, empty slots, input edges, menus and load guards");
}
