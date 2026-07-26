#pragma once
#include "neon-types.h"

namespace neon::vfs {
// Tries to read a resource. Supports comma separated resource names.
Option<List<ubyte>> Read(string_view name);

// Reads a resource handle
//Option<List<ubyte>> Read(const ResourceHandle& resource);

// Tries to find a resource. Supports comma separated resource names.
//Option<ResourceHandle> Find(string_view name);

// Mounts a directory, zip, hog or file. Level name is used to search for folders inside of zips or directories.
//void Mount(const std::filesystem::path& path, std::initializer_list<string_view> filter = {}, string_view levelName = {});

// Unmounts all directories and archives
//void Unmount();

// Prints all of the mounted resources
//void Print();
}
