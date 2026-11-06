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
// Ultimate Stage Pianos Custom Hardware Drawing Helpers
//==============================================================================
static void draw7SegmentNumber(juce::Graphics& g, float x, float y, float w, float h, int num) {
    juce::Rectangle<float> rect(x, y, w, h);
    // Dark red glass background with inner bezel
    g.setColour(juce::Colour::fromRGB(18, 3, 5));
    g.fillRoundedRectangle(rect, 3.0f);
    g.setColour(juce::Colour::fromRGB(45, 10, 14));
    g.drawRoundedRectangle(rect, 3.0f, 1.2f);

    // Digit layout
    const float cx = x + w * 0.5f;
    const float cy = y + h * 0.5f;
    const float segW = w * 0.42f;
    const float segH = h * 0.72f;
    const float thick = 4.2f;

    // Segment active states: A(top), B(top-right), C(bot-right), D(bottom), E(bot-left), F(top-left), G(mid)
    bool sA = (num != 1 && num != 4);
    bool sB = (num != 5 && num != 6);
    bool sC = (num != 2);
    bool sD = (num != 1 && num != 4 && num != 7);
    bool sE = (num == 0 || num == 2 || num == 6 || num == 8);
    bool sF = (num != 1 && num != 2 && num != 3 && num != 7);
    bool sG = (num != 0 && num != 1 && num != 7);

    juce::Colour litCol = juce::Colour::fromRGB(255, 38, 48);
    juce::Colour unlitCol = juce::Colour::fromRGB(38, 6, 8);

    auto drawSegH = [&](float sx, float sy, bool lit) {
        g.setColour(lit ? litCol : unlitCol);
        if (lit) {
            g.setColour(litCol.withAlpha(0.25f));
            g.fillRoundedRectangle(sx - 1.0f, sy - 1.0f, segW + 2.0f, thick + 2.0f, 1.5f);
            g.setColour(litCol);
        }
        g.fillRoundedRectangle(sx, sy, segW, thick, 1.0f);
    };

    auto drawSegV = [&](float sx, float sy, bool lit) {
        float vh = (segH - thick * 3.0f) * 0.5f;
        g.setColour(lit ? litCol : unlitCol);
        if (lit) {
            g.setColour(litCol.withAlpha(0.25f));
            g.fillRoundedRectangle(sx - 1.0f, sy - 1.0f, thick + 2.0f, vh + 2.0f, 1.5f);
            g.setColour(litCol);
        }
        g.fillRoundedRectangle(sx, sy, thick, vh, 1.0f);
    };

    float leftX = cx - segW * 0.5f;
    float rightX = cx + segW * 0.5f - thick;
    float topY = cy - segH * 0.5f;
    float midY = cy - thick * 0.5f;
    float botY = cy + segH * 0.5f - thick;
    float topVY = topY + thick;
    float botVY = midY + thick;

    drawSegH(leftX, topY, sA);
    drawSegV(rightX, topVY, sB);
    drawSegV(rightX, botVY, sC);
    drawSegH(leftX, botY, sD);
    drawSegV(leftX, botVY, sE);
    drawSegV(leftX, topVY, sF);
    drawSegH(leftX, midY, sG);
}

static void drawKnobGradTicks(juce::Graphics& g, float cx, float cy, float radius, float startAngle, float endAngle, int numTicks, bool centerDetent) {
    for (int i = 0; i < numTicks; ++i) {
        float norm = float(i) / float(numTicks - 1);
        float angle = startAngle + norm * (endAngle - startAngle);
        float r1 = radius + 3.0f;
        bool isCenter = centerDetent && (i == numTicks / 2);
        bool isMajor = (i == 0 || i == numTicks - 1 || isCenter);
        float r2 = radius + (isMajor ? 8.0f : 5.0f);

        float x1 = cx + std::sin(angle) * r1;
        float y1 = cy - std::cos(angle) * r1;
        float x2 = cx + std::sin(angle) * r2;
        float y2 = cy - std::cos(angle) * r2;

        if (isCenter) {
            g.setColour(juce::Colour::fromRGB(235, 45, 52));
            g.drawLine(x1, y1, x2, y2, 2.0f);
        } else {
            g.setColour(juce::Colour::fromRGB(isMajor ? 170 : 110, isMajor ? 180 : 120, isMajor ? 195 : 135));
            g.drawLine(x1, y1, x2, y2, isMajor ? 1.4f : 1.0f);
        }
    }
}

static void drawNordEncoder(juce::Graphics& g, float cx, float cy, float radius) {
    // Outer drop shadow
    g.setColour(juce::Colour::fromRGB(0, 0, 0).withAlpha(0.7f));
    g.fillEllipse(cx - radius - 1.0f, cy - radius + 2.0f, (radius + 1.0f) * 2.0f, (radius + 1.0f) * 2.0f);

    // Red Notched Gear Ring (Nord Stage encoder hallmark)
    const float ringR = radius;
    for (int i = 0; i < 24; ++i) {
        float angle = float(i) * juce::MathConstants<float>::pi / 12.0f;
        float rx1 = cx + std::cos(angle) * (ringR - 3.0f);
        float ry1 = cy + std::sin(angle) * (ringR - 3.0f);
        float rx2 = cx + std::cos(angle) * ringR;
        float ry2 = cy + std::sin(angle) * ringR;
        g.setColour(juce::Colour::fromRGB(229, 57, 53)); // Nord Red
        g.drawLine(rx1, ry1, rx2, ry2, 2.0f);
    }

    // Inner Matte Black Body
    const float innerR = radius - 3.5f;
    juce::ColourGradient knobGrad(juce::Colour::fromRGB(45, 52, 64), cx, cy - innerR,
                                  juce::Colour::fromRGB(14, 16, 22), cx, cy + innerR, false);
    g.setGradientFill(knobGrad);
    g.fillEllipse(cx - innerR, cy - innerR, innerR * 2.0f, innerR * 2.0f);
    g.setColour(juce::Colour::fromRGB(25, 30, 40));
    g.drawEllipse(cx - innerR, cy - innerR, innerR * 2.0f, innerR * 2.0f, 1.0f);

    // Center Red Dot
    g.setColour(juce::Colour::fromRGB(229, 57, 53));
    g.fillEllipse(cx - 2.5f, cy - 2.5f, 5.0f, 5.0f);
}

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
    g.setColour(juce::Colour::fromRGB(2, 3, 5).withAlpha(0.9f));
    g.fillRoundedRectangle(bf.translated(0.0f, 3.0f), 5.0f);

    // 2. Metal Panel Base (#191D24 to #0E1015)
    juce::ColourGradient panelGrad(juce::Colour::fromRGB(24, 28, 36), bf.getX(), bf.getY(),
                                  juce::Colour::fromRGB(12, 14, 18), bf.getX(), bf.getBottom(), false);
    g.setGradientFill(panelGrad);
    g.fillRoundedRectangle(bf, 4.0f);

    // 3. Brushed Metal Bevel Rim (#3D4552)
    g.setColour(juce::Colour::fromRGB(55, 63, 75));
    g.drawRoundedRectangle(bf, 4.0f, 1.2f);

    // Top Highlight Reflection
    g.setColour(juce::Colour::fromRGB(255, 255, 255).withAlpha(0.07f));
    g.drawLine(bf.getX() + 4.0f, bf.getY() + 1.0f, bf.getRight() - 4.0f, bf.getY() + 1.0f, 1.0f);

    // 4. Authentic Allen Screws in corners
    drawAllenScrew(g, bf.getX() + 9.0f, bf.getY() + 9.0f);
    drawAllenScrew(g, bf.getRight() - 9.0f, bf.getY() + 9.0f);
    drawAllenScrew(g, bf.getX() + 9.0f, bf.getBottom() - 9.0f);
    drawAllenScrew(g, bf.getRight() - 9.0f, bf.getBottom() - 9.0f);

    // 5. Panel Header Title & Crimson Status LED
    if (title.isNotEmpty()) {
        const float tx = bf.getX() + 20.0f;
        const float ty = bf.getY() + 10.0f;

        if (hasLed) {
            // Nord Stage Crimson Red LED (#E53935)
            g.setColour(juce::Colour::fromRGB(229, 57, 53).withAlpha(0.45f));
            g.fillEllipse(tx - 2.0f, ty, 11.0f, 11.0f); // Halo glow

            g.setColour(juce::Colour::fromRGB(255, 55, 55));
            g.fillEllipse(tx, ty + 2.0f, 7.0f, 7.0f); // LED Core

            g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
            g.setColour(juce::Colour::fromRGB(245, 248, 252));
            g.drawText(title, int(tx + 14.0f), int(ty), int(bf.getWidth() - 45.0f), 13, juce::Justification::left);
        } else {
            g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
            g.setColour(juce::Colour::fromRGB(245, 248, 252));
            g.drawText(title, int(tx), int(ty), int(bf.getWidth() - 40.0f), 13, juce::Justification::left);
        }
    }
}

//==============================================================================
// Main Paint Function (1024 x 682 Stage Interface)
//==============================================================================
void NordPianoPrototypeAudioProcessorEditor::paint(juce::Graphics& g) {
    // 1. Classic Nord Crimson Red Chassis Background (#88131B) with Subtle Metallic Grain
    juce::ColourGradient chassiGrad(juce::Colour::fromRGB(142, 22, 32), 0.0f, 0.0f,
                                    juce::Colour::fromRGB(100, 12, 18), 0.0f, (float)getHeight(), false);
    g.setGradientFill(chassiGrad);
    g.fillAll();

    // 4 Corner Chassis Hex Screws
    drawAllenScrew(g, 12.0f, 12.0f);
    drawAllenScrew(g, (float)getWidth() - 12.0f, 12.0f);
    drawAllenScrew(g, 12.0f, (float)getHeight() - 12.0f);
    drawAllenScrew(g, (float)getWidth() - 12.0f, (float)getHeight() - 12.0f);

    // 2. TOP KONTAKT-STYLE HEADER BAR (x: 18, y: 10, w: 988, h: 44)
    juce::Rectangle<float> topBar(18.0f, 8.0f, 988.0f, 44.0f);
    g.setColour(juce::Colour::fromRGB(18, 22, 28));
    g.fillRoundedRectangle(topBar, 3.5f);
    g.setColour(juce::Colour::fromRGB(42, 48, 60));
    g.drawRoundedRectangle(topBar, 3.5f, 1.0f);

    // Left Icon (wrench & u logo)
    g.setFont(juce::Font(juce::FontOptions(13.0f, juce::Font::bold)));
    g.setColour(juce::Colour::fromRGB(160, 175, 195));
        // Left Icon (Vector Wrench - Kontakt hallmark)
    {
        juce::Path wrench;
        wrench.startNewSubPath(25.0f, 26.0f);
        wrench.lineTo(30.0f, 21.0f);
        wrench.lineTo(29.0f, 18.0f);
        wrench.lineTo(32.0f, 16.0f);
        wrench.lineTo(34.0f, 18.0f);
        wrench.lineTo(33.0f, 21.0f);
        wrench.lineTo(30.0f, 22.0f);
        wrench.lineTo(26.0f, 27.0f);
        wrench.closeSubPath();
        g.setColour(juce::Colour::fromRGB(160, 175, 195));
        g.fillPath(wrench);
    }

    // Purge button & camera icons
    g.setColour(juce::Colour::fromRGB(32, 38, 50));
    g.fillRoundedRectangle(360.0f, 13.0f, 48.0f, 18.0f, 2.5f);
    g.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::bold)));
    g.setColour(juce::Colour::fromRGB(200, 210, 225));
    g.drawText("Purge", 360, 13, 48, 18, juce::Justification::centred);

    // Header Status Strip: Line 2 (OUTPUT, VOICES, MEMORY)
    g.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::bold)));
    g.setColour(juce::Colour::fromRGB(150, 165, 185));
    g.drawText("Output: st. 1", 48, 32, 75, 16, juce::Justification::left);
    g.drawText("MIDI Ch: omni", 130, 32, 85, 16, juce::Justification::left);

    g.drawText("Voices: " + juce::String(processor.getVoiceCount() > 0 ? processor.getVoiceCount() : 0) + "  Max: 128", 225, 32, 115, 16, juce::Justification::left);
    const double memMb = processor.getTotalMemoryMB();
    g.drawText("Memory: " + juce::String(memMb > 0.1 ? memMb : 35.46, 2) + " MB", 345, 32, 110, 16, juce::Justification::left);

    // Top Right: Tune Knob & Solo/Mute
    g.setFont(juce::Font(juce::FontOptions(8.5f, juce::Font::plain)));
    g.setColour(juce::Colour::fromRGB(150, 165, 185));
    g.drawText("Tune", 736, 12, 40, 12, juce::Justification::centred);
    g.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::bold)));
    g.setColour(juce::Colour::fromRGB(240, 245, 255));
    g.drawText("0.00", 736, 32, 40, 14, juce::Justification::centred);

    // Solo & Mute Buttons
    g.setColour(juce::Colour::fromRGB(32, 38, 50));
    g.fillRoundedRectangle(696.0f, 13.0f, 18.0f, 16.0f, 2.0f);
    g.fillRoundedRectangle(696.0f, 31.0f, 18.0f, 16.0f, 2.0f);
    g.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::bold)));
    g.setColour(juce::Colour::fromRGB(220, 230, 245));
    g.drawText("S", 696, 13, 18, 16, juce::Justification::centred);
    g.drawText("M", 696, 31, 18, 16, juce::Justification::centred);

    // Top Right Stereo VU Meters with Volume Slider Fader
    const float topMeterX = 810.0f;
    const float topMeterY = 14.0f;
    g.setColour(juce::Colour::fromRGB(12, 15, 20));
    g.fillRect(topMeterX, topMeterY, 150.0f, 12.0f);
    g.fillRect(topMeterX, topMeterY + 16.0f, 150.0f, 12.0f);
    for (int seg = 0; seg < 20; ++seg) {
        juce::Colour c = (seg < 13) ? juce::Colour::fromRGB(40, 210, 45) : (seg < 17 ? juce::Colour::fromRGB(245, 205, 35) : juce::Colour::fromRGB(235, 45, 45));
        g.setColour(c.withAlpha(seg < 10 ? 0.95f : 0.18f));
        g.fillRect(topMeterX + 2.0f + seg * 7.3f, topMeterY + 2.0f, 5.2f, 8.0f);
        g.fillRect(topMeterX + 2.0f + seg * 7.3f, topMeterY + 18.0f, 5.2f, 8.0f);
    }

    //==========================================================================
    // 3. MAIN LOGO: "omni stage pianos" (y: 54, h: 36)
    //==========================================================================
    // Drop shadow
    g.setFont(juce::Font(juce::FontOptions(29.0f, juce::Font::bold)));
    g.setColour(juce::Colour::fromRGB(0, 0, 0).withAlpha(0.6f));
    g.drawText("omni stage pianos", 0, 55, getWidth(), 32, juce::Justification::centred);
    g.setColour(juce::Colour::fromRGB(255, 255, 255));
    g.drawText("omni stage pianos", 0, 54, getWidth(), 32, juce::Justification::centred);

    g.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::bold)));
    g.setColour(juce::Colour::fromRGB(255, 175, 180));
    g.drawText("STAGE WORKSTATION HD", 0, 84, getWidth(), 14, juce::Justification::centred);

    //==========================================================================
    // 4. THE 5 MAIN CONTROL COLUMNS (y: 100 to 716, h: 616)
    //==========================================================================
    // COLUMN 1: EQ PIANO & DELAY (x: 18, w: 156)
    drawPanelCard(g, {18, 100, 156, 316}, "EQ PIANO", true);
    // EQ Labels & Graduated Ticks
    drawKnobGradTicks(g, 96.0f, 148.0f, 21.0f, -2.4f, 2.4f, 11, true);
    drawKnobGradTicks(g, 96.0f, 228.0f, 21.0f, -2.4f, 2.4f, 11, true);
    drawKnobGradTicks(g, 96.0f, 308.0f, 21.0f, -2.4f, 2.4f, 11, true);

    g.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::bold)));
    g.setColour(juce::Colour::fromRGB(180, 190, 205));
    g.drawText("TREBLE", 18, 174, 156, 12, juce::Justification::centred);
    g.drawText("MID", 18, 254, 156, 12, juce::Justification::centred);
    g.drawText("BASS", 18, 334, 156, 12, juce::Justification::centred);

    // EQ Scale tick texts
    g.setFont(juce::Font(juce::FontOptions(7.5f, juce::Font::plain)));
    g.setColour(juce::Colour::fromRGB(120, 130, 145));
    g.drawText("-15 dB", 24, 154, 30, 10, juce::Justification::left);
    g.drawText("+15 dB", 138, 154, 30, 10, juce::Justification::right);
    g.drawText("-15 dB", 24, 234, 30, 10, juce::Justification::left);
    g.drawText("+15 dB", 138, 234, 30, 10, juce::Justification::right);
    g.drawText("-15 dB", 24, 314, 30, 10, juce::Justification::left);
    g.drawText("+15 dB", 138, 314, 30, 10, juce::Justification::right);

    // DELAY Module (x: 18, y: 424, w: 156, h: 292)
    drawPanelCard(g, {18, 424, 156, 292}, "DELAY", true);
    // Delay meters flanking knobs
    drawVerticalVuMeter(g, 34.0f, 482.0f, 6.0f, 70.0f, 0.65f);
    drawVerticalVuMeter(g, 153.0f, 482.0f, 6.0f, 70.0f, 0.65f);
    drawKnobGradTicks(g, 63.0f, 516.0f, 16.0f, -2.4f, 2.4f, 7, false);
    drawKnobGradTicks(g, 99.0f, 516.0f, 16.0f, -2.4f, 2.4f, 7, false);
    drawKnobGradTicks(g, 135.0f, 516.0f, 16.0f, -2.4f, 2.4f, 7, false);

    g.setFont(juce::Font(juce::FontOptions(8.5f, juce::Font::bold)));
    g.setColour(juce::Colour::fromRGB(180, 190, 205));
    g.drawText("FEED", 46, 564, 34, 12, juce::Justification::centred);
    g.drawText("TIME", 82, 564, 34, 12, juce::Justification::centred);
    g.drawText("MIX", 118, 564, 34, 12, juce::Justification::centred);

    // COLUMN 2: PIANO ENGINE & COMP (x: 180, w: 182)
    drawPanelCard(g, {180, 100, 182, 420}, "");
    // Red Backlit Header Pill: PIANO
    juce::Rectangle<float> pianoPill(194.0f, 108.0f, 154.0f, 24.0f);
    g.setColour(juce::Colour::fromRGB(185, 26, 36));
    g.fillRoundedRectangle(pianoPill, 3.0f);
    g.setFont(juce::Font(juce::FontOptions(13.0f, juce::Font::bold)));
    g.setColour(juce::Colour::fromRGB(255, 255, 255));
    g.drawText("PIANO", pianoPill, juce::Justification::centred);

    // Model 7-Segment Display & Label
    draw7SegmentNumber(g, 236.0f, 138.0f, 70.0f, 38.0f, 1);
    g.setFont(juce::Font(juce::FontOptions(8.0f, juce::Font::bold)));
    g.setColour(juce::Colour::fromRGB(150, 160, 175));
    g.drawText("MODEL", 236, 176, 70, 12, juce::Justification::centred);

    // Robust Vertical Studio LED VU Ladder Meter alongside the Piano Fader
    float pLevel = float(sliderPianoVol.getValue()) / 1.5f;
    drawVerticalVuMeter(g, 196.0f, 226.0f, 10.0f, 188.0f, pLevel);

    // Fader Section Labels & dB Scale markers
    g.setFont(juce::Font(juce::FontOptions(8.5f, juce::Font::bold)));
    g.setColour(juce::Colour::fromRGB(175, 185, 200));
    g.drawText("LEVEL", 194, 422, 154, 12, juce::Justification::centred);

    // LED dots for STR RES & OPN STR
    g.setColour(juce::Colour::fromRGB(229, 57, 53));
    g.fillEllipse(204.0f, 442.0f, 5.0f, 5.0f);
    g.fillEllipse(280.0f, 442.0f, 5.0f, 5.0f);
    g.setFont(juce::Font(juce::FontOptions(8.0f, juce::Font::bold)));
    g.setColour(juce::Colour::fromRGB(215, 225, 240));
    g.drawText("STR RES", 212, 438, 54, 14, juce::Justification::left);
    g.drawText("OPN STR", 288, 438, 54, 14, juce::Justification::left);

    // COMP Module (x: 180, y: 528, w: 182, h: 188)
    drawPanelCard(g, {180, 528, 182, 188}, "COMP", true);
    drawKnobGradTicks(g, 271.0f, 597.0f, 24.0f, -2.4f, 2.4f, 9, false);
    g.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::bold)));
    g.setColour(juce::Colour::fromRGB(180, 190, 205));
    g.drawText("WARM AMOUNT", 180, 646, 182, 12, juce::Justification::centred);

    // COLUMN 3: MASTER OLED DISPLAY & MULTI-PROGRAM (x: 368, w: 288)
    drawPanelCard(g, {368, 100, 288, 616}, "");

    // Central OLED Stage Screen (x: 378, y: 110, w: 268, h: 140)
    juce::Rectangle<float> oledBox(378.0f, 110.0f, 268.0f, 140.0f);
    g.setColour(juce::Colour::fromRGB(4, 7, 12));
    g.fillRoundedRectangle(oledBox, 4.0f);
    g.setColour(juce::Colour::fromRGB(0, 180, 220).withAlpha(0.6f)); // Cyan border
    g.drawRoundedRectangle(oledBox, 4.0f, 1.2f);

    // Program 1 Header
    g.setFont(juce::Font(juce::FontOptions(22.0f, juce::Font::bold)));
    g.setColour(juce::Colour::fromRGB(0, 225, 255)); // Vivid Cyan
    g.drawText("Program 1", 390, 116, 240, 28, juce::Justification::left);

    // Divider line
    g.setColour(juce::Colour::fromRGB(0, 160, 210).withAlpha(0.5f));
    g.drawHorizontalLine(148, 390.0f, 634.0f);

    // Sound Layers Active
    g.setFont(juce::Font(juce::FontOptions(11.5f, juce::Font::bold)));
    g.setColour(juce::Colour::fromRGB(0, 220, 245));
    g.drawText("PIANO: Royal Grand 3D", 390, 154, 240, 18, juce::Justification::left);
    g.drawText("SYNTH: Cloud Shimmer", 390, 174, 240, 18, juce::Justification::left);

    g.setFont(juce::Font(juce::FontOptions(8.5f, juce::Font::plain)));
    g.setColour(juce::Colour::fromRGB(120, 150, 180));
    g.drawText("OmniStage HD Audio Engine", 390, 204, 240, 14, juce::Justification::left);
    g.drawText("Boutique Stage Fidelity | 24-bit", 390, 220, 240, 14, juce::Justification::left);

    // Transpose & Octave Readout Boxes
    g.setFont(juce::Font(juce::FontOptions(8.5f, juce::Font::bold)));
    g.setColour(juce::Colour::fromRGB(150, 160, 175));
    g.drawText("TRANSPOSE", 378, 274, 76, 12, juce::Justification::centred);
    g.drawText("OCTAVE", 572, 274, 76, 12, juce::Justification::centred);

    // Transpose box (0)
    g.setColour(juce::Colour::fromRGB(12, 14, 18));
    g.fillRoundedRectangle(398.0f, 290.0f, 36.0f, 22.0f, 3.0f);
    g.setColour(juce::Colour::fromRGB(45, 52, 65));
    g.drawRoundedRectangle(398.0f, 290.0f, 36.0f, 22.0f, 3.0f, 1.0f);
    g.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
    g.setColour(juce::Colour::fromRGB(250, 252, 255));
    g.drawText("0", 398, 290, 36, 22, juce::Justification::centred);

    // Octave box (0)
    g.setColour(juce::Colour::fromRGB(12, 14, 18));
    g.fillRoundedRectangle(592.0f, 290.0f, 36.0f, 22.0f, 3.0f);
    g.setColour(juce::Colour::fromRGB(45, 52, 65));
    g.drawRoundedRectangle(592.0f, 290.0f, 36.0f, 22.0f, 3.0f, 1.0f);
    g.drawText("0", 592, 290, 36, 22, juce::Justification::centred);

    // Center PART MIX Header & Knob Ticks
    g.setFont(juce::Font(juce::FontOptions(10.5f, juce::Font::bold)));
    g.setColour(juce::Colour::fromRGB(230, 235, 245));
    g.drawText("PART MIX", 462, 268, 100, 14, juce::Justification::centred);
    drawKnobGradTicks(g, 512.0f, 326.0f, 35.0f, -2.4f, 2.4f, 13, true);

    // PART MIX Arc Labels
    g.setFont(juce::Font(juce::FontOptions(8.5f, juce::Font::bold)));
    g.setColour(juce::Colour::fromRGB(229, 57, 53));
    g.drawText("PIANO", 432, 356, 54, 12, juce::Justification::left);
    g.setColour(juce::Colour::fromRGB(50, 140, 255));
    g.drawText("SYNTH", 538, 356, 54, 12, juce::Justification::right);

    // Memory Mode Subtitle
    g.setFont(juce::Font(juce::FontOptions(8.5f, juce::Font::plain)));
    g.setColour(juce::Colour::fromRGB(130, 140, 155));
    g.drawText("Memory Mode: Single Program", 368, 434, 288, 12, juce::Justification::centred);

    // PROGRAMS Header with LED Dots
    g.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::bold)));
    g.setColour(juce::Colour::fromRGB(220, 230, 245));
    g.drawText("PROGRAMS", 368, 452, 288, 14, juce::Justification::centred);

    // Red LED dots over Program buttons 1-4 and 5-8
    for (int b = 0; b < 4; ++b) {
        float bx = 424.0f + b * 48.0f;
        // Upper row dots
        g.setColour(b == 0 ? juce::Colour::fromRGB(229, 57, 53) : juce::Colour::fromRGB(50, 15, 20));
        g.fillEllipse(bx + 18.0f, 474.0f, 6.0f, 6.0f);
        // Lower row dots
        g.setColour(juce::Colour::fromRGB(50, 15, 20));
        g.fillEllipse(bx + 18.0f, 526.0f, 6.0f, 6.0f);
    }

    // COLUMN 4: SYNTH ENGINE & FEATURES / KEY RANGE (x: 662, w: 182)
    drawPanelCard(g, {662, 100, 182, 420}, "");
    // Red Backlit Header Pill: SYNTH
    juce::Rectangle<float> synthPill(676.0f, 108.0f, 154.0f, 24.0f);
    g.setColour(juce::Colour::fromRGB(185, 26, 36));
    g.fillRoundedRectangle(synthPill, 3.0f);
    g.setFont(juce::Font(juce::FontOptions(13.0f, juce::Font::bold)));
    g.setColour(juce::Colour::fromRGB(255, 255, 255));
    g.drawText("SYNTH", synthPill, juce::Justification::centred);

    // Sample 7-Segment Display & Label
    draw7SegmentNumber(g, 718.0f, 138.0f, 70.0f, 38.0f, 3);
    g.setFont(juce::Font(juce::FontOptions(8.0f, juce::Font::bold)));
    g.setColour(juce::Colour::fromRGB(150, 160, 175));
    g.drawText("SAMPLE", 718, 176, 70, 12, juce::Justification::centred);

    // Robust Vertical Studio LED VU Ladder Meter alongside the Synth Fader
    float sLevel = float(sliderPadVol.getValue()) / 1.5f;
    drawVerticalVuMeter(g, 678.0f, 226.0f, 10.0f, 188.0f, sLevel);

    // Fader Section Labels & dB Scale markers
    g.setFont(juce::Font(juce::FontOptions(8.5f, juce::Font::bold)));
    g.setColour(juce::Colour::fromRGB(175, 185, 200));
    g.drawText("PAD LEVEL", 676, 422, 154, 12, juce::Justification::centred);

    // LED dots for LNG REL & NO SENS
    g.setColour(juce::Colour::fromRGB(229, 57, 53));
    g.fillEllipse(686.0f, 442.0f, 5.0f, 5.0f);
    g.fillEllipse(762.0f, 442.0f, 5.0f, 5.0f);
    g.setFont(juce::Font(juce::FontOptions(8.0f, juce::Font::bold)));
    g.setColour(juce::Colour::fromRGB(215, 225, 240));
    g.drawText("LNG REL", 694, 438, 54, 14, juce::Justification::left);
    g.drawText("NO SENS", 770, 438, 54, 14, juce::Justification::left);

    // FEATURES & KEY RANGE (x: 662, y: 528, w: 182, h: 188)
    drawPanelCard(g, {662, 528, 182, 188}, "");
    g.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::bold)));
    g.setColour(juce::Colour::fromRGB(170, 180, 195));
    g.drawText("FEATURES", 662, 536, 182, 12, juce::Justification::centred);

    g.drawText("KEY RANGE", 662, 630, 182, 12, juce::Justification::centred);

    // Split Note Display Box (C7)
    g.setColour(juce::Colour::fromRGB(14, 16, 22));
    g.fillRoundedRectangle(794.0f, 656.0f, 36.0f, 24.0f, 3.0f);
    g.setColour(juce::Colour::fromRGB(45, 52, 65));
    g.drawRoundedRectangle(794.0f, 656.0f, 36.0f, 24.0f, 3.0f, 1.0f);
    g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
    g.setColour(juce::Colour::fromRGB(229, 57, 53));
    g.drawText("C7", 794, 656, 36, 24, juce::Justification::centred);

    // COLUMN 5: CHORUS, TREMOLO & REVERB (x: 850, w: 156)
    // CHORUS Module (top)
    drawPanelCard(g, {850, 100, 156, 196}, "CHORUS", true);
    // Chorus meter
    drawVerticalVuMeter(g, 862.0f, 146.0f, 6.0f, 58.0f, 0.55f);
    drawKnobGradTicks(g, 893.0f, 176.0f, 16.0f, -2.4f, 2.4f, 7, false);
    drawKnobGradTicks(g, 929.0f, 176.0f, 16.0f, -2.4f, 2.4f, 7, false);
    drawKnobGradTicks(g, 965.0f, 176.0f, 16.0f, -2.4f, 2.4f, 7, false);

    g.setFont(juce::Font(juce::FontOptions(8.5f, juce::Font::bold)));
    g.setColour(juce::Colour::fromRGB(180, 190, 205));
    g.drawText("DPTH", 876, 224, 34, 12, juce::Justification::centred);
    g.drawText("RATE", 912, 224, 34, 12, juce::Justification::centred);
    g.drawText("MIX", 948, 224, 34, 12, juce::Justification::centred);

    // TREMOLO Module (middle)
    drawPanelCard(g, {850, 302, 156, 172}, "TREMOLO", true);
    drawKnobGradTicks(g, 928.0f, 372.0f, 24.0f, -2.4f, 2.4f, 9, false);
    g.setFont(juce::Font(juce::FontOptions(8.5f, juce::Font::bold)));
    g.setColour(juce::Colour::fromRGB(180, 190, 205));
    g.drawText("RATE", 850, 420, 156, 12, juce::Justification::centred);

    // REVERB Module (bottom)
    drawPanelCard(g, {850, 480, 156, 236}, "REVERB", true);
    drawKnobGradTicks(g, 928.0f, 550.0f, 24.0f, -2.4f, 2.4f, 9, false);
    g.setFont(juce::Font(juce::FontOptions(8.5f, juce::Font::bold)));
    g.setColour(juce::Colour::fromRGB(180, 190, 205));
    g.drawText("AMOUNT", 850, 598, 156, 12, juce::Justification::centred);

    // REV. SIZE Label & 3 dots (Room, Stage, Hall)
    g.setColour(juce::Colour::fromRGB(229, 57, 53));
    g.fillEllipse(876.0f, 680.0f, 5.0f, 5.0f);
    g.fillEllipse(886.0f, 680.0f, 5.0f, 5.0f);
    g.fillEllipse(896.0f, 680.0f, 5.0f, 5.0f);
    g.setFont(juce::Font(juce::FontOptions(8.5f, juce::Font::bold)));
    g.setColour(juce::Colour::fromRGB(190, 200, 215));
    g.drawText("REV. SIZE", 906, 676, 70, 14, juce::Justification::left);
}
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
    // 1. Top Bar Controls (Kontakt-style header)
    btnQuickPrev.setBounds(46, 13, 18, 18);
    btnQuickSelect.setBounds(68, 12, 216, 20);
    btnQuickNext.setBounds(288, 13, 18, 18);
    sliderTune.setBounds(736, 24, 40, 20);

    //==========================================================================
    // COLUMN 1: EQ PIANO & DELAY (x: 18, w: 156)
    //==========================================================================
    // EQ Knobs (Treble, Mid, Bass)
    sliderTreble.setBounds(70, 124, 52, 48);
    sliderMid.setBounds(70, 204, 52, 48);
    sliderBass.setBounds(70, 284, 52, 48);

    // DELAY Knobs (Feed, Time, Mix) & Tap Delay
    sliderDelayFeedback.setBounds(44, 494, 38, 44);
    sliderDelayTime.setBounds(80, 494, 38, 44);
    sliderTremoloDepth.setBounds(116, 494, 38, 44); // Mix
    btnTapTempo.setBounds(38, 620, 116, 30);

    //==========================================================================
    // COLUMN 2: PIANO ENGINE & COMP (x: 180, w: 182)
    //==========================================================================
    btnPianoSelect.setBounds(198, 190, 146, 26);
    sliderPianoVol.setBounds(228, 224, 82, 192);

    // Push buttons (String Res & Open String)
    btnStringRes.setBounds(198, 458, 70, 26);
    btnPedalNoise.setBounds(274, 458, 70, 26);

    // Comp Knob & Button
    sliderCompressor.setBounds(242, 570, 58, 54);
    btnCompressorOn.setBounds(242, 672, 60, 24);

    //==========================================================================
    // COLUMN 3: MASTER OLED & MULTI-PROGRAM (x: 368, w: 288)
    //==========================================================================
    btnTransDown.setBounds(378, 290, 18, 22);
    btnTransUp.setBounds(436, 290, 18, 22);

    btnOctaveDown.setBounds(572, 290, 18, 22);
    btnOctaveUp.setBounds(630, 290, 18, 22);

    // Red Square Buttons (STORE & TOUCH)
    btnStore.setBounds(378, 322, 76, 28);
    btnTouch1.setBounds(572, 322, 76, 28);

    // Big Center PART MIX Knob
    sliderPartMix.setBounds(472, 288, 80, 76);

    // Programs Matrix: Row 1 (1-4) & Row 2 (LOAD, 5-8, SAVE)
    for (int p = 0; p < 4; ++p) {
        btnPrograms[size_t(p)].setBounds(424 + p * 48, 488, 44, 34);
        btnPrograms[size_t(p + 4)].setBounds(424 + p * 48, 540, 44, 34);
    }
    btnLoadNord.setBounds(376, 540, 44, 34);
    btnSaveNord.setBounds(616, 540, 44, 34);

    //==========================================================================
    // COLUMN 4: SYNTH ENGINE & FEATURES / KEY RANGE (x: 662, w: 182)
    //==========================================================================
    btnPadSelect.setBounds(680, 190, 146, 26);
    sliderPadVol.setBounds(710, 224, 82, 192);

    // Synth buttons (Long Release & No Sens)
    btnTimbreSoft.setBounds(680, 458, 70, 26); // LNG REL
    btnTimbreMid.setBounds(756, 458, 70, 26);  // NO SENS

    // Features buttons
    btnTimbreRaw.setBounds(682, 560, 68, 24);    // NO PDL
    btnTimbreBright.setBounds(756, 560, 68, 24); // DOUBLE

    // Key Range Learn button
    btnTouch2.setBounds(682, 656, 68, 24);       // LEARN

    //==========================================================================
    // COLUMN 5: CHORUS, TREMOLO & REVERB (x: 850, w: 156)
    //==========================================================================
    // CHORUS Knobs (Dpth, Rate, Mix)
    sliderChorusDepth.setBounds(874, 154, 38, 44);
    sliderPadRes.setBounds(910, 154, 38, 44);       // Chorus Rate
    sliderPadAttack.setBounds(946, 154, 38, 44);    // Chorus Mix

    // TREMOLO Knob & Button
    sliderPadRelease.setBounds(900, 346, 56, 52);   // Tremolo Rate
    btnDelayOn.setBounds(880, 440, 96, 24);         // Tremolo Intensity

    // REVERB Knob & Button
    sliderReverbAmount.setBounds(900, 524, 56, 52); // Reverb Amount
    btnReverbType.setBounds(876, 626, 104, 26);     // REV. SIZE

    if (midiMapOverlay != nullptr) {
        midiMapOverlay->setBounds(getLocalBounds());
    }
}
