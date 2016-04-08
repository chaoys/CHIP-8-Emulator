#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>
#include <assert.h>

#include <SDL2/SDL.h>

#define die(fmt, args...) do { fprintf(stderr, fmt, ##args); exit(1); } while(0)

#define DISPLAY_MEM	0xf00
#define STACK_MEM	0xea0
#define PROGRAM_MEM	0x200
#define OP_HLT		0xff
#define SURFACE_WIDTH	640
#define SURFACE_HEIGHT	320
#define SCREEN_WIDTH	64
#define SCREEN_HEIGHT	32

struct chip8_video {
	uint32_t bgcolor;
	uint32_t fgcolor;
	SDL_Surface *screen;
	SDL_Surface *surface;
	SDL_Window *window;
};

struct chip8_state {
	uint8_t v[16];

	uint16_t ip;
	uint16_t mp;
	int16_t sp;
	uint16_t hlt;

	uint8_t dt;
	uint8_t st;

	uint8_t key[16];

	struct chip8_video video;

	uint16_t *stack;
	uint8_t mem[4096];
};

static struct chip8_state chip8;
static struct chip8_state *c8 = &chip8;
static struct chip8_video *c8v = &chip8.video;

static void chip8_video_init(void)
{
	SDL_Init(SDL_INIT_VIDEO);
	c8v->window = SDL_CreateWindow("chip8 emulator", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SURFACE_WIDTH, SURFACE_HEIGHT, SDL_WINDOW_SHOWN);
	c8v->surface = SDL_GetWindowSurface(c8v->window);
	c8v->screen = SDL_CreateRGBSurface(0, 64, 32, 32, 0xff, 0xff00, 0xff0000, 0xff000000);
	assert(c8v->window);
	assert(c8v->surface);
	assert(c8v->screen);
	c8v->bgcolor = SDL_MapRGB(c8v->screen->format, 0x00, 0x00, 0x00);
	c8v->fgcolor = SDL_MapRGB(c8v->screen->format, 0xff, 0xff, 0xff);
}
static void chip8_video_close(void)
{
	SDL_DestroyWindow(c8v->window);
	SDL_FreeSurface(c8v->screen);
	SDL_Quit();
}
static void chip8_dump(void)
{
	printf("CHIP8 State\n");
	printf("IP 0x%x MP 0x%x SP 0x%x DT %d ST %d\n",
		chip8.ip, chip8.mp, chip8.sp, chip8.dt, chip8.st);
	printf("V0 %d V1 %d V2 %d V3 %d V4 %d V5 %d V6 %d V7 %d\n",
		chip8.v[0], chip8.v[1], chip8.v[2], chip8.v[3], chip8.v[4], chip8.v[5], chip8.v[6], chip8.v[7]);
	printf("V8 %d V9 %d VA %d VB %d VC %d VD %d VE %d VF %d \n",
		chip8.v[8], chip8.v[9], chip8.v[0xa], chip8.v[0xb], chip8.v[0xc], chip8.v[0xd], chip8.v[0xe], chip8.v[0xf]);
}
static void chip8_init(void)
{
	uint8_t fonts[] = {
		0xF0, 0x90, 0x90, 0x90, 0xF0,  /* 0 */
		0x20, 0x60, 0x20, 0x20, 0x70,  /* 1 */
		0xF0, 0x10, 0xF0, 0x80, 0xF0,  /* 2 */
		0xF0, 0x10, 0xF0, 0x10, 0xF0,  /* 3 */
		0x90, 0x90, 0xF0, 0x10, 0x10,  /* 4 */
		0xF0, 0x80, 0xF0, 0x10, 0xF0,  /* 5 */
		0xF0, 0x80, 0xF0, 0x90, 0xF0,  /* 6 */
		0xF0, 0x10, 0x20, 0x40, 0x40,  /* 7 */
		0xF0, 0x90, 0xF0, 0x90, 0xF0,  /* 8 */
		0xF0, 0x90, 0xF0, 0x10, 0xF0,  /* 9 */
		0xF0, 0x90, 0xF0, 0x90, 0x90,  /* A */
		0xE0, 0x90, 0xE0, 0x90, 0xE0,  /* B */
		0xF0, 0x80, 0x80, 0x80, 0xF0,  /* C */
		0xE0, 0x90, 0x90, 0x90, 0xE0,  /* D */
		0xF0, 0x80, 0xF0, 0x80, 0xF0,  /* E */
		0xF0, 0x80, 0xF0, 0x80, 0x80, /* F */
	};

	chip8_video_init();

	c8->ip = PROGRAM_MEM;
	c8->mp = 0;
	c8->sp = 0;
	c8->stack = (uint16_t *)&chip8.mem[STACK_MEM];

	memcpy(c8->mem, fonts, sizeof(fonts));
}
static void chip8_close(void)
{
	chip8_video_close();
	chip8_dump();
	exit(1);
}
static void chip8_video_clearscreen(void)
{
	SDL_Rect rect;
	rect.w = SCREEN_WIDTH;
	rect.h = SCREEN_HEIGHT;
	rect.x = 0;
	rect.y = 0;
	SDL_FillRect(c8v->screen, &rect, c8v->bgcolor);	
	rect.x = 0;
	rect.y = 0;
	rect.w = SURFACE_WIDTH;
	rect.h = SURFACE_HEIGHT;
	SDL_BlitScaled(c8v->screen, NULL, c8v->surface, &rect);
	SDL_UpdateWindowSurface(c8v->window);
}
static int chip8_video_draw(uint8_t *s, uint8_t x, uint8_t y, uint8_t n)
{
	int i, j;
	int wrap = 0;
	int collision = 0;
	uint32_t *pixel;
	SDL_Rect rect;

	if ((y + n > c8v->screen->h) || (x + 8 > c8v->screen->w))
		wrap = 1;
	else
		wrap = 0;

	for (i = 0; i < n; i++) {
		for (j = 0; j < 8; j++) {
			if ((0x80>>j) & s[i]) {
				pixel = (uint32_t*)c8v->screen->pixels + ((y+i)%c8v->screen->h) * c8v->screen->w + (x+j)%c8v->screen->w;
				if (*pixel == c8v->fgcolor) {
					collision = 1;
					*pixel = c8v->bgcolor;
				}
				else {
					*pixel = c8v->fgcolor;
				}
			}
		}
	}

	wrap = 1;
	if (wrap) {
		rect.x = 0;
		rect.y = 0;
		rect.w = SURFACE_WIDTH;
		rect.h = SURFACE_HEIGHT;
		SDL_BlitScaled(c8v->screen, NULL, c8v->surface, &rect);
		SDL_UpdateWindowSurface(c8v->window);
	} else {
		rect.x = x;
		rect.y = y;
		rect.w = 8;
		rect.h = n;
		SDL_UpdateWindowSurfaceRects(c8v->window, &rect, 1);
	}
	return collision;
}
/* 0xff means no key pressed */
static uint8_t chip8_video_key_wait(void)
{
	uint8_t i;

	for (i = 0; i < 0x0f; i++) {
		if (c8->key[i])
			return i;
	}
	return 0xff;
}
static void chip8_video_key_process(void)
{
	int key;
	SDL_Event e;

	while (SDL_PollEvent(&e)) {
		if (e.type == SDL_QUIT)
			chip8_close();

		if (e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) {
			switch (e.key.keysym.sym) {
				case SDLK_0:
				case SDLK_1:
				case SDLK_2:
				case SDLK_3:
				case SDLK_4:
				case SDLK_5:
				case SDLK_6:
				case SDLK_7:
				case SDLK_8:
				case SDLK_9:
					key = e.key.keysym.sym - SDLK_0;
					break;
				case SDLK_a:
				case SDLK_b:
				case SDLK_c:
				case SDLK_d:
				case SDLK_e:
				case SDLK_f:
					key = e.key.keysym.sym - SDLK_a + 10;
					break;
				default:
					return;
					break;
			}
			if (e.type == SDL_KEYDOWN)
				c8->key[key] = 1;
			else
				c8->key[key] = 0;
		}
	}
}
static inline uint16_t address(uint8_t *buf)
{
	return ((buf[0] & 0x0f) << 8) | (buf[1]);
}
static inline uint8_t value4(uint8_t *buf)
{
	return (buf[1] & 0x0f);
}
static inline int8_t value8(uint8_t *buf)
{
	return (buf[1]);
}
static inline uint8_t reg1(uint8_t *buf)
{
	return (buf[0] & 0x0f);
}
static inline uint8_t reg2(uint8_t *buf)
{
	return ((buf[1] & 0xf0) >> 4);
}
static inline void pc_advance(int n)
{
	chip8.ip += 2 * n;
}
static void clear_screen(void)
{
	chip8_video_clearscreen();
}
static void sub_call(uint16_t addr)
{
	chip8.stack[chip8.sp] = chip8.ip + 2;
	chip8.sp += 2;
	chip8.ip = addr;
}
static void sub_return(void)
{
	chip8.sp -= 2;
	assert(chip8.sp >= 0);
	chip8.ip = chip8.stack[chip8.sp];
}
static uint8_t wait_input(void)
{
	return chip8_video_key_wait();
}
static uint16_t rand16(void)
{
	return (uint16_t)(random() & 0xffff);
}
static uint16_t charat(uint8_t c)
{
	assert(c >= 0 && c <= 0xf);
	return (c * 5);
}
static int op0(uint8_t *buf)
{
	uint8_t p;

	p = (buf[0] & 0x0f);
	if (p == 0) {
		p = (buf[1]);
		if (p == 0xe0) {
			clear_screen();
		} else if (p == 0xee) {
			sub_return();
			return 0;
		} else if (p == OP_HLT) {
			chip8.hlt = 1;
		}
	} else {
		die("op 0x0NNN not implemented\n");
	}
	return 1;
}

static int op1(uint8_t *buf)
{
	chip8.ip = address(buf);
	return 0;
}

static int op2(uint8_t *buf)
{
	sub_call(address(buf));
	return 0;
}

static int op3(uint8_t *buf)
{
	if (c8->v[reg1(buf)] == value8(buf)) {
		pc_advance(2);
		return 0;
	}
	return 1;
}

static int op4(uint8_t *buf)
{
	if (c8->v[reg1(buf)] != value8(buf)) {
		pc_advance(2);
		return 0;
	}
	return 1;
}

static int op5(uint8_t *buf)
{
	if (c8->v[reg1(buf)] == c8->v[reg2(buf)]) {
		pc_advance(2);
		return 0;
	}
	return 1;
}

static int op6(uint8_t *buf)
{
	c8->v[reg1(buf)] = value8(buf);
	return 1;
}

static int op7(uint8_t *buf)
{
	c8->v[reg1(buf)] += value8(buf);
	return 1;
}

static int op8(uint8_t *buf)
{
	uint8_t *vx, *vy, *vf;
	int flag;
	flag = value4(buf);
	vx = &c8->v[reg1(buf)];
	vy = &c8->v[reg2(buf)];
	vf = &c8->v[0xf];
	switch (flag) {
		case 0:
			*vx = *vy;
			break;
		case 1:
			*vx |= *vy;
			break;
		case 2:
			*vx &= *vy;
			break;
		case 3:
			*vx ^= *vy;
			break;
		case 4:
			*vx += *vy;
			/* carry ? */
			if (*vx < *vy) {
				*vf= 1;
			}
			else {
				*vf = 0;
			}
			break;
		case 5:
			if (*vx < *vy) {
				*vf = 0;
			} else {
				*vf = 1;
			}
			*vx -= *vy;
			break;
		case 6:
			*vf = (*vx & 0x1);
			*vx = (*vx >> 1);
			break;
		case 7:
			if (*vy < *vx) {
				*vf = 0;
			} else {
				*vf = 1;
			}
			*vx = *vy - *vx;
			break;
		case 0xE:
			*vf = ((*vx & 0x80) >> 7);
			*vx = (*vx << 1);
			break;
		default:
			die("bad op8: %d\n", flag);
			break;
	}
	return 1;
}

static int op9(uint8_t *buf)
{
	if (c8->v[reg1(buf)] != c8->v[reg2(buf)]) {
		pc_advance(2);
		return 0;
	}
	return 1;
}

static int opa(uint8_t *buf)
{
	chip8.mp = address(buf);
	return 1;
}

static int opb(uint8_t *buf)
{
	chip8.ip = c8->v[0] + address(buf);
	return 0;
}

static int opc(uint8_t *buf)
{
	c8->v[reg1(buf)] = value8(buf) & rand16();
	return 1;
}

static int opd(uint8_t *buf)
{
	uint16_t op;

	op = (buf[0] << 8) | buf[1];
	c8->v[0xf] = chip8_video_draw(&c8->mem[c8->mp], c8->v[(op&0x0f00)>>8], c8->v[(op&0x00f0)>>4], (op&0x0f));
	return 1;
}

static int ope(uint8_t *buf)
{
	uint8_t flag = value8(buf);
	uint8_t key = c8->v[reg1(buf)];
	switch (flag) {
		case 0x9e:
			if (chip8.key[key] != 0) {
				pc_advance(1);
			}
			break;
		case 0xa1:
			if (chip8.key[key] == 0) {
				pc_advance(1);
			}
			break;
		default:
			die("bad ope: 0x%02x\n", flag);
			break;
	}
	return 1;
}

static int opf(uint8_t *buf)
{
	uint8_t *vx = &c8->v[reg1(buf)];
	uint8_t flag = value8(buf);
	switch (flag ) {
		case 0x07:
			*vx = chip8.dt;
			break;
		case 0x0a:
			*vx = wait_input();
			/*if no key was pressed, redo this instruction */
			if (*vx == 0xff)
				c8->ip -= 2;
			break;
		case 0x15:
			chip8.dt = *vx;
			break;
		case 0x18:
			chip8.st = *vx;
			break;
		case 0x1e:
			chip8.mp += *vx;
			break;
		case 0x29:
			chip8.mp = charat(*vx);
			break;
		case 0x33:
			chip8.mem[chip8.mp] = (*vx) / 100;
			chip8.mem[chip8.mp + 1] = ((*vx) % 100) / 10;
			chip8.mem[chip8.mp + 2] = (*vx) % 10;
			break;
		case 0x55:
			{
				int i;
				for (i = 0; i <= reg1(buf); i++) {
					chip8.mem[chip8.mp + i] = c8->v[i];
				}
			}
			break;
		case 0x65:
			{
				int i;
				for (i = 0; i <= reg1(buf); i++) {
					c8->v[i] = chip8.mem[chip8.mp + i];
				}
			}
			break;
		default:
			die("bad opf: 0x%02x\n", flag);
			break;
	}
	return 1;
}

typedef int (*op_fun_t) (uint8_t *);
static op_fun_t optables[] = {
	op0, op1, op2, op3, op4, op5, op6, op7,
	op8, op9, opa, opb, opc, opd, ope, opf,
};

static void chip8_decode(uint8_t *buf)
{
	uint8_t op;

	op = (buf[0] & 0xf0) >> 4;

	if (optables[op](buf)) {
		pc_advance(1);
	}
}

static void load_prog(const char *file)
{
	FILE * fp;
	void *mem;
	int rc;

	assert(file);

	fp = fopen(file, "r");
	assert(fp);
	mem = &chip8.mem[PROGRAM_MEM];
	while (!feof(fp)) {
		rc = fread(mem, 1, 256, fp);
		if (rc == 0) {
			if (ferror(fp)) {
				die("cannot load program\n");
			} else {	/* eof ? */
				//die("strange\n");
				break;
			}
		}
		mem += rc;
	}
	fclose(fp);
}
int main(int argc, char **argv)
{
	int i;
	uint32_t last, now;

	assert(argc == 2);

	srandom(time(NULL));

	chip8_init();

	load_prog(argv[1]);

	last = SDL_GetTicks();
	while (1) {
		chip8_video_key_process();
		chip8_decode(&c8->mem[c8->ip]);
		if (c8->hlt)
			break;
		if ((now = SDL_GetTicks()) - last >= 15) {
			if (c8->dt > 0)
				c8->dt--;
			if (c8->st > 0)
				c8->st--;
			last = now;
		}

	}

	chip8_close();

	return 0;
}
