#ifndef HASHMAP_HPP
#define HASHMAP_HPP

#include <vector>
#include <string>
#include <functional>
#include <stdexcept>

// Элемент хеш-таблицы (пара Ключ-Значение)
template <typename K, typename V>
struct HashNode
{
    K key;
    V value;
    HashNode(K k, V v) : key(k), value(v) {}
};

template <typename K, typename V>
class HashMap
{
private:
    // Используем вектор векторов для метода цепочек
    // std::vector<HashNode> внутри - это "цепочка" при коллизии
    std::vector<std::vector<HashNode<K, V>>> buckets;
    size_t tableSize;
    size_t elementCount;
    const float maxLoadFactor = 0.75f;

    // Хеш-функция (для строк используем стандартную или DJB2)
    size_t hashFunction(const K &key) const
    {
        return std::hash<K>{}(key) % tableSize;
    }

    // Метод изменения размера таблицы при достижении Load Factor
    void rehash()
    {
        size_t oldSize = tableSize;
        tableSize *= 2;
        auto oldBuckets = std::move(buckets);

        buckets.assign(tableSize, std::vector<HashNode<K, V>>());
        elementCount = 0;

        for (size_t i = 0; i < oldSize; ++i)
        {
            for (auto &node : oldBuckets[i])
            {
                insert(node.key, node.value);
            }
        }
    }

public:
    HashMap(size_t initialSize = 1009) : tableSize(initialSize), elementCount(0)
    {
        buckets.resize(tableSize);
    }

    void insert(const K &key, const V &value)
    {
        if ((float)elementCount / tableSize > maxLoadFactor)
        {
            rehash();
        }

        size_t index = hashFunction(key);
        // Проверяем, существует ли ключ, чтобы обновить значение
        for (auto &node : buckets[index])
        {
            if (node.key == key)
            {
                node.value = value;
                return;
            }
        }

        buckets[index].emplace_back(key, value);
        elementCount++;
    }

    // Возвращает указатель на значение, или nullptr если не найдено
    V *get(const K &key)
    {
        size_t index = hashFunction(key);
        for (auto &node : buckets[index])
        {
            if (node.key == key)
            {
                return &node.value;
            }
        }
        return nullptr;
    }

    bool contains(const K &key) const
    {
        size_t index = hashFunction(key);
        for (const auto &node : buckets[index])
        {
            if (node.key == key)
                return true;
        }
        return false;
    }

    size_t size() const { return elementCount; }

    // Метод для обхода всех элементов (нужен для сохранения на диск)
    void traverse(std::function<void(const K &key, const V &value)> callback) const
    {
        for (const auto &bucket : buckets)
        {
            for (const auto &node : bucket)
            {
                callback(node.key, node.value);
            }
        }
    }

    void clear()
    {
        for (auto &bucket : buckets)
        {
            bucket.clear();
        }
        elementCount = 0;
    }
};

#endif