#include "pch.h"
#include "VirtualFileSystem.h"
#include "FileSystem.h"

namespace neon::vfs {
Option<List<ubyte>> Read(string_view name) {
    return neon::fs::ReadAllBytes(name); // passthrough for now
}
}
