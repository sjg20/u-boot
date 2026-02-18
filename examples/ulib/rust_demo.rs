// SPDX-License-Identifier: GPL-2.0+
//
// Rust demo program showing U-Boot library functionality
//
// Demonstrates calling C helper functions from Rust via FFI, producing
// identical output to demo.c so assert_demo_output() works unchanged.
//
// Copyright 2025 Canonical Ltd.

#![no_std]
#![no_main]

use core::ffi::c_int;

extern "C" {
    fn printf(fmt: *const u8, ...) -> c_int;
    fn demo_show_banner();
    fn demo_show_footer();
    fn demo_add_numbers(a: c_int, b: c_int) -> c_int;
    static version_string: u8;
}

#[no_mangle]
pub extern "C" fn ulib_has_main() -> bool {
    true
}

/// Call demo_add_numbers() matching the target's C calling convention.
///
/// On 32-bit x86, U-Boot compiles with -mregparm=3 so the first three
/// integer arguments are passed in eax, edx, ecx.  Rust's extern "C"
/// uses cdecl (all arguments on the stack), so we bridge via inline
/// assembly.  Variadic functions (printf) and functions with no
/// arguments are unaffected.
#[cfg(target_arch = "x86")]
unsafe fn call_demo_add_numbers(a: c_int, b: c_int) -> c_int {
    let result: c_int;
    core::arch::asm!(
        "call {func}",
        func = sym demo_add_numbers,
        inout("eax") a => result,
        in("edx") b,
        out("ecx") _,
    );
    result
}

#[cfg(not(target_arch = "x86"))]
unsafe fn call_demo_add_numbers(a: c_int, b: c_int) -> c_int {
    demo_add_numbers(a, b)
}

fn demo_run() -> c_int {
    unsafe {
        demo_show_banner();
        printf(
            b"U-Boot version: %s\n\0".as_ptr(),
            &version_string as *const u8,
        );
        printf(b"\n\0".as_ptr());
        call_demo_add_numbers(42, 13);
        demo_show_footer();
    }
    0
}

#[no_mangle]
pub extern "C" fn main() -> c_int {
    demo_run()
}

#[panic_handler]
fn panic(_: &core::panic::PanicInfo) -> ! {
    loop {}
}
