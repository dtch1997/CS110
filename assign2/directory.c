#include "directory.h"
#include "inode.h"
#include "diskimg.h"
#include "file.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

// remove the placeholder implementation and replace with your own
int directory_findname(struct unixfilesystem *fs, const char *name,
		       int dirinumber, struct direntv6 *dirEnt) {

    struct inode in;
    int err = inode_iget(fs, dirinumber, &in);
    if (err < 0) return -1; // Inode not found
    if (!(inode_isdir(&in))) return -1; // Not a directory

    int size = inode_getsize(&in);
    // Iterate over all the blocks in the directory
    for (int offset = 0; offset < size; offset += DISKIMG_SECTOR_SIZE) {
        int fileBlockIndex = offset / DISKIMG_SECTOR_SIZE;
        struct direntv6 buf[DISKIMG_SECTOR_SIZE / sizeof(struct direntv6)];
        int bytes_read = file_getblock(fs, dirinumber, fileBlockIndex, buf);
        if (bytes_read < 0) return -1;

        // Iterate over each entry in the block
        for (int i = 0; i < (int)sizeof(buf); i ++) {
            if (strncmp(buf[i].d_name, name, D_NAME_MAX_SIZE) == 0) {
                *dirEnt = buf[i];
                return 0;
            } 
        }
    }
    // Entry with specified name not found
    return -1;
}
