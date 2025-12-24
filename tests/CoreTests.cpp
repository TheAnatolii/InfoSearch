#include <gtest/gtest.h>
#include "core/HashMap.hpp"
#include "core/InvertedIndex.hpp"

// ==========================================
// Тесты для HashMap
// ==========================================

// 1. Проверка базовых операций: вставка и получение
TEST(HashMapTest, InsertAndGet)
{
    HashMap<std::string, int> map;
    map.insert("apple", 100);
    map.insert("banana", 200);

    int *val1 = map.get("apple");
    int *val2 = map.get("banana");
    int *val3 = map.get("cherry"); // Нет в мапе

    ASSERT_NE(val1, nullptr);
    EXPECT_EQ(*val1, 100);

    ASSERT_NE(val2, nullptr);
    EXPECT_EQ(*val2, 200);

    EXPECT_EQ(val3, nullptr);
}

// 2. Проверка обновления значения по существующему ключу
TEST(HashMapTest, UpdatesExistingKey)
{
    HashMap<std::string, std::string> map;
    map.insert("key1", "value1");

    // Обновляем
    map.insert("key1", "value2");

    std::string *val = map.get("key1");
    ASSERT_NE(val, nullptr);
    EXPECT_EQ(*val, "value2");

    // Размер не должен измениться
    EXPECT_EQ(map.size(), 1);
}

// 3. Проверка метода contains
TEST(HashMapTest, CheckContains)
{
    HashMap<int, int> map;
    map.insert(1, 10);

    EXPECT_TRUE(map.contains(1));
    EXPECT_FALSE(map.contains(2));
}

// 4. Стресс-тест и рехеширование (Rehash)
// Проверяем, что при вставке большого кол-ва элементов таблица расширяется
// и данные не теряются.
TEST(HashMapTest, HandlesResizeAndCollisions)
{
    HashMap<int, int> map(10); // Маленький начальный размер
    int items = 1000;

    for (int i = 0; i < items; ++i)
    {
        map.insert(i, i * 2);
    }

    EXPECT_EQ(map.size(), items);

    // Проверяем выборочно, что данные на месте
    int *valStart = map.get(0);
    int *valMid = map.get(500);
    int *valEnd = map.get(999);

    ASSERT_NE(valStart, nullptr);
    ASSERT_NE(valMid, nullptr);
    ASSERT_NE(valEnd, nullptr);

    EXPECT_EQ(*valStart, 0);
    EXPECT_EQ(*valMid, 1000);
    EXPECT_EQ(*valEnd, 1998);
}

// 5. Проверка очистки
TEST(HashMapTest, ClearWorks)
{
    HashMap<std::string, int> map;
    map.insert("test", 1);
    map.clear();

    EXPECT_EQ(map.size(), 0);
    EXPECT_EQ(map.get("test"), nullptr);
}

// ==========================================
// Тесты для InvertedIndex
// ==========================================

// 6. Добавление одного документа и слова
TEST(InvertedIndexTest, AddsSingleTerm)
{
    InvertedIndex index;
    // docId=1, слово="hello"
    index.addTerm("hello", 1);

    auto postings = index.getPostings("hello");
    ASSERT_NE(postings, nullptr);
    ASSERT_EQ(postings->size(), 1);

    EXPECT_EQ((*postings)[0].docId, 1);
    EXPECT_EQ((*postings)[0].termFrequency, 1);
}

// 7. Подсчет Term Frequency (TF) для одного документа
// Логика: если docId тот же самый, мы не создаем новый постинг, а инкрементим счетчик
TEST(InvertedIndexTest, CalculatesTFCorrectly)
{
    InvertedIndex index;
    // Документ 10 содержит слово "apple" три раза
    index.addTerm("apple", 10);
    index.addTerm("apple", 10);
    index.addTerm("apple", 10);

    auto postings = index.getPostings("apple");
    ASSERT_NE(postings, nullptr);
    ASSERT_EQ(postings->size(), 1); // Должна быть одна запись для документа 10

    EXPECT_EQ((*postings)[0].docId, 10);
    EXPECT_EQ((*postings)[0].termFrequency, 3);
}

// 8. Добавление разных документов
TEST(InvertedIndexTest, HandlesMultipleDocuments)
{
    InvertedIndex index;
    // Слово "test" встречается в doc 1 и doc 2
    index.addTerm("test", 1);
    index.addTerm("test", 2);

    auto postings = index.getPostings("test");
    ASSERT_NE(postings, nullptr);
    ASSERT_EQ(postings->size(), 2);

    EXPECT_EQ((*postings)[0].docId, 1);
    EXPECT_EQ((*postings)[1].docId, 2);
}

// 9. Сложный сценарий индексации
TEST(InvertedIndexTest, ComplexIndexingScenario)
{
    InvertedIndex index;

    // Doc 1: "cat dog"
    index.addTerm("cat", 1);
    index.addTerm("dog", 1);

    // Doc 2: "cat cat"
    index.addTerm("cat", 2);
    index.addTerm("cat", 2);

    // Проверяем "cat"
    auto catList = index.getPostings("cat");
    ASSERT_EQ(catList->size(), 2);
    EXPECT_EQ((*catList)[0].docId, 1);
    EXPECT_EQ((*catList)[0].termFrequency, 1);
    EXPECT_EQ((*catList)[1].docId, 2);
    EXPECT_EQ((*catList)[1].termFrequency, 2);

    // Проверяем "dog"
    auto dogList = index.getPostings("dog");
    ASSERT_EQ(dogList->size(), 1);
    EXPECT_EQ((*dogList)[0].docId, 1);
}

// 10. Обработка несуществующего слова
TEST(InvertedIndexTest, ReturnsNullForUnknownTerm)
{
    InvertedIndex index;
    index.addTerm("exists", 1);

    auto postings = index.getPostings("missing");
    EXPECT_EQ(postings, nullptr);
}