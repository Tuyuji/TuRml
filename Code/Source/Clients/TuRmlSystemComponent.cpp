/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: Copyright (c) 2025 Reece Hagan
 *
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 */
#include "TuRmlSystemComponent.h"

#include <AzCore/Serialization/SerializeContext.h>
#include <AzFramework/Input/Devices/Mouse/InputDeviceMouse.h>
#include <AzFramework/Input/Devices/Keyboard/InputDeviceKeyboard.h>
#include <Atom/RPI.Public/FeatureProcessorFactory.h>
#include <Atom/RPI.Public/Scene.h>
#include <Atom/RPI.Public/Pass/PassSystemInterface.h>

#include <TuRml/TuRmlTypeIds.h>
#include <Console/TuRmlConsoleDocument.h>
#include <Render/TuRmlFeatureProcessor.h>
#include <Render/TuRmlParentPass.h>
#include <Render/TuRmlChildPass.h>
#include <Render/TuRmlRenderInterface.h>

#include <RmlUi/Core.h>
#include <RmlUi/Debugger/Debugger.h>

#include "../RmlBudget.h"
AZ_DEFINE_BUDGET(RmlBudget);

namespace TuRml
{
    AZ_COMPONENT_IMPL(TuRmlSystemComponent, "TuRmlSystemComponent",
        TuRmlSystemComponentTypeId);

    void TuRmlSystemComponent::Reflect(AZ::ReflectContext* context)
    {
        if (auto serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<TuRmlSystemComponent, AZ::Component>()
                ->Version(0)
                ;
        }

        TuRmlFeatureProcessor::Reflect(context);
    }

    void TuRmlSystemComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC_CE("TuRmlSystemService"));
    }

    void TuRmlSystemComponent::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        incompatible.push_back(AZ_CRC_CE("TuRmlSystemService"));
    }

    void TuRmlSystemComponent::GetRequiredServices([[maybe_unused]] AZ::ComponentDescriptor::DependencyArrayType& required)
    {
        required.push_back(AZ_CRC_CE("RPISystem"));
    }

    void TuRmlSystemComponent::GetDependentServices([[maybe_unused]] AZ::ComponentDescriptor::DependencyArrayType& dependent)
    {
    }

    TuRmlSystemComponent::TuRmlSystemComponent()
    {
        if (TuRmlInterface::Get() == nullptr)
        {
            TuRmlInterface::Register(this);
        }
    }

    TuRmlSystemComponent::~TuRmlSystemComponent()
    {
        if (TuRmlInterface::Get() == this)
        {
            TuRmlInterface::Unregister(this);
        }
    }

    void TuRmlSystemComponent::Init()
    {
    }

    void TuRmlSystemComponent::Activate()
    {
        AZ::SystemTickBus::Handler::BusConnect();
        AzFramework::InputChannelEventListener::Connect();
        TuRmlRequestBus::Handler::BusConnect();
        TuRmlInterface::Register(this);

        m_renderInterface = AZStd::make_unique<TuRmlRenderInterface>();
        Rml::SetRenderInterface(m_renderInterface.get());
        // Rml::SetSystemInterface(this);
        Rml::SetFileInterface(this);
        Rml::SetTextInputHandler(this);

        if (!Rml::Initialise())
        {
            AZ_Error("TuRml", false, "Failed to initialise RmlUi");
            return;
        }

        Rml::LoadFontFace("Fonts/Roboto-Regular.ttf");
        Rml::LoadFontFace("Fonts/Roboto-Bold.ttf");
        Rml::LoadFontFace("Fonts/Roboto-Italic.ttf");
        Rml::LoadFontFace("Fonts/LatoLatin-Regular.ttf");
        Rml::LoadFontFace("Fonts/LatoLatin-Italic.ttf");
        Rml::LoadFontFace("Console/JetBrainsMono-Regular.ttf");
        Rml::LoadFontFace("Fonts/NotoSansJP-VariableFont_wght.ttf", true);

        AZ::RPI::FeatureProcessorFactory::Get()->RegisterFeatureProcessor<TuRmlFeatureProcessor>();
        
        // Register pass classes
        auto* passSystem = AZ::RPI::PassSystemInterface::Get();
        passSystem->AddPassCreator(AZ::Name("TuRmlParentPass"), &TuRmlParentPass::Create);
        passSystem->AddPassCreator(AZ::Name("TuRmlChildPass"), &TuRmlChildPass::Create);
    }

    void TuRmlSystemComponent::Deactivate()
    {
        AZ::SystemTickBus::Handler::BusDisconnect();

        AZ::RPI::FeatureProcessorFactory::Get()->UnregisterFeatureProcessor<TuRmlFeatureProcessor>();

        AzFramework::InputChannelEventListener::Disconnect();
        Rml::Shutdown();

        if (m_renderInterface)
        {
            m_renderInterface.reset();
        }

        TuRmlInterface::Unregister(this);
        TuRmlRequestBus::Handler::BusDisconnect();
    }

    void TuRmlSystemComponent::OnSystemTick()
    {
        const auto numCtxs = Rml::GetNumContexts();
        for (auto i = 0; i < numCtxs; ++i)
        {
            auto ctx = Rml::GetContext(i);
            if (ctx == nullptr)
                continue;

            ctx->Update();
        }
    }

    TuRmlRenderInterface* TuRmlSystemComponent::GetRenderInterface()
    {
        return m_renderInterface.get();
    }

    void TuRmlSystemComponent::OnActivate(Rml::TextInputContext* ctx)
    {
        m_activeTxtContext = ctx;
        AzFramework::InputTextNotificationBus::Handler::BusConnect();
    }

    void TuRmlSystemComponent::OnDeactivate(Rml::TextInputContext* ctx)
    {
        m_activeTxtContext = nullptr;
        AzFramework::InputTextNotificationBus::Handler::BusDisconnect();
    }

    void TuRmlSystemComponent::OnDestroy(Rml::TextInputContext* ctx)
    {
        m_activeTxtContext = nullptr;
        AzFramework::InputTextNotificationBus::Handler::BusDisconnect();
    }

    static bool HandleMouseDevice(const AzFramework::InputChannel& inputChannel, Rml::Context* ctx)
    {
        const AzFramework::InputChannelId& channelId = inputChannel.GetInputChannelId();

        auto size = ctx->GetDimensions();
        if (channelId == AzFramework::InputDeviceMouse::Movement::X || channelId == AzFramework::InputDeviceMouse::Movement::Y)
        {
            if (inputChannel.GetValue() == 0.0f)
            {
                //Just allow it, lets things like camera movement not freak out too badly.
                return false;
            }
            return ctx->IsMouseInteracting();
        }
        if (channelId == AzFramework::InputDeviceMouse::SystemCursorPosition)
        {
            const auto* positionData = inputChannel.GetCustomData<AzFramework::InputChannel::PositionData2D>();
            if (positionData != nullptr)
            {
                int screenX = positionData->m_normalizedPosition.GetX() * size.x;
                int screenY = positionData->m_normalizedPosition.GetY() * size.y;

                return !ctx->ProcessMouseMove(screenX, screenY, 0);
            }
        }
        else if (channelId == AzFramework::InputDeviceMouse::Button::Left)
        {
            if (inputChannel.IsStateBegan())
            {
                return !ctx->ProcessMouseButtonDown(0, 0);
            }
            if (inputChannel.IsStateEnded())
            {
                return !ctx->ProcessMouseButtonUp(0, 0);
            }
        }
        else if (channelId == AzFramework::InputDeviceMouse::Button::Right)
        {
            if (inputChannel.IsStateBegan())
            {
                return !ctx->ProcessMouseButtonDown(1, 0);
            }
            if (inputChannel.IsStateEnded())
            {
                return !ctx->ProcessMouseButtonUp(1, 0);
            }
        }
        else if (channelId == AzFramework::InputDeviceMouse::Movement::Z)
        {
            if (inputChannel.IsStateBegan() || inputChannel.IsStateUpdated())
            {
                constexpr float MouseWheelDeltaScale = 1.0f / 120.0f; // based on WHEEL_DELTA in WinUser.h
                const auto delta = -(inputChannel.GetValue() * MouseWheelDeltaScale);
                return !ctx->ProcessMouseWheel(Rml::Vector2f(0, delta), 0);
            }
            if (inputChannel.IsStateEnded())
            {
                return !ctx->ProcessMouseWheel(Rml::Vector2f(), 0);
            }
        }

        return false;
    }

    static const AZStd::unordered_map<AzFramework::InputChannelId, Rml::Input::KeyIdentifier> s_KeyIdentifierMap = {
        // Alphanumeric Keys (0-9)
        {AzFramework::InputDeviceKeyboard::Key::Alphanumeric0, Rml::Input::KeyIdentifier::KI_0},
        {AzFramework::InputDeviceKeyboard::Key::Alphanumeric1, Rml::Input::KeyIdentifier::KI_1},
        {AzFramework::InputDeviceKeyboard::Key::Alphanumeric2, Rml::Input::KeyIdentifier::KI_2},
        {AzFramework::InputDeviceKeyboard::Key::Alphanumeric3, Rml::Input::KeyIdentifier::KI_3},
        {AzFramework::InputDeviceKeyboard::Key::Alphanumeric4, Rml::Input::KeyIdentifier::KI_4},
        {AzFramework::InputDeviceKeyboard::Key::Alphanumeric5, Rml::Input::KeyIdentifier::KI_5},
        {AzFramework::InputDeviceKeyboard::Key::Alphanumeric6, Rml::Input::KeyIdentifier::KI_6},
        {AzFramework::InputDeviceKeyboard::Key::Alphanumeric7, Rml::Input::KeyIdentifier::KI_7},
        {AzFramework::InputDeviceKeyboard::Key::Alphanumeric8, Rml::Input::KeyIdentifier::KI_8},
        {AzFramework::InputDeviceKeyboard::Key::Alphanumeric9, Rml::Input::KeyIdentifier::KI_9},

        // Alphanumeric Keys (A-Z)
        {AzFramework::InputDeviceKeyboard::Key::AlphanumericA, Rml::Input::KeyIdentifier::KI_A},
        {AzFramework::InputDeviceKeyboard::Key::AlphanumericB, Rml::Input::KeyIdentifier::KI_B},
        {AzFramework::InputDeviceKeyboard::Key::AlphanumericC, Rml::Input::KeyIdentifier::KI_C},
        {AzFramework::InputDeviceKeyboard::Key::AlphanumericD, Rml::Input::KeyIdentifier::KI_D},
        {AzFramework::InputDeviceKeyboard::Key::AlphanumericE, Rml::Input::KeyIdentifier::KI_E},
        {AzFramework::InputDeviceKeyboard::Key::AlphanumericF, Rml::Input::KeyIdentifier::KI_F},
        {AzFramework::InputDeviceKeyboard::Key::AlphanumericG, Rml::Input::KeyIdentifier::KI_G},
        {AzFramework::InputDeviceKeyboard::Key::AlphanumericH, Rml::Input::KeyIdentifier::KI_H},
        {AzFramework::InputDeviceKeyboard::Key::AlphanumericI, Rml::Input::KeyIdentifier::KI_I},
        {AzFramework::InputDeviceKeyboard::Key::AlphanumericJ, Rml::Input::KeyIdentifier::KI_J},
        {AzFramework::InputDeviceKeyboard::Key::AlphanumericK, Rml::Input::KeyIdentifier::KI_K},
        {AzFramework::InputDeviceKeyboard::Key::AlphanumericL, Rml::Input::KeyIdentifier::KI_L},
        {AzFramework::InputDeviceKeyboard::Key::AlphanumericM, Rml::Input::KeyIdentifier::KI_M},
        {AzFramework::InputDeviceKeyboard::Key::AlphanumericN, Rml::Input::KeyIdentifier::KI_N},
        {AzFramework::InputDeviceKeyboard::Key::AlphanumericO, Rml::Input::KeyIdentifier::KI_O},
        {AzFramework::InputDeviceKeyboard::Key::AlphanumericP, Rml::Input::KeyIdentifier::KI_P},
        {AzFramework::InputDeviceKeyboard::Key::AlphanumericQ, Rml::Input::KeyIdentifier::KI_Q},
        {AzFramework::InputDeviceKeyboard::Key::AlphanumericR, Rml::Input::KeyIdentifier::KI_R},
        {AzFramework::InputDeviceKeyboard::Key::AlphanumericS, Rml::Input::KeyIdentifier::KI_S},
        {AzFramework::InputDeviceKeyboard::Key::AlphanumericT, Rml::Input::KeyIdentifier::KI_T},
        {AzFramework::InputDeviceKeyboard::Key::AlphanumericU, Rml::Input::KeyIdentifier::KI_U},
        {AzFramework::InputDeviceKeyboard::Key::AlphanumericV, Rml::Input::KeyIdentifier::KI_V},
        {AzFramework::InputDeviceKeyboard::Key::AlphanumericW, Rml::Input::KeyIdentifier::KI_W},
        {AzFramework::InputDeviceKeyboard::Key::AlphanumericX, Rml::Input::KeyIdentifier::KI_X},
        {AzFramework::InputDeviceKeyboard::Key::AlphanumericY, Rml::Input::KeyIdentifier::KI_Y},
        {AzFramework::InputDeviceKeyboard::Key::AlphanumericZ, Rml::Input::KeyIdentifier::KI_Z},

        // Edit Keys
        {AzFramework::InputDeviceKeyboard::Key::EditBackspace, Rml::Input::KeyIdentifier::KI_BACK},
        {AzFramework::InputDeviceKeyboard::Key::EditCapsLock, Rml::Input::KeyIdentifier::KI_CAPITAL},
        {AzFramework::InputDeviceKeyboard::Key::EditEnter, Rml::Input::KeyIdentifier::KI_RETURN},
        {AzFramework::InputDeviceKeyboard::Key::EditSpace, Rml::Input::KeyIdentifier::KI_SPACE},
        {AzFramework::InputDeviceKeyboard::Key::EditTab, Rml::Input::KeyIdentifier::KI_TAB},
        {AzFramework::InputDeviceKeyboard::Key::Escape, Rml::Input::KeyIdentifier::KI_ESCAPE},

        // Function Keys
        {AzFramework::InputDeviceKeyboard::Key::Function01, Rml::Input::KeyIdentifier::KI_F1},
        {AzFramework::InputDeviceKeyboard::Key::Function02, Rml::Input::KeyIdentifier::KI_F2},
        {AzFramework::InputDeviceKeyboard::Key::Function03, Rml::Input::KeyIdentifier::KI_F3},
        {AzFramework::InputDeviceKeyboard::Key::Function04, Rml::Input::KeyIdentifier::KI_F4},
        {AzFramework::InputDeviceKeyboard::Key::Function05, Rml::Input::KeyIdentifier::KI_F5},
        {AzFramework::InputDeviceKeyboard::Key::Function06, Rml::Input::KeyIdentifier::KI_F6},
        {AzFramework::InputDeviceKeyboard::Key::Function07, Rml::Input::KeyIdentifier::KI_F7},
        {AzFramework::InputDeviceKeyboard::Key::Function08, Rml::Input::KeyIdentifier::KI_F8},
        {AzFramework::InputDeviceKeyboard::Key::Function09, Rml::Input::KeyIdentifier::KI_F9},
        {AzFramework::InputDeviceKeyboard::Key::Function10, Rml::Input::KeyIdentifier::KI_F10},
        {AzFramework::InputDeviceKeyboard::Key::Function11, Rml::Input::KeyIdentifier::KI_F11},
        {AzFramework::InputDeviceKeyboard::Key::Function12, Rml::Input::KeyIdentifier::KI_F12},
        {AzFramework::InputDeviceKeyboard::Key::Function13, Rml::Input::KeyIdentifier::KI_F13},
        {AzFramework::InputDeviceKeyboard::Key::Function14, Rml::Input::KeyIdentifier::KI_F14},
        {AzFramework::InputDeviceKeyboard::Key::Function15, Rml::Input::KeyIdentifier::KI_F15},
        {AzFramework::InputDeviceKeyboard::Key::Function16, Rml::Input::KeyIdentifier::KI_F16},
        {AzFramework::InputDeviceKeyboard::Key::Function17, Rml::Input::KeyIdentifier::KI_F17},
        {AzFramework::InputDeviceKeyboard::Key::Function18, Rml::Input::KeyIdentifier::KI_F18},
        {AzFramework::InputDeviceKeyboard::Key::Function19, Rml::Input::KeyIdentifier::KI_F19},
        {AzFramework::InputDeviceKeyboard::Key::Function20, Rml::Input::KeyIdentifier::KI_F20},

        // Navigation Keys
        {AzFramework::InputDeviceKeyboard::Key::NavigationArrowDown, Rml::Input::KeyIdentifier::KI_DOWN},
        {AzFramework::InputDeviceKeyboard::Key::NavigationArrowLeft, Rml::Input::KeyIdentifier::KI_LEFT},
        {AzFramework::InputDeviceKeyboard::Key::NavigationArrowRight, Rml::Input::KeyIdentifier::KI_RIGHT},
        {AzFramework::InputDeviceKeyboard::Key::NavigationArrowUp, Rml::Input::KeyIdentifier::KI_UP},
        {AzFramework::InputDeviceKeyboard::Key::NavigationDelete, Rml::Input::KeyIdentifier::KI_DELETE},
        {AzFramework::InputDeviceKeyboard::Key::NavigationEnd, Rml::Input::KeyIdentifier::KI_END},
        {AzFramework::InputDeviceKeyboard::Key::NavigationHome, Rml::Input::KeyIdentifier::KI_HOME},
        {AzFramework::InputDeviceKeyboard::Key::NavigationInsert, Rml::Input::KeyIdentifier::KI_INSERT},
        {AzFramework::InputDeviceKeyboard::Key::NavigationPageDown, Rml::Input::KeyIdentifier::KI_NEXT},
        {AzFramework::InputDeviceKeyboard::Key::NavigationPageUp, Rml::Input::KeyIdentifier::KI_PRIOR},

        // Numpad Keys
        {AzFramework::InputDeviceKeyboard::Key::NumLock, Rml::Input::KeyIdentifier::KI_NUMLOCK},
        {AzFramework::InputDeviceKeyboard::Key::NumPad0, Rml::Input::KeyIdentifier::KI_NUMPAD0},
        {AzFramework::InputDeviceKeyboard::Key::NumPad1, Rml::Input::KeyIdentifier::KI_NUMPAD1},
        {AzFramework::InputDeviceKeyboard::Key::NumPad2, Rml::Input::KeyIdentifier::KI_NUMPAD2},
        {AzFramework::InputDeviceKeyboard::Key::NumPad3, Rml::Input::KeyIdentifier::KI_NUMPAD3},
        {AzFramework::InputDeviceKeyboard::Key::NumPad4, Rml::Input::KeyIdentifier::KI_NUMPAD4},
        {AzFramework::InputDeviceKeyboard::Key::NumPad5, Rml::Input::KeyIdentifier::KI_NUMPAD5},
        {AzFramework::InputDeviceKeyboard::Key::NumPad6, Rml::Input::KeyIdentifier::KI_NUMPAD6},
        {AzFramework::InputDeviceKeyboard::Key::NumPad7, Rml::Input::KeyIdentifier::KI_NUMPAD7},
        {AzFramework::InputDeviceKeyboard::Key::NumPad8, Rml::Input::KeyIdentifier::KI_NUMPAD8},
        {AzFramework::InputDeviceKeyboard::Key::NumPad9, Rml::Input::KeyIdentifier::KI_NUMPAD9},
        {AzFramework::InputDeviceKeyboard::Key::NumPadAdd, Rml::Input::KeyIdentifier::KI_ADD},
        {AzFramework::InputDeviceKeyboard::Key::NumPadDecimal, Rml::Input::KeyIdentifier::KI_DECIMAL},
        {AzFramework::InputDeviceKeyboard::Key::NumPadDivide, Rml::Input::KeyIdentifier::KI_DIVIDE},
        {AzFramework::InputDeviceKeyboard::Key::NumPadEnter, Rml::Input::KeyIdentifier::KI_NUMPADENTER},
        {AzFramework::InputDeviceKeyboard::Key::NumPadMultiply, Rml::Input::KeyIdentifier::KI_MULTIPLY},
        {AzFramework::InputDeviceKeyboard::Key::NumPadSubtract, Rml::Input::KeyIdentifier::KI_SUBTRACT},

        // Punctuation Keys
        {AzFramework::InputDeviceKeyboard::Key::PunctuationApostrophe, Rml::Input::KeyIdentifier::KI_OEM_7},
        {AzFramework::InputDeviceKeyboard::Key::PunctuationBackslash, Rml::Input::KeyIdentifier::KI_OEM_5},
        {AzFramework::InputDeviceKeyboard::Key::PunctuationBracketL, Rml::Input::KeyIdentifier::KI_OEM_4},
        {AzFramework::InputDeviceKeyboard::Key::PunctuationBracketR, Rml::Input::KeyIdentifier::KI_OEM_6},
        {AzFramework::InputDeviceKeyboard::Key::PunctuationComma, Rml::Input::KeyIdentifier::KI_OEM_COMMA},
        {AzFramework::InputDeviceKeyboard::Key::PunctuationEquals, Rml::Input::KeyIdentifier::KI_OEM_PLUS},
        {AzFramework::InputDeviceKeyboard::Key::PunctuationHyphen, Rml::Input::KeyIdentifier::KI_OEM_MINUS},
        {AzFramework::InputDeviceKeyboard::Key::PunctuationPeriod, Rml::Input::KeyIdentifier::KI_OEM_PERIOD},
        {AzFramework::InputDeviceKeyboard::Key::PunctuationSemicolon, Rml::Input::KeyIdentifier::KI_OEM_1},
        {AzFramework::InputDeviceKeyboard::Key::PunctuationSlash, Rml::Input::KeyIdentifier::KI_OEM_2},
        {AzFramework::InputDeviceKeyboard::Key::PunctuationTilde, Rml::Input::KeyIdentifier::KI_OEM_3},

        // Supplementary ISO Key
        {AzFramework::InputDeviceKeyboard::Key::SupplementaryISO, Rml::Input::KeyIdentifier::KI_OEM_102},

        // Windows System Keys
        {AzFramework::InputDeviceKeyboard::Key::WindowsSystemPause, Rml::Input::KeyIdentifier::KI_PAUSE},
        {AzFramework::InputDeviceKeyboard::Key::WindowsSystemPrint, Rml::Input::KeyIdentifier::KI_SNAPSHOT},
        {AzFramework::InputDeviceKeyboard::Key::WindowsSystemScrollLock, Rml::Input::KeyIdentifier::KI_SCROLL},

        // Modifier Keys (for key identification, not modifier flags)
        {AzFramework::InputDeviceKeyboard::Key::ModifierAltL, Rml::Input::KeyIdentifier::KI_LMENU},
        {AzFramework::InputDeviceKeyboard::Key::ModifierAltR, Rml::Input::KeyIdentifier::KI_RMENU},
        {AzFramework::InputDeviceKeyboard::Key::ModifierCtrlL, Rml::Input::KeyIdentifier::KI_LCONTROL},
        {AzFramework::InputDeviceKeyboard::Key::ModifierCtrlR, Rml::Input::KeyIdentifier::KI_RCONTROL},
        {AzFramework::InputDeviceKeyboard::Key::ModifierShiftL, Rml::Input::KeyIdentifier::KI_LSHIFT},
        {AzFramework::InputDeviceKeyboard::Key::ModifierShiftR, Rml::Input::KeyIdentifier::KI_RSHIFT},
        {AzFramework::InputDeviceKeyboard::Key::ModifierSuperL, Rml::Input::KeyIdentifier::KI_LWIN},
        {AzFramework::InputDeviceKeyboard::Key::ModifierSuperR, Rml::Input::KeyIdentifier::KI_RWIN},
    };

    static const AZStd::unordered_map<AzFramework::InputChannelId, Rml::Input::KeyModifier> s_KeyModifierMap = {
        {AzFramework::InputDeviceKeyboard::Key::ModifierAltL, Rml::Input::KM_ALT},
        {AzFramework::InputDeviceKeyboard::Key::ModifierAltR, Rml::Input::KM_ALT},
        {AzFramework::InputDeviceKeyboard::Key::ModifierCtrlL, Rml::Input::KM_CTRL},
        {AzFramework::InputDeviceKeyboard::Key::ModifierCtrlR, Rml::Input::KM_CTRL},
        {AzFramework::InputDeviceKeyboard::Key::ModifierShiftL, Rml::Input::KM_SHIFT},
        {AzFramework::InputDeviceKeyboard::Key::ModifierShiftR, Rml::Input::KM_SHIFT},
        {AzFramework::InputDeviceKeyboard::Key::ModifierSuperL, Rml::Input::KM_META},
        {AzFramework::InputDeviceKeyboard::Key::ModifierSuperR, Rml::Input::KM_META},
        {AzFramework::InputDeviceKeyboard::Key::EditCapsLock, Rml::Input::KM_CAPSLOCK},
        {AzFramework::InputDeviceKeyboard::Key::NumLock, Rml::Input::KM_NUMLOCK},
        {AzFramework::InputDeviceKeyboard::Key::WindowsSystemScrollLock, Rml::Input::KM_SCROLLLOCK},
    };

    static bool HandleKeyboardDevice(const AzFramework::InputChannel& inputChannel, Rml::Context* ctx)
    {
        const AzFramework::InputChannelId& channelId = inputChannel.GetInputChannelId();

        const auto* device = azrtti_cast<const AzFramework::InputDeviceKeyboard*>(&inputChannel.GetInputDevice());
        if (device == nullptr)
            return false;

        auto keyIt = s_KeyIdentifierMap.find(channelId);
        if (keyIt == s_KeyIdentifierMap.end())
        {
            return false;
        }

        const Rml::Input::KeyIdentifier keyIdentifier = keyIt->second;

        static int modifiers = 0;
        auto keyModIt = s_KeyModifierMap.find(channelId);
        if (keyModIt != s_KeyModifierMap.end())
        {
            //set the needed flag depending on if its began or ended
            if (inputChannel.IsStateBegan())
            {
                modifiers |= keyModIt->second;
            }
            else if (inputChannel.IsStateEnded())
            {
                modifiers &= ~keyModIt->second;
            }

            //this was just a modifer press and we handled it
            return true;
        }

        if (inputChannel.IsStateBegan())
        {
            return !ctx->ProcessKeyDown(keyIdentifier, modifiers);
        }
        if (inputChannel.IsStateEnded())
        {
            return !ctx->ProcessKeyUp(keyIdentifier, modifiers);
        }

        return false;
    }

    bool TuRmlSystemComponent::OnInputChannelEventFiltered(const AzFramework::InputChannel& inputChannel)
    {
        const AzFramework::InputChannelId& channelId = inputChannel.GetInputChannelId();
        const auto& deviceId = inputChannel.GetInputDevice().GetInputDeviceId();

        if (inputChannel.IsStateBegan() &&
            channelId == AzFramework::InputDeviceKeyboard::Key::PunctuationTilde)
        {
            auto ctx = Rml::GetContext(0);
            if (ctx)
            {
                for (auto i = 0; i < ctx->GetNumDocuments(); ++i)
                {
                    auto doc = ctx->GetDocument(i);
                    if (!doc)
                    {
                        continue;
                    }

                    if (doc->GetId() == "console_overlay")
                    {
                        if (doc->IsVisible())
                        {
                            doc->Hide();
                        }else
                        {
                            doc->Show();
                        }
                    }
                }
            }
        }else if (inputChannel.IsStateBegan() && channelId == AzFramework::InputDeviceKeyboard::Key::Function09)
        {
            Rml::Debugger::SetVisible(!Rml::Debugger::IsVisible());
            return true;
        }

        const auto numCtxs = Rml::GetNumContexts();
        for (auto i = 0; i < numCtxs; ++i)
        {
            auto ctx = Rml::GetContext(i);
            if (ctx == nullptr)
                continue;

            if (AzFramework::InputDeviceMouse::IsMouseDevice(deviceId))
            {
                return HandleMouseDevice(inputChannel, ctx);
            }
            if (AzFramework::InputDeviceKeyboard::IsKeyboardDevice(deviceId))
            {
                if (!HandleKeyboardDevice(inputChannel, ctx))
                {
                    //Not handled
                    //If we're currently inputting text then assume we handled this keyboard event
                    return m_activeTxtContext != nullptr;
                }
                return true;
            }

        }
        return false;
    }

    void TuRmlSystemComponent::OnInputTextEvent(const AZStd::string& text, bool& consumed)
    {
        if (text.empty())
        {
            return;
        }

        // Skip control characters (ASCII < 32) except tab (9)
        // Common control chars: backspace (8), delete (127), escape (27), etc.
        for (char c : text)
        {
            if ((c < 32 && c != 9) || c == 127 || c == 96)
            {
                consumed = false;
                return;
            }
        }

        const auto numCtxs = Rml::GetNumContexts();
        for (auto i = 0; i < numCtxs; ++i)
        {
            auto ctx = Rml::GetContext(i);
            if (ctx == nullptr)
                continue;

            consumed = !ctx->ProcessTextInput(text.c_str());
            if (consumed)
            {
                return;
            }
        }
    }

    void TuRmlSystemComponent::JoinPath(Rml::String& translated_path, const Rml::String& document_path, const Rml::String& path)
    {
        if (path.empty())
        {
            translated_path = document_path;
            return;
        }

        if (path[0] == '/' || path.find(':') != Rml::String::npos)
        {
            // Absolute path
            translated_path = path;
        }
        else
        {
            // Relative path, join with document path
            AZ::IO::Path docPath(document_path.c_str());
            AZ::IO::Path relativePath(path.c_str());
            AZ::IO::Path parentPath(docPath.ParentPath());
            AZ::IO::Path result = parentPath / relativePath;
            translated_path = result.c_str();
        }
    }

    Rml::FileHandle TuRmlSystemComponent::Open(const Rml::String& path)
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

        AZ::IO::FileIOStream* f = aznew AZ::IO::FileIOStream(info.m_relativePath.c_str(), AZ::IO::OpenMode::ModeRead | AZ::IO::OpenMode::ModeBinary);
        if (!f->IsOpen())
        {
            return 0;
        }

        return reinterpret_cast<Rml::FileHandle>(f);
    }

    void TuRmlSystemComponent::Close(Rml::FileHandle file)
    {
        auto* storedFile = reinterpret_cast<AZ::IO::FileIOStream*>(file);
        if (!storedFile)
        {
            return;
        }
        
        storedFile->Close();

        azfree (storedFile);
    }

    size_t TuRmlSystemComponent::Read(void* buffer, size_t size, Rml::FileHandle file)
    {
        if (!file || !buffer || size == 0)
        {
            return 0;
        }

        auto* storedFile = reinterpret_cast<AZ::IO::FileIOStream*>(file);

        return storedFile->Read(size, buffer);
    }

    bool TuRmlSystemComponent::Seek(Rml::FileHandle file, long offset, int origin)
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

    size_t TuRmlSystemComponent::Tell(Rml::FileHandle file)
    {
        if (!file)
        {
            return 0;
        }

        auto* storedFile = reinterpret_cast<AZ::IO::FileIOStream*>(file);

        return storedFile->GetCurPos();
    }

    size_t TuRmlSystemComponent::Length(Rml::FileHandle file)
    {
        if (!file)
        {
            return 0;
        }

        auto* storedFile = reinterpret_cast<AZ::IO::FileIOStream*>(file);
        return storedFile->GetLength();
    }

    bool TuRmlSystemComponent::LoadFile(const Rml::String& path, Rml::String& out_data)
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
} // namespace TuRml
