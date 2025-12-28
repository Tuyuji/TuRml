/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: Copyright (c) 2025 Reece Hagan
 *
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 */
#pragma once

#include <TuRml/TuRmlFeatureProcessorInterface.h>
#include <RmlUi/Core/Context.h>
#include <AzCore/Math/PackedVector2.h>
#include <AzCore/std/containers/unordered_map.h>
#include <Atom/RPI.Public/Pass/PassSystem.h>
#include <Atom/RPI.Public/Image/AttachmentImage.h>
#include <Atom/Bootstrap/BootstrapNotificationBus.h>
#include "TuRmlParentPass.h"
#include "Console/TuRmlConsoleDocument.h"

namespace TuRml
{
    struct UICanvasRenderData
    {
        // Should we update/render this context?
        bool m_isActive = true;
        // If true, resize context to screen size and render directly to swapchain.
        bool m_displayToScreen = false;
        // Are we rendering to a render target?
        bool m_needsRenderTarget = true;
        // Current size of render target, unused if m_display to screen is enabled
        AZ::PackedVector2i m_renderTargetSize = AZ::PackedVector2i(1024, 768);
        // Render target instance, unused/null if m_displayToScreen is enabled.
        AZ::Data::Instance<AZ::RPI::AttachmentImage> m_renderTarget;
    };

    class TuRmlFeatureProcessor final
        : public TuRmlFeatureProcessorInterface
          , public AZ::Render::Bootstrap::NotificationBus::Handler
    {
    public:
        AZ_RTTI(TuRmlFeatureProcessor, "{5FC82712-8460-4DD6-A6CC-496F44F14DB6}", TuRmlFeatureProcessorInterface);
        AZ_CLASS_ALLOCATOR(TuRmlFeatureProcessor, AZ::SystemAllocator)

        static void Reflect(AZ::ReflectContext* context);

        TuRmlFeatureProcessor() = default;
        virtual ~TuRmlFeatureProcessor() = default;

        // FeatureProcessor overrides
        void Activate() override;
        void Deactivate() override;
        void ResizeDisplayCtxs();
        void UpdateContextOutput();
        void Simulate(const FeatureProcessor::SimulatePacket& packet) override;
        void Render(const RenderPacket& packet) override;
        void AddRenderPasses(AZ::RPI::RenderPipeline* renderPipeline) override;

        // Internal methods for RmlUi context management
        Rml::Context* GetContext() override;
        void RegisterContext(Rml::Context* context, bool renderTargetMode = false);
        void UnregisterContext(Rml::Context* context);
        void SetContextDisplayToScreen(Rml::Context* context);

        // Bootstrap::NotificationBus::Handler overrides
        void OnBootstrapSceneReady(AZ::RPI::Scene* bootstrapScene) override;

    private:
        void AddDebugToPrimeContext();
        Rml::Context* m_context = nullptr;
        AZStd::unique_ptr<TuRmlConsoleDocument> m_consoleDocument;

        void CreateRenderTarget(Rml::Context* context);

        AZStd::unordered_map<Rml::Context*, UICanvasRenderData> m_contextRenderData = {};
        bool m_renderTargetsDirty = false;

        AZ::RPI::Ptr<TuRmlParentPass> m_parentPass = nullptr;
    };
}
