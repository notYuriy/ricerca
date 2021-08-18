# List of defined commands
# build rule is not to be used by the user, however it is included in this list to make makefile
# evaluate it
.PHONY: build-debug build-release clean kernel-clean run-release run-debug run-safe run-ci

# Use timestamp of the commit to ensure that builds are reproducible
export SOURCE_DATE_EPOCH=$(shell git show -s --format=%ct)
XORRISO_DATE=$(shell git log -1 --format="%at" | xargs -I{} date -d @{} +%Y%m%d%H%M%S00)

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

# Build image with safe kernel
ricerca-safe.iso: KERNEL_PKG = "safe-kernel"
ricerca-safe.iso: IMAGE = "ricerca-safe.iso"
ricerca-safe.iso: image

# Build image with profile kernel
ricerca-profile.iso: KERNEL_PKG = "profile-kernel"
ricerca-profile.iso: IMAGE = "ricerca-profile.iso"
ricerca-profile.iso: image

# Generic image build rule
image: build/bootstrap.link
# Clean sysroot
	rm -rf build/system-root/*
# Install limine
	cd build && xbstrap install-tool limine
# Reinstall system files
	cd build && xbstrap install system-files --rebuild
# Reinstall kernel
	cd build && xbstrap install $(KERNEL_PKG) --rebuild
# Reinstall limine CD files
	cd build && xbstrap install limine-cd --rebuild
# Add limine files for network boot
	cd build && xbstrap install limine-pxe --rebuild
# Delete existing image
	rm -f $(IMAGE)
# Create image with xorriso
	echo $(SOURCE_DATE_EPOCH)
	xorriso -volume_date "all_file_dates" $(XORRISO_DATE) \
	-as mkisofs -b boot/limine-cd.bin \
	-no-emul-boot -boot-load-size 4 -boot-info-table \
	--efi-boot boot/limine-eltorito-efi.bin \
	-efi-boot-part --efi-boot-image --protective-msdos-label \
	build/system-root -o $(IMAGE)
# Install limine for legacy BIOS boot
	build/build-tools/limine/limine-install $(IMAGE)

# Clean rule
# Deletes kernel object files, kernel binaries, and images
clean:
	make -C src/kernel clean
	rm -rf ricerca-debug.iso ricerca-release.iso ricerca-safe.iso ricerca-profile.iso

# Binary clean rule
# Clears only kenrel binaries and object files
kernel-clean:
	make -C src/kernel clean

# Run release image in QEMU with KVM enabled
run-release-kvm: ricerca-release.iso
	qemu-system-x86_64 \
	-cdrom ricerca-release.iso \
	-debugcon file:e9.txt \
	-no-shutdown -no-reboot \
	--enable-kvm \
	`cat machines/$(MACHINE) | tr '\n' ' '`

# Run safe image in QEMU with KVM enabled
run-safe-kvm: ricerca-safe.iso
	qemu-system-x86_64 \
	-cdrom ricerca-safe.iso \
	-debugcon file:e9.txt \
	-no-shutdown -no-reboot \
	--enable-kvm \
	`cat machines/$(MACHINE) | tr '\n' ' '`

# Run release image in QEMU without KVM enabled
run-release-tcg: ricerca-release.iso
	qemu-system-x86_64 \
	-cdrom ricerca-release.iso \
	-debugcon file:e9.txt \
	-no-shutdown -no-reboot \
	`cat machines/$(MACHINE) | tr '\n' ' '`

# Run safe image in QEMU without KVM enabled
run-safe-tcg: ricerca-safe.iso
	qemu-system-x86_64 \
	-cdrom ricerca-safe.iso \
	-debugcon file:e9.txt \
	-no-shutdown -no-reboot \
	`cat machines/$(MACHINE) | tr '\n' ' '`

# Run profiling kernel build in QEMU with KVM enabled
profile-kvm: ricerca-profile.iso
	qemu-system-x86_64 \
	-cdrom ricerca-profile.iso \
	-debugcon file:profiling/dump \
	-no-shutdown -no-reboot \
	--enable-kvm \
	`cat machines/$(MACHINE) | tr '\n' ' '`
	python3 profile/process.py

# Run profiling kernel build in QEMU without KVM enabled
profile-tcg: ricerca-profile.iso
	qemu-system-x86_64 \
	-cdrom ricerca-profile.iso \
	-debugcon file:profiling/dump \
	-no-shutdown -no-reboot \
	`cat machines/$(MACHINE) | tr '\n' ' '`
	python3 profile/process.py

# Run image on CI rule.
# Image runs in TCG with 2 minute timeout
run-ci: ricerca-safe.iso
	qemu-system-x86_64 \
	-cdrom ricerca-safe.iso \
	-debugcon file:e9.txt \
	-display none \
	-no-shutdown -no-reboot \
	`cat machines/$(MACHINE) | tr '\n' ' '`

# Start QEMU with debug image and wait for debugger to attach
debug: ricerca-debug.iso
	qemu-system-x86_64 \
	-cdrom ricerca-debug.iso \
	-debugcon file:e9.txt \
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

# Safe build rule
build-safe: ricerca-safe.iso

# Profile build rule
build-profile: ricerca-profile.iso

# Find symbol by address rule
addr2line:
	addr2line -e build/system-root/boot/ricerca-kernel.elf $(SYMBOL)

# List kernel symbols
nm:
	nm -n build/system-root/boot/ricerca-kernel.elf
