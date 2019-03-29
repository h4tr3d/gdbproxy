#pragma once

#include <asio.hpp>

#include <memory>

#include <thread>
#include <functional>
#include <regex>
#include <iostream>
#include <string>

typedef std::shared_ptr<asio::ip::tcp::socket> socket_ptr;
typedef std::shared_ptr<asio::io_service>      io_service_ptr;


