#include "platform/psvita/shaders/PsVitaShaderArtifactHash.hpp"

#include <array>
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <vector>

namespace {
    /// Rotates one 32-bit word right by the requested number of bits.
    std::uint32_t RotateRight(std::uint32_t value, std::uint32_t amount) {
        return (value >> amount) | (value << (32u - amount));
    }

    /// Converts one big-endian digest word into four output bytes.
    void WriteWord(std::array<std::uint8_t, 32u>& digest, std::size_t offset, std::uint32_t word) {
        digest[offset] = static_cast<std::uint8_t>(word >> 24u);
        digest[offset + 1u] = static_cast<std::uint8_t>(word >> 16u);
        digest[offset + 2u] = static_cast<std::uint8_t>(word >> 8u);
        digest[offset + 3u] = static_cast<std::uint8_t>(word);
    }
}

namespace helengine::psvita::shaders {
    /// Computes SHA-256 over one complete byte sequence.
    std::string PsVitaShaderArtifactHash::Compute(const std::uint8_t* bytes, std::size_t byteCount) {
        if (bytes == nullptr && byteCount != 0u) {
            return std::string();
        }

        static constexpr std::uint32_t RoundConstants[64u] = {
            0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
            0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
            0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
            0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
            0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
            0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
            0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
            0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u, 0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u
        };
        std::array<std::uint32_t, 8u> state = {
            0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
            0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u
        };

        std::size_t paddedSize = byteCount + 1u;
        while ((paddedSize % 64u) != 56u) {
            ++paddedSize;
        }
        std::vector<std::uint8_t> padded(paddedSize + 8u, 0u);
        if (byteCount != 0u) {
            std::copy(bytes, bytes + byteCount, padded.begin());
        }
        padded[byteCount] = 0x80u;
        std::uint64_t bitCount = static_cast<std::uint64_t>(byteCount) * 8u;
        for (std::size_t index = 0u; index < 8u; ++index) {
            padded[padded.size() - 1u - index] = static_cast<std::uint8_t>(bitCount >> (index * 8u));
        }

        for (std::size_t block = 0u; block < padded.size(); block += 64u) {
            std::array<std::uint32_t, 64u> schedule{};
            for (std::size_t index = 0u; index < 16u; ++index) {
                std::size_t offset = block + index * 4u;
                schedule[index] = (static_cast<std::uint32_t>(padded[offset]) << 24u)
                    | (static_cast<std::uint32_t>(padded[offset + 1u]) << 16u)
                    | (static_cast<std::uint32_t>(padded[offset + 2u]) << 8u)
                    | static_cast<std::uint32_t>(padded[offset + 3u]);
            }
            for (std::size_t index = 16u; index < 64u; ++index) {
                std::uint32_t s0 = RotateRight(schedule[index - 15u], 7u) ^ RotateRight(schedule[index - 15u], 18u) ^ (schedule[index - 15u] >> 3u);
                std::uint32_t s1 = RotateRight(schedule[index - 2u], 17u) ^ RotateRight(schedule[index - 2u], 19u) ^ (schedule[index - 2u] >> 10u);
                schedule[index] = schedule[index - 16u] + s0 + schedule[index - 7u] + s1;
            }

            std::uint32_t a = state[0u];
            std::uint32_t b = state[1u];
            std::uint32_t c = state[2u];
            std::uint32_t d = state[3u];
            std::uint32_t e = state[4u];
            std::uint32_t f = state[5u];
            std::uint32_t g = state[6u];
            std::uint32_t h = state[7u];
            for (std::size_t index = 0u; index < 64u; ++index) {
                std::uint32_t s1 = RotateRight(e, 6u) ^ RotateRight(e, 11u) ^ RotateRight(e, 25u);
                std::uint32_t choice = (e & f) ^ ((~e) & g);
                std::uint32_t temporary1 = h + s1 + choice + RoundConstants[index] + schedule[index];
                std::uint32_t s0 = RotateRight(a, 2u) ^ RotateRight(a, 13u) ^ RotateRight(a, 22u);
                std::uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
                std::uint32_t temporary2 = s0 + majority;
                h = g;
                g = f;
                f = e;
                e = d + temporary1;
                d = c;
                c = b;
                b = a;
                a = temporary1 + temporary2;
            }
            state[0u] += a;
            state[1u] += b;
            state[2u] += c;
            state[3u] += d;
            state[4u] += e;
            state[5u] += f;
            state[6u] += g;
            state[7u] += h;
        }

        std::array<std::uint8_t, 32u> digest{};
        for (std::size_t index = 0u; index < state.size(); ++index) {
            WriteWord(digest, index * 4u, state[index]);
        }
        std::ostringstream result;
        result << std::uppercase << std::hex << std::setfill('0');
        for (std::uint8_t byte : digest) {
            result << std::setw(2) << static_cast<unsigned int>(byte);
        }
        return result.str();
    }
}
