#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/placeholders.hpp>
#include <boost/asio/socket_base.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/http/buffer_body.hpp>
#include <boost/beast/http/dynamic_body_fwd.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/dynamic_body.hpp>
#include <boost/beast/http/parser.hpp>
#include <boost/beast/http/read.hpp>
#include <boost/beast/http/serializer.hpp>
#include <boost/beast/http/string_body_fwd.hpp>
#include <boost/beast/http/write.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/system/detail/error_code.hpp>
#include <chrono>
#include <cstddef>
#include <print>
#include <memory>

boost::beast::http::response<boost::beast::http::string_body> process_request(const boost::beast::http::request<boost::beast::http::string_body>& msg) {
    boost::beast::http::message<false, boost::beast::http::string_body> send_msg;
    if (msg.target().starts_with("/something")) {
        send_msg.body() = "Something";
    } else {
        send_msg.body() = "Other";
    }
    // send_msg.keep_alive(msg.keep_alive());
    send_msg.result(boost::beast::http::status::ok);
    send_msg.prepare_payload(); // This is called if a message contains body
    return send_msg;
}

struct Session : public std::enable_shared_from_this<Session> {
    boost::beast::tcp_stream stream_;
    boost::beast::flat_buffer buf_;
    boost::beast::http::request<boost::beast::http::string_body> recv_msg_;
    boost::beast::http::response<boost::beast::http::string_body> send_msg_;

    Session(boost::asio::ip::tcp::socket&& sock) : stream_(std::move(sock)) {}

    void process() {
        stream_.expires_after(std::chrono::seconds(5)); // Drops connection after specified time
        // shared_from_this to keep object alive, since it is supposed to be dead at this point in Server::start_accept
        boost::beast::http::async_read(stream_, buf_, recv_msg_, std::bind(&Session::do_read, shared_from_this(), boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
    }

    void do_read(const boost::system::error_code& err, std::size_t bytes_tf) {
        if (!err) {
            send_msg_ = process_request(recv_msg_);
            boost::beast::http::async_write(stream_, send_msg_, std::bind(&Session::do_write, shared_from_this(), boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
        } else {
            std::println("Read failed: {}", err.message());
        }
    }
    void do_write(const boost::system::error_code& err, std::size_t bytes_tf) {
        if (!err) {
            if (!recv_msg_.keep_alive()) {
                do_close();
                return;
            }
            send_msg_ = {};
            recv_msg_ = {};
            process();
        } else {
            std::println("Write failed: {}", err.message());
        }
    }
    void do_close() {
        // Graceful shutdown
        stream_.socket().shutdown(boost::asio::socket_base::shutdown_both);
    }
};

class Server {
    boost::asio::io_context& io_;
    boost::asio::ip::tcp::acceptor acceptor_;
public:
    Server(boost::asio::io_context& io, boost::asio::ip::address addr, unsigned short port) : io_(io), acceptor_(io, {addr, port}) {
        acceptor_.set_option(boost::asio::socket_base::reuse_address{});
        start_accept();
    }

    void start_accept() {
        acceptor_.async_accept([this](const boost::system::error_code& err, boost::asio::ip::tcp::socket sock) {
            if (!err) {
                auto session = std::make_shared<Session>(std::move(sock));
                session->process();
            }
            start_accept();
        });
    }
};


int main(int, char**){
    boost::asio::io_context io;
    Server serv{io, boost::asio::ip::make_address("127.0.0.1"), 8080};
    io.run();
    return 0;
}
