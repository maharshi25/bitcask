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

#include "s8.h"

#include <string.h>

inline bool s8cmp(s8 a, s8 b) {
  return a.len == b.len && !memcmp(a.data, b.data, a.len);
}
