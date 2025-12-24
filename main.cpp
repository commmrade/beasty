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
#include <boost/beast/http/message_fwd.hpp>
#include <boost/beast/http/parser.hpp>
#include <boost/beast/http/read.hpp>
#include <boost/beast/http/serializer.hpp>
#include <boost/beast/http/status.hpp>
#include <boost/beast/http/string_body_fwd.hpp>
#include <boost/beast/http/verb.hpp>
#include <boost/beast/http/write.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/system/detail/error_code.hpp>
#include <chrono>
#include <cstddef>
#include <nlohmann/detail/conversions/from_json.hpp>
#include <nlohmann/json_fwd.hpp>
#include <print>
#include <memory>
#include <type_traits>
#include <unordered_map>
#include <nlohmann/json.hpp>

using response = boost::beast::http::response<boost::beast::http::string_body>;
using request = boost::beast::http::request<boost::beast::http::string_body>;

class Bank {
    std::unordered_map<std::string, int> bank_accounts_;
public:
    response route(const request& req) {
        response result;
        if (req.target() == "/echo" && req.method() == boost::beast::http::verb::post) {
            result = route_echo(req);
        } else if (req.target() == "/add-money" && req.method() == boost::beast::http::verb::post) {
            result = route_add_money(req);
        } else {
            result.clear();
            result.body() = "";
            result.result(boost::beast::http::status::ok);
        }
        return result;
    }
private:
    response route_echo(const request& req) {
        auto body = req.body();
        nlohmann::json json = nlohmann::json::parse(body);
        auto message = json["message"];

        response resp;
        resp.body() = message;
        resp.result(boost::beast::http::status::ok);
        resp.prepare_payload();
        return resp;
    }

    response route_add_money(const request& req) {
        nlohmann::json json = nlohmann::json::parse(req.body());
        auto name = nlohmann::to_string(json["username"]);
        int value = std::stoi(nlohmann::to_string(json["value"]));
        bank_accounts_[name] += value;

        response resp;
        resp.body() = std::format("Balance of {} is {}$", name, bank_accounts_[name]);
        resp.prepare_payload();
        return resp;
    }
};

template<typename T>
concept Service = requires(T a, request req) {
    { a.route(req) } -> std::convertible_to<response>;
};



template<Service S>
class Session : public std::enable_shared_from_this<Session<S>> {
    boost::beast::tcp_stream stream_;
    boost::beast::flat_buffer buf_;
    request recv_msg_;
    response send_msg_;

    S& service_;
public:
    Session(boost::asio::ip::tcp::socket&& sock, S& service) : stream_(std::move(sock)), service_(service) {}

    void process() {
        stream_.expires_after(std::chrono::seconds(30)); // Drops connection after specified time
        // shared_from_this to keep object alive, since it is supposed to be dead at this point in Server::start_accept
        boost::beast::http::async_read(stream_, buf_, recv_msg_, std::bind(&Session::do_read, this->shared_from_this(), boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
    }

    void do_read(const boost::system::error_code& err, std::size_t bytes_tf) {
        if (!err) {
            send_msg_ = service_.route(recv_msg_);
            boost::beast::http::async_write(stream_, send_msg_, std::bind(&Session::do_write, this->shared_from_this(), boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
        } else {
            std::println("Read failed: {}", err.message());
        }
    }
    void do_write(const boost::system::error_code& err, std::size_t bytes_tf) {
        if (!err) {
            std::println("Written");
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

template<Service S>
class Server {
    boost::asio::io_context& io_;
    boost::asio::ip::tcp::acceptor acceptor_;

    S bank_;
public:
    Server(boost::asio::io_context& io, boost::asio::ip::address addr, unsigned short port) : io_(io), acceptor_(io, {addr, port}) {
        acceptor_.set_option(boost::asio::socket_base::reuse_address{true});
        start_accept();
    }

    void start_accept() {
        acceptor_.async_accept([this](const boost::system::error_code& err, boost::asio::ip::tcp::socket sock) {
            if (!err) {
                auto session = std::make_shared<Session<S>>(std::move(sock), bank_);
                session->process();
            }
            start_accept();
        });
    }
};


int main(int, char**){
    boost::asio::io_context io;
    Server<Bank> serv{io, boost::asio::ip::make_address("127.0.0.1"), 8000};
    io.run();
    return 0;
}
