# file: "makefile"

OS_NAME = "\"MINOS2 (**** ajoutez vos noms ici ****)\""
KERNEL_START = 0x20000

KERNEL_OBJECTS = kernel.o main.o thread.o time.o ps2.o fifo.o term.o video.o intr.o rtlib.o
DEFS =

GCC = gcc
GPP = g++

SPECIAL_OPTIONS =

GCC_OPTIONS = $(SPECIAL_OPTIONS) $(DEFS) -DOS_NAME=$(OS_NAME) -DKERNEL_START=$(KERNEL_START) -fomit-frame-pointer -fno-strict-aliasing -Wall -O3 -nostdinc -nostdinc++ -Iinclude

GPP_OPTIONS = $(GCC_OPTIONS) -fno-rtti -fno-builtin -fno-exceptions

.SUFFIXES:
.SUFFIXES: .h .s .c .cpp .o .asm .bin .map .d

all: floppy

mf:
	make clean
	make SPECIAL_OPTIONS=-MMD
	sed "/^# dependencies:$$/q" makefile | cat - *.d > mf
	rm -f *.d
	mv makefile makefile.old
	mv mf makefile

floppy: bootsect.bin kernel.bin
	dd if=bootsect.bin of=tmp1.tmp bs=512 count=1
	dd if=blank_floppy of=tmp2.tmp bs=512 count=32 skip=1
	dd if=/dev/zero of=tmp3.tmp bs=512 count=2880
	cat tmp1.tmp tmp2.tmp kernel.bin tmp3.tmp > tmp4.tmp
	dd if=tmp4.tmp of=floppy bs=512 count=2880
	rm -f tmp1.tmp tmp2.tmp tmp3.tmp tmp4.tmp

kernel.bin: $(KERNEL_OBJECTS)
	ld --script=script.ld -fno-rtti $(KERNEL_OBJECTS) -o $*.bin -Ttext $(KERNEL_START) -N -e kernel_entry -oformat binary -Map kernel.map

kernel.bss:
	cat kernel.map | grep '\.bss ' | grep -v '\.o' | sed 's/.*0x/0x/'

kernel.o: kernel.s
	as --defsym KERNEL_START=$(KERNEL_START) -o $*.o $*.s

.o.asm:
	objdump --disassemble-all $*.o > $*.asm

bootsect.o: bootsect.s kernel.bin
	as --defsym KERNEL_START=$(KERNEL_START) --defsym KERNEL_SIZE=`cat kernel.bin | wc --bytes | sed -e "s/ //g"` -o $*.o $*.s

bootsect.bin: bootsect.o
	ld -fno-rtti $*.o -o $*.bin -Ttext 0x7c00 -N -e bootsect_entry -oformat binary -Map bootsect.map

.cpp.o:
	$(GPP) $(GPP_OPTIONS) -c $*.cpp

.c.o:
	$(GCC) $(GCC_OPTIONS) -c $*.c

.s.o: kernel.bin
	as --defsym OS_NAME=$(OS_NAME) --defsym KERNEL_START=$(KERNEL_START) --defsym KERNEL_SIZE=`cat kernel.bin | wc --bytes | sed -e "s/ //g"` -o $*.o $*.s

clean:
	rm -f *.o *.asm *.bin *.tmp *.d

# dependencies:
fifo.o: fifo.cpp include/fifo.h include/general.h include/thread.h \
  include/intr.h include/asm.h include/pic.h include/apic.h \
  include/time.h include/pit.h include/queue.h
intr.o: intr.cpp include/intr.h include/general.h include/asm.h \
  include/pic.h include/apic.h include/term.h include/video.h
main.o: main.cpp include/general.h include/term.h include/video.h \
  include/fifo.h include/thread.h include/intr.h include/asm.h \
  include/pic.h include/apic.h include/time.h include/pit.h \
  include/queue.h include/ps2.h
ps2.o: ps2.cpp include/ps2.h include/general.h include/intr.h \
  include/asm.h include/pic.h include/apic.h include/time.h include/pit.h \
  include/video.h include/term.h include/thread.h include/queue.h
rtlib.o: rtlib.cpp include/rtlib.h include/general.h include/intr.h \
  include/asm.h include/pic.h include/apic.h include/time.h include/pit.h \
  include/ps2.h include/term.h include/video.h include/thread.h \
  include/queue.h
term.o: term.cpp include/term.h include/general.h include/video.h
thread.o: thread.cpp include/thread.h include/general.h include/intr.h \
  include/asm.h include/pic.h include/apic.h include/time.h include/pit.h \
  include/queue.h include/rtlib.h include/term.h include/video.h
time.o: time.cpp include/time.h include/general.h include/asm.h \
  include/pit.h include/apic.h include/intr.h include/pic.h include/rtc.h \
  include/term.h include/video.h
video.o: video.cpp include/video.h include/general.h include/asm.h \
  include/vga.h include/term.h mono_5x7.cpp mono_6x9.cpp
