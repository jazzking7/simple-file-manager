#ifndef SFS_API_H
#define SFS_API_H

// Structure of the sfs
// | Superblock | I-node table | Data blocks | Free BitMap |
#define DISKNAME "disk.sfs"
#define MAXFILENAME 32
#define FILENAMEBYTES (MAXFILENAME + 1)
#define BLOCKSIZE 1024
#define SUPERBLOCK_SIZE 1
#define INODET_SIZE 128
#define DATABLOCK_COUNT 1024
#define BITMAP_SIZE 10 // CEIL(DATABLOCKCOUNT/(BLOCKSIZE/4))
#define TOTALBLOCKCOUNT (SUPERBLOCK_SIZE + INODET_SIZE + DATABLOCK_COUNT + BITMAP_SIZE)
#define ROOT_INODE_NUMBER 0
#define FIRST_DATABLOCK_ADDRESS (SUPERBLOCK_SIZE + INODET_SIZE)
#define FIRST_BITMAPBLOCK_ADDRESS (SUPERBLOCK_SIZE + INODET_SIZE + DATABLOCK_COUNT)

// I-nodes
typedef struct inode{
    int size;
    int direct_addrs[12];
    int indirect_addr;
    int available;
} inode;
// Free Space Bitmap
typedef struct bmap_entry{
    int block_address;
    int available;
} bmap_entry;

// Functions:
struct inode* get_inode_from_number(int number);
void init_inode(struct inode* node);
void create_inode(int inode_number);
void init_all_inodes();
void load_inode(int inode_number);
void load_all_inodes();

int get_block_address(int inode_number, int* fileptr, int* blockptr, int length);
void allocate_direct_blocks(int inode_number, int index);
void allocate_indirect_data_blocks(int inode_number, int blockptr);
void allocate_indirect_block(int inode_number);

void update_inode_entry(int inode_number);
void read_inode_entry(int inode_number, int* buffer);

int read_block_at_byte(int block_address, void* buffer, int blockptr, int length);
int read_from_inode(int inode_number, char* buffer, int* fileptr, int length);

int write_block_at_byte(int block_address, void* buffer, int blockptr, int length);
int write_to_inode(int inode_number, char* buffer, int* fileptr, int length);

int get_free_inode_number();
void remove_inode(int inode_number);

void init_array();
void init_free_bmap();
void load_free_bmap();
void update_bmap_entry(int index, int available_status);
int get_free_data_block();
void release_data_block(int block_address);

// Directory
// Structure:
typedef struct dir_entry{
    char* filename;
    int inode_number;
    int available;
} dir_entry;
// Functions:
struct dir_entry* get_dir_entry(int index);
int get_dir_size();
void init_new_directory();
void load_directory();
void update_dir_entry(int index);
int init_dir_entry(int index, char* filename);
int create_new_entry(char* filename);
int get_inode_number_of_file(char* filename, int create_new_flag);
void remove_dir_entry(char* filename);

// FDT
// Structure:
typedef struct fd_entry{
    int inode_number;
    int fileptr;
    int available;
} fd_entry;
// Functions:
struct fd_entry* get_fd_entry(int index);
void init_fdt();
int init_fd_entry(int index, int inode_number);
int create_new_fd_entry(int inode_number);
int get_fd_from_inode_number(int inode_number);
int fd_opened(int inode_number);

// Provided Functions to be implemented
void mksfs(int);
int sfs_getnextfilename(char*);
int sfs_getfilesize(const char*);
int sfs_fopen(char*);
int sfs_fclose(int);
int sfs_fwrite(int, const char*, int);
int sfs_fread(int, char*, int);
int sfs_fseek(int, int);
int sfs_remove(char*);

#endif // SFS_API_H