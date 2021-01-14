/* file system format on disk and struct & operation in kernel-OS */
#ifndef FILE_SYSTEM_H
#define FILE_SYSTEM_H

#include <type.h>

#define BSIZE 4096            // block size: 4KB
#define SSIZE 512             // secotor size: 512B
#define START_SECTOR 1048576  // start sector_num in disk
#define FSYS_SIZE 1048576     // sectors = 512MB/512B

/**************************************************************************************************
Disk layout:
[ boot block | kernel image ] (512MB)
[ Superblock(512B) | block map(16KB) | inode map(512B) | inode blocks(256KB) | data blocks ] (512MB)
****************************************************************************************************/

//Initial Addr in MEM
#define SB_MEM_ADDR       0x5d000000
#define BMAP_MEM_ADDR     0x5d000200
#define IMAP_MEM_ADDR     0x5d004200
#define INODE_MEM_ADDR    0x5d004400
#define DATA_MEM_ADDR     0x5d044400

//Initial Offset in Disk (sector)
#define SB_SD_OFFSET       0             
#define BMAP_SD_OFFSET     1   
#define IMAP_SD_OFFSET     33
#define INODE_SD_OFFSET    34
#define DATA_SD_OFFSET     546

#define MAGIC_NUM 0x666666
#define P2V_OFFSET 0xffffffc000000000

#define ROOTINUM 0            // root-dir i-number
#define T_FILE  1             // type of target is a file
#define T_DIR   2             // type of target is a dir

#define IENTRY_SIZE 64        // inode entry size:64B
#define NDIRECT 8             // max_num of direct point
#define NINDIRECT (BSIZE / sizeof(uint))
#define MAXFILE (NDIRECT + NINDIRECT)
#define IPS  (SSIZE / sizeof(inode_t)) // Inodes per sector

#define DENTRY_SIZE 32        // directory entry size:32B
#define MAX_FILE_NAME 24

#define NOFILE   16           // max number of open files
#define A_R  1
#define A_W  2
#define A_RW 3

/* Super Block: describes the structure of the file system */
typedef struct superblock {
    uint32_t magic;                 // identify the file system
    uint32_t size;                  // Size of file system image (sectors)
    uint32_t start_sector;

    uint32_t blockmap_offset;       // by sector
    uint32_t blockmap_num;          

    uint32_t inodemap_offset;
    uint32_t inodemap_num;

    uint32_t inodes_offset;
    uint32_t inodes_num;

    uint32_t datablock_offset;
    uint32_t datablock_num;

    uint32_t inode_sz;
    uint32_t dentry_sz;
}superblock_t;

/* Inode: describes the structure of file/directory */
typedef struct inode {
    uint32_t inum;
    uint8_t  mode;                  // Inode mode: FILE/DIR
    uint8_t  access;                // File mode: R/W/RW
    uint16_t ref;                   // link num
    uint32_t size;                  // Size of file (bytes)
    uint32_t valid_size;            // used data/dentry bytes
    uint32_t direct_bnum[NDIRECT];  // Direct data block addresses
    uint32_t indirect1_bnum;        // First indirect address
    uint32_t indirect2_bnum;        // Double indirect address
    uint64_t mtime;                 // modified time
}inode_t;
inode_t *current_inode;

/* Directory: special file containing list of files and dir */
/* datablock of dir has several dentries for dir's contain */
typedef struct dentry {
    uint32_t mode;                  // FILE/DIR
    uint32_t inum;                  // inode number of file/dir in this dir
    char name[MAX_FILE_NAME];
    //uint8_t soft_link;
}dentry_t;

/* file descriptor: keeping information of open file */
typedef struct fd {      
    uint32_t inum; 
    uint8_t  access;
    uint32_t read_point;            // byte
    uint32_t write_point;
    //int used;
}fd_t;

fd_t FD_table[NOFILE];               // file descriptor table

/* file system operation */
void do_mkfs();
void do_statfs();
/* directory operation */
void do_cd(uintptr_t dirname);
void do_mkdir(uintptr_t dirname);
void do_rmdir(uintptr_t dirname);
void do_ls(uintptr_t dirname);
/* file operation */
void do_touch(uintptr_t filename);
void do_cat(uintptr_t filename);
long do_file_open(uintptr_t name, int access);
long do_file_read(int fd, uintptr_t buff, int size);
long do_file_write(int fd, uintptr_t buff, int size);
void do_file_close(int fd);
/* link operation */
void do_ln(uintptr_t source, uintptr_t link_name);
void do_ln_s(uintptr_t source, uintptr_t link_name);

void clear_sector(uintptr_t mem_addr);
void Set_block_map(uint32_t block_id);
void Set_inode_map(uint32_t inode_id);
void Clear_block_map(uint32_t block_id);
void Clear_inode_map(uint32_t inode_id);
uint32_t Alloc_datablock();
uint32_t Alloc_inode();
void write_inode_sector(uint32_t inum);
void init_dentry(uint32_t block_num, uint32_t cur_inum, uint32_t parent_inum);
inode_t *iget(uint32_t inum);
inode_t *dirlookup(inode_t *dp, char *name, int type);
int find_path(char * path, int type);
int Alloc_fd();

#endif