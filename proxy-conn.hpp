#pragma once

#include <memory>

#include "common.h"
#include "gdb-packet.h"
#include "target.h"

template<size_t capacity>
struct data_buffer
{
    std::array<char, capacity> data;
    size_t size = 0;
    size_t consumed = 0;

    void reset() 
    {
        size = 0;
        consumed = 0;
    }

    bool empty() const
    {
        return size == consumed;
    }
};

// Only Data packets
struct transfer
{
    struct internal {};
    using on_response_cb = std::function<void(const gdb_packet& request, const gdb_packet& response)>;

    transfer(gdb_packet&& pkt, on_response_cb&& cb = on_response_cb())
        : pkt(std::move(pkt)),
          on_response(std::move(cb))
    {}

    transfer(gdb_packet&& pkt, internal, on_response_cb&& cb = on_response_cb())
        : pkt(std::move(pkt)),
          is_internal(true),
          on_response(std::move(cb))
    {}

    gdb_packet pkt;
    bool const is_internal = false;
    on_response_cb on_response;
};


class connection : public std::enable_shared_from_this<connection> {
public:
    typedef std::shared_ptr<connection> pointer;
    friend class channel;

    static pointer
    create(asio::io_service &io_service, std::string_view target, std::string_view remote_host, int remote_port) {
        return pointer(new connection(io_service, target, remote_host, remote_port));
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

    void push_internal_request(gdb_packet&& req, transfer::on_response_cb cb = transfer::on_response_cb());
    void push_request(gdb_packet&& req, transfer::on_response_cb cb = transfer::on_response_cb());

    void push_internal_response(gdb_packet&& req, transfer::on_response_cb cb = transfer::on_response_cb());

private:
    struct channel
    {
    public:
        using on_packet_handler = std::function<bool(const gdb_packet& packet)>;

        enum direction {
            requests,
            responses,
        };

        channel(asio::io_service& io,
                asio::ip::tcp::socket& src,
                asio::ip::tcp::socket& dst,
                direction dir);

        void start_read(std::shared_ptr<connection> con);
        void start_write(std::shared_ptr<connection> con, transfer&& req);

        void set_packet_handler(on_packet_handler handler);

        std::deque<transfer>& transfers_queue();

    private:
        void process_data(std::shared_ptr<connection> con);

    private:
        asio::io_service& m_io;

        asio::ip::tcp::socket& m_src;
        asio::ip::tcp::socket& m_dst;

        data_buffer<8192> m_buffer;
        gdb_packet        m_packet;

        std::deque<transfer> m_transfers;

        on_packet_handler m_handler;

        direction m_dir = requests;
    };

    connection(asio::io_service &io_service, std::string_view target, std::string_view remote_host, int remote_port);
    
    /// Start connecting to the web-server, initially to resolve the DNS-name of Web server into the IP address
    void start_connect();
    void handle_resolve(const std::error_code& err,
                        asio::ip::tcp::resolver::iterator endpoint_iterator);
    void handle_connect(const std::error_code& err,
                        asio::ip::tcp::resolver::iterator endpoint_iterator, const bool first_time);

    /// Close both sockets: for browser and web-server
    void shutdown();

    bool on_request(const gdb_packet& pkt);
    bool on_response(const gdb_packet& pkt);

    asio::io_service&     io_service_;
    asio::ip::tcp::socket m_client_socket;
    asio::ip::tcp::socket m_target_socket;
    asio::ip::tcp::resolver resolver_;

    channel m_requests_channel;
    channel m_responses_channel;

    std::string fServer = "localhost";
    int fPort = 3002;
    
    size_t seq = 0;

    bool m_ack_mode = true;

    std::unique_ptr<target> m_target;
};

