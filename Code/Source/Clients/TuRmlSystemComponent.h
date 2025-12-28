/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: Copyright (c) 2025 Reece Hagan
 *
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 */
#pragma once

#include <TuRml/TuRmlBus.h>

#include <AzCore/Component/Component.h>
#include <AzCore/Component/TickBus.h>
#include <AzCore/Math/PackedVector2.h>
#include <AzCore/Memory/Memory.h>
#include <AzCore/Asset/AssetManager.h>
#include <AzCore/IO/FileIO.h>

#include <AzFramework/Input/Events/InputChannelEventListener.h>
#include <AzFramework/Input/Buses/Notifications/InputTextNotificationBus.h>

#include <RmlUi/Core/FileInterface.h>
#include <RmlUi/Core/SystemInterface.h>
#include <RmlUi/Core/TextInputHandler.h>

namespace TuRml
{
    class TuRmlRenderInterface;

    class TuRmlSystemComponent
        : public AZ::Component
        , protected TuRmlRequestBus::Handler
        , protected AzFramework::InputChannelEventListener
        , protected AzFramework::InputTextNotificationBus::Handler
        , protected AZ::SystemTickBus::Handler
        , protected Rml::SystemInterface
        , protected Rml::FileInterface
        , protected Rml::TextInputHandler
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

        //Rml::TextInputHandler
        void OnActivate(Rml::TextInputContext* ctx) override;
        void OnDeactivate(Rml::TextInputContext* ctx) override;
        void OnDestroy(Rml::TextInputContext* ctx) override;

        //AzFramework::InputChannelEventListener
        bool OnInputChannelEventFiltered(const AzFramework::InputChannel& inputChannel) override;

        //AzFramework::InputTextNotificationBus
        void OnInputTextEvent(const AZStd::string&, bool&) override;
        AZ::s32 GetPriority() const override
        {
            return AzFramework::InputChannelEventListener::GetPriorityUI();
        }

        //System interface
        void JoinPath(Rml::String& translated_path, const Rml::String& document_path, const Rml::String& path) override;

        //File interface
        Rml::FileHandle Open(const Rml::String& path) override;
        void Close(Rml::FileHandle file) override;
        size_t Read(void* buffer, size_t size, Rml::FileHandle file) override;
        bool Seek(Rml::FileHandle file, long offset, int origin) override;
        size_t Tell(Rml::FileHandle file) override;
        size_t Length(Rml::FileHandle file) override;
        bool LoadFile(const Rml::String& path, Rml::String& out_data) override;

    private:
        Rml::TextInputContext* m_activeTxtContext = nullptr;
        AZStd::unique_ptr<TuRmlRenderInterface> m_renderInterface;
    };

} // namespace TuRml
