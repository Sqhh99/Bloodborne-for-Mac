// SPDX-License-Identifier: GPL-2.0-or-later
// bbport: the textures of the button prompts (shim/bbport_button_prompts.h) for the Metal-native
// renderer. The game's key guide icons are 32x32 BC7 textures, and its HUD packs some into an
// atlas; while another device than a PlayStation pad is in use, the rasterizer binds these
// instead: one RGBA8 texture per icon, and a copy of the atlas with the port's icons in its cells.
// Plain C++ (Objective-C++ in the .mm). After bloodborne-mac's vk_prompt_images.cpp.

#pragma once

#include <array>
#include <atomic>
#include <memory>
#include "common/types.h"

namespace Metal {

class Device;

class PromptImages {
public:
    explicit PromptImages(Device& device);
    ~PromptImages();

    /// GPU thread, between submits: draws the icons again when the device in use changed, and
    /// makes the atlas copies that are due.
    void Update();

    /// Whether any icon is the port's now (false: bind the game's own textures).
    bool Active() const {
        return active.load(std::memory_order_relaxed);
    }

    /// The id<MTLTexture> (unretained) to bind for the game's icon `icon` (BbPrompts::IconOf)
    /// through a view of this kind, or nullptr: the game's own stays. `srgb`: the view decodes
    /// sRGB; `swizzle`: four MTLTextureSwizzle values.
    void* View(u32 icon, bool array, bool srgb, std::array<u8, 4> swizzle);

    /// The same for an atlas (BbPrompts::AtlasOf) whose game texture is `source` (id<MTLTexture>,
    /// BC7, one level): a copy with the port's icons in its cells. `upload` changes when the game
    /// uploads the atlas again. nullptr until the copy is made (a few submits after the upload).
    void* AtlasView(u64 image_uid, u32 atlas, u32 upload, void* source, bool srgb,
                    std::array<u8, 4> swizzle);

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
    std::atomic<bool> active{false};
};

} // namespace Metal
