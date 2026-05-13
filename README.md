# 💰 Personal Financial Management Application (PFMA)

A Windows desktop finance tracker built in C++ with Win32 API and GDI+, developed as a data structures course project. Every core feature is powered by a specific data structure from lectures.

> ⚠️ **Windows only** — uses Win32 API and GDI+. Will not compile on Linux or macOS without significant changes.

---

## 📸 Features

- **First-launch setup** — prompts for your name and monthly income before anything else; hidden after setup completes
- Track **income and expenses** with category, date, and notes
- **Budget progress bar** — red fill bar showing `expenses / monthly income × 100%`, always live
- **Pie chart** and **bar chart** analytics by category
- **Search** transactions by category (DFS) or date (BFS)
- **Undo / Redo** support
- **Dark / Light mode** toggle
- **Monthly auto-reset** — on startup, if a new month is detected, data is automatically exported to `expenses_MM_YYYY.txt` and the app resets
- **Monthly export report** — includes all transactions + a Most Spent Category analysis powered by BFS on the DateTree
- Persistent save/load via human-readable `.txt` file

---

## 🏗️ Data Structures Used

| Data Structure | Where Used | Lecture Reference |
|---|---|---|
| **Singly Linked List** | Stores all transactions; tail insertion, full traversal for totals | Lec 1 |
| **Stack (LIFO) × 2** | `undoStack` and `redoStack` for undo/redo | Lec 2 |
| **Binary Search Tree (BST)** | `CategoryTree` — stores category totals, ordered alphabetically | Lec 4 |
| **DFS Inorder Traversal** | Traverses `CategoryTree` Left→Root→Right for sorted category list (pie chart, bar chart, category search) | Lec 4 |
| **3-Level Tree** | `DateTree` — Root = Month, Level 1 = Days, Level 2 = Transactions | Lec 4 |
| **BFS with Queue (FIFO)** | Traverses `DateTree` level-by-level to search transactions by date | Lec 4 |

### How the data structures interact

```
addTransaction()
    │
    ├─► TransactionList::add()         → Linked List (tail insertion, O(n))
    ├─► undoStack.push()               → Stack LIFO push, O(1)
    ├─► redoStack.clear()              → New action clears redo history
    ├─► CategoryTree::insert()         → BST insert by category name, O(log n) avg
    └─► DateTree::buildFromLinkedList()→ Rebuilds 3-level date tree from linked list

searchByCategory()  → DFS inorder on CategoryTree  → alphabetical results
searchByDate()      → BFS with queue on DateTree    → level-order results
undo()              → undoStack.pop() → redoStack.push() → removes last node
redo()              → redoStack.pop() → re-calls addTransaction()
```

---

## 🗂️ Project Structure

```
PFMA/
├── main.cpp          # All source code (single-file project)
├── finance.txt       # Auto-generated save file (created on first run)
├── README.md
└── expenses_MM_YYYY.txt  # Monthly export reports (auto-generated)
```

---

## 🔧 Build Instructions

### Requirements

- Windows 10 or 11
- Visual Studio 2019 or 2022 (Community edition is free)
- No external libraries needed — uses Win32 API and GDI+ (both ship with Windows SDK)

### Steps

1. Open Visual Studio
2. Create a new **Empty C++ Project** (Windows Desktop Application)
3. Add `main.cpp` to the project
4. Make sure the subsystem is set to **Windows** (not Console):
   - Right-click project → Properties → Linker → System → SubSystem → `Windows (/SUBSYSTEM:WINDOWS)`
5. Build and run (`Ctrl+F5`)

### Compiler flags (if using g++ / MinGW)

```bash
g++ main.cpp -o PFMA.exe -lgdiplus -lcomctl32 -lgdi32 -luser32 -mwindows
```

---

## 📋 Save File Format

Data is saved to `finance.txt` in plain text — easy to read and debug:

```
NAME:Alice
INCOME:3000
DARKMODE:0
RESETMONTH:5
RESETYEAR:2024
COUNT:2
---TRANSACTION---
CAT:Food
AMT:150.5
DAY:15 MON:5 YEAR:2024
NOTE:grocery run
TYPE:1
---TRANSACTION---
CAT:Salary
AMT:500
DAY:1 MON:5 YEAR:2024
NOTE:freelance payment
TYPE:0
```

---

## 📊 Budget Progress Bar

- Displayed on the main window below the balance card
- Calculated as: `(total expenses / monthly income) × 100`
- Rendered as a **red gradient fill bar** that grows as you spend
- Capped at 100% visually even if you go over budget
- Recalculated on every repaint by traversing the full linked list — always live

---

## 🔄 Monthly Auto-Reset

On every app launch, the app checks the current system date against `lastResetMonth` / `lastResetYear` stored in `finance.txt`. If a new month is detected:

1. Calls `exportMonthlyExpenses()` — writes a full report to `expenses_MM_YYYY.txt`
2. Clears the linked list, CategoryBST, and DateTree
3. Resets `monthlyIncome` to 0
4. Updates the stored reset month/year and saves

This happens automatically — no user action needed.

---

## 📁 Monthly Export Report

When exported (manually via the Export button, or automatically on month change), the report file `expenses_MM_YYYY.txt` contains:

- User name, monthly income, total expenses, balance
- Full transaction list from the linked list
- **Most Spent Category Analysis** — powered by BFS on the DateTree:
  - Loops through every day 1–31 of the month
  - For each day, runs `searchByDateBFS()` (queue-based level-order traversal)
  - Collects all expense transactions found at Level 2 leaf nodes
  - Accumulates totals per category, then finds the highest

---

## 🔍 Search Feature Details

### Search by Category — DFS on BST
- Runs a **DFS inorder traversal** (Left → Root → Right) on the `CategoryTree`
- At each visited node, checks if the category name contains the search term (case-insensitive partial match)
- Results are returned **alphabetically sorted** — a free property of BST inorder traversal
- Then also walks the linked list to show individual matching transactions

### Search by Date — BFS on DateTree
- The `DateTree` is a 3-level tree: **Root = Month → Level 1 = Days → Level 2 = Transactions**
- BFS uses a **FIFO queue** to traverse level by level:
  - Level 0: find the matching month node
  - Level 1: find the matching day node within that month
  - Level 2: collect all transaction leaf nodes on that day
- Returns only transactions that match the exact `day/month/year`

---

## ↩️ Undo / Redo Details

- Every `addTransaction()` call **pushes** a snapshot onto `undoStack` (LIFO)
- `undo()` **pops** the top of `undoStack`, moves it to `redoStack`, removes the last linked list node
- `redo()` **pops** the top of `redoStack` and re-adds the transaction
- Adding a **new transaction clears** `redoStack` (same behavior as Word / Excel)

---

## 📄 License

This project was built for educational purposes as part of a Data Structures course.
