/*
* SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: Copyright (c) 2025 Reece Hagan
 *
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 */
#pragma once

#include <RmlUi/Core/FileInterface.h>

namespace TuRml
{
   class TuFile final
        : public Rml::FileInterface
   {
   public:
       void Init();
       void Shutdown();

       //File interface
       Rml::FileHandle Open(const Rml::String& path) override;
       void Close(Rml::FileHandle file) override;
       size_t Read(void* buffer, size_t size, Rml::FileHandle file) override;
       bool Seek(Rml::FileHandle file, long offset, int origin) override;
       size_t Tell(Rml::FileHandle file) override;
       size_t Length(Rml::FileHandle file) override;
       bool LoadFile(const Rml::String& path, Rml::String& out_data) override;
   };
}