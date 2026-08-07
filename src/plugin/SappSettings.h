#pragma once
// Shared Sapp-wide settings (samples folder) used by the processor and the
// Instruments panel. Every Sapp instrument reads the same file, so a folder
// chosen once applies to the whole family.

#include <juce_data_structures/juce_data_structures.h>

namespace sapporch::settings {

inline juce::PropertiesFile& file()
{
    static juce::PropertiesFile::Options options = [] {
        juce::PropertiesFile::Options o;
        o.applicationName = "SampleLibraries";
        o.filenameSuffix = ".settings";
        o.folderName = "Sapp";
        o.osxLibrarySubFolder = "Application Support";
        return o;
    }();
    static juce::PropertiesFile instance(options);
    return instance;
}

inline juce::File samplesRoot()
{
    const auto fallback = juce::File::getSpecialLocation(juce::File::userHomeDirectory)
                              .getChildFile("Samples");
    const juce::File root(file().getValue("samplesRoot", fallback.getFullPathName()));
    return root.isDirectory() ? root : fallback;
}

inline void setSamplesRoot(const juce::File& root)
{
    file().setValue("samplesRoot", root.getFullPathName());
    file().saveIfNeeded();
}

} // namespace sapporch::settings
