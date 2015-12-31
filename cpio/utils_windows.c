/*
 * utils_windows.c
 * 
 * Copyright 2013 Trevor Drake <trevd1234@gmail.com>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 * 
 * 
 */
#ifdef __WIN32__

#include <string.h>

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "utils_windows.h"
#define D( ...) 
#define WINDOWS_SYMBOLIC_LINK_FILE_MAGIC "LNK:"
#define WINDOWS_SYMBOLIC_LINK_FILE_MAGIC_SIZE 4

unsigned char* read_from_block_device(const char *name, unsigned* data_size){
    return NULL;
}

/* symlink - creates a text file which contains the path to the 
 * symlink source. the path is prefixed by the magic LNK: to identify
 * itself as a symlink. 
 * Although a little bit lame I chose this method instead of the 
 * CreateSymbolicLink windows api as this is more portable and should 
 * be okay on all windows version
 */
int symlink(const char *source, const char *path){
    
    size_t source_size = strlen(source); 
    errno = 0 ; 
    FILE *output_file_fp = fopen(path, "wb");
    if (output_file_fp != NULL)
    {
        fwrite(WINDOWS_SYMBOLIC_LINK_FILE_MAGIC,
               WINDOWS_SYMBOLIC_LINK_FILE_MAGIC_SIZE,
               1,
               output_file_fp);
        
        fwrite(source,source_size,1,output_file_fp);
        
        fclose(output_file_fp);
    }
    return 0;
}
ssize_t readlink(const char *path, char *buf, size_t bufsiz){
    
    FILE *symlink_file_fp = fopen(path, "rb");
    if ( symlink_file_fp != NULL ){
        fseek(symlink_file_fp,4,SEEK_SET);
        fread(buf,bufsiz,1,symlink_file_fp);
        fclose(symlink_file_fp);
    }
    return bufsiz;
}

/* lstat - get file status
 * Windows implementation for lstat which accounts for "our" method
 * of representing symbolic links on the windows platform
 * 
 * Symbolic links are regular files on the windows platform, this implementation
 * looks for the symbolic link magic of LNK: and set the symlink mode flag if 
 * it is found. 
 */ 
int lstat(const char *path, struct stat *buf){
    
    errno = 0 ; 
    
    if(stat(path,buf) == -1 ) {
        D("failed to stat %s err: %d %s\n",path,errno,strerror(errno));
        return -1;
    }
    
    // regular file check
    if( ! S_ISREG(buf->st_mode) ) {
        D("no need to check %s - not a regular file\n",path);
        return 0 ;
    }
    
    // size check - don't check if the file size is less than 5 bytes
    if ( buf->st_size <= WINDOWS_SYMBOLIC_LINK_FILE_MAGIC_SIZE ){
        D("no need to check %s - regular file is too small %ld to hold symlink data\n",path,buf->st_size);
        return 0 ;
    }

    // regular file found - open  it up 
    FILE *symlink_file_fp = fopen(path, "rb");
    if ( symlink_file_fp == NULL ) {
        D("failed to open %s for symlink check err: %d %s\n",path,errno,strerror(errno));
        return -1;
    }
    
    // allocate a buffer for magic checking    
    char* magic_check_buffer = calloc(WINDOWS_SYMBOLIC_LINK_FILE_MAGIC_SIZE,sizeof(char));
    if ( magic_check_buffer == NULL ){
        
        // allocation failed - close symlink_file_fp and exit
        D("failed to allocate magic_check_buffer err: %d %s\n",errno,strerror(errno));
        goto exit_closefile;
    }
           
         
    // Read the first 4 bytes in the magic_check_buffer
    if ( ! fread(   magic_check_buffer,
                    WINDOWS_SYMBOLIC_LINK_FILE_MAGIC_SIZE,1,
                    symlink_file_fp) 
    ){ 
        // file read failed - free magic_check_buffer memory, close symlink_file_fp and exit
        D("failed to read contents of %s into magic_check_buffer err: %d %s\n",path,errno,strerror(errno));
        goto exit_freememory;
    }
    
    D("mode %d\n",buf->st_mode);
    
    // check to see if we have a link file
    if( ! strncmp(magic_check_buffer,
                  WINDOWS_SYMBOLIC_LINK_FILE_MAGIC,
                  WINDOWS_SYMBOLIC_LINK_FILE_MAGIC_SIZE)
    ){
        // we do switch the link flag in mode
        buf->st_mode = (buf->st_mode | S_IFLNK);
        D("LINK:mode %d\n",buf->st_mode);
        
        buf->st_size -= 4 ; 

    }
    errno = 0 ;     
    
    
exit_freememory:
   // internal_errno = errno ;
    if(magic_check_buffer != NULL ) free(magic_check_buffer);
    // fall on through and close the file
exit_closefile:
    //internal_errno = errno ;
    if( symlink_file_fp != NULL ) fclose(symlink_file_fp) ;
    //errno = internal_errno ; 
    return (!errno) ? 0 : -1;
 
}
#else
#define NOTHING_TO_COMPILE "Yes"
#endif
