// SPDX-License-Identifier: GPL-2.0-or-later
/* Bloodborne 1.09: the mouse as a pointer in the game's own menus (Scaleform lists: the title
 * menu, the inventory, the shops, the options). The item under the pointer gets the game's
 * cursor, a click presses the confirm button on it (or the slider's arrow it is on), the right
 * button goes back, the wheel moves up and down.
 *
 * Ported from bloodborne-mac's gpu/games/bloodborne_menu_pointer.cpp (itself from bbport 0.6,
 * deadinside28/bloodborne_pc 8185b88). There the window and the game share a process; here the
 * window is in bbgpu-host, which publishes the pointer in the shared block
 * (host/bbgpu_pointer_input.h) and lets the cursor go while `menu_pointer` is set. The game's
 * functions are called directly: this code and the game are the same x86-64 System V code.
 *
 * Reverse engineering (offsets in the 1.09 executable; published with bbport 0.6, the bytes
 * checked here before anything is used):
 *  - 0x1acff30 updates one list component each frame it is shown (rdi the component, rsi its
 *    input: the first byte is nonzero while the list has the input focus). runtime_hooks.c
 *    hooks its entry (an observer: the function then runs as it was).
 *  - The component: +0x30 its listener (vtable +0x10: cursor moved, with a pointer to the index),
 *    +0x70 item count, +0x74 cursor, +0x78 cursor override (>= 0 wins), +0x7c items per line,
 *    +0x80 lines, +0x84 scroll within a line, +0x24c line scroll, +0x258 the item provider
 *    (vtable +0x18 makes the Scaleform value of item (cell | line << 32)), +0x2b0 wraps around,
 *    +0xc8/+0xd0 the type and data of its root value.
 *  - 0x1acf6b0 sets the cursor (component, index; 1 when it scrolled), 0x1ad0dd0 scrolls by
 *    (lines, cells), 0x1acf0e0 hides every item's cursor.
 *  - Scaleform values are 0x80 bytes: +0x30 the value, +0x40 the object interface (vtable +0x18
 *    releases), +0x48 the type (& 0x40: managed; & 0x8f == 10: a display object), +0x50 its data
 *    (+0x88: the native display object). 0x1adb220 finds a child by path ("Cursor" at 0x4934044,
 *    "Widgets/Slider/LeftArrow", ...); a value's own vtable +8 shows or hides it.
 *  - Display objects: +0x38 parent, +0x6b flags (0x40 visible), vtable +0x1f0 local bounds
 *    (twips, x1 y1 x2 y2); 0x2040 their world matrix (2x4 floats, twips). The stage is 1920x1080
 *    pixels, 20 twips each.
 *  - 0x5580e38 the menu flow (+0xb11 zero: the title menu; +0xa88: a menu is open; +0xa78: input
 *    blocked), 0x5562878 another input block (+0x2e4), 0x5128948 the region (5: Japan, circle
 *    confirms), 0x557fc60 the menu sound request (+8 its label: 0x49344e4 the cursor's move).
 *
 * BB_MENU_POINTER=0: off. BB_MENU_POINTER_FILE=<file> (tests): "x y [click|right|up|down]" in
 * stage pixels, applied when the file changes, as the mouse would. */
#define _GNU_SOURCE
#include "runtime.h"
#include "gpu/bbgpu.h"
#include <math.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "host/bbgpu_pointer_input.h"
#ifdef __APPLE__
#include "host/bbgpu_shared.h"
#include <mach/mach.h>
#include <mach/mach_vm.h>
#elif defined(__linux__)
#include <sys/uio.h>
#include <unistd.h>
#endif

typedef uintptr_t Addr;

#define CODE_END 0x50d96dcu
#define LIST_UPDATE 0x1acff30u
#define SET_CURSOR_FN 0x1acf6b0u
#define SCROLL_BY_FN 0x1ad0dd0u
#define HIDE_CURSORS_FN 0x1acf0e0u
#define FIND_CHILD_FN 0x1adb220u
#define WORLD_MATRIX_FN 0x2040u
#define CURSOR_NAME 0x4934044u
#define MENU_FLOW 0x5580e38u
#define INPUT_BLOCK 0x5562878u
#define REGION 0x5128948u
#define MENU_SOUND 0x557fc60u
#define CURSOR_MOVE_SOUND 0x49344e4u

enum { BTN_UP=0x10, BTN_RIGHT=0x20, BTN_DOWN=0x40, BTN_LEFT=0x80, BTN_CIRCLE=0x2000, BTN_CROSS=0x4000 };
/* List component fields. */
enum {
    F_LISTENER=0x30, F_COUNT=0x70, F_CURSOR=0x74, F_CURSOR_OVERRIDE=0x78, F_PER_LINE=0x7c,
    F_LINES=0x80, F_LINE_SCROLL=0x84, F_ROOT_TYPE=0xc8, F_ROOT_DATA=0xd0, F_SCROLL=0x24c,
    F_PROVIDER=0x258, F_WRAPS=0x2b0, COMPONENT_SIZE=0x2b8,
};

static unsigned char *image;  /* the game's image; NULL: another build, or switched off */
static int flow_ready;        /* the menu flow can be read (menus are told from play) */
static _Atomic int hooked;    /* the list update is hooked: the pointer works */
static void *(*pointer_shared)(void);

#define AT(offset) ((Addr)image+(offset))

int runtime_read_guest(uintptr_t address, void *out, size_t size) {
    if (address<0x10000 || address>UINTPTR_MAX-size) return 0;
#ifdef __APPLE__
    mach_vm_size_t got=0;
    return mach_vm_read_overwrite(mach_task_self(),address,size,(mach_vm_address_t)out,&got)==KERN_SUCCESS && got==size;
#elif defined(__linux__)
    struct iovec local={out,size},remote={(void *)address,size};
    return process_vm_readv(getpid(),&local,1,&remote,1,0)==(ssize_t)size;
#else
    (void)out; return 0;
#endif
}
static Addr get_addr(Addr a) { Addr v=0; return runtime_read_guest(a,&v,sizeof v) ? v : 0; }
static uint64_t get_u64(Addr a) { uint64_t v=0; return runtime_read_guest(a,&v,sizeof v) ? v : 0; }
static uint32_t get_u32(Addr a) { uint32_t v=0; return runtime_read_guest(a,&v,sizeof v) ? v : 0; }
static int get_u8(Addr a, int fallback) { uint8_t v=0; return runtime_read_guest(a,&v,1) ? v : fallback; }
static int32_t field_i32(const uint8_t *object, size_t offset) { int32_t v; memcpy(&v,object+offset,sizeof v); return v; }
static uint32_t field_u32(const uint8_t *object, size_t offset) { uint32_t v; memcpy(&v,object+offset,sizeof v); return v; }
static Addr field_addr(const uint8_t *object, size_t offset) { Addr v; memcpy(&v,object+offset,sizeof v); return v; }

/* Calls a game function (System V integer arguments; unused ones are harmless). */
static uint64_t call(Addr fn, uint64_t a, uint64_t b, uint64_t c, uint64_t d) {
    return ((uint64_t (*)(uint64_t,uint64_t,uint64_t,uint64_t))fn)(a,b,c,d);
}
/* A function of the game (its usual prologue), from a vtable or a field. */
static int is_game_function(Addr address) {
    uint8_t prologue[4]={0};
    return address>=AT(0) && address<AT(CODE_END) && runtime_read_guest(address,prologue,sizeof prologue) &&
        prologue[0]==0x55 && prologue[1]==0x48 && prologue[2]==0x89 && prologue[3]==0xe5;
}
static Addr virtual_function(Addr object, size_t slot_offset) {
    const Addr vtable=get_addr(object);
    const Addr function=vtable ? get_addr(vtable+slot_offset) : 0;
    return is_game_function(function) ? function : 0;
}
static int64_t now_ms(void) { return (int64_t)(host_monotonic_ns()/1000000u); }

/* A Scaleform value the game fills in: value_release when done with it. */
typedef struct { _Alignas(16) uint8_t bytes[0x80]; } Value;
static void value_release(Value *v) {
    if (!(field_u32(v->bytes,0x48)&0x40)) return;
    const Addr object_interface=field_addr(v->bytes,0x40);
    const Addr release=object_interface ? virtual_function(object_interface,0x18) : 0;
    if (release) call(release,object_interface,(Addr)(v->bytes+0x30),field_addr(v->bytes,0x50),0);
    memset(v->bytes,0,sizeof v->bytes);
}
/* The native display object, or 0 (not a display object). */
static Addr value_display_object(const Value *v) {
    if ((field_u32(v->bytes,0x48)&0x8f)!=10) return 0;
    const Addr data=field_addr(v->bytes,0x50);
    return data ? get_addr(data+0x88) : 0;
}
/* Its child at `path` ("Cursor", "Widgets/Slider/LeftArrow") into `out` (zeroed by the caller). */
static void value_child(Value *v, const char *path, Value *out) {
    call(AT(FIND_CHILD_FN),(Addr)out->bytes,(Addr)v->bytes,(Addr)path,0);
}
static void value_set_visible(Value *v, int visible) {
    const Addr vtable=field_addr(v->bytes,0);
    const Addr set=vtable ? get_addr(vtable+8) : 0;
    if (value_display_object(v) && is_game_function(set)) call(set,(Addr)v->bytes,(uint64_t)(visible!=0),0,0);
}

typedef struct { float x1,y1,x2,y2; } Rect;
static float rect_center_x(const Rect *r) { return (r->x1+r->x2)*0.5f; }

/* Shown: it and every parent visible. */
static int shown(Addr object) {
    for (int depth=0; object && depth<32; ++depth) {
        if (!(get_u8(object+0x6b,0)&0x40)) return 0;
        object=get_addr(object+0x38);
    }
    return 1;
}
/* Whether stage point (x, y) is in the display object (its bounds grown by `grow` pixels);
 * `stage` gets its bounds on the stage. */
static int contains(Addr object, float x, float y, Rect *stage, float grow) {
    const Addr get_bounds=virtual_function(object,0x1f0);
    if (!get_bounds) return 0;
    _Alignas(16) const float identity[8]={1,0,0,0,0,1,0,0};
    _Alignas(16) float local[4]={0};
    call(get_bounds,(Addr)local,object,(Addr)identity,0);
    if (!(local[2]>local[0]) || !(local[3]>local[1])) return 0;
    _Alignas(16) float m[8]={1,0,0,0,0,1,0,0};
    call(AT(WORLD_MATRIX_FN),object,(Addr)m,0,0);
    const float twips=20.0f;
    if (stage) {
        *stage=(Rect){1e9f,1e9f,-1e9f,-1e9f};
        for (int i=0;i<4;++i) {
            const float lx=local[i&1 ? 2 : 0],ly=local[i&2 ? 3 : 1];
            const float sx=(m[0]*lx+m[1]*ly+m[3])/twips,sy=(m[4]*lx+m[5]*ly+m[7])/twips;
            if (sx<stage->x1) stage->x1=sx;
            if (sy<stage->y1) stage->y1=sy;
            if (sx>stage->x2) stage->x2=sx;
            if (sy>stage->y2) stage->y2=sy;
        }
    }
    /* The point in the object's own space (the inverse of the matrix's 2x2 part). */
    const float det=m[0]*m[5]-m[1]*m[4];
    if (!(fabsf(det)>=1e-12f)) return 0;
    const float px=x*twips-m[3],py=y*twips-m[7];
    const float lx=(m[5]*px-m[1]*py)/det,ly=(m[0]*py-m[4]*px)/det;
    const float g=grow*twips/sqrtf(fabsf(det));
    return lx>=local[0]-g && lx<=local[2]+g && ly>=local[1]-g && ly<=local[3]+g;
}

/* ---- A list component of the game ---- */
typedef struct { uint8_t *c; Addr provider,make_item; } List;

static int list_cursor(const List *l) {
    const int forced=field_i32(l->c,F_CURSOR_OVERRIDE);
    return forced>=0 ? forced : field_i32(l->c,F_CURSOR);
}
/* A one-line spinner (one item shown of several: left and right change it). */
static int list_is_spinner(const List *l) {
    return field_i32(l->c,F_LINES)==1 && field_i32(l->c,F_PER_LINE)==1 && field_i32(l->c,F_COUNT)>1;
}
/* Its root display object (spinners). */
static Addr list_root(const List *l) {
    if ((field_u32(l->c,F_ROOT_TYPE)&0x8f)!=10) return 0;
    const Addr data=field_addr(l->c,F_ROOT_DATA);
    return data ? get_addr(data+0x88) : 0;
}
static int list_maker_ready(List *l) {
    l->provider=field_addr(l->c,F_PROVIDER);
    l->make_item=l->provider ? virtual_function(l->provider,0x18) : 0;
    return l->make_item!=0;
}
static void list_make_item(List *l, uint64_t slot, Value *out) {
    call(l->make_item,(Addr)out->bytes,l->provider,(Addr)&slot,0);
}
static const char *cursor_name(void) { return (const char *)AT(CURSOR_NAME); }
/* The item's own cursor area if it has one, else the item; its combo box and slider too. */
static int list_item_contains(Value *item, Addr object, float x, float y, Rect *bounds) {
    Value cursor={{0}};
    value_child(item,cursor_name(),&cursor);
    const Addr area=value_display_object(&cursor);
    int in=contains(area ? area : object,x,y,bounds,0.0f);
    value_release(&cursor);
    static const char *const widgets[]={"Widgets/ComboBox","Widgets/Slider"};
    for (int i=0;i<2 && !in;++i) {
        Value widget={{0}};
        value_child(item,widgets[i],&widget);
        const Addr part=value_display_object(&widget);
        in=part && shown(part) && contains(part,x,y,bounds,0.0f);
        value_release(&widget);
    }
    return in;
}
/* The shown item under stage point (x, y): its index, or -1; `slot` gets its cell and line. */
static int list_item_at(List *l, float x, float y, uint64_t *slot) {
    const int count=field_i32(l->c,F_COUNT),per_line=field_i32(l->c,F_PER_LINE),lines=field_i32(l->c,F_LINES);
    if (count<=0 || per_line<=0 || lines<=0 || per_line>64 || lines>64 || !list_maker_ready(l)) return -1;
    const int stride=lines!=1 ? per_line : (per_line>count ? per_line : count);
    const int line_scroll=field_i32(l->c,F_SCROLL),cell_scroll=field_i32(l->c,F_LINE_SCROLL);
    const int wraps=l->c[F_WRAPS]!=0;
    int best=-1;
    float best_distance=0.0f;
    for (int line=0;line<lines;++line) {
        for (int cell=0;cell<per_line;++cell) {
            int index=(line_scroll+line)*stride+cell_scroll+cell;
            if (wraps) index%=count;
            if (index<0 || index>=count) continue;
            const uint64_t at=(uint32_t)cell|((uint64_t)(uint32_t)line<<32);
            Value item={{0}};
            list_make_item(l,at,&item);
            const Addr object=value_display_object(&item);
            Rect bounds={0};
            if (object && shown(object) && list_item_contains(&item,object,x,y,&bounds)) {
                const float dx=rect_center_x(&bounds)-x,dy=(bounds.y1+bounds.y2)*0.5f-y;
                if (best<0 || dx*dx+dy*dy<best_distance) { best=index; best_distance=dx*dx+dy*dy; *slot=at; }
            }
            value_release(&item);
        }
    }
    return best;
}
/* The button a click at (x, y) on item `slot` presses: a slider's arrow or a combo box's side,
 * else confirm. */
static uint32_t list_click_button(List *l, uint64_t slot, float x, float y, uint32_t confirm) {
    if (!list_maker_ready(l)) return confirm;
    Value item={{0}};
    list_make_item(l,slot,&item);
    uint32_t button=confirm;
    if (value_display_object(&item)) {
        static const struct { const char *path; float grow; } parts[]={
            {"Widgets/Slider/LeftArrow",14.0f}, {"Widgets/Slider/RightArrow",14.0f}, {"Widgets/ComboBox",0.0f},
        };
        for (int i=0;i<3 && button==confirm;++i) {
            Value part={{0}};
            Rect bounds={0};
            value_child(&item,parts[i].path,&part);
            const Addr object=value_display_object(&part);
            if (object && shown(object) && contains(object,x,y,&bounds,parts[i].grow))
                button=i==0 ? BTN_LEFT : i==1 ? BTN_RIGHT : x<rect_center_x(&bounds) ? BTN_LEFT : BTN_RIGHT;
            value_release(&part);
        }
    }
    value_release(&item);
    return button;
}
static void list_cursor_moved(List *l, int index) {
    const Addr listener=field_addr(l->c,F_LISTENER);
    const Addr moved=listener ? virtual_function(listener,0x10) : 0;
    if (moved) {
        int32_t at=index;
        call(moved,listener,(Addr)&at,0,0);
    }
    /* The cursor's sound, unless one is already queued. */
    int32_t *sound=(int32_t *)AT(MENU_SOUND);
    if (sound[1]<=0) {
        sound[0]=0; sound[1]=0;
        *(Addr *)(AT(MENU_SOUND)+8)=AT(CURSOR_MOVE_SOUND);
    }
}
/* Moves the game's cursor to `index` (item `slot`) as its own input would: the scroll, the
 * cursor shown, the listener told, the sound. 0: it did not move there. */
static int list_move_to(List *l, int index, uint64_t slot) {
    const int before=list_cursor(l);
    if (index==before) return 1;
    const int line_scroll=field_i32(l->c,F_LINE_SCROLL),scroll=field_i32(l->c,F_SCROLL);
    const int scrolled=(int32_t)call(AT(SET_CURSOR_FN),(Addr)l->c,(uint64_t)(int64_t)index,0,0);
    const int d_line=line_scroll-field_i32(l->c,F_LINE_SCROLL),d_scroll=scroll-field_i32(l->c,F_SCROLL);
    if (scrolled==1 && (d_line || d_scroll)) {
        /* The list scrolled to it: back where the pointer is, with the cursor on that item. */
        call(AT(SCROLL_BY_FN),(Addr)l->c,(uint64_t)(int64_t)d_line,(uint64_t)(int64_t)d_scroll,0);
        call(AT(HIDE_CURSORS_FN),(Addr)l->c,0,0,0);
        if (list_maker_ready(l)) {
            Value item={{0}},cursor={{0}};
            list_make_item(l,slot,&item);
            value_child(&item,cursor_name(),&cursor);
            value_set_visible(&cursor,1);
            value_release(&cursor);
            value_release(&item);
        }
    }
    const int now=list_cursor(l);
    if (now!=before) list_cursor_moved(l,now);
    return now==index;
}

/* ---- The pointer (host window), for the game's thread and the pad's ---- */
static _Atomic uint32_t presses;      /* buttons to press once (pad thread) */
static _Atomic uint32_t repeat_press; /* an arrow held under the pointer */
static _Atomic int pad_steering;      /* the pad's directions are in use: hover waits */
static _Atomic int64_t list_focus_ms;
static _Atomic int64_t choice_focus_ms; /* a focused list with several items was last updated */
static _Atomic uint32_t click_used;   /* the left press (its serial) already acted on */
/* BB_MENU_POINTER_FILE: a pointer of the test's instead of the window's. */
static _Atomic int test_pointer;
static BbGpuPointerSample test_sample;

#define CLICK_LIFE_NS UINT64_C(250000000)

static int pointer_sample(BbGpuPointerSample *out) {
    if (atomic_load_explicit(&test_pointer,memory_order_acquire)) { *out=test_sample; return 1; }
#ifdef __APPLE__
    const BbGpuShared *s=pointer_shared ? (const BbGpuShared *)pointer_shared() : NULL;
    return s && bbgpu_pointer_read(&s->pointer,out);
#else
    (void)out; return 0;
#endif
}
static int input_blocked(void) {
    const Addr flow=get_addr(AT(MENU_FLOW));
    if (flow && get_u64(flow+0xa78)) return 1;
    const Addr block=get_addr(AT(INPUT_BLOCK));
    return block && get_u32(block+0x2e4);
}
/* 1 the title menu, 2 a menu object: what runtime_pad.c's menu keys go by. */
int runtime_menu_pointer_game_menu(void) {
    if (!flow_ready) return 0;
    const Addr flow=get_addr(AT(MENU_FLOW));
    uint8_t state[0x9a]; /* flow+0xa78 .. flow+0xb11 (the title flag) */
    if (!flow || flow>UINTPTR_MAX-0xb12 || !runtime_read_guest(flow+0xa78,state,sizeof state)) return 0;
    Addr menu=0;
    memcpy(&menu,state+0x10,sizeof menu);
    return (!state[0x99] ? 1 : 0)|(menu ? 2 : 0);
}
/* A menu the pointer works in: the title menu, and any list with several items that has the
 * input focus (400 ms on, so the pointer stays through a menu's transitions). Not a message with
 * one button or a pick-up notice: the game counts those as menus (a menu object, a focused list
 * of one item), but the hunter moves and fights on behind them (seen at the locked gate by the
 * Central Yharnam lamp), so the mouse must stay the camera and its buttons the weapons. The menu
 * object going away is the menu closing: the tail would keep the pointer ~0.4 s over the game. */
static int in_menu(void) {
    const int menu=runtime_menu_pointer_game_menu();
    static _Atomic int had_menu;
    if (atomic_exchange_explicit(&had_menu,menu&2,memory_order_relaxed) && !(menu&2))
        atomic_store_explicit(&choice_focus_ms,0,memory_order_relaxed);
    return (menu&1) || now_ms()-atomic_load_explicit(&choice_focus_ms,memory_order_relaxed)<400;
}
static uint32_t confirm_button(void) {
    int32_t region=-1;
    return runtime_read_guest(AT(REGION),&region,sizeof region) && region==5 ? BTN_CIRCLE : BTN_CROSS;
}
static uint32_t back_button(void) { return confirm_button()==BTN_CIRCLE ? BTN_CROSS : BTN_CIRCLE; }

/* Where the pointer was over each list the last time it was seen: hovering acts on a move only,
 * so a pointer resting somewhere does not fight the keyboard or the pad. */
static struct { const uint8_t *list; float x,y; int64_t ms; } seen[32];
static unsigned seen_next;
/* Whether the pointer moved over `list` since it was last seen (a list not seen for a while
 * counts as not moved: it just opened under the pointer). */
static int pointer_moved(const uint8_t *list, float x, float y) {
    const int64_t now=now_ms();
    for (int i=0;i<32;++i) {
        if (seen[i].list!=list) continue;
        const int moved=now-seen[i].ms<=250 && (seen[i].x!=x || seen[i].y!=y);
        seen[i].x=x; seen[i].y=y; seen[i].ms=now;
        return moved;
    }
    const unsigned at=seen_next++%32;
    seen[at].list=list; seen[at].x=x; seen[at].y=y; seen[at].ms=now;
    return 0;
}

/* The game's list update (0x1acff30) is about to run for `component` (the game's thread). */
void runtime_menu_pointer_list_update(uint64_t component_address, uint64_t input_address) {
    if (!atomic_load_explicit(&hooked,memory_order_acquire)) return;
    uint8_t *component=(uint8_t *)(uintptr_t)component_address;
    uint8_t focus=0,probe=0;
    if (!component || !runtime_read_guest((Addr)input_address,&focus,1) || !focus ||
        !runtime_read_guest((Addr)component+COMPONENT_SIZE-1,&probe,1) || input_blocked()) return;
    atomic_store_explicit(&list_focus_ms,now_ms(),memory_order_relaxed);
    if (field_i32(component,F_COUNT)>1) atomic_store_explicit(&choice_focus_ms,now_ms(),memory_order_relaxed);
    BbGpuPointerSample p;
    if (bbgpu_overlay_captures_input() || !pointer_sample(&p) || !p.inside) return;
    const float x=p.x,y=p.y;
    const int moved=pointer_moved(component,x,y);
    const uint64_t now=host_monotonic_ns();
    const int clicked=p.click_serial!=atomic_load_explicit(&click_used,memory_order_relaxed) &&
        now>=p.click_ns && now-p.click_ns<=CLICK_LIFE_NS;
    List list={component,0,0};
    if (list_is_spinner(&list)) {
        /* One value shown: a click on its left half goes left, on the right half right. */
        Rect bounds={0};
        const Addr root=list_root(&list);
        if (clicked && root && shown(root) && contains(root,x,y,&bounds,0.0f)) {
            atomic_store_explicit(&click_used,p.click_serial,memory_order_relaxed);
            atomic_fetch_or_explicit(&presses,x<rect_center_x(&bounds) ? BTN_LEFT : BTN_RIGHT,memory_order_relaxed);
        }
        return;
    }
    if ((!moved && !clicked) || atomic_load_explicit(&pad_steering,memory_order_relaxed)) return;
    uint64_t slot=0;
    const int index=list_item_at(&list,x,y,&slot);
    if (index<0) return;
    if (clicked) atomic_store_explicit(&click_used,p.click_serial,memory_order_relaxed);
    static int trace=-1;
    if (trace<0) trace=getenv("BB_MENU_POINTER_FILE")!=NULL || getenv("BB_MENU_POINTER_LOG")!=NULL;
    if (trace) printf("Menu pointer: list %p item %d (cursor %d)%s\n",(void *)component,index,list_cursor(&list),
                      clicked ? " click" : "");
    if (list_move_to(&list,index,slot) && clicked) {
        const uint32_t button=list_click_button(&list,slot,x,y,confirm_button());
        atomic_fetch_or_explicit(&presses,button,memory_order_relaxed);
        /* A held click on a slider's arrow keeps moving it. */
        if (button==BTN_LEFT || button==BTN_RIGHT) atomic_store_explicit(&repeat_press,button,memory_order_relaxed);
    }
}

static void read_test_file(void) {
    static const char *path;
    static int checked;
    static int64_t next_ms;
    static char last[128];
    if (!checked) { path=getenv("BB_MENU_POINTER_FILE"); checked=1; }
    const int64_t now=now_ms();
    if (!path || now<next_ms) return;
    next_ms=now+200;
    char text[128]={0};
    FILE *f=fopen(path,"r");
    if (f) { text[fread(text,1,sizeof text-1,f)]=0; fclose(f); }
    if (!text[0] || !strcmp(text,last)) return;
    memcpy(last,text,sizeof last);
    float x=0,y=0;
    char action[16]={0};
    if (sscanf(text,"%f %f %15s",&x,&y,action)<2) return;
    atomic_store_explicit(&test_pointer,0,memory_order_release);
    test_sample.x=x; test_sample.y=y; test_sample.inside=1;
    if (!strcmp(action,"click")) { ++test_sample.click_serial; test_sample.click_ns=host_monotonic_ns(); }
    else if (!strcmp(action,"right")) ++test_sample.back_serial;
    else if (!strcmp(action,"up")) ++test_sample.up_serial;
    else if (!strcmp(action,"down")) ++test_sample.down_serial;
    atomic_store_explicit(&test_pointer,1,memory_order_release);
    printf("Menu pointer: test %.0f %.0f %s (in a menu: %d)\n",x,y,action,in_menu());
}

/* Whether a list with several items has the input focus now: a menu the d-pad moves through.
 * A message with one button and a pick-up notice are menus to the game too, but have nothing to
 * move through: the d-pad then reaches the hunter (weapons, quick items, blood bullets). */
int runtime_menu_pointer_choice(void) {
    return atomic_load_explicit(&hooked,memory_order_acquire) &&
        now_ms()-atomic_load_explicit(&choice_focus_ms,memory_order_relaxed)<250;
}

int runtime_menu_pointer_wanted(void) {
    return atomic_load_explicit(&hooked,memory_order_acquire) && in_menu();
}

/* The pad state the game reads (pad thread): the pointer's presses, each held long enough for
 * the game to see it, with a gap after. Tells the host whether a menu wants the pointer. */
void runtime_menu_pointer_pad(uint32_t *buttons, uint8_t left_x, uint8_t left_y) {
    if (!atomic_load_explicit(&hooked,memory_order_acquire)) return;
    read_test_file();
    const int menu=in_menu() && !bbgpu_overlay_captures_input();
    /* BB_MENU_POINTER_LOG=1: each signal a menu is told by, at every change, with the pad. */
    static int log_signals=-1,last_signals=-1;
    if (log_signals<0) { const char *e=getenv("BB_MENU_POINTER_LOG"); log_signals=e && e[0]=='1'; }
    if (log_signals) {
        const int game=runtime_menu_pointer_game_menu();
        const int focus=now_ms()-atomic_load_explicit(&list_focus_ms,memory_order_relaxed)<400;
        const int signals=game|focus<<2|input_blocked()<<3|runtime_menu_pointer_choice()<<4;
        if (signals!=last_signals) {
            last_signals=signals;
            printf("Menu pointer signals at %lld ms: title %d, menu object %d, list focus %d, input blocked %d, several items %d (pad buttons 0x%x, stick %u %u)\n",
                   (long long)now_ms(),game&1,game>>1&1,focus,signals>>3&1,signals>>4&1,*buttons,left_x,left_y);
        }
    }
    static int was=-1;
    if (menu!=was) {
        was=menu;
        printf("Menu pointer: %s\n",menu ? "a game menu is open (pointer)" : "menus closed");
    }
#ifdef __APPLE__
    BbGpuShared *s=pointer_shared ? (BbGpuShared *)pointer_shared() : NULL;
    if (s) __atomic_store_n(&s->menu_pointer,(uint32_t)menu,__ATOMIC_RELEASE);
#endif
#define OFF_CENTRE(v) ((v)<64 || (v)>192)
    atomic_store_explicit(&pad_steering,(*buttons&(BTN_UP|BTN_RIGHT|BTN_DOWN|BTN_LEFT)) || OFF_CENTRE(left_x) ||
                          OFF_CENTRE(left_y),memory_order_relaxed);
#undef OFF_CENTRE
    static uint32_t holding,back_seen,up_seen,down_seen;
    static int primed,left_held;
    static int64_t until_ms,repeat_ms;
    BbGpuPointerSample p;
    if (pointer_sample(&p)) {
        if (primed && menu) {
            if (p.back_serial!=back_seen) atomic_fetch_or_explicit(&presses,back_button(),memory_order_relaxed);
            if (p.up_serial!=up_seen) atomic_fetch_or_explicit(&presses,BTN_UP,memory_order_relaxed);
            if (p.down_serial!=down_seen) atomic_fetch_or_explicit(&presses,BTN_DOWN,memory_order_relaxed);
        }
        back_seen=p.back_serial; up_seen=p.up_serial; down_seen=p.down_serial;
        left_held=p.left_held!=0; primed=1;
        if (!left_held) atomic_store_explicit(&repeat_press,0,memory_order_relaxed);
    }
    const int64_t now=now_ms();
    if (!menu) {
        atomic_store_explicit(&presses,0,memory_order_relaxed);
        atomic_store_explicit(&repeat_press,0,memory_order_relaxed);
        holding=0;
        return;
    }
    enum { PRESS_MS=50, GAP_MS=40, REPEAT_DELAY_MS=400, REPEAT_MS=120 };
    if (holding && now>=until_ms) { holding=0; until_ms=now+GAP_MS; }
    if (!holding && now>=until_ms) {
        const uint32_t next=atomic_exchange_explicit(&presses,0,memory_order_relaxed);
        const uint32_t arrow=atomic_load_explicit(&repeat_press,memory_order_relaxed);
        if (next) { holding=next; until_ms=now+PRESS_MS; repeat_ms=now+REPEAT_DELAY_MS; }
        else if (arrow && left_held && now>=repeat_ms) { holding=arrow; until_ms=now+PRESS_MS; repeat_ms=now+REPEAT_MS; }
    }
    *buttons|=holding;
}

void runtime_menu_pointer_set_shared(void *(*shared)(void)) { pointer_shared=shared; }

/* The loader's image: the menu flow is read from it; the functions the pointer calls are checked
 * here, the list update's hook by runtime_hooks.c (runtime_menu_pointer_hooked). */
void runtime_menu_pointer_set_image(unsigned char *base, uint64_t size) {
    static const unsigned char entry[]={0x55,0x48,0x89,0xe5};
    static const uint32_t functions[]={LIST_UPDATE,SET_CURSOR_FN,SCROLL_BY_FN,HIDE_CURSORS_FN,FIND_CHILD_FN,WORLD_MATRIX_FN};
    image=NULL; flow_ready=0;
    atomic_store_explicit(&hooked,0,memory_order_release);
    if (!base || size<MENU_FLOW+sizeof(Addr) || size<MENU_SOUND+16 || size<INPUT_BLOCK+sizeof(Addr)) return;
    for (size_t i=0;i<sizeof functions/sizeof *functions;++i)
        if (memcmp(base+functions[i],entry,sizeof entry)) return;
    if (memcmp(base+CURSOR_NAME,"Cursor",7)) return;
    image=base; flow_ready=1;
}
int runtime_menu_pointer_ready(void) {
    const char *off=getenv("BB_MENU_POINTER");
    return image && !(off && off[0]=='0');
}
void runtime_menu_pointer_hooked(void) {
    atomic_store_explicit(&hooked,1,memory_order_release);
    puts("Menu pointer: on (the mouse points and clicks in the game's menus)");
}
