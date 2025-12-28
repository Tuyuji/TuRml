/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: Copyright (c) 2025 Reece Hagan
 *
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 */
#pragma once

#include "TuRmlChildPass.h"
#include <RmlUi/Core/Context.h>
#include <AzCore/Memory/SystemAllocator.h>
#include <AzCore/std/containers/unordered_map.h>
#include <Atom/RPI.Public/Pass/ParentPass.h>
#include <Atom/RPI.Public/Image/AttachmentImage.h>

namespace TuRml
{
    struct ContextPassData
    {
        AZ::RPI::Ptr<TuRmlChildPass> m_childPass = nullptr;
        AZ::Data::Instance<AZ::RPI::AttachmentImage> m_renderTarget = nullptr;
        bool m_isDirectPipelineMode = false; // Track which mode this pass is in
    };

    //! Parent pass that manages child passes for each RmlUi context
    class TuRmlParentPass final
        : public AZ::RPI::ParentPass
    {
        AZ_RPI_PASS(TuRmlParentPass);

    public:
        AZ_CLASS_ALLOCATOR(TuRmlParentPass, AZ::SystemAllocator);
        AZ_RTTI(TuRmlParentPass, "{9F3E8B56-2D7C-4E8A-AF4F-3F5B6C7D8E9F}", AZ::RPI::ParentPass);

        ~TuRmlParentPass() override = default;
        static AZ::RPI::Ptr<TuRmlParentPass> Create(const AZ::RPI::PassDescriptor& descriptor);

        void UpdateRenderTarget(Rml::Context* context, AZ::Data::Instance<AZ::RPI::AttachmentImage> attachmentImage);

        //! Set context to direct pipeline mode (render directly to main pipeline)
        void SetDirectPipelineMode(Rml::Context* context);

        //! Removes the child pass for the given context.
        void RemoveChildPass(Rml::Context* context);

        AZ::RPI::Ptr<TuRmlChildPass> GetChildPass(Rml::Context* context) const;

    protected:
        // Pass behavior overrides
        void BuildInternal() override;
        void CreateChildPassesInternal() override;

    private:
        TuRmlParentPass() = delete;
        explicit TuRmlParentPass(const AZ::RPI::PassDescriptor& descriptor);

        void AddChildPassForContext(Rml::Context* context,
                                    AZ::Data::Instance<AZ::RPI::AttachmentImage> attachmentImage);
        void AddDirectPipelineChildPassForContext(Rml::Context* context);

        //! Helper method to switch between direct pipeline and render target modes
        void SwitchContextMode(Rml::Context* context, bool isDirectPipeline,
                               AZ::Data::Instance<AZ::RPI::AttachmentImage> renderTarget);

        AZStd::unordered_map<Rml::Context*, ContextPassData> m_contextPasses = {};
    };
}
