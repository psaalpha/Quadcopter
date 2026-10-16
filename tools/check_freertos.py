#!/usr/bin/env python3
"""Check RTOS integration contracts and, when present, Keil link sizes."""
from pathlib import Path
import hashlib
import json
import re
import sys
import xml.etree.ElementTree as ET
ROOT = Path(__file__).resolve().parents[1]

def main():
    errors, sizes = [], {}
    kernel = ROOT / 'Platform/FreeRTOS/Kernel'
    for line in (kernel.parent / 'UPSTREAM.sha256').read_text().splitlines():
        expected, path = line.split('  ', 1)
        if hashlib.sha256((kernel / path).read_bytes().replace(b'\r\n', b'\n')).hexdigest() != expected:
            errors.append('vendored kernel changed: ' + path)
    required = ['tasks.c', 'queue.c', 'list.c', 'port.c', 'rtos_support.c', 'task_health.c']
    for board in ['Master_MCU', 'Slave_MCU']:
        project = ET.parse(ROOT / board / 'Project.uvprojx')
        names = [n.text for n in project.findall('.//Files/File/FileName')]
        for name in required:
            if names.count(name) != 1:
                errors.append(board + ': RTOS source missing/duplicated: ' + name)
        if any(re.fullmatch(r'heap_[1-5]\.c', n) for n in names):
            errors.append(board + ': unexpected dynamic heap source')
        for source in (ROOT / board).rglob('*.c'):
            if 'Objects' in source.parts or 'RTE' in source.parts:
                continue
            code = re.sub(r'/\*.*?\*/|//[^\n]*', '', source.read_text(encoding='utf-8-sig', errors='replace'), flags=re.S)
            if 'NVIC_PriorityGroup_2' in code:
                errors.append(str(source) + ': old NVIC group')
            for value in re.findall(r'NVIC_IRQChannelPreemptionPriority\s*=\s*(\d+)', code):
                if not 5 <= int(value) <= 15:
                    errors.append(str(source) + ': IRQ priority cannot call FromISR')
        exceptions = (ROOT / board / 'Core/stm32f10x_it.c').read_text(encoding='utf-8-sig')
        if re.search(r'void\s+(SVC|PendSV|SysTick)_Handler\s*\(', exceptions):
            errors.append(board + ': exception stub conflicts with official port')
        link_map = ROOT / board / 'Listings/Project.map'
        build_log = ROOT / board / 'Objects/engineering_build.log'
        if link_map.exists() and build_log.exists():
            log = build_log.read_text(errors='replace')
            if '0 Error(s), 0 Warning(s)' not in log:
                errors.append(board + ': build log does not confirm success')
            data = link_map.read_text(errors='replace')
            ram = int(re.search(r'Total RW\s+Size.*?\s(\d+)\s+\(', data).group(1))
            rom = int(re.search(r'Total ROM Size.*?\s(\d+)\s+\(', data).group(1))
            ro = int(re.search(r'Total RO\s+Size.*?\s(\d+)\s+\(', data).group(1))
            raw = int(re.search(r'Load Region LR_IROM1 .*?Size: (0x[\da-f]+)', data).group(1), 16)
            limit = 0xfc00 if board == 'Slave_MCU' else 0x10000
            sizes[board] = dict(ro_bytes=ro, rom_compressed_bytes=rom, load_region_uncompressed_bytes=raw,
                                ram_bytes=ram, physical_flash_limit_bytes=limit,
                                conservative_32k_headroom_bytes=32768-raw)
            if ram > 20480 or raw > limit or raw > 32768:
                errors.append(board + ': memory budget exceeded')
            for vector in ['SVC_Handler', 'PendSV_Handler', 'SysTick_Handler']:
                if not re.search(r'^\s+' + vector + r'\s+.*?port\.o', data, re.M):
                    errors.append(board + ': vector not provided by port.o: ' + vector)
    delay = (ROOT / 'Platform/STM32F1/System/Delay.c').read_text(encoding='utf-8')
    code = re.sub(r'/\*.*?\*/|//[^\n]*', '', delay, flags=re.S)
    if 'SysTick->' in code:
        errors.append('Delay.c still owns SysTick')
    if errors:
        print('\n'.join(errors))
        return 1
    print('FreeRTOS source, IRQ, static allocation, exception ownership checks passed.')
    print(json.dumps(sizes, indent=2))
    if not sizes:
        print('Keil maps absent: firmware size/link success NOT checked on this machine.')
    return 0

if __name__ == '__main__':
    sys.exit(main())
