"""Offline contract tests for the experimental PS5 10.xx request layout."""
import ctypes
import os
import shutil
import struct
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HARNESS = r'''
#include <stdint.h>
void *memset(void *d, int value, unsigned long long n) {
 unsigned char *p = d;
 while (n--) *p++ = (unsigned char)value;
 return d;
}
void *memcpy(void *d, const void *s, unsigned long long n) {
 unsigned char *p = d;
 const unsigned char *q = s;
 while (n--) *p++ = *q++;
 return d;
}
static void u32(unsigned char *p, int offset, uint32_t v) { memcpy(p + offset, &v, 4); }
static void u64(unsigned char *p, int offset, uint64_t v) { memcpy(p + offset, &v, 8); }
#include "fw10-capture.h"
#ifdef _WIN32
#define EXPORT __declspec(dllexport)
#else
#define EXPORT __attribute__((visibility("default")))
#endif
EXPORT int pb_fw10(uint32_t fw) { return fw10_capture_layout(fw); }
EXPORT uint32_t pb_make(unsigned char *p, uint32_t fps, uint32_t bitrate, uint32_t output_mode) {
 return fw10_prepare_video_request(p, fps, bitrate, output_mode);
}
EXPORT uint32_t pb_size(void) { return FW10_REQUEST_SIZE; }
EXPORT uint32_t pb_header_size(void) { return FW10_HEADER_SIZE; }
'''

def compiler():
    if os.name == "nt":
        result = shutil.which("clang")
        if result:
            return result
        candidate = Path(os.environ.get("LLVM_BIN", r"C:\Program Files\LLVM\bin")) / "clang.exe"
        return str(candidate) if candidate.is_file() else None
    return shutil.which("cc") or shutil.which("clang")

@unittest.skipUnless(compiler(), "requires a C compiler")
class Firmware10LayoutTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory()
        source = Path(cls.temp.name) / "fw10-test.c"
        source.write_text(HARNESS, encoding="ascii")
        out = Path(cls.temp.name) / ("fw10.dll" if os.name == "nt" else "fw10.so")
        if os.name == "nt":
            flags = ["--target=x86_64-pc-windows-msvc", "-O1", "-fno-builtin",
                     "-nostdlib", "-shared", "-fuse-ld=lld", "-Wl,/noentry"]
        else:
            flags = ["-std=c11", "-O1", "-fno-builtin", "-shared", "-fPIC"]
        subprocess.run([compiler(), *flags, "-Wall", "-Wextra", "-Werror",
                        "-I", str(ROOT / "payload"), str(source), "-o", str(out)],
                       check=True, capture_output=True, text=True)
        cls.lib = ctypes.WinDLL(str(out)) if os.name == "nt" else ctypes.CDLL(str(out))
        for name in ("pb_fw10", "pb_make", "pb_size", "pb_header_size"):
            getattr(cls.lib, name).restype = ctypes.c_uint32

    @classmethod
    def tearDownClass(cls):
        if os.name == "nt":
            ctypes.windll.kernel32.FreeLibrary.argtypes = [ctypes.c_void_p]
            ctypes.windll.kernel32.FreeLibrary(cls.lib._handle)
        cls.temp.cleanup()

    def test_1020_uses_pbs_1001_family(self):
        for fw in (0x10010000, 0x10200000, 0x10200004, 0x10600006):
            self.assertEqual(self.lib.pb_fw10(fw), 1, hex(fw))
        for fw in (0x10000000, 0x10800000, 0x13600007):
            self.assertEqual(self.lib.pb_fw10(fw), 0, hex(fw))

    def test_request_layout_and_bitrate(self):
        self.assertEqual(self.lib.pb_size(), 0x2b8)
        self.assertEqual(self.lib.pb_header_size(), 0x188)
        p = (ctypes.c_ubyte * 0x2e8)()
        rate = self.lib.pb_make(p, 60, 8000000, 0x138)
        data = bytes(p)
        field = lambda offset: struct.unpack_from("<I", data, offset)[0]
        qfield = lambda offset: struct.unpack_from("<Q", data, offset)[0]
        self.assertEqual(rate, 8000000)
        self.assertEqual(qfield(0), 0x2b8)
        self.assertEqual((field(0x18), field(0x28)), (4, 12))
        self.assertEqual((qfield(0xa8), qfield(0xb0)), (0xd8, 0x20))
        self.assertEqual((field(0xb8), field(0xbc)), (8000000, 8000000))
        self.assertEqual((qfield(0x1d0), qfield(0x1d8)), (0xe8, 0x28))
        self.assertEqual((field(0x1ec), field(0x1f0), field(0x1f4)), (384, 320, 512))
        self.assertEqual(qfield(0x1f8), 0x3c000)
        self.assertEqual(data[0x208:0x288], bytes([255]) * 0x80)
        self.assertEqual(qfield(0x200), 0x88)
        self.assertEqual(field(0x288), 0x138)
        self.assertEqual(field(0x28c), 2)
        self.assertEqual(qfield(0x290), 0x10)
        self.assertEqual((field(0x298), data[0x29c]), (1, 1))
        self.assertEqual((qfield(0x2a0), data[0x2a8], field(0x2ac)), (0x10, 1, 110))

    def test_1080p_bitrate_ceiling(self):
        p = (ctypes.c_ubyte * 0x2e8)()
        self.assertEqual(self.lib.pb_make(p, 60, 80000000, 0x138), 30000000)
        self.assertEqual(struct.unpack_from("<I", bytes(p), 0xb8)[0], 30000000)

if __name__ == "__main__":
    unittest.main()
