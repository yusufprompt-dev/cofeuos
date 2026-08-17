<div align="center">

# 🖥️ CofeuOS

**Sıfırdan yazılmış, UEFI üzerinde çalışan minimal bir x86_64 işletim sistemi**

*Kendi kernel'i, kendi dosya sistemi, kendi shell'i, kendi masaüstü ortamı ve gömülü Python yorumlayıcısıyla.*

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-UEFI%20x86__64-informational)](#)
[![Language](https://img.shields.io/badge/language-C%20%2F%20NASM-orange)](#)
[![Status](https://img.shields.io/badge/status-active%20development-yellow)](#)

</div>

---

## 📖 İçindekiler

- [Nedir Bu?](#-nedir-bu)
- [Öne Çıkan Özellikler](#-öne-çıkan-özellikler)
- [Mimari](#-mimari)
- [Proje Yapısı](#-proje-yapısı)
- [Gereksinimler](#-gereksinimler)
- [Derleme](#-derleme)
- [QEMU ile Test](#-qemu-ile-test)
- [Gerçek Donanıma Kurulum](#-gerçek-donanıma-kurulum)
- [Shell Komutları](#-shell-komutları)
- [cofeuDE — Masaüstü Ortamı](#-cofeude--masaüstü-ortamı)
- [Ağ Yığını](#-ağ-yığını)
- [Klavye Kısayolları](#-klavye-kısayolları)
- [Yol Haritası](#-yol-haritası)
- [Katkıda Bulunma](#-katkıda-bulunma)
- [Lisans](#-lisans)

---

## 🚀 Nedir Bu?

**CofeuOS**, herhangi bir BIOS/Legacy boot katmanına ya da mevcut bir çekirdeğe (Linux, vb.) dayanmadan, doğrudan **UEFI** üzerinden **GOP (Graphics Output Protocol)** kullanarak açılan, sıfırdan yazılmış bir işletim sistemi projesidir. Amaç; boot aşamasından dosya sistemine, kullanıcı girişinden ağ yığınına, komut satırından pencereli bir masaüstü ortamına kadar tüm katmanları kendi başına inşa etmektir.

Proje hâlâ aktif geliştirme aşamasındadır ve bir "hobi kernel"in ötesine geçip; TCP/IP yığını, HTTP(S) istemcisi, `pacman` benzeri bir paket yöneticisi ve gömülü bir Python yorumlayıcısı gibi gerçek dünyada işe yarayan bileşenler barındırır.

## ✨ Öne Çıkan Özellikler

| Kategori | Açıklama |
|---|---|
| 🔌 **Boot** | BIOS gerektirmeyen saf **UEFI** boot, `gnu-efi` üzerinden GOP grafik çıkışı |
| 🧠 **Çekirdek** | 64-bit x86_64 mimarisi, kendi bellek yöneticisi (`memory.c`) |
| 🗂️ **Dosya Sistemi** | Dahili, sıfırdan yazılmış dosya sistemi (`fs.c`) — `ls`, `cd`, `mkdir`, `rm`, `cat`, `touch` |
| 💻 **Shell** | Zengin komut kümesine sahip dahili kabuk (`shell.c`, ~2000 satır) |
| 🖼️ **GUI** | `startx` ile açılan pencereli masaüstü ortamı **cofeuDE** |
| 🌐 **Ağ** | Sıfırdan TCP/IP yığını: ARP, DNS, TCP, HTTP/HTTPS, `wget`, `curl`, `ping` |
| 📦 **Paket Yönetimi** | `pacman` tarzı komutlar (`-S`, `-Sy`, `-Syu`, `-Ss`) |
| 🐍 **Python** | Gömülü **MicroPython** yorumlayıcısı, shell içinden çalıştırılabilir |
| 🔐 **Güvenlik** | Dahili **SHA-256** implementasyonu, kullanıcı girişi / `passwd` |
| 🔤 **Yazı Tipi** | PSF2 ve gohufont/terminus font desteği (`gen_font.py` ile üretilir) |
| 🪟 **Bölünmüş Terminal** | `Ctrl+P` ile ekranı ikiye bölme desteği |
| 📝 **Editörler** | Dahili `vim` / `nano` benzeri metin editörleri |

## 🏗️ Mimari

```
┌─────────────────────────────────────────────┐
│   UEFI Firmware (OVMF / gerçek donanım)      │
└───────────────────┬───────────────────────────┘
                     │  boot/efi_main.c
                     ▼
┌─────────────────────────────────────────────┐
│  GOP ile grafik modu + kernel'e sıçrama      │
│  boot/boot.asm · boot/gdt.asm                │
│  boot/kernel_entry.asm                       │
└───────────────────┬───────────────────────────┘
                     ▼
┌─────────────────────────────────────────────┐
│               CofeuOS Kernel                 │
│  src/kernel/main.c                           │
│  ├─ video.c      → GOP framebuffer render    │
│  ├─ memory.c     → bellek arena yönetimi     │
│  ├─ keyboard.c   → klavye sürücüsü           │
│  ├─ fs.c         → dahili dosya sistemi      │
│  ├─ string.c/io.c→ libc benzeri yardımcılar  │
│  ├─ sha256.c     → hash implementasyonu      │
│  ├─ network.c    → ARP/DNS/TCP/HTTP(S)       │
│  ├─ python.c     → MicroPython köprüsü       │
│  └─ shell.c      → kabuk + cofeuDE masaüstü  │
└─────────────────────────────────────────────┘
```

## 📂 Proje Yapısı

```
cofeuos/
├── boot/                    # UEFI giriş noktası ve düşük seviye asm
│   ├── efi_main.c
│   ├── boot.asm
│   ├── gdt.asm
│   └── kernel_entry.asm
├── src/
│   ├── kernel/main.c        # Kernel giriş noktası, login, ana döngü
│   ├── include/              # Header dosyaları (fs, network, shell, video…)
│   ├── lib/                  # Çekirdek modül implementasyonları
│   │   ├── fs.c               # Dosya sistemi
│   │   ├── shell.c            # Shell + cofeuDE GUI (en büyük modül)
│   │   ├── network.c          # TCP/IP yığını
│   │   ├── video.c            # GOP framebuffer sürücüsü
│   │   ├── memory.c           # Bellek yönetimi
│   │   ├── keyboard.c         # Klavye sürücüsü
│   │   ├── sha256.c           # SHA-256
│   │   ├── string.c / io.c    # Yardımcı kütüphaneler
│   │   └── python.c           # MicroPython köprüsü
│   ├── micropython_embed/    # Gömülü MicroPython kaynakları
│   └── uefi_stdlib.c
├── gnu-efi                  # gnu-efi bağımlılık referansı
├── gen_font.py               # PSF2 font üretici script
├── font_data.c / gohufont.h  # Üretilmiş font verisi
├── chkstk.c                  # Stack-check yardımcı fonksiyonu (Windows target ABI için gerekli)
├── Makefile
├── LICENSE                   # GPL-3.0
└── README.md
```

## ⚙️ Gereksinimler

| Araç | Amaç |
|---|---|
| `clang` | C derleyici (Windows/EFI hedefi ile) |
| `lld` | Bağlayıcı (`lld-link`) |
| `nasm` | Assembly (`boot.asm`, `gdt.asm`, `kernel_entry.asm`) |
| `gnu-efi` | EFI header'ları ve yardımcı kütüphaneler |
| `python3` | Font üretimi (`gen_font.py`) için |
| `qemu-system-x86_64` | Sanal ortamda test için |
| `edk2-ovmf` | UEFI firmware emülasyonu (OVMF) için |

**Arch Linux üzerinde kurulum:**

```bash
sudo pacman -S clang lld nasm python3 qemu-system-x86 edk2-ovmf
yay -S gnu-efi
```

## 🔨 Derleme

```bash
# 1) Gerekliyse chkstk.o üretilir (yoksa derleme hata verir)
make

# 2) Framebuffer fontunu üret
python3 gen_font.py

# 3) MicroPython nesnelerini derle (hataları filtrelemek için)
make $(find src/micropython_embed -name "*.c" | sed 's/\.c$/.o/') 2>&1 | grep "error:" | head -10

# 4) UEFI çalıştırılabilir dosyasını (BOOTX64.EFI) üret
make BOOTX64.EFI

```

> 💡 **İpucu:** Derleme hedefi `x86_64-unknown-windows` PE/COFF ABI'sini kullanır (UEFI'nin gerektirdiği format), bu yüzden `clang` + `lld-link` kombinasyonu gereklidir; klasik ELF toolchain'i burada işe yaramaz.

## 🧪 QEMU ile Test

```bash
# 64MB'lık FAT32 disk imajı oluştur
dd if=/dev/zero of=uefi-disk.img bs=1M count=64
mkfs.fat -F 32 uefi-disk.img

# EFI dosya sistemini hazırla
mkdir -p /tmp/cofeu_mnt
sudo mount -o loop uefi-disk.img /tmp/cofeu_mnt
sudo mkdir -p /tmp/cofeu_mnt/EFI/BOOT
sudo cp BOOTX64.EFI /tmp/cofeu_mnt/EFI/BOOT/
sudo umount /tmp/cofeu_mnt

# OVMF ile boot et
qemu-system-x86_64 \
  -bios /usr/share/edk2/x64/OVMF.4m.fd \
  -drive format=raw,file=uefi-disk.img \
  -m 256M
```

## 💾 Gerçek Donanıma Kurulum

> ⚠️ Aşağıdaki komutlar hedef USB sürücüsünün **tamamen sıfırlanmasına** neden olur. Doğru cihaz yolunu (`/dev/sdX1`) seçtiğinizden emin olun.

```bash
sudo mkfs.fat -F 32 /dev/sdX1
sudo mount /dev/sdX1 /mnt/usb
sudo mkdir -p /mnt/usb/EFI/BOOT
sudo cp BOOTX64.EFI /mnt/usb/EFI/BOOT/
sudo umount /mnt/usb
```

Ardından bilgisayarı UEFI boot menüsünden USB'yi seçerek başlatın.

## 🖥️ Shell Komutları

CofeuOS'un dahili kabuğu, klasik bir Unix shell'inin temel komutlarını ve kendine özgü birçok ek özelliği bir araya getirir.

### Dosya sistemi & genel

| Komut | Açıklama |
|---|---|
| `ls` | Dizin içeriğini listeler |
| `cd` | Dizin değiştirir |
| `pwd` | Bulunulan dizini gösterir |
| `mkdir` / `rmdir` | Dizin oluşturur / siler |
| `touch` | Boş dosya oluşturur |
| `rm` | Dosya siler |
| `cat` | Dosya içeriğini gösterir |
| `write` | Dosyaya içerik yazar |
| `vim` / `nano` | Dahili metin editörleri |
| `tar` / `untar` / `ustar` / `unzip` | Arşiv işlemleri |
| `df` | Disk kullanımını gösterir |
| `clear` | Ekranı temizler |

### Sistem & oturum

| Komut | Açıklama |
|---|---|
| `whoami` / `passwd` / `sudo` | Kullanıcı ve yetki yönetimi |
| `uname` / `sysinfo` / `neofetch` | Sistem bilgisi |
| `ps` / `free` / `uptime` | Süreç / bellek / çalışma süresi bilgisi |
| `env` | Ortam değişkenlerini listeler |
| `date` | Tarih/saat gösterir |
| `theme` | Terminal temasını değiştirir |
| `about` / `help` | Sistem hakkında bilgi / yardım |
| `reboot` / `halt` / `hlt` | Yeniden başlatma / kapatma |

### Ağ

| Komut | Açıklama |
|---|---|
| `ifconfig` | Ağ arayüzü bilgisini gösterir |
| `ping` | ICMP ping testi |
| `nettest` | Ağ yığını teşhis testi |
| `nslookup` | DNS çözümleme |
| `curl` / `wget` | HTTP(S) istekleri ve dosya indirme |

### Paket yönetimi

| Komut | Açıklama |
|---|---|
| `pacman -S <paket>` | Paket kurar |
| `pacman -Ss <sorgu>` | Paket arar |
| `pacman -Sy` | Paket veritabanını günceller |
| `pacman -Syu` | Sistemi tamamen günceller |

### Diğer

| Komut | Açıklama |
|---|---|
| `python` | Gömülü MicroPython yorumlayıcısını başlatır |
| `calc` | Hesap makinesi |
| `startx` | cofeuDE masaüstü ortamını başlatır |
| `apps` | Uygulama listesini gösterir |

## 🪟 cofeuDE — Masaüstü Ortamı

`startx` komutu ile açılan **cofeuDE**, GOP framebuffer üzerine çizilen, pencere tabanlı hafif bir masaüstü ortamıdır. Aynı anda birden fazla pencere yönetebilir ve şu pencere tiplerini destekler:

- 🖳 **Terminal** — tam işlevli, GUI içinde gömülü kabuk penceresi
- 📁 **Dosyalar** — dosya sistemi gezgini
- 📝 **Notlar** — basit not defteri
- ℹ️ **Bilgi** — sistem bilgisi paneli
- 🧮 **Hesap Makinesi** — GUI tabanlı `calc`

## 🌐 Ağ Yığını

CofeuOS, üçüncü parti bir ağ kütüphanesine dayanmadan **kendi TCP/IP yığınını** içerir:

- **ARP** çözümleme ve teşhis (`arp_test`)
- **DHCP** ile otomatik IP alma (`network_dhcp`)
- **DNS** çözümleme (`dns_resolve`)
- **TCP** bağlantı kurma/kapatma (`tcp_connect` / `tcp_disconnect`)
- **HTTP GET/POST** ve **HTTPS GET** istemcisi
- **`wget`** ile dosya indirme desteği

> 🔒 Not: HTTPS istekleri, bir TLS sağlayıcısı bağlı olmadığında `NETWORK_ERR_TLS_UNAVAILABLE` hatasını döndürür — TLS katmanı henüz gömülü değildir.

## ⌨️ Klavye Kısayolları

| Kısayol | Etki |
|---|---|
| `Ctrl + P` | Terminali ikiye böler (split view) |
| `Ctrl + X` | Açık split'i kapatır |

## 🗺️ Yol Haritası

- [X] TLS/SSL desteği ile tam `https_get` implementasyonu
- [ ] Kalıcı depolama için gerçek bir disk dosya sistemi (FAT32/ext benzeri) sürücüsü
- [X] cofeuDE için daha fazla yerleşik uygulama
- [ ] Çoklu görev (multitasking) desteği
- [ ] Genişletilmiş paket yöneticisi altyapısı (`pacman`)

## 🤝 Katkıda Bulunma

Katkılar memnuniyetle karşılanır! Bir pull request göndermeden önce:

1. Depoyu fork'layın ve yeni bir dal (branch) oluşturun.
2. Değişikliklerinizi [Derleme](#-derleme) bölümündeki adımlarla yerel olarak test edin.
3. Mümkünse QEMU üzerinde açılış testinden geçirin.
4. Açık ve açıklayıcı bir commit mesajıyla PR gönderin.

Hata bildirimleri ve özellik istekleri için **Issues** sekmesini kullanabilirsiniz.

## 📄 Lisans

Bu proje **GNU General Public License v3.0** ile lisanslanmıştır. Ayrıntılar için [LICENSE](LICENSE) dosyasına bakınız.

---

<div align="center">

*CofeuOS — sıfırdan, meraktan ve inatla yazılmış bir işletim sistemi.* 🧡

</div>
