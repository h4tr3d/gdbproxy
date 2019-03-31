#pragma once

#include <unordered_map>

#include "common.h"
#include "gdb-packet.h"

class connection : public std::enable_shared_from_this<connection> {
public:
    typedef std::shared_ptr<connection> pointer;

    static pointer create(asio::io_service& io_service) {
        return pointer(new connection(io_service));
    }

    ~connection() 
    {
        std::clog << "~connection\n";
    }

    asio::ip::tcp::socket& socket() {
        return m_client_socket;
    }

    /// Start read data of request from browser
    void start();

private:
    connection(asio::io_service& io_service);
    
    /// Start connecting to the web-server, initially to resolve the DNS-name of Web server into the IP address
    void start_connect();
    void handle_resolve(const std::error_code& err,
                        asio::ip::tcp::resolver::iterator endpoint_iterator);
    void handle_connect(const std::error_code& err,
                        asio::ip::tcp::resolver::iterator endpoint_iterator, const bool first_time);
    
    void start_client_read();
    void start_client_write();

    void start_remote_write();
    void start_remote_read();

    /// Close both sockets: for browser and web-server
    void shutdown();


    asio::io_service&     io_service_;
    asio::ip::tcp::socket m_client_socket;

    asio::ip::tcp::socket m_target_socket;
    asio::ip::tcp::resolver resolver_;

    bool proxy_closed;
    bool isPersistent;
    int32_t RespLen;
    int32_t RespReaded;

    std::array<char, 8192> bbuffer;
    std::array<char, 8192> sbuffer;

    std::array<char, 8192> target_send_buffer;
    std::array<char, 8192> client_recv_buffer;

    gdb_packet from_client;
    gdb_packet from_target;
    std::deque<gdb_packet> to_server;
    std::deque<gdb_packet> to_client;
    std::deque<gdb_packet> to_server_process;
    std::deque<gdb_packet> to_client_process;

    std::string fServer = "localhost";
    std::string fPort = "3002";
    
    bool isOpened = false;

    std::string fReq;

    typedef std::unordered_map<std::string,std::string> headersMap;
    headersMap reqHeaders, respHeaders;

    void parseHeaders(const std::string& h, headersMap& hm);
};

