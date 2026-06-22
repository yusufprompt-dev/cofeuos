# CofeuOS v3.0 - x86 Freestanding Kernel

Basit x86 tabanlı, Unix-benzeri shell komutları sunan bir freestanding kernel ve bootloader projesi.

## 🎯 Özellikler

- **VESA Grafik Modunu Destekler**: BIOS VBE lineer framebuffer ile 32-bit grafik, VGA 13h fallback
- **cofeuDE Desktop Environment**: `desktop` veya `startx` ile açılan basit pencereli masaüstü
- **Login Sistemi**: Kullanıcı adı girişi ve doğrulaması
- **Unix-benzeri Shell**: Komut satırı arayüzü `user@host [partition] path # ` formatında
- **x86 Assembly Bootloader**: BIOS'tan grafik moduna güvenli geçiş
- **Modüler Kod Yapısı**: Kütüphane tabanlı mimarı (string, io, memory, fs, sha256, shell, video)
- **Boyut Optimized**: ~30KB kernel, 62 sektörde (~31KB)

---

## 📁 Proje Yapısı ve Dosya Konumları

```
CofeuOS/
├── boot/                   # Bootloader ve x86 Assembly
│   ├── boot.asm           # Ana bootloader (0x7c00'de çalışır)
│   │                       # - Diskten kernel'ı yükler (BIOS int 0x13)
│   │                       # - VESA VBE LFB modunu başlatır, VGA fallback içerir
│   │                       # - 16-bit mode'den protected mode'e geçiş hazırlar
│   ├── kernel_entry.asm   # 16→32-bit geçiş, protected mode, kernel çağrısı
│   │                       # - GDT setup
│   │                       # - Protected mode (CR0.PE=1)
│   │                       # - kernel_main() çağrısı
│   └── gdt.asm            # Global Descriptor Table (GDT)
│
├── src/                   # Kaynak Kodlar (Modüler)
│   ├── kernel/
│   │   └── main.c         # **Kernel Entry Point**
│   │                       # - video_init() ile grafik başlatma
│   │                       # - show_login() ile login akışı
│   │                       # - Shell loop'u komut çalıştırma
│   │                       # - Fonksiyonlar: read_key, next_line, get_login_input, 
│   │                       #   print_shell_prompt, main_get_input
│   │                       # Nereden çağrılır: boot/kernel_entry.asm
│   │
│   ├── lib/               # Kütüphane Modülleri
│   │   ├── video.c        # **VESA Grafik Sürücüsü**
│   │   │                   # - video_init(): VBE framebuffer + font başlatma
│   │   │                   # - video_clear(): Ekran temizleme
│   │   │                   # - video_fill_rect(), video_draw_rect(): UI çizimi
│   │   │                   # - video_draw_char(): PSF2 glyph çizimi
│   │   │                   # - video_print(): String yazdırma
│   │   │                   # - video_scroll(): Satır kaydırma
│   │   │                   # Neresi: main.c'den çağrılır
│   │   │
│   │   ├── io.c           # **Giriş/Çıkış Kontrolü**
│   │   │                   # - inb/outb: Port I/O işlemleri
│   │   │                   # - Keyboard controller (0x60, 0x64)
│   │   │                   # - Basit port echo
│   │   │
│   │   ├── string.c       # **Dize İşlemleri**
│   │   │                   # - strcpy, strlen, strcmp, strncmp
│   │   │                   # - strcpy_n: Güvenli copy
│   │   │                   # - String utilities
│   │   │
│   │   ├── memory.c       # **Bellek Yönetimi**
│   │   │                   # - mem_init(): Arena başlatma (0x100000)
│   │   │                   # - kmalloc/kfree: Dinamik bellek
│   │   │                   # Neresi: kernel_main()'de mem_init() çağrısı
│   │   │
│   │   ├── fs.c           # **Dosya Sistemi**
│   │   │                   # - fs_init(): FS başlatma
│   │   │                   # - Komutlar: ls, cd, pwd, touch, mkdir
│   │   │                   # Neresi: shell.c komutlarından çağrılır
│   │   │
│   │   ├── shell.c        # **Kabuk ve Komut Executive**
│   │   │                   # - shell_execute(): Komut parser ve çalıştırıcı
│   │   │                   # - cmd_*: 25+ yerleşik komut
│   │   │                   # - Komutlar: help, ls, cd, pwd, whoami, uname,
│   │   │                   #   cat, touch, mkdir, clear, neofetch, 
│   │   │                   #   calc, write, sysinfo, apps, desktop/startx,
│   │   │                   #   reboot, halt, date, uptime, free, ps, df, env
│   │   │                   # Neresi: kernel_main() shell loop'unda
│   │   │
│   │   └── sha256.c       # **SHA-256 Hash (Güvenlik)**
│   │                       # - sha256 hash algoritması
│   │                       # - Password hashing için hazır
│   │
│   └── include/           # Header Dosyaları (API Tanımlamaları)
│       ├── types.h        # **Temel Veri Türleri**
│       │                   # - u8, u16, u32, i32 vb.
│       │
│       ├── video.h        # **Video API**
│       │                   # - video_* fonksiyonları deklarasyon
│       │                   # - extern variables: font_width, font_height
│       │
│       ├── io.h           # **I/O API**
│       │                   # - inb(), outb() deklarasyon
│       │
│       ├── string.h       # **String API**
│       │                   # - strcpy, strlen vb. deklarasyon
│       │
│       ├── memory.h       # **Memory API**
│       │                   # - kmalloc, kfree deklarasyon
│       │
│       ├── fs.h           # **Dosya Sistemi Yapısı**
│       │                   # - fs_control_block, File struct
│       │
│       ├── shell.h        # **Kabuk Yapısı**
│       │                   # - shell_control: user, host, cwd, partition
│       │
│       ├── sha256.h       # **SHA256 API**
│       │
│       └── terminus_font.h # **Font Yapısı**
│                           # - psf2_t: PSF2 header struktur
│                           # - extern font_psf, font_psf_len
│
├── kernel/                # Eski Kernel Dosyaları (Backup)
│   ├── kernel.c          # (Yedeğe alınmış)
│   └── terminus_font.h   # (Yedeğe alınmış)
│
├── boot.bin              # Bootloader Binary (512 byte)
├── kernel.bin            # Kernel Binary (~30KB, 62 sektör)
├── os-image.bin          # Tam Boot İmajı (1MB)
├── font.psf              # PSF2 Font Binary (Makefile tarafından üretilir)
├── gohufont.h            # Font Hex Veri (font.psf üretimi için kaynak)
├── Makefile              # Derleme Kuralları
├── TODO.md               # Yapılacaklar listesi
└── README.md             # Bu dosya
```

---

## 🔧 Derleme ve Çalıştırma

### Gereksinimler

```bash
# Ubuntu/Debian
sudo apt-get install nasm qemu-system-x86 python3

# Cross-compiler (i686-elf-gcc) kurulumu
# https://os.phil-opp.com/edition-2/appendix-b-setup-rust-elf-toolchain/
```

### Derleme

```bash
cd /path/to/CofeuOS
make clean-new    # Temizle
make kernel.bin   # Derle
```

### QEMU'da Çalıştırma

```bash
make run
# veya
qemu-system-i386 -drive format=raw,file=os-image.bin
```

---

## 🖥️ Yeni Komutlar ve Desktop

Shell'de deneyebileceğin yeni komutlar:

```text
apps                 # Kurulu mini uygulamaları listeler
about                # Sistem hakkında kısa bilgi
sysinfo              # Video, kernel, bellek ve FS bilgileri
calc 12 + 30         # Basit tam sayı hesap makinesi
write notes.txt hi   # Dosyaya metin yazar
theme                # Grafik tema önizlemesi
desktop              # cofeuDE masaüstünü açar
startx               # desktop alias
```

cofeuDE içinde alt komutlar:

```text
files   # Geçerli dizini pencerede gösterir
notes   # /home/notes.txt dosyasını gösterir
info    # Sistem penceresi
home    # Ana desktop görünümüne döner
term    # Shell'e geri döner
exit    # Shell'e geri döner
```

---

## 🚀 Boot Süreci

1. **BIOS POST** → Bilgisayar başladığında BIOS 0x7c00'e kontrol verir
2. **Boot (boot/boot.asm)** → Diskten kernel'i 0x1000'e yükler (62 sektör)
3. **GDT & Protected Mode (boot/kernel_entry.asm)** → Guard bit ayarla, JMP CODE_SEG
4. **Kernel Entry (src/kernel/main.c)** → kernel_main() başlar
5. **Video Init** → VESA framebuffer ve PSF2 font yükle
6. **Login Screen** → Kullanıcı adı/parola girişi
7. **Shell Loop** → Komut satırı `user@host [partition] path # ` göster
8. **Command Parsing** → shell_execute() ile komut çalıştır

---

## 🎛️ Sistem Mimarisi

| Bileşen | Adres | Boyut | Açıklama |
|--------|-------|-------|----------|
| Bootloader | 0x7C00 | 512 byte | BIOS tarafından yüklenir |
| Kernel | 0x1000 | ~30KB | Protected mode'de çalışır |
| Video Buffer | VBE LFB | BIOS bildirir | VESA lineer framebuffer |
| Heap | 0x100000 | 16MB | Dinamik bellek alanı |

---

## 📊 Derleme Ayarları

```makefile
CC = i686-elf-gcc
CFLAGS = -ffreestanding -Os -Wall -Wextra -fno-stack-protector -m32 \
         -ffunction-sections -fdata-sections
LDFLAGS = -Ttext 0x1000 --oformat binary --gc-sections
```

- `-Os`: **Boyut optimizasyonu** (kernel'ı bootloader sektör okumasına sığdırmak için)
- `-ffreestanding`: İşletim sistemi olmadan çalışan code
- `-fno-stack-protector`: Stack guard devre dışı
- `-m32`: 32-bit x86 üretimi
- `--gc-sections`: Ölü kod ve bölümleri kaldır

---

## 🔐 Güvenlik Notları

- Demo amaçlı: Login şu anda password doğrulaması yapılmıyor
- SHA256 içerilir ama henüz login'de kullanılmıyor
- Production için hash-based password comparison gerekli

---

## 🐛 Bilinen Sınırlamalar

- Dosya sistemi minimal test aşaması
- Ağ desteği yok
- Çoklu process/threading yok
- Bellek paging/virtual memory yok
- Interrupt handler sistemin temeli değil, dummy yazılmış

---

## 📝 Lisans

GNU GPLv3

---

**Not**: Bu proje freestanding kernel öğrenmesi ve deneme amaçlıdır. Production sistemi değildir.
