format ELF64

public gdtr_apply
public gdt_ltr_48h

section '.text' executable

; 64-bit code segment
CODE64=0x28

; 64-bit data segment
DATA64=0x30

; Load GDT. rdi=GDTR address
gdtr_apply:
    lgdt [rdi]
    ; Load CS
    push qword CODE64
	push qword .cs_flush
	retf
.cs_flush:
    ; Load other segment regs
    mov ax, DATA64
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax
	mov ss, ax
	ret

; Load TR from segment 0x48
gdt_ltr_48h:
	mov ax, 0x48
	ltr ax
	ret
