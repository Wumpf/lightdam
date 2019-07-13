#pragma once

class DirectoryWatcher
{
public:
    DirectoryWatcher(const wchar_t* directoryPath);
    ~DirectoryWatcher();

    DirectoryWatcher(const DirectoryWatcher&) = delete;

    bool HasDirectoryFileChangesSinceLastCheck();

private:
    void* m_watcherHandle;
};
