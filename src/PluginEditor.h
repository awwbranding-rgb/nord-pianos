#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "FaderCapImage.h"

//==============================================================================
// Black Edition Custom LookAndFeel (Zero Red, Ice-Blue #65BFFF Accents)
//==============================================================================
// Black Edition Custom LookAndFeel (OmniStage Flagship Console)
//==============================================================================
//==============================================================================
// Black Edition Custom LookAndFeel (OmniStage Flagship Console)
//==============================================================================
class BlackKnobLookAndFeel : public juce::LookAndFeel_V4 {
public:
    BlackKnobLookAndFeel() {
        setColour(juce::Slider::rotarySliderFillColourId, juce::Colour::fromRGB(229, 57, 53)); // #E53935
        setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour::fromRGB(25, 29, 36));
        setColour(juce::Slider::thumbColourId, juce::Colour::fromRGB(229, 57, 53));

        // Load 3D Brushed Aluminum Fader Cap Image from memory
        juce::MemoryInputStream mis(faderCapPngData, faderCapPngSize, false);
        juce::PNGImageFormat pngFormat;
        faderCapImage = pngFormat.decodeImage(mis);
    }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                          juce::Slider& slider) override {
        // When not dragged and at default, let the photorealistic 3D chassis knob show!
        if (!slider.isMouseOverOrDragging()) return;

        const float radius = juce::jmin(width / 2.0f, height / 2.0f) - 2.0f;
        const float centreX = x + width * 0.5f;
        const float centreY = y + height * 0.5f;
        const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

        // Highlight ring
        g.setColour(juce::Colour::fromRGB(255, 45, 55).withAlpha(0.9f));
        juce::Path arc;
        arc.addCentredArc(centreX, centreY, radius + 1.0f, radius + 1.0f, 0.0f,
                          rotaryStartAngle, angle, true);
        g.strokePath(arc, juce::PathStrokeType(2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Rotating white indicator needle
        juce::Path needle;
        needle.addRectangle(-1.25f, -radius * 0.85f, 2.5f, radius * 0.50f);
        needle.applyTransform(juce::AffineTransform::rotation(angle).translated(centreX, centreY));
        g.setColour(juce::Colour::fromRGB(255, 255, 255));
        g.fillPath(needle);
    }

    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float minSliderPos, float maxSliderPos,
                          const juce::Slider::SliderStyle style, juce::Slider& slider) override {
        juce::ignoreUnused(minSliderPos, maxSliderPos, style, slider);
        const float cx = x + width * 0.5f;
        const float capW = 34.0f;
        const float capH = 44.0f;
        const float capX = cx - capW * 0.5f;
        const float capY = sliderPos - capH * 0.5f;

        // 3D Drop Shadow
        g.setColour(juce::Colour::fromRGB(0, 0, 0).withAlpha(0.75f));
        g.fillRoundedRectangle(capX + 1.0f, capY + 3.0f, capW, capH, 3.5f);

        // 3D Brushed Aluminum Fader Cap
        if (faderCapImage.isValid()) {
            g.drawImage(faderCapImage,
                        int(capX), int(capY), int(capW), int(capH),
                        0, 0, faderCapImage.getWidth(), faderCapImage.getHeight());
        } else {
            juce::ColourGradient capGrad(juce::Colour::fromRGB(230, 235, 245), capX, capY,
                                         juce::Colour::fromRGB(80, 88, 102), capX, capY + capH, false);
            g.setGradientFill(capGrad);
            g.fillRoundedRectangle(capX, capY, capW, capH, 3.0f);
        }
    }

    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                              const juce::Colour& backgroundColour,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override {
        juce::ignoreUnused(backgroundColour);
        auto bounds = button.getLocalBounds().toFloat();
        const bool isToggled = button.getToggleState();
        const auto btnText = button.getButtonText();

        // Slide Switches
        if (btnText.isEmpty() && button.getWidth() <= 42) {
            return; // Pure photoreal hardware switches from chassis
        }

        // The background plate pre-renders default active states in glorious 3D ruby red:
        // (tabPiano, btnOnA, btnPrograms[0], btnCatGrand, btnFxChorus)
        const bool isDefaultActive = (btnText == "A  Piano" ||
                                      (btnText == "ON" && button.getX() < 100) ||
                                      btnText == "1" ||
                                      btnText == "Piano" ||
                                      btnText == "GRAND" ||
                                      btnText == "Chorus");

        // If it's a default active button and toggled, let the chassis 3D ruby red show!
        if (isDefaultActive && isToggled && !shouldDrawButtonAsDown) {
            return;
        }

        // If it's a default active button that was TURNED OFF, draw dark charcoal over it:
        if (isDefaultActive && !isToggled) {
            juce::ColourGradient offGrad(juce::Colour::fromRGB(45, 52, 65), bounds.getX(), bounds.getY(),
                                         juce::Colour::fromRGB(22, 26, 34), bounds.getX(), bounds.getBottom(), false);
            g.setGradientFill(offGrad);
            g.fillRoundedRectangle(bounds, 4.0f);
            g.setColour(juce::Colour::fromRGB(60, 70, 85));
            g.drawRoundedRectangle(bounds, 4.0f, 1.0f);
            return;
        }

        // When a non-default button is toggled ON, or when clicked down:
        if (isToggled || shouldDrawButtonAsDown) {
            auto shadowBounds = bounds.translated(0.0f, 2.0f);
            g.setColour(juce::Colour::fromRGB(0, 0, 0).withAlpha(0.55f));
            g.fillRoundedRectangle(shadowBounds, 4.0f);

            juce::ColourGradient redGrad(juce::Colour::fromRGB(240, 45, 55), bounds.getX(), bounds.getY(),
                                         juce::Colour::fromRGB(150, 16, 24), bounds.getX(), bounds.getBottom(), false);
            g.setGradientFill(redGrad);
            g.fillRoundedRectangle(bounds, 4.0f);

            g.setColour(juce::Colour::fromRGB(255, 200, 205).withAlpha(0.65f));
            g.drawHorizontalLine(int(bounds.getY() + 1.0f), bounds.getX() + 3.0f, bounds.getRight() - 3.0f);

            g.setColour(juce::Colour::fromRGB(255, 60, 70).withAlpha(0.85f));
            g.drawRoundedRectangle(bounds, 4.0f, 1.5f);
        }
    }

    void drawButtonText(juce::Graphics& g, juce::TextButton& button,
                        bool /*shouldDrawButtonAsHighlighted*/, bool /*shouldDrawButtonAsDown*/) override {
        const auto btnText = button.getButtonText();
        const bool isDefaultActive = (btnText == "A  Piano" ||
                                      (btnText == "ON" && button.getX() < 100) ||
                                      btnText == "1" ||
                                      btnText == "Piano" ||
                                      btnText == "GRAND" ||
                                      btnText == "Chorus");

        // Only draw text if it's a non-default button that became active
        if (button.getToggleState() && !isDefaultActive) {
            g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
            g.setColour(juce::Colour::fromRGB(255, 255, 255));
            g.drawText(btnText, button.getLocalBounds(), juce::Justification::centred, true);
        }
    }

private:
    juce::Image faderCapImage;
};

class InteractivePianoKeyboard final : public juce::Component {
public:
    explicit InteractivePianoKeyboard(NordPianoPrototypeAudioProcessor& proc)
        : processor(proc) {
        setOpaque(true);
    }

    void paint(juce::Graphics& g) override {
        const float w = float(getWidth());
        const float h = float(getHeight());
        if (w <= 10.0f || h <= 10.0f) return;

        // Background dark frame
        g.fillAll(juce::Colour::fromRGB(8, 10, 13));

        constexpr int numWhiteKeys = 52;
        const float whiteW = w / float(numWhiteKeys);
        const float blackW = whiteW * 0.62f;
        const float blackH = (h - 16.0f) * 0.62f;

        const int lowKeyA = processor.getEngine().getLayerAConfig().keyLow.load(std::memory_order_relaxed);
        const int highKeyA = processor.getEngine().getLayerAConfig().keyHigh.load(std::memory_order_relaxed);

        // 1. Draw 52 White Keys (A0 = MIDI 21 to C8 = MIDI 108)
        int whiteIdx = 0;
        for (int note = 21; note <= 108; ++note) {
            if (!isBlackKey(note)) {
                const float kx = float(whiteIdx) * whiteW;
                const float kw = (whiteIdx == numWhiteKeys - 1) ? (w - kx) : whiteW;
                const juce::Rectangle<float> kRect(kx, 3.0f, kw, h - 19.0f);

                const bool isDown = processor.getKeyboardState().isNoteOnForChannels(0xffff, note) || (activeMouseNote == note);
                const bool inRange = (note >= lowKeyA && note <= highKeyA);

                if (isDown) {
                    // Active Pressed Key: Vivid Nord Crimson Red Illumination
                    juce::ColourGradient downGrad(juce::Colour::fromRGB(235, 55, 55), kx, 3.0f,
                                                 juce::Colour::fromRGB(150, 18, 22), kx, h - 19.0f, false);
                    g.setGradientFill(downGrad);
                    g.fillRect(kRect);

                    g.setColour(juce::Colour::fromRGB(255, 140, 140));
                    g.fillRect(kx, h - 22.0f, kw, 3.0f);
                } else if (inRange) {
                    // Active Key Range Zone: Subtle warm ivory tint
                    juce::ColourGradient rangeGrad(juce::Colour::fromRGB(248, 246, 248), kx, 3.0f,
                                                   juce::Colour::fromRGB(222, 220, 224), kx, h - 24.0f, false);
                    g.setGradientFill(rangeGrad);
                    g.fillRect(kRect);

                    g.setColour(juce::Colour::fromRGB(229, 57, 53).withAlpha(0.6f)); // Nord Red active zone marker
                    g.fillRect(kx, h - 24.0f, kw, 4.0f);
                } else {
                    // Pristine Ivory Keys
                    juce::ColourGradient ivoryGrad(juce::Colour::fromRGB(246, 247, 250), kx, 3.0f,
                                                   juce::Colour::fromRGB(220, 224, 230), kx, h - 24.0f, false);
                    g.setGradientFill(ivoryGrad);
                    g.fillRect(kRect);

                    g.setColour(juce::Colour::fromRGB(190, 195, 202));
                    g.fillRect(kx, h - 24.0f, kw, 5.0f);
                }

                // Vertical Separation Line
                g.setColour(juce::Colour::fromRGB(30, 34, 41));
                g.drawVerticalLine(int(kx), 3.0f, h - 19.0f);

                // Octave Labels (C-1, C0, C1, C2, C3, C4, C5, C6, C7, C8)
                if (note % 12 == 0) {
                    int octave = (note / 12) - 2;
                    g.setFont(juce::Font(juce::FontOptions(8.5f, juce::Font::bold)));
                    g.setColour(juce::Colour::fromRGB(166, 175, 188));
                    g.drawText("C" + juce::String(octave), int(kx - 4.0f), int(h - 15.0f), int(kw + 8.0f), 12, juce::Justification::centred);
                }

                ++whiteIdx;
            }
        }

        // 2. Draw 36 Black Keys
        whiteIdx = 0;
        for (int note = 21; note <= 108; ++note) {
            if (!isBlackKey(note)) {
                ++whiteIdx;
            } else {
                const float bx = (float(whiteIdx) * whiteW) - (blackW * 0.5f);
                const juce::Rectangle<float> bRect(bx, 3.0f, blackW, blackH);

                const bool isDown = processor.getKeyboardState().isNoteOnForChannels(0xffff, note) || (activeMouseNote == note);

                if (isDown) {
                    // Active Pressed Black Key: Deep Crimson Red Illumination
                    juce::ColourGradient downGrad(juce::Colour::fromRGB(225, 45, 45), bx, 3.0f,
                                                 juce::Colour::fromRGB(130, 15, 20), bx, blackH, false);
                    g.setGradientFill(downGrad);
                    g.fillRect(bRect);
                } else {
                    // Deep Matte Ebony
                    juce::ColourGradient ebonyGrad(juce::Colour::fromRGB(28, 32, 38), bx, 3.0f,
                                                   juce::Colour::fromRGB(10, 12, 16), bx, blackH, false);
                    g.setGradientFill(ebonyGrad);
                    g.fillRect(bRect);

                    // Top highlight
                    g.setColour(juce::Colour::fromRGB(52, 58, 67));
                    g.drawRect(bRect, 0.8f);

                    g.setColour(juce::Colour::fromRGB(255, 255, 255).withAlpha(0.12f));
                    g.drawLine(bx + 1.0f, 4.0f, bx + blackW - 1.0f, 4.0f, 1.0f);
                }
            }
        }

        // Iconic Nord Crimson Red Keyboard Felt Strip
        g.setColour(juce::Colour::fromRGB(190, 26, 30));
        g.fillRect(0.0f, 0.0f, w, 3.0f);
    }

    void mouseDown(const juce::MouseEvent& e) override {
        const int note = getNoteAtPos(e.position);
        if (note >= 21 && note <= 108) {
            activeMouseNote = note;
            processor.getKeyboardState().noteOn(1, note, 0.8f);
            repaint();
        }
    }

    void mouseUp(const juce::MouseEvent&) override {
        if (activeMouseNote >= 0) {
            processor.getKeyboardState().noteOff(1, activeMouseNote, 0.5f);
            activeMouseNote = -1;
            repaint();
        }
    }

private:
    static bool isBlackKey(int note) {
        const int n = note % 12;
        return (n == 1 || n == 3 || n == 6 || n == 8 || n == 10);
    }

    int getNoteAtPos(juce::Point<float> pos) const {
        const float w = float(getWidth());
        const float h = float(getHeight());
        constexpr int numWhiteKeys = 52;
        const float whiteW = w / float(numWhiteKeys);
        const float blackW = whiteW * 0.62f;
        const float blackH = (h - 16.0f) * 0.62f;

        // Check black keys first (they sit on top)
        if (pos.y <= blackH) {
            int whiteIdx = 0;
            for (int note = 21; note <= 108; ++note) {
                if (!isBlackKey(note)) {
                    ++whiteIdx;
                } else {
                    const float bx = (float(whiteIdx) * whiteW) - (blackW * 0.5f);
                    if (pos.x >= bx && pos.x <= bx + blackW) {
                        return note;
                    }
                }
            }
        }

        // Check white keys
        int whiteIdx = int(pos.x / whiteW);
        whiteIdx = juce::jlimit(0, numWhiteKeys - 1, whiteIdx);
        int currentWhite = 0;
        for (int note = 21; note <= 108; ++note) {
            if (!isBlackKey(note)) {
                if (currentWhite == whiteIdx) return note;
                ++currentWhite;
            }
        }
        return -1;
    }

    NordPianoPrototypeAudioProcessor& processor;
    int activeMouseNote = -1;
};

//==============================================================================
// Main Editor Component: NORD PIANOS (Dedicated Stage 4 Architecture)
//==============================================================================
class MidiMapOverlay;

class NordPianoPrototypeAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                                     public juce::KeyListener,
                                                     private juce::Timer {
public:
    friend class MidiMapOverlay;
    explicit NordPianoPrototypeAudioProcessorEditor(NordPianoPrototypeAudioProcessor&);
    ~NordPianoPrototypeAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void paintOverChildren(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    bool keyPressed(const juce::KeyPress& key) override;
    bool keyPressed(const juce::KeyPress& key, juce::Component* originatingComponent) override;

private:
    void updateAllControlsFromEngine();
    bool handleKeyPress(const juce::KeyPress& key);
    void timerCallback() override;

    bool storeModeActive = false;
    juce::String bannerMessage;
    int bannerTimer = 0;

        void drawAllenScrew(juce::Graphics& g, float x, float y);
    void drawPanelCard(juce::Graphics& g, juce::Rectangle<int> bounds, const juce::String& title, bool hasLed = true);
    void setupRotary(juce::Slider& s, const juce::String& text, double minV, double maxV, double defV);
    void setupVerticalFader(juce::Slider& s, const juce::String& text, double minV, double maxV, double defV);
    void setupPillButton(juce::TextButton& btn, const juce::String& text, bool isToggle = true);
    void showControlContextMenu(const juce::String& paramId, juce::Component* comp);

    NordPianoPrototypeAudioProcessor& processor;
    BlackKnobLookAndFeel blackKnobLAF;
    std::unique_ptr<juce::FileChooser> chooser;

    // Top Bar Components
    juce::TextButton btnQuickPrev{"<"};
    juce::TextButton btnQuickNext{">"};
    juce::TextButton btnQuickSelect{"Black Upright Med 6.1"};
    juce::TextButton btnMidiMap{"MIDI MAP"};
    juce::TextButton btnMidiCh{"1 v"};
    juce::TextButton btnOutput{"st. 1 v"};
    juce::TextButton btnSettings{""}; // Cog icon

    // Top Navigation Tabs (A, B, C, D)
    // Live Setlist Data & Performance Parts
    struct SetlistSong {
        juce::String id;
        juce::String title;
        juce::String key;
        int bpm;
        juce::String timeSig;
    };
    std::vector<SetlistSong> setlistQueue = {
        { "01", "Great Is The Lord", "D", 72, "4/4" },
        { "02", "Reckless Love", "F#m", 68, "6/8" },
        { "03", "The Blessing", "B", 70, "4/4" },
        { "04", "Way Maker", "E", 66, "4/4" },
        { "05", "King of Kings", "D", 68, "4/4" }
    };
    int currentSongIndex = 0;
    int activeSongPart = 2; // 0=Intro, 1=Verse, 2=Chorus, etc.
    int currentTranspose = 0; // Transpose in semitones
    int activeTab = 0; // 0 = Piano, 1 = Worship Pad, 2 = Synth Lead, 3 = Organ B3
    juce::TextButton tabPiano{"A  Piano"};
    juce::TextButton tabPad{"B  Worship Pad"};
    juce::TextButton tabSynth{"C  Synth Lead"};
    juce::TextButton tabOrgan{"D  Organ B3"};
    juce::TextButton btnSplitLayer{"Split / Layer v"};

    // 4-Channel Mixer Strips (Faders, Meters, Mute, Solo, On)
    juce::Slider sliderVolA; // Piano
    juce::Slider sliderVolB; // Pad
    juce::Slider sliderVolC; // Synth
    juce::Slider sliderVolD; // Organ
    juce::TextButton btnOnA{"ON"};
    juce::TextButton btnOnB{"ON"};
    juce::TextButton btnOnC{"ON"};
    juce::TextButton btnOnD{"ON"};
    juce::TextButton btnMuteA{"M"};
    juce::TextButton btnMuteB{"M"};
    juce::TextButton btnMuteC{"M"};
    juce::TextButton btnMuteD{"M"};
    juce::TextButton btnSoloA{"S"};
    juce::TextButton btnSoloB{"S"};
    juce::TextButton btnSoloC{"S"};
    juce::TextButton btnSoloD{"S"};

    // Bank Selector Buttons (Bank A, B, C, D)
    int activeBank = 0; // 0 = Bank A, 1 = Bank B, 2 = Bank C, 3 = Bank D
    juce::TextButton btnBankA{"A"};
    juce::TextButton btnBankB{"B"};
    juce::TextButton btnBankC{"C"};
    juce::TextButton btnBankD{"D"};

    // Sound / Timbre Category Buttons
    juce::TextButton btnCatEPiano{"E.Piano"};
    juce::TextButton btnCatClav{"Clav"};
    juce::TextButton btnCatOrgan{"Organ"};
    juce::TextButton btnCatPad{"Pad"};
    juce::TextButton btnCatSynth{"Synth"};
    juce::TextButton btnCatStrings{"Strings"};
    juce::TextButton btnCatMisc{"Misc"};

    // Character Switches
    juce::TextButton swDynamics{"Dynamics"};
    juce::TextButton swPedalNoise{"Pedal Noise"};
    juce::TextButton swDamperRes{"Damper Res"};
    juce::TextButton swSoftRelease{"Soft Release"};

    // Tone & Amp Knobs
    juce::Slider sliderCutoff;
    juce::Slider sliderReso;
    juce::Slider sliderEnv;
    juce::Slider sliderAmpAttack;
    juce::Slider sliderAmpDecay;
    juce::Slider sliderAmpRelease;

    // Comp & Dynamics
    juce::Slider sliderCompThresh;
    juce::Slider sliderCompRatio;
    juce::Slider sliderDynAttack;
    juce::Slider sliderDynRelease;

    // Effects
    juce::TextButton btnFxChorus{"Chorus"};
    juce::TextButton btnFxDelay{"Delay"};
    juce::TextButton btnFxReverb{"Reverb"};
    juce::Slider sliderFxMix;

    // Column 1: EQ / Tone & Dynamics
    juce::TextButton btnEqPiano{"PIANO"};
    juce::TextButton btnEqPad{"PAD"};
    bool eqTargetIsPiano = true;
    juce::Slider sliderTreble;
    juce::Slider sliderMid;
    juce::Slider sliderBass;
    juce::Slider sliderDynamics;
    juce::TextButton btnTouch1{"TOUCH 1"};
    juce::TextButton btnTouch2{"TOUCH 2"};
    juce::TextButton btnTouch3{"TOUCH 3"};

    // Column 2: Dedicated Nord Stage 4 Piano Module
    juce::TextButton btnPianoOn{"ON"};
    juce::TextButton btnPianoPrev{"<"};
    juce::TextButton btnPianoNext{">"};
    juce::TextButton btnPianoSelect{"PIANO SELECT"};
    juce::Slider sliderPianoSelect;
    juce::Slider sliderPianoVol;
    juce::Slider sliderPianoRelease;
    juce::Slider sliderTone;
    juce::Slider sliderResonanceKnob;

    // Nord Stage 4 Timbre / Voicing Filters
    juce::TextButton btnTimbreRaw{"RAW"};
    juce::TextButton btnTimbreSoft{"SOFT"};
    juce::TextButton btnTimbreMid{"MID"};
    juce::TextButton btnTimbreBright{"BRIGHT"};

    // Nord Stage 4 Acoustic Realism Engine
    juce::TextButton btnCatGrand{"GRAND"};
    juce::TextButton btnCatUpright{"UPRIGHT"};
    juce::TextButton btnStringRes{"STRING RES"};
    juce::TextButton btnPedalNoise{"PEDAL NOISE"};
    juce::TextButton btnSoftRelease{"SOFT RELEASE"};
    juce::TextButton btnResonance{"RESONANCE"};
    juce::TextButton btnPedalPolarity{"PEDAL: NORM"};

    // Program Presets & Store
    std::array<juce::TextButton, 8> btnPrograms;
    juce::TextButton btnStore{"STORE"};
    juce::TextButton btnSave{"SAVE"};
    juce::TextButton btnLoadNord{"LOAD"};
    juce::TextButton btnSaveNord{"SAVE"};
    juce::TextButton btnRescanLibrary{"RESCAN SOUNDS"};

    // Dedicated Worship Pad Layer (Layer B)
    juce::TextButton btnPadOn{"ON"};
    juce::TextButton btnPadPrev{"<"};
    juce::TextButton btnPadNext{">"};
    juce::TextButton btnPadSelect{"Celestial Pad"};
    juce::Slider sliderPadVol;
    juce::Slider sliderPadCutoff;
    juce::Slider sliderPadRes;
    juce::Slider sliderPadAttack;
    juce::Slider sliderPadRelease;

    // Column 3: Nord Master Effects Rack & Output
    juce::Slider sliderCompressor;
    juce::TextButton btnCompressorOn{"ON"};

    juce::Slider sliderDelayTime, sliderDelayFeedback;
    juce::TextButton btnDelayOn{"ON"};
    juce::TextButton btnTapTempo{"TAP"};

    juce::Slider sliderReverbAmount;
    juce::Slider sliderReverbTone;
    juce::TextButton btnReverbType{"HALL"};
    juce::TextButton btnReverbVar{"NORM"};
    juce::TextButton btnReverbOn{"ON"};

    juce::Slider sliderChorusDepth;
    juce::Slider sliderTremoloDepth;

    juce::TextButton btnTransDown{"<"};
    juce::TextButton btnTransUp{">"};
    juce::TextButton btnTuneDown{"<"};
    juce::TextButton btnTuneUp{">"};
    juce::Slider sliderTune;
    juce::Slider sliderPartMix;
    juce::TextButton btnOctaveDown{"<"};
    juce::TextButton btnOctaveUp{">"};
    juce::Slider sliderMainVolume;

    // Bottom Keyboard & Status
    std::unique_ptr<InteractivePianoKeyboard> pianoKeyboard;
    std::unique_ptr<MidiMapOverlay> midiMapOverlay;
    juce::Image pianoImage;
    juce::Image imgOledGrandPiano;
    juce::Image chassisBgImage;

    // Status indicators & UI synchronization
    bool midiLedLit = false;
    int midiDecayTimer = 0;
    juce::String lastKnownPiano;
    bool lastKnownLoadingA = false;
    int loadingAnimPulse = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NordPianoPrototypeAudioProcessorEditor)
};

//==============================================================================
// Ableton-Style Full GUI Interactive MIDI Map Overlay
//==============================================================================
class MidiMapOverlay final : public juce::Component {
public:
    struct Target {
        juce::Component* comp = nullptr;
        juce::String paramId;
        juce::String label;
    };

    explicit MidiMapOverlay(NordPianoPrototypeAudioProcessorEditor& ed, NordPianoPrototypeAudioProcessor& proc)
        : editor(ed), processor(proc) {
        setOpaque(false);
        setAlwaysOnTop(true);
    }

    void buildTargets();
    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;

private:
    NordPianoPrototypeAudioProcessorEditor& editor;
    NordPianoPrototypeAudioProcessor& processor;
    std::vector<Target> targets;
};
