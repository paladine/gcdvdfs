#ifndef __BINARY_READER__H_
#define __BINARY_READER__H_

class BinaryReader {
public:
  BinaryReader() {}
  virtual ~BinaryReader() {}
  virtual int read(void *buf, int size, size_t offset) = 0;
};

class BinaryFILEReader : public BinaryReader {
private:
  FILE *mFile;

public:
  BinaryFILEReader();
  virtual ~BinaryFILEReader();

  bool open(const char *path);

  virtual int read(void *buf, int size, size_t offset);
};

#endif
