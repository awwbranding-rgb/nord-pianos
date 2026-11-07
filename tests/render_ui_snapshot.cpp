#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <fstream>
#include <iostream>

int runSnapshot() {
    std::cout << "Starting UI Snapshot..." << std::endl;
    juce::ScopedJuceInitialiser_GUI guiInit;

    NordPianoPrototypeAudioProcessor processor;
    std::cout << "Processor created." << std::endl;

    auto editor = std::unique_ptr<juce::AudioProcessorEditor>(processor.createEditor());
    if (!editor) {
        std::cerr << "Failed to create editor!" << std::endl;
        return 1;
    }

    editor->setSize(1024, 768);
    editor->setVisible(true);
    std::cout << "Editor sized: " << editor->getBounds().toString().toStdString() << std::endl;

    for (int waitCount = 0; waitCount < 100; ++waitCount) {
        juce::Thread::sleep(50);
        if (!processor.isLoadingLayerA() && !processor.isLoadingLayerB() && !processor.isLoadingLayerC()) {
            break;
        }
    }
    juce::Thread::sleep(100);

    juce::Image snapshot = editor->createComponentSnapshot(editor->getLocalBounds(), false, 1.0f);
    std::cout << "Snapshot taken: " << snapshot.getWidth() << "x" << snapshot.getHeight() << std::endl;

    juce::File outputFile("C:\\Projetos\\Nord_Piano_Kit\\Nord_Piano_Reader\\black_edition_ui_snapshot.png");
    outputFile.deleteFile();

    {
        juce::FileOutputStream stream(outputFile);
        if (stream.openedOk()) {
            juce::PNGImageFormat png;
            png.writeImageToStream(snapshot, stream);
            stream.flush();
            std::cout << "Successfully saved snapshot: " << outputFile.getSize() << " bytes!" << std::endl;
        } else {
            std::cerr << "Failed to open output stream!" << std::endl;
        }
    }

    return 0;
}

int main(int argc, char* argv[]) {
    juce::ignoreUnused(argc, argv);
    return runSnapshot();
}

