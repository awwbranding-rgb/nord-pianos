#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
// Constructor & Setup: Nord Pianos (Stage 4 Architecture)
//==============================================================================
NordPianoPrototypeAudioProcessorEditor::NordPianoPrototypeAudioProcessorEditor(NordPianoPrototypeAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p) {
    setLookAndFeel(&blackKnobLAF);

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
    setupPillButton(btnMuteA, "M", true); setupPillButton(btnSoloA, "S", true);
    setupPillButton(btnMuteB, "M", true); setupPillButton(btnSoloB, "S", true);
    setupPillButton(btnMuteC, "M", true); setupPillButton(btnSoloC, "S", true);
    setupPillButton(btnMuteD, "M", true); setupPillButton(btnSoloD, "S", true);

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
    setupPillButton(swDynamics, "Dynamics", true);
    setupPillButton(swPedalNoise, "Pedal Noise", true);
    setupPillButton(swDamperRes, "Damper Res", true);
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
    setupPillButton(btnCatGrand, "GRAND", true);
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
    addAndMakeVisible(*pianoKeyboard);

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

    // 2. Render Top Bar Panel (18, 8, 988, 42)
    drawPanelCard(g, {18, 8, 988, 42}, "");

    // Top Bar Left: OmniStage Logo with Red Waveform Icon
    const float iconX = 30.0f;
    const float iconY = 18.0f;
    g.setColour(juce::Colour::fromRGB(229, 57, 53)); // Neon Red
    g.fillRoundedRectangle(iconX, iconY + 4.0f, 3.5f, 14.0f, 1.5f);
    g.fillRoundedRectangle(iconX + 6.0f, iconY, 3.5f, 22.0f, 1.5f);
    g.fillRoundedRectangle(iconX + 12.0f, iconY + 6.0f, 3.5f, 10.0f, 1.5f);
    g.fillRoundedRectangle(iconX + 18.0f, iconY + 2.0f, 3.5f, 18.0f, 1.5f);

    g.setFont(juce::Font(juce::FontOptions(20.0f, juce::Font::bold)));
    g.setColour(juce::Colour::fromRGB(255, 255, 255));
    g.drawText("Omni", 58, 14, 52, 22, juce::Justification::left);

    g.setColour(juce::Colour::fromRGB(229, 57, 53)); // OmniStage Red Accent
    g.drawText("Stage", 110, 14, 60, 22, juce::Justification::left);

    g.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::plain)));
    g.setColour(juce::Colour::fromRGB(150, 160, 175));
    g.drawText("Stage Preset Player", 59, 34, 130, 12, juce::Justification::left);

    // Top Bar Metrics (OUTPUT, VOICES, MEMORY)
    g.setFont(juce::Font(juce::FontOptions(8.5f, juce::Font::bold)));
    g.setColour(juce::Colour::fromRGB(150, 160, 175));
    g.drawText("OUTPUT: st. 1", 620, 18, 80, 12, juce::Justification::left);
    g.drawText("VOICES: " + juce::String(processor.getVoiceCount() > 0 ? processor.getVoiceCount() : 0) + " / 512", 710, 18, 90, 12, juce::Justification::left);
    const double memMb = processor.getTotalMemoryMB();
    g.drawText("MEMORY: " + juce::String(memMb > 0.1 ? memMb : 284.0, 0) + " MB", 810, 18, 90, 12, juce::Justification::left);

    // Top Bar Small Stereo VU Meter (Right side)
    const float miniMeterX = 910.0f;
    const float miniMeterY = 16.0f;
    for (int seg = 0; seg < 8; ++seg) {
        juce::Colour c = (seg < 5) ? juce::Colour::fromRGB(45, 200, 45) : (seg < 7 ? juce::Colour::fromRGB(240, 200, 30) : juce::Colour::fromRGB(230, 45, 45));
        g.setColour(c.withAlpha(seg < 4 ? 0.9f : 0.25f));
        g.fillRect(miniMeterX + seg * 4.0f, miniMeterY, 3.0f, 6.0f);
        g.fillRect(miniMeterX + seg * 4.0f, miniMeterY + 8.0f, 3.0f, 6.0f);
    }
    g.setFont(juce::Font(juce::FontOptions(7.5f, juce::Font::plain)));
    g.setColour(juce::Colour::fromRGB(120, 130, 145));
    g.drawText("L", miniMeterX - 8.0f, miniMeterY - 2.0f, 8.0f, 8.0f, juce::Justification::centred);
    g.drawText("R", miniMeterX - 8.0f, miniMeterY + 6.0f, 8.0f, 8.0f, juce::Justification::centred);

    //==========================================================================
    // 3. MAIN UPPER CONSOLE (Mixer, Central OLED & Programs, Master)
    //==========================================================================
    // Panel A: 4-Channel Mixer Strip (18, 96, 334, 244)
    drawPanelCard(g, {18, 96, 334, 244}, "");

    const std::array<juce::String, 4> chNames = {"A Piano", "B Worship Pad", "C Synth Lead", "D Organ B3"};
    const std::array<juce::Colour, 4> chColors = {
        juce::Colour::fromRGB(229, 57, 53),  // Red
        juce::Colour::fromRGB(0, 220, 240),  // Cyan
        juce::Colour::fromRGB(50, 130, 255), // Blue
        juce::Colour::fromRGB(255, 160, 30)  // Amber
    };

    for (int ch = 0; ch < 4; ++ch) {
        const int cx = 22 + ch * 82;
        // Channel Header Pill
        juce::Rectangle<float> chHdr(float(cx), 102.0f, 74.0f, 32.0f);
        g.setColour(ch == activeTab ? chColors[size_t(ch)].withAlpha(0.25f) : juce::Colour::fromRGB(16, 18, 24));
        g.fillRoundedRectangle(chHdr, 4.0f);
        g.setColour(ch == activeTab ? chColors[size_t(ch)] : juce::Colour::fromRGB(45, 52, 65));
        g.drawRoundedRectangle(chHdr, 4.0f, ch == activeTab ? 1.5f : 1.0f);

        // Header letter and name
        g.setFont(juce::Font(juce::FontOptions(13.0f, juce::Font::bold)));
        g.setColour(chColors[size_t(ch)]);
        g.drawText(chNames[size_t(ch)].substring(0, 1), cx + 4, 106, 16, 14, juce::Justification::centred);

        g.setFont(juce::Font(juce::FontOptions(8.5f, juce::Font::bold)));
        g.setColour(juce::Colour::fromRGB(230, 235, 245));
        g.drawText(chNames[size_t(ch)].substring(2), cx + 18, 107, 54, 14, juce::Justification::left);

        // Vertical LED VU Meter next to fader
        const float meterX = float(cx + 54);
        const float meterY = 148.0f;
        const float meterH = 130.0f;
        const int numSegments = 16;
        for (int s = 0; s < numSegments; ++s) {
            const float sy = meterY + meterH - (s + 1) * 7.5f;
            juce::Colour segCol = (s < 10) ? juce::Colour::fromRGB(50, 210, 50) : (s < 13 ? juce::Colour::fromRGB(245, 200, 35) : juce::Colour::fromRGB(235, 50, 50));
            const bool lit = (s < (ch == 0 ? 11 : (ch == 1 ? 8 : (ch == 2 ? 0 : 0))));
            g.setColour(lit ? segCol : segCol.withAlpha(0.15f));
            g.fillRect(meterX, sy, 5.0f, 5.5f);
        }

        // dB Readout below fader
        g.setFont(juce::Font(juce::FontOptions(8.5f, juce::Font::plain)));
        g.setColour(juce::Colour::fromRGB(150, 160, 175));
        g.drawText("0.0 dB", cx, 284, 52, 12, juce::Justification::centred);
    }

    // Panel B: Central OLED Display & Programs (360, 96, 484, 244)
    drawPanelCard(g, {360, 96, 484, 244}, "");

    // Central OLED Screen Glass (436, 108, 396, 134)
    juce::Rectangle<float> oledRect(436.0f, 108.0f, 396.0f, 134.0f);
    juce::ColourGradient oledGrad(juce::Colour::fromRGB(8, 12, 16), oledRect.getX(), oledRect.getY(),
                                  juce::Colour::fromRGB(4, 6, 9), oledRect.getX(), oledRect.getBottom(), false);
    g.setGradientFill(oledGrad);
    g.fillRoundedRectangle(oledRect, 6.0f);
    g.setColour(juce::Colour::fromRGB(0, 180, 220).withAlpha(0.6f)); // Cyan neon glow frame
    g.drawRoundedRectangle(oledRect, 6.0f, 1.2f);

    const int curProg = processor.getCurrentProgramIndex();

    // OLED Content: Song Title
    g.setFont(juce::Font(juce::FontOptions(15.0f, juce::Font::bold)));
    g.setColour(juce::Colour::fromRGB(0, 225, 255)); // Bright Cyan
    g.drawText("0" + juce::String(curProg + 1) + ": Great Is The Lord", 450, 116, 260, 20, juce::Justification::left);

    // OLED Layer List
    g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
    g.setColour(juce::Colour::fromRGB(229, 57, 53)); // Red
    g.drawText("A: Royal Grand", 450, 142, 180, 15, juce::Justification::left);

    g.setColour(juce::Colour::fromRGB(0, 220, 240)); // Cyan
    g.drawText("B: Shimmer Pad", 450, 160, 180, 15, juce::Justification::left);

    g.setColour(juce::Colour::fromRGB(50, 130, 255)); // Blue
    g.drawText("C: Bright Lead", 450, 178, 180, 15, juce::Justification::left);

    g.setColour(juce::Colour::fromRGB(255, 160, 30)); // Amber
    g.drawText("D: Organ B3", 450, 196, 180, 15, juce::Justification::left);

    // Minimalist Grand Piano Line-Art Graphic (right side of OLED)
    const float gpX = 720.0f;
    const float gpY = 145.0f;
    g.setColour(juce::Colour::fromRGB(0, 200, 240).withAlpha(0.75f));
    juce::Path pianoOutline;
    pianoOutline.startNewSubPath(gpX + 10.0f, gpY + 45.0f);
    pianoOutline.lineTo(gpX + 80.0f, gpY + 45.0f);
    pianoOutline.lineTo(gpX + 80.0f, gpY + 28.0f);
    pianoOutline.quadraticTo(gpX + 60.0f, gpY + 5.0f, gpX + 35.0f, gpY + 12.0f);
    pianoOutline.quadraticTo(gpX + 15.0f, gpY + 18.0f, gpX + 10.0f, gpY + 35.0f);
    pianoOutline.closeSubPath();
    g.strokePath(pianoOutline, juce::PathStrokeType(1.2f));

    // Prop stick and lid
    g.drawLine(gpX + 10.0f, gpY + 35.0f, gpX + 65.0f, gpY - 4.0f, 1.2f);
    g.drawLine(gpX + 45.0f, gpY + 10.0f, gpX + 48.0f, gpY + 28.0f, 1.0f);
    // Legs
    g.drawLine(gpX + 15.0f, gpY + 45.0f, gpX + 15.0f, gpY + 60.0f, 1.2f);
    g.drawLine(gpX + 75.0f, gpY + 45.0f, gpX + 75.0f, gpY + 60.0f, 1.2f);

    // Below OLED Screen: PROGRAMS Label with Red LED Dot
    g.setColour(juce::Colour::fromRGB(229, 57, 53));
    g.fillEllipse(374.0f, 260.0f, 6.0f, 6.0f); // LED Dot
    g.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::bold)));
    g.setColour(juce::Colour::fromRGB(220, 225, 235));
    g.drawText("PROGRAMS", 385, 256, 60, 14, juce::Justification::left);

    // Panel C: Master Section (852, 96, 154, 244)
    drawPanelCard(g, {852, 96, 154, 244}, "MASTER", true);

    // Dual 24-Segment Stereo LED Peak Meters
    const float masterMeterX = 926.0f;
    const float masterMeterY = 146.0f;
    const float masterMeterH = 138.0f;
    const int numMasterSegs = 20;
    for (int ms = 0; ms < numMasterSegs; ++ms) {
        const float my = masterMeterY + masterMeterH - (ms + 1) * 6.6f;
        juce::Colour mCol = (ms < 13) ? juce::Colour::fromRGB(50, 215, 50) : (ms < 17 ? juce::Colour::fromRGB(245, 205, 30) : juce::Colour::fromRGB(235, 45, 45));
        const bool mLit = (ms < 14);
        g.setColour(mLit ? mCol : mCol.withAlpha(0.15f));
        g.fillRect(masterMeterX, my, 6.0f, 5.0f);        // Left
        g.fillRect(masterMeterX + 9.0f, my, 6.0f, 5.0f); // Right
    }
    g.setFont(juce::Font(juce::FontOptions(8.5f, juce::Font::plain)));
    g.setColour(juce::Colour::fromRGB(150, 160, 175));
    g.drawText("0.0 dB", 866, 292, 52, 12, juce::Justification::centred);

    //==========================================================================
    // 4. BOTTOM HALF: DEDICATED SOUND EDITING SECTION (PIANO A)
    //==========================================================================
    // Card 1: SOUND / TIMBRE (18, 348, 988, 128)
    drawPanelCard(g, {18, 348, 988, 128}, "SOUND / TIMBRE", true);

    g.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::bold)));
    g.setColour(juce::Colour::fromRGB(150, 160, 175));
    g.drawText("CHARACTER", 310, 368, 86, 12, juce::Justification::left);
    g.drawText("TONE", 430, 368, 160, 12, juce::Justification::left);
    g.drawText("AMP / FX SEND", 630, 368, 160, 12, juce::Justification::left);

    // Tone Knobs Labels
    g.setFont(juce::Font(juce::FontOptions(8.5f, juce::Font::bold)));
    g.drawText("Cutoff", 430, 446, 52, 12, juce::Justification::centred);
    g.drawText("Reso", 488, 446, 52, 12, juce::Justification::centred);
    g.drawText("Env", 546, 446, 52, 12, juce::Justification::centred);

    // Amp Knobs Labels
    g.drawText("A", 630, 446, 52, 12, juce::Justification::centred);
    g.drawText("D", 688, 446, 52, 12, juce::Justification::centred);
    g.drawText("R", 746, 446, 52, 12, juce::Justification::centred);

    // Bottom Sub-Panels: EQ, COMP, DYNAMICS, EFFECTS (y: 484, h: 104)
    drawPanelCard(g, {18, 484, 236, 104}, "EQ", true);
    drawPanelCard(g, {262, 484, 180, 104}, "COMP", true);
    drawPanelCard(g, {450, 484, 180, 104}, "DYNAMICS", true);
    drawPanelCard(g, {638, 484, 368, 104}, "EFFECTS", true);

    g.setFont(juce::Font(juce::FontOptions(8.5f, juce::Font::bold)));
    g.setColour(juce::Colour::fromRGB(150, 160, 175));
    // EQ labels
    g.drawText("Low", 38, 570, 52, 12, juce::Justification::centred);
    g.drawText("Mid", 110, 570, 52, 12, juce::Justification::centred);
    g.drawText("High", 182, 570, 52, 12, juce::Justification::centred);

    // Comp labels
    g.drawText("Thresh", 282, 570, 52, 12, juce::Justification::centred);
    g.drawText("Ratio", 354, 570, 52, 12, juce::Justification::centred);

    // Dynamics labels
    g.drawText("Attack", 470, 570, 52, 12, juce::Justification::centred);
    g.drawText("Release", 542, 570, 52, 12, juce::Justification::centred);

    // Effects Mix label
    g.drawText("Mix", 880, 568, 56, 12, juce::Justification::centred);
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
    // 1. Top Bar
    btnQuickPrev.setBounds(270, 16, 22, 22);
    btnQuickSelect.setBounds(296, 14, 280, 26);
    btnQuickNext.setBounds(580, 16, 22, 22);
    btnSplitLayer.setBounds(850, 14, 110, 26);
    btnSettings.setBounds(968, 14, 26, 26);

    // 2. Instrument Tabs (y: 56, h: 32)
    tabPiano.setBounds(18, 56, 175, 32);
    tabPad.setBounds(201, 56, 175, 32);
    tabSynth.setBounds(384, 56, 175, 32);
    tabOrgan.setBounds(567, 56, 175, 32);

    // 3. Main Upper Console: 4-Channel Mixer Strip (x: 18, w: 324)
    const int mixerY = 100;
    const int chanW = 76;
    for (int ch = 0; ch < 4; ++ch) {
        const int cx = 22 + ch * (chanW + 6);
        if (ch == 0) {
            sliderVolA.setBounds(cx + 4, mixerY + 40, 42, 142);
            btnOnA.setBounds(cx + 6, mixerY + 202, 40, 24);
        } else if (ch == 1) {
            sliderVolB.setBounds(cx + 4, mixerY + 40, 42, 142);
            btnOnB.setBounds(cx + 6, mixerY + 202, 40, 24);
        } else if (ch == 2) {
            sliderVolC.setBounds(cx + 4, mixerY + 40, 42, 142);
            btnOnC.setBounds(cx + 6, mixerY + 202, 40, 24);
        } else {
            sliderVolD.setBounds(cx + 4, mixerY + 40, 42, 142);
            btnOnD.setBounds(cx + 6, mixerY + 202, 40, 24);
        }
    }

    // Center Console: Store, Save, Bank Selector & Programs
    btnStore.setBounds(372, 116, 52, 26);
    btnSave.setBounds(372, 148, 52, 26);

    btnBankA.setBounds(362, 182, 18, 18);
    btnBankB.setBounds(382, 182, 18, 18);
    btnBankC.setBounds(402, 182, 18, 18);
    btnBankD.setBounds(422, 182, 18, 18);

    // 8 Program Buttons below OLED
    for (int i = 0; i < 8; ++i) {
        btnPrograms[size_t(i)].setBounds(446 + i * 46, 252, 40, 30);
    }

    // Right Console: Master Section
    sliderMainVolume.setBounds(868, 140, 44, 142);

    // 4. Bottom Half: Sound Editing Section (y: 348 to 726)
    // Sound / Timbre Categories
    btnCatGrand.setBounds(28, 388, 62, 22);
    btnCatEPiano.setBounds(94, 388, 62, 22);
    btnCatClav.setBounds(160, 388, 62, 22);
    btnCatOrgan.setBounds(226, 388, 62, 22);

    btnCatPad.setBounds(28, 414, 62, 22);
    btnCatSynth.setBounds(94, 414, 62, 22);
    btnCatStrings.setBounds(160, 414, 62, 22);
    btnCatMisc.setBounds(226, 414, 62, 22);

    // Character Switches
    swDynamics.setBounds(310, 388, 86, 20);
    swPedalNoise.setBounds(310, 412, 86, 20);
    swDamperRes.setBounds(310, 436, 86, 20);

    // Tone Knobs
    sliderTone.setBounds(430, 396, 52, 48);
    sliderResonanceKnob.setBounds(488, 396, 52, 48);
    sliderEnv.setBounds(546, 396, 52, 48);

    // Amp / FX Send Knobs
    sliderAmpAttack.setBounds(630, 396, 52, 48);
    sliderAmpDecay.setBounds(688, 396, 52, 48);
    sliderAmpRelease.setBounds(746, 396, 52, 48);

    // Bottom Sub-Panels: EQ, Comp, Dynamics, Effects
    // EQ (x: 18, w: 236)
    sliderBass.setBounds(38, 520, 52, 48);
    sliderMid.setBounds(110, 520, 52, 48);
    sliderTreble.setBounds(182, 520, 52, 48);

    // Comp (x: 262, w: 180)
    sliderCompThresh.setBounds(282, 520, 52, 48);
    sliderCompRatio.setBounds(354, 520, 52, 48);

    // Dynamics (x: 450, w: 180)
    sliderDynAttack.setBounds(470, 520, 52, 48);
    sliderDynRelease.setBounds(542, 520, 52, 48);

    // Effects (x: 638, w: 368)
    btnFxChorus.setBounds(656, 528, 64, 22);
    btnFxDelay.setBounds(726, 528, 64, 22);
    btnFxReverb.setBounds(796, 528, 64, 22);
    sliderFxMix.setBounds(880, 514, 56, 52);

    if (midiMapOverlay != nullptr) {
        midiMapOverlay->setBounds(getLocalBounds());
    }
}
