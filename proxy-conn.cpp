#include "proxy-conn.hpp"

#define SYNC_TRANSFER

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
      isOpened(false) 
{
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
    if(!isOpened) {
        asio::ip::tcp::resolver::query query(fServer, fPort);
        auto conn = shared_from_this();
        resolver_.async_resolve(query, [conn](const std::error_code& err, asio::ip::tcp::resolver::iterator endpoint_iterator) {
            std::clog << "resolve request handled\n";
            conn->handle_resolve(err, endpoint_iterator);
        });
    } else {
        //start_write_to_server();
    }
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
        isOpened = true;

        start_client_read();
#ifndef SYNC_TRANSFER
        start_remote_read();
#endif

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

void connection::start_client_read()
{
    client_buffer.reset();
    auto conn = shared_from_this();
    asio::async_read(m_client_socket, asio::buffer(client_buffer.data), asio::transfer_at_least(1),
                     [conn,this](const std::error_code& ec, size_t readed) {
                        // client read done
                        
                        client_buffer.data[readed] = 0x00;
                        client_buffer.size = readed;
                        client_buffer.consumed = 0;

                        std::clog << "<- " << std::string_view(client_buffer.data.data(), readed) << std::endl;

                        if (!ec && readed) {
                            process_client_data();
                        } else {
                            shutdown();
                        }

                    });
}

void connection::process_client_data()
{
    // process packets
    auto const consumed = from_client.push_back(
        client_buffer.data.data() + client_buffer.consumed,
        client_buffer.size - client_buffer.consumed
    );
    client_buffer.consumed += consumed;

    assert(consumed && "Invalid packet data?");
    if (consumed == 0) {
        // Invalid packet data?
    }

    if (from_client.is_complete()) {
        // TBD: handle complete handle, process it and if needed - sent to the target

        // Send packet to target
        start_remote_write();
    } else if (client_buffer.empty()) {
        // Request data from client
        start_client_read();
    }
}

void connection::start_remote_write()
{
    auto conn = shared_from_this();
    auto const total = from_client.raw_data().size();
    asio::async_write(m_target_socket, asio::buffer(from_client.raw_data()),
                      [conn,this,total](const std::error_code& ec, size_t transfered) {
                          std::clog << "transfered to target: " << transfered << " / " << total << std::endl;
                          // remote write done
                          assert(transfered == total && "Unhandled case, when server receive less data than was sent");
                          if (!ec) {
                              const auto is_ack = from_client.type() == gdb_packet_type::ack;
                            from_client.reset();
                            if (client_buffer.empty()) {
#ifndef SYNC_TRANSFER
                                start_client_read();
#else
                                // always start read from client too: vCont does not receive response
                                // but client can send Break
                                start_client_read();
                                if (!is_ack)
                                    start_remote_read();
#endif
                            } else {
                                process_client_data();
                            }
                          } else {
                            shutdown();
                          }
                      });
}

void connection::start_remote_read()
{
    remote_buffer.reset();
    auto conn = shared_from_this();
    asio::async_read(m_target_socket, asio::buffer(remote_buffer.data), asio::transfer_at_least(1),
                     [conn,this](const std::error_code& ec, size_t readed) {
                        // client read done
                        
                        remote_buffer.data[readed] = 0x00;
                        remote_buffer.size = readed;
                        remote_buffer.consumed = 0;

                        std::clog << "-> " << std::string_view(remote_buffer.data.data(), readed) << std::endl;
                        if (!ec && readed) {
                            process_remote_data();
                        } else {
                            shutdown();
                        }
                    });
}

void connection::process_remote_data()
{
    // process packets
    auto const consumed = from_remote.push_back(
        remote_buffer.data.data() + remote_buffer.consumed,
        remote_buffer.size - remote_buffer.consumed
    );
    remote_buffer.consumed += consumed;

    assert(consumed && "Invalid packet data?");
    if (consumed == 0) {
        // Invalid packet data?
    }

    if (from_remote.is_complete()) {
        // TBD: handle response packet, process it and if needed - sent to the target
        // Send packet to target
        start_client_write();
    } else if (remote_buffer.empty()) {
        // Request data from client
        start_remote_read();
    }
}

void connection::start_client_write()
{
    auto conn = shared_from_this();
    auto const total = from_remote.raw_data().size();
    asio::async_write(m_client_socket, asio::buffer(from_remote.raw_data()),
                      [conn,this,total](const std::error_code& ec, size_t transfered) {
                          std::clog << "transfered to client: " << transfered << " / " << total << std::endl;
                          // remote write done
                          assert(transfered == total && "Unhandled case, when server receive less data than was sent");
                          if (!ec) {
                              const auto is_ack = from_remote.type() == gdb_packet_type::ack;
                            from_remote.reset();
                            if (remote_buffer.empty()) {
#ifndef SYNC_TRANSFER
                                start_remote_read();
#else
                                if (is_ack)
                                    start_remote_read();
                                else
                                    start_client_read();
#endif
                            } else {
                                process_remote_data();
                            }
                          } else {
                            shutdown();
                          }
                      });

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
