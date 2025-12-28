/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: Copyright (c) 2025 Reece Hagan
 *
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 */
#pragma once

#include <TuRml/TuRmlTypeIds.h>

#include <AzCore/EBus/EBus.h>
#include <AzCore/Interface/Interface.h>

namespace TuRml
{
    class TuRmlRenderInterface;

    class TuRmlRequests
    {
    public:
        AZ_RTTI(TuRmlRequests, TuRmlRequestsTypeId);
        virtual ~TuRmlRequests() = default;
        
        //! Get the global render interface instance
        virtual TuRmlRenderInterface* GetRenderInterface() = 0;
    };

    class TuRmlBusTraits
        : public AZ::EBusTraits
    {
    public:
        //////////////////////////////////////////////////////////////////////////
        // EBusTraits overrides
        static constexpr AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;
        static constexpr AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::Single;
        //////////////////////////////////////////////////////////////////////////
    };

    using TuRmlRequestBus = AZ::EBus<TuRmlRequests, TuRmlBusTraits>;
    using TuRmlInterface = AZ::Interface<TuRmlRequests>;

} // namespace TuRml
