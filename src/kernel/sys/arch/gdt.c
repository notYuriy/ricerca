//! @file gdt.c
//! @brief File containing implementations of global descriptor table functions

#include <misc/attributes.h>
#include <misc/types.h>
#include <sys/arch/gdt.h>

//! @brief Fill GDT segment descriptor
//! @param seg Pointer to the segment
//! @param base Segment base
//! @param limit Segment limit
//! @param exe 1 if segment is executable
//! @param sz 1 if size bit should be set
//! @param l 1 if L bit should be set
//! @param dpl Maximum privelege level
void gdt_fill_seg(uint64_t *seg, uint32_t base, uint32_t limit, bool exe, bool sz, bool l,
                  uint8_t dpl) {
	uint16_t exei = exe ? 1 : 0;
	uint16_t szi = sz ? 1 : 0;
	uint16_t li = l ? 1 : 0;

	uint64_t limit_low = (uint64_t)(limit & 0xffff);
	uint64_t base_low = (uint64_t)(base & 0xffff);
	uint64_t base_mid = (uint64_t)((base >> 16) & 0xff);
	uint64_t access = (uint64_t)((exei << 3) | 0b010 | (0b1001 << 4) | (dpl << 5));
	uint64_t flags = (uint64_t)(((limit >> 16) & 0xf) | 0b10000000 | (li << 5) | (szi << 6));
	uint64_t base_high = (uint64_t)((base >> 24) & 0xff);

	*seg = limit_low | (base_low << 16) | (base_mid << 32) | (access << 40) | (flags << 48) |
	       (base_high << 56);
}

//! @brief Apply GDT
//! @param gdtr Pointer to GDTR
void gdtr_apply(void *gdtr);

//! @brief Initialize and load GDT
//! @param gdt Pointer to uninitialized GDT
void gdt_init(struct gdt *gdt) {
	// Fill GDT descriptors
	gdt->descrs[0] = 0;                                                // NULL
	gdt_fill_seg(gdt->descrs + 1, 0, 0xfffff, true, false, false, 0);  // 16-bit kernel code
	gdt_fill_seg(gdt->descrs + 2, 0, 0xfffff, false, false, false, 0); // 16-bit kernel data
	gdt_fill_seg(gdt->descrs + 3, 0, 0xfffff, true, true, false, 0);   // 32-bit kernel code
	gdt_fill_seg(gdt->descrs + 4, 0, 0xfffff, false, true, false, 0);  // 32-bit kernel data
	gdt_fill_seg(gdt->descrs + 5, 0, 0xfffff, true, false, true, 0);   // 64-bit kernel code
	gdt_fill_seg(gdt->descrs + 6, 0, 0xfffff, false, false, true, 0);  // 64-bit kernel data
	gdt_fill_seg(gdt->descrs + 7, 0, 0xfffff, true, false, true, 3);   // 64-bit user code
	gdt_fill_seg(gdt->descrs + 8, 0, 0xfffff, false, false, true, 3);  // 64-bit user data
	gdt->descrs[9] = 0;                                                // Reserved for TSS
	gdt->descrs[10] = 0;                                               // Reserved for TSS

	// Load GDT
	struct {
		uint16_t length;
		uint64_t base;
	} attribute_packed gdtr = {GDT_DESCRIPTORS * 8 - 1, (uint64_t)gdt->descrs};
	gdtr_apply(&gdtr);
}

//! @brief Load TSS
//! @param gdt Pointer to the GDT
//! @param tss Pointer to the TSS
void gdt_load_tss(struct gdt *gdt, struct tss *tss) {
	// 1001
	gdt->descrs[9] = (((uint64_t)(sizeof(struct tss)) - 1) & 0xffffULL) |
	                 ((((uintptr_t)tss) & 0xffffffULL) << 16) | (0b1001ULL << 40) | (1ULL << 47) |
	                 (((((uintptr_t)tss) >> 24) & 0xffULL) << 56);
	gdt->descrs[10] = ((uintptr_t)tss) >> 32;
	asm volatile("ltr %0" ::"r"((uint16_t)(9 * 8)));
}
