### Build configuration ###

# C compiler used to compile kernel C code
# Compiler built by xbstrap is in its build directory
CC = gcc

# Assembler used to assemble kernel asm files
AS = nasm

# Linker used to generate kernel binary from object files
# We use gcc itself for linking
# TODO: replace with x86_64-ricerca-gcc once we have xbstrap config
LD = $(CC)

# Common (used both in debug and release mode) C flags, 
# that are essential for compiling kernel code correctly
# -c - create object file
# -mgeneral-regs-only - do not generate code
# that uses extended registers (87 fpu, MMX, SSE, SSE2)
# The reason for those flags is that using those CPU extensions
# requires us to enable them explicitely, and we dont do that yet
# -mno-red-zone - do not assume red zone 
# -mcmodel=kernel - instruct gcc that kernel will be located
# in negative 2 gigabytes of the virtual address space
# -ffrestanding - do not assume running in standard C library
# environment (which bare metal does not offer)
# -fno-stack-protector - disable stack protection checks.
# We don't enable them, as this feature requires runtime support we dont have yet.
# -fno-pic - do not generate position independent code. Hosted compilers
# usually enable it by default, as they generate ASLR compatible
# executables
# -nostdlib - do not link with any standard libraries (including libgcc and libc)
C_HARD_FLAGS := -c -mgeneral-regs-only -mno-red-zone -mcmodel=kernel \
		-ffreestanding -nostdlib -fno-stack-protector -fno-pic

# Common C flags, that are not essential for correct
# compilation, but useful anyways. Value of this variable can be changed from the command line
# -Wall -Werror -Wextra - guard against simple mistakes using GCC diagnostics capabilities
CFLAGS = -Wall -Werror -Wextra

# C flags used for release builds
# -flto - enable link time optimization
# -O2 - optimize for speed
C_RELEASE_FLAGS = -O2 -flto

# C flags used for debug builds
# -g - generate debugging symbols
C_DEBUG_FLAGS = -g -DDEBUG

# All C flags used in release mode
C_RELEASE_FLAGS_ALL := $(C_RELEASE_FLAGS) $(CFLAGS) $(C_HARD_FLAGS)

# All C flags used in debug mode
C_DEBUG_FLAGS_ALL := $(C_DEBUG_FLAGS) $(CFLAGS) $(C_HARD_FLAGS)

# Common linker flags (used both in debug and release mode), that are
# essential for correct kernel linkage
# -nostdlib - do not link to C standard library
# -no-pie - do not link as position independent executable. Hosted linker
# usually enables PIE by default to make output ASLR compatible
LD_HARD_FLAGS := -nostdlib -no-pie

# Common linker flags, thare are not essential, but useful anyways.
# Value of this variable can be changed from the command line
# -z max-page-size=0x1000 - align sections to only 4kb
LDFLAGS = -z max-page-size=0x1000

# Release LD flags
LD_RELEASE_FLAGS = -flto

# Debug LD flags
LD_DEBUG_FLAGS = 

# All release LD flags
LD_RELEASE_FLAGS_ALL := $(LD_RELEASE_FLAGS) $(LDFLAGS) $(LD_HARD_FLAGS)

# All debug LD flags
LD_DEBUG_FLAGS_ALL := $(LD_DEBUG_FLAGS) $(LDFLAGS) $(LD_HARD_FLAGS)

# Path to linker script
LD_SCRIPT = link.ld

# Assembly flags (same for debug and release)
ASFLAGS := -f elf64

# Destination directory
DESTDIR = .

# Release kernel binary output path
RELEASE_PATH = $(DESTDIR)/ricerca-kernel.elf

# Debug kernel binary output path
DEBUG_PATH = $(DESTDIR)/ricerca-kernel.elf

### Source files and object lists ###

# List of all C source files for the kernel
C_SOURCES = $(shell find . -type f -name '*.c')

# List of all Assembly files for the kernel
AS_SOURCES = $(shell find . -type f -name '*.asm')

# List of object files created from C files in debug mode
C_DEBUG_OBJS = $(C_SOURCES:.c=.c.debug.o)

# List of object files created from C files in release mode
C_RELEASE_OBJS =$(C_SOURCES:.c=.c.release.o)

# List of object files from assembly sources
AS_OBJS = $(AS_SOURCES:.asm=.asm.o)

# List of all object files in the debug mode
DEBUG_OBJS = $(C_DEBUG_OBJS) $(AS_OBJS)

# List of all object files in the release mode
RELEASE_OBJS = $(C_RELEASE_OBJS) $(AS_OBJS)

### Rules for assembling/compiling files ###

# Release C code compilation rule
# This rule matches all dependencies names that end with .c.release.o.
# This extension is used by C object files in release mode
# Extension ".c.release.o" is substituted with ".c" to get source file name
%.c.release.o: %.c
# Compile C file
# $(C_RELEASE_FLAGS_ALL) - C release mode compiler flags
# "$@" - name of the output of this rule ("%.c.release.o"). In this case,
# its simply name of the object file that we should build in this rule
# "$<" - name of the first (and in this case, the only one) dependency. In this case,
# its simply name of the C file we are building
# "-I." - add current directory as include path (so that we can include files from this directory)
	$(CC) $(C_RELEASE_FLAGS_ALL) -o $@ $< -I.

# Debug C code compilation rule
# This rule matches all dependencies names that end with .c.debug.o.
# This extension is used by C object files in debug mode
# Extension ".c.debug.o" is substituted with ".c" to get source file name
%.c.debug.o: %.c
# Compile C file
# $(C_DEBUG_FLAGS_ALL) - C debug mode compiler flags
# "$@" - name of the output of this rule ("%.c.debug.o"). In this case,
# its simply name of the object file that we should build in this rule
# "$<" - name of the first (and in this case, the only one) dependency. In this case,
# its simply name of the C file we are building
# "-I." - add current directory as include path (so that we can include files from this directory)
	$(CC) $(C_DEBUG_FLAGS_ALL) -o $@ $< -I.

# Assembly code rule. Produces object files from asm files
# This rule matches all dependencies names that end with .asm.o.
# This extension is used by ASM object files
# Extension ".asm.o" is substituted with ".asm" to get source file name
%.asm.o: %.asm
# Assemble .asm file
# $(AS_FLAGS) - Flags passed to assembler
# "$<" - name of the first (and in this case, the only one) dependency. In this case,
# its simply name of the asm file to be assembled
# "$@" - name of the output of this rule ("%.asm.o"). In this case,
# its simply name of the object file that we should build in this rule
	$(AS) $(AS_FLAGS) -o $@ $<

# Object clean rule that can be used to delete 

### Make commands ###

# List of defined commands
.PHONY: release debug obj_clean clean

# Release build rule (links kernel from object files)
release: $(RELEASE_OBJS)
# Link kernel
# "-T $(LD_SCRIPT)" - use LDSCRIPT as a linker script
# "-o $(RELEASE_PATH)" - output kernel binary to RELEASEPATH
# "$(LD_RELEASE_FLAGS_ALL)" - use LD release flags (as defined in "Build configuration" section)
# "$^" - list of dependencies, which is equal to $(release_objs) in this case
# Supplies a list of objects that needs to be linked to the linker
	$(LD) -T $(LD_SCRIPT) -o $(RELEASE_PATH) $(LD_RELEASE_FLAGS_ALL) $^

# Debug build rule (links kernel from object files)
debug: $(DEBUG_OBJS)
# Link kernel
# "-T $(LD_SCRIPT)" - use LDSCRIPT as a linker script
# "-o $(DEBUG_PATH)" - output kernel binary to DEBUGPATH
# "$(LD_DEBUG_FLAGS_ALL)" - use LD debug flags (as defined in "Build configuration" section)
# "$^" - list of dependencies, which is equal to $(debug_objs) in this case
# Supplies a list of objects that needs to be linked to the linker
	$(LD) -T $(LD_SCRIPT) -o $(DEBUG_PATH) $(LD_DEBUG_FLAGS_ALL) $^

# Object clean rule that deletes all object files, but keeps kernel binaries
obj_clean:
	rm -f $(RELEASE_OBJS) $(DEBUG_OBJS)

# Clean rule that cleans everything, along with kernel binaries
clean: obj_clean
	rm -f $(RELEASE_PATH) $(DEBUG_PATH)
