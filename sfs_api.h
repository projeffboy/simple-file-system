#ifndef SFS_API_H
#define SFS_API_H

// You can add more into this file.

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "disk_emu.h"
#include "sfs_dir.h"

void mksfs(int);

int sfs_getnextfilename(char*);

int sfs_getfilesize(const char*);

int sfs_fopen(char*);

int sfs_fclose(int);

int sfs_fwrite(int, const char*, int);

int sfs_fread(int, char*, int);

// don't seek to a location beyond what is written
// i-node size depends on this assumption
int sfs_fseek(int, int);

int sfs_remove(char*);

#define MAXFILENAME 20

#define BLOCK_SIZE 2048

#define NUM_INDIRECT_PTR_ENTRIES (BLOCK_SIZE / sizeof(unsigned int))

typedef struct {
  uint64_t magic;
  uint64_t block_size;
  uint64_t fs_size; // # blocks
  uint64_t inode_table_len;
  uint64_t root_dir_inode;
  uint64_t length_free_block_list;
  uint64_t number_free_blocks;
} superblock;

typedef struct {
  uint64_t inode; // nth inode, -1 if no i-node allocated
  uint64_t rwptr; // read/write pointer
} fd;

typedef struct {
  unsigned int mode; // does it exist? 1 or yes, 0 for no
  // unused even though handout has it
  unsigned int link_cnt;
  unsigned int uid;
  unsigned int gid;
  //
  unsigned int size;
  unsigned int direct[12];
  unsigned int indirect[NUM_INDIRECT_PTR_ENTRIES];
} inode;

#endif

/*
make the arrays consistent
remove the file and descriptor exists case
can we close the directory?
i-node size
proper indirect pointer
have the root i-node take in the dir table
2048 -> 1024
can further simplify getnextfilename
are the uint64_t necessary?
deal with the question marks
isn't i-node block default -1
tmp variable
argument checking
*/