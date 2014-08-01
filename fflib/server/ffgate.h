//! 连接管理
#ifndef _FF_FFGATE_H_
#define _FF_FFGATE_H_

#include <string>
#include <map>
#include <vector>
#include <set>
#include <queue>
using namespace std;

#include "net/msg_handler_i.h"
#include "base/task_queue_impl.h"
#include "base/ffslot.h"
#include "net/codec.h"
#include "base/thread.h"
#include "rpc/ffrpc.h"
#include "net/msg_sender.h"
#include "base/timer_service.h"
#include "base/arg_helper.h"

namespace ff {
#define DEFAULT_LOGIC_SERVICE "scene@0"

class ffgate_t: public msg_handler_i
{
    enum limite_e
    {
        MAX_MSG_QUEUE_SIZE = 1024
    };
    struct session_data_t;
    struct client_info_t;
public:
    ffgate_t();
    virtual ~ffgate_t();
    
    int open(arg_helper_t& arg);
    int close();
    
    //! 处理连接断开
    int handle_broken(socket_ptr_t sock_);
    //! 处理消息
    int handle_msg(const message_t& msg_, socket_ptr_t sock_);
    
    task_queue_i* get_tq_ptr();
private:
    int close_impl();
    //! 处理连接断开
    //int handle_broken_impl(socket_ptr_t sock_);
    //! 处理消息
    //int handle_msg_impl(const message_t& msg_, socket_ptr_t sock_);
    //! 验证sessionid
    int verify_session_id(const message_t& msg_, socket_ptr_t sock_);
    //! 验证sessionid 的回调函数
    int verify_session_callback(ffreq_t<session_first_entere_t::out_t>& req_, userid_t sock_id_);
    
    //! 逻辑处理,转发消息到logic service
    int route_logic_msg(const message_t& msg_, socket_ptr_t sock_);
    //! 逻辑处理,转发消息到logic service
    int route_logic_msg_callback(ffreq_t<route_logic_msg_t::out_t>& req_, const userid_t& session_id_);
    //! enter scene 回调函数
    int enter_scene_callback(ffreq_t<session_enter_scene_t::out_t>& req_, const userid_t& session_id_);
    
    //! 改变处理client 逻辑的对应的节点
    int change_session_logic(ffreq_t<gate_change_logic_node_t::in_t, gate_change_logic_node_t::out_t>& req_);
    //! 关闭某个session socket
    int close_session(ffreq_t<gate_close_session_t::in_t, gate_close_session_t::out_t>& req_);
    //! 转发消息给client
    int route_msg_to_session(ffreq_t<gate_route_msg_to_session_t::in_t, gate_route_msg_to_session_t::out_t>& req_);
    //! 广播消息给所有的client
    int broadcast_msg_to_session(ffreq_t<gate_broadcast_msg_to_session_t::in_t, gate_broadcast_msg_to_session_t::out_t>& req_);
    
    
    userid_t alloc_id() { return ++m_alloc_id; }
private:
    userid_t                                    m_alloc_id;
    string                                      m_gate_name;
    shared_ptr_t<ffrpc_t>                       m_ffrpc;
    map<userid_t, client_info_t>                m_wait_verify_set;
    map<userid_t/*sessionid*/, client_info_t>   m_client_set;
};


struct ffgate_t::session_data_t
{
    session_data_t(userid_t new_id_ = 0)
    {
        socket_id = new_id_;
        ::time(&online_time);
        session_id = 0;
    }
    bool is_valid()
    {
        return 0 != session_id;
    }
    const userid_t& id() const        { return session_id;    }
    void set_id(const userid_t& s_)   { session_id = s_;      }
    userid_t socket_id;
    userid_t session_id;
    time_t online_time;
};


struct ffgate_t::client_info_t
{
    client_info_t():
        sock(NULL),
        alloc_logic_service(DEFAULT_LOGIC_SERVICE)
    {}
    socket_ptr_t     sock;
    string           alloc_logic_service;
    string           group_name;
    queue<route_logic_msg_t::in_t>    request_queue;//! 请求队列，客户端有可能发送多个请求，但是服务器需要一个一个处理
};

}


#endif
