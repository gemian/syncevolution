/*
 * Copyright (C) 2010 Intel Corporation
 */

#ifndef INCL_WEBDAVSOURCE
#define INCL_WEBDAVSOURCE

#include <config.h>

#ifdef ENABLE_DAV

#include <syncevo/TrackingSyncSource.h>
#include <boost/noncopyable.hpp>
#include "NeonCXX.h"

#include <syncevo/declarations.h>
SE_BEGIN_CXX

/**
 * Implements generic access to a WebDAV collection.
 *
 * Change tracking is based on TrackingSyncSource, with the following mapping:
 * - locally unique id = relative URI of resource in collection
 * - revision string = ETag of resource in collection
 */
class WebDAVSource : public TrackingSyncSource, private boost::noncopyable
{
 public:
    /**
     * @param settings     instance which provides necessary settings callbacks for Neon
     */
    WebDAVSource(const SyncSourceParams &params,
                 const boost::shared_ptr<Neon::Settings> &settings);

 protected:
    /* implementation of SyncSource interface */
    virtual void open();
    virtual bool isEmpty();
    virtual void close();
    virtual Databases getDatabases();

    /* implementation of TrackingSyncSource interface */
    virtual void listAllItems(RevisionMap_t &revisions);
    virtual InsertItemResult insertItem(const string &luid, const std::string &item, bool raw);
    void readItem(const std::string &luid, std::string &item, bool raw);
    virtual void removeItem(const string &uid);

    /**
     * A resource path is turned into a locally unique ID by
     * stripping the calendar path prefix, or keeping the full
     * path for resources outside of the calendar.
     */
    std::string path2luid(const std::string &path);

    /**
     * Full path can be reconstructed from relative LUID by
     * appending it to the calendar path, or using the path
     * as it is.
     */
    std::string luid2path(const std::string &luid);

    /**
     * ETags are turned into revision strings by ignoring the W/ weak
     * marker (because we don't care for literal storage of items) and
     * by stripping the quotation marks.
     */
    std::string ETag2Rev(const std::string &etag);

 protected:
    // access to neon session and calendar, valid between open() and close()
    boost::shared_ptr<Neon::Session> getSession() { return m_session; }
    Neon::URI &getCalendar() { return m_calendar; }

    // access to settings owned by this instance
    Neon::Settings &settings() { return *m_settings; }

    /**
     * return true if resource with the given properties is something we can work with;
     * properties which are queried are currently hard-coded in WebDAVSource::open()
     *
     * @param props    mapping from fully qualified properties to their values,
     *                 normalized by neon library
     */
    virtual bool typeMatches(const StringMap &props) const = 0;

    /**
     * property pointing to URL path with suitable collections ("calendar-home-set", "address-home-set", ...);
     * must be known to WebDAVSource::open() already
     */
    virtual std::string homeSetProp() const = 0;

    /**
     * HTTP content type for PUT
     */
    virtual std::string contentType() const = 0;

    /**
     * create new resource name (only last component, not full path)
     *
     * Some servers require that this matches the item content,
     * for example Yahoo CardDAV wants <uid>.vcf.
     *
     * @param item    original item data
     * @param buffer  empty, may be filled with modified item data
     * @retval luid   new resource name, not URL encoded
     * @return item data to be sent
     */
    virtual const std::string *createResourceName(const std::string &item, std::string &buffer, std::string &luid) { luid = UUID(); return &item; }

    /**
     * optionally modify item content to match the luid of the item we are going to update
     */
    virtual const std::string *setResourceName(const std::string &item, std::string &buffer, const std::string &luid) { return &item; }

 private:
    boost::shared_ptr<Neon::Settings> m_settings;
    boost::shared_ptr<Neon::Session> m_session;

    /** normalized path: including backslash, URI encoded */
    Neon::URI m_calendar;

    /** information about certain paths (path->property->value)*/
    typedef std::map<std::string, std::map<std::string, std::string> > Props_t;
    Props_t m_davProps;

    void openPropCallback(const Neon::URI &uri,
                          const ne_propname *prop,
                          const char *value,
                          const ne_status *status);

    void listAllItemsCallback(const Neon::URI &uri,
                              const ne_prop_result_set *results,
                              RevisionMap_t &revisions,
                              bool &failed);

    /**
     * Extracts ETag from response header, empty if not found.
     */
    std::string getETag(Neon::Request &req) { return ETag2Rev(req.getResponseHeader("ETag")); }

    /**
     * Extracts new LUID from response header, empty if not found.
     */
    std::string getLUID(Neon::Request &req);
};

SE_END_CXX

#endif // ENABLE_DAV
#endif // INCL_WEBDAVSOURCE
