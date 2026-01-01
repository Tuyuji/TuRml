/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: Copyright (c) 2025 Reece Hagan
 *
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 */
#pragma once

#include <AzCore/Math/Vector2.h>
#include <AzCore/Math/PackedVector2.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/Asset/AssetCommon.h>
#include <AzCore/Math/Matrix4x4.h>
#include <AzCore/std/parallel/mutex.h>
#include <Atom/RPI.Reflect/Buffer/BufferAsset.h>
#include <Atom/RPI.Public/Buffer/Buffer.h>
#include <Atom/RPI.Public/Image/StreamingImage.h>
#include <Atom/RHI/IndexBufferView.h>
#include <Atom/RHI/StreamBufferView.h>

#include <RmlUi/Core/RenderInterface.h>

#include <TuRml/Allocators.h>

#include <ImGuiBus.h>

namespace TuRml
{
    class TuRmlChildPass;

    struct ReusableBuffer
    {
        AZ_CLASS_ALLOCATOR(ReusableBuffer, TuRmlRenderAllocator);
        AZ::Data::Instance<AZ::RPI::Buffer> buffer = {};
        size_t elementSize = 0;
        bool inUse = false;
    };

    //! Stored geometry data for compiled RmlUi geometry
    struct TuRmlStoredGeometry
    {
        AZ_CLASS_ALLOCATOR(TuRmlStoredGeometry, TuRmlRenderAllocator);
        size_t indexCount = 0;

        // Persistent buffer assets and instances
        ReusableBuffer* vertexBuffer = nullptr;
        ReusableBuffer* indexBuffer = nullptr;

        // Pre-created buffer views for rendering
        AZ::RHI::StreamBufferView vertexBufferView = {};
        AZ::RHI::IndexBufferView indexBufferView = {};

        static void ReleaseGeometry(Rml::CompiledGeometryHandle geometry);
    };

    //! Stored texture data for RmlUi textures
    struct TuRmlStoredTexture
    {
        AZ_CLASS_ALLOCATOR(TuRmlStoredTexture, TuRmlRenderAllocator);
        AZ::Data::Instance<AZ::RPI::StreamingImage> streamingImage = {};
        AZ::PackedVector2i dimensions = AZ::PackedVector2i();

        AZ::Data::Asset<AZ::RPI::StreamingImageAsset> textureAsset = {};
    };

    //! Collected draw command from RmlUi rendering
    struct TuRmlDrawCommand
    {
        Rml::CompiledGeometryHandle geometryHandle;
        AZ::Vector2 translation;
        Rml::TextureHandle texture = 0;

        AZ::Matrix4x4 transform = AZ::Matrix4x4::CreateIdentity();

        Rml::Rectanglei scissorRegion;
        bool clipmaskEnabled = false;
        uint8_t stencilRef = 0;

        enum class DrawType
        {
            Normal,
            Clipmask,

            //No arguments needed, just clear clipmask
            ClearClipmask,
        };

        DrawType drawType = DrawType::Normal;
        Rml::ClipMaskOperation clipmask_op;
    };

    class TuRmlRenderInterface
        : public Rml::RenderInterface
        , public ImGui::ImGuiUpdateListenerBus::Handler
    {
    public:
        TuRmlRenderInterface();
        ~TuRmlRenderInterface() override;

        void Begin(Rml::Context* ctx);
        AZStd::vector<TuRmlDrawCommand> End();

        TuRmlStoredGeometry* GetStoredGeometry(Rml::CompiledGeometryHandle handle) const;
        const TuRmlStoredTexture* GetStoredTexture(Rml::TextureHandle handle) const;

#pragma region Rml::RenderInterface
        //begin Rml::RenderInterface
        // Required functions for basic rendering
        Rml::CompiledGeometryHandle CompileGeometry(Rml::Span<const Rml::Vertex> vertices, Rml::Span<const int> indices) override;
        void RenderGeometry(Rml::CompiledGeometryHandle geometry, Rml::Vector2f translation,
                            Rml::TextureHandle texture) override;
        void ReleaseGeometry(Rml::CompiledGeometryHandle geometry) override;

        Rml::TextureHandle LoadTexture(Rml::Vector2i& texture_dimensions, const Rml::String& source) override;
        Rml::TextureHandle GenerateTexture(Rml::Span<const Rml::byte> source, Rml::Vector2i source_dimensions) override;
        void ReleaseTexture(Rml::TextureHandle texture) override;

        void EnableScissorRegion(bool enable) override;
        void SetScissorRegion(Rml::Rectanglei region) override;

        //Semi-advanced rendering
        void SetTransform(const Rml::Matrix4f* transform) override;

        void EnableClipMask(bool enable) override;
        void RenderToClipMask(Rml::ClipMaskOperation operation, Rml::CompiledGeometryHandle geometry,
                              Rml::Vector2f translation) override;
#pragma endregion
    private:
        friend class TuRmlChildPass;

        void ProcessClearQueue();
        ReusableBuffer* RequestBuffer(size_t capacity, size_t elementSize);

        AZStd::vector<AZStd::unique_ptr<ReusableBuffer>> m_buffers;
        AZStd::atomic_uint64_t m_textureCreationCount = 0;

        AZStd::mutex m_queuedFreeMutex;
        AZStd::vector<Rml::CompiledGeometryHandle> m_queuedFree;

        //Per frame:
        //! Current draw commands being collected
        AZStd::vector<TuRmlDrawCommand> m_drawCommands;
        AZ::Matrix4x4 m_transform;
        AZ::Matrix4x4 m_contextTransform;
        Rml::Rectanglei m_scissorRegion;
        Rml::ClipMaskOperation m_clipmaskOperation;
        uint8_t m_stencilRef = 0;
        bool m_scissorEnabled = false;
        bool m_draw_to_clipmask = false;
        bool m_testClipMask = false;

        //ImGui
        void OnImGuiUpdate() override;
    };
}
