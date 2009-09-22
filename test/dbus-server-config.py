#! /usr/bin/python

'''Get config/reports and set config.
Usage: dbus-server-sync <server> [--getconfig | --config | --reports] [config options] 
<server> - configuration name
'''

import dbus
from dbus.mainloop.glib import DBusGMainLoop
import gobject
import sys

DBusGMainLoop(set_as_default=True)

bus = dbus.SessionBus()

object = dbus.Interface(bus.get_object('org.syncevolution',
                                       '/org/syncevolution/Server'),
                        'org.syncevolution.Server')

loop = gobject.MainLoop()

sessionpath = None
def SessionChanged(object, ready):
    print "SessionChanged:", object, ready
    if sessionpath == object:
        loop.quit()

bus.add_signal_receiver(SessionChanged,
                        'SessionChanged',
                        'org.syncevolution.Server',
                        'org.syncevolution',
                        None,
                        byte_arrays=True)

dummysessionpath = object.StartSession("")
sessionpath = object.StartSession(sys.argv[1])

# detach from dummy session so that real session can run
session = dbus.Interface(bus.get_object('org.syncevolution',
                                        dummysessionpath),
                         'org.syncevolution.Session')
session.Detach()
session = dbus.Interface(bus.get_object('org.syncevolution',
                                        sessionpath),
                         'org.syncevolution.Session')
if sys.argv[2] == "--getconfig":
    print 'Get Config:'
    dict = session.GetConfig(0)
    for name, value in dict.items():
        print 'name: [', name, ']'
        for config, confvalue in value.items():
            print '\t', config, ' = ', confvalue 
elif sys.argv[2] == "--config":
    print 'Set Config:'
    i = 3;
    length = len(sys.argv)
    dict = {}
    update = 1
    temporary = 0
    while i < length:
        if sys.argv[i] == "--clear":
            update = 0
            i = i + 1
            continue
        elif sys.argv[i] == "--temp":
            temporary = 1
            i = i + 1
            continue
        elif sys.argv[i].startswith('-z'):
            l1 = sys.argv[i].split('=')
            if len(l1) == 2:
                name = 'source/' + l1[1]
        elif sys.argv[i].startswith('-y'):
            name = ''

        l2 = sys.argv[i+1].split('=')
        idict = {}
        if dict.has_key(name):
            idict = dict[name]
            idict[l2[0]] = l2[1]
        else:
            idict[l2[0]] = l2[1]
            dict[name] = idict
        i = i+2

    print dict
    session.SetConfig(update, temporary, dict)
    session.Sync("", {})
elif sys.argv[2] == "--reports":
    dict = session.GetReports(0, 10)
    print ''

