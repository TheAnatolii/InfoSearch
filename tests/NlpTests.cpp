#include <gtest/gtest.h>
#include "nlp/Tokenizer.hpp"
#include "nlp/HtmlParser.hpp"

// ==========================================
// Тесты для Tokenizer
// ==========================================

// 1. Базовый тест: разбиение по пробелам и нижний регистр (английский)
TEST(TokenizerTest, SimpleEnglish)
{
    std::string text = "Hello World";
    auto tokens = Tokenizer::tokenize(text);

    ASSERT_EQ(tokens.size(), 2);
    EXPECT_EQ(tokens[0], "hello");
    EXPECT_EQ(tokens[1], "world");
}

// 2. Тест на знаки препинания: они должны исчезнуть
TEST(TokenizerTest, RemovesPunctuation)
{
    std::string text = "Hello, world! It's me.";
    // Ожидаем: "hello", "world", "it", "s", "me"
    // (так как "'" не входит в isAlpha, слово "It's" разобьется на "it" и "s")

    auto tokens = Tokenizer::tokenize(text);

    ASSERT_GE(tokens.size(), 3);
    EXPECT_EQ(tokens[0], "hello");
    EXPECT_EQ(tokens[1], "world");
    // Проверяем, что точек и запятых нет внутри токенов
    for (const auto &t : tokens)
    {
        EXPECT_EQ(t.find(','), std::string::npos);
        EXPECT_EQ(t.find('.'), std::string::npos);
    }
}

// 3. Тест на русский язык: проверка кодировки и lowercasing
TEST(TokenizerTest, RussianSupport)
{
    // UTF-8 строки
    std::string text = "Привет МИР";
    auto tokens = Tokenizer::tokenize(text);

    ASSERT_EQ(tokens.size(), 2);
    EXPECT_EQ(tokens[0], "привет");
    EXPECT_EQ(tokens[1], "мир");
}

// 4. Тест на букву Ё (граничный случай в твоем коде 0x0401 -> 0x0451)
TEST(TokenizerTest, HandlesYoLetter)
{
    std::string text = "Ёлка ёж";
    auto tokens = Tokenizer::tokenize(text);

    ASSERT_EQ(tokens.size(), 2);
    EXPECT_EQ(tokens[0], "ёлка");
    EXPECT_EQ(tokens[1], "ёж");
}

// 5. Тест на цифры (они включены в твой isAlpha)
TEST(TokenizerTest, KeepsDigits)
{
    std::string text = "User 12345 id";
    auto tokens = Tokenizer::tokenize(text);

    ASSERT_EQ(tokens.size(), 3);
    EXPECT_EQ(tokens[1], "12345");
}

// ==========================================
// Тесты для HtmlParser
// ==========================================

// 6. Базовая очистка тегов
TEST(HtmlParserTest, RemovesSimpleTags)
{
    std::string html = "<h1>Title</h1><p>Paragraph</p>";
    std::string clean = HtmlParser::getCleanText(html);

    // Твой код заменяет теги на пробелы, поэтому ожидаем пробелы
    // Проверяем, что тегов нет
    EXPECT_EQ(clean.find("<h1>"), std::string::npos);
    EXPECT_EQ(clean.find("</p>"), std::string::npos);

    // Проверяем, что контент остался
    EXPECT_NE(clean.find("Title"), std::string::npos);
    EXPECT_NE(clean.find("Paragraph"), std::string::npos);
}

// 7. Проверка предотвращения склеивания слов
// Если код просто удаляет теги без замены на пробел, "One</div><div>Two" стало бы "OneTwo"
TEST(HtmlParserTest, PreventsWordConcatenation)
{
    std::string html = "<div>One</div><div>Two</div>";
    std::string clean = HtmlParser::getCleanText(html);

    // Твой regex_replace заменяет match на " ", поэтому между словами должны быть пробелы
    // Мы токенизируем результат, чтобы не считать точное кол-во пробелов
    auto tokens = Tokenizer::tokenize(clean);

    ASSERT_EQ(tokens.size(), 2);
    EXPECT_EQ(tokens[0], "one");
    EXPECT_EQ(tokens[1], "two");
}

// 8. Удаление скриптов и стилей (самое важное для поисковика)
TEST(HtmlParserTest, RemovesScriptsAndStyles)
{
    std::string html = "Start <script>var x = 100; alert('hack');</script> End";
    std::string clean = HtmlParser::getCleanText(html);

    // Контент внутри скрипта должен исчезнуть
    EXPECT_EQ(clean.find("var x"), std::string::npos);
    EXPECT_EQ(clean.find("hack"), std::string::npos);

    // "Start" и "End" должны остаться
    EXPECT_NE(clean.find("Start"), std::string::npos);
    EXPECT_NE(clean.find("End"), std::string::npos);
}

// 9. Очистка HTML-сущностей
TEST(HtmlParserTest, ReplacesEntities)
{
    std::string html = "Fish&nbsp;Chips"; // &nbsp; -> пробел
    std::string clean = HtmlParser::getCleanText(html);

    EXPECT_EQ(clean.find("&nbsp;"), std::string::npos);

    // Проверяем через токенайзер, что слова разделены
    auto tokens = Tokenizer::tokenize(clean);
    ASSERT_EQ(tokens.size(), 2);
    EXPECT_EQ(tokens[0], "fish");
    EXPECT_EQ(tokens[1], "chips");
}

// 10. Пустая строка или строка без тегов
TEST(HtmlParserTest, HandlesEmptyAndPlainInput)
{
    std::string empty = "";
    EXPECT_EQ(HtmlParser::getCleanText(empty), "");

    std::string plain = "Just text";
    EXPECT_EQ(HtmlParser::getCleanText(plain), "Just text ");
}