# ♟️ Chess Game (C++ / Win32 API)

A complete chess game built entirely in **C++** using the **Win32 API**, featuring a fully native Windows GUI, valid move detection, visual highlights, captures tracking, undo functionality, and check/checkmate detection.

---

## 🚀 Features

### 🖥️ User Interface
- Win32 native graphical interface (no external libraries)
- Highlighted last move
- Highlighted selected square
- Display of legal moves (green dots + red capture rings)
- Chessboard coordinates (a–h, 1–8)
- Side panel including:
  - Move history
  - Capture counters
  - **New Game** button
  - **Undo** button

---

## ♟️ Chess Logic
- Full piece movement rules
- Check detection
- Checkmate detection
- Stalemate detection
- Piece capturing
- Pawn auto-promotion to Queen
- Undo / rollback system (board, captures, moves, highlights)

---

## 🧱 Project Structure

Core components:
- **GameState**: full game-state container  
- **Move stack** with undo support  
- Logic functions:
  - `isLegalMove()`
  - `isInCheck()`
  - `hasLegalMoves()`
  - `isSquareAttacked()`
  - `clearPath()`
- UI functions:
  - `drawBoard()`
  - `drawPiece()`
  - `drawSidePanel()`
  - `drawCoordinates()`

---


