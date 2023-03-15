#ifndef ZOS_DEF_STRUCT_FS_H
#define ZOS_DEF_STRUCT_FS_H

#include <stdlib.h>
#include <stdbool.h>

/* STRUCTURES DEFINED BY TEACHER - START */

struct superblock {
    char signature[9];                 //FS author
    char volume_descriptor[251];       //description of generated FS
    int32_t disk_size;                 //total size of VFS
    int32_t cluster_size;              //cluster size
    int32_t cluster_count;             //cluster count
    int32_t bitmap_clus_start_address; //address of beginning bitmap of data blocks (clusters)
    int32_t bitmap_in_start_address;   //address of beginning bitmap of i-nodes (added by creator)
    int32_t inode_start_address;       //address of beginning of i-nodes
    int32_t data_start_address;        //address of beginning of data blocks
};


struct pseudo_inode {
    int32_t nodeid;                 //ID i-uzlu, pokud ID = ID_ITEM_FREE, je polozka volna
    bool isDirectory;               //soubor, nebo adresar
    int8_t references;              //počet odkazů na i-uzel, používá se pro hardlinky
    int32_t file_size;              //velikost souboru v bytech
    int32_t direct1;                // 1. přímý odkaz na datové bloky
    int32_t direct2;                // 2. přímý odkaz na datové bloky
    int32_t direct3;                // 3. přímý odkaz na datové bloky
    int32_t direct4;                // 4. přímý odkaz na datové bloky
    int32_t direct5;                // 5. přímý odkaz na datové bloky
    int32_t indirect1;              // 1. nepřímý odkaz (odkaz - datové bloky)
    int32_t indirect2;              // 2. nepřímý odkaz (odkaz - odkaz - datové bloky)
};

struct directory_item {
    int32_t inode;                   // inode odpovídající souboru
    char item_name[15];              //12+3 + /0 C/C++ ukoncovaci string znak
};
/* STRUCTURES DEFINED BY TEACHER - END */

struct bitmap_clusters{
    int *cluster_bitmap; /* array which contains state of clusters */
    int cluster_free_total; /* number of free clusters */
    int cluster_occupied_total; /* number of occupied clusters */
};

struct bitmap_inodes{
    int *inodes_bitmap; /* array which contains state of inodes */
    int inodes_free_total; /* number of free inodes */
    int inodes_occupied_total; /* number of occupied inodes */
};

struct fs_file_content{
    struct superblock *loaded_sb; /* superblock */
    struct bitmap_clusters *loaded_bit_clus; /* bitmap which represents state of clusters */
    struct bitmap_inodes *loaded_bit_in; /* bitmap which represents state of inodes */
    struct pseudo_inode *loaded_pseudo_in_arr; /* array of inodes */
};

struct free_clusters_struct{
    int *free_clusters_arr;
    int free_clusters_arr_length;
};

#endif //ZOS_DEF_STRUCT_FS_H
