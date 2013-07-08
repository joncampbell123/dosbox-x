#ifndef DOSBOX_LOGGING_H
#define DOSBOX_LOGGING_H
enum LOG_TYPES {
	LOG_ALL,
	LOG_VGA, LOG_VGAGFX,LOG_VGAMISC,LOG_INT10,
	LOG_SB,LOG_DMACONTROL,
	LOG_FPU,LOG_CPU,LOG_PAGING,
	LOG_FCB,LOG_FILES,LOG_IOCTL,LOG_EXEC,LOG_DOSMISC,
	LOG_PIT,LOG_KEYBOARD,LOG_PIC,
	LOG_MOUSE,LOG_BIOS,LOG_GUI,LOG_MISC,
	LOG_IO,
	LOG_MAX
};

enum LOG_SEVERITIES {
	LOG_NORMAL,
	LOG_WARN,
	LOG_ERROR
};

#if C_DEBUG
class LOG 
{ 
	LOG_TYPES       d_type;
	LOG_SEVERITIES  d_severity;
public:

	LOG (LOG_TYPES type , LOG_SEVERITIES severity):
		d_type(type),
		d_severity(severity)
		{}
	void operator() (char const* buf, ...) GCC_ATTRIBUTE(__format__(__printf__, 2, 3));  //../src/debug/debug_gui.cpp

};

void DEBUG_ShowMsg(char const* format,...) GCC_ATTRIBUTE(__format__(__printf__, 1, 2));
#define LOG_MSG DEBUG_ShowMsg

#else  //C_DEBUG

struct LOG
{
	LOG(LOG_TYPES , LOG_SEVERITIES )										{ }
	void operator()(char const* )													{ }
	void operator()(char const* , double )											{ }
	void operator()(char const* , double , double )								{ }
	void operator()(char const* , double , double , double )					{ }
	void operator()(char const* , double , double , double , double )					{ }
	void operator()(char const* , double , double , double , double , double )					{ }

	void operator()(char const* , char const* )									{ }
	void operator()(char const* , char const* , double )							{ }
	void operator()(char const* , char const* , double ,double )				{ }
	void operator()(char const* , double , char const* )						{ }
	void operator()(char const* , double , double, char const* )						{ }
	void operator()(char const* , char const*, char const*)				{ }


}; //add missing operators to here
	//try to avoid anything smaller than bit32...
void GFX_ShowMsg(char const* format,...) GCC_ATTRIBUTE(__format__(__printf__, 1, 2));
#define LOG_MSG GFX_ShowMsg

#endif //C_DEBUG


#endif //DOSBOX_LOGGING_H
