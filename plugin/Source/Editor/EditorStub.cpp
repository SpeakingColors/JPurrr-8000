// PLACEHOLDER ONLY: parameter list window used during development. The native mockup editor replaces it; it never ships.
#include "EditorFactory.h"
juce::AudioProcessorEditor* createJPurrrEditor(JPurrr8000AudioProcessor& p) { return new juce::GenericAudioProcessorEditor(p); }
