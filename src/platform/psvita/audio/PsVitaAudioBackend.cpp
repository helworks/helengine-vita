#include "platform/psvita/audio/PsVitaAudioBackend.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <stdexcept>

#include <psp2/audioout.h>

namespace helengine::psvita {
    namespace {
        constexpr const char* AudioTracePath = "ux0:/data/helengine_psvita_audio.log";
    }

    PsVitaAudioBackend::PsVitaAudioBackend()
        : Running(true)
        , PortId(-1)
        , ThreadStarted(false)
        , ThreadHandle()
        , NextVoiceId(0)
        , HasActiveVoice(false)
        , HasLoggedActiveMix(false)
        , ActiveVoice()
        , BusGainsById()
        , PausedBusIds()
        , OutputBuffer()
        , StateMutex() {
        ResetAudioTrace();
        AppendAudioTrace("ctor");

        BusGainsById.emplace("master", 1.0f);
        BusGainsById.emplace("music", 1.0f);
        BusGainsById.emplace("sfx", 1.0f);

        PortId = sceAudioOutOpenPort(
            SCE_AUDIO_OUT_PORT_TYPE_MAIN,
            BufferFrameCount,
            OutputSampleRate,
            SCE_AUDIO_OUT_MODE_STEREO);
        if (PortId < 0) {
            AppendAudioTrace(std::string("sceAudioOutOpenPort failed result=") + std::to_string(PortId));
            throw std::runtime_error("Failed to open the PS Vita main audio output port.");
        }

        int threadCreateResult = pthread_create(&ThreadHandle, nullptr, &PsVitaAudioBackend::AudioThreadEntry, this);
        if (threadCreateResult != 0) {
            sceAudioOutReleasePort(PortId);
            PortId = -1;
            AppendAudioTrace(std::string("pthread_create failed result=") + std::to_string(threadCreateResult));
            throw std::runtime_error("Failed to create the PS Vita audio output thread.");
        }

        ThreadStarted = true;
        AppendAudioTrace("thread started");
    }

    PsVitaAudioBackend::~PsVitaAudioBackend() {
        Running.store(false);

        if (ThreadStarted) {
            pthread_join(ThreadHandle, nullptr);
            ThreadStarted = false;
        }

        if (PortId >= 0) {
            sceAudioOutReleasePort(PortId);
            PortId = -1;
        }
    }

    int32_t PsVitaAudioBackend::Play(::AudioAsset* asset, ::AudioPlaybackRequest* request) {
        if (asset == nullptr) {
            throw std::invalid_argument("asset");
        }
        if (asset->SampleRate <= 0) {
            throw std::runtime_error("PS Vita audio playback requires one audio asset with a positive sample rate.");
        }
        if (asset->Channels != 1 && asset->Channels != 2) {
            throw std::runtime_error("PS Vita audio playback supports only mono or stereo PCM assets.");
        }
        if (!UsesPcmEncoding(asset->EncodingFamilyId)) {
            throw std::runtime_error("PS Vita audio playback currently requires shared PCM assets.");
        }
        if (asset->EncodedBytes == nullptr || asset->EncodedBytes->Length <= 0 || asset->EncodedBytes->Data == nullptr) {
            throw std::runtime_error("PS Vita audio playback requires one non-empty encoded payload.");
        }

        const int32_t bytesPerFrame = static_cast<int32_t>(sizeof(std::int16_t) * asset->Channels);
        if (bytesPerFrame <= 0 || (asset->EncodedBytes->Length % bytesPerFrame) != 0) {
            throw std::runtime_error("PS Vita audio playback requires 16-bit PCM frame alignment.");
        }

        std::lock_guard<std::mutex> lock(StateMutex);

        ActiveVoiceState voice = {};
        voice.VoiceId = NextVoiceId++;
        voice.SourceSamples = reinterpret_cast<const std::int16_t*>(asset->EncodedBytes->Data);
        voice.SourceFrameCount = asset->EncodedBytes->Length / bytesPerFrame;
        voice.SourceSampleRate = asset->SampleRate;
        voice.SourceChannelCount = asset->Channels;
        voice.BusId = NormalizeBusId(request != nullptr && !request->BusId.empty() ? request->BusId : asset->DefaultBusId);
        voice.BaseGain = ClampGain(request != nullptr ? request->Gain : 1.0f);
        voice.Loop = request != nullptr ? request->Loop : asset->DefaultLoop;
        voice.Playing = true;
        voice.SourceCursorQ16 = 0;

        ActiveVoice = voice;
        HasActiveVoice = true;
        HasLoggedActiveMix = false;

        AppendAudioTrace(
            std::string("play voiceId=") + std::to_string(voice.VoiceId)
            + " channels=" + std::to_string(asset->Channels)
            + " sampleRate=" + std::to_string(asset->SampleRate)
            + " frameCount=" + std::to_string(voice.SourceFrameCount)
            + " loop=" + (voice.Loop ? "true" : "false")
            + " bus=" + voice.BusId
            + " gain=" + std::to_string(voice.BaseGain));
        return voice.VoiceId;
    }

    void PsVitaAudioBackend::Stop(int32_t voiceId) {
        std::lock_guard<std::mutex> lock(StateMutex);
        if (!HasActiveVoice || ActiveVoice.VoiceId != voiceId) {
            return;
        }

        AppendAudioTrace(std::string("stop voiceId=") + std::to_string(voiceId));
        ActiveVoice.Playing = false;
        HasActiveVoice = false;
        HasLoggedActiveMix = false;
    }

    void PsVitaAudioBackend::SetBusGain(std::string busId, float gain) {
        std::lock_guard<std::mutex> lock(StateMutex);
        BusGainsById[NormalizeBusId(std::move(busId))] = ClampGain(gain);
    }

    void PsVitaAudioBackend::SetBusPaused(std::string busId, bool paused) {
        std::lock_guard<std::mutex> lock(StateMutex);
        std::string normalizedBusId = NormalizeBusId(std::move(busId));
        if (paused) {
            PausedBusIds.insert(normalizedBusId);
        } else {
            PausedBusIds.erase(normalizedBusId);
        }
    }

    bool PsVitaAudioBackend::IsPlaying(int32_t voiceId) {
        std::lock_guard<std::mutex> lock(StateMutex);
        return HasActiveVoice && ActiveVoice.VoiceId == voiceId && ActiveVoice.Playing;
    }

    void PsVitaAudioBackend::Update() {
    }

    void* PsVitaAudioBackend::AudioThreadEntry(void* argument) {
        if (argument == nullptr) {
            return nullptr;
        }

        static_cast<PsVitaAudioBackend*>(argument)->RunAudioThread();
        return nullptr;
    }

    void PsVitaAudioBackend::RunAudioThread() {
        while (Running.load()) {
            int volume = 0;
            {
                std::lock_guard<std::mutex> lock(StateMutex);
                volume = FillOutputBufferLocked();
            }

            if (PortId < 0) {
                break;
            }

            int clampedVolume = std::clamp(volume, 0, HardwareMaxVolume);
            int channelVolumes[2] = { clampedVolume, clampedVolume };
            sceAudioOutSetVolume(
                PortId,
                static_cast<SceAudioOutChannelFlag>(SCE_AUDIO_VOLUME_FLAG_L_CH | SCE_AUDIO_VOLUME_FLAG_R_CH),
                channelVolumes);
            sceAudioOutOutput(PortId, OutputBuffer.data());
        }
    }

    int PsVitaAudioBackend::FillOutputBufferLocked() {
        std::fill(OutputBuffer.begin(), OutputBuffer.end(), static_cast<std::int16_t>(0));

        if (!HasActiveVoice || !ActiveVoice.Playing) {
            return 0;
        }
        if (IsBusPausedLocked(ActiveVoice.BusId)) {
            return 0;
        }

        float combinedGain = ResolveCombinedGainLocked(ActiveVoice.BusId, ActiveVoice.BaseGain);
        if (combinedGain <= 0.0f) {
            return 0;
        }

        if (!HasLoggedActiveMix) {
            AppendAudioTrace(
                std::string("mix voiceId=") + std::to_string(ActiveVoice.VoiceId)
                + " combinedGain=" + std::to_string(combinedGain)
                + " sourceFrames=" + std::to_string(ActiveVoice.SourceFrameCount)
                + " sourceRate=" + std::to_string(ActiveVoice.SourceSampleRate)
                + " sourceChannels=" + std::to_string(ActiveVoice.SourceChannelCount));
            HasLoggedActiveMix = true;
        }

        std::uint64_t stepQ16 = (static_cast<std::uint64_t>(ActiveVoice.SourceSampleRate) << 16) / OutputSampleRate;
        if (stepQ16 == 0) {
            stepQ16 = 1;
        }

        std::uint64_t loopLengthQ16 = static_cast<std::uint64_t>(ActiveVoice.SourceFrameCount) << 16;
        for (int frameIndex = 0; frameIndex < BufferFrameCount; frameIndex++) {
            int32_t sourceFrameIndex = static_cast<int32_t>(ActiveVoice.SourceCursorQ16 >> 16);
            if (sourceFrameIndex >= ActiveVoice.SourceFrameCount) {
                if (!ActiveVoice.Loop || ActiveVoice.SourceFrameCount <= 0) {
                    ActiveVoice.Playing = false;
                    HasActiveVoice = false;
                    HasLoggedActiveMix = false;
                    break;
                }

                ActiveVoice.SourceCursorQ16 %= loopLengthQ16;
                sourceFrameIndex = static_cast<int32_t>(ActiveVoice.SourceCursorQ16 >> 16);
            }

            if (ActiveVoice.SourceChannelCount == 1) {
                std::int16_t sample = ActiveVoice.SourceSamples[sourceFrameIndex];
                OutputBuffer[frameIndex * OutputChannelCount] = sample;
                OutputBuffer[(frameIndex * OutputChannelCount) + 1] = sample;
            } else {
                int32_t sourceSampleOffset = sourceFrameIndex * ActiveVoice.SourceChannelCount;
                OutputBuffer[frameIndex * OutputChannelCount] = ActiveVoice.SourceSamples[sourceSampleOffset];
                OutputBuffer[(frameIndex * OutputChannelCount) + 1] = ActiveVoice.SourceSamples[sourceSampleOffset + 1];
            }

            ActiveVoice.SourceCursorQ16 += stepQ16;
        }

        return ConvertGainToVolume(combinedGain);
    }

    float PsVitaAudioBackend::ResolveCombinedGainLocked(const std::string& busId, float baseGain) const {
        float masterGain = 1.0f;
        auto masterGainIterator = BusGainsById.find("master");
        if (masterGainIterator != BusGainsById.end()) {
            masterGain = masterGainIterator->second;
        }

        float busGain = 1.0f;
        auto busGainIterator = BusGainsById.find(busId);
        if (busGainIterator != BusGainsById.end()) {
            busGain = busGainIterator->second;
        }

        return ClampGain(masterGain * busGain * baseGain);
    }

    bool PsVitaAudioBackend::IsBusPausedLocked(const std::string& busId) const {
        return PausedBusIds.contains("master") || PausedBusIds.contains(busId);
    }

    std::string PsVitaAudioBackend::NormalizeBusId(std::string busId) {
        if (busId.empty()) {
            return "master";
        }

        std::transform(
            busId.begin(),
            busId.end(),
            busId.begin(),
            [](unsigned char value) {
                return static_cast<char>(std::tolower(value));
            });
        return busId;
    }

    float PsVitaAudioBackend::ClampGain(float gain) {
        if (!(gain >= 0.0f) || gain != gain) {
            return 0.0f;
        }

        return std::clamp(gain, 0.0f, 1.0f);
    }

    int PsVitaAudioBackend::ConvertGainToVolume(float gain) {
        return static_cast<int>(std::clamp(gain, 0.0f, 1.0f) * HardwareMaxVolume);
    }

    bool PsVitaAudioBackend::UsesPcmEncoding(const std::string& encodingFamilyId) {
        return encodingFamilyId == "pcm-streamed" || encodingFamilyId == "pcm-buffered";
    }

    void PsVitaAudioBackend::ResetAudioTrace() {
        std::FILE* file = std::fopen(AudioTracePath, "w");
        if (file != nullptr) {
            std::fclose(file);
        }
    }

    void PsVitaAudioBackend::AppendAudioTrace(const std::string& message) {
        std::FILE* file = std::fopen(AudioTracePath, "a");
        if (file == nullptr) {
            return;
        }

        std::fwrite(message.c_str(), 1, message.size(), file);
        const char newline = '\n';
        std::fwrite(&newline, 1, 1, file);
        std::fclose(file);
    }
}
