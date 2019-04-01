#include "proxy-conn.hpp"

//#define SYNC_TRANSFER

/** 
 * 
 * 
 * @param io_service 
 */
connection::connection(asio::io_service& io_service) 
    : io_service_(io_service),
      m_client_socket(io_service),
      m_target_socket(io_service),
      resolver_(io_service),
      m_requests_channel(io_service, m_client_socket, m_target_socket, channel::requests),
      m_responses_channel(io_service, m_target_socket, m_client_socket, channel::responses)
{

    m_requests_channel.set_packet_handler([this](const gdb_packet& pkt) {
        return on_request(pkt);
    });

    m_responses_channel.set_packet_handler([this](const gdb_packet& pkt) {
        return on_response(pkt);
    });
}

/** 
 * Start read data of request from browser
 * 
 */
void connection::start() 
{
    start_connect();
}

/** 
 * Start connecting to the web-server, initially to resolve the DNS-name of web-server into the IP address
 * 
 */
void connection::start_connect() 
{
    asio::ip::tcp::resolver::query query(fServer, fPort);
    auto conn = shared_from_this();
    resolver_.async_resolve(query, [conn](const std::error_code& err, asio::ip::tcp::resolver::iterator endpoint_iterator) {
        std::clog << "resolve request handled\n";
        conn->handle_resolve(err, endpoint_iterator);
    });
}

/** 
 * If successful, after the resolved DNS-names of web-server into the IP addresses, try to connect
 * 
 * @param err 
 * @param endpoint_iterator 
 */
void connection::handle_resolve(const std::error_code& err,
                asio::ip::tcp::resolver::iterator endpoint_iterator) {
    //	std::cout << "handle_resolve. Error: " << err.message() << "\n";
    if (!err) {
        const bool first_time = true;
        handle_connect(std::error_code(), endpoint_iterator, first_time);
    } else {
        shutdown();
    }
}

/** 
 * Try to connect to the web-server
 * 
 * @param err 
 * @param endpoint_iterator 
 */
void connection::handle_connect(const std::error_code& err,
                asio::ip::tcp::resolver::iterator endpoint_iterator, const bool first_time) {
    std::clog << "handle_connect. Error: " << err << ", " << err.message() << ", first=" << first_time << "\n";
    if (!err && !first_time) {

        // arm channels
        m_requests_channel.start_read(shared_from_this());
        m_responses_channel.start_read(shared_from_this());

    } else if (endpoint_iterator != asio::ip::tcp::resolver::iterator()) {
        m_target_socket.close();
        asio::ip::tcp::endpoint endpoint = *endpoint_iterator;
        std::clog << "endpoint: " << endpoint << std::endl;
        auto conn = shared_from_this();
        ++endpoint_iterator;
        m_target_socket.async_connect(endpoint, [this, conn, endpoint_iterator](const std::error_code& ec) {
            handle_connect(ec, endpoint_iterator, false);
        });
    } else {
        shutdown();
    }
}

/** 
 * Close both sockets: for browser and web-server
 * 
 */
void connection::shutdown() 
{
  m_target_socket.close();
  m_client_socket.close();
}

//
// TBD: rework
//

bool connection::on_request(const gdb_packet& pkt)
{
    if (pkt.type() == gdb_packet_type::generic) {
        std::clog << "req: " << ++seq << std::endl;
    }

    return false;
}

bool connection::on_response(const gdb_packet& pkt)
{
    if (pkt.type() == gdb_packet_type::generic) {
        std::clog << "rsp: " << seq << std::endl;

        auto &queue = m_requests_channel.transfers_queue();
        auto const& req = queue.front();
        auto const& req_pkt = req.pkt;

        std::clog << "rsp to the req: " << req_pkt.raw_data() << std::endl;
        queue.pop_front();
    }
    return false;
}

//
// Channel
//

connection::channel::channel(asio::io_service &io, asio::ip::tcp::socket &src, asio::ip::tcp::socket &dst, direction dir)
    : m_io(io), m_src(src), m_dst(dst), m_dir(dir)
{}

void connection::channel::start_read(std::shared_ptr<connection> con)
{
    m_buffer.reset();
    asio::async_read(m_src, asio::buffer(m_buffer.data), asio::transfer_at_least(1),
                     [con,this](const std::error_code& ec, size_t readed) {
                        // client read done

                        m_buffer.data[readed] = 0x00;
                        m_buffer.size = readed;
                        m_buffer.consumed = 0;

                        std::clog << (m_dir == requests ? "<- " : "-> ") << std::string_view(m_buffer.data.data(), readed) << std::endl;

                        if (!ec && readed) {
                            process_data(con);
                        } else {
                            con->shutdown();
                        }
                    });
}

void connection::channel::start_write(std::shared_ptr<connection> con, transfer &&req)
{
    m_transfers.push_back(std::move(req));

    auto const& process_req = m_transfers.back();
    auto const& pkt = process_req.pkt;
    auto const total = pkt.raw_data().size();

    asio::async_write(m_dst, asio::buffer(pkt.raw_data()),
                      [con,this,total,&process_req](const std::error_code& ec, size_t transfered) {
                        std::clog << "transfered to " << (m_dir == requests ? "remote" : "client") << ": " << transfered << " / " << total << std::endl;
                        auto const& pkt = process_req.pkt;
                        // remote write done
                        assert(transfered == total && "Unhandled case, when server receive less data than was sent");
                        if (!ec) {
                            // Data packets must be cleaned at the Response handler
                            if (pkt.type() != gdb_packet_type::generic || m_dir == responses || !m_handler)
                                m_transfers.pop_front();
                          } else {
                            con->shutdown();
                          }
    });
}

void connection::channel::set_packet_handler(connection::channel::on_packet_handler handler)
{
    m_handler = std::move(handler);
}

std::deque<transfer> &connection::channel::transfers_queue()
{
    return m_transfers;
}

void connection::channel::process_data(std::shared_ptr<connection> con)
{
    do {
        // process packets
        auto const consumed = m_packet.parse(
            m_buffer.data.data() + m_buffer.consumed,
            m_buffer.size - m_buffer.consumed
        );
        m_buffer.consumed += consumed;

        assert(consumed && "Invalid packet data?");
        if (consumed == 0) {
            // Invalid packet data?
        }

        if (m_packet.is_complete()) {
            // TBD: handle complete handle, process it and if needed - sent to the target
            auto const handled = m_handler ? m_handler(m_packet) : false;
            if (!handled) {
                start_write(con, std::move(m_packet));
            }
            m_packet.reset();
        }
    } while (!m_buffer.empty());

    // Processed all client data, start client reading again
    start_read(con);
}
