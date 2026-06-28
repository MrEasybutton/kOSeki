qemu-system-i386 -cdrom out/kOSeki.iso -drive file=fat32.img,format=raw -vga std -m 64 \
    -netdev user,id=u1,hostfwd=udp::1234-:1234 -device e1000,netdev=u1
