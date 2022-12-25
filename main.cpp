// Ключи для компиляции -lstdc++fs -pthread

#include <iostream>
#include <experimental/filesystem>
#include <algorithm>
#include <cstring>
#include <vector>
#include <thread>
#include <chrono>
#include <atomic>
#include <iomanip>

#include "Node.h"

#ifdef __unix__
    #define FROM "/"
#else
    #define FROM "C:/"
#endif

// Используем для надежности experimental,
// так как filesystem лишь после 17 вышел из него.
namespace fs = std::experimental::filesystem;


std::atomic<bool> exit_flag;        // Атомик, фиксирующий нахождение значения
std::string detected_path;          // Найденный путь

std::atomic<unsigned int> threads_counter;      // Счетчик потоков
std::atomic<unsigned int> max_threads;          // Максимальное число потоков

namespace thread_func {
    void findFileByName(Node *root, std::string name) {
        // Если нашли среди дочерних файлов нужное название - выводим,
        // предусмотрен одновременный вход в условие разными потоками, если есть совпадающие названия файлов
        if (strcmp(name.c_str(), root->path.filename().string().c_str()) == 0 && !root->is_dir) {
            bool tmp = exit_flag.exchange(true);                 // Обмениваем флаг, если нашли файл
            if (tmp) {                                              // Если уже был найден
                threads_counter--;  // Уменьшаем кол-во потоков и заканчиваем работу
                return;
            }
            detected_path = root->path.string();                    // Записываем путь до файла
            threads_counter--;      // Уменьшаем кол-во потоков и завершаем работу
            return;
        }



        // Если директория, то создает потоки для всех ее дочерних элементов, потоки сразу отделяем от основного
        if (root->is_dir) {
            for (Node *object: root->children) {
                if (exit_flag.load()) {                                     // Если файл уже найден
                    threads_counter--;      // Уменьшаем кол-во потоков и завершаем работу
                    return;
                }
                if (threads_counter.load() < max_threads.load()) {          // Если в нашем "пуле" есть доступные треды, то создаем новый
                    std::thread([object, name]() { thread_func::findFileByName(object, name); }).detach();
                    threads_counter++;      // И увеличиваем счетчик

                } else
                    findFileByName(object, name);                   // Иначе просто рекурсивно вызываем метод в рамках данного потока
            }
        }
        threads_counter--;                  // Уменьшаем кол-во потоков и завершаем работу потока
    }
}

// Аналогичная(стартовая) функция, формирующая потоки
void findFileByName(Node *root, const std::string &name) {

    if (strcmp(name.c_str(), root->path.filename().string().c_str()) == 0 && !root->is_dir) {
        bool tmp = exit_flag.exchange(true);
        if (tmp) {
            threads_counter--;
            return;
        }
        detected_path = root->path.string();
        threads_counter--;
        return;
    }


    if (root->is_dir)
        for (Node *object: root->children) {
            if (exit_flag.load()) {
                threads_counter--;
                return;
            }
            if (threads_counter.load() < max_threads.load()) {
                std::thread([object, name]() { thread_func::findFileByName(object, name); }).detach();
                threads_counter++;

            } else
                findFileByName(object, name);
        }
    threads_counter--;
}

// Вывод содержимого дерева
void printDirectoryTree(Node *object) {
    if (!object)
        return;

    if (object->is_dir) {
        std::cout << std::setw(object->depth * 3) << "" << object->name << " - DIR\n";
        for (const auto &child: object->children) {
            printDirectoryTree(child);
        }
        return;
    }

    if (object->is_other) {
        std::cout << std::setw(object->depth * 3) << "" << object->name << " - OTHER\n";
        return;
    }

    std::cout << std::setw(object->depth * 3) << "" << object->name << " - FILE(" << object->size << " bytes)\n";
}

// Заполнение структуры дерева с использлованием путей через std::filesystem
void getDirectoryTree(Node *root, const fs::path &pathToScan) {
    // Формируем итератор файловой системы
    for (const auto &entry: fs::directory_iterator(pathToScan)) {
        auto filename = entry.path().filename().string();

        // Создаем узел и заполняем все его поля

        root->name = pathToScan.filename().string();
        root->path = pathToScan;
        root->is_dir = true;

        Node *new_node = new Node();
        new_node->name = filename;
        new_node->depth = root->depth + 1;
        new_node->path = entry.path();

        root->addChild(new_node);

        if (is_directory(entry)) {
            new_node->is_dir = true;
            // Формируем итератор файловой системы рекурсивно если встретили папку
            getDirectoryTree(new_node, entry);
        } else if (is_regular_file(entry)) {
            new_node->size = file_size(entry);
        } else {
            new_node->is_other = true;
        }
    }
}

// Рекурсивная очистка дерева
void clearTree(Node *root) {
    if (!root->is_dir) {        // Если не директория, то просто удаляем файл
        delete root;
        return;
    }

    for (auto &ptr: root->children)
        clearTree(ptr);   // Если директория, то вызываем рекурсивное удаление содержимого

    delete root;                // После удаляем саму директорию
}

int main(int argc, char **argv) {
    if (argc < 2)                                   // Если доп аргументов меньше 1, то завершаем работу программы
        return 1;

    std::string path_from = std::string(FROM);   // Устанавливаем путь по умолчанию в зависимости от системы
    std::string filename;                           // Объявляем переменную под название искомого файла
    max_threads = 10;                               // Устанавливаем максимальное число потоков по умолчанию

    std::vector<std::string> args_v;        // Вектор аргументов

    if (argc > 2)
        for (int i = 2; i < argc; i++) {
            args_v.emplace_back(argv[i]);   // Заполняем вектор содержимым аргументов
        }
    filename = std::string(argv[1]);    // Записываем имя файла

    // Проходим по нашему вектору и ищем ключ пути
    auto it = std::find(args_v.begin(), args_v.end(), "--path");
    if (it != args_v.end()) {   // Если нашли не дойдя до конца вектора
        ++it;                   // То сдвигаем указатель на шаг вперед
        path_from = *it;        // Записываем путь
    }

    // Проходим по нашему вектору и ищем ключ потоков
    it = std::find(args_v.begin(), args_v.end(), "--num_threads");
    if (it != args_v.end()) {                   // Если нашли не дойдя до конца вектора
        ++it;                                   // То сдвигаем указатель на шаг вперед
        if (std::stoi(*it) > 0)            // И если значение больше 0
            max_threads = std::stoi(*it);  // Устанавливаем его новым числом потоков
    }

    // Устанавливаем стартовое число потоков
    threads_counter = 1;

    // Инициализируем дерево согласно указаному пути в системе.
    // Выбросит ошибку, если путь не существует
    Node *baseRoot = new Node();

    // Заполнение дерева
    getDirectoryTree(baseRoot, fs::path(path_from));

    // Вывод дерева
    printDirectoryTree(baseRoot);

    // Устанавливаем стартовые значения для флагов и итогового пути
    exit_flag.store(false);

    // Начинаем поиск по названию
    findFileByName(baseRoot, filename);

    std::this_thread::sleep_for(std::chrono::microseconds(500));

    // Пока не активирован флаг, усыпляем текущий поток
    while (!exit_flag.load() && threads_counter.load() > 1) {
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }

    if (!detected_path.empty()) {
        // Выводим найденный путь, если он не пустой
        std::cout << "Path to file: " << detected_path << std::endl;
    } else {
        // Выводим в противном случае, что файл не найден
        std::cout << "Can't find file!" << std::endl;
    }

    // Очищаем дерево
    clearTree(baseRoot);
    return 0;
}
