#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <arpa/inet.h>

/* only support lower case */

typedef uint16_t	u16;
typedef uint8_t		u8;

#define die(fmt, args...) do { fprintf(stderr, fmt, ##args); exit(1); } while(0)

struct elem {
	char *str;
	struct elem *next;
};

typedef u16 (*op_fun_t) (struct elem *);

struct op {
	const char *name;
	op_fun_t fun;
};

struct label {
	char *str;
	u16 insn;
	u16 lineno;
	struct label *next;
};
struct label *ltable;

static int is_raw_label(const char *str)
{
	if (!strncasecmp(str, "L_", 2) && str[strlen(str)-1] == ':')
		return 1;
	return 0;
}
static int is_cooked_label(const char *str)
{
	if (!strncasecmp(str, "L_", 2))
		return 1;
	return 0;
}
static void add_label(const char *str, int lineno, int insno)
{
	struct label *lbl, *head;

	lbl = (struct label *)malloc(sizeof(*lbl));
	assert(lbl);
	/* strip ':' */
	lbl->str = strndup(str, strlen(str)-1);
	assert(lbl->str);
	lbl->insn = insno;
	lbl->lineno = lineno;
	lbl->next = NULL;
	if (!ltable) {
		ltable = lbl;
	} else {
		head = ltable;
		while (head->next) {
			head = head->next;
		}
		head->next = lbl;
	}
}
static void dump_ltable()
{
	struct label *head;

	head = ltable;
	while (head) {
		printf("label: str %s insn 0x%x line %d\n", head->str, head->insn, head->lineno);
		head = head->next;
	}
}
static void free_ltable()
{
	struct label *lbl, *head;

	head = ltable;
	ltable = NULL;
	while (head) {
		lbl = head->next;
		free(head->str);
		free(head);
		head = lbl;
	}
}

static int is_register(const char *str)
{
	assert(str);
	if (strlen(str) != 2)
		return 0;
	if (str[0] != 'v')
		return 0;
	if ((str[1] >= '0' && str[1] <= '9') || (str[1] >= 'a' && str[1] <= 'f'))
		return 1;
	return 0;
}
/* FIXME */
static int is_int(const char *str)
{
	if (isdigit(str[0]))
		return 1;
	return 0;
}
/* FIXME */
static unsigned get_int(const char *str)
{
	assert(str);
	return strtoul(str, NULL, 0);
}
static int get_reg(const char *str)
{
	assert(str);
	if (isdigit(str[1]))
		return (str[1] - '0');
	else
		return (str[1] - 'a' + 10);
}
static unsigned get_insn(const char *str)
{
	struct label *head;

	head = ltable;
	while (head) {
		if (!strcmp(str, head->str))
			return head->insn;
		head = head->next;
	}
	die("undefined label: %s", str);
}
static unsigned get_address(const char *str)
{
	if (is_int(str)) {
		return get_int(str);
	} else if (is_cooked_label(str)) {
		//printf("lable %s resolved: %d\n", str, get_insn(str));
		return get_insn(str);
	} else
		die("bad jmp/call target: %s\n", str);
}
static u16 make2(int op, int arg)
{
	u16 res;
	arg &= 0x0fff;
	res = op;
	res = (res << 4) | (arg >> 8);
	res = (res << 8) | (arg & 0x0ff);
	return htons(res);
}
static u16 make3(int op, int arg1, int arg2)
{
	u16 res;
	res = op;
	res = (res << 4) | arg1;
	res = (res << 8) | arg2;
	return htons(res);
}
static u16 make4(int op, int arg1, int arg2, int arg3)
{
	u16 res;
	res = op;
	res = (res << 4) | arg1;
	res = (res << 8) | ((arg2 << 4) | arg3);
	return htons(res);
}
static u16 make_bytes(int arg1, int arg2)
{
	u16 res;
	res = arg1;
	res = (res << 8) | arg2;
	return htons(res);
}
u16 do_add(struct elem *e)
{
	char *arg1, *arg2;
	u8 v1, v2;

	arg1 = e->str;
	arg2 = e->next->str;
	assert(e->next->next == NULL);
	assert(is_register(arg2));
	v2 = get_reg(arg2);
	if (!is_register(arg1)) {
		v1 = get_int(arg1);
		return make3(7, v2, v1);
	} else {
		v1 = get_reg(arg1);
		return make4(8, v2, v1, 4);
	}
}
u16 do_and(struct elem *e)
{
	char *arg1, *arg2;
	u8 v1, v2;

	arg1 = e->str;
	arg2 = e->next->str;
	assert(e->next->next == NULL);
	assert(is_register(arg1));
	assert(is_register(arg2));
	v1 = get_reg(arg1);
	v2 = get_reg(arg2);
	return make4(8, v2, v1, 2);
}
u16 do_or(struct elem *e)
{
	char *arg1, *arg2;
	arg1 = e->str;
	arg2 = e->next->str;
	assert(e->next->next == NULL);
	assert(is_register(arg1));
	assert(is_register(arg2));
	return make4(8, get_reg(arg2), get_reg(arg1), 1);
}
u16 do_xor(struct elem *e)
{
	char *arg1, *arg2;
	arg1 = e->str;
	arg2 = e->next->str;
	assert(e->next->next == NULL);
	assert(is_register(arg1));
	assert(is_register(arg2));
	return make4(8, get_reg(arg2), get_reg(arg1), 3);
}
u16 do_cs(struct elem *e)
{
	assert(e == NULL);
	return make4(0, 0, 0xe, 0);
}
u16 do_j(struct elem *e)
{
	assert(e->next == NULL);
	assert(!is_register(e->str));
	return make2(1, get_address(e->str));
}
u16 do_j0(struct elem *e)
{
	assert(e->next == NULL);
	assert(!is_register(e->str));
	return make2(0xb, get_address(e->str));
}
u16 do_je(struct elem *e)
{
	char *arg1, *arg2;

	arg1 = e->str;
	arg2 = e->next->str;
	assert(e->next->next == NULL);
	if (is_register(arg1)) {
		if (is_register(arg2)) {
			return make4(5, get_reg(arg1), get_reg(arg2), 0);
		} else {
			return make3(3, get_reg(arg1), get_int(arg2));
		}
	} else {
		assert(is_register(arg2));
		return make3(3, get_reg(arg2), get_int(arg1));
	}
}
u16 do_jne(struct elem *e)
{
	char *arg1, *arg2;

	arg1 = e->str;
	arg2 = e->next->str;
	assert(e->next->next == NULL);
	if (is_register(arg1)) {
		if (is_register(arg2)) {
			return make4(9, get_reg(arg1), get_reg(arg2), 0);
		} else {
			return make3(4, get_reg(arg1), get_int(arg2));
		}
	} else {
		assert(is_register(arg2));
		return make3(4, get_reg(arg2), get_int(arg1));
	}
}
u16 do_jk(struct elem *e)
{
	assert(e->next == NULL);
	assert(is_register(e->str));
	return make3(0xe, get_reg(e->str), 0x9e);
}
u16 do_jnk(struct elem *e)
{
	assert(e->next == NULL);
	assert(is_register(e->str));
	return make3(0xe, get_reg(e->str), 0xa1);
}
u16 do_wait(struct elem *e)
{
	assert(e->next == NULL);
	assert(is_register(e->str));
	return make3(0xf, get_reg(e->str), 0x0a);
}
u16 do_call(struct elem *e)
{
	assert(e->next == NULL);
	assert(!is_register(e->str));
	return make2(2, get_address(e->str));
}
u16 do_ret(struct elem *e)
{
	assert(e == NULL);
	return make4(0, 0, 0xe, 0xe);
}
u16 do_bcd(struct elem *e)
{
	assert(e->next == NULL);
	assert(is_register(e->str));
	return make3(0xf, get_reg(e->str), 0x33);
}
u16 do_mov(struct elem *e)
{
	char *arg1, *arg2;
	arg1 = e->str;
	arg2 = e->next->str;
	assert(e->next->next == NULL);
	assert(is_register(arg2));
	if (is_register(arg1)) {
		return make4(8, get_reg(arg2), get_reg(arg1), 0);
	} else {
		assert(get_int(arg1) < 0x100);
		return make3(6, get_reg(arg2), get_int(arg1));
	}
}
u16 do_rand(struct elem *e)
{
	char *arg1, *arg2;
	arg1 = e->str;
	arg2 = e->next->str;
	assert(e->next->next == NULL);
	assert(!is_register(arg1));
	assert(is_register(arg2));
	return make3(0xc, get_reg(arg2), get_int(arg1));
}
u16 do_i(struct elem *e)
{
	assert(e->next == NULL);
	assert(!is_register(e->str));

	if (is_int(e->str))
		return make2(0xa, get_int(e->str));
	else if (is_cooked_label(e->str))
		return make2(0xa, get_insn(e->str));
}
u16 do_ix(struct elem *e)
{
	assert(e->next == NULL);
	assert(is_register(e->str));
	return make3(0xf, get_reg(e->str), 0x1e);
}
u16 do_is(struct elem *e)
{
	assert(e->next == NULL);
	assert(is_register(e->str));
	return make3(0xf, get_reg(e->str), 0x29);
}
u16 do_sound(struct elem *e)
{
	assert(e->next == NULL);
	assert(is_register(e->str));
	return make3(0xf, get_reg(e->str), 0x18);
}
u16 do_delay(struct elem *e)
{
	assert(e->next == NULL);
	assert(is_register(e->str));
	return make3(0xf, get_reg(e->str), 0x15);
}
u16 do_ldelay(struct elem *e)
{
	assert(e->next == NULL);
	assert(is_register(e->str));
	return make3(0xf, get_reg(e->str), 0x7);
}
u16 do_sub(struct elem *e)
{
	assert(e->next->next == NULL);
	assert(is_register(e->str));
	assert(is_register(e->next->str));
	return make4(8, get_reg(e->next->str), get_reg(e->str), 5);
}
u16 do_subv(struct elem *e)
{
	assert(e->next->next == NULL);
	assert(is_register(e->str));
	assert(is_register(e->next->str));
	return make4(8, get_reg(e->next->str), get_reg(e->str), 7);
}
u16 do_shl(struct elem *e)
{
	assert(e->next == NULL);
	assert(is_register(e->str));
	return make3(8, get_reg(e->str), 0xe);
}
u16 do_shr(struct elem *e)
{
	assert(e->next == NULL);
	assert(is_register(e->str));
	return make3(8, get_reg(e->str), 6);
}
u16 do_load(struct elem *e)
{
	assert(e->next == NULL);
	assert(!is_register(e->str));
	return make3(0xf, get_int(e->str), 0x65);
}
u16 do_store(struct elem *e)
{
	assert(e->next == NULL);
	assert(!is_register(e->str));
	return make3(0xf, get_int(e->str), 0x55);
}
u16 do_draw(struct elem *e)
{
	assert(e->next->next->next == NULL);
	assert(is_register(e->str));
	assert(is_register(e->next->str));
	assert(!is_register(e->next->next->str));
	return make4(0xd, get_reg(e->str), get_reg(e->next->str), get_int(e->next->next->str));
}
u16 do_hlt(struct elem *e)
{
	assert(e == NULL);
	return make3(0x0, 0, 0xff);
}
u16 do_byte(struct elem *e)
{
	assert(e->next->next == NULL);
	return make_bytes(get_int(e->str), get_int(e->next->str));
}
struct op optables[] = {
	{"add", do_add},
	{"and", do_and},
	{"bcd", do_bcd},
	{"call", do_call},
	{"cs", do_cs},
	{"draw", do_draw},
	{"i", do_i},
	{"j0", do_j0},
	{"ix", do_ix},
	{"is", do_is},
	{"j", do_j},
	{"je", do_je},
	{"jne", do_jne},
	{"jk", do_jk},
	{"jnk", do_jnk},
	{"ldelay", do_ldelay},
	{"load", do_load},
	{"mov", do_mov},
	{"rand", do_rand},
	{"or", do_or},
	{"ret", do_ret},
	{"delay", do_delay},
	{"shl", do_shl},
	{"shr", do_shr},
	{"sound", do_sound},
	{"store", do_store},
	{"sub", do_sub},
	{"subv", do_subv},
	{"wait", do_wait},
	{"xor", do_xor},
	{"hlt", do_hlt},
	/* pesudo op */
	{".byte", do_byte},
};

FILE *openfile_in(const char *f)
{
	FILE *fp;
	assert(f);
	fp = fopen(f, "r");
	assert(fp);
	return fp;
}
FILE *openfile_out(const char *f)
{
	char *fout;
	FILE *fp;
	size_t len;

	len = strlen(f) + strlen(".rom") + 1;
	fout = (char *)malloc(len);
	assert(fout);
	snprintf(fout, len, "%s.rom", f);
	fp = fopen(fout, "wb");
	assert(fp);
	return fp;
}
static void free_elem(struct elem *e)
{
	struct elem *n;
	while (n = e) {
		e = e->next;
		free(n->str);
		free(n);
	}
}
static void dump_elem(struct elem *e)
{
	while (e) {
		fprintf(stderr, "%s ", e->str);
		e = e->next;
	}
	fprintf(stderr, "\n");
}
static struct elem *lex(char *line, int lineno)
{
	char *s, *n;
	struct elem *e, *p;

	e = NULL;
	p = NULL;
	s = line;
redo:
	while (*s == ' ' || *s == '\t') {
		s++;
		continue;
	}
	if (*s == '\n' || *s == '\0')
		return e;
	n = s;
	while (*n != ' ' && *n != '\t' && *n != '\n' && *n != '\0') {
		n++;
	}
	if (*n != '\0') {
		*n = 0;
		n++;
	}
	if (!p) {
		p = (struct elem *)malloc(sizeof(*p));
		assert(p);
	} else {
		p->next = (struct elem *)malloc(sizeof(*p));
		assert(p->next);
		p = p->next;
	}
	p->next = NULL;
	p->str = strdup(s);
	assert(p->str);
	s = n;
	if (!e) {
		e = p;
	}
	goto redo;
	return NULL;
}
static op_fun_t find_op(char *str)
{
	int i;
	struct op *ope;
#define ARRAY_SIZE(a)	(sizeof(a) / sizeof(a[0]))
	for (i = 0; i < ARRAY_SIZE(optables); i++) {
		ope = &optables[i];
		if (!strcmp(ope->name, str))
			return ope->fun;
	}
	return NULL;
}
static void assemble(char *line, int lineno, FILE *fp)
{
	struct elem *e;
	op_fun_t handler;
	uint16_t opcode;

	assert(line);
	assert(fp);

	e = lex(line, lineno);
	if (!e) {
		return;
	}
	//dump_elem(e);
	if (is_raw_label(e->str))
		return;

	handler = find_op(e->str);
	if (!handler) {
		die("line %d: bad op: %s\n", lineno, e->str);
	}
	opcode = handler(e->next);
	free_elem(e);
	assert(fwrite(&opcode, 1, 2, fp) == 2);
}
static void preprocess(char *line, int lineno, int *insno)
{
	struct elem *e;

	assert(line);

	e = lex(line, lineno);
	if (!e) {
		return;
	}
	if (is_raw_label(e->str)) {
		assert(e->next == NULL);
		add_label(e->str, lineno, *insno);
	} else { /* opcode */
		*insno += 2;
	}

	free_elem(e);
}
int main(int argc, char **argv)
{
	assert(argc == 2);
	FILE *fpin, *fpout;
	#define LINE_LEN	128
	char line[LINE_LEN];
	char *s;
	int lineno;
	int insno;

	fpin = openfile_in(argv[1]);
	fpout = openfile_out(argv[1]);

	/* first pass to collect labels */
	lineno = 1;
	insno = 0x200;
	while (!feof(fpin)) {
		if (!fgets(line, LINE_LEN - 1, fpin)) {
			if (feof(fpin)) {
				break;
			} else {
				die("cannot read file\n");
			}
		}
		preprocess(line, lineno, &insno);
		lineno++;
	}
	//dump_ltable();

	assert(0 == fseek(fpin, 0, SEEK_SET));
	/* second pass */
	lineno = 1;
	while (!feof(fpin)) {
		if (!fgets(line, LINE_LEN - 1, fpin)) {
			if (feof(fpin)) {
				break;
			} else {
				die("cannot read file\n");
			}
		}
		assemble(line, lineno, fpout);
		lineno++;
	}

	fclose(fpin);
	fclose(fpout);

	free_ltable();

	return 0;
}
