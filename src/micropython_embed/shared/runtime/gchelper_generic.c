#include "py/mpstate.h"
#include "py/gc.h"
#include "shared/runtime/gchelper.h"

#if MICROPY_ENABLE_GC
MP_NOINLINE void gc_helper_collect_regs_and_stack(void) {
    /* Basit stack tarama — register kaydetme olmadan */
    volatile uintptr_t dummy;
    void **stack_ptr = (void **)&dummy;
    gc_collect_root(stack_ptr, 
        ((uintptr_t)MP_STATE_THREAD(stack_top) - (uintptr_t)stack_ptr) / sizeof(uintptr_t));
}
#endif
