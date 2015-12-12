/*
 * fs/gcdvdfs/fst.h
 *
 * Nintendo GameCube Filesystem driver
 * Copyright (C) 2006 The GameCube Linux Team
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 */
#ifndef __fst__h
#define __fst__h

#include <stdint.h>
#include "BinaryReader.h"

#define GC_DVD_SECTOR_SIZE 2048

#define FST_OFFSET 0x0424

#define FST_FILE 0
#define FST_DIRECTORY 1

#pragma pack(1)
struct gc_dvdfs_dol_header {
  uint32_t text_file_pos[7];
  uint32_t data_file_pos[11];
  uint32_t text_mem_pos[7];
  uint32_t data_mem_pos[11];
  uint32_t text_section_size[7];
  uint32_t data_section_size[11];
  uint32_t bss_mem_address;
  uint32_t bss_size;
  uint32_t entry_point;
};

struct gc_dvdfs_disc_header {
  uint32_t game_code;
  uint16_t maker_code;
  uint8_t disc_id;
  uint8_t version;
  uint8_t streaming;
  uint8_t streamBufSize;
  uint8_t padding1[22];
  uint8_t game_name[992];
  uint32_t offset_dh_bin;
  uint32_t addr_debug_monitor;
  uint8_t padding2[24];
  uint32_t offset_bootfile;
  uint32_t offset_fst;
  uint32_t fst_size;
  uint32_t max_fst_size;
  uint32_t user_position;
  uint32_t user_length;
  uint8_t padding[7];
};

struct gc_dvdfs_file_entry {
  uint8_t type;
  uint8_t offset_filename[3];
  union {
    struct {
      uint32_t offset;
      uint32_t length; /* if root, number of entries */
    } file;
    struct {
      uint32_t offset_parent;
      uint32_t offset_next;
    } dir;
  };
};

#define APPLOADER_OFFSET 0x2440
struct gc_dvdfs_apploader {
  uint8_t version[10];
  uint8_t padding[6];
  uint32_t entry_point;
  uint32_t size;
};

#pragma pack()

class GamecubeFilesystemTable;

struct gc_dvdfs_directory_info {
  uint32_t total_files;
  uint32_t total_directories;
  uint32_t total_file_size;
};

class GamecubeFilesystemTable {
private:
  struct gc_dvdfs_file_entry *root;
  const char *str_table;
  uint32_t size;
  uint32_t str_table_size;
  uint32_t dol_length;
  uint32_t dol_offset;
  uint32_t total_files;
  uint32_t total_directories;
  uint32_t total_file_size;
  struct gc_dvdfs_apploader apploader;
  struct gc_dvdfs_dol_header dol_header;

public:
  GamecubeFilesystemTable();
  ~GamecubeFilesystemTable();

  bool open(BinaryReader *in);

  int enumerate(struct gc_dvdfs_file_entry *root,
                int (*callback)(struct gc_dvdfs_file_entry *pfe, void *param),
                void *param) const;

  int getDirectoryInfo(struct gc_dvdfs_file_entry *root,
                       gc_dvdfs_directory_info *directoryInfo) const;

  uint32_t getTotalFileSize() const { return total_file_size; }
  uint32_t getTotalFiles() const { return total_files; }

  const struct gc_dvdfs_apploader &getApploader() const { return apploader; }
  const uint32_t getDolLength() const { return dol_length; }
  const uint32_t getDolOffset() const { return dol_offset; }

  struct gc_dvdfs_file_entry *getRoot() const {
    return root;
  }

  const char *getFileName(struct gc_dvdfs_file_entry *pfe) const {
    int fileNameOffset =
        ((pfe->offset_filename[0] << 16) | (pfe->offset_filename[1] << 8) |
         pfe->offset_filename[2]);

    return (str_table + fileNameOffset);
  }

private:
  bool isValidFileEntry(struct gc_dvdfs_file_entry *pfe) const;
  bool validate();

  inline static uint32_t filenameOffset(struct gc_dvdfs_file_entry *pfe) {
    return (((pfe)->offset_filename[0] << 16) |
            ((pfe)->offset_filename[1] << 8) | (pfe)->offset_filename[2]);
  }

  inline static uint32_t directoryEntries(struct gc_dvdfs_file_entry *root) {
    return root->dir.offset_next;
  }
};

#endif
