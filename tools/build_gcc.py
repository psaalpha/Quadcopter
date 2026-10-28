#!/usr/bin/env python3
"""Build both STM32F103 targets from the same source/include lists as Keil."""
from pathlib import Path
import argparse,os,shutil,subprocess,xml.etree.ElementTree as ET,json,re,struct
ROOT=Path(__file__).resolve().parents[1]
def run(args):
    result=subprocess.run([str(a) for a in args],cwd=ROOT,text=True,capture_output=True)
    if result.returncode:
        print(result.stdout,result.stderr);raise RuntimeError('ARM GCC build failed')
    if result.stderr.strip():print(result.stderr.strip())
    return result.stdout
def main():
    p=argparse.ArgumentParser();p.add_argument('--compiler',default=os.environ.get('ARM_GCC','arm-none-eabi-gcc'))
    p.add_argument('--target',choices=['Master_MCU','Slave_MCU']);args=p.parse_args()
    compiler=shutil.which(args.compiler) or args.compiler
    if not Path(compiler).is_file():raise SystemExit('ARM GCC not found; set ARM_GCC or use --compiler')
    prefix=str(Path(compiler).parent/'arm-none-eabi-')
    extension='.exe' if str(compiler).lower().endswith('.exe') else ''
    summary={}
    compiler_version=run([compiler,'--version']).splitlines()[0]
    for board in ([args.target] if args.target else ['Master_MCU','Slave_MCU']):
        project=ET.parse(ROOT/board/'Project.uvprojx')
        base=ROOT/board
        includes=[]
        for node in project.findall('.//Cads/VariousControls/IncludePath'):
            for item in (node.text or '').split(';'):
                if item:
                    item=item.replace('\\','/').replace('/RVDS/','/GCC/')
                    includes+=['-I',str((base/item).resolve())]
        defines=['-DSTM32F10X_MD']
        for node in project.findall('.//Cads/VariousControls/Define'):
            defines+=['-D'+x.strip() for x in (node.text or '').split(',') if x.strip()]
        build=ROOT/'build'/'firmware-gcc'/board;build.mkdir(parents=True,exist_ok=True)
        cflags=['-mcpu=cortex-m3','-mthumb','-std=c99','-Os','-g3','-ffunction-sections','-fdata-sections',
                '-fno-common','-fno-strict-aliasing','-Wall','-Wextra','-Werror','-Wno-missing-braces',
                '-Wno-unused-parameter','--specs=nano.specs']+defines+includes
        sources=[]
        for node in project.findall('.//Files/File/FilePath'):
            path=(node.text or '').replace('\\','/').replace('/RVDS/','/GCC/')
            if path.endswith('.c'):sources.append((base/path).resolve())
        sources+=[ROOT/'Platform/STM32F1/GCC/syscalls.c']
        objects=[]
        for number,source in enumerate(sources):
            obj=build/(str(number)+'_'+source.stem+'.o')
            if source.name=='core_cm3.c':
                # CMSIS 1.x predates modern GCC register allocation. STREX's
                # status output must not overlap either input register.
                # Apply the three early-clobber constraints to a build copy;
                # the vendored CMSIS/ARMCC sources remain unchanged.
                original=source.read_text(encoding='utf-8-sig')
                patched,count=re.subn(r'("strex[bh]? %0, %2, \[%1\]"\s*:\s*)"=r"',r'\1"=&r"',original)
                if count!=3:raise RuntimeError('Unexpected CMSIS GCC intrinsic layout')
                source=build/'core_cm3_gcc_compat.c';source.write_text(patched,encoding='utf-8')
            run([compiler,*cflags,'-c',source,'-o',obj]);objects.append(obj)
        startup=ROOT/'Platform/STM32F1/GCC/startup_stm32f103.S';obj=build/'startup.o'
        run([compiler,'-mcpu=cortex-m3','-mthumb','-c',startup,'-o',obj]);objects.append(obj)
        elf=build/'firmware.elf';link_map=build/'firmware.map'
        result=run([compiler,'-mcpu=cortex-m3','-mthumb','--specs=nano.specs','--specs=nosys.specs',
            '-T',ROOT/'Platform/STM32F1/GCC/stm32f103.ld','-Wl,--gc-sections',
            '-Wl,-Map='+str(link_map),'-Wl,--print-memory-usage',*objects,
            '-Wl,--start-group','-lc','-lm','-lgcc','-lnosys','-Wl,--end-group','-o',elf])
        size=run([prefix+'size'+extension,elf]);print(board+'\n'+size+result)
        for fmt,suffix in [('ihex','hex'),('binary','bin')]:run([prefix+'objcopy'+extension,'-O',fmt,elf,build/('firmware.'+suffix)])
        symbols={}
        for line in run([prefix+'nm'+extension,'--defined-only',elf]).splitlines():
            fields=line.split()
            if len(fields)==3:symbols[fields[2]]=(int(fields[0],16),fields[1])
        vectors=struct.unpack('<16I',(build/'firmware.bin').read_bytes()[:64])
        rtos=any('QUADCOPTER_FREERTOS' in d for d in defines)
        for name,index in [('SVC_Handler',11),('PendSV_Handler',14),('SysTick_Handler',15)]:
            address,kind=symbols[name]
            if vectors[index]!=(address|1) or (rtos and kind!='T'):
                raise RuntimeError('Wrong exception vector: '+name)
        if vectors[0]!=0x20005000 or vectors[1]!=(symbols['Reset_Handler'][0]|1):
            raise RuntimeError('Invalid reset vector or initial stack')
        fields=size.splitlines()[-1].split();flash=int(fields[0])+int(fields[1])
        ram=symbols['_end'][0]+1536-0x20000000
        if (build/'firmware.bin').stat().st_size>62*1024 or ram>20*1024:raise RuntimeError('Physical memory budget exceeded')
        summary[board]={'compiler':compiler_version,'runtime':'freertos' if rtos else 'baremetal',
            'flash_text_and_data_bytes':flash,'flash_image_bytes':(build/'firmware.bin').stat().st_size,
            'ram_data_bss_heap_msp_bytes':ram,'vectors_checked':True,'elf':elf.relative_to(ROOT).as_posix()}
    (ROOT/'build/firmware-gcc/summary.json').write_text(json.dumps(summary,indent=2)+'\n',encoding='utf-8')
    return 0
if __name__=='__main__':raise SystemExit(main())
