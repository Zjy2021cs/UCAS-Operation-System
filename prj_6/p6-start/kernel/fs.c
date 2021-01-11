#include <os/fs.h>
#include <sbi.h>
#include <os/stdio.h>
#include <screen.h>
#include <os/time.h>
#include <os/string.h>

inode_t *current_inode;
/* create a FS */
void do_mkfs(){
    // read superblock from disk
    uintptr_t status;
    status = sbi_sd_read(SB_MEM_ADDR, 1, START_SECTOR);
    superblock_t *superblock = (superblock_t *)(SB_MEM_ADDR+P2V_OFFSET);

    // judge if the fs exists. 
    if(superblock->magic == MAGIC_NUM){
        prints("The FS has existed!\n");
        current_inode = (inode_t *)(INODE_MEM_ADDR+P2V_OFFSET);
        sbi_sd_read(INODE_MEM_ADDR, 1, START_SECTOR+INODE_SD_OFFSET); 
        // how to flush? 
    }else{
        screen_reflush();
        prints("[FS] Start initialize filesystem!\n");

        // init superblock
        prints("[FS] Setting superblock...\n");
        clear_sector(superblock);
        superblock->magic = MAGIC_NUM;
        superblock->size = FSYS_SIZE;  
        superblock->start_sector=START_SECTOR; 

        superblock->blockmap_offset=BMAP_SD_OFFSET;
        superblock->blockmap_num=32;                // 16KB/512B = 32
        superblock->inodemap_offset=IMAP_SD_OFFSET;
        superblock->inodemap_num=1;                 // 512B/512B = 1
        superblock->inodes_offset=INODE_SD_OFFSET;
        superblock->inodes_num=512;                 // 256KB/512B = 512
        superblock->datablock_offset=DATA_SD_OFFSET;
        superblock->datablock_num=1048001;

        superblock->inode_sz=IENTRY_SIZE;           //64B
        superblock->dentry_sz=DENTRY_SIZE;          //32B

        prints("magic : 0x%x                                      \n",superblock->magic);
        prints("num sector : %d, start_sector : %d                \n",superblock->size,superblock->start_sector);
        prints("block map offset : %d (%d)                        \n",superblock->blockmap_offset,superblock->blockmap_num);
        prints("inode map offset : %d (%d)                        \n",superblock->inodemap_offset,superblock->inodemap_num);
        prints("data offset : %d (%d)                             \n",superblock->datablock_offset,superblock->datablock_num);
        prints("inode entry size : 64B, dir entry size : 32B      \n");

        // init block map
        prints("[FS] Setting block-map...\n");
        uint8_t *blockmap = (uint8_t *)(BMAP_MEM_ADDR+P2V_OFFSET);
        //clear_sectors();
        int i;
        for(i=0;i<69;i++)   // the first 69 blocks has been used..(512B+16KB+512B+256KB)/4KB = 69
            blockmap[i/8]=blockmap[i/8] | (1<<(7-(i % 8)));

        // init inode map
        prints("[FS] Setting inode-map...\n");
        uint8_t *inodemap = (uint8_t *)(IMAP_MEM_ADDR+P2V_OFFSET);
        clear_sector(inodemap);
        // the first inode has been used as root-dir
        inodemap[0]=1<<7;     

        // write back into disk
        status = sbi_sd_write(SB_MEM_ADDR, 34, START_SECTOR);

        // init root-dir inode
        prints("[FS] Setting inode...\n");
        inode_t *inode = (inode_t *)(INODE_MEM_ADDR+P2V_OFFSET);
        inode[0].inum = ROOTINUM;
        inode[0].mode = T_DIR;
        inode[0].access = A_RW;
        inode[0].ref = 0;
        inode[0].size = 4096;
        inode[0].valid_size = 64;  //(has 2-entry:".","..")
        inode[0].mtime = get_timer();
        inode[0].direct_bnum[0] = Alloc_datablock(); 
        for(i=1;i<NDIRECT;i++)
            inode[0].direct_bnum[i] = 0;
        inode[0].indirect1_bnum = 0;
        inode[0].indirect2_bnum = 0;
        write_inode_sector(inode[0].inum);
        // init dentry of root-dir "." point to itself, ".." point to its father(also itself)
        init_dentry(inode[0].direct_bnum[0], 0, 0);
        
        // write back to disk
        //uintptr_t sbi_sd_write(unsigned int mem_address, unsigned int num_of_blocks(512B), unsigned int block_id)
        prints("[FS] Initialize filesystem finished!\n");
        screen_reflush();
        current_inode = inode;
    }
}

/* print metadata info of fs */
void do_statfs(){
    // read superblock
    uintptr_t status;
    status = sbi_sd_read(SB_MEM_ADDR, 1, START_SECTOR);
    superblock_t *superblock = (superblock_t *)(SB_MEM_ADDR+P2V_OFFSET);

    // print info
    if(superblock->magic != MAGIC_NUM){
        prints("[ERROR] No File System!\n");
        return;
    }
    int i, j;
    uint32_t used_block = 0, used_inodes = 0;
    status = sbi_sd_read(BMAP_MEM_ADDR, 32, START_SECTOR+BMAP_SD_OFFSET);
    uint8_t *blockmap = (uint8_t *)(BMAP_MEM_ADDR+P2V_OFFSET);
    for(i = 0; i < superblock->blockmap_num*0x200/8; i++)
        for(j = 0; j < 8; j++)
            used_block += (blockmap[i] >> j) & 1;
    status = sbi_sd_read(IMAP_MEM_ADDR, 32, START_SECTOR+IMAP_SD_OFFSET);
    uint8_t *inodemap = (uint8_t *)(IMAP_MEM_ADDR+P2V_OFFSET);
    for(i = 0; i < superblock->inodemap_num*0x200/8; i++)
        for(j = 0; j < 8; j++)
            used_inodes += (inodemap[i] >> j) & 1;

    prints("magic : 0x%x\n",superblock->magic);
    prints("used block: %d/%d, start sector: %d (0x%x)\n", used_block, FSYS_SIZE, START_SECTOR, START_SECTOR);
    prints("block map offset: %d, occupied sector: %d\n", superblock->blockmap_offset, superblock->datablock_num);
    prints("inode map offset: %d, occupied sector: %d, used: %d\n", superblock->inodemap_offset, superblock->inodemap_num, used_inodes);
    prints("inode offset: %d, occupied sector: %d\n", superblock->inodes_offset, superblock->inodes_num);
    prints("data offset: %d, occupied sector: %d\n", superblock->datablock_offset, superblock->datablock_num);
    prints("inode entry size: %dB, dir entry size : %dB\n",  sizeof(inode_t),sizeof(dentry_t));
    // screen_reflush();
}

/* create a dir */
void do_mkdir(uintptr_t dirname){
    // check if FS exist
    sbi_sd_read(SB_MEM_ADDR, 1, START_SECTOR);
    superblock_t *superblock = (superblock_t *)(SB_MEM_ADDR+P2V_OFFSET);
    if(superblock->magic != MAGIC_NUM){
        prints("[ERROR] No File System!\n");
        return;
    }

    // check its parent dir and return parent inode point (simple: parent = current_dir_inode?)
    inode_t *dp = current_inode;
    // read parent inode and check if the name has existed in parent dir
    inode_t *ip = dirlookup(dp, (char *)dirname, T_DIR);
    if(ip!=0 && ip->mode==T_DIR){
        prints("[ERROR] The dir has existed!\n");
        return;
    }
    // alloc an inode for dir
    uint32_t inum;
    if((inum = Alloc_inode()) == 0){
        prints("[ERROR] Inode is full!\n");
        return;
    }
    ip = iget(inum);
    // init inode
    ip->inum = inum;
    ip->mode = T_DIR;
    ip->access = A_RW;
    ip->ref = 0;
    ip->size = 4096;
    ip->valid_size = 64;  //(has 2-entry:".","..")
    ip->mtime = get_timer();
    ip->direct_bnum[0] = Alloc_datablock(); 
    for(int i=1;i<NDIRECT;i++)
        ip->direct_bnum[i] = 0;
    ip->indirect1_bnum = 0;
    ip->indirect2_bnum = 0;
    write_inode_sector(ip->inum);
    // Create . and .. dentries in new dir
    init_dentry(ip->direct_bnum[0], ip->inum, dp->inum);
    // add dentry in its parent dir
    sbi_sd_read(DATA_MEM_ADDR, 1, START_SECTOR + (dp->direct_bnum[0])*8);
    dentry_t *dentry = (dentry_t *)(DATA_MEM_ADDR+P2V_OFFSET+dp->valid_size);
    dentry->mode = T_DIR;
    dentry->inum = ip->inum;
    kstrcpy(dentry->name, (char *)dirname);
    sbi_sd_write(DATA_MEM_ADDR, 1, START_SECTOR + (dp->direct_bnum[0])*8);
    // update parent dir's time and link
    dp->ref++; 
    dp->valid_size += DENTRY_SIZE;
    dp->mtime = get_timer();
    write_inode_sector(dp->inum);
    
    prints("[SYS] Successed! inum:%d sd_addr:%x\n", ip->inum,ip->direct_bnum[0]);
}

/* remove a dir */
void do_rmdir(uintptr_t dirname){
    // check if FS exist
    uintptr_t status;
    status = sbi_sd_read(SB_MEM_ADDR, 1, START_SECTOR);
    superblock_t *superblock = (superblock_t *)(SB_MEM_ADDR+P2V_OFFSET);
    if(superblock->magic != MAGIC_NUM){
        prints("[ERROR] No File System!\n");
        return;
    }
    // check its parent dir and return parent inode point (simple: parent = current_dir_inode)
    inode_t *dp = current_inode;
    // read parent inode and check if the name has existed in parent dir, if exist get its inode
    inode_t *ip;
    if((ip = dirlookup(dp, (char *)dirname, T_DIR)) == 0){
        prints("[ERROR] No such directory!\n");
        return;
    }
    // free the inode and the datablock for files in it /*do not include son-dir*/???

    // free the inode and the datablock for dir (nlink--?)
    for(int i=0;i<(ip->size/BSIZE);i++){
        Clear_block_map(ip->direct_bnum[i]);
    }
    Clear_inode_map(ip->inum);
    // delete dentry in its parent dir
    sbi_sd_read(DATA_MEM_ADDR, 1, START_SECTOR + (dp->direct_bnum[0])*8);
    dentry_t *dentry = (dentry_t *)(DATA_MEM_ADDR+P2V_OFFSET);
    int off;int found=0;
    for(off = 0; off < dp->valid_size; off += DENTRY_SIZE){
        if(found){
            memcpy((uint8_t *)(dentry-DENTRY_SIZE), (uint8_t *)dentry, DENTRY_SIZE);
        }
        if(ip->inum==dentry->inum){
            found = 1;
        }
        dentry++;
    }
    sbi_sd_write(DATA_MEM_ADDR, 1, START_SECTOR + (dp->direct_bnum[0])*8);
    // update parent dir's size, time and link
    dp->ref--; 
    dp->valid_size -= DENTRY_SIZE;
    dp->mtime = get_timer();
    write_inode_sector(dp->inum);

    prints("[SYS] Delete %s!\n", (char *)dirname);
}

/* go to a dir */
void do_cd(uintptr_t dirname){
    // check if FS exist
    uintptr_t status;
    status = sbi_sd_read(SB_MEM_ADDR, 1, START_SECTOR);
    superblock_t *superblock = (superblock_t *)(SB_MEM_ADDR+P2V_OFFSET);
    if(superblock->magic != MAGIC_NUM){
        prints("[ERROR] No File System!\n");
        return;
    }
    // look up the path
    inode_t *ionde_temp=current_inode;
    char *path = (char *)dirname;
    if(path[0]!='\0'){
        if(find_path(path,T_DIR)==0 || current_inode->mode != T_DIR){
            prints("[ERROR] PATH NOT FOUND!\n");
            current_inode = ionde_temp;
            return;
        }
    }
}

/* print all contents in a dir */
void do_ls(uintptr_t dirname){
    // check if FS exist
    sbi_sd_read(SB_MEM_ADDR, 1, START_SECTOR);
    superblock_t *superblock = (superblock_t *)(SB_MEM_ADDR+P2V_OFFSET);
    if(superblock->magic != MAGIC_NUM){
        prints("[ERROR] No File System!\n");
        return;
    }
    // look up the path and go to it
    inode_t *ionde_temp=current_inode;
    char *path = (char *)dirname;
    if(path[0]!='\0'){
        if(find_path(path,T_DIR)==0 || current_inode->mode != T_DIR){
            prints("[ERROR] PATH NOT FOUND!\n");
            current_inode = ionde_temp;
            return;
        }
    }

    // read dentries in current dir
    sbi_sd_read(DATA_MEM_ADDR, 1, START_SECTOR + (current_inode->direct_bnum[0])*8);
    dentry_t *dentry = (dentry_t *)(DATA_MEM_ADDR+P2V_OFFSET);
    int off;
    for(off = 0; off < current_inode->valid_size; off += DENTRY_SIZE){
        prints("%s\n",dentry->name);
        dentry++;
    }

    // back to origin path
    current_inode = ionde_temp;
}

//file operation============================================================================================
/* create a file */
void do_touch(uintptr_t filename){
    // check if FS exist
    sbi_sd_read(SB_MEM_ADDR, 1, START_SECTOR);
    superblock_t *superblock = (superblock_t *)(SB_MEM_ADDR+P2V_OFFSET);
    if(superblock->magic != MAGIC_NUM){
        prints("[ERROR] No File System!\n");
        return;
    }
    // check its parent dir and return parent inode point (simple: parent = current_dir_inode)
    inode_t *dp = current_inode;
    // read parent inode and check if the name has existed in parent dir
    inode_t *ip = dirlookup(dp, (char *)filename, T_FILE);
    if(ip!=0 && ip->mode==T_FILE){        
        prints("[ERROR] The file has existed!\n");
        return;
    }
    // alloc an inode for file
    uint32_t inum;
    if((inum = Alloc_inode()) == 0){
        prints("[ERROR] Inode is full!\n");
        return;
    }
    ip = iget(inum);
    // init inode
    ip->inum = inum;
    ip->mode = T_FILE;
    ip->access = A_RW;
    ip->ref = 0;
    ip->size = 4096;      // init 1 block for file
    ip->valid_size = 0;
    ip->mtime = get_timer();
    ip->direct_bnum[0] = Alloc_datablock(); 
    for(int i=1;i<NDIRECT;i++)
        ip->direct_bnum[i] = 0;
    ip->indirect1_bnum = 0;
    ip->indirect2_bnum = 0;
    write_inode_sector(ip->inum);
    // add dentry in its parent dir
    sbi_sd_read(DATA_MEM_ADDR, 1, START_SECTOR + (dp->direct_bnum[0])*8);
    dentry_t *dentry = (dentry_t *)(DATA_MEM_ADDR+P2V_OFFSET+dp->valid_size);
    dentry->mode = T_FILE;
    dentry->inum = ip->inum;
    kstrcpy(dentry->name, (char *)filename);
    sbi_sd_write(DATA_MEM_ADDR, 1, START_SECTOR + (dp->direct_bnum[0])*8);
    // update parent dir's time and link
    dp->ref++; 
    dp->valid_size += DENTRY_SIZE;
    dp->mtime = get_timer();
    write_inode_sector(dp->inum);
    
    prints("[SYS] Create file successed! inum:%d sd_addr:%x\n", ip->inum,ip->direct_bnum[0]);
}
/* print contents of a file */
void do_cat(uintptr_t filename){
    // check if FS exist
    sbi_sd_read(SB_MEM_ADDR, 1, START_SECTOR);
    superblock_t *superblock = (superblock_t *)(SB_MEM_ADDR+P2V_OFFSET);
    if(superblock->magic != MAGIC_NUM){
        prints("[ERROR] No File System!\n");
        return;
    }
    // look up the path and find the target file 
    inode_t *ionde_temp=current_inode;
    char *path = (char *)filename;
    if(path[0]!='\0'){
        if(find_path(path,T_FILE)==0 || current_inode->mode != T_FILE){
            prints("[ERROR] FILE NOT FOUND!\n");
            current_inode = ionde_temp;
            return;
        }
    }
    // read its datablock and print (now only small file!!!)
    uint32_t cnt=0;
    int off,i,j,k;
    for(off = 0; off < current_inode->valid_size; off += BSIZE){
        //read 1 block once
        for(i=0;i<8;i++)
            clear_sector(DATA_MEM_ADDR+P2V_OFFSET+i*SSIZE);
        j = off / BSIZE;
        sbi_sd_read(DATA_MEM_ADDR, 8, START_SECTOR + (current_inode->direct_bnum[j])*8);
        char *file_buf = (char *)(DATA_MEM_ADDR+P2V_OFFSET);
        for(k=0;cnt < current_inode->valid_size && k < BSIZE ;cnt++,k++){
            prints("%c",file_buf[k]);
        }
    }
    prints("\n");

    // back to origin path
    current_inode = ionde_temp;
}

fd_t FD_table[NOFILE];
/* open an existing/non-existing file */
long do_file_open(uintptr_t name, int access){ // access = A_R/A_W/A_RW
    // check the target file:
        // if exist, return inode point; if not exist, create one
    inode_t *ionde_temp=current_inode;
    char *path = (char *)name;
    if(path[0]!='\0'){
        if(find_path(path,T_FILE)==0 || current_inode->mode != T_FILE){
            prints("File not exist! creating...\n");
            // create a new file(touch)
            inode_t *dp = current_inode; 
            // alloc an inode for file
            uint32_t inum;
            if((inum = Alloc_inode()) == 0){
                prints("[ERROR] Inode is full!\n");
                return;
            }
            inode_t *ip = iget(inum);
            // init inode
            ip->inum = inum;
            ip->mode = T_FILE;
            ip->access = A_RW;
            ip->ref = 0;
            ip->size = 4096;      // init 1 block for file
            ip->valid_size = 0;
            ip->mtime = get_timer();
            ip->direct_bnum[0] = Alloc_datablock(); 
            for(int i=1;i<NDIRECT;i++)
                ip->direct_bnum[i] = 0;
            ip->indirect1_bnum = 0;
            ip->indirect2_bnum = 0;
            write_inode_sector(ip->inum);
            // add dentry in its parent dir
            sbi_sd_read(DATA_MEM_ADDR, 1, START_SECTOR + (dp->direct_bnum[0])*8);
            dentry_t *dentry = (dentry_t *)(DATA_MEM_ADDR+P2V_OFFSET+dp->valid_size);
            dentry->mode = T_FILE;
            dentry->inum = ip->inum;
            kstrcpy(dentry->name, (char *)name);
            sbi_sd_write(DATA_MEM_ADDR, 1, START_SECTOR + (dp->direct_bnum[0])*8);
            // update parent dir's time and link
            dp->ref++; 
            dp->valid_size += DENTRY_SIZE;
            dp->mtime = get_timer();
            write_inode_sector(dp->inum);
            current_inode = ip;
        }
    } 
        // check its access
    if(current_inode->access!=A_RW && current_inode->access!=access){
        prints("[ERROR] Can not ACCESS!\n");
        return -1; 
    }
           
    // alloc a fd for it, fill fd
    int fd_idx= Alloc_fd();
    if(fd_idx==-1){
        prints("[ERROR] FD is FULL!\n");
        return -1;
    }     
    FD_table[fd_idx].inum = current_inode->inum;
    FD_table[fd_idx].access = access;
    FD_table[fd_idx].read_point = 0;
    FD_table[fd_idx].write_point = 0;
    
    // return fd index
    current_inode = ionde_temp;
    return fd_idx;
}
/* read bytes from an open file(idx=fd) */
long do_file_read(int fd, uintptr_t buff, int size){
    // check if the file can read
    if(FD_table[fd].inum==0 || FD_table[fd].access == A_W){
        prints("[ERROR] The FILE CAN NOT READ!\n");
        return -1;
    }
    inode_t *ionde_file = iget(FD_table[fd].inum);
    uint32_t cur_pos =  FD_table[fd].read_point;
    uint32_t start_pos =  FD_table[fd].read_point;
    uint32_t start_block = cur_pos/BSIZE;
    uint32_t end_block = (cur_pos+size)/BSIZE;
    
    /* read from sd-card, now only small file!!! */
    char *file_buf = (char *)(DATA_MEM_ADDR+P2V_OFFSET);
    int i;
    for(i=0;i<(end_block-start_block+1);i++){
        if( (start_block+i) >= (ionde_file->valid_size/BSIZE +1) ){
            prints("[ERROR] SIZE OUT OF FILE!\n");
            return -1;
        }
        if( (cur_pos%BSIZE + size - (cur_pos-start_pos)) < BSIZE)
        {   //read the rest
            sbi_sd_read(DATA_MEM_ADDR, 8, START_SECTOR + (ionde_file->direct_bnum[start_block+i])*8);
            memcpy((uint8_t *)(buff + cur_pos%BSIZE), (uint8_t *)(file_buf + cur_pos%BSIZE), size);
            cur_pos += size;
        }else
        {   //read whole block
            sbi_sd_read(DATA_MEM_ADDR, 8, START_SECTOR + (ionde_file->direct_bnum[start_block+i])*8);
            /*memcpy(buff + (cur_pos-start_pos), file_buf, BSIZE);
            cur_pos += BSIZE;*/
            memcpy((uint8_t *)(file_buf + cur_pos%BSIZE), (uint8_t *)(buff + (cur_pos-start_pos)), BSIZE - cur_pos%BSIZE);
            cur_pos += (BSIZE - cur_pos%BSIZE);
        }
    }

    FD_table[fd].read_point=cur_pos;
    return 1;
}
/* write bytes into an open file(idx=fd) */
long do_file_write(int fd, uintptr_t buff, int size){
    // check if the file can read
    if(FD_table[fd].inum==0 || FD_table[fd].access == A_R){
        prints("[ERROR] The FILE CAN NOT WRITE!\n");
        return -1;
    }
    inode_t *ionde_file = iget(FD_table[fd].inum);
    uint32_t cur_pos =  FD_table[fd].write_point;
    uint32_t start_pos =  FD_table[fd].write_point;
    uint32_t start_block = cur_pos/BSIZE;
    uint32_t end_block = (cur_pos+size)/BSIZE;
    // write back to disk
    char *file_buf = (char *)(DATA_MEM_ADDR+P2V_OFFSET);
    int i,m,cnt=0;
    for(i=0;i<(end_block-start_block+1);i++){
        if( (start_block+i) >= (ionde_file->valid_size/BSIZE+1) ){ // expand the file
            ionde_file->direct_bnum[start_block+i]=Alloc_datablock();
            ionde_file->size += BSIZE;
        }
        
        if( (cur_pos%BSIZE + size - (cur_pos-start_pos)) < BSIZE)
        {
            sbi_sd_read(DATA_MEM_ADDR, 8, START_SECTOR + (ionde_file->direct_bnum[start_block+i])*8);
            memcpy((uint8_t *)(file_buf + cur_pos%BSIZE), (uint8_t *)buff, size);
            cur_pos += size;
            sbi_sd_write(DATA_MEM_ADDR, 8, START_SECTOR + (ionde_file->direct_bnum[start_block+i])*8);
        }else
        {
            sbi_sd_read(DATA_MEM_ADDR, 8, START_SECTOR + (ionde_file->direct_bnum[start_block+i])*8);
            memcpy((uint8_t *)(file_buf + cur_pos%BSIZE), (uint8_t *)(buff + (cur_pos-start_pos)), BSIZE - cur_pos%BSIZE);
            cur_pos += (BSIZE - cur_pos%BSIZE);
            sbi_sd_write(DATA_MEM_ADDR, 8, START_SECTOR + (ionde_file->direct_bnum[start_block+i])*8); 
        }
    }
    // update the file's inode
    if(FD_table[fd].write_point==0){
        ionde_file->valid_size = size;
    }else{
        ionde_file->valid_size+=size;
    }
    ionde_file->mtime=get_timer();
    write_inode_sector(ionde_file->inum);
    FD_table[fd].write_point=cur_pos;
    return 1;
}
void do_file_close(int fd){
    uint32_t inum = FD_table[fd].inum;
    inode_t *ip = iget(inum);
    ip->mtime = get_timer();
    write_inode_sector(inum);
    FD_table[fd].inum = 0;
    FD_table[fd].access = 0;
}

//=============================================================================================
/* clear one sector in mem */
void clear_sector(uintptr_t mem_addr){
    int i;
    for(i=0;i<SSIZE;i+=8){
        *((uint64_t *)(mem_addr+i)) = 0;
    }
}

/* set block_id bit=1 */
void Set_block_map(uint32_t block_id){
    //sbi_sd_read(BMAP_MEM_ADDR, 32, START_SECTOR+BMAP_SD_OFFSET);
    uint8_t *blockmap = (uint8_t *)(BMAP_MEM_ADDR+P2V_OFFSET);
    blockmap[block_id/8]=blockmap[block_id/8] | (1<<(7-(block_id % 8)));
    uint32_t sector_offset = block_id/4096;
    sbi_sd_write(BMAP_MEM_ADDR+sector_offset*512, 1, START_SECTOR+BMAP_SD_OFFSET+sector_offset);
}

/* set inode_id bit=1 */
void Set_inode_map(uint32_t inode_id){
    //sbi_sd_read(BMAP_MEM_ADDR, 32, START_SECTOR+BMAP_SD_OFFSET);
    uint8_t *inodemap = (uint8_t *)(IMAP_MEM_ADDR+P2V_OFFSET);
    inodemap[inode_id/8]=inodemap[inode_id/8] | (1<<(7-(inode_id % 8)));
    sbi_sd_write(IMAP_MEM_ADDR, 1, START_SECTOR+IMAP_SD_OFFSET);
}

/* set block_id bit=0 */
void Clear_block_map(uint32_t block_id){
    //sbi_sd_read(BMAP_MEM_ADDR, 32, START_SECTOR+BMAP_SD_OFFSET);
    uint8_t *blockmap = (uint8_t *)(BMAP_MEM_ADDR+P2V_OFFSET);
    blockmap[block_id/8]=blockmap[block_id/8] & (0xff-1<<(7-(block_id % 8)));
    uint32_t sector_offset = block_id/4096;
    sbi_sd_write(BMAP_MEM_ADDR+sector_offset*512, 1, START_SECTOR+BMAP_SD_OFFSET+sector_offset);
}

/* set inode_id bit=0 */
void Clear_inode_map(uint32_t inode_id){
    //sbi_sd_read(BMAP_MEM_ADDR, 32, START_SECTOR+BMAP_SD_OFFSET);
    uint8_t *inodemap = (uint8_t *)(IMAP_MEM_ADDR+P2V_OFFSET);
    inodemap[inode_id/8]=inodemap[inode_id/8] & (0xff-1<<(7-(inode_id % 8)));
    sbi_sd_write(IMAP_MEM_ADDR, 1, START_SECTOR+IMAP_SD_OFFSET);
}

/* alloc 4KB for data, return the block_num */
uint32_t Alloc_datablock(){
    sbi_sd_read(BMAP_MEM_ADDR, 32, START_SECTOR+BMAP_SD_OFFSET);
    uint8_t *blockmap = (uint8_t *)(BMAP_MEM_ADDR+P2V_OFFSET);
    //search blockmap to find a free block
    int i,j;
    uint32_t free_block = 0;
    for(i = 0; i < 16*1024/8; i++){
        for(j = 0; j < 8; j++)
            if(!(blockmap[i] &  (0x80 >>j))){
                free_block= 8*i+j;
                Set_block_map(free_block);
                return free_block;
            }
    }
    return free_block;
}

/* alloc an inode */
uint32_t Alloc_inode(){
    //search inodemap to find a free inode
    sbi_sd_read(IMAP_MEM_ADDR, 1, START_SECTOR+IMAP_SD_OFFSET);
    uint8_t *inodemap = (uint8_t *)(IMAP_MEM_ADDR+P2V_OFFSET);
    int i,j;
    uint32_t free_inode = 0;
    for(i = 0; i < 512/8; i++){
        for(j = 0; j < 8; j++)
            if(!(inodemap[i] &  (0x80 >>j))){
                free_inode= 8*i+j;
                Set_inode_map(free_inode);
                return free_inode;
            }
    }
    return free_inode;
}

/* write a sector that include inode(=inum) into disk */
void write_inode_sector(uint32_t inum){
    uint32_t sector_offset = inum/IPS;
    sbi_sd_write(INODE_MEM_ADDR+sector_offset*512, 1, START_SECTOR+INODE_SD_OFFSET+sector_offset);        
}
/* init "."and".." for a dir */
void init_dentry(uint32_t block_num, uint32_t cur_inum, uint32_t parent_inum)
{
    dentry_t *dentry = (dentry_t *)(DATA_MEM_ADDR+P2V_OFFSET);
    clear_sector(dentry);
    dentry[0].mode = T_DIR;
    dentry[0].inum = cur_inum;
    kstrcpy(dentry[0].name, (char *)".");

    dentry[1].mode = T_DIR;
    dentry[1].inum = parent_inum;
    kstrcpy(dentry[1].name, (char *)"..");

    sbi_sd_write(DATA_MEM_ADDR, 1, START_SECTOR+block_num*8);        
}

/* Find the inode with inum and return the in-memory copy inode point */
inode_t *iget(uint32_t inum)  
{
  inode_t *ip = (inode_t *)(INODE_MEM_ADDR+P2V_OFFSET+IENTRY_SIZE*inum);
  uint32_t sector_offset = inum/IPS; 
  sbi_sd_read(INODE_MEM_ADDR+P2V_OFFSET+512*sector_offset, 1, START_SECTOR+INODE_SD_OFFSET+sector_offset);
  return ip;
}

/* Look for a directory entry in a directory, return in-memory copy inode point */
inode_t *dirlookup(inode_t *dp, char *name, int type)
{
    int off, inum;
    dentry_t *dentry = (dentry_t *)(DATA_MEM_ADDR+P2V_OFFSET);

    if(dp->mode != T_DIR){
        prints("[ERROR] Input not a dir!\n");
        return 0;
    } 
    // in my fs, a dir only has 1 datablock
    sbi_sd_read(DATA_MEM_ADDR, 1, START_SECTOR + (dp->direct_bnum[0])*8);
    for(off = 0; off < dp->valid_size; off += DENTRY_SIZE){
        if((strcmp(name, dentry->name) == 0) && (dentry->mode==type)){
            // entry matches path element
            inum = dentry->inum;
            return iget(inum);
        }
        dentry++;
    }
    return 0;
}

/* Look for a path(dir1/dir2/dir3) in fs, return find or not find */
// for resolve path
static    char head_dir[30];
static    char tail_dir[30];
int find=0;
int dep=0;
int find_path(char * path, int type){
    inode_t *inode_temp = current_inode;
    if(dep==0){
        find=0;
    }

    /* Absolute path */
    if(path[0]=='/'){
        current_inode=(inode_t *)(INODE_MEM_ADDR+P2V_OFFSET);
        if(strlen(path)==1)
            return 1;
        int m;
        for(m=0;m<strlen(path);m++)
            path[m]=path[m+1];
    }

    int i,j;
    for(i=0,j=0;i<strlen(path) && path[i]!='/';i++,j++){
        head_dir[j]=path[i];
    }
    head_dir[j++]='\0';
    i++;

    for(j=0;i<strlen(path);i++,j++)
        tail_dir[j]=path[i];
    tail_dir[j]='\0';

    if(head_dir[0]=='\0')
		return 0;

    inode_t *ip;
    if((ip = dirlookup(current_inode,head_dir,type)) != 0){
        current_inode = ip;
        if(tail_dir[0]=='\0'){
            find=1;
		    return 1;
        }
        dep++;
        find_path(tail_dir,type);
    }

    if(find==0){
        current_inode = inode_temp;
        dep=0;
        return 0;
    }else{
        dep=0;
        return 1;
    }
}

/* alloc a fd in-memory and return the index */
int Alloc_fd(){
    int i;
    for(i=0;i<NOFILE;i++){
        if(FD_table[i].inum==0)
            return i;
    }
    return -1;
}
