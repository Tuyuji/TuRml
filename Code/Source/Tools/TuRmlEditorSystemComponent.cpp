/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: Copyright (c) 2025 Reece Hagan
 *
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 */
#include <AzCore/Serialization/SerializeContext.h>
#include "TuRmlEditorSystemComponent.h"

#include <TuRml/TuRmlTypeIds.h>

namespace TuRml
{
    AZ_COMPONENT_IMPL(TuRmlEditorSystemComponent, "TuRmlEditorSystemComponent",
        TuRmlEditorSystemComponentTypeId, BaseSystemComponent);

    void TuRmlEditorSystemComponent::Reflect(AZ::ReflectContext* context)
    {
        if (auto serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<TuRmlEditorSystemComponent, TuRmlSystemComponent>()
                ->Version(0);
        }
    }

    TuRmlEditorSystemComponent::TuRmlEditorSystemComponent() = default;

    TuRmlEditorSystemComponent::~TuRmlEditorSystemComponent() = default;

    void TuRmlEditorSystemComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        BaseSystemComponent::GetProvidedServices(provided);
        provided.push_back(AZ_CRC_CE("TuRmlSystemEditorService"));
    }

    void TuRmlEditorSystemComponent::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        BaseSystemComponent::GetIncompatibleServices(incompatible);
        incompatible.push_back(AZ_CRC_CE("TuRmlSystemEditorService"));
    }

    void TuRmlEditorSystemComponent::GetRequiredServices([[maybe_unused]] AZ::ComponentDescriptor::DependencyArrayType& required)
    {
        BaseSystemComponent::GetRequiredServices(required);
    }

    void TuRmlEditorSystemComponent::GetDependentServices([[maybe_unused]] AZ::ComponentDescriptor::DependencyArrayType& dependent)
    {
        BaseSystemComponent::GetDependentServices(dependent);
    }

    void TuRmlEditorSystemComponent::Activate()
    {
        TuRmlSystemComponent::Activate();
        AzToolsFramework::EditorEvents::Bus::Handler::BusConnect();
    }

    void TuRmlEditorSystemComponent::Deactivate()
    {
        AzToolsFramework::EditorEvents::Bus::Handler::BusDisconnect();
        TuRmlSystemComponent::Deactivate();
    }

} // namespace TuRml
