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

#include "bitcask.h"

#include <errno.h>
#include <dirent.h>
#include <linux/limits.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>

#include "alloc.h"
#include "crc64speed.h"
#include "ht.h"
#include "utils.h"

#define HEADER_SIZE sizeof(i64) + 2 * sizeof(isize)
#define VAL_POS_SIZE sizeof(isize)

#define KEY_LEN_OFFSET sizeof(i64)
#define VAL_LEN_OFFSET KEY_LEN_OFFSET + sizeof(isize)
#define KEY_OFFSET VAL_LEN_OFFSET + sizeof(isize)
#define VAL_OFFSET(key_len) KEY_OFFSET + key_len
#define CRC_OFFSET(key_len, val_len) VAL_OFFSET(key_len) + val_len

typedef struct {
  i64 timestamp;
  isize key_len;
  isize val_len;
} Header;

typedef struct {
  Header header;
  char *key;
  char *val;
  u64 crc;

  char *buffer;
  isize buffer_len;
} BcEntry;

private isize getRamSize(void);
private char *getFileName(u32 num);
private bool getNewFileHandle(BcHandle *bc);
private i64 getTimestamp(void);
private Header decodeHeader(char *buffer);
private void encodeEntry(BcEntry bc_entry);
private isize countFiles(char *dir_path);
private bool mergeEntries(BcHandle *bc, char *data_file_path);
private bool getFilePath(char *file_path, char *dir_path, s8 extension, isize num_files);
private bool growKeyDir(BcHandle *bc, isize data_files_num, isize hint_files_num);

#define DATA_FILES s8("data_files")
#define MERGED_FILES  s8("merged_files")
#define HINT_FILES s8("hint_files")

#define BIN_EXT s8("bin")
#define MERGED_EXT s8("merge")
#define HINT_EXT s8("hint")

BcHandleResult bc_open(Arena arena, s8 dir_path, Options options) {
  BcHandleResult bc_res = {.bc = {0}, .is_ok = false};
  BcHandle *bc = &bc_res.bc;

  struct stat st;
  if (stat(dir_path.data, &st) == -1) {
    i8 out = mkdir(dir_path.data, 0700);

    DIR *dirp = opendir(dir_path.data);
    return_value_if(dirp == NULL, bc_res, ERR_ACCESS);

    i8 fd = dirfd(dirp);
    return_value_if(fd == -1, bc_res, ERR_ACCESS);

    out = mkdirat(fd, DATA_FILES.data, 0700);
    out = mkdirat(fd, MERGED_FILES.data, 0700);
    out = mkdirat(fd, HINT_FILES.data, 0700);

    return_value_if(out == -1, bc_res, ERR_ACCESS);

    closedir(dirp);
  }

  snprintf(bc->data_dir_path, dir_path.len + DATA_FILES.len + 2, "%s/%s", dir_path.data,
           DATA_FILES.data);
  snprintf(bc->merged_dir_path, dir_path.len + MERGED_FILES.len + 2, "%s/%s", dir_path.data,
           MERGED_FILES.data);
  snprintf(bc->hint_dir_path, dir_path.len + HINT_FILES.len + 2, "%s/%s", dir_path.data,
           HINT_FILES.data);

  memcpy(bc->parent_dir_path, dir_path.data, dir_path.len);

  crc64speed_init();
  bc->arena = arena;
  bc->options = options;
  bc->num_files = countFiles(bc->data_dir_path);


  bool out = getFilePath(bc->active_file_path, bc->data_dir_path, BIN_EXT, bc->num_files);

  return_value_if(bc->num_files == -1, bc_res, ERR_ACCESS);
  return_value_if(!out, bc_res, ERR_ACCESS);

  bc->active_fp = fopen(bc->active_file_path, "ab+");
  return_value_if(bc->active_fp == NULL, bc_res, ERR_ACCESS);

  isize cap = (isize)1 << 21;
  HashTableResult ht_res = ht_create(&bc->arena, cap);
  bc->key_dir = ht_res.ht;
  return_value_if(!ht_res.is_ok, bc_res, ERR_OBJECT_INITIALIZATION_FAILED);

  i8 res = stat(bc->active_file_path, &st);
  return_value_if(res == -1, bc_res, ERR_ACCESS);
  bc->cursor = st.st_size;

  isize hint_files_num = countFiles(bc->hint_dir_path);
  return_value_if(hint_files_num == -1, bc_res, ERR_ACCESS);

  out = growKeyDir(bc, bc->num_files, hint_files_num);
  return_value_if(!out, bc_res, ERR_OBJECT_INITIALIZATION_FAILED);

  if (options.read_write) {
    i8 res = flock(bc->active_fp->_fileno, LOCK_SH);
    return_value_if(res == -1, bc_res, ERR_ACCESS);
  }

  bc_res.is_ok = true;
  return bc_res;
}

void bc_close(BcHandle *bc) { fclose(bc->active_fp); }

s8 bc_get(BcHandle *bc, s8 key) {
  s8 null_s8 = {.data = NULL, .len = -1};

  KeyDirEntry *kd_entry = ht_get(&bc->key_dir, key);
  return_value_if(kd_entry == NULL, null_s8, ERR_KEY_MISSING);

  FILE *fp;
  i8 out = strcmp(kd_entry->file_id, bc->active_file_path);
  if (out == 0) {
    fp = bc->active_fp;
  } else {
    fp = fopen(kd_entry->file_id, "r");
    return_value_if(fp == NULL, null_s8, ERR_ACCESS);
  }
  i8 res = fseek(fp, kd_entry->val_pos, SEEK_SET);

  return_value_if(res == -1, null_s8, ERR_ACCESS);
  return_value_if(key.len >= PTRDIFF_MAX - HEADER_SIZE - kd_entry->val_len, null_s8,
                  ERR_ARITHEMATIC_OVERFLOW);

  BcEntry bc_entry = {0};
  bc_entry.buffer_len = HEADER_SIZE + sizeof(u64) + key.len + kd_entry->val_len;
  bc_entry.buffer = new (&bc->arena, char, bc_entry.buffer_len);
  bc_entry.val = new (&bc->arena, char, kd_entry->val_len);

  return_value_if(bc_entry.buffer == NULL, null_s8, ERR_OUT_OF_MEMORY);
  return_value_if(bc_entry.val == NULL, null_s8, ERR_OUT_OF_MEMORY);

  isize bytes_read = fread(bc_entry.buffer, sizeof(char), bc_entry.buffer_len, fp);
  u64 crc = crc64speed(0, bc_entry.buffer, bc_entry.buffer_len);

  return_value_if(crc != 0, null_s8, ERR_CRC_FAILED);
  return_value_if(bytes_read < bc_entry.buffer_len, null_s8, ERR_ACCESS);

  memcpy(bc_entry.val, bc_entry.buffer + VAL_OFFSET(key.len), kd_entry->val_len);

  if (out != 0) fclose(fp);

  s8 val = {.data = bc_entry.val, .len = kd_entry->val_len};

  return_value_if(s8cmp(s8("🪦"), val), null_s8, ERR_KEY_MISSING);

  return val;
}

bool bc_put(BcHandle *bc, s8 key, s8 val) {
  return_value_if(!bc->options.read_write, false, ERR_READ_ONLY);

  if (bc->cursor >= bc->options.max_file_size) {
    bool out = getNewFileHandle(bc);
    return_value_if(!out, false, ERR_ACCESS);
  }

  Header header = {
    .timestamp = getTimestamp(),
    .key_len = key.len,
    .val_len = val.len,
  };

  BcEntry bc_entry = {
      .header = header,
      .key = key.data,
      .val = val.data,
      .crc = 0,
  };

  return_value_if(key.len >= PTRDIFF_MAX - HEADER_SIZE - val.len, false, ERR_ARITHEMATIC_OVERFLOW);

  bc_entry.buffer_len = HEADER_SIZE + sizeof(u64) + key.len + val.len;
  bc_entry.buffer = new (&bc->arena, char, bc_entry.buffer_len);

  return_value_if(bc_entry.buffer == NULL, false, ERR_OUT_OF_MEMORY);


  KeyDirEntry kd_entry = {
      .timestamp = bc_entry.header.timestamp,
      .val_len = bc_entry.header.val_len,
      .val_pos = bc->cursor,
  };
  kd_entry.file_id = new(&bc->arena, char, PATH_MAX);
  memcpy(kd_entry.file_id, bc->active_file_path, PATH_MAX);

  encodeEntry(bc_entry);

  fwrite(bc_entry.buffer, sizeof(char), bc_entry.buffer_len, bc->active_fp);

  bool res = ht_insert(&bc->key_dir, key, kd_entry);
  return_value_if(!res, false, ERR_KEY_INSERT_FAILED);

  return_value_if(bc->cursor >= PTRDIFF_MAX - bc_entry.buffer_len, false, ERR_ARITHEMATIC_OVERFLOW);
  bc->cursor += bc_entry.buffer_len;

  if (bc->options.sync_on_put) {
    i8 out = fflush(bc->active_fp);
    return_value_if(out != 0, false, ERR_ACCESS);
  }

  return true;
}

bool bc_delete(BcHandle *bc, s8 key) {
  KeyDirEntry *kd_entry = ht_get(&bc->key_dir, key);

  return_value_if(kd_entry == NULL, false, ERR_KEY_MISSING);

  i8 out = bc_put(bc, key, s8("🪦"));
  return_value_if(!out, false, ERR_KEY_DELETE_FAILED);

  return true;
}

bool bc_merge(BcHandle *bc) {
  return_value_if(bc->num_files < 2, false, ERR_MERGE);

  for (isize i = 1; i < bc->num_files; i++) {
    char data_file[PATH_MAX] = {0};
    getFilePath(data_file, bc->data_dir_path, BIN_EXT, i);
    mergeEntries(bc, data_file);
  }

  char new_name[PATH_MAX];
  getFilePath(new_name, bc->data_dir_path, BIN_EXT, 0);
  i8 out = rename(bc->active_file_path, new_name);
  return_value_if(out == -1, false, ERR_ACCESS);

  return true;
}

bool bc_sync(BcHandle *bc) {
  i8 out = fflush(bc->active_fp);
  return_value_if(out == EOF, false, ERR_ACCESS);
  return true;
}

private isize countFiles(char *dir_path) {
  DIR *dirp = opendir(dir_path);
  return_value_if(dirp == NULL, -1, ERR_ACCESS);

  errno = 0;
  struct dirent *entry;
  isize num_files = 0;
  while ((entry = readdir(dirp)) != NULL) {
    if (entry->d_type == DT_REG) {
      num_files++;
    }
  }

  return_value_if(errno != 0, -1, ERR_ACCESS);

  closedir(dirp);

  return num_files;
}

private Header decodeHeader(char *buffer) {
  Header header = {0};
  memcpy(&header.timestamp, buffer, sizeof(i64));
  memcpy(&header.key_len, buffer + KEY_LEN_OFFSET, sizeof(isize));
  memcpy(&header.val_len, buffer + VAL_LEN_OFFSET, sizeof(isize));
  return header;
}

private void encodeEntry(BcEntry bc_entry) {
  memcpy(bc_entry.buffer, &bc_entry.header.timestamp, sizeof(i64));
  memcpy(bc_entry.buffer + KEY_LEN_OFFSET, &bc_entry.header.key_len, sizeof(isize));
  memcpy(bc_entry.buffer + VAL_LEN_OFFSET, &bc_entry.header.val_len, sizeof(isize));
  memcpy(bc_entry.buffer + KEY_OFFSET, bc_entry.key, bc_entry.header.key_len);
  memcpy(bc_entry.buffer + VAL_OFFSET(bc_entry.header.key_len), bc_entry.val,
         bc_entry.header.val_len);

  bc_entry.crc = crc64speed(0, bc_entry.buffer, bc_entry.buffer_len - sizeof(u64));
  memcpy(bc_entry.buffer + CRC_OFFSET(bc_entry.header.key_len, bc_entry.header.val_len),
         &bc_entry.crc, sizeof(u64));
}

private bool getFilePath(char *file_path, char *dir_path, s8 extension, isize num_files) {
  isize dir_path_len = strnlen(dir_path, PATH_MAX);
  u8 file_len = 14 + extension.len;

  return_value_if(dir_path_len >= PTRDIFF_MAX - file_len, false, ERR_ARITHEMATIC_OVERFLOW);
  isize file_path_len = dir_path_len + file_len;

  if (num_files == 0) num_files = 1;
  snprintf(file_path, file_path_len, "%s/%s.%s", dir_path, getFileName(num_files), extension.data);

  return true;
}

private char *getFileName(u32 num) {
  static char hex_num[16];
  snprintf(hex_num, 16, "%0*X", 8, num);
  return hex_num;
}

private bool getNewFileHandle(BcHandle *bc) {
  fclose(bc->active_fp);

  bool out = getFilePath(bc->active_file_path, bc->data_dir_path, BIN_EXT, bc->num_files + 1);
  bc->active_fp = fopen(bc->active_file_path, "ab+");

  return_value_if(!out || bc->active_fp == NULL, false, ERR_ACCESS);
  return_value_if(bc->num_files > PTRDIFF_MAX - 1, false, ERR_ARITHEMATIC_OVERFLOW);
  bc->num_files++;
  bc->cursor = 0;

  if (bc->options.read_write) {
    i8 out = flock(bc->active_fp->_fileno, LOCK_SH);
    return_value_if(out == -1, false, ERR_ACCESS);
  }

  return true;
}

private isize getRamSize(void) {
    isize pages = sysconf(_SC_PHYS_PAGES);
    isize page_size = sysconf(_SC_PAGE_SIZE);

    return_value_if(pages == -1 || page_size == -1, -1, ERR_SYSCONF);
    return_value_if(pages >= PTRDIFF_MAX / page_size, -1, ERR_ARITHEMATIC_OVERFLOW);

    isize ram_size = pages * page_size;

    return ram_size;
}

private i64 getTimestamp(void) {
  struct timeval tv;
  i8 out = gettimeofday(&tv, NULL);
  return_value_if(out == -1, -1, "gettimeofday failed.\n");
  return tv.tv_sec;
}

private bool growKeyDir(BcHandle *bc, isize data_files_num, isize hint_files_num) {
  char header_buffer[HEADER_SIZE];

  for (isize i = 1; i <= data_files_num; i++) {
    char file_path[PATH_MAX] = {0};
    bool out = getFilePath(file_path, bc->data_dir_path,BIN_EXT, i);
    FILE *fp = fopen(file_path, "r");

    isize val_len = 0;
    isize crc_len = 0;

    return_value_if(!out, false, ERR_OBJECT_INITIALIZATION_FAILED);
    return_value_if(fp == NULL, false, ERR_ACCESS);

    while (true) {
      i8 out = fseek(fp, val_len + crc_len, SEEK_CUR);
      isize val_pos = ftell(fp);

      return_value_if(out == -1 || val_pos == -1, false, ERR_ACCESS);

      isize header_bytes_read = fread(header_buffer, sizeof(char), HEADER_SIZE, fp);
      if (header_bytes_read < HEADER_SIZE) break;

      Header header = decodeHeader(header_buffer);

      char *key_data = new (&bc->arena, char, header.key_len);
      return_value_if(key_data == NULL, false, ERR_OUT_OF_MEMORY);

      isize bytes_read = fread(key_data, sizeof(char), header.key_len, fp);
      if (bytes_read < header.key_len) break;

      s8 key = {.data = key_data, .len = header.key_len};

      KeyDirEntry kd_entry = {
          .timestamp = header.timestamp,
          .val_len = header.val_len,
          .val_pos = val_pos,
      };
      kd_entry.file_id = new(&bc->arena, char, PATH_MAX);
      memcpy(kd_entry.file_id, file_path, PATH_MAX);

      bool res = ht_insert(&bc->key_dir, key, kd_entry);
      val_len = header.val_len;
      crc_len = sizeof(u64);

      return_value_if(!res, false, ERR_KEY_INSERT_FAILED);
    }

    fclose(fp);
  }

  for (isize i = 1; i <= hint_files_num; i++) {
    char hint_file_path[PATH_MAX] = {0};
    bool out = getFilePath(hint_file_path, bc->hint_dir_path, HINT_EXT, i);
    FILE *fp = fopen(hint_file_path, "r");

    return_value_if(!out, false, ERR_OBJECT_INITIALIZATION_FAILED);
    return_value_if(fp == NULL, false, ERR_ACCESS);

    char merged_file_path[PATH_MAX] = {0};
    out = getFilePath(merged_file_path, bc->merged_dir_path, MERGED_EXT, i);
    return_value_if(!out, false, ERR_OBJECT_INITIALIZATION_FAILED);

    while (true) {
      isize header_bytes_read = fread(header_buffer, sizeof(char), HEADER_SIZE, fp);
      if (header_bytes_read < HEADER_SIZE) break;

      Header header = decodeHeader(header_buffer);

      isize val_pos = 0;
      isize val_pos_bytes_read = fread(&val_pos, sizeof(char), VAL_POS_SIZE, fp);
      if (val_pos_bytes_read < VAL_POS_SIZE) break;

      char *key_data = new (&bc->arena, char, header.key_len);
      return_value_if(key_data == NULL, false, ERR_OUT_OF_MEMORY);

      isize bytes_read = fread(key_data, sizeof(char), header.key_len, fp);
      if (bytes_read < header.key_len) break;

      s8 key = {.data = (char *)key_data, .len = header.key_len};

      KeyDirEntry kd_entry = {
          .timestamp = header.timestamp,
          .val_len = header.val_len,
          .val_pos = val_pos,
      };

      kd_entry.file_id = new(&bc->arena, char, PATH_MAX);
      memcpy(kd_entry.file_id, merged_file_path, PATH_MAX);

      bool res = ht_insert(&bc->key_dir, key, kd_entry);
      return_value_if(!res, false, ERR_KEY_INSERT_FAILED);
    }

    fclose(fp);
  }

  return true;
}

private bool mergeEntries(BcHandle *bc, char *data_file_path) {
  isize merged_files_count = countFiles(bc->merged_dir_path);
  return_value_if(merged_files_count == -1, false, ERR_ACCESS);

  char merged_file_path[PATH_MAX];
  char hint_file_path[PATH_MAX];

  bool out = getFilePath(merged_file_path, bc->merged_dir_path, MERGED_EXT, merged_files_count);
  out = getFilePath(hint_file_path, bc->hint_dir_path, HINT_EXT, merged_files_count);

  return_value_if(!out, false, ERR_ACCESS);

  FILE *data_fp = fopen(data_file_path, "rb");
  FILE *merged_fp = fopen(merged_file_path, "ab");
  FILE *hint_fp = fopen(hint_file_path, "ab");

  return_value_if(data_fp == NULL || merged_fp == NULL || hint_fp == NULL, false, ERR_ACCESS);

  struct stat st;
  int res = stat(merged_file_path, &st);
  return_value_if(res == -1, false, ERR_ACCESS);

  char header_buffer[HEADER_SIZE] = {0};

  isize cursor = st.st_size;
  while (true) {
    isize val_pos = ftell(merged_fp);

    isize header_bytes_read = fread(header_buffer, sizeof(char), HEADER_SIZE, data_fp);
    if (header_bytes_read < HEADER_SIZE) break;

    Header header = decodeHeader(header_buffer);

    char *key = new (&bc->arena, char, header.key_len);
    char *val = new (&bc->arena, char, header.val_len);
    return_value_if(key == NULL || val == NULL, false, ERR_OUT_OF_MEMORY);

    s8 s8val = {.data = val, .len = header.val_len};

    isize key_bytes_read = fread(key, sizeof(char), header.key_len, data_fp);
    if (key_bytes_read < header.key_len) break;

    isize val_bytes_read = fread(val, sizeof(char), header.val_len, data_fp);
    if (val_bytes_read < header.val_len) break;

    u64 crc = 0;
    isize crc_bytes_read = fread(&crc, sizeof(char), sizeof(u64), data_fp);
    if (crc_bytes_read < sizeof(u64)) break;

    if (!s8cmp(s8val, s8("🪦"))) {
      fwrite(header_buffer, sizeof(char), HEADER_SIZE, merged_fp);
      fwrite(key, sizeof(char), header.key_len, merged_fp);
      fwrite(val, sizeof(char), header.val_len, merged_fp);
      fwrite(&crc, sizeof(char), sizeof(u64), merged_fp);

      fwrite(header_buffer, sizeof(char), HEADER_SIZE, hint_fp);
      fwrite(&val_pos, sizeof(char), sizeof(isize), hint_fp);
      fwrite(key, sizeof(char), header.key_len, hint_fp);
    }

    return_value_if(header.key_len >= PTRDIFF_MAX - HEADER_SIZE - header.val_len - sizeof(u64),
                    false, ERR_ARITHEMATIC_OVERFLOW);
    return_value_if(
        cursor >= PTRDIFF_MAX - HEADER_SIZE - header.val_len - header.key_len - sizeof(u64), false,
		    ERR_ARITHEMATIC_OVERFLOW);

    cursor += HEADER_SIZE + header.key_len + header.val_len + sizeof(u64);

    if (cursor >= bc->options.max_file_size) {
      fclose(merged_fp);
      fclose(hint_fp);

      getFilePath(merged_file_path, bc->merged_dir_path, MERGED_EXT, merged_files_count + 1);
      getFilePath(hint_file_path, bc->hint_dir_path, HINT_EXT, merged_files_count + 1);

      merged_fp = fopen(merged_file_path, "ab");
      hint_fp = fopen(hint_file_path, "ab");

      return_value_if(merged_fp == NULL || hint_fp == NULL, false, ERR_ACCESS);
    }
  }

  unlink(data_file_path);
  fclose(data_fp);
  fclose(merged_fp);
  fclose(hint_fp);

  return true;
}
