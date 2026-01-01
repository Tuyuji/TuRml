/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: Copyright (c) 2025 Reece Hagan
 *
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 */
#pragma once

#include <TuRml/TuRmlBus.h>
#include "Interfaces/TuFile.h"
#include "Interfaces/TuInput.h"
#include "Interfaces/TuSystem.h"

#include <AzCore/Component/Component.h>
#include <AzCore/Component/TickBus.h>
#include <AzCore/Math/PackedVector2.h>
#include <AzCore/Memory/Memory.h>
#include <AzCore/Asset/AssetManager.h>
#include <AzCore/IO/FileIO.h>

#include <AzFramework/Input/Events/InputChannelEventListener.h>
#include <AzFramework/Input/Buses/Notifications/InputTextNotificationBus.h>

namespace TuRml
{
    class TuRmlRenderInterface;

    class TuRmlSystemComponent
        : public AZ::Component
        , protected TuRmlRequestBus::Handler
        , protected AZ::SystemTickBus::Handler
    {
    public:
        AZ_COMPONENT_DECL(TuRmlSystemComponent);

        static void Reflect(AZ::ReflectContext* context);

        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);
        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);
        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required);
        static void GetDependentServices(AZ::ComponentDescriptor::DependencyArrayType& dependent);

        TuRmlSystemComponent();
        ~TuRmlSystemComponent() override;

    protected:
        ////////////////////////////////////////////////////////////////////////
        // TuRmlRequestBus interface implementation
        TuRmlRenderInterface* GetRenderInterface() override;
        ////////////////////////////////////////////////////////////////////////

        ////////////////////////////////////////////////////////////////////////
        // AZ::Component interface implementation
        void Init() override;
        void Activate() override;
        void Deactivate() override;
        ////////////////////////////////////////////////////////////////////////

        //AZ::SystemTickBus
        void OnSystemTick() override;

    private:
        TuFile m_fileInterface;
        TuInput m_inputInterface;
        TuSystem m_systemInterface;
        AZStd::unique_ptr<TuRmlRenderInterface> m_renderInterface;
    };

} // namespace TuRml
