#ifndef LEMMATIZER_HPP
#define LEMMATIZER_HPP

#include <string>
#include <vector>
#include "libstemmer.h"

class Lemmatizer
{
private:
    struct sb_stemmer *stemmer;

public:
    Lemmatizer()
    {
        // Инициализируем для русского языка (UTF-8)
        stemmer = sb_stemmer_new("russian", "UTF_8");
    }

    ~Lemmatizer()
    {
        if (stemmer)
            sb_stemmer_delete(stemmer);
    }

    std::string lemmatize(const std::string &word)
    {
        const sb_symbol *stemmed = sb_stemmer_stem(stemmer,
                                                   reinterpret_cast<const sb_symbol *>(word.c_str()),
                                                   word.length());
        return std::string(reinterpret_cast<const char *>(stemmed));
    }
};

#endif