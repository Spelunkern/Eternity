#include "eternity/net/world_crypto.hpp"
#include <iostream>
#include <openssl/evp.h>

void require(bool condition,const char* message) { if(!condition) throw std::runtime_error(message); }

eternity::net::Packet hex(std::string_view value) {
    eternity::net::Packet result;
    const auto digit=[](char c)->unsigned int { return c<='9' ? static_cast<unsigned int>(c-'0') : static_cast<unsigned int>(c-'a'+10); };
    for(std::size_t index=0; index<value.size(); index+=2)
        result.push_back(static_cast<std::uint8_t>((digit(value[index])<<4)|digit(value[index+1])));
    return result;
}

int main() {
    using namespace eternity::net;
    try {
        std::array<std::uint8_t,16> seed{};
        for(std::size_t index=0; index<seed.size(); ++index) seed[index]=static_cast<std::uint8_t>(index);
        auto world_xor=std::make_shared<WorldXor>(seed);
        const std::vector<std::pair<std::size_t,std::string_view>> vectors={
            {1,"45"},{16,"43c79a53a7d9e3f9d3af9323e237879e"},
            {17,"c69952a0d8e0f8dcae9022e536849f13ba"},
            {36,"33ad4294334c52312df5fc55ef0f316e10f07c552a1d3d00a9b5407ed82df52f8d028950"}};
        for(const auto [size,expected]:vectors) {
            Packet value(size);
            for(std::size_t index=0; index<size; ++index) value[index]=static_cast<std::uint8_t>(index);
            const auto original=value; world_xor->crypt(value);
            require(value==hex(expected),"Original chained-hash XOR vector changed");
            world_xor->crypt(value); require(value==original,"XOR transform is not symmetric");
        }
        Packet maximum(2046);
        for(std::size_t index=0; index<maximum.size(); ++index) maximum[index]=static_cast<std::uint8_t>(index);
        world_xor->crypt(maximum);
        std::array<std::uint8_t,32> digest{};unsigned int length=0;
        require(EVP_Digest(maximum.data(),maximum.size(),digest.data(),&length,EVP_sha256(),nullptr)==1 && length==32,
                "Test ciphertext digest failed");
        require(Packet(digest.begin(),digest.end())==hex("2439eaa75d2d3c232474d44ddaf211bc689ed8620a03fe73e3e5b6ecfdb57a47"),
                "Maximum packet did not use the original XOR table extent");

        WorldCipher cipher({},world_xor);
        Packet selected(16,0);cipher.encrypt(selected);
        require(selected==hex("66e94bd4ef8a2c3b884cfa59ca342b2e"),"Character selection did not begin with AES");
        cipher.enter_world();
        Packet gameplay(17);
        for(std::size_t index=0; index<gameplay.size(); ++index) gameplay[index]=static_cast<std::uint8_t>(index);
        cipher.encrypt(gameplay);
        require(gameplay==hex("c69952a0d8e0f8dcae9022e536849f13ba"),"World entry did not switch outgoing encryption");
        LegacyCipher peer_sender(SessionSecrets{});
        Packet incoming(33,0x5a);peer_sender.crypt(incoming);cipher.decrypt(incoming);
        require(incoming==Packet(33,0x5a),"World entry incorrectly changed incoming AES");
        cipher.enter_character_selection();
        Packet returned(16,0);cipher.encrypt(returned);
        require(returned==hex("47711816e91d6ff059bbbf2bf58e0fd3"),"Character selection reset or advanced the AES counter while in world");
        bool rejected=false;
        try {Packet too_large(2047);world_xor->crypt(too_large);} catch(const std::invalid_argument&) {rejected=true;}
        require(rejected,"Oversize world packet was accepted");
        return 0;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
