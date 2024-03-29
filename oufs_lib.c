/**
 *  Project 3
 *  oufs_lib.c
 *
 *  Author: CS3113
 *
 */

#include "oufs_lib.h"
#include "oufs_lib_support.h"
#include "virtual_disk.h"

// Yes ... a global variable
int debug = 1;

// Translate inode types to descriptive strings
const char *INODE_TYPE_NAME[] = {"UNUSED", "DIRECTORY", "FILE"};

/**
 Read the OUFS_PWD, OUFS_DISK, OUFS_PIPE_NAME_BASE environment
 variables copy their values into cwd, disk_name an pipe_name_base.  If these
 environment variables are not set, then reasonable defaults are
 given.
 @param cwd String buffer in which to place the OUFS current working directory.
 @param disk_name String buffer in which to place file name of the virtual disk.
 @param pipe_name_base String buffer in which to place the base name of the
 named pipes for communication to the server.
 PROVIDED
 */
void oufs_get_environment(char *cwd, char *disk_name,
                          char *pipe_name_base)
{
    // Current working directory for the OUFS
    char *str = getenv("OUFS_PWD");
    if(str == NULL) {
        // Provide default
        strcpy(cwd, "/");
    }else{
        // Exists
        strncpy(cwd, str, MAX_PATH_LENGTH-1);
    }
    
    // Virtual disk location
    str = getenv("OUFS_DISK");
    if(str == NULL) {
        // Default
        strcpy(disk_name, "vdisk1");
    }else{
        // Exists: copy
        strncpy(disk_name, str, MAX_PATH_LENGTH-1);
    }
    
    // Pipe name base
    str = getenv("OUFS_PIPE_NAME_BASE");
    if(str == NULL) {
        // Default
        strcpy(pipe_name_base, "pipe");
    }else{
        // Exists: copy
        strncpy(pipe_name_base, str, MAX_PATH_LENGTH-1);
    }
    
}

/**
 * Completely format the virtual disk (including creation of the space).
 *
 * NOTE: this function attaches to the virtual disk at the beginning and
 *  detaches after the format is complete.
 *
 * - Zero out all blocks on the disk.
 * - Initialize the master block: mark inode 0 as allocated and initialize
 *    the linked list of free blocks
 * - Initialize root directory inode
 * - Initialize the root directory in block ROOT_DIRECTORY_BLOCK
 *
 * @return 0 if no errors
 *         -x if an error has occurred.
 *
 */

int oufs_format_disk(char  *virtual_disk_name, char *pipe_name_base)
{
    // Attach to the virtual disk
    if(virtual_disk_attach(virtual_disk_name, pipe_name_base) != 0) {
        return(-1);
    }
    
    BLOCK block;
    
    // Zero out the block
    memset(&block, 0, BLOCK_SIZE);
    for(BLOCK_REFERENCE i = 0; i < N_BLOCKS; ++i) {
        if(virtual_disk_write_block(i, &block) < 0) {
            return(-2);
        }
    }
    
    //////////////////////////////
    // Master block
    block.next_block = UNALLOCATED_BLOCK;
    block.content.master.inode_allocated_flag[0] = 0x80;
    // configure front and end references
    block.content.master.unallocated_front = N_INODE_BLOCKS+2; // this will be block #6
    block.content.master.unallocated_end = N_BLOCKS-1;    // will be block # 127
    // write master block to virtual disk
    if (virtual_disk_write_block(0, &block)<0)
    {
        return -2;
    }
    
    //  clear block again
    memset(&block, 0, BLOCK_SIZE);
    
    //////////////////////////////
    // Root directory inode / block
    INODE inode;
    // ROOT_DIRECTORY_BLOCK is block #5
    oufs_init_directory_structures(&inode, &block, ROOT_DIRECTORY_BLOCK,
                                   ROOT_DIRECTORY_INODE, ROOT_DIRECTORY_INODE);
    
    // Write the results to the disk
    if(oufs_write_inode_by_reference(0, &inode) != 0)
    {
        return(-3);
    }
    
    // write the directory block to disk
    if (virtual_disk_write_block(ROOT_DIRECTORY_BLOCK, &block)<0)
    {
        return -2;
    }
    
    //////////////////////////////
    // All other blocks are free blocks
    // for loop to initialize linked list of free blocks
    for (BLOCK_REFERENCE i=6; i<N_BLOCKS; i++)
    {
        memset(&block, 0, BLOCK_SIZE);
        if (i == 127)
        {
            block.next_block = UNALLOCATED_BLOCK;
        }
        else
            block.next_block = i+1;
        // Write each block to the virtual disk
        if (virtual_disk_write_block(i, &block)<0)
        {
            return -2;
        }
    }
    // Done
    virtual_disk_detach();
    
    return(0);
}

/*
 * Compare two inodes for sorting, handling the
 *  cases where the inodes are not valid
 *
 * @param e1 Pointer to a directory entry
 * @param e2 Pointer to a directory entry
 * @return -1 if e1 comes before e2 (or if e1 is the only valid one)
 * @return  0 if equal (or if both are invalid)
 * @return  1 if e1 comes after e2 (or if e2 is the only valid one)
 *
 * Note: this function is useful for qsort()
 */
static int inode_compare_to(const void *d1, const void *d2)
{
    // Type casting from generic to DIRECTORY_ENTRY*
    DIRECTORY_ENTRY* e1 = (DIRECTORY_ENTRY*) d1;
    DIRECTORY_ENTRY* e2 = (DIRECTORY_ENTRY*) d2;

    // if e1 comes before e2 or if e1 is only valid one
    int e1valid = 1;
    int e2valid = 1;
    if (e1->inode_reference == UNALLOCATED_INODE)
        e1valid = 0;
    if (e2->inode_reference == UNALLOCATED_INODE)
        e2valid = 0;
    
    if (e1valid)
    {
        if (e2valid)
        {
            // both valid. compare e1 and e2
            if (strcmp(e1->name, e2->name) < 0)
            {
                return -1;
            }
            else if (strcmp(e1->name, e2->name) > 0)
            {
                return 1;
            }
            else return 0;
        }
        else    // e1 is valid, e2 is invalid
        {
            return -1;
        }
    }
    else    // e1 is invalid
    {
        if (e2valid)    // e1 invalid, e2 valid
        {
            return 1;
        }
        else        // both invalid
        {
            return 0;
        }
    }
}


/**
 * Print out the specified file (if it exists) or the contents of the
 *   specified directory (if it exists)
 *
 * If a directory is listed, then the valid contents are printed in sorted order
 *   (as defined by strcmp()), one per line.  We know that a directory entry is
 *   valid if the inode_reference is not UNALLOCATED_INODE.
 *   Hint: qsort() will do the sort for you.  You just have to provide a compareTo()
 *   function (just like in Java!)
 *   Note: if an entry is a directory itself, then its name must be followed by "/"
 *
 * @param cwd Absolute path representing the current working directory
 * @param path Absolute or relative path to the file/directory
 * @return 0 if success
 *         -x if error
 *
 */

int oufs_list(char *cwd, char *path)
{
    INODE_REFERENCE parent;
    INODE_REFERENCE child;
    
    // Look up the inodes for the parent and child
    int ret = oufs_find_file(cwd, path, &parent, &child, NULL);
    
    // Did we find the specified file?
    if(ret == 0 && child != UNALLOCATED_INODE) {
        // Element found: read the inode
        INODE inode;
        if(oufs_read_inode_by_reference(child, &inode) != 0) {
            return(-1);
        }
        if(debug)
            fprintf(stderr, "\tDEBUG: Child found (type=%s).\n",  INODE_TYPE_NAME[inode.type]);
        
        BLOCK b;
        memset(&b, 0, sizeof(BLOCK));
        virtual_disk_read_block(inode.content, &b);
        // Have the child inode
        // check if it is a directory or a file inode
        if (inode.type == DIRECTORY_TYPE)
        {
            // Don't need pointer below?? According to TA
            qsort(b.content.directory.entry, N_DIRECTORY_ENTRIES_PER_BLOCK, sizeof(b.content.directory.entry[0]), inode_compare_to);
            for (int i = 0; i < N_DIRECTORY_ENTRIES_PER_BLOCK; i++)
            {
                // May be holes in the directory. So must check whole list. Not just inode.size many
                if (b.content.directory.entry[i].inode_reference != UNALLOCATED_INODE)
                {
                    // check if the entry is a directory and if so, add a '/' to the end
                    oufs_read_inode_by_reference(b.content.directory.entry[i].inode_reference, &inode);
                    if (inode.type == DIRECTORY_TYPE)
                    {
                        // add a slash to the directory name
                        printf("%s/\n", b.content.directory.entry[i].name);
                    }
                    else
                    {
                        printf("%s\n", b.content.directory.entry[i].name);
                    }
                    
                }
            }
        }
        else if (inode.type == FILE_TYPE)
        {
            for (int n = 0; n<DATA_BLOCK_SIZE; n++)
            {
                // data[n] is unsigned char. So i think a printf with %c should do it
                printf("%c\n", b.content.data.data[n]);
            }
        }
        else
            return (-2);
    }
    else
    {
        // Did not find the specified file/directory
        fprintf(stderr, "Not found\n");
        if(debug)
            fprintf(stderr, "\tDEBUG: (%d)\n", ret);
    }
    // Done: return the status from the search
    return(ret);
}




///////////////////////////////////
/**
 * Make a new directory
 *
 * To be successful:
 *  - the parent must exist and be a directory
 *  - the parent must have space for the new directory
 *  - the child must not exist
 *
 * @param cwd Absolute path representing the current working directory
 * @param path Absolute or relative path to the file/directory
 * @return 0 if success
 *         -x if error
 *
 */
int oufs_mkdir(char *cwd, char *path)
{
    INODE_REFERENCE parent;
    INODE_REFERENCE child;
    
    // Name of a directory within another directory
    char local_name[MAX_PATH_LENGTH];
    int ret;
    
    // Attempt to find the specified directory
    if((ret = oufs_find_file(cwd, path, &parent, &child, local_name)) < -1) {
        if(debug)
            fprintf(stderr, "oufs_mkdir(): ret = %d\n", ret);
        return(-1);
    };
    
    fprintf(stderr, "\nlocal_name is  = %s\n", local_name);
    // parent inode and block
    INODE parentinode;
    oufs_read_inode_by_reference(parent, &parentinode);
    
    BLOCK pblock;
    virtual_disk_read_block(parentinode.content, &pblock);
    
    
    // add to parent directory and increment size
    
    // add it to first AVAILABLE ENTRY
    for (int i=2; i<N_DIRECTORY_ENTRIES_PER_BLOCK; i++)
    {
        if (pblock.content.directory.entry[i].inode_reference == UNALLOCATED_INODE)
        {
            fprintf(stderr, "allocating directory on inode: %d\n", parent);
            child = oufs_allocate_new_directory(parent);
            if (child == UNALLOCATED_INODE)
            {
                fprintf(stderr, "oufs_mkdir(): got UNALLOCATED_INODE calling allocate_new_dir");
                return (-3);
            }
            pblock.content.directory.entry[i].inode_reference =  child;// insert new inode ref
            // FIXME: this name is not copying correctly
            
            strcpy(pblock.content.directory.entry[i].name, local_name);
            parentinode.size++;
            // write parent directory block and inode back to disk
            virtual_disk_write_block(parentinode.content, &pblock);
            oufs_write_inode_by_reference(parent, &parentinode);
            return 0;
        }
    }
    // no space to store directory if it hits this point
    fprintf(stderr, "No space in directory to store new entry");
    return (-2);
}

/**
 * Remove a directory
 *
 * To be successul:
 *  - The directory must exist and must be empty
 *  - The directory must not be . or ..
 *  - The directory must not be /
 *
 * @param cwd Absolute path representing the current working directory
 * @param path Abslute or relative path to the file/directory
 * @return 0 if success
 *         -x if error
 *
 */
int oufs_rmdir(char *cwd, char *path)
{
    INODE_REFERENCE parent;
    INODE_REFERENCE child;
    char local_name[MAX_PATH_LENGTH];
    
    // Try to find the inode of the child
    int result = oufs_find_file(cwd, path, &parent, &child, local_name);
    if(result < -1) {
        return(-4);
    }
    else if (result==-1)    // could not find child
    {
        return -1;
    }
    // TODO: Will be error for: name does not exist, if its not a directory, if name is . or .., and if not an empty directory
    
    INODE pnode;
    oufs_read_inode_by_reference(parent, &pnode);
    // error if type isn't directory type
    INODE cnode;
    oufs_read_inode_by_reference(child, &cnode);
    if (cnode.type != DIRECTORY_TYPE)
    {
        return -2;
    }
    
    BLOCK childdirectory;
    virtual_disk_read_block(cnode.content, &childdirectory);
    int count = 0;
    for (int i=0; i<N_DIRECTORY_ENTRIES_PER_BLOCK; i++)
    {
        if (childdirectory.content.directory.entry[i].inode_reference != UNALLOCATED_INODE)
        {
            count++;
        }
    }
    if (count > 2)
    {
        fprintf(stderr, "trying to remove non-empty directory\n");
        return -3;
    }
    // check to make sure name is not . or ..
    if (strcmp(local_name, ".") == 0)
        return -2;
    if (strcmp(local_name, "..") == 0)
        return -2;
    
    BLOCK directory;
    virtual_disk_read_block(pnode.content, &directory);

    for (int i=0; i<N_DIRECTORY_ENTRIES_PER_BLOCK; i++)
    {
        fprintf(stderr, "checking directory entry %d:%s\n", i, directory.content.directory.entry[i].name);
        if (strcmp(directory.content.directory.entry[i].name, local_name) == 0)
        {
            directory.content.directory.entry[i].inode_reference = UNALLOCATED_INODE;
            pnode.size--;
            break;
        }
    }
    BLOCK master;
    virtual_disk_read_block(MASTER_BLOCK_REFERENCE, &master);
    // change bit in master block's inode allocation table
    int byte = child / 8;
    int bit = 7 - (child % 8);
    fprintf(stderr, "byte %d bit %d\n", byte, bit);
    master.content.master.inode_allocated_flag[byte] = master.content.master.inode_allocated_flag[byte] ^ (1<<bit);
    
    
    //if(cnode.size==2)
    //    cnode.type= UNUSED_TYPE;
    
    //write blocks back to disk
    
    oufs_write_inode_by_reference(child, &cnode);
    virtual_disk_write_block(cnode.content, &childdirectory);
    oufs_deallocate_block(&master, cnode.content);
    virtual_disk_write_block(MASTER_BLOCK_REFERENCE, &master);
    virtual_disk_write_block(pnode.content, &directory);
    oufs_write_inode_by_reference(parent, &pnode);
    
    
    
    // Success
    return(0);
}


/*********************************************************************/
// Project 4
/**
 * Open a file
 * - mode = "r": the file must exist; offset is set to 0
 * - mode = "w": the file may or may not exist;
 *                 - if it does not exist, it is created 
 *                 - if it does exist, then the file is truncated
 *                       (size=0 and data blocks deallocated);
 *                 offset = 0 and size = 0
 * - mode = "a": the file may or may not exist
 *                 - if it does not exist, it is created 
 *                 offset = size
 *
 * @param cwd Absolute path for the current working directory
 * @param path Relative or absolute path for the file in question
 * @param mode String: one of "r", "w" or "a"
 *                 (note: only the first character matters here)
 * @return Pointer to a new OUFILE structure if success
 *         NULL if error
 */
OUFILE* oufs_fopen(char *cwd, char *path, char *mode)
{
  INODE_REFERENCE parent;
  INODE_REFERENCE child;
  char local_name[MAX_PATH_LENGTH];
  INODE inode;
  int ret;

  // Check for valid mode
  if(mode[0] != 'r' && mode[0] != 'w' && mode[0] != 'a') {
    fprintf(stderr, "fopen(): bad mode.\n");
    return(NULL);
  };

  // Try to find the inode of the child
  if((ret = oufs_find_file(cwd, path, &parent, &child, local_name)) < -1) {
    if(debug)
      fprintf(stderr, "oufs_fopen(%d)\n", ret);
    return(NULL);
  }
  
  if(parent == UNALLOCATED_INODE) {
    fprintf(stderr, "Parent directory not found.\n");
    return(NULL);
  }

  // TODO
    OUFILE *file = malloc(sizeof(OUFILE));
    
    ////// READ ///////
    if (mode[0] == 'r')
    {
        // Child does not exist - ERROR
        if (child == UNALLOCATED_INODE)
        {
            fprintf(stderr, "oufs_fopen() Child not found for mode 'r'\n");
            return (NULL);
        }
        oufs_read_inode_by_reference(child, &inode);
        // Walk down the linked list of blocks (while loop, read content of inode - assuming it exists) increment counter. Put those block references in the cache array
        if (inode.type == DIRECTORY_TYPE)
        {
            return NULL;
        }
        int count = 0;
        BLOCK_REFERENCE b;
        b = inode.content;
        BLOCK *c = malloc(BLOCK_SIZE);
        virtual_disk_read_block(b, c);
        while (b != UNALLOCATED_BLOCK)
        {
            file->block_reference_cache[count] = b;
            count++;
            virtual_disk_read_block(b, c);
            b = c->next_block;
        }
        file->offset = 0;
        file->n_data_blocks = count;
        file->inode_reference = child;
        file->mode = 'r';
        //fprintf(stderr, "inside fopen for read: n_data_blocks = %d\n", file->n_data_blocks);

        //////
    }
    //// WRITE /////////
    else if (mode[0]=='w')
    {
        // File does not exist. Create file.
        if (child == UNALLOCATED_INODE)
        {
            child = oufs_create_file(parent, local_name);
            fprintf(stderr, "INSIDE FOPEN 'w': child is is %d\n", child);
            if (child == UNALLOCATED_INODE)
            {
                return (NULL);
            }
            oufs_read_inode_by_reference(child, &inode);
            //file->inode_reference = child;
            // inode is now the inode for new file
            
        }
        else // File exists. Truncate (steps to that)
        {
            oufs_read_inode_by_reference(child, &inode);
            // deallocate blocks of file
            if (inode.type == DIRECTORY_TYPE)
            {
                return NULL;
            }
            if (oufs_deallocate_blocks(&inode) < 0)
                return (NULL);
            
        }
        // below things apply to 'w' for both prexisting and non preexisting files
        file->n_data_blocks = 0;
        file->inode_reference = child;
        file->offset = 0;
        inode.size = 0;   // should be done in create_file I think
        file->mode = 'w';
        // write back to disk for setting size (create file should've done it already for no prev file)
        oufs_write_inode_by_reference(child, &inode);
        
    }
    ///// APPEND ////////
    else    // mode is 'a'  APPEND
    {
        //fprintf(stderr, "INSIDE FOPEN 'A': local_name is %s\n", local_name);
        // if file doesn't exist
        if (child == UNALLOCATED_INODE)
        {
            // File does not exist. Create it.
            child = oufs_create_file(parent, local_name);
            if (child == UNALLOCATED_INODE)
            {
                fprintf(stderr, "child == UNALLOCATED_INODE after create_file() \n");
                return (NULL);
            }

            oufs_read_inode_by_reference(child, &inode);
            file->offset = 0;
            file->n_data_blocks = 0;
            
        }
        else        // File does exist
        {
            
            oufs_read_inode_by_reference(child, &inode);
            file->offset = inode.size;
            // Walk down the linked list of blocks (while loop, read content of inode - assuming it exists) increment counter. Put those block references in the cache array
            int count = 0;
            BLOCK_REFERENCE b;
            b = inode.content;
            BLOCK *c = malloc(BLOCK_SIZE);
            
            while (b != UNALLOCATED_BLOCK)
            {
                file->block_reference_cache[count] = b;
                count++;
                virtual_disk_read_block(b, c);
                b = c->next_block;
            }
            file->n_data_blocks = count;
        }
        // for both conditions (pre-existing or non file)
        file->inode_reference = child;
        file->mode = 'a';
        
    }
    //fprintf(stderr, "End of fopen(): returning file. file->mode == %c\n", file->mode);
  return (file);
};

/**
 *  Close a file
 *   Deallocates the OUFILE structure
 *
 * @param fp Pointer to the OUFILE structure
 */
     
void oufs_fclose(OUFILE *fp) {
  fp->inode_reference = UNALLOCATED_INODE;
  free(fp);
}



/*
 * Write bytes to an open file.
 * - Allocate new data blocks, as necessary
 * - Can allocate up to MAX_BLOCKS_IN_FILE, at which point, no more bytes may be written
 * - file offset will always match file size; both will be updated as bytes are written
 *
 * @param fp OUFILE pointer (must be opened for w or a)
 * @param buf Character buffer of bytes to write
 * @param len Number of bytes to write
 * @return The number of written bytes
 *          0 if file is full and no more bytes can be written
 *         -x if an error
 * 
 */
int oufs_fwrite(OUFILE *fp, unsigned char * buf, int len)
{
  if(fp->mode == 'r') {
    fprintf(stderr, "Can't write to read-only file");
    return(0);
  }
  if(debug)
    fprintf(stderr, "-------\noufs_fwrite(%d)\n", len);
    
  INODE inode;
  BLOCK block;
  if(oufs_read_inode_by_reference(fp->inode_reference, &inode) != 0) {
    return(-1);
  }

  // Compute the index for the last block in the file + the first free byte within the block
  
  int current_blocks = (fp->offset + DATA_BLOCK_SIZE - 1) / DATA_BLOCK_SIZE;
  int used_bytes_in_last_block = fp->offset % DATA_BLOCK_SIZE;
  int free_bytes_in_last_block = DATA_BLOCK_SIZE - used_bytes_in_last_block;
  int len_written = 0;

  // TODO
    // do this check whre the inode is loaded.
    
    //if(inode.size + len > (DATA_BLOCK_SIZE * MAX_BLOCKS_IN_FILE))
    //{
    //    return 0;
    //}
    
    // while we have more bytes to write, allocate new block, copy in some number of bytes ( either bytes left to copy or bytes left to get to end of block - whichever is less)
    
    int bytes_left_to_write = 0;
    BLOCK master;
    virtual_disk_read_block(MASTER_BLOCK_REFERENCE, &master);
    //fprintf(stderr, "top of fwrite. current_blocks ==   %d\n", current_blocks);
    //fprintf(stderr, "top of fwrite. free_bytes_in_last_block ==   %d\n", free_bytes_in_last_block);
    //fprintf(stderr, "fp->inode_reference ==   %d\n", fp->inode_reference);
    //fprintf(stderr, "initial fp->offset ==   %d\n", fp->offset);
    
     // TODO: ???? I believe I have to handle inode sizes of 0 seperately CHECK THIS
     if ((current_blocks == 0) && (len > 0))
     {
         BLOCK_REFERENCE startref;
         //BLOCK startblock;
         BLOCK *startblock = malloc(BLOCK_SIZE);
         startref = oufs_allocate_new_block(&master, startblock);
         //fprintf(stderr, "startref ==   %d\n", startref);
         inode.content = startref;
         virtual_disk_write_block(MASTER_BLOCK_REFERENCE, &master);
         //fprintf(stderr, "wrote master block\n");
         // TODO: do i need to write the block that i just alloated to disk?
         virtual_disk_write_block(startref, startblock);
         //fprintf(stderr, "wrote to disk(startref, &masterblock");
         // TODO: check if this free() should be here
         //free(startblock);
         fp->n_data_blocks = 1;
         fp->block_reference_cache[0] = startref;
         //fprintf(stderr, "end of first loop where current_blocks == 0. n_data_blocks =  %d\n", fp->n_data_blocks);
     }
    
    BLOCK_REFERENCE currBlock;
    BLOCK tempBlock;
    currBlock = inode.content;
    // currBlock is reference to initial block connected to inode
    // Loop to get the refrence of the last block in the chain of blocks connected to the file. Storing it in currBlock.
    while (1)
    {
        virtual_disk_read_block(currBlock, &tempBlock);
        if (tempBlock.next_block != UNALLOCATED_BLOCK)
        {
            currBlock = tempBlock.next_block;
        }
        else
            break;
    }
    if (currBlock == UNALLOCATED_BLOCK)
        return -2;
    virtual_disk_read_block(currBlock, &block);
    //fprintf(stderr, "before for loop. inode.content =  %d\n", inode.content);
    BLOCK_REFERENCE new;
    BLOCK * newBlock = malloc(BLOCK_SIZE);
    memset(newBlock, 0, BLOCK_SIZE);
    
    //fprintf(stderr, "FWRITE: before while loop, inode.size == %d\n", inode.size);
    while(len_written < len)
    {
    
    /*     TODO: check to see if need to add check towards end of this loop to see if allocating a
     *     newblock would put it over the 100 limit. If so, fild out how best to break. (tricky)
     */
        
        
        // check to make sure we haven't added the maximum number of blocks yet
        //if (fp->n_data_blocks > MAX_BLOCKS_IN_FILE)
        //{
        //    fprintf(stderr, "in fwrite loop: Hit 100 blocks in fp. breaking");
            // TODO: check if possibly return 0 here????
         //   break;
        //}
        bytes_left_to_write = len - len_written;
        
        //if (fp->n_data_blocks < )
        //fprintf(stderr, "Top of for loop byes_left_to_write ==   %d\n", bytes_left_to_write);
        
        // fewer bytes of space available in last block than need to be written
        if (free_bytes_in_last_block < bytes_left_to_write)
        {
                                        /*
            memcpy(block.content.data.data+used_bytes_in_last_block, buf, free_bytes_in_last_block);
            //memcpy(block.content.data.data[used_bytes_in_last_block], &buf[len_written], free_bytes_in_last_block);
            buf += free_bytes_in_last_block;
            len_written += free_bytes_in_last_block;
            fp->offset += free_bytes_in_last_block;
            inode.size += free_bytes_in_last_block;
            
            virtual_disk_write_block(currBlock, &block);
                                         */
            virtual_disk_read_block(MASTER_BLOCK_REFERENCE, &master);
            for (int i=used_bytes_in_last_block; i<DATA_BLOCK_SIZE; i++)
            {
                
                block.content.data.data[i] = buf[len_written];
                len_written++;
                fp->offset++;
                inode.size++;
                //fprintf(stderr, "fwrite: block.data[%d] = buf[%d] = %c\n", i, len_written, buf[len_written]);
                //debug print statement
                //fprintf(stderr, "len_written ==  %d\n", len_written);
            }
            virtual_disk_write_block(currBlock, &block);
            // check to see if number of blocks <= 100
            // allocate new block
            // TODO: check that this is assigning correctly. May be staying the same each time due to the function creating its own block_ref and assigning it to new with = operator
            
            // TODO: see if need to be using malloc for newBlock to be returned correctly
            new = oufs_allocate_new_block(&master, newBlock);
            
            //fprintf(stderr, "new == %d\n", new);
            // TODO: check that this shouldn't return 0 or something
            if (new == UNALLOCATED_BLOCK)
                return -2;
            // write master block back to disk - changed its unallocated_front
            virtual_disk_write_block(MASTER_BLOCK_REFERENCE, &master);
            //fprintf(stderr, "block.next_block pointed to %d BEFORE adding new one to end\n", block.next_block);
            block.next_block = new;
            //fprintf(stderr, "block.next_block now points to %d AFTER adding new one to end\n", block.next_block);
            //TODO: check if i need to use malloc for block or not
            virtual_disk_write_block(currBlock, &block);
            virtual_disk_write_block(new, newBlock);
            
            // recalculations for next loop:
            current_blocks = (fp->offset + DATA_BLOCK_SIZE - 1) / DATA_BLOCK_SIZE;
            used_bytes_in_last_block = fp->offset % DATA_BLOCK_SIZE;
            // free bytes should reset to 252
            free_bytes_in_last_block = DATA_BLOCK_SIZE - used_bytes_in_last_block;
            
            fp->n_data_blocks++;
            //TODO: check that this is right. Not subtracting 1 due to setting next one in chain to the new BLOCK_REF
            fp->block_reference_cache[current_blocks] = new;
            
            //TODO: currBLOCK is not changing correctly around here somewhere.
            // FIXME: fix this!
            //fprintf(stderr, "\tcurrBlock BEFORE shift to be new(reference): %d\n ", currBlock);
            //TODO: this is showing up as 6 everytime???
            currBlock = new;
            //fprintf(stderr, "\tcurrBlock AFTER shift to be new(reference): %d\n ", currBlock);
            //memcpy(&currBlock, &new, sizeof(BLOCK_REFERENCE));
            //TODO: check that this copy works
            //fprintf(stderr, "\tBefore memcpy to shift block to be newBlock: block.next_block is %d\n", block.next_block);
            memset(&block, 0, BLOCK_SIZE);
            memcpy(&block, newBlock, BLOCK_SIZE);
            // After memcpy
            //fprintf(stderr, "\tAfter memcpy to shift block to be newBlock: block.next_block is %d\n", block.next_block);
            //block = newBlock;
            memset(newBlock, 0, BLOCK_SIZE);
        }
        else    // whats left to write will fit in free space left in last block
        {
                                                    /*
            memcpy(block.content.data.data+used_bytes_in_last_block, buf, bytes_left_to_write);
            //memcpy(block.content.data.data[used_bytes_in_last_block], &buf[len_written], free_bytes_in_last_block);
            buf += bytes_left_to_write;
            len_written += bytes_left_to_write;
            fp->offset += bytes_left_to_write;
            inode.size += bytes_left_to_write;
                                                     */
            
            for (int i=used_bytes_in_last_block; i<(used_bytes_in_last_block + bytes_left_to_write); i++)
            {
                // TODO: check. changed below line from block.content.data.data[i] = buf[i-used_bytes_in_last_block];
                block.content.data.data[i] = buf[len_written];
                len_written++;
                fp->offset++;
                inode.size++;
                //fprintf(stderr, "fwrite: block.data[%d] = buf[%d] = %c\n", i, len_written, buf[len_written]);
                //fprintf(stderr, "FWRITE: block will hold data, inode.size == %d\n", inode.size);
                //fprintf(stderr, "FWRITE: Loops #%d bytes left to write = %d\n", i, bytes_left_to_write);
            }
            
            virtual_disk_write_block(currBlock, &block);
        }
    }
    if (debug)
    {
        //fprintf(stderr, "end of fwrite: fp->offset is %d\n", fp->offset);
        // number of data_blocks is 0 before an attempted write. one should be allocated first.
        //fprintf(stderr, "end of fwrite: fp->n_data_blocks is %d\n", fp->n_data_blocks);
        //fprintf(stderr, "end of fwrite: inode.size is %d\n", inode.size);
        //fprintf(stderr, "end of fwrite: len_written is %d\n", len_written);
    }
    //inode size has changed. write it to disk
    oufs_write_inode_by_reference(fp->inode_reference, &inode);
  // Done
  return(len_written);
}


/*
 * Read a sequence of bytes from an open file.
 * - offset is the current position within the file, and will never be larger than size
 * - offset will be updated with each read operation
 *
 * @param fp OUFILE pointer (must be opened for r)
 * @param buf Character buffer to place the bytes into
 * @param len Number of bytes to read at max
 * @return The number of bytes read
 *         0 if offset is at size
 *         -x if an error
 * 
 */

int oufs_fread(OUFILE *fp, unsigned char * buf, int len)
{
  // Check open mode
  if(fp->mode != 'r') {
    fprintf(stderr, "Can't read from a write-only file");
    return(0);
  }
  if(debug)
    fprintf(stderr, "\n-------\noufs_fread(%d)\n", len);
    
  INODE inode;
  BLOCK block;
  if(oufs_read_inode_by_reference(fp->inode_reference, &inode) != 0) {
    return(-1);
  }
      
  // Compute the current block and offset within the block
  int current_block = fp->offset / DATA_BLOCK_SIZE;
  int byte_offset_in_block = fp->offset % DATA_BLOCK_SIZE;
  int len_read = 0;
  int end_of_file = inode.size;
  len = MIN(len, end_of_file - fp->offset);
  int len_left = len;

  // TODO
    // TODO: check if this should be here or later, or both??
    
    //fprintf(stderr, "inside fread()\n");
    //fprintf(stderr, "inside fread(): fp->offset == %d\n", fp->offset);
    //fprintf(stderr, "inside fread(): inode.size == %d\n", inode.size);
    //fprintf(stderr, "inside fread(): current_block == %d\n", current_block);
    //fprintf(stderr, "inside fread(): fp->inode_reference is %d\n", fp->inode_reference);
    if (fp->offset == inode.size)
    {
        fprintf(stderr, "in fread(): At end of file\n");
        return 0;
    }
    // read len bytes from the file (len is min of len, other thing)
    
    // read first data block from inode
    BLOCK_REFERENCE currentRef = inode.content;
    //fprintf(stderr, "inside fread(): Initial Block_reference is %d\n", currentRef);
    if (currentRef == UNALLOCATED_BLOCK)
        return -2;
   
    // get correct block in chain of blocks
    
    if (current_block > 0)
    {
        // TODO: check this. adding 1 to current_block to make sure iterates correct num times
        for (int i=0; i<(current_block+1); i++)
        {
            virtual_disk_read_block(currentRef, &block);
            
            currentRef = block.next_block;
        }
    }
    else      // current_block == 0
    {
        virtual_disk_read_block(currentRef, &block);
        currentRef = block.next_block;
    }
                         
    // test this do-while loop.
                            /*
    int thisBlock = 0;
    do
    {
        virtual_disk_read_block(currentRef, &block);
        currentRef = block.next_block;
        thisBlock++;
    }
    while (thisBlock < current_block);
                             */
    
    // Cases: 1)block_size contains all the bytes needed to read len bytes. Or 2)len is bigger than block_size - byte_offset_in_block
    // now I have the correct data block in block
    //unsigned char * placeholder = NULL;
    //*placeholder = *buf;
    while (len_read < len)
    {
        // if whats left to write will fit inside whats left of the last block
        if (len_left <= (DATA_BLOCK_SIZE - byte_offset_in_block))
        {
            // read all remaining bytes in len_left
            int start = byte_offset_in_block;
            int finish = byte_offset_in_block + len_left;
            
            
            
                                /*
            memcpy(buf, block.content.data.data+byte_offset_in_block, len_left);
            buf += len_left;
            len_read += len_left;
            fp->offset += len_left;
            len_left -= len_left;
            
            fprintf(stderr, "in fread(): after memcpy, len_read is %d, len is %d\n", len_read, len);
                                 */
            
            for (int i=start; i<finish; i++)
            {
                //changed below from buf[len - len_left]
                // TODO: make sure can copy this over this way. Might have to do strcpy or something
                buf[len_read] = block.content.data.data[i];
                //fprintf(stderr, "inside fread: wrote to buf[%d] = %c\n", len_read, block.content.data.data[i]);
                len_left--;
                len_read++;
                fp->offset++;
            }
            
            
        }
        else        // Not enough space in block. We will have to grab the next block
        {
            
                                        /*
            
            memcpy(buf, block.content.data.data+byte_offset_in_block, (DATA_BLOCK_SIZE - byte_offset_in_block));
            buf += (DATA_BLOCK_SIZE - byte_offset_in_block);
            len_read += (DATA_BLOCK_SIZE - byte_offset_in_block);
            fp->offset += (DATA_BLOCK_SIZE - byte_offset_in_block);
            len_left -= (DATA_BLOCK_SIZE - byte_offset_in_block);
                                         */
            
            
            for (int i=byte_offset_in_block; i<DATA_BLOCK_SIZE; i++)
            {
                // this should start at 0 for buff and in correct place in block  for block
                buf[len_read] = block.content.data.data[i];
                len_left--;
                len_read++;
                fp->offset++;
                //fprintf(stderr, "byte offset in block = %d\n", byte_offset_in_block);
                //fprintf(stderr, "buf[%d] = block.data[%d] = %c\n", len_read, i, block.content.data.data[i]);
                //fprintf(stderr, "len_left is %d\n", len_left);
                //fprintf(stderr, "len_read is %d\n", len_read);
            }
            
            
            // might chek to make  sure currentRef isn't UNALLOCATED_BLOCK but shouldn't be if len and such numbers are correct
            // move to next block
            // currentRef is already pointing to block.next_block from earlier loop
            if (currentRef == UNALLOCATED_BLOCK)
            {
                // nore more blocks left to  grab
                fprintf(stderr, "no blocks remaining in file. returning len_read = %d\n", len_read);
                return (len_read);
            }
            virtual_disk_read_block(currentRef, &block);
            currentRef = block.next_block;
            //fprintf(stderr, "**** BLOCK %d ****\n", currentRef);
        }
        // recalculate things that I use for next loop
        byte_offset_in_block = fp->offset % DATA_BLOCK_SIZE;
    }
    
  // Done
    //fprintf(stderr, "Made it to end of fread(): returning %d\n", len_read);
  return(len_read);
}


/**
 * Remove a file
 *
 * Full implementation:
 * - Remove the directory entry
 * - Decrement inode.n_references
 * - If n_references == 0, then deallocate the contents and the inode
 *
 * @param cwd Absolute path for the current working directory
 * @param path Absolute or relative path of the file to be removed
 * @return 0 if success
 *         -x if error
 *
 */

int oufs_remove(char *cwd, char *path)
{
  INODE_REFERENCE parent;
  INODE_REFERENCE child;
  char local_name[MAX_PATH_LENGTH];
  INODE inode;
  INODE inode_parent;
  BLOCK block;

  // Try to find the inode of the child
  if(oufs_find_file(cwd, path, &parent, &child, local_name) < -1) {
    return(-3);
  };
  
  if(child == UNALLOCATED_INODE) {
    fprintf(stderr, "File not found\n");
    return(-1);
  }
  // Get the inode
  if(oufs_read_inode_by_reference(child, &inode) != 0) {
    return(-4);
  }

  // Is it a file?
  if(inode.type != FILE_TYPE) {
    // Not a file
    fprintf(stderr, "Not a file\n");
    return(-2);
  }

  // TODO
    // Read parent into inode_parent
    if(oufs_read_inode_by_reference(parent, &inode_parent) != 0) {
        return(-4);
    }
    // read parent directory to block
    virtual_disk_read_block(inode_parent.content, &block);
    
    for (int i=0; i<N_DIRECTORY_ENTRIES_PER_BLOCK; i++)
    {
        if (block.content.directory.entry[i].inode_reference == child)
        {
            strcpy(block.content.directory.entry[i].name, "");
            block.content.directory.entry[i].inode_reference = UNALLOCATED_INODE;
            inode.n_references--;
            inode_parent.size--;
            break;
        }
    }
    fprintf(stderr, "REMOVE: inode.n_references = %d\n", inode.n_references);
    if (inode.n_references == 0)
    {
        if (oufs_deallocate_blocks(&inode) < 0)
        {
            return -2;
        }
        BLOCK master;
        virtual_disk_read_block(MASTER_BLOCK_REFERENCE, &master);
        // deallocate inode in inode allocation table in master block
        int byte = child / 8;
        int bit = 7 - (child%8);
        master.content.master.inode_allocated_flag[byte] = (master.content.master.inode_allocated_flag[byte] ^ (1<<bit) );
        virtual_disk_write_block(MASTER_BLOCK_REFERENCE, &master);
        
    }
    virtual_disk_write_block(inode_parent.content, &block);
    oufs_write_inode_by_reference(parent, &inode_parent);
    oufs_write_inode_by_reference(child, &inode);
        
  // Success
  return(0);
};


/**
 * Create a hard link to a specified file
 *
 * Full implemenation:
 * - Add the new directory entry
 * - Increment inode.n_references
 *
 * @param cwd Absolute path for the current working directory
 * @param path_src Absolute or relative path of the existing file to be linked
 * @param path_dst Absolute or relative path of the new file inode to be linked
 * @return 0 if success
 *         -x if error
 * 
 */
int oufs_link(char *cwd, char *path_src, char *path_dst)
{
  INODE_REFERENCE parent_src;
  INODE_REFERENCE child_src;
  INODE_REFERENCE parent_dst;
  INODE_REFERENCE child_dst;
  char local_name[MAX_PATH_LENGTH];
  char local_name_bogus[MAX_PATH_LENGTH];
  INODE inode_src;
  INODE inode_dst;
  BLOCK block;

  // Try to find the inodes
  if(oufs_find_file(cwd, path_src, &parent_src, &child_src, local_name_bogus) < -1) {
    return(-5);
  }
  if(oufs_find_file(cwd, path_dst, &parent_dst, &child_dst, local_name) < -1) {
    return(-6);
  }

  // SRC must exist
  if(child_src == UNALLOCATED_INODE) {
    fprintf(stderr, "Source not found\n");
    return(-1);
  }

  // DST must not exist, but its parent must exist
  if(parent_dst == UNALLOCATED_INODE) {
    fprintf(stderr, "Destination parent does not exist.\n");
    return(-2);
  }
  if(child_dst != UNALLOCATED_INODE) {
    fprintf(stderr, "Destination already exists.\n");
    return(-3);
  }

  // Get the inode of the dst parent
  if(oufs_read_inode_by_reference(parent_dst, &inode_dst) != 0) {
    return(-7);
  }

  if(inode_dst.type != DIRECTORY_TYPE) {
    fprintf(stderr, "Destination parent must be a directory.");
  }
  // There must be space in the directory
  if(inode_dst.size == N_DIRECTORY_ENTRIES_PER_BLOCK) {
    fprintf(stderr, "No space in destination parent.\n");
    return(-4);
  }


  // TODO
    // Get the inode of the src parent
    if(oufs_read_inode_by_reference(parent_dst, &inode_dst) != 0) {
        return(-7);
    }
    if(oufs_read_inode_by_reference(child_src, &inode_src) != 0) {
        return(-7);
    }
    if (inode_src.type == DIRECTORY_TYPE)
        return -2;
    // Read content
    virtual_disk_read_block(inode_dst.content, &block);
    for (int i=0; i<N_DIRECTORY_ENTRIES_PER_BLOCK; i++)
    {
        if (block.content.directory.entry[i].inode_reference == UNALLOCATED_INODE)
        {
            block.content.directory.entry[i].inode_reference = child_src;
            strcpy(block.content.directory.entry[i].name, local_name);
            inode_dst.size++;
            //INODE inode_child_dst;
            inode_src.n_references++;
            //void oufs_set_inode(*inode_child_dest, INODE_TYPE type, int n_references, BLOCK_REFERENCE content, int size)
            break;
        }
    }
    // write block and both inodes back to disk
    virtual_disk_write_block(inode_dst.content, &block);
    oufs_write_inode_by_reference(parent_dst, &inode_dst);
    oufs_write_inode_by_reference(child_src, &inode_src);
    
    
    // SUCCESS
    return 0;
}


