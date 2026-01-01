/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: Copyright (c) 2025 Reece Hagan
 *
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 */

#include "TuFile.h"

#include <AzCore/IO/FileIO.h>
#include <AzCore/Asset/AssetManager.h>

#include <RmlUi/Core/Core.h>

using namespace TuRml;

void TuFile::Init()
{
    Rml::SetFileInterface(this);
}

void TuFile::Shutdown()
{
}

Rml::FileHandle TuFile::Open(const Rml::String& path)
{
    if (path.empty())
    {
        return 0;
    }

    AZ::Data::AssetId assetId;
    AZ::Data::AssetInfo info;
    AZ::Data::AssetCatalogRequestBus::BroadcastResult(
        assetId,
        &AZ::Data::AssetCatalogRequestBus::Events::GetAssetIdByPath,
        path.c_str(),
        AZ::Uuid::CreateNull(),
        true
    );

    if (!assetId.IsValid())
    {
        AZ_Warning("TuRml", false, "Failed to find asset for path: %s", path.c_str());
        return 0;
    }

    AZ::Data::AssetCatalogRequestBus::BroadcastResult(
        info,
        &AZ::Data::AssetCatalogRequestBus::Events::GetAssetInfoById,
        assetId
    );

    AZ::IO::FileIOStream* f = aznew AZ::IO::FileIOStream(info.m_relativePath.c_str(),
                                                         AZ::IO::OpenMode::ModeRead | AZ::IO::OpenMode::ModeBinary);
    if (!f->IsOpen())
    {
        return 0;
    }

    return reinterpret_cast<Rml::FileHandle>(f);
}

void TuFile::Close(Rml::FileHandle file)
{
    auto* storedFile = reinterpret_cast<AZ::IO::FileIOStream*>(file);
    if (!storedFile)
    {
        return;
    }

    storedFile->Close();

    azfree(storedFile);
}

size_t TuFile::Read(void* buffer, size_t size, Rml::FileHandle file)
{
    if (!file || !buffer || size == 0)
    {
        return 0;
    }

    auto* storedFile = reinterpret_cast<AZ::IO::FileIOStream*>(file);

    return storedFile->Read(size, buffer);
}

bool TuFile::Seek(Rml::FileHandle file, long offset, int origin)
{
    if (!file)
    {
        return false;
    }

    auto* storedFile = reinterpret_cast<AZ::IO::FileIOStream*>(file);

    switch (origin)
    {
    case SEEK_SET:
        storedFile->Seek(offset, AZ::IO::GenericStream::ST_SEEK_BEGIN);
        break;
    case SEEK_CUR:
        storedFile->Seek(offset, AZ::IO::GenericStream::ST_SEEK_CUR);
        break;
    case SEEK_END:
        storedFile->Seek(offset, AZ::IO::GenericStream::ST_SEEK_END);
        break;
    default:
        return false;
    }

    return true;
}

size_t TuFile::Tell(Rml::FileHandle file)
{
    if (!file)
    {
        return 0;
    }

    auto* storedFile = reinterpret_cast<AZ::IO::FileIOStream*>(file);

    return storedFile->GetCurPos();
}

size_t TuFile::Length(Rml::FileHandle file)
{
    if (!file)
    {
        return 0;
    }

    auto* storedFile = reinterpret_cast<AZ::IO::FileIOStream*>(file);
    return storedFile->GetLength();
}

bool TuFile::LoadFile(const Rml::String& path, Rml::String& out_data)
{
    auto file = Open(path);
    if (!file)
    {
        return false;
    }

    auto len = Length(file);
    out_data.resize(len);
    Read(out_data.data(), len, file);
    Close(file);
    return true;
}
