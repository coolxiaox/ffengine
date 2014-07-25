//! 消息发送管理
#ifndef _FF_FFRPC_H_
#define _FF_FFRPC_H_

#include <string>
#include <map>
#include <vector>
#include <set>
using namespace std;

#include "net/msg_handler_i.h"
#include "base/task_queue_impl.h"
#include "base/ffslot.h"
#include "net/codec.h"
#include "base/thread.h"
#include "rpc/ffrpc_ops.h"
#include "net/msg_sender.h"
#include "base/timer_service.h"
#include "base/arg_helper.h"

namespace ff {
class ffrpc_t: public msg_handler_i, ffresponser_t
{
    struct session_data_t;
    struct slave_broker_info_t;
    struct broker_client_info_t;
public:
    ffrpc_t(string service_name_ = "");
    virtual ~ffrpc_t();

    int open(arg_helper_t& arg_);
    int close();
    
    //! 处理连接断开
    int handle_broken(socket_ptr_t sock_);
    //! 处理消息
    int handle_msg(const message_t& msg_, socket_ptr_t sock_);

    //! 注册接口
    template <typename R, typename IN, typename OUT>
    ffrpc_t& reg(R (*)(ffreq_t<IN, OUT>&));
    template <typename R, typename CLASS_TYPE, typename IN, typename OUT>
    ffrpc_t& reg(R (CLASS_TYPE::*)(ffreq_t<IN, OUT>&), CLASS_TYPE* obj);

    //! 调用远程的接口
    template <typename T>
    int call(const string& name_, T& req_, ffslot_t::callback_t* callback_ = NULL);
    //! 调用其他broker master 组的远程的接口
    template <typename T>
    int call(const string& namespace_, const string& name_, T& req_, ffslot_t::callback_t* callback_ = NULL);
    
#ifdef FF_ENABLE_PROTOCOLBUF
    template <typename R, typename IN, typename OUT>
    ffrpc_t& reg(R (*)(ffreq_pb_t<IN, OUT>&));
    template <typename R, typename CLASS_TYPE, typename IN, typename OUT>
    ffrpc_t& reg(R (CLASS_TYPE::*)(ffreq_pb_t<IN, OUT>&), CLASS_TYPE* obj);
    //! 调用远程的接口
    template <typename T>
    int call_pb(const string& name_, T& req_, ffslot_t::callback_t* callback_ = NULL);
    //! 调用其他broker master 组的远程的接口
    template <typename T>
    int call_pb(const string& namespace_, const string& name_, T& req_, ffslot_t::callback_t* callback_ = NULL);
#endif
    
    uint32_t get_callback_id() { return ++m_callback_id; }
    //! call 接口的实现函数，call会将请求投递到该线程，保证所有请求有序
    int call_impl(const string& service_name_, const string& msg_name_, const string& body_, ffslot_t::callback_t* callback_);
    //! 调用接口后，需要回调消息给请求者
    virtual void response(const string& dest_namespace_, const string& msg_name_,  uint64_t dest_node_id_, int64_t callback_id_, const string& body_);
    //! 通过node id 发送消息给broker
    void send_to_dest_node(const string& dest_namespace_, const string& service_name_, const string& msg_name_,  uint64_t dest_node_id_, int64_t callback_id_, const string& body_);
    //! 获取任务队列对象
    task_queue_t& get_tq();
    //! 定时重连 broker master
    void timer_reconnect_broker();

    timer_service_t& get_timer() { return m_timer; }
    //! 通过bridge broker调用远程的service
    int bridge_call_impl(const string& namespace_, const string& service_name_, const string& msg_name_,
                         const string& body_, ffslot_t::callback_t* callback_);

    //! 判断某个service是否存在
    bool is_exist(const string& service_name_);
    
    //!获取broker socket
    socket_ptr_t get_broker_socket();
    //! 新版 调用消息对应的回调函数
    int handle_rpc_call_msg(broker_route_msg_t::in_t& msg_, socket_ptr_t sock_);
	//! 注册接口【支持动态的增加接口】
	void reg(const string& name_, ffslot_t::callback_t* func_);
private:
    //! 处理连接断开
    int handle_broken_impl(socket_ptr_t sock_);
    //! 处理消息
    int handle_msg_impl(const message_t& msg_, socket_ptr_t sock_);
    //! 连接到broker master
    socket_ptr_t connect_to_broker(const string& host_, uint32_t node_id_);
    
    //! 新版实现
    //! 处理注册, 
    int handle_broker_reg_response(register_to_broker_t::out_t& msg_, socket_ptr_t sock_);
    
public:
    bool                                    m_runing;
private:
    string                                  m_host;
    string                                  m_service_name;//! 注册的服务名称
    uint64_t                                m_node_id;     //! 通过注册broker，分配的node id
    
    uint32_t                                m_callback_id;//! 回调函数的唯一id值
    task_queue_t                            m_tq;
    timer_service_t                         m_timer;
    thread_t                                m_thread;
    ffslot_t                                m_ffslot;//! 注册broker 消息的回调函数
    ffslot_t                                m_ffslot_interface;//! 通过reg 注册的接口会暂时的存放在这里
    ffslot_t                                m_ffslot_callback;//! 
    socket_ptr_t                            m_master_broker_sock;

    map<uint32_t, slave_broker_info_t>      m_slave_broker_sockets;//! node id -> info
    map<string, uint32_t>                   m_msg2id;
    map<uint32_t, broker_client_info_t>     m_broker_client_info;//! node id -> service
    map<string, uint32_t>                   m_broker_client_name2nodeid;//! service name -> service node id
    
    //!ff绑定的broker id
    uint64_t                                m_bind_broker_id;
    //!所有的broker socket
    map<uint64_t, socket_ptr_t>             m_broker_sockets;
    //!ff new impl
    register_to_broker_t::out_t             m_broker_data;
};

//! 注册接口
template <typename R, typename IN, typename OUT>
ffrpc_t& ffrpc_t::reg(R (*func_)(ffreq_t<IN, OUT>&))
{
    m_ffslot_interface.bind(TYPE_NAME(IN), ffrpc_ops_t::gen_callback(func_));
    return *this;
}
template <typename R, typename CLASS_TYPE, typename IN, typename OUT>
ffrpc_t& ffrpc_t::reg(R (CLASS_TYPE::*func_)(ffreq_t<IN, OUT>&), CLASS_TYPE* obj)
{
    m_ffslot_interface.bind(TYPE_NAME(IN), ffrpc_ops_t::gen_callback(func_, obj));
    return *this;
}

struct ffrpc_t::session_data_t
{
    session_data_t(uint32_t n = 0):
        node_id(n)
    {}
    uint32_t get_node_id() { return node_id; }
    uint32_t node_id;
};
struct ffrpc_t::slave_broker_info_t
{
    slave_broker_info_t():
        sock(NULL)
    {}
    string          host;
    socket_ptr_t    sock;
};
struct ffrpc_t::broker_client_info_t
{
    broker_client_info_t():
        bind_broker_id(0),
        service_id(0)
    {}
    uint32_t bind_broker_id;
    string   service_name;
    uint16_t service_id;
};

//! 调用远程的接口
template <typename T>
int ffrpc_t::call(const string& name_, T& req_, ffslot_t::callback_t* callback_)
{
    m_tq.produce(task_binder_t::gen(&ffrpc_t::call_impl, this, name_, TYPE_NAME(T), ffthrift_t::EncodeAsString(req_), callback_));
    return 0;
}

//! 调用其他broker master 组的远程的接口
template <typename T>
int ffrpc_t::call(const string& namespace_, const string& name_, T& req_, ffslot_t::callback_t* callback_)
{
    if (namespace_.empty())
    {
        return this->call(name_, req_, callback_);
    }
    else{
        m_tq.produce(task_binder_t::gen(&ffrpc_t::bridge_call_impl, this, namespace_, name_, TYPE_NAME(T), ffthrift_t::EncodeAsString(req_), callback_));
    }
    return 0;
}

#ifdef FF_ENABLE_PROTOCOLBUF
template <typename R, typename IN, typename OUT>
ffrpc_t& ffrpc_t::reg(R (*func_)(ffreq_pb_t<IN, OUT>&))
{
    m_ffslot_interface.bind(TYPE_NAME(IN), ffrpc_ops_t::gen_callback(func_));
    return *this;
}
template <typename R, typename CLASS_TYPE, typename IN, typename OUT>
ffrpc_t& ffrpc_t::reg(R (CLASS_TYPE::*func_)(ffreq_pb_t<IN, OUT>&), CLASS_TYPE* obj)
{
    m_ffslot_interface.bind(TYPE_NAME(IN), ffrpc_ops_t::gen_callback(func_, obj));
    return *this;
}


//! 调用远程的接口
template <typename T>
int ffrpc_t::call_pb(const string& name_, T& req_, ffslot_t::callback_t* callback_)
{
    string ret;
    req_.SerializeToString(&ret);
    m_tq.produce(task_binder_t::gen(&ffrpc_t::call_impl, this, name_, TYPE_NAME(T), ret, callback_));
    return 0;
}

//! 调用其他broker master 组的远程的接口
template <typename T>
int ffrpc_t::call_pb(const string& namespace_, const string& name_, T& req_, ffslot_t::callback_t* callback_)
{
    if (namespace_.empty())
    {
        return this->call_pb(name_, req_, callback_);
    }
    else{
        string ret;
        req_.SerializeToString(&ret);
        m_tq.produce(task_binder_t::gen(&ffrpc_t::bridge_call_impl, this, namespace_, name_, TYPE_NAME(T), ret, callback_));
    }
    return 0;
}
#endif
}
#endif
