#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>

// On-disk file system format.
// Both the kernel and user programs use this header file.

#define ROOTINO 1  // root i-number
#define BSIZE 512  // block size

// Disk layout:
// [ boot block | super block | log | inode blocks |
//                                          free bit map | data blocks]
//
// mkfs computes the super block and builds an initial file system. The
// super block describes the disk layout:
struct superblock {
  uint size;         // Size of file system image (blocks)
  uint nblocks;      // Number of data blocks
  uint ninodes;      // Number of inodes.
  uint nlog;         // Number of log blocks
  uint logstart;     // Block number of first log block
  uint inodestart;   // Block number of first inode block
  uint bmapstart;    // Block number of first free map block
};

#define NDIRECT 12
#define NINDIRECT (BSIZE / sizeof(uint))
#define MAXFILE (NDIRECT + NINDIRECT)

// On-disk inode structure
struct dinode {
  short type;           // File type
  short major;          // Major device number (T_DEV only)
  short minor;          // Minor device number (T_DEV only)
  short nlink;          // Number of links to inode in file system
  uint size;            // Size of file (bytes)
  uint addrs[NDIRECT+1];   // Data block addresses
};

// Inodes per block.
#define IPB           (BSIZE / sizeof(struct dinode))

// Block containing inode i
#define IBLOCK(i, sb)     ((i) / IPB + sb.inodestart)

// Bitmap bits per block
#define BPB           (BSIZE*8)

// Block of free map containing bit for block b
#define BBLOCK(b, sb) (b/BPB + sb.bmapstart)

// Directory is a file containing a sequence of dirent structures.
#define DIRSIZ 14

struct dirent {
  ushort inum;
  char name[DIRSIZ];
};

int main(int argc, char* argv[]) {
  int img;
  img = open(argv[1], O_RDONLY);
  // Validate FS image
  if (img == -1) {
    fprintf(stderr, "ERROR: image not found.\n");
    exit(1);
  }

  struct stat imgStat;
  int check;
  check = fstat(img, &imgStat);
  assert(check == 0);

  // Pointer to beginning of FS
  void* imgPtr = mmap(NULL, imgStat.st_size, PROT_READ, MAP_PRIVATE, img, 0);
  // First inode block pointer
  struct superblock *sb = (struct superblock*)(imgPtr + BSIZE);
  // Inode pointer
  struct dinode *curr = (struct dinode*) (imgPtr + sb->inodestart * BSIZE);

  for (int i = 0; i < sb->ninodes; i++) {
    // Inode checks begin
    struct dinode *di = (struct dinode*) (imgPtr + sb->inodestart * BSIZE);
    di++;

    for (int j = 0; j < sb->ninodes; j++) {
      int blocks = 0;
      // Inode type out of bounds
      if (di->type > 3 || di->type < 0) {
        fprintf(stderr, "ERROR: bad inode.\n");
        exit(1);
      }

      // Determine size
      for (int k = 0; k < NDIRECT; k++) {
        if (di->addrs[k] != 0) {
          blocks++;
        }
      }

      // Indirect size
      uint *a = (uint*) (imgPtr + (di->addrs[NDIRECT] * BSIZE));
      if (di->addrs[NDIRECT]) {
        for (int j = 0; j < NINDIRECT; j++) {
          if (*a) {
            blocks++;
          }
          a++;
        }
        if (abs((blocks*BSIZE) - di->size) > 512) {
          fprintf(stderr, "ERROR: bad size in inode.\n");
          exit(1);
        }
      }
      di++;
    }
    // End inode checks

    // Root directory check
    if (i == 1) {
      // The first inode must be a directory
      if (curr->type != 1) {
        fprintf(stderr, "ERROR: root directory does not exist.\n");
        exit(1);
      }

      // Verify the inum of the first and second blocks
      struct dirent *currDir = (struct dirent*)(imgPtr+curr->addrs[0]*BSIZE);
      struct dirent *parentDir = currDir++;

      if (currDir->inum != ROOTINO || parentDir->inum != ROOTINO) {
        fprintf(stderr, "ERROR: root directory does not exist.\n");
        exit(1);
      }
    } else if (i > 1) {  // Current directory check
      // Check if inode is a directory
      if (curr->type == 1) {
        struct dirent *currDir = (struct dirent*)(imgPtr+curr->addrs[0]*BSIZE);
        // Check if indecies match
        if (currDir->inum != i) {
          fprintf(stderr, "ERROR: current directory mismatch.\n");
          exit(1);
        }
      }
      // Next inode
    }
    curr++;
  }
}
