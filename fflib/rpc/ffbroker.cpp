#include "rpc/ffbroker.h"
#include "base/log.h"
#include "base/arg_helper.h"
#include "rpc/ffrpc.h"

using namespace ff;

static void route_call_reconnect(ffbroker_t* ffbroker_);

ffbroker_t::ffbroker_t():
    m_acceptor(NULL),
    m_for_alloc_id(0),
    m_node_id(0),
    m_master_broker_sock(NULL),
    m_timer(&m_tq)
{
}
ffbroker_t::~ffbroker_t()
{

}
//!ff 获取节点信息
uint64_t ffbroker_t::alloc_node_id(socket_ptr_t sock_)
{
    return ++m_for_alloc_id;
}

int ffbroker_t::open(arg_helper_t& arg)
{
    net_factory_t::start(1);
    m_listen_host = arg.get_option_value("-broker");
    m_acceptor = net_factory_t::listen(m_listen_host, this);
    if (NULL == m_acceptor)
    {
        LOGERROR((BROKER, "ffbroker_t::open failed<%s>", m_listen_host));
        return -1;
    }
    //! 绑定cmd 对应的回调函数
    //!新版本
    //! 处理其他broker或者client注册到此server
    m_ffslot.bind(REGISTER_TO_BROKER_REQ, ffrpc_ops_t::gen_callback(&ffbroker_t::handle_regiter_to_broker, this))
            .bind(BROKER_ROUTE_MSG, ffrpc_ops_t::gen_callback(&ffbroker_t::handle_broker_route_msg, this))
            .bind(REGISTER_TO_BROKER_RET, ffrpc_ops_t::gen_callback(&ffbroker_t::handle_register_master_ret, this))
            .bind(SYNC_CLIENT_REQ, ffrpc_ops_t::gen_callback(&ffbroker_t::process_sync_client_req, this));//! 处理同步客户端的调用请求

    //! 任务队列绑定线程
    m_thread.create_thread(task_binder_t::gen(&task_queue_t::run, &m_tq), 1);

    //!如果需要连接bride broker 读取配置连接
    if (arg.is_enable_option("-bridge_broker"))
    {
        string tmp_config =  arg.get_option_value("-bridge_broker");
        vector<string> vt;
        strtool::split(tmp_config, vt, ",");
        for (size_t i = 0; i < vt.size(); ++i)
        {
            m_bridge_broker_config[vt[i]] = NULL;
            LOGWARN((BROKER, "ffbroker_t::open bridge_broker<%s>", vt[i]));
        }
        if (connect_to_bridge_broker())
        {
            return -1;
        }
    }
    else if (arg.is_enable_option("-master_broker"))
    {
        m_broker_host = arg.get_option_value("-master_broker");
        if (connect_to_master_broker())
        {
            return -1;
        }
        for (int i = 0; i < 300; ++i)
        {
            if (m_node_id != 0)
            {
                break;
            }            
            usleep(10);
        }
        if (m_node_id == 0)
            return -1;
        //! 注册到master broker
    }
    else//! 内存中注册此broker
    {
        singleton_t<ffrpc_memory_route_t>::instance().add_node(BROKER_MASTER_NODE_ID, this);
    }
    LOGINFO((BROKER, "ffbroker_t::open end ok m_node_id=%d", m_node_id));
    return 0;
}

//! 获取任务队列对象
task_queue_t& ffbroker_t::get_tq()
{
    return m_tq;
}
//! 定时器
timer_service_t& ffbroker_t::get_timer()
{
    return m_timer;
}
void ffbroker_t::real_cleaanup()
{
    map<uint64_t/* node id*/, socket_ptr_t>::iterator it = m_all_registered_info.node_sockets.begin();
    for (; it != m_all_registered_info.node_sockets.end(); ++it)
    {
        it->second->close();
    }
    m_all_registered_info.node_sockets.clear();
}
int ffbroker_t::close()
{
    if (!m_acceptor)
        return 0;
    m_acceptor->close();
    m_acceptor = NULL;
    m_tq.produce(task_binder_t::gen(&ffbroker_t::real_cleaanup, this));
    m_tq.close();
    m_thread.join();
    //usleep(100);
    return 0;
}
task_queue_i* ffbroker_t::get_tq_ptr()
{
    return &m_tq;
}
/*
int ffbroker_t::handle_broken(socket_ptr_t sock_)
{
    m_tq.produce(task_binder_t::gen(&ffbroker_t::handle_broken_impl, this, sock_));
    return 0;
}
int ffbroker_t::handle_msg(const message_t& msg_, socket_ptr_t sock_)
{
    m_tq.produce(task_binder_t::gen(&ffbroker_t::handle_msg_impl, this, msg_, sock_));
    return 0;
}*/
//! 当有连接断开，则被回调
int ffbroker_t::handle_broken(socket_ptr_t sock_)
{
    LOGTRACE((BROKER, "ffbroker_t::handle_broken_impl begin"));
    session_data_t* psession = sock_->get_data<session_data_t>();
    if (NULL == psession)
    {
        sock_->safe_delete();
        LOGTRACE((BROKER, "ffbroker_t::handle_broken_impl no session"));
        return 0;
    }

    if (sock_ == m_master_broker_sock)
    {
        m_master_broker_sock = NULL;
        //!检测是否需要重连master broker
        m_timer.once_timer(RECONNECT_TO_BROKER_TIMEOUT, task_binder_t::gen(&route_call_reconnect, this));
        sock_->safe_delete();
        LOGTRACE((BROKER, "ffbroker_t::handle_broken_impl check master reconnect"));
        return 0;
    }
    else if (BRIDGE_BROKER == psession->get_type())
    {
        //! 如果需要连接bridge broker，记录各个bridge broker的配置
        for (map<string, socket_ptr_t>::iterator it = m_bridge_broker_config.begin(); it != m_bridge_broker_config.end(); ++it)
        {
            if (it->second == sock_)
            {
                it->second = NULL;
            }
        }
        //! 记录所有的namespace 注册在那些bridge broker 上
        for (map<string, set<socket_ptr_t> >::iterator it2 = m_namespace2bridge.begin(); it2 != m_namespace2bridge.end(); ++it2)
        {
            it2->second.erase(sock_);
        }
        //!检测是否需要重连master broker
        m_timer.once_timer(RECONNECT_TO_BROKER_TIMEOUT, task_binder_t::gen(&route_call_reconnect, this));
    }
    else
    {
        m_all_registered_info.node_sockets.erase(psession->node_id);
        LOGTRACE((BROKER, "ffbroker_t::handle_broken_impl node_id<%u> close", psession->node_id ));
        m_all_registered_info.broker_data.service2node_id.erase(psession->service_name);
    }
    delete psession;
    sock_->set_data(NULL);
    sock_->safe_delete();
    //!广播给所有的子节点
    register_to_broker_t::out_t ret_msg = m_all_registered_info.broker_data;
    sync_node_info(ret_msg);
    LOGTRACE((BROKER, "ffbroker_t::handle_broken_impl end ok"));
    return 0;

}
//! 当有消息到来，被回调
int ffbroker_t::handle_msg(const message_t& msg_, socket_ptr_t sock_)
{
    uint16_t cmd = msg_.get_cmd();
    LOGTRACE((BROKER, "ffbroker_t::handle_msg_impl cmd<%u> begin", cmd));

    ffslot_t::callback_t* cb = m_ffslot.get_callback(cmd);
    if (cb)
    {
        try
        {
            ffslot_msg_arg arg(msg_.get_body(), sock_);
            cb->exe(&arg);
            LOGTRACE((BROKER, "ffbroker_t::handle_msg_impl cmd<%u> end ok", cmd));
            return 0;
        }
        catch(exception& e_)
        {
            LOGERROR((BROKER, "ffbroker_t::handle_msg_impl exception<%s>", e_.what()));
            return -1;
        }
    }
    else
    {
        LOGERROR((BROKER, "ffbroker_t::handle_msg_impl cmd<%u> not supported", cmd));
        return -1;
    }
    return -1;
}


//! 处理其他broker或者client注册到此server
int ffbroker_t::handle_regiter_to_broker(register_to_broker_t::in_t& msg_, socket_ptr_t sock_)
{
    LOGINFO((BROKER, "ffbroker_t::handle_regiter_to_broker begin service_name=%s", msg_.service_name));

    session_data_t* psession = sock_->get_data<session_data_t>();
    if (NULL != psession)
    {
        sock_->close();
        return 0;
    }

    register_to_broker_t::out_t ret_msg;
    ret_msg.register_flag = -1;//! -1表示注册失败，0表示同步消息，1表示注册成功,

    if (is_master_broker() == false)//!如果自己是slave broker，不做任何的处理
    {
        m_all_registered_info.node_sockets[msg_.node_id] = sock_;
        LOGINFO((BROKER, "ffbroker_t::handle_regiter_to_broker register slave broker service_name=%s, node_id=%d", msg_.service_name, msg_.node_id));
        return 0;
    }
    
    //! 那么自己是master broker，处理来自slave 和 rpc node的注册消息
    if (SLAVE_BROKER == msg_.node_type)//! 分配slave broker 节点id，同步slave的监听信息到其他rpc node
    {
        uint64_t node_id = alloc_node_id(sock_);
        session_data_t* psession = new session_data_t(msg_.node_type, node_id);
        psession->node_id = node_id;
        sock_->set_data(psession);

        m_all_registered_info.node_sockets[node_id] = sock_;
        //!如果是slave broker，那么host参数会被赋值
        m_all_registered_info.broker_data.slave_broker_data[msg_.host] = node_id;
        LOGTRACE((BROKER, "ffbroker_t::handle_regiter_to_broker slave register=%s", msg_.host));

        ret_msg = m_all_registered_info.broker_data;
        ret_msg.node_id = node_id;
    }
    else if (RPC_NODE == msg_.node_type)
    {
        if (m_all_registered_info.broker_data.service2node_id.find(msg_.service_name) != m_all_registered_info.broker_data.service2node_id.end())
        {
            msg_sender_t::send(sock_, REGISTER_TO_BROKER_RET, ffthrift_t::EncodeAsString(ret_msg));
            LOGERROR((BROKER, "ffbroker_t::handle_regiter_to_broker service<%s> exist", msg_.service_name));
            return 0;
        }

        uint64_t node_id = alloc_node_id(sock_);
        session_data_t* psession = new session_data_t(msg_.node_type, node_id);
        psession->node_id = node_id;
        sock_->set_data(psession);

        m_all_registered_info.node_sockets[node_id] = sock_;
        if  (0 != msg_.bind_broker_id)
        {
            LOGINFO((BROKER, "ffbroker_t::handle_regiter_to_broker service<%s> bind_broker_id=%u exist",
                                msg_.service_name, msg_.bind_broker_id));
            m_all_registered_info.broker_data.rpc_bind_broker_info[node_id] = msg_.bind_broker_id;
        }
        if (msg_.service_name.empty() == false)
        {
            psession->service_name = msg_.service_name;
            m_all_registered_info.broker_data.service2node_id[msg_.service_name] = node_id;
        }
        ret_msg = m_all_registered_info.broker_data;
        ret_msg.node_id = node_id;
    }
    else if (MASTER_BROKER == msg_.node_type)//!bridge broker接受master broker的注册
    {
        uint64_t node_id = alloc_node_id(sock_);
        session_data_t* psession = new session_data_t(msg_.node_type, node_id);
        psession->node_id = node_id;
        sock_->set_data(psession);

        m_all_registered_info.node_sockets[node_id]   = sock_;
        m_namespace2master_nodeid[msg_.reg_namespace] = node_id;
        m_all_registered_info.broker_data.reg_namespace_list.push_back(msg_.reg_namespace);
        ret_msg = m_all_registered_info.broker_data;
        LOGINFO((BROKER, "ffbroker_t::handle_regiter_to_broker reg_namespace=%s", msg_.reg_namespace));
    }
    else
    {
        msg_sender_t::send(sock_, REGISTER_TO_BROKER_RET, ffthrift_t::EncodeAsString(ret_msg));
        LOGERROR((BROKER, "ffbroker_t::handle_regiter_to_broker type invalid<%d>", msg_.node_type));
        return 0;
    }
    
    //!广播给所有的子节点
    sync_node_info(ret_msg, sock_);
    LOGTRACE((BROKER, "ffbroker_t::handle_regiter_to_broker end ok id=%d, type=%d", ret_msg.node_id, msg_.node_type));
    return 0;
}
//! 同步给所有的节点，当前的各个节点的信息
int ffbroker_t::sync_node_info(register_to_broker_t::out_t& ret_msg, socket_ptr_t sock_)
{
    //!广播给所有的子节点
    map<uint64_t/* node id*/, socket_ptr_t>::iterator it = m_all_registered_info.node_sockets.begin();
    for (; it != m_all_registered_info.node_sockets.end(); ++it)
    {
        session_data_t* psession = it->second->get_data<session_data_t>();
        if (RPC_NODE == psession->get_type())//!如果没有分配对应的slavebroker，那么分配一个
        {
            map<int64_t, int64_t>::iterator it_id = m_all_registered_info.broker_data.rpc_bind_broker_info.find(
                                                        psession->get_node_id());
            if (it_id == m_all_registered_info.broker_data.rpc_bind_broker_info.end() ||
                m_all_registered_info.node_sockets.find(it_id->first) == m_all_registered_info.node_sockets.end())
            {
                if (m_all_registered_info.broker_data.slave_broker_data.empty() == false)
                {
                    int index = ret_msg.node_id % m_all_registered_info.broker_data.slave_broker_data.size();
                    map<string/*host*/, int64_t>::iterator it_broker = m_all_registered_info.broker_data.slave_broker_data.begin();
                    for (int i =0;i < index; ++i)
                    {
                        ++it_broker;
                    }
                    m_all_registered_info.broker_data.rpc_bind_broker_info[psession->get_node_id()] = it_broker->second;
                    ret_msg.rpc_bind_broker_info = m_all_registered_info.broker_data.rpc_bind_broker_info;
                }
                else
                {
                    m_all_registered_info.broker_data.rpc_bind_broker_info[psession->get_node_id()] = BROKER_MASTER_NODE_ID;//!broker master
                    ret_msg.rpc_bind_broker_info = m_all_registered_info.broker_data.rpc_bind_broker_info;
                }
            }
        }

        if (sock_ == it->second)
        {
            ret_msg.register_flag  = 1;
        }
        else
        {
            ret_msg.register_flag = 0;
        }
        msg_sender_t::send(it->second, REGISTER_TO_BROKER_RET, ffthrift_t::EncodeAsString(ret_msg));
    }
    return 0;
}

//! 处理转发消息的操作
int ffbroker_t::handle_broker_route_msg(broker_route_msg_t::in_t& msg_, socket_ptr_t sock_)
{
    LOGTRACE((BROKER, "ffbroker_t::handle_broker_route_msg begin"));
    session_data_t* psession = sock_->get_data<session_data_t>();
    if (NULL == psession)
    {
        sock_->close();
        return 0;
    }
    if (RPC_NODE == psession->get_type())
    {
        //!如果找到对应的节点，那么发给对应的节点
        send_to_rpc_node(msg_);
    }
    else if (MASTER_BROKER == psession->get_type())//!需要转给其他的broker master
    {
        LOGTRACE((BROKER, "ffbroker_t::handle_broker_route_msg from MASTER_BROKER dest_namespace=%s", msg_.dest_namespace));
        map<string, uint64_t>::iterator it = m_namespace2master_nodeid.find(msg_.dest_namespace);
        if (it != m_namespace2master_nodeid.end())
        {
            map<uint64_t/* node id*/, socket_ptr_t>::iterator it2 = m_all_registered_info.node_sockets.find(it->second);
            if (it2 != m_all_registered_info.node_sockets.end())
            {
                msg_sender_t::send(it2->second, BROKER_ROUTE_MSG, ffthrift_t::EncodeAsString(msg_));
                return 0;
            }
        }
        msg_.dest_namespace.clear();
        msg_.dest_service_name.clear();
        msg_.dest_node_id = msg_.from_node_id;
        msg_.err_info = "dest_namespace named " + msg_.dest_namespace + " not exist in broker";
        msg_sender_t::send(sock_, BROKER_TO_CLIENT_MSG, ffthrift_t::EncodeAsString(msg_));
    }
    else if (BRIDGE_BROKER == psession->get_type())//!需要转给其rpc node
    {
        LOGTRACE((BROKER, "ffbroker_t::handle_broker_route_msg from BRIDGE_BROKER dest_namespace=%s", msg_.dest_namespace));
        if (msg_.dest_service_name.empty() == false)
        {
            map<string, int64_t>::iterator it = m_all_registered_info.broker_data.service2node_id.find(msg_.dest_service_name);
            if (it != m_all_registered_info.broker_data.service2node_id.end())
            {
                uint64_t dest_node_id = it->second;
                msg_.dest_node_id = dest_node_id;
                map<uint64_t/* node id*/, socket_ptr_t>::iterator it = m_all_registered_info.node_sockets.find(msg_.dest_node_id);
                if (it != m_all_registered_info.node_sockets.end())
                {
                    msg_sender_t::send(it->second, BROKER_TO_CLIENT_MSG, ffthrift_t::EncodeAsString(msg_));
                    LOGTRACE((BROKER, "ffbroker_t::handle_broker_route_msg from BRIDGE_BROKER dest_node_id=%s", msg_.dest_node_id));
                    return 0;
                }
            }
        }
        else
        {
            map<uint64_t/* node id*/, socket_ptr_t>::iterator it = m_all_registered_info.node_sockets.find(msg_.dest_node_id);
            if (it != m_all_registered_info.node_sockets.end())
            {
                msg_sender_t::send(it->second, BROKER_TO_CLIENT_MSG, ffthrift_t::EncodeAsString(msg_));
                LOGTRACE((BROKER, "ffbroker_t::handle_broker_route_msg from BRIDGE_BROKER callback dest_node_id=%s", msg_.dest_node_id));
                return 0;
            }
        }
        msg_.dest_namespace = msg_.from_namespace;
        msg_.dest_service_name.clear();
        msg_.dest_node_id = msg_.from_node_id;
        msg_.err_info = "dest_namespace named " + msg_.dest_namespace + "::" + msg_.dest_service_name + " not exist in broker";
        msg_sender_t::send(sock_, BROKER_ROUTE_MSG, ffthrift_t::EncodeAsString(msg_));
        LOGTRACE((BROKER, "ffbroker_t::handle_broker_route_msg from BRIDGE_BROKER dest_service_name=%s not exist", msg_.dest_service_name));
    }
    LOGTRACE((BROKER, "ffbroker_t::handle_broker_route_msg end ok msg body_size=%d", msg_.body.size()));
    return 0;
}

//! 处理同步客户端的调用请求
int ffbroker_t::process_sync_client_req(broker_route_msg_t::in_t& msg_, socket_ptr_t sock_)
{
    LOGTRACE((BROKER, "ffbroker_t::process_sync_client_req begin"));
    session_data_t* psession = sock_->get_data<session_data_t>();
    if (NULL == psession)
    {
        psession = new session_data_t(SYNC_CLIENT_NODE, alloc_node_id(sock_));
        sock_->set_data(psession);
        m_all_registered_info.node_sockets[psession->get_node_id()] = sock_;
    }
    msg_.from_node_id = psession->get_node_id();
    map<string, int64_t>::iterator it = m_all_registered_info.broker_data.service2node_id.find(msg_.dest_service_name);
    if (it == m_all_registered_info.broker_data.service2node_id.end())
    {
        LOGWARN((BROKER, "ffbroker_t::process_sync_client_req dest_service_name=%s none", msg_.dest_service_name));
        msg_.err_info = "dest_service_name named " + msg_.dest_service_name + " not exist in broker";
        msg_sender_t::send(sock_, BROKER_ROUTE_MSG, ffthrift_t::EncodeAsString(msg_));
        return 0;
    }

    msg_.dest_node_id = it->second;
    msg_.callback_id  = msg_.from_node_id;
    //!如果找到对应的节点，那么发给对应的节点
    send_to_rpc_node(msg_);
    LOGTRACE((BROKER, "ffbroker_t::process_sync_client_req end ok service=%s from_node_id=%u",
              msg_.dest_service_name, msg_.from_node_id));
    return 0;
}

int ffbroker_t::send_to_rpc_node(broker_route_msg_t::in_t& msg_)
{
    //!如果找到对应的节点，那么发给对应的节点
    //!如果在内存中,那么内存间直接投递
    //!如果赋值了 dest_namespace 不为空,不等于本身的namespace 要通过bridge转发
    if (msg_.dest_namespace.empty() == false)
    {
        msg_.from_namespace = m_namespace;
        LOGTRACE((BROKER, "ffbroker_t::send_to_rpc_node post to bridge dest_namespace=%s bodylen=%d",
                            msg_.dest_namespace, msg_.body.size()));
        set<socket_ptr_t>& bridge_socket = m_namespace2bridge[msg_.dest_namespace];
        if (bridge_socket.empty())
        {
            map<uint64_t/* node id*/, socket_ptr_t>::iterator it = m_all_registered_info.node_sockets.find(msg_.from_node_id);
            if (it != m_all_registered_info.node_sockets.end())
            {
                msg_.err_info = "dest_namespace named " + msg_.dest_namespace + " not exist in broker";
                msg_.dest_namespace.clear();
                msg_.dest_service_name.clear();
                msg_sender_t::send(it->second, BROKER_TO_CLIENT_MSG, ffthrift_t::EncodeAsString(msg_));
            }
        }
        else
        {
            msg_sender_t::send(*(bridge_socket.begin()), BROKER_ROUTE_MSG, ffthrift_t::EncodeAsString(msg_));
        }
        return 0;
    }
    
    ffrpc_t* pffrpc = singleton_t<ffrpc_memory_route_t>::instance().get_rpc(msg_.dest_node_id);
    if (pffrpc)
    {
        LOGTRACE((BROKER, "ffbroker_t::send_to_rpc_node memory post"));
        pffrpc->get_tq().produce(task_binder_t::gen(&ffrpc_t::handle_rpc_call_msg, pffrpc, msg_, socket_ptr_t(NULL)));
        return 0;
    }
    LOGINFO((BROKER, "ffbroker_t::send_to_rpc_node dest_node=%d bodylen=%d, by socket", msg_.dest_node_id, msg_.body.size()));
    map<uint64_t/* node id*/, socket_ptr_t>::iterator it = m_all_registered_info.node_sockets.find(msg_.dest_node_id);
    if (it != m_all_registered_info.node_sockets.end())
    {
        msg_sender_t::send(it->second, BROKER_TO_CLIENT_MSG, ffthrift_t::EncodeAsString(msg_));
    }
    else
    {
        it = m_all_registered_info.node_sockets.find(msg_.from_node_id);
        if (it != m_all_registered_info.node_sockets.end())
        {
            msg_.err_info = "service named " + msg_.dest_service_name + " not exist in broker";
            msg_.dest_service_name.clear();
            msg_sender_t::send(it->second, BROKER_TO_CLIENT_MSG, ffthrift_t::EncodeAsString(msg_));
        }
        LOGERROR((BROKER, "ffbroker_t::handle_broker_route_msg end failed node=%d none exist", msg_.dest_node_id));
        return 0;
    }
    return 0;
}

//! 连接到broker master
int ffbroker_t::connect_to_master_broker()
{
    if (m_master_broker_sock || m_broker_host.empty() == true)
    {
        return 0;
    }
    LOGINFO((BROKER, "ffbroker_t::connect_to_master_broker begin<%s>", m_broker_host.c_str()));
    m_master_broker_sock = net_factory_t::connect(m_broker_host, this);
    if (NULL == m_master_broker_sock)
    {
        LOGERROR((BROKER, "ffbroker_t::register_to_broker_master failed, can't connect to master broker<%s>", m_broker_host.c_str()));
        return -1;
    }
    session_data_t* psession = new session_data_t(MASTER_BROKER, BROKER_MASTER_NODE_ID);
    m_master_broker_sock->set_data(psession);

    //! 发送注册消息给master broker
    //!新版发送注册消息
    register_to_broker_t::in_t reg_msg;
    reg_msg.node_type = SLAVE_BROKER;
    reg_msg.host      = m_listen_host;
    reg_msg.node_id   = 0;
    msg_sender_t::send(m_master_broker_sock, REGISTER_TO_BROKER_REQ, ffthrift_t::EncodeAsString(reg_msg));

    LOGINFO((BROKER, "ffbroker_t::connect_to_master_broker ok<%s>", m_broker_host.c_str()));
    return 0;
}

//! 连接到bridge broker
int ffbroker_t::connect_to_bridge_broker()
{
    LOGINFO((BROKER, "ffbroker_t::connect_to_bridge_broker begin"));
    int ret = 0;
    map<string, socket_ptr_t>::iterator it = m_bridge_broker_config.begin();
    for (; it != m_bridge_broker_config.end(); ++it)
    {
        if (it->second)
            continue;
        const string& tmp_host = it->first;
        LOGINFO((BROKER, "ffbroker_t::connect_to_bridge_broker try connect <%s>", tmp_host));
        
        vector<string> vt;
        strtool::split(tmp_host, vt, "@");
        if (vt.size() != 2)
        {
            ret = -1;
            continue;
        }
        it->second = net_factory_t::connect(vt[1], this);
            
        if (NULL == it->second)
        {
            LOGERROR((BROKER, "ffbroker_t::connect_to_bridge_broker can't connect to <%s>", vt[1]));
            ret = -1;
            continue;
        }

        session_data_t* psession = new session_data_t(BRIDGE_BROKER);
        it->second->set_data(psession);

        //! 发送注册消息给master broker
        //!新版发送注册消息
        register_to_broker_t::in_t reg_msg;
        reg_msg.node_type = MASTER_BROKER;
        reg_msg.node_id   = 0;
        reg_msg.reg_namespace = vt[0];
        if (m_namespace.empty())
        {
            m_namespace = reg_msg.reg_namespace;
        }
        msg_sender_t::send(it->second, REGISTER_TO_BROKER_REQ, ffthrift_t::EncodeAsString(reg_msg));
    }
    LOGINFO((BROKER, "ffbroker_t::connect_to_bridge_broker end ok"));
    return ret;
}

//!处理注册到master broker的消息
int ffbroker_t::handle_register_master_ret(register_to_broker_t::out_t& msg_, socket_ptr_t sock_)
{
    LOGTRACE((BROKER, "ffbroker_t::handle_register_master_ret begin"));
    session_data_t* psession = sock_->get_data<session_data_t>();
    if (NULL == psession)
    {
        sock_->close();
        return 0;
    }

    if (psession->get_type() == MASTER_BROKER)//!注册到master broker的返回消息
    {
        if (msg_.register_flag < 0)
        {
            LOGERROR((BROKER, "ffbroker_t::handle_register_master_ret register failed, service exist"));
            return -1;
        }
        if (msg_.register_flag == 1)
        {
            m_node_id = msg_.node_id;//! -1表示注册失败，0表示同步消息，1表示注册成功
            singleton_t<ffrpc_memory_route_t>::instance().add_node(m_node_id, this);
        }
        m_all_registered_info.broker_data = msg_;
    }
    else if (psession->get_type() == BRIDGE_BROKER)//!注册到bridge broker的返回消息
    {
        LOGINFO((BROKER, "ffbroker_t::handle_register_master_ret BRIDGE_BROKER size=%u", msg_.reg_namespace_list.size()));
        for (size_t i = 0; i < msg_.reg_namespace_list.size(); ++i)
        {
            m_namespace2bridge[msg_.reg_namespace_list[i]].insert(sock_);
            LOGINFO((BROKER, "ffbroker_t::handle_register_master_ret BRIDGE_BROKER reg_namespace=%s", msg_.reg_namespace_list[i]));
        }
    }
    LOGINFO((BROKER, "ffbroker_t::handle_register_master_ret end ok m_node_id=%d", m_node_id));
    return 0;
}

static void reconnect_loop(ffbroker_t* ffbroker_)
{
    if (ffbroker_->connect_to_master_broker())
    {
        ffbroker_->get_timer().once_timer(RECONNECT_TO_BROKER_TIMEOUT, task_binder_t::gen(&route_call_reconnect, ffbroker_));
    }
    if (ffbroker_->connect_to_bridge_broker())
    {
        ffbroker_->get_timer().once_timer(RECONNECT_TO_BROKER_TIMEOUT, task_binder_t::gen(&route_call_reconnect, ffbroker_));
    }
}
//! 投递到ffrpc 特定的线程
static void route_call_reconnect(ffbroker_t* ffbroker_)
{
    ffbroker_->get_tq().produce(task_binder_t::gen(&reconnect_loop, ffbroker_));
}
