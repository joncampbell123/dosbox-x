/*
    This file is part of RetroWave.

    Copyright (C) 2021 ReimuNotMoe <reimu@sudomaker.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include "STM32_HAL_SPI.h"

extern void HAL_GPIO_WritePin(void *GPIOx, uint16_t GPIO_Pin, int PinState);
extern int HAL_SPI_TransmitReceive(void *hspi, uint8_t *pTxData, uint8_t *pRxData, uint16_t Size, uint32_t Timeout);

static void io_callback(void *userp, uint32_t data_rate, const void *tx_buf, void *rx_buf, uint32_t len) {
	RetroWavePlatform_STM32_HAL_SPI *ctx = userp;

	HAL_GPIO_WritePin(ctx->cs_gpiox, ctx->cs_gpio_pin, 0);

	HAL_SPI_TransmitReceive(ctx->hspi, (uint8_t *)tx_buf, rx_buf, len, 0x7ffffff);

	HAL_GPIO_WritePin(ctx->cs_gpiox, ctx->cs_gpio_pin, 1);
}

int retrowave_init_stm32_hal_spi(RetroWaveContext *ctx, void *hspi, void *cs_gpiox, uint16_t cs_gpio_pin) {
	retrowave_init(ctx);

	ctx->user_data = malloc(sizeof(RetroWavePlatform_STM32_HAL_SPI));

	RetroWavePlatform_STM32_HAL_SPI *pctx = ctx->user_data;

	pctx->hspi = hspi;
	pctx->cs_gpiox = cs_gpiox;
	pctx->cs_gpio_pin = cs_gpio_pin;

	ctx->callback_io = io_callback;

	HAL_GPIO_WritePin(cs_gpiox, cs_gpio_pin, 1);

	return 0;
}

void retrowave_deinit_stm32_hal_spi(RetroWaveContext *ctx) {
	free(ctx->user_data);
}