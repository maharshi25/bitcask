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

#pragma once

#include <stdalign.h>
#include <stddef.h>

#include "utils.h"

#define new(...) newx(__VA_ARGS__, new4, new3, new2)(__VA_ARGS__)
#define newx(a, b, c, d, e, ...) e
#define new2(a, t) (t *)alloc(a, sizeof(t), alignof(t), 1, 0)
#define new3(a, t, n) (t *)alloc(a, sizeof(t), alignof(t), n, 0)
#define new4(a, t, n, f) (t *)alloc(a, sizeof(t), alignof(t), n, f)

#define NOZERO 1

typedef struct {
  char *beg;
  char *end;
} Arena;

void *alloc(Arena *a, isize size, isize align, isize count, i8 flags);
