#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <mach-o/loader.h>


void usage(){
	puts("Usage: mach_prot <filename> <segname> [rwx]");
}

void mach_prot_64(char *cmds, char *segname, struct mach_header_64 header, vm_prot_t maxprot){
	bool wrote_prot = false;

	/* Loop through load commands */
	uint8_t *address = (uint8_t*)cmds;
	for(int i = 0; i < header.ncmds; i++){
		struct load_command *command = (struct load_command*)address;

		if(command->cmd == LC_SEGMENT_64){
			struct segment_command_64 *seg_cmd = (struct segment_command_64*)command;

			if(strcmp(seg_cmd->segname, segname) == 0){
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

void mach_prot(char *cmds, char *segname, struct mach_header header, vm_prot_t maxprot){
	bool wrote_prot = false;

	/* Loop through load commands */
	uint8_t *address = (uint8_t*)cmds;
	for(int i = 0; i < header.ncmds; i++){
		struct load_command *command = (struct load_command*)address;

		if(command->cmd == LC_SEGMENT){
			struct segment_command *seg_cmd = (struct segment_command*)command;

			if(strcmp(seg_cmd->segname, segname) == 0){
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
	vm_prot_t maxprot;

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

	FILE *fd = fopen(argv[1], "r+");

	if(fd == NULL){
		fprintf(stderr, "Error opening %s.\n", argv[1]);
		exit(1);
	}

	/* Get the mach header */
	struct mach_header header;
	struct mach_header_64 header64;

	fread(&header.magic, sizeof(header.magic), 1, fd);
	fseek(fd, 0, SEEK_SET);

	bool bits64 = false;

	if(header.magic == MH_MAGIC_64)
		bits64 = true;

	if(!bits64)
		fread(&header, sizeof(header), 1, fd);
	else
		fread(&header64, sizeof(header64), 1, fd);

	/* Read load commands into buffer */
	void *cmds = malloc(bits64 ? header64.sizeofcmds : header.sizeofcmds);
	fread(cmds, bits64 ? header64.sizeofcmds : header.sizeofcmds, 1, fd);

	if(bits64)
		mach_prot_64(cmds, argv[2], header64, maxprot);
	else
		mach_prot(cmds, argv[2], header, maxprot);

	/* Write load commands back to file */
	fseek(fd, bits64 ? sizeof(struct mach_header_64) : sizeof(struct mach_header), SEEK_SET);
	fwrite(cmds, bits64 ? header64.sizeofcmds : header.sizeofcmds, 1, fd);

	return 0;
}