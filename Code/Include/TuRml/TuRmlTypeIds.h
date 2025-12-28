/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: Copyright (c) 2025 Reece Hagan
 *
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 */
#pragma once

namespace TuRml
{
    // System Component TypeIds
    inline constexpr const char* TuRmlSystemComponentTypeId = "{A6B3FAB2-8001-4BB4-9415-851B7AB4A7EF}";
    inline constexpr const char* TuRmlEditorSystemComponentTypeId = "{F0B440CA-45F5-4C41-A68D-6AE8FE290849}";

    // Module derived classes TypeIds
    inline constexpr const char* TuRmlModuleInterfaceTypeId = "{2E6F1C23-B923-4CD9-99E6-F0D35765C6B7}";
    inline constexpr const char* TuRmlModuleTypeId = "{20E3E049-9D01-46C1-9CC3-093AFB77FF7A}";
    // The Editor Module by default is mutually exclusive with the Client Module
    // so they use the Same TypeId
    inline constexpr const char* TuRmlEditorModuleTypeId = TuRmlModuleTypeId;

    // Interface TypeIds
    inline constexpr const char* TuRmlRequestsTypeId = "{871006E3-8C28-4444-991B-E8B30FC113E6}";
} // namespace TuRml
