/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: Copyright (c) 2025 Reece Hagan
 *
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 */
#include <TuRml/TuRmlTypeIds.h>
#include <TuRmlModuleInterface.h>
#include "TuRmlEditorSystemComponent.h"
#include "Components/EditorTuRmlComponent.h"
#include "Components/TuRmlTestComponent.h"

namespace TuRml
{
    class TuRmlEditorModule
        : public TuRmlModuleInterface
    {
    public:
        AZ_RTTI(TuRmlEditorModule, TuRmlEditorModuleTypeId, TuRmlModuleInterface);
        AZ_CLASS_ALLOCATOR(TuRmlEditorModule, AZ::SystemAllocator);

        TuRmlEditorModule()
        {
            // Push results of [MyComponent]::CreateDescriptor() into m_descriptors here.
            // Add ALL components descriptors associated with this gem to m_descriptors.
            // This will associate the AzTypeInfo information for the components with the the SerializeContext, BehaviorContext and EditContext.
            // This happens through the [MyComponent]::Reflect() function.
            m_descriptors.insert(m_descriptors.end(), {
                TuRmlEditorSystemComponent::CreateDescriptor()
            });
        }

        /**
         * Add required SystemComponents to the SystemEntity.
         * Non-SystemComponents should not be added here
         */
        AZ::ComponentTypeList GetRequiredSystemComponents() const override
        {
            return AZ::ComponentTypeList {
                azrtti_typeid<TuRmlEditorSystemComponent>(),
            };
        }
    };
}// namespace TuRml

#if defined(O3DE_GEM_NAME)
AZ_DECLARE_MODULE_CLASS(AZ_JOIN(Gem_, O3DE_GEM_NAME, _Editor), TuRml::TuRmlEditorModule)
#else
AZ_DECLARE_MODULE_CLASS(Gem_TuRml_Editor, TuRml::TuRmlEditorModule)
#endif
