#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>
#include <math.h>
#include "io_fs.h"
#include "def_struct_fs.h"
#include "cmd_functions_fs.h"
#include "save_struct_fs.h"
#include "load_struct_fs.h"
#include "main.h"

struct fs_file_content *content_fs = NULL;

char actual_folder_name[15]; /* name of currently traversed folder */
struct pseudo_inode *actual_pseudo_inode = NULL; /* inode which represents currently traversed folder */

/* ____________________________________________________________________________

    int get_free_cluster()

    Function returns index of free cluster (if exists) - else -1 is returned.
   ____________________________________________________________________________
*/
int get_free_cluster(){
    int free_cluster_number = -1;
    int total_cluster_count = (content_fs -> loaded_bit_clus -> cluster_free_total) + (content_fs -> loaded_bit_clus -> cluster_occupied_total);
    int i = 0;
    for(; i < total_cluster_count; i++){
        if(content_fs -> loaded_bit_clus -> cluster_bitmap[i] == 0){
            free_cluster_number = i;
            break;
        }
    }

    return free_cluster_number;
}

/* ___________________________________________________________________________________

    struct free_clusters_struct * get_free_cluster_arr(int count)

    Function returns a pointer to structure which contains array of free clusters
    if number of clusters specified by "int count" was found, else NULL is returned.
   ___________________________________________________________________________________
*/
struct free_clusters_struct * get_free_cluster_arr(int count){
    struct free_clusters_struct *free_clusters = malloc(sizeof(struct free_clusters_struct));
    free_clusters -> free_clusters_arr = NULL;
    free_clusters -> free_clusters_arr_length = 0;

    int total_cluster_count = (content_fs -> loaded_bit_clus -> cluster_free_total) + (content_fs -> loaded_bit_clus -> cluster_occupied_total);

    int i = 0;
    bool count_ok = false;
    for(; i < total_cluster_count; i++){
        if(free_clusters -> free_clusters_arr_length == count){
            count_ok = true;
            break;
        }

        if(content_fs -> loaded_bit_clus -> cluster_bitmap[i] == 0){
            free_clusters -> free_clusters_arr_length += 1;
            free_clusters -> free_clusters_arr = realloc(free_clusters -> free_clusters_arr, free_clusters -> free_clusters_arr_length *
                    sizeof(int));
            free_clusters -> free_clusters_arr[free_clusters -> free_clusters_arr_length - 1] = i;
        }
    }

    if(count_ok == true){
        return free_clusters;
    }else{
        return NULL;
    }
}

/* __________________________________________________________________________________

    bool content_fs_check()

    Function checks whether file with user given name exists and if it does exist,
    then pointer to structure with info about FS is assigned - else NULL is assigned.

    Function returns:
    a) true - if file already exists
    b) false - file does not exist
   __________________________________________________________________________________
*/
bool content_fs_check(){
    if(check_file_existence(get_filename_fs()) == true){
        printf(CONTENT_FS_FILE_FOUND);
        content_fs = load_fs_file();
        return true;
    }else{
        content_fs = NULL;
        return false;
    }
}

/* ____________________________________________________________________________
    void perform_cp(char *first_word, char *second_word)

    Performs command cp (syntax: cp f1 f2), which is used for copying
    file f1 to location f2.
   ____________________________________________________________________________
*/
void perform_cp(char *first_word, char *second_word){
    struct pseudo_inode *actual_pseudo_inode_backup = actual_pseudo_inode; /* we will switch inodes */

    struct pseudo_inode *pseudo_inode_source = NULL; /* inode of folder from which the file will be copied */
    struct pseudo_inode *pseudo_inode_target = NULL; /* inode of folder to which the file will be copied */

    char (*item_source_arr)[15] = NULL; /* array of item names which are in the source path */
    int item_source_arr_length = 0;
    char (*item_target_arr)[15] = NULL; /* array of item names which are in the path of target */
    int item_target_arr_length = 0;

    bool error_occured = false; /* if true, then error occured (not enough clusters, etc.) -> file cannot be copied */

    char *one_word = NULL;
    /* split source path by '/' to get individual folders - START */
    one_word = strtok(first_word, "/");
    while(one_word != NULL) {
        item_source_arr = realloc(item_source_arr, (item_source_arr_length + 1) * sizeof *item_source_arr);
        strcpy(item_source_arr[item_source_arr_length], one_word);
        one_word = strtok(NULL, "/"); /* move to next word */

        item_source_arr_length += 1;
    }
    /* split source path by '/' to get individual folders - END */

    /* split target path by '/' to get individual folders - START */
    one_word = strtok(second_word, "/");
    while(one_word != NULL) {
        item_target_arr = realloc(item_target_arr, (item_target_arr_length + 1) * sizeof *item_target_arr);
        strcpy(item_target_arr[item_target_arr_length], one_word);
        one_word = strtok(NULL, "/"); /* move to next word */

        item_target_arr_length += 1;
    }
    /* split target path by '/' to get individual folders - END */

    /* find inode for source path - START */
    if(first_word[0] == '/'){ /* path is absolute, load first inode and continue traversing */
        actual_pseudo_inode = &content_fs -> loaded_pseudo_in_arr[0];

        int x = 0;
        for(; x < item_source_arr_length; x++){
            open_rb_fs();
            int numb_it_fol_untraver = (actual_pseudo_inode -> file_size) / sizeof(struct directory_item); /* number of untraversed items in folder */
            struct directory_item *directory_items = malloc(sizeof(struct directory_item) * numb_it_fol_untraver);
            int free_index_directory_items = 0;
            int array_data_blocks[5];

            /* fill array of data block numbers for better traversing */
            array_data_blocks[0] = actual_pseudo_inode -> direct1;
            array_data_blocks[1] = actual_pseudo_inode -> direct2;
            array_data_blocks[2] = actual_pseudo_inode -> direct3;
            array_data_blocks[3] = actual_pseudo_inode -> direct4;
            array_data_blocks[4] = actual_pseudo_inode -> direct5;

            int i = 0;
            for(; i < 5; i++){ /* read direct */
                if(numb_it_fol_untraver != 0){
                    int read_count = -1;
                    int j = 0;

                    if(numb_it_fol_untraver * sizeof(struct directory_item) <= FORMAT_CLUSTER_SIZE_B){ /* read just part of a cluster */
                        read_count = numb_it_fol_untraver;
                    }else{ /* read whole cluster */
                        read_count = FORMAT_CLUSTER_SIZE_B / sizeof(struct directory_item);
                    }

                    for(; j < read_count; j++){
                        int start_address = content_fs -> loaded_sb -> data_start_address + array_data_blocks[i] * FORMAT_CLUSTER_SIZE_B + (j * sizeof(struct directory_item));
                        set_pointer_position_fs(start_address, SEEK_SET);
                        struct directory_item *dir_item = (struct directory_item *) read_from_file_fs(sizeof(struct directory_item));
                        directory_items[free_index_directory_items] = *dir_item;
                        free(dir_item);
                        free_index_directory_items++;
                        numb_it_fol_untraver--;
                    }
                }
            }
            close_file_fs();

            /* all directory items loaded, does wanted folder exist? */
            i = 0;
            int new_inode_number = -1;
            for(; i < free_index_directory_items; i++){
                if(strcmp(directory_items[i].item_name, item_source_arr[x]) == 0){ /* inode with desired name has been found */
                    new_inode_number = directory_items[i].inode;
                    break;
                }
            }

            if(new_inode_number != -1){
                if(x == item_source_arr_length - 1){ /* inode on last position should represent file */
                    if(content_fs -> loaded_pseudo_in_arr[new_inode_number].isDirectory == false){
                        pseudo_inode_source = &content_fs -> loaded_pseudo_in_arr[new_inode_number];
                    }else{
                        printf(FILE_NOT_FOUND);
                        error_occured = true;
                        free(directory_items);
                        break;
                    }
                }else{ /* other inodes should represent folder */
                    if(content_fs -> loaded_pseudo_in_arr[new_inode_number].isDirectory == true){
                        actual_pseudo_inode = &content_fs -> loaded_pseudo_in_arr[new_inode_number];
                    }else{
                        printf(FILE_NOT_FOUND);
                        error_occured = true;
                        free(directory_items);
                        break;
                    }
                }
            }else{
                printf(FILE_NOT_FOUND);
                error_occured = true;
                free(directory_items);
                break;
            }
            free(directory_items);
        }
    }else{ /* path is relative, go from current inode and try to traverse */
        actual_pseudo_inode = actual_pseudo_inode_backup;

        int x = 0;
        for(; x < item_source_arr_length; x++){
            open_rb_fs();
            int numb_it_fol_untraver = (actual_pseudo_inode -> file_size) / sizeof(struct directory_item); /* number of untraversed items in folder */
            struct directory_item *directory_items = malloc(sizeof(struct directory_item) * numb_it_fol_untraver);
            int free_index_directory_items = 0;
            int array_data_blocks[5];

            /* fill array of data block numbers for better traversing */
            array_data_blocks[0] = actual_pseudo_inode -> direct1;
            array_data_blocks[1] = actual_pseudo_inode -> direct2;
            array_data_blocks[2] = actual_pseudo_inode -> direct3;
            array_data_blocks[3] = actual_pseudo_inode -> direct4;
            array_data_blocks[4] = actual_pseudo_inode -> direct5;

            int i = 0;
            for(; i < 5; i++){ /* read direct */
                if(numb_it_fol_untraver != 0){
                    int read_count = -1;
                    int j = 0;

                    if(numb_it_fol_untraver * sizeof(struct directory_item) <= FORMAT_CLUSTER_SIZE_B){ /* read just part of a cluster */
                        read_count = numb_it_fol_untraver;
                    }else{ /* read whole cluster */
                        read_count = FORMAT_CLUSTER_SIZE_B / sizeof(struct directory_item);
                    }

                    for(; j < read_count; j++){
                        int start_address = content_fs -> loaded_sb -> data_start_address + array_data_blocks[i] * FORMAT_CLUSTER_SIZE_B + (j * sizeof(struct directory_item));
                        set_pointer_position_fs(start_address, SEEK_SET);
                        struct directory_item *dir_item = (struct directory_item *) read_from_file_fs(sizeof(struct directory_item));
                        directory_items[free_index_directory_items] = *dir_item;
                        free(dir_item);
                        free_index_directory_items++;
                        numb_it_fol_untraver--;
                    }
                }
            }
            close_file_fs();

            /* all directory items loaded, does wanted folder exist? */
            int new_inode_number = -1;
            bool in_root_upper = false;

            if(strlen(item_source_arr[x]) == 1 && item_source_arr[x][0] == '.'){
                new_inode_number = directory_items[0].inode; /* first directory_item in folder is the folder itself */
            }else if(strlen(item_source_arr[x]) == 2 && item_source_arr[x][0] == '.' && item_source_arr[x][1] == '.'){
                if(directory_items[0].item_name[0] == '/'){ /* user is in root, cannot move upper */
                    in_root_upper = true;
                    error_occured = true;
                }else{
                    new_inode_number = directory_items[1].inode; /* second directory_item in folder is the upper folder */
                }
            }else{
                i = 0;
                for(; i < free_index_directory_items; i++){
                    if(strcmp(directory_items[i].item_name, item_source_arr[x]) == 0){ /* inode with desired name has been found */
                        new_inode_number = directory_items[i].inode;
                        break;
                    }
                }
            }

            if(new_inode_number != -1){
                if(x == item_source_arr_length - 1){ /* inode on last position should represent file */
                    if(content_fs -> loaded_pseudo_in_arr[new_inode_number].isDirectory == false){
                        pseudo_inode_source = &content_fs -> loaded_pseudo_in_arr[new_inode_number];
                    }else{
                        printf(FILE_NOT_FOUND);
                        error_occured = true;
                        free(directory_items);
                        break;
                    }
                }else{ /* other inodes should represent folder */
                    if(content_fs -> loaded_pseudo_in_arr[new_inode_number].isDirectory == true){
                        actual_pseudo_inode = &content_fs -> loaded_pseudo_in_arr[new_inode_number];
                    }else{
                        printf(FILE_NOT_FOUND);
                        error_occured = true;
                        free(directory_items);
                        break;
                    }
                }
            }else{
                if(in_root_upper == true){
                    printf(ROOT_FS_SEARCH, item_source_arr[x]);
                }else{
                    printf(FILE_NOT_FOUND);
                }
                free(directory_items);
                break;
            }
            free(directory_items);
        }
    }
    /* find inode for source path - END */

    /* find inode for target path - START */
    if(second_word[0] == '/'){ /* path is absolute, load first inode and continue traversing */
        actual_pseudo_inode = &content_fs -> loaded_pseudo_in_arr[0];

        int x = 0;
        for(; x < item_target_arr_length - 1; x++){ /* -1 because last word should be name of the file which is not yet created */
            open_rb_fs();
            int numb_it_fol_untraver = (actual_pseudo_inode -> file_size) / sizeof(struct directory_item); /* number of untraversed items in folder */
            struct directory_item *directory_items = malloc(sizeof(struct directory_item) * numb_it_fol_untraver);
            int free_index_directory_items = 0;
            int array_data_blocks[5];

            /* fill array of data block numbers for better traversing */
            array_data_blocks[0] = actual_pseudo_inode -> direct1;
            array_data_blocks[1] = actual_pseudo_inode -> direct2;
            array_data_blocks[2] = actual_pseudo_inode -> direct3;
            array_data_blocks[3] = actual_pseudo_inode -> direct4;
            array_data_blocks[4] = actual_pseudo_inode -> direct5;

            int i = 0;
            for(; i < 5; i++){ /* read direct */
                if(numb_it_fol_untraver != 0){
                    int read_count = -1;
                    int j = 0;

                    if(numb_it_fol_untraver * sizeof(struct directory_item) <= FORMAT_CLUSTER_SIZE_B){ /* read just part of a cluster */
                        read_count = numb_it_fol_untraver;
                    }else{ /* read whole cluster */
                        read_count = FORMAT_CLUSTER_SIZE_B / sizeof(struct directory_item);
                    }

                    for(; j < read_count; j++){
                        int start_address = content_fs -> loaded_sb -> data_start_address + array_data_blocks[i] * FORMAT_CLUSTER_SIZE_B + (j * sizeof(struct directory_item));
                        set_pointer_position_fs(start_address, SEEK_SET);
                        struct directory_item *dir_item = (struct directory_item *) read_from_file_fs(sizeof(struct directory_item));
                        directory_items[free_index_directory_items] = *dir_item;
                        free(dir_item);
                        free_index_directory_items++;
                        numb_it_fol_untraver--;
                    }
                }
            }
            close_file_fs();

            /* all directory items loaded, does wanted folder exist? */
            i = 0;
            int new_inode_number = -1;
            for(; i < free_index_directory_items; i++){
                if(strcmp(directory_items[i].item_name, item_target_arr[x]) == 0){ /* inode with desired name has been found */
                    new_inode_number = directory_items[i].inode;
                    break;
                }
            }

            if(new_inode_number != -1){
                if(content_fs -> loaded_pseudo_in_arr[new_inode_number].isDirectory == true){
                    if(x == item_target_arr_length - 2){
                        pseudo_inode_target = &content_fs -> loaded_pseudo_in_arr[new_inode_number];
                    }else{
                        actual_pseudo_inode = &content_fs -> loaded_pseudo_in_arr[new_inode_number];
                    }
                }else{
                    printf(FILE_NOT_FOUND);
                    error_occured = true;
                    free(directory_items);
                    break;
                }
            }else{
                printf(FILE_NOT_FOUND);
                error_occured = true;
                free(directory_items);
                break;
            }
            free(directory_items);
        }
    }else{ /* path is relative, go from current inode and try to traverse */
        actual_pseudo_inode = actual_pseudo_inode_backup;

        int x = 0;
        for(; x < item_target_arr_length - 1; x++){
            open_rb_fs();
            int numb_it_fol_untraver = (actual_pseudo_inode -> file_size) / sizeof(struct directory_item); /* number of untraversed items in folder */
            struct directory_item *directory_items = malloc(sizeof(struct directory_item) * numb_it_fol_untraver);
            int free_index_directory_items = 0;
            int array_data_blocks[5];

            /* fill array of data block numbers for better traversing */
            array_data_blocks[0] = actual_pseudo_inode -> direct1;
            array_data_blocks[1] = actual_pseudo_inode -> direct2;
            array_data_blocks[2] = actual_pseudo_inode -> direct3;
            array_data_blocks[3] = actual_pseudo_inode -> direct4;
            array_data_blocks[4] = actual_pseudo_inode -> direct5;

            int i = 0;
            for(; i < 5; i++){ /* read direct */
                if(numb_it_fol_untraver != 0){
                    int read_count = -1;
                    int j = 0;

                    if(numb_it_fol_untraver * sizeof(struct directory_item) <= FORMAT_CLUSTER_SIZE_B){ /* read just part of a cluster */
                        read_count = numb_it_fol_untraver;
                    }else{ /* read whole cluster */
                        read_count = FORMAT_CLUSTER_SIZE_B / sizeof(struct directory_item);
                    }

                    for(; j < read_count; j++){
                        int start_address = content_fs -> loaded_sb -> data_start_address + array_data_blocks[i] * FORMAT_CLUSTER_SIZE_B + (j * sizeof(struct directory_item));
                        set_pointer_position_fs(start_address, SEEK_SET);
                        struct directory_item *dir_item = (struct directory_item *) read_from_file_fs(sizeof(struct directory_item));
                        directory_items[free_index_directory_items] = *dir_item;
                        free(dir_item);
                        free_index_directory_items++;
                        numb_it_fol_untraver--;
                    }
                }
            }
            close_file_fs();

            /* all directory items loaded, does wanted folder exist? */
            int new_inode_number = -1;
            bool in_root_upper = false;

            if(strlen(item_target_arr[x]) == 1 && item_target_arr[x][0] == '.'){
                new_inode_number = directory_items[0].inode; /* first directory_item in folder is the folder itself */
            }else if(strlen(item_target_arr[x]) == 2 && item_target_arr[x][0] == '.' && item_target_arr[x][1] == '.'){
                if(directory_items[0].item_name[0] == '/'){ /* user is in root, cannot move upper */
                    in_root_upper = true;
                    error_occured = true;
                }else{
                    new_inode_number = directory_items[1].inode; /* second directory_item in folder is the upper folder */
                }
            }else{
                i = 0;
                for(; i < free_index_directory_items; i++){
                    if(strcmp(directory_items[i].item_name, item_target_arr[x]) == 0){ /* inode with desired name has been found */
                        new_inode_number = directory_items[i].inode;
                        break;
                    }
                }
            }

            if(new_inode_number != -1){
                if(content_fs -> loaded_pseudo_in_arr[new_inode_number].isDirectory == true){
                    if(x == item_target_arr_length - 2){
                        pseudo_inode_target = &content_fs -> loaded_pseudo_in_arr[new_inode_number];
                    }else{
                        actual_pseudo_inode = &content_fs -> loaded_pseudo_in_arr[new_inode_number];
                    }
                }else{
                    printf(FILE_NOT_FOUND);
                    error_occured = true;
                    free(directory_items);
                    break;
                }
            }else{
                if(in_root_upper == true){
                    printf(ROOT_FS_SEARCH, item_target_arr[x]);
                }else{
                    printf(FILE_NOT_FOUND);
                }
                free(directory_items);
                break;
            }
            free(directory_items);
        }
    }
    /* find inode for target path - END */

    if(pseudo_inode_source == NULL || pseudo_inode_target == NULL){
        if(item_source_arr_length != 0){
            free(item_source_arr);
        }

        if(item_target_arr_length != 0){
            free(item_target_arr);
        }

        printf(PROVIDE_VALID_PATH);
        return;
    }

    open_wb_add_fs();

    /* get free inode */
    struct pseudo_inode *free_pseudo_inode = NULL;
    int free_pseudo_inode_number = -1;
    int total_inode_count =
            (content_fs->loaded_bit_in->inodes_free_total) + (content_fs->loaded_bit_in->inodes_occupied_total);

    int i = 0;
    for (; i < total_inode_count; i++) {
        if (content_fs->loaded_bit_in->inodes_bitmap[i] == 0) { /* free inode on "i" position found */
            free_pseudo_inode = &content_fs->loaded_pseudo_in_arr[i]; /* (index is the same as with bitmap) */
            free_pseudo_inode_number = i;
            break;
        }
    }

    /* no free inode found */
    if(free_pseudo_inode_number == -1){
        printf(CP_NO_INODE);
        error_occured = true;
    }

    free_pseudo_inode -> file_size = pseudo_inode_source -> file_size;

    int cluster_inode[7];
    cluster_inode[0] = pseudo_inode_source -> direct1;
    cluster_inode[1] = pseudo_inode_source -> direct2;
    cluster_inode[2] = pseudo_inode_source -> direct3;
    cluster_inode[3] = pseudo_inode_source -> direct4;
    cluster_inode[4] = pseudo_inode_source -> direct5;
    cluster_inode[5] = pseudo_inode_source -> indirect1;
    cluster_inode[6] = pseudo_inode_source -> indirect2;

    int file_size = pseudo_inode_source->file_size;

    int occupied_cluster_data = ceil(
            (float) file_size / FORMAT_CLUSTER_SIZE_B); /* number of clusters reserved for data */
    int occupied_cluster_extra = 0; /* number of extra clusters (for indirect pointer list etc.) */
    /* extra clusters will be needed if indirect is going to be used */
    int indirect1_hold = FORMAT_CLUSTER_SIZE_B / sizeof(int);

    /* count extra clusters - START */
    if(occupied_cluster_data > 5){ /* direct pointers not enough, extra cluster for indirect1 */
        occupied_cluster_extra += 1;

        if (occupied_cluster_data > (5 + indirect1_hold)) { /* indirect2 */
            occupied_cluster_extra += 1; /* for first cluster list */

            int second_list_count = ceil((float) (occupied_cluster_data - (5 + indirect1_hold)) /
                                         indirect1_hold); /* needed cluster count / list capacity */
            occupied_cluster_extra += second_list_count;
        }
    }
    /* count extra clusters - END */

    /* get array of clusters */
    struct free_clusters_struct *free_clusters = get_free_cluster_arr(
            occupied_cluster_data + occupied_cluster_extra);

    /* not enough clusters */
    if(free_clusters == NULL){
        printf(CP_NO_CLUSTER);
        error_occured = true;
    }

    if(error_occured == false){
        /* set target inode as actual */
        actual_pseudo_inode = pseudo_inode_target;

        /* read cluster & write - START */
        int address_offset = 0;

        /* direct - START */
        int end;
        if (occupied_cluster_data < 5) {
            end = occupied_cluster_data;
        } else {
            end = 5;
        }

        int cluster_data_counter_cp = 0; /* counter for data clusters assigned to new file */
        i = 0;
        for (; i < end; i++) {
            int start_address = content_fs->loaded_sb->data_start_address + cluster_inode[i] * FORMAT_CLUSTER_SIZE_B;
            set_pointer_position_fs(start_address, SEEK_SET);
            int write_start_addr = content_fs->loaded_sb->data_start_address +
                                   free_clusters->free_clusters_arr[cluster_data_counter_cp] * FORMAT_CLUSTER_SIZE_B;
            void *data;

            if (end < 5 && (i == end - 1)) { /* reading last cluster */
                int size_to_read = file_size - (i * FORMAT_CLUSTER_SIZE_B);
                data = read_from_file_fs(size_to_read); /* remaining size */
                set_pointer_position_fs(write_start_addr, SEEK_SET);
                write_to_file_fs(data, size_to_read);
                address_offset += size_to_read;
            } else {
                data = read_from_file_fs(FORMAT_CLUSTER_SIZE_B);
                set_pointer_position_fs(write_start_addr, SEEK_SET);
                write_to_file_fs(data, FORMAT_CLUSTER_SIZE_B);
                address_offset += FORMAT_CLUSTER_SIZE_B;
            }

            cluster_data_counter_cp += 1;

            free(data);
        }
        /* direct - END */

        /* read indirect */
        if(occupied_cluster_extra > 0){ /* first extra cluster is a list for indirect1 */
            int loop_end;
            if ((occupied_cluster_data - 5) > indirect1_hold) {
                loop_end = indirect1_hold;
            } else {
                loop_end = occupied_cluster_data - 5;
            }
            int start_address = content_fs->loaded_sb->data_start_address + cluster_inode[5] * FORMAT_CLUSTER_SIZE_B;
            set_pointer_position_fs(start_address, SEEK_SET);
            int *clust_indir1_arr = (int *) read_from_file_fs(loop_end * sizeof(int));
            i = 0;
            for (; i < loop_end; i++) {
                int start_address =
                        content_fs->loaded_sb->data_start_address + clust_indir1_arr[i] * FORMAT_CLUSTER_SIZE_B;
                set_pointer_position_fs(start_address, SEEK_SET);
                int write_start_addr = content_fs->loaded_sb->data_start_address +
                                       free_clusters->free_clusters_arr[cluster_data_counter_cp] *
                                       FORMAT_CLUSTER_SIZE_B;
                void *data;
                if (loop_end < indirect1_hold && (i == loop_end - 1)) { /* reading last cluster */
                    int size_to_read = file_size - ((i + 5) * FORMAT_CLUSTER_SIZE_B);
                    data = read_from_file_fs(size_to_read); /* remaining size */
                    set_pointer_position_fs(write_start_addr, SEEK_SET);
                    write_to_file_fs(data, size_to_read);
                    address_offset += size_to_read;
                } else {
                    data = read_from_file_fs(FORMAT_CLUSTER_SIZE_B);
                    set_pointer_position_fs(write_start_addr, SEEK_SET);
                    write_to_file_fs(data, FORMAT_CLUSTER_SIZE_B);
                    address_offset += FORMAT_CLUSTER_SIZE_B;
                }

                cluster_data_counter_cp += 1;
                free(data);
            }
            free(clust_indir1_arr);
            if (occupied_cluster_extra > 1) { /* second extra cluster is a first list for indirect2 */
                int second_list_count = ceil((float) (occupied_cluster_data - 5 - indirect1_hold) / indirect1_hold);
                int start_address =
                        content_fs->loaded_sb->data_start_address + cluster_inode[6] * FORMAT_CLUSTER_SIZE_B;
                set_pointer_position_fs(start_address, SEEK_SET);
                int *second_list_arr = (int *) read_from_file_fs(sizeof(int) * second_list_count);

                i = 0;
                for (; i < second_list_count; i++) {
                    int start_address_sec_list =
                            content_fs->loaded_sb->data_start_address + second_list_arr[i] * FORMAT_CLUSTER_SIZE_B;
                    set_pointer_position_fs(start_address_sec_list, SEEK_SET);
                    int *clusters_arr;
                    if ((occupied_cluster_data - 5 - indirect1_hold - (indirect1_hold * i)) < indirect1_hold) {
                        loop_end = occupied_cluster_data - 5 - indirect1_hold - (indirect1_hold * i);
                    } else {
                        loop_end = indirect1_hold;
                    }
                    clusters_arr = read_from_file_fs(loop_end * sizeof(int));
                    void *data;
                    int j = 0;
                    for (; j < loop_end; j++) {
                        set_pointer_position_fs(
                                content_fs->loaded_sb->data_start_address + clusters_arr[j] * FORMAT_CLUSTER_SIZE_B,
                                SEEK_SET);
                        int write_start_addr = content_fs->loaded_sb->data_start_address +
                                               free_clusters->free_clusters_arr[cluster_data_counter_cp] *
                                               FORMAT_CLUSTER_SIZE_B;
                        if(loop_end < indirect1_hold && (j == loop_end - 1)){ /* reading last cluster */
                            int size_to_read = file_size - ((5 + indirect1_hold + (indirect1_hold * i) + j) *
                                                            FORMAT_CLUSTER_SIZE_B);
                            data = read_from_file_fs(size_to_read); /* remaining size */
                            set_pointer_position_fs(write_start_addr, SEEK_SET);
                            write_to_file_fs(data, size_to_read);
                            address_offset += size_to_read;
                        }else{
                            data = read_from_file_fs(FORMAT_CLUSTER_SIZE_B);
                            set_pointer_position_fs(write_start_addr, SEEK_SET);
                            write_to_file_fs(data, FORMAT_CLUSTER_SIZE_B);
                            address_offset += FORMAT_CLUSTER_SIZE_B;
                        }
                        cluster_data_counter_cp += 1;

                        free(data);
                    }
                    free(clusters_arr);
                }
                free(second_list_arr);
            }
        }
        /* read cluster & write - END */

        /* assign clusters with data to inode - START */
        /* assign direct */
        int *cluster_direct[5];

        cluster_direct[0] = &free_pseudo_inode->direct1;
        cluster_direct[1] = &free_pseudo_inode->direct2;
        cluster_direct[2] = &free_pseudo_inode->direct3;
        cluster_direct[3] = &free_pseudo_inode->direct4;
        cluster_direct[4] = &free_pseudo_inode->direct5;

        end;
        if (file_size <= (FORMAT_CLUSTER_SIZE_B * 5)) {
            end = ceil((float) file_size / FORMAT_CLUSTER_SIZE_B);
        } else {
            end = 5;
        }

        i = 0;
        for (; i < end; i++) {
            *cluster_direct[i] = free_clusters->free_clusters_arr[i];
        }

        /* assign indirect */
        if (occupied_cluster_extra > 0) { /* first extra cluster is a list for indirect1 */
            int *indirect1_arr = NULL; /* array of clusters which will be saved on indirect1 */
            int rem_data_clus = occupied_cluster_data - 5; /* clusters with data count, not assigned */

            int num_items;
            if (rem_data_clus <
                indirect1_hold) { /* less unassigned than cluster is able to hold => indirect2 will not be used */
                num_items = rem_data_clus;
            } else {
                num_items = indirect1_hold;
            }

            indirect1_arr = malloc(sizeof(int) * num_items);

            i = 0;
            for (; i < num_items; i++) { /* fill indirect1 array */
                indirect1_arr[i] = free_clusters->free_clusters_arr[5 + i]; /* 5*direct */
            }

            /* array filled, now write data to first extra cluster */
            int indirect1_list_clust = free_clusters->free_clusters_arr[free_clusters->free_clusters_arr_length - 1 -
                                                                        occupied_cluster_extra +
                                                                        1]; /* find first extra cluster */
            free_pseudo_inode->indirect1 = indirect1_list_clust;
            int indirect1_list_start_addr =
                    (content_fs->loaded_sb->data_start_address) + (indirect1_list_clust * FORMAT_CLUSTER_SIZE_B);
            set_pointer_position_fs(indirect1_list_start_addr, SEEK_SET);
            write_to_file_fs((void *) indirect1_arr, sizeof(int) * num_items);
            rem_data_clus -= num_items; /* update unassigned cluster count */
            free(indirect1_arr);

            if (occupied_cluster_extra > 1) { /* second extra cluster is a first list for indirect2 */
                /* fill second list for indirect2 - START */
                int second_list_count = ceil(
                        (float) (rem_data_clus) / indirect1_hold); /* needed cluster count / list capacity */

                i = 0;
                for (; i < second_list_count; i++) {
                    int indirect2_sec_list_clust = free_clusters->free_clusters_arr[
                            free_clusters->free_clusters_arr_length - 1 - occupied_cluster_extra + 3 +
                            i]; /* find address for extra cluster */
                    int indirect2_sec_list_start_addr = (content_fs->loaded_sb->data_start_address) +
                                                        (indirect2_sec_list_clust * FORMAT_CLUSTER_SIZE_B);
                    set_pointer_position_fs(indirect2_sec_list_start_addr, SEEK_SET);

                    if (i == (second_list_count - 1)) { /* in case of last cluster write */
                        write_to_file_fs((void *) free_clusters->free_clusters_arr +
                                         (5 + indirect1_hold + (i * indirect1_hold)) *
                                         sizeof(int), rem_data_clus * sizeof(int));
                        rem_data_clus = 0;
                    } else {
                        write_to_file_fs((void *) free_clusters->free_clusters_arr +
                                         (5 + indirect1_hold + (i * indirect1_hold)) *
                                         sizeof(int), indirect1_hold * sizeof(int));
                        rem_data_clus -= indirect1_hold;
                    }
                }
                /* fill second list for indirect2 - END */

                /* fill first list for indirect2 - START */
                int indirect2_first_list_clust = free_clusters->free_clusters_arr[
                        free_clusters->free_clusters_arr_length - 1 - occupied_cluster_extra +
                        2]; /* find address for extra cluster */
                free_pseudo_inode -> indirect2 = indirect2_first_list_clust;
                int indirect2_first_list_start_addr = (content_fs->loaded_sb->data_start_address) +
                                                      (indirect2_first_list_clust * FORMAT_CLUSTER_SIZE_B);
                set_pointer_position_fs(indirect2_first_list_start_addr, SEEK_SET);

                write_to_file_fs((void *) free_clusters->free_clusters_arr +
                                 (free_clusters->free_clusters_arr_length - second_list_count) * sizeof(int),
                                 sizeof(int) * second_list_count);
                /* fill first list for indirect1 - END */
            }
        }
        /* assign clusters with data to inode - END */

        /* assign copied file to actual folder - START */
        struct directory_item *file_written = malloc(sizeof(struct directory_item));
        memset(file_written, 0, sizeof(struct directory_item));
        strcpy(file_written->item_name, item_target_arr[item_target_arr_length - 1]);
        file_written->inode = free_pseudo_inode->nodeid;

        int needed_size = actual_pseudo_inode -> file_size + sizeof(struct directory_item);
        int one_cluster_count = FORMAT_CLUSTER_SIZE_B /
                                sizeof(struct directory_item); /* count of directory items which is one cluster able to hold */
        int start_address = -1;
        bool new_cluster_alloc = false; /* will be true if we recently tried to assign a new cluster */
        int free_clus_num = -1; /* will contain number of newly assigned cluster, -1 if no free cluster has been found */

        if (needed_size <=
            (one_cluster_count * sizeof(struct directory_item))) { /* direct 1 has enough space to store */
            start_address = (content_fs->loaded_sb->data_start_address) +
                            (actual_pseudo_inode -> direct1) * FORMAT_CLUSTER_SIZE_B + actual_pseudo_inode->file_size;
        } else if (needed_size <= (one_cluster_count * sizeof(struct directory_item)) * 2) { /* direct2 */
            if (actual_pseudo_inode->direct2 == -1) { /* cluster was not reserved yet */
                new_cluster_alloc = true;
                free_clus_num = get_free_cluster();
                actual_pseudo_inode->direct2 = free_clus_num;
            }
            start_address = (content_fs->loaded_sb->data_start_address) +
                            (actual_pseudo_inode->direct2) * FORMAT_CLUSTER_SIZE_B + (actual_pseudo_inode->file_size) -
                            (one_cluster_count * sizeof(struct directory_item));
        } else if (needed_size <= (one_cluster_count * sizeof(struct directory_item)) * 3) { /* direct3 */
            if (actual_pseudo_inode->direct3 == -1) { /* cluster was not reserved yet */
                new_cluster_alloc = true;
                free_clus_num = get_free_cluster();
                actual_pseudo_inode->direct3 = free_clus_num;
            }
            start_address = (content_fs->loaded_sb->data_start_address) +
                            (actual_pseudo_inode->direct3) * FORMAT_CLUSTER_SIZE_B + (actual_pseudo_inode->file_size) -
                            2 * (one_cluster_count * sizeof(struct directory_item));
        } else if (needed_size <= (one_cluster_count * sizeof(struct directory_item)) * 4) { /* direct4 */
            if (actual_pseudo_inode->direct4 == -1) { /* cluster was not reserved yet */
                new_cluster_alloc = true;
                free_clus_num = get_free_cluster();
                actual_pseudo_inode->direct4 = free_clus_num;
            }
            start_address = (content_fs->loaded_sb->data_start_address) +
                            (actual_pseudo_inode->direct4) * FORMAT_CLUSTER_SIZE_B + (actual_pseudo_inode->file_size) -
                            3 * (one_cluster_count * sizeof(struct directory_item));;
        } else if (needed_size <= (one_cluster_count * sizeof(struct directory_item)) * 5) { /* direct5 */
            if (actual_pseudo_inode->direct5 == -1) { /* cluster was not reserved yet */
                new_cluster_alloc = true;
                free_clus_num = get_free_cluster();
                actual_pseudo_inode->direct5 = free_clus_num;
            }
            start_address = (content_fs->loaded_sb->data_start_address) +
                            (actual_pseudo_inode->direct5) * FORMAT_CLUSTER_SIZE_B + (actual_pseudo_inode->file_size) -
                            4 * (one_cluster_count * sizeof(struct directory_item));
        }

        if (new_cluster_alloc == true) { /* no free cluster found */
            if (free_clus_num == -1) {
                printf(CP_NO_CLUSTER);
                return;
            } else { /* update bitmap with cluster state */
                content_fs->loaded_bit_clus->cluster_occupied_total++;
                content_fs->loaded_bit_clus->cluster_free_total--;
                content_fs->loaded_bit_clus->cluster_bitmap[free_clus_num] = 1;
                save_bitmap_clusters(content_fs->loaded_bit_clus);
            }
        }

        set_pointer_position_fs(start_address, SEEK_SET);
        write_to_file_fs(file_written, sizeof(struct directory_item));
        free(file_written);

        /* update file size of folder */
        pseudo_inode_target -> file_size = actual_pseudo_inode -> file_size + sizeof(struct directory_item);

        save_pseudo_inodes(content_fs->loaded_pseudo_in_arr, (content_fs->loaded_bit_in->inodes_free_total +
                                                              content_fs->loaded_bit_in->inodes_occupied_total),
                           (content_fs->loaded_bit_clus->cluster_free_total +
                            content_fs->loaded_bit_clus->cluster_occupied_total),
                           (content_fs->loaded_bit_in->inodes_free_total +
                            content_fs->loaded_bit_in->inodes_occupied_total));
        /* assign copied file to actual folder - END */

        /* update bitmap with cluster state */
        i = 0;
        for (; i < free_clusters->free_clusters_arr_length; i++) {
            content_fs->loaded_bit_clus->cluster_occupied_total++;
            content_fs->loaded_bit_clus->cluster_free_total--;
            content_fs->loaded_bit_clus->cluster_bitmap[free_clusters->free_clusters_arr[i]] = 1;
        }
        save_bitmap_clusters(content_fs->loaded_bit_clus);

        /* update bitmap with inode state */
        content_fs->loaded_bit_in->inodes_occupied_total++;
        content_fs->loaded_bit_in->inodes_free_total--;
        content_fs->loaded_bit_in->inodes_bitmap[free_pseudo_inode_number] = 1;
        save_bitmap_inodes(content_fs->loaded_bit_in, (content_fs->loaded_bit_clus->cluster_occupied_total +
                                                       content_fs->loaded_bit_clus->cluster_free_total));

        free(free_clusters -> free_clusters_arr);
        free(free_clusters);
    }
    close_file_fs();

    if(item_source_arr_length != 0){
        free(item_source_arr);
    }
    if(item_target_arr_length != 0){
        free(item_target_arr);
    }

    /* go back to previously traversed inode */
    actual_pseudo_inode = actual_pseudo_inode_backup;
}

/* ____________________________________________________________________________
    void perform_mv(char *first_word, char *second_word)

    Performs command mv (syntax: mv f1 f2), which is used for moving
    file f1 to location f2.
   ____________________________________________________________________________
*/
void perform_mv(char *first_word, char *second_word){
    struct pseudo_inode *actual_pseudo_inode_backup = actual_pseudo_inode; /* we will switch inodes */

    struct pseudo_inode *pseudo_inode_source = NULL; /* inode of folder from which the file will be copied */
    struct pseudo_inode *pseudo_inode_target = NULL; /* inode of folder to which the file will be copied */

    char (*item_source_arr)[15] = NULL; /* array of item names which are in the source path */
    int item_source_arr_length = 0;
    char (*item_target_arr)[15] = NULL; /* array of item names which are in the path of target */
    int item_target_arr_length = 0;
    char *first_word_backup = malloc(sizeof(char) * strlen(first_word) + 1);
    strcpy(first_word_backup, first_word);

    bool error_occured = false; /* if true, then error occured (not enough clusters, etc.) -> file cannot be copied */

    char *one_word = NULL;
    /* split source path by '/' to get individual folders - START */
    one_word = strtok(first_word, "/");
    while(one_word != NULL) {
        item_source_arr = realloc(item_source_arr, (item_source_arr_length + 1) * sizeof *item_source_arr);
        strcpy(item_source_arr[item_source_arr_length], one_word);
        one_word = strtok(NULL, "/"); /* move to next word */

        item_source_arr_length += 1;
    }
    /* split source path by '/' to get individual folders - END */

    /* split target path by '/' to get individual folders - START */
    one_word = strtok(second_word, "/");
    while(one_word != NULL) {
        item_target_arr = realloc(item_target_arr, (item_target_arr_length + 1) * sizeof *item_target_arr);
        strcpy(item_target_arr[item_target_arr_length], one_word);
        one_word = strtok(NULL, "/"); /* move to next word */

        item_target_arr_length += 1;
    }
    /* split target path by '/' to get individual folders - END */

    /* find inode for source path - START */
    if(first_word[0] == '/'){ /* path is absolute, load first inode and continue traversing */
        actual_pseudo_inode = &content_fs -> loaded_pseudo_in_arr[0];

        int x = 0;
        for(; x < item_source_arr_length; x++){
            open_rb_fs();
            int numb_it_fol_untraver = (actual_pseudo_inode -> file_size) / sizeof(struct directory_item); /* number of untraversed items in folder */
            struct directory_item *directory_items = malloc(sizeof(struct directory_item) * numb_it_fol_untraver);
            int free_index_directory_items = 0;
            int array_data_blocks[5];

            /* fill array of data block numbers for better traversing */
            array_data_blocks[0] = actual_pseudo_inode -> direct1;
            array_data_blocks[1] = actual_pseudo_inode -> direct2;
            array_data_blocks[2] = actual_pseudo_inode -> direct3;
            array_data_blocks[3] = actual_pseudo_inode -> direct4;
            array_data_blocks[4] = actual_pseudo_inode -> direct5;

            int i = 0;
            for(; i < 5; i++){ /* read direct */
                if(numb_it_fol_untraver != 0){
                    int read_count = -1;
                    int j = 0;

                    if(numb_it_fol_untraver * sizeof(struct directory_item) <= FORMAT_CLUSTER_SIZE_B){ /* read just part of a cluster */
                        read_count = numb_it_fol_untraver;
                    }else{ /* read whole cluster */
                        read_count = FORMAT_CLUSTER_SIZE_B / sizeof(struct directory_item);
                    }

                    for(; j < read_count; j++){
                        int start_address = content_fs -> loaded_sb -> data_start_address + array_data_blocks[i] * FORMAT_CLUSTER_SIZE_B + (j * sizeof(struct directory_item));
                        set_pointer_position_fs(start_address, SEEK_SET);
                        struct directory_item *dir_item = (struct directory_item *) read_from_file_fs(sizeof(struct directory_item));
                        directory_items[free_index_directory_items] = *dir_item;
                        free(dir_item);
                        free_index_directory_items++;
                        numb_it_fol_untraver--;
                    }
                }
            }
            close_file_fs();

            /* all directory items loaded, does wanted folder exist? */
            i = 0;
            int new_inode_number = -1;
            for(; i < free_index_directory_items; i++){
                if(strcmp(directory_items[i].item_name, item_source_arr[x]) == 0){ /* inode with desired name has been found */
                    new_inode_number = directory_items[i].inode;
                    break;
                }
            }

            if(new_inode_number != -1){
                if(x == item_source_arr_length - 1){ /* inode on last position should represent file */
                    if(content_fs -> loaded_pseudo_in_arr[new_inode_number].isDirectory == false){
                        pseudo_inode_source = &content_fs -> loaded_pseudo_in_arr[new_inode_number];
                    }else{
                        printf(FILE_NOT_FOUND);
                        error_occured = true;
                        free(directory_items);
                        break;
                    }
                }else{ /* other inodes should represent folder */
                    if(content_fs -> loaded_pseudo_in_arr[new_inode_number].isDirectory == true){
                        actual_pseudo_inode = &content_fs -> loaded_pseudo_in_arr[new_inode_number];
                    }else{
                        printf(FILE_NOT_FOUND);
                        error_occured = true;
                        free(directory_items);
                        break;
                    }
                }
            }else{
                printf(FILE_NOT_FOUND);
                error_occured = true;
                free(directory_items);
                break;
            }
            free(directory_items);
        }
    }else{ /* path is relative, go from current inode and try to traverse */
        actual_pseudo_inode = actual_pseudo_inode_backup;

        int x = 0;
        for(; x < item_source_arr_length; x++){
            open_rb_fs();
            int numb_it_fol_untraver = (actual_pseudo_inode -> file_size) / sizeof(struct directory_item); /* number of untraversed items in folder */
            struct directory_item *directory_items = malloc(sizeof(struct directory_item) * numb_it_fol_untraver);
            int free_index_directory_items = 0;
            int array_data_blocks[5];

            /* fill array of data block numbers for better traversing */
            array_data_blocks[0] = actual_pseudo_inode -> direct1;
            array_data_blocks[1] = actual_pseudo_inode -> direct2;
            array_data_blocks[2] = actual_pseudo_inode -> direct3;
            array_data_blocks[3] = actual_pseudo_inode -> direct4;
            array_data_blocks[4] = actual_pseudo_inode -> direct5;

            int i = 0;
            for(; i < 5; i++){ /* read direct */
                if(numb_it_fol_untraver != 0){
                    int read_count = -1;
                    int j = 0;

                    if(numb_it_fol_untraver * sizeof(struct directory_item) <= FORMAT_CLUSTER_SIZE_B){ /* read just part of a cluster */
                        read_count = numb_it_fol_untraver;
                    }else{ /* read whole cluster */
                        read_count = FORMAT_CLUSTER_SIZE_B / sizeof(struct directory_item);
                    }

                    for(; j < read_count; j++){
                        int start_address = content_fs -> loaded_sb -> data_start_address + array_data_blocks[i] * FORMAT_CLUSTER_SIZE_B + (j * sizeof(struct directory_item));
                        set_pointer_position_fs(start_address, SEEK_SET);
                        struct directory_item *dir_item = (struct directory_item *) read_from_file_fs(sizeof(struct directory_item));
                        directory_items[free_index_directory_items] = *dir_item;
                        free(dir_item);
                        free_index_directory_items++;
                        numb_it_fol_untraver--;
                    }
                }
            }
            close_file_fs();

            /* all directory items loaded, does wanted folder exist? */
            int new_inode_number = -1;
            bool in_root_upper = false;

            if(strlen(item_source_arr[x]) == 1 && item_source_arr[x][0] == '.'){
                new_inode_number = directory_items[0].inode; /* first directory_item in folder is the folder itself */
            }else if(strlen(item_source_arr[x]) == 2 && item_source_arr[x][0] == '.' && item_source_arr[x][1] == '.'){
                if(directory_items[0].item_name[0] == '/'){ /* user is in root, cannot move upper */
                    in_root_upper = true;
                    error_occured = true;
                }else{
                    new_inode_number = directory_items[1].inode; /* second directory_item in folder is the upper folder */
                }
            }else{
                i = 0;
                for(; i < free_index_directory_items; i++){
                    if(strcmp(directory_items[i].item_name, item_source_arr[x]) == 0){ /* inode with desired name has been found */
                        new_inode_number = directory_items[i].inode;
                        break;
                    }
                }
            }

            if(new_inode_number != -1){
                if(x == item_source_arr_length - 1){ /* inode on last position should represent file */
                    if(content_fs -> loaded_pseudo_in_arr[new_inode_number].isDirectory == false){
                        pseudo_inode_source = &content_fs -> loaded_pseudo_in_arr[new_inode_number];
                    }else{
                        printf(FILE_NOT_FOUND);
                        error_occured = true;
                        free(directory_items);
                        break;
                    }
                }else{ /* other inodes should represent folder */
                    if(content_fs -> loaded_pseudo_in_arr[new_inode_number].isDirectory == true){
                        actual_pseudo_inode = &content_fs -> loaded_pseudo_in_arr[new_inode_number];
                    }else{
                        printf(FILE_NOT_FOUND);
                        error_occured = true;
                        free(directory_items);
                        break;
                    }
                }
            }else{
                if(in_root_upper == true){
                    printf(ROOT_FS_SEARCH, item_source_arr[x]);
                }else{
                    printf(FILE_NOT_FOUND);
                }
                free(directory_items);
                break;
            }
            free(directory_items);
        }
    }
    /* find inode for source path - END */

    /* find inode for target path - START */
    if(second_word[0] == '/'){ /* path is absolute, load first inode and continue traversing */
        actual_pseudo_inode = &content_fs -> loaded_pseudo_in_arr[0];

        int x = 0;
        for(; x < item_target_arr_length - 1; x++){ /* -1 because last word should be name of the file which is not yet created */
            open_rb_fs();
            int numb_it_fol_untraver = (actual_pseudo_inode -> file_size) / sizeof(struct directory_item); /* number of untraversed items in folder */
            struct directory_item *directory_items = malloc(sizeof(struct directory_item) * numb_it_fol_untraver);
            int free_index_directory_items = 0;
            int array_data_blocks[5];

            /* fill array of data block numbers for better traversing */
            array_data_blocks[0] = actual_pseudo_inode -> direct1;
            array_data_blocks[1] = actual_pseudo_inode -> direct2;
            array_data_blocks[2] = actual_pseudo_inode -> direct3;
            array_data_blocks[3] = actual_pseudo_inode -> direct4;
            array_data_blocks[4] = actual_pseudo_inode -> direct5;

            int i = 0;
            for(; i < 5; i++){ /* read direct */
                if(numb_it_fol_untraver != 0){
                    int read_count = -1;
                    int j = 0;

                    if(numb_it_fol_untraver * sizeof(struct directory_item) <= FORMAT_CLUSTER_SIZE_B){ /* read just part of a cluster */
                        read_count = numb_it_fol_untraver;
                    }else{ /* read whole cluster */
                        read_count = FORMAT_CLUSTER_SIZE_B / sizeof(struct directory_item);
                    }

                    for(; j < read_count; j++){
                        int start_address = content_fs -> loaded_sb -> data_start_address + array_data_blocks[i] * FORMAT_CLUSTER_SIZE_B + (j * sizeof(struct directory_item));
                        set_pointer_position_fs(start_address, SEEK_SET);
                        struct directory_item *dir_item = (struct directory_item *) read_from_file_fs(sizeof(struct directory_item));
                        directory_items[free_index_directory_items] = *dir_item;
                        free(dir_item);
                        free_index_directory_items++;
                        numb_it_fol_untraver--;
                    }
                }
            }
            close_file_fs();

            /* all directory items loaded, does wanted folder exist? */
            i = 0;
            int new_inode_number = -1;
            for(; i < free_index_directory_items; i++){
                if(strcmp(directory_items[i].item_name, item_target_arr[x]) == 0){ /* inode with desired name has been found */
                    new_inode_number = directory_items[i].inode;
                    break;
                }
            }

            if(new_inode_number != -1){
                if(content_fs -> loaded_pseudo_in_arr[new_inode_number].isDirectory == true){
                    if(x == item_target_arr_length - 2){
                        pseudo_inode_target = &content_fs -> loaded_pseudo_in_arr[new_inode_number];
                    }else{
                        actual_pseudo_inode = &content_fs -> loaded_pseudo_in_arr[new_inode_number];
                    }
                }else{
                    printf(FILE_NOT_FOUND);
                    error_occured = true;
                    free(directory_items);
                    break;
                }
            }else{
                printf(FILE_NOT_FOUND);
                error_occured = true;
                free(directory_items);
                break;
            }
            free(directory_items);
        }
    }else{ /* path is relative, go from current inode and try to traverse */
        actual_pseudo_inode = actual_pseudo_inode_backup;

        int x = 0;
        for(; x < item_target_arr_length - 1; x++){
            open_rb_fs();
            int numb_it_fol_untraver = (actual_pseudo_inode -> file_size) / sizeof(struct directory_item); /* number of untraversed items in folder */
            struct directory_item *directory_items = malloc(sizeof(struct directory_item) * numb_it_fol_untraver);
            int free_index_directory_items = 0;
            int array_data_blocks[5];

            /* fill array of data block numbers for better traversing */
            array_data_blocks[0] = actual_pseudo_inode -> direct1;
            array_data_blocks[1] = actual_pseudo_inode -> direct2;
            array_data_blocks[2] = actual_pseudo_inode -> direct3;
            array_data_blocks[3] = actual_pseudo_inode -> direct4;
            array_data_blocks[4] = actual_pseudo_inode -> direct5;

            int i = 0;
            for(; i < 5; i++){ /* read direct */
                if(numb_it_fol_untraver != 0){
                    int read_count = -1;
                    int j = 0;

                    if(numb_it_fol_untraver * sizeof(struct directory_item) <= FORMAT_CLUSTER_SIZE_B){ /* read just part of a cluster */
                        read_count = numb_it_fol_untraver;
                    }else{ /* read whole cluster */
                        read_count = FORMAT_CLUSTER_SIZE_B / sizeof(struct directory_item);
                    }

                    for(; j < read_count; j++){
                        int start_address = content_fs -> loaded_sb -> data_start_address + array_data_blocks[i] * FORMAT_CLUSTER_SIZE_B + (j * sizeof(struct directory_item));
                        set_pointer_position_fs(start_address, SEEK_SET);
                        struct directory_item *dir_item = (struct directory_item *) read_from_file_fs(sizeof(struct directory_item));
                        directory_items[free_index_directory_items] = *dir_item;
                        free(dir_item);
                        free_index_directory_items++;
                        numb_it_fol_untraver--;
                    }
                }
            }
            close_file_fs();

            /* all directory items loaded, does wanted folder exist? */
            int new_inode_number = -1;
            bool in_root_upper = false;

            if(strlen(item_target_arr[x]) == 1 && item_target_arr[x][0] == '.'){
                new_inode_number = directory_items[0].inode; /* first directory_item in folder is the folder itself */
            }else if(strlen(item_target_arr[x]) == 2 && item_target_arr[x][0] == '.' && item_target_arr[x][1] == '.'){
                if(directory_items[0].item_name[0] == '/'){ /* user is in root, cannot move upper */
                    in_root_upper = true;
                    error_occured = true;
                }else{
                    new_inode_number = directory_items[1].inode; /* second directory_item in folder is the upper folder */
                }
            }else{
                i = 0;
                for(; i < free_index_directory_items; i++){
                    if(strcmp(directory_items[i].item_name, item_target_arr[x]) == 0){ /* inode with desired name has been found */
                        new_inode_number = directory_items[i].inode;
                        break;
                    }
                }
            }

            if(new_inode_number != -1){
                if(content_fs -> loaded_pseudo_in_arr[new_inode_number].isDirectory == true){
                    if(x == item_target_arr_length - 2){
                        pseudo_inode_target = &content_fs -> loaded_pseudo_in_arr[new_inode_number];
                    }else{
                        actual_pseudo_inode = &content_fs -> loaded_pseudo_in_arr[new_inode_number];
                    }
                }else{
                    printf(FILE_NOT_FOUND);
                    error_occured = true;
                    free(directory_items);
                    break;
                }
            }else{
                if(in_root_upper == true){
                    printf(ROOT_FS_SEARCH, item_target_arr[x]);
                }else{
                    printf(FILE_NOT_FOUND);
                }
                free(directory_items);
                break;
            }
            free(directory_items);
        }
    }
    /* find inode for target path - END */

    if(pseudo_inode_source == NULL || pseudo_inode_target == NULL){
        if(item_source_arr_length != 0){
            free(item_source_arr);
        }

        if(item_target_arr_length != 0){
            free(item_target_arr);
        }

        printf(PROVIDE_VALID_PATH);
        return;
    }

    open_wb_add_fs();

    /* get free inode */
    struct pseudo_inode *free_pseudo_inode = NULL;
    int free_pseudo_inode_number = -1;
    int total_inode_count =
            (content_fs->loaded_bit_in->inodes_free_total) + (content_fs->loaded_bit_in->inodes_occupied_total);

    int i = 0;
    for (; i < total_inode_count; i++) {
        if (content_fs->loaded_bit_in->inodes_bitmap[i] == 0) { /* free inode on "i" position found */
            free_pseudo_inode = &content_fs->loaded_pseudo_in_arr[i]; /* (index is the same as with bitmap) */
            free_pseudo_inode_number = i;
            break;
        }
    }

    /* no free inode found */
    if(free_pseudo_inode_number == -1){
        printf(CP_NO_INODE);
        error_occured = true;
    }

    free_pseudo_inode -> file_size = pseudo_inode_source -> file_size;

    int cluster_inode[7];
    cluster_inode[0] = pseudo_inode_source -> direct1;
    cluster_inode[1] = pseudo_inode_source -> direct2;
    cluster_inode[2] = pseudo_inode_source -> direct3;
    cluster_inode[3] = pseudo_inode_source -> direct4;
    cluster_inode[4] = pseudo_inode_source -> direct5;
    cluster_inode[5] = pseudo_inode_source -> indirect1;
    cluster_inode[6] = pseudo_inode_source -> indirect2;

    int file_size = pseudo_inode_source->file_size;

    int occupied_cluster_data = ceil(
            (float) file_size / FORMAT_CLUSTER_SIZE_B); /* number of clusters reserved for data */
    int occupied_cluster_extra = 0; /* number of extra clusters (for indirect pointer list etc.) */
    /* extra clusters will be needed if indirect is going to be used */
    int indirect1_hold = FORMAT_CLUSTER_SIZE_B / sizeof(int);

    /* count extra clusters - START */
    if(occupied_cluster_data > 5){ /* direct pointers not enough, extra cluster for indirect1 */
        occupied_cluster_extra += 1;

        if (occupied_cluster_data > (5 + indirect1_hold)) { /* indirect2 */
            occupied_cluster_extra += 1; /* for first cluster list */

            int second_list_count = ceil((float) (occupied_cluster_data - (5 + indirect1_hold)) /
                                         indirect1_hold); /* needed cluster count / list capacity */
            occupied_cluster_extra += second_list_count;
        }
    }
    /* count extra clusters - END */

    /* get array of clusters */
    struct free_clusters_struct *free_clusters = get_free_cluster_arr(
            occupied_cluster_data + occupied_cluster_extra);

    /* not enough clusters */
    if(free_clusters == NULL){
        printf(CP_NO_CLUSTER);
        error_occured = true;
    }

    if(error_occured == false){
        /* set target inode as actual */
        actual_pseudo_inode = pseudo_inode_target;

        /* read cluster & write - START */
        int address_offset = 0;

        /* direct - START */
        int end;
        if (occupied_cluster_data < 5) {
            end = occupied_cluster_data;
        } else {
            end = 5;
        }

        int cluster_data_counter_cp = 0; /* counter for data clusters assigned to new file */
        i = 0;
        for (; i < end; i++) {
            int start_address = content_fs->loaded_sb->data_start_address + cluster_inode[i] * FORMAT_CLUSTER_SIZE_B;
            set_pointer_position_fs(start_address, SEEK_SET);
            int write_start_addr = content_fs->loaded_sb->data_start_address +
                                   free_clusters->free_clusters_arr[cluster_data_counter_cp] * FORMAT_CLUSTER_SIZE_B;
            void *data;

            if (end < 5 && (i == end - 1)) { /* reading last cluster */
                int size_to_read = file_size - (i * FORMAT_CLUSTER_SIZE_B);
                data = read_from_file_fs(size_to_read); /* remaining size */
                set_pointer_position_fs(write_start_addr, SEEK_SET);
                write_to_file_fs(data, size_to_read);
                address_offset += size_to_read;
            } else {
                data = read_from_file_fs(FORMAT_CLUSTER_SIZE_B);
                set_pointer_position_fs(write_start_addr, SEEK_SET);
                write_to_file_fs(data, FORMAT_CLUSTER_SIZE_B);
                address_offset += FORMAT_CLUSTER_SIZE_B;
            }

            cluster_data_counter_cp += 1;

            free(data);
        }
        /* direct - END */

        /* read indirect */
        if(occupied_cluster_extra > 0){ /* first extra cluster is a list for indirect1 */
            int loop_end;
            if ((occupied_cluster_data - 5) > indirect1_hold) {
                loop_end = indirect1_hold;
            } else {
                loop_end = occupied_cluster_data - 5;
            }
            int start_address = content_fs->loaded_sb->data_start_address + cluster_inode[5] * FORMAT_CLUSTER_SIZE_B;
            set_pointer_position_fs(start_address, SEEK_SET);
            int *clust_indir1_arr = (int *) read_from_file_fs(loop_end * sizeof(int));
            i = 0;
            for (; i < loop_end; i++) {
                int start_address =
                        content_fs->loaded_sb->data_start_address + clust_indir1_arr[i] * FORMAT_CLUSTER_SIZE_B;
                set_pointer_position_fs(start_address, SEEK_SET);
                int write_start_addr = content_fs->loaded_sb->data_start_address +
                                       free_clusters->free_clusters_arr[cluster_data_counter_cp] *
                                       FORMAT_CLUSTER_SIZE_B;
                void *data;
                if (loop_end < indirect1_hold && (i == loop_end - 1)) { /* reading last cluster */
                    int size_to_read = file_size - ((i + 5) * FORMAT_CLUSTER_SIZE_B);
                    data = read_from_file_fs(size_to_read); /* remaining size */
                    set_pointer_position_fs(write_start_addr, SEEK_SET);
                    write_to_file_fs(data, size_to_read);
                    address_offset += size_to_read;
                } else {
                    data = read_from_file_fs(FORMAT_CLUSTER_SIZE_B);
                    set_pointer_position_fs(write_start_addr, SEEK_SET);
                    write_to_file_fs(data, FORMAT_CLUSTER_SIZE_B);
                    address_offset += FORMAT_CLUSTER_SIZE_B;
                }

                cluster_data_counter_cp += 1;
                free(data);
            }
            free(clust_indir1_arr);
            if (occupied_cluster_extra > 1) { /* second extra cluster is a first list for indirect2 */
                int second_list_count = ceil((float) (occupied_cluster_data - 5 - indirect1_hold) / indirect1_hold);
                int start_address =
                        content_fs->loaded_sb->data_start_address + cluster_inode[6] * FORMAT_CLUSTER_SIZE_B;
                set_pointer_position_fs(start_address, SEEK_SET);
                int *second_list_arr = (int *) read_from_file_fs(sizeof(int) * second_list_count);

                i = 0;
                for (; i < second_list_count; i++) {
                    int start_address_sec_list =
                            content_fs->loaded_sb->data_start_address + second_list_arr[i] * FORMAT_CLUSTER_SIZE_B;
                    set_pointer_position_fs(start_address_sec_list, SEEK_SET);
                    int *clusters_arr;
                    if ((occupied_cluster_data - 5 - indirect1_hold - (indirect1_hold * i)) < indirect1_hold) {
                        loop_end = occupied_cluster_data - 5 - indirect1_hold - (indirect1_hold * i);
                    } else {
                        loop_end = indirect1_hold;
                    }
                    clusters_arr = read_from_file_fs(loop_end * sizeof(int));
                    void *data;
                    int j = 0;
                    for (; j < loop_end; j++) {
                        set_pointer_position_fs(
                                content_fs->loaded_sb->data_start_address + clusters_arr[j] * FORMAT_CLUSTER_SIZE_B,
                                SEEK_SET);
                        int write_start_addr = content_fs->loaded_sb->data_start_address +
                                               free_clusters->free_clusters_arr[cluster_data_counter_cp] *
                                               FORMAT_CLUSTER_SIZE_B;
                        if(loop_end < indirect1_hold && (j == loop_end - 1)){ /* reading last cluster */
                            int size_to_read = file_size - ((5 + indirect1_hold + (indirect1_hold * i) + j) *
                                                            FORMAT_CLUSTER_SIZE_B);
                            data = read_from_file_fs(size_to_read); /* remaining size */
                            set_pointer_position_fs(write_start_addr, SEEK_SET);
                            write_to_file_fs(data, size_to_read);
                            address_offset += size_to_read;
                        }else{
                            data = read_from_file_fs(FORMAT_CLUSTER_SIZE_B);
                            set_pointer_position_fs(write_start_addr, SEEK_SET);
                            write_to_file_fs(data, FORMAT_CLUSTER_SIZE_B);
                            address_offset += FORMAT_CLUSTER_SIZE_B;
                        }
                        cluster_data_counter_cp += 1;

                        free(data);
                    }
                    free(clusters_arr);
                }
                free(second_list_arr);
            }
        }
        /* read cluster & write - END */

        /* assign clusters with data to inode - START */
        /* assign direct */
        int *cluster_direct[5];

        cluster_direct[0] = &free_pseudo_inode->direct1;
        cluster_direct[1] = &free_pseudo_inode->direct2;
        cluster_direct[2] = &free_pseudo_inode->direct3;
        cluster_direct[3] = &free_pseudo_inode->direct4;
        cluster_direct[4] = &free_pseudo_inode->direct5;

        end;
        if (file_size <= (FORMAT_CLUSTER_SIZE_B * 5)) {
            end = ceil((float) file_size / FORMAT_CLUSTER_SIZE_B);
        } else {
            end = 5;
        }

        i = 0;
        for (; i < end; i++) {
            *cluster_direct[i] = free_clusters->free_clusters_arr[i];
        }

        /* assign indirect */
        if (occupied_cluster_extra > 0) { /* first extra cluster is a list for indirect1 */
            int *indirect1_arr = NULL; /* array of clusters which will be saved on indirect1 */
            int rem_data_clus = occupied_cluster_data - 5; /* clusters with data count, not assigned */

            int num_items;
            if (rem_data_clus <
                indirect1_hold) { /* less unassigned than cluster is able to hold => indirect2 will not be used */
                num_items = rem_data_clus;
            } else {
                num_items = indirect1_hold;
            }

            indirect1_arr = malloc(sizeof(int) * num_items);

            i = 0;
            for (; i < num_items; i++) { /* fill indirect1 array */
                indirect1_arr[i] = free_clusters->free_clusters_arr[5 + i]; /* 5*direct */
            }

            /* array filled, now write data to first extra cluster */
            int indirect1_list_clust = free_clusters->free_clusters_arr[free_clusters->free_clusters_arr_length - 1 -
                                                                        occupied_cluster_extra +
                                                                        1]; /* find first extra cluster */
            free_pseudo_inode->indirect1 = indirect1_list_clust;
            int indirect1_list_start_addr =
                    (content_fs->loaded_sb->data_start_address) + (indirect1_list_clust * FORMAT_CLUSTER_SIZE_B);
            set_pointer_position_fs(indirect1_list_start_addr, SEEK_SET);
            write_to_file_fs((void *) indirect1_arr, sizeof(int) * num_items);
            rem_data_clus -= num_items; /* update unassigned cluster count */
            free(indirect1_arr);

            if (occupied_cluster_extra > 1) { /* second extra cluster is a first list for indirect2 */
                /* fill second list for indirect2 - START */
                int second_list_count = ceil(
                        (float) (rem_data_clus) / indirect1_hold); /* needed cluster count / list capacity */

                i = 0;
                for (; i < second_list_count; i++) {
                    int indirect2_sec_list_clust = free_clusters->free_clusters_arr[
                            free_clusters->free_clusters_arr_length - 1 - occupied_cluster_extra + 3 +
                            i]; /* find address for extra cluster */
                    int indirect2_sec_list_start_addr = (content_fs->loaded_sb->data_start_address) +
                                                        (indirect2_sec_list_clust * FORMAT_CLUSTER_SIZE_B);
                    set_pointer_position_fs(indirect2_sec_list_start_addr, SEEK_SET);

                    if (i == (second_list_count - 1)) { /* in case of last cluster write */
                        write_to_file_fs((void *) free_clusters->free_clusters_arr +
                                         (5 + indirect1_hold + (i * indirect1_hold)) *
                                         sizeof(int), rem_data_clus * sizeof(int));
                        rem_data_clus = 0;
                    } else {
                        write_to_file_fs((void *) free_clusters->free_clusters_arr +
                                         (5 + indirect1_hold + (i * indirect1_hold)) *
                                         sizeof(int), indirect1_hold * sizeof(int));
                        rem_data_clus -= indirect1_hold;
                    }
                }
                /* fill second list for indirect2 - END */

                /* fill first list for indirect2 - START */
                int indirect2_first_list_clust = free_clusters->free_clusters_arr[
                        free_clusters->free_clusters_arr_length - 1 - occupied_cluster_extra +
                        2]; /* find address for extra cluster */
                free_pseudo_inode -> indirect2 = indirect2_first_list_clust;
                int indirect2_first_list_start_addr = (content_fs->loaded_sb->data_start_address) +
                                                      (indirect2_first_list_clust * FORMAT_CLUSTER_SIZE_B);
                set_pointer_position_fs(indirect2_first_list_start_addr, SEEK_SET);

                write_to_file_fs((void *) free_clusters->free_clusters_arr +
                                 (free_clusters->free_clusters_arr_length - second_list_count) * sizeof(int),
                                 sizeof(int) * second_list_count);
                /* fill first list for indirect1 - END */
            }
        }
        /* assign clusters with data to inode - END */

        /* assign copied file to actual folder - START */
        struct directory_item *file_written = malloc(sizeof(struct directory_item));
        memset(file_written, 0, sizeof(struct directory_item));
        strcpy(file_written->item_name, item_target_arr[item_target_arr_length - 1]);
        file_written->inode = free_pseudo_inode->nodeid;

        int needed_size = actual_pseudo_inode -> file_size + sizeof(struct directory_item);
        int one_cluster_count = FORMAT_CLUSTER_SIZE_B /
                                sizeof(struct directory_item); /* count of directory items which is one cluster able to hold */
        int start_address = -1;
        bool new_cluster_alloc = false; /* will be true if we recently tried to assign a new cluster */
        int free_clus_num = -1; /* will contain number of newly assigned cluster, -1 if no free cluster has been found */

        if (needed_size <=
            (one_cluster_count * sizeof(struct directory_item))) { /* direct 1 has enough space to store */
            start_address = (content_fs->loaded_sb->data_start_address) +
                            (actual_pseudo_inode -> direct1) * FORMAT_CLUSTER_SIZE_B + actual_pseudo_inode->file_size;
        } else if (needed_size <= (one_cluster_count * sizeof(struct directory_item)) * 2) { /* direct2 */
            if (actual_pseudo_inode->direct2 == -1) { /* cluster was not reserved yet */
                new_cluster_alloc = true;
                free_clus_num = get_free_cluster();
                actual_pseudo_inode->direct2 = free_clus_num;
            }
            start_address = (content_fs->loaded_sb->data_start_address) +
                            (actual_pseudo_inode->direct2) * FORMAT_CLUSTER_SIZE_B + (actual_pseudo_inode->file_size) -
                            (one_cluster_count * sizeof(struct directory_item));
        } else if (needed_size <= (one_cluster_count * sizeof(struct directory_item)) * 3) { /* direct3 */
            if (actual_pseudo_inode->direct3 == -1) { /* cluster was not reserved yet */
                new_cluster_alloc = true;
                free_clus_num = get_free_cluster();
                actual_pseudo_inode->direct3 = free_clus_num;
            }
            start_address = (content_fs->loaded_sb->data_start_address) +
                            (actual_pseudo_inode->direct3) * FORMAT_CLUSTER_SIZE_B + (actual_pseudo_inode->file_size) -
                            2 * (one_cluster_count * sizeof(struct directory_item));
        } else if (needed_size <= (one_cluster_count * sizeof(struct directory_item)) * 4) { /* direct4 */
            if (actual_pseudo_inode->direct4 == -1) { /* cluster was not reserved yet */
                new_cluster_alloc = true;
                free_clus_num = get_free_cluster();
                actual_pseudo_inode->direct4 = free_clus_num;
            }
            start_address = (content_fs->loaded_sb->data_start_address) +
                            (actual_pseudo_inode->direct4) * FORMAT_CLUSTER_SIZE_B + (actual_pseudo_inode->file_size) -
                            3 * (one_cluster_count * sizeof(struct directory_item));;
        } else if (needed_size <= (one_cluster_count * sizeof(struct directory_item)) * 5) { /* direct5 */
            if (actual_pseudo_inode->direct5 == -1) { /* cluster was not reserved yet */
                new_cluster_alloc = true;
                free_clus_num = get_free_cluster();
                actual_pseudo_inode->direct5 = free_clus_num;
            }
            start_address = (content_fs->loaded_sb->data_start_address) +
                            (actual_pseudo_inode->direct5) * FORMAT_CLUSTER_SIZE_B + (actual_pseudo_inode->file_size) -
                            4 * (one_cluster_count * sizeof(struct directory_item));
        }

        if(new_cluster_alloc == true){ /* no free cluster found */
            if (free_clus_num == -1) {
                printf(CP_NO_CLUSTER);
                return;
            } else { /* update bitmap with cluster state */
                content_fs->loaded_bit_clus->cluster_occupied_total++;
                content_fs->loaded_bit_clus->cluster_free_total--;
                content_fs->loaded_bit_clus->cluster_bitmap[free_clus_num] = 1;
                save_bitmap_clusters(content_fs->loaded_bit_clus);
            }
        }

        set_pointer_position_fs(start_address, SEEK_SET);
        write_to_file_fs(file_written, sizeof(struct directory_item));
        free(file_written);

        /* update file size of folder */
        pseudo_inode_target -> file_size = actual_pseudo_inode -> file_size + sizeof(struct directory_item);

        save_pseudo_inodes(content_fs->loaded_pseudo_in_arr, (content_fs->loaded_bit_in->inodes_free_total +
                                                              content_fs->loaded_bit_in->inodes_occupied_total),
                           (content_fs->loaded_bit_clus->cluster_free_total +
                            content_fs->loaded_bit_clus->cluster_occupied_total),
                           (content_fs->loaded_bit_in->inodes_free_total +
                            content_fs->loaded_bit_in->inodes_occupied_total));
        /* assign copied file to actual folder - END */

        /* update bitmap with cluster state */
        i = 0;
        for (; i < free_clusters->free_clusters_arr_length; i++) {
            content_fs->loaded_bit_clus->cluster_occupied_total++;
            content_fs->loaded_bit_clus->cluster_free_total--;
            content_fs->loaded_bit_clus->cluster_bitmap[free_clusters->free_clusters_arr[i]] = 1;
        }
        save_bitmap_clusters(content_fs->loaded_bit_clus);

        /* update bitmap with inode state */
        content_fs->loaded_bit_in->inodes_occupied_total++;
        content_fs->loaded_bit_in->inodes_free_total--;
        content_fs->loaded_bit_in->inodes_bitmap[free_pseudo_inode_number] = 1;
        save_bitmap_inodes(content_fs->loaded_bit_in, (content_fs->loaded_bit_clus->cluster_occupied_total +
                                                       content_fs->loaded_bit_clus->cluster_free_total));

        free(free_clusters -> free_clusters_arr);
        free(free_clusters);
    }
    close_file_fs();

    if(item_source_arr_length != 0){
        free(item_source_arr);
    }
    if(item_target_arr_length != 0){
        free(item_target_arr);
    }

    /* remove original file */
    actual_pseudo_inode = actual_pseudo_inode_backup;
    perform_rm(first_word_backup);
    free(first_word_backup);

    /* go back to previously traversed inode */
    actual_pseudo_inode = actual_pseudo_inode_backup;
}
/* ____________________________________________________________________________
    void perform_rm(char *first_word)

    Performs command rm (syntax: rm f1), which is used for removing
    file f1.
   ____________________________________________________________________________
*/
void perform_rm(char *first_word){
    struct pseudo_inode *actual_pseudo_inode_backup = actual_pseudo_inode; /* we will switch inodes */
    struct pseudo_inode *pseudo_inode_target = NULL; /* inode of folder from which the folder will be deleted */
    char (*item_target_arr)[15] = NULL;
    int item_target_arr_length = 0;

    bool error_occured = false; /* if true, then error occured (wrong file name etc.) */

    char *one_word = NULL;
    /* split target path by '/' to get individual folders - START */
    one_word = strtok(first_word, "/");
    while(one_word != NULL){
        /* check length of folder name */
        if(strlen(one_word) > 14){
            printf(RMDIR_EXCEED_LENGTH);
            if(item_target_arr_length != 0){
                free(item_target_arr);
            }
            return;
        }

        item_target_arr = realloc(item_target_arr, (item_target_arr_length + 1) * sizeof *item_target_arr);
        strcpy(item_target_arr[item_target_arr_length], one_word);
        one_word = strtok(NULL, "/"); /* move to next word */

        item_target_arr_length += 1;
    }
    /* split target path by '/' to get individual folders - END */

    /* find inode for target path - START */
    if(first_word[0] == '/'){ /* path is absolute, load first inode and continue traversing */
        if(item_target_arr_length == 1){ /* delete folder in root of FS */
            pseudo_inode_target = actual_pseudo_inode_backup;
        }else{
            actual_pseudo_inode = &content_fs -> loaded_pseudo_in_arr[0];

            int x = 0;
            for(; x < item_target_arr_length - 1; x++){
                open_rb_fs();
                int numb_it_fol_untraver = (actual_pseudo_inode -> file_size) / sizeof(struct directory_item); /* number of untraversed items in folder */
                struct directory_item *directory_items = malloc(sizeof(struct directory_item) * numb_it_fol_untraver);
                int free_index_directory_items = 0;
                int array_data_blocks[5];

                /* fill array of data block numbers for better traversing */
                array_data_blocks[0] = actual_pseudo_inode -> direct1;
                array_data_blocks[1] = actual_pseudo_inode -> direct2;
                array_data_blocks[2] = actual_pseudo_inode -> direct3;
                array_data_blocks[3] = actual_pseudo_inode -> direct4;
                array_data_blocks[4] = actual_pseudo_inode -> direct5;

                int i = 0;
                for(; i < 5; i++){ /* read direct */
                    if(numb_it_fol_untraver != 0){
                        int read_count = -1;
                        int j = 0;

                        if(numb_it_fol_untraver * sizeof(struct directory_item) <= FORMAT_CLUSTER_SIZE_B){ /* read just part of a cluster */
                            read_count = numb_it_fol_untraver;
                        }else{ /* read whole cluster */
                            read_count = FORMAT_CLUSTER_SIZE_B / sizeof(struct directory_item);
                        }

                        for(; j < read_count; j++){
                            int start_address = content_fs -> loaded_sb -> data_start_address + array_data_blocks[i] * FORMAT_CLUSTER_SIZE_B + (j * sizeof(struct directory_item));
                            set_pointer_position_fs(start_address, SEEK_SET);
                            struct directory_item *dir_item = (struct directory_item *) read_from_file_fs(sizeof(struct directory_item));
                            directory_items[free_index_directory_items] = *dir_item;
                            free(dir_item);
                            free_index_directory_items++;
                            numb_it_fol_untraver--;
                        }
                    }
                }
                close_file_fs();

                /* all directory items loaded, does wanted folder exist? */
                i = 0;
                int new_inode_number = -1;
                for(; i < free_index_directory_items; i++){
                    if(strcmp(directory_items[i].item_name, item_target_arr[x]) == 0){ /* inode with desired name has been found */
                        new_inode_number = directory_items[i].inode;
                        break;
                    }
                }

                if(new_inode_number != -1){
                    if(content_fs -> loaded_pseudo_in_arr[new_inode_number].isDirectory == true){
                        if(x == item_target_arr_length - 2){
                            pseudo_inode_target = &content_fs -> loaded_pseudo_in_arr[new_inode_number];
                        }else{
                            actual_pseudo_inode = &content_fs -> loaded_pseudo_in_arr[new_inode_number];
                        }
                    }else{
                        printf(FILE_NOT_FOUND);
                        error_occured = true;
                        free(directory_items);
                        break;
                    }
                }else{
                    printf(FILE_NOT_FOUND);
                    error_occured = true;
                    free(directory_items);
                    break;
                }
                free(directory_items);
            }
        }
    }else{ /* path is relative, go from current inode and try to traverse */
        if(item_target_arr_length == 1){
            pseudo_inode_target = actual_pseudo_inode_backup;
        }else{
            actual_pseudo_inode = actual_pseudo_inode_backup;

            int x = 0;
            for(; x < item_target_arr_length - 1; x++){
                open_rb_fs();
                int numb_it_fol_untraver = (actual_pseudo_inode -> file_size) / sizeof(struct directory_item); /* number of untraversed items in folder */
                struct directory_item *directory_items = malloc(sizeof(struct directory_item) * numb_it_fol_untraver);
                int free_index_directory_items = 0;
                int array_data_blocks[5];

                /* fill array of data block numbers for better traversing */
                array_data_blocks[0] = actual_pseudo_inode -> direct1;
                array_data_blocks[1] = actual_pseudo_inode -> direct2;
                array_data_blocks[2] = actual_pseudo_inode -> direct3;
                array_data_blocks[3] = actual_pseudo_inode -> direct4;
                array_data_blocks[4] = actual_pseudo_inode -> direct5;

                int i = 0;
                for(; i < 5; i++){ /* read direct */
                    if(numb_it_fol_untraver != 0){
                        int read_count = -1;
                        int j = 0;

                        if(numb_it_fol_untraver * sizeof(struct directory_item) <= FORMAT_CLUSTER_SIZE_B){ /* read just part of a cluster */
                            read_count = numb_it_fol_untraver;
                        }else{ /* read whole cluster */
                            read_count = FORMAT_CLUSTER_SIZE_B / sizeof(struct directory_item);
                        }

                        for(; j < read_count; j++){
                            int start_address = content_fs -> loaded_sb -> data_start_address + array_data_blocks[i] * FORMAT_CLUSTER_SIZE_B + (j * sizeof(struct directory_item));
                            set_pointer_position_fs(start_address, SEEK_SET);
                            struct directory_item *dir_item = (struct directory_item *) read_from_file_fs(sizeof(struct directory_item));
                            directory_items[free_index_directory_items] = *dir_item;
                            free(dir_item);
                            free_index_directory_items++;
                            numb_it_fol_untraver--;
                        }
                    }
                }
                close_file_fs();

                /* all directory items loaded, does wanted folder exist? */
                int new_inode_number = -1;
                bool in_root_upper = false;

                if(strlen(item_target_arr[x]) == 1 && item_target_arr[x][0] == '.'){
                    new_inode_number = directory_items[0].inode; /* first directory_item in folder is the folder itself */
                }else if(strlen(item_target_arr[x]) == 2 && item_target_arr[x][0] == '.' && item_target_arr[x][1] == '.'){
                    if(directory_items[0].item_name[0] == '/'){ /* user is in root, cannot move upper */
                        in_root_upper = true;
                        error_occured = true;
                    }else{
                        new_inode_number = directory_items[1].inode; /* second directory_item in folder is the upper folder */
                    }
                }else{
                    i = 0;
                    for(; i < free_index_directory_items; i++){
                        if(strcmp(directory_items[i].item_name, item_target_arr[x]) == 0){ /* inode with desired name has been found */
                            new_inode_number = directory_items[i].inode;
                            break;
                        }
                    }
                }

                if(new_inode_number != -1){
                    if(content_fs -> loaded_pseudo_in_arr[new_inode_number].isDirectory == true){
                        if(x == item_target_arr_length - 2){
                            pseudo_inode_target = &content_fs -> loaded_pseudo_in_arr[new_inode_number];
                        }else{
                            actual_pseudo_inode = &content_fs -> loaded_pseudo_in_arr[new_inode_number];
                        }
                    }else{
                        printf(FILE_NOT_FOUND);
                        error_occured = true;
                        free(directory_items);
                        break;
                    }
                }else{
                    if(in_root_upper == true){
                        printf(ROOT_FS_SEARCH, item_target_arr[x]);
                    }else{
                        printf(FILE_NOT_FOUND);
                    }
                    free(directory_items);
                    break;
                }
                free(directory_items);
            }
        }
    }
    /* find inode for target path - END */

    if(pseudo_inode_target == NULL){
        return;
    }

    open_wb_add_fs();

    actual_pseudo_inode = pseudo_inode_target;
    int numb_it_fol_untraver = (actual_pseudo_inode -> file_size) / sizeof(struct directory_item); /* number of untraversed items in folder */
    struct directory_item *directory_items = malloc(sizeof(struct directory_item) * numb_it_fol_untraver);
    int free_index_directory_items = 0;
    int array_data_blocks[7];

    /* fill array of data block numbers for better traversing */
    array_data_blocks[0] = actual_pseudo_inode -> direct1;
    array_data_blocks[1] = actual_pseudo_inode -> direct2;
    array_data_blocks[2] = actual_pseudo_inode -> direct3;
    array_data_blocks[3] = actual_pseudo_inode -> direct4;
    array_data_blocks[4] = actual_pseudo_inode -> direct5;

    int i = 0;
    for(; i < 5; i++){ /* read direct */
        if(numb_it_fol_untraver != 0){
            int read_count = -1;
            int j = 0;

            if(numb_it_fol_untraver * sizeof(struct directory_item) <= FORMAT_CLUSTER_SIZE_B){ /* read just part of a cluster */
                read_count = numb_it_fol_untraver;
            }else{ /* read whole cluster */
                read_count = FORMAT_CLUSTER_SIZE_B / sizeof(struct directory_item);
            }

            for(; j < read_count; j++){
                int start_address = content_fs -> loaded_sb -> data_start_address + array_data_blocks[i] * FORMAT_CLUSTER_SIZE_B + (j * sizeof(struct directory_item));
                set_pointer_position_fs(start_address, SEEK_SET);
                struct directory_item *dir_item = (struct directory_item *) read_from_file_fs(sizeof(struct directory_item));
                directory_items[free_index_directory_items] = *dir_item;
                free(dir_item);
                free_index_directory_items++;
                numb_it_fol_untraver--;
            }
        }
    }

    /* all directory items loaded, does wanted file exist? */
    i = 0;
    int file_inode = -1;
    int index_struct_del = -1;
    for(; i < free_index_directory_items; i++){
        if(strcmp(directory_items[i].item_name, item_target_arr[item_target_arr_length - 1]) == 0){ /* inode with desired name has been found */
            file_inode = directory_items[i].inode;
            index_struct_del = i;
            break;
        }
    }

    free(directory_items);

    if(file_inode != -1){ /* inode with specified name found */
        if(content_fs -> loaded_pseudo_in_arr[file_inode].isDirectory == false){ /* is it file? */
            array_data_blocks[0] = content_fs -> loaded_pseudo_in_arr[file_inode].direct1;
            array_data_blocks[1] = content_fs -> loaded_pseudo_in_arr[file_inode].direct2;
            array_data_blocks[2] = content_fs -> loaded_pseudo_in_arr[file_inode].direct3;
            array_data_blocks[3] = content_fs -> loaded_pseudo_in_arr[file_inode].direct4;
            array_data_blocks[4] = content_fs -> loaded_pseudo_in_arr[file_inode].direct5;
            array_data_blocks[5] = content_fs -> loaded_pseudo_in_arr[file_inode].indirect1;
            array_data_blocks[6] = content_fs -> loaded_pseudo_in_arr[file_inode].indirect2;
            /* remove reference in bitmap with clusters */
            float cluster_f = (float) content_fs -> loaded_pseudo_in_arr[file_inode].file_size / FORMAT_CLUSTER_SIZE_B;
            int cluster_count_d = ceil(cluster_f); /* number of clusters occupied by file */
            int indirect1_hold = FORMAT_CLUSTER_SIZE_B / sizeof(int);
            i = 0;
            for(; i < 5; i++){ /* direct pointers */
                if(array_data_blocks[i] != -1){
                    content_fs -> loaded_bit_clus -> cluster_bitmap[array_data_blocks[i]] = 0;
                    content_fs -> loaded_bit_clus -> cluster_occupied_total--;
                    content_fs -> loaded_bit_clus -> cluster_free_total++;
                }
            }
            int rem_clus = -1;
            /* indirect pointer1 */
            if(array_data_blocks[5] != -1){ /* indirect pointer1 is not empty */
                rem_clus = cluster_count_d - 5;
                int cluster_indir1_count;
                if(rem_clus > indirect1_hold){
                    cluster_indir1_count = indirect1_hold;
                }else{
                    cluster_indir1_count = rem_clus;
                }
                int start_pos_indir1 = content_fs -> loaded_sb -> data_start_address + (array_data_blocks[5] * FORMAT_CLUSTER_SIZE_B);
                set_pointer_position_fs(start_pos_indir1, SEEK_SET);
                int *cluster_indir1_arr = (int *) read_from_file_fs(cluster_indir1_count * sizeof(int));
                int i = 0;
                for(; i < cluster_indir1_count; i++){
                    content_fs -> loaded_bit_clus -> cluster_bitmap[cluster_indir1_arr[i]] = 0;
                    content_fs -> loaded_bit_clus -> cluster_occupied_total--;
                    content_fs -> loaded_bit_clus -> cluster_free_total++;
                }
                free(cluster_indir1_arr);

                rem_clus -= (i + 1);
                /* the one with references */
                content_fs -> loaded_bit_clus -> cluster_bitmap[array_data_blocks[5]] = 0;
                content_fs -> loaded_bit_clus -> cluster_occupied_total--;
                content_fs -> loaded_bit_clus -> cluster_free_total++;
            }

            /* indirect pointer2 */
            if(array_data_blocks[6] != -1){ /* indirect pointer2 is not empty */
                /* check second list count */
                int sec_list_count = ceil((float) rem_clus / indirect1_hold);
                int last_sec_list_it = ceil((float) (content_fs -> loaded_pseudo_in_arr[file_inode].file_size - (((sec_list_count - 1) * indirect1_hold + indirect1_hold + 5) * FORMAT_CLUSTER_SIZE_B)) / FORMAT_CLUSTER_SIZE_B); /* number of items on last list */
                int *indir_first_list_addr;
                int *clus_to_del_arr;
                int num_items;

                set_pointer_position_fs(content_fs -> loaded_sb -> data_start_address + (array_data_blocks[6] * FORMAT_CLUSTER_SIZE_B), SEEK_SET);
                indir_first_list_addr = read_from_file_fs(sec_list_count * sizeof(int));

                int i = 0;
                for(; i < sec_list_count; i++){
                    set_pointer_position_fs(content_fs -> loaded_sb -> data_start_address + (indir_first_list_addr[i] * FORMAT_CLUSTER_SIZE_B), SEEK_SET);

                    if(i == (sec_list_count - 1)){ /* last one */
                        clus_to_del_arr = read_from_file_fs(last_sec_list_it * sizeof(int));
                        num_items = last_sec_list_it;
                    }else{
                        clus_to_del_arr = read_from_file_fs(indirect1_hold * sizeof(int));
                        num_items = indirect1_hold;
                    }

                    /* delete indirect2 second lists */
                    int j = 0;
                    for(; j < num_items; j++){
                        content_fs -> loaded_bit_clus -> cluster_bitmap[clus_to_del_arr[j]] = 0;
                        content_fs -> loaded_bit_clus -> cluster_occupied_total--;
                        content_fs -> loaded_bit_clus -> cluster_free_total++;
                    }
                    free(clus_to_del_arr);

                    content_fs -> loaded_bit_clus -> cluster_bitmap[indir_first_list_addr[i]] = 0;
                    content_fs -> loaded_bit_clus -> cluster_occupied_total--;
                    content_fs -> loaded_bit_clus -> cluster_free_total++;
                }
                free(indir_first_list_addr);
                content_fs -> loaded_bit_clus -> cluster_bitmap[array_data_blocks[6]] = 0;
                content_fs -> loaded_bit_clus -> cluster_occupied_total--;
                content_fs -> loaded_bit_clus -> cluster_free_total++;
            }
            save_bitmap_clusters(content_fs -> loaded_bit_clus);

            /* remove reference in bitmap with inodes */
            content_fs -> loaded_bit_in -> inodes_occupied_total--;
            content_fs -> loaded_bit_in -> inodes_free_total++;
            content_fs -> loaded_bit_in -> inodes_bitmap[file_inode] = 0;
            save_bitmap_inodes(content_fs -> loaded_bit_in,(content_fs -> loaded_bit_clus -> cluster_occupied_total + content_fs -> loaded_bit_clus -> cluster_free_total));
            /* set default values for respective inode */
            content_fs -> loaded_pseudo_in_arr[file_inode].file_size = -1;
            content_fs -> loaded_pseudo_in_arr[file_inode].references = 1;
            content_fs -> loaded_pseudo_in_arr[file_inode].isDirectory = false;
            content_fs -> loaded_pseudo_in_arr[file_inode].direct1 = -1;
            content_fs -> loaded_pseudo_in_arr[file_inode].direct2 = -1;
            content_fs -> loaded_pseudo_in_arr[file_inode].direct3 = -1;
            content_fs -> loaded_pseudo_in_arr[file_inode].direct4 = -1;
            content_fs -> loaded_pseudo_in_arr[file_inode].direct5 = -1;
            content_fs -> loaded_pseudo_in_arr[file_inode].indirect1 = -1;
            content_fs -> loaded_pseudo_in_arr[file_inode].indirect2 = -1;
            /* update current inode information - START */

            int num_it_fol = (actual_pseudo_inode -> file_size) / sizeof(struct directory_item); /* number of items in folder */
            int num_clus_it = FORMAT_CLUSTER_SIZE_B / sizeof(struct directory_item);/* number of items which one cluster is able to hold */

            int address_freed_struct_start = -1; /* address of struct directory_item which was deleted */
            int address_last_struct_start = -1; /* address of last written struct directory_item which should be moved to freed space */

            if(i != (num_it_fol - 1)){ /* if it was not the last created folder, then copy */
                if(num_it_fol <= num_clus_it){ /* direct 1 has last struct directory_item */
                    address_last_struct_start = (content_fs -> loaded_sb -> data_start_address) + (actual_pseudo_inode -> direct1) * FORMAT_CLUSTER_SIZE_B + ((num_it_fol - 1)
                                                                                                                                                              * sizeof(struct directory_item));
                }else if(num_it_fol <= num_clus_it * 2){ /* direct2 */
                    int num_items_sec_clus = num_it_fol - num_clus_it; /* number of struct directory_item stored in second cluster */
                    address_last_struct_start = (content_fs -> loaded_sb -> data_start_address) + (actual_pseudo_inode -> direct2) * FORMAT_CLUSTER_SIZE_B + ((num_items_sec_clus - 1)
                                                                                                                                                              * sizeof(struct directory_item));
                }else if(num_it_fol <= num_clus_it * 3){ /* direct3 */
                    int num_items_third_clus = num_it_fol - 2 * num_clus_it; /* number of struct directory_item stored in third cluster */
                    address_last_struct_start = (content_fs -> loaded_sb -> data_start_address) + (actual_pseudo_inode -> direct3) * FORMAT_CLUSTER_SIZE_B + ((num_items_third_clus - 1)
                                                                                                                                                              * sizeof(struct directory_item));
                }else if(num_it_fol <= num_clus_it * 4){ /* direct4 */
                    int num_items_fourth_clus = num_it_fol - 3 * num_clus_it; /* number of struct directory_item stored in fourth cluster */
                    address_last_struct_start = (content_fs -> loaded_sb -> data_start_address) + (actual_pseudo_inode -> direct4) * FORMAT_CLUSTER_SIZE_B + ((num_items_fourth_clus - 1)
                                                                                                                                                              * sizeof(struct directory_item));
                }else if(num_it_fol <= num_clus_it * 5){ /* direct5 */
                    int num_items_fifth_clus = num_it_fol - 4 * num_clus_it; /* number of struct directory_item stored in fifth cluster */
                    address_last_struct_start = (content_fs -> loaded_sb -> data_start_address) + (actual_pseudo_inode -> direct5) * FORMAT_CLUSTER_SIZE_B + ((num_items_fifth_clus - 1)
                                                                                                                                                              * sizeof(struct directory_item));
                }

                /* now we need to determine start address of deleted struct directory_item */
                if(index_struct_del + 1 <= num_clus_it){ /* direct 1 has deleted struct directory_item */
                    address_freed_struct_start = (content_fs -> loaded_sb -> data_start_address) + (actual_pseudo_inode -> direct1) * FORMAT_CLUSTER_SIZE_B + ((index_struct_del)
                                                                                                                                                               * sizeof(struct directory_item));
                }else if(index_struct_del + 1 <= num_clus_it * 2){ /* direct2 */
                    int num_items_sec_clus = (index_struct_del + 1) - num_clus_it;
                    address_freed_struct_start = (content_fs -> loaded_sb -> data_start_address) + (actual_pseudo_inode -> direct2) * FORMAT_CLUSTER_SIZE_B + ((num_items_sec_clus - 1)
                                                                                                                                                               * sizeof(struct directory_item));
                }else if(index_struct_del + 1 <= num_clus_it * 3){ /* direct3 */
                    int num_items_third_clus = (index_struct_del + 1) - (2 * num_clus_it);
                    address_freed_struct_start = (content_fs -> loaded_sb -> data_start_address) + (actual_pseudo_inode -> direct3) * FORMAT_CLUSTER_SIZE_B + ((num_items_third_clus - 1)
                                                                                                                                                               * sizeof(struct directory_item));
                }else if(index_struct_del + 1 <= num_clus_it * 4){ /* direct4 */
                    int num_items_fourth_clus = (index_struct_del + 1) - (3 * num_clus_it);
                    address_freed_struct_start = (content_fs -> loaded_sb -> data_start_address) + (actual_pseudo_inode -> direct4) * FORMAT_CLUSTER_SIZE_B + ((num_items_fourth_clus - 1)
                                                                                                                                                               * sizeof(struct directory_item));
                }else if(index_struct_del + 1 <= num_clus_it * 5){ /* direct5 */
                    int num_items_fifth_clus = (index_struct_del + 1) - (4 * num_clus_it);
                    address_freed_struct_start = (content_fs -> loaded_sb -> data_start_address) + (actual_pseudo_inode -> direct5) * FORMAT_CLUSTER_SIZE_B + ((num_items_fifth_clus - 1)
                                                                                                                                                               * sizeof(struct directory_item));
                }

                /* read last */
                set_pointer_position_fs(address_last_struct_start, SEEK_SET);
                struct directory_item *dir_last_item = (struct directory_item *) read_from_file_fs(sizeof(struct directory_item));
                set_pointer_position_fs(address_freed_struct_start, SEEK_SET);
                write_to_file_fs((void *) dir_last_item, sizeof(struct directory_item));
                free(dir_last_item);
            }

            actual_pseudo_inode -> file_size = actual_pseudo_inode -> file_size - sizeof(struct directory_item); /* one file removed */
            /* update current inode information - END */
            save_pseudo_inodes(content_fs -> loaded_pseudo_in_arr, (content_fs -> loaded_bit_in -> inodes_free_total + content_fs -> loaded_bit_in -> inodes_occupied_total),
                               (content_fs -> loaded_bit_clus -> cluster_free_total + content_fs -> loaded_bit_clus -> cluster_occupied_total),
                               (content_fs -> loaded_bit_in -> inodes_free_total + content_fs -> loaded_bit_in -> inodes_occupied_total));
        }else{
            printf(FILE_NOT_FOUND);
        }
    }else{
        printf(FILE_NOT_FOUND);
    }

    actual_pseudo_inode = actual_pseudo_inode_backup;
    close_file_fs();
}
/* ____________________________________________________________________________
    void perform_mkdir(char *first_word)

    Performs command mkdir (syntax: mkdir d1), which is used for creating
    directory d1.
   ____________________________________________________________________________
*/
void perform_mkdir(char *first_word){
    if(actual_pseudo_inode == NULL){ /* no currently traversed folder, will execute when creating root of FS */
        /* get free inode */
        struct pseudo_inode *free_pseudo_inode = NULL;
        int free_pseudo_inode_number = -1;
        int total_inode_count = (content_fs -> loaded_bit_in -> inodes_free_total) + (content_fs -> loaded_bit_in -> inodes_occupied_total);

        int i = 0;
        for(; i < total_inode_count; i++){
            if(content_fs -> loaded_bit_in -> inodes_bitmap[i] == 0){ /* free inode on "i" position found */
                free_pseudo_inode = &content_fs -> loaded_pseudo_in_arr[i]; /* (index is the same as with bitmap) */
                free_pseudo_inode_number = i;
                break;
            }
        }

        /* get free cluster */
        int free_cluster_number = get_free_cluster();

        /* got free cluster number, now we need to determine start address of the cluster */
        long int cluster_start_addr = (content_fs -> loaded_sb -> data_start_address) + (free_cluster_number * (content_fs -> loaded_sb -> cluster_size));

        /* fill and then write directory_item struct to data section of drive */
        struct directory_item *dir = malloc(sizeof(struct directory_item));
        memset(dir, 0, sizeof(struct directory_item));
        strcpy(dir -> item_name, first_word);
        dir -> inode = free_pseudo_inode_number;
        open_wb_add_fs();
        set_pointer_position_fs(cluster_start_addr, SEEK_SET);
        write_to_file_fs(dir, sizeof(struct directory_item));

        /* write directory_item struct containing data about actual inode, new inode will have reference to upper folder */
        struct directory_item *upper_folder; /* directory_item of upper folder */
        if(actual_pseudo_inode != NULL){
            int upper_folder_start_addr = content_fs -> loaded_sb -> data_start_address + actual_pseudo_inode -> direct1 * FORMAT_CLUSTER_SIZE_B;
            set_pointer_position_fs(upper_folder_start_addr, SEEK_SET);
            upper_folder = (struct directory_item *) read_from_file_fs(sizeof(struct directory_item));
            set_pointer_position_fs(cluster_start_addr + sizeof(struct directory_item), SEEK_SET);
            write_to_file_fs(upper_folder, sizeof(struct directory_item));
        }

        /* update bitmap with cluster state */
        content_fs -> loaded_bit_clus -> cluster_occupied_total++;
        content_fs -> loaded_bit_clus -> cluster_free_total--;
        content_fs -> loaded_bit_clus -> cluster_bitmap[free_cluster_number] = 1;
        save_bitmap_clusters(content_fs -> loaded_bit_clus);

        /* update bitmap with inode state */
        content_fs -> loaded_bit_in -> inodes_occupied_total++;
        content_fs -> loaded_bit_in -> inodes_free_total--;
        content_fs -> loaded_bit_in -> inodes_bitmap[free_pseudo_inode_number] = 1;
        save_bitmap_inodes(content_fs -> loaded_bit_in, (content_fs -> loaded_bit_clus -> cluster_occupied_total + content_fs -> loaded_bit_clus -> cluster_free_total));

        /* update rest of pseudo_inode struct and write to FS file */
        free_pseudo_inode -> isDirectory = true;
        if(actual_pseudo_inode != NULL){
            free_pseudo_inode -> file_size = sizeof(struct directory_item) * 2; /* first is the directory itself and second is reference to upper folder */
        }else{
            free_pseudo_inode -> file_size = sizeof(struct directory_item); /* first is the directory itself */
        }
        free_pseudo_inode -> direct1 = free_cluster_number;
        save_pseudo_inodes(content_fs -> loaded_pseudo_in_arr, (content_fs -> loaded_bit_in -> inodes_free_total + content_fs -> loaded_bit_in -> inodes_occupied_total),
                           (content_fs -> loaded_bit_clus -> cluster_free_total + content_fs -> loaded_bit_clus -> cluster_occupied_total),
                           (content_fs -> loaded_bit_in -> inodes_free_total + content_fs -> loaded_bit_in -> inodes_occupied_total));
        free(dir);
        close_file_fs();
        return;
    }

    struct pseudo_inode *actual_pseudo_inode_backup = actual_pseudo_inode; /* we will switch inodes */
    struct pseudo_inode *pseudo_inode_target = NULL; /* inode of folder in which the new fodler will be created */
    char (*item_target_arr)[15] = NULL;
    int item_target_arr_length = 0;

    bool error_occured = false; /* if true, then error occured (not enough clusters, etc.) */

    char *one_word = NULL;
    /* split target path by '/' to get individual folders - START */
    one_word = strtok(first_word, "/");
    while(one_word != NULL){
        /* check length of folder name */
        if(strlen(one_word) > 14){
            printf(MKDIR_EXCEED_LENGTH);
            if(item_target_arr_length != 0){
                free(item_target_arr);
            }
            return;
        }

        item_target_arr = realloc(item_target_arr, (item_target_arr_length + 1) * sizeof *item_target_arr);
        strcpy(item_target_arr[item_target_arr_length], one_word);
        one_word = strtok(NULL, "/"); /* move to next word */

        item_target_arr_length += 1;
    }
    /* split target path by '/' to get individual folders - END */

    /* find inode for target path - START */
    if(first_word[0] == '/'){ /* path is absolute, load first inode and continue traversing */
        if(item_target_arr_length == 1){ /* create folder in root of FS */
            pseudo_inode_target = actual_pseudo_inode_backup;
        }else{
            actual_pseudo_inode = &content_fs -> loaded_pseudo_in_arr[0];

            int x = 0;
            for(; x < item_target_arr_length - 1; x++){ /* -1 because last word should be name of the folder which is not yet created */
                open_rb_fs();
                int numb_it_fol_untraver = (actual_pseudo_inode -> file_size) / sizeof(struct directory_item); /* number of untraversed items in folder */
                struct directory_item *directory_items = malloc(sizeof(struct directory_item) * numb_it_fol_untraver);
                int free_index_directory_items = 0;
                int array_data_blocks[5];

                /* fill array of data block numbers for better traversing */
                array_data_blocks[0] = actual_pseudo_inode -> direct1;
                array_data_blocks[1] = actual_pseudo_inode -> direct2;
                array_data_blocks[2] = actual_pseudo_inode -> direct3;
                array_data_blocks[3] = actual_pseudo_inode -> direct4;
                array_data_blocks[4] = actual_pseudo_inode -> direct5;

                int i = 0;
                for(; i < 5; i++){ /* read direct */
                    if(numb_it_fol_untraver != 0){
                        int read_count = -1;
                        int j = 0;

                        if(numb_it_fol_untraver * sizeof(struct directory_item) <= FORMAT_CLUSTER_SIZE_B){ /* read just part of a cluster */
                            read_count = numb_it_fol_untraver;
                        }else{ /* read whole cluster */
                            read_count = FORMAT_CLUSTER_SIZE_B / sizeof(struct directory_item);
                        }

                        for(; j < read_count; j++){
                            int start_address = content_fs -> loaded_sb -> data_start_address + array_data_blocks[i] * FORMAT_CLUSTER_SIZE_B + (j * sizeof(struct directory_item));
                            set_pointer_position_fs(start_address, SEEK_SET);
                            struct directory_item *dir_item = (struct directory_item *) read_from_file_fs(sizeof(struct directory_item));
                            directory_items[free_index_directory_items] = *dir_item;
                            free(dir_item);
                            free_index_directory_items++;
                            numb_it_fol_untraver--;
                        }
                    }
                }
                close_file_fs();

                /* all directory items loaded, does wanted folder exist? */
                i = 0;
                int new_inode_number = -1;
                for(; i < free_index_directory_items; i++){
                    if(strcmp(directory_items[i].item_name, item_target_arr[x]) == 0){ /* inode with desired name has been found */
                        new_inode_number = directory_items[i].inode;
                        break;
                    }
                }

                if(new_inode_number != -1){
                    if(content_fs -> loaded_pseudo_in_arr[new_inode_number].isDirectory == true){
                        if(x == item_target_arr_length - 2){
                            pseudo_inode_target = &content_fs -> loaded_pseudo_in_arr[new_inode_number];
                        }else{
                            actual_pseudo_inode = &content_fs -> loaded_pseudo_in_arr[new_inode_number];
                        }
                    }else{
                        printf(FILE_NOT_FOUND);
                        error_occured = true;
                        free(directory_items);
                        break;
                    }
                }else{
                    printf(FILE_NOT_FOUND);
                    error_occured = true;
                    free(directory_items);
                    break;
                }
                free(directory_items);
            }
        }
    }else{ /* path is relative, go from current inode and try to traverse */
        if(item_target_arr_length == 1){
            pseudo_inode_target = actual_pseudo_inode_backup;
        }else{
            actual_pseudo_inode = actual_pseudo_inode_backup;

            int x = 0;
            for(; x < item_target_arr_length - 1; x++){
                open_rb_fs();
                int numb_it_fol_untraver = (actual_pseudo_inode -> file_size) / sizeof(struct directory_item); /* number of untraversed items in folder */
                struct directory_item *directory_items = malloc(sizeof(struct directory_item) * numb_it_fol_untraver);
                int free_index_directory_items = 0;
                int array_data_blocks[5];

                /* fill array of data block numbers for better traversing */
                array_data_blocks[0] = actual_pseudo_inode -> direct1;
                array_data_blocks[1] = actual_pseudo_inode -> direct2;
                array_data_blocks[2] = actual_pseudo_inode -> direct3;
                array_data_blocks[3] = actual_pseudo_inode -> direct4;
                array_data_blocks[4] = actual_pseudo_inode -> direct5;

                int i = 0;
                for(; i < 5; i++){ /* read direct */
                    if(numb_it_fol_untraver != 0){
                        int read_count = -1;
                        int j = 0;

                        if(numb_it_fol_untraver * sizeof(struct directory_item) <= FORMAT_CLUSTER_SIZE_B){ /* read just part of a cluster */
                            read_count = numb_it_fol_untraver;
                        }else{ /* read whole cluster */
                            read_count = FORMAT_CLUSTER_SIZE_B / sizeof(struct directory_item);
                        }

                        for(; j < read_count; j++){
                            int start_address = content_fs -> loaded_sb -> data_start_address + array_data_blocks[i] * FORMAT_CLUSTER_SIZE_B + (j * sizeof(struct directory_item));
                            set_pointer_position_fs(start_address, SEEK_SET);
                            struct directory_item *dir_item = (struct directory_item *) read_from_file_fs(sizeof(struct directory_item));
                            directory_items[free_index_directory_items] = *dir_item;
                            free(dir_item);
                            free_index_directory_items++;
                            numb_it_fol_untraver--;
                        }
                    }
                }
                close_file_fs();

                /* all directory items loaded, does wanted folder exist? */
                int new_inode_number = -1;
                bool in_root_upper = false;

                if(strlen(item_target_arr[x]) == 1 && item_target_arr[x][0] == '.'){
                    new_inode_number = directory_items[0].inode; /* first directory_item in folder is the folder itself */
                }else if(strlen(item_target_arr[x]) == 2 && item_target_arr[x][0] == '.' && item_target_arr[x][1] == '.'){
                    if(directory_items[0].item_name[0] == '/'){ /* user is in root, cannot move upper */
                        in_root_upper = true;
                        error_occured = true;
                    }else{
                        new_inode_number = directory_items[1].inode; /* second directory_item in folder is the upper folder */
                    }
                }else{
                    i = 0;
                    for(; i < free_index_directory_items; i++){
                        if(strcmp(directory_items[i].item_name, item_target_arr[x]) == 0){ /* inode with desired name has been found */
                            new_inode_number = directory_items[i].inode;
                            break;
                        }
                    }
                }

                if(new_inode_number != -1){
                    if(content_fs -> loaded_pseudo_in_arr[new_inode_number].isDirectory == true){
                        if(x == item_target_arr_length - 2){
                            pseudo_inode_target = &content_fs -> loaded_pseudo_in_arr[new_inode_number];
                        }else{
                            actual_pseudo_inode = &content_fs -> loaded_pseudo_in_arr[new_inode_number];
                        }
                    }else{
                        printf(FILE_NOT_FOUND);
                        error_occured = true;
                        free(directory_items);
                        break;
                    }
                }else{
                    if(in_root_upper == true){
                        printf(ROOT_FS_SEARCH, item_target_arr[x]);
                    }else{
                        printf(FILE_NOT_FOUND);
                    }
                    free(directory_items);
                    break;
                }
                free(directory_items);
            }
        }
    }
    /* find inode for target path - END */

    if(pseudo_inode_target == NULL){
        return;
    }

    /* get free inode */
    struct pseudo_inode *free_pseudo_inode = NULL;
    int free_pseudo_inode_number = -1;
    int total_inode_count = (content_fs -> loaded_bit_in -> inodes_free_total) + (content_fs -> loaded_bit_in -> inodes_occupied_total);

    int i = 0;
    for(; i < total_inode_count; i++){
        if(content_fs -> loaded_bit_in -> inodes_bitmap[i] == 0){ /* free inode on "i" position found */
            free_pseudo_inode = &content_fs -> loaded_pseudo_in_arr[i]; /* (index is the same as with bitmap) */
            free_pseudo_inode_number = i;
            break;
        }
    }

    /* no free inode found */
    if(free_pseudo_inode_number == -1){
        printf(MKDIR_NO_INODE);
        error_occured = true;
        return;
    }

    /* get free cluster */
    int free_cluster_number = get_free_cluster();

    /* no free cluster found */
    if(free_cluster_number == -1){
        printf(MKDIR_NO_CLUSTER);
        error_occured = true;
        return;
    }

    actual_pseudo_inode = pseudo_inode_target;
    if(error_occured == false){
        /* got free cluster number, now we need to determine start address of the cluster */
        long int cluster_start_addr = (content_fs -> loaded_sb -> data_start_address) + (free_cluster_number * (content_fs -> loaded_sb -> cluster_size));

        /* fill and then write directory_item struct to data section of drive */
        struct directory_item *dir = malloc(sizeof(struct directory_item));
        memset(dir, 0, sizeof(struct directory_item));
        strcpy(dir -> item_name, item_target_arr[item_target_arr_length - 1]);
        free(item_target_arr);
        dir -> inode = free_pseudo_inode_number;
        open_wb_add_fs();
        set_pointer_position_fs(cluster_start_addr, SEEK_SET);
        write_to_file_fs(dir, sizeof(struct directory_item));

        /* write directory_item struct containing data about actual inode, new inode will have reference to upper folder */
        struct directory_item *upper_folder; /* directory_item of upper folder */
        if(actual_pseudo_inode != NULL){
            int upper_folder_start_addr = content_fs -> loaded_sb -> data_start_address + actual_pseudo_inode -> direct1 * FORMAT_CLUSTER_SIZE_B;
            set_pointer_position_fs(upper_folder_start_addr, SEEK_SET);
            upper_folder = (struct directory_item *) read_from_file_fs(sizeof(struct directory_item));
            set_pointer_position_fs(cluster_start_addr + sizeof(struct directory_item), SEEK_SET);
            write_to_file_fs(upper_folder, sizeof(struct directory_item));
        }

        /* update bitmap with cluster state */
        content_fs -> loaded_bit_clus -> cluster_occupied_total++;
        content_fs -> loaded_bit_clus -> cluster_free_total--;
        content_fs -> loaded_bit_clus -> cluster_bitmap[free_cluster_number] = 1;
        save_bitmap_clusters(content_fs -> loaded_bit_clus);

        /* update bitmap with inode state */
        content_fs -> loaded_bit_in -> inodes_occupied_total++;
        content_fs -> loaded_bit_in -> inodes_free_total--;
        content_fs -> loaded_bit_in -> inodes_bitmap[free_pseudo_inode_number] = 1;
        save_bitmap_inodes(content_fs -> loaded_bit_in, (content_fs -> loaded_bit_clus -> cluster_occupied_total + content_fs -> loaded_bit_clus -> cluster_free_total));

        /* update rest of pseudo_inode struct and write to FS file */
        free_pseudo_inode -> isDirectory = true;
        if(actual_pseudo_inode != NULL){
            free_pseudo_inode -> file_size = sizeof(struct directory_item) * 2; /* first is the directory itself and second is reference to upper folder */
        }else{
            free_pseudo_inode -> file_size = sizeof(struct directory_item); /* first is the directory itself */
        }
        free_pseudo_inode -> direct1 = free_cluster_number;
        save_pseudo_inodes(content_fs -> loaded_pseudo_in_arr, (content_fs -> loaded_bit_in -> inodes_free_total + content_fs -> loaded_bit_in -> inodes_occupied_total),
                           (content_fs -> loaded_bit_clus -> cluster_free_total + content_fs -> loaded_bit_clus -> cluster_occupied_total),
                           (content_fs -> loaded_bit_in -> inodes_free_total + content_fs -> loaded_bit_in -> inodes_occupied_total));

        /* link newly created folder to currently traversed folder */
        int needed_size = actual_pseudo_inode -> file_size + sizeof(struct directory_item);
        int one_cluster_count = FORMAT_CLUSTER_SIZE_B / sizeof(struct directory_item); /* count of directory items which is one cluster able to hold */
        int start_address;
        bool new_cluster_alloc = false; /* will be true if we recently tried to assign a new cluster */
        int free_clus_num = -1; /* will contain number of newly assigned cluster, -1 if no free cluster has been found */

        if(needed_size <= (one_cluster_count * sizeof(struct directory_item))){ /* direct 1 has enough space to store */
            start_address = (content_fs -> loaded_sb -> data_start_address) + (actual_pseudo_inode -> direct1) * FORMAT_CLUSTER_SIZE_B + actual_pseudo_inode -> file_size;
        }else if(needed_size <= (one_cluster_count * sizeof(struct directory_item)) * 2){ /* direct2 */
            if(actual_pseudo_inode -> direct2 == -1){ /* cluster was not reserved yet */
                new_cluster_alloc = true;
                free_clus_num = get_free_cluster();
                actual_pseudo_inode -> direct2 = free_clus_num;
            }
            start_address = (content_fs -> loaded_sb -> data_start_address) + (actual_pseudo_inode -> direct2) * FORMAT_CLUSTER_SIZE_B + (actual_pseudo_inode -> file_size) - (one_cluster_count * sizeof(struct directory_item));
        }else if(needed_size <= (one_cluster_count * sizeof(struct directory_item)) * 3){ /* direct3 */
            if(actual_pseudo_inode -> direct3 == -1){ /* cluster was not reserved yet */
                new_cluster_alloc = true;
                free_clus_num = get_free_cluster();
                actual_pseudo_inode -> direct3 = free_clus_num;
            }
            start_address = (content_fs -> loaded_sb -> data_start_address) + (actual_pseudo_inode -> direct3) * FORMAT_CLUSTER_SIZE_B + (actual_pseudo_inode -> file_size) - 2 * (one_cluster_count * sizeof(struct directory_item));
        }else if(needed_size <= (one_cluster_count * sizeof(struct directory_item)) * 4){ /* direct4 */
            if(actual_pseudo_inode -> direct4 == -1){ /* cluster was not reserved yet */
                new_cluster_alloc = true;
                free_clus_num = get_free_cluster();
                actual_pseudo_inode -> direct4 = free_clus_num;
            }
            start_address = (content_fs -> loaded_sb -> data_start_address) + (actual_pseudo_inode -> direct4) * FORMAT_CLUSTER_SIZE_B + (actual_pseudo_inode -> file_size) - 3 * (one_cluster_count * sizeof(struct directory_item));
        }else if(needed_size <= (one_cluster_count * sizeof(struct directory_item)) * 5){ /* direct5 */
            if(actual_pseudo_inode -> direct5 == -1){ /* cluster was not reserved yet */
                new_cluster_alloc = true;
                free_clus_num = get_free_cluster();
                actual_pseudo_inode -> direct5 = free_clus_num;
            }
            start_address = (content_fs -> loaded_sb -> data_start_address) + (actual_pseudo_inode -> direct5) * FORMAT_CLUSTER_SIZE_B + (actual_pseudo_inode -> file_size) - 4 * (one_cluster_count * sizeof(struct directory_item));
        }

        if(new_cluster_alloc == true){ /* no free cluster found */
            if(free_clus_num == -1){
                printf(MKDIR_NO_CLUSTER);
                return;
            }else{ /* update bitmap with cluster state */
                content_fs -> loaded_bit_clus -> cluster_occupied_total++;
                content_fs -> loaded_bit_clus -> cluster_free_total--;
                content_fs -> loaded_bit_clus -> cluster_bitmap[free_clus_num] = 1;
                save_bitmap_clusters(content_fs -> loaded_bit_clus);
            }
        }

        set_pointer_position_fs(start_address, SEEK_SET);
        write_to_file_fs(dir, sizeof(struct directory_item));
        free(upper_folder);
        free(dir);
        /* update file size of folder */
        actual_pseudo_inode -> file_size = actual_pseudo_inode -> file_size + sizeof(struct directory_item);
        save_pseudo_inodes(content_fs -> loaded_pseudo_in_arr, (content_fs -> loaded_bit_in -> inodes_free_total + content_fs -> loaded_bit_in -> inodes_occupied_total),
                           (content_fs -> loaded_bit_clus -> cluster_free_total + content_fs -> loaded_bit_clus -> cluster_occupied_total),
                           (content_fs -> loaded_bit_in -> inodes_free_total + content_fs -> loaded_bit_in -> inodes_occupied_total));
    }
    actual_pseudo_inode = actual_pseudo_inode_backup;
}
/* ____________________________________________________________________________
    void perform_rmdir(char *first_word)

    Performs command rmdir (syntax: rmdir d1), which is used for removing
    empty directory d1.
   ____________________________________________________________________________
*/
void perform_rmdir(char *first_word){
    struct pseudo_inode *actual_pseudo_inode_backup = actual_pseudo_inode; /* we will switch inodes */
    struct pseudo_inode *pseudo_inode_target = NULL; /* inode of folder from which the folder will be deleted */
    char (*item_target_arr)[15] = NULL;
    int item_target_arr_length = 0;

    bool error_occured = false; /* if true, then error occured (wrong folder name etc.) */

    char *one_word = NULL;
    /* split target path by '/' to get individual folders - START */
    one_word = strtok(first_word, "/");
    while(one_word != NULL){
        /* check length of folder name */
        if(strlen(one_word) > 14){
            printf(RMDIR_EXCEED_LENGTH);
            if(item_target_arr_length != 0){
                free(item_target_arr);
            }
            return;
        }

        item_target_arr = realloc(item_target_arr, (item_target_arr_length + 1) * sizeof *item_target_arr);
        strcpy(item_target_arr[item_target_arr_length], one_word);
        one_word = strtok(NULL, "/"); /* move to next word */

        item_target_arr_length += 1;
    }
    /* split target path by '/' to get individual folders - END */

    /* find inode for target path - START */
    if(first_word[0] == '/'){ /* path is absolute, load first inode and continue traversing */
        if(item_target_arr_length == 1){ /* delete folder in root of FS */
            pseudo_inode_target = actual_pseudo_inode_backup;
        }else{
            actual_pseudo_inode = &content_fs -> loaded_pseudo_in_arr[0];

            int x = 0;
            for(; x < item_target_arr_length - 1; x++){
                open_rb_fs();
                int numb_it_fol_untraver = (actual_pseudo_inode -> file_size) / sizeof(struct directory_item); /* number of untraversed items in folder */
                struct directory_item *directory_items = malloc(sizeof(struct directory_item) * numb_it_fol_untraver);
                int free_index_directory_items = 0;
                int array_data_blocks[5];

                /* fill array of data block numbers for better traversing */
                array_data_blocks[0] = actual_pseudo_inode -> direct1;
                array_data_blocks[1] = actual_pseudo_inode -> direct2;
                array_data_blocks[2] = actual_pseudo_inode -> direct3;
                array_data_blocks[3] = actual_pseudo_inode -> direct4;
                array_data_blocks[4] = actual_pseudo_inode -> direct5;

                int i = 0;
                for(; i < 5; i++){ /* read direct */
                    if(numb_it_fol_untraver != 0){
                        int read_count = -1;
                        int j = 0;

                        if(numb_it_fol_untraver * sizeof(struct directory_item) <= FORMAT_CLUSTER_SIZE_B){ /* read just part of a cluster */
                            read_count = numb_it_fol_untraver;
                        }else{ /* read whole cluster */
                            read_count = FORMAT_CLUSTER_SIZE_B / sizeof(struct directory_item);
                        }

                        for(; j < read_count; j++){
                            int start_address = content_fs -> loaded_sb -> data_start_address + array_data_blocks[i] * FORMAT_CLUSTER_SIZE_B + (j * sizeof(struct directory_item));
                            set_pointer_position_fs(start_address, SEEK_SET);
                            struct directory_item *dir_item = (struct directory_item *) read_from_file_fs(sizeof(struct directory_item));
                            directory_items[free_index_directory_items] = *dir_item;
                            free(dir_item);
                            free_index_directory_items++;
                            numb_it_fol_untraver--;
                        }
                    }
                }
                close_file_fs();

                /* all directory items loaded, does wanted folder exist? */
                i = 0;
                int new_inode_number = -1;
                for(; i < free_index_directory_items; i++){
                    if(strcmp(directory_items[i].item_name, item_target_arr[x]) == 0){ /* inode with desired name has been found */
                        new_inode_number = directory_items[i].inode;
                        break;
                    }
                }

                if(new_inode_number != -1){
                    if(content_fs -> loaded_pseudo_in_arr[new_inode_number].isDirectory == true){
                        if(x == item_target_arr_length - 2){
                            pseudo_inode_target = &content_fs -> loaded_pseudo_in_arr[new_inode_number];
                        }else{
                            actual_pseudo_inode = &content_fs -> loaded_pseudo_in_arr[new_inode_number];
                        }
                    }else{
                        printf(FILE_NOT_FOUND);
                        error_occured = true;
                        free(directory_items);
                        break;
                    }
                }else{
                    printf(FILE_NOT_FOUND);
                    error_occured = true;
                    free(directory_items);
                    break;
                }
                free(directory_items);
            }
        }
    }else{ /* path is relative, go from current inode and try to traverse */
        if(item_target_arr_length == 1){
            pseudo_inode_target = actual_pseudo_inode_backup;
        }else{
            actual_pseudo_inode = actual_pseudo_inode_backup;

            int x = 0;
            for(; x < item_target_arr_length - 1; x++){
                open_rb_fs();
                int numb_it_fol_untraver = (actual_pseudo_inode -> file_size) / sizeof(struct directory_item); /* number of untraversed items in folder */
                struct directory_item *directory_items = malloc(sizeof(struct directory_item) * numb_it_fol_untraver);
                int free_index_directory_items = 0;
                int array_data_blocks[5];

                /* fill array of data block numbers for better traversing */
                array_data_blocks[0] = actual_pseudo_inode -> direct1;
                array_data_blocks[1] = actual_pseudo_inode -> direct2;
                array_data_blocks[2] = actual_pseudo_inode -> direct3;
                array_data_blocks[3] = actual_pseudo_inode -> direct4;
                array_data_blocks[4] = actual_pseudo_inode -> direct5;

                int i = 0;
                for(; i < 5; i++){ /* read direct */
                    if(numb_it_fol_untraver != 0){
                        int read_count = -1;
                        int j = 0;

                        if(numb_it_fol_untraver * sizeof(struct directory_item) <= FORMAT_CLUSTER_SIZE_B){ /* read just part of a cluster */
                            read_count = numb_it_fol_untraver;
                        }else{ /* read whole cluster */
                            read_count = FORMAT_CLUSTER_SIZE_B / sizeof(struct directory_item);
                        }

                        for(; j < read_count; j++){
                            int start_address = content_fs -> loaded_sb -> data_start_address + array_data_blocks[i] * FORMAT_CLUSTER_SIZE_B + (j * sizeof(struct directory_item));
                            set_pointer_position_fs(start_address, SEEK_SET);
                            struct directory_item *dir_item = (struct directory_item *) read_from_file_fs(sizeof(struct directory_item));
                            directory_items[free_index_directory_items] = *dir_item;
                            free(dir_item);
                            free_index_directory_items++;
                            numb_it_fol_untraver--;
                        }
                    }
                }
                close_file_fs();

                /* all directory items loaded, does wanted folder exist? */
                int new_inode_number = -1;
                bool in_root_upper = false;

                if(strlen(item_target_arr[x]) == 1 && item_target_arr[x][0] == '.'){
                    new_inode_number = directory_items[0].inode; /* first directory_item in folder is the folder itself */
                }else if(strlen(item_target_arr[x]) == 2 && item_target_arr[x][0] == '.' && item_target_arr[x][1] == '.'){
                    if(directory_items[0].item_name[0] == '/'){ /* user is in root, cannot move upper */
                        in_root_upper = true;
                        error_occured = true;
                    }else{
                        new_inode_number = directory_items[1].inode; /* second directory_item in folder is the upper folder */
                    }
                }else{
                    i = 0;
                    for(; i < free_index_directory_items; i++){
                        if(strcmp(directory_items[i].item_name, item_target_arr[x]) == 0){ /* inode with desired name has been found */
                            new_inode_number = directory_items[i].inode;
                            break;
                        }
                    }
                }

                if(new_inode_number != -1){
                    if(content_fs -> loaded_pseudo_in_arr[new_inode_number].isDirectory == true){
                        if(x == item_target_arr_length - 2){
                            pseudo_inode_target = &content_fs -> loaded_pseudo_in_arr[new_inode_number];
                        }else{
                            actual_pseudo_inode = &content_fs -> loaded_pseudo_in_arr[new_inode_number];
                        }
                    }else{
                        printf(FILE_NOT_FOUND);
                        error_occured = true;
                        free(directory_items);
                        break;
                    }
                }else{
                    if(in_root_upper == true){
                        printf(ROOT_FS_SEARCH, item_target_arr[x]);
                    }else{
                        printf(FILE_NOT_FOUND);
                    }
                    free(directory_items);
                    break;
                }
                free(directory_items);
            }
        }
    }
    /* find inode for target path - END */

    if(pseudo_inode_target == NULL){
        return;
    }

    open_wb_add_fs();

    actual_pseudo_inode = pseudo_inode_target;

    int numb_it_fol_untraver = (actual_pseudo_inode -> file_size) / sizeof(struct directory_item); /* number of untraversed items in folder */
    struct directory_item *directory_items = malloc(sizeof(struct directory_item) * numb_it_fol_untraver);
    int free_index_directory_items = 0;
    int array_data_blocks[7];

    /* fill array of data block numbers for better traversing */
    array_data_blocks[0] = actual_pseudo_inode -> direct1;
    array_data_blocks[1] = actual_pseudo_inode -> direct2;
    array_data_blocks[2] = actual_pseudo_inode -> direct3;
    array_data_blocks[3] = actual_pseudo_inode -> direct4;
    array_data_blocks[4] = actual_pseudo_inode -> direct5;

    int i = 0;
    for(; i < 5; i++){ /* read direct */
        if(numb_it_fol_untraver != 0){
            int read_count = -1;
            int j = 0;

            if(numb_it_fol_untraver * sizeof(struct directory_item) <= FORMAT_CLUSTER_SIZE_B){ /* read just part of a cluster */
                read_count = numb_it_fol_untraver;
            }else{ /* read whole cluster */
                read_count = FORMAT_CLUSTER_SIZE_B / sizeof(struct directory_item);
            }

            for(; j < read_count; j++){
                int start_address = content_fs -> loaded_sb -> data_start_address + array_data_blocks[i] * FORMAT_CLUSTER_SIZE_B + (j * sizeof(struct directory_item));
                set_pointer_position_fs(start_address, SEEK_SET);
                struct directory_item *dir_item = (struct directory_item *) read_from_file_fs(sizeof(struct directory_item));
                directory_items[free_index_directory_items] = *dir_item;
                free(dir_item);
                free_index_directory_items++;
                numb_it_fol_untraver--;
            }
        }
    }

    /* all directory items loaded, does wanted folder exist? */
    i = 0;
    int dir_inode = -1;
    int index_struct_del = -1;
    for(; i < free_index_directory_items; i++){
        if(strcmp(directory_items[i].item_name, item_target_arr[item_target_arr_length - 1]) == 0){ /* inode with desired name has been found */
            dir_inode = directory_items[i].inode;
            index_struct_del = i;
            break;
        }
    }

    if(dir_inode != -1){ /* folder found */
        if(content_fs -> loaded_pseudo_in_arr[dir_inode].file_size == sizeof(struct directory_item) * 2){ /* check emptiness - first folder itself, second is upper folder */
            /* copy last struct directory_item to deleted */
            int num_it_fol = (actual_pseudo_inode -> file_size) / sizeof(struct directory_item); /* number of items in folder */
            int num_clus_it = FORMAT_CLUSTER_SIZE_B / sizeof(struct directory_item);/* number of items which one cluster is able to hold */

            int address_freed_struct_start = -1; /* address of struct directory_item which was deleted */
            int address_last_struct_start = -1; /* address of last written struct directory_item which should be moved to freed space */

            if(i != (num_it_fol - 1)){ /* if it was not the last created folder, then copy */
                if(num_it_fol <= num_clus_it){ /* direct 1 has last struct directory_item */
                    address_last_struct_start = (content_fs -> loaded_sb -> data_start_address) + (actual_pseudo_inode -> direct1) * FORMAT_CLUSTER_SIZE_B + ((num_it_fol - 1)
                            * sizeof(struct directory_item));
                }else if(num_it_fol <= num_clus_it * 2){ /* direct2 */
                    int num_items_sec_clus = num_it_fol - num_clus_it; /* number of struct directory_item stored in second cluster */
                    address_last_struct_start = (content_fs -> loaded_sb -> data_start_address) + (actual_pseudo_inode -> direct2) * FORMAT_CLUSTER_SIZE_B + ((num_items_sec_clus - 1)
                            * sizeof(struct directory_item));
                }else if(num_it_fol <= num_clus_it * 3){ /* direct3 */
                    int num_items_third_clus = num_it_fol - 2 * num_clus_it; /* number of struct directory_item stored in third cluster */
                    address_last_struct_start = (content_fs -> loaded_sb -> data_start_address) + (actual_pseudo_inode -> direct3) * FORMAT_CLUSTER_SIZE_B + ((num_items_third_clus - 1)
                            * sizeof(struct directory_item));
                }else if(num_it_fol <= num_clus_it * 4){ /* direct4 */
                    int num_items_fourth_clus = num_it_fol - 3 * num_clus_it; /* number of struct directory_item stored in fourth cluster */
                    address_last_struct_start = (content_fs -> loaded_sb -> data_start_address) + (actual_pseudo_inode -> direct4) * FORMAT_CLUSTER_SIZE_B + ((num_items_fourth_clus - 1)
                            * sizeof(struct directory_item));
                }else if(num_it_fol <= num_clus_it * 5){ /* direct5 */
                    int num_items_fifth_clus = num_it_fol - 4 * num_clus_it; /* number of struct directory_item stored in fifth cluster */
                    address_last_struct_start = (content_fs -> loaded_sb -> data_start_address) + (actual_pseudo_inode -> direct5) * FORMAT_CLUSTER_SIZE_B + ((num_items_fifth_clus - 1)
                            * sizeof(struct directory_item));
                }

                /* now we need to determine start address of deleted struct directory_item */
                if(index_struct_del + 1 <= num_clus_it){ /* direct 1 has deleted struct directory_item */
                    address_freed_struct_start = (content_fs -> loaded_sb -> data_start_address) + (actual_pseudo_inode -> direct1) * FORMAT_CLUSTER_SIZE_B + ((index_struct_del)
                                                                                                                                                              * sizeof(struct directory_item));
                }else if(index_struct_del + 1 <= num_clus_it * 2){ /* direct2 */
                    int num_items_sec_clus = (index_struct_del + 1) - num_clus_it;
                    address_freed_struct_start = (content_fs -> loaded_sb -> data_start_address) + (actual_pseudo_inode -> direct2) * FORMAT_CLUSTER_SIZE_B + ((num_items_sec_clus - 1)
                                                                                                                                                              * sizeof(struct directory_item));
                }else if(index_struct_del + 1 <= num_clus_it * 3){ /* direct3 */
                    int num_items_third_clus = (index_struct_del + 1) - (2 * num_clus_it);
                    address_freed_struct_start = (content_fs -> loaded_sb -> data_start_address) + (actual_pseudo_inode -> direct3) * FORMAT_CLUSTER_SIZE_B + ((num_items_third_clus - 1)
                                                                                                                                                              * sizeof(struct directory_item));
                }else if(index_struct_del + 1 <= num_clus_it * 4){ /* direct4 */
                    int num_items_fourth_clus = (index_struct_del + 1) - (3 * num_clus_it);
                    address_freed_struct_start = (content_fs -> loaded_sb -> data_start_address) + (actual_pseudo_inode -> direct4) * FORMAT_CLUSTER_SIZE_B + ((num_items_fourth_clus - 1)
                                                                                                                                                              * sizeof(struct directory_item));
                }else if(index_struct_del + 1 <= num_clus_it * 5){ /* direct5 */
                    int num_items_fifth_clus = (index_struct_del + 1) - (4 * num_clus_it);
                    address_freed_struct_start = (content_fs -> loaded_sb -> data_start_address) + (actual_pseudo_inode -> direct5) * FORMAT_CLUSTER_SIZE_B + ((num_items_fifth_clus - 1)
                                                                                                                                                              * sizeof(struct directory_item));
                }

                /* read last */
                set_pointer_position_fs(address_last_struct_start, SEEK_SET);
                struct directory_item *dir_last_item = (struct directory_item *) read_from_file_fs(sizeof(struct directory_item));
                set_pointer_position_fs(address_freed_struct_start, SEEK_SET);
                write_to_file_fs((void *) dir_last_item, sizeof(struct directory_item));
                free(dir_last_item);
            }

            /* update current inode information */
            actual_pseudo_inode -> file_size = actual_pseudo_inode -> file_size - sizeof(struct directory_item); /* one folder removed */

            /* fill array of data block numbers for better traversing */
            array_data_blocks[0] = content_fs -> loaded_pseudo_in_arr[dir_inode].direct1;
            array_data_blocks[1] = content_fs -> loaded_pseudo_in_arr[dir_inode].direct2;
            array_data_blocks[2] = content_fs -> loaded_pseudo_in_arr[dir_inode].direct3;
            array_data_blocks[3] = content_fs -> loaded_pseudo_in_arr[dir_inode].direct4;
            array_data_blocks[4] = content_fs -> loaded_pseudo_in_arr[dir_inode].direct5;
            array_data_blocks[5] = content_fs -> loaded_pseudo_in_arr[dir_inode].indirect1;
            array_data_blocks[6] = content_fs -> loaded_pseudo_in_arr[dir_inode].indirect2;

            /* remove reference in bitmap with clusters */
            i = 0;
            for(; i < 5; i++){ /* direct pointers */
                if(array_data_blocks[i] != -1){
                    content_fs -> loaded_bit_clus -> cluster_bitmap[array_data_blocks[i]] = 0;
                    content_fs -> loaded_bit_clus -> cluster_occupied_total--;
                    content_fs -> loaded_bit_clus -> cluster_free_total++;
                }
            }
            save_bitmap_clusters(content_fs -> loaded_bit_clus);

            /* remove reference in bitmap with inodes */
            content_fs -> loaded_bit_in -> inodes_occupied_total--;
            content_fs -> loaded_bit_in -> inodes_free_total++;
            content_fs -> loaded_bit_in -> inodes_bitmap[dir_inode] = 0;
            save_bitmap_inodes(content_fs -> loaded_bit_in,(content_fs -> loaded_bit_clus -> cluster_occupied_total + content_fs -> loaded_bit_clus -> cluster_free_total));

            /* set default values for respective inode */
            content_fs -> loaded_pseudo_in_arr[dir_inode].file_size = -1;
            content_fs -> loaded_pseudo_in_arr[dir_inode].references = 1;
            content_fs -> loaded_pseudo_in_arr[dir_inode].isDirectory = false;
            content_fs -> loaded_pseudo_in_arr[dir_inode].direct1 = -1;
            content_fs -> loaded_pseudo_in_arr[dir_inode].direct2 = -1;
            content_fs -> loaded_pseudo_in_arr[dir_inode].direct3 = -1;
            content_fs -> loaded_pseudo_in_arr[dir_inode].direct4 = -1;
            content_fs -> loaded_pseudo_in_arr[dir_inode].direct5 = -1;
            content_fs -> loaded_pseudo_in_arr[dir_inode].indirect1 = -1;
            content_fs -> loaded_pseudo_in_arr[dir_inode].indirect2 = -1;
            save_pseudo_inodes(content_fs -> loaded_pseudo_in_arr, (content_fs -> loaded_bit_in -> inodes_free_total + content_fs -> loaded_bit_in -> inodes_occupied_total),
                               (content_fs -> loaded_bit_clus -> cluster_free_total + content_fs -> loaded_bit_clus -> cluster_occupied_total),
                               (content_fs -> loaded_bit_in -> inodes_free_total + content_fs -> loaded_bit_in -> inodes_occupied_total));
        }else{
            printf(RMDIR_NOT_EMPTY, first_word);
        }
    }else{
        printf(RMDIR_NOT_FOUND, item_target_arr[item_target_arr_length - 1]);
    }
    free(directory_items);
    free(item_target_arr);

    actual_pseudo_inode = actual_pseudo_inode_backup;
    close_file_fs();
}
/* ____________________________________________________________________________
    void perform_ls(char *first_word)

    Performs command ls (syntax: ls d1), which is used for printing
    content of directory d1.
   ____________________________________________________________________________
*/
void perform_ls(char *first_word){
    struct pseudo_inode *actual_pseudo_inode_backup = actual_pseudo_inode; /* we will switch inodes */
    struct pseudo_inode *pseudo_inode_target = NULL; /* inode of folder in which the new fodler will be created */
    char (*item_target_arr)[15] = NULL;
    int item_target_arr_length = 0;

    bool error_occured = false; /* if true, then error occured (not enough clusters, etc.) */

    char *one_word = NULL;
    /* split target path by '/' to get individual folders - START */
    one_word = strtok(first_word, "/");
    while(one_word != NULL){
        /* check length of folder name */
        if(strlen(one_word) > 14){
            printf(MKDIR_EXCEED_LENGTH);
            if(item_target_arr_length != 0){
                free(item_target_arr);
            }
            return;
        }

        item_target_arr = realloc(item_target_arr, (item_target_arr_length + 1) * sizeof *item_target_arr);
        strcpy(item_target_arr[item_target_arr_length], one_word);
        one_word = strtok(NULL, "/"); /* move to next word */

        item_target_arr_length += 1;
    }
    /* split target path by '/' to get individual folders - END */

    /* find inode for target path - START */
    if(first_word[0] == '/'){ /* path is absolute, load first inode and continue traversing */
        if(item_target_arr_length == 1 && item_target_arr[0][0] == '.'){
            pseudo_inode_target = actual_pseudo_inode_backup;
        }else{
            actual_pseudo_inode = &content_fs -> loaded_pseudo_in_arr[0];

            int x = 0;
            for(; x < item_target_arr_length; x++){
                open_rb_fs();
                int numb_it_fol_untraver = (actual_pseudo_inode -> file_size) / sizeof(struct directory_item); /* number of untraversed items in folder */
                struct directory_item *directory_items = malloc(sizeof(struct directory_item) * numb_it_fol_untraver);
                int free_index_directory_items = 0;
                int array_data_blocks[5];

                /* fill array of data block numbers for better traversing */
                array_data_blocks[0] = actual_pseudo_inode -> direct1;
                array_data_blocks[1] = actual_pseudo_inode -> direct2;
                array_data_blocks[2] = actual_pseudo_inode -> direct3;
                array_data_blocks[3] = actual_pseudo_inode -> direct4;
                array_data_blocks[4] = actual_pseudo_inode -> direct5;

                int i = 0;
                for(; i < 5; i++){ /* read direct */
                    if(numb_it_fol_untraver != 0){
                        int read_count = -1;
                        int j = 0;

                        if(numb_it_fol_untraver * sizeof(struct directory_item) <= FORMAT_CLUSTER_SIZE_B){ /* read just part of a cluster */
                            read_count = numb_it_fol_untraver;
                        }else{ /* read whole cluster */
                            read_count = FORMAT_CLUSTER_SIZE_B / sizeof(struct directory_item);
                        }

                        for(; j < read_count; j++){
                            int start_address = content_fs -> loaded_sb -> data_start_address + array_data_blocks[i] * FORMAT_CLUSTER_SIZE_B + (j * sizeof(struct directory_item));
                            set_pointer_position_fs(start_address, SEEK_SET);
                            struct directory_item *dir_item = (struct directory_item *) read_from_file_fs(sizeof(struct directory_item));
                            directory_items[free_index_directory_items] = *dir_item;
                            free(dir_item);
                            free_index_directory_items++;
                            numb_it_fol_untraver--;
                        }
                    }
                }
                close_file_fs();

                /* all directory items loaded, does wanted folder exist? */
                i = 0;
                int new_inode_number = -1;
                for(; i < free_index_directory_items; i++){
                    if(strcmp(directory_items[i].item_name, item_target_arr[x]) == 0){ /* inode with desired name has been found */
                        new_inode_number = directory_items[i].inode;
                        break;
                    }
                }

                if(new_inode_number != -1){
                    if(content_fs -> loaded_pseudo_in_arr[new_inode_number].isDirectory == true){
                        if(x == item_target_arr_length - 1){
                            pseudo_inode_target = &content_fs -> loaded_pseudo_in_arr[new_inode_number];
                        }else{
                            actual_pseudo_inode = &content_fs -> loaded_pseudo_in_arr[new_inode_number];
                        }
                    }else{
                        printf(FILE_NOT_FOUND);
                        error_occured = true;
                        free(directory_items);
                        break;
                    }
                }else{
                    printf(FILE_NOT_FOUND);
                    error_occured = true;
                    free(directory_items);
                    break;
                }
                free(directory_items);
            }
        }
    }else{ /* path is relative, go from current inode and try to traverse */
        if(item_target_arr_length == 1 && item_target_arr[0][0] == '.'){
            pseudo_inode_target = actual_pseudo_inode_backup;
        }else{
            actual_pseudo_inode = actual_pseudo_inode_backup;

            int x = 0;
            for(; x < item_target_arr_length; x++){
                open_rb_fs();
                int numb_it_fol_untraver = (actual_pseudo_inode -> file_size) / sizeof(struct directory_item); /* number of untraversed items in folder */
                struct directory_item *directory_items = malloc(sizeof(struct directory_item) * numb_it_fol_untraver);
                int free_index_directory_items = 0;
                int array_data_blocks[5];

                /* fill array of data block numbers for better traversing */
                array_data_blocks[0] = actual_pseudo_inode -> direct1;
                array_data_blocks[1] = actual_pseudo_inode -> direct2;
                array_data_blocks[2] = actual_pseudo_inode -> direct3;
                array_data_blocks[3] = actual_pseudo_inode -> direct4;
                array_data_blocks[4] = actual_pseudo_inode -> direct5;

                int i = 0;
                for(; i < 5; i++){ /* read direct */
                    if(numb_it_fol_untraver != 0){
                        int read_count = -1;
                        int j = 0;

                        if(numb_it_fol_untraver * sizeof(struct directory_item) <= FORMAT_CLUSTER_SIZE_B){ /* read just part of a cluster */
                            read_count = numb_it_fol_untraver;
                        }else{ /* read whole cluster */
                            read_count = FORMAT_CLUSTER_SIZE_B / sizeof(struct directory_item);
                        }

                        for(; j < read_count; j++){
                            int start_address = content_fs -> loaded_sb -> data_start_address + array_data_blocks[i] * FORMAT_CLUSTER_SIZE_B + (j * sizeof(struct directory_item));
                            set_pointer_position_fs(start_address, SEEK_SET);
                            struct directory_item *dir_item = (struct directory_item *) read_from_file_fs(sizeof(struct directory_item));
                            directory_items[free_index_directory_items] = *dir_item;
                            free(dir_item);
                            free_index_directory_items++;
                            numb_it_fol_untraver--;
                        }
                    }
                }
                close_file_fs();

                /* all directory items loaded, does wanted folder exist? */
                int new_inode_number = -1;
                bool in_root_upper = false;

                if(strlen(item_target_arr[x]) == 1 && item_target_arr[x][0] == '.'){
                    new_inode_number = directory_items[0].inode; /* first directory_item in folder is the folder itself */
                }else if(strlen(item_target_arr[x]) == 2 && item_target_arr[x][0] == '.' && item_target_arr[x][1] == '.'){
                    if(directory_items[0].item_name[0] == '/'){ /* user is in root, cannot move upper */
                        in_root_upper = true;
                        error_occured = true;
                    }else{
                        new_inode_number = directory_items[1].inode; /* second directory_item in folder is the upper folder */
                    }
                }else{
                    i = 0;
                    for(; i < free_index_directory_items; i++){
                        if(strcmp(directory_items[i].item_name, item_target_arr[x]) == 0){ /* inode with desired name has been found */
                            new_inode_number = directory_items[i].inode;
                            break;
                        }
                    }
                }

                if(new_inode_number != -1){
                    if(content_fs -> loaded_pseudo_in_arr[new_inode_number].isDirectory == true){
                        if(x == item_target_arr_length - 1){
                            pseudo_inode_target = &content_fs -> loaded_pseudo_in_arr[new_inode_number];
                        }else{
                            actual_pseudo_inode = &content_fs -> loaded_pseudo_in_arr[new_inode_number];
                        }
                    }else{
                        printf(FILE_NOT_FOUND);
                        error_occured = true;
                        free(directory_items);
                        break;
                    }
                }else{
                    if(in_root_upper == true){
                        printf(ROOT_FS_SEARCH, item_target_arr[x]);
                    }else{
                        printf(FILE_NOT_FOUND);
                    }
                    free(directory_items);
                    break;
                }
                free(directory_items);
            }
        }
    }
    /* find inode for target path - END */

    if(pseudo_inode_target == NULL){
        return;
    }

    actual_pseudo_inode = pseudo_inode_target;

    open_rb_fs();
    int numb_it_fol_untraver = (actual_pseudo_inode -> file_size) / sizeof(struct directory_item); /* number of untraversed items in folder */
    struct directory_item *directory_items = malloc(sizeof(struct directory_item) * numb_it_fol_untraver);
    int free_index_directory_items = 0;
    int array_data_blocks[7];

    /* fill array of data block numbers for better traversing */
    array_data_blocks[0] = actual_pseudo_inode -> direct1;
    array_data_blocks[1] = actual_pseudo_inode -> direct2;
    array_data_blocks[2] = actual_pseudo_inode -> direct3;
    array_data_blocks[3] = actual_pseudo_inode -> direct4;
    array_data_blocks[4] = actual_pseudo_inode -> direct5;
    array_data_blocks[5] = actual_pseudo_inode -> indirect1;
    array_data_blocks[6] = actual_pseudo_inode -> indirect2;

    int i = 0;
    for(; i < 5; i++){ /* read direct */
        if(numb_it_fol_untraver != 0){
            int read_count = -1;
            int j = 0;

            if(numb_it_fol_untraver * sizeof(struct directory_item) <= FORMAT_CLUSTER_SIZE_B){ /* read just part of a cluster */
                read_count = numb_it_fol_untraver;
            }else{ /* read whole cluster */
                read_count = FORMAT_CLUSTER_SIZE_B / sizeof(struct directory_item);
            }

            for(; j < read_count; j++){
                int start_address = content_fs -> loaded_sb -> data_start_address + array_data_blocks[i] * FORMAT_CLUSTER_SIZE_B + (j * sizeof(struct directory_item));
                set_pointer_position_fs(start_address, SEEK_SET);
                struct directory_item *dir_item = (struct directory_item *) read_from_file_fs(sizeof(struct directory_item));
                directory_items[free_index_directory_items] = *dir_item;
                free(dir_item);
                free_index_directory_items++;
                numb_it_fol_untraver--;
            }
        }
    }

    if(strcmp("/", directory_items[0].item_name) == 0){
        i = 1;
    }else{
        i = 2;
    }

    for(; i < free_index_directory_items; i++){
        printf("%s \n", directory_items[i].item_name);
    }

    free(item_target_arr);
    free(directory_items);
    close_file_fs();

    actual_pseudo_inode = actual_pseudo_inode_backup;
}
/* ____________________________________________________________________________
    void perform_cat(char *first_word)

    Performs command cat (syntax: cat f1), which is used for printing
    content of file f1.
   ____________________________________________________________________________
*/
void perform_cat(char *first_word){
    struct pseudo_inode *actual_pseudo_inode_backup = actual_pseudo_inode; /* we will switch inodes */
    struct pseudo_inode *pseudo_inode_target = NULL; /* inode of folder from which the folder will be deleted */
    char (*item_target_arr)[15] = NULL;
    int item_target_arr_length = 0;

    bool error_occured = false; /* if true, then error occured (wrong folder name etc.) */

    char *one_word = NULL;
    /* split target path by '/' to get individual folders - START */
    one_word = strtok(first_word, "/");
    while(one_word != NULL){
        /* check length of folder name */
        if(strlen(one_word) > 14){
            printf(RMDIR_EXCEED_LENGTH);
            if(item_target_arr_length != 0){
                free(item_target_arr);
            }
            return;
        }

        item_target_arr = realloc(item_target_arr, (item_target_arr_length + 1) * sizeof *item_target_arr);
        strcpy(item_target_arr[item_target_arr_length], one_word);
        one_word = strtok(NULL, "/"); /* move to next word */

        item_target_arr_length += 1;
    }
    /* split target path by '/' to get individual folders - END */

    /* find inode for target path - START */
    if(first_word[0] == '/'){ /* path is absolute, load first inode and continue traversing */
        if(item_target_arr_length == 1){ /* delete folder in root of FS */
            pseudo_inode_target = actual_pseudo_inode_backup;
        }else{
            actual_pseudo_inode = &content_fs -> loaded_pseudo_in_arr[0];

            int x = 0;
            for(; x < item_target_arr_length - 1; x++){
                open_rb_fs();
                int numb_it_fol_untraver = (actual_pseudo_inode -> file_size) / sizeof(struct directory_item); /* number of untraversed items in folder */
                struct directory_item *directory_items = malloc(sizeof(struct directory_item) * numb_it_fol_untraver);
                int free_index_directory_items = 0;
                int array_data_blocks[5];

                /* fill array of data block numbers for better traversing */
                array_data_blocks[0] = actual_pseudo_inode -> direct1;
                array_data_blocks[1] = actual_pseudo_inode -> direct2;
                array_data_blocks[2] = actual_pseudo_inode -> direct3;
                array_data_blocks[3] = actual_pseudo_inode -> direct4;
                array_data_blocks[4] = actual_pseudo_inode -> direct5;

                int i = 0;
                for(; i < 5; i++){ /* read direct */
                    if(numb_it_fol_untraver != 0){
                        int read_count = -1;
                        int j = 0;

                        if(numb_it_fol_untraver * sizeof(struct directory_item) <= FORMAT_CLUSTER_SIZE_B){ /* read just part of a cluster */
                            read_count = numb_it_fol_untraver;
                        }else{ /* read whole cluster */
                            read_count = FORMAT_CLUSTER_SIZE_B / sizeof(struct directory_item);
                        }

                        for(; j < read_count; j++){
                            int start_address = content_fs -> loaded_sb -> data_start_address + array_data_blocks[i] * FORMAT_CLUSTER_SIZE_B + (j * sizeof(struct directory_item));
                            set_pointer_position_fs(start_address, SEEK_SET);
                            struct directory_item *dir_item = (struct directory_item *) read_from_file_fs(sizeof(struct directory_item));
                            directory_items[free_index_directory_items] = *dir_item;
                            free(dir_item);
                            free_index_directory_items++;
                            numb_it_fol_untraver--;
                        }
                    }
                }
                close_file_fs();

                /* all directory items loaded, does wanted folder exist? */
                i = 0;
                int new_inode_number = -1;
                for(; i < free_index_directory_items; i++){
                    if(strcmp(directory_items[i].item_name, item_target_arr[x]) == 0){ /* inode with desired name has been found */
                        new_inode_number = directory_items[i].inode;
                        break;
                    }
                }

                if(new_inode_number != -1){
                    if(content_fs -> loaded_pseudo_in_arr[new_inode_number].isDirectory == true){
                        if(x == item_target_arr_length - 2){
                            pseudo_inode_target = &content_fs -> loaded_pseudo_in_arr[new_inode_number];
                        }else{
                            actual_pseudo_inode = &content_fs -> loaded_pseudo_in_arr[new_inode_number];
                        }
                    }else{
                        printf(FILE_NOT_FOUND);
                        error_occured = true;
                        free(directory_items);
                        break;
                    }
                }else{
                    printf(FILE_NOT_FOUND);
                    error_occured = true;
                    free(directory_items);
                    break;
                }
                free(directory_items);
            }
        }
    }else{ /* path is relative, go from current inode and try to traverse */
        if(item_target_arr_length == 1){
            pseudo_inode_target = actual_pseudo_inode_backup;
        }else{
            actual_pseudo_inode = actual_pseudo_inode_backup;

            int x = 0;
            for(; x < item_target_arr_length - 1; x++){
                open_rb_fs();
                int numb_it_fol_untraver = (actual_pseudo_inode -> file_size) / sizeof(struct directory_item); /* number of untraversed items in folder */
                struct directory_item *directory_items = malloc(sizeof(struct directory_item) * numb_it_fol_untraver);
                int free_index_directory_items = 0;
                int array_data_blocks[5];

                /* fill array of data block numbers for better traversing */
                array_data_blocks[0] = actual_pseudo_inode -> direct1;
                array_data_blocks[1] = actual_pseudo_inode -> direct2;
                array_data_blocks[2] = actual_pseudo_inode -> direct3;
                array_data_blocks[3] = actual_pseudo_inode -> direct4;
                array_data_blocks[4] = actual_pseudo_inode -> direct5;

                int i = 0;
                for(; i < 5; i++){ /* read direct */
                    if(numb_it_fol_untraver != 0){
                        int read_count = -1;
                        int j = 0;

                        if(numb_it_fol_untraver * sizeof(struct directory_item) <= FORMAT_CLUSTER_SIZE_B){ /* read just part of a cluster */
                            read_count = numb_it_fol_untraver;
                        }else{ /* read whole cluster */
                            read_count = FORMAT_CLUSTER_SIZE_B / sizeof(struct directory_item);
                        }

                        for(; j < read_count; j++){
                            int start_address = content_fs -> loaded_sb -> data_start_address + array_data_blocks[i] * FORMAT_CLUSTER_SIZE_B + (j * sizeof(struct directory_item));
                            set_pointer_position_fs(start_address, SEEK_SET);
                            struct directory_item *dir_item = (struct directory_item *) read_from_file_fs(sizeof(struct directory_item));
                            directory_items[free_index_directory_items] = *dir_item;
                            free(dir_item);
                            free_index_directory_items++;
                            numb_it_fol_untraver--;
                        }
                    }
                }
                close_file_fs();

                /* all directory items loaded, does wanted folder exist? */
                int new_inode_number = -1;
                bool in_root_upper = false;

                if(strlen(item_target_arr[x]) == 1 && item_target_arr[x][0] == '.'){
                    new_inode_number = directory_items[0].inode; /* first directory_item in folder is the folder itself */
                }else if(strlen(item_target_arr[x]) == 2 && item_target_arr[x][0] == '.' && item_target_arr[x][1] == '.'){
                    if(directory_items[0].item_name[0] == '/'){ /* user is in root, cannot move upper */
                        in_root_upper = true;
                        error_occured = true;
                    }else{
                        new_inode_number = directory_items[1].inode; /* second directory_item in folder is the upper folder */
                    }
                }else{
                    i = 0;
                    for(; i < free_index_directory_items; i++){
                        if(strcmp(directory_items[i].item_name, item_target_arr[x]) == 0){ /* inode with desired name has been found */
                            new_inode_number = directory_items[i].inode;
                            break;
                        }
                    }
                }

                if(new_inode_number != -1){
                    if(content_fs -> loaded_pseudo_in_arr[new_inode_number].isDirectory == true){
                        if(x == item_target_arr_length - 2){
                            pseudo_inode_target = &content_fs -> loaded_pseudo_in_arr[new_inode_number];
                        }else{
                            actual_pseudo_inode = &content_fs -> loaded_pseudo_in_arr[new_inode_number];
                        }
                    }else{
                        printf(FILE_NOT_FOUND);
                        error_occured = true;
                        free(directory_items);
                        break;
                    }
                }else{
                    if(in_root_upper == true){
                        printf(ROOT_FS_SEARCH, item_target_arr[x]);
                    }else{
                        printf(FILE_NOT_FOUND);
                    }
                    free(directory_items);
                    break;
                }
                free(directory_items);
            }
        }
    }
    /* find inode for target path - END */

    if(pseudo_inode_target == NULL){
        return;
    }

    actual_pseudo_inode = pseudo_inode_target;

    open_rb_fs();
    int numb_it_fol_untraver = (actual_pseudo_inode -> file_size) / sizeof(struct directory_item); /* number of untraversed items in folder */
    struct directory_item *directory_items = malloc(sizeof(struct directory_item) * numb_it_fol_untraver);
    int free_index_directory_items = 0;
    int array_data_blocks[5];

    /* fill array of data block numbers for better traversing */
    array_data_blocks[0] = actual_pseudo_inode -> direct1;
    array_data_blocks[1] = actual_pseudo_inode -> direct2;
    array_data_blocks[2] = actual_pseudo_inode -> direct3;
    array_data_blocks[3] = actual_pseudo_inode -> direct4;
    array_data_blocks[4] = actual_pseudo_inode -> direct5;

    int i = 0;
    for(; i < 5; i++){ /* read direct */
        if(numb_it_fol_untraver != 0){
            int read_count = -1;
            int j = 0;

            if(numb_it_fol_untraver * sizeof(struct directory_item) <= FORMAT_CLUSTER_SIZE_B){ /* read just part of a cluster */
                read_count = numb_it_fol_untraver;
            }else{ /* read whole cluster */
                read_count = FORMAT_CLUSTER_SIZE_B / sizeof(struct directory_item);
            }

            for(; j < read_count; j++){
                int start_address = content_fs -> loaded_sb -> data_start_address + array_data_blocks[i] * FORMAT_CLUSTER_SIZE_B + (j * sizeof(struct directory_item));
                set_pointer_position_fs(start_address, SEEK_SET);
                struct directory_item *dir_item = (struct directory_item *) read_from_file_fs(sizeof(struct directory_item));
                directory_items[free_index_directory_items] = *dir_item;
                free(dir_item);
                free_index_directory_items++;
                numb_it_fol_untraver--;
            }
        }
    }

    /* all directory items loaded, does wanted item exist? */
    i = 0;
    int file_inode_number = -1;
    for(; i < free_index_directory_items; i++){
        if(strcmp(directory_items[i].item_name, item_target_arr[item_target_arr_length - 1]) == 0){ /* inode with desired name has been found */
            file_inode_number = directory_items[i].inode;
            break;
        }
    }

    if(file_inode_number != -1 && content_fs -> loaded_pseudo_in_arr[file_inode_number].isDirectory == false){ /* file exists */
        struct pseudo_inode *pseudo_in;
        pseudo_in = &content_fs -> loaded_pseudo_in_arr[file_inode_number];

        printf(CAT_START, item_target_arr[item_target_arr_length - 1]);
        printf(CAT_SIZE, pseudo_in -> file_size);

        int cluster_inode[7];
        cluster_inode[0] = pseudo_in -> direct1;
        cluster_inode[1] = pseudo_in -> direct2;
        cluster_inode[2] = pseudo_in -> direct3;
        cluster_inode[3] = pseudo_in -> direct4;
        cluster_inode[4] = pseudo_in -> direct5;
        cluster_inode[5] = pseudo_in -> indirect1;
        cluster_inode[6] = pseudo_in -> indirect2;

        int file_size = pseudo_in -> file_size;

        int occupied_cluster_data = ceil((float) file_size / FORMAT_CLUSTER_SIZE_B); /* number of clusters reserved for data */
        int occupied_cluster_extra = 0; /* number of extra clusters (for indirect pointer list etc.) */
        /* extra clusters will be needed if indirect is going to be used */
        int indirect1_hold = FORMAT_CLUSTER_SIZE_B / sizeof(int);

        /* count extra clusters - START */
        if(occupied_cluster_data > 5){ /* direct pointers not enough, extra cluster for indirect1 */
            occupied_cluster_extra += 1;

            if(occupied_cluster_data > (5 + indirect1_hold)){ /* indirect2 */
                occupied_cluster_extra += 1; /* for first cluster list */

                int second_list_count = ceil((float) (occupied_cluster_data - (5 + indirect1_hold)) / indirect1_hold); /* needed cluster count / list capacity */
                occupied_cluster_extra += second_list_count;
            }
        }
        /* count extra clusters - END */

        int address_offset = 0;

        /* read direct - START */
        int end;
        if(occupied_cluster_data < 5){
            end = occupied_cluster_data;
        }else{
            end = 5;
        }

        int i = 0;
        for(; i < end; i++){
            int start_address = content_fs -> loaded_sb -> data_start_address + cluster_inode[i] * FORMAT_CLUSTER_SIZE_B;
            set_pointer_position_fs(start_address, SEEK_SET);
            char *data;

            int j = 0;
            if(end < 5 && (i == end - 1)){ /* reading last cluster */
                int size_to_read = file_size - (i * FORMAT_CLUSTER_SIZE_B);
                data = (char *) read_from_file_fs(size_to_read); /* remaining size */
                address_offset += size_to_read;

                for(; j < size_to_read; j++){
                    printf("%c", data[j]);
                }
            }else{
                data = (char *) read_from_file_fs(FORMAT_CLUSTER_SIZE_B);
                address_offset += FORMAT_CLUSTER_SIZE_B;

                for(; j < FORMAT_CLUSTER_SIZE_B; j++){
                    printf("%c", data[j]);
                }
            }
            free(data);
        }
        /* read direct - END */

        /* read indirect */
        if(occupied_cluster_extra > 0){ /* first extra cluster is a list for indirect1 */
            int loop_end;
            if((occupied_cluster_data - 5) > indirect1_hold){
                loop_end = indirect1_hold;
            }else{
                loop_end = occupied_cluster_data - 5;
            }

            int start_address = content_fs -> loaded_sb -> data_start_address + cluster_inode[5] * FORMAT_CLUSTER_SIZE_B;
            set_pointer_position_fs(start_address, SEEK_SET);
            int *clust_indir1_arr = (int *) read_from_file_fs(loop_end * sizeof(int));

            i = 0;
            for(; i < loop_end; i++){
                int start_address = content_fs -> loaded_sb -> data_start_address + clust_indir1_arr[i] * FORMAT_CLUSTER_SIZE_B;
                set_pointer_position_fs(start_address, SEEK_SET);
                char *data;

                int j = 0;
                if(loop_end < indirect1_hold && (i == loop_end - 1)){ /* reading last cluster */
                    int size_to_read = file_size - ((i + 5) * FORMAT_CLUSTER_SIZE_B);
                    data = (char *) read_from_file_fs(size_to_read); /* remaining size */
                    address_offset += size_to_read;
                    for(; j < size_to_read; j++){
                        printf("%c", data[j]);
                    }
                }else{
                    data = (char *) read_from_file_fs(FORMAT_CLUSTER_SIZE_B);
                    address_offset += FORMAT_CLUSTER_SIZE_B;
                    for(; j < FORMAT_CLUSTER_SIZE_B; j++){
                        printf("%c", data[j]);
                    }
                }
                free(data);
            }

            free(clust_indir1_arr);

            if(occupied_cluster_extra > 1){ /* second extra cluster is a first list for indirect2 */
                int second_list_count = ceil((float) (occupied_cluster_data - 5 - indirect1_hold) / indirect1_hold);

                int start_address = content_fs -> loaded_sb -> data_start_address + cluster_inode[6] * FORMAT_CLUSTER_SIZE_B;
                set_pointer_position_fs(start_address, SEEK_SET);
                int *second_list_arr = (int *) read_from_file_fs(sizeof(int) * second_list_count);

                i = 0;
                for(; i < second_list_count; i++){
                    int start_address_sec_list = content_fs -> loaded_sb -> data_start_address + second_list_arr[i] * FORMAT_CLUSTER_SIZE_B;
                    set_pointer_position_fs(start_address_sec_list, SEEK_SET);
                    int *clusters_arr;

                    if((occupied_cluster_data - 5 - indirect1_hold - (indirect1_hold * i)) < indirect1_hold){
                        loop_end = occupied_cluster_data - 5 - indirect1_hold - (indirect1_hold * i);
                    }else{
                        loop_end = indirect1_hold;
                    }

                    clusters_arr = read_from_file_fs(loop_end * sizeof(int));
                    char *data;

                    int j = 0;
                    for(; j < loop_end; j++){
                        set_pointer_position_fs(content_fs -> loaded_sb -> data_start_address + clusters_arr[j] * FORMAT_CLUSTER_SIZE_B, SEEK_SET);

                        int p = 0;
                        if(loop_end < indirect1_hold && (j == loop_end - 1)){ /* reading last cluster */
                            int size_to_read = file_size - ((5 + indirect1_hold + (indirect1_hold * i) + j) * FORMAT_CLUSTER_SIZE_B);
                            data = (char *) read_from_file_fs(size_to_read); /* remaining size */
                            address_offset += size_to_read;
                            for(; p < size_to_read; p++){
                                printf("%c", data[p]);
                            }
                        }else{
                            data = (char *) read_from_file_fs(FORMAT_CLUSTER_SIZE_B);
                            address_offset += FORMAT_CLUSTER_SIZE_B;
                            for(; p < FORMAT_CLUSTER_SIZE_B; p++){
                                printf("%c", data[p]);
                            }
                        }
                        free(data);
                    }

                    free(clusters_arr);
                }

                free(second_list_arr);
            }
        }

        printf(CAT_END, item_target_arr[item_target_arr_length - 1]);
    }else{
        printf(FILE_NOT_FOUND);
    }
    free(item_target_arr);
    free(directory_items);
    close_file_fs();

    actual_pseudo_inode = actual_pseudo_inode_backup;
}
/* ____________________________________________________________________________
    void perform_cd(char *first_word)

    Performs command cd (syntax: cd d1), which is used for changing
    actual path to directory d1.
   ____________________________________________________________________________
*/
void perform_cd(char *first_word){
    bool load_upper = false;
    if(strlen(first_word) == 2 && first_word[0] == '.' && first_word[1] == '.'){
        load_upper = true;
    }

    open_rb_fs();
    if(actual_pseudo_inode == NULL){ /* load root of FS if previous folder unknown - "char *first_word" ignored in this case */
        actual_pseudo_inode = &content_fs -> loaded_pseudo_in_arr[0]; /* root is always represented by first inode */
        int start_address = content_fs -> loaded_sb -> data_start_address;
        set_pointer_position_fs(start_address, SEEK_SET);
        struct directory_item *dir = (struct directory_item *) read_from_file_fs(sizeof(struct directory_item));
        strcpy(actual_folder_name, dir -> item_name);
        free(dir);
        close_file_fs();
        return;
    }

    struct pseudo_inode *actual_pseudo_inode_backup = actual_pseudo_inode; /* we will switch inodes */
    struct pseudo_inode *pseudo_inode_target = NULL; /* inode of folder from which the folder will be deleted */
    char (*item_target_arr)[15] = NULL;
    int item_target_arr_length = 0;

    bool error_occured = false; /* if true, then error occured (wrong folder name etc.) */

    char *one_word = NULL;
    /* split target path by '/' to get individual folders - START */
    one_word = strtok(first_word, "/");
    while(one_word != NULL){
        /* check length of folder name */
        if(strlen(one_word) > 14){
            printf(RMDIR_EXCEED_LENGTH);
            if(item_target_arr_length != 0){
                free(item_target_arr);
            }
            return;
        }

        item_target_arr = realloc(item_target_arr, (item_target_arr_length + 1) * sizeof *item_target_arr);
        strcpy(item_target_arr[item_target_arr_length], one_word);
        one_word = strtok(NULL, "/"); /* move to next word */

        item_target_arr_length += 1;
    }
    /* split target path by '/' to get individual folders - END */

    /* find inode for target path - START */
    if(first_word[0] == '/'){ /* path is absolute, load first inode and continue traversing */
        if(item_target_arr_length == 1 && strlen(item_target_arr[0]) == 1 && item_target_arr[0][0] == '.'){
            pseudo_inode_target = actual_pseudo_inode_backup;
        }else{
            actual_pseudo_inode = &content_fs -> loaded_pseudo_in_arr[0];

            int x = 0;
            for(; x < item_target_arr_length - 1; x++){
                open_rb_fs();
                int numb_it_fol_untraver = (actual_pseudo_inode -> file_size) / sizeof(struct directory_item); /* number of untraversed items in folder */
                struct directory_item *directory_items = malloc(sizeof(struct directory_item) * numb_it_fol_untraver);
                int free_index_directory_items = 0;
                int array_data_blocks[5];

                /* fill array of data block numbers for better traversing */
                array_data_blocks[0] = actual_pseudo_inode -> direct1;
                array_data_blocks[1] = actual_pseudo_inode -> direct2;
                array_data_blocks[2] = actual_pseudo_inode -> direct3;
                array_data_blocks[3] = actual_pseudo_inode -> direct4;
                array_data_blocks[4] = actual_pseudo_inode -> direct5;

                int i = 0;
                for(; i < 5; i++){ /* read direct */
                    if(numb_it_fol_untraver != 0){
                        int read_count = -1;
                        int j = 0;

                        if(numb_it_fol_untraver * sizeof(struct directory_item) <= FORMAT_CLUSTER_SIZE_B){ /* read just part of a cluster */
                            read_count = numb_it_fol_untraver;
                        }else{ /* read whole cluster */
                            read_count = FORMAT_CLUSTER_SIZE_B / sizeof(struct directory_item);
                        }

                        for(; j < read_count; j++){
                            int start_address = content_fs -> loaded_sb -> data_start_address + array_data_blocks[i] * FORMAT_CLUSTER_SIZE_B + (j * sizeof(struct directory_item));
                            set_pointer_position_fs(start_address, SEEK_SET);
                            struct directory_item *dir_item = (struct directory_item *) read_from_file_fs(sizeof(struct directory_item));
                            directory_items[free_index_directory_items] = *dir_item;
                            free(dir_item);
                            free_index_directory_items++;
                            numb_it_fol_untraver--;
                        }
                    }
                }
                close_file_fs();

                /* all directory items loaded, does wanted folder exist? */
                i = 0;
                int new_inode_number = -1;
                for(; i < free_index_directory_items; i++){
                    if(strcmp(directory_items[i].item_name, item_target_arr[x]) == 0){ /* inode with desired name has been found */
                        new_inode_number = directory_items[i].inode;
                        break;
                    }
                }

                if(new_inode_number != -1){
                    if(content_fs -> loaded_pseudo_in_arr[new_inode_number].isDirectory == true){
                        if(x == item_target_arr_length - 2){
                            pseudo_inode_target = &content_fs -> loaded_pseudo_in_arr[new_inode_number];
                        }else{
                            actual_pseudo_inode = &content_fs -> loaded_pseudo_in_arr[new_inode_number];
                        }
                    }else{
                        printf(FILE_NOT_FOUND);
                        error_occured = true;
                        free(directory_items);
                        break;
                    }
                }else{
                    printf(FILE_NOT_FOUND);
                    error_occured = true;
                    free(directory_items);
                    break;
                }
                free(directory_items);
            }
        }
    }else{ /* path is relative, go from current inode and try to traverse */
        if(item_target_arr_length == 1){
            pseudo_inode_target = actual_pseudo_inode_backup;
        }else{
            actual_pseudo_inode = actual_pseudo_inode_backup;

            int x = 0;
            for(; x < item_target_arr_length - 1; x++){
                open_rb_fs();
                int numb_it_fol_untraver = (actual_pseudo_inode -> file_size) / sizeof(struct directory_item); /* number of untraversed items in folder */
                struct directory_item *directory_items = malloc(sizeof(struct directory_item) * numb_it_fol_untraver);
                int free_index_directory_items = 0;
                int array_data_blocks[5];

                /* fill array of data block numbers for better traversing */
                array_data_blocks[0] = actual_pseudo_inode -> direct1;
                array_data_blocks[1] = actual_pseudo_inode -> direct2;
                array_data_blocks[2] = actual_pseudo_inode -> direct3;
                array_data_blocks[3] = actual_pseudo_inode -> direct4;
                array_data_blocks[4] = actual_pseudo_inode -> direct5;

                int i = 0;
                for(; i < 5; i++){ /* read direct */
                    if(numb_it_fol_untraver != 0){
                        int read_count = -1;
                        int j = 0;

                        if(numb_it_fol_untraver * sizeof(struct directory_item) <= FORMAT_CLUSTER_SIZE_B){ /* read just part of a cluster */
                            read_count = numb_it_fol_untraver;
                        }else{ /* read whole cluster */
                            read_count = FORMAT_CLUSTER_SIZE_B / sizeof(struct directory_item);
                        }

                        for(; j < read_count; j++){
                            int start_address = content_fs -> loaded_sb -> data_start_address + array_data_blocks[i] * FORMAT_CLUSTER_SIZE_B + (j * sizeof(struct directory_item));
                            set_pointer_position_fs(start_address, SEEK_SET);
                            struct directory_item *dir_item = (struct directory_item *) read_from_file_fs(sizeof(struct directory_item));
                            directory_items[free_index_directory_items] = *dir_item;
                            free(dir_item);
                            free_index_directory_items++;
                            numb_it_fol_untraver--;
                        }
                    }
                }
                close_file_fs();

                /* all directory items loaded, does wanted folder exist? */
                int new_inode_number = -1;
                bool in_root_upper = false;

                if(strlen(item_target_arr[x]) == 1 && item_target_arr[x][0] == '.'){
                    new_inode_number = directory_items[0].inode; /* first directory_item in folder is the folder itself */
                }else if(strlen(item_target_arr[x]) == 2 && item_target_arr[x][0] == '.' && item_target_arr[x][1] == '.'){
                    if(directory_items[0].item_name[0] == '/'){ /* user is in root, cannot move upper */
                        in_root_upper = true;
                        error_occured = true;
                    }else{
                        new_inode_number = directory_items[1].inode; /* second directory_item in folder is the upper folder */
                    }
                }else{
                    i = 0;
                    for(; i < free_index_directory_items; i++){
                        if(strcmp(directory_items[i].item_name, item_target_arr[x]) == 0){ /* inode with desired name has been found */
                            new_inode_number = directory_items[i].inode;
                            break;
                        }
                    }
                }

                if(new_inode_number != -1){
                    if(content_fs -> loaded_pseudo_in_arr[new_inode_number].isDirectory == true){
                        if(x == item_target_arr_length - 2){
                            pseudo_inode_target = &content_fs -> loaded_pseudo_in_arr[new_inode_number];
                        }else{
                            actual_pseudo_inode = &content_fs -> loaded_pseudo_in_arr[new_inode_number];
                        }
                    }else{
                        printf(FILE_NOT_FOUND);
                        error_occured = true;
                        free(directory_items);
                        break;
                    }
                }else{
                    if(in_root_upper == true){
                        printf(ROOT_FS_SEARCH, item_target_arr[x]);
                    }else{
                        printf(FILE_NOT_FOUND);
                    }
                    free(directory_items);
                    break;
                }
                free(directory_items);
            }
        }
    }
    /* find inode for target path - END */

    open_rb_fs();
    if(pseudo_inode_target == NULL){
        free(item_target_arr);
        return;
    }

    actual_pseudo_inode = pseudo_inode_target;
    int numb_it_fol_untraver = (actual_pseudo_inode -> file_size) / sizeof(struct directory_item); /* number of untraversed items in folder */
    struct directory_item *directory_items = malloc(sizeof(struct directory_item) * numb_it_fol_untraver);
    int free_index_directory_items = 0;
    int array_data_blocks[7];

    /* fill array of data block numbers for better traversing */
    array_data_blocks[0] = actual_pseudo_inode -> direct1;
    array_data_blocks[1] = actual_pseudo_inode -> direct2;
    array_data_blocks[2] = actual_pseudo_inode -> direct3;
    array_data_blocks[3] = actual_pseudo_inode -> direct4;
    array_data_blocks[4] = actual_pseudo_inode -> direct5;

    int i = 0;
    for(; i < 5; i++){ /* read direct */
        if(numb_it_fol_untraver != 0){
            int read_count = -1;
            int j = 0;

            if(numb_it_fol_untraver * sizeof(struct directory_item) <= FORMAT_CLUSTER_SIZE_B){ /* read just part of a cluster */
                read_count = numb_it_fol_untraver;
            }else{ /* read whole cluster */
                read_count = FORMAT_CLUSTER_SIZE_B / sizeof(struct directory_item);
            }

            for(; j < read_count; j++){
                int start_address = content_fs -> loaded_sb -> data_start_address + array_data_blocks[i] * FORMAT_CLUSTER_SIZE_B + (j * sizeof(struct directory_item));
                set_pointer_position_fs(start_address, SEEK_SET);
                struct directory_item *dir_item = (struct directory_item *) read_from_file_fs(sizeof(struct directory_item));
                directory_items[free_index_directory_items] = *dir_item;
                free(dir_item);
                free_index_directory_items++;
                numb_it_fol_untraver--;
            }
        }
    }

    /* all directory items loaded, does wanted folder exist? */
    i = 0;
    int new_inode_number = -1;
    if(load_upper == true){
        new_inode_number = directory_items[1].inode;
    }else{
        for(; i < free_index_directory_items; i++){
            if(strcmp(directory_items[i].item_name, item_target_arr[item_target_arr_length - 1]) == 0){ /* inode with desired name has been found */
                new_inode_number = directory_items[i].inode;
                break;
            }
        }
    }

    if(new_inode_number != -1){
        actual_pseudo_inode = &content_fs -> loaded_pseudo_in_arr[new_inode_number];
    }else{
        printf(CD_NOT_FOUND, item_target_arr[item_target_arr_length - 1]);
    }
    free(directory_items);
    free(item_target_arr);

    close_file_fs();
}
/* ____________________________________________________________________________
    void perform_pwd()

    Performs command pwd (syntax: pwd), which is used for printing
    actual path.
   ____________________________________________________________________________
*/
void perform_pwd(){
    struct pseudo_inode *actual_pseudo_inode_backup = actual_pseudo_inode; /* we will use perform_cd and switch inodes */
    struct directory_item *dir_items = NULL;
    int dir_items_length = 0;

    /* read actual folder */
    int start_address_name = (content_fs -> loaded_sb -> data_start_address) + (actual_pseudo_inode -> direct1 * FORMAT_CLUSTER_SIZE_B);
    open_rb_fs();
    set_pointer_position_fs(start_address_name, SEEK_SET);
    struct directory_item *dir_item_act = (struct directory_item *) read_from_file_fs(sizeof(struct directory_item));
    close_file_fs();
    dir_items = realloc(dir_items, (dir_items_length + 1) * sizeof(struct directory_item));
    dir_items[dir_items_length] = *dir_item_act;
    dir_items_length += 1;
    free(dir_item_act);

    while(true){ /* read upper folders */
        if(actual_pseudo_inode -> nodeid == 0){ /* root of fs is always located on first inode */
            break;
        }

        int start_address_name_upper = (content_fs -> loaded_sb -> data_start_address) + (actual_pseudo_inode -> direct1 * FORMAT_CLUSTER_SIZE_B) +
                sizeof(struct directory_item);
        open_rb_fs();
        set_pointer_position_fs(start_address_name_upper, SEEK_SET);
        struct directory_item *dir_item = (struct directory_item *) read_from_file_fs(sizeof(struct directory_item));
        close_file_fs();
        dir_items = realloc(dir_items, (dir_items_length + 1) * sizeof(struct directory_item));
        dir_items[dir_items_length] = *dir_item;
        dir_items_length += 1;

        perform_cd(dir_item -> item_name);
        free(dir_item);
    }

    printf(PWD_ACTUAL_PATH);
    int i = 0;
    for(; i < dir_items_length; i++){
        printf("%s", dir_items[dir_items_length - 1 - i].item_name);
        if(i != 0 &&  i != (dir_items_length - 1)){
            printf("/");
        }
    }
    printf("\n");

    free(dir_items);

    /* go back to previously traversed inode */
    actual_pseudo_inode = actual_pseudo_inode_backup;
}
/* ____________________________________________________________________________
    void perform_info(char *first_word)

    Performs command info (syntax: info d1/f1), which is used for printing
    info about file/directory f1/d1 (in which cluster is located).
   ____________________________________________________________________________
*/
void perform_info(char *first_word){
    struct pseudo_inode *actual_pseudo_inode_backup = actual_pseudo_inode; /* we will switch inodes */
    struct pseudo_inode *pseudo_inode_target = NULL;
    char (*item_target_arr)[15] = NULL;
    int item_target_arr_length = 0;

    bool error_occured = false; /* if true, then error occured */

    char *one_word = NULL;
    /* split target path by '/' to get individual folders - START */
    one_word = strtok(first_word, "/");
    while(one_word != NULL){
        /* check length of folder name */
        if(strlen(one_word) > 14){
            printf(RMDIR_EXCEED_LENGTH);
            if(item_target_arr_length != 0){
                free(item_target_arr);
            }
            return;
        }

        item_target_arr = realloc(item_target_arr, (item_target_arr_length + 1) * sizeof *item_target_arr);
        strcpy(item_target_arr[item_target_arr_length], one_word);
        one_word = strtok(NULL, "/"); /* move to next word */

        item_target_arr_length += 1;
    }
    /* split target path by '/' to get individual folders - END */

    /* find inode for target path - START */
    if(first_word[0] == '/'){ /* path is absolute, load first inode and continue traversing */
        if(item_target_arr_length == 1 && strlen(item_target_arr[0]) == 1 && item_target_arr[0][0] == '.'){
            pseudo_inode_target = actual_pseudo_inode_backup;
        }else{
            actual_pseudo_inode = &content_fs -> loaded_pseudo_in_arr[0];

            int x = 0;
            for(; x < item_target_arr_length - 1; x++){
                open_rb_fs();
                int numb_it_fol_untraver = (actual_pseudo_inode -> file_size) / sizeof(struct directory_item); /* number of untraversed items in folder */
                struct directory_item *directory_items = malloc(sizeof(struct directory_item) * numb_it_fol_untraver);
                int free_index_directory_items = 0;
                int array_data_blocks[5];

                /* fill array of data block numbers for better traversing */
                array_data_blocks[0] = actual_pseudo_inode -> direct1;
                array_data_blocks[1] = actual_pseudo_inode -> direct2;
                array_data_blocks[2] = actual_pseudo_inode -> direct3;
                array_data_blocks[3] = actual_pseudo_inode -> direct4;
                array_data_blocks[4] = actual_pseudo_inode -> direct5;

                int i = 0;
                for(; i < 5; i++){ /* read direct */
                    if(numb_it_fol_untraver != 0){
                        int read_count = -1;
                        int j = 0;

                        if(numb_it_fol_untraver * sizeof(struct directory_item) <= FORMAT_CLUSTER_SIZE_B){ /* read just part of a cluster */
                            read_count = numb_it_fol_untraver;
                        }else{ /* read whole cluster */
                            read_count = FORMAT_CLUSTER_SIZE_B / sizeof(struct directory_item);
                        }

                        for(; j < read_count; j++){
                            int start_address = content_fs -> loaded_sb -> data_start_address + array_data_blocks[i] * FORMAT_CLUSTER_SIZE_B + (j * sizeof(struct directory_item));
                            set_pointer_position_fs(start_address, SEEK_SET);
                            struct directory_item *dir_item = (struct directory_item *) read_from_file_fs(sizeof(struct directory_item));
                            directory_items[free_index_directory_items] = *dir_item;
                            free(dir_item);
                            free_index_directory_items++;
                            numb_it_fol_untraver--;
                        }
                    }
                }
                close_file_fs();

                /* all directory items loaded, does wanted folder exist? */
                i = 0;
                int new_inode_number = -1;
                for(; i < free_index_directory_items; i++){
                    if(strcmp(directory_items[i].item_name, item_target_arr[x]) == 0){ /* inode with desired name has been found */
                        new_inode_number = directory_items[i].inode;
                        break;
                    }
                }

                if(new_inode_number != -1){
                    if(content_fs -> loaded_pseudo_in_arr[new_inode_number].isDirectory == true){
                        if(x == item_target_arr_length - 2){
                            pseudo_inode_target = &content_fs -> loaded_pseudo_in_arr[new_inode_number];
                        }else{
                            actual_pseudo_inode = &content_fs -> loaded_pseudo_in_arr[new_inode_number];
                        }
                    }else{
                        printf(FILE_NOT_FOUND);
                        error_occured = true;
                        free(directory_items);
                        break;
                    }
                }else{
                    printf(FILE_NOT_FOUND);
                    error_occured = true;
                    free(directory_items);
                    break;
                }
                free(directory_items);
            }
        }
    }else{ /* path is relative, go from current inode and try to traverse */
        if(item_target_arr_length == 1){
            pseudo_inode_target = actual_pseudo_inode_backup;
        }else{
            actual_pseudo_inode = actual_pseudo_inode_backup;

            int x = 0;
            for(; x < item_target_arr_length - 1; x++){
                open_rb_fs();
                int numb_it_fol_untraver = (actual_pseudo_inode -> file_size) / sizeof(struct directory_item); /* number of untraversed items in folder */
                struct directory_item *directory_items = malloc(sizeof(struct directory_item) * numb_it_fol_untraver);
                int free_index_directory_items = 0;
                int array_data_blocks[5];

                /* fill array of data block numbers for better traversing */
                array_data_blocks[0] = actual_pseudo_inode -> direct1;
                array_data_blocks[1] = actual_pseudo_inode -> direct2;
                array_data_blocks[2] = actual_pseudo_inode -> direct3;
                array_data_blocks[3] = actual_pseudo_inode -> direct4;
                array_data_blocks[4] = actual_pseudo_inode -> direct5;

                int i = 0;
                for(; i < 5; i++){ /* read direct */
                    if(numb_it_fol_untraver != 0){
                        int read_count = -1;
                        int j = 0;

                        if(numb_it_fol_untraver * sizeof(struct directory_item) <= FORMAT_CLUSTER_SIZE_B){ /* read just part of a cluster */
                            read_count = numb_it_fol_untraver;
                        }else{ /* read whole cluster */
                            read_count = FORMAT_CLUSTER_SIZE_B / sizeof(struct directory_item);
                        }

                        for(; j < read_count; j++){
                            int start_address = content_fs -> loaded_sb -> data_start_address + array_data_blocks[i] * FORMAT_CLUSTER_SIZE_B + (j * sizeof(struct directory_item));
                            set_pointer_position_fs(start_address, SEEK_SET);
                            struct directory_item *dir_item = (struct directory_item *) read_from_file_fs(sizeof(struct directory_item));
                            directory_items[free_index_directory_items] = *dir_item;
                            free(dir_item);
                            free_index_directory_items++;
                            numb_it_fol_untraver--;
                        }
                    }
                }
                close_file_fs();

                /* all directory items loaded, does wanted folder exist? */
                int new_inode_number = -1;
                bool in_root_upper = false;

                if(strlen(item_target_arr[x]) == 1 && item_target_arr[x][0] == '.'){
                    new_inode_number = directory_items[0].inode; /* first directory_item in folder is the folder itself */
                }else if(strlen(item_target_arr[x]) == 2 && item_target_arr[x][0] == '.' && item_target_arr[x][1] == '.'){
                    if(directory_items[0].item_name[0] == '/'){ /* user is in root, cannot move upper */
                        in_root_upper = true;
                        error_occured = true;
                    }else{
                        new_inode_number = directory_items[1].inode; /* second directory_item in folder is the upper folder */
                    }
                }else{
                    i = 0;
                    for(; i < free_index_directory_items; i++){
                        if(strcmp(directory_items[i].item_name, item_target_arr[x]) == 0){ /* inode with desired name has been found */
                            new_inode_number = directory_items[i].inode;
                            break;
                        }
                    }
                }

                if(new_inode_number != -1){
                    if(content_fs -> loaded_pseudo_in_arr[new_inode_number].isDirectory == true){
                        if(x == item_target_arr_length - 2){
                            pseudo_inode_target = &content_fs -> loaded_pseudo_in_arr[new_inode_number];
                        }else{
                            actual_pseudo_inode = &content_fs -> loaded_pseudo_in_arr[new_inode_number];
                        }
                    }else{
                        printf(FILE_NOT_FOUND);
                        error_occured = true;
                        free(directory_items);
                        break;
                    }
                }else{
                    if(in_root_upper == true){
                        printf(ROOT_FS_SEARCH, item_target_arr[x]);
                    }else{
                        printf(FILE_NOT_FOUND);
                    }
                    free(directory_items);
                    break;
                }
                free(directory_items);
            }
        }
    }
    /* find inode for target path - END */

    open_rb_fs();

    if(pseudo_inode_target == NULL){
        free(item_target_arr);
        return;
    }

    actual_pseudo_inode = pseudo_inode_target;

    open_rb_fs();

    int numb_it_fol_untraver = (actual_pseudo_inode -> file_size) / sizeof(struct directory_item); /* number of untraversed items in folder */
    struct directory_item *directory_items = malloc(sizeof(struct directory_item) * numb_it_fol_untraver);
    int free_index_directory_items = 0;
    int array_data_blocks[7];

    /* fill array of data block numbers for better traversing */
    array_data_blocks[0] = actual_pseudo_inode -> direct1;
    array_data_blocks[1] = actual_pseudo_inode -> direct2;
    array_data_blocks[2] = actual_pseudo_inode -> direct3;
    array_data_blocks[3] = actual_pseudo_inode -> direct4;
    array_data_blocks[4] = actual_pseudo_inode -> direct5;
    array_data_blocks[5] = actual_pseudo_inode -> indirect1;
    array_data_blocks[6] = actual_pseudo_inode -> indirect2;

    int i = 0;
    for(; i < 5; i++){ /* read direct */
        if(numb_it_fol_untraver != 0){
            int read_count = -1;
            int j = 0;

            if(numb_it_fol_untraver * sizeof(struct directory_item) <= FORMAT_CLUSTER_SIZE_B){ /* read just part of a cluster */
                read_count = numb_it_fol_untraver;
            }else{ /* read whole cluster */
                read_count = FORMAT_CLUSTER_SIZE_B / sizeof(struct directory_item);
            }

            for(; j < read_count; j++){
                int start_address = content_fs -> loaded_sb -> data_start_address + array_data_blocks[i] * FORMAT_CLUSTER_SIZE_B + (j * sizeof(struct directory_item));
                set_pointer_position_fs(start_address, SEEK_SET);
                struct directory_item *dir_item = (struct directory_item *) read_from_file_fs(sizeof(struct directory_item));
                directory_items[free_index_directory_items] = *dir_item;
                free(dir_item);
                free_index_directory_items++;
                numb_it_fol_untraver--;
            }
        }
    }

    /* all directory items loaded, does wanted folder / item exist? */
    i = 0;
    int new_inode_number = -1;
    for(; i < free_index_directory_items; i++){
        if(strcmp(directory_items[i].item_name, item_target_arr[item_target_arr_length - 1]) == 0){ /* inode with desired name has been found */
            new_inode_number = directory_items[i].inode;
            break;
        }
    }

    if(new_inode_number != -1){ /* folder / file exists */
        struct pseudo_inode *pseudo_in;
        pseudo_in = &content_fs -> loaded_pseudo_in_arr[new_inode_number];

        printf(INFO_START, item_target_arr[item_target_arr_length - 1]);
        printf(INFO_INODE_NUM, pseudo_in -> nodeid);
        printf(INFO_NUM_REF, pseudo_in -> references);
        printf(INFO_SIZE, pseudo_in -> file_size);

        /* print info about type (file / directory) */
        if(pseudo_in -> isDirectory == true){
            printf(INFO_DIRECTORY);
        }else{
            printf(INFO_FILE);
        }

        int cluster_direct[5];
        /* printing direct cluster - START */
        cluster_direct[0] = pseudo_in -> direct1;
        cluster_direct[1] = pseudo_in -> direct2;
        cluster_direct[2] = pseudo_in -> direct3;
        cluster_direct[3] = pseudo_in -> direct4;
        cluster_direct[4] = pseudo_in -> direct5;

        i = 0;
        bool first_print = true;
        for(; i < 5; i++){
            if(cluster_direct[i] != -1){
                if(first_print == true){
                    printf(INFO_DIRECT_CLUS, cluster_direct[i]);
                    first_print = false;
                }else{
                    printf(", %d", cluster_direct[i]);
                }
            }
        }

        if(first_print == true){ /* no direct pointer assigned -> should not ever be printed... */
            printf(INFO_NO_DIRECT);
        }else{
            printf("\n"); /* add linebreak */
        }
        /* printing direct cluster - END */

        /* printing indirect cluster - START */
        int occupied_cluster_data = ceil((float) pseudo_in -> file_size / FORMAT_CLUSTER_SIZE_B); /* number of clusters reserved for data */
        int occupied_cluster_extra = 0; /* number of extra clusters (for indirect pointer list etc.) */
        /* extra clusters will be needed if indirect is going to be used */
        int indirect1_hold = FORMAT_CLUSTER_SIZE_B / sizeof(int);

        /* count extra clusters - START */
        if(occupied_cluster_data > 5){ /* direct pointers not enough, extra cluster for indirect1 */
            occupied_cluster_extra += 1;

            if (occupied_cluster_data > (5 + indirect1_hold)) { /* indirect2 */
                occupied_cluster_extra += 1; /* for first cluster list */

                int second_list_count = ceil((float) (occupied_cluster_data - (5 + indirect1_hold)) /
                                             indirect1_hold); /* needed cluster count / list capacity */
                occupied_cluster_extra += second_list_count;
            }
        }
        /* count extra clusters - END */

        if(occupied_cluster_extra > 0){ /* first extra cluster is a list for indirect1 */
            printf(INFO_INDIRECT1_LIST, pseudo_in -> indirect1);

            int loop_end;
            if((occupied_cluster_data - 5) > indirect1_hold){
                loop_end = indirect1_hold;
            }else{
                loop_end = occupied_cluster_data - 5;
            }
            int start_address = content_fs -> loaded_sb -> data_start_address + pseudo_in -> indirect1 * FORMAT_CLUSTER_SIZE_B;
            set_pointer_position_fs(start_address, SEEK_SET);
            int *clust_indir1_arr = (int *) read_from_file_fs(loop_end * sizeof(int));

            first_print = true;
            i = 0;
            for(; i < loop_end; i++){
                if(first_print == true){
                    printf(INFO_OCCUPIED_INDIRECT1, clust_indir1_arr[i]);
                    first_print = false;
                }else{
                    printf(", %d", clust_indir1_arr[i]);
                }
            }

            free(clust_indir1_arr);

            if(first_print == true){ /* no direct pointer assigned -> should not ever be printed... */
                printf(INFO_INDIRECT1_NOT_ASSIGNED);
            }else{
                printf("\n"); /* add linebreak */
            }

            if(occupied_cluster_extra > 1){ /* second extra cluster is a first list for indirect2 */
                printf("Indirect2 first list with clusters is on: %d cluster\n", pseudo_in -> indirect2);

                int second_list_count = ceil((float) (occupied_cluster_data - 5 - indirect1_hold) / indirect1_hold);
                int start_address = content_fs -> loaded_sb -> data_start_address + pseudo_in -> indirect2 * FORMAT_CLUSTER_SIZE_B;
                set_pointer_position_fs(start_address, SEEK_SET);
                int *second_list_arr = (int *) read_from_file_fs(sizeof(int) * second_list_count);

                i = 0;
                for(; i < second_list_count; i++){
                    printf("Indirect2 second list number %d with clusters is on: %d cluster\n", (i + 1), second_list_arr[i]);
                    int start_address_sec_list = content_fs -> loaded_sb -> data_start_address + second_list_arr[i] * FORMAT_CLUSTER_SIZE_B;
                    set_pointer_position_fs(start_address_sec_list, SEEK_SET);
                    int *clusters_arr;
                    if((occupied_cluster_data - 5 - indirect1_hold - (indirect1_hold * i)) < indirect1_hold){
                        loop_end = occupied_cluster_data - 5 - indirect1_hold - (indirect1_hold * i);
                    }else{
                        loop_end = indirect1_hold;
                    }
                    clusters_arr = read_from_file_fs(loop_end * sizeof(int));

                    int j = 0;
                    first_print = true;
                    for(; j < loop_end; j++){
                        if(first_print == true){
                            printf("Occupied clusters: %d", clusters_arr[j]);
                            first_print = false;
                        }else{
                            printf(", %d", clusters_arr[j]);
                        }
                    }
                    free(clusters_arr);

                    if(first_print == true){ /* no indirect pointer assigned -> should not ever be printed... */
                        printf("No indirect2 second list assigned... \n");
                    }else{
                        printf("\n"); /* add linebreak */
                    }
                }
                free(second_list_arr);
            }
        }

        /* printing indirect cluster - END*/
        printf("Info about inode named: %s - END\n", item_target_arr[item_target_arr_length - 1]);
    }else{
        printf("Folder or file with name: \"%s\" was not found in current directory...\n", first_word);
    }
    free(directory_items);
    free(item_target_arr);
    close_file_fs();

    actual_pseudo_inode = actual_pseudo_inode_backup;
}
/* ____________________________________________________________________________
    void perform_incp(char *first_word, char *second_word)

    Performs command incp (syntax: incp f1 f2), which is used for copying
    file f1 to location f2 in pseudoNTFS.
   ____________________________________________________________________________
*/
void perform_incp(char *first_word, char *second_word){
    struct pseudo_inode *actual_pseudo_inode_backup = actual_pseudo_inode; /* we will switch inodes */
    struct pseudo_inode *pseudo_inode_target = NULL; /* inode of folder to which the file will be copied */

    char (*item_target_arr)[15] = NULL; /* array of item names which are in the path of target */
    int item_target_arr_length = 0;

    bool error_occured = false; /* if true, then error occured (not enough clusters, etc.) -> file cannot be copied */

    char *one_word = NULL;
    /* split target path by '/' to get individual folders - START */
    one_word = strtok(second_word, "/");
    while(one_word != NULL) {
        item_target_arr = realloc(item_target_arr, (item_target_arr_length + 1) * sizeof *item_target_arr);
        strcpy(item_target_arr[item_target_arr_length], one_word);
        one_word = strtok(NULL, "/"); /* move to next word */

        item_target_arr_length += 1;
    }
    /* split target path by '/' to get individual folders - END */

    /* find inode for target path - START */
    if(second_word[0] == '/'){ /* path is absolute, load first inode and continue traversing */
        if(item_target_arr_length == 1 && strlen(item_target_arr[0]) == 1 && item_target_arr[0][0] == '.'){
            pseudo_inode_target = actual_pseudo_inode_backup;
        }else{
            actual_pseudo_inode = &content_fs -> loaded_pseudo_in_arr[0];

            int x = 0;
            for(; x < item_target_arr_length - 1; x++){ /* -1 because last word should be name of the file which is not yet created */
                open_rb_fs();
                int numb_it_fol_untraver = (actual_pseudo_inode -> file_size) / sizeof(struct directory_item); /* number of untraversed items in folder */
                struct directory_item *directory_items = malloc(sizeof(struct directory_item) * numb_it_fol_untraver);
                int free_index_directory_items = 0;
                int array_data_blocks[5];

                /* fill array of data block numbers for better traversing */
                array_data_blocks[0] = actual_pseudo_inode -> direct1;
                array_data_blocks[1] = actual_pseudo_inode -> direct2;
                array_data_blocks[2] = actual_pseudo_inode -> direct3;
                array_data_blocks[3] = actual_pseudo_inode -> direct4;
                array_data_blocks[4] = actual_pseudo_inode -> direct5;

                int i = 0;
                for(; i < 5; i++){ /* read direct */
                    if(numb_it_fol_untraver != 0){
                        int read_count = -1;
                        int j = 0;

                        if(numb_it_fol_untraver * sizeof(struct directory_item) <= FORMAT_CLUSTER_SIZE_B){ /* read just part of a cluster */
                            read_count = numb_it_fol_untraver;
                        }else{ /* read whole cluster */
                            read_count = FORMAT_CLUSTER_SIZE_B / sizeof(struct directory_item);
                        }

                        for(; j < read_count; j++){
                            int start_address = content_fs -> loaded_sb -> data_start_address + array_data_blocks[i] * FORMAT_CLUSTER_SIZE_B + (j * sizeof(struct directory_item));
                            set_pointer_position_fs(start_address, SEEK_SET);
                            struct directory_item *dir_item = (struct directory_item *) read_from_file_fs(sizeof(struct directory_item));
                            directory_items[free_index_directory_items] = *dir_item;
                            free(dir_item);
                            free_index_directory_items++;
                            numb_it_fol_untraver--;
                        }
                    }
                }
                close_file_fs();

                /* all directory items loaded, does wanted folder exist? */
                i = 0;
                int new_inode_number = -1;
                for(; i < free_index_directory_items; i++){
                    if(strcmp(directory_items[i].item_name, item_target_arr[x]) == 0){ /* inode with desired name has been found */
                        new_inode_number = directory_items[i].inode;
                        break;
                    }
                }

                if(new_inode_number != -1){
                    if(content_fs -> loaded_pseudo_in_arr[new_inode_number].isDirectory == true){
                        if(x == item_target_arr_length - 2){
                            pseudo_inode_target = &content_fs -> loaded_pseudo_in_arr[new_inode_number];
                        }else{
                            actual_pseudo_inode = &content_fs -> loaded_pseudo_in_arr[new_inode_number];
                        }
                    }else{
                        printf(FILE_NOT_FOUND);
                        error_occured = true;
                        free(directory_items);
                        break;
                    }
                }else{
                    printf(FILE_NOT_FOUND);
                    error_occured = true;
                    free(directory_items);
                    break;
                }
                free(directory_items);
            }
        }
    }else{ /* path is relative, go from current inode and try to traverse */
        if(item_target_arr_length == 1){
            pseudo_inode_target = actual_pseudo_inode_backup;
        }else{
            actual_pseudo_inode = actual_pseudo_inode_backup;

            int x = 0;
            for(; x < item_target_arr_length - 1; x++){
                open_rb_fs();
                int numb_it_fol_untraver = (actual_pseudo_inode -> file_size) / sizeof(struct directory_item); /* number of untraversed items in folder */
                struct directory_item *directory_items = malloc(sizeof(struct directory_item) * numb_it_fol_untraver);
                int free_index_directory_items = 0;
                int array_data_blocks[5];

                /* fill array of data block numbers for better traversing */
                array_data_blocks[0] = actual_pseudo_inode -> direct1;
                array_data_blocks[1] = actual_pseudo_inode -> direct2;
                array_data_blocks[2] = actual_pseudo_inode -> direct3;
                array_data_blocks[3] = actual_pseudo_inode -> direct4;
                array_data_blocks[4] = actual_pseudo_inode -> direct5;

                int i = 0;
                for(; i < 5; i++){ /* read direct */
                    if(numb_it_fol_untraver != 0){
                        int read_count = -1;
                        int j = 0;

                        if(numb_it_fol_untraver * sizeof(struct directory_item) <= FORMAT_CLUSTER_SIZE_B){ /* read just part of a cluster */
                            read_count = numb_it_fol_untraver;
                        }else{ /* read whole cluster */
                            read_count = FORMAT_CLUSTER_SIZE_B / sizeof(struct directory_item);
                        }

                        for(; j < read_count; j++){
                            int start_address = content_fs -> loaded_sb -> data_start_address + array_data_blocks[i] * FORMAT_CLUSTER_SIZE_B + (j * sizeof(struct directory_item));
                            set_pointer_position_fs(start_address, SEEK_SET);
                            struct directory_item *dir_item = (struct directory_item *) read_from_file_fs(sizeof(struct directory_item));
                            directory_items[free_index_directory_items] = *dir_item;
                            free(dir_item);
                            free_index_directory_items++;
                            numb_it_fol_untraver--;
                        }
                    }
                }
                close_file_fs();

                /* all directory items loaded, does wanted folder exist? */
                int new_inode_number = -1;
                bool in_root_upper = false;

                if(strlen(item_target_arr[x]) == 1 && item_target_arr[x][0] == '.'){
                    new_inode_number = directory_items[0].inode; /* first directory_item in folder is the folder itself */
                }else if(strlen(item_target_arr[x]) == 2 && item_target_arr[x][0] == '.' && item_target_arr[x][1] == '.'){
                    if(directory_items[0].item_name[0] == '/'){ /* user is in root, cannot move upper */
                        in_root_upper = true;
                        error_occured = true;
                    }else{
                        new_inode_number = directory_items[1].inode; /* second directory_item in folder is the upper folder */
                    }
                }else{
                    i = 0;
                    for(; i < free_index_directory_items; i++){
                        if(strcmp(directory_items[i].item_name, item_target_arr[x]) == 0){ /* inode with desired name has been found */
                            new_inode_number = directory_items[i].inode;
                            break;
                        }
                    }
                }

                if(new_inode_number != -1){
                    if(content_fs -> loaded_pseudo_in_arr[new_inode_number].isDirectory == true){
                        if(x == item_target_arr_length - 2){
                            pseudo_inode_target = &content_fs -> loaded_pseudo_in_arr[new_inode_number];
                        }else{
                            actual_pseudo_inode = &content_fs -> loaded_pseudo_in_arr[new_inode_number];
                        }
                    }else{
                        printf(FILE_NOT_FOUND);
                        error_occured = true;
                        free(directory_items);
                        break;
                    }
                }else{
                    if(in_root_upper == true){
                        printf(ROOT_FS_SEARCH, item_target_arr[x]);
                    }else{
                        printf(FILE_NOT_FOUND);
                    }
                    free(directory_items);
                    break;
                }
                free(directory_items);
            }
        }
    }
    /* find inode for target path - END */

    actual_pseudo_inode = pseudo_inode_target;

    if(pseudo_inode_target == NULL){
        free(item_target_arr);
        return;
    }

    open_wb_add_fs();
    /* GET RIGHT INODE!! */
    /* check file existence */
    if(check_file_existence(first_word) == false){
        printf(INCP_FILE_DOESNT_EXIST, first_word);
        return;
    }

    /* check filename length */
    if(strlen(first_word) > INCP_MAX_FNAME_LENGTH){
        printf(INCP_EXCEED_LENGTH);
        return;
    }

    /* get free inode */
    struct pseudo_inode *free_pseudo_inode = NULL;
    int free_pseudo_inode_number = -1;
    int total_inode_count = (content_fs -> loaded_bit_in -> inodes_free_total) + (content_fs -> loaded_bit_in -> inodes_occupied_total);

    int i = 0;
    for(; i < total_inode_count; i++){
        if(content_fs -> loaded_bit_in -> inodes_bitmap[i] == 0){ /* free inode on "i" position found */
            free_pseudo_inode = &content_fs -> loaded_pseudo_in_arr[i]; /* (index is the same as with bitmap) */
            free_pseudo_inode_number = i;
            break;
        }
    }

    /* no free inode found */
    if(free_pseudo_inode_number == -1){
        printf(INCP_NO_INODE);
        return;
    }

    /* get free clusters */
    long file_size = get_file_size(first_word);
    float cluster_f = (float) file_size / FORMAT_CLUSTER_SIZE_B;
    int cluster_count_d = ceil(cluster_f); /* number of clusters needed for file */

    /* extra clusters will be needed if indirect is going to be used */
    int indirect1_hold = FORMAT_CLUSTER_SIZE_B / sizeof(int); /* cluster will contain just numbers of other clusters */

    int extra_cluster = 0;
    if(cluster_count_d > 5){ /* direct pointers not enough, extra cluster for indirect1 */
        extra_cluster += 1;

        if(cluster_count_d > (5 + indirect1_hold)){ /* indirect2 */
            extra_cluster += 1; /* for first cluster list */

            int second_list_count = ceil((float) (cluster_count_d - (5 + indirect1_hold)) / indirect1_hold); /* needed cluster count / list capacity */
            extra_cluster += second_list_count;
        }
    }
    cluster_count_d += extra_cluster;

    struct free_clusters_struct *free_clusters = get_free_cluster_arr(cluster_count_d);

    /* not enough clusters */
    if(free_clusters == NULL){
        printf(INCP_NO_CLUSTER);
    }

    void *file_content = NULL;

    i = 0;
    for(; i < (free_clusters -> free_clusters_arr_length - extra_cluster); i++){
        int start_address = (content_fs -> loaded_sb -> data_start_address) + (free_clusters -> free_clusters_arr[i]) * FORMAT_CLUSTER_SIZE_B;
        set_pointer_position_fs(start_address, SEEK_SET);

        if(i == (free_clusters -> free_clusters_arr_length - extra_cluster) - 1){ /* filling last assigned cluster */
            file_content = read_file_extern(first_word, i * FORMAT_CLUSTER_SIZE_B, (i * FORMAT_CLUSTER_SIZE_B) + file_size - ((free_clusters -> free_clusters_arr_length - extra_cluster) - 1) * FORMAT_CLUSTER_SIZE_B);
            write_to_file_fs(file_content, file_size - ((free_clusters -> free_clusters_arr_length - extra_cluster) - 1) * FORMAT_CLUSTER_SIZE_B);
        }else{
            file_content = read_file_extern(first_word, i * FORMAT_CLUSTER_SIZE_B, (i * FORMAT_CLUSTER_SIZE_B) + FORMAT_CLUSTER_SIZE_B);
            write_to_file_fs(file_content, FORMAT_CLUSTER_SIZE_B);
        }

        free(file_content);
    }

    free_pseudo_inode -> file_size = file_size;

    /* assign clusters to inode - START */
    /* assign direct */
    int *cluster_direct[5];

    cluster_direct[0] = &free_pseudo_inode -> direct1;
    cluster_direct[1] = &free_pseudo_inode -> direct2;
    cluster_direct[2] = &free_pseudo_inode -> direct3;
    cluster_direct[3] = &free_pseudo_inode -> direct4;
    cluster_direct[4] = &free_pseudo_inode -> direct5;

    int end;
    if(file_size <= (FORMAT_CLUSTER_SIZE_B * 5)){
        end = ceil((float) file_size / FORMAT_CLUSTER_SIZE_B);
    }else{
        end = 5;
    }

    i = 0;
    for(; i < end; i++){
        *cluster_direct[i] = free_clusters -> free_clusters_arr[i];
    }

    /* assign indirect */
    if(extra_cluster > 0){ /* first extra cluster is a list for indirect1 */
        int *indirect1_arr = NULL; /* array of clusters which will be saved on indirect1 */
        int rem_data_clus = cluster_count_d - extra_cluster - 5; /* clusters with data count, not assigned */

        int num_items;
        if(rem_data_clus < indirect1_hold){ /* less unassigned than cluster is able to hold => indirect2 will not be used */
            num_items = rem_data_clus;
        }else{
            num_items = indirect1_hold;
        }

        indirect1_arr = malloc(sizeof(int) * num_items);

        i = 0;
        for(; i < num_items; i++){ /* fill indirect1 array */
            indirect1_arr[i] = free_clusters -> free_clusters_arr[5 + i]; /* 5*direct */
        }

        /* array filled, now write data to first extra cluster */
        int indirect1_list_clust = free_clusters -> free_clusters_arr[free_clusters -> free_clusters_arr_length - 1 - extra_cluster + 1]; /* find first extra cluster */
        free_pseudo_inode -> indirect1 = indirect1_list_clust;
        int indirect1_list_start_addr = (content_fs -> loaded_sb -> data_start_address) + (indirect1_list_clust * FORMAT_CLUSTER_SIZE_B);
        set_pointer_position_fs(indirect1_list_start_addr, SEEK_SET);
        write_to_file_fs((void *) indirect1_arr, sizeof(int) * num_items);
        rem_data_clus -= num_items; /* update unassigned cluster count */
        free(indirect1_arr);

        if(extra_cluster > 1){ /* second extra cluster is a first list for indirect2 */
            /* fill second list for indirect2 - START */
            int second_list_count = ceil((float) (rem_data_clus) / indirect1_hold); /* needed cluster count / list capacity */
            int unwritten_size = file_size - ((5 + num_items) * FORMAT_CLUSTER_SIZE_B); /* 5 for direct + first indirect */

            i = 0;
            for(; i < second_list_count; i++){
                int indirect2_sec_list_clust = free_clusters -> free_clusters_arr[free_clusters -> free_clusters_arr_length - 1 - extra_cluster + 3 + i]; /* find address for extra cluster */
                int indirect2_sec_list_start_addr = (content_fs -> loaded_sb -> data_start_address) + (indirect2_sec_list_clust * FORMAT_CLUSTER_SIZE_B);
                set_pointer_position_fs(indirect2_sec_list_start_addr, SEEK_SET);

                if(i == (second_list_count - 1)){ /* in case of last cluster write */
                    write_to_file_fs((void *) free_clusters -> free_clusters_arr + (5 + indirect1_hold + (i * indirect1_hold)) *
                                                                                           sizeof(int), rem_data_clus * sizeof(int));
                    //unwritten_size = 0;
                    rem_data_clus = 0;
                }else{
                    write_to_file_fs((void *) free_clusters -> free_clusters_arr + (5 + indirect1_hold + (i * indirect1_hold)) *
                                                                                           sizeof(int), indirect1_hold * sizeof(int));
                    //unwritten_size -= FORMAT_CLUSTER_SIZE_B;
                    rem_data_clus -= indirect1_hold;
                }
            }
            /* fill second list for indirect2 - END */

            /* fill first list for indirect2 - START */
            int indirect2_first_list_clust = free_clusters -> free_clusters_arr[free_clusters -> free_clusters_arr_length - 1 - extra_cluster + 2]; /* find address for extra cluster */
            free_pseudo_inode -> indirect2 = indirect2_first_list_clust;
            int indirect2_first_list_start_addr = (content_fs -> loaded_sb -> data_start_address) + (indirect2_first_list_clust * FORMAT_CLUSTER_SIZE_B);
            set_pointer_position_fs(indirect2_first_list_start_addr, SEEK_SET);

            write_to_file_fs((void *) free_clusters -> free_clusters_arr + (free_clusters -> free_clusters_arr_length - second_list_count) * sizeof(int),
                             sizeof(int) * second_list_count);
            /* fill first list for indirect1 - END */
        }
    }
    /* assign clusters to inode - END */

    /* assign new file to actual folder - START */
    struct directory_item *file_written = malloc(sizeof(struct directory_item));
    memset(file_written, 0, sizeof(struct directory_item));
    strcpy(file_written -> item_name, item_target_arr[item_target_arr_length - 1]);
    file_written -> inode = free_pseudo_inode_number;

    int needed_size = actual_pseudo_inode -> file_size + sizeof(struct directory_item);
    int one_cluster_count = FORMAT_CLUSTER_SIZE_B / sizeof(struct directory_item); /* count of directory items which is one cluster able to hold */
    int start_address = -1;
    bool new_cluster_alloc = false; /* will be true if we recently tried to assign a new cluster */
    int free_clus_num = -1; /* will contain number of newly assigned cluster, -1 if no free cluster has been found */

    if(needed_size <= (one_cluster_count * sizeof(struct directory_item))){ /* direct 1 has enough space to store */
        start_address = (content_fs -> loaded_sb -> data_start_address) + (actual_pseudo_inode -> direct1) * FORMAT_CLUSTER_SIZE_B + actual_pseudo_inode -> file_size;
    }else if(needed_size <= (one_cluster_count * sizeof(struct directory_item)) * 2){ /* direct2 */
        if(actual_pseudo_inode -> direct2 == -1){ /* cluster was not reserved yet */
            new_cluster_alloc = true;
            free_clus_num = get_free_cluster();
            actual_pseudo_inode -> direct2 = free_clus_num;
        }
        start_address = (content_fs -> loaded_sb -> data_start_address) + (actual_pseudo_inode -> direct2) * FORMAT_CLUSTER_SIZE_B + (actual_pseudo_inode -> file_size) - (one_cluster_count * sizeof(struct directory_item));
    }else if(needed_size <= (one_cluster_count * sizeof(struct directory_item)) * 3){ /* direct3 */
        if(actual_pseudo_inode -> direct3 == -1){ /* cluster was not reserved yet */
            new_cluster_alloc = true;
            free_clus_num = get_free_cluster();
            actual_pseudo_inode -> direct3 = free_clus_num;
        }
        start_address = (content_fs -> loaded_sb -> data_start_address) + (actual_pseudo_inode -> direct3) * FORMAT_CLUSTER_SIZE_B + (actual_pseudo_inode -> file_size) - 2 * (one_cluster_count * sizeof(struct directory_item));
    }else if(needed_size <= (one_cluster_count * sizeof(struct directory_item)) * 4){ /* direct4 */
        if(actual_pseudo_inode -> direct4 == -1){ /* cluster was not reserved yet */
            new_cluster_alloc = true;
            free_clus_num = get_free_cluster();
            actual_pseudo_inode -> direct4 = free_clus_num;
        }
        start_address = (content_fs -> loaded_sb -> data_start_address) + (actual_pseudo_inode -> direct4) * FORMAT_CLUSTER_SIZE_B + (actual_pseudo_inode -> file_size) - 3 * (one_cluster_count * sizeof(struct directory_item));
    }else if(needed_size <= (one_cluster_count * sizeof(struct directory_item)) * 5){ /* direct5 */
        if(actual_pseudo_inode -> direct5 == -1){ /* cluster was not reserved yet */
            new_cluster_alloc = true;
            free_clus_num = get_free_cluster();
            actual_pseudo_inode -> direct5 = free_clus_num;
        }
        start_address = (content_fs -> loaded_sb -> data_start_address) + (actual_pseudo_inode -> direct5) * FORMAT_CLUSTER_SIZE_B + (actual_pseudo_inode -> file_size) - 4 * (one_cluster_count * sizeof(struct directory_item));
    }
    if(new_cluster_alloc == true){ /* no free cluster found */
        if(free_clus_num == -1){
            printf(MKDIR_NO_CLUSTER);
            return;
        }else{ /* update bitmap with cluster state */
            content_fs -> loaded_bit_clus -> cluster_occupied_total++;
            content_fs -> loaded_bit_clus -> cluster_free_total--;
            content_fs -> loaded_bit_clus -> cluster_bitmap[free_clus_num] = 1;
            save_bitmap_clusters(content_fs -> loaded_bit_clus);
        }
    }

    set_pointer_position_fs(start_address, SEEK_SET);
    write_to_file_fs(file_written, sizeof(struct directory_item));
    free(file_written);

    /* update file size of folder */
    actual_pseudo_inode -> file_size = actual_pseudo_inode -> file_size + sizeof(struct directory_item);

    save_pseudo_inodes(content_fs -> loaded_pseudo_in_arr, (content_fs -> loaded_bit_in -> inodes_free_total + content_fs -> loaded_bit_in -> inodes_occupied_total),
                       (content_fs -> loaded_bit_clus -> cluster_free_total + content_fs -> loaded_bit_clus -> cluster_occupied_total),
                       (content_fs -> loaded_bit_in -> inodes_free_total + content_fs -> loaded_bit_in -> inodes_occupied_total));
    /* assign new file to actual folder - END */

    /* update bitmap with cluster state */
    i = 0;
    for(; i < free_clusters -> free_clusters_arr_length; i++){
        content_fs -> loaded_bit_clus -> cluster_occupied_total++;
        content_fs -> loaded_bit_clus -> cluster_free_total--;
        content_fs -> loaded_bit_clus -> cluster_bitmap[free_clusters -> free_clusters_arr[i]] = 1;
    }
    save_bitmap_clusters(content_fs -> loaded_bit_clus);

     /* update bitmap with inode state */
    content_fs -> loaded_bit_in -> inodes_occupied_total++;
    content_fs -> loaded_bit_in -> inodes_free_total--;
    content_fs -> loaded_bit_in -> inodes_bitmap[free_pseudo_inode_number] = 1;
    save_bitmap_inodes(content_fs -> loaded_bit_in, (content_fs -> loaded_bit_clus -> cluster_occupied_total + content_fs -> loaded_bit_clus -> cluster_free_total));

    free(free_clusters -> free_clusters_arr);
    free(free_clusters);
    free(item_target_arr);
    close_file_fs();

    actual_pseudo_inode = actual_pseudo_inode_backup;
}
/* ____________________________________________________________________________
    void perform_outcp(char *first_word, char *second_word)

    Performs command outcp (syntax: outcp f1 f2), which is used for copying
    file f1 from pseudoNTFS to location f2 on hard drive.
   ____________________________________________________________________________
*/
void perform_outcp(char *first_word, char *second_word){
    struct pseudo_inode *actual_pseudo_inode_backup = actual_pseudo_inode; /* we will switch inodes */
    struct pseudo_inode *pseudo_inode_target = NULL;
    char (*item_target_arr)[15] = NULL;
    int item_target_arr_length = 0;

    bool error_occured = false; /* if true, then error occured */
    char *one_word = NULL;
    /* split target path by '/' to get individual folders - START */
    one_word = strtok(first_word, "/");
    while(one_word != NULL){
        /* check length of folder name */
        if(strlen(one_word) > 14){
            printf(RMDIR_EXCEED_LENGTH);
            if(item_target_arr_length != 0){
                free(item_target_arr);
            }
            return;
        }

        item_target_arr = realloc(item_target_arr, (item_target_arr_length + 1) * sizeof *item_target_arr);
        strcpy(item_target_arr[item_target_arr_length], one_word);
        one_word = strtok(NULL, "/"); /* move to next word */

        item_target_arr_length += 1;
    }
    /* split target path by '/' to get individual folders - END */

    /* find inode for target path - START */
    if(first_word[0] == '/'){ /* path is absolute, load first inode and continue traversing */
        if(item_target_arr_length == 1 && strlen(item_target_arr[0]) == 1 && item_target_arr[0][0] == '.'){
            pseudo_inode_target = actual_pseudo_inode_backup;
        }else{
            actual_pseudo_inode = &content_fs -> loaded_pseudo_in_arr[0];

            int x = 0;
            for(; x < item_target_arr_length - 1; x++){
                open_rb_fs();
                int numb_it_fol_untraver = (actual_pseudo_inode -> file_size) / sizeof(struct directory_item); /* number of untraversed items in folder */
                struct directory_item *directory_items = malloc(sizeof(struct directory_item) * numb_it_fol_untraver);
                int free_index_directory_items = 0;
                int array_data_blocks[5];

                /* fill array of data block numbers for better traversing */
                array_data_blocks[0] = actual_pseudo_inode -> direct1;
                array_data_blocks[1] = actual_pseudo_inode -> direct2;
                array_data_blocks[2] = actual_pseudo_inode -> direct3;
                array_data_blocks[3] = actual_pseudo_inode -> direct4;
                array_data_blocks[4] = actual_pseudo_inode -> direct5;

                int i = 0;
                for(; i < 5; i++){ /* read direct */
                    if(numb_it_fol_untraver != 0){
                        int read_count = -1;
                        int j = 0;

                        if(numb_it_fol_untraver * sizeof(struct directory_item) <= FORMAT_CLUSTER_SIZE_B){ /* read just part of a cluster */
                            read_count = numb_it_fol_untraver;
                        }else{ /* read whole cluster */
                            read_count = FORMAT_CLUSTER_SIZE_B / sizeof(struct directory_item);
                        }

                        for(; j < read_count; j++){
                            int start_address = content_fs -> loaded_sb -> data_start_address + array_data_blocks[i] * FORMAT_CLUSTER_SIZE_B + (j * sizeof(struct directory_item));
                            set_pointer_position_fs(start_address, SEEK_SET);
                            struct directory_item *dir_item = (struct directory_item *) read_from_file_fs(sizeof(struct directory_item));
                            directory_items[free_index_directory_items] = *dir_item;
                            free(dir_item);
                            free_index_directory_items++;
                            numb_it_fol_untraver--;
                        }
                    }
                }
                close_file_fs();

                /* all directory items loaded, does wanted folder exist? */
                i = 0;
                int new_inode_number = -1;
                for(; i < free_index_directory_items; i++){
                    if(strcmp(directory_items[i].item_name, item_target_arr[x]) == 0){ /* inode with desired name has been found */
                        new_inode_number = directory_items[i].inode;
                        break;
                    }
                }

                if(new_inode_number != -1){
                    if(content_fs -> loaded_pseudo_in_arr[new_inode_number].isDirectory == true){
                        if(x == item_target_arr_length - 2){
                            pseudo_inode_target = &content_fs -> loaded_pseudo_in_arr[new_inode_number];
                        }else{
                            actual_pseudo_inode = &content_fs -> loaded_pseudo_in_arr[new_inode_number];
                        }
                    }else{
                        printf(FILE_NOT_FOUND);
                        error_occured = true;
                        free(directory_items);
                        break;
                    }
                }else{
                    printf(FILE_NOT_FOUND);
                    error_occured = true;
                    free(directory_items);
                    break;
                }
                free(directory_items);
            }
        }
    }else{ /* path is relative, go from current inode and try to traverse */
        if(item_target_arr_length == 1){
            pseudo_inode_target = actual_pseudo_inode_backup;
        }else{
            actual_pseudo_inode = actual_pseudo_inode_backup;

            int x = 0;
            for(; x < item_target_arr_length - 1; x++){
                open_rb_fs();
                int numb_it_fol_untraver = (actual_pseudo_inode -> file_size) / sizeof(struct directory_item); /* number of untraversed items in folder */
                struct directory_item *directory_items = malloc(sizeof(struct directory_item) * numb_it_fol_untraver);
                int free_index_directory_items = 0;
                int array_data_blocks[5];

                /* fill array of data block numbers for better traversing */
                array_data_blocks[0] = actual_pseudo_inode -> direct1;
                array_data_blocks[1] = actual_pseudo_inode -> direct2;
                array_data_blocks[2] = actual_pseudo_inode -> direct3;
                array_data_blocks[3] = actual_pseudo_inode -> direct4;
                array_data_blocks[4] = actual_pseudo_inode -> direct5;

                int i = 0;
                for(; i < 5; i++){ /* read direct */
                    if(numb_it_fol_untraver != 0){
                        int read_count = -1;
                        int j = 0;

                        if(numb_it_fol_untraver * sizeof(struct directory_item) <= FORMAT_CLUSTER_SIZE_B){ /* read just part of a cluster */
                            read_count = numb_it_fol_untraver;
                        }else{ /* read whole cluster */
                            read_count = FORMAT_CLUSTER_SIZE_B / sizeof(struct directory_item);
                        }

                        for(; j < read_count; j++){
                            int start_address = content_fs -> loaded_sb -> data_start_address + array_data_blocks[i] * FORMAT_CLUSTER_SIZE_B + (j * sizeof(struct directory_item));
                            set_pointer_position_fs(start_address, SEEK_SET);
                            struct directory_item *dir_item = (struct directory_item *) read_from_file_fs(sizeof(struct directory_item));
                            directory_items[free_index_directory_items] = *dir_item;
                            free(dir_item);
                            free_index_directory_items++;
                            numb_it_fol_untraver--;
                        }
                    }
                }
                close_file_fs();

                /* all directory items loaded, does wanted folder exist? */
                int new_inode_number = -1;
                bool in_root_upper = false;

                if(strlen(item_target_arr[x]) == 1 && item_target_arr[x][0] == '.'){
                    new_inode_number = directory_items[0].inode; /* first directory_item in folder is the folder itself */
                }else if(strlen(item_target_arr[x]) == 2 && item_target_arr[x][0] == '.' && item_target_arr[x][1] == '.'){
                    if(directory_items[0].item_name[0] == '/'){ /* user is in root, cannot move upper */
                        in_root_upper = true;
                        error_occured = true;
                    }else{
                        new_inode_number = directory_items[1].inode; /* second directory_item in folder is the upper folder */
                    }
                }else{
                    i = 0;
                    for(; i < free_index_directory_items; i++){
                        if(strcmp(directory_items[i].item_name, item_target_arr[x]) == 0){ /* inode with desired name has been found */
                            new_inode_number = directory_items[i].inode;
                            break;
                        }
                    }
                }

                if(new_inode_number != -1){
                    if(content_fs -> loaded_pseudo_in_arr[new_inode_number].isDirectory == true){
                        if(x == item_target_arr_length - 2){
                            pseudo_inode_target = &content_fs -> loaded_pseudo_in_arr[new_inode_number];
                        }else{
                            actual_pseudo_inode = &content_fs -> loaded_pseudo_in_arr[new_inode_number];
                        }
                    }else{
                        printf(FILE_NOT_FOUND);
                        error_occured = true;
                        free(directory_items);
                        break;
                    }
                }else{
                    if(in_root_upper == true){
                        printf(ROOT_FS_SEARCH, item_target_arr[x]);
                    }else{
                        printf(FILE_NOT_FOUND);
                    }
                    free(directory_items);
                    break;
                }
                free(directory_items);
            }
        }
    }
    /* find inode for target path - END */

    if(pseudo_inode_target == NULL){
        free(item_target_arr);
        return;
    }

    actual_pseudo_inode = pseudo_inode_target;

    open_rb_fs();
    int numb_it_fol_untraver = (actual_pseudo_inode -> file_size) / sizeof(struct directory_item); /* number of untraversed items in folder */
    struct directory_item *directory_items = malloc(sizeof(struct directory_item) * numb_it_fol_untraver);
    int free_index_directory_items = 0;
    int array_data_blocks[5];

    /* fill array of data block numbers for better traversing */
    array_data_blocks[0] = actual_pseudo_inode -> direct1;
    array_data_blocks[1] = actual_pseudo_inode -> direct2;
    array_data_blocks[2] = actual_pseudo_inode -> direct3;
    array_data_blocks[3] = actual_pseudo_inode -> direct4;
    array_data_blocks[4] = actual_pseudo_inode -> direct5;

    int i = 0;
    for(; i < 5; i++){ /* read direct */
        if(numb_it_fol_untraver != 0){
            int read_count = -1;
            int j = 0;

            if(numb_it_fol_untraver * sizeof(struct directory_item) <= FORMAT_CLUSTER_SIZE_B){ /* read just part of a cluster */
                read_count = numb_it_fol_untraver;
            }else{ /* read whole cluster */
                read_count = FORMAT_CLUSTER_SIZE_B / sizeof(struct directory_item);
            }

            for(; j < read_count; j++){
                int start_address = content_fs -> loaded_sb -> data_start_address + array_data_blocks[i] * FORMAT_CLUSTER_SIZE_B + (j * sizeof(struct directory_item));
                set_pointer_position_fs(start_address, SEEK_SET);
                struct directory_item *dir_item = (struct directory_item *) read_from_file_fs(sizeof(struct directory_item));
                directory_items[free_index_directory_items] = *dir_item;
                free(dir_item);
                free_index_directory_items++;
                numb_it_fol_untraver--;
            }
        }
    }

    /* all directory items loaded, does wanted item exist? */
    i = 0;
    int file_inode_number = -1;
    for(; i < free_index_directory_items; i++){
        if(strcmp(directory_items[i].item_name, item_target_arr[item_target_arr_length - 1]) == 0){ /* inode with desired name has been found */
            file_inode_number = directory_items[i].inode;
            break;
        }
    }

    if(file_inode_number != -1 && content_fs -> loaded_pseudo_in_arr[file_inode_number].isDirectory == false){ /* file exists */
        struct pseudo_inode *pseudo_in;
        pseudo_in = &content_fs -> loaded_pseudo_in_arr[file_inode_number];

        printf(OUTCP_COPY_START);
        printf(OUTCP_INODE_NUMBER, pseudo_in -> nodeid);
        printf(OUTCP_NUM_REF, pseudo_in -> references);
        printf(OUTCP_SIZE, pseudo_in -> file_size);

        int cluster_inode[7];
        cluster_inode[0] = pseudo_in -> direct1;
        cluster_inode[1] = pseudo_in -> direct2;
        cluster_inode[2] = pseudo_in -> direct3;
        cluster_inode[3] = pseudo_in -> direct4;
        cluster_inode[4] = pseudo_in -> direct5;
        cluster_inode[5] = pseudo_in -> indirect1;
        cluster_inode[6] = pseudo_in -> indirect2;

        int file_size = pseudo_in -> file_size;

        int occupied_cluster_data = ceil((float) file_size / FORMAT_CLUSTER_SIZE_B); /* number of clusters reserved for data */
        int occupied_cluster_extra = 0; /* number of extra clusters (for indirect pointer list etc.) */
        /* extra clusters will be needed if indirect is going to be used */
        int indirect1_hold = FORMAT_CLUSTER_SIZE_B / sizeof(int);

        /* count extra clusters - START */
        if(occupied_cluster_data > 5){ /* direct pointers not enough, extra cluster for indirect1 */
            occupied_cluster_extra += 1;

            if(occupied_cluster_data > (5 + indirect1_hold)){ /* indirect2 */
                occupied_cluster_extra += 1; /* for first cluster list */

                int second_list_count = ceil((float) (occupied_cluster_data - (5 + indirect1_hold)) / indirect1_hold); /* needed cluster count / list capacity */
                occupied_cluster_extra += second_list_count;
            }
        }
        /* count extra clusters - END */

        int address_offset = 0;

        /* read direct - START */
        int end;
        if(occupied_cluster_data < 5){
            end = occupied_cluster_data;
        }else{
            end = 5;
        }

        int i = 0;
        for(; i < end; i++){
            int start_address = content_fs -> loaded_sb -> data_start_address + cluster_inode[i] * FORMAT_CLUSTER_SIZE_B;
            set_pointer_position_fs(start_address, SEEK_SET);
            void *data;

            if(end < 5 && (i == end - 1)){ /* reading last cluster */
                int size_to_read = file_size - (i * FORMAT_CLUSTER_SIZE_B);
                data = read_from_file_fs(size_to_read); /* remaining size */
                write_file_extern(data, second_word, address_offset, address_offset + size_to_read);
                address_offset += size_to_read;
            }else{
                data = read_from_file_fs(FORMAT_CLUSTER_SIZE_B);
                write_file_extern(data, second_word, address_offset, address_offset + FORMAT_CLUSTER_SIZE_B);
                address_offset += FORMAT_CLUSTER_SIZE_B;
            }
        }
        /* read direct - END */

        /* read indirect */
        if(occupied_cluster_extra > 0){ /* first extra cluster is a list for indirect1 */
            int loop_end;
            if((occupied_cluster_data - 5) > indirect1_hold){
                loop_end = indirect1_hold;
            }else{
                loop_end = occupied_cluster_data - 5;
            }

            int start_address = content_fs -> loaded_sb -> data_start_address + cluster_inode[5] * FORMAT_CLUSTER_SIZE_B;
            set_pointer_position_fs(start_address, SEEK_SET);
            int *clust_indir1_arr = (int *) read_from_file_fs(loop_end * sizeof(int));

            i = 0;
            for(; i < loop_end; i++){
                int start_address = content_fs -> loaded_sb -> data_start_address + clust_indir1_arr[i] * FORMAT_CLUSTER_SIZE_B;
                set_pointer_position_fs(start_address, SEEK_SET);
                void *data;

                if(loop_end < indirect1_hold && (i == loop_end - 1)){ /* reading last cluster */
                    int size_to_read = file_size - ((i + 5) * FORMAT_CLUSTER_SIZE_B);
                    data = read_from_file_fs(size_to_read); /* remaining size */
                    write_file_extern(data, second_word, address_offset, address_offset + size_to_read);
                    address_offset += size_to_read;
                }else{
                    data = read_from_file_fs(FORMAT_CLUSTER_SIZE_B);
                    write_file_extern(data, second_word, address_offset, address_offset + FORMAT_CLUSTER_SIZE_B);
                    address_offset += FORMAT_CLUSTER_SIZE_B;
                }
             }

            free(clust_indir1_arr);

            if(occupied_cluster_extra > 1){ /* second extra cluster is a first list for indirect2 */
                int second_list_count = ceil((float) (occupied_cluster_data - 5 - indirect1_hold) / indirect1_hold);

                int start_address = content_fs -> loaded_sb -> data_start_address + cluster_inode[6] * FORMAT_CLUSTER_SIZE_B;
                set_pointer_position_fs(start_address, SEEK_SET);
                int *second_list_arr = (int *) read_from_file_fs(sizeof(int) * second_list_count);

                i = 0;
                for(; i < second_list_count; i++){
                    int start_address_sec_list = content_fs -> loaded_sb -> data_start_address + second_list_arr[i] * FORMAT_CLUSTER_SIZE_B;
                    set_pointer_position_fs(start_address_sec_list, SEEK_SET);
                    int *clusters_arr;

                    if((occupied_cluster_data - 5 - indirect1_hold - (indirect1_hold * i)) < indirect1_hold){
                        loop_end = occupied_cluster_data - 5 - indirect1_hold - (indirect1_hold * i);
                    }else{
                        loop_end = indirect1_hold;
                    }

                    clusters_arr = read_from_file_fs(loop_end * sizeof(int));
                    void *data;

                    int j = 0;
                    for(; j < loop_end; j++){
                        set_pointer_position_fs(content_fs -> loaded_sb -> data_start_address + clusters_arr[j] * FORMAT_CLUSTER_SIZE_B, SEEK_SET);
                        if(loop_end < indirect1_hold && (j == loop_end - 1)){ /* reading last cluster */
                            int size_to_read = file_size - ((5 + indirect1_hold + (indirect1_hold * i) + j) * FORMAT_CLUSTER_SIZE_B);
                            data = read_from_file_fs(size_to_read); /* remaining size */
                            write_file_extern(data, second_word, address_offset, address_offset + size_to_read);
                            address_offset += size_to_read;
                        }else{
                            data = read_from_file_fs(FORMAT_CLUSTER_SIZE_B);
                            write_file_extern(data, second_word, address_offset, address_offset + FORMAT_CLUSTER_SIZE_B);
                            address_offset += FORMAT_CLUSTER_SIZE_B;
                        }
                    }

                    free(clusters_arr);
                }

                free(second_list_arr);
            }
        }

        printf(OUTCP_COPY_END);
    }else{
        printf(FILE_NOT_FOUND);
    }
    free(directory_items);
    close_file_fs();

    actual_pseudo_inode = actual_pseudo_inode_backup;
}
/* ____________________________________________________________________________
    void perform_load(char *first_word)

    Performs command load (syntax: load f1), which is used for loading
    file with commands from hard drive. Commands in the file must be
    in corresponding format (1 command = 1 line).
   ____________________________________________________________________________
*/
void perform_load(char *first_word){
    if(check_file_existence(first_word) == false){
        printf(LOAD_FILE_DOESNT_EXIST, first_word);
        return;
    }

    char *file_content = (char *) read_file_extern(first_word, 0, get_file_size(first_word));
    char buffer[100];
    memset(buffer, 0, 100 * sizeof(char));

    int buffer_length = 0;

    int i = 0;

    for(; i < get_file_size(first_word); i++){
        if(file_content[i] != '\n'){
            buffer[buffer_length] = (char) file_content[i];
            buffer_length += 1;
        }else{
            buffer[buffer_length] = '\0';
            buffer_length = 0;

            char *words_entered[3] = {NULL, NULL, NULL}; /* if there are more than 3 spaces in input, then input is wrong */
            char *one_word_entered = NULL;               /* one of the words entered by user - used for traversing */
            int words_entered_counter = 0;               /* number of words stored in "words_entered" array */
            one_word_entered = strtok(buffer, " ");
            while(one_word_entered != NULL){
                /* check whether number of words in command is correct (max 3 words) */
                if(words_entered_counter > 2){
                    printf(WRONG_COMMAND_LENGTH);
                }

                words_entered[words_entered_counter] = one_word_entered;
                one_word_entered = strtok(NULL, " ");
                words_entered_counter = words_entered_counter + 1;
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

    free(file_content);
}
/* ____________________________________________________________________________
    void perform_format(char *first_word)

    Performs command format (syntax: format xMB), which is used for formatting
    file which was entered by user (as a program parameter) to a file system
    of given size (x has to be replaced by size in MB). All data within the
    file will be deleted. If the file does not exists, then it will be created.
   ____________________________________________________________________________
*/
void perform_format(char *first_word){
    actual_pseudo_inode = NULL;

    /* does size of disk contain "MB"? */
    int length_size_disc = strlen(first_word);
    if(strcmp(first_word, "mb") == 0 || strcmp(first_word, "MB") == 0){ /* just "MB" or "mb" entered */
        printf(FORMAT_SIZE_NOT_ENTERED);
        return;
    }else if(length_size_disc <= 2){
        printf(FORMAT_WRONG_USAGE);
        return;
    }
    /* remove last 2 characters (MB/mb) from user entered text */
    first_word[strlen(first_word) - 1] = 0;
    first_word[strlen(first_word) - 1] = 0;
    int number_entered; /* will contain the numeric part of string which user entered */
    char* not_number_part; /* will contain the non-numeric part of string that user entered */
    number_entered = (int)strtol(first_word, &not_number_part, FORMAT_NUM_BASE);
    if(*not_number_part) {
        printf(FORMAT_WRONG_SIZE);
        return;
    }else{
        printf(FORMAT_REQ_SIZE, number_entered);
    }
    /* does file already exist? */
    int format = -1; //1 or 2 when valid option is selected ("Y" = 1 or "N" = 2)
    char *name_file_fs = get_filename_fs(); /* get name of file which will be used as core of our FS */
    if(check_file_existence(name_file_fs) == true){
        printf(FORMAT_FILE_EXISTS, name_file_fs);
        while(format == -1){
            printf(FORMAT_FILE_EXISTS_CHOICE);
            char user_choice[3]; /* answer entered by user */
            fgets(user_choice, 3, stdin);
            /* if last character was not linebreak, then empty the buffer of stdin because of remaining characters */
            if(user_choice[strlen(user_choice) - 1] != '\n'){
                int c;
                while(((c = getchar()) != '\n') && (c != EOF));
            }else{
                user_choice[strlen(user_choice) - 1] = 0; /* delete linebreak on the end */
            }
            if(strcmp(user_choice, "Y") == 0 || strcmp(user_choice, "y") == 0){
                format = 1;
            }else if(strcmp(user_choice, "N") == 0 || strcmp(user_choice, "n") == 0){
                format = 2;
            }else{
                printf(FORMAT_FILE_EXISTS_INVALID);
            }
        }
    }else{
        format = 1;
    }
    if(format == 1){ /* yes to format - get file ready for binary write */
        printf(FORMAT_FORMAT_START);
        if(open_wb_fs() != 0){ //error occured while creating file
            printf(FORMAT_CANNOT_CREATE_FILE);
        }
        /* creating file with specific size - START */
        int total_file_size = (number_entered * FORMAT_MB_TO_B) - 1;
        set_pointer_position_fs(total_file_size, SEEK_SET);
        char end_of_file = '\0';
        write_to_file_fs((void *) &end_of_file, sizeof(char));
        /* creating file with specific size - END */
        set_pointer_position_fs(0, SEEK_SET); /* go back to beginning of the FS file */
        /* prepare structure which defines superblock of FS - START */
        struct superblock sb;
        memset(&sb, 0, sizeof(struct superblock));
        /* superblock signature - nickname of creator */
        strcpy(sb.signature, FORMAT_SB_SIGNATURE);
        /* get info about current date & time - used as volume descriptor */
        time_t actual_time = time(NULL);
        struct tm *time = localtime(&actual_time);
        /* superblock volume descriptor - actual time & date */
        strcpy(sb.volume_descriptor, asctime(time));
        sb.volume_descriptor[strlen(sb.volume_descriptor) - 1] = '\0'; /* delete linebreak at the end */
        sb.disk_size = number_entered * FORMAT_MB_TO_B; /* convert MB to B */
        sb.cluster_size = FORMAT_CLUSTER_SIZE_B; /* cluster size is 1000B = 1kB */
        sb.cluster_count = (sb.disk_size * FORMAT_CLUSTER_DISC_PERC) / sb.cluster_size; /* cluster count - 95% of total disc size */
        int inode_count = (sb.disk_size * FORMAT_CLUSTER_INODE_PERC) / sizeof(struct pseudo_inode); /* 4% for inodes */
        /* prepare structure which defines superblock of our FS - END */
        /* prepare structure which defines bitmap of inodes - START */
        struct bitmap_inodes bit_in;
        bit_in.inodes_free_total = inode_count;
        bit_in.inodes_occupied_total = 0;
        bit_in.inodes_bitmap = (int*) malloc(bit_in.inodes_free_total * sizeof(int));
        /* init array of inodes - at this moment all inodes are free */
        int i = 0;
        for(; i < bit_in.inodes_free_total; i++){
            bit_in.inodes_bitmap[i] = 0;
        }
        /* prepare structure which defines bitmap of inodes - END */
        /* prepare structure which defines bitmap of clusters - START */
        struct bitmap_clusters bit_clus;
        bit_clus.cluster_free_total = sb.cluster_count;
        bit_clus.cluster_occupied_total = 0;
        bit_clus.cluster_bitmap = (int*) malloc(bit_clus.cluster_free_total * sizeof(int));
        /* init array of clusters - at this moment all clusters are free */
        i = 0;
        for(; i < bit_clus.cluster_free_total; i++){
            bit_clus.cluster_bitmap[i] = 0;
        }
        /* prepare structure which defines bitmap of clusters - END */
        /* prepare structure array which defines inodes - START */
        struct pseudo_inode *pseudo_in_arr = malloc(sizeof(struct pseudo_inode) * bit_in.inodes_free_total);
        memset(pseudo_in_arr, 0, sizeof(struct pseudo_inode) * bit_in.inodes_free_total);
        i = 0;
        for(; i < bit_in.inodes_free_total; i++){
            pseudo_in_arr[i].file_size = -1;
            pseudo_in_arr[i].references = 1;
            pseudo_in_arr[i].isDirectory = false;
            pseudo_in_arr[i].direct1 = -1;
            pseudo_in_arr[i].direct2 = -1;
            pseudo_in_arr[i].direct3 = -1;
            pseudo_in_arr[i].direct4 = -1;
            pseudo_in_arr[i].direct5 = -1;
            pseudo_in_arr[i].indirect1 = -1;
            pseudo_in_arr[i].indirect2 = -1;
            pseudo_in_arr[i].nodeid = i;
        }

        /* prepare structure array which defines inodes - END */
        /* all needed info prepared, determine start addresses - fill rest of superblock info - START */
        sb.bitmap_clus_start_address = sizeof(struct superblock); /* before bitmap with clusters will be: superblock */
        sb.bitmap_in_start_address = sb.bitmap_clus_start_address + sizeof(struct bitmap_clusters) + (bit_clus.cluster_free_total * sizeof(int)); /* before bitmap with inodes will be: superblock + bitmap with clusters */
        sb.inode_start_address = sb.bitmap_in_start_address + sizeof(struct bitmap_inodes) + (bit_in.inodes_free_total * sizeof(int)); /* before inodes will be: superblock + bitmap with clusters + bitmap with inodes */
        sb.data_start_address = sb.inode_start_address + (sizeof(struct pseudo_inode) * inode_count); /* before inodes will be: superblock + bitmap with clusters + bitmap with inodes + inodes */
        /* all needed info prepared, determine start addresses - fill rest of superblock info - END */

        save_superblock(&sb); /* write superblock */
        save_bitmap_clusters(&bit_clus); /* write cluster bitmap */
        save_bitmap_inodes(&bit_in, bit_clus.cluster_free_total); /* write inode bitmap */
        save_pseudo_inodes(pseudo_in_arr, bit_in.inodes_free_total, bit_clus.cluster_free_total, bit_in.inodes_free_total); /* write inodes */
        format_data_space(sb.data_start_address, sb.cluster_count, sb.cluster_size);

        int rem_space_data = (number_entered * FORMAT_MB_TO_B) - get_pointer_position_fs();
        printf(FORMAT_REM_SPACE, rem_space_data);
        /* free structures after write */
        free(bit_clus.cluster_bitmap);
        free(bit_in.inodes_bitmap);
        free(pseudo_in_arr);
        close_file_fs();
        /* reload data from file into memory */
        if(content_fs != NULL){ /* if file is  */
            free_fs_file_content(content_fs);
        }
        content_fs_check();
        perform_mkdir("/"); /* create main folder */
        perform_cd(".");
    }else if(format == 2){ /* no to format */
        printf(FORMAT_FORMAT_CANCEL);
    }
    return; /* virtual disc created OK */
}

/* ____________________________________________________________________________
    int * get_clust_inode_arr_defrag()
    Purpose of this function is to prepare cluster -> inode array which
    is used mainly by function perform_defrag().
    Index is the number of cluster and value is respective inode id which points
    to the cluster.
   ____________________________________________________________________________
*/
int * get_clust_inode_arr_defrag(){
    open_rb_fs();
    int *cluster_inode_arr = malloc(content_fs -> loaded_sb -> cluster_count * sizeof(int));

    /* init inode array - START */
    int i = 0;
    for(; i < content_fs -> loaded_sb -> cluster_count; i++){
        cluster_inode_arr[i] = -1; /* -1 => cluster not assigned */
    }
    /* init inode array - END */

    /* go through occupied inodes */
    struct pseudo_inode *occupied_pseudo_inode = NULL;
    int occupied_pseudo_inode_number = -1;

    int x = 0;
    int total_inode_count = content_fs -> loaded_bit_in -> inodes_occupied_total + content_fs -> loaded_bit_in -> inodes_free_total;
    for(; x < total_inode_count; x++){
        if(content_fs -> loaded_bit_in -> inodes_bitmap[x] == 1){ /* inode is occupied */
            occupied_pseudo_inode = &content_fs -> loaded_pseudo_in_arr[x]; /* (index is the same as with bitmap) */
            occupied_pseudo_inode_number = x;

            /* got occupied inode, assign inode to clusters - START */
            int occupied_cluster_data = ceil((float) occupied_pseudo_inode -> file_size / FORMAT_CLUSTER_SIZE_B); /* number of clusters reserved for data */
            int occupied_cluster_extra = 0; /* number of extra clusters (for indirect pointer list etc.) */
            /* extra clusters will be needed if indirect is going to be used */
            int indirect1_hold = FORMAT_CLUSTER_SIZE_B / sizeof(int);

            /* count extra clusters - START */
            if(occupied_cluster_data > 5){ /* direct pointers not enough, extra cluster for indirect1 */
                occupied_cluster_extra += 1;

                if(occupied_cluster_data > (5 + indirect1_hold)){ /* indirect2 */
                    occupied_cluster_extra += 1; /* for first cluster list */

                    int second_list_count = ceil((float) (occupied_cluster_data - (5 + indirect1_hold)) / indirect1_hold); /* needed cluster count / list capacity */
                    occupied_cluster_extra += second_list_count;
                }
            }
            /* count extra clusters - END */

            int cluster_direct[5];
            /* direct cluster - START */
            cluster_direct[0] = occupied_pseudo_inode -> direct1;
            cluster_direct[1] = occupied_pseudo_inode -> direct2;
            cluster_direct[2] = occupied_pseudo_inode -> direct3;
            cluster_direct[3] = occupied_pseudo_inode -> direct4;
            cluster_direct[4] = occupied_pseudo_inode -> direct5;

            i = 0;
            for(; i < 5; i++){
                if(cluster_direct[i] != -1){
                    cluster_inode_arr[cluster_direct[i]] = occupied_pseudo_inode_number;
                }
            }
            /* direct cluster - END */

            /* indirect cluster - START */
            if(occupied_cluster_extra > 0){ /* first extra cluster is a list for indirect1 */
                cluster_inode_arr[occupied_pseudo_inode -> indirect1] = occupied_pseudo_inode_number;

                int loop_end;
                if((occupied_cluster_data - 5) > indirect1_hold){
                    loop_end = indirect1_hold;
                }else{
                    loop_end = occupied_cluster_data - 5;
                }
                int start_address = content_fs -> loaded_sb -> data_start_address + occupied_pseudo_inode -> indirect1 * FORMAT_CLUSTER_SIZE_B;
                set_pointer_position_fs(start_address, SEEK_SET);
                int *clust_indir1_arr = (int *) read_from_file_fs(loop_end * sizeof(int));

                i = 0;
                for (; i < loop_end; i++) {
                    cluster_inode_arr[clust_indir1_arr[i]] = occupied_pseudo_inode_number;
                }
                free(clust_indir1_arr);

                if(occupied_cluster_extra > 1){ /* second extra cluster is a first list for indirect2 */
                    cluster_inode_arr[occupied_pseudo_inode -> indirect2] = occupied_pseudo_inode_number; /* indirect2 first list */
                    int second_list_count = ceil((float) (occupied_cluster_data - 5 - indirect1_hold) / indirect1_hold);
                    int start_address = content_fs -> loaded_sb -> data_start_address + occupied_pseudo_inode -> indirect2 * FORMAT_CLUSTER_SIZE_B;
                    set_pointer_position_fs(start_address, SEEK_SET);
                    int *second_list_arr = (int *) read_from_file_fs(sizeof(int) * second_list_count);

                    i = 0;
                    for(; i < second_list_count; i++){
                        cluster_inode_arr[second_list_arr[i]] = occupied_pseudo_inode_number; /* indirect2 second list */
                        int start_address_sec_list = content_fs -> loaded_sb -> data_start_address + second_list_arr[i] * FORMAT_CLUSTER_SIZE_B;
                        set_pointer_position_fs(start_address_sec_list, SEEK_SET);
                        int *clusters_arr;
                        if((occupied_cluster_data - 5 - indirect1_hold - (indirect1_hold * i)) < indirect1_hold){
                            loop_end = occupied_cluster_data - 5 - indirect1_hold - (indirect1_hold * i);
                        } else {
                            loop_end = indirect1_hold;
                        }
                        clusters_arr = read_from_file_fs(loop_end * sizeof(int));

                        int j = 0;
                        for(; j < loop_end; j++){
                            cluster_inode_arr[clusters_arr[j]] = occupied_pseudo_inode_number;
                        }
                        free(clusters_arr);
                    }
                    free(second_list_arr);
                }
            }
        }
    }
    /* indirect cluster - END */
    /* got occupied inode, assign inode to clusters - END */
    close_file_fs();

    return cluster_inode_arr;
}

/* _____________________________________________________________________________________________________________
    void move_inode_clust_defrag(int *cluster_inode_arr_defrag, int *cluster_wanted, int cluster_wanted_length)

    Function is called by int perform_defrag() and is used to move content of clusters included
    in cluster_wanted to other free clusters.
   _____________________________________________________________________________________________________________
*/
void move_inode_clust_defrag(int *cluster_inode_arr_defrag, int *cluster_wanted, int cluster_wanted_length){
    int *inode_list = NULL; /* array of indexes of unmoved inodes */
    int inode_list_length = 0;

    /* make a list of inodes, which clusters need to be moved in order to free all clusters contained in int *cluster_wanted */
    int i = 0;
    for(; i < cluster_wanted_length; i++){ /* go through clusters */
        int inode_to_add = cluster_inode_arr_defrag[cluster_wanted[i]];
        if(inode_list_length == 0){ /* first inode to add */
            /* add new inode to the list */
            inode_list_length += 1;
            inode_list = realloc(inode_list, inode_list_length * sizeof(int));
            inode_list[inode_list_length - 1] = inode_to_add;
        }else{
            bool found = false;

            int j = 0;
            for(; j < inode_list_length; j++){ /* check if inode is not already in the list */
                if(inode_list[j] == inode_to_add){ /* inode already in list */
                    found = true;
                    break;
                }
            }

            if(found == false){ /* add new inode to the list */
                inode_list_length += 1;
                inode_list = realloc(inode_list, inode_list_length * sizeof(int));
                inode_list[inode_list_length - 1] = inode_to_add;
            }
        }
    }

    /* move inodes with wanted clusters - START */
    i = 0;
    for(; i < inode_list_length; i++){
        open_wb_add_fs();
        struct pseudo_inode *pseudo_in_to_move = &content_fs -> loaded_pseudo_in_arr[inode_list[i]];
        /* copy content of file to move to another clusters - START */
        int cluster_inode[7];
        cluster_inode[0] = pseudo_in_to_move -> direct1;
        cluster_inode[1] = pseudo_in_to_move -> direct2;
        cluster_inode[2] = pseudo_in_to_move -> direct3;
        cluster_inode[3] = pseudo_in_to_move -> direct4;
        cluster_inode[4] = pseudo_in_to_move -> direct5;
        cluster_inode[5] = pseudo_in_to_move -> indirect1;
        cluster_inode[6] = pseudo_in_to_move -> indirect2;
        int file_size = pseudo_in_to_move -> file_size;

        int occupied_cluster_data = ceil((float) file_size / FORMAT_CLUSTER_SIZE_B); /* number of clusters reserved for data */
        int occupied_cluster_extra = 0; /* number of extra clusters (for indirect pointer list etc.) */
        /* extra clusters will be needed if indirect is going to be used */
        int indirect1_hold = FORMAT_CLUSTER_SIZE_B / sizeof(int);
        /* count extra clusters - START */
        if(occupied_cluster_data > 5){ /* direct pointers not enough, extra cluster for indirect1 */
            occupied_cluster_extra += 1;
            if (occupied_cluster_data > (5 + indirect1_hold)) { /* indirect2 */
                occupied_cluster_extra += 1; /* for first cluster list */
                int second_list_count = ceil((float) (occupied_cluster_data - (5 + indirect1_hold)) /
                                             indirect1_hold); /* needed cluster count / list capacity */
                occupied_cluster_extra += second_list_count;
            }
        }
        /* count extra clusters - END */
        struct free_clusters_struct *free_clusters = get_free_cluster_arr(occupied_cluster_data + occupied_cluster_extra);

        /* not enough clusters */
        if(free_clusters == NULL){
            printf(DEFRAG_NO_CLUSTER);
        }

        /* read cluster & write - START */
        int address_offset = 0;
        /* direct - START */
        int end;
        if(occupied_cluster_data < 5){
            end = occupied_cluster_data;
        }else{
            end = 5;
        }

        int cluster_data_counter_cp = 0; /* counter for data clusters assigned to new file */
        int j = 0;
        for(; j < end; j++){
            int start_address = content_fs -> loaded_sb -> data_start_address + cluster_inode[j] * FORMAT_CLUSTER_SIZE_B;
            set_pointer_position_fs(start_address, SEEK_SET);
            int write_start_addr = content_fs -> loaded_sb -> data_start_address + free_clusters -> free_clusters_arr[cluster_data_counter_cp] * FORMAT_CLUSTER_SIZE_B;
            void *data;
            if(end < 5 && (j == end - 1)){ /* reading last cluster */
                int size_to_read = pseudo_in_to_move -> file_size - (j * FORMAT_CLUSTER_SIZE_B);
                data = read_from_file_fs(size_to_read); /* remaining size */
                set_pointer_position_fs(write_start_addr, SEEK_SET);
                write_to_file_fs(data, size_to_read);
                address_offset += size_to_read;
            }else{
                data = read_from_file_fs(FORMAT_CLUSTER_SIZE_B);
                set_pointer_position_fs(write_start_addr, SEEK_SET);
                write_to_file_fs(data, FORMAT_CLUSTER_SIZE_B);
                address_offset += FORMAT_CLUSTER_SIZE_B;
            }
            cluster_data_counter_cp += 1;
            free(data);
        }
        /* direct - END */
        /* read indirect */
        if(occupied_cluster_extra > 0){ /* first extra cluster is a list for indirect1 */
            int loop_end;
            if((occupied_cluster_data - 5) > indirect1_hold){
                loop_end = indirect1_hold;
            }else{
                loop_end = occupied_cluster_data - 5;
            }
            int start_address = content_fs -> loaded_sb -> data_start_address + cluster_inode[5] * FORMAT_CLUSTER_SIZE_B;
            set_pointer_position_fs(start_address, SEEK_SET);
            int *clust_indir1_arr = (int *) read_from_file_fs(loop_end * sizeof(int));
            int j = 0;
            for(; j < loop_end; j++){
                long int start_address = content_fs -> loaded_sb -> data_start_address + clust_indir1_arr[j] * FORMAT_CLUSTER_SIZE_B;
                set_pointer_position_fs(start_address, SEEK_SET);
                long int write_start_addr = content_fs -> loaded_sb -> data_start_address + free_clusters -> free_clusters_arr[cluster_data_counter_cp] * FORMAT_CLUSTER_SIZE_B;
                void *data;
                if(loop_end < indirect1_hold && (j == loop_end - 1)){ /* reading last cluster */
                    int size_to_read = pseudo_in_to_move -> file_size - ((j + 5) * FORMAT_CLUSTER_SIZE_B);
                    data = read_from_file_fs(size_to_read); /* remaining size */
                    set_pointer_position_fs(write_start_addr, SEEK_SET);
                    write_to_file_fs(data, size_to_read);
                    address_offset += size_to_read;
                }else{
                    data = read_from_file_fs(FORMAT_CLUSTER_SIZE_B);
                    set_pointer_position_fs(write_start_addr, SEEK_SET);
                    write_to_file_fs(data, FORMAT_CLUSTER_SIZE_B);
                    address_offset += FORMAT_CLUSTER_SIZE_B;
                }
                cluster_data_counter_cp += 1;
                free(data);
            }
            free(clust_indir1_arr);
            if(occupied_cluster_extra > 1){ /* second extra cluster is a first list for indirect2 */
                int second_list_count = ceil((float) (occupied_cluster_data - 5 - indirect1_hold) / indirect1_hold);
                int start_address = content_fs -> loaded_sb -> data_start_address + cluster_inode[6] * FORMAT_CLUSTER_SIZE_B;
                set_pointer_position_fs(start_address, SEEK_SET);
                int *second_list_arr = (int *) read_from_file_fs(sizeof(int) * second_list_count);
                int j = 0;
                for(; j < second_list_count; j++){
                    int start_address_sec_list = content_fs -> loaded_sb -> data_start_address + second_list_arr[j] * FORMAT_CLUSTER_SIZE_B;
                    set_pointer_position_fs(start_address_sec_list, SEEK_SET);
                    int *clusters_arr;
                    if((occupied_cluster_data - 5 - indirect1_hold - (indirect1_hold * j)) < indirect1_hold){
                        loop_end = occupied_cluster_data - 5 - indirect1_hold - (indirect1_hold * j);
                    }else{
                        loop_end = indirect1_hold;
                    }
                    clusters_arr = read_from_file_fs(loop_end * sizeof(int));
                    void *data;
                    int k = 0;
                    for(; k < loop_end; k++){
                        set_pointer_position_fs(content_fs -> loaded_sb -> data_start_address + clusters_arr[k] * FORMAT_CLUSTER_SIZE_B, SEEK_SET);
                        int write_start_addr = content_fs -> loaded_sb -> data_start_address + free_clusters -> free_clusters_arr[cluster_data_counter_cp] * FORMAT_CLUSTER_SIZE_B;
                        if(loop_end < indirect1_hold && (k == loop_end - 1)){ /* reading last cluster */
                            int size_to_read = pseudo_in_to_move -> file_size - ((5 + indirect1_hold + (indirect1_hold * j) + k) * FORMAT_CLUSTER_SIZE_B);
                            data = read_from_file_fs(size_to_read); /* remaining size */
                            set_pointer_position_fs(write_start_addr, SEEK_SET);
                            write_to_file_fs(data, size_to_read);
                            address_offset += size_to_read;
                        }else{
                            data = read_from_file_fs(FORMAT_CLUSTER_SIZE_B);
                            set_pointer_position_fs(write_start_addr, SEEK_SET);
                            write_to_file_fs(data, FORMAT_CLUSTER_SIZE_B);
                            address_offset += FORMAT_CLUSTER_SIZE_B;
                        }
                        cluster_data_counter_cp += 1;
                        free(data);
                    }
                    free(clusters_arr);
                }
                free(second_list_arr);
            }
        }
        /* read cluster & write - END */

        /* mark originally used clusters as free */
        close_file_fs();
        mark_cluster_unused_defrag(pseudo_in_to_move -> nodeid);
        open_wb_add_fs();

        /* update cluster_inode_arr_defrag */
        j = 0;
        for(; j < free_clusters -> free_clusters_arr_length; j++){
            cluster_inode_arr_defrag[free_clusters -> free_clusters_arr[j]] = pseudo_in_to_move -> nodeid;
        }

        /* marked wanted clusters as used again, some were freed in previous step */
        j = 0;
        for(; j < cluster_wanted_length; j++){
            if(content_fs -> loaded_bit_clus -> cluster_bitmap[cluster_wanted[j]] == 0){
                content_fs -> loaded_bit_clus -> cluster_occupied_total++;
                content_fs -> loaded_bit_clus -> cluster_free_total--;
                content_fs -> loaded_bit_clus -> cluster_bitmap[cluster_wanted[j]] = 1;
            }
        }
        save_bitmap_clusters(content_fs -> loaded_bit_clus);

        /* assign clusters with data to inode - START */
        /* assign direct */
        int *cluster_direct_mv[5];
        cluster_direct_mv[0] = &pseudo_in_to_move -> direct1;
        cluster_direct_mv[1] = &pseudo_in_to_move -> direct2;
        cluster_direct_mv[2] = &pseudo_in_to_move -> direct3;
        cluster_direct_mv[3] = &pseudo_in_to_move -> direct4;
        cluster_direct_mv[4] = &pseudo_in_to_move -> direct5;
        if(pseudo_in_to_move -> file_size <= (FORMAT_CLUSTER_SIZE_B * 5)){
            end = ceil((float) pseudo_in_to_move -> file_size / FORMAT_CLUSTER_SIZE_B);
        }else{
            end = 5;
        }

        j = 0;
        for(; j < end; j++){
            *cluster_direct_mv[j] = free_clusters -> free_clusters_arr[j];
        }
        /* assign indirect */
        if(occupied_cluster_extra > 0){ /* first extra cluster is a list for indirect1 */
            int *indirect1_arr = NULL; /* array of clusters which will be saved on indirect1 */
            int rem_data_clus = occupied_cluster_data - 5; /* clusters with data count, not assigned */
            int num_items;
            if(rem_data_clus < indirect1_hold){ /* less unassigned than cluster is able to hold => indirect2 will not be used */
                num_items = rem_data_clus;
            }else{
                num_items = indirect1_hold;
            }
            indirect1_arr = malloc(sizeof(int) * num_items);
            j = 0;
            for(; j < num_items; j++){ /* fill indirect1 array */
                indirect1_arr[j] = free_clusters -> free_clusters_arr[5 + j]; /* 5*direct */
            }
            /* array filled, now write data to first extra cluster */
            int indirect1_list_clust = free_clusters -> free_clusters_arr[free_clusters -> free_clusters_arr_length - 1 - occupied_cluster_extra + 1]; /* find first extra cluster */
            pseudo_in_to_move -> indirect1 = indirect1_list_clust;
            int indirect1_list_start_addr = (content_fs -> loaded_sb -> data_start_address) + (indirect1_list_clust * FORMAT_CLUSTER_SIZE_B);
            set_pointer_position_fs(indirect1_list_start_addr, SEEK_SET);
            write_to_file_fs((void *) indirect1_arr, sizeof(int) * num_items);
            rem_data_clus -= num_items; /* update unassigned cluster count */
            free(indirect1_arr);

            if(occupied_cluster_extra > 1){ /* second extra cluster is a first list for indirect2 */
                /* fill second list for indirect2 - START */
                int second_list_count = ceil((float) (rem_data_clus) / indirect1_hold); /* needed cluster count / list capacity */
                j = 0;
                for(; j < second_list_count; j++){
                    int indirect2_sec_list_clust = free_clusters -> free_clusters_arr[free_clusters -> free_clusters_arr_length - 1 - occupied_cluster_extra + 3 + j]; /* find address for extra cluster */
                    int indirect2_sec_list_start_addr = (content_fs -> loaded_sb -> data_start_address) + (indirect2_sec_list_clust * FORMAT_CLUSTER_SIZE_B);
                    set_pointer_position_fs(indirect2_sec_list_start_addr, SEEK_SET);
                    if(j == (second_list_count - 1)){ /* in case of last cluster write */
                        write_to_file_fs((void *) free_clusters -> free_clusters_arr + (5 + indirect1_hold + (j * indirect1_hold)) *
                                                                                       sizeof(int), rem_data_clus * sizeof(int));
                        rem_data_clus = 0;
                    }else{
                        write_to_file_fs((void *) free_clusters -> free_clusters_arr + (5 + indirect1_hold + (j * indirect1_hold)) *
                                                                                       sizeof(int), indirect1_hold * sizeof(int));
                        rem_data_clus -= indirect1_hold;
                    }
                }
                /* fill second list for indirect2 - END */
                /* fill first list for indirect2 - START */
                int indirect2_first_list_clust = free_clusters -> free_clusters_arr[free_clusters -> free_clusters_arr_length - 1 - occupied_cluster_extra + 2]; /* find address for extra cluster */
                pseudo_in_to_move -> indirect2 = indirect2_first_list_clust;
                int indirect2_first_list_start_addr = (content_fs -> loaded_sb -> data_start_address) + (indirect2_first_list_clust * FORMAT_CLUSTER_SIZE_B);
                set_pointer_position_fs(indirect2_first_list_start_addr, SEEK_SET);
                write_to_file_fs((void *) free_clusters -> free_clusters_arr + (free_clusters -> free_clusters_arr_length - second_list_count) * sizeof(int),
                                 sizeof(int) * second_list_count);
                /* fill first list for indirect1 - END */
            }
        }
        /* assign clusters with data to inode - END */
        /* copy content of file to move to another clusters - END */

        save_pseudo_inodes(content_fs -> loaded_pseudo_in_arr, (content_fs -> loaded_bit_in -> inodes_free_total + content_fs -> loaded_bit_in -> inodes_occupied_total),
                           (content_fs -> loaded_bit_clus -> cluster_free_total + content_fs -> loaded_bit_clus -> cluster_occupied_total),
                           (content_fs -> loaded_bit_in -> inodes_free_total + content_fs -> loaded_bit_in -> inodes_occupied_total));

        /* update bitmap with cluster state after moving - START */
        j = 0;
        for(; j < free_clusters -> free_clusters_arr_length; j++){
            content_fs -> loaded_bit_clus -> cluster_occupied_total++;
            content_fs -> loaded_bit_clus -> cluster_free_total--;
            content_fs -> loaded_bit_clus -> cluster_bitmap[free_clusters -> free_clusters_arr[j]] = 1;
        }
        save_bitmap_clusters(content_fs -> loaded_bit_clus);
        /* update bitmap with cluster state after moving - END */

        free(free_clusters -> free_clusters_arr);
        free(free_clusters);
        close_file_fs();
    }
    /* move inodes with wanted clusters - END */

    if(inode_list_length != 0){ /* no inodes to move */
        free(inode_list);
    }
}

/* ____________________________________________________________________________
    void mark_cluster_unused_defrag(int inode_index)

    Function is called by int perform_defrag() and is used to mark clusters
    pointed to by inode on inode_index as unused.
   ____________________________________________________________________________
*/
void mark_cluster_unused_defrag(int inode_index){
    open_wb_add_fs();

    struct pseudo_inode *pseudo_in_to_move = &content_fs -> loaded_pseudo_in_arr[inode_index];

    int cluster_direct[5];
    cluster_direct[0] = pseudo_in_to_move -> direct1;
    cluster_direct[1] = pseudo_in_to_move -> direct2;
    cluster_direct[2] = pseudo_in_to_move -> direct3;
    cluster_direct[3] = pseudo_in_to_move -> direct4;
    cluster_direct[4] = pseudo_in_to_move -> direct5;

    int i = 0;
    for(; i < 5; i++){
        if(cluster_direct[i] != -1){
            content_fs -> loaded_bit_clus -> cluster_bitmap[cluster_direct[i]] = 0;
            content_fs -> loaded_bit_clus -> cluster_occupied_total--;
            content_fs -> loaded_bit_clus -> cluster_free_total++;
        }
    }

    int occupied_cluster_data = ceil((float) pseudo_in_to_move -> file_size / FORMAT_CLUSTER_SIZE_B); /* number of clusters reserved for data */
    int occupied_cluster_extra = 0; /* number of extra clusters (for indirect pointer list etc.) */
    int indirect1_hold = FORMAT_CLUSTER_SIZE_B / sizeof(int);

    /* count extra clusters - START */
    if(occupied_cluster_data > 5){ /* direct pointers not enough, extra cluster for indirect1 */
        occupied_cluster_extra += 1;

        if(occupied_cluster_data > (5 + indirect1_hold)){ /* indirect2 */
            occupied_cluster_extra += 1; /* for first cluster list */

            int second_list_count = ceil((float) (occupied_cluster_data - (5 + indirect1_hold)) /
                                         indirect1_hold); /* needed cluster count / list capacity */
            occupied_cluster_extra += second_list_count;
        }
    }
    /* count extra clusters - END */

    if(occupied_cluster_extra > 0){ /* first extra cluster is a list for indirect1 */
        content_fs -> loaded_bit_clus -> cluster_bitmap[pseudo_in_to_move -> indirect1] = 0;
        content_fs -> loaded_bit_clus -> cluster_occupied_total--;
        content_fs -> loaded_bit_clus -> cluster_free_total++;

        int loop_end;
        if((occupied_cluster_data - 5) > indirect1_hold){
            loop_end = indirect1_hold;
        }else{
            loop_end = occupied_cluster_data - 5;
        }
        int start_address = content_fs -> loaded_sb -> data_start_address + pseudo_in_to_move -> indirect1 * FORMAT_CLUSTER_SIZE_B;
        set_pointer_position_fs(start_address, SEEK_SET);
        int *clust_indir1_arr = (int *) read_from_file_fs(loop_end * sizeof(int));

        int i = 0;
        for(; i < loop_end; i++){
            content_fs -> loaded_bit_clus -> cluster_bitmap[clust_indir1_arr[i]] = 0;
            content_fs -> loaded_bit_clus -> cluster_occupied_total--;
            content_fs -> loaded_bit_clus -> cluster_free_total++;
        }

        free(clust_indir1_arr);

        if(occupied_cluster_extra > 1){ /* second extra cluster is a first list for indirect2 */
            content_fs -> loaded_bit_clus -> cluster_bitmap[pseudo_in_to_move -> indirect2] = 0;
            content_fs -> loaded_bit_clus -> cluster_occupied_total--;
            content_fs -> loaded_bit_clus -> cluster_free_total++;

            int second_list_count = ceil((float) (occupied_cluster_data - 5 - indirect1_hold) / indirect1_hold);
            int start_address = content_fs -> loaded_sb -> data_start_address + pseudo_in_to_move -> indirect2 * FORMAT_CLUSTER_SIZE_B;
            set_pointer_position_fs(start_address, SEEK_SET);
            int *second_list_arr = (int *) read_from_file_fs(sizeof(int) * second_list_count);

            int i = 0;
            for(; i < second_list_count; i++){
                content_fs -> loaded_bit_clus -> cluster_bitmap[second_list_arr[i]] = 0;
                content_fs -> loaded_bit_clus -> cluster_occupied_total--;
                content_fs -> loaded_bit_clus -> cluster_free_total++;

                int start_address_sec_list = content_fs -> loaded_sb -> data_start_address + second_list_arr[i] * FORMAT_CLUSTER_SIZE_B;
                set_pointer_position_fs(start_address_sec_list, SEEK_SET);
                int *clusters_arr;
                if((occupied_cluster_data - 5 - indirect1_hold - (indirect1_hold * i)) < indirect1_hold){
                    loop_end = occupied_cluster_data - 5 - indirect1_hold - (indirect1_hold * i);
                }else{
                    loop_end = indirect1_hold;
                }
                clusters_arr = read_from_file_fs(loop_end * sizeof(int));

                int j = 0;
                for(; j < loop_end; j++){
                    content_fs -> loaded_bit_clus -> cluster_bitmap[clusters_arr[j]] = 0;
                    content_fs -> loaded_bit_clus -> cluster_occupied_total--;
                    content_fs -> loaded_bit_clus -> cluster_free_total++;
                }
                free(clusters_arr);
            }
            free(second_list_arr);
        }
    }
    save_bitmap_clusters(content_fs -> loaded_bit_clus);

    close_file_fs();
}

/* ________________________________________________________________________________________
    void move_inode_spec_clust_defrag(int inode_index, int *clusters, int clusters_length)

    Moves content from clusters of inode specified by int inode_index to clusters
    specified by int *clusters.
   ________________________________________________________________________________________
*/
void move_inode_spec_clust_defrag(int inode_index, int *clusters, int clusters_length){
    open_wb_add_fs();

    struct pseudo_inode *pseudo_in_to_move = &content_fs -> loaded_pseudo_in_arr[inode_index];
    int address_offset = 0;

    int cluster_inode[7];
    cluster_inode[0] = pseudo_in_to_move -> direct1;
    cluster_inode[1] = pseudo_in_to_move -> direct2;
    cluster_inode[2] = pseudo_in_to_move -> direct3;
    cluster_inode[3] = pseudo_in_to_move -> direct4;
    cluster_inode[4] = pseudo_in_to_move -> direct5;
    cluster_inode[5] = pseudo_in_to_move -> indirect1;
    cluster_inode[6] = pseudo_in_to_move -> indirect2;

    int file_size = pseudo_in_to_move -> file_size;

    int occupied_cluster_data = ceil((float) file_size / FORMAT_CLUSTER_SIZE_B); /* number of clusters reserved for data */
    int occupied_cluster_extra = 0; /* number of extra clusters (for indirect pointer list etc.) */
    /* extra clusters will be needed if indirect is going to be used */
    int indirect1_hold = FORMAT_CLUSTER_SIZE_B / sizeof(int);

    /* count extra clusters - START */
    if(occupied_cluster_data > 5){ /* direct pointers not enough, extra cluster for indirect1 */
        occupied_cluster_extra += 1;
        if(occupied_cluster_data > (5 + indirect1_hold)){ /* indirect2 */
            occupied_cluster_extra += 1; /* for first cluster list */

            int second_list_count = ceil((float) (occupied_cluster_data - (5 + indirect1_hold)) / indirect1_hold); /* needed cluster count / list capacity */
            occupied_cluster_extra += second_list_count;
        }
    }
    /* count extra clusters - END */

    /* direct - START */
    int end;
    if(occupied_cluster_data < 5){
        end = occupied_cluster_data;
    }else{
        end = 5;
    }

    int cluster_data_counter_cp = 0; /* counter for data clusters assigned to new file */
    int j = 0;
    for(; j < end; j++){
        int start_address = content_fs -> loaded_sb -> data_start_address + cluster_inode[j] * FORMAT_CLUSTER_SIZE_B;
        set_pointer_position_fs(start_address, SEEK_SET);
        int write_start_addr = content_fs -> loaded_sb -> data_start_address + clusters[cluster_data_counter_cp] * FORMAT_CLUSTER_SIZE_B;
        void *data;

        if(end < 5 && (j == end - 1)){ /* reading last cluster */
            int size_to_read = file_size - (j * FORMAT_CLUSTER_SIZE_B);
            data = read_from_file_fs(size_to_read); /* remaining size */
            set_pointer_position_fs(write_start_addr, SEEK_SET);
            write_to_file_fs(data, size_to_read);
            address_offset += size_to_read;
        }else{
            data = read_from_file_fs(FORMAT_CLUSTER_SIZE_B);
            set_pointer_position_fs(write_start_addr, SEEK_SET);
            write_to_file_fs(data, FORMAT_CLUSTER_SIZE_B);
            address_offset += FORMAT_CLUSTER_SIZE_B;
        }

        cluster_data_counter_cp += 1;

        free(data);
    }
    /* direct - END */

    /* read indirect */
    if(occupied_cluster_extra > 0){ /* first extra cluster is a list for indirect1 */
        int loop_end;
        if((occupied_cluster_data - 5) > indirect1_hold){
            loop_end = indirect1_hold;
        }else{
            loop_end = occupied_cluster_data - 5;
        }
        int start_address = content_fs -> loaded_sb -> data_start_address + cluster_inode[5] * FORMAT_CLUSTER_SIZE_B;
        set_pointer_position_fs(start_address, SEEK_SET);
        int *clust_indir1_arr = (int *) read_from_file_fs(loop_end * sizeof(int));
        j = 0;
        for(; j < loop_end; j++){
            int start_address = content_fs -> loaded_sb -> data_start_address + clust_indir1_arr[j] * FORMAT_CLUSTER_SIZE_B;
            set_pointer_position_fs(start_address, SEEK_SET);
            int write_start_addr = content_fs -> loaded_sb -> data_start_address + clusters[cluster_data_counter_cp] * FORMAT_CLUSTER_SIZE_B;
            void *data;
            if(loop_end < indirect1_hold && (j == loop_end - 1)){ /* reading last cluster */
                int size_to_read = file_size - ((j + 5) * FORMAT_CLUSTER_SIZE_B);
                data = read_from_file_fs(size_to_read); /* remaining size */
                set_pointer_position_fs(write_start_addr, SEEK_SET);
                write_to_file_fs(data, size_to_read);
                address_offset += size_to_read;
            }else{
                data = read_from_file_fs(FORMAT_CLUSTER_SIZE_B);
                set_pointer_position_fs(write_start_addr, SEEK_SET);
                write_to_file_fs(data, FORMAT_CLUSTER_SIZE_B);
                address_offset += FORMAT_CLUSTER_SIZE_B;
            }

            cluster_data_counter_cp += 1;
            free(data);
        }
        free(clust_indir1_arr);
        if(occupied_cluster_extra > 1){ /* second extra cluster is a first list for indirect2 */
            int second_list_count = ceil((float) (occupied_cluster_data - 5 - indirect1_hold) / indirect1_hold);
            int start_address = content_fs -> loaded_sb -> data_start_address + cluster_inode[6] * FORMAT_CLUSTER_SIZE_B;
            set_pointer_position_fs(start_address, SEEK_SET);
            int *second_list_arr = (int *) read_from_file_fs(sizeof(int) * second_list_count);

            int j = 0;
            for(; j < second_list_count; j++){
                int start_address_sec_list = content_fs -> loaded_sb -> data_start_address + second_list_arr[j] * FORMAT_CLUSTER_SIZE_B;
                set_pointer_position_fs(start_address_sec_list, SEEK_SET);
                int *clusters_arr;
                if((occupied_cluster_data - 5 - indirect1_hold - (indirect1_hold * j)) < indirect1_hold){
                    loop_end = occupied_cluster_data - 5 - indirect1_hold - (indirect1_hold * j);
                }else{
                    loop_end = indirect1_hold;
                }
                clusters_arr = read_from_file_fs(loop_end * sizeof(int));
                void *data;
                int k = 0;
                for(; k < loop_end; k++){
                    set_pointer_position_fs(content_fs -> loaded_sb -> data_start_address + clusters_arr[k] * FORMAT_CLUSTER_SIZE_B, SEEK_SET);
                    int write_start_addr = content_fs -> loaded_sb -> data_start_address + clusters[cluster_data_counter_cp] * FORMAT_CLUSTER_SIZE_B;
                    if(loop_end < indirect1_hold && (k == loop_end - 1)){ /* reading last cluster */
                        int size_to_read = file_size - ((5 + indirect1_hold + (indirect1_hold * j) + k) * FORMAT_CLUSTER_SIZE_B);
                        data = read_from_file_fs(size_to_read); /* remaining size */
                        set_pointer_position_fs(write_start_addr, SEEK_SET);
                        write_to_file_fs(data, size_to_read);
                        address_offset += size_to_read;
                    }else{
                        data = read_from_file_fs(FORMAT_CLUSTER_SIZE_B);
                        set_pointer_position_fs(write_start_addr, SEEK_SET);
                        write_to_file_fs(data, FORMAT_CLUSTER_SIZE_B);
                        address_offset += FORMAT_CLUSTER_SIZE_B;
                    }
                    cluster_data_counter_cp += 1;

                    free(data);
                }
                free(clusters_arr);
            }
            free(second_list_arr);
        }
    }
    save_bitmap_clusters(content_fs -> loaded_bit_clus);

    /* mark originally used clusters as free */
    close_file_fs();
    mark_cluster_unused_defrag(pseudo_in_to_move -> nodeid);
    open_wb_add_fs();

    /* marked wanted clusters as used again, some were freed in previous step */
    j = 0;
    for(; j < clusters_length; j++){
        if(content_fs -> loaded_bit_clus -> cluster_bitmap[clusters[j]] == 0){
            content_fs -> loaded_bit_clus -> cluster_occupied_total++;
            content_fs -> loaded_bit_clus -> cluster_free_total--;
            content_fs -> loaded_bit_clus -> cluster_bitmap[clusters[j]] = 1;
        }
    }
    save_bitmap_clusters(content_fs -> loaded_bit_clus);

    /* assign clusters to inode - START */
    /* assign direct */
    int *cluster_direct[5];

    cluster_direct[0] = &pseudo_in_to_move -> direct1;
    cluster_direct[1] = &pseudo_in_to_move -> direct2;
    cluster_direct[2] = &pseudo_in_to_move -> direct3;
    cluster_direct[3] = &pseudo_in_to_move -> direct4;
    cluster_direct[4] = &pseudo_in_to_move -> direct5;

    end;
    if(file_size <= (FORMAT_CLUSTER_SIZE_B * 5)){
        end = ceil((float) file_size / FORMAT_CLUSTER_SIZE_B);
    }else{
        end = 5;
    }

    int i = 0;
    for(; i < end; i++){
        *cluster_direct[i] = clusters[i];
    }

    /* assign indirect */
    if(occupied_cluster_extra > 0){ /* first extra cluster is a list for indirect1 */
        int *indirect1_arr = NULL; /* array of clusters which will be saved on indirect1 */
        int rem_data_clus = occupied_cluster_data - 5; /* clusters with data count, not assigned */

        int num_items;
        if(rem_data_clus < indirect1_hold){ /* less unassigned than cluster is able to hold => indirect2 will not be used */
            num_items = rem_data_clus;
        }else{
            num_items = indirect1_hold;
        }

        indirect1_arr = malloc(sizeof(int) * num_items);

        i = 0;
        for(; i < num_items; i++){ /* fill indirect1 array */
            indirect1_arr[i] = clusters[5 + i]; /* 5*direct */
        }

        /* array filled, now write data to first extra cluster */
        int indirect1_list_clust = clusters[clusters_length - 1 - occupied_cluster_extra + 1]; /* find first extra cluster */
        pseudo_in_to_move -> indirect1 = indirect1_list_clust;
        int indirect1_list_start_addr = (content_fs -> loaded_sb -> data_start_address) + (indirect1_list_clust * FORMAT_CLUSTER_SIZE_B);
        set_pointer_position_fs(indirect1_list_start_addr, SEEK_SET);
        write_to_file_fs((void *) indirect1_arr, sizeof(int) * num_items);
        rem_data_clus -= num_items; /* update unassigned cluster count */
        free(indirect1_arr);

        if(occupied_cluster_extra > 1){ /* second extra cluster is a first list for indirect2 */
            /* fill second list for indirect2 - START */
            int second_list_count = ceil((float) (rem_data_clus) / indirect1_hold); /* needed cluster count / list capacity */

            i = 0;
            for(; i < second_list_count; i++){
                int indirect2_sec_list_clust = clusters[clusters_length - 1 - occupied_cluster_extra + 3 + i]; /* find address for extra cluster */
                int indirect2_sec_list_start_addr = (content_fs -> loaded_sb -> data_start_address) + (indirect2_sec_list_clust * FORMAT_CLUSTER_SIZE_B);
                set_pointer_position_fs(indirect2_sec_list_start_addr, SEEK_SET);

                if(i == (second_list_count - 1)){ /* in case of last cluster write */
                    write_to_file_fs((void *) clusters + (5 + indirect1_hold + (i * indirect1_hold)) *
                                                                                   sizeof(int), rem_data_clus * sizeof(int));
                    rem_data_clus = 0;
                }else{
                    write_to_file_fs((void *) clusters + (5 + indirect1_hold + (i * indirect1_hold)) *
                                                                                   sizeof(int), indirect1_hold * sizeof(int));
                    rem_data_clus -= indirect1_hold;
                }
            }
            /* fill second list for indirect2 - END */

            /* fill first list for indirect2 - START */
            int indirect2_first_list_clust = clusters[clusters_length - 1 - occupied_cluster_extra + 2]; /* find address for extra cluster */
            pseudo_in_to_move -> indirect2 = indirect2_first_list_clust;
            int indirect2_first_list_start_addr = (content_fs -> loaded_sb -> data_start_address) + (indirect2_first_list_clust * FORMAT_CLUSTER_SIZE_B);
            set_pointer_position_fs(indirect2_first_list_start_addr, SEEK_SET);

            write_to_file_fs((void *) clusters + (clusters_length - second_list_count) * sizeof(int),
                             sizeof(int) * second_list_count);
            /* fill first list for indirect1 - END */
        }
    }

    save_pseudo_inodes(content_fs -> loaded_pseudo_in_arr, (content_fs -> loaded_bit_in -> inodes_free_total + content_fs -> loaded_bit_in -> inodes_occupied_total),
                       (content_fs -> loaded_bit_clus -> cluster_free_total + content_fs -> loaded_bit_clus -> cluster_occupied_total),
                       (content_fs -> loaded_bit_in -> inodes_free_total + content_fs -> loaded_bit_in -> inodes_occupied_total));
    /* assign clusters to inode - END */

    close_file_fs();
}

/* ____________________________________________________________________________
    void perform_defrag()

    Performs command defrag (syntax: defrag), which is used for defragmentation
    of the file which contains file system. Ie.: First will be occupied data
    blocks and after them will be free data blocks.
   ____________________________________________________________________________
*/
void perform_defrag(){
    /* Make a cluster -> inode array. Index is number of cluster and value is respective inode id. - START */
    int *cluster_inode_arr = get_clust_inode_arr_defrag();

    struct pseudo_inode *occupied_pseudo_in = NULL;
    int moved_clust_index = 0; /* index of cluster to which occupied cluster can be moved */
    int total_inode_count = content_fs -> loaded_bit_in -> inodes_free_total + content_fs -> loaded_bit_in -> inodes_occupied_total;
    int i = 0;
    for(; i < total_inode_count; i++){
        if(content_fs -> loaded_bit_in -> inodes_bitmap[i] == 1){ /* occupied inode found */
            open_rb_fs();
            occupied_pseudo_in = &content_fs -> loaded_pseudo_in_arr[i]; /* (index is the same as with bitmap) */
            /* got one inode, check clusters */
            int *cluster_occupied = NULL; /* clusters already occupied by inode */
            int *cluster_wanted = NULL; /* clusters which inode will use after defrag */
            int cluster_arrs_free_in = 0; /* length of both array will be the same */
            int cluster_direct[5];
            /* find occupied direct cluster - START */
            cluster_direct[0] = occupied_pseudo_in -> direct1;
            cluster_direct[1] = occupied_pseudo_in -> direct2;
            cluster_direct[2] = occupied_pseudo_in -> direct3;
            cluster_direct[3] = occupied_pseudo_in -> direct4;
            cluster_direct[4] = occupied_pseudo_in -> direct5;
            int j = 0;
            for(; j < 5; j++){
                if(cluster_direct[j] != -1){
                    cluster_occupied = realloc(cluster_occupied, (cluster_arrs_free_in + 1) * sizeof(int));
                    cluster_wanted = realloc(cluster_wanted, (cluster_arrs_free_in + 1) * sizeof(int));
                    cluster_occupied[cluster_arrs_free_in] = cluster_direct[j];
                    cluster_wanted[cluster_arrs_free_in] = moved_clust_index;
                    cluster_arrs_free_in += 1;
                    moved_clust_index += 1;
                }
            }
            /* find occupied direct cluster - END */
            int occupied_cluster_data = ceil((float) occupied_pseudo_in -> file_size / FORMAT_CLUSTER_SIZE_B); /* number of clusters reserved for data */
            int occupied_cluster_extra = 0; /* number of extra clusters (for indirect pointer list etc.) */
            int indirect1_hold = FORMAT_CLUSTER_SIZE_B / sizeof(int);
            /* count extra clusters - START */
            if(occupied_cluster_data > 5){ /* direct pointers not enough, extra cluster for indirect1 */
                occupied_cluster_extra += 1;
                if(occupied_cluster_data > (5 + indirect1_hold)){ /* indirect2 */
                    occupied_cluster_extra += 1; /* for first cluster list */
                    int second_list_count = ceil((float) (occupied_cluster_data - (5 + indirect1_hold)) / indirect1_hold); /* needed cluster count / list capacity */
                    occupied_cluster_extra += second_list_count;
                }
            }
            /* count extra clusters - END */
            /* find occupied indirect cluster - START */
            int *second_list_arr = NULL;
            if(occupied_cluster_extra > 0){ /* first extra cluster is a list for indirect1 */
                int loop_end;
                if((occupied_cluster_data - 5) > indirect1_hold){
                    loop_end = indirect1_hold;
                }else{
                    loop_end = occupied_cluster_data - 5;
                }
                int start_address = content_fs -> loaded_sb -> data_start_address + occupied_pseudo_in -> indirect1 * FORMAT_CLUSTER_SIZE_B;
                set_pointer_position_fs(start_address, SEEK_SET);
                int *clust_indir1_arr = (int *) read_from_file_fs(loop_end * sizeof(int));
                j = 0;
                for (; j < loop_end; j++) {
                    cluster_occupied = realloc(cluster_occupied, (cluster_arrs_free_in + 1) * sizeof(int));
                    cluster_wanted = realloc(cluster_wanted, (cluster_arrs_free_in + 1) * sizeof(int));
                    cluster_occupied[cluster_arrs_free_in] = clust_indir1_arr[j];
                    cluster_wanted[cluster_arrs_free_in] = moved_clust_index;
                    cluster_arrs_free_in += 1;
                    moved_clust_index += 1;
                }
                free(clust_indir1_arr);
                if(occupied_cluster_extra > 1){ /* second extra cluster is a first list for indirect2 */
                    int second_list_count = ceil((float) (occupied_cluster_data - 5 - indirect1_hold) / indirect1_hold);
                    long int start_address = content_fs -> loaded_sb -> data_start_address + occupied_pseudo_in -> indirect2 * FORMAT_CLUSTER_SIZE_B;
                    set_pointer_position_fs(start_address, SEEK_SET);
                    second_list_arr = (int *) read_from_file_fs(sizeof(int) * second_list_count);
                    j = 0;
                    for(; j < second_list_count; j++){
                        int start_address_sec_list = content_fs -> loaded_sb -> data_start_address + second_list_arr[j] * FORMAT_CLUSTER_SIZE_B;
                        set_pointer_position_fs(start_address_sec_list, SEEK_SET);
                        int *clusters_arr;
                        if((occupied_cluster_data - 5 - indirect1_hold - (indirect1_hold * j)) < indirect1_hold){
                            loop_end = occupied_cluster_data - 5 - indirect1_hold - (indirect1_hold * j);
                        }else{
                            loop_end = indirect1_hold;
                        }
                        clusters_arr = read_from_file_fs(loop_end * sizeof(int));
                        int k = 0;
                        for(; k < loop_end; k++){
                            cluster_occupied = realloc(cluster_occupied, (cluster_arrs_free_in + 1) * sizeof(int));
                            cluster_wanted = realloc(cluster_wanted, (cluster_arrs_free_in + 1) * sizeof(int));
                            cluster_occupied[cluster_arrs_free_in] = clusters_arr[k];
                            cluster_wanted[cluster_arrs_free_in] = moved_clust_index;
                            cluster_arrs_free_in += 1;
                            moved_clust_index += 1;
                        }
                        free(clusters_arr);
                    }
                }
            }
            /* find occupied indirect cluster - END */
            /* first list for indirect1 - START */
            if(occupied_cluster_extra > 0){
                cluster_occupied = realloc(cluster_occupied, (cluster_arrs_free_in + 1) * sizeof(int));
                cluster_wanted = realloc(cluster_wanted, (cluster_arrs_free_in + 1) * sizeof(int));
                cluster_occupied[cluster_arrs_free_in] = occupied_pseudo_in -> indirect1;
                cluster_wanted[cluster_arrs_free_in] = moved_clust_index;
                cluster_arrs_free_in += 1;
                moved_clust_index += 1;
            }
            /* first list for indirect1 - END */
            if(occupied_cluster_extra > 1){
                /* first list for indirect2 - START */
                cluster_occupied = realloc(cluster_occupied, (cluster_arrs_free_in + 1) * sizeof(int));
                cluster_wanted = realloc(cluster_wanted, (cluster_arrs_free_in + 1) * sizeof(int));
                cluster_occupied[cluster_arrs_free_in] = occupied_pseudo_in -> indirect2;
                cluster_wanted[cluster_arrs_free_in] = moved_clust_index;
                cluster_arrs_free_in += 1;
                moved_clust_index += 1;
                /* first list for indirect2 - END */
                /* second list for indirect2 - START */
                j = 0;
                int second_list_count = ceil((float) (occupied_cluster_data - 5 - indirect1_hold) / indirect1_hold);
                for(; j < second_list_count; j++){
                    cluster_occupied = realloc(cluster_occupied, (cluster_arrs_free_in + 1) * sizeof(int));
                    cluster_wanted = realloc(cluster_wanted, (cluster_arrs_free_in + 1) * sizeof(int));
                    cluster_occupied[cluster_arrs_free_in] = second_list_arr[j];
                    cluster_wanted[cluster_arrs_free_in] = moved_clust_index;
                    cluster_arrs_free_in += 1;
                    moved_clust_index += 1;
                }
                free(second_list_arr);
                /* second list for indirect2 - END */
            }
            /* find occupied indirect cluster - END */

            close_file_fs();
            open_wb_add_fs();
            /* go through wanted clusters - START */
            int *cluster_move = NULL;
            int cluster_move_length = 0;
            j = 0;
            for(; j < cluster_arrs_free_in; j++){
                if(content_fs -> loaded_bit_clus -> cluster_bitmap[cluster_wanted[j]] == 0){ /* cluster is not occupied */
                    content_fs -> loaded_bit_clus -> cluster_occupied_total++;
                    content_fs -> loaded_bit_clus -> cluster_free_total--;
                    content_fs -> loaded_bit_clus -> cluster_bitmap[cluster_wanted[j]] = 1;
                }else{ /* cluster is already occupied by another inode, make a list of clusters which should be moved */
                    cluster_move = realloc(cluster_move, (cluster_move_length + 1) * sizeof(int));
                    cluster_move[cluster_move_length] = cluster_wanted[j];
                    cluster_move_length += 1;
                }
            }
            save_bitmap_clusters(content_fs -> loaded_bit_clus);
            close_file_fs();
            /* go through wanted clusters - END */

            /* move content of cluster_move to other clusters */
            if(cluster_move_length != 0){
                move_inode_clust_defrag(cluster_inode_arr, cluster_move, cluster_move_length);
                open_wb_add_fs();
                j = 0;
                for(; j < cluster_arrs_free_in; j++){
                    /* update cluster_inode_arr for wanted cluster */
                    cluster_inode_arr[cluster_wanted[j]] = occupied_pseudo_in -> nodeid;
                }
                save_bitmap_clusters(content_fs -> loaded_bit_clus);
                close_file_fs();
                move_inode_spec_clust_defrag(occupied_pseudo_in -> nodeid, cluster_wanted, cluster_arrs_free_in);
                free(cluster_move);
            }
            free(cluster_occupied);
            free(cluster_wanted);
        }
    }
    /* traverse inodes and check occupied clusters - END */
    free(cluster_inode_arr);
}

/* ____________________________________________________________________________
    void perform_help()

    Performs command help (syntax: help), which is used for printing
    content of manual.
   ____________________________________________________________________________
*/
void perform_help(){
    printf(HELP_CP_CONT);
    printf(HELP_MV_CONT);
    printf(HELP_RM_CONT);
    printf(HELP_MKDIR_CONT);
    printf(HELP_RMDIR_CONT);
    printf(HELP_LS_CONT);
    printf(HELP_CAT_CONT);
    printf(HELP_CD_CONT);
    printf(HELP_PWD_CONT);
    printf(HELP_INFO_CONT);
    printf(HELP_INCP_CONT);
    printf(HELP_OUTCP_CONT);
    printf(HELP_LOAD_CONT);
    printf(HELP_FORMAT_CONT);
    printf(HELP_DEFRAG_CONT);
    printf(HELP_EXIT_CONT);
}
/* ____________________________________________________________________________
    void perform_exit()

    Performs command exit (syntax: exit), which is used for exiting.
    Function returns:
   ____________________________________________________________________________
*/
void perform_exit(){
    if(content_fs != NULL){
        free_fs_file_content(content_fs);
    }
}
