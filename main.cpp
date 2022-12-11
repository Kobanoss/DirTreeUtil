#include <iostream>
#include <cinttypes>
#include <filesystem>
#include <algorithm>
#include <cstring>
#include <vector>
#include <thread>
#include <chrono>
#include <atomic>

#include "Node.cpp"

namespace fs = std::filesystem;

std::atomic<bool> exit_flag;    // Атомик, фиксирующий нахождение значения
std::string detected_path;      // Найденный путь

namespace thread_func {
    void findFileByName(Node *root, std::string name) {
        // Если нашли среди дочерних файлов нужное название - выводим, 
        // предусмотрен одновременный вход в условие разными потоками, если есть совпадающие названия файлов
        if (strcmp(name.c_str(), root->path.filename().string().c_str()) == 0 && !root->is_dir) {
            if (exit_flag.load())
                return;
            detected_path = root->path.string();
            exit_flag.store(true);
        }



        // Если директория, то создает потоки для всех ее дочерних элементов, потоки сразу отделяем от основного
        if (root->is_dir) {
            for (Node* object: root->children) {
                if (exit_flag.load())
                    return;
                std::thread([object, name]() { thread_func::findFileByName(object, name); }).detach();
            }
        }
    }
}

// Аналогичная(стартовая) функция, формирующая потоки
void findFileByName(Node *root, const std::string &name) {

    if (strcmp(name.c_str(), root->path.filename().string().c_str()) == 0 && !root->is_dir) {
        if (exit_flag.load())
            return;
        exit_flag.store(true);
        detected_path = root->path.string();
        return;
    }


    if (root->is_dir)
        for (Node* object: root->children) {
            if (exit_flag.load())
                return;
            std::thread([object, name]() { thread_func::findFileByName(object, name); }).detach();
        }
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
    for (const auto &entry: fs::directory_iterator(pathToScan)) {
        auto filename = entry.path().filename().string();

        root->name = pathToScan.filename().string();
        root->path = pathToScan;
        root->is_dir = true;

        Node *new_node = new Node();
        new_node->name = filename;
        new_node->depth = root->depth + 1;
        new_node->path = entry.path();

        root->addChild(new_node);

        if (entry.is_directory()) {
            new_node->is_dir = true;
            getDirectoryTree(new_node, entry);
        } else if (entry.is_regular_file()) {
            new_node->size = entry.file_size();
        } else {
            new_node->is_other = true;
        }
    }
}

// Рекурсивная очистка дерева
void clearTree(Node *root) {
    if (!root->is_dir) {
        delete root;
        return;
    }

    for (auto &ptr: root->children)
        clearTree(ptr);

    delete root;
}

int main() {
    // Инициализируем дерево согласно указаному пути в системе
    // Выбросит ошибку, если путь не существует
    Node *baseRoot = new Node();
    
    // Заполнение дерева
    getDirectoryTree(baseRoot, fs::path("/home/kondrativvo/DocSup/TestDir"));

    // Вывод дерева
    printDirectoryTree(baseRoot);

    // Устанавливаем стартовые значения для флагов и итогового пути
    exit_flag.store(false);
    detected_path = "";
    
    // Начинаем поиск по названию
    findFileByName(baseRoot, "1.txt");

    // Пока не активирован флаг, усыпляем текущий поток
    while (!exit_flag.load()) {
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }

    // Выводим найденный путь
    std::cout << "Path to file: " << detected_path << std::endl;

    // Очищаем дерево
    clearTree(baseRoot);
    return 0;
}
