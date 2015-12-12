#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <algorithm>
#include "GamecubeIsoFilesystem.h"
#include "Tokenizer.h"

const struct timespec GamecubeIsoFilesystem::defaultTime = {1006095600, 0};

struct root_dir_entry {
  const char *name;
  ino_t inode;
};

static const struct root_dir_entry root_dir_entries[] = {
    {"apploader", GamecubeIsoFilesystem::APPLOADER_INO},
    {"boot.dol", GamecubeIsoFilesystem::BOOTDOL_INO},
    {"data", GamecubeIsoFilesystem::DATA_INO}};

static const unsigned int num_root_dir_entries =
    sizeof(root_dir_entries) / sizeof(root_dir_entries[0]);

GamecubeIsoFilesystem::GamecubeIsoFilesystem(uid_t uid, gid_t gid,
                                             const char *logFile)
    : mLogFile(nullptr), mFile(nullptr), mUid(uid), mGid(gid),
      mLogFilePath(logFile) {
  memset(&mOperations, 0, sizeof(mOperations));

  mOperations.destroy = static_destroy;
  mOperations.statfs = static_statfs;
  mOperations.fgetattr = static_fgetattr;
  mOperations.getattr = static_getattr;
  mOperations.opendir = static_opendir;
  mOperations.releasedir = static_releasedir;
  mOperations.readdir = static_readdir;
  mOperations.open = static_open;
  mOperations.release = static_release;
  mOperations.read = static_read;
}

GamecubeIsoFilesystem::~GamecubeIsoFilesystem() {
  if (mLogFile) {
    fclose(mLogFile);
  }

  if (mFile) {
    delete mFile;
  }
}

bool GamecubeIsoFilesystem::open(const char *filePath) {
  if (!mLogFilePath.empty()) {
    mLogFile = fopen(mLogFilePath.c_str(), "w");
    if (mLogFile == nullptr) {
      fprintf(stderr, "Unable to open log file %s\n", mLogFilePath.c_str());
      return false;
    }
  }

  log("Attempting to open %s\n", filePath);

  BinaryFILEReader *reader = new BinaryFILEReader();
  if (reader == nullptr) {
    log("Unable to allocate BinaryFILEReader\n");
    return false;
  }

  if (!reader->open(filePath)) {
    log("Unable to open %s\n", filePath);
    delete reader;
    return false;
  }

  if (!mFst.open(reader)) {
    log("Unable to read FST from %s\n", filePath);
    delete reader;
    return false;
  }

  mFile = reader;
  log("Successfully opened %s\n", filePath);
  return true;
}

void GamecubeIsoFilesystem::log(const char *format, ...) {
  if (mLogFile) {
    va_list ap;
    va_start(ap, format);

    vfprintf(mLogFile, format, ap);
    fflush(mLogFile);
  }
}

void GamecubeIsoFilesystem::destroy(void *userdata) { delete this; }

int GamecubeIsoFilesystem::statfs(const char *path, struct statvfs *sfs) {
  log("statfs %s\n", path);

  sfs->f_bsize = GC_DVD_SECTOR_SIZE;
  sfs->f_frsize = 512;
  sfs->f_blocks = mFst.getTotalFileSize() / sfs->f_frsize;
  sfs->f_bfree = 0;
  sfs->f_bavail = 0;
  sfs->f_files = mFst.getTotalFiles();
  sfs->f_ffree = 0;
  sfs->f_favail = 0;
  sfs->f_fsid = 0;
  sfs->f_namemax = 256;
  return 0;
}

struct search_callback_data {
  GamecubeFilesystemTable *fileSystem;
  const char *name;
  gc_dvdfs_file_entry *found;
};

static int search_callback(gc_dvdfs_file_entry *pfe, void *param) {
  search_callback_data *const data =
      reinterpret_cast<search_callback_data *>(param);

  if (strcmp(data->name, data->fileSystem->getFileName(pfe)) == 0) {
    data->found = pfe;
    return -1;
  }
  return 0;
}

gc_dvdfs_file_entry *GamecubeIsoFilesystem::search(gc_dvdfs_file_entry *pfe,
                                                   const char *name) {
  search_callback_data data;

  if (pfe->type != FST_DIRECTORY) {
    return nullptr;
  }

  data.fileSystem = &mFst;
  data.name = name;
  data.found = nullptr;

  mFst.enumerate(pfe, search_callback, &data);
  return data.found;
}

// Returns 0 on error
ino_t GamecubeIsoFilesystem::convertPathToInode(const char *path) {
  if (!strcmp(path, "/")) {
    return ROOT_INO;
  } else if (!strcmp(path, "/apploader")) {
    return APPLOADER_INO;
  } else if (!strcmp(path, "/boot.dol")) {
    return BOOTDOL_INO;
  } else if (!strcmp(path, "/data")) {
    return DATA_INO;
  } else {
    Tokenizer tokenizer(path);
    auto const tokens = tokenizer.getTokens();

    if (tokens.size() <= 0) {
      log("convertPathToInode failed for %s\n", path);
      return 0;
    }
    // grab first, ensure it's data
    if (strcmp("data", tokens.front())) {
      log("convertPathToInode failed for %s\n", path);
      return 0;
    }

    // walk the directory searching for the path
    gc_dvdfs_file_entry *root = mFst.getRoot();
    for (auto iter = tokens.begin() + 1; iter != tokens.end(); ++iter) {
      if ((root = search(root, *iter)) == nullptr) {
        log("convertPathToInode failed for %s\n", path);
        return 0;
      }
    }
    return fileEntryToInode(root);
  }
}

void GamecubeIsoFilesystem::init_statbuf(struct stat *statbuf, ino_t inode) {
  memset(statbuf, 0, sizeof(struct stat));
  statbuf->st_ino = inode;
  statbuf->st_atim = defaultTime;
  statbuf->st_mtim = defaultTime;
  statbuf->st_ctim = defaultTime;
  statbuf->st_mode = 0444;
  statbuf->st_nlink = 1;
  statbuf->st_uid = mUid;
  statbuf->st_gid = mGid;
}

int GamecubeIsoFilesystem::fgetattr_by_pfe(struct stat *statbuf,
                                           gc_dvdfs_file_entry *pfe) {
  init_statbuf(statbuf, fileEntryToInode(pfe));
  if (pfe->type == FST_DIRECTORY) {
    struct gc_dvdfs_directory_info di;
    // get information about the directory
    mFst.getDirectoryInfo(pfe, &di);

    statbuf->st_mode |= S_IFDIR | 0111;
    statbuf->st_nlink = /*2 +*/ di.total_directories;
  } else {
    statbuf->st_mode |= S_IFREG;
    statbuf->st_size = pfe->file.length;
    statbuf->st_blocks = pfe->file.length / 512;
  }
  return 0;
}

int GamecubeIsoFilesystem::fgetattr_by_inode(const char *path,
                                             struct stat *statbuf,
                                             ino_t inode) {
  if (path) {
    log("fgetattr %s:%d\n", path, inode);
  } else {
    log("fgetattr %d\n", inode);
  }

  switch (inode) {
  case ROOT_INO:
    init_statbuf(statbuf, inode);
    statbuf->st_mode |= S_IFDIR | 0111;
    statbuf->st_nlink = num_root_dir_entries;
    break;
  case APPLOADER_INO:
    init_statbuf(statbuf, inode);
    statbuf->st_mode |= S_IFREG;
    statbuf->st_size = mFst.getApploader().size;
    statbuf->st_blocks = statbuf->st_size / 512;
    break;
  case BOOTDOL_INO:
    init_statbuf(statbuf, inode);
    statbuf->st_mode |= S_IFREG;
    statbuf->st_size = mFst.getDolLength();
    statbuf->st_blocks = statbuf->st_size / 512;
    break;
  default:
    return fgetattr_by_pfe(statbuf, inodeToFileEntry(inode));
  }
  return 0;
}

int GamecubeIsoFilesystem::fgetattr(const char *path, struct stat *statbuf,
                                    struct fuse_file_info *fi) {
  log("fgetattr %s\n", path);

  return fgetattr_by_inode(path, statbuf, fi->fh);
}

int GamecubeIsoFilesystem::getattr(const char *path, struct stat *statbuf) {
  log("getattr %s\n", path);

  const ino_t inode = convertPathToInode(path);
  if (inode <= 0) {
    return -ENOENT;
  }
  return fgetattr_by_inode(path, statbuf, inode);
}

int GamecubeIsoFilesystem::opendir(const char *path,
                                   struct fuse_file_info *fi) {
  log("opendir %s\n", path);

  const ino_t inode = convertPathToInode(path);
  if (inode > 0) {
    fi->fh = inode;
    struct stat statbuf;
    if (const int i = fgetattr_by_inode(path, &statbuf, inode)) {
      return i;
    }
    return (statbuf.st_mode & S_IFDIR) ? 0 : -ENOTDIR;
  }
  return -EEXIST;
}

int GamecubeIsoFilesystem::releasedir(const char *path,
                                      struct fuse_file_info *fi) {
  log("releasedir %s\n", path);
  return 0;
}

struct readdir_callback_data {
  GamecubeIsoFilesystem *context;
  void *buf;
  fuse_fill_dir_t filler;
};

int GamecubeIsoFilesystem::readdir_callback(gc_dvdfs_file_entry *pfe,
                                            void *param) {
  struct stat statbuf;
  readdir_callback_data *const data =
      reinterpret_cast<readdir_callback_data *>(param);

  data->context->fgetattr_by_pfe(&statbuf, pfe);
  return data->filler(data->buf, data->context->mFst.getFileName(pfe), &statbuf,
                      0);
}

int GamecubeIsoFilesystem::readdir(const char *path, void *buf,
                                   fuse_fill_dir_t filler, off_t offset,
                                   struct fuse_file_info *fi) {
  const auto inode = fi->fh;

  log("readdir %s:%d\n", path, inode);
  if (inode == ROOT_INO) {
    for (unsigned int i = 0; i < num_root_dir_entries; ++i) {
      struct stat statbuf;
      if (fgetattr_by_inode(nullptr, &statbuf, root_dir_entries[i].inode)) {
        break;
      }

      if (filler(buf, root_dir_entries[i].name, &statbuf, 0)) {
        log("filler buffer full\n");
        break;
      }
    }
    return 0;
  } else {
    readdir_callback_data data;

    data.context = this;
    data.buf = buf;
    data.filler = filler;

    mFst.enumerate(inodeToFileEntry(inode), readdir_callback, &data);
    return 0;
  }
}

int GamecubeIsoFilesystem::open(const char *path, struct fuse_file_info *fi) {
  log("opendir %s\n", path);

  const ino_t inode = convertPathToInode(path);
  if (inode > 0) {
    fi->fh = inode;
    struct stat statbuf;
    if (const int i = fgetattr_by_inode(path, &statbuf, inode)) {
      return i;
    }
    return (statbuf.st_mode & S_IFDIR) ? -ENOENT : 0;
  }
  return -EEXIST;
}

int GamecubeIsoFilesystem::release(const char *path,
                                   struct fuse_file_info *fi) {
  log("release %s\n", path);
  return 0;
}

int GamecubeIsoFilesystem::read(const char *path, char *buf, size_t size,
                                off_t offset, struct fuse_file_info *fi) {
  const ino_t inode = fi->fh;
  const gc_dvdfs_file_entry *pfe = inodeToFileEntry(inode);
  off_t block_base;
  size_t file_length;
  off_t read;

  log("read %d:%d:%d\n", inode, size, offset);

  switch (inode) {
  case ROOT_INO:
    return 0;
  case APPLOADER_INO:
    file_length = mFst.getApploader().size;
    block_base = APPLOADER_OFFSET;
    break;
  case BOOTDOL_INO:
    file_length = mFst.getDolLength();
    block_base = mFst.getDolOffset();
    break;
  default:
    file_length = pfe->file.length;
    block_base = pfe->file.offset;
    break;
  }

  if (static_cast<size_t>(offset) >= file_length) {
    return 0;
  }

  read = std::min(size, static_cast<size_t>(file_length - offset));
  return mFile->read(buf, read, block_base + offset);
}
