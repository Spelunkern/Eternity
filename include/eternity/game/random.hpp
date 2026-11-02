#pragma once
#include <cstdint>
#include <array>

namespace eternity::game {
class RandomSource {
public:
    virtual ~RandomSource() = default;
    virtual std::int32_t next() = 0;
};
// MSVCRT rand 00504697, state owned per original executor/thread.
class MicrosoftRandom final:public RandomSource {
public:
    explicit MicrosoftRandom(std::uint32_t seed=1):state_(seed) {}
    std::int32_t next() override {state_=state_*214013U+2531011U;return static_cast<std::int32_t>((state_>>16)&32767U);}
    std::uint32_t state() const noexcept {return state_;}
private:
    std::uint32_t state_;
};
// Original SRandom 004a8840/004a8860/004a8980; uses the earlier 69069
// seeding recurrence, so std::mt19937 does not produce the same stream.
class ServerRandom {
public:
    ServerRandom()=default;
    explicit ServerRandom(std::uint32_t initial_seed):seed_(initial_seed) {}
    void seed(std::uint32_t) noexcept;
    std::uint32_t next(std::uint32_t bound) noexcept;
    std::uint32_t seed() const noexcept {return seed_;}
    std::uint32_t index() const noexcept {return index_;}
    const std::array<std::uint32_t,624>& words() const noexcept {return words_;}
private:
    std::uint32_t seed_=0,index_=625;
    std::array<std::uint32_t,624> words_{};
};
}
