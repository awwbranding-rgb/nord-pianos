#pragma once
#include <JuceHeader.h>
#include "PianoEngine.h"
#include "NordReverbEngine.h"
#include <atomic>
#include <memory>
#include <vector>
#include <unordered_map>

//==============================================================================
// NPno Piano Reader Audio Processor (Black Edition)
// 3-Engine Architecture: Piano (A), Strings (B), Synth (C)
// Full APVTS Host Automation & Ableton-style In-Plugin MIDI Learn
//==============================================================================
class NordPianoPrototypeAudioProcessor final : public juce::AudioProcessor,
                                              public juce::AudioProcessorValueTreeState::Listener {
public:
    struct ProgramPreset {
        juce::String name = "Program 1";
        juce::String category = "Stage Trio";
        juce::String layerAPath;
        juce::String layerBPath;
        juce::String layerCPath;
        juce::String layerAName = "Astoria Grand XL 6.1";
        juce::String layerBName = "Evolving Pad 1";
        juce::String layerCName = "Smooth Lead 3";
        float mixBalance = 0.5f;
        float volA = 0.85f;
        float volB = 0.70f;
        float volC = 0.80f;
        float panA = 0.0f;
        float panB = 0.0f;
        float panC = 0.0f;
        bool enabledA = true;
        bool enabledB = false;
        bool enabledC = false;
        bool monoA = false;
        bool monoB = false;
        bool monoC = true;
        float tone = 0.75f;
        float dynamics = 1.0f;
        bool dynamicsHard = false;
        float resonanceAmount = 0.5f;
        float pianoRelease = 0.6f;
        bool stringRes = true;
        bool pedalNoise = true;
        bool softRelease = true;
        int touchCurve = 1;
        float padCutoff = 4500.0f;
        float padRes = 1.0f;
        float padAttack = 0.35f;
        float padRelease = 2.0f;
        float compAmount = 0.0f;
        bool compOn = false;
        float delayTime = 3.5f;
        float delayFeedback = 3.5f;
        bool delayOn = false;
        float reverbAmount = 3.0f;
        bool reverbOn = true;
        int reverbType = 1; // Hall
        int reverbVariation = 0; // Norm
        float reverbTone = 5.0f;
        float eqTreble = 0.0f;
        float eqMid = 0.0f;
        float eqBass = 0.0f;
        float padEqTreble = 0.0f;
        float padEqMid = 0.0f;
        float padEqBass = 0.0f;
        float chorusDepth = 0.0f;
        float chorusRate = 2.5f;
        float tremoloDepth = 0.0f;
        float tremoloRate = 4.0f;
        int transpose = 0;
        float tune = 0.0f;
        float mainVolume = 8.0f;
    };

    NordPianoPrototypeAudioProcessor();
    ~NordPianoPrototypeAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layout) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Nord Pianos"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }
    int getNumPrograms() override { return 8; }
    int getCurrentProgram() override { return currentProgramIndex.load(std::memory_order_relaxed); }
    void setCurrentProgram(int index) override { selectProgram(index); }
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    // APVTS (AudioProcessorValueTreeState)
    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts; }
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void parameterChanged(const juce::String& parameterID, float newValue) override;

    juce::MidiKeyboardState& getKeyboardState() { return keyboardState; }

    // Triple Sound Engine Loaders: Piano (A), Strings (B), Synth (C)
    void loadLayerAAsync(const juce::File& file);
    void loadLayerBAsync(const juce::File& file);
    void loadLayerCAsync(const juce::File& file);

    bool isLoadingLayerA() const { return loadingA.load(std::memory_order_relaxed); }
    bool isLoadingLayerB() const { return loadingB.load(std::memory_order_relaxed); }
    bool isLoadingLayerC() const { return loadingC.load(std::memory_order_relaxed); }

    bool hasLoadedPiano() const { return engine.hasLayerA() || engine.hasLayerB() || engine.hasLayerC(); }
    juce::String getStatus() const;

    juce::String getLayerAName() const;
    juce::String getLayerBName() const;
    juce::String getLayerCName() const;
    juce::String getLayerADetails() const;
    juce::String getLayerBDetails() const;
    juce::String getLayerCDetails() const;
    juce::String getCurrentFilePath() const;

    PianoEngine& getEngine() { return engine; }
    const PianoEngine& getEngine() const { return engine; }

    void handleMidi(const juce::MidiMessage& msg);
    bool consumeMidiActivity() { return engine.consumeMidiActivity(); }

    // Sound Library Scanner & Dedicated Collections
    const std::vector<juce::File>& getDiscoveredSounds() const { return discoveredSounds; }
    const std::vector<juce::File>& getPianoSounds() const { return pianoSounds; }
    const std::vector<juce::File>& getStringSounds() const { return stringSounds; }
    const std::vector<juce::File>& getSynthSounds() const { return synthSounds; }
    const std::vector<juce::File>& getPadSounds() const { return padSounds; }
    void rescanSoundLibraries();
    juce::String getCustomLibraryFolder() const { return customLibraryFolder; }
    void setCustomLibraryFolder(const juce::String& folder) {
        customLibraryFolder = folder;
        savePresetsToDisk();
    }
    void stepSoundLayerA(int direction);
    void stepSoundLayerB(int direction);
    void stepSoundLayerC(int direction);
    void stepSoundPad(int direction) { stepSoundLayerB(direction); }
    void loadPadAsync(const juce::File& file) { loadLayerBAsync(file); }
    juce::String getPadName() const { return getLayerBName(); }
    juce::String getPadDetails() const { return getLayerBDetails(); }
    bool isLoadingPad() const { return isLoadingLayerB(); }

    // In-Memory Bank Cache (Instantaneous 0ms switching!)
    std::shared_ptr<const npno::Bank> getCachedBank(const std::string& path) const;
    void cacheBank(const std::string& path, std::shared_ptr<const npno::Bank> bank);
    bool isBankCached(const std::string& path) const;

    // Programs (8 Memories)
    void selectProgram(int index);
    void saveCurrentToProgram(int index);
    int getCurrentProgramIndex() const { return currentProgramIndex.load(std::memory_order_relaxed); }
    const ProgramPreset& getProgramPreset(int index) const { return presets[juce::jlimit(0, 7, index)]; }
    void savePresetsToDisk();
    void loadPresetsFromDisk();
    bool consumeProgramChangedFlag() { return programChangedFlag.exchange(false, std::memory_order_relaxed); }
    int consumeEqTargetFlag() { return eqTargetFlag.exchange(-1, std::memory_order_relaxed); }
    void requestEqTarget(int target) { eqTargetFlag.store(target, std::memory_order_relaxed); }

    // Memory stats
    double getTotalMemoryMB() const;
    int getVoiceCount() const { return engine.getActiveVoiceCount(); }

    // Ableton-style Internal MIDI Learn & CC Mappings
    bool isMidiMapModeActive() const { return midiMapModeActive.load(std::memory_order_relaxed); }
    void setMidiMapModeActive(bool active) {
        midiMapModeActive.store(active, std::memory_order_relaxed);
        if (!active) {
            const juce::ScopedLock sl(midiMapLock);
            selectedMidiLearnParam = "";
        }
    }
    void setSelectedMidiLearnParam(const juce::String& paramId) {
        const juce::ScopedLock sl(midiMapLock);
        selectedMidiLearnParam = paramId;
    }
    juce::String getSelectedMidiLearnParam() const {
        const juce::ScopedLock sl(midiMapLock);
        return selectedMidiLearnParam;
    }
    void mapCcToParameter(int cc, const juce::String& paramId);
    void mapNoteToParameter(int note, const juce::String& paramId);
    void unmapParameter(const juce::String& paramId);
    void clearAllMidiMappings();
    int getMappedCcForParam(const juce::String& paramId) const;
    int getMappedNoteForParam(const juce::String& paramId) const;
    juce::String getMappingBadgeForParam(const juce::String& paramId) const;
    void triggerVirtualAction(const juce::String& paramId, int value);

    NordReverbEngine& getReverbEngine() { return nordReverb; }
    const NordReverbEngine& getReverbEngine() const { return nordReverb; }

private:
    class LayerLoaderThread final : public juce::Thread {
    public:
        explicit LayerLoaderThread(NordPianoPrototypeAudioProcessor& proc, int layerIdx);
        ~LayerLoaderThread() override;
        void triggerLoad(const juce::File& file);
        void run() override;
    private:
        NordPianoPrototypeAudioProcessor& processor;
        int layerIndex = 0; // 0 = Piano (A), 1 = Strings (B), 2 = Synth (C)
        juce::CriticalSection queueLock;
        juce::File targetFile;
        juce::WaitableEvent wakeEvent;
    };

    class PreloaderThread final : public juce::Thread {
    public:
        explicit PreloaderThread(NordPianoPrototypeAudioProcessor& proc);
        ~PreloaderThread() override;
        void run() override;
    private:
        NordPianoPrototypeAudioProcessor& processor;
    };

    PianoEngine engine;
    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;
    NordReverbEngine nordReverb;

    std::atomic<bool> loadingA{false};
    std::atomic<bool> loadingB{false};
    std::atomic<bool> loadingC{false};

    mutable juce::CriticalSection infoLock;
    juce::String status = "Carregue timbres do Nord para Piano, Strings e Synth";
    juce::String layerAName = "Black Upright Med 6.1";
    juce::String layerBName = "Full Orchestra 1";
    juce::String layerCName = "Smooth Lead 3";
    juce::String layerADetails = "Acoustic Piano | Nord Piano Library";
    juce::String layerBDetails = "Orchestral / Strings | Nord Sound Library";
    juce::String layerCDetails = "Sample Synth / Lead | Nord Sample Library";
    juce::String currentPath;
    juce::String customLibraryFolder;

    std::unique_ptr<LayerLoaderThread> loaderThreadA;
    std::unique_ptr<LayerLoaderThread> loaderThreadB;
    std::unique_ptr<LayerLoaderThread> loaderThreadC;
    std::unique_ptr<PreloaderThread> preloaderThread;

    mutable juce::CriticalSection cacheLock;
    std::unordered_map<std::string, std::shared_ptr<const npno::Bank>> bankCache;

    std::vector<juce::File> discoveredSounds;
    std::vector<juce::File> pianoSounds;
    std::vector<juce::File> stringSounds;
    std::vector<juce::File> synthSounds;
    std::vector<juce::File> padSounds;

    int soundIndexA = -1;
    int soundIndexB = -1;
    int soundIndexC = -1;
    int soundIndexPad = -1;

    std::array<ProgramPreset, 8> presets;
    std::atomic<int> currentProgramIndex{0};
    std::atomic<bool> programChangedFlag{false};
    std::atomic<int> eqTargetFlag{-1};

    // MIDI Map State
    mutable juce::CriticalSection midiMapLock;
    std::unordered_map<int, juce::String> ccToParam;
    std::unordered_map<juce::String, int> paramToCc;
    std::unordered_map<int, juce::String> noteToParam;
    std::unordered_map<juce::String, int> paramToNote;
    std::atomic<bool> midiMapModeActive{false};
    juce::String selectedMidiLearnParam;

    void initDefaultPresets();
    void initDefaultMidiMappings();
    void updateReverbParameters();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NordPianoPrototypeAudioProcessor)
};
