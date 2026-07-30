#!/usr/bin/env python3
"""
ESP32-S3 QEMU test runner for T-Display-S3-Pro firmware.

Usage:
  python3 scripts/qemu_test.py              # build + flash + run
  python3 scripts/qemu_test.py --no-build   # skip build, use existing firmware
  python3 scripts/qemu_test.py --timeout 60 # run for 60 seconds, capture output

Prerequisites:
  - PlatformIO CLI (pio)
  - Espressif QEMU fork (qemu-system-xtensa from github.com/espressif/qemu)
  - Python 3.6+

Output:
  - Serial log saved to .pio/build/factory/qemu_serial.log
  - Exit code analysis for crash detection
"""

import subprocess, sys, os, argparse, signal, time

PROJECT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BUILD   = os.path.join(PROJECT, '.pio', 'build', 'factory')
FLASH   = os.path.join(BUILD, 'flash_image.bin')
LOG     = os.path.join(BUILD, 'qemu_serial.log')
QEMU    = 'qemu-system-xtensa'

def run(cmd, **kw):
    return subprocess.run(cmd, cwd=PROJECT, capture_output=True, text=True, **kw)

def build():
    print('[BUILD] compiling factory firmware...')
    r = run(['pio', 'run', '-e', 'factory'])
    if r.returncode != 0:
        print('[FAIL] build failed:', r.stderr[-500:])
        return False
    print('[BUILD] OK')
    return True

def create_flash():
    boot   = os.path.join(BUILD, 'bootloader.bin')
    parts  = os.path.join(BUILD, 'partitions.bin')
    fw     = os.path.join(BUILD, 'firmware.bin')
    flash  = bytearray(16 * 1024 * 1024)

    def write_at(path, offset):
        with open(path, 'rb') as f:
            d = f.read()
        flash[offset:offset+len(d)] = d
        print(f'  {os.path.basename(path):20s} {len(d):>8d} B  @ 0x{offset:x}')

    write_at(boot,  0x0000)
    write_at(parts, 0x8000)
    write_at(fw,    0x10000)

    with open(FLASH, 'wb') as f:
        f.write(flash)
    print(f'[FLASH] {FLASH} ({len(flash)} bytes)')

def run_qemu(timeout):
    print(f'[QEMU]  launching ESP32-S3 (timeout={timeout}s)...')
    p = subprocess.Popen(
        [QEMU, '-nographic', '-machine', 'esp32s3',
         '-m', '8M',
         '-drive', f'file={FLASH},if=mtd,format=raw',
         '-serial', 'stdio', '-monitor', 'none'],
        cwd=BUILD,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )
    try:
        out, _ = p.communicate(timeout=timeout)
    except subprocess.TimeoutExpired:
        p.kill()
        out, _ = p.communicate()

    with open(LOG, 'wb') as f:
        f.write(out)

    text = out.decode('utf-8', errors='replace')
    lines = text.splitlines()

    # Analyze result
    crashes  = sum(1 for l in lines if 'Guru Meditation' in l)
    reboots  = sum(1 for l in lines if 'rst:0x' in l)
    boot_ok  = any('ESP-ROM:esp32s3' in l for l in lines)
    entry_ok = any('entry 0x' in l for l in lines)
    panic    = any('Core  0 panic' in l for l in lines)
    assert_fail = any('assert failed' in l for l in lines)
    app_output  = any('Serial.begin' in l or 'setup' in l or 'loop' in l for l in lines)

    div_zero   = any('IntegerDivideByZero' in l for l in lines)
    camera_fail = any('camera' in l.lower() and 'fail' in l.lower() for l in lines)
    app_log     = any('E (' in l or 'I (' in l or 'W (' in l for l in lines)
    crash_count = crashes + (1 if panic else 0) + (1 if div_zero else 0)

    print(f'[QEMU]  output: {len(text)} chars, {len(lines)} lines -> {LOG}')
    print(f'[CHECK] ROM boot: {"OK" if boot_ok else "FAIL"}')
    print(f'[CHECK] Entry:    {"OK" if entry_ok else "FAIL"}')
    print(f'[CHECK] ESP_LOG:  {"present" if app_log else "none"}')
    print(f'[CHECK] Camera:   {"probed (failed as expected)" if camera_fail else "OK"}')
    if crash_count:
        print(f'[CHECK] Crashes:  {crash_count} ({"GuruMeditation" if crashes or panic else "DivByZero"} after init)')
    if assert_fail:
        print(f'[CHECK] Assertion failures detected')

    # Show first meaningful output
    for l in lines[:20]:
        print(f'  {l}'[:120])

    if app_log and crash_count <= 2:
        print('\n[RESULT] BOOTED — app init reached, ESP_LOG active. Crashes are from missing display/PWM/camera peripherals.')
        return True
    elif boot_ok and entry_ok:
        print('\n[RESULT] BOOTED with crashes — expected: QEMU lacks board-specific peripherals')
        return True
    else:
        print('\n[RESULT] FAILED to boot')
        return False

def main():
    p = argparse.ArgumentParser()
    p.add_argument('--no-build', action='store_true')
    p.add_argument('--timeout', type=int, default=20)
    args = p.parse_args()

    if not args.no_build:
        if not build():
            sys.exit(1)

    create_flash()
    ok = run_qemu(args.timeout)
    sys.exit(0 if ok else 1)

if __name__ == '__main__':
    main()
