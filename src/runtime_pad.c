// SPDX-FileCopyrightText: Copyright 2026 Mark Hall
/* libScePad on SDL3 gamepads, with a keyboard fallback. SDL events are pumped
 * by the window thread (gpu/shim/window.cpp); here state is only sampled.
 *
 * Keyboard and mouse buttons (beside a gamepad): bbport.ini bind_ keys, runtime_bindings.c. The
 * defaults follow Dark Souls III: WASD moves (and moves through the game's menus), Space taps / holds
 * Circle for dodge / run, E interacts, Q locks on / backs out of menus, F transforms the weapon.
 * Bloodborne's blood vial uses V, leaving R for quick items; Ctrl / right click fires the gun.
 * Optional separate dodge / sprint bindings and custom controller mappings are still supported.
 * Controller buttons: bbport.ini bind_pad_ keys (remap_buttons).
 * Mouse (split mode, after a click in the window): the camera (right stick); by default left
 * button R1, Shift + left R2, right L2, middle R3, side buttons R2 (back) and L1 (forward),
 * Shift + wheel up / down the d-pad's right (weapon) and down (quick item). */
#define _GNU_SOURCE
#include "runtime.h"
#include "gpu/bbgpu.h"
#include "runtime_bindings.h"
#include "host/bbgpu_mouse_input.h"
#include "host/bbgpu_pointer_input.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <SDL3/SDL.h>
#include <math.h>
#include <sys/stat.h>
#ifdef __APPLE__
#include "host/bbgpu_shared.h"
#include <mach/mach.h>
#include <mach/mach_vm.h>
#elif defined(__linux__)
#include <sys/uio.h>
#include <unistd.h>
#endif

#define ERR_INVALID_ARG ((int32_t)0x80920001)
#define ERR_INVALID_HANDLE ((int32_t)0x80920003)
#define ERR_ALREADY_OPENED ((int32_t)0x80920004)
#define ERR_NOT_INITIALIZED ((int32_t)0x80920005)
#define PAD_HANDLE 1

enum {
    BTN_L3=0x2, BTN_R3=0x4, BTN_OPTIONS=0x8, BTN_UP=0x10, BTN_RIGHT=0x20, BTN_DOWN=0x40, BTN_LEFT=0x80,
    BTN_L2=0x100, BTN_R2=0x200, BTN_L1=0x400, BTN_R1=0x800, BTN_TRIANGLE=0x1000, BTN_CIRCLE=0x2000,
    BTN_CROSS=0x4000, BTN_SQUARE=0x8000, BTN_TOUCHPAD=0x100000,
};
typedef struct { uint16_t x, y; uint8_t id, reserve[3]; } PadTouch;
typedef struct {
    uint32_t buttons;
    uint8_t left_x, left_y, right_x, right_y;
    uint8_t l2, r2, analog_padding[2];
    float orientation[4], acceleration[3], angular_velocity[3];
    uint8_t touch_count, touch_reserve[3];
    uint32_t touch_held_time;
    PadTouch touches[2];
    uint8_t connected, pad0[3];
    uint64_t timestamp;
    uint8_t extension[16];
    uint8_t connected_count, reserve[2], unique_length, unique[12];
} PadData;
typedef struct {
    float pixel_density; uint16_t resolution_x, resolution_y;
    uint8_t dead_zone_left, dead_zone_right, connection_type, connected_count;
    uint8_t connected, pad[3];
    int32_t device_class;
    uint8_t reserve[8];
} ControllerInfo;
_Static_assert(sizeof(PadData)==120,"OrbisPadData layout");
_Static_assert(sizeof(PadTouch)==8,"OrbisPadTouch layout");
_Static_assert(__builtin_offsetof(PadData,touches)==60,"OrbisPadData touch offset");
_Static_assert(__builtin_offsetof(PadData,timestamp)==80,"OrbisPadData timestamp offset");
_Static_assert(sizeof(ControllerInfo)==28,"OrbisPadControllerInformation layout");

static HostMutex lock = HOST_MUTEX_INIT;
static int initialized, opened, sdl_ready;
static SDL_Gamepad *gamepad;
static size_t reads;
static uint8_t connected_count;

static uint64_t now_us(void) { return host_monotonic_ns() / 1000u; }
static uint8_t axis(int16_t v) { int x=(v+32768)>>8; return (uint8_t)(x<0 ? 0 : x>255 ? 255 : x); }
static uint8_t trigger(int16_t v) { int x=v>>7; return (uint8_t)(x<0 ? 0 : x>255 ? 255 : x); }
static uint16_t touch_axis(float v, int max) {
    return (uint16_t)(v<=0.0f ? 0 : v>=1.0f ? max : (int)(v*max+0.5f));
}
static void touch_click(PadData *d, int right) {
    d->buttons|=BTN_TOUCHPAD;
    d->touch_count=1;
    d->touches[0]=(PadTouch){.x=right ? 1440 : 480,.y=471,.id=0};
}

/* Opens the first gamepad SDL knows about; called under lock. */
static SDL_Gamepad *current_gamepad(void) {
    if (!sdl_ready) sdl_ready = SDL_WasInit(SDL_INIT_GAMEPAD) ? 1 : SDL_InitSubSystem(SDL_INIT_GAMEPAD) ? 1 : -1;
    if (sdl_ready<0) return NULL;
    /* Without a window in this process (the GPU process split: the window and its event loop are
     * in bbgpu-host) nothing pumps SDL's events: poll the gamepads here. */
    if (!SDL_WasInit(SDL_INIT_VIDEO)) SDL_UpdateGamepads();
    if (gamepad && !SDL_GamepadConnected(gamepad)) { SDL_CloseGamepad(gamepad); gamepad=NULL; }
    if (!gamepad) {
        int count=0;
        SDL_JoystickID *ids=SDL_GetGamepads(&count);
        if (ids && count>0) {
            gamepad=SDL_OpenGamepad(ids[0]);
            if (gamepad) { ++connected_count; printf("Runtime: gamepad connected: %s\n",SDL_GetGamepadName(gamepad)); }
        }
        SDL_free(ids);
    }
    return gamepad;
}
static int mouse_look_active(void);
static uint32_t mouse_inputs(const bool *keys,uint64_t *wheel_actions);
static void *(*pad_shared)(void);
static int mouse_moving(void);
/* A stick axis from two opposite keys: neutral 128, `reach` towards the held side (both: 127). */
static uint8_t key_axis(int negative, int positive, int reach) {
    const int v=128-(negative ? reach : 0)+(positive ? (reach>127 ? 127 : reach) : 0);
    return (uint8_t)(v<0 ? 0 : v>255 ? 255 : v);
}

/* Bloodborne 1.09 menu-flow offsets, documented by bloodborne-mac's
 * gpu/games/bloodborne_menu_pointer.cpp. Read-only: no guest hooks are installed. The list
 * update prologue guards the layout; a missing / unsupported image leaves gameplay alone.
 * Kernel-checked reads also tolerate a menu object disappearing between pad samples. */
#define MENU_FLOW_OFFSET 0x5580e38u
#define MENU_LIST_UPDATE_OFFSET 0x1acff30u
static uintptr_t menu_flow_address;
void runtime_pad_set_image(unsigned char *image, uint64_t size) {
    static const unsigned char signature[]={0x55,0x48,0x89,0xe5,0x41,0x57};
    menu_flow_address=0;
    runtime_menu_pointer_set_image(image,size);
    if (!image || size<MENU_FLOW_OFFSET+sizeof(uintptr_t) ||
        memcmp(image+MENU_LIST_UPDATE_OFFSET,signature,sizeof signature)) return;
    menu_flow_address=(uintptr_t)image+MENU_FLOW_OFFSET;
}
static int pad_read_memory(uintptr_t address,void *out,size_t size) {
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
static int game_menu_active(void) {
    uintptr_t flow=0,menu=0;
    unsigned char state[0x9a]; /* flow+0xa78 .. flow+0xb11 (title flag) */
    if (!menu_flow_address || !pad_read_memory(menu_flow_address,&flow,sizeof flow) ||
        !flow || flow>UINTPTR_MAX-0xb12 || !pad_read_memory(flow+0xa78,state,sizeof state)) return 0;
    memcpy(&menu,state+0x10,sizeof menu);
    /* The title menu, or a menu with something to choose from (a list with several items has
     * the input focus: runtime_menu_pointer.c). A message with one button and a pick-up notice
     * are menu objects too, but the hunter moves and fights on behind them: the keys and the
     * mouse must stay what they are in play (the d-pad there switched weapons and made blood
     * bullets; Back was a dodge). */
    return !state[0x99] || (menu!=0 && runtime_menu_pointer_choice());
}
int runtime_pad_menu_active(void) { return game_menu_active(); }
/* The controller's buttons (bit per PAD_* button) and triggers as the game sees them, after
 * bbport.ini bind_pad_ keys (runtime_bindings.c). A trigger counts as pressed past 30 and keeps
 * its analog value when it is bound to L2/R2; any other button bound there presses it fully. */
static void remap_buttons(PadData *d, uint32_t held, uint8_t l2, uint8_t r2) {
    static const uint32_t ps[PAD_COUNT]={
        [PAD_CROSS]=BTN_CROSS, [PAD_CIRCLE]=BTN_CIRCLE, [PAD_SQUARE]=BTN_SQUARE, [PAD_TRIANGLE]=BTN_TRIANGLE,
        [PAD_L1]=BTN_L1, [PAD_R1]=BTN_R1, [PAD_L2]=BTN_L2, [PAD_R2]=BTN_R2, [PAD_L3]=BTN_L3, [PAD_R3]=BTN_R3,
        [PAD_OPTIONS]=BTN_OPTIONS, [PAD_TOUCHPAD]=BTN_TOUCHPAD,
        [PAD_UP]=BTN_UP, [PAD_DOWN]=BTN_DOWN, [PAD_LEFT]=BTN_LEFT, [PAD_RIGHT]=BTN_RIGHT,
    };
    if (l2>30) held|=1u<<PAD_L2; else held&=~(1u<<PAD_L2);
    if (r2>30) held|=1u<<PAD_R2; else held&=~(1u<<PAD_R2);
    for (int input=0;input<PAD_COUNT;++input) {
        for (int slot=0;slot<BIND_SLOTS;++slot) {
            const int b=runtime_bindings_pad(input,slot);
            if (b<0) continue;
            const uint8_t value=b==PAD_L2 ? l2 : b==PAD_R2 ? r2 : (held>>b)&1 ? 255 : 0;
            if (input==PAD_L2 && value>d->l2) d->l2=value;
            if (input==PAD_R2 && value>d->r2) d->r2=value;
            if ((held>>b)&1) d->buttons|=ps[input];
        }
    }
}
/* Circle from the keyboard (bloodborne-mac's keyboard_circle). The dodge is a short press whatever
 * the key does: held, it would be the run (the game tells a tap from a hold by the time). The
 * sprint key holds Circle: the run. The dodge key while the sprint key is held is the jump: the
 * way a player gets it with a pad, Circle let go, pressed, let go, then held again (the run goes
 * on). One release and press only stopped the run and gave no jump. */
#define DODGE_PULSE_US 90000u
#define JUMP_GAP_US 70000u /* each pause */
#define JUMP_TAP_US 70000u /* the press between them */
static uint64_t dodge_pulse_until, jump_start_us;
static int dodge_was_down;
static int keyboard_circle(int dodge, int sprint, uint64_t now) {
    const uint64_t jump_us=2u*JUMP_GAP_US+JUMP_TAP_US;
    const int jumping=jump_start_us && now<jump_start_us+jump_us;
    if (dodge && !dodge_was_down && !jumping) {
        if (sprint) { jump_start_us=now; dodge_pulse_until=0; }
        else { dodge_pulse_until=now+DODGE_PULSE_US; jump_start_us=0; }
    }
    dodge_was_down=dodge;
    if (jump_start_us && now<jump_start_us+jump_us) {
        const uint64_t t=now-jump_start_us;
        return t>=JUMP_GAP_US && t<JUMP_GAP_US+JUMP_TAP_US; /* low, the press, low */
    }
    return sprint || now<dodge_pulse_until;
}

/* Button prompts (gpu/shim/bbport_button_prompts.cpp draws the game's key guide icons for the
 * device in use): the device whose input came last. 0 a PlayStation pad (the game's own icons),
 * 1 another gamepad (its labels), 2 the keyboard and mouse. The generation changes with the
 * device, the pad's type and the Mouse camera setting; the labels go to the GPU host in the
 * shared block. From bloodborne-mac's runtime_pad.c (bbport 0.6). */
static int prompt_device=-1, prompt_look=-1;
static SDL_GamepadType prompt_pad_type=SDL_GAMEPAD_TYPE_UNKNOWN;
static uint32_t prompt_generation, prompt_published;
static int stick_moved(uint8_t v) { return v<128-48 || v>128+48; }
/* What a controller button is called on a pad that is not a PlayStation one. */
static int pad_prompt(int input, SDL_GamepadType type, char *label, int size) {
    const int nintendo=type==SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO ||
                       type==SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_LEFT ||
                       type==SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT ||
                       type==SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_PAIR;
    int button=-1;
    for (int slot=0;slot<BIND_SLOTS && button<0;++slot) button=runtime_bindings_pad(input,slot);
    switch (button) {
    case PAD_CROSS: case PAD_CIRCLE: case PAD_SQUARE: case PAD_TRIANGLE: {
        static const SDL_GamepadButton sdl[]={SDL_GAMEPAD_BUTTON_SOUTH,SDL_GAMEPAD_BUTTON_EAST,
                                              SDL_GAMEPAD_BUTTON_WEST,SDL_GAMEPAD_BUTTON_NORTH};
        static const char *const by_position[]={"A","B","X","Y"};
        const int at=button==PAD_CROSS ? 0 : button==PAD_CIRCLE ? 1 : button==PAD_SQUARE ? 2 : 3;
        const SDL_GamepadButtonLabel l=SDL_GetGamepadButtonLabelForType(type,sdl[at]);
        snprintf(label,size,"%s",l==SDL_GAMEPAD_BUTTON_LABEL_A ? "A" : l==SDL_GAMEPAD_BUTTON_LABEL_B ? "B"
                 : l==SDL_GAMEPAD_BUTTON_LABEL_X ? "X" : l==SDL_GAMEPAD_BUTTON_LABEL_Y ? "Y" : by_position[at]);
        return 'F';
    }
    case PAD_L1: snprintf(label,size,"%s",nintendo ? "L" : "LB"); return 'S';
    case PAD_R1: snprintf(label,size,"%s",nintendo ? "R" : "RB"); return 'S';
    case PAD_L2: snprintf(label,size,"%s",nintendo ? "ZL" : "LT"); return 'S';
    case PAD_R2: snprintf(label,size,"%s",nintendo ? "ZR" : "RT"); return 'S';
    case PAD_L3: snprintf(label,size,"LS"); return 'C';
    case PAD_R3: snprintf(label,size,"RS"); return 'C';
    case PAD_OPTIONS: snprintf(label,size,"%s",nintendo ? "+" : "Menu"); return 'C';
    case PAD_SELECT: snprintf(label,size,"%s",nintendo ? "-" : "View"); return 'C';
    case PAD_MISC: snprintf(label,size,"Share"); return 'C';
    case PAD_TOUCHPAD: snprintf(label,size,"Pad"); return 'C';
    case PAD_PADDLE1: case PAD_PADDLE2: case PAD_PADDLE3: case PAD_PADDLE4:
        snprintf(label,size,"P%d",button-PAD_PADDLE1+1); return 'C';
    default: return 0; /* the d-pad: the game's icons fit any pad */
    }
}
static void prompt_fill(BbGpuPrompts *out) {
    /* Circle: the real button first (Space by default: it is Circle in menus and in play). */
    static const int8_t keyboard[BBGPU_PROMPT_INPUTS][3]={
        [BBGPU_PROMPT_CROSS]={BIND_CROSS,-1,-1}, [BBGPU_PROMPT_CIRCLE]={BIND_CIRCLE,BIND_DODGE,BIND_MENU_BACK},
        [BBGPU_PROMPT_SQUARE]={BIND_SQUARE,-1,-1}, [BBGPU_PROMPT_TRIANGLE]={BIND_TRIANGLE,-1,-1},
        [BBGPU_PROMPT_L1]={BIND_L1,-1,-1}, [BBGPU_PROMPT_L2]={BIND_L2,-1,-1},
        [BBGPU_PROMPT_R1]={BIND_R1,-1,-1}, [BBGPU_PROMPT_R2]={BIND_R2,-1,-1},
        [BBGPU_PROMPT_L3]={BIND_L3,-1,-1}, [BBGPU_PROMPT_R3]={BIND_R3,-1,-1},
        [BBGPU_PROMPT_OPTIONS]={BIND_OPTIONS,-1,-1},
        [BBGPU_PROMPT_TOUCH_LEFT]={BIND_TOUCH_LEFT,-1,-1}, [BBGPU_PROMPT_TOUCH_RIGHT]={BIND_TOUCH_RIGHT,-1,-1},
        [BBGPU_PROMPT_UP]={BIND_DPAD_UP,-1,-1}, [BBGPU_PROMPT_DOWN]={BIND_DPAD_DOWN,-1,-1},
        [BBGPU_PROMPT_LEFT]={BIND_DPAD_LEFT,-1,-1}, [BBGPU_PROMPT_RIGHT]={BIND_DPAD_RIGHT,-1,-1},
        [BBGPU_PROMPT_MOVE_UP]={BIND_MOVE_FORWARD,-1,-1}, [BBGPU_PROMPT_MOVE_LEFT]={BIND_MOVE_LEFT,-1,-1},
        [BBGPU_PROMPT_MOVE_DOWN]={BIND_MOVE_BACK,-1,-1}, [BBGPU_PROMPT_MOVE_RIGHT]={BIND_MOVE_RIGHT,-1,-1},
        [BBGPU_PROMPT_LOOK_UP]={BIND_CAMERA_UP,-1,-1}, [BBGPU_PROMPT_LOOK_LEFT]={BIND_CAMERA_LEFT,-1,-1},
        [BBGPU_PROMPT_LOOK_DOWN]={BIND_CAMERA_DOWN,-1,-1}, [BBGPU_PROMPT_LOOK_RIGHT]={BIND_CAMERA_RIGHT,-1,-1},
    };
    static const int8_t pad[BBGPU_PROMPT_INPUTS]={
        [BBGPU_PROMPT_CROSS]=PAD_CROSS, [BBGPU_PROMPT_CIRCLE]=PAD_CIRCLE, [BBGPU_PROMPT_SQUARE]=PAD_SQUARE,
        [BBGPU_PROMPT_TRIANGLE]=PAD_TRIANGLE, [BBGPU_PROMPT_L1]=PAD_L1, [BBGPU_PROMPT_L2]=PAD_L2,
        [BBGPU_PROMPT_R1]=PAD_R1, [BBGPU_PROMPT_R2]=PAD_R2, [BBGPU_PROMPT_L3]=PAD_L3, [BBGPU_PROMPT_R3]=PAD_R3,
        [BBGPU_PROMPT_OPTIONS]=PAD_OPTIONS, [BBGPU_PROMPT_TOUCH_LEFT]=PAD_TOUCHPAD, [BBGPU_PROMPT_TOUCH_RIGHT]=PAD_TOUCHPAD,
        [BBGPU_PROMPT_UP]=PAD_UP, [BBGPU_PROMPT_DOWN]=PAD_DOWN, [BBGPU_PROMPT_LEFT]=PAD_LEFT, [BBGPU_PROMPT_RIGHT]=PAD_RIGHT,
    };
    memset(out,0,sizeof *out);
    out->generation=prompt_generation; out->device=prompt_device; out->mouse_look=prompt_look>0;
    for (int input=0;input<BBGPU_PROMPT_INPUTS;++input) for (int variant=0;variant<2;++variant) {
        char *label=out->label[variant][input];
        int kind=0;
        if (prompt_device==2) {
            for (int i=0;i<3 && !kind && keyboard[input][i]>=0;++i)
                kind=runtime_bindings_prompt(keyboard[input][i],variant && prompt_look>0,label,BBGPU_PROMPT_LABEL);
        } else if (prompt_device==1) {
            if (input>=BBGPU_PROMPT_MOVE_UP) {
                snprintf(label,BBGPU_PROMPT_LABEL,"%s",input<BBGPU_PROMPT_LOOK_UP ? "LS" : "RS"); kind='C';
            } else kind=pad_prompt(pad[input],prompt_pad_type,label,BBGPU_PROMPT_LABEL);
        }
        out->kind[variant][input]=(uint8_t)kind;
    }
}
static void prompt_note(SDL_Gamepad *g, int pad_active, int keyboard_active) {
    int device=prompt_device;
    const SDL_GamepadType type=g ? SDL_GetGamepadType(g) : SDL_GAMEPAD_TYPE_UNKNOWN;
    const int playstation=type==SDL_GAMEPAD_TYPE_PS3 || type==SDL_GAMEPAD_TYPE_PS4 || type==SDL_GAMEPAD_TYPE_PS5;
    if (keyboard_active) device=2;
    else if (g && (pad_active || device<0)) device=playstation ? 0 : 1;
    else if (device<0) device=2;
    const int look=bbgpu_mouse_look_enabled()!=0;
    if (device!=prompt_device || (device!=2 && type!=prompt_pad_type) || look!=prompt_look) {
        prompt_device=device; prompt_look=look;
        if (device!=2) prompt_pad_type=type;
        ++prompt_generation;
    }
#ifdef __APPLE__
    BbGpuShared *s=pad_shared ? (BbGpuShared *)pad_shared() : NULL;
    if (s && prompt_published!=prompt_generation) {
        static BbGpuPrompts set; /* 1.3 KiB: not on the game's stack */
        prompt_fill(&set);
        bbgpu_prompts_publish(&s->prompts,&set);
        prompt_published=prompt_generation;
    }
#endif
}

static void sample_devices(PadData *d) {
    memset(d,0,sizeof(*d));
    d->left_x=d->left_y=d->right_x=d->right_y=128;
    d->orientation[3]=1.0f;
    d->connected=1; d->connected_count=connected_count ? connected_count : 1;
    d->timestamp=now_us();
    SDL_Gamepad *g=current_gamepad();
    if (bbgpu_overlay_captures_input()) return; /* settings menu open: neutral input */
    const bool *k=bbgpu_split_keyboard(); /* the window is in the GPU host (split) */
    if (!k) k=SDL_WasInit(SDL_INIT_VIDEO) ? SDL_GetKeyboardState(NULL) : NULL;
    if (g) {
        static const struct { SDL_GamepadButton sdl; int pad; } map[]={
            {SDL_GAMEPAD_BUTTON_SOUTH,PAD_CROSS}, {SDL_GAMEPAD_BUTTON_EAST,PAD_CIRCLE},
            {SDL_GAMEPAD_BUTTON_WEST,PAD_SQUARE}, {SDL_GAMEPAD_BUTTON_NORTH,PAD_TRIANGLE},
            {SDL_GAMEPAD_BUTTON_LEFT_SHOULDER,PAD_L1}, {SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER,PAD_R1},
            {SDL_GAMEPAD_BUTTON_LEFT_STICK,PAD_L3}, {SDL_GAMEPAD_BUTTON_RIGHT_STICK,PAD_R3},
            {SDL_GAMEPAD_BUTTON_START,PAD_OPTIONS}, {SDL_GAMEPAD_BUTTON_BACK,PAD_SELECT},
            {SDL_GAMEPAD_BUTTON_TOUCHPAD,PAD_TOUCHPAD}, {SDL_GAMEPAD_BUTTON_MISC1,PAD_MISC},
            {SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1,PAD_PADDLE1}, {SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2,PAD_PADDLE2},
            {SDL_GAMEPAD_BUTTON_LEFT_PADDLE1,PAD_PADDLE3}, {SDL_GAMEPAD_BUTTON_LEFT_PADDLE2,PAD_PADDLE4},
            {SDL_GAMEPAD_BUTTON_DPAD_UP,PAD_UP}, {SDL_GAMEPAD_BUTTON_DPAD_DOWN,PAD_DOWN},
            {SDL_GAMEPAD_BUTTON_DPAD_LEFT,PAD_LEFT}, {SDL_GAMEPAD_BUTTON_DPAD_RIGHT,PAD_RIGHT},
        };
        uint32_t held=0;
        for (size_t i=0;i<sizeof(map)/sizeof(*map);++i) if (SDL_GetGamepadButton(g,map[i].sdl)) held|=1u<<map[i].pad;
        d->left_x=axis(SDL_GetGamepadAxis(g,SDL_GAMEPAD_AXIS_LEFTX)); d->left_y=axis(SDL_GetGamepadAxis(g,SDL_GAMEPAD_AXIS_LEFTY));
        d->right_x=axis(SDL_GetGamepadAxis(g,SDL_GAMEPAD_AXIS_RIGHTX)); d->right_y=axis(SDL_GetGamepadAxis(g,SDL_GAMEPAD_AXIS_RIGHTY));
        remap_buttons(d,held,trigger(SDL_GetGamepadAxis(g,SDL_GAMEPAD_AXIS_LEFT_TRIGGER)),
                      trigger(SDL_GetGamepadAxis(g,SDL_GAMEPAD_AXIS_RIGHT_TRIGGER)));
        if (SDL_GetNumGamepadTouchpads(g)>0) {
            const int fingers=SDL_GetNumGamepadTouchpadFingers(g,0);
            for (int finger=0;finger<fingers && d->touch_count<2;++finger) {
                bool down=false;
                float x=0, y=0;
                if (SDL_GetGamepadTouchpadFinger(g,0,finger,&down,&x,&y,NULL) && down) {
                    d->touches[d->touch_count++]=(PadTouch){.x=touch_axis(x,1919),
                        .y=touch_axis(y,942),.id=(uint8_t)finger};
                }
            }
        }
        // Back/Select on pads without a touch surface is a left-side click.
        if ((d->buttons & BTN_TOUCHPAD) && !d->touch_count) touch_click(d,0);
    }
    const int pad_active=g && (d->buttons || d->l2>30 || d->r2>30 || d->touch_count || stick_moved(d->left_x) ||
                               stick_moved(d->left_y) || stick_moved(d->right_x) || stick_moved(d->right_y));
    /* The keyboard works beside a controller: its buttons add to the pad's, and the movement /
     * camera keys take over a stick only while held (with a controller connected the keyboard used
     * to be ignored, so Enter could not press Play Offline). While Cmd is held the keys are Mac
     * shortcuts (Cmd+Q, Cmd+, ...), not game input: Cmd+Q drank a Blood Vial. What each key and
     * mouse button does comes from bbport.ini bind_ keys (runtime_bindings.c). */
    if (k && (k[SDL_SCANCODE_LGUI] || k[SDL_SCANCODE_RGUI])) k=NULL;
    uint64_t wheel_actions=0;
    const uint32_t mouse=mouse_inputs(k,&wheel_actions);
    if (!k && !mouse && !wheel_actions) { dodge_was_down=0; prompt_note(g,pad_active,mouse_moving()); return; }
    const uint64_t held=runtime_bindings_held(k,mouse)|wheel_actions;
    prompt_note(g,pad_active,held!=0 || mouse_moving());
    const int menu=game_menu_active();
    const int choice=menu && runtime_menu_pointer_choice(); /* the title before its list: not yet */
#define HELD(a) ((held>>(a))&1)
    /* A menu opened by Esc must not close again while that same Esc is held. Q consumed as
     * Back must not turn into lock-on if the menu disappears before the key is released. */
    static int options_latched,back_consumed;
    if (!HELD(BIND_OPTIONS)) options_latched=0;
    else if (!menu) options_latched=1;
    if (!HELD(BIND_MENU_BACK)) back_consumed=0;
    else if (menu) back_consumed=1;
    static const struct { int action; uint32_t ps; } buttons[]={
        {BIND_CROSS,BTN_CROSS}, {BIND_CIRCLE,BTN_CIRCLE}, {BIND_SQUARE,BTN_SQUARE}, {BIND_TRIANGLE,BTN_TRIANGLE},
        {BIND_R1,BTN_R1}, {BIND_R2,BTN_R2}, {BIND_L1,BTN_L1}, {BIND_L2,BTN_L2}, {BIND_R3,BTN_R3}, {BIND_L3,BTN_L3},
        {BIND_DPAD_UP,BTN_UP}, {BIND_DPAD_DOWN,BTN_DOWN}, {BIND_DPAD_LEFT,BTN_LEFT}, {BIND_DPAD_RIGHT,BTN_RIGHT},
        {BIND_OPTIONS,BTN_OPTIONS},
    };
    for (size_t i=0;i<sizeof(buttons)/sizeof(*buttons);++i) {
        const int action=buttons[i].action;
        if (!HELD(action) || (action==BIND_R3 && back_consumed)) continue;
        d->buttons|=action==BIND_OPTIONS && menu && !options_latched ? BTN_CIRCLE : buttons[i].ps;
    }
    if (menu) {
        dodge_pulse_until=jump_start_us=0; dodge_was_down=HELD(BIND_DODGE);
        if (HELD(BIND_MENU_BACK) || HELD(BIND_DODGE) || HELD(BIND_SPRINT)) d->buttons|=BTN_CIRCLE;
        /* The movement keys are the menu's arrows while there is something to move through (a
         * list with several items has the input focus): the game's menus follow the d-pad only,
         * the stick walks the hunter even then. Opposite keys cancel, as on a stick. */
        if (choice) {
            if (HELD(BIND_MOVE_FORWARD) && !HELD(BIND_MOVE_BACK)) d->buttons|=BTN_UP;
            if (HELD(BIND_MOVE_BACK) && !HELD(BIND_MOVE_FORWARD)) d->buttons|=BTN_DOWN;
            if (HELD(BIND_MOVE_LEFT) && !HELD(BIND_MOVE_RIGHT)) d->buttons|=BTN_LEFT;
            if (HELD(BIND_MOVE_RIGHT) && !HELD(BIND_MOVE_LEFT)) d->buttons|=BTN_RIGHT;
        }
    } else if (keyboard_circle(HELD(BIND_DODGE),HELD(BIND_SPRINT),d->timestamp)) d->buttons|=BTN_CIRCLE;
    if (HELD(BIND_TOUCH_LEFT)) touch_click(d,0);
    if (HELD(BIND_TOUCH_RIGHT)) touch_click(d,1);
    if (HELD(BIND_L2)) d->l2=255;
    if (HELD(BIND_R2)) d->r2=255;
    /* Walk: the movement keys push the stick half way (full deflection always runs). */
    const int reach=HELD(BIND_WALK) ? 64 : 128;
    if (!choice && (HELD(BIND_MOVE_LEFT) || HELD(BIND_MOVE_RIGHT)))
        d->left_x=key_axis(HELD(BIND_MOVE_LEFT),HELD(BIND_MOVE_RIGHT),reach);
    if (!choice && (HELD(BIND_MOVE_FORWARD) || HELD(BIND_MOVE_BACK)))
        d->left_y=key_axis(HELD(BIND_MOVE_FORWARD),HELD(BIND_MOVE_BACK),reach);
    if (mouse_look_active() && runtime_bindings_camera_keys_dpad()) {
        /* The mouse has the camera, so the camera keys are the d-pad: menus scroll with them and
         * in game they switch weapons, tools and quick items. */
        if (HELD(BIND_CAMERA_UP)) d->buttons|=BTN_UP;
        if (HELD(BIND_CAMERA_DOWN)) d->buttons|=BTN_DOWN;
        if (HELD(BIND_CAMERA_LEFT)) d->buttons|=BTN_LEFT;
        if (HELD(BIND_CAMERA_RIGHT)) d->buttons|=BTN_RIGHT;
        return;
    }
    if (HELD(BIND_CAMERA_LEFT) || HELD(BIND_CAMERA_RIGHT))
        d->right_x=key_axis(HELD(BIND_CAMERA_LEFT),HELD(BIND_CAMERA_RIGHT),128);
    if (HELD(BIND_CAMERA_UP) || HELD(BIND_CAMERA_DOWN))
        d->right_y=key_axis(HELD(BIND_CAMERA_UP),HELD(BIND_CAMERA_DOWN),128);
#undef HELD
}

/* Mouse look: the host window publishes the mouse's speed while it holds the cursor
 * (gpu/shim/bbport_mouse.cpp). The speed sets the right stick's deflection, full at
 * MOUSE_FULL_SPEED points/s (the sensitivity is applied by the host), and never below
 * MOUSE_MIN_DEFLECTION so slow moves still clear the game's stick dead zone. The stick is only
 * taken over while the mouse moves; gamepad and keyboard keep it otherwise. */
#define MOUSE_FULL_SPEED 450.0
#define MOUSE_MIN_DEFLECTION 24.0
/* Kept outside the platform-specific shared-memory reader so the response can be verified
 * with real speeds, diagonals, stopped input and the full-stick ceiling. */
static inline void mouse_stick(double vx,double vy,uint8_t *x,uint8_t *y) {
    const double speed=hypot(vx,vy);
    if (!isfinite(speed) || speed<1.0) return;
    const double fraction=fmin(speed/MOUSE_FULL_SPEED,1.0);
    const double deflection=MOUSE_MIN_DEFLECTION+(127.0-MOUSE_MIN_DEFLECTION)*fraction;
    *x=(uint8_t)lround(128.0+vx/speed*deflection);
    *y=(uint8_t)lround(128.0+vy/speed*deflection);
}
/* The GPU process's shared control block: gpu_process.c registers its accessor when it starts.
 * Without one (the unit tests link the runtime without gpu_process.c) there is no mouse. */
void runtime_pad_set_shared(void *(*shared)(void)) {
    pad_shared=shared; runtime_mouse_set_shared(shared); runtime_menu_pointer_set_shared(shared);
}
/* Mouse look is on when the Mouse camera setting is (bbport.ini mouse_look, read at start; the
 * launcher sets it) or while the host window holds the cursor (switched on in the in-game menu).
 * The arrow keys are then the d-pad, from the title screen on. */
static int mouse_look_active(void) {
    if (bbgpu_mouse_look_enabled()) return 1;
#ifdef __APPLE__
    const BbGpuShared *s=pad_shared ? (const BbGpuShared *)pad_shared() : NULL;
    return s && __atomic_load_n(&s->mouse_captured,__ATOMIC_ACQUIRE);
#else
    return 0;
#endif
}
/* Scroll is a live gesture, never a queued macro. Coalesce a burst into a bounded press;
 * ignore extra same-direction steps until release, and discard them instead of replaying them.
 * At rest no action can outlive the last pulse's 35 ms press. */
#define WHEEL_PRESS_US 35000u
#define WHEEL_RELEASE_US 35000u
typedef struct {
    uint32_t serial,epoch;
    int primed,direction,started;
    uint64_t press_until,release_until,actions;
} WheelPulse;
static void wheel_pulse(WheelPulse *pulse,const BbGpuMouseSample *input,uint64_t now,int enabled) {
    pulse->started=0;
    if (!enabled || !input->captured || !pulse->primed || pulse->epoch!=input->epoch) {
        *pulse=(WheelPulse){.serial=input->wheel_serial,.epoch=input->epoch,.primed=1};
        return;
    }
    if (pulse->direction && now>=pulse->press_until) {
        pulse->direction=0; pulse->actions=0;
        pulse->release_until=pulse->press_until+WHEEL_RELEASE_US;
    }
    const int fresh=pulse->serial!=input->wheel_serial;
    pulse->serial=input->wheel_serial; /* consume every event, including coalesced ones */
    const uint64_t event_us=input->wheel_ns/1000u;
    if (!fresh || !input->wheel_direction || now<event_us || now-event_us>100000u) return;
    if ((pulse->direction && pulse->direction!=input->wheel_direction) ||
        (!pulse->direction && now>=pulse->release_until)) {
        pulse->direction=input->wheel_direction; pulse->started=1;
        pulse->press_until=now+WHEEL_PRESS_US;
    }
}
static uint32_t mouse_inputs(const bool *keys,uint64_t *wheel_actions) {
    *wheel_actions=0;
#ifdef __APPLE__
    const BbGpuShared *s=pad_shared ? (const BbGpuShared *)pad_shared() : NULL;
    static WheelPulse pulse;
    static BbGpuMouseSample input;
    if (!s) return 0;
    BbGpuMouseSample next;
    if (bbgpu_mouse_read(&s->mouse_input,&next)) input=next;
    const int enabled=__atomic_load_n(&s->mouse_captured,__ATOMIC_ACQUIRE) && !bbgpu_overlay_captures_input();
    wheel_pulse(&pulse,&input,now_us(),enabled && runtime_bindings_use_wheel());
    if (pulse.started) pulse.actions=runtime_bindings_wheel_actions(keys,
        pulse.direction>0 ? BIND_WHEEL_UP_MASK : BIND_WHEEL_DOWN_MASK);
    if (!enabled) return 0;
    if (pulse.direction) *wheel_actions=pulse.actions;
    const uint32_t mouse=__atomic_load_n(&s->mouse_buttons,__ATOMIC_RELAXED) & 0xffu;
    return mouse;
#else
    (void)keys;
    return 0;
#endif
}
/* The mouse turns the camera right now (the window holds it): the keyboard and mouse are in use. */
static int mouse_moving(void) {
#ifdef __APPLE__
    const BbGpuShared *s=pad_shared ? (const BbGpuShared *)pad_shared() : NULL;
    return s && __atomic_load_n(&s->mouse_captured,__ATOMIC_ACQUIRE) &&
        (__atomic_load_n(&s->mouse_vx,__ATOMIC_RELAXED) || __atomic_load_n(&s->mouse_vy,__ATOMIC_RELAXED));
#else
    return 0;
#endif
}
static void add_mouse(PadData *d) {
#ifdef __APPLE__
    const BbGpuShared *s=pad_shared ? (const BbGpuShared *)pad_shared() : NULL;
    if (!s || !__atomic_load_n(&s->mouse_captured,__ATOMIC_ACQUIRE) || bbgpu_overlay_captures_input()) return;
    if (game_menu_active() || runtime_mouse_direct_active()) return;
    const double vx=__atomic_load_n(&s->mouse_vx,__ATOMIC_RELAXED);
    const double vy=__atomic_load_n(&s->mouse_vy,__ATOMIC_RELAXED);
    mouse_stick(vx,vy,&d->right_x,&d->right_y);
#else
    (void)d;
#endif
}

static void sample_host(PadData *d) {
    sample_devices(d);
    add_mouse(d);
}

/* BB_PAD_FILE=<file>: scripted input for automated runs. The file holds whitespace-separated
 * tokens, re-read when it changes: button names (cross circle square triangle l1 r1 l2 r2 l3 r3
 * options touchpad touchpad_left touchpad_right up down left right) are held while listed;
 * touchpad defaults to a left-side click; lx= ly= rx= ry= (0..255) override
 * the sticks. An empty file releases everything. */
static struct { uint32_t buttons; int stick[4]; int touch_side; } injected={0,{-1,-1,-1,-1},-1};
static int replay_armed;      /* 1 while a BB_PAD_REPLAY recording plays, 2 once it ended */
static uint64_t replay_start; /* 0: (re)start at the next sample */
static void read_inject(void) {
    static const char *path; static int checked; static uint64_t last_check;
    static struct timespec mtime;
    if (!checked) { path=getenv("BB_PAD_FILE"); checked=1; }
    if (!path || !*path) return;
    uint64_t now=now_us();
    if (now-last_check<20000) return;
    last_check=now;
    struct stat st;
    if (stat(path,&st)!=0) return;
#ifdef __APPLE__
#define st_mtim st_mtimespec
#endif
    if (st.st_mtim.tv_sec==mtime.tv_sec && st.st_mtim.tv_nsec==mtime.tv_nsec) return;
    mtime=st.st_mtim;
    FILE *f=fopen(path,"r");
    if (!f) return;
    static const struct { const char *name; uint32_t ps; } names[]={
        {"cross",BTN_CROSS}, {"circle",BTN_CIRCLE}, {"square",BTN_SQUARE}, {"triangle",BTN_TRIANGLE},
        {"l1",BTN_L1}, {"r1",BTN_R1}, {"l2",BTN_L2}, {"r2",BTN_R2}, {"l3",BTN_L3}, {"r3",BTN_R3},
        {"options",BTN_OPTIONS}, {"touchpad",BTN_TOUCHPAD},
        {"up",BTN_UP}, {"down",BTN_DOWN}, {"left",BTN_LEFT}, {"right",BTN_RIGHT},
    };
    static const char *sticks[]={"lx=","ly=","rx=","ry="};
    injected.buttons=0;
    injected.touch_side=-1;
    for (int i=0;i<4;++i) injected.stick[i]=-1;
    char token[64];
    while (fscanf(f,"%63s",token)==1) {
        if (!strcmp(token,"replay") && replay_armed!=1) { replay_armed=1; replay_start=0; } /* BB_PAD_REPLAY */
        if (!strcmp(token,"touchpad_left") || !strcmp(token,"touchpad_right")) {
            injected.buttons|=BTN_TOUCHPAD;
            injected.touch_side=!strcmp(token,"touchpad_right");
        }
        for (size_t i=0;i<sizeof(names)/sizeof(*names);++i) if (!strcmp(token,names[i].name)) injected.buttons|=names[i].ps;
        for (int i=0;i<4;++i) if (!strncmp(token,sticks[i],3)) { int v=atoi(token+3); injected.stick[i]=v<0 ? 0 : v>255 ? 255 : v; }
    }
    fclose(f);
    printf("Runtime: pad file: buttons 0x%x sticks %d %d %d %d\n",injected.buttons,
           injected.stick[0],injected.stick[1],injected.stick[2],injected.stick[3]);
}
/* BB_PAD_RECORD=<file>: F9 starts and stops recording the pad state (gamepad or keyboard) with
 * the time since F9; BB_PAD_REPLAY=<file> plays such a recording back, started by the token
 * "replay" in BB_PAD_FILE (scripted tests repeat a route the player ran once). Lines: ms buttons
 * lx ly rx ry l2 r2, written when the state changes. */
typedef struct { uint32_t ms, buttons; uint8_t axes[4], l2, r2; } PadSample;
static FILE *record_file;
static uint64_t record_start;
static PadSample record_last;
static void record_sample(const PadData *d) {
    static const char *path; static int checked, f9_was_down;
    if (!checked) { path=getenv("BB_PAD_RECORD"); checked=1; }
    if (!path || !*path || !sdl_ready) return;
    const bool *k=bbgpu_split_keyboard(); /* the window is in the GPU host (split) */
    if (!k) k=SDL_WasInit(SDL_INIT_VIDEO) ? SDL_GetKeyboardState(NULL) : NULL;
    const int f9=k && k[SDL_SCANCODE_F9];
    if (f9 && !f9_was_down) {
        if (record_file) {
            fclose(record_file); record_file=NULL;
            printf("Runtime: pad recording stopped (%s)\n",path);
        } else if ((record_file=fopen(path,"w"))) {
            record_start=now_us();
            memset(&record_last,0xff,sizeof(record_last));
            printf("Runtime: pad recording started (%s, F9 stops)\n",path);
        }
    }
    f9_was_down=f9;
    if (!record_file) return;
    PadSample s={(uint32_t)((now_us()-record_start)/1000),d->buttons,
                 {d->left_x,d->left_y,d->right_x,d->right_y},d->l2,d->r2};
    if (s.buttons==record_last.buttons && !memcmp(s.axes,record_last.axes,4) &&
        s.l2==record_last.l2 && s.r2==record_last.r2) return;
    record_last=s;
    fprintf(record_file,"%u %u %u %u %u %u %u %u\n",s.ms,s.buttons,s.axes[0],s.axes[1],s.axes[2],
            s.axes[3],s.l2,s.r2);
    fflush(record_file);
}
static PadSample *replay; static size_t replay_count, replay_next;
/* Benchmark traversal: the replay's clock is the game's own frames at 60 per second (the fixed
 * timestep), so a Mac below 60 FPS walks the recorded path more slowly, not elsewhere. The game
 * reads the pad once per frame (7815 reads for 7815 frames shown on the route), so the read count
 * is that clock. The GPU host's presented frames lag the game by a varying frame or two: as the
 * clock they let an input land a frame early or late, and the route split into different paths
 * after about a minute. */
static int replay_by_frames;
static uint64_t replay_start_flips;
static size_t replay_start_reads;
static uint32_t replay_ms(void) {
    if (replay_by_frames) return (uint32_t)((reads-replay_start_reads)*1000/60);
    return (uint32_t)((now_us()-replay_start)/1000);
}
static void replay_sample(PadData *d) {
    if (!replay_armed) return;
    if (!replay_start) {
        static int loaded;
        if (!loaded) {
            loaded=1;
            const char *path=getenv("BB_PAD_REPLAY");
            FILE *f=path ? fopen(path,"r") : NULL;
            PadSample s; unsigned v[8]; size_t cap=0;
            while (f && fscanf(f,"%u %u %u %u %u %u %u %u",&v[0],&v[1],&v[2],&v[3],&v[4],&v[5],&v[6],&v[7])==8) {
                s=(PadSample){v[0],v[1],{(uint8_t)v[2],(uint8_t)v[3],(uint8_t)v[4],(uint8_t)v[5]},(uint8_t)v[6],(uint8_t)v[7]};
                if (replay_count==cap && !(replay=realloc(replay,(cap=cap ? cap*2 : 1024)*sizeof(*replay)))) break;
                replay[replay_count++]=s;
            }
            if (f) fclose(f);
            printf("Runtime: pad replay of %zu samples from %s\n",replay_count,path ? path : "(unset)");
        }
        replay_start=now_us();
        replay_start_flips=bbgpu_bench_host_flips();
        replay_start_reads=reads;
        replay_next=0;
    }
    const uint32_t ms=replay_ms();
    while (replay_next<replay_count && replay[replay_next].ms<=ms) ++replay_next;
    if (!replay_next) return;
    if (replay_next==replay_count && ms>replay[replay_count-1].ms+500) {
        if (replay_armed==1) {
            printf("Runtime: pad replay finished (%zu pad reads, %llu frames shown)\n",reads-replay_start_reads,
                   (unsigned long long)(bbgpu_bench_host_flips()-replay_start_flips));
            replay_armed=2;
        }
        return;
    }
    const PadSample *s=&replay[replay_next-1];
    d->buttons=s->buttons;
    d->left_x=s->axes[0]; d->left_y=s->axes[1]; d->right_x=s->axes[2]; d->right_y=s->axes[3];
    d->l2=s->l2; d->r2=s->r2;
}
/* Touches as the DualShock 4 reports them: every finger that goes down gets a new id (1..127, kept
 * while it stays down) and the time since the first one went down. With id 0 and no hold time
 * the game ignored touchpad presses: no gesture menu from the touchpad, Back or Tab, and no
 * reading of notes (from upstream bbport). */
static void touch_ids(PadData *d) {
    static uint8_t next_id=1, ids[2]; static int down[2]; static uint64_t since;
    for (int i=0;i<2;++i) {
        const int now=i<d->touch_count;
        if (now && !down[i]) { ids[i]=next_id; next_id=next_id==127 ? 1 : next_id+1; }
        down[i]=now;
        if (now) d->touches[i].id=ids[i];
    }
    if (!d->touch_count) since=0;
    else if (!since) since=d->timestamp;
    d->touch_held_time=d->touch_count ? (uint32_t)(d->timestamp-since) : 0;
}
/* Benchmark (BB_BENCHMARK=traversal|throughput, docs/DESIGN_BENCHMARK.md): presses Cross until a
 * save starts loading (title, then Continue), waits for gameplay (the GPU host's draws a frame),
 * then measures: traversal replays BB_PAD_REPLAY by frames, throughput stands BB_BENCH_SECONDS.
 * The player's own input is ignored throughout. The GPU host records while the phase is 1. */
static int bench_mode=-1; /* 0 off, 1 traversal, 2 throughput */
static void bench_fail(int *state, const char *why) {
    printf("Benchmark: failed: %s\n",why);
    bbgpu_bench_set_phase(3,why);
    *state=4;
}
static void bench_sample(PadData *d) {
    if (bench_mode<0) {
        const char *m=getenv("BB_BENCHMARK");
        bench_mode=!m ? 0 : !strcmp(m,"traversal") ? 1 : !strcmp(m,"throughput") ? 2 : 0;
        if (bench_mode) printf("Runtime: benchmark (%s): pressing Cross until a save loads\n",m);
    }
    if (!bench_mode) return;
    d->buttons=0;
    d->left_x=d->left_y=d->right_x=d->right_y=128;
    d->l2=d->r2=0;
    d->touch_count=0;
    static int state; /* 0 title and menu, 1 loading, 2 settling, 3 measuring, 4 finished */
    static uint64_t started, since, pressed, gameplay, heavy;
    const uint64_t now=now_us();
    if (!started) started=since=now;
    if (state<3 && now-started>150000000) { bench_fail(&state,"no gameplay 150 s after the start"); return; }
    if (state==3 && now-since>600000000) { bench_fail(&state,"the pass took over 10 minutes"); return; }
    switch (state) {
    case 0:
        if (runtime_bench_map_opened) { state=1; since=now; puts("Runtime: benchmark: loading"); break; }
        if (now-pressed>=2500000) pressed=now;
        if (now-pressed<300000) d->buttons|=BTN_CROSS;
        break;
    case 1: /* gameplay: heavy frames (over 1000 draws) for 2 s, never a second without one;
               uncapped, light frames come in between */
        if (bbgpu_bench_frame_draws()>1000) { heavy=now; if (!gameplay) gameplay=now; }
        else if (now-heavy>1000000) gameplay=0;
        if (gameplay && now-gameplay>=2000000) { state=2; since=now; }
        break;
    case 2:
        if (now-since<3000000) break;
        state=3; since=now;
        if (bench_mode==1) { replay_by_frames=1; replay_armed=1; replay_start=0; }
        printf("Runtime: benchmark: measuring\n");
        bbgpu_bench_set_phase(1,NULL);
        break;
    case 3: {
        static double seconds;
        if (!seconds) { const char *e=getenv("BB_BENCH_SECONDS"); seconds=e && atof(e)>0 ? atof(e) : 30; }
        const int over=bench_mode==1 ? replay_armed==2 : now-since>=(uint64_t)(seconds*1e6);
        if (over) { state=4; puts("Runtime: benchmark: done"); bbgpu_bench_set_phase(2,NULL); }
        break;
    }
    default: break;
    }
}
static void sample(PadData *d) {
    sample_host(d);
    if (bbgpu_overlay_captures_input()) return;
    bench_sample(d);
    record_sample(d);
    read_inject();
    replay_sample(d);
    d->buttons|=injected.buttons;
    if (injected.touch_side>=0) touch_click(d,injected.touch_side);
    else if ((d->buttons & BTN_TOUCHPAD) && !d->touch_count) touch_click(d,0);
    if (injected.buttons & BTN_L2) d->l2=255;
    if (injected.buttons & BTN_R2) d->r2=255;
    uint8_t *axes[4]={&d->left_x,&d->left_y,&d->right_x,&d->right_y};
    for (int i=0;i<4;++i) if (injected.stick[i]>=0) *axes[i]=(uint8_t)injected.stick[i];
    touch_ids(d);
    runtime_menu_pointer_pad(&d->buttons,d->left_x,d->left_y);
    runtime_mac_settings_pad(&d->buttons);
}

static ABI int32_t pad_init(void) { host_lock(&lock); initialized=1; host_unlock(&lock); return 0; }
static ABI int32_t pad_open(int32_t user, int32_t type, int32_t index, const void *param) {
    (void)param;
    if (!initialized) return ERR_NOT_INITIALIZED;
    if (user!=1) return ERR_INVALID_ARG;
    if (type!=0 && type!=2) return ERR_INVALID_ARG; /* standard / special port */
    if (index) return ERR_INVALID_ARG;
    host_lock(&lock);
    int already=opened; opened=1;
    host_unlock(&lock);
    if (already) return ERR_ALREADY_OPENED;
    puts("Runtime: pad opened for user 1 (SDL gamepad or keyboard)");
    return PAD_HANDLE;
}
static ABI int32_t pad_close(int32_t handle) {
    if (handle!=PAD_HANDLE || !opened) return ERR_INVALID_HANDLE;
    opened=0; return 0;
}
static ABI int32_t pad_read_state(int32_t handle, PadData *data) {
    if (handle!=PAD_HANDLE || !opened) return ERR_INVALID_HANDLE;
    if (!data) return ERR_INVALID_ARG;
    host_lock(&lock);
    sample(data); ++reads;
    host_unlock(&lock);
    return 0;
}
/* Buffered read: the port samples once per call, so one entry is returned. */
static ABI int32_t pad_read(int32_t handle, PadData *data, int32_t count) {
    if (handle!=PAD_HANDLE || !opened) return ERR_INVALID_HANDLE;
    if (!data || count<1 || count>64) return ERR_INVALID_ARG;
    pad_read_state(handle,data);
    return 1;
}
static ABI int32_t pad_info(int32_t handle, ControllerInfo *info) {
    if (handle!=PAD_HANDLE || !opened) return ERR_INVALID_HANDLE;
    if (!info) return ERR_INVALID_ARG;
    memset(info,0,sizeof(*info));
    info->pixel_density=44.86f; info->resolution_x=1920; info->resolution_y=943;
    info->dead_zone_left=info->dead_zone_right=2;
    info->connection_type=0; info->connected=1; info->device_class=0;
    host_lock(&lock);
    current_gamepad();
    info->connected_count=connected_count ? connected_count : 1;
    host_unlock(&lock);
    return 0;
}
static ABI int32_t pad_vibration(int32_t handle, const uint8_t *param) {
    if (handle!=PAD_HANDLE || !opened) return ERR_INVALID_HANDLE;
    if (!param) return ERR_INVALID_ARG;
    host_lock(&lock);
    SDL_Gamepad *g=current_gamepad();
    if (g) SDL_RumbleGamepad(g,(uint16_t)(param[0]*257),(uint16_t)(param[1]*257),1000);
    host_unlock(&lock);
    return 0;
}
static ABI int32_t pad_ok_handle(int32_t handle) { return handle==PAD_HANDLE && opened ? 0 : ERR_INVALID_HANDLE; }
static ABI int32_t pad_ok_handle_flag(int32_t handle, uint8_t flag) { (void)flag; return pad_ok_handle(handle); }

static const RuntimeExport exports[]={
    {"scePadInit",pad_init}, {"scePadOpen",pad_open}, {"scePadClose",pad_close},
    {"scePadReadState",pad_read_state}, {"scePadRead",pad_read},
    {"scePadGetControllerInformation",pad_info}, {"scePadSetVibration",pad_vibration},
    {"scePadResetOrientation",pad_ok_handle},
    {"scePadSetAngularVelocityDeadbandState",pad_ok_handle_flag}, {"scePadSetTiltCorrectionState",pad_ok_handle_flag},
    {"scePadSetMotionSensorState",pad_ok_handle_flag},
};
uintptr_t runtime_pad_resolve(const char *name) { return RUNTIME_LOOKUP(exports,name); }
void runtime_pad_report(void) { printf("Runtime: pad reads=%zu, gamepad=%s\n",reads,gamepad ? SDL_GetGamepadName(gamepad) : "none"); }
