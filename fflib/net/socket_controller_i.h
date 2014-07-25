#ifndef _SOCKET_CONTROLLER_I_
#define _SOCKET_CONTROLLER_I_

#include <string>
using namespace std;

namespace ff {

class socket_i;
class ff_buffer_t;

class socket_controller_i
{
public:
    virtual ~socket_controller_i(){}

    virtual int handle_open(socket_i*)                          {return 0;}
    virtual int check_pre_send(socket_i*, ff_buffer_t*)         {return 0;}

    virtual int handle_error(socket_i*)                                  = 0;
    virtual int handle_read(socket_i*, char* buff, size_t len)  = 0;
    virtual int handle_write_completed(socket_i*)  {return 0;}

};

}

#endif
