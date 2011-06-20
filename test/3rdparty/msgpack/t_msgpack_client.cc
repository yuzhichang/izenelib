#include <iostream>
#include "echo_server.h"
#include <3rdparty/msgpack/rpc/client.h>
#include <3rdparty/msgpack/rpc/session_pool.h>
#include <3rdparty/msgpack/mp/functional.h>

using namespace mp::placeholders;

void simple_client()
{
    msgpack::rpc::client c("127.0.0.1", 9090);
    int result = c.call("add", 1, 2).get<int>();
    std::cout << result << std::endl;
}

void sync_call()
{
    // create client
    msgpack::rpc::client cli("127.0.0.1", 18811);

    // call
    std::string msg("MessagePack-RPC");
    A a;
    a.s = "hahahah";
    a.i = 99;
    A ret = cli.call("echo_a", a).get<A>();

    std::cout << "call: echo(\"MessagePack-RPC\") = " << ret << std::endl;
}

void async_call()
{
    // create session pool
    msgpack::rpc::session_pool sp;

    // get session
    msgpack::rpc::session s = sp.get_session("127.0.0.1", 18811);

    // async call
    msgpack::rpc::future fs[10];

    for(int i=0; i < 10; ++i) {
        fs[i] = s.call("add", 1, 2);
    }

    for(int i=0; i < 10; ++i) {
        int ret = fs[i].get<int>();
        std::cout << "async call: add(1, 2) = " << ret << std::endl;
    }
}

void add_callback(msgpack::rpc::future f, msgpack::rpc::loop lo)
{
    try {
        int result = f.get<int>();
        std::cout << "add_callback: add(1, 2) = " << result << std::endl;
    } catch (msgpack::rpc::remote_error& e) {
        std::cout << e.what() << std::endl;
        exit(1);
    }
    lo->end();
}

void callback()
{
    // create session pool
    msgpack::rpc::session_pool sp;

    // get session
    msgpack::rpc::session s = sp.get_session("127.0.0.1", 18811);

    // call
    msgpack::rpc::future f = s.call("add", 1, 2);


    f.attach_callback(
            mp::bind(add_callback, _1, sp.get_loop()) );

    sp.run(4);
}


void notify()
{
    // create client
    msgpack::rpc::client cli("127.0.0.1", 18811);

    // notify
    cli.notify("echo");
    cli.notify("echo", 0);
    cli.notify("echo", 0, 1);

    cli.get_loop()->flush();

    usleep(100000);
}

int main(void)
{
    //simple_client();

    sync_call();

    //async_call();

    //callback();

    //notify();
}
