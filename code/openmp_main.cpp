#include <ctime>
#include <fstream>
#include <libc.h>
#include <iostream>
#include <omp.h>
#include <random>
#include <vector>

// статусы болтунов
enum class STATUS {
    WAITING, RECEIVING, TALKING
};

// структура болтунов
struct Caller {
    int id, interlocutor_id;
    STATUS status;

    Caller(int id, STATUS status) {
        this->id = id;
        this->status = status;
        interlocutor_id = -1;
    }
};

// флаги, отвечающие за файловый ввод и остановку программы
bool file_stream = false, stopped = false;
// количество болтунов
int callers_count;
// вектор болтунов
std::vector<Caller> callers;
// вектор строк для вывода
std::vector<std::string> output;

// функция вывода строки (в консоль или в файл)
void print_string(const std::string &str) {
    if (!file_stream) {
        std::cout << str;
        std::cout.flush();
    } else {
        output.emplace_back(str);
    }
}

// функция, обрабатывающая все потоки
void process_threads() {
    // объявление потоков
#pragma omp parallel num_threads(3 * callers_count + 1)
    // если поток отвечает за инициирование звонков
    if (omp_get_thread_num() < callers_count) {
        while (!stopped) {
#pragma omp critical
            {
                int current_id = omp_get_thread_num() % callers_count;
                if (callers[current_id].status == STATUS::WAITING && (rand() % 5) > 2) {
                    int other_id;
                    do {
                        other_id = rand() % callers_count;
                    } while (other_id == current_id);
                    if (callers[other_id].status == STATUS::WAITING) {
                        callers[current_id].status = callers[other_id].status = STATUS::RECEIVING;
                        callers[current_id].interlocutor_id = other_id;
                        callers[other_id].interlocutor_id = current_id;
                    } else {
                        print_string(
                                "(failed call attempt) - (" + std::to_string(current_id) + " to " +
                                std::to_string(other_id) + ") at " +
                                std::to_string(clock() * 1. / CLOCKS_PER_SEC) + "\n");
                    }
                }
            }
            sleep(rand() % 10);
        }
    } else if (omp_get_thread_num() < 2 * callers_count) {
        // если поток отвечает за ответы на звонки
        while (!stopped) {
#pragma omp critical
            {
                int current_id = omp_get_thread_num() % callers_count;
                if (callers[current_id].status == STATUS::RECEIVING) {
                    int other_id = callers[current_id].interlocutor_id;
                    callers[current_id].status = callers[other_id].status = STATUS::TALKING;
                    callers[current_id].interlocutor_id = other_id;
                    callers[other_id].interlocutor_id = current_id;
                    print_string(
                            "(call started) - (" + std::to_string(std::min(current_id, other_id)) +
                            " and " + std::to_string(std::max(current_id, other_id)) + ") at " +
                            std::to_string(clock() * 1. / CLOCKS_PER_SEC) + "\n");
                }
            }
            sleep(rand() % 10);
        }
    } else if (omp_get_thread_num() < 3 * callers_count) {
        // если поток отвечает за прерывания или продолжение звонков
        while (!stopped) {
#pragma omp critical
            {
                int current_id = omp_get_thread_num() % callers_count;
                if (callers[current_id].status == STATUS::TALKING && (rand() % 20) > 13) {
                    int other_id = callers[current_id].interlocutor_id;
                    callers[current_id].status = callers[other_id].status = STATUS::WAITING;
                    callers[current_id].interlocutor_id = other_id;
                    callers[other_id].interlocutor_id = current_id;
                    print_string(
                            "(call ended) - (" + std::to_string(std::min(current_id, other_id)) +
                            " and " +
                            std::to_string(std::max(current_id, other_id)) + ") at " +
                            std::to_string(clock() * 1. / CLOCKS_PER_SEC) + "\n");
                }
            }
            sleep(rand() % 10);
        }
    } else {
        // если поток отвечает за прерывание программы
        while (std::cin.get() != 'q');
        stopped ^= true;
    }
}

int main(int argc, char *argv[]) {
    srand(time(nullptr));

    std::ifstream fin;
    std::ofstream fout;

    std::string out_file;

    // проверка правильности входных данных
    if (strcmp(argv[1], "-r") == 0) {
        if (argc != 4 && argc != 5) {
            std::cout << "Incorrect arguments count!";
            return 0;
        }
        int lower, upper;
        lower = atoi(argv[2]);
        upper = atoi(argv[3]);
        if (lower > upper) {
            std::cout << "Incorrect random range!";
            return 0;
        }
        callers_count = lower + (rand() % (upper - lower + 1));
        if (argc == 5) {
            file_stream = true;
            out_file = argv[4];
        }
        print_string("Generated number: " + std::to_string(callers_count) + "\n");
    } else if (strcmp(argv[1], "-f") == 0) {
        if (argc != 4) {
            std::cout << "Incorrect arguments count!";
            return 0;
        }
        file_stream = true;
        fin.open(argv[2]);
        if (!fin.is_open()) {
            std::cout << "Can't access input file!";
            return 0;
        }
        fin >> callers_count;
        fin.close();
        out_file = argv[3];
    } else if (strcmp(argv[1], "-c") == 0) {
        if (argc != 3 && argc != 4) {
            std::cout << "Incorrect arguments count!";
            return 0;
        }
        callers_count = atoi(argv[2]);
        if (argc == 4) {
            file_stream = true;
            out_file = argv[3];
        }
    } else {
        std::cout << "Incorrect input!";
        return 0;
    }
    callers.reserve(callers_count);

    // создание объектов болтунов
    for (int i = 0; i < callers_count; i++) {
        callers[i] = Caller(i, STATUS::WAITING);
    }

    process_threads();

    // вывод в файл (если нужно)
    if (file_stream) {
        fout.open(out_file);
        if (!fout.is_open()) {
            std::cout << "Can't access output file!";
            return 0;
        }
        for (const auto &str : output) {
            fout << str;
        }
        fout.flush();
        fout.close();
    }
    return 0;
}
