/*
* SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: Copyright (c) 2025 Reece Hagan
 *
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 */
#pragma once

#include <RmlUi/Core/SystemInterface.h>

namespace TuRml
{
    class TuSystem final
        : public Rml::SystemInterface
    {
    public:
        void Init();
        void Shutdown();

        //Keeping the functions from SystemInterface here commented out with details on why it isn't overridden.

        //Don't really see a need to override it
        //double GetElapsedTime() override;

        //No standard translation system in O3DE :(
        // int TranslateString(Rml::String& translated, const Rml::String& input) override;

        //Default implementation works well for us
        // void JoinPath(Rml::String& translated_path, const Rml::String& document_path, const Rml::String& path) override;

        bool LogMessage(Rml::Log::Type type, const Rml::String& message) override;

        //InputDeviceMouse doesn't expose this, we'd need to draw our own cursor
        //I don't want to do that as it can feel laggy, id prefer setting the system cursors icon instead.
        // void SetMouseCursor(const Rml::String& cursor_name) override;

        //Default Rml implementation just keeps an internal clipboard, would be nice to get access
        //to the systems, maybe have our own implementation in our Platform folder?
        // void SetClipboardText(const Rml::String& text) override;
        // void GetClipboardText(Rml::String& text) override;

        //No idea how to test this but leaving it here.
        // void ActivateKeyboard(Rml::Vector2f caret_position, float line_height) override;
        // void DeactivateKeyboard() override;
    protected:

    private:
    };
}