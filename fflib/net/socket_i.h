#ifndef _SOCKET_I_
#define _SOCKET_I_

#include <string>
using namespace std;

#include "netbase.h"

namespace ff {

class ff_buffer_t
{
public:
    virtual ~ff_buffer_t() {}
    virtual const char* data()       = 0;
    virtual const char* cur_data()   = 0;
    virtual int         total_size() = 0;
    virtual int         left_size()  = 0;
    virtual void        seek(int n)  = 0;
    virtual void        consume(int n)= 0;
    virtual void        release()    = 0;
};

class socket_i: public fd_i
{
public:
    socket_i():
        m_data(NULL) {}
    virtual ~socket_i(){}

    virtual void open() = 0;
    virtual void async_send(const string& buff_) = 0;
    virtual void async_send(ff_buffer_t* buff_)  = 0;
    virtual void async_recv() = 0;
    virtual void safe_delete() { delete this; }
    virtual void set_data(void* p) { m_data = p; }
    template<typename T>
    T* get_data() const { return (T*)m_data; }
    
private:
    void*   m_data;
};

typedef socket_i*  socket_ptr_t;

}

#endif
