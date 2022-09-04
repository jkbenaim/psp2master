#pragma once

#include <inttypes.h>
#include <stdbool.h>

enum keytype {
	KT_INVALID = 0,
	KT_BINARY = 0x0004,
	KT_STRING = 0x0204,
	KT_UINT32 = 0x0404,
};

struct sfo_keyrow {
	uint16_t offset;
	uint16_t type;
	uint32_t len;
	uint32_t maxlen;
	uint32_t data_offset;
} __attribute__((packed));

struct sfo_hdr {
	char magic[4];
	uint32_t ver;
	uint32_t stab;
	uint32_t btab;
	uint32_t numrows;
	struct sfo_keyrow rows[];
} __attribute__((packed));

struct sfo_ctx {
	struct sfo_hdr *hdr;
	char **keys;
};

struct sfo_row {
	char *key;
	void *val;
	uint32_t len;
	uint32_t maxlen;
	enum keytype type;
};

struct sfo_keyrow keyrow2h(struct sfo_keyrow k);
struct sfo_hdr hdr2h(struct sfo_hdr h);
char *keytype_tostring(enum keytype t);
uint32_t sfo_numrows(struct sfo_ctx *ctx);
struct sfo_row sfo_get_row_by_index(struct sfo_ctx *ctx, int index);
int sfo_get_index_for_key(struct sfo_ctx *ctx, char *key);
struct sfo_row sfo_get_row_for_key(struct sfo_ctx *ctx, char *key);
char *sfo_get_string_for_key(struct sfo_ctx *ctx, char *key);
int_least64_t sfo_get_num_for_key(struct sfo_ctx *ctx, char *key);
int sfo_init(struct sfo_ctx *ctx, void *data);
void sfo_free(struct sfo_ctx *ctx);
char *escape_c(char *s);
char *escape_xml(const char *orig, bool nobreaks);
char *get_note_for_key(char *key);
char *get_desc_for_cat(char *cat);
