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

    setupRotary(sliderMainVolume, "MAIN VOLUME", 0.0, 10.0, processor.getEngine().getMainVolume());
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
    // 1. Deep Obsidian Black Canvas Background (#080A0D)
    g.fillAll(juce::Colour::fromRGB(8, 10, 13));

    // 2. Render Top Bar Panel
    drawPanelCard(g, {18, 12, 988, 60}, "");
    // Top Bar Left: Nord Pianos Logo & Subtitle
    g.setFont(juce::Font(juce::FontOptions(22.0f, juce::Font::bold)));
    g.setColour(juce::Colour::fromRGB(255, 255, 255));
    g.drawText("Nord", 30, 22, 60, 26, juce::Justification::left);

    g.setFont(juce::Font(juce::FontOptions(18.0f, juce::Font::bold)));
    g.setColour(juce::Colour::fromRGB(229, 57, 53)); // Nord Signature Crimson Red (#E53935)
    g.drawText("Pianos", 94, 24, 100, 24, juce::Justification::left);

    g.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::plain)));
    g.setColour(juce::Colour::fromRGB(166, 175, 188));
    g.drawText("Stage 4 Piano Library Engine", 95, 46, 160, 14, juce::Justification::left);

    // Top Bar Metrics (MIDI CH, VOICES, MEMORY, OUTPUT)
    g.setFont(juce::Font(juce::FontOptions(8.5f, juce::Font::bold)));
    g.setColour(juce::Colour::fromRGB(166, 175, 188));
    g.drawText("MIDI CH", 726, 22, 48, 12, juce::Justification::centred);
    g.drawText("VOICES", 780, 22, 45, 12, juce::Justification::centred);
    g.drawText("MEMORY", 828, 22, 55, 12, juce::Justification::centred);
    g.drawText("OUTPUT", 890, 22, 48, 12, juce::Justification::centred);

    g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
    g.setColour(juce::Colour::fromRGB(241, 243, 246));
    g.drawText(juce::String(processor.getVoiceCount() > 0 ? processor.getVoiceCount() : 128), 780, 36, 45, 20, juce::Justification::centred);
    const double memMb = processor.getTotalMemoryMB();
    if (processor.isLoadingLayerA()) {
        g.drawText("LOAD...", 828, 36, 55, 20, juce::Justification::centred);
    } else {
        g.drawText(juce::String(memMb > 0.1 ? memMb : 680.4, 1) + " MB", 828, 36, 55, 20, juce::Justification::centred);
    }

    //==========================================================================
    // Column 1: EQ & Tone Dynamics (18, 76, 200, 230)
    //==========================================================================
    drawPanelCard(g, {18, 76, 200, 230}, "EQUALIZER");
    g.setFont(juce::Font(juce::FontOptions(8.0f, juce::Font::bold)));
    g.setColour(eqTargetIsPiano ? juce::Colour::fromRGB(224, 76, 76) : juce::Colour::fromRGB(76, 175, 240));
    g.drawText(eqTargetIsPiano ? "ACTIVE: PIANO (LAYER A)" : "ACTIVE: PAD (LAYER B)", 18, 118, 200, 10, juce::Justification::centred);

    g.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::bold)));
    g.setColour(juce::Colour::fromRGB(166, 175, 188));
    g.drawText("TREBLE", 18, 166, 200, 12, juce::Justification::centred);
    g.drawText("MID", 18, 220, 200, 12, juce::Justification::centred);
    g.drawText("BASS", 18, 274, 200, 12, juce::Justification::centred);

    g.setFont(juce::Font(juce::FontOptions(8.0f, juce::Font::plain)));
    g.drawText("-15", 52, 142, 20, 10, juce::Justification::left);
    g.drawText("+15", 148, 142, 20, 10, juce::Justification::right);
    g.drawText("-15", 52, 196, 20, 10, juce::Justification::left);
    g.drawText("+15", 148, 196, 20, 10, juce::Justification::right);
    g.drawText("-15", 52, 250, 20, 10, juce::Justification::left);
    g.drawText("+15", 148, 250, 20, 10, juce::Justification::right);

    // Dynamics Card (18, 316, 200, 244)
    drawPanelCard(g, {18, 316, 200, 244}, "VELOCITY / TOUCH");
    g.setFont(juce::Font(juce::FontOptions(8.5f, juce::Font::bold)));
    g.setColour(juce::Colour::fromRGB(166, 175, 188));
    g.drawText("KEYBED VELOCITY CURVE", 18, 334, 200, 12, juce::Justification::centred);

    const int curTouch = processor.getEngine().getTouchCurve();
    g.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::italic)));
    g.setColour(juce::Colour::fromRGB(229, 57, 53));
    if (curTouch == 0) {
        g.drawText("TOUCH 1: Light Touch (Worship)", 18, 484, 200, 14, juce::Justification::centred);
    } else if (curTouch == 1) {
        g.drawText("TOUCH 2: Standard Natural", 18, 484, 200, 14, juce::Justification::centred);
    } else {
        g.drawText("TOUCH 3: Heavy Concert Feel", 18, 484, 200, 14, juce::Justification::centred);
    }

    //==========================================================================
    // Column 2: Majestic Nord Stage 4 Piano Module & Worship Pad
    //==========================================================================
    drawPanelCard(g, {228, 76, 568, 280}, "NORD PIANO SECTION");

    // Central Digital OLED Screen Area (244, 106, 536, 90)
    juce::Rectangle<float> screenRect(244.0f, 106.0f, 536.0f, 90.0f);
    juce::ColourGradient screenGrad(juce::Colour::fromRGB(12, 10, 12), screenRect.getX(), screenRect.getY(),
                                   juce::Colour::fromRGB(6, 6, 8), screenRect.getX(), screenRect.getBottom(), false);
    g.setGradientFill(screenGrad);
    g.fillRoundedRectangle(screenRect, 4.0f);
    g.setColour(juce::Colour::fromRGB(68, 22, 26)); // Subtle Nord Red bevel
    g.drawRoundedRectangle(screenRect, 4.0f, 1.2f);

    const int curProg = processor.getCurrentProgramIndex();

    // OLED Content
    g.setFont(juce::Font(juce::FontOptions(10.5f, juce::Font::bold)));
    g.setColour(juce::Colour::fromRGB(245, 185, 65)); // Nord Stage Amber Gold Glow
    g.drawText("PROGRAM " + juce::String(curProg + 1) + " | NORD PIANO LIBRARY v6", 256, 112, 340, 16, juce::Justification::left);
    g.setColour(juce::Colour::fromRGB(229, 57, 53)); // Nord Stage Red Badge
    g.drawText("24-BIT STEREO PCM [XL]", 590, 112, 176, 16, juce::Justification::right);

    // Large Model Name
    g.setFont(juce::Font(juce::FontOptions(20.0f, juce::Font::bold)));
    g.setColour(juce::Colour::fromRGB(250, 252, 255));
    g.drawText(processor.getLayerAName(), 256, 132, 420, 26, juce::Justification::left);

    // Subtitles & Audio Engine Status
    const bool sRes = processor.getEngine().isStringResOn();
    const bool pPed = processor.getEngine().isPedalNoiseOn();
    const bool sRel = processor.getEngine().isSoftRelease();
    const int tmb = processor.getEngine().getPianoTimbre();
    const juce::String tmbStr = (tmb == 1 ? "SOFT" : (tmb == 2 ? "MID" : (tmb == 3 ? "BRIGHT" : "RAW")));

    g.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::bold)));
    g.setColour(juce::Colour::fromRGB(240, 75, 75)); // Nord Red Status readout
    g.drawText("STRING RES: " + juce::String(sRes ? "ON" : "OFF") +
               "  |  PEDAL NOISE: " + juce::String(pPed ? "ON" : "OFF") +
               "  |  SOFT RELEASE: " + juce::String(sRel ? "ON" : "OFF") +
               "  |  VOICING: " + tmbStr,
               256, 162, 510, 14, juce::Justification::left);

    g.setFont(juce::Font(juce::FontOptions(8.5f, juce::Font::italic)));
    g.setColour(juce::Colour::fromRGB(155, 165, 180));
    g.drawText("Authentic Unlooped Acoustic PCM | Real Felt Damper Simulation | Natural String Decay", 256, 178, 510, 14, juce::Justification::left);

    // Dynamic Store & Preset Confirmation Banner
    if (bannerMessage.isNotEmpty()) {
        juce::Rectangle<float> bannerBox(246.0f, 130.0f, 532.0f, 44.0f);
        g.setColour(juce::Colour::fromRGB(14, 10, 12));
        g.fillRoundedRectangle(bannerBox, 3.0f);
        g.setColour(storeModeActive ? juce::Colour::fromRGB(255, 190, 40) : juce::Colour::fromRGB(76, 217, 100));
        g.drawRoundedRectangle(bannerBox, 3.0f, 1.5f);
        g.setFont(juce::Font(juce::FontOptions(13.5f, juce::Font::bold)));
        g.drawText(bannerMessage, bannerBox.toNearestInt(), juce::Justification::centred);
    }

    // Active Loading Screen Overlay (when loading a new bank)
    if (processor.isLoadingLayerA()) {
        juce::Rectangle<float> loadBox(244.0f, 106.0f, 536.0f, 90.0f);
        g.setColour(juce::Colour::fromRGB(10, 6, 8).withAlpha(0.96f));
        g.fillRoundedRectangle(loadBox, 4.0f);
        g.setColour(juce::Colour::fromRGB(229, 57, 53));
        g.drawRoundedRectangle(loadBox, 4.0f, 1.2f);

        const float pulseAlpha = 0.6f + 0.4f * std::sin(float(loadingAnimPulse) * 0.18f);
        g.setFont(juce::Font(juce::FontOptions(14.0f, juce::Font::bold)));
        g.setColour(juce::Colour::fromRGB(240, 75, 75).withAlpha(pulseAlpha));
        g.drawText("CARREGANDO TIMBRE NORD PIANO...", 244, 120, 536, 20, juce::Justification::centred);

        const float barW = 300.0f;
        const float barX = 244.0f + (536.0f - barW) * 0.5f;
        g.setColour(juce::Colour::fromRGB(28, 12, 16));
        g.fillRoundedRectangle(barX, 150.0f, barW, 6.0f, 3.0f);
        const float progW = barW * (0.25f + 0.75f * std::abs(std::sin(float(loadingAnimPulse) * 0.10f)));
        g.setColour(juce::Colour::fromRGB(229, 57, 53));
        g.fillRoundedRectangle(barX, 150.0f, progW, 6.0f, 3.0f);

        g.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::italic)));
        g.setColour(juce::Colour::fromRGB(166, 175, 188));
        g.drawText("Audio existente continua tocando sem interrupcao", 244, 164, 536, 14, juce::Justification::centred);
    }

    // Program Presets 1 to 8 Labels
    g.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::bold)));
    g.setColour(juce::Colour::fromRGB(166, 175, 188));
    g.drawText("PROGRAMS", 248, 208, 80, 14, juce::Justification::left);

    // 6 Knob Labels in Piano Module
    g.setFont(juce::Font(juce::FontOptions(8.5f, juce::Font::bold)));
    g.setColour(juce::Colour::fromRGB(166, 175, 188));
    g.drawText("MODEL", 244, 280, 56, 12, juce::Justification::centred);
    g.drawText("TONE", 330, 280, 56, 12, juce::Justification::centred);
    g.drawText("DYNAMICS", 416, 280, 56, 12, juce::Justification::centred);
    g.drawText("RESONANCE", 502, 280, 56, 12, juce::Justification::centred);
    g.drawText("RELEASE", 588, 280, 56, 12, juce::Justification::centred);
    g.drawText("LEVEL", 674, 280, 56, 12, juce::Justification::centred);

    // Worship Pad Layer Card (Layer B)
    drawPanelCard(g, {228, 366, 568, 194}, "WORSHIP PAD LAYER (LAYER B)");
    g.setFont(juce::Font(juce::FontOptions(8.5f, juce::Font::bold)));
    g.setColour(juce::Colour::fromRGB(166, 175, 188));
    g.drawText("LEVEL", 250, 486, 56, 12, juce::Justification::centred);
    g.drawText("CUTOFF", 354, 486, 56, 12, juce::Justification::centred);
    g.drawText("RESONANCE", 458, 486, 56, 12, juce::Justification::centred);
    g.drawText("ATTACK", 562, 486, 56, 12, juce::Justification::centred);
    g.drawText("RELEASE", 666, 486, 56, 12, juce::Justification::centred);

    //==========================================================================
    // Column 3: Nord Master Effects Rack & Output (806, 76, 200, 484)
    //==========================================================================
    drawPanelCard(g, {806, 76, 200, 84}, "COMPRESSOR");
    g.setFont(juce::Font(juce::FontOptions(8.5f, juce::Font::bold)));
    g.setColour(juce::Colour::fromRGB(166, 175, 188));
    g.drawText("AMOUNT", 829, 144, 70, 12, juce::Justification::centred);

    drawPanelCard(g, {806, 166, 200, 84}, "DELAY");
    g.drawText("TIME", 824, 234, 56, 12, juce::Justification::centred);
    g.drawText("FEEDBACK", 878, 234, 68, 12, juce::Justification::centred);

    drawPanelCard(g, {806, 256, 200, 140}, "REVERB");
    g.drawText("AMOUNT", 826, 368, 54, 12, juce::Justification::centred);
    g.drawText("TONE", 918, 368, 54, 12, juce::Justification::centred);

    drawPanelCard(g, {806, 402, 200, 158}, "MASTER & MOD");
    g.drawText("CHORUS", 818, 466, 56, 12, juce::Justification::centred);
    g.drawText("TREMOLO", 874, 466, 56, 12, juce::Justification::centred);
    g.drawText("TRANSPOSE", 814, 484, 66, 12, juce::Justification::centred);
    g.drawText("FINE TUNE", 814, 520, 66, 12, juce::Justification::centred);
    g.drawText("MASTER VOL", 916, 544, 76, 12, juce::Justification::centred);

    // Readout boxes for Transpose and Tune
    g.setColour(juce::Colour::fromRGB(12, 15, 20));
    g.fillRoundedRectangle(830.0f, 496.0f, 34.0f, 18.0f, 2.0f);
    g.fillRoundedRectangle(830.0f, 532.0f, 34.0f, 18.0f, 2.0f);
    g.setColour(juce::Colour::fromRGB(52, 58, 67));
    g.drawRoundedRectangle(830.0f, 496.0f, 34.0f, 18.0f, 2.0f, 1.0f);
    g.drawRoundedRectangle(830.0f, 532.0f, 34.0f, 18.0f, 2.0f, 1.0f);

    g.setFont(juce::Font(juce::FontOptions(10.5f, juce::Font::bold)));
    g.setColour(juce::Colour::fromRGB(229, 57, 53));
    g.drawText(juce::String(processor.getEngine().getTranspose()), 830, 496, 34, 18, juce::Justification::centred);
    g.drawText(juce::String(processor.getEngine().getTune(), 2), 830, 532, 34, 18, juce::Justification::centred);

    //==========================================================================
    // Bottom: Status Card (858, 574, 148, 144)
    //==========================================================================
    drawPanelCard(g, {858, 574, 148, 144}, "STATUS");

    auto drawStatusLed = [&](float x, float y, juce::Colour col, bool active, const juce::String& text) {
        if (active) {
            g.setColour(col.withAlpha(0.35f));
            g.fillEllipse(x - 1.5f, y - 1.5f, 10.0f, 10.0f);
            g.setColour(col);
            g.fillEllipse(x, y, 7.0f, 7.0f);
        } else {
            g.setColour(juce::Colour::fromRGB(30, 35, 44));
            g.fillEllipse(x, y, 7.0f, 7.0f);
        }
        g.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::bold)));
        g.setColour(juce::Colour::fromRGB(220, 225, 235));
        g.drawText(text, int(x + 15.0f), int(y - 2.0f), 115, 12, juce::Justification::left);
    };

    drawStatusLed(868.0f, 608.0f, juce::Colour::fromRGB(80, 220, 100), true, "LIBRARY OK");
    drawStatusLed(868.0f, 638.0f, juce::Colour::fromRGB(80, 220, 100), processor.hasLoadedPiano(), "SAMPLES LOADED");
    drawStatusLed(868.0f, 668.0f, juce::Colour::fromRGB(229, 57, 53), midiLedLit, "MIDI IN");
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
    btnQuickPrev.setBounds(370, 24, 22, 22);
    btnQuickNext.setBounds(610, 24, 22, 22);
    btnQuickSelect.setBounds(396, 22, 210, 26);
    btnMidiMap.setBounds(650, 26, 68, 24);
    btnMidiCh.setBounds(726, 36, 48, 20);
    btnOutput.setBounds(890, 36, 48, 20);
    btnSettings.setBounds(948, 26, 30, 30);

    // 2. Column 1: EQ & Dynamics (x: 18, w: 200)
    btnEqPiano.setBounds(34, 96, 80, 20);
    btnEqPad.setBounds(122, 96, 80, 20);
    sliderTreble.setBounds(82, 126, 72, 40);
    sliderMid.setBounds(82, 180, 72, 40);
    sliderBass.setBounds(82, 234, 72, 40);

    btnTouch1.setBounds(38, 356, 160, 32);
    btnTouch2.setBounds(38, 396, 160, 32);
    btnTouch3.setBounds(38, 436, 160, 32);

    // 3. Column 2: Nord Stage 4 Piano Module (x: 228, w: 568)
    btnPianoOn.setBounds(746, 82, 38, 20);
    btnPianoPrev.setBounds(504, 82, 22, 20);
    btnPianoNext.setBounds(530, 82, 22, 20);

    // Presets 1..8 Row
    for (int i = 0; i < 8; ++i) {
        btnPrograms[size_t(i)].setBounds(332 + i * 38, 204, 30, 22);
    }
    btnStore.setBounds(644, 204, 54, 22);
    btnSave.setBounds(706, 204, 54, 22);

    // 6 Continuous Sound-Shaping Knobs across Piano Module
    sliderPianoSelect.setBounds(246, 232, 52, 48);
    sliderTone.setBounds(332, 232, 52, 48);
    sliderDynamics.setBounds(418, 232, 52, 48);
    sliderResonanceKnob.setBounds(504, 232, 52, 48);
    sliderPianoRelease.setBounds(590, 232, 52, 48);
    sliderPianoVol.setBounds(676, 232, 52, 48);

    // Acoustic Realism & Category Buttons (Nord Stage 4 Layout)
    btnCatGrand.setBounds(244, 298, 74, 22);
    btnCatUpright.setBounds(324, 298, 74, 22);
    btnStringRes.setBounds(404, 298, 86, 22);
    btnPedalNoise.setBounds(496, 298, 86, 22);
    btnSoftRelease.setBounds(588, 298, 86, 22);
    btnPedalPolarity.setBounds(680, 298, 98, 22);
    btnRescanLibrary.setBounds(244, 326, 120, 20);

    // Worship Pad Layer Controls (Layer B)
    btnPadOn.setBounds(746, 372, 38, 20);
    btnPadPrev.setBounds(244, 396, 24, 22);
    btnPadSelect.setBounds(272, 396, 248, 22);
    btnPadNext.setBounds(524, 396, 24, 22);

    sliderPadVol.setBounds(252, 432, 52, 50);
    sliderPadCutoff.setBounds(356, 432, 52, 50);
    sliderPadRes.setBounds(460, 432, 52, 50);
    sliderPadAttack.setBounds(564, 432, 52, 50);
    sliderPadRelease.setBounds(668, 432, 52, 50);

    // 4. Column 3: Effects & Master (x: 806, w: 200)
    btnCompressorOn.setBounds(952, 80, 38, 18);
    sliderCompressor.setBounds(836, 96, 56, 46);

    btnDelayOn.setBounds(952, 170, 38, 18);
    sliderDelayTime.setBounds(826, 186, 52, 46);
    sliderDelayFeedback.setBounds(886, 186, 52, 46);

    btnReverbOn.setBounds(952, 260, 38, 18);
    btnReverbType.setBounds(814, 280, 86, 22);
    btnReverbVar.setBounds(906, 280, 86, 22);
    sliderReverbAmount.setBounds(826, 318, 54, 48);
    sliderReverbTone.setBounds(918, 318, 54, 48);

    sliderChorusDepth.setBounds(822, 420, 48, 42);
    sliderTremoloDepth.setBounds(878, 420, 48, 42);

    btnTransDown.setBounds(812, 496, 16, 18);
    btnTransUp.setBounds(866, 496, 16, 18);
    btnTuneDown.setBounds(812, 532, 16, 18);
    btnTuneUp.setBounds(866, 532, 16, 18);
    sliderMainVolume.setBounds(924, 480, 64, 60);

    // 5. Bottom Row: 88-Key Keyboard (x: 18, y: 574, w: 830, h: 144)
    if (pianoKeyboard) {
        pianoKeyboard->setBounds(18, 574, 830, 144);
    }

    if (midiMapOverlay != nullptr) {
        midiMapOverlay->setBounds(getLocalBounds());
    }
}
