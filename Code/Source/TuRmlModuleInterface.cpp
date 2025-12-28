/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: Copyright (c) 2025 Reece Hagan
 *
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 */
#include "TuRmlModuleInterface.h"
#include <AzCore/Memory/Memory.h>

#include <TuRml/TuRmlTypeIds.h>

#include <Clients/TuRmlSystemComponent.h>

namespace TuRml
{
    AZ_TYPE_INFO_WITH_NAME_IMPL(TuRmlModuleInterface,
        "TuRmlModuleInterface", TuRmlModuleInterfaceTypeId);
    AZ_RTTI_NO_TYPE_INFO_IMPL(TuRmlModuleInterface, AZ::Module);
    AZ_CLASS_ALLOCATOR_IMPL(TuRmlModuleInterface, AZ::SystemAllocator);

    TuRmlModuleInterface::TuRmlModuleInterface()
    {
        // Push results of [MyComponent]::CreateDescriptor() into m_descriptors here.
        // Add ALL components descriptors associated with this gem to m_descriptors.
        // This will associate the AzTypeInfo information for the components with the the SerializeContext, BehaviorContext and EditContext.
        // This happens through the [MyComponent]::Reflect() function.
        m_descriptors.insert(m_descriptors.end(), {
            TuRmlSystemComponent::CreateDescriptor(),
            });
    }

    AZ::ComponentTypeList TuRmlModuleInterface::GetRequiredSystemComponents() const
    {
        return AZ::ComponentTypeList{
            azrtti_typeid<TuRmlSystemComponent>(),
        };
    }
} // namespace TuRml
