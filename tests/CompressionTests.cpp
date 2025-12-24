#include <gtest/gtest.h>
#include "utils/Compression.hpp"
#include <limits>

// 1. Тест для маленьких чисел (< 128)
// Они должны занимать ровно 1 байт и совпадать с самим числом (так как старший бит 0)
TEST(CompressionTest, EncodesSingleByte)
{
    std::vector<uint8_t> buffer;

    // Число 5
    Compression::encodeVarByte(5, buffer);
    ASSERT_EQ(buffer.size(), 1);
    EXPECT_EQ(buffer[0], 5); // 00000101

    // Декодируем обратно
    size_t pos = 0;
    uint32_t result = Compression::decodeVarByte(buffer, pos);
    EXPECT_EQ(result, 5);
    EXPECT_EQ(pos, 1); // Позиция должна сдвинуться на 1
}

// 2. Тест границы (127 и 128)
// 127 влезает в 1 байт. 128 уже требует 2 байта.
TEST(CompressionTest, BoundaryCheck128)
{
    std::vector<uint8_t> buffer;

    // 127 -> 0x7F (1 байт)
    Compression::encodeVarByte(127, buffer);
    ASSERT_EQ(buffer.size(), 1);

    // 128 -> Требует 2 байта.
    // 128 бинарно: 1000 0000.
    // Младшие 7 бит: 000 0000. С флагом продолжения: 1000 0000 (0x80 = 128)
    // Оставшиеся биты: 1. Байт: 0000 0001 (0x01 = 1)
    Compression::encodeVarByte(128, buffer);

    // Теперь в буфере 3 байта: [127] [128, 1]
    ASSERT_EQ(buffer.size(), 3);

    size_t pos = 0;
    EXPECT_EQ(Compression::decodeVarByte(buffer, pos), 127);
    EXPECT_EQ(Compression::decodeVarByte(buffer, pos), 128);
}

// 3. Тест больших чисел
// 300 = 256 + 44. Это больше 127, значит минимум 2 байта.
TEST(CompressionTest, EncodesLargeNumbers)
{
    std::vector<uint8_t> buffer;
    uint32_t val = 300;

    Compression::encodeVarByte(val, buffer);

    size_t pos = 0;
    uint32_t decoded = Compression::decodeVarByte(buffer, pos);
    EXPECT_EQ(decoded, 300);
}

// 4. Тест максимального значения uint32_t
// Проверяем, что алгоритм не ломается на пределе (4 миллиарда)
// Это займет 5 байт (32 бита / 7 = 4.5 -> 5 групп)
TEST(CompressionTest, HandlesMaxUint32)
{
    std::vector<uint8_t> buffer;
    uint32_t maxVal = std::numeric_limits<uint32_t>::max();

    Compression::encodeVarByte(maxVal, buffer);

    ASSERT_EQ(buffer.size(), 5); // 5 байт для VarByte

    size_t pos = 0;
    uint32_t decoded = Compression::decodeVarByte(buffer, pos);
    EXPECT_EQ(decoded, maxVal);
}

// 5. Тест сжатия списка (compressList)
// Эмуляция того, что делает InvertedIndex при сохранении
TEST(CompressionTest, CompressesListCorrectly)
{
    std::vector<uint32_t> original = {10, 150, 0, 99999, 1};

    // Сжимаем
    auto compressed = Compression::compressList(original);

    // Проверяем, что размер сжатого вектора логичен
    // 10 -> 1B, 150 -> 2B, 0 -> 1B, 99999 -> 3B, 1 -> 1B. Итого ~8 байт
    EXPECT_GE(compressed.size(), 5);

    // Распаковываем вручную по одному
    size_t pos = 0;
    for (uint32_t expected : original)
    {
        uint32_t got = Compression::decodeVarByte(compressed, pos);
        EXPECT_EQ(got, expected);
    }

    // Убеждаемся, что мы дошли до конца буфера
    EXPECT_EQ(pos, compressed.size());
}

// 6. Защита от выхода за границы буфера
// Если мы пытаемся читать из пустого буфера или конца буфера
TEST(CompressionTest, ReturnsZeroOnOverflow)
{
    std::vector<uint8_t> buffer = {127}; // 1 байт
    size_t pos = 1;                      // Указывает уже за пределы

    // Должен вернуть 0 и не упасть
    EXPECT_EQ(Compression::decodeVarByte(buffer, pos), 0);
}

// 7. Проверка восстановления последовательности (как DocID + Delta)
// Если мы закодировали 5 чисел подряд, мы должны раскодировать их именно в том же порядке
TEST(CompressionTest, SequentialDecodingIntegrity)
{
    std::vector<uint32_t> numbers;
    for (int i = 0; i < 1000; ++i)
    {
        numbers.push_back(i * 10);
    }

    auto compressed = Compression::compressList(numbers);

    size_t pos = 0;
    for (size_t i = 0; i < numbers.size(); ++i)
    {
        uint32_t val = Compression::decodeVarByte(compressed, pos);
        if (val != numbers[i])
        {
            FAIL() << "Mismatch at index " << i << ": expected " << numbers[i] << ", got " << val;
        }
    }
}