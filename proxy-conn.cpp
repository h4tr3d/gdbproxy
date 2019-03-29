#include "proxy-conn.hpp"

/** 
 * 
 * 
 * @param io_service 
 */
connection::connection(asio::io_service& io_service) : io_service_(io_service),
                           bsocket_(io_service),
                           ssocket_(io_service),
                           resolver_(io_service),
                           proxy_closed(false),
                           isPersistent(false),
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
        start_remote_read();

    } else if (endpoint_iterator != asio::ip::tcp::resolver::iterator()) {
        ssocket_.close();
        asio::ip::tcp::endpoint endpoint = *endpoint_iterator;
        std::clog << "endpoint: " << endpoint << std::endl;
        auto conn = shared_from_this();
        ++endpoint_iterator;
        ssocket_.async_connect(endpoint, [this, conn, endpoint_iterator](const std::error_code& ec) {
            handle_connect(ec, endpoint_iterator, false);
        });
    } else {
        shutdown();
    }
}

void connection::start_client_read()
{
    auto conn = shared_from_this();
    bbuffer.fill('\0');
    asio::async_read(bsocket_, asio::buffer(bbuffer), asio::transfer_at_least(1),
                     [conn,this](const std::error_code& ec, size_t readed) {
                        // client read done
                        std::clog << "<- "   << bbuffer.data() << std::endl;
                        if (!ec && readed) 
                            start_remote_write();
                        else
                            shutdown();

                    });

}

void connection::start_remote_write()
{
    auto conn = shared_from_this();
    asio::async_write(ssocket_, asio::buffer(bbuffer),
                      [conn,this](const std::error_code& ec, size_t transfered) {
                          // remote write done
                          if (!ec)
                            start_client_read();
                          else
                            shutdown();
                      });
}

void connection::start_remote_read()
{
    auto conn = shared_from_this();
    sbuffer.fill('\0');
    asio::async_read(ssocket_, asio::buffer(sbuffer), asio::transfer_at_least(1),
                     [conn, this](const std::error_code& ec, size_t readed) {
                        // remote read done
                        std::clog << "-> "   << sbuffer.data() << std::endl;
                        if (!ec && readed)
                            start_client_write();
                        else
                            shutdown();
                    });
}

void connection::start_client_write()
{
    auto conn = shared_from_this();
    asio::async_write(bsocket_, asio::buffer(sbuffer),
                      [conn, this](const std::error_code& ec, size_t transfered) {
                            // remote write done
                            if (!ec)
                                start_remote_read();
                            else
                                shutdown();
                      });
}

/** 
 * Close both sockets: for browser and web-server
 * 
 */
void connection::shutdown() 
{
  ssocket_.close();
  bsocket_.close();
}
