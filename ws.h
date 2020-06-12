#pragma once

using namespace websocketpp;

typedef server<websocketpp::config::asio> ws;

//
struct Connection
{
	connection_hdl hdl;
	bool head_sended;
};

//
class WServer
{
	ws m_server;
	std::thread m_thread;
	std::list<Connection> m_connections;

	std::ofstream m_file;

public:
	//
	void on_open(connection_hdl hdl)
	{
		std::cout << "on open" << std::endl;
		m_connections.push_back({hdl, false});	
	}

	//
	void on_close(connection_hdl hdl)
	{
		m_connections.remove_if([hdl](Connection c) {
			std::shared_ptr<void> swp = hdl.lock();
			std::shared_ptr<void> sp = c.hdl.lock();
			if (swp && sp) 
			{
				return swp == sp;
			}
				
			return false;
		});
	}

	//
	void on_http(connection_hdl hdl)
	{
		auto con = m_server.get_con_from_hdl(hdl);

		std::ifstream file;
		std::string filename = con->get_resource();
		std::stringstream ss;
		
		if (filename == "/") {
			filename = "index.html";
		}
		else {
			filename = filename.substr(1);
		}

		file.open(filename.c_str(), std::ios::binary);
		if (file.is_open())
		{
			ss << file.rdbuf();
			con->set_body(ss.str());
			con->append_header("Content-Type", "text/html");
			con->set_status(websocketpp::http::status_code::ok);
		}
		else
		{
			ss << "<!doctype html><html><head>"
				<< "<title>Error 404 (Resource not found)</title><body>"
				<< "<h1>Error 404</h1>"
				<< "<p>The requested URL " << filename << " was not found on this server.</p>"
				<< "</body></head></html>";
			con->set_body(ss.str());
			con->set_status(websocketpp::http::status_code::not_found);
		}
	}

	//
	void send_head(void const * data, size_t len)
	{
		for (auto & connection : m_connections)
		{
			if (connection.head_sended) continue;
			auto con = m_server.get_con_from_hdl(connection.hdl);
			con->send(data, len);
			connection.head_sended = true;
		}
	}

	//
	void send_data(void const * data, size_t len)
	{
		for (auto & connection : m_connections)
		{
			if (connection.head_sended == false) continue;
			auto con = m_server.get_con_from_hdl(connection.hdl);
			con->send(data, len);
		}
	}

	//
	void run()
	{
		m_server.init_asio();

		m_server.set_open_handler([=](connection_hdl hdl) {
			std::cout << "on open 1" << std::endl;
			on_open(hdl);
		});

		m_server.set_close_handler([=](connection_hdl hdl) {
			on_close(hdl);
		});

		m_server.set_http_handler([=](connection_hdl hdl) {
			on_http(hdl);
		});

		m_server.listen(9000);
		m_server.start_accept();
		m_server.clear_access_channels(log::alevel::all);		

		m_server.run();
	}

	//
	void start()
	{
		m_thread = std::thread(&WServer::run, this);
	}

	//
	void stop()
	{
		 m_server.stop_listening();
		 m_server.stop();

		if (m_thread.joinable())
		{
			m_thread.join();
		}
	}	
};