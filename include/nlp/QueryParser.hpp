#ifndef QUERY_PARSER_HPP
#define QUERY_PARSER_HPP

#include <string>
#include <vector>
#include <stack>
#include <sstream>
#include <algorithm>
#include <set>
#include "../core/BooleanIndex.hpp"
#include "Lemmatizer.hpp"
#include "Tokenizer.hpp"

class QueryParser
{
private:
    enum TokenType
    {
        WORD,
        AND,
        OR,
        NOT,
        LPAREN,
        RPAREN
    };

    struct Token
    {
        std::string value;
        TokenType type;
        int precedence;
    };

    Lemmatizer &lemmatizer;

    // Хелперы для парсинга операторов
    bool tryParseOperator(const std::string &raw, TokenType &type, int &prec)
    {
        if (raw == "ИЛИ" || raw == "или" || raw == "|")
        {
            type = OR;
            prec = 1;
            return true;
        }
        if (raw == "&" || raw == "&&" || raw == "И" || raw == "и")
        {
            type = AND;
            prec = 2;
            return true;
        }
        if (raw == "!" || raw == "НЕ" || raw == "не")
        {
            type = NOT;
            prec = 3;
            return true;
        }
        if (raw == "(")
        {
            type = LPAREN;
            prec = 0;
            return true;
        }
        if (raw == ")")
        {
            type = RPAREN;
            prec = 0;
            return true;
        }
        return false;
    }

    // Булевы операции над списками ID
    std::vector<uint32_t> opAND(const std::vector<uint32_t> &a, const std::vector<uint32_t> &b)
    {
        std::vector<uint32_t> res;
        std::set_intersection(a.begin(), a.end(), b.begin(), b.end(), std::back_inserter(res));
        return res;
    }

    std::vector<uint32_t> opOR(const std::vector<uint32_t> &a, const std::vector<uint32_t> &b)
    {
        std::vector<uint32_t> res;
        std::set_union(a.begin(), a.end(), b.begin(), b.end(), std::back_inserter(res));
        return res;
    }

    std::vector<uint32_t> opNOT(const std::vector<uint32_t> &a, size_t totalDocs)
    {
        std::vector<uint32_t> res;
        res.reserve(totalDocs - a.size());
        size_t a_idx = 0;
        for (uint32_t docId = 0; docId < totalDocs; ++docId)
        {
            if (a_idx < a.size() && a[a_idx] == docId)
            {
                a_idx++;
            }
            else
            {
                res.push_back(docId);
            }
        }
        return res;
    }

public:
    QueryParser(Lemmatizer &lemm) : lemmatizer(lemm) {}

    std::vector<std::string> parseTerms(const std::string &query)
    {
        std::vector<std::string> cleanTerms;
        std::vector<std::string> rawTokens = Tokenizer::tokenize(query);
        for (const auto &t : rawTokens)
        {
            std::string lemma = lemmatizer.lemmatize(t);
            if (!lemma.empty())
                cleanTerms.push_back(lemma);
        }
        return cleanTerms;
    }

    std::vector<uint32_t> parseBoolean(const std::string &query, BooleanIndex &index)
    {
        std::vector<Token> tokens;

        std::string processedQuery = query;
        for (const std::string &symb : {"(", ")", "!"})
        {
            size_t pos = 0;
            while ((pos = processedQuery.find(symb, pos)) != std::string::npos)
            {
                processedQuery.replace(pos, symb.length(), " " + symb + " ");
                pos += symb.length() + 2;
            }
        }

        std::stringstream ss(processedQuery);
        std::string segment;

        // Токенизация
        while (ss >> segment)
        {
            TokenType type;
            int prec;
            if (tryParseOperator(segment, type, prec))
            {
                tokens.push_back({segment, type, prec});
            }
            else
            {
                std::vector<std::string> words = Tokenizer::tokenize(segment);
                for (const auto &w : words)
                {
                    std::string lemma = lemmatizer.lemmatize(w);
                    if (!lemma.empty())
                        tokens.push_back({lemma, WORD, 0});
                }
            }
        }

        // Shunting-yard (Infix -> RPN)
        std::vector<Token> rpn;
        std::stack<Token> opStack;

        for (const auto &token : tokens)
        {
            if (token.type == WORD)
                rpn.push_back(token);
            else if (token.type == LPAREN)
                opStack.push(token);
            else if (token.type == RPAREN)
            {
                while (!opStack.empty() && opStack.top().type != LPAREN)
                {
                    rpn.push_back(opStack.top());
                    opStack.pop();
                }
                if (!opStack.empty())
                    opStack.pop();
            }
            else
            {
                while (!opStack.empty() && opStack.top().type != LPAREN &&
                       opStack.top().precedence >= token.precedence)
                {
                    rpn.push_back(opStack.top());
                    opStack.pop();
                }
                opStack.push(token);
            }
        }
        while (!opStack.empty())
        {
            rpn.push_back(opStack.top());
            opStack.pop();
        }

        // Вычисление RPN
        std::stack<std::vector<uint32_t>> evalStack;

        for (const auto &token : rpn)
        {
            if (token.type == WORD)
            {
                std::vector<uint32_t> docs;
                auto ptr = index.getDocIds(token.value);
                if (ptr)
                    docs = *ptr; // Копируем список ID
                evalStack.push(docs);
            }
            else if (token.type == NOT)
            {
                if (evalStack.empty())
                    continue;
                auto a = evalStack.top();
                evalStack.pop();
                evalStack.push(opNOT(a, index.getTotalDocs()));
            }
            else
            { // AND, OR
                if (evalStack.size() < 2)
                    continue;
                auto b = evalStack.top();
                evalStack.pop();
                auto a = evalStack.top();
                evalStack.pop();
                if (token.type == AND)
                    evalStack.push(opAND(a, b));
                else
                    evalStack.push(opOR(a, b));
            }
        }

        return evalStack.empty() ? std::vector<uint32_t>{} : evalStack.top();
    }
};

#endif