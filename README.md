<<<<<<< HEAD
# CofeuOS

CofeuOS, sıfırdan yazılmış minimal bir UEFI işletim sistemidir. GOP (Graphics Output Protocol) kullanarak modern UEFI sistemlerde çalışır.

## Özellikler

- UEFI GOP desteği (BIOS gerektirmez)
- 64-bit x86 mimarisi
- PSF2 font desteği
- Dahili shell (ls, cd, mkdir, rm, cat, touch, vim, nano...)
- SHA256 desteği
- Dahili dosya sistemi
- Kullanıcı girişi (login)
- Split terminal (Ctrl+P)
- Minimal Python yorumlayıcı

## Derleme

### Gereksinimler
- clang
- lld
- nasm
- gnu-efi
- python3
- qemu (test için)
- edk2-ovmf (test için)

Arch Linux:
```bash
sudo pacman -S clang lld nasm python3 qemu-system-x86 edk2-ovmf
yay -S gnu-efi
```

### Derleme

```bash
make  # eğer chkstk.o yoksa [EĞER CHKSTK.O YOKSA HATA VERIR]
python3 gen_font.py # fontu olusturma
make BOOTX64.EFI # Uefi için .efi dosyası olusturma
make $(find src/micropython_embed -name "*.c" | sed 's/\.c$/.o/') 2>&1 | grep "error:" | head -10 # MicroPython derlemesi hatayi gosterir
```

### QEMU ile Test

```bash
dd if=/dev/zero of=uefi-disk.img bs=1M count=64
mkfs.fat -F 32 uefi-disk.img
mkdir -p /tmp/cofeu_mnt
sudo mount -o loop uefi-disk.img /tmp/cofeu_mnt
sudo mkdir -p /tmp/cofeu_mnt/EFI/BOOT
sudo cp BOOTX64.EFI /tmp/cofeu_mnt/EFI/BOOT/
sudo umount /tmp/cofeu_mnt
qemu-system-x86_64 \
  -bios /usr/share/edk2/x64/OVMF.4m.fd \
  -drive format=raw,file=uefi-disk.img \
  -m 256M
```

## Gerçek Makineye Kurulum

```bash
# USB'yi hazırla
sudo mkfs.fat -F 32 /dev/sdX1
sudo mount /dev/sdX1 /mnt/usb
sudo mkdir -p /mnt/usb/EFI/BOOT
sudo cp BOOTX64.EFI /mnt/usb/EFI/BOOT/
sudo umount /mnt/usb
```

Bilgisayarı USB'den boot et.

## Shell Komutları

| Komut | Açıklama |
|-------|----------|
| ls | Dosyaları listele |
| cd | Dizin değiştir |
| mkdir | Dizin oluştur |
| rm | Dosya sil |
| cat | Dosya içeriğini göster |
| touch | Dosya oluştur |
| vim / nano | Metin editörü |
| clear | Ekranı temizle |
| neofetch | Sistem bilgisi |
| reboot | Yeniden başlat |
| halt | Kapat |
| help | Yardım |

## Kısayollar

- `Ctrl+P` — Terminal'i ikiye böl
- `Ctrl+X` — Split'i kapat

## Lisans

GNU GPL v3
=======
# cofeuos
CofeuOS Again!
>>>>>>> d3c6de0e60ed61c3905fb3033df4ba60a690777b
