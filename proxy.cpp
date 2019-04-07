#include <vector>
#include <functional>
#include <iostream>

#include "proxy-server.hpp"

#include "cxxopts.hpp"

using namespace std;

int main(int argc, char** argv)
{
    cxxopts::Options options("gdbproxy", "GDB TCP protocol proxy to inject/hooks GDB client requests to the target");
    options.custom_help("[options...] [--] target [target-options...]");
    options.add_options()
            ("h,help",    "Help screen")
            ("port",      "Port for GDB client connections", cxxopts::value<int>()->default_value("4002"))
            ("interface", "Listen interface address", cxxopts::value<std::string>()->default_value("127.0.0.1"))
            ("remote-host", "Remote GDB-server host", cxxopts::value<std::string>()->default_value("localhost"))
            ("remote-port", "Remote GDB-server port", cxxopts::value<int>()->default_value("3002"))
            ;

    auto result = options.parse(argc, argv);

    if (result.count("help") || argc < 2) {
        cout << options.help();
        return 0;
    }

	try {
		int thread_num = 1;
		int port = result["port"].as<int>();
		string interface_address = result["interface"].as<string>();

		int remote_port = result["remote-port"].as<int>();
		string remote_host = result["remote-host"].as<string>();

		string target = argv[1];
		// now target name is program name
		argc--;
		argv++;

		ios_deque io_services;
		std::deque<asio::io_service::work> io_service_work;
		
		std::vector<std::thread> workers;
		workers.reserve(thread_num);
		
		for (int i = 0; i < thread_num; ++i) {
			io_service_ptr ios(new asio::io_service);
			io_services.push_back(ios);
			io_service_work.push_back(asio::io_service::work(*ios));
			workers.emplace_back([ios]() {
				ios->run();
			});
		}
        server server(io_services, target, 0, nullptr, port, interface_address, remote_host, remote_port);
		
		for (auto & thr : workers) {
			if (thr.joinable())
				thr.join();
		}

	} catch (std::exception& e) {
		std::cerr << e.what() << std::endl;
	}


	return 0;
}

