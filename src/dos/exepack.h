
/* EXEPACK variables [https://moddingwiki.shikadi.net/wiki/Microsoft_EXEPACK#Games_using_EXEPACK] */
#pragma pack(push,1)
typedef struct EXEPACKVARSv1 {
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
static_assert( sizeof(EXEPACKVARSv1) == 0x12, "oops" );
#pragma pack(pop)

/* older version of EXEPACK (Microsoft Flight Simulator 4) */
#pragma pack(push,1)
typedef struct EXEPACKVARSv2 {
	uint16_t	real_IP;
	uint16_t	real_CS;
	uint16_t	mem_start;
	uint16_t	exepack_size;
	uint16_t	real_SP;
	uint16_t	real_SS;
	uint16_t	dest_len;
	uint16_t	signature; /* "RB" */
};
static_assert( sizeof(EXEPACKVARSv2) == 0x10, "oops" );

/* another variant seen in "PGA Golf" */
typedef EXEPACKVARSv2 EXEPACKVARSv3;

/* another variant seen in "Popcorn" */
typedef EXEPACKVARSv2 EXEPACKVARSv4;
#pragma pack(pop)

