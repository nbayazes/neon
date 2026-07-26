#include "pch.h"
#include "FileSystem.h"
#include <fstream>
#include <ranges>
#include "Logging.h"

namespace neon::fs {
List<ubyte> ReadAllBytes(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        auto msg = fmt::format("Required file not found:\n{}", path.string());
        throw Exception(msg);
    }

    auto size = filesystem::file_size(path);
    List<ubyte> buffer(size);
    if (!file.read((char*)buffer.data(), (std::streamsize)size)) {
        auto msg = fmt::format("File read error: {}", path.string());
        throw Exception(msg);
    }

    return buffer;
}

//void WriteAllBytes(const std::filesystem::path& path, span<ubyte> data) {
//    std::ofstream file(path, std::ios::binary);
//    StreamWriter writer(file, false);
//    writer.WriteBytes(data);
//    SPDLOG_INFO("Wrote {} bytes to {}", data.size(), path.string());
//}

std::string ReadAllText(const filesystem::path& path) {
    std::ifstream stream(path);
    if (!stream) {
        SPDLOG_WARN("Unable to open file `{}`", path.string());
        return {};
    }

    return { std::istreambuf_iterator(stream), std::istreambuf_iterator<char>() };
}

std::vector<std::string> ReadLines(const filesystem::path& path) {
    std::ifstream stream(path);
    if (!stream) {
        SPDLOG_WARN("Unable to open file `{}`", path.string());
        return {};
    }

    std::vector<std::string> lines;
    std::string line;

    while (std::getline(stream, line))
        lines.push_back(line);

    return lines;
}

//Option<List<ubyte>> ReadZipEntry(const filesystem::path& path, string_view entry) {
//    if (auto zip = OpenZip(path)) {
//        return zip->TryReadEntry(entry);
//    }

//    return {};
//}
}
