#include <stdlib.h>
#include <string.h>
#include "base64.h"

const unsigned char b64a[64] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

char *base64_encode(const uint8_t *in, size_t inlen)
{
	size_t outlen;
	char *out, *outp;
	uint8_t tail[3] = {0};

	outlen = ((inlen + 2) / 3) * 4 + 1;
	out = outp = malloc(outlen);
	if (!out) return NULL;

	while (inlen >= 3) {
		*outp++ = b64a[ (in[0] >> 2) & 0x3f];
		*outp++ = b64a[((in[0] << 4) + (in[1] >> 4)) & 0x3f];
		*outp++ = b64a[((in[1] << 2) + (in[2] >> 6)) & 0x3f];
		*outp++ = b64a[in[2] & 0x3f];
		inlen -= 3;
		in += 3;
	}

	memcpy(tail, in, inlen);
	in = tail;
	switch (inlen) {
	case 2:
		*outp++ = b64a[ (in[0] >> 2) & 0x3f];
		*outp++ = b64a[((in[0] << 4) + (in[1] >> 4)) & 0x3f];
		*outp++ = b64a[((in[1] << 2) + (in[2] >> 6)) & 0x3f];
		*outp++ = '=';
		break;
	case 1:
		*outp++ = b64a[ (in[0] >> 2) & 0x3f];
		*outp++ = b64a[((in[0] << 4) + (in[1] >> 4)) & 0x3f];
		*outp++ = '=';
		*outp++ = '=';
		break;
	case 0:
	default:
		break;
	}

	out[outlen - 1] = '\0';
	return out;
}
