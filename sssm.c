
#include <bsd/string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <X11/Xlib.h>

#include "arg.h"
char *argv0;

/* Macros */
#define LEN(a)          (sizeof(a) / sizeof(a)[0])
#define OUTPUTC(c)      *(outp++) = (c)
#define OUTPUTS(s)      outp += strlcpy(outp, (s), outsize - (size_t)(outp - out))

/* Structs */
typedef struct {
	char head;
	void (*func)(char *);
} Method;

typedef struct {
	char *begin;
	char *end;
	char head;
	char *subexpr;
} Spec;

/* Functions used in config.h */
static void runcmd(char *cmd);

/* Configuration */
#include "config.h"

/* Functions */
static void callmethod(char head, char *subexpr);
static void die(char *errstr, ...);
static Spec parsespec(char *str);
static void strfstat(void);
static void usage(void);
static void xsetroot(char *name);

/* Globals */
Display *dpy = NULL;
char *outp;

char out[1024]; /* output buffer */
char buf[1024]; /* buffer allocated for modules */

/* Array lengths */
size_t fmtsize = LEN(fmt);
size_t outsize = LEN(out);
size_t bufsize = LEN(buf);
size_t methodslen = LEN(methods);


void
callmethod(char head, char *subexpr)
{
	int i;

	for (i = 0; i < methodslen && methods[i].head != head; i++);
	if (i == methodslen)
		die("invalid method @%c", head);
	methods[i].func(subexpr);
}

void
die(char *errstr, ...)
{
	va_list ap;

	va_start(ap, errstr);
	vfprintf(stderr, errstr, ap);
	va_end(ap);
	exit(1);
}

Spec
parsespec(char *str) {
	// assert(*str == '@');
	char *strp;
	Spec spec = {
		.begin = str,
		.end = &str[1],
		.head = str[1],
		.subexpr = NULL,
	};

	if (str[2] != '[')
		return spec;

	spec.subexpr = strp = &str[3];
	for (; *strp != ']'; strp++) {
		if (*strp == '\0')
			die("sub-expression for @%c does not terminate.", spec.head);
		if (*strp == '\\')
			strp++;
	}
	spec.end = strp;

	return spec;
}

void
runcmd(char *cmd)
{
	FILE *fp;
	int ret;

	fp = popen(cmd, "r");
	do {
		ret = fread(buf, sizeof(char), bufsize, fp);
		buf[ret] = '\0';
		OUTPUTS(buf);
	} while (!feof(fp));

	/* trim newlines at the end */
	while(*(outp-1) == '\n')
		*(--outp) = '\0';
	pclose(fp);	
}

void
strfstat(void)
{
	Spec spec;
	char *fmtp;

	outp = out;
	for (fmtp = fmt; *fmtp; fmtp++) {
		if (*fmtp == '@') {
			spec = parsespec(fmtp);
			if (spec.subexpr)
				spec.end[0] = '\0';
			callmethod(spec.head, spec.subexpr);
			if (spec.subexpr)
				spec.end[0] = ']';
			fmtp = spec.end;
		} else
			OUTPUTC(*fmtp);
	}
	OUTPUTC('\0');
}

void
usage(void)
{
	fputs("usage: sssm [-lw] [format]\n", stderr);
	exit(0);
}

void
xsetroot(char *name)
{
	XStoreName(dpy, DefaultRootWindow(dpy), name);
	XSync(dpy, 0);
}

int
main(int argc, char *argv[])
{
	struct timespec delay = {0};

	ARGBEGIN {
	case 'l': /* loop */
		mode_loop = 1;
		break;
	case 'x': /* xsetroot mode */
		mode_xsetroot = 1;
		break;
	} ARGEND;

	if (argc > 1)
		usage();
	if (argc == 1)
		strncpy(fmt, argv[0], fmtsize);

	if (mode_xsetroot) {
		if (!(dpy = XOpenDisplay(NULL))) {
			fprintf(stderr, "sssm: cannot open display\n");
			return 1;
		}
	}

	if (mode_loop)
		delay = (struct timespec) {
			.tv_sec = interval / 1000,
			.tv_nsec = (long)(interval % 1000) * 1000 * 1000
		};

	do {
		strfstat();

		if(mode_xsetroot)
			xsetroot(out);
		else
			puts(out);

		nanosleep(&delay, NULL);
	} while(mode_loop);

	return 0;
}