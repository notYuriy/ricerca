format ELF64

; GDT descripor
struc gdt_descriptor base, limit, executable, sz, long, dpl
{
	dw limit and 0xffff
	dw (base and 0xffff)
	db (base shr 16) and 0xff
	db ((executable shl 3) or 010b) or (1001b shl 4) or (dpl shl 5)
	db ((limit shr 16) and 0xf) or 10000000b or (long shl 5) or (sz shl 6)
	db ((base shr 24) and 0xff)
}

public gdt_load

section '.rodata'
; GDT descriptors
gdt:
; NULL descriptor
.null dq 0
; 16 bit code
.code16_descr gdt_descriptor 0, 0xfffff, 1, 0, 0, 0
; 16 bit data
.data16_descr gdt_descriptor 0, 0xfffff, 0, 0, 0, 0
; 32 bit code
.code32_descr gdt_descriptor 0, 0xfffff, 1, 1, 0, 0
; 32 bit data
.data32_descr gdt_descriptor 0, 0xfffff, 0, 1, 0, 0
; 64 bit code
.code64 = ($ - gdt)
.code64_descr gdt_descriptor 0, 0xfffff, 1, 0, 1, 0
; 64 bit data
.data64 = ($ - gdt)
.data64_descr gdt_descriptor 0, 0xfffff, 0, 0, 1, 0
; 64 bit code (user)
.user_code64_descr gdt_descriptor 0, 0xfffff, 1, 0, 1, 3
; 64 bit data (user)
.user_data64_descr gdt_descriptor 0, 0xfffff, 0, 0, 1, 3
; GDT pointer
.pointer:
	dw gdt.pointer - gdt - 1
	dq gdt

section '.text' executable
; Load GDT
gdt_load:
	; Load GDT from GDTR
	lgdt [gdt.pointer]
	; Flush CS with far jump
	push qword gdt.code64
	push qword .cs_flush
	retf
.cs_flush:
	; Flush other regs
	mov ax, gdt.data64
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax
	mov ss, ax
	ret
