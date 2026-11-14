// SPDX-FileCopyrightText: Copyright 2026 Mark Hall
// SPDX-License-Identifier: GPL-2.0-or-later
/* Keyboard, mouse and controller bindings (runtime_bindings.h). */
#include "runtime_bindings.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <SDL3/SDL.h>

/* Inputs: SDL scancodes below INPUT_MOUSE, then the mouse buttons (SDL_BUTTON_LEFT .. X2) and the
 * wheel. 0 = an empty slot (SDL_SCANCODE_UNKNOWN). */
enum { INPUT_MOUSE=0x200, INPUT_WHEEL_UP=0x210, INPUT_WHEEL_DOWN };
static const struct { const char *name; uint16_t input; } mouse_names[]={
    {"Mouse Left",INPUT_MOUSE+SDL_BUTTON_LEFT}, {"Mouse Right",INPUT_MOUSE+SDL_BUTTON_RIGHT},
    {"Mouse Middle",INPUT_MOUSE+SDL_BUTTON_MIDDLE}, {"Mouse Back",INPUT_MOUSE+SDL_BUTTON_X1},
    {"Mouse Forward",INPUT_MOUSE+SDL_BUTTON_X2},
    {"Wheel Up",INPUT_WHEEL_UP}, {"Wheel Down",INPUT_WHEEL_DOWN},
};

/* Dark Souls III's default PC controls, adapted to Bloodborne: Space is the real Circle
 * button (tap to dodge, hold to run), E interacts, Q locks on / goes back in menus, F transforms.
 * V uses a blood vial so R remains the quick item. Right click also fires: Bloodborne has no
 * universal shield action. Optional separate dodge/run bindings remain available. */
static const struct { const char *key, *defaults; } actions[BIND_COUNT]={
    [BIND_MOVE_FORWARD]={"bind_move_forward","W"}, [BIND_MOVE_BACK]={"bind_move_back","S"},
    [BIND_MOVE_LEFT]={"bind_move_left","A"}, [BIND_MOVE_RIGHT]={"bind_move_right","D"},
    [BIND_WALK]={"bind_walk","Left Alt"},
    [BIND_DODGE]={"bind_dodge",""}, [BIND_SPRINT]={"bind_sprint",""},
    [BIND_CAMERA_UP]={"bind_camera_up","I"}, [BIND_CAMERA_DOWN]={"bind_camera_down","K"},
    [BIND_CAMERA_LEFT]={"bind_camera_left","J"}, [BIND_CAMERA_RIGHT]={"bind_camera_right","L"},
    [BIND_CROSS]={"bind_cross","E,Return,Keypad Enter"}, [BIND_CIRCLE]={"bind_circle","Space"},
    [BIND_SQUARE]={"bind_square","R,Tab"}, [BIND_TRIANGLE]={"bind_triangle","V"},
    [BIND_R1]={"bind_r1","Mouse Left"}, [BIND_R2]={"bind_r2","Left Shift+Mouse Left,Mouse Back"},
    [BIND_L1]={"bind_l1","F,Mouse Forward"}, [BIND_L2]={"bind_l2","Left Ctrl,Left Shift+Mouse Right,Mouse Right"},
    [BIND_R3]={"bind_r3","Q,Mouse Middle"}, [BIND_L3]={"bind_l3","X"},
    [BIND_DPAD_UP]={"bind_dpad_up","3,Up"}, [BIND_DPAD_DOWN]={"bind_dpad_down","4,Down,Wheel Down"},
    [BIND_DPAD_LEFT]={"bind_dpad_left","1,Left,Left Shift+Wheel Down"}, [BIND_DPAD_RIGHT]={"bind_dpad_right","2,Right,Wheel Up"},
    [BIND_OPTIONS]={"bind_options","Escape"},
    [BIND_TOUCH_LEFT]={"bind_touch_left","G"}, [BIND_TOUCH_RIGHT]={"bind_touch_right","Backspace"},
    [BIND_MENU_BACK]={"bind_menu_back","Q"},
};

static const char *const pad_names[PAD_BUTTON_COUNT]={
    [PAD_CROSS]="Cross", [PAD_CIRCLE]="Circle", [PAD_SQUARE]="Square", [PAD_TRIANGLE]="Triangle",
    [PAD_L1]="L1", [PAD_R1]="R1", [PAD_L2]="L2", [PAD_R2]="R2", [PAD_L3]="L3", [PAD_R3]="R3",
    [PAD_OPTIONS]="Options", [PAD_TOUCHPAD]="Touchpad", [PAD_UP]="D-pad Up", [PAD_DOWN]="D-pad Down",
    [PAD_LEFT]="D-pad Left", [PAD_RIGHT]="D-pad Right", [PAD_SELECT]="Select",
    [PAD_PADDLE1]="Paddle 1", [PAD_PADDLE2]="Paddle 2", [PAD_PADDLE3]="Paddle 3", [PAD_PADDLE4]="Paddle 4",
    [PAD_MISC]="Misc",
};
/* Each button presses its own input, as before controller bindings (0.5.14). */
static const struct { const char *key, *defaults; } pad_inputs[PAD_COUNT]={
    [PAD_CROSS]={"bind_pad_cross","Cross"}, [PAD_CIRCLE]={"bind_pad_circle","Circle"},
    [PAD_SQUARE]={"bind_pad_square","Square"}, [PAD_TRIANGLE]={"bind_pad_triangle","Triangle"},
    [PAD_L1]={"bind_pad_l1","L1"}, [PAD_R1]={"bind_pad_r1","R1"}, [PAD_L2]={"bind_pad_l2","L2"},
    [PAD_R2]={"bind_pad_r2","R2"}, [PAD_L3]={"bind_pad_l3","L3"}, [PAD_R3]={"bind_pad_r3","R3"},
    [PAD_OPTIONS]={"bind_pad_options","Options"}, [PAD_TOUCHPAD]={"bind_pad_touchpad","Touchpad,Select"},
    [PAD_UP]={"bind_pad_dpad_up","D-pad Up"}, [PAD_DOWN]={"bind_pad_dpad_down","D-pad Down"},
    [PAD_LEFT]={"bind_pad_dpad_left","D-pad Left"}, [PAD_RIGHT]={"bind_pad_dpad_right","D-pad Right"},
};

/* bound_mod: the input that must be held too (a chord), 0 for a plain binding. */
static uint16_t bound[BIND_COUNT][BIND_SLOTS], bound_mod[BIND_COUNT][BIND_SLOTS];
static int8_t pad_bound[PAD_COUNT][BIND_SLOTS];
static bool camera_keys_dpad, use_wheel, use_chords;
static int loaded;

static uint16_t parse_input(const char *name) {
    for (size_t i=0;i<sizeof(mouse_names)/sizeof(*mouse_names);++i)
        if (!strcasecmp(name,mouse_names[i].name)) return mouse_names[i].input;
    const SDL_Scancode code=SDL_GetScancodeFromName(name);
    if (code==SDL_SCANCODE_LGUI || code==SDL_SCANCODE_RGUI) return 0; /* Cmd is the Mac's */
    return code>SDL_SCANCODE_UNKNOWN && code<INPUT_MOUSE ? (uint16_t)code : 0;
}

/* "Space, Left Shift+Mouse Left" -> slots; names it does not know are logged and skipped. */
static void parse_list(int action, const char *list, int log) {
    memset(bound[action],0,sizeof(bound[action]));
    memset(bound_mod[action],0,sizeof(bound_mod[action]));
    int slot=0;
    for (const char *p=list; *p && slot<BIND_SLOTS;) {
        const char *end=strchr(p,',');
        size_t n=end ? (size_t)(end-p) : strlen(p);
        while (n && isspace((unsigned char)*p)) { ++p; --n; }
        while (n && isspace((unsigned char)p[n-1])) --n;
        if (n && n<64) {
            char name[64];
            memcpy(name,p,n); name[n]=0;
            /* A chord: "<held input>+<input>" ("Keypad +" is one key: nothing after its '+'). */
            uint16_t mod=0;
            const char *key=name;
            char *plus=strchr(name+1,'+');
            if (plus && plus[1]) {
                char *mod_end=plus;
                while (mod_end>name && isspace((unsigned char)mod_end[-1])) --mod_end;
                *mod_end=0;
                key=plus+1;
                while (isspace((unsigned char)*key)) ++key;
                mod=parse_input(name);
                *mod_end='+'; /* the whole name again, for the log */
                if (!mod) key="";
            }
            const uint16_t input=parse_input(key);
            if (input) { bound_mod[action][slot]=mod; bound[action][slot++]=input; }
            else if (log) printf("Runtime: %s: ignored \"%s\" (not a key or mouse button the game can use)\n",actions[action].key,name);
        }
        if (!end) break;
        p=end+1;
    }
}

/* "Cross, Paddle 1" -> slots of a pad input; names it does not know are logged and skipped. */
static void parse_pad_list(int input, const char *list, int log) {
    memset(pad_bound[input],-1,sizeof(pad_bound[input]));
    int slot=0;
    for (const char *p=list; *p && slot<BIND_SLOTS;) {
        const char *end=strchr(p,',');
        size_t n=end ? (size_t)(end-p) : strlen(p);
        while (n && isspace((unsigned char)*p)) { ++p; --n; }
        while (n && isspace((unsigned char)p[n-1])) --n;
        if (n) {
            int button=-1;
            for (int b=0;b<PAD_BUTTON_COUNT && button<0;++b)
                if (strlen(pad_names[b])==n && !strncasecmp(p,pad_names[b],n)) button=b;
            if (button>=0) pad_bound[input][slot++]=(int8_t)button;
            else if (log) printf("Runtime: %s: ignored \"%.*s\" (not a controller button)\n",pad_inputs[input].key,(int)n,p);
        }
        if (!end) break;
        p=end+1;
    }
}

static void trim(char *s) {
    size_t n=strlen(s);
    while (n && isspace((unsigned char)s[n-1])) s[--n]=0;
    size_t lead=strspn(s," \t");
    if (lead) memmove(s,s+lead,strlen(s+lead)+1);
}

void runtime_bindings_load(const char *ini_path) {
    for (int a=0;a<BIND_COUNT;++a) parse_list(a,actions[a].defaults,0);
    for (int i=0;i<PAD_COUNT;++i) parse_pad_list(i,pad_inputs[i].defaults,0);
    camera_keys_dpad=false;
    loaded=1;
    bool circle_configured=false;
    FILE *f=ini_path ? fopen(ini_path,"r") : NULL;
    if (f) {
        char line[512];
        while (fgets(line,sizeof line,f)) {
            char *eq=strchr(line,'=');
            if (line[0]=='#' || !eq) continue;
            *eq=0;
            char *key=line, *value=eq+1;
            trim(key); trim(value);
            if (!strcmp(key,"camera_keys_dpad")) { camera_keys_dpad=atoi(value)!=0; continue; }
            for (int i=0;i<PAD_COUNT;++i) {
                if (strcmp(key,pad_inputs[i].key)) continue;
                parse_pad_list(i,value,1);
                printf("Runtime: %s=%s\n",key,value);
            }
            for (int a=0;a<BIND_COUNT;++a) {
                if (strcmp(key,actions[a].key)) continue;
                if (a==BIND_CIRCLE) circle_configured=true;
                parse_list(a,value,1);
                printf("Runtime: %s=%s\n",key,value);
            }
        }
        fclose(f);
    }
    /* An older explicit Space -> short dodge must not also pick up the new default
     * Space -> held Circle. Explicit Circle bindings still belong to the user. */
    if (!circle_configured) {
        for (int s=0;s<BIND_SLOTS;++s) {
            for (int a=BIND_DODGE;a<=BIND_SPRINT;++a) for (int t=0;t<BIND_SLOTS;++t) {
                if (bound[BIND_CIRCLE][s] && bound[BIND_CIRCLE][s]==bound[a][t] &&
                    bound_mod[BIND_CIRCLE][s]==bound_mod[a][t]) bound[BIND_CIRCLE][s]=0;
            }
        }
    }
    use_wheel=use_chords=false;
    for (int a=0;a<BIND_COUNT;++a)
        for (int s=0;s<BIND_SLOTS;++s) {
            const uint16_t in=bound[a][s], mod=bound_mod[a][s];
            if (in==INPUT_WHEEL_UP || in==INPUT_WHEEL_DOWN || mod==INPUT_WHEEL_UP || mod==INPUT_WHEEL_DOWN) use_wheel=true;
            if (mod) use_chords=true;
        }
}

static void load_once(void) {
    if (!loaded) runtime_bindings_load(getenv("BB_CONFIG"));
}

static bool input_down(uint16_t in, const bool *keys, uint32_t mouse) {
    return !in ? false
        : in<INPUT_MOUSE ? keys && keys[in]
        : in==INPUT_WHEEL_UP ? (mouse & BIND_WHEEL_UP_MASK)!=0
        : in==INPUT_WHEEL_DOWN ? (mouse & BIND_WHEEL_DOWN_MASK)!=0
        : (mouse & SDL_BUTTON_MASK(in-INPUT_MOUSE))!=0;
}

/* Whether a chord on `in` (any action's) has its other input held: the plain bindings of `in` do
 * not count then. */
static bool chord_held(uint16_t in, const bool *keys, uint32_t mouse) {
    for (int a=0;a<BIND_COUNT;++a)
        for (int s=0;s<BIND_SLOTS;++s)
            if (bound[a][s]==in && bound_mod[a][s] && input_down(bound_mod[a][s],keys,mouse)) return true;
    return false;
}

uint64_t runtime_bindings_held(const bool *keys, uint32_t mouse) {
    load_once();
    uint64_t held=0;
    for (int a=0;a<BIND_COUNT;++a) {
        for (int s=0;s<BIND_SLOTS;++s) {
            const uint16_t in=bound[a][s], mod=bound_mod[a][s];
            if (!input_down(in,keys,mouse)) continue;
            if (mod ? !input_down(mod,keys,mouse) : use_chords && chord_held(in,keys,mouse)) continue;
            held|=UINT64_C(1)<<a;
            break;
        }
    }
    return held;
}

/* Capture only the actions driven by this wheel pulse. Modifiers are sampled once, so
 * releasing Shift during the pulse cannot turn a weapon change into an item change. */
uint64_t runtime_bindings_wheel_actions(const bool *keys,uint32_t wheel) {
    load_once();
    uint64_t held=0;
    for (int a=0;a<BIND_COUNT;++a) for (int s=0;s<BIND_SLOTS;++s) {
        const uint16_t in=bound[a][s],mod=bound_mod[a][s];
        if (in!=INPUT_WHEEL_UP && in!=INPUT_WHEEL_DOWN && mod!=INPUT_WHEEL_UP && mod!=INPUT_WHEEL_DOWN) continue;
        if (!input_down(in,keys,wheel) || (mod ? !input_down(mod,keys,wheel) : use_chords && chord_held(in,keys,wheel))) continue;
        held|=UINT64_C(1)<<a;
    }
    return held;
}

/* A key's name for a prompt icon: short (an icon is a key cap), arrows as arrows. */
static void prompt_name(uint16_t in, char *out, size_t size) {
    static const struct { const char *sdl, *label; } names[]={
        {"Left Shift","Shift"}, {"Right Shift","RShift"}, {"Left Ctrl","Ctrl"}, {"Right Ctrl","RCtrl"},
        {"Left Alt","Alt"}, {"Right Alt","RAlt"}, {"Left Option","Alt"}, {"Right Option","RAlt"},
        {"Return","Enter"}, {"Keypad Enter","Enter"},
        {"Backspace","Bksp"}, {"Escape","Esc"}, {"CapsLock","Caps"}, {"Delete","Del"},
        {"Insert","Ins"}, {"PageUp","PgUp"}, {"PageDown","PgDn"}, {"Up","\xe2\x86\x91"},
        {"Down","\xe2\x86\x93"}, {"Left","\xe2\x86\x90"}, {"Right","\xe2\x86\x92"},
    };
    static const struct { uint16_t input; const char *label; } mouse[]={
        {INPUT_MOUSE+SDL_BUTTON_LEFT,"Left"}, {INPUT_MOUSE+SDL_BUTTON_RIGHT,"Right"},
        {INPUT_MOUSE+SDL_BUTTON_MIDDLE,"Middle"}, {INPUT_MOUSE+SDL_BUTTON_X1,"X1"},
        {INPUT_MOUSE+SDL_BUTTON_X2,"X2"}, {INPUT_WHEEL_UP,"Wheel Up"}, {INPUT_WHEEL_DOWN,"Wheel Down"},
    };
    for (size_t i=0;i<sizeof mouse/sizeof *mouse;++i)
        if (mouse[i].input==in) { snprintf(out,size,"%s",mouse[i].label); return; }
    const char *name=SDL_GetScancodeName((SDL_Scancode)in);
    if (!name || !*name) name="?";
    for (size_t i=0;i<sizeof names/sizeof *names;++i)
        if (!strcmp(name,names[i].sdl)) { name=names[i].label; break; }
    if (!strncmp(name,"Keypad ",7)) snprintf(out,size,"Num%s",name+7);
    else snprintf(out,size,"%s",name);
}

int runtime_bindings_prompt(int action, int prefer_mouse, char *label, int size) {
    load_once();
    if (action<0 || action>=BIND_COUNT || !label || size<2) return 0;
    label[0]=0;
    int key=-1,mouse=-1;
    for (int pass=0;pass<2;++pass) /* plain bindings first, then chords */
        for (int s=0;s<BIND_SLOTS;++s) {
            const uint16_t in=bound[action][s];
            if (!in || (bound_mod[action][s]!=0)!=pass) continue;
            if (in>=INPUT_MOUSE) { if (mouse<0) mouse=s; }
            else if (key<0) key=s;
        }
    /* The d-pad moves through the game's menus, where its prompts show: an arrow key says so
     * better than the digit that switches a weapon in play. */
    if (action>=BIND_DPAD_UP && action<=BIND_DPAD_RIGHT)
        for (int s=0;s<BIND_SLOTS;++s) {
            const uint16_t in=bound[action][s];
            if (!bound_mod[action][s] && (in==SDL_SCANCODE_UP || in==SDL_SCANCODE_DOWN ||
                                          in==SDL_SCANCODE_LEFT || in==SDL_SCANCODE_RIGHT)) { key=s; break; }
        }
    const int slot=mouse>=0 && (prefer_mouse || key<0) ? mouse : key;
    if (slot<0) return 0;
    char name[32],held[32]="";
    prompt_name(bound[action][slot],name,sizeof name);
    if (bound_mod[action][slot]) prompt_name(bound_mod[action][slot],held,sizeof held);
    snprintf(label,(size_t)size,"%s%s%s",held,held[0] ? "+" : "",name);
    return slot==mouse ? 'M' : 'K';
}

bool runtime_bindings_camera_keys_dpad(void) { load_once(); return camera_keys_dpad; }
bool runtime_bindings_use_wheel(void) { load_once(); return use_wheel; }
int runtime_bindings_pad(int input, int slot) {
    load_once();
    return input>=0 && input<PAD_COUNT && slot>=0 && slot<BIND_SLOTS ? pad_bound[input][slot] : -1;
}
const char *runtime_bindings_pad_key(int input) { return input>=0 && input<PAD_COUNT ? pad_inputs[input].key : NULL; }
const char *runtime_bindings_pad_default(int input) { return input>=0 && input<PAD_COUNT ? pad_inputs[input].defaults : NULL; }
const char *runtime_bindings_pad_name(int button) { return button>=0 && button<PAD_BUTTON_COUNT ? pad_names[button] : NULL; }
const char *runtime_bindings_key(int action) { return action>=0 && action<BIND_COUNT ? actions[action].key : NULL; }
const char *runtime_bindings_default(int action) { return action>=0 && action<BIND_COUNT ? actions[action].defaults : NULL; }
