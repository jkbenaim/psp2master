#pragma once

#include <stdio.h>
#include <stdarg.h>
#ifndef dprintf
extern int dprintf(int fd, const char *restrict format, ...)
	__attribute__((__format__ (__printf__, 2, 3)));
#endif
#ifndef vdprintf
extern int vdprintf(int fd, const char *restrict format, va_list ap)
	__attribute__((__format__ (__printf__, 2, 0)));
#endif
