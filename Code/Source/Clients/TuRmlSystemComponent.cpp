/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: Copyright (c) 2025 Reece Hagan
 *
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 */
#include "TuRmlSystemComponent.h"

#include <AzCore/Serialization/SerializeContext.h>
#include <Atom/RPI.Public/FeatureProcessorFactory.h>
#include <Atom/RPI.Public/Scene.h>
#include <Atom/RPI.Public/Pass/PassSystemInterface.h>

#include <TuRml/TuRmlTypeIds.h>
#include <Console/TuRmlConsoleDocument.h>
#include <Render/TuRmlFeatureProcessor.h>
#include <Render/TuRmlParentPass.h>
#include <Render/TuRmlChildPass.h>
#include <Render/TuRmlRenderInterface.h>

#include <RmlUi/Core.h>

#include "../RmlBudget.h"
AZ_DEFINE_BUDGET(RmlBudget);

namespace TuRml
{
    AZ_COMPONENT_IMPL(TuRmlSystemComponent, "TuRmlSystemComponent",
        TuRmlSystemComponentTypeId);

    void TuRmlSystemComponent::Reflect(AZ::ReflectContext* context)
    {
        if (auto serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<TuRmlSystemComponent, AZ::Component>()
                ->Version(0)
                ;
        }

        TuRmlFeatureProcessor::Reflect(context);
    }

    void TuRmlSystemComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC_CE("TuRmlSystemService"));
    }

    void TuRmlSystemComponent::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        incompatible.push_back(AZ_CRC_CE("TuRmlSystemService"));
    }

    void TuRmlSystemComponent::GetRequiredServices([[maybe_unused]] AZ::ComponentDescriptor::DependencyArrayType& required)
    {
        required.push_back(AZ_CRC_CE("RPISystem"));
    }

    void TuRmlSystemComponent::GetDependentServices([[maybe_unused]] AZ::ComponentDescriptor::DependencyArrayType& dependent)
    {
        dependent.push_back(AZ_CRC_CE("PassTemplatesAutoLoader"));
    }

    TuRmlSystemComponent::TuRmlSystemComponent()
    {
        if (TuRmlInterface::Get() == nullptr)
        {
            TuRmlInterface::Register(this);
        }
    }

    TuRmlSystemComponent::~TuRmlSystemComponent()
    {
        if (TuRmlInterface::Get() == this)
        {
            TuRmlInterface::Unregister(this);
        }
    }

    TuRmlRenderInterface* TuRmlSystemComponent::GetRenderInterface()
    {
        return m_renderInterface.get();
    }

    void TuRmlSystemComponent::Init()
    {
    }

    void TuRmlSystemComponent::Activate()
    {
        AZ::SystemTickBus::Handler::BusConnect();
        TuRmlRequestBus::Handler::BusConnect();
        TuRmlInterface::Register(this);

        m_renderInterface = AZStd::make_unique<TuRmlRenderInterface>();
        Rml::SetRenderInterface(m_renderInterface.get());
        m_fileInterface.Init();
        m_inputInterface.Init();
        m_systemInterface.Init();

        if (!Rml::Initialise())
        {
            AZ_Error("TuRml", false, "Failed to initialise RmlUi");
            return;
        }

        Rml::LoadFontFace("Fonts/Roboto-Regular.ttf");
        Rml::LoadFontFace("Fonts/Roboto-Bold.ttf");
        Rml::LoadFontFace("Fonts/Roboto-Italic.ttf");
        Rml::LoadFontFace("Fonts/LatoLatin-Regular.ttf");
        Rml::LoadFontFace("Fonts/LatoLatin-Italic.ttf");
        Rml::LoadFontFace("Console/JetBrainsMono-Regular.ttf");
        Rml::LoadFontFace("Fonts/NotoSansJP-VariableFont_wght.ttf", true);
        
        // Register pass classes
        auto* passSystem = AZ::RPI::PassSystemInterface::Get();
        passSystem->AddPassCreator(AZ::Name("TuRmlParentPass"), &TuRmlParentPass::Create);
        passSystem->AddPassCreator(AZ::Name("TuRmlChildPass"), &TuRmlChildPass::Create);

        AZ::RPI::FeatureProcessorFactory::Get()->RegisterFeatureProcessor<TuRmlFeatureProcessor>();
    }

    void TuRmlSystemComponent::Deactivate()
    {
        AZ::SystemTickBus::Handler::BusDisconnect();
        m_inputInterface.Shutdown();
        m_fileInterface.Shutdown();
        m_systemInterface.Shutdown();

        AZ::RPI::FeatureProcessorFactory::Get()->UnregisterFeatureProcessor<TuRmlFeatureProcessor>();

        Rml::Shutdown();

        if (m_renderInterface)
        {
            m_renderInterface.reset();
        }

        TuRmlInterface::Unregister(this);
        TuRmlRequestBus::Handler::BusDisconnect();
    }

    void TuRmlSystemComponent::OnSystemTick()
    {
        const auto numCtxs = Rml::GetNumContexts();
        for (auto i = 0; i < numCtxs; ++i)
        {
            auto ctx = Rml::GetContext(i);
            if (ctx == nullptr)
                continue;

            ctx->Update();
        }
    }
} // namespace TuRml
