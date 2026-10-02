// Non-Apple platforms: desktop-output capture is not available. The class keeps the same interface so the
// rest of the app compiles unchanged; isSupported() is false, so the UI never offers it.
#include "MacosSystemAudioInput.h"

class MacosSystemAudioInputPrivate {};

MacosSystemAudioInput::MacosSystemAudioInput(QObject* parent) : AudioInput(parent) {}
MacosSystemAudioInput::~MacosSystemAudioInput() = default;

bool MacosSystemAudioInput::start()
{
    emit errorOccurred(QStringLiteral("System audio capture is only available on macOS."));
    return false;
}

void MacosSystemAudioInput::stop() {}

QString MacosSystemAudioInput::name() const
{
    return QStringLiteral("System Audio (unsupported on this platform)");
}

bool MacosSystemAudioInput::isSupported()
{
    return false;
}
