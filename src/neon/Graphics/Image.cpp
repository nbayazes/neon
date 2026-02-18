#include "pch.h"
#include "Image.h"
#include "vfs/FileSystem.h"
#include "neon-strings.h"
#include "vfs/VirtualFileSystem.h"

namespace neon::gfx {
    Option<Image> ReadImage(string_view name, bool srgb) {
        auto ext = String::ToLower(String::Extension(name));
        Image image;

        if (ext.empty()) {
            // prioritize dds
            if (auto dds = vfs::Read(name + ".dds")) {
                image.LoadDDS(*dds, srgb);
            }
            else if (auto png = vfs::Read(name + ".png")) {
                image.LoadWIC(*png, srgb);
            }
            else if (auto tga = vfs::Read(name + ".tga")) {
                image.LoadTGA(*tga, srgb);
            }
            // don't scan for pcx or bbm automatically
            //else if (auto pcxData = vfs::Read(name + ".pcx")) {
            //    auto pcx = ReadPCX(*pcxData);
            //    image.LoadBitmap2D(pcx);
            //}
            //else if (auto bbmData = vfs::Read(name + ".bbm")) {
            //    auto bbm = ReadBbm(*bbmData);
            //    image.LoadBitmap2D(bbm);
            //}
        }
        else if (auto data = vfs::Read(name)) {
            if (ext == ".dds") {
                image.LoadDDS(*data, srgb);
            }
            else if (ext == ".png") {
                image.LoadWIC(*data, srgb);
            }
            else if (ext == ".tga") {
                image.LoadTGA(*data, srgb);
            }
            //else if (ext == ".pcx") {
            //    auto pcx = ReadPCX(*data);
            //    image.Load<Palette::Color>(pcx.Data, pcx.Width, pcx.Height);
            //}
            //else if (ext == ".bbm") {
            //    auto bbm = ReadBbm(*data);
            //    image.Load<Palette::Color>(bbm.Data, bbm.Width, bbm.Height);
            //}
            //else if (ext == ".ogf") {
            //    image.LoadOGF(*data);
            //}
        }

        // use game data
        //if (!image.GetPixels()) {
        //    for (auto& bitmap : GameData.bitmaps) {
        //        if (bitmap.Info.Name == "default") continue;
        //        if (String::EqualsIgnoreCase(bitmap.Info.Name, name)) {
        //            image.Load<Palette::Color>(bitmap.Data, bitmap.Info.Width, bitmap.Info.Height);
        //            break;
        //        }
        //    }
        //}

        if (!image.GetPixels()) return {}; // no data
        return image;
    }
}
