#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <fstream>
#include <ctime>
#include <cstdio>
#include <algorithm>

using namespace std;

constexpr int FIELD_WIDTH = 12;
constexpr int FIELD_HEIGHT = 18;
constexpr int MIN_LAYOUT_WIDTH = 52;
constexpr int MIN_LAYOUT_HEIGHT = 24;

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

    // 7-Bag Randomizer
    int bag[7];
    int bagIndex = 7;

    int nCurrentPiece = 0;
    int nNextPiece = 0;
    int nHoldPiece = -1;
    bool bCanHold = true;

    // Input state & DAS (Delayed Auto Shift)
    bool bHoldKeyHold = true;
    bool bSpaceKeyHold = true;
    bool bPauseKeyHold = true;
    bool bRotateHold = true;
    int nLeftHoldCount = 0;
    int nRightHoldCount = 0;

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

    int GetNextTetromino() {
        if (bagIndex >= 7) {
            for (int i = 0; i < 7; i++) bag[i] = i;
            for (int i = 6; i > 0; i--) {
                int j = rand() % (i + 1);
                swap(bag[i], bag[j]);
            }
            bagIndex = 0;
        }
        return bag[bagIndex++];
    }

    int Rotate(int px, int py, int r) {
        int pi = 0;
        int rot = (r % 4 + 4) % 4;
        switch (rot) {
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
                if (tetromino[nTetromino][pi] != L'.') {
                    int fx = nPosX + px;
                    int fy = nPosY + py;

                    // Bounds check: must be strictly inside horizontal boundaries and above bottom floor
                    if (fx < 0 || fx >= FIELD_WIDTH || fy >= FIELD_HEIGHT)
                        return false;

                    // If within field vertically, check for collision with walls or locked blocks
                    if (fy >= 0) {
                        if (pField[fy * FIELD_WIDTH + fx] != 0)
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

            SHORT requiredW = static_cast<SHORT>(csbi.srWindow.Left + winW);
            SHORT requiredH = static_cast<SHORT>(csbi.srWindow.Top + winH);

            // Ensure buffer covers the window viewport
            if (csbi.dwSize.X < requiredW || csbi.dwSize.Y < requiredH) {
                COORD newSize = {
                    max(csbi.dwSize.X, requiredW),
                    max(csbi.dwSize.Y, requiredH)
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

    void DrawPiecePreview(int startX, int startY, int pieceId) {
        if (pieceId < 0 || pieceId >= 7) return;
        for (int py = 0; py < 4; py++) {
            for (int px = 0; px < 4; px++) {
                if (tetromino[pieceId][py * 4 + px] != L'.') {
                    DrawTile(startX + px * 2, startY + py, L"[]", PIECE_COLORS[pieceId]);
                }
            }
        }
    }

    void DrawGameScene(bool bDrawActivePiece = true) {
        // Clear screen buffer
        CHAR_INFO emptyCell;
        emptyCell.Char.UnicodeChar = L' ';
        emptyCell.Attributes = COLOR_WHITE;
        fill(screen.begin(), screen.end(), emptyCell);

        int boardCharWidth = FIELD_WIDTH * 2;
        int totalLayoutWidth = 51;
        int totalLayoutHeight = 23;
        int nOffsetX = max(0, (nScreenWidth - totalLayoutWidth) / 2);
        int nOffsetY = max(0, (nScreenHeight - totalLayoutHeight) / 2);

        int nBoardOffsetX = nOffsetX;
        int nBoardOffsetY = nOffsetY + 2;

        // Draw Field (walls and locked blocks)
        for (int x = 0; x < FIELD_WIDTH; x++) {
            for (int y = 0; y < FIELD_HEIGHT; y++) {
                int sx = nBoardOffsetX + x * 2;
                int sy = nBoardOffsetY + y;
                unsigned char val = pField[y * FIELD_WIDTH + x];

                if (val == 9) { // Border wall
                    if (y == FIELD_HEIGHT - 1) {
                        if (x == 0) {
                            DrawTile(sx, sy, L"<!", COLOR_GRAY);
                        } else if (x == FIELD_WIDTH - 1) {
                            DrawTile(sx, sy, L"!>", COLOR_GRAY);
                        } else {
                            DrawTile(sx, sy, L"==", COLOR_GRAY);
                        }
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

        if (bDrawActivePiece && !bGameOver) {
            // Draw Ghost Piece (Shadow)
            int nGhostY = nCurrentY;
            while (DoesPieceFit(nCurrentPiece, nCurrentRotation, nCurrentX, nGhostY + 1)) {
                nGhostY++;
            }

            if (nGhostY > nCurrentY) {
                for (int px = 0; px < 4; px++) {
                    for (int py = 0; py < 4; py++) {
                        if (tetromino[nCurrentPiece][Rotate(px, py, nCurrentRotation)] != L'.') {
                            int sx = nBoardOffsetX + (nCurrentX + px) * 2;
                            int sy = nBoardOffsetY + (nGhostY + py);
                            DrawTile(sx, sy, L"::", COLOR_DARK_GRAY);
                        }
                    }
                }
            }

            // Draw Active Piece
            for (int px = 0; px < 4; px++) {
                for (int py = 0; py < 4; py++) {
                    if (tetromino[nCurrentPiece][Rotate(px, py, nCurrentRotation)] != L'.') {
                        int sx = nBoardOffsetX + (nCurrentX + px) * 2;
                        int sy = nBoardOffsetY + (nCurrentY + py);
                        DrawTile(sx, sy, L"[]", PIECE_COLORS[nCurrentPiece]);
                    }
                }
            }
        }

        // Draw Side HUD
        int hudX = nOffsetX + boardCharWidth + 2;
        int hudY = nOffsetY;

        DrawString(hudX, hudY,     L"+=======================+", COLOR_CYAN);
        DrawString(hudX, hudY + 1, L"|      T E T R I S      |", COLOR_YELLOW);
        DrawString(hudX, hudY + 2, L"+=======================+", COLOR_CYAN);

        wchar_t szInfo[32];
        wsprintfW(szInfo, L" SCORE: %15d ", nScore);
        DrawString(hudX, hudY + 3, szInfo, COLOR_WHITE);

        wsprintfW(szInfo, L" HIGH:  %15d ", nHighScore);
        DrawString(hudX, hudY + 4, szInfo, COLOR_YELLOW);

        wsprintfW(szInfo, L" LINES: %15d ", nLinesCleared);
        DrawString(hudX, hudY + 5, szInfo, COLOR_GREEN);

        wsprintfW(szInfo, L" SPEED: %15d ", 21 - nSpeed);
        DrawString(hudX, hudY + 6, szInfo, COLOR_CYAN);

        // NEXT & HOLD Side-by-Side Preview Boxes (Full 4x4)
        DrawString(hudX, hudY + 7,  L"+----------+ +----------+", COLOR_GRAY);
        DrawString(hudX, hudY + 8,  L"|   NEXT   | |   HOLD   |", COLOR_WHITE);
        DrawString(hudX, hudY + 9,  L"|          | |          |", COLOR_GRAY);
        DrawString(hudX, hudY + 10, L"|          | |          |", COLOR_GRAY);
        DrawString(hudX, hudY + 11, L"|          | |          |", COLOR_GRAY);
        DrawString(hudX, hudY + 12, L"|          | |          |", COLOR_GRAY);
        DrawString(hudX, hudY + 13, L"+----------+ +----------+", COLOR_GRAY);

        // Render full 4x4 preview for NEXT
        DrawPiecePreview(hudX + 2, hudY + 9, nNextPiece);

        // Render full 4x4 preview for HOLD
        if (nHoldPiece != -1) {
            DrawPiecePreview(hudX + 15, hudY + 9, nHoldPiece);
        } else {
            DrawString(hudX + 17, hudY + 10, L"NONE", COLOR_DARK_GRAY);
        }

        DrawString(hudX, hudY + 14, L"-------------------------", COLOR_DARK_GRAY);
        DrawString(hudX, hudY + 15, L" <-/-> / A/D : Move", COLOR_WHITE);
        DrawString(hudX, hudY + 16, L" DOWN  / S   : Soft Drop", COLOR_WHITE);
        DrawString(hudX, hudY + 17, L" SPACE       : Hard Drop", COLOR_YELLOW);
        DrawString(hudX, hudY + 18, L" UP / W / Z  : Rotate", COLOR_WHITE);
        DrawString(hudX, hudY + 19, L" C / H       : Hold Piece", COLOR_CYAN);
        DrawString(hudX, hudY + 20, L" P           : Pause", COLOR_WHITE);
        DrawString(hudX, hudY + 21, L" Q / ESC     : Quit", COLOR_RED);
        DrawString(hudX, hudY + 22, L"-------------------------", COLOR_DARK_GRAY);
    }

    void LockPieceAndSpawn() {
        nPieceCount++;
        if (nPieceCount % 40 == 0) {
            if (nSpeed > 2) nSpeed--;
        }

        // 1. Lock active piece into pField
        for (int px = 0; px < 4; px++) {
            for (int py = 0; py < 4; py++) {
                if (tetromino[nCurrentPiece][Rotate(px, py, nCurrentRotation)] != L'.') {
                    int fx = nCurrentX + px;
                    int fy = nCurrentY + py;
                    if (fx >= 0 && fx < FIELD_WIDTH && fy >= 0 && fy < FIELD_HEIGHT) {
                        pField[fy * FIELD_WIDTH + fx] = nCurrentPiece + 1;
                    }
                }
            }
        }

        // 2. Check for completed lines
        vLines.clear();
        for (int py = 0; py < 4; py++) {
            int fy = nCurrentY + py;
            if (fy >= 0 && fy < FIELD_HEIGHT - 1) {
                bool bLine = true;
                for (int px = 1; px < FIELD_WIDTH - 1; px++) {
                    if (pField[fy * FIELD_WIDTH + px] == 0) {
                        bLine = false;
                        break;
                    }
                }
                if (bLine) {
                    for (int px = 1; px < FIELD_WIDTH - 1; px++)
                        pField[fy * FIELD_WIDTH + px] = 8;
                    vLines.push_back(fy);
                }
            }
        }

        // 3. Scoring
        nScore += 25;
        if (!vLines.empty()) {
            nScore += (1 << vLines.size()) * 100;
            nLinesCleared += static_cast<int>(vLines.size());
        }

        if (nScore > nHighScore) {
            nHighScore = nScore;
            SaveHighScore(nHighScore);
        }

        // 4. Animate line completion flash and shift before spawning next piece
        if (!vLines.empty()) {
            DrawGameScene(false);
            RenderFrame();
            Sleep(250);

            for (auto& v : vLines) {
                for (int px = 1; px < FIELD_WIDTH - 1; px++) {
                    for (int py = v; py > 0; py--)
                        pField[py * FIELD_WIDTH + px] = pField[(py - 1) * FIELD_WIDTH + px];
                    pField[px] = 0;
                }
            }
            vLines.clear();
        }

        // 5. Spawn next piece
        nCurrentX = FIELD_WIDTH / 2 - 2;
        nCurrentY = 0;
        nCurrentRotation = 0;
        nCurrentPiece = nNextPiece;
        nNextPiece = GetNextTetromino();
        bCanHold = true;

        // 6. Check Game Over condition
        if (!DoesPieceFit(nCurrentPiece, nCurrentRotation, nCurrentX, nCurrentY)) {
            bGameOver = true;
        }
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

        bagIndex = 7;
        nCurrentPiece = GetNextTetromino();
        nNextPiece = GetNextTetromino();
        nHoldPiece = -1;
        bCanHold = true;
        bHoldKeyHold = true;
        bSpaceKeyHold = true;
        bPauseKeyHold = true;
        bRotateHold = true;
        nLeftHoldCount = 0;
        nRightHoldCount = 0;

        nCurrentRotation = 0;
        nCurrentX = FIELD_WIDTH / 2 - 2;
        nCurrentY = 0;
        nSpeed = 20;
        nSpeedCount = 0;
        nPieceCount = 0;
        nLinesCleared = 0;
        nScore = 0;
        bGameOver = false;
        bPaused = false;
        vLines.clear();
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
            hConsole = NULL;
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

                // Minimum terminal size check
                if (nScreenWidth < MIN_LAYOUT_WIDTH || nScreenHeight < MIN_LAYOUT_HEIGHT) {
                    CHAR_INFO emptyCell;
                    emptyCell.Char.UnicodeChar = L' ';
                    emptyCell.Attributes = COLOR_WHITE;
                    fill(screen.begin(), screen.end(), emptyCell);

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
                        t1 = chrono::system_clock::now();
                    }
                } else {
                    bPauseKeyHold = true;
                }

                if (bPaused) {
                    CHAR_INFO emptyCell;
                    emptyCell.Char.UnicodeChar = L' ';
                    emptyCell.Attributes = COLOR_WHITE;
                    fill(screen.begin(), screen.end(), emptyCell);

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
                        int nextCur;
                        if (nHoldPiece == -1) {
                            nHoldPiece = nCurrentPiece;
                            nextCur = nNextPiece;
                            nNextPiece = GetNextTetromino();
                        } else {
                            nextCur = nHoldPiece;
                            nHoldPiece = nCurrentPiece;
                        }
                        int testX = FIELD_WIDTH / 2 - 2;
                        int testY = 0;
                        int testRot = 0;
                        if (DoesPieceFit(nextCur, testRot, testX, testY)) {
                            nCurrentPiece = nextCur;
                            nCurrentX = testX;
                            nCurrentY = testY;
                            nCurrentRotation = testRot;
                            bCanHold = false;
                            nSpeedCount = 0;
                        } else {
                            nCurrentPiece = nextCur;
                            nCurrentX = testX;
                            nCurrentY = testY;
                            nCurrentRotation = testRot;
                            bCanHold = false;
                            bGameOver = true;
                            break;
                        }
                        bHoldKeyHold = false;
                    }
                } else {
                    bHoldKeyHold = true;
                }

                // Hard Drop (Space)
                bool bHardDropped = false;
                bool bSpacePressed = (0x8000 & GetAsyncKeyState(VK_SPACE)) != 0;
                if (bSpacePressed) {
                    if (bSpaceKeyHold) {
                        int dropDist = 0;
                        while (DoesPieceFit(nCurrentPiece, nCurrentRotation, nCurrentX, nCurrentY + 1)) {
                            nCurrentY++;
                            dropDist++;
                        }
                        nScore += dropDist * 2;
                        if (nScore > nHighScore) {
                            nHighScore = nScore;
                            SaveHighScore(nHighScore);
                        }
                        bHardDropped = true;
                        bSpaceKeyHold = false;
                    }
                } else {
                    bSpaceKeyHold = true;
                }

                if ((0x8000 & GetAsyncKeyState('Q')) || (0x8000 & GetAsyncKeyState(VK_ESCAPE))) {
                    bExitApp = true;
                    break;
                }

                if (bHardDropped) {
                    // Hard drop directly locks the piece in position
                    LockPieceAndSpawn();
                } else {
                    // Lateral movement with DAS (Delayed Auto Shift)
                    bool bLeftKey = ((0x8000 & GetAsyncKeyState(VK_LEFT)) || (0x8000 & GetAsyncKeyState('A'))) != 0;
                    bool bRightKey = ((0x8000 & GetAsyncKeyState(VK_RIGHT)) || (0x8000 & GetAsyncKeyState('D'))) != 0;

                    if (bLeftKey) {
                        if (nLeftHoldCount == 0 || (nLeftHoldCount >= 4 && nLeftHoldCount % 2 == 0)) {
                            if (DoesPieceFit(nCurrentPiece, nCurrentRotation, nCurrentX - 1, nCurrentY))
                                nCurrentX--;
                        }
                        nLeftHoldCount++;
                    } else {
                        nLeftHoldCount = 0;
                    }

                    if (bRightKey) {
                        if (nRightHoldCount == 0 || (nRightHoldCount >= 4 && nRightHoldCount % 2 == 0)) {
                            if (DoesPieceFit(nCurrentPiece, nCurrentRotation, nCurrentX + 1, nCurrentY))
                                nCurrentX++;
                        }
                        nRightHoldCount++;
                    } else {
                        nRightHoldCount = 0;
                    }

                    // Soft Drop (Down / S)
                    bool bDownKey = ((0x8000 & GetAsyncKeyState(VK_DOWN)) || (0x8000 & GetAsyncKeyState('S'))) != 0;
                    if (bDownKey) {
                        if (DoesPieceFit(nCurrentPiece, nCurrentRotation, nCurrentX, nCurrentY + 1)) {
                            nCurrentY++;
                            nScore += 1;
                            nSpeedCount = 0;
                            if (nScore > nHighScore) {
                                nHighScore = nScore;
                                SaveHighScore(nHighScore);
                            }
                        }
                    }

                    // Rotation with Wall Kicks & Floor Kick
                    bool bRotateKey = ((0x8000 & GetAsyncKeyState(VK_UP)) || (0x8000 & GetAsyncKeyState('W')) ||
                                       (0x8000 & GetAsyncKeyState('Z'))) != 0;
                    if (bRotateKey) {
                        if (bRotateHold) {
                            int nextRot = (nCurrentRotation + 1) % 4;
                            if (DoesPieceFit(nCurrentPiece, nextRot, nCurrentX, nCurrentY)) {
                                nCurrentRotation = nextRot;
                            } else if (DoesPieceFit(nCurrentPiece, nextRot, nCurrentX - 1, nCurrentY)) {
                                nCurrentX -= 1; nCurrentRotation = nextRot; // Wall kick left
                            } else if (DoesPieceFit(nCurrentPiece, nextRot, nCurrentX + 1, nCurrentY)) {
                                nCurrentX += 1; nCurrentRotation = nextRot; // Wall kick right
                            } else if (DoesPieceFit(nCurrentPiece, nextRot, nCurrentX - 2, nCurrentY)) {
                                nCurrentX -= 2; nCurrentRotation = nextRot; // Wall kick left 2 (I piece)
                            } else if (DoesPieceFit(nCurrentPiece, nextRot, nCurrentX + 2, nCurrentY)) {
                                nCurrentX += 2; nCurrentRotation = nextRot; // Wall kick right 2 (I piece)
                            } else if (DoesPieceFit(nCurrentPiece, nextRot, nCurrentX, nCurrentY - 1)) {
                                nCurrentY -= 1; nCurrentRotation = nextRot; // Floor kick up
                            }
                            bRotateHold = false;
                        }
                    } else {
                        bRotateHold = true;
                    }

                    // Gravity
                    nSpeedCount++;
                    if (nSpeedCount >= nSpeed) {
                        nSpeedCount = 0;
                        if (DoesPieceFit(nCurrentPiece, nCurrentRotation, nCurrentX, nCurrentY + 1)) {
                            nCurrentY++;
                        } else {
                            LockPieceAndSpawn();
                        }
                    }
                }

                // Render current frame
                DrawGameScene(true);
                RenderFrame();
            }

            if (bExitApp) break;

            // Update high score one final time on game over
            if (nScore > nHighScore) {
                nHighScore = nScore;
                SaveHighScore(nHighScore);
            }

            // Game Over Screen
            CHAR_INFO emptyCell;
            emptyCell.Char.UnicodeChar = L' ';
            emptyCell.Attributes = COLOR_WHITE;
            fill(screen.begin(), screen.end(), emptyCell);

            int goY = max(0, nScreenHeight / 2 - 5);
            int goX = max(0, (nScreenWidth - 34) / 2);
            DrawString(goX, goY,     L"+--------------------------------+", COLOR_RED);
            DrawString(goX, goY + 1, L"|          GAME OVER!            |", COLOR_RED);
            DrawString(goX, goY + 2, L"+--------------------------------+", COLOR_RED);
            wchar_t szFinal[40];
            wsprintfW(szFinal,       L"|  Final Score:   %-14d |", nScore);
            DrawString(goX, goY + 3, szFinal, COLOR_YELLOW);
            wsprintfW(szFinal,       L"|  High Score:    %-14d |", nHighScore);
            DrawString(goX, goY + 4, szFinal, COLOR_CYAN);
            wsprintfW(szFinal,       L"|  Lines Cleared: %-14d |", nLinesCleared);
            DrawString(goX, goY + 5, szFinal, COLOR_GREEN);
            DrawString(goX, goY + 6, L"|                                |", COLOR_WHITE);
            DrawString(goX, goY + 7, L"|  Press 'R' to Play Again       |", COLOR_WHITE);
            DrawString(goX, goY + 8, L"|  Press 'Q' to Quit             |", COLOR_WHITE);
            DrawString(goX, goY + 9, L"+--------------------------------+", COLOR_RED);
            RenderFrame();

            // Wait for R to restart or Q to quit
            while (!bExitApp) {
                Sleep(20);
                UpdateTerminalDimensions();
                if ((0x8000 & GetAsyncKeyState('R')) != 0) {
                    while ((0x8000 & GetAsyncKeyState('R')) != 0) Sleep(10);
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