#include "def_struct_fs.h"

#ifndef ZOS_SAVE_STRUCT_FS_H
#define ZOS_SAVE_STRUCT_FS_H

#define WRITING_SUPERBLOCK_START "Writing superblock - START\n"
#define WRITING_SUPERBLOCK_SIZE "Size of superblock is %luB and is located at address: %ld-%ld\n"
#define WRITING_SUPERBLOCK_END "Writing superblock - END\n"

#define WRITING_BIT_CLUS_START "Writing bitmap of clusters - START\n"
#define WRITING_BIT_CLUS_SIZE "Size of bitmap of clusters is %luB and is located at address: %ld-%ld\n"
#define WRITING_BIT_CLUS_END "Writing bitmap of clusters - END\n"

#define WRITING_BIT_IN_START "Writing bitmap of inodes - START\n"
#define WRITING_BIT_IN_SIZE "Size of bitmap of inodes is %luB and is located at address: %ld-%ld\n"
#define WRITING_BIT_IN_END "Writing bitmap of inodes - END\n"

#define WRITING_INODES_START "Writing inodes - START\n"
#define WRITING_INODES_SIZE "Size of inode is %ldB, all inodes take: %luB (%ld x %d) and are located at address: %ld-%ld\n"
#define WRITING_INODES_END "Writing inodes - END\n"

#define WRITING_ZERO_START "Formatting data space - START\n"
#define WRITING_ZERO_SIZE "Size of cluster: %dB, all clusters take: %luB (%d x %d) and are located at address: %ld-%ld\n"
#define WRITING_ZERO_END "Formatting data space - END\n"

/* ____________________________________________________________________________

    Function prototypes
   ____________________________________________________________________________
*/
void save_superblock(struct superblock *sb_to_save);
void save_bitmap_clusters(struct bitmap_clusters *bit_clus_to_save);
void save_bitmap_inodes(struct bitmap_inodes *bit_in_to_save, int cluster_count_total);
void save_pseudo_inodes(struct pseudo_inode *pseudo_in_arr_to_save, int arr_length, int cluster_count_total, int inodes_count_total);
void format_data_space(int data_start_address, int cluster_count, int cluster_size);

#endif //ZOS_SAVE_STRUCT_FS_H
