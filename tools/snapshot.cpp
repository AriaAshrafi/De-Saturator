// Renders the window to PNG files to compare with design/states. PLACEHOLDER: writes the default state only.
//
//   desat_snapshot [folder]

#include "plugin/PluginProcessor.h"
#include "ui/DeSaturatorView.h"

int main (int argc, char** argv)
{
    const juce::ScopedJuceInitialiser_GUI gui;
    const juce::File folder = juce::File::getCurrentWorkingDirectory().getChildFile (argc > 1 ? argv[1] : "snapshots");
    folder.createDirectory();

    DeSaturatorProcessor processor;
    DeSaturatorView view (processor);

    const auto image = view.createComponentSnapshot (view.getLocalBounds(), true, 1.0f);
    juce::FileOutputStream out (folder.getChildFile ("default.png"));
    if (! out.openedOk())
        return 1;
    out.setPosition (0);
    out.truncate();
    juce::PNGImageFormat().writeImageToStream (image, out);
    return 0;
}
