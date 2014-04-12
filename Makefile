CC = clang
LD = $(CC)
CFLAGS = 
LDFLAGS = 

FILES = main.c
OBJ_FILES = $(FILES:.c=.o)

all: mach_prot

mach_prot: $(OBJ_FILES)
	$(LD) $(LDFLAGS) $< -o $@
	rm ./*.o

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm *.o