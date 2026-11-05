#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
// Asynchronous Layer Loader Thread (Layers A=Piano, B=Strings, C=Synth)
//==============================================================================
NordPianoPrototypeAudioProcessor::LayerLoaderThread::LayerLoaderThread(NordPianoPrototypeAudioProcessor& proc, int layerIdx)
    : juce::Thread("NordPianoLoaderThread_" + juce::String(layerIdx)), processor(proc), layerIndex(layerIdx) {
}

NordPianoPrototypeAudioProcessor::LayerLoaderThread::~LayerLoaderThread() {
    stopThread(4000);
}

void NordPianoPrototypeAudioProcessor::LayerLoaderThread::triggerLoad(const juce::File& file) {
    {
        const juce::ScopedLock sl(queueLock);
        targetFile = file;
    }
    if (layerIndex == 0) processor.loadingA.store(true, std::memory_order_relaxed);
    else if (layerIndex == 1) processor.loadingB.store(true, std::memory_order_relaxed);
    else processor.loadingC.store(true, std::memory_order_relaxed);
    wakeEvent.signal();
}

void NordPianoPrototypeAudioProcessor::LayerLoaderThread::run() {
    while (!threadShouldExit()) {
        wakeEvent.wait(-1);
        if (threadShouldExit()) break;

        juce::File fileToLoad;
        {
            const juce::ScopedLock sl(queueLock);
            fileToLoad = targetFile;
        }

        if (!fileToLoad.existsAsFile()) {
            if (layerIndex == 0) processor.loadingA.store(false, std::memory_order_relaxed);
            else if (layerIndex == 1) processor.loadingB.store(false, std::memory_order_relaxed);
            else processor.loadingC.store(false, std::memory_order_relaxed);
            continue;
        }

        const std::string fullPath = fileToLoad.getFullPathName().toStdString();
        const juce::String fn = fileToLoad.getFileNameWithoutExtension();

        juce::String category = "Acoustic Piano";
        if (layerIndex == 1) {
            category = "Orchestral / Strings";
        } else if (layerIndex == 2) {
            category = "Sample Synth / Lead";
        } else if (fileToLoad.getFileExtension().equalsIgnoreCase(".nsmp") ||
                   fileToLoad.getFileExtension().equalsIgnoreCase(".nsmp4")) {
            category = "Sample Synth";
        }

        try {
            // Check cache first (instant switch)
            std::shared_ptr<const npno::Bank> bank = processor.getCachedBank(fullPath);
            if (!bank) {
                auto newBank = npno::loadBank(fullPath);
                if (newBank && !newBank->samples.empty()) {
                    processor.cacheBank(fullPath, newBank);
                    bank = newBank;
                }
            }

            if (bank && !bank->samples.empty()) {
                if (layerIndex == 0) {
                    processor.engine.setLayerABank(bank, fn.toStdString(), fullPath, category.toStdString());
                    const juce::ScopedLock sl(processor.infoLock);
                    processor.layerAName = fn;
                    processor.layerADetails = category + " | Nord Piano Library";
                    processor.status = "Piano Carregado: " + fn;
                } else if (layerIndex == 1) {
                    processor.engine.setLayerBBank(bank, fn.toStdString(), fullPath, category.toStdString());
                    const juce::ScopedLock sl(processor.infoLock);
                    processor.layerBName = fn;
                    processor.layerBDetails = category + " | Nord Sound Library";
                    processor.status = "Strings Carregado: " + fn;
                } else {
                    processor.engine.setLayerCBank(bank, fn.toStdString(), fullPath, category.toStdString());
                    const juce::ScopedLock sl(processor.infoLock);
                    processor.layerCName = fn;
                    processor.layerCDetails = category + " | Nord Sample Library";
                    processor.status = "Synth Carregado: " + fn;
                }
            } else {
                const juce::ScopedLock sl(processor.infoLock);
                processor.status = "Erro: Amostras nao encontradas em " + fileToLoad.getFileName();
            }
        } catch (const std::exception& e) {
            const juce::ScopedLock sl(processor.infoLock);
            processor.status = "Erro ao carregar: " + juce::String(e.what());
        } catch (...) {
            const juce::ScopedLock sl(processor.infoLock);
            processor.status = "Erro desconhecido ao decodificar arquivo.";
        }

        if (layerIndex == 0) processor.loadingA.store(false, std::memory_order_relaxed);
        else if (layerIndex == 1) processor.loadingB.store(false, std::memory_order_relaxed);
        else processor.loadingC.store(false, std::memory_order_relaxed);
    }
}

//==============================================================================
// Background Preloader Thread (Pre-caches priority instruments into RAM)
//==============================================================================
NordPianoPrototypeAudioProcessor::PreloaderThread::PreloaderThread(NordPianoPrototypeAudioProcessor& proc)
    : juce::Thread("NordPianoPreloaderThread"), processor(proc) {
}

NordPianoPrototypeAudioProcessor::PreloaderThread::~PreloaderThread() {
    stopThread(3000);
}

void NordPianoPrototypeAudioProcessor::PreloaderThread::run() {
    const std::vector<std::string> priorityList = {
        "C:\\Projetos\\Nord_Piano_Kit\\Pianos_Teste\\Black Upright Med 6.1.npno",
        "E:\\Disco Elisson\\NORD\\Upright Pianos\\Black Upright Med 6.1.npno",
        "E:\\Disco Elisson\\NORD\\Grand Pianos\\Soft Grand XL 6.2.npno",
        "E:\\Disco Elisson\\NORD\\Grand Pianos\\White Grand XL 6.3.npno",
        "E:\\Disco Elisson\\NORD\\Upright Pianos\\Felt Upright XL 6.2.npno",
        "E:\\Disco Elisson\\NORD\\Grand Pianos\\Grand Imperial XL 6.1.npno",
        "E:\\Disco Elisson\\NORD\\Grand Pianos\\Italian Grand XL 6.1.npno",
        "E:\\Disco Elisson\\NORD\\Grand Pianos\\Royal Grand 3D XL 6.1.npno"
    };

    for (const auto& path : priorityList) {
        if (threadShouldExit()) break;
        if (processor.isBankCached(path)) continue;
        juce::File f(path);
        if (!f.existsAsFile()) continue;

        try {
            auto b = npno::loadBank(path);
            if (b && !b->samples.empty()) {
                processor.cacheBank(path, b);
            }
        } catch (...) {}

        juce::Thread::sleep(40);
    }
}

//==============================================================================
// APVTS Parameter Layout Factory (32 Parameters for Ableton Live Mapping)
//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout NordPianoPrototypeAudioProcessor::createParameterLayout() {
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // 1. 3-Band Equalizer (Piano)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"eq_treble", 1}, "EQ Treble", juce::NormalisableRange<float>(-15.0f, 15.0f, 0.1f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"eq_mid", 1}, "EQ Mid", juce::NormalisableRange<float>(-15.0f, 15.0f, 0.1f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"eq_bass", 1}, "EQ Bass", juce::NormalisableRange<float>(-15.0f, 15.0f, 0.1f), 0.0f));

    // Pad Equalizer
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"pad_eq_treble", 1}, "Pad EQ Treble", juce::NormalisableRange<float>(-15.0f, 15.0f, 0.1f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"pad_eq_mid", 1}, "Pad EQ Mid", juce::NormalisableRange<float>(-15.0f, 15.0f, 0.1f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"pad_eq_bass", 1}, "Pad EQ Bass", juce::NormalisableRange<float>(-15.0f, 15.0f, 0.1f), 0.0f));

    // 2. Master Section
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"main_volume", 1}, "Master Volume", juce::NormalisableRange<float>(0.0f, 10.0f, 0.05f), 8.0f));
    params.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID{"transpose", 1}, "Transpose", -12, 12, 0));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"tune", 1}, "Fine Tune", juce::NormalisableRange<float>(-1.0f, 1.0f, 0.01f), 0.0f));

    // 3. Piano Section (Module 2)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"tone", 1}, "Tone / Cutoff", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.75f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"piano_select", 1}, "Piano Select", juce::NormalisableRange<float>(0.0f, 10.0f, 1.0f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"piano_release", 1}, "Piano Release", juce::NormalisableRange<float>(0.1f, 2.0f, 0.01f), 0.6f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"piano_vol", 1}, "Piano Volume", juce::NormalisableRange<float>(0.0f, 1.5f, 0.01f), 0.85f));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"piano_on", 1}, "Piano Enabled", true));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"piano_res_amount", 1}, "Resonance Amount", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"piano_res", 1}, "String Resonance", true));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"piano_pedal", 1}, "Pedal Noise", true));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"piano_soft_rel", 1}, "Soft Release", true));
    params.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID{"piano_timbre", 1}, "Piano Timbre", 0, 3, 0));
    params.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID{"piano_touch", 1}, "Touch Curve", 0, 2, 1));

    // 4. Dedicated Worship Pad Section (Layer B)
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"pad_on", 1}, "Worship Pad Enabled", false));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"pad_vol", 1}, "Pad Level", juce::NormalisableRange<float>(0.0f, 1.5f, 0.01f), 0.70f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"pad_cutoff", 1}, "Pad Cutoff", juce::NormalisableRange<float>(150.0f, 14000.0f, 1.0f, 0.35f), 4500.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"pad_resonance", 1}, "Pad Resonance", juce::NormalisableRange<float>(0.5f, 5.0f, 0.05f), 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"pad_attack", 1}, "Pad Attack", juce::NormalisableRange<float>(0.01f, 3.5f, 0.01f), 0.35f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"pad_release", 1}, "Pad Release", juce::NormalisableRange<float>(0.1f, 5.5f, 0.05f), 2.0f));

    // Legacy Strings parameters for session compatibility
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"strings_on", 1}, "Strings Enabled", true));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"strings_vol", 1}, "Strings Volume", juce::NormalisableRange<float>(0.0f, 1.5f, 0.01f), 0.70f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"strings_release", 1}, "Strings Release", juce::NormalisableRange<float>(0.1f, 4.0f, 0.05f), 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"strings_mono", 1}, "Strings Mono", false));

    // 5. Synth / Leads Section (Module 4 Bottom)
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"synth_on", 1}, "Synth Enabled", false));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"synth_vol", 1}, "Synth Volume", juce::NormalisableRange<float>(0.0f, 1.5f, 0.01f), 0.80f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"synth_glide", 1}, "Synth Glide", juce::NormalisableRange<float>(0.005f, 0.45f, 0.005f), 0.05f));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"synth_porta", 1}, "Synth Portamento On", true));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"synth_mono", 1}, "Synth Mono", true));

    // 6. Dynamics & Compressor
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"dynamics", 1}, "Dynamics", juce::NormalisableRange<float>(0.2f, 2.2f, 0.05f), 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"dynamics_hard", 1}, "Dynamics Hard", false));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"comp_amount", 1}, "Compressor Amount", juce::NormalisableRange<float>(0.0f, 10.0f, 0.1f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"comp_on", 1}, "Compressor On", false));

    // 7. Chorus
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"chorus_depth", 1}, "Chorus Depth", juce::NormalisableRange<float>(0.0f, 10.0f, 0.1f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"chorus_rate", 1}, "Chorus Rate", juce::NormalisableRange<float>(0.0f, 10.0f, 0.1f), 2.5f));

    // 8. Tremolo
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"tremolo_depth", 1}, "Tremolo Depth", juce::NormalisableRange<float>(0.0f, 10.0f, 0.1f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"tremolo_rate", 1}, "Tremolo Rate", juce::NormalisableRange<float>(0.0f, 10.0f, 0.1f), 4.0f));

    // 9. Stereo Delay
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"delay_time", 1}, "Delay Time", juce::NormalisableRange<float>(0.0f, 10.0f, 0.1f), 3.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"delay_feedback", 1}, "Delay Feedback", juce::NormalisableRange<float>(0.0f, 10.0f, 0.1f), 3.5f));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"delay_on", 1}, "Delay On", false));

    // 10. Nord Stage Reverb Section (Stage, Hall, Cathedral, Room, Booth + Variations)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"reverb_amount", 1}, "Reverb Amount", juce::NormalisableRange<float>(0.0f, 10.0f, 0.1f), 3.0f));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"reverb_type", 1}, "Reverb Type",
        juce::StringArray{"Stage", "Hall", "Cathedral", "Room", "Booth"}, 1));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"reverb_variation", 1}, "Reverb Variation",
        juce::StringArray{"Normal", "Bright", "Dark"}, 0));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"reverb_tone", 1}, "Reverb Tone", juce::NormalisableRange<float>(0.0f, 10.0f, 0.1f), 5.0f));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"reverb_on", 1}, "Reverb On", true));

    // 11. Crossfader Mix
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"mix_balance", 1}, "Mix Balance", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));

    return { params.begin(), params.end() };
}

void NordPianoPrototypeAudioProcessor::parameterChanged(const juce::String& parameterID, float newValue) {
    if (parameterID == "eq_treble") engine.setEqTreble(newValue);
    else if (parameterID == "eq_mid") engine.setEqMid(newValue);
    else if (parameterID == "eq_bass") engine.setEqBass(newValue);
    else if (parameterID == "pad_eq_treble") engine.setPadEqTreble(newValue);
    else if (parameterID == "pad_eq_mid") engine.setPadEqMid(newValue);
    else if (parameterID == "pad_eq_bass") engine.setPadEqBass(newValue);
    else if (parameterID == "main_volume") engine.setMainVolume(newValue);
    else if (parameterID == "transpose") engine.setTranspose(int(std::round(newValue)));
    else if (parameterID == "tune") engine.setTune(newValue);
    else if (parameterID == "piano_release") engine.setRelease(newValue);
    else if (parameterID == "piano_vol") engine.getLayerAConfig().volume.store(newValue, std::memory_order_relaxed);
    else if (parameterID == "piano_on") engine.getLayerAConfig().enabled.store(newValue > 0.5f, std::memory_order_relaxed);
    else if (parameterID == "piano_res_amount") engine.setResonance(newValue);
    else if (parameterID == "piano_res") engine.setStringRes(newValue > 0.5f);
    else if (parameterID == "piano_pedal") engine.setPedalNoise(newValue > 0.5f);
    else if (parameterID == "piano_soft_rel") engine.setSoftRelease(newValue > 0.5f);
    else if (parameterID == "piano_timbre") engine.setPianoTimbre(int(std::round(newValue)));
    else if (parameterID == "piano_touch") engine.setTouchCurve(int(std::round(newValue)));
    else if (parameterID == "pad_vol" || parameterID == "strings_vol") engine.getLayerBConfig().volume.store(newValue, std::memory_order_relaxed);
    else if (parameterID == "pad_on" || parameterID == "strings_on") engine.getLayerBConfig().enabled.store(newValue > 0.5f, std::memory_order_relaxed);
    else if (parameterID == "pad_cutoff") engine.setPadCutoff(newValue);
    else if (parameterID == "pad_resonance") engine.setPadResonance(newValue);
    else if (parameterID == "pad_attack") engine.setPadAttack(newValue);
    else if (parameterID == "pad_release" || parameterID == "strings_release") engine.setPadRelease(newValue);
    else if (parameterID == "strings_mono") {
        const bool m = (newValue > 0.5f);
        engine.getLayerBConfig().mono.store(m, std::memory_order_relaxed);
        engine.getLayerBConfig().stereo.store(!m, std::memory_order_relaxed);
    }
    else if (parameterID == "synth_vol") engine.getLayerCConfig().volume.store(newValue, std::memory_order_relaxed);
    else if (parameterID == "synth_on") engine.getLayerCConfig().enabled.store(newValue > 0.5f, std::memory_order_relaxed);
    else if (parameterID == "synth_glide") engine.setGlide(newValue);
    else if (parameterID == "synth_porta") engine.setPortamentoOn(newValue > 0.5f);
    else if (parameterID == "synth_mono") {
        const bool m = (newValue > 0.5f);
        engine.getLayerCConfig().mono.store(m, std::memory_order_relaxed);
        engine.getLayerCConfig().stereo.store(!m, std::memory_order_relaxed);
    }
    else if (parameterID == "tone" || parameterID == "piano_tone") engine.setTone(newValue);
    else if (parameterID == "dynamics") engine.setDynamics(newValue);
    else if (parameterID == "dynamics_hard") engine.setDynamicsHard(newValue > 0.5f);
    else if (parameterID == "comp_amount") engine.setCompressorAmount(newValue);
    else if (parameterID == "comp_on") engine.setCompressorOn(newValue > 0.5f);
    else if (parameterID == "chorus_depth") engine.setChorusDepth(newValue);
    else if (parameterID == "chorus_rate") engine.setChorusRate(newValue);
    else if (parameterID == "tremolo_depth") engine.setTremoloDepth(newValue);
    else if (parameterID == "tremolo_rate") engine.setTremoloRate(newValue);
    else if (parameterID == "delay_time") engine.setDelayTime(newValue);
    else if (parameterID == "delay_feedback") engine.setDelayFeedback(newValue);
    else if (parameterID == "delay_on") engine.setDelayOn(newValue > 0.5f);
    else if (parameterID == "reverb_amount") engine.setReverbAmount(newValue);
    else if (parameterID == "reverb_on") engine.setReverbOn(newValue > 0.5f);
    else if (parameterID == "reverb_type") nordReverb.setType(int(newValue));
    else if (parameterID == "reverb_variation") nordReverb.setVariation(int(newValue));
    else if (parameterID == "reverb_tone") nordReverb.setTone(newValue);
    else if (parameterID == "mix_balance") engine.setMixBalance(newValue);
}

//==============================================================================
// NordPianoPrototypeAudioProcessor Implementation
//==============================================================================
NordPianoPrototypeAudioProcessor::NordPianoPrototypeAudioProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", createParameterLayout()) {

    // Register APVTS parameter listeners for live updates
    for (auto* param : getParameters()) {
        if (auto* p = dynamic_cast<juce::AudioProcessorParameterWithID*>(param)) {
            apvts.addParameterListener(p->paramID, this);
        }
    }

    loaderThreadA = std::make_unique<LayerLoaderThread>(*this, 0); // Piano
    loaderThreadB = std::make_unique<LayerLoaderThread>(*this, 1); // Strings
    loaderThreadC = std::make_unique<LayerLoaderThread>(*this, 2); // Synth
    loaderThreadA->startThread(juce::Thread::Priority::normal);
    loaderThreadB->startThread(juce::Thread::Priority::normal);
    loaderThreadC->startThread(juce::Thread::Priority::normal);

    initDefaultMidiMappings();
    loadPresetsFromDisk();
    rescanSoundLibraries();
    initDefaultPresets();

    // Auto-load default Piano (Layer A) - 100% Focus on Nord Piano Engine
    engine.getLayerAConfig().enabled.store(true);
    engine.getLayerBConfig().enabled.store(false);
    engine.getLayerCConfig().enabled.store(false);

    juce::File defaultPiano;
    if (juce::File("C:\\Projetos\\Nord_Piano_Kit\\Pianos_Teste\\Black Upright Med 6.1.npno").existsAsFile()) {
        defaultPiano = juce::File("C:\\Projetos\\Nord_Piano_Kit\\Pianos_Teste\\Black Upright Med 6.1.npno");
    } else if (juce::File("E:\\Disco Elisson\\NORD\\Upright Pianos\\Black Upright Med 6.1.npno").existsAsFile()) {
        defaultPiano = juce::File("E:\\Disco Elisson\\NORD\\Upright Pianos\\Black Upright Med 6.1.npno");
    } else if (!pianoSounds.empty()) {
        defaultPiano = pianoSounds[0];
    }
    if (defaultPiano.existsAsFile()) {
        loadLayerAAsync(defaultPiano);
    }

    // Auto-load default Worship Pad (Layer B)
    juce::File defaultPad;
    if (juce::File("E:\\Disco Elisson\\NORD\\Synth - Nord Piano 6\\Celestial Pad_NLA1 AW 4.1.nsmp4").existsAsFile()) {
        defaultPad = juce::File("E:\\Disco Elisson\\NORD\\Synth - Nord Piano 6\\Celestial Pad_NLA1 AW 4.1.nsmp4");
    } else if (!padSounds.empty()) {
        defaultPad = padSounds[0];
    }
    if (defaultPad.existsAsFile()) {
        loadLayerBAsync(defaultPad);
    }

    // Start background preloader to pre-cache remaining banks into RAM
    preloaderThread = std::make_unique<PreloaderThread>(*this);
    preloaderThread->startThread(juce::Thread::Priority::low);
}

NordPianoPrototypeAudioProcessor::~NordPianoPrototypeAudioProcessor() {
    for (auto* param : getParameters()) {
        if (auto* p = dynamic_cast<juce::AudioProcessorParameterWithID*>(param)) {
            apvts.removeParameterListener(p->paramID, this);
        }
    }

    preloaderThread.reset();
    loaderThreadA.reset();
    loaderThreadB.reset();
    loaderThreadC.reset();
}

std::shared_ptr<const npno::Bank> NordPianoPrototypeAudioProcessor::getCachedBank(const std::string& path) const {
    const juce::ScopedLock sl(cacheLock);
    auto it = bankCache.find(path);
    if (it != bankCache.end()) return it->second;
    return nullptr;
}

void NordPianoPrototypeAudioProcessor::cacheBank(const std::string& path, std::shared_ptr<const npno::Bank> bank) {
    if (!bank) return;
    const juce::ScopedLock sl(cacheLock);
    bankCache[path] = bank;
}

bool NordPianoPrototypeAudioProcessor::isBankCached(const std::string& path) const {
    const juce::ScopedLock sl(cacheLock);
    return bankCache.find(path) != bankCache.end();
}

void NordPianoPrototypeAudioProcessor::initDefaultMidiMappings() {
    const juce::ScopedLock sl(midiMapLock);
    ccToParam.clear();
    paramToCc.clear();
    noteToParam.clear();
    paramToNote.clear();

    // Ableton-standard default CC assignments
    auto map = [this](int cc, const juce::String& p) {
        ccToParam[cc] = p;
        paramToCc[p] = cc;
    };

    map(7,  "main_volume");
    map(10, "mix_balance");
    map(71, "piano_release");
    map(74, "pad_cutoff");
    map(72, "pad_release");
    map(73, "pad_attack");
    map(91, "reverb_amount");
    map(92, "reverb_tone");
    map(93, "chorus_depth");
    map(94, "delay_time");
}

void NordPianoPrototypeAudioProcessor::mapCcToParameter(int cc, const juce::String& paramId) {
    const juce::ScopedLock sl(midiMapLock);
    if (paramId.isEmpty()) return;

    // Remove any previous parameter assigned to this CC
    auto oldParam = ccToParam.find(cc);
    if (oldParam != ccToParam.end()) {
        paramToCc.erase(oldParam->second);
    }

    // Remove any previous CC assigned to this parameter
    auto oldCc = paramToCc.find(paramId);
    if (oldCc != paramToCc.end()) {
        ccToParam.erase(oldCc->second);
    }

    // Also remove from Note mappings if it was mapped to a note
    auto oldNote = paramToNote.find(paramId);
    if (oldNote != paramToNote.end()) {
        noteToParam.erase(oldNote->second);
        paramToNote.erase(oldNote);
    }

    ccToParam[cc] = paramId;
    paramToCc[paramId] = cc;
}

void NordPianoPrototypeAudioProcessor::mapNoteToParameter(int note, const juce::String& paramId) {
    const juce::ScopedLock sl(midiMapLock);
    if (paramId.isEmpty()) return;

    auto oldParam = noteToParam.find(note);
    if (oldParam != noteToParam.end()) {
        paramToNote.erase(oldParam->second);
    }

    auto oldNote = paramToNote.find(paramId);
    if (oldNote != paramToNote.end()) {
        noteToParam.erase(oldNote->second);
    }

    // Also remove from CC mappings if it was mapped to a CC
    auto oldCc = paramToCc.find(paramId);
    if (oldCc != paramToCc.end()) {
        ccToParam.erase(oldCc->second);
        paramToCc.erase(oldCc);
    }

    noteToParam[note] = paramId;
    paramToNote[paramId] = note;
}

void NordPianoPrototypeAudioProcessor::unmapParameter(const juce::String& paramId) {
    const juce::ScopedLock sl(midiMapLock);
    auto it = paramToCc.find(paramId);
    if (it != paramToCc.end()) {
        ccToParam.erase(it->second);
        paramToCc.erase(it);
    }
    auto itN = paramToNote.find(paramId);
    if (itN != paramToNote.end()) {
        noteToParam.erase(itN->second);
        paramToNote.erase(itN);
    }
}

void NordPianoPrototypeAudioProcessor::clearAllMidiMappings() {
    const juce::ScopedLock sl(midiMapLock);
    ccToParam.clear();
    paramToCc.clear();
    noteToParam.clear();
    paramToNote.clear();
}

int NordPianoPrototypeAudioProcessor::getMappedCcForParam(const juce::String& paramId) const {
    const juce::ScopedLock sl(midiMapLock);
    auto it = paramToCc.find(paramId);
    if (it != paramToCc.end()) return it->second;
    return -1;
}

int NordPianoPrototypeAudioProcessor::getMappedNoteForParam(const juce::String& paramId) const {
    const juce::ScopedLock sl(midiMapLock);
    auto it = paramToNote.find(paramId);
    if (it != paramToNote.end()) return it->second;
    return -1;
}

juce::String NordPianoPrototypeAudioProcessor::getMappingBadgeForParam(const juce::String& paramId) const {
    const juce::ScopedLock sl(midiMapLock);
    auto itCc = paramToCc.find(paramId);
    if (itCc != paramToCc.end()) return "CC " + juce::String(itCc->second);
    auto itN = paramToNote.find(paramId);
    if (itN != paramToNote.end()) return "N " + juce::String(itN->second);
    return {};
}

void NordPianoPrototypeAudioProcessor::triggerVirtualAction(const juce::String& mappedParam, int val) {
    if (mappedParam == "prog_1") { if (val >= 64) selectProgram(0); return; }
    if (mappedParam == "prog_2") { if (val >= 64) selectProgram(1); return; }
    if (mappedParam == "prog_3") { if (val >= 64) selectProgram(2); return; }
    if (mappedParam == "prog_4") { if (val >= 64) selectProgram(3); return; }
    if (mappedParam == "prog_5") { if (val >= 64) selectProgram(4); return; }
    if (mappedParam == "prog_6") { if (val >= 64) selectProgram(5); return; }
    if (mappedParam == "prog_7") { if (val >= 64) selectProgram(6); return; }
    if (mappedParam == "prog_8") { if (val >= 64) selectProgram(7); return; }
    if (mappedParam == "prog_store" || mappedParam == "prog_save") {
        if (val >= 64) saveCurrentToProgram(currentProgramIndex.load(std::memory_order_relaxed));
        return;
    }
    if (mappedParam == "piano_prev") { if (val >= 64) stepSoundLayerA(-1); return; }
    if (mappedParam == "piano_next") { if (val >= 64) stepSoundLayerA(1); return; }
    if (mappedParam == "pad_prev") { if (val >= 64) stepSoundLayerB(-1); return; }
    if (mappedParam == "pad_next") { if (val >= 64) stepSoundLayerB(1); return; }
    if (mappedParam == "eq_select_piano") { if (val >= 64) requestEqTarget(0); return; }
    if (mappedParam == "eq_select_pad") { if (val >= 64) requestEqTarget(1); return; }

    if (mappedParam == "touch_1") { if (val >= 64) engine.setTouchCurve(1); return; }
    if (mappedParam == "touch_2") { if (val >= 64) engine.setTouchCurve(2); return; }
    if (mappedParam == "touch_3") { if (val >= 64) engine.setTouchCurve(3); return; }

    if (mappedParam == "piano_res") {
        if (val >= 64) engine.setStringRes(!engine.isStringResOn());
        return;
    }
    if (mappedParam == "piano_pedal") {
        if (val >= 64) engine.setPedalNoise(!engine.isPedalNoiseOn());
        return;
    }
    if (mappedParam == "piano_soft_rel") {
        if (val >= 64) engine.setSoftRelease(!engine.isSoftRelease());
        return;
    }
    if (mappedParam == "piano_cat_grand" || mappedParam == "piano_cat_upright") {
        if (val >= 64) stepSoundLayerA(1);
        return;
    }
    if (mappedParam == "piano_on") {
        if (val >= 64) engine.getLayerAConfig().enabled.store(!engine.getLayerAConfig().enabled.load(std::memory_order_relaxed), std::memory_order_relaxed);
        return;
    }
    if (mappedParam == "pad_on") {
        if (val >= 64) engine.getLayerBConfig().enabled.store(!engine.getLayerBConfig().enabled.load(std::memory_order_relaxed), std::memory_order_relaxed);
        return;
    }
    if (mappedParam == "comp_on") {
        if (val >= 64) engine.setCompressorOn(!engine.isCompressorOn());
        return;
    }
    if (mappedParam == "delay_on") {
        if (val >= 64) engine.setDelayOn(!engine.isDelayOn());
        return;
    }
    if (mappedParam == "reverb_on") {
        if (val >= 64) engine.setReverbOn(!engine.isReverbOn());
        return;
    }
    if (mappedParam == "reverb_type") {
        if (val >= 64) nordReverb.setType(nordReverb.getType() == 0 ? 1 : 0);
        return;
    }
    if (mappedParam == "reverb_variation") {
        if (val >= 64) nordReverb.setVariation(nordReverb.getVariation() == 0 ? 1 : 0);
        return;
    }

    if (auto* param = apvts.getParameter(mappedParam)) {
        if (auto* bp = dynamic_cast<juce::AudioParameterBool*>(param)) {
            bp->setValueNotifyingHost(val >= 64 ? 1.0f : 0.0f);
        } else {
            param->setValueNotifyingHost(float(val) / 127.0f);
        }
    }
}

void NordPianoPrototypeAudioProcessor::initDefaultPresets() {
    for (int i = 0; i < 8; ++i) {
        presets[size_t(i)].name = "Program " + juce::String(i + 1);
        presets[size_t(i)].layerAPath = "";
        presets[size_t(i)].layerBPath = "";
        presets[size_t(i)].layerCPath = "";
        presets[size_t(i)].volA = 0.85f;
        presets[size_t(i)].volB = 0.0f;
        presets[size_t(i)].volC = 0.0f;
        presets[size_t(i)].enabledA = true;
        presets[size_t(i)].enabledB = false;
        presets[size_t(i)].enabledC = false;
        presets[size_t(i)].monoC = false;
    }

    auto findPiano = [this](const juce::String& keyword) -> juce::File {
        for (const auto& f : pianoSounds) {
            if (f.getFileNameWithoutExtension().containsIgnoreCase(keyword)) return f;
        }
        return juce::File();
    };

    juce::File defaultPianoFile = juce::File("C:\\Projetos\\Nord_Piano_Kit\\Pianos_Teste\\Black Upright Med 6.1.npno");
    if (!defaultPianoFile.existsAsFile()) defaultPianoFile = juce::File("E:\\Disco Elisson\\NORD\\Upright Pianos\\Black Upright Med 6.1.npno");
    if (!defaultPianoFile.existsAsFile() && !pianoSounds.empty()) defaultPianoFile = pianoSounds[0];
    const juce::String defaultPianoPath = defaultPianoFile.getFullPathName();

    auto assignPreset = [&](int idx, const juce::String& name, const juce::String& cat, const juce::String& keyword) {
        presets[idx].name = name;
        presets[idx].category = cat;
        presets[idx].mixBalance = 0.0f;
        juce::File pf = findPiano(keyword);
        if (pf.existsAsFile()) {
            presets[idx].layerAName = pf.getFileNameWithoutExtension();
            presets[idx].layerAPath = pf.getFullPathName();
        } else {
            presets[idx].layerAName = defaultPianoFile.getFileNameWithoutExtension();
            presets[idx].layerAPath = defaultPianoPath;
        }
    };

    assignPreset(0, "Program 1", "Concert Acoustic Grand", "Grand Imperial");
    assignPreset(1, "Program 2", "Italian Concert Grand", "Italian Grand");
    assignPreset(2, "Program 3", "Soft Grand Acoustic", "Soft Grand");
    assignPreset(3, "Program 4", "White Grand Acoustic", "White Grand");
    assignPreset(4, "Program 5", "Felt Upright Intimate", "Felt Upright");
    assignPreset(5, "Program 6", "Royal Grand 3D Stage", "Royal Grand");
    assignPreset(6, "Program 7", "Velvet Grand Studio", "Velvet Grand");
    presets[6].reverbAmount = 4.5f;
    assignPreset(7, "Program 8", "Black Upright Vintage", "Black Upright");
    presets[7].reverbOn = false;

    // Load any saved custom user programs from disk
    loadPresetsFromDisk();
}

void NordPianoPrototypeAudioProcessor::rescanSoundLibraries() {
    discoveredSounds.clear();
    pianoSounds.clear();
    stringSounds.clear();
    synthSounds.clear();
    padSounds.clear();

    juce::StringArray dirs;

    // 1. Custom user selected folder (persisted)
    if (customLibraryFolder.isNotEmpty()) {
        dirs.add(customLibraryFolder);
    }

    // 2. Portable locations relative to executable / VST3
    auto exeDir = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory();
    dirs.add(exeDir.getFullPathName());
    dirs.add(exeDir.getChildFile("Sounds").getFullPathName());
    dirs.add(exeDir.getChildFile("Sons").getFullPathName());
    dirs.add(exeDir.getChildFile("NORD").getFullPathName());
    dirs.add(exeDir.getChildFile("Pianos").getFullPathName());

    auto appDir = juce::File::getSpecialLocation(juce::File::currentApplicationFile).getParentDirectory();
    dirs.add(appDir.getFullPathName());
    dirs.add(appDir.getChildFile("Sounds").getFullPathName());
    dirs.add(appDir.getChildFile("Sons").getFullPathName());
    dirs.add(appDir.getChildFile("NORD").getFullPathName());

    // 3. Standard common Windows system directories
    dirs.add("C:\\Nord Pianos\\Sounds");
    dirs.add("C:\\Nord Pianos");
    dirs.add("C:\\Nord_Pianos\\Sounds");
    dirs.add("C:\\Nord_Pianos");
    dirs.add(juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("Nord Pianos").getChildFile("Sounds").getFullPathName());
    dirs.add(juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("Nord Pianos").getFullPathName());
    dirs.add(juce::File::getSpecialLocation(juce::File::commonDocumentsDirectory).getChildFile("Nord Pianos").getChildFile("Sounds").getFullPathName());
    dirs.add(juce::File::getSpecialLocation(juce::File::commonDocumentsDirectory).getChildFile("Nord Pianos").getFullPathName());

#if JUCE_MAC
    auto userMusic = juce::File::getSpecialLocation(juce::File::userMusicDirectory);
    dirs.add(userMusic.getChildFile("Nord Pianos").getChildFile("Sounds").getFullPathName());
    dirs.add(userMusic.getChildFile("Nord Pianos").getFullPathName());
    dirs.add("/Library/Application Support/Nord Pianos/Sounds");
    dirs.add("/Library/Audio/Presets/Nord Pianos/Sounds");
#endif

    // 4. Developer / Machine default paths
    dirs.add("C:\\Projetos\\Nord_Piano_Kit\\Pianos_Teste");
    dirs.add("E:\\Disco Elisson\\NORD");
    dirs.add("E:\\Disco Elisson\\Timbres");

    std::vector<juce::File> allRaw;
    for (const auto& d : dirs) {
        juce::File dir(d);
        if (dir.isDirectory()) {
            auto files = dir.findChildFiles(juce::File::findFiles, true, "*.npno;*.nsmp;*.nsmp4");
            for (const auto& f : files) {
                bool dup = false;
                for (const auto& existing : allRaw) {
                    if (existing.getFileName().equalsIgnoreCase(f.getFileName())) {
                        dup = true;
                        break;
                    }
                }
                if (!dup) allRaw.push_back(f);
            }
        }
    }

    for (const auto& f : allRaw) {
        const juce::String ext = f.getFileExtension().toLowerCase();
        const juce::String fn = f.getFileNameWithoutExtension().toLowerCase();
        const juce::String parent = f.getParentDirectory().getFileName().toLowerCase();

        if (fn.contains("pad") || parent.contains("pad")) {
            padSounds.push_back(f);
        }

        if (ext == ".npno" || fn.contains("grand") || fn.contains("upright") || fn.contains("piano") || fn.contains("clav")) {
            pianoSounds.push_back(f);
        } else if (parent.contains("orchestral") || fn.contains("orchestra") || fn.contains("string") ||
                   fn.contains("horn") || fn.contains("wind") || parent.contains("accordion")) {
            stringSounds.push_back(f);
        } else {
            synthSounds.push_back(f);
        }
    }

    auto sortByName = [](const juce::File& a, const juce::File& b) {
        return a.getFileNameWithoutExtension().compareNatural(b.getFileNameWithoutExtension()) < 0;
    };
    std::sort(pianoSounds.begin(), pianoSounds.end(), sortByName);
    std::sort(stringSounds.begin(), stringSounds.end(), sortByName);
    std::sort(synthSounds.begin(), synthSounds.end(), sortByName);
    std::sort(padSounds.begin(), padSounds.end(), sortByName);

    for (const auto& f : pianoSounds) discoveredSounds.push_back(f);
    for (const auto& f : stringSounds) discoveredSounds.push_back(f);
    for (const auto& f : synthSounds) discoveredSounds.push_back(f);
    for (const auto& f : padSounds) discoveredSounds.push_back(f);
}

void NordPianoPrototypeAudioProcessor::stepSoundLayerA(int direction) {
    if (pianoSounds.empty()) rescanSoundLibraries();
    if (pianoSounds.empty()) return;

    soundIndexA = (soundIndexA + direction + int(pianoSounds.size())) % int(pianoSounds.size());
    loadLayerAAsync(pianoSounds[size_t(soundIndexA)]);
}

void NordPianoPrototypeAudioProcessor::stepSoundLayerB(int direction) {
    if (padSounds.empty() && stringSounds.empty()) rescanSoundLibraries();
    auto& targetList = !padSounds.empty() ? padSounds : stringSounds;
    if (targetList.empty()) return;

    soundIndexB = (soundIndexB + direction + int(targetList.size())) % int(targetList.size());
    loadLayerBAsync(targetList[size_t(soundIndexB)]);
}

void NordPianoPrototypeAudioProcessor::stepSoundLayerC(int direction) {
    if (synthSounds.empty()) rescanSoundLibraries();
    if (synthSounds.empty()) return;

    soundIndexC = (soundIndexC + direction + int(synthSounds.size())) % int(synthSounds.size());
    loadLayerCAsync(synthSounds[size_t(soundIndexC)]);
}

void NordPianoPrototypeAudioProcessor::selectProgram(int index) {
    index = juce::jlimit(0, 7, index);
    currentProgramIndex.store(index, std::memory_order_relaxed);
    const auto& p = presets[size_t(index)];

    // 1. Update Piano Engine Directly
    engine.getLayerAConfig().volume.store(p.volA, std::memory_order_relaxed);
    engine.getLayerBConfig().volume.store(p.volB, std::memory_order_relaxed);
    engine.getLayerCConfig().volume.store(p.volC, std::memory_order_relaxed);
    engine.getLayerAConfig().enabled.store(p.enabledA, std::memory_order_relaxed);
    engine.getLayerBConfig().enabled.store(p.enabledB, std::memory_order_relaxed);
    engine.getLayerCConfig().enabled.store(p.enabledC, std::memory_order_relaxed);

    engine.setTone(p.tone);
    engine.setDynamics(p.dynamics);
    engine.setDynamicsHard(p.dynamicsHard);
    engine.setResonance(p.resonanceAmount);
    engine.setRelease(p.pianoRelease);
    engine.setStringRes(p.stringRes);
    engine.setPedalNoise(p.pedalNoise);
    engine.setSoftRelease(p.softRelease);
    engine.setTouchCurve(p.touchCurve);

    engine.setPadCutoff(p.padCutoff);
    engine.setPadResonance(p.padRes);
    engine.setPadAttack(p.padAttack);
    engine.setPadRelease(p.padRelease);

    engine.setCompressorAmount(p.compAmount);
    engine.setCompressorOn(p.compOn);
    engine.setDelayTime(p.delayTime);
    engine.setDelayFeedback(p.delayFeedback);
    engine.setDelayOn(p.delayOn);

    engine.setReverbAmount(p.reverbAmount);
    engine.setReverbOn(p.reverbOn);
    nordReverb.setType(p.reverbType);
    nordReverb.setVariation(p.reverbVariation);
    nordReverb.setTone(p.reverbTone);

    engine.setEqTreble(p.eqTreble);
    engine.setEqMid(p.eqMid);
    engine.setEqBass(p.eqBass);
    engine.setPadEqTreble(p.padEqTreble);
    engine.setPadEqMid(p.padEqMid);
    engine.setPadEqBass(p.padEqBass);
    engine.setChorusDepth(p.chorusDepth);
    engine.setChorusRate(p.chorusRate);
    engine.setTremoloDepth(p.tremoloDepth);
    engine.setTremoloRate(p.tremoloRate);
    engine.setTranspose(p.transpose);
    engine.setTune(p.tune);
    engine.setMainVolume(p.mainVolume);

    // 2. Load Piano & Pad files if specified and different from current
    if (p.layerAPath.isNotEmpty()) {
        juce::File f(p.layerAPath);
        if (f.existsAsFile() && f.getFullPathName().toStdString() != engine.getLayerAConfig().path) {
            loadLayerAAsync(f);
        }
    }
    if (p.layerBPath.isNotEmpty()) {
        juce::File f(p.layerBPath);
        if (f.existsAsFile() && f.getFullPathName().toStdString() != engine.getLayerBConfig().path) {
            loadLayerBAsync(f);
        }
    }

    // 3. Update APVTS Parameters for DAW Host Automation
    auto setParam = [this](const juce::String& id, float val) {
        if (auto* param = apvts.getParameter(id))
            param->setValueNotifyingHost(param->convertTo0to1(val));
    };
    auto setBool = [this](const juce::String& id, bool val) {
        if (auto* param = apvts.getParameter(id))
            param->setValueNotifyingHost(val ? 1.0f : 0.0f);
    };

    setParam("piano_vol", p.volA);
    setBool("piano_on", p.enabledA);
    setParam("tone", p.tone);
    setParam("dynamics", p.dynamics);
    setParam("piano_res_amount", p.resonanceAmount);
    setParam("piano_release", p.pianoRelease);
    setBool("piano_res", p.stringRes);
    setBool("piano_pedal", p.pedalNoise);
    setBool("piano_soft_rel", p.softRelease);
    setParam("piano_touch", float(p.touchCurve));

    setParam("pad_vol", p.volB);
    setBool("pad_on", p.enabledB);
    setParam("pad_cutoff", p.padCutoff);
    setParam("pad_resonance", p.padRes);
    setParam("pad_attack", p.padAttack);
    setParam("pad_release", p.padRelease);

    setParam("comp_amount", p.compAmount);
    setBool("comp_on", p.compOn);
    setParam("delay_time", p.delayTime);
    setParam("delay_feedback", p.delayFeedback);
    setBool("delay_on", p.delayOn);

    setParam("reverb_amount", p.reverbAmount);
    setBool("reverb_on", p.reverbOn);
    setParam("reverb_type", float(p.reverbType));
    setParam("reverb_variation", float(p.reverbVariation));
    setParam("reverb_tone", p.reverbTone);

    setParam("eq_treble", p.eqTreble);
    setParam("eq_mid", p.eqMid);
    setParam("eq_bass", p.eqBass);
    setParam("pad_eq_treble", p.padEqTreble);
    setParam("pad_eq_mid", p.padEqMid);
    setParam("pad_eq_bass", p.padEqBass);
    setParam("chorus_depth", p.chorusDepth);
    setParam("tremolo_depth", p.tremoloDepth);
    setParam("transpose", float(p.transpose));
    setParam("tune", p.tune);
    setParam("main_volume", p.mainVolume);

    // 4. Notify UI to animate and reposition all physical knobs
    programChangedFlag.store(true, std::memory_order_relaxed);
}

void NordPianoPrototypeAudioProcessor::saveCurrentToProgram(int index) {
    index = juce::jlimit(0, 7, index);
    auto& p = presets[size_t(index)];

    // Sound Library Files
    p.layerAPath = engine.getLayerAConfig().path;
    p.layerAName = getLayerAName();
    p.layerBPath = engine.getLayerBConfig().path;
    p.layerBName = getPadName();

    // Volumes & Layers
    p.volA = engine.getLayerAConfig().volume.load(std::memory_order_relaxed);
    p.volB = engine.getLayerBConfig().volume.load(std::memory_order_relaxed);
    p.volC = engine.getLayerCConfig().volume.load(std::memory_order_relaxed);
    p.enabledA = engine.getLayerAConfig().enabled.load(std::memory_order_relaxed);
    p.enabledB = engine.getLayerBConfig().enabled.load(std::memory_order_relaxed);
    p.enabledC = engine.getLayerCConfig().enabled.load(std::memory_order_relaxed);

    // Acoustic Piano Shaping
    p.tone = engine.getTone();
    p.dynamics = engine.getDynamics();
    p.dynamicsHard = engine.isDynamicsHard();
    p.resonanceAmount = engine.getResonance();
    p.pianoRelease = engine.getRelease();
    p.stringRes = engine.isStringResOn();
    p.pedalNoise = engine.isPedalNoiseOn();
    p.softRelease = engine.isSoftRelease();
    p.touchCurve = engine.getTouchCurve();

    // Worship Pad Shaping
    p.padCutoff = engine.getPadCutoff();
    p.padRes = engine.getPadResonance();
    p.padAttack = engine.getPadAttack();
    p.padRelease = engine.getPadRelease();

    // Effects
    p.compAmount = engine.getCompressorAmount();
    p.compOn = engine.isCompressorOn();
    p.delayTime = engine.getDelayTime();
    p.delayFeedback = engine.getDelayFeedback();
    p.delayOn = engine.isDelayOn();

    // Reverb Engine
    p.reverbAmount = engine.getReverbAmount();
    p.reverbOn = engine.isReverbOn();
    p.reverbType = nordReverb.getType();
    p.reverbVariation = nordReverb.getVariation();
    p.reverbTone = nordReverb.getTone();

    // Master & EQ
    p.eqTreble = engine.getEqTreble();
    p.eqMid = engine.getEqMid();
    p.eqBass = engine.getEqBass();
    p.padEqTreble = engine.getPadEqTreble();
    p.padEqMid = engine.getPadEqMid();
    p.padEqBass = engine.getPadEqBass();
    p.chorusDepth = engine.getChorusDepth();
    p.chorusRate = engine.getChorusRate();
    p.tremoloDepth = engine.getTremoloDepth();
    p.tremoloRate = engine.getTremoloRate();
    p.transpose = engine.getTranspose();
    p.tune = engine.getTune();
    p.mainVolume = engine.getMainVolume();

    // Persist immediately to disk
    savePresetsToDisk();
}

void NordPianoPrototypeAudioProcessor::savePresetsToDisk() {
    juce::File dir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("NordPianos");
    dir.createDirectory();
    juce::File file = dir.getChildFile("nord_presets.xml");

    juce::XmlElement xml("NordPresets");
    xml.setAttribute("version", 1);
    if (customLibraryFolder.isNotEmpty()) {
        xml.setAttribute("customLibraryFolder", customLibraryFolder);
    }
    for (size_t i = 0; i < 8; ++i) {
        const auto& p = presets[i];
        auto* el = xml.createNewChildElement("Preset");
        el->setAttribute("index", int(i));
        el->setAttribute("name", p.name);
        el->setAttribute("layerAPath", p.layerAPath);
        el->setAttribute("layerAName", p.layerAName);
        el->setAttribute("layerBPath", p.layerBPath);
        el->setAttribute("layerBName", p.layerBName);
        el->setAttribute("volA", double(p.volA));
        el->setAttribute("volB", double(p.volB));
        el->setAttribute("enabledA", p.enabledA);
        el->setAttribute("enabledB", p.enabledB);
        el->setAttribute("tone", double(p.tone));
        el->setAttribute("dynamics", double(p.dynamics));
        el->setAttribute("resonanceAmount", double(p.resonanceAmount));
        el->setAttribute("pianoRelease", double(p.pianoRelease));
        el->setAttribute("stringRes", p.stringRes);
        el->setAttribute("pedalNoise", p.pedalNoise);
        el->setAttribute("softRelease", p.softRelease);
        el->setAttribute("touchCurve", p.touchCurve);
        el->setAttribute("padCutoff", double(p.padCutoff));
        el->setAttribute("padRes", double(p.padRes));
        el->setAttribute("padAttack", double(p.padAttack));
        el->setAttribute("padRelease", double(p.padRelease));
        el->setAttribute("compAmount", double(p.compAmount));
        el->setAttribute("compOn", p.compOn);
        el->setAttribute("delayTime", double(p.delayTime));
        el->setAttribute("delayFeedback", double(p.delayFeedback));
        el->setAttribute("delayOn", p.delayOn);
        el->setAttribute("reverbAmount", double(p.reverbAmount));
        el->setAttribute("reverbOn", p.reverbOn);
        el->setAttribute("reverbType", p.reverbType);
        el->setAttribute("reverbVariation", p.reverbVariation);
        el->setAttribute("reverbTone", double(p.reverbTone));
        el->setAttribute("eqTreble", double(p.eqTreble));
        el->setAttribute("eqMid", double(p.eqMid));
        el->setAttribute("eqBass", double(p.eqBass));
        el->setAttribute("padEqTreble", double(p.padEqTreble));
        el->setAttribute("padEqMid", double(p.padEqMid));
        el->setAttribute("padEqBass", double(p.padEqBass));
        el->setAttribute("chorusDepth", double(p.chorusDepth));
        el->setAttribute("tremoloDepth", double(p.tremoloDepth));
        el->setAttribute("transpose", p.transpose);
        el->setAttribute("tune", double(p.tune));
        el->setAttribute("mainVolume", double(p.mainVolume));
    }

    // Save MIDI Mappings to XML
    auto* mapXml = xml.createNewChildElement("MidiCcMappings");
    {
        const juce::ScopedLock sl(midiMapLock);
        for (const auto& pair : ccToParam) {
            auto* item = mapXml->createNewChildElement("Mapping");
            item->setAttribute("cc", pair.first);
            item->setAttribute("param", pair.second);
        }
    }
    auto* noteXml = xml.createNewChildElement("MidiNoteMappings");
    {
        const juce::ScopedLock sl(midiMapLock);
        for (const auto& pair : noteToParam) {
            auto* item = noteXml->createNewChildElement("Mapping");
            item->setAttribute("note", pair.first);
            item->setAttribute("param", pair.second);
        }
    }

    xml.writeTo(file);
}

void NordPianoPrototypeAudioProcessor::loadPresetsFromDisk() {
    juce::File dir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("NordPianos");
    juce::File file = dir.getChildFile("nord_presets.xml");
    if (!file.existsAsFile()) return;

    std::unique_ptr<juce::XmlElement> xml = juce::parseXML(file);
    if (!xml || !xml->hasTagName("NordPresets")) return;

    if (xml->hasAttribute("customLibraryFolder")) {
        customLibraryFolder = xml->getStringAttribute("customLibraryFolder");
    }

    for (auto* el : xml->getChildIterator()) {
        if (!el->hasTagName("Preset")) continue;
        const int idx = el->getIntAttribute("index", -1);
        if (idx < 0 || idx >= 8) continue;
        auto& p = presets[size_t(idx)];
        p.name = el->getStringAttribute("name", p.name);
        p.layerAPath = el->getStringAttribute("layerAPath", p.layerAPath);
        p.layerAName = el->getStringAttribute("layerAName", p.layerAName);
        p.layerBPath = el->getStringAttribute("layerBPath", p.layerBPath);
        p.layerBName = el->getStringAttribute("layerBName", p.layerBName);
        p.volA = float(el->getDoubleAttribute("volA", double(p.volA)));
        p.volB = float(el->getDoubleAttribute("volB", double(p.volB)));
        p.enabledA = el->getBoolAttribute("enabledA", p.enabledA);
        p.enabledB = el->getBoolAttribute("enabledB", p.enabledB);
        p.tone = float(el->getDoubleAttribute("tone", double(p.tone)));
        p.dynamics = float(el->getDoubleAttribute("dynamics", double(p.dynamics)));
        p.resonanceAmount = float(el->getDoubleAttribute("resonanceAmount", double(p.resonanceAmount)));
        p.pianoRelease = float(el->getDoubleAttribute("pianoRelease", double(p.pianoRelease)));
        p.stringRes = el->getBoolAttribute("stringRes", p.stringRes);
        p.pedalNoise = el->getBoolAttribute("pedalNoise", p.pedalNoise);
        p.softRelease = el->getBoolAttribute("softRelease", p.softRelease);
        p.touchCurve = el->getIntAttribute("touchCurve", p.touchCurve);
        p.padCutoff = float(el->getDoubleAttribute("padCutoff", double(p.padCutoff)));
        p.padRes = float(el->getDoubleAttribute("padRes", double(p.padRes)));
        p.padAttack = float(el->getDoubleAttribute("padAttack", double(p.padAttack)));
        p.padRelease = float(el->getDoubleAttribute("padRelease", double(p.padRelease)));
        p.compAmount = float(el->getDoubleAttribute("compAmount", double(p.compAmount)));
        p.compOn = el->getBoolAttribute("compOn", p.compOn);
        p.delayTime = float(el->getDoubleAttribute("delayTime", double(p.delayTime)));
        p.delayFeedback = float(el->getDoubleAttribute("delayFeedback", double(p.delayFeedback)));
        p.delayOn = el->getBoolAttribute("delayOn", p.delayOn);
        p.reverbAmount = float(el->getDoubleAttribute("reverbAmount", double(p.reverbAmount)));
        p.reverbOn = el->getBoolAttribute("reverbOn", p.reverbOn);
        p.reverbType = el->getIntAttribute("reverbType", p.reverbType);
        p.reverbVariation = el->getIntAttribute("reverbVariation", p.reverbVariation);
        p.reverbTone = float(el->getDoubleAttribute("reverbTone", double(p.reverbTone)));
        p.eqTreble = float(el->getDoubleAttribute("eqTreble", double(p.eqTreble)));
        p.eqMid = float(el->getDoubleAttribute("eqMid", double(p.eqMid)));
        p.eqBass = float(el->getDoubleAttribute("eqBass", double(p.eqBass)));
        p.padEqTreble = float(el->getDoubleAttribute("padEqTreble", double(p.padEqTreble)));
        p.padEqMid = float(el->getDoubleAttribute("padEqMid", double(p.padEqMid)));
        p.padEqBass = float(el->getDoubleAttribute("padEqBass", double(p.padEqBass)));
        p.chorusDepth = float(el->getDoubleAttribute("chorusDepth", double(p.chorusDepth)));
        p.tremoloDepth = float(el->getDoubleAttribute("tremoloDepth", double(p.tremoloDepth)));
        p.transpose = el->getIntAttribute("transpose", p.transpose);
        p.tune = float(el->getDoubleAttribute("tune", double(p.tune)));
        p.mainVolume = float(el->getDoubleAttribute("mainVolume", double(p.mainVolume)));
    }

    // Restore MIDI CC mappings
    if (auto* mapXml = xml->getChildByName("MidiCcMappings")) {
        const juce::ScopedLock sl(midiMapLock);
        ccToParam.clear();
        paramToCc.clear();
        for (auto* item : mapXml->getChildIterator()) {
            if (item->hasTagName("Mapping")) {
                int cc = item->getIntAttribute("cc", -1);
                juce::String p = item->getStringAttribute("param");
                if (cc >= 0 && p.isNotEmpty()) {
                    ccToParam[cc] = p;
                    paramToCc[p] = cc;
                }
            }
        }
    }

    // Restore MIDI Note mappings
    if (auto* noteXml = xml->getChildByName("MidiNoteMappings")) {
        const juce::ScopedLock sl(midiMapLock);
        noteToParam.clear();
        paramToNote.clear();
        for (auto* item : noteXml->getChildIterator()) {
            if (item->hasTagName("Mapping")) {
                int note = item->getIntAttribute("note", -1);
                juce::String p = item->getStringAttribute("param");
                if (note >= 0 && p.isNotEmpty()) {
                    noteToParam[note] = p;
                    paramToNote[p] = note;
                }
            }
        }
    }
}

const juce::String NordPianoPrototypeAudioProcessor::getProgramName(int index) {
    if (index >= 0 && index < 8) return presets[size_t(index)].name;
    return {};
}

void NordPianoPrototypeAudioProcessor::changeProgramName(int index, const juce::String& newName) {
    if (index >= 0 && index < 8) presets[size_t(index)].name = newName;
}

void NordPianoPrototypeAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    juce::ignoreUnused(samplesPerBlock);
    engine.prepare(sampleRate);
    nordReverb.prepare(sampleRate);
}

bool NordPianoPrototypeAudioProcessor::isBusesLayoutSupported(const BusesLayout& layout) const {
    if (layout.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    return true;
}

void NordPianoPrototypeAudioProcessor::updateReverbParameters() {
}

void NordPianoPrototypeAudioProcessor::loadLayerAAsync(const juce::File& file) {
    if (!file.existsAsFile()) return;
    const std::string fullPath = file.getFullPathName().toStdString();
    const juce::String fn = file.getFileNameWithoutExtension();

    auto cached = getCachedBank(fullPath);
    if (cached) {
        engine.setLayerABank(cached, fn.toStdString(), fullPath, "Acoustic Piano");
        const juce::ScopedLock sl(infoLock);
        layerAName = fn;
        layerADetails = "Acoustic Piano | Nord Piano Library (RAM)";
        status = "Pronto: " + fn;
        loadingA.store(false, std::memory_order_relaxed);
        return;
    }

    loaderThreadA->triggerLoad(file);
}

void NordPianoPrototypeAudioProcessor::loadLayerBAsync(const juce::File& file) {
    if (!file.existsAsFile()) return;
    const std::string fullPath = file.getFullPathName().toStdString();
    const juce::String fn = file.getFileNameWithoutExtension();

    auto cached = getCachedBank(fullPath);
    if (cached) {
        engine.setLayerBBank(cached, fn.toStdString(), fullPath, "Orchestral / Strings");
        const juce::ScopedLock sl(infoLock);
        layerBName = fn;
        layerBDetails = "Orchestral / Strings | Nord Sound Library (RAM)";
        status = "Pronto: " + fn;
        loadingB.store(false, std::memory_order_relaxed);
        return;
    }

    loaderThreadB->triggerLoad(file);
}

void NordPianoPrototypeAudioProcessor::loadLayerCAsync(const juce::File& file) {
    if (!file.existsAsFile()) return;
    const std::string fullPath = file.getFullPathName().toStdString();
    const juce::String fn = file.getFileNameWithoutExtension();

    auto cached = getCachedBank(fullPath);
    if (cached) {
        engine.setLayerCBank(cached, fn.toStdString(), fullPath, "Sample Synth / Lead");
        const juce::ScopedLock sl(infoLock);
        layerCName = fn;
        layerCDetails = "Sample Synth / Lead | Nord Sample Library (RAM)";
        status = "Pronto: " + fn;
        loadingC.store(false, std::memory_order_relaxed);
        return;
    }

    loaderThreadC->triggerLoad(file);
}

juce::String NordPianoPrototypeAudioProcessor::getStatus() const {
    const juce::ScopedLock sl(infoLock);
    return status;
}

juce::String NordPianoPrototypeAudioProcessor::getLayerAName() const {
    const juce::ScopedLock sl(infoLock);
    return layerAName;
}

juce::String NordPianoPrototypeAudioProcessor::getLayerBName() const {
    const juce::ScopedLock sl(infoLock);
    return layerBName;
}

juce::String NordPianoPrototypeAudioProcessor::getLayerCName() const {
    const juce::ScopedLock sl(infoLock);
    return layerCName;
}

juce::String NordPianoPrototypeAudioProcessor::getLayerADetails() const {
    const juce::ScopedLock sl(infoLock);
    return layerADetails;
}

juce::String NordPianoPrototypeAudioProcessor::getLayerBDetails() const {
    const juce::ScopedLock sl(infoLock);
    return layerBDetails;
}

juce::String NordPianoPrototypeAudioProcessor::getLayerCDetails() const {
    const juce::ScopedLock sl(infoLock);
    return layerCDetails;
}

juce::String NordPianoPrototypeAudioProcessor::getCurrentFilePath() const {
    const juce::ScopedLock sl(infoLock);
    return currentPath;
}

double NordPianoPrototypeAudioProcessor::getTotalMemoryMB() const {
    double total = 0.0;
    const juce::ScopedLock sl(cacheLock);
    for (const auto& pair : bankCache) {
        if (pair.second) {
            for (const auto& s : pair.second->samples) {
                total += double(s.pcm.size() * sizeof(int16_t));
            }
        }
    }
    if (total < 1.0) {
        auto bA = engine.getLayerABank();
        if (bA) for (const auto& s : bA->samples) total += double(s.pcm.size() * sizeof(int16_t));
        auto bB = engine.getLayerBBank();
        if (bB) for (const auto& s : bB->samples) total += double(s.pcm.size() * sizeof(int16_t));
        auto bC = engine.getLayerCBank();
        if (bC) for (const auto& s : bC->samples) total += double(s.pcm.size() * sizeof(int16_t));
    }
    return total / (1024.0 * 1024.0);
}

void NordPianoPrototypeAudioProcessor::handleMidi(const juce::MidiMessage& msg) {
    if (msg.isNoteOn()) {
        const int note = msg.getNoteNumber();
        const int vel = msg.getVelocity();

        // 1. Ableton-style MIDI Learn for Notes / Pads
        if (midiMapModeActive.load(std::memory_order_relaxed)) {
            const juce::ScopedLock sl(midiMapLock);
            if (selectedMidiLearnParam.isNotEmpty()) {
                mapNoteToParameter(note, selectedMidiLearnParam);
                selectedMidiLearnParam = "";
                savePresetsToDisk();
                return;
            }
        }

        // 2. Check if this Note is mapped to an action
        juce::String mappedParam;
        {
            const juce::ScopedLock sl(midiMapLock);
            auto it = noteToParam.find(note);
            if (it != noteToParam.end()) mappedParam = it->second;
        }

        if (mappedParam.isNotEmpty() && vel > 0) {
            triggerVirtualAction(mappedParam, vel);
            return;
        }

        engine.noteOn(note, vel);
    } else if (msg.isNoteOff()) {
        engine.noteOff(msg.getNoteNumber());
    } else if (msg.isSustainPedalOn()) {
        const bool inv = engine.isPedalPolarityInverted();
        engine.sustain(!inv);
    } else if (msg.isSustainPedalOff()) {
        const bool inv = engine.isPedalPolarityInverted();
        engine.sustain(inv);
    } else if (msg.isAllNotesOff()) {
        engine.allNotesOff();
    } else if (msg.isProgramChange()) {
        const int pNum = msg.getProgramChangeNumber();
        selectProgram(pNum % 8);
    } else if (msg.isController()) {
        const int cc = msg.getControllerNumber();
        const int val = msg.getControllerValue();

        // 1. Ableton-style MIDI Learn Capture
        if (midiMapModeActive.load(std::memory_order_relaxed)) {
            const juce::ScopedLock sl(midiMapLock);
            if (selectedMidiLearnParam.isNotEmpty()) {
                mapCcToParameter(cc, selectedMidiLearnParam);
                selectedMidiLearnParam = "";
                savePresetsToDisk();
                return;
            }
        }

        // 2. Check if this CC is mapped to an APVTS parameter or Virtual Action
        juce::String mappedParam;
        {
            const juce::ScopedLock sl(midiMapLock);
            auto it = ccToParam.find(cc);
            if (it != ccToParam.end()) mappedParam = it->second;
        }

        if (mappedParam.isNotEmpty()) {
            triggerVirtualAction(mappedParam, val);
            return;
        }

        // Standard hardwired fallbacks if unmapped
        if (cc == 1) {
            engine.setModWheel(float(val) / 127.0f);
        }
    } else if (msg.isPitchWheel()) {
        engine.setPitchBend(float(msg.getPitchWheelValue() - 8192) / 8192.0f);
    }
}

void NordPianoPrototypeAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) {
    juce::ScopedNoDenormals noDenormals;

    // Process UI keyboard events
    keyboardState.processNextMidiBuffer(midiMessages, 0, buffer.getNumSamples(), true);

    for (const auto metadata : midiMessages) {
        handleMidi(metadata.getMessage());
    }

    buffer.clear();
    const int numSamples = buffer.getNumSamples();
    float* channelL = buffer.getWritePointer(0);
    float* channelR = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : channelL;

    engine.render(channelL, channelR, numSamples);

    const float revAmt = engine.getReverbAmount();
    const bool revOn = engine.isReverbOn();

    nordReverb.processStereo(channelL, channelR, numSamples, revAmt, revOn);
}

juce::AudioProcessorEditor* NordPianoPrototypeAudioProcessor::createEditor() {
    return new NordPianoPrototypeAudioProcessorEditor(*this);
}

void NordPianoPrototypeAudioProcessor::getStateInformation(juce::MemoryBlock& destData) {
    juce::XmlElement xml("NordPianoReaderBlackEdition");
    xml.setAttribute("currentProgram", currentProgramIndex.load());
    xml.setAttribute("layerAPath", engine.getLayerAConfig().path);
    xml.setAttribute("layerBPath", engine.getLayerBConfig().path);
    xml.setAttribute("layerCPath", engine.getLayerCConfig().path);

    // Save APVTS parameters
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> apvtsXml(state.createXml());
    if (apvtsXml) xml.addChildElement(apvtsXml.release());

    // Save MIDI Mappings
    auto* mapXml = xml.createNewChildElement("MidiMappings");
    {
        const juce::ScopedLock sl(midiMapLock);
        for (const auto& pair : ccToParam) {
            auto* item = mapXml->createNewChildElement("Mapping");
            item->setAttribute("cc", pair.first);
            item->setAttribute("param", pair.second);
        }
    }

    copyXmlToBinary(xml, destData);
}

void NordPianoPrototypeAudioProcessor::setStateInformation(const void* data, int sizeInBytes) {
    if (data == nullptr || sizeInBytes <= 0) return;

    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState && xmlState->hasTagName("NordPianoReaderBlackEdition")) {
        currentProgramIndex.store(xmlState->getIntAttribute("currentProgram", 0));

        // Restore APVTS parameters
        if (auto* apvtsXml = xmlState->getChildByName(apvts.state.getType())) {
            apvts.replaceState(juce::ValueTree::fromXml(*apvtsXml));
        }

        // Restore MIDI mappings
        if (auto* mapXml = xmlState->getChildByName("MidiMappings")) {
            const juce::ScopedLock sl(midiMapLock);
            ccToParam.clear();
            paramToCc.clear();
            for (auto* item : mapXml->getChildIterator()) {
                if (item->hasTagName("Mapping")) {
                    int cc = item->getIntAttribute("cc", -1);
                    juce::String p = item->getStringAttribute("param");
                    if (cc >= 0 && p.isNotEmpty()) {
                        ccToParam[cc] = p;
                        paramToCc[p] = cc;
                    }
                }
            }
        }

        const juce::String pathA = xmlState->getStringAttribute("layerAPath");
        if (pathA.isNotEmpty()) {
            juce::File fA(pathA);
            if (fA.existsAsFile()) loadLayerAAsync(fA);
        }
        const juce::String pathB = xmlState->getStringAttribute("layerBPath");
        if (pathB.isNotEmpty()) {
            juce::File fB(pathB);
            if (fB.existsAsFile()) loadLayerBAsync(fB);
        }
        const juce::String pathC = xmlState->getStringAttribute("layerCPath");
        if (pathC.isNotEmpty()) {
            juce::File fC(pathC);
            if (fC.existsAsFile()) loadLayerCAsync(fC);
        }
    }
}

//==============================================================================
// JUCE Plugin Filter Factory
//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new NordPianoPrototypeAudioProcessor();
}
