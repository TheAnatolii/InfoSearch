#ifndef BOOLEAN_INDEX_HPP
#define BOOLEAN_INDEX_HPP

#include "HashMap.hpp"
#include <vector>
#include <string>
#include <fstream>
#include <iostream>
#include <algorithm>

class BooleanIndex
{
private:
    // Для каждого термина храним просто список ID документов (без TF и сжатия)
    HashMap<std::string, std::vector<uint32_t>> index;
    size_t totalDocs = 0;

public:
    void addTerm(const std::string &term, uint32_t docId)
    {
        std::vector<uint32_t> *list = index.get(term);
        if (list == nullptr)
        {
            std::vector<uint32_t> newList;
            newList.push_back(docId);
            index.insert(term, newList);
        }
        else
        {
            // Избегаем дубликатов
            if (list->empty() || list->back() != docId)
            {
                list->push_back(docId);
            }
        }
    }

    std::vector<uint32_t> *getDocIds(const std::string &term)
    {
        return index.get(term);
    }

    void setTotalDocs(size_t docs) { totalDocs = docs; }
    size_t getTotalDocs() const { return totalDocs; }

    bool save(const std::string &filename)
    {
        std::ofstream out(filename, std::ios::binary);
        if (!out.is_open())
            return false;

        out.write(reinterpret_cast<const char *>(&totalDocs), sizeof(totalDocs));

        size_t termCount = index.size();
        out.write(reinterpret_cast<const char *>(&termCount), sizeof(termCount));

        index.traverse([&](const std::string &term, const std::vector<uint32_t> &docIds)
                       {
            // 1. Пишем слово
            size_t termLen = term.size();
            out.write(reinterpret_cast<const char*>(&termLen), sizeof(termLen));
            out.write(term.c_str(), termLen);

            // 2. Пишем количество документов и сами ID документов
            size_t docCount = docIds.size();
            out.write(reinterpret_cast<const char*>(&docCount), sizeof(docCount));
            for (const auto &docId : docIds)
            {
                out.write(reinterpret_cast<const char*>(&docId), sizeof(docId));
            } });

        out.close();
        return true;
    }

    bool load(const std::string &filename)
    {
        std::ifstream in(filename, std::ios::binary);
        if (!in.is_open())
            return false;

        index.clear();
        in.read(reinterpret_cast<char *>(&totalDocs), sizeof(totalDocs));

        size_t termCount = 0;
        in.read(reinterpret_cast<char *>(&termCount), sizeof(termCount));

        for (size_t i = 0; i < termCount; ++i)
        {
            // 1. Читаем слово
            size_t termLen = 0;
            in.read(reinterpret_cast<char *>(&termLen), sizeof(termLen));
            std::string term(termLen, '\0');
            in.read(&term[0], termLen);

            // 2. Читаем документы
            size_t docCount = 0;
            in.read(reinterpret_cast<char *>(&docCount), sizeof(docCount));
            std::vector<uint32_t> docIds(docCount);
            for (size_t j = 0; j < docCount; ++j)
            {
                in.read(reinterpret_cast<char *>(&docIds[j]), sizeof(docIds[j]));
            }

            index.insert(term, docIds);
        }

        in.close();
        return true;
    }
};

#endif
