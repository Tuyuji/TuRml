/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: Copyright (c) 2025 Reece Hagan
 *
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 */
#pragma once

#include <AzToolsFramework/API/ToolsApplicationAPI.h>

#include <Clients/TuRmlSystemComponent.h>

namespace TuRml
{
    /// System component for TuRml editor
    class TuRmlEditorSystemComponent
        : public TuRmlSystemComponent
        , protected AzToolsFramework::EditorEvents::Bus::Handler
    {
        using BaseSystemComponent = TuRmlSystemComponent;
    public:
        AZ_COMPONENT_DECL(TuRmlEditorSystemComponent);

        static void Reflect(AZ::ReflectContext* context);

        TuRmlEditorSystemComponent();
        ~TuRmlEditorSystemComponent() override;

    private:
        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);
        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);
        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required);
        static void GetDependentServices(AZ::ComponentDescriptor::DependencyArrayType& dependent);

        // AZ::Component
        void Activate() override;
        void Deactivate() override;
    };
} // namespace TuRml
