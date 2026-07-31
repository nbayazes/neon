#pragma once

#include "neon-strings.h"
#include "neon-types.h"
#include "Streams.h"

namespace neon::d3 {

// Descent 3 HOG2 file
class Hog2 {
    Dictionary<string, int> _table;

public:
    struct Entry {
        string name;
        uint flags;
        uint len;
        uint timestamp;
        int64 offset;
    };

    filesystem::path path;
    List<Entry> entries;

    static bool IsHog2(const filesystem::path& path) {
        StreamReader reader(path);
        auto id = reader.ReadString(4);
        return id == "HOG2";
    }

    static Hog2 Read(const filesystem::path& path) {
        Hog2 hog;
        hog.path = path;

        StreamReader r(path);
        auto id = r.ReadString(4);
        if (id != "HOG2")
            throw Exception("Not a HOG2 file");

        uint nfiles = r.ReadUInt32();
        hog.entries.reserve(nfiles);

        const auto dataOffset = r.ReadUInt32();
        constexpr int PSFILENAME_LEN = 35;
        constexpr int HOG_HDR_SIZE = 64;

        r.Seek(4 + HOG_HDR_SIZE);
        auto offset = dataOffset;
        for (uint i = 0; i < nfiles; i++) {
            auto& entry = hog.entries.emplace_back();
            entry.name = String::ToLower(r.ReadString(PSFILENAME_LEN + 1));
            entry.flags = r.ReadUInt32();
            entry.len = r.ReadUInt32();
            entry.timestamp = r.ReadUInt32();
            entry.offset = offset;
            offset += entry.len;

            hog._table.insert({ entry.name, i });
        }

        return hog;
    }

    List<ubyte> ReadEntry(int index) const {
        if (!Seq::inRange(entries, index))
            throw Exception("Invalid entry index");

        StreamReader r(path);
        const auto& entry = entries.at(index);
        r.Seek(entry.offset);
        List<ubyte> data(entry.len);
        r.ReadBytes(data);
        return data;
    }

    Option<List<ubyte>> ReadEntry(string_view name) const {
        auto lname = String::ToLower(name);
        auto find = _table.find(lname);
        if (find == _table.end()) return {};
        else return ReadEntry(find->second);
    }
};
}
