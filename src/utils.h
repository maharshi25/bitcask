/*
Copyright 2024 Maharshi Shukla

This file is part of bitcask.

bitcask is free software: you can redistribute it and/or modify it under the terms of the GNU
General Public License as published by the Free Software Foundation, either version 3 of the
License, or (at your option) any later version.

bitcask is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even
the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
Public License for more details.

You should have received a copy of the GNU General Public License along with bitcask. If not, see
<https://www.gnu.org/licenses/>. */

#pragma once

#include <stddef.h>
#include <stdint.h>

typedef int8_t i8;
typedef uint8_t u8;
typedef int32_t i32;
typedef uint32_t u32;
typedef int64_t i64;
typedef uint64_t u64;
typedef ptrdiff_t isize;
typedef uintptr_t uptr;

#define private static inline

#define countof(a) (ptrdiff_t)(sizeof(a) / sizeof(*(a)))
#define lengthof(s) (countof(s) - 1)


#define ERR_SYSCONF "Sysconf failed.\n"
#define ERR_MERGE "No files to merge.\n"
#define ERR_CRC_FAILED "Failed to verify CRC\n"
#define ERR_KEY_INSERT_FAILED "Cannot insert key.\n"
#define ERR_KEY_DELETE_FAILED "Cannot delete key.\n"
#define ERR_KEY_MISSING "Attempt to access a key which is either deleted or does not exist.\n"
#define ERR_ACCESS "Cannot access file or directory.\n"
#define ERR_READ_ONLY "Cannot write to a read only database.\n"
#define ERR_OUT_OF_MEMORY "Out of memory (allocation failed)\n"
#define ERR_OBJECT_INITIALIZATION_FAILED "Failed to initialize object\n"
#define ERR_ARITHEMATIC_OVERFLOW "An arithematic operation caused an overflow (result > MAX)\n"
#define ERR_INVALID_SIZE "Invalid capacity size provided (capacity should be a power of 2 and > 0)\n"

#define return_value_if(cond, value, ...) \
  do {				  \
    if ((cond)) {                         \
      fprintf(stderr, "%s:", __func__); \
      fprintf(stderr, "%d", __LINE__);  \
      fputs(" : ", stderr);               \
      fprintf(stderr, __VA_ARGS__);       \
      return value;                       \
    }                                     \
  } while (0)
