#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "def_struct_fs.h"

char *filename_fs = NULL; /* name of file which will be used as core for inode filesystem simulation (given by user) */
FILE* pointer_fs_file = NULL; /* pointer to file */

/* ____________________________________________________________________________
    void set_filename_fs(char *name)

    Sets a name of file which will be used as a core for inode filesystem simulation.
   ____________________________________________________________________________
*/
void set_filename_fs(char *name){
    filename_fs = name;
}

/* ____________________________________________________________________________
    bool check_file_existence(char *filename)

    Checks whether file specified by "char *filename" exists or not.
   ____________________________________________________________________________
*/
bool check_file_existence(char *filename){
    if(access(filename, F_OK) != -1){
        return true;
    }else{
        return false;
    }
}

/* ____________________________________________________________________________
    char * get_filename_fs()

    Returns a name of file which will be used as a core for inode filesystem simulation.
   ____________________________________________________________________________
*/
char * get_filename_fs(){
    return filename_fs;
}

/* ____________________________________________________________________________
    int open_wb_fs()

    Opens user specified file in binary write mode.
   ____________________________________________________________________________
*/
int open_wb_fs(){
    pointer_fs_file = fopen(filename_fs, "wb");

    if(pointer_fs_file == NULL){ //error occured while creating file
        return EXIT_FAILURE;
    }else{
        return EXIT_SUCCESS;
    }
}

/* ____________________________________________________________________________

    int open_wb_add_fs()

    Opens user specified file in r+b mode.
   ____________________________________________________________________________
*/
int open_wb_add_fs(){
    pointer_fs_file = fopen(filename_fs, "r+b");

    if(pointer_fs_file == NULL){ //error occured while creating file
        return EXIT_FAILURE;
    }else{
        return EXIT_SUCCESS;
    }
}

/* ____________________________________________________________________________
    int open_rb_fs()

    Opens user specified file in binary read mode.
   ____________________________________________________________________________
*/
int open_rb_fs(){
    pointer_fs_file = fopen(filename_fs, "rb");

    if(pointer_fs_file == NULL){ //error occured while creating
        return EXIT_FAILURE;
    }else{
        return EXIT_SUCCESS;
    }
}

/* ____________________________________________________________________________

    void close_file_fs()

    Closes opened file with filesystem.
   ____________________________________________________________________________
*/
void close_file_fs(){
    fclose(pointer_fs_file);
}

/* ____________________________________________________________________________

    long int get_pointer_position_fs()

    Returns current position of pointer in the opened file.
   ____________________________________________________________________________
*/
long int get_pointer_position_fs(){
    return ftell(pointer_fs_file);
}

/* ____________________________________________________________________________

    void set_pointer_position_fs(long int offset, int whence)

    Sets position of pointer in the opened file.
   ____________________________________________________________________________
*/
void set_pointer_position_fs(long int offset, int whence){
    fseek(pointer_fs_file, offset, whence);
}

/* ___________________________________________________________________________________

    void write_to_file_fs(void *data_to_write, int size_to_write)

    Writes data which are given by pointer to file which represents inode filesystem.
   ___________________________________________________________________________________
*/
void write_to_file_fs(void *data_to_write, int size_to_write){
    fwrite(data_to_write, 1, size_to_write, pointer_fs_file);
}

/* ____________________________________________________________________________
    void * read_from_file_fs(int bytes_count)

    Reads n bytes (specified by "bytes_count") from actual position
    of the pointer to fs file and returns pointer to desired content.
   ____________________________________________________________________________
*/
void * read_from_file_fs(int bytes_count){
    void *pointer_content = malloc(sizeof(char) * bytes_count);

    fread(pointer_content, bytes_count, 1, pointer_fs_file);
    return pointer_content;
}

/* ____________________________________________________________________________
    void * read_file_extern(char *filename, int start_addr, int end_addr)

    Reads part of file specified by "char *filename" in binary and returns
    pointer to desired content.
   ____________________________________________________________________________
*/
void * read_file_extern(char *filename, int start_addr, int end_addr){
    FILE *pointer_file = NULL;
    void *content_file = NULL;

    pointer_file = fopen(filename, "rb");

    fseek(pointer_file, start_addr, SEEK_SET);
    content_file = malloc(end_addr - start_addr);
    fread(content_file, 1, (end_addr - start_addr), pointer_file);

    fclose(pointer_file);
    return content_file;
}

/* ____________________________________________________________________________
    long get_file_size(char *filename)

    Returns size of file specified by "char *filename".
   ____________________________________________________________________________
*/
long get_file_size(char *filename){
    FILE *pointer_file = NULL;
    long file_size = 0;

    pointer_file = fopen(filename, "rb");
    fseek(pointer_file, 0, SEEK_END);
    file_size = ftell(pointer_file);
    fclose(pointer_file);

    return file_size;
}

/* ___________________________________________________________________________________________
    void write_file_extern(void *data_to_write, char *filename, int start_addr, int end_addr)

    Writes content specified by "void *data_to_write" to file named as "char *filename".
   ___________________________________________________________________________________________
*/
void write_file_extern(void *data_to_write, char *filename, int start_addr, int end_addr){
    FILE *pointer_file = NULL;

    if(start_addr == 0){ /* create new file */
        pointer_file = fopen(filename, "wb");
    }else{
        pointer_file = fopen(filename, "r+b");
    }

    fseek(pointer_file, start_addr, SEEK_SET);
    fwrite(data_to_write, 1, (end_addr - start_addr), pointer_file);
    fclose(pointer_file);
    free(data_to_write);
}




