#define SaveIP() reg_eip=(Bit32u)(inst.cseip-SegBase(cs));
#define LoadIP() inst.cseip=SegBase(cs)+reg_eip;
#define GetIP()	(inst.cseip-SegBase(cs))

#define RunException() {										\
	CPU_Exception(cpu.exception.which,cpu.exception.error);		\
	continue;													\
}

static INLINE Bit8u the_Fetchb(EAPoint & loc) {
	Bit8u temp=LoadMb(loc);
	loc+=1;
	return temp;
}
	
static INLINE Bit16u the_Fetchw(EAPoint & loc) {
	Bit16u temp=LoadMw(loc);
	loc+=2;
	return temp;
}
static INLINE Bit32u the_Fetchd(EAPoint & loc) {
	Bit32u temp=LoadMd(loc);
	loc+=4;
	return temp;
}

#define Fetchb() the_Fetchb(inst.cseip)
#define Fetchw() the_Fetchw(inst.cseip)
#define Fetchd() the_Fetchd(inst.cseip)

#define Fetchbs() (Bit8s)the_Fetchb(inst.cseip)
#define Fetchws() (Bit16s)the_Fetchw(inst.cseip)
#define Fetchds() (Bit32s)the_Fetchd(inst.cseip)

#define Push_16 CPU_Push16
#define Push_32 CPU_Push32
#define Pop_16 CPU_Pop16
#define Pop_32 CPU_Pop32

