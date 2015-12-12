#include <stdio.h>
#include <unistd.h>
#include "BinaryReader.h"

BinaryFILEReader::BinaryFILEReader() : mFile(nullptr) {}

BinaryFILEReader::~BinaryFILEReader() {
  if (mFile != nullptr) {
    fclose(mFile);
  }
}

bool BinaryFILEReader::open(const char *path) {
  mFile = fopen(path, "rb");
  return mFile != nullptr;
}

int BinaryFILEReader::read(void *buf, int size, size_t offset) {
  return pread(fileno(mFile), buf, size, offset);
}
