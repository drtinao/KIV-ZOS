#include <stdio.h>

#include "save_struct_fs.h"
#include "io_fs.h"

int display_sb = 0;
int display_bit_in = 0;
int display_bit_clus = 0;
int display_in = 0;

/* ____________________________________________________________________________
    void save_superblock(struct superblock *sb_to_save)

    Saves data regarding superblock to data file.
    Superblock is located at the beginning of the FS.
   ____________________________________________________________________________
*/
void save_superblock(struct superblock *sb_to_save){
    if(display_sb == 0)
    printf(WRITING_SUPERBLOCK_START);

    /* move pointer to beginning of the FS */
    if(get_pointer_position_fs() != 0){
        set_pointer_position_fs(0, SEEK_SET);
    }

    long int before_pointer = get_pointer_position_fs();

    write_to_file_fs(sb_to_save, sizeof(struct superblock));
    long int after_pointer = get_pointer_position_fs();

    if(display_sb == 0)
    printf(WRITING_SUPERBLOCK_SIZE, sizeof(struct superblock), before_pointer, after_pointer);
    if(display_sb == 0)
    printf(WRITING_SUPERBLOCK_END);
    display_sb = 1;
}

/* ____________________________________________________________________________
    void save_bitmap_clusters(struct bitmap_clusters *bit_clus_to_save)

    Saves data regarding bitmap of clusters (struct bitmap_clusters),
    which represents state of clusters, to data file.
    Before bitmap with clusters is located superblock (in inode based FS).
   ____________________________________________________________________________
*/
void save_bitmap_clusters(struct bitmap_clusters *bit_clus_to_save){
    if(display_bit_clus == 0)
    printf(WRITING_BIT_CLUS_START);
    int bit_clus_start_pos = sizeof(struct superblock);

    /* move pointer to desired position of the FS */
    if(get_pointer_position_fs() != bit_clus_start_pos){
        set_pointer_position_fs(bit_clus_start_pos, SEEK_SET);
    }

    long int before_pointer = get_pointer_position_fs();
    write_to_file_fs((void *) bit_clus_to_save, sizeof(struct bitmap_clusters));
    /* write allocated memory for cluster bitmap */
    int i = 0;
    for(; i < bit_clus_to_save -> cluster_free_total; i++){
        write_to_file_fs((void *) &bit_clus_to_save -> cluster_bitmap[i], sizeof(bit_clus_to_save -> cluster_bitmap[0]));
    }
    long int after_pointer = get_pointer_position_fs();

    long int total_size_bit_clus = sizeof(struct bitmap_clusters) + (bit_clus_to_save -> cluster_free_total * sizeof(bit_clus_to_save -> cluster_bitmap[0])); /* total size of bitmap which holds clusters state */
    if(display_bit_clus == 0)
    printf(WRITING_BIT_CLUS_SIZE, total_size_bit_clus, before_pointer, after_pointer);
    if(display_bit_clus == 0)
    printf(WRITING_BIT_CLUS_END);
    display_bit_clus = 1;
}

/* ______________________________________________________________________________________________
    void save_bitmap_inodes(struct bitmap_inodes *bit_in_to_save, int cluster_count_total)

    Saves data regarding bitmap of inodes (struct bitmap_inodes),
    which represents state of inodes, to data file.
    Before bitmap with inodes is located superblock and bitmap with clusters (in inode based FS).
   ______________________________________________________________________________________________
*/
void save_bitmap_inodes(struct bitmap_inodes *bit_in_to_save, int cluster_count_total){
    if(display_bit_in == 0)
    printf(WRITING_BIT_IN_START);

    int bit_in_start_pos = sizeof(struct superblock) + sizeof(struct bitmap_clusters) + (cluster_count_total * sizeof(int));

    /* move pointer to desired position of the FS */
    if(get_pointer_position_fs() != bit_in_start_pos){
        set_pointer_position_fs(bit_in_start_pos, SEEK_SET);
    }

    long int before_pointer = get_pointer_position_fs();
    write_to_file_fs((void *) bit_in_to_save, sizeof(struct bitmap_inodes));
    /* write allocated memory for inode bitmap */
    int i = 0;
    for(; i < bit_in_to_save -> inodes_free_total; i++){
        write_to_file_fs((void *) &bit_in_to_save -> inodes_bitmap[i], sizeof(bit_in_to_save -> inodes_bitmap[0]));
    }
    long int after_pointer = get_pointer_position_fs();

    long int total_size_bit_in = sizeof(struct bitmap_inodes) + (bit_in_to_save -> inodes_free_total * sizeof(bit_in_to_save -> inodes_bitmap[0])); /* total size of bitmap which holds inodes state */
    if(display_bit_in == 0)
    printf(WRITING_BIT_IN_SIZE, total_size_bit_in, before_pointer, after_pointer);
    if(display_bit_in == 0)
    printf(WRITING_BIT_IN_END);
    display_bit_in = 1;
}

/* ______________________________________________________________________________________________________________________________________
    void save_pseudo_inodes(struct pseudo_inode *pseudo_in_arr_to_save, int arr_length, int cluster_count_total, int inodes_count_total)

    Saves data regarding inodes to data file.
    Before inodes is located superblock + bitmap with clusters + bitmap with inodes (in inode based FS).
   ______________________________________________________________________________________________________________________________________
*/
void save_pseudo_inodes(struct pseudo_inode *pseudo_in_arr_to_save, int arr_length, int cluster_count_total, int inodes_count_total){
    if(display_in == 0)
    printf(WRITING_INODES_START);

    int pseudo_in_start_pos = sizeof(struct superblock) + sizeof(struct bitmap_clusters) + (cluster_count_total * sizeof(int)) + sizeof(struct bitmap_inodes) + (inodes_count_total * sizeof(int));

    /* move pointer to desired position of the FS */
    if(get_pointer_position_fs() != pseudo_in_start_pos){
        set_pointer_position_fs(pseudo_in_start_pos, SEEK_SET);
    }

    long int before_pointer = get_pointer_position_fs();
    int i = 0;
    for(; i < arr_length; i++){
        write_to_file_fs((void *) (pseudo_in_arr_to_save+i), sizeof(struct pseudo_inode));
    }
    long int after_pointer = get_pointer_position_fs();

    long int total_size_pseudo_in = sizeof(struct pseudo_inode) * arr_length; /* total size of inodes */
    if(display_in == 0)
    printf(WRITING_INODES_SIZE, sizeof(struct pseudo_inode), total_size_pseudo_in, sizeof(struct pseudo_inode), arr_length, before_pointer, after_pointer);
    if(display_in == 0)
    printf(WRITING_INODES_END);
    display_in = 1;
}

/* _____________________________________________________________________________________
    void format_data_space(int data_start_address, int cluster_count, int cluster_size)

    Formats space of file which is reserved for data (ie.: writes 0).
   _____________________________________________________________________________________
*/
void format_data_space(int data_start_address, int cluster_count, int cluster_size){
    printf(WRITING_ZERO_START);

    /* move pointer to desired position of the FS */
    if(get_pointer_position_fs() != data_start_address){
        set_pointer_position_fs(data_start_address, SEEK_SET);
    }

    long int before_pointer = get_pointer_position_fs();
    int i = 0;
    char zero = 0;
    for(; i < cluster_count * cluster_size; i++){
        write_to_file_fs((void *) &zero, sizeof(char));
    }
    long int after_pointer = get_pointer_position_fs();

    long int total_size_clusters = cluster_count * cluster_size; /* total size of clusters */
    printf(WRITING_ZERO_SIZE, cluster_size, total_size_clusters, cluster_size, cluster_count, before_pointer, after_pointer);
    printf(WRITING_ZERO_END);
}