#include <stdio.h>
#include <string.h>

#include "kirk_engine.h"

#define ALIGN(x, align) (((x) + ((align) - 1)) & ~((align) - 1))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

#define MAX_PCBC_SIZE 0x100000

u8 buf[MAX_PCBC_SIZE + 0x140];

u8 pcbc_xor_key[0x40] = {
  0x28, 0x34, 0x06, 0x24, 0x6A, 0x9B, 0x9C, 0x9F,
  0x09, 0x5C, 0xF0, 0x2D, 0x98, 0x30, 0x73, 0x32,
  0xEC, 0xAA, 0x97, 0xCB, 0xAA, 0xC7, 0x0B, 0x10,
  0x12, 0x35, 0xE7, 0x7D, 0x84, 0xA9, 0xFD, 0x16,
  0x91, 0x42, 0xDE, 0xB5, 0xEB, 0xD9, 0x09, 0x84,
  0x6A, 0x18, 0x64, 0xD2, 0xFE, 0xB8, 0x1E, 0xCD,
  0x07, 0x50, 0xD8, 0x63, 0x13, 0x0B, 0xF1, 0x90,
  0x27, 0x9D, 0x46, 0xDB, 0xA0, 0x44, 0x9A, 0xF1
};

int ReadFile(char *file, void *buf, int size) {
  FILE *f = fopen(file, "rb");
  if (!f)
    return -1;

  int rd = fread(buf, 1, size, f);
  fclose(f);

  return rd;
}

int WriteFile(char *file, void *buf, int size) {
  FILE *f = fopen(file, "wb");
  if (!f)
    return -1;

  int wt = fwrite(buf, 1, size, f);
  fclose(f);

  return wt;
}

int main(int argc, char *argv[]) {
  char *mode, *input, *output;
  KIRK_CMD1_HEADER *header;
  int buf_size;
  int size;
  int res;

  if (argc < 3 || argc > 4) {
    printf("Usage: pcbctool {-d|-e} input [output]\n");
    return 1;
  }

  mode = argv[1];
  input = argv[2];
  output = argc == 3 ? argv[2] : argv[3];

  kirk_init();
  memset(buf, 0, sizeof(buf));

  if (strcmp(mode, "-d") == 0) {
    buf_size = ReadFile(input, buf, MAX_PCBC_SIZE);
    if (buf_size < 0) {
      printf("Error: Could not read input %s.\n", input);
      return 1;
    } else if (buf_size < 0x1000) {
      printf("Error: File %s too small.\n", input);
      return 1;
    }

    for (int i = 0; i < sizeof(pcbc_xor_key); i++)
      buf[i] ^= pcbc_xor_key[i];

    res = kirk_CMD1(buf, buf, 0x1000);
    if (res != 0) {
      printf("Error: kirk_CMD1 returned %d\n", res);
      return 1;
    }

    memmove(buf + 0xa0, buf, 0xf60);

    header = (KIRK_CMD1_HEADER *)(buf + 0xa0);
    size = header->data_size;

    res = kirk_CMD1(buf, buf + 0xa0, buf_size);
    if (res != 0) {
      printf("Error: kirk_CMD1 returned %d\n", res);
      return 1;
    }

    res = WriteFile(output, buf, size);
    if (res < 0) {
      printf("Error could not write output %s.\n", output);
      return 1;
    }
  } else if (strcmp(mode, "-e") == 0) {
    buf_size = ReadFile(input, buf, MAX_PCBC_SIZE);
    if (buf_size < 0) {
      printf("Error: Could not read input %s.\n", input);
      return 1;
    }

    size = ALIGN(MAX(buf_size, 0xf60), 0x10);

    memmove(buf + 0xa0, buf, size);

    memset(buf, 0, 0xa0);
    header = (KIRK_CMD1_HEADER *)buf;
    header->mode = KIRK_MODE_CMD1;
    header->ecdsa = 1;
    header->data_offset = 0x10;
    header->data_size = size;

    res = kirk_CMD0(buf, buf, 0xa0 + size);
    if (res != 0) {
      printf("Error: kirk_CMD1 returned %d\n", res);
      return 1;
    }

    memmove(buf + 0xa0, buf, 0xa0 + size);

    memset(buf, 0, 0xa0);
    header = (KIRK_CMD1_HEADER *)buf;
    header->mode = KIRK_MODE_CMD1;
    header->ecdsa = 1;
    header->data_offset = 0x10;
    header->data_size = 0xf60;

    res = kirk_CMD0(buf, buf, 0x1000);
    if (res != 0) {
      printf("Error: kirk_CMD1 returned %d\n", res);
      return 1;
    }

    for (int i = 0; i < sizeof(pcbc_xor_key); i++)
      buf[i] ^= pcbc_xor_key[i];

    res = WriteFile(output, buf, 0x140 + size);
    if (res < 0) {
      printf("Error could not write output %s.\n", output);
      return 1;
    }
  }

  return 0;
}
