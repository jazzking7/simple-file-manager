#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "sfs_api.h"
#include "disk_emu.h"

// Deleted all my files 2 days before deadline by accident and I didn't have backup :)
// Not enough time to comment and format the code
// Hopefully I won't get a bad grade

/* INODES */
// ========================================================================================================
/* Initialization Functions*/
int num_inodes; // inode count
struct inode* inode_t; //inode table
struct inode* get_inode_from_number(int number){ // Get entry from inode table
return &inode_t[number];
}
/* Free Space Management */
struct bmap_entry* free_blocks;
struct bmap_entry* get_bmap_entry(int index){
    return &free_blocks[index];
}
/* Starting a new disk */
// ==================================================================================================
void init_inode(struct inode* node){
    node->size = 0;
    for(int i = 0; i < 12; i++){
        node->direct_addrs[i] = -1;
    }
    node->indirect_addr = -1;
    node->available = 1;
}

void create_inode(int inode_number){
    struct inode* new_node = (struct inode*)malloc(sizeof(struct inode));
    init_inode(new_node);
    inode_t[inode_number] = *new_node;
    update_inode_entry(inode_number);
}

// Starting a new disk
void init_all_inodes(){
    inode_t = (struct inode*) malloc(sizeof(struct inode) * num_inodes);
    for(int i = 0; i < num_inodes; i++){
        create_inode(i);
    }
    // set up root node
    struct inode* root_inode = get_inode_from_number(ROOT_INODE_NUMBER);
    root_inode->direct_addrs[0] = FIRST_DATABLOCK_ADDRESS;
    root_inode->available = 0;
    update_inode_entry(ROOT_INODE_NUMBER);
}
// ==================================================================================================
/* Loading from old disk */
// ==================================================================================================
void load_inode(int inode_number){
    struct inode* entry = (struct inode*)malloc(sizeof(struct inode));
    int buffer[sizeof(struct inode) / sizeof(int)];
    read_inode_entry(inode_number, buffer);
    entry->size = buffer[0];
    for(int i = 0; i < 12; i++){
        entry->direct_addrs[i] = buffer[1 + i]; 
    }
    entry->indirect_addr = buffer[13];
    entry->available = buffer[14];
    if(!entry->available){
    }
    inode_t[inode_number] = *entry;
}
void load_all_inodes(){
    inode_t = (struct inode*) malloc(sizeof(struct inode) * num_inodes);
    for(int i = 0; i < num_inodes; i++){
        load_inode(i);
    }
}
// ==================================================================================================
// Get specific address & Free space allocation
int get_block_address(int inode_number, int* fileptr, int* blockptr, int length){
    // printf("Get block address for inode %d at byte %d.\n", inode_number, *fileptr);
    struct inode* node = get_inode_from_number(inode_number);
    int block_addr;

    // Direct pointer
    if (*fileptr < BLOCKSIZE * 12){
        int index = *fileptr / BLOCKSIZE;

        allocate_direct_blocks(inode_number, index);

        block_addr = node->direct_addrs[index];
        *blockptr = *fileptr-(BLOCKSIZE * index);
        //printf("Block pointer has been set to byte %d of the direct block at index %d.\n", *blockptr, index);

    // Indirect pointer
    } else{
        if(node->indirect_addr < 0){
            allocate_indirect_block(inode_number);
        }

        int indirect_file_ptr = *fileptr - BLOCKSIZE * 12;
        int indirect_index = indirect_file_ptr / BLOCKSIZE;
        int indirect_blockptr = indirect_index * sizeof(int);

        if(indirect_blockptr  >= BLOCKSIZE){
            return -1;
        }
        if(*fileptr + length > node->size){
            int stored_address;
            read_block_at_byte(node->indirect_addr, &stored_address, indirect_blockptr, sizeof(int));
            if(stored_address == 0){
                allocate_indirect_data_blocks(inode_number, indirect_blockptr);
            }
        }
        read_block_at_byte(node->indirect_addr, &block_addr, indirect_blockptr, sizeof(int));
        *blockptr = indirect_file_ptr - indirect_index * BLOCKSIZE;
    }
    //printf("Block address has been retrieved: %d.\n", block_addr);
    return block_addr;
}

void allocate_direct_blocks(int inode_number, int index){
    struct inode* node = get_inode_from_number(inode_number);
    for(int i = 0; i <= index; i++){
        if(node->direct_addrs[i] == -1){
            //printf("Allocating free data block to direct block address index %d of inode %d.\n", index, inode_number);
            node->direct_addrs[i] = get_free_data_block();
            update_inode_entry(inode_number);
            //printf("Direct block address index %d of inode %d has been assigned the free data block %d.\n", i, inode_number, node->direct_addrs[index]);
        }
    }
}

void allocate_indirect_block(int inode_number){
    struct inode* node = get_inode_from_number(inode_number);
    node->indirect_addr = get_free_data_block();
    char buffer[BLOCKSIZE] = {0};
    write_block_at_byte(node->indirect_addr, buffer, 0, BLOCKSIZE);
    update_inode_entry(inode_number);
}

void allocate_indirect_data_blocks(int inode_number, int blockptr){
    //printf("Allocating free datablock using indirect address for i-node %d at indirect block pointer %d\n", inode_number, blockptr);
     // Allocate indirect block
    struct inode* node = get_inode_from_number(inode_number);
    int new_data_block = get_free_data_block();
    write_block_at_byte(node->indirect_addr, &new_data_block, blockptr, sizeof(int));
    //printf("Indirect data block has been allocated for i-node %d.\n", inode_number);
}
// ==================================================================================================

void update_inode_entry(int inode_number){
    // Get inode block index and address
    int first_inode_block = SUPERBLOCK_SIZE;
    int inode_per_block = BLOCKSIZE / sizeof(struct inode);
    int inode_block_index = inode_number / inode_per_block;

    int block_addr = first_inode_block + inode_block_index;
    int blockptr = (inode_number - inode_block_index * inode_per_block) * sizeof(struct inode);

    struct inode* entry = get_inode_from_number(inode_number);

    int buffer[sizeof(struct inode) / sizeof(int)];
    buffer[0] = entry->size;
    for(int i = 0; i < 12; i++){
        buffer[1 + i] = entry->direct_addrs[i];  
    }
    buffer[13] = entry->indirect_addr;
    buffer[14] = entry->available;
    write_block_at_byte(block_addr, buffer, blockptr, sizeof(struct inode));
    //printf("Successfully updated i-node %d\n", inode_number);
}

void read_inode_entry(int inode_number, int* buffer){

    int first_inode_block = SUPERBLOCK_SIZE;
    int inode_per_block = BLOCKSIZE / sizeof(struct inode);
    int inode_block_index = inode_number / inode_per_block;

    int block_address = first_inode_block + inode_block_index;
    int blockptr = (inode_number - inode_block_index * inode_per_block) * sizeof(struct inode);

    read_block_at_byte(block_address, buffer, blockptr, sizeof(struct inode));
}

int read_block_at_byte(int block_address, void* buffer, int blockptr, int length){

    // Load block
    char* block_data = malloc(BLOCKSIZE);
    read_blocks(block_address, 1, block_data);
    //printf("Block at address %d loaded to memory.\n", block_address);

    // Point to file pointer
    void* start_reading_from = &(block_data[blockptr]);
    //printf("Block pointer has been moved to start point.\n");
    
    // Set up number of bytes to read
    int read_bytes = length;
    int bytes_left_in_block = BLOCKSIZE - blockptr;
    if(read_bytes > bytes_left_in_block){
        read_bytes = bytes_left_in_block;
    }

    // Read to buffer
    //printf("Reading %d bytes from the block to the buffer.\n", read_bytes);
    memcpy(buffer, start_reading_from, read_bytes);
    free(block_data);
    return read_bytes;
}

int read_from_inode(int inode_number, char* buffer, int* fileptr, int length){

    //printf("Reading from i-node %d at byte %d.\n", inode_number, *fileptr);
    // Initiate block pointer and set up read counter
    int* blockptr = malloc(sizeof(int));
    int block_address;
    int read_bytes;
    int read_so_far = 0;
    // Set read limit to file size.
    int max_length = get_inode_from_number(inode_number)->size - *fileptr;
    if(length > max_length){
        length = max_length;
    }
    // read until limit is reached.
    do{
        block_address = get_block_address(inode_number, fileptr, blockptr, length);
        // Max File Size Reached
        if(block_address < 0){
            return read_so_far;
        }
        read_bytes = read_block_at_byte(block_address, &buffer[read_so_far], *blockptr, length);
        *fileptr += read_bytes;
        length -= read_bytes;
        read_so_far += read_bytes;
        //printf("%d bytes left to read\n", length);
    }
    while(length > 0);

    return read_so_far;
}

int write_block_at_byte(int block_address, void* buffer, int blockptr, int length){

    // Load block
    char* block_data = malloc(BLOCKSIZE);
    read_blocks(block_address, 1, block_data);
    //printf("Block at address %d has been loaded to memory.\n", block_address);

    // Point to file pointer
    void* start_writing_at = &(block_data[blockptr]);
    //printf("Block pointer has been moved to start point.\n");
    
    // Set up number of bytes to write
    int written_bytes = length;
    int bytes_left_in_block = BLOCKSIZE - blockptr;
    if(written_bytes > bytes_left_in_block){
        written_bytes = bytes_left_in_block;
    }

    // Write to buffer
    //printf("Writing %d bytes from the block to the buffer.\n", written_bytes);
    memcpy(start_writing_at, buffer, written_bytes);
    //printf("Buffer content has been copied\n");

    // Update disk
    write_blocks(block_address, 1, block_data);
    //printf("In-memory block has been written to the disk.\n");
    free(block_data);
    return written_bytes;
}

int write_to_inode(int inode_number, char* buffer, int* fileptr, int length){
    
    // Initiate block pointer and written bytes counter
    int* blockptr = malloc(sizeof(int));
    int block_addr;
    int written_bytes;
    int written_so_far = 0;

    // Write until limit is reached
    do{
        block_addr = get_block_address(inode_number, fileptr, blockptr, length);
        // Max file size reached
        if(block_addr < 0){
            return written_so_far;
        }
        written_bytes = write_block_at_byte(block_addr, &buffer[written_so_far], *blockptr, length);
        *fileptr += written_bytes;
        written_so_far += written_bytes;
        length -= written_bytes;
        //printf("%d bytes left to write\n", length);
    } while(length > 0);

    if(*fileptr > get_inode_from_number(inode_number)->size){
        get_inode_from_number(inode_number)->size = *fileptr;
        update_inode_entry(inode_number);
    }
    //printf("File pointer => byte %d.", *fileptr);
    //printf("%d bytes written to inode %d, %d left to write.\n",written_so_far, inode_number, length);

    return written_so_far;
}

int get_free_inode_number(){
    for(int i = 0; i < num_inodes; i++){
        struct inode* node = get_inode_from_number(i);
        if(node->available){
            node->available = 0;
            update_inode_entry(i);
            return i;
        }
    }
    return -1;
}

void remove_inode(int inode_number){
    struct inode* node = get_inode_from_number(inode_number);
    // Remove direct pointers
    for(int i = 0; i < 12; i++){
        int block_address = node->direct_addrs[i];
        if(block_address > 0){
            release_data_block(block_address);
        }
    }
    // Remove indirect pointers
    if(node->indirect_addr > 0){
        int entry_count = BLOCKSIZE/sizeof(int);
        int buffer[entry_count];
        read_block_at_byte(node->indirect_addr, buffer, 0, BLOCKSIZE);
        for(int i = 0; i < entry_count; i++){
            int indirect_data_block_to_free = buffer[i];
            if(indirect_data_block_to_free > 0){
                release_data_block(indirect_data_block_to_free);
            }
        }
        release_data_block(node->indirect_addr);
    }
    init_inode(node);
    update_inode_entry(inode_number);
}

/* Free Space Management */
//=========================================================================================================
// Start a fresh disk
void init_array(){
    if(DATABLOCK_COUNT * 4 > BITMAP_SIZE * BLOCKSIZE){
        printf("%s\n", "Not enough blocks allocated for the bitmap!");
        exit(1);
    }
    free_blocks = (struct bmap_entry*) malloc(sizeof(bmap_entry) * DATABLOCK_COUNT);
    for(int i = 0; i < DATABLOCK_COUNT; i++){
        struct bmap_entry entry = {-1, 0};
        free_blocks[i] = entry;
    }
}
void init_free_bmap(){
    //printf("Initializing a new free bitmap.\n");
    init_array();
    for(int i = 0; i < DATABLOCK_COUNT; i++){
        struct bmap_entry* entry = get_bmap_entry(i);
        entry->block_address = FIRST_DATABLOCK_ADDRESS + i;
        update_bmap_entry(i, 1);
    }
    update_bmap_entry(ROOT_INODE_NUMBER, 0);
}
// Load existing disk
void load_free_bmap(){
    init_array();
    for(int i = 0; i < DATABLOCK_COUNT; i++){
        int available;
        read_block_at_byte(get_bmap_block_address_from_index(i), &available, get_bmap_blockpointer_from_index(i), sizeof(int));
        get_bmap_entry(i)->available = available;
        get_bmap_entry(i)->block_address = FIRST_DATABLOCK_ADDRESS + i;
    }
}
//=========================================================================================================
int get_bmap_blockpointer_from_index(int index){
    int bytes_till_index = index * sizeof(int);
    return bytes_till_index % BLOCKSIZE;
}

int get_bmap_block_address_from_index(int index){
    int bytes_till_index = index * sizeof(int);
    int block_index = bytes_till_index / BLOCKSIZE;
    return FIRST_BITMAPBLOCK_ADDRESS + block_index;
}

void update_bmap_entry(int index, int available_status){
    
    struct bmap_entry* entry = get_bmap_entry(index);
    entry->available = available_status;
    //printf("Updated bmap entry at index %d with status: %d.\n", index, available_status);

    char* buffer = malloc(sizeof(int));
    memcpy(buffer, &entry->available, sizeof(int));
    //printf("Buffer has been setup.\n");

    //printf("Update bmap entry on the disk.\n");
    int block_addr = get_bmap_block_address_from_index(index);
    int blockptr = get_bmap_blockpointer_from_index(index);
    if(blockptr < 0){
        printf("Invalid block pointer %d with address $%d for index %d!", blockptr, block_addr, index);
        exit(1);
    }
    write_block_at_byte(block_addr, buffer, blockptr, sizeof(int));
    //printf("Bitmap update completed.\n");
}

int get_free_data_block(){
    //printf("Getting a free data block\n");
    for(int i = 0; i < DATABLOCK_COUNT; i++){
        struct bmap_entry* entry = get_bmap_entry(i);
        if(entry->available){
            update_bmap_entry(i, 0);
            return entry->block_address;
        }
    }
    printf("ERROR: RAN OUT OF DATA BLOCKS!");
    exit(1);
    return -1;
}

void release_data_block(int block_address){
    //printf("Freeing block_address %d from bmap.\n", block_address);
    int index = block_address - FIRST_DATABLOCK_ADDRESS;
    // Delete all
    char buffer[BLOCKSIZE] = {0};
    write_block_at_byte(block_address, buffer, 0, BLOCKSIZE);
    // Update bitmap
    update_bmap_entry(index, 1);
    //printf("The block at address %d has been freed.", block_address);
}
//=========================================================================================================

/* DIRECTORY */
//====================================================================================================================
struct dir_entry* directory;
struct inode* root;
int dir_fileptr = 0;
int next_file_index = 0; // directory iterator
struct dir_entry* get_dir_entry(int index){ // get directory entry at given index
    return &directory[index];
}
int get_dir_size(){
    int size = (root->size)/(FILENAMEBYTES + sizeof(int) * 2);
    return size;
}
/* Initialization */
// Creating new disk
// ===================================================================================================
void init_new_directory(){
    //printf("Initializing a new directory\n");
    root = get_inode_from_number(ROOT_INODE_NUMBER);
    directory = (struct dir_entry*) malloc(sizeof(struct dir_entry) * num_inodes);

    for(int i = 0; i < num_inodes; i++){
        struct dir_entry entry = {"", -1, 1};
        directory[i] = entry;
    }
    //printf("Successful initialization, maximum file count: %d\n", num_inodes);
}
// ===================================================================================================
// Load from old disk
// ===================================================================================================
void load_directory(){
    //printf("Loading from existing directory.\n");
    root = get_inode_from_number(ROOT_INODE_NUMBER);
    directory = (struct dir_entry*) malloc(sizeof(struct dir_entry) * num_inodes);

    for(int i = 0; i < num_inodes; i++){
        struct dir_entry entry = {"", -1, 1};
        directory[i] = entry;
    }

    int recovered_files = 0;
    for(int i = 0; i < get_dir_size(); i++){
        
        // create buffer to read from existing inode
        char* buffer = (char*) malloc(FILENAMEBYTES + 2*sizeof(int));
        read_from_inode(ROOT_INODE_NUMBER, buffer, &dir_fileptr, FILENAMEBYTES + 2*sizeof(int));
        //printf("Data retrieved from inode: '%s'\n.", buffer);

        char* name = (char*) malloc(FILENAMEBYTES);
        int inode_number;
        int available;

        // copy data from buffer
        memcpy(name, buffer, FILENAMEBYTES);
        memcpy(&inode_number, &buffer[FILENAMEBYTES], sizeof(int));
        memcpy(&available, &buffer[FILENAMEBYTES + sizeof(int)], sizeof(int));
        free(buffer);
        
        // transfer data from buffer to current dir_entry
        struct dir_entry* entry = get_dir_entry(i);
        entry->filename = name;
        entry->inode_number = inode_number;
        entry->available = available;

        if(!entry->available){
            recovered_files += 1;
            /*
            printf("New dir_entry recovered, with following data:");
            printf("Index: %d\tfilename: '%s'\tinode number: %d\n", i, entry->filename, entry->inode_number);
            printf("available: %d\tsize: %d.\n", entry->available, get_inode_from_number(inode_number)->size);
            */
        }
    }
    //printf("Recovered %d files in total.\n", recovered_files);
}
// ===================================================================================================

void update_dir_entry(int index){
    //printf("Updating dir_entry in disk, with index %d.\n", index);
    struct dir_entry* entry = get_dir_entry(index);
    int fileptr = index * (FILENAMEBYTES + 2*sizeof(int));
    //printf("The filepointer is %d.\n", fileptr);

    char* buffer = malloc(FILENAMEBYTES + 2 * sizeof(int));
    memcpy(buffer,entry->filename, FILENAMEBYTES);
    int int_buff[2];
    int_buff[0] = entry->inode_number;
    int_buff[1] = entry->available;
    memcpy(&buffer[FILENAMEBYTES], int_buff, 2*sizeof(int));
    //printf("The buffer has been setup %s.\n", buffer);

    write_to_inode(ROOT_INODE_NUMBER, buffer, &fileptr, FILENAMEBYTES + 2*sizeof(int));
    free(buffer);
    //printf("Update successful.\n", index);
}

/* Dir_entry creation */
// ===================================================================================================
int init_dir_entry(int index, char * filename){
    //printf("Initializing a directory entry for %s at directory index %d.\n", filename, index);

    // Get filename
    char* name =  (char*) malloc(FILENAMEBYTES);
    strcpy(name, filename);

    // Get directory entry from given index
    struct dir_entry* entry = get_dir_entry(index);
    // Get free inode number
    entry->inode_number = get_free_inode_number();

    if(entry->inode_number == -1){
        return -1;
    }
    entry->filename = name;
    // No longer free
    entry->available = 0;

    update_dir_entry(index);
    //printf("Updated dir_entry.");
    return entry->inode_number;
}

int create_new_entry(char* filename){
    //printf("Creating new directory entry for file %s \n", filename);
    struct dir_entry* new_entry;
    // Find available slot from currently created/initialized directory slots
    for(int i = 0; i < get_dir_size(); i++){
        new_entry = get_dir_entry(i);
        if(new_entry->available){
            //printf("Free dir_entry slot found.\n");
            return init_dir_entry(i, filename);
        }
    }
    //printf("No usable dir_entry slot found for file %s. Initialite new dir_entry\n", filename);
    //printf("Current directory size: %d\n", get_dir_size());
    int inode_number = init_dir_entry(get_dir_size(), filename);
    //printf("New directory size: %d\n", get_dir_size());
    return inode_number;
}

// Get the inode number of a file, if the file does not exist, create new entry in
// directory if permitted and return its inode number.
int get_inode_number_of_file(char* filename, int create_flag){
    //printf("Fetching inode number for %s\n", filename);
    for(int i = 0; i < get_dir_size(); i++){
        struct dir_entry* entry = get_dir_entry(i);
        if(!entry->available && strcmp(entry->filename, filename) == 0){
            //printf("%s exists in the directory, with i-node number %d.\n", filename, entry->inode_number);
            return entry->inode_number;
        }
    }
    //printf("%s DNE, create if necessary\n", filename);
    if(create_flag){
        return create_new_entry(filename);
    }else{
        return -1;
    }
}

void remove_dir_entry(char* filename){
    struct dir_entry* entry;
    for(int i = 0; i < get_dir_size(); i++){
        entry = get_dir_entry(i);
        if(strcmp(entry->filename, filename) == 0){
            int inode_number = entry->inode_number;
            remove_inode(inode_number);
            entry->available = 1;
            entry->filename = "";
            entry->inode_number = -1;
            update_dir_entry(i);
            return;
        }
    }
}
//=========================================================================================================================

/* FDT */
//=========================================================================================================================
struct fd_entry* fd_table;
struct fd_entry* get_fd_entry(int index);
struct fd_entry* get_fd_entry(int index){
    return &fd_table[index];
}

void init_fdt(){
    //printf("Initializing file descriptor table with %d entries.\n", num_inodes);
    fd_table = (struct fd_entry*) malloc(sizeof(struct fd_entry) * num_inodes);

    for(int i = 0; i < num_inodes; i++){
        struct fd_entry entry = {-1, -1, 1};
        fd_table[i] = entry;
    }
    //printf("%s\n", "Success.");
}

int init_fd_entry(int index, int inode_number){
    struct fd_entry* entry = get_fd_entry(index);
    entry->inode_number = inode_number;
    entry->fileptr = 0;
    entry->available = 0;
    return index;
}

int create_new_fd_entry(int inode_number){
    struct fd_entry* entry;
    for(int i = 0; i < num_inodes; i++){
        entry = get_fd_entry(i);
        if(entry->available){
            int fd = init_fd_entry(i, inode_number);
            return fd;
        }
    }
    printf("%s\n", "No fd entries available!");
    exit(1);
    return -1;
}

int get_fd_from_inode_number(int inode_number){
    struct fd_entry* entry;
    for(int i = 0; i < num_inodes; i++){
        entry = get_fd_entry(i);
        if((!(entry->available)) && (entry->inode_number == inode_number)){
            //printf("File descriptor found, the fd is %d for i-node: %d.\n", i, inode_number);
            return i;
        }
    }
    // Create new fd entry if fd not found
    //printf("Creating new file descriptor for i-node: %d.\n", inode_number);
    return create_new_fd_entry(inode_number);
}

int fd_opened(int inode_number){
    struct fd_entry* entry;
    for(int i = 0; i < num_inodes; i++){
        entry = get_fd_entry(i);
        if(entry->inode_number == inode_number){
            if(entry->available){
                return 0;
            }else{
                return 1;
            }
        }
    }
    return 0;
}
//=========================================================================================================

/* IMPLEMENTATION OF SFS FUNCTIONS */
//=========================================================================================================
// Helper functions for mksfs
void init_superblock(){
    int file_system_size = TOTALBLOCKCOUNT * BLOCKSIZE;
    int superblock[5];
    superblock[0] = 0xACBD0005;
    superblock[1] = BLOCKSIZE;
    superblock[2] = file_system_size;
    superblock[3] = num_inodes;
    superblock[4] = ROOT_INODE_NUMBER;
    int superblock_address = 0;
    write_block_at_byte(superblock_address, superblock, 0, 5*sizeof(int));
}

void create_new_disk(){
    init_fresh_disk(DISKNAME, BLOCKSIZE, TOTALBLOCKCOUNT);
    init_superblock();
    init_free_bmap();
    init_all_inodes();
    init_new_directory();
    init_fdt();
}

void load_old_disk(){
    init_disk(DISKNAME, BLOCKSIZE, TOTALBLOCKCOUNT);
    load_free_bmap();
    load_all_inodes();
    load_directory();
    init_fdt();
}

void mksfs(int fresh_flag){
    //printf("%s\n", "Initialize Disk");
    num_inodes = (INODET_SIZE*BLOCKSIZE)/sizeof(struct inode);
    if(fresh_flag){
        create_new_disk();
    }
    else{
        load_old_disk();
    }
    //printf("%s\n", "Disk has been initialized.");
}

int sfs_getnextfilename(char* filename){
    //printf("Fetching the file after %s\n", filename);
    if(get_dir_size() <= 0){
        filename =  (char*) malloc(FILENAMEBYTES);
        strcpy(filename, " ");
        return 0;
    }
    // Go through all directory entries and get the next unavailable entry
    for(next_file_index; next_file_index < get_dir_size(); next_file_index++){
        struct dir_entry* entry = get_dir_entry(next_file_index);
        if(!entry->available){
            strcpy(filename, entry->filename);
            next_file_index++;
            return 1;
        }
    }
    next_file_index = next_file_index % get_dir_size();
    filename =  (char*) malloc(FILENAMEBYTES);
    strcpy(filename, " ");
    return 0;
}

int sfs_getfilesize(const char* filename){
    char* name = (char*) malloc(strlen(filename));
    strcpy(name, filename);
    int inode_number = get_inode_number_of_file(name, 0);
    if(inode_number < 0){
        return -1;
    }
    int size = get_inode_from_number(inode_number)->size;
    return size;
}

int sfs_fopen(char* filename){
    //printf("Opening file: %s\n", filename);
    if(strlen(filename) > MAXFILENAME){
        printf("Filename '%s' contains %d characters, which exceeds the %d character limit!\n", filename, strlen(filename), MAXFILENAME);
        return -1;
    }

    // Get the inode number of the file, create new entry in directory 
    // if the file has never been opened before.
    int inode_number = get_inode_number_of_file(filename, 1);
    if(inode_number == -1){
        printf("Not enough inodes! inodes count: %d.\n", num_inodes);
    }

    // Get the fd of the file, create new fd entry
    // if the file has never been opened before
    int fd = get_fd_from_inode_number(inode_number);
    sfs_fseek(fd, get_inode_from_number(inode_number)->size);
    //printf("%s is opened.\n", filename);
    return fd;
}

int sfs_fclose(int fd){
    //printf("Closing fd_entry: %d\n", fd);
    if(fd < 0){
        //printf("Invalid file descriptor!\n");
        return -1;
    }
    struct fd_entry* entry = get_fd_entry(fd);
    if(entry->available){
        //printf("%s\n", "Entry has not been opened!");
        return -1;
    }
    entry->inode_number = -1;
    entry->fileptr = -1;
    entry->available = 1;
    //printf("fd %d has been closed.\n", fd);
    return 0;
}

int sfs_fwrite(int fd, const char* buffer, int length){
     if(fd < 0){
        printf("Invalid file descriptor %d! Verify the i-node count.\n", fd);
        return -1;
    }
    //printf("Starts writing to: %d\n", fd);
    struct fd_entry* entry = get_fd_entry(fd);
    if(entry->available){
        //printf("%s\n", "fd entry has not been opened!");
        return -1;
    }
    
    // Get inode number and set up pointer
    int inode_number = entry->inode_number;
    int* fileptr = (int*) malloc(sizeof(int));
    *fileptr = entry->fileptr;

    // Write to corresponding inode
    //printf("Writing to inode %d\n", inode_number);
    int bytes_written = write_to_inode(inode_number, buffer, fileptr, length);
    //printf("Writing completed. %d bytes have been written to inode %d.\n", bytes_written, inode_number);

    // Update pointer
    entry->fileptr = *fileptr;
    //printf("Updated filepointer from %d to %d\n", entry->fileptr, *fileptr);
    free(fileptr);

    return bytes_written;
}

int sfs_fread(int fd, char* buffer, int length){
    if(fd < 0){
        printf("Invalid file descriptor %d! Verify the i-node count\n", fd);
        return -1;
    }
    //printf("Reading from file with fd %d\n", fd);
    struct fd_entry* entry = get_fd_entry(fd);
    if(entry->available){
        //printf("%s\n", "Entry is not opened!");
        return -1;
    }

    // Get inode number and set up pointer
    int inode_number = entry->inode_number;
    int* fileptr = (int*) malloc(sizeof(int));
    *fileptr = entry->fileptr;

    // Read from corresponding inoe
    int bytes_read = read_from_inode(inode_number, buffer, fileptr, length);
    //printf("Finished reading from inode numbered %d\n", inode_number);

    // Update pointer
    entry->fileptr = *fileptr;
    //printf("Updated filepointer from %d to %d\n", entry->filepointer, *fileptr);
    free(fileptr);

    return bytes_read;
}

int sfs_fseek(int fd, int loc){
    if(fd < 0){
        printf("Invalid file descriptor %d! Verify the i-node count\n", fd);
        return -1;
    }
    //printf("Fseek on fd %d to location %d\n", fd, loc);
    // Get fd entry
    struct fd_entry* entry = get_fd_entry(fd);
    if(entry->available){
        printf("%s\n", "Entry is not opened!");
        return -1;
    }
    // Update pointer
    entry->fileptr = loc;
    //printf("Success, new location of the fd_entry: %d.\n", entry->fileptr);
    return 0;
}

int sfs_remove(char* filename){
    // Get inode associated with filename
    int inode_number = get_inode_number_of_file(filename, 0);
    //printf("File associated with inode_number: %d.\n", inode_number);

    if(inode_number < 0){
        printf("File '%s' does not exist!.\n", filename);
        return -1;
    }

    if(fd_opened(inode_number)){
        printf("The file '%s' is currently opened!\n", filename);
        return -1;
    }
    // Remove file
    remove_dir_entry(filename);
    return 0;
}
//=========================================================================================================