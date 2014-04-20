#ifndef _FF_FFLUA_MOD_H_
#define _FF_FFLUA_MOD_H_

//#include <assert.h>
//#include <string>
using namespace std;

#include "server/ffscene.h"
#include "base/log.h"
#include "base/smart_ptr.h"
#include "server/db_mgr.h"
#include "server/fftask_processor.h"

namespace ff
{
#define FFSCENE_LUA "FFSCENE_LUA"

#define MOD_NAME            "ffext"
#define VERIFY_CB_NAME      "ff_session_verify"
#define ENTER_CB_NAME       "ff_session_enter"
#define OFFLINE_CB_NAME     "ff_session_offline"
#define LOGIC_CB_NAME       "ff_session_logic"
#define TIMER_CB_NAME       "ff_timer_callback"
#define DB_QUERY_CB_NAME    "ff_db_query_callback"
#define SCENE_CALL_CB_NAME  "ff_scene_call"
#define CALL_SERVICE_RETURN_MSG_CB_NAME "ff_rpc_call_return_msg"

class fflua_t;
class ffjson_tool_t;

class fflua_mod_t: public task_processor_i
{
public:
    arg_helper_t    m_arg_helper;
public:
    fflua_mod_t();
    ~fflua_mod_t();
    int open(arg_helper_t& arg_helper, string scene_name = "");
    int close();
    string reload(const string& name_);
    void pylog(int level, const string& mod_, const string& content_);
    //! 判断某个service是否存在
    bool is_exist(const string& service_name_);
    //! 定时器接口
    int once_timer(int timeout_, uint64_t id_);
    
    ffslot_t::callback_t* gen_db_query_callback(long callback_id_);
    
    //! 创建数据库连接
    long connect_db(const string& host_);
    void db_query(long db_id_,const string& sql_, long callback_id_);
    vector<vector<string> > sync_db_query(long db_id_,const string& sql_);
    
    void call_service(const string& name_space_, const string& service_name_,
                      const string& interface_name, const string& msg_body_, long callback_id_);

    fflua_t& get_fflua(){ return *m_fflua; }
    
    //! 线程间传递消息
    bool post_task(const string& name, const string& task_name, const ffjson_tool_t& task_args, long callback_id);
    bool task_callback(const string& name, const ffjson_tool_t& task_args, long callback_id);
    
    void post(const string& task_name, const ffjson_tool_t& task_args,
              const string& from_name, long callback_id);
    void post_impl(const string& task_name, const ffjson_tool_t& task_args,
                   const string& from_name, long callback_id);
    void callback(const ffjson_tool_t& task_args, long callback_id);
    void callback_impl(const ffjson_tool_t& task_args, long callback_id);
    //!定时
    timer_service_t& get_timer() { return m_timer; }
    //! 获取任务队列对象
    task_queue_t& get_tq() { return m_tq; }
    
    //! 发送消息给特定的client
    int send_msg_session(const string& gate_name, const userid_t& session_id_, uint16_t cmd_, const string& data_);
    //! 多播
    int multicast_msg_session(const vector<userid_t>& session_id_, uint16_t cmd_, const string& data_);
    //! 广播
    int broadcast_msg_session(uint16_t cmd_, const string& data_);
    //! 广播 整个gate
    int broadcast_msg_gate(const string& gate_name_, uint16_t cmd_, const string& data_);
    //! 关闭某个session
    int close_session(const string& gate_name_, const userid_t& session_id_);
    //! 切换scene
    int change_session_scene(const string& gate_name_, const userid_t& session_id_, const string& to_scene_, const string& extra_data);
    
public:
    string          m_name;
    fflua_t*        m_fflua;
    db_mgr_t        m_db_mgr;
    timer_service_t m_timer;
    task_queue_t    m_tq;
    thread_t        m_thread;
};

}

#endif
