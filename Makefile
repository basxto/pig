SDCCBIN=
GBDKDIR=/opt/gbdk-2020/
GBDKLIB=$(GBDKDIR)/lib/small/asxxxx/
MKROM?=$(SDCCBIN)makebin -Z -yc
CC=$(SDCCBIN)sdcc -mgbz80 --fsigned-char --no-std-crt0 -I "$(GBDKDIR)/include" -I "$(GBDKDIR)/include/asm" -c $(CFLAGS)
CA=$(SDCCBIN)sdasgb -plosgff
LD=$(SDCCBIN)sdldgb
XXD=xxd -i 
EMU?=sameboy
#EMU?=java -jar dev/Emulicious.jar 
pngconvert?=rgbgfx
ROM=pig
BANK=
MKROM+= -yt 0x03 -ya 1
BUILDIR=build/

.PHONY: build
build: $(ROM).gb

%.gb: $(BUILDIR)%.ihx
	dev/noi2sym.sh $(BUILDIR)/$*.noi $*.sym
	$(MKROM) -yn "PROC-ISLAND-GEN" $^ $@

$(BUILDIR):
	mkdir -p $@

%$(ROM).ihx: %main.rel
	$(LD) -nmjwxi -k "$(GBDKLIB)/gbz80/" -l gbz80.lib -k "$(GBDKLIB)/gb/" -l gb.lib -g .OAM=0xC000 -g .STACK=0xE000 -g .refresh_OAM=0xFF80 -g .init=0x000 -b _DATA=0xc0a0 -b _CODE=0x0200 $@ "${GBDKDIR}/lib/small/asxxxx/gb/crt0.o" $^

$(BUILDIR)main.asm: src/main.c | $(BUILDIR) $(BUILDIR)squont8ng_micro_2bpp.c $(BUILDIR)blowharder_path_2bpp.c
$(BUILDIR)%.asm: src/%.c | $(BUILDIR)
	$(CC) --fverbose-asm -S -o $@ $^

$(BUILDIR)%.2bpp: pix/%_gbc.png | $(BUILDIR)
	$(pngconvert) $< -o $@

$(BUILDIR)blowharder_path.2bpp: pix/blowharder_path_gbc.png | $(BUILDIR)
	$(pngconvert) -h $< -o $@

%_2bpp.c: %.2bpp
	$(XXD) $< $@

# generated
%.rel: %.asm
	$(CA) -o $@ $<

# handwritten
$(BUILDIR)%.rel: src/%.s | $(BUILDIR)
	$(CA) -o $@ $<


.PHONY: run
run: build
	$(EMU) $(ROM).gb

.PHONY: clean
clean:
	rm -rf $(BUILDIR) *.sym

.PHONY: spaceleft
spaceleft: build
	@hexdump -v -e '/1 "%02X\n"' $(ROM).gb | awk '/FF/ {n += 1} !/FF/ {n = 0} END {print n}'