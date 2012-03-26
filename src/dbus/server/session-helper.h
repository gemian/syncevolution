/*
 * Copyright (C) 2012 Intel Corporation
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

#ifndef SESSION_HELPER_H
#define SESSION_HELPER_H

#include "session-common.h"
#include "dbus-sync.h"

#include <gdbus-cxx-bridge.h>
#include <boost/function.hpp>
#include <glib.h>

#include <syncevo/declarations.h>
SE_BEGIN_CXX

/**
 * Waits for requests via the internal D-Bus connection in run(), sent
 * by the Session class in syncevo-dbus-server. Then for each request
 * it remembers what to do in m_operation and returns from the event
 * loop and executes the requested operation, pretty much like the
 * traditional syncevo-dbus-server did.
 */
class SessionHelper : public GDBusCXX::DBusObjectHelper,
    private boost::noncopyable
{
    GMainLoop *m_loop;
    GDBusCXX::DBusConnectionPtr m_conn;
    boost::function<bool ()> m_operation;

    /** valid during doSync() */
    boost::scoped_ptr<DBusSync> m_sync;

    /** called by main event loop: initiate a sync operation */
    void sync(const SessionCommon::SyncParams &params,
              const boost::shared_ptr<GDBusCXX::Result0> &result);

    /**
     * called by run(): do the sync operation
     * @return true if the helper is meant to terminate
     */
    bool doSync(const SessionCommon::SyncParams &params,
                const boost::shared_ptr<GDBusCXX::Result0> &result);

    void restore(const std::string &configName,
                 const string &dir, bool before, const std::vector<std::string> &sources,
                 const boost::shared_ptr<GDBusCXX::Result0> &result);
    bool doRestore(const std::string &configName,
                   const string &dir, bool before, const std::vector<std::string> &sources,
                   const boost::shared_ptr<GDBusCXX::Result0> &result);

    void execute(const vector<string> &args, const map<string, string> &vars,
                 const boost::shared_ptr< GDBusCXX::Result1<bool> > &result);
    bool doExecute(const vector<string> &args, const map<string, string> &vars,
                   const boost::shared_ptr< GDBusCXX::Result1<bool> > &result);

    /** SessionHelper.PasswordResponse */
    void passwordResponse(bool timedOut, bool aborted, const std::string &password);

 public:
    SessionHelper(GMainLoop *loop,
                  const GDBusCXX::DBusConnectionPtr &conn);
    void run();
    GMainLoop *getLoop() const { return m_loop; }

    /** SyncContext::displaySyncProgress */
    GDBusCXX::EmitSignal4<sysync::TProgressEventEnum,
        int32_t, int32_t, int32_t> emitSyncProgress;

    /** SyncContext::displaySourceProgress */
    GDBusCXX::EmitSignal6<sysync::TProgressEventEnum,
        std::string, SyncMode,
        int32_t, int32_t, int32_t> emitSourceProgress;

    /** SyncContext::reportStepCmd -> true/false for "waiting on IO" */
    GDBusCXX::EmitSignal1<bool> emitWaiting;

    /** SyncContext::syncSuccessStart */
    GDBusCXX::EmitSignal0 emitSyncSuccessStart;

    /** Cmdline::configWasModified() */
    GDBusCXX::EmitSignal0 emitConfigChanged;

    /** SyncContext::askPassword */
    GDBusCXX::EmitSignal2<std::string, ConfigPasswordKey> emitPasswordRequest;

    /** send message to parent's connection (buffer, type, url) */
    GDBusCXX::EmitSignal3<GDBusCXX::DBusArray<uint8_t>, std::string, std::string> emitMessage;

    /** tell parent's connection to shut down */
    GDBusCXX::EmitSignal0 emitShutdown;

    /** store the next message received by the session's connection */
    void storeMessage(const GDBusCXX::DBusArray<uint8_t> &message,
                      const std::string &type) {
        m_messageSignal(message, type);
    }
    typedef boost::signals2::signal<void (const GDBusCXX::DBusArray<uint8_t> &,
                                          const std::string &)> MessageSignal_t;
    MessageSignal_t m_messageSignal;

    /** store the latest connection state information */
    void connectionState(const std::string &error)
    {
        m_connectionStateSignal(error);
    }
    typedef boost::signals2::signal<void (const std::string &)> ConnectionStateSignal_t;
    ConnectionStateSignal_t m_connectionStateSignal;
};

SE_END_CXX

#endif // SESSION_HELPER_H
