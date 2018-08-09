
extern unsigned char*		avi_io_buf;
extern unsigned char*		avi_io_read;
extern unsigned char*		avi_io_write;
extern unsigned char*		avi_io_fence;
extern size_t			avi_io_elemsize;
extern size_t			avi_io_next_adv;
extern size_t			avi_io_elemcount;
extern unsigned char*		avi_io_readfence;

unsigned char *avi_io_buffer_init(size_t structsize);
void avi_io_buffer_free();

