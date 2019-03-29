#include <vector>
#include <functional>

#include "proxy-server.hpp"

int main(int argc, char** argv) {
	try {
		int thread_num = 1, port = 4002;
		std::string interface_address = "127.0.0.1";

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
		server server(io_services, port, interface_address);
		
		for (auto & thr : workers) {
			if (thr.joinable())
				thr.join();
		}

	} catch (std::exception& e) {
		std::cerr << e.what() << std::endl;
	}


	return 0;
}

