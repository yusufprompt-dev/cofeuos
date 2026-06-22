[extern kernel_main] ; kernel.c içindeki fonksiyonun adı

[bits 32]
    push ebx
    call kernel_main
    jmp $
