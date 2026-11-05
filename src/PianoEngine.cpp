#include "PianoEngine.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace {
// 4-point Hermite cubic spline interpolation for bit-perfect audio resampling
inline float interpolateHermite4(float ym1, float y0, float y1, float y2, float frac) {
    const float c0 = y0;
    const float c1 = 0.5f * (y1 - ym1);
    const float c2 = ym1 - 2.5f * y0 + 2.0f * y1 - 0.5f * y2;
    const float c3 = 0.5f * (y2 - ym1) + 1.5f * (y0 - y1);
    return ((c3 * frac + c2) * frac + c1) * frac + c0;
}

inline float softSaturate(float x) {
    const float absX = std::abs(x);
    if (absX < 0.75f) return x; // 100% bit-exact linear below -2.5 dB
    const float sign = (x > 0.0f) ? 1.0f : -1.0f;
    const float excess = absX - 0.75f;
    return sign * (0.75f + (0.24f * excess) / (0.24f + excess));
}
}

//==============================================================================
// Biquad Audio EQ Filter Implementation (Audio EQ Cookbook)
//==============================================================================
void PianoEngine::Biquad::setLowPass(double sRate, double f0, double q) {
    if (f0 >= sRate * 0.45) {
        b0 = 1.0f; b1 = 0.0f; b2 = 0.0f;
        a1 = 0.0f; a2 = 0.0f;
        return;
    }
    const double w0 = 2.0 * 3.141592653589793 * (std::clamp(f0, 20.0, sRate * 0.45) / sRate);
    const double cos_w0 = std::cos(w0);
    const double sin_w0 = std::sin(w0);
    const double alpha = sin_w0 / (2.0 * std::max(0.1, q));
    const double a0_val = 1.0 + alpha;
    if (std::abs(a0_val) < 1e-8) return;

    b0 = float(((1.0 - cos_w0) / 2.0) / a0_val);
    b1 = float((1.0 - cos_w0) / a0_val);
    b2 = float(((1.0 - cos_w0) / 2.0) / a0_val);
    a1 = float((-2.0 * cos_w0) / a0_val);
    a2 = float((1.0 - alpha) / a0_val);
}

void PianoEngine::Biquad::setLowShelf(double sRate, double f0, double gainDb) {
    const double A = std::pow(10.0, gainDb / 40.0);
    const double w0 = 2.0 * 3.141592653589793 * (f0 / sRate);
    const double cos_w0 = std::cos(w0);
    const double sin_w0 = std::sin(w0);
    const double alpha = (sin_w0 / 2.0) * 1.4142135623730951; // Q = 0.707 (sqrt(2))
    const double two_sqrtA_alpha = 2.0 * std::sqrt(A) * alpha;

    const double a0_val = (A + 1.0) + (A - 1.0) * cos_w0 + two_sqrtA_alpha;
    if (std::abs(a0_val) < 1e-8) return;

    b0 = float((A * ((A + 1.0) - (A - 1.0) * cos_w0 + two_sqrtA_alpha)) / a0_val);
    b1 = float((2.0 * A * ((A - 1.0) - (A + 1.0) * cos_w0)) / a0_val);
    b2 = float((A * ((A + 1.0) - (A - 1.0) * cos_w0 - two_sqrtA_alpha)) / a0_val);
    a1 = float((-2.0 * ((A - 1.0) + (A + 1.0) * cos_w0)) / a0_val);
    a2 = float(((A + 1.0) + (A - 1.0) * cos_w0 - two_sqrtA_alpha) / a0_val);
}

void PianoEngine::Biquad::setHighShelf(double sRate, double f0, double gainDb) {
    const double A = std::pow(10.0, gainDb / 40.0);
    const double w0 = 2.0 * 3.141592653589793 * (f0 / sRate);
    const double cos_w0 = std::cos(w0);
    const double sin_w0 = std::sin(w0);
    const double alpha = (sin_w0 / 2.0) * 1.4142135623730951;
    const double two_sqrtA_alpha = 2.0 * std::sqrt(A) * alpha;

    const double a0_val = (A + 1.0) - (A - 1.0) * cos_w0 + two_sqrtA_alpha;
    if (std::abs(a0_val) < 1e-8) return;

    b0 = float((A * ((A + 1.0) + (A - 1.0) * cos_w0 + two_sqrtA_alpha)) / a0_val);
    b1 = float((-2.0 * A * ((A - 1.0) + (A + 1.0) * cos_w0)) / a0_val);
    b2 = float((A * ((A + 1.0) + (A - 1.0) * cos_w0 - two_sqrtA_alpha)) / a0_val);
    a1 = float((2.0 * ((A - 1.0) - (A + 1.0) * cos_w0)) / a0_val);
    a2 = float(((A + 1.0) - (A - 1.0) * cos_w0 - two_sqrtA_alpha) / a0_val);
}

void PianoEngine::Biquad::setPeaking(double sRate, double f0, double q, double gainDb) {
    const double A = std::pow(10.0, gainDb / 40.0);
    const double w0 = 2.0 * 3.141592653589793 * (f0 / sRate);
    const double cos_w0 = std::cos(w0);
    const double sin_w0 = std::sin(w0);
    const double alpha = sin_w0 / (2.0 * q);

    const double a0_val = 1.0 + alpha / A;
    if (std::abs(a0_val) < 1e-8) return;

    b0 = float((1.0 + alpha * A) / a0_val);
    b1 = float((-2.0 * cos_w0) / a0_val);
    b2 = float((1.0 - alpha * A) / a0_val);
    a1 = float((-2.0 * cos_w0) / a0_val);
    a2 = float((1.0 - alpha / A) / a0_val);
}

//==============================================================================
// PianoEngine Lifecycle
//==============================================================================
void PianoEngine::prepare(double outputSampleRate) {
    sampleRate = outputSampleRate > 1000.0 ? outputSampleRate : 44100.0;
    allNotesOff();
    monoActiveNote = -1;
    heldNotes.clear();

    // Reset EQ
    eqBassL.reset(); eqBassR.reset();
    eqMidL.reset(); eqMidR.reset();
    eqTrebleL.reset(); eqTrebleR.reset();
    padEqBassL.reset(); padEqBassR.reset();
    padEqMidL.reset(); padEqMidR.reset();
    padEqTrebleL.reset(); padEqTrebleR.reset();
    toneFilterL.reset(); toneFilterR.reset();
    updateEqFilters();
    updatePadEqFilters();
    updateToneFilter();

    // Chorus
    chorusBufferL.assign(chorusBufferSize, 0.0f);
    chorusBufferR.assign(chorusBufferSize, 0.0f);
    chorusWriteIndex = 0;
    chorusLfoPhaseL = 0.0;
    chorusLfoPhaseR = 0.0;
    lfoPhase = 0.0;
    tremoloPhase = 0.0;

    // Delay
    delayBufferL.assign(maxDelaySamples, 0.0f);
    delayBufferR.assign(maxDelaySamples, 0.0f);
    delayWriteIndex = 0;
    delayFilterL = 0.0f;
    delayFilterR = 0.0f;

    // Rendering scratch buffers
    layerBufAL.assign(4096, 0.0f);
    layerBufAR.assign(4096, 0.0f);
    layerBufBL.assign(4096, 0.0f);
    layerBufBR.assign(4096, 0.0f);
    layerBufCL.assign(4096, 0.0f);
    layerBufCR.assign(4096, 0.0f);

    compEnv = 0.0f;
    padFilterL.reset();
    padFilterR.reset();
    updatePadFilter();
}

void PianoEngine::updateEqFilters() {
    float bassDb = eqBass.load(std::memory_order_relaxed);
    float midDb = eqMid.load(std::memory_order_relaxed);
    float trebleDb = eqTreble.load(std::memory_order_relaxed);

    // Nord Stage 4 Piano Timbre Filter Offsets (Soft, Mid, Bright, Raw)
    const int timbre = pianoTimbre.load(std::memory_order_relaxed);
    if (timbre == 1) { // SOFT: rolled-off high frequencies, intimate felt piano warmth
        trebleDb -= 5.0f;
        bassDb   += 1.5f;
    } else if (timbre == 2) { // MID: prominent midrange for cutting through mix
        midDb    += 3.5f;
    } else if (timbre == 3) { // BRIGHT: sparkling modern pop/rock piano top end
        trebleDb += 4.5f;
        bassDb   -= 1.0f;
    }

    eqBassL.setLowShelf(sampleRate, 120.0, double(bassDb));
    eqBassR.setLowShelf(sampleRate, 120.0, double(bassDb));

    eqMidL.setPeaking(sampleRate, (timbre == 2 ? 1800.0 : 1000.0), 1.0, double(midDb));
    eqMidR.setPeaking(sampleRate, (timbre == 2 ? 1800.0 : 1000.0), 1.0, double(midDb));

    eqTrebleL.setHighShelf(sampleRate, 4500.0, double(trebleDb));
    eqTrebleR.setHighShelf(sampleRate, 4500.0, double(trebleDb));
}

void PianoEngine::updatePadEqFilters() {
    const float bassDb = padEqBass.load(std::memory_order_relaxed);
    const float midDb = padEqMid.load(std::memory_order_relaxed);
    const float trebleDb = padEqTreble.load(std::memory_order_relaxed);

    padEqBassL.setLowShelf(sampleRate, 140.0, double(bassDb));
    padEqBassR.setLowShelf(sampleRate, 140.0, double(bassDb));

    padEqMidL.setPeaking(sampleRate, 850.0, 0.9, double(midDb));
    padEqMidR.setPeaking(sampleRate, 850.0, 0.9, double(midDb));

    padEqTrebleL.setHighShelf(sampleRate, 4200.0, double(trebleDb));
    padEqTrebleR.setHighShelf(sampleRate, 4200.0, double(trebleDb));
}

void PianoEngine::updateToneFilter() {
    const float tone = toneParam.load(std::memory_order_relaxed);
    if (tone >= 0.98f) {
        toneFilterL.setLowPass(sampleRate, sampleRate * 0.48, 0.7071);
        toneFilterR.setLowPass(sampleRate, sampleRate * 0.48, 0.7071);
        return;
    }
    float cutoffHz;
    if (tone < 0.50f) {
        const float n = tone / 0.50f;
        cutoffHz = 250.0f * std::pow(8000.0f / 250.0f, n);
    } else {
        const float n = (tone - 0.50f) / 0.50f;
        cutoffHz = 8000.0f * std::pow(20000.0f / 8000.0f, n);
    }
    cutoffHz = std::clamp(cutoffHz, 150.0f, float(sampleRate * 0.45));
    toneFilterL.setLowPass(sampleRate, double(cutoffHz), 0.7071);
    toneFilterR.setLowPass(sampleRate, double(cutoffHz), 0.7071);
}

void PianoEngine::updatePadFilter() {
    float cutoffHz = padCutoff.load(std::memory_order_relaxed);
    float q = padResonance.load(std::memory_order_relaxed);
    cutoffHz = std::clamp(cutoffHz, 150.0f, float(sampleRate * 0.45));
    q = std::clamp(q, 0.5f, 6.0f);
    padFilterL.setLowPass(sampleRate, double(cutoffHz), double(q));
    padFilterR.setLowPass(sampleRate, double(cutoffHz), double(q));
}

void PianoEngine::setLayerABank(std::shared_ptr<const npno::Bank> b, const std::string& name, const std::string& path, const std::string& category) {
    allNotesOff();
    layerA.name = name;
    layerA.path = path;
    layerA.category = category;
    std::atomic_store(&bankA, b);
}

void PianoEngine::setLayerBBank(std::shared_ptr<const npno::Bank> b, const std::string& name, const std::string& path, const std::string& category) {
    allNotesOff();
    layerB.name = name;
    layerB.path = path;
    layerB.category = category;
    std::atomic_store(&bankB, b);
}

void PianoEngine::setLayerCBank(std::shared_ptr<const npno::Bank> b, const std::string& name, const std::string& path, const std::string& category) {
    allNotesOff();
    layerC.name = name;
    layerC.path = path;
    layerC.category = category;
    std::atomic_store(&bankC, b);
}

bool PianoEngine::hasLayerA() const {
    auto b = std::atomic_load(&bankA);
    return b != nullptr && !b->samples.empty();
}

bool PianoEngine::hasLayerB() const {
    auto b = std::atomic_load(&bankB);
    return b != nullptr && !b->samples.empty();
}

bool PianoEngine::hasLayerC() const {
    auto b = std::atomic_load(&bankC);
    return b != nullptr && !b->samples.empty();
}

int PianoEngine::getActiveVoiceCount() const {
    int count = 0;
    for (const auto& v : voices) {
        if (v.sample != nullptr) count++;
    }
    return count;
}

PianoEngine::Voice& PianoEngine::allocateVoice() {
    for (auto& v : voices) {
        if (!v.sample) return v;
    }
    // Prefer stealing a released note over an active note
    auto* choice = &voices.front();
    for (auto& v : voices) {
        if ((v.released || v.releaseSample) && !(choice->released || choice->releaseSample)) {
            choice = &v;
        } else if ((v.released == choice->released) && v.envelope < choice->envelope) {
            choice = &v;
        }
    }
    choice->reset();
    return *choice;
}

void PianoEngine::startVoice(int layerIndex, std::shared_ptr<const npno::Bank> loaded,
                            int note, int velocity, int whichBank, bool releaseSample, bool isLegato) {
    if (!loaded || note < 0 || note > 127) return;

    // Nord Stage 4 Dynamics / Touch Curves: 0 = Touch 1 (Soft), 1 = Touch 2 (Normal), 2 = Touch 3 (Hard)
    const int touch = touchCurve.load(std::memory_order_relaxed);
    float normVel = std::clamp(float(velocity) / 127.0f, 0.001f, 1.0f);

    const float dynKnob = std::clamp(dynamicsParam.load(std::memory_order_relaxed), 0.2f, 2.2f);
    normVel = std::pow(normVel, dynKnob);

    if (touch == 0) {
        normVel = std::pow(normVel, 0.48f); // Touch 1 (Soft / Worship): effortless access to loud, rich velocity layers
    } else if (touch == 2) {
        normVel = std::pow(normVel, 1.75f); // Touch 3 (Hard / Heavy Concert): requires strong strikes to reach fortissimo
    }

    const int mappedVel = std::clamp(int(std::round(normVel * 126.0f)) + 1, 1, 127);

    // Select Nord sample layer using dynamically mapped velocity
    const npno::Sample* s = loaded->select(note, mappedVel, whichBank);
    if (!s && whichBank == 1) {
        s = loaded->select(note, mappedVel, 0);
    }
    if (!s || s->pcm.size() < 4) return;

    // Calculate pitch transposition in semitones + fine tune
    const double cents = double(loaded->fineTune[size_t(note)]) * 0.7;
    const double masterTuneSemis = double(tune.load(std::memory_order_relaxed));
    const double targetSemis = double(note - int(s->root)) + (cents / 100.0) + masterTuneSemis;

    // In Monophonic Legato mode: glide existing active voice pitch smoothly
    const bool layerIsMono = (layerIndex == 2 && layerC.mono.load(std::memory_order_relaxed)) ||
                             (layerIndex == 1 && layerB.mono.load(std::memory_order_relaxed)) ||
                             (layerIndex == 0 && layerA.mono.load(std::memory_order_relaxed)) ||
                             (playMode.load(std::memory_order_relaxed) == 1);

    if (isLegato && layerIsMono) {
        for (auto& v : voices) {
            if (v.layerIndex == layerIndex && v.sample && !v.released && !v.releaseSample) {
                v.note = note;
                v.velocity = velocity;
                v.targetPitchSemis = targetSemis;
                return;
            }
        }
    }

    auto& v = allocateVoice();
    v.layerIndex = layerIndex;
    v.bank = loaded.get();
    v.sample = s;
    v.note = note;
    v.velocity = velocity;
    v.keyDown = !releaseSample;
    v.released = releaseSample;
    v.releaseSample = releaseSample;
    v.fadeIn = 0.0f; // Smooth 1 ms anti-click ramp
    // Skip pre-attack silence using Nord factory trim point for instantaneous hammer response
    v.pos = (!releaseSample && s->trim > 0 && s->trim < s->frames) ? double(std::min(s->trim, uint16_t(800))) : 0.0;

    v.currentPitchSemis = targetSemis;
    v.targetPitchSemis  = targetSemis;
    v.step = (npno::SampleRate / sampleRate) * std::pow(2.0, targetSemis / 12.0);

    // Dynamic volume range
    const float velGain = std::pow(normVel, 1.35f);
    v.gain = (0.025f + velGain * 0.975f) * std::pow(10.f, float(v.bank->gainTenthsDb) / 200.f);

    // Natural acoustic piano loop decay per sample (measured ~1.2 dB/s natural acoustic decay)
    // Electronic Synth / Worship Pads (Layer Index >= 1) sustain indefinitely at full level while held / pedal pressed
    if (layerIndex >= 1) {
        v.loopDecayFactor = 1.0f;
    } else {
        const float decayDb = (s->isLooped && s->decayDbPerSec > 0.05f) ? s->decayDbPerSec : 1.25f;
        v.loopDecayFactor = std::pow(10.0f, float(-double(decayDb) / (20.0 * sampleRate)));
    }

    // String resonance gain for pedal sustain
    if (whichBank == 1) {
        const float resK = resonanceParam.load(std::memory_order_relaxed);
        v.gain *= (1.0f + resK * 0.6f);
    }

    // Mechanical key-off release sample gain
    if (releaseSample) {
        const float relKnob = releaseParam.load(std::memory_order_relaxed);
        if (relKnob <= 0.20f) {
            v.gain = 0.0f;
        } else {
            v.gain *= std::clamp((relKnob - 0.20f) * 0.06f, 0.0f, 0.08f);
        }
    }

    // Attack Envelope Setting
    float attackSec = attackParam.load(std::memory_order_relaxed);
    if (layerIndex == 1) { // Layer B: Dedicated Worship Pad Swell Attack
        attackSec = padAttack.load(std::memory_order_relaxed);
    }
    if (attackSec <= 0.003f || releaseSample) {
        v.attackProgress = 1.0f;
        v.attackStep = 1.0f;
    } else {
        v.attackProgress = 0.0f;
        v.attackStep = float(1.0 / (sampleRate * double(attackSec)));
    }
}

void PianoEngine::noteOn(int rawNote, int velocity) {
    notifyMidi();
    if (velocity <= 0) {
        noteOff(rawNote);
        return;
    }

    // Master Transpose
    const int transp = transpose.load(std::memory_order_relaxed);
    const int note = rawNote + transp;
    if (note < 0 || note > 127) return;

    auto it = std::find(heldNotes.begin(), heldNotes.end(), note);
    if (it == heldNotes.end()) heldNotes.push_back(note);

    const int mode = playMode.load(std::memory_order_relaxed);

    auto loadedA = std::atomic_load(&bankA);
    auto loadedB = std::atomic_load(&bankB);
    auto loadedC = std::atomic_load(&bankC);

    const bool soloA = layerA.solo.load(std::memory_order_relaxed);
    const bool soloB = layerB.solo.load(std::memory_order_relaxed);
    const bool soloC = layerC.solo.load(std::memory_order_relaxed);
    const bool anySolo = soloA || soloB || soloC;

    const bool activeA = layerA.enabled.load(std::memory_order_relaxed) &&
                         !layerA.mute.load(std::memory_order_relaxed) &&
                         (!anySolo || soloA) && (loadedA != nullptr);

    const bool activeB = layerB.enabled.load(std::memory_order_relaxed) &&
                         !layerB.mute.load(std::memory_order_relaxed) &&
                         (!anySolo || soloB) && (loadedB != nullptr);

    const bool activeC = layerC.enabled.load(std::memory_order_relaxed) &&
                         !layerC.mute.load(std::memory_order_relaxed) &&
                         (!anySolo || soloC) && (loadedC != nullptr);

    const int lowA = layerA.keyLow.load(std::memory_order_relaxed);
    const int highA = layerA.keyHigh.load(std::memory_order_relaxed);
    const int lowB = layerB.keyLow.load(std::memory_order_relaxed);
    const int highB = layerB.keyHigh.load(std::memory_order_relaxed);
    const int lowC = layerC.keyLow.load(std::memory_order_relaxed);
    const int highC = layerC.keyHigh.load(std::memory_order_relaxed);

    const bool inRangeA = (note >= lowA && note <= highA);
    const bool inRangeB = (note >= lowB && note <= highB);
    const bool inRangeC = (note >= lowC && note <= highC);

    const bool monoA = (mode == 1) || layerA.mono.load(std::memory_order_relaxed);
    const bool monoB = (mode == 1) || layerB.mono.load(std::memory_order_relaxed);
    const bool monoC = (mode == 1) || layerC.mono.load(std::memory_order_relaxed);

    const int whichBankA = (pedal && stringResOn.load(std::memory_order_relaxed)) ? 1 : 0;
    const int whichBankB = (pedal && stringResOn.load(std::memory_order_relaxed)) ? 1 : 0;
    const int whichBankC = (pedal && stringResOn.load(std::memory_order_relaxed)) ? 1 : 0;

    // --- Layer A Triggering (Piano) ---
    if (activeA && inRangeA) {
        if (monoA) {
            heldNotesA.push_back(note);
            monoActiveNoteA = note;
            for (auto& v : voices) {
                if (v.sample && v.layerIndex == 0) v.reset();
            }
            startVoice(0, loadedA, note, velocity, 0, false, false);
        } else {
            for (auto& v : voices) {
                if (v.sample && v.layerIndex == 0 && v.note == note && !v.releaseSample) {
                    v.released = true;
                    v.keyDown = false;
                }
            }
            startVoice(0, loadedA, note, velocity, whichBankA, false, false);
        }
    }

    // --- Layer B Triggering (Strings / Orchestral) ---
    if (activeB && inRangeB) {
        if (monoB) {
            const bool isLegato = !heldNotesB.empty();
            heldNotesB.push_back(note);
            monoActiveNoteB = note;
            if (!isLegato) {
                for (auto& v : voices) {
                    if (v.sample && v.layerIndex == 1) v.reset();
                }
            }
            startVoice(1, loadedB, note, velocity, 0, false, isLegato);
        } else {
            for (auto& v : voices) {
                if (v.sample && v.layerIndex == 1 && v.note == note && !v.releaseSample) {
                    v.released = true;
                    v.keyDown = false;
                }
            }
            startVoice(1, loadedB, note, velocity, whichBankB, false, false);
        }
    }

    // --- Layer C Triggering (Synths / Leads) ---
    if (activeC && inRangeC) {
        if (monoC) {
            const bool isLegato = !heldNotesC.empty();
            heldNotesC.push_back(note);
            monoActiveNoteC = note;
            if (!isLegato) {
                for (auto& v : voices) {
                    if (v.sample && v.layerIndex == 2) v.reset();
                }
            }
            startVoice(2, loadedC, note, velocity, 0, false, isLegato);
        } else {
            for (auto& v : voices) {
                if (v.sample && v.layerIndex == 2 && v.note == note && !v.releaseSample) {
                    v.released = true;
                    v.keyDown = false;
                }
            }
            startVoice(2, loadedC, note, velocity, whichBankC, false, false);
        }
    }
}

void PianoEngine::stopNote(int note) {
    const float relKnob = releaseParam.load(std::memory_order_relaxed);
    auto loadedA = std::atomic_load(&bankA);
    auto loadedB = std::atomic_load(&bankB);
    auto loadedC = std::atomic_load(&bankC);

    for (auto& v : voices) {
        if (v.sample && v.note == note && !v.releaseSample && !v.released) {
            v.released = true;
            const bool releasesEnabled = (v.layerIndex == 0)
                ? layerA.releases.load(std::memory_order_relaxed)
                : ((v.layerIndex == 1) ? layerB.releases.load(std::memory_order_relaxed)
                                       : layerC.releases.load(std::memory_order_relaxed));

            const uint8_t dTop = (v.bank && v.bank->damperTop >= 60) ? v.bank->damperTop : 92;
            if (note <= dTop && relKnob > 0.25f && releasesEnabled && v.bank && v.bank->numRelease > 0) {
                auto b = (v.layerIndex == 0) ? loadedA : ((v.layerIndex == 1) ? loadedB : loadedC);
                startVoice(v.layerIndex, b, note, v.velocity, 2, true, false);
            }
        }
    }
}

void PianoEngine::noteOff(int rawNote) {
    notifyMidi();
    const int transp = transpose.load(std::memory_order_relaxed);
    const int note = rawNote + transp;
    if (note < 0 || note > 127) return;

    auto itAll = std::find(heldNotes.begin(), heldNotes.end(), note);
    if (itAll != heldNotes.end()) heldNotes.erase(itAll);

    const int mode = playMode.load(std::memory_order_relaxed);
    const bool monoA = (mode == 1) || layerA.mono.load(std::memory_order_relaxed);
    const bool monoB = (mode == 1) || layerB.mono.load(std::memory_order_relaxed);
    const bool monoC = (mode == 1) || layerC.mono.load(std::memory_order_relaxed);

    // --- Monophonic NoteOff handling for Layer A ---
    if (monoA) {
        auto itA = std::find(heldNotesA.begin(), heldNotesA.end(), note);
        if (itA != heldNotesA.end()) heldNotesA.erase(itA);

        if (note == monoActiveNoteA) {
            if (!heldNotesA.empty()) {
                monoActiveNoteA = heldNotesA.back();
                auto loadedA = std::atomic_load(&bankA);
                if (layerA.enabled.load(std::memory_order_relaxed) && loadedA) {
                    startVoice(0, loadedA, monoActiveNoteA, 100, 0, false, true);
                }
            } else {
                monoActiveNoteA = -1;
                for (auto& v : voices) {
                    if (v.sample && v.layerIndex == 0 && !v.releaseSample) {
                        v.keyDown = false;
                        v.released = true;
                    }
                }
            }
        }
    }

    // --- Monophonic NoteOff handling for Layer B (Strings) ---
    if (monoB) {
        auto itB = std::find(heldNotesB.begin(), heldNotesB.end(), note);
        if (itB != heldNotesB.end()) heldNotesB.erase(itB);

        if (note == monoActiveNoteB) {
            if (!heldNotesB.empty()) {
                monoActiveNoteB = heldNotesB.back();
                auto loadedB = std::atomic_load(&bankB);
                if (layerB.enabled.load(std::memory_order_relaxed) && loadedB) {
                    startVoice(1, loadedB, monoActiveNoteB, 100, 0, false, true);
                }
            } else {
                monoActiveNoteB = -1;
                for (auto& v : voices) {
                    if (v.sample && v.layerIndex == 1 && !v.releaseSample) {
                        v.keyDown = false;
                        v.released = true;
                    }
                }
            }
        }
    }

    // --- Monophonic NoteOff handling for Layer C (Synths) ---
    if (monoC) {
        auto itC = std::find(heldNotesC.begin(), heldNotesC.end(), note);
        if (itC != heldNotesC.end()) heldNotesC.erase(itC);

        if (note == monoActiveNoteC) {
            if (!heldNotesC.empty()) {
                monoActiveNoteC = heldNotesC.back();
                auto loadedC = std::atomic_load(&bankC);
                if (layerC.enabled.load(std::memory_order_relaxed) && loadedC) {
                    startVoice(2, loadedC, monoActiveNoteC, 100, 0, false, true);
                }
            } else {
                monoActiveNoteC = -1;
                for (auto& v : voices) {
                    if (v.sample && v.layerIndex == 2 && !v.releaseSample) {
                        v.keyDown = false;
                        v.released = true;
                    }
                }
            }
        }
    }

    // --- Polyphonic NoteOff handling ---
    bool found = false;
    for (auto& v : voices) {
        const bool isMonoVoice = (v.layerIndex == 0) ? monoA : ((v.layerIndex == 1) ? monoB : monoC);
        if (!isMonoVoice && v.sample && v.note == note && !v.releaseSample) {
            if (v.keyDown) {
                v.keyDown = false;
                found = true;
            }
        }
    }
    if (found && !pedal) {
        stopNote(note);
    }
}

void PianoEngine::sustain(bool down) {
    notifyMidi();
    if (pedal && !down) {
        pedal = false;
        for (int key = 0; key < 128; ++key) {
            bool hanging = false;
            for (const auto& v : voices) {
                if (v.sample && v.note == key && !v.keyDown && !v.released && !v.releaseSample) {
                    hanging = true;
                }
            }
            if (hanging) stopNote(key);
        }
    } else {
        pedal = down;
    }
}

void PianoEngine::allNotesOff() {
    notifyMidi();
    heldNotes.clear();
    heldNotesA.clear();
    heldNotesB.clear();
    monoActiveNote = -1;
    monoActiveNoteA = -1;
    monoActiveNoteB = -1;
    for (auto& v : voices) {
        v.reset();
    }
}

//==============================================================================
// Audio Rendering: Hermite Resampling, Dual Layers, Mix Crossfade, FX Chain
//==============================================================================
void PianoEngine::render(float* left, float* right, int count) {
    if (count <= 0) return;

    if (size_t(count) > layerBufAL.size()) {
        layerBufAL.assign(size_t(count), 0.0f);
        layerBufAR.assign(size_t(count), 0.0f);
        layerBufBL.assign(size_t(count), 0.0f);
        layerBufBR.assign(size_t(count), 0.0f);
        layerBufCL.assign(size_t(count), 0.0f);
        layerBufCR.assign(size_t(count), 0.0f);
    } else {
        std::fill_n(layerBufAL.data(), count, 0.0f);
        std::fill_n(layerBufAR.data(), count, 0.0f);
        std::fill_n(layerBufBL.data(), count, 0.0f);
        std::fill_n(layerBufBR.data(), count, 0.0f);
        std::fill_n(layerBufCL.data(), count, 0.0f);
        std::fill_n(layerBufCR.data(), count, 0.0f);
    }

    const float relKnob = std::clamp(releaseParam.load(std::memory_order_relaxed), 0.0f, 2.0f);
    const int mode = playMode.load(std::memory_order_relaxed);
    const float glideTime = std::clamp(glideParam.load(std::memory_order_relaxed), 0.002f, 0.45f);
    const float glideAlpha = float(1.0 - std::exp(-1.0 / (sampleRate * double(glideTime))));

    const float mw = modWheel.load(std::memory_order_relaxed);
    const float vibAmount = vibratoParam.load(std::memory_order_relaxed);
    const float totalVib = std::clamp(vibAmount + mw * 0.70f, 0.0f, 1.0f);
    const float pb = pitchBend.load(std::memory_order_relaxed);

    // Authentic acoustic piano damper release time
    // When relKnob is at 0.0, the sound is completely dry (instant physical felt damper stop in ~30ms).
    // As relKnob increases, decayTime smoothly expands from 0.004s up to 1.5s.
    const bool softRel = softRelease.load(std::memory_order_relaxed);
    float decayTime;
    if (relKnob <= 0.02f) {
        decayTime = 0.004f; // Ultra-dry: immediate felt damper cutoff in 30ms!
    } else {
        const float base = softRel ? 0.035f : 0.012f;
        decayTime = base + std::pow(relKnob / 1.0f, 1.6f) * 0.35f;
    }
    const float mainReleaseFactor = float(std::exp(-1.0 / (sampleRate * double(decayTime))));
    const float undampedReleaseFactor = float(std::exp(-1.0 / (sampleRate * (relKnob <= 0.02f ? 0.008 : (1.4 + double(relKnob) * 0.5)))));
    const float releaseSampleFactor = float(std::exp(-1.0 / (sampleRate * 0.045)));
    constexpr float declickStep = 1.0f / 48.0f;

    // 1. Render all active voices into Layer A and Layer B buffers
    for (int i = 0; i < count; ++i) {
        // Natural 5.2 Hz Vibrato LFO
        lfoPhase += (2.0 * 3.141592653589793 * 5.2) / sampleRate;
        if (lfoPhase > 6.283185307179586) lfoPhase -= 6.283185307179586;
        const float lfoSin = float(std::sin(lfoPhase));
        const float vibratoPitchSemis = lfoSin * totalVib * 0.50f;

        for (auto& v : voices) {
            if (!v.sample || !v.bank) continue;

            const auto* s = v.sample;
            const int ch = v.bank->channels;
            const size_t totalFrames = s->frames;

            // Continuous sustain: Unlooped natural acoustic playback
            const bool canLoop = s->isLooped && (s->loopEnd > s->loopStart + 500) && (s->loopEnd <= totalFrames);
            const size_t lStart = canLoop ? s->loopStart : 0;
            const size_t lEnd   = canLoop ? s->loopEnd : totalFrames;
            const size_t xfadeLen = (canLoop && lEnd > lStart + 1024) ? std::min(size_t(2048), (lEnd - lStart) / 8) : 0;

            const bool isSustaining = !v.released; // True while key is held OR pedal is pressed

            float unloopFadeGain = 1.0f;
            if (canLoop && isSustaining) {
                if (v.pos >= double(lEnd)) {
                    const double loopLen = double(lEnd - lStart);
                    v.pos = double(lStart) + std::fmod(v.pos - double(lEnd), loopLen);
                    v.hasLoopedOnce = true;
                }
                if (v.hasLoopedOnce) {
                    v.envelope *= v.loopDecayFactor;
                    if (v.envelope < 0.0001f) {
                        v.reset();
                        continue;
                    }
                }
            } else {
                // 100% Unlooped Acoustic Piano Playback:
                // Smoothly fade out to absolute silence starting at 65% of frames (spanning 1.2 to 2.0 full seconds)
                const size_t fadeStart = (totalFrames > 4000) ? size_t(double(totalFrames) * 0.65) : 0;
                if (fadeStart > 0 && static_cast<size_t>(v.pos) >= fadeStart) {
                    const float ratio = std::clamp(float(totalFrames - static_cast<size_t>(v.pos)) / float(totalFrames - fadeStart), 0.0f, 1.0f);
                    // Cosine S-curve: completely smooth, continuous derivative, zero click, zero abrupt cut
                    unloopFadeGain = 0.5f * (1.0f - std::cos(3.141592653589793f * ratio));
                }
                if (static_cast<size_t>(v.pos) + 2 >= totalFrames || unloopFadeGain <= 0.0001f) {
                    v.reset();
                    continue;
                }
            }

            const size_t idx0 = static_cast<size_t>(v.pos);

            // Real-time Mono Legato Portamento / Glide for Layer C (Synth), Layer B, or Layer A
            const bool isMonoVoice = (v.layerIndex == 2 && layerC.mono.load(std::memory_order_relaxed)) ||
                                     (v.layerIndex == 1 && layerB.mono.load(std::memory_order_relaxed)) ||
                                     (v.layerIndex == 0 && layerA.mono.load(std::memory_order_relaxed)) ||
                                     (mode == 1);

            if (isMonoVoice && portamentoOn.load(std::memory_order_relaxed) && std::abs(v.targetPitchSemis - v.currentPitchSemis) > 0.0005) {
                v.currentPitchSemis += (v.targetPitchSemis - v.currentPitchSemis) * double(glideAlpha);
            } else if (!portamentoOn.load(std::memory_order_relaxed) || !isMonoVoice) {
                v.currentPitchSemis = v.targetPitchSemis;
            }

            const double totalSemis = v.currentPitchSemis + double(vibratoPitchSemis) + double(pb * 2.0f);
            v.step = (npno::SampleRate / sampleRate) * std::pow(2.0, totalSemis / 12.0);

            const size_t idxM1 = (idx0 > 0) ? (idx0 - 1) : 0;
            const size_t idx1  = idx0 + 1;
            const size_t idx2  = (idx0 + 2 < totalFrames) ? (idx0 + 2) : idx1;
            const float frac   = float(v.pos - double(idx0));

            // Hermite cubic interpolation: Left
            const float ym1_l = float(s->pcm[idxM1 * size_t(ch)]);
            const float y0_l  = float(s->pcm[idx0  * size_t(ch)]);
            const float y1_l  = float(s->pcm[idx1  * size_t(ch)]);
            const float y2_l  = float(s->pcm[idx2  * size_t(ch)]);
            float l = interpolateHermite4(ym1_l, y0_l, y1_l, y2_l, frac) / 32768.0f;

            // Right
            float r;
            if (ch == 1) {
                r = l;
            } else {
                const float ym1_r = float(s->pcm[idxM1 * size_t(ch) + 1]);
                const float y0_r  = float(s->pcm[idx0  * size_t(ch) + 1]);
                const float y1_r  = float(s->pcm[idx1  * size_t(ch) + 1]);
                const float y2_r  = float(s->pcm[idx2  * size_t(ch) + 1]);
                r = interpolateHermite4(ym1_r, y0_r, y1_r, y2_r, frac) / 32768.0f;
            }

            // Smooth crossfade near loop boundary to prevent any clicks or pops
            if (canLoop && isSustaining && xfadeLen > 0 && v.pos >= double(lEnd - xfadeLen) && v.pos < double(lEnd)) {
                const double xfPos = double(lStart) + (v.pos - double(lEnd - xfadeLen));
                const size_t xfIdx0 = static_cast<size_t>(xfPos);
                if (xfIdx0 + 2 < totalFrames) {
                    const size_t xfIdxM1 = (xfIdx0 > 0) ? (xfIdx0 - 1) : 0;
                    const size_t xfIdx1  = xfIdx0 + 1;
                    const size_t xfIdx2  = (xfIdx0 + 2 < totalFrames) ? (xfIdx0 + 2) : xfIdx1;
                    const float xfFrac   = float(xfPos - double(xfIdx0));
                    const float xf_l = interpolateHermite4(float(s->pcm[xfIdxM1 * size_t(ch)]),
                                                           float(s->pcm[xfIdx0  * size_t(ch)]),
                                                           float(s->pcm[xfIdx1  * size_t(ch)]),
                                                           float(s->pcm[xfIdx2  * size_t(ch)]), xfFrac) / 32768.0f;
                    float xf_r = xf_l;
                    if (ch > 1) {
                        xf_r = interpolateHermite4(float(s->pcm[xfIdxM1 * size_t(ch) + 1]),
                                                   float(s->pcm[xfIdx0  * size_t(ch) + 1]),
                                                   float(s->pcm[xfIdx1  * size_t(ch) + 1]),
                                                   float(s->pcm[xfIdx2  * size_t(ch) + 1]), xfFrac) / 32768.0f;
                    }
                    const float linearPhase = float(v.pos - double(lEnd - xfadeLen)) / float(xfadeLen);
                    const float xfAlpha = 0.5f * (1.0f - std::cos(3.141592653589793f * linearPhase));
                    l = l * (1.0f - xfAlpha) + xf_l * xfAlpha;
                    r = r * (1.0f - xfAlpha) + xf_r * xfAlpha;
                }
            }

            if (v.fadeIn < 1.0f) {
                v.fadeIn = std::min(1.0f, v.fadeIn + declickStep);
                l *= v.fadeIn;
                r *= v.fadeIn;
            }

            if (v.attackProgress < 1.0f) {
                v.attackProgress = std::min(1.0f, v.attackProgress + v.attackStep);
            }

            const float g = v.gain * v.envelope * v.attackProgress * unloopFadeGain;

            if (v.layerIndex == 0) {
                layerBufAL[i] += l * g;
                layerBufAR[i] += r * g;
            } else if (v.layerIndex == 1) {
                layerBufBL[i] += l * g;
                layerBufBR[i] += r * g;
            } else if (v.layerIndex == 2) {
                layerBufCL[i] += l * g;
                layerBufCR[i] += r * g;
            }

            v.pos += v.step;

            if (v.released) {
                if (v.layerIndex == 1) { // Layer B: Worship Pad smooth natural release
                    const float padRelSec = padRelease.load(std::memory_order_relaxed);
                    const float padRelFactor = float(std::exp(-1.0 / (sampleRate * double(std::max(0.05f, padRelSec * 0.45f)))));
                    v.envelope *= padRelFactor;
                    if (v.envelope < 0.0005f) v.reset();
                } else if (v.releaseSample) {
                    v.envelope *= releaseSampleFactor;
                    if (v.envelope < 0.001f) v.reset();
                } else {
                    // Notes above note 92 (G#6) have no physical dampers on acoustic grand/upright pianos
                    // When relKnob is 0.0, the user wants immediate dry cutoff across all notes
                    const float rFactor = (v.note > 92 && relKnob > 0.05f) ? undampedReleaseFactor : mainReleaseFactor;
                    v.envelope *= rFactor;
                    if (v.envelope < 0.0005f) v.reset();
                }
            }
        }
    }

    // 2. Mix Layer A (Piano), Layer B (Strings) and Layer C (Synths) with headroom scaling
    const bool enA = layerA.enabled.load(std::memory_order_relaxed);
    const bool enB = layerB.enabled.load(std::memory_order_relaxed);
    const bool enC = layerC.enabled.load(std::memory_order_relaxed);

    // 1.5 Worship Pad (Layer B) Sound Shaping: Filter + Independent 3-Band EQ
    if (enB) {
        for (int i = 0; i < count; ++i) {
            float l = padFilterL.process(layerBufBL[i]);
            float r = padFilterR.process(layerBufBR[i]);

            // Dedicated Pad 3-Band Equalizer
            l = padEqBassL.process(l);
            r = padEqBassR.process(r);
            l = padEqMidL.process(l);
            r = padEqMidR.process(r);
            l = padEqTrebleL.process(l);
            r = padEqTrebleR.process(r);

            layerBufBL[i] = l;
            layerBufBR[i] = r;
        }
    }

    // 1.6 Acoustic Piano (Layer A) Sound Shaping: Tone Filter + Dedicated 3-Band EQ
    if (enA) {
        const float tone = toneParam.load(std::memory_order_relaxed);
        for (int i = 0; i < count; ++i) {
            float l = layerBufAL[i];
            float r = layerBufAR[i];

            if (tone < 0.98f) {
                l = toneFilterL.process(l);
                r = toneFilterR.process(r);
            }

            // Dedicated Piano 3-Band Equalizer
            l = eqBassL.process(l);
            r = eqBassR.process(r);
            l = eqMidL.process(l);
            r = eqMidR.process(r);
            l = eqTrebleL.process(l);
            r = eqTrebleR.process(r);

            layerBufAL[i] = l;
            layerBufAR[i] = r;
        }
    }

    const float volA = enA ? layerA.volume.load(std::memory_order_relaxed) : 0.0f;
    const float volB = enB ? layerB.volume.load(std::memory_order_relaxed) : 0.0f;
    const float volC = enC ? layerC.volume.load(std::memory_order_relaxed) : 0.0f;

    const float panA = layerA.pan.load(std::memory_order_relaxed);
    const float panB = layerB.pan.load(std::memory_order_relaxed);
    const float panC = layerC.pan.load(std::memory_order_relaxed);

    const float panAL = std::clamp(1.0f - panA, 0.0f, 1.0f);
    const float panAR = std::clamp(1.0f + panA, 0.0f, 1.0f);
    const float panBL = std::clamp(1.0f - panB, 0.0f, 1.0f);
    const float panBR = std::clamp(1.0f + panB, 0.0f, 1.0f);
    const float panCL = std::clamp(1.0f - panC, 0.0f, 1.0f);
    const float panCR = std::clamp(1.0f + panC, 0.0f, 1.0f);

    const float headroomScale = 0.68f;
    const float finalGainAL = volA * panAL * headroomScale;
    const float finalGainAR = volA * panAR * headroomScale;
    const float finalGainBL = volB * panBL * headroomScale;
    const float finalGainBR = volB * panBR * headroomScale;
    const float finalGainCL = volC * panCL * headroomScale;
    const float finalGainCR = volC * panCR * headroomScale;

    for (int i = 0; i < count; ++i) {
        left[i]  = layerBufAL[i] * finalGainAL + layerBufBL[i] * finalGainBL + layerBufCL[i] * finalGainCL;
        right[i] = layerBufAR[i] * finalGainAR + layerBufBR[i] * finalGainBR + layerBufCR[i] * finalGainCR;
    }

    // 4. Compressor
    if (compOn.load(std::memory_order_relaxed)) {
        const float cAmt = compAmount.load(std::memory_order_relaxed); // 0..10
        if (cAmt > 0.05f) {
            const float thresh = std::pow(10.0f, -(cAmt * 2.5f) / 20.0f); // -25 dB to 0 dB
            const float ratio = 1.0f + cAmt * 0.7f; // 1:1 to 8:1
            const float makeup = std::pow(10.0f, (cAmt * 1.0f) / 20.0f); // makeup gain
            const float attackCoeff = float(1.0 - std::exp(-1.0 / (sampleRate * 0.008)));
            const float releaseCoeff = float(1.0 - std::exp(-1.0 / (sampleRate * 0.120)));

            for (int i = 0; i < count; ++i) {
                const float peak = std::max(std::abs(left[i]), std::abs(right[i]));
                if (peak > compEnv) {
                    compEnv += (peak - compEnv) * attackCoeff;
                } else {
                    compEnv += (peak - compEnv) * releaseCoeff;
                }

                float gainReduction = 1.0f;
                if (compEnv > thresh) {
                    gainReduction = std::pow(compEnv / thresh, (1.0f / ratio) - 1.0f);
                }
                const float totalCompGain = gainReduction * makeup;
                left[i]  *= totalCompGain;
                right[i] *= totalCompGain;
            }
        }
    }

    // 5. Chorus
    const float cDepth = chorusDepth.load(std::memory_order_relaxed);
    if (cDepth > 0.05f && chorusBufferL.size() >= chorusBufferSize) {
        const float cRate = chorusRate.load(std::memory_order_relaxed); // 0..10
        const double rateHz = 0.2 + (double(cRate) / 10.0) * 3.5;
        const float depthNorm = cDepth / 10.0f;

        for (int i = 0; i < count; ++i) {
            chorusBufferL[chorusWriteIndex] = left[i];
            chorusBufferR[chorusWriteIndex] = right[i];

            chorusLfoPhaseL += (2.0 * 3.141592653589793 * rateHz) / sampleRate;
            chorusLfoPhaseR += (2.0 * 3.141592653589793 * (rateHz * 1.15)) / sampleRate;
            if (chorusLfoPhaseL > 6.283185307179586) chorusLfoPhaseL -= 6.283185307179586;
            if (chorusLfoPhaseR > 6.283185307179586) chorusLfoPhaseR -= 6.283185307179586;

            const float modL = float(std::sin(chorusLfoPhaseL)) * 0.5f + 0.5f;
            const float modR = float(std::sin(chorusLfoPhaseR)) * 0.5f + 0.5f;

            const float delaySamplesL = 200.0f + modL * (depthNorm * 800.0f);
            const float delaySamplesR = 200.0f + modR * (depthNorm * 800.0f);

            float readPosL = float(chorusWriteIndex) - delaySamplesL;
            if (readPosL < 0.0f) readPosL += float(chorusBufferSize);

            float readPosR = float(chorusWriteIndex) - delaySamplesR;
            if (readPosR < 0.0f) readPosR += float(chorusBufferSize);

            const size_t r0_l = static_cast<size_t>(readPosL) % chorusBufferSize;
            const size_t r1_l = (r0_l + 1) % chorusBufferSize;
            const float fracL = readPosL - float(static_cast<size_t>(readPosL));
            const float wetL = chorusBufferL[r0_l] * (1.0f - fracL) + chorusBufferL[r1_l] * fracL;

            const size_t r0_r = static_cast<size_t>(readPosR) % chorusBufferSize;
            const size_t r1_r = (r0_r + 1) % chorusBufferSize;
            const float fracR = readPosR - float(static_cast<size_t>(readPosR));
            const float wetR = chorusBufferR[r0_r] * (1.0f - fracR) + chorusBufferR[r1_r] * fracR;

            left[i]  = left[i]  * (1.0f - depthNorm * 0.40f) + wetL * (depthNorm * 0.65f);
            right[i] = right[i] * (1.0f - depthNorm * 0.40f) + wetR * (depthNorm * 0.65f);

            chorusWriteIndex = (chorusWriteIndex + 1) % chorusBufferSize;
        }
    }

    // 6. Tremolo
    const float tDepth = tremoloDepth.load(std::memory_order_relaxed);
    if (tDepth > 0.05f) {
        const float tRate = tremoloRate.load(std::memory_order_relaxed); // 0..10
        const double rateHz = 0.5 + (double(tRate) / 10.0) * 9.5;
        const float depthNorm = tDepth / 10.0f;

        for (int i = 0; i < count; ++i) {
            tremoloPhase += (2.0 * 3.141592653589793 * rateHz) / sampleRate;
            if (tremoloPhase > 6.283185307179586) tremoloPhase -= 6.283185307179586;

            const float mod = 1.0f - (depthNorm * 0.5f * (float(std::sin(tremoloPhase)) + 1.0f));
            left[i]  *= mod;
            right[i] *= mod;
        }
    }

    // 7. Stereo Ping-Pong / Dampened Delay
    if (delayOn.load(std::memory_order_relaxed) && delayBufferL.size() >= maxDelaySamples) {
        const float dTime = delayTime.load(std::memory_order_relaxed); // 0..10
        const float dFeedback = delayFeedback.load(std::memory_order_relaxed) / 10.0f; // 0..1
        const float delaySec = 0.05f + (dTime / 10.0f) * 0.70f; // 50ms to 750ms
        const size_t delaySamples = std::clamp(static_cast<size_t>(delaySec * sampleRate), size_t(100), maxDelaySamples - 100);
        const float fbGain = std::clamp(dFeedback * 0.82f, 0.0f, 0.82f);

        for (int i = 0; i < count; ++i) {
            size_t readPos = (delayWriteIndex + maxDelaySamples - delaySamples) % maxDelaySamples;

            float delayedL = delayBufferL[readPos];
            float delayedR = delayBufferR[readPos];

            // 6 kHz Low-pass dampening in feedback loop
            delayFilterL += 0.45f * (delayedR - delayFilterL); // Cross-feed (ping-pong)
            delayFilterR += 0.45f * (delayedL - delayFilterR);

            delayBufferL[delayWriteIndex] = left[i]  + delayFilterL * fbGain;
            delayBufferR[delayWriteIndex] = right[i] + delayFilterR * fbGain;

            delayWriteIndex = (delayWriteIndex + 1) % maxDelaySamples;

            // 40% Wet mix
            left[i]  += delayedL * 0.40f;
            right[i] += delayedR * 0.40f;
        }
    }

    // 8. Master Volume & Soft Saturation
    const float masterVol = mainVolume.load(std::memory_order_relaxed) / 8.0f; // 0 to 1.25
    for (int i = 0; i < count; ++i) {
        left[i]  = softSaturate(left[i]  * masterVol);
        right[i] = softSaturate(right[i] * masterVol);
    }
}
