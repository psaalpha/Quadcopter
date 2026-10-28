#!/usr/bin/env python3
"""Check pinned RTOS sources, interrupt rules and exception ownership."""
from pathlib import Path
import hashlib
import re
import sys
import xml.etree.ElementTree as ET
ROOT = Path(__file__).resolve().parents[1]

def main():
    errors = []
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
    delay = (ROOT / 'Platform/STM32F1/System/Delay.c').read_text(encoding='utf-8')
    code = re.sub(r'/\*.*?\*/|//[^\n]*', '', delay, flags=re.S)
    if 'SysTick->' in code:
        errors.append('Delay.c still owns SysTick')
    if errors:
        print('\n'.join(errors))
        return 1
    print('FreeRTOS source, IRQ, static allocation, exception ownership checks passed.')
    print('Run tools/build_gcc.py for actual firmware linking, vectors and memory budgets.')
    return 0

if __name__ == '__main__':
    sys.exit(main())
