#ifndef _FF_FFSCENE_H_
#define _FF_FFSCENE_H_

//#include <assert.h>
#include <string>
using namespace std;

#include "base/ffslot.h"
#include "net/socket_i.h"
#include "base/fftype.h"
#include "net/codec.h"
#include "rpc/ffrpc.h"
#include "base/arg_helper.h"

namespace ff
{

class ffscene_t
{
public:
    struct callback_info_t
    {
        callback_info_t():
            verify_callback(NULL),
            enter_callback(NULL),
            offline_callback(NULL),
            logic_callback(NULL),
            scene_call_callback(NULL)
        {}
        ffslot_t::callback_t*   verify_callback;
        ffslot_t::callback_t*   enter_callback;
        ffslot_t::callback_t*   offline_callback;
        ffslot_t::callback_t*   logic_callback;
        ffslot_t::callback_t*   scene_call_callback;
    };
    
    class session_verify_arg;
    class session_enter_arg;
    class session_offline_arg;
    class logic_msg_arg;
    class scene_call_msg_arg;
    
    //! 记录session的信息
    struct session_info_t
    {
        string gate_name;//! 所在的gate
    };
public:
    ffscene_t();
    virtual ~ffscene_t();
    virtual int open(arg_helper_t& arg, string scene_name = "");
    virtual int close();

    callback_info_t& callback_info();
    
    //! 发送消息给特定的client
    int send_msg_session(const string& gate_name, const userid_t& session_id_, uint16_t cmd_, const string& data_);
    int kf_send_msg_session(const string& group_name, const string& gate_name, const userid_t& session_id_,
                            uint16_t cmd_, const string& data_);
    //! 多播
    //TODO int multicast_msg_session(const vector<userid_t>& session_id_, uint16_t cmd_, const string& data_);
    //! 广播
    //TODO broadcast_msg_session(uint16_t cmd_, const string& data_);
    //! 广播 整个gate
    int broadcast_msg_gate(const string& gate_name_, uint16_t cmd_, const string& data_);
    //! 关闭某个session
    int close_session(const string& gate_name_, const userid_t& session_id_);
    //! 切换scene
    int change_session_scene(const string& gate_name_, const userid_t& session_id_, const string& to_scene_, const string& extra_data);
    //! 跨服
    int change_session_kf_scene(const string& group_name_, const string& gate_name_, const userid_t& session_id_,
                                const string& to_scene_, const string& extra_data);
    ffrpc_t& get_rpc() { return *m_ffrpc; }
    
    const string& get_scene_name() const { return m_logic_name;}
    const string& get_group_name() const { return m_group_name;}
private:
    //! 处理client 上线
    int process_session_verify(ffreq_t<session_first_entere_t::in_t, session_first_entere_t::out_t>& req_);
    //! 处理client 进入场景
    int process_session_enter(ffreq_t<session_enter_scene_t::in_t, session_enter_scene_t::out_t>& req_);
    //! 处理client 下线
    int process_session_offline(ffreq_t<session_offline_t::in_t, session_offline_t::out_t>& req_);
    //! 转发client消息
    int process_session_req(ffreq_t<route_logic_msg_t::in_t, route_logic_msg_t::out_t>& req_);
    //! scene 之间的互调用
    int process_scene_call(ffreq_t<scene_call_msg_t::in_t, scene_call_msg_t::out_t>& req_);
    userid_t alloc_uid() { return ++m_alloc_id; }
protected:
    userid_t                                    m_alloc_id;
    string                                      m_group_name;//! 区组名称
    string                                      m_logic_name;
    shared_ptr_t<ffrpc_t>                       m_ffrpc;
    callback_info_t                             m_callback_info;
    //map<userid_t/*sessionid*/, session_info_t>    m_session_info;
    typedef ffreq_t<session_first_entere_t::in_t, session_first_entere_t::out_t> ffreq_verify_t;
};


class ffscene_mgr_t
{
public:
    ffscene_t* get(const string& name_)
    {
        map<string, ffscene_t*>::iterator it = m_all_scene.find(name_);
        if (it != m_all_scene.end())
        {
            return it->second;
        }
        return NULL;
    }
    ffscene_t* get_any()
    {
        map<string, ffscene_t*>::iterator it = m_all_scene.begin();
        if (it != m_all_scene.end())
        {
            return it->second;
        }
        return NULL;
    }
    void       add(const string& name_, ffscene_t* p)
    {
        m_all_scene[name_] = p;
    }
    void       del(const string& name_)
    {
        m_all_scene.erase(name_);
    }
    
private:
    map<string, ffscene_t*>     m_all_scene;
};

class ffscene_t::session_verify_arg: public ffslot_t::callback_arg_t
{
public:
    session_verify_arg(const string& s_, uint16_t cmd_, userid_t t_, const string& ip_, const string& gate_):
        cmd(cmd_),
        msg_body(s_),
        socket_id(t_),
        ip(ip_),
        gate_name(gate_),
        alloc_session_id(0),
        flag_verify(false)
    {}
    virtual int type()
    {
        return TYPEID(session_verify_arg);
    }
    uint16_t        cmd;
    string          msg_body;
    userid_t        socket_id;
    string          ip;
    string          gate_name;

    //! 验证后的sessionid
    userid_t         alloc_session_id;
    //! 需要额外的返回给client的消息内容
    string          extra_data;
    bool            flag_verify;//!是否完成了账号验证
};

class ffscene_t::session_enter_arg: public ffslot_t::callback_arg_t
{
public:
    session_enter_arg(const string& group_, const string& gate_, const userid_t& s_,
                      const string& from_, const string& to_, const string& data_):
        group_name(group_),
        gate_name(gate_),
        session_id(s_),
        from_scene(from_),
        to_scene(to_),
        extra_data(data_)
    {}
    virtual int type()
    {
        return TYPEID(session_enter_arg);
    }
    string    group_name;
    string    gate_name;
    userid_t    session_id;//! 包含用户id
    string    from_scene;//! 从哪个scene跳转过来,若是第一次上线，from_scene为空
    string    to_scene;//! 跳到哪个scene上面去,若是下线，to_scene为空
    string    extra_data;//! 附带数据
};
class ffscene_t::session_offline_arg: public ffslot_t::callback_arg_t
{
public:
    session_offline_arg(const userid_t& s_):
        session_id(s_)
    {}
    virtual int type()
    {
        return TYPEID(session_offline_arg);
    }
    userid_t          session_id;
};
class ffscene_t::logic_msg_arg: public ffslot_t::callback_arg_t
{
public:
    logic_msg_arg(const userid_t& s_, uint16_t cmd_, const string& t_):
        session_id(s_),
        cmd(cmd_),
        body(t_)
    {}
    virtual int type()
    {
        return TYPEID(logic_msg_arg);
    }
    userid_t          session_id;
    uint16_t        cmd;
    string          body;
};

class ffscene_t::scene_call_msg_arg: public ffslot_t::callback_arg_t
{
public:
    scene_call_msg_arg(uint16_t cmd_, const string& t_, string& err_, string& msg_type_, string& ret_):
        cmd(cmd_),
        body(t_),
        err(err_),
        msg_type(msg_type_),
        ret(ret_)
    {}
    virtual int type()
    {
        return TYPEID(scene_call_msg_arg);
    }
    uint16_t        cmd;
    const string&   body;
    
    string&         err;
    string&         msg_type;
    string&         ret;
};

}


#endif
