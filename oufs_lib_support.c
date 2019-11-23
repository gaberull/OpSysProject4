/**
 *  Project 3
 *  oufs_lib_support.c
 *
 *  Author: CS3113
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include "virtual_disk.h"
#include "oufs_lib_support.h"

extern int debug;

/**
 * Deallocate a single block.
 * - Modify the in-memory copy of the master block
 * - Add the specified block to THE END of the free block linked list
 * - Modify the disk copy of the deallocated block: next_block points to
 *     UNALLOCATED_BLOCK
 *
 *
 * @param master_block Pointer to a loaded master block.  Changes to the MB will
 *           be made here, but not written to disk
 *
 * @param block_reference Reference to the block that is being deallocated
 *
 */
int oufs_deallocate_block(BLOCK *master_block, BLOCK_REFERENCE block_reference)
{
    BLOCK b;
    
    if(master_block->content.master.unallocated_front == UNALLOCATED_BLOCK) {
        // No blocks on the free list.  Both pointers point to this block now
        master_block->content.master.unallocated_front = master_block->content.master.unallocated_end =
        block_reference;
        //fprintf(stderr, "Resetting master unallocated end and front to point to newly empty block.\n");
        
    }else{
        BLOCK prevEndBlock;
        BLOCK_REFERENCE prevEnd;
        prevEnd = master_block->content.master.unallocated_end;
        if(virtual_disk_read_block(prevEnd, &prevEndBlock) != 0) {
            fprintf(stderr, "deallocate_block: error reading old end block\n");
            return(-1);
        }
        
        fprintf(stderr, "inside oufs_deallocate_block: deallocating block_reference %d\n", block_reference);
        prevEndBlock.next_block = block_reference;
        
        if(virtual_disk_write_block(prevEnd, &prevEndBlock) != 0) {
            //fprintf(stderr, "deallocate_block: error writing old end block\n");
            return(-1);
        }
        
        master_block->content.master.unallocated_end = block_reference;
        
        //fprintf(stderr, "Resetting master unallocated end to point to newly empty block.\n");
    }
    
    //add block back to unallocated block list
    
    // Update the new end block
    if(virtual_disk_read_block(block_reference, &b) != 0) {
        fprintf(stderr, "deallocate_block: error reading new end block\n");
        return(-1);
    }
    
    // Change the new end block to point to nowhere
    b.next_block = UNALLOCATED_BLOCK;
    for (int i=0; i<N_DIRECTORY_ENTRIES_PER_BLOCK; i++)
    {
        b.content.directory.entry[i].inode_reference = UNALLOCATED_INODE;
    }
    
    // Write the block back
    if(virtual_disk_write_block(block_reference, &b) != 0) {
        fprintf(stderr, "deallocate_block: error writing new end block\n");
        return(-1);
    }
    
    //fprintf(stderr, "Writing block back to disk.\n");
    return(0);
};


/**
 *  Initialize an inode and a directory block structure as a new directory.
 *  - Inode points to directory block (self_block_reference)
 *  - Inode size = 2 (for . and ..)
 *  - Directory block: add entries . (self_inode_reference and .. (parent_inode_reference)
 *  -- Set all other entries to UNALLOCATED_INODE
 *
 * @param inode Pointer to inode structure to initialize
 * @param block Pointer to block structure to initialize as a directory
 * @param self_block_reference The block reference to the new directory block
 * @param self_inode_reference The inode reference to the new inode
 * @param parent_inode_reference The inode reference to the parent inode
 */
void oufs_init_directory_structures(INODE *inode, BLOCK *block,
                                    BLOCK_REFERENCE self_block_reference,
                                    INODE_REFERENCE self_inode_reference,
                                    INODE_REFERENCE parent_inode_reference)
{
    // set up Inode
    inode->type = DIRECTORY_TYPE;
    inode->n_references = 1;
    inode->size = 2;
    inode->content = self_block_reference;
    
    // Initialize directory block
    block->next_block = UNALLOCATED_BLOCK;
    // set up '.'
    strcpy(block->content.directory.entry[0].name, ".");
    block->content.directory.entry[0].inode_reference= self_inode_reference;
    // set up ".."
    strcpy(block->content.directory.entry[1].name, "..");
    block->content.directory.entry[1].inode_reference= parent_inode_reference;
    
    // set all other entries to UNALLOCATED_INODE
    for (int i=2; i<N_DIRECTORY_ENTRIES_PER_BLOCK; i++)
    {
        block->content.directory.entry[i].inode_reference = UNALLOCATED_INODE;
    }
    
}


/**
 *  Given an inode reference, read the inode from the virtual disk.
 *
 *  @param i Inode reference (index into the inode list)
 *  @param inode Pointer to an inode memory structure.  This structure will be
 *                filled in before return)
 *  @return 0 = successfully loaded the inode
 *         -1 = an error has occurred
 *
 */
int oufs_read_inode_by_reference(INODE_REFERENCE i, INODE *inode)
{
    if(debug)
        fprintf(stderr, "\tDEBUG: Fetching inode %d\n", i);
    
    // Find the address of the inode block and the inode within the block
    BLOCK_REFERENCE block = i / N_INODES_PER_BLOCK + 1;
    int element = (i % N_INODES_PER_BLOCK);
    
    // Load the block that contains the inode
    BLOCK b;
    if(virtual_disk_read_block(block, &b) == 0) {
        // Successfully loaded the block: copy just this inode
        *inode = b.content.inodes.inode[element];
        return(0);
    }
    // Error case
    return(-1);
}


/**
 * Write a single inode to the disk
 *
 * @param i Inode reference index
 * @param inode Pointer to an inode structure
 * @return 0 if success
 *         -x if error
 *
 */
int oufs_write_inode_by_reference(INODE_REFERENCE i, INODE *inode)
{
    if(debug)
        fprintf(stderr, "\tDEBUG: Writing inode %d\n", i);
    
    // TODO:
    BLOCK_REFERENCE b = i / N_INODES_PER_BLOCK + 1;     // add one to account for master block I think
    int element = (i % N_INODES_PER_BLOCK);
    
    BLOCK tempBlock;
    memset(&tempBlock, 0, BLOCK_SIZE);
    // read the block from disk to tempBlock
    if(virtual_disk_read_block(b, &tempBlock) != 0) {
        fprintf(stderr, "deallocate_block: error reading inode block\n");
        return(-1);
    }
    // set tempBlock's inode to the input inode
    tempBlock.content.inodes.inode[element] = *inode;
    
    // Write the block back
    if(virtual_disk_write_block(b, &tempBlock) != 0) {
        fprintf(stderr, "deallocate_block: error writing inode block\n");
        return(-1);
    }
    
    // Success
    return(0);
}

/**
 * Set all of the properties of an inode
 *
 * @param inode Pointer to the inode structure to be initialized
 * @param type Type of inode
 * @param n_references Number of references to this inode
 *          (when first created, will always be 1)
 * @param content Block reference to the block that contains the information within this inode
 * @param size Size of the inode (# of directory entries or size of file in bytes)
 *
 */

void oufs_set_inode(INODE *inode, INODE_TYPE type, int n_references,
                    BLOCK_REFERENCE content, int size)
{
    inode->type = type;
    inode->n_references = n_references;
    inode->content = content;
    inode->size = size;
}


/*
 * Given a valid directory inode, return the inode reference for the sub-item
 * that matches <element_name>
 *
 * @param inode Pointer to a loaded inode structure.  Must be a directory inode
 * @param element_name Name of the directory element to look up
 *
 * @return = INODE_REFERENCE for the sub-item if found; UNALLOCATED_INODE if not found
 */

int oufs_find_directory_element(INODE *inode, char *element_name)
{
    if(debug)
        fprintf(stderr,"\tDEBUG: oufs_find_directory_element: %s\n", element_name);
    
    // TODO
    // TODO: should I return -1 for its "must be directory inode" if not directory inode??
    if (inode->type == DIRECTORY_TYPE)
    {
        BLOCK b;
        memset(&b, 0, sizeof(BLOCK));
        virtual_disk_read_block(inode->content, &b);
        for (int i=0; i<N_DIRECTORY_ENTRIES_PER_BLOCK; i++)
        {
            if(strcmp(b.content.directory.entry[i].name, element_name) == 0)
            {
                return b.content.directory.entry[i].inode_reference;
            }
        }
        return UNALLOCATED_INODE;
    }
    // changed this from -1
    return UNALLOCATED_INODE;
}

/**
 *  Given a current working directory and either an absolute or relative path, find both the inode of the
 * file or directory and the inode of the parent directory.  If one or both are not found, then they are
 * set to UNALLOCATED_INODE.
 *
 *  This implementation handles a variety of strange cases, such as consecutive /'s and /'s at the end of
 * of the path (we have to maintain some extra state to make this work properly).
 *
 * @param cwd Absolute path for the current working directory
 * @param path Absolute or relative path of the file/directory to be found
 * @param parent Pointer to the found inode reference for the parent directory
 * @param child pointer to the found node reference for the file or directory specified by path
 * @param local_name String name of the file or directory without any path information
 *             (i.e., name relative to the parent)
 * @return 0 if no errors
 *         -1 if child not found
 *         -x if an error
 *
 */
int oufs_find_file(char *cwd, char * path, INODE_REFERENCE *parent, INODE_REFERENCE *child,
                   char *local_name)
{
    INODE_REFERENCE grandparent;
    char full_path[MAX_PATH_LENGTH];
    
    // Construct an absolute path for the file/directory in question
    if(path[0] == '/') {
        strncpy(full_path, path, MAX_PATH_LENGTH-1);
    }else{    // path is a relative path
        if(strlen(cwd) > 1) {
            strncpy(full_path, cwd, MAX_PATH_LENGTH-1);
            strncat(full_path, "/", 2);
            strncat(full_path, path, MAX_PATH_LENGTH-1-strnlen(full_path, MAX_PATH_LENGTH));
        }else{
            strncpy(full_path, "/", 2);
            strncat(full_path, path, MAX_PATH_LENGTH-2);
        }
    }
    
    if(debug) {
        fprintf(stderr, "\tDEBUG: Full path: %s\n", full_path);
    };
    
    // Start scanning from the root directory
    grandparent = *parent = *child = 0;
    if(debug)
        fprintf(stderr, "\tDEBUG: Start search: %d\n", *parent);
    
    // Parse the full path
    char *directory_name;
    directory_name = strtok(full_path, "/");
    while(directory_name != NULL) {
        if(strlen(directory_name) >= FILE_NAME_SIZE-1)
            // Truncate the name
            directory_name[FILE_NAME_SIZE - 1] = 0;
        if(debug){
            fprintf(stderr, "\tDEBUG: Searching Directory: %s\n", directory_name);
        }
        // TODO: finish
        
        INODE start;
        BLOCK b;
        memset(&b, 0, sizeof(BLOCK));
        oufs_read_inode_by_reference(*child, &start);
        INODE_REFERENCE temp = (INODE_REFERENCE)oufs_find_directory_element(&start, directory_name);
        if ((int)temp == -1)
        {
            // inode is (possibly) a file
            fprintf(stderr, "\tThis is a file not a directory\n");
        }
        else if (temp != UNALLOCATED_INODE)
        {
            fprintf(stderr, "\tFound directory\n");
            //found subdirectory
            grandparent = *parent;
            //fprintf(stderr, "\tParent inode was %d next round will be %d\n", *parent, temp);
            
            if(local_name!=NULL)
                strcpy(local_name, directory_name);
            directory_name = strtok(NULL, "/");
            if(directory_name!=NULL)
                *parent = temp;
        }
        else
        {
            fprintf(stderr,"\tDirectory not found, setting local_name\n");
            //unallocated inode
            *parent= *child;
            *child=UNALLOCATED_INODE;
            if(local_name!=NULL)
                strcpy(local_name, directory_name);
            return (-1);
        }
        
        *child = temp;
    };
    
    // Item found.
    
    if(debug) {
        fprintf(stderr, "\tDEBUG: Found: parent %d, child %d\n", *parent, *child);
    }
    // Success!
    return(0);
}


/**
 * Return the bit index for the first 0 bit in a byte (starting from 7th bit
 *   and scanning to the right)
 *
 * @param value: a byte
 * @return The bit number of the first 0 in value (starting from the 7th bit
 *         -1 if no zero bit is found
 */

int oufs_find_open_bit(unsigned char value)
{
    // TODO
    
    
    //fprintf(stderr, "inside oufs_find_open_bit 357");
    // handle no bits available
    //fprintf(stderr, "value is %x\n", value);
    if ((value ^ 0xFF) == 0x00)
    {
        return -1;
    }
    // handle all bits available
    else
    {
        for (int i=7; i>0; i--)
        {
            //fprintf(stderr, "\nvalue & (1<<i) is %x \n", (value & (1<<i)));
            if((value & (1<<i))==0)
            {
                return i;
            }
        }
    }
    // Not found
    return(-1);
}

/**
 *  Allocate a new directory (an inode and block to contain the directory).  This
 *  includes initialization of the new directory.
 *
 * @param parent_reference The inode of the parent directory
 * @return The inode reference of the new directory
 *         UNALLOCATED_INODE if we cannot allocate the directory
 */
int oufs_allocate_new_directory(INODE_REFERENCE parent_reference)
{
    BLOCK block;
    BLOCK block2;
    // Read the master block
    if(virtual_disk_read_block(MASTER_BLOCK_REFERENCE, &block) != 0) {
        // Read error
        return(UNALLOCATED_INODE);
    }
    // TODO
    INODE_REFERENCE newdir = UNALLOCATED_INODE;
    int byte = -1;
    int bit = -1;
    
    //FIXME: this looping is wrong. uses already used inodes
    for (int i=0; i<N_INODES; i++)
    {
        // TODO: double check all this
        byte = i/8;
        // TODO: must make sure find_open_bit works for this function to work
        //fprintf(stderr, "before find_open_bit in allocate_new_dir line 403");
        bit = oufs_find_open_bit(block.content.master.inode_allocated_flag[byte]);
        //fprintf(stderr, "\n bit is %d \n", bit);
        if (bit != -1)
        {
            // INODE REFERENCE may be j*8 + (7-i)
            newdir = (INODE_REFERENCE)((7-bit) +(i));
            //fprintf(stderr, "\n i is %d \n", i);
            //fprintf(stderr, "\n newdir is %d \n", newdir);
            // TODO: make sure this shift works correctly
            
            block.content.master.inode_allocated_flag[byte] = (block.content.master.inode_allocated_flag[byte] | (1<<bit) );
            break;
        }
    }
    // couldn't find an open bit
    if (bit == -1)
        return UNALLOCATED_INODE;
    // change allocation table to mark new one being allocated
    
    INODE inode;
    // read the inode from virtual disk TODO: need this??
    oufs_read_inode_by_reference(newdir, &inode);
    
    // using a temporary variable for the block at front of list
    BLOCK_REFERENCE temp = block.content.master.unallocated_front;
    if (temp == UNALLOCATED_BLOCK)
    {
        fprintf(stderr, "\n unallocated_front was an unallocated block. \n");
        return UNALLOCATED_INODE;
    }
    virtual_disk_read_block(temp, &block2);
    
    //TODO: set the end of the chain to UNALLOCATED IF NONE LEFT
    if (block2.next_block == UNALLOCATED_BLOCK)
    {
        block.content.master.unallocated_front = UNALLOCATED_BLOCK;
    }
    else        // TODO: double check this logic
    {
        block.content.master.unallocated_front = block2.next_block;
    }
    // TODO: double check this call that all parameters are correct
    fprintf(stderr, "\n about to init directory structures \n");
    oufs_init_directory_structures(&inode, &block2, temp, newdir, parent_reference);
    // write inode and block to virtual disk
    oufs_write_inode_by_reference(newdir, &inode);
    //  changed allocation table. write master block back to disk
    virtual_disk_write_block(MASTER_BLOCK_REFERENCE, &block);
    virtual_disk_write_block(temp, &block2); // TODO: changed to temp from inode.content. Check this
    return newdir;
    
};


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
    BLOCK dirblock;
    virtual_disk_read_block(inode.content, &dirblock);
    BLOCK masterblock;
    virtual_disk_read_block(MASTER_BLOCK_REFERENCE, &masterblock);
    // Get open bit in master block
    INODE_REFERENCE fileref = UNALLOCATED_INODE;
    int byte = -1;
    int bit = -1;
    for (int i=0; i<N_INODES; i++)
    {
        // TODO: double check all this
        byte = i/8;
        bit = oufs_find_open_bit(masterblock.content.master.inode_allocated_flag[byte]);
        if (bit != -1)
        {
            // open bit found, set bit to 1 in inode allocation table
            fileref = (INODE_REFERENCE)((7-bit) +(i));
            masterblock.content.master.inode_allocated_flag[byte] = (masterblock.content.master.inode_allocated_flag[byte] | (1<<bit) );
            break;
        }
    }
    // error if no open bit was found above. return UNALLOCATED_INODE
    if (fileref == UNALLOCATED_INODE)
        return fileref;
    INODE newFile;
    oufs_read_inode_by_reference(fileref, &newFile);
    
    for (int i=2; i<N_DIRECTORY_ENTRIES_PER_BLOCK; i++)
    {
        if (dirblock.content.directory.entry[i].inode_reference == UNALLOCATED_INODE)
        {
            oufs_set_inode(&newFile, FILE_TYPE, 1, UNALLOCATED_BLOCK, 0);
            dirblock.content.directory.entry[i].inode_reference =  fileref;
            // copy local name to parent directory block entry
            strcpy(dirblock.content.directory.entry[i].name, local_name);
            // write parent directory block and inode back to disk
            inode.size++;
            break;
        }
    }
    // TODO: not sure if i need to set up OUFILE or what mode would be seems like it would be lost without memset() call
    
    // Write back to disk for all
    oufs_write_inode_by_reference(parent, &inode);
    virtual_disk_write_block(MASTER_BLOCK_REFERENCE, &masterblock);
    virtual_disk_write_block(inode.content, &dirblock);
    oufs_write_inode_by_reference(fileref, &newFile);
    

  // Success
    return (fileref);
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
    BLOCK_REFERENCE br;
    BLOCK_REFERENCE next;
    br = inode->content;
    //virtual_disk_read_block(br, &block);
    while (br != UNALLOCATED_BLOCK)
    {
        // read master block from disk to block mb
        virtual_disk_read_block(MASTER_BLOCK_REFERENCE, &master_block);
        
        // read the block in the chain of blocks in the file
        virtual_disk_read_block(br, &block);
        
        // get next block in chain before deallocating
        next = block.next_block;
        
        // deallocate in memory master block and DISK copy of br
        if (oufs_deallocate_block(&master_block, br) <0 )
        {
            fprintf(stderr, "error while deallocating and individual block in chain\n");
            return -2;
        }
        
        // write master block to disk
        virtual_disk_write_block(MASTER_BLOCK_REFERENCE, &master_block);
        
        // change block reference to next one
        br = next;
    }
    inode->content = UNALLOCATED_BLOCK;

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
    BLOCK_REFERENCE front;
    front = master_block->content.master.unallocated_front;
    fprintf(stderr, "inside allocate_new_block: master.unallocated front before shift = %d\n", front);
    //TODO: check to see if I need like a memset or memcpy to clear new_block before setting it
    virtual_disk_read_block(front, new_block);
    //fprintf(stderr, "inside allocate_new_b pointer &newblock is %d\n", new_block);
    /////
    //fprintf(stderr, "inside allocate_new_b new_block->next_block == %d\n", new_block->next_block);
    master_block->content.master.unallocated_front = new_block->next_block;
    fprintf(stderr, "inside allocate_new_block: master.unallocated front AFTER shift = %d\n", new_block->next_block);
    //fprintf(stderr, "inside allocate_new_b new_block->next_block == %d\n", new_block->next_block);
    new_block->next_block = UNALLOCATED_BLOCK;
    //fprintf(stderr, "made it to the end of allocate_new_block");
    

    // change this master block reference
  return front;
}

