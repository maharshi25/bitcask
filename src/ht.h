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
#include <stddef.h>

#include <linux/limits.h>

#include "alloc.h"
#include "s8.h"
#include "utils.h"

typedef struct {
  char *file_id;
  isize val_len;
  isize val_pos;
  i64 timestamp;
} KeyDirEntry;

typedef struct {
  s8 key;
  KeyDirEntry val;
  bool is_occupied;
} KvPair;

typedef struct {
  isize len;
  isize capacity;
  KvPair *kv_pairs;
} HashTable;

typedef struct {
  HashTable ht;
  bool is_ok;
} HashTableResult;

HashTableResult ht_create(Arena *arena, isize ht_capacity);
bool ht_insert(HashTable *ht, s8 key, KeyDirEntry val);
KeyDirEntry *ht_get(HashTable *ht, s8 key);
