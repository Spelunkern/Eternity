#pragma once
#include <cstdint>

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
}
