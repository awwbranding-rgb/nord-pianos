#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
// Black Edition Custom LookAndFeel (Zero Red, Ice-Blue #65BFFF Accents)
//==============================================================================
// Black Edition Custom LookAndFeel (OmniStage Flagship Console)
//==============================================================================
class BlackKnobLookAndFeel : public juce::LookAndFeel_V4 {
public:
    BlackKnobLookAndFeel() {
        setColour(juce::Slider::rotarySliderFillColourId, juce::Colour::fromRGB(229, 57, 53)); // #E53935
        setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour::fromRGB(25, 29, 36));
        setColour(juce::Slider::thumbColourId, juce::Colour::fromRGB(229, 57, 53));
    }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                          juce::Slider& slider) override {
        juce::ignoreUnused(slider);
        const float radius = juce::jmin(width / 2.0f, height / 2.0f) - 3.5f;
        const float centreX = x + width * 0.5f;
        const float centreY = y + height * 0.5f;
        const float rx = centreX - radius;
        const float ry = centreY - radius;
        const float rw = radius * 2.0f;
        const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

        // 1. Outer Deep Cast Shadow
        g.setColour(juce::Colour::fromRGB(4, 5, 7).withAlpha(0.9f));
        g.fillEllipse(rx + 1.0f, ry + 2.0f, rw, rw);

        // 2. Metallic Beveled Graphite Rim (#343A43 to #1E2229)
        juce::ColourGradient rimGrad(juce::Colour::fromRGB(52, 58, 67), centreX, ry,
                                     juce::Colour::fromRGB(24, 27, 33), centreX, ry + rw, false);
        g.setGradientFill(rimGrad);
        g.fillEllipse(rx, ry, rw, rw);

        // 3. Fine Knurling Groove Ring
        const float knurlR = radius - 1.8f;
        g.setColour(juce::Colour::fromRGB(14, 16, 20));
        g.fillEllipse(centreX - knurlR, centreY - knurlR, knurlR * 2.0f, knurlR * 2.0f);

        // 4. Matte Anodized Black Face (#1E2229 to #0B0D11)
        const float faceR = radius - 4.0f;
        juce::ColourGradient faceGrad(juce::Colour::fromRGB(28, 32, 39), centreX, centreY - faceR,
                                      juce::Colour::fromRGB(11, 13, 17), centreX, centreY + faceR, false);
        g.setGradientFill(faceGrad);
        g.fillEllipse(centreX - faceR, centreY - faceR, faceR * 2.0f, faceR * 2.0f);

        // Concentric Accent Line
        g.setColour(juce::Colour::fromRGB(52, 58, 67).withAlpha(0.4f));
        g.drawEllipse(centreX - faceR + 2.0f, centreY - faceR + 2.0f, (faceR - 2.0f) * 2.0f, (faceR - 2.0f) * 2.0f, 1.0f);

        // 5. LED Track Background (Groove)
        juce::Path trackPath;
        trackPath.addCentredArc(centreX, centreY, radius + 1.5f, radius + 1.5f, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour(juce::Colour::fromRGB(20, 24, 30));
        g.strokePath(trackPath, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // 6. Active Nord Red LED Arc (#E53935)
        if (sliderPos > 0.005f) {
            juce::Path activePath;
            activePath.addCentredArc(centreX, centreY, radius + 1.5f, radius + 1.5f, 0.0f, rotaryStartAngle, angle, true);
            g.setColour(juce::Colour::fromRGB(229, 57, 53)); // Nord Vivid Crimson Red
            g.strokePath(activePath, juce::PathStrokeType(2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // 7. Precision Needle Pointer (White with high contrast)
        juce::Path needle;
        needle.addRoundedRectangle(-1.2f, -faceR + 2.0f, 2.4f, faceR * 0.52f, 1.0f);
        needle.applyTransform(juce::AffineTransform::rotation(angle).translated(centreX, centreY));
        g.setColour(juce::Colour::fromRGB(250, 252, 255));
        g.fillPath(needle);

        // Center Cap Accent (Nord Red Dot)
        g.setColour(juce::Colour::fromRGB(229, 57, 53));
        g.fillEllipse(centreX - 1.8f, centreY - 1.8f, 3.6f, 3.6f);
    }

    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float minSliderPos, float maxSliderPos,
                          const juce::Slider::SliderStyle style, juce::Slider& slider) override {
        if (slider.isVertical()) {
            const float cx = x + width * 0.5f;
            const float trackW = 6.0f;
            const float trackX = cx - trackW * 0.5f;
            const float topY = (float)y + 6.0f;
            const float botY = (float)(y + height) - 6.0f;
            const float trackH = botY - topY;

            // 1. Recessed Fader Well Plate (#12151C to #0A0C10)
            juce::Rectangle<float> well(cx - 20.0f, topY - 2.0f, 40.0f, trackH + 4.0f);
            g.setColour(juce::Colour::fromRGB(2, 3, 5).withAlpha(0.85f));
            g.fillRoundedRectangle(well.translated(0.0f, 2.0f), 3.0f); // cast shadow

            juce::ColourGradient wellGrad(juce::Colour::fromRGB(19, 23, 29), cx, topY,
                                          juce::Colour::fromRGB(10, 12, 16), cx, botY, false);
            g.setGradientFill(wellGrad);
            g.fillRoundedRectangle(well, 3.0f);

            // Inner Shadow & Bevel Edge
            g.setColour(juce::Colour::fromRGB(0, 0, 0).withAlpha(0.7f));
            g.drawRoundedRectangle(well, 3.0f, 1.0f);
            g.setColour(juce::Colour::fromRGB(45, 52, 65).withAlpha(0.5f));
            g.drawRoundedRectangle(well.reduced(0.8f), 2.5f, 0.8f);

            // 2. Calibrated dB Scale Ticks alongside the slot
            struct Tick { float posNorm; bool isZero; };
            const Tick ticks[] = {
                { 0.08f, false },
                { 0.25f, true },  // 0 dB
                { 0.45f, false },
                { 0.65f, false },
                { 0.85f, false }
            };

            for (const auto& tk : ticks) {
                float ty = topY + tk.posNorm * trackH;
                g.setColour(tk.isZero ? juce::Colour::fromRGB(229, 57, 53) : juce::Colour::fromRGB(130, 140, 155));
                g.drawLine(cx - 18.0f, ty, cx - 11.0f, ty, tk.isZero ? 1.6f : 1.0f);
                g.drawLine(cx + 11.0f, ty, cx + 18.0f, ty, tk.isZero ? 1.6f : 1.0f);
            }

            // 3. Central Slot Slit with Dark Inner Shadow
            g.setColour(juce::Colour::fromRGB(4, 5, 8));
            g.fillRoundedRectangle(trackX, topY, trackW, trackH, 2.0f);
            g.setColour(juce::Colour::fromRGB(22, 26, 34));
            g.drawRoundedRectangle(trackX, topY, trackW, trackH, 2.0f, 1.0f);

            // 4. Illuminated Neon Channel along fader position
            juce::Colour neonColor = juce::Colour::fromRGB(229, 57, 53); // Nord Crimson Red
            if (sliderPos < botY) {
                float activeH = botY - sliderPos;
                g.setColour(neonColor.withAlpha(0.28f));
                g.fillRect(trackX + 1.0f, sliderPos, trackW - 2.0f, activeH);
                g.setColour(neonColor);
                g.fillRect(trackX + 2.0f, sliderPos, trackW - 4.0f, activeH);
            }

            // 5. Chunky 3D Aluminum Stage Fader Cap (Roland RD-2000 / Neve style)
            const float capW = 36.0f;
            const float capH = 34.0f;
            const float capX = cx - capW * 0.5f;
            const float capY = juce::jlimit(topY - 1.0f, botY - capH + 1.0f, sliderPos - capH * 0.5f);

            // A. Deep 3D Cast Drop Shadow underneath cap
            g.setColour(juce::Colour::fromRGB(0, 0, 0).withAlpha(0.85f));
            g.fillRoundedRectangle(capX + 1.0f, capY + 3.0f, capW, capH, 3.0f);

            // B. Cap Base Metallic Body (Multi-stage brushed silver-graphite gradient)
            juce::ColourGradient capGrad(juce::Colour::fromRGB(220, 228, 238), capX, capY,
                                         juce::Colour::fromRGB(80, 88, 102), capX, capY + capH, false);
            capGrad.addColour(0.20, juce::Colour::fromRGB(175, 184, 196));
            capGrad.addColour(0.50, juce::Colour::fromRGB(120, 130, 144));
            capGrad.addColour(0.80, juce::Colour::fromRGB(90, 98, 112));
            g.setGradientFill(capGrad);
            g.fillRoundedRectangle(capX, capY, capW, capH, 3.0f);

            // C. Top Specular CNC-Machined Edge Highlight
            g.setColour(juce::Colour::fromRGB(255, 255, 255).withAlpha(0.90f));
            g.drawHorizontalLine(int(capY + 1.0f), capX + 2.0f, capX + capW - 2.0f);
            g.setColour(juce::Colour::fromRGB(20, 24, 32));
            g.drawHorizontalLine(int(capY + capH - 1.0f), capX + 2.0f, capX + capW - 2.0f);
            g.setColour(juce::Colour::fromRGB(40, 48, 60));
            g.drawRoundedRectangle(capX, capY, capW, capH, 3.0f, 1.0f);

            // D. Tactile Grip Ridges (Horizontal finger groove)
            const float midCapY = capY + capH * 0.5f;
            g.setColour(juce::Colour::fromRGB(20, 24, 30));
            g.fillRect(capX + 4.0f, midCapY - 1.5f, capW - 8.0f, 3.0f);

            // E. Center Illuminated Marker Line (High-visibility neon Nord Red)
            g.setColour(neonColor.withAlpha(0.5f));
            g.fillRect(capX + 3.0f, midCapY - 1.5f, capW - 6.0f, 3.0f);
            g.setColour(juce::Colour::fromRGB(255, 70, 75));
            g.fillRect(capX + 4.0f, midCapY - 1.0f, capW - 8.0f, 2.0f);
            g.setColour(juce::Colour::fromRGB(255, 255, 255));
            g.fillRect(capX + 10.0f, midCapY - 0.5f, capW - 20.0f, 1.0f);
        } else {
            juce::LookAndFeel_V4::drawLinearSlider(g, x, y, width, height, sliderPos, minSliderPos, maxSliderPos, style, slider);
        }
    }

    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                              const juce::Colour& backgroundColour,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override {
        juce::ignoreUnused(backgroundColour);
        auto bounds = button.getLocalBounds().toFloat();
        const bool isToggled = button.getToggleState();
        const float corner = 3.5f;

        // Special Case: Slide Toggle Switch (for Dynamics, Pedal Noise, Damper Res)
        if (button.getButtonText().isEmpty() && button.getWidth() <= 42) {
            const float trackH = 18.0f;
            const float trackW = bounds.getWidth() - 2.0f;
            const float trackX = bounds.getX() + 1.0f;
            const float trackY = bounds.getCentreY() - trackH * 0.5f;
            juce::Rectangle<float> trackRect(trackX, trackY, trackW, trackH);

            if (isToggled) {
                juce::ColourGradient tGrad(juce::Colour::fromRGB(235, 45, 52), trackX, trackY,
                                           juce::Colour::fromRGB(160, 20, 26), trackX, trackY + trackH, false);
                g.setGradientFill(tGrad);
            } else {
                g.setColour(juce::Colour::fromRGB(32, 38, 48));
            }
            g.fillRoundedRectangle(trackRect, trackH * 0.5f);
            g.setColour(isToggled ? juce::Colour::fromRGB(255, 90, 95) : juce::Colour::fromRGB(50, 58, 70));
            g.drawRoundedRectangle(trackRect, trackH * 0.5f, 1.0f);

            const float knobD = 14.0f;
            const float knobY = trackY + 2.0f;
            const float knobX = isToggled ? (trackRect.getRight() - knobD - 2.0f) : (trackRect.getX() + 2.0f);

            g.setColour(juce::Colour::fromRGB(0, 0, 0).withAlpha(0.6f));
            g.fillEllipse(knobX, knobY + 1.0f, knobD, knobD);

            juce::ColourGradient kGrad(juce::Colour::fromRGB(255, 255, 255), knobX, knobY,
                                       juce::Colour::fromRGB(210, 215, 222), knobX, knobY + knobD, false);
            g.setGradientFill(kGrad);
            g.fillEllipse(knobX, knobY, knobD, knobD);
            g.setColour(juce::Colour::fromRGB(180, 185, 195));
            g.drawEllipse(knobX, knobY, knobD, knobD, 0.8f);
            return;
        }

        // Special Case: Mixer ON button
        if (button.getButtonText() == "ON") {
            g.setColour(juce::Colour::fromRGB(0, 0, 0).withAlpha(0.6f));
            g.fillRoundedRectangle(bounds.translated(0.0f, 1.5f), corner);

            if (isToggled) {
                juce::ColourGradient onGrad(juce::Colour::fromRGB(45, 20, 24), bounds.getX(), bounds.getY(),
                                            juce::Colour::fromRGB(22, 10, 12), bounds.getX(), bounds.getBottom(), false);
                g.setGradientFill(onGrad);
                g.fillRoundedRectangle(bounds, corner);

                g.setColour(juce::Colour::fromRGB(255, 50, 60).withAlpha(0.45f));
                g.drawRoundedRectangle(bounds.expanded(1.0f), corner + 1.0f, 1.5f);
                g.setColour(juce::Colour::fromRGB(240, 45, 52));
                g.drawRoundedRectangle(bounds, corner, 1.2f);
            } else {
                g.setColour(juce::Colour::fromRGB(20, 24, 30));
                g.fillRoundedRectangle(bounds, corner);
                g.setColour(juce::Colour::fromRGB(45, 52, 65));
                g.drawRoundedRectangle(bounds, corner, 1.0f);
            }
            return;
        }

        // 1. Drop shadow
        g.setColour(juce::Colour::fromRGB(0, 0, 0).withAlpha(0.65f));
        g.fillRoundedRectangle(bounds.translated(0.0f, 1.5f), corner);

        const juce::Colour customBg = button.findColour(juce::TextButton::buttonColourId);
        const bool isRedButton = (customBg == juce::Colour::fromRGB(229, 57, 53) ||
                                  customBg == juce::Colour::fromRGB(198, 32, 32) ||
                                  customBg == juce::Colour::fromRGB(210, 35, 40));

        // 2. Button Body
        if (isToggled || isRedButton) {
            juce::ColourGradient redGrad(
                isToggled ? juce::Colour::fromRGB(235, 45, 52) : juce::Colour::fromRGB(195, 30, 36),
                bounds.getX(), bounds.getY(),
                isToggled ? juce::Colour::fromRGB(140, 16, 22) : juce::Colour::fromRGB(105, 12, 16),
                bounds.getX(), bounds.getBottom(), false);
            g.setGradientFill(redGrad);
            g.fillRoundedRectangle(bounds, corner);

            // Highlight & Glowing Red Border
            g.setColour(juce::Colour::fromRGB(255, 120, 120).withAlpha(0.7f));
            g.drawHorizontalLine(int(bounds.getY() + 1.0f), bounds.getX() + 2.0f, bounds.getRight() - 2.0f);
            g.setColour(isToggled ? juce::Colour::fromRGB(255, 90, 90) : juce::Colour::fromRGB(160, 25, 30));
            g.drawRoundedRectangle(bounds, corner, 1.0f);
        } else {
            juce::ColourGradient baseGrad(
                shouldDrawButtonAsHighlighted ? juce::Colour::fromRGB(45, 52, 64) : juce::Colour::fromRGB(32, 37, 46),
                bounds.getX(), bounds.getY(),
                juce::Colour::fromRGB(15, 18, 23),
                bounds.getX(), bounds.getBottom(), false);
            g.setGradientFill(baseGrad);
            g.fillRoundedRectangle(bounds, corner);

            // Top edge beveled highlight
            g.setColour(juce::Colour::fromRGB(255, 255, 255).withAlpha(shouldDrawButtonAsHighlighted ? 0.15f : 0.08f));
            g.drawHorizontalLine(int(bounds.getY() + 1.0f), bounds.getX() + 2.0f, bounds.getRight() - 2.0f);

            // Border
            g.setColour(shouldDrawButtonAsHighlighted ? juce::Colour::fromRGB(90, 105, 125) : juce::Colour::fromRGB(48, 55, 68));
            g.drawRoundedRectangle(bounds, corner, 1.0f);
        }

        if (shouldDrawButtonAsDown) {
            g.setColour(juce::Colour::fromRGB(0, 0, 0).withAlpha(0.45f));
            g.fillRoundedRectangle(bounds, corner);
        }
    }

    void drawButtonText(juce::Graphics& g, juce::TextButton& button,
                        bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override {
        juce::ignoreUnused(shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);
        const bool isToggled = button.getToggleState();

        if (isToggled) {
            g.setColour(juce::Colour::fromRGB(255, 255, 255)); // Crisp White on Red
        } else {
            g.setColour(juce::Colour::fromRGB(215, 222, 232)); // Off-White Text
        }

        g.setFont(juce::Font(juce::FontOptions(button.getHeight() > 22 ? 10.5f : 9.0f, juce::Font::bold)));
        g.drawFittedText(button.getButtonText(), button.getLocalBounds().reduced(2),
                         juce::Justification::centred, 1);
    }
};

//==============================================================================
// Photorealistic 88-Key Interactive Piano Keyboard (Black Edition)
// Displays active split key range and glows brightly in Ice-Blue when pressed!
//==============================================================================
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
