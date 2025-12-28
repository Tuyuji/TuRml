/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: Copyright (c) 2025 Reece Hagan
 *
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 */
#include "TuRmlRenderInterface.h"

#include <AzCore/Console/ILogger.h>
#include <AzCore/Asset/AssetCommon.h>
#include <Atom/RPI.Reflect/Buffer/BufferAssetCreator.h>
#include <Atom/RPI.Public/Buffer/Buffer.h>
#include <Atom/RPI.Public/Image/ImageSystemInterface.h>
#include <Atom/RPI.Public/Image/StreamingImagePool.h>
#include <Atom/RHI/IndexBufferView.h>
#include <Atom/RHI/DeviceGeometryView.h>

#include <RmlUi/Core/Context.h>

#include <imgui/imgui.h>

namespace TuRml
{
    void TuRmlStoredGeometry::ReleaseGeometry(Rml::CompiledGeometryHandle geoId)
    {
        auto geometry = reinterpret_cast<TuRmlStoredGeometry*>(geoId);
        geometry->vertexBuffer->inUse = false;
        geometry->indexBuffer->inUse = false;
        geometry->vertexBufferView = {};
        geometry->indexBufferView = {};
        delete geometry;
    }

    TuRmlRenderInterface::TuRmlRenderInterface()
    {
        ImGui::ImGuiUpdateListenerBus::Handler::BusConnect();
    }

    TuRmlRenderInterface::~TuRmlRenderInterface()
    {
        ImGui::ImGuiUpdateListenerBus::Handler::BusDisconnect();

        // Clear any pending draw commands
        m_drawCommands.clear();

        AZStd::lock_guard<AZStd::mutex> lock(m_creationCountMutex);
        if (m_creationCount != 0)
        {
            AZ_Error("TuRmlRenderInterface", false, "Still %zu textures left", m_creationCount);
        }

        AZ_Info("TuRmlRenderInterface", "Destroyed render interface and released all resources");
    }

    Rml::CompiledGeometryHandle TuRmlRenderInterface::CompileGeometry(Rml::Span<const Rml::Vertex> vertices,
                                                                      Rml::Span<const int> indices)
    {
        if (vertices.empty() || indices.empty())
        {
            return 0;
        }
        auto* storedGeo = aznew TuRmlStoredGeometry();

        const auto vertexBytes = vertices.size() * sizeof(Rml::Vertex);
        const auto indexBytes = indices.size() * sizeof(int);

        storedGeo->indexCount = static_cast<uint32_t>(indices.size());
        storedGeo->vertexBuffer = RequestBuffer(vertexBytes, sizeof(Rml::Vertex));
        storedGeo->indexBuffer = RequestBuffer(indexBytes, sizeof(int));

        if (storedGeo->vertexBuffer == nullptr || storedGeo->indexBuffer == nullptr)
        {
            AZ_Error("TuRmlRenderInterface", false, "Failed to allocate geometry buffer");
            delete storedGeo;
            return 0;
        }

        storedGeo->vertexBuffer->buffer->UpdateData(vertices.data(), vertexBytes);
        storedGeo->indexBuffer->buffer->UpdateData(indices.data(), indexBytes);

        storedGeo->vertexBufferView = AZ::RHI::StreamBufferView(
            *storedGeo->vertexBuffer->buffer->GetRHIBuffer(),
            0,
            vertexBytes,
            sizeof(Rml::Vertex)
            );

        storedGeo->indexBufferView = AZ::RHI::IndexBufferView(
            *storedGeo->indexBuffer->buffer->GetRHIBuffer(),
            0,
            indexBytes,
            AZ::RHI::IndexFormat::Uint32
            );

        storedGeo->vertexBuffer->inUse = true;
        storedGeo->indexBuffer->inUse = true;
        return reinterpret_cast<Rml::CompiledGeometryHandle>(storedGeo);
    }

    void TuRmlRenderInterface::RenderGeometry(Rml::CompiledGeometryHandle geometry, Rml::Vector2f translation,
                                              Rml::TextureHandle texture)
    {
        if (!geometry)
        {
            return;
        }

        TuRmlDrawCommand drawCmd;
        drawCmd.geometryHandle = geometry;
        drawCmd.translation = AZ::Vector2(translation.x, translation.y);
        drawCmd.texture = texture;
        drawCmd.transform = m_transform;
        drawCmd.clipmaskEnabled = m_testClipMask;
        drawCmd.stencilRef = m_stencilRef;
        if (m_scissorEnabled)
        {
            drawCmd.scissorRegion = m_scissorRegion;
        }
        else
        {
            drawCmd.scissorRegion = {};
        }

        if (m_draw_to_clipmask)
        {
            drawCmd.drawType = TuRmlDrawCommand::DrawType::Clipmask;
            drawCmd.clipmask_op = m_clipmaskOperation;
        }
        else
        {
            drawCmd.drawType = TuRmlDrawCommand::DrawType::Normal;
        }

        m_drawCommands.push_back(drawCmd);
    }

    void TuRmlRenderInterface::ReleaseGeometry(Rml::CompiledGeometryHandle geometry)
    {
        if (!geometry)
        {
            return;
        }

        AZStd::lock_guard<AZStd::mutex> lock(m_queuedFreeMutex);
        m_queuedFree.push_back(geometry);
    }

    Rml::TextureHandle TuRmlRenderInterface::LoadTexture(Rml::Vector2i& texture_dimensions, const Rml::String& source)
    {
        AZ::Data::AssetId assetId;
        AZ::Data::AssetInfo assetInfo;
        AZ::Data::AssetCatalogRequestBus::BroadcastResult(
            assetId,
            &AZ::Data::AssetCatalogRequestBus::Events::GetAssetIdByPath,
            source.c_str(),
            azrtti_typeid<AZ::RPI::StreamingImageAsset>(), // Let it auto-detect asset type
            true
        );

        if (!assetId.IsValid())
        {
            AZ_Warning("TuRml", false, "Failed to find texture asset: %s", source.c_str());
            return 0;
        }

        // Load the texture asset
        auto imageAsset = AZ::Data::AssetManager::Instance().GetAsset<AZ::RPI::StreamingImageAsset>(
            assetId,
            AZ::Data::AssetLoadBehavior::PreLoad
        );

        imageAsset.BlockUntilLoadComplete();

        if (!imageAsset.IsReady())
        {
            AZ_Warning("TuRml", false, "Failed to load texture asset: %s", source.c_str());
            return 0;
        }

        // Get texture dimensions
        const AZ::RHI::ImageDescriptor& imageDesc = imageAsset->GetImageDescriptor();
        texture_dimensions.x = static_cast<int>(imageDesc.m_size.m_width);
        texture_dimensions.y = static_cast<int>(imageDesc.m_size.m_height);

        TuRmlStoredTexture* storedTex = aznew TuRmlStoredTexture();
        storedTex->textureAsset = imageAsset;

        storedTex->streamingImage = AZ::RPI::StreamingImage::FindOrCreate(imageAsset);

        if (!storedTex->streamingImage)
        {
            AZ_Warning("TuRml", false, "Failed to create StreamingImage from asset: %s", source.c_str());
            delete storedTex;
            return 0;
        }

        {
            AZStd::lock_guard<AZStd::mutex> lock(m_creationCountMutex);
            m_creationCount++;
        }
        return reinterpret_cast<Rml::TextureHandle>(storedTex);
    }

    Rml::TextureHandle TuRmlRenderInterface::GenerateTexture(Rml::Span<const Rml::byte> source,
                                                             Rml::Vector2i source_dimensions)
    {
        if (source.empty() || source_dimensions.x <= 0 || source_dimensions.y <= 0)
        {
            return 0;
        }

        TuRmlStoredTexture* storedTex = aznew TuRmlStoredTexture();
        storedTex->dimensions = AZ::PackedVector2i(source_dimensions.x, source_dimensions.y);

        AZ::RHI::Size imageSize;
        imageSize.m_width = aznumeric_cast<uint32_t>(source_dimensions.x);
        imageSize.m_height = aznumeric_cast<uint32_t>(source_dimensions.y);

        AZ::Data::Instance<AZ::RPI::StreamingImagePool> streamingImagePool = AZ::RPI::ImageSystemInterface::Get()->
            GetSystemStreamingPool();

        const uint32_t pixelDataSize = source_dimensions.x * source_dimensions.y * 4;

        AZStd::string textureName = AZStd::string::format("TuRml Texture #%p", storedTex);
        AZ::Uuid textureId = AZ::Uuid::CreateRandom();

        storedTex->streamingImage = AZ::RPI::StreamingImage::CreateFromCpuData(
            *streamingImagePool,
            AZ::RHI::ImageDimension::Image2D,
            imageSize,
            AZ::RHI::Format::R8G8B8A8_UNORM,
            source.data(),
            pixelDataSize,
            textureId
        );

        if (!storedTex->streamingImage)
        {
            AZ_Error("TuRmlRenderInterface", false, "Failed to create texture handle %p (%dx%d)", storedTex,
                     source_dimensions.x, source_dimensions.y);
            delete storedTex;
            return 0;
        }

        if (storedTex->streamingImage->GetRHIImage())
        {
            storedTex->streamingImage->GetRHIImage()->SetName(AZ::Name(textureName));
        }

        AZ_Info("TuRmlRenderInterface", "Created texture handle %p (%dx%d, %u bytes)", storedTex, source_dimensions.x,
                source_dimensions.y, pixelDataSize);
        {
            AZStd::lock_guard<AZStd::mutex> lock(m_creationCountMutex);
            m_creationCount++;
        }
        return reinterpret_cast<Rml::TextureHandle>(storedTex);
    }

    void TuRmlRenderInterface::ReleaseTexture(Rml::TextureHandle textureId)
    {
        if (!textureId)
        {
            return;
        }

        auto texture = reinterpret_cast<TuRmlStoredTexture*>(textureId);
        texture->streamingImage.reset();
        texture->textureAsset.Reset();

        {
            AZStd::lock_guard<AZStd::mutex> lock(m_creationCountMutex);
            m_creationCount--;
        }
        AZ_Info("TuRmlRenderInterface", "Released texture handle %p (index %zu)", texture, index);
        delete texture;
    }

    void TuRmlRenderInterface::EnableScissorRegion(bool enable)
    {
        m_scissorEnabled = enable;
    }

    void TuRmlRenderInterface::SetScissorRegion(Rml::Rectanglei region)
    {
        m_scissorRegion = region;
    }

    void TuRmlRenderInterface::SetTransform(const Rml::Matrix4f* transform)
    {
        if (transform)
        {
            m_transform = m_contextTransform * AZ::Matrix4x4::CreateFromColumnMajorFloat16(
                reinterpret_cast<const float*>(transform));
        }
        else
        {
            m_transform = m_contextTransform * AZ::Matrix4x4::CreateIdentity();
        }
    }

    void TuRmlRenderInterface::EnableClipMask(bool enable)
    {
        m_testClipMask = enable;
    }

    void TuRmlRenderInterface::RenderToClipMask(Rml::ClipMaskOperation operation, Rml::CompiledGeometryHandle geometry,
                                                Rml::Vector2f translation)
    {
        m_draw_to_clipmask = true;

        const bool clearClipmask = (operation == Rml::ClipMaskOperation::Set || operation ==
            Rml::ClipMaskOperation::SetInverse);
        if (clearClipmask)
        {
            //Submit clear cmd
            TuRmlDrawCommand drawCmd;
            drawCmd.drawType = TuRmlDrawCommand::DrawType::ClearClipmask;
            m_drawCommands.push_back(drawCmd);
        }

        auto oldStencilRef = m_stencilRef;

        m_stencilRef = 1;

        switch (operation)
        {
        case Rml::ClipMaskOperation::Set:
            oldStencilRef = 1;
            break;
        case Rml::ClipMaskOperation::SetInverse:
            oldStencilRef = 0;
            break;
        case Rml::ClipMaskOperation::Intersect:
            oldStencilRef += 1;
            break;
        }
        m_clipmaskOperation = operation;

        RenderGeometry(geometry, translation, {});

        m_stencilRef = oldStencilRef;
        m_draw_to_clipmask = false;
    }

    void TuRmlRenderInterface::Begin(Rml::Context* ctx)
    {
        // Clear any previous draw commands to start fresh
        m_drawCommands.clear();

        m_transform = AZ::Matrix4x4::CreateIdentity();

        const Rml::Vector2i dia = ctx->GetDimensions();

        auto ortho = Rml::Matrix4f::ProjectOrtho(
            0.0f,
            static_cast<float>(dia.x),
            static_cast<float>(dia.y),
            0.0f,
            -1000, 1000);
        /*Rml::Matrix4f correction_matrix;
        correction_matrix.SetColumns(Rml::Vector4f(1.0f, 0.0f, 0.0f, 0.0f), Rml::Vector4f(0.0f, -1.0f, 0.0f, 0.0f), Rml::Vector4f(0.0f, 0.0f, 0.5f, 0.0f),
                Rml::Vector4f(0.0f, 0.0f, 0.5f, 1.0f));
        auto finalProj = correction_matrix * ortho;*/
        m_contextTransform = AZ::Matrix4x4::CreateFromColumnMajorFloat16(reinterpret_cast<const float*>(&ortho));

        m_stencilRef = 1;
        SetTransform(nullptr);
    }

    AZStd::vector<TuRmlDrawCommand> TuRmlRenderInterface::End()
    {
        AZStd::vector<TuRmlDrawCommand> commands;
        commands.swap(m_drawCommands); // Move commands and clear the list
        // AZ_Info("TuRmlRenderInterface", "End collecting draw commands, returning %zu commands", commands.size());

        m_imguiData.m_cachedDrawCmds = commands;
        return commands;
    }

    TuRmlStoredGeometry* TuRmlRenderInterface::GetStoredGeometry(Rml::CompiledGeometryHandle handle) const
    {
        if (!handle)
        {
            return nullptr;
        }

        return reinterpret_cast<TuRmlStoredGeometry*>(handle);
    }

    const TuRmlStoredTexture* TuRmlRenderInterface::GetStoredTexture(Rml::TextureHandle handle) const
    {
        if (!handle)
        {
            return nullptr;
        }

        return reinterpret_cast<TuRmlStoredTexture*>(handle);
    }

    void TuRmlRenderInterface::ProcessClearQueue()
    {
        AZStd::lock_guard<AZStd::mutex> lock(m_queuedFreeMutex);
        for (auto h : m_queuedFree)
        {
            TuRmlStoredGeometry::ReleaseGeometry(h);
        }
        m_queuedFree.clear();

    }

    ReusableBuffer* TuRmlRenderInterface::RequestBuffer(size_t capacity, size_t elementSize)
    {
        const auto elementCount = capacity / elementSize;
        constexpr size_t MinElementCount = 32;
        if (elementCount < MinElementCount)
        {
            capacity = elementSize * MinElementCount;
        }

        auto it = std::lower_bound(
            m_buffers.begin(), m_buffers.end(),
            capacity,
            [](const AZStd::unique_ptr<ReusableBuffer>& lhs, size_t capacity)
            {
                return lhs->buffer->GetBufferSize() < capacity;
            });

        for (auto tmp_it = it; tmp_it != m_buffers.end(); ++tmp_it)
        {
            const auto& buffer = *tmp_it;
            if (buffer->buffer->GetBufferSize() >= capacity &&
                !buffer->inUse &&
                buffer->elementSize == elementSize)
            {
                return buffer.get();
            }
        }

        auto buffer = AZStd::make_unique<ReusableBuffer>();
        AZ::RPI::CommonBufferDescriptor desc;
        desc.m_poolType = AZ::RPI::CommonBufferPoolType::DynamicInputAssembly;
        desc.m_bufferName = "Rml Reusable Buffer #" + AZStd::to_string(m_buffers.size());
        desc.m_byteCount = capacity;
        desc.m_elementSize = elementSize;
        desc.m_bufferData = nullptr;

        buffer->buffer = AZ::RPI::BufferSystemInterface::Get()->CreateBufferFromCommonPool(desc);
        buffer->inUse = false;
        buffer->elementSize = elementSize;
        auto inserted_it = m_buffers.insert(it, AZStd::move(buffer));
        return inserted_it->get();
    }

    void TuRmlRenderInterface::OnImGuiUpdate()
    {
        ImGui::Begin("TuRml Render Interface");
        {
            AZStd::lock_guard<AZStd::mutex> lock(m_queuedFreeMutex);
            ImGui::Text("Queued Free Geos: %zu", m_queuedFree.size());
            ImGui::Text("Reusable Buffers: %zu", m_buffers.size());
            size_t inuseCount = 0;
            for (auto it = m_buffers.begin(); it != m_buffers.end(); ++it)
            {
                const auto& buffer = *it;
                if (buffer->inUse)
                {
                    inuseCount++;
                }
            }
            ImGui::Text("In Use Count: %zu", inuseCount);
        }

        ImGui::Text("Draw Commands: %zu", m_imguiData.m_cachedDrawCmds.size());
        ImGui::Separator();
        for (size_t i = 0; i < m_imguiData.m_cachedDrawCmds.size(); ++i)
        {
            const auto& cmd = m_imguiData.m_cachedDrawCmds[i];

            // Create tree node for each draw command
            AZStd::string nodeLabel;
            switch (cmd.drawType)
            {
            case TuRmlDrawCommand::DrawType::Normal:
                nodeLabel = AZStd::string::format("Draw %zu: Normal", i);
                break;
            case TuRmlDrawCommand::DrawType::Clipmask:
                nodeLabel = AZStd::string::format("Draw %zu: Clipmask", i);
                break;
            case TuRmlDrawCommand::DrawType::ClearClipmask:
                nodeLabel = AZStd::string::format("Draw %zu: Clear Clipmask", i);
                break;
            default:
                nodeLabel = AZStd::string::format("Draw %zu: Unknown", i);
                break;
            }

            if (ImGui::TreeNode(nodeLabel.c_str()))
            {
                // Basic command info
                ImGui::Text("Draw Type: %s",
                            cmd.drawType == TuRmlDrawCommand::DrawType::Normal
                                ? "Normal"
                                : cmd.drawType == TuRmlDrawCommand::DrawType::Clipmask
                                ? "Clipmask"
                                : cmd.drawType == TuRmlDrawCommand::DrawType::ClearClipmask
                                ? "Clear Clipmask"
                                : "Unknown");

                if (cmd.drawType != TuRmlDrawCommand::DrawType::ClearClipmask)
                {
                    // Geometry info
                    if (ImGui::TreeNode("Geometry"))
                    {
                        ImGui::Text("Handle: %p", (void*)(cmd.geometryHandle));
                        ImGui::TreePop();
                    }

                    // Transform info
                    if (ImGui::TreeNode("Transform"))
                    {
                        ImGui::Text("Translation: (%.2f, %.2f)", cmd.translation.GetX(), cmd.translation.GetY());

                        if (ImGui::TreeNode("Transform Matrix"))
                        {
                            const auto& m = cmd.transform;
                            ImGui::Text("| %.3f %.3f %.3f %.3f |", m.GetRow(0).GetX(), m.GetRow(0).GetY(),
                                        m.GetRow(0).GetZ(), m.GetRow(0).GetW());
                            ImGui::Text("| %.3f %.3f %.3f %.3f |", m.GetRow(1).GetX(), m.GetRow(1).GetY(),
                                        m.GetRow(1).GetZ(), m.GetRow(1).GetW());
                            ImGui::Text("| %.3f %.3f %.3f %.3f |", m.GetRow(2).GetX(), m.GetRow(2).GetY(),
                                        m.GetRow(2).GetZ(), m.GetRow(2).GetW());
                            ImGui::Text("| %.3f %.3f %.3f %.3f |", m.GetRow(3).GetX(), m.GetRow(3).GetY(),
                                        m.GetRow(3).GetZ(), m.GetRow(3).GetW());
                            ImGui::TreePop();
                        }
                        ImGui::TreePop();
                    }

                    // Texture info
                    if (ImGui::TreeNode("Texture"))
                    {
                        ImGui::Text("Handle: %p", reinterpret_cast<void*>(cmd.texture));

                        if (cmd.texture != 0)
                        {
                            if (auto storedTex = GetStoredTexture(cmd.texture))
                            {
                                    ImGui::Text("Dimensions: %dx%d", storedTex->dimensions.GetX(),
                                                storedTex->dimensions.GetY());
                                    ImGui::Text("StreamingImage: %s", storedTex->streamingImage ? "Valid" : "Invalid");
                            }
                            else
                            {
                                ImGui::Text("Texture: Not found");
                            }
                        }
                        else
                        {
                            ImGui::Text("No texture");
                        }
                        ImGui::TreePop();
                    }

                    // Clipping info
                    if (ImGui::TreeNode("Clipping"))
                    {
                        ImGui::Text("Clipmask Enabled: %s", cmd.clipmaskEnabled ? "Yes" : "No");
                        ImGui::Text("Stencil Ref: %u", cmd.stencilRef);

                        if (cmd.scissorRegion.p0.x != 0 || cmd.scissorRegion.p0.y != 0 ||
                            cmd.scissorRegion.p1.x != 0 || cmd.scissorRegion.p1.y != 0)
                        {
                            ImGui::Text("Scissor Region: (%d, %d) - (%d, %d)",
                                        cmd.scissorRegion.p0.x, cmd.scissorRegion.p0.y,
                                        cmd.scissorRegion.p1.x, cmd.scissorRegion.p1.y);
                            ImGui::Text("Scissor Size: %dx%d",
                                        cmd.scissorRegion.p1.x - cmd.scissorRegion.p0.x,
                                        cmd.scissorRegion.p1.y - cmd.scissorRegion.p0.y);
                        }
                        else
                        {
                            ImGui::Text("No scissor region");
                        }
                        ImGui::TreePop();
                    }

                    // Clipmask operation details
                    if (cmd.drawType == TuRmlDrawCommand::DrawType::Clipmask)
                    {
                        if (ImGui::TreeNode("Clipmask Operation"))
                        {
                            const char* opName = "Unknown";
                            switch (cmd.clipmask_op)
                            {
                            case Rml::ClipMaskOperation::Set: opName = "Set";
                                break;
                            case Rml::ClipMaskOperation::SetInverse: opName = "SetInverse";
                                break;
                            case Rml::ClipMaskOperation::Intersect: opName = "Intersect";
                                break;
                            }
                            ImGui::Text("Operation: %s", opName);
                            ImGui::TreePop();
                        }
                    }
                }

                ImGui::TreePop();
            }
        }

        ImGui::End();

        m_imguiData.m_cachedDrawCmds.clear();
    }
}
