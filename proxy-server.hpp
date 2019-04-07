#pragma once

#include "common.h"
#include "proxy-conn.hpp"

#include <deque>

typedef std::deque<io_service_ptr> ios_deque;

class server {
public:
    server(const ios_deque &io_services, std::string_view target, int argc, char **argv, int port,
           std::string interface_address, std::string remote_host, int remote_port);

private:
	void start_accept();
	void handle_accept(connection::pointer new_connection, const std::error_code& error);
	
	ios_deque io_services_;
	const asio::ip::tcp::endpoint endpoint_;   /**< object, that points to the connection endpoint */
	asio::ip::tcp::acceptor acceptor_;         /**< object, that accepts new connections */

    std::string m_target_name;
    std::string m_remote_host;
    int m_remote_port;

    // non-owned list of target options
    std::vector<char *> m_target_options;
};

