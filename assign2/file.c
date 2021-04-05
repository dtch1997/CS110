#include <stdio.h>
#include <assert.h>

#include "file.h"
#include "inode.h"
#include "diskimg.h"

// remove the placeholder implementation and replace with your own
int file_getblock(struct unixfilesystem *fs, int inumber, int fileBlockIndex, void *buf) {
    struct inode node;
    struct inode * inp = &node;
    int err = inode_iget(fs, inumber, inp);
    if (err < 0) return -1;  

    int block_num = inode_indexlookup(fs, inp, fileBlockIndex);
    if (block_num < 0) return -1;
    int bytes_read = diskimg_readsector(fs->dfd, block_num, buf);
    if (bytes_read < 0) return -1;

    const int filesize = inode_getsize(inp);
    if (filesize % DISKIMG_SECTOR_SIZE == 0) 
    {
        int num_blocks = filesize / DISKIMG_SECTOR_SIZE;
        if (fileBlockIndex >= num_blocks) return -1;
        return DISKIMG_SECTOR_SIZE;
    }
    else 
    {
        // Figure out whether this is the last block
        int num_blocks = filesize / DISKIMG_SECTOR_SIZE + 1;
        if (fileBlockIndex >= num_blocks) return -1;
        else if (fileBlockIndex == num_blocks - 1) return filesize % DISKIMG_SECTOR_SIZE;
        else return DISKIMG_SECTOR_SIZE;
    }
}
