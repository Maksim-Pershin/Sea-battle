#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>
#include <vector>
#include <iostream>
#include <random>
#include <string>
#include <sstream>
#include <algorithm>
#include <cmath>

const int CELL_SIZE = 40;
const int MARGIN = 50;
const int GRID_OFFSET_X = MARGIN;
const int GRID_OFFSET_Y = MARGIN;
const int GRID_SIZE = 10;
const int WINDOW_WIDTH = 2 * MARGIN + 2 * GRID_SIZE * CELL_SIZE + MARGIN;
const int WINDOW_HEIGHT = MARGIN + GRID_SIZE * CELL_SIZE + MARGIN + 200;

enum class CellState {
    Empty,
    Ship,
    Hit,
    Miss,
    Destroyed
};

enum class GameState {
    PlayerTurn,
    ComputerTurn,
    PlayerWins,
    ComputerWins,
    ShipPlacement,
    DifficultySelection,
    Animation
};

enum class Difficulty {
    Easy,
    Medium,
    Hard
};

class Ship {
public:
    int size;
    bool horizontal;
    std::vector<std::pair<int, int>> positions;
    std::vector<bool> hits;

    Ship(int s, bool h, int x, int y) : size(s), horizontal(h) {
        for (int i = 0; i < size; ++i) {
            if (horizontal) {
                positions.emplace_back(x + i, y);
            }
            else {
                positions.emplace_back(x, y + i);
            }
            hits.push_back(false);
        }
    }

    bool isDestroyed() const {
        for (bool hit : hits) {
            if (!hit) return false;
        }
        return true;
    }
};

class BattleGrid {
private:
    std::vector<std::vector<CellState>> grid;
    std::vector<Ship> ships;

public:
    BattleGrid() {
        grid.resize(GRID_SIZE, std::vector<CellState>(GRID_SIZE, CellState::Empty));
    }

    void clear() {
        for (auto& row : grid) {
            for (auto& cell : row) {
                cell = CellState::Empty;
            }
        }
        ships.clear();
    }

    bool canPlaceShip(int x, int y, int size, bool horizontal) const {
        if (horizontal) {
            if (x + size > GRID_SIZE) return false;
            for (int i = x - 1; i <= x + size; ++i) {
                for (int j = y - 1; j <= y + 1; ++j) {
                    if (i >= 0 && i < GRID_SIZE && j >= 0 && j < GRID_SIZE) {
                        if (grid[j][i] != CellState::Empty) return false;
                    }
                }
            }
        }
        else {
            if (y + size > GRID_SIZE) return false;
            for (int i = x - 1; i <= x + 1; ++i) {
                for (int j = y - 1; j <= y + size; ++j) {
                    if (i >= 0 && i < GRID_SIZE && j >= 0 && j < GRID_SIZE) {
                        if (grid[j][i] != CellState::Empty) return false;
                    }
                }
            }
        }
        return true;
    }

    bool placeShip(int x, int y, int size, bool horizontal) {
        if (!canPlaceShip(x, y, size, horizontal)) return false;

        Ship ship(size, horizontal, x, y);
        ships.push_back(ship);

        for (const auto& pos : ship.positions) {
            grid[pos.second][pos.first] = CellState::Ship;
        }
        return true;
    }

    CellState attack(int x, int y) {
        if (grid[y][x] == CellState::Ship) {
            grid[y][x] = CellState::Hit;

            for (auto& ship : ships) {
                for (int i = 0; i < ship.positions.size(); ++i) {
                    if (ship.positions[i].first == x && ship.positions[i].second == y) {
                        ship.hits[i] = true;
                        if (ship.isDestroyed()) {
                            markAroundDestroyedShip(ship);
                            return CellState::Destroyed;
                        }
                        return CellState::Hit;
                    }
                }
            }
        }
        else if (grid[y][x] == CellState::Empty) {
            grid[y][x] = CellState::Miss;
            return CellState::Miss;
        }
        return grid[y][x];
    }

    void markAroundDestroyedShip(const Ship& ship) {
        for (const auto& pos : ship.positions) {
            for (int x = pos.first - 1; x <= pos.first + 1; ++x) {
                for (int y = pos.second - 1; y <= pos.second + 1; ++y) {
                    if (x >= 0 && x < GRID_SIZE && y >= 0 && y < GRID_SIZE) {
                        if (grid[y][x] == CellState::Empty) {
                            grid[y][x] = CellState::Miss;
                        }
                    }
                }
            }
            grid[pos.second][pos.first] = CellState::Destroyed;
        }
    }

    bool allShipsDestroyed() const {
        for (const auto& ship : ships) {
            if (!ship.isDestroyed()) return false;
        }
        return true;
    }

    const std::vector<std::vector<CellState>>& getGrid() const {
        return grid;
    }

    const std::vector<Ship>& getShips() const {
        return ships;
    }
};

class Game {
private:
    BattleGrid playerGrid;
    BattleGrid computerGrid;
    GameState state;
    Difficulty difficulty;
    int currentShipSize;
    bool currentShipHorizontal;
    std::vector<int> shipSizes;
    sf::Font font;
    sf::Text statusText;
    sf::Text playerShipsText;
    sf::Text computerShipsText;
    sf::Text difficultyText;
    int playerShipsLeft;
    int computerShipsLeft;

    // Анимационные переменные
    sf::Clock animationClock;
    sf::Clock computerTurnClock;
    float animationProgress;
    std::pair<int, int> animationTarget;
    bool isPlayerAnimation;
    sf::CircleShape rippleEffect;
    bool showRipple;
    sf::Vector2f ripplePosition;
    float rippleSize;

    // Улучшенный ИИ бота
    std::pair<int, int> lastHitPos;
    bool hasLastHit;
    std::vector<std::pair<int, int>> possibleTargets;
    std::vector<std::pair<int, int>> directions = { {1, 0}, {-1, 0}, {0, 1}, {0, -1} };
    bool isHorizontalPossible;
    bool isVerticalPossible;
    bool isHuntingMode;
    std::pair<int, int> firstHitPos;
    std::pair<int, int> lastDirection;

    // Генератор случайных чисел
    std::random_device rd;
    std::mt19937 gen;

    void addPossibleTargets(int x, int y) {
        for (const auto& dir : directions) {
            int nx = x + dir.first;
            int ny = y + dir.second;
            if (nx >= 0 && nx < GRID_SIZE && ny >= 0 && ny < GRID_SIZE) {
                CellState cell = playerGrid.getGrid()[ny][nx];
                if (cell == CellState::Empty || cell == CellState::Ship) {
                    possibleTargets.emplace_back(nx, ny);
                }
            }
        }
    }

    void clearPossibleTargets() {
        possibleTargets.clear();
        hasLastHit = false;
        isHorizontalPossible = true;
        isVerticalPossible = true;
        isHuntingMode = false;
    }

    void updateDirectionInfo(int x, int y) {
        // Проверяем попадания по горизонтали
        bool leftHit = (x > 0 && playerGrid.getGrid()[y][x - 1] == CellState::Hit);
        bool rightHit = (x < GRID_SIZE - 1 && playerGrid.getGrid()[y][x + 1] == CellState::Hit);

        // Проверяем попадания по вертикали
        bool topHit = (y > 0 && playerGrid.getGrid()[y - 1][x] == CellState::Hit);
        bool bottomHit = (y < GRID_SIZE - 1 && playerGrid.getGrid()[y + 1][x] == CellState::Hit);

        // Если есть попадания по горизонтали, но нет по вертикали - корабль горизонтальный
        if ((leftHit || rightHit) && !topHit && !bottomHit) {
            isHorizontalPossible = true;
            isVerticalPossible = false;
        }
        // Если есть попадания по вертикали, но нет по горизонтали - корабль вертикальный
        else if (!leftHit && !rightHit && (topHit || bottomHit)) {
            isHorizontalPossible = false;
            isVerticalPossible = true;
        }
    }

    void startAnimation(int x, int y, bool isPlayer) {
        animationTarget = { x, y };
        animationProgress = 0.0f;
        animationClock.restart();
        isPlayerAnimation = isPlayer;
        state = GameState::Animation;

        // Инициализация ripple effect
        rippleEffect.setRadius(1);
        rippleEffect.setFillColor(sf::Color(255, 255, 255, 150));
        rippleEffect.setOutlineColor(sf::Color(0, 0, 255, 200));
        rippleEffect.setOutlineThickness(2);
        ripplePosition = sf::Vector2f(
            (isPlayer ? GRID_OFFSET_X + GRID_SIZE * CELL_SIZE + MARGIN : GRID_OFFSET_X) + x * CELL_SIZE + CELL_SIZE / 2,
            GRID_OFFSET_Y + y * CELL_SIZE + CELL_SIZE / 2
        );
        rippleEffect.setPosition(ripplePosition);
        rippleSize = 1;
        showRipple = true;
    }

public:
    Game() : state(GameState::DifficultySelection), difficulty(Difficulty::Medium),
        currentShipSize(4), currentShipHorizontal(true), playerShipsLeft(0),
        computerShipsLeft(0), hasLastHit(false), isHorizontalPossible(true),
        isVerticalPossible(true), gen(rd()), animationProgress(0), showRipple(false),
        isHuntingMode(false) {
        shipSizes = { 4, 3, 3, 2, 2, 2, 1, 1, 1, 1 };
        if (!font.loadFromFile("arial.ttf")) {
            std::cerr << "Failed to load font" << std::endl;
        }

        statusText.setFont(font);
        statusText.setCharacterSize(24);
        statusText.setFillColor(sf::Color::Black);

        playerShipsText.setFont(font);
        playerShipsText.setCharacterSize(20);
        playerShipsText.setFillColor(sf::Color::Black);
        playerShipsText.setPosition(GRID_OFFSET_X, GRID_OFFSET_Y + GRID_SIZE * CELL_SIZE + 10);

        computerShipsText.setFont(font);
        computerShipsText.setCharacterSize(20);
        computerShipsText.setFillColor(sf::Color::Black);
        computerShipsText.setPosition(GRID_OFFSET_X + GRID_SIZE * CELL_SIZE + MARGIN, GRID_OFFSET_Y + GRID_SIZE * CELL_SIZE + 10);

        difficultyText.setFont(font);
        difficultyText.setCharacterSize(24);
        difficultyText.setFillColor(sf::Color::Black);
        difficultyText.setString("Select difficulty:");
        difficultyText.setPosition(WINDOW_WIDTH / 2 - difficultyText.getLocalBounds().width / 2, 50);

        rippleEffect.setRadius(1);
        rippleEffect.setFillColor(sf::Color::Transparent);
        rippleEffect.setOutlineColor(sf::Color(0, 0, 255, 200));
        rippleEffect.setOutlineThickness(2);
    }

    void start() {
        playerGrid.clear();
        computerGrid.clear();
        placeComputerShips();
        state = GameState::ShipPlacement;
        currentShipSize = 4;
        currentShipHorizontal = true;
        shipSizes = { 4, 3, 3, 2, 2, 2, 1, 1, 1, 1 };
        playerShipsLeft = 10;
        computerShipsLeft = 10;
        clearPossibleTargets();
        updateStatusText();
    }

    void placeComputerShips() {
        std::uniform_int_distribution<> dis(0, GRID_SIZE - 1);
        std::uniform_int_distribution<> dir(0, 1);

        for (int size : {4, 3, 3, 2, 2, 2, 1, 1, 1, 1}) {
            bool placed = false;
            while (!placed) {
                int x = dis(gen);
                int y = dis(gen);
                bool horizontal = dir(gen) == 0;
                placed = computerGrid.placeShip(x, y, size, horizontal);
            }
        }
    }

    void handleEvent(const sf::Event& event) {
        if (event.type == sf::Event::MouseButtonPressed) {
            if (event.mouseButton.button == sf::Mouse::Left) {
                int mouseX = event.mouseButton.x;
                int mouseY = event.mouseButton.y;

                if (state == GameState::DifficultySelection) {
                    if (mouseY >= 100 && mouseY <= 140) {
                        if (mouseX >= WINDOW_WIDTH / 2 - 100 && mouseX <= WINDOW_WIDTH / 2 - 20) {
                            difficulty = Difficulty::Easy;
                            start();
                        }
                        else if (mouseX >= WINDOW_WIDTH / 2 - 10 && mouseX <= WINDOW_WIDTH / 2 + 70) {
                            difficulty = Difficulty::Medium;
                            start();
                        }
                        else if (mouseX >= WINDOW_WIDTH / 2 + 80 && mouseX <= WINDOW_WIDTH / 2 + 180) {
                            difficulty = Difficulty::Hard;
                            start();
                        }
                    }
                }
                else if (state == GameState::ShipPlacement) {
                    if (mouseX >= GRID_OFFSET_X && mouseX < GRID_OFFSET_X + GRID_SIZE * CELL_SIZE &&
                        mouseY >= GRID_OFFSET_Y && mouseY < GRID_OFFSET_Y + GRID_SIZE * CELL_SIZE) {
                        int gridX = (mouseX - GRID_OFFSET_X) / CELL_SIZE;
                        int gridY = (mouseY - GRID_OFFSET_Y) / CELL_SIZE;

                        if (playerGrid.placeShip(gridX, gridY, currentShipSize, currentShipHorizontal)) {
                            shipSizes.erase(shipSizes.begin());
                            if (shipSizes.empty()) {
                                state = GameState::PlayerTurn;
                            }
                            else {
                                currentShipSize = shipSizes[0];
                            }
                            updateStatusText();
                        }
                    }
                    else if (mouseX >= WINDOW_WIDTH / 2 - 50 && mouseX < WINDOW_WIDTH / 2 + 50 &&
                        mouseY >= GRID_OFFSET_Y + GRID_SIZE * CELL_SIZE + 20 &&
                        mouseY < GRID_OFFSET_Y + GRID_SIZE * CELL_SIZE + 60) {
                        currentShipHorizontal = !currentShipHorizontal;
                        updateStatusText();
                    }
                }
                else if (state == GameState::PlayerTurn) {
                    if (mouseX >= GRID_OFFSET_X + GRID_SIZE * CELL_SIZE + MARGIN &&
                        mouseX < GRID_OFFSET_X + 2 * GRID_SIZE * CELL_SIZE + MARGIN &&
                        mouseY >= GRID_OFFSET_Y && mouseY < GRID_OFFSET_Y + GRID_SIZE * CELL_SIZE) {
                        int gridX = (mouseX - GRID_OFFSET_X - GRID_SIZE * CELL_SIZE - MARGIN) / CELL_SIZE;
                        int gridY = (mouseY - GRID_OFFSET_Y) / CELL_SIZE;

                        CellState cell = computerGrid.getGrid()[gridY][gridX];
                        if (cell == CellState::Empty || cell == CellState::Ship) {
                            startAnimation(gridX, gridY, true);
                        }
                    }
                }
            }
        }
        else if (event.type == sf::Event::KeyPressed) {
            if (event.key.code == sf::Keyboard::R && state == GameState::ShipPlacement) {
                currentShipHorizontal = !currentShipHorizontal;
                updateStatusText();
            }
            else if (event.key.code == sf::Keyboard::R &&
                (state == GameState::PlayerWins || state == GameState::ComputerWins)) {
                state = GameState::DifficultySelection;
            }
        }
    }

    void computerTurn() {
        if (computerTurnClock.getElapsedTime().asMilliseconds() < 800) {
            return;
        }

        bool attacked = false;
        while (!attacked) {
            if (difficulty == Difficulty::Easy) {
                // Легкий уровень - случайные атаки
                std::uniform_int_distribution<> dis(0, GRID_SIZE - 1);
                int x = dis(gen);
                int y = dis(gen);

                CellState cell = playerGrid.getGrid()[y][x];
                if (cell == CellState::Empty || cell == CellState::Ship) {
                    startAnimation(x, y, false);
                    attacked = true;
                }
            }
            else if (difficulty == Difficulty::Medium) {
                // Средний уровень - добивает корабль до конца
                if (hasLastHit && !possibleTargets.empty()) {
                    // Атакуем первую доступную цель из списка возможных
                    for (auto& target : possibleTargets) {
                        CellState cell = playerGrid.getGrid()[target.second][target.first];
                        if (cell == CellState::Empty || cell == CellState::Ship) {
                            startAnimation(target.first, target.second, false);
                            attacked = true;
                            break;
                        }
                    }

                    // Удаляем все невозможные цели
                    possibleTargets.erase(
                        std::remove_if(possibleTargets.begin(), possibleTargets.end(),
                            [this](const std::pair<int, int>& pos) {
                                CellState cell = playerGrid.getGrid()[pos.second][pos.first];
                                return cell != CellState::Empty && cell != CellState::Ship;
                            }),
                        possibleTargets.end()
                    );
                }
                else {
                    // Случайная атака, если нет возможных целей
                    std::uniform_int_distribution<> dis(0, GRID_SIZE - 1);
                    int x = dis(gen);
                    int y = dis(gen);

                    CellState cell = playerGrid.getGrid()[y][x];
                    if (cell == CellState::Empty || cell == CellState::Ship) {
                        startAnimation(x, y, false);
                        attacked = true;
                    }
                }
            }
            else if (difficulty == Difficulty::Hard) {
                // Сложный уровень использует интеллектуальный алгоритм
                if (hasLastHit && !possibleTargets.empty()) {
                    // Если есть последнее попадание и возможные цели, атакуем их
                    std::pair<int, int> target;
                    bool found = false;

                    // На сложном уровне определяем ориентацию корабля
                    updateDirectionInfo(lastHitPos.first, lastHitPos.second);

                    // Фильтруем возможные цели по определенной ориентации
                    std::vector<std::pair<int, int>> filteredTargets;
                    for (const auto& pos : possibleTargets) {
                        if ((isHorizontalPossible && pos.second == lastHitPos.second) ||
                            (!isHorizontalPossible && pos.first == lastHitPos.first)) {
                            filteredTargets.push_back(pos);
                        }
                    }

                    if (!filteredTargets.empty()) {
                        // Выбираем из отфильтрованных целей
                        std::uniform_int_distribution<> dis(0, filteredTargets.size() - 1);
                        target = filteredTargets[dis(gen)];
                        found = true;

                        // Удаляем выбранную цель из основного списка
                        possibleTargets.erase(
                            std::remove(possibleTargets.begin(), possibleTargets.end(), target),
                            possibleTargets.end()
                        );
                    }

                    if (!found) {
                        // Если ориентация не определена или нет подходящих целей, выбираем случайную из возможных
                        std::uniform_int_distribution<> dis(0, possibleTargets.size() - 1);
                        target = possibleTargets[dis(gen)];
                        possibleTargets.erase(possibleTargets.begin() + dis(gen));
                    }

                    CellState cell = playerGrid.getGrid()[target.second][target.first];
                    if (cell == CellState::Empty || cell == CellState::Ship) {
                        startAnimation(target.first, target.second, false);
                        attacked = true;
                    }
                }
                else {
                    // Случайная атака, если нет возможных целей
                    std::uniform_int_distribution<> dis(0, GRID_SIZE - 1);
                    int x = dis(gen);
                    int y = dis(gen);

                    CellState cell = playerGrid.getGrid()[y][x];
                    if (cell == CellState::Empty || cell == CellState::Ship) {
                        startAnimation(x, y, false);
                        attacked = true;
                    }
                }
            }
        }
    }

    void updateAnimation() {
        float elapsed = animationClock.getElapsedTime().asSeconds();
        animationProgress = std::min(elapsed / 0.5f, 1.0f); // Анимация длится 0.5 секунды

        // Обновление ripple effect
        if (showRipple) {
            rippleSize += 20.0f;
            rippleEffect.setRadius(rippleSize);
            rippleEffect.setPosition(ripplePosition.x - rippleSize, ripplePosition.y - rippleSize);

            float alpha = 255 * (1.0f - rippleSize / 50.0f);
            if (alpha < 0) {
                showRipple = false;
            }
            else {
                rippleEffect.setOutlineColor(sf::Color(0, 0, 255, static_cast<sf::Uint8>(alpha)));
            }
        }

        if (animationProgress >= 1.0f) {
            // Завершение анимации
            if (isPlayerAnimation) {
                CellState result = computerGrid.attack(animationTarget.first, animationTarget.second);

                if (computerGrid.allShipsDestroyed()) {
                    state = GameState::PlayerWins;
                }
                else if (result == CellState::Miss) {
                    state = GameState::ComputerTurn;
                    computerTurnClock.restart();
                }
                else {
                    // Если игрок попал, он продолжает ходить
                    state = GameState::PlayerTurn;
                }
            }
            else {
                CellState result = playerGrid.attack(animationTarget.first, animationTarget.second);

                if (result == CellState::Hit || result == CellState::Destroyed) {
                    lastHitPos = animationTarget;
                    hasLastHit = true;
                    if (difficulty == Difficulty::Hard) {
                        updateDirectionInfo(animationTarget.first, animationTarget.second);
                        addPossibleTargets(animationTarget.first, animationTarget.second);
                    }
                    else if (difficulty == Difficulty::Medium) {
                        addPossibleTargets(animationTarget.first, animationTarget.second);
                    }
                }

                if (result == CellState::Destroyed) {
                    clearPossibleTargets();
                }

                if (playerGrid.allShipsDestroyed()) {
                    state = GameState::ComputerWins;
                }
                else if (result == CellState::Miss) {
                    state = GameState::PlayerTurn;
                }
                else {
                    // Если компьютер попал, он продолжает ходить
                    state = GameState::ComputerTurn;
                    computerTurnClock.restart();
                }
            }

            updateShipsCount();
            updateStatusText();
            state = (state == GameState::Animation) ? GameState::PlayerTurn : state;
        }
    }

    void update() {
        if (state == GameState::Animation) {
            updateAnimation();
        }
        else if (state == GameState::ComputerTurn) {
            computerTurn();
        }
    }

    void updateStatusText() {
        std::stringstream ss;
        switch (state) {
        case GameState::DifficultySelection:
            ss << "Select difficulty level";
            break;
        case GameState::ShipPlacement:
            ss << "Place your ships (Size: " << currentShipSize << ", "
                << (currentShipHorizontal ? "Horizontal" : "Vertical") << ")";
            break;
        case GameState::PlayerTurn:
            ss << "Your turn - Attack enemy fleet!";
            break;
        case GameState::ComputerTurn:
            ss << "Computer is thinking...";
            break;
        case GameState::PlayerWins:
            ss << "Congratulations! You won! Press R to restart";
            break;
        case GameState::ComputerWins:
            ss << "Computer won! Press R to restart";
            break;
        case GameState::Animation:
            ss << (isPlayerAnimation ? "Your attack!" : "Computer attacks!");
            break;
        }
        statusText.setString(ss.str());
        statusText.setPosition(WINDOW_WIDTH / 2 - statusText.getLocalBounds().width / 2,
            GRID_OFFSET_Y + GRID_SIZE * CELL_SIZE + 70);
    }

    void updateShipsCount() {
        int playerAlive = 0;
        for (const auto& ship : playerGrid.getShips()) {
            if (!ship.isDestroyed()) playerAlive++;
        }
        playerShipsLeft = playerAlive;

        int computerAlive = 0;
        for (const auto& ship : computerGrid.getShips()) {
            if (!ship.isDestroyed()) computerAlive++;
        }
        computerShipsLeft = computerAlive;

        playerShipsText.setString("Your ships: " + std::to_string(playerShipsLeft) + "/10");
        computerShipsText.setString("Enemy ships: " + std::to_string(computerShipsLeft) + "/10");
    }

    void draw(sf::RenderWindow& window) {
        window.clear(sf::Color::White);

        if (state == GameState::DifficultySelection) {
            window.draw(difficultyText);

            sf::RectangleShape easyButton(sf::Vector2f(80, 40));
            easyButton.setFillColor(sf::Color(200, 200, 200));
            easyButton.setPosition(WINDOW_WIDTH / 2 - 100, 100);
            window.draw(easyButton);

            sf::Text easyText("Easy", font, 20);
            easyText.setFillColor(sf::Color::Black);
            easyText.setPosition(WINDOW_WIDTH / 2 - 80, 110);
            window.draw(easyText);

            sf::RectangleShape mediumButton(sf::Vector2f(80, 40));
            mediumButton.setFillColor(sf::Color(200, 200, 200));
            mediumButton.setPosition(WINDOW_WIDTH / 2 - 10, 100);
            window.draw(mediumButton);

            sf::Text mediumText("Medium", font, 20);
            mediumText.setFillColor(sf::Color::Black);
            mediumText.setPosition(WINDOW_WIDTH / 2 - 5, 110);
            window.draw(mediumText);

            sf::RectangleShape hardButton(sf::Vector2f(100, 40));
            hardButton.setFillColor(sf::Color(200, 200, 200));
            hardButton.setPosition(WINDOW_WIDTH / 2 + 80, 100);
            window.draw(hardButton);

            sf::Text hardText("Hard", font, 20);
            hardText.setFillColor(sf::Color::Black);
            hardText.setPosition(WINDOW_WIDTH / 2 + 100, 110);
            window.draw(hardText);
        }
        else {
            drawGrid(window, GRID_OFFSET_X, GRID_OFFSET_Y, playerGrid.getGrid(), true);
            drawGrid(window, GRID_OFFSET_X + GRID_SIZE * CELL_SIZE + MARGIN, GRID_OFFSET_Y, computerGrid.getGrid(), false);

            sf::Text playerLabel("Your fleet", font, 20);
            playerLabel.setFillColor(sf::Color::Black);
            playerLabel.setPosition(GRID_OFFSET_X, GRID_OFFSET_Y - 30);
            window.draw(playerLabel);

            sf::Text computerLabel("Enemy fleet", font, 20);
            computerLabel.setFillColor(sf::Color::Black);
            computerLabel.setPosition(GRID_OFFSET_X + GRID_SIZE * CELL_SIZE + MARGIN, GRID_OFFSET_Y - 30);
            window.draw(computerLabel);

            window.draw(playerShipsText);
            window.draw(computerShipsText);
            window.draw(statusText);

            // Рисуем анимацию выстрела
            if (state == GameState::Animation && animationProgress < 1.0f) {
                float size = CELL_SIZE * 0.8f * animationProgress;
                sf::CircleShape explosion(size / 2);
                explosion.setFillColor(sf::Color(255, 165, 0, 200 - static_cast<sf::Uint8>(200 * animationProgress)));

                if (isPlayerAnimation) {
                    explosion.setPosition(
                        GRID_OFFSET_X + GRID_SIZE * CELL_SIZE + MARGIN + animationTarget.first * CELL_SIZE + (CELL_SIZE - size) / 2,
                        GRID_OFFSET_Y + animationTarget.second * CELL_SIZE + (CELL_SIZE - size) / 2
                    );
                }
                else {
                    explosion.setPosition(
                        GRID_OFFSET_X + animationTarget.first * CELL_SIZE + (CELL_SIZE - size) / 2,
                        GRID_OFFSET_Y + animationTarget.second * CELL_SIZE + (CELL_SIZE - size) / 2
                    );
                }
                window.draw(explosion);
            }

            // Рисуем ripple effect
            if (showRipple && rippleSize < 50) {
                window.draw(rippleEffect);
            }

            if (state == GameState::ShipPlacement) {
                sf::Text shipSizeText("Current ship size: " + std::to_string(currentShipSize), font, 20);
                shipSizeText.setFillColor(sf::Color::Black);
                shipSizeText.setPosition(WINDOW_WIDTH / 2 - 100, GRID_OFFSET_Y + GRID_SIZE * CELL_SIZE + 100);
                window.draw(shipSizeText);

                sf::RectangleShape rotateButton(sf::Vector2f(100, 40));
                rotateButton.setFillColor(sf::Color(200, 200, 200));
                rotateButton.setPosition(WINDOW_WIDTH / 2 - 50, GRID_OFFSET_Y + GRID_SIZE * CELL_SIZE + 20);
                window.draw(rotateButton);

                sf::Text rotateText("Rotate", font, 20);
                rotateText.setFillColor(sf::Color::Black);
                rotateText.setPosition(WINDOW_WIDTH / 2 - 30, GRID_OFFSET_Y + GRID_SIZE * CELL_SIZE + 30);
                window.draw(rotateText);
            }
        }
    }

    void drawGrid(sf::RenderWindow& window, int offsetX, int offsetY, const std::vector<std::vector<CellState>>& grid, bool showShips) {
        for (int i = 0; i <= GRID_SIZE; ++i) {
            sf::Vertex lineV[] = {
                sf::Vertex(sf::Vector2f(offsetX + i * CELL_SIZE, offsetY), sf::Color::Black),
                sf::Vertex(sf::Vector2f(offsetX + i * CELL_SIZE, offsetY + GRID_SIZE * CELL_SIZE), sf::Color::Black)
            };
            window.draw(lineV, 2, sf::Lines);

            sf::Vertex lineH[] = {
                sf::Vertex(sf::Vector2f(offsetX, offsetY + i * CELL_SIZE), sf::Color::Black),
                sf::Vertex(sf::Vector2f(offsetX + GRID_SIZE * CELL_SIZE, offsetY + i * CELL_SIZE), sf::Color::Black)
            };
            window.draw(lineH, 2, sf::Lines);
        }

        for (int y = 0; y < GRID_SIZE; ++y) {
            for (int x = 0; x < GRID_SIZE; ++x) {
                sf::RectangleShape cell(sf::Vector2f(CELL_SIZE - 2, CELL_SIZE - 2));
                cell.setPosition(offsetX + x * CELL_SIZE + 1, offsetY + y * CELL_SIZE + 1);

                switch (grid[y][x]) {
                case CellState::Empty:
                    cell.setFillColor(sf::Color::White);
                    break;
                case CellState::Ship:
                    if (showShips) {
                        cell.setFillColor(sf::Color(100, 100, 100));
                    }
                    else {
                        cell.setFillColor(sf::Color::White);
                    }
                    break;
                case CellState::Hit:
                    cell.setFillColor(sf::Color::Red);
                    break;
                case CellState::Miss:
                    cell.setFillColor(sf::Color(200, 200, 200));
                    break;
                case CellState::Destroyed:
                    cell.setFillColor(sf::Color(150, 0, 0));
                    break;
                }

                window.draw(cell);

                if (grid[y][x] == CellState::Hit || grid[y][x] == CellState::Destroyed) {
                    sf::Vertex hitX1[] = {
                        sf::Vertex(sf::Vector2f(offsetX + x * CELL_SIZE + 5, offsetY + y * CELL_SIZE + 5), sf::Color::Black),
                        sf::Vertex(sf::Vector2f(offsetX + (x + 1) * CELL_SIZE - 5, offsetY + (y + 1) * CELL_SIZE - 5), sf::Color::Black)
                    };
                    sf::Vertex hitX2[] = {
                        sf::Vertex(sf::Vector2f(offsetX + (x + 1) * CELL_SIZE - 5, offsetY + y * CELL_SIZE + 5), sf::Color::Black),
                        sf::Vertex(sf::Vector2f(offsetX + x * CELL_SIZE + 5, offsetY + (y + 1) * CELL_SIZE - 5), sf::Color::Black)
                    };
                    window.draw(hitX1, 2, sf::Lines);
                    window.draw(hitX2, 2, sf::Lines);
                }
                else if (grid[y][x] == CellState::Miss) {
                    sf::CircleShape miss(CELL_SIZE / 8);
                    miss.setFillColor(sf::Color::Black);
                    miss.setPosition(offsetX + x * CELL_SIZE + CELL_SIZE / 2 - CELL_SIZE / 8,
                        offsetY + y * CELL_SIZE + CELL_SIZE / 2 - CELL_SIZE / 8);
                    window.draw(miss);
                }
            }
        }
    }
};

int main() {
    sf::RenderWindow window(sf::VideoMode(WINDOW_WIDTH, WINDOW_HEIGHT), "Sea Battle");
    window.setFramerateLimit(60);

    Game game;

    while (window.isOpen()) {
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) {
                window.close();
            }
            game.handleEvent(event);
        }

        game.update();

        window.clear();
        game.draw(window);
        window.display();
    }

    return 0;
}