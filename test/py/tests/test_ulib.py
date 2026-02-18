# SPDX-License-Identifier: GPL-2.0+
# Copyright (c) 2025, Canonical Ltd.

"""Test U-Boot library functionality"""

import os
import shutil
import subprocess
import tempfile
import pytest
import utils

def check_output(out):
    """Check output from the ulib test"""
    assert 'Hello, world from ub_printf' in out
    assert '- U-Boot' in out
    assert 'Uses libc printf before ulib_init' in out
    assert 'another printf()' in out

@pytest.mark.boardspec('sandbox')
@pytest.mark.buildconfigspec("ulib")
def test_ulib_shared(ubman):
    """Test the ulib shared library test program"""

    build = ubman.config.build_dir
    prog = os.path.join(build, 'test', 'ulib', 'ulib_test')

    # ulib is not yet supported with clang
    if ubman.config.buildconfig.get('config_cc_is_clang'):
        pytest.skip('ulib not supported with clang')

    assert os.path.exists(prog), 'ulib_test not found in build dir'

    out = utils.run_and_log(ubman, [prog], cwd=build)
    check_output(out)
    assert 'dynamically linked' in out

@pytest.mark.boardspec('sandbox')
def test_ulib_static(ubman):
    """Test the ulib static library test program"""

    build = ubman.config.build_dir
    prog = os.path.join(build, 'test', 'ulib', 'ulib_test_static')

    # ulib is not yet supported with clang
    if ubman.config.buildconfig.get('config_cc_is_clang'):
        pytest.skip('ulib not supported with clang')

    assert os.path.exists(prog), 'ulib_test_static not found in build directory'

    out = utils.run_and_log(ubman, [prog])
    check_output(out)
    assert 'statically linked' in out

def check_demo_output(ubman, out):
    """Check output from the ulib demo programs exactly line by line"""
    lines = out.split('\n')

    # Read the actual system version from /proc/version
    with open('/proc/version', 'r', encoding='utf-8') as f:
        proc_version = f.read().strip()

    # demo.c uses U-Boot's printf (compiled with U-Boot headers) while
    # demo_helper.c uses glibc's printf, so their output streams are
    # buffered separately.  The helper output appears first, then the
    # U-Boot printf output is flushed at exit.
    expected = [
        'U-Boot Library Demo Helper\r',
        '==========================\r',
        'helper: Adding 42 + 13 = 55\r',
        '=================================\r',
        'Demo complete\r',
        '\r',
        f'System version:U-Boot version: {ubman.u_boot_version_string}',
        '',
        f'  {proc_version}',
        '',
        'Read 1 line(s) using U-Boot library functions.',
        ''
    ]

    assert len(lines) == len(expected), \
        f"Expected {len(expected)} lines, got {len(lines)}"

    for i, expected in enumerate(expected):
        # Exact match for all other lines
        assert lines[i] == expected, \
            f"Line {i}: expected '{expected}', got '{lines[i]}'"

def check_rust_demo_output(_ubman, out):
    """Check output from the Rust ulib demo programs exactly line by line"""
    lines = out.split('\n')

    # Read the actual system version from /proc/version
    with open('/proc/version', 'r', encoding='utf-8') as f:
        proc_version = f.read().strip()

    # Check individual parts of the output
    assert len(lines) == 13, f"Expected 13 lines, got {len(lines)}"

    assert lines[0] == 'U-Boot Library Demo Helper\r'
    assert lines[1] == '==========================\r'
    assert lines[2].startswith('U-Boot version: ') and lines[2].endswith('\r')
    assert lines[3] == '\r'
    assert lines[4] == 'System version:\r'
    assert lines[5] == f'  {proc_version}\r'
    assert lines[6] == '\r'
    assert lines[7] == 'Read 1 line(s) using U-Boot library functions.\r'
    assert lines[8] == 'helper: Adding 42 + 13 = 55\r'
    assert lines[9] == 'Helper function result: 55\r'
    assert lines[10] == '=================================\r'
    assert lines[11] == 'Demo complete (hi from rust)\r'
    assert lines[12] == ''

@pytest.mark.boardspec('sandbox')
def test_ulib_demos(ubman):
    """Test both ulib demo programs (dynamic and static)."""

    build = ubman.config.build_dir
    src = ubman.config.source_dir
    examples = os.path.join(src, 'examples', 'ulib')
    test_program = os.path.join(build, 'test', 'ulib', 'ulib_test')

    # ulib is not yet supported with clang
    if ubman.config.buildconfig.get('config_cc_is_clang'):
        pytest.skip('ulib not supported with clang')

    assert os.path.exists(test_program), 'ulib_test not found in build dir'

    # Build the demo programs - clean first to ensure fresh build, since this
    # test is run in the source directory
    cmd = ['make', 'clean']
    utils.run_and_log(ubman, cmd, cwd=examples)

    cmd = ['make', f'UBOOT_BUILD={os.path.abspath(build)}', f'srctree={src}']
    utils.run_and_log(ubman, cmd, cwd=examples)

    # Test static demo program
    demo_static = os.path.join(examples, 'demo_static')
    out_static = utils.run_and_log(ubman, [demo_static])
    check_demo_output(ubman, out_static)

    # Test dynamic demo program (with proper LD_LIBRARY_PATH)
    demo = os.path.join(examples, 'demo')
    env = os.environ.copy()
    env['LD_LIBRARY_PATH'] = os.path.abspath(build)
    out_dynamic = utils.run_and_log(ubman, [demo], env=env)
    check_demo_output(ubman, out_dynamic)

@pytest.mark.boardspec('sandbox')
def test_ulib_rust_demos(ubman):
    """Test both Rust ulib demo programs (dynamic and static)."""

    build = ubman.config.build_dir
    src = ubman.config.source_dir
    examples = os.path.join(src, 'examples', 'rust')
    test_program = os.path.join(build, 'test', 'ulib', 'ulib_test')

    # ulib is not yet supported with clang
    if ubman.config.buildconfig.get('config_cc_is_clang'):
        pytest.skip('ulib not supported with clang')

    assert os.path.exists(test_program), 'ulib_test not found in build dir'

    # Check if cargo is available
    try:
        utils.run_and_log(ubman, ['cargo', '--version'])
    except (subprocess.CalledProcessError, FileNotFoundError):
        pytest.skip('cargo not found - Rust toolchain required')

    # Build the Rust demo programs - clean first to ensure fresh build
    cmd = ['make', 'clean']
    utils.run_and_log(ubman, cmd, cwd=examples)

    cmd = ['make', f'UBOOT_BUILD={os.path.abspath(build)}', f'srctree={src}']
    utils.run_and_log(ubman, cmd, cwd=examples)

    # Test static demo program
    demo_static = os.path.join(examples, 'demo_static')
    out_static = utils.run_and_log(ubman, [demo_static])
    check_rust_demo_output(ubman, out_static)

    # Test dynamic demo program (with proper LD_LIBRARY_PATH)
    demo = os.path.join(examples, 'demo')
    env = os.environ.copy()
    env['LD_LIBRARY_PATH'] = os.path.abspath(build)
    out_dynamic = utils.run_and_log(ubman, [demo], env=env)
    check_rust_demo_output(ubman, out_dynamic)

@pytest.mark.boardspec('sandbox')
def test_ulib_api_header(ubman):
    """Test that the u-boot-api.h header is generated correctly."""

    hdr = os.path.join(ubman.config.build_dir, 'include', 'u-boot-api.h')

    # ulib is not yet supported with clang
    if ubman.config.buildconfig.get('config_cc_is_clang'):
        pytest.skip('ulib not supported with clang')

    assert os.path.exists(hdr), 'u-boot-api.h not found in build directory'

    # Read and verify header content
    with open(hdr, 'r', encoding='utf-8') as inf:
        out = inf.read()

    # Check header guard
    assert '#ifndef __ULIB_API_H' in out
    assert '#define __ULIB_API_H' in out
    assert '#endif /* __ULIB_API_H */' in out

    # Check required includes
    assert '#include <stdarg.h>' in out
    assert '#include <stddef.h>' in out

    # Check for renamed function declarations
    assert 'ub_printf' in out
    assert 'ub_snprintf' in out
    assert 'ub_vprintf' in out

    # Check that functions have proper signatures
    assert 'ub_printf(const char *fmt, ...)' in out
    assert 'ub_snprintf(char *buf, size_t size, const char *fmt, ...)' in out
    assert 'ub_vprintf(const char *fmt, va_list args)' in out

def assert_demo_output(out):
    """Assert that demo output contains expected strings.

    Args:
        out (str): Decoded output string from QEMU
    """
    assert 'U-Boot Library Demo Helper' in out
    assert '==========================' in out
    assert 'U-Boot version:' in out
    assert 'helper: Adding 42 + 13 = 55' in out
    assert '=================================' in out
    assert 'Demo complete' in out

def run_qemu_demo(qemu_cmd, timeout=5):
    """Run a ulib demo under QEMU and return output.

    Args:
        qemu_cmd (list): QEMU command and arguments (e.g.
                  ['qemu-system-i386', '-bios', 'demo.rom', ...])
        timeout (int): Seconds to wait before killing QEMU (default 5)

    Returns:
        str: Decoded stdout from QEMU
    """
    with subprocess.Popen(qemu_cmd, stdout=subprocess.PIPE,
                          stderr=subprocess.PIPE) as proc:
        try:
            stdout, _ = proc.communicate(timeout=timeout)
        except subprocess.TimeoutExpired:
            proc.kill()
            stdout, _ = proc.communicate()

    return stdout.decode('utf-8', errors='replace')

def run_x86_rom_demo(ubman, qemu_binary):
    """Boot the demo ROM image under QEMU and check for expected output.

    Locates demo.rom in the build directory, launches the given QEMU
    binary with it, and asserts that the expected demo output is present.

    Args:
        ubman (ConsoleBase): Test fixture providing build directory
            etc.
        qemu_binary (str): QEMU system binary
            (e.g. 'qemu-system-i386')
    """
    build = ubman.config.build_dir
    demo_rom = os.path.join(build, 'demo.rom')

    assert os.path.exists(demo_rom), 'demo.rom not found in build directory'
    assert shutil.which(qemu_binary), f'{qemu_binary} not found'

    cmd = [qemu_binary, '-bios', demo_rom, '-nographic', '-no-reboot']
    out = run_qemu_demo(cmd)
    assert_demo_output(out)

@pytest.mark.localqemu
@pytest.mark.boardspec('qemu-x86')
@pytest.mark.buildconfigspec("examples")
def test_ulib_demo_rom(ubman):
    """Test the ulib demo ROM image under QEMU x86."""
    run_x86_rom_demo(ubman, 'qemu-system-i386')

@pytest.mark.localqemu
@pytest.mark.boardspec('qemu-x86_64_nospl')
@pytest.mark.buildconfigspec("examples")
def test_ulib_demo_rom_64(ubman):
    """Test the ulib demo ROM image under QEMU x86_64."""
    run_x86_rom_demo(ubman, 'qemu-system-x86_64')

def run_x86_rom_rust_demo(ubman, qemu_binary):
    """Boot the Rust demo ROM image under QEMU and check for expected output.

    Locates rust-demo.rom in the build directory, launches the given QEMU
    binary with it, and asserts that the expected demo output is present.

    Args:
        ubman (ConsoleBase): Test fixture providing build directory
            etc.
        qemu_binary (str): QEMU system binary
            (e.g. 'qemu-system-x86_64')
    """
    build = ubman.config.build_dir
    demo_rom = os.path.join(build, 'rust-demo.rom')

    assert os.path.exists(demo_rom), \
        'rust-demo.rom not found in build directory'
    assert shutil.which(qemu_binary), f'{qemu_binary} not found'

    cmd = [qemu_binary, '-bios', demo_rom, '-nographic', '-no-reboot']
    out = run_qemu_demo(cmd)
    assert_demo_output(out)

@pytest.mark.localqemu
@pytest.mark.boardspec('qemu-x86')
@pytest.mark.buildconfigspec("rust_examples")
def test_ulib_rust_demo_rom(ubman):
    """Test the Rust ulib demo ROM image under QEMU x86."""
    run_x86_rom_rust_demo(ubman, 'qemu-system-i386')

@pytest.mark.localqemu
@pytest.mark.boardspec('qemu-x86_64_nospl')
@pytest.mark.buildconfigspec("rust_examples")
def test_ulib_rust_demo_rom_64(ubman):
    """Test the Rust ulib demo ROM image under QEMU x86_64."""
    run_x86_rom_rust_demo(ubman, 'qemu-system-x86_64')

def run_bios_demo(ubman, qemu_binary, extra_qemu_args=None):
    """Boot the demo.bin binary under QEMU and check for expected output.

    Locates demo.bin in the build directory, launches the given QEMU
    binary with it as -bios, and asserts that the expected demo output
    is present.

    Args:
        ubman (ConsoleBase): Test fixture providing build directory
            etc.
        qemu_binary (str): QEMU system binary
            (e.g. 'qemu-system-aarch64')
        extra_qemu_args (list): Additional QEMU arguments
            (e.g. ['-cpu', 'cortex-a57'])
    """
    build = ubman.config.build_dir
    demo_bin = os.path.join(build, 'examples', 'ulib', 'demo.bin')

    assert os.path.exists(demo_bin), 'demo.bin not found in build directory'
    assert shutil.which(qemu_binary), f'{qemu_binary} not found'

    cmd = [qemu_binary, '-machine', 'virt', '-nographic', '-no-reboot',
           '-bios', demo_bin]
    if extra_qemu_args:
        cmd += extra_qemu_args
    out = run_qemu_demo(cmd)
    assert_demo_output(out)

def run_bios_rust_demo(ubman, qemu_binary, extra_qemu_args=None):
    """Boot the rust-demo.bin binary under QEMU and check expected output.

    Locates rust-demo.bin in the build directory, launches the given QEMU
    binary with it as -bios, and asserts that the expected demo output
    is present.

    Args:
        ubman (ConsoleBase): Test fixture providing build directory
            etc.
        qemu_binary (str): QEMU system binary
            (e.g. 'qemu-system-riscv64')
        extra_qemu_args (list): Additional QEMU arguments
    """
    build = ubman.config.build_dir
    demo_bin = os.path.join(build, 'examples', 'ulib', 'rust-demo.bin')

    assert os.path.exists(demo_bin), \
        'rust-demo.bin not found in build directory'
    assert shutil.which(qemu_binary), f'{qemu_binary} not found'

    cmd = [qemu_binary, '-machine', 'virt', '-nographic', '-no-reboot',
           '-bios', demo_bin]
    if extra_qemu_args:
        cmd += extra_qemu_args
    out = run_qemu_demo(cmd)
    assert_demo_output(out)

@pytest.mark.localqemu
@pytest.mark.boardspec('qemu_arm64')
@pytest.mark.buildconfigspec("examples")
def test_ulib_demo_arm64(ubman):
    """Test the ulib demo binary under QEMU ARM64."""
    run_bios_demo(ubman, 'qemu-system-aarch64', ['-cpu', 'cortex-a57'])

@pytest.mark.localqemu
@pytest.mark.boardspec('qemu-riscv64')
@pytest.mark.buildconfigspec("examples")
def test_ulib_demo_riscv64(ubman):
    """Test the ulib demo binary under QEMU RISC-V 64."""
    run_bios_demo(ubman, 'qemu-system-riscv64')

@pytest.mark.localqemu
@pytest.mark.boardspec('qemu-riscv64')
@pytest.mark.buildconfigspec("rust_examples")
def test_ulib_rust_demo_riscv64(ubman):
    """Test the Rust ulib demo binary under QEMU RISC-V 64."""
    run_bios_rust_demo(ubman, 'qemu-system-riscv64')

def run_efi_demo(ubman, qemu_binary, fw_code, fw_vars, extra_qemu_args=None):
    """Run a ulib demo EFI application under QEMU with UEFI firmware.

    Writes a startup.nsh script next to demo-app.efi in the build
    directory, boots QEMU with the given firmware, and checks for
    expected output.

    Args:
        ubman (ConsoleBase): Test fixture providing build directory
            etc.
        qemu_binary (str): QEMU system binary name
            (e.g. 'qemu-system-x86_64')
        fw_code (str): Path to UEFI firmware code file
        fw_vars (str): Path to UEFI firmware variables file (or None)
        extra_qemu_args (list): Additional QEMU arguments
    """
    build = ubman.config.build_dir
    efi_dir = os.path.join(build, 'examples', 'ulib')
    demo_efi = os.path.join(efi_dir, 'demo-app.efi')

    assert os.path.exists(demo_efi), 'demo-app.efi not found in build directory'
    assert shutil.which(qemu_binary), f'{qemu_binary} not found'
    assert os.path.exists(fw_code), f'UEFI firmware not found: {fw_code}'

    with open(os.path.join(efi_dir, 'startup.nsh'), 'w',
              encoding='utf-8') as nsh:
        nsh.write('fs0:demo-app.efi\n')

    cmd = [qemu_binary]

    # Set up firmware pflash drives
    cmd += ['-drive', f'if=pflash,format=raw,file={fw_code},readonly=on']
    if fw_vars:
        vars_copy = os.path.join(efi_dir, 'vars.fd')
        shutil.copy(fw_vars, vars_copy)
        cmd += ['-drive', f'if=pflash,format=raw,file={vars_copy}']

    if extra_qemu_args:
        cmd += extra_qemu_args

    # FAT drive with EFI binary and startup script
    cmd += ['-drive', f'file=fat:rw:{efi_dir},format=raw',
            '-nographic', '-no-reboot', '-nic', 'none']
    out = run_qemu_demo(cmd, timeout=15)
    assert_demo_output(out)

def run_efi_rust_demo(ubman, qemu_binary, fw_code, fw_vars,
                      extra_qemu_args=None):
    """Run a Rust ulib demo EFI application under QEMU with UEFI firmware.

    Writes a startup.nsh script next to rust-demo-app.efi in the build
    directory, boots QEMU with the given firmware, and checks for
    expected output.

    Args:
        ubman (ConsoleBase): Test fixture providing build directory
            etc.
        qemu_binary (str): QEMU system binary name
            (e.g. 'qemu-system-x86_64')
        fw_code (str): Path to UEFI firmware code file
        fw_vars (str): Path to UEFI firmware variables file (or None)
        extra_qemu_args (list): Additional QEMU arguments
    """
    build = ubman.config.build_dir
    efi_dir = os.path.join(build, 'examples', 'ulib')
    demo_efi = os.path.join(efi_dir, 'rust-demo-app.efi')

    assert os.path.exists(demo_efi), \
        'rust-demo-app.efi not found in build directory'
    assert shutil.which(qemu_binary), f'{qemu_binary} not found'
    assert os.path.exists(fw_code), f'UEFI firmware not found: {fw_code}'

    with open(os.path.join(efi_dir, 'startup.nsh'), 'w',
              encoding='utf-8') as nsh:
        nsh.write('fs0:rust-demo-app.efi\n')

    cmd = [qemu_binary]

    # Set up firmware pflash drives
    cmd += ['-drive', f'if=pflash,format=raw,file={fw_code},readonly=on']
    if fw_vars:
        vars_copy = os.path.join(efi_dir, 'vars.fd')
        shutil.copy(fw_vars, vars_copy)
        cmd += ['-drive', f'if=pflash,format=raw,file={vars_copy}']

    if extra_qemu_args:
        cmd += extra_qemu_args

    # FAT drive with EFI binary and startup script
    cmd += ['-drive', f'file=fat:rw:{efi_dir},format=raw',
            '-nographic', '-no-reboot', '-nic', 'none']
    out = run_qemu_demo(cmd, timeout=15)
    assert_demo_output(out)

@pytest.mark.localqemu
@pytest.mark.boardspec('efi-x86_app64')
@pytest.mark.buildconfigspec("examples")
def test_ulib_demo_efi_x86(ubman):
    """Test the ulib demo EFI application under QEMU x86_64 with OVMF."""
    run_efi_demo(ubman, 'qemu-system-x86_64',
                 '/usr/share/OVMF/OVMF_CODE_4M.fd',
                 '/usr/share/OVMF/OVMF_VARS_4M.fd')

@pytest.mark.localqemu
@pytest.mark.boardspec('efi-x86_app64')
@pytest.mark.buildconfigspec("rust_examples")
def test_ulib_rust_demo_efi_x86(ubman):
    """Test the Rust ulib demo EFI app under QEMU x86_64 with OVMF."""
    run_efi_rust_demo(ubman, 'qemu-system-x86_64',
                      '/usr/share/OVMF/OVMF_CODE_4M.fd',
                      '/usr/share/OVMF/OVMF_VARS_4M.fd')

@pytest.mark.localqemu
@pytest.mark.boardspec('efi-arm_app64')
@pytest.mark.buildconfigspec("examples")
def test_ulib_demo_efi_arm64(ubman):
    """Test the ulib demo EFI application under QEMU aarch64 with UEFI."""
    run_efi_demo(ubman, 'qemu-system-aarch64',
                 '/usr/share/qemu-efi-aarch64/QEMU_EFI.fd', None,
                 ['--machine', 'virt', '-cpu', 'max'])

@pytest.mark.localqemu
@pytest.mark.boardspec('efi-arm_app64')
@pytest.mark.buildconfigspec("rust_examples")
def test_ulib_rust_demo_efi_arm64(ubman):
    """Test the Rust ulib demo EFI app under QEMU aarch64 with UEFI."""
    run_efi_rust_demo(ubman, 'qemu-system-aarch64',
                      '/usr/share/qemu-efi-aarch64/QEMU_EFI.fd', None,
                      ['--machine', 'virt', '-cpu', 'max'])

@pytest.mark.localqemu
@pytest.mark.boardspec('efi-riscv_app64')
@pytest.mark.buildconfigspec("examples")
def test_ulib_demo_efi_riscv64(ubman):
    """Test the ulib demo EFI application under QEMU RISC-V 64 with UEFI."""
    run_efi_demo(ubman, 'qemu-system-riscv64',
                 '/usr/share/qemu-efi-riscv64/RISCV_VIRT_CODE.fd',
                 '/usr/share/qemu-efi-riscv64/RISCV_VIRT_VARS.fd',
                 ['--machine', 'virt'])

@pytest.mark.localqemu
@pytest.mark.boardspec('efi-riscv_app64')
@pytest.mark.buildconfigspec("rust_examples")
def test_ulib_rust_demo_efi_riscv64(ubman):
    """Test the Rust ulib demo EFI app under QEMU RISC-V 64 with UEFI."""
    run_efi_rust_demo(ubman, 'qemu-system-riscv64',
                      '/usr/share/qemu-efi-riscv64/RISCV_VIRT_CODE.fd',
                      '/usr/share/qemu-efi-riscv64/RISCV_VIRT_VARS.fd',
                      ['--machine', 'virt'])
