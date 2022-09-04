#include <ctype.h>
#include <inttypes.h>
#include <iso646.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <string.h>
#include <unistd.h>

#include "base64.h"
#include "dprintf.h"
#include "endian.h"
#include "err.h"
#include "hexdump.h"
#include "makehtml.h"
#include "sfo.h"
#include "version.h"

const char paramsfo_name[] = "PSP_GAME/PARAM.SFO";
const char icon_name[] = "PSP_GAME/ICON0.PNG";

const char html_header[] =
R"(<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Strict//EN"
        "http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd">
<html xmlns="http://www.w3.org/1999/xhtml" lang="en" xml:lang="en">
<head>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8"/>
<meta name="viewport" content="width=device-width, initial-scale=1"/>
<meta name="generator" content="%s"/>
<title>PSP Master Disc - %s</title>
<style type="text/css">
body {
	font-family: sans-serif;
}
@media (min-width: 1200px) {
body {
	max-width: 1000px;
	margin-left: auto;
	margin-right: auto;
}
}
table {border: 1px solid lightgrey; border-collapse: collapse;table-layout:fixed;margin-bottom:1em;}
caption {background-color: grey; color: white;font: italic bold 1.5em sans-serif;padding:10px;}
td {border: 1px solid lightgrey; font: 1em monospace;padding:10px;}
th {font: 1em monospace;}
th:nth-child(1),th:nth-child(2) {width:150px;}
.note {color: gray; border-bottom: 1px dotted;}
.keydesc {color: gray; font-style: italic;}
</style>

<script id="lang-fi" type="application/json">
{
	"APP_VER": "Sovellusversio",
	"BOOTABLE": "Voidaan bootata"
}
</script>

<script type="application/json" id="rfc5646">
{
	"ja": 0,
	"en": 1,
	"fr": 2,
	"es": 3,
	"de": 4,
	"it": 5,
	"nl": 6,
	"pt": 7,
	"ru": 8,
	"ko": 9,
	"ch": 10,
	"zh": 11
}
</script>

<script type="text/javascript">
function localize(lang) {
	const strtab = JSON.parse(document.getElementById("lang-" + lang).text);
	let params = document.getElementById('params').getElementsByTagName('tbody')[0];
	let rows = params.getElementsByTagName('tr');

	for (let r of rows) {
		let keytag = r.getElementsByClassName('key')[0];
		let key = keytag.textContent;
		if (strtab.hasOwnProperty(key)) {
			keytag.innerHTML += '<br/><span class="keydesc">' + strtab[key] + '</span>';
		}
	}
	console.log(strtab);
};

function boot() {
	//localize('fi');
}
document.addEventListener("DOMContentLoaded", boot);

</script>
</head>
<body>

<h1>PSP Master Disc</h1>

<table id="params">
<caption>%s%s</caption>
<thead>
<tr>
	<th scope="col">key</th>
	<th scope="col">value</th>
</tr>
</thead>
<tbody>
)";

void z(const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
}

char *make_icontag(uint8_t *png, size_t pnglen)
{
	const char template[] = "<img src=\"data:image/png;base64,%s\" alt=\"game icon\"/><br />";
	char *b64;
	char *out;
	size_t outlen;

	b64 = base64_encode(png, pnglen);
	if (!b64) {
		return NULL;
	}
	outlen = strlen(b64);
	outlen += strlen(template);
	outlen -= strlen("%s");
	outlen += 1;
	out = malloc(outlen);
	if (!out) {
		free(b64);
		return NULL;
	}
	sprintf(out, template, b64);
	free(b64);
	out[outlen - 1] = '\0';
	return out;
}

int make_html(iso9660_t *ctx, int outfd)
{
	size_t paramsfo_bytes;
	size_t paramsfo_secsize;
	size_t paramsfo_lsn;
	size_t icon_bytes;
	size_t icon_secsize;
	size_t icon_lsn;
	uint8_t *paramsfo_buf = NULL;
	uint8_t *icon_buf = NULL;
	char *icontag = NULL;
	iso9660_stat_t *sb;
	struct sfo_ctx sfoctx;
	long int rc;

	// 'z' is a utility function for appending string to outfd
	void z(const char *fmt, ...) {
		va_list ap;
		va_start(ap, fmt);
		vdprintf(outfd, fmt, ap);
		va_end(ap);
	}

	sb = iso9660_ifs_stat_translate(ctx, paramsfo_name);
	if (!sb) return -1;
	paramsfo_bytes = sb->size;
	paramsfo_secsize = sb->secsize;
	paramsfo_lsn = sb->lsn;
	iso9660_stat_free(sb);
	sb = NULL;

	sb = iso9660_ifs_stat_translate(ctx, icon_name);
	if (sb) {
		icon_bytes = sb->size;
		icon_secsize = sb->secsize;
		icon_lsn = sb->lsn;
		iso9660_stat_free(sb);
		sb = NULL;
	} else {
		icon_bytes = 0;
	}

	paramsfo_buf = calloc(ISO_BLOCKSIZE, paramsfo_secsize);
	if (!paramsfo_buf) return -2;

	rc = iso9660_iso_seek_read(ctx, paramsfo_buf, paramsfo_lsn, paramsfo_secsize);
	if (!rc) {
		free(paramsfo_buf);
		return -3;
	}

	if (icon_bytes > 0) {
		icon_buf = calloc(ISO_BLOCKSIZE, icon_secsize);
	}
	if (icon_buf) {
		rc = iso9660_iso_seek_read(ctx, icon_buf, icon_lsn, icon_secsize);
		if (!rc) {
			free(icon_buf);
			icon_buf = NULL;
		}
	}
	if (icon_buf) {
		icontag = make_icontag(icon_buf, icon_bytes);
		free(icon_buf);
		icon_buf = NULL;
	}
	
	rc = sfo_init(&sfoctx, paramsfo_buf);
	if (rc < 0) {
		free(icon_buf);
		free(paramsfo_buf);
		return -4;
	}

	char *title = escape_xml(sfo_get_string_for_key(&sfoctx, "TITLE"), false);
	char *title_nobreaks = escape_xml(sfo_get_string_for_key(&sfoctx, "TITLE"), true);
	z(html_header, 
		PSP2MASTER_GENERATOR_STRING,
		title,
		icontag?:"",
		title_nobreaks
	);
	free(title);
	title = NULL;
	free(icontag);
	icontag = NULL;
	free(title_nobreaks);
	title_nobreaks = NULL;

	// print param table
	for (int i = 0; i < sfo_numrows(&sfoctx); i++) {
		struct sfo_row row = sfo_get_row_by_index(&sfoctx, i);
		z("<tr>\n");

		// key
		z("\t<td class=\"key\">%s", row.key);
		z("</td>\n");

		// value
		z("\t<td class=\"value\">");
		switch (row.type) {
		case KT_UINT32:
				z("%u", *(uint32_t *)row.val);
			break;
		case KT_STRING:
			char *s = escape_xml((char *)row.val, false);
			z("%s", s);
			free(s);
			break;
		case KT_BINARY:
			z("(%d bytes)\n", row.len);
			break;
		default:
			break;
		}
		z("</td>\n");
		z("</tr>\n");
	}
	z("</tbody>\n</table>\n");
	z("<p>\nGenerated by %s.<br />\n", PSP2MASTER_PROGRAM_NAME);
	z("<a href=\"%s\">%s</a></p>\n</body>\n</html>\n", PSP2MASTER_URL, PSP2MASTER_URL);
	sfo_free(&sfoctx);
	free(paramsfo_buf);
	return 0;
}
