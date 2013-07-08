enum {
	L_N=0,
	L_SKIP,
	/* Grouped ones using MOD/RM */
	L_MODRM,L_MODRM_NVM,L_POPwRM,L_POPdRM,
	
	L_Ib,L_Iw,L_Id,
	L_Ibx,L_Iwx,L_Idx,				//Sign extend
	L_Ifw,L_Ifd,
	L_OP,

	L_REGb,L_REGw,L_REGd,
	L_REGbIb,L_REGwIw,L_REGdId,
	L_POPw,L_POPd,
	L_POPfw,L_POPfd,
	L_SEG,

	L_INTO,

	L_VAL,
	L_PRESEG,
	L_DOUBLE,
	L_PREOP,L_PREADD,L_PREREP,L_PREREPNE,
	L_STRING,

/* Direct ones */
	D_IRETw,D_IRETd,
	D_PUSHAw,D_PUSHAd,
	D_POPAw,D_POPAd,
	D_POPSEGw,D_POPSEGd,
	D_DAA,D_DAS,
	D_AAA,D_AAS,
	D_CBW,D_CWDE,
	D_CWD,D_CDQ,
	D_SETALC,
	D_XLAT,
	D_CLI,D_STI,D_STC,D_CLC,D_CMC,D_CLD,D_STD,
	D_NOP,D_WAIT,
	D_ENTERw,D_ENTERd,
	D_LEAVEw,D_LEAVEd,
	
	D_RETFw,D_RETFd,
	D_RETFwIw,D_RETFdIw,
	D_POPF,D_PUSHF,
	D_SAHF,D_LAHF,
	D_CPUID,
	D_HLT,D_CLTS,
	D_LOCK,D_ICEBP,
	L_ERROR
};


enum {
	O_N=t_LASTFLAG,
	O_COND,
	O_XCHG_AX,O_XCHG_EAX,
	O_IMULRw,O_IMULRd,
	O_BOUNDw,O_BOUNDd,
	O_CALLNw,O_CALLNd,
	O_CALLFw,O_CALLFd,
	O_JMPFw,O_JMPFd,

	O_OPAL,O_ALOP,
	O_OPAX,O_AXOP,
	O_OPEAX,O_EAXOP,
	O_INT,
	O_SEGDS,O_SEGES,O_SEGFS,O_SEGGS,O_SEGSS,
	O_LOOP,O_LOOPZ,O_LOOPNZ,O_JCXZ,
	O_INb,O_INw,O_INd,
	O_OUTb,O_OUTw,O_OUTd,

	O_NOT,O_AAM,O_AAD,
	O_MULb,O_MULw,O_MULd,
	O_IMULb,O_IMULw,O_IMULd,
	O_DIVb,O_DIVw,O_DIVd,
	O_IDIVb,O_IDIVw,O_IDIVd,
	O_CBACK,


	O_DSHLw,O_DSHLd,
	O_DSHRw,O_DSHRd,
	O_C_O	,O_C_NO	,O_C_B	,O_C_NB	,O_C_Z	,O_C_NZ	,O_C_BE	,O_C_NBE,
	O_C_S	,O_C_NS	,O_C_P	,O_C_NP	,O_C_L	,O_C_NL	,O_C_LE	,O_C_NLE,

	O_GRP6w,O_GRP6d,
	O_GRP7w,O_GRP7d,
	O_M_CRx_Rd,O_M_Rd_CRx,
	O_M_DRx_Rd,O_M_Rd_DRx,
	O_M_TRx_Rd,O_M_Rd_TRx,
	O_LAR,O_LSL,
	O_ARPL,
	
	O_BTw,O_BTSw,O_BTRw,O_BTCw,
	O_BTd,O_BTSd,O_BTRd,O_BTCd,
	O_BSFw,O_BSRw,O_BSFd,O_BSRd,

	O_BSWAPw, O_BSWAPd,
	O_CMPXCHG,
	O_FPU


};

enum {
	S_N=0,
	S_C_Eb,
	S_Eb,S_Gb,S_EbGb,
	S_Ew,S_Gw,S_EwGw,
	S_Ed,S_Gd,S_EdGd,S_EdMw,

	
	S_REGb,S_REGw,S_REGd,
	S_PUSHw,S_PUSHd,
	S_SEGm,
	S_SEGGw,S_SEGGd,

	
	S_AIPw,S_C_AIPw,
	S_AIPd,S_C_AIPd,

	S_IP,S_IPIw
};

enum {
	R_OUTSB,R_OUTSW,R_OUTSD,
	R_INSB,R_INSW,R_INSD,
	R_MOVSB,R_MOVSW,R_MOVSD,
	R_LODSB,R_LODSW,R_LODSD,
	R_STOSB,R_STOSW,R_STOSD,
	R_SCASB,R_SCASW,R_SCASD,
	R_CMPSB,R_CMPSW,R_CMPSD
};

enum {
	M_None=0,
	M_Ebx,M_Eb,M_Gb,M_EbGb,M_GbEb,
	M_Ewx,M_Ew,M_Gw,M_EwGw,M_GwEw,M_EwxGwx,M_EwGwt,
	M_Edx,M_Ed,M_Gd,M_EdGd,M_GdEd,M_EdxGdx,M_EdGdt,
	
	M_EbIb,M_EwIb,M_EdIb,
	M_EwIw,M_EwIbx,M_EwxIbx,M_EwxIwx,M_EwGwIb,M_EwGwCL,
	M_EdId,M_EdIbx,M_EdGdIb,M_EdGdCL,
	
	M_Efw,M_Efd,
	
	M_Ib,M_Iw,M_Id,


	M_SEG,M_EA,
	M_GRP,
	M_GRP_Ib,M_GRP_CL,M_GRP_1,

	M_POPw,M_POPd
};

struct OpCode {
	Bit8u load,op,save,extra;
};

struct FullData {
	Bitu entry;
	Bitu rm;
	EAPoint rm_eaa;
	Bitu rm_off;
	Bitu rm_eai;
	Bitu rm_index;
	Bitu rm_mod;
	OpCode code;
	EAPoint cseip;
#ifdef WORDS_BIGENDIAN
	union {
		Bit32u dword[1];
		Bit32s dwords[1];
		Bit16u word[2];
		Bit16s words[2];
		Bit8u byte[4];
		Bit8s bytes[4];
		} blah1,blah2,blah_imm;
#else
	union {	
		Bit8u b;Bit8s bs;
		Bit16u w;Bit16s ws;
		Bit32u d;Bit32s ds;
	} op1,op2,imm;
#endif
	Bitu new_flags;
	struct {
		EAPoint base;
	} seg;
	Bitu cond;
	bool repz;
	Bitu prefix;
};

/* Some defines to get the names correct. */
#ifdef WORDS_BIGENDIAN

#define inst_op1_b  inst.blah1.byte[3]
#define inst_op1_bs inst.blah1.bytes[3]
#define inst_op1_w  inst.blah1.word[1]
#define inst_op1_ws inst.blah1.words[1]
#define inst_op1_d  inst.blah1.dword[0]
#define inst_op1_ds inst.blah1.dwords[0]

#define inst_op2_b  inst.blah2.byte[3]
#define inst_op2_bs inst.blah2.bytes[3]
#define inst_op2_w  inst.blah2.word[1]
#define inst_op2_ws inst.blah2.words[1]
#define inst_op2_d  inst.blah2.dword[0]
#define inst_op2_ds inst.blah2.dwords[0]

#define inst_imm_b  inst.blah_imm.byte[3]
#define inst_imm_bs inst.blah_imm.bytes[3]
#define inst_imm_w  inst.blah_imm.word[1]
#define inst_imm_ws inst.blah_imm.words[1]
#define inst_imm_d  inst.blah_imm.dword[0]
#define inst_imm_ds inst.blah_imm.dwords[0]

#else

#define inst_op1_b  inst.op1.b
#define inst_op1_bs inst.op1.bs
#define inst_op1_w  inst.op1.w
#define inst_op1_ws inst.op1.ws
#define inst_op1_d  inst.op1.d
#define inst_op1_ds inst.op1.ds

#define inst_op2_b  inst.op2.b
#define inst_op2_bs inst.op2.bs
#define inst_op2_w  inst.op2.w
#define inst_op2_ws inst.op2.ws
#define inst_op2_d  inst.op2.d
#define inst_op2_ds inst.op2.ds

#define inst_imm_b  inst.imm.b
#define inst_imm_bs inst.imm.bs
#define inst_imm_w  inst.imm.w
#define inst_imm_ws inst.imm.ws
#define inst_imm_d  inst.imm.d
#define inst_imm_ds inst.imm.ds

#endif


#define PREFIX_NONE		0x0
#define PREFIX_ADDR		0x1
#define PREFIX_SEG		0x2
#define PREFIX_REP		0x4

