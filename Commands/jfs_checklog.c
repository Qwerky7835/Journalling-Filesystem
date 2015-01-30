#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "fs_disk.h"
#include "jfs_common.h"

void checklog(jfs_t *jfs)
{
    int root_inodenum, logfile_inodenum, logfile_missing = 0;
    struct inode logfile_inode;
    struct cached_block *last_cache_entry, *first_cache_entry;
    struct commit_block *commit_block;
    char block[BLOCKSIZE];

    /* Find and open the logfile */
    root_inodenum = find_root_directory(jfs);
    logfile_inodenum = findfile_recursive(jfs, ".log", root_inodenum, DT_FILE);
    if (logfile_inodenum < 0) {
	fprintf(stderr, "Missing logfile!\n");
	logfile_missing = 1;
    }

    if (logfile_missing == 0) {
	get_inode(jfs, logfile_inodenum, &logfile_inode);
    }
    /* Check if the commit block is committed and if the checksum adds up*/
    /* The commit block is after the cache_tail */
       
    last_cache_entry = jfs->cache_tail;
    if (last_cache_entry == NULL){
      printf("Filesystem is consistent \n");
      return;
    }
    printf("cache tail \n",last_cache_entry);
    int checksum = 0;
    int c_ptr_list;
    for (c_ptr_list = 0; c_ptr_list < INODE_BLOCK_PTRS; c_ptr_list++){
      checksum += logfile_inode.blockptrs[c_ptr_list];
      
      if(logfile_inode.blockptrs[c_ptr_list]==last_cache_entry->blocknum){
	/* Found the commit block */	
	printf("%i \n", logfile_inode.blockptrs[c_ptr_list+1]);
	read_block(jfs->d_img, block, logfile_inode.blockptrs[c_ptr_list+1]);
    	commit_block = (struct commit_block *)block;
	/* Check Magicnum */
    	if (commit_block -> magicnum != 0x89abcdef){
    	  fprintf(stderr, "Incorrect Magic Number.");
    	  exit(1);
  	}
	/* Check checksum */
   	if (commit_block->sum != checksum){
    	  fprintf(stderr, "Incorrect checksum. Corrupted log entries.");
    	  exit(1);
    	}  
      }	
    }

    /*If the commit block is uncommited then read logfile and reapply changes*/ 
     
 if (commit_block->uncommitted){
      first_cache_entry = jfs->cache_head;
      while (first_cache_entry != NULL) {
	write_block(jfs->d_img, first_cache_entry->blockdata, first_cache_entry->blocknum);
	jfs->cache_head = first_cache_entry->next;
	first_cache_entry = jfs->cache_head;
       }
       jfs->cache_tail = NULL;
    } else {
      printf("All changes committed. File consistent");
      return;
    }
    
     /* Data has reached the disk, erase the logfile by overwriting the previous commit block. */
    
    int i;
	for (i = 0; i < INODE_BLOCK_PTRS; i++)
	    commit_block->blocknums[i] = -1;
	commit_block->sum = 0;
	commit_block->uncommitted = 0;
	write_block(jfs->d_img, block, logfile_inode.blockptrs[c_ptr_list+1]);
     
    free(first_cache_entry);
    free(last_cache_entry);
    return;
}
     
void usage() {
    fprintf(stderr, "Usage:  jfs_checklog <volumename>\n");
    exit(1);
}

int main(int argc, char **argv)
{
     struct disk_image *di;
     jfs_t *jfs;

     if (argc != 2) {
	 usage();
     }

    di = mount_disk_image(argv[1]);
    jfs = init_jfs(di);
    checklog(jfs);

    unmount_disk_image(di);
    
    exit(0);
}
