#include "server/ffscene.h"
#include "base/log.h"
using namespace ff;

#define FFSCENE                   "FFSCENE"

ffscene_t::ffscene_t():m_alloc_id(0),m_ffrpc(NULL)
{
    
}
ffscene_t::~ffscene_t()
{
    
}
int ffscene_t::open(arg_helper_t& arg_helper, string scene_name)
{
    LOGTRACE((FFSCENE, "ffscene_t::open begin"));
    m_logic_name = scene_name;
    if (scene_name.empty())
    {
        if (false == arg_helper.is_enable_option("-scene"))
        {
            LOGERROR((FFSCENE, "ffscene_t::open failed without -scene argmuent"));
            return -1;
        }
        m_logic_name = arg_helper.get_option_value("-scene");
    }
    m_group_name = arg_helper.get_option_value("-group_name");
    m_ffrpc = new ffrpc_t(m_logic_name);
    
    m_ffrpc->reg(&ffscene_t::process_session_verify, this);
    m_ffrpc->reg(&ffscene_t::process_session_enter, this);
    m_ffrpc->reg(&ffscene_t::process_session_offline, this);
    m_ffrpc->reg(&ffscene_t::process_session_req, this);
    m_ffrpc->reg(&ffscene_t::process_scene_call, this);
    
    if (m_ffrpc->open(arg_helper))
    {
        LOGERROR((FFSCENE, "ffscene_t::open failed check -broker or -master_broker argmuent"));
        return -1;
    }
    
    singleton_t<ffscene_mgr_t>::instance().add(m_logic_name, this);
    LOGTRACE((FFSCENE, "ffscene_t::open end ok"));
    return 0;
}
int ffscene_t::close()
{
    if (m_ffrpc)
    {
        m_ffrpc->close();
        singleton_t<ffscene_mgr_t>::instance().del(m_logic_name);
    }
    return 0;
}
//! 处理client 上线
int ffscene_t::process_session_verify(ffreq_t<session_first_entere_t::in_t, session_first_entere_t::out_t>& req_)
{
    LOGTRACE((FFSCENE, "ffscene_t::process_session_verify begin msg_body size=%u", req_.msg.msg_body.size()));
    session_first_entere_t::out_t out;
    out.uid = this->alloc_uid()+100;
    req_.response(out);
    if (m_callback_info.verify_callback)
    {
        session_verify_arg arg(req_.msg.msg_body, req_.msg.cmd, out.uid, req_.msg.ip, req_.msg.gate_name);
        m_callback_info.verify_callback->exe(&arg);
        if (arg.flag_verify)
        {
            req_.response(out);
        }
    }

    LOGTRACE((FFSCENE, "ffscene_t::process_session_verify end ok socket_id=%d", req_.msg.socket_id));
    return 0;
}
//! 处理client 进入场景
int ffscene_t::process_session_enter(ffreq_t<session_enter_scene_t::in_t, session_enter_scene_t::out_t>& req_)
{
    LOGTRACE((FFSCENE, "ffscene_t::process_session_enter begin gate[%s]", req_.msg.from_gate));

    session_enter_scene_t::out_t out;
    req_.response(out);
    if (m_callback_info.enter_callback)
    {
        session_enter_arg arg(req_.msg.from_group, req_.msg.from_gate, req_.msg.session_id, req_.msg.from_scene, req_.msg.to_scene, req_.msg.extra_data);
        m_callback_info.enter_callback->exe(&arg);
    }
    LOGTRACE((FFSCENE, "ffscene_t::process_session_enter end ok"));

    return 0;
}

//! 处理client 下线
int ffscene_t::process_session_offline(ffreq_t<session_offline_t::in_t, session_offline_t::out_t>& req_)
{
    LOGTRACE((FFSCENE, "ffscene_t::process_session_offline begin"));
    //useless m_session_info.erase(req_.msg.session_id);
    session_offline_t::out_t out;
    if (m_callback_info.offline_callback)
    {
        session_offline_arg arg(req_.msg.session_id);
        m_callback_info.offline_callback->exe(&arg);
    }
    req_.response(out);
    LOGTRACE((FFSCENE, "ffscene_t::process_session_offline end ok"));
    return 0;
}
//! 转发client消息
int ffscene_t::process_session_req(ffreq_t<route_logic_msg_t::in_t, route_logic_msg_t::out_t>& req_)
{
    LOGTRACE((FFSCENE, "ffscene_t::process_session_req begin cmd[%u]", req_.msg.cmd));
    route_logic_msg_t::out_t out;
    if (m_callback_info.logic_callback)
    {
        logic_msg_arg arg(req_.msg.session_id, req_.msg.cmd, req_.msg.body);
        m_callback_info.logic_callback->exe(&arg);
    }
    req_.response(out);
    LOGTRACE((FFSCENE, "ffscene_t::process_session_req end ok"));
    return 0;
}

//! scene 之间的互调用
int ffscene_t::process_scene_call(ffreq_t<scene_call_msg_t::in_t, scene_call_msg_t::out_t>& req_)
{
    LOGTRACE((FFSCENE, "ffscene_t::process_scene_call begin cmd[%u]", req_.msg.cmd));
    scene_call_msg_t::out_t out;
    if (m_callback_info.scene_call_callback)
    {
        scene_call_msg_arg arg(req_.msg.cmd, req_.msg.body, out.err, out.msg_type, out.body);
        m_callback_info.scene_call_callback->exe(&arg);
    }
    else
    {
        out.err = "no scene_call_callback bind";
    }
    req_.response(out);

    LOGTRACE((FFSCENE, "ffscene_t::process_scene_call end ok"));
    return 0;
}

ffscene_t::callback_info_t& ffscene_t::callback_info()
{
    return m_callback_info;
}

//! 发送消息给特定的client
int ffscene_t::send_msg_session(const string& gate_name, const userid_t& session_id_, uint16_t cmd_, const string& data_)
{
    LOGTRACE((FFSCENE, "ffscene_t::send_msg_session begin session_id_<%ld>", session_id_));

    gate_route_msg_to_session_t::in_t msg;
    msg.session_id.push_back(session_id_);
    msg.cmd  = cmd_;
    msg.body = data_;
    m_ffrpc->call(gate_name, msg);
    LOGTRACE((FFSCENE, "ffscene_t::send_msg_session end ok gate[%s]", gate_name));
    return 0;
}
int ffscene_t::kf_send_msg_session(const string& group_name, const string& gate_name,
                                   const userid_t& session_id_,
                                   uint16_t cmd_, const string& data_)
{
    LOGTRACE((FFSCENE, "ffscene_t::send_msg_session begin session_id_<%ld>", session_id_));

    gate_route_msg_to_session_t::in_t msg;
    msg.session_id.push_back(session_id_);
    msg.cmd  = cmd_;
    msg.body = data_;
    m_ffrpc->call(group_name, gate_name, msg);
    LOGTRACE((FFSCENE, "ffscene_t::send_msg_session end ok gate[%s]", gate_name));
    return 0;
}
//! 广播 整个gate
int ffscene_t::broadcast_msg_gate(const string& gate_name_, uint16_t cmd_, const string& data_)
{
    gate_broadcast_msg_to_session_t::in_t msg;
    msg.cmd = cmd_;
    msg.body = data_;
    m_ffrpc->call(gate_name_, msg);
    return 0;
}
//! 关闭某个session
int ffscene_t::close_session(const string& gate_name_, const userid_t& session_id_)
{
    gate_close_session_t::in_t msg;
    msg.session_id = session_id_;
    m_ffrpc->call(gate_name_, msg);
    return 0;
}
//! 切换scene
int ffscene_t::change_session_scene(const string& gate_name_, const userid_t& session_id_, const string& to_scene_, const string& extra_data_)
{
	LOGTRACE((FFSCENE, "ffscene_t::change_session_scene session id[%ld]", session_id_));

    gate_change_logic_node_t::in_t msg;
    msg.session_id = session_id_;
    msg.alloc_logic_service = to_scene_;
    msg.extra_data = extra_data_;
    m_ffrpc->call(gate_name_, msg);
    return 0;
}
int ffscene_t::change_session_kf_scene(const string& group_name, const string& gate_name_, const userid_t& session_id_,
                                       const string& to_scene_, const string& extra_data_)
{
	LOGTRACE((FFSCENE, "ffscene_t::change_session_kf_scene session id[%ld]", session_id_));

    gate_change_logic_node_t::in_t msg;
    msg.cur_group_name  = this->get_group_name();
    msg.dest_group_name = group_name;
    msg.session_id = session_id_;
    msg.alloc_logic_service = to_scene_;
    msg.extra_data = extra_data_;
    m_ffrpc->call(gate_name_, msg);
    return 0;
}
