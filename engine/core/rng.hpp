#pragma once

#include <random>

class RNG {
public:
    explicit RNG(std::uint_fast32_t seed = std::random_device{}())
        : engine_(seed) {}

    // Uniform float in [0, 1)
    float uniform() {
        return std::uniform_real_distribution<float>(0.f, 1.f)(engine_);
    }

    // Uniform float in [min, max)
    float uniform(float min, float max) {
        return std::uniform_real_distribution<float>(min, max)(engine_);
    }

    std::mt19937& engine() { return engine_; }
    const std::mt19937& engine() const { return engine_; }

private:
    std::mt19937 engine_;
};
