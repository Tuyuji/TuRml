/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: Copyright (c) 2025 Reece Hagan
 *
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 */
#pragma once

#include <AzCore/base.h>
#include <Atom/RPI.Public/FeatureProcessor.h>

namespace Rml
{
    class Context;
}

namespace TuRml
{
    class TuRml;
    class TuRmlConsoleOverlay;

    using TuRmlHandle = AZStd::shared_ptr<TuRml>;

    class TuRmlFeatureProcessorInterface
        : public AZ::RPI::FeatureProcessor
    {
    public:
        AZ_RTTI(TuRmlFeatureProcessorInterface, "{D41034AD-D364-4BBE-B021-35BA8F50967B}", AZ::RPI::FeatureProcessor);

        virtual Rml::Context* GetContext() = 0;
    };
}
