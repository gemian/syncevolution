/*
 * Copyright (C) 2008-2009 Patrick Ohly <patrick.ohly@gmx.de>
 */

#ifndef INCL_EVOLUTION_FILE_CONFIG_NODE
# define INCL_EVOLUTION_FILE_CONFIG_NODE

#include "ConfigNode.h"

#include <string>
#include <list>
using namespace std;

/**
 * This class started its life as the Posix implementation of the
 * ManagementNode in the Funambol C++ client library. Nowadays it is
 * part of the SyncEvoluition ConfigTree (see there for details).
 *
 * Each node is mapped to one file whose location is determined by
 * the ConfigTree when the node gets created. Each node represents
 * one .ini file with entries of the type
 * <property>\s*=\s*<value>\s*\n
 *
 * Comments look like:
 * \s*# <comment>
 *
 * @todo rewrite with standard C++ containers
 */
class FileConfigNode : public ConfigNode {
    string m_path;
    string m_fileName;

    list<string> m_lines;
    bool m_modified;
    const bool m_readonly;
    bool m_exists;

    void read();

 public:
    /**
     * Open or create a new file. The file will be read (if it exists)
     * but not create or written to unless flush() is called explicitly
     *
     * @param path      node name, maps to directory
     * @param fileName  name of file inside that directory
     * @param readonly  do not create or write file, it must exist;
     *                  flush() will throw an exception when changes would have to be written
     */
    FileConfigNode(const string &path, const string &fileName, bool readonly);

    /* keep underlying methods visible; our own setProperty() would hide them */
    using ConfigNode::setProperty;

    virtual string getName() const { return m_path + "/" + m_fileName; }

    virtual void flush();
    virtual string readProperty(const string &property) const;
    virtual void setProperty(const string &property,
                             const string &value,
                             const string &comment = "",
                             const string *defValue = NULL);
    virtual void readProperties(map<string, string> &props) const;
    virtual void removeProperty(const string &property);
    virtual bool exists() const { return m_exists; }
};

#endif
