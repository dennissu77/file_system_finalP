#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>

#define MAX_FILE_NAME_LENGTH 256
#define MAX_PATH_LENGTH 256
#define MAX_FILE_SIZE 1024000 // 1MB for simplicity

// File structure
typedef struct file_t {
    char name[MAX_FILE_NAME_LENGTH];
    char *content;
    long size;
    long blocks_used;
    long inode;
} file_t;

// Directory entry
typedef struct dir_entry_t {
    char name[MAX_FILE_NAME_LENGTH];
    bool is_dir;
    union {
        struct directory_t *dir_ptr;
        file_t *file_ptr;
    };
    struct dir_entry_t *next;
} dir_entry_t;

// Directory structure
typedef struct directory_t {
    dir_entry_t *entries;
    struct directory_t *parent;
    long inode;
} directory_t;

// Filesystem structure
typedef struct filesystem_t {
    directory_t *root;
    directory_t *current_dir;
    long total_size;
    long used_size;
    long total_inodes;
    long used_inodes;
    long total_blocks;
    long used_blocks;
    long block_size;
    char *memory;
    long next_inode;
} filesystem_t;

// Global variables
filesystem_t fs;

// Function declarations
void create_partition(long size);
void ls();
void cd(const char *path);
void rm(const char *name);
void mkdir(const char *name);
void rmdir(const char *name);
void put(const char *local_path, const char *file_name);
void get(const char *file_name, const char *local_path);
void cat(const char *file_name);
void status();
void help();
void exit_and_store_img();

// Helper function to find a directory entry
dir_entry_t *find_entry(directory_t *dir, const char *name) {
    if (!dir || !name) return NULL;
    dir_entry_t *current = dir->entries;
    while (current != NULL) {
        if (strcmp(current->name, name) == 0) {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

// Helper function to add a directory entry
void add_entry(directory_t *dir, const char *name, bool is_dir, void *ptr) {
    dir_entry_t *new_entry = malloc(sizeof(dir_entry_t));
    if (!new_entry) {
        perror("Failed to allocate memory for directory entry");
        return;
    }

    strcpy(new_entry->name, name);
    new_entry->is_dir = is_dir;
    if (is_dir) {
      new_entry->dir_ptr = (directory_t*)ptr;
    } else {
      new_entry->file_ptr = (file_t*)ptr;
    }
    new_entry->next = dir->entries;
    dir->entries = new_entry;
}

// Implementation of functions
void create_partition(long size) {
    fs.total_size = size;
    fs.used_size = 0;
    fs.memory = malloc(size);
    fs.block_size = 1024;
    fs.total_blocks = size / fs.block_size;
    fs.used_blocks = 0;
    fs.next_inode = 1; // Inode 0 reserved
    fs.total_inodes = 221;
    fs.used_inodes = 0;

    if (!fs.memory) {
        perror("Memory allocation failed");
        exit(1);
    }
    // Create the root directory
    fs.root = malloc(sizeof(directory_t));
    if(!fs.root) {
        perror("Failed to allocate memory for root directory");
        exit(1);
    }

    fs.root->entries = NULL;
    fs.root->parent = NULL;
    fs.root->inode = fs.next_inode++;
    fs.current_dir = fs.root;

    fs.used_inodes++;

    printf("Make new partition successful !\n");
}

void ls() {
    if(!fs.current_dir || !fs.current_dir->entries){
      printf("Empty\n");
      return;
    }

    dir_entry_t *current = fs.current_dir->entries;
    while (current != NULL) {
        printf("%s%s  ", current->name, current->is_dir ? "/" : "");
        current = current->next;
    }
    printf("\n");
}

void cd(const char *path) {
  if (!path || strlen(path) == 0) {
    fs.current_dir = fs.root;
    return;
  }

    if(strcmp(path, "..") == 0) {
       if(fs.current_dir->parent) {
           fs.current_dir = fs.current_dir->parent;
        }
        return;
    }

  dir_entry_t *entry = find_entry(fs.current_dir, path);
  if (!entry) {
        printf("Directory '%s' not found.\n", path);
    } else if (!entry->is_dir) {
         printf("'%s' is not a directory.\n", path);
    } else {
        fs.current_dir = entry->dir_ptr;
    }

}

void rm(const char *name) {
    if (!name) return;
    if (!fs.current_dir || !fs.current_dir->entries) {
        printf("File '%s' not found.\n", name);
        return;
    }

    dir_entry_t *prev = NULL;
    dir_entry_t *current = fs.current_dir->entries;

     while(current) {
        if(strcmp(current->name, name) == 0) {
             // Found
              if (prev == NULL) {
                fs.current_dir->entries = current->next; // First element
            } else {
                prev->next = current->next;
            }

            if(current->is_dir) {
                // Free all elements of the directory tree here
                 free(current->dir_ptr); // TODO: Free recursively.
                 fs.used_inodes--;
            } else{
                 fs.used_size -= current->file_ptr->size;
                 fs.used_blocks -= current->file_ptr->blocks_used;
                free(current->file_ptr->content);
                free(current->file_ptr);
                fs.used_inodes--;
            }

           free(current);
           printf("File '%s' removed.\n", name);
            return;
        }
        prev = current;
        current = current->next;
    }

    printf("File '%s' not found.\n", name);
}

void mkdir(const char *name) {
    if (!name) return;

    if (find_entry(fs.current_dir, name)) {
      printf("Directory '%s' already exists.\n", name);
        return;
    }

    directory_t *new_dir = malloc(sizeof(directory_t));
    if(!new_dir){
        perror("Memory allocation failed for new directory");
        return;
    }

    new_dir->entries = NULL;
    new_dir->parent = fs.current_dir;
    new_dir->inode = fs.next_inode++;

    add_entry(fs.current_dir, name, true, new_dir);
    fs.used_inodes++;

    printf("Directory '%s' created.\n", name);
}

void rmdir(const char *name) {
    rm(name);
}

void put(const char *local_path, const char *file_name) {
    if (!local_path || !file_name) return;

    FILE *local_file = fopen(local_path, "r");
    if (!local_file) {
        perror("Error opening local file");
        return;
    }

    fseek(local_file, 0, SEEK_END);
    long file_size = ftell(local_file);
    fseek(local_file, 0, SEEK_SET);

    if (file_size > fs.total_size - fs.used_size) {
        fclose(local_file);
        printf("Not enough space on the file system.\n");
        return;
    }

    char *content = malloc(file_size);
    if (!content) {
        perror("Failed to allocate file content");
        fclose(local_file);
        return;
    }

    fread(content, 1, file_size, local_file);
    fclose(local_file);

     file_t *new_file = malloc(sizeof(file_t));
    if (!new_file) {
        perror("Failed to allocate memory for file struct");
        free(content);
        return;
    }

    strcpy(new_file->name, file_name);
    new_file->content = content;
    new_file->size = file_size;
    new_file->inode = fs.next_inode++;
     new_file->blocks_used = (long)ceil((double)file_size / fs.block_size);

    add_entry(fs.current_dir, file_name, false, new_file);
     fs.used_size += file_size;
    fs.used_blocks += new_file->blocks_used;
    fs.used_inodes++;

    printf("File '%s' put successfully.\n", file_name);
}

void get(const char *file_name, const char *local_path) {
    if(!file_name || !local_path) return;

    dir_entry_t *entry = find_entry(fs.current_dir, file_name);

    if (!entry || entry->is_dir) {
        printf("File '%s' not found.\n", file_name);
        return;
    }

    file_t *file = entry->file_ptr;

    FILE *local_file = fopen(local_path, "w");
    if (!local_file) {
        perror("Error opening local file for writing");
        return;
    }

    fwrite(file->content, 1, file->size, local_file);
    fclose(local_file);

    printf("File '%s' got successfully.\n", file_name);
}

void cat(const char *file_name) {
    if(!file_name) return;
    dir_entry_t *entry = find_entry(fs.current_dir, file_name);

    if (!entry || entry->is_dir) {
        printf("File '%s' not found.\n", file_name);
        return;
    }

    file_t *file = entry->file_ptr;
    printf("%.*s\n", (int)file->size, file->content);
}

void status() {
    printf("partition size: %ld\n", fs.total_size);
    printf("total inodes: %ld\n", fs.total_inodes);
    printf("used inodes: %ld\n", fs.used_inodes);
    printf("total blocks: %ld\n", fs.total_blocks);
    printf("used blocks: %ld\n", fs.used_blocks);
     long used_files_blocks = 0;
     if(fs.root){
          dir_entry_t *current = fs.root->entries;
         while(current) {
              if(!current->is_dir){
                  used_files_blocks+= current->file_ptr->blocks_used;
              }
              current = current->next;
          }
     }
    printf("files' blocks: %ld\n", used_files_blocks);
    printf("block size: %ld\n", fs.block_size);
    printf("free space: %ld\n", fs.total_size - fs.used_size);
}

void help() {
    printf("List of commands:\n");
    printf("'ls' list directory\n");
    printf("'cd' change directory\n");
    printf("'rm' remove file\n");
    printf("'mkdir' make directory\n");
    printf("'rmdir' remove directory\n");
    printf("'put' put file into the space\n");
    printf("'get' get file from the space\n");
    printf("'cat' show content\n");
    printf("'status' show status of the space\n");
    printf("'help' show help\n");
    printf("'exit' exit and store img (not fully implemented)\n");
}

void exit_and_store_img() {
    // If we were to persist this data, this function would serialize the
    // whole fs.memory buffer into a file. It would not be a direct image copy,
    // since we would need to account for the file entries metadata.

    printf("Exiting program. Note: Data is not persisted in this demo implementation\n");
}

// Helper function to get the current path
void get_current_path(char *path) {
    path[0] = '\0';
    directory_t *current = fs.current_dir;

    if (current == fs.root) {
      strcpy(path, "/");
      return;
    }

    char temp_path[MAX_PATH_LENGTH];
    temp_path[0] = '\0';
    while(current && current != fs.root) {
      char dir_name[MAX_FILE_NAME_LENGTH];
       dir_entry_t *current_entry = fs.root->entries;
       while (current_entry != NULL) {
             if (current_entry->is_dir && current_entry->dir_ptr == current){
                 strcpy(dir_name, current_entry->name);
                  break;
             }
            current_entry = current_entry->next;
        }
        char tmp[MAX_PATH_LENGTH];
        snprintf(tmp, sizeof(tmp), "/%s%s", dir_name, temp_path);
        strcpy(temp_path, tmp);

        current = current->parent;
    }

    strcpy(path, temp_path);
}

int main() {
    long partition_size;

    printf("options:\n");
    printf("1. loads from file\n");
    printf("2. create new partition in memory\n");

    int choice;
    scanf("%d", &choice);

    if (choice == 2) {
        printf("Input size of a new partition (example 102400)\n");
        scanf("%ld", &partition_size);
        create_partition(partition_size);
        printf("partition size = %ld\n", partition_size);
    }
     else {
         printf("Loading from file is not implemented in demo, creating default partition\n");
         create_partition(2048000);
    }

    help();

    char command[MAX_FILE_NAME_LENGTH];
    char arg1[MAX_PATH_LENGTH];
    char arg2[MAX_PATH_LENGTH];

    while (true) {
        char current_path[MAX_PATH_LENGTH];
        get_current_path(current_path);
        printf("%s$ ", current_path);
        scanf("%s", command);

        if (strcmp(command, "ls") == 0) {
             ls();
        } else if (strcmp(command, "cd") == 0) {
             scanf("%s", arg1);
            cd(arg1);
         }else if (strcmp(command, "rm") == 0) {
            scanf("%s", arg1);
            rm(arg1);
        } else if (strcmp(command, "mkdir") == 0) {
             scanf("%s", arg1);
            mkdir(arg1);
        } else if (strcmp(command, "rmdir") == 0) {
            scanf("%s", arg1);
            rmdir(arg1);
        } else if (strcmp(command, "put") == 0) {
            // scanf("%s %s", arg1, arg2); // Old way
              char input[MAX_PATH_LENGTH * 2];
              if(fgets(input, sizeof(input), stdin) != NULL){
                 char *token = strtok(input, " \n"); // Tokenize the input
                    if(token != NULL){
                        strcpy(arg1, token); // first token is local path
                        token = strtok(NULL, " \n"); // Get the second argument, if exists.
                         if(token!=NULL){
                              strcpy(arg2, token); // file name is the second token, if exists.
                         } else {
                            printf("Missing file name.\n");
                           continue; // Missing file name.
                         }

                        put(arg1, arg2); // Call with args

                   }else{
                         printf("Invalid command.\n");
                    }
               }

        } else if (strcmp(command, "get") == 0) {
            scanf("%s %s", arg1, arg2);
            get(arg1, arg2);
        } else if (strcmp(command, "cat") == 0) {
            scanf("%s", arg1);
            cat(arg1);
        } else if (strcmp(command, "status") == 0) {
            status();
        } else if (strcmp(command, "help") == 0) {
            help();
        } else if (strcmp(command, "exit") == 0) {
            exit_and_store_img();
            break;
        } else {
            printf("Invalid command\n");
        }
    }
    return 0;
}