#define _GNU_SOURCE
#ifdef __MINGW32__
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#endif
#include <inttypes.h>
#include <iso646.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#include <cdio/iso9660.h>

#include "data.h"
#include "endian.h"
#include "err.h"
#include "hexdump.h"
#include "makehtml.h"
#include "mapfile.h"
#include "prxdecrypt.h"

#define ARRAY_SIZE(x) (sizeof(x)/sizeof(*x))

const char bootname[] = "PSP_GAME/SYSDIR/BOOT.BIN";
const char ebootname[] = "PSP_GAME/SYSDIR/EBOOT.BIN";
const size_t layersize = 906362880ULL;

#define PATHSEP "\\"

struct {
	size_t isosize;
	size_t boot_bytes;
	size_t boot_secsize;
	size_t boot_lsn;
	size_t eboot_bytes;
	size_t eboot_secsize;
	size_t eboot_lsn;
	struct psp_appdata_s appdata;
	int layers;
} gameinfo;

#ifdef __MINGW32__
char *__progname;
#else
extern char *__progname;
#endif

static void noreturn usage(void);

bool iso9660_ifs_read_psp_appdata(iso9660_t *p_iso, struct psp_appdata_s *appdata)
{
	bool rc;
	iso9660_pvd_t pvd;

	rc = iso9660_ifs_read_pvd(p_iso, &pvd);
	if (!rc) return rc;

	memcpy(appdata, pvd.application_data, sizeof(*appdata));
	if ((appdata->pipe1 != '|') || (appdata->pipe2 != '|'))
		return false;
	return true;
}

int main(int argc, char *argv[])
{
	char *infilename = NULL;
	char *outdir = NULL;
	int rc;
	int outdir_fd = -1;
	char pathbuf[BUFSIZ];

#ifdef __MINGW32__
	char *temp;
	__progname = strrchr(argv[0], '\\');
	if (!__progname) __progname = argv[0];
	else __progname++;
	temp = strrchr(argv[0], '.');
	if (temp) *temp = '\0';
#endif

	while ((rc = getopt(argc, argv, "i:d:")) != -1)
		switch (rc) {
		case 'i':
			if (infilename)
				usage();
			infilename = optarg;
			break;
		case 'd':
			if (outdir)
				usage();
			outdir = optarg;
			break;
		default:
			usage();
		}
	argc -= optind;
	argv += optind;
	if (!infilename)
		usage();
	if (*argv != NULL)
		usage();

	struct stat sb;
	rc = stat(infilename, &sb);
	if (rc == -1) err(1, "couldn't find '%s'", infilename);

	gameinfo.isosize = sb.st_size;
	if (gameinfo.isosize > layersize) {
		gameinfo.layers = 2;
	} else {
		gameinfo.layers = 1;
	}
	
	if (outdir) {
		rc = stat(outdir, &sb);
		if (!S_ISDIR(sb.st_mode)) {
			errx(1, "output directory '%s' was specified, but it is not a directory", outdir);
		}
	} else {
		outdir = ".";
	}

	int infd = open(infilename, O_RDONLY|O_BINARY);
	if (infd == -1) err(1, "couldn't open '%s' for reading", infilename);

	iso9660_t *ctx = iso9660_open(infilename);
	if (!ctx) errx(1, "couldn't open '%s' as iso image", infilename);

	char *app_id = NULL;
	rc = iso9660_ifs_get_application_id(ctx, &app_id);
	if (!rc) errx(1, "couldn't get iso image's application id");
	if (strcmp(app_id, "PSP GAME")) {
		errx(1, "not a PSP game: '%s'", infilename);
	}
	free(app_id);
	
	rc = iso9660_ifs_read_psp_appdata(ctx, &gameinfo.appdata);
	if (!rc) errx(1, "couldn't get game id for '%s'", infilename);

	iso9660_stat_t *s;
	s = iso9660_ifs_stat_translate(ctx, ebootname);
	if (!s) errx(1, "couldn't stat file in image: '%s'", ebootname);
	gameinfo.eboot_bytes = s->size;
	gameinfo.eboot_secsize = s->secsize;
	gameinfo.eboot_lsn = s->lsn;
	iso9660_stat_free(s);
	s = iso9660_ifs_stat_translate(ctx, bootname);
	if (!s) errx(1, "couldn't stat file '%s' in image", bootname);
	gameinfo.boot_bytes = s->size;
	gameinfo.boot_secsize = s->secsize;
	gameinfo.boot_lsn = s->lsn;
	iso9660_stat_free(s);
	s = NULL;

	uint8_t *bootbuf;
	bootbuf = calloc(ISO_BLOCKSIZE, gameinfo.boot_secsize + 512); // yes really. the psp decrypt shit needs a slightly bigger buffer. no idea why. seems to spew trash at the end. idgi
	if (!bootbuf) err(1, "in calloc");

	uint8_t *ebootbuf;
	ebootbuf = calloc(ISO_BLOCKSIZE, gameinfo.eboot_secsize);
	if (!ebootbuf) err(1, "in calloc");

	long int z;
	z = iso9660_iso_seek_read(ctx, ebootbuf, gameinfo.eboot_lsn, gameinfo.eboot_secsize);
	if (!z) err(1, "couldn't read file from image: '%s'", ebootname);

	z = pspDecryptPRX(ebootbuf, bootbuf, ISO_BLOCKSIZE * gameinfo.eboot_secsize, NULL, false);
	if (z < 0) errx(1, "couldn't decrypt eboot");

	snprintf(pathbuf, BUFSIZ, "%s%s%s", outdir, PATHSEP, "GAMEINFO.HTM");
	int htmlfd = open(pathbuf, O_CREAT|O_TRUNC|O_WRONLY|O_BINARY, 0644);
	if (htmlfd == -1) err(1, "couldn't open '%s' for writing", "GAMEINFO.HTM");
	rc = make_html(ctx, htmlfd);
	close(htmlfd);
	if (rc) {
		unlink(pathbuf);
	}

	iso9660_close(ctx);
	ctx = NULL;

	snprintf(pathbuf, BUFSIZ, "%s%s%s", outdir, PATHSEP, "USER_L0.IMG");
	int outfd = open(pathbuf, O_CREAT|O_TRUNC|O_WRONLY|O_BINARY, 0644);
	if (outfd == -1) err(1, "couldn't open '%s' for writing", "USER_L0.IMG");

	uint8_t buf[BUFSIZ];
	size_t bytes;
	int layer2_fd;
	lseek(infd, 0, SEEK_SET);
	lseek(outfd, 0, SEEK_SET);
	switch (gameinfo.layers) {
	case 1:
		bytes = gameinfo.isosize;
		while (bytes) {
			ssize_t readsz, writesz;
			readsz = read(infd, buf, BUFSIZ);
			if (readsz == -1) err(1, "couldn't read from '%s'", infilename);
			writesz = write(outfd, buf, readsz);
			if (writesz == -1) err(1, "couldn't write to '%s'", "USER_L0.IMG");
			if (readsz != writesz) errx(1, "wtf");
			bytes -= readsz;
		}
		break;
	case 2:
		bytes = layersize;
		while (bytes) {
			ssize_t readsz, writesz;
			readsz = read(infd, buf, BUFSIZ);
			if (readsz == -1) err(1, "couldn't read from '%s'", infilename);
			writesz = write(outfd, buf, readsz);
			if (writesz == -1) err(1, "couldn't write to '%s'", "USER_L0.IMG");
			if (readsz != writesz) errx(1, "wtf");
			bytes -= readsz;
		}
		snprintf(pathbuf, BUFSIZ, "%s%s%s", outdir, PATHSEP, "USER_L1.IMG");
		layer2_fd = open(pathbuf, O_CREAT|O_TRUNC|O_WRONLY|O_BINARY, 0644);
		if (layer2_fd == -1) err(1, "couldn't open '%s' for writing", "USER_L1.IMG");
		bytes = gameinfo.isosize - layersize;
		while (bytes) {
			ssize_t readsz, writesz;
			readsz = read(infd, buf, BUFSIZ);
			if (readsz == -1) err(1, "couldn't read from '%s'", infilename);
			writesz = write(layer2_fd, buf, readsz);
			if (writesz == -1) err(1, "couldn't write to '%s'", "USER_L1.IMG");
			if (readsz != writesz) errx(1, "wtf");
			bytes -= readsz;
		}
		close(layer2_fd);
		break;
	default:
		errx(1, "weird number of layers?");
	}

	lseek(outfd, ISO_BLOCKSIZE * gameinfo.boot_lsn, SEEK_SET);
	ssize_t sz = write(outfd, bootbuf, gameinfo.boot_bytes);

	close(infd);
	close(outfd);
	infd = outfd = -1;

	snprintf(pathbuf, BUFSIZ, "%s%s%s", outdir, PATHSEP, "CONT_L0.IMG");
	struct MappedFile_s m = MappedFile_Create(pathbuf, cont_l0_img_size);
	if (!m.data) err(1, "couldn't create CONT_L0.IMG");
	memcpy(m.data, cont_l0_img, cont_l0_img_size);

	uint32_t weirdsize1 = 0xAAAAAAAA;
	uint32_t weirdsize2 = 0xAAAAAAAA;
	struct cont_s *c = (struct cont_s *)m.data;
	switch (gameinfo.layers) {
	case 1:
		weirdsize1 = (gameinfo.isosize / 2048) - 1;
		weirdsize2 = 0;
		c->l0size = htobe32(weirdsize1 + 0x30000);
		c->l1size = htobe32(weirdsize2 + 0x30000);
		c->disc_structure = DS_SINGLE;
		break;
	case 2:
		weirdsize1 = (gameinfo.isosize / 2048) - 1;
		weirdsize2 = 0x6c0bf;
		c->l0size = htobe32(weirdsize1 + 0x30000 + 0xef7e80);
		c->l1size = htobe32(weirdsize2 + 0x30000);
		c->disc_structure = DS_DUAL;
		break;
	}
	for (int i = 0 ; i < ARRAY_SIZE(c->chunks); i++) {
		c->chunks[i].appdata = gameinfo.appdata;
	}
	MappedFile_Close(m);
	m.data = NULL;

	snprintf(pathbuf, BUFSIZ, "%s%s%s", outdir, PATHSEP, "MDI.IMG");
	m = MappedFile_Create(pathbuf, mdi_img_size);
	if (!m.data) err(1, "couldn't create MDI.IMG");
	memcpy(m.data, mdi_img, mdi_img_size);
	struct mdi_s *mdi = (struct mdi_s *)m.data;
	mdi->appdata = gameinfo.appdata;
	switch (gameinfo.layers) {
	case 1:
		mdi->layer_structure = LS_SINGLE;
		mdi->l0size = htole32(weirdsize1);
		mdi->l1size = htole32(weirdsize2);
		break;
	case 2:
		mdi->layer_structure = LS_DUAL_OTP;
		mdi->l0size = htole32(weirdsize2);
		mdi->l1size = htole32(weirdsize1);
		break;
	}
	MappedFile_Close(m);
	m.data = NULL;

	snprintf(pathbuf, BUFSIZ, "%s%s%s", outdir, PATHSEP, "UMD_AUTH.DAT");
	m = MappedFile_Create(pathbuf, umd_auth_dat_size);
	if (!m.data) err(1, "couldn't create UMD_AUTH.DAT");
	memcpy(m.data, umd_auth_dat, umd_auth_dat_size);
	struct umd_auth_s *umd_auth = (struct umd_auth_s *)m.data;
	umd_auth->flags[5] = '\0';
	switch (gameinfo.layers) {
	case 1: umd_auth->flags[0] = 'S'; break;
	case 2: umd_auth->flags[0] = 'O'; break;
	}
	umd_auth->boot_lsn = htole32(gameinfo.boot_lsn);
	umd_auth->boot_bytes = htole32(gameinfo.boot_bytes);
	umd_auth->eboot_lsn = htole32(gameinfo.eboot_lsn);
	umd_auth->eboot_bytes = htole32(gameinfo.eboot_bytes);
	MappedFile_Close(m);
	m.data = NULL;

	if (outdir_fd != -1) {
		close(outdir_fd);
		outdir_fd = -1;
	}


	free(bootbuf);
	free(ebootbuf);
	bootbuf = ebootbuf = NULL;

	return EXIT_SUCCESS;
}

static void noreturn usage(void)
{
	(void)fprintf(stderr, "usage: %s -i <pspgame.iso> [-d <dirname>]\n",
		__progname
	);
	exit(EXIT_FAILURE);
}
