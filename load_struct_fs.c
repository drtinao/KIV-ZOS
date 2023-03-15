#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "io_fs.h"
#include "def_struct_fs.h"
#include "load_struct_fs.h"

/* _______________________________________________________________________________
    struct superblock * load_superblock()

    Loads data regarding superblock from data file and puts them to the structure,
    finally returns pointer.
   _______________________________________________________________________________
*/
struct superblock * load_superblock(){
    struct superblock *load_sb;

    printf(SB_READ_START);
    /* move pointer to beginning of the FS */
    if(get_pointer_position_fs() != 0){
        set_pointer_position_fs(0, SEEK_SET);
    }

    load_sb = (struct superblock *) read_from_file_fs(sizeof(struct superblock));
    printf(SB_INFO,
           load_sb -> signature, load_sb -> volume_descriptor, load_sb -> disk_size, load_sb -> cluster_size,
           load_sb -> cluster_count, load_sb -> bitmap_clus_start_address, load_sb -> bitmap_in_start_address,
           load_sb -> inode_start_address, load_sb -> data_start_address);
    printf(SB_READ_END);

    return load_sb;
}

/* ________________________________________________________________________________________
    struct bitmap_clusters * load_bitmap_clusters()

    Loads data regarding bitmap which represents state of clusters (struct bitmap_clusters)
    from data file and returns pointer.
    Before bitmap with clusters is located superblock (in inode based FS).
   ________________________________________________________________________________________
*/
struct bitmap_clusters * load_bitmap_clusters(){
    printf(BIT_CLUS_READ_START);
    struct bitmap_clusters *load_bit_clus = NULL;

    int bit_clus_start_pos = sizeof(struct superblock);
    /* move pointer to desired position of the FS */
    if(get_pointer_position_fs() != bit_clus_start_pos){
        set_pointer_position_fs(bit_clus_start_pos, SEEK_SET);
    }

    load_bit_clus = (struct bitmap_clusters *) read_from_file_fs(sizeof(struct bitmap_clusters));
    /* load memory allocated for cluster bitmap */
    load_bit_clus -> cluster_bitmap = malloc((load_bit_clus -> cluster_free_total + load_bit_clus -> cluster_occupied_total) * sizeof(int)); /* malloc for total cluster count */

    int i = 0;
    for(; i < (load_bit_clus -> cluster_free_total + load_bit_clus -> cluster_occupied_total); i++){
        void *p_number_bitmap = read_from_file_fs(sizeof(int));
        load_bit_clus -> cluster_bitmap[i] = *((int*) p_number_bitmap);
        free(p_number_bitmap);
    }
    printf(BIT_CLUS_STATE, load_bit_clus -> cluster_occupied_total, load_bit_clus -> cluster_free_total);
    printf(BIT_CLUS_READ_END);

    return load_bit_clus;
}

/* ______________________________________________________________________________________________
    struct bitmap_inodes * load_bitmap_inodes(int cluster_count_total)

    Loads data regarding bitmap which represents state of inodes (struct bitmap_inodes)
    from data file and returns pointer.
    Before bitmap with inodes is located superblock and bitmap with clusters (in inode based FS).
   ______________________________________________________________________________________________
*/
struct bitmap_inodes * load_bitmap_inodes(int cluster_count_total){
    printf(BIT_IN_READ_START);
    struct bitmap_inodes *load_bit_in = NULL;

    int bit_in_start_pos = sizeof(struct superblock) + sizeof(struct bitmap_clusters) + (cluster_count_total * sizeof(int));

    /* move pointer to desired position of the FS */
    if(get_pointer_position_fs() != bit_in_start_pos){
        set_pointer_position_fs(bit_in_start_pos, SEEK_SET);
    }

    load_bit_in = (struct bitmap_inodes *) read_from_file_fs(sizeof(struct bitmap_inodes));
    /* load memory allocated for inodes bitmap */
    load_bit_in -> inodes_bitmap = malloc((load_bit_in -> inodes_free_total + load_bit_in -> inodes_occupied_total) * sizeof(int)); /* malloc for total cluster count */

    int i = 0;
    for(; i < (load_bit_in -> inodes_free_total + load_bit_in -> inodes_occupied_total); i++){
        void *p_number_bitmap = read_from_file_fs(sizeof(int));
        load_bit_in -> inodes_bitmap[i] = *((int*) p_number_bitmap);
        free(p_number_bitmap);
    }
    printf(BIT_IN_READ_STATE, load_bit_in -> inodes_occupied_total, load_bit_in -> inodes_free_total);
    printf(BIT_IN_READ_END);

    return load_bit_in;
}

/* ___________________________________________________________________________________________________________
    struct pseudo_inode * load_pseudo_inodes(int arr_length, int cluster_count_total, int inodes_count_total)

    Loads data regarding inodes from data file and returns pointer.
    Before inodes is located superblock + bitmap with clusters + bitmap with inodes (in inode based FS).
   ___________________________________________________________________________________________________________
*/
struct pseudo_inode * load_pseudo_inodes(int arr_length, int cluster_count_total, int inodes_count_total){
    printf(INODE_READ_START);
    struct pseudo_inode *load_pseudo_in = malloc(sizeof(struct pseudo_inode) * arr_length);

    int pseudo_in_start_pos = sizeof(struct superblock) + sizeof(struct bitmap_clusters) + (cluster_count_total * sizeof(int)) + sizeof(struct bitmap_inodes) + (inodes_count_total * sizeof(int));

    /* move pointer to desired position of the FS */
    if(get_pointer_position_fs() != pseudo_in_start_pos){
        set_pointer_position_fs(pseudo_in_start_pos, SEEK_SET);
    }

    int i = 0;
    for(; i < arr_length; i++){
        void *p_pseudo_inode = read_from_file_fs(sizeof(struct pseudo_inode));
        load_pseudo_in[i] = *((struct pseudo_inode*) p_pseudo_inode);
        free(p_pseudo_inode);
    }

    printf(INODE_READ_END);
    return  load_pseudo_in;
}

/* ___________________________________________________________________________________________________________
    struct fs_file_content * load_fs_file()

    Loads content regarding bitmaps (cluster + inode) and inodes from data file and puts them to structure.
    Finally, pointer to the structure is returned.
   ___________________________________________________________________________________________________________
*/
struct fs_file_content * load_fs_file(){
    struct fs_file_content *content_fs_struct = malloc(sizeof(struct fs_file_content));
    open_rb_fs();

    content_fs_struct -> loaded_sb = load_superblock();
    content_fs_struct -> loaded_bit_clus = load_bitmap_clusters();
    content_fs_struct -> loaded_bit_in = load_bitmap_inodes(content_fs_struct -> loaded_bit_clus -> cluster_occupied_total + content_fs_struct -> loaded_bit_clus -> cluster_free_total);
    content_fs_struct -> loaded_pseudo_in_arr = load_pseudo_inodes((content_fs_struct -> loaded_bit_in -> inodes_occupied_total + content_fs_struct -> loaded_bit_in -> inodes_free_total), (content_fs_struct -> loaded_bit_clus -> cluster_occupied_total + content_fs_struct -> loaded_bit_clus -> cluster_free_total), (content_fs_struct -> loaded_bit_in -> inodes_occupied_total + content_fs_struct -> loaded_bit_in -> inodes_free_total));

    close_file_fs();

    return content_fs_struct;
}

/* ____________________________________________________________________________
    void free_fs_file_content(struct fs_file_content *content_fs)

    Function is used to free all content which is pointed to by
    "struct fs_file_content *content_fs" and its inner pointers.
   ____________________________________________________________________________
*/
void free_fs_file_content(struct fs_file_content *content_fs){
    free(content_fs -> loaded_sb);
    free(content_fs -> loaded_bit_clus -> cluster_bitmap);
    free(content_fs -> loaded_bit_clus);
    free(content_fs -> loaded_bit_in -> inodes_bitmap);
    free(content_fs -> loaded_bit_in);
    free(content_fs -> loaded_pseudo_in_arr);
    free(content_fs);
}