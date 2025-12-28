/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: Copyright (c) 2025 Reece Hagan
 *
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 */
#include <AzCore/RTTI/RTTI.h>
#include <TuRml/TuRmlTypeIds.h>
#include <TuRmlModuleInterface.h>
#include "TuRmlSystemComponent.h"

namespace TuRml
{
    class TuRmlModule
        : public TuRmlModuleInterface
    {
    public:
        AZ_RTTI(TuRmlModule, TuRmlModuleTypeId, TuRmlModuleInterface);
        AZ_CLASS_ALLOCATOR(TuRmlModule, AZ::SystemAllocator);

        TuRmlModule()
        {
            m_descriptors.insert(m_descriptors.end(),
                {
                    TuRmlSystemComponent::CreateDescriptor()
                });
        }

        AZ::ComponentTypeList GetRequiredSystemComponents() const
        {
            return AZ::ComponentTypeList{ azrtti_typeid<TuRmlSystemComponent>() };
        }
    };
}// namespace TuRml

#if defined(O3DE_GEM_NAME)
AZ_DECLARE_MODULE_CLASS(AZ_JOIN(Gem_, O3DE_GEM_NAME), TuRml::TuRmlModule)
#else
AZ_DECLARE_MODULE_CLASS(Gem_TuRml, TuRml::TuRmlModule)
#endif
