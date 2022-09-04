#pragma once

#ifdef __MINGW32__
// On Windows we pretend that the symbols here don't have a leading underscore,
// even though they do. The linker will add them for some reason.
extern size_t binary_data_cont_l0_img_size;
extern const void *binary_data_cont_l0_img_start;
extern size_t binary_data_mdi_img_size;
extern const void *binary_data_mdi_img_start;
extern size_t binary_data_umd_auth_dat_size;
extern const void *binary_data_umd_auth_dat_start;
#define cont_l0_img		&binary_data_cont_l0_img_start
#define cont_l0_img_size	((size_t) &binary_data_cont_l0_img_size)
#define mdi_img			&binary_data_mdi_img_start
#define mdi_img_size		((size_t) &binary_data_mdi_img_size)
#define umd_auth_dat		&binary_data_umd_auth_dat_start
#define umd_auth_dat_size	((size_t) &binary_data_umd_auth_dat_size)

#else

extern size_t _binary_data_cont_l0_img_size;
extern const void *_binary_data_cont_l0_img_start;
extern size_t _binary_data_mdi_img_size;
extern const void *_binary_data_mdi_img_start;
extern size_t _binary_data_umd_auth_dat_size;
extern const void *_binary_data_umd_auth_dat_start;
#define cont_l0_img		&_binary_data_cont_l0_img_start
#define cont_l0_img_size	((size_t) &_binary_data_cont_l0_img_size)
#define mdi_img			&_binary_data_mdi_img_start
#define mdi_img_size		((size_t) &_binary_data_mdi_img_size)
#define umd_auth_dat		&_binary_data_umd_auth_dat_start
#define umd_auth_dat_size	((size_t) &_binary_data_umd_auth_dat_size)

#endif


struct psp_appdata_s {
	char game_id[10];
	char pipe1;
	char hex_idk[16];
	char pipe2;
	char number_idk[4];
} __attribute__((packed));

enum disc_structure_e {
	DS_SINGLE = 0x01,
	DS_DUAL = 0x31,
	DS_IDK = 0x21,
};

enum layer_structure_e {
	LS_SINGLE = 1,
	LS_DUAL_PTP = 2,
	LS_DUAL_OTP = 3,
};

struct cont_s {
	uint8_t book_type;	// either 80h or 81h
	uint8_t idk1;		// never checked?
	uint8_t disc_structure;		// one of DS_SINGLE or DS_DUAL
	uint8_t recorded_density;	// always E0h
	uint32_t idk4;
	uint32_t l0size;
	uint32_t l1size;
	uint8_t pad[0x1000-4-4-4-4];
	struct {
		struct psp_appdata_s appdata;
		uint8_t idk[0x800-sizeof(struct psp_appdata_s)];
	} chunks[14];
} __attribute__((packed));

struct mdi_s {
	struct psp_appdata_s appdata;
	char producer[32];
	char copyright[32];
	char date[8];	// ASCII digits, YYYYMMDD, not null-terminated
	char id[24];	// "PSP(R) Master Disc      " not null-terminated
	uint8_t ps_type;// always 50h
	uint8_t region;	// always 3Fh
	uint8_t idk82;
	uint8_t media_type;	// always 80h
	uint8_t idk84;	// always FFh
	uint8_t layer_structure;	// one of LS_SINGLE, LS_DUAL_PTP, LS_DUAL_OTP
	uint8_t idk86;
	uint8_t idk87;
	uint32_t l0size;
	uint32_t l1size;
	// offset 0x100
	char generator_id[8];	// "UMDGEN  "
	char generator_ver[8];	// "1.2.7   "
} __attribute__((packed));

struct umd_auth_s {
	char flags[6];
	uint8_t pad[0x40 - 6];
	uint32_t boot_lsn;
	uint32_t boot_bytes;
	uint32_t eboot_lsn;
	uint32_t eboot_bytes;
} __attribute__((packed));
