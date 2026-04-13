#include "File.hpp"
#include <cstring>
#include <cassert>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <climits>
#include <limits.h>
#include <sys/mman.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

static char BaseDir[File::BASE_DIR_SIZE] = { 0 };

File::Error File::ClearBaseDir() noexcept
{
    std::memset(BaseDir, 0, sizeof(BaseDir));
    return File::Error::SUCCESS;
}

File::Error File::GetBaseDir(char* const pDirOut, size_t DirOutSize) noexcept
{
    if (pDirOut == nullptr)
    {
        return File::Error::BASE_DIR_NULLPTR_FAIL;
    }

    const size_t len = std::strlen(BaseDir);
    if (len == 0)
    {
        return File::Error::BASE_DIR_FAIL;
    }

    if (DirOutSize < len + 1)
    {
        return File::Error::BASE_DIR_INSUFFICIENT_SPACE_FAIL;
    }

    std::memcpy(pDirOut, BaseDir, len + 1);
    return File::Error::SUCCESS;
}

File::Error File::SetBaseDir(const char* const pDir) noexcept
{
    if (pDir == nullptr)
    {
        return File::Error::BASE_DIR_NULLPTR_FAIL;
    }

    const size_t len = std::strlen(pDir);
    if (len >= File::BASE_DIR_SIZE)
    {
        return File::Error::BASE_DIR_INSUFFICIENT_SPACE_FAIL;
    }

    std::memcpy(BaseDir, pDir, len + 1);
    return File::Error::SUCCESS;
}

bool File::IsHandleValid(File::Handle fh) noexcept
{
    int fd = (int)(intptr_t)fh;
    return fd >= 0;
}

File::Error File::Map(File::Handle fh, void*& addr, uint32_t length) noexcept
{
    int fd = (int)(intptr_t)fh;
    if (fd < 0) return File::Error::UNDEFINED;

    addr = mmap(nullptr, length, PROT_READ, MAP_PRIVATE, fd, 0);
    if (addr == MAP_FAILED)
    {
        addr = nullptr;
        return File::Error::UNDEFINED; // Using UNDEFINED as generic error for map fail
    }
    return File::Error::SUCCESS;
}

File::Error File::Unmap(void* addr, uint32_t length) noexcept
{
    if (munmap(addr, length) != 0)
    {
        return File::Error::UNDEFINED;
    }
    return File::Error::SUCCESS;
}

File::Error File::Open(File::Handle& fh, const char* const fileName, File::Mode mode, bool UseBaseAddr) noexcept
{
    if (fileName == nullptr)
    {
        return File::Error::OPEN_FILENAME_FAIL;
    }

    int flags = 0;
    mode_t mode_flags = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH; // rw-r--r--

    switch (mode)
    {
    case Mode::READ:
        flags = O_RDONLY;
        break;
    }

    char fullPath[File::BASE_DIR_SIZE + PATH_MAX] = { 0 };
    const char* path = fileName;

    if (UseBaseAddr)
    {
        const size_t baseLen = std::strlen(BaseDir);
        if (baseLen == 0)
        {
            return File::Error::OPEN_BASE_DIR_FAIL;
        }

        // Check length to avoid overflow
        if (baseLen + std::strlen(fileName) + 2 > sizeof(fullPath)) {
                return File::Error::OPEN_FAIL; 
        }
        
        std::strcpy(fullPath, BaseDir);
        if (fullPath[baseLen - 1] != '/' && fullPath[baseLen - 1] != '\\')
        {
            std::strcat(fullPath, "/");
        }
        std::strcat(fullPath, fileName);
        path = fullPath;
    }

    int fd = open(path, flags, mode_flags);
    if (fd < 0)
    {
        return File::Error::OPEN_FAIL;
    }

    fh = (Handle)(intptr_t)fd;
    return File::Error::SUCCESS;
}

File::Error File::Close(File::Handle& fh) noexcept
{
    if (!IsHandleValid(fh))
    {
        return File::Error::CLOSE_FAIL;
    }

    int fd = (int)(intptr_t)fh;
    if (close(fd) != 0)
    {
        return File::Error::CLOSE_FAIL;
    }

    fh = (Handle)(intptr_t)-1;
    return File::Error::SUCCESS;
}

File::Error File::Seek(File::Handle fh, File::Position location, int offset) noexcept
{
    if (!IsHandleValid(fh)) return File::Error::SEEK_FAIL;
    int fd = (int)(intptr_t)fh;
    
    int whence = SEEK_SET;
    switch (location)
    {
    case File::Position::BEGIN: whence = SEEK_SET; break;
    case File::Position::CURRENT: whence = SEEK_CUR; break;
    case File::Position::END: whence = SEEK_END; break;
    }

    if (lseek(fd, offset, whence) == (off_t)-1)
    {
        return File::Error::SEEK_FAIL;
    }
    return File::Error::SUCCESS;
}

File::Error File::Tell(File::Handle fh, uint32_t& offset) noexcept
{
    if (!IsHandleValid(fh)) return File::Error::TELL_FAIL;
    int fd = (int)(intptr_t)fh;

    off_t current = lseek(fd, 0, SEEK_CUR);
    if (current == (off_t)-1)
    {
        return File::Error::TELL_FAIL;
    }
    offset = (uint32_t)current;
    return File::Error::SUCCESS;
}
