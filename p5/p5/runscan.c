#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ext2_fs.h"
#include "read_ext2.h"
#include <sys/types.h>
#include <dirent.h>

#define blocks_per_block	(int)(block_size/sizeof(int))

#define MAX_FILENAME_LEN 128
#define MAX_PATHNAME_LEN 256
#define MAX_OUT_PATHNAME_LEN 512

void read_block(fd, ngroup, offset, block_no, block)
     int                            fd;        /* the floppy disk file descriptor */
	 int 							ngroup;
     off_t 			   				offset;    /* offset to the start of the inode table */
     int                            block_no;  /* the inode number to read  */
     char             				*block;     /* where to put the block */
{
		ngroup = ngroup;
		offset = offset;
        lseek(fd, BLOCK_OFFSET(block_no), SEEK_SET);
        read(fd, block, block_size);
}

int is_jpg(int fd, struct ext2_inode *inode) {
	int __is_jpg = 0;
	char buffer[block_size];
	int block_no = inode->i_block[0];
	read_block(fd, 0, 0, block_no, buffer);
	if (buffer[0] == (char)0xff &&
    buffer[1] == (char)0xd8 &&
    buffer[2] == (char)0xff &&
    (buffer[3] == (char)0xe0 ||
     buffer[3] == (char)0xe1 ||
     buffer[3] == (char)0xe8)) {
	 	__is_jpg = 1;
	 }
	return __is_jpg;
}



void copy_indirect_block(int fd, int output_fd, struct ext2_inode *inode, char *ind_block, int current_ind_block) {
	for (int i = 0; i < blocks_per_block && ((int*)ind_block)[i] != 0; i++) {
		char buffer[block_size];
		read_block(fd, 0, 0, ((int*)ind_block)[i], buffer);
		if (inode->i_size < (EXT2_NDIR_BLOCKS+current_ind_block*blocks_per_block+i+1)*block_size) {
			write(output_fd, buffer, inode->i_size - (EXT2_NDIR_BLOCKS+current_ind_block*blocks_per_block+i)*block_size);
		}
		else {
			write(output_fd, buffer, block_size);
		}
	}
}

void copy_fread(char * src, char * dst){
    FILE *fpbr, *fpbw;

    // Try to open source file
    fpbr = fopen(src, "rb");
    if (fpbr == NULL) {
        printf("Error for opening source file %s!\n", src);
        exit(1);
    }

    // Try to open destination file
    fpbw = fopen(dst, "wb");
    if (fpbr == NULL) {
        printf("Error for opening destination file %s!\n", dst);
        exit(1);
    }

    // Copy file with buffer fread() and fwrite()
    size_t len = 0;
    char buffer[BUFSIZ] = {'\0'};  // BUFSIZ macro defined in <stdio.h>
    while ((len = fread(buffer, sizeof(char), BUFSIZ, fpbr)) > 0)
        fwrite(buffer, sizeof(char), len, fpbw);

    //"Copy file successfully!\n");

    fclose(fpbr);
    fclose(fpbw);
}

void copy_file(int fd, struct ext2_inode *inode, char* path, int node_no, char *filename) {
	char file_path_buf[MAX_OUT_PATHNAME_LEN];
	char real_name[MAX_OUT_PATHNAME_LEN];
	sprintf(file_path_buf, "%s/file-%d.jpg", path, node_no);
	if (filename != NULL) {
		sprintf(real_name, "%s/%s", path, filename);
	}
	int output_fd = open(file_path_buf, O_WRONLY|O_CREAT|O_TRUNC,S_IRUSR|S_IWUSR);	
	for (int i = 0; i < EXT2_NDIR_BLOCKS && inode->i_block[i] != 0; i++) {
		char buffer[block_size];
		read_block(fd, 0, 0, inode->i_block[i], buffer);
		if (inode->i_size < (i+1)*block_size) {
			write(output_fd, buffer, inode->i_size - i*block_size);
		}
		else {
			write(output_fd, buffer, block_size);
		}
	}
	if (inode->i_block[EXT2_IND_BLOCK] != 0) {
		char ind_block[block_size];
		read_block(fd, 0, 0, inode->i_block[EXT2_IND_BLOCK], ind_block);
		copy_indirect_block(fd, output_fd, inode, ind_block, 0);
	}
	if (inode->i_block[EXT2_DIND_BLOCK] != 0) {
		char dind_block[block_size];
		read_block(fd, 0, 0, inode->i_block[EXT2_DIND_BLOCK], dind_block);
		for (int i = 0; i < blocks_per_block && ((int*)dind_block)[i] != 0; i++) {
			char ind_block[block_size];
			read_block(fd, 0, 0, ((int*)dind_block)[i], ind_block);
			copy_indirect_block(fd, output_fd, inode, ind_block, i+1);
		}
	}
	close(output_fd);
	copy_fread(file_path_buf, real_name);
}




int get_inode(char* entry) {
	return ((int*)entry)[0];
}

char *get_next_entry(char* buffer, char* cur_entry) {
	if (cur_entry == NULL) {
		return buffer;
	}
	short name_len = ((__u8*)cur_entry)[6];
	short offset = (name_len%4) ? name_len - name_len % 4 + 4 + 8 : name_len +8;
	char* next_entry = cur_entry + offset;
	if (next_entry - buffer >= block_size || get_inode(next_entry) == 0) {
		return NULL;
	}
	return next_entry;
}

void get_name(char* buffer, char* entry, char *name) {
	short offset = ((short*)entry)[2];
	short name_len = ((__u8*)entry)[6];
	// printf("name_len: %d\n", name_len);
	if (entry - buffer + offset >= block_size) {
		strncpy(name, (const char *)(entry+8), name_len);
		name[name_len] = '\0';
	}
	else {
		strcpy(name, (const char *)(entry+8));
	}
	name[name_len] = '\0';
}

void read_name_indir(int fd, struct ext2_inode *inode, struct ext2_group_desc *group, char *outpath) {
	char buffer[block_size];
	read_block(fd, 0, 0, inode->i_block[0], buffer);
	off_t start_inode_table = locate_inode_table(0, group);
	char *entry = NULL;
	for (entry = get_next_entry(buffer, entry); entry != NULL; entry = get_next_entry(buffer, entry)) {
		int inode_no = get_inode(entry);
		struct ext2_inode *filenode = malloc(sizeof(struct ext2_inode));
		read_inode(fd, 0, start_inode_table, inode_no, filenode);
		if (S_ISREG(filenode->i_mode) && is_jpg(fd, filenode)) {
			char name[MAX_PATHNAME_LEN];
			get_name(buffer, entry, name);
			copy_file(fd, filenode, outpath, inode_no, name);
			// printf("%s\n", name);
		}
		free(filenode);
	}
}


int main(int argc, char **argv) {
	if (argc != 3) {
		printf("expected usage: ./runscan inputfile outputfile\n");
		exit(0);
	}
	
	int fd;
	char *output_path = argv[2];
	DIR *path = opendir(output_path);

	fd = open(argv[1], O_RDONLY);    /* open disk image */
	if (path != NULL) {
		printf("error, directory already existed. \n");
		// FILE *fp = fopen("output-image/file-13.jpg", "w");
		// if (fp == NULL) {
		// 	printf("Write failed\n");
		// }
		// else {
		// 	fclose(fp);
		// }
		exit(1);
	}

	if (mkdir((const char *)output_path, S_IRWXG)) {
		printf("create directory failed\n");
		exit(1);
	}
	path = opendir(output_path);
	ext2_read_init(fd);

	struct ext2_super_block super;
	struct ext2_group_desc group;
	
	// example read first the super-block and group-descriptor
	read_super_block(fd, 0, &super);
	read_group_desc(fd, 0, &group);
	
	printf("There are %u inodes in an inode table block and %u blocks in the idnode table\n", inodes_per_block, itable_blocks);
	//iterate the first inode block
	off_t start_inode_table = locate_inode_table(0, &group);
	// off_t start_block = locate_data_blocks(0, &group);
	// int file_num = 0;
	// int inode_num = 0;
	// int jpg_num = 0;
    for (unsigned int i = 0; i < super.s_inodes_per_group; i++) {
            // printf("inode %u: \n", i);
		struct ext2_inode *inode = malloc(sizeof(struct ext2_inode));
		read_inode(fd, 0, start_inode_table, i, inode);
		// if (S_ISREG(inode->i_mode)) {
		// 	// is_jpg(fd, inode, start_block);
		// 	if (is_jpg(fd, inode)) {
		// 		copy_file(fd, inode, output_path, i, NULL);
		// 		jpg_num++;
		// 	}
		// 	file_num++;
		// }
		if (S_ISDIR(inode->i_mode)) {
			read_name_indir(fd, inode, &group, output_path);
		}
		// if (S_ISDIR(inode->i_mode)) {
		// 	printf("inode: %d \n", i);
		// 	for(unsigned int i=0; i<EXT2_N_BLOCKS; i++)
		// 	{       if (i < EXT2_NDIR_BLOCKS)                                 /* direct blocks */
		// 					printf("Block %2u : %u\n", i, inode->i_block[i]);
		// 			else if (i == EXT2_IND_BLOCK)                             /* single indirect block */
		// 					printf("Single   : %u\n", inode->i_block[i]);
		// 			else if (i == EXT2_DIND_BLOCK)                            /* double indirect block */
		// 					printf("Double   : %u\n", inode->i_block[i]);
		// 			else if (i == EXT2_TIND_BLOCK)                            /* triple indirect block */
		// 					printf("Triple   : %u\n", inode->i_block[i]);	
		// 	}
		// }
		// inode_num++;
	// /* the maximum index of the i_block array should be computed from i_blocks / ((1024<<s_log_block_size)/512)
	// 	 * or once simplified, i_blocks/(2<<s_log_block_size)
	// 	 * https://www.nongnu.org/ext2-doc/ext2.html#i-blocks
	// 	 */
		
		free(inode);

    }
	// char buffer[1024];
	
	// struct ext2_inode *inode = malloc(sizeof(struct ext2_inode));
    // read_inode(fd, 0, start_inode_table, 11, inode);
	// printf("Is directory? %s \n Is Regular file? %s\n",
    //             S_ISDIR(inode->i_mode) ? "true" : "false",
    //             S_ISREG(inode->i_mode) ? "true" : "false");
	// for(unsigned int i=0; i<EXT2_N_BLOCKS; i++)
	// {       if (i < EXT2_NDIR_BLOCKS)                                 /* direct blocks */
	// 				printf("Block %2u : %u\n", i, inode->i_block[i]);
	// 		else if (i == EXT2_IND_BLOCK)                             /* single indirect block */
	// 				printf("Single   : %u\n", inode->i_block[i]);
	// 		else if (i == EXT2_DIND_BLOCK)                            /* double indirect block */
	// 				printf("Double   : %u\n", inode->i_block[i]);
	// 		else if (i == EXT2_TIND_BLOCK)                            /* triple indirect block */
	// 				printf("Triple   : %u\n", inode->i_block[i]);	
	// }
	// read_block(fd, 0, start_inode_table, inode->i_block[0], buffer);
	// char *entry = NULL;
	// entry = get_next_entry(buffer, NULL);
	// entry = get_next_entry(buffer, entry);
	// entry = get_next_entry(buffer, entry);
	// entry = get_next_entry(buffer, entry);
	// entry = get_next_entry(buffer, entry);
	// entry = get_next_entry(buffer, entry);
	// entry = entry -4;
	// char *entry = NULL;
	// for (entry = get_next_entry(buffer, entry); entry != NULL; entry = get_next_entry(buffer, entry)) {
	// 	char name[255];
	// 	get_name(buffer, entry, name);
	// 	printf("%s\n", name);
	// }
	
	
	// char file_path_buf[512];
	// sprintf(file_path_buf, "%s/file-%d.jpg", output_path, 13);
	// int output_fd = open(file_path_buf, O_WRONLY|O_CREAT|O_TRUNC,S_IRUSR|S_IWUSR);
	// write(output_fd, buffer, inode->i_size);
	// close(output_fd);
	// free(inode);
	close(fd);
	// printf("jpg_num: %d\n", jpg_num);
	// printf("file_num: %d\n", file_num);
}
