#ifndef SCORER_HPP
#define SCORER_HPP

#include "../core/InvertedIndex.hpp"
#include <vector>
#include <cmath>
#include <algorithm>

struct SearchResult
{
    uint32_t docId;
    double score;
};

class Scorer
{
public:
    static std::vector<SearchResult> search(
        const std::vector<std::string> &queryTerms,
        InvertedIndex &index,
        const std::vector<uint32_t> *allowedDocIds = nullptr);
};

#endif