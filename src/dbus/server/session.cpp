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

#include <syncevo/LogRedirect.h>

#include "session.h"
#include "session-listener.h"
#include "restart.h"
#include "info-req.h"
#include "cmdline-wrapper.h"
#include "connection.h"
#include "client.h"
#include "dbus-sync.h"

#include <boost/foreach.hpp>

using namespace GDBusCXX;

SE_BEGIN_CXX

/**
 * validate key/value property and copy it to the filter
 * if okay
 */
static void copyProperty(const StringPair &keyvalue,
                         ConfigPropertyRegistry &registry,
                         FilterConfigNode::ConfigFilter &filter)
{
    const std::string &name = keyvalue.first;
    const std::string &value = keyvalue.second;
    const ConfigProperty *prop = registry.find(name);
    if (!prop) {
        SE_THROW_EXCEPTION(InvalidCall, StringPrintf("unknown property '%s'", name.c_str()));
    }
    std::string error;
    if (!prop->checkValue(value, error)) {
        SE_THROW_EXCEPTION(InvalidCall, StringPrintf("invalid value '%s' for property '%s': '%s'",
                                                     value.c_str(), name.c_str(), error.c_str()));
    }
    filter.insert(keyvalue);
}

static void setSyncFilters(const ReadOperations::Config_t &config,FilterConfigNode::ConfigFilter &syncFilter,
                           std::map<std::string, FilterConfigNode::ConfigFilter> &sourceFilters)
{
    ReadOperations::Config_t::const_iterator it;
    for (it = config.begin(); it != config.end(); it++) {
        map<string, string>::const_iterator sit;
        string name = it->first;
        if (name.empty()) {
            ConfigPropertyRegistry &registry = SyncConfig::getRegistry();
            for (sit = it->second.begin(); sit != it->second.end(); sit++) {
                // read-only properties can (and have to be) ignored
                static const char *init[] = {
                    "configName",
                    "description",
                    "score",
                    "deviceName",
                    "hardwareName",
                    "templateName",
                    "fingerprint"
                };
                static const set< std::string, Nocase<std::string> >
                    special(init,
                            init + (sizeof(init) / sizeof(*init)));
                if (special.find(sit->first) == special.end()) {
                    copyProperty(*sit, registry, syncFilter);
                }
            }
        } else if (boost::starts_with(name, "source/")) {
            name = name.substr(strlen("source/"));
            FilterConfigNode::ConfigFilter &sourceFilter = sourceFilters[name];
            ConfigPropertyRegistry &registry = SyncSourceConfig::getRegistry();
            for (sit = it->second.begin(); sit != it->second.end(); sit++) {
                copyProperty(*sit, registry, sourceFilter);
            }
        } else {
            SE_THROW_EXCEPTION(InvalidCall, StringPrintf("invalid config entry '%s'", name.c_str()));
        }
    }
}

void Session::setConfig(bool update, bool temporary,
                        const ReadOperations::Config_t &config)
{
    bool setConfig = m_setConfig;
    setNamedConfig(setConfig, m_configName, update, temporary, config);
}

void Session::setNamedConfig(bool &setConfig, const std::string &configName,
                             bool update, bool temporary,
                             const ReadOperations::Config_t &config)
{
    if (!m_active) {
        SE_THROW_EXCEPTION(InvalidCall, "session is not active, call not allowed at this time");
    }
    if (m_runOperation != OP_NULL) {
        string msg = StringPrintf("%s started, cannot change configuration at this time",
                                  runOpToString(m_runOperation).c_str());
        SE_THROW_EXCEPTION(InvalidCall, msg);
    }

    /** check whether we need remove the entire configuration */
    if(!update && !temporary && config.empty()) {
        boost::shared_ptr<SyncConfig> syncConfig(new SyncConfig(configName));
        if(syncConfig.get()) {
            syncConfig->remove();
            m_setConfig = true;
        }
        setConfig = m_setConfig;
        return;
    }

    /*
     * validate input config and convert to filters;
     * if validation fails, no harm was done at this point yet
     */
    FilterConfigNode::ConfigFilter syncFilter;
    SourceFilters_t sourceFilters;
    setSyncFilters(config, syncFilter, sourceFilters);

    if (temporary) {
        /* save temporary configs in session filters, either erasing old
           temporary settings or adding to them */
        if (update) {
            m_syncFilter.insert(syncFilter.begin(), syncFilter.end());
            BOOST_FOREACH(SourceFilters_t::value_type &source, sourceFilters) {
                SourceFilters_t::iterator it = m_sourceFilters.find(source.first);
                if (it != m_sourceFilters.end()) {
                    // add to existing source filter
                    it->second.insert(source.second.begin(), source.second.end());
                } else {
                    // add source filter
                    m_sourceFilters.insert(source);
                }
            }
        } else {
            m_syncFilter = syncFilter;
            m_sourceFilters = sourceFilters;
        }
        m_tempConfig = true;
    } else {
        /* need to save configurations */
        boost::shared_ptr<SyncConfig> from(new SyncConfig(configName));
        /* if it is not clear mode and config does not exist, an error throws */
        if(update && !from->exists()) {
            SE_THROW_EXCEPTION(NoSuchConfig, "The configuration '" + configName + "' doesn't exist" );
        }
        if(!update) {
            list<string> sources = from->getSyncSources();
            list<string>::iterator it;
            for(it = sources.begin(); it != sources.end(); ++it) {
                string source = "source/";
                source += *it;
                ReadOperations::Config_t::const_iterator configIt = config.find(source);
                if(configIt == config.end()) {
                    /** if no config for this source, we remove it */
                    from->removeSyncSource(*it);
                } else {
                    /** just clear visiable properties, remove them and their values */
                    from->clearSyncSourceProperties(*it);
                }
            }
            from->clearSyncProperties();
        }
        /** generate new sources in the config map */
        for (ReadOperations::Config_t::const_iterator it = config.begin(); it != config.end(); ++it) {
            string sourceName = it->first;
            if(sourceName.find("source/") == 0) {
                sourceName = sourceName.substr(7); ///> 7 is the length of "source/"
                from->getSyncSourceNodes(sourceName);
            }
        }
        /* apply user settings */
        from->setConfigFilter(true, "", syncFilter);
        map<string, FilterConfigNode::ConfigFilter>::iterator it;
        for ( it = sourceFilters.begin(); it != sourceFilters.end(); it++ ) {
            from->setConfigFilter(false, it->first, it->second);
        }
        boost::shared_ptr<DBusSync> syncConfig(new DBusSync(configName, *this));
        syncConfig->prepareConfigForWrite();
        syncConfig->copy(*from, NULL);

        syncConfig->preFlush(*syncConfig);
        syncConfig->flush();
        m_setConfig = true;
    }

    setConfig = m_setConfig;
}

void Session::initServer(SharedBuffer data, const std::string &messageType)
{
    m_serverMode = true;
    m_initialMessage = data;
    m_initialMessageType = messageType;
}

void Session::sync(const std::string &mode, const SessionCommon::SourceModes_t &source_modes)
{
    if (!m_active) {
        SE_THROW_EXCEPTION(InvalidCall, "session is not active, call not allowed at this time");
    }
    if (m_runOperation == OP_SYNC) {
        string msg = StringPrintf("%s started, cannot start again", runOpToString(m_runOperation).c_str());
        SE_THROW_EXCEPTION(InvalidCall, msg);
    } else if (m_runOperation != OP_NULL) {
        string msg = StringPrintf("%s started, cannot start sync", runOpToString(m_runOperation).c_str());
        SE_THROW_EXCEPTION(InvalidCall, msg);
    }

    m_sync.reset(new DBusSync(getConfigName(), *this));
    m_sync->setServerAlerted(m_serverAlerted);
    if (m_serverMode) {
        m_sync->initServer(m_sessionID,
                           m_initialMessage,
                           m_initialMessageType);
        boost::shared_ptr<Connection> c = m_connection.lock();
        if (c && !c->mustAuthenticate()) {
            // unsetting username/password disables checking them
            m_syncFilter["password"] = "";
            m_syncFilter["username"] = "";
        }
    }

    if (m_remoteInitiated) {
        m_sync->setRemoteInitiated (true);
    }

    // Apply temporary config filters. The parameters of this function
    // override the source filters, if set.
    m_sync->setConfigFilter(true, "", m_syncFilter);
    FilterConfigNode::ConfigFilter filter;
    filter = m_sourceFilter;
    if (!mode.empty()) {
        filter["sync"] = mode;
    }
    m_sync->setConfigFilter(false, "", filter);
    BOOST_FOREACH(const std::string &source,
                  m_sync->getSyncSources()) {
        filter = m_sourceFilters[source];
        SessionCommon::SourceModes_t::const_iterator it = source_modes.find(source);
        if (it != source_modes.end()) {
            filter["sync"] = it->second;
        }
        m_sync->setConfigFilter(false, source, filter);
    }

    // Update status and progress. From now on, all configured sources
    // have their default entry (referencing them by name creates the
    // entry).
    BOOST_FOREACH(const std::string source,
                  m_sync->getSyncSources()) {
        m_sourceStatus[source];
        m_sourceProgress[source];
    }
    fireProgress(true);
    fireStatus(true);
    m_runOperation = OP_SYNC;
}

void Session::abort()
{
    if (m_runOperation != OP_SYNC && m_runOperation != OP_CMDLINE) {
        SE_THROW_EXCEPTION(InvalidCall, "sync not started, cannot abort at this time");
    }
    m_syncStatus = SessionCommon::SYNC_ABORT;
    fireStatus(true);

    // state change, return to caller so that it can react
    g_main_loop_quit(m_loop);
}

void Session::suspend()
{
    if (m_runOperation != OP_SYNC && m_runOperation != OP_CMDLINE) {
        SE_THROW_EXCEPTION(InvalidCall, "sync not started, cannot suspend at this time");
    }
    m_syncStatus = SessionCommon::SYNC_SUSPEND;
    fireStatus(true);
}

void Session::getStatus(std::string &status,
                        uint32_t &error,
                        SessionCommon::SourceStatuses_t &sources)
{
    status = syncStatusToString(m_syncStatus);
    if (m_stepIsWaiting) {
        status += ";waiting";
    }

    error = m_error;
    sources = m_sourceStatus;
}

void Session::getProgress(int32_t &progress,
                          SessionCommon::SourceProgresses_t &sources)
{
    progress = m_progress;
    sources = m_sourceProgress;
}

void Session::fireStatus(bool flush)
{
    std::string status;
    uint32_t error;
    SessionCommon::SourceStatuses_t sources;

    /** not force flushing and not timeout, return */
    if(!flush && !m_statusTimer.timeout()) {
        return;
    }
    m_statusTimer.reset();

    getStatus(status, error, sources);
    emitStatus(status, error, sources);
}

void Session::fireProgress(bool flush)
{
    int32_t progress;
    SessionCommon::SourceProgresses_t sources;

    /** not force flushing and not timeout, return */
    if(!flush && !m_progressTimer.timeout()) {
        return;
    }
    m_progressTimer.reset();

    getProgress(progress, sources);
    emitProgress(progress, sources);
}
string Session::syncStatusToString(SessionCommon::SyncStatus state)
{
    switch(state) {
    case SessionCommon::SYNC_QUEUEING:
        return "queueing";
    case SessionCommon::SYNC_IDLE:
        return "idle";
    case SessionCommon::SYNC_RUNNING:
        return "running";
    case SessionCommon::SYNC_ABORT:
        return "aborting";
    case SessionCommon::SYNC_SUSPEND:
        return "suspending";
    case SessionCommon::SYNC_DONE:
        return "done";
    default:
        return "";
    };
}

boost::shared_ptr<Session> Session::createSession(GMainLoop *loop,
                                                  const GDBusCXX::DBusConnectionPtr &conn,
                                                  const std::string &config_name,
                                                  const std::string &session,
                                                  const std::vector<std::string> &flags)
{
    boost::shared_ptr<Session> me(new Session(loop, conn, config_name, session, flags));
    return me;
}

Session::Session(GMainLoop *loop,
                 const GDBusCXX::DBusConnectionPtr &conn,
                 const std::string &config_name,
                 const std::string &session,
                 const std::vector<std::string> &flags) :
    DBusObjectHelper(conn,
                     std::string("/dbushelper"),
                     std::string("dbushelper.Session") + session,
                     DBusObjectHelper::Callback_t(),
                     true),
    ReadOperations(config_name),
    m_flags(flags),
    m_sessionID(session),
    m_serverMode(false),
    m_loop(loop),
    m_useConnection(false),
    m_tempConfig(false),
    m_setConfig(false),
    m_active(true),
    m_remoteInitiated(false),
    m_syncStatus(SessionCommon::SYNC_QUEUEING),
    m_stepIsWaiting(false),
    m_progress(0),
    m_progData(m_progress),
    m_error(0),
    m_statusTimer(100),
    m_progressTimer(50),
    m_restoreBefore(true),
    m_restoreSrcTotal(0),
    m_restoreSrcEnd(0),
    m_runOperation(OP_NULL),
    m_listener(NULL),
    m_pwResponseStatus(SessionCommon::PW_RES_IDLE),
    emitStatus(*this, "StatusChanged"),
    emitProgress(*this, "ProgressChanged"),
    emitDone(*this, "Done"),
    emitPasswordRequest(*this, "PasswordRequest")
{
    add(static_cast<ReadOperations *>(this), &ReadOperations::getNamedConfig, "GetNamedConfig");
    add(this, &Session::setNamedConfig, "SetNamedConfig");
    add(static_cast<ReadOperations *>(this), &ReadOperations::getReports, "GetReports");
    add(static_cast<ReadOperations *>(this), &ReadOperations::checkSource, "CheckSource");
    add(static_cast<ReadOperations *>(this), &ReadOperations::getDatabases, "GetDatabases");
    add(this, &Session::sync, "Sync");
    add(this, &Session::abort, "Abort");
    add(this, &Session::suspend, "Suspend");
    add(this, &Session::getStatus, "GetStatus");
    add(this, &Session::getProgress, "GetProgress");
    add(this, &Session::restore, "Restore");
    add(this, &Session::execute, "Execute");
    add(this, &Session::passwordResponse, "PasswordResponse");
    add(this, &Session::serverShutdown, "ServerShutdown");
    add(this, &Session::setActive, "SetActive");
    add(emitStatus);
    add(emitProgress);
    add(emitPasswordRequest);
    add(emitDone);

    SE_LOG_DEBUG(NULL, NULL, "session %s created", getPath());
}

Session::~Session()
{
    SE_LOG_DEBUG(NULL, NULL, "session %s deconstructing", getPath());
    done();
}

void Session::serverShutdown()
{
    // TODO: Server process is gone so we need to avoid emitting dbus signals, etc.
}

void Session::setActive(bool active)
{
    if (active) {
        if (m_syncStatus == SessionCommon::SYNC_QUEUEING) {
            m_syncStatus = SessionCommon::SYNC_IDLE;
            fireStatus(true);
        }

        boost::shared_ptr<Connection> c = m_connection.lock();
        if (c) {
            c->ready();
        }
    }
}

void Session::syncProgress(sysync::TProgressEventEnum type,
                           int32_t extra1, int32_t extra2, int32_t extra3)
{
    switch(type) {
    case sysync::PEV_SESSIONSTART:
        m_progData.setStep(ProgressData::PRO_SYNC_INIT);
        fireProgress(true);
        break;
    case sysync::PEV_SESSIONEND:
        if((uint32_t)extra1 != m_error) {
            m_error = extra1;
            fireStatus(true);
        }
        m_progData.setStep(ProgressData::PRO_SYNC_INVALID);
        fireProgress(true);
        break;
    case sysync::PEV_SENDSTART:
        m_progData.sendStart();
        break;
    case sysync::PEV_SENDEND:
    case sysync::PEV_RECVSTART:
    case sysync::PEV_RECVEND:
        m_progData.receiveEnd();
        fireProgress();
        break;
    case sysync::PEV_DISPLAY100:
    case sysync::PEV_SUSPENDCHECK:
    case sysync::PEV_DELETING:
        break;
    case sysync::PEV_SUSPENDING:
        m_syncStatus = SessionCommon::SYNC_SUSPEND;
        fireStatus(true);
        break;
    default:
        ;
    }
}

void Session::sourceProgress(sysync::TProgressEventEnum type,
                             SyncSource &source,
                             int32_t extra1, int32_t extra2, int32_t extra3)
{
    switch(m_runOperation) {
    case OP_SYNC: {
        SourceProgress &progress = m_sourceProgress[source.getName()];
        SourceStatus &status = m_sourceStatus[source.getName()];
        switch(type) {
        case sysync::PEV_SYNCSTART:
            if(source.getFinalSyncMode() != SYNC_NONE) {
                m_progData.setStep(ProgressData::PRO_SYNC_UNINIT);
                fireProgress();
            }
            break;
        case sysync::PEV_SYNCEND:
            if(source.getFinalSyncMode() != SYNC_NONE) {
                status.set(PrettyPrintSyncMode(source.getFinalSyncMode()), "done", extra1);
                fireStatus(true);
            }
            break;
        case sysync::PEV_PREPARING:
            if(source.getFinalSyncMode() != SYNC_NONE) {
                progress.m_phase        = "preparing";
                progress.m_prepareCount = extra1;
                progress.m_prepareTotal = extra2;
                m_progData.itemPrepare();
                fireProgress(true);
            }
            break;
        case sysync::PEV_ITEMSENT:
            if(source.getFinalSyncMode() != SYNC_NONE) {
                progress.m_phase     = "sending";
                progress.m_sendCount = extra1;
                progress.m_sendTotal = extra2;
                fireProgress(true);
            }
            break;
        case sysync::PEV_ITEMRECEIVED:
            if(source.getFinalSyncMode() != SYNC_NONE) {
                progress.m_phase        = "receiving";
                progress.m_receiveCount = extra1;
                progress.m_receiveTotal = extra2;
                m_progData.itemReceive(source.getName(), extra1, extra2);
                fireProgress(true);
            }
            break;
        case sysync::PEV_ALERTED:
            if(source.getFinalSyncMode() != SYNC_NONE) {
                status.set(PrettyPrintSyncMode(source.getFinalSyncMode()), "running", 0);
                fireStatus(true);
                m_progData.setStep(ProgressData::PRO_SYNC_DATA);
                m_progData.addSyncMode(source.getFinalSyncMode());
                fireProgress();
            }
            break;
        default:
            ;
        }
        break;
    }
    case OP_RESTORE: {
        switch(type) {
        case sysync::PEV_ALERTED:
            // count the total number of sources to be restored
            m_restoreSrcTotal++;
            break;
        case sysync::PEV_SYNCSTART: {
            if (source.getFinalSyncMode() != SYNC_NONE) {
                SourceStatus &status = m_sourceStatus[source.getName()];
                // set statuses as 'restore-from-backup'
                status.set(PrettyPrintSyncMode(source.getFinalSyncMode()), "running", 0);
                fireStatus(true);
            }
            break;
        }
        case sysync::PEV_SYNCEND: {
            if (source.getFinalSyncMode() != SYNC_NONE) {
                m_restoreSrcEnd++;
                SourceStatus &status = m_sourceStatus[source.getName()];
                status.set(PrettyPrintSyncMode(source.getFinalSyncMode()), "done", 0);
                m_progress = 100 * m_restoreSrcEnd / m_restoreSrcTotal;
                fireStatus(true);
                fireProgress(true);
            }
            break;
        }
        default:
            break;
        }
        break;
    }
    default:
        break;
    }
}

void Session::run(LogRedirect &redirect)
{
    if (m_runOperation != OP_NULL) {
        try {
            m_syncStatus = SessionCommon::SYNC_RUNNING;
            fireStatus(true);
            switch(m_runOperation) {
            case OP_SYNC: {
                SyncMLStatus status;
                m_progData.setStep(ProgressData::PRO_SYNC_PREPARE);
                try {
                    status = m_sync->sync();
                } catch (...) {
                    status = m_sync->handleException();
                }
                if (!m_error) {
                    m_error = status;
                }
                // if there is a connection, then it is no longer needed
                boost::shared_ptr<Connection> c = m_connection.lock();
                if (c) {
                    c->shutdown();
                }
                // report 'sync done' event to listener
                if(m_listener) {
                    m_listener->syncDone(status);
                }
                break;
            }
            case OP_RESTORE:
                m_sync->restore(m_restoreDir,
                                m_restoreBefore ? SyncContext::DATABASE_BEFORE_SYNC :
                                                  SyncContext::DATABASE_AFTER_SYNC);
                break;
            case OP_CMDLINE:
                try {
                    m_cmdline->run(redirect);
                } catch (...) {
                    SyncMLStatus status = Exception::handle();
                    if (!m_error) {
                        m_error = status;
                    }
                }
                m_setConfig = m_cmdline->configWasModified();
                break;
            default:
                break;
            };
        } catch (...) {
            // we must enter SessionCommon::SYNC_DONE under all
            // circumstances, even when failing during connection
            // shutdown or while getting ready for SYNC_RUNNING
            SyncMLStatus status = Exception::handle();
            if (status && !m_error) {
                m_error = status;
            }
            m_syncStatus = SessionCommon::SYNC_DONE;
            m_stepIsWaiting = false;
            fireStatus(true);
            throw;
        }
        m_syncStatus = SessionCommon::SYNC_DONE;
        m_stepIsWaiting = false;
        fireStatus(true);
    }
}

bool Session::setFilters(SyncConfig &config)
{
    /** apply temporary configs to config */
    config.setConfigFilter(true, "", m_syncFilter);
    // set all sources in the filter to config
    BOOST_FOREACH(const SourceFilters_t::value_type &value, m_sourceFilters) {
        config.setConfigFilter(false, value.first, value.second);
    }
    return m_tempConfig;
}

void Session::setStepInfo(bool isWaiting)
{
    // if stepInfo doesn't change, then ignore it to avoid duplicate status info
    if(m_stepIsWaiting != isWaiting) {
        m_stepIsWaiting = isWaiting;
        fireStatus(true);
    }
}

void Session::restore(const string &dir, bool before, const std::vector<std::string> &sources)
{
    if (!m_active) {
        SE_THROW_EXCEPTION(InvalidCall, "session is not active, call not allowed at this time");
    }
    if (m_runOperation == OP_RESTORE) {
        string msg = StringPrintf("restore started, cannot restore again");
        SE_THROW_EXCEPTION(InvalidCall, msg);
    } else if (m_runOperation != OP_NULL) {
        // actually this never happen currently, for during the real restore process,
        // it never poll the sources in default main context
        string msg = StringPrintf("%s started, cannot restore", runOpToString(m_runOperation).c_str());
        SE_THROW_EXCEPTION(InvalidCall, msg);
    }

    m_sync.reset(new DBusSync(getConfigName(), *this));

    if(!sources.empty()) {
        BOOST_FOREACH(const std::string &source, sources) {
            FilterConfigNode::ConfigFilter filter;
            filter["sync"] = "two-way";
            m_sync->setConfigFilter(false, source, filter);
        }
        // disable other sources
        FilterConfigNode::ConfigFilter disabled;
        disabled["sync"] = "disabled";
        m_sync->setConfigFilter(false, "", disabled);
    }
    m_restoreBefore = before;
    m_restoreDir = dir;
    m_runOperation = OP_RESTORE;

    // initiate status and progress and sourceProgress is not calculated currently
    BOOST_FOREACH(const std::string source,
                  m_sync->getSyncSources()) {
        m_sourceStatus[source];
    }
    fireProgress(true);
    fireStatus(true);
}

void Session::SyncStatusOwner::setStatus(SessionCommon::SyncStatus status)
{
    // skip operation before it even starts?
    if (status == SessionCommon::SYNC_RUNNING &&
        (m_status == SessionCommon::SYNC_SUSPEND || m_status == SessionCommon::SYNC_ABORT)) {
        SE_LOG_DEBUG(NULL, NULL, "D-Bus session %s already before it started to run",
                     m_status == SessionCommon::SYNC_SUSPEND ? "suspended" : "aborted");
        SE_THROW_EXCEPTION_STATUS(StatusException,
                                  "preventing start of session as requested by users",
                                  SyncMLStatus(sysync::LOCERR_USERABORT));
    }

    if (status == SessionCommon::SYNC_RUNNING) {
        // activated, allow blockers until we are done again
        m_active = true;
        SE_LOG_DEBUG(NULL, NULL, "D-Bus session running");
    } else if (status == SessionCommon::SYNC_DONE) {
        // deactivated
        m_active = false;
        m_blocker.reset();
        SE_LOG_DEBUG(NULL, NULL, "D-Bus session done, SuspendFlags state %d",
                     (int)SuspendFlags::getSuspendFlags().getState());
    } else if (m_active &&
               (status == SessionCommon::SYNC_SUSPEND || status == SessionCommon::SYNC_ABORT)) {
        // only take global suspend or abort blockers while active
        SE_LOG_DEBUG(NULL, NULL, "running D-Bus session about to %s, taking blocker (SuspendFlags state %d)",
                     status == SessionCommon::SYNC_SUSPEND ? "suspend" : "abort",
                     (int)SuspendFlags::getSuspendFlags().getState());
        m_blocker = status == SessionCommon::SYNC_SUSPEND ?
            SuspendFlags::getSuspendFlags().suspend() :
            SuspendFlags::getSuspendFlags().abort();
        SE_LOG_DEBUG(NULL, NULL, "SuspendFlags state %d after taking blocker in D-Bus session",
                     (int)SuspendFlags::getSuspendFlags().getState());
    }
    SE_LOG_DEBUG(NULL, NULL, "D-Bus session changes state %d -> %d",
                 (int)m_status, (int)status);
    m_status = status;
}

string Session::runOpToString(RunOperation op)
{
    switch(op) {
    case OP_SYNC:
        return "sync";
    case OP_RESTORE:
        return "restore";
    case OP_CMDLINE:
        return "cmdline";
    default:
        return "";
    };
}

void Session::execute(const vector<string> &args, const map<string, string> &vars)
{
    if (!m_active) {
        SE_THROW_EXCEPTION(InvalidCall, "session is not active, call not allowed at this time");
    }
    if (m_runOperation == OP_CMDLINE) {
        SE_THROW_EXCEPTION(InvalidCall, "cmdline started, cannot start again");
    } else if (m_runOperation != OP_NULL) {
        string msg = StringPrintf("%s started, cannot start cmdline", runOpToString(m_runOperation).c_str());
        SE_THROW_EXCEPTION(InvalidCall, msg);
    }
    //create ostream with a specified streambuf
    m_cmdline.reset(new CmdlineWrapper(*this, args, vars));

    if(!m_cmdline->parse()) {
        m_cmdline.reset();
        SE_THROW_EXCEPTION(DBusSyncException, "arguments parsing error");
    }

    m_runOperation = OP_CMDLINE;
}

inline void insertPair(std::map<string, string> &params,
                       const string &key,
                       const string &value)
{
    if(!value.empty()) {
        params.insert(pair<string, string>(key, value));
    }
}

void Session::passwordResponse(bool timed_out, const std::string &password)
{
    m_passwordReqResponse.empty();
    if (!timed_out) {
        if (password.empty()) {
            m_pwResponseStatus = SessionCommon::PW_RES_INVALID;
        } else {
            m_pwResponseStatus = SessionCommon::PW_RES_OK;
            m_passwordReqResponse = password;
        }
    } else {
        m_pwResponseStatus = SessionCommon::PW_RES_TIMEOUT;
    }
}

string Session::askPassword(const string &passwordName,
                            const string &descr,
                            const ConfigPasswordKey &key)
{
    std::map<string, string> params;
    insertPair(params, "description", descr);
    insertPair(params, "user", key.user);
    insertPair(params, "SyncML server", key.server);
    insertPair(params, "domain", key.domain);
    insertPair(params, "object", key.object);
    insertPair(params, "protocol", key.protocol);
    insertPair(params, "authtype", key.authtype);
    insertPair(params, "port", key.port ? StringPrintf("%u",key.port) : "");

    m_pwResponseStatus = SessionCommon::PW_RES_WAITING;
    emitPasswordRequest(params);

    // Wait till we've got a response from our password request.
    while(m_pwResponseStatus == SessionCommon::PW_RES_WAITING) {
        g_main_context_iteration(g_main_context_default(), true);
    }

    // Save the response state and reset status to idle.
    SessionCommon::PwRespStatus respStatus = m_pwResponseStatus;
    m_pwResponseStatus = SessionCommon::PW_RES_IDLE;

    // Check status and take apropriate action.
    if(respStatus == SessionCommon::PW_RES_OK) {
        return m_passwordReqResponse;
    } else if(respStatus == SessionCommon::PW_RES_TIMEOUT) {
        SE_THROW_EXCEPTION_STATUS(StatusException,
                                  std::string("can't get the password from clients. ") +
                                  "The password request has timed out", STATUS_PASSWORD_TIMEOUT);
    } else {
        SE_THROW_EXCEPTION_STATUS(StatusException, "user didn't provide password, abort",
                                  SyncMLStatus(sysync::LOCERR_USERABORT));
    }

    return "";
}

void Session::syncSuccessStart()
{
    // if listener, report 'sync started' to it
    if(m_listener) {
        m_listener->syncSuccessStart();
    }
}

SessionListener* Session::addListener(SessionListener *listener)
{
    SessionListener *old = m_listener;
    m_listener = listener;
    return old;
}

SE_END_CXX
