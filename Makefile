
TARGET = puzzattack4vita
PSVITAIP = 192.168.100.8

PROJECT_TITLE := puzzattack4vita
PROJECT_TITLEID := PUZZ00001

OBJS   = puzzattack.o

LIBS =  -lSDL -lSDL_ttf -lSDL_mixer -lSceKernel_stub -lSceDisplay_stub -lSceGxm_stub \
        -lSceSysmodule_stub -lSceCtrl_stub -lScePgf_stub -lSceAudio_stub \
        -lSceCommonDialog_stub -lSceNetCtl_stub -lSceNet_stub -lfreetype -lpng -lz -lm -lc

PREFIX  = arm-vita-eabi
CC      = $(PREFIX)-gcc
CFLAGS  = -Wl,-q -Wall -O3 -DVITA 

ASFLAGS = $(CFLAGS)

all: package

package: $(TARGET).vpk

$(TARGET).vpk: eboot.bin param.sfo
	vita-pack-vpk -s param.sfo -b eboot.bin \
		--add sce_sys/icon0.png=sce_sys/icon0.png \
		--add sce_sys/livearea/contents/bg.png=sce_sys/livearea/contents/bg.png \
		--add sce_sys/livearea/contents/startup.png=sce_sys/livearea/contents/startup.png \
		--add sce_sys/livearea/contents/template.xml=sce_sys/livearea/contents/template.xml \
		--add assets/brick0.bmp=assets/brick0.bmp \
		--add assets/brick1.bmp=assets/brick1.bmp \
		--add assets/brick2.bmp=assets/brick2.bmp \
		--add assets/brick3.bmp=assets/brick3.bmp \
		--add assets/brick4.bmp=assets/brick4.bmp \
		--add assets/brick5.bmp=assets/brick5.bmp \
		--add assets/brick6.bmp=assets/brick6.bmp \
		--add assets/cursor.bmp=assets/cursor.bmp \
		--add assets/lightmask.bmp=assets/lightmask.bmp \
		--add assets/background.bmp=assets/background.bmp \
		--add assets/Ubuntu-R.ttf=assets/Ubuntu-R.ttf \
		--add assets/cursor.wav=assets/cursor.wav \
		--add assets/explode.wav=assets/explode.wav \
	$(TARGET).vpk
	
eboot.bin: $(TARGET).velf
	vita-make-fself -s $(TARGET).velf eboot.bin

param.sfo:
	vita-mksfoex -s TITLE_ID="$(PROJECT_TITLEID)" "$(PROJECT_TITLE)" param.sfo

$(TARGET).velf: $(TARGET).elf
	$(PREFIX)-strip -g $<
	vita-elf-create $< $@

$(TARGET).elf: $(OBJS)
	$(CC) $(CFLAGS) $^ $(LIBS) -o $@

clean:
	rm -f $(TARGET).velf $(TARGET).elf $(TARGET).vpk param.sfo eboot.bin $(OBJS)

vpksend: $(TARGET).vpk
	curl -T $(TARGET).vpk ftp://$(PSVITAIP):1337/ux0:/
	@echo "Sent."

send: eboot.bin
	curl -T eboot.bin ftp://$(PSVITAIP):1337/ux0:/app/$(PROJECT_TITLEID)/
	@echo "Sent."

