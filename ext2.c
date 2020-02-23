#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <linux/fs.h>
#include <ext2fs/ext2_fs.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>

unsigned int block_size;

void printInode(struct ext2_inode inode) {
	printf("uid = %u size = %u blocks = %u, first block = %u\n",
		inode.i_uid,
		inode.i_size,
		inode.i_blocks,
		inode.i_block[0]);
}

void printSuperBlock(struct ext2_super_block sb) {
	printf("Magic: %x\n", sb.s_magic);
	printf("Inodes Count: %d\n", sb.s_inodes_count);
	printf("Blocks Count: %d\n", sb.s_blocks_count);
	printf("Blocks per group: %d\n", sb.s_blocks_per_group);
	printf("Inodes per group: %d\n", sb.s_inodes_per_group);
	printf("Last mounted : %s\n", sb.s_last_mounted);
}

int getInodeNumber(int fd, struct ext2_inode inode, char *str) {
	//a function which takes two arguments: directory inode and name of a file in that directory, and returns inode number of the file

	struct ext2_dir_entry_2 dirent;
	int j, size = 0;
		
	lseek(fd, 0, SEEK_SET);
	for(j = 0; j < block_size; j++) {
		lseek(fd, inode.i_block[0], SEEK_CUR);
	}

	read(fd, &dirent, sizeof(struct ext2_dir_entry_2));

	while(size < inode.i_size) {
		if(strncmp(dirent.name, str, dirent.name_len) == 0) {
			break;
		}
		else {
			size = size + dirent.rec_len;
			lseek(fd, dirent.rec_len - sizeof(struct ext2_dir_entry_2), SEEK_CUR);
			read(fd, &dirent, sizeof(struct ext2_dir_entry_2));
		}
			
	}
	//printf("Inode no: %u Name:%s \n\n", dirent.inode, str);

	if(size >= inode.i_size)
		return 0;

	return dirent.inode;
	
	
}

void printInodeDirectories(int fd,  struct ext2_inode inode) {
	
	struct ext2_dir_entry_2 dirent;
	int j, l, size = 0;
	char *str;

	lseek(fd, 0, SEEK_SET);
	for(j = 0; j < block_size; j++) {
		lseek(fd, inode.i_block[0], SEEK_CUR);
	}

	read(fd, &dirent, sizeof(struct ext2_dir_entry_2));

	while(size < inode.i_size) {
		l = dirent.name_len;
		str = (char*)malloc(sizeof(char) * l);
		for(j = 0; j < l; j++) {
			str[j] = dirent.name[j];
		}
		
		printf("%s\n", str);
		size = size + dirent.rec_len;
		lseek(fd, dirent.rec_len - sizeof(struct ext2_dir_entry_2), SEEK_CUR);
		read(fd, &dirent, sizeof(struct ext2_dir_entry_2));

		free(str);
	}
		
}

void printInodeData(int fd, struct ext2_inode inode) {

	int i, j, size = 0, cur_size = 0, count = 0;
	char block[block_size];
	char ch;
	
	lseek(fd, 0, SEEK_SET);

	for(i=0; i<EXT2_N_BLOCKS; i++) {

		if(inode.i_block[i] != 0) {
			
			printf("\nBlock %2u : %u\n", i, inode.i_block[i]);
			lseek(fd, 0, SEEK_SET);
			for(j = 0; j < block_size; j++) {
				lseek(fd, inode.i_block[i], SEEK_CUR);
			}

			printf("Size %d InodeSize %d curSize %d\n", size, inode.i_size, cur_size); 
			while(size < inode.i_size && cur_size < block_size) {
				count = read(fd, &ch, 1);
				printf("%c", ch);
				size += count;
				cur_size += count;
			}

			cur_size = 0;	
		}
	}

	/*for(i=0; i<EXT2_N_BLOCKS; i++)
		if (i < EXT2_NDIR_BLOCKS)                                 // direct blocks
			printf("Block %2u : %u\n", i, inode.i_block[i]);
		else if (i == EXT2_IND_BLOCK)                             //single indirect block
			printf("Single   : %u\n", inode.i_block[i]);
		else if (i == EXT2_DIND_BLOCK)                            //double indirect block
			printf("Double   : %u\n", inode.i_block[i]);
		else if (i == EXT2_TIND_BLOCK)                            //triple indirect block
			printf("Triple   : %u\n", inode.i_block[i]);*/
	
	
}

void printInfo(char* path, char* type) {

	int fd = open("/dev/sdb", O_RDONLY);
	if(fd == -1) {
		perror("Error:");
		exit(errno);
	}

	char *path_cpy;
	int l = strlen(path);
	path_cpy = (char*)malloc(sizeof(char) * l);
	strcpy(path_cpy, path);

	//Traversing path 
	char *str;

	//Structures 
	struct ext2_super_block sb; 
	struct ext2_group_desc bgdesc;
	struct ext2_inode inode;
	struct ext2_dir_entry_2 dirent;

	//Values
	unsigned int bg_desc_count;
	int bg_size;
	int inodes_per_bg;
	
	//Repetitive 
	long long inode_add;
	long bg_no;
	long inode_no;
	long inode_in_table;
	int count;
	int i, flag = 0, dataFlag = 0;

	//Read super block 
	lseek(fd, 1024, SEEK_CUR);
	read(fd, &sb, sizeof(struct ext2_super_block));
	//printSuperBlock(sb);

	//Calculations based on super block
	block_size = 1024 << sb.s_log_block_size;
	bg_desc_count = sb.s_blocks_count / sb.s_blocks_per_group;
	inodes_per_bg = sb.s_inodes_per_group;
	//printf("Number of block groups : %d\n\n", bg_desc_count);
	
	//Read block group desc table entry (one entry = 32 bytes)
	lseek(fd, block_size, SEEK_SET);
	bg_size = read(fd, &bgdesc, sizeof(struct ext2_group_desc));
	//printf("BG0 Inode Table: %d\n", bgdesc.bg_inode_table); 
	
	//Read inode 2 ie root in inode table of BG0
	inode_add = bgdesc.bg_inode_table * block_size + 2*sizeof(struct ext2_inode);
	lseek(fd, inode_add, SEEK_SET);
	read(fd, &inode, sizeof(struct ext2_inode));

	str = strtok(path, "/");
	while(str != NULL) {
		flag = 1;

		inode_no = getInodeNumber(fd,inode,str); //Read directory entries and get inode number 
		str = strtok(NULL, "/");
		if(str == NULL) {
			if(inode_no == 0) {
				printf("%s : File not found by ext2_lookup\n", path_cpy);
				break;
			}
			else {
				if(strcmp(type, "inode") == 0) {
					printf("Inode : %ld\n", inode_no);
					break;
				}
				else {
					dataFlag = 1;
				}
			}
		}
			
		bg_no = (inode_no + 1) / sb.s_inodes_per_group;
		
		lseek(fd, block_size, SEEK_SET); //First bg descriptor 
		lseek(fd, sizeof(struct ext2_group_desc) * bg_no, SEEK_CUR);
		count = read(fd, &bgdesc, sizeof(struct ext2_group_desc));
		lseek(fd, 0, SEEK_SET);
		//printf("BG%ld Inode Table: %d\n", bg_no, bgdesc.bg_inode_table); 
		
		for(i = 0; i < block_size; i++) {
			lseek(fd, bgdesc.bg_inode_table, SEEK_CUR);
		}
		lseek(fd, (inode_no - bg_no * sb.s_inodes_per_group - 1) * sb.s_inode_size, SEEK_CUR);		
		read(fd, &inode, sizeof(struct ext2_inode));
		
		if(dataFlag == 1) {
			if ((inode.i_mode & S_IFMT) == S_IFDIR) {
				printInodeDirectories(fd, inode);		
			}
			else {
				printInodeData(fd, inode);
			}
		}
	}

	if(flag == 0) {
		if(strcmp(type, "inode") == 0) {
			printf("Inode : 2\n");
		}
		else {
			if ((inode.i_mode & S_IFMT) == S_IFDIR) {
				printInodeDirectories(fd, inode);		
			}
		}
			
	}
	
	free(path_cpy);
	close(fd);
}
	

int main(int argc, char *argv[]) {

	if(argc != 3) {
		printf("Usage <./a.out> <path> <inode/data>\n");
		return 0;
	}
	
	if((strcmp(argv[2], "inode") != 0) && (strcmp(argv[2], "data") != 0)) {
		printf("Usage <./a.out> <path> <inode/data>\n");
		return 0;
	}

	printInfo(argv[1], argv[2]);
	
	return 0;
}


