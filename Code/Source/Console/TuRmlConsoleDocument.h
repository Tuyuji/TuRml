/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: Copyright (c) 2025 Reece Hagan
 *
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 */
#pragma once

#include <AzCore/std/string/string.h>
#include <AzCore/std/containers/deque.h>
#include <AzCore/std/parallel/mutex.h>
#include <AzCore/Math/Color.h>
#include <AzCore/Console/Console.h>
#include <AzCore/Console/ILogger.h>
#include <AzCore/Debug/TraceMessageBus.h>
#include <AzCore/Component/TickBus.h>
#include <RmlUi/Core/ElementDocument.h>

#include "RmlUi/Core/EventListener.h"

namespace Rml
{
    class Context;
}

namespace TuRml
{
    class TuRmlConsoleDocument
        : protected Rml::EventListener
        , protected AZ::SystemTickBus::Handler
        , protected AZ::Debug::TraceMessageBus::Handler
    {
    public:
        AZ_CLASS_ALLOCATOR(TuRmlConsoleDocument, AZ::SystemAllocator);

        // Initialize and shutdown
        bool Initialize(Rml::Context* ctx, const char* rmlPath);
        ~TuRmlConsoleDocument() override;

        // TraceMessageBus interface
        bool OnPreError(const char* window, const char* fileName, int line,
                       const char* func, const char* message) override;
        bool OnPreWarning(const char* window, const char* fileName, int line,
                         const char* func, const char* message) override;
        bool OnPrintf(const char* window, const char* message) override;

        void OnCommandInput(const AZStd::string& command);
        void OnClearLogs();
        void OnHistoryUp();
        void OnHistoryDown();
        void OnAutoComplete(const AZStd::string& partial);

        void OnSystemTick() override
        {
            UpdateLogDisplay();
        }
        void ProcessEvent(Rml::Event& event) override;
    private:

        void SetLogHTML(const AZStd::string& html);
        void ScrollToBottom();
        void SetInputText(const AZStd::string& text);
        AZStd::string GetInputText();
        void FocusInput();

        void SetupEventListeners();

        struct LogEntry
        {
            AZStd::string message;
            AZ::Color color;
        };

        void AddLog(const char* window, const char* message, AZ::LogLevel level);
        AZ::Color GetColorForLogLevel(AZ::LogLevel level) const;
        void RebuildLogHTML();
        void UpdateLogDisplay();

        Rml::ElementDocument* m_doc = nullptr;
        AZStd::deque<LogEntry> m_logEntries = {};
        AZStd::mutex m_logMutex = {};
        static constexpr size_t MaxLogEntries = 1024;
        bool m_logsDirty = false;

        AZStd::deque<AZStd::string> m_commandHistory = {};
        int m_historyIndex = -1;
        static constexpr size_t MaxHistorySize = 512;

        bool m_autoScroll = true;
    };
} // namespace TuRml