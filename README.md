# Project: A Defragmenter

## 1. Introduction

The old Unix file system can get decreasing performance because of fragmentation. File system defragmenters improve the performance by laying out all blocks of a file in a sequential order on disk, and we are going to implement one in this assignment.

### Disk structure ###

Given a disk image containing a file system with no corruption. Two major data structures that are used to manage this disk are related to the defragmenter: the *superblock* and the *inode*. The superblock structure is given as follows:
```c
  struct superblock {
    int blocksize; /* size of blocks in bytes */
    int inode_offset; /* offset of inode region in blocks */
    int data_offset; /* data region offset in blocks */
    int swap_offset; /* swap region offset in blocks */
    int free_inode; /* head of free inode list */
    int free_block; /* head of free block list */
  }
```
On disk, the first 512 bytes contains the boot block, and its actual format is not relevant to us. The second 512 bytes contains the superblock, the structure of which is defined above. All offsets in the superblock are given as blocks. Thus, if `inode_offset` is 1 and `blocksize` is 1 KB, the inode region starts at address `1024 B + 1*1 KB = 2 KB` into the disk. Each region fills up the disk up to the next region, and the swap region fills the disk to the end.

The inode region is effectively a large array of inodes, and the inode structure is given as follows:
```c
  #define N_DBLOCKS 10 
  #define N_IBLOCKS 4  
  
  struct inode {  
      int next_inode; /* list for free inodes */  
      int protect;        /*  protection field */ 
      int nlink;  /* Number of links to this file */ 
      int size;  /* Number of bytes in file */   
      int uid;   /* Owner's user ID */  
      int gid;   /* Owner's group ID */  
      int ctime;  /* Time field */  
      int mtime;  /* Time field */  
      int atime;  /* Time field */  
      int dblocks[N_DBLOCKS];   /* Pointers to data blocks */  
      int iblocks[N_IBLOCKS];   /* Pointers to indirect blocks */  
      int i2block;     /* Pointer to doubly indirect block */  
      int i3block;     /* Pointer to triply indirect block */  
   };
```
An unused inode has zero in the `nlink` field and the `next_inode` field contains the index of the next free inode, and the last free inode contains -1 for the index. For inodes in use, the `next_inode` field is not used. The head of the free inode list is a index into the inode region. The inodes in the inode region are all contiguous. Independent of block boundaries, the whole region is viewed as one big array. So it is possible for there to be an inode that overlaps two blocks.

The size field of the inode is used to determine which data block pointers are valid. If the file is small enough to be fit in `N_DBLOCKS` blocks, the indirect blocks are not used. Note that there may be more than one indirect block. However, there is only one pointer to a double indirect block and one pointer to a triple indirect block. All block pointers are indexes relative to the start of the data block region.

The free block list is maintained as a linked list. The first four bytes of a free block contain an integer index to the next free block; the last free block contains -1 for the index. The head of the free block list is also a index into the data block region.


## 2. Goal

Read in the disk image, find inodes containing valid files, and write out a new image containing the same set of files, with the same inode numbers, but **with all the blocks in a file laid out contiguously**. Thus, if a file originally contains blocks `{6,2,15,22,84,7}` and it is relocated to the beginning of the data section, then the new blocks would be `{0,1,2,3,4,5}`. If there are indirect pointer blocks, the block containing the pointers need to be before the blocks being pointed to, and this applies to all level of indirect pointers.

After defragmenting, new disk image should contain the same boot block (just copy it), a new superblock with the same list of free inodes but a new list of free blocks sorted from lowest to highest (to prevent future fragmentation), new inodes for valid files, and data blocks at their new locations.

Do not need to copy/save the contents of free blocks when you create the defragmented disk. They can be left as they are, initialized to zeros, or anything else as long as the free list itself is well constructed, i.e., it is a valid linked list.

Program should take one parameter, i.e., the name of the disk image (e.g., `images_frag/disk_frag_1`). The output disk image should be named as "`disk_defrag`". 

## 3. Disk images 
You can find two directories: `images_frag` and `images_defrag`. The `images_frag` directory contains test input images: `disk_frag_1`, `disk_frag_2`, and `disk_frag_3`. The expected outputs are in the `images_defrag` directory. That is, for an input image `disk_frag_x`, your output should be identical to `disk_defrag_x`. You can use `diff` command. 

The input images follow the following rules:
  * All unused inodes have -1 for the last four index-related pointer fields and 0 for other fields after next_inode. For inodes in use, index-related pointer fields that are not used should have values -1.
  * A data block used as an indirect pointer block will have -1 for all the unused pointers beyond file size.
  * A free data block will contain only 0 after the first four bytes for pointing to the next free data block. A data block in use will contain only 0 beyond the file size.
  * Unused bytes in the super block and the inode region are 0. For the inode region, if the total size is 1024 bytes, then the last 24 bytes are unused bytes, as each inode is 100 bytes and the inode region can hold 10 inodes.
