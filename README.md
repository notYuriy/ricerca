# Ricerca

ricerca-os fork that I am using as a playground to experiment with SMP, NUMA, load balancing, and memory/CPU hotplug

# Building

Install `python3`, `fuse`, `rsync`, `qemu-system-x86_64` and `build-essential` packages.

Install xbstrap from pip: `pip3 install xbstrap`.

Run `make build-release`/`make build-debug` in root directory to build release image (OS image with kernel built in release mode) and image for debugging respectively.

Run `make run-release` to run release image in QEMU

Run `make run-debug` to run debug image in QEMU. QEMU will wait for debugger to be attached. Run `make gdb-attach` in a separate terminal to start GDB and attach it to QEMU.
