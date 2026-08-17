#include "pch.h"
#include "TextureRegistry.h"
#include "Graphics/DeviceResources.h"

namespace neon {

void TextureRegistry::Upload(string_view name, const gfx::Image& image, float opacity, int vclip) {
    auto& entry = GetEntry(name);
    gfx::FreeTexture(entry.texid);

    auto& resources = gfx::GetDeviceResources();
    entry.texid = gfx::UploadTexture(image, name);
    entry.handle = resources.textureDescriptors->Count() - 1;
    entry.vclip = vclip;
    entry.name = name;
    entry.opacity = opacity;
    entry.descriptor = resources.textureDescriptors->GetHandle(entry.handle).GetGpuHandle().ptr;
    //SPDLOG_INFO("Loaded {} - idx: {} - handle: {}", name, (uint)entry.texid, entry.handle);
}
TextureRegistry g_TextureRegistry;

//void TextureRegistry::UpdateTextureTable() {
//
//
//    // info.frames = (int)vclip.frames.size();
//    // info.frameTime = vclip.frameTime;
//    // info.pingpong = HasFlag(entry.flags, d3::TextureFlag::PingPong);
//    // info.index = textureInfoIndex;
//}

}
