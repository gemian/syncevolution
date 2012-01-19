/*
 * Copyright (C) 2005-2009 Patrick Ohly <patrick.ohly@gmx.de>
 * Copyright (C) 2009 Intel Corporation
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

#include <syncevo/SuspendFlags.h>
#include <syncevo/util.h>

#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>

#include <glib.h>

#include <syncevo/declarations.h>
SE_BEGIN_CXX

SuspendFlags::SuspendFlags() :
    m_state(NORMAL),
    m_lastSuspend(0),
    m_senderFD(-1),
    m_receiverFD(-1)
{
}

SuspendFlags::~SuspendFlags()
{
    deactivate();
}

SuspendFlags &SuspendFlags::getSuspendFlags()
{
    static SuspendFlags flags;
    return flags;
}

static gboolean SignalChannelReadyCB(GIOChannel *source,
                                     GIOCondition condition,
                                     gpointer data) throw()
{
    try {
        SuspendFlags &me = SuspendFlags::getSuspendFlags();
        me.printSignals();
    } catch (...) {
        Exception::handle();
    }

    return TRUE;
}

/**
 * Own glib IO watch for file descriptor
 * which calls printSignals()
 */
class GLibGuard : public SuspendFlags::Guard
{
    GIOChannel *m_channel;
    guint m_channelReady;

public:
    GLibGuard(int fd)
    {
        // glib watch which calls printSignals()
        m_channel = g_io_channel_unix_new(fd);
        m_channelReady = g_io_add_watch(m_channel, G_IO_IN, SignalChannelReadyCB, NULL);
    }

    ~GLibGuard()
    {
        if (m_channelReady) {
            g_source_remove(m_channelReady);
            m_channelReady = 0;
        }
        if (m_channel) {
            g_io_channel_unref(m_channel);
            m_channel = NULL;
        }
    }
};

boost::shared_ptr<SuspendFlags::Guard> SuspendFlags::activate()
{
    int fds[2];
    if (pipe(fds)) {
        SE_THROW(StringPrintf("allocating pipe for signals failed: %s", strerror(errno)));
    }
    // nonblocking, to avoid deadlocks when the pipe's buffer overflows
    fcntl(fds[0], F_SETFL, fcntl(fds[0], F_GETFL) | O_NONBLOCK);
    fcntl(fds[1], F_SETFL, fcntl(fds[1], F_GETFL) | O_NONBLOCK);
    m_senderFD = fds[1];
    m_receiverFD = fds[0];
    sigaction(SIGINT, NULL, &m_oldSigInt);
    sigaction(SIGTERM, NULL, &m_oldSigTerm);

    struct sigaction new_action;
    memset(&new_action, 0, sizeof(new_action));
    new_action.sa_handler = handleSignal;
    sigemptyset(&new_action.sa_mask);
    // don't let processing of SIGINT be interrupted
    // of SIGTERM and vice versa, if we are doing the
    // handling
    if (m_oldSigInt.sa_handler == SIG_DFL) {
        sigaddset(&new_action.sa_mask, SIGINT);
    }
    if (m_oldSigTerm.sa_handler == SIG_DFL) {
        sigaddset(&new_action.sa_mask, SIGTERM);
    }
    if (m_oldSigInt.sa_handler == SIG_DFL) {
        sigaction(SIGINT, &new_action, NULL);
    }
    if (m_oldSigTerm.sa_handler == SIG_DFL) {
        sigaction(SIGTERM, &new_action, NULL);
    }

    return boost::shared_ptr<Guard>(new GLibGuard(m_receiverFD));
}

void SuspendFlags::deactivate()
{
    if (m_receiverFD >= 0) {
        sigaction(SIGTERM, &m_oldSigTerm, NULL);
        sigaction(SIGINT, &m_oldSigInt, NULL);
        close(m_receiverFD);
        close(m_senderFD);
        m_receiverFD = -1;
        m_senderFD = -1;
    }
}

void SuspendFlags::handleSignal(int sig)
{
    SuspendFlags &me(getSuspendFlags());

    unsigned char msg;
    switch (sig) {
    case SIGTERM:
        switch (me.m_state) {
        case ABORT:
            msg = ABORT_AGAIN;
            break;
        default:
            msg = me.m_state = ABORT;
            break;
        }
        break;
    case SIGINT: {
        time_t current;
        time (&current);
        switch (me.m_state) {
        case NORMAL:
            // first time suspend or already aborted
            msg = me.m_state = SUSPEND;
            me.m_lastSuspend = current;
            break;
        case SUSPEND:
            // turn into abort?
            if (current - me.m_lastSuspend < ABORT_INTERVAL) {
                msg = me.m_state = ABORT;
            } else {
                me.m_lastSuspend = current;
                msg = SUSPEND_AGAIN;
            }
            break;
        case SuspendFlags::ABORT:
            msg = ABORT_AGAIN;
            break;
        case SuspendFlags::ABORT_AGAIN:
        case SuspendFlags::SUSPEND_AGAIN:
            // shouldn't happen
            break;
        }
        break;
    }
    }
    if (me.m_senderFD >= 0) {
        write(me.m_senderFD, &msg, 1);
    }
}

void SuspendFlags::printSignals()
{
    if (m_receiverFD >= 0) {
        unsigned char msg;
        while (read(m_receiverFD, &msg, 1) == 1) {
            const char *str = NULL;
            switch (msg) {
            case SUSPEND:
                str = "Asking to suspend...\nPress CTRL-C again quickly (within 2s) to stop immediately (can cause problems in the future!)";
                break;
            case SUSPEND_AGAIN:
                str = "Suspend in progress...\nPress CTRL-C again quickly (within 2s) to stop immediately (can cause problems in the future!)";
                break;
            case ABORT:
                str = "Aborting immediately ...";
                break;
            case ABORT_AGAIN:
                str = "Already aborting as requested earlier ...";
                break;
            }
            if (!str) {
                SE_LOG_DEBUG(NULL, NULL, "internal error: received invalid signal msg %d", msg);
            } else {
                SE_LOG_INFO(NULL, NULL, "%s", str);
            }
            m_stateChanged(*this);
        }
    }
}

SE_END_CXX
