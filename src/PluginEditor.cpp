#include "GrandPianoImage.h"
#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
// Constructor & Setup: Nord Pianos (Stage 4 Architecture)
//==============================================================================
NordPianoPrototypeAudioProcessorEditor::NordPianoPrototypeAudioProcessorEditor(NordPianoPrototypeAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p) {
    setLookAndFeel(&blackKnobLAF);

    // Load OLED Grand Piano image directly from embedded JPEG memory data
    {
        juce::MemoryInputStream mis(grandPianoJpgData, grandPianoJpgSize, false);
        juce::JPEGImageFormat jpgFormat;
        imgOledGrandPiano = jpgFormat.decodeImage(mis);
    }


    // Try loading piano image from project assets or bundle/app resources
    juce::File artFile("C:\\Projetos\\Nord_Piano_Kit\\Nord_Piano_Reader\\src\\piano_art.png");
    if (!artFile.existsAsFile()) {
        artFile = juce::File::getSpecialLocation(juce::File::currentExecutableFile)
                    .getParentDirectory().getChildFile("piano_art.png");
    }
#if JUCE_MAC
    if (!artFile.existsAsFile()) {
        artFile = juce::File::getSpecialLocation(juce::File::currentExecutableFile)
                    .getParentDirectory().getParentDirectory().getChildFile("Resources").getChildFile("piano_art.png");
    }
    if (!artFile.existsAsFile()) {
        artFile = juce::File("/Library/Application Support/Nord Pianos/piano_art.png");
    }
#endif
    if (artFile.existsAsFile()) {
        pianoImage = juce::ImageFileFormat::loadFrom(artFile);
    }

    // Top Bar Controls
    //==========================================================================
    // OmniStage Master Console Setup
    //==========================================================================
    addAndMakeVisible(tabPiano);
    addAndMakeVisible(tabPad);
    addAndMakeVisible(tabSynth);
    addAndMakeVisible(tabOrgan);
    addAndMakeVisible(btnSplitLayer);

    setupPillButton(tabPiano, "A  Piano", true);
    setupPillButton(tabPad, "B  Worship Pad", true);
    setupPillButton(tabSynth, "C  Synth Lead", true);
    setupPillButton(tabOrgan, "D  Organ B3", true);
    setupPillButton(btnSplitLayer, "Split / Layer v", false);

    tabPiano.setToggleState(true, juce::dontSendNotification);

    auto updateTabs = [this](int tabIdx) {
        activeTab = tabIdx;
        tabPiano.setToggleState(activeTab == 0, juce::dontSendNotification);
        tabPad.setToggleState(activeTab == 1, juce::dontSendNotification);
        tabSynth.setToggleState(activeTab == 2, juce::dontSendNotification);
        tabOrgan.setToggleState(activeTab == 3, juce::dontSendNotification);
        repaint();
    };

    tabPiano.onClick = [updateTabs]() { updateTabs(0); };
    tabPad.onClick = [updateTabs]() { updateTabs(1); };
    tabSynth.onClick = [updateTabs]() { updateTabs(2); };
    tabOrgan.onClick = [updateTabs]() { updateTabs(3); };

    // 4-Channel Mixer Faders
    auto setupMixerFader = [this](juce::Slider& s, juce::Colour col, double defVal) {
        addAndMakeVisible(s);
        s.setSliderStyle(juce::Slider::LinearVertical);
        s.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        s.setRange(0.0, 1.5, 0.01);
        s.setValue(defVal, juce::dontSendNotification);
        s.setLookAndFeel(&blackKnobLAF);
        s.setColour(juce::Slider::thumbColourId, col);
    };

    setupMixerFader(sliderVolA, juce::Colour::fromRGB(229, 57, 53), 1.0);  // Red
    setupMixerFader(sliderVolB, juce::Colour::fromRGB(0, 220, 240), 0.85); // Cyan
    setupMixerFader(sliderVolC, juce::Colour::fromRGB(50, 130, 255), 0.75);// Blue
    setupMixerFader(sliderVolD, juce::Colour::fromRGB(255, 160, 30), 0.70);// Amber

    sliderVolA.onValueChange = [this]() {
        processor.getEngine().getLayerAConfig().volume.store(float(sliderVolA.getValue()), std::memory_order_relaxed);
    };
    sliderVolB.onValueChange = [this]() {
        processor.getEngine().getLayerBConfig().volume.store(float(sliderVolB.getValue()), std::memory_order_relaxed);
    };
    sliderVolC.onValueChange = [this]() {
        processor.getEngine().getLayerCConfig().volume.store(float(sliderVolC.getValue()), std::memory_order_relaxed);
    };

    // Mixer Channel ON/Mute/Solo Buttons
    addAndMakeVisible(btnOnA);
    addAndMakeVisible(btnOnB);
    addAndMakeVisible(btnOnC);
    addAndMakeVisible(btnOnD);
    setupPillButton(btnOnA, "ON", true);
    setupPillButton(btnOnB, "ON", true);
    setupPillButton(btnOnC, "ON", true);
    setupPillButton(btnOnD, "ON", true);
    btnOnA.setToggleState(true, juce::dontSendNotification);
    btnOnB.setToggleState(true, juce::dontSendNotification);
    btnOnC.setToggleState(false, juce::dontSendNotification);
    btnOnD.setToggleState(false, juce::dontSendNotification);

    // Mute / Solo buttons hidden for clean stage strip
    addChildComponent(btnMuteA); addChildComponent(btnSoloA);
    addChildComponent(btnMuteB); addChildComponent(btnSoloB);
    addChildComponent(btnMuteC); addChildComponent(btnSoloC);
    addChildComponent(btnMuteD); addChildComponent(btnSoloD);
    setupPillButton(btnMuteA, "MUTE", true); setupPillButton(btnSoloA, "S", true);
    setupPillButton(btnMuteB, "MUTE", true); setupPillButton(btnSoloB, "S", true);
    setupPillButton(btnMuteC, "MUTE", true); setupPillButton(btnSoloC, "S", true);
    setupPillButton(btnMuteD, "MUTE", true); setupPillButton(btnSoloD, "S", true);

    // Bank Selector Buttons
    addAndMakeVisible(btnBankA);
    addAndMakeVisible(btnBankB);
    addAndMakeVisible(btnBankC);
    addAndMakeVisible(btnBankD);
    setupPillButton(btnBankA, "A", true);
    setupPillButton(btnBankB, "B", true);
    setupPillButton(btnBankC, "C", true);
    setupPillButton(btnBankD, "D", true);
    btnBankA.setToggleState(true, juce::dontSendNotification);

    auto updateBank = [this](int bIdx) {
        activeBank = bIdx;
        btnBankA.setToggleState(activeBank == 0, juce::dontSendNotification);
        btnBankB.setToggleState(activeBank == 1, juce::dontSendNotification);
        btnBankC.setToggleState(activeBank == 2, juce::dontSendNotification);
        btnBankD.setToggleState(activeBank == 3, juce::dontSendNotification);
        repaint();
    };
    btnBankA.onClick = [updateBank]() { updateBank(0); };
    btnBankB.onClick = [updateBank]() { updateBank(1); };
    btnBankC.onClick = [updateBank]() { updateBank(2); };
    btnBankD.onClick = [updateBank]() { updateBank(3); };

    // Sound / Timbre Category Buttons
    addAndMakeVisible(btnCatEPiano);
    addAndMakeVisible(btnCatClav);
    addAndMakeVisible(btnCatOrgan);
    addAndMakeVisible(btnCatPad);
    addAndMakeVisible(btnCatSynth);
    addAndMakeVisible(btnCatStrings);
    addAndMakeVisible(btnCatMisc);
    setupPillButton(btnCatEPiano, "E.Piano", true);
    setupPillButton(btnCatClav, "Clav", true);
    setupPillButton(btnCatOrgan, "Organ", true);
    setupPillButton(btnCatPad, "Pad", true);
    setupPillButton(btnCatSynth, "Synth", true);
    setupPillButton(btnCatStrings, "Strings", true);
    setupPillButton(btnCatMisc, "Misc", true);

    // Character Switches
    addAndMakeVisible(swDynamics);
    addAndMakeVisible(swPedalNoise);
    addAndMakeVisible(swDamperRes);
    setupPillButton(swDynamics, "", true);
    setupPillButton(swPedalNoise, "", true);
    setupPillButton(swDamperRes, "", true);
    swDynamics.setToggleState(true, juce::dontSendNotification);
    swPedalNoise.setToggleState(true, juce::dontSendNotification);
    swDamperRes.setToggleState(false, juce::dontSendNotification);

    // Tone & Amp Knobs
    setupRotary(sliderCutoff, "Cutoff", 100.0, 15000.0, 8000.0);
    setupRotary(sliderReso, "Reso", 0.1, 4.0, 1.0);
    setupRotary(sliderEnv, "Env", 0.0, 1.0, 0.5);
    setupRotary(sliderAmpAttack, "A", 0.001, 2.0, 0.01);
    setupRotary(sliderAmpDecay, "D", 0.05, 5.0, 1.5);
    setupRotary(sliderAmpRelease, "R", 0.05, 5.0, 0.6);

    // Comp & Dynamics Knobs
    setupRotary(sliderCompThresh, "Thresh", -40.0, 0.0, -18.0);
    setupRotary(sliderCompRatio, "Ratio", 1.0, 12.0, 4.0);
    setupRotary(sliderDynAttack, "Attack", 0.5, 50.0, 10.0);
    setupRotary(sliderDynRelease, "Release", 10.0, 500.0, 100.0);

    // Effects
    addAndMakeVisible(btnFxChorus);
    addAndMakeVisible(btnFxDelay);
    addAndMakeVisible(btnFxReverb);
    setupPillButton(btnFxChorus, "Chorus", true);
    setupPillButton(btnFxDelay, "Delay", true);
    setupPillButton(btnFxReverb, "Reverb", true);
    btnFxChorus.setToggleState(true, juce::dontSendNotification);
    setupRotary(sliderFxMix, "Mix", 0.0, 1.0, 0.35);
    addAndMakeVisible(btnQuickPrev);
    addAndMakeVisible(btnQuickNext);
    addAndMakeVisible(btnQuickSelect);
    btnQuickSelect.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    btnQuickSelect.setColour(juce::TextButton::textColourOffId, juce::Colour::fromRGB(255, 255, 255));
    addAndMakeVisible(btnMidiMap);
    addAndMakeVisible(btnMidiCh);
    addAndMakeVisible(btnOutput);
    addAndMakeVisible(btnSettings);
    btnSettings.setButtonText("");
    btnSettings.setAlpha(0.01f);

    btnQuickPrev.onClick = [this]() { processor.stepSoundLayerA(-1); };
    btnQuickNext.onClick = [this]() { processor.stepSoundLayerA(1); };

    setupPillButton(btnMidiMap, "MIDI MAP", true);
    btnMidiMap.onClick = [this]() {
        const bool active = btnMidiMap.getToggleState();
        processor.setMidiMapModeActive(active);
        if (midiMapOverlay != nullptr) {
            if (active) {
                midiMapOverlay->buildTargets();
                midiMapOverlay->setBounds(getLocalBounds());
                midiMapOverlay->setVisible(true);
                midiMapOverlay->toFront(true);
                btnMidiMap.toFront(true);
            } else {
                midiMapOverlay->setVisible(false);
            }
        }
        repaint();
    };

    btnSettings.onClick = [this]() {
        juce::PopupMenu m;
        m.addItem(1, "Selecionar Pasta de Sons/Timbres (Library Folder)...");
        m.addItem(2, "Escanear Timbres Novamente (Rescan Sounds)");
        m.addSeparator();
        m.addItem(3, "Nord Pianos v1.0 - Stage 4 Library Engine", false);

        auto safe = juce::Component::SafePointer<NordPianoPrototypeAudioProcessorEditor>(this);
        m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&btnSettings), [safe](int res) {
            if (!safe) return;
            if (res == 1) {
                juce::File initialDir(safe->processor.getCustomLibraryFolder());
                if (!initialDir.isDirectory()) {
                    initialDir = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory().getChildFile("Sounds");
                }
                if (!initialDir.isDirectory()) {
                    initialDir = juce::File("E:\\Disco Elisson\\NORD");
                }
                if (!initialDir.isDirectory()) {
                    initialDir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
                }
                safe->chooser = std::make_unique<juce::FileChooser>(
                    "Selecione a Pasta contendo os Timbres (.npno / .nsmp)",
                    initialDir,
                    "*"
                );
                safe->chooser->launchAsync(
                    juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                    [safe](const juce::FileChooser& fc) {
                        if (!safe) return;
                        auto result = fc.getResult();
                        if (result.isDirectory()) {
                            safe->processor.setCustomLibraryFolder(result.getFullPathName());
                            safe->processor.rescanSoundLibraries();
                            if (!safe->processor.getPianoSounds().empty()) {
                                safe->processor.loadLayerAAsync(safe->processor.getPianoSounds()[0]);
                            }
                            safe->repaint();
                        }
                    }
                );
            } else if (res == 2) {
                safe->processor.rescanSoundLibraries();
                safe->repaint();
            }
        });
    };

    btnQuickSelect.onClick = [this]() {
        juce::PopupMenu menu;
        const auto& pianos = processor.getPianoSounds();
        for (size_t i = 0; i < pianos.size(); ++i) {
            menu.addItem(int(i + 1), pianos[i].getFileNameWithoutExtension());
        }
        menu.addSeparator();
        menu.addItem(1000, "Carregar outro arquivo (.npno)...");

        auto safe = juce::Component::SafePointer<NordPianoPrototypeAudioProcessorEditor>(this);
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&btnQuickSelect), [safe, pianos](int result) {
            if (!safe) return;
            if (result >= 1 && result <= int(pianos.size())) {
                safe->processor.loadLayerAAsync(pianos[size_t(result - 1)]);
            } else if (result == 1000) {
                juce::File initialDir(safe->processor.getCustomLibraryFolder());
                if (!initialDir.isDirectory()) {
                    initialDir = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory().getChildFile("Sounds");
                }
                if (!initialDir.isDirectory()) {
                    initialDir = juce::File("E:\\Disco Elisson\\NORD");
                }
                if (!initialDir.isDirectory()) {
                    initialDir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
                }
                safe->chooser = std::make_unique<juce::FileChooser>(
                    "Selecione um Timbre Nord Piano (.npno)",
                    initialDir,
                    "*.npno;*.nsmp;*.nsmp4"
                );
                safe->chooser->launchAsync(
                    juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                    [safe](const juce::FileChooser& fc) {
                        if (!safe) return;
                        const auto selected = fc.getResult();
                        if (selected.existsAsFile()) safe->processor.loadLayerAAsync(selected);
                    }
                );
            }
        });
    };

    //==========================================================================
    // Column 1: EQ / Tone & Dynamics (Independent Piano / Pad EQ)
    //==========================================================================
    setupPillButton(btnEqPiano, "PIANO", false);
    setupPillButton(btnEqPad, "PAD", false);
    btnEqPiano.setToggleState(true, juce::dontSendNotification);
    btnEqPad.setToggleState(false, juce::dontSendNotification);

    auto updateEqControls = [this]() {
        btnEqPiano.setToggleState(eqTargetIsPiano, juce::dontSendNotification);
        btnEqPad.setToggleState(!eqTargetIsPiano, juce::dontSendNotification);
        if (eqTargetIsPiano) {
            sliderTreble.setValue(processor.getEngine().getEqTreble(), juce::dontSendNotification);
            sliderMid.setValue(processor.getEngine().getEqMid(), juce::dontSendNotification);
            sliderBass.setValue(processor.getEngine().getEqBass(), juce::dontSendNotification);
        } else {
            sliderTreble.setValue(processor.getEngine().getPadEqTreble(), juce::dontSendNotification);
            sliderMid.setValue(processor.getEngine().getPadEqMid(), juce::dontSendNotification);
            sliderBass.setValue(processor.getEngine().getPadEqBass(), juce::dontSendNotification);
        }
        repaint();
    };

    btnEqPiano.onClick = [this, updateEqControls]() {
        eqTargetIsPiano = true;
        updateEqControls();
    };
    btnEqPad.onClick = [this, updateEqControls]() {
        eqTargetIsPiano = false;
        updateEqControls();
    };

    setupRotary(sliderTreble, "TREBLE", -15.0, 15.0, processor.getEngine().getEqTreble());
    setupRotary(sliderMid, "MID", -15.0, 15.0, processor.getEngine().getEqMid());
    setupRotary(sliderBass, "BASS", -15.0, 15.0, processor.getEngine().getEqBass());
    setupRotary(sliderDynamics, "DYNAMICS", 0.2, 2.2, processor.getEngine().getDynamics());

    sliderTreble.onValueChange = [this]() {
        const float val = float(sliderTreble.getValue());
        if (eqTargetIsPiano) {
            processor.getEngine().setEqTreble(val);
            if (auto* p = processor.getAPVTS().getParameter("eq_treble"))
                p->setValueNotifyingHost(p->convertTo0to1(val));
        } else {
            processor.getEngine().setPadEqTreble(val);
            if (auto* p = processor.getAPVTS().getParameter("pad_eq_treble"))
                p->setValueNotifyingHost(p->convertTo0to1(val));
        }
    };
    sliderMid.onValueChange = [this]() {
        const float val = float(sliderMid.getValue());
        if (eqTargetIsPiano) {
            processor.getEngine().setEqMid(val);
            if (auto* p = processor.getAPVTS().getParameter("eq_mid"))
                p->setValueNotifyingHost(p->convertTo0to1(val));
        } else {
            processor.getEngine().setPadEqMid(val);
            if (auto* p = processor.getAPVTS().getParameter("pad_eq_mid"))
                p->setValueNotifyingHost(p->convertTo0to1(val));
        }
    };
    sliderBass.onValueChange = [this]() {
        const float val = float(sliderBass.getValue());
        if (eqTargetIsPiano) {
            processor.getEngine().setEqBass(val);
            if (auto* p = processor.getAPVTS().getParameter("eq_bass"))
                p->setValueNotifyingHost(p->convertTo0to1(val));
        } else {
            processor.getEngine().setPadEqBass(val);
            if (auto* p = processor.getAPVTS().getParameter("pad_eq_bass"))
                p->setValueNotifyingHost(p->convertTo0to1(val));
        }
    };
    sliderDynamics.onValueChange = [this]() {
        if (auto* p = processor.getAPVTS().getParameter("dynamics"))
            p->setValueNotifyingHost(p->convertTo0to1(float(sliderDynamics.getValue())));
    };

    // Nord Stage 4 Touch Curves (Touch 1 = Soft, Touch 2 = Normal, Touch 3 = Hard)
    setupPillButton(btnTouch1, "TOUCH 1", true);
    setupPillButton(btnTouch2, "TOUCH 2", true);
    setupPillButton(btnTouch3, "TOUCH 3", true);

    auto updateTouchButtons = [this](int curve) {
        processor.getEngine().setTouchCurve(curve);
        if (auto* p = processor.getAPVTS().getParameter("piano_touch"))
            p->setValueNotifyingHost(p->convertTo0to1(float(curve)));
        btnTouch1.setToggleState(curve == 0, juce::dontSendNotification);
        btnTouch2.setToggleState(curve == 1, juce::dontSendNotification);
        btnTouch3.setToggleState(curve == 2, juce::dontSendNotification);
        repaint();
    };

    btnTouch1.onClick = [updateTouchButtons]() { updateTouchButtons(0); };
    btnTouch2.onClick = [updateTouchButtons]() { updateTouchButtons(1); };
    btnTouch3.onClick = [updateTouchButtons]() { updateTouchButtons(2); };

    const int initialTouch = processor.getEngine().getTouchCurve();
    btnTouch1.setToggleState(initialTouch == 0, juce::dontSendNotification);
    btnTouch2.setToggleState(initialTouch == 1, juce::dontSendNotification);
    btnTouch3.setToggleState(initialTouch == 2, juce::dontSendNotification);

    //==========================================================================
    // Column 2: Dedicated Nord Stage 4 Piano Module
    //==========================================================================
    setupPillButton(btnPianoOn, "ON", true);
    btnPianoOn.setToggleState(processor.getEngine().getLayerAConfig().enabled.load(), juce::dontSendNotification);
    btnPianoOn.onClick = [this]() {
        const bool en = btnPianoOn.getToggleState();
        processor.getEngine().getLayerAConfig().enabled.store(en);
        if (auto* p = processor.getAPVTS().getParameter("piano_on"))
            p->setValueNotifyingHost(en ? 1.0f : 0.0f);
        if (!en) {
            processor.getEngine().stopVoicesForLayer(0);
        }
        repaint();
    };

    addAndMakeVisible(btnPianoPrev);
    addAndMakeVisible(btnPianoNext);
    btnPianoPrev.onClick = [this]() { processor.stepSoundLayerA(-1); };
    btnPianoNext.onClick = [this]() { processor.stepSoundLayerA(1); };

    setupRotary(sliderPianoSelect, "MODEL", 0.0, 10.0, 5.0);
    sliderPianoSelect.onValueChange = [this]() {
        const auto& pianos = processor.getPianoSounds();
        if (!pianos.empty()) {
            int idx = int(sliderPianoSelect.getValue() * 0.35) % int(pianos.size());
            processor.loadLayerAAsync(pianos[size_t(idx)]);
        }
    };

    setupVerticalFader(sliderPianoVol, "LEVEL", 0.0, 1.5, processor.getEngine().getLayerAConfig().volume.load());
    sliderPianoVol.onValueChange = [this]() {
        if (auto* p = processor.getAPVTS().getParameter("piano_vol"))
            p->setValueNotifyingHost(p->convertTo0to1(float(sliderPianoVol.getValue())));
    };

    setupRotary(sliderPianoRelease, "RELEASE", 0.1, 2.0, processor.getEngine().getRelease());
    sliderPianoRelease.onValueChange = [this]() {
        if (auto* p = processor.getAPVTS().getParameter("piano_release"))
            p->setValueNotifyingHost(p->convertTo0to1(float(sliderPianoRelease.getValue())));
    };

    // Continuous TONE / CUTOFF Knob (Warm/Dark <-> Natural <-> Bright/Open)
        setupRotary(sliderTune, "Tune", -12.0, 12.0, 0.0);
    sliderTune.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    setupRotary(sliderTone, "TONE", 0.0, 1.0, processor.getEngine().getTone());
    sliderTone.onValueChange = [this]() {
        const float val = float(sliderTone.getValue());
        processor.getEngine().setTone(val);
        if (auto* p = processor.getAPVTS().getParameter("tone"))
            p->setValueNotifyingHost(p->convertTo0to1(val));
        repaint();
    };

    // Continuous RESONANCE Knob (Acoustic string body resonance)
    setupRotary(sliderResonanceKnob, "RESONANCE", 0.0, 1.0, processor.getEngine().getResonance());
    sliderResonanceKnob.onValueChange = [this]() {
        const float val = float(sliderResonanceKnob.getValue());
        processor.getEngine().setResonance(val);
        if (auto* p = processor.getAPVTS().getParameter("piano_res_amount"))
            p->setValueNotifyingHost(p->convertTo0to1(val));
        repaint();
    };

    // Nord Stage 4 Acoustic Realism Engine
    setupPillButton(btnCatGrand, "Piano", true);
    setupPillButton(btnCatUpright, "UPRIGHT", true);
    setupPillButton(btnStringRes, "STRING RES", true);
    setupPillButton(btnPedalNoise, "PEDAL NOISE", true);
    setupPillButton(btnSoftRelease, "SOFT RELEASE", true);
    setupPillButton(btnPedalPolarity, processor.getEngine().isPedalPolarityInverted() ? "PEDAL: INV" : "PEDAL: NORM", true);

    const juce::String initialSound = processor.getLayerAName();
    const bool isUpright = initialSound.containsIgnoreCase("Upright");
    btnCatGrand.setToggleState(!isUpright, juce::dontSendNotification);
    btnCatUpright.setToggleState(isUpright, juce::dontSendNotification);

    btnCatGrand.onClick = [this]() {
        btnCatGrand.setToggleState(true, juce::dontSendNotification);
        btnCatUpright.setToggleState(false, juce::dontSendNotification);
        for (const auto& f : processor.getPianoSounds()) {
            if (f.getFileName().containsIgnoreCase("Grand")) {
                processor.loadLayerAAsync(f);
                break;
            }
        }
    };

    btnCatUpright.onClick = [this]() {
        btnCatGrand.setToggleState(false, juce::dontSendNotification);
        btnCatUpright.setToggleState(true, juce::dontSendNotification);
        for (const auto& f : processor.getPianoSounds()) {
            if (f.getFileName().containsIgnoreCase("Upright")) {
                processor.loadLayerAAsync(f);
                break;
            }
        }
    };

    btnStringRes.setToggleState(processor.getEngine().isStringResOn(), juce::dontSendNotification);
    btnPedalNoise.setToggleState(processor.getEngine().isPedalNoiseOn(), juce::dontSendNotification);
    btnSoftRelease.setToggleState(processor.getEngine().isSoftRelease(), juce::dontSendNotification);
    btnPedalPolarity.setToggleState(processor.getEngine().isPedalPolarityInverted(), juce::dontSendNotification);

    btnStringRes.onClick = [this]() {
        const bool on = btnStringRes.getToggleState();
        processor.getEngine().setStringRes(on);
        if (auto* p = processor.getAPVTS().getParameter("piano_res"))
            p->setValueNotifyingHost(on ? 1.0f : 0.0f);
    };
    btnPedalNoise.onClick = [this]() {
        const bool on = btnPedalNoise.getToggleState();
        processor.getEngine().setPedalNoise(on);
        if (auto* p = processor.getAPVTS().getParameter("piano_pedal"))
            p->setValueNotifyingHost(on ? 1.0f : 0.0f);
    };
    btnSoftRelease.onClick = [this]() {
        const bool on = btnSoftRelease.getToggleState();
        processor.getEngine().setSoftRelease(on);
        if (auto* p = processor.getAPVTS().getParameter("piano_soft_rel"))
            p->setValueNotifyingHost(on ? 1.0f : 0.0f);
    };
    btnPedalPolarity.onClick = [this]() {
        const bool inv = btnPedalPolarity.getToggleState();
        processor.getEngine().setPedalPolarityInverted(inv);
        btnPedalPolarity.setButtonText(inv ? "PEDAL: INV" : "PEDAL: NORM");
        repaint();
    };

    // Programs 1 to 8 & Flexible STORE / SAVE System
    auto toggleStoreMode = [this]() {
        storeModeActive = !storeModeActive;
        if (storeModeActive) {
            btnStore.setButtonText("SELECT 1-8");
            btnSave.setButtonText("CANCEL");
            bannerMessage = "STORE MODE: SELECT DESTINATION PROGRAM (1 - 8)";
            bannerTimer = 180; // 6 seconds
        } else {
            btnStore.setButtonText("STORE");
            btnSave.setButtonText("SAVE");
            bannerMessage = "";
            bannerTimer = 0;
        }
        repaint();
    };

    for (int i = 0; i < 8; ++i) {
        setupPillButton(btnPrograms[size_t(i)], juce::String(i + 1), true);
        btnPrograms[size_t(i)].onClick = [this, i, toggleStoreMode]() {
            if (storeModeActive) {
                processor.saveCurrentToProgram(i);
                storeModeActive = false;
                btnStore.setButtonText("STORE");
                btnSave.setButtonText("SAVE");
                bannerMessage = "CURRENT SOUND STORED TO PROGRAM " + juce::String(i + 1) + "!";
                bannerTimer = 90;
                processor.selectProgram(i);
                updateAllControlsFromEngine();
            } else {
                processor.selectProgram(i);
                updateAllControlsFromEngine();
            }
        };
    }
    btnPrograms[size_t(processor.getCurrentProgramIndex())].setToggleState(true, juce::dontSendNotification);

    setupPillButton(btnStore, "STORE", false);
    setupPillButton(btnSave, "SAVE", false);
    btnSave.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGB(229, 57, 53));
    setupPillButton(btnRescanLibrary, "RESCAN SOUNDS", false);

    btnStore.onClick = toggleStoreMode;
    btnSave.onClick = [this, toggleStoreMode]() {
        if (storeModeActive) {
            toggleStoreMode();
        } else {
            const int cur = processor.getCurrentProgramIndex();
            processor.saveCurrentToProgram(cur);
            bannerMessage = "PROGRAM " + juce::String(cur + 1) + " SAVED!";
            bannerTimer = 90;
            repaint();
        }
    };
    btnRescanLibrary.onClick = [this]() {
        processor.rescanSoundLibraries();
        if (processor.getCurrentFilePath().isEmpty() && !processor.getPianoSounds().empty()) {
            processor.loadLayerAAsync(processor.getPianoSounds()[0]);
        }
        repaint();
    };

    //==========================================================================
    // Worship Pad Layer (Layer B) Setup
    //==========================================================================
    setupPillButton(btnPadOn, "ON", true);
    btnPadOn.setToggleState(processor.getEngine().getLayerBConfig().enabled.load(), juce::dontSendNotification);
    btnPadOn.onClick = [this]() {
        const bool on = btnPadOn.getToggleState();
        processor.getEngine().getLayerBConfig().enabled.store(on, std::memory_order_relaxed);
        if (auto* p = processor.getAPVTS().getParameter("pad_on"))
            p->setValueNotifyingHost(on ? 1.0f : 0.0f);
        repaint();
    };

    setupPillButton(btnPadPrev, "<", false);
    btnPadPrev.onClick = [this]() {
        processor.stepSoundLayerB(-1);
        btnPadSelect.setButtonText(processor.getPadName());
        repaint();
    };

    setupPillButton(btnPadNext, ">", false);
    btnPadNext.onClick = [this]() {
        processor.stepSoundLayerB(1);
        btnPadSelect.setButtonText(processor.getPadName());
        repaint();
    };

    setupPillButton(btnPadSelect, processor.getPadName(), false);
    btnPadSelect.onClick = [this]() {
        juce::PopupMenu menu;
        const auto& pads = processor.getPadSounds();
        for (size_t i = 0; i < pads.size(); ++i) {
            menu.addItem(int(i + 1), pads[i].getFileNameWithoutExtension());
        }
        menu.addSeparator();
        menu.addItem(1000, "Carregar outro Pad / Synth (.nsmp4)...");

        auto safe = juce::Component::SafePointer<NordPianoPrototypeAudioProcessorEditor>(this);
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&btnPadSelect), [safe, pads](int result) {
            if (!safe) return;
            if (result >= 1 && result <= int(pads.size())) {
                safe->processor.loadLayerBAsync(pads[size_t(result - 1)]);
                safe->btnPadSelect.setButtonText(safe->processor.getPadName());
                safe->repaint();
            } else if (result == 1000) {
                juce::File initialDir(safe->processor.getCustomLibraryFolder());
                if (!initialDir.isDirectory()) {
                    initialDir = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory().getChildFile("Sounds");
                }
                if (!initialDir.isDirectory()) {
                    initialDir = juce::File("E:\\Disco Elisson\\NORD");
                }
                safe->chooser = std::make_unique<juce::FileChooser>(
                    "Selecione um Pad / Synth (.nsmp / .nsmp4)",
                    initialDir,
                    "*.nsmp;*.nsmp4;*.npno"
                );
                safe->chooser->launchAsync(
                    juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                    [safe](const juce::FileChooser& fc) {
                        if (!safe) return;
                        auto file = fc.getResult();
                        if (file.existsAsFile()) {
                            safe->processor.loadLayerBAsync(file);
                            safe->btnPadSelect.setButtonText(safe->processor.getPadName());
                            safe->repaint();
                        }
                    }
                );
            }
        });
    };

    setupVerticalFader(sliderPadVol, "PAD LEVEL", 0.0, 1.5, processor.getEngine().getLayerBConfig().volume.load());
    sliderPadVol.onValueChange = [this]() {
        if (auto* p = processor.getAPVTS().getParameter("pad_vol"))
            p->setValueNotifyingHost(p->convertTo0to1(float(sliderPadVol.getValue())));
    };

    setupRotary(sliderPadCutoff, "CUTOFF", 150.0, 14000.0, processor.getEngine().getPadCutoff());
    sliderPadCutoff.onValueChange = [this]() {
        if (auto* p = processor.getAPVTS().getParameter("pad_cutoff"))
            p->setValueNotifyingHost(p->convertTo0to1(float(sliderPadCutoff.getValue())));
    };

    setupRotary(sliderPadRes, "RESONANCE", 0.5, 5.0, processor.getEngine().getPadResonance());
    sliderPadRes.onValueChange = [this]() {
        if (auto* p = processor.getAPVTS().getParameter("pad_resonance"))
            p->setValueNotifyingHost(p->convertTo0to1(float(sliderPadRes.getValue())));
    };

    setupRotary(sliderPadAttack, "ATTACK", 0.01, 3.5, processor.getEngine().getPadAttack());
    sliderPadAttack.onValueChange = [this]() {
        if (auto* p = processor.getAPVTS().getParameter("pad_attack"))
            p->setValueNotifyingHost(p->convertTo0to1(float(sliderPadAttack.getValue())));
    };

    setupRotary(sliderPadRelease, "RELEASE", 0.1, 5.5, processor.getEngine().getPadRelease());
    sliderPadRelease.onValueChange = [this]() {
        if (auto* p = processor.getAPVTS().getParameter("pad_release"))
            p->setValueNotifyingHost(p->convertTo0to1(float(sliderPadRelease.getValue())));
    };

    //==========================================================================
    // Column 3: Nord Master Effects Rack & Output
    //==========================================================================
    setupRotary(sliderCompressor, "AMOUNT", 0.0, 10.0, processor.getEngine().getCompressorAmount());
    setupPillButton(btnCompressorOn, "ON", true);
    btnCompressorOn.setToggleState(processor.getEngine().isCompressorOn(), juce::dontSendNotification);
    btnCompressorOn.onClick = [this]() {
        if (auto* p = processor.getAPVTS().getParameter("comp_on"))
            p->setValueNotifyingHost(btnCompressorOn.getToggleState() ? 1.0f : 0.0f);
    };
    sliderCompressor.onValueChange = [this]() {
        if (auto* p = processor.getAPVTS().getParameter("comp_amount"))
            p->setValueNotifyingHost(p->convertTo0to1(float(sliderCompressor.getValue())));
    };

    setupRotary(sliderDelayTime, "TIME", 0.0, 10.0, processor.getEngine().getDelayTime());
    setupRotary(sliderDelayFeedback, "FEEDBACK", 0.0, 10.0, processor.getEngine().getDelayFeedback());
    setupPillButton(btnDelayOn, "ON", true);
    btnDelayOn.setToggleState(processor.getEngine().isDelayOn(), juce::dontSendNotification);
    btnDelayOn.onClick = [this]() {
        if (auto* p = processor.getAPVTS().getParameter("delay_on"))
            p->setValueNotifyingHost(btnDelayOn.getToggleState() ? 1.0f : 0.0f);
    };
    sliderDelayTime.onValueChange = [this]() {
        if (auto* p = processor.getAPVTS().getParameter("delay_time"))
            p->setValueNotifyingHost(p->convertTo0to1(float(sliderDelayTime.getValue())));
    };
    sliderDelayFeedback.onValueChange = [this]() {
        if (auto* p = processor.getAPVTS().getParameter("delay_feedback"))
            p->setValueNotifyingHost(p->convertTo0to1(float(sliderDelayFeedback.getValue())));
    };

    setupRotary(sliderReverbAmount, "AMOUNT", 0.0, 10.0, processor.getEngine().getReverbAmount());
    setupPillButton(btnReverbOn, "ON", true);
    btnReverbOn.setToggleState(processor.getEngine().isReverbOn(), juce::dontSendNotification);
    btnReverbOn.onClick = [this]() {
        if (auto* p = processor.getAPVTS().getParameter("reverb_on"))
            p->setValueNotifyingHost(btnReverbOn.getToggleState() ? 1.0f : 0.0f);
    };
    sliderReverbAmount.onValueChange = [this]() {
        if (auto* p = processor.getAPVTS().getParameter("reverb_amount"))
            p->setValueNotifyingHost(p->convertTo0to1(float(sliderReverbAmount.getValue())));
    };

    float defTone = 5.0f;
    if (auto* p = processor.getAPVTS().getRawParameterValue("reverb_tone")) defTone = p->load();

    setupRotary(sliderReverbTone, "TONE", 0.0, 10.0, defTone);
    sliderReverbTone.onValueChange = [this]() {
        if (auto* p = processor.getAPVTS().getParameter("reverb_tone"))
            p->setValueNotifyingHost(p->convertTo0to1(float(sliderReverbTone.getValue())));
    };

    setupPillButton(btnReverbType, processor.getReverbEngine().getTypeName(), false);
    btnReverbType.onClick = [this]() {
        int nextType = (processor.getReverbEngine().getType() + 1) % 5;
        processor.getReverbEngine().setType(nextType);
        if (auto* p = processor.getAPVTS().getParameter("reverb_type"))
            p->setValueNotifyingHost(p->convertTo0to1(float(nextType)));
        btnReverbType.setButtonText(processor.getReverbEngine().getTypeName());
        repaint();
    };

    setupPillButton(btnReverbVar, processor.getReverbEngine().getVariationName(), false);
    btnReverbVar.onClick = [this]() {
        int nextVar = (processor.getReverbEngine().getVariation() + 1) % 3;
        processor.getReverbEngine().setVariation(nextVar);
        if (auto* p = processor.getAPVTS().getParameter("reverb_variation"))
            p->setValueNotifyingHost(p->convertTo0to1(float(nextVar)));
        btnReverbVar.setButtonText(processor.getReverbEngine().getVariationName());
        repaint();
    };

    setupRotary(sliderChorusDepth, "CHORUS", 0.0, 10.0, processor.getEngine().getChorusDepth());
    sliderChorusDepth.onValueChange = [this]() {
        if (auto* p = processor.getAPVTS().getParameter("chorus_depth"))
            p->setValueNotifyingHost(p->convertTo0to1(float(sliderChorusDepth.getValue())));
    };

    setupRotary(sliderTremoloDepth, "TREMOLO", 0.0, 10.0, processor.getEngine().getTremoloDepth());

    // Ultimate Stage Pianos Controls Setup
    setupRotary(sliderPartMix, "PART MIX", 0.0, 1.0, 0.5);
    sliderPartMix.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    sliderPartMix.onValueChange = [this]() {
        float mix = float(sliderPartMix.getValue());
        processor.getEngine().getLayerAConfig().volume.store(std::cos(mix * 1.570796f));
        processor.getEngine().getLayerBConfig().volume.store(std::sin(mix * 1.570796f));
        repaint();
    };

    setupPillButton(btnOctaveDown, "<", false);
    setupPillButton(btnOctaveUp, ">", false);
    btnOctaveDown.onClick = [this]() { repaint(); };
    btnOctaveUp.onClick = [this]() { repaint(); };

    setupPillButton(btnPianoSelect, "PIANO SELECT", false);
    btnPianoSelect.onClick = [this]() {
        juce::PopupMenu menu;
        const auto& pianos = processor.getPianoSounds();
        for (size_t i = 0; i < pianos.size(); ++i) {
            menu.addItem(int(i + 1), pianos[i].getFileNameWithoutExtension());
        }
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&btnPianoSelect), [this, pianos](int result) {
            if (result >= 1 && result <= int(pianos.size())) {
                processor.loadLayerAAsync(pianos[size_t(result - 1)]);
                repaint();
            }
        });
    };

    setupPillButton(btnLoadNord, "LOAD", false);
    setupPillButton(btnSaveNord, "SAVE", false);
    btnLoadNord.onClick = [this]() {
        if (chooser != nullptr) return;
        chooser = std::make_unique<juce::FileChooser>("Load OmniStage Preset", juce::File::getSpecialLocation(juce::File::userDocumentsDirectory), "*.preset;*.xml");
        chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this](const juce::FileChooser&) {});
    };
    btnSaveNord.onClick = [this]() {
        const int cur = processor.getCurrentProgramIndex();
        processor.saveCurrentToProgram(cur);
        bannerMessage = "PROGRAM " + juce::String(cur + 1) + " SAVED!";
        bannerTimer = 90;
        repaint();
    };

    // Signature Nord Red tactile buttons
    btnStore.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGB(229, 57, 53));
    btnTouch1.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGB(229, 57, 53));
    btnTouch1.setButtonText("TOUCH");
    btnTapTempo.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGB(229, 57, 53));
    btnTapTempo.setButtonText("TAP DELAY");

    btnStringRes.setButtonText("STR RES");
    btnPedalNoise.setButtonText("OPN STR");
    setupPillButton(btnTimbreSoft, "LNG REL", true);
    setupPillButton(btnTimbreMid, "NO SENS", true);
    setupPillButton(btnTimbreRaw, "NO PDL", true);
    setupPillButton(btnTimbreBright, "DOUBLE", true);
    btnPadSelect.setButtonText("SYNTH SELECT");
    btnTapTempo.setButtonText("TAP DELAY");
    btnTimbreMid.setButtonText("NO SENS");
    btnTimbreRaw.setButtonText("NO PDL");
    btnTimbreBright.setButtonText("DOUBLE");
    btnTouch2.setButtonText("LEARN");
    btnCompressorOn.setButtonText("INTENSITY");
    btnDelayOn.setButtonText("INTENSITY");
    btnQuickSelect.setButtonText("Omni Stage Workstation 1.4");
    sliderTremoloDepth.onValueChange = [this]() {
        if (auto* p = processor.getAPVTS().getParameter("tremolo_depth"))
            p->setValueNotifyingHost(p->convertTo0to1(float(sliderTremoloDepth.getValue())));
    };

        addAndMakeVisible(btnTapTempo);
    setupPillButton(btnTapTempo, "TAP DELAY", false);
    btnTapTempo.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGB(229, 57, 53));
    btnTapTempo.setColour(juce::TextButton::textColourOffId, juce::Colour::fromRGB(255, 255, 255));
    addAndMakeVisible(btnTransDown);
    addAndMakeVisible(btnTransUp);
    btnTransDown.onClick = [this]() {
        if (auto* p = processor.getAPVTS().getParameter("transpose")) {
            int t = processor.getEngine().getTranspose() - 1;
            p->setValueNotifyingHost(p->convertTo0to1(float(t)));
        }
        repaint();
    };
    btnTransUp.onClick = [this]() {
        if (auto* p = processor.getAPVTS().getParameter("transpose")) {
            int t = processor.getEngine().getTranspose() + 1;
            p->setValueNotifyingHost(p->convertTo0to1(float(t)));
        }
        repaint();
    };

    addAndMakeVisible(btnTuneDown);
    addAndMakeVisible(btnTuneUp);
    btnTuneDown.onClick = [this]() {
        if (auto* p = processor.getAPVTS().getParameter("tune")) {
            float t = processor.getEngine().getTune() - 0.05f;
            p->setValueNotifyingHost(p->convertTo0to1(t));
        }
        repaint();
    };
    btnTuneUp.onClick = [this]() {
        if (auto* p = processor.getAPVTS().getParameter("tune")) {
            float t = processor.getEngine().getTune() + 0.05f;
            p->setValueNotifyingHost(p->convertTo0to1(t));
        }
        repaint();
    };

    setupMixerFader(sliderMainVolume, juce::Colour::fromRGB(229, 57, 53), 1.0);
    sliderMainVolume.setRange(0.0, 10.0, 0.01);
    sliderMainVolume.setValue(processor.getEngine().getMainVolume(), juce::dontSendNotification);
    sliderMainVolume.onValueChange = [this]() {
        if (auto* p = processor.getAPVTS().getParameter("main_volume"))
            p->setValueNotifyingHost(p->convertTo0to1(float(sliderMainVolume.getValue())));
    };

    //==========================================================================
    // Bottom: 88-Key Interactive Piano Keyboard
    //==========================================================================
    pianoKeyboard = std::make_unique<InteractivePianoKeyboard>(processor);
    // Piano keyboard removed - hardware console mode
    if (pianoKeyboard != nullptr) pianoKeyboard->setVisible(false);

    // Initial Window Bounds (1024 x 736)
    setSize(1024, 768);
    setResizable(false, false);

    setWantsKeyboardFocus(true);
    addKeyListener(this);

    midiMapOverlay = std::make_unique<MidiMapOverlay>(*this, processor);
    addChildComponent(*midiMapOverlay);
    midiMapOverlay->setVisible(false);

    startTimerHz(30);
}

NordPianoPrototypeAudioProcessorEditor::~NordPianoPrototypeAudioProcessorEditor() {
    removeKeyListener(this);
    stopTimer();
    setLookAndFeel(nullptr);
}

void NordPianoPrototypeAudioProcessorEditor::updateAllControlsFromEngine() {
    btnEqPiano.setToggleState(eqTargetIsPiano, juce::dontSendNotification);
    btnEqPad.setToggleState(!eqTargetIsPiano, juce::dontSendNotification);
    if (eqTargetIsPiano) {
        sliderTreble.setValue(processor.getEngine().getEqTreble(), juce::dontSendNotification);
        sliderMid.setValue(processor.getEngine().getEqMid(), juce::dontSendNotification);
        sliderBass.setValue(processor.getEngine().getEqBass(), juce::dontSendNotification);
    } else {
        sliderTreble.setValue(processor.getEngine().getPadEqTreble(), juce::dontSendNotification);
        sliderMid.setValue(processor.getEngine().getPadEqMid(), juce::dontSendNotification);
        sliderBass.setValue(processor.getEngine().getPadEqBass(), juce::dontSendNotification);
    }
    sliderDynamics.setValue(processor.getEngine().getDynamics(), juce::dontSendNotification);
    sliderResonanceKnob.setValue(processor.getEngine().getResonance(), juce::dontSendNotification);
    sliderTone.setValue(processor.getEngine().getTone(), juce::dontSendNotification);
    sliderPianoRelease.setValue(processor.getEngine().getRelease(), juce::dontSendNotification);
    sliderPianoVol.setValue(processor.getEngine().getLayerAConfig().volume.load(std::memory_order_relaxed), juce::dontSendNotification);
    sliderPadVol.setValue(processor.getEngine().getLayerBConfig().volume.load(std::memory_order_relaxed), juce::dontSendNotification);
    sliderPadCutoff.setValue(processor.getEngine().getPadCutoff(), juce::dontSendNotification);
    sliderPadRes.setValue(processor.getEngine().getPadResonance(), juce::dontSendNotification);
    sliderPadAttack.setValue(processor.getEngine().getPadAttack(), juce::dontSendNotification);
    sliderPadRelease.setValue(processor.getEngine().getPadRelease(), juce::dontSendNotification);
    sliderCompressor.setValue(processor.getEngine().getCompressorAmount(), juce::dontSendNotification);
    sliderDelayTime.setValue(processor.getEngine().getDelayTime(), juce::dontSendNotification);
    sliderDelayFeedback.setValue(processor.getEngine().getDelayFeedback(), juce::dontSendNotification);
    sliderReverbAmount.setValue(processor.getEngine().getReverbAmount(), juce::dontSendNotification);
    sliderReverbTone.setValue(processor.getReverbEngine().getTone(), juce::dontSendNotification);
    sliderChorusDepth.setValue(processor.getEngine().getChorusDepth(), juce::dontSendNotification);
    sliderTremoloDepth.setValue(processor.getEngine().getTremoloDepth(), juce::dontSendNotification);
    sliderMainVolume.setValue(processor.getEngine().getMainVolume(), juce::dontSendNotification);

    const int tCur = processor.getEngine().getTouchCurve();
    btnTouch1.setToggleState(tCur == 0, juce::dontSendNotification);
    btnTouch2.setToggleState(tCur == 1, juce::dontSendNotification);
    btnTouch3.setToggleState(tCur == 2, juce::dontSendNotification);

    btnPianoOn.setToggleState(processor.getEngine().getLayerAConfig().enabled.load(std::memory_order_relaxed), juce::dontSendNotification);
    btnStringRes.setToggleState(processor.getEngine().isStringResOn(), juce::dontSendNotification);
    btnPedalNoise.setToggleState(processor.getEngine().isPedalNoiseOn(), juce::dontSendNotification);
    btnSoftRelease.setToggleState(processor.getEngine().isSoftRelease(), juce::dontSendNotification);
    btnPadOn.setToggleState(processor.getEngine().getLayerBConfig().enabled.load(std::memory_order_relaxed), juce::dontSendNotification);
    btnCompressorOn.setToggleState(processor.getEngine().isCompressorOn(), juce::dontSendNotification);
    btnDelayOn.setToggleState(processor.getEngine().isDelayOn(), juce::dontSendNotification);
    btnReverbOn.setToggleState(processor.getEngine().isReverbOn(), juce::dontSendNotification);

    btnReverbType.setButtonText(processor.getReverbEngine().getTypeName());
    btnReverbVar.setButtonText(processor.getReverbEngine().getVariationName());

    const int curProg = processor.getCurrentProgramIndex();
    for (size_t i = 0; i < 8; ++i) {
        btnPrograms[i].setToggleState(int(i) == curProg, juce::dontSendNotification);
    }
    repaint();
}

bool NordPianoPrototypeAudioProcessorEditor::keyPressed(const juce::KeyPress& key) {
    return handleKeyPress(key);
}

bool NordPianoPrototypeAudioProcessorEditor::keyPressed(const juce::KeyPress& key, juce::Component* originatingComponent) {
    juce::ignoreUnused(originatingComponent);
    return handleKeyPress(key);
}

bool NordPianoPrototypeAudioProcessorEditor::handleKeyPress(const juce::KeyPress& key) {
    const int keyCode = key.getKeyCode();
    const juce::juce_wchar ch = key.getTextCharacter();

    // 1. Numeric Keypad and Top-Row Numbers 1 to 8: Instant Timbre / Program Switch OR Store Destination
    int targetProgram = -1;
    if (ch >= '1' && ch <= '8') {
        targetProgram = ch - '1';
    } else if (keyCode >= juce::KeyPress::numberPad1 && keyCode <= juce::KeyPress::numberPad8) {
        targetProgram = keyCode - juce::KeyPress::numberPad1;
    }

    if (targetProgram >= 0 && targetProgram < 8) {
        if (storeModeActive) {
            processor.saveCurrentToProgram(targetProgram);
            storeModeActive = false;
            btnStore.setButtonText("STORE");
            btnSave.setButtonText("SAVE");
            bannerMessage = "CURRENT SOUND STORED TO PROGRAM " + juce::String(targetProgram + 1) + "!";
            bannerTimer = 90;
            processor.selectProgram(targetProgram);
            updateAllControlsFromEngine();
            return true;
        } else {
            processor.selectProgram(targetProgram);
            updateAllControlsFromEngine();
            return true;
        }
    }

    // Cancel Store Mode via Esc
    if (keyCode == juce::KeyPress::escapeKey && storeModeActive) {
        storeModeActive = false;
        btnStore.setButtonText("STORE");
        btnSave.setButtonText("SAVE");
        bannerMessage = "";
        bannerTimer = 0;
        repaint();
        return true;
    }

    // 2. Shortcut 'S' / 's' or Ctrl+S: Quick Save Current Preset
    if (ch == 's' || ch == 'S' || (key.getModifiers().isCtrlDown() && (ch == 's' || ch == 'S' || keyCode == 'S' || keyCode == 's'))) {
        const int cur = processor.getCurrentProgramIndex();
        processor.saveCurrentToProgram(cur);
        bannerMessage = "PROGRAM " + juce::String(cur + 1) + " SAVED!";
        bannerTimer = 90;
        repaint();
        return true;
    }

    // 3. Arrow Left / Arrow Right: Step Piano Library Sounds
    if (keyCode == juce::KeyPress::leftKey) {
        processor.stepSoundLayerA(-1);
        repaint();
        return true;
    }
    if (keyCode == juce::KeyPress::rightKey) {
        processor.stepSoundLayerA(1);
        repaint();
        return true;
    }

    return false;
}

//==============================================================================
// Helpers: Setup Controls
//==============================================================================
static void drawVerticalVuMeter(juce::Graphics& g, float x, float y, float w, float h, float levelNorm) {
    // Smoky Acrylic Bezel Background
    g.setColour(juce::Colour::fromRGB(6, 8, 12));
    g.fillRoundedRectangle(x - 2.5f, y - 2.5f, w + 5.0f, h + 5.0f, 3.0f);
    g.setColour(juce::Colour::fromRGB(36, 42, 54));
    g.drawRoundedRectangle(x - 2.5f, y - 2.5f, w + 5.0f, h + 5.0f, 3.0f, 1.0f);

    const int numSegments = 16;
    const float segGap = 2.0f;
    const float segH = (h - float(numSegments - 1) * segGap) / float(numSegments);
    const int activeSegments = juce::jlimit(0, numSegments, int(std::round(levelNorm * float(numSegments))));

    for (int i = 0; i < numSegments; ++i) {
        float sy = y + h - float(i + 1) * (segH + segGap);
        bool isLit = (i < activeSegments);

        juce::Colour litColor;
        juce::Colour unlitColor;
        if (i < 10) {
            litColor = juce::Colour::fromRGB(45, 225, 60);
            unlitColor = juce::Colour::fromRGB(12, 28, 16);
        } else if (i < 13) {
            litColor = juce::Colour::fromRGB(255, 195, 30);
            unlitColor = juce::Colour::fromRGB(34, 26, 8);
        } else {
            litColor = juce::Colour::fromRGB(255, 42, 52);
            unlitColor = juce::Colour::fromRGB(38, 8, 10);
        }

        if (isLit) {
            g.setColour(litColor.withAlpha(0.40f));
            g.fillRoundedRectangle(x - 1.2f, sy - 1.0f, w + 2.4f, segH + 2.0f, 1.5f);
            g.setColour(litColor);
        } else {
            g.setColour(unlitColor);
        }
        g.fillRoundedRectangle(x, sy, w, segH, 1.0f);

        if (isLit) {
            g.setColour(juce::Colour::fromRGB(255, 255, 255).withAlpha(0.6f));
            g.fillRect(x + 1.0f, sy + 0.5f, w - 2.0f, 1.0f);
        }
    }
}

void NordPianoPrototypeAudioProcessorEditor::setupVerticalFader(juce::Slider& s, const juce::String& text, double minV, double maxV, double defV) {
    juce::ignoreUnused(text);
    s.setSliderStyle(juce::Slider::LinearVertical);
    s.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    s.setRange(minV, maxV, 0.01);
    s.setValue(defV, juce::dontSendNotification);
    s.setLookAndFeel(&blackKnobLAF);
    addAndMakeVisible(s);
}

void NordPianoPrototypeAudioProcessorEditor::setupRotary(juce::Slider& s, const juce::String& text, double minV, double maxV, double defV) {
    juce::ignoreUnused(text);
    s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    s.setRange(minV, maxV, 0.01);
    s.setValue(defV, juce::dontSendNotification);
    addAndMakeVisible(s);
}

void NordPianoPrototypeAudioProcessorEditor::setupPillButton(juce::TextButton& btn, const juce::String& text, bool isToggle) {
    btn.setButtonText(text);
    btn.setClickingTogglesState(isToggle);
    addAndMakeVisible(btn);
}

//==============================================================================
// Timer Callback: UI Synchronization (30 FPS)
//==============================================================================
void NordPianoPrototypeAudioProcessorEditor::timerCallback() {
    loadingAnimPulse++;

    if (processor.isMidiMapModeActive() && midiMapOverlay != nullptr && midiMapOverlay->isVisible()) {
        midiMapOverlay->repaint();
    }

    if (processor.consumeProgramChangedFlag()) {
        updateAllControlsFromEngine();
    }
    const int reqEq = processor.consumeEqTargetFlag();
    if (reqEq == 0 && !eqTargetIsPiano) {
        eqTargetIsPiano = true;
        updateAllControlsFromEngine();
    } else if (reqEq == 1 && eqTargetIsPiano) {
        eqTargetIsPiano = false;
        updateAllControlsFromEngine();
    }
    if (bannerTimer > 0) {
        if (--bannerTimer == 0) {
            bannerMessage = "";
            repaint(244, 106, 536, 90);
        }
    }

    // 1. Live MIDI Note Activity Indicator
    if (processor.consumeMidiActivity()) {
        midiLedLit = true;
        midiDecayTimer = 8;
        repaint(860, 532, 148, 138); // repaint status panel
    } else if (midiLedLit) {
        if (--midiDecayTimer <= 0) {
            midiLedLit = false;
            repaint(860, 532, 148, 138);
        }
    }

    // 2. Refresh OLED screen if loaded piano name changes
    const juce::String currentPiano = processor.getLayerAName();
    if (currentPiano != lastKnownPiano) {
        lastKnownPiano = currentPiano;
        btnQuickSelect.setButtonText(currentPiano);
        repaint(230, 80, 564, 440);
    }

    // 3. Loading animation update
    const bool isLoading = processor.isLoadingLayerA();
    if (isLoading != lastKnownLoadingA) {
        lastKnownLoadingA = isLoading;
        repaint(230, 80, 564, 440);
    } else if (isLoading) {
        repaint(230, 80, 564, 440);
    }

    // 4. Always repaint keyboard on live note playback
    if (pianoKeyboard) {
        pianoKeyboard->repaint();
    }

    // 5. Sync Worship Pad UI button text and toggle state
    const juce::String padName = processor.getPadName();
    if (padName.isNotEmpty() && btnPadSelect.getButtonText() != padName) {
        btnPadSelect.setButtonText(padName);
    }
    const bool padEn = processor.getEngine().getLayerBConfig().enabled.load(std::memory_order_relaxed);
    if (btnPadOn.getToggleState() != padEn) {
        btnPadOn.setToggleState(padEn, juce::dontSendNotification);
    }

    // 6. Sync Reverb Type and Variation buttons
    const juce::String rTypeName = processor.getReverbEngine().getTypeName();
    if (btnReverbType.getButtonText() != rTypeName) {
        btnReverbType.setButtonText(rTypeName);
    }
    const juce::String rVarName = processor.getReverbEngine().getVariationName();
    if (btnReverbVar.getButtonText() != rVarName) {
        btnReverbVar.setButtonText(rVarName);
    }

    // 7. Live bidirectional sync of knobs and toggles from MIDI CC / Host Automation
    auto syncRotary = [](juce::Slider& knob, float targetVal) {
        if (!knob.isMouseButtonDown()) {
            if (std::abs(float(knob.getValue()) - targetVal) > 0.005f) {
                knob.setValue(double(targetVal), juce::dontSendNotification);
            }
        }
    };

    if (eqTargetIsPiano) {
        syncRotary(sliderTreble, processor.getEngine().getEqTreble());
        syncRotary(sliderMid, processor.getEngine().getEqMid());
        syncRotary(sliderBass, processor.getEngine().getEqBass());
    } else {
        syncRotary(sliderTreble, processor.getEngine().getPadEqTreble());
        syncRotary(sliderMid, processor.getEngine().getPadEqMid());
        syncRotary(sliderBass, processor.getEngine().getPadEqBass());
    }
    syncRotary(sliderDynamics, processor.getEngine().getDynamics());
    syncRotary(sliderResonanceKnob, processor.getEngine().getResonance());
    syncRotary(sliderTone, processor.getEngine().getTone());
    syncRotary(sliderPianoRelease, processor.getEngine().getRelease());
    syncRotary(sliderPianoVol, processor.getEngine().getLayerAConfig().volume.load(std::memory_order_relaxed));
    syncRotary(sliderPadVol, processor.getEngine().getLayerBConfig().volume.load(std::memory_order_relaxed));
    syncRotary(sliderPadCutoff, processor.getEngine().getPadCutoff());
    syncRotary(sliderPadRes, processor.getEngine().getPadResonance());
    syncRotary(sliderPadAttack, processor.getEngine().getPadAttack());
    syncRotary(sliderPadRelease, processor.getEngine().getPadRelease());
    syncRotary(sliderCompressor, processor.getEngine().getCompressorAmount());
    syncRotary(sliderDelayTime, processor.getEngine().getDelayTime());
    syncRotary(sliderDelayFeedback, processor.getEngine().getDelayFeedback());
    syncRotary(sliderReverbAmount, processor.getEngine().getReverbAmount());
    syncRotary(sliderReverbTone, processor.getReverbEngine().getTone());
    syncRotary(sliderChorusDepth, processor.getEngine().getChorusDepth());
    syncRotary(sliderTremoloDepth, processor.getEngine().getTremoloDepth());
    syncRotary(sliderMainVolume, processor.getEngine().getMainVolume());

    const int tCur = processor.getEngine().getTouchCurve();
    if (btnTouch1.getToggleState() != (tCur == 0)) btnTouch1.setToggleState(tCur == 0, juce::dontSendNotification);
    if (btnTouch2.getToggleState() != (tCur == 1)) btnTouch2.setToggleState(tCur == 1, juce::dontSendNotification);
    if (btnTouch3.getToggleState() != (tCur == 2)) btnTouch3.setToggleState(tCur == 2, juce::dontSendNotification);

    const bool pOn = processor.getEngine().getLayerAConfig().enabled.load(std::memory_order_relaxed);
    if (btnPianoOn.getToggleState() != pOn) btnPianoOn.setToggleState(pOn, juce::dontSendNotification);

    const bool sRes = processor.getEngine().isStringResOn();
    if (btnStringRes.getToggleState() != sRes) btnStringRes.setToggleState(sRes, juce::dontSendNotification);

    const bool pNoise = processor.getEngine().isPedalNoiseOn();
    if (btnPedalNoise.getToggleState() != pNoise) btnPedalNoise.setToggleState(pNoise, juce::dontSendNotification);

    const bool sRel = processor.getEngine().isSoftRelease();
    if (btnSoftRelease.getToggleState() != sRel) btnSoftRelease.setToggleState(sRel, juce::dontSendNotification);

    const bool cOn = processor.getEngine().isCompressorOn();
    if (btnCompressorOn.getToggleState() != cOn) btnCompressorOn.setToggleState(cOn, juce::dontSendNotification);

    const bool dOn = processor.getEngine().isDelayOn();
    if (btnDelayOn.getToggleState() != dOn) btnDelayOn.setToggleState(dOn, juce::dontSendNotification);

    const bool rOn = processor.getEngine().isReverbOn();
    if (btnReverbOn.getToggleState() != rOn) btnReverbOn.setToggleState(rOn, juce::dontSendNotification);

    // 8. Sync Programs 1..8 active toggle state
    const int activeProg = processor.getCurrentProgramIndex();
    for (size_t i = 0; i < 8; ++i) {
        const bool isCur = (int(i) == activeProg);
        if (btnPrograms[i].getToggleState() != isCur) {
            btnPrograms[i].setToggleState(isCur, juce::dontSendNotification);
        }
    }
}

//==============================================================================
// Custom Drawing: Premium Black Edition Metal Panels & Cards
//==============================================================================
//==============================================================================
//==============================================================================
// Custom Drawing: OmniStage Studio Console Helpers
//==============================================================================
static void drawOmniVuMeter(juce::Graphics& g, float x, float y, float w, float h, float levelNorm) {
    // Smoky Acrylic Bezel Background
    g.setColour(juce::Colour::fromRGB(6, 8, 12));
    g.fillRoundedRectangle(x - 1.5f, y - 1.5f, w + 3.0f, h + 3.0f, 2.0f);
    g.setColour(juce::Colour::fromRGB(26, 32, 42));
    g.drawRoundedRectangle(x - 1.5f, y - 1.5f, w + 3.0f, h + 3.0f, 2.0f, 0.8f);

    const int numSegments = 14;
    const float segGap = 2.0f;
    const float segH = (h - float(numSegments - 1) * segGap) / float(numSegments);
    const int activeSegments = juce::jlimit(0, numSegments, int(std::round(levelNorm * float(numSegments))));

    for (int i = 0; i < numSegments; ++i) {
        float sy = y + h - float(i + 1) * (segH + segGap);
        bool isLit = (i < activeSegments);

        juce::Colour litColor;
        juce::Colour unlitColor;
        if (i < 8) {
            litColor = juce::Colour::fromRGB(45, 225, 60);    // Emerald Green
            unlitColor = juce::Colour::fromRGB(12, 28, 16);
        } else if (i < 12) {
            litColor = juce::Colour::fromRGB(255, 195, 30);   // Amber Yellow
            unlitColor = juce::Colour::fromRGB(34, 26, 8);
        } else {
            litColor = juce::Colour::fromRGB(255, 42, 52);    // Ruby Red
            unlitColor = juce::Colour::fromRGB(38, 8, 10);
        }

        if (isLit) {
            g.setColour(litColor.withAlpha(0.35f));
            g.fillRoundedRectangle(x - 1.0f, sy - 0.5f, w + 2.0f, segH + 1.0f, 1.2f);
            g.setColour(litColor);
        } else {
            g.setColour(unlitColor);
        }
        g.fillRoundedRectangle(x, sy, w, segH, 1.0f);

        if (isLit) {
            g.setColour(juce::Colour::fromRGB(255, 255, 255).withAlpha(0.6f));
            g.fillRect(x + 1.0f, sy + 0.5f, w - 2.0f, 1.0f);
        }
    }
}

static void drawOmniPanelCard(juce::Graphics& g, juce::Rectangle<int> bounds, const juce::String& title, bool hasLed = true) {
    auto bf = bounds.toFloat();

    // 1. Outer Deep Drop Shadow
    g.setColour(juce::Colour::fromRGB(0, 0, 0).withAlpha(0.7f));
    g.fillRoundedRectangle(bf.translated(0.0f, 2.0f), 4.0f);

    // 2. Matte Dark Slate Panel Base (#141720 to #0D0F15)
    juce::ColourGradient panelGrad(juce::Colour::fromRGB(20, 24, 32), bf.getX(), bf.getY(),
                                  juce::Colour::fromRGB(12, 14, 19), bf.getX(), bf.getBottom(), false);
    g.setGradientFill(panelGrad);
    g.fillRoundedRectangle(bf, 4.0f);

    // 3. Subtle Nord Crimson Red Border Glow (#E53935 with smooth falloff)
    g.setColour(juce::Colour::fromRGB(229, 57, 53).withAlpha(0.20f));
    g.drawRoundedRectangle(bf.expanded(0.8f), 4.0f, 1.5f);
    g.setColour(juce::Colour::fromRGB(229, 57, 53).withAlpha(0.55f));
    g.drawRoundedRectangle(bf, 4.0f, 1.0f);

    // Top Highlight Reflection
    g.setColour(juce::Colour::fromRGB(255, 255, 255).withAlpha(0.08f));
    g.drawLine(bf.getX() + 4.0f, bf.getY() + 1.0f, bf.getRight() - 4.0f, bf.getY() + 1.0f, 1.0f);

    // 4. Panel Header Title & Status LED
    if (title.isNotEmpty()) {
        const float tx = bf.getX() + 14.0f;
        const float ty = bf.getY() + 8.0f;

        if (hasLed) {
            // Glowing Red LED Dot
            g.setColour(juce::Colour::fromRGB(229, 57, 53).withAlpha(0.40f));
            g.fillEllipse(tx - 2.0f, ty, 10.0f, 10.0f);

            g.setColour(juce::Colour::fromRGB(255, 60, 65));
            g.fillEllipse(tx, ty + 1.5f, 6.5f, 6.5f);

            g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
            g.setColour(juce::Colour::fromRGB(245, 248, 252));
            g.drawText(title, int(tx + 12.0f), int(ty - 2.0f), int(bf.getWidth() - 35.0f), 15, juce::Justification::left);
        } else {
            g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
            g.setColour(juce::Colour::fromRGB(245, 248, 252));
            g.drawText(title, int(tx), int(ty - 2.0f), int(bf.getWidth() - 25.0f), 15, juce::Justification::left);
        }
    }
}

//==============================================================================
// Main Paint Function (1024 x 768 OmniStage Flagship Console)
//==============================================================================
void NordPianoPrototypeAudioProcessorEditor::paint(juce::Graphics& g) {
    // 1. Dark Slate Anodized Console Chassis Background (#0E1117 to #090B0F)
    juce::ColourGradient bgGrad(juce::Colour::fromRGB(15, 18, 24), 0.0f, 0.0f,
                               juce::Colour::fromRGB(8, 10, 14), 0.0f, (float)getHeight(), false);
    g.setGradientFill(bgGrad);
    g.fillAll();

    // Subtle edge framing
    g.setColour(juce::Colour::fromRGB(32, 38, 48));
    g.drawRect(getLocalBounds().toFloat(), 1.0f);

    //==========================================================================
    // 2. TOP BAR (OmniStage Brand, Preset Capsule, Tune, Peak Meters)
    //==========================================================================
    // A. OmniStage Brand Logo (4-bar Soundwave + Omni bold white + Stage red)
    {
        const float logoX = 18.0f;
        const float logoY = 16.0f;

        // 4 red soundwave bars
        const float barH[] = { 14.0f, 26.0f, 20.0f, 10.0f };
        for (int i = 0; i < 4; ++i) {
            float bx = logoX + float(i) * 6.5f;
            float by = logoY + 14.0f - barH[i] * 0.5f;
            g.setColour(juce::Colour::fromRGB(229, 57, 53));
            g.fillRoundedRectangle(bx, by, 3.8f, barH[i], 1.8f);
        }

        // "Omni" (White Bold) + "Stage" (Nord Red Bold)
        g.setFont(juce::Font(juce::FontOptions(22.0f, juce::Font::bold)));
        g.setColour(juce::Colour::fromRGB(255, 255, 255));
        g.drawText("Omni", int(logoX + 32.0f), int(logoY - 4.0f), 54, 26, juce::Justification::left);

        g.setColour(juce::Colour::fromRGB(229, 57, 53));
        g.drawText("Stage", int(logoX + 86.0f), int(logoY - 4.0f), 65, 26, juce::Justification::left);

        // Subtitle "Stage Preset Player"
        g.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::bold)));
        g.setColour(juce::Colour::fromRGB(130, 142, 160));
        g.drawText("Stage Preset Player", int(logoX + 32.0f), int(logoY + 20.0f), 120, 14, juce::Justification::left);
    }

    // B. Preset Capsule Center Box (x: 236, y: 12, w: 420, h: 42)
    {
        juce::Rectangle<float> capsule(236.0f, 12.0f, 420.0f, 42.0f);
        g.setColour(juce::Colour::fromRGB(12, 14, 19));
        g.fillRoundedRectangle(capsule, 4.0f);
        g.setColour(juce::Colour::fromRGB(36, 42, 54));
        g.drawRoundedRectangle(capsule, 4.0f, 1.0f);

        // Subline: Output: st. 1 | Voices: 0 / 512 | Memory: 284 MB
        g.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::bold)));
        g.setColour(juce::Colour::fromRGB(140, 155, 175));

        const int voices = processor.getVoiceCount() > 0 ? processor.getVoiceCount() : 0;
        const double memMb = processor.getTotalMemoryMB();
        juce::String statusStr = "Output: st. 1   |   Voices: " + juce::String(voices) + " / 512   |   Memory: " +
                                 juce::String(memMb > 0.1 ? memMb : 284.0, 0) + " MB";
        g.drawText(statusStr, 240, 32, 412, 16, juce::Justification::centred);
    }

    // C. Settings Gear Button Box (x: 666, y: 14, w: 38, h: 38)
    {
        juce::Rectangle<float> gearBox(666.0f, 14.0f, 38.0f, 38.0f);
        g.setColour(juce::Colour::fromRGB(18, 22, 28));
        g.fillRoundedRectangle(gearBox, 4.0f);
        g.setColour(juce::Colour::fromRGB(45, 52, 65));
        g.drawRoundedRectangle(gearBox, 4.0f, 1.0f);

        // Vector Gear Icon
        const float gcx = gearBox.getCentreX();
        const float gcy = gearBox.getCentreY();
        g.setColour(juce::Colour::fromRGB(180, 192, 210));
        g.drawEllipse(gcx - 5.0f, gcy - 5.0f, 10.0f, 10.0f, 2.0f);
        for (int i = 0; i < 6; ++i) {
            float ang = float(i) * juce::MathConstants<float>::pi / 3.0f;
            g.drawLine(gcx + std::cos(ang) * 5.0f, gcy + std::sin(ang) * 5.0f,
                       gcx + std::cos(ang) * 8.5f, gcy + std::sin(ang) * 8.5f, 2.0f);
        }
    }

    // D. Tune Rotary Knob Readout (x: 720, y: 12)
    {
        g.setFont(juce::Font(juce::FontOptions(8.5f, juce::Font::bold)));
        g.setColour(juce::Colour::fromRGB(140, 152, 170));
        g.drawText("Tune", 764, 14, 38, 12, juce::Justification::left);
        g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
        g.setColour(juce::Colour::fromRGB(240, 245, 255));
        g.drawText("0.00", 764, 30, 38, 14, juce::Justification::left);
    }

    // E. Top-Right Stereo Peak VU Meters (x: 820, y: 14, w: 188, h: 38)
    {
        const float meterX = 824.0f;
        const float meterY = 16.0f;
        const float meterW = 180.0f;
        const float meterH = 14.0f;

        g.setColour(juce::Colour::fromRGB(14, 17, 23));
        g.fillRoundedRectangle(meterX, meterY, meterW, 34.0f, 3.0f);
        g.setColour(juce::Colour::fromRGB(36, 42, 54));
        g.drawRoundedRectangle(meterX, meterY, meterW, 34.0f, 3.0f, 1.0f);

        g.setFont(juce::Font(juce::FontOptions(8.5f, juce::Font::bold)));
        g.setColour(juce::Colour::fromRGB(150, 165, 185));
        g.drawText("L", int(meterX + 6.0f), int(meterY + 2.0f), 12, 12, juce::Justification::left);
        g.drawText("R", int(meterX + 6.0f), int(meterY + 18.0f), 12, 12, juce::Justification::left);

        // Draw horizontal LED segments
        const float startSegX = meterX + 22.0f;
        const int numSegs = 18;
        const float segW = 7.5f;
        for (int s = 0; s < numSegs; ++s) {
            juce::Colour c = (s < 11) ? juce::Colour::fromRGB(45, 225, 60) :
                             (s < 15 ? juce::Colour::fromRGB(255, 195, 30) : juce::Colour::fromRGB(255, 42, 52));
            bool isLit = (s < 9); // default active display
            g.setColour(isLit ? c : c.withAlpha(0.18f));
            g.fillRect(startSegX + float(s) * (segW + 1.2f), meterY + 3.0f, segW, 11.0f);
            g.fillRect(startSegX + float(s) * (segW + 1.2f), meterY + 19.0f, segW, 11.0f);
        }
    }

    //==========================================================================
    // 3. MID SECTION CARDS (y: 112 to 436, h: 324)
    //==========================================================================

    // A. CARD 1: 4-CHANNEL MIXER STRIP (x: 12, y: 112, w: 338, h: 324)
    drawOmniPanelCard(g, {12, 112, 338, 324}, "");
    {
        // Badges at top of each channel
        struct ChannelBadge { const char* letter; const char* title; juce::Colour col; };
        const ChannelBadge badges[] = {
            { "A", "Piano", juce::Colour::fromRGB(229, 57, 53) },        // Red
            { "B", "Worship Pad", juce::Colour::fromRGB(0, 190, 230) },  // Cyan
            { "C", "Synth Lead", juce::Colour::fromRGB(60, 150, 255) },  // Blue
            { "D", "Organ B3", juce::Colour::fromRGB(150, 90, 240) }     // Purple
        };

        for (int i = 0; i < 4; ++i) {
            float bx = 18.0f + float(i) * 82.0f;
            juce::Rectangle<float> bRect(bx, 120.0f, 74.0f, 46.0f);

            // Badge Background
            if (i == 0) {
                // Active Channel A (Nord Red Glow)
                juce::ColourGradient bGrad(juce::Colour::fromRGB(210, 32, 38), bx, 120.0f,
                                           juce::Colour::fromRGB(120, 14, 18), bx, 166.0f, false);
                g.setGradientFill(bGrad);
                g.fillRoundedRectangle(bRect, 3.5f);
                g.setColour(juce::Colour::fromRGB(255, 90, 95));
                g.drawRoundedRectangle(bRect, 3.5f, 1.2f);
            } else {
                juce::ColourGradient bGrad(juce::Colour::fromRGB(20, 26, 36), bx, 120.0f,
                                           juce::Colour::fromRGB(11, 14, 20), bx, 166.0f, false);
                g.setGradientFill(bGrad);
                g.fillRoundedRectangle(bRect, 3.5f);
                g.setColour(badges[i].col.withAlpha(0.6f));
                g.drawRoundedRectangle(bRect, 3.5f, 1.0f);
            }

            // Letter (A, B, C, D)
            g.setFont(juce::Font(juce::FontOptions(13.0f, juce::Font::bold)));
            g.setColour(i == 0 ? juce::Colour::fromRGB(255, 255, 255) : badges[i].col);
            g.drawText(badges[i].letter, int(bx), 123, 74, 18, juce::Justification::centred);

            // Subtitle
            g.setFont(juce::Font(juce::FontOptions(8.5f, juce::Font::bold)));
            g.setColour(juce::Colour::fromRGB(235, 240, 250));
            g.drawText(badges[i].title, int(bx + 2.0f), 140, 70, 22, juce::Justification::centred);

            // Vertical 14-Segment VU Meters next to faders
            float vuLevel = 0.70f;
            if (i == 0) vuLevel = float(sliderVolA.getValue()) / 1.5f;
            else if (i == 1) vuLevel = float(sliderVolB.getValue()) / 1.5f;
            else if (i == 2) vuLevel = float(sliderVolC.getValue()) / 1.5f;
            else if (i == 3) vuLevel = float(sliderVolD.getValue()) / 1.5f;

            drawOmniVuMeter(g, bx + 58.0f, 178.0f, 9.0f, 186.0f, vuLevel);

            // 0.0 dB Digital Readout Box
            juce::Rectangle<float> dbBox(bx + 10.0f, 372.0f, 54.0f, 16.0f);
            g.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::bold)));
            g.setColour(juce::Colour::fromRGB(180, 192, 210));
            g.drawText("0.0 dB", dbBox, juce::Justification::centred);
        }
    }

    // B. CARD 2: OLED DISPLAY & MULTI-PROGRAM (x: 358, y: 112, w: 480, h: 324)
    drawOmniPanelCard(g, {358, 112, 480, 324}, "");
    {
        // OLED Screen Box (x: 444, y: 124, w: 382, h: 148)
        juce::Rectangle<float> oledBox(444.0f, 124.0f, 382.0f, 148.0f);
        juce::ColourGradient oledGrad(juce::Colour::fromRGB(8, 16, 30), 444.0f, 124.0f,
                                     juce::Colour::fromRGB(4, 9, 18), 444.0f, 272.0f, false);
        g.setGradientFill(oledGrad);
        g.fillRoundedRectangle(oledBox, 4.0f);
        g.setColour(juce::Colour::fromRGB(0, 180, 230).withAlpha(0.65f)); // Cyan border glow
        g.drawRoundedRectangle(oledBox, 4.0f, 1.2f);

        // Header Title: 01: Great Is The Lord
        g.setFont(juce::Font(juce::FontOptions(19.0f, juce::Font::bold)));
        g.setColour(juce::Colour::fromRGB(0, 229, 255)); // Electric Cyan
        g.drawText("01: Great Is The Lord", 458, 132, 210, 24, juce::Justification::left);

        // Cyan Horizontal Divider
        g.setColour(juce::Colour::fromRGB(0, 160, 210).withAlpha(0.55f));
        g.drawHorizontalLine(160, 458.0f, 814.0f);

        // Active Layer List (A: Royal Grand, B: Shimmer Pad, C: Bright Lead, D: Organ B3)
        g.setFont(juce::Font(juce::FontOptions(11.5f, juce::Font::bold)));
        g.setColour(juce::Colour::fromRGB(0, 215, 245));
        g.drawText("A:   Royal Grand", 458, 172, 190, 18, juce::Justification::left);
        g.drawText("B:   Shimmer Pad", 458, 194, 190, 18, juce::Justification::left);
        g.drawText("C:   Bright Lead", 458, 216, 190, 18, juce::Justification::left);
        g.drawText("D:   Organ B3", 458, 238, 190, 18, juce::Justification::left);

        // Draw Embedded Grand Piano 3D Wireframe on the right side of the OLED
        if (imgOledGrandPiano.isValid()) {
            g.drawImage(imgOledGrandPiano,
                        674, 136, 146, 124,
                        0, 0, imgOledGrandPiano.getWidth(), imgOledGrandPiano.getHeight());
        }

        // Sub-divider line above PROGRAMS
        g.setColour(juce::Colour::fromRGB(36, 42, 54));
        g.drawHorizontalLine(294, 370.0f, 826.0f);

        // Glowing Red LED Dot + PROGRAMS Header
        g.setColour(juce::Colour::fromRGB(229, 57, 53).withAlpha(0.40f));
        g.fillEllipse(374.0f, 304.0f, 10.0f, 10.0f);
        g.setColour(juce::Colour::fromRGB(255, 60, 65));
        g.fillEllipse(376.0f, 305.5f, 7.0f, 7.0f);

        g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
        g.setColour(juce::Colour::fromRGB(240, 245, 255));
        g.drawText("PROGRAMS", 390, 302, 120, 16, juce::Justification::left);
    }

    // C. CARD 3: MASTER SECTION (x: 846, y: 112, w: 166, h: 324)
    drawOmniPanelCard(g, {846, 112, 166, 324}, "MASTER", true);
    {
        // Master Subline
        g.setColour(juce::Colour::fromRGB(36, 42, 54));
        g.drawHorizontalLine(140, 856.0f, 1002.0f);

        // Master Stereo Dual VU Meter
        float mLevel = float(sliderMainVolume.getValue()) / 10.0f;
        drawOmniVuMeter(g, 922.0f, 162.0f, 10.0f, 204.0f, mLevel);
        drawOmniVuMeter(g, 936.0f, 162.0f, 10.0f, 204.0f, mLevel);

        // 0.0 dB Readout Box
        juce::Rectangle<float> dbBox(858.0f, 386.0f, 60.0f, 18.0f);
        g.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::bold)));
        g.setColour(juce::Colour::fromRGB(190, 202, 220));
        g.drawText("0.0 dB", dbBox, juce::Justification::centred);
    }

    //==========================================================================
    // 4. CARD 4: MIDDLE PARAMETERS PANEL (x: 12, y: 446, w: 1000, h: 128)
    //==========================================================================
    drawOmniPanelCard(g, {12, 446, 1000, 128}, "SOUND / TIMBRE", true);
    {
        // Dividers
        g.setColour(juce::Colour::fromRGB(36, 42, 54));
        g.drawVerticalLine(310, 454.0f, 566.0f);
        g.drawVerticalLine(440, 454.0f, 566.0f);
        g.drawVerticalLine(668, 454.0f, 566.0f);

        // Section Headers
        g.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::bold)));
        g.setColour(juce::Colour::fromRGB(150, 162, 180));
        g.drawText("CHARACTER", 326, 452, 90, 14, juce::Justification::left);
        g.drawText("TONE", 460, 452, 180, 14, juce::Justification::left);
        g.drawText("AMP / FX SEND", 690, 452, 280, 14, juce::Justification::left);

        // Character Switch Labels
        g.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::bold)));
        g.setColour(juce::Colour::fromRGB(220, 230, 245));
        g.drawText("Dynamics", 326, 477, 60, 16, juce::Justification::left);
        g.drawText("Pedal Noise", 326, 503, 60, 16, juce::Justification::left);
        g.drawText("Damper Res", 326, 529, 60, 16, juce::Justification::left);

        // Tone Knob Labels (Cutoff, Reso, Env)
        g.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::bold)));
        g.setColour(juce::Colour::fromRGB(160, 172, 190));
        g.drawText("Cutoff", 456, 542, 54, 14, juce::Justification::centred);
        g.drawText("Reso", 526, 542, 54, 14, juce::Justification::centred);
        g.drawText("Env", 596, 542, 54, 14, juce::Justification::centred);

        // Amp Knob Labels (A, D, R)
        g.drawText("A", 730, 542, 54, 14, juce::Justification::centred);
        g.drawText("D", 810, 542, 54, 14, juce::Justification::centred);
        g.drawText("R", 890, 542, 54, 14, juce::Justification::centred);
    }

    //==========================================================================
    // 5. RACK INFERIOR: 4 CARDS (EQ, COMP, DYNAMICS, EFFECTS) (y: 584, h: 172)
    //==========================================================================
    // CARD 1: EQ (x: 12, w: 228)
    drawOmniPanelCard(g, {12, 584, 228, 172}, "EQ", true);
    {
        g.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::bold)));
        g.setColour(juce::Colour::fromRGB(160, 172, 190));
        g.drawText("Low", 28, 722, 54, 14, juce::Justification::centred);
        g.drawText("Mid", 98, 722, 54, 14, juce::Justification::centred);
        g.drawText("High", 168, 722, 54, 14, juce::Justification::centred);
    }

    // CARD 2: COMP (x: 248, w: 186)
    drawOmniPanelCard(g, {248, 584, 186, 172}, "", false);
    {
        g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
        g.setColour(juce::Colour::fromRGB(240, 245, 255));
        g.drawText("COMP", 286, 592, 120, 16, juce::Justification::left);

        g.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::bold)));
        g.setColour(juce::Colour::fromRGB(160, 172, 190));
        g.drawText("Thresh", 268, 722, 54, 14, juce::Justification::centred);
        g.drawText("Ratio", 348, 722, 54, 14, juce::Justification::centred);
    }

    // CARD 3: DYNAMICS (x: 442, w: 186)
    drawOmniPanelCard(g, {442, 584, 186, 172}, "", false);
    {
        g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
        g.setColour(juce::Colour::fromRGB(240, 245, 255));
        g.drawText("DYNAMICS", 484, 592, 120, 16, juce::Justification::left);

        g.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::bold)));
        g.setColour(juce::Colour::fromRGB(160, 172, 190));
        g.drawText("Attack", 462, 722, 54, 14, juce::Justification::centred);
        g.drawText("Release", 542, 722, 54, 14, juce::Justification::centred);
    }

    // CARD 4: EFFECTS (x: 636, w: 376)
    drawOmniPanelCard(g, {636, 584, 376, 172}, "EFFECTS", true);
    {
        g.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::bold)));
        g.setColour(juce::Colour::fromRGB(160, 172, 190));
        g.drawText("Mix", 906, 722, 62, 14, juce::Justification::centred);
    }
}

//==============================================================================
// Resized Layout: Perfect 1024 x 768 OmniStage Console Architecture
//==============================================================================
void NordPianoPrototypeAudioProcessorEditor::resized() {
    // 1. Top Bar Preset & Controls
    btnQuickPrev.setBounds(240, 18, 20, 20);
    btnQuickSelect.setBounds(264, 16, 360, 24);
    btnQuickNext.setBounds(628, 18, 20, 20);
    btnSettings.setBounds(666, 14, 38, 38);
    sliderTune.setBounds(720, 16, 40, 36);

    // 2. Navigation Tabs (y: 68, h: 34)
    tabPiano.setBounds(12, 68, 150, 34);
    tabPad.setBounds(168, 68, 160, 34);
    tabSynth.setBounds(334, 68, 160, 34);
    tabOrgan.setBounds(500, 68, 150, 34);
    btnSplitLayer.setBounds(834, 68, 178, 34);

    // 3. Mid Section (y: 112 to 436, h: 324)
    // 4-Channel Mixer Strips
    // Channel A: Piano
    sliderVolA.setBounds(20, 174, 52, 194);
    btnOnA.setBounds(24, 396, 52, 26);

    // Channel B: Worship Pad
    sliderVolB.setBounds(102, 174, 52, 194);
    btnOnB.setBounds(106, 396, 52, 26);

    // Channel C: Synth Lead
    sliderVolC.setBounds(184, 174, 52, 194);
    btnOnC.setBounds(188, 396, 52, 26);

    // Channel D: Organ B3
    sliderVolD.setBounds(266, 174, 52, 194);
    btnOnD.setBounds(270, 396, 52, 26);

    // Center Console: STORE, SAVE, PROGRAMS 1..8
    btnStore.setBounds(372, 138, 62, 34);
    btnSave.setBounds(372, 186, 62, 34);

    for (int p = 0; p < 8; ++p) {
        btnPrograms[size_t(p)].setBounds(372 + p * 56, 328, 50, 42);
    }

    // Master Fader
    sliderMainVolume.setBounds(860, 160, 52, 208);

    // 4. Middle Parameters Panel (y: 446 to 574, h: 128)
    // SOUND / TIMBRE: 8 Category Buttons
    // Row 1 (y: 476)
    btnCatGrand.setBounds(22, 476, 64, 30);
    btnCatEPiano.setBounds(90, 476, 64, 30);
    btnCatClav.setBounds(158, 476, 64, 30);
    btnCatOrgan.setBounds(226, 476, 64, 30);
    // Row 2 (y: 512)
    btnCatPad.setBounds(22, 512, 64, 30);
    btnCatSynth.setBounds(90, 512, 64, 30);
    btnCatStrings.setBounds(158, 512, 64, 30);
    btnCatMisc.setBounds(226, 512, 64, 30);

    // CHARACTER: 3 Slide Toggle Switches
    swDynamics.setBounds(388, 474, 38, 20);
    swPedalNoise.setBounds(388, 500, 38, 20);
    swDamperRes.setBounds(388, 526, 38, 20);

    // TONE: 3 Knobs
    sliderCutoff.setBounds(456, 476, 54, 54);
    sliderReso.setBounds(526, 476, 54, 54);
    sliderEnv.setBounds(596, 476, 54, 54);

    // AMP / FX SEND: 3 Knobs
    sliderAmpAttack.setBounds(730, 476, 54, 54);
    sliderAmpDecay.setBounds(810, 476, 54, 54);
    sliderAmpRelease.setBounds(890, 476, 54, 54);

    // 5. Rack Inferior (y: 584, h: 172)
    // Card 1: EQ
    sliderBass.setBounds(28, 636, 54, 54);
    sliderMid.setBounds(98, 636, 54, 54);
    sliderTreble.setBounds(168, 636, 54, 54);

    // Card 2: COMP
    btnCompressorOn.setButtonText("");
    btnCompressorOn.setToggleState(true, juce::dontSendNotification);
    btnCompressorOn.setBounds(258, 594, 22, 13);
    sliderCompThresh.setBounds(268, 636, 54, 54);
    sliderCompRatio.setBounds(348, 636, 54, 54);

    // Card 3: DYNAMICS
    btnTouch1.setButtonText("");
    btnTouch1.setToggleState(true, juce::dontSendNotification);
    btnTouch1.setBounds(452, 594, 22, 13);
    sliderDynAttack.setBounds(462, 636, 54, 54);
    sliderDynRelease.setBounds(542, 636, 54, 54);

    // Card 4: EFFECTS
    btnFxChorus.setBounds(654, 638, 66, 34);
    btnFxDelay.setBounds(726, 638, 66, 34);
    btnFxReverb.setBounds(798, 638, 66, 34);
    sliderFxMix.setBounds(906, 626, 62, 62);

    // Keyboard disabled for pure stage console
    if (pianoKeyboard != nullptr) {
        pianoKeyboard->setVisible(false);
    }

    if (midiMapOverlay != nullptr) {
        midiMapOverlay->setBounds(getLocalBounds());
    }
}



void NordPianoPrototypeAudioProcessorEditor::paintOverChildren(juce::Graphics&) {}

void NordPianoPrototypeAudioProcessorEditor::mouseDown(const juce::MouseEvent& e) {
    juce::AudioProcessorEditor::mouseDown(e);
}

void MidiMapOverlay::buildTargets() {
    targets.clear();
    for (int i = 0; i < 8; ++i) {
        targets.push_back({ &editor.btnPrograms[size_t(i)], "prog_" + juce::String(i + 1), "Prog " + juce::String(i + 1) });
    }
    targets.push_back({ &editor.btnStore, "prog_store", "STORE" });
    targets.push_back({ &editor.btnSave, "prog_save", "SAVE" });
    targets.push_back({ &editor.sliderVolA, "vol_a", "Vol A" });
    targets.push_back({ &editor.sliderVolB, "vol_b", "Vol B" });
    targets.push_back({ &editor.sliderVolC, "vol_c", "Vol C" });
    targets.push_back({ &editor.sliderVolD, "vol_d", "Vol D" });
    targets.push_back({ &editor.sliderMainVolume, "main_volume", "Master Vol" });
    targets.push_back({ &editor.sliderCutoff, "cutoff", "Cutoff" });
    targets.push_back({ &editor.sliderReso, "resonance", "Reso" });
    targets.push_back({ &editor.sliderEnv, "env", "Env" });
    targets.push_back({ &editor.sliderAmpAttack, "amp_attack", "Attack" });
    targets.push_back({ &editor.sliderAmpDecay, "amp_decay", "Decay" });
    targets.push_back({ &editor.sliderAmpRelease, "amp_release", "Release" });
    targets.push_back({ &editor.sliderBass, "eq_bass", "Bass" });
    targets.push_back({ &editor.sliderMid, "eq_mid", "Mid" });
    targets.push_back({ &editor.sliderTreble, "eq_treble", "Treble" });
    targets.push_back({ &editor.sliderFxMix, "fx_mix", "FX Mix" });
}

void MidiMapOverlay::paint(juce::Graphics& g) {
    if (!processor.isMidiMapModeActive()) return;
    g.fillAll(juce::Colour::fromRGBA(14, 20, 32, 140));

    auto topBanner = getLocalBounds().removeFromTop(24);
    g.setColour(juce::Colour::fromRGB(15, 25, 42));
    g.fillRect(topBanner);
    g.setColour(juce::Colour::fromRGB(255, 190, 40));
    g.setFont(juce::Font(juce::FontOptions(11.5f, juce::Font::bold)));
    g.drawText("MODO MAPEAMENTO MIDI ATIVO: Clique no controle que deseja mapear e mexa no controlador",
               topBanner, juce::Justification::centred);

    const juce::String selectedParam = processor.getSelectedMidiLearnParam();

    for (const auto& t : targets) {
        if (!t.comp || !t.comp->isVisible()) continue;
        auto b = getLocalArea(t.comp, t.comp->getLocalBounds()).toFloat();
        if (b.isEmpty()) continue;

        const bool isSelected = (t.paramId == selectedParam);
        const juce::String badge = processor.getMappingBadgeForParam(t.paramId);

        if (isSelected) {
            g.setColour(juce::Colour::fromRGB(255, 200, 30).withAlpha(0.40f));
            g.fillRoundedRectangle(b, 4.0f);
            g.setColour(juce::Colour::fromRGB(255, 220, 50));
            g.drawRoundedRectangle(b, 4.0f, 2.5f);
            g.setColour(juce::Colour::fromRGB(255, 255, 255));
            g.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::bold)));
            g.drawText("LEARNING...", b.toNearestInt(), juce::Justification::centred);
        } else if (badge.isNotEmpty()) {
            g.setColour(juce::Colour::fromRGB(24, 80, 150).withAlpha(0.50f));
            g.fillRoundedRectangle(b, 3.0f);
            g.setColour(juce::Colour::fromRGB(100, 190, 255));
            g.drawRoundedRectangle(b, 3.0f, 1.5f);
            auto pillW = juce::jmax(34.0f, float(badge.length() * 7 + 8));
            g.setColour(juce::Colour::fromRGB(8, 14, 24));
            g.fillRoundedRectangle(b.getX() + 1.0f, b.getY() + 1.0f, pillW, 13.0f, 2.0f);
            g.setColour(juce::Colour::fromRGB(110, 200, 255));
            g.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::bold)));
            g.drawText(badge, int(b.getX() + 1.0f), int(b.getY() + 1.0f), int(pillW), 13, juce::Justification::centred);
        } else {
            g.setColour(juce::Colour::fromRGB(35, 50, 75).withAlpha(0.25f));
            g.fillRoundedRectangle(b, 3.0f);
            g.setColour(juce::Colour::fromRGB(70, 110, 160).withAlpha(0.70f));
            g.drawRoundedRectangle(b, 3.0f, 1.0f);
        }
    }
}

void MidiMapOverlay::mouseDown(const juce::MouseEvent& e) {
    auto midiBtnArea = getLocalArea(&editor.btnMidiMap, editor.btnMidiMap.getLocalBounds());
    if (midiBtnArea.contains(e.getPosition())) {
        editor.btnMidiMap.setToggleState(false, juce::sendNotificationSync);
        return;
    }

    for (const auto& t : targets) {
        if (!t.comp || !t.comp->isVisible()) continue;
        auto b = getLocalArea(t.comp, t.comp->getLocalBounds());
        if (b.contains(e.getPosition())) {
            processor.setSelectedMidiLearnParam(t.paramId);
            repaint();
            return;
        }
    }

    processor.setSelectedMidiLearnParam("");
    repaint();
}

