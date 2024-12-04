#include <iostream>
#include <fstream>
#include <memory>
#include <string>
#include "curl.h"
#include <sstream>
#include "json.hpp"

using namespace std;

// Интерфейс для обработки текста
class TextProcessor {
public:
    virtual ~TextProcessor() = default;
    virtual string process(const string& text) = 0;
};

// Базовый переводчик с использованием Yandex API
class BaseTranslator : public TextProcessor {
public:
    BaseTranslator(const string& apiKey) : apiKey(apiKey) {}

    string process(const string& text) override {
        return translateUsingYandexAPI(text);
    }

private:
    string apiKey;

    // Статическая функция для обработки данных ответа от сервера
    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
        ((string*)userp)->append((char*)contents, size * nmemb);
        return size * nmemb;
    }

    string translateUsingYandexAPI(const string& text) {
        CURL* curl;
        CURLcode res;
        string readBuffer;

        curl = curl_easy_init();
        if (curl) {
            ostringstream url;
            url << "https://translate.api.cloud.yandex.net/translate/v2/translate";

            // Формирование JSON-запроса
            string jsonPayload = R"({"targetLanguageCode": "en", "texts": [")" + text + R"("]})";

            struct curl_slist* headers = nullptr;
            headers = curl_slist_append(headers, ("Authorization: Api-Key " + apiKey).c_str());
            headers = curl_slist_append(headers, "Content-Type: application/json");

            curl_easy_setopt(curl, CURLOPT_URL, url.str().c_str());
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonPayload.c_str());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);

            res = curl_easy_perform(curl);
            if (res != CURLE_OK) {
                cerr << "Ошибка запроса: " << curl_easy_strerror(res) << endl;
            }

            curl_easy_cleanup(curl);
            curl_slist_free_all(headers);
        }

        // Парсинг JSON-ответа
        auto jsonResponse = nlohmann::json::parse(readBuffer);
        return jsonResponse["translations"][0]["text"];
    }
};

// Базовый класс декоратора
class TextProcessorDecorator : public TextProcessor {
protected:
    unique_ptr<TextProcessor> component;
public:
    TextProcessorDecorator(unique_ptr<TextProcessor> comp) : component(move(comp)) {}
};

// Декоратор для логирования
class LoggingDecorator : public TextProcessorDecorator {
public:
    LoggingDecorator(unique_ptr<TextProcessor> comp) : TextProcessorDecorator(move(comp)) {}

    string process(const string& text) override {
        cout << "Лог: перевод текста...\n";
        return component->process(text);
    }
};

// Декоратор для подсчёта слов
class WordCountDecorator : public TextProcessorDecorator {
public:
    WordCountDecorator(unique_ptr<TextProcessor> comp) : TextProcessorDecorator(move(comp)) {}

    string process(const string& text) override {
        cout << "Количество слов: " << countWords(text) << "\n";
        return component->process(text);
    }

private:
    int countWords(const string& text) {
        int count = 0;
        bool inWord = false;
        for (char c : text) {
            if (isspace(c))
                inWord = false;
            else if (!inWord) {
                inWord = true;
                ++count;
            }
        }
        return count;
    }
};

// Декоратор для сохранения результата в файл
class SaveToFileDecorator : public TextProcessorDecorator {
public:
    SaveToFileDecorator(unique_ptr<TextProcessor> comp, const string& fileName)
        : TextProcessorDecorator(move(comp)), fileName(fileName) {}

    string process(const string& text) override {
        string translatedText = component->process(text);
        saveToFile(translatedText);
        return translatedText;
    }

private:
    string fileName;

    void saveToFile(const string& text) {
        ofstream outFile(fileName);
        if (outFile) {
            outFile << text;
            cout << "Текст сохранён в файл: " << fileName << "\n";
        }
    }
};

// Функция для чтения текста из файла
string readTextFromFile(const string& fileName) {
    ifstream inputFile(fileName);
    if (!inputFile) {
        cerr << "Не удалось открыть файл: " << fileName << endl;
        return "";
    }
    stringstream buffer;
    buffer << inputFile.rdbuf();
    return buffer.str();
}

int main() {
    string apiKey = ""; // ключ API

    // Чтение текста из файла
    string inputFileName = "input.txt";
    string text = readTextFromFile(inputFileName);
    if (text.empty()) {
        return 1; // Завершаем программу, если файл не открылся
    }

    unique_ptr<TextProcessor> translator = make_unique<BaseTranslator>(apiKey);
    translator = make_unique<LoggingDecorator>(move(translator));
    translator = make_unique<WordCountDecorator>(move(translator));
    translator = make_unique<SaveToFileDecorator>(move(translator), "output.txt");

    string translatedText = translator->process(text);
    cout << "Результат перевода: " << translatedText << "\n";

    return 0;
}
