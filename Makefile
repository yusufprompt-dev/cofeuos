# Araçlar
CC = i686-elf-gcc
LD = i686-elf-ld
AS = nasm
OBJCOPY = objcopy

# Dosya Yolları
BOOT_DIR = boot
KERNEL_DIR = kernel
SRC_DIR = src

# Yeni modüler yapı dosyaları
LIB_SOURCES = $(SRC_DIR)/lib/string.c $(SRC_DIR)/lib/io.c $(SRC_DIR)/lib/memory.c \
              $(SRC_DIR)/lib/sha256.c $(SRC_DIR)/lib/fs.c $(SRC_DIR)/lib/shell.c \
              $(SRC_DIR)/lib/python.c $(SRC_DIR)/lib/keyboard.c
KERNEL_SOURCES = $(SRC_DIR)/kernel/main.c
BOOT_SOURCES = $(BOOT_DIR)/boot.asm $(BOOT_DIR)/kernel_entry.asm

# Çıktı Dosyaları
# kernel_entry.o'nun EN BAŞTA olması hayati önem taşır!
OBJ = $(BOOT_DIR)/kernel_entry.o font.o \
      $(SRC_DIR)/lib/string.o $(SRC_DIR)/lib/io.o $(SRC_DIR)/lib/memory.o \
      $(SRC_DIR)/lib/sha256.o $(SRC_DIR)/lib/fs.o $(SRC_DIR)/lib/shell.o \
      $(SRC_DIR)/lib/python.o $(SRC_DIR)/lib/keyboard.o \
      $(SRC_DIR)/kernel/main.o

# Bayraklar
CFLAGS = -ffreestanding -Os -Wall -Wextra -fno-stack-protector -m32 -ffunction-sections -fdata-sections
LDFLAGS = -Ttext 0x10000 --oformat binary --gc-sections

all: os-image.bin run

os-image.bin: boot.bin kernel.bin
	cat $^ > $@
	truncate -s 1M $@

kernel.bin: $(BOOT_DIR)/kernel_entry.o font.o \
           $(SRC_DIR)/lib/video.o $(SRC_DIR)/kernel/main.o \
           $(SRC_DIR)/lib/string.o $(SRC_DIR)/lib/io.o $(SRC_DIR)/lib/memory.o \
           $(SRC_DIR)/lib/sha256.o $(SRC_DIR)/lib/fs.o $(SRC_DIR)/lib/shell.o \
           $(SRC_DIR)/lib/python.o $(SRC_DIR)/lib/keyboard.o

	$(LD) -o $@ $(LDFLAGS) $^



# Eski kernel derleme
$(KERNEL_DIR)/kernel.o: $(KERNEL_DIR)/kernel.c
	$(CC) $(CFLAGS) -c $< -o $@

# Yeni modüler kernel derleme
$(SRC_DIR)/kernel/main.o: $(SRC_DIR)/kernel/main.c $(SRC_DIR)/include/types.h \
                         $(SRC_DIR)/include/string.h $(SRC_DIR)/include/io.h \
                         $(SRC_DIR)/include/memory.h $(SRC_DIR)/include/shell.h \
                         $(SRC_DIR)/include/terminus_font.h $(SRC_DIR)/include/video.h
	$(CC) $(CFLAGS) -c $< -o $@

# Video library
$(SRC_DIR)/lib/video.o: $(SRC_DIR)/lib/video.c $(SRC_DIR)/include/video.h \
                       $(SRC_DIR)/include/types.h $(SRC_DIR)/include/io.h \
                       $(SRC_DIR)/include/terminus_font.h
	$(CC) $(CFLAGS) -c $< -o $@

# Kütüphane modülleri
$(SRC_DIR)/lib/string.o: $(SRC_DIR)/lib/string.c $(SRC_DIR)/include/string.h $(SRC_DIR)/include/types.h
	$(CC) $(CFLAGS) -c $< -o $@

$(SRC_DIR)/lib/io.o: $(SRC_DIR)/lib/io.c $(SRC_DIR)/include/io.h $(SRC_DIR)/include/types.h
	$(CC) $(CFLAGS) -c $< -o $@

$(SRC_DIR)/lib/memory.o: $(SRC_DIR)/lib/memory.c $(SRC_DIR)/include/memory.h $(SRC_DIR)/include/string.h $(SRC_DIR)/include/types.h
	$(CC) $(CFLAGS) -c $< -o $@

$(SRC_DIR)/lib/sha256.o: $(SRC_DIR)/lib/sha256.c $(SRC_DIR)/include/sha256.h $(SRC_DIR)/include/string.h $(SRC_DIR)/include/types.h
	$(CC) $(CFLAGS) -c $< -o $@

$(SRC_DIR)/lib/fs.o: $(SRC_DIR)/lib/fs.c $(SRC_DIR)/include/fs.h $(SRC_DIR)/include/string.h $(SRC_DIR)/include/memory.h $(SRC_DIR)/include/types.h
	$(CC) $(CFLAGS) -c $< -o $@

$(SRC_DIR)/lib/shell.o: $(SRC_DIR)/lib/shell.c $(SRC_DIR)/include/shell.h $(SRC_DIR)/include/sha256.h \
                       $(SRC_DIR)/include/string.h $(SRC_DIR)/include/io.h $(SRC_DIR)/include/memory.h $(SRC_DIR)/include/fs.h $(SRC_DIR)/include/types.h
	$(CC) $(CFLAGS) -c $< -o $@

$(SRC_DIR)/lib/python.o: $(SRC_DIR)/lib/python.c $(SRC_DIR)/include/python.h $(SRC_DIR)/include/video.h $(SRC_DIR)/include/keyboard.h $(SRC_DIR)/include/string.h $(SRC_DIR)/include/types.h
	$(CC) $(CFLAGS) -c $< -o $@

$(SRC_DIR)/lib/keyboard.o: $(SRC_DIR)/lib/keyboard.c $(SRC_DIR)/include/keyboard.h $(SRC_DIR)/include/io.h $(SRC_DIR)/include/types.h
	$(CC) $(CFLAGS) -c $< -o $@

# Bootloader
boot.bin: $(BOOT_DIR)/boot.asm
	$(AS) -f bin $< -o $@

$(BOOT_DIR)/kernel_entry.o: $(BOOT_DIR)/kernel_entry.asm
	$(AS) -f elf32 $< -o $@

font.psf: gohufont.h
	python3 - <<'PY'
	import re
	from pathlib import Path
	src = Path('gohufont.h').read_text()
	vals = [int(v, 16) for v in re.findall(r'0x[0-9a-fA-F]+', src)]
	if len(vals) < 32:
	    raise SystemExit('font source too small')
	length = vals[4]
	charsize = vals[5]
	total = 32 + length * charsize
	Path('font.psf').write_bytes(bytearray(vals[:total]))
	PY

font.o: font.psf
	$(OBJCOPY) -I binary -O elf32-i386 -B i386 $< $@ \
	--redefine-sym _binary_font_psf_start=font_psf

# Çalıştırma seçenekleri
run: os-image.bin
	qemu-system-i386 -drive format=raw,file=$<

run-new: os-image.bin
	qemu-system-i386 -drive format=raw,file=$< -boot c

run-debug: os-image.bin
	qemu-system-i386 -drive format=raw,file=$< -s -S & gdb

# Temizleme
clean:
	rm -rf *.bin $(BOOT_DIR)/*.o $(KERNEL_DIR)/*.o $(SRC_DIR)/lib/*.o $(SRC_DIR)/kernel/*.o os-image.bin font.o

# Yeni yapı için temizleme
clean-new:
	rm -rf *.bin $(SRC_DIR)/lib/*.o $(SRC_DIR)/kernel/*.o os-image.bin font.o

# Sadece eski yapıyı temizle
clean-old:
	rm -rf *.bin $(BOOT_DIR)/*.o $(KERNEL_DIR)/*.o os-image.bin font.o

# Derleme kontrolü
check:
	@echo "=== Derleme Kontrolü ==="
	@echo "CC: $(CC)"
	@echo "LD: $(LD)"
	@echo "AS: $(AS)"
	@echo "CFLAGS: $(CFLAGS)"
	@echo "LDFLAGS: $(LDFLAGS)"
	@echo "=== Dosya Kontrolü ==="
	@ls -la $(BOOT_SOURCES) 2>/dev/null || echo "Boot dosyaları eksik"
	@ls -la $(KERNEL_SOURCES) 2>/dev/null || echo "Kernel dosyaları eksik"
	@ls -la $(LIB_SOURCES) 2>/dev/null || echo "Kütüphane dosyaları eksik"
