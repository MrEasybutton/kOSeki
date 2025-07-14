all: build run
	@echo ""
	@echo "Kernel size: $$(ls -l boot/bin/kernel.bin | awk '{print $$5}') bytes" > .kinfo.tmp
	@echo "Sectors calculated: $$((($$(ls -l boot/bin/kernel.bin | awk '{print $$5}') + 511) / 512))" >> .kinfo.tmp
	@echo "Bootloader limit: 128 sectors (65536 bytes)" >> .kinfo.tmp
	@cat .kinfo.tmp
	@sectors=$$(awk '/Sectors calculated:/ {print $$3}' .kinfo.tmp); \
	if [ $$sectors -le 128 ]; then \
		echo "Doesn't exceed sector limit, this will probably run :D"; \
	else \
		echo "Bweh it's not going to run chat dang it"; \
	fi
	@rm .kinfo.tmp

help:
	@echo "Available:"
	@echo "  make              - Build the kernel and OS image"
	@echo "  make run          - Run kOSeki in QEMU"
	@echo "  make qsize		   - Query kOSeki kernel size"
	@echo "  make clear        - Remove all binary files"
	@echo "  make font         - Convert TTF font to kOSeki system font"
	@echo "  	(e.g.):        make fonts FONT=Mono.ttf"
	@echo "  make fonts-default - Convert the default font (Fuzzy.ttf)"
	

build:
	# -------------------------------------
	# sticking out your gyatt for nerizzler
	# -------------------------------------
	nasm boot/boot_S1.asm -f bin -o boot/bin/boot.bin
	
	# --------------------------------------
	# you're so bau bau, you're so biboo tax
	# --------------------------------------
	nasm boot/kernel_entry.asm -f elf -o boot/bin/kernel_entry.bin
	gcc -m32 -ffreestanding -c boot/loader.c -o boot/bin/kernel.o
	gcc -m32 -ffreestanding -c boot/coreutils.c -o boot/bin/coreutils.o
	gcc -m32 -ffreestanding -c boot/time.c -o boot/bin/time.o
	ld -m elf_i386 -o boot/bin/kernel.img -Ttext 0x9000 boot/bin/kernel_entry.bin boot/bin/kernel.o boot/bin/coreutils.o boot/bin/time.o
	
	objcopy -O binary -j .text boot/bin/kernel.img boot/bin/kernel.bin
	# ---------------------------
	# i just wanna be your shiori
	# ---------------------------
	cat boot/bin/boot.bin boot/bin/kernel.bin > kOSeki.iso

font:
	# 
	# Converting .ttf font to the system font
	# USage: make fonts [FONT=filename.ttf] [NAME=fontname]
	#
	python3 boot/utilities/python_tools/make_font.py \
		--font=$(FONT) \
		--output=boot/font.c

font-d:
	@echo "converting to (Fuzzy.ttf)..."
	python3 boot/utilities/python_tools/make_font.py

clear: 
	rm -f boot/bin/boot.bin boot/bin/kernel.bin boot/bin/kernel_entry.bin boot/bin/kernel.o boot/bin/coreutils.o boot/bin/kernel.img kOSeki.iso

run:
	qemu-system-x86_64 -L "C:\Program Files\qemu" -drive format=raw,file=kOSeki.iso -audio none

qsize:
	@echo "Kernel size: $$(ls -l boot/bin/kernel.bin | awk '{print $$5}') bytes"
	@echo "Sectors: $$((($$(ls -l boot/bin/kernel.bin | awk '{print $$5}') + 511) / 512))"
	@echo "boot limit: 128 sectors (65536 bytes)"
	@echo "sometimes bootloader limit doesn't apply but it generally is accurate"