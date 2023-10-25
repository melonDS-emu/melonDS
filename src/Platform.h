/*
    Copyright 2016-2022 melonDS team

    This file is part of melonDS.

    melonDS is free software: you can redistribute it and/or modify it under
    the terms of the GNU General Public License as published by the Free
    Software Foundation, either version 3 of the License, or (at your option)
    any later version.

    melonDS is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with melonDS. If not, see http://www.gnu.org/licenses/.
*/

#ifndef PLATFORM_H
#define PLATFORM_H

#include "types.h"

#include <functional>
#include <string>

namespace SPI_Firmware
{
    class Firmware;
}

namespace Platform
{

void Init(int argc, char** argv);

/**
 * Frees all resources that were allocated in \c Init
 * or by any other \c Platform function.
 */
void DeInit();

enum StopReason {
    /**
     * The emulator stopped for some unspecified reason.
     * Not necessarily an error.
     */
    Unknown,

    /**
     * The emulator stopped due to an external call to \c NDS::Stop,
     * most likely because the user stopped the game manually.
     */
    External,

    /**
     * The emulator stopped because it tried to enter GBA mode,
     * which melonDS does not support.
     */
    GBAModeNotSupported,

    /**
     * The emulator stopped because of an error in the emulated console,
     * not necessarily because of an error in melonDS.
     */
    BadExceptionRegion,

    /**
     * The emulated console shut itself down normally,
     * likely because its system settings were adjusted
     * or its "battery" ran out.
     */
    PowerOff,
};

/**
 * Signals to the frontend that no more frames should be requested.
 * Frontends should not call this directly;
 * use \c NDS::Stop instead.
 */
void SignalStop(StopReason reason);

/**
 * @returns The ID of the running melonDS instance if running in local multiplayer mode,
 * or 0 if not.
 */
int InstanceID();

/**
 * @returns A suffix that should be appended to all instance-specific paths
 * if running in local multiplayer mode,
 * or the empty string if not.
 */
std::string InstanceFileSuffix();

// configuration values

enum ConfigEntry
{
#ifdef JIT_ENABLED
    JIT_Enable,
    JIT_MaxBlockSize,
    JIT_LiteralOptimizations,
    JIT_BranchOptimizations,
    JIT_FastMemory,
#endif

    ExternalBIOSEnable,

    DLDI_Enable,
    DLDI_ImagePath,
    DLDI_ImageSize,
    DLDI_ReadOnly,
    DLDI_FolderSync,
    DLDI_FolderPath,

    DSiSD_Enable,
    DSiSD_ImagePath,
    DSiSD_ImageSize,
    DSiSD_ReadOnly,
    DSiSD_FolderSync,
    DSiSD_FolderPath,

    Firm_MAC,

    WifiSettingsPath,

    AudioBitDepth,

    DSi_FullBIOSBoot,

#ifdef GDBSTUB_ENABLED
    GdbEnabled,
    GdbPortARM7,
    GdbPortARM9,
    GdbARM7BreakOnStartup,
    GdbARM9BreakOnStartup,
#endif
};

int GetConfigInt(ConfigEntry entry);
bool GetConfigBool(ConfigEntry entry);
std::string GetConfigString(ConfigEntry entry);
bool GetConfigArray(ConfigEntry entry, void* data);

/**
 * Denotes how a file will be opened and accessed.
 * Flags may or may not correspond to the operating system's file API.
 */
enum FileMode : unsigned {
    None = 0,

    /**
     * Opens a file for reading.
     * Either this or \c Write must be set.
     * Similar to \c "r" in \c fopen.
     */
    Read = 0b00'00'01,

    /**
     * Opens a file for writing, creating it if it doesn't exist.
     * Will truncate existing files unless \c Preserve is set.
     * Either this or \c Read must be set.
     * Similar to <tt>fopen</tt>'s \c "w" flag.
     */
    Write = 0b00'00'10,

    /**
     * Opens an existing file as-is without truncating it.
     * The file may still be created unless \c NoCreate is set.
     * @note This flag has no effect if \c Write is not set.
     */
    Preserve = 0b00'01'00,

    /**
     * Do not create the file if it doesn't exist.
     * @note This flag has no effect if \c Write is not set.
     */
    NoCreate = 0b00'10'00,

    /**
     * Opens a file in text mode,
     * rather than the default binary mode.
     * Text-mode files may have their line endings converted
     * to match the operating system,
     * and may also be line-buffered.
     */
    Text = 0b01'00'00,

    /**
     * Opens a file for reading and writing.
     * Equivalent to <tt>Read | Write</tt>.
     */
    ReadWrite = Read | Write,

    /**
     * Opens a file for reading and writing
     * without truncating it or creating a new one.
     * Equivalent to <tt>Read | Write | Preserve | NoCreate</tt>.
     */
    ReadWriteExisting = Read | Write | Preserve | NoCreate,

    /**
     * Opens a file for reading in text mode.
     * Equivalent to <tt>Read | Text</tt>.
     */
    ReadText = Read | Text,

    /**
     * Opens a file for writing in text mode,
     * creating it if it doesn't exist.
     * Equivalent to <tt>Write | Text</tt>.
     */
    WriteText = Write | Text,
};

/**
 * Denotes the origin of a seek operation.
 * Similar to \c fseek's \c SEEK_* constants.
 */
enum class FileSeekOrigin
{
    Start,
    Current,
    End,
};

/**
 * Opaque handle for a file object.
 * This can be implemented as a struct defined by the frontend,
 * or as a simple pointer cast.
 * The core will never look inside this struct,
 * but frontends may do so freely.
 */
struct FileHandle;

// Simple fopen() wrapper that supports UTF8.
// Can be optionally restricted to only opening a file that already exists.
FileHandle* OpenFile(const std::string& path, FileMode mode);

// opens files local to the emulator (melonDS.ini, BIOS, firmware, ...)
// For Windows builds, or portable UNIX builds it checks, by order of priority:
//   * current working directory
//   * emulator directory (essentially where the melonDS executable is) if supported
//   * any platform-specific application data directories
// in create mode, if the file doesn't exist, it will be created in the emulator
// directory if supported, or in the current directory otherwise
// For regular UNIX builds, the user's configuration directory is always used.
FileHandle* OpenLocalFile(const std::string& path, FileMode mode);

/// Returns true if the given file exists.
bool FileExists(const std::string& name);
bool LocalFileExists(const std::string& name);

/** Close a file opened with \c OpenFile.
 * @returns \c true if the file was closed successfully, false otherwise.
 * @post \c file is no longer valid and should not be used.
 * The underlying object may still be allocated (e.g. if the frontend refcounts files),
 * but that's an implementation detail.
 * @see fclose
 */
bool CloseFile(FileHandle* file);

/// @returns \c true if there is no more data left to read in this file,
/// \c false if there is still data left to read or if there was an error.
/// @see feof
bool IsEndOfFile(FileHandle* file);

/// @see fgets
bool FileReadLine(char* str, int count, FileHandle* file);

/// @see fseek
bool FileSeek(FileHandle* file, s64 offset, FileSeekOrigin origin);

/// @see rewind
void FileRewind(FileHandle* file);

/// @see fread
u64 FileRead(void* data, u64 size, u64 count, FileHandle* file);

/// @see fflush
bool FileFlush(FileHandle* file);

/// @see fwrite
u64 FileWrite(const void* data, u64 size, u64 count, FileHandle* file);

/// @see fprintf
u64 FileWriteFormatted(FileHandle* file, const char* fmt, ...);

/// @returns The length of the file in bytes, or 0 if there was an error.
/// @note If this function checks the length by using \c fseek and \c ftell
/// (or local equivalents), it must leave the stream position as it was found.
u64 FileLength(FileHandle* file);

enum LogLevel
{
    Debug,
    Info,
    Warn,
    Error,
};

void Log(LogLevel level, const char* fmt, ...);

struct Thread;
Thread* Thread_Create(std::function<void()> func);
void Thread_Free(Thread* thread);
void Thread_Wait(Thread* thread);

struct Semaphore;
Semaphore* Semaphore_Create();
void Semaphore_Free(Semaphore* sema);
void Semaphore_Reset(Semaphore* sema);
void Semaphore_Wait(Semaphore* sema);
void Semaphore_Post(Semaphore* sema, int count = 1);

struct Mutex;
Mutex* Mutex_Create();
void Mutex_Free(Mutex* mutex);
void Mutex_Lock(Mutex* mutex);
void Mutex_Unlock(Mutex* mutex);
bool Mutex_TryLock(Mutex* mutex);

void Sleep(u64 usecs);


// functions called when the NDS or GBA save files need to be written back to storage
// savedata and savelen are always the entire save memory buffer and its full length
// writeoffset and writelen indicate which part of the memory was altered
void WriteNDSSave(const u8* savedata, u32 savelen, u32 writeoffset, u32 writelen);
void WriteGBASave(const u8* savedata, u32 savelen, u32 writeoffset, u32 writelen);

/// Called when the firmware needs to be written back to storage,
/// after one of the supported write commands finishes execution.
/// @param firmware The firmware that was just written.
/// @param writeoffset The offset of the first byte that was written to firmware.
/// @param writelen The number of bytes that were written to firmware.
void WriteFirmware(const SPI_Firmware::Firmware& firmware, u32 writeoffset, u32 writelen);


// local multiplayer comm interface
// packet type: DS-style TX header (12 bytes) + original 802.11 frame
bool MP_Init();
void MP_DeInit();
void MP_Begin();
void MP_End();
int MP_SendPacket(u8* data, int len, u64 timestamp);
int MP_RecvPacket(u8* data, u64* timestamp);
int MP_SendCmd(u8* data, int len, u64 timestamp);
int MP_SendReply(u8* data, int len, u64 timestamp, u16 aid);
int MP_SendAck(u8* data, int len, u64 timestamp);
int MP_RecvHostPacket(u8* data, u64* timestamp);
u16 MP_RecvReplies(u8* data, u64 timestamp, u16 aidmask);


// LAN comm interface
// packet type: Ethernet (802.3)
bool LAN_Init();
void LAN_DeInit();
int LAN_SendPacket(u8* data, int len);
int LAN_RecvPacket(u8* data);


// interface for camera emulation
// camera numbers:
// 0 = DSi outer camera
// 1 = DSi inner camera
// other values reserved for future camera addon emulation
void Camera_Start(int num);
void Camera_Stop(int num);
void Camera_CaptureFrame(int num, u32* frame, int width, int height, bool yuv);

struct DynamicLibrary;

/**
 * @param lib The name of the library to load.
 * @return A handle to the loaded library, or \c nullptr if the library could not be loaded.
 */
DynamicLibrary* DynamicLibrary_Load(const char* lib);

/**
 * Releases a loaded library.
 * Pointers to functions in the library will be invalidated.
 * @param lib The library to unload.
 */
void DynamicLibrary_Unload(DynamicLibrary* lib);

/**
 * Loads a function from a library.
 * @param lib The library to load the function from.
 * @param name The name of the function to load.
 * @return A pointer to the loaded function, or \c nullptr if the function could not be loaded.
 */
void* DynamicLibrary_LoadFunction(DynamicLibrary* lib, const char* name);
}

#endif // PLATFORM_H
