all: c8as c8emu

c8as: chip8as.c
	gcc -g -o $@ $<

c8emu: chip8emu.c
	gcc -g -o $@ $< -lSDL2

.PHONY:  all
