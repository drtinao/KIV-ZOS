#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "main.h"
#include "io_fs.h"
#include "cmd_functions_fs.h"

bool formatted = false;

/* ____________________________________________________________________________
    int main(int argc, char *argv[])

    Entry point of the program - 2 parameters are expected (1 given by user).
    First parameter is the name of the program, second is the name of data file.

    Function returns:
    a) EXIT_SUCCESS (0) - if parameter count is right
    b) EXIT_FAILURE (1) - if wrong number of parameters entered
   ____________________________________________________________________________
*/
int main(int argc, char *argv[]){
    /* perform check for parameters count */
    if(check_args_count(argc, argv) != 0) {
        printf(WRONG_PARAM_COUNT);
        return EXIT_FAILURE;
    }

    /* save name of file for later use */
    set_filename_fs(argv[1]);

    bool result = content_fs_check();
    if(result == false){ /* user must first perform format, other functions will be unavailable... */
        formatted = false;
        printf(UNFORMATTED);
    }else{
        formatted = true;
        perform_cd(".");
    }

    read_command();
    return EXIT_SUCCESS;
}

/* ____________________________________________________________________________
    int check_args_count(int argc, char *argv[])

    Function checks the number of parameters given to the program.
    Two arguments expected (= user given name of filesystem).

    Function returns:
    a) EXIT_SUCCESS (0) - if number of parameters == 2
    b) EXIT_FAILURE (1) - if number of parameters != 2
   ____________________________________________________________________________
*/
int check_args_count(int argc, char *argv[]) {
    if(argc == 2) {
        return EXIT_SUCCESS;
    }else {
        return EXIT_FAILURE;
    }
}

/* ____________________________________________________________________________
    void read_command()

    Reads commands entered by user and reacts to them.
    Command can be up to 120 characters long.
   ____________________________________________________________________________
*/
void read_command(){
    while(1){
        char cmd_user[120];                          /* command entered by user */
        char *words_entered[3] = {NULL, NULL, NULL}; /* if there are more than 3 spaces in input, then input is wrong */
        int words_entered_counter = 0;               /* number of words stored in "words_entered" array */
        char *one_word_entered = NULL;               /* one of the words entered by user - used for traversing */

        printf(">"); /* just to show that some input from user is expected */
        fgets(cmd_user, 120, stdin);

        cmd_user[strlen(cmd_user) - 1] = 0; /* delete linebreak on the end */

        one_word_entered = strtok(cmd_user, " ");
        while(one_word_entered != NULL){
            /* check whether number of words in command is correct (max 3 words) */
            if(words_entered_counter > 2){
                printf(WRONG_COMMAND_LENGTH);
            }

            words_entered[words_entered_counter] = one_word_entered;
            one_word_entered = strtok(NULL, " ");
            words_entered_counter = words_entered_counter + 1;
        }

        /* maximum number of words ok, continue with check for every single command
         * - some commands consist of just one word ("pwd"),
         * some of two words ("ls a1", "rmdir a1"),
         * and some of three words ("mv s1 s2", "cp s1 s2")*/
        if(strcmp(words_entered[0], "format") != 0 && formatted == false){
            printf(UNFORMATTED);
            continue;
        }

        /* case for every possible command */
        if(strcmp(words_entered[0], "cp") == 0) { /* cp s1 s2 */
            if(words_entered[1] && words_entered[2]){ /* three words total */
                if(strcmp(words_entered[1], "/") == 0 || strcmp(words_entered[1], ".") == 0 || strcmp(words_entered[1], "..") == 0 || strcmp(words_entered[2], "/") == 0 || strcmp(words_entered[2], ".") == 0 || strcmp(words_entered[2], "..") == 0){
                    printf(WRONG_CP_COPY);
                }else{
                    perform_cp(words_entered[1], words_entered[2]);
                }
            }else{
                printf(WRONG_CP_SYNTAX);
            }
        }else if(strcmp(words_entered[0], "mv") == 0){ /* mv s1 s2 */
            if(words_entered[1] && words_entered[2]){ /* three words total */
                if(strcmp(words_entered[1], "/") == 0 || strcmp(words_entered[1], ".") == 0 || strcmp(words_entered[1], "..") == 0 || strcmp(words_entered[2], "/") == 0 || strcmp(words_entered[2], ".") == 0 || strcmp(words_entered[2], "..") == 0){
                    printf(WRONG_MV_MOVE);
                }else{
                    perform_mv(words_entered[1], words_entered[2]);
                }
            }else{
                printf(WRONG_MV_SYNTAX);
            }
        }else if(strcmp(words_entered[0], "rm") == 0){ /* rm s1 */
            if(words_entered[1] && !words_entered[2]){ /* two words total */;
                if(strcmp(words_entered[1], "/") == 0 || strcmp(words_entered[1], ".") == 0 || strcmp(words_entered[1], "..") == 0){
                    printf(WRONG_RM_REMOVE);
                }else{
                    perform_rm(words_entered[1]);
                }
            }else{
                printf(WRONG_RM_SYNTAX);
            }
        }else if(strcmp(words_entered[0], "mkdir") == 0){ /* mkdir a1 */
            if(words_entered[1] && !words_entered[2]){ /* two words total */
                if(strcmp(words_entered[1], "/") == 0 || strcmp(words_entered[1], ".") == 0 || strcmp(words_entered[1], "..") == 0){
                    printf(WRONG_MKDIR_CREATE);
                }else{
                    perform_mkdir(words_entered[1]);
                }
            }else{
                printf(WRONG_MKDIR_SYNTAX);
            }
        }else if(strcmp(words_entered[0], "rmdir") == 0){ /* rmdir a1 */
            if(words_entered[1] && !words_entered[2]){ /* two words total */
                if(strcmp(words_entered[1], "/") == 0 || strcmp(words_entered[1], ".") == 0 || strcmp(words_entered[1], "..") == 0){
                    printf(WRONG_RMDIR_REMOVE);
                }else{
                    perform_rmdir(words_entered[1]);
                }
            }else{
                printf(WRONG_RMDIR_SYNTAX);
            }
        }else if(strcmp(words_entered[0], "ls") == 0){ /* ls a1 */
            if(words_entered[1] && !words_entered[2]){ /* two words total */
                perform_ls(words_entered[1]);
            }else{
                printf(WRONG_LS_SYNTAX);
            }
        }else if(strcmp(words_entered[0], "cat") == 0){ /* cat s1 */
            if(words_entered[1] && !words_entered[2]){ /* two words total */
                if(strcmp(words_entered[1], "/") == 0 || strcmp(words_entered[1], ".") == 0 || strcmp(words_entered[1], "..") == 0){
                    printf(WRONG_CAT_INPUT);
                }else{
                    perform_cat(words_entered[1]);
                }
            }else{
                printf(WRONG_CAT_SYNTAX);
            }
        }else if(strcmp(words_entered[0], "cd") == 0){ /* cd a1 */
            if(words_entered[1] && !words_entered[2]){ /* two words total */
                perform_cd(words_entered[1]);
            }else{
                printf(WRONG_CD_SYNTAX);
            }
        }else if(strcmp(words_entered[0], "pwd") == 0){ /* pwd */
            if(!words_entered[1] && !words_entered[2]){ /* one word total */
                perform_pwd();
            }else{
                printf(WRONG_PWD_SYNTAX);
            }
        }else if(strcmp(words_entered[0], "info") == 0){ /* info a1/s1 */
            if(words_entered[1] && !words_entered[2]){ /* two words total */
                if(strcmp(words_entered[1], "/") == 0 || strcmp(words_entered[1], ".") == 0 || strcmp(words_entered[1], "..") == 0){
                    printf(WRONG_INFO_USAGE);
                }else{
                    perform_info(words_entered[1]);
                }
            }else{
                printf(WRONG_INFO_SYNTAX);
            }
        }else if(strcmp(words_entered[0], "incp") == 0){ /* incp s1 s2 */
            if(words_entered[1] && words_entered[2]){ /* two words total */
                if(strcmp(words_entered[2], "/") == 0 || strcmp(words_entered[2], ".") == 0 || strcmp(words_entered[2], "..") == 0){
                    printf(WRONG_INCP_USAGE);
                }else{
                    perform_incp(words_entered[1], words_entered[2]);
                }
            }else{
                printf(WRONG_INCP_SYNTAX);
            }
        }else if(strcmp(words_entered[0], "outcp") == 0){ /* outcp s1 s2 */
            if(words_entered[1] && words_entered[2]){ /* two words total */
                if(strcmp(words_entered[1], "/") == 0 || strcmp(words_entered[1], ".") == 0 || strcmp(words_entered[1], "..") == 0){
                    printf(WRONG_OUTCP_USAGE);
                }else{
                    perform_outcp(words_entered[1], words_entered[2]);
                }
            }else{
                printf(WRONG_OUTCP_SYNTAX);
            }
        }else if(strcmp(words_entered[0], "load") == 0){ /* load s1 */
            if(words_entered[1] && !words_entered[2]){ /* two words total */
                perform_load(words_entered[1]);
            }else{
                printf(WRONG_LOAD_SYNTAX);
            }
        }else if(strcmp(words_entered[0], "format") == 0){ /* format 600MB */
            if(words_entered[1] && !words_entered[2]){ /* two words total */
                formatted = true;
                perform_format(words_entered[1]);
            }else{
                printf(WRONG_FORMAT_SYNTAX);
            }
        }else if(strcmp(words_entered[0], "defrag") == 0){ /* defrag */
            if(!words_entered[1] && !words_entered[2]){ /* one word total */
                perform_defrag();
            }else{
                printf(WRONG_DEFRAG_SYNTAX);
            }
        }else if(strcmp(words_entered[0], "help") == 0){ /* help */
            if(!words_entered[1] && !words_entered[2]){ /* one word total */
                perform_help();
            }else{
                printf(WRONG_HELP_SYNTAX);
            }
        }else if(strcmp(words_entered[0], "exit") == 0){ /* exit */
            if(!words_entered[1] && !words_entered[2]){ /* one word total */
                perform_exit();
                break;
            }else{
                printf(WRONG_EXIT_SYNTAX);
            }
        }else{
            printf(UNKNOWN);
        }
    }
}