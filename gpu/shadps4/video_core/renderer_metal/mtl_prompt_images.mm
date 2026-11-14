// SPDX-License-Identifier: GPL-2.0-or-later
// bbport: button prompt textures (mtl_prompt_images.h).

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <unordered_map>
#include <vector>

#include "bbport_button_prompts.h"
#include "video_core/renderer_metal/mtl_device.h"
#include "video_core/renderer_metal/mtl_prompt_images.h"

namespace Metal {
namespace {

constexpr u32 IconSide = 64;                       // the textures' top level
constexpr u32 CellSide = BbPrompts::GameIconSize;  // an atlas cell
constexpr u64 AtlasDelayMs = 150;  // between the game's upload and the copy: the upload is on
                                   // the renderer's queue, the copy on another
constexpr u64 RetireDelayMs = 5000; // a replaced texture is kept (frames in flight may read it)

u64 NowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

// Reads the source texel for texel: the copy has the game's own bytes around the cells.
constexpr const char* CopySource = R"(
#include <metal_stdlib>
using namespace metal;
struct Out { float4 position [[position]]; };
vertex Out bb_prompt_vertex(uint id [[vertex_id]]) {
    Out out;
    out.position = float4(float((id << 1) & 2) * 2.0 - 1.0, float(id & 2) * 2.0 - 1.0, 0.0, 1.0);
    return out;
}
fragment float4 bb_prompt_fragment(Out in [[stage_in]], texture2d<float> source [[texture(0)]]) {
    return source.read(uint2(in.position.xy), 0);
}
)";

struct ViewKey {
    u64 id; // icon, or the atlas image's uid | 1 << 63
    u32 kind; // array | srgb << 1 | swizzle << 8
    bool operator==(const ViewKey&) const = default;
};
struct ViewKeyHash {
    size_t operator()(const ViewKey& k) const noexcept {
        return std::hash<u64>{}(k.id * 0x9e3779b97f4a7c15ull ^ k.kind);
    }
};

/// `side` x `side` straight-alpha RGBA8 to half the side (alpha-weighted, so edges keep their
/// colour).
std::vector<u32> Halve(const std::vector<u32>& in, u32 side) {
    const u32 half = side / 2;
    std::vector<u32> out(size_t(half) * half);
    for (u32 y = 0; y < half; ++y) {
        for (u32 x = 0; x < half; ++x) {
            u32 sum[4] = {};
            for (u32 d = 0; d < 4; ++d) {
                const u32 texel = in[size_t(y * 2 + (d >> 1)) * side + x * 2 + (d & 1)];
                const u32 a = texel >> 24;
                for (u32 c = 0; c < 3; ++c) {
                    sum[c] += ((texel >> (c * 8)) & 0xff) * a;
                }
                sum[3] += a;
            }
            u32 texel = ((sum[3] + 2) / 4) << 24;
            for (u32 c = 0; c < 3 && sum[3]; ++c) {
                texel |= std::min((sum[c] + sum[3] / 2) / sum[3], 255u) << (c * 8);
            }
            out[size_t(y) * half + x] = texel;
        }
    }
    return out;
}

} // namespace

struct PromptImages::Impl {
    id<MTLDevice> device;
    id<MTLCommandQueue> queue; // its own: never waits behind the renderer's open command buffers
    id<MTLRenderPipelineState> copy_linear, copy_srgb;
    bool copy_failed = false;

    std::mutex mutex; // the views (draw threads) against Update (GPU thread)
    u32 generation = 0;
    u64 now_ms = 0;
    std::vector<bool> drawn;
    std::vector<id<MTLTexture>> icons;         // per icon, RGBA8Unorm with mips
    std::vector<std::vector<u32>> cells;       // per icon, CellSide^2 RGBA8
    struct Atlas {
        id<MTLTexture> source;   // the game's, as of `wanted`
        id<MTLTexture> copy;     // RGBA8Unorm, the cells replaced
        u32 atlas = 0;
        u32 wanted = 0, made = 0; // uploads: asked for, in the copy (0: none)
        u64 since_ms = 0;         // when the upload was first seen
        u32 generation = 0;       // the icons in its cells
        std::vector<std::vector<u32>> original; // the game's cells, per AtlasCells entry
    };
    std::unordered_map<u64, Atlas> atlases;
    std::unordered_map<ViewKey, id<MTLTexture>, ViewKeyHash> views;
    std::vector<std::pair<u64, id>> retired; // when, object

    void Retire(id object) {
        if (object) {
            retired.emplace_back(NowMs(), object);
        }
    }
    void DropViews(u64 id_bits) {
        for (auto it = views.begin(); it != views.end();) {
            if (it->first.id == id_bits) {
                Retire(it->second);
                it = views.erase(it);
            } else {
                ++it;
            }
        }
    }
    id<MTLTexture> ViewOf(id<MTLTexture> texture, u64 id_bits, bool array, bool srgb,
                          std::array<u8, 4> swizzle) {
        const ViewKey key{id_bits, u32(array) | u32(srgb) << 1 | u32(swizzle[0]) << 8 |
                                       u32(swizzle[1]) << 12 | u32(swizzle[2]) << 16 |
                                       u32(swizzle[3]) << 20};
        if (const auto it = views.find(key); it != views.end()) {
            return it->second;
        }
        const MTLTextureSwizzleChannels channels = MTLTextureSwizzleChannelsMake(
            MTLTextureSwizzle(swizzle[0]), MTLTextureSwizzle(swizzle[1]),
            MTLTextureSwizzle(swizzle[2]), MTLTextureSwizzle(swizzle[3]));
        id<MTLTexture> view =
            [texture newTextureViewWithPixelFormat:srgb ? MTLPixelFormatRGBA8Unorm_sRGB
                                                        : MTLPixelFormatRGBA8Unorm
                                       textureType:array ? MTLTextureType2DArray : MTLTextureType2D
                                            levels:NSMakeRange(0, texture.mipmapLevelCount)
                                            slices:NSMakeRange(0, 1)
                                           swizzle:channels];
        if (view) {
            views.emplace(key, view);
        }
        return view;
    }
    id<MTLTexture> NewTexture(u32 width, u32 height, bool mips, bool target) {
        MTLTextureDescriptor* desc =
            [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                                               width:width
                                                              height:height
                                                           mipmapped:mips];
        desc.storageMode = MTLStorageModeShared;
        desc.usage = MTLTextureUsageShaderRead | MTLTextureUsagePixelFormatView |
                     (target ? MTLTextureUsageRenderTarget : 0);
        id<MTLTexture> texture = [device newTextureWithDescriptor:desc];
        texture.label = target ? @"bb prompt atlas" : @"bb prompt icon";
        return texture;
    }
    id<MTLRenderPipelineState> CopyPipeline(bool srgb) {
        id<MTLRenderPipelineState> __strong& slot = srgb ? copy_srgb : copy_linear;
        if (slot || copy_failed) {
            return slot;
        }
        NSError* error = nil;
        id<MTLLibrary> library = [device newLibraryWithSource:@(CopySource) options:nil error:&error];
        MTLRenderPipelineDescriptor* desc = [MTLRenderPipelineDescriptor new];
        desc.vertexFunction = [library newFunctionWithName:@"bb_prompt_vertex"];
        desc.fragmentFunction = [library newFunctionWithName:@"bb_prompt_fragment"];
        desc.colorAttachments[0].pixelFormat =
            srgb ? MTLPixelFormatRGBA8Unorm_sRGB : MTLPixelFormatRGBA8Unorm;
        slot = library ? [device newRenderPipelineStateWithDescriptor:desc error:&error] : nil;
        if (!slot) {
            copy_failed = true;
            std::printf("Button prompts: no atlas copy pipeline (%s): the HUD keeps the game's icons\n",
                        error ? error.localizedDescription.UTF8String : "?");
        }
        return slot;
    }
    void WriteCell(id<MTLTexture> texture, u32 x, u32 y, const std::vector<u32>& pixels) {
        [texture replaceRegion:MTLRegionMake2D(x, y, CellSide, CellSide)
                   mipmapLevel:0
                     withBytes:pixels.data()
                   bytesPerRow:CellSide * 4];
    }
    /// The cells of `atlas` as the device in use wants them: the port's icons, or the game's.
    void WriteCells(Atlas& atlas) {
        const auto cell_list = BbPrompts::AtlasCells(atlas.atlas);
        for (size_t i = 0; i < cell_list.size() && i < atlas.original.size(); ++i) {
            const auto& cell = cell_list[i];
            const bool ours = cell.icon >= 1 && cell.icon <= drawn.size() && drawn[cell.icon - 1] &&
                              !cells[cell.icon - 1].empty();
            WriteCell(atlas.copy, cell.x, cell.y, ours ? cells[cell.icon - 1] : atlas.original[i]);
        }
        atlas.generation = generation;
    }
    /// Copies the game's atlas into `atlas.copy` (the same bytes, uncompressed) and waits for it.
    bool MakeCopy(Atlas& atlas) {
        id<MTLTexture> source = atlas.source;
        const bool srgb = source.pixelFormat == MTLPixelFormatBC7_RGBAUnorm_sRGB;
        id<MTLRenderPipelineState> pipeline = CopyPipeline(srgb);
        if (!pipeline) {
            return false;
        }
        const u32 width = u32(source.width), height = u32(source.height);
        if (!atlas.copy || atlas.copy.width != width || atlas.copy.height != height) {
            Retire(atlas.copy);
            atlas.copy = NewTexture(width, height, false, true);
        }
        id<MTLTexture> target = srgb ? [atlas.copy newTextureViewWithPixelFormat:MTLPixelFormatRGBA8Unorm_sRGB]
                                     : atlas.copy;
        id<MTLTexture> level = source.textureType == MTLTextureType2D && source.mipmapLevelCount == 1
                                   ? source
                                   : [source newTextureViewWithPixelFormat:source.pixelFormat
                                                               textureType:MTLTextureType2D
                                                                    levels:NSMakeRange(0, 1)
                                                                    slices:NSMakeRange(0, 1)];
        if (!atlas.copy || !target || !level) {
            return false;
        }
        MTLRenderPassDescriptor* pass = [MTLRenderPassDescriptor renderPassDescriptor];
        pass.colorAttachments[0].texture = target;
        pass.colorAttachments[0].loadAction = MTLLoadActionDontCare;
        pass.colorAttachments[0].storeAction = MTLStoreActionStore;
        id<MTLCommandBuffer> commands = [queue commandBuffer];
        commands.label = @"bb prompt atlas";
        id<MTLRenderCommandEncoder> encoder = [commands renderCommandEncoderWithDescriptor:pass];
        [encoder setRenderPipelineState:pipeline];
        [encoder setFragmentTexture:level atIndex:0];
        [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
        [encoder endEncoding];
        [commands commit];
        [commands waitUntilCompleted];
        if (commands.status != MTLCommandBufferStatusCompleted) {
            return false;
        }
        const auto cell_list = BbPrompts::AtlasCells(atlas.atlas);
        atlas.original.assign(cell_list.size(), {});
        for (size_t i = 0; i < cell_list.size(); ++i) {
            const auto& cell = cell_list[i];
            if (cell.x + CellSide > width || cell.y + CellSide > height) {
                continue;
            }
            atlas.original[i].resize(size_t(CellSide) * CellSide);
            [atlas.copy getBytes:atlas.original[i].data()
                     bytesPerRow:CellSide * 4
                      fromRegion:MTLRegionMake2D(cell.x, cell.y, CellSide, CellSide)
                     mipmapLevel:0];
        }
        // Cells off the texture have no original: left as the copy has them.
        for (size_t i = 0; i < cell_list.size(); ++i) {
            if (atlas.original[i].empty()) {
                atlas.original.resize(i);
                break;
            }
        }
        return true;
    }
};

PromptImages::PromptImages(Device& device_) : impl{std::make_unique<Impl>()} {
    impl->device = (__bridge id<MTLDevice>)device_.NativeDevice();
    impl->queue = impl->device ? [impl->device newCommandQueue] : nil;
    impl->queue.label = @"bb prompts";
    BbPrompts::Start();
}

PromptImages::~PromptImages() = default;

void PromptImages::Update() {
    Impl& m = *impl;
    if (!m.device || !BbPrompts::Enabled()) {
        return;
    }
    std::scoped_lock lock{m.mutex};
    m.now_ms = NowMs();
    std::erase_if(m.retired, [&](const auto& entry) { return m.now_ms - entry.first > RetireDelayMs; });

    BbPrompts::Set set;
    if (BbPrompts::Update(m.generation, set)) {
        m.generation = set.generation;
        m.drawn = std::move(set.drawn);
        m.icons.resize(set.count);
        m.cells.assign(set.count, {});
        constexpr u32 side = BbPrompts::IconSize;
        bool any = false;
        for (u32 i = 0; i < set.count; ++i) {
            // A new texture each time: frames in flight may still read the old one.
            m.DropViews(i + 1);
            m.Retire(m.icons[i]);
            m.icons[i] = nil;
            if (!m.drawn[i]) {
                continue;
            }
            std::vector<u32> level(set.pixels.begin() + size_t(i) * side * side,
                                   set.pixels.begin() + size_t(i + 1) * side * side);
            u32 level_side = side;
            while (level_side > IconSide) {
                level = Halve(level, level_side);
                level_side /= 2;
            }
            id<MTLTexture> texture = m.NewTexture(IconSide, IconSide, true, false);
            if (!texture) {
                m.drawn[i] = false;
                continue;
            }
            for (u32 mip = 0; mip < texture.mipmapLevelCount; ++mip) {
                [texture replaceRegion:MTLRegionMake2D(0, 0, level_side, level_side)
                           mipmapLevel:mip
                             withBytes:level.data()
                           bytesPerRow:level_side * 4];
                if (level_side == CellSide) {
                    m.cells[i] = level;
                }
                if (level_side == 1) {
                    break;
                }
                level = Halve(level, level_side);
                level_side /= 2;
            }
            m.icons[i] = texture;
            any = true;
        }
        active.store(any, std::memory_order_relaxed);
    }
    // The atlases: a copy once the game's upload has had time to reach the GPU, then its cells.
    for (auto& [uid, atlas] : m.atlases) {
        if (atlas.wanted != atlas.made && atlas.source && m.now_ms - atlas.since_ms >= AtlasDelayMs) {
            id<MTLTexture> before = atlas.copy;
            const bool made = m.MakeCopy(atlas);
            if (atlas.copy != before) {
                m.DropViews(uid | 1ull << 63);
            }
            if (made) {
                atlas.made = atlas.wanted;
                atlas.generation = ~m.generation;
            } else {
                atlas.wanted = atlas.made; // not again until the game uploads it again
            }
            atlas.source = nil;
        }
        if (atlas.copy && atlas.made && atlas.generation != m.generation) {
            m.WriteCells(atlas);
        }
    }
}

void* PromptImages::View(u32 icon, bool array, bool srgb, std::array<u8, 4> swizzle) {
    Impl& m = *impl;
    std::scoped_lock lock{m.mutex};
    if (!icon || icon > m.icons.size() || !m.drawn[icon - 1] || !m.icons[icon - 1]) {
        return nullptr;
    }
    return (__bridge void*)m.ViewOf(m.icons[icon - 1], icon, array, srgb, swizzle);
}

void* PromptImages::AtlasView(u64 image_uid, u32 atlas_index, u32 upload, void* source, bool srgb,
                              std::array<u8, 4> swizzle) {
    Impl& m = *impl;
    std::scoped_lock lock{m.mutex};
    const auto cell_list = BbPrompts::AtlasCells(atlas_index);
    const bool needed = std::any_of(cell_list.begin(), cell_list.end(), [&](const auto& cell) {
        return cell.icon >= 1 && cell.icon <= m.drawn.size() && m.drawn[cell.icon - 1];
    });
    if (!needed || !source || !upload) {
        return nullptr;
    }
    const u64 id_bits = image_uid | 1ull << 63;
    if (m.atlases.size() > 32 && !m.atlases.contains(image_uid)) {
        // A menu that keeps making new atlases: start over (their copies are kept a while).
        for (auto& [uid, old] : m.atlases) {
            m.DropViews(uid | 1ull << 63);
            m.Retire(old.copy);
        }
        m.atlases.clear();
    }
    Impl::Atlas& atlas = m.atlases[image_uid];
    if (atlas.wanted != upload) {
        id<MTLTexture> texture = (__bridge id<MTLTexture>)source;
        const bool bc7 = texture.pixelFormat == MTLPixelFormatBC7_RGBAUnorm ||
                         texture.pixelFormat == MTLPixelFormatBC7_RGBAUnorm_sRGB;
        atlas.atlas = atlas_index;
        atlas.wanted = upload;
        atlas.since_ms = NowMs();
        atlas.source = bc7 ? texture : nil;
        if (!bc7) {
            atlas.made = upload; // nothing to copy: the game's stays
            m.DropViews(id_bits);
            m.Retire(atlas.copy);
            atlas.copy = nil;
        }
    }
    // Until the new upload's copy is made the previous copy would show stale pixels.
    if (!atlas.copy || atlas.made != atlas.wanted || atlas.generation != m.generation) {
        return nullptr;
    }
    return (__bridge void*)m.ViewOf(atlas.copy, id_bits, false, srgb, swizzle);
}

} // namespace Metal
