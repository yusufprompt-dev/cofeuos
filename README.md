# CofeuOS

CofeuOS, sıfırdan yazılmış minimal bir **UEFI işletim sistemidir**. BIOS gerektirmeden, GOP (Graphics Output Protocol) kullanarak modern UEFI sistemlerde çalışır. Unix benzeri bir kabuk (shell), basit bir dosya sistemi, gömülü bir Python yorumlayıcısı ve temel bir masaüstü ortamı (cofeuDE) içerir.

> **Not:** CofeuOS deneysel/eğitim amaçlı bir projedir. `date`, `uptime`, `free`, `df`, `pacman` gibi bazı komutlar gerçek sistem verisi döndürmez, sabit (mock) çıktı üretir — aşağıda ilgili bölümlerde belirtilmiştir.

## İçindekiler

- [Özellikler](#özellikler)
- [Gereksinimler](#gereksinimler)
- [Derleme](#derleme)
- [QEMU ile Test](#qemu-ile-test)
- [Gerçek Makineye Kurulum](#gerçek-makineye-kurulum)
- [Giriş (Login)](#giriş-login)
- [Shell Komutları](#shell-komutları)
- [Ağ Kullanımı: curl, wget, ping, nslookup](#ağ-kullanımı-curl-wget-ping-nslookup)
- [Python Yorumlayıcısı](#python-yorumlayıcısı)
- [cofeuDE (Masaüstü)](#cofeude-masaüstü)
- [Kısayollar](#kısayollar)
- [Bilinen Sınırlamalar](#bilinen-sınırlamalar)
- [Lisans](#lisans)

## Özellikler

- UEFI GOP desteği (BIOS gerektirmez)
- 64-bit x86 mimarisi
- PSF2 font desteği
- Dahili Unix benzeri shell (50'den fazla komut)
- Bellek içi (RAM) dosya sistemi
- SHA256 desteği
- Kullanıcı girişi (login) ekranı
- Ağ desteği: `ping`, `curl`, `wget`, `ifconfig`, `nslookup`, `nettest`
- Gömülü MicroPython yorumlayıcısı (`python` / `python3`)
- ZIP ve TAR arşiv çıkarma (`unzip`, `untar`)
- Basit grafik masaüstü ortamı: cofeuDE (`desktop` / `startx`)
- Split terminal (`Ctrl+P`)

## Gereksinimler

- `clang`
- `lld` (Makefile `lld-link`'i kullanır)
- `llvm-objcopy` (genelde `llvm` paketiyle gelir)
- `nasm`
- `gnu-efi`
- `python3`
- `xorriso` (ISO oluşturmak için — `make` / `make iso` hedefleri bunu kullanır)
- `qemu-system-x86_64` (test için)
- `edk2-ovmf` / `ovmf` (test için, UEFI firmware imajı sağlar)

Arch Linux:

```bash
sudo pacman -S clang lld llvm nasm python3 xorriso qemu-system-x86 edk2-ovmf
yay -S gnu-efi
```

## Derleme

Repoyu klonladıktan sonra proje kökünde:

```bash
# 1. Font verisini oluştur (font_data.c bunu üretir, ilk derlemeden önce gerekli)
python3 gen_font.py

# 2. chkstk nesnesini derle
make chkstk.o

# 3. Sadece EFI ikilisini derle
make BOOTX64.EFI

# 4. ISO imajı oluştur (varsayılan hedef, xorriso gerektirir)
make
# veya açıkça:
make iso
```

Bu adımların sonunda proje kökünde `BOOTX64.EFI` ve `CofeuOS-x86_64.iso` dosyaları oluşur.

> MicroPython derlemesinde hata alırsanız, tek tek hangi dosyanın patladığını görmek için:
> ```bash
> make $(find src/micropython_embed -name "*.c" | sed 's/\.c$/.o/') 2>&1 | grep "error:" | head -10
> ```

Derleme çıktısını temizlemek için:

```bash
make clean
```

## QEMU ile Test

Makefile bunu otomatikleştiren hazır hedefler içeriyor — manuel disk imajı hazırlamanıza gerek yok:

```bash
make run
```

Bu komut `BOOTX64.EFI`'yi derler, 64MB'lık bir FAT32 disk imajı (`uefi-disk.img`) oluşturur, `sudo` ile mount edip EFI ikilisini `EFI/BOOT/` altına kopyalar ve QEMU'yu ağ desteğiyle (`e1000` NIC, kullanıcı modu ağ) başlatır.

**Önemli:** `make run` hedefi OVMF firmware'ini `/usr/share/ovmf/OVMF.fd` yolunda arar. Dağıtımınıza göre bu yol farklı olabilir (örn. Arch'ta `edk2-ovmf` paketiyle genelde `/usr/share/edk2-ovmf/x64/OVMF.4m.fd` veya benzeri bir yola kurulur). Yol sizde farklıysa `Makefile` içindeki `run` hedefindeki `-bios` satırını kendi OVMF yolunuza göre güncelleyin.

Manuel olarak aynı işlemi yapmak isterseniz:

```bash
dd if=/dev/zero of=uefi-disk.img bs=1M count=64
mkfs.fat -F 32 uefi-disk.img
mkdir -p /tmp/cofeu_mnt
sudo mount -o loop uefi-disk.img /tmp/cofeu_mnt
sudo mkdir -p /tmp/cofeu_mnt/EFI/BOOT
sudo cp BOOTX64.EFI /tmp/cofeu_mnt/EFI/BOOT/
sudo umount /tmp/cofeu_mnt

qemu-system-x86_64 \
  -bios /usr/share/ovmf/OVMF.fd \
  -drive format=raw,file=uefi-disk.img \
  -m 256M \
  -netdev user,id=net0 -device e1000,netdev=net0
```

`-netdev user,...` satırı QEMU'nun kullanıcı modu (SLIRP) ağını etkinleştirir; bu, `curl`/`wget`/`ping`/`nslookup` komutlarının çalışması için gereklidir (bkz. [Ağ Kullanımı](#ağ-kullanımı-curl-wget-ping-nslookup)).

## Gerçek Makineye Kurulum

```bash
# USB'yi hazırla (X harfini kendi USB aygıtınızla değiştirin, örn. sdb)
sudo mkfs.fat -F 32 /dev/sdX1
sudo mount /dev/sdX1 /mnt/usb
sudo mkdir -p /mnt/usb/EFI/BOOT
sudo cp BOOTX64.EFI /mnt/usb/EFI/BOOT/
sudo umount /mnt/usb
```

Bilgisayarı USB'den (UEFI modunda) boot edin.

> ⚠️ Bu adımlar hedef diski biçimlendirir. Yanlış aygıt yolu (`/dev/sdX1`) kullanmak veri kaybına yol açabilir — `lsblk` ile doğru USB aygıtını doğrulayın.

## Giriş (Login)

Açılışta bir kullanıcı adı/parola ekranı karşılar. Şu an gerçek bir kimlik doğrulama **yapılmaz** — girilen kullanıcı adı sadece shell içinde (`whoami`, `env`, prompt vb.) görüntülenmek üzere kaydedilir; parola alanı sadece arayüz amaçlıdır. Herhangi bir kullanıcı adı/parola ile giriş yapabilirsiniz.

## Shell Komutları

### Dosya ve dizin işlemleri

| Komut | Açıklama |
|---|---|
| `ls [dizin]` | Dosyaları/dizinleri listeler |
| `pwd` | Geçerli dizini gösterir |
| `cd <dizin>` | Dizin değiştirir |
| `mkdir <dizin>` | Dizin oluşturur |
| `rmdir <dizin>` | Boş dizini siler |
| `touch <dosya>` | Boş dosya oluşturur |
| `cat <dosya>` | Dosya içeriğini gösterir |
| `rm <dosya>` | Dosya siler |
| `write <dosya> <metin...>` | Dosyaya metin yazar |
| `nano <dosya>` / `vim <dosya>` | Metin editörü açar |

### Arşiv

| Komut | Açıklama |
|---|---|
| `unzip <dosya.zip> [hedef]` | ZIP dosyasını çıkarır |
| `untar <dosya.tar> [hedef]` (takma ad: `tar`) | Sıkıştırılmamış `.tar` (USTAR) dosyasını çıkarır — `.tar.gz` şu an desteklenmiyor |

### Sistem

| Komut | Açıklama |
|---|---|
| `whoami` | Giriş yapan kullanıcıyı gösterir |
| `uname` | Sistem/mimari bilgisi |
| `sysinfo` | OS, kernel, video modu, bellek, dosya sistemi özeti |
| `about` | Sürüm ve kısa açıklama |
| `neofetch` | ASCII sistem bilgisi ekranı |
| `date` | *(sabit değer döndürür, gerçek saat değildir)* |
| `uptime` | *(sabit değer döndürür)* |
| `free` | *(sabit değer döndürür)* |
| `df` | *(sabit değer döndürür)* |
| `ps` | *(sabit süreç listesi döndürür)* |
| `echo <metin...>` | Metni ekrana yazar |
| `env` | Ortam değişkenlerini gösterir (`USER`, `HOST`, `PATH`, `SHELL`) |
| `clear` | Ekranı temizler |
| `reboot` | Sistemi yeniden başlatır |
| `halt` | Sistemi kapatır |
| `sudo <komut>` / `rodo <komut>` | Komutu (yetki kontrolü olmadan) çalıştırır |
| `pacman -S/-Ss/-Sy/-Syu <paket>` | *(simülasyon — gerçek paket kurulumu yapmaz, sadece durum mesajı basar)* |
| `calc <a> +|-|*|/ <b>` | Basit aritmetik hesap makinesi |
| `apps` | Yüklü uygulamaları listeler |
| `theme` | Tema önizlemesi gösterir |

### Ağ

| Komut | Açıklama |
|---|---|
| `ifconfig` | MAC adresi ve atanmış IP'yi gösterir |
| `ping <ip>` | Verilen IP'ye 4 ICMP paketi gönderir |
| `nslookup <domain>` | Alan adını DNS ile çözer (DNS sunucu: `10.0.2.3`) |
| `nettest` | Ağ bağlantısını test eder |
| `curl [-v] [-o dosya] [-X POST] [-d veri] <url>` | HTTP isteği yapar |
| `wget <url>` | Dosya indirir |

Ayrıntılı kullanım için bkz. [Ağ Kullanımı](#ağ-kullanımı-curl-wget-ping-nslookup).

### Diğer

| Komut | Açıklama |
|---|---|
| `python` / `python3 [dosya.py]` | MicroPython REPL'i açar veya bir betik çalıştırır |
| `desktop` / `startx` | Grafik masaüstünü (cofeuDE) açar |
| `help` | Komut özetini gösterir |

Shell içinde her an `help` yazarak kısa bir komut özetine ulaşabilirsiniz.

## Ağ Kullanımı: curl, wget, ping, nslookup

Ağ komutlarının çalışması için CofeuOS'un ağ bağlantısı olması gerekir — QEMU'da bu, `-netdev user,id=net0 -device e1000,netdev=net0` bayraklarıyla sağlanır (yukarıdaki `make run` bunu otomatik ekler). Gerçek donanımda ağ desteği deneyseldir.

### curl

```
kullanim: curl [-v] [-o dosya] <url>
  -v        : verbose mod
  -o dosya  : sonucu verilen dosyaya kaydet
  varsayilan: mevcut dizinde index.html olarak kaydedilir
  -X POST   : POST istegi gonder
  -d veri   : POST verisi
```

Örnekler:

```bash
# Basit GET isteği, sonucu ./index.html olarak kaydeder
curl http://10.0.2.2/

# Sonucu belirli bir dosyaya kaydet
curl -o sayfa.html http://example.com/

# Ayrıntılı (verbose) çıktı
curl -v http://10.0.2.2/api/status

# POST isteği
curl -X POST -d "kullanici=test&sifre=1234" http://10.0.2.2/login
```

> `curl`, hem alan adı (domain) hem de doğrudan IP adresiyle çalışır. `http://` ve `https://` önekleri ayrıştırılır (https için gerçek TLS uygulaması olmayabilir — sadece URL ayrıştırmasını etkiler).
> QEMU kullanıcı modu ağında `10.0.2.2`, host makinenizi temsil eder; host'ta basit bir HTTP sunucusu (örn. `python3 -m http.server`) çalıştırarak `curl`/`wget`'i test edebilirsiniz.

### wget

```
kullanim: wget <url>
```

```bash
wget http://10.0.2.2/dosya.txt
wget /ornek.txt   # host adı verilmezse varsayılan olarak 10.0.2.2 kullanılır
```

### ping

```
kullanim: ping <ip>
```

```bash
ping 10.0.2.2
```

IP adresi doğrudan `a.b.c.d` biçiminde verilmelidir (alan adı çözümü yapılmaz — bunun için `nslookup` kullanın).

### nslookup

```
kullanim: nslookup <domain>
```

```bash
nslookup example.com
```

## Python Yorumlayıcısı

CofeuOS, gömülü bir MicroPython yorumlayıcısı içerir.

```bash
python          # REPL'i açar
python3 betik.py  # bir .py dosyasını çalıştırır
```

## cofeuDE (Masaüstü)

`desktop` veya `startx` komutuyla basit bir grafik masaüstü açılır. Masaüstü kendi mini komut satırını kullanır:

| Komut | Açıklama |
|---|---|
| `files` | Dosya yöneticisini açar |
| `notes` | `/home/notes.txt` içeriğini gösterir |
| `info` | Sistem bilgisi penceresini gösterir |
| `clear` / `home` | Ana ekrana döner |
| `term` / `exit` | Masaüstünden çıkıp terminale döner |

## Kısayollar

- `Ctrl+P` — Terminali ikiye böler (split terminal)
- `Ctrl+X` — Split'i kapatır

## Bilinen Sınırlamalar

- Dosya sistemi tamamen **RAM üzerinde** çalışır; kalıcı disk depolaması yoktur — her yeniden başlatmada sıfırlanır.
- `date`, `uptime`, `free`, `df`, `ps`, `pacman` komutları gerçek zamanlı/gerçek sistem verisi döndürmez; sabit (mock) çıktılar üretirler.
- Giriş (login) ekranı gerçek bir kimlik doğrulama yapmaz.
- `untar`, sıkıştırılmamış (`ustar`) `.tar` dosyalarını destekler; `.tar.gz` henüz desteklenmiyor.
- `curl` için `https://` ayrıştırılır ama gerçek bir TLS/şifreleme katmanı yoktur.
- `boot/boot.asm`, `boot/gdt.asm`, `boot/kernel_entry.asm` dosyaları repoda bulunur ancak `Makefile` içinde bu dosyaları derleyen bir hedef yoktur (kullanılmıyorlar).

## Lisans

GNU GPL v3 — ayrıntılar için [LICENSE](LICENSE) dosyasına bakın.
