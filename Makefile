# List of defined commands
# build rule is not to be used by the user, however it is included in this list to make makefile
# evaluate it
.PHONY: build-debug build-release clean kernel-clean run-release run-debug run-safe run-ci

# Machine to test on
MACHINE=numa-distances

# Rule for xbstrap init
build/bootstrap.link:
	mkdir -p build
	cd build && xbstrap init .

# Build image with release kernel
ricerca-release.iso: KERNEL_PKG = "release-kernel"
ricerca-release.iso: IMAGE = "ricerca-release.iso"
ricerca-release.iso: image

# Build image with debug kernel
ricerca-debug.iso: KERNEL_PKG = "debug-kernel"
ricerca-debug.iso: IMAGE = "ricerca-debug.iso"
ricerca-debug.iso: image

# Generic image build rule
image: build/bootstrap.link
# Install limine
	cd build && xbstrap install-tool limine
# Reinstall system files
	cd build && xbstrap install system-files --rebuild
# Reinstall kernel
	cd build && xbstrap install $(KERNEL_PKG) --rebuild
# Reinstall limine files
	cd build && xbstrap install limine-cd --rebuild
# Delete existing image
	rm -f $(IMAGE)
# Create image with xorriso
	xorriso -as mkisofs -b boot/limine-cd.bin \
	-no-emul-boot -boot-load-size 4 -boot-info-table \
	--efi-boot boot/limine-eltorito-efi.bin \
	-efi-boot-part --efi-boot-image --protective-msdos-label \
	build/system-root -o $(IMAGE)
# Install limine for legacy BIOS boot
	build/build-tools/limine/limine-install $(IMAGE)

# Clean rule
# Deletes kernel object files, kernel binaries, and images
clean:
	make -C kernel clean
	rm -rf ricerca-debug.iso ricerca-release.iso

# Binary clean rule
# Clears only kenrel binaries and object files
kernel-clean:
	make -C src/kernel clean

# Run release image rule
# Runs OS in QEMU with kvm enabled
run-release: ricerca-release.iso
# Run QEMU
# -hda ricerca-release.iso - Attach hard drive with our image
# -cpu host --accel kvm --accel hax --accel tcg - Use hardware virtualization if available
# If no hardware acceleration is present, fallback to tcg with --accel tcg
# -debugcon stdio - Add debug connection, so that we can print logs to e9 port from the kernel
# and see them in the terminal
# -no-shutdown -no-reboot - Halt on fatal errrors
	qemu-system-x86_64 \
	-hda ricerca-release.iso \
	-debugcon stdio \
	-no-shutdown -no-reboot \
	`cat machines/$(MACHINE) | tr '\n' ' '`

# Run safe image rule
# Runs debug image without waiting for debugger
# Run QEMU
# -hda ricerca-debug.iso - Attach hard drive with our image
# -cpu host --accel kvm --accel hax --accel tcg - Use hardware virtualization if available
# If no hardware acceleration is present, fallback to tcg with --accel tcg
# -debugcon stdio - Add debug connection, so that we can print logs to e9 port from the kernel
# and see them in the terminal
# -no-shutdown -no-reboot - Halt on fatal errrors
run-safe: ricerca-debug.iso
	qemu-system-x86_64 \
	-hda ricerca-debug.iso \
	-debugcon stdio \
	-no-shutdown -no-reboot \
	`cat machines/$(MACHINE) | tr '\n' ' '`

# Run image on CI rule
# Runs release image on CI
# Run QEMU
# -hda ricerca-debug.iso - Attach hard drive with our image
# If no hardware acceleration is present, fallback to tcg with --accel tcg
# -debugcon stdio - Add debug connection, so that we can print logs to e9 port from the kernel
# -display none - Do not actually open the window
# and see them in the terminal
# -no-shutdown -no-reboot - Halt on fatal errrors
run-ci: ricerca-debug.iso
	qemu-system-x86_64 \
	-hda ricerca-debug.iso \
	-debugcon stdio \
	-display none \
	-no-shutdown -no-reboot \
	`cat machines/$(MACHINE) | tr '\n' ' '`


# Run debug image rule
run-debug: ricerca-debug.iso
# Run QEMU
# -hda ricerca-debug.iso - Attach harddrive with our image
# If no hardware acceleration is present, fallback to tcg with --accel tcg
# -debugcon stdio - Add debug connection, so that we can print logs to e9 port from the kernel
# and see them in the terminal
# -S -s - attach and wait for the debugger
# -no-shutdown -no-reboot - Halt on fatal errrors
	echo `cat machines/$(MACHINE) | tr '\n' ' '`
	qemu-system-x86_64 \
	-hda ricerca-debug.iso \
	-debugcon stdio \
	-S -s \
	-no-shutdown -no-reboot \
	`cat machines/$(MACHINE) | tr '\n' ' '`

# Attach GDB to running session
gdb-attach:
	gdb -x gdb-startup

# Release build rule
build-release: ricerca-release.iso

# Debug build rule
build-debug: ricerca-debug.iso
