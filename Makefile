# List of defined commands
# build rule is not to be used by the user, however it is included in this list to make makefile
# evaluate it
.PHONY: build-debug build-release clean kernel-clean run-release run-debug run-safe

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
	rm -rf ricerca-debug.hdd ricerca-release.hdd

# Binary clean rule
# Clears only kenrel binaries and object files
kernel-clean:
	make -C src/kernel clean

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
	-m size=4G,slots=4,maxmem=8G \
	-no-shutdown -no-reboot \
	-machine q35 \
	-object memory-backend-ram,size=1G,id=m0 \
    -object memory-backend-ram,size=1G,id=m1 \
    -object memory-backend-ram,size=1G,id=m2 \
    -object memory-backend-ram,size=1G,id=m3 \
	-smp cpus=16,maxcpus=32 \
	-numa node,cpus=0-7,nodeid=0,memdev=m0 \
	-numa node,cpus=8-15,nodeid=1,memdev=m1 \
	-numa node,cpus=16-23,nodeid=2,memdev=m2 \
	-numa node,cpus=24-31,nodeid=3,memdev=m3 \
	-numa dist,src=0,dst=1,val=15 \
	-numa dist,src=2,dst=3,val=15 \
	-numa dist,src=0,dst=2,val=20 \
	-numa dist,src=0,dst=3,val=20 \
	-numa dist,src=1,dst=2,val=20 \
	-numa dist,src=1,dst=3,val=20

# Run safe image rule
# Runs debug image without waiting for debugger
# Run QEMU
# -hda ricerca-debug.hdd - Attach hard drive with our image
# -cpu host --accel kvm --accel hax --accel tcg - Use hardware virtualization if available
# If no hardware acceleration is present, fallback to tcg with --accel tcg
# -debugcon stdio - Add debug connection, so that we can print logs to e9 port from the kernel
# and see them in the terminal
# -machine q35 - Use modern hw, duh
# -m size=1G,slots=4,maxmem=4G - 1 gigabyte of RAM on boot + 4 slots to hotplug up to 4 GB of ram
# -no-shutdown -no-reboot - Halt on fatal errrors
# SMP configuration is taken from https://futurewei-cloud.github.io/ARM-Datacenter/qemu/how-to-configure-qemu-numa-nodes/
run-safe: ricerca-debug.hdd
	qemu-system-x86_64 \
	-hda ricerca-debug.hdd \
	--enable-kvm -cpu host \
	-debugcon stdio \
	-m size=4G,slots=4,maxmem=8G \
	-no-shutdown -no-reboot \
	-machine q35 \
	-object memory-backend-ram,size=1G,id=m0 \
    -object memory-backend-ram,size=1G,id=m1 \
    -object memory-backend-ram,size=1G,id=m2 \
    -object memory-backend-ram,size=1G,id=m3 \
	-smp cpus=16,maxcpus=32 \
	-numa node,cpus=0-7,nodeid=0,memdev=m0 \
	-numa node,cpus=8-15,nodeid=1,memdev=m1 \
	-numa node,cpus=16-23,nodeid=2,memdev=m2 \
	-numa node,cpus=24-31,nodeid=3,memdev=m3 \
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
	-debugcon stdio \
	-m size=4G,slots=4,maxmem=8G \
	-S -s \
	-no-shutdown -no-reboot \
	-machine q35 \
	-object memory-backend-ram,size=1G,id=m0 \
    -object memory-backend-ram,size=1G,id=m1 \
    -object memory-backend-ram,size=1G,id=m2 \
    -object memory-backend-ram,size=1G,id=m3 \
	-smp cpus=16,maxcpus=32 \
	-numa node,cpus=0-7,nodeid=0,memdev=m0 \
	-numa node,cpus=8-15,nodeid=1,memdev=m1 \
	-numa node,cpus=16-23,nodeid=2,memdev=m2 \
	-numa node,cpus=24-31,nodeid=3,memdev=m3 \
	-numa dist,src=0,dst=1,val=15 \
	-numa dist,src=2,dst=3,val=15 \
	-numa dist,src=0,dst=2,val=20 \
	-numa dist,src=0,dst=3,val=20 \
	-numa dist,src=1,dst=2,val=20 \
	-numa dist,src=1,dst=3,val=20
# Attach GDB to running session
gdb-attach:
	gdb -x gdb-startup

# Release build rule
build-release: ricerca-release.hdd

# Debug build rule
build-debug: ricerca-debug.hdd
