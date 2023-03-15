#include "def_struct_fs.h"

#ifndef ZOS_MAIN_H
#define ZOS_MAIN_H

#define WRONG_PARAM_COUNT "EXITING - WRONG PARAMETERS COUNT... PLEASE ENTER NAME OF FILESYSTEM.\n"
#define WRONG_COMMAND_LENGTH "WRONG COMMAND LENGTH... MAXIMUM 3 SEPARATED WORDS EXPECTED.\n"
#define UNFORMATTED "Disc has not yet been formatted, perform format command first!\n"

#define WRONG_CP_SYNTAX "Wrong \"cp\" command syntax - use \"help\" command for manual.\n"
#define WRONG_CP_COPY "Cannot create files with names \".\", \"..\", \"/\"!\n"
#define WRONG_MV_SYNTAX "Wrong \"mv\" command syntax - use \"help\" command for manual.\n"
#define WRONG_MV_MOVE "Cannot move files with names \".\", \"..\", \"/\"!\n"
#define WRONG_RM_SYNTAX "Wrong \"rm\" command syntax - use \"help\" command for manual.\n"
#define WRONG_RM_REMOVE "File with name \".\", \"..\", \"/\" cannot even be created!\n"
#define WRONG_MKDIR_SYNTAX "Wrong \"mkdir\" command syntax - use \"help\" command for manual.\n"
#define WRONG_RMDIR_SYNTAX "Wrong \"rmdir\" command syntax - use \"help\" command for manual.\n"
#define WRONG_RMDIR_REMOVE "Folder with names \".\", \"..\", \"/\" cannot even be created!\n"
#define WRONG_MKDIR_CREATE "Cannot make folder with names \".\", \"..\", \"/\"!\n"
#define WRONG_LS_SYNTAX "Wrong \"ls\" command syntax - use \"help\" command for manual.\n"
#define WRONG_CAT_SYNTAX "Wrong \"cat\" command syntax - use \"help\" command for manual.\n"
#define WRONG_CAT_INPUT "File with name \".\", \"..\", \"/\" cannot even be created!\n"
#define WRONG_CD_SYNTAX "Wrong \"cd\" command syntax - use \"help\" command for manual.\n"
#define WRONG_PWD_SYNTAX "Wrong \"pwd\" command syntax - use \"help\" command for manual.\n"
#define WRONG_INFO_SYNTAX "Wrong \"info\" command syntax - use \"help\" command for manual.\n"
#define WRONG_INFO_USAGE "Folders or files with names \".\", \"..\", \"/\" cannot even be created!\n"
#define WRONG_INCP_SYNTAX "Wrong \"incp\" command syntax - use \"help\" command for manual.\n"
#define WRONG_INCP_USAGE "Files with names \".\", \"..\", \"/\" cannot be created!\n"
#define WRONG_OUTCP_SYNTAX "Wrong \"outcp\" command syntax - use \"help\" command for manual.\n"
#define WRONG_OUTCP_USAGE "Files with names \".\", \"..\", \"/\" cannot even be created!\n"
#define WRONG_LOAD_SYNTAX "Wrong \"load\" command syntax - use \"help\" command for manual.\n"
#define WRONG_FORMAT_SYNTAX "Wrong \"format\" command syntax - use \"help\" command for manual.\n"
#define WRONG_DEFRAG_SYNTAX "Wrong \"defrag\" command syntax - use \"help\" command for manual.\n"
#define WRONG_HELP_SYNTAX "Wrong \"help\" command syntax - use just \"help\" for manual.\n"
#define WRONG_EXIT_SYNTAX "Wrong \"exit\" command syntax - use \"help\" command for manual.\n"
#define UNKNOWN "unknown command...\n"
/* ____________________________________________________________________________

    Function prototypes
   ____________________________________________________________________________
*/
int check_args_count(int argc, char *argv[]);
void free_fs_file_content(struct fs_file_content *content_fs);
void read_command();

#endif //ZOS_MAIN_H
