/*
 * Copyright (C) 2008-2009 Patrick Ohly <patrick.ohly@gmx.de>
 */

#ifndef INCL_EVOLUTION_SAFE_CONFIG_NODE
# define INCL_EVOLUTION_SAFE_CONFIG_NODE

#include <ConfigNode.h>
#include <boost/shared_ptr.hpp>

#include <map>
#include <utility>
#include <vector>
#include <string>
using namespace std;

/**
 * This class acts as filter between a real config node and its user:
 * key/value strings which normally wouldn't be valid are escaped
 * before passing them into the underlying node. When reading, they
 * are unescaped again.
 *
 * Unsafe characters are replaced by ! followed by two characters
 * giving the character value in hex notation.
 */
class SafeConfigNode : public ConfigNode {
 public:
    /** read-write access to underlying node */
    SafeConfigNode(const boost::shared_ptr<ConfigNode> &node);

    /** read-only access to underlying node */
    SafeConfigNode(const boost::shared_ptr<const ConfigNode> &node);

    /**
     * chooses which characters are accepted by underlying node:
     * in strict mode, only alphanumeric and -_ are supported;
     * in non-strict mode, only line breaks, = and spaces at start and end are escaped
     */
    void setMode(bool strict) { m_strictMode = strict; }
    bool getMode() { return m_strictMode; }

    virtual string getName() const { return m_readOnlyNode->getName(); }

    /* keep underlying methods visible; our own setProperty() would hide them */
    using ConfigNode::setProperty;

    /* ConfigNode API */
    virtual void flush();
    virtual string readProperty(const string &property) const;
    virtual void setProperty(const string &property,
                             const string &value,
                             const string &comment = "",
                             const string *defValue = NULL);
    virtual void readProperties(map<string, string> &props) const;
    virtual void removeProperty(const string &property);
    virtual bool exists() const { return m_readOnlyNode->exists(); }

 private:
    boost::shared_ptr<ConfigNode> m_node;
    boost::shared_ptr<const ConfigNode> m_readOnlyNode;
    bool m_strictMode;

    /**
     * turn str into something which can be used as key or value in ConfigNode
     */
    string escape(const string &str) const;

    /** inverse operation for escape() */
    string unescape(const string &str) const;
};

#endif
