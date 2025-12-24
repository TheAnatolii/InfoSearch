#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <filesystem>
#include <algorithm>

#include "db/MongoConnector.hpp"
#include "nlp/HtmlParser.hpp"
#include "nlp/Tokenizer.hpp"
#include "nlp/Lemmatizer.hpp"
#include "core/InvertedIndex.hpp"
#include "core/BooleanIndex.hpp"
#include "ranking/Scorer.hpp"
#include "nlp/QueryParser.hpp"

// --- Хелперы для загрузки/сохранения URL ---
bool saveUrls(const std::string &filename, const std::vector<std::string> &urls)
{
    std::ofstream out(filename, std::ios::binary);
    if (!out.is_open())
        return false;
    size_t count = urls.size();
    out.write(reinterpret_cast<const char *>(&count), sizeof(count));
    for (const auto &url : urls)
    {
        size_t len = url.size();
        out.write(reinterpret_cast<const char *>(&len), sizeof(len));
        out.write(url.c_str(), len);
    }
    return true;
}

bool loadUrls(const std::string &filename, std::vector<std::string> &urls)
{
    std::ifstream in(filename, std::ios::binary);
    if (!in.is_open())
        return false;
    urls.clear();
    size_t count = 0;
    in.read(reinterpret_cast<char *>(&count), sizeof(count));
    urls.reserve(count);
    for (size_t i = 0; i < count; ++i)
    {
        size_t len = 0;
        in.read(reinterpret_cast<char *>(&len), sizeof(len));
        std::string url(len, '\0');
        in.read(&url[0], len);
        urls.push_back(url);
    }
    return true;
}

int main(int argc, char *argv[])
{
    // Конфигурация
    bool useBooleanMode = false;
    for (int i = 1; i < argc; ++i)
    {
        if (std::string(argv[i]) == "--bool")
        {
            useBooleanMode = true;
            break;
        }
    }

    const std::string INDEX_FILE = "index.bin";
    const std::string BOOLEAN_INDEX_FILE = "boolean_index.bin";
    const std::string URLS_FILE = "urls.bin";

    Lemmatizer lemmatizer;
    QueryParser queryParser(lemmatizer);

    std::vector<std::string> docUrls;

    std::cout << "=== Search Engine Initialization ===" << std::endl;

    // Проверка наличия индекса
    if (!std::filesystem::exists(INDEX_FILE))
    {
        std::cout << "[INIT] Main index not found. Starting indexing from MongoDB..." << std::endl;

        // Подключение к БД
        MongoConnector db("mongodb://localhost:27017", "search_engine", "pages");
        InvertedIndex tempIndex;

        std::cout << "[INIT] Processing documents..." << std::endl;

        // Обработка документов
        db.processAllDocuments([&](const RawDocument &doc)
                               {

            if (docUrls.size() <= doc.id) docUrls.resize(doc.id + 1);
            docUrls[doc.id] = doc.url;

            if (doc.html.empty()) return;

            std::string plainText = HtmlParser::getCleanText(doc.html);
            
            std::vector<std::string> tokens = Tokenizer::tokenize(plainText);
            
            std::vector<std::string> terms;
            terms.reserve(tokens.size());
            for(const auto& token : tokens) {
                std::string lemma = lemmatizer.lemmatize(token);
                if (!lemma.empty()) {
                    terms.push_back(lemma);
                }
            }

            if (!terms.empty()) {
                for (const auto& term : terms) {
                    tempIndex.addTerm(term, doc.id); 
                }

                tempIndex.incrementDocCount();
            } });

        std::cout << "\n[INIT] Finished. Total indexed docs: " << tempIndex.getTotalDocs() << std::endl;

        std::cout << "[INIT] Saving " << INDEX_FILE << "..." << std::endl;
        tempIndex.save(INDEX_FILE);

        std::cout << "[INIT] Saving " << URLS_FILE << "..." << std::endl;
        saveUrls(URLS_FILE, docUrls);

        std::cout << "[INIT] Exporting frequency statistics..." << std::endl;
        tempIndex.exportFrequencyStats("zipf_data.csv");
    }
    else
    {
        // Если индекс уже есть, просто подгружаем URL
        if (std::filesystem::exists(URLS_FILE))
        {
            loadUrls(URLS_FILE, docUrls);
        }
    }

    // Булев индекс
    if (useBooleanMode && !std::filesystem::exists(BOOLEAN_INDEX_FILE))
    {
        std::cout << "[INIT] Boolean mode requested. Converting index..." << std::endl;

        InvertedIndex tempInverted;
        if (!tempInverted.load(INDEX_FILE))
        {
            std::cerr << "Error: Failed to load index.bin." << std::endl;
            return 1;
        }

        tempInverted.exportToBooleanIndex(BOOLEAN_INDEX_FILE);
        std::cout << "[INIT] Conversion complete." << std::endl;
    }

    std::cout << "=== Initialization Complete ===\n"
              << std::endl;

    // Режим поиска: белев или tf-idf

    if (useBooleanMode)
    {
        std::cout << "Mode: BOOLEAN SEARCH" << std::endl;

        BooleanIndex booleanIndex;
        if (!booleanIndex.load(BOOLEAN_INDEX_FILE))
            return 1;

        std::cout << "\n> ";
        std::string query;
        while (std::getline(std::cin, query) && query != "exit")
        {
            std::vector<uint32_t> results = queryParser.parseBoolean(query, booleanIndex);

            if (results.empty())
                std::cout << "No documents found." << std::endl;
            else
            {
                for (size_t i = 0; i < std::min(results.size(), (size_t)10); ++i)
                {
                    uint32_t id = results[i];
                    std::string url = (id < docUrls.size()) ? docUrls[id] : "UNKNOWN";
                    std::cout << i + 1 << ". " << url << std::endl;
                }
            }
            std::cout << "\n> ";
        }
    }
    else
    {
        std::cout << "Mode: RANKING SEARCH (TF-IDF)" << std::endl;

        InvertedIndex invertedIndex;
        if (!invertedIndex.load(INDEX_FILE))
            return 1;

        // Проверка на случай битого индекса
        if (invertedIndex.getTotalDocs() == 0)
        {
            std::cerr << "Error: Index contains 0 documents! Please delete index.bin and re-run." << std::endl;
            return 1;
        }

        std::string query;
        std::cout << "> ";
        while (std::getline(std::cin, query) && query != "exit")
        {
            std::vector<std::string> terms = queryParser.parseTerms(query);
            std::vector<SearchResult> results = Scorer::search(terms, invertedIndex, nullptr);

            if (results.empty())
                std::cout << "Nothing found." << std::endl;
            else
            {
                for (size_t i = 0; i < std::min(results.size(), (size_t)10); ++i)
                {
                    uint32_t id = results[i].docId;
                    std::string url = (id < docUrls.size()) ? docUrls[id] : "UNKNOWN";
                    std::cout << i + 1 << ". [" << results[i].score << "] " << url << std::endl;
                }
            }
            std::cout << "\n> ";
        }
    }

    return 0;
}