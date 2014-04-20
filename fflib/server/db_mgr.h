#ifndef _FF_DB_MGR_H_
#define _FF_DB_MGR_H_

#include <assert.h>
#include <string>
#include <vector>
#include <map>
using namespace std;


#include "base/log.h"
#include "base/smart_ptr.h"
#include "db/ffdb.h"
#include "base/lock.h"
#include "base/ffslot.h"
#include "base/fftype.h"

namespace ff
{

class db_mgr_t
{
public:
    struct db_connection_info_t;
    class db_query_result_t;
public:
    db_mgr_t();
    ~db_mgr_t();
    int start();
    int stop();

    long connect_db(const string& host_);
    void db_query(long db_id_,const string& sql_, ffslot_t::callback_t* callback_);
    int  sync_db_query(long db_id_,const string& sql_, vector<vector<string> >& ret_data_);

    void db_query_impl(db_connection_info_t* db_connection_info_, const string& sql_, ffslot_t::callback_t* callback_);
private:
    long                                        m_db_index;
    vector<shared_ptr_t<task_queue_t> >         m_tq;
    mutex_t                                     m_mutex;
    map<long/*dbid*/, db_connection_info_t>     m_db_connection;
    thread_t                                    m_thread;
};

class db_mgr_t::db_query_result_t: public ffslot_t::callback_arg_t
{
public:
    db_query_result_t():
        ok(false)
    {}
    virtual int type()
    {
        return TYPEID(db_query_result_t);
    }
    void clear()
    {
        ok = false;
        result_data.clear();
        col_names.clear();
    }
    bool                    ok;
    vector<vector<string> > result_data;
    vector<string>          col_names;
};

struct db_mgr_t::db_connection_info_t
{
    db_connection_info_t()
    {
    }
    db_connection_info_t(const db_connection_info_t& src_):
        tq(src_.tq),
        db(src_.db)
    {
    }
    shared_ptr_t<task_queue_t> tq;
    shared_ptr_t<ffdb_t>       db;
    db_query_result_t          ret;
};

}

#endif
