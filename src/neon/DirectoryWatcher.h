#pragma once
#include <filesystem>
#include <thread>
#include <unordered_set>

namespace neon {
class DirectoryWatcher {
    std::filesystem::path _directory;
    std::thread _worker;
    std::atomic<bool> _alive;
    HANDLE _handle = nullptr;

public:
    DirectoryWatcher(std::filesystem::path directory)
        : _directory(std::move(directory)) {
        _worker = std::thread(&DirectoryWatcher::Worker, this);
    }

    DirectoryWatcher(const DirectoryWatcher& other) = delete;
    DirectoryWatcher(DirectoryWatcher&& other) noexcept = delete;
    DirectoryWatcher& operator=(const DirectoryWatcher& other) = delete;
    DirectoryWatcher& operator=(DirectoryWatcher&& other) noexcept = delete;

    ~DirectoryWatcher() {
        _alive = false;

        if (_worker.joinable())
            _worker.join();
    }

private:
    void Worker();
};


class DirectoryChangeNotifier {
    HANDLE _watcherHandle{};
    OVERLAPPED _overlapped{};
    std::vector<std::byte> _buffer;
    std::unordered_set<std::filesystem::path> _filesToWatch;

public:
    DirectoryChangeNotifier(const std::filesystem::path& path, size_t bufferSize);

    DirectoryChangeNotifier(const DirectoryChangeNotifier& other) = delete;
    DirectoryChangeNotifier(DirectoryChangeNotifier&& other) noexcept = delete;
    DirectoryChangeNotifier& operator=(const DirectoryChangeNotifier& other) = delete;
    DirectoryChangeNotifier& operator=(DirectoryChangeNotifier&& other) noexcept = delete;

    ~DirectoryChangeNotifier();

    void AddFileToWatch(const std::filesystem::path& path) {
        _filesToWatch.insert(path);
    }

    void StopWatchingFiles() {
        _filesToWatch.clear();
    }

    std::unordered_set<std::filesystem::path> GetChangedFiles();
};
}
