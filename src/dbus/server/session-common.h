/*
 * Copyright (C) 2011 Intel Corporation
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) version 3.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

#ifndef SESSION_COMMON_H
#define SESSION_COMMON_H

#include "source-status.h"
#include "source-progress.h"
#include <syncevo/util.h>
#include <syncevo/DBusTraits.h>
#include <syncevo/FilterConfigNode.h>

SE_BEGIN_CXX

/**
 * This namespace holds constants and defines for Sessions and its
 * consumers.
 */
namespace SessionCommon
{
    const char * const SERVICE_NAME = "org.syncevolution";
    const char * const CONNECTION_PATH = "/org/syncevolution/Connection";
    const char * const CONNECTION_IFACE = "org.syncevolution.Connection";
    const char * const SESSION_PATH = "/org/syncevolution/Session";
    const char * const SESSION_IFACE = "org.syncevolution.Session";
    const char * const SERVER_PATH = "/org/syncevolution/Server";
    const char * const SERVER_IFACE = "org.syncevolution.Server";

    const char * const HELPER_PATH = "/dbushelper";
    const char * const HELPER_IFACE = "org.syncevolution.Helper";
    const char * const HELPER_DESTINATION = "direct.peer"; // doesn't matter, routing is off

    /**
     * Used by both Connection class (inside server) and
     * DBusTransportAgent (inside helper).
     */
    enum ConnectionState {
        SETUP,          /**< ready for first message */
        PROCESSING,     /**< received message, waiting for engine's reply */
        WAITING,        /**< waiting for next follow-up message */
        FINAL,          /**< engine has sent final reply, wait for ACK by peer */
        DONE,           /**< peer has closed normally after the final reply */
        FAILED          /**< in a failed state, no further operation possible */
    };

    /** maps to names for debugging */
    inline std::string ConnectionStateToString(ConnectionState state)
    {
        static const char * const strings[] = {
            "SETUP",
            "PROCESSING",
            "WAITING",
            "FINAL",
            "DONE",
            "FAILED"
        };
        return state >= SETUP && state <= FAILED ?
            strings[state] :
            "???";
    }

    typedef StringMap SourceModes_t;
    typedef std::map<std::string, SyncEvo::FilterConfigNode::ConfigFilter> SourceFilters_t;

    /**
     * all the information that syncevo-dbus-server needs to
     * send to syncevo-dbus-helper before the latter can
     * run a sync
     */
    struct SyncParams
    {
        SyncParams() :
           m_serverMode(false),
           m_serverAlerted(false),
           m_remoteInitiated(false)
        {}

        std::string m_config;
        std::string m_mode;
        SourceModes_t m_sourceModes;
        bool m_serverMode;
        bool m_serverAlerted;
        bool m_remoteInitiated;
        std::string m_sessionID;
        std::string m_initialMessage;
        std::string m_initialMessageType;
        SyncEvo::FilterConfigNode::ConfigFilter m_syncFilter;
        SyncEvo::FilterConfigNode::ConfigFilter m_sourceFilter;
        SourceFilters_t m_sourceFilters;
    };
}

SE_END_CXX

namespace GDBusCXX {
    using namespace SyncEvo::SessionCommon;
    using namespace SyncEvo;
    template<> struct dbus_traits<SyncParams> :
        public dbus_struct_traits<SyncParams,
        dbus_member<SyncParams, std::string, &SyncParams::m_config,
        dbus_member<SyncParams, std::string, &SyncParams::m_mode,
        dbus_member<SyncParams, SourceModes_t, &SyncParams::m_sourceModes,
        dbus_member<SyncParams, bool, &SyncParams::m_serverMode,
        dbus_member<SyncParams, bool, &SyncParams::m_serverAlerted,
        dbus_member<SyncParams, bool, &SyncParams::m_remoteInitiated,
        dbus_member<SyncParams, std::string, &SyncParams::m_sessionID,
        dbus_member<SyncParams, std::string, &SyncParams::m_initialMessage,
        dbus_member<SyncParams, std::string, &SyncParams::m_initialMessageType,
        dbus_member<SyncParams, FilterConfigNode::ConfigFilter, &SyncParams::m_syncFilter,
        dbus_member<SyncParams, FilterConfigNode::ConfigFilter, &SyncParams::m_sourceFilter,
        dbus_member_single<SyncParams, SourceFilters_t, &SyncParams::m_sourceFilters
        > > > > > > > > > > > > >
        {};
}

#endif // SESSION_COMMON_H
