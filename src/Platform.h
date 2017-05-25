/*
    Copyright 2016-2017 StapleButter

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

namespace Platform
{

void* Thread_Create(void (*func)());
void Thread_Free(void* thread);
void Thread_Wait(void* thread);

void* Semaphore_Create();
void Semaphore_Free(void* sema);
void Semaphore_Reset(void* sema);
void Semaphore_Wait(void* sema);
void Semaphore_Post(void* sema);

bool MP_Init();
void MP_DeInit();
int MP_SendPacket(u8* data, int len);
int MP_RecvPacket(u8* data, bool block);

}

#endif // PLATFORM_H
