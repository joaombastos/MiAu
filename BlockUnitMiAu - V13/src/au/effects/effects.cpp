#include "effects/effects.h"
#include "esp_heap_caps.h"

#ifndef FX_DELAY_MS
#define FX_DELAY_MS 500
#endif
#ifndef FX_DELAY_FB
#define FX_DELAY_FB 0.60f
#endif

static bool s_fxInit = false;
static uint32_t s_sr = 44100;

// Delay mono simples aplicado a L/R iguais
static int16_t* s_delayBuf = nullptr;
static uint32_t s_delayLen = 0;
static uint32_t s_delayIdx = 0;
static float s_delayMix = 0.0f;   // 0..1
static bool s_delayEnabled = false;
static float s_delayFb = FX_DELAY_FB;

// Reverb tipo "spring" leve: 3 allpass em série + pequeno tank
static bool s_reverbEnabled = false;
static float s_reverbMix = 0.0f; // 0..1
static int16_t* s_revAp1 = nullptr;
static int16_t* s_revAp2 = nullptr;
static int16_t* s_revAp3 = nullptr;
static int16_t* s_revTank = nullptr;
static uint32_t s_revLenAp1 = 0, s_revLenAp2 = 0, s_revLenAp3 = 0, s_revLenTank = 0;
static uint32_t s_revIdxAp1 = 0, s_revIdxAp2 = 0, s_revIdxAp3 = 0, s_revIdxTank = 0;
static float s_revGAp1 = 0.7f, s_revGAp2 = 0.7f, s_revGAp3 = 0.7f; // ganhos allpass (mais difusão)
static float s_revTankFb = 0.80f; // feedback do tank (cauda bem mais longa)
static float s_revDamp = 0.75f; // 0..1 (maior = mais escuro)
static float s_revDampState = 0.0f;
static int s_revStereoOffset = 113; // descorelação entre L/R (ajustado após alocar tank)
// DC blocker simples para saída do tank
static float s_revDcPrevInL = 0.0f, s_revDcPrevOutL = 0.0f;
static float s_revDcPrevInR = 0.0f, s_revDcPrevOutR = 0.0f;
static const float kDcR = 0.995f; // coeficiente (próximo de 1)
// Low-pass suave no wet para reduzir hiss
static float s_revWetLpL = 0.0f, s_revWetLpR = 0.0f;
static const float kWetLpCoef = 0.15f;

// Chorus em paralelo
static bool s_chEnabled = false;
static float s_chMix = 0.0f; // 0..1
static int16_t* s_chBuf = nullptr;
static uint32_t s_chLen = 0;
static uint32_t s_chWriteIdx = 0;
static float s_chPhase = 0.0f; // 0..1
static float s_chRateHz = 0.8f;
static float s_chDelayMs = 12.0f;
static float s_chDepthMs = 6.0f;

// Flanger em paralelo
static bool s_flEnabled = false;
static float s_flMix = 0.0f; // 0..1
static int16_t* s_flBuf = nullptr;
static uint32_t s_flLen = 0;
static uint32_t s_flWriteIdx = 0;
static float s_flPhase = 0.0f;
static float s_flRateHz = 0.35f;
static float s_flDelayMs = 3.0f;
static float s_flDepthMs = 2.0f;
static float s_flFeedback = 0.55f;

void effects_begin(uint32_t sample_rate_hz) {
	if (s_fxInit) return;
	s_sr = sample_rate_hz;
	s_delayLen = (uint32_t)((s_sr * FX_DELAY_MS) / 1000);
	if (s_delayLen < 8) s_delayLen = 8;
	// Tenta alocar; se falhar, reduz tamanho até conseguir
	uint32_t tryLen = s_delayLen;
	while (tryLen >= 1024 && s_delayBuf == nullptr) {
		s_delayBuf = (int16_t*)heap_caps_malloc(tryLen * sizeof(int16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
		if (!s_delayBuf) s_delayBuf = (int16_t*)malloc(tryLen * sizeof(int16_t));
		if (!s_delayBuf) {
			tryLen /= 2;
		} else {
			s_delayLen = tryLen;
		}
	}
	if (s_delayBuf) {
		for (uint32_t i = 0; i < s_delayLen; ++i) s_delayBuf[i] = 0;
		s_delayIdx = 0;
	}
	s_delayMix = 0.0f;
	s_delayEnabled = false;
	// Reverb: tamanhos (spring-like): ~50ms, ~70ms, ~90ms, tank ~200ms
	uint32_t ap1 = (uint32_t)(s_sr * 0.050f);
	uint32_t ap2 = (uint32_t)(s_sr * 0.070f);
	uint32_t ap3 = (uint32_t)(s_sr * 0.090f);
	uint32_t tk  = (uint32_t)(s_sr * 0.200f);
	if (ap1 < 32) ap1 = 32; if (ap2 < 32) ap2 = 32; if (ap3 < 32) ap3 = 32; if (tk < 64) tk = 64;
	s_revAp1 = (int16_t*)heap_caps_malloc(ap1 * sizeof(int16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
	if (!s_revAp1) s_revAp1 = (int16_t*)malloc(ap1 * sizeof(int16_t));
	s_revAp2 = (int16_t*)heap_caps_malloc(ap2 * sizeof(int16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
	if (!s_revAp2) s_revAp2 = (int16_t*)malloc(ap2 * sizeof(int16_t));
	s_revAp3 = (int16_t*)heap_caps_malloc(ap3 * sizeof(int16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
	if (!s_revAp3) s_revAp3 = (int16_t*)malloc(ap3 * sizeof(int16_t));
	s_revTank = (int16_t*)heap_caps_malloc(tk * sizeof(int16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
	if (!s_revTank) s_revTank = (int16_t*)malloc(tk * sizeof(int16_t));
	s_revLenAp1 = s_revAp1 ? ap1 : 0;
	s_revLenAp2 = s_revAp2 ? ap2 : 0;
	s_revLenAp3 = s_revAp3 ? ap3 : 0;
	s_revLenTank = s_revTank ? tk : 0;
	if (s_revAp1) for (uint32_t i=0;i<s_revLenAp1;++i) s_revAp1[i]=0;
	if (s_revAp2) for (uint32_t i=0;i<s_revLenAp2;++i) s_revAp2[i]=0;
	if (s_revAp3) for (uint32_t i=0;i<s_revLenAp3;++i) s_revAp3[i]=0;
	if (s_revTank) for (uint32_t i=0;i<s_revLenTank;++i) s_revTank[i]=0;
	s_revIdxAp1 = s_revIdxAp2 = s_revIdxAp3 = s_revIdxTank = 0;
	s_reverbEnabled = false;
	s_reverbMix = 0.0f;
	s_revDampState = 0.0f;
	// Ajusta offset estéreo (usar valor menor que tank len e primo)
	if (s_revLenTank > 0) {
		int cand = 113;
		if ((uint32_t)cand >= s_revLenTank) cand = (int)(s_revLenTank / 3);
		if (cand < 7) cand = 7;
		s_revStereoOffset = cand;
	}
	// Chorus: padrão desligado
	s_chEnabled = false;
	s_chMix = 0.0f;
	s_chRateHz = 0.8f;
	s_chDelayMs = 12.0f;
	s_chDepthMs = 6.0f;
	// Chorus buffer (~30ms max)
	uint32_t chMaxMs = 30;
	uint32_t chLen = (uint32_t)((s_sr * chMaxMs) / 1000);
	if (chLen < 64) chLen = 64;
	s_chBuf = (int16_t*)heap_caps_malloc(chLen * sizeof(int16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
	if (!s_chBuf) s_chBuf = (int16_t*)malloc(chLen * sizeof(int16_t));
	s_chLen = s_chBuf ? chLen : 0;
	if (s_chBuf) for (uint32_t i=0;i<s_chLen;++i) s_chBuf[i] = 0;
	s_chWriteIdx = 0;
	// Flanger: padrão desligado
	s_flEnabled = false;
	s_flMix = 0.0f;
	s_flRateHz = 0.35f;
	s_flDelayMs = 3.0f;
	s_flDepthMs = 2.0f;
	s_flFeedback = 0.55f;
	// Flanger buffer (~15ms max)
	uint32_t flMaxMs = 15;
	uint32_t flLen = (uint32_t)((s_sr * flMaxMs) / 1000);
	if (flLen < 64) flLen = 64;
	s_flBuf = (int16_t*)heap_caps_malloc(flLen * sizeof(int16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
	if (!s_flBuf) s_flBuf = (int16_t*)malloc(flLen * sizeof(int16_t));
	s_flLen = s_flBuf ? flLen : 0;
	if (s_flBuf) for (uint32_t i=0;i<s_flLen;++i) s_flBuf[i] = 0;
	s_flWriteIdx = 0;
	s_fxInit = true;
	// Intensificar chorus
	s_chRateHz = 1.2f;
	s_chDepthMs = 10.0f;
}

void effects_enable_delay(bool enabled) {
	s_delayEnabled = enabled;
}

void effects_set_delay_mix(float mix) {
	if (mix < 0.0f) mix = 0.0f; else if (mix > 1.0f) mix = 1.0f;
	s_delayMix = mix;
}

void effects_set_delay_time_ms(uint32_t time_ms) {
	if (!s_fxInit || time_ms == 0) return;
	uint32_t newLen = (uint32_t)((s_sr * time_ms) / 1000);
	if (newLen < 8) newLen = 8;
	if (newLen == s_delayLen) return;
	// Reusar buffer se novo tamanho couber no atual; caso contrário realocar
	if (newLen <= s_delayLen && s_delayBuf) {
		// Só ajusta índice/len e limpa ponta
		for (uint32_t i = newLen; i < s_delayLen; ++i) s_delayBuf[i] = 0;
		s_delayLen = newLen;
		if (s_delayIdx >= s_delayLen) s_delayIdx = 0;
		return;
	}
	// Realocar (pode fragmentar; manter simples)
	int16_t* nb = (int16_t*)heap_caps_malloc(newLen * sizeof(int16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
	if (!nb) nb = (int16_t*)malloc(newLen * sizeof(int16_t));
	if (!nb) return;
	for (uint32_t i = 0; i < newLen; ++i) nb[i] = 0;
	if (s_delayBuf) free(s_delayBuf);
	s_delayBuf = nb;
	s_delayLen = newLen;
	s_delayIdx = 0;
}

void effects_set_delay_feedback(float feedback) {
	if (feedback < 0.0f) feedback = 0.0f; else if (feedback > 0.95f) feedback = 0.95f;
	s_delayFb = feedback;
}

void effects_enable_reverb(bool enabled) {
	s_reverbEnabled = enabled;
}

void effects_set_reverb_mix(float mix) {
	if (mix < 0.0f) mix = 0.0f; else if (mix > 1.0f) mix = 1.0f;
	s_reverbMix = mix;
}

static inline int16_t clamp16(int32_t v) {
	if (v > 32767) return 32767; if (v < -32768) return -32768; return (int16_t)v;
}

void effects_enable_chorus(bool enabled) {
	s_chEnabled = enabled;
}

void effects_set_chorus_mix(float mix) {
	if (mix < 0.0f) mix = 0.0f; else if (mix > 1.0f) mix = 1.0f;
	s_chMix = mix;
}

void effects_set_chorus_rate_hz(float rate_hz) {
	if (rate_hz < 0.1f) rate_hz = 0.1f; if (rate_hz > 20.0f) rate_hz = 20.0f;
	s_chRateHz = rate_hz;
}

void effects_set_chorus_delay_ms(float delay_ms) {
	if (delay_ms < 0.1f) delay_ms = 0.1f; if (delay_ms > 100.0f) delay_ms = 100.0f;
	s_chDelayMs = delay_ms;
}

void effects_set_chorus_depth_ms(float depth_ms) {
	if (depth_ms < 0.1f) depth_ms = 0.1f; if (depth_ms > 100.0f) depth_ms = 100.0f;
	s_chDepthMs = depth_ms;
}

void effects_enable_flanger(bool enabled) {
	s_flEnabled = enabled;
}

void effects_set_flanger_mix(float mix) {
	if (mix < 0.0f) mix = 0.0f; else if (mix > 1.0f) mix = 1.0f;
	s_flMix = mix;
}

void effects_set_flanger_rate_hz(float rate_hz) {
	if (rate_hz < 0.1f) rate_hz = 0.1f; if (rate_hz > 20.0f) rate_hz = 20.0f;
	s_flRateHz = rate_hz;
}

void effects_set_flanger_delay_ms(float delay_ms) {
	if (delay_ms < 0.1f) delay_ms = 0.1f; if (delay_ms > 100.0f) delay_ms = 100.0f;
	s_flDelayMs = delay_ms;
}

void effects_set_flanger_depth_ms(float depth_ms) {
	if (depth_ms < 0.1f) depth_ms = 0.1f; if (depth_ms > 100.0f) depth_ms = 100.0f;
	s_flDepthMs = depth_ms;
}

void effects_set_flanger_feedback(float feedback) {
	if (feedback < 0.0f) feedback = 0.0f; else if (feedback > 0.95f) feedback = 0.95f;
	s_flFeedback = feedback;
}

void effects_process_block(int16_t* interleaved_lr, int frames) {
	if (!s_fxInit || interleaved_lr == nullptr || frames <= 0) return;
	// Parallel processing: compute wet signals from the original dry and sum
	const bool doDelay = (s_delayEnabled && s_delayMix > 0.0f && s_delayBuf && s_delayLen > 0);
	const bool doReverb = (s_reverbEnabled && s_reverbMix > 0.0f && s_revAp1 && s_revAp2 && s_revAp3 && s_revTank &&
				   s_revLenAp1 && s_revLenAp2 && s_revLenAp3 && s_revLenTank);
	const bool doChorus = (s_chEnabled && s_chMix > 0.0f && s_chBuf && s_chLen > 0);
	const bool doFlanger = (s_flEnabled && s_flMix > 0.0f && s_flBuf && s_flLen > 0);
	if (!doDelay && !doReverb && !doChorus && !doFlanger) return;
	const float dMix = s_delayMix;
	const float dFb = s_delayFb;
	const float rMix = s_reverbMix;
	const float g1 = s_revGAp1, g2 = s_revGAp2, g3 = s_revGAp3;
	const float rFb = s_revTankFb;
	const float chMix = s_chMix;
	const float chRate = s_chRateHz;
	const float chDelayMs = s_chDelayMs;
	const float chDepthMs = s_chDepthMs;
	const float flMix = s_flMix;
	const float flRate = s_flRateHz;
	const float flDelayMs = s_flDelayMs;
	const float flDepthMs = s_flDepthMs;
	const float flFeedback = s_flFeedback;
	for (int i = 0; i < frames; ++i) {
		int idx = i * 2;
		int32_t dry = interleaved_lr[idx];
		int32_t wetSumL = 0;
		int32_t wetSumR = 0;
		// Delay (from dry)
		if (doDelay) {
			int16_t delayed = s_delayBuf[s_delayIdx];
			int32_t wetD = (int32_t)((float)delayed * dMix);
			int32_t fbWrite = (int32_t)((float)dry * 0.5f) + (int32_t)((float)delayed * dFb);
			s_delayBuf[s_delayIdx] = clamp16(fbWrite);
			s_delayIdx++; if (s_delayIdx >= s_delayLen) s_delayIdx = 0;
			wetSumL += wetD;
			wetSumR += wetD;
		}
		// Reverb (from dry)
		if (doReverb) {
			float x = (float)dry;
			// Allpass 1
			float ap1buf = (float)s_revAp1[s_revIdxAp1];
			float ap1y = -g1 * x + ap1buf;
			float ap1new = x + g1 * ap1y;
			s_revAp1[s_revIdxAp1] = clamp16((int32_t)ap1new);
			s_revIdxAp1++; if (s_revIdxAp1 >= s_revLenAp1) s_revIdxAp1 = 0;
			// Allpass 2
			float ap2buf = (float)s_revAp2[s_revIdxAp2];
			float ap2y = -g2 * ap1y + ap2buf;
			float ap2new = ap1y + g2 * ap2y;
			s_revAp2[s_revIdxAp2] = clamp16((int32_t)ap2new);
			s_revIdxAp2++; if (s_revIdxAp2 >= s_revLenAp2) s_revIdxAp2 = 0;
			// Allpass 3
			float ap3buf = (float)s_revAp3[s_revIdxAp3];
			float ap3y = -g3 * ap2y + ap3buf;
			float ap3new = ap2y + g3 * ap3y;
			s_revAp3[s_revIdxAp3] = clamp16((int32_t)ap3new);
			s_revIdxAp3++; if (s_revIdxAp3 >= s_revLenAp3) s_revIdxAp3 = 0;
			// Tank (comb) com damping e estéreo por offset de leitura
			int16_t tkReadL = s_revTank[s_revIdxTank];
			int32_t idxR = (int32_t)s_revIdxTank + s_revStereoOffset; if (idxR >= (int32_t)s_revLenTank) idxR -= (int32_t)s_revLenTank;
			int16_t tkReadR = s_revTank[idxR];
			float combOutL = (float)tkReadL;
			float combOutR = (float)tkReadR;
			// Damping (simple one-pole LP) no feedback usando L como referência
			s_revDampState += s_revDamp * (combOutL - s_revDampState);
			float tankIn = ap3y + rFb * s_revDampState;
			s_revTank[s_revIdxTank] = clamp16((int32_t)tankIn);
			s_revIdxTank++; if (s_revIdxTank >= s_revLenTank) s_revIdxTank = 0;
			// Low-pass e DC blocker no wet
			s_revWetLpL += kWetLpCoef * (combOutL - s_revWetLpL);
			s_revWetLpR += kWetLpCoef * (combOutR - s_revWetLpR);
			float dcOutL = s_revWetLpL - s_revDcPrevInL + kDcR * s_revDcPrevOutL;
			float dcOutR = s_revWetLpR - s_revDcPrevInR + kDcR * s_revDcPrevOutR;
			s_revDcPrevInL = s_revWetLpL; s_revDcPrevOutL = dcOutL;
			s_revDcPrevInR = s_revWetLpR; s_revDcPrevOutR = dcOutR;
			int32_t wetL = (int32_t)(rMix * dcOutL);
			int32_t wetR = (int32_t)(rMix * dcOutR);
			wetSumL += wetL;
			wetSumR += wetR;
		}
		// Chorus (from dry)
		if (doChorus) {
			if (s_chBuf && s_chLen > 0) {
				s_chBuf[s_chWriteIdx] = (int16_t)clamp16(dry);
				float delayMs = chDelayMs + chDepthMs * sinf(2.0f * 3.14159265f * s_chPhase);
				float delaySamps = (delayMs * (float)s_sr) / 1000.0f;
				if (delaySamps < 1.0f) delaySamps = 1.0f;
				if (delaySamps > (float)(s_chLen - 2)) delaySamps = (float)(s_chLen - 2);
				float readPos = (float)s_chWriteIdx - delaySamps;
				while (readPos < 0.0f) readPos += (float)s_chLen;
				int idx0 = (int)readPos;
				int idx1 = idx0 + 1; if (idx1 >= (int)s_chLen) idx1 = 0;
				float frac = readPos - (float)idx0;
				float s0 = (float)s_chBuf[idx0];
				float s1 = (float)s_chBuf[idx1];
				float chor = s0 + (s1 - s0) * frac;
				int32_t wetC = (int32_t)(chMix * chor);
				wetSumL += wetC;
				wetSumR += wetC;
				s_chPhase += chRate / (float)s_sr; if (s_chPhase >= 1.0f) s_chPhase -= 1.0f;
				s_chWriteIdx++; if (s_chWriteIdx >= s_chLen) s_chWriteIdx = 0;
			}
		}
		// Flanger (from dry)
		if (doFlanger) {
			if (s_flBuf && s_flLen > 0) {
				// write with feedback
				int16_t prev = s_flBuf[s_flWriteIdx];
				int32_t fbIn = dry + (int32_t)((float)prev * flFeedback);
				s_flBuf[s_flWriteIdx] = (int16_t)clamp16(fbIn);
				// modulated short delay
				float delayMs = flDelayMs + flDepthMs * sinf(2.0f * 3.14159265f * s_flPhase);
				float delaySamps = (delayMs * (float)s_sr) / 1000.0f;
				if (delaySamps < 1.0f) delaySamps = 1.0f;
				if (delaySamps > (float)(s_flLen - 2)) delaySamps = (float)(s_flLen - 2);
				float readPos = (float)s_flWriteIdx - delaySamps;
				while (readPos < 0.0f) readPos += (float)s_flLen;
				int idx0 = (int)readPos;
				int idx1 = idx0 + 1; if (idx1 >= (int)s_flLen) idx1 = 0;
				float frac = readPos - (float)idx0;
				float s0 = (float)s_flBuf[idx0];
				float s1 = (float)s_flBuf[idx1];
				float flg = s0 + (s1 - s0) * frac;
				int32_t wetF = (int32_t)(flMix * flg);
				wetSumL += wetF;
				wetSumR += wetF;
				s_flPhase += flRate / (float)s_sr; if (s_flPhase >= 1.0f) s_flPhase -= 1.0f;
				s_flWriteIdx++; if (s_flWriteIdx >= s_flLen) s_flWriteIdx = 0;
			}
		}
		int32_t outL = dry + wetSumL;
		int32_t outR = dry + wetSumR;
		interleaved_lr[idx] = clamp16(outL);
		interleaved_lr[idx + 1] = clamp16(outR);
	}
}


