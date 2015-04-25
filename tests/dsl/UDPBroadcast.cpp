/*
 * Copyright (C) 2013 Trent Houliston <trent@houliston.me>, Jake Woods <jake.f.woods@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
 * WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <catch.hpp>

#include "nuclear"

namespace {
    
    constexpr unsigned short port = 40001;
    const std::string testString = "Hello UDP Broadcast World!";
    
    struct Message {
    };
    
    class TestReactor : public NUClear::Reactor {
    public:
        TestReactor(std::unique_ptr<NUClear::Environment> environment) : Reactor(std::move(environment)) {
            
            on<UDP::Broadcast>(port).then([this](const UDP::Packet& packet) {
                
                // Check that the data we received is correct
                REQUIRE(packet.data.size() == testString.size());
                REQUIRE(std::memcmp(packet.data.data(), testString.data(), testString.size()) == 0);
                
                // Shutdown we are done with the test
                powerplant.shutdown();
            });
            
            on<Trigger<Message>>().then([this] {
                
                // Get all the network interfaces
                auto interfaces = NUClear::util::network::get_interfaces();
                
                std::vector<uint32_t> addresses;
                
                for(auto& iface : interfaces) {
                    // We send on broadcast addresses and we don't want loopback or point to point
                    if(!iface.flags.loopback && !iface.flags.pointtopoint && iface.flags.broadcast) {
                        // Two broadcast ips that are the same are probably on the same network so ignore those
                        if(std::find(std::begin(addresses), std::end(addresses), iface.broadcast) == std::end(addresses)) {
                            addresses.push_back(iface.broadcast);
                        }
                    }
                }
                
                for(auto& ad : addresses) {
                    
                    // Open a random socket
                    int fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
                    
                    // Set our socket to broadcast
                    int broadcast = 1;
                    REQUIRE(setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)) >= 0);
                    
                    sockaddr_in address;
                    address.sin_family = AF_INET;
                    address.sin_addr.s_addr = htonl(ad);
                    address.sin_port = htons(port);
                    
                    int sent = sendto(fd, testString.data(), testString.size(), 0, reinterpret_cast<sockaddr*>(&address), sizeof(sockaddr_in));
                    if(sent < 0) {
                        throw std::system_error(errno, std::system_category());
                    }
                    
                    close(fd);
                    
                    REQUIRE(sent == testString.size());
                }
            });
            
            on<Startup>().then([this] {
                
                // Emit a message just so it will be when everything is running
                emit(std::make_unique<Message>());
            });
                               
        }
    };
}

TEST_CASE("Testing sending and receiving of UDP Broadcast messages", "[api][network][udp][broadcast]") {
    
    NUClear::PowerPlant::Configuration config;
    config.threadCount = 1;
    NUClear::PowerPlant plant(config);
    plant.install<TestReactor>();
    
    plant.start();
}
