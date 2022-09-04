#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include "endian.h"
#include "err.h"
#include "sfo.h"

#define ARRAY_SIZE(x) (sizeof(x)/sizeof(*x))

char *keytype_tostring(enum keytype t)
{
	switch (t) {
	case KT_INVALID:
		return "(invalid)";
	case KT_BINARY:
		return "data";
	case KT_STRING:
		return "string";
	case KT_UINT32:
		return "int";
	default:
		return "(idk)";
	}
}

struct sfo_keyrow keyrow2h(struct sfo_keyrow k)
{
	struct sfo_keyrow out;
	out.offset = le16toh(k.offset);
	out.type = le16toh(k.type);
	out.len = le32toh(k.len);
	out.maxlen = le32toh(k.maxlen);
	out.data_offset = le32toh(k.data_offset);
	return out;
}

struct sfo_hdr hdr2h(struct sfo_hdr h)
{
	struct sfo_hdr out;
	memcpy(&out.magic, &h.magic, sizeof(out.magic));
	out.ver = le32toh(h.ver);
	out.stab = le32toh(h.stab);
	out.btab = le32toh(h.btab);
	out.numrows = le32toh(h.numrows);
	return out;
}

uint32_t sfo_numrows(struct sfo_ctx *ctx)
{
	return hdr2h(*ctx->hdr).numrows;
}

struct sfo_row sfo_get_row_by_index(struct sfo_ctx *ctx, int index)
{
	struct sfo_row out;

	if (index >= sfo_numrows(ctx))
		return (struct sfo_row){.type = KT_INVALID};
	
	struct sfo_keyrow k = keyrow2h(ctx->hdr->rows[index]);
	out.type = k.type;
	out.len = k.len;
	out.maxlen = k.maxlen;

	out.key = ctx->keys[index];
	out.val = (uint8_t *)ctx->hdr + hdr2h(*ctx->hdr).btab + k.data_offset;

	return out;
}

int sfo_get_index_for_key(struct sfo_ctx *ctx, char *key)
{
	for (size_t i = 0; i < sfo_numrows(ctx); i++) {
		if (!strcmp(key, ctx->keys[i]))
			return i;
	}
	return -1;
}

struct sfo_row sfo_get_row_for_key(struct sfo_ctx *ctx, char *key)
{
	int index;

	index = sfo_get_index_for_key(ctx, key);
	if (index < 0) return (struct sfo_row) {.type = KT_INVALID};
	
	return sfo_get_row_by_index(ctx, index);
}

char *sfo_get_string_for_key(struct sfo_ctx *ctx, char *key)
{
	struct sfo_row row = sfo_get_row_for_key(ctx, key);
	if (row.type != KT_STRING) return NULL;
	return (char *)row.val;
}

int_least64_t sfo_get_num_for_key(struct sfo_ctx *ctx, char *key)
{
	struct sfo_row row = sfo_get_row_for_key(ctx, key);
	if (row.type != KT_UINT32) return -1;
	return *(uint32_t *)row.val;
}

int sfo_init(struct sfo_ctx *ctx, void *data)
{
	ctx->hdr = (struct sfo_hdr *)data;

	// hack to support PSFs embedded in PBP files
	if (!memcmp(hdr2h(*ctx->hdr).magic, "\0PBP", 4))
		ctx->hdr = (struct sfo_hdr *)((uint8_t *)ctx->hdr + 0x28);
	
	if (memcmp(hdr2h(*ctx->hdr).magic, "\0PSF", 4))
		return -1;

	if (hdr2h(*ctx->hdr).ver != 0x101)
		return -2;
	
	// build keytab ptrs
	ctx->keys = calloc(sfo_numrows(ctx), sizeof(char *));
	if (!ctx->keys) err(1, "in calloc");
	for (int i = 0; i < sfo_numrows(ctx); i++) {
		ctx->keys[i] = (char *)ctx->hdr + hdr2h(*ctx->hdr).stab + ctx->hdr->rows[i].offset;
	}

	return 0;
}

void sfo_free(struct sfo_ctx *ctx)
{
	free(ctx->keys);
	ctx->keys = NULL;
	ctx->hdr = NULL;
}

struct {
	char *key;
	char *desc;
} key_descriptions[] = {
	{ "APP_VER", "Application version" },
	{ "BOOTABLE", "Bootable" },
	{ "CATEGORY", "Category" },
	{ "DISC_ID", "Disc ID" },
	{ "DISC_NUMBER", "Disc number" },
	{ "DISC_TOTAL", "Disc total" },
	{ "DISC_VERSION", "Disc version" },
	{ "PARENTAL_LEVEL", "Parental level" },
	{ "PSP_SYSTEM_VER", "Minimum PSP system version" },
	{ "REGION", "Region flags" },
	{ "SAVEDATA_DETAIL", "Detail" },
	{ "SAVEDATA_DIRECTORY", "Directory name" },
	{ "SAVEDATA_FILE_LIST", "File list" },
	{ "SAVEDATA_PARAMS", "Save parameters" },
	{ "SAVEDATA_TITLE", "Save title" },
	{ "TITLE", "Title" },
//	{ "HRKGMP_VER", "" },
	{ "USE_USB", "Use USB" },
	{ "LICENSE", "License text" },
	{ "UPDATE_VER", "Update version" },
};

const struct {
	char *key;
	char *note;
} key_notes[] = {
	{ "HRKGMP_VER", "So far, nobody knows what this means." },
	{ "REGION", "Only the value 0x8000 has been observed." },
};

char *get_note_for_key(char *key)
{
	for (int i = 0; i < ARRAY_SIZE(key_notes); i++) {
		if (!strcmp(key_notes[i].key, key)) {
			return key_notes[i].note;
		}
	}
	return NULL;
}

const struct {
	char *cat;
	char *desc;
} cat_descs[] = {
	{ "EG", "Downloaded game" },
	{ "ME", "PS1 Classics" },
	{ "MG", "PSP system update" },
	{ "MS", "Save game" },
	{ "UG", "UMD game" },
};

char *get_desc_for_cat(char *cat)
{
	for (int i = 0; i < ARRAY_SIZE(cat_descs); i++) {
		if (!strcmp(cat_descs[i].cat, cat)) {
			return cat_descs[i].desc;
		}
	}
	return NULL;
}

char *_escape_aux(char *s, char *newline)
{
	size_t bufsiz = 1024;
	size_t bufused = 0;
	char *buf = malloc(bufsiz);
	if (!buf) err(1, "in malloc");
	memset(buf, '\0', bufsiz);
	
	void emit(char c) {
		if (bufused == bufsiz) {
			bufsiz += 1024;
			buf = realloc(buf, bufsiz);
			if (!buf) err(1, "in realloc");
		}
		buf[bufused++] = c;
	}

	size_t len = strlen(s);
	for (size_t i = 0; i < len ; i++) {
		switch (s[i]) {
		case '\n':
			for (int i = 0; i < strlen(newline); i++)
				emit(newline[i]);
			break;
		default:
			emit(s[i]);
			break;
		}
	}

	emit('\0');
	return buf;
}

char *escape_c(char *s)
{
	return _escape_aux(s, "\\n");
}

char *escape_html(char *s)
{
	return _escape_aux(s, "<br/>");
}

char *escape_xml(const char *orig, bool nobreaks)
{
	char *out = NULL;
	size_t outLength = 0;
	size_t origLength;
	const char *inp = orig;
	char *outp;
	char *test;
	size_t i;

	if (!orig) return NULL;

	origLength = strlen(orig);

	out = malloc(origLength*strlen("&amp;") + 1);
	if (!out) err(1, "in malloc");

	outp = out;

	for (i = 0; i < origLength; i++) {
		char suspect = *inp++;
		switch (suspect) {
		case '&':
			*outp++ = '&';
			*outp++ = 'a';
			*outp++ = 'm';
			*outp++ = 'p';
			*outp++ = ';';
			outLength += 5;
			break;
		case '<':
			*outp++ = '&';
			*outp++ = 'l';
			*outp++ = 't';
			*outp++ = ';';
			outLength += 4;
			break;
		case ' ':
			if (nobreaks) {
				*outp++ = '&';
				*outp++ = 'n';
				*outp++ = 'b';
				*outp++ = 's';
				*outp++ = 'p';
				*outp++ = ';';
				outLength += 6;
			} else {
				*outp++ = suspect;
				outLength += 1;
			}
			break;
		default:
			*outp++ = suspect;
			outLength += 1;
			break;
		}
	}
	*outp = '\0';
	outLength++;
	test = realloc(out, outLength);
	if (test) {
		out = test;
	}
	return out;
}

