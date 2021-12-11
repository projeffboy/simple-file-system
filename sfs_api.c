#include "sfs_api.h"
#include <stdbool.h>

#define DISK "fs.sfs"
#define NUM_BLOCKS 120000
#define NUM_INODES 193  // also max number of files (including the directory)

int divide_round_up(int p, int q) {  // p and q have to be positive
  return (p + q - 1) / p;
}

// NOTE: If you see +1 after an integer division, it's likely there for rounding
// up

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
inode inode_table[NUM_INODES];
unsigned int num_files = 0;
unsigned int current_file = 0;  // among the existing files
int num_root_blocks = 0;
dir_entry dir_table[NUM_INODES - 1];  // root directory (cache)
fd fdt[NUM_INODES];                   // stores open files including root
uint64_t free_block_list[NUM_FREE_BITMAP_ROWS];

void write_inode_table() {
  write_blocks(1, NUM_INODE_BLOCKS, inode_table);
}
void write_dir_table() {
  write_blocks(1 + NUM_INODE_BLOCKS, NUM_ROOT_BLOCKS, dir_table);
}
void write_free_block_list() {
  write_blocks(FREE_BLOCK_LIST_ADDR, NUM_FREE_BITMAP_BLOCKS, free_block_list);
}

int get_num_files() {
  int num_files = 0;

  for (int i = 1; i < NUM_INODES; i++) {
    if (inode_table[i].mode) {
      num_files++;
    }
  }

  return num_files++;
}

void init_superblock() {
  supblock.magic = 0xACBD0005;
  supblock.block_size = BLOCK_SIZE;
  supblock.inode_table_len = NUM_INODE_BLOCKS;
  supblock.root_dir_inode = 0;  // 0th i-node -> root dir
  supblock.fs_size = (1 /* superblock */ + num_root_blocks + NUM_INODE_BLOCKS
    + MAX_BLOCKS_ALL_FILES + NUM_FREE_BITMAP_BLOCKS);

  // use uint64 which maps 64 blocks with 8 bytes (should be 4
  // bytes long)
  //?
  supblock.length_free_block_list = MAX_BLOCKS_ALL_FILES;
  supblock.number_free_blocks
    = (sizeof(uint64_t) * NUM_FREE_BITMAP_ROWS) / BLOCK_SIZE + 1;  // round up
}

void mksfs(int fresh) {
  // Reset global variables
  current_file = 0;
  num_files = 0;
  num_root_blocks = NUM_ROOT_BLOCKS;

  if (fresh) {
    // Init superblock and disk
    init_superblock();
    init_fresh_disk(DISK, BLOCK_SIZE, supblock.fs_size + 1);
    for (int i = 0; i < NUM_INODES; i++) {
      inode_table[i].mode = 0;
      fdt[i].inode = -1;
      fdt[i].rwptr = 0;
    }
    for (int i = 0; i < NUM_INODES - 1; i++) {
      dir_table[i].name = "";
      dir_table[i].mode = 0;
    }
    // Write superblock onto disk
    write_blocks(0, 1, &supblock);

    // Init root
    fdt[0].inode = 0;  // root takes 0th i-node
    fdt[0].rwptr = 0;
    inode_table[0].mode = 1;  // 0th i-node (for root) is taken
    free_block_list[0] = (uint64_t)1 << 63;

    // Write i-node table, directory table, and free block list onto disk
    write_dir_table();
    write_inode_table();
    // leave space for the data blocks
    write_free_block_list();

    fflush(stdout);
  } else {
    // all files are unopened
    for (int i = 0; i < NUM_INODES; i++) {
      fdt[i].inode = -1;
      fdt[i].rwptr = 0;
    }

    // open superblock, i-node table, directory table, free block list
    read_blocks(0, 1, &supblock);
    read_blocks(1, NUM_INODE_BLOCKS, inode_table);
    read_blocks(1 + NUM_INODE_BLOCKS, NUM_ROOT_BLOCKS, dir_table);
    read_blocks(FREE_BLOCK_LIST_ADDR, NUM_FREE_BITMAP_BLOCKS, free_block_list);

    num_files = get_num_files();
  }
}

int sfs_getnextfilename(char* fname) {
  num_files = get_num_files();

  if (num_files > 0) {
    int visited = 0;
    for (int i = 0; i < NUM_INODES - 1; i++) {
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
  }

  current_file = 0;

  return 0;
}

void sfs_getfilesize_helper(int* index_data_block, int* size) {
  if (*index_data_block > 0) {
    char buff_data[BLOCK_SIZE];
    read_blocks(*index_data_block, 1, (void*)buff_data);
    for (int i = 0; i < BLOCK_SIZE; i++) {
      if (buff_data[i] != '\0') {
        (*size)++;
      }
    }
  }
}
int sfs_getfilesize(const char* path) {
  int size = 0;

  for (int i = 0; i < NUM_INODES - 1; i++) {
    if (strcmp(path, dir_table[i].name) == 0) {
      size += inode_table[i + 1].size;

      // size of direct blocks
      for (int j = 0; j < 12; j++) {
        int index_data_block = inode_table[i + 1].direct[j];
        sfs_getfilesize_helper(&index_data_block, &size);
      }
      // size from indirect blocks
      for (int j = 0; j < NUM_INDIRECT_PTR_ENTRIES; j++) {
        int index_data_block = inode_table[i + 1].indirect[j];
        sfs_getfilesize_helper(&index_data_block, &size);
      }

      break;
    }
  }

  return size;
}

int sfs_fopen(char* name) {
  num_files = get_num_files();

  if (!(0 <= strlen(name) && strlen(name) <= MAXFILENAME)) {
    return -1;
  }
  // filename length is valid

  // must check for three cases: file and descriptor exists, only file exists,
  // both don't exist

  for (int i = 0; i < NUM_INODES - 1; i++) {
    if (strcmp(name, dir_table[i].name) == 0) {
      // file exists
      int nth_inode = i + 1;
      for (int j = 0; j < NUM_INODES; j++) {
        if (fdt[j].inode == nth_inode) {
          // descriptor exists
          return j;
        }
      }

      // descriptor doesn't exist
      for (int j = 0; i < NUM_INODES; j++) {
        if (fdt[j].inode == -1) {
          // FDT slot available
          fdt[j].inode = nth_inode;
          fdt[j].rwptr = sfs_getfilesize(name);
          dir_table[i].mode = 1;
          inode_table[nth_inode].mode = 1;  //?

          return j;
        }
      }

      // FDT is full
      return -1;
    }
  }

  // file doesn't exist
  // file (and descriptor) don't exist
  for (int i = 1; i < NUM_INODES; i++) {
    if (inode_table[i].mode == 0) {
      inode_table[i].mode = 1;
      dir_table[i - 1].name = (char*)malloc(strlen(name));
      strcpy(dir_table[i - 1].name, name);
      dir_table[i - 1].mode = 1;
      num_files++;  // new file created.
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
  if (1 <= fileID && fileID < NUM_INODES) {
    fd* f = &fdt[fileID];
    if (f->inode == -1) {
      return -1;  // already closed file
    }
    f->inode = -1;
    f->rwptr = 0;

    return 0;
  }

  return -1;
}

int sfs_fwrite(int fileID, const char* buf, int length) {
  // ARGUMENT CHECKING
  if (fileID < 0 || length == 0) {
    return 0;
  }

  // INITIALIZE VARIABLES
  num_files = get_num_files(); //?
  bool wrote_to_disk = false;
  int buf_len = length;
  int bytes_written = 0;
  fd* f = &fdt[fileID];
  inode* file_inode = &inode_table[f->inode];
  int nth_inode_block = (f->rwptr) / BLOCK_SIZE; // starts from 0
  unsigned int* nth_data_block;

  // SANITY CHECK
  if (
    !(0 <= f->rwptr && f->rwptr < FILE_CAPACITY)
    || f->inode <= 0 // can't write to root
    || nth_inode_block < 0
  ) {
    return 0;
  }

  while (bytes_written < buf_len && nth_inode_block < MAX_BLOCKS_PER_FILE) {
    // GET N-TH I-NODE BLOCK
    int block_offset = (f->rwptr) % BLOCK_SIZE;
    char block_buf[BLOCK_SIZE];

    if (nth_inode_block < 12) {
      // direct pointer

      nth_data_block = &(file_inode->direct[nth_inode_block]);
    } else if (nth_inode_block >= 12 && nth_inode_block < MAX_BLOCKS_PER_FILE) {
      // indirect pointer

      nth_data_block = &(file_inode->indirect[nth_inode_block - 12]);
    } else {
      // shouldn't get here

      return bytes_written;
    }

    // PREPARE BLOCK BUFFER
    if (*nth_data_block > 0) {
      // data block is allocated

      read_blocks(*nth_data_block, 1, (void*)block_buf);
    } else if (*nth_data_block == 0) {
      for (int i = 0; i < BLOCK_SIZE; i++) {
        block_buf[i] = '\0'; // bcuz initializing doesn't set the values to 0
      }
    }

    // EDIT BLOCK BUFFER
    while (block_offset < BLOCK_SIZE && bytes_written < buf_len) {
      if (buf[bytes_written] == '\0') {
        (file_inode->size)++;
      }
      block_buf[block_offset] = buf[bytes_written];
      block_offset++;
      bytes_written++;
      (f->rwptr)++;
    }

    // WRITE BLOCK BUFFER INTO DISK
    if (*nth_data_block > 0) {
      write_blocks(*nth_data_block, 1, (void*)block_buf);
    } else if (*nth_data_block == 0) {
      // need to find a free data block
      int tmp = -1;
      for (int row_num = 0; row_num < NUM_FREE_BITMAP_ROWS; row_num++) {
        uint64_t row = free_block_list[row_num];

        for (int col_num = 63; col_num >= 0; col_num--) {
          uint64_t bit = 1;
          uint64_t bit_mask = bit << col_num;

          if ((bit_mask & row) >> col_num == 0) { // found a 0 in free_block_list
            tmp = 1 + NUM_INODE_BLOCKS + NUM_ROOT_BLOCKS + (63 - col_num); //?
            free_block_list[row_num] |= bit << col_num;
            break;
          }
        }

        if (tmp >= 0) {
          *nth_data_block = tmp + (row_num * 64);
          write_blocks(*nth_data_block, 1, (void*)block_buf);
          write_inode_table();
          write_free_block_list();
          break; // since a data block was found
        }
      }
      if (tmp == -1) {
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
  num_files = get_num_files(); //?
  int buf_len = length;
  int bytes_read = 0;
  fd* f = &fdt[fileID];
  inode* file_inode = &inode_table[f->inode];
  int nth_inode_block = (f->rwptr) / BLOCK_SIZE; // starts from 0
  unsigned int* nth_data_block;

  // SANITY CHECK
  if (
    !(0 <= f->rwptr && f->rwptr < FILE_CAPACITY)
    || f->inode <= 0 // 0 is root, -1 means not set
    || nth_inode_block < 0
  ) {
    return 0;
  }

  while (bytes_read < buf_len && nth_inode_block < (MAX_BLOCKS_PER_FILE)) {
    // GET N-TH I-NODE BLOCK
    int block_offset = (f->rwptr) % BLOCK_SIZE;
    char block_buf[BLOCK_SIZE];

    if (nth_inode_block < 12) {
      // direct pointer

      nth_data_block = &(file_inode->direct[nth_inode_block]);
    } else if (12 <= nth_inode_block && nth_inode_block < MAX_BLOCKS_PER_FILE) {
      // indirect pointer

      nth_data_block = &(file_inode->direct[nth_inode_block]);
    } else {
      // shouldn't get here

      return bytes_read;
    }

    // PREPARE BLOCK BUFFER
    read_blocks(*nth_data_block, 1, (void*)block_buf);

    // READ BLOCK BUFFER
    while (block_offset < BLOCK_SIZE && bytes_read < buf_len) {
      buf[bytes_read] = block_buf[block_offset];
      bytes_read++;
      block_offset++;
      (f->rwptr)++;
    }

    nth_inode_block++;
  }

  if (inode_table[f->inode].size == 0) {
    int chars = 0;
    for (int i = 0; i < length; i++) {
      if (buf[i] != '\0') {
        chars++;
      }
    }
    return chars;
  } //?

  return bytes_read;
}

int sfs_fseek(int fileID, int loc) {
  // argument checking
  if (fileID < 0 || !(0 <= loc && loc < FILE_CAPACITY)) {
    return -1;
  }

  fd* f = &fdt[fileID];
  if (f->inode <= 0) { // don't allow opening root
    return -1;
  }
  f->rwptr = loc;

  return 0;
}

void free_from_block_list(int nth_data_block) {
  int a = nth_data_block - (1 + NUM_INODE_BLOCKS + NUM_ROOT_BLOCKS);
  int b = a / 64;
  int c = a % 64;
  c = 63 - c;

  free_block_list[b] &= ~((uint64_t)1 << c);
}

int sfs_remove(char* file) {
  for (int i = 0; i < NUM_INODES - 1; i++) {
    if (strcmp(file, dir_table[i].name) == 0) {
      inode_table[i + 1].mode = 0;
      inode_table[i + 1].size = 0;

      // DELETE I-NODE
      // UPDATE FREE BLOCK LIST
      for (int j = 0; j < 12; j++) { // direct blocks
        free_from_block_list(inode_table[i + 1].direct[j]);

        inode_table[i + 1].direct[j] = 0;
      }
      for (int j = 0; j < NUM_INDIRECT_PTR_ENTRIES; j++) { // indirect blocks
        free_from_block_list(inode_table[i + 1].indirect[j]);

        inode_table[i + 1].indirect[j] = 0;
      }

      // DELETE FD
      fdt[i + 1].inode = -1;

      // DELETE DIR ENTRY
      dir_table[i].mode = 0;

      // UPDATE DISK
      write_inode_table();
      write_dir_table();
      write_free_block_list();

      return 1;
    }
  }

  return -1;
}
