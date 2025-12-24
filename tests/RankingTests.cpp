#include <gtest/gtest.h>
#include "ranking/Scorer.hpp"
#include "core/InvertedIndex.hpp"

// Хелпер для быстрой настройки индекса
class RankingTest : public ::testing::Test
{
protected:
    InvertedIndex index;

    void SetUp() override
    {
        // По умолчанию ничего не делаем, настраиваем в каждом тесте
    }

    // Вспомогательный метод: устанавливаем N документов в индексе
    void setDocCount(size_t n)
    {
        for (size_t i = 0; i < n; ++i)
        {
            index.incrementDocCount();
        }
    }
};

// 1. Проверка влияния TF (Term Frequency)
// Если слово встречается чаще в документе, его скор должен быть выше.
TEST_F(RankingTest, HighTFScoredHigher)
{
    setDocCount(3); // N=3 (чтобы IDF не был 0)

    // Doc 1: "apple" (1 раз)
    index.addTerm("apple", 1);

    // Doc 2: "apple apple apple" (3 раза)
    index.addTerm("apple", 2);
    index.addTerm("apple", 2);
    index.addTerm("apple", 2);

    std::vector<std::string> query = {"apple"};
    auto results = Scorer::search(query, index);

    ASSERT_EQ(results.size(), 2);

    // Doc 2 должен быть первым (TF=3 > TF=1)
    EXPECT_EQ(results[0].docId, 2);
    EXPECT_EQ(results[1].docId, 1);

    // Проверяем, что скор первого больше второго
    EXPECT_GT(results[0].score, results[1].score);
}

// 2. Проверка влияния IDF (Inverse Document Frequency)
// Редкое слово должно весить больше, чем частое.
TEST_F(RankingTest, RareTermBoostsScore)
{
    setDocCount(10); // N=10

    // Слово "common" есть в 9 документах (IDF низкий)
    for (int i = 1; i <= 9; ++i)
        index.addTerm("common", i);

    // Слово "rare" есть только в документе 1 (IDF высокий)
    index.addTerm("rare", 1);

    // Ищем: "common rare"
    // Doc 1 содержит оба слова. Doc 2 только "common".
    std::vector<std::string> query = {"common", "rare"};
    auto results = Scorer::search(query, index);

    ASSERT_GE(results.size(), 2);

    // Doc 1 должен победить с большим отрывом за счет "rare"
    EXPECT_EQ(results[0].docId, 1);

    // Скор Doc 1 должен быть существенно выше, чем просто TF*IDF("common")
    // Потому что у него добавляется TF*IDF("rare")
    EXPECT_GT(results[0].score, results[1].score * 2.0);
}

// 3. Суммирование скоров (Multi-term query)
TEST_F(RankingTest, AccumulatesScores)
{
    setDocCount(5);

    // Doc 1: "A B"
    index.addTerm("A", 1);
    index.addTerm("B", 1);

    // Doc 2: "A"
    index.addTerm("A", 2);

    // Doc 3: "B"
    index.addTerm("B", 3);

    std::vector<std::string> query = {"A", "B"};
    auto results = Scorer::search(query, index);

    ASSERT_EQ(results.size(), 3);

    // Doc 1 должен быть первым, так как содержит оба слова
    EXPECT_EQ(results[0].docId, 1);
}

// 4. Поиск несуществующего слова
TEST_F(RankingTest, ReturnsEmptyForUnknownTerm)
{
    setDocCount(5);
    index.addTerm("exist", 1);

    std::vector<std::string> query = {"missing"};
    auto results = Scorer::search(query, index);

    EXPECT_TRUE(results.empty());
}

// 5. Проверка фильтрации (allowedDocIds)
TEST_F(RankingTest, FiltersResults)
{
    setDocCount(5);

    // Слово "test" есть в docs 1, 2, 3
    index.addTerm("test", 1);
    index.addTerm("test", 2);
    index.addTerm("test", 3);

    // Разрешаем поиск только в docs 1 и 3
    std::vector<uint32_t> allowed = {1, 3};
    // ВАЖНО: allowed должен быть отсортирован для binary_search, который внутри Scorer
    std::sort(allowed.begin(), allowed.end());

    std::vector<std::string> query = {"test"};
    auto results = Scorer::search(query, index, &allowed);

    ASSERT_EQ(results.size(), 2);

    // Проверяем, что ID 2 не попал в выдачу
    bool foundDoc2 = false;
    for (auto &res : results)
    {
        if (res.docId == 2)
            foundDoc2 = true;
    }
    EXPECT_FALSE(foundDoc2);
}

// 6. Пустой запрос
TEST_F(RankingTest, HandlesEmptyQuery)
{
    setDocCount(5);
    index.addTerm("something", 1);

    std::vector<std::string> query = {};
    auto results = Scorer::search(query, index);

    EXPECT_TRUE(results.empty());
}

// 7. Сортировка результатов
// Убеждаемся, что выдача действительно отсортирована по убыванию скора
TEST_F(RankingTest, ResultsAreSortedDescending)
{
    setDocCount(5);

    // Создаем ситуацию с разным кол-вом повторений
    // Doc 1: 1 раз
    // Doc 2: 5 раз
    // Doc 3: 3 раза
    index.addTerm("word", 1);

    for (int i = 0; i < 5; ++i)
        index.addTerm("word", 2);

    for (int i = 0; i < 3; ++i)
        index.addTerm("word", 3);

    std::vector<std::string> query = {"word"};
    auto results = Scorer::search(query, index);

    ASSERT_EQ(results.size(), 3);

    // Ожидаемый порядок: Doc 2 (score high), Doc 3 (med), Doc 1 (low)
    EXPECT_EQ(results[0].docId, 2);
    EXPECT_EQ(results[1].docId, 3);
    EXPECT_EQ(results[2].docId, 1);

    // Проверка математики сортировки
    EXPECT_GT(results[0].score, results[1].score);
    EXPECT_GT(results[1].score, results[2].score);
}