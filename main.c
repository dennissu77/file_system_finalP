#include "FileSystem.h"
#define MAX_INPUT_LENGTH 512
#define MAX_PATH_LENGTH 512
#define CAT_BUFFER_SIZE (BLOCK_SIZE * 4)

int main() {
    FileSystem fs;
    uint32_t num_blocks;

    // Ask user for the number of blocks
#ifndef LOAD_IMG
    printf("Enter the number of blocks for the file system: ");
    if (scanf("%u", &num_blocks) != 1 || num_blocks == 0) {
        printf("Invalid input! Number of blocks must be a positive integer.\n");
        return 1;
    }

    // Initialize file system
    initialize_file_system(&fs, num_blocks);

    // Create a root directory
    int current_dir_inode = create_directory(&fs, "root");
#endif

#ifdef LOAD_IMG
    load_file_system(&fs, "disk_image.bin");
    int current_dir_inode = 0;
#endif

    char command[MAX_FILENAME];
    char arg1[MAX_PATH_LENGTH];
    char arg2[MAX_PATH_LENGTH];

    while (true) {
        char current_path[MAX_PATH_LENGTH];
        get_inode_path(&fs, current_dir_inode, current_path, MAX_PATH_LENGTH);
        printf("%s$", current_path);

        char input[MAX_INPUT_LENGTH];
        if (fgets(input, sizeof(input), stdin) != NULL) {
            char *token = strtok(input, " \n");
            if (token != NULL) {
                strcpy(command, token);
                bool invalid_command = false;

                switch (command[0]) { // switch on the first char of the command for improved speed.
                case 'l':
                    if (strcmp(command, "ls") == 0) {
                            list_directory(&fs, current_dir_inode);
                        } else {
                           invalid_command = true;
                         }
                        break;
                   case 'c':
                        if (strcmp(command, "cd") == 0) {
                            token = strtok(NULL, " \n");
                            if (token != NULL) {
                                strcpy(arg1, token);
                                if (strcmp(arg1, "..") == 0) {
                                        if(current_dir_inode != 0){
                                        int parent_inode_index = -1;
                                         for (uint32_t parent_inode_idx = 0; parent_inode_idx < fs.total_inodes; parent_inode_idx++) {
                                            if (fs.inodes[parent_inode_idx].is_directory) {
                                                for (int j = 0; j < fs.inodes[parent_inode_idx].dir_entry_count; j++) {
                                                    if (fs.inodes[parent_inode_idx].entries[j].inode_index == current_dir_inode) {
                                                        parent_inode_index = parent_inode_idx;
                                                        break;
                                                    }
                                                }
                                                if (parent_inode_index != -1)
                                                    break;
                                            }
                                        }
                                            if(parent_inode_index != -1)
                                                current_dir_inode = parent_inode_index;
                                            }
                                } else {
                                    int found_inode = -1;
                                    Inode *dir_inode = &fs.inodes[current_dir_inode];
                                    if (dir_inode->is_directory) {
                                        for (uint32_t i = 0; i < dir_inode->dir_entry_count; i++) {
                                            DirectoryEntry *entry = &dir_inode->entries[i];
                                            if (strcmp(entry->name, arg1) == 0) {
                                                if (fs.inodes[entry->inode_index].is_directory) {
                                                    found_inode = entry->inode_index;
                                                    break;
                                                } else {
                                                    printf("'%s' is not a directory.\n", arg1);
                                                }
                                            }
                                        }
                                    }

                                    if(found_inode != -1)
                                        current_dir_inode = found_inode;
                                    else {
                                       printf("Directory '%s' not found on cd\n", arg1);
                                    }
                                }

                            } else {
                                printf("Missing argument\n");
                            }
                        }else if (strcmp(command, "cat") == 0) {
                             token = strtok(NULL, " \n");
                            if(token!=NULL){
                                 strcpy(arg1, token);
                                  int found_file = -1;
                                   Inode *dir_inode = &fs.inodes[current_dir_inode];
                                    if (dir_inode->is_directory) {
                                        for (uint32_t i = 0; i < dir_inode->dir_entry_count; i++) {
                                             DirectoryEntry *entry = &dir_inode->entries[i];
                                                if(strcmp(entry->name, arg1) == 0){
                                                     found_file = entry->inode_index;
                                                      break;
                                                  }
                                              }
                                    }
                                  if(found_file != -1) {
                                      uint8_t buffer[CAT_BUFFER_SIZE] = {0};
                                       read_from_file(&fs, found_file, buffer, fs.inodes[found_file].size);
                                      printf("%s\n", buffer);
                                   } else{
                                          printf("File '%s' not found on cat.\n", arg1);
                                        }
                               } else{
                                  printf("Missing file name.\n");
                                }
                        }
                        else{
                            invalid_command = true;
                         }
                            break;
                    case 'm':
                            if (strcmp(command, "mkdir") == 0) {
                            token = strtok(NULL, " \n");
                            if (token != NULL) {
                                strcpy(arg1, token);
                                int new_dir_inode = create_directory(&fs, arg1);
                                if (new_dir_inode != -1) {
                                    add_to_directory(&fs, current_dir_inode, new_dir_inode, arg1);
                                    printf("Directory '%s' created.\n", arg1);
                                }
                            } else {
                                printf("Missing directory name\n");
                            }
                         }else{
                             invalid_command = true;
                         }
                        break;
                   case 'r':
                        for (int i = 0;i<10;i++)
                        {
                            printf("%d  ",fs.inode_bitmap[i]);
                        }

                        if (strcmp(command, "rm") == 0) {
                               token = strtok(NULL, " \n");
                                if (token != NULL) {
                                    strcpy(arg1, token);
                                    int found_file = -1;
                                    Inode *dir_inode = &fs.inodes[current_dir_inode];
                                     if (dir_inode->is_directory) {
                                        for (uint32_t i = 0; i < dir_inode->dir_entry_count; i++) {
                                            DirectoryEntry *entry = &dir_inode->entries[i];
                                             if (strcmp(entry->name, arg1) == 0) {
                                                 found_file = entry->inode_index;
                                                    if (fs.inodes[entry->inode_index].is_directory) {
                                                     printf("'%s' is a directory, use rmdir instead.\n", arg1);
                                                      }
                                                    break;
                                                }
                                           }
                                        }

                                   if (found_file != -1) {
                                         int parent_inode_index = -1;
                                             for (uint32_t parent_inode_idx = 0; parent_inode_idx < fs.total_inodes; parent_inode_idx++) {
                                                 if (fs.inodes[parent_inode_idx].is_directory) {
                                                     for (int j = 0; j < fs.inodes[parent_inode_idx].dir_entry_count; j++) {
                                                         if (fs.inodes[parent_inode_idx].entries[j].inode_index == current_dir_inode) {
                                                             parent_inode_index = parent_inode_idx;
                                                               break;
                                                          }
                                                    }
                                                      if (parent_inode_index != -1)
                                                         break;
                                                    }
                                             }

                                       Inode *parent_dir_inode = &fs.inodes[parent_inode_index];
                                        int entry_index_to_remove = -1;
                                        for (int i = 0; i < parent_dir_inode->dir_entry_count; i++) {
                                            if (parent_dir_inode->entries[i].inode_index == found_file) {
                                                entry_index_to_remove = i;
                                                break;
                                             }
                                        }
                                         if (entry_index_to_remove != -1) {
                                            for (int k = entry_index_to_remove; k < parent_dir_inode->dir_entry_count - 1; k++) {
                                               parent_dir_inode->entries[k] = parent_dir_inode->entries[k + 1];
                                           }
                                            parent_dir_inode->dir_entry_count--;
                                        }
                                        remove_from_directory(&fs, current_dir_inode, found_file, arg1);
                                        free_inode(&fs, found_file);
                                        printf("File '%s' removed.\n", arg1);
                                    }else {
                                          printf("File '%s' not found on rm.\n", arg1);
                                    }

                                } else {
                                    printf("Missing file name\n");
                                }
                        } else if (strcmp(command, "rmdir") == 0) {
                                    token = strtok(NULL, " \n");
                                if (token != NULL) {
                                    strcpy(arg1, token);
                                    int found_dir = -1;
                                   Inode *dir_inode = &fs.inodes[current_dir_inode];
                                        if (dir_inode->is_directory) {
                                          for (uint32_t i = 0; i < dir_inode->dir_entry_count; i++) {
                                              DirectoryEntry *entry = &dir_inode->entries[i];
                                               if (strcmp(entry->name, arg1) == 0) {
                                                    if (fs.inodes[entry->inode_index].is_directory) {
                                                         found_dir = entry->inode_index;
                                                       break;
                                                      } else {
                                                        printf("'%s' is not a directory.\n", arg1);
                                                    }
                                                  }
                                             }
                                        }

                                    if (found_dir != -1) {
                                          int parent_inode_index = -1;
                                             for (uint32_t parent_inode_idx = 0; parent_inode_idx < fs.total_inodes; parent_inode_idx++) {
                                                 if (fs.inodes[parent_inode_idx].is_directory) {
                                                     for (int j = 0; j < fs.inodes[parent_inode_idx].dir_entry_count; j++) {
                                                        if (fs.inodes[parent_inode_idx].entries[j].inode_index == current_dir_inode) {
                                                             parent_inode_index = parent_inode_idx;
                                                               break;
                                                          }
                                                     }
                                                      if (parent_inode_index != -1)
                                                         break;
                                                     }
                                             }
                                         Inode *parent_dir_inode = &fs.inodes[parent_inode_index];
                                        int entry_index_to_remove = -1;
                                        for (int i = 0; i < parent_dir_inode->dir_entry_count; i++) {
                                              if (parent_dir_inode->entries[i].inode_index == found_dir) {
                                                   entry_index_to_remove = i;
                                                      break;
                                                 }
                                        }
                                        if (entry_index_to_remove != -1) {
                                            for (int k = entry_index_to_remove; k < parent_dir_inode->dir_entry_count - 1; k++) {
                                                 parent_dir_inode->entries[k] = parent_dir_inode->entries[k + 1];
                                           }
                                            parent_dir_inode->dir_entry_count--;
                                        }
                                        free_inode(&fs, found_dir);
                                        
                                        printf("Directory '%s' removed.\n", arg1);
                                    } else {
                                           printf("Directory '%s' not found on rmdir\n", arg1);
                                        }
                                } else {
                                    printf("Missing directory name\n");
                                }
                            }else{
                                invalid_command = true;
                             }
                           break;
                   case 'p':
                      if (strcmp(command, "put") == 0) {
                            token = strtok(NULL, " \n");
                            if (token != NULL) {
                                strcpy(arg1, token);
                                token = strtok(NULL, " \n");
                                if (token != NULL) {
                                    strcpy(arg2, token);
                                    int file_inode = read_file_to_fs(&fs, arg1, arg2);
                                    if (file_inode != -1) {
                                        add_to_directory(&fs, current_dir_inode, file_inode, arg2);
                                        printf("File '%s' put successfully.\n", arg2);
                                    }
                                } else {
                                    printf("Missing file name.\n");
                                }
                            } else {
                                printf("Missing file name.\n");
                            }
                         }else{
                             invalid_command = true;
                        }
                        break;
                    case 'g':
                         if (strcmp(command, "get") == 0) {
                            token = strtok(NULL, " \n");
                            if (token != NULL) {
                                strcpy(arg1, token);
                                token = strtok(NULL, " \n");
                                if (token != NULL) {
                                    strcpy(arg2, token);
                                    int found_file = -1;
                                   Inode *dir_inode = &fs.inodes[current_dir_inode];
                                    if (dir_inode->is_directory) {
                                      for (uint32_t i = 0; i < dir_inode->dir_entry_count; i++) {
                                            DirectoryEntry *entry = &dir_inode->entries[i];
                                             if(strcmp(entry->name, arg1) == 0){
                                                 found_file = entry->inode_index;
                                                 break;
                                             }
                                        }
                                     }

                                    if (found_file != -1) {
                                          write_file_to_host(&fs,found_file, arg2);
                                          printf("File '%s' got successfully.\n", arg1);
                                       } else {
                                             printf("File '%s' not found on get.\n", arg1);
                                        }
                                } else {
                                    printf("Missing destination path.\n");
                                }
                            } else {
                                printf("Missing file name.\n");
                            }
                         }else{
                             invalid_command = true;
                         }
                         break;
                    case 's':
                         if (strcmp(command, "status") == 0) {
                            status(&fs);
                        } else {
                             invalid_command = true;
                        }
                         break;
                    case 'h':
                         if (strcmp(command, "help") == 0) {
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
                            }else {
                                  invalid_command = true;
                           }
                         break;
                     case 'e':
                            if (strcmp(command, "exit") == 0) {
                                 #ifndef LOAD_IMG
                                 save_file_system(&fs, "disk_image.bin");
                                 #endif
                                 goto exit_loop;
                                }else{
                                invalid_command = true;
                                }
                                break;
                    default:
                        invalid_command = true;
                         break;
                    }
                    if(invalid_command)
                       printf("Invalid command\n");
             } else {
                  printf("Invalid command\n");
               }
        }
    }
        exit_loop:
       cleanup_file_system(&fs);
       return 0;
}