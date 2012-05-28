// Copyright (c) 2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.

#ifndef _BITCOIN_LOGDB_H_
#define _BITCOIN_LOGDB_H_

#include <map>
#include <set>

#include <openssl/sha.h>
#include <boost/thread/shared_mutex.hpp>
#include <boost/thread/locks.hpp>

#include "sync.h"
#include "serialize.h"
#include "version.h"

typedef std::vector<unsigned char> data_t;

class CLogDB;

class CLogDBFile
{
private:
    mutable boost::shared_mutex mutex;

    FILE *file;
    SHA256_CTX ctxState;

    // database
    std::map<data_t, data_t> mapData;
    size_t nUsed; // continuously updated
    size_t nWritten; // updated when writing a new block

    mutable CCriticalSection cs_nRefCount;
    int nRefCount; // number of attached CLogDB's

    // cached changes
    std::set<data_t> setDirty;

    friend class CLogDB;

protected:
    void Init_();
    bool Load_();
    bool Write_(const data_t &key, const data_t &value, bool fOverwrite = true, bool fLoad = false);
    bool Read_(const data_t &key, data_t &value) const;
    bool Exists_(const data_t &key) const;
    bool Erase_(const data_t &key, bool fLoad = false);
    bool Flush_();
    bool Close_();

public:
    CLogDBFile()
    {
        Init_();
    }

    ~CLogDBFile()
    {
        Close_();
    }

    bool Open(const char *pszFile, bool fCreate = true)
    {
        boost::lock_guard<boost::shared_mutex> lock(mutex);
        Close_();

        file = fopen(pszFile, fCreate ? "a+b" : "r+b");

        if (file == NULL) {
            printf("Error opening %s: %s\n", pszFile, strerror(errno));
                return false;
        }

        return Load_();
    }

//    bool Flush()            { CRITICAL_BLOCK(cs) return Flush_();          return false; }
//    bool Close()            { CRITICAL_BLOCK(cs) return Close_();          return false; }
//    bool IsDirty() const    { CRITICAL_BLOCK(cs) return !setDirty.empty(); return false; }
//    bool IsOpen() const     { return file != NULL; }

    bool Close() {
        boost::lock_guard<boost::shared_mutex> lock(mutex);
        return Close_();
    }
};

class CLogDB
{
public:
    typedef data_t key_type;
    typedef data_t value_type;
    typedef std::map<key_type, value_type>::const_iterator const_iterator;

private:
    mutable CCriticalSection cs;
    CLogDBFile * const db; // const pointer to non-const db
    const bool fReadOnly; // readonly CLogDB's use a shared lock instead of a normal

    bool fTransaction; // true inside a transaction
    std::map<data_t, data_t> mapData; // must be empty outside transactions
    std::set<data_t> setDirty;

public:
    bool TxnAbort();
    bool TxnBegin();
    bool TxnCommit();

    CLogDB(CLogDBFile *dbIn, bool fReadOnlyIn = false) : db(dbIn), fReadOnly(fReadOnlyIn), fTransaction(false) {
        LOCK(db->cs_nRefCount);
        db->nRefCount++;
    }

    ~CLogDB() {
        TxnAbort();

        LOCK(db->cs_nRefCount);
        db->nRefCount--;
        if (db->nRefCount == 0) {
            boost::lock_guard<boost::shared_mutex> lock(db->mutex);
            db->Flush_();
        }
    }

protected:
    bool Write_(const data_t &key, const data_t &value, bool fOverwrite = true);
    bool Erase_(const data_t &key);
    bool Read_(const data_t &key, data_t &value);
    bool Exists_(const data_t &key);

public:
    // only reads committed data, no local modifications
    const_iterator begin() const { return db->mapData.begin(); }
    const_iterator end() const   { return db->mapData.end(); }

    template<typename K, typename V>
    bool Write(const K &key, const V &value, bool fOverwrite = true)
    {
        CDataStream ssk(SER_DISK, CLIENT_VERSION);
        ssk << key;
        data_t datak(ssk.begin(), ssk.end());
        CDataStream ssv(SER_DISK, CLIENT_VERSION);
        ssv << value;
        data_t datav(ssv.begin(), ssv.end());
        return Write_(datak, datav, fOverwrite);
    }

    template<typename K, typename V>
    bool Read(const K &key, V &value)
    {
        CDataStream ssk(SER_DISK, CLIENT_VERSION);
        ssk << key;
        data_t datak(ssk.begin(), ssk.end());
        data_t datav;
        if (!Read_(datak,datav))
            return false;
        CDataStream ssv(datav, SER_DISK, CLIENT_VERSION);
        ssv >> value;
        return true;
    }

    template<typename K>
    bool Exists(const K &key)
    {
        CDataStream ssk(SER_DISK, CLIENT_VERSION);
        ssk << key;
        data_t datak(ssk.begin(), ssk.end());
        return Exists_(datak);
    }

    template<typename K>
    bool Erase(const K &key)
    { 
        CDataStream ssk(SER_DISK, CLIENT_VERSION);
        ssk << key;
        data_t datak(ssk.begin(), ssk.end());
        return Erase_(datak);
    }
};

#endif
