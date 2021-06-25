format ELF64

public gdtr_apply
public gdt_ltr_48h

section '.text' executable

IA32_GS_MSR = 0xC0000101

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
	; It's important that we preserve GS hidden part (it used for per CPU data access), so we read 
	; this hidden part from IA32_GS_MSR to write it back later
	mov ecx, IA32_GS_MSR
	rdmsr
	mov esi, eax
    ; Load other segment regs
    mov ax, DATA64
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax
	mov ss, ax
	; Load hidden GS part back
	mov eax, esi
	wrmsr
	ret

; Load TR from segment 0x48
gdt_ltr_48h:
	mov ax, 0x48
	ltr ax
	ret
