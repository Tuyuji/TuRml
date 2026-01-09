/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: Copyright (c) 2025 Reece Hagan
 *
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 */
#include "TuRmlParentPass.h"
#include <AzCore/Name/Name.h>
#include <Atom/RPI.Public/Pass/PassSystemInterface.h>

namespace TuRml
{
    AZ::RPI::Ptr<TuRmlParentPass> TuRmlParentPass::Create(const AZ::RPI::PassDescriptor& descriptor)
    {
        return aznew TuRmlParentPass(descriptor);
    }


    TuRmlParentPass::TuRmlParentPass(const AZ::RPI::PassDescriptor& descriptor)
        : ParentPass(descriptor)
    {
    }

    void TuRmlParentPass::UpdateRenderTarget(Rml::Context* context,
                                             AZ::Data::Instance<AZ::RPI::AttachmentImage> attachmentImage)
    {
        if (!context || !attachmentImage)
        {
            return;
        }

        //Do we have it?
        bool bExists = m_contextPasses.find(context) != m_contextPasses.end();
        if (!bExists)
        {
            m_contextPasses[context] = {nullptr, attachmentImage, false}; // render target mode
            QueueForBuildAndInitialization();
            return;
        }

        auto& contextData = m_contextPasses[context];

        // Check if we need to switch from direct pipeline mode to render target mode
        if (contextData.m_isDirectPipelineMode)
        {
            SwitchContextMode(context, false, attachmentImage);
            return;
        }

        //Do we need to update it?
        if (contextData.m_renderTarget != attachmentImage)
        {
            contextData.m_renderTarget = attachmentImage;
            if (contextData.m_childPass)
            {
                contextData.m_childPass->UpdateRenderTarget(attachmentImage);
            }
            return;
        }
    }

    void TuRmlParentPass::SetDirectPipelineMode(Rml::Context* context)
    {
        if (!context)
        {
            return;
        }

        //Do we have it?
        bool bExists = m_contextPasses.find(context) != m_contextPasses.end();
        if (!bExists)
        {
            m_contextPasses[context] = {nullptr, nullptr, true}; // direct pipeline mode, no render target
            QueueForBuildAndInitialization();
            return;
        }

        auto& contextData = m_contextPasses[context];

        // Check if we need to switch from render target mode to direct pipeline mode
        if (!contextData.m_isDirectPipelineMode)
        {
            SwitchContextMode(context, true, nullptr);
            return;
        }
    }

    void TuRmlParentPass::BuildInternal()
    {
        for (auto& [context, data] : m_contextPasses)
        {
            if (data.m_childPass == nullptr)
            {
                if (data.m_isDirectPipelineMode)
                {
                    AddDirectPipelineChildPassForContext(context);
                }
                else
                {
                    AddChildPassForContext(context, data.m_renderTarget);
                }
            }
        }

        ParentPass::BuildInternal();
    }

    void TuRmlParentPass::CreateChildPassesInternal()
    {
        // Child passes are created in BuildInternal
    }

    void TuRmlParentPass::AddChildPassForContext(Rml::Context* context,
                                                 AZ::Data::Instance<AZ::RPI::AttachmentImage> attachmentImage)
    {
        if (!context || !attachmentImage)
        {
            return;
        }

        auto contextName = context->GetName().c_str();

        // Create a unique name for this child pass
        AZStd::string passName = AZStd::string::format("TuRmlChildPass_%s", contextName);

        // Create the child pass from template
        AZ::RPI::PassSystemInterface* passSystem = AZ::RPI::PassSystemInterface::Get();
        AZ::RPI::Ptr<TuRmlChildPass> childPass = azrtti_cast<TuRmlChildPass*>(
            passSystem->CreatePassFromTemplate(AZ::Name("TuRmlChildPassTemplate"), AZ::Name(passName)).get()
        );

        if (childPass)
        {
            // Set the attachment image for this child pass
            childPass->UpdateRenderTarget(attachmentImage);

            // Add as child
            AddChild(childPass);
            m_contextPasses[context].m_childPass = childPass;
            m_contextPasses[context].m_renderTarget = attachmentImage;
            m_contextPasses[context].m_isDirectPipelineMode = false;

            AZ_Info("TuRmlParentPass", "Created render target child pass '%s' for context %s", passName.c_str(),
                    contextName);
        }
        else
        {
            AZ_Error("TuRmlParentPass", false, "Failed to create TuRmlChildPass from template");
        }
    }

    void TuRmlParentPass::AddDirectPipelineChildPassForContext(Rml::Context* context)
    {
        if (!context)
        {
            return;
        }

        auto contextName = context->GetName().c_str();

        AZStd::string passName = AZStd::string::format("TuRmlDirectPipelineChildPass_%s", contextName);

        AZ::RPI::PassSystemInterface* passSystem = AZ::RPI::PassSystemInterface::Get();
        AZ::RPI::Ptr<TuRmlChildPass> childPass = azrtti_cast<TuRmlChildPass*>(
            passSystem->CreatePassFromTemplate(AZ::Name("TuRmlChildPassDirectTemplate"), AZ::Name(passName)).get()
        );

        if (childPass)
        {
            childPass->SetDirectPipelineMode();

            InsertChild(childPass, 1);
            m_contextPasses[context].m_childPass = childPass;
            m_contextPasses[context].m_renderTarget = nullptr;
            m_contextPasses[context].m_isDirectPipelineMode = true;

            AZ_Info("TuRmlParentPass", "Created direct pipeline child pass '%s' for context %s", passName.c_str(),
                    contextName);
        }
        else
        {
            AZ_Error("TuRmlParentPass", false,
                     "Failed to create TuRmlChildPass from template for direct pipeline mode");
        }
    }

    void TuRmlParentPass::RemoveChildPass(Rml::Context* context)
    {
        if (!context)
        {
            return;
        }

        auto it = m_contextPasses.find(context);
        if (it != m_contextPasses.end())
        {
            it->second.m_childPass->QueueForRemoval();
            m_contextPasses.erase(it);
            return;
        }

        AZ_Warning("TuRmlParentPass", false, "Failed to find child pass for context %p", context);
    }

    AZ::RPI::Ptr<TuRmlChildPass> TuRmlParentPass::GetChildPass(Rml::Context* context) const
    {
        if (!context)
        {
            return nullptr;
        }

        return m_contextPasses.find(context)->second.m_childPass;
    }

    void TuRmlParentPass::SwitchContextMode(Rml::Context* context, bool isDirectPipeline,
                                            AZ::Data::Instance<AZ::RPI::AttachmentImage> renderTarget)
    {
        auto& contextData = m_contextPasses[context];

        // Remove existing child pass
        if (contextData.m_childPass)
        {
            RemoveChild(contextData.m_childPass);
            contextData.m_childPass = nullptr;
        }

        // Update mode and render target
        contextData.m_isDirectPipelineMode = isDirectPipeline;
        contextData.m_renderTarget = renderTarget;

        QueueForBuildAndInitialization();
    }
}
