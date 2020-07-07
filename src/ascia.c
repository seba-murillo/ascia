/*
 ============================================================================
 Name        : Ascia.c
 Author      : Seba Murillo
 Version     : 1.0
 Copyright   : GPL
 Description : file splitter and joiner
 ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <openssl/md5.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>

#define TAB "    "
#define BUFFER_SIZE 10240  // 10 KB
#define MIN_BLOCKSIZE 1024 // 1 KB
#define MIN_FILESIZE 10240 // 10 Kb
#define MAX_FILENAME 256

//#define verbose

void split(const char* filename, uint32_t block_size);
void join(char* filename);
void show_syntax();
char* get_MD5(const char* target);

typedef struct _metadata{
	uint32_t parts;
	char MD5[32];
} metadata;

// ascia <split/join> <filename> <block_size>
int main(int argc, char** argv){
	if(argc < 3) show_syntax();
	if(strcmp(argv[1], "split") == 0){
		if(argc != 4) show_syntax();
		int32_t block_size = strtol(argv[3], NULL, 10);
		if(block_size < MIN_BLOCKSIZE){
			printf("> block sizes lower than %d bytes are not allowed\n", MIN_BLOCKSIZE);
			exit(EXIT_FAILURE);
		}
		split(argv[2], (uint32_t) block_size);
	}else if(strcmp(argv[1], "join") == 0){
		if(argc != 3) show_syntax();
		join(argv[2]);
	}else show_syntax();
	return EXIT_SUCCESS;
}

void split(const char* filename, uint32_t block_size){
	FILE* file = fopen(filename, "r");
	if(file == NULL){
		fprintf(stderr, "ERROR: could not open [%s] (%s)\n", filename, strerror(errno));
		exit(EXIT_FAILURE);
	}
	// get file size
	fseek(file, 0, SEEK_END);
	uint64_t size = ftell(file);
	fseek(file, 0, SEEK_SET);
	if(size < MIN_FILESIZE){
		printf("> file sizes lower than %d bytes are not allowed\n", MIN_FILESIZE);
		exit(EXIT_FAILURE);
	}
	// calculate metadata
	metadata meta;
	meta.parts = size / (block_size - sizeof(metadata));
	uint32_t remainder = size % (block_size - sizeof(metadata));
	uint32_t block_reads = (block_size - sizeof(metadata)) / BUFFER_SIZE;
	uint32_t block_remainder = (block_size - sizeof(metadata)) % BUFFER_SIZE;
	if(remainder != 0) meta.parts++;
	strcpy(meta.MD5, get_MD5(filename));
#ifdef verbose
	printf("> splitting: %s\n", filename);
	printf(TAB "%-20s%lu\n", "file size:", size);
	printf(TAB "%-20s%s\n", "file MD5:", meta.MD5);
	printf(TAB "%-20s%u\n", "meta.parts:", meta.parts);
	printf(TAB "%-20s%u\n", "remainder:", remainder);
	printf(TAB "%-20s%u\n", "block_size:", block_size);
	printf(TAB "%-20s%d\n", "BUFFER_SIZE:", BUFFER_SIZE);
	printf(TAB "%-20s%u\n", "block_reads:", block_reads);
	printf(TAB "%-20s%u\n", "block_remainder:", block_remainder);
#endif
	uint32_t i = 1;
	uint32_t R, W;
	char output_filename[MAX_FILENAME];
	char buffer[BUFFER_SIZE];
	// start split
	for(;i < meta.parts;i++){
		sprintf(output_filename, "%s.part%d", filename, i);
		FILE* out = fopen(output_filename, "w");
		if(file == NULL){
			fprintf(stderr, "ERROR: could not write to [%s] (%s)\n", output_filename, strerror(errno));
			exit(EXIT_FAILURE);
		}
		// write metadata
		W = fwrite(&meta.parts, 1, sizeof(meta.parts), out);
		W = fwrite(meta.MD5, 1, sizeof(meta.MD5), out);
		// write data
		for(uint32_t j = 0;j < block_reads;j++){
			R = fread(&buffer, sizeof(char), BUFFER_SIZE, file);
			if(R != BUFFER_SIZE){
				fprintf(stderr, "ERROR: reading [%s] (%s)\n", output_filename, strerror(errno));
				exit(EXIT_FAILURE);
			}
			W = fwrite(buffer, sizeof(char), BUFFER_SIZE, out);
			if(W != BUFFER_SIZE){
				fprintf(stderr, "ERROR: writing [%s] (%s)\n", output_filename, strerror(errno));
				exit(EXIT_FAILURE);
			}
		}
		// write remainder
		if(block_remainder != 0){
			R = fread(&buffer, sizeof(char), block_remainder, file);
			if(R != block_remainder){
				fprintf(stderr, "ERROR: reading [%s] (%s)\n", output_filename, strerror(errno));
				exit(EXIT_FAILURE);
			}
			W = fwrite(buffer, sizeof(char), block_remainder, out);
			if(W != block_remainder){
				fprintf(stderr, "ERROR: writing [%s] (%s)\n", output_filename, strerror(errno));
				exit(EXIT_FAILURE);
			}
		}
		fclose(out);
	}
	// last part
	sprintf(output_filename, "%s.part%d", filename, i);
	FILE* out = fopen(output_filename, "w");
	if(file == NULL){
		fprintf(stderr, "ERROR: could not write to [%s] (%s)\n", output_filename, strerror(errno));
		exit(EXIT_FAILURE);
	}
	// write metadata
	W = fwrite(&meta.parts, 1, sizeof(meta.parts), out);
	W = fwrite(meta.MD5, 1, sizeof(meta.MD5), out);
	// write data
	while((R = fread(&buffer, sizeof(char), BUFFER_SIZE, file)) > 0){
		W = fwrite(buffer, sizeof(char), R, out);
	}
	fclose(out);
	fclose(file);
}

void join(char* filename){
	char* ext = strrchr(filename, '.');
	if(strncmp(ext, ".part", sizeof(".part")) == 0){
		filename[ext - filename] = '\0'; // remove extension
	}
	char filename_full[MAX_FILENAME];
	sprintf(filename_full, "%s_rejoined", filename);
	FILE* file_full = fopen(filename_full, "w");
	if(file_full == NULL){
		fprintf(stderr, "ERROR: could not open [%s] (%s)\n", filename, strerror(errno));
		exit(EXIT_FAILURE);
	}
	metadata meta;
	uint32_t i = 1, R;
	char filename_part[MAX_FILENAME];
	char buffer[BUFFER_SIZE];
	do{
		sprintf(filename_part, "%s.part%d", filename, i);
		FILE* file_part = fopen(filename_part, "r");
		if(file_part == NULL){
			fprintf(stderr, "ERROR: could not open [%s] (%s)\n", filename_part, strerror(errno));
			exit(EXIT_FAILURE);
		}
		// read metadata
		R = fread(&meta, 1, sizeof(meta), file_part);
		if(R != sizeof(meta)){
			fprintf(stderr, "ERROR: reading [%s] (%s)\n", filename_part, strerror(errno));
			exit(EXIT_FAILURE);
		}
		// read part data
		while((R = fread(&buffer, sizeof(char), BUFFER_SIZE, file_part)) > 0){
			fwrite(buffer, sizeof(char), R, file_full);
		}
		fclose(file_part);
		i++;
	}while(i < meta.parts + 1);
	fclose(file_full);
	// check MD5
	if(strcmp(meta.MD5, get_MD5(filename_full)) != 0){
		fprintf(stderr, "ERROR: MD5 of resulting file doesn't match\n");
		exit(EXIT_FAILURE);
	}
}

char* get_MD5(const char* target){
	int FD = open(target, O_RDONLY);
	if(FD < 0){
		fprintf(stderr, "ERROR: opening %s for MD5 hash (%s)\n", target, strerror(errno));
		exit(EXIT_FAILURE);
	}
	// get size
	struct stat stat_struct;
	if(stat(target, &stat_struct) != 0){
		fprintf(stderr, "ERROR: reading file size (%s)\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	// init MD5
	unsigned char MD5[MD5_DIGEST_LENGTH];
	char buffer[BUFFER_SIZE];
	MD5_CTX CTX;
	uint32_t reads = stat_struct.st_size / BUFFER_SIZE;
	uint32_t R;
	MD5_Init(&CTX);
	// calculate MD5
	for(int i = 0;i < reads;i++){
		R = read(FD, buffer, BUFFER_SIZE);
		if(R < 0){
			fprintf(stderr, "ERROR: reading %s for MD5 hash (%s)\n", target, strerror(errno));
			return NULL;
		}
		MD5_Update(&CTX, buffer, R);
	}
	if(stat_struct.st_size % BUFFER_SIZE != 0){
		R = read(FD, buffer, stat_struct.st_size % BUFFER_SIZE);
		if(R < 0){
			fprintf(stderr, "ERROR: reading %s for MD5 hash (%s)\n", target, strerror(errno));
			return NULL;
		}
		MD5_Update(&CTX, buffer, R);
	}
	MD5_Final(MD5, &CTX);
	close(FD);
	// transform to string
	char* result = calloc(2 * MD5_DIGEST_LENGTH, sizeof(char));
	char tmp[10];
	for(int i = 0;i < MD5_DIGEST_LENGTH;i++){
		sprintf(tmp, "%02x", MD5[i]);
		strcat(result, tmp);
	}
	return result;
}

void show_syntax(){
	printf("> incorrect syntax, use:\n");
	printf(TAB "ascia [split] [filename] [block size]\n");
	printf(TAB "ascia [join] [filename]\n");
	exit(EXIT_SUCCESS);
}
