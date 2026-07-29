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

# ─── EFI Derleme Bayrakları ────────────────────────────
CFLAGS_EFI = -target x86_64-unknown-windows \
             -ffreestanding -fno-stack-protector -fno-stack-check \
             -fshort-wchar -mno-red-zone \
             -Wall -Wextra \
             -I$(EFI_INC) -I$(EFI_INC)/x86_64 \
             -DEFI_FUNCTION_WRAPPER

# ─── Kernel Derleme Bayrakları ─────────────────────────
CFLAGS_K = -target x86_64-unknown-windows \
           -ffreestanding -Os \
           -fno-stack-protector -fno-stack-check \
           -fshort-wchar -mno-red-zone \
           -Wall -Wextra \
           -ffunction-sections -fdata-sections \
           -I$(EFI_INC) -I$(EFI_INC)/x86_64 \
           -DEFI_FUNCTION_WRAPPER

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
       $(SRC_DIR)/lib/shell.o \
       $(SRC_DIR)/lib/python.o \
       $(SRC_DIR)/lib/keyboard.o \
       $(SRC_DIR)/lib/network.o \
       font.o

# ─── Hedefler ──────────────────────────────────────────
chkstk.o: chkstk.c
	$(CC) $(CFLAGS_K) -c chkstk.c -o chkstk.o

all: $(ISO_NAME)

$(ISO_NAME): BOOTX64.EFI
	rm -f $@
	mkdir -p /tmp/cofeu_iso/EFI/BOOT
	cp -r $(ISO_ROOT)/. /tmp/cofeu_iso/ 2>/dev/null || true
	cp BOOTX64.EFI /tmp/cofeu_iso/EFI/BOOT/BOOTX64.EFI
	xorriso -indev /dev/null -outdev $@ -map /tmp/cofeu_iso / -boot_image any path=/EFI/BOOT/BOOTX64.EFI -commit >/dev/null
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

$(SRC_DIR)/lib/shell.o: $(SRC_DIR)/lib/shell.c
	$(CC) $(CFLAGS_K) -c $< -o $@

$(SRC_DIR)/lib/python.o: $(SRC_DIR)/lib/python.c
	$(CC) $(CFLAGS_K) -c $< -o $@

# ─── Düzeltilmiş Hali ───
$(SRC_DIR)/lib/keyboard.o: $(SRC_DIR)/lib/keyboard.c
	$(CC) $(CFLAGS_K) -c $< -o $@

$(SRC_DIR)/lib/network.o: $(SRC_DIR)/lib/network.c
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
