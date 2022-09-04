#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#ifndef vdprintf
int vdprintf(int fd, const char *restrict format, va_list ap)
{
	int rc;
	char *buf;
	int siz;
	siz = vsnprintf(NULL, 0, format, ap);
	buf = (char *)malloc(siz+1);
	vsnprintf(buf, siz+1, format, ap);
	rc = write(fd, buf, siz);
	free(buf);
	return rc;
}
#endif

#ifndef dprintf
int dprintf(int fd, const char *restrict format, ...)
{
	va_list ap;
	int rc;
	va_start(ap, format);
	rc = vdprintf(fd, format, ap);
	va_end(ap);
	return rc;
}
#endif

