#include "proxy-server.hpp"

server::server(const ios_deque &io_services, std::string_view target, int argc, char **argv, int port,
               std::string interface_address, std::string remote_host, int remote_port)
	: io_services_(io_services),
	  endpoint_(interface_address.empty()?	
				(asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port)): // INADDR_ANY for v4 (in6addr_any if the fix to v6)
				asio::ip::tcp::endpoint(asio::ip::address().from_string(interface_address), port) ),	// specified ip address
      acceptor_(*io_services.front(), endpoint_),	// By default set option to reuse the address (i.e. SO_REUSEADDR)
      m_target_name(target),
      m_remote_host(remote_host),
      m_remote_port(remote_port)
{
	std::cout << endpoint_.address().to_string() << ":" << endpoint_.port() << std::endl;
//	std::cout << "server::server" << std::endl;
	start_accept();
}

void server::start_accept() {
//	std::cout << "server::start_accept" << std::endl;
	// Round robin.
	io_services_.push_back(io_services_.front());
	io_services_.pop_front();
    connection::pointer new_connection = connection::create(*io_services_.front(), m_target_name,
                                                            m_remote_host, m_remote_port);

	acceptor_.async_accept(new_connection->socket(), [this, new_connection](const std::error_code& error) {
		handle_accept(new_connection, error);
	});
}

void server::handle_accept(connection::pointer new_connection, const std::error_code& error) {
//	std::cout << "server::handle_accept" << std::endl;
	std::clog << "new connection established\n";
	if (!error) {
		new_connection->start();
		start_accept();
	}
}
