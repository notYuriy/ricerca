# List of defined commands
# build rule is not to be used by the user, however it is included in this list to make makefile
# evaluate it
.PHONY: build-debug build-release clean kernel-clean run-release

# Rule for xbstrap init
build/bootstrap.link:
	mkdir -p build
	cd build && xbstrap init .

# Build image with release kernel
ricerca-release.hdd: KERNEL_PKG = "release-kernel"
ricerca-release.hdd: IMAGE = "ricerca-release.hdd"
ricerca-release.hdd: image

# Build image with debug kernel
ricerca-debug.hdd: KERNEL_PKG = "debug-kernel"
ricerca-debug.hdd: IMAGE = "ricerca-debug.hdd"
ricerca-debug.hdd: image

# Generic image build rule
image: build/bootstrap.link
# Install echfs utils
	cd build && xbstrap install-tool echfs
# Reinstall system files
	cd build && xbstrap install system-files --rebuild
# Reinstall kernel
	cd build && xbstrap install $(KERNEL_PKG) --rebuild
# Reinstall limine stage 3
	cd build && xbstrap install limine-stage3 --rebuild
# Delete existing image
	rm -rf $(IMAGE)
# Make a new image with 64 Megabytes of disk space. seek is used to allocate space without actually
# filling it with zeroes.
	dd if=/dev/zero bs=1M count=0 seek=64 of=$(IMAGE)
# Create GPT partition table on disk
	parted -s $(IMAGE) mklabel gpt
# Create one partition that will be used for the kernel
	parted -s $(IMAGE) mkpart primary 2048s 100%
# Format this partition to echfs filesystem
	./build/build-tools/echfs/echfs-utils -g -p0 $(IMAGE) quick-format 512
# Delete mountpoint target.
	rm -rf build/sysroot-mount
	mkdir -p build/sysroot-mount
	./build/build-tools/echfs/echfs-fuse --gpt -p0 $(IMAGE) build/sysroot-mount
# Copy system root to echfs
	cp -r build/system-root/* build/sysroot-mount/
# Unmount echfs
	fusermount -z -u build/sysroot-mount
# Remove mountpoint
	rm -rf build/sysroot-mount/
# Install limine on the resulting image
	./build/build-tools/limine/limine-install $(IMAGE)

# Clean rule
# Deletes kernel object files, kernel binaries, and images
clean:
	make -C kernel clean
	rm -rf ricerca-debug.hdd ricerca-release.hdd

# Binary clean rule
# Clears only kenrel binaries and object files
kernel-clean:
	make -C kernel clean

# Run release image rule
# Runs OS in QEMU with kvm enabled
run-release: ricerca-release.hdd
# Run QEMU
# -hda ricerca-release.hdd - Attach hard drive with our image
# -cpu host --accel kvm --accel hax --accel tcg - Use hardware virtualization if available
# If no hardware acceleration is present, fallback to tcg with --accel tcg
# -debugcon stdio - Add debug connection, so that we can print logs to e9 port from the kernel
# and see them in the terminal
# -machine q35 - Use modern hw, duh
# -m size=1G,slots=4,maxmem=4G - 1 gigabyte of RAM on boot + 4 slots to hotplug up to 4 GB of ram
# -no-shutdown -no-reboot - Halt on fatal errrors
# SMP configuration is taken from https://futurewei-cloud.github.io/ARM-Datacenter/qemu/how-to-configure-qemu-numa-nodes/
	qemu-system-x86_64 \
	-hda ricerca-release.hdd \
	--enable-kvm -cpu host \
	-debugcon stdio \
	-m size=1G,slots=4,maxmem=4G \
	-no-shutdown -no-reboot \
	-smp cpus=16 -numa node,cpus=0-3,nodeid=0 \
	-numa node,cpus=4-7,nodeid=1 \
	-numa node,cpus=8-11,nodeid=2 \
	-numa node,cpus=12-15,nodeid=3 \
	-numa dist,src=0,dst=1,val=15 \
	-numa dist,src=2,dst=3,val=15 \
	-numa dist,src=0,dst=2,val=20 \
	-numa dist,src=0,dst=3,val=20 \
	-numa dist,src=1,dst=2,val=20 \
	-numa dist,src=1,dst=3,val=20

# Run debug image rule
run-debug: ricerca-debug.hdd
# Run QEMU
# -hda ricerca-debug.hdd - Attach harddrive with our image
# -cpu host --accel kvm --accel hax --accel tcg - Use hardware virtualization if available
# If no hardware acceleration is present, fallback to tcg with --accel tcg
# -debugcon stdio - Add debug connection, so that we can print logs to e9 port from the kernel
# and see them in the terminal
# -S -s - attach and wait for the debugger
# -machine q35 - Use modern hw, duh
# -m size=1G,slots=4,maxmem=4G - 1 gigabyte of RAM on boot + 4 slots to hotplug up to 4 GB of ram
# -no-shutdown -no-reboot - Halt on fatal errrors
# SMP configuration is taken from https://futurewei-cloud.github.io/ARM-Datacenter/qemu/how-to-configure-qemu-numa-nodes/
	qemu-system-x86_64 \
	-hda ricerca-debug.hdd \
	--enable-kvm -cpu host \
	-debugcon stdio \
	-S -s \
	-m size=1G,slots=4,maxmem=4G \
	-no-shutdown -no-reboot \
	-smp cpus=16 -numa node,cpus=0-3,nodeid=0 \
	-numa node,cpus=4-7,nodeid=1 \
	-numa node,cpus=8-11,nodeid=2 \
	-numa node,cpus=12-15,nodeid=3 \
	-numa dist,src=0,dst=1,val=15 \
	-numa dist,src=2,dst=3,val=15 \
	-numa dist,src=0,dst=2,val=20 \
	-numa dist,src=0,dst=3,val=20 \
	-numa dist,src=1,dst=2,val=20 \
	-numa dist,src=1,dst=3,val=20

# Attach GDB to running session
gdb-attach:
	gdb -x gdb-startup
