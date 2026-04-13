//--------------------------------------------------------------
// Copyright 2025, Ed Keenan, all rights reserved.
//--------------------------------------------------------------

#ifndef FILE_H
#define FILE_H

#include <cstdint>
#include <cstddef>

// Make the assumption of c-char strings, not UNICODE
class File
{
public:

    // Constants for the library
    static const uint32_t MAJOR_VERSION = 4;
    static const uint32_t MINOR_VERSION = 0;
    // hide the base directory.. in File.cpp
    static const uint32_t BASE_DIR_SIZE = 128;

    typedef void* Handle;

    enum class Mode : uint32_t
    {
        READ
    };

    enum class Position : uint32_t
    {
        BEGIN,
        CURRENT,
        END
    };

    enum class Error : uint32_t
    {
        SUCCESS,
        BASE_DIR_FAIL,
        BASE_DIR_NULLPTR_FAIL,
        BASE_DIR_INSUFFICIENT_SPACE_FAIL,
        OPEN_FAIL,
        OPEN_FILENAME_FAIL,
        OPEN_BASE_DIR_FAIL,
        CLOSE_FAIL,
        SEEK_FAIL,
        TELL_FAIL,
        UNDEFINED
    };

public:
    static File::Error GetBaseDir(char * const pDirOut, size_t DirOutSize) noexcept;
    static File::Error SetBaseDir(const char *const pDir) noexcept;
    static File::Error ClearBaseDir() noexcept;

    static File::Error Open(File::Handle &fh, const char* const fileName, File::Mode mode, bool UseBaseAddr = false) noexcept;
    static File::Error Close(File::Handle &fh) noexcept;
    static File::Error Seek(File::Handle fh, File::Position location, int offset) noexcept;
    static File::Error Tell(File::Handle fh, uint32_t& offset) noexcept;
    static bool IsHandleValid(File::Handle fh) noexcept;

    // Memory Mapping
    static File::Error Map(File::Handle fh, void*& addr, uint32_t length) noexcept;
    static File::Error Unmap(void* addr, uint32_t length) noexcept;

};



#endif

// --- End of File ---
