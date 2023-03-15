#ifndef ZOS_CMD_FUNCTIONS_FS_H
#define ZOS_CMD_FUNCTIONS_FS_H

#define CONTENT_FS_FILE_FOUND "File with user given name was found - reading data, please wait...\n"

#define FORMAT_SIZE_NOT_ENTERED "Size of disc not entered - use \"help\" command for manual."
#define FORMAT_WRONG_SIZE "Wrong size of disk structure which defines superblock entered - use \"help\" command for manual."
#define FORMAT_WRONG_USAGE "Wrong usage of format command - use \"help\" command for manual."
#define FORMAT_REQ_SIZE "Requested disc size is %iMB\n"
#define FORMAT_FILE_EXISTS "File with name \"%s\" already exists, do you really want to format and erase all data in the file?\n"
#define FORMAT_FILE_EXISTS_CHOICE "Your choice (Y/y = yes OR N/n = no): "
#define FORMAT_FILE_EXISTS_INVALID "Invalid input, please enter Y/y for yes or N/n for no...\n"
#define FORMAT_FORMAT_START "Disc will be formatted, please wait...\n"
#define FORMAT_REM_SPACE "Disc was formatted, WASTED space: %dB.\n"
#define FORMAT_FORMAT_CANCEL "Disc will NOT be formatted.\n"
#define FORMAT_MB_TO_B 1000000
#define FORMAT_NUM_BASE 10
#define FORMAT_CLUSTER_SIZE_B 1000
#define FORMAT_CLUSTER_DISC_PERC 0.95
#define FORMAT_CLUSTER_INODE_PERC 0.04
#define FORMAT_SB_SIGNATURE "drtinao"
#define FORMAT_CANNOT_CREATE_FILE "CANNOT CREATE FILE"

#define MKDIR_NO_INODE "No free inode found, folder cannot be created..."
#define MKDIR_NO_CLUSTER "No free cluster found, folder cannot be created..."
#define MKDIR_EXCEED_LENGTH "Maximum length for folder name exceeded, folder cannot be created..."

#define RMDIR_NOT_EMPTY "Folder \"%s\" is not empty and cannot be deleted..."
#define RMDIR_NOT_FOUND "Nothing to delete - folder \"%s\" was not found in current directory...\n"

#define RMDIR_EXCEED_LENGTH "Maximum length for folder name exceeded, folder with the given name was not even created..."

#define HELP_CP_CONT "****cp - START****\nPerforms command cp (syntax: cp f1 f2), which is used for copying file f1 to location f2.\n****cp - END****\n"
#define HELP_MV_CONT "****mv - START****\nPerforms command mv (syntax: mv f1 f2), which is used for moving file f1 to location f2.\n****mv - END****\n"
#define HELP_RM_CONT "****rm - START****\nPerforms command rm (syntax: rm f1), which is used for removing file f1.\n****rm - END****\n"
#define HELP_MKDIR_CONT "****mkdir - START****\nPerforms command mkdir (syntax: mkdir d1), which is used for creating directory d1.\n****mkdir - END****\n"
#define HELP_RMDIR_CONT "****rmdir - START****\nPerforms command rmdir (syntax: rmdir d1), which is used for removing empty directory d1.\n****rmdir - END****\n"
#define HELP_LS_CONT "****ls - START****\nPerforms command ls (syntax: ls d1), which is used for printing content of directory d1.\n****ls - END****\n"
#define HELP_CAT_CONT "****cat - START****\nPerforms command cat (syntax: cat f1), which is used for printing content of file f1.\n****cat - END****\n"
#define HELP_CD_CONT "****cd - START****\nPerforms command cd (syntax: cd d1), which is used for changing actual path to directory d1.\n****cd - END****\n"
#define HELP_PWD_CONT "****pwd - START****\nPerforms command pwd (syntax: pwd), which is used for printing actual path.\n****pwd - END****\n"
#define HELP_INFO_CONT "****info - START****\nPerforms command info (syntax: info d1/f1), which is used for printing info about file/directory f1/d1 (in which cluster is located).\n****info - END****\n"
#define HELP_INCP_CONT "****incp - START****\nPerforms command incp (syntax: incp f1 f2), which is used for copying file f1 to location f2 in pseudoNTFS.\n****incp - END****\n"
#define HELP_OUTCP_CONT "****outcp - START****\nPerforms command outcp (syntax: outcp f1 f2), which is used for copying file f1 from pseudoNTFS to location f2 on hard drive.\n****outcp - END****\n"
#define HELP_LOAD_CONT "****load - START****\nPerforms command load (syntax: load f1), which is used for loading file with commands from hard drive. Commands in the file must be in corresponding format (1 command = 1 line).\n****load - END****\n"
#define HELP_FORMAT_CONT "****format - START****\nPerforms command format (syntax: format xMB), which is used for formatting file which was entered by user (as a program parameter) to a file system of given size (x has to be replaced by size in MB). All data within the file will be deleted. If the file does not exists, then it will be created.\n****format - END****\n"
#define HELP_DEFRAG_CONT "****defrag - START****\nPerforms command defrag (syntax: defrag), which is used for defragmentation of the file which contains file system. Ie.: First will be occupied data blocks and after them will be free data blocks.\n****defrag - END****\n"
#define HELP_EXIT_CONT "****exit - START****\nPerforms command exit (syntax: exit), which is used for exiting.\n****exit - END****\n"

#define CAT_START "Printing content of: %s - START\n"
#define CAT_SIZE "Size of file: %dB\n"
#define CAT_END "Printing content of: %s - END\n"

#define CD_NOT_FOUND "Folder with name \"%s\" does not exist!\n"

#define PWD_ACTUAL_PATH "Actual path: "

#define INCP_FILE_DOESNT_EXIST "File with name: \"%s\" does not exist on your physical drive...\n"
#define INCP_NO_INODE "No free inode found, file cannot be imported..."
#define INCP_NO_CLUSTER "Not enough space (clusters) on your drive, file cannot be imported..."
#define INCP_EXCEED_LENGTH "Maximum length for file name exceeded, file cannot be imported..."
#define INCP_MAX_FNAME_LENGTH 14

#define LOAD_FILE_DOESNT_EXIST "File with name: \"%s\" does not exist on your physical drive...\n"

#define OUTCP_COPY_START "Copying file - START\n"
#define OUTCP_INODE_NUMBER "Inode number (id): %d\n"
#define OUTCP_NUM_REF "Number of references: %d\n"
#define OUTCP_SIZE "Size: %dB\n"
#define OUTCP_COPY_END "Copying file - END\n"

#define DEFRAG_NO_CLUSTER "Not enough space (clusters) on your drive, drive was not fully defragmented..."

#define CP_NO_CLUSTER "Not enough space (clusters) on your drive, file cannot be copied..."
#define CP_NO_INODE "No free inode found, file cannot be copied..."

#define INFO_START "Info about inode named: %s - START\n"
#define INFO_INODE_NUM "Inode number (id): %d\n"
#define INFO_NUM_REF "Number of references: %d\n"
#define INFO_SIZE "Size: %dB\n"
#define INFO_DIRECTORY "Inode is pointing to directory\n"
#define INFO_FILE "Inode is pointing to file\n"
#define INFO_DIRECT_CLUS "Occupied clusters (direct pointer): %d"
#define INFO_NO_DIRECT "No direct pointer assigned... \n"
#define INFO_INDIRECT1_LIST "Indirect1 list with clusters is on: %d cluster\n"
#define INFO_OCCUPIED_INDIRECT1 "Occupied clusters (indirect1 pointer): %d"
#define INFO_INDIRECT1_NOT_ASSIGNED "No indirect1 pointer assigned... \n"

#define FILE_NOT_FOUND "Item in path was not found. No changes have been made...\n"
#define ROOT_FS_SEARCH "You are in root of FS! You cannot use \"%s\" in this place. No changes have been made...\n"
#define PROVIDE_VALID_PATH "Please provide valid path...\n"

#include "def_struct_fs.h"
/* ____________________________________________________________________________

    Function prototypes
   ____________________________________________________________________________
*/
int get_free_cluster();
struct free_clusters_struct * get_free_cluster_arr(int count);
bool content_fs_check();
int * get_clust_inode_arr_defrag();
void move_inode_clust_defrag(int *cluster_inode_arr_defrag, int *cluster_wanted, int cluster_wanted_length);
void mark_cluster_unused_defrag(int inode_index);
void move_inode_spec_clust_defrag(int inode_index, int *clusters, int clusters_length);
void perform_cp(char *first_word, char *second_word);
void perform_mv(char *first_word, char *second_word);
void perform_rm(char *first_word);
void perform_mkdir(char *first_word);
void perform_rmdir(char *first_word);
void perform_ls(char *first_word);
void perform_cat(char *first_word);
void perform_cd(char *first_word);
void perform_pwd();
void perform_info(char *first_word);
void perform_incp(char *first_word, char *second_word);
void perform_outcp(char *first_word, char *second_word);
void perform_load(char *first_word);
void perform_format(char *first_word);
void perform_defrag();
void perform_help();
void perform_exit();

#endif //ZOS_CMD_FUNCTIONS_FS_H
