#include "ranking/Scorer.hpp"

std::vector<SearchResult> Scorer::search(
    const std::vector<std::string> &queryTerms,
    InvertedIndex &index,
    const std::vector<uint32_t> *allowedDocIds)
{
    HashMap<uint32_t, double> docScores;
    size_t N = index.getTotalDocs();

    for (const auto &term : queryTerms)
    {
        auto postings = index.getPostings(term);
        if (!postings)
            continue;

        double idf = std::log((double)N / (double)postings->size());

        for (const auto &p : *postings)
        {
            if (allowedDocIds != nullptr)
            {
                if (!std::binary_search(allowedDocIds->begin(), allowedDocIds->end(), p.docId))
                {
                    continue;
                }
            }

            double tf = (double)p.termFrequency;
            double score = tf * idf;

            double *currentScore = docScores.get(p.docId);
            if (currentScore)
            {
                *currentScore += score;
            }
            else
            {
                docScores.insert(p.docId, score);
            }
        }
    }

    std::vector<SearchResult> results;

    docScores.traverse([&](const uint32_t &docId, const double &score)
                       { results.push_back({docId, score}); });

    std::sort(results.begin(), results.end(), [](const SearchResult &a, const SearchResult &b)
              { return a.score > b.score; });

    return results;
}