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

#include <stdio.h>
#include <cassert>
#include <cstring>
#include "Savestate.h"
#include "Platform.h"

using Platform::Log;
using Platform::LogLevel;

static const char* SAVESTATE_MAGIC = "MELN";

/*
    Savestate format

    header:
    00 - magic MELN
    04 - version major
    06 - version minor
    08 - length
    0C - reserved (should be game serial later!)

    section header:
    00 - section magic
    04 - section length
    08 - reserved
    0C - reserved

    Implementation details

    version difference:
    * different major means savestate file is incompatible
    * different minor means adjustments may have to be made
*/

Savestate::Savestate(void *buffer, u32 size, bool save) :
    Error(false),
    Saving(save),
    CurSection(NO_SECTION),
    buffer(static_cast<u8 *>(buffer)),
    buffer_offset(0),
    buffer_length(size),
    buffer_owned(false),
    finished(false)
{
    if (Saving)
    {
        WriteSavestateHeader();
    }
    else
    {
        // Ensure that the file starts with "MELN"
        u32 read_magic = 0;
        Var32(&read_magic);

        if (read_magic != *((u32*)SAVESTATE_MAGIC))
        {
            Log(LogLevel::Error, "savestate: expected magic number %#08x (%s), got %#08x\n",
                *((u32*)SAVESTATE_MAGIC),
                SAVESTATE_MAGIC,
                read_magic
            );
            Error = true;
            return;
        }

        u16 major = 0;
        Var16(&major);
        if (major != SAVESTATE_MAJOR)
        {
            Log(LogLevel::Error, "savestate: bad version major %d, expecting %d\n", major, SAVESTATE_MAJOR);
            Error = true;
            return;
        }

        u16 minor = 0;
        Var16(&minor);
        if (minor > SAVESTATE_MINOR)
        {
            Log(LogLevel::Error, "savestate: state from the future, %d > %d\n", minor, SAVESTATE_MINOR);
            Error = true;
            return;
        }

        u32 read_length = 0;
        Var32(&read_length);
        if (read_length != buffer_length)
        {
            Log(LogLevel::Error, "savestate: expected a length of %d, got %d\n", buffer_length, read_length);
            Error = true;
            return;
        }

        // The next 4 bytes are reserved
        buffer_offset += 4;
    }
}


Savestate::Savestate(u32 initial_size) :
    Error(false),
    Saving(true), // Can't load from an empty buffer
    CurSection(NO_SECTION),
    buffer(nullptr),
    buffer_offset(0),
    buffer_length(initial_size),
    buffer_owned(true),
    finished(false)
{
    buffer = static_cast<u8 *>(malloc(buffer_length));

    if (buffer == nullptr)
    {
        Log(LogLevel::Error, "savestate: failed to allocate %d bytes\n", buffer_length);
        Error = true;
        return;
    }

    WriteSavestateHeader();
}

Savestate::~Savestate()
{
    if (Saving && !finished && !buffer_owned && !Error)
    { // If we haven't finished saving, and there hasn't been an error...
        Finish();
        // No need to close the active section for an owned buffer,
        // it's about to be thrown out.
    }

    if (buffer_owned)
    {
        free(buffer);
    }
}

void Savestate::Section(const char* magic)
{
    if (Error || finished) return;

    if (Saving)
    {
        // Go back to the current section's header and write the length
        CloseCurrentSection();

        CurSection = buffer_offset;

        // Write the new section's magic number
        VarArray((void*)magic, 4);

        // The next 4 bytes are the length, which we'll come back to later.
        u32 zero = 0;
        Var32(&zero);

        // The 8 bytes afterward are reserved, so we skip them.
        Var32(&zero);
        Var32(&zero);
    }
    else
    {
        u32 section_offset = FindSection(magic);

        if (section_offset != NO_SECTION)
        {
            buffer_offset = section_offset;
        }
        else
        {
            Log(LogLevel::Error, "savestate: section %s not found. blarg\n", magic);
            Error = true;
        }
    }
}

void Savestate::Bool32(bool* var)
{
    // for compatibility
    if (Saving)
    {
        u32 val = *var;
        Var32(&val);
    }
    else
    {
        u32 val;
        Var32(&val);
        *var = val != 0;
    }
}

void Savestate::VarArray(void* data, u32 len)
{
    if (Error || finished) return;

    assert(buffer_offset <= buffer_length);

    if (Saving)
    {
        if (buffer_offset + len > buffer_length)
        { // If writing the given data would take us past the buffer's end...
            Log(LogLevel::Warn, "savestate: %u-byte write would exceed %u-byte savestate buffer\n", len, buffer_length);

            if (!(buffer_owned && Resize(buffer_length * 2 + len)))
            { // If we're not allowed to resize this buffer, or if we are but failed...
                Log(LogLevel::Error, "savestate: Failed to write %d bytes to savestate\n", len);
                Error = true;
                return;
            }
            // The buffer's length is doubled, plus however much memory is needed for this write.
            // This way we can write the data and reduce the chance of needing to resize again.
        }

        memcpy(buffer + buffer_offset, data, len);
    }
    else
    {
        if (buffer_offset + len > buffer_length)
        { // If reading the requested amount of data would take us past the buffer's edge...
            Log(LogLevel::Error, "savestate: %u-byte read would exceed %u-byte savestate buffer\n", len, buffer_length);
            Error = true;
            return;

            // Can't realloc here.
            // Not only do we not own the buffer pointer (when loading a state),
            // but we can't magically make the desired data appear.
        }

        memcpy(data, buffer + buffer_offset, len);
    }

    buffer_offset += len;
}

void Savestate::Finish()
{
    if (Error || finished) return;
    CloseCurrentSection();
    WriteStateLength();
    finished = true;
}

void Savestate::Rewind(bool save)
{
    Error = false;
    Saving = save;
    CurSection = NO_SECTION;

    buffer_offset = 0;
    finished = false;
}

void Savestate::CloseCurrentSection()
{
    if (CurSection != NO_SECTION && !finished)
    { // If we're in the middle of writing a section...

        // Go back to the section's header
        // Get the length of the section we've written thus far
        u32 section_length = buffer_offset - CurSection;

        // Write the length in the section's header
        // (specifically the first 4 bytes after the magic number)
        memcpy(buffer + CurSection + 4, &section_length, sizeof(section_length));

        CurSection = NO_SECTION;
    }
}

bool Savestate::Resize(u32 new_length)
{
    if (!buffer_owned)
    { // If we're not allowed to resize this buffer...
        Log(LogLevel::Error, "savestate: Buffer is externally-owned, cannot resize it\n");
        return false;
    }

    u32 old_length = buffer_length;
    void* resized = realloc(buffer, new_length);
    if (!resized)
    { // If the buffer couldn't be expanded...
        Log(LogLevel::Error, "savestate: Failed to resize owned savestate buffer from %dB to %dB\n", old_length, new_length);
        return false;
    }

    u32 length_diff = new_length - old_length;
    buffer = static_cast<u8 *>(resized);
    buffer_length = new_length;

    Log(LogLevel::Debug, "savestate: Expanded %uB savestate buffer to %uB\n", old_length, new_length);
    // Zero out the newly-allocated memory (to ensure we don't introduce a security hole)
    memset(buffer + old_length, 0, length_diff);
    return true;
}

void Savestate::WriteSavestateHeader()
{
    // The magic number
    VarArray((void *) SAVESTATE_MAGIC, 4);

    // The major and minor versions
    u16 major = SAVESTATE_MAJOR;
    Var16(&major);

    u16 minor = SAVESTATE_MINOR;
    Var16(&minor);

    // The next 4 bytes are the file's length, which will be filled in at the end
    u32 zero = 0;
    Var32(&zero);

    // The following 4 bytes are reserved
    Var32(&zero);
}

void Savestate::WriteStateLength()
{
    // Not to be confused with the buffer length.
    // The buffer might not be full,
    // so we don't want to write out the extra stuff.
    u32 state_length = buffer_offset;

    // Write the length in the header
    memcpy(buffer + 0x08, &state_length, sizeof(state_length));
}

u32 Savestate::FindSection(const char* magic) const
{
    if (!magic) return NO_SECTION;

    // Start looking at the savestate's beginning, right after its global header
    // (we can't start from the current offset because then we'd lose the ability to rearrange sections)

    for (u32 offset = 0x10; offset < buffer_length;)
    { // Until we've found the desired section...

        // Get this section's magic number
        char read_magic[4] = {0};
        memcpy(read_magic, buffer + offset, sizeof(read_magic));

        if (memcmp(read_magic, magic, sizeof(read_magic)) == 0)
        { // If this is the right section...
            return offset + 16; // ...return the offset of the first byte of the section after the header
        }

        // Haven't found our section yet. Let's move on to the next one.

        u32 section_length_offset = offset + sizeof(read_magic);
        if (section_length_offset >= buffer_length)
        { // If trying to read the section length would take us past the file's end...
            break;
        }

        // First we need to find out how big this section is...
        u32 section_length = 0;
        memcpy(&section_length, buffer + section_length_offset, sizeof(section_length));

        // ...then skip it. (The section length includes the 16-byte header.)
        offset += section_length;
    }

    // We've reached the end of the file without finding the requested section...
    Log(LogLevel::Error, "savestate: section %s not found. blarg\n", magic);
    return NO_SECTION;
}