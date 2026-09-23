#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <thread>
#include <time.h>
#include <stdio.h>
#include <Windows.h>

using namespace std;

constexpr int SCREEN_WIDTH = 80;
constexpr int SCREEN_HEIGHT = 30;
constexpr int FIELD_WIDTH = 12;
constexpr int FIELD_HEIGHT = 18;

class TetrisGame {
private:
    wstring tetromino[7];
    unsigned char* pField = nullptr;
    wchar_t* screen = nullptr;
    HANDLE hConsole;
    DWORD dwBytesWritten = 0;

    int nCurrentPiece;
    int nCurrentRotation;
    int nCurrentX;
    int nCurrentY;
    int nSpeed;
    int nSpeedCount;
    int nPieceCount;
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

public:
    TetrisGame() {
        // Initialize random seed
        srand(static_cast<unsigned int>(time(NULL)));
        
        screen = new wchar_t[SCREEN_WIDTH * SCREEN_HEIGHT];
        for (int i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; i++) screen[i] = L' ';
        hConsole = CreateConsoleScreenBuffer(GENERIC_READ | GENERIC_WRITE, 0, NULL, CONSOLE_TEXTMODE_BUFFER, NULL);
        SetConsoleActiveScreenBuffer(hConsole);

        // Hide Cursor
        CONSOLE_CURSOR_INFO cursorInfo;
        GetConsoleCursorInfo(hConsole, &cursorInfo);
        cursorInfo.bVisible = false;
        SetConsoleCursorInfo(hConsole, &cursorInfo);

        tetromino[0].append(L"..X...X...X...X."); 
        tetromino[1].append(L"..X..XX...X.....");
        tetromino[2].append(L".....XX..XX.....");
        tetromino[3].append(L"..X..XX..X......");
        tetromino[4].append(L".X...XX...X.....");
        tetromino[5].append(L".X...X...XX.....");
        tetromino[6].append(L"..X...X..XX.....");

        pField = new unsigned char[FIELD_WIDTH * FIELD_HEIGHT]; 
        for (int x = 0; x < FIELD_WIDTH; x++) 
            for (int y = 0; y < FIELD_HEIGHT; y++)
                pField[y * FIELD_WIDTH + x] = (x == 0 || x == FIELD_WIDTH - 1 || y == FIELD_HEIGHT - 1) ? 9 : 0;

        nCurrentPiece = rand() % 7;
        nCurrentRotation = 0;
        nCurrentX = FIELD_WIDTH / 2;
        nCurrentY = 0;
        nSpeed = 20;
        nSpeedCount = 0;
        bRotateHold = true;
        nPieceCount = 0;
        nScore = 0;
        bGameOver = false;
    }

    ~TetrisGame() {
        delete[] screen;
        delete[] pField;
        CloseHandle(hConsole);
    }

    void Run() {
        auto t1 = chrono::system_clock::now();
        
        while (!bGameOver) {
            // Timing - we want a loop to run approx every 50ms
            auto t2 = chrono::system_clock::now();
            chrono::duration<float> elapsedTime = t2 - t1;
            
            // Wait until 50ms has passed since last tick
            if (elapsedTime.count() < 0.05f) {
                continue;
            }
            t1 = t2;
            
            nSpeedCount++;
            bool bForceDown = (nSpeedCount == nSpeed);

            // Input
            for (int k = 0; k < 4; k++)
                bKey[k] = (0x8000 & GetAsyncKeyState((unsigned char)("\x27\x25\x28Z"[k]))) != 0;

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
                    if (!vLines.empty()) nScore += (1 << vLines.size()) * 100;

                    nCurrentX = FIELD_WIDTH / 2;
                    nCurrentY = 0;
                    nCurrentRotation = 0;
                    nCurrentPiece = rand() % 7;

                    bGameOver = !DoesPieceFit(nCurrentPiece, nCurrentRotation, nCurrentX, nCurrentY);
                }
            }

            // Draw Field
            for (int x = 0; x < FIELD_WIDTH; x++)
                for (int y = 0; y < FIELD_HEIGHT; y++)
                    screen[(y + 2) * SCREEN_WIDTH + (x + 2)] = L" ABCDEFG=#"[pField[y * FIELD_WIDTH + x]];

            // Draw Current Piece
            for (int px = 0; px < 4; px++)
                for (int py = 0; py < 4; py++)
                    if (tetromino[nCurrentPiece][Rotate(px, py, nCurrentRotation)] != L'.')
                        screen[(nCurrentY + py + 2) * SCREEN_WIDTH + (nCurrentX + px + 2)] = nCurrentPiece + 65;

            // Draw Score
            wsprintfW(&screen[2 * SCREEN_WIDTH + FIELD_WIDTH + 6], L"SCORE: %8d", nScore);

            // Animate Line Completion
            if (!vLines.empty()) {
                WriteConsoleOutputCharacterW(hConsole, screen, SCREEN_WIDTH * SCREEN_HEIGHT, { 0,0 }, &dwBytesWritten);
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
            WriteConsoleOutputCharacterW(hConsole, screen, SCREEN_WIDTH * SCREEN_HEIGHT, { 0,0 }, &dwBytesWritten);
        }
        
        CloseHandle(hConsole);
        cout << "Game Over!! Score:" << nScore << endl;
        system("pause");
    }
};

int main() {
    TetrisGame game;
    game.Run();
    return 0;
}