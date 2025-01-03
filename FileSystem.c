#include "FileSystem.h"


void initialize_file_system(FileSystem *fs, uint32_t num_blocks) {
    fs->total_blocks = num_blocks;
    int num_inode = num_blocks / INODE_BLOCK_RATIO;
    fs->total_inodes = num_inode;

    printf("Total blocks: %d\n", num_blocks);
    // Allocate memory for the block bitmap and blocks
    fs->block_bitmap = (bool *)malloc(num_blocks * sizeof(bool));
    if (fs->block_bitmap == NULL) {
        printf("Memory allocation for block bitmap failed!\n");
        exit(1);
    }

    fs->blocks = (Block *)malloc(num_blocks * sizeof(Block));
    if (fs->blocks == NULL) {
        printf("Memory allocation for blocks failed!\n");
        exit(1);
    }

    // Allocate memory for the inode bitmap and inodes
    fs->inode_bitmap = (bool *)malloc(num_blocks * sizeof(bool));
    if (fs->block_bitmap == NULL) {
        printf("Memory allocation for inode bitmap failed!\n");
        exit(1);
    }

    fs->inodes = (Inode *)malloc(num_blocks * sizeof(Inode));
    if (fs->block_bitmap == NULL) {
        printf("Memory allocation for inodes failed!\n");
        exit(1);
    }



    if (fs->block_bitmap == NULL || fs->blocks == NULL) {
        printf("Memory allocation failed!\n");
        exit(1);
    }


    // Initialize block bitmap
    #ifdef DEBUG
    printf("Initialize block bitmap !\n");
    #endif

    for (uint32_t i = 0; i < num_blocks; i++) {
        fs->block_bitmap[i] = false;
    }

    // Initialize inode bitmap
    #ifdef DEBUG
    printf("Initialize inode bitmap !\n");
    #endif
    for (int i = 0; i < num_inode; i++) {
        fs->inode_bitmap[i] = false;
    }


    printf("File System Memory allocated\n");

}

void cleanup_file_system(FileSystem *fs) {
    free(fs->block_bitmap);
    free(fs->blocks);
    free(fs->inodes);
    free(fs->inode_bitmap);
}


int allocate_block(FileSystem *fs) {
    for (uint32_t i = 0; i < fs->total_blocks; i++) {
        if (!fs->block_bitmap[i]) {
            fs->block_bitmap[i] = true; // Mark block as used
            return i;                   // Return block index
        }
    }
    return -1; // No free blocks
}

void free_block(FileSystem *fs, int block_index) {
    if (block_index >= 0 && block_index < fs->total_blocks) {
        fs->block_bitmap[block_index] = false; // Mark block as free
    }
}

int allocate_inode(FileSystem *fs) {
    for (int i = 0; i < fs->total_inodes; i++) {
        if (!fs->inode_bitmap[i]) {
            fs->inode_bitmap[i] = true; // Mark inode as used
            return i;                   // Return inode index
        }
    }
    return -1; // No free inodes
}

void free_inode(FileSystem *fs, int inode_index) {
    if (inode_index >= 0 && inode_index < fs->total_inodes) {
        printf("test_free    ");
        fs->inode_bitmap[inode_index] = false;// Mark inode as free

    }
}


void remove_from_directory(FileSystem *fs, int dir_inode_index, int child_inode_index, const char *name)
{
    Inode *dir_inode = &fs->inodes[dir_inode_index];
    if (!dir_inode->is_directory) {
        printf("Inode is not a directory!\n");
        return;
    }

    if (dir_inode->dir_entry_count >= MAX_DIR_ENTRIES) {
        printf("Directory is full!\n");
        return;
    }

    // Add a new entry
    DirectoryEntry *entry = &dir_inode->entries[dir_inode->dir_entry_count];
    entry->inode_index = child_inode_index;
    //strncpy(entry->name, name, MAX_FILENAME);
    printf("Before delete dir_entry_count:  %d\n", dir_inode->dir_entry_count);

    dir_inode->dir_entry_count--;

    printf("After delete dir_entry_count:  %d\n", dir_inode->dir_entry_count);
    return;
}

void write_to_file(FileSystem *fs, int inode_index, const uint8_t *data, uint32_t size) {
    Inode *inode = &fs->inodes[inode_index];
    uint32_t remaining = size;
    uint32_t block_index;

    for (int i = 0; i < DIRECT_POINTERS && remaining > 0; i++) {
        if (inode->blocks[i] == 0) {
            block_index = allocate_block(fs);
            if (block_index == -1) {
                printf("No free blocks available!\n");
                return;
            }
            inode->blocks[i] = block_index;
        }

        // Write data to block
        uint32_t to_write = (remaining > BLOCK_SIZE) ? BLOCK_SIZE : remaining;
        memcpy(fs->blocks[inode->blocks[i]].data, data, to_write);
         if (remaining <= BLOCK_SIZE) {
           fs->blocks[inode->blocks[i]].data[to_write] = '\0'; // Null-terminate
         }
        data += to_write;
        remaining -= to_write;
    }

    inode->size = size; // Update file size
}


void read_from_file(FileSystem *fs, int inode_index, uint8_t *buffer, uint32_t size) {
    Inode *inode = &fs->inodes[inode_index];
    uint32_t remaining = size;
    uint32_t block_index;

    for (int i = 0; i < DIRECT_POINTERS && remaining > 0; i++) {
        if (inode->blocks[i] == 0) {
           // printf("Block not allocated! i=%d\n",i);
            //return;
        }

        // Read data from block
        uint32_t to_read = (remaining > BLOCK_SIZE) ? BLOCK_SIZE : remaining;
        memcpy(buffer, fs->blocks[inode->blocks[i]].data, to_read);
        buffer += to_read;
        remaining -= to_read;
    }
    *buffer = '\0'; // Null-terminate the read data
}

int create_directory(FileSystem *fs, const char *name) {
    int inode_index = allocate_inode(fs);
    if (inode_index == -1) {
        printf("No free inodes available!\n");
        return -1;
    }

    Inode *inode = &fs->inodes[inode_index];
    inode->is_directory = true;
    inode->dir_entry_count = 0;
    strncpy(inode->filename, name, MAX_FILENAME);
    inode->size = 0; // Directory size is 0 (data stored in entries)
    return inode_index;
}

int add_to_directory(FileSystem *fs, int dir_inode_index, int child_inode_index, const char *name) {
    Inode *dir_inode = &fs->inodes[dir_inode_index];
    if (!dir_inode->is_directory) {
        printf("Inode is not a directory!\n");
        return -1;
    }

    if (dir_inode->dir_entry_count >= MAX_DIR_ENTRIES) {
        printf("Directory is full!\n");
        return -1;
    }

    // Add a new entry
    DirectoryEntry *entry = &dir_inode->entries[dir_inode->dir_entry_count];
    entry->inode_index = child_inode_index;
    strncpy(entry->name, name, MAX_FILENAME);

    dir_inode->dir_entry_count++;
    return 0;
}

void list_directory(FileSystem *fs, int dir_inode_index) {
    Inode *dir_inode = &fs->inodes[dir_inode_index];
    if (!dir_inode->is_directory) {
         printf("Inode is not a directory!\n");
         return;
    }
    printf("Contents of directory '%s':\n", dir_inode->filename);
    printf("!!dir_entry_count:  %d\n", dir_inode->dir_entry_count);
    for (uint32_t i = 0; i < dir_inode->dir_entry_count; i++) {
        printf("dir_entry_count:  %d\n", dir_inode->dir_entry_count);
         DirectoryEntry *entry = &dir_inode->entries[i];
         Inode *child = &fs->inodes[entry->inode_index];
         printf("  %s (inode %d, %s)\n", entry->name, entry->inode_index,
                child->is_directory ? "directory" : "file");
    }
}

int read_file_to_fs(FileSystem *fs, const char *external_filename, const char *internal_filename) {
    FILE *file = fopen(external_filename, "rb");
    if (!file) {
        printf("Failed to open external file '%s'.\n", external_filename);
        return -1;
    }

    int inode_index = allocate_inode(fs);
    if (inode_index == -1) {
        printf("No free inodes available!\n");
        fclose(file);
        return -1;
    }

    Inode *inode = &fs->inodes[inode_index];
    strncpy(inode->filename, internal_filename, sizeof(inode->filename));
    inode->is_directory = false;
    inode->size = 0;

    size_t bytes_read;
    uint32_t total_written = 0;
    uint8_t buffer[BLOCK_SIZE];
    int block_index;

    while ((bytes_read = fread(buffer, 1, BLOCK_SIZE, file)) > 0) {
        block_index = allocate_block(fs);
        if (block_index == -1) {
            printf("No free blocks available! File partially written.\n");
            break;
        }

        memcpy(fs->blocks[block_index].data, buffer, bytes_read);
        inode->blocks[inode->size / BLOCK_SIZE] = block_index;
        inode->size += bytes_read;
        total_written += bytes_read;
    }

    fclose(file);
    printf("File '%s' written to internal file system as '%s'. Total bytes: %u\n",
           external_filename, internal_filename, total_written);

    return inode_index;
}

int write_file_to_host(FileSystem *fs, int inode_index, const char *external_filename)
{
    // 取得模擬檔案系統裡的 inode
    Inode *inode = &fs->inodes[inode_index];
    if (inode->is_directory) {
        printf("'%s' is a directory, not a file.\n", inode->filename);
        return -1;
    }

    // 以 "wb" 模式開啟(或建立)外部檔案
    FILE *file = fopen(external_filename, "wb");
    if (!file) {
        printf("Failed to create external file '%s'.\n", external_filename);
        return -1;
    }

    // 將模擬檔案系統的資料寫入到 host 上的檔案
    size_t total_written = 0;
    size_t bytes_to_write = inode->size;
    uint8_t buffer[BLOCK_SIZE];

    for (uint32_t i = 0; i < (bytes_to_write + BLOCK_SIZE - 1) / BLOCK_SIZE; i++) {
        uint32_t block_index = inode->blocks[i];
        if (block_index >= fs->total_blocks) {
            printf("Invalid block index encountered during write.\n");
            fclose(file);
            return -1;
        }

        // 計算本次要寫入的區塊大小
        size_t chunk_size = (bytes_to_write > BLOCK_SIZE) ? BLOCK_SIZE : bytes_to_write;
        memcpy(buffer, fs->blocks[block_index].data, chunk_size);
        fwrite(buffer, 1, chunk_size, file);

        total_written += chunk_size;
        bytes_to_write -= chunk_size;
    }

    fclose(file);
    printf("File '%s' written to host file '%s'. Total bytes: %zu\n",
           inode->filename, external_filename, total_written);

    return total_written;
}

void save_file_system(FileSystem *fs, const char *image_filename) {
    FILE *file = fopen(image_filename, "wb");
    if (!file) {
        printf("Failed to create disk image file '%s'.\n", image_filename);
        return;
    }

    // Write the total number of blocks to the disk image
    fwrite(&fs->total_blocks, sizeof(uint32_t), 1, file);

    // Write the total number of inodes
    fwrite(&fs->total_inodes, sizeof(uint32_t), 1, file);

    // Write the block bitmap
    fwrite(fs->block_bitmap, sizeof(bool), fs->total_blocks, file);

    // Write the inode bitmap
    fwrite(fs->inode_bitmap, sizeof(bool), fs->total_inodes, file);

    // Write inodes
    fwrite(fs->inodes, sizeof(Inode), fs->total_inodes, file);

    // Write blocks
    for (uint32_t i = 0; i < fs->total_blocks; i++) {
        fwrite(&fs->blocks[i], sizeof(Block), 1, file);
    }

    fclose(file);
    printf("File system saved to disk image '%s'.\n", image_filename);
}


void load_file_system(FileSystem *fs, const char *image_filename) {
    FILE *file = fopen(image_filename, "rb");
    if (!file) {
        printf("Failed to open disk image file '%s'.\n", image_filename);
        return;
    }

    // Read the total number of blocks from the disk image
    fread(&fs->total_blocks, sizeof(uint32_t), 1, file);

    // Read the total number of inodes
    fread(&fs->total_inodes, sizeof(uint32_t), 1, file);

    #ifdef DEBUG
    printf("Total blocks: %d\n", fs->total_blocks);
    printf("Total inodes: %d\n", fs->total_inodes);
    #endif

    // Allocate memory for block bitmap
    fs->block_bitmap = (bool *)malloc(fs->total_blocks * sizeof(bool));
    if (!fs->block_bitmap) {
        printf("Memory allocation for block bitmap failed!\n");
        fclose(file);
        return;
    }

    #ifdef DEBUG
    printf("Memory allocated for block bitmap\n");
    #endif

    // Allocate memory for inode bitmap
    fs->inode_bitmap = (bool *)malloc(fs->total_inodes * sizeof(bool));
    if (!fs->inode_bitmap) {
        printf("Memory allocation for inode bitmap failed!\n");
        free(fs->block_bitmap);
        fclose(file);
        return;
    }

    #ifdef DEBUG
    printf("Memory allocated for inode bitmap\n");
    #endif

    // Allocate memory for blocks
    fs->blocks = (Block *)malloc(fs->total_blocks * sizeof(Block));
    if (!fs->blocks) {
        printf("Memory allocation for blocks failed!\n");
        free(fs->block_bitmap);
        fclose(file);
        return;
    }

    #ifdef DEBUG
    printf("Memory allocated for blocks\n");
    #endif

    // Allocate memory for inodes
    fs->inodes = (Inode *)malloc(fs->total_inodes * sizeof(Inode));
    if (!fs->inodes) {
        printf("Memory allocation for inodes failed!\n");
        free(fs->block_bitmap);
        free(fs->blocks);
        fclose(file);
        return;
    }

    #ifdef DEBUG
    printf("Memory allocated for inodes\n");
    #endif

    // Read the block bitmap
    fread(fs->block_bitmap, sizeof(bool), fs->total_blocks, file);

    #ifdef DEBUG
    printf("Block bitmap read\n");
    #endif

    // Read the inode bitmap
    fread(fs->inode_bitmap, sizeof(bool), fs->total_inodes, file);

    #ifdef DEBUG
    printf("inode bitmap read\n");
    #endif


    // Read inodes
    fread(fs->inodes, sizeof(Inode), fs->total_inodes, file);

    #ifdef DEBUG
    printf("inode read\n");
    #endif

    // Read blocks
    for (uint32_t i = 0; i < fs->total_blocks; i++) {
        fread(&fs->blocks[i], sizeof(Block), 1, file);
    }

    #ifdef DEBUG
    printf("blocks read\n");
    #endif

    fclose(file);
    printf("File system loaded from disk image '%s'. Total blocks: %u\n", image_filename, fs->total_blocks);
}

void get_inode_path(FileSystem *fs, int inode_index, char *path, int max_path_len) {
    path[0] = '\0';
    if (inode_index == 0) {
        strcpy(path, "/");
        return;
    }

    char temp_path[max_path_len];
    temp_path[0] = '\0';
    int current_inode_index = inode_index;

    while(current_inode_index != 0) {
       char dir_name[MAX_FILENAME];
       int parent_inode_index = -1;

         for (uint32_t parent_inode_idx = 0; parent_inode_idx < fs->total_inodes; parent_inode_idx++){
            if (fs->inodes[parent_inode_idx].is_directory) {
                 for (int j = 0; j < fs->inodes[parent_inode_idx].dir_entry_count; j++) {
                      if(fs->inodes[parent_inode_idx].entries[j].inode_index == current_inode_index){
                        strcpy(dir_name, fs->inodes[parent_inode_idx].entries[j].name);
                        parent_inode_index = parent_inode_idx;
                        break;
                      }
                 }
                 if(parent_inode_index != -1)
                    break;
             }
        }
        char tmp[max_path_len];
         snprintf(tmp, max_path_len, "/%s%s", dir_name, temp_path);
        strcpy(temp_path, tmp);
        current_inode_index = parent_inode_index;

        if(current_inode_index == -1){
            printf("Invalid Path\n");
            path[0] = '\0';
            return;
        }
    }

    strcpy(path, temp_path);
}

void status(FileSystem *fs){
  int used_blocks = 0;
  int used_inodes = 0;
  int used_files_blocks = 0;

   for(int i =0; i < fs->total_blocks; i++){
     if(fs->block_bitmap[i])
        used_blocks++;
   }

    for (int i = 0; i < fs->total_inodes; i++) {
      if(fs->inode_bitmap[i]){
          used_inodes++;
           if(!fs->inodes[i].is_directory){
                used_files_blocks+= (fs->inodes[i].size + BLOCK_SIZE - 1) / BLOCK_SIZE;
          }
      }
    }


    printf("partition size: %ld\n", (long)(fs->total_blocks * BLOCK_SIZE));
    printf("total inodes: %d\n", fs->total_inodes);
    printf("used inodes: %d\n", used_inodes);
     printf("total blocks: %d\n", fs->total_blocks);
    printf("used blocks: %d\n", used_blocks);
     printf("files' blocks: %d\n", used_files_blocks);
    printf("block size: %d\n", BLOCK_SIZE);
   printf("free space: %ld\n", (long)((fs->total_blocks - used_blocks) * BLOCK_SIZE));
}