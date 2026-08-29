#!/usr/bin/env python3
"""Build-time isolation helpers for the H4V1 post-PASS Flash probe.

The qualified H4V1 image is intentionally left source-identical.  Extra
Flash objects are moved to the end of ITCM, then one already-existing JAL
(the watchdog-complete call immediately after RESULT PASS) is redirected to
the post-pass trampoline.  The trampoline still calls the original target;
the original address is recovered from that JAL and written into a sentinel
word in the isolated tail.

Both operations fail closed: if the linker script shape, ELF layout, call
site signature, or sentinel differs from the qualified image, no output is
silently produced.
"""

from __future__ import annotations

import argparse
import pathlib
import struct
import sys


TAIL_OBJECTS = (
    "*h4v1_flash_postpass_stage1.o",
    "*h4v1_flash_postpass_stage2.o",
    "*h4v1_flash_postpass_stage3.o",
    "*h4v1_flash_postpass_stage4.o",
    "*h4v1_flash_postpass_stage5.o",
    "*h4v1_flash_postpass_stage6.o",
    "*h4v1_flash_postpass_hook.o",
    "*h4v1_flash_write_probe_hook.o",
    "*h4v1_flash_install_hook.o",
    "*h4v1_flash_coldboot*.o",
)

CALLSITE_SIGNATURES = (
    bytes.fromhex(
        "ef e0 ff 80 "  # qualified 90-frame RESULT PASS call
        "ef c0 ff be "  # watchdog-complete JAL; this word is patched
        "37 35 0b 20 "  # first instruction after the original call
        "13 05 45 bf"
    ),
    bytes.fromhex(
        "ef e0 2f fe "  # qualified 120/165-frame RESULT PASS call
        "ef c0 3f bc "  # watchdog-complete JAL; this word is patched
        "83 26 41 18 "  # first instructions of dynamic LOOP log setup
        "37 36 0b 20"
    ),
)
CALLSITE_JAL_OFFSET = 4
ORIGINAL_TARGET_SENTINEL = 0x48345031  # ASCII-ish "H4P1"
COLD_BOOT_LOOP_LOG_SENTINEL = 0x4834434C  # ASCII "H4CL"
WRITE_PROBE_165_IDENTITY = (
    b"FLASH WRITE_PROBE HOOK ENTER stage5=armed frames=165 "
    b"reference=chunk165_pass block=1015 row=0000fdc0"
)
WRITE_PROBE_165_BANNER_FROM = (
    b"H417 SDRAM VIDEO H4V1 ISOLATED v58 CHUNK165 FULL"
)
WRITE_PROBE_165_BANNER_TO = (
    b"H417 FLASH H4V1 WRITE PROBE v001 CHUNK165 STAGE5"
)
if len(WRITE_PROBE_165_BANNER_FROM) != len(WRITE_PROBE_165_BANNER_TO):
    raise RuntimeError("write-probe banner replacement must preserve layout")

FLASH_INSTALL_165_IDENTITY = (
    b"FLASH INSTALL HOOK ENTER stage6=armed frames=165 "
    b"reference=chunk165_pass blocks=768..1014 manifest=768 scratch=1015"
)
FLASH_INSTALL_165_BANNER_FROM = WRITE_PROBE_165_BANNER_FROM
FLASH_INSTALL_165_BANNER_TO = (
    b"H417 FLASH H4V1 INSTALL v001 CHUNK165 COMMIT V01"
)
if len(FLASH_INSTALL_165_BANNER_FROM) != len(FLASH_INSTALL_165_BANNER_TO):
    raise RuntimeError("Flash-install banner replacement must preserve layout")

COLD_BOOT_165_IDENTITY = (
    b"FLASH COLD BOOT HOOK ENTER readonly=1 frames=165 "
    b"reference=chunk165_pass manifest=768 payload=769..1014 scratch=1015"
)
COLD_BOOT_165_BANNER_FROM = WRITE_PROBE_165_BANNER_FROM
COLD_BOOT_165_BANNER_TO = (
    b"H417 FLASH H4V1 COLD BOOT v001 READONLY PLAY V01"
)
if len(COLD_BOOT_165_BANNER_FROM) != len(COLD_BOOT_165_BANNER_TO):
    raise RuntimeError("cold-boot banner replacement must preserve layout")

# The qualified 165-frame upload function is inlined.  These two exact
# call-site signatures are therefore the narrowest ABI-preserving way to
# substitute a read-only NAND source while leaving the USB, SDRAM, verify,
# decoder and LTDC implementation byte-identical.  Both signatures are unique
# in the qualified 165-frame .highcode image.
COLD_BOOT_LINE_SIGNATURE = bytes.fromhex(
    "13 0c 61 2b "  # addi s8,sp,694
    "93 05 00 04 "  # li a1,64
    "08 1d "        # addi a0,sp,688 (compressed)
    "ef 80 9f 95 "  # ch32h417_usb_cdc_read_line; patched
    "63 57 a0 02"   # blez a0,...
)
COLD_BOOT_RAW_SIGNATURE = bytes.fromhex(
    "ef e0 af a8 "  # qualified watchdog-context call
    "ce 9c "        # add s9,s9,s3
    "da 85 "        # mv a1,s6
    "66 85 "        # mv a0,s9
    "ef 80 cf c3 "  # ch32h417_usb_cdc_raw_rx_read; patched
    "2a 8b "        # mv s6,a0
    "ef 80 af c9"   # ch32h417_usb_cdc_raw_rx_overflowed
)
COLD_BOOT_RESULT_SIGNATURE = CALLSITE_SIGNATURES[1]
COLD_BOOT_LOOP_LOG_SIGNATURE = bytes.fromhex(
    "ef 30 ef 93 "  # qualified rt_snprintf call
    "7d 15 "        # addi a0,a0,-1
    "93 07 e0 07 "  # li a5,126
    "63 e5 a7 00 "  # bltu a5,a0,...
    "08 1d "        # addi a0,sp,688
    "ef d0 ff 90 "  # H4C LOOP logger; patched
    "26 8d "        # mv s10,s1
    "63 88 0d 00"   # beqz s11,...
)
COLD_BOOT_ACK_SIGNATURE = bytes.fromhex(
    "83 26 01 16 "  # lw a3,352(sp)
    "37 0b 00 60 "  # lui s6,0x60000
    "13 d6 ea 00 "  # srli a2,s5,14
    "da 96 "        # add a3,a3,s6
    "d6 96 "        # add a3,a3,s5
    "c5 45 "        # li a1,17 (ACK_SEND)
    "05 45 "        # li a0,1
    "32 de "        # sw a2,60(sp)
    "ef e0 2f 92 "  # watchdog-context call
    "08 1d "        # addi a0,sp,688
    "ef f0 0f a1 "  # credit-ACK logger; patched
    "83 27 01 16"   # lw a5,352(sp)
)
COLD_BOOT_LINE_JAL_OFFSET = 10
COLD_BOOT_RAW_JAL_OFFSET = 10
COLD_BOOT_RESULT_JAL_OFFSET = CALLSITE_JAL_OFFSET
COLD_BOOT_LOOP_LOG_JAL_OFFSET = 16
COLD_BOOT_ACK_JAL_OFFSET = 28

BANNER_PROFILES = (
    (
        "write-probe-165",
        WRITE_PROBE_165_IDENTITY,
        WRITE_PROBE_165_BANNER_FROM,
        WRITE_PROBE_165_BANNER_TO,
    ),
    (
        "install-165",
        FLASH_INSTALL_165_IDENTITY,
        FLASH_INSTALL_165_BANNER_FROM,
        FLASH_INSTALL_165_BANNER_TO,
    ),
    (
        "cold-boot-165",
        COLD_BOOT_165_IDENTITY,
        COLD_BOOT_165_BANNER_FROM,
        COLD_BOOT_165_BANNER_TO,
    ),
)


def die(message: str) -> "NoReturn":
    raise SystemExit(f"h4v1-postpass: {message}")


def generate_linker(input_path: pathlib.Path, output_path: pathlib.Path) -> None:
    source = input_path.read_text(encoding="utf-8")
    # The normal .highcode and every existing load address stay untouched.
    # Re-purpose the already-present (and qualified-empty) second highcode
    # copy window.  Its source is relocated to a new section appended after
    # .image_rodata; its VMA remains the old 0x200b42fc RAM_CODE cursor.
    old_lma = "        PROVIDE(_highcode_lma1 = .);"
    if source.count(old_lma) != 1:
        die("qualified second-copy LMA anchor changed")
    source = source.replace(
        old_lma,
        "        /* _highcode_lma1 is the postpass LOADADDR below. */",
        1,
    )

    old_end = "      PROVIDE(_highcode_vma_end1 = .);"
    if source.count(old_end) != 1:
        die("qualified second-copy end anchor changed")
    source = source.replace(
        old_end,
        "      /* _highcode_vma_end1 is the postpass end below. */",
        1,
    )

    image_anchor = (
        "\t.image_rodata :\n"
        "\t{\n"
        "\t\t. = ALIGN(4);\n"
        "\t\tKEEP(*(.image_rodata))\n"
        "\t\tKEEP(*(.image_rodata.*))\n"
        "\t\t. = ALIGN(4);\n"
        "\t} >FLASH AT>FLASH\n\n"
        "\t.bss :"
    )
    tail_lines = [
        "\t.image_rodata :",
        "\t{",
        "\t\t. = ALIGN(4);",
        "\t\tKEEP(*(.image_rodata))",
        "\t\tKEEP(*(.image_rodata.*))",
        "\t\t. = ALIGN(4);",
        "\t} >FLASH AT>FLASH",
        "",
        "\t.h4v1_postpass :",
        "\t{",
        "\t\t. = ALIGN(4);",
        "\t\t__h4v1_postpass_start = .;",
    ]
    for obj in TAIL_OBJECTS:
        tail_lines.append(
            f"\t\tKEEP({obj}(.h4v1_postpass.text .h4v1_postpass.text.* "
            ".h4v1_postpass.rodata .h4v1_postpass.rodata.* "
            ".h4v1_postpass.data .h4v1_postpass.data.* "
            ".h4v1_coldboot.text .h4v1_coldboot.text.* "
            ".h4v1_coldboot.rodata .h4v1_coldboot.rodata.*))"
        )
    tail_lines.extend(
        (
            "\t\t. = ALIGN(4);",
            "\t\t__h4v1_postpass_end = .;",
            "\t\tPROVIDE(_highcode_vma_end1 = .);",
            "\t} >RAM_CODE AT>FLASH",
            "\tPROVIDE(_highcode_lma1 = LOADADDR(.h4v1_postpass));",
            "\tASSERT(ADDR(.h4v1_postpass) == _highcode_vma_start1,",
            "\t       \"pre-existing second highcode copy is no longer empty\")",
            "",
            "\t.h4v1_coldboot_bss (NOLOAD) :",
            "\t{",
            "\t\t. = ALIGN(8);",
            "\t\t__h4v1_coldboot_bss_start = .;",
            "\t\tKEEP(*h4v1_flash_coldboot*.o(.h4v1_coldboot.bss "
            ".h4v1_coldboot.bss.*))",
            "\t\t. = ALIGN(8);",
            "\t\t__h4v1_coldboot_bss_end = .;",
            "\t} >RAM_CODE",
            "\tASSERT(__h4v1_coldboot_bss_end <= "
            "ORIGIN(RAM_CODE) + LENGTH(RAM_CODE),",
            "\t       \"cold-boot state exceeds the V5 ITCM region\")",
            "",
            "\t.bss :",
        )
    )
    if source.count(image_anchor) != 1:
        die("qualified image_rodata anchor changed")
    source = source.replace(image_anchor, "\n".join(tail_lines), 1)

    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(source, encoding="utf-8")


class Elf32:
    def __init__(self, path: pathlib.Path):
        self.path = path
        self.data = bytearray(path.read_bytes())
        if len(self.data) < 52 or self.data[:4] != b"\x7fELF":
            die(f"{path} is not an ELF file")
        if self.data[4] != 1 or self.data[5] != 1:
            die("only ELF32 little-endian images are supported")
        header = struct.unpack_from("<16sHHIIIIIHHHHHH", self.data, 0)
        self.phoff = header[5]
        self.shoff = header[6]
        self.phentsize = header[9]
        self.phnum = header[10]
        self.shentsize = header[11]
        self.shnum = header[12]
        self.shstrndx = header[13]
        if self.shentsize != 40 or self.shstrndx >= self.shnum:
            die("unexpected ELF32 section table")
        self.sections = [
            struct.unpack_from("<IIIIIIIIII", self.data, self.shoff + i * 40)
            for i in range(self.shnum)
        ]
        shstr = self.sections[self.shstrndx]
        self.shstr = self.data[shstr[4] : shstr[4] + shstr[5]]
        if self.phentsize != 32:
            die("unexpected ELF32 program-header table")
        self.program_headers = [
            struct.unpack_from("<IIIIIIII", self.data, self.phoff + i * 32)
            for i in range(self.phnum)
        ]

    @staticmethod
    def _cstring(blob: bytes | bytearray, offset: int) -> str:
        if offset >= len(blob):
            return ""
        end = blob.find(b"\0", offset)
        if end < 0:
            end = len(blob)
        return bytes(blob[offset:end]).decode("utf-8", errors="replace")

    def section(self, name: str):
        for section in self.sections:
            if self._cstring(self.shstr, section[0]) == name:
                return section
        die(f"missing ELF section {name}")

    def section_or_none(self, name: str):
        for section in self.sections:
            if self._cstring(self.shstr, section[0]) == name:
                return section
        return None

    def section_bytes(self, name: str) -> bytes:
        section = self.section(name)
        if section[1] == 8:  # SHT_NOBITS
            return b""
        return bytes(self.data[section[4] : section[4] + section[5]])

    def section_lma(self, name: str) -> int:
        section = self.section(name)
        address, offset, size = section[3], section[4], section[5]
        for program in self.program_headers:
            p_type, p_offset, p_vaddr, p_paddr, p_filesz = program[:5]
            if (
                p_type == 1
                and p_vaddr <= address
                and address + size <= p_vaddr + program[5]
                and p_offset <= offset
                and offset + (0 if section[1] == 8 else size) <= p_offset + p_filesz
            ):
                return p_paddr + address - p_vaddr
        die(f"section {name} is not covered by a load segment")

    def symbol_entries(self) -> dict[str, tuple[int, int]]:
        symtab = self.section(".symtab")
        if symtab[9] != 16 or symtab[6] >= self.shnum:
            die("unexpected ELF32 symbol table")
        strtab = self.sections[symtab[6]]
        names = self.data[strtab[4] : strtab[4] + strtab[5]]
        result: dict[str, tuple[int, int]] = {}
        for offset in range(symtab[4], symtab[4] + symtab[5], 16):
            name_off, value, size, _info, _other, _shndx = struct.unpack_from(
                "<IIIBBH", self.data, offset
            )
            name = self._cstring(names, name_off)
            if name:
                result[name] = (value, size)
        return result

    def symbols(self) -> dict[str, int]:
        return {name: entry[0] for name, entry in self.symbol_entries().items()}

    def address_to_offset(self, address: int, size: int = 1) -> int:
        for section in self.sections:
            sh_type, sh_addr, sh_offset, sh_size = section[1], section[3], section[4], section[5]
            if sh_type != 8 and sh_addr <= address and address + size <= sh_addr + sh_size:
                return sh_offset + address - sh_addr
        die(f"address 0x{address:08x} is not backed by an ELF section")


def decode_jal_target(pc: int, instruction: int) -> int:
    if instruction & 0x7F != 0x6F:
        die(f"instruction at 0x{pc:08x} is not JAL: 0x{instruction:08x}")
    immediate = (
        ((instruction >> 31) & 0x1) << 20
        | ((instruction >> 21) & 0x3FF) << 1
        | ((instruction >> 20) & 0x1) << 11
        | ((instruction >> 12) & 0xFF) << 12
    )
    if immediate & (1 << 20):
        immediate -= 1 << 21
    return (pc + immediate) & 0xFFFFFFFF


def encode_jal(pc: int, target: int, rd: int = 1) -> int:
    delta = target - pc
    if delta & 1 or not (-(1 << 20) <= delta < (1 << 20)):
        die(
            f"JAL target 0x{target:08x} is out of range from 0x{pc:08x}"
        )
    immediate = delta & 0x1FFFFF
    return (
        ((immediate >> 20) & 0x1) << 31
        | ((immediate >> 1) & 0x3FF) << 21
        | ((immediate >> 11) & 0x1) << 20
        | ((immediate >> 12) & 0xFF) << 12
        | (rd & 0x1F) << 7
        | 0x6F
    )


def changed_offsets(left: bytes, right: bytes) -> set[int]:
    if len(left) != len(right):
        die(f"section length changed from {len(left)} to {len(right)}")
    return {index for index, pair in enumerate(zip(left, right)) if pair[0] != pair[1]}


def verify_candidate(
    candidate: Elf32,
    reference: Elf32,
    call_pc: int,
    original_target: int,
    trampoline: int,
    banner_profile: tuple[str, bytes, bytes, bytes] | None,
) -> None:
    # Existing executable/data layout must be identical.  The only normal
    # code mutation is the one post-PASS JAL; the only startup mutations are
    # immediates for the already-existing second-copy source/end addresses.
    for name in (".highcode", ".loadcode", ".text", ".data", ".image_rodata"):
        ref_section = reference.section(name)
        candidate_section = candidate.section(name)
        for field in (1, 2, 3, 5, 8):  # type, flags, VMA, size, alignment
            if ref_section[field] != candidate_section[field]:
                die(f"qualified section metadata changed for {name}")
        if reference.section_lma(name) != candidate.section_lma(name):
            die(f"qualified section LMA changed for {name}")

    highcode = candidate.section(".highcode")
    hook_offset = call_pc - highcode[3]
    reference_highcode = reference.section_bytes(".highcode")
    candidate_highcode = candidate.section_bytes(".highcode")
    highcode_changes = changed_offsets(reference_highcode, candidate_highcode)
    allowed_highcode_changes = set(range(hook_offset, hook_offset + 4))
    if banner_profile is not None:
        profile_name, _identity, banner_from, banner_to = banner_profile
        banner_offset = candidate_highcode.find(banner_to)
        if banner_offset < 0 or candidate_highcode.count(banner_to) != 1:
            die(f"candidate {profile_name} banner is missing or not unique")
        if reference_highcode[
            banner_offset : banner_offset + len(banner_from)
        ] != banner_from:
            die(f"{profile_name} banner did not replace the qualified banner")
        allowed_highcode_changes |= set(
            range(banner_offset, banner_offset + len(banner_to))
        )
    else:
        for profile_name, _identity, _banner_from, banner_to in BANNER_PROFILES:
            if banner_to in candidate_highcode:
                die(f"unexpected {profile_name} banner without tail identity")
    if not highcode_changes or not highcode_changes <= allowed_highcode_changes:
        die(
            "qualified .highcode changed outside the post-PASS JAL: "
            + ",".join(f"0x{offset:x}" for offset in sorted(highcode_changes)[:16])
        )

    load_changes = changed_offsets(
        reference.section_bytes(".loadcode"),
        candidate.section_bytes(".loadcode"),
    )
    allowed_load_changes = set(range(0x2C, 0x34)) | set(range(0x3C, 0x44))
    if not load_changes or not load_changes <= allowed_load_changes:
        die(
            "startup changed outside second-copy address immediates: "
            + ",".join(f"0x{offset:x}" for offset in sorted(load_changes)[:16])
        )

    for name in (".text", ".data", ".image_rodata"):
        if reference.section_bytes(name) != candidate.section_bytes(name):
            die(f"qualified contents changed for {name}")

    for name in (".bss",):
        ref_section = reference.section(name)
        candidate_section = candidate.section(name)
        if (ref_section[3], ref_section[5]) != (candidate_section[3], candidate_section[5]):
            die(f"qualified VMA/size changed for {name}")

    for name in (
        ".fini",
        ".preinit_array",
        ".init_array",
        ".fini_array",
        ".ctors",
        ".dtors",
    ):
        ref_section = reference.section_or_none(name)
        candidate_section = candidate.section_or_none(name)
        ref_size = 0 if ref_section is None else ref_section[5]
        candidate_size = 0 if candidate_section is None else candidate_section[5]
        if ref_size != 0 or candidate_size != 0:
            die(f"pre-existing second-copy section {name} is not empty")

    ref_symbols = reference.symbol_entries()
    candidate_symbols = candidate.symbol_entries()
    for name in (
        "cdc_acm_data_recv",
        "ch32h417_usb_cdc_write",
        "ch32h417_dual_cdc_poll",
        "USBFS_IRQHandler",
        "sdram_memtest_watchdog_complete",
        "run_sdram_video_test",
        "__rt_init_rti_start",
        "__rt_init_rti_board_start",
        "__rt_init_rti_board_end",
        "__rt_init_finsh_system_init",
        "__rt_init_rti_end",
        "__fsymtab_start",
        "__fsymtab_end",
    ):
        if name not in ref_symbols or name not in candidate_symbols:
            die(f"qualified symbol disappeared: {name}")
        if ref_symbols[name] != candidate_symbols[name]:
            die(f"qualified symbol address/size changed: {name}")

    tail = candidate.section(".h4v1_postpass")
    ref_highcode = reference.section(".highcode")
    expected_vma = ref_highcode[3] + ref_highcode[5]
    expected_lma = reference.section_lma(".image_rodata") + reference.section(".image_rodata")[5]
    if tail[3] != expected_vma or candidate.section_lma(".h4v1_postpass") != expected_lma:
        die(
            f"postpass placement changed: VMA=0x{tail[3]:08x} "
            f"LMA=0x{candidate.section_lma('.h4v1_postpass'):08x}"
        )
    if tail[3] + tail[5] > 0x200C0000:
        die("postpass tail exceeds the V5 ITCM region")

    call_file_offset = candidate.address_to_offset(call_pc, 4)
    patched_instruction = struct.unpack_from("<I", candidate.data, call_file_offset)[0]
    if decode_jal_target(call_pc, patched_instruction) != trampoline:
        die("post-PASS JAL does not target the trampoline after patching")
    original_word = candidate.symbols()["h4v1_flash_postpass_original_call"]
    original_word_offset = candidate.address_to_offset(original_word, 4)
    if struct.unpack_from("<I", candidate.data, original_word_offset)[0] != original_target:
        die("trampoline original-call word does not preserve watchdog-complete")


def find_unique_signature(blob: bytes | bytearray,
                          signature: bytes,
                          description: str) -> int:
    hits: list[int] = []
    cursor = 0
    while True:
        found = blob.find(signature, cursor)
        if found < 0:
            break
        hits.append(found)
        cursor = found + 1
    if len(hits) != 1:
        die(f"expected one qualified {description} signature, found {len(hits)}")
    return hits[0]


def verify_coldboot_candidate(
    candidate: Elf32,
    reference: Elf32,
    patched_calls: tuple[tuple[int, int], ...],
    original_watchdog: int,
    original_loop_log: int,
    banner_profile: tuple[str, bytes, bytes, bytes],
) -> None:
    # The qualified image is immutable except for the five explicit JALs,
    # the equal-length identity banner and the existing startup copy bounds.
    for name in (".highcode", ".loadcode", ".text", ".data", ".image_rodata"):
        ref_section = reference.section(name)
        candidate_section = candidate.section(name)
        for field in (1, 2, 3, 5, 8):
            if ref_section[field] != candidate_section[field]:
                die(f"qualified section metadata changed for {name}")
        if reference.section_lma(name) != candidate.section_lma(name):
            die(f"qualified section LMA changed for {name}")

    reference_highcode = reference.section_bytes(".highcode")
    candidate_highcode = candidate.section_bytes(".highcode")
    highcode = candidate.section(".highcode")
    highcode_changes = changed_offsets(reference_highcode, candidate_highcode)
    allowed_highcode_changes: set[int] = set()
    for call_pc, _target in patched_calls:
        call_offset = call_pc - highcode[3]
        allowed_highcode_changes |= set(range(call_offset, call_offset + 4))

    profile_name, _identity, banner_from, banner_to = banner_profile
    banner_offset = candidate_highcode.find(banner_to)
    if banner_offset < 0 or candidate_highcode.count(banner_to) != 1:
        die(f"candidate {profile_name} banner is missing or not unique")
    if reference_highcode[banner_offset : banner_offset + len(banner_from)] != banner_from:
        die(f"{profile_name} banner did not replace the qualified banner")
    allowed_highcode_changes |= set(range(banner_offset,
                                           banner_offset + len(banner_to)))
    if not highcode_changes or not highcode_changes <= allowed_highcode_changes:
        die(
            "qualified .highcode changed outside the cold-boot JALs/banner: "
            + ",".join(f"0x{offset:x}"
                       for offset in sorted(highcode_changes)[:16])
        )

    load_changes = changed_offsets(
        reference.section_bytes(".loadcode"),
        candidate.section_bytes(".loadcode"),
    )
    allowed_load_changes = set(range(0x2C, 0x34)) | set(range(0x3C, 0x44))
    if not load_changes or not load_changes <= allowed_load_changes:
        die(
            "startup changed outside second-copy address immediates: "
            + ",".join(f"0x{offset:x}" for offset in sorted(load_changes)[:16])
        )

    for name in (".text", ".data", ".image_rodata"):
        if reference.section_bytes(name) != candidate.section_bytes(name):
            die(f"qualified contents changed for {name}")

    ref_bss = reference.section(".bss")
    candidate_bss = candidate.section(".bss")
    if (ref_bss[3], ref_bss[5]) != (candidate_bss[3], candidate_bss[5]):
        die("qualified VMA/size changed for .bss")

    for name in (
        ".fini", ".preinit_array", ".init_array", ".fini_array",
        ".ctors", ".dtors",
    ):
        ref_section = reference.section_or_none(name)
        candidate_section = candidate.section_or_none(name)
        ref_size = 0 if ref_section is None else ref_section[5]
        candidate_size = 0 if candidate_section is None else candidate_section[5]
        if ref_size != 0 or candidate_size != 0:
            die(f"pre-existing second-copy section {name} is not empty")

    # These hot functions cover the transport, upload, SDRAM verify, H4V1
    # decode and LTDC paths.  The section-byte gate above already proves every
    # other qualified instruction is identical; symbol equality additionally
    # prevents an apparently-identical byte stream from being reinterpreted at
    # a shifted ABI boundary.
    ref_symbols = reference.symbol_entries()
    candidate_symbols = candidate.symbol_entries()
    for name in (
        "cdc_acm_data_recv",
        "ch32h417_usb_cdc_write",
        "ch32h417_usb_cdc_read_line",
        "ch32h417_usb_cdc_raw_rx_enable",
        "ch32h417_usb_cdc_raw_rx_read",
        "ch32h417_usb_cdc_raw_rx_overflowed",
        "ch32h417_dual_cdc_poll",
        "USBFS_IRQHandler",
        "sdram_memtest_watchdog_complete",
        "sdram_usb_debug_write_line.isra.0",
        "sdram_memtest_dma_stream_transfer",
        "sdram_video_h4v1_decode_chunked_frame",
        "sdram_video_h4v1_live_swap",
        "sdram_video_ltdc_start_full",
        "run_sdram_video_test",
        "__rt_init_rti_start",
        "__rt_init_rti_board_start",
        "__rt_init_rti_board_end",
        "__rt_init_finsh_system_init",
        "__rt_init_rti_end",
        "__fsymtab_start",
        "__fsymtab_end",
    ):
        if name not in ref_symbols or name not in candidate_symbols:
            die(f"qualified symbol disappeared: {name}")
        if ref_symbols[name] != candidate_symbols[name]:
            die(f"qualified symbol address/size changed: {name}")

    symbols = candidate.symbols()
    tail = candidate.section(".h4v1_postpass")
    ref_highcode = reference.section(".highcode")
    expected_vma = ref_highcode[3] + ref_highcode[5]
    expected_lma = (reference.section_lma(".image_rodata") +
                    reference.section(".image_rodata")[5])
    if tail[3] != expected_vma or candidate.section_lma(".h4v1_postpass") != expected_lma:
        die(
            f"postpass placement changed: VMA=0x{tail[3]:08x} "
            f"LMA=0x{candidate.section_lma('.h4v1_postpass'):08x}"
        )
    if symbols["__h4v1_postpass_start"] != tail[3] or \
       symbols["__h4v1_postpass_end"] != tail[3] + tail[5]:
        die("cold-boot postpass boundary symbols disagree with the section")

    cold_bss = candidate.section(".h4v1_coldboot_bss")
    if cold_bss[1] != 8:  # SHT_NOBITS
        die("cold-boot state is not isolated in a NOBITS section")
    expected_bss_vma = (tail[3] + tail[5] + 7) & ~7
    if cold_bss[3] != expected_bss_vma:
        die("cold-boot state is not immediately after the isolated tail")
    if cold_bss[5] == 0 or cold_bss[5] > 0x2000:
        die(f"unexpected cold-boot state size: {cold_bss[5]}")
    if symbols["__h4v1_coldboot_bss_start"] != cold_bss[3] or \
       symbols["__h4v1_coldboot_bss_end"] != cold_bss[3] + cold_bss[5]:
        die("cold-boot state boundary symbols disagree with the section")
    if cold_bss[3] + cold_bss[5] > 0x200C0000:
        die("cold-boot tail/state exceeds the V5 ITCM region")

    post_start = symbols["__h4v1_postpass_start"]
    post_end = symbols["__h4v1_postpass_end"]
    for name in (
        "h4v1_flash_coldboot_line_entry",
        "h4v1_flash_coldboot_raw_provider",
        "h4v1_flash_postpass_trampoline",
        "h4v1_flash_coldboot_log_line",
        "h4v1_flash_coldboot_loop_log_entry",
        "h4v1_flash_coldboot_replay_status",
        "h4v1_flash_coldboot_ack_suppress",
    ):
        if name not in symbols or not (post_start <= symbols[name] < post_end):
            die(f"cold-boot hook escaped the isolated tail: {name}")

    for call_pc, target in patched_calls:
        call_file_offset = candidate.address_to_offset(call_pc, 4)
        instruction = struct.unpack_from("<I", candidate.data,
                                         call_file_offset)[0]
        if decode_jal_target(call_pc, instruction) != target:
            die(f"cold-boot JAL at 0x{call_pc:08x} has the wrong target")

    original_word = symbols["h4v1_flash_postpass_original_call"]
    original_word_offset = candidate.address_to_offset(original_word, 4)
    if struct.unpack_from("<I", candidate.data,
                          original_word_offset)[0] != original_watchdog:
        die("cold-boot trampoline did not preserve watchdog-complete")
    loop_word = symbols["h4v1_flash_coldboot_original_loop_log"]
    loop_word_offset = candidate.address_to_offset(loop_word, 4)
    if struct.unpack_from("<I", candidate.data,
                          loop_word_offset)[0] != original_loop_log:
        die("cold-boot loop wrapper did not preserve the qualified logger")


def patch_coldboot_elf(
    elf: Elf32,
    reference: Elf32,
    path: pathlib.Path,
    banner_profile: tuple[str, bytes, bytes, bytes],
) -> None:
    highcode = elf.section(".highcode")
    blob_start, blob_size = highcode[4], highcode[5]
    blob = elf.data[blob_start : blob_start + blob_size]
    symbols = elf.symbols()
    required = (
        "h4v1_flash_coldboot_line_entry",
        "h4v1_flash_coldboot_raw_provider",
        "h4v1_flash_postpass_trampoline",
        "h4v1_flash_postpass_original_call",
        "h4v1_flash_coldboot_loop_log_entry",
        "h4v1_flash_coldboot_original_loop_log",
        "h4v1_flash_coldboot_replay_status",
        "h4v1_flash_coldboot_ack_suppress",
        "ch32h417_usb_cdc_read_line",
        "ch32h417_usb_cdc_raw_rx_read",
        "sdram_memtest_watchdog_complete",
        "sdram_usb_debug_write_line.isra.0",
        "__h4v1_postpass_start",
        "__h4v1_postpass_end",
        "__h4v1_coldboot_bss_start",
        "__h4v1_coldboot_bss_end",
    )
    missing = [name for name in required if name not in symbols]
    if missing:
        die("missing cold-boot symbols: " + ", ".join(missing))

    call_specs = (
        (
            "config-provider",
            COLD_BOOT_LINE_SIGNATURE,
            COLD_BOOT_LINE_JAL_OFFSET,
            "ch32h417_usb_cdc_read_line",
            "h4v1_flash_coldboot_line_entry",
        ),
        (
            "raw-provider",
            COLD_BOOT_RAW_SIGNATURE,
            COLD_BOOT_RAW_JAL_OFFSET,
            "ch32h417_usb_cdc_raw_rx_read",
            "h4v1_flash_coldboot_raw_provider",
        ),
        (
            "post-RESULT",
            COLD_BOOT_RESULT_SIGNATURE,
            COLD_BOOT_RESULT_JAL_OFFSET,
            "sdram_memtest_watchdog_complete",
            "h4v1_flash_postpass_trampoline",
        ),
        (
            "loop-status-replay",
            COLD_BOOT_LOOP_LOG_SIGNATURE,
            COLD_BOOT_LOOP_LOG_JAL_OFFSET,
            "sdram_usb_debug_write_line.isra.0",
            "h4v1_flash_coldboot_loop_log_entry",
        ),
        (
            "credit-ACK-suppress",
            COLD_BOOT_ACK_SIGNATURE,
            COLD_BOOT_ACK_JAL_OFFSET,
            "sdram_usb_debug_write_line.isra.0",
            "h4v1_flash_coldboot_ack_suppress",
        ),
    )
    patched_calls: list[tuple[int, int]] = []
    original_watchdog = 0
    original_loop_log = 0
    for description, signature, jal_offset, old_name, new_name in call_specs:
        signature_offset = find_unique_signature(blob, signature, description)
        call_offset = signature_offset + jal_offset
        call_pc = highcode[3] + call_offset
        call_file_offset = blob_start + call_offset
        instruction = struct.unpack_from("<I", elf.data, call_file_offset)[0]
        old_target = decode_jal_target(call_pc, instruction)
        if old_target != symbols[old_name]:
            die(
                f"qualified {description} JAL target is 0x{old_target:08x}, "
                f"symbol {old_name} is 0x{symbols[old_name]:08x}"
            )
        new_target = symbols[new_name]
        post_start = symbols["__h4v1_postpass_start"]
        post_end = symbols["__h4v1_postpass_end"]
        if not (post_start <= new_target < post_end):
            die(f"{description} replacement escaped the isolated tail")
        struct.pack_into("<I", elf.data, call_file_offset,
                         encode_jal(call_pc, new_target, rd=1))
        patched_calls.append((call_pc, new_target))
        if old_name == "sdram_memtest_watchdog_complete":
            original_watchdog = old_target
        elif old_name == "sdram_usb_debug_write_line.isra.0":
            original_loop_log = old_target

    original_word_addr = symbols["h4v1_flash_postpass_original_call"]
    original_word_offset = elf.address_to_offset(original_word_addr, 4)
    sentinel = struct.unpack_from("<I", elf.data, original_word_offset)[0]
    if sentinel != ORIGINAL_TARGET_SENTINEL:
        die(
            f"original-call sentinel is 0x{sentinel:08x}, expected "
            f"0x{ORIGINAL_TARGET_SENTINEL:08x}"
        )
    struct.pack_into("<I", elf.data, original_word_offset, original_watchdog)
    loop_word_addr = symbols["h4v1_flash_coldboot_original_loop_log"]
    loop_word_offset = elf.address_to_offset(loop_word_addr, 4)
    loop_sentinel = struct.unpack_from("<I", elf.data, loop_word_offset)[0]
    if loop_sentinel != COLD_BOOT_LOOP_LOG_SENTINEL:
        die(
            f"loop-log sentinel is 0x{loop_sentinel:08x}, expected "
            f"0x{COLD_BOOT_LOOP_LOG_SENTINEL:08x}"
        )
    struct.pack_into("<I", elf.data, loop_word_offset, original_loop_log)

    profile_name, identity, banner_from, banner_to = banner_profile
    tail_bytes = elf.section_bytes(".h4v1_postpass")
    if tail_bytes.count(identity) != 1:
        die(f"{profile_name} tail identity is not unique")
    banner_offset = blob.find(banner_from)
    if banner_offset < 0 or blob.count(banner_from) != 1:
        die(f"qualified banner for {profile_name} is missing or not unique")
    if banner_to in blob:
        die(f"candidate already contains the {profile_name} banner")
    banner_file_offset = blob_start + banner_offset
    elf.data[banner_file_offset : banner_file_offset + len(banner_to)] = banner_to

    verify_coldboot_candidate(
        elf,
        reference,
        tuple(patched_calls),
        original_watchdog,
        original_loop_log,
        banner_profile,
    )
    path.write_bytes(elf.data)
    sites = ",".join(f"0x{pc:08x}->0x{target:08x}"
                     for pc, target in patched_calls)
    cold_bss = elf.section(".h4v1_coldboot_bss")
    print(
        "H4V1 COLD BOOT PATCH "
        f"sites={sites} tail={elf.section('.h4v1_postpass')[5]} "
        f"state={cold_bss[5]}"
    )


def patch_elf(path: pathlib.Path, reference_path: pathlib.Path) -> None:
    elf = Elf32(path)
    reference = Elf32(reference_path)
    highcode = elf.section(".highcode")
    blob_start, blob_size = highcode[4], highcode[5]
    blob = elf.data[blob_start : blob_start + blob_size]
    tail_bytes = elf.section_bytes(".h4v1_postpass")
    early_profiles = [profile for profile in BANNER_PROFILES
                      if profile[1] in tail_bytes]
    if len(early_profiles) > 1:
        die("multiple mutually exclusive post-PASS banner identities found")
    if early_profiles and early_profiles[0][0] == "cold-boot-165":
        patch_coldboot_elf(elf, reference, path, early_profiles[0])
        return

    hits: list[int] = []
    for signature in CALLSITE_SIGNATURES:
        cursor = 0
        while True:
            found = blob.find(signature, cursor)
            if found < 0:
                break
            hits.append(found)
            cursor = found + 1
    if len(hits) != 1:
        die(f"expected one qualified post-PASS signature, found {len(hits)}")

    symbols = elf.symbols()
    required = (
        "h4v1_flash_postpass_trampoline",
        "h4v1_flash_postpass_original_call",
        "sdram_memtest_watchdog_complete",
        "__h4v1_postpass_start",
        "__h4v1_postpass_end",
    )
    missing = [name for name in required if name not in symbols]
    if missing:
        die("missing symbols: " + ", ".join(missing))

    call_offset_in_section = hits[0] + CALLSITE_JAL_OFFSET
    call_pc = highcode[3] + call_offset_in_section
    call_file_offset = blob_start + call_offset_in_section
    old_instruction = struct.unpack_from("<I", elf.data, call_file_offset)[0]
    old_target = decode_jal_target(call_pc, old_instruction)
    expected_old_target = symbols["sdram_memtest_watchdog_complete"]
    if old_target != expected_old_target:
        die(
            f"qualified JAL target is 0x{old_target:08x}, symbol is "
            f"0x{expected_old_target:08x}"
        )

    post_start = symbols["__h4v1_postpass_start"]
    post_end = symbols["__h4v1_postpass_end"]
    trampoline = symbols["h4v1_flash_postpass_trampoline"]
    original_word_addr = symbols["h4v1_flash_postpass_original_call"]
    if not (post_start <= trampoline < post_end):
        die("trampoline escaped the isolated post-pass tail")
    if not (post_start <= original_word_addr < post_end):
        die("original-call word escaped the isolated post-pass tail")

    original_word_offset = elf.address_to_offset(original_word_addr, 4)
    sentinel = struct.unpack_from("<I", elf.data, original_word_offset)[0]
    if sentinel != ORIGINAL_TARGET_SENTINEL:
        die(
            f"original-call sentinel is 0x{sentinel:08x}, expected "
            f"0x{ORIGINAL_TARGET_SENTINEL:08x}"
        )

    tail_bytes = elf.section_bytes(".h4v1_postpass")
    banner_matches = [
        profile for profile in BANNER_PROFILES if profile[1] in tail_bytes
    ]
    if len(banner_matches) > 1:
        die("multiple mutually exclusive post-PASS banner identities found")
    banner_profile = banner_matches[0] if banner_matches else None
    if banner_profile is not None:
        profile_name, identity, banner_from, banner_to = banner_profile
        if tail_bytes.count(identity) != 1:
            die(f"{profile_name} tail identity is not unique")
        banner_offset = blob.find(banner_from)
        if banner_offset < 0 or blob.count(banner_from) != 1:
            die(f"qualified banner for {profile_name} is missing or not unique")
        if banner_to in blob:
            die(f"candidate already contains the {profile_name} banner")
        banner_file_offset = blob_start + banner_offset
        elf.data[banner_file_offset :
                 banner_file_offset + len(banner_to)] = banner_to

    new_instruction = encode_jal(call_pc, trampoline, rd=1)
    struct.pack_into("<I", elf.data, original_word_offset, old_target)
    struct.pack_into("<I", elf.data, call_file_offset, new_instruction)
    verify_candidate(
        elf,
        reference,
        call_pc,
        old_target,
        trampoline,
        banner_profile,
    )
    path.write_bytes(elf.data)
    print(
        "H4V1 POSTPASS PATCH "
        f"call=0x{call_pc:08x} original=0x{old_target:08x} "
        f"trampoline=0x{trampoline:08x} tail={post_end - post_start}"
    )


def main() -> None:
    parser = argparse.ArgumentParser()
    subparsers = parser.add_subparsers(dest="command", required=True)
    linker = subparsers.add_parser("linker")
    linker.add_argument("--input", required=True, type=pathlib.Path)
    linker.add_argument("--output", required=True, type=pathlib.Path)
    patch = subparsers.add_parser("patch")
    patch.add_argument("--elf", required=True, type=pathlib.Path)
    patch.add_argument("--reference-elf", required=True, type=pathlib.Path)
    args = parser.parse_args()

    if args.command == "linker":
        generate_linker(args.input, args.output)
    else:
        patch_elf(args.elf, args.reference_elf)


if __name__ == "__main__":
    main()
