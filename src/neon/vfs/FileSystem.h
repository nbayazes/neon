#pragma once
#include "neon-types.h"

namespace neon::fs {
    // Tries to read an entry from a zip file. Immediately closes the zip afterwards.
    //Option<List<ubyte>> ReadZipEntry(const filesystem::path& path, string_view entry);

    // Reads the file at the given path. Throws an exception if not found.
    std::vector<ubyte> ReadAllBytes(const std::filesystem::path& path);
    void WriteAllBytes(const std::filesystem::path& path, std::span<ubyte> data);
    std::string ReadAllText(const filesystem::path& path);
    std::vector<std::string> ReadLines(const filesystem::path& path);
}
