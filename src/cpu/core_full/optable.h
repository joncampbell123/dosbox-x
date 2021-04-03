/*
 *  Copyright (C) 2002-2021  The DOSBox Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/* Big ass opcode table normal,double, 66 normal, 66 double */
static OpCode OpCodeTable[1024]={
/* 0x00 - 0x07 */
{L_MODRM	,t_ADDb	,S_Eb	,M_EbGb		},{L_MODRM	,t_ADDw	,S_Ew	,M_EwGw		},
{L_MODRM	,t_ADDb	,S_Gb	,M_GbEb		},{L_MODRM	,t_ADDw	,S_Gw	,M_GwEw		},
{L_REGbIb	,t_ADDb	,S_REGb	,REGI_AL	},{L_REGwIw	,t_ADDw	,S_REGw	,REGI_AX	},
{L_SEG		,0		,S_PUSHw,es			},{D_POPSEGw,0		,0		,es			},
/* 0x08 - 0x0f */
{L_MODRM	,t_ORb	,S_Eb	,M_EbGb		},{L_MODRM	,t_ORw	,S_Ew	,M_EwGw		},
{L_MODRM	,t_ORb	,S_Gb	,M_GbEb		},{L_MODRM	,t_ORw	,S_Gw	,M_GwEw		},
{L_REGbIb	,t_ORb	,S_REGb	,REGI_AL	},{L_REGwIw	,t_ORw	,S_REGw	,REGI_AX	},
{L_SEG		,0		,S_PUSHw,cs			},{L_DOUBLE	,0		,0		,0			},

/* 0x10 - 0x17 */
{L_MODRM	,t_ADCb	,S_Eb	,M_EbGb		},{L_MODRM	,t_ADCw	,S_Ew	,M_EwGw		},
{L_MODRM	,t_ADCb	,S_Gb	,M_GbEb		},{L_MODRM	,t_ADCw	,S_Gw	,M_GwEw		},
{L_REGbIb	,t_ADCb	,S_REGb	,REGI_AL	},{L_REGwIw	,t_ADCw	,S_REGw	,REGI_AX	},
{L_SEG		,0		,S_PUSHw,ss			},{D_POPSEGw,0		,0		,ss			},
/* 0x18 - 0x1f */
{L_MODRM	,t_SBBb	,S_Eb	,M_EbGb		},{L_MODRM	,t_SBBw	,S_Ew	,M_EwGw		},
{L_MODRM	,t_SBBb	,S_Gb	,M_GbEb		},{L_MODRM	,t_SBBw	,S_Gw	,M_GwEw		},
{L_REGbIb	,t_SBBb	,S_REGb	,REGI_AL	},{L_REGwIw	,t_SBBw	,S_REGw	,REGI_AX	},
{L_SEG		,0		,S_PUSHw,ds			},{D_POPSEGw,0		,0		,ds			},

/* 0x20 - 0x27 */
{L_MODRM	,t_ANDb	,S_Eb	,M_EbGb		},{L_MODRM	,t_ANDw	,S_Ew	,M_EwGw		},
{L_MODRM	,t_ANDb	,S_Gb	,M_GbEb		},{L_MODRM	,t_ANDw	,S_Gw	,M_GwEw		},
{L_REGbIb	,t_ANDb	,S_REGb	,REGI_AL	},{L_REGwIw	,t_ANDw	,S_REGw	,REGI_AX	},
{L_PRESEG	,0		,0		,es			},{D_DAA	,0		,0		,0			},
/* 0x28 - 0x2f */
{L_MODRM	,t_SUBb	,S_Eb	,M_EbGb		},{L_MODRM	,t_SUBw	,S_Ew	,M_EwGw		},
{L_MODRM	,t_SUBb	,S_Gb	,M_GbEb		},{L_MODRM	,t_SUBw	,S_Gw	,M_GwEw		},
{L_REGbIb	,t_SUBb	,S_REGb	,REGI_AL	},{L_REGwIw	,t_SUBw	,S_REGw	,REGI_AX	},
{L_PRESEG	,0		,0		,cs			},{D_DAS	,0		,0		,0			},

/* 0x30 - 0x37 */
{L_MODRM	,t_XORb	,S_Eb	,M_EbGb		},{L_MODRM	,t_XORw	,S_Ew	,M_EwGw		},
{L_MODRM	,t_XORb	,S_Gb	,M_GbEb		},{L_MODRM	,t_XORw	,S_Gw	,M_GwEw		},
{L_REGbIb	,t_XORb	,S_REGb	,REGI_AL	},{L_REGwIw	,t_XORw	,S_REGw	,REGI_AX	},
{L_PRESEG	,0		,0		,ss			},{D_AAA	,0		,0		,0			},
/* 0x38 - 0x3f */
{L_MODRM	,t_CMPb	,0		,M_EbGb		},{L_MODRM	,t_CMPw	,0		,M_EwGw		},
{L_MODRM	,t_CMPb	,0		,M_GbEb		},{L_MODRM	,t_CMPw	,0		,M_GwEw		},
{L_REGbIb	,t_CMPb	,0		,REGI_AL	},{L_REGwIw	,t_CMPw	,0		,REGI_AX	},
{L_PRESEG	,0		,0		,ds			},{D_AAS	,0		,0		,0			},

/* 0x40 - 0x47 */
{L_REGw		,t_INCw	,S_REGw	,REGI_AX},{L_REGw	,t_INCw	,S_REGw	,REGI_CX},
{L_REGw		,t_INCw	,S_REGw	,REGI_DX},{L_REGw	,t_INCw	,S_REGw	,REGI_BX},
{L_REGw		,t_INCw	,S_REGw	,REGI_SP},{L_REGw	,t_INCw	,S_REGw	,REGI_BP},
{L_REGw		,t_INCw	,S_REGw	,REGI_SI},{L_REGw	,t_INCw	,S_REGw	,REGI_DI},
/* 0x48 - 0x4f */
{L_REGw		,t_DECw	,S_REGw	,REGI_AX},{L_REGw	,t_DECw	,S_REGw	,REGI_CX},
{L_REGw		,t_DECw	,S_REGw	,REGI_DX},{L_REGw	,t_DECw	,S_REGw	,REGI_BX},
{L_REGw		,t_DECw	,S_REGw	,REGI_SP},{L_REGw	,t_DECw	,S_REGw	,REGI_BP},
{L_REGw		,t_DECw	,S_REGw	,REGI_SI},{L_REGw	,t_DECw	,S_REGw	,REGI_DI},

/* 0x50 - 0x57 */
{L_REGw		,0		,S_PUSHw,REGI_AX},{L_REGw	,0		,S_PUSHw,REGI_CX},
{L_REGw		,0		,S_PUSHw,REGI_DX},{L_REGw	,0		,S_PUSHw,REGI_BX},
{L_REGw		,0		,S_PUSHw,REGI_SP},{L_REGw	,0		,S_PUSHw,REGI_BP},
{L_REGw		,0		,S_PUSHw,REGI_SI},{L_REGw	,0		,S_PUSHw,REGI_DI},
/* 0x58 - 0x5f */
{L_POPw		,0		,S_REGw	,REGI_AX},{L_POPw	,0		,S_REGw	,REGI_CX},
{L_POPw		,0		,S_REGw	,REGI_DX},{L_POPw	,0		,S_REGw	,REGI_BX},
{L_POPw		,0		,S_REGw	,REGI_SP},{L_POPw	,0		,S_REGw	,REGI_BP},
{L_POPw		,0		,S_REGw	,REGI_SI},{L_POPw	,0		,S_REGw	,REGI_DI},


/* 0x60 - 0x67 */
{D_PUSHAw	,0			,0		,0		},{D_POPAw	,0			,0		,0		},
{L_MODRM	,O_BOUNDw	,0		,M_Gw	},{L_MODRM_NVM	,O_ARPL		,S_Ew	,M_EwGw	},
{L_PRESEG	,0			,0		,fs		},{L_PRESEG	,0			,0		,gs		},
{L_PREOP	,0			,0		,0		},{L_PREADD	,0			,0		,0		},
/* 0x68 - 0x6f */
{L_Iw		,0			,S_PUSHw,0		},{L_MODRM	,O_IMULRw	,S_Gw	,M_EwxIwx},
{L_Ibx		,0			,S_PUSHw,0		},{L_MODRM	,O_IMULRw	,S_Gw	,M_EwxIbx},
{L_STRING	,R_INSB		,0		,0		},{L_STRING	,R_INSW		,0		,0		},
{L_STRING	,R_OUTSB	,0		,0		},{L_STRING	,R_OUTSW	,0		,0		},


/* 0x70 - 0x77 */
{L_Ibx		,O_C_O		,S_C_AIPw,0		},{L_Ibx	,O_C_NO		,S_C_AIPw,0		},
{L_Ibx		,O_C_B		,S_C_AIPw,0		},{L_Ibx	,O_C_NB		,S_C_AIPw,0		},
{L_Ibx		,O_C_Z		,S_C_AIPw,0		},{L_Ibx	,O_C_NZ		,S_C_AIPw,0		},
{L_Ibx		,O_C_BE		,S_C_AIPw,0		},{L_Ibx	,O_C_NBE	,S_C_AIPw,0		},
/* 0x78 - 0x7f */
{L_Ibx		,O_C_S		,S_C_AIPw,0		},{L_Ibx	,O_C_NS		,S_C_AIPw,0		},
{L_Ibx		,O_C_P		,S_C_AIPw,0		},{L_Ibx	,O_C_NP		,S_C_AIPw,0		},
{L_Ibx		,O_C_L		,S_C_AIPw,0		},{L_Ibx	,O_C_NL		,S_C_AIPw,0		},
{L_Ibx		,O_C_LE		,S_C_AIPw,0		},{L_Ibx	,O_C_NLE	,S_C_AIPw,0		},


/* 0x80 - 0x87 */
{L_MODRM	,0			,0		,M_GRP	},{L_MODRM	,1			,0		,M_GRP	},
{L_MODRM	,0			,0		,M_GRP	},{L_MODRM	,3			,0		,M_GRP	},
{L_MODRM	,t_TESTb	,0		,M_EbGb	},{L_MODRM	,t_TESTw	,0		,M_EwGw	},
{L_MODRM	,0			,S_EbGb	,M_GbEb	},{L_MODRM	,0			,S_EwGw	,M_GwEw	},
/* 0x88 - 0x8f */
{L_MODRM		,0		,S_Eb	,M_Gb	},{L_MODRM	,0			,S_Ew	,M_Gw	},
{L_MODRM		,0		,S_Gb	,M_Eb	},{L_MODRM	,0			,S_Gw	,M_Ew	},
{L_MODRM		,0		,S_Ew	,M_SEG	},{L_MODRM	,0			,S_Gw	,M_EA	},
{L_MODRM		,0		,S_SEGm	,M_Ew	},{L_POPwRM	,0			,S_Ew	,M_None	},

/* 0x90 - 0x97 */
{D_NOP		,0			,0		,0		},{L_REGw	,O_XCHG_AX	,S_REGw	,REGI_CX},
{L_REGw		,O_XCHG_AX	,S_REGw	,REGI_DX},{L_REGw	,O_XCHG_AX	,S_REGw	,REGI_BX},
{L_REGw		,O_XCHG_AX	,S_REGw	,REGI_SP},{L_REGw	,O_XCHG_AX	,S_REGw	,REGI_BP},
{L_REGw		,O_XCHG_AX	,S_REGw	,REGI_SI},{L_REGw	,O_XCHG_AX	,S_REGw	,REGI_DI},
/* 0x98 - 0x9f */
{D_CBW		,0			,0		,0		},{D_CWD	,0			,0		,0		},
{L_Ifw		,O_CALLFw	,0		,0		},{D_WAIT	,0			,0		,0		},
{D_PUSHF	,0			,0		,0		},{D_POPF	,0			,0		,0		},
{D_SAHF		,0			,0		,0		},{D_LAHF	,0			,0		,0		},


/* 0xa0 - 0xa7 */
{L_OP		,O_ALOP		,0		,0		},{L_OP		,O_AXOP		,0		,0		},
{L_OP		,O_OPAL		,0		,0		},{L_OP		,O_OPAX		,0		,0		},
{L_STRING	,R_MOVSB	,0		,0		},{L_STRING	,R_MOVSW	,0		,0		},
{L_STRING	,R_CMPSB	,0		,0		},{L_STRING	,R_CMPSW	,0		,0		},
/* 0xa8 - 0xaf */
{L_REGbIb	,t_TESTb	,0		,REGI_AL},{L_REGwIw	,t_TESTw	,0		,REGI_AX},
{L_STRING	,R_STOSB	,0		,0		},{L_STRING	,R_STOSW	,0		,0		},
{L_STRING	,R_LODSB	,0		,0		},{L_STRING	,R_LODSW	,0		,0		},
{L_STRING	,R_SCASB	,0		,0		},{L_STRING	,R_SCASW	,0		,0		},

/* 0xb0	- 0xb7 */
{L_Ib		,0			,S_REGb	,REGI_AL},{L_Ib	,0			,S_REGb	,REGI_CL},
{L_Ib		,0			,S_REGb	,REGI_DL},{L_Ib	,0			,S_REGb	,REGI_BL},
{L_Ib		,0			,S_REGb	,REGI_AH},{L_Ib	,0			,S_REGb	,REGI_CH},
{L_Ib		,0			,S_REGb	,REGI_DH},{L_Ib	,0			,S_REGb	,REGI_BH},
/* 0xb8 - 0xbf */
{L_Iw		,0			,S_REGw	,REGI_AX},{L_Iw	,0			,S_REGw	,REGI_CX},
{L_Iw		,0			,S_REGw	,REGI_DX},{L_Iw	,0			,S_REGw	,REGI_BX},
{L_Iw		,0			,S_REGw	,REGI_SP},{L_Iw	,0			,S_REGw	,REGI_BP},
{L_Iw		,0			,S_REGw	,REGI_SI},{L_Iw	,0			,S_REGw	,REGI_DI},

/* 0xc0 - 0xc7 */
{L_MODRM	,5			,0	,M_SHIFT_Ib	},{L_MODRM	,6			,0	,M_SHIFT_Ib	},
{L_POPw		,0			,S_IPIw	,0		},{L_POPw	,0			,S_IP	,0		},
{L_MODRM	,O_SEGES	,S_SEGGw,M_Efw	},{L_MODRM	,O_SEGDS	,S_SEGGw,M_Efw	},
{L_MODRM	,0			,S_Eb	,M_Ib	},{L_MODRM	,0			,S_Ew	,M_Iw	},
/* 0xc8 - 0xcf */
{D_ENTERw	,0			,0		,0		},{D_LEAVEw	,0			,0		,0		},
{D_RETFwIw	,0			,0		,0		},{D_RETFw	,0			,0		,0		},
{L_VAL		,O_INT		,0		,3		},{L_Ib		,O_INT		,0		,0		},
{L_INTO		,O_INT		,0		,0		},{D_IRETw	,0			,0		,0		},

/* 0xd0 - 0xd7 */
{L_MODRM	,5			,0	,M_SHIFT_1	},{L_MODRM	,6			,0	,M_SHIFT_1	},
{L_MODRM	,5			,0	,M_SHIFT_CL	},{L_MODRM	,6			,0	,M_SHIFT_CL	},
{L_Ib		,O_AAM		,0		,0		},{L_Ib		,O_AAD		,0		,0		},
{D_SETALC	,0			,0		,0		},{D_XLAT	,0			,0		,0		},
//TODO FPU
/* 0xd8 - 0xdf */
{L_MODRM	,O_FPU		,0		,0		},{L_MODRM	,O_FPU		,1		,0		},
{L_MODRM	,O_FPU		,2		,0		},{L_MODRM	,O_FPU		,3		,0		},
{L_MODRM	,O_FPU		,4		,0		},{L_MODRM	,O_FPU		,5		,0		},
{L_MODRM	,O_FPU		,6		,0		},{L_MODRM	,O_FPU		,7		,0		},

/* 0xe0 - 0xe7 */
{L_Ibx		,O_LOOPNZ	,S_AIPw	,0		},{L_Ibx	,O_LOOPZ	,S_AIPw	,0		},
{L_Ibx		,O_LOOP		,S_AIPw	,0		},{L_Ibx	,O_JCXZ		,S_AIPw	,0		},
{L_Ib		,O_INb		,0		,0		},{L_Ib		,O_INw		,0		,0		},
{L_Ib		,O_OUTb		,0		,0		},{L_Ib		,O_OUTw		,0		,0		},
/* 0xe8 - 0xef */
{L_Iw		,O_CALLNw	,S_AIPw	,0		},{L_Iwx	,0			,S_AIPw	,0		},
{L_Ifw		,O_JMPFw	,0		,0		},{L_Ibx	,0			,S_AIPw	,0		},
{L_REGw		,O_INb		,0		,REGI_DX},{L_REGw	,O_INw		,0		,REGI_DX},
{L_REGw		,O_OUTb		,0		,REGI_DX},{L_REGw	,O_OUTw		,0		,REGI_DX},

/* 0xf0 - 0xf7 */
{D_LOCK		,0			,0		,0		},{D_ICEBP	,0			,0		,0		},
{L_PREREPNE	,0			,0		,0		},{L_PREREP	,0			,0		,0		},
{D_HLT		,0			,0		,0		},{D_CMC	,0			,0		,0		},
{L_MODRM	,8			,0		,M_GRP	},{L_MODRM	,9			,0		,M_GRP	},
/* 0xf8 - 0xff */
{D_CLC		,0			,0		,0		},{D_STC	,0			,0		,0		},
{D_CLI		,0			,0		,0		},{D_STI	,0			,0		,0		},
{D_CLD		,0			,0		,0		},{D_STD	,0			,0		,0		},
{L_MODRM	,0xb		,0		,M_GRP	},{L_MODRM	,0xc		,0		,M_GRP	},

/* 0x100 - 0x107 */
{L_MODRM	,O_GRP6w	,S_Ew	,M_Ew	},{L_MODRM	,O_GRP7w	,S_Ew	,M_Ew	},
{L_MODRM_NVM	,O_LAR		,S_Gw	,M_EwGw	},{L_MODRM_NVM	,O_LSL		,S_Gw	,M_EwGw	},
{0			,0			,0		,0		},{0		,0			,0		,0		},
{D_CLTS		,0			,0		,0		},{0		,0			,0		,0		},
/* 0x108 - 0x10f */
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},

/* 0x110 - 0x117 */
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},
/* 0x118 - 0x11f */
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},

/* 0x120 - 0x127 */
{L_MODRM	,O_M_Rd_CRx	,S_Ed	,0		},{L_MODRM	,O_M_Rd_DRx	,S_Ed	,0		},
{L_MODRM	,O_M_CRx_Rd	,0		,M_Ed	},{L_MODRM	,O_M_DRx_Rd	,0		,M_Ed	},
{L_MODRM	,O_M_Rd_TRx	,S_Ed	,0		},{0		,0			,0		,0		},
{L_MODRM	,O_M_TRx_Rd	,0		,M_Ed	},{0		,0			,0		,0		},

/* 0x128 - 0x12f */
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},

/* 0x130 - 0x137 */
{0			,0			,0		,0		},{D_RDTSC	,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},
/* 0x138 - 0x13f */
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},

/* 0x140 - 0x147 */
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},
/* 0x148 - 0x14f */
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},

/* 0x150 - 0x157 */
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},
/* 0x158 - 0x15f */
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},

/* 0x160 - 0x167 */
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},
/* 0x168 - 0x16f */
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},


/* 0x170 - 0x177 */
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},
/* 0x178 - 0x17f */
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},

/* 0x180 - 0x187 */
{L_Iwx		,O_C_O		,S_C_AIPw,0		},{L_Iwx	,O_C_NO		,S_C_AIPw,0		},
{L_Iwx		,O_C_B		,S_C_AIPw,0		},{L_Iwx	,O_C_NB		,S_C_AIPw,0		},
{L_Iwx		,O_C_Z		,S_C_AIPw,0		},{L_Iwx	,O_C_NZ		,S_C_AIPw,0		},
{L_Iwx		,O_C_BE		,S_C_AIPw,0		},{L_Iwx	,O_C_NBE	,S_C_AIPw,0		},
/* 0x188 - 0x18f */
{L_Iwx		,O_C_S		,S_C_AIPw,0		},{L_Iwx	,O_C_NS		,S_C_AIPw,0		},
{L_Iwx		,O_C_P		,S_C_AIPw,0		},{L_Iwx	,O_C_NP		,S_C_AIPw,0		},
{L_Iwx		,O_C_L		,S_C_AIPw,0		},{L_Iwx	,O_C_NL		,S_C_AIPw,0		},
{L_Iwx		,O_C_LE		,S_C_AIPw,0		},{L_Iwx	,O_C_NLE	,S_C_AIPw,0		},

/* 0x190 - 0x197 */
{L_MODRM	,O_C_O		,S_C_Eb,0		},{L_MODRM	,O_C_NO		,S_C_Eb,0		},
{L_MODRM	,O_C_B		,S_C_Eb,0		},{L_MODRM	,O_C_NB		,S_C_Eb,0		},
{L_MODRM	,O_C_Z		,S_C_Eb,0		},{L_MODRM	,O_C_NZ		,S_C_Eb,0		},
{L_MODRM	,O_C_BE		,S_C_Eb,0		},{L_MODRM	,O_C_NBE	,S_C_Eb,0		},
/* 0x198 - 0x19f */
{L_MODRM	,O_C_S		,S_C_Eb,0		},{L_MODRM	,O_C_NS		,S_C_Eb,0		},
{L_MODRM	,O_C_P		,S_C_Eb,0		},{L_MODRM	,O_C_NP		,S_C_Eb,0		},
{L_MODRM	,O_C_L		,S_C_Eb,0		},{L_MODRM	,O_C_NL		,S_C_Eb,0		},
{L_MODRM	,O_C_LE		,S_C_Eb,0		},{L_MODRM	,O_C_NLE	,S_C_Eb,0		},

/* 0x1a0 - 0x1a7 */
{L_SEG		,0		,S_PUSHw	,fs		},{D_POPSEGw,0		,0		,fs			},
{D_CPUID	,0			,0		,0		},{L_MODRM	,O_BTw		,S_Ew	,M_EwGwt	},
{L_MODRM	,O_DSHLw	,S_Ew,M_EwGwIb	},{L_MODRM	,O_DSHLw	,S_Ew	,M_EwGwCL	},
{0			,0			,0		,0		},{0		,0			,0		,0		},
/* 0x1a8 - 0x1af */
{L_SEG		,0		,S_PUSHw	,gs		},{D_POPSEGw,0		,0		,gs			},
{0			,0			,0		,0		},{L_MODRM	,O_BTSw		,S_Ew	,M_EwGwt	},
{L_MODRM	,O_DSHRw	,S_Ew,M_EwGwIb	},{L_MODRM	,O_DSHRw	,S_Ew	,M_EwGwCL	},
{0			,0			,0		,0		},{L_MODRM	,O_IMULRw	,S_Gw	,M_EwxGwx	},

/* 0x1b0 - 0x1b7 */
{0			,0			,0		,0		},{0		,0			,0		,0		},
{L_MODRM	,O_SEGSS	,S_SEGGw,M_Efw	},{L_MODRM	,O_BTRw		,S_Ew	,M_EwGwt	},
{L_MODRM	,O_SEGFS	,S_SEGGw,M_Efw	},{L_MODRM	,O_SEGGS	,S_SEGGw,M_Efw	},
{L_MODRM	,0			,S_Gw	,M_Eb	},{L_MODRM	,0			,S_Gw	,M_Ew	},
/* 0x1b8 - 0x1bf */
{0			,0			,0		,0		},{0		,0			,0		,0		},
{L_MODRM	,0xe		,0		,M_GRP	},{L_MODRM	,O_BTCw		,S_Ew	,M_EwGwt	},
{L_MODRM	,O_BSFw		,S_Gw	,M_Ew	},{L_MODRM	,O_BSRw		,S_Gw	,M_Ew	},
{L_MODRM	,0			,S_Gw	,M_Ebx	},{L_MODRM	,0			,S_Gw	,M_Ewx	},

/* 0x1c0 - 0x1cc */
{L_MODRM		,t_ADDb			,S_EbGb		,M_GbEb		},{L_MODRM		,t_ADDw		,S_EwGw		,M_GwEw	},
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},
/* 0x1c8 - 0x1cf */
{L_REGw		,O_BSWAPw	,S_REGw	,REGI_AX},{L_REGw	,O_BSWAPw	,S_REGw	,REGI_CX},
{L_REGw		,O_BSWAPw	,S_REGw	,REGI_DX},{L_REGw	,O_BSWAPw	,S_REGw	,REGI_BX},
{L_REGw		,O_BSWAPw	,S_REGw	,REGI_SP},{L_REGw	,O_BSWAPw	,S_REGw	,REGI_BP},
{L_REGw		,O_BSWAPw	,S_REGw	,REGI_SI},{L_REGw	,O_BSWAPw	,S_REGw	,REGI_DI},

/* 0x1d0 - 0x1d7 */
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},
/* 0x1d8 - 0x1df */
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},

/* 0x1e0 - 0x1ee */
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},
/* 0x1e8 - 0x1ef */
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},

/* 0x1f0 - 0x1fc */
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},
/* 0x1f8 - 0x1ff */
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},


/* 0x200 - 0x207 */
{L_MODRM	,t_ADDb	,S_Eb	,M_EbGb		},{L_MODRM	,t_ADDd	,S_Ed	,M_EdGd		},
{L_MODRM	,t_ADDb	,S_Gb	,M_GbEb		},{L_MODRM	,t_ADDd	,S_Gd	,M_GdEd		},
{L_REGbIb	,t_ADDb	,S_REGb	,REGI_AL	},{L_REGdId	,t_ADDd	,S_REGd	,REGI_AX	},
{L_SEG		,0		,S_PUSHd,es			},{D_POPSEGd,0		,0		,es			},
/* 0x208 - 0x20f */
{L_MODRM	,t_ORb	,S_Eb	,M_EbGb		},{L_MODRM	,t_ORd	,S_Ed	,M_EdGd		},
{L_MODRM	,t_ORb	,S_Gb	,M_GbEb		},{L_MODRM	,t_ORd	,S_Gd	,M_GdEd		},
{L_REGbIb	,t_ORb	,S_REGb	,REGI_AL	},{L_REGdId	,t_ORd	,S_REGd	,REGI_AX	},
{L_SEG		,0		,S_PUSHd,cs			},{L_DOUBLE	,0		,0		,0			},

/* 0x210 - 0x217 */
{L_MODRM	,t_ADCb	,S_Eb	,M_EbGb		},{L_MODRM	,t_ADCd	,S_Ed	,M_EdGd		},
{L_MODRM	,t_ADCb	,S_Gb	,M_GbEb		},{L_MODRM	,t_ADCd	,S_Gd	,M_GdEd		},
{L_REGbIb	,t_ADCb	,S_REGb	,REGI_AL	},{L_REGdId	,t_ADCd	,S_REGd	,REGI_AX	},
{L_SEG		,0		,S_PUSHd,ss			},{D_POPSEGd,0		,0		,ss			},
/* 0x218 - 0x21f */
{L_MODRM	,t_SBBb	,S_Eb	,M_EbGb		},{L_MODRM	,t_SBBd	,S_Ed	,M_EdGd		},
{L_MODRM	,t_SBBb	,S_Gb	,M_GbEb		},{L_MODRM	,t_SBBd	,S_Gd	,M_GdEd		},
{L_REGbIb	,t_SBBb	,S_REGb	,REGI_AL	},{L_REGdId	,t_SBBd	,S_REGd	,REGI_AX	},
{L_SEG		,0		,S_PUSHd,ds			},{D_POPSEGd,0		,0		,ds			},

/* 0x220 - 0x227 */
{L_MODRM	,t_ANDb	,S_Eb	,M_EbGb		},{L_MODRM	,t_ANDd	,S_Ed	,M_EdGd		},
{L_MODRM	,t_ANDb	,S_Gb	,M_GbEb		},{L_MODRM	,t_ANDd	,S_Gd	,M_GdEd		},
{L_REGbIb	,t_ANDb	,S_REGb	,REGI_AL	},{L_REGdId	,t_ANDd	,S_REGd	,REGI_AX	},
{L_PRESEG	,0		,0		,es			},{D_DAA	,0		,0		,0			},
/* 0x228 - 0x22f */
{L_MODRM	,t_SUBb	,S_Eb	,M_EbGb		},{L_MODRM	,t_SUBd	,S_Ed	,M_EdGd		},
{L_MODRM	,t_SUBb	,S_Gb	,M_GbEb		},{L_MODRM	,t_SUBd	,S_Gd	,M_GdEd		},
{L_REGbIb	,t_SUBb	,S_REGb	,REGI_AL	},{L_REGdId	,t_SUBd	,S_REGd	,REGI_AX	},
{L_PRESEG	,0		,0		,cs			},{D_DAS	,0		,0		,0			},

/* 0x230 - 0x237 */
{L_MODRM	,t_XORb	,S_Eb	,M_EbGb		},{L_MODRM	,t_XORd	,S_Ed	,M_EdGd		},
{L_MODRM	,t_XORb	,S_Gb	,M_GbEb		},{L_MODRM	,t_XORd	,S_Gd	,M_GdEd		},
{L_REGbIb	,t_XORb	,S_REGb	,REGI_AL	},{L_REGdId	,t_XORd	,S_REGd	,REGI_AX	},
{L_PRESEG	,0		,0		,ss			},{D_AAA	,0		,0		,0			},
/* 0x238 - 0x23f */
{L_MODRM	,t_CMPb	,0		,M_EbGb		},{L_MODRM	,t_CMPd	,0		,M_EdGd		},
{L_MODRM	,t_CMPb	,0		,M_GbEb		},{L_MODRM	,t_CMPd	,0		,M_GdEd		},
{L_REGbIb	,t_CMPb	,0		,REGI_AL	},{L_REGdId	,t_CMPd	,0		,REGI_AX	},
{L_PRESEG	,0		,0		,ds			},{D_AAS	,0		,0		,0			},

/* 0x240 - 0x247 */
{L_REGd		,t_INCd	,S_REGd	,REGI_AX},{L_REGd	,t_INCd	,S_REGd	,REGI_CX},
{L_REGd		,t_INCd	,S_REGd	,REGI_DX},{L_REGd	,t_INCd	,S_REGd	,REGI_BX},
{L_REGd		,t_INCd	,S_REGd	,REGI_SP},{L_REGd	,t_INCd	,S_REGd	,REGI_BP},
{L_REGd		,t_INCd	,S_REGd	,REGI_SI},{L_REGd	,t_INCd	,S_REGd	,REGI_DI},
/* 0x248 - 0x24f */
{L_REGd		,t_DECd	,S_REGd	,REGI_AX},{L_REGd	,t_DECd	,S_REGd	,REGI_CX},
{L_REGd		,t_DECd	,S_REGd	,REGI_DX},{L_REGd	,t_DECd	,S_REGd	,REGI_BX},
{L_REGd		,t_DECd	,S_REGd	,REGI_SP},{L_REGd	,t_DECd	,S_REGd	,REGI_BP},
{L_REGd		,t_DECd	,S_REGd	,REGI_SI},{L_REGd	,t_DECd	,S_REGd	,REGI_DI},

/* 0x250 - 0x257 */
{L_REGd		,0		,S_PUSHd,REGI_AX},{L_REGd	,0		,S_PUSHd,REGI_CX},
{L_REGd		,0		,S_PUSHd,REGI_DX},{L_REGd	,0		,S_PUSHd,REGI_BX},
{L_REGd		,0		,S_PUSHd,REGI_SP},{L_REGd	,0		,S_PUSHd,REGI_BP},
{L_REGd		,0		,S_PUSHd,REGI_SI},{L_REGd	,0		,S_PUSHd,REGI_DI},
/* 0x258 - 0x25f */
{L_POPd		,0		,S_REGd	,REGI_AX},{L_POPd	,0		,S_REGd	,REGI_CX},
{L_POPd		,0		,S_REGd	,REGI_DX},{L_POPd	,0		,S_REGd	,REGI_BX},
{L_POPd		,0		,S_REGd	,REGI_SP},{L_POPd	,0		,S_REGd	,REGI_BP},
{L_POPd		,0		,S_REGd	,REGI_SI},{L_POPd	,0		,S_REGd	,REGI_DI},

/* 0x260 - 0x267 */
{D_PUSHAd	,0			,0		,0		},{D_POPAd	,0			,0		,0		},
{L_MODRM	,O_BOUNDd	,0		,0		},{0		,0			,0		,0		},
{L_PRESEG	,0			,0		,fs		},{L_PRESEG	,0			,0		,gs		},
{L_PREOP	,0			,0		,0		},{L_PREADD	,0			,0		,0		},
/* 0x268 - 0x26f */
{L_Id		,0			,S_PUSHd,0		},{L_MODRM	,O_IMULRd	,S_Gd	,M_EdId},
{L_Ibx		,0			,S_PUSHd,0		},{L_MODRM	,O_IMULRd	,S_Gd	,M_EdIbx},
{L_STRING	,R_INSB		,0		,0		},{L_STRING	,R_INSD		,0		,0		},
{L_STRING	,R_OUTSB	,0		,0		},{L_STRING	,R_OUTSD	,0		,0		},

/* 0x270 - 0x277 */
{L_Ibx		,O_C_O		,S_C_AIPd,0		},{L_Ibx	,O_C_NO		,S_C_AIPd,0		},
{L_Ibx		,O_C_B		,S_C_AIPd,0		},{L_Ibx	,O_C_NB		,S_C_AIPd,0		},
{L_Ibx		,O_C_Z		,S_C_AIPd,0		},{L_Ibx	,O_C_NZ		,S_C_AIPd,0		},
{L_Ibx		,O_C_BE		,S_C_AIPd,0		},{L_Ibx	,O_C_NBE	,S_C_AIPd,0		},
/* 0x278 - 0x27f */
{L_Ibx		,O_C_S		,S_C_AIPd,0		},{L_Ibx	,O_C_NS		,S_C_AIPd,0		},
{L_Ibx		,O_C_P		,S_C_AIPd,0		},{L_Ibx	,O_C_NP		,S_C_AIPd,0		},
{L_Ibx		,O_C_L		,S_C_AIPd,0		},{L_Ibx	,O_C_NL		,S_C_AIPd,0		},
{L_Ibx		,O_C_LE		,S_C_AIPd,0		},{L_Ibx	,O_C_NLE	,S_C_AIPd,0		},

/* 0x280 - 0x287 */
{L_MODRM	,0			,0		,M_GRP	},{L_MODRM	,2			,0		,M_GRP	},
{L_MODRM	,0			,0		,M_GRP	},{L_MODRM	,4			,0		,M_GRP	},
{L_MODRM	,t_TESTb	,0		,M_EbGb	},{L_MODRM	,t_TESTd	,0		,M_EdGd	},
{L_MODRM	,0			,S_EbGb	,M_GbEb	},{L_MODRM	,0			,S_EdGd	,M_GdEd	},
/* 0x288 - 0x28f */
{L_MODRM		,0		,S_Eb	,M_Gb	},{L_MODRM	,0			,S_Ed	,M_Gd	},
{L_MODRM		,0		,S_Gb	,M_Eb	},{L_MODRM	,0			,S_Gd	,M_Ed	},
{L_MODRM		,0		,S_EdMw	,M_SEG	},{L_MODRM	,0			,S_Gd	,M_EA	},
{L_MODRM		,0		,S_SEGm	,M_Ew	},{L_POPdRM	,0			,S_Ed	,M_None	},

/* 0x290 - 0x297 */
{D_NOP		,0			,0		,0		},{L_REGd	,O_XCHG_EAX	,S_REGd	,REGI_CX},
{L_REGd		,O_XCHG_EAX	,S_REGd	,REGI_DX},{L_REGd	,O_XCHG_EAX	,S_REGd	,REGI_BX},
{L_REGd		,O_XCHG_EAX	,S_REGd	,REGI_SP},{L_REGd	,O_XCHG_EAX	,S_REGd	,REGI_BP},
{L_REGd		,O_XCHG_EAX	,S_REGd	,REGI_SI},{L_REGd	,O_XCHG_EAX	,S_REGd	,REGI_DI},
/* 0x298 - 0x29f */
{D_CWDE		,0			,0		,0		},{D_CDQ	,0			,0		,0		},
{L_Ifd		,O_CALLFd	,0		,0		},{D_WAIT	,0			,0		,0		},
{D_PUSHF	,0			,0		,true	},{D_POPF	,0			,0		,true	},
{D_SAHF		,0			,0		,0		},{D_LAHF	,0			,0		,0		},

/* 0x2a0 - 0x2a7 */
{L_OP		,O_ALOP		,0		,0		},{L_OP		,O_EAXOP	,0		,0		},
{L_OP		,O_OPAL		,0		,0		},{L_OP		,O_OPEAX	,0		,0		},
{L_STRING	,R_MOVSB	,0		,0		},{L_STRING	,R_MOVSD	,0		,0		},
{L_STRING	,R_CMPSB	,0		,0		},{L_STRING	,R_CMPSD	,0		,0		},
/* 0x2a8 - 0x2af */
{L_REGbIb	,t_TESTb	,0		,REGI_AL},{L_REGdId	,t_TESTd	,0		,REGI_AX},
{L_STRING	,R_STOSB	,0		,0		},{L_STRING	,R_STOSD	,0		,0		},
{L_STRING	,R_LODSB	,0		,0		},{L_STRING	,R_LODSD	,0		,0		},
{L_STRING	,R_SCASB	,0		,0		},{L_STRING	,R_SCASD	,0		,0		},

/* 0x2b0	- 0x2b7 */
{L_Ib		,0			,S_REGb	,REGI_AL},{L_Ib	,0			,S_REGb	,REGI_CL},
{L_Ib		,0			,S_REGb	,REGI_DL},{L_Ib	,0			,S_REGb	,REGI_BL},
{L_Ib		,0			,S_REGb	,REGI_AH},{L_Ib	,0			,S_REGb	,REGI_CH},
{L_Ib		,0			,S_REGb	,REGI_DH},{L_Ib	,0			,S_REGb	,REGI_BH},
/* 0x2b8 - 0x2bf */
{L_Id		,0			,S_REGd	,REGI_AX},{L_Id	,0			,S_REGd	,REGI_CX},
{L_Id		,0			,S_REGd	,REGI_DX},{L_Id	,0			,S_REGd	,REGI_BX},
{L_Id		,0			,S_REGd	,REGI_SP},{L_Id	,0			,S_REGd	,REGI_BP},
{L_Id		,0			,S_REGd	,REGI_SI},{L_Id	,0			,S_REGd	,REGI_DI},

/* 0x2c0 - 0x2c7 */
{L_MODRM	,5			,0	,M_SHIFT_Ib	},{L_MODRM	,7			,0	,M_SHIFT_Ib	},
{L_POPd		,0			,S_IPIw	,0		},{L_POPd	,0			,S_IP	,0		},
{L_MODRM	,O_SEGES	,S_SEGGd,M_Efd	},{L_MODRM	,O_SEGDS	,S_SEGGd,M_Efd	},
{L_MODRM	,0			,S_Eb	,M_Ib	},{L_MODRM	,0			,S_Ed	,M_Id	},
/* 0x2c8 - 0x2cf */
{D_ENTERd	,0			,0		,0		},{D_LEAVEd	,0			,0		,0		},
{D_RETFdIw	,0			,0		,0		},{D_RETFd	,0			,0		,0		},
{L_VAL		,O_INT		,0		,3		},{L_Ib		,O_INT		,0		,0		},
{L_INTO		,O_INT		,0		,0		},{D_IRETd	,0			,0		,0		},

/* 0x2d0 - 0x2d7 */
{L_MODRM	,5			,0	,M_SHIFT_1	},{L_MODRM	,7			,0	,M_SHIFT_1	},
{L_MODRM	,5			,0	,M_SHIFT_CL	},{L_MODRM	,7			,0	,M_SHIFT_CL	},
{L_Ib		,O_AAM		,0		,0		},{L_Ib		,O_AAD		,0		,0		},
{D_SETALC	,0			,0		,0		},{D_XLAT	,0			,0		,0		},
/* 0x2d8 - 0x2df */
{L_MODRM	,O_FPU		,0		,0		},{L_MODRM	,O_FPU		,1		,0		},
{L_MODRM	,O_FPU		,2		,0		},{L_MODRM	,O_FPU		,3		,0		},
{L_MODRM	,O_FPU		,4		,0		},{L_MODRM	,O_FPU		,5		,0		},
{L_MODRM	,O_FPU		,6		,0		},{L_MODRM	,O_FPU		,7		,0		},

/* 0x2e0 - 0x2e7 */
{L_Ibx		,O_LOOPNZ	,S_AIPd	,0		},{L_Ibx	,O_LOOPZ	,S_AIPd	,0		},
{L_Ibx		,O_LOOP		,S_AIPd	,0		},{L_Ibx	,O_JCXZ		,S_AIPd	,0		},
{L_Ib		,O_INb		,0		,0		},{L_Ib		,O_INd		,0		,0		},
{L_Ib		,O_OUTb		,0		,0		},{L_Ib		,O_OUTd		,0		,0		},
/* 0x2e8 - 0x2ef */
{L_Id		,O_CALLNd	,S_AIPd	,0		},{L_Idx	,0			,S_AIPd	,0		},
{L_Ifd		,O_JMPFd	,0		,0		},{L_Ibx	,0			,S_AIPd	,0		},
{L_REGw		,O_INb		,0		,REGI_DX},{L_REGw	,O_INd		,0		,REGI_DX},
{L_REGw		,O_OUTb		,0		,REGI_DX},{L_REGw	,O_OUTd		,0		,REGI_DX},

/* 0x2f0 - 0x2f7 */
{D_LOCK		,0			,0		,0		},{D_ICEBP	,0			,0		,0		},
{L_PREREPNE	,0			,0		,0		},{L_PREREP	,0			,0		,0		},
{D_HLT		,0			,0		,0		},{D_CMC	,0			,0		,0		},
{L_MODRM	,8			,0		,M_GRP	},{L_MODRM	,0xa		,0		,M_GRP	},
/* 0x2f8 - 0x2ff */
{D_CLC		,0			,0		,0		},{D_STC	,0			,0		,0		},
{D_CLI		,0			,0		,0		},{D_STI	,0			,0		,0		},
{D_CLD		,0			,0		,0		},{D_STD	,0			,0		,0		},
{L_MODRM	,0xb		,0		,M_GRP	},{L_MODRM	,0xd		,0		,M_GRP	},


/* 0x300 - 0x307 */
{L_MODRM	,O_GRP6d	,S_Ew	,M_Ew	},{L_MODRM	,O_GRP7d	,S_Ew	,M_Ew	},
{L_MODRM_NVM	,O_LAR		,S_Gd	,M_EdGd	},{L_MODRM_NVM	,O_LSL		,S_Gd	,M_EdGd	},
{0			,0			,0		,0		},{0		,0			,0		,0		},
{D_CLTS		,0			,0		,0		},{0		,0			,0		,0		},
/* 0x308 - 0x30f */
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},

/* 0x310 - 0x317 */
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},
/* 0x318 - 0x31f */
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},

/* 0x320 - 0x327 */
{L_MODRM	,O_M_Rd_CRx	,S_Ed	,0		},{L_MODRM	,O_M_Rd_DRx	,S_Ed	,0		},
{L_MODRM	,O_M_CRx_Rd	,0		,M_Ed	},{L_MODRM	,O_M_DRx_Rd	,0		,M_Ed	},
{L_MODRM	,O_M_Rd_TRx	,S_Ed	,0		},{0		,0			,0		,0		},
{L_MODRM	,O_M_TRx_Rd	,0		,M_Ed	},{0		,0			,0		,0		},

/* 0x328 - 0x32f */
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},

/* 0x330 - 0x337 */
{0			,0			,0		,0		},{D_RDTSC	,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},
/* 0x338 - 0x33f */
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},

/* 0x340 - 0x347 */
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},
/* 0x348 - 0x34f */
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},

/* 0x350 - 0x357 */
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},
/* 0x358 - 0x35f */
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},

/* 0x360 - 0x367 */
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},
/* 0x368 - 0x36f */
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},


/* 0x370 - 0x377 */
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},
/* 0x378 - 0x37f */
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},

/* 0x380 - 0x387 */
{L_Idx		,O_C_O		,S_C_AIPd,0		},{L_Idx	,O_C_NO		,S_C_AIPd,0		},
{L_Idx		,O_C_B		,S_C_AIPd,0		},{L_Idx	,O_C_NB		,S_C_AIPd,0		},
{L_Idx		,O_C_Z		,S_C_AIPd,0		},{L_Idx	,O_C_NZ		,S_C_AIPd,0		},
{L_Idx		,O_C_BE		,S_C_AIPd,0		},{L_Idx	,O_C_NBE	,S_C_AIPd,0		},
/* 0x388 - 0x38f */
{L_Idx		,O_C_S		,S_C_AIPd,0		},{L_Idx	,O_C_NS		,S_C_AIPd,0		},
{L_Idx		,O_C_P		,S_C_AIPd,0		},{L_Idx	,O_C_NP		,S_C_AIPd,0		},
{L_Idx		,O_C_L		,S_C_AIPd,0		},{L_Idx	,O_C_NL		,S_C_AIPd,0		},
{L_Idx		,O_C_LE		,S_C_AIPd,0		},{L_Idx	,O_C_NLE	,S_C_AIPd,0		},

/* 0x390 - 0x397 */
{L_MODRM	,O_C_O		,S_C_Eb,0		},{L_MODRM	,O_C_NO		,S_C_Eb,0		},
{L_MODRM	,O_C_B		,S_C_Eb,0		},{L_MODRM	,O_C_NB		,S_C_Eb,0		},
{L_MODRM	,O_C_Z		,S_C_Eb,0		},{L_MODRM	,O_C_NZ		,S_C_Eb,0		},
{L_MODRM	,O_C_BE		,S_C_Eb,0		},{L_MODRM	,O_C_NBE	,S_C_Eb,0		},
/* 0x398 - 0x39f */
{L_MODRM	,O_C_S		,S_C_Eb,0		},{L_MODRM	,O_C_NS		,S_C_Eb,0		},
{L_MODRM	,O_C_P		,S_C_Eb,0		},{L_MODRM	,O_C_NP		,S_C_Eb,0		},
{L_MODRM	,O_C_L		,S_C_Eb,0		},{L_MODRM	,O_C_NL		,S_C_Eb,0		},
{L_MODRM	,O_C_LE		,S_C_Eb,0		},{L_MODRM	,O_C_NLE	,S_C_Eb,0		},

/* 0x3a0 - 0x3a7 */
{L_SEG		,0		,S_PUSHd	,fs		},{D_POPSEGd,0			,0		,fs			},
{D_CPUID	,0			,0		,0		},{L_MODRM	,O_BTd		,S_Ed	,M_EdGdt	},
{L_MODRM	,O_DSHLd	,S_Ed,M_EdGdIb	},{L_MODRM	,O_DSHLd	,S_Ed	,M_EdGdCL	},
{0			,0			,0		,0		},{0		,0			,0		,0		},
/* 0x3a8 - 0x3af */
{L_SEG		,0		,S_PUSHd	,gs		},{D_POPSEGd,0			,0		,gs			},
{0			,0			,0		,0		},{L_MODRM	,O_BTSd		,S_Ed	,M_EdGdt	},
{L_MODRM	,O_DSHRd	,S_Ed,M_EdGdIb	},{L_MODRM	,O_DSHRd	,S_Ed	,M_EdGdCL	},
{0			,0			,0		,0		},{L_MODRM	,O_IMULRd	,S_Gd	,M_EdxGdx	},

/* 0x3b0 - 0x3b7 */
{0			,0			,0		,0		},{L_MODRM	,O_CMPXCHG	,S_Ed	,M_Ed	},
{L_MODRM	,O_SEGSS	,S_SEGGd,M_Efd	},{L_MODRM	,O_BTRd		,S_Ed	,M_EdGdt	},
{L_MODRM	,O_SEGFS	,S_SEGGd,M_Efd	},{L_MODRM	,O_SEGGS	,S_SEGGd,M_Efd	},
{L_MODRM	,0			,S_Gd	,M_Eb	},{L_MODRM	,0			,S_Gd	,M_Ew	},
/* 0x3b8 - 0x3bf */
{0			,0			,0		,0		},{0		,0			,0		,0		},
{L_MODRM	,0xf		,0		,M_GRP	},{L_MODRM	,O_BTCd		,S_Ed	,M_EdGdt	},
{L_MODRM	,O_BSFd		,S_Gd	,M_Ed	},{L_MODRM	,O_BSRd		,S_Gd	,M_Ed	},
{L_MODRM	,0			,S_Gd	,M_Ebx	},{L_MODRM	,0			,S_Gd	,M_Ewx	},

/* 0x3c0 - 0x3cc */
{L_MODRM		,t_ADDb			,S_EbGb		,M_GbEb		},{L_MODRM		,t_ADDd		,S_EdGd		,M_GdEd	},
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},
/* 0x3c8 - 0x3cf */
{L_REGd		,O_BSWAPd	,S_REGd	,REGI_AX},{L_REGd	,O_BSWAPd	,S_REGd	,REGI_CX},
{L_REGd		,O_BSWAPd	,S_REGd	,REGI_DX},{L_REGd	,O_BSWAPd	,S_REGd	,REGI_BX},
{L_REGd		,O_BSWAPd	,S_REGd	,REGI_SP},{L_REGd	,O_BSWAPd	,S_REGd	,REGI_BP},
{L_REGd		,O_BSWAPd	,S_REGd	,REGI_SI},{L_REGd	,O_BSWAPd	,S_REGd	,REGI_DI},

/* 0x3d0 - 0x3d7 */
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},
/* 0x3d8 - 0x3df */
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},

/* 0x3e0 - 0x3ee */
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},
/* 0x3e8 - 0x3ef */
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},

/* 0x3f0 - 0x3fc */
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},
/* 0x3f8 - 0x3ff */
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},
{0			,0			,0		,0		},{0		,0			,0		,0		},

};

static OpCode Groups[16][8]={
{	/* 0x00 Group 1 Eb,Ib */
{0		,t_ADDb		,S_Eb	,M_EbIb		},{0	,t_ORb		,S_Eb	,M_EbIb		},
{0		,t_ADCb		,S_Eb	,M_EbIb		},{0	,t_SBBb		,S_Eb	,M_EbIb		},
{0		,t_ANDb		,S_Eb	,M_EbIb		},{0	,t_SUBb		,S_Eb	,M_EbIb		},
{0		,t_XORb		,S_Eb	,M_EbIb		},{0	,t_CMPb		,0		,M_EbIb		},
},{	/* 0x01 Group 1 Ew,Iw */
{0		,t_ADDw		,S_Ew	,M_EwIw		},{0	,t_ORw		,S_Ew	,M_EwIw		},
{0		,t_ADCw		,S_Ew	,M_EwIw		},{0	,t_SBBw		,S_Ew	,M_EwIw		},
{0		,t_ANDw		,S_Ew	,M_EwIw		},{0	,t_SUBw		,S_Ew	,M_EwIw		},
{0		,t_XORw		,S_Ew	,M_EwIw		},{0	,t_CMPw		,0		,M_EwIw		},
},{	/* 0x02 Group 1 Ed,Id */
{0		,t_ADDd		,S_Ed	,M_EdId		},{0	,t_ORd		,S_Ed	,M_EdId		},
{0		,t_ADCd		,S_Ed	,M_EdId		},{0	,t_SBBd		,S_Ed	,M_EdId		},
{0		,t_ANDd		,S_Ed	,M_EdId		},{0	,t_SUBd		,S_Ed	,M_EdId		},
{0		,t_XORd		,S_Ed	,M_EdId		},{0	,t_CMPd		,0		,M_EdId		},
},{	/* 0x03 Group 1 Ew,Ibx */
{0		,t_ADDw		,S_Ew	,M_EwIbx	},{0	,t_ORw		,S_Ew	,M_EwIbx	},
{0		,t_ADCw		,S_Ew	,M_EwIbx	},{0	,t_SBBw		,S_Ew	,M_EwIbx	},
{0		,t_ANDw		,S_Ew	,M_EwIbx	},{0	,t_SUBw		,S_Ew	,M_EwIbx	},
{0		,t_XORw		,S_Ew	,M_EwIbx	},{0	,t_CMPw		,0		,M_EwIbx	},
},{	/* 0x04 Group 1 Ed,Ibx */
{0		,t_ADDd		,S_Ed	,M_EdIbx	},{0	,t_ORd		,S_Ed	,M_EdIbx	},
{0		,t_ADCd		,S_Ed	,M_EdIbx	},{0	,t_SBBd		,S_Ed	,M_EdIbx	},
{0		,t_ANDd		,S_Ed	,M_EdIbx	},{0	,t_SUBd		,S_Ed	,M_EdIbx	},
{0		,t_XORd		,S_Ed	,M_EdIbx	},{0	,t_CMPd		,0		,M_EdIbx	},

},{	/* 0x05 Group 2 Eb,XXX */
{0		,t_ROLb		,S_Eb	,M_Eb		},{0	,t_RORb		,S_Eb	,M_Eb		},
{0		,t_RCLb		,S_Eb	,M_Eb		},{0	,t_RCRb		,S_Eb	,M_Eb		},
{0		,t_SHLb		,S_Eb	,M_Eb		},{0	,t_SHRb		,S_Eb	,M_Eb		},
{0		,t_SHLb		,S_Eb	,M_Eb		},{0	,t_SARb		,S_Eb	,M_Eb		},
},{	/* 0x06 Group 2 Ew,XXX */
{0		,t_ROLw		,S_Ew	,M_Ew		},{0	,t_RORw		,S_Ew	,M_Ew		},
{0		,t_RCLw		,S_Ew	,M_Ew		},{0	,t_RCRw		,S_Ew	,M_Ew		},
{0		,t_SHLw		,S_Ew	,M_Ew		},{0	,t_SHRw		,S_Ew	,M_Ew		},
{0		,t_SHLw		,S_Ew	,M_Ew		},{0	,t_SARw		,S_Ew	,M_Ew		},
},{	/* 0x07 Group 2 Ed,XXX */
{0		,t_ROLd		,S_Ed	,M_Ed		},{0	,t_RORd		,S_Ed	,M_Ed		},
{0		,t_RCLd		,S_Ed	,M_Ed		},{0	,t_RCRd		,S_Ed	,M_Ed		},
{0		,t_SHLd		,S_Ed	,M_Ed		},{0	,t_SHRd		,S_Ed	,M_Ed		},
{0		,t_SHLd		,S_Ed	,M_Ed		},{0	,t_SARd		,S_Ed	,M_Ed		},


},{	/* 0x08 Group 3 Eb */
{0		,t_TESTb	,0		,M_EbIb		},{0	,t_TESTb	,0		,M_EbIb		},
{0		,O_NOT		,S_Eb	,M_Eb		},{0	,t_NEGb		,S_Eb	,M_Eb		},
{0		,O_MULb		,0		,M_Eb		},{0	,O_IMULb	,0		,M_Eb		},
{0		,O_DIVb		,0		,M_Eb		},{0	,O_IDIVb	,0		,M_Eb		},
},{	/* 0x09 Group 3 Ew */
{0		,t_TESTw	,0		,M_EwIw		},{0	,t_TESTw	,0		,M_EwIw		},
{0		,O_NOT		,S_Ew	,M_Ew		},{0	,t_NEGw		,S_Ew	,M_Ew		},
{0		,O_MULw		,0		,M_Ew		},{0	,O_IMULw	,0		,M_Ew		},
{0		,O_DIVw		,0		,M_Ew		},{0	,O_IDIVw	,0		,M_Ew		},
},{	/* 0x0a Group 3 Ed */
{0		,t_TESTd	,0		,M_EdId		},{0	,t_TESTd	,0		,M_EdId		},
{0		,O_NOT		,S_Ed	,M_Ed		},{0	,t_NEGd		,S_Ed	,M_Ed		},
{0		,O_MULd		,0		,M_Ed		},{0	,O_IMULd	,0		,M_Ed		},
{0		,O_DIVd		,0		,M_Ed		},{0	,O_IDIVd	,0		,M_Ed		},

},{	/* 0x0b Group 4 Eb */
{0		,t_INCb		,S_Eb	,M_Eb		},{0	,t_DECb		,S_Eb	,M_Eb		},
{0		,0			,0		,0			},{0	,0			,0		,0			},
{0		,0			,0		,0			},{0	,0			,0		,0			},
{0		,0			,0		,0			},{0	,O_CBACK	,0		,M_Iw		},
},{	/* 0x0c Group 5 Ew */
{0		,t_INCw		,S_Ew	,M_Ew		},{0	,t_DECw		,S_Ew	,M_Ew		},
{0		,O_CALLNw	,S_IP	,M_Ew		},{0	,O_CALLFw	,0		,M_Efw		},
{0		,0			,S_IP	,M_Ew		},{0	,O_JMPFw	,0		,M_Efw		},
{0		,0			,S_PUSHw,M_Ew		},{0	,0			,0		,0			},
},{	/* 0x0d Group 5 Ed */
{0		,t_INCd		,S_Ed	,M_Ed		},{0	,t_DECd		,S_Ed	,M_Ed		},
{0		,O_CALLNd	,S_IP	,M_Ed		},{0	,O_CALLFd	,0		,M_Efd		},
{0		,0			,S_IP	,M_Ed		},{0	,O_JMPFd	,0		,M_Efd		},
{0		,0			,S_PUSHd,M_Ed		},{0	,0			,0		,0			},


},{	/* 0x0e Group 8 Ew */
{0		,0			,0		,0			},{0	,0			,0		,0			},
{0		,0			,0		,0			},{0	,0			,0		,0			},
{0		,O_BTw		,S_Ew	,M_EwIb		},{0	,O_BTSw		,S_Ew	,M_EwIb		},
{0		,O_BTRw		,S_Ew	,M_EwIb		},{0	,O_BTCw		,S_Ew	,M_EwIb		},
},{	/* 0x0f Group 8 Ed */
{0		,0			,0		,0			},{0	,0			,0		,0			},
{0		,0			,0		,0			},{0	,0			,0		,0			},
{0		,O_BTd		,S_Ed	,M_EdIb		},{0	,O_BTSd		,S_Ed	,M_EdIb		},
{0		,O_BTRd		,S_Ed	,M_EdIb		},{0	,O_BTCd		,S_Ed	,M_EdIb		},



}
};



