#pragma once

#include "CecButton.h"

#include <QObject>

// Identical interface everywhere; CecAdapter.cpp (real, libCEC-backed,
// only compiled when QQCS_ENABLE_CEC is set -- see src/cec/CMakeLists.txt)
// and CecAdapterStub.cpp (no-op) both implement this exact header, so
// nothing outside src/cec/ -- not even main.cpp -- can tell which one is
// linked in. That is what makes "CEC's absence must never block the app"
// (SPEC §25/§28) structurally true rather than a runtime check someone
// could forget: there is no code path where CEC's absence is even
// observable at the call site.
class CecAdapter : public QObject
{
    Q_OBJECT

public:
    explicit CecAdapter(QObject *parent = nullptr);
    ~CecAdapter() override;

    // Never throws. Returns false and logs if no CEC library/adapter
    // hardware is available -- true whether that's because this build has
    // no libCEC linked in at all, or because it does but no adapter was
    // found (e.g. running on a desktop with no HDMI-CEC device attached).
    bool start();

    // Public only so the real implementation's libCEC C-callback
    // trampoline (a free function in CecAdapter.cpp, not a member, so the
    // real cec_keypress type never has to appear in this header) can
    // reach it without this header ever naming a libCEC type.
    void handleButtonPress(CecButton button);

signals:
    void buttonPressed(CecButton button);

private:
    struct Impl;
    Impl *m_impl;
};

inline void CecAdapter::handleButtonPress(CecButton button)
{
    emit buttonPressed(button);
}
