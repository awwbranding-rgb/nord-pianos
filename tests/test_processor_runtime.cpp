#include "PluginProcessor.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <cassert>

int main() {
    std::cout << "=================================================" << std::endl;
    std::cout << "TEST: Nord Piano Reader (Mono / Pre-Cache / Swap)" << std::endl;
    std::cout << "=================================================" << std::endl;

    NordPianoPrototypeAudioProcessor proc;

    // 1. Wait for initial boot loading
    for (int i = 0; i < 60; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if (proc.getEngine().hasLayerA() && proc.getEngine().hasLayerB()) {
            std::cout << "[OK] Both Layer A and Layer B loaded at " << (i * 100) << " ms!" << std::endl;
            break;
        }
    }

    std::cout << "Discovered Sounds: " << proc.getDiscoveredSounds().size() << std::endl;
    std::cout << "Pianos: " << proc.getPianoSounds().size() << std::endl;
    std::cout << "Strings: " << proc.getStringSounds().size() << std::endl;
    std::cout << "Synths: " << proc.getSynthSounds().size() << std::endl;
    std::cout << "Layer A: " << proc.getLayerAName().toStdString() << std::endl;
    std::cout << "Layer B: " << proc.getLayerBName().toStdString() << " (Mono=" 
              << proc.getEngine().getLayerBConfig().mono.load() << ")" << std::endl;

    // 2. Test Audio Generation
    proc.prepareToPlay(44100.0, 512);
    juce::AudioBuffer<float> buffer(2, 512);
    juce::MidiBuffer midi;

    // Test Layer B Monophonic mode
    std::cout << "\n--- Testing Layer B Monophonic Mode ---" << std::endl;
    proc.getEngine().getLayerAConfig().enabled.store(false);
    proc.getEngine().getLayerBConfig().enabled.store(true);
    proc.getEngine().getLayerBConfig().mono.store(true);

    // Play Note 60
    proc.handleMidi(juce::MidiMessage::noteOn(1, 60, (juce::uint8)100));
    std::cout << "Voices playing after Note 60: " << proc.getVoiceCount() << std::endl;
    assert(proc.getVoiceCount() == 1);

    // Play Note 64 while 60 is held (Legato Lead)
    proc.handleMidi(juce::MidiMessage::noteOn(1, 64, (juce::uint8)100));
    std::cout << "Voices playing after Note 64 (should be 1 voice for mono): " << proc.getVoiceCount() << std::endl;
    assert(proc.getVoiceCount() == 1);

    // Release Note 64 -> Should retrigger Note 60
    proc.handleMidi(juce::MidiMessage::noteOff(1, 64));
    std::cout << "Voices playing after releasing Note 64: " << proc.getVoiceCount() << std::endl;
    assert(proc.getVoiceCount() == 1);

    // Release Note 60 -> All off
    proc.handleMidi(juce::MidiMessage::noteOff(1, 60));
    buffer.clear();
    proc.processBlock(buffer, midi);
    std::cout << "Voices playing after releasing Note 60: " << proc.getVoiceCount() << std::endl;

    // 3. Test Layer A Polyphonic Mode
    std::cout << "\n--- Testing Layer A Polyphonic Mode (Piano) ---" << std::endl;
    proc.getEngine().getLayerAConfig().enabled.store(true);
    proc.getEngine().getLayerBConfig().enabled.store(false);
    proc.getEngine().getLayerAConfig().mono.store(false);

    // Play 4-note chord: C E G B (60, 64, 67, 71)
    proc.handleMidi(juce::MidiMessage::noteOn(1, 60, (juce::uint8)90));
    proc.handleMidi(juce::MidiMessage::noteOn(1, 64, (juce::uint8)90));
    proc.handleMidi(juce::MidiMessage::noteOn(1, 67, (juce::uint8)90));
    proc.handleMidi(juce::MidiMessage::noteOn(1, 71, (juce::uint8)90));
    std::cout << "Voices playing chord (should be 4): " << proc.getVoiceCount() << std::endl;
    assert(proc.getVoiceCount() >= 4);

    buffer.clear();
    proc.processBlock(buffer, midi);
    float peak = buffer.getMagnitude(0, 0, 512);
    std::cout << "Audio Output Peak: " << peak << std::endl;
    assert(peak > 0.01f);

    proc.handleMidi(juce::MidiMessage::allNotesOff(1));

    // 4. Test Instant Switch between Presets
    std::cout << "\n--- Testing Instant Preset Switch ---" << std::endl;
    auto t0 = std::chrono::high_resolution_clock::now();
    proc.selectProgram(0); // Program 1
    auto t1 = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    std::cout << "Switched to Program 1 (cached) in: " << ms << " ms!" << std::endl;
    std::cout << "Active Layer A: " << proc.getLayerAName().toStdString() << std::endl;
    std::cout << "Active Layer B: " << proc.getLayerBName().toStdString() << std::endl;

    std::cout << "\n[ALL TESTS PASSED SUCCESSFULLY!]" << std::endl;
    return 0;
}
