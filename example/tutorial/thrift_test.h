#ifndef _FF_THRIFT_ECHO_H_
#define _FF_THRIFT_ECHO_H_

#include "echo_types.h"
#include "rpc/ffrpc.h"

namespace ff
{

struct thrift_service_t
{
    //! echo接口，返回请求的发送的消息ffreq_t可以提供两个模板参数，第一个表示输入的消息（请求者发送的）
    //! 第二个模板参数表示该接口要返回的结果消息类型
    void echo(ffreq_t<echo_thrift_in_t, echo_thrift_out_t>& req_)
    {
        LOGINFO(("XX", "foo_t::echo: recv data=%s", req_.msg.data));
        echo_thrift_out_t out;
        out.data = "123456";
        //sleep(1);
        req_.response(out);
    }
};

struct thrift_client_t
{
    //! 远程调用接口，可以指定回调函数（也可以留空），同样使用ffreq_t指定输入消息类型，并且可以使用lambda绑定参数
    void echo_callback(ffreq_t<echo_thrift_out_t>& req_, int index, ffrpc_t* ffrpc_client)
    {
        if (req_.error())
        {
            LOGERROR(("XX", "error_msg <%s>", req_.error_msg()));
            return;
        }
        else if (index < 1)
        {
            echo_thrift_in_t in;
            in.data = "Ohnice";
            LOGINFO(("XX", "%s data=%s index=%d callback...", __FUNCTION__, req_.msg.data, index));
            sleep(1);
            ffrpc_client->call("echo", in, ffrpc_ops_t::gen_callback(&thrift_client_t::echo_callback, this, ++index, ffrpc_client));
        }
        else
        {
            LOGINFO(("XX", "%s %d callback end", __FUNCTION__, index));
        }
    }
};

static  int run_thrift_test(arg_helper_t& arg_helper)
{
    thrift_service_t foo;
    //! broker客户端，可以注册到broker，并注册服务以及接口，也可以远程调用其他节点的接口
    ffrpc_t ffrpc_service("echo");
    ffrpc_service.reg(&thrift_service_t::echo, &foo);

    if (ffrpc_service.open(arg_helper))
    {
        return -1;
    }
    
    ffrpc_t ffrpc_client;
    if (ffrpc_client.open(arg_helper))
    {
        return -1;
    }
    
	thrift_client_t client;
    echo_thrift_in_t in;
    in.data = "Ohnice";

    ffrpc_client.call("echo", in, ffrpc_ops_t::gen_callback(&thrift_client_t::echo_callback, &client, 1, &ffrpc_client));


    //sleep(3);
    
    signal_helper_t::wait();
    ffrpc_client.close();
    ffrpc_service.close();
    return 0;
}

}


#endif

