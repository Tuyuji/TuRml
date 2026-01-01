/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: Copyright (c) 2025 Reece Hagan
 *
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 */
#include "TuRmlConsoleDocument.h"

#include <AzCore/IO/Path/Path.h>
#include <AzCore/Interface/Interface.h>

#include <RmlUi/Core/Core.h>
#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/Event.h>
#include <RmlUi/Core/EventListener.h>
#include <RmlUi/Core/Input.h>
#include <RmlUi/Core/Elements/ElementFormControlInput.h>
#include <RmlUi/Core/ElementText.h>

#include "RmlUi/Core/Factory.h"

namespace TuRml
{
    bool TuRmlConsoleDocument::Initialize(Rml::Context* ctx, const char* rmlPath)
    {
        m_doc = ctx->LoadDocument(rmlPath);
        if (!m_doc)
            return false;

        SetupEventListeners();
        AZ::SystemTickBus::Handler::BusConnect();
        AZ::Debug::TraceMessageBus::Handler::BusConnect();
        return true;
    }

    TuRmlConsoleDocument::~TuRmlConsoleDocument()
    {
        AZ::SystemTickBus::Handler::BusDisconnect();
        AZ::Debug::TraceMessageBus::Handler::BusDisconnect();
        {
            AZStd::lock_guard<AZStd::mutex> lock(m_logMutex);
            m_logEntries.clear();
        }

        m_commandHistory.clear();
    }

    void TuRmlConsoleDocument::ScrollToBottom()
    {
        Rml::Element* logContainer = m_doc->GetElementById("log_container");
        if (logContainer)
        {
            Rml::ScrollIntoViewOptions options{};
            options.vertical = Rml::ScrollAlignment::Nearest;
            options.horizontal = Rml::ScrollAlignment::Nearest;
            options.behavior = Rml::ScrollBehavior::Smooth;
            logContainer->GetLastChild()->ScrollIntoView(options);
        }
    }

    void TuRmlConsoleDocument::SetInputText(const AZStd::string& text)
    {
        Rml::Element* inputElement = m_doc->GetElementById("console_input");
        if (inputElement)
        {
            inputElement->SetAttribute("value", text.c_str());
            if (auto* i = rmlui_dynamic_cast<Rml::ElementFormControlInput*>(inputElement))
            {
                i->SetSelectionRange(text.size(), text.size());
            }
        }
    }

    AZStd::string TuRmlConsoleDocument::GetInputText()
    {
        Rml::Element* inputElement = m_doc->GetElementById("console_input");
        if (inputElement)
        {
            return inputElement->GetAttribute<Rml::String>("value", "").c_str();
        }
        return "";
    }

    void TuRmlConsoleDocument::FocusInput()
    {
        Rml::Element* inputElement = m_doc->GetElementById("console_input");
        if (inputElement)
        {
            inputElement->Focus();
        }
    }

    void TuRmlConsoleDocument::SetupEventListeners()
    {
        m_doc->AddEventListener(Rml::EventId::Show, this);
        m_doc->AddEventListener(Rml::EventId::Hide, this);

        // Input field event listener
        Rml::Element* inputElement = m_doc->GetElementById("console_input");
        if (inputElement)
        {
            inputElement->AddEventListener(Rml::EventId::Keydown, this);
        }

        // Clear button event listener
        Rml::Element* clearButton = m_doc->GetElementById("clear_button");
        if (clearButton)
        {
            clearButton->AddEventListener(Rml::EventId::Click, this);
        }
    }

    bool TuRmlConsoleDocument::OnPreError(const char* window, const char* fileName, int line,
                                          const char* func, const char* message)
    {
        AddLog(window, message, AZ::LogLevel::Error);
        return false;
    }

    bool TuRmlConsoleDocument::OnPreWarning(const char* window, const char* fileName, int line,
                                            const char* func, const char* message)
    {
        AddLog(window, message, AZ::LogLevel::Warn);
        return false;
    }

    bool TuRmlConsoleDocument::OnPrintf(const char* window, const char* message)
    {
        AddLog(window, message, AZ::LogLevel::Notice);
        return false;
    }

    void TuRmlConsoleDocument::OnCommandInput(const AZStd::string& command)
    {
        if (command.empty())
        {
            return;
        }

        m_commandHistory.push_back(command);
        if (m_commandHistory.size() > MaxHistorySize)
        {
            m_commandHistory.pop_front();
        }
        m_historyIndex = -1;

        if (command == "sreload")
        {
            //Style reload
            const auto numCtx = Rml::GetNumContexts();
            for (int i = 0; i < numCtx; ++i)
            {
                auto ctx = Rml::GetContext(i);
                if (!ctx)
                    continue;

                const auto numDocs = ctx->GetNumDocuments();
                for (int j = 0; j < numDocs; ++j)
                {
                    auto doc = ctx->GetDocument(j);
                    if (!doc)
                        continue;

                    doc->ReloadStyleSheet();
                }
            }
            return;
        }

        AZ::IConsole* console = AZ::Interface<AZ::IConsole>::Get();
        if (console)
        {
            auto result = console->PerformCommand(command.c_str());
            if (!result.IsSuccess())
            {
                AddLog("Console", "Failed to execute cmd.", AZ::LogLevel::Notice);
            }
        }
        else
        {
            AddLog("Console", "Error: IConsole interface not available", AZ::LogLevel::Error);
        }
    }

    void TuRmlConsoleDocument::OnClearLogs()
    {
        AZStd::lock_guard<AZStd::mutex> lock(m_logMutex);
        m_logEntries.clear();
        m_logsDirty = true;
    }

    void TuRmlConsoleDocument::OnHistoryUp()
    {
        if (m_commandHistory.empty())
        {
            return;
        }

        if (m_historyIndex < 0)
        {
            m_historyIndex = static_cast<int>(m_commandHistory.size()) - 1;
        }
        else if (m_historyIndex > 0)
        {
            --m_historyIndex;
        }

        if (m_historyIndex >= 0 && m_historyIndex < static_cast<int>(m_commandHistory.size()))
        {
            SetInputText(m_commandHistory[m_historyIndex]);
        }
    }

    void TuRmlConsoleDocument::OnHistoryDown()
    {
        if (m_commandHistory.empty() || m_historyIndex < 0)
        {
            return;
        }

        if (m_historyIndex < static_cast<int>(m_commandHistory.size()) - 1)
        {
            ++m_historyIndex;
            SetInputText(m_commandHistory[m_historyIndex]);
        }
        else
        {
            m_historyIndex = -1;
            SetInputText(""); // Clear input
        }
    }

    void TuRmlConsoleDocument::OnAutoComplete(const AZStd::string& partial)
    {
        AZ::IConsole* console = AZ::Interface<AZ::IConsole>::Get();
        if (!console || partial.empty())
        {
            return;
        }

        AZStd::vector<AZStd::string> matches;
        AZStd::string longestMatch = console->AutoCompleteCommand(partial.c_str(), &matches);

        if (!longestMatch.empty())
        {
            SetInputText(longestMatch);
        }
    }

    void TuRmlConsoleDocument::ProcessEvent(Rml::Event& event)
    {
        if (event.GetTargetElement() == m_doc)
        {
            if (event.GetId() == Rml::EventId::Show)
            {
                FocusInput();
                ScrollToBottom();
            }
            else if (event.GetId() == Rml::EventId::Hide)
            {
                Rml::Element* inputElement = m_doc->GetElementById("console_input");
                if (inputElement) inputElement->Blur();
            }
            return;
        }
        if (event.GetId() == Rml::EventId::Click && event.GetTargetElement()->GetId() == "clear_button")
        {
            OnClearLogs();
            event.StopPropagation();
            return;
        }
        if (event.GetId() == Rml::EventId::Keydown && event.GetTargetElement()->GetId() == "console_input")
        {
            int keyIdentifier = event.GetParameter<int>("key_identifier", 0);

            if (keyIdentifier == Rml::Input::KI_RETURN)
            {
                // Get input text
                Rml::Element* inputElement = event.GetTargetElement();
                if (inputElement)
                {
                    Rml::String text = inputElement->GetAttribute<Rml::String>("value", "");
                    if (!text.empty())
                    {
                        OnCommandInput(text.c_str());
                        inputElement->SetAttribute("value", "");
                    }
                }
            }
            else if (keyIdentifier == Rml::Input::KI_UP)
            {
                event.StopImmediatePropagation();
                OnHistoryUp();
            }
            else if (keyIdentifier == Rml::Input::KI_DOWN)
            {
                event.StopImmediatePropagation();
                OnHistoryDown();
            }
            else if (keyIdentifier == Rml::Input::KI_TAB)
            {
                // Get input text for auto-completion
                Rml::Element* inputElement = event.GetTargetElement();
                if (inputElement)
                {
                    Rml::String text = inputElement->GetAttribute<Rml::String>("value", "");
                    OnAutoComplete(text.c_str());
                }
                event.StopPropagation();
            }
        }
    }

    // Log management
    void TuRmlConsoleDocument::AddLog(const char* window, const char* message, AZ::LogLevel level)
    {
        AZStd::lock_guard<AZStd::mutex> lock(m_logMutex);

        LogEntry entry;
        if (window && window[0] != '\0')
        {
            entry.message = AZStd::string::format("[%s] %s", window, message);
        }
        else
        {
            entry.message = message;
        }
        entry.color = GetColorForLogLevel(level);

        m_logEntries.push_back(AZStd::move(entry));

        if (m_logEntries.size() > MaxLogEntries)
        {
            m_logEntries.pop_front();
        }

        m_logsDirty = true; // Mark for rebuild
    }

    AZ::Color TuRmlConsoleDocument::GetColorForLogLevel(AZ::LogLevel level) const
    {
        switch (level)
        {
        case AZ::LogLevel::Fatal:
        case AZ::LogLevel::Error:
            return AZ::Colors::Red;
        case AZ::LogLevel::Warn:
            return AZ::Colors::Yellow;
        case AZ::LogLevel::Debug:
        case AZ::LogLevel::Trace:
            return AZ::Colors::Gray;
        default:
            return AZ::Colors::White;
        }
    }

    void TuRmlConsoleDocument::UpdateLogDisplay()
    {
        if (!m_logsDirty)
        {
            return;
        }

        RebuildLogHTML();
        m_logsDirty = false;
    }

    void TuRmlConsoleDocument::RebuildLogHTML()
    {
        Rml::Element* logContent = m_doc->GetElementById("log_container");
        if (!logContent)
        {
            return;
        }

        //Clear it out
        logContent->SetInnerRML("");

        AZStd::string html;
        {
            AZStd::lock_guard<AZStd::mutex> lock(m_logMutex);

            for (const LogEntry& entry : m_logEntries)
            {
                const char* cssClass = "log_info";
                if (entry.color == AZ::Colors::Red)
                {
                    cssClass = "log_error";
                }
                else if (entry.color == AZ::Colors::Yellow)
                {
                    cssClass = "log_warning";
                }
                else if (entry.color == AZ::Colors::Gray)
                {
                    cssClass = "log_debug";
                }

                auto element = m_doc->CreateElement("div");
                if (!element)
                {
                    return;
                }
                element->SetClassNames("log_entry " + Rml::String(cssClass));
                element->SetInnerRML(entry.message.c_str());

                logContent->AppendChild(std::move(element));
            }
        }

        if (m_autoScroll)
        {
            m_doc->GetContext()->Update();
            ScrollToBottom();
        }
    }
} // namespace TuRml