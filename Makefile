all: bootloader

# Build process
bootloader:
	# Assemble bootloader
	nasm boot/boot.asm -f bin -o boot/bin/boot.bin
	
	# Build + Link kernel
	nasm boot/kernel_entry.asm -f elf -o boot/bin/kernel_entry.bin
	gcc -m32 -ffreestanding -c boot/loader.c -o boot/bin/kernel.o
	ld -m elf_i386 -o boot/bin/kernel.img -Ttext 0x9000 boot/bin/kernel_entry.bin boot/bin/kernel.o
	
	objcopy -O binary -j .text boot/bin/kernel.img boot/bin/kernel.bin
	
	# Convert to iso
	cat boot/bin/boot.bin boot/bin/kernel.bin > kOSeki.iso

# Remove all the binaries
clear: 
	rm -f boot/bin/boot.bin boot/bin/kernel.bin boot/bin/kernel_entry.bin boot/bin/kernel.o boot/bin/kernel.img kOSeki.iso

# Run iso
run:
	qemu-system-x86_64 -L "C:\Program Files\qemu" -drive format=raw,file=kOSeki.iso
