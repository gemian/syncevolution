/*
 * Copyright (C) 2009 Patrick Ohly
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef INCL_SOUPTRANSPORTAGENT
#define INCL_SOUPTRANSPORTAGENT

#include <config.h>

#ifdef ENABLE_LIBSOUP

#include "TransportAgent.h"
#include "EvolutionSmartPtr.h"
#include <libsoup/soup.h>
#include <glib/gmain.h>

namespace SyncEvolution {

class GLibUnref {
 public:
    static void unref(GMainLoop *pointer) { g_main_loop_unref(pointer); }
    static void unref(SoupMessageBody *pointer) { soup_message_body_free(pointer); }
    static void unref(SoupBuffer *pointer) { soup_buffer_free(pointer); }
    static void unref(SoupURI *pointer) { soup_uri_free(pointer); }
};

/**
 * message send/receive with libsoup
 *
 * An asynchronous soup session is used and the main loop
 * is invoked in the wait() method to make progress.
 */
class SoupTransportAgent : public TransportAgent
{
 public:
    SoupTransportAgent();
    ~SoupTransportAgent();

    virtual void setURL(const std::string &url);
    virtual void setProxy(const std::string &proxy);
    virtual void setProxyAuth(const std::string &user, const std::string &password);
    virtual void setContentType(const std::string &type);
    virtual void send(const char *data, size_t len);
    virtual void cancel();
    virtual Status wait();
    virtual void getReply(const char *&data, size_t &len);

 private:
    std::string m_proxyUser;
    std::string m_proxyPassword;
    std::string m_URL;
    std::string m_contentType;
    eptr<SoupSession, GObject> m_session;
    eptr<GMainLoop, GMainLoop, GLibUnref> m_loop;
    Status m_status;

    /** response, copied from SoupMessage */
    eptr<SoupBuffer, SoupBuffer, GLibUnref> m_response;

    /** SoupSessionCallback, redirected into user_data->HandleSessionCallback() */
    static void SessionCallback(SoupSession *session,
                                SoupMessage *msg,
                                gpointer user_data);
    void HandleSessionCallback(SoupSession *session,
                               SoupMessage *msg);
};

} // namespace SyncEvolution

#endif // ENABLE_LIBSOUP
#endif // INCL_TRANSPORTAGENT
