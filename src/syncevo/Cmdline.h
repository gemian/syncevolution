/*
 * Copyright (C) 2008-2009 Patrick Ohly <patrick.ohly@gmx.de>
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

#ifndef INCL_SYNC_EVOLUTION_CMDLINE
# define INCL_SYNC_EVOLUTION_CMDLINE

#include <syncevo/SyncConfig.h>
#include <syncevo/FilterConfigNode.h>

#include <set>
using namespace std;

#include <boost/shared_ptr.hpp>
#include <boost/scoped_array.hpp>

#include <syncevo/declarations.h>
SE_BEGIN_CXX

class SyncSource;
class SyncContext;
class CmdlineTest;

class Cmdline {
public:
    /**
     * @param out      stdout stream for normal messages
     * @param err      stderr stream for error messages
     */
    Cmdline(int argc, const char * const *argv, ostream &out, ostream &err);
    Cmdline(const vector<string> &args, ostream &out, ostream &err);

    /**
     * parse the command line options
     *
     * @retval true if command line was okay
     */
    bool parse();

    /**
     * @return false if run() still needs to be invoked, true when parse() already did
     *         the job (like --sync-property ?)
     */
    bool dontRun() const { return m_dontrun; }

    bool run();

    /**
     * Acts like a boolean, but in addition, can also tell whether the
     * value was explicitly set.
     */
    class Bool { 
    public:
    Bool(bool val = false) : m_value(val), m_wasSet(false) {}
        operator bool () const { return m_value; }
        Bool & operator = (bool val) { m_value = val; m_wasSet = true; return *this; }
        bool wasSet() const { return m_wasSet; }
    private:
        bool m_value;
        bool m_wasSet;
    };

    Bool useDaemon() { return m_useDaemon; }

    /** whether '--monitor' is set */
    bool monitor() { return m_monitor; }

    /** whether 'status' is set */
    bool status() { return m_status; }

    /* server name */
    string getConfigName() { return m_server; }

    /* check whether command line runs sync. It should be called after parsing. */
    bool isSync();

protected:
    // vector to store strings for arguments 
    vector<string> m_args;

    int m_argc;
    const char * const * m_argv;
    ostream &m_out, &m_err;

    //array to store pointers of arguments
    boost::scoped_array<const char *> m_argvArray;

    Bool m_quiet;
    Bool m_dryrun;
    Bool m_status;
    Bool m_version;
    Bool m_usage;
    Bool m_configure;
    Bool m_remove;
    Bool m_run;
    Bool m_migrate;
    Bool m_printServers;
    Bool m_printTemplates;
    Bool m_printConfig;
    Bool m_printSessions;
    Bool m_dontrun;
    Bool m_keyring;
    Bool m_monitor;
    Bool m_useDaemon;
    FilterConfigNode::ConfigFilter m_syncProps, m_sourceProps;
    const ConfigPropertyRegistry &m_validSyncProps;
    const ConfigPropertyRegistry &m_validSourceProps;

    string m_restore;
    Bool m_before, m_after;

    string m_server;
    string m_template;
    set<string> m_sources;

    /** compose description of cmd line option with optional parameter */
    static string cmdOpt(const char *opt, const char *param = NULL);

    /**
     * parse sync or source property
     *
     * @param validProps     list of valid properties
     * @retval props         add property name/value pair here
     * @param opt            command line option as it appeard in argv (e.g. --sync|--sync-property|-z)
     * @param param          the parameter following the opt, may be NULL if none given (error!)
     * @param propname       if given, then this is the property name and param contains the param value (--sync <param>)
     */
    bool parseProp(const ConfigPropertyRegistry &validProps,
                   FilterConfigNode::ConfigFilter &props,
                   const char *opt,
                   const char *param,
                   const char *propname = NULL);

    bool listPropValues(const ConfigPropertyRegistry &validProps,
                        const string &propName,
                        const string &opt);

    bool listProperties(const ConfigPropertyRegistry &validProps,
                        const string &opt);

    typedef map<string, ConfigProps> SourceFilters_t;

    /**
     * read properties from context, then update with
     * command line properties
     *
     * @param context         context name, without @ sign
     * @retval syncFilter     global sync properties
     * @retval sourceFilters  entries for specific sources, key "" as fallback
     */
    void getFilters(const string &context,
                    ConfigProps &syncFilter,
                    SourceFilters_t &sourceFilters);

    /**
     * check that m_syncProps and m_sourceProps don't contain
     * properties which only apply to peers, throw error
     * if found
     */
    void checkForPeerProps();

    /**
     * list all known data sources of a certain type
     */
    void listSources(SyncSource &syncSource, const string &header);

    void dumpConfigs(const string &preamble,
                     const SyncConfig::ConfigList &servers);

    void dumpConfigTemplates(const string &preamble,
                     const SyncConfig::TemplateList &templates,
                     bool printRank = false);

    enum DumpPropertiesFlags {
        DUMP_PROPS_NORMAL = 0,
        HIDE_LEGEND = 1<<0,       /**<
                                   * do not show the explanation which properties are shared,
                                   * used while dumping any source which is not the last one
                                   */
        HIDE_PER_PEER = 1<<1      /**<
                                   * config is for a context, not a peer, so do not show those
                                   * properties which are only per-peer
                                   */
    };
    void dumpProperties(const ConfigNode &configuredProps,
                        const ConfigPropertyRegistry &allProps,
                        int flags);

    void copyProperties(const ConfigNode &fromProps,
                        ConfigNode &toProps,
                        bool hidden,
                        const ConfigPropertyRegistry &allProps);

    void dumpComment(ostream &stream,
                     const string &prefix,
                     const string &comment);

    /** print usage information */
    void usage(bool full,
               const string &error = string(""),
               const string &param = string(""));

    /**
     * This is a factory method used to delay sync client creation to its
     * subclass. The motivation is to let user implement their own 
     * clients to avoid dependency.
     * @return the created sync client
     */
    virtual SyncContext* createSyncClient();

    friend class CmdlineTest;

 private:
    /**
     * Utility function to check m_argv[opt] against a specific boolean
     * parameter of the form "<longName|shortName>[=yes/1/t/true/no/0/f/false].
     *
     * @param opt        current index in m_argv
     * @param longName   long form of the parameter, including --, may be NULL
     * @param shortName  short form, including -, may be NULL
     * @param  def       default value if m_argv[opt] contains no explicit value
     * @retval value     if and only if m_argv[opt] matches, then this is set to to true or false
     * @retval ok        true if parsing succeeded, false if not and error message was printed
     */
    bool parseBool(int opt, const char *longName, const char *shortName,
                   bool def, Bool &value,
                   bool &ok);
};


SE_END_CXX
#endif // INCL_SYNC_EVOLUTION_CMDLINE
