#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include <pthread.h>

#include "AudioAsset.hpp"
#include "AudioPlaybackRequest.hpp"
#include "IAudioBackend.hpp"

namespace helengine::psvita {
    /// <summary>
    /// Streams shared Helengine PCM audio through the PS Vita main audio output port.
    /// </summary>
    class PsVitaAudioBackend final : public ::IAudioBackend {
    public:
        /// <summary>
        /// Opens the PS Vita main audio output port and starts one dedicated output thread.
        /// </summary>
        PsVitaAudioBackend();

        /// <summary>
        /// Stops playback, joins the output thread, and releases the PS Vita audio port.
        /// </summary>
        ~PsVitaAudioBackend();

        int32_t Play(::AudioAsset* asset, ::AudioPlaybackRequest* request) override;

        void Stop(int32_t voiceId) override;

        void SetBusGain(std::string busId, float gain) override;

        void SetBusPaused(std::string busId, bool paused) override;

        bool IsPlaying(int32_t voiceId) override;

        void Update() override;

    private:
        static constexpr int OutputSampleRate = 48000;
        static constexpr int OutputChannelCount = 2;
        static constexpr int BufferFrameCount = 1024;
        static constexpr int HardwareMaxVolume = 32768;

        /// <summary>
        /// Stores one active voice routed through the PS Vita output thread.
        /// </summary>
        struct ActiveVoiceState {
            int32_t VoiceId;
            const std::int16_t* SourceSamples;
            int32_t SourceFrameCount;
            int32_t SourceSampleRate;
            int32_t SourceChannelCount;
            std::string BusId;
            float BaseGain;
            bool Loop;
            bool Playing;
            std::uint64_t SourceCursorQ16;
        };

        static void* AudioThreadEntry(void* argument);

        void RunAudioThread();

        int FillOutputBufferLocked();

        float ResolveCombinedGainLocked(const std::string& busId, float baseGain) const;

        bool IsBusPausedLocked(const std::string& busId) const;

        static std::string NormalizeBusId(std::string busId);

        static float ClampGain(float gain);

        static int ConvertGainToVolume(float gain);

        static bool UsesPcmEncoding(const std::string& encodingFamilyId);

        static void ResetAudioTrace();

        static void AppendAudioTrace(const std::string& message);

        std::atomic<bool> Running;
        int PortId;
        bool ThreadStarted;
        pthread_t ThreadHandle;
        int32_t NextVoiceId;
        bool HasActiveVoice;
        bool HasLoggedActiveMix;
        ActiveVoiceState ActiveVoice;
        std::unordered_map<std::string, float> BusGainsById;
        std::unordered_set<std::string> PausedBusIds;
        std::array<std::int16_t, BufferFrameCount * OutputChannelCount> OutputBuffer;
        mutable std::mutex StateMutex;
    };
}
