#include "eternity/net/world_crypto.hpp"
#include <algorithm>
#include <openssl/evp.h>

namespace eternity::net {
namespace {
SessionSecrets split_key(const std::array<std::uint8_t,32>& key) {
    SessionSecrets result{};
    std::copy_n(key.begin(),16,result.key.begin());
    std::copy_n(key.begin()+16,16,result.iv.begin());
    return result;
}
}

WorldXor::WorldXor(std::array<std::uint8_t,16> seed) : seed_(seed) {
    // First block = SHA256(seed). Each next block hashes the preceding block's last 16 bytes.
    for(std::size_t offset=0; offset<table_.size(); offset+=32) {
        const auto* input=offset==0 ? seed_.data() : table_.data()+offset-16;
        unsigned int written=0;
        if(EVP_Digest(input,16,table_.data()+offset,&written,EVP_sha256(),nullptr)!=1 || written!=32)
            throw std::runtime_error("Build original world XOR table failed");
    }
}

void WorldXor::crypt(std::span<std::uint8_t> payload) const {
    if(payload.size()>maximum_frame_size-2)
        throw std::invalid_argument("World XOR payload exceeds original transport limit");
    // CryptoXor 00457f70 and CUserCrypto::Enc 00458080 index the table by payload length.
    for(std::size_t index=0; index<payload.size(); ++index)
        payload[index]^=table_[payload.size()+index];
}

WorldCipher::WorldCipher(const std::array<std::uint8_t,32>& key, std::shared_ptr<const WorldXor> world_xor)
    : receiver_(split_key(key)),sender_(split_key(key)),world_xor_(std::move(world_xor)) {
    if(!world_xor_) throw std::invalid_argument("World cipher requires the advertised XOR seed");
}
void WorldCipher::decrypt(std::span<std::uint8_t> payload) { receiver_.crypt(payload); }
void WorldCipher::encrypt(std::span<std::uint8_t> payload) {
    if(world_) world_xor_->crypt(payload);
    else sender_.crypt(payload);
}
}
