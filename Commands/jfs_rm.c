#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "fs_disk.h"
#include "jfs_common.h"

void rm(jfs_t *jfs, char *pathname)
{
    /* Find the file's inode and its name */
    int root_inode, file_inodenum, parent_inodenum, dir_size, bytes_done = 0, numofbytestocopy;
    char parent_path[MAX_FILENAME_LEN], filename[MAX_FILENAME_LEN];
    struct inode file_inode, parent_inode;
    struct inode * temp_inode;
    struct dirent* dir_entry;
    char block[BLOCKSIZE], parentinodeblock[BLOCKSIZE];

    /* Find file inode and its name. Check if the path exists */
    
    root_inode = find_root_directory(jfs);
    while(pathname[0]=='/') {
      	pathname++;
    }
    
    file_inodenum = findfile_recursive(jfs, pathname, root_inode, DT_FILE);
    if (file_inodenum < 0) {
	fprintf(stderr, "Cannot open file %s\n", pathname);
	exit(1);
    }
   
    last_part(pathname, filename);

    /*Find the parent directory's inode. (No error checking as the file error check should take care of the parent)*/
    all_but_last_part(pathname,parent_path);
    if (strlen(parent_path) == 0)
      parent_inodenum = root_inode;
    else
      parent_inodenum = findfile_recursive(jfs, parent_path, root_inode, DT_DIRECTORY);

    /*The parent will have its dirent deleted and its dirent block size reduced*/
       
    get_inode(jfs, parent_inodenum, &parent_inode);
   
    jfs_read_block(jfs, block, parent_inode.blockptrs[0]);

    dir_size = parent_inode.size;
    
    /* walk the directory block and locate the offset of the dirent */
    
    dir_entry = (struct dirent*)block;
    
    
     while (bytes_done <= dir_size) {
       char name[MAX_FILENAME_LEN + 1];
       memcpy(name, dir_entry->name, dir_entry->namelen);
	
       if(strcmp(name, filename)==0) {
	
	    /* we've found the right one */
	    /* Check it actually is a file */
	
	    if (dir_entry->file_type != DT_FILE) {
		continue;
	    }
	  break;
	}
	
	bytes_done += dir_entry->entry_len;
	dir_entry = (struct dirent*)(block + bytes_done);
	
    }
	
    /* Copy the rest of the block and overwrite the block starting at the dirent that needs to be deleted*/
    numofbytestocopy = dir_size - bytes_done - dir_entry->entry_len;
    memmove(block+bytes_done,block+bytes_done+dir_entry->entry_len, numofbytestocopy);
													   
    /* save block and changes */
    jfs_write_block(jfs, block,  parent_inode.blockptrs[0]);

    /* Reduce the size of directory (The nonsense data at the end of the block is now free to be overwritten) and save */
    
    jfs_read_block(jfs, parentinodeblock, inode_to_block(parent_inodenum));
    temp_inode = (struct inode*)(parentinodeblock + (parent_inodenum % INODES_PER_BLOCK) * INODE_SIZE);

    temp_inode->size -= dir_entry->entry_len;
    jfs_write_block(jfs, parentinodeblock, inode_to_block(parent_inodenum));

    /* List the file inode and all its blocks as free */
    get_inode(jfs, file_inodenum, &file_inode);
	
    int ptrcount;
    /* Check num of blocks file actually used */
    int numoffileblocks = file_inode.size / 512;
    if (file_inode.size%512 != 0){
      numoffileblocks += 1;
    }
	
    for (ptrcount = 0; ptrcount < numoffileblocks; ptrcount++){
      return_block_to_freelist(jfs, file_inode.blockptrs[ptrcount]);
    }
	
    return_inode_to_freelist(jfs, file_inodenum);
    /* commit all this to disk */
    jfs_commit(jfs);
}
     
void usage()
{
  fprintf(stderr, "Usage: jfs_rm <volumename> <filename>\n");
  exit(1);
}

int main(int argc, char **argv)
{
     struct disk_image *di;
     jfs_t *jfs;

     if (argc != 3) {
	 usage();
     }

    di = mount_disk_image(argv[1]);
    jfs = init_jfs(di);
    rm(jfs, argv[2]);

    unmount_disk_image(di);
    
    exit(0);
}
