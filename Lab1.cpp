#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>

using namespace std;

const int YEARS = 10;
const int FOOD_PER_PERSON = 20;
const int ACRES_PER_PERSON = 10;
const int MIN_PRICE = 17, MAX_PRICE = 26;
const int MIN_YIELD = 1, MAX_YIELD = 6;
const int PLAGUE_CHANCE = 15, STARVATION_LIMIT = 45;
const int MAX_NEWCOMERS = 50;
const int MIGRATION_YIELD = 5, MIGRATION_DIVISOR = 600;
const double SEED_PER_ACRE = 0.5, RAT_SHARE = 0.07;
const double BAD_PERCENT = 33, FAIR_PERCENT = 10, GOOD_PERCENT = 3;
const double BAD_LAND = 7, FAIR_LAND = 9, GOOD_LAND = 10;
const char* SAVE_FILE = "hammurabi.save";

struct State {
    int year = 1, population = 100, land = 1000;
    double grain = 2800;
    int starved = 0, newcomers = 0;
    bool plague = false;
    int harvest = 0, yield = 0, rats = 0, price = 0;
    double percentSum = 0;
};

struct Orders {
    int buy, sell, food, seed;
};

mt19937 rng(random_device{}());

int randomInt(int left, int right) {
    return uniform_int_distribution<int>(left, right)(rng);
}

string readLine(const string& prompt) {
    cout << prompt;
    string line;
    if (!getline(cin, line)) throw runtime_error("Ввод завершён.");
    return line;
}

int readAmount(const string& prompt, double limit, const string& reason) {
    while (true) {
        istringstream input(readLine(prompt));
        int value;
        string extra;
        if (!(input >> value) || (input >> extra) || value < 0)
            cout << "Введите неотрицательное целое число.\n";
        else if (value > limit)
            cout << reason << " Допустимо не более " << floor(limit) << ".\n";
        else
            return value;
    }
}

bool saveGame(const State& s) {
    ofstream out(SAVE_FILE);
    out << setprecision(17) << "HAMMURABI_V2\n";
    out << s.year << ' ' << s.population << ' ' << s.land << ' ' << s.grain << ' '
        << s.starved << ' ' << s.newcomers << ' ' << s.plague << ' '
        << s.harvest << ' ' << s.yield << ' ' << s.rats << ' '
        << s.price << ' ' << s.percentSum << '\n';
    return out.good();
}

bool loadGame(State& s) {
    ifstream in(SAVE_FILE);
    string version;
    State loaded;
    if (!(in >> version) || version != "HAMMURABI_V2") return false;
    if (!(in >> loaded.year >> loaded.population >> loaded.land >> loaded.grain
          >> loaded.starved >> loaded.newcomers >> loaded.plague
          >> loaded.harvest >> loaded.yield >> loaded.rats
          >> loaded.price >> loaded.percentSum)) return false;
    if (loaded.year < 1 || loaded.year > YEARS || loaded.population <= 0
        || !isfinite(loaded.grain) || loaded.grain < 0 || loaded.land < 0
        || loaded.price < MIN_PRICE || loaded.price > MAX_PRICE
        || !isfinite(loaded.percentSum) || loaded.percentSum < 0) return false;
    s = loaded;
    return true;
}

State startGame() {
    State s;
    ifstream file(SAVE_FILE);
    if (file.good()) {
        string answer = readLine("Найдено сохранение. Продолжить (y) или начать заново (n)? ");
        if (answer == "y" || answer == "Y") {
            if (loadGame(s)) return s;
            cout << "Сохранение устарело или повреждено. Начата новая игра.\n";
        }
    }
    s.price = randomInt(MIN_PRICE, MAX_PRICE);
    return s;
}

void printReport(const State& s) {
    if (s.year > YEARS)
        cout << "\nРезультаты десятого года.\n";
    else
        cout << "\nГод " << s.year << ".\n";
    if (s.year > 1) {
        if (s.starved > 0) cout << "Умерли от голода: " << s.starved << ".\n";
        if (s.newcomers > 0) cout << "Прибыли в город: " << s.newcomers << ".\n";
        cout << "Чума: " << (s.plague ? "была" : "не было") << ".\n";
        cout << "Урожай: " << s.harvest << " бушелей, по " << s.yield << " с акра.\n";
        cout << "Крысы съели " << s.rats << " бушелей.\n";
    }
    cout << "Население: " << s.population << ".\n";
    cout << "Пшеница: " << s.grain << " бушелей. Земля: " << s.land << " акров.\n";
    if (s.year <= YEARS) cout << "Цена акра: " << s.price << " бушелей.\n";
}

bool wantsToExit(const State& s) {
    while (true) {
        string answer = readLine("Enter — продолжить, q — сохранить и выйти: ");
        if (answer.empty()) return false;
        if (answer != "q" && answer != "Q")
            cout << "Нажмите Enter или введите q.\n";
        else if (saveGame(s)) {
            cout << "Игра сохранена.\n";
            return true;
        } else
            cout << "Не удалось записать сохранение. Можно повторить попытку.\n";
    }
}

Orders readOrders(const State& s) {
    Orders o;
    o.buy = readAmount("Сколько акров купить? ", s.grain / s.price,
                      "Не хватает пшеницы на покупку.");
    o.sell = readAmount("Сколько акров продать? ", s.land,
                       "У города нет столько земли.");
    double grain = s.grain + double(o.sell - o.buy) * s.price;
    o.food = readAmount("Сколько бушелей выделить на еду? ", grain,
                       "Не хватает пшеницы на питание.");
    double landLimit = double(s.land) + o.buy - o.sell;
    double workerLimit = double(s.population) * ACRES_PER_PERSON;
    double seedLimit = (grain - o.food) / SEED_PER_ACRE;
    double limit = min({landLimit, workerLimit, seedLimit});
    string reason = "Не хватает земли для посева.";
    if (limit == workerLimit) reason = "Не хватает жителей для обработки земли.";
    if (limit == seedLimit) reason = "Не хватает пшеницы на семена.";
    o.seed = readAmount("Сколько акров засеять? ", limit, reason);
    return o;
}

void harvestGrain(State& s, const Orders& o) {
    s.land += o.buy - o.sell;
    s.grain += double(o.sell - o.buy) * s.price;
    // Полбушеля на акр: нечётная площадь оставляет дробный запас.
    s.grain -= o.food + o.seed * SEED_PER_ACRE;
    s.yield = randomInt(MIN_YIELD, MAX_YIELD);
    s.harvest = o.seed * s.yield;
    s.grain += s.harvest;
    s.rats = randomInt(0, int(floor(s.grain * RAT_SHARE)));
    s.grain -= s.rats;
}

bool finishYear(State& s, const Orders& o) {
    harvestGrain(s, o);
    int population = s.population;
    int fed = o.food / FOOD_PER_PERSON;
    s.starved = max(0, population - fed);
    double percent = 100.0 * s.starved / population;
    s.percentSum += percent;
    s.population -= s.starved;
    if (percent > STARVATION_LIMIT || s.population == 0) {
        cout << "От голода умерли " << s.starved << " жителей. Вы проиграли.\n";
        return false;
    }
    // Формула из задания; дробную часть отбрасываем после расчёта.
    double migration = s.starved / 2.0;
    migration += (MIGRATION_YIELD - s.yield) * s.grain / MIGRATION_DIVISOR + 1;
    s.newcomers = int(clamp(migration, 0.0, double(MAX_NEWCOMERS)));
    s.population += s.newcomers;
    s.plague = randomInt(1, 100) <= PLAGUE_CHANCE;
    if (s.plague) s.population /= 2;
    if (s.population == 0) {
        cout << "После чумы в городе не осталось жителей. Вы проиграли.\n";
        return false;
    }
    ++s.year;
    // Цена уже подготовлена к началу года и сохраняется вместе с ним.
    if (s.year <= YEARS) s.price = randomInt(MIN_PRICE, MAX_PRICE);
    return true;
}

void printResults(const State& s) {
    double p = s.percentSum / YEARS;
    double land = double(s.land) / s.population;
    cout << fixed << setprecision(2);
    cout << "\nСреднегодовой процент умерших от голода: " << p << "%.\n";
    cout << "Акров на жителя: " << land << ".\n";
    if (p > BAD_PERCENT && land < BAD_LAND)
        cout << "Плохо: Из-за вашей некомпетентности в управлении, народ устроил бунт, "
                "и изгнал вас их города. Теперь вы вынуждены влачить жалкое существование "
                "в изгнании.\n";
    else if (p > FAIR_PERCENT && land < FAIR_LAND)
        cout << "Удовлетворительно: Вы правили железной рукой, подобно Нерону и Ивану "
                "Грозному. Народ вздохнул с облегчением, и никто больше не желает "
                "видеть вас правителем.\n";
    else if (p > GOOD_PERCENT && land < GOOD_LAND)
        cout << "Хорошо: Вы справились вполне неплохо, у вас, конечно, есть недоброжелатели, "
                "но многие хотели бы увидеть вас во главе города снова.\n";
    else
        cout << "Отлично: Фантастика! Карл Великий, Дизраэли и Джефферсон вместе "
                "не справились бы лучше.\n";
}

int main() {
    try {
        State state = startGame();
        while (state.year <= YEARS) {
            printReport(state);
            if (wantsToExit(state)) return 0;
            Orders orders = readOrders(state);
            if (!finishYear(state, orders)) return 0;
        }
        printReport(state);
        printResults(state);
    } catch (const runtime_error& error) {
        cout << '\n' << error.what() << '\n';
    }
}
