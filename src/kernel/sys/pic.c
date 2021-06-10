//! @file pic.c
//! @brief  File containing PIC functions definitions

#include <lib/log.h>
#include <sys/pic.h>
#include <sys/ports.h>

TARGET(pic_remap_target, pic_remap, {})

//! @brief Wait for IO completition
static void iowait(void) {
	outb(0x80, 0x00);
}

//! @brief Remap PIC
static void pic_remap(void) {
	outb(0x20, 0x11);
	iowait();
	outb(0xa0, 0x11);
	iowait();
	outb(0x21, 0x20);
	iowait();
	outb(0xa1, 0x28);
	iowait();
	outb(0x21, 0x04);
	iowait();
	outb(0xa1, 0x02);
	iowait();
	outb(0x21, 0x01);
	iowait();
	outb(0xa1, 0x01);
	iowait();
	outb(0x21, 0xff);
	iowait();
	outb(0xa1, 0xff);
	iowait();
}
