# ricercaOS nightly release (version $VERSION)

This release contains images that were built automatically on the Github Actions CI.

There are 4 images attached

- `ricerca-release.iso` - ricercaOS image optimized for speed (kernel is built with -O2 and LTO enabled). Assertions are disabled
- `ricerca-debug.iso` - ricercaOS image with debug symbols and no optimizations. Used for debugging
- `ricerca-safe.iso` - similar to `ricerca-release.iso`, but assertions are enabled. Good candidate for testing on the real hardware
- `ricerca-profile.iso` - for now ignored, in the future this image will be used for kernel profiling

You can run release image with the following bash command

```bash
qemu-system-x86_64 -cdrom ricerca-release.iso -smp 8 -debugcon stdio -no-shutdown -no-reboot --enable-kvm
```

Similar command would work for the `ricerca-safe.iso` image

```bash
qemu-system-x86_64 -cdrom ricerca-safe.iso -smp 8 -debugcon stdio -no-shutdown -no-reboot --enable-kvm
```

See the root `Makefile` to figure out how to run other images.

## Checksums

### SHA1 checksums

```
$SHA1_INFO
```

### SHA256 checksums

```
$SHA256_INFO
```

### SHA512 checksums

```
$SHA512_INFO
```

### MD5 checksums

```
$MD5_INFO
```
