TARGET = kernel.elf
OBJS = main.o graphics.o mouse.o font.o hankaku.o newlib_support.o console.o pci.o asmfunc.o libcxx_support.o logger.o \
		usb/memory.o usb/device.o usb/xhci/ring.o usb/xhci/trb.o usb/xhci/xhci.o usb/xhci/port.o usb/xhci/device.o \
		usb/xhci/devmgr.o usb/xhci/registers.o usb/classdriver/base.o usb/classdriver/hid.o usb/classdriver/keyboard.o \
		usb/classdriver/mouse.o
DEPENDS = $(join $(dir $(OBJS)),$(addprefix .,$(notdir $(OBJS:.o=.d))))

CPPFLAGS += -I.
CFLAGS   += -O2 -Wall -g --target=x86_64-elf -ffreestanding -mno-red-zone
CXXFLAGS += -O2 -Wall -g --target=x86_64-elf -ffreestanding -mno-red-zone -fno-exceptions -fno-rtti -std=c++17
LDFLAGS  += --entry KernelMain -z norelro --image-base 0x100000 --static

BASEDIR=$(HOME)/myos/devenv/x86_64-elf
CPPFLAGS += -I$(BASEDIR)/include/c++/v1 -I$(BASEDIR)/include -I$(BASEDIR)/include/freetype2 -nostdlibinc -D__ELF__ -D_LDBL_EQ_DBL -D_GNU_SOURCE -D_POSIX_TIMERS
LDFLAGS += -L$(BASEDIR)/lib

.PHONY: all
all: $(TARGET)

.PHONY: clean
clean:
	rm -rf *.o
	rm -rf *.d


kernel.elf: $(OBJS) Makefile
	ld.lld $(LDFLAGS) -o kernel.elf $(OBJS) -lc -lc++

%.o: %.cpp Makefile
	clang++ $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

.%.d: %.cpp
	clang++ $(CPPFLAGS) $(CXXFLAGS) -MM $< > $@
	$(eval OBJ = $(<:.cpp=.o))
	sed --in-place 's|$(notdir $(OBJ))|$(OBJ)|' $@

%.o: %.c Makefile
	clang $(CPPFLAGS) $(CFLAGS) -c $< -o $@

.%.d: %.c
	clang $(CPPFLAGS) $(CFLAGS) -MM $< > $@
	$(eval OBJ = $(<:.c=.o))
	sed --in-place 's|$(notdir $(OBJ))|$(OBJ)|' $@

%.o: %.asm Makefile
	nasm -f elf64 -o $@ $<

hankaku.bin: hankaku.txt
	../../tool/makefont.py -o $@ $<

hankaku.o: hankaku.bin
	objcopy -I binary -O elf64-x86-64 -B i386:x86-64 $< $@

.%.d: %.bin
	touch $@


.PHONY: depends
depends:
	$(MAKE) $(DEPENDS)

-include $(DEPENDS)