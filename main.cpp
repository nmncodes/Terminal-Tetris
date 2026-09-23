#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <time.h>
#include <stdio.h>
#include <algorithm>
#include <Windows.h>

using namespace std;

constexpr int FIELD_WIDTH = 12;
constexpr int FIELD_HEIGHT = 18;
constexpr int MIN_LAYOUT_WIDTH = 36;
constexpr int MIN_LAYOUT_HEIGHT = 22;

class TetrisGame {
private:
    wstring tetromino[7];
    unsigned char* pField = nullptr;
    vector<wchar_t> screen;
    int nScreenWidth = 0;
    int nScreenHeight = 0;

    HANDLE hConsole = NULL;
    HANDLE hOriginalConsole = NULL;
    DWORD dwBytesWritten = 0;

    int nCurrentPiece;
    int nNextPiece;
    int nCurrentRotation;
    int nCurrentX;
    int nCurrentY;
    int nSpeed;
    int nSpeedCount;
    int nPieceCount;
    int nLinesCleared;
    int nScore;
    vector<int> vLines;
    bool bGameOver;
    bool bRotateHold;
    bool bKey[4];

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

            // Ensure the console screen buffer accommodates the window
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
                screen.assign(nScreenWidth * nScreenHeight, L' ');
            }
        }
    }

    void DrawString(int x, int y, const wstring& str) {
        for (size_t i = 0; i < str.length(); i++) {
            int sx = x + static_cast<int>(i);
            int sy = y;
            if (sx >= 0 && sx < nScreenWidth && sy >= 0 && sy < nScreenHeight) {
                screen[sy * nScreenWidth + sx] = str[i];
            }
        }
    }

    void RenderFrame() {
        if (nScreenWidth <= 0 || nScreenHeight <= 0 || screen.empty()) return;

        CONSOLE_SCREEN_BUFFER_INFO csbi;
        if (!GetConsoleScreenBufferInfo(hConsole, &csbi)) return;

        for (int y = 0; y < nScreenHeight; y++) {
            COORD coord = { csbi.srWindow.Left, static_cast<SHORT>(csbi.srWindow.Top + y) };
            WriteConsoleOutputCharacterW(
                hConsole,
                &screen[y * nScreenWidth],
                nScreenWidth,
                coord,
                &dwBytesWritten
            );
        }
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
        for (int x = 0; x < FIELD_WIDTH; x++)
            for (int y = 0; y < FIELD_HEIGHT; y++)
                pField[y * FIELD_WIDTH + x] = (x == 0 || x == FIELD_WIDTH - 1 || y == FIELD_HEIGHT - 1) ? 9 : 0;

        nCurrentPiece = rand() % 7;
        nNextPiece = rand() % 7;
        nCurrentRotation = 0;
        nCurrentX = FIELD_WIDTH / 2;
        nCurrentY = 0;
        nSpeed = 20;
        nSpeedCount = 0;
        bRotateHold = true;
        nPieceCount = 0;
        nLinesCleared = 0;
        nScore = 0;
        bGameOver = false;
    }

    ~TetrisGame() {
        delete[] pField;
        if (hConsole) {
            SetConsoleActiveScreenBuffer(hOriginalConsole);
            CloseHandle(hConsole);
        }
    }

    void Run() {
        auto t1 = chrono::system_clock::now();

        while (!bGameOver) {
            auto t2 = chrono::system_clock::now();
            chrono::duration<float> elapsedTime = t2 - t1;

            if (elapsedTime.count() < 0.05f) {
                Sleep(5);
                continue;
            }
            t1 = t2;

            // Dynamically detect and update terminal dimensions
            UpdateTerminalDimensions();

            // Clear frame buffer
            fill(screen.begin(), screen.end(), L' ');

            // Check if terminal is too small to render comfortably
            if (nScreenWidth < MIN_LAYOUT_WIDTH || nScreenHeight < MIN_LAYOUT_HEIGHT) {
                int warnY = max(0, nScreenHeight / 2 - 3);
                int warnX = max(0, (nScreenWidth - 26) / 2);
                DrawString(warnX, warnY,     L"+------------------------+");
                DrawString(warnX, warnY + 1, L"|  TERMINAL TOO SMALL!   |");
                wchar_t szSize[32];
                wsprintfW(szSize, L"|  Current: %3dx%-3d       |", nScreenWidth, nScreenHeight);
                DrawString(warnX, warnY + 2, szSize);
                wsprintfW(szSize, L"|  Minimum: %3dx%-3d       |", MIN_LAYOUT_WIDTH, MIN_LAYOUT_HEIGHT);
                DrawString(warnX, warnY + 3, szSize);
                DrawString(warnX, warnY + 4, L"|  Please resize window  |");
                DrawString(warnX, warnY + 5, L"+------------------------+");

                RenderFrame();

                if ((0x8000 & GetAsyncKeyState('Q')) || (0x8000 & GetAsyncKeyState(VK_ESCAPE))) {
                    bGameOver = true;
                }
                continue;
            }

            nSpeedCount++;
            bool bForceDown = (nSpeedCount == nSpeed);

            // Input handling: Arrow keys, WASD, Z / Space, Q / ESC
            bKey[0] = ((0x8000 & GetAsyncKeyState(VK_RIGHT)) || (0x8000 & GetAsyncKeyState('D'))) != 0;
            bKey[1] = ((0x8000 & GetAsyncKeyState(VK_LEFT))  || (0x8000 & GetAsyncKeyState('A'))) != 0;
            bKey[2] = ((0x8000 & GetAsyncKeyState(VK_DOWN))  || (0x8000 & GetAsyncKeyState('S'))) != 0;
            bKey[3] = ((0x8000 & GetAsyncKeyState(VK_UP))    || (0x8000 & GetAsyncKeyState('W')) ||
                       (0x8000 & GetAsyncKeyState('Z'))       || (0x8000 & GetAsyncKeyState(VK_SPACE))) != 0;

            if ((0x8000 & GetAsyncKeyState('Q')) || (0x8000 & GetAsyncKeyState(VK_ESCAPE))) {
                bGameOver = true;
                break;
            }

            // Logic
            nCurrentX += (bKey[0] && DoesPieceFit(nCurrentPiece, nCurrentRotation, nCurrentX + 1, nCurrentY)) ? 1 : 0;
            nCurrentX -= (bKey[1] && DoesPieceFit(nCurrentPiece, nCurrentRotation, nCurrentX - 1, nCurrentY)) ? 1 : 0;
            nCurrentY += (bKey[2] && DoesPieceFit(nCurrentPiece, nCurrentRotation, nCurrentX, nCurrentY + 1)) ? 1 : 0;

            if (bKey[3]) {
                nCurrentRotation += (bRotateHold && DoesPieceFit(nCurrentPiece, nCurrentRotation + 1, nCurrentX, nCurrentY)) ? 1 : 0;
                bRotateHold = false;
            }
            else {
                bRotateHold = true;
            }

            if (bForceDown) {
                // Update difficulty every 50 pieces
                nSpeedCount = 0;
                nPieceCount++;
                if (nPieceCount % 50 == 0)
                    if (nSpeed >= 10) nSpeed--;

                if (DoesPieceFit(nCurrentPiece, nCurrentRotation, nCurrentX, nCurrentY + 1))
                    nCurrentY++;
                else {
                    for (int px = 0; px < 4; px++)
                        for (int py = 0; py < 4; py++)
                            if (tetromino[nCurrentPiece][Rotate(px, py, nCurrentRotation)] != L'.')
                                pField[(nCurrentY + py) * FIELD_WIDTH + (nCurrentX + px)] = nCurrentPiece + 1;

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

                    nCurrentX = FIELD_WIDTH / 2;
                    nCurrentY = 0;
                    nCurrentRotation = 0;
                    nCurrentPiece = nNextPiece;
                    nNextPiece = rand() % 7;

                    bGameOver = !DoesPieceFit(nCurrentPiece, nCurrentRotation, nCurrentX, nCurrentY);
                }
            }

            // Calculate responsive centering offsets
            int totalLayoutWidth = FIELD_WIDTH + 3 + 18;
            int totalLayoutHeight = FIELD_HEIGHT;
            int nOffsetX = max(1, (nScreenWidth - totalLayoutWidth) / 2);
            int nOffsetY = max(1, (nScreenHeight - totalLayoutHeight) / 2);

            // Draw Field
            for (int x = 0; x < FIELD_WIDTH; x++)
                for (int y = 0; y < FIELD_HEIGHT; y++) {
                    int sx = nOffsetX + x;
                    int sy = nOffsetY + y;
                    if (sx >= 0 && sx < nScreenWidth && sy >= 0 && sy < nScreenHeight)
                        screen[sy * nScreenWidth + sx] = L" ABCDEFG=#"[pField[y * FIELD_WIDTH + x]];
                }

            // Draw Current Piece
            for (int px = 0; px < 4; px++)
                for (int py = 0; py < 4; py++)
                    if (tetromino[nCurrentPiece][Rotate(px, py, nCurrentRotation)] != L'.') {
                        int sx = nOffsetX + nCurrentX + px;
                        int sy = nOffsetY + nCurrentY + py;
                        if (sx >= 0 && sx < nScreenWidth && sy >= 0 && sy < nScreenHeight)
                            screen[sy * nScreenWidth + sx] = nCurrentPiece + 65;
                    }

            // Draw Responsive Side HUD
            int hudX = nOffsetX + FIELD_WIDTH + 3;
            int hudY = nOffsetY;

            DrawString(hudX, hudY,     L"==================");
            DrawString(hudX, hudY + 1, L"   T E T R I S    ");
            DrawString(hudX, hudY + 2, L"==================");

            wchar_t szInfo[32];
            wsprintfW(szInfo, L"SCORE: %8d", nScore);
            DrawString(hudX, hudY + 4, szInfo);

            wsprintfW(szInfo, L"LINES: %8d", nLinesCleared);
            DrawString(hudX, hudY + 5, szInfo);

            wsprintfW(szInfo, L"SPEED: %8d", 21 - nSpeed);
            DrawString(hudX, hudY + 6, szInfo);

            // Next Piece Preview
            DrawString(hudX, hudY + 8, L"NEXT:");
            for (int px = 0; px < 4; px++) {
                for (int py = 0; py < 2; py++) {
                    wchar_t c = (tetromino[nNextPiece][py * 4 + px] != L'.') ? (nNextPiece + 65) : L' ';
                    int sx = hudX + 6 + px;
                    int sy = hudY + 8 + py;
                    if (sx >= 0 && sx < nScreenWidth && sy >= 0 && sy < nScreenHeight) {
                        screen[sy * nScreenWidth + sx] = c;
                    }
                }
            }

            DrawString(hudX, hudY + 11, L"------------------");
            DrawString(hudX, hudY + 12, L"CONTROLS:");
            DrawString(hudX, hudY + 13, L"  <-/->/A/D: Move");
            DrawString(hudX, hudY + 14, L"  DOWN / S : Drop");
            DrawString(hudX, hudY + 15, L"  UP/W/Z/Spc: Turn");
            DrawString(hudX, hudY + 16, L"  Q / ESC  : Quit");
            DrawString(hudX, hudY + 17, L"------------------");

            // Animate Line Completion
            if (!vLines.empty()) {
                RenderFrame();
                Sleep(400);

                for (auto& v : vLines)
                    for (int px = 1; px < FIELD_WIDTH - 1; px++) {
                        for (int py = v; py > 0; py--)
                            pField[py * FIELD_WIDTH + px] = pField[(py - 1) * FIELD_WIDTH + px];
                        pField[px] = 0;
                    }

                vLines.clear();
            }

            // Display Frame
            RenderFrame();
        }

        // Game Over screen display
        fill(screen.begin(), screen.end(), L' ');
        int goY = max(0, nScreenHeight / 2 - 3);
        int goX = max(0, (nScreenWidth - 26) / 2);
        DrawString(goX, goY,     L"+------------------------+");
        DrawString(goX, goY + 1, L"|      GAME OVER!        |");
        wchar_t szScore[32];
        wsprintfW(szScore, L"|  Final Score: %-8d |", nScore);
        DrawString(goX, goY + 2, szScore);
        wsprintfW(szScore, L"|  Lines:       %-8d |", nLinesCleared);
        DrawString(goX, goY + 3, szScore);
        DrawString(goX, goY + 4, L"+------------------------+");
        RenderFrame();
        Sleep(1500);

        // Restore original console buffer
        SetConsoleActiveScreenBuffer(hOriginalConsole);
        CloseHandle(hConsole);
        hConsole = NULL;

        cout << "\nGame Over! Final Score: " << nScore << " | Lines Cleared: " << nLinesCleared << endl;
        system("pause");
    }
};

int main() {
    TetrisGame game;
    game.Run();
    return 0;
}