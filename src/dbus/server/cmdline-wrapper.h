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

#ifndef CMD_LINE_WRAPPER_H
#define CMD_LINE_WRAPPER_H

#include <syncevo/Cmdline.h>

#include "dbus-sync.h"
#include "exceptions.h"
#include "session-helper.h"

SE_BEGIN_CXX

/**
 * A wrapper to maintain the execution of command line arguments from
 * dbus clients. It creates the DBusSync instance when required and
 * sets up the same environment as in the D-Bus client.
 */
class CmdlineWrapper
{
    class DBusCmdline : public Cmdline {
        SessionHelper &m_helper;
    public:
        DBusCmdline(SessionHelper &helper,
                    const vector<string> &args) :
            Cmdline(args),
            m_helper(helper)
        {}

        SyncContext* createSyncClient() {
            return new DBusSync(SessionCommon::SyncParams(), m_helper);
        }
    };

    /** instance to run command line arguments */
    DBusCmdline m_cmdline;

    /** environment variables passed from client */
    map<string, string> m_envVars;

public:
    CmdlineWrapper(SessionHelper &helper,
                   const vector<string> &args,
                   const map<string, string> &vars) :
        m_cmdline(helper, args),
        m_envVars(vars)
    {}

    bool parse() { return m_cmdline.parse(); }
    bool run()
    {
        //temporarily set environment variables and restore them after running
        list<boost::shared_ptr<ScopedEnvChange> > changes;
        BOOST_FOREACH(const StringPair &var, m_envVars) {
            changes.push_back(boost::shared_ptr<ScopedEnvChange>(new ScopedEnvChange(var.first, var.second)));
        }
        bool success = m_cmdline.run();
        return success;
    }

    bool configWasModified() { return m_cmdline.configWasModified(); }
};

SE_END_CXX

#endif // CMD_LINE_WRAPPER_H
