#ifndef TOKENIZER_HPP
#define TOKENIZER_HPP

#include <string>
#include <vector>
#include <locale>
#include <codecvt>
#include <algorithm>

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

class Tokenizer
{
public:
    static std::vector<std::string> tokenize(const std::string &text)
    {
        std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
        std::wstring wtext;

        try
        {
            wtext = converter.from_bytes(text);
        }
        catch (...)
        {
            return {};
        }

        std::vector<std::string> tokens;
        std::wstring current;

        for (wchar_t wc : wtext)
        {

            if (wc >= L'A' && wc <= L'Z')
            {
                wc += 32;
            }

            else if (wc >= 0x0410 && wc <= 0x042F)
            {
                wc += 0x20;
            }

            else if (wc == 0x0401)
            {
                wc = 0x0451;
            }

            // Считаем символом всё, что в диапазонах букв или цифр
            bool isAlpha = (wc >= L'a' && wc <= L'z') ||     // Eng
                           (wc >= L'0' && wc <= L'9') ||     // Digits
                           (wc >= 0x0430 && wc <= 0x044F) || // Rus a-я
                           (wc == 0x0451);                   // ё

            if (isAlpha)
            {
                current += wc;
            }
            else
            {
                if (!current.empty())
                {
                    tokens.push_back(converter.to_bytes(current));
                    current.clear();
                }
            }
        }

        if (!current.empty())
        {
            tokens.push_back(converter.to_bytes(current));
        }

        return tokens;
    }
};

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

#endif