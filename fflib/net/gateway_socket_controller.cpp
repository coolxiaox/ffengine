
#include "net/gateway_socket_controller.h"
#include "net/net_stat.h"
#include "base/log.h"

#include <stdio.h>
using namespace ff;
#define FFNET "FFNET"

gateway_socket_controller_t::gateway_socket_controller_t(msg_handler_ptr_t msg_handler_, net_stat_t* ns_):
    common_socket_controller_t(msg_handler_),
    m_first_pkg(true),
    m_crossfile_req(false),
    m_net_stat(ns_),
    m_last_update_tm(0)
{
    
}

gateway_socket_controller_t::~gateway_socket_controller_t()
{
    
}

int gateway_socket_controller_t::handle_open(socket_i* s_)
{
    m_last_update_tm = m_net_stat->get_heartbeat().tick();
    m_net_stat->get_heartbeat().add(s_);
    return 0;
}

int gateway_socket_controller_t::handle_read(socket_i* s_, char* buff, size_t len)
{
    LOGTRACE((FFNET, "gateway_socket_controller_t::handle_read begin len<%u>", len));
    if (m_first_pkg)
    {
        m_first_pkg = false;
        const char* cross_file_req = "<policy-file-request/>";
        size_t tmp = len > 22? 22: len;
        size_t i   = 0;
        for (; i < tmp; ++i)
        {
            if (buff[i] != cross_file_req[i])
            {
                break;
            }
        }
        if (i == tmp)
        {
            LOGINFO((FFNET, "gateway_socket_controller_t::handle_read begin crossfile len<%u>", len));
            m_crossfile_req = true;
            string ret = "<cross-domain-policy><allow-access-from domain=\"*\" to-ports=\"*\"/></cross-domain-policy>";
            ret.append("\0", 1);
            s_->async_send(ret);
        }
    }
    if (m_crossfile_req)
    {
        return 0;
    }
    common_socket_controller_t::handle_read(s_, buff, len);

    //! 判断消息包是否超过限制

    if (true == m_message.have_recv_head(m_have_recv_size) && m_message.size() > (size_t)m_net_stat->get_max_packet_size())
    {
        s_->close();
    }

    //! 更新心跳
    if (m_last_update_tm != m_net_stat->get_heartbeat().tick())
    {
        m_last_update_tm = m_net_stat->get_heartbeat().tick();
        m_net_stat->get_heartbeat().update(s_);
    }

    LOGTRACE((FFNET, "gateway_socket_controller_t::handle_read end msg size<%u>", m_message.size()));
    return 0;
}

int gateway_socket_controller_t::handle_error(socket_i* s_)
{
    m_net_stat->get_heartbeat().del(s_);
    common_socket_controller_t::handle_error(s_);
    return 0;
}
