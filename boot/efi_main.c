#include <efi.h>

typedef struct {
    UINT32 *framebuffer;
    UINT32  width;
    UINT32  height;
    UINT32  pitch;
} gop_info_t;

void keyboard_init(void *st);
void kernel_main(gop_info_t *gop_info);

EFI_STATUS EFIAPI efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    /* BS ve Print yerine direkt SystemTable kullan */
    EFI_BOOT_SERVICES *bs = SystemTable->BootServices;

    /* Klavyeyi başlat */
    keyboard_init(SystemTable);

    /* GOP'u bul */
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop;
    EFI_GUID gop_guid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
    EFI_STATUS status = bs->LocateProtocol(&gop_guid, (void*)0, (void**)&gop);

    if (status != 0) {
        SystemTable->ConOut->OutputString(SystemTable->ConOut, L"GOP bulunamadi!\r\n");
        return status;
    }

    SystemTable->ConOut->OutputString(SystemTable->ConOut, L"GOP bulundu!\r\n");

    static gop_info_t info;
    info.framebuffer = (UINT32*)gop->Mode->FrameBufferBase;
    info.width       = gop->Mode->Info->HorizontalResolution;
    info.height      = gop->Mode->Info->VerticalResolution;
    info.pitch       = gop->Mode->Info->PixelsPerScanLine;

    kernel_main(&info);

    while (1) __asm__ volatile("hlt");
    return 0;
}
