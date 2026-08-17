// Copyright (c) 2026 Akos Kiss.
//
// Licensed under the BSD 3-Clause License
// <LICENSE.md or https://opensource.org/licenses/BSD-3-Clause>.
// This file may not be copied, modified, or distributed except
// according to those terms.

#include "CecAdapter.h"

#include <QLoggingCategory>

namespace {
Q_LOGGING_CATEGORY(lcCec, "qqcs.cec")
}

// No-op stub, selected by CMake (QQCS_ENABLE_CEC=OFF) on every platform
// without libCEC -- desktop Linux, macOS, and Windows dev environments.
// Identical header to the real CecAdapter.cpp, so nothing outside src/cec/
// can tell which one is linked in.
struct CecAdapter::Impl { };

CecAdapter::CecAdapter(QObject *parent) : QObject(parent), m_impl(new Impl) { }

CecAdapter::~CecAdapter()
{
    delete m_impl;
}

bool CecAdapter::start()
{
    qCInfo(lcCec) << "HDMI-CEC not available on this platform (stub build)";
    return false;
}
