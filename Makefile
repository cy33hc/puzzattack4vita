
TARGET = puzzattack4vita

OBJS   = puzzattack.o data/brick0.o data/brick1.o data/brick2.o data/brick3.o data/font.o \
	 data/brick4.o data/brick5.o data/brick6.o data/cursor.o data/lightmask.o data/background.o

LIBS =  -lvita2d -lSceKernel_stub -lSceDisplay_stub -lSceGxm_stub \
        -lSceSysmodule_stub -lSceCtrl_stub -lScePgf_stub \
        -lSceCommonDialog_stub -lpng -ljpeg -lfreetype -lz -lm -lc

INCLUDES = data

PREFIX  = arm-vita-eabi
CC      = $(PREFIX)-gcc
CFLAGS  = -Wl,-q -Wall -O3 -DVITA -I$(INCLUDES)

ASFLAGS = $(CFLAGS)

all: $(TARGET).velf

%.velf: %.elf
	$(PREFIX)-strip -g $<
	vita-elf-create $< $@

$(TARGET).elf: $(OBJS)
	$(CC) $(CFLAGS) $^ $(LIBS) -o $@

clean:
	@rm -rf $(TARGET).velf $(TARGET).elf $(OBJS)

