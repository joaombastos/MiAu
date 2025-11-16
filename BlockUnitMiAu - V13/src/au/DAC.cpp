#include "DAC.h"
#include "pinos.h"
#include "driver/i2s.h"

void dac_init(void) {
    // Configuração I2S para DAC UDA1334A
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
        .sample_rate = DAC_SAMPLE_RATE,
        .bits_per_sample = DAC_BITS_PER_SAMPLE,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 6,
        .dma_buf_len = DAC_BUFFER_SIZE,
        .use_apll = false,
        .tx_desc_auto_clear = true,
        .fixed_mclk = 0
    };

    i2s_pin_config_t pin_config = {
        .bck_io_num = UDA1334A_BCLK,
        .ws_io_num = UDA1334A_WSEL,
        .data_out_num = UDA1334A_DIN,
        .data_in_num = I2S_PIN_NO_CHANGE
    };

    // Instala e inicia I2S
    i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
    i2s_set_pin(I2S_NUM_0, &pin_config);
    i2s_zero_dma_buffer(I2S_NUM_0);
}

void dac_play_buffer(const void* buffer, size_t bytes) {
    size_t bytes_written;
    i2s_write(I2S_NUM_0, buffer, bytes, &bytes_written, portMAX_DELAY);
}
