#include "server/ffgate.h"
#include "net/net_factory.h"
#include "base/log.h"
#include "net/socket_op.h"

using namespace ff;

#define FFGATE                   "FFGATE"

ffgate_t::ffgate_t():
    m_alloc_id(0)
{
    
}
ffgate_t::~ffgate_t()
{
    
}

int ffgate_t::open(arg_helper_t& arg_helper)
{
    LOGTRACE((FFGATE, "ffgate_t::open begin broker<%s>", arg_helper.get_option_value("-broker")));
    if (false == arg_helper.is_enable_option("-gate"))
    {
        LOGERROR((FFGATE, "ffgate_t::open failed without -gate argmuent"));
        return -1;
    }
    m_gate_name = arg_helper.get_option_value("-gate");
    m_ffrpc = new ffrpc_t(m_gate_name);
    
    m_ffrpc->reg(&ffgate_t::change_session_logic, this);
    m_ffrpc->reg(&ffgate_t::close_session, this);
    m_ffrpc->reg(&ffgate_t::route_msg_to_session, this);
    m_ffrpc->reg(&ffgate_t::broadcast_msg_to_session, this);
    
    if (m_ffrpc->open(arg_helper))
    {
        LOGERROR((FFGATE, "ffgate_t::open failed check -broker argmuent"));
        return -1;
    }
    
    if (NULL == net_factory_t::gateway_listen(arg_helper, this))
    {
        LOGERROR((FFGATE, "ffgate_t::open failed without -gate_listen"));
        return -1;
    }
    
    LOGTRACE((FFGATE, "ffgate_t::open end ok"));
    return 0;
}
int ffgate_t::close()
{
    if (m_ffrpc)
        m_ffrpc->get_tq().produce(task_binder_t::gen(&ffgate_t::close_impl, this));
    return 0;
}
int ffgate_t::close_impl()
{
    map<userid_t/*sessionid*/, client_info_t>::iterator it = m_client_set.begin();
    for (; it != m_client_set.end(); ++it)
    {
        it->second.sock->close();
    }
    if (m_ffrpc)
    {
        m_ffrpc->close();
    }
    return 0;
}
//! 处理连接断开
int ffgate_t::handle_broken(socket_ptr_t sock_)
{
    m_ffrpc->get_tq().produce(task_binder_t::gen(&ffgate_t::handle_broken_impl, this, sock_));
    return 0;
}
//! 处理消息
int ffgate_t::handle_msg(const message_t& msg_, socket_ptr_t sock_)
{
    m_ffrpc->get_tq().produce(task_binder_t::gen(&ffgate_t::handle_msg_impl, this, msg_, sock_));
    return 0;
}

//! 处理连接断开
int ffgate_t::handle_broken_impl(socket_ptr_t sock_)
{
    session_data_t* session_data = sock_->get_data<session_data_t>();
    if (NULL == session_data)
    {
        LOGDEBUG((FFGATE, "ffgate_t::broken ignore"));
        sock_->safe_delete();
        return 0;
    }
    
    LOGTRACE((FFGATE, "ffgate_t::broken begin"));
    if (false == session_data->is_valid())
    {
        //! 还未通过验证
        m_wait_verify_set.erase(session_data->socket_id);
    }
    else
    {
        client_info_t& client_info = m_client_set[session_data->id()];
        if (client_info.sock == sock_)
        {
            session_offline_t::in_t msg;
            msg.session_id  = session_data->id();
            m_ffrpc->call(client_info.alloc_logic_service, msg);
            m_client_set.erase(session_data->id());
        }
    }
    LOGTRACE((FFGATE, "ffgate_t::broken session_id[%ld]", session_data->id()));
    delete session_data;
    sock_->set_data(NULL);
    sock_->safe_delete();
    return 0;
}
//! 处理消息
int ffgate_t::handle_msg_impl(const message_t& msg_, socket_ptr_t sock_)
{
    session_data_t* session_data = sock_->get_data<session_data_t>();
    if (NULL == session_data)//! 还未验证sessionid
    {
        return verify_session_id(msg_, sock_);
    }
    else if (false == session_data->is_valid())
    {
        //! sessionid再未验证之前，client是不能发送消息的
        route_logic_msg_t::in_t msg;
        msg.session_id = session_data->id();
        msg.cmd        = msg_.get_cmd();
        msg.body       = msg_.get_body();
        m_wait_verify_set[session_data->socket_id].request_queue.push(msg);
        return 0;
    }
    else
    {
        return route_logic_msg(msg_, sock_);
    }
    return 0;
}

//! 验证sessionid
int ffgate_t::verify_session_id(const message_t& msg_, socket_ptr_t sock_)
{
    string ip = socket_op_t::getpeername(sock_->socket());
    LOGTRACE((FFGATE, "ffgate_t::verify_session_id session_key len=%u, ip[%s]", msg_.get_body().size(), ip));
    if (ip.empty())
    {
        sock_->close();
        return -1;
    }
    session_data_t* session_data = new session_data_t(this->alloc_id());
    sock_->set_data(session_data);
    m_wait_verify_set[session_data->socket_id].sock = sock_;
    
    session_first_entere_t::in_t msg;
    msg.cmd         = msg_.get_cmd();
    msg.socket_id   = session_data->socket_id;
    msg.msg_body    = msg_.get_body();
    msg.gate_name   = m_gate_name;
    msg.ip          = ip;
    m_ffrpc->call(DEFAULT_LOGIC_SERVICE, msg, ffrpc_ops_t::gen_callback(&ffgate_t::verify_session_callback, this, session_data->socket_id));
    LOGTRACE((FFGATE, "ffgate_t::verify_session_id end ok"));
    return 0;
}
//! 验证sessionid 的回调函数
int ffgate_t::verify_session_callback(ffreq_t<session_first_entere_t::out_t>& req_, userid_t sock_id_)
{
    LOGTRACE((FFGATE, "ffgate_t::verify_session_callback session_id=%d", sock_id_));
    ffreq_t<route_logic_msg_t::out_t> ret_msg;
    map<userid_t/*sessionid*/, client_info_t>::iterator it = m_wait_verify_set.find(sock_id_);
    if (it == m_wait_verify_set.end())
    {
        session_offline_t::in_t msg;
        msg.session_id  = req_.msg.uid;
        m_ffrpc->call(DEFAULT_LOGIC_SERVICE, msg);
        return 0;
    }
    socket_ptr_t sock = it->second.sock;
    
    session_data_t* session_data = sock->get_data<session_data_t>();
    
    session_data->set_id(req_.msg.uid);
    
    client_info_t& client_info = m_client_set[session_data->id()];
    client_info.sock           = sock;
    client_info.request_queue  = it->second.request_queue;
    
    while(false == it->second.request_queue.empty())
    {
        client_info.request_queue.push(it->second.request_queue.front());
        it->second.request_queue.pop();
    }
    m_wait_verify_set.erase(it);
    LOGTRACE((FFGATE, "ffgate_t::verify_session_callback end ok uid=%d", session_data->id()));
    return 0;
}

//! enter scene 回调函数
int ffgate_t::enter_scene_callback(ffreq_t<session_enter_scene_t::out_t>& req_, const userid_t& session_id_)
{
    LOGTRACE((FFGATE, "ffgate_t::enter_scene_callback session_id[%ld]", session_id_));
    LOGTRACE((FFGATE, "ffgate_t::enter_scene_callback end ok"));
    return 0;
}

//! 逻辑处理,转发消息到logic service
int ffgate_t::route_logic_msg(const message_t& msg_, socket_ptr_t sock_)
{
    session_data_t* session_data = sock_->get_data<session_data_t>();
    LOGTRACE((FFGATE, "ffgate_t::route_logic_msg session_id[%ld]", session_data->id()));
    
    client_info_t& client_info   = m_client_set[session_data->id()];
    if (client_info.request_queue.size() == MAX_MSG_QUEUE_SIZE)
    {
        //!  消息队列超限，关闭sock
        sock_->close();
        return 0;
    }
    
    route_logic_msg_t::in_t msg;
    msg.session_id = session_data->id();
    msg.cmd        = msg_.get_cmd();
    msg.body       = msg_.get_body();
    if (client_info.request_queue.empty())
    {
        m_ffrpc->call(client_info.group_name, client_info.alloc_logic_service, msg,
                      ffrpc_ops_t::gen_callback(&ffgate_t::route_logic_msg_callback, this, session_data->id()));
    }
    else
    {
        client_info.request_queue.push(msg);
    }
    LOGTRACE((FFGATE, "ffgate_t::route_logic_msg end ok alloc_logic_service[%s]", client_info.alloc_logic_service));
    return 0;
}

//! 逻辑处理,转发消息到logic service
int ffgate_t::route_logic_msg_callback(ffreq_t<route_logic_msg_t::out_t>& req_, const userid_t& session_id_)
{
    LOGTRACE((FFGATE, "ffgate_t::route_logic_msg_callback session_id[%ld]", session_id_));
    map<userid_t/*sessionid*/, client_info_t>::iterator it = m_client_set.find(session_id_);
    if (it == m_client_set.end())
    {
        return 0;
    }
    client_info_t& client_info = it->second;
    if (client_info.request_queue.empty())
    {
        return 0;
    }
    
    m_ffrpc->call(client_info.group_name, client_info.alloc_logic_service, client_info.request_queue.front(),
                  ffrpc_ops_t::gen_callback(&ffgate_t::route_logic_msg_callback, this, session_id_));
    
    client_info.request_queue.pop();
    LOGTRACE((FFGATE, "ffgate_t::route_logic_msg_callback end ok queue_size[%d],alloc_logic_service[%s]",
                client_info.request_queue.size(), client_info.alloc_logic_service));
    return 0;
}

//! 改变处理client 逻辑的对应的节点
int ffgate_t::change_session_logic(ffreq_t<gate_change_logic_node_t::in_t, gate_change_logic_node_t::out_t>& req_)
{
    LOGTRACE((FFGATE, "ffgate_t::change_session_logic session_id[%ld]", req_.msg.session_id));
    map<userid_t/*sessionid*/, client_info_t>::iterator it = m_client_set.find(req_.msg.session_id);
    if (it == m_client_set.end())
    {
        return 0;
    }
    
    session_enter_scene_t::in_t enter_msg;
    it->second.group_name = req_.msg.dest_group_name;
    enter_msg.from_group = req_.msg.cur_group_name;
    enter_msg.from_scene = it->second.alloc_logic_service;
    
    it->second.alloc_logic_service = req_.msg.alloc_logic_service;
    gate_change_logic_node_t::out_t out;
    req_.response(out);
    
    enter_msg.session_id = req_.msg.session_id;
    enter_msg.from_gate = m_gate_name;
    
    enter_msg.to_scene = req_.msg.alloc_logic_service;
    enter_msg.extra_data = req_.msg.extra_data;
    m_ffrpc->call(it->second.group_name, req_.msg.alloc_logic_service, enter_msg,
                  ffrpc_ops_t::gen_callback(&ffgate_t::enter_scene_callback, this, req_.msg.session_id));
    
    LOGTRACE((FFGATE, "ffgate_t::change_session_logic end ok"));
    return 0;
}

//! 关闭某个session socket
int ffgate_t::close_session(ffreq_t<gate_close_session_t::in_t, gate_close_session_t::out_t>& req_)
{
    LOGTRACE((FFGATE, "ffgate_t::close_session session_id[%ld]", req_.msg.session_id));
    
    map<userid_t/*sessionid*/, client_info_t>::iterator it = m_client_set.find(req_.msg.session_id);
    if (it == m_client_set.end())
    {
        return 0;
    }
    it->second.sock->close();
    gate_close_session_t::out_t out;
    req_.response(out);
    LOGTRACE((FFGATE, "ffgate_t::gate_close_session_t end ok"));
    return 0;
}

//! 转发消息给client
int ffgate_t::route_msg_to_session(ffreq_t<gate_route_msg_to_session_t::in_t, gate_route_msg_to_session_t::out_t>& req_)
{
    LOGTRACE((FFGATE, "ffgate_t::route_msg_to_session begin num[%d]", req_.msg.session_id.size()));
    
    for (size_t i = 0; i < req_.msg.session_id.size(); ++i)
    {
        userid_t& session_id = req_.msg.session_id[i];
        LOGTRACE((FFGATE, "ffgate_t::route_msg_to_session session_id[%ld]", session_id));

        map<userid_t/*sessionid*/, client_info_t>::iterator it = m_client_set.find(session_id);
        if (it == m_client_set.end())
        {
            continue;
        }

        msg_sender_t::send(it->second.sock, req_.msg.cmd, req_.msg.body);
    }
    gate_route_msg_to_session_t::out_t out;
    req_.response(out);
    LOGTRACE((FFGATE, "ffgate_t::route_msg_to_session end ok"));
    return 0;
}

//! 广播消息给所有的client
int ffgate_t::broadcast_msg_to_session(ffreq_t<gate_broadcast_msg_to_session_t::in_t, gate_broadcast_msg_to_session_t::out_t>& req_)
{
    LOGTRACE((FFGATE, "ffgate_t::broadcast_msg_to_session begin"));
    
    map<userid_t/*sessionid*/, client_info_t>::iterator it = m_client_set.begin();
    for (; it != m_client_set.end(); ++it)
    {
        msg_sender_t::send(it->second.sock, req_.msg.cmd, req_.msg.body);
    }
    
    gate_broadcast_msg_to_session_t::out_t out;
    req_.response(out);
    LOGTRACE((FFGATE, "ffgate_t::broadcast_msg_to_session end ok"));
    return 0;
}
