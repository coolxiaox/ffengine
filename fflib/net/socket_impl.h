#ifndef _SOCKET_IMPL_H_
#define _SOCKET_IMPL_H_

#include <list>
#include <string>
using namespace std;

#include "net/socket_i.h"
#include "base/obj_pool.h"

namespace ff {

class epoll_i;
class socket_controller_i;
class task_queue_i;

#define  RECV_BUFFER_SIZE 8096

class ff_str_buffer_t: public ff_buffer_t
{
public:
    ff_str_buffer_t():
        m_seek_index(0)
    {
    }
    void assign(const string& data_, ff_obj_pool_t<ff_str_buffer_t>* pool_)
    {
        m_data = data_;
        m_buff_pool = pool_;
    }
    ~ff_str_buffer_t() {}
    virtual const char* data()
    {
        return m_data.c_str();
    }
    virtual const char* cur_data()
    {
        return m_data.c_str() + m_seek_index;
    }
    virtual int total_size()
    {
        return m_data.size();
    }
    virtual int left_size()
    {
        return this->total_size() - m_seek_index;
    }
    virtual void seek(int n)
    {
        m_seek_index = n;
    }
    virtual void consume(int n)
    {
        m_seek_index += n;
    }
    virtual void release()
    {
        if (m_buff_pool){
            m_buff_pool->release(this);
        }
        else{
            delete this;
        }
    }

protected:
    string  m_data;
    int     m_seek_index;
    ff_obj_pool_t<ff_str_buffer_t>* m_buff_pool;
};

class socket_impl_t: public socket_i
{
public:
    typedef list<ff_buffer_t*>    send_buffer_t;

public:
    socket_impl_t(epoll_i*, socket_controller_i*, int fd, task_queue_i* tq_);
    ~socket_impl_t();

    virtual int socket() { return m_fd; }
    virtual void close();
    virtual void open();

    virtual int handle_epoll_read();
    virtual int handle_epoll_write();
    virtual int handle_epoll_del();

    virtual void async_send(const string& buff_);
    virtual void async_send(ff_buffer_t* buff_);
    virtual void async_recv();
    virtual void safe_delete();
    
    int handle_epoll_read_impl();
    int handle_epoll_write_impl();
    int handle_epoll_error_impl();

    void send_str_impl(const string& buff_);
    void send_impl(ff_buffer_t* buff_);
    void close_impl();
    
    socket_controller_i* get_sc() { return m_sc; }
private:
    bool is_open() { return m_fd > 0; }

    int do_send(ff_buffer_t* buff_);
private:
    epoll_i*                            m_epoll;
    socket_controller_i*                m_sc;
    int                                 m_fd;
    task_queue_i*                       m_tq;
    send_buffer_t                       m_send_buffer;
    ff_obj_pool_t<ff_str_buffer_t>      m_buff_pool;
};

}

#endif
