#include <iostream>
using namespace std;
#include <asio.hpp>
#include <asio/ts/buffer.hpp>
#include <asio/ts/internet.hpp>
#include "exprtk.hpp"
#include <memory>
#include <utility>
#include <functional>
#include <string>
#include <fstream>
#include <time.h>
#include <stdlib.h>

#ifdef _WIN32
#pragma warning(disable:4996)
#endif

ofstream logger;

#define divade "\n------------------------------------------------\n"
inline
void log(const string&text)
{
    auto t=time(0);
    const char*const time=asctime(localtime(&t));
    logger  <<divade<<time<<'['<<text<<']'<<divade;
    cout    <<divade<<time<<'['<<text<<']'<<divade;
}

exprtk::expression<double>expression;
exprtk::parser<double>parser;

///////////////////////////////////////////////////////////////////////////

class session:public enable_shared_from_this<session>
{
    asio::streambuf storage;
    asio::ip::tcp::socket client;

#define client_addr client.remote_endpoint().address().to_string()

    void read()
    {
        log("Server wait for recieve expression from "+client_addr);

        shared_ptr<session>self(shared_from_this());

        asio::async_read_until(client,storage,'\0',
        [this,self](asio::error_code e,std::size_t length)
        {
            if(e){log("Error on recieving expression:"+e.message());return;}

            const string message((const char*)storage.data().data(),length-1);

            log("Recieve expression "+message+" from "+client_addr);

            parser.compile(message,expression);
            auto r=new double(expression.value());
            cout<<message<<'='<<(*r)<<endl;

            log("Calculation done with resault:"+to_string(*r)+" for "+client_addr);

            write(r);
        });
    }
    
    void write(double*n)
    {
        log("Server try to send resault "+to_string(*n)+" on client "+client_addr);

        auto self(shared_from_this());
        asio::async_write(client,asio::buffer(n,8),
        [this,self,n](asio::error_code e,std::size_t)
        {
            if(e){log("Error on sending expression:"+e.message());return;}
            else
            {
                log("Success sending resault "+to_string(*n)+" on client "+client_addr);
                client.close(e);
                
                if(e)log("Error on closing connection:"+e.message());
                else log("Successful closing connection and finish job with"+client_addr);
            }
            
            delete n;
        });
    }
public:
    session(asio::ip::tcp::socket client)
        :client(std::move(client))
    {}
    
    inline
    void start()
    {
        log("New session for "+client.remote_endpoint().address().to_string());
        read();
    }
};

///////////////////////////////////////////////////////////////////////////

class server
{
    asio::ip::tcp::acceptor listener;
    
    void acception()
    {
        log("Server is listening");

        listener.async_accept(
        [this](asio::error_code e,asio::ip::tcp::socket client)
        {
            log(client.remote_endpoint().address().to_string()+" client want for connection");

            if(e)log("Error on acception:"+e.message());
            else
            {
                //cout<<client.remote_endpoint()<<" was connected\n";
                std::make_shared<session>(std::move(client))->start();
            }

            acception();
        });
    }
public:
    server(asio::io_context&context,const asio::ip::tcp::endpoint&address)
        :listener(context,address)
    {acception();}
};

///////////////////////////////////////////////////////////////////////////

int main(int argc,char*argv[])
{
    const char*ip="127.0.0.1";
    unsigned short port=1111;
    const char*log_path="server_log.txt";

    if(argc>1)ip=argv[1];
    if(argc>2)port=atoi(argv[2]);
    if(argc>3)log_path=argv[3];

    logger.open(log_path,ios_base::app|ios_base::end);
    if(!logger){cout<<"logger error";return 1;}
    log("Logger starting");

    try
    {
        asio::io_context context;
        server s
        (
            context,
            asio::ip::tcp::endpoint(asio::ip::make_address(ip),port)
        );
        context.run();
    }
    catch(exception&e)
    {
        log("Exception: "+string(e.what()));
        return 0;
    }

    log("Logger stoping");
    logger.close();
    return 0;
}