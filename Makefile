CC = i686-elf-gcc
LD = i686-elf-ld
OBJCOPY = i686-elf-objcopy
CFLAGS = -ffreestanding -O2 -Wall -Wextra

OBJS = build/entry.o \
       build/isr.o \
       build/irq.o \
       build/idt.o \
       build/isr_c.o \
       build/irq_c.o \
       build/kernel.o \
       build/keyboard.o \
       build/shell.o \
       build/ata.o \
       build/mm.o \
	   build/fat32.o 


all:
	mkdir -p build
	nasm -f bin boot.asm -o build/boot.bin
	nasm -f elf32 kernel_entry.asm -o build/entry.o
	nasm -f elf32 isr.asm -o build/isr.o
	nasm -f elf32 irq.asm -o build/irq.o
	$(CC) -c kernel.c -o build/kernel.o $(CFLAGS)
	$(CC) -c idt.c -o build/idt.o $(CFLAGS)
	$(CC) -c isr.c -o build/isr_c.o $(CFLAGS)
	$(CC) -c irq.c -o build/irq_c.o $(CFLAGS)
	$(CC) -c keyboard.c -o build/keyboard.o $(CFLAGS)
	$(CC) -c shell.c -o build/shell.o $(CFLAGS)
	$(CC) -c ata.c -o build/ata.o $(CFLAGS)
	$(CC) -c fat32.c -o build/fat32.o $(CFLAGS)
	$(CC) -c mm.c -o build/mm.o $(CFLAGS)
	$(LD) -T linker.ld $(OBJS) -o build/kernel.elf -nostdlib
	$(OBJCOPY) -O binary build/kernel.elf build/kernel.bin
	cat build/boot.bin build/kernel.bin > build/disk.img
	dd if=/dev/zero bs=1M count=6 >> build/disk.img

mkfs:
	dd if=/dev/zero of=build/fat32.img bs=1M count=16
	mkfs.fat -F 32 build/fat32.img
	cp build/disk.img build/padded.img
	truncate -s $$((20480*512)) build/padded.img
	cat build/padded.img build/fat32.img > build/final.img



run:
	qemu-system-i386 -drive format=raw,file=build/final.img,if=ide -m 32 -no-reboot

clean:
	rm -rf build