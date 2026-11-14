#!/usr/bin/env bash
# SPDX-FileCopyrightText: Copyright 2026 Mark Hall
set -euo pipefail
cd -- "$(dirname -- "$0")"
# BB_PREBUILT=1 (packaged builds, Bloodborne.app): out/bb-probe and its GPU library are installed
# next to this script; nothing is built.
# BB_DATA_DIR: writable directory for the generated files (out/), saves (user/) and bbport.ini;
# by default ~/Library/Application Support/Bloodborne.
# The game process (x86-64) runs under Rosetta 2; a native arm64 process renders with Metal.
if [[ $OSTYPE != darwin* ]]; then echo 'This port runs on macOS (Apple Silicon) only.' >&2; exit 1; fi
data=${BB_DATA_DIR:-.}
# Data defaults to ~/Library/Application Support/Bloodborne. A checkout that already keeps
# saves in this directory goes on using them (moving saves is left to the player).
if [[ -z ${BB_DATA_DIR:-} ]]; then
    data="$HOME/Library/Application Support/Bloodborne"
    if [[ ! -d $data && ( -d user/savedata || -f bbport.ini ) ]]; then
        data=.
        echo "Saves and settings stay in $PWD (move user/ and bbport.ini to $HOME/Library/Application Support/Bloodborne to use the default)."
    fi
    # A "Saves" link next to this script leads to the save folder from Finder.
    if [[ $data != . ]]; then
        mkdir -p "$data/user/savedata"
        [[ -e Saves || -L Saves ]] || ln -s "$data/user/savedata" Saves 2>/dev/null || true
    fi
fi
out=$data/out
mkdir -p "$out"
export BB_CONFIG=${BB_CONFIG:-$data/bbport.ini}
# The launcher's fps_cap / fullscreen (bbport.ini) as BB_FPS_LIMIT / BB_FULLSCREEN.
# The windowless guest does not pump SDL quit events. Leave SIGTERM at its normal
# disposition so Stop works after SDL audio/gamepad init, including the mod-view child.
# The native GPU host installs its own termination handling and watches parent exit.
export SDL_NO_SIGNAL_HANDLERS=1
source scripts/launch_settings.sh
bb_launch_settings "$BB_CONFIG"
# GPU caches (pipelines, shaders) next to the saves. Exported before anything starts: the GPU
# library reads it while loading, in bb-probe and in the native GPU host alike.
export BB_GPU_USER_DIR=${BB_GPU_USER_DIR:-${BB_USER_DIR:-$data/user}}
mkdir -p "$BB_GPU_USER_DIR"
if [[ -z ${PYTHON:-} ]]; then PYTHON=$(command -v python3 || true); fi
if [[ -z ${PYTHON:-} ]]; then echo 'Install Python 3 or set PYTHON.' >&2; exit 1; fi
# BB_GAME_DIR: the game's folder (eboot.bin, sce_module, ...); default next to this directory.
game=${BB_GAME_DIR:-../CUSA03173}
if [[ ! -f $game/eboot.bin ]]; then echo "No eboot.bin in $game (set BB_GAME_DIR)." >&2; exit 1; fi
# Hooks and patches use the addresses of the 1.09 executable (BB_SKIP_GAME_CHECK=1 skips this).
if ! "$PYTHON" scripts/game_check.py "$game" >&2; then exit 1; fi
original_game=$game
mod_views=()
cleanup_mod_views() {
    for view in "${mod_views[@]}"; do
        "$PYTHON" -c 'import shutil,sys; shutil.rmtree(sys.argv[1])' "$view"
    done
}
game=$("$PYTHON" scripts/mods.py "$game" --out "$out" \
    --mods-dir "${BB_MODS_DIR:-$data/mods}" --config "${BB_MODS_CONFIG:-$data/mods.json}" \
    --enabled "${BB_MODS_ENABLED:-1}")
# A private merged view lasts for this launch, including restarts. Cleanup only our own view.
if [[ $game != "$(realpath "$original_game")" ]]; then
    mod_game=$game
    mod_views+=("$mod_game")
    trap cleanup_mod_views EXIT
fi
unset BB_MAC_SETTINGS_READY BB_EXIT_GAME_READY
if [[ ${BB_MAC_SETTINGS:-1} != 0 || ${BB_EXIT_GAME:-1} != 0 ]]; then
    if mac_game=$("$PYTHON" scripts/mac_menu.py "$game" "$out") && [[ -n $mac_game && -d $mac_game ]]; then
        game=$mac_game
        mod_game=$mac_game
        mod_views+=("$mac_game")
        export BB_MAC_SETTINGS_READY=1
        export BB_EXIT_GAME_READY=1
        trap cleanup_mod_views EXIT
    fi
fi
# The prepared executable, its linked system modules and the AppContent profile depend only on
# the game's eboot.bin, param.sfo and two modules and on the scripts that make them: about a
# second of Python at every start for the same $out/boot-linked.bin, content.bin and eboot.elf.
# They are kept between starts under a key of those files' sizes and times (mods cannot replace
# them: scripts/mods.py refuses to). BB_PREPARE_CACHE=0 prepares again every time.
prepare_key() {
    printf 'v1 game=%s gs=%s sku=%s\n' "$(realpath "$original_game")" "${BBPORT_GS_SLOT:-}" "${BB_CONTENT_SKU:-full}"
    stat -L -f '%z %Fm %N' scripts/prepare.py scripts/link_libc.py scripts/link_modules.py scripts/content_profile.py || return 1
    (cd "$game" && stat -L -f '%z %Fm %N' eboot.bin sce_sys/param.sfo sce_module/libc.prx sce_module/libSceFios2.prx) || return 1
}
prepared_key=''
if [[ ${BB_PREPARE_CACHE:-1} != 0 ]]; then prepared_key=$(prepare_key 2>/dev/null) || prepared_key=''; fi
if [[ -n $prepared_key && -f $out/prepare.key && $(<"$out/prepare.key") == "$prepared_key" &&
      -s $out/boot-linked.bin && -s $out/content.bin && -s $out/eboot.elf ]]; then
    echo "Prepared game image: unchanged since the last start (BB_PREPARE_CACHE=0 prepares it again)"
else
    rm -f "$out/prepare.key" # (a failed step must not leave a key for half-written files)
    "$PYTHON" scripts/prepare.py "$game" --out "$out"
    "$PYTHON" scripts/link_libc.py "$game" --out "$out"
    "$PYTHON" scripts/link_modules.py "$game" --out "$out"
    "$PYTHON" scripts/content_profile.py "$game" --out "$out" --sku "${BB_CONTENT_SKU:-full}"
    if [[ -n $prepared_key ]]; then printf '%s\n' "$prepared_key" > "$out/prepare.key"; fi
fi
# Sizes chosen below for the previous launch are recomputed after an in-game restart.
if [[ ${BB_AUTO_RENDER_RES:-} == 1 ]]; then
    unset BB_RENDER_RES BB_OUTPUT_RES BB_AUTO_RENDER_RES
fi
# BB_RENDER_RES=WxH explicitly sets the game's render resolution (a patch at start).
# Frame rate: BB_FPS=uncap (delta-time patch, vblank follows the display), 60/90 (fixed-timestep
# patches) or 30 (unpatched). Unset, scripts/launch_settings.sh picks it from bbport.ini fps_cap
# and game_timing (fixed steps for caps 30/60/90 unless game_timing=variable). BB_PATCHES adds patch names ("a;b").
fps=${BB_FPS:-uncap}
# bbport.ini output_res other than 1080p (720p for the Steam Deck, 1440p, 2160p): the whole game
# renders at the preset's size of the output (a patch), the upscaler fills the output, the UI is
# drawn at the output size. Preset and output changes need a restart.
# Live resolution changes keep the guest at 1920x1080 and scale host targets at run time instead
# (output and presets change in the menu without a restart, but post-processing stays at 1080p
# and scene targets are copied back: much slower on the Steam Deck and older GPUs). Chosen by
# BB_LIVE_RES=0/1, else bbport.ini live_resolution=0/1/auto (auto: the GPU check, strong
# discrete GPUs get them); off when unset. metalfx_white_scene_fix=1 opts into the native-guest
# workaround for white scenes above 1080p (#116), at a GPU cost. BB_RENDER_RES is an explicit override.
wide_patch=$("$PYTHON" scripts/patches.py --print-ultrawide --settings "$BB_CONFIG")
bb_native_ultrawide "$wide_patch"
scaled_render='' scaled_output=''
if [[ -z $wide_patch && -z ${BB_RENDER_RES:-} ]]; then
    read -r scaled_render scaled_output < <("$PYTHON" scripts/patches.py --print-scaled --settings "$BB_CONFIG") || true
fi
live=0 above_1080=0 metalfx_live=0 metalfx_white_scene_fix=0
if [[ -n ${scaled_output:-} ]]; then
    live=${BB_LIVE_RES:-}
    provider=${BB_UPSCALER:-}
    # Bash builtins only (no sed/grep): a bare PATH must not end run.sh silently.
    if [[ -f $BB_CONFIG ]]; then
        while IFS= read -r line || [[ -n $line ]]; do
            if [[ -z ${BB_LIVE_RES:-} && $line =~ ^live_resolution=([01]|auto)$ ]]; then
                live=${BASH_REMATCH[1]}
            fi
            if [[ -z ${BB_UPSCALER:-} && $line =~ ^[[:space:]]*upscaler[[:space:]]*=[[:space:]]*([^[:space:]]+)[[:space:]]*$ ]]; then
                provider=${BASH_REMATCH[1]}
            fi
            if [[ $line =~ ^[[:space:]]*metalfx_white_scene_fix[[:space:]]*=(.*)$ ]]; then
                if [[ ${BASH_REMATCH[1]} =~ ^[[:space:]]*1[[:space:]]*$ ]]; then
                    metalfx_white_scene_fix=1
                else
                    metalfx_white_scene_fix=0
                fi
            fi
        done < "$BB_CONFIG"
    fi
    # auto is off: the old Vulkan check wanted a discrete GPU with 8 GB, which a Mac never has.
    [[ $live == 1 ]] || live=0
    # Above 1920x1080 the startup patch breaks the scene's geometry (stretched polygons across
    # the screen, seen at 2560x1440 on macOS in both process modes): the live path keeps the
    # game's buffers at 1080p and renders host targets at the size instead.
    IFS=x read -r render_w render_h <<< "$scaled_render"
    if (( render_w > 1920 || render_h > 1080 )); then live=1 above_1080=1; fi
    # Keep the same native guest allocations as starting with upscaling off and enabling
    # MetalFX in game. The startup-patched path can feed a flat white image into MetalFX
    # after a room transition; resetting the scaler cannot repair its input.
    IFS=x read -r output_w output_h <<< "$scaled_output"
    if [[ $metalfx_white_scene_fix == 1 && $provider == metalfx ]] && (( output_w > 1920 || output_h > 1080 )); then
        live=1 metalfx_live=1
    fi
fi
if [[ $metalfx_live == 1 ]]; then
    echo "Output ${scaled_output}: MetalFX white-scene workaround enabled (native guest targets; extra post-processing and resolve cost)"
elif [[ $live == 1 && $above_1080 == 1 ]]; then
    echo "Output ${scaled_output}: scene ${scaled_render} through live resolution changes (the startup patch breaks scenes above 1080p)"
elif [[ $live == 1 ]]; then
    echo "Output ${scaled_output}: live resolution changes (live_resolution=0: startup patch)"
elif [[ -n ${scaled_output:-} ]]; then
    export BB_RENDER_RES=$scaled_render BB_OUTPUT_RES=$scaled_output BB_AUTO_RENDER_RES=1
    # More direct memory only with patches.py's heap patch (scenes larger than 1920x1080). At
    # or below 1080p the retail 5056 MiB is enough, and 9152 made the GPU arena 10 GiB, past a
    # 16 GB Mac's Metal working set (#22: 9 FPS, Metal 11.9 GB allocated at 720p).
    if (( render_w * render_h > 1920 * 1080 )); then export BB_DMEM_MB=${BB_DMEM_MB:-9152}; fi
    echo "Output ${scaled_output}: scene ${scaled_render}, direct memory ${BB_DMEM_MB:-5056} MiB (live_resolution=1: live changes)"
fi
"$PYTHON" scripts/patches.py --out "$out" --fps "$fps" --extra "$launch_patches" --settings "$BB_CONFIG" --game-dir "$game" --render-res "${BB_RENDER_RES:-}" --output-res "${BB_OUTPUT_RES:-}" \
    --patches-dir "${BB_PATCHES_DIR:-$data/patches}" --patches-config "${BB_PATCHES_CONFIG:-$data/patches.json}"
if [[ -z ${BB_VBLANK_HZ:-} ]]; then
    case $fps in uncap) export BB_VBLANK_HZ=0 ;; 90) export BB_VBLANK_HZ=90 ;; *) export BB_VBLANK_HZ=60 ;; esac
    export BB_LAUNCH_AUTO_VBLANK=$BB_VBLANK_HZ
fi
if [[ -n ${BB_PREBUILT:-} ]]; then
    probe=${BB_PROBE:-bin/bb-probe}
else
    # A checkout builds what changed, and only then: building at every start took about 2 s, and
    # the freshly linked bb-probe and bbgpu-host then cost 1.5 to 4 s more, since macOS checks
    # new binaries and Rosetta translates them again (measured: 0.03 s to the GPU host with
    # unchanged binaries). out/.built is build_macos.sh's stamp. BB_BUILD=always builds every
    # time, BB_BUILD=never runs what is there.
    stale=''
    if [[ ${BB_BUILD:-auto} == always || ! -f out/.built || ! -x out/bb-probe || ! -x out/bbgpu-host ]]; then
        stale=1
    elif [[ ${BB_BUILD:-auto} != never ]]; then
        stale=$(find src host gpu third_party/LibAtrac9 deps/x86_64/.built-* deps/arm64/.built-* build_macos.sh \
            scripts/macos/build_gpu_arm64.sh -newer out/.built -type f -print -quit 2>/dev/null) || true
    fi
    if [[ -n $stale ]]; then
        [[ $stale == 1 ]] || echo "Building: $stale changed since the last build"
        bash build_macos.sh
    fi
    probe=out/bb-probe
fi
probe_args=("$out/boot-linked.bin" --content-profile "$out/content.bin" --patches "$out/patches.bin" --app0 "$game" --user "${BB_USER_DIR:-$data/user}" --timeout "${BB_TIMEOUT:-0}" "$@")
if [[ -n ${mod_game:-} ]]; then
    "$probe" "${probe_args[@]}" &
    mod_pid=$!
    trap 'kill -TERM "$mod_pid" 2>/dev/null || true' TERM INT
    mod_status=0
    wait "$mod_pid" || mod_status=$?
    # An interrupted wait must finish the child before removing its mounted view.
    if kill -0 "$mod_pid" 2>/dev/null; then wait "$mod_pid" || mod_status=$?; fi
    exit "$mod_status"
fi
exec "$probe" "${probe_args[@]}"
