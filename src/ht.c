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

#include "ht.h"

#include <stdio.h>
#include <string.h>

private u64 hash(isize ht_capacity, s8 key);
private void copyKeyToHt(HashTable *ht, KvPair kv_pair, isize index, u8 inc_len);

HashTableResult ht_create(Arena *arena, isize ht_capacity) {
  HashTableResult ht_res = {.ht = {0}, .is_ok = false};
  return_value_if(ht_capacity == 0 || (ht_capacity & (ht_capacity - 1)) != 0, ht_res,
                  ERR_INVALID_SIZE);

  HashTable *ht = &ht_res.ht;
  ht->len = 0;
  ht->capacity = ht_capacity;
  ht->kv_pairs = new(arena, KvPair, ht_capacity, NOZERO);

  ht_res.is_ok = true;
  return ht_res;
}

bool ht_insert(HashTable *ht, s8 key, KeyDirEntry val) {
  KvPair kv_pair = {.key = key, .val = val, .is_occupied = false};

  u64 index = hash(ht->capacity, kv_pair.key);

  //if (ht->kv_pairs[index].is_occupied != 0 || ht->kv_pairs[index].is_occupied != 1) printf("%s\n", key.data);
  if (!ht->kv_pairs[index].is_occupied) {
    copyKeyToHt(ht, kv_pair, index, 1);
    return true;
  }

  while (ht->kv_pairs[index].is_occupied) {
    if (s8cmp(kv_pair.key, ht->kv_pairs[index].key)) {
      copyKeyToHt(ht, kv_pair, index, 0);
      return true;
    }
    index = (index + 1) & (ht->capacity - 1);
  }

  copyKeyToHt(ht, kv_pair, index, 1);
  return true;
}

KeyDirEntry *ht_get(HashTable *ht, s8 key) {
  u64 index = hash(ht->capacity, key);

  while (ht->kv_pairs[index].is_occupied) {
    if (s8cmp(key, ht->kv_pairs[index].key)) {
      return &ht->kv_pairs[index].val;
    }

    index = (index + 1) & (ht->capacity - 1);
  }

  return NULL;
}

private u64 hash(isize ht_capacity, s8 key) {
  u64 hash = 0XCBF29CE484222325;

  for (isize i = 0; i < key.len; i++) {
    hash ^= key.data[i];
    hash *= 0x00000100000001B3;
  }

  return (hash) & (ht_capacity - 1);
}

private void copyKeyToHt(HashTable *ht, KvPair kv_pair, isize index, u8 inc_len) {
  memcpy(ht->kv_pairs + index, &kv_pair, sizeof(s8) + sizeof(KeyDirEntry));
  ht->len += inc_len;
  ht->kv_pairs[index].is_occupied = true;
}
