#pragma once
#include "NpnoDecoder.h"
#include <array>
#include <atomic>
#include <memory>
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>

//==============================================================================
// Audiophile Dual-Layer Sample Playback Engine (Black Edition)
// Supports independent Layer A and Layer B instruments, 4-point Hermite
// cubic resampling, 3-band EQ, compressor, chorus, tremolo, stereo delay,
// reverb, dynamics velocity curve, and equal-power mix crossfader.
//==============================================================================
class PianoEngine {
public:
    struct LayerConfig {
        std::atomic<bool> enabled{true};
        std::atomic<float> volume{0.8f};     // 0.0 to 1.5
        std::atomic<float> pan{0.0f};        // -1.0 (Left) to +1.0 (Right)
        std::atomic<bool> solo{false};
        std::atomic<bool> mute{false};
        std::atomic<int> keyLow{0};          // 0 to 127
        std::atomic<int> keyHigh{127};       // 0 to 127
        std::atomic<bool> stereo{true};
        std::atomic<bool> mono{false};       // Per-layer True Monophonic Mode
        std::atomic<bool> releases{true};
        std::atomic<bool> halfPedal{true};

        std::string name;
        std::string path;
        std::string category = "Acoustic Piano";
    };

    void prepare(double outputSampleRate);

    // Triple-Layer Management: Piano (A), Strings (B), Synth (C)
    void setBank(std::shared_ptr<const npno::Bank> b) { setLayerABank(b); }
    bool hasLoadedBank() const { return hasLayerA() || hasLayerB() || hasLayerC(); }
    void setLayerABank(std::shared_ptr<const npno::Bank> b, const std::string& name = "", const std::string& path = "", const std::string& category = "Acoustic Piano");
    void setLayerBBank(std::shared_ptr<const npno::Bank> b, const std::string& name = "", const std::string& path = "", const std::string& category = "Orchestral / Strings");
    void setLayerCBank(std::shared_ptr<const npno::Bank> b, const std::string& name = "", const std::string& path = "", const std::string& category = "Sample Synth");

    std::shared_ptr<const npno::Bank> getLayerABank() const { return std::atomic_load(&bankA); }
    std::shared_ptr<const npno::Bank> getLayerBBank() const { return std::atomic_load(&bankB); }
    std::shared_ptr<const npno::Bank> getLayerCBank() const { return std::atomic_load(&bankC); }

    bool hasLayerA() const;
    bool hasLayerB() const;
    bool hasLayerC() const;

    LayerConfig& getLayerAConfig() { return layerA; }
    LayerConfig& getLayerBConfig() { return layerB; }
    LayerConfig& getLayerCConfig() { return layerC; }
    const LayerConfig& getLayerAConfig() const { return layerA; }
    const LayerConfig& getLayerBConfig() const { return layerB; }
    const LayerConfig& getLayerCConfig() const { return layerC; }

    // MIDI performance
    void noteOn(int note, int velocity);
    void noteOff(int note);
    void sustain(bool down);
    void allNotesOff();

    // High-resolution audio rendering
    void render(float* left, float* right, int samples);

    // Crossfader Mix: 0.0 = 100% Layer A, 0.5 = 50% A / 50% B, 1.0 = 100% Layer B
    void setMixBalance(float b) { mixBalance.store(std::clamp(b, 0.0f, 1.0f), std::memory_order_relaxed); }
    float getMixBalance() const { return mixBalance.load(std::memory_order_relaxed); }

    // 3-Band Equalizer (-15 dB to +15 dB)
    void setEqTreble(float db) { eqTreble.store(std::clamp(db, -15.0f, 15.0f), std::memory_order_relaxed); updateEqFilters(); }
    float getEqTreble() const { return eqTreble.load(std::memory_order_relaxed); }

    void setEqMid(float db) { eqMid.store(std::clamp(db, -15.0f, 15.0f), std::memory_order_relaxed); updateEqFilters(); }
    float getEqMid() const { return eqMid.load(std::memory_order_relaxed); }

    void setEqBass(float db) { eqBass.store(std::clamp(db, -15.0f, 15.0f), std::memory_order_relaxed); updateEqFilters(); }
    float getEqBass() const { return eqBass.load(std::memory_order_relaxed); }

    // 3-Band Equalizer for Worship Pad (Layer B) (-15 dB to +15 dB)
    void setPadEqTreble(float db) { padEqTreble.store(std::clamp(db, -15.0f, 15.0f), std::memory_order_relaxed); updatePadEqFilters(); }
    float getPadEqTreble() const { return padEqTreble.load(std::memory_order_relaxed); }

    void setPadEqMid(float db) { padEqMid.store(std::clamp(db, -15.0f, 15.0f), std::memory_order_relaxed); updatePadEqFilters(); }
    float getPadEqMid() const { return padEqMid.load(std::memory_order_relaxed); }

    void setPadEqBass(float db) { padEqBass.store(std::clamp(db, -15.0f, 15.0f), std::memory_order_relaxed); updatePadEqFilters(); }
    float getPadEqBass() const { return padEqBass.load(std::memory_order_relaxed); }
    void updatePadEqFilters();

    // Dynamics (Velocity Response)
    void setDynamics(float d) { dynamicsParam.store(std::clamp(d, 0.2f, 2.2f), std::memory_order_relaxed); }
    float getDynamics() const { return dynamicsParam.load(std::memory_order_relaxed); }

    // Continuous Piano Tone / Cutoff Filter (Warm/Dark <-> Natural <-> Bright/Open)
    void setTone(float t) { toneParam.store(std::clamp(t, 0.0f, 1.0f), std::memory_order_relaxed); updateToneFilter(); }
    float getTone() const { return toneParam.load(std::memory_order_relaxed); }
    void updateToneFilter();

    void setDynamicsHard(bool hard) { dynamicsHard.store(hard, std::memory_order_relaxed); }
    bool isDynamicsHard() const { return dynamicsHard.load(std::memory_order_relaxed); }

    // Compressor
    void setCompressorAmount(float a) { compAmount.store(std::clamp(a, 0.0f, 10.0f), std::memory_order_relaxed); }
    float getCompressorAmount() const { return compAmount.load(std::memory_order_relaxed); }

    void setCompressorOn(bool on) { compOn.store(on, std::memory_order_relaxed); }
    bool isCompressorOn() const { return compOn.load(std::memory_order_relaxed); }

    // Chorus
    void setChorusDepth(float d) { chorusDepth.store(std::clamp(d, 0.0f, 10.0f), std::memory_order_relaxed); }
    float getChorusDepth() const { return chorusDepth.load(std::memory_order_relaxed); }

    void setChorusRate(float r) { chorusRate.store(std::clamp(r, 0.0f, 10.0f), std::memory_order_relaxed); }
    float getChorusRate() const { return chorusRate.load(std::memory_order_relaxed); }

    // Tremolo
    void setTremoloDepth(float d) { tremoloDepth.store(std::clamp(d, 0.0f, 10.0f), std::memory_order_relaxed); }
    float getTremoloDepth() const { return tremoloDepth.load(std::memory_order_relaxed); }

    void setTremoloRate(float r) { tremoloRate.store(std::clamp(r, 0.0f, 10.0f), std::memory_order_relaxed); }
    float getTremoloRate() const { return tremoloRate.load(std::memory_order_relaxed); }

    // Stereo Delay
    void setDelayTime(float t) { delayTime.store(std::clamp(t, 0.0f, 10.0f), std::memory_order_relaxed); }
    float getDelayTime() const { return delayTime.load(std::memory_order_relaxed); }

    void setDelayFeedback(float fb) { delayFeedback.store(std::clamp(fb, 0.0f, 10.0f), std::memory_order_relaxed); }
    float getDelayFeedback() const { return delayFeedback.load(std::memory_order_relaxed); }

    void setDelayOn(bool on) { delayOn.store(on, std::memory_order_relaxed); }
    bool isDelayOn() const { return delayOn.load(std::memory_order_relaxed); }

    // Reverb
    void setReverbAmount(float amt) { reverbAmount.store(std::clamp(amt, 0.0f, 10.0f), std::memory_order_relaxed); }
    float getReverbAmount() const { return reverbAmount.load(std::memory_order_relaxed); }

    void setReverbOn(bool on) { reverbOn.store(on, std::memory_order_relaxed); }
    bool isReverbOn() const { return reverbOn.load(std::memory_order_relaxed); }

    // Master Transpose (-12 to +12 semitones) & Tune (-1.00 to +1.00 semitones)
    void setTranspose(int t) { transpose.store(std::clamp(t, -12, 12), std::memory_order_relaxed); }
    int getTranspose() const { return transpose.load(std::memory_order_relaxed); }

    void setTune(float t) { tune.store(std::clamp(t, -1.0f, 1.0f), std::memory_order_relaxed); }
    float getTune() const { return tune.load(std::memory_order_relaxed); }

    // Master Volume (0 to 10)
    void setMainVolume(float v) { mainVolume.store(std::clamp(v, 0.0f, 10.0f), std::memory_order_relaxed); }
    float getMainVolume() const { return mainVolume.load(std::memory_order_relaxed); }

    // Acoustic Piano Toggles (Nord Stage 4 Piano Engine)
    void setResonance(float r) { resonanceParam.store(std::clamp(r, 0.0f, 1.0f), std::memory_order_relaxed); }
    float getResonance() const { return resonanceParam.load(std::memory_order_relaxed); }

    void setStringRes(bool on) { stringResOn.store(on, std::memory_order_relaxed); }
    bool isStringResOn() const { return stringResOn.load(std::memory_order_relaxed); }

    void setPedalNoise(bool on) { pedalNoiseOn.store(on, std::memory_order_relaxed); }
    bool isPedalNoiseOn() const { return pedalNoiseOn.load(std::memory_order_relaxed); }

    void setSoftRelease(bool on) { softRelease.store(on, std::memory_order_relaxed); }
    bool isSoftRelease() const { return softRelease.load(std::memory_order_relaxed); }

    // Worship Pad Layer Sound Shaping (Layer B)
    void setPadCutoff(float hz) { padCutoff.store(std::clamp(hz, 150.0f, 16000.0f), std::memory_order_relaxed); updatePadFilter(); }
    float getPadCutoff() const { return padCutoff.load(std::memory_order_relaxed); }

    void setPadResonance(float q) { padResonance.store(std::clamp(q, 0.5f, 5.0f), std::memory_order_relaxed); updatePadFilter(); }
    float getPadResonance() const { return padResonance.load(std::memory_order_relaxed); }

    void setPadAttack(float a) { padAttack.store(std::clamp(a, 0.01f, 4.0f), std::memory_order_relaxed); }
    float getPadAttack() const { return padAttack.load(std::memory_order_relaxed); }

    void setPadRelease(float r) { padRelease.store(std::clamp(r, 0.1f, 6.0f), std::memory_order_relaxed); }
    float getPadRelease() const { return padRelease.load(std::memory_order_relaxed); }

    void updatePadFilter();

    void setPedalPolarityInverted(bool inv) { pedalPolarityInverted.store(inv, std::memory_order_relaxed); }
    bool isPedalPolarityInverted() const { return pedalPolarityInverted.load(std::memory_order_relaxed); }

    void setPianoTimbre(int t) { pianoTimbre.store(t, std::memory_order_relaxed); updateEqFilters(); }
    int getPianoTimbre() const { return pianoTimbre.load(std::memory_order_relaxed); }

    void setTouchCurve(int c) { touchCurve.store(c, std::memory_order_relaxed); }
    int getTouchCurve() const { return touchCurve.load(std::memory_order_relaxed); }

    void setOpenStrings(bool on) { openStringsOn.store(on, std::memory_order_relaxed); }
    bool isOpenStringsOn() const { return openStringsOn.load(std::memory_order_relaxed); }

    // Performance envelope/synth parameters
    void setAttack(float a) { attackParam.store(a, std::memory_order_relaxed); }
    float getAttack() const { return attackParam.load(std::memory_order_relaxed); }

    void setRelease(float r) { releaseParam.store(r, std::memory_order_relaxed); }
    float getRelease() const { return releaseParam.load(std::memory_order_relaxed); }

    void setPlayMode(int mode) { playMode.store(mode, std::memory_order_relaxed); } // 0 = Poly, 1 = Mono/Legato
    int getPlayMode() const { return playMode.load(std::memory_order_relaxed); }

    void setGlide(float g) { glideParam.store(g, std::memory_order_relaxed); }
    float getGlide() const { return glideParam.load(std::memory_order_relaxed); }

    void setPortamentoOn(bool on) { portamentoOn.store(on, std::memory_order_relaxed); }
    bool isPortamentoOn() const { return portamentoOn.load(std::memory_order_relaxed); }

    void stopVoicesForLayer(int layerIndex) {
        for (auto& v : voices) {
            if (v.layerIndex == layerIndex) v.reset();
        }
    }

    void setVibrato(float v) { vibratoParam.store(v, std::memory_order_relaxed); }
    float getVibrato() const { return vibratoParam.load(std::memory_order_relaxed); }

    void setModWheel(float mw) { modWheel.store(std::clamp(mw, 0.0f, 1.0f), std::memory_order_relaxed); }
    float getModWheel() const { return modWheel.load(std::memory_order_relaxed); }

    void setPitchBend(float pb) { pitchBend.store(std::clamp(pb, -1.0f, 1.0f), std::memory_order_relaxed); }
    float getPitchBend() const { return pitchBend.load(std::memory_order_relaxed); }

    void notifyMidi() { midiActivity.store(true, std::memory_order_relaxed); }
    bool consumeMidiActivity() { return midiActivity.exchange(false, std::memory_order_relaxed); }

    int getActiveVoiceCount() const;

private:
    static constexpr size_t maxVoices = 64;

    struct Voice {
        int layerIndex = 0; // 0 = Layer A, 1 = Layer B
        const npno::Bank* bank = nullptr;
        const npno::Sample* sample = nullptr;
        int note = -1, velocity = 0;
        double pos = 0.0, step = 0.0;
        double currentPitchSemis = 0.0;
        double targetPitchSemis = 0.0;
        float gain = 0.0f, envelope = 1.0f;
        float attackProgress = 1.0f;
        float attackStep = 1.0f;
        float loopDecayFactor = 1.0f;
        float fadeIn = 0.0f;
        bool keyDown = false, released = false, releaseSample = false;
        bool hasLoopedOnce = false;

        void reset() {
            layerIndex = 0;
            bank = nullptr;
            sample = nullptr;
            note = -1;
            velocity = 0;
            pos = 0.0;
            step = 0.0;
            currentPitchSemis = 0.0;
            targetPitchSemis = 0.0;
            gain = 0.0f;
            envelope = 1.0f;
            attackProgress = 1.0f;
            attackStep = 1.0f;
            loopDecayFactor = 1.0f;
            fadeIn = 0.0f;
            keyDown = false;
            released = false;
            releaseSample = false;
            hasLoopedOnce = false;
        }
    };

    struct Biquad {
        float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
        float z1 = 0.0f, z2 = 0.0f;

        void reset() { z1 = z2 = 0.0f; }

        inline float process(float in) noexcept {
            float out = in * b0 + z1;
            z1 = in * b1 - out * a1 + z2;
            z2 = in * b2 - out * a2;
            return out;
        }

        void setLowPass(double sampleRate, double frequency, double q = 0.7071);
        void setLowShelf(double sampleRate, double frequency, double gainDb);
        void setHighShelf(double sampleRate, double frequency, double gainDb);
        void setPeaking(double sampleRate, double frequency, double q, double gainDb);
    };

    std::array<Voice, maxVoices> voices{};

    std::shared_ptr<const npno::Bank> bankA;
    std::shared_ptr<const npno::Bank> bankB;
    std::shared_ptr<const npno::Bank> bankC;

    LayerConfig layerA;
    LayerConfig layerB;
    LayerConfig layerC;

    double sampleRate = 44100.0;
    bool pedal = false;
    int monoActiveNoteA = -1;
    int monoActiveNoteB = -1;
    int monoActiveNoteC = -1;
    std::vector<int> heldNotesA;
    std::vector<int> heldNotesB;
    std::vector<int> heldNotesC;
    int monoActiveNote = -1;
    std::vector<int> heldNotes;

    // Crossfader Mix
    std::atomic<float> mixBalance{0.5f}; // 0 = A, 1 = B

    // EQ
    std::atomic<float> eqTreble{0.0f};
    std::atomic<float> eqMid{0.0f};
    std::atomic<float> eqBass{0.0f};

    Biquad eqBassL, eqBassR;
    Biquad eqMidL, eqMidR;
    Biquad eqTrebleL, eqTrebleR;

    // Pad EQ (Independent 3-Band Equalizer for Layer B)
    std::atomic<float> padEqTreble{0.0f};
    std::atomic<float> padEqMid{0.0f};
    std::atomic<float> padEqBass{0.0f};

    Biquad padEqBassL, padEqBassR;
    Biquad padEqMidL, padEqMidR;
    Biquad padEqTrebleL, padEqTrebleR;

    // Dynamics
    std::atomic<float> dynamicsParam{1.0f};
    std::atomic<bool>  dynamicsHard{false};

    // Compressor
    std::atomic<float> compAmount{0.0f};
    std::atomic<bool>  compOn{false};
    float compEnv = 0.0f;

    // Chorus
    std::atomic<float> chorusDepth{0.0f};
    std::atomic<float> chorusRate{2.5f};
    static constexpr size_t chorusBufferSize = 4096;
    std::vector<float> chorusBufferL;
    std::vector<float> chorusBufferR;
    size_t chorusWriteIndex = 0;
    double chorusLfoPhaseL = 0.0;
    double chorusLfoPhaseR = 0.0;

    // Tremolo
    std::atomic<float> tremoloDepth{0.0f};
    std::atomic<float> tremoloRate{4.0f};
    double tremoloPhase = 0.0;

    // Stereo Ping-Pong / Dampened Delay
    std::atomic<float> delayTime{3.5f};      // 0..10 (50ms to 750ms)
    std::atomic<float> delayFeedback{3.5f};  // 0..10 (0% to 80%)
    std::atomic<bool>  delayOn{false};
    static constexpr size_t maxDelaySamples = 96000;
    std::vector<float> delayBufferL;
    std::vector<float> delayBufferR;
    size_t delayWriteIndex = 0;
    float delayFilterL = 0.0f;
    float delayFilterR = 0.0f;

    // Reverb
    std::atomic<float> reverbAmount{2.5f};   // 0..10
    std::atomic<bool>  reverbOn{true};

    // Master
    std::atomic<int>   transpose{0};         // -12 to +12
    std::atomic<float> tune{0.0f};           // -1.00 to +1.00
    std::atomic<float> mainVolume{8.0f};     // 0 to 10

    // Acoustic Piano Toggles (Nord Stage 4)
    std::atomic<float> resonanceParam{0.5f};
    std::atomic<bool>  stringResOn{true};
    std::atomic<bool>  pedalNoiseOn{true};
    std::atomic<bool>  openStringsOn{false};
    std::atomic<bool>  softRelease{true};
    std::atomic<bool>  pedalPolarityInverted{false};
    std::atomic<float> toneParam{0.75f};
    Biquad toneFilterL, toneFilterR;
    std::atomic<int>   pianoTimbre{0}; // 0 = Raw, 1 = Soft, 2 = Mid, 3 = Bright
    std::atomic<int>   touchCurve{1};  // 0 = Soft, 1 = Normal, 2 = Hard

    // Worship Pad (Layer B) Sound Shaping
    std::atomic<float> padCutoff{4500.0f};    // 150 Hz to 16000 Hz
    std::atomic<float> padResonance{1.0f};    // Q = 0.5 to 5.0
    std::atomic<float> padAttack{0.35f};      // 0.01s to 4.0s
    std::atomic<float> padRelease{2.0f};      // 0.1s to 6.0s
    Biquad padFilterL, padFilterR;

    // Synth / Envelope
    std::atomic<float> attackParam{0.001f};
    std::atomic<float> releaseParam{0.6f};
    std::atomic<int>   playMode{0};          // 0 = Poly, 1 = Mono/Legato
    std::atomic<float> glideParam{0.05f};
    std::atomic<bool>  portamentoOn{true};
    std::atomic<float> vibratoParam{0.0f};
    std::atomic<float> modWheel{0.0f};
    std::atomic<float> pitchBend{0.0f};
    double lfoPhase = 0.0;

    std::atomic<bool> midiActivity{false};

    // Preallocated layer buffers for rendering block
    std::vector<float> layerBufAL;
    std::vector<float> layerBufAR;
    std::vector<float> layerBufBL;
    std::vector<float> layerBufBR;
    std::vector<float> layerBufCL;
    std::vector<float> layerBufCR;

    Voice& allocateVoice();
    void startVoice(int layerIndex, std::shared_ptr<const npno::Bank> loadedBank,
                    int note, int velocity, int whichBank, bool releaseSample, bool isLegato);
    void stopNote(int note);
    void updateEqFilters();
};
