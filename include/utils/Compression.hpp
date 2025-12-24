#ifndef COMPRESSION_HPP
#define COMPRESSION_HPP

#include <vector>
#include <cstdint>

class Compression
{
public:
    static void encodeVarByte(uint32_t number, std::vector<uint8_t> &output)
    {
        while (number >= 128)
        {
            output.push_back((number & 127) | 128);
            number >>= 7;
        }
        output.push_back(number);
    }

    static uint32_t decodeVarByte(const std::vector<uint8_t> &input, size_t &pos)
    {
        uint32_t number = 0;
        int shift = 0;
        while (true)
        {
            if (pos >= input.size())
                return 0; // Защита от выхода за границы

            uint8_t byte = input[pos++];
            number |= (uint32_t)(byte & 127) << shift;

            if (!(byte & 128))
                break; // Если старший бит 0, значит число кончилось
            shift += 7;
        }
        return number;
    }

    static std::vector<uint8_t> compressList(const std::vector<uint32_t> &numbers)
    {
        std::vector<uint8_t> compressed;
        // Резервируем память примерно (1 байт на число - оптимистично)
        compressed.reserve(numbers.size());
        for (uint32_t n : numbers)
        {
            encodeVarByte(n, compressed);
        }
        return compressed;
    }
};

#endif