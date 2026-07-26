#include "pch.h"
#include "DirectoryWatcher.h"
#include <array>
#include <unordered_set>
#include <windows.h>
#include "Logging.h"

namespace neon {
HANDLE CreateWatchHandle(const std::filesystem::path& directory) {
    auto handle = FindFirstChangeNotification(
        directory.c_str(), // directory to watch 
        FALSE, // do not watch subtree 
        FILE_NOTIFY_CHANGE_LAST_WRITE); // watch for file modified

    if (handle == INVALID_HANDLE_VALUE) {
        SPDLOG_ERROR("FindFirstChangeNotification function failed: {}", GetLastError());
        throw std::exception();
    }

    return handle;
}

void WatchDirectory(HANDLE handle) {
    // Wait for notification
    auto dwWaitStatus = WaitForSingleObject(handle, INFINITE);

    switch (dwWaitStatus) {
        case WAIT_OBJECT_0:
            //RefreshDirectory(lpDir);
            SPDLOG_INFO("directory contents changed");

            if (!FindNextChangeNotification(handle)) {
                throw std::exception(fmt::format("FindNextChangeNotification failed {}", GetLastError()).c_str());
            }

            break;

        default:
            throw std::exception(fmt::format("Unhandled dwWaitStatus {}", GetLastError()).c_str());
    }
}

void WatchDirectory2(const std::filesystem::path& path) {
    std::vector<std::byte> buffer;
    HANDLE directoryWatcher{};
    OVERLAPPED overlapped{};

    directoryWatcher = CreateFile(
        path.c_str(),
        FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_OVERLAPPED | FILE_FLAG_BACKUP_SEMANTICS,
        nullptr);

    if (directoryWatcher == INVALID_HANDLE_VALUE)
        throw std::runtime_error("Invalid path");

    buffer.resize(1024);
    overlapped.hEvent = CreateEvent(nullptr, true, false, nullptr);

    DWORD filter = FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_FILE_NAME;
    ReadDirectoryChangesW(directoryWatcher, buffer.data(), (DWORD)buffer.size(), false, filter, nullptr, &overlapped, nullptr);
}

void DirectoryWatcher::Worker() {
    SPDLOG_INFO("Watching directory: `{}`", _directory.string());
    _handle = CreateWatchHandle(_directory);
    _alive = true;

    while (_alive) {
        try {
            WatchDirectory(_handle);
        }
        catch (const std::exception& e) {
            SPDLOG_ERROR("Exception in directory watcher: {}", e.what());
            _alive = false;
        }
    }

    SPDLOG_INFO("No longer watching directory: `{}`", _directory.string());

    //if (_worker.joinable())
    //    _worker.join();
}

DirectoryChangeNotifier::DirectoryChangeNotifier(const std::filesystem::path& path, size_t bufferSize) {
    _watcherHandle = CreateFile(
        path.c_str(),
        FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_OVERLAPPED | FILE_FLAG_BACKUP_SEMANTICS,
        nullptr);

    if (_watcherHandle == INVALID_HANDLE_VALUE)
        throw std::runtime_error("Invalid path");

    _buffer.resize(bufferSize);
    _overlapped.hEvent = CreateEvent(nullptr, true, false, nullptr);
    if (!ReadDirectoryChangesW(
        _watcherHandle,
        _buffer.data(),
        (DWORD)_buffer.size(),
        false,
        FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_FILE_NAME,
        nullptr,
        &_overlapped,
        nullptr)) {
        CloseHandle(_watcherHandle);
        if (_overlapped.hEvent) CloseHandle(_overlapped.hEvent);
        throw std::runtime_error("Failed to start watching");
    }
}

DirectoryChangeNotifier::~DirectoryChangeNotifier() {
    CancelIo(_watcherHandle);
    CloseHandle(_watcherHandle);
    CloseHandle(_overlapped.hEvent);
}

std::unordered_set<std::filesystem::path> DirectoryChangeNotifier::GetChangedFiles() {
    DWORD bytesTransferred;
    std::unordered_set<std::filesystem::path> changedFiles;

    if (GetOverlappedResult(_watcherHandle, &_overlapped, &bytesTransferred, false)) {
        auto info = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(_buffer.data());

        for (std::byte* currentBuffer = _buffer.data();; currentBuffer += info->NextEntryOffset) {
            info = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(currentBuffer);
            std::filesystem::path changedFile{ std::wstring{ info->FileName, info->FileNameLength / sizeof(wchar_t) } };

            if (_filesToWatch.contains(changedFile))
                changedFiles.insert(changedFile);

            if (info->NextEntryOffset == 0)
                break;
        }

        ResetEvent(_overlapped.hEvent);

        if (!ReadDirectoryChangesW(
            _watcherHandle,
            _buffer.data(),
            (DWORD)_buffer.size(),
            false,
            FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_FILE_NAME,
            nullptr,
            &_overlapped,
            nullptr)) {
            throw std::runtime_error("Failed to get changed files");
        }
    }

    return changedFiles;
}
}
