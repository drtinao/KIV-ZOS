#ifndef ZOS_LOAD_STRUCT_FS_H
#define ZOS_LOAD_STRUCT_FS_H

#define SB_READ_START "Reading superblock - START\n"
#define SB_INFO "Signature: %s, volume descriptor: %s, disc size: %d, cluster size: %d, cluster count: %d, cluster bitmap start addr.: %d, inode bitmap start addr.: %d, inode start addr.: %d data start addr.: %d\n"
#define SB_READ_END "Reading superblock - END\n"

#define BIT_CLUS_READ_START "Reading bitmap of clusters - START\n"
#define BIT_CLUS_STATE "Occupied count: %d, free count: %d\n"
#define BIT_CLUS_READ_END "Reading bitmap of clusters - END\n"

#define BIT_IN_READ_START "Reading bitmap of inodes - START\n"
#define BIT_IN_READ_STATE "Occupied count: %d, free count: %d\n"
#define BIT_IN_READ_END "Reading bitmap of inodes - END\n"

#define INODE_READ_START "Reading inodes - START\n"
#define INODE_READ_END "Reading inodes - END\n"


/* ____________________________________________________________________________

    Function prototypes
   ____________________________________________________________________________
*/
struct superblock * load_superblock();
struct bitmap_clusters * load_bitmap_clusters();
struct bitmap_inodes * load_bitmap_inodes(int cluster_count_total);
struct pseudo_inode * load_pseudo_inodes(int arr_length, int cluster_count_total, int inodes_count_total);
struct fs_file_content * load_fs_file();
void free_fs_file_content(struct fs_file_content *content_fs);

#endif //ZOS_LOAD_STRUCT_FS_H
