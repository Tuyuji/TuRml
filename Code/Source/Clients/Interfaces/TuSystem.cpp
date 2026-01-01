/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: Copyright (c) 2025 Reece Hagan
 *
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 */

#include "TuSystem.h"

#include <RmlUi/Core.h>

#include <AzCore/Debug/Trace.h>

using namespace TuRml;

[[maybe_unused]] constexpr const char logRml[] = "Rml";

void TuSystem::Init()
{
    Rml::SetSystemInterface(this);
}

void TuSystem::Shutdown()
{
}

bool TuSystem::LogMessage(Rml::Log::Type type, const Rml::String& message)
{
    switch (type)
    {
    case Rml::Log::LT_ALWAYS:
    case Rml::Log::LT_INFO:
        AZ_Info(logRml, message.c_str());
        break;
    case Rml::Log::LT_ASSERT:
    case Rml::Log::LT_ERROR:
        AZ_Error(logRml, false, message.c_str());
        break;
    case Rml::Log::LT_WARNING:
        AZ_Warning(logRml, false, message.c_str());
        break;
    case Rml::Log::LT_DEBUG:
        AZ_Trace(logRml, message.c_str());
        break;
    case Rml::Log::LT_MAX:
    default:
        break;
    }

    return false;
}
