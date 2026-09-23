#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <fstream>
#include <time.h>
#include <stdio.h>
#include <algorithm>
#include <Windows.h>

using namespace std;

constexpr int FIELD_WIDTH = 12;
constexpr int FIELD_HEIGHT = 18;
constexpr int MIN_LAYOUT_WIDTH = 52;
constexpr int MIN_LAYOUT_HEIGHT = 22;

// Console Color Definitions
constexpr WORD COLOR_BLACK        = 0;
constexpr WORD COLOR_DARK_BLUE    = FOREGROUND_BLUE;
constexpr WORD COLOR_DARK_GREEN   = FOREGROUND_GREEN;
constexpr WORD COLOR_DARK_CYAN    = FOREGROUND_GREEN | FOREGROUND_BLUE;
constexpr WORD COLOR_DARK_RED     = FOREGROUND_RED;
constexpr WORD COLOR_DARK_MAGENTA = FOREGROUND_RED | FOREGROUND_BLUE;
constexpr WORD COLOR_DARK_YELLOW  = FOREGROUND_RED | FOREGROUND_GREEN;
constexpr WORD COLOR_GRAY         = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
constexpr WORD COLOR_DARK_GRAY    = FOREGROUND_INTENSITY;
constexpr WORD COLOR_BLUE         = FOREGROUND_BLUE | FOREGROUND_INTENSITY;
constexpr WORD COLOR_GREEN        = FOREGROUND_GREEN | FOREGROUND_INTENSITY;
constexpr WORD COLOR_CYAN         = FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
constexpr WORD COLOR_RED          = FOREGROUND_RED | FOREGROUND_INTENSITY;
constexpr WORD COLOR_MAGENTA      = FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
constexpr WORD COLOR_YELLOW       = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
constexpr WORD COLOR_WHITE        = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;

// Official Tetris piece colors
const WORD PIECE_COLORS[7] = {
    COLOR_CYAN,        // 0: I (Cyan)
    COLOR_MAGENTA,     // 1: T (Purple / Magenta)
    COLOR_YELLOW,      // 2: O (Yellow)
    COLOR_GREEN,       // 3: S (Green)
    COLOR_RED,         // 4: Z (Red)
    COLOR_BLUE,        // 5: J (Blue)
    COLOR_DARK_YELLOW  // 6: L (Orange / Dark Yellow)
};

class TetrisGame {
private:
    wstring tetromino[7];
    unsigned char* pField = nullptr;
    vector<CHAR_INFO> screen;
    int nScreenWidth = 0;
    int nScreenHeight = 0;

    HANDLE hConsole = NULL;
    HANDLE hOriginalConsole = NULL;

    int nCurrentPiece = 0;
    int nNextPiece = 0;
    int nHoldPiece = -1;
    bool bCanHold = true;
    bool bHoldKeyHold = true;
    bool bSpaceKeyHold = true;
    bool bPauseKeyHold = true;

    int nCurrentRotation = 0;
    int nCurrentX = 0;
    int nCurrentY = 0;
    int nSpeed = 20;
    int nSpeedCount = 0;
    int nPieceCount = 0;
    int nLinesCleared = 0;
    int nScore = 0;
    int nHighScore = 0;

    vector<int> vLines;
    bool bGameOver = false;
    bool bPaused = false;
    bool bRotateHold = true;
    bool bKey[4];

    int LoadHighScore() {
        ifstream file("highscore.dat");
        int hs = 0;
        if (file >> hs) return hs;
        return 0;
    }

    void SaveHighScore(int hs) {
        ofstream file("highscore.dat");
        if (file) file << hs;
    }

    int Rotate(int px, int py, int r) {
        int pi = 0;
        switch (r % 4) {
        case 0: pi = py * 4 + px;           break; // 0 degrees
        case 1: pi = 12 + py - (px * 4);    break; // 90 degrees
        case 2: pi = 15 - (py * 4) - px;    break; // 180 degrees
        case 3: pi = 3 - py + (px * 4);     break; // 270 degrees
        }
        return pi;
    }

    bool DoesPieceFit(int nTetromino, int nRotation, int nPosX, int nPosY) {
        for (int px = 0; px < 4; px++) {
            for (int py = 0; py < 4; py++) {
                int pi = Rotate(px, py, nRotation);
                int fi = (nPosY + py) * FIELD_WIDTH + (nPosX + px);

                if (nPosX + px >= 0 && nPosX + px < FIELD_WIDTH) {
                    if (nPosY + py >= 0 && nPosY + py < FIELD_HEIGHT) {
                        if (tetromino[nTetromino][pi] != L'.' && pField[fi] != 0)
                            return false;
                    }
                }
            }
        }
        return true;
    }

    void ClearConsoleBuffer() {
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        if (GetConsoleScreenBufferInfo(hConsole, &csbi)) {
            int winW = csbi.srWindow.Right - csbi.srWindow.Left + 1;
            int winH = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
            DWORD written = 0;
            for (int y = 0; y < winH; y++) {
                COORD coord = { csbi.srWindow.Left, static_cast<SHORT>(csbi.srWindow.Top + y) };
                FillConsoleOutputCharacterW(hConsole, L' ', winW, coord, &written);
                FillConsoleOutputAttribute(hConsole, COLOR_WHITE, winW, coord, &written);
            }
        }
    }

    void UpdateTerminalDimensions() {
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        if (GetConsoleScreenBufferInfo(hConsole, &csbi)) {
            int winW = csbi.srWindow.Right - csbi.srWindow.Left + 1;
            int winH = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;

            if (winW <= 0 || winH <= 0) {
                winW = csbi.dwSize.X;
                winH = csbi.dwSize.Y;
            }

            // Ensure buffer covers the window area
            if (csbi.dwSize.X < winW || csbi.dwSize.Y < winH) {
                COORD newSize = {
                    static_cast<SHORT>(max(static_cast<int>(csbi.dwSize.X), winW)),
                    static_cast<SHORT>(max(static_cast<int>(csbi.dwSize.Y), winH))
                };
                SetConsoleScreenBufferSize(hConsole, newSize);
                GetConsoleScreenBufferInfo(hConsole, &csbi);
                winW = csbi.srWindow.Right - csbi.srWindow.Left + 1;
                winH = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
            }

            if (winW != nScreenWidth || winH != nScreenHeight) {
                ClearConsoleBuffer();
                nScreenWidth = winW;
                nScreenHeight = winH;
                CHAR_INFO emptyCell;
                emptyCell.Char.UnicodeChar = L' ';
                emptyCell.Attributes = COLOR_WHITE;
                screen.assign(nScreenWidth * nScreenHeight, emptyCell);
            }
        }
    }

    void DrawCell(int x, int y, wchar_t ch, WORD attr) {
        if (x >= 0 && x < nScreenWidth && y >= 0 && y < nScreenHeight) {
            screen[y * nScreenWidth + x].Char.UnicodeChar = ch;
            screen[y * nScreenWidth + x].Attributes = attr;
        }
    }

    void DrawString(int x, int y, const wstring& str, WORD attr = COLOR_WHITE) {
        for (size_t i = 0; i < str.length(); i++) {
            DrawCell(x + static_cast<int>(i), y, str[i], attr);
        }
    }

    void DrawTile(int x, int y, const wchar_t* tile2, WORD attr) {
        DrawCell(x, y, tile2[0], attr);
        DrawCell(x + 1, y, tile2[1], attr);
    }

    void RenderFrame() {
        if (nScreenWidth <= 0 || nScreenHeight <= 0 || screen.empty()) return;

        CONSOLE_SCREEN_BUFFER_INFO csbi;
        if (!GetConsoleScreenBufferInfo(hConsole, &csbi)) return;

        COORD bufSize = { static_cast<SHORT>(nScreenWidth), static_cast<SHORT>(nScreenHeight) };
        COORD bufCoord = { 0, 0 };
        SMALL_RECT writeRegion = {
            csbi.srWindow.Left,
            csbi.srWindow.Top,
            static_cast<SHORT>(csbi.srWindow.Left + nScreenWidth - 1),
            static_cast<SHORT>(csbi.srWindow.Top + nScreenHeight - 1)
        };

        WriteConsoleOutputW(hConsole, screen.data(), bufSize, bufCoord, &writeRegion);
    }

    void ResetGame() {
        nHighScore = LoadHighScore();
        for (int x = 0; x < FIELD_WIDTH; x++)
            for (int y = 0; y < FIELD_HEIGHT; y++)
                pField[y * FIELD_WIDTH + x] = (x == 0 || x == FIELD_WIDTH - 1 || y == FIELD_HEIGHT - 1) ? 9 : 0;

        nCurrentPiece = rand() % 7;
        nNextPiece = rand() % 7;
        nHoldPiece = -1;
        bCanHold = true;
        nCurrentRotation = 0;
        nCurrentX = FIELD_WIDTH / 2 - 2;
        nCurrentY = 0;
        nSpeed = 20;
        nSpeedCount = 0;
        bRotateHold = true;
        nPieceCount = 0;
        nLinesCleared = 0;
        nScore = 0;
        bGameOver = false;
        bPaused = false;
    }

public:
    TetrisGame() {
        srand(static_cast<unsigned int>(time(NULL)));

        hOriginalConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        hConsole = CreateConsoleScreenBuffer(GENERIC_READ | GENERIC_WRITE, 0, NULL, CONSOLE_TEXTMODE_BUFFER, NULL);
        SetConsoleActiveScreenBuffer(hConsole);

        CONSOLE_CURSOR_INFO cursorInfo;
        GetConsoleCursorInfo(hConsole, &cursorInfo);
        cursorInfo.bVisible = false;
        SetConsoleCursorInfo(hConsole, &cursorInfo);

        UpdateTerminalDimensions();

        tetromino[0].append(L"..X...X...X...X."); // I
        tetromino[1].append(L"..X..XX...X....."); // T
        tetromino[2].append(L".....XX..XX....."); // O
        tetromino[3].append(L"..X..XX..X......"); // S
        tetromino[4].append(L".X...XX...X....."); // Z
        tetromino[5].append(L".X...X...XX....."); // J
        tetromino[6].append(L"..X...X..XX....."); // L

        pField = new unsigned char[FIELD_WIDTH * FIELD_HEIGHT];
        ResetGame();
    }

    ~TetrisGame() {
        delete[] pField;
        if (hConsole) {
            SetConsoleActiveScreenBuffer(hOriginalConsole);
            CloseHandle(hConsole);
        }
    }

    void Run() {
        bool bExitApp = false;

        while (!bExitApp) {
            auto t1 = chrono::system_clock::now();

            while (!bGameOver) {
                auto t2 = chrono::system_clock::now();
                chrono::duration<float> elapsedTime = t2 - t1;

                if (elapsedTime.count() < 0.05f) {
                    Sleep(5);
                    continue;
                }
                t1 = t2;

                UpdateTerminalDimensions();

                // Clear screen buffer
                CHAR_INFO emptyCell;
                emptyCell.Char.UnicodeChar = L' ';
                emptyCell.Attributes = COLOR_WHITE;
                fill(screen.begin(), screen.end(), emptyCell);

                // Minimum terminal size check
                if (nScreenWidth < MIN_LAYOUT_WIDTH || nScreenHeight < MIN_LAYOUT_HEIGHT) {
                    int warnY = max(0, nScreenHeight / 2 - 3);
                    int warnX = max(0, (nScreenWidth - 32) / 2);
                    DrawString(warnX, warnY,     L"+------------------------------+", COLOR_YELLOW);
                    DrawString(warnX, warnY + 1, L"|     TERMINAL TOO SMALL!      |", COLOR_RED);
                    wchar_t szSize[40];
                    wsprintfW(szSize, L"|     Current: %3dx%-3d         |", nScreenWidth, nScreenHeight);
                    DrawString(warnX, warnY + 2, szSize, COLOR_WHITE);
                    wsprintfW(szSize, L"|     Minimum: %3dx%-3d         |", MIN_LAYOUT_WIDTH, MIN_LAYOUT_HEIGHT);
                    DrawString(warnX, warnY + 3, szSize, COLOR_WHITE);
                    DrawString(warnX, warnY + 4, L"|     Please resize window     |", COLOR_GREEN);
                    DrawString(warnX, warnY + 5, L"+------------------------------+", COLOR_YELLOW);

                    RenderFrame();

                    if ((0x8000 & GetAsyncKeyState('Q')) || (0x8000 & GetAsyncKeyState(VK_ESCAPE))) {
                        bExitApp = true;
                        break;
                    }
                    continue;
                }

                // Pause toggle ('P')
                bool bPausePressed = (0x8000 & GetAsyncKeyState('P')) != 0;
                if (bPausePressed) {
                    if (bPauseKeyHold) {
                        bPaused = !bPaused;
                        bPauseKeyHold = false;
                    }
                } else {
                    bPauseKeyHold = true;
                }

                if (bPaused) {
                    int centerY = max(0, nScreenHeight / 2 - 2);
                    int centerX = max(0, (nScreenWidth - 32) / 2);
                    DrawString(centerX, centerY,     L"+------------------------------+", COLOR_CYAN);
                    DrawString(centerX, centerY + 1, L"|         P A U S E D          |", COLOR_YELLOW);
                    DrawString(centerX, centerY + 2, L"|    Press 'P' to Resume       |", COLOR_WHITE);
                    DrawString(centerX, centerY + 3, L"+------------------------------+", COLOR_CYAN);
                    RenderFrame();

                    if ((0x8000 & GetAsyncKeyState('Q')) || (0x8000 & GetAsyncKeyState(VK_ESCAPE))) {
                        bExitApp = true;
                        break;
                    }
                    continue;
                }

                // Hold Key ('C' or 'H')
                bool bHoldPressed = ((0x8000 & GetAsyncKeyState('C')) || (0x8000 & GetAsyncKeyState('H'))) != 0;
                if (bHoldPressed) {
                    if (bHoldKeyHold && bCanHold) {
                        if (nHoldPiece == -1) {
                            nHoldPiece = nCurrentPiece;
                            nCurrentPiece = nNextPiece;
                            nNextPiece = rand() % 7;
                        } else {
                            int temp = nCurrentPiece;
                            nCurrentPiece = nHoldPiece;
                            nHoldPiece = temp;
                        }
                        nCurrentX = FIELD_WIDTH / 2 - 2;
                        nCurrentY = 0;
                        nCurrentRotation = 0;
                        bCanHold = false;
                        bHoldKeyHold = false;
                    }
                } else {
                    bHoldKeyHold = true;
                }

                // Hard Drop (Space)
                bool bSpacePressed = (0x8000 & GetAsyncKeyState(VK_SPACE)) != 0;
                bool bHardDropped = false;
                if (bSpacePressed) {
                    if (bSpaceKeyHold) {
                        int dropDist = 0;
                        while (DoesPieceFit(nCurrentPiece, nCurrentRotation, nCurrentX, nCurrentY + 1)) {
                            nCurrentY++;
                            dropDist++;
                        }
                        nScore += dropDist * 2;
                        bHardDropped = true;
                        bSpaceKeyHold = false;
                    }
                } else {
                    bSpaceKeyHold = true;
                }

                nSpeedCount++;
                bool bForceDown = (nSpeedCount >= nSpeed) || bHardDropped;

                // Input handling: Arrows / WASD
                bKey[0] = ((0x8000 & GetAsyncKeyState(VK_RIGHT)) || (0x8000 & GetAsyncKeyState('D'))) != 0;
                bKey[1] = ((0x8000 & GetAsyncKeyState(VK_LEFT))  || (0x8000 & GetAsyncKeyState('A'))) != 0;
                bKey[2] = ((0x8000 & GetAsyncKeyState(VK_DOWN))  || (0x8000 & GetAsyncKeyState('S'))) != 0;
                bKey[3] = ((0x8000 & GetAsyncKeyState(VK_UP))    || (0x8000 & GetAsyncKeyState('W')) ||
                           (0x8000 & GetAsyncKeyState('Z'))) != 0;

                if ((0x8000 & GetAsyncKeyState('Q')) || (0x8000 & GetAsyncKeyState(VK_ESCAPE))) {
                    bExitApp = true;
                    break;
                }

                // Lateral and vertical soft movement
                nCurrentX += (bKey[0] && DoesPieceFit(nCurrentPiece, nCurrentRotation, nCurrentX + 1, nCurrentY)) ? 1 : 0;
                nCurrentX -= (bKey[1] && DoesPieceFit(nCurrentPiece, nCurrentRotation, nCurrentX - 1, nCurrentY)) ? 1 : 0;
                if (bKey[2] && DoesPieceFit(nCurrentPiece, nCurrentRotation, nCurrentX, nCurrentY + 1)) {
                    nCurrentY++;
                    nScore += 1;
                }

                // Rotation with Wall Kick attempt
                if (bKey[3]) {
                    if (bRotateHold) {
                        if (DoesPieceFit(nCurrentPiece, nCurrentRotation + 1, nCurrentX, nCurrentY)) {
                            nCurrentRotation++;
                        } else if (DoesPieceFit(nCurrentPiece, nCurrentRotation + 1, nCurrentX - 1, nCurrentY)) {
                            nCurrentX--; nCurrentRotation++; // Wall kick left
                        } else if (DoesPieceFit(nCurrentPiece, nCurrentRotation + 1, nCurrentX + 1, nCurrentY)) {
                            nCurrentX++; nCurrentRotation++; // Wall kick right
                        }
                        bRotateHold = false;
                    }
                } else {
                    bRotateHold = true;
                }

                if (bForceDown) {
                    nSpeedCount = 0;
                    nPieceCount++;
                    if (nPieceCount % 50 == 0)
                        if (nSpeed >= 10) nSpeed--;

                    if (DoesPieceFit(nCurrentPiece, nCurrentRotation, nCurrentX, nCurrentY + 1))
                        nCurrentY++;
                    else {
                        // Lock piece into field
                        for (int px = 0; px < 4; px++)
                            for (int py = 0; py < 4; py++)
                                if (tetromino[nCurrentPiece][Rotate(px, py, nCurrentRotation)] != L'.')
                                    pField[(nCurrentY + py) * FIELD_WIDTH + (nCurrentX + px)] = nCurrentPiece + 1;

                        // Check for completed lines
                        for (int py = 0; py < 4; py++)
                            if (nCurrentY + py < FIELD_HEIGHT - 1) {
                                bool bLine = true;
                                for (int px = 1; px < FIELD_WIDTH - 1; px++)
                                    bLine &= (pField[(nCurrentY + py) * FIELD_WIDTH + px]) != 0;

                                if (bLine) {
                                    for (int px = 1; px < FIELD_WIDTH - 1; px++)
                                        pField[(nCurrentY + py) * FIELD_WIDTH + px] = 8;
                                    vLines.push_back(nCurrentY + py);
                                }
                            }

                        nScore += 25;
                        if (!vLines.empty()) {
                            nScore += (1 << vLines.size()) * 100;
                            nLinesCleared += static_cast<int>(vLines.size());
                        }

                        if (nScore > nHighScore) {
                            nHighScore = nScore;
                            SaveHighScore(nHighScore);
                        }

                        // Spawn next piece
                        nCurrentX = FIELD_WIDTH / 2 - 2;
                        nCurrentY = 0;
                        nCurrentRotation = 0;
                        nCurrentPiece = nNextPiece;
                        nNextPiece = rand() % 7;
                        bCanHold = true;

                        bGameOver = !DoesPieceFit(nCurrentPiece, nCurrentRotation, nCurrentX, nCurrentY);
                    }
                }

                // Layout calculations (double width tiles: 2 cols per board unit)
                int boardCharWidth = FIELD_WIDTH * 2;
                int totalLayoutWidth = boardCharWidth + 3 + 24; // ~51 cols
                int totalLayoutHeight = FIELD_HEIGHT;           // 18 rows
                int nOffsetX = max(1, (nScreenWidth - totalLayoutWidth) / 2);
                int nOffsetY = max(1, (nScreenHeight - totalLayoutHeight) / 2);

                // Draw Field (walls and locked blocks)
                for (int x = 0; x < FIELD_WIDTH; x++) {
                    for (int y = 0; y < FIELD_HEIGHT; y++) {
                        int sx = nOffsetX + x * 2;
                        int sy = nOffsetY + y;
                        unsigned char val = pField[y * FIELD_WIDTH + x];

                        if (val == 9) { // Border wall
                            if (y == FIELD_HEIGHT - 1) {
                                DrawTile(sx, sy, L"==", COLOR_GRAY);
                            } else if (x == 0) {
                                DrawTile(sx, sy, L"<!", COLOR_GRAY);
                            } else {
                                DrawTile(sx, sy, L"!>", COLOR_GRAY);
                            }
                        } else if (val == 8) { // Line clear flash
                            DrawTile(sx, sy, L"==", COLOR_WHITE);
                        } else if (val >= 1 && val <= 7) { // Locked piece
                            DrawTile(sx, sy, L"[]", PIECE_COLORS[val - 1]);
                        } else {
                            DrawTile(sx, sy, L"  ", COLOR_BLACK);
                        }
                    }
                }

                // Draw Ghost Piece (Shadow)
                int nGhostY = nCurrentY;
                while (DoesPieceFit(nCurrentPiece, nCurrentRotation, nCurrentX, nGhostY + 1)) {
                    nGhostY++;
                }

                if (nGhostY > nCurrentY) {
                    for (int px = 0; px < 4; px++) {
                        for (int py = 0; py < 4; py++) {
                            if (tetromino[nCurrentPiece][Rotate(px, py, nCurrentRotation)] != L'.') {
                                int sx = nOffsetX + (nCurrentX + px) * 2;
                                int sy = nOffsetY + (nGhostY + py);
                                DrawTile(sx, sy, L"::", COLOR_DARK_GRAY);
                            }
                        }
                    }
                }

                // Draw Active Piece
                for (int px = 0; px < 4; px++) {
                    for (int py = 0; py < 4; py++) {
                        if (tetromino[nCurrentPiece][Rotate(px, py, nCurrentRotation)] != L'.') {
                            int sx = nOffsetX + (nCurrentX + px) * 2;
                            int sy = nOffsetY + (nCurrentY + py);
                            DrawTile(sx, sy, L"[]", PIECE_COLORS[nCurrentPiece]);
                        }
                    }
                }

                // Draw Side HUD
                int hudX = nOffsetX + boardCharWidth + 3;
                int hudY = nOffsetY;

                DrawString(hudX, hudY,     L"========================", COLOR_CYAN);
                DrawString(hudX, hudY + 1, L"     T E T R I S        ", COLOR_YELLOW);
                DrawString(hudX, hudY + 2, L"========================", COLOR_CYAN);

                wchar_t szInfo[32];
                wsprintfW(szInfo, L"HIGH:  %8d", nHighScore);
                DrawString(hudX, hudY + 4, szInfo, COLOR_YELLOW);

                wsprintfW(szInfo, L"SCORE: %8d", nScore);
                DrawString(hudX, hudY + 5, szInfo, COLOR_WHITE);

                wsprintfW(szInfo, L"LINES: %8d", nLinesCleared);
                DrawString(hudX, hudY + 6, szInfo, COLOR_GREEN);

                wsprintfW(szInfo, L"SPEED: %8d", 21 - nSpeed);
                DrawString(hudX, hudY + 7, szInfo, COLOR_CYAN);

                // NEXT Piece Box
                DrawString(hudX, hudY + 9, L"NEXT: ", COLOR_WHITE);
                for (int px = 0; px < 4; px++) {
                    for (int py = 0; py < 2; py++) {
                        int sx = hudX + 7 + px * 2;
                        int sy = hudY + 9 + py;
                        if (tetromino[nNextPiece][py * 4 + px] != L'.') {
                            DrawTile(sx, sy, L"[]", PIECE_COLORS[nNextPiece]);
                        } else {
                            DrawTile(sx, sy, L"  ", COLOR_BLACK);
                        }
                    }
                }

                // HOLD Piece Box
                DrawString(hudX, hudY + 11, L"HOLD: ", COLOR_WHITE);
                if (nHoldPiece != -1) {
                    for (int px = 0; px < 4; px++) {
                        for (int py = 0; py < 2; py++) {
                            int sx = hudX + 7 + px * 2;
                            int sy = hudY + 11 + py;
                            if (tetromino[nHoldPiece][py * 4 + px] != L'.') {
                                DrawTile(sx, sy, L"[]", PIECE_COLORS[nHoldPiece]);
                            } else {
                                DrawTile(sx, sy, L"  ", COLOR_BLACK);
                            }
                        }
                    }
                } else {
                    DrawString(hudX + 7, hudY + 11, L"[NONE]", COLOR_DARK_GRAY);
                }

                DrawString(hudX, hudY + 13, L"------------------------", COLOR_DARK_GRAY);
                DrawString(hudX, hudY + 14, L"A/D / <-/-> : Move", COLOR_WHITE);
                DrawString(hudX, hudY + 15, L"S / DOWN    : Soft Drop", COLOR_WHITE);
                DrawString(hudX, hudY + 16, L"SPACE       : Hard Drop", COLOR_YELLOW);
                DrawString(hudX, hudY + 17, L"W / UP / Z  : Rotate", COLOR_WHITE);
                DrawString(hudX, hudY + 18, L"C / H       : Hold Piece", COLOR_CYAN);
                DrawString(hudX, hudY + 19, L"P           : Pause", COLOR_WHITE);
                DrawString(hudX, hudY + 20, L"Q / ESC     : Quit", COLOR_RED);
                DrawString(hudX, hudY + 21, L"------------------------", COLOR_DARK_GRAY);

                // Animate Line Completion
                if (!vLines.empty()) {
                    RenderFrame();
                    Sleep(300);

                    for (auto& v : vLines)
                        for (int px = 1; px < FIELD_WIDTH - 1; px++) {
                            for (int py = v; py > 0; py--)
                                pField[py * FIELD_WIDTH + px] = pField[(py - 1) * FIELD_WIDTH + px];
                            pField[px] = 0;
                        }

                    vLines.clear();
                }

                // Render current frame
                RenderFrame();
            }

            if (bExitApp) break;

            // Game Over Screen
            CHAR_INFO emptyCell;
            emptyCell.Char.UnicodeChar = L' ';
            emptyCell.Attributes = COLOR_WHITE;
            fill(screen.begin(), screen.end(), emptyCell);
            int goY = max(0, nScreenHeight / 2 - 4);
            int goX = max(0, (nScreenWidth - 32) / 2);
            DrawString(goX, goY,     L"+------------------------------+", COLOR_RED);
            DrawString(goX, goY + 1, L"|         GAME OVER!           |", COLOR_RED);
            wchar_t szFinal[40];
            wsprintfW(szFinal, L"|   Final Score: %-13d |", nScore);
            DrawString(goX, goY + 2, szFinal, COLOR_YELLOW);
            wsprintfW(szFinal, L"|   High Score:  %-13d |", nHighScore);
            DrawString(goX, goY + 3, szFinal, COLOR_CYAN);
            wsprintfW(szFinal, L"|   Lines:       %-13d |", nLinesCleared);
            DrawString(goX, goY + 4, szFinal, COLOR_GREEN);
            DrawString(goX, goY + 5, L"|                              |", COLOR_WHITE);
            DrawString(goX, goY + 6, L"|  'R' to Play Again           |", COLOR_WHITE);
            DrawString(goX, goY + 7, L"|  'Q' to Quit                 |", COLOR_WHITE);
            DrawString(goX, goY + 8, L"+------------------------------+", COLOR_RED);
            RenderFrame();

            // Wait for R to restart or Q to quit
            while (true) {
                Sleep(20);
                if ((0x8000 & GetAsyncKeyState('R')) != 0) {
                    ResetGame();
                    break;
                }
                if ((0x8000 & GetAsyncKeyState('Q')) || (0x8000 & GetAsyncKeyState(VK_ESCAPE))) {
                    bExitApp = true;
                    break;
                }
            }
        }

        SetConsoleActiveScreenBuffer(hOriginalConsole);
        CloseHandle(hConsole);
        hConsole = NULL;

        cout << "\nThanks for playing Terminal-Tetris!\nFinal Score: " << nScore << " | High Score: " << nHighScore << endl;
    }
};

int main() {
    TetrisGame game;
    game.Run();
    return 0;
}