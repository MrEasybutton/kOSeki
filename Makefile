ASM = /usr/bin/nasm
CC = /usr/bin/gcc
LD = /usr/bin/ld
GRUB = /usr/bin/grub-mkrescue

SRC = src
ASM_SRC = $(SRC)/asm
LWIP_DIR = external/lwip/src
MBEDTLS_DIR = external/mbedtls

OBJ = obj
ASM_OBJ = $(OBJ)/asm
APPS_OBJ = $(OBJ)/apps
LWIP_OBJ = $(OBJ)/lwip
MBEDTLS_OBJ = $(OBJ)/mbedtls

CONFIG = ./config
OUT = out
INC = ./include
INCLUDE=-I$(INC) -Iinclude/net_port -I$(LWIP_DIR)/include -I$(MBEDTLS_DIR)/include -I$(MBEDTLS_DIR)/library

MKDIR= mkdir -p
CP = cp -f
RM = rm -rf
DEFINES= -DMBEDTLS_CONFIG_FILE=\"net_port/mbedtls_config.h\"

QEMU= qemu-system-x86_64
QEMU_FLAGS= -boot d -cdrom out/kOSeki.iso -drive file=fat32.img,format=raw -audiodev pa,id=snd0 -device pcspk,audiodev=snd0 -serial file:debug.log -vga virtio -full-screen

ASM_FLAGS = -f elf32
CC_FLAGS = $(INCLUDE) -I$(SRC) $(DEFINES) -m32 -std=gnu99 -ffreestanding -Wall -Wextra
LD_FLAGS = -m elf_i386 -T $(CONFIG)/linker.ld -nostdlib --allow-multiple-definition

TARGET=$(OUT)/kOSeki.bin
TARGET_ISO=$(OUT)/kOSeki.iso
ISO_DIR=$(OUT)/isodir

LWIP_SOURCES = $(LWIP_DIR)/core/init.c \
               $(LWIP_DIR)/core/def.c \
               $(LWIP_DIR)/core/dns.c \
               $(LWIP_DIR)/core/inet_chksum.c \
               $(LWIP_DIR)/core/ip.c \
               $(LWIP_DIR)/core/mem.c \
               $(LWIP_DIR)/core/memp.c \
               $(LWIP_DIR)/core/netif.c \
               $(LWIP_DIR)/core/pbuf.c \
               $(LWIP_DIR)/core/raw.c \
               $(LWIP_DIR)/core/stats.c \
               $(LWIP_DIR)/core/sys.c \
               $(LWIP_DIR)/core/tcp.c \
               $(LWIP_DIR)/core/tcp_in.c \
               $(LWIP_DIR)/core/tcp_out.c \
               $(LWIP_DIR)/core/timeouts.c \
               $(LWIP_DIR)/core/udp.c \
               $(LWIP_DIR)/core/ipv4/acd.c \
               $(LWIP_DIR)/core/ipv4/autoip.c \
               $(LWIP_DIR)/core/ipv4/dhcp.c \
               $(LWIP_DIR)/core/ipv4/etharp.c \
               $(LWIP_DIR)/core/ipv4/icmp.c \
               $(LWIP_DIR)/core/ipv4/igmp.c \
               $(LWIP_DIR)/core/ipv4/ip4.c \
               $(LWIP_DIR)/core/ipv4/ip4_addr.c \
               $(LWIP_DIR)/core/ipv4/ip4_frag.c \
               $(LWIP_DIR)/core/altcp.c \
               $(LWIP_DIR)/core/altcp_alloc.c \
               $(LWIP_DIR)/core/altcp_tcp.c \
               $(LWIP_DIR)/apps/altcp_tls/altcp_tls_mbedtls.c \
               $(LWIP_DIR)/apps/altcp_tls/altcp_tls_mbedtls_mem.c \
               $(LWIP_DIR)/apps/mqtt/mqtt.c \
               $(LWIP_DIR)/netif/ethernet.c

MBEDTLS_SOURCES = $(MBEDTLS_DIR)/library/aes.c \
                  $(MBEDTLS_DIR)/library/asn1parse.c \
                  $(MBEDTLS_DIR)/library/asn1write.c \
                  $(MBEDTLS_DIR)/library/bignum.c \
                  $(MBEDTLS_DIR)/library/cipher.c \
                  $(MBEDTLS_DIR)/library/cipher_wrap.c \
                  $(MBEDTLS_DIR)/library/constant_time.c \
                  $(MBEDTLS_DIR)/library/ctr_drbg.c \
                  $(MBEDTLS_DIR)/library/ccm.c \
                  $(MBEDTLS_DIR)/library/chacha20.c \
                  $(MBEDTLS_DIR)/library/poly1305.c \
                  $(MBEDTLS_DIR)/library/chachapoly.c \
                  $(MBEDTLS_DIR)/library/ecdh.c \
                  $(MBEDTLS_DIR)/library/ecdsa.c \
                  $(MBEDTLS_DIR)/library/ecp.c \
                  $(MBEDTLS_DIR)/library/ecp_curves.c \
                  $(MBEDTLS_DIR)/library/entropy.c \
                  $(MBEDTLS_DIR)/library/gcm.c \
                  $(MBEDTLS_DIR)/library/md.c \
                  $(MBEDTLS_DIR)/library/oid.c \
                  $(MBEDTLS_DIR)/library/pk.c \
                  $(MBEDTLS_DIR)/library/pk_wrap.c \
                  $(MBEDTLS_DIR)/library/pkparse.c \
                  $(MBEDTLS_DIR)/library/rsa.c \
                  $(MBEDTLS_DIR)/library/rsa_internal.c \
                  $(MBEDTLS_DIR)/library/sha256.c \
                  $(MBEDTLS_DIR)/library/sha1.c \
                  $(MBEDTLS_DIR)/library/sha512.c \
                  $(MBEDTLS_DIR)/library/ssl_cli.c \
                  $(MBEDTLS_DIR)/library/ssl_msg.c \
                  $(MBEDTLS_DIR)/library/ssl_tls.c \
                  $(MBEDTLS_DIR)/library/ssl_ciphersuites.c \
                  $(MBEDTLS_DIR)/library/x509.c \
                  $(MBEDTLS_DIR)/library/x509_crt.c \
                  $(MBEDTLS_DIR)/library/platform.c \
                  $(MBEDTLS_DIR)/library/platform_util.c

LWIP_OBJS = $(patsubst $(LWIP_DIR)/%.c, $(LWIP_OBJ)/%.o, $(LWIP_SOURCES))
MBEDTLS_OBJS = $(patsubst $(MBEDTLS_DIR)/library/%.c, $(MBEDTLS_OBJ)/%.o, $(MBEDTLS_SOURCES))

OBJECTS=$(ASM_OBJ)/boot.o $(ASM_OBJ)/tables.o $(ASM_OBJ)/interrupts.o $(ASM_OBJ)/bios32_call.o \
        $(OBJ)/ports.o \
        $(OBJ)/string.o $(OBJ)/console.o \
        $(OBJ)/gdt.o $(OBJ)/idt.o $(OBJ)/isr.o $(OBJ)/8259_pic.o \
        $(OBJ)/keyboard.o $(OBJ)/bae.o \
        $(OBJ)/kernel.o \
        $(OBJ)/graphics.o $(OBJ)/fonts/fuzzy.o $(OBJ)/fonts/kalnia.o $(OBJ)/fonts/fonts.o \
        $(OBJ)/ide.o $(OBJ)/kheap.o $(OBJ)/pmm.o $(OBJ)/bios32.o \
        $(OBJ)/vesa.o $(OBJ)/fat32.o $(OBJ)/bmp.o $(OBJ)/libgcc.o \
        $(OBJ)/gui.o $(OBJ)/pon.o $(OBJ)/cmos.o $(OBJ)/serial.o \
		$(OBJ)/pci.o \
		$(OBJ)/e1000.o \
		$(OBJ)/net.o \
		$(OBJ)/net_mqtt.o \
		$(OBJ)/net_test.o \
		$(OBJ)/kronii.o \
		$(OBJ)/procsys.o $(OBJ)/utils.o $(OBJ)/kmath.o \
		$(OBJ)/mbedtls_port.o \
        $(OBJ)/ac97.o $(OBJ)/synth.o $(OBJ)/mp3.o \
		$(APPS_OBJ)/Pebbleshell.o $(APPS_OBJ)/DOOM.o $(APPS_OBJ)/Casefiles.o $(APPS_OBJ)/Preferences.o $(APPS_OBJ)/Novella.o $(APPS_OBJ)/WAHtercolour.o \
		$(APPS_OBJ)/SBG.o $(APPS_OBJ)/sbg_chaser.o $(APPS_OBJ)/sbg_blox.o \
		$(APPS_OBJ)/Reaper.o $(APPS_OBJ)/html_engine.o $(APPS_OBJ)/CLPlayer.o $(APPS_OBJ)/CLStudio.o $(APPS_OBJ)/Gawrculator.o $(APPS_OBJ)/Baetracer.o \
		$(OBJ)/texture_cache.o \
		$(OBJ)/net_port/k_netif.o \
		$(OBJ)/baux2/baux2.o $(OBJ)/baux2/scanner.o $(OBJ)/baux2/parser.o $(OBJ)/baux2/environment.o $(OBJ)/baux2/interpreter.o \
		$(LWIP_OBJS) \
		$(MBEDTLS_OBJS)

.PHONY: all clean clean-libs distclean libs rootfs run rebuild

src-clean:
	@printf "[ cleaning OBJs for src... ]\n"
	@if [ -d $(OBJ) ]; then find $(OBJ) -mindepth 1 -maxdepth 1 ! -name lwip ! -name mbedtls ! -name net_port -exec $(RM) {} +; fi
	$(RM) $(APPS_OBJ)
	@printf "[ done :D ]\n"

apps: $(APPS_OBJ) $(patsubst $(SRC)/apps/%.c, $(APPS_OBJ)/%.o, $(wildcard $(SRC)/apps/*.c))
	@printf "[ linking... ]\n\n"
	@printf "𝓼𝓽𝓲𝓬𝓴𝓲𝓷𝓰 𝓸𝓾𝓽 𝔂𝓸𝓾𝓻 𝓰𝔂𝓪𝓽𝓽 𝓯𝓸𝓻 𝓝𝓮𝓻𝓲𝔃𝔃𝓵𝓮𝓻\n\n"
	$(LD) $(LD_FLAGS) -o $(TARGET) $(OBJECTS)
	@printf "\n[ building ISO... ]\n"
	@printf "𝔂𝓸𝓾'𝓻𝓮 𝓼𝓸 𝓑𝓐𝓤 𝓑𝓐𝓤\n\n"
	$(MKDIR) $(ISO_DIR)/boot/grub
	$(CP) $(TARGET) $(ISO_DIR)/boot/
	$(CP) $(CONFIG)/grub.cfg $(ISO_DIR)/boot/grub/
	$(GRUB) -o $(TARGET_ISO) $(ISO_DIR)
	$(RM) $(TARGET)
	@printf "\n[ finished rebuilding apps ]\n"

apps-clean:
	@printf "[ cleaning OBJs for apps ]\n"
	$(RM) $(APPS_OBJ)
	@printf "[ done :D ]\n"

appsfull: apps-clean apps

all: $(OBJECTS) $(OUT)
	@printf "[ linking... ]\n"
	@printf "𝓼𝓽𝓲𝓬𝓴𝓲𝓷𝓰 𝓸𝓾𝓽 𝔂𝓸𝓾𝓻 𝓰𝔂𝓪𝓽𝓽 𝓯𝓸𝓻 𝓝𝓮𝓻𝓲𝔃𝔃𝓵𝓮𝓻\n\n"
	$(LD) $(LD_FLAGS) -o $(TARGET) $(OBJECTS)
	@printf "\n[ building ISO... ]\n"
	@printf "𝔂𝓸𝓾'𝓻𝓮 𝓼𝓸 𝓑𝓐𝓤 𝓑𝓐𝓤\n\n"
	$(MKDIR) $(ISO_DIR)/boot/grub
	$(CP) $(TARGET) $(ISO_DIR)/boot/
	$(CP) $(CONFIG)/grub.cfg $(ISO_DIR)/boot/grub/
	$(GRUB) -o $(TARGET_ISO) $(ISO_DIR)
	$(RM) $(TARGET)
	@printf "𝔂𝓸𝓾'𝓻𝓮 𝓼𝓸 𝓑𝓘𝓑𝓞𝓞 𝓣𝓐𝓧\n\n\n"
	@printf "\n[ BUILD COMPLETE ]\n\n"

$(OBJ)/%.o : $(SRC)/%.c
	@printf "[ $< ]\n"
	$(CC) $(CC_FLAGS) -c $< -o $@
	@printf "\n"

$(OBJ)/fonts/%.o : $(SRC)/fonts/%.c | $(OBJ)/fonts
	@printf "[ $< ]\n"
	$(CC) $(CC_FLAGS) -c $< -o $@
	@printf "\n"

$(APPS_OBJ)/%.o : $(SRC)/apps/%.c | $(APPS_OBJ)
	@printf "[ $< ]\n"
	$(CC) $(CC_FLAGS) -c $< -o $@
	@printf "\n"

$(LWIP_OBJ)/%.o : $(LWIP_DIR)/%.c | $(LWIP_OBJ)
	@printf "[ $< ]\n"
	@mkdir -p $(dir $@)
	$(CC) $(CC_FLAGS) -c $< -o $@
	@printf "\n"

$(MBEDTLS_OBJ)/%.o : $(MBEDTLS_DIR)/library/%.c | $(MBEDTLS_OBJ)
	@printf "[ $< ]\n"
	$(CC) $(CC_FLAGS) -c $< -o $@
	@printf "\n"

$(OBJ)/net_port/%.o : $(SRC)/net_port/%.c | $(OBJ)/net_port
	@printf "[ $< ]\n"
	$(CC) $(CC_FLAGS) -c $< -o $@
	@printf "\n"

$(OBJ)/baux2/%.o : $(SRC)/baux2/%.c | $(OBJ)/baux2
	@printf "[ $< ]\n"
	$(CC) $(CC_FLAGS) -c $< -o $@
	@printf "\n"

$(ASM_OBJ)/%.o : $(ASM_SRC)/%.asm | $(ASM_OBJ)
	@printf "[ $< ]\n"
	$(ASM) $(ASM_FLAGS) $< -o $@
	@printf "\n"

$(OBJ)/fonts:
	$(MKDIR) $(OBJ)/fonts

$(APPS_OBJ):
	$(MKDIR) $(APPS_OBJ)

$(LWIP_OBJ):
	$(MKDIR) $(LWIP_OBJ)

$(MBEDTLS_OBJ):
	$(MKDIR) $(MBEDTLS_OBJ)

$(OBJ)/net_port:
	$(MKDIR) $(OBJ)/net_port

$(OBJ)/baux2:
	$(MKDIR) $(OBJ)/baux2

$(OBJ):
	$(MKDIR) $(OBJ)

$(ASM_OBJ):
	$(MKDIR) $(ASM_OBJ)

$(OUT):
	@$(MKDIR) $(OUT)

FAT32_IMG = fat32.img
FAT32_SIZE_MB = 64
ROOTFS = rootfs
MOUNT_POINT = /mnt/fat32

VBOXMANAGE = "/mnt/c/Program Files/Oracle/VirtualBox/VBoxManage.exe"

rootfs:
	@printf "[ reformatting image... ]\n"
	@if mountpoint -q $(MOUNT_POINT); then sudo umount $(MOUNT_POINT); fi
	dd if=/dev/zero of=$(FAT32_IMG) bs=1M count=$(FAT32_SIZE_MB)
	mkfs.fat -F 32 -s 8 $(FAT32_IMG)
	@printf "[ mounting... (zamn) ]\n"
	sudo mkdir -p $(MOUNT_POINT)
	sudo mount -o loop $(FAT32_IMG) $(MOUNT_POINT)
	sudo cp -r $(ROOTFS)/. $(MOUNT_POINT)/
	sudo umount $(MOUNT_POINT)
	@printf "[ done :D ]\n"

vdi:
	@printf "[ CONVERTING UR IMAGE TO VDI (use in VBox) ]\n"
	$(RM) fat32.vdi
	$(VBOXMANAGE) convertfromraw $(FAT32_IMG) fat32.vdi --format VDI
	@printf ">> assigning hardcoded ID (you can change this in makefile) >>\n"
	$(VBOXMANAGE) internalcommands sethduuid fat32.vdi "6b438206-696d-496e-6920-636c6920726b"
	@printf ">> created fat32.vdi successfully! >>\n"

clean: rootfs
	@printf "[ cleaning all build artifacts (external libs are preserved)... ]\n"
	$(RM) $(OUT)
	@if [ -d $(OBJ) ]; then find $(OBJ) -mindepth 1 -maxdepth 1 ! -name lwip ! -name mbedtls -exec $(RM) {} +; fi
	@printf "[ done :D ]\n"

clean-libs:
	@printf "[ cleaning external libs... ]\n"
	$(RM) $(LWIP_OBJ) $(MBEDTLS_OBJ)
	@printf "[ done :D ]\n"

distclean: rootfs
	@printf "[ cleaning all artifacts... ]\n"
	$(RM) $(OUT) $(OBJ)
	@printf "[ done :D ]\n"

libs: $(LWIP_OBJS) $(MBEDTLS_OBJS)
	@printf "[ built external libs!! ]\n"

rebuild: clean all

# -display gtk,zoom-to-fit=on
# do -accel kvm -cpu host for slow startup, high performance

#-accel kvm -cpu host
run:
	@printf "𝓲 𝓳𝓾𝓼𝓽 𝔀𝓪𝓷𝓷𝓪 𝓫𝓮 𝔂𝓸𝓾𝓻 𝓢𝓗𝓘𝓞𝓡𝓘\n\n"
	qemu-system-x86_64 \
		-boot d \
		-cdrom out/kOSeki.iso \
		-drive file=fat32.img,format=raw \
		-m 128 \
		-netdev user,id=u1,hostfwd=udp::1234-:1234 \
		-device e1000,netdev=u1

# sound doesnt work in WSL cause audio forwarding issue, this is just for directSound

run_win:
	@printf "𝓲 𝓳𝓾𝓼𝓽 𝔀𝓪𝓷𝓷𝓪 𝓫𝓮 𝔂𝓸𝓾𝓻 𝓢𝓗𝓘𝓞𝓡𝓘 (sing it with me! :D)\n\n"
	"/mnt/c/Program Files/qemu/qemu-system-x86_64.exe" \
		-accel whpx,kernel-irqchip=off \
		-boot d \
		-cdrom out/kOSeki.iso \
		-drive file=fat32.img,format=raw \
		-m 64 \
		-audiodev dsound,id=snd0 \
		-device AC97,audiodev=snd0

# check serial.log for debug log output!!

debug:
	qemu-system-x86_64 \
		-boot d \
		-cdrom out/kOSeki.iso \
		-drive file=fat32.img,format=raw \
		-m 64 \
		-netdev user,id=u1,hostfwd=udp::1234-:1234 \
		-device e1000,netdev=u1 \
		-serial file:serial.log \