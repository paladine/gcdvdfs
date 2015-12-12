#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <endian.h>
#include "GamecubeFilesystemTable.h"

#define be32_to_cpu(x) be32toh(x)

static inline uint32_t directoryEntries(struct gc_dvdfs_file_entry *root) {
  return root->dir.offset_next;
}

static void gc_dvdfs_fix_raw_dol_header(struct gc_dvdfs_dol_header *pdh) {
#define SWAP(a) (a) = be32_to_cpu(a)
  unsigned int i;

  for (i = 0; i < 7; ++i) {
    SWAP(pdh->text_file_pos[i]);
    SWAP(pdh->text_mem_pos[i]);
    SWAP(pdh->text_section_size[i]);
  }

  for (i = 0; i < 11; ++i) {
    SWAP(pdh->data_file_pos[i]);
    SWAP(pdh->data_mem_pos[i]);
    SWAP(pdh->data_section_size[i]);
  }

  SWAP(pdh->bss_mem_address);
  SWAP(pdh->bss_size);
  SWAP(pdh->entry_point);
}

static uint32_t gc_dvdfs_get_dol_file_size(struct gc_dvdfs_dol_header *pdh) {
  uint32_t tmp;
  uint32_t i;
  uint32_t max = 0;

  for (i = 0; i < 7; ++i) {
    tmp = pdh->text_file_pos[i] + pdh->text_section_size[i];
    if (tmp > max)
      max = tmp;
  }
  for (i = 0; i < 11; ++i) {
    tmp = pdh->data_file_pos[i] + pdh->data_section_size[i];
    if (tmp > max)
      max = tmp;
  }
  return max;
}

GamecubeFilesystemTable::GamecubeFilesystemTable()
    : root(nullptr), str_table(nullptr), size(0), str_table_size(0),
      dol_length(0), dol_offset(0), total_files(0), total_directories(0),
      total_file_size(0)

{
  memset(&apploader, 0, sizeof(apploader));
  memset(&dol_header, 0, sizeof(dol_header));
}

GamecubeFilesystemTable::~GamecubeFilesystemTable() {
  if (root) {
    delete[] reinterpret_cast<unsigned char *>(root);
  }
}

bool GamecubeFilesystemTable::isValidFileEntry(
    struct gc_dvdfs_file_entry *pfe) const {
  const unsigned int foffset = filenameOffset(pfe);

  if (((pfe->type != FST_FILE) && (pfe->type != FST_DIRECTORY)) ||
      (foffset >= str_table_size)) {
    return false;
  }
  return true;
}

int GamecubeFilesystemTable::enumerate(
    struct gc_dvdfs_file_entry *root,
    int (*callback)(struct gc_dvdfs_file_entry *pfe, void *param),
    void *param) const {
  /* get the filename */
  uintptr_t i;
  int r;

  /* only enumerate directories */
  if (root->type != FST_DIRECTORY) {
    return -EINVAL;
  }

  const uintptr_t entries = directoryEntries(root);
  i = ((uintptr_t)root - (uintptr_t) this->root) /
          sizeof(struct gc_dvdfs_file_entry) +
      1;
  /* check if out of bounds */
  const uint32_t rootDirEntries = directoryEntries(this->root);
  if (i >= rootDirEntries || entries > rootDirEntries) {
    return -EINVAL;
  }

  /* loop through the files */
  while (i < entries) {
    struct gc_dvdfs_file_entry *pfe = this->root + i;
    /* verify the file is proper */
    if (!isValidFileEntry(pfe)) {
      return 0;
    }

    /* do the callback */
    if ((r = callback(pfe, param)) < 0) {
      return r;
    }

    if (pfe->type == FST_DIRECTORY) {
      if (pfe->dir.offset_next <= i) {
        /* we're going backwards or looping, abort */
        return -EINVAL;
      }
      i = pfe->dir.offset_next;
    } else {
      ++i;
    }
  }
  return 0;
}

static int gc_dvdfs_get_directory_info_callback(struct gc_dvdfs_file_entry *pfe,
                                                void *param) {
  struct gc_dvdfs_directory_info *const di =
      (struct gc_dvdfs_directory_info *)param;

  if (pfe->type == FST_FILE) {
    di->total_files++;
    di->total_file_size += pfe->file.length;
  } else if (pfe->type == FST_DIRECTORY) {
    di->total_directories++;
  }
  return 0;
}

int GamecubeFilesystemTable::getDirectoryInfo(
    struct gc_dvdfs_file_entry *root,
    struct gc_dvdfs_directory_info *di) const {
  memset(di, 0, sizeof(*di));

  return enumerate(root, gc_dvdfs_get_directory_info_callback, di);
}

bool GamecubeFilesystemTable::validate() {
  unsigned int i;

  /* make sure the FST is completely valid */
  if (root->type != FST_DIRECTORY) {
    fprintf(stderr, "gcdvdfs: Root entry is not a directory!\n");
    return false;
  }

  const uint32_t entries = be32_to_cpu(root->dir.offset_next);
  if (entries >= (size / sizeof(struct gc_dvdfs_file_entry))) {
    fprintf(stderr, "gcdvdfs: Too many entries, will overflow the FST!\n");
    return false;
  }
  /* ok let's convert all the data to native format and compute total size*/
  for (i = 0; i < entries; ++i) {
    root[i].file.length = be32_to_cpu(root[i].file.length);
    root[i].file.offset = be32_to_cpu(root[i].file.offset);
    if (root[i].type == FST_FILE) {
      total_files++;
      total_file_size += root[i].file.length;
    } else if (root[i].type == FST_DIRECTORY) {
      total_directories++;
    }
  }

  return true;
}

bool GamecubeFilesystemTable::open(BinaryReader *in) {
  char buffer[GC_DVD_SECTOR_SIZE];

  // read the header
  if (in->read(buffer, GC_DVD_SECTOR_SIZE, 0) <
      static_cast<int>(sizeof(struct gc_dvdfs_disc_header))) {
    return false;
  }

  /* read the FST into memory */
  struct gc_dvdfs_disc_header *const dh = (struct gc_dvdfs_disc_header *)buffer;
  const uint32_t fst_offset = be32_to_cpu(dh->offset_fst);
  const uint32_t fst_size = be32_to_cpu(dh->fst_size);
  const uint32_t dol_offset = be32_to_cpu(dh->offset_bootfile);

  /* now allocate the fst */
  if (!(root = reinterpret_cast<struct gc_dvdfs_file_entry *>(
            new unsigned char[fst_size]))) {
    return false;
  }

  size = fst_size;
  /* now try to read the fst */
  if (in->read(root, fst_size, fst_offset) != static_cast<int>(fst_size)) {
    fprintf(stderr, "gcdvdfs: Unable to read FST into memory\n");
    goto fst_error;
  }
  /* now try to read the apploader */
  if (in->read(&apploader, sizeof(struct gc_dvdfs_apploader),
               APPLOADER_OFFSET) != sizeof(struct gc_dvdfs_apploader)) {
    fprintf(stderr, "gcdvdfs: Unable to read apploader into memory\n");
    goto fst_error;
  }
  /* fix up the apploader */
  apploader.entry_point = be32_to_cpu(apploader.entry_point);
  apploader.size = be32_to_cpu(apploader.size);
  /* now try to read the dol header */
  if (in->read(&dol_header, sizeof(struct gc_dvdfs_dol_header), dol_offset) !=
      sizeof(struct gc_dvdfs_dol_header)) {
    fprintf(stderr, "gcdvdfs: Unable to read DOL Header\n");
    goto fst_error;
  }
  gc_dvdfs_fix_raw_dol_header(&dol_header);

  this->dol_offset = dol_offset;
  dol_length = gc_dvdfs_get_dol_file_size(&dol_header);
  /* compute the location of the string table */
  {
    const uint32_t str_table_offset =
        be32_to_cpu(root->dir.offset_next) * sizeof(struct gc_dvdfs_file_entry);
    str_table = (char *)((uintptr_t)root + str_table_offset);
    str_table_size = size - str_table_offset;
  }
  total_files = 0;
  total_directories = 0;
  total_file_size = 0;
  /* now validate the fst */
  if (!validate()) {
    goto fst_error;
  }

  return true;
fst_error:
  if (root) {
    delete[] reinterpret_cast<unsigned char *>(root);
    root = nullptr;
  }
  return false;
}
