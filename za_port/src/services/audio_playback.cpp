#include "audio/audio_playback.hpp"
#include "audio/ambient_stream.hpp"
#include <iostream>
#include <vector>
#include <cmath>
#include <thread>
#include <mutex>
#include <algorithm>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <mmsystem.h>

namespace za::audio {

struct ActiveVoice {
    std::vector<int16_t> samples;
    size_t cursor{0};
    float volume{1.0f};
    bool active{false};
};

struct AudioPlayback::Impl {
    HWAVEOUT h_wave_out{nullptr};
    uint32_t sample_rate{48000};
    uint16_t channels{2};

    static constexpr size_t BUFFER_COUNT = 3;
    static constexpr size_t SAMPLES_PER_BUFFER = 2048; // ~42.6 ms de latência

    WAVEHDR headers[BUFFER_COUNT]{};
    std::vector<int16_t> buffers[BUFFER_COUNT];

    std::thread audio_thread;
    std::atomic<bool> thread_running{false};
    std::mutex voice_mutex;
    std::vector<ActiveVoice> voices;
    std::shared_ptr<AmbientAudioStream> ambient_stream;

    HANDLE buffer_event{nullptr};
};

AudioPlayback& AudioPlayback::Instance() {
    static AudioPlayback instance;
    return instance;
}

AudioPlayback::AudioPlayback() : impl_(std::make_unique<Impl>()) {}
AudioPlayback::~AudioPlayback() { Shutdown(); }

bool AudioPlayback::Initialize(uint32_t sample_rate, uint16_t channels) {
    if (active_) return true;

    impl_->sample_rate = sample_rate;
    impl_->channels = channels;
    impl_->voices.resize(16); // Até 16 vozes simultâneas

    impl_->buffer_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!impl_->buffer_event) return false;

    WAVEFORMATEX wfx{};
    wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nChannels = channels;
    wfx.nSamplesPerSec = sample_rate;
    wfx.wBitsPerSample = 16;
    wfx.nBlockAlign = (wfx.nChannels * wfx.wBitsPerSample) / 8;
    wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;
    wfx.cbSize = 0;

    MMRESULT res = waveOutOpen(
        &impl_->h_wave_out,
        WAVE_MAPPER,
        &wfx,
        (DWORD_PTR)impl_->buffer_event,
        0,
        CALLBACK_EVENT
    );

    if (res != MMSYSERR_NOERROR) {
        std::cout << "[AudioPlayback] Aviso: Dispositivo de som nao disponivel (MMRESULT=" << res << ")" << std::endl;
        CloseHandle(impl_->buffer_event);
        impl_->buffer_event = nullptr;
        return false;
    }

    for (size_t i = 0; i < Impl::BUFFER_COUNT; ++i) {
        impl_->buffers[i].resize(Impl::SAMPLES_PER_BUFFER * channels, 0);
        impl_->headers[i].lpData = reinterpret_cast<LPSTR>(impl_->buffers[i].data());
        impl_->headers[i].dwBufferLength = static_cast<DWORD>(impl_->buffers[i].size() * sizeof(int16_t));
        impl_->headers[i].dwFlags = 0;

        waveOutPrepareHeader(impl_->h_wave_out, &impl_->headers[i], sizeof(WAVEHDR));
        impl_->headers[i].dwFlags |= WHDR_DONE; // Inicialmente livres
    }

    impl_->thread_running = true;
    impl_->audio_thread = std::thread([this]() {
        while (impl_->thread_running) {
            WaitForSingleObject(impl_->buffer_event, 50);

            for (size_t b = 0; b < Impl::BUFFER_COUNT; ++b) {
                if (impl_->headers[b].dwFlags & WHDR_DONE) {
                    // Mixagem das vozes ativas
                    auto& buf = impl_->buffers[b];
                    std::fill(buf.begin(), buf.end(), 0);

                    {
                        std::lock_guard<std::mutex> lock(impl_->voice_mutex);
                        float master = master_volume_.load();

                        for (auto& voice : impl_->voices) {
                            if (!voice.active) continue;

                            size_t frames_to_mix = Impl::SAMPLES_PER_BUFFER;
                            size_t voice_frames_left = (voice.samples.size() - voice.cursor) / impl_->channels;
                            size_t mix_frames = std::min(frames_to_mix, voice_frames_left);

                            float vol = voice.volume * master;

                            for (size_t f = 0; f < mix_frames; ++f) {
                                for (size_t c = 0; c < impl_->channels; ++c) {
                                    size_t dst_idx = f * impl_->channels + c;
                                    size_t src_idx = voice.cursor + f * impl_->channels + c;

                                    int32_t mixed = buf[dst_idx] + static_cast<int32_t>(voice.samples[src_idx] * vol);
                                    buf[dst_idx] = static_cast<int16_t>(std::clamp(mixed, -32768, 32767));
                                }
                            }

                            voice.cursor += mix_frames * impl_->channels;
                            if (voice.cursor >= voice.samples.size()) {
                                voice.active = false;
                            }
                        }

                        // Mixagem contínua da faixa de ambiência / BGM em loop
                        if (impl_->ambient_stream && impl_->ambient_stream->IsValid()) {
                            std::vector<int16_t> amb_pcm;
                            if (impl_->ambient_stream->GetNextBuffer(amb_pcm, Impl::SAMPLES_PER_BUFFER, true)) {
                                float amb_vol = impl_->ambient_stream->GetVolume() * master;
                                size_t amb_count = std::min(amb_pcm.size(), buf.size());
                                for (size_t s = 0; s < amb_count; ++s) {
                                    int32_t mixed = buf[s] + static_cast<int32_t>(amb_pcm[s] * amb_vol);
                                    buf[s] = static_cast<int16_t>(std::clamp(mixed, -32768, 32767));
                                }
                            }
                        }
                    }

                    // Envia buffer para a placa de som
                    impl_->headers[b].dwFlags &= ~WHDR_DONE;
                    waveOutWrite(impl_->h_wave_out, &impl_->headers[b], sizeof(WAVEHDR));
                }
            }
        }
    });

    active_ = true;
    std::cout << "[AudioPlayback] Dispositivo de som Win32 (waveOut) inicializado com sucesso (" 
              << sample_rate << " Hz, " << channels << " canais)!" << std::endl;
    return true;
}

void AudioPlayback::Shutdown() {
    if (!active_) return;
    active_ = false;

    if (impl_->thread_running) {
        impl_->thread_running = false;
        SetEvent(impl_->buffer_event);
        if (impl_->audio_thread.joinable()) {
            impl_->audio_thread.join();
        }
    }

    if (impl_->h_wave_out) {
        waveOutReset(impl_->h_wave_out);
        for (size_t i = 0; i < Impl::BUFFER_COUNT; ++i) {
            waveOutUnprepareHeader(impl_->h_wave_out, &impl_->headers[i], sizeof(WAVEHDR));
        }
        waveOutClose(impl_->h_wave_out);
        impl_->h_wave_out = nullptr;
    }

    if (impl_->buffer_event) {
        CloseHandle(impl_->buffer_event);
        impl_->buffer_event = nullptr;
    }
}

void AudioPlayback::PlayPcm(const int16_t* pcm_data, size_t sample_count, float volume) {
    if (!active_ || !pcm_data || sample_count == 0) return;

    std::lock_guard<std::mutex> lock(impl_->voice_mutex);
    for (auto& voice : impl_->voices) {
        if (!voice.active) {
            voice.samples.assign(pcm_data, pcm_data + sample_count);
            voice.cursor = 0;
            voice.volume = volume;
            voice.active = true;
            SetEvent(impl_->buffer_event);
            return;
        }
    }
}

void AudioPlayback::PlayPokeChime(float freq, float duration_seconds, float volume) {
    if (!active_) return;

    size_t total_frames = static_cast<size_t>(impl_->sample_rate * duration_seconds);
    std::vector<int16_t> pcm(total_frames * impl_->channels);

    // Gera um acorde cintilante no estilo Pokémon (fundamental + 2ª harmônica + oitava) com envelope suave
    constexpr float kPi2 = 6.28318530718f;
    for (size_t i = 0; i < total_frames; ++i) {
        float t = static_cast<float>(i) / impl_->sample_rate;
        float progress = t / duration_seconds;

        // Envelope exponencial com ataque rápido
        float env = (1.0f - std::exp(-progress * 25.0f)) * std::exp(-progress * 4.5f);

        // Som de sino com harmônicos
        float wave = 0.60f * std::sin(kPi2 * freq * t) +
                     0.25f * std::sin(kPi2 * (freq * 1.5f) * t) +
                     0.15f * std::sin(kPi2 * (freq * 2.0f) * t);

        int16_t val = static_cast<int16_t>(std::clamp(wave * env * 24000.0f, -32767.0f, 32767.0f));

        for (size_t c = 0; c < impl_->channels; ++c) {
            pcm[i * impl_->channels + c] = val;
        }
    }

    PlayPcm(pcm.data(), pcm.size(), volume);
}

void AudioPlayback::PlayLockonBeep(float freq, float duration_seconds, float volume) {
    if (!active_) return;

    size_t total_frames = static_cast<size_t>(impl_->sample_rate * duration_seconds);
    std::vector<int16_t> pcm(total_frames * impl_->channels);

    constexpr float kPi2 = 6.28318530718f;
    for (size_t i = 0; i < total_frames; ++i) {
        float t = static_cast<float>(i) / impl_->sample_rate;
        float env = std::exp(-(t / duration_seconds) * 6.0f);
        float wave = std::sin(kPi2 * freq * t);
        int16_t val = static_cast<int16_t>(std::clamp(wave * env * 20000.0f, -32767.0f, 32767.0f));

        for (size_t c = 0; c < impl_->channels; ++c) {
            pcm[i * impl_->channels + c] = val;
        }
    }

    PlayPcm(pcm.data(), pcm.size(), volume);
}

void AudioPlayback::PlayAttackImpact(float power, float volume) {
    if (!active_) return;

    float duration_seconds = 0.42f;
    size_t total_frames = static_cast<size_t>(impl_->sample_rate * duration_seconds);
    std::vector<int16_t> pcm(total_frames * impl_->channels);

    constexpr float kPi2 = 6.28318530718f;
    float phase = 0.0f;
    for (size_t i = 0; i < total_frames; ++i) {
        float t = static_cast<float>(i) / impl_->sample_rate;
        float progress = t / duration_seconds;

        // Frequência decrescente com o impacto (sweep de 320 Hz até 65 Hz)
        float cur_freq = 65.0f + 255.0f * std::exp(-progress * 9.0f) * power;
        phase += (kPi2 * cur_freq) / impl_->sample_rate;
        if (phase > kPi2) phase -= kPi2;

        // Envelope: soco forte inicial + cauda ressonante
        float punch_env = std::exp(-progress * 14.0f);
        float body_env  = (1.0f - std::exp(-progress * 40.0f)) * std::exp(-progress * 4.0f);

        // Síntese: onda senoidal fundamental + harmônico de distorção + subgrave
        float fundamental = std::sin(phase);
        float sub = std::sin(phase * 0.5f);
        float transient = (i < 300) ? ((static_cast<float>(std::rand()) / RAND_MAX - 0.5f) * punch_env * 0.5f) : 0.0f;

        float sample_val = (fundamental * 0.65f + sub * 0.35f) * body_env + transient;
        int16_t val = static_cast<int16_t>(std::clamp(sample_val * 27000.0f, -32767.0f, 32767.0f));

        for (size_t c = 0; c < impl_->channels; ++c) {
            pcm[i * impl_->channels + c] = val;
        }
    }

    PlayPcm(pcm.data(), pcm.size(), volume);
}

void AudioPlayback::SetMasterVolume(float volume) {
    master_volume_ = std::clamp(volume, 0.0f, 1.0f);
}

float AudioPlayback::GetMasterVolume() const {
    return master_volume_.load();
}

void AudioPlayback::SetAmbientStream(std::shared_ptr<AmbientAudioStream> stream) {
    std::lock_guard<std::mutex> lock(impl_->voice_mutex);
    impl_->ambient_stream = stream;
    if (active_) {
        SetEvent(impl_->buffer_event);
    }
}

std::shared_ptr<AmbientAudioStream> AudioPlayback::GetAmbientStream() const {
    std::lock_guard<std::mutex> lock(impl_->voice_mutex);
    return impl_->ambient_stream;
}

void AudioPlayback::StopAmbientStream() {
    std::lock_guard<std::mutex> lock(impl_->voice_mutex);
    impl_->ambient_stream = nullptr;
}

} // namespace za::audio
