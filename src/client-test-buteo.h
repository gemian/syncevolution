/*
 * Copyright (C) 2010 Intel Corporation
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
#ifndef INCL_SYNC_BUTEOTEST
#define INCL_SYNC_BUTEOTEST

#include <syncevo/util.h>
#include <syncevo/declarations.h>
#include <QString>
#include <QObject>
#include <QtDBus>
#include <QDomDocument>
#include "ClientTest.h"

class ButeoTest : public QObject {
    Q_OBJECT
public:
    ButeoTest(const string &server,
              const string &logbase,
              const SyncEvo::SyncOptions &options); 

    ~ButeoTest();

    // prepare sync sources
    void prepareSources(const int *sources, const vector<string> &source2Config);

    // do actually sync
    SyncEvo::SyncMLStatus doSync(SyncEvo::SyncReport *report); 

private slots:
    void syncStatus(QString, int, QString, int);
    void resultsAvailable(QString, QString);
    void serviceUnregistered(QString);

private:

    /** init device ids used for 2 clients
     */
    static void initDeviceIds();

    /**
     * 1. set deviceid, max-message-size options to /etc/sync/meego-sync-conf.xml
     * 2. set wbxml option, sync mode, enabled selected sources and disable other sources 
     */
    void setupOptions();

    // kill all msyncd  
    void killAllMsyncd();

    // start msyncd  
    void startMsyncd();

    // do actually running 
    bool run();

    // get sync results from buteo and set them to sync report
    void genSyncResults(const QString &text, SyncEvo::SyncReport *report);

    // truncate file and write content to file
    static void writeToFile(const QString &filePath, const QString &content );

    //replace the element value with a new value
    static void replaceElement(QString &xml, const QString &elem, const QString &value);

    // build a dom tree from file
    static void buildDomFromFile(QDomDocument &doc, const QString &filePath);

    string m_server;
    string m_logbase;
    SyncEvo::SyncOptions m_options;
    std::set<string> m_configedSources;
    QString m_syncResults;

    //dbus watcher to watch buteo daemon 
    QDBusServiceWatcher *m_dbusWatcher;

    //device ids
    static QString m_deviceIds[2];
    //mappings for syncevolution source and buteo storage
    static map<string, string> m_source2storage;
    //flag for initialization
    static bool m_inited;
};


/**
 * Qtcontacts use tracker to store data. However, it can't specify
 * the place where to store them. Since we have to separate client A
 * and B's data, restore and backup their databases
 */
class QtContactsSwitcher {
public:
    /**
     * prepare storage:
     * 1. terminate tracker
     * 2. copy tracker databases from backup to its default place
     * according to id
     * 3. restart tracker 
     */
    static void restoreStorage(const string &id);

    /**
     * backup storage:
     * 1. terminate tracker
     * 2. copy tracker databases from default place to backup
     * 3. restart tracker
     */
    static void backupStorage(const string &id);
private:
    // get the file path of databases
    static std::string getDatabasePath();

    //terminate tracker daemons
    static void terminate();

    //start tracker daemons
    static void start();

    //copy databases between default place and backup place
    static void copyDatabases(const string &id, bool fromDefault = true);

    //databases used by tracker
    static std::string m_databases[];
};
#endif
