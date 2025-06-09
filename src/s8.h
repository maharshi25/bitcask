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

#include <stdbool.h>

#include "utils.h"

#define s8(s) \
  (s8) { (char *)s, lengthof(s) }

typedef struct {
  char *data;
  isize len;
} s8;

bool s8cmp(s8 a, s8 b);
