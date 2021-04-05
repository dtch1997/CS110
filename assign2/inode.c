#include <stdio.h>
#include <assert.h>

#include "inode.h"
#include "diskimg.h"

typedef int diskimg_block_t;
#define n_address_per_block (int)(DISKIMG_SECTOR_SIZE / sizeof(uint16_t))
const int DISKIMG_MAX_FILESIZE = 7 * n_address_per_block * DISKIMG_SECTOR_SIZE + n_address_per_block * n_address_per_block * DISKIMG_SECTOR_SIZE;
#define n_inode_per_sector (int)(DISKIMG_SECTOR_SIZE / sizeof(struct inode))

int inode_iget(struct unixfilesystem *fs, int inumber, struct inode *inp) {
    // Check that inumber is 1-indexed
    // Convert 1-indexed inumber to 0-indexed
    if (inumber == 0) return -1;
    inumber -= 1;

    int num_inodes = fs->superblock.s_isize * n_inode_per_sector;
    if (inumber >= num_inodes) return -1; // inumber out of range

    // calculate which sector to go to
    int sector_containing_inode = (inumber / n_inode_per_sector) + INODE_START_SECTOR ;
    int inode_index_within_sector = inumber % n_inode_per_sector;
    
    struct inode buf[n_inode_per_sector];
    int n_bytes_read = diskimg_readsector(fs->dfd, sector_containing_inode, buf);
    struct inode * tmp = buf + inode_index_within_sector;

    // Check: does this successfully copy tmp into inp?
    // Debugging with gdb suggests that it works!
    *inp = *tmp;
    return 0;
}

static diskimg_block_t lookup_address_block(struct unixfilesystem *fs, diskimg_block_t block, int index) {
    uint16_t buf[DISKIMG_SECTOR_SIZE / sizeof(uint16_t)];
    int n_bytes_read = diskimg_readsector(fs->dfd, block, buf);
    return buf[index];
}


int inode_indexlookup(struct unixfilesystem *fs, struct inode *inp, int fileBlockIndex) {
    const int filesize = inode_getsize(inp);
    if (!(inode_islarge(inp))) 
    {
        // All 8 addresses are direct block addresses
        if (fileBlockIndex >= 8) return -1;
        return (int) inp->i_addr[fileBlockIndex];
    } 
    else if (filesize <= DISKIMG_MAX_FILESIZE)
    {
        if (fileBlockIndex < 7 * n_address_per_block) {
            // Look up a singly indirect address
            int index1 = fileBlockIndex / n_address_per_block;
            int index2 = fileBlockIndex % n_address_per_block;
            diskimg_block_t address_block = inp->i_addr[index1];
            diskimg_block_t file_block = lookup_address_block(fs, address_block, index2);
            return file_block;
        }
        else
        {
            // Look up a doubly indirect address
            if (fileBlockIndex >= 7 * n_address_per_block + n_address_per_block * n_address_per_block) return -1;

            fileBlockIndex = fileBlockIndex - 7 * n_address_per_block;
            int index1 = 7;
            int index2 = fileBlockIndex / n_address_per_block;
            int index3 = fileBlockIndex % n_address_per_block;
            diskimg_block_t address_block1 = inp->i_addr[index1];
            diskimg_block_t address_block2 = lookup_address_block(fs, address_block1, index2);
            diskimg_block_t file_block = lookup_address_block(fs, address_block2, index3);
            return file_block;
        }
    }
    else
    {
        // file is too large
        return -1;
    }
}

int inode_getsize(struct inode *inp) {
    return ((inp->i_size0 << 16) | inp->i_size1); 
}

bool inode_islarge(const struct inode * const inp) {
    return (inp->i_mode & ILARG) != 0;
}

bool inode_isdir(const struct inode * const inp) {
    return (inp->i_mode & IFMT) == IFDIR;
}

bool inode_isalloc(const struct inode * const inp) {
    return (inp->i_mode & IALLOC) != 0;
}

