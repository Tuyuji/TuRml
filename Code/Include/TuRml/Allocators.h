/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: Copyright (c) 2025 Reece Hagan
 *
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 */
#pragma once

#include <AzCore/Memory/SystemAllocator.h>
#include <AzCore/Memory/ChildAllocatorSchema.h>

namespace TuRml
{
    AZ_CHILD_ALLOCATOR_WITH_NAME(TuRmlRenderAllocator, "TuRmlRenderAllocator", "{273DC4B0-0869-45D7-9572-4B75E905BF7C}", AZ::SystemAllocator);
}