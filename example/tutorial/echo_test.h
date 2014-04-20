#ifndef _FF_ECHO_TEST_H_
#define _FF_ECHO_TEST_H_

namespace ff
{

//! 定义echo 接口的消息， in_t代表输入消息，out_t代表的结果消息
//! 提醒大家的是，这里没有为echo_t定义神马cmd，也没有制定其名称，ffmsg_t会自动能够获取echo_t的名称
struct echo_t
{
    struct in_t: public ffmsg_t<in_t>
    {
        void encode()
        {
            encoder() << data;
        }
        void decode()
        {
            decoder() >> data;
        }
        string data;
    };
    struct out_t: public ffmsg_t<out_t>
    {
        void encode()
        {
            encoder() << data;
        }
        void decode()
        {
            decoder() >> data;
        }
        string data;
    };
};

struct echo_service_t
{
    //! echo接口，返回请求的发送的消息ffreq_t可以提供两个模板参数，第一个表示输入的消息（请求者发送的）
    //! 第二个模板参数表示该接口要返回的结果消息类型
    void echo(ffreq_t<echo_t::in_t, echo_t::out_t>& req_)
    {
        echo_t::out_t out;
        out.data = req_.msg.data;
        LOGINFO(("XX", "foo_t::echo: recv %s", req_.msg.data.c_str()));

        req_.response(out);
    }
};
struct echo_client_t
{
    //! 远程调用接口，可以指定回调函数（也可以留空），同样使用ffreq_t指定输入消息类型，并且可以使用lambda绑定参数
    void echo_callback(ffreq_t<echo_t::out_t>& req_, int index, ffrpc_t* ffrpc_client)
    {
        if (req_.error())
        {
            LOGERROR(("XX", "error_msg <%s>", req_.error_msg()));
            return;
        }
        else if (index < 10)
        {
            echo_t::in_t in;
            in.data = "helloworld";
            LOGINFO(("XX", "%s %s index=%d callback...", __FUNCTION__, req_.msg.data.c_str(), index));
            sleep(1);
            ffrpc_client->call("echo", in, ffrpc_ops_t::gen_callback(&echo_client_t::echo_callback, this, ++index, ffrpc_client));
        }
        else
        {
            LOGINFO(("XX", "%s %s %d callback end", __FUNCTION__, req_.msg.data.c_str(), index));
        }
    }
};

static  int run_echo_test(arg_helper_t& arg_helper)
{
    echo_service_t foo;
    //! broker客户端，可以注册到broker，并注册服务以及接口，也可以远程调用其他节点的接口
    ffrpc_t ffrpc_service("echo");
    ffrpc_service.reg(&echo_service_t::echo, &foo);

    if (ffrpc_service.open(arg_helper))
    {
        return -1;
    }
    
    ffrpc_t ffrpc_client;
    if (ffrpc_client.open(arg_helper))
    {
        return -1;
    }
    
    echo_t::in_t in;
    in.data = "helloworld";
	echo_client_t client;
    ffrpc_client.call("echo", in, ffrpc_ops_t::gen_callback(&echo_client_t::echo_callback, &client, 1, &ffrpc_client));


    signal_helper_t::wait();
    ffrpc_client.close();
    ffrpc_service.close();
    return 0;
}

}

#endif

