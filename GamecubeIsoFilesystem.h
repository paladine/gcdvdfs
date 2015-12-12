#ifndef __CONTEXT__H_
#define __CONTEXT__H_

#include <fuse.h>
#include "BinaryReader.h"
#include "GamecubeFilesystemTable.h"
#include <string>

class GamecubeIsoFilesystem {
public:
  static const ino_t ROOT_INO = 1;
  static const ino_t APPLOADER_INO = 2;
  static const ino_t BOOTDOL_INO = 3;
  static const ino_t DATA_INO = 4;

private:
  // Nov 18th, 2001. Date the Gamecube was released in NA!
  static const struct timespec defaultTime;

  static inline GamecubeIsoFilesystem *getContext() {
    return reinterpret_cast<GamecubeIsoFilesystem *>(
        fuse_get_context()->private_data);
  }

  fuse_operations mOperations;
  GamecubeFilesystemTable mFst;
  FILE *mLogFile;
  BinaryReader *mFile;
  uid_t mUid;
  gid_t mGid;
  std::string mLogFilePath;

public:
  GamecubeIsoFilesystem(uid_t uid, gid_t gid, const char *logFile);
  ~GamecubeIsoFilesystem();

  bool open(const char *filePath);

  void log(const char *format, ...);

  fuse_operations *getFuseOperations() { return &mOperations; }

private:
#define FUSE_FUNCTION1(type_ret, name, type_one, one)                          \
  static type_ret static_##name(type_one one) {                                \
    return getContext()->name(one);                                            \
  }                                                                            \
  type_ret name(type_one one)

#define FUSE_FUNCTION2(type_ret, name, type_one, one, type_two, two)           \
  static type_ret static_##name(type_one one, type_two two) {                  \
    return getContext()->name(one, two);                                       \
  }                                                                            \
  type_ret name(type_one one, type_two two)

#define FUSE_FUNCTION3(type_ret, name, type_one, one, type_two, two,           \
                       type_three, three)                                      \
  static type_ret static_##name(type_one one, type_two two,                    \
                                type_three three) {                            \
    return getContext()->name(one, two, three);                                \
  }                                                                            \
  type_ret name(type_one one, type_two two, type_three three)

#define FUSE_FUNCTION5(type_ret, name, type_one, one, type_two, two,           \
                       type_three, three, type_four, four, type_five, five)    \
  static type_ret static_##name(type_one one, type_two two, type_three three,  \
                                type_four four, type_five five) {              \
    return getContext()->name(one, two, three, four, five);                    \
  }                                                                            \
  type_ret name(type_one one, type_two two, type_three three, type_four four,  \
                type_five five)

  FUSE_FUNCTION1(void, destroy, void *, userdata);
  FUSE_FUNCTION2(int, statfs, const char *, path, struct statvfs *, sfs);
  FUSE_FUNCTION3(int, fgetattr, const char *, path, struct stat *, statbuf,
                 struct fuse_file_info *, fi);
  FUSE_FUNCTION2(int, getattr, const char *, path, struct stat *, statbuf);
  FUSE_FUNCTION2(int, opendir, const char *, path, struct fuse_file_info *, fi);
  FUSE_FUNCTION2(int, releasedir, const char *, path, struct fuse_file_info *,
                 fi);
  FUSE_FUNCTION5(int, readdir, const char *, path, void *, buf, fuse_fill_dir_t,
                 filler, off_t, offset, struct fuse_file_info *, fi);
  FUSE_FUNCTION2(int, open, const char *, path, struct fuse_file_info *, fi);
  FUSE_FUNCTION2(int, release, const char *, path, struct fuse_file_info *, fi);
  FUSE_FUNCTION5(int, read, const char *, path, char *, buf, size_t, size,
                 off_t, offset, struct fuse_file_info *, fi);

  void init_statbuf(struct stat *statbuf, ino_t inode);
  ino_t convertPathToInode(const char *path);

  int fgetattr_by_pfe(struct stat *statbuf, gc_dvdfs_file_entry *pfe);
  int fgetattr_by_inode(const char *path, struct stat *statbuf, ino_t inode);

  gc_dvdfs_file_entry *search(gc_dvdfs_file_entry *pfe, const char *name);

  static int readdir_callback(gc_dvdfs_file_entry *pfe, void *param);

  inline ino_t fileEntryToInode(struct gc_dvdfs_file_entry *pfe) const {
    return ((reinterpret_cast<uintptr_t>(pfe) -
             reinterpret_cast<uintptr_t>(mFst.getRoot())) /
                sizeof(struct gc_dvdfs_file_entry) +
            DATA_INO);
  }

  inline struct gc_dvdfs_file_entry *inodeToFileEntry(ino_t inode) const {
    return (mFst.getRoot() + inode - DATA_INO);
  }
};

#endif
