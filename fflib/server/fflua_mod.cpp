
#include "server/fflua_mod.h"
#include "base/performance_daemon.h"
using namespace ff;

#ifdef FF_ENABLE_LUA

#include "lua/fflua.h"
#include "server/fflua_json_traits.h"

fflua_mod_t::fflua_mod_t():
    m_arg_helper("")
{
    m_fflua = new fflua_t();
}
fflua_mod_t::~fflua_mod_t()
{
    delete m_fflua;
    m_fflua = NULL;
}

static string json_encode(ffjson_tool_t& ffjson_tool)
{
    return ffjson_tool.encode();
}

static ffjson_tool_t json_decode(const string& src)
{
    ffjson_tool_t ffjson_tool;
    ffjson_tool.decode(src);
    return ffjson_tool;
}

bool fflua_mod_t::post_task(const string& name, const string& task_name, const ffjson_tool_t& task_args, long callback_id)
{
    task_processor_i* d = singleton_t<task_processor_mgr_t>::instance().get(name);
    if (d)
    {
        d->post(task_name, task_args, m_name, callback_id);
    }
    else
    {
        d = singleton_t<task_processor_mgr_t>::instance().get(m_name);
        if (d)
        {
            ffjson_tool_t tmp;
            d->callback(tmp, callback_id);
        }
    }
    return d != NULL;
}
bool fflua_mod_t::task_callback(const string& name, const ffjson_tool_t& task_args, long callback_id)
{
    task_processor_i* d = singleton_t<task_processor_mgr_t>::instance().get(name);
    if (d)
    {
        d->callback(task_args, callback_id);
        LOGTRACE((FFSCENE_LUA, "ffscene_lua_t::py_task_callback end ok name=%s", name));
    }
    else
    {
        LOGERROR((FFSCENE_LUA, "ffscene_lua_t::py_task_callback none dest=%s", name));
    }
    return d != NULL;
}

static void lua_reg(lua_State* ls)  
{
    //! 注册基类函数, ctor() 为构造函数的类型  
    fflua_register_t<fflua_mod_t, ctor()>(ls, "ffscene_t")  //! 注册构造函数
                    .def(&fflua_mod_t::send_msg_session, "send_msg_session")
                    .def(&fflua_mod_t::multicast_msg_session, "multicast_msg_session")
                    .def(&fflua_mod_t::broadcast_msg_session, "broadcast_msg_session")
                    .def(&fflua_mod_t::broadcast_msg_gate, "broadcast_msg_gate")
                    .def(&fflua_mod_t::close_session, "close_session")
                    .def(&fflua_mod_t::change_session_scene, "change_session_scene")
                    .def(&fflua_mod_t::once_timer, "once_timer")
                    .def(&fflua_mod_t::reload, "reload")
                    .def(&fflua_mod_t::pylog, "pylog")
                    .def(&fflua_mod_t::is_exist, "is_exist")
                    .def(&fflua_mod_t::connect_db, "connect_db")
                    .def(&fflua_mod_t::db_query, "db_query")
                    .def(&fflua_mod_t::sync_db_query, "sync_db_query")
                    .def(&fflua_mod_t::call_service, "call_service")
                    .def(&fflua_mod_t::post_task, "post_task")
                    .def(&fflua_mod_t::task_callback, "task_callback");
    fflua_register_t<>(ls)  
                    .def(&ffdb_t::escape, "escape")
                    .def(&json_encode, "json_encode")
                    .def(&json_decode, "json_decode");

}

int fflua_mod_t::open(arg_helper_t& arg_helper, string scene_name)
{
    LOGTRACE((FFSCENE_LUA, "fflua_mod_t::open begin"));
    m_name = scene_name;
    m_arg_helper = arg_helper;
    
    (*m_fflua).reg(lua_reg);
    (*m_fflua).set_global_variable("ffscene", (fflua_mod_t*)this);

    (*m_fflua).add_package_path("./lualib");
    (*m_fflua).add_package_path("./luaproject");
    if (arg_helper.is_enable_option("-lua_path"))
    {
        LOGTRACE((FFSCENE_LUA, "add_package_path lua_path=%s\n", arg_helper.get_option_value("-lua_path")));
        (*m_fflua).add_package_path(arg_helper.get_option_value("-lua_path"));
    }
    
    m_db_mgr.start();

    int ret = 0;
    singleton_t<task_processor_mgr_t>::instance().add(m_name, this);
    try
    {
        (*m_fflua).do_file("main.lua");
        ret = (*m_fflua).call<int>("init");
        /*
        //rapidjson::Document::AllocatorType allocator;
        ffjson_tool_t ffjson_tool;
        ffjson_tool.jval->SetObject();
        //json_value_t jval(rapidjson::kObjectType);
        json_value_t ibj_json(rapidjson::kObjectType);
        string dest_ = "TTTT";
        json_value_t tmp_val;
        tmp_val.SetString(dest_.c_str(), dest_.length(), *ffjson_tool.allocator);
        json_value_t tmp_val2(-10241.3);
        ibj_json.AddMember("dumy", tmp_val, *ffjson_tool.allocator);
        ffjson_tool.jval->AddMember("foo", ibj_json, *(ffjson_tool.allocator));
        ffjson_tool.jval->AddMember("go", tmp_val2, *(ffjson_tool.allocator));     

        (*m_fflua).call<void>("test", ffjson_tool);
        */
    }
    catch(exception& e_)
    {
        LOGERROR((FFSCENE_LUA, "fflua_mod_t::open failed er=<%s>", e_.what()));
        return -1;
    }
    m_thread.create_thread(task_binder_t::gen(&task_queue_t::run, &m_tq), 1);
    LOGTRACE((FFSCENE_LUA, "fflua_mod_t::open end ok"));
    return ret;
}

int fflua_mod_t::close()
{
    singleton_t<task_processor_mgr_t>::instance().del(m_name);
    int ret = 0;
    try
    {
        ret = (*m_fflua).call<int>("cleanup");
    }
    catch(exception& e_)
    {
        LOGERROR((FFSCENE_LUA, "fflua_mod_t::close failed er=%s", e_.what()));
    }
    m_db_mgr.stop();
    return ret;
}

string fflua_mod_t::reload(const string& name_)
{
    AUTO_PERF();
    LOGTRACE((FFSCENE_LUA, "fflua_mod_t::reload begin name_[%s]", name_));
    try
    {
        //! ffpython_t::reload(name_);
        //! TODO
    }
    catch(exception& e_)
    {
        LOGERROR((FFSCENE_LUA, "fflua_mod_t::reload exeception=%s", e_.what()));
        return e_.what();
    }
    LOGTRACE((FFSCENE_LUA, "fflua_mod_t::reload end ok name_[%s]", name_));
    return "";
}

void fflua_mod_t::pylog(int level_, const string& mod_, const string& content_)
{
    switch (level_)
    {
        case 1:
        {
            LOGFATAL((mod_.c_str(), "%s", content_));
        }
        break;
        case 2:
        {
            LOGERROR((mod_.c_str(), "%s", content_));
        }
        break;
        case 3:
        {
            LOGWARN((mod_.c_str(), "%s", content_));
        }
        break;
        case 4:
        {
            LOGINFO((mod_.c_str(), "%s", content_));
        }
        break;
        case 5:
        {
            LOGDEBUG((mod_.c_str(), "%s", content_));
        }
        break;
        case 6:
        {
            LOGTRACE((mod_.c_str(), "%s", content_));
        }
        break;
        default:
        {
            LOGTRACE((mod_.c_str(), "%s", content_));
        }
        break;
    }
}
//! 判断某个service是否存在
bool fflua_mod_t::is_exist(const string& service_name_)
{
    return false; //!TODO m_ffrpc->is_exist(service_name_);
}

//! 定时器接口
int fflua_mod_t::once_timer(int timeout_, uint64_t id_)
{
    struct lambda_cb
    {
        static void call_py(fflua_mod_t* ffscene, uint64_t id)
        {
            LOGDEBUG((FFSCENE_LUA, "fflua_mod_t::once_timer call_py id<%u>", id));
            static string func_name  = TIMER_CB_NAME;
            PERF("once_timer");
            try
            {
                ffscene->get_fflua().call<void>(func_name.c_str(), id);
            }
            catch(exception& e_)
            {
                LOGERROR((FFSCENE_LUA, "fflua_mod_t::gen_logic_callback exception<%s>", e_.what()));
            }
        }
        static void callback(fflua_mod_t* ffscene, task_queue_t* tq_, uint64_t id)
        {
            tq_->produce(task_binder_t::gen(&lambda_cb::call_py, ffscene, id));
        }
    };
    LOGDEBUG((FFSCENE_LUA, "fflua_mod_t::once_timer begin id<%u>", id_));
    get_timer().once_timer(timeout_, task_binder_t::gen(&lambda_cb::callback, this, &(get_tq()), id_));
    return 0;
}

ffslot_t::callback_t* fflua_mod_t::gen_db_query_callback(long callback_id_)
{
    struct lambda_cb: public ffslot_t::callback_t
    {
        lambda_cb(fflua_mod_t* p, long callback_id_):ffscene(p), callback_id(callback_id_){}
        virtual void exe(ffslot_t::callback_arg_t* args_)
        {
            if (args_->type() != TYPEID(db_mgr_t::db_query_result_t))
            {
                return;
            }
            db_mgr_t::db_query_result_t* data = (db_mgr_t::db_query_result_t*)args_;

            ffscene->get_tq().produce(task_binder_t::gen(&lambda_cb::call_python, ffscene, callback_id,
                                                                   data->ok, data->result_data, data->col_names));
        }
        static void call_python(fflua_mod_t* ffscene, long callback_id_,
                                bool ok, const vector<vector<string> >& ret_, const vector<string>& col_)
        {
            PERF("db_query_callback");
            static string func_name   = DB_QUERY_CB_NAME;
            try
            {
                ffscene->get_fflua().call<void>(func_name.c_str(),
                                               callback_id_, ok, ret_, col_);

            }
            catch(exception& e_)
            {
                LOGERROR((FFSCENE_LUA, "fflua_mod_t::gen_db_query_callback exception<%s>", e_.what()));
            }
        }
        virtual ffslot_t::callback_t* fork() { return new lambda_cb(ffscene, callback_id); }
        fflua_mod_t* ffscene;
        long              callback_id;
    };
    return new lambda_cb(this, callback_id_);
}


//! 创建数据库连接
long fflua_mod_t::connect_db(const string& host_)
{
    return m_db_mgr.connect_db(host_);
}
void fflua_mod_t::db_query(long db_id_,const string& sql_, long callback_id_)
{
    m_db_mgr.db_query(db_id_, sql_, gen_db_query_callback(callback_id_));
}
vector<vector<string> > fflua_mod_t::sync_db_query(long db_id_,const string& sql_)
{
    vector<vector<string> > ret;
    m_db_mgr.sync_db_query(db_id_, sql_, ret);
    return ret;
}
void fflua_mod_t::call_service(const string& name_space_, const string& service_name_,
                                 const string& interface_name, const string& msg_body_, long callback_id_)
{
    LOGTRACE((FFSCENE_LUA, "fflua_mod_t::call_service service=%s,interface=%s", service_name_, interface_name));
    ffscene_t* ffscene = singleton_t<ffscene_mgr_t>::instance().get_any();
    if (ffscene == NULL)
    {
        LOGERROR((FFSCENE_LUA, "fflua_mod_t::call_service no service=%s,interface=%s", service_name_, interface_name));
        return;
    }
    struct lambda_cb: public ffslot_t::callback_t
    {
        static void call_back(fflua_mod_t* ffscene, ffslot_req_arg ffslot_req, long m_callback_id)
        {
            ffslot_req_arg* msg_data = &ffslot_req;
            try
            {
                ffscene->get_fflua().call<void>(CALL_SERVICE_RETURN_MSG_CB_NAME, m_callback_id,
                                                  msg_data->body, msg_data->err_info);
            }
            catch(exception& e_)
            {
                LOGERROR((FFSCENE_LUA, "FFSCENE_LUA_t::call_service exception=%s", e_.what()));
            }
        }
        lambda_cb(fflua_mod_t* ffscene, long callback_id_):m_ffscene(ffscene),m_callback_id(callback_id_){}
        virtual void exe(ffslot_t::callback_arg_t* args_)
        {
            if (args_->type() != TYPEID(ffslot_req_arg))
            {
                return;
            }
            ffslot_req_arg* msg_data = (ffslot_req_arg*)args_;
            m_ffscene->get_tq().produce(task_binder_t::gen(&call_back, m_ffscene, *msg_data, m_callback_id));
        }
        virtual ffslot_t::callback_t* fork() { return new lambda_cb(m_ffscene, m_callback_id); }
        fflua_mod_t*      m_ffscene;
        long		      m_callback_id;
    };

    ffslot_t::callback_t* func = NULL;
    if (callback_id_ != 0)
    {
        func = new lambda_cb(this, callback_id_);
    }
	
    if (name_space_.empty())
    {
        ffscene->get_rpc().get_tq().produce(task_binder_t::gen(&ffrpc_t::call_impl, &(ffscene->get_rpc()),
                                            service_name_, interface_name, msg_body_, func));
    }
    else
    {
        ffscene->get_rpc().get_tq().produce(task_binder_t::gen(&ffrpc_t::bridge_call_impl, &(ffscene->get_rpc()),
                                            name_space_, service_name_, interface_name, msg_body_, func));
    }
}
//! 线程间传递消息

void fflua_mod_t::post(const string& task_name, const ffjson_tool_t& task_args,
                         const string& from_name, long callback_id)
{
    get_tq().produce(task_binder_t::gen(&fflua_mod_t::post_impl, this, task_name, task_args, from_name, callback_id));
}
void fflua_mod_t::post_impl(const string& task_name, const ffjson_tool_t& task_args,
                              const string& from_name, long callback_id)
{
    try
    {
        (*m_fflua).call<void>("process_task", task_name, task_args, from_name, callback_id);
         
    }
    catch(exception& e_)
    {
        LOGERROR((FFSCENE_LUA, "fflua_mod_t::post_task_impl exception<%s>", e_.what()));
    }
}

void fflua_mod_t::callback(const ffjson_tool_t& task_args, long callback_id)
{
    get_tq().produce(task_binder_t::gen(&fflua_mod_t::callback_impl, this, task_args, callback_id));
}

void fflua_mod_t::callback_impl(const ffjson_tool_t& task_args, long callback_id)
{
    try
    {
        (*m_fflua).call<void>("process_task_callback", task_args, callback_id);
         
    }
    catch(exception& e_)
    {
        LOGERROR((FFSCENE_LUA, "fflua_mod_t::callback_impl exception<%s>", e_.what()));
    }
}

//! 发送消息给特定的client
int fflua_mod_t::send_msg_session(const string& gate_name, const userid_t& session_id_, uint16_t cmd_, const string& data_)
{
    ffscene_t* ffscene = singleton_t<ffscene_mgr_t>::instance().get_any();
    if (ffscene)
    {
        ffscene->get_rpc().get_tq().produce(task_binder_t::gen(&ffscene_t::send_msg_session,
                                            ffscene, gate_name, session_id_, cmd_, data_));
    }
    return 0;
}

//! 多播
int fflua_mod_t::multicast_msg_session(const vector<userid_t>& session_id_, uint16_t cmd_, const string& data_)
{
    ffscene_t* ffscene = singleton_t<ffscene_mgr_t>::instance().get_any();
    if (ffscene)
    {
        //! TODO ffscene->get_rpc().get_tq().produce(task_binder_t::gen(&ffscene_t::multicast_msg_session,
        //! TODO                                     ffscene, session_id_, cmd_, data_));
    }
    return 0;
}


//! 广播
int fflua_mod_t::broadcast_msg_session(uint16_t cmd_, const string& data_)
{
    ffscene_t* ffscene = singleton_t<ffscene_mgr_t>::instance().get_any();
    if (ffscene)
    {
        //! TODO ffscene->get_rpc().get_tq().produce(task_binder_t::gen(&ffscene_t::broadcast_msg_session, ffscene, cmd_, data_));
    }
    return 0;
}

//! 广播 整个gate
int fflua_mod_t::broadcast_msg_gate(const string& gate_name_, uint16_t cmd_, const string& data_)
{
    ffscene_t* ffscene = singleton_t<ffscene_mgr_t>::instance().get_any();
    if (ffscene)
    {
        ffscene->get_rpc().get_tq().produce(task_binder_t::gen(&ffscene_t::broadcast_msg_gate,
                                                               ffscene, gate_name_, cmd_, data_));
    }
    return 0;
}

//! 关闭某个session
int fflua_mod_t::close_session(const string& gate_name_, const userid_t& session_id_)
{
    ffscene_t* ffscene = singleton_t<ffscene_mgr_t>::instance().get_any();
    if (ffscene)
    {
        ffscene->get_rpc().get_tq().produce(task_binder_t::gen(&ffscene_t::close_session, ffscene, gate_name_, session_id_));
    }
    return 0;
}

//! 切换scene

int fflua_mod_t::change_session_scene(const string& gate_name_, const userid_t& session_id_, const string& to_scene_, const string& extra_data)
{
    ffscene_t* ffscene = singleton_t<ffscene_mgr_t>::instance().get_any();
    if (ffscene)
    {
        //ffscene->get_rpc().get_tq().produce(task_binder_t::gen(&ffscene_t::change_session_scene, ffscene, session_id_, to_scene_, extra_data));
    }
    return 0;
}

#else
    
fflua_mod_t::fflua_mod_t():
    m_arg_helper(""), m_fflua(NULL)
{
}
fflua_mod_t::~fflua_mod_t()
{
}

int fflua_mod_t::open(arg_helper_t& arg_helper, string scene_name)
{
    return -1;
}
int fflua_mod_t::close()
{
    return -1;
}
void fflua_mod_t::post(const string& task_name, const ffjson_tool_t& task_args,
              const string& from_name, long callback_id)
{
}
void fflua_mod_t::callback(const ffjson_tool_t& task_args, long callback_id)
{
}

#endif

