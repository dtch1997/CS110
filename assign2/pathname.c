
#include "pathname.h"
#include "directory.h"
#include "inode.h"
#include "diskimg.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "ino.h"
static int pathname_lookup_helper(struct unixfilesystem *fs, char ** pathname_ptr, int dirinumber) {
  
    char * filename = strsep(pathname_ptr, "/");
    assert(filename != NULL);

    struct direntv6 dirEnt;
    int err = directory_findname(fs, filename, dirinumber, &dirEnt);
    if (err < 0) return -1;
    assert(strncmp(dirEnt.d_name, filename, D_NAME_MAX_SIZE) == 0);
    int file_inumber = dirEnt.d_inumber;

    struct inode in; 
    err = inode_iget(fs, dirEnt.d_inumber, &in);
    if (err < 0) return -1;
    

    if (*pathname_ptr == NULL) {
        return file_inumber;
    }
    else {
        return pathname_lookup_helper(fs, pathname_ptr, file_inumber);
    }
}

int pathname_lookup(struct unixfilesystem *fs, const char *pathname) {
    assert(pathname[0] == '/'); // Make sure it is an absolute path
    pathname = pathname + 1; //Remove leading backslash
    if (strlen(pathname) == 0) return ROOT_INUMBER;

    const int PATHNAME_MAX_LEN = 1024;

    char buf[PATHNAME_MAX_LEN];
    char * buff = buf;
    strncpy(buff, pathname, PATHNAME_MAX_LEN);
    char** buf_ptr = (&buff);
    return pathname_lookup_helper(fs, buf_ptr, ROOT_INUMBER);
}
