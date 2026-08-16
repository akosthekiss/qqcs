#include "CecAdapter.h"

// cecc.h (the C API wrapper) expects CEC::ICECAdapter to already be
// forward-declared -- it's normally provided by cec.h (the full C++ API),
// which we deliberately don't include since we only use the C functions.
namespace CEC {
class ICECAdapter;
}

#include <libcec/cecc.h>

#include <QLoggingCategory>

#include <cstring>

using namespace CEC;

namespace {

Q_LOGGING_CATEGORY(lcCec, "qqcs.cec")

bool mapUserControlCode(cec_user_control_code code, CecButton &outButton)
{
    switch (code) {
    case CEC_USER_CONTROL_CODE_UP:
        outButton = CecButton::Up;
        return true;
    case CEC_USER_CONTROL_CODE_DOWN:
        outButton = CecButton::Down;
        return true;
    case CEC_USER_CONTROL_CODE_LEFT:
        outButton = CecButton::Left;
        return true;
    case CEC_USER_CONTROL_CODE_RIGHT:
        outButton = CecButton::Right;
        return true;
    case CEC_USER_CONTROL_CODE_SELECT:
        outButton = CecButton::Select;
        return true;
    case CEC_USER_CONTROL_CODE_EXIT:
        outButton = CecButton::Back;
        return true;
    case CEC_USER_CONTROL_CODE_F2_RED:
        outButton = CecButton::Red;
        return true;
    case CEC_USER_CONTROL_CODE_F3_GREEN:
        outButton = CecButton::Green;
        return true;
    case CEC_USER_CONTROL_CODE_F1_BLUE:
        outButton = CecButton::Blue;
        return true;
    case CEC_USER_CONTROL_CODE_F4_YELLOW:
        outButton = CecButton::Yellow;
        return true;
    case CEC_USER_CONTROL_CODE_NUMBER0:
        outButton = CecButton::Digit0;
        return true;
    case CEC_USER_CONTROL_CODE_NUMBER1:
        outButton = CecButton::Digit1;
        return true;
    case CEC_USER_CONTROL_CODE_NUMBER2:
        outButton = CecButton::Digit2;
        return true;
    case CEC_USER_CONTROL_CODE_NUMBER3:
        outButton = CecButton::Digit3;
        return true;
    case CEC_USER_CONTROL_CODE_NUMBER4:
        outButton = CecButton::Digit4;
        return true;
    case CEC_USER_CONTROL_CODE_NUMBER5:
        outButton = CecButton::Digit5;
        return true;
    case CEC_USER_CONTROL_CODE_NUMBER6:
        outButton = CecButton::Digit6;
        return true;
    case CEC_USER_CONTROL_CODE_NUMBER7:
        outButton = CecButton::Digit7;
        return true;
    case CEC_USER_CONTROL_CODE_NUMBER8:
        outButton = CecButton::Digit8;
        return true;
    case CEC_USER_CONTROL_CODE_NUMBER9:
        outButton = CecButton::Digit9;
        return true;
    default:
        return false;
    }
}

// libCEC's C-callback trampoline. Takes void* (not a CecAdapter*) because
// that is the type ICECCallbacks::keyPress actually declares -- keeping
// this a free function in the .cpp, not a class member, is what lets
// CecAdapter.h stay entirely free of libCEC types.
void CEC_CDECL onKeyPress(void *cbParam, const cec_keypress *key)
{
    if (!key || key->duration != 0) // libCEC fires this twice per press; react once, on press
        return;
    auto *adapter = static_cast<CecAdapter *>(cbParam);
    CecButton button;
    if (mapUserControlCode(key->keycode, button))
        adapter->handleButtonPress(button);
}

} // namespace

struct CecAdapter::Impl {
    libcec_connection_t connection = nullptr;
    libcec_configuration config{};
    ICECCallbacks callbacks{};
};

CecAdapter::CecAdapter(QObject *parent) : QObject(parent), m_impl(new Impl) { }

CecAdapter::~CecAdapter()
{
    if (m_impl->connection) {
        libcec_close(m_impl->connection);
        libcec_destroy(m_impl->connection);
    }
    delete m_impl;
}

bool CecAdapter::start()
{
    libcec_clear_configuration(&m_impl->config);
    m_impl->config.clientVersion = LIBCEC_VERSION_CURRENT;
    m_impl->config.deviceTypes.Add(CEC_DEVICE_TYPE_PLAYBACK_DEVICE);
    std::strncpy(m_impl->config.strDeviceName, "qqcs", sizeof(m_impl->config.strDeviceName) - 1);

    m_impl->callbacks.keyPress = &onKeyPress;
    m_impl->config.callbacks = &m_impl->callbacks;
    m_impl->config.callbackParam = this;

    m_impl->connection = libcec_initialise(&m_impl->config);
    if (!m_impl->connection) {
        qCWarning(lcCec) << "Failed to initialise libCEC";
        return false;
    }

    cec_adapter deviceList[10];
    const int8_t found = libcec_find_adapters(m_impl->connection, deviceList, 10, nullptr);
    if (found <= 0) {
        qCInfo(lcCec) << "No HDMI-CEC adapter found";
        libcec_destroy(m_impl->connection);
        m_impl->connection = nullptr;
        return false;
    }

    if (!libcec_open(m_impl->connection, deviceList[0].comm, 5000)) {
        qCWarning(lcCec) << "Failed to open HDMI-CEC adapter" << deviceList[0].comm;
        libcec_destroy(m_impl->connection);
        m_impl->connection = nullptr;
        return false;
    }

    qCInfo(lcCec) << "HDMI-CEC adapter connected:" << deviceList[0].comm;
    return true;
}
