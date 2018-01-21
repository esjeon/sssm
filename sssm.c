/* 
 * Copyright 2018 Eon S. Jeon <esjeon@hyunmu.am>
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

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


/* Functions */
static void callmethod(char head, char *subexpr);
static void die(char *errstr, ...);
static void method_date(char *subexpr);
static void method_exec(char *cmd);
static Spec parsespec(char *str);
static void strfstat(void);
static void usage(void);
static void xsetroot(char *name);


/* Globals */
Display *dpy = NULL;

char buf[1024]; /* buffer allocated for modules */
size_t bufsize = LEN(buf);

char out[1024]; /* output buffer */
char *outp;
size_t outsize = LEN(out);

char usedfmt[1024];
size_t usedfmtsize = LEN(usedfmt);


/* Configuration */
#include "config.h"
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
method_date(char *subexpr)
{
	struct tm now_tm;
	time_t now_time;

	if (subexpr == NULL)
		subexpr = "%c";

	if ((now_time = time(NULL)) == -1) {
		OUTPUTS("#time-error");
		return;
	}

	/* TODO: might need error handling here */
	localtime_r(&now_time, &now_tm);
	strftime(buf, bufsize, subexpr, &now_tm);
	OUTPUTS(buf);
}

void
method_exec(char *cmd)
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
	for (fmtp = usedfmt; *fmtp; fmtp++) {
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
	fputs("usage: sssm [-hlw] [-i msec] [format]\n", stderr);
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
	char *strp;
	char *endp;

	ARGBEGIN {
	case 'i': /* interval */
		strp = EARGF(usage());
		interval = strtol(strp, &endp, 10);
		if (endp[0] != '\0' || interval <= 0)
			usage();
		break;
	case 'l': /* loop */
		mode_loop = 1;
		break;
	case 'x': /* xsetroot mode */
		mode_xsetroot = 1;
		break;
	case 'h':
		usage();
	} ARGEND;

	if (argc == 0)
		strncpy(usedfmt, fmt, usedfmtsize);
	else if(argc == 1)
		strncpy(usedfmt, argv[0], usedfmtsize);
	else
		usage();

	if (mode_xsetroot) {
		if (!(dpy = XOpenDisplay(NULL)))
			die("cannot open display\n");
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
