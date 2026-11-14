// SPDX-License-Identifier: GPL-2.0-or-later
/* Run the production displacement reader and the actual x86 detour. No game or window needed. */
#include <assert.h>
#include <unistd.h>
#include "../src/runtime_mouse.c"

static uint64_t test_now=UINT64_C(1000000000);
static BbGpuShared shared;
static int menu;
uint64_t host_monotonic_ns(void) { return test_now; }
int runtime_pad_menu_active(void) { return menu; }
static void *shared_input(void) { return &shared; }

#if defined(__x86_64__) && defined(__APPLE__)
typedef struct {
    float turn[8],sentinel[8],out_turn[8],out_sentinel[8];
    uint64_t rax,rcx,r10,flags;
    uint32_t frame_flag;
} Probe;
static const float test_one __attribute__((used))=1.0f;
extern void run_mouse_detour(void *site,Probe *probe,void *camera);
__asm__(
    ".text\n.p2align 4\n_run_mouse_detour:\n"
    "pushq %rbp\nmovq %rsp,%rbp\npushq %rbx\npushq %r13\nsubq $1024,%rsp\n"
    "movq %rsi,%rbx\nmovq %rdx,%r13\n"
    "vmovups 0(%rbx),%ymm1\nvmovups 32(%rbx),%ymm3\n"
    "vbroadcastss _test_one(%rip),%xmm10\nvmovaps %xmm10,-0x3f0(%rbp)\n"
    "movl $0,-0x3c4(%rbp)\n"
    "movabsq $0x1122334455667788,%rax\nmovabsq $0x2233445566778899,%rcx\nmovabsq $0x33445566778899aa,%r10\n"
    "stc\ncallq *%rdi\n"
    "movq %rax,128(%rbx)\nmovq %rcx,136(%rbx)\nmovq %r10,144(%rbx)\n"
    "pushfq\npopq %rax\nmovq %rax,152(%rbx)\n"
    "vmovups %ymm1,64(%rbx)\nvmovups %ymm3,96(%rbx)\n"
    "movl -0x3c4(%rbp),%eax\nmovl %eax,160(%rbx)\n"
    "vzeroupper\naddq $1024,%rsp\npopq %r13\npopq %rbx\npopq %rbp\nretq\n"
);

static void publish(BbGpuMouseSample *input,double x,double y) {
    input->x=(int64_t)llround(x*BBGPU_MOUSE_POINT_SCALE);
    input->y=(int64_t)llround(y*BBGPU_MOUSE_POINT_SCALE);
    input->motion_ns=test_now;
    bbgpu_mouse_publish(&shared.mouse_input,input);
}
static void test_displacement(void) {
    unsigned char frame[0x500]={0},camera[0x200]={0};
    const uintptr_t rbp=(uintptr_t)(frame+0x450),cam=(uintptr_t)camera;
    BbGpuMouseSample input={.epoch=1,.captured=1};
    float turn[2]={0};
    publish(&input,100,50);
    bb_mouse_look_apply(turn,rbp,cam);
    assert(fabsf(turn[0]-0.075f)<1e-6f && fabsf(turn[1]-0.15f)<1e-6f);
    assert(camera[0x15c]==1 && *(uint32_t *)(frame+0x450-0x3c4)==1);
    turn[0]=turn[1]=0;
    bb_mouse_look_apply(turn,rbp,cam);
    assert(turn[0]==0 && turn[1]==0); /* polling again cannot repeat a swipe */
    float slow=0;
    for (int i=1;i<=10;++i) {
        test_now+=1000000;
        publish(&input,100+i*10,50);
        turn[0]=turn[1]=0; bb_mouse_look_apply(turn,rbp,cam); slow+=turn[1];
    }
    assert(fabsf(slow-0.15f)<1e-6f); /* same distance fast / slow: same turn */
    menu=1; publish(&input,300,50);
    turn[0]=turn[1]=0; bb_mouse_look_apply(turn,rbp,cam);
    assert(!turn[0] && !turn[1]);
    menu=0; bb_mouse_look_apply(turn,rbp,cam);
    assert(!turn[1]); /* closing the menu cannot release a stored turn */
    shared.overlay_captures=1; publish(&input,400,50); bb_mouse_look_apply(turn,rbp,cam);
    shared.overlay_captures=0; bb_mouse_look_apply(turn,rbp,cam); assert(!turn[1]);
    input.epoch=2; publish(&input,25,0);
    bb_mouse_look_apply(turn,rbp,cam); assert(fabsf(turn[1]-0.0375f)<1e-6f);
    turn[0]=turn[1]=0;
    input.captured=0; publish(&input,100,0); bb_mouse_look_apply(turn,rbp,cam);
    assert(!turn[1]);
    input.captured=1; input.epoch=3; publish(&input,20,0);
    input.motion_ns=test_now-MOUSE_LOOK_STALE_NS-1; bbgpu_mouse_publish(&shared.mouse_input,&input);
    bb_mouse_look_apply(turn,rbp,cam); assert(!turn[1]);
    publish(&input,40,0); test_now+=MOUSE_LOOK_STALE_NS+1;
    bb_mouse_look_apply(turn,rbp,cam); assert(!turn[1]);
    publish(&input,50,0); bb_mouse_look_apply(turn,rbp,cam);
    assert(fabsf(turn[1]-0.015f)<1e-6f);
    BbGpuMouseSample read;
    shared.mouse_input.sequence|=1;
    assert(!bbgpu_mouse_read(&shared.mouse_input,&read));
    ++shared.mouse_input.sequence;
    assert(bbgpu_mouse_read(&shared.mouse_input,&read));
}
static void test_detour(void) {
    const size_t size=(MOUSE_LOOK_OFFSET+sizeof mouse_look_code+4096)&~(size_t)4095;
    unsigned char *image=mmap(NULL,size,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    assert(image!=MAP_FAILED);
    memcpy(image+MOUSE_LOOK_OFFSET,mouse_look_code,sizeof mouse_look_code);
    image[MOUSE_LOOK_OFFSET+sizeof mouse_look_code]=0xc3;
    assert(!runtime_install_mouse_look(image,MOUSE_LOOK_OFFSET));
    for (unsigned i=0;i<sizeof mouse_look_code;++i) {
        image[MOUSE_LOOK_OFFSET+i]^=1;
        assert(!runtime_install_mouse_look(image,size));
        image[MOUSE_LOOK_OFFSET+i]^=1;
    }
    setenv("BB_NATIVE_MOUSE","0",1);
    assert(!runtime_install_mouse_look(image,size));
    assert(!memcmp(image+MOUSE_LOOK_OFFSET,mouse_look_code,sizeof mouse_look_code));
    unsetenv("BB_NATIVE_MOUSE");
    assert(!runtime_mouse_patch_conflict(MOUSE_LOOK_OFFSET,mouse_look_code,sizeof mouse_look_code));
    const unsigned char changed=0x90;
    assert(runtime_mouse_patch_conflict(MOUSE_LOOK_OFFSET,&changed,1));
    assert(runtime_install_mouse_look(image,size));
    assert(!mprotect(image,size,PROT_READ|PROT_EXEC));
    unsigned char camera[0x200]={0};
    BbGpuMouseSample input={.epoch=10,.captured=1};
    mouse_primed=0; atomic_store(&mouse_update_ns,0);
    publish(&input,100,50);
    Probe probe={.turn={0.25f,0.5f,0.75f,1.0f},.sentinel={11,12,13,14,15,16,17,18}};
    run_mouse_detour(image+MOUSE_LOOK_OFFSET,&probe,camera);
    assert(fabsf(probe.out_turn[0]-0.325f)<1e-6f && fabsf(probe.out_turn[1]-0.65f)<1e-6f);
    assert(probe.out_turn[2]==0.75f && probe.out_turn[3]==1);
    assert(!memcmp(probe.sentinel,probe.out_sentinel,sizeof probe.sentinel)); /* full YMM, upper lanes too */
    assert(probe.rax==UINT64_C(0x1122334455667788) && probe.rcx==UINT64_C(0x2233445566778899));
    assert(probe.r10==UINT64_C(0x33445566778899aa) && (probe.flags&1));
    assert(probe.frame_flag==1 && camera[0x15c]==1);
    assert(runtime_mouse_direct_active());
    test_now+=MOUSE_LOOK_STALE_NS+1; assert(!runtime_mouse_direct_active());
    munmap(image,size);
}
#endif

int main(void) {
    runtime_mouse_set_shared(shared_input);
#if defined(__x86_64__) && defined(__APPLE__)
    test_displacement(); test_detour();
#endif
    puts("PASS: native mouse distance, no replay, menu/focus gating, hook guards and x86 register preservation");
}
