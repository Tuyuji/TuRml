/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: Copyright (c) 2025 Reece Hagan
 *
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 */
#pragma once

#include <AzCore/Memory/SystemAllocator.h>
#include <Atom/RPI.Public/Pass/RasterPass.h>
#include <Atom/RPI.Public/Image/AttachmentImage.h>
#include <Atom/RPI.Public/PipelineState.h>

#include "TuRmlRenderInterface.h"

namespace Rml
{
    class Context;
}

namespace TuRml
{
    class SrgRecycler
    {
    public:
        SrgRecycler(const AZ::Data::Instance<AZ::RPI::Shader>& shader);
        struct Srg
        {
            AZ::Data::Instance<AZ::RPI::ShaderResourceGroup> m_srg;
            bool inUse = false;
        };

        Srg* GetSrg();
        void FreeSrg(Srg* srg);

    private:
        AZ::Data::Instance<AZ::RPI::Shader> m_shader = {};
        AZStd::vector<AZStd::unique_ptr<Srg>> m_srgs = {};
        AZStd::mutex m_mutex;
    };

    struct TuRmlChildPassDrawCommand
    {
        TuRmlDrawCommand drawCommand = {};
        SrgRecycler::Srg* drawSrg = nullptr;
        bool srgReady = false;
    };

    struct BufferedTuRmlDrawCommands
    {
        static constexpr AZ::u32 DrawCommandBuffering = 2;
        AZStd::array<AZStd::vector<TuRmlChildPassDrawCommand>, DrawCommandBuffering> m_drawCommands;
        AZ::u8 m_currentIndex = 0;

        void NextBuffer() { m_currentIndex = (m_currentIndex + 1) % DrawCommandBuffering; }

        AZStd::vector<TuRmlChildPassDrawCommand>& Get() { return m_drawCommands[m_currentIndex]; }
    };

    struct PipelineStates
    {
        AZ::RPI::Ptr<AZ::RPI::PipelineStateForDraw> standard;
        AZ::RPI::Ptr<AZ::RPI::PipelineStateForDraw> standardStencilTest;
        //The following pipeline states are for Rml::ClipMaskOperation
        AZ::RPI::Ptr<AZ::RPI::PipelineStateForDraw> CMO_Set;
        //For SetInverse use Set
        AZ::RPI::Ptr<AZ::RPI::PipelineStateForDraw> CMO_Intersect;

        AZ::RPI::Ptr<AZ::RPI::PipelineStateForDraw> GetPipelineStateForClipMaskOp(
            const Rml::ClipMaskOperation operation)
        {
            switch (operation)
            {
            case Rml::ClipMaskOperation::SetInverse:
            case Rml::ClipMaskOperation::Set:
                return CMO_Set;
            case Rml::ClipMaskOperation::Intersect:
                return CMO_Intersect;
            default:
                return standard;
            }
        }
    };

    //! Child pass that can render RmlUi either to a specific render target or directly to the main pipeline
    class TuRmlChildPass final
        : public AZ::RPI::RasterPass
    {
        AZ_RPI_PASS(TuRmlChildPass);

    public:
        AZ_CLASS_ALLOCATOR(TuRmlChildPass, AZ::SystemAllocator);
        AZ_RTTI(TuRmlChildPass, "{8F2E7A45-1C6B-4D89-9B3F-2E4A5C6D7E8F}", AZ::RPI::RasterPass);

        ~TuRmlChildPass() override = default;
        static AZ::RPI::Ptr<TuRmlChildPass> Create(const AZ::RPI::PassDescriptor& descriptor);

        void UpdateRenderTarget(AZ::Data::Instance<AZ::RPI::AttachmentImage> attachmentImage);
        void SetRmlContext(Rml::Context* context);

        //! Set the pass to render directly to the main pipeline (no specific render target
        void SetDirectPipelineMode();

        AZ::Data::Instance<AZ::RPI::AttachmentImage> GetAttachmentImage() const
        {
            return m_attachmentImage;
        }

        Rml::Context* GetRmlContext() const
        {
            return m_rmlContext;
        }

    protected:
        void BuildInternal() override;
        void SetupFrameGraphDependencies(AZ::RHI::FrameGraphInterface frameGraph) override;
        void CompileResources(const AZ::RHI::FrameGraphCompileContext& context) override;
        void BuildCommandListInternal(const AZ::RHI::FrameGraphExecuteContext& context) override;

        void StandardPipelineStateInit(AZ::RPI::Ptr<AZ::RPI::PipelineStateForDraw>& ps);
        void StandardPipelineStateFinish(AZ::RPI::Ptr<AZ::RPI::PipelineStateForDraw>& ps);

    private:
        TuRmlChildPass() = delete;
        explicit TuRmlChildPass(const AZ::RPI::PassDescriptor& descriptor);

        AZ::Data::Instance<AZ::RPI::AttachmentImage> m_attachmentImage;
        Rml::Context* m_rmlContext = nullptr;

        BufferedTuRmlDrawCommands m_drawCommands = {};

        AZStd::unique_ptr<SrgRecycler> m_srgRecycler;
        AZ::Data::Instance<AZ::RPI::Shader> m_shader;

        //! Shader for clearing stencil buffer (fullscreen triangle)
        AZ::Data::Instance<AZ::RPI::Shader> m_clearShader;
        AZ::RPI::Ptr<AZ::RPI::PipelineStateForDraw> m_clearStencilPipelineState;

        void CreatePipelineStates(PipelineStates& states, AZ::Data::Instance<AZ::RPI::Shader> shader);

        PipelineStates m_standard;
    };
}
