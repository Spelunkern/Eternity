#pragma once
#include "eternity/net/framing.hpp"
#include <asio.hpp>
#include <chrono>
#include <thread>

using namespace std::chrono_literals;
using asio::ip::tcp;
using eternity::net::Packet;

struct PeerSocket {
    asio::io_context io;
    tcp::socket socket{io};
    explicit PeerSocket(std::uint16_t port) {
        socket.connect({asio::ip::make_address("127.0.0.1"),port});
        socket.non_blocking(true);
    }
    void write(std::span<const std::uint8_t> bytes) {
        const auto deadline=std::chrono::steady_clock::now()+3s;
        while (!bytes.empty()) {
            asio::error_code error;
            const auto count=socket.write_some(asio::buffer(bytes.data(),bytes.size()),error);
            if (error && error!=asio::error::would_block && error!=asio::error::try_again) throw asio::system_error(error);
            bytes=bytes.subspan(count);
            if (std::chrono::steady_clock::now()>=deadline) throw std::runtime_error("Timed out writing login packets");
            if (!count) std::this_thread::sleep_for(1ms);
        }
    }
    void read(std::span<std::uint8_t> bytes) {
        const auto deadline=std::chrono::steady_clock::now()+3s;
        while (!bytes.empty()) {
            asio::error_code error;
            const auto count=socket.read_some(asio::buffer(bytes.data(),bytes.size()),error);
            if (error && error!=asio::error::would_block && error!=asio::error::try_again) throw asio::system_error(error);
            bytes=bytes.subspan(count);
            if (std::chrono::steady_clock::now()>=deadline) throw std::runtime_error("Timed out reading login packets");
            if (!count) std::this_thread::sleep_for(1ms);
        }
    }
    Packet packet() {
        std::array<std::uint8_t,2> header{}; read(header);
        const auto size=static_cast<std::size_t>(header[0])+(static_cast<std::size_t>(header[1])<<8);
        if(size<2 || size>2048) throw std::runtime_error("Invalid server frame length");
        Packet payload(size-2); read(payload); return payload;
    }
    void closed() {
        const auto deadline=std::chrono::steady_clock::now()+3s;
        for (;;) {
            std::uint8_t byte{};
            asio::error_code error;
            const auto count=socket.read_some(asio::buffer(&byte,1),error);
            if(error==asio::error::eof || error==asio::error::connection_reset) return;
            if(count) throw std::runtime_error("Unexpected packet while awaiting disconnect");
            if(error && error!=asio::error::would_block && error!=asio::error::try_again) throw asio::system_error(error);
            if(std::chrono::steady_clock::now()>=deadline) throw std::runtime_error("Login connection was not closed");
            std::this_thread::sleep_for(1ms);
        }
    }
};
