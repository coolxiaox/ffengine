#include <stdio.h>
#include "base/daemon_tool.h"
#include "base/arg_helper.h"
#include "base/strtool.h"
#include "base/smart_ptr.h"

#include "rpc/ffrpc.h"
#include "rpc/ffbroker.h"
#include "base/log.h"
#include "base/signal_helper.h"
#include "echo_test.h"
#include "protobuf_test.h"
#include "thrift_test.h"

using namespace ff;

int main(int argc, char* argv[])
{
    //! 美丽的日志组件，shell输出是彩色滴！！
    LOG.start("-log_path ./log -log_filename log -log_class XX,BROKER,FFRPC -log_print_screen true -log_print_file false -log_level 4");

    if (argc == 1)
    {
        printf("usage: %s -broker tcp://127.0.0.1:10241\n", argv[0]);
        return 1;
    }
    arg_helper_t arg_helper(argc, argv);

    //! 启动broker，负责网络相关的操作，如消息转发，节点注册，重连等

    ffbroker_t ffbroker;
    if (ffbroker.open(arg_helper))
    {
        return -1;
    }

    sleep(1);
    if (arg_helper.is_enable_option("-echo_test"))
    {
        run_echo_test(arg_helper);        
    }
    else if (arg_helper.is_enable_option("-protobuf_test"))
    {
        run_protobuf_test(arg_helper);
    }
    else if (arg_helper.is_enable_option("-thrift_test"))
    {
        run_thrift_test(arg_helper);
    }
    else
    {
        printf("usage %s -broker tcp://127.0.0.1:10241 -echo_test\n", argv[0]);
        return -1;
    }

    ffbroker.close();
    return 0;
}
