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
    swDamperRes.setToggleState(true, juce::dontSendNotification);

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
    addAndMakeVisible(btnMidiMap);
    addAndMakeVisible(btnMidiCh);
    addAndMakeVisible(btnOutput);
    addAndMakeVisible(btnSettings);

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

    setupRotary(sliderPianoVol, "LEVEL", 0.0, 1.5, processor.getEngine().getLayerAConfig().volume.load());
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

    setupRotary(sliderPadVol, "PAD LEVEL", 0.0, 1.5, processor.getEngine().getLayerBConfig().volume.load());
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
    sliderTremoloDepth.onValueChange = [this]() {
        if (auto* p = processor.getAPVTS().getParameter("tremolo_depth"))
            p->setValueNotifyingHost(p->convertTo0to1(float(sliderTremoloDepth.getValue())));
    };

        addAndMakeVisible(btnTapTempo);
    setupPillButton(btnTapTempo, "TAP", false);
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
    setSize(1024, 736);
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
void NordPianoPrototypeAudioProcessorEditor::drawAllenScrew(juce::Graphics& g, float x, float y) {
    // Outer shadow
    g.setColour(juce::Colour::fromRGB(4, 5, 8).withAlpha(0.7f));
    g.fillEllipse(x - 3.5f, y - 2.5f, 7.5f, 7.5f);

    // Metallic screw head
    juce::ColourGradient screwGrad(juce::Colour::fromRGB(65, 74, 88), x, y - 3.5f,
                                   juce::Colour::fromRGB(24, 28, 36), x, y + 3.5f, false);
    g.setGradientFill(screwGrad);
    g.fillEllipse(x - 3.5f, y - 3.5f, 7.0f, 7.0f);
    g.setColour(juce::Colour::fromRGB(18, 22, 28));
    g.drawEllipse(x - 3.5f, y - 3.5f, 7.0f, 7.0f, 0.8f);

    // Inner hex socket
    juce::Path hex;
    for (int i = 0; i < 6; ++i) {
        float angle = float(i) * juce::MathConstants<float>::pi / 3.0f;
        float px = x + std::cos(angle) * 1.8f;
        float py = y + std::sin(angle) * 1.8f;
        if (i == 0) hex.startNewSubPath(px, py);
        else hex.lineTo(px, py);
    }
    hex.closeSubPath();
    g.setColour(juce::Colour::fromRGB(10, 12, 16));
    g.fillPath(hex);
}

void NordPianoPrototypeAudioProcessorEditor::drawPanelCard(juce::Graphics& g, juce::Rectangle<int> bounds, const juce::String& title, bool hasLed) {
    auto bf = bounds.toFloat();

    // 1. Outer Deep Drop Shadow
    g.setColour(juce::Colour::fromRGB(4, 5, 7).withAlpha(0.85f));
    g.fillRoundedRectangle(bf.translated(0.0f, 2.5f), 5.0f);

    // 2. Metal Panel Base (#111419 to #0B0D11)
    juce::ColourGradient panelGrad(juce::Colour::fromRGB(18, 21, 27), bf.getX(), bf.getY(),
                                  juce::Colour::fromRGB(11, 13, 17), bf.getX(), bf.getBottom(), false);
    g.setGradientFill(panelGrad);
    g.fillRoundedRectangle(bf, 4.0f);

    // 3. Brushed Metal Bevel Rim (#343A43)
    g.setColour(juce::Colour::fromRGB(52, 58, 67));
    g.drawRoundedRectangle(bf, 4.0f, 1.2f);

    // Top Highlight Reflection
    g.setColour(juce::Colour::fromRGB(255, 255, 255).withAlpha(0.05f));
    g.drawLine(bf.getX() + 4.0f, bf.getY() + 1.0f, bf.getRight() - 4.0f, bf.getY() + 1.0f, 1.0f);

    // 4. Panel Header Title & Ice-Blue Status LED
    if (title.isNotEmpty()) {
        const float tx = bf.getX() + 12.0f;
        const float ty = bf.getY() + 10.0f;

        if (hasLed) {
            // Nord Stage Crimson Red LED (#E53935)
            g.setColour(juce::Colour::fromRGB(229, 57, 53).withAlpha(0.40f));
            g.fillEllipse(tx - 1.5f, ty + 0.5f, 10.0f, 10.0f); // Halo glow

            g.setColour(juce::Colour::fromRGB(240, 60, 60));
            g.fillEllipse(tx, ty + 2.0f, 7.0f, 7.0f); // LED Core

            g.setFont(juce::Font(juce::FontOptions(10.5f, juce::Font::bold)));
            g.setColour(juce::Colour::fromRGB(245, 247, 250));
            g.drawText(title, int(tx + 14.0f), int(ty), int(bf.getWidth() - 30.0f), 12, juce::Justification::left);
        } else {
            g.setFont(juce::Font(juce::FontOptions(10.5f, juce::Font::bold)));
            g.setColour(juce::Colour::fromRGB(245, 247, 250));
            g.drawText(title, int(tx), int(ty), int(bf.getWidth() - 20.0f), 12, juce::Justification::left);
        }
    }
}

//==============================================================================
// Main Paint Function (1024 x 682 Stage Interface)
//==============================================================================
void NordPianoPrototypeAudioProcessorEditor::paint(juce::Graphics& g) {
    // 1. Deep Stealth Obsidian Black Canvas (#080A0D)
    g.fillAll(juce::Colour::fromRGB(8, 10, 13));

    //==========================================================================
    // 2. TOP BAR (x: 16, y: 12, w: 992, h: 42)
    //==========================================================================
    // Left: OmniStage Logo with Red Waveform Circle Icon
    const float logoX = 26.0f;
    const float logoY = 16.0f;
    g.setColour(juce::Colour::fromRGB(229, 57, 53)); // Neon Red
    g.drawEllipse(logoX, logoY, 32.0f, 32.0f, 1.8f);
    // Waveform bars inside logo circle
    g.fillRect(logoX + 7.0f, logoY + 11.0f, 2.4f, 10.0f);
    g.fillRect(logoX + 11.5f, logoY + 7.0f, 2.4f, 18.0f);
    g.fillRect(logoX + 16.0f, logoY + 5.0f, 2.4f, 22.0f);
    g.fillRect(logoX + 20.5f, logoY + 9.0f, 2.4f, 14.0f);
    g.fillRect(logoX + 25.0f, logoY + 13.0f, 2.4f, 6.0f);

    g.setFont(juce::Font(juce::FontOptions(22.0f, juce::Font::bold)));
    g.setColour(juce::Colour::fromRGB(255, 255, 255));
    g.drawText("Omni", logoX + 38.0f, logoY + 4.0f, 54.0f, 24.0f, juce::Justification::left);

    g.setColour(juce::Colour::fromRGB(229, 57, 53)); // Neon Red
    g.drawText("Stage", logoX + 92.0f, logoY + 4.0f, 64.0f, 24.0f, juce::Justification::left);

    // Center Stage Capsule: Multi-Preset Browser (x: 340, y: 12, w: 320, h: 38)
    juce::Rectangle<float> presetCapsule(340.0f, 12.0f, 320.0f, 38.0f);
    g.setColour(juce::Colour::fromRGB(14, 18, 25));
    g.fillRoundedRectangle(presetCapsule, 5.0f);
    g.setColour(juce::Colour::fromRGB(38, 46, 60));
    g.drawRoundedRectangle(presetCapsule, 5.0f, 1.0f);

    g.setFont(juce::Font(juce::FontOptions(13.5f, juce::Font::bold)));
    g.setColour(juce::Colour::fromRGB(245, 248, 255));
    g.drawText("01: Royal Worship Grand", 375, 12, 250, 38, juce::Justification::centred);

    // Top Bar Right: Master Tune & Delay BPM
    // Master Tune (440Hz)
    g.setFont(juce::Font(juce::FontOptions(8.5f, juce::Font::plain)));
    g.setColour(juce::Colour::fromRGB(140, 152, 170));
    g.drawText("Master Tune", 710, 14, 70, 12, juce::Justification::centred);
    g.setFont(juce::Font(juce::FontOptions(10.5f, juce::Font::bold)));
    g.setColour(juce::Colour::fromRGB(220, 230, 245));
    g.drawText("440Hz", 710, 28, 70, 14, juce::Justification::centred);

    // Delay BPM Box
    juce::Rectangle<float> bpmBox(800.0f, 16.0f, 84.0f, 30.0f);
    g.setColour(juce::Colour::fromRGB(14, 18, 25));
    g.fillRoundedRectangle(bpmBox, 4.0f);
    g.setColour(juce::Colour::fromRGB(45, 54, 70));
    g.drawRoundedRectangle(bpmBox, 4.0f, 1.0f);
    g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
    g.setColour(juce::Colour::fromRGB(240, 245, 255));
    g.drawText("BPM: 72", bpmBox, juce::Justification::centred);

    //==========================================================================
    // 3. UPPER CONSOLE DECK (Mixer, OLED Screen & Programs, Master)
    //==========================================================================
    // Panel 1: 4-Channel Live Stage Mixer (x: 16, y: 62, w: 390, h: 282)
    drawPanelCard(g, {16, 62, 390, 282}, "");
    drawAllenScrew(g, 26.0f, 72.0f);
    drawAllenScrew(g, 396.0f, 72.0f);
    drawAllenScrew(g, 26.0f, 334.0f);
    drawAllenScrew(g, 396.0f, 334.0f);

    const std::array<juce::String, 4> chTitles = {"Channel 1", "Channel 2", "Channel 3", "Channel 4"};
    const std::array<juce::String, 4> chSubtitles = {"Piano", "Pad", "Synth", "Organ"};
    const std::array<juce::Colour, 4> chColors = {
        juce::Colour::fromRGB(229, 57, 53),  // Red
        juce::Colour::fromRGB(0, 220, 240),  // Cyan
        juce::Colour::fromRGB(50, 130, 255), // Blue
        juce::Colour::fromRGB(255, 160, 30)  // Amber
    };

    for (int ch = 0; ch < 4; ++ch) {
        const int cx = 28 + ch * 92;

        // Channel Header (w: 84, h: 36)
        g.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::bold)));
        g.setColour(juce::Colour::fromRGB(150, 160, 175));
        g.drawText(chTitles[size_t(ch)], cx, 74, 84, 12, juce::Justification::centred);

        g.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
        g.setColour(chColors[size_t(ch)]);
        g.drawText(chSubtitles[size_t(ch)], cx, 88, 84, 16, juce::Justification::centred);

        // Vertical Brushed Aluminum Fader Plate
        juce::Rectangle<float> faderPlate(float(cx + 4), 112.0f, 40.0f, 154.0f);
        juce::ColourGradient plateGrad(juce::Colour::fromRGB(32, 38, 48), faderPlate.getX(), faderPlate.getY(),
                                       juce::Colour::fromRGB(16, 20, 26), faderPlate.getX(), faderPlate.getBottom(), false);
        g.setGradientFill(plateGrad);
        g.fillRoundedRectangle(faderPlate, 3.0f);
        g.setColour(juce::Colour::fromRGB(46, 54, 68));
        g.drawRoundedRectangle(faderPlate, 3.0f, 1.0f);

        // Vertical LED Ladder Peak Meter next to fader plate
        const float meterX = float(cx + 50);
        const float meterY = 114.0f;
        const float meterH = 150.0f;
        const int numSegments = 24;
        for (int s = 0; s < numSegments; ++s) {
            const float sy = meterY + meterH - (s + 1) * 6.2f;
            juce::Colour segCol = (s < 16) ? juce::Colour::fromRGB(45, 215, 45) : (s < 20 ? juce::Colour::fromRGB(245, 205, 30) : juce::Colour::fromRGB(235, 45, 45));
            const bool lit = (s < (ch == 0 ? 17 : (ch == 1 ? 12 : 0)));
            g.setColour(lit ? segCol : segCol.withAlpha(0.15f));
            g.fillRect(meterX, sy, 7.0f, 5.0f);
        }

        // Channel ON Button (drawn button background)
        juce::Rectangle<float> onRect(float(cx + 8), 282.0f, 32.0f, 22.0f);
        g.setColour(ch == 0 ? juce::Colour::fromRGB(229, 57, 53) : juce::Colour::fromRGB(24, 28, 36));
        g.fillRoundedRectangle(onRect, 3.0f);
        g.setColour(ch == 0 ? juce::Colour::fromRGB(255, 80, 75) : juce::Colour::fromRGB(45, 52, 65));
        g.drawRoundedRectangle(onRect, 3.0f, 1.0f);
        g.setFont(juce::Font(juce::FontOptions(8.5f, juce::Font::bold)));
        g.setColour(ch == 0 ? juce::Colour::fromRGB(255, 255, 255) : juce::Colour::fromRGB(130, 140, 155));
        g.drawText("ON", onRect, juce::Justification::centred);
    }

    // Panel 2: Central OLED Display & Programs (x: 418, y: 62, w: 400, h: 282)
    drawPanelCard(g, {418, 62, 400, 282}, "");
    drawAllenScrew(g, 428.0f, 72.0f);
    drawAllenScrew(g, 808.0f, 72.0f);
    drawAllenScrew(g, 428.0f, 334.0f);
    drawAllenScrew(g, 808.0f, 334.0f);

    // OLED Screen Glass (x: 426, y: 72, w: 384, h: 168)
    juce::Rectangle<float> oledRect(426.0f, 72.0f, 384.0f, 168.0f);
    juce::ColourGradient oledGrad(juce::Colour::fromRGB(6, 10, 15), oledRect.getX(), oledRect.getY(),
                                  juce::Colour::fromRGB(2, 4, 7), oledRect.getX(), oledRect.getBottom(), false);
    g.setGradientFill(oledGrad);
    g.fillRoundedRectangle(oledRect, 6.0f);
    g.setColour(juce::Colour::fromRGB(0, 215, 255).withAlpha(0.7f)); // Bright Cyan neon glow frame
    g.drawRoundedRectangle(oledRect, 6.0f, 1.2f);

    // OLED Preset Title
    g.setFont(juce::Font(juce::FontOptions(14.0f, juce::Font::bold)));
    g.setColour(juce::Colour::fromRGB(0, 230, 255));
    g.drawText("01: Royal Worship Grand", 440, 78, 250, 18, juce::Justification::left);

    // Thin cyan divider line
    g.setColour(juce::Colour::fromRGB(0, 180, 220).withAlpha(0.35f));
    g.drawHorizontalLine(98, 440.0f, 620.0f);

    // OLED Layer Icons & Mix Bars
    g.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::bold)));
    g.setColour(juce::Colour::fromRGB(0, 215, 240));
    g.drawText("Sound Layer", 440, 134, 80, 12, juce::Justification::left);
    g.fillRect(440.0f, 150.0f, 59.0f, 6.0f);
    g.drawText("59%", 504, 146, 30, 12, juce::Justification::left);

    g.drawText("Sound Layer Mix", 440, 168, 90, 12, juce::Justification::left);
    g.fillRect(440.0f, 184.0f, 57.0f, 6.0f);
    g.drawText("57%", 504, 180, 30, 12, juce::Justification::left);

    // Photorealistic 3D Grand Piano Graphic on the right side of OLED
    if (imgOledGrandPiano.isValid()) {
        g.drawImageWithin(imgOledGrandPiano, 610, 78, 194, 148,
                          juce::RectanglePlacement::centred | juce::RectanglePlacement::onlyReduceInSize);
    }

    // Below OLED Screen: Red LED Dot + PROGRAMS Label (y: 250)
    g.setColour(juce::Colour::fromRGB(229, 57, 53));
    g.fillEllipse(436.0f, 253.0f, 6.0f, 6.0f); // LED Dot
    g.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::bold)));
    g.setColour(juce::Colour::fromRGB(220, 225, 235));
    g.drawText("PROGRAMS", 448, 249, 80, 14, juce::Justification::left);

    // Panel 3: Master Output Section (x: 830, y: 62, w: 178, h: 282)
    drawPanelCard(g, {830, 62, 178, 282}, "MASTER", false);
    drawAllenScrew(g, 840.0f, 72.0f);
    drawAllenScrew(g, 998.0f, 72.0f);
    drawAllenScrew(g, 840.0f, 334.0f);
    drawAllenScrew(g, 998.0f, 334.0f);

    // Dual 24-Segment Stereo Peak Meters (L and R)
    const float masterMeterX = 924.0f;
    const float masterMeterY = 112.0f;
    const float masterMeterH = 170.0f;
    const int numMasterSegs = 24;
    for (int ms = 0; ms < numMasterSegs; ++ms) {
        const float my = masterMeterY + masterMeterH - (ms + 1) * 7.0f;
        juce::Colour mCol = (ms < 16) ? juce::Colour::fromRGB(50, 215, 50) : (ms < 20 ? juce::Colour::fromRGB(245, 205, 30) : juce::Colour::fromRGB(235, 45, 45));
        const bool mLit = (ms < 17);
        g.setColour(mLit ? mCol : mCol.withAlpha(0.15f));
        g.fillRect(masterMeterX, my, 8.0f, 5.5f);         // Left
        g.fillRect(masterMeterX + 11.0f, my, 8.0f, 5.5f); // Right
    }

    // Master Meter Scale Numbers
    g.setFont(juce::Font(juce::FontOptions(7.5f, juce::Font::plain)));
    g.setColour(juce::Colour::fromRGB(130, 140, 155));
    g.drawText("0", int(masterMeterX + 23), int(masterMeterY - 2), 14, 8, juce::Justification::left);
    g.drawText("-4", int(masterMeterX + 23), int(masterMeterY + 32), 14, 8, juce::Justification::left);
    g.drawText("-8", int(masterMeterX + 23), int(masterMeterY + 60), 14, 8, juce::Justification::left);
    g.drawText("-16", int(masterMeterX + 23), int(masterMeterY + 98), 16, 8, juce::Justification::left);
    g.drawText("-26", int(masterMeterX + 23), int(masterMeterY + 130), 16, 8, juce::Justification::left);
    g.drawText("-40", int(masterMeterX + 23), int(masterMeterY + 160), 16, 8, juce::Justification::left);
    g.setFont(juce::Font(juce::FontOptions(8.5f, juce::Font::bold)));
    g.drawText("24 VU", int(masterMeterX - 8), int(masterMeterY + masterMeterH + 4), 54, 12, juce::Justification::centred);

    //==========================================================================
    // 4. NAVIGATION TABS ROW (y: 354, h: 36)
    //==========================================================================
    // 5 Large Engine Tabs: A Piano, B Worship Pad, C Synth Lead, D Organ B3, Master FX
    const std::array<juce::String, 5> tabLabels = {"A  PIANO", "B  WORSHIP PAD", "C  SYNTH LEAD", "D  ORGAN B3", "MASTER FX"};
    for (int t = 0; t < 5; ++t) {
        const float tx = 16.0f + float(t) * 200.0f;
        const bool isActive = (t == activeTab);

        juce::Rectangle<float> tabRect(tx, 354.0f, 192.0f, 34.0f);
        if (isActive) {
            g.setColour(juce::Colour::fromRGB(22, 26, 34));
            g.fillRoundedRectangle(tabRect, 4.0f);
            // Glowing Red bottom border underline
            g.setColour(juce::Colour::fromRGB(229, 57, 53));
            g.fillRect(tx + 8.0f, 385.0f, 176.0f, 3.0f);
            g.drawRoundedRectangle(tabRect, 4.0f, 1.2f);
        } else {
            g.setColour(juce::Colour::fromRGB(14, 17, 24));
            g.fillRoundedRectangle(tabRect, 4.0f);
            g.setColour(juce::Colour::fromRGB(34, 40, 52));
            g.drawRoundedRectangle(tabRect, 4.0f, 1.0f);
        }

        g.setFont(juce::Font(juce::FontOptions(11.5f, juce::Font::bold)));
        if (t == 0) g.setColour(isActive ? juce::Colour::fromRGB(255, 255, 255) : juce::Colour::fromRGB(229, 57, 53));
        else if (t == 1) g.setColour(juce::Colour::fromRGB(0, 220, 240));
        else if (t == 2) g.setColour(juce::Colour::fromRGB(50, 130, 255));
        else if (t == 3) g.setColour(juce::Colour::fromRGB(255, 160, 30));
        else g.setColour(juce::Colour::fromRGB(180, 190, 205));
        g.drawText(tabLabels[size_t(t)], tabRect, juce::Justification::centred);
    }

    //==========================================================================
    // 5. LOWER CONSOLE DECK (DEDICATED SOUND SCULPTING & FX RACK)
    //==========================================================================
    // Main Lower Card (x: 16, y: 398, w: 992, h: 356)
    drawPanelCard(g, {16, 398, 992, 356}, "");
    drawAllenScrew(g, 26.0f, 408.0f);
    drawAllenScrew(g, 998.0f, 408.0f);
    drawAllenScrew(g, 26.0f, 744.0f);
    drawAllenScrew(g, 998.0f, 744.0f);

    // Section 1: CATEGORY (x: 28 to 178, w: 150)
    g.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::bold)));
    g.setColour(juce::Colour::fromRGB(150, 160, 175));
    g.drawText("CATEGORY", 28, 412, 150, 14, juce::Justification::centred);

    // Divider Line 1
    g.setColour(juce::Colour::fromRGB(32, 38, 50));
    g.drawVerticalLine(190, 414.0f, 738.0f);

    // Section 2: ACOUSTIC CHARACTER (x: 202 to 360, w: 158)
    g.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::bold)));
    g.setColour(juce::Colour::fromRGB(150, 160, 175));
    g.drawText("ACOUSTIC CHARACTER", 202, 412, 158, 14, juce::Justification::left);

    // Character Labels
    g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
    g.setColour(juce::Colour::fromRGB(220, 230, 245));
    g.drawText("Pedal Noise", 260, 444, 100, 18, juce::Justification::left);
    g.drawText("String Res", 260, 508, 100, 18, juce::Justification::left);
    g.drawText("Damper Res", 260, 572, 100, 18, juce::Justification::left);
    g.drawText("Soft Release", 260, 636, 100, 18, juce::Justification::left);

    // Draw Rocker Switches
    auto drawRocker = [&](float rx, float ry, bool on, juce::Colour activeCol) {
        juce::Rectangle<float> slot(rx, ry, 36.0f, 20.0f);
        g.setColour(juce::Colour::fromRGB(14, 17, 24));
        g.fillRoundedRectangle(slot, 10.0f);
        g.setColour(juce::Colour::fromRGB(45, 54, 70));
        g.drawRoundedRectangle(slot, 10.0f, 1.0f);
        if (on) {
            g.setColour(activeCol);
            g.fillEllipse(rx + 18.0f, ry + 3.0f, 14.0f, 14.0f);
            g.setColour(juce::Colour::fromRGB(255, 255, 255));
            g.fillEllipse(rx + 23.0f, ry + 7.0f, 4.0f, 4.0f);
        } else {
            g.setColour(juce::Colour::fromRGB(70, 80, 96));
            g.fillEllipse(rx + 4.0f, ry + 3.0f, 14.0f, 14.0f);
        }
    };
    drawRocker(214.0f, 444.0f, true, juce::Colour::fromRGB(229, 57, 53));
    drawRocker(214.0f, 508.0f, true, juce::Colour::fromRGB(245, 160, 30));
    drawRocker(214.0f, 572.0f, true, juce::Colour::fromRGB(245, 160, 30));
    drawRocker(214.0f, 636.0f, false, juce::Colour::fromRGB(70, 80, 96));

    // Divider Line 2
    g.drawVerticalLine(374, 414.0f, 738.0f);

    // Section 3: TONE & FILTER (x: 386 to 516, w: 130)
    g.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::bold)));
    g.setColour(juce::Colour::fromRGB(150, 160, 175));
    g.drawText("TONE & FILTER", 386, 412, 130, 14, juce::Justification::centred);

    // Tone Labels
    g.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::bold)));
    g.setColour(juce::Colour::fromRGB(180, 190, 205));
    g.drawText("Cutoff", 420, 506, 60, 12, juce::Justification::centred);
    g.drawText("Reso", 474, 600, 60, 12, juce::Justification::centred);
    g.drawText("Env", 420, 694, 60, 12, juce::Justification::centred);

    // Divider Line 3
    g.drawVerticalLine(530, 414.0f, 738.0f);

    // Section 4: AMP ENVELOPE (x: 542 to 642, w: 100)
    g.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::bold)));
    g.setColour(juce::Colour::fromRGB(150, 160, 175));
    g.drawText("AMP ENVELOPE", 542, 412, 100, 14, juce::Justification::centred);

    // Amp Labels
    g.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::bold)));
    g.setColour(juce::Colour::fromRGB(180, 190, 205));
    g.drawText("Attack", 562, 506, 60, 12, juce::Justification::centred);
    g.drawText("Decay", 562, 600, 60, 12, juce::Justification::centred);
    g.drawText("Release", 562, 694, 60, 12, juce::Justification::centred);

    // Divider Line 4
    g.drawVerticalLine(654, 414.0f, 738.0f);

    // Section 5: MASTER FX CHAIN (x: 664 to 994, w: 330)
    g.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::bold)));
    g.setColour(juce::Colour::fromRGB(150, 160, 175));
    g.drawText("MASTER FX CHAIN", 664, 412, 330, 14, juce::Justification::centred);

    // 4 Brushed Metal FX Faceplate Modules:
    auto drawFxModule = [&](float mx, float my, float mw, float mh, const juce::String& title) {
        juce::Rectangle<float> modRect(mx, my, mw, mh);
        juce::ColourGradient modGrad(juce::Colour::fromRGB(195, 205, 220), mx, my,
                                     juce::Colour::fromRGB(130, 140, 155), mx, my + mh, false);
        g.setGradientFill(modGrad);
        g.fillRoundedRectangle(modRect, 4.0f);
        g.setColour(juce::Colour::fromRGB(240, 248, 255).withAlpha(0.6f));
        g.drawHorizontalLine((int)my, mx + 1.0f, mx + mw - 1.0f);
        g.setColour(juce::Colour::fromRGB(45, 52, 65));
        g.drawRoundedRectangle(modRect, 4.0f, 1.0f);

        // Header Title in Module
        g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
        g.setColour(juce::Colour::fromRGB(18, 22, 28));
        g.drawText(title, int(mx), int(my + 4), int(mw), 14, juce::Justification::centred);
    };

    // 1. EQ Module (x: 664, y: 432, w: 154, h: 144)
    drawFxModule(664.0f, 432.0f, 154.0f, 144.0f, "EQ");
    g.setFont(juce::Font(juce::FontOptions(8.5f, juce::Font::bold)));
    g.setColour(juce::Colour::fromRGB(28, 34, 44));
    g.drawText("Low", 670, 552, 44, 12, juce::Justification::centred);
    g.drawText("Mid", 718, 552, 44, 12, juce::Justification::centred);
    g.drawText("High", 766, 552, 44, 12, juce::Justification::centred);

    // 2. DYNAMICS Module (x: 832, y: 432, w: 154, h: 144)
    drawFxModule(832.0f, 432.0f, 154.0f, 144.0f, "DYNAMICS");
    g.setFont(juce::Font(juce::FontOptions(8.5f, juce::Font::bold)));
    g.setColour(juce::Colour::fromRGB(28, 34, 44));
    g.drawText("Comp", 846, 552, 54, 12, juce::Justification::centred);
    g.drawText("Ratio", 916, 552, 54, 12, juce::Justification::centred);

    // 3. DELAY Module (with BPM sync!) (x: 664, y: 590, w: 154, h: 144)
    drawFxModule(664.0f, 590.0f, 154.0f, 144.0f, "DELAY");
    g.setFont(juce::Font(juce::FontOptions(8.5f, juce::Font::bold)));
    g.setColour(juce::Colour::fromRGB(28, 34, 44));
    g.drawText("Sync", 670, 710, 44, 12, juce::Justification::centred);
    g.drawText("Feedback", 714, 710, 52, 12, juce::Justification::centred);
    g.drawText("Mix", 770, 710, 44, 12, juce::Justification::centred);

    // 4. REVERB Module (x: 832, y: 590, w: 154, h: 144)
    drawFxModule(832.0f, 590.0f, 154.0f, 144.0f, "REVERB");
    g.setFont(juce::Font(juce::FontOptions(8.5f, juce::Font::bold)));
    g.setColour(juce::Colour::fromRGB(28, 34, 44));
    g.drawText("Size", 840, 710, 44, 12, juce::Justification::centred);
    g.drawText("Tone", 886, 710, 44, 12, juce::Justification::centred);
    g.drawText("Mix", 934, 710, 44, 12, juce::Justification::centred);
}


//==============================================================================
// Ableton-Style Full GUI Interactive MIDI Map Overlay Implementation
//==============================================================================
void NordPianoPrototypeAudioProcessorEditor::paintOverChildren(juce::Graphics&) {}

void NordPianoPrototypeAudioProcessorEditor::mouseDown(const juce::MouseEvent& e) {
    juce::AudioProcessorEditor::mouseDown(e);
}

void MidiMapOverlay::buildTargets() {
    targets.clear();
    // Programs 1..8
    for (int i = 0; i < 8; ++i) {
        targets.push_back({ &editor.btnPrograms[size_t(i)], "prog_" + juce::String(i + 1), "Prog " + juce::String(i + 1) });
    }
    // Store & Save
    targets.push_back({ &editor.btnStore, "prog_store", "STORE" });
    targets.push_back({ &editor.btnSave, "prog_save", "SAVE" });

    // Sound Navigation
    targets.push_back({ &editor.btnPianoPrev, "piano_prev", "< Piano" });
    targets.push_back({ &editor.btnPianoNext, "piano_next", "> Piano" });
    targets.push_back({ &editor.btnQuickPrev, "piano_prev", "< Quick" });
    targets.push_back({ &editor.btnQuickNext, "piano_next", "> Quick" });
    targets.push_back({ &editor.btnPadPrev, "pad_prev", "< Pad" });
    targets.push_back({ &editor.btnPadNext, "pad_next", "> Pad" });

    // EQ
    targets.push_back({ &editor.btnEqPiano, "eq_select_piano", "EQ Piano" });
    targets.push_back({ &editor.btnEqPad, "eq_select_pad", "EQ Pad" });
    targets.push_back({ &editor.sliderTreble, editor.eqTargetIsPiano ? "eq_treble" : "pad_eq_treble", editor.eqTargetIsPiano ? "Treble (Pno)" : "Treble (Pad)" });
    targets.push_back({ &editor.sliderMid, editor.eqTargetIsPiano ? "eq_mid" : "pad_eq_mid", editor.eqTargetIsPiano ? "Mid (Pno)" : "Mid (Pad)" });
    targets.push_back({ &editor.sliderBass, editor.eqTargetIsPiano ? "eq_bass" : "pad_eq_bass", editor.eqTargetIsPiano ? "Bass (Pno)" : "Bass (Pad)" });

    // Touch Curves
    targets.push_back({ &editor.btnTouch1, "touch_1", "Touch 1" });
    targets.push_back({ &editor.btnTouch2, "touch_2", "Touch 2" });
    targets.push_back({ &editor.btnTouch3, "touch_3", "Touch 3" });

    // Piano Module
    targets.push_back({ &editor.btnPianoOn, "piano_on", "Piano On" });
    targets.push_back({ &editor.sliderPianoSelect, "piano_select", "Model" });
    targets.push_back({ &editor.sliderTone, "tone", "Tone" });
    targets.push_back({ &editor.sliderDynamics, "dynamics", "Dynamics" });
    targets.push_back({ &editor.sliderResonanceKnob, "piano_res_amount", "Resonance" });
    targets.push_back({ &editor.sliderPianoRelease, "piano_release", "Release" });
    targets.push_back({ &editor.sliderPianoVol, "piano_vol", "Piano Level" });

    // Acoustic Realism Buttons
    targets.push_back({ &editor.btnCatGrand, "piano_cat_grand", "Grand" });
    targets.push_back({ &editor.btnCatUpright, "piano_cat_upright", "Upright" });
    targets.push_back({ &editor.btnStringRes, "piano_res", "String Res" });
    targets.push_back({ &editor.btnPedalNoise, "piano_pedal", "Pedal Noise" });
    targets.push_back({ &editor.btnSoftRelease, "piano_soft_rel", "Soft Release" });

    // Worship Pad Layer (Layer B)
    targets.push_back({ &editor.btnPadOn, "pad_on", "Pad On" });
    targets.push_back({ &editor.sliderPadVol, "pad_vol", "Pad Level" });
    targets.push_back({ &editor.sliderPadCutoff, "pad_cutoff", "Pad Cutoff" });
    targets.push_back({ &editor.sliderPadRes, "pad_resonance", "Pad Res" });
    targets.push_back({ &editor.sliderPadAttack, "pad_attack", "Pad Attack" });
    targets.push_back({ &editor.sliderPadRelease, "pad_release", "Pad Release" });

    // Master Effects
    targets.push_back({ &editor.sliderCompressor, "comp_amount", "Compressor" });
    targets.push_back({ &editor.btnCompressorOn, "comp_on", "Comp On" });
    targets.push_back({ &editor.sliderDelayTime, "delay_time", "Delay Time" });
    targets.push_back({ &editor.sliderDelayFeedback, "delay_feedback", "Delay Fdbk" });
    targets.push_back({ &editor.btnDelayOn, "delay_on", "Delay On" });

    // Reverb
    targets.push_back({ &editor.sliderReverbAmount, "reverb_amount", "Reverb Amt" });
    targets.push_back({ &editor.sliderReverbTone, "reverb_tone", "Reverb Tone" });
    targets.push_back({ &editor.btnReverbType, "reverb_type", "Reverb Type" });
    targets.push_back({ &editor.btnReverbVar, "reverb_variation", "Reverb Var" });
    targets.push_back({ &editor.btnReverbOn, "reverb_on", "Reverb On" });

    // Modulation & Volume
    targets.push_back({ &editor.sliderChorusDepth, "chorus_depth", "Chorus" });
    targets.push_back({ &editor.sliderTremoloDepth, "tremolo_depth", "Tremolo" });
    targets.push_back({ &editor.sliderMainVolume, "main_volume", "Master Vol" });
}

void MidiMapOverlay::paint(juce::Graphics& g) {
    if (!processor.isMidiMapModeActive()) return;

    // Ableton-style subtle dark purple/blue wash
    g.fillAll(juce::Colour::fromRGBA(14, 20, 32, 140));

    // Top Header Banner
    auto topBanner = getLocalBounds().removeFromTop(24);
    g.setColour(juce::Colour::fromRGB(15, 25, 42));
    g.fillRect(topBanner);
    g.setColour(juce::Colour::fromRGB(255, 190, 40));
    g.setFont(juce::Font(juce::FontOptions(11.5f, juce::Font::bold)));
    g.drawText("MODO MAPEAMENTO MIDI ATIVO: Clique no controle que deseja mapear e gire o knob / fader / bata no pad MIDI",
               topBanner, juce::Justification::centred);

    const juce::String selectedParam = processor.getSelectedMidiLearnParam();

    for (const auto& t : targets) {
        if (!t.comp || !t.comp->isVisible()) continue;
        auto b = getLocalArea(t.comp, t.comp->getLocalBounds()).toFloat();
        if (b.isEmpty()) continue;

        const bool isSelected = (t.paramId == selectedParam);
        const juce::String badge = processor.getMappingBadgeForParam(t.paramId);

        if (isSelected) {
            // High-contrast glowing amber learning box
            g.setColour(juce::Colour::fromRGB(255, 200, 30).withAlpha(0.40f));
            g.fillRoundedRectangle(b, 4.0f);
            g.setColour(juce::Colour::fromRGB(255, 220, 50));
            g.drawRoundedRectangle(b, 4.0f, 2.5f);

            // "LEARNING..." text in the center
            g.setColour(juce::Colour::fromRGB(255, 255, 255));
            g.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::bold)));
            g.drawText("LEARNING...", b.toNearestInt(), juce::Justification::centred);
        } else if (badge.isNotEmpty()) {
            // Ableton-style vibrant blue mapped box
            g.setColour(juce::Colour::fromRGB(24, 80, 150).withAlpha(0.50f));
            g.fillRoundedRectangle(b, 3.0f);
            g.setColour(juce::Colour::fromRGB(100, 190, 255));
            g.drawRoundedRectangle(b, 3.0f, 1.5f);

            // Badge pill at top left
            auto pillW = juce::jmax(34.0f, float(badge.length() * 7 + 8));
            g.setColour(juce::Colour::fromRGB(8, 14, 24));
            g.fillRoundedRectangle(b.getX() + 1.0f, b.getY() + 1.0f, pillW, 13.0f, 2.0f);
            g.setColour(juce::Colour::fromRGB(110, 200, 255));
            g.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::bold)));
            g.drawText(badge, int(b.getX() + 1.0f), int(b.getY() + 1.0f), int(pillW), 13, juce::Justification::centred);
        } else {
            // Unmapped mappable control (subtle blue-gray frame)
            g.setColour(juce::Colour::fromRGB(35, 50, 75).withAlpha(0.25f));
            g.fillRoundedRectangle(b, 3.0f);
            g.setColour(juce::Colour::fromRGB(70, 110, 160).withAlpha(0.70f));
            g.drawRoundedRectangle(b, 3.0f, 1.0f);
        }
    }
}

void MidiMapOverlay::mouseDown(const juce::MouseEvent& e) {
    // If click is on btnMidiMap, let editor toggle off
    auto midiBtnArea = getLocalArea(&editor.btnMidiMap, editor.btnMidiMap.getLocalBounds());
    if (midiBtnArea.contains(e.getPosition())) {
        editor.btnMidiMap.setToggleState(false, juce::sendNotificationSync);
        return;
    }

    for (const auto& t : targets) {
        if (!t.comp || !t.comp->isVisible()) continue;
        auto b = getLocalArea(t.comp, t.comp->getLocalBounds());
        if (b.contains(e.getPosition())) {
            if (e.mods.isRightButtonDown()) {
                juce::PopupMenu m;
                const juce::String badge = processor.getMappingBadgeForParam(t.paramId);
                if (badge.isNotEmpty()) {
                    m.addItem(1, "Desmapear " + badge);
                }
                m.addItem(2, "Aprender Controlador MIDI (Learn)");
                m.addSeparator();
                m.addItem(3, "Limpar Todos os Mapeamentos");

                auto safeThis = juce::Component::SafePointer<MidiMapOverlay>(this);
                const juce::String pId = t.paramId;
                m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(t.comp), [safeThis, pId](int res) {
                    if (!safeThis) return;
                    if (res == 1) {
                        safeThis->processor.unmapParameter(pId);
                        safeThis->processor.savePresetsToDisk();
                        safeThis->repaint();
                    } else if (res == 2) {
                        safeThis->processor.setSelectedMidiLearnParam(pId);
                        safeThis->repaint();
                    } else if (res == 3) {
                        safeThis->processor.clearAllMidiMappings();
                        safeThis->processor.savePresetsToDisk();
                        safeThis->repaint();
                    }
                });
            } else {
                processor.setSelectedMidiLearnParam(t.paramId);
                repaint();
            }
            return;
        }
    }

    // Clicked empty space on overlay: deselect
    processor.setSelectedMidiLearnParam("");
    repaint();
}

//==============================================================================
// Resized Layout: Perfect 1024 x 682 Architectural Symmetry
//==============================================================================
void NordPianoPrototypeAudioProcessorEditor::resized() {
    // 1. Top Bar Controls
    btnQuickPrev.setBounds(344, 18, 26, 26);
    btnQuickSelect.setBounds(372, 16, 256, 30);
    btnQuickNext.setBounds(630, 18, 26, 26);
    btnTapTempo.setBounds(890, 16, 48, 30);

    // 2. 4-Channel Live Stage Mixer (x: 16, y: 62)
    for (int ch = 0; ch < 4; ++ch) {
        const int cx = 28 + ch * 92;
        if (ch == 0) {
            sliderVolA.setBounds(cx + 4, 114, 40, 150);
            btnOnA.setBounds(cx + 8, 282, 32, 22);
        } else if (ch == 1) {
            sliderVolB.setBounds(cx + 4, 114, 40, 150);
            btnOnB.setBounds(cx + 8, 282, 32, 22);
        } else if (ch == 2) {
            sliderVolC.setBounds(cx + 4, 114, 40, 150);
            btnOnC.setBounds(cx + 8, 282, 32, 22);
        } else {
            sliderVolD.setBounds(cx + 4, 114, 40, 150);
            btnOnD.setBounds(cx + 8, 282, 32, 22);
        }
    }

    // 8 Program Buttons under OLED (x: 430 to 810, y: 268, w: 42, h: 34)
    for (int p = 0; p < 8; ++p) {
        btnPrograms[size_t(p)].setBounds(432 + p * 46, 268, 42, 34);
    }

    // Master Fader (x: 846, y: 114, w: 42, h: 166)
    sliderMainVolume.setBounds(846, 114, 42, 166);

    // 3. Navigation Tabs (y: 354, h: 34)
    tabPiano.setBounds(16, 354, 192, 34);
    tabPad.setBounds(216, 354, 192, 34);
    tabSynth.setBounds(416, 354, 192, 34);
    tabOrgan.setBounds(616, 354, 192, 34);
    btnSplitLayer.setBounds(816, 354, 192, 34);

    // 4. Lower Console Deck: Dedicated Sound Sculpting (y: 398)
    // Category Buttons (3x2 Grid)
    btnCatGrand.setBounds(30, 436, 68, 30);
    btnCatUpright.setBounds(104, 436, 68, 30);
    btnCatEPiano.setBounds(30, 474, 68, 30);
    btnCatClav.setBounds(104, 474, 68, 30);
    btnCatSynth.setBounds(30, 512, 68, 30); // Digital
    btnCatPad.setBounds(104, 512, 68, 30);   // Layered

    // Character Switches click targets (over drawn rockers)
    swDynamics.setBounds(214, 444, 36, 20);
    swPedalNoise.setBounds(214, 508, 36, 20);
    swDamperRes.setBounds(214, 572, 36, 20);
    swSoftRelease.setBounds(214, 636, 36, 20);
    swDynamics.setAlpha(0.01f);
    swPedalNoise.setAlpha(0.01f);
    swDamperRes.setAlpha(0.01f);
    swSoftRelease.setAlpha(0.01f);

    // Tone & Filter Knobs (x: 424 to 484)
    sliderTone.setBounds(424, 436, 52, 48);          // Cutoff
    sliderResonanceKnob.setBounds(478, 530, 52, 48); // Reso
    sliderEnv.setBounds(424, 624, 52, 48);           // Env

    // Amp Envelope Knobs (x: 566)
    sliderAmpAttack.setBounds(566, 436, 52, 48);
    sliderAmpDecay.setBounds(566, 530, 52, 48);
    sliderAmpRelease.setBounds(566, 624, 52, 48);

    // Master FX Modules:
    // EQ (x: 664, y: 432)
    sliderBass.setBounds(672, 478, 40, 48);
    sliderMid.setBounds(720, 478, 40, 48);
    sliderTreble.setBounds(768, 478, 40, 48);

    // DYNAMICS (x: 832, y: 432)
    sliderCompThresh.setBounds(850, 478, 46, 48);
    sliderCompRatio.setBounds(920, 478, 46, 48);

    // DELAY (x: 664, y: 590)
    sliderDelayTime.setBounds(672, 636, 40, 48);
    sliderDelayFeedback.setBounds(720, 636, 40, 48);
    sliderTremoloDepth.setBounds(768, 636, 40, 48); // Delay Mix

    // REVERB (x: 832, y: 590)
    sliderReverbTone.setBounds(842, 636, 40, 48);
    sliderReverbAmount.setBounds(888, 636, 40, 48);
    sliderCompressor.setBounds(934, 636, 40, 48); // Reverb Mix knob

    if (midiMapOverlay != nullptr) {
        midiMapOverlay->setBounds(getLocalBounds());
    }
}
