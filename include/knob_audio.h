#pragma once
#include "knob_samples.h"

enum class KnobSound : uint8_t { TURN, PRESS, RELEASE };

#ifdef __EMSCRIPTEN__
extern "C" void host_knob_pcm(const int8_t*, int, int, float, int, int);
#endif

inline void playKnobSound(KnobSound kind) {
  if (volumeLevel == VolumeLevel::VOL_MUTE) return;
  const bool turn = kind == KnobSound::TURN;
  const int8_t* pcm = turn ? KNOB_TURN_PCM : KNOB_PRESS_PCM;
  const uint32_t count = turn ? sizeof(KNOB_TURN_PCM) : sizeof(KNOB_PRESS_PCM);
  const int level = volumeLevel == VolumeLevel::VOL_LOW ? 64
                  : volumeLevel == VolumeLevel::VOL_MED ? 128 : 230;
#ifdef __EMSCRIPTEN__
  host_knob_pcm(pcm, count, KNOB_SAMPLE_RATE,
                turn ? KNOB_TURN_DEMO_GAIN : KNOB_PRESS_DEMO_GAIN, level, (int)kind);
#else
  // Use the existing buzzer pin/channel as a PWM DAC. No wiring changes.
  // Playback is bounded to 27/45ms, like the existing blocking feedback tones.
  const uint32_t previousFrequency = ledcReadFreq(BUZZER_CHANNEL);
  const uint32_t previousDuty = ledcRead(BUZZER_CHANNEL);
  if (ledcSetup(BUZZER_CHANNEL, 62500, 8) > 0) {
    const uint32_t started = micros();
    for (uint32_t i = 0; i < count; i++) {
      const int sample = (int8_t)pgm_read_byte(pcm + i);
      ledcWrite(BUZZER_CHANNEL, 128 + sample * level / 255);
      const uint32_t deadline = (i + 1) * 1000000UL / KNOB_SAMPLE_RATE;
      while ((uint32_t)(micros() - started) < deadline) {}
    }
  }
  ledcWriteTone(BUZZER_CHANNEL, previousFrequency);
  if (previousFrequency) ledcWrite(BUZZER_CHANNEL, previousDuty);
#endif
}
