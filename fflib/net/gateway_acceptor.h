#ifndef _GATEWAY_ACCEPTOR_H_
#define _GATEWAY_ACCEPTOR_H_

#include "net/acceptor_impl.h"
#include "net/common_socket_controller.h"
#include "net/net_stat.h"
#include "base/arg_helper.h"

namespace ff {

class gateway_acceptor_t: public acceptor_impl_t
{
public:
    gateway_acceptor_t(epoll_i*, msg_handler_i*, task_queue_pool_t* tq_);
    ~gateway_acceptor_t();

    int open(const string& address_);
    int open(arg_helper_t& arg_helper);

protected:
    virtual socket_i* create_socket(int new_fd_);

private:
    //! data field
    net_stat_t   m_net_stat;
};

}
#endif
