#include "FileSystem.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include <sys/stat.h>   // mkdir 所需
#include <sys/types.h>  // mkdir 所需

#define MAX_INPUT_LENGTH 512
#define MAX_PATH_LENGTH 512
#define CAT_BUFFER_SIZE (BLOCK_SIZE * 4)


//#define LOAD_IMG


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

static void handle_get(FileSystemContext* ctx, char* arg1, char* arg2)
{
    if (!arg1) {
        printf("Missing file name.\n");
        return;
    }

    // 如果沒有指定 arg2，就用 arg1 做為外部檔名
    const char* dest_name = arg2 && *arg2 ? arg2 : arg1;

    //==== (1) 檢查並建立 "dump" 目錄(資料夾) ====
    // 如果 mkdir("dump", 0777) 回傳 -1，代表失敗；
    // 若 errno == EEXIST，表示資料夾已經存在，可以忽略。
    // 在真正使用前可再判斷 errno 做錯誤處理。
    mkdir("dump", 0777);

    //==== (2) 構建新的目的檔路徑: "dump/原始檔名" ====
    char path_in_dump[1024];
    snprintf(path_in_dump, sizeof(path_in_dump), "dump/%s", dest_name);

    //==== (3) 在模擬檔案系統找出 arg1 這個檔名對應的 inode ====
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

    //==== (4) 如果找到檔案，就呼叫 write_file_to_host ====
    if (found_file != -1) {
        // 把「dump/檔名」傳給 write_file_to_host
        if (write_file_to_host(ctx->fs, found_file, path_in_dump) > 0) {
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
    FileSystemContext ctx;
    uint32_t num_blocks;

    // 讓使用者選擇要「載入檔案系統」或是「建立新檔案系統」
    printf("選擇要進行的動作:\n");
    printf("1. Loads from file\n");
    printf("2. Create new partition in memory\n");
    printf("請輸入選項 (1 or 2): ");

    int choice;
    if (scanf("%d", &choice) != 1) {
        fprintf(stderr, "輸入無效，請重新執行程式。\n");
        return 1;
    }

    // 清空輸入緩衝區
    int c;
    while ((c = getchar()) != '\n' && c != EOF);

    if (choice == 1) {
        // 使用者選擇載入現有檔案系統
        load_file_system(&fs, "disk_image.bin");
        // 假設載入的檔案系統根目錄 inode 為 0
        ctx.fs = &fs;
        ctx.current_dir_inode = 0;
    } 
    else if (choice == 2) {
        // 使用者選擇在記憶體中建立新檔案系統
        printf("Enter the number of blocks for the file system: ");
        if (scanf("%u", &num_blocks) != 1 || num_blocks == 0) {
            printf("Invalid input! Number of blocks must be a positive integer.\n");
            return 1;
        }

        // 清空輸入緩衝區
        while ((c = getchar()) != '\n' && c != EOF);

        // 初始化檔案系統，並建立一個 root 目錄
        initialize_file_system(&fs, num_blocks);
        int root_inode = create_directory(&fs, "root");
        ctx.fs = &fs;
        ctx.current_dir_inode = root_inode;
    } 
    else {
        printf("無效的選項，請重新執行程式。\n");
        return 1;
    }

    char input[MAX_INPUT_LENGTH];
    while (true) {
        // 印出目前所在的路徑
        char current_path[MAX_PATH_LENGTH];
        get_inode_path(&fs, ctx.current_dir_inode, current_path, sizeof(current_path));
        printf("%s$ ", current_path);

        if (fgets(input, sizeof(input), stdin) == NULL) {
            break;  // EOF or error
        }

        // 如果輸入 "exit" 就離開
        if (strncmp(input, "exit", 4) == 0) {
            // 如果是「記憶體中新建」的檔案系統，離開前可視需求決定是否要儲存
            // 這裡假設都要存成 "disk_image.bin"
            if (choice == 2) {
                save_file_system(&fs, "disk_image.bin");
            }
            break;
        }

        // 處理指令
        process_command(&ctx, input);
    }

    // 清理系統資源
    cleanup_file_system(&fs);

    return 0;
}