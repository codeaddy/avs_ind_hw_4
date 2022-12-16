#include <ctime>
#include <fstream>
#include <libc.h>
#include <iostream>
#include <pthread.h>
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
// мьютекс
pthread_mutex_t mutex;

// функция вывода строки (в консоль или в файл)
void print_string(const std::string &str) {
    if (!file_stream) {
        std::cout << str;
        std::cout.flush();
    } else {
        output.emplace_back(str);
    }
}

// функция, инициирующая звонок
void *process_call(void *args) {
    // номер текущего болтуна
    int current_id = *(int *) args;
    while (!stopped) {
        pthread_mutex_lock(&mutex);
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
            pthread_mutex_unlock(&mutex);
            sleep(rand() % 10);
        } else {
            pthread_mutex_unlock(&mutex);
            sleep(1);
        }
    }
}

// функция, обрабатывающая начало звонков
void *process_answer(void *args) {
    // номер текущего болтуна
    int current_id = *(int *) args;
    while (!stopped) {
        pthread_mutex_lock(&mutex);
        if (callers[current_id].status == STATUS::RECEIVING) {
            int other_id = callers[current_id].interlocutor_id;
            callers[current_id].status = callers[other_id].status = STATUS::TALKING;
            callers[current_id].interlocutor_id = other_id;
            callers[other_id].interlocutor_id = current_id;
            print_string(
                    "(call started) - (" + std::to_string(std::min(current_id, other_id)) +
                    " and " + std::to_string(std::max(current_id, other_id)) + ") at " +
                    std::to_string(clock() * 1. / CLOCKS_PER_SEC) + "\n");
            pthread_mutex_unlock(&mutex);
            sleep(rand() % 10);
        } else {
            pthread_mutex_unlock(&mutex);
            sleep(1);
        }
    }
}

// функция, обеспечивающая прерывание или продолжение звонка
void *process_talking(void *args) {
    // номер текущего болтуна
    int current_id = *(int *) args;
    while (!stopped) {
        pthread_mutex_lock(&mutex);
        if (callers[current_id].status == STATUS::TALKING && (rand() % 20) > 13) {
            int other_id = callers[current_id].interlocutor_id;
            callers[current_id].status = callers[other_id].status = STATUS::WAITING;
            callers[current_id].interlocutor_id = other_id;
            callers[other_id].interlocutor_id = current_id;
            print_string(
                    "(call ended) - (" + std::to_string(std::min(current_id, other_id)) + " and " +
                    std::to_string(std::max(current_id, other_id)) + ") at " +
                    std::to_string(clock() * 1. / CLOCKS_PER_SEC) + "\n");
            pthread_mutex_unlock(&mutex);
            sleep(rand() % 10);
        } else {
            pthread_mutex_unlock(&mutex);
            sleep(1);
        }
    }
}

// функция, ответственная за отслеживание ввода 'q'
void *process_key(void *args) {
    while (std::cin.get() != 'q');
    stopped ^= true;
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
    }
    callers.reserve(callers_count);

    // создание объектов болтунов
    for (int i = 0; i < callers_count; i++) {
        callers[i] = Caller(i, STATUS::WAITING);
    }

    // инициализация мьютекса
    pthread_mutex_init(&mutex, nullptr);

    // объявление потоков
    std::vector<pthread_t> threads(3 * callers_count);
    pthread_t key_thread;

    // запуск потока отслеживания кнопки
    pthread_create(&key_thread, nullptr, process_key, nullptr);

    // запуск потоков с функциями для каждого болтуна
    for (size_t i = 0; i < callers_count; i++) {
        pthread_create(&threads[i], nullptr, process_call, (void *) &callers[i].id);
    }

    for (size_t i = 0; i < callers_count; i++) {
        pthread_create(&threads[callers_count + i], nullptr, process_answer,
                       (void *) &callers[i].id);
    }

    for (size_t i = 0; i < callers_count; i++) {
        pthread_create(&threads[callers_count * 2 + i], nullptr, process_talking,
                       (void *) &callers[i].id);
    }

    // ждем ввода 'q' для прерывания программы
    pthread_join(key_thread, nullptr);

    // уничтожаем мьютекс
    pthread_mutex_destroy(&mutex);

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
