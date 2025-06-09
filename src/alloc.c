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

// Code taken from Chris Wellons at nullprogram.com

#include "alloc.h"

#include <string.h>

void *alloc(Arena *a, isize size, isize align, isize count, i8 flags) {
  isize padding = -(uptr)a->beg & (align - 1);
  isize available = a->end - a->beg - padding;
  if (available < 0 || count > available / size) {
    return NULL;
  }
  void *p = a->beg + padding;
  a->beg += padding + count * size;
  return flags & NOZERO ? p : memset(p, 0, count * size);
}
