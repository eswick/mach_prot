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
	puts("Usage: mach_prot <filename> <segname> [rwx]");
}

void mach_prot_64(struct mach_header_64 *header, char *segname, vm_prot_t maxprot){
	bool wrote_prot = false;

	/* Loop through load commands */
	uint8_t *address = (uint8_t*)header + sizeof(struct mach_header_64);
	for(int i = 0; i < header->ncmds; i++){
		struct load_command *command = (struct load_command*)address;

		if(command->cmd == LC_SEGMENT_64){
			struct segment_command_64 *seg_cmd = (struct segment_command_64*)command;

			if(strcmp(seg_cmd->segname, segname) == 0){

				/* Write new segment maxprot */
				seg_cmd->maxprot = maxprot;
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

void mach_prot(struct mach_header *header, char *segname, vm_prot_t maxprot){
	bool wrote_prot = false;

	/* Loop through load commands */
	uint8_t *address = (uint8_t*)header + sizeof(struct mach_header);
	for(int i = 0; i < header->ncmds; i++){
		struct load_command *command = (struct load_command*)address;

		if(command->cmd == LC_SEGMENT){
			struct segment_command *seg_cmd = (struct segment_command*)command;

			if(strcmp(seg_cmd->segname, segname) == 0){

				/* Write new segment maxprot */
				seg_cmd->maxprot = maxprot;
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

	if(argc < 4){
		usage();
		exit(0);
	}

	/* Calculate maxprot */
	vm_prot_t maxprot = 0;

	char *protstring = argv[3];
	while(*protstring != '\0'){

		switch(*protstring){
			case 'r': maxprot |= VM_PROT_READ; break;
			case 'w': maxprot |= VM_PROT_WRITE; break;
			case 'x': maxprot |= VM_PROT_EXECUTE; break;
			default: usage(); exit(0);
		}

		protstring++;
	}

	/* Open file */
	struct stat stat_buf;
	int fd = open(argv[1], O_RDWR, 0);

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
				mach_prot_64((struct mach_header_64*)(offset + (uint8_t*)mh), argv[2], maxprot);
			else
				/* Slice is 32 bit */
				mach_prot((struct mach_header*)(offset + (uint8_t*)mh), argv[2], maxprot);
		}
	}else if( mh->magic == MH_MAGIC_64 ){
		mach_prot_64((struct mach_header_64*)mh, argv[2], maxprot);
	}else if( mh->magic == MH_MAGIC ){
		mach_prot((struct mach_header*)mh, argv[2], maxprot);
	}else{
		fprintf(stderr, "Invalid file!\n");
		exit(1);
	}

	return 0;
}