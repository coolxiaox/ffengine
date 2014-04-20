
#include "server/ffscene_lua.h"
#include "base/performance_daemon.h"

using namespace ff;

#ifdef FF_ENABLE_LUA

#include "lua/fflua.h"
#include "server/fflua_json_traits.h"

ffscene_lua_t::ffscene_lua_t():
    m_arg_helper(""),m_fflua(NULL)
{
    m_fflua = new fflua_t();
}
ffscene_lua_t::~ffscene_lua_t()
{
    if (m_fflua)
    {
        delete m_fflua;
        m_fflua = NULL;
    }
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
bool ffscene_lua_t::post_task(const string& name, const string& task_name, const ffjson_tool_t& task_args, long callback_id)
{
    task_processor_i* d = singleton_t<task_processor_mgr_t>::instance().get(name);
    if (d)
    {
        d->post(task_name, task_args, this->get_scene_name(), callback_id);
    }
    else
    {
        d = singleton_t<task_processor_mgr_t>::instance().get(this->get_scene_name());
        if (d)
        {
            ffjson_tool_t tmp;
            d->callback(tmp, callback_id);
        }
    }
    return d != NULL;
}
bool ffscene_lua_t::task_callback(const string& name, const ffjson_tool_t& task_args, long callback_id)
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
    fflua_register_t<ffscene_lua_t, ctor()>(ls, "ffscene_t")  //! 注册构造函数
                    .def(&ffscene_lua_t::send_msg_session, "send_msg_session")
                    //.def(&ffscene_lua_t::multicast_msg_session, "multicast_msg_session")
                    //.def(&ffscene_lua_t::broadcast_msg_session, "broadcast_msg_session")
                    .def(&ffscene_lua_t::broadcast_msg_gate, "broadcast_msg_gate")
                    .def(&ffscene_lua_t::close_session, "close_session")
                    .def(&ffscene_lua_t::change_session_scene, "change_session_scene")
                    .def(&ffscene_lua_t::once_timer, "once_timer")
                    .def(&ffscene_lua_t::reload, "reload")
                    .def(&ffscene_lua_t::pylog, "pylog")
                    .def(&ffscene_lua_t::is_exist, "is_exist")
                    .def(&ffscene_lua_t::connect_db, "connect_db")
                    .def(&ffscene_lua_t::db_query, "db_query")
                    .def(&ffscene_lua_t::sync_db_query, "sync_db_query")
                    .def(&ffscene_lua_t::call_service, "call_service")
                    .def(&ffscene_lua_t::post_task, "post_task")
                    .def(&ffscene_lua_t::task_callback, "task_callback");

    fflua_register_t<>(ls)  
                    .def(&ffdb_t::escape, "escape")
                    .def(&json_encode, "json_encode")
                    .def(&json_decode, "json_decode");

}

int ffscene_lua_t::open(arg_helper_t& arg_helper, string scene_name)
{
    LOGTRACE((FFSCENE_LUA, "ffscene_lua_t::open begin"));
    m_arg_helper = arg_helper;
    
    (*m_fflua).reg(lua_reg);
    (*m_fflua).set_global_variable("ffscene", (ffscene_lua_t*)this);

    this->callback_info().verify_callback = gen_verify_callback();
    this->callback_info().enter_callback = gen_enter_callback();
    this->callback_info().offline_callback = gen_offline_callback();
    this->callback_info().logic_callback = gen_logic_callback();
    this->callback_info().scene_call_callback = gen_scene_call_callback();

    (*m_fflua).add_package_path("./lualib");
    (*m_fflua).add_package_path("./luaproject");
    if (arg_helper.is_enable_option("-lua_path"))
    {
        LOGTRACE((FFSCENE_LUA, "add_package_path lua_path=%s\n", arg_helper.get_option_value("-lua_path")));
        (*m_fflua).add_package_path(arg_helper.get_option_value("-lua_path"));
    }
    
    m_db_mgr.start();

    int ret = ffscene_t::open(arg_helper, scene_name);
    singleton_t<task_processor_mgr_t>::instance().add(this->get_scene_name(), this);
    try
    {
        (*m_fflua).do_file("main.lua");
        ret = (*m_fflua).call<int>("init");
        
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
    }
    catch(exception& e_)
    {
        LOGERROR((FFSCENE_LUA, "ffscene_lua_t::open failed er=<%s>", e_.what()));
        ffscene_t::close();
        return -1;
    }
    LOGTRACE((FFSCENE_LUA, "ffscene_lua_t::open end ok"));
    return ret;
}

int ffscene_lua_t::close()
{
    if (this->get_scene_name().empty())
    {
        return 0;
    }
    singleton_t<task_processor_mgr_t>::instance().del(this->get_scene_name());
    int ret = 0;
    try
    {
        ret = (*m_fflua).call<int>("cleanup");
    }
    catch(exception& e_)
    {
        LOGERROR((FFSCENE_LUA, "ffscene_lua_t::close failed er=%s", e_.what()));
    }
    ffscene_t::close();
    m_db_mgr.stop();
    return ret;
}

string ffscene_lua_t::reload(const string& name_)
{
    AUTO_PERF();
    LOGTRACE((FFSCENE_LUA, "ffscene_lua_t::reload begin name_[%s]", name_));
    try
    {
        //! ffpython_t::reload(name_);
        //! TODO
    }
    catch(exception& e_)
    {
        LOGERROR((FFSCENE_LUA, "ffscene_lua_t::reload exeception=%s", e_.what()));
        return e_.what();
    }
    LOGTRACE((FFSCENE_LUA, "ffscene_lua_t::reload end ok name_[%s]", name_));
    return "";
}

void ffscene_lua_t::pylog(int level_, const string& mod_, const string& content_)
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
bool ffscene_lua_t::is_exist(const string& service_name_)
{
    return m_ffrpc->is_exist(service_name_);
}

ffslot_t::callback_t* ffscene_lua_t::gen_verify_callback()
{
    struct lambda_cb: public ffslot_t::callback_t
    {
        lambda_cb(ffscene_lua_t* p):ffscene(p){}
        virtual void exe(ffslot_t::callback_arg_t* args_)
        {
            PERF("verify_callback");
            if (args_->type() != TYPEID(session_verify_arg))
            {
                return;
            }
            session_verify_arg* data = (session_verify_arg*)args_;
            static string func_name  = VERIFY_CB_NAME;
            try
            {
                vector<string> ret = ffscene->get_fflua().call<vector<string> >(func_name.c_str(),
                                                                               data->cmd, data->msg_body,
                                                                               data->socket_id,
                                                                               data->ip, data->gate_name);
                if (ret.size() >= 1)
                {
                    data->flag_verify = true;
                    data->alloc_session_id = ::atol(ret[0].c_str());
                }
                if (ret.size() >= 2)
                {
                    data->extra_data = ret[1];
                }
            }
            catch(exception& e_)
            {
                LOGERROR((FFSCENE_LUA, "ffscene_lua_t::gen_verify_callback exception<%s>", e_.what()));
            }
        }
        virtual ffslot_t::callback_t* fork() { return new lambda_cb(ffscene); }
        ffscene_lua_t* ffscene;
    };
    return new lambda_cb(this);
}

ffslot_t::callback_t* ffscene_lua_t::gen_enter_callback()
{
    struct lambda_cb: public ffslot_t::callback_t
    {
        lambda_cb(ffscene_lua_t* p):ffscene(p){}
        virtual void exe(ffslot_t::callback_arg_t* args_)
        {
            PERF("enter_callback");
            if (args_->type() != TYPEID(session_enter_arg))
            {
                return;
            }
            session_enter_arg* data = (session_enter_arg*)args_;
            static string func_name  = ENTER_CB_NAME;
            try
            {
                ffscene->get_fflua().call<void>(func_name.c_str(),
                                               data->session_id, data->from_scene,
                                               data->extra_data);
            }
            catch(exception& e_)
            {
                LOGERROR((FFSCENE_LUA, "ffscene_lua_t::gen_enter_callback exception<%s>", e_.what()));
            }
        }
        virtual ffslot_t::callback_t* fork() { return new lambda_cb(ffscene); }
        ffscene_lua_t* ffscene;
    };
    return new lambda_cb(this);
}

ffslot_t::callback_t* ffscene_lua_t::gen_offline_callback()
{
    struct lambda_cb: public ffslot_t::callback_t
    {
        lambda_cb(ffscene_lua_t* p):ffscene(p){}
        virtual void exe(ffslot_t::callback_arg_t* args_)
        {
            PERF("offline_callback");
            if (args_->type() != TYPEID(session_offline_arg))
            {
                return;
            }
            session_offline_arg* data = (session_offline_arg*)args_;
            static string func_name   = OFFLINE_CB_NAME;
            try
            {
                ffscene->get_fflua().call<void>(func_name.c_str(),
                                               data->session_id);
            }
            catch(exception& e_)
            {
                LOGERROR((FFSCENE_LUA, "ffscene_lua_t::gen_offline_callback exception<%s>", e_.what()));
            }
        }
        virtual ffslot_t::callback_t* fork() { return new lambda_cb(ffscene); }
        ffscene_lua_t* ffscene;
    };
    return new lambda_cb(this);
}
ffslot_t::callback_t* ffscene_lua_t::gen_logic_callback()
{
    struct lambda_cb: public ffslot_t::callback_t
    {
        lambda_cb(ffscene_lua_t* p):ffscene(p){}
        virtual void exe(ffslot_t::callback_arg_t* args_)
        {
            if (args_->type() != TYPEID(logic_msg_arg))
            {
                return;
            }
            logic_msg_arg* data = (logic_msg_arg*)args_;
            static string func_name  = LOGIC_CB_NAME;
            LOGDEBUG((FFSCENE_LUA, "ffscene_lua_t::gen_logic_callback len[%lu]", data->body.size()));
            
            AUTO_CMD_PERF("logic_callback", data->cmd);
            try
            {
                ffscene->get_fflua().call<void>(func_name.c_str(),
                                               data->session_id, data->cmd, data->body);
            }
            catch(exception& e_)
            {
                LOGERROR((FFSCENE_LUA, "ffscene_lua_t::gen_logic_callback exception<%s>", e_.what()));
            }
        }
        virtual ffslot_t::callback_t* fork() { return new lambda_cb(ffscene); }
        ffscene_lua_t* ffscene;
    };
    return new lambda_cb(this);
}

ffslot_t::callback_t* ffscene_lua_t::gen_scene_call_callback()
{
    struct lambda_cb: public ffslot_t::callback_t
    {
        lambda_cb(ffscene_lua_t* p):ffscene(p){}
        virtual void exe(ffslot_t::callback_arg_t* args_)
        {
            if (args_->type() != TYPEID(scene_call_msg_arg))
            {
                return;
            }
            scene_call_msg_arg* data = (scene_call_msg_arg*)args_;
            static string func_name  = SCENE_CALL_CB_NAME;
            LOGINFO((FFSCENE_LUA, "ffscene_lua_t::gen_scene_call_callback len[%lu]", data->body.size()));
            
            AUTO_CMD_PERF("scene_callback", data->cmd);
            try
            {
                vector<string> ret = ffscene->get_fflua().call<vector<string> >(func_name.c_str(),
                                                                                data->cmd, data->body);
                if (ret.size() == 2)
                {
                    data->msg_type = ret[0];
                    data->ret      = ret[1];
                }
            }
            catch(exception& e_)
            {
                data->err = e_.what();
                LOGERROR((FFSCENE_LUA, "ffscene_lua_t::gen_scene_call_callback exception<%s>", e_.what()));
            }
        }
        virtual ffslot_t::callback_t* fork() { return new lambda_cb(ffscene); }
        ffscene_lua_t* ffscene;
    };
    return new lambda_cb(this);
}

//! 定时器接口
int ffscene_lua_t::once_timer(int timeout_, uint64_t id_)
{
    struct lambda_cb
    {
        static void call_py(ffscene_lua_t* ffscene, uint64_t id)
        {
            LOGDEBUG((FFSCENE_LUA, "ffscene_lua_t::once_timer call_py id<%u>", id));
            static string func_name  = TIMER_CB_NAME;
            PERF("once_timer");
            try
            {
                ffscene->get_fflua().call<void>(func_name.c_str(), id);
            }
            catch(exception& e_)
            {
                LOGERROR((FFSCENE_LUA, "ffscene_lua_t::gen_logic_callback exception<%s>", e_.what()));
            }
        }
        static void callback(ffscene_lua_t* ffscene, task_queue_t* tq_, uint64_t id)
        {
            tq_->produce(task_binder_t::gen(&lambda_cb::call_py, ffscene, id));
        }
    };
    LOGDEBUG((FFSCENE_LUA, "ffscene_lua_t::once_timer begin id<%u>", id_));
    m_ffrpc->get_timer().once_timer(timeout_, task_binder_t::gen(&lambda_cb::callback, this, &(m_ffrpc->get_tq()), id_));
    return 0;
}

ffslot_t::callback_t* ffscene_lua_t::gen_db_query_callback(long callback_id_)
{
    struct lambda_cb: public ffslot_t::callback_t
    {
        lambda_cb(ffscene_lua_t* p, long callback_id_):ffscene(p), callback_id(callback_id_){}
        virtual void exe(ffslot_t::callback_arg_t* args_)
        {
            if (args_->type() != TYPEID(db_mgr_t::db_query_result_t))
            {
                return;
            }
            db_mgr_t::db_query_result_t* data = (db_mgr_t::db_query_result_t*)args_;

            ffscene->get_rpc().get_tq().produce(task_binder_t::gen(&lambda_cb::call_python, ffscene, callback_id,
                                                                   data->ok, data->result_data, data->col_names));
        }
        static void call_python(ffscene_lua_t* ffscene, long callback_id_,
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
                LOGERROR((FFSCENE_LUA, "ffscene_lua_t::gen_db_query_callback exception<%s>", e_.what()));
            }
        }
        virtual ffslot_t::callback_t* fork() { return new lambda_cb(ffscene, callback_id); }
        ffscene_lua_t* ffscene;
        long              callback_id;
    };
    return new lambda_cb(this, callback_id_);
}


//! 创建数据库连接
long ffscene_lua_t::connect_db(const string& host_)
{
    return m_db_mgr.connect_db(host_);
}
void ffscene_lua_t::db_query(long db_id_,const string& sql_, long callback_id_)
{
    m_db_mgr.db_query(db_id_, sql_, gen_db_query_callback(callback_id_));
}
vector<vector<string> > ffscene_lua_t::sync_db_query(long db_id_,const string& sql_)
{
    vector<vector<string> > ret;
    m_db_mgr.sync_db_query(db_id_, sql_, ret);
    return ret;
}
void ffscene_lua_t::call_service(const string& name_space_, const string& service_name_,
                                 const string& interface_name, const string& msg_body_, long callback_id_)
{
    struct lambda_cb: public ffslot_t::callback_t
    {
        lambda_cb(ffscene_lua_t* ffscene, long callback_id_):m_ffscene(ffscene),m_callback_id(callback_id_){}
        virtual void exe(ffslot_t::callback_arg_t* args_)
        {
            if (args_->type() != TYPEID(ffslot_req_arg))
            {
                return;
            }
            ffslot_req_arg* msg_data = (ffslot_req_arg*)args_;
            try
            {
                m_ffscene->get_fflua().call<void>(CALL_SERVICE_RETURN_MSG_CB_NAME, m_callback_id, msg_data->body, msg_data->err_info);
            }
            catch(exception& e_)
            {
                LOGERROR((FFSCENE_LUA, "FFSCENE_LUA_t::call_service exception=%s", e_.what()));
            }
        }
        virtual ffslot_t::callback_t* fork() { return new lambda_cb(m_ffscene, m_callback_id); }
        ffscene_lua_t*    m_ffscene;
        long		      m_callback_id;
    };

    ffslot_t::callback_t* func = NULL;
    if (callback_id_ != 0)
    {
        func = new lambda_cb(this, callback_id_);
    }
	
    if (name_space_.empty())
    {
        m_ffrpc->call_impl(service_name_, interface_name, msg_body_, func);
    }
    else
    {
        m_ffrpc->bridge_call_impl(name_space_, service_name_, interface_name, msg_body_, func);
    }
}

//! 线程间传递消息
void ffscene_lua_t::post(const string& task_name, const ffjson_tool_t& task_args,
                         const string& from_name, long callback_id)
{
    m_ffrpc->get_tq().produce(task_binder_t::gen(&ffscene_lua_t::post_impl, this, task_name, task_args, from_name, callback_id));
}
void ffscene_lua_t::post_impl(const string& task_name, const ffjson_tool_t& task_args,
                              const string& from_name, long callback_id)
{
    try
    {
        (*m_fflua).call<void>("process_task", task_name, task_args, from_name, callback_id);
         
    }
    catch(exception& e_)
    {
        LOGERROR((FFSCENE_LUA, "ffscene_lua_t::post_task_impl exception<%s>", e_.what()));
    }
}

void ffscene_lua_t::callback(const ffjson_tool_t& task_args, long callback_id)
{
    m_ffrpc->get_tq().produce(task_binder_t::gen(&ffscene_lua_t::callback_impl, this, task_args, callback_id));
}

void ffscene_lua_t::callback_impl(const ffjson_tool_t& task_args, long callback_id)
{
    try
    {
        (*m_fflua).call<void>("process_task_callback", task_args, callback_id);
         
    }
    catch(exception& e_)
    {
        LOGERROR((FFSCENE_LUA, "ffscene_lua_t::callback_impl exception<%s>", e_.what()));
    }
}


//! 使用lua注册scene接口  name_为输入消息的名称 func_id_为函数id
void ffscene_lua_t::reg_scene_interface(const string& name_)
{
    struct lambda_cb: public ffslot_t::callback_t
    {
        lambda_cb(ffscene_lua_t* ffscene, const string& name_):m_ffscene(ffscene),m_name(name_){}
        virtual void exe(ffslot_t::callback_arg_t* args_)
        {
            if (args_->type() != TYPEID(ffslot_req_arg))
            {
                return;
            }
            ffslot_req_arg* msg_data = (ffslot_req_arg*)args_;
            m_ffscene->get_fflua().call<void>("ff_call_scene_interface", m_name, msg_data->body);
        }
        virtual ffslot_t::callback_t* fork() { return new lambda_cb(m_ffscene, m_name); }
        ffscene_lua_t*	  m_ffscene;
        string		      m_name;
    };
    ffslot_t::callback_t* func = new lambda_cb(this, name_);
    this->m_ffrpc->reg(name_, func);
}

#else


ffscene_lua_t::ffscene_lua_t():
    m_arg_helper(""),m_fflua(NULL)
{
}
ffscene_lua_t::~ffscene_lua_t()
{
}

int ffscene_lua_t::open(arg_helper_t& arg_helper, string scene_name)
{
    return -1;
}
int ffscene_lua_t::close()
{
    return -1;
}
void ffscene_lua_t::post(const string& task_name, const ffjson_tool_t& task_args,
              const string& from_name, long callback_id)
{
}
void ffscene_lua_t::callback(const ffjson_tool_t& task_args, long callback_id)
{
}
#endif


