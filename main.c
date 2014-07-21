/* The MIT License (MIT)

Copyright (c) 2014 Evan Swick

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE. */

#import <sys/types.h>
#import <sys/stat.h>
#import <sys/mman.h>
#import <stdio.h>
#import <fcntl.h>
#import <stdlib.h>
#import <unistd.h>
#import <stdbool.h>
#import <string.h>

#include <mach-o/loader.h>
#include <mach-o/fat.h>

void usage(){
	puts("Usage: mach_prot <init/max> <filename> <segname> [rwx]");
}

typedef enum {
  PROT_INIT = 1 << 0,
  PROT_MAX = 1 << 1
} PROT_TYPE;

void mach_prot_64(struct mach_header_64 *header, char *segname, vm_prot_t prot, PROT_TYPE prot_type){
	bool wrote_prot = false;

	/* Loop through load commands */
	uint8_t *address = (uint8_t*)header + sizeof(struct mach_header_64);
	for(int i = 0; i < header->ncmds; i++){
		struct load_command *command = (struct load_command*)address;

		if(command->cmd == LC_SEGMENT_64){
			struct segment_command_64 *seg_cmd = (struct segment_command_64*)command;

			if(strcmp(seg_cmd->segname, segname) == 0){

				/* Write new segment prots */
        if (prot_type & PROT_MAX)
          seg_cmd->maxprot = prot;
        if (prot_type & PROT_INIT)
          seg_cmd->initprot = prot;
        
				wrote_prot = true;
			}

			seg_cmd++;
		}

		address += command->cmdsize;
	}

	if(!wrote_prot){
		fprintf(stderr, "Segment %s not found.\n", segname);
		exit(1);
	}
}

void mach_prot(struct mach_header *header, char *segname, vm_prot_t prot, PROT_TYPE prot_type){
	bool wrote_prot = false;

	/* Loop through load commands */
	uint8_t *address = (uint8_t*)header + sizeof(struct mach_header);
	for(int i = 0; i < header->ncmds; i++){
		struct load_command *command = (struct load_command*)address;

		if(command->cmd == LC_SEGMENT){
			struct segment_command *seg_cmd = (struct segment_command*)command;

			if(strcmp(seg_cmd->segname, segname) == 0){

				/* Write new segment prots */
        if (prot_type & PROT_MAX)
          seg_cmd->maxprot = prot;
        if (prot_type & PROT_INIT)
          seg_cmd->initprot = prot;
        
				wrote_prot = true;
			}

			seg_cmd++;
		}

		address += command->cmdsize;
	}

	if(!wrote_prot){
		fprintf(stderr, "Segment %s not found.\n", segname);
		exit(1);
	}
}

int main(int argc, char *argv[]){

	if(argc < 5){
		usage();
		exit(0);
	}
  
  PROT_TYPE prottype = 0;
  
  /* Determine prot type */
  if (strcmp(argv[1], "init") == 0)
    prottype = PROT_INIT;
  else if (strcmp(argv[1], "max") == 0)
    prottype = PROT_MAX;
  else {
    fprintf(stderr, "Unrecognized prot type '%s'\n", argv[1]);
    exit(1);
  }
    

	/* Calculate prot */
	vm_prot_t prot = 0;

	char *protstring = argv[4];
	while(*protstring != '\0'){

		switch(*protstring){
			case 'r': prot |= VM_PROT_READ; break;
			case 'w': prot |= VM_PROT_WRITE; break;
			case 'x': prot |= VM_PROT_EXECUTE; break;
			default: usage(); exit(0);
		}

		protstring++;
	}

	/* Open file */
	struct stat stat_buf;
	int fd = open(argv[2], O_RDWR, 0);

	if( fd == -1 )
		perror("Cannot open file"), exit(1);
	if( fstat(fd, &stat_buf) != 0 )
		perror("fstat failed"), exit(1);
	uint32_t length = stat_buf.st_size;

	uint8_t *p = (uint8_t*)mmap(NULL, stat_buf.st_size, PROT_READ | PROT_WRITE, MAP_FILE | MAP_SHARED, fd, 0);
	if( p == ((uint8_t*)(-1)) )
		perror("cannot map file"), exit(1);
	close(fd);
	
	/* Call proper methods */
	const struct mach_header* mh = (struct mach_header*)p;

	/* Check if binary is thin/fat and 64/32 bit */

	if ( mh->magic == OSSwapBigToHostInt32(FAT_MAGIC) ) {
		const struct fat_header* fh = (struct fat_header*)p;
		const struct fat_arch* archs = (struct fat_arch*)(p + sizeof(struct fat_header));

		for (unsigned long i = 0; i < OSSwapBigToHostInt32(fh->nfat_arch); i++) {
			size_t offset = OSSwapBigToHostInt32(archs[i].offset);
			cpu_type_t cputype = OSSwapBigToHostInt32(archs[i].cputype);

			if(cputype & CPU_ARCH_ABI64)
				/* Slice is 64 bit */
				mach_prot_64((struct mach_header_64*)(offset + (uint8_t*)mh), argv[3], prot, prottype);
			else
				/* Slice is 32 bit */
				mach_prot((struct mach_header*)(offset + (uint8_t*)mh), argv[3], prot, prottype);
		}
	}else if( mh->magic == MH_MAGIC_64 ){
		mach_prot_64((struct mach_header_64*)mh, argv[3], prot, prottype);
	}else if( mh->magic == MH_MAGIC ){
		mach_prot((struct mach_header*)mh, argv[3], prot, prottype);
	}else{
		fprintf(stderr, "Invalid file!\n");
		exit(1);
	}

	return 0;
}