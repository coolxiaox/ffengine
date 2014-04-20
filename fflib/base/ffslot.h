
#ifndef _FF_SLOT_H_
#define _FF_SLOT_H_

#include <assert.h>
#include <string>
#include <map>
using namespace std;

namespace ff
{

class ffslot_t //! 封装回调函数的注册、回调
{
public:
    class callback_arg_t
    {
    public:
        virtual ~callback_arg_t(){}
        virtual int type() = 0;
    };    
    class callback_t
    {
    public:
        virtual ~callback_t(){}
        virtual void exe(ffslot_t::callback_arg_t* args_) = 0;
        virtual callback_t* fork() { return NULL; }
    };

public:
    virtual ~ffslot_t()
    {
        clear();
    }
    ffslot_t& bind(long cmd_, callback_t* callback_)
    {
        assert(callback_);
		this->del(cmd_);
        m_cmd2callback.insert(make_pair(cmd_, callback_));
        return *this;
    }
    ffslot_t& bind(const string& cmd_, callback_t* callback_)
    {
        assert(callback_);
		this->del(cmd_);
        m_name2callback.insert(make_pair(cmd_, callback_));
        return *this;
    }
    callback_t* get_callback(long cmd_)
    {
        map<long, callback_t*>::iterator it = m_cmd2callback.find(cmd_);
        if (it != m_cmd2callback.end())
        {
            return it->second;
        }
        return NULL;
    }
    callback_t* get_callback(const string& cmd_)
    {
        map<string, callback_t*>::iterator it = m_name2callback.find(cmd_);
        if (it != m_name2callback.end())
        {
            return it->second;
        }
        return NULL;
    }
    void del(long cmd_)
    {
        map<long, callback_t*>::iterator it = m_cmd2callback.find(cmd_);
        if (it != m_cmd2callback.end())
        {
            delete it->second;
            m_cmd2callback.erase(it);
        }
    }
    void del(const string& cmd_)
    {
        map<string, callback_t*>::iterator it = m_name2callback.find(cmd_);
        if (it != m_name2callback.end())
        {
            delete it->second;
            m_name2callback.erase(it);
        }
    }
    void clear()
    {
        map<long, callback_t*>::iterator it = m_cmd2callback.begin();
        for (; it != m_cmd2callback.end(); ++it)
        {
            delete it->second;
        }
        map<string, callback_t*>::iterator it2 = m_name2callback.begin();
        for (; it2 != m_name2callback.end(); ++it2)
        {
            delete it2->second;
        }
        m_cmd2callback.clear();
        m_name2callback.clear();
    }
    map<string, callback_t*>&  get_str_cmd() { return  m_name2callback; }
private:
    map<long, callback_t*>       m_cmd2callback;
    map<string, callback_t*>    m_name2callback;
};


}
#endif

