//
// Created by root on 4/16/21.
//

#include "RetroWave_DOSBoX.hpp"

RetroWaveContext retrowave_global_context;
int retrowave_global_context_inited = 0;

static void retrowave_iocb_empty(void *userp, uint32_t data_rate, const void *tx_buf, void *rx_buf, uint32_t len) {

}

static std::vector<std::string> string_split(const std::string &s, char delim) {
	std::vector<std::string> result;
	std::stringstream ss (s);
	std::string item;

	while (getline(ss, item, delim)) {
		result.push_back(item);
	}

	return result;
}

void retrowave_init_dosbox(const std::string& bus, const std::string& path, const std::string& spi_cs) {
	if (retrowave_global_context_inited)
		return;

	int rc = -1;

	if (bus == "serial") {
#if defined (__linux__) || defined (__unix__) || (defined (__APPLE__) && defined (__MACH__))
		char buf[128];
		snprintf(buf, sizeof(buf)-1, "/dev/%s", path.c_str());

		rc = retrowave_init_posix_serialport(&retrowave_global_context, buf);
#endif

#ifdef _WIN32
		rc = retrowave_init_win32_serialport(&retrowave_global_context, path.c_str());
#endif
	} else if (bus == "spi") {
#if defined (__linux__)
		auto scgs = string_split(spi_cs, ',');
		int scg[2] = {0};

		if (scgs.size() != 2) {
			puts("error: bad GPIO specification. Please use the `gpiochip,line' format.");
			exit(2);
		}

		scg[0] = strtol(scgs[0].c_str(), nullptr, 10);
		scg[1] = strtol(scgs[1].c_str(), nullptr, 10);

		printf("SPI CS: chip=%d, line=%d\n", scg[0], scg[1]);

		rc = retrowave_init_linux_spi(&retrowave_global_context, path.c_str(), scg[0], scg[1]);
#else
		DEBUG_ShowMsg("RetroWave: error: SPI is not supported on your platform!");
#endif
	}

	if (rc < 0) {
		DEBUG_ShowMsg("RetroWave: Failed to init board! Please change configuration!");
		retrowave_init(&retrowave_global_context);
		retrowave_global_context.callback_io = retrowave_iocb_empty;
	}

	retrowave_io_init(&retrowave_global_context);

	retrowave_global_context_inited = true;
}