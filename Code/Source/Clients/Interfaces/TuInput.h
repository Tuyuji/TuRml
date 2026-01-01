/*
* SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: Copyright (c) 2025 Reece Hagan
 *
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 */
#pragma once

#include <AzFramework/Input/Events/InputChannelEventListener.h>
#include <AzFramework/Input/Buses/Notifications/InputTextNotificationBus.h>

#include <RmlUi/Core.h>
#include <RmlUi/Core/TextInputHandler.h>

namespace TuRml
{
    class TuInput final
       : public Rml::TextInputHandler
       , protected AzFramework::InputChannelEventListener
       , protected AzFramework::InputTextNotificationBus::Handler
   {
   public:
       void Init();
       void Shutdown();

       //Rml::TextInputHandler
       void OnActivate(Rml::TextInputContext* ctx) override;
       void OnDeactivate(Rml::TextInputContext* ctx) override;
       void OnDestroy(Rml::TextInputContext* ctx) override;

   protected:
       //AzFramework::InputChannelEventListener
       bool OnInputChannelEventFiltered(const AzFramework::InputChannel& inputChannel) override;

       //AzFramework::InputTextNotificationBus
       void OnInputTextEvent(const AZStd::string&, bool&) override;
       AZ::s32 GetPriority() const override
       {
           return AzFramework::InputChannelEventListener::GetPriorityUI();
       }
   private:
        Rml::TextInputContext* m_activeTxtContext = nullptr;
   };
}
