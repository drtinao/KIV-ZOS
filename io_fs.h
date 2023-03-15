#ifndef ZOS_IO_FS_H
#define ZOS_IO_FS_H
#define STRUCT_SUPERBLOCK 1
#define STRUCT_BITMAP_CLUSTERS 2
#define STRUCT_BITMAP_INODES 3

#include <stdbool.h>
/* ____________________________________________________________________________

    Function prototypes
   ____________________________________________________________________________
*/
void set_filename_fs(char *name);
bool check_file_existence(char *filename);
char * get_filename_fs();
int open_wb_fs();
int open_wb_add_fs();
int open_rb_fs();
void close_file_fs();
long int get_pointer_position_fs();
void set_pointer_position_fs(long int offset, int whence);
void write_to_file_fs(void *data_to_write, int size_to_write);
void * read_from_file_fs(int bytes_count);
void * read_file_extern(char *filename, int start_addr, int end_addr);
long get_file_size(char *filename);
void write_file_extern(void *data_to_write, char *filename, int start_addr, int end_addr);

#endif //ZOS_IO_FS_H
