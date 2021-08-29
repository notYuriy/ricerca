### Build configuration. LD, CC, and AS variables should be provided by the caller ###

# Common (used both in debug and release mode) C flags, 
# that are essential for compiling kernel code correctly
# -c - create object file
# -mgeneral-regs-only - do not generate code
# that uses extended registers (87 fpu, MMX, SSE, SSE2)
# The reason for those flags is that using those CPU extensions
# requires us to enable them explicitely, and we dont do that yet
# -mno-red-zone - do not assume red zone
# -ffrestanding - do not assume running in standard C library
# environment (which bare metal does not offer)
# We don't enable them, as this feature requires runtime support we dont have yet.
# -nostdlib - do not link with any standard libraries (including libgcc and libc)
# "-I." - add current directory as include path (so that we can include files from this directory)
# -fpie - build position independent executable
C_HARD_FLAGS := -c -mgeneral-regs-only -mno-red-zone \
		-ffreestanding -nostdlib  -I. -fpie

# Common C flags, that are not essential for correct
# compilation, but useful anyways. Value of this variable can be changed from the command line
# -Wall -Werror -Wextra - guard against simple mistakes using GCC diagnostics capabilities
CFLAGS = -Wall -Werror -Wextra

# C flags used for release builds
# -O3 - optimize for speed
# -fno-stack-protector - Disable stack protector
# -flto - Enable link time optimization
C_RELEASE_FLAGS = -O3 -fno-stack-protector -flto

# C flags used for debug builds
# -g - generate debugging symbols
# -DDEBUG - define DEBUG macro to check assertions
# -fstack-protector-all - Enable stack protector
# -fsanitize-undefined - Enable UB sanitizer
C_DEBUG_FLAGS = -g -DDEBUG -fstack-protector-all -fsanitize=undefined

# C flags used for safe builds
# -g - generate debugging symbols
# -O2 - optimize for spped
# -DDEBUG - define DEBUG macro to check assertions
# -fstack-protector-all - Enable stack protector
# -fsanitize-undefined - Enable UB sanitizer
C_SAFE_FLAGS = -g -O2 -DDEBUG -fstack-protector -fsanitize=undefined

# C flags used for profiled builds
# -g - generate debugging symbols
# -O2 - optimize for speed
# -pg - Add calls to mcount on every function call
# -DPROFILE - Hint for kernel logger to disable logging to e9 (e9 is used for profiling logging)
# -fstack-protector-all - Enable stack protector
# -fsanitize-undefined - Enable UB sanitizer
C_PROFILE_FLAGS = -O2 -g -pg -DPROFILE -fstack-protector -fsanitize=undefined

# Common linker flags (used both in debug and release mode), that are
# essential for correct kernel linkage
# -nostdlib - do not link to C standard library
# -static-pie -Wl,--no-dynamic-linker - build static position independent binary
LD_HARD_FLAGS := -nostdlib -static-pie -Wl,--no-dynamic-linker

# Common linker flags, thare are not essential, but useful anyways.
# Value of this variable can be changed from the command line
# -z max-page-size=0x1000 - align sections to only 4kb
LDFLAGS = -z max-page-size=0x1000

# Release LD flags
LD_RELEASE_FLAGS = -flto

# Debug LD flags
LD_DEBUG_FLAGS = -fstack-protector -fsanitize=undefined

# Safe LD flags
LD_SAFE_FLAGS = -fstack-protector -fsanitize=undefined

# Profile LD flags
LD_PROFILE_FLAGS = -fstack-protector -fsanitize=undefined

# Path to linker script
LD_SCRIPT = link.ld

# Destination directory
DESTDIR = .

# Release kernel binary output path
OUTPUT_PATH = $(DESTDIR)/ricerca-kernel.elf

### Source files and object lists ###

# List of all C source files for the kernel
C_SOURCES = $(shell find . -type f -name '*.c')

# List of all Assembly files for the kernel
AS_SOURCES = $(shell find . -type f -name '*.asm')

# List of object files created from C files in debug mode
C_DEBUG_OBJS = $(C_SOURCES:.c=.c.debug.o)

# List of object files created from C files in release mode
C_RELEASE_OBJS =$(C_SOURCES:.c=.c.release.o)

# List of object files created from C files in safe mode
C_SAFE_OBJS =$(C_SOURCES:.c=.c.safe.o)

# List of object files created from C files in profile mode
C_PROFILE_OBJS =$(C_SOURCES:.c=.c.profile.o)

# List of object files from assembly sources
AS_OBJS = $(AS_SOURCES:.asm=.asm.o)

### Rules for assembling/compiling files ###

# C code compilation rules.
%.c.release.o: %.c
	$(CC) $(CFLAGS) $(C_HARD_FLAGS) $(C_RELEASE_FLAGS) -o $@ $<

%.c.debug.o: %.c
	$(CC) $(CFLAGS) $(C_HARD_FLAGS) $(C_DEBUG_FLAGS) -o $@ $<

%.c.safe.o: %.c
	$(CC) $(CFLAGS) $(C_HARD_FLAGS) $(C_SAFE_FLAGS) -o $@ $<

%.c.profile.o: %.c
	$(CC) $(CFLAGS) $(C_HARD_FLAGS) $(C_PROFILE_FLAGS) -o $@ $<


# Assembly code rule.
%.asm.o: %.asm
# its simply name of the object file that we should build in this rule
	$(AS) $< $@

### Make commands ###

# List of defined commands
.PHONY: release debug safe profile obj_clean clean

# Release kernel build rule
release: $(C_RELEASE_OBJS) $(AS_OBJS)
	$(LD) -T $(LD_SCRIPT) -o $(OUTPUT_PATH) $(LD_HARD_FLAGS) $(LDFLAGS) $(LD_RELEASE_FLAGS) $^

# Debug kernel build rule
debug: $(C_DEBUG_OBJS) $(AS_OBJS)
	$(LD) -T $(LD_SCRIPT) -o $(OUTPUT_PATH) $(LD_HARD_FLAGS) $(LDFLAGS) $(LD_DEBUG_FLAGS) $^

# Safe kernel build rule
safe: $(C_SAFE_OBJS) $(AS_OBJS)
	$(LD) -T $(LD_SCRIPT) -o $(OUTPUT_PATH) $(LD_HARD_FLAGS) $(LDFLAGS) $(LD_SAFE_FLAGS) $^

# Profile kernel build rule
profile: $(C_PROFILE_OBJS) $(AS_OBJS)
	$(LD) -T $(LD_SCRIPT) -o $(OUTPUT_PATH) $(LD_HARD_FLAGS) $(LDFLAGS) $(LD_PROFILE_FLAGS) $^

# Clean object files
obj_clean:
	rm -f $(shell find . -type f -name '*.o')

# Clean rule that cleans everything, along with kernel binaries
clean: obj_clean
	rm -f $(RELEASE_PATH) $(DEBUG_PATH)
