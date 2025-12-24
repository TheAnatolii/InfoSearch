#ifndef INVERTED_INDEX_HPP
#define INVERTED_INDEX_HPP

#include "HashMap.hpp"
#include "../utils/Compression.hpp"
#include <vector>
#include <string>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <utility>

struct Posting
{
    uint32_t docId;
    uint32_t termFrequency;
    Posting(uint32_t id, uint32_t tf) : docId(id), termFrequency(tf) {}
    Posting() : docId(0), termFrequency(0) {}
};

using PostingsList = std::vector<Posting>;

class InvertedIndex
{
private:
    HashMap<std::string, PostingsList> index;
    size_t totalDocs = 0;

public:
    void addTerm(const std::string &term, uint32_t docId)
    {
        PostingsList *list = index.get(term);
        if (list == nullptr)
        {
            PostingsList newList;
            newList.emplace_back(docId, 1);
            index.insert(term, newList);
        }
        else
        {
            if (!list->empty() && list->back().docId == docId)
            {
                list->back().termFrequency++;
            }
            else
            {
                list->emplace_back(docId, 1);
            }
        }
    }

    PostingsList *getPostings(const std::string &term)
    {
        return index.get(term);
    }

    void incrementDocCount() { totalDocs++; }
    size_t getTotalDocs() const { return totalDocs; }

    bool save(const std::string &filename)
    {
        std::ofstream out(filename, std::ios::binary);
        if (!out.is_open())
            return false;

        out.write(reinterpret_cast<const char *>(&totalDocs), sizeof(totalDocs));

        size_t termCount = index.size();
        out.write(reinterpret_cast<const char *>(&termCount), sizeof(termCount));

        index.traverse([&](const std::string &term, const PostingsList &postings)
                       {
            // 1. Пишем слово
            size_t termLen = term.size();
            out.write(reinterpret_cast<const char*>(&termLen), sizeof(termLen));
            out.write(term.c_str(), termLen);

            // 2. Подготавливаем данные для сжатия
            std::vector<uint32_t> deltaDocIds;
            std::vector<uint32_t> tfs;
            deltaDocIds.reserve(postings.size());
            tfs.reserve(postings.size());

            uint32_t previousDocId = 0;
            for (const auto& p : postings) {
                deltaDocIds.push_back(p.docId - previousDocId);
                previousDocId = p.docId;
                
                tfs.push_back(p.termFrequency);
            }

            // 3. Сжимаем оба списка (DocID's и TF's)
            std::vector<uint8_t> compressedDeltas = Compression::compressList(deltaDocIds);
            std::vector<uint8_t> compressedTfs = Compression::compressList(tfs);

            // 4. Пишем размеры сжатых блоков
            size_t sizeDeltas = compressedDeltas.size();
            size_t sizeTfs = compressedTfs.size();
            out.write(reinterpret_cast<const char*>(&sizeDeltas), sizeof(sizeDeltas));
            out.write(reinterpret_cast<const char*>(&sizeTfs), sizeof(sizeTfs));

            // 5. Пишем сами сжатые данные
            out.write(reinterpret_cast<const char*>(compressedDeltas.data()), sizeDeltas);
            out.write(reinterpret_cast<const char*>(compressedTfs.data()), sizeTfs); });

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

            // 2. Читаем размеры блоков
            size_t sizeDeltas = 0, sizeTfs = 0;
            in.read(reinterpret_cast<char *>(&sizeDeltas), sizeof(sizeDeltas));
            in.read(reinterpret_cast<char *>(&sizeTfs), sizeof(sizeTfs));

            // 3. Читаем сжатые данные
            std::vector<uint8_t> compressedDeltas(sizeDeltas);
            std::vector<uint8_t> compressedTfs(sizeTfs);
            in.read(reinterpret_cast<char *>(compressedDeltas.data()), sizeDeltas);
            in.read(reinterpret_cast<char *>(compressedTfs.data()), sizeTfs);

            // 4. Распаковка (VarByte -> Delta -> DocID)
            PostingsList postings;
            size_t posD = 0;
            size_t posT = 0;

            uint32_t currentDocId = 0;

            // Пока не дочитаем весь буфер дельт
            while (posD < sizeDeltas)
            {
                uint32_t delta = Compression::decodeVarByte(compressedDeltas, posD);
                uint32_t tf = Compression::decodeVarByte(compressedTfs, posT);

                currentDocId += delta;
                postings.emplace_back(currentDocId, tf);
            }

            index.insert(term, postings);
        }

        in.close();
        return true;
    }

    void exportFrequencyStats(const std::string &filename)
    {
        std::ofstream out(filename);
        if (!out.is_open())
            return;

        // 1. Собираем пары <Слово, ОбщаяЧастота>
        std::vector<std::pair<std::string, uint64_t>> stats;

        index.traverse([&](const std::string &term, const PostingsList &postings)
                       {
            uint64_t collectionFreq = 0;
            for (const auto& p : postings) {
                collectionFreq += p.termFrequency;
            }
            stats.push_back({term, collectionFreq}); });

        // 2. Сортируем по убыванию частоты (самые частые — в начале)
        std::sort(stats.begin(), stats.end(),
                  [](const auto &a, const auto &b)
                  {
                      return a.second > b.second;
                  });

        // 3. Пишем CSV: Rank,Term,Frequency
        out << "Rank,Term,Frequency\n";
        size_t rank = 1;
        for (const auto &pair : stats)
        {
            out << rank << "," << pair.first << "," << pair.second << "\n";
            rank++;
        }

        out.close();
        std::cout << "Zipf stats exported to " << filename << std::endl;
    }

    void exportToBooleanIndex(const std::string &filename)
    {
        std::ofstream out(filename, std::ios::binary);
        if (!out.is_open())
            return;

        out.write(reinterpret_cast<const char *>(&totalDocs), sizeof(totalDocs));

        size_t termCount = index.size();
        out.write(reinterpret_cast<const char *>(&termCount), sizeof(termCount));

        index.traverse([&](const std::string &term, const PostingsList &postings)
                       {
            // 1. Пишем слово
            size_t termLen = term.size();
            out.write(reinterpret_cast<const char*>(&termLen), sizeof(termLen));
            out.write(term.c_str(), termLen);

            // 2. Пишем список уникальных документов (без TF)
            size_t docCount = postings.size();
            out.write(reinterpret_cast<const char*>(&docCount), sizeof(docCount));
            for (const auto &p : postings)
            {
                out.write(reinterpret_cast<const char*>(&p.docId), sizeof(p.docId));
            } });

        out.close();
    }
};

#endif