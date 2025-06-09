#include <string.h>
#include <unistd.h>
#include <sys/mman.h>

#include "bitcask.h"
#include "utils.h"

private isize getRamSize(void);

private isize getRamSize(void) {
    isize pages = sysconf(_SC_PHYS_PAGES);
    isize page_size = sysconf(_SC_PAGE_SIZE);

    return_value_if(pages == -1 || page_size == -1, -1, ERR_SYSCONF);
    return_value_if(pages >= PTRDIFF_MAX / page_size, -1, ERR_ARITHEMATIC_OVERFLOW);

    isize ram_size = pages * page_size;

    return ram_size;
 }

int main(void) {
  isize cap = getRamSize();
  char *heap = mmap(NULL, cap, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  return_value_if(heap == MAP_FAILED, -1, ERR_OUT_OF_MEMORY);

  Arena arena = {.beg = heap, .end = heap + cap};

  Options options = {.read_write = true, .sync_on_put = false, .max_file_size = 6000};
  BcHandleResult bc_res = bc_open(arena, s8("./bitcask-test"), options);
  return_value_if(!bc_res.is_ok, -1, ERR_OBJECT_INITIALIZATION_FAILED);

  BcHandle bc = bc_res.bc;

  // Insert values
  for (u32 i = 0; i < 5000; i++) {
    char key[10];
    char val[10];

    snprintf(key, 10, "key%u", i);
    snprintf(val, 10, "val%u", i);

    isize key_len = strnlen(key, 10);
    isize val_len = strnlen(val, 10);

    s8 str_key = {0};
    str_key.data = new (&arena, char, key_len);
    memcpy(str_key.data, key, key_len);
    str_key.len = key_len;

    s8 str_val = {0};
    str_val.data = new (&arena, char, val_len);
    memcpy(str_val.data, val, val_len);
    str_val.len = val_len;

    bc_put(&bc, str_key, str_val);
  }

  // Get value
  s8 val = bc_get(&bc, s8("key4444"));
  return_value_if(!s8cmp(val, s8("val4444")), -1, "values are not equal.\n");

  // Delete values
  for (u32 i = 0; i < 5000; i += 2) {
    char key[10];

    snprintf(key, 10, "key%u", i);

    isize key_len = strnlen(key, 10);

    s8 str_key = {0};
    str_key.data = new (&arena, char, key_len);
    memcpy(str_key.data, key, key_len);
    str_key.len = key_len;

    bc_delete(&bc, str_key);
  }

  // Merge values
  bc_merge(&bc);

  bc_sync(&bc);

  // Get value
  s8 val2 = bc_get(&bc, s8("key1"));
  return_value_if(!s8cmp(val2, s8("val1")), -1, "values are not equal.\n");

  munmap(heap, cap);
  bc_close(&bc);

  return 0;
}
