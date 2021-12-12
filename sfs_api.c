#include "sfs_api.h"
#include <stdbool.h>

#define DISK "fs.sfs"
#define NUM_INODES 200  // also max number of files (including the directory)

// NOTE:
// If you see +1 after an integer division, it's likely there for rounding up.
// I am assuming that global arrays are initialized to 0 and so far it seems to
// be true.

#define NUM_INODE_BLOCKS (sizeof(inode) * NUM_INODES / BLOCK_SIZE + 1)
#define NUM_ROOT_BLOCKS (sizeof(dir_entry) * (NUM_INODES - 1) / BLOCK_SIZE + 1)
#define MAX_BLOCKS_PER_FILE (12 + NUM_INDIRECT_PTR_ENTRIES)
#define FILE_CAPACITY (BLOCK_SIZE * MAX_BLOCKS_PER_FILE)
//
#define MAX_BLOCKS_ALL_FILES (NUM_INODES - 1) * MAX_BLOCKS_PER_FILE
// -1 is for the root aka directory
// block size / int size is the single pointer
//
#define NUM_FREE_BITMAP_ROWS (MAX_BLOCKS_ALL_FILES / 64)
#define NUM_FREE_BITMAP_BLOCKS \
sizeof(uint64_t) * NUM_FREE_BITMAP_ROWS / BLOCK_SIZE + 1
#define FREE_BLOCK_LIST_ADDR \
  1 + NUM_INODE_BLOCKS + NUM_ROOT_BLOCKS + MAX_BLOCKS_ALL_FILES

superblock supblock;
inode inode_table[NUM_INODES]; // cannot operate on root i-node
unsigned int current_file = 0; // among the existing files
// 0th element is unused for consistency (keep it that way!)
dir_entry dir_table[NUM_INODES];
//
fd fdt[NUM_INODES]; // stores root which cannot be closed
uint64_t free_block_list[NUM_FREE_BITMAP_ROWS];

void write_inode_table() {
  write_blocks(1, NUM_INODE_BLOCKS, inode_table);
}
void write_dir_table() {
  write_blocks(1 + NUM_INODE_BLOCKS, NUM_ROOT_BLOCKS, dir_table);
}
void write_free_block_list() {
  // left space for the data blocks
  write_blocks(FREE_BLOCK_LIST_ADDR, NUM_FREE_BITMAP_BLOCKS, free_block_list);
}

void init_superblock() {
  supblock.magic = 0xACBD0005;
  supblock.block_size = BLOCK_SIZE;
  supblock.inode_table_len = NUM_INODE_BLOCKS;
  supblock.root_dir_inode = 0;  // 0th i-node -> root dir
  supblock.fs_size = 1 // superblock 
    + NUM_ROOT_BLOCKS
    + NUM_INODE_BLOCKS
    + MAX_BLOCKS_ALL_FILES
    + NUM_FREE_BITMAP_BLOCKS;
}

void mksfs(int fresh) {
  // Reset global variables
  current_file = 0;

  if (fresh) {
    // Init superblock and disk
    init_superblock();
    init_fresh_disk(DISK, BLOCK_SIZE, supblock.fs_size);
    for (int i = 0; i < NUM_INODES; i++) {
      inode_table[i].mode = 0;
      inode_table[i].size = 0;

      fdt[i].inode = -1;
      fdt[i].rwptr = 0;

      dir_table[i].name = "";
      dir_table[i].mode = 0;
    }
    // Write superblock onto disk
    write_blocks(0, 1, &supblock);

    // Init root
    fdt[0].inode = 0; // 0th i-node is for the root
    inode_table[0].mode = 1;
    for (int i = 0; i < NUM_ROOT_BLOCKS; i++) {
      free_block_list[0] |= (uint64_t)1 << (63 - i);
    }

    // Write these onto disk
    write_dir_table();
    write_inode_table();
    write_free_block_list();

    fflush(stdout);
  } else {
    // all files are unopened except root
    for (int i = 0; i < NUM_INODES; i++) {
      fdt[i].inode = -1;
      fdt[i].rwptr = 0;
    }
    fdt[0].inode = 0;

    // open superblock, i-node table, directory table, free block list
    read_blocks(0, 1, &supblock);
    read_blocks(1, NUM_INODE_BLOCKS, inode_table);
    read_blocks(1 + NUM_INODE_BLOCKS, NUM_ROOT_BLOCKS, dir_table);
    read_blocks(FREE_BLOCK_LIST_ADDR, NUM_FREE_BITMAP_BLOCKS, free_block_list);
  }
}

int sfs_getnextfilename(char* fname) {
  if (!(0 <= strlen(fname) && strlen(fname) <= MAXFILENAME)) { // check arg
    return 0;
  }

  int visited = 0;
  for (int i = 1; i < NUM_INODES; i++) {
    if (dir_table[i].mode == 1) {
      if (visited == current_file) {
        strcpy(fname, dir_table[i].name);
        current_file++;

        return 1;
      } else {
        visited++;
      }
    }
  }
  current_file = 0;

  return 0;

}

int sfs_getfilesize(const char* path) {
  if (!(0 <= strlen(path) && strlen(path) <= MAXFILENAME)) { // check arg
    return 0;
  }

  for (int i = 1; i < NUM_INODES; i++) {
    if (dir_table[i].mode == 1 && strcmp(path, dir_table[i].name) == 0) {
      return inode_table[i].size;
    }
  }

  return 0;
}

int sfs_fopen(char* name) {
  if (!(0 <= strlen(name) && strlen(name) <= MAXFILENAME)) {
    return -1;
  }

  // filename length is valid

  // must check for three cases: file and descriptor exists, only file exists,
  // both don't exist

  for (int i = 1; i < NUM_INODES; i++) {
    if (dir_table[i].mode == 1 && strcmp(name, dir_table[i].name) == 0) {
      // file exists

      for (int j = 1; j < NUM_INODES; j++) {
        if (fdt[j].inode == i) {
          // descriptor exists

          return j;
        }
      }

      // descriptor doesn't exist
      for (int j = 1; j < NUM_INODES; j++) {
        if (fdt[j].inode == -1) {
          // FDT slot available

          fdt[j].inode = i;
          fdt[j].rwptr = sfs_getfilesize(name);

          return j;
        }
      }

      // FDT is full
      return -1;
    }
  }

  // file (and descriptor) doesn't exist
  for (int i = 1; i < NUM_INODES; i++) {
    if (inode_table[i].mode == 0) {
      inode_table[i].mode = 1;
      dir_table[i].name = (char*)malloc(strlen(name));
      strcpy(dir_table[i].name, name);
      dir_table[i].mode = 1;
      write_inode_table();
      write_dir_table();

      for (int j = 1; j < NUM_INODES; j++) {
        if (fdt[j].inode == -1) {
          fdt[j].inode = i;
          fdt[j].rwptr = 0;

          return j;
        }
      }
    }
  }

  // Idk something went wrong
  return -1;
}

int sfs_fclose(int fileID) {
  if (!(1 <= fileID && fileID < NUM_INODES)) { // check arg
    return -1;
  }

  fd* f = &fdt[fileID];
  if (f->inode == -1) {
    return -1;  // already closed file
  }
  f->inode = -1;
  f->rwptr = 0;

  return 0;
}

int sfs_fwrite(int fileID, const char* buf, int length) {
  // ARGUMENT CHECKING
  if (fileID < 0 || length == 0) {
    return 0;
  }

  // INITIALIZE VARIABLES
  bool wrote_to_disk = false;
  int buf_len = length;
  int bytes_written = 0;
  fd* f = &fdt[fileID];
  inode* file_inode;
  int nth_inode_block = (f->rwptr) / BLOCK_SIZE; // starts from 0
  unsigned int* data_block_addr;

  // SANITY CHECK
  if (
    !(0 <= f->rwptr && f->rwptr < FILE_CAPACITY)
    || f->inode <= 0 // can't write to root
    || nth_inode_block < 0
  ) {
    return 0;
  }
  file_inode = &inode_table[f->inode];

  while (bytes_written < buf_len && nth_inode_block < MAX_BLOCKS_PER_FILE) {
    // GET N-TH I-NODE BLOCK
    int block_offset = (f->rwptr) % BLOCK_SIZE;
    char block_buf[BLOCK_SIZE];

    if (nth_inode_block < 12) {
      // direct pointer

      data_block_addr = &(file_inode->direct[nth_inode_block]);
    } else if (nth_inode_block >= 12 && nth_inode_block < MAX_BLOCKS_PER_FILE) {
      // indirect pointer

      data_block_addr = &(file_inode->indirect[nth_inode_block - 12]);
    } else {
      // shouldn't get here

      return bytes_written;
    }

    // PREPARE BLOCK BUFFER
    if (*data_block_addr > 0) {
      // data block is allocated

      read_blocks(*data_block_addr, 1, (void*)block_buf);
    } else if (*data_block_addr == 0) {
      for (int i = 0; i < BLOCK_SIZE; i++) {
        block_buf[i] = '\0'; // bcuz initializing doesn't set the values to 0
      }
    }

    // EDIT BLOCK BUFFER
    while (block_offset < BLOCK_SIZE && bytes_written < buf_len) {
      (f->rwptr)++;
      if (file_inode->size < f->rwptr) {
        (file_inode->size)++;
      }
      block_buf[block_offset] = buf[bytes_written];
      block_offset++;
      bytes_written++;
    }

    // WRITE BLOCK BUFFER INTO DISK
    if (*data_block_addr > 0) {
      write_blocks(*data_block_addr, 1, (void*)block_buf);
    } else if (*data_block_addr == 0) {
      // need to find a free data block
      int new_data_block_addr = -1;
      for (int row_num = 0; row_num < NUM_FREE_BITMAP_ROWS; row_num++) {
        uint64_t row = free_block_list[row_num];

        for (int col_num = 0; col_num < 64; col_num++) {
          uint64_t bit = 1;
          uint64_t bit_mask = bit << (63 - col_num);

          if ((bit_mask & row) >> (63 - col_num) == 0) { // found a 0 in free_block_list
            new_data_block_addr = 1 // superblock
              + NUM_INODE_BLOCKS
              + NUM_ROOT_BLOCKS
              + col_num
              + row_num * 64;
            free_block_list[row_num] |= bit_mask;
            break;
          }
        }

        if (new_data_block_addr >= 0) {
          *data_block_addr = new_data_block_addr;
          write_blocks(*data_block_addr, 1, (void*)block_buf);
          write_inode_table();
          write_free_block_list();
          break; // since a data block was found
        }
      }
      if (new_data_block_addr == -1) {
        // no free blocks

        if (wrote_to_disk) {
          return bytes_written;
        } else {
          return 0;
        }
      }
    }

    wrote_to_disk = true;
    nth_inode_block++;
  }

  return bytes_written;
}

int sfs_fread(int fileID, char* buf, int length) {
  // ARGUMENT CHECKING
  if (fileID < 0 || length == 0) {
    return 0;
  }

  // INITIALIZE VARIABLES
  int buf_len = length;
  int bytes_read = 0;
  fd* f = &fdt[fileID];
  inode* file_inode;
  int nth_inode_block = (f->rwptr) / BLOCK_SIZE; // starts from 0
  unsigned int* data_block_addr;

  // SANITY CHECK
  if (
    !(0 <= f->rwptr && f->rwptr < FILE_CAPACITY)
    || f->inode <= 0 // 0 is root, -1 means not set
    || nth_inode_block < 0
  ) {
    return 0;
  }
  file_inode = &inode_table[f->inode];

  while (bytes_read < buf_len && nth_inode_block < (MAX_BLOCKS_PER_FILE)) {
    // GET N-TH I-NODE BLOCK
    int block_offset = (f->rwptr) % BLOCK_SIZE;
    char block_buf[BLOCK_SIZE];

    if (nth_inode_block < 12) {
      // direct pointer

      data_block_addr = &(file_inode->direct[nth_inode_block]);
    } else if (12 <= nth_inode_block && nth_inode_block < MAX_BLOCKS_PER_FILE) {
      // indirect pointer

      data_block_addr = &(file_inode->direct[nth_inode_block]);
    } else {
      // shouldn't get here

      return bytes_read;
    }

    // PREPARE BLOCK BUFFER
    read_blocks(*data_block_addr, 1, (void*)block_buf);

    // READ BLOCK BUFFER
    while (block_offset < BLOCK_SIZE && bytes_read < buf_len) {
      buf[bytes_read] = block_buf[block_offset];
      bytes_read++;
      block_offset++;
      (f->rwptr)++;
    }

    nth_inode_block++;
  }

  // can't just return bytes_read, because some of buf could just be empty space
  int valid_bytes_read = 0;
  for (int i = 0; i < length; i++) {
    if (buf[i] != '\0') {
      valid_bytes_read = i;
    }
  }
  return valid_bytes_read + 1;
}

int sfs_fseek(int fileID, int loc) {
  if (fileID < 0 || !(0 <= loc && loc < FILE_CAPACITY)) { // check args
    return -1;
  }

  fd* f = &fdt[fileID];
  if (f->inode <= 0) { // don't allow seeking root
    return -1;
  }
  f->rwptr = loc;

  return 0;
}

void free_from_block_list(int data_block_addr) {
  int nth_data_block = data_block_addr \
    - (1 /* superblock */ + NUM_INODE_BLOCKS + NUM_ROOT_BLOCKS);
  int row_num = nth_data_block / 64;
  int col_num = nth_data_block % 64;
  uint64_t bit_mask = ~((uint64_t)1 << (63 - col_num));

  free_block_list[row_num] &= bit_mask;
}
int sfs_remove(char* file) {
  // ARGUMENT CHECKING
  if (!(0 <= strlen(file) && strlen(file) <= MAXFILENAME)) {
    return -1;
  }

  for (int i = 1; i < NUM_INODES - 1; i++) {
    if (dir_table[i].mode == 1 && strcmp(file, dir_table[i].name) == 0) {
      inode_table[i].mode = 0;
      inode_table[i].size = 0;

      // DELETE I-NODE
      // UPDATE FREE BLOCK LIST
      for (int j = 0; j < 12; j++) { // direct blocks
        free_from_block_list(inode_table[i].direct[j]);

        inode_table[i].direct[j] = 0;
      }
      for (int j = 0; j < NUM_INDIRECT_PTR_ENTRIES; j++) { // indirect blocks
        free_from_block_list(inode_table[i].indirect[j]);

        inode_table[i].indirect[j] = 0;
      }

      // DELETE FD
      fdt[i].inode = -1;

      // DELETE DIR ENTRY
      dir_table[i].mode = 0;

      // UPDATE DISK
      write_inode_table();
      write_dir_table();
      write_free_block_list();

      return 0;
    }
  }

  return -1;
}
