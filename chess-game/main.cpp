#include <windows.h>
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

const int CELL = 80;
const int BOARD_PADDING = 40;
const int BOARD_SIZE = CELL * 8;
const int SIDE_PANEL_WIDTH = 300;
const int WINDOW_WIDTH = BOARD_PADDING * 2 + BOARD_SIZE + SIDE_PANEL_WIDTH + 20;
const int WINDOW_HEIGHT = BOARD_PADDING * 2 + BOARD_SIZE + 80;

struct GameState {
    char board[8][8];
    int selX = -1, selY = -1;
    bool whiteTurn = true;
    bool legalMoves[8][8] = {{false}};
    int moveCount = 0;
    bool whiteKingMoved = false;
    bool blackKingMoved = false;
    bool whiteRookKMoved = false;
    bool whiteRookQMoved = false;
    bool blackRookKMoved = false;
    bool blackRookQMoved = false;
    std::vector<std::wstring> moveHistory;
    char lastCaptured = '.';
    bool gameOver = false;
    std::wstring gameResult;
    int lastMoveFromX = -1, lastMoveFromY = -1;
    int lastMoveToX = -1, lastMoveToY = -1;
    int whiteCaptures = 0;
    int blackCaptures = 0;
};

GameState game;
HWND hMainWnd = NULL;
HWND hStatus = NULL;
HWND hMoveList = NULL;
HWND hNewGameBtn = NULL;
HWND hUndoBtn = NULL;
HWND hWhiteCapturesLabel = NULL;
HWND hBlackCapturesLabel = NULL;
HFONT hFontStatus = NULL;
HFONT hFontPiece = NULL;
HFONT hFontMoves = NULL;
HFONT hFontLabel = NULL;

struct Move {
    int sx, sy, tx, ty;
    char captured;
    bool wasKingMove;
    bool wasRookMove;
};

std::vector<Move> moveStack;

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void initBoard();
void drawBoard(HDC hdc);
void drawPiece(HDC hdc, int x, int y, char piece);
void drawSidePanel(HDC hdc);
void drawCoordinates(HDC hdc);
bool isInside(int x, int y) { return x >= 0 && x < 8 && y >= 0 && y < 8; }
bool isWhitePiece(char p) { return p >= 'A' && p <= 'Z'; }
bool isBlackPiece(char p) { return p >= 'a' && p <= 'z'; }
bool sameColor(char a, char b);
bool isLegalMove(int sx, int sy, int tx, int ty);
bool isSquareAttacked(int x, int y, bool byWhite);
bool isInCheck(bool white);
bool hasLegalMoves(bool white);
std::wstring pieceToUnicode(char p);
std::wstring pieceToName(char p);
std::wstring posToNotation(int x, int y);
void makeMove(int sx, int sy, int tx, int ty);
void undoMove();
void updateStatus();
void updateMoveList();
void computeLegalMoves(int sx, int sy);
void clearLegalMoves();
void checkGameEnd();
void newGame();
bool clearPath(int sx, int sy, int tx, int ty);

void initBoard() {
    const char* start[8] = {
        "rnbqkbnr",
        "pppppppp",
        "........",
        "........",
        "........",
        "........",
        "PPPPPPPP",
        "RNBQKBNR"
    };
    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
            game.board[y][x] = start[y][x];
        }
    }
    game.selX = game.selY = -1;
    game.whiteTurn = true;
    game.moveCount = 0;
    game.whiteKingMoved = game.blackKingMoved = false;
    game.whiteRookKMoved = game.whiteRookQMoved = false;
    game.blackRookKMoved = game.blackRookQMoved = false;
    game.moveHistory.clear();
    game.gameOver = false;
    game.gameResult.clear();
    game.lastMoveFromX = game.lastMoveFromY = -1;
    game.lastMoveToX = game.lastMoveToY = -1;
    game.whiteCaptures = game.blackCaptures = 0;
    moveStack.clear();
    clearLegalMoves();
}

std::wstring pieceToUnicode(char p) {
    switch (p) {
        case 'K': return L"\u2654";
        case 'Q': return L"\u2655";
        case 'R': return L"\u2656";
        case 'B': return L"\u2657";
        case 'N': return L"\u2658";
        case 'P': return L"\u2659";
        case 'k': return L"\u265A";
        case 'q': return L"\u265B";
        case 'r': return L"\u265C";
        case 'b': return L"\u265D";
        case 'n': return L"\u265E";
        case 'p': return L"\u265F";
        default: return L" ";
    }
}

std::wstring pieceToName(char p) {
    switch (toupper(p)) {
        case 'K': return L"King";
        case 'Q': return L"Queen";
        case 'R': return L"Rook";
        case 'B': return L"Bishop";
        case 'N': return L"Knight";
        case 'P': return L"Pawn";
        default: return L"";
    }
}

std::wstring posToNotation(int x, int y) {
    wchar_t col = L'a' + x;
    wchar_t row = L'8' - y;
    std::wstring result;
    result += col;
    result += row;
    return result;
}

bool sameColor(char a, char b) {
    if (a == '.' || b == '.') return false;
    return (isWhitePiece(a) && isWhitePiece(b)) || (isBlackPiece(a) && isBlackPiece(b));
}

bool clearPath(int sx, int sy, int tx, int ty) {
    int dx = (tx > sx) ? 1 : (tx < sx) ? -1 : 0;
    int dy = (ty > sy) ? 1 : (ty < sy) ? -1 : 0;
    int x = sx + dx, y = sy + dy;
    while (x != tx || y != ty) {
        if (game.board[y][x] != '.') return false;
        x += dx; y += dy;
    }
    return true;
}

bool isSquareAttacked(int x, int y, bool byWhite) {
    for (int sy = 0; sy < 8; ++sy) {
        for (int sx = 0; sx < 8; ++sx) {
            char p = game.board[sy][sx];
            if (p == '.') continue;
            if (byWhite && !isWhitePiece(p)) continue;
            if (!byWhite && !isBlackPiece(p)) continue;
            int dx = x - sx;
            int dy = y - sy;
            int adx = abs(dx);
            int ady = abs(dy);
            switch (toupper(p)) {
                case 'P': {
                    int dir = byWhite ? -1 : 1;
                    if (abs(dx) == 1 && dy == dir) return true;
                    break;
                }
                case 'N': {
                    if ((adx == 1 && ady == 2) || (adx == 2 && ady == 1)) return true;
                    break;
                }
                case 'B': {
                    if (adx == ady && adx > 0 && clearPath(sx, sy, x, y)) return true;
                    break;
                }
                case 'R': {
                    if (((adx == 0 && ady > 0) || (ady == 0 && adx > 0)) && clearPath(sx, sy, x, y)) return true;
                    break;
                }
                case 'Q': {
                    if (((adx == ady && adx > 0) || (adx == 0 && ady > 0) || (ady == 0 && adx > 0)) && clearPath(sx, sy, x, y)) return true;
                    break;
                }
                case 'K': {
                    if ((std::max)(adx, ady) == 1) return true;
                    break;
                }
            }
        }
    }
    return false;
}

bool isInCheck(bool white) {
    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
            char p = game.board[y][x];
            if ((white && p == 'K') || (!white && p == 'k')) {
                return isSquareAttacked(x, y, !white);
            }
        }
    }
    return false;
}

bool wouldBeInCheck(int sx, int sy, int tx, int ty, bool white) {
    char temp = game.board[ty][tx];
    game.board[ty][tx] = game.board[sy][sx];
    game.board[sy][sx] = '.';
    bool check = isInCheck(white);
    game.board[sy][sx] = game.board[ty][tx];
    game.board[ty][tx] = temp;
    return check;
}

bool isLegalMove(int sx, int sy, int tx, int ty) {
    if (!isInside(sx, sy) || !isInside(tx, ty)) return false;
    if (sx == tx && sy == ty) return false;
    char p = game.board[sy][sx];
    if (p == '.') return false;
    if (sameColor(p, game.board[ty][tx])) return false;
    int dx = tx - sx;
    int dy = ty - sy;
    int adx = abs(dx);
    int ady = abs(dy);
    bool white = isWhitePiece(p);
    switch (toupper(p)) {
        case 'P': {
            int dir = white ? -1 : 1;
            int startRow = white ? 6 : 1;
            if (dx == 0 && dy == dir && game.board[ty][tx] == '.') break;
            else if (dx == 0 && dy == 2 * dir && sy == startRow && game.board[sy + dir][sx] == '.' && game.board[ty][tx] == '.') break;
            else if (abs(dx) == 1 && dy == dir && game.board[ty][tx] != '.' && !sameColor(p, game.board[ty][tx])) break;
            else return false;
            break;
        }
        case 'N': {
            if (!((adx == 1 && ady == 2) || (adx == 2 && ady == 1))) return false;
            break;
        }
        case 'B': {
            if (!(adx == ady && adx > 0) || !clearPath(sx, sy, tx, ty)) return false;
            break;
        }
        case 'R': {
            if (!((adx == 0 && ady > 0) || (ady == 0 && adx > 0)) || !clearPath(sx, sy, tx, ty)) return false;
            break;
        }
        case 'Q': {
            if (!((adx == ady && adx > 0) || (adx == 0 && ady > 0) || (ady == 0 && adx > 0)) || !clearPath(sx, sy, tx, ty)) return false;
            break;
        }
        case 'K': {
            if ((std::max)(adx, ady) != 1) return false;
            break;
        }
    }
    return !wouldBeInCheck(sx, sy, tx, ty, white);
}

bool hasLegalMoves(bool white) {
    for (int sy = 0; sy < 8; ++sy) {
        for (int sx = 0; sx < 8; ++sx) {
            char p = game.board[sy][sx];
            if (p == '.') continue;
            if (white && !isWhitePiece(p)) continue;
            if (!white && !isBlackPiece(p)) continue;
            for (int ty = 0; ty < 8; ++ty) {
                for (int tx = 0; tx < 8; ++tx) {
                    if (isLegalMove(sx, sy, tx, ty)) {
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

void makeMove(int sx, int sy, int tx, int ty) {
    Move m;
    m.sx = sx; m.sy = sy; m.tx = tx; m.ty = ty;
    m.captured = game.board[ty][tx];
    m.wasKingMove = false;
    m.wasRookMove = false;
    char p = game.board[sy][sx];

    if (m.captured != '.') {
        if (isWhitePiece(p)) game.whiteCaptures++;
        else game.blackCaptures++;
    }

    if (toupper(p) == 'K') {
        m.wasKingMove = true;
        if (isWhitePiece(p)) game.whiteKingMoved = true;
        else game.blackKingMoved = true;
    }
    if (toupper(p) == 'R') {
        m.wasRookMove = true;
        if (isWhitePiece(p)) {
            if (sx == 7 && sy == 7) game.whiteRookKMoved = true;
            if (sx == 0 && sy == 7) game.whiteRookQMoved = true;
        } else {
            if (sx == 7 && sy == 0) game.blackRookKMoved = true;
            if (sx == 0 && sy == 0) game.blackRookQMoved = true;
        }
    }
    moveStack.push_back(m);
    game.board[ty][tx] = p;
    game.board[sy][sx] = '.';

    game.lastMoveFromX = sx;
    game.lastMoveFromY = sy;
    game.lastMoveToX = tx;
    game.lastMoveToY = ty;

    if (game.board[ty][tx] == 'P' && ty == 0) game.board[ty][tx] = 'Q';
    if (game.board[ty][tx] == 'p' && ty == 7) game.board[ty][tx] = 'q';
    std::wstring moveStr = posToNotation(sx, sy) + L"-" + posToNotation(tx, ty);
    if (m.captured != '.') {
        moveStr += L" x" + pieceToName(m.captured);
    }
    game.moveHistory.push_back(moveStr);
    game.moveCount++;
}

void undoMove() {
    if (moveStack.empty()) return;
    Move m = moveStack.back();
    moveStack.pop_back();
    game.board[m.sy][m.sx] = game.board[m.ty][m.tx];
    game.board[m.ty][m.tx] = m.captured;

    if (m.captured != '.') {
        char p = game.board[m.sy][m.sx];
        if (isWhitePiece(p)) game.whiteCaptures--;
        else game.blackCaptures--;
    }

    if (!game.moveHistory.empty()) {
        game.moveHistory.pop_back();
    }
    game.moveCount--;
    game.whiteTurn = !game.whiteTurn;
    game.gameOver = false;

    if (moveStack.size() > 0) {
        Move &prev = moveStack.back();
        game.lastMoveFromX = prev.sx;
        game.lastMoveFromY = prev.sy;
        game.lastMoveToX = prev.tx;
        game.lastMoveToY = prev.ty;
    } else {
        game.lastMoveFromX = game.lastMoveFromY = -1;
        game.lastMoveToX = game.lastMoveToY = -1;
    }

    updateMoveList();
    updateStatus();
    InvalidateRect(hMainWnd, NULL, TRUE);
}

void checkGameEnd() {
    bool currentPlayerHasMoves = hasLegalMoves(game.whiteTurn);
    if (!currentPlayerHasMoves) {
        game.gameOver = true;
        if (isInCheck(game.whiteTurn)) {
            game.gameResult = game.whiteTurn ? L"Checkmate! Black Wins!" : L"Checkmate! White Wins!";
        } else {
            game.gameResult = L"Stalemate! Draw.";
        }
    }
}

void computeLegalMoves(int sx, int sy) {
    clearLegalMoves();
    if (!isInside(sx, sy)) return;
    char p = game.board[sy][sx];
    if (p == '.') return;
    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
            if (isLegalMove(sx, sy, x, y)) {
                game.legalMoves[y][x] = true;
            }
        }
    }
    game.legalMoves[sy][sx] = false;
}

void clearLegalMoves() {
    for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 8; ++x)
            game.legalMoves[y][x] = false;
}

void drawCoordinates(HDC hdc) {
    if (!hFontLabel) {
        hFontLabel = CreateFontW(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                 DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                 ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    }
    HFONT hOld = (HFONT)SelectObject(hdc, hFontLabel);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(100, 100, 100));

    for (int i = 0; i < 8; ++i) {
        wchar_t col = L'a' + i;
        wchar_t row = L'8' - i;
        std::wstring colStr; colStr += col;
        std::wstring rowStr; rowStr += row;

        RECT colRect = {BOARD_PADDING + i * CELL, BOARD_PADDING + BOARD_SIZE + 5,
                        BOARD_PADDING + (i + 1) * CELL, BOARD_PADDING + BOARD_SIZE + 25};
        DrawTextW(hdc, colStr.c_str(), 1, &colRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        RECT rowRect = {5, BOARD_PADDING + i * CELL,
                        BOARD_PADDING - 10, BOARD_PADDING + (i + 1) * CELL};
        DrawTextW(hdc, rowStr.c_str(), 1, &rowRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
    SelectObject(hdc, hOld);
}

void drawBoard(HDC hdc) {
    if (!hFontPiece) {
        hFontPiece = CreateFontW(CELL - 15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                 DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                 ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI Symbol");
    }
    HFONT hOld = (HFONT)SelectObject(hdc, hFontPiece);

    HBRUSH hBorderBrush = CreateSolidBrush(RGB(50, 40, 30));
    RECT borderRect = {BOARD_PADDING - 5, BOARD_PADDING - 5,
                       BOARD_PADDING + BOARD_SIZE + 5, BOARD_PADDING + BOARD_SIZE + 5};
    FrameRect(hdc, &borderRect, hBorderBrush);
    DeleteObject(hBorderBrush);

    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
            int px = BOARD_PADDING + x * CELL;
            int py = BOARD_PADDING + y * CELL;
            RECT cellRect = { px, py, px + CELL, py + CELL };
            bool light = ((x + y) % 2 == 0);

            COLORREF lightColor = RGB(240, 217, 181);
            COLORREF darkColor = RGB(181, 136, 99);

            if (x == game.lastMoveFromX && y == game.lastMoveFromY) {
                lightColor = RGB(205, 210, 106);
                darkColor = RGB(170, 162, 58);
            } else if (x == game.lastMoveToX && y == game.lastMoveToY) {
                lightColor = RGB(205, 210, 106);
                darkColor = RGB(170, 162, 58);
            }

            HBRUSH hBrush = CreateSolidBrush(light ? lightColor : darkColor);
            FillRect(hdc, &cellRect, hBrush);
            DeleteObject(hBrush);

            if (x == game.selX && y == game.selY) {
                HPEN hPenSel = CreatePen(PS_SOLID, 4, RGB(70, 130, 180));
                HPEN hOldPen = (HPEN)SelectObject(hdc, hPenSel);
                HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
                RoundRect(hdc, px + 4, py + 4, px + CELL - 4, py + CELL - 4, 8, 8);
                SelectObject(hdc, hOldPen);
                SelectObject(hdc, hOldBrush);
                DeleteObject(hPenSel);
            }

            if (game.legalMoves[y][x]) {
                int cx = px + CELL / 2;
                int cy = py + CELL / 2;
                bool isCapture = game.board[y][x] != '.';

                if (isCapture) {
                    HPEN hPen = CreatePen(PS_SOLID, 5, RGB(220, 50, 50));
                    HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
                    HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
                    Ellipse(hdc, px + 8, py + 8, px + CELL - 8, py + CELL - 8);
                    SelectObject(hdc, hOldPen);
                    SelectObject(hdc, hOldBrush);
                    DeleteObject(hPen);
                } else {
                    int r = CELL / 8;
                    COLORREF color = RGB(100, 160, 100);
                    HBRUSH hDot = CreateSolidBrush(color);
                    HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, hDot);
                    HPEN hPen = CreatePen(PS_SOLID, 1, color);
                    HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
                    Ellipse(hdc, cx - r, cy - r, cx + r, cy + r);
                    SelectObject(hdc, hOldPen);
                    SelectObject(hdc, hOldBrush);
                    DeleteObject(hPen);
                    DeleteObject(hDot);
                }
            }

            char p = game.board[y][x];
            if (p != '.') {
                drawPiece(hdc, px, py, p);
            }
        }
    }

    drawCoordinates(hdc);
    SelectObject(hdc, hOld);
}

void drawPiece(HDC hdc, int x, int y, char piece) {
    std::wstring s = pieceToUnicode(piece);
    RECT r = { x, y, x + CELL, y + CELL };
    SetBkMode(hdc, TRANSPARENT);

    COLORREF shadowColor = RGB(0, 0, 0);
    SetTextColor(hdc, shadowColor);
    RECT shadowRect = { x + 2, y + 2, x + CELL + 2, y + CELL + 2 };
    DrawTextW(hdc, s.c_str(), (int)s.size(), &shadowRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    SetTextColor(hdc, isWhitePiece(piece) ? RGB(255, 255, 255) : RGB(30, 30, 30));
    DrawTextW(hdc, s.c_str(), (int)s.size(), &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

void drawSidePanel(HDC hdc) {
    int panelX = BOARD_PADDING * 2 + BOARD_SIZE + 10;

    HBRUSH hPanelBrush = CreateSolidBrush(RGB(250, 250, 250));
    RECT panelRect = {panelX, BOARD_PADDING, panelX + SIDE_PANEL_WIDTH - 20, BOARD_PADDING + BOARD_SIZE};
    FillRect(hdc, &panelRect, hPanelBrush);
    DeleteObject(hPanelBrush);
}

void pixelToCell(int px, int py, int &cx, int &cy) {
    cx = (px - BOARD_PADDING) / CELL;
    cy = (py - BOARD_PADDING) / CELL;
}

void updateStatus() {
    if (hStatus) {
        std::wstring s;
        if (game.gameOver) {
            s = game.gameResult;
        } else {
            s = game.whiteTurn ? L"Turn: White" : L"Turn: Black";
            if (isInCheck(game.whiteTurn)) {
                s += L"  ⚠ CHECK!";
            }
        }
        SetWindowTextW(hStatus, s.c_str());
    }

    if (hWhiteCapturesLabel) {
        std::wstring wCaptures = L"White Captures: " + std::to_wstring(game.whiteCaptures);
        SetWindowTextW(hWhiteCapturesLabel, wCaptures.c_str());
    }

    if (hBlackCapturesLabel) {
        std::wstring bCaptures = L"Black Captures: " + std::to_wstring(game.blackCaptures);
        SetWindowTextW(hBlackCapturesLabel, bCaptures.c_str());
    }
}

void updateMoveList() {
    if (!hMoveList) return;
    SendMessage(hMoveList, LB_RESETCONTENT, 0, 0);
    for (size_t i = 0; i < game.moveHistory.size(); ++i) {
        std::wstring moveText = std::to_wstring(i + 1) + L". " + game.moveHistory[i];
        SendMessage(hMoveList, LB_ADDSTRING, 0, (LPARAM)moveText.c_str());
    }
    if (!game.moveHistory.empty()) {
        SendMessage(hMoveList, LB_SETCURSEL, game.moveHistory.size() - 1, 0);
    }
}

void newGame() {
    int result = MessageBoxW(hMainWnd, L"Start a new game? Current game will be lost.",
                            L"New Game", MB_YESNO | MB_ICONQUESTION);
    if (result == IDYES) {
        initBoard();
        updateStatus();
        updateMoveList();
        InvalidateRect(hMainWnd, NULL, TRUE);
    }
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE: {
            hMainWnd = hwnd;
            initBoard();

            int panelX = BOARD_PADDING * 2 + BOARD_SIZE + 10;

            hStatus = CreateWindowW(L"STATIC", L"Turn: White",
                                    WS_CHILD | WS_VISIBLE | SS_CENTER | SS_CENTERIMAGE,
                                    BOARD_PADDING, BOARD_PADDING + 8 * CELL + 30,
                                    BOARD_SIZE, 35,
                                    hwnd, NULL, NULL, NULL);
            hFontStatus = CreateFontW(20, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                      DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                      ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
            SendMessage(hStatus, WM_SETFONT, (WPARAM)hFontStatus, TRUE);

            HFONT hLabelFont = CreateFontW(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                          DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                          ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

            CreateWindowW(L"STATIC", L"Move History:",
                         WS_CHILD | WS_VISIBLE | SS_LEFT,
                         panelX, BOARD_PADDING, SIDE_PANEL_WIDTH - 20, 25,
                         hwnd, NULL, NULL, NULL);

            hMoveList = CreateWindowW(L"LISTBOX", NULL,
                                      WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | LBS_NOTIFY,
                                      panelX, BOARD_PADDING + 30, SIDE_PANEL_WIDTH - 20, 350,
                                      hwnd, (HMENU)101, NULL, NULL);
            hFontMoves = CreateFontW(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                     DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                     ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Consolas");
            SendMessage(hMoveList, WM_SETFONT, (WPARAM)hFontMoves, TRUE);

            hWhiteCapturesLabel = CreateWindowW(L"STATIC", L"White Captures: 0",
                                                WS_CHILD | WS_VISIBLE | SS_LEFT,
                                                panelX, BOARD_PADDING + 390, SIDE_PANEL_WIDTH - 20, 25,
                                                hwnd, NULL, NULL, NULL);
            SendMessage(hWhiteCapturesLabel, WM_SETFONT, (WPARAM)hLabelFont, TRUE);

            hBlackCapturesLabel = CreateWindowW(L"STATIC", L"Black Captures: 0",
                                                WS_CHILD | WS_VISIBLE | SS_LEFT,
                                                panelX, BOARD_PADDING + 420, SIDE_PANEL_WIDTH - 20, 25,
                                                hwnd, NULL, NULL, NULL);
            SendMessage(hBlackCapturesLabel, WM_SETFONT, (WPARAM)hLabelFont, TRUE);

            hNewGameBtn = CreateWindowW(L"BUTTON", L"New Game",
                                        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                        panelX, BOARD_PADDING + 460, 130, 35,
                                        hwnd, (HMENU)102, NULL, NULL);
            SendMessage(hNewGameBtn, WM_SETFONT, (WPARAM)hLabelFont, TRUE);

            hUndoBtn = CreateWindowW(L"BUTTON", L"Undo",
                                     WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                     panelX + 140, BOARD_PADDING + 460, 130, 35,
                                     hwnd, (HMENU)103, NULL, NULL);
            SendMessage(hUndoBtn, WM_SETFONT, (WPARAM)hLabelFont, TRUE);

            updateStatus();
            return 0;
        }

        case WM_COMMAND: {
            if (LOWORD(wParam) == 102) {
                newGame();
            } else if (LOWORD(wParam) == 103) {
                undoMove();
            }
            return 0;
        }

        case WM_LBUTTONDOWN: {
            if (game.gameOver) return 0;

            int mx = LOWORD(lParam);
            int my = HIWORD(lParam);
            int cx, cy;
            pixelToCell(mx, my, cx, cy);

            if (!isInside(cx, cy)) return 0;

            if (game.selX == -1) {
                char p = game.board[cy][cx];
                if (p == '.') return 0;
                if ((game.whiteTurn && !isWhitePiece(p)) || (!game.whiteTurn && !isBlackPiece(p))) {
                    return 0;
                }
                game.selX = cx;
                game.selY = cy;
                computeLegalMoves(cx, cy);
                InvalidateRect(hwnd, NULL, TRUE);
            } else {
                if (game.legalMoves[cy][cx]) {
                    makeMove(game.selX, game.selY, cx, cy);
                    game.whiteTurn = !game.whiteTurn;
                    game.selX = game.selY = -1;
                    clearLegalMoves();
                    checkGameEnd();
                    updateStatus();
                    updateMoveList();
                    InvalidateRect(hwnd, NULL, TRUE);
                } else {
                    char p = game.board[cy][cx];
                    if (p != '.' && ((game.whiteTurn && isWhitePiece(p)) || (!game.whiteTurn && isBlackPiece(p)))) {
                        game.selX = cx;
                        game.selY = cy;
                        computeLegalMoves(cx, cy);
                        InvalidateRect(hwnd, NULL, TRUE);
                    } else {
                        game.selX = game.selY = -1;
                        clearLegalMoves();
                        InvalidateRect(hwnd, NULL, TRUE);
                    }
                }
            }
            return 0;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);

            RECT clientRect;
            GetClientRect(hwnd, &clientRect);
            HBRUSH hBgBrush = CreateSolidBrush(RGB(245, 245, 245));
            FillRect(hdc, &clientRect, hBgBrush);
            DeleteObject(hBgBrush);

            drawBoard(hdc);
            drawSidePanel(hdc);

            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_DESTROY: {
            if (hFontStatus) DeleteObject(hFontStatus);
            if (hFontPiece) DeleteObject(hFontPiece);
            if (hFontMoves) DeleteObject(hFontMoves);
            if (hFontLabel) DeleteObject(hFontLabel);
            PostQuitMessage(0);
            return 0;
        }
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    const wchar_t CLASS_NAME[] = L"ChessGameWindow";

    WNDCLASSW wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

    RegisterClassW(&wc);

    HWND hwnd = CreateWindowExW(
        0,
        CLASS_NAME,
        L"Chess Game",
        WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT,
        WINDOW_WIDTH, WINDOW_HEIGHT,
        NULL, NULL, hInstance, NULL
    );

    if (hwnd == NULL) {
        return 0;
    }

    // Optional: Dark mode support (requires Windows 10 build 18985+)
    // BOOL value = TRUE;
    // DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &value, sizeof(value));

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}
