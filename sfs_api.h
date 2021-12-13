#ifndef SFS_API_H
#define SFS_API_H

// You can add more into this file.

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "disk_emu.h"

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

#define BLOCK_SIZE 1024

#define NUM_INDIRECT_PTR_ENTRIES (BLOCK_SIZE / sizeof(unsigned int))

typedef struct {
  uint32_t magic;
  uint32_t block_size;
  uint32_t fs_size; // # blocks
  uint32_t inode_table_len;
  uint32_t root_dir_inode;
} superblock;

typedef struct {
  int inode; // nth inode, -1 if no i-node allocated
  uint32_t rwptr; // read/write pointer
} fd;

typedef struct {
  unsigned int mode; // does it exist? 1 or yes, 0 for no
  // unused even though handout has it
  // unsigned int link_cnt;
  // unsigned int uid;
  // unsigned int gid;
  //
  unsigned int size;
  unsigned int direct[12]; // set to 0 if unassigned
  // technically not an indirect pointer, but it won't take up too much memory
  // since we don't have double or triple indirect
  unsigned int indirect[NUM_INDIRECT_PTR_ENTRIES]; // set to 0 if unassigned
  //
} inode;

typedef struct {
  // idk why, but if I don't times 7 or more it segfaults. Otherwise works
  // perfectly. It results in 20 wasted blocks for 1024B blocks, so not a big
  // deal, but it's worth exploring in the future. The other option is turning
  // this into a char pointer, but that requires malloc, which makes the old
  // fuse wrapper fail.
  char name[MAXFILENAME * 7];
  //
  unsigned int mode; // does it exist? 1 or yes, 0 for no
} dir_entry;

#endif