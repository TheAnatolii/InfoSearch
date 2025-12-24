#ifndef HTML_PARSER_HPP
#define HTML_PARSER_HPP

#include <string>
#include <vector>
#include <gumbo.h>

class HtmlParser
{
public:
    static std::string getCleanText(const std::string &html)
    {
        // Парсим HTML
        GumboOutput *output = gumbo_parse(html.c_str());

        std::string result = "";
        extractText(output->root, result);

        // Очищаем память, выделенную Gumbo
        gumbo_destroy_output(&kGumboDefaultOptions, output);

        return result;
    }

private:
    // Рекурсивная функция для обхода DOM-дерева
    static void extractText(GumboNode *node, std::string &text)
    {
        if (node->type == GUMBO_NODE_TEXT)
        {
            // Если это текстовый узел, добавляем его содержимое
            text.append(node->v.text.text);
            text.append(" ");
            return;
        }

        // Если это элементы, содержимое которых нам НЕ нужно (скрипты и стили)
        if (node->type == GUMBO_NODE_ELEMENT &&
            (node->v.element.tag == GUMBO_TAG_SCRIPT ||
             node->v.element.tag == GUMBO_TAG_STYLE))
        {
            return;
        }

        // Рекурсивно обходим дочерние элементы
        if (node->type == GUMBO_NODE_ELEMENT || node->type == GUMBO_NODE_DOCUMENT)
        {
            GumboVector *children = &node->v.element.children;
            for (unsigned int i = 0; i < children->length; ++i)
            {
                extractText(static_cast<GumboNode *>(children->data[i]), text);
            }
        }
    }
};

#endif