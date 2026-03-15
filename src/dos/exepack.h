
/* EXEPACK variables [https://moddingwiki.shikadi.net/wiki/Microsoft_EXEPACK#Games_using_EXEPACK] */
#pragma pack(push,1)
typedef struct EXEPACKVARS {
	uint16_t	real_IP;
	uint16_t	real_CS;
	uint16_t	mem_start;
	uint16_t	exepack_size;
	uint16_t	real_SP;
	uint16_t	real_SS;
	uint16_t	dest_len;
	uint16_t	skip_len;
	uint16_t	signature; /* "RB" */
};
static_assert( sizeof(EXEPACKVARS) == 0x12, "oops" );
#pragma pack(pop)

