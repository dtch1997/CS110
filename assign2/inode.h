#ifndef _INODE_H
#define _INODE_H

#include <stdbool.h>
#include "unixfilesystem.h"

/**
 * Fetches the specified inode from the filesystem. 
 * Returns 0 on success, -1 on error.
 *
 * @param  inumber: 1-indexed
 */
int inode_iget(struct unixfilesystem *fs, int inumber, struct inode *inp); 

/**
 * Given an index of a file block, retrieves the file's actual block number
 * from the given inode.
 * @param  fileBlockIndex: 0-indexed
 *
 * Returns the disk block number on success, -1 on error.  
 */
int inode_indexlookup(struct unixfilesystem *fs, struct inode *inp, int fileBlockIndex);

/**
 * Computes the size in bytes of the file identified by the given inode
 */
int inode_getsize(struct inode *inp);

/**
 * Returns true if file uses large file mapping scheme and false otherwise
 */
bool inode_islarge(const struct inode * const inp);

/**
 * Returns true if file is a directory and false otherwise
 */
bool inode_isdir(const struct inode * const inp);

/**
 * Returns true if inode is allocated and false otherwise
 */
bool inode_isalloc(const struct inode * const inp);

#endif // _INODE_
