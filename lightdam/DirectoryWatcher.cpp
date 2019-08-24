#include "DirectoryWatcher.h"
#include "StringConversion.h"
#include "ErrorHandling.h"
#include <Windows.h>

DirectoryWatcher::DirectoryWatcher(const wchar_t* directoryPath)
{
    m_watcherHandle = FindFirstChangeNotification(directoryPath, TRUE, FILE_NOTIFY_CHANGE_LAST_WRITE);
    if (m_watcherHandle == INVALID_HANDLE_VALUE)
    {
        m_watcherHandle = nullptr;
        throw std::runtime_error(std::string("Failed to create directory watcher for \"") + Utf16toUtf8(directoryPath) + "\"");
    }
}

DirectoryWatcher::~DirectoryWatcher()
{
    FindCloseChangeNotification(m_watcherHandle);
}

bool DirectoryWatcher::HasDirectoryFileChangesSinceLastCheck()
{
    if (WaitForSingleObject(m_watcherHandle, 0) == WAIT_OBJECT_0)
    {
        if (FindNextChangeNotification(m_watcherHandle) == FALSE)
            LogPrint(LogLevel::Failure, "Failed to continue directory watching.");
        return true;
    }
    else
        return false;
}
