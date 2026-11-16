#define _GNU_SOURCE
#include <assert.h>
#include <unistd.h>
#include "../src/runtime_pad.c"

static int capture, mouse_look;
static bool keys[512];
static int use_keys;
int bbgpu_overlay_captures_input(void) { return capture; }
int bbgpu_mouse_look_enabled(void) { return mouse_look; }
const bool *bbgpu_split_keyboard(void) { return use_keys ? keys : NULL; }
uint32_t bbgpu_bench_frame_draws(void) { return 0; }
uint64_t bbgpu_bench_host_flips(void) { return 0; }
void bbgpu_bench_set_phase(uint32_t phase, const char *error) { (void)phase; (void)error; }
volatile int runtime_bench_map_opened;
void runtime_mac_settings_pad(uint32_t *buttons) { (void)buttons; }
uintptr_t runtime_lookup(const RuntimeExport *table, size_t count, const char *name) {
    (void)table; (void)count; (void)name;
    return 0;
}

static void inject(const char *path, const char *tokens) {
    FILE *f=fopen(path,"w");
    assert(f);
    fputs(tokens,f);
    fclose(f);
    usleep(25000);
}

static PadData press(const SDL_Scancode *codes, size_t count) {
    memset(keys,0,sizeof keys);
    for (size_t i=0;i<count;++i) keys[codes[i]]=true;
    PadData data;
    assert(pad_read_state(1,&data)==0);
    return data;
}
#define PRESS(...) press((const SDL_Scancode[]){__VA_ARGS__},sizeof((const SDL_Scancode[]){__VA_ARGS__})/sizeof(SDL_Scancode))

static void write_file(const char *path, const char *text) {
    FILE *f=fopen(path,"w");
    assert(f);
    fputs(text,f);
    fclose(f);
}

/* Bindings (runtime_bindings.c): the defaults (Dark Souls III's PC layout), the dodge, run and
 * jump on Circle, chords, the ini (rebinds, unbinds, ignores what it cannot use), the walk key,
 * camera keys as the d-pad under mouse look when asked. */
static void test_bindings(void) {
    use_keys=1;
    runtime_bindings_load(NULL);
    PadData d=PRESS(SDL_SCANCODE_W);
    assert(d.left_y==0 && d.left_x==128 && d.buttons==0);
    d=PRESS(SDL_SCANCODE_S,SDL_SCANCODE_D);
    assert(d.left_y==255 && d.left_x==255);
    d=PRESS(SDL_SCANCODE_W,SDL_SCANCODE_S);
    assert(d.left_y==127);
    d=PRESS(SDL_SCANCODE_LALT,SDL_SCANCODE_A,SDL_SCANCODE_S);
    assert(d.left_x==64 && d.left_y==192);
    d=PRESS(SDL_SCANCODE_E,SDL_SCANCODE_R,SDL_SCANCODE_V,SDL_SCANCODE_F);
    assert(d.buttons==(BTN_CROSS|BTN_SQUARE|BTN_TRIANGLE|BTN_L1) && d.l2==0 && d.r2==0);
    d=PRESS(SDL_SCANCODE_KP_ENTER,SDL_SCANCODE_ESCAPE,SDL_SCANCODE_3,SDL_SCANCODE_RIGHT);
    assert(d.buttons==(BTN_CROSS|BTN_OPTIONS|BTN_UP|BTN_RIGHT));
    d=PRESS(SDL_SCANCODE_1,SDL_SCANCODE_2,SDL_SCANCODE_4,SDL_SCANCODE_X,SDL_SCANCODE_Q,SDL_SCANCODE_TAB);
    assert(d.buttons==(BTN_LEFT|BTN_RIGHT|BTN_DOWN|BTN_L3|BTN_R3|BTN_SQUARE));
    memset(keys,0,sizeof keys); keys[SDL_SCANCODE_G]=true; /* not pressed through the pad: touch ids are checked below */
    assert(runtime_bindings_held(keys,0)==(UINT64_C(1)<<BIND_TOUCH_LEFT));
    /* IJKL are the camera, with the mouse camera too; the arrow keys are the d-pad. */
    d=PRESS(SDL_SCANCODE_J,SDL_SCANCODE_K);
    assert(d.right_x==0 && d.right_y==255 && d.buttons==0);
    mouse_look=1;
    d=PRESS(SDL_SCANCODE_J,SDL_SCANCODE_K,SDL_SCANCODE_LEFT);
    assert(d.right_x==0 && d.right_y==255 && d.buttons==BTN_LEFT);
    mouse_look=0;
    d=PRESS(SDL_SCANCODE_LGUI,SDL_SCANCODE_Q);
    assert(d.buttons==0); /* Cmd+Q is the Mac's */

    /* Soul 3's Space is the real button: a tap/release dodges; holding it keeps running.
     * Shift is only a chord modifier now, so a heavy attack cannot also start a sprint. */
    d=PRESS(SDL_SCANCODE_LSHIFT,SDL_SCANCODE_W);
    assert(d.buttons==0);
    d=PRESS(SDL_SCANCODE_W);
    assert(d.buttons==0);
    d=PRESS(SDL_SCANCODE_SPACE);
    assert(d.buttons==BTN_CIRCLE);
    usleep(120000);
    d=PRESS(SDL_SCANCODE_SPACE);
    assert(d.buttons==BTN_CIRCLE); /* still held: keep running */
    d=PRESS(SDL_SCANCODE_W);
    assert(d.buttons==0);
    d=PRESS(SDL_SCANCODE_SPACE,SDL_SCANCODE_W);
    assert(d.buttons==BTN_CIRCLE && d.left_y==0); /* repress while running: native jump input */
    (void)PRESS(SDL_SCANCODE_W);
    /* Custom split dodge/run bindings keep the short dodge and timed jump sequence. */
    dodge_pulse_until=jump_start_us=0; dodge_was_down=0;
    assert(keyboard_circle(1,0,1000000));
    assert(!keyboard_circle(1,0,1120000));
    assert(keyboard_circle(0,1,1200000));
    assert(!keyboard_circle(1,1,1300000));
    assert(keyboard_circle(1,1,1390000));
    assert(!keyboard_circle(0,1,1460000));
    assert(keyboard_circle(0,1,1530000));
    dodge_pulse_until=jump_start_us=0; dodge_was_down=0;

    /* The mouse: left R1, Shift + left R2 alone (the chord hides the plain click), right L2, the
     * side buttons R2 and L1, the wheel the d-pad. */
    const uint64_t r1=UINT64_C(1)<<BIND_R1, r2=UINT64_C(1)<<BIND_R2;
    assert(runtime_bindings_held(NULL,SDL_BUTTON_LMASK|SDL_BUTTON_RMASK)==(r1|(UINT64_C(1)<<BIND_L2)));
    assert(runtime_bindings_held(NULL,SDL_BUTTON_X1MASK|SDL_BUTTON_X2MASK)==(r2|(UINT64_C(1)<<BIND_L1)));
    memset(keys,0,sizeof keys); keys[SDL_SCANCODE_LSHIFT]=true;
    assert(runtime_bindings_held(keys,SDL_BUTTON_LMASK)==r2);
    assert(runtime_bindings_held(keys,0)==0);
    assert(runtime_bindings_held(keys,SDL_BUTTON_RMASK)==(UINT64_C(1)<<BIND_L2));
    assert(runtime_bindings_held(keys,BIND_WHEEL_UP_MASK)==(UINT64_C(1)<<BIND_DPAD_RIGHT));
    assert(runtime_bindings_held(keys,BIND_WHEEL_DOWN_MASK)==(UINT64_C(1)<<BIND_DPAD_LEFT));
    /* Without Shift: previous / next item. With Shift: only the weapon, never an item too. */
    assert(runtime_bindings_held(NULL,BIND_WHEEL_UP_MASK)==(UINT64_C(1)<<BIND_ITEM_PREVIOUS));
    assert(runtime_bindings_held(NULL,BIND_WHEEL_DOWN_MASK)==(UINT64_C(1)<<BIND_DPAD_DOWN));
    assert(runtime_bindings_held(NULL,SDL_BUTTON_MMASK)==(UINT64_C(1)<<BIND_R3));
    assert(runtime_bindings_use_wheel());

    char path[]="/tmp/bbport-bindings-test-XXXXXX";
    int fd=mkstemp(path);
    assert(fd>=0);
    close(fd);
    write_file(path,"bind_dodge=Space\nbind_sprint=Left Shift\n");
    runtime_bindings_load(path);
    assert(runtime_bindings_held(keys,0)==(UINT64_C(1)<<BIND_SPRINT));
    d=PRESS(SDL_SCANCODE_SPACE);
    assert(d.buttons==BTN_CIRCLE);
    usleep(120000);
    d=PRESS(SDL_SCANCODE_SPACE);
    assert(d.buttons==0); /* the new default cannot turn a saved short dodge into sprint */
    (void)PRESS(SDL_SCANCODE_UNKNOWN);
    write_file(path,"# comment\r\nupscaler=metalfx\r\nbind_cross = K , Mouse Left\r\nbind_dpad_down=\r\nbind_dpad_up=3\r\n"
                    "bind_square=Left GUI,Bogus,y,Left GUI+H,Bogus+H\r\nbind_r1=Wheel Up\r\nbind_walk=CapsLock\r\ncamera_keys_dpad=1\r\n"
                    "bind_circle=Left Ctrl + Keypad +, Z\r\nbind_r2=Left Alt+F\r\nbind_l2=F\r\nbind_l1=E\r\nbind_dpad_right=2,Right\r\nbind_item_previous=\r\n");
    runtime_bindings_load(path);
    d=PRESS(SDL_SCANCODE_K);
    assert(d.buttons==BTN_CROSS && d.right_y==255); /* K is Cross now, and still the camera's down */
    d=PRESS(SDL_SCANCODE_F,SDL_SCANCODE_Y);
    assert(d.buttons==(BTN_SQUARE|BTN_L2) && d.l2==255); /* F is no longer Cross; Cmd, Cmd chords and unknown names ignored */
    d=PRESS(SDL_SCANCODE_H);
    assert(d.buttons==0);
    d=PRESS(SDL_SCANCODE_LALT,SDL_SCANCODE_F);
    assert(d.buttons==BTN_R2 && d.r2==255 && d.l2==0); /* the chord, not F's plain binding */
    d=PRESS(SDL_SCANCODE_KP_PLUS);
    assert(d.buttons==0);
    d=PRESS(SDL_SCANCODE_LCTRL,SDL_SCANCODE_KP_PLUS);
    assert(d.buttons==BTN_CIRCLE); /* "Keypad +" is one key, the chord's second */
    d=PRESS(SDL_SCANCODE_Z);
    assert(d.buttons==BTN_CIRCLE); /* the plain Circle button: held */
    d=PRESS(SDL_SCANCODE_4);
    assert(d.buttons==0); /* unbound */
    d=PRESS(SDL_SCANCODE_CAPSLOCK,SDL_SCANCODE_D);
    assert(d.left_x==192);
    mouse_look=1;
    d=PRESS(SDL_SCANCODE_I);
    assert(d.right_y==128 && d.buttons==BTN_UP); /* camera_keys_dpad=1: the camera keys press the d-pad */
    mouse_look=0;
    d=PRESS(SDL_SCANCODE_I);
    assert(d.right_y==0 && d.buttons==0);
    assert(runtime_bindings_held(NULL,SDL_BUTTON_LMASK)==(UINT64_C(1)<<BIND_CROSS));
    assert(runtime_bindings_held(NULL,BIND_WHEEL_UP_MASK)==(UINT64_C(1)<<BIND_R1));
    assert(runtime_bindings_held(NULL,BIND_WHEEL_DOWN_MASK)==0);
    assert(runtime_bindings_use_wheel());
    write_file(path,"bind_dpad_up=3\nbind_dpad_down=4\nbind_dpad_left=Left\nbind_dpad_right=Right\nbind_item_previous=\n");
    runtime_bindings_load(path);
    assert(!runtime_bindings_use_wheel());
    unlink(path);
    runtime_bindings_load(NULL);
    /* Button prompts: what each action is on the keyboard and mouse, short enough for an icon. */
    char label[BBGPU_PROMPT_LABEL];
    assert(runtime_bindings_prompt(BIND_CROSS,0,label,sizeof label)=='K' && !strcmp(label,"E"));
    assert(runtime_bindings_prompt(BIND_CROSS,1,label,sizeof label)=='K' && !strcmp(label,"E"));
    assert(runtime_bindings_prompt(BIND_CIRCLE,1,label,sizeof label)=='K' && !strcmp(label,"Space"));
    assert(runtime_bindings_prompt(BIND_R1,0,label,sizeof label)=='M' && !strcmp(label,"Left"));
    /* A plain binding before a chord; with the hand on the mouse, its button before a key. */
    assert(runtime_bindings_prompt(BIND_R2,0,label,sizeof label)=='M' && !strcmp(label,"X1"));
    assert(runtime_bindings_prompt(BIND_L2,0,label,sizeof label)=='K' && !strcmp(label,"Ctrl"));
    assert(runtime_bindings_prompt(BIND_L2,1,label,sizeof label)=='M' && !strcmp(label,"Right"));
    assert(runtime_bindings_prompt(BIND_OPTIONS,1,label,sizeof label)=='K' && !strcmp(label,"Esc"));
    /* The d-pad's prompts show in menus: an arrow key, not the digit or the wheel. */
    assert(runtime_bindings_prompt(BIND_DPAD_UP,0,label,sizeof label)=='K' && !strcmp(label,"\xe2\x86\x91"));
    assert(runtime_bindings_prompt(BIND_DPAD_DOWN,0,label,sizeof label)=='K' && !strcmp(label,"\xe2\x86\x93"));
    /* The plain wheel scrolls menus too; Shift+Wheel still falls back to an arrow prompt. */
    assert(runtime_bindings_prompt(BIND_DPAD_DOWN,1,label,sizeof label)=='M' && !strcmp(label,"Wheel Down"));
    assert(runtime_bindings_prompt(BIND_DPAD_RIGHT,1,label,sizeof label)=='K' && !strcmp(label,"\xe2\x86\x92"));
    assert(!runtime_bindings_prompt(BIND_DODGE,1,label,sizeof label) && !label[0]); /* unbound */
    write_file(path,"bind_r2=Left Shift+Mouse Left\nbind_triangle=Left Ctrl+V\nbind_cross=Keypad +\n");
    runtime_bindings_load(path);
    assert(runtime_bindings_prompt(BIND_R2,1,label,sizeof label)=='M' && !strcmp(label,"Shift+Left"));
    assert(runtime_bindings_prompt(BIND_TRIANGLE,0,label,sizeof label)=='K' && !strcmp(label,"Ctrl+V"));
    assert(runtime_bindings_prompt(BIND_CROSS,0,label,sizeof label)=='K' && !strcmp(label,"Num+"));
    unlink(path);
    runtime_bindings_load(NULL);
    /* The set the renderer gets: the keyboard's when nothing else was used, by input. */
    prompt_device=-1; prompt_look=-1;
    prompt_note(NULL,0,0);
    static BbGpuPrompts set;
    prompt_fill(&set);
    assert(set.device==2 && set.generation==prompt_generation && !set.mouse_look);
    assert(set.kind[1][BBGPU_PROMPT_CROSS]=='K' && !strcmp(set.label[1][BBGPU_PROMPT_CROSS],"E"));
    assert(set.kind[1][BBGPU_PROMPT_CIRCLE]=='K' && !strcmp(set.label[1][BBGPU_PROMPT_CIRCLE],"Space"));
    assert(set.kind[0][BBGPU_PROMPT_MOVE_LEFT]=='K' && !strcmp(set.label[0][BBGPU_PROMPT_MOVE_LEFT],"A"));
    assert(set.kind[1][BBGPU_PROMPT_R1]=='M' && !strcmp(set.label[1][BBGPU_PROMPT_R1],"Left"));
    const uint32_t before=prompt_generation;
    prompt_note(NULL,0,1); /* still the keyboard: nothing to draw again */
    assert(prompt_generation==before);
    memset(keys,0,sizeof keys);
    use_keys=0;
}

/* Menu aliases are contextual, including the title screen. No speculative pointer can crash
 * input polling, and holding Back across a close cannot turn into a fresh lock-on. */
static void test_menu_keys(void) {
    const size_t size=MENU_FLOW_OFFSET+sizeof(uintptr_t);
    unsigned char *image=calloc(1,size),flow[0xb12]={0};
    assert(image);
    const unsigned char signature[]={0x55,0x48,0x89,0xe5,0x41,0x57};
    memcpy(image+MENU_LIST_UPDATE_OFFSET,signature,sizeof signature);
    uintptr_t ptr=(uintptr_t)flow,menu=1;
    memcpy(image+MENU_FLOW_OFFSET,&ptr,sizeof ptr);
    flow[0xb11]=1;
    /* A list of the game's as its update hook sees it (runtime_menu_pointer.c): +0x70 the item
     * count; the second argument's first byte is nonzero while it has the input focus. */
    unsigned char component[0x2b8]={0},focus=1;
    int32_t items=5;
    memcpy(component+0x70,&items,sizeof items);
#define LIST_UPDATE() runtime_menu_pointer_list_update((uintptr_t)component,(uintptr_t)&focus)
    use_keys=1; runtime_bindings_load(NULL);
    runtime_pad_set_image(image,size-1);
    assert(!menu_flow_address);
    runtime_pad_set_image(image,size);
    assert(menu_flow_address && !game_menu_active());
    PadData d=PRESS(SDL_SCANCODE_W);
    assert(d.left_y==0 && d.buttons==0);
    d=PRESS(SDL_SCANCODE_ESCAPE);
    assert(d.buttons==BTN_OPTIONS);
    memcpy(flow+0xa88,&menu,sizeof menu);
    assert(!game_menu_active()); /* a menu object alone is not a menu to choose in */
    runtime_menu_pointer_hooked();
    LIST_UPDATE();
    assert(game_menu_active());
    d=PRESS(SDL_SCANCODE_ESCAPE);
    assert(d.buttons==BTN_OPTIONS); /* same press that opened the menu */
    (void)PRESS(SDL_SCANCODE_UNKNOWN);
    d=PRESS(SDL_SCANCODE_ESCAPE);
    assert(d.buttons==BTN_CIRCLE);
    /* The movement keys are the menu's arrows, and the hunter stands (once half a second has
     * passed since that Back: presses during a closing list's fade are not the d-pad). */
    (void)PRESS(SDL_SCANCODE_UNKNOWN); menu_back_guard_until=0;
    d=PRESS(SDL_SCANCODE_W,SDL_SCANCODE_D);
    assert(d.buttons==(BTN_UP|BTN_RIGHT) && d.left_x==128 && d.left_y==128);
    d=PRESS(SDL_SCANCODE_A,SDL_SCANCODE_S);
    assert(d.buttons==(BTN_LEFT|BTN_DOWN) && d.left_x==128 && d.left_y==128);
    d=PRESS(SDL_SCANCODE_W,SDL_SCANCODE_S);
    assert(!d.buttons && d.left_y==128);
    d=PRESS(SDL_SCANCODE_E);
    assert(d.buttons==BTN_CROSS);
    d=PRESS(SDL_SCANCODE_Q);
    assert(d.buttons==BTN_CIRCLE); /* not R3 and never a blood vial */
    menu=0; memcpy(flow+0xa88,&menu,sizeof menu);
    d=PRESS(SDL_SCANCODE_Q);
    assert(!d.buttons); /* Back still held after the menu closed */
    (void)PRESS(SDL_SCANCODE_UNKNOWN);
    d=PRESS(SDL_SCANCODE_Q);
    assert(d.buttons==BTN_R3);
    /* Headstone / travel lists: Back + WASD must never expose a d-pad press while closing.
     * Keep each consumed direction neutral through the fade and parent menus until released;
     * a held movement key can immediately walk once the menu has gone. */
    const struct { SDL_Scancode key; uint32_t button; } directions[]={
        {SDL_SCANCODE_A,BTN_LEFT},{SDL_SCANCODE_D,BTN_RIGHT},
        {SDL_SCANCODE_W,BTN_UP},{SDL_SCANCODE_S,BTN_DOWN},
    };
    const SDL_Scancode backs[]={SDL_SCANCODE_Q,SDL_SCANCODE_ESCAPE,SDL_SCANCODE_SPACE};
    for (size_t b=0;b<sizeof backs/sizeof backs[0];++b) {
        for (size_t i=0;i<sizeof directions/sizeof directions[0];++i) {
            (void)PRESS(SDL_SCANCODE_UNKNOWN);
            menu_back_guard_until=0; /* a new list, opened long after the last Back */
            menu=1; memcpy(flow+0xa88,&menu,sizeof menu); LIST_UPDATE();
            d=press(&directions[i].key,1);
            assert(d.buttons==directions[i].button); /* navigation before closing */
            SDL_Scancode together[]={backs[b],directions[i].key};
            d=press(together,2);
            assert(d.buttons==BTN_CIRCLE);
            d=press(&directions[i].key,1); /* Back released, list still fading / parent open */
            assert(!d.buttons && d.left_x==128 && d.left_y==128);
            menu=0; memcpy(flow+0xa88,&menu,sizeof menu);
            d=press(&directions[i].key,1);
            assert(!d.buttons && (d.left_x!=128 || d.left_y!=128)); /* walk, never a d-pad */
            menu=1; memcpy(flow+0xa88,&menu,sizeof menu); LIST_UPDATE();
            d=press(&directions[i].key,1);
            assert(!d.buttons); /* held direction stays consumed even in the next list */
            (void)PRESS(SDL_SCANCODE_UNKNOWN);
            menu_back_guard_until=0; /* half a second after Back: a parent list's own input */
            d=press(&directions[i].key,1);
            assert(d.buttons==directions[i].button); /* a fresh press navigates normally */
            (void)PRESS(SDL_SCANCODE_UNKNOWN);
            (void)press(&backs[b],1); /* Back first, direction pressed on the next sample */
            d=press(together,2);
            assert(d.buttons==BTN_CIRCLE);
            /* A quick tap of Back, then the key while the list fades out (still focused):
             * neither the d-pad nor a step, then a walk once the list has gone. */
            (void)PRESS(SDL_SCANCODE_UNKNOWN);
            menu_back_guard_until=0;
            (void)press(&backs[b],1);
            (void)PRESS(SDL_SCANCODE_UNKNOWN);
            LIST_UPDATE();
            d=press(&directions[i].key,1);
            assert(!d.buttons && d.left_x==128 && d.left_y==128);
            menu=0; memcpy(flow+0xa88,&menu,sizeof menu);
            d=press(&directions[i].key,1);
            assert(!d.buttons && (d.left_x!=128 || d.left_y!=128));
            (void)PRESS(SDL_SCANCODE_UNKNOWN);
            menu_back_guard_until=0;
        }
    }
    (void)PRESS(SDL_SCANCODE_UNKNOWN);
    /* A message with one button (or a pick-up notice): a menu object and a focused list of one
     * item, but the hunter moves and fights on behind it. Holding A there and pressing E pressed
     * the d-pad (weapons switched, blood bullets made): the keys must stay what they are in
     * play. */
    menu=1; memcpy(flow+0xa88,&menu,sizeof menu);
    items=1; memcpy(component+0x70,&items,sizeof items);
    usleep(270000); /* the several-item list's focus has run out */
    LIST_UPDATE();
    assert(!game_menu_active());
    d=PRESS(SDL_SCANCODE_A,SDL_SCANCODE_E);
    assert(d.buttons==BTN_CROSS && d.left_x==0 && d.left_y==128);
    d=PRESS(SDL_SCANCODE_W,SDL_SCANCODE_D);
    assert(!d.buttons && d.left_x==255 && d.left_y==0);
    d=PRESS(SDL_SCANCODE_Q);
    assert(d.buttons==BTN_R3); /* lock-on, not a dodge */
    d=PRESS(SDL_SCANCODE_ESCAPE);
    assert(d.buttons==BTN_OPTIONS);
    (void)PRESS(SDL_SCANCODE_UNKNOWN);
    menu=0; memcpy(flow+0xa88,&menu,sizeof menu);
    items=5; memcpy(component+0x70,&items,sizeof items);
    flow[0xb11]=0;
    assert(game_menu_active()); /* title menu has no active in-game menu object */
    d=PRESS(SDL_SCANCODE_S);
    assert(!d.buttons && d.left_y==255); /* before its list shows there is nothing to move through */
    LIST_UPDATE();
    d=PRESS(SDL_SCANCODE_S);
    assert(d.buttons==BTN_DOWN && d.left_y==128);
    ptr=1; memcpy(image+MENU_FLOW_OFFSET,&ptr,sizeof ptr);
    assert(!game_menu_active()); /* unmapped pointer is harmless */
    ptr=UINTPTR_MAX-8; memcpy(image+MENU_FLOW_OFFSET,&ptr,sizeof ptr);
    assert(!game_menu_active());
    image[MENU_LIST_UPDATE_OFFSET]^=1;
    runtime_pad_set_image(image,size);
    assert(!menu_flow_address);
    runtime_pad_set_image(NULL,0);
#undef LIST_UPDATE
    free(image); use_keys=0; memset(keys,0,sizeof keys);
}

/* Scrolling a menu with the wheel. Hovering waits for the whole scroll, however the pointer
 * moves meanwhile (a nudge of the hand or a move towards the item scrolled to), and comes back
 * on a move once it has settled, or at a click. */
static void test_menu_wheel_hover(void) {
    assert(!runtime_menu_pointer_wheel_holds(0,500,500,0)); /* plain hover */
    assert(runtime_menu_pointer_wheel_holds(1,500,500,0));  /* scrolling */
    assert(runtime_menu_pointer_wheel_holds(1,503,502,0));  /* the hand's nudge */
    assert(runtime_menu_pointer_wheel_holds(1,620,700,0));  /* moved on purpose meanwhile */
    assert(runtime_menu_pointer_wheel_holds(0,620,700,0));  /* settled: no move yet */
    assert(runtime_menu_pointer_wheel_holds(0,623,702,0));  /* a nudge */
    assert(!runtime_menu_pointer_wheel_holds(0,620,710,0)); /* a move: hover again */
    assert(!runtime_menu_pointer_wheel_holds(0,620,711,0));
    assert(runtime_menu_pointer_wheel_holds(1,620,711,0));
    assert(!runtime_menu_pointer_wheel_holds(1,620,711,1)); /* a click picks what it is on */
    assert(!runtime_menu_pointer_wheel_holds(0,621,711,0));

    /* Every step is a d-pad press of 2 pad reads (frames) and a release of 1, never merged. */
    RuntimeMenuWheel w={0};
    int64_t t=1000;
    runtime_menu_wheel_add(&w,1,t);
    runtime_menu_wheel_add(&w,1,t);
    uint32_t seen[9];
    for (int i=0;i<9;++i) seen[i]=runtime_menu_wheel_next(&w,t+=16);
    static const uint32_t two[9]={BTN_DOWN,BTN_DOWN,0,BTN_DOWN,BTN_DOWN,0,0,0,0};
    assert(!memcmp(seen,two,sizeof seen));
    assert(runtime_menu_wheel_scrolling(&w,t) && !runtime_menu_wheel_scrolling(&w,t+400));
    /* At most 2 steps wait; turning back drops the rest the other way. */
    runtime_menu_wheel_add(&w,40,t);
    assert(w.pending==2);
    assert(runtime_menu_wheel_next(&w,t+=16)==BTN_DOWN && w.pending==1);
    runtime_menu_wheel_add(&w,-1,t);
    assert(w.pending==-1);
    assert(runtime_menu_wheel_next(&w,t+=16)==BTN_DOWN); /* the step under way ends first */
    assert(runtime_menu_wheel_next(&w,t+=16)==0);
    assert(runtime_menu_wheel_next(&w,t+=16)==BTN_UP && w.pending==0);
    /* The cursor stops with the wheel: steps 150 ms old are dropped. */
    runtime_menu_wheel_clear(&w);
    runtime_menu_wheel_add(&w,2,t);
    t+=200;
    assert(runtime_menu_wheel_next(&w,t)==0 && w.pending==0);
}

static void test_mouse_response(void) {
    uint8_t x=128,y=128;
    mouse_stick(0,0,&x,&y);
    assert(x==128 && y==128);
    mouse_stick(100,0,&x,&y);
    const uint8_t slow=x;
    assert(slow>152 && slow<255 && y==128);
    mouse_stick(200,0,&x,&y); /* new default 2x sensitivity */
    assert(x>slow && x<255 && y==128);
    mouse_stick(MOUSE_FULL_SPEED,0,&x,&y);
    assert(x==255 && y==128);
    mouse_stick(-100000,0,&x,&y);
    assert(x==1 && y==128);
    mouse_stick(100000,100000,&x,&y);
    assert(x==y && x<255); /* radial cap, not faster along diagonals */
    x=128; y=128;
    mouse_stick(NAN,0,&x,&y); mouse_stick(INFINITY,1,&x,&y);
    assert(x==128 && y==128);
}

static void test_wheel_pulses(void) {
    runtime_bindings_load(NULL);
    WheelPulse pulse={0};
    BbGpuMouseSample input={.epoch=1,.captured=1};
    uint64_t now=1000000;
    wheel_pulse(&pulse,&input,now,1);
    input.wheel_serial+=1000; input.wheel_direction=-1; input.wheel_ns=now*1000;
    wheel_pulse(&pulse,&input,now,1);
    assert(pulse.started && pulse.direction==-1 && pulse.press_until==now+35000);
    bool shift[512]={0}; shift[SDL_SCANCODE_LSHIFT]=true;
    pulse.actions=runtime_bindings_wheel_actions(shift,BIND_WHEEL_DOWN_MASK);
    assert(pulse.actions==(UINT64_C(1)<<BIND_DPAD_LEFT));
    shift[SDL_SCANCODE_LSHIFT]=false;
    for (int i=1;i<=10;++i) {
        now=1000000+(uint64_t)i*2000;
        input.wheel_serial+=500; input.wheel_ns=now*1000;
        wheel_pulse(&pulse,&input,now,1);
        assert(!pulse.started && pulse.direction==-1 && pulse.press_until==1035000);
        assert(pulse.actions==(UINT64_C(1)<<BIND_DPAD_LEFT)); /* modifier is latched */
    }
    wheel_pulse(&pulse,&input,1040000,1);
    assert(!pulse.direction && !pulse.actions);
    wheel_pulse(&pulse,&input,2000000,1);
    assert(!pulse.direction); /* thousands of scroll steps cannot replay after stopping */
    ++input.wheel_serial; input.wheel_ns=2000000*UINT64_C(1000);
    wheel_pulse(&pulse,&input,2000000,1); assert(pulse.direction==-1);
    ++input.wheel_serial; input.wheel_direction=1; input.wheel_ns=2001000*UINT64_C(1000);
    wheel_pulse(&pulse,&input,2001000,1); assert(pulse.started && pulse.direction==1);
    wheel_pulse(&pulse,&input,2002000,0); assert(!pulse.direction);
    wheel_pulse(&pulse,&input,2003000,1); assert(!pulse.direction);
    ++input.wheel_serial; input.wheel_ns=1;
    wheel_pulse(&pulse,&input,2004000,1); assert(!pulse.direction); /* stale event */
    ++input.epoch; input.wheel_ns=2005000*UINT64_C(1000);
    wheel_pulse(&pulse,&input,2005000,1); assert(!pulse.direction);
    assert(runtime_bindings_wheel_actions(shift,BIND_WHEEL_UP_MASK)==(UINT64_C(1)<<BIND_ITEM_PREVIOUS));
    assert(runtime_bindings_wheel_actions(shift,BIND_WHEEL_DOWN_MASK)==(UINT64_C(1)<<BIND_DPAD_DOWN));
    shift[SDL_SCANCODE_LSHIFT]=true;
    assert(runtime_bindings_wheel_actions(shift,BIND_WHEEL_UP_MASK)==(UINT64_C(1)<<BIND_DPAD_RIGHT));
    assert(runtime_bindings_wheel_actions(shift,BIND_WHEEL_DOWN_MASK)==(UINT64_C(1)<<BIND_DPAD_LEFT));
    assert(!(runtime_bindings_wheel_actions(shift,BIND_WHEEL_UP_MASK)&(UINT64_C(1)<<BIND_DPAD_UP)));
}

/* The launcher's tables (launcher/Model/KeyCodes.swift, Bindings.swift) against SDL and
 * runtime_bindings.c: every key name is SDL's own spelling, every action and default the same. */
static void test_launcher_tables(void) {
    FILE *f=fopen("launcher/Model/KeyCodes.swift","r");
    assert(f);
    char line[256]; int names=0;
    while (fgets(line,sizeof line,f)) {
        char name[64]; unsigned code;
        if (sscanf(line," (0x%x, \"%63[^\n]",&code,name)!=2) continue;
        char *end=strrchr(name,'"'); assert(end); *end=0;
        if (!strcmp(name,"\\\\")) strcpy(name,"\\"); /* the Swift escape */
        const SDL_Scancode sc=SDL_GetScancodeFromName(name);
        if (sc==SDL_SCANCODE_UNKNOWN || strcmp(SDL_GetScancodeName(sc),name)) {
            printf("KeyCodes.swift: \"%s\" is not an SDL scancode name (SDL: \"%s\")\n",name,sc ? SDL_GetScancodeName(sc) : "");
            assert(0);
        }
        ++names;
    }
    fclose(f);
    assert(names>100);
    f=fopen("launcher/Model/Bindings.swift","r");
    assert(f);
    int actions=0;
    while (fgets(line,sizeof line,f)) {
        const char *k=strstr(line,"BindAction(key: \""), *d=strstr(line,"defaults: [");
        if (!k || !d) continue;
        char key[64], list[128]="";
        assert(sscanf(k,"BindAction(key: \"%63[^\"]",key)==1);
        for (const char *p=d+11; *p && *p!=']'; ++p) if (*p!='"') strncat(list,(char[]){*p==' ' && p[-1]==',' ? 0 : *p,0},1);
        int found=0;
        for (int a=0;a<BIND_COUNT;++a) {
            if (strcmp(runtime_bindings_key(a),key)) continue;
            found=1;
            if (strcmp(runtime_bindings_default(a),list)) { printf("Bindings.swift %s: %s, runtime_bindings.c: %s\n",key,list,runtime_bindings_default(a)); assert(0); }
        }
        assert(found);
        ++actions;
    }
    fclose(f);
    assert(actions==BIND_COUNT);
    f=fopen("launcher/Model/Bindings.swift","r");
    assert(f);
    int pads=0, buttons=-1;
    while (fgets(line,sizeof line,f)) {
        const char *k=strstr(line,"PadAction(key: \""), *d=strstr(line,"defaults: [");
        if (k && d) {
            char key[64], list[128]="";
            assert(sscanf(k,"PadAction(key: \"%63[^\"]",key)==1);
            for (const char *p=d+11; *p && *p!=']'; ++p) if (*p!='"') strncat(list,(char[]){*p==' ' && p[-1]==',' ? 0 : *p,0},1);
            int found=0;
            for (int i=0;i<PAD_COUNT;++i) {
                if (strcmp(runtime_bindings_pad_key(i),key)) continue;
                found=1;
                if (strcmp(runtime_bindings_pad_default(i),list)) { printf("Bindings.swift %s: %s, runtime_bindings.c: %s\n",key,list,runtime_bindings_pad_default(i)); assert(0); }
            }
            assert(found);
            ++pads;
        }
        const char *b=strstr(line,"static let buttons = [");
        if (b) { /* "Cross", "Circle", ... in PAD_ order */
            buttons=0;
            for (const char *p=strchr(b,'"'); p; p=strchr(p,'"')) {
                const char *end=strchr(p+1,'"');
                assert(end && buttons<PAD_BUTTON_COUNT);
                const char *want=runtime_bindings_pad_name(buttons++);
                assert(strlen(want)==(size_t)(end-p-1) && !strncmp(want,p+1,strlen(want)));
                p=end+1;
            }
        }
    }
    fclose(f);
    assert(pads==PAD_COUNT && buttons==PAD_BUTTON_COUNT);
}

/* Controller bindings (bbport.ini bind_pad_): defaults as before, a swap, two buttons for one
 * input, a button onto a trigger, a trigger left unbound, extra buttons (paddle, misc), unbound
 * inputs, an unknown name ignored. */
/* Virtual trigger axes span -32768 (released) .. 32767 (SDL scales them to 0 .. 32767). */
static void set_pad(SDL_Joystick *joystick, const SDL_GamepadButton *down, size_t count, int16_t right_trigger) {
    for (int b=0;b<SDL_GAMEPAD_BUTTON_COUNT;++b) assert(SDL_SetJoystickVirtualButton(joystick,b,false));
    for (size_t i=0;i<count;++i) assert(SDL_SetJoystickVirtualButton(joystick,down[i],true));
    assert(SDL_SetJoystickVirtualAxis(joystick,SDL_GAMEPAD_AXIS_RIGHT_TRIGGER,right_trigger));
    SDL_UpdateJoysticks();
    SDL_UpdateGamepads();
}
#define SET_PAD(trigger,...) set_pad(joystick,(const SDL_GamepadButton[]){__VA_ARGS__}, \
    sizeof((const SDL_GamepadButton[]){__VA_ARGS__})/sizeof(SDL_GamepadButton),trigger)
static void test_controller_remap(SDL_Joystick *joystick) {
    PadData d;
    SET_PAD(20000,SDL_GAMEPAD_BUTTON_SOUTH,SDL_GAMEPAD_BUTTON_LEFT_SHOULDER,SDL_GAMEPAD_BUTTON_DPAD_UP);
    assert(pad_read_state(1,&d)==0);
    assert(d.buttons==(BTN_CROSS|BTN_L1|BTN_UP|BTN_R2) && d.r2==206 && d.l2==0);
    SET_PAD(-28000,SDL_GAMEPAD_BUTTON_SOUTH);
    assert(pad_read_state(1,&d)==0 && d.buttons==BTN_CROSS && d.r2==18); /* light pull: analog only */

    char path[]="/tmp/bbport-padmap-test-XXXXXX";
    int fd=mkstemp(path);
    assert(fd>=0);
    close(fd);
    write_file(path,"bind_pad_circle=cross\nbind_pad_cross = Circle , paddle 1\nbind_pad_r2=L1\nbind_pad_l1=\n"
                    "bind_pad_dpad_up=\nbind_pad_square=Bogus,Square\nbind_pad_options=Misc\n");
    runtime_bindings_load(path);
    assert(runtime_bindings_pad(PAD_CROSS,0)==PAD_CIRCLE && runtime_bindings_pad(PAD_CROSS,1)==PAD_PADDLE1 &&
           runtime_bindings_pad(PAD_CROSS,2)==-1 && runtime_bindings_pad(PAD_SQUARE,0)==PAD_SQUARE);
    SET_PAD(-32768,SDL_GAMEPAD_BUTTON_SOUTH,SDL_GAMEPAD_BUTTON_DPAD_UP,SDL_GAMEPAD_BUTTON_START,SDL_GAMEPAD_BUTTON_WEST);
    assert(pad_read_state(1,&d)==0 && d.buttons==(BTN_CIRCLE|BTN_SQUARE));
    SET_PAD(-32768,SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1,SDL_GAMEPAD_BUTTON_LEFT_SHOULDER,SDL_GAMEPAD_BUTTON_MISC1);
    assert(pad_read_state(1,&d)==0 && d.buttons==(BTN_CROSS|BTN_R2|BTN_OPTIONS) && d.r2==255);
    SET_PAD(20000,SDL_GAMEPAD_BUTTON_LEFT_STICK);
    assert(pad_read_state(1,&d)==0 && d.buttons==BTN_L3 && d.r2==0); /* R2 itself is unbound now */
    SET_PAD(-32768,SDL_GAMEPAD_BUTTON_BACK);
    assert(pad_read_state(1,&d)==0 && (d.buttons & BTN_TOUCHPAD) && d.touch_count==1); /* Select: still the touchpad */
    unlink(path);
    runtime_bindings_load(NULL);
    SET_PAD(-32768,SDL_GAMEPAD_BUTTON_LEFT_STICK);
}

int main(void) {
    char path[]="/tmp/bbport-pad-test-XXXXXX";
    int fd=mkstemp(path);
    assert(fd>=0);
    close(fd);
    setenv("BB_PAD_FILE",path,1);
    setenv("SDL_VIDEODRIVER","dummy",1);
    /* Only the virtual test controller is a gamepad, whatever is plugged in. */
    SDL_SetHint(SDL_HINT_GAMECONTROLLER_IGNORE_DEVICES_EXCEPT,"0x1d50/0x6189");
    assert(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_GAMEPAD));
    assert(pad_init()==0 && pad_open(1,0,0,NULL)==1);
    test_bindings();
    test_menu_keys();
    test_menu_wheel_hover();
    test_mouse_response();
    test_wheel_pulses();
    test_launcher_tables();
    PadData data;
    inject(path,"cross l3 touchpad_left");
    assert(pad_read_state(1,&data)==0);
    assert((data.buttons & (BTN_CROSS|BTN_L3|BTN_TOUCHPAD))==(BTN_CROSS|BTN_L3|BTN_TOUCHPAD));
    /* A new touch gets a new id (1..127), as from a DualShock 4: the game ignores id 0. */
    assert(data.touch_count==1 && data.touches[0].x==480 && data.touches[0].y==471 && data.touches[0].id==1);
    inject(path,"touchpad_right");
    assert(pad_read_state(1,&data)==0 && data.touch_count==1 && data.touches[0].x==1440);
    inject(path,"");
    assert(pad_read_state(1,&data)==0 && data.buttons==0 && data.touch_count==0);

    SDL_VirtualJoystickTouchpadDesc touch={.nfingers=2};
    SDL_VirtualJoystickDesc desc;
    SDL_INIT_INTERFACE(&desc);
    desc.type=SDL_JOYSTICK_TYPE_GAMEPAD;
    desc.naxes=SDL_GAMEPAD_AXIS_COUNT;
    desc.nbuttons=SDL_GAMEPAD_BUTTON_COUNT;
    desc.button_mask=(1u<<SDL_GAMEPAD_BUTTON_COUNT)-1;
    desc.axis_mask=(1u<<SDL_GAMEPAD_AXIS_COUNT)-1;
    desc.name="bbport test controller";
    desc.vendor_id=0x1d50;
    desc.product_id=0x6189;
    desc.ntouchpads=1;
    desc.touchpads=&touch;
    SDL_JoystickID id=SDL_AttachVirtualJoystick(&desc);
    assert(id!=0);
    SDL_Joystick *joystick=SDL_OpenJoystick(id);
    assert(joystick);
    assert(SDL_SetJoystickVirtualTouchpad(joystick,0,0,true,0.75f,0.5f,1.0f));
    assert(SDL_SetJoystickVirtualTouchpad(joystick,0,1,true,0.25f,1.0f,1.0f));
    assert(SDL_SetJoystickVirtualButton(joystick,SDL_GAMEPAD_BUTTON_TOUCHPAD,true));
    SDL_UpdateJoysticks();
    SDL_UpdateGamepads();
    assert(pad_read_state(1,&data)==0);
    assert(gamepad && data.touch_count==2 && (data.buttons & BTN_TOUCHPAD));
    assert(data.touches[0].x==1439 && data.touches[0].y==471 && data.touches[0].id==2);
    assert(data.touches[1].x==480 && data.touches[1].y==942 && data.touches[1].id==3);
    capture=1;
    assert(pad_read_state(1,&data)==0 && data.touch_count==0 && data.buttons==0);
    capture=0;
    assert(SDL_SetJoystickVirtualTouchpad(joystick,0,0,false,0,0,0));
    assert(SDL_SetJoystickVirtualTouchpad(joystick,0,1,false,0,0,0));
    SDL_UpdateJoysticks();
    SDL_UpdateGamepads();
    assert(pad_read_state(1,&data)==0 && data.touch_count==1 && data.touches[0].x==480);
    test_controller_remap(joystick);
    SDL_CloseJoystick(joystick);
    if (gamepad) SDL_CloseGamepad(gamepad);
    gamepad=NULL;
    assert(SDL_DetachVirtualJoystick(id));
    SDL_Quit();
    unlink(path);
    puts("PASS: pad ABI, debug camera chord, left/right clicks, SDL touch coordinates, overlay capture, bindings, controller remapping");
}
