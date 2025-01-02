#include "FileSystem.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define MAX_INPUT_LENGTH 512
#define MAX_PATH_LENGTH 512
#define CAT_BUFFER_SIZE (BLOCK_SIZE * 4)

typedef struct {
    FileSystem* fs;
    int current_dir_inode;
} FileSystemContext;

typedef void (*CommandHandler)(FileSystemContext* ctx, char* arg1, char* arg2);

// Command handlers
static void handle_ls(FileSystemContext* ctx, char* arg1, char* arg2) {
    Inode* dir_inode = &ctx->fs->inodes[ctx->current_dir_inode];
    printf("Contents of directory:\n");
    
    for (uint32_t i = 0; i < dir_inode->dir_entry_count; i++) {
        DirectoryEntry* entry = &dir_inode->entries[i];
        printf("  %s (inode %d, %s)\n", 
            entry->name,
            entry->inode_index,
            ctx->fs->inodes[entry->inode_index].is_directory ? "directory" : "file");
    }
}

static void handle_cd(FileSystemContext* ctx, char* arg1, char* arg2) {
    if (!arg1) {
        printf("Missing argument\n");
        return;
    }

    if (strcmp(arg1, "..") == 0) {
        if (ctx->current_dir_inode != 0) {
            int parent_inode_index = -1;
            for (uint32_t parent_inode_idx = 0; parent_inode_idx < ctx->fs->total_inodes; parent_inode_idx++) {
                if (ctx->fs->inodes[parent_inode_idx].is_directory) {
                    for (int j = 0; j < ctx->fs->inodes[parent_inode_idx].dir_entry_count; j++) {
                        if (ctx->fs->inodes[parent_inode_idx].entries[j].inode_index == ctx->current_dir_inode) {
                            parent_inode_index = parent_inode_idx;
                            break;
                        }
                    }
                    if (parent_inode_index != -1)
                        break;
                }
            }
            if (parent_inode_index != -1)
                ctx->current_dir_inode = parent_inode_index;
        }
    } else {
        int found_inode = -1;
        Inode* dir_inode = &ctx->fs->inodes[ctx->current_dir_inode];
        if (dir_inode->is_directory) {
            for (uint32_t i = 0; i < dir_inode->dir_entry_count; i++) {
                DirectoryEntry* entry = &dir_inode->entries[i];
                if (strcmp(entry->name, arg1) == 0) {
                    if (ctx->fs->inodes[entry->inode_index].is_directory) {
                        found_inode = entry->inode_index;
                        break;
                    } else {
                        printf("'%s' is not a directory.\n", arg1);
                    }
                }
            }
        }

        if (found_inode != -1)
            ctx->current_dir_inode = found_inode;
        else {
            printf("Directory '%s' not found on cd\n", arg1);
        }
    }
}

static void handle_cat(FileSystemContext* ctx, char* arg1, char* arg2) {
    if (!arg1) {
        printf("Missing file name.\n");
        return;
    }

    int found_file = -1;
    Inode* dir_inode = &ctx->fs->inodes[ctx->current_dir_inode];
    if (dir_inode->is_directory) {
        for (uint32_t i = 0; i < dir_inode->dir_entry_count; i++) {
            DirectoryEntry* entry = &dir_inode->entries[i];
            if (strcmp(entry->name, arg1) == 0) {
                found_file = entry->inode_index;
                break;
            }
        }
    }

    if (found_file != -1) {
        uint8_t buffer[CAT_BUFFER_SIZE] = {0};
        read_from_file(ctx->fs, found_file, buffer, ctx->fs->inodes[found_file].size);
        printf("%s\n", buffer);
    } else {
        printf("File '%s' not found on cat.\n", arg1);
    }
}

static void handle_mkdir(FileSystemContext* ctx, char* arg1, char* arg2) {
    if (!arg1) {
        printf("Missing directory name\n");
        return;
    }

    int new_dir_inode = create_directory(ctx->fs, arg1);
    if (new_dir_inode != -1) {
        add_to_directory(ctx->fs, ctx->current_dir_inode, new_dir_inode, arg1);
        printf("Directory '%s' created.\n", arg1);
    }
}

static void handle_rm(FileSystemContext* ctx, char* arg1, char* arg2) {
    if (!arg1) {
        printf("Missing file name\n");
        return;
    }

    int found_file = -1;
    Inode* dir_inode = &ctx->fs->inodes[ctx->current_dir_inode];
    if (dir_inode->is_directory) {
        for (uint32_t i = 0; i < dir_inode->dir_entry_count; i++) {
            DirectoryEntry* entry = &dir_inode->entries[i];
            if (strcmp(entry->name, arg1) == 0) {
                found_file = entry->inode_index;
                if (ctx->fs->inodes[entry->inode_index].is_directory) {
                    printf("'%s' is a directory, use rmdir instead.\n", arg1);
                }
                break;
            }
        }
    }

    if (found_file != -1) {
        remove_from_directory(ctx->fs, ctx->current_dir_inode, found_file, arg1);
        free_inode(ctx->fs, found_file);
        printf("File '%s' removed.\n", arg1);
    } else {
        printf("File '%s' not found on rm.\n", arg1);
    }
}

static void handle_rmdir(FileSystemContext* ctx, char* arg1, char* arg2) {
    if (!arg1) {
        printf("Missing directory name\n");
        return;
    }

    int found_dir = -1;
    Inode* dir_inode = &ctx->fs->inodes[ctx->current_dir_inode];
    if (dir_inode->is_directory) {
        for (uint32_t i = 0; i < dir_inode->dir_entry_count; i++) {
            DirectoryEntry* entry = &dir_inode->entries[i];
            if (strcmp(entry->name, arg1) == 0) {
                if (ctx->fs->inodes[entry->inode_index].is_directory) {
                    found_dir = entry->inode_index;
                    break;
                } else {
                    printf("'%s' is not a directory.\n", arg1);
                }
            }
        }
    }

    if (found_dir != -1) {
        remove_from_directory(ctx->fs, ctx->current_dir_inode, found_dir, arg1);
        free_inode(ctx->fs, found_dir);
        printf("Directory '%s' removed.\n", arg1);
    } else {
        printf("Directory '%s' not found on rmdir\n", arg1);
    }
}

static void handle_put(FileSystemContext* ctx, char* arg1, char* arg2) {
    if (!arg1) {
        printf("Missing file name.\n");
        return;
    }

    // If arg2 is not provided, use arg1 as the destination name
    const char* dest_name = arg2 && *arg2 ? arg2 : arg1;

    int file_inode = read_file_to_fs(ctx->fs, arg1, dest_name);
    if (file_inode != -1) {
        add_to_directory(ctx->fs, ctx->current_dir_inode, file_inode, dest_name);
        printf("File '%s' put successfully.\n", dest_name);
    }
}

static void handle_get(FileSystemContext* ctx, char* arg1, char* arg2) {
    if (!arg1) {
        printf("Missing file name.\n");
        return;
    }

    // If arg2 is not provided, use arg1 as the destination name
    const char* dest_name = arg2 && *arg2 ? arg2 : arg1;

    int found_file = -1;
    Inode* dir_inode = &ctx->fs->inodes[ctx->current_dir_inode];
    if (dir_inode->is_directory) {
        for (uint32_t i = 0; i < dir_inode->dir_entry_count; i++) {
            DirectoryEntry* entry = &dir_inode->entries[i];
            if (strcmp(entry->name, arg1) == 0) {
                found_file = entry->inode_index;
                break;
            }
        }
    }

    if (found_file != -1) {
        if (write_file_to_host(ctx->fs, found_file, dest_name)) {
            printf("File '%s' got successfully.\n", arg1);
        }
    } else {
        printf("File '%s' not found on get.\n", arg1);
    }
}

static void handle_status(FileSystemContext* ctx, char* arg1, char* arg2) {
    status(ctx->fs);
}

static void handle_help(FileSystemContext* ctx, char* arg1, char* arg2) {
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
    printf("'exit' exit and store img\n");
}

typedef struct {
    const char* command;
    CommandHandler handler;
} CommandEntry;

static const CommandEntry COMMAND_TABLE[] = {
    {"ls", handle_ls},
    {"cd", handle_cd},
    {"cat", handle_cat},
    {"mkdir", handle_mkdir},
    {"rm", handle_rm},
    {"rmdir", handle_rmdir},
    {"put", handle_put},
    {"get", handle_get},
    {"status", handle_status},
    {"help", handle_help},
    {NULL, NULL}
};

static void process_command(FileSystemContext* ctx, const char* input) {
    char command[MAX_FILENAME];
    char arg1[MAX_PATH_LENGTH] = "";
    char arg2[MAX_PATH_LENGTH] = "";
    
    char input_copy[MAX_INPUT_LENGTH];
    strncpy(input_copy, input, MAX_INPUT_LENGTH - 1);
    
    char* token = strtok(input_copy, " \n");
    if (!token) return;
    
    strncpy(command, token, MAX_FILENAME - 1);
    
    token = strtok(NULL, " \n");
    if (token) strncpy(arg1, token, MAX_PATH_LENGTH - 1);
    
    token = strtok(NULL, " \n");
    if (token) strncpy(arg2, token, MAX_PATH_LENGTH - 1);
    
    for (const CommandEntry* entry = COMMAND_TABLE; entry->command != NULL; entry++) {
        if (strcmp(command, entry->command) == 0) {
            entry->handler(ctx, arg1, arg2);
            return;
        }
    }
    
    printf("Invalid command\n");
}

int main() {
    FileSystem fs;
    uint32_t num_blocks;
    FileSystemContext ctx;

#ifndef LOAD_IMG
    printf("Enter the number of blocks for the file system: ");
    if (scanf("%u", &num_blocks) != 1 || num_blocks == 0) {
        printf("Invalid input! Number of blocks must be a positive integer.\n");
        return 1;
    }

    // Clear input buffer
    int c;
    while ((c = getchar()) != '\n' && c != EOF);

    initialize_file_system(&fs, num_blocks);
    int root_inode = create_directory(&fs, "root");
    ctx.fs = &fs;
    ctx.current_dir_inode = root_inode;
#else
    load_file_system(&fs, "disk_image.bin");
    ctx.fs = &fs;
    ctx.current_dir_inode = 0;
#endif

    char input[MAX_INPUT_LENGTH];
    while (true) {
        // Print prompt
        char current_path[MAX_PATH_LENGTH];
        get_inode_path(&fs, ctx.current_dir_inode, current_path, MAX_PATH_LENGTH);
        printf("%s$ ", current_path);
        
        if (fgets(input, sizeof(input), stdin) == NULL) {
            break;
        }

        if (strncmp(input, "exit", 4) == 0) {
#ifndef LOAD_IMG
            save_file_system(&fs, "disk_image.bin");
#endif
            break;
        }

        process_command(&ctx, input);
    }

    cleanup_file_system(&fs);
    return 0;
}