
/************************************************************************/
// Project 4

/**
 *  Create a zero-length file within a specified diretory
 *
 *  @param parent Inode reference for the parent directory
 *  @param local_name Name of the file within the parent directory
 *  @return Inode reference index for the newly created file
 *          UNALLOCATED_INODE if an error
 *
 *  Errors include: virtual disk read/write errors, no available inodes,
 *    no available directory entrie
 */
INODE_REFERENCE oufs_create_file(INODE_REFERENCE parent, char *local_name)
{
  // Does the parent have a slot?
  INODE inode;

  // Read the parent inode
  if(oufs_read_inode_by_reference(parent, &inode) != 0) {
    return UNALLOCATED_INODE;
  }

  // Is the parent full?
  if(inode.size == N_DIRECTORY_ENTRIES_PER_BLOCK) {
    // Directory is full
    fprintf(stderr, "Parent directory is full.\n");
    return UNALLOCATED_INODE;
  }

  // TODO

  // Success
  return(inode_reference);
}

/**
 * Deallocate all of the blocks that are being used by an inode
 *
 * - Modifies the inode to set content to UNALLOCATED_BLOCK
 * - Adds any content blocks to the end of the free block list
 *    (these are added in the same order as they are in the file)
 * - If the file is using no blocks, then return success without
 *    modifications.
 * - Note: the inode is not written back to the disk (we will let
 *    the calling function handle this)
 *
 * @param inode A pointer to an inode structure that is already in memory
 * @return 0 if success
 *         -x if error
 */

int oufs_deallocate_blocks(INODE *inode)
{
  BLOCK master_block;
  BLOCK block;

  // Nothing to do if the inode has no content
  if(inode->content == UNALLOCATED_BLOCK)
    return(0);

  // TODO


  // Success
  return(0);
}

/**
 * Allocate a new data block
 * - If one is found, then the free block linked list is updated
 *
 * @param master_block A link to a buffer ALREADY containing the data from the master block.
 *    This buffer may be modified (but will not be written to the disk; we will let
 *    the calling function handle this).
 * @param new_block A link to a buffer into which the new block will be read.
 *
 * @return The index of the allocated data block.  If no blocks are available,
 *        then UNALLOCATED_BLOCK is returned
 *
 */
BLOCK_REFERENCE oufs_allocate_new_block(BLOCK *master_block, BLOCK *new_block)
{
  // Is there an available block?
  if(master_block->content.master.unallocated_front == UNALLOCATED_BLOCK) {
    // Did not find an available block
    if(debug)
      fprintf(stderr, "No blocks\n");
    return(UNALLOCATED_BLOCK);
  }

  // TODO

  return(block_reference);
}

