#include "player_wav/player_wav.h"
#include "DAC.h"
#include "pinos.h"
#include <SD.h>
#include <SPI.h>

// Header WAV simples (PCM 16-bit, mono/estéreo)
struct __attribute__((packed)) WavHeader {
    char riff[4];
    uint32_t size;
    char wave[4];
    char fmt[4];
    uint32_t fmt_size;
    uint16_t audio_format;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
};

static bool read_until_data(File &f) {
    // Procura chunk 'data' varrendo chunks após o cabeçalho RIFF/WAVE
    while (f.available()) {
        char id[4];
        if (f.read((uint8_t*)id, 4) != 4) return false;
        uint32_t chunk_size;
        if (f.read((uint8_t*)&chunk_size, 4) != 4) return false;
        if (memcmp(id, "data", 4) == 0) {
            return true; // posição logo no início dos dados
        }
        // avança para o próximo chunk (alinhamento natural)
        f.seek(f.position() + chunk_size);
    }
    return false;
}

bool player_wav_play(const char* filepath) {
    // Inicializa SPI com pinos do AU e depois SD
    SPI.begin(SD_SCK, SD_MISO, SD_MOSI);
    if (!SD.begin(SD_CS)) {
        return false;
    }

    File f = SD.open(filepath, FILE_READ);
    if (!f) return false;

    // Lê cabeçalho RIFF
    char riff[4]; uint32_t riff_size; char wave[4];
    if (f.read((uint8_t*)riff, 4) != 4) { f.close(); return false; }
    if (f.read((uint8_t*)&riff_size, 4) != 4) { f.close(); return false; }
    if (f.read((uint8_t*)wave, 4) != 4) { f.close(); return false; }
    if (memcmp(riff, "RIFF", 4) || memcmp(wave, "WAVE", 4)) { f.close(); return false; }

    // Procura e lê chunk 'fmt '
    WavHeader fmtHdr;
    bool gotFmt = false;
    while (f.available() && !gotFmt) {
        char id[4]; uint32_t size;
        if (f.read((uint8_t*)id, 4) != 4) { f.close(); return false; }
        if (f.read((uint8_t*)&size, 4) != 4) { f.close(); return false; }
        if (memcmp(id, "fmt ", 4) == 0) {
            // lê ao menos os 16 bytes base da fmt
            if (size < 16) { f.close(); return false; }
            if (f.read((uint8_t*)&fmtHdr.audio_format, 16) != 16) { f.close(); return false; }
            // salta quaisquer bytes extra da fmt
            if (size > 16) { f.seek(f.position() + (size - 16)); }
            gotFmt = true;
            break;
        } else {
            // salta chunk desconhecido
            f.seek(f.position() + size);
        }
    }
    if (!gotFmt) { f.close(); return false; }

    if (fmtHdr.audio_format != 1 /* PCM */ || fmtHdr.bits_per_sample != 16) { f.close(); return false; }
    // Nota: mantemos DAC em 44100 Hz; se sample_rate do arquivo diferente, tocará com pitch/tempo diferente

    // Agora procura chunk 'data'
    if (!read_until_data(f)) { f.close(); return false; }

    // Buffers de E/S
    static int16_t inBuf[DAC_BUFFER_SIZE];
    static int16_t outBuf[DAC_BUFFER_SIZE];

    if (fmtHdr.num_channels == 2) {
        // Estéreo 16-bit: transmitimos diretamente
        while (f.available()) {
            int maxBytes = DAC_BUFFER_SIZE * (int)sizeof(int16_t);
            int n = f.read((uint8_t*)outBuf, maxBytes);
            if (n <= 0) break;
            dac_play_buffer(outBuf, (size_t)n);
        }
    } else if (fmtHdr.num_channels == 1) {
        // Mono 16-bit: duplicar amostras para estéreo intercalado
        while (f.available()) {
            // Ler metade do que caberia em estéreo, para depois duplicar
            int maxInSamples = DAC_BUFFER_SIZE / 2; // porque out precisa 2x
            int maxInBytes = maxInSamples * (int)sizeof(int16_t);
            int nIn = f.read((uint8_t*)inBuf, maxInBytes);
            if (nIn <= 0) break;
            int samplesRead = nIn / (int)sizeof(int16_t);
            int outIdx = 0;
            for (int i = 0; i < samplesRead && (outIdx + 1) < DAC_BUFFER_SIZE; ++i) {
                int16_t s = inBuf[i];
                outBuf[outIdx++] = s; // L
                outBuf[outIdx++] = s; // R
            }
            dac_play_buffer(outBuf, outIdx * sizeof(int16_t));
        }
    } else {
        // Outros formatos de canais não suportados
        f.close();
        return false;
    }

    f.close();
    return true;
}


