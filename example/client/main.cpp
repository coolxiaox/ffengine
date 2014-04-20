#include <stdio.h>
#include "base/daemon_tool.h"
#include "base/arg_helper.h"
#include "base/strtool.h"
#include "base/smart_ptr.h"

#include "base/log.h"
#include "base/signal_helper.h"
#include "base/daemon_tool.h"
#include "base/performance_daemon.h"
#include "net/net_factory.h"

#define FFNET "FFNET"

using namespace ff;
//./app_redrabbit -gate gate@0 -broker tcp://127.0.0.1:10241 -gate_listen tcp://121.199.21.238:10242 -python_path ./ -scene scene@0

class ffclient_t: public msg_handler_i
{
public:
    //! 处理连接断开
    int handle_broken(socket_ptr_t sock_){
        //! 处理连接断开，只需要销毁socket即可
        sock_->safe_delete();
        return 0;
    }
    //! 处理消息
    int handle_msg(const message_t& msg_, socket_ptr_t sock_)
    {
        uint16_t cmd       = msg_.get_cmd();
        const string& body = msg_.get_body();
        LOGINFO((FFNET, "handle_msg cmd=%d, body=%d", cmd, body.size()));
        switch (cmd)
        {
            default:
                break;
        }
        return 0;
    }
};


int main(int argc, char* argv[])
{
    if (argc == 1)
    {
        printf("usage: %s -host tcp://ip:port\n", argv[0]);
        return 0;
    }

    arg_helper_t arg_helper(argc, argv);
    //arg_helper.load_from_file("default.config");

    if (arg_helper.is_enable_option("-d"))
    {
        daemon_tool_t::daemon();
    }
    
    //! 美丽的日志组件，shell输出是彩色滴！！
    if (arg_helper.is_enable_option("-log_path"))
    {
        LOG.start(arg_helper);
    }
    else
    {
        LOG.start("-log_path ./log -log_filename log -log_class DB_MGR,XX,BROKER,FFRPC,FFGATE,FFSCENE,FFSCENE_PYTHON,FFNET -log_print_screen true -log_print_file true -log_level 6");
    }
    string perf_path = "./perf";
    long perf_timeout = 30*60;//! second
    if (arg_helper.is_enable_option("-perf_path"))
    {
        perf_path = arg_helper.get_option_value("-perf_path");
    }
    if (arg_helper.is_enable_option("-perf_timeout"))
    {
        perf_timeout = ::atoi(arg_helper.get_option_value("-perf_timeout").c_str());
    }
    if (PERF_MONITOR.start(perf_path, perf_timeout))
    {
        return -1;
    }
    
    ffclient_t ffclient;
    string host = arg_helper.get_option_value("-host");
    socket_ptr_t sock = net_factory_t::connect(host, &ffclient);
    if (NULL == sock)
    {
        LOGERROR((FFNET, "can't connect to remote <%s>", host));
        goto err_proc;
    }
    //!发送消息, 可以直接发送 pb thrift string 消息
    //msg_sender_t::send(sock, CHAT_REQ, reg_msg);
    signal_helper_t::wait();

err_proc:
    net_factory_t::stop();
    return 0;
}

