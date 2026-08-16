#pragma once

#include "ankerl/ankerl.h"
#include "neon-types.h"

namespace neon::d3 {

// Descent 3 HOG2 file
class Hog2 {
    CaseInsensitiveDictionary<int> _table;

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

    static bool IsHog2(const filesystem::path& path);

    static Hog2 Read(const filesystem::path& path);

    List<ubyte> ReadEntry(int index) const;

    Option<List<ubyte>> ReadEntry(string_view name) const;
};

}
