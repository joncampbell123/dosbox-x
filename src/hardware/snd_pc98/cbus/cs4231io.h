

#ifdef __cplusplus
extern "C" {
#endif

void cs4231io_reset(void);
void cs4231io_bind(void);

void IOOUTCALL cs4231io0_w8(UINT port, REG8 value);
REG8 IOINPCALL cs4231io0_r8(UINT port);
void IOOUTCALL cs4231io2_w8(UINT port, REG8 value);
REG8 IOINPCALL cs4231io2_r8(UINT port);
void IOOUTCALL cs4231io5_w8(UINT port, REG8 value);
REG8 IOINPCALL cs4231io5_r8(UINT port);

#ifdef __cplusplus
}
#endif

