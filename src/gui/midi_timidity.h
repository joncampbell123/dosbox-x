/*
 * Output to TiMidity++ MIDI server support
 *                            by Dmitry Marakasov <amdmi3@amdmi3.ru>
 * based on:
 * - Raw output support (seq.cpp) by Michael Pearce
 * - Pseudo /dev/sequencer of TiMidity (timidity-io.c)
 *                                 by Masanao Izumo <mo@goice.co.jp>
 * - sys/soundcard.h by Hannu Savolainen (got from my FreeBSD
 *   distribution, for which it was modified by Luigi Rizzo)
 *
 */
/* NTS: Anyone notice that while midi.cpp includes this file, it doesn't
 *      actually use the Timidity MIDI handler? You could delete this header
 *      entirely and remove the #include and it would have no effect on
 *      DOSBox-X. --J.C. */

#ifdef C_SDL_NET
//#ifdef C_TIMIDITY

#include "SDL.h"
#include "SDL_net.h"

#define SEQ_MIDIPUTC 5

#define TIMIDITY_LOW_DELAY

#ifdef TIMIDITY_LOW_DELAY
#define BUF_LOW_SYNC	0.1
#define BUF_HIGH_SYNC	0.15
#else
#define BUF_LOW_SYNC	0.4
#define BUF_HIGH_SYNC	0.8
#endif

/* default host & port */
#define DEFAULT_TIMIDITY_HOST "127.0.0.1"
#define DEFAULT_TIMIDITY_PORT 7777

class MidiHandler_timidity: public MidiHandler {
public:
        MidiHandler_timidity() : MidiHandler(),_isOpen(false),_device_num(0),
				 _controlbuffer_count(0), _controlbuffer_size(0) {
		if(!SDLNetInited) {
			if (SDLNet_Init() == -1) {
				LOG_MSG("SDLNet_Init failed: %s\n", SDLNet_GetError());
				return;
			}
			SDLNetInited = true;
		}
	};

	const char * GetName(void) { return "timidity";};
	bool	Open(const char * conf);
	void	Close(void);
	void	PlayMsg(uint8_t * msg);
	void	PlaySysex(uint8_t *msg, Bitu length);

private:
	/* creates a tcp connection to TiMidity server, returns filedesc (like open()) */
	bool	connect_to_server(const char* hostname, int tcp_port, TCPsocket * sd);

	/* send command to the server; printf-like; returns http status and reply string (by reference). buff is expected to have at least BUFSIZ size */
	int	timidity_ctl_command(char *buff, const char *fmt, ...);

	/* timidity data socket-related stuff */
	void	timidity_meta_seq(int p1, int p2, int p3);
	int	timidity_sync(int centsec);
	int	timidity_eot();

	/* write() analogue for any midi data */
	void	timidity_write_data(const void *buf, int nbytes);

	/* get single line of server reply on control connection */
	int	fdgets(char *buff, size_t buff_size);

	/* teardown connection to server */
	void	teardown();

	/* close (if needed) and nullify both control and data filedescs */
	void	close_all();

private:
	bool	_isOpen;
	int	_device_num;

	/* buffer for partial data read from _control_fd - from timidity-io.c, see fdgets() */
	char	_controlbuffer[BUFSIZ];
	int	_controlbuffer_count;	/* beginning of read pointer */
	int	_controlbuffer_size;	/* end of read pointer */

	TCPsocket control_socket, data_socket;
};

bool MidiHandler_timidity::Open(const char *conf) {
	char	timidity_host[512], * p, res[BUFSIZ];
	int	timidity_port, data_port, status;

	/* count ourselves open */
	if (_isOpen) return false;
	_isOpen = true;

	if (conf && conf[0]) safe_strncpy(timidity_host, conf, 512);
	else safe_strncpy(timidity_host, DEFAULT_TIMIDITY_HOST, 512);

	if ((p = strrchr(timidity_host, ',')) != NULL) {
		*p++ = '\0';
		_device_num = atoi(p);
	} else {
		_device_num = 0;
	}

	/* extract control port */
	if ((p = strrchr(timidity_host, ':')) != NULL) {
		*p++ = '\0';
		timidity_port = atoi(p);
	} else {
		timidity_port = DEFAULT_TIMIDITY_PORT;
	}

	/*
	 * create control connection to the server
	 */
	if ((connect_to_server(timidity_host, timidity_port, &control_socket)) == false) {
		LOG_MSG("TiMidity: can't open control connection (host=%s, port=%d)", timidity_host, timidity_port);
		return false;
	}

	/* should read greeting issued by server upon connect:
	 * "220 TiMidity++ v2.13.2 ready)" */
	status = timidity_ctl_command(res, NULL);
	if (status != 220) {
		LOG_MSG("TiMidity: bad response from server (host=%s, port=%d): %s", timidity_host, timidity_port, res);
		close_all();
		return false;
	}

	/*
	 * setup buf and prepare data connection
	 */
	/* should read: "200 OK" */
	status = timidity_ctl_command(res, "SETBUF %f %f", BUF_LOW_SYNC, BUF_HIGH_SYNC);
	if (status != 200)
		LOG_MSG("TiMidity: bad reply for SETBUF command: %s", res);

	/* should read something like "200 63017 is ready acceptable",
	 * where 63017 is port for data connection */
#ifdef WORDS_BIGENDIAN
		status = timidity_ctl_command(res, "OPEN msb");
#else
		status = timidity_ctl_command(res, "OPEN lsb");
#endif

	if (status != 200) {
		LOG_MSG("TiMidity: bad reply for OPEN command: %s", res);
		close_all();
		return false;
	}

	/*
	 * open data connection
	 */
	data_port = atoi(res + 4);
	if (connect_to_server(timidity_host, data_port, &data_socket) == false) {
		LOG_MSG("TiMidity: can't open data connection (host=%s, port=%d)", timidity_host, data_port);
		close_all();
		return false;
	}

	/* should read message issued after connecting to data port:
	 * "200 Ready data connection" */
	status = timidity_ctl_command(res, NULL);
	if (status != 200) {
		LOG_MSG("Can't connect timidity: %s\t(host=%s, port=%d)\n", res, timidity_host, data_port);
		close_all();
		return false;
	}

	return true;
}

void MidiHandler_timidity::Close() {
	teardown();
}

void MidiHandler_timidity::close_all() {
	SDLNet_TCP_Close (control_socket);
	SDLNet_TCP_Close (data_socket);

	_isOpen = false;
}

void MidiHandler_timidity::teardown() {
	char res[BUFSIZ];
	int status;

	/* teardown connection to server (see timidity-io.c) if it
	 * is initialized */
	if (_isOpen) {
		timidity_eot();
		timidity_sync(0);

		/* scroll through all "302 Data connection is (already) closed"
		 * messages till we reach something like "200 Bye" */
		do {
			status = timidity_ctl_command(res, "QUIT");
		} while (status && status != 302);
	}

	/* now close and nullify both filedescs */
	close_all();
}

bool MidiHandler_timidity::connect_to_server(const char* hostname, int tcp_port,
					     TCPsocket * sd) {
	IPaddress ip;

	if (SDLNet_ResolveHost(&ip, hostname, tcp_port) < 0)
	{
		LOG_MSG("SDLNet_ResolveHost: %s\n", SDLNet_GetError());
		return false;
	}

	if (!(*sd = SDLNet_TCP_Open(&ip)))
	{
		LOG_MSG("SDLNet_TCP_Open: %s\n", SDLNet_GetError());
		return false;
	}

	return true;
}

int MidiHandler_timidity::timidity_ctl_command(char * buff, const char *fmt, ...) {
	int status, len;
	va_list ap;

	if (fmt != NULL) {
		/* if argumends are present, write them to control connection */
		va_start(ap, fmt);
		len = vsnprintf(buff, BUFSIZ-1, fmt, ap); /* leave one byte for \n */
		va_end(ap);
		if (len <= 0 || len >= BUFSIZ-1) {
			LOG_MSG("timidity_ctl_command: vsnprintf returned %d!\n", len);
			return 0;
		}

		/* add newline if needed */
		if (buff[len-1] != '\n')
			buff[len++] = '\n';

		/* write command to control socket */
		if (SDLNet_TCP_Send(control_socket, buff, len) < len) {
			LOG_MSG("SDLNet_TCP_Send: %s\n", SDLNet_GetError());
		}
 	}

	while (1) {
		/* read reply */
		if (fdgets(buff, BUFSIZ) <= 0) {
			strcpy(buff, "Read error\n");
			return 0;
		}

		/* report errors from server */
		status = atoi(buff);
		if (400 <= status && status <= 499) { /* Error of data stream */
			LOG_MSG("TiMidity: error from server: %s", buff);
			continue;
		}
		break;
	}

	return status;
}

void MidiHandler_timidity::timidity_meta_seq(int p1, int p2, int p3) {
	/* see _CHN_COMMON from soundcard.h; this is simplified
	 * to just send seq to the server without any buffers,
	 * delays and extra functions/macros */
	uint8_t seqbuf[8];

	seqbuf[0] = 0x92;
	seqbuf[1] = 0;
	seqbuf[2] = 0xff;
	seqbuf[3] = 0x7f;
	seqbuf[4] = p1;
	seqbuf[5] = p2;
	*(Bit16s *)&seqbuf[6] = p3;

	timidity_write_data(seqbuf, sizeof(seqbuf));
}

int MidiHandler_timidity::timidity_sync(int centsec) {
	char res[BUFSIZ];
	int status;
	unsigned long sleep_usec;

	timidity_meta_seq(0x02, 0x00, centsec); /* Wait playout */

	/* Wait "301 Sync OK" */
	do {
		status = timidity_ctl_command(res, NULL);

		if (status != 301)
			LOG_MSG("TiMidity: error: SYNC: %s", res);

	} while (status && status != 301);

	if (status != 301)
		return -1; /* error */

	sleep_usec = (unsigned long)(atof(res + 4) * 1000000);

	if (sleep_usec > 0)
		SDL_Delay (sleep_usec / 1000);

	return 0;
}

int MidiHandler_timidity::timidity_eot(void) {
	timidity_meta_seq(0x00, 0x00, 0); /* End of playing */
	return timidity_sync(0);
}

void MidiHandler_timidity::timidity_write_data(const void *buf, int nbytes) {
	if (SDLNet_TCP_Send(data_socket, buf, nbytes) < nbytes) {
		LOG_MSG("TiMidity: DATA WRITE FAILED (%s), DISABLING MUSIC OUTPUT", SDLNet_GetError());
		close_all();
	}
}

int MidiHandler_timidity::fdgets(char *buff, size_t buff_size) {
	int n, count, size;
	char *buff_endp = buff + buff_size - 1, *pbuff, *beg;

	count = _controlbuffer_count;
	size = _controlbuffer_size;
	pbuff = _controlbuffer;
	beg = buff;
	do {
		if (count == size) {

			if ((n = SDLNet_TCP_Recv(control_socket, pbuff, BUFSIZ)) <= 0) {
				*buff = '\0';
				if (n == 0) {
					_controlbuffer_count = _controlbuffer_size = 0;
					return buff - beg;
				}
				return -1; /* < 0 error */
			}
			count = _controlbuffer_count = 0;
			size = _controlbuffer_size = n;
		}
		*buff++ = pbuff[count++];
	} while (*(buff - 1) != '\n' && buff != buff_endp);

	*buff = '\0';
	_controlbuffer_count = count;

	return buff - beg;
}

void MidiHandler_timidity::PlayMsg(uint8_t *msg) {
	uint8_t buf[256];
	int position = 0;

	switch (msg[0] & 0xF0) {
	case 0x80:
	case 0x90:
	case 0xA0:
	case 0xB0:
	case 0xE0:
		buf[position++] = SEQ_MIDIPUTC;
		buf[position++] = msg[0];
		buf[position++] = _device_num;
		buf[position++] = 0;
		buf[position++] = SEQ_MIDIPUTC;
		buf[position++] = msg[1] & 0x7F;
		buf[position++] = _device_num;
		buf[position++] = 0;
		buf[position++] = SEQ_MIDIPUTC;
		buf[position++] = msg[2] & 0x7F;
		buf[position++] = _device_num;
		buf[position++] = 0;
		break;
	case 0xC0:
	case 0xD0:
		buf[position++] = SEQ_MIDIPUTC;
		buf[position++] = msg[0];
		buf[position++] = _device_num;
		buf[position++] = 0;
		buf[position++] = SEQ_MIDIPUTC;
		buf[position++] = msg[1] & 0x7F;
		buf[position++] = _device_num;
		buf[position++] = 0;
		break;
	default:
		LOG_MSG("MidiHandler_timidity::PlayMsg: unknown : %08lx", (long)msg);
		break;
	}

	timidity_write_data(buf, position);
}

void MidiHandler_timidity::PlaySysex(uint8_t *msg, Bitu length) {
	uint8_t buf[SYSEX_SIZE*4+8];
	int position = 0;
	const uint8_t *chr = msg;

	buf[position++] = SEQ_MIDIPUTC;
	buf[position++] = 0xF0;
	buf[position++] = _device_num;
	buf[position++] = 0;
	for (; length; --length, ++chr) {
		buf[position++] = SEQ_MIDIPUTC;
		buf[position++] = *chr & 0x7F;
		buf[position++] = _device_num;
		buf[position++] = 0;
	}
	buf[position++] = SEQ_MIDIPUTC;
	buf[position++] = 0xF7;
	buf[position++] = _device_num;
	buf[position++] = 0;

	timidity_write_data(buf, position);
}

MidiHandler_timidity Midi_timidity;

#endif /*C_SDL_NET*/

