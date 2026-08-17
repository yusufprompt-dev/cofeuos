# ─── Araçlar ───────────────────────────────────────────
CC      = clang
LD      = lld-link
OBJCOPY = llvm-objcopy
AS      = nasm

# ─── gnu-efi yolları ───────────────────────────────────
EFI_INC = /usr/include/efi

# ─── Dosya Yolları ─────────────────────────────────────
BOOT_DIR = boot
SRC_DIR  = src
ISO_NAME = CofeuOS-x86_64.iso
ISO_ROOT = iso

# ─── Ağ / Teşhis Bayrakları ────────────────────────────
# UEFI ConOut'a yoğun ağ teşhisi masaüstünü bozup yavaşlatmasın.
# Gerektiğinde: make iso NETWORK_DEBUG_FLAGS=-DNETWORK_DEBUG
NETWORK_DEBUG_FLAGS ?=
NETWORK_CONFIG_FLAGS ?= -DNETWORK_IP0=10 -DNETWORK_IP1=0 -DNETWORK_IP2=2 -DNETWORK_IP3=15 \
                        -DNETWORK_GATEWAY0=10 -DNETWORK_GATEWAY1=0 -DNETWORK_GATEWAY2=2 -DNETWORK_GATEWAY3=2 \
                        -DNETWORK_DNS0=10 -DNETWORK_DNS1=0 -DNETWORK_DNS2=2 -DNETWORK_DNS3=3

# ─── EFI Derleme Bayrakları ────────────────────────────
CFLAGS_EFI = -target x86_64-unknown-windows \
             -ffreestanding -fno-stack-protector -fno-stack-check \
             -fshort-wchar -mno-red-zone \
             -Wall -Wextra \
             -I$(EFI_INC) -I$(EFI_INC)/x86_64 \
           -DEFI_FUNCTION_WRAPPER -DUEFI_BUILD $(NETWORK_DEBUG_FLAGS) $(NETWORK_CONFIG_FLAGS)

# ─── Kernel Derleme Bayrakları ─────────────────────────
CFLAGS_K = -target x86_64-unknown-windows \
           -ffreestanding -Os \
           -fno-stack-protector -fno-stack-check \
           -fshort-wchar -mno-red-zone \
           -Wall -Wextra \
           -ffunction-sections -fdata-sections \
           -I$(EFI_INC) -I$(EFI_INC)/x86_64 \
           -DEFI_FUNCTION_WRAPPER -DUEFI_BUILD $(NETWORK_DEBUG_FLAGS) $(NETWORK_CONFIG_FLAGS)

# ─── Linker Bayrakları ─────────────────────────────────
LDFLAGS = -subsystem:efi_application \
          -entry:efi_main \
          -dll \
          

# ─── Objeler ───────────────────────────────────────────
OBJS = $(BOOT_DIR)/efi_main.o \
       $(SRC_DIR)/kernel/main.o \
       $(SRC_DIR)/lib/video.o \
       $(SRC_DIR)/lib/string.o \
       $(SRC_DIR)/lib/io.o \
       $(SRC_DIR)/lib/memory.o \
       $(SRC_DIR)/lib/sha256.o \
       $(SRC_DIR)/lib/fs.o \
       $(SRC_DIR)/lib/session.o \
       $(SRC_DIR)/lib/shell.o \
       $(SRC_DIR)/lib/python.o \
       $(SRC_DIR)/lib/keyboard.o \
       $(SRC_DIR)/lib/network.o \
       $(SRC_DIR)/lib/aes.o \
       $(SRC_DIR)/lib/rsa.o \
       $(SRC_DIR)/lib/tls_prf.o \
       $(SRC_DIR)/lib/tls_record.o \
       $(SRC_DIR)/lib/tls_handshake.o \
       $(SRC_DIR)/lib/tls_client.o \
       $(SRC_DIR)/lib/x509.o \
       $(SRC_DIR)/lib/trust_store.o \
       $(SRC_DIR)/lib/ec.o \
       $(SRC_DIR)/lib/sched.o \
       $(SRC_DIR)/lib/time.o \
       $(SRC_DIR)/lib/html.o \
       $(SRC_DIR)/lib/css.o \
       $(SRC_DIR)/lib/layout.o \
       $(SRC_DIR)/lib/web_mem.o \
       $(SRC_DIR)/lib/js.o \
       $(SRC_DIR)/lib/wifi.o \
       $(SRC_DIR)/lib/wpa_supplicant.o \
       $(SRC_DIR)/lib/dhcp.o \
       $(SRC_DIR)/lib/bluetooth.o \
       $(SRC_DIR)/lib/sound.o \
       $(SRC_DIR)/lib/usb.o \
       $(SRC_DIR)/lib/ssh.o \
       $(SRC_DIR)/lib/pkgmgr.o \
       $(SRC_DIR)/lib/sysmon.o \
       $(SRC_DIR)/lib/editor.o \
       $(SRC_DIR)/lib/calendar.o \
       $(SRC_DIR)/lib/games.o \
       $(SRC_DIR)/lib/mail.o \
       $(SRC_DIR)/lib/pdf.o \
       $(SRC_DIR)/lib/music.o \
       font.o

# ─── Hedefler ──────────────────────────────────────────
.DEFAULT_GOAL := all

all: $(ISO_NAME)

chkstk.o: chkstk.c
	$(CC) $(CFLAGS_K) -c chkstk.c -o chkstk.o

$(ISO_NAME): BOOTX64.EFI
	rm -f $@
	mkdir -p /tmp/cofeu_iso/EFI/BOOT
	cp -r $(ISO_ROOT)/. /tmp/cofeu_iso/ 2>/dev/null || true
	cp BOOTX64.EFI /tmp/cofeu_iso/EFI/BOOT/BOOTX64.EFI
	cp BOOTX64.EFI /tmp/cofeu_iso/BOOTX64.EFI
	xorriso -as mkisofs -o $@ -e BOOTX64.EFI -no-emul-boot /tmp/cofeu_iso
	rm -rf /tmp/cofeu_iso
	@echo ">>> $(ISO_NAME) hazir!"

BOOTX64.EFI: $(OBJS) $(MP_OBJS) src/uefi_stdlib.o chkstk.o
	$(LD) $(LDFLAGS) $(OBJS) $(MP_OBJS) src/uefi_stdlib.o chkstk.o -out:$@
	@echo ">>> BOOTX64.EFI hazir!"

# ─── EFI Entry ─────────────────────────────────────────
$(BOOT_DIR)/efi_main.o: $(BOOT_DIR)/efi_main.c
	$(CC) $(CFLAGS_EFI) -c $< -o $@

# ─── Kernel ────────────────────────────────────────────
$(SRC_DIR)/kernel/main.o: $(SRC_DIR)/kernel/main.c
	$(CC) $(CFLAGS_K) -c $< -o $@

# ─── Lib ───────────────────────────────────────────────
$(SRC_DIR)/lib/video.o: $(SRC_DIR)/lib/video.c
	$(CC) $(CFLAGS_K) -c $< -o $@

$(SRC_DIR)/lib/string.o: $(SRC_DIR)/lib/string.c
	$(CC) $(CFLAGS_K) -c $< -o $@

$(SRC_DIR)/lib/io.o: $(SRC_DIR)/lib/io.c
	$(CC) $(CFLAGS_K) -c $< -o $@

$(SRC_DIR)/lib/memory.o: $(SRC_DIR)/lib/memory.c
	$(CC) $(CFLAGS_K) -c $< -o $@

$(SRC_DIR)/lib/sha256.o: $(SRC_DIR)/lib/sha256.c
	$(CC) $(CFLAGS_K) -c $< -o $@

$(SRC_DIR)/lib/fs.o: $(SRC_DIR)/lib/fs.c
	$(CC) $(CFLAGS_K) -c $< -o $@

$(SRC_DIR)/lib/session.o: $(SRC_DIR)/lib/session.c
	$(CC) $(CFLAGS_K) -c $< -o $@

$(SRC_DIR)/lib/shell.o: $(SRC_DIR)/lib/shell.c
	$(CC) $(CFLAGS_K) -c $< -o $@

$(SRC_DIR)/lib/python.o: $(SRC_DIR)/lib/python.c
	$(CC) $(CFLAGS_K) -c $< -o $@

# ─── Düzeltilmiş Hali ───
$(SRC_DIR)/lib/keyboard.o: $(SRC_DIR)/lib/keyboard.c
	$(CC) $(CFLAGS_K) -c $< -o $@

$(SRC_DIR)/lib/network.o: $(SRC_DIR)/lib/network.c
	$(CC) $(CFLAGS_K) -c $< -o $@

$(SRC_DIR)/lib/aes.o: $(SRC_DIR)/lib/aes.c
	$(CC) $(CFLAGS_K) -c $< -o $@

$(SRC_DIR)/lib/rsa.o: $(SRC_DIR)/lib/rsa.c
	$(CC) $(CFLAGS_K) -c $< -o $@

$(SRC_DIR)/lib/tls_prf.o: $(SRC_DIR)/lib/tls_prf.c
	$(CC) $(CFLAGS_K) -c $< -o $@

$(SRC_DIR)/lib/tls_record.o: $(SRC_DIR)/lib/tls_record.c
	$(CC) $(CFLAGS_K) -c $< -o $@

$(SRC_DIR)/lib/tls_handshake.o: $(SRC_DIR)/lib/tls_handshake.c
	$(CC) $(CFLAGS_K) -c $< -o $@

$(SRC_DIR)/lib/tls_client.o: $(SRC_DIR)/lib/tls_client.c
	$(CC) $(CFLAGS_K) -c $< -o $@

$(SRC_DIR)/lib/x509.o: $(SRC_DIR)/lib/x509.c
	$(CC) $(CFLAGS_K) -c $< -o $@

$(SRC_DIR)/lib/trust_store.o: $(SRC_DIR)/lib/trust_store.c
	$(CC) $(CFLAGS_K) -c $< -o $@

$(SRC_DIR)/lib/ec.o: $(SRC_DIR)/lib/ec.c
	$(CC) $(CFLAGS_K) -c $< -o $@

$(SRC_DIR)/lib/sched.o: $(SRC_DIR)/lib/sched.c
	$(CC) $(CFLAGS_K) -c $< -o $@

$(SRC_DIR)/lib/time.o: $(SRC_DIR)/lib/time.c
	$(CC) $(CFLAGS_K) -c $< -o $@

$(SRC_DIR)/lib/html.o: $(SRC_DIR)/lib/html.c
	$(CC) $(CFLAGS_K) -c $< -o $@

$(SRC_DIR)/lib/css.o: $(SRC_DIR)/lib/css.c
	$(CC) $(CFLAGS_K) -c $< -o $@

$(SRC_DIR)/lib/layout.o: $(SRC_DIR)/lib/layout.c
	$(CC) $(CFLAGS_K) -c $< -o $@

$(SRC_DIR)/lib/web_mem.o: $(SRC_DIR)/lib/web_mem.c
	$(CC) $(CFLAGS_K) -c $< -o $@

$(SRC_DIR)/lib/js.o: $(SRC_DIR)/lib/js.c
	$(CC) $(CFLAGS_K) -c $< -o $@

$(SRC_DIR)/lib/wifi.o: $(SRC_DIR)/lib/wifi.c
	$(CC) $(CFLAGS_K) -c $< -o $@

$(SRC_DIR)/lib/wpa_supplicant.o: $(SRC_DIR)/lib/wpa_supplicant.c
	$(CC) $(CFLAGS_K) -c $< -o $@

$(SRC_DIR)/lib/dhcp.o: $(SRC_DIR)/lib/dhcp.c
	$(CC) $(CFLAGS_K) -c $< -o $@

$(SRC_DIR)/lib/bluetooth.o: $(SRC_DIR)/lib/bluetooth.c
	$(CC) $(CFLAGS_K) -c $< -o $@

$(SRC_DIR)/lib/sound.o: $(SRC_DIR)/lib/sound.c
	$(CC) $(CFLAGS_K) -c $< -o $@

$(SRC_DIR)/lib/usb.o: $(SRC_DIR)/lib/usb.c
	$(CC) $(CFLAGS_K) -c $< -o $@

$(SRC_DIR)/lib/ssh.o: $(SRC_DIR)/lib/ssh.c
	$(CC) $(CFLAGS_K) -c $< -o $@

$(SRC_DIR)/lib/pkgmgr.o: $(SRC_DIR)/lib/pkgmgr.c
	$(CC) $(CFLAGS_K) -c $< -o $@

$(SRC_DIR)/lib/sysmon.o: $(SRC_DIR)/lib/sysmon.c
	$(CC) $(CFLAGS_K) -c $< -o $@

$(SRC_DIR)/lib/editor.o: $(SRC_DIR)/lib/editor.c
	$(CC) $(CFLAGS_K) -c $< -o $@

$(SRC_DIR)/lib/calendar.o: $(SRC_DIR)/lib/calendar.c
	$(CC) $(CFLAGS_K) -c $< -o $@

$(SRC_DIR)/lib/games.o: $(SRC_DIR)/lib/games.c
	$(CC) $(CFLAGS_K) -c $< -o $@

$(SRC_DIR)/lib/mail.o: $(SRC_DIR)/lib/mail.c
	$(CC) $(CFLAGS_K) -c $< -o $@

$(SRC_DIR)/lib/pdf.o: $(SRC_DIR)/lib/pdf.c
	$(CC) $(CFLAGS_K) -c $< -o $@

$(SRC_DIR)/lib/music.o: $(SRC_DIR)/lib/music.c
	$(CC) $(CFLAGS_K) -c $< -o $@


# ─── Font ──────────────────────────────────────────────
font.psf: gohufont.h
	python3 gen_font.py

font.o: font_data.c
	$(CC) $(CFLAGS_K) -c font_data.c -o font.o

# ─── QEMU ile Test ─────────────────────────────────────
run: uefi-disk.img
	qemu-system-x86_64 \
	  -bios /usr/share/ovmf/OVMF.fd \
	  -drive format=raw,file=$< \
	  -m 256M \
	  -netdev user,id=net0 -device e1000,netdev=net0

uefi-disk.img: BOOTX64.EFI
	dd if=/dev/zero of=$@ bs=1M count=64
	mkfs.fat -F 32 $@
	mkdir -p /tmp/cofeu_mnt
	sudo mount -o loop $@ /tmp/cofeu_mnt
	sudo mkdir -p /tmp/cofeu_mnt/EFI/BOOT
	sudo cp BOOTX64.EFI /tmp/cofeu_mnt/EFI/BOOT/
	sudo umount /tmp/cofeu_mnt

# ─── Temizlik ──────────────────────────────────────────
clean:
	rm -rf *.efi *.EFI *.so *.img *.psf *.iso font.o \
	       $(BOOT_DIR)/*.o \
	       $(SRC_DIR)/lib/*.o \
	       $(SRC_DIR)/kernel/*.o
# ─── ISO Oluşturma ─────────────────────────────────────
iso: BOOTX64.EFI
	@echo ">>> ISO oluşturuluyor..."
	@mkdir -p iso/EFI/BOOT
	@cp BOOTX64.EFI iso/EFI/BOOT/
	@dd if=/dev/zero of=iso/efiboot.img bs=1M count=4 2>/dev/null
	@mkfs.vfat iso/efiboot.img >/dev/null
	@mmd -i iso/efiboot.img ::EFI
	@mmd -i iso/efiboot.img ::EFI/BOOT
	@mcopy -i iso/efiboot.img BOOTX64.EFI ::EFI/BOOT/BOOTX64.EFI
	xorriso -as mkisofs \
	  -o CofeuOS-x86_64.iso \
	  -eltorito-alt-boot \
	  -e efiboot.img \
	  -no-emul-boot \
	  -isohybrid-gpt-basdat \
	  iso
	@echo ">>> $(ISO_NAME) hazır!"
runiso: iso OVMF_VARS_local.fd
	qemu-system-x86_64 \
	  -drive if=pflash,format=raw,readonly=on,file=/usr/share/OVMF/OVMF_CODE_4M.fd \
	  -drive if=pflash,format=raw,file=$(CURDIR)/OVMF_VARS_local.fd \
	  -cdrom $(ISO_NAME) \
	  -m 256M
OVMF_VARS_local.fd:
	cp /usr/share/OVMF/OVMF_VARS_4M.fd $@

# MicroPython
MP_DIR = src/micropython_embed
MP_SRCS = $(shell find $(MP_DIR) -name "*.c")
MP_OBJS = $(MP_SRCS:.c=.o)

$(MP_DIR)/%.o: $(MP_DIR)/%.c
	$(CC) $(CFLAGS_K) -DMICROPY_PY_BUILTINS_INPUT=1 -I$(MP_DIR) -I$(MP_DIR)/port -I$(MP_DIR)/genhdr -c $< -o $@

src/uefi_stdlib.o: src/uefi_stdlib.c
	$(CC) $(CFLAGS_K) -c $< -o $@
