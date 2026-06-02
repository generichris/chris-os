CC      = i686-elf-gcc
LD      = i686-elf-ld
OBJCOPY = i686-elf-objcopy

CFLAGS  = -ffreestanding -O2 -Wall -Wextra \
          -Ikernel -Idrivers -Ifs -Iui -Ishell -Inet

OBJS = build/entry.o       \
       build/isr_asm.o     \
       build/irq_asm.o     \
       build/idt.o         \
       build/isr_c.o       \
       build/irq_c.o       \
       build/kernel.o      \
       build/mm.o          \
       build/sched_c.o     \
       build/sched_asm.o   \
       build/serial.o      \
       build/kprintf.o     \
       build/panic.o       \
       build/syscall.o     \
       build/syscall_asm.o \
       build/keyboard.o    \
       build/mouse.o       \
       build/ata.o         \
       build/vesa.o        \
       build/fat32.o       \
       build/elf.o         \
       build/installer.o   \
       build/draw.o        \
       build/font.o        \
       build/gterm.o       \
       build/shell.o       \
       build/eth.o         \
       build/netstack.o    \
       build/arp.o         \
       build/ipv4.o        \
       build/icmp.o        \
       build/udp.o         \
       build/tcp.o         \
       build/dns.o         \
       build/http.o        \
       build/dhcp.o

all: build/boot.bin $(OBJS)
	$(LD) -T kernel/linker.ld $(OBJS) -o build/kernel.elf -nostdlib
	$(OBJCOPY) -O binary build/kernel.elf build/kernel.bin
	@SECTORS=$$(( ($$(stat -c%s build/kernel.bin) + 511) / 512 )); \
	 echo "Kernel: $$(stat -c%s build/kernel.bin) bytes = $$SECTORS sectors"; \
	 if [ $$SECTORS -gt 128 ]; then echo "WARNING: kernel >64KB"; fi
	cat build/boot.bin build/kernel.bin > build/disk.img
	dd if=/dev/zero bs=1M count=6 >> build/disk.img 2>/dev/null

build/boot.bin: boot.asm | build
	nasm -f bin $< -o $@

build/entry.o: kernel/kernel_entry.asm | build
	nasm -f elf32 $< -o $@

build/isr_asm.o: kernel/isr.asm | build
	nasm -f elf32 $< -o $@

build/irq_asm.o: kernel/irq.asm | build
	nasm -f elf32 $< -o $@

build/sched_asm.o: kernel/sched.asm | build
	nasm -f elf32 $< -o $@

build/syscall_asm.o: kernel/syscall_entry.asm | build
	nasm -f elf32 $< -o $@

build/idt.o:       kernel/idt.c       | build
	$(CC) -c $< -o $@ $(CFLAGS)

build/isr_c.o:     kernel/isr.c       | build
	$(CC) -c $< -o $@ $(CFLAGS)

build/irq_c.o:     kernel/irq.c       | build
	$(CC) -c $< -o $@ $(CFLAGS)

build/kernel.o:    kernel/kernel.c    | build
	$(CC) -c $< -o $@ $(CFLAGS)

build/mm.o:        kernel/mm.c        | build
	$(CC) -c $< -o $@ $(CFLAGS)

build/sched_c.o:   kernel/sched.c     | build
	$(CC) -c $< -o $@ $(CFLAGS)

build/serial.o:    drivers/serial.c   | build
	$(CC) -c $< -o $@ $(CFLAGS)

build/kprintf.o:   kernel/kprintf.c   | build
	$(CC) -c $< -o $@ $(CFLAGS)

build/panic.o:     kernel/panic.c     | build
	$(CC) -c $< -o $@ $(CFLAGS)

build/syscall.o:   kernel/syscall.c   | build
	$(CC) -c $< -o $@ $(CFLAGS)

build/keyboard.o:  drivers/keyboard.c | build
	$(CC) -c $< -o $@ $(CFLAGS)

build/mouse.o:     drivers/mouse.c    | build
	$(CC) -c $< -o $@ $(CFLAGS)

build/ata.o:       drivers/ata.c      | build
	$(CC) -c $< -o $@ $(CFLAGS)

build/vesa.o:      drivers/vesa.c     | build
	$(CC) -c $< -o $@ $(CFLAGS)

build/fat32.o:     fs/fat32.c         | build
	$(CC) -c $< -o $@ $(CFLAGS)

build/elf.o:       fs/elf.c           | build
	$(CC) -c $< -o $@ $(CFLAGS)

build/installer.o: fs/installer.c     | build
	$(CC) -c $< -o $@ $(CFLAGS)

build/draw.o:      ui/draw.c          | build
	$(CC) -c $< -o $@ $(CFLAGS)

build/font.o:      ui/font.c          | build
	$(CC) -c $< -o $@ $(CFLAGS)

build/gterm.o:     ui/gterm.c         | build
	$(CC) -c $< -o $@ $(CFLAGS)

build/shell.o:     shell/shell.c      | build
	$(CC) -c $< -o $@ $(CFLAGS)

build/eth.o:       net/eth.c          | build
	$(CC) -c $< -o $@ $(CFLAGS)

build/netstack.o:  net/netstack.c     | build
	$(CC) -c $< -o $@ $(CFLAGS)

build/arp.o:       net/arp.c          | build
	$(CC) -c $< -o $@ $(CFLAGS)

build/ipv4.o:      net/ipv4.c         | build
	$(CC) -c $< -o $@ $(CFLAGS)

build/icmp.o:      net/icmp.c         | build
	$(CC) -c $< -o $@ $(CFLAGS)

build/udp.o:       net/udp.c          | build
	$(CC) -c $< -o $@ $(CFLAGS)

build/tcp.o:       net/tcp.c          | build
	$(CC) -c $< -o $@ $(CFLAGS)

build/dns.o:       net/dns.c          | build
	$(CC) -c $< -o $@ $(CFLAGS)

build/http.o:      net/http.c         | build
	$(CC) -c $< -o $@ $(CFLAGS)

build/dhcp.o:      net/dhcp.c         | build
	$(CC) -c $< -o $@ $(CFLAGS)

build:
	mkdir -p build

mkfs:
	dd if=/dev/zero of=build/fat32.img bs=1M count=16
	mkfs.fat -F 32 build/fat32.img
	cp build/disk.img build/padded.img
	truncate -s $$((20480*512)) build/padded.img
	cat build/padded.img build/fat32.img > build/final.img

hdd:
	dd if=/dev/zero of=build/hdd.img bs=1M count=64 2>/dev/null

run:
	qemu-system-i386 \
	  -drive format=raw,file=build/final.img,if=ide \
	  -m 32 \
	  -nographic \
	  -no-reboot

run-vga:
	qemu-system-i386 \
	  -drive format=raw,file=build/final.img,if=ide \
	  -m 128 \
	  -vga std \
	  -monitor none \
	  -serial stdio \
	  -no-reboot

run-net:
	qemu-system-i386 \
	  -drive format=raw,file=build/final.img,if=ide \
	  -m 128 \
	  -nographic \
	  -net nic,model=rtl8139 \
	  -net user \
	  -no-reboot

run-net-vga:
	qemu-system-i386 \
	  -drive format=raw,file=build/final.img,if=ide \
	  -m 128 \
	  -vga std \
	  -monitor none \
	  -serial stdio \
	  -net nic,model=rtl8139 \
	  -net user \
	  -no-reboot

debug:
	mkdir -p logs
	qemu-system-i386 \
	  -drive format=raw,file=build/final.img,if=ide \
	  -m 128 \
	  -vga std \
	  -net nic,model=rtl8139 \
	  -net user \
	  -no-reboot \
	  -d cpu_reset,int \
	  -D logs/qemu_debug.log

clean:
	rm -rf build

# Install build dependencies on Fedora (dnf, not apt)
fedora-deps:
	sudo dnf install -y nasm qemu-system-i386 make gcc binutils
	@echo "Cross-compiler (i686-elf-gcc) must be built separately: see i686-elf-gcc_install.sh"
