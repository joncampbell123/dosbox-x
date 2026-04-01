
#include <stdint.h>

#define MAX_PATTERN			2
#define MAX_PATPARAM			1
#define MAX_SPARAM			1

struct opcode_param_t {
	const uint8_t			p;
	const uint8_t			s;
};

struct opcode_t {
	const char*			opcode_name;
	const uint8_t			pattern_sz;
	const uint8_t			pattern[MAX_PATTERN];
	const uint8_t			patparam[MAX_PATPARAM];
	const uint8_t			flags;
	const struct opcode_param_t	p_dst;
	const struct opcode_param_t	p_src[MAX_SPARAM];
};

enum {
	CPUFL_PREFIX_SEGMENT=1 << 0
};

enum {
	CPUPPM_NONE=0,
	CPUPPM_MODREGRM=1,
	CPUPPM_IMMEDIATEB=2,
	CPUPPM_IMMEDIATEW=3,
	CPUPPM_IMMEDIATED=4,
	CPUPPM_IMMEDIATENW=5 // depends on CPU mode, 16-bit = word, 32-bit = dword
};

enum {
	CPUPS_BYTE=1, // 8-bit
	CPUPS_WORD=2, // 16-bit
	CPUPS_DWORD=3, // 32-bit
	CPUPS_QWORD=4, // 64-bit

	CPUPS_NATIVEWORD=0x40 // depends on CPU mode, 16-bit = word, 32-bit = dword
};

enum {
	//////////////////////////////////
	CPUP_NONE=0x00,
	CPUP_ZERO=0x01,
	CPUP_ONE=0x02,
	CPUP_IMMEDIATE=0x03,
	CPUP_IMMEDIATE_SX=0x04, // sign extend

	////////////////////////////
	CPUP_MEM_RM=0x08,

	CPUP_MEM_IMM_MIN=0x0C,
	CPUP_MEM_IMM=0x0C, // ex: mov al,[immediate]
	CPUP_MEM_IMM_MAX=0x0F,

	/////////////////////////////////////////////
	CPUP_GREG_MIN=0x10,

	CPUP_GREG_A=0x10,
	CPUP_GREG_C=0x11,
	CPUP_GREG_D=0x12,
	CPUP_GREG_B=0x13,

	CPUP_GREG_EAX=0x10,
	CPUP_GREG_ECX=0x11,
	CPUP_GREG_EDX=0x12,
	CPUP_GREG_EBX=0x13,
	CPUP_GREG_ESP=0x14,
	CPUP_GREG_EBP=0x15,
	CPUP_GREG_ESI=0x16,
	CPUP_GREG_EDI=0x17,

	CPUP_GREG_AX=0x10,
	CPUP_GREG_CX=0x11,
	CPUP_GREG_DX=0x12,
	CPUP_GREG_BX=0x13,
	CPUP_GREG_SP=0x14,
	CPUP_GREG_BP=0x15,
	CPUP_GREG_SI=0x16,
	CPUP_GREG_DI=0x17,

	CPUP_GREG_AL=0x10,
	CPUP_GREG_CL=0x11,
	CPUP_GREG_DL=0x12,
	CPUP_GREG_BL=0x13,
	CPUP_GREG_AH=0x14,
	CPUP_GREG_CH=0x15,
	CPUP_GREG_DH=0x16,
	CPUP_GREG_BH=0x17,

	CPUP_GREG_DXAX=0x1D,
	CPUP_GREG_REG=0x1E,
	CPUP_GREG_RM=0x1F,

	CPUP_GREG_MAX=0x1F,

	////////////////////////////////////////////////////
	CPUP_SREG_MIN=0x20,

	CPUP_SREG_ES=0x20,
	CPUP_SREG_CS=0x21,
	CPUP_SREG_SS=0x22,
	CPUP_SREG_DS=0x23,
	CPUP_SREG_FS=0x24,
	CPUP_SREG_GS=0x25,

	CPUP_SREG_REG=0x2E,
	CPUP_SREG_RM=0x2F,

	CPUP_SREG_MAX=0x2F
};

extern const struct opcode_t* oplist_gen_8086[];

