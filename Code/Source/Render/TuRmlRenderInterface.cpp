/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: Copyright (c) 2025 Reece Hagan
 *
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 */
#include "TuRmlRenderInterface.h"
#include "RmlBudget.h"
#include "TuRmlChildPass.h"

#include <AzCore/Console/ILogger.h>
#include <AzCore/Asset/AssetCommon.h>
#include <Atom/RPI.Reflect/Buffer/BufferAssetCreator.h>
#include <Atom/RPI.Public/Buffer/Buffer.h>
#include <Atom/RPI.Public/Image/ImageSystemInterface.h>
#include <Atom/RPI.Public/Image/StreamingImagePool.h>
#include <Atom/RHI/IndexBufferView.h>

#include <RmlUi/Core/Context.h>

#include <imgui/imgui.h>
#include <TuRml/TuRmlFeatureProcessorInterface.h>
#include <AzFramework/Entity/EntityContextBus.h>
#include <AzFramework/Entity/GameEntityContextBus.h>
#include <Atom/RPI.Public/Scene.h>

namespace TuRml
{
    void TuRmlStoredGeometry::ReleaseGeometry(Rml::CompiledGeometryHandle geoId)
    {
        auto geometry = reinterpret_cast<TuRmlStoredGeometry*>(geoId);

        if (geometry->storageType == StorageType::Persistent)
        {
            if (geometry->vertexBuffer)
            {
                geometry->vertexBuffer->inUse = false;
            }
            if (geometry->indexBuffer)
            {
                geometry->indexBuffer->inUse = false;
            }
        }

        // Transient geometry doesn't own buffers, just clear views
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

        const AZ::u64 texturesLeft = m_textureCreationCount;
        AZ_Error("TuRmlRenderInterface", texturesLeft == 0, "Still %zu textures left", texturesLeft);

        AZ_Info("TuRmlRenderInterface", "Destroyed render interface and released all resources");
    }

    void TuRmlRenderInterface::Begin(Rml::Context* ctx, TuRmlChildPass* pass)
    {
        AZ_Assert(m_pass == nullptr, "Begin already called!");
        // Clear any previous draw commands to start fresh
        m_createdThisFrame.clear();
        m_pass = pass;
        GetDrawCommands().clear();

        m_transform = AZ::Matrix4x4::CreateIdentity();

        const Rml::Vector2i dia = ctx->GetDimensions();

        auto ortho = Rml::Matrix4f::ProjectOrtho(
            0.0f,
            static_cast<float>(dia.x),
            static_cast<float>(dia.y),
            0.0f,
            -1000, 1000);

        m_contextTransform = AZ::Matrix4x4::CreateFromColumnMajorFloat16(reinterpret_cast<const float*>(&ortho));

        m_stencilRef = 1;
        SetTransform(nullptr);
    }

    void TuRmlRenderInterface::End()
    {
        AZ_PROFILE_FUNCTION(RmlBudget);

        // Detect transient geometry: geometry created AND queued for release in the same frame
        const auto& queuedFreeGeos = m_pass->m_drawCommands.Get().queuedFreeGeos;

        for (auto handle : queuedFreeGeos)
        {
            // If this geometry was created this frame AND is being released, it's transient
            if (m_createdThisFrame.contains(handle))
            {
                auto* geo = GetStoredGeometry(handle);
                if (geo && geo->storageType == TuRmlStoredGeometry::StorageType::Undecided)
                {
                    geo->storageType = TuRmlStoredGeometry::StorageType::Transient;
                }
            }
        }

        auto& drawCmds = GetDrawCommands();

        // Mark any remaining Undecided geometry as Persistent
        for (const auto& cmd : drawCmds)
        {
            auto* geo = GetStoredGeometry(cmd.drawCommand.geometryHandle);
            if (geo && geo->storageType == TuRmlStoredGeometry::StorageType::Undecided)
            {
                geo->storageType = TuRmlStoredGeometry::StorageType::Persistent;
            }
        }

        AllocateGPUBuffers();

        m_pass = nullptr;
    }

    void TuRmlRenderInterface::OnFinishedFrame(TuRmlChildPass* pass, AZ::u8 idx)
    {
        for (const auto& cmd : pass->m_drawCommands.Get(idx).drawCmds)
        {
            auto it = m_destroyedGeometries.find(cmd.drawCommand.geometryHandle);
            if (it != m_destroyedGeometries.end())
            {
                TuRmlStoredGeometry::ReleaseGeometry(*it);
                m_destroyedGeometries.erase(it);
            }
        }
    }

    TuRmlStoredGeometry* TuRmlRenderInterface::GetStoredGeometry(Rml::CompiledGeometryHandle handle)
    {
        if (!handle)
        {
            return nullptr;
        }

        return reinterpret_cast<TuRmlStoredGeometry*>(handle);
    }

    const TuRmlStoredTexture* TuRmlRenderInterface::GetStoredTexture(Rml::TextureHandle handle)
    {
        if (!handle)
        {
            return nullptr;
        }

        return reinterpret_cast<TuRmlStoredTexture*>(handle);
    }

#pragma region Rml::RenderInterface
    Rml::CompiledGeometryHandle TuRmlRenderInterface::CompileGeometry(Rml::Span<const Rml::Vertex> vertices,
                                                                      Rml::Span<const int> indices)
    {
        if (vertices.empty() || indices.empty())
        {
            return 0;
        }

        auto* storedGeo = aznew TuRmlStoredGeometry();

        storedGeo->vertices.assign(vertices.begin(), vertices.end());
        storedGeo->indices.assign(indices.begin(), indices.end());
        storedGeo->indexCount = static_cast<uint32_t>(indices.size());

        storedGeo->storageType = TuRmlStoredGeometry::StorageType::Undecided;
        storedGeo->creatorPass = m_pass;

        auto handle = reinterpret_cast<Rml::CompiledGeometryHandle>(storedGeo);

        m_createdThisFrame.insert(handle);

        return handle;
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
            drawCmd.clipmask_op = m_clipmaskOperation;
        }

        GetDrawCommands().push_back({drawCmd});
    }

    void TuRmlRenderInterface::ReleaseGeometry(Rml::CompiledGeometryHandle geometry)
    {
        if (!geometry)
        {
            return;
        }

        auto* storedGeo = GetStoredGeometry(geometry);
        AZ_Assert(storedGeo->creatorPass != nullptr,
                  "Trying to release geometry when no pass is present for a Undecided/Transient geo");
        storedGeo->creatorPass->m_drawCommands.Get().queuedFreeGeos.push_back(geometry);
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

        ++m_textureCreationCount;
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
        ++m_textureCreationCount;
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

        --m_textureCreationCount;
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

        auto& drawCmds = GetDrawCommands();

        const bool clearClipmask = (operation == Rml::ClipMaskOperation::Set || operation ==
            Rml::ClipMaskOperation::SetInverse);
        if (clearClipmask)
        {
            //Submit clear cmd
            TuRmlDrawCommand drawCmd;
            drawCmd.drawType = TuRmlDrawCommand::DrawType::ClearClipmask;
            drawCmds.push_back({drawCmd});
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

    AZStd::vector<TuRmlChildPassDrawCommand>& TuRmlRenderInterface::GetDrawCommands() const
    {
        return m_pass->m_drawCommands.Get().drawCmds;
    }

#pragma endregion

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

    void TuRmlRenderInterface::AllocateGPUBuffers()
    {
        AZ_PROFILE_FUNCTION(RmlBudget);
        auto& frameInfo = m_pass->m_drawCommands.Get();
        auto& drawCmds = frameInfo.drawCmds;

        // Calculate total transient geometry size
        size_t totalTransientVertices = 0;
        size_t totalTransientIndices = 0;

        for (const auto& cmd : drawCmds)
        {
            auto* geo = TuRmlRenderInterface::GetStoredGeometry(cmd.drawCommand.geometryHandle);
            if (geo && geo->storageType == TuRmlStoredGeometry::StorageType::Transient)
            {
                totalTransientVertices += geo->vertices.size();
                totalTransientIndices += geo->indices.size();
            }
        }

        if (totalTransientVertices > 0 && totalTransientIndices > 0)
        {
           frameInfo.EnsureTransientBufferCapacity(totalTransientVertices, totalTransientIndices);
        }

        // Allocate GPU buffers and upload data
        size_t transientVertexOffset = 0;
        size_t transientIndexOffset = 0;
        AZStd::vector<Rml::Vertex> transientVertexBuffer(totalTransientVertices);
        AZStd::vector<int> transientIndexBuffer(totalTransientIndices);

        for (const auto& cmd : drawCmds)
        {
            auto* geo = GetStoredGeometry(cmd.drawCommand.geometryHandle);
            if (!geo || geo->vertices.empty() || geo->indices.empty())
            {
                continue;
            }

            if (geo->storageType == TuRmlStoredGeometry::StorageType::Persistent)
            {
                const size_t vertexBytes = geo->vertices.size() * sizeof(Rml::Vertex);
                const size_t indexBytes = geo->indices.size() * sizeof(int);

                geo->vertexBuffer = RequestBuffer(vertexBytes, sizeof(Rml::Vertex));
                geo->indexBuffer = RequestBuffer(indexBytes, sizeof(int));

                if (geo->vertexBuffer && geo->indexBuffer)
                {
                    geo->vertexBuffer->buffer->UpdateData(geo->vertices.data(), vertexBytes);
                    geo->indexBuffer->buffer->UpdateData(geo->indices.data(), indexBytes);

                    geo->vertexBufferView = AZ::RHI::StreamBufferView(
                        *geo->vertexBuffer->buffer->GetRHIBuffer(),
                        0,
                        vertexBytes,
                        sizeof(Rml::Vertex)
                    );

                    geo->indexBufferView = AZ::RHI::IndexBufferView(
                        *geo->indexBuffer->buffer->GetRHIBuffer(),
                        0,
                        indexBytes,
                        AZ::RHI::IndexFormat::Uint32
                    );

                    geo->vertexBuffer->inUse = true;
                    geo->indexBuffer->inUse = true;

                    geo->vertices.clear();
                    geo->indices.clear();
                }
            }
            else if (geo->storageType == TuRmlStoredGeometry::StorageType::Transient)
            {
                geo->vertexOffsetInShared = transientVertexOffset;
                geo->indexOffsetInShared = transientIndexOffset;

                AZStd::copy(
                    geo->vertices.begin(),
                    geo->vertices.end(),
                    transientVertexBuffer.begin() + transientVertexOffset
                );

                AZStd::copy(
                    geo->indices.begin(),
                    geo->indices.end(),
                    transientIndexBuffer.begin() + transientIndexOffset
                    );

                transientVertexOffset += geo->vertices.size();
                transientIndexOffset += geo->indices.size();

                geo->vertexBufferView = AZ::RHI::StreamBufferView(
                    *frameInfo.m_sharedVertexBuffer->GetRHIBuffer(),
                    geo->vertexOffsetInShared * sizeof(Rml::Vertex),
                    geo->vertices.size() * sizeof(Rml::Vertex),
                    sizeof(Rml::Vertex)
                );

                geo->indexBufferView = AZ::RHI::IndexBufferView(
                    *frameInfo.m_sharedIndexBuffer->GetRHIBuffer(),
                    geo->indexOffsetInShared * sizeof(int),
                    geo->indices.size() * sizeof(int),
                    AZ::RHI::IndexFormat::Uint32
                );

                geo->vertices.clear();
                geo->indices.clear();
            }
        }

        if (transientVertexBuffer.size() > 0)
        {
            frameInfo.m_sharedVertexBuffer->UpdateData(
                    transientVertexBuffer.data(),
                    totalTransientVertices * sizeof(Rml::Vertex),
                    0
                );

            frameInfo.m_sharedIndexBuffer->UpdateData(
                transientIndexBuffer.data(),
                totalTransientIndices * sizeof(int),
                0
            );
        }
    }

    void TuRmlRenderInterface::OnImGuiUpdate()
    {
        ImGui::Begin("TuRml Render Interface");
        {
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
            ImGui::Text("Created This Frame: %zu geometries", m_createdThisFrame.size());

            AzFramework::EntityContextId ctxid;
            AzFramework::GameEntityContextRequestBus::BroadcastResult(
                ctxid, &AzFramework::GameEntityContextRequestBus::Events::GetGameEntityContextId);

            auto scene = AZ::RPI::Scene::GetSceneForEntityContextId(ctxid);
            auto fp = scene->GetFeatureProcessor<TuRmlFeatureProcessorInterface>();

            if (fp)
            {
                ImGui::Text("Child Pass Info:");
                fp->GetChildPasses([](TuRmlChildPass* child)
                {
                    ImGui::Text("ChildPass %s:", child->GetPathName().GetCStr());
                    for (const auto& frameInfo : child->m_drawCommands.m_drawCommands)
                    {
                        ImGui::Separator();
                        ImGui::Text("FrameInfo:");
                        ImGui::Text("Shared Vertex Buffer: %zu bytes", frameInfo.m_sharedIndexCapacity);
                        ImGui::Text("Shared Index Buffer: %zu bytes", frameInfo.m_sharedIndexCapacity);
                    }
                });
            }
        }

        ImGui::End();
    }
}
