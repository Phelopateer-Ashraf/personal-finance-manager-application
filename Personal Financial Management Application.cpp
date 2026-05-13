#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <gdiplus.h>
#include <string>
#include <vector>
#include <fstream>
#include <algorithm>
#include <ctime>
#include <map>
#include <queue>   // FIFO data structure - used for BFS traversal in DateTree
#include <stack>   // LIFO data structure - used implicitly by DFS recursion in CategoryTree
#include <sstream>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "gdiplus.lib")

using namespace std;
using namespace Gdiplus;

// ========================================================================
// COLOR SCHEMES - Light and Dark Mode Support
// ========================================================================

constexpr COLORREF LIGHT_BG = RGB(240, 240, 250);
constexpr COLORREF LIGHT_CARD = RGB(255, 255, 255);
constexpr COLORREF LIGHT_TEXT = RGB(0, 0, 0);
constexpr COLORREF LIGHT_BORDER = RGB(220, 220, 220);
constexpr COLORREF LIGHT_HEADER_START = RGB(255, 0, 0);
constexpr COLORREF LIGHT_HEADER_END = RGB(180, 0, 0);
constexpr COLORREF LIGHT_PROGRESS_BG = RGB(230, 230, 230);

constexpr COLORREF DARK_BG = RGB(32, 32, 32);
constexpr COLORREF DARK_CARD = RGB(45, 45, 45);
constexpr COLORREF DARK_TEXT = RGB(255, 255, 255);
constexpr COLORREF DARK_BORDER = RGB(60, 60, 60);
constexpr COLORREF DARK_HEADER_START = RGB(255, 0, 0);
constexpr COLORREF DARK_HEADER_END = RGB(180, 0, 0);
constexpr COLORREF DARK_PROGRESS_BG = RGB(70, 70, 70);

constexpr COLORREF RACING_RED = RGB(255, 0, 0);
constexpr COLORREF RACING_RED_DARK = RGB(180, 0, 0);

bool g_darkMode = false;

// ========================================================================
// DATA STRUCTURE 1: DATE
// Simple struct to hold day, month, year for each transaction
// ========================================================================
struct Date {
    int d, m, y;
};

// ========================================================================
// DATA STRUCTURE 2: TRANSACTION NODE  (Linked List Node)
// Based on Lec 1 - each node has data fields + a pointer to next node
// The "next" pointer is what makes this a linked list node
// ========================================================================
struct Transaction {
    string       category; // category name e.g. "Food"
    double       amount;   // how much money
    Date         date;     // when it happened
    string       note;     // optional description
    int          type;     // 1 = expense, 0 = income
    Transaction* next;     // pointer to next node - this is the linked list link

    Transaction(string c, double a, Date d, string n, int t)
        : category(c), amount(a), date(d), note(n), type(t), next(nullptr) {
    }
};

// ========================================================================
// DATA STRUCTURE 3: SINGLY LINKED LIST
// Based on Lec 1 - stores all transactions as a chain of nodes
// Head points to first node. Last node points to nullptr (NULL)
// Operations:
//   add()   -> traverse to end, attach new node  O(n)
//   clear() -> delete every node one by one       O(n)
//   traverse-> start at head, follow next ptrs    O(n)
// ========================================================================
class TransactionList {
    Transaction* head; // pointer to first node in the chain
    int size;          // count of nodes (so getSize() is O(1) not O(n))

public:
    TransactionList() : head(nullptr), size(0) {}

    // Add a new transaction to the END of the linked list
    // Matches Lec 1 insertion diagram: walk to last node, point its next to new node
    void add(string cat, double amt, Date dt, string note, int type) {
        Transaction* newNode = new Transaction(cat, amt, dt, note, type); // allocate on heap
        if (!head) {
            head = newNode; // empty list - new node becomes the head
        }
        else {
            Transaction* temp = head;
            while (temp->next) temp = temp->next; // walk to last node (next == nullptr)
            temp->next = newNode;                 // attach new node at the tail
        }
        size++;
    }

    Transaction* getHead()  const { return head; }
    int          getSize()  const { return size; }

    // Delete every node and free heap memory
    // Matches Lec 1 - always free memory you allocated with new
    void clear() {
        while (head) {
            Transaction* temp = head;
            head = head->next; // move head forward
            delete temp;       // free this node's memory
        }
        size = 0;
    }

    ~TransactionList() { clear(); }
};

// ========================================================================
// DATA STRUCTURE 4: CATEGORY BST NODE
// Based on Lec 4 tree node: data + left pointer + right pointer
// The key for BST ordering is the category name (alphabetical)
// ========================================================================
struct CategoryNode {
    string       category;    // the key - BST orders by this alphabetically
    double       totalAmount; // total expenses in this category (aggregated)
    int          count;       // how many transactions are in this category
    CategoryNode* left;       // left child  - alphabetically SMALLER categories
    CategoryNode* right;      // right child - alphabetically LARGER categories

    CategoryNode(string cat)
        : category(cat), totalAmount(0), count(0), left(nullptr), right(nullptr) {
    }
};

// ========================================================================
// DATA STRUCTURE 5: CATEGORY BINARY SEARCH TREE (BST)
// Based on Lec 4 - BST property: left < root < right (alphabetical)
// Used for:
//   - Pie chart display (DFS inorder -> alphabetical list)
//   - Analytics bar chart (DFS inorder -> alphabetical list)
//   - SEARCH BY CATEGORY (DFS inorder search -> finds matching nodes)
//
// DFS INORDER (Left -> Root -> Right):
//   Recursion acts as the stack (Lec 4 shows DFS uses a stack internally)
//   Result comes out in alphabetical order because of BST property
// ========================================================================
class CategoryTree {
    CategoryNode* root;

    // Recursive BST insert - maintains left < root < right property
    // Time: O(log n) average, O(n) worst case if tree is unbalanced
    CategoryNode* insert(CategoryNode* node, string cat, double amt) {
        if (!node) {
            // reached an empty spot - create the new node here
            CategoryNode* n = new CategoryNode(cat);
            n->totalAmount = amt;
            n->count = 1;
            return n;
        }
        if (cat < node->category)
            node->left = insert(node->left, cat, amt); // go left (smaller)
        else if (cat > node->category)
            node->right = insert(node->right, cat, amt); // go right (larger)
        else {
            // category already exists - just add to its total
            node->totalAmount += amt;
            node->count++;
        }
        return node;
    }

    // Post-order delete: children before parent (Lec 4 tree deletion pattern)
    void deleteTree(CategoryNode* node) {
        if (!node) return;
        deleteTree(node->left);
        deleteTree(node->right);
        delete node;
    }

    // ====================================================================
    // DFS INORDER TRAVERSAL - Left -> Root -> Right
    // Based on Lec 4 DFS diagram:
    //   1. Go all the way left first (push onto call stack)
    //   2. Visit current node (process it)
    //   3. Then go right
    // Because BST is ordered alphabetically, inorder gives SORTED output
    // This is used for: pie chart, analytics chart, AND category search
    // Time: O(n) - visits every node exactly once
    // Space: O(h) - h = height of tree, used by recursion call stack
    // ====================================================================
    void inorderDFS(CategoryNode* node, vector<pair<string, double>>& result) const {
        if (!node) return;                                        // base case - empty node
        inorderDFS(node->left, result);                         // LEFT  - go deeper left first
        result.push_back({ node->category, node->totalAmount });   // ROOT  - process this node
        inorderDFS(node->right, result);                         // RIGHT - then go right
    }

    // ====================================================================
    // DFS SEARCH - Search by Category Name using DFS Inorder Traversal
    // NEW IMPLEMENTATION: replaces the old linked list search
    //
    // How it works:
    //   We run full inorder DFS on the BST (Left->Root->Right)
    //   At each visited node, we check if category name matches search string
    //   Case-insensitive partial match (e.g. "fo" matches "Food")
    //   Results come out SORTED alphabetically - free bonus of BST+inorder
    //
    // This is a REAL use of DFS on the BST for search - not a linked list walk
    // Time: O(n) - must visit all nodes since it's a partial string match
    // ====================================================================
    void inorderDFSSearch(CategoryNode* node,
        const string& searchLower,
        vector<pair<string, double>>& results) const {
        if (!node) return;

        inorderDFSSearch(node->left, searchLower, results); // DFS - go left first

        // Check if this node's category matches the search term
        string nodeLower = node->category;
        transform(nodeLower.begin(), nodeLower.end(), nodeLower.begin(), ::tolower);
        if (nodeLower.find(searchLower) != string::npos) {
            results.push_back({ node->category, node->totalAmount }); // match found
        }

        inorderDFSSearch(node->right, searchLower, results); // DFS - then go right
    }

public:
    CategoryTree() : root(nullptr) {}

    void insert(string cat, double amt) { root = insert(root, cat, amt); }
    void clear() { deleteTree(root); root = nullptr; }

    // Get all categories in alphabetical order via DFS inorder
    // Used by: pie chart, analytics bar chart
    vector<pair<string, double>> getDFSList() const {
        vector<pair<string, double>> result;
        inorderDFS(root, result);
        return result;
    }

    // ====================================================================
    // SEARCH BY CATEGORY using DFS on the BST
    // Returns all matching categories with their total amounts
    // Used by: Search dialog "Search by Category" option
    // Results are alphabetically sorted (DFS inorder on BST property)
    // ====================================================================
    vector<pair<string, double>> searchDFS(const string& searchTerm) const {
        vector<pair<string, double>> results;
        string searchLower = searchTerm;
        transform(searchLower.begin(), searchLower.end(), searchLower.begin(), ::tolower);
        inorderDFSSearch(root, searchLower, results);
        return results;
    }

    ~CategoryTree() { clear(); }
};

// ========================================================================
// DATA STRUCTURE 6: DATE TREE NODE
// This is the node for the NEW Date BST used for BFS date search
//
// TREE STRUCTURE (3 levels - designed for BFS level-order traversal):
//
//   Level 0 (Root)   : Month number (e.g. month = 5 for May)
//   Level 1 (Children of root) : Day numbers that have transactions
//   Level 2 (Leaf nodes)       : Individual transactions on that day
//
// Example tree for May 2024:
//
//              [Month: 5]          <- Level 0: root
//             /          \
//        [Day: 3]      [Day: 15]   <- Level 1: day nodes
//        /      \           \
//  [Food:50] [Rent:800]  [Transport:30] <- Level 2: transaction leaves
//
// nodeType:
//   0 = Month node  (root level)
//   1 = Day node    (middle level)
//   2 = Transaction node (leaf level - carries full transaction data)
// ========================================================================
struct DateNode {
    int    nodeType;   // 0=month, 1=day, 2=transaction
    int    value;      // month number (type 0) or day number (type 1) or 0 (type 2)

    // Transaction data - only used when nodeType == 2 (leaf node)
    string category;
    double amount;
    Date   date;
    string note;
    int    transType;  // 1=expense, 0=income

    // Child pointers - a node can have multiple children stored as a linked list of siblings
    // We use left=first child, right=next sibling pattern
    DateNode* firstChild;   // pointer to first child node
    DateNode* nextSibling;  // pointer to next sibling (same level, same parent)

    // Constructor for Month node (Level 0)
    DateNode(int monthNum)
        : nodeType(0), value(monthNum), amount(0), transType(0),
        firstChild(nullptr), nextSibling(nullptr) {
    }

    // Constructor for Day node (Level 1)
    DateNode(int dayNum, bool isDay)
        : nodeType(1), value(dayNum), amount(0), transType(0),
        firstChild(nullptr), nextSibling(nullptr) {
        (void)isDay;
    }

    // Constructor for Transaction leaf node (Level 2)
    DateNode(string cat, double amt, Date d, string n, int t)
        : nodeType(2), value(0), category(cat), amount(amt),
        date(d), note(n), transType(t),
        firstChild(nullptr), nextSibling(nullptr) {
    }
};

// ========================================================================
// DATA STRUCTURE 7: DATE TREE
// A 3-level tree organized as Month -> Days -> Transactions
// Built from the linked list every time a transaction is added
//
// BFS SEARCH (Breadth-First Search using QUEUE):
// Based exactly on Lec 4 BFS algorithm:
//   1. Enqueue the root (month node)
//   2. While queue is not empty:
//       a. Dequeue front node
//       b. Check if it matches what we are looking for
//       c. Enqueue all its children
//
// Level 0: We check if this is the right month
// Level 1: We check if this is the right day
// Level 2: We collect the transaction (this is the result)
//
// The Queue (FIFO) ensures we process level 0 before level 1, level 1 before level 2
// This is exactly "Level Order Traversal" from Lec 4
// ========================================================================
class DateTree {
    DateNode* root; // root = the month node

    // Find a day child node under the month root, or nullptr if not found
    DateNode* findDayChild(int dayNum) {
        DateNode* child = root ? root->firstChild : nullptr;
        while (child) {
            if (child->value == dayNum) return child; // found this day
            child = child->nextSibling;               // try next sibling
        }
        return nullptr;
    }

    // Delete the entire tree recursively
    void deleteTree(DateNode* node) {
        if (!node) return;
        deleteTree(node->firstChild);  // delete all children first
        deleteTree(node->nextSibling); // delete siblings
        delete node;
    }

public:
    DateTree() : root(nullptr) {}

    // ====================================================================
    // BUILD the date tree from the linked list
    // Called every time a transaction is added
    // Structure: Root=Month -> Children=Days -> Children=Transactions
    // ====================================================================
    void buildFromLinkedList(Transaction* head, int targetMonth, int targetYear) {
        // Clear existing tree
        deleteTree(root);
        root = nullptr;

        // Create the root node = the month number
        root = new DateNode(targetMonth);

        // Walk the linked list and insert each transaction into the tree
        Transaction* curr = head;
        while (curr) {
            // Only add transactions that belong to this month/year
            if (curr->date.m == targetMonth && curr->date.y == targetYear) {

                // Look for an existing Day node for this transaction's day
                DateNode* dayNode = findDayChild(curr->date.d);

                if (!dayNode) {
                    // Day not found - create a new Day node and add it as a child of root
                    dayNode = new DateNode(curr->date.d, true);
                    // Insert as first child of root (prepend to sibling list)
                    dayNode->nextSibling = root->firstChild;
                    root->firstChild = dayNode;
                }

                // Create a Transaction leaf node for this transaction
                DateNode* transNode = new DateNode(
                    curr->category, curr->amount, curr->date, curr->note, curr->type
                );

                // Add transaction as first child of its Day node
                transNode->nextSibling = dayNode->firstChild;
                dayNode->firstChild = transNode;
            }
            curr = curr->next; // move to next node in linked list
        }
    }

    // ====================================================================
    // BFS SEARCH by date - uses QUEUE for level-order traversal
    // Based on Lec 4 BFS algorithm exactly:
    //
    // Queue state during search for day=15, month=5:
    //   Start:   Queue = [Month:5]
    //   Step 1:  Dequeue Month:5. Is it month 5? Yes. Enqueue its day children.
    //            Queue = [Day:3, Day:15]
    //   Step 2:  Dequeue Day:3. Is it day 15? No. Enqueue its children anyway? No - skip.
    //            Queue = [Day:15]
    //   Step 3:  Dequeue Day:15. Is it day 15? Yes! Enqueue its transaction children.
    //            Queue = [Trans:Food, Trans:Transport]
    //   Step 4:  Dequeue Trans:Food. It's a leaf - collect it as result.
    //   Step 5:  Dequeue Trans:Transport. Collect it.
    //   Queue empty - done.
    //
    // Returns vector of matching transactions found at Level 2
    // ====================================================================
    vector<Transaction> searchByDateBFS(int searchDay, int searchMonth, int searchYear) {
        vector<Transaction> results;
        if (!root) return results;

        // QUEUE data structure for BFS - FIFO (First In First Out)
        // This is exactly what Lec 4 shows for level-order traversal
        queue<DateNode*> q;
        q.push(root); // enqueue root (Level 0 = month node) to start BFS

        while (!q.empty()) {
            DateNode* current = q.front(); // get the front node (FIFO principle)
            q.pop();                       // remove it from the queue

            if (current->nodeType == 0) {
                // Level 0: Month node
                // Check if this is the month we are looking for
                if (current->value == searchMonth) {
                    // Correct month - enqueue all day children (Level 1)
                    DateNode* child = current->firstChild;
                    while (child) {
                        q.push(child);           // enqueue each day child
                        child = child->nextSibling;
                    }
                }
                // If wrong month, we do not enqueue children - prune this branch
            }
            else if (current->nodeType == 1) {
                // Level 1: Day node
                // Check if this is the day we are looking for
                if (current->value == searchDay) {
                    // Correct day - enqueue all transaction children (Level 2)
                    DateNode* child = current->firstChild;
                    while (child) {
                        q.push(child);
                        child = child->nextSibling;
                    }
                }
                // If wrong day, do not enqueue its children
            }
            else if (current->nodeType == 2) {
                // Level 2: Transaction leaf node
                // We reached a transaction - collect it as a result
                // Check year here since tree only stores month/day levels
                if (current->date.y == searchYear) {
                    Transaction t(current->category, current->amount,
                        current->date, current->note,
                        current->transType);
                    results.push_back(t);
                }
            }
        } // end while - queue is empty, BFS complete

        return results;
    }

    void clear() { deleteTree(root); root = nullptr; }

    ~DateTree() { clear(); }
};

// ========================================================================
// DATA STRUCTURE 8: UNDO RECORD
// A plain struct that stores a snapshot of one transaction's data
// This is what gets pushed onto the Undo and Redo stacks
// We store all 5 fields so we can fully recreate or remove a transaction
// ========================================================================
struct UndoRecord {
    string category;
    double amount;
    Date   date;
    string note;
    int    type; // 1=expense, 0=income
};

// ========================================================================
// MAIN FINANCE MANAGER CLASS
// Integrates all data structures:
//   - TransactionList  : linked list of all transactions
//   - CategoryTree     : BST for category totals (DFS for charts + search)
//   - DateTree         : 3-level tree for date-based BFS search
//   - undoStack        : LIFO stack (Lec 2) - stores last added transactions
//   - redoStack        : LIFO stack (Lec 2) - stores undone transactions
// ========================================================================
class FinanceManager {
public:
    string          userName;
    double          monthlyIncome;
    TransactionList transactions; // DATA STRUCTURE: Singly Linked List
    CategoryTree    categoryTree; // DATA STRUCTURE: BST (DFS search + charts)
    DateTree        dateTree;     // DATA STRUCTURE: 3-level tree (BFS date search)
    bool            darkModePreference;
    int             lastResetMonth;
    int             lastResetYear;

    // ====================================================================
    // DATA STRUCTURE: TWO STACKS for Undo and Redo
    // Based on Lec 2 - Stack is LIFO (Last In First Out)
    //
    // undoStack: every time a transaction is added, push its data here
    //   - LIFO means the LAST added transaction is always on top
    //   - pop() removes the top = removes the most recent transaction
    //
    // redoStack: when undo is called, the removed record goes here
    //   - so redo can pop it back and re-add the transaction
    //   - redoStack is cleared whenever a NEW transaction is added
    //     (you can't redo after a new action - same as Word/Excel behavior)
    //
    // Both use STL stack<UndoRecord> from <stack> library (already included)
    // ====================================================================
    stack<UndoRecord> undoStack; // LIFO - top = most recently added transaction
    stack<UndoRecord> redoStack; // LIFO - top = most recently undone transaction

    FinanceManager() : monthlyIncome(0), userName(""), darkModePreference(false) {
        time_t now = time(0);
        tm* ltm = localtime(&now);
        lastResetMonth = 1 + ltm->tm_mon;
        lastResetYear = 1900 + ltm->tm_year;
    }

    // ====================================================================
    // ADD TRANSACTION
    // 1. Add node to end of linked list           -> TransactionList::add()
    // 2. Push snapshot to undoStack               -> LIFO Stack push O(1)
    // 3. Clear redoStack                          -> new action kills redo history
    // 4. Rebuild CategoryBST from linked list     -> for DFS chart/search
    // 5. Rebuild DateTree from linked list        -> for BFS date search
    // All three structures stay in sync after every add
    // ====================================================================
    void addTransaction(string cat, double amt, Date dt, string note, int type) {
        // Step 1: Add to linked list (Lec 1 - tail insertion)
        transactions.add(cat, amt, dt, note, type);

        // Step 2: Push this transaction's data onto the undoStack (Lec 2 - LIFO)
        // Stack push is O(1) - just places record on top
        UndoRecord rec;
        rec.category = cat;
        rec.amount = amt;
        rec.date = dt;
        rec.note = note;
        rec.type = type;
        undoStack.push(rec); // PUSH onto LIFO stack - top is now this transaction

        // Step 3: Clear redoStack - adding a new transaction invalidates
        // any previously undone actions (same behavior as all undo/redo systems)
        while (!redoStack.empty()) redoStack.pop();

        // Step 4: Rebuild Category BST for expenses only
        // (Lec 4 BST - category is the key, alphabetical ordering)
        if (type == 1) {
            categoryTree.clear();
            Transaction* curr = transactions.getHead();
            while (curr) {
                if (curr->type == 1)
                    categoryTree.insert(curr->category, curr->amount);
                curr = curr->next; // linked list traversal
            }
        }

        // Step 5: Rebuild Date Tree for current month
        // (3-level tree: Month -> Days -> Transactions, used for BFS search)
        dateTree.buildFromLinkedList(
            transactions.getHead(), lastResetMonth, lastResetYear
        );

        save("finance.txt"); // auto-save after every transaction
    }

    // ====================================================================
    // UNDO TRANSACTION - uses the undoStack (LIFO from Lec 2)
    //
    // How it works:
    //   1. Check undoStack is not empty (stack underflow check - Lec 2)
    //   2. pop() the top record - this is the LAST added transaction (LIFO)
    //   3. Push that record onto redoStack so redo can bring it back
    //   4. Remove the last node from the linked list
    //   5. Rebuild CategoryBST and DateTree from the updated linked list
    //   6. Save
    //
    // Stack pop is O(1) - just removes top element
    // Removing last linked list node is O(n) - must walk to find it
    // ====================================================================
    bool undoTransaction() {
        // Stack underflow check (Lec 2 - always check before pop)
        if (undoStack.empty()) return false;

        // Pop the top record from undoStack (LIFO - most recent transaction)
        UndoRecord rec = undoStack.top(); // peek at top
        undoStack.pop();                  // remove from undoStack

        // Push this record onto redoStack so the user can redo it
        redoStack.push(rec);

        // Remove the LAST node from the linked list
        // We must walk the list to find it (O(n) - Lec 1 traversal)
        Transaction* head = transactions.getHead();
        if (!head) return false;

        if (!head->next) {
            // Only one node in the list - clear it entirely
            transactions.clear();
        }
        else {
            // Walk to the second-to-last node
            Transaction* prev = head;
            while (prev->next && prev->next->next) {
                prev = prev->next;
            }
            // prev->next is now the last node - delete it
            delete prev->next;
            prev->next = nullptr;
            // We bypassed the size counter, so rebuild structures directly
        }

        // Rebuild CategoryBST from the updated linked list
        categoryTree.clear();
        Transaction* curr = transactions.getHead();
        while (curr) {
            if (curr->type == 1)
                categoryTree.insert(curr->category, curr->amount);
            curr = curr->next;
        }

        // Rebuild DateTree from the updated linked list
        dateTree.buildFromLinkedList(
            transactions.getHead(), lastResetMonth, lastResetYear
        );

        save("finance.txt");
        return true;
    }

    // ====================================================================
    // REDO TRANSACTION - uses the redoStack (LIFO from Lec 2)
    //
    // How it works:
    //   1. Check redoStack is not empty
    //   2. pop() the top record - this is the LAST undone transaction
    //   3. Call addTransaction() with that data - which also pushes to undoStack
    //      and clears redoStack... BUT we re-push redoStack contents first
    //
    // Simple approach: pop from redoStack, save remaining redo records,
    // call addTransaction (which clears redo), then restore remaining redo records
    // ====================================================================
    bool redoTransaction() {
        // Stack underflow check
        if (redoStack.empty()) return false;

        // Save all remaining redo records (except top which we are redoing)
        UndoRecord rec = redoStack.top();
        redoStack.pop();

        // Save the remaining redo stack contents before addTransaction clears it
        vector<UndoRecord> remaining;
        while (!redoStack.empty()) {
            remaining.push_back(redoStack.top());
            redoStack.pop();
        }

        // Re-add the transaction (this also pushes to undoStack and clears redoStack)
        addTransaction(rec.category, rec.amount, rec.date, rec.note, rec.type);

        // Restore the remaining redo records (in correct order)
        for (int i = (int)remaining.size() - 1; i >= 0; i--) {
            redoStack.push(remaining[i]);
        }

        return true;
    }

    // ====================================================================
    // LINKED LIST TRAVERSAL - Total Expenses
    // Walks entire linked list, sums nodes where type == 1 (expense)
    // Called on every repaint - this is why balance updates dynamically
    // Time: O(n)
    // ====================================================================
    double getTotalExpenses() const {
        double total = 0;
        Transaction* curr = transactions.getHead();
        while (curr) {
            if (curr->type == 1) total += curr->amount;
            curr = curr->next;
        }
        return total;
    }

    // ====================================================================
    // LINKED LIST TRAVERSAL - Total Income
    // Same pattern - sums nodes where type == 0 (income)
    // Also adds the base monthly income set by user
    // Time: O(n)
    // ====================================================================
    double getTotalIncome() const {
        double total = monthlyIncome; // base monthly income (user-set)
        Transaction* curr = transactions.getHead();
        while (curr) {
            if (curr->type == 0) total += curr->amount;
            curr = curr->next;
        }
        return total;
    }

    // Balance is always recalculated from the two traversals above
    double getBalance() const { return getTotalIncome() - getTotalExpenses(); }

    // Monthly auto-reset: backup data, clear both trees and linked list
    bool checkAndResetMonthly() {
        time_t now = time(0);
        tm* ltm = localtime(&now);
        int currentMonth = 1 + ltm->tm_mon;
        int currentYear = 1900 + ltm->tm_year;

        if (currentYear > lastResetYear ||
            (currentYear == lastResetYear && currentMonth > lastResetMonth)) {
            exportMonthlyExpenses();
            monthlyIncome = 0;
            lastResetMonth = currentMonth;
            lastResetYear = currentYear;
            transactions.clear(); // clear linked list
            categoryTree.clear(); // clear category BST
            dateTree.clear();     // clear date tree
            save("finance.txt");
            return true;
        }
        return false;
    }

    // Export monthly data as readable text file
    void exportMonthlyExpenses() {
        char filename[100];
        sprintf(filename, "expenses_%02d_%d.txt", lastResetMonth, lastResetYear);
        ofstream file(filename);
        if (!file) return;

        file << "========================================\n";
        file << "MONTHLY EXPENSE REPORT - "
            << lastResetMonth << "/" << lastResetYear << "\n";
        file << "========================================\n";
        file << "User: " << userName << "\n";
        file << "Monthly Income: $" << monthlyIncome << "\n";
        file << "Total Expenses: $" << getTotalExpenses() << "\n";
        file << "Balance: $" << getBalance() << "\n\n";
        file << "TRANSACTIONS:\n";
        file << "----------------------------------------\n";

        // Linked list traversal to write each transaction
        Transaction* curr = transactions.getHead();
        while (curr) {
            file << (curr->type ? "EXPENSE" : "INCOME") << " | "
                << curr->category << " | $"
                << curr->amount << " | "
                << curr->date.d << "/"
                << curr->date.m << "/"
                << curr->date.y << " | "
                << curr->note << "\n";
            curr = curr->next;
        }
        // ====================================================================
        // MOST SPENT CATEGORY - Found using DateTree BFS
        //
        // How it works:
        //   We run BFS on the DateTree for every day 1-31 of this month
        //   BFS returns all transactions on each day (Level 2 leaves)
        //   We collect only expense transactions and sum them per category
        //   Then we walk the sums to find which category has the highest total
        //
        // This reuses searchByDateBFS() - the same BFS queue algorithm
        // from Lec 4 that powers the Search by Date feature
        // ====================================================================
        file << "\n----------------------------------------\n";
        file << "MOST SPENT CATEGORY ANALYSIS (via DateTree BFS):\n";
        file << "----------------------------------------\n";

        // Use a map to accumulate category totals from BFS results
        // We scan every possible day (1-31) using BFS on the DateTree
        map<string, double> categoryTotals;

        for (int day = 1; day <= 31; day++) {
            // BFS call: searches Level0(Month)->Level1(Day)->Level2(Transactions)
            vector<Transaction> dayResults = dateTree.searchByDateBFS(
                day, lastResetMonth, lastResetYear
            );

            // Collect expense transactions found by BFS on this day
            for (auto& t : dayResults) {
                if (t.type == 1) { // only expenses
                    categoryTotals[t.category] += t.amount;
                }
            }
        }

        if (categoryTotals.empty()) {
            file << "No expense data found.\n";
        }
        else {
            // Find the category with the highest total
            // Walk the map entries (each is a category + its BFS-collected total)
            string topCategory = "";
            double topAmount = 0.0;
            for (auto& entry : categoryTotals) {
                if (entry.second > topAmount) {
                    topAmount = entry.second;
                    topCategory = entry.first;
                }
            }

            // Write all category totals (from BFS data)
            for (auto& entry : categoryTotals) {
                file << "  " << entry.first << ": $" << entry.second << "\n";
            }

            file << "\n>>> Most Spent Category: "
                << topCategory << " ($" << topAmount << ") <<<\n";
        }

        file << "\n========================================\n";
        file << "End of Report\n";
        file.close();
    }

    // ====================================================================
    // SAVE TO TEXT FILE (.txt) - Human readable, NOT binary
    // Each field is written as a labeled line so the file is easy to read
    // Format:
    //   NAME:Alice
    //   INCOME:3000
    //   DARKMODE:0
    //   RESETMONTH:5
    //   RESETYEAR:2024
    //   COUNT:3
    //   ---TRANSACTION---
    //   CAT:Food
    //   AMT:150.5
    //   DAY:15  MON:5  YEAR:2024
    //   NOTE:grocery
    //   TYPE:1
    //   ---TRANSACTION---
    //   ...
    // ====================================================================
    void save(const char* filename) {
        ofstream file(filename);
        if (!file) return;

        // Write user info
        file << "NAME:" << userName << "\n";
        file << "INCOME:" << monthlyIncome << "\n";
        file << "DARKMODE:" << darkModePreference << "\n";
        file << "RESETMONTH:" << lastResetMonth << "\n";
        file << "RESETYEAR:" << lastResetYear << "\n";
        file << "COUNT:" << transactions.getSize() << "\n";

        // Walk linked list and write each transaction
        Transaction* curr = transactions.getHead();
        while (curr) {
            file << "---TRANSACTION---\n";
            file << "CAT:" << curr->category << "\n";
            file << "AMT:" << curr->amount << "\n";
            file << "DAY:" << curr->date.d
                << " MON:" << curr->date.m
                << " YEAR:" << curr->date.y << "\n";
            file << "NOTE:" << curr->note << "\n";
            file << "TYPE:" << curr->type << "\n";
            curr = curr->next;
        }
        file.close();
    }

    // ====================================================================
    // LOAD FROM TEXT FILE (.txt)
    // Reads the labeled lines written by save()
    // Rebuilds the linked list, then CategoryBST and DateTree are rebuilt
    // automatically by addTransaction() for each loaded record
    // ====================================================================
    void load(const char* filename) {
        ifstream file(filename);
        if (!file) return;

        string line;
        int count = 0;

        // Read header fields
        while (getline(file, line)) {
            if (line.substr(0, 5) == "NAME:")
                userName = line.substr(5);
            else if (line.substr(0, 7) == "INCOME:")
                monthlyIncome = stod(line.substr(7));
            else if (line.substr(0, 9) == "DARKMODE:")
                darkModePreference = (line.substr(9) == "1");
            else if (line.substr(0, 11) == "RESETMONTH:")
                lastResetMonth = stoi(line.substr(11));
            else if (line.substr(0, 10) == "RESETYEAR:")
                lastResetYear = stoi(line.substr(10));
            else if (line.substr(0, 6) == "COUNT:")
                count = stoi(line.substr(6));
            else if (line == "---TRANSACTION---") {
                // Read one transaction block
                string cat = "", note = "";
                double amt = 0;
                Date   dt = { 0, 0, 0 };
                int    type = 0;

                for (int i = 0; i < 5 && getline(file, line); i++) {
                    if (line.substr(0, 4) == "CAT:")
                        cat = line.substr(4);
                    else if (line.substr(0, 4) == "AMT:")
                        amt = stod(line.substr(4));
                    else if (line.substr(0, 4) == "DAY:") {
                        // Format: "DAY:15 MON:5 YEAR:2024"
                        sscanf(line.c_str(), "DAY:%d MON:%d YEAR:%d",
                            &dt.d, &dt.m, &dt.y);
                    }
                    else if (line.substr(0, 5) == "NOTE:")
                        note = line.substr(5);
                    else if (line.substr(0, 5) == "TYPE:")
                        type = stoi(line.substr(5));
                }
                addTransaction(cat, amt, dt, note, type);
            }
        }
        file.close();
        checkAndResetMonthly();
    }
};

// ========================================================================
// GLOBAL VARIABLES
// ========================================================================
FinanceManager* g_fm = nullptr;
HWND            g_mainWnd = nullptr;
HWND            g_nameEdit = nullptr;
HWND            g_incomeEdit = nullptr;
HWND            g_nextBtn = nullptr;
HWND            g_transactionList = nullptr;
HWND            g_darkModeBtn = nullptr;
HWND            g_undoBtn = nullptr; // Undo button handle
HWND            g_redoBtn = nullptr; // Redo button handle
bool            g_setupComplete = false;

// ========================================================================
// UI HELPER: Draw a rounded rectangle using GDI+ GraphicsPath
// ========================================================================
static void DrawRoundedRect(Graphics* g, Brush* brush, Pen* pen,
    int x, int y, int w, int h, int radius) {
    GraphicsPath path;
    path.AddArc((REAL)x, (REAL)y, (REAL)radius, (REAL)radius, 180.0f, 90.0f);
    path.AddArc((REAL)(x + w - radius), (REAL)y, (REAL)radius, (REAL)radius, 270.0f, 90.0f);
    path.AddArc((REAL)(x + w - radius), (REAL)(y + h - radius), (REAL)radius, (REAL)radius, 0.0f, 90.0f);
    path.AddArc((REAL)x, (REAL)(y + h - radius), (REAL)radius, (REAL)radius, 90.0f, 90.0f);
    path.CloseFigure();
    if (brush) g->FillPath(brush, &path);
    if (pen)   g->DrawPath(pen, &path);
}

// ========================================================================
// Dynamic tab width - prevents numbers from overlapping at large values
// ========================================================================
static int CalculateTabWidth(double balance, double income, double expenses) {
    double maxVal = max(abs(balance), max(income, expenses));
    int digits = 0;
    double temp = maxVal;
    if (temp == 0) digits = 1;
    else { while (temp >= 1) { temp /= 10; digits++; } }
    int width = 100 + (digits - 3) * 15;
    if (width < 120) width = 120;
    if (width > 250) width = 250;
    return width;
}

// ========================================================================
// PIE CHART - uses CategoryTree DFS inorder result
// getDFSList() returns categories sorted A-Z (BST inorder property)
// Each category gets a slice proportional to its share of total expenses
// ========================================================================
static void DrawPieChartWithLegend(Graphics* g, int centerX, int centerY,
    int radius, int legendX, int legendY) {
    // Get category data via DFS inorder traversal on the BST
    auto   categories = g_fm->categoryTree.getDFSList();
    double total = g_fm->getTotalExpenses();

    if (total <= 0) {
        Font       font(L"Segoe UI", 10);
        SolidBrush grayBrush(Color(255, 150, 150, 150));
        g->DrawString(L"No expense data", -1, &font,
            PointF((REAL)(centerX - 40), (REAL)(centerY - 10)), &grayBrush);
        return;
    }

    // Vibrant color palette - each category gets a distinct color
    Color colors[] = {
        Color(255, 54,162,235), Color(255, 46,204,113),
        Color(255,255,159, 64), Color(255,153,102,255),
        Color(255, 75,192,192), Color(255,255, 99,132),
        Color(255,255,206, 86), Color(255,255,127, 80)
    };

    float      startAngle = -90.0f; // start at 12 o'clock
    Font       legendFont(L"Segoe UI", 11);
    COLORREF   tc = g_darkMode ? DARK_TEXT : LIGHT_TEXT;
    SolidBrush textBrush(Color(255, GetRValue(tc), GetGValue(tc), GetBValue(tc)));

    for (size_t i = 0; i < categories.size() && i < 8; i++) {
        double percent = categories[i].second / total;
        float  sweepAngle = (float)(percent * 360.0);

        // Draw pie slice
        SolidBrush brush(colors[i % 8]);
        g->FillPie(&brush, centerX - radius, centerY - radius,
            radius * 2, radius * 2, startAngle, sweepAngle);

        // Draw legend color box
        SolidBrush legendBrush(colors[i % 8]);
        g->FillRectangle(&legendBrush, (REAL)legendX, (REAL)(legendY + (int)i * 20), 12.0f, 12.0f);

        // Draw legend text
        WCHAR legendText[100];
        swprintf(legendText, 100, L"%S: $%.2f (%.1f%%)",
            categories[i].first.c_str(), categories[i].second, percent * 100);
        g->DrawString(legendText, -1, &legendFont,
            PointF((REAL)(legendX + 18), (REAL)(legendY + i * 20)), &textBrush);

        startAngle += sweepAngle;
    }
}

// ========================================================================
// UPDATE TRANSACTION LISTVIEW
// Traverses linked list head to tail, fills the ListView control
// This is a pure linked list traversal - O(n)
// ========================================================================
static void UpdateTransactionList(HWND listView) {
    ListView_DeleteAllItems(listView);
    Transaction* curr = g_fm->transactions.getHead();
    int          index = 0;

    while (curr) {
        LVITEMW item = { 0 };
        item.mask = LVIF_TEXT;
        item.iItem = index;

        WCHAR dateStr[20];
        swprintf(dateStr, 20, L"%02d/%02d/%d", curr->date.d, curr->date.m, curr->date.y);
        item.pszText = dateStr;
        ListView_InsertItem(listView, &item);

        const wchar_t* typeText = curr->type ? L"Expense" : L"Income";
        ListView_SetItemText(listView, index, 1, (LPWSTR)typeText);

        WCHAR category[100];
        MultiByteToWideChar(CP_ACP, 0, curr->category.c_str(), -1, category, 100);
        ListView_SetItemText(listView, index, 2, category);

        WCHAR amount[50];
        swprintf(amount, 50, L"$%.2f", curr->amount);
        ListView_SetItemText(listView, index, 3, amount);

        WCHAR note[200];
        MultiByteToWideChar(CP_ACP, 0, curr->note.c_str(), -1, note, 200);
        ListView_SetItemText(listView, index, 4, note);

        curr = curr->next;
        index++;
    }
}

// ========================================================================
// APPLY THEME - update listview colors and trigger repaint
// ========================================================================
static void ApplyTheme() {
    COLORREF cardColor = g_darkMode ? DARK_CARD : LIGHT_CARD;
    COLORREF textColor = g_darkMode ? DARK_TEXT : LIGHT_TEXT;
    ListView_SetTextColor(g_transactionList, textColor);
    ListView_SetTextBkColor(g_transactionList, cardColor);
    ListView_SetBkColor(g_transactionList, cardColor);
    g_fm->darkModePreference = g_darkMode;
    g_fm->save("finance.txt");
    InvalidateRect(g_mainWnd, NULL, TRUE);
    RedrawWindow(g_mainWnd, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
}

// ========================================================================
// ADD TRANSACTION DIALOG
// ========================================================================
static LRESULT CALLBACK AddTransProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    static HWND catEdit = nullptr, amtEdit = nullptr;
    static HWND dayEdit = nullptr, monthEdit = nullptr, yearEdit = nullptr;
    static HWND noteEdit = nullptr;
    static int  transType = 0;

    switch (msg) {
    case WM_CREATE: {
        transType = (int)(INT_PTR)((CREATESTRUCT*)lp)->lpCreateParams;
        SetWindowTextW(hwnd, transType ? L"Add Expense" : L"Add Income");
        HBRUSH bgBrush = CreateSolidBrush(g_darkMode ? DARK_BG : LIGHT_BG);
        SetClassLongPtr(hwnd, GCLP_HBRBACKGROUND, (LONG_PTR)bgBrush);

        CreateWindowW(L"STATIC", L"Category:", WS_VISIBLE | WS_CHILD, 20, 30, 70, 25, hwnd, NULL, NULL, NULL);
        catEdit = CreateWindowW(L"EDIT", L"", WS_VISIBLE | WS_CHILD | WS_BORDER, 100, 30, 200, 28, hwnd, NULL, NULL, NULL);

        CreateWindowW(L"STATIC", L"Amount:", WS_VISIBLE | WS_CHILD, 20, 70, 70, 25, hwnd, NULL, NULL, NULL);
        amtEdit = CreateWindowW(L"EDIT", L"", WS_VISIBLE | WS_CHILD | WS_BORDER, 100, 70, 150, 28, hwnd, NULL, NULL, NULL);

        CreateWindowW(L"STATIC", L"Date:", WS_VISIBLE | WS_CHILD, 20, 110, 70, 25, hwnd, NULL, NULL, NULL);
        dayEdit = CreateWindowW(L"EDIT", L"1", WS_VISIBLE | WS_CHILD | WS_BORDER, 100, 110, 55, 28, hwnd, NULL, NULL, NULL);
        CreateWindowW(L"STATIC", L"/", WS_VISIBLE | WS_CHILD, 158, 110, 10, 25, hwnd, NULL, NULL, NULL);
        monthEdit = CreateWindowW(L"EDIT", L"1", WS_VISIBLE | WS_CHILD | WS_BORDER, 170, 110, 55, 28, hwnd, NULL, NULL, NULL);
        CreateWindowW(L"STATIC", L"/", WS_VISIBLE | WS_CHILD, 228, 110, 10, 25, hwnd, NULL, NULL, NULL);
        yearEdit = CreateWindowW(L"EDIT", L"2024", WS_VISIBLE | WS_CHILD | WS_BORDER, 240, 110, 70, 28, hwnd, NULL, NULL, NULL);

        CreateWindowW(L"STATIC", L"Note:", WS_VISIBLE | WS_CHILD, 20, 150, 70, 25, hwnd, NULL, NULL, NULL);
        noteEdit = CreateWindowW(L"EDIT", L"", WS_VISIBLE | WS_CHILD | WS_BORDER, 100, 150, 200, 28, hwnd, NULL, NULL, NULL);

        CreateWindowW(L"BUTTON", L"Save", WS_VISIBLE | WS_CHILD, 90, 200, 80, 35, hwnd, (HMENU)1, NULL, NULL);
        CreateWindowW(L"BUTTON", L"Cancel", WS_VISIBLE | WS_CHILD, 200, 200, 80, 35, hwnd, (HMENU)2, NULL, NULL);
        break;
    }
    case WM_CTLCOLORSTATIC: {
        HDC hdc = (HDC)wp;
        SetTextColor(hdc, g_darkMode ? DARK_TEXT : LIGHT_TEXT);
        SetBkColor(hdc, g_darkMode ? DARK_BG : LIGHT_BG);
        return (LRESULT)CreateSolidBrush(g_darkMode ? DARK_BG : LIGHT_BG);
    }
    case WM_CTLCOLOREDIT: {
        HDC hdc = (HDC)wp;
        SetTextColor(hdc, g_darkMode ? DARK_TEXT : LIGHT_TEXT);
        SetBkColor(hdc, g_darkMode ? DARK_CARD : LIGHT_CARD);
        return (LRESULT)CreateSolidBrush(g_darkMode ? DARK_CARD : LIGHT_CARD);
    }
    case WM_COMMAND:
        if (LOWORD(wp) == 1) {
            char cat[100] = { 0 }, amt[20] = { 0 }, d[10] = { 0 }, m[10] = { 0 }, y[10] = { 0 }, note[200] = { 0 };
            GetWindowTextA(catEdit, cat, 100);
            GetWindowTextA(amtEdit, amt, 20);
            GetWindowTextA(dayEdit, d, 10);
            GetWindowTextA(monthEdit, m, 10);
            GetWindowTextA(yearEdit, y, 10);
            GetWindowTextA(noteEdit, note, 200);

            if (strlen(cat) > 0 && atof(amt) > 0) {
                Date dt = { atoi(d), atoi(m), atoi(y) };
                g_fm->addTransaction(cat, atof(amt), dt, note, transType);
                UpdateTransactionList(g_transactionList);
                InvalidateRect(g_mainWnd, NULL, TRUE);
                MessageBoxW(hwnd, L"Transaction added!", L"Success", MB_OK);
                DestroyWindow(hwnd);
            }
            else {
                MessageBoxW(hwnd, L"Please fill category and amount!", L"Error", MB_OK | MB_ICONERROR);
            }
        }
        else if (LOWORD(wp) == 2) {
            DestroyWindow(hwnd);
        }
        break;
    case WM_CLOSE: DestroyWindow(hwnd); break;
    default: return DefWindowProc(hwnd, msg, wp, lp);
    }
    return 0;
}

// ========================================================================
// SEARCH DIALOG
//
// SEARCH BY CATEGORY - uses DFS on the CategoryBST
//   Calls categoryTree.searchDFS(term)
//   DFS inorder traversal visits every BST node Left->Root->Right
//   At each node, checks if category name contains the search string
//   Results come back alphabetically sorted (BST inorder property)
//   Shows: category name, total amount spent in that category, transaction count
//   Then also shows individual transactions from linked list for matched categories
//
// SEARCH BY DATE - uses BFS on the DateTree
//   The DateTree is a 3-level tree: Root=Month, Level1=Days, Level2=Transactions
//   BFS uses a Queue (FIFO) to traverse level by level
//   Level 0: find the correct month
//   Level 1: find the correct day within that month
//   Level 2: collect all transactions on that day
//   This is exactly Lec 4 BFS level-order traversal applied to a date structure
// ========================================================================
static LRESULT CALLBACK SearchDlgProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    static HWND searchEdit = nullptr, resultList = nullptr;
    static HWND dayEdit = nullptr, monthEdit = nullptr, yearEdit = nullptr;
    static int  searchType = 1; // 1 = category (DFS), 0 = date (BFS)

    switch (msg) {
    case WM_CREATE: {
        SetWindowTextW(hwnd, L"Search Transactions");
        HBRUSH bgBrush = CreateSolidBrush(g_darkMode ? DARK_BG : LIGHT_BG);
        SetClassLongPtr(hwnd, GCLP_HBRBACKGROUND, (LONG_PTR)bgBrush);

        // Radio buttons to choose search type - wider to fit bigger window
        CreateWindowW(L"BUTTON", L"Search by Category (DFS on BST)",
            WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON, 15, 15, 280, 25, hwnd, (HMENU)100, NULL, NULL);
        CreateWindowW(L"BUTTON", L"Search by Date (BFS on Date Tree)",
            WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON, 305, 15, 280, 25, hwnd, (HMENU)101, NULL, NULL);
        CheckRadioButton(hwnd, 100, 101, 100);

        // Category search input - wider input field
        CreateWindowW(L"STATIC", L"Category Name:", WS_VISIBLE | WS_CHILD, 15, 55, 110, 25, hwnd, (HMENU)200, NULL, NULL);
        searchEdit = CreateWindowW(L"EDIT", L"", WS_VISIBLE | WS_CHILD | WS_BORDER, 135, 55, 450, 28, hwnd, NULL, NULL, NULL);

        // Date search input fields (hidden initially) - spaced for bigger window
        CreateWindowW(L"STATIC", L"Day:", WS_VISIBLE | WS_CHILD, 15, 55, 40, 25, hwnd, (HMENU)201, NULL, NULL);
        dayEdit = CreateWindowW(L"EDIT", L"", WS_VISIBLE | WS_CHILD | WS_BORDER, 60, 55, 60, 28, hwnd, NULL, NULL, NULL);
        CreateWindowW(L"STATIC", L"Month:", WS_VISIBLE | WS_CHILD, 135, 55, 50, 25, hwnd, (HMENU)202, NULL, NULL);
        monthEdit = CreateWindowW(L"EDIT", L"", WS_VISIBLE | WS_CHILD | WS_BORDER, 190, 55, 60, 28, hwnd, NULL, NULL, NULL);
        CreateWindowW(L"STATIC", L"Year:", WS_VISIBLE | WS_CHILD, 265, 55, 40, 25, hwnd, (HMENU)203, NULL, NULL);
        yearEdit = CreateWindowW(L"EDIT", L"", WS_VISIBLE | WS_CHILD | WS_BORDER, 310, 55, 80, 28, hwnd, NULL, NULL, NULL);

        // Hide date fields at start
        ShowWindow(dayEdit, SW_HIDE); ShowWindow(GetDlgItem(hwnd, 201), SW_HIDE);
        ShowWindow(monthEdit, SW_HIDE); ShowWindow(GetDlgItem(hwnd, 202), SW_HIDE);
        ShowWindow(yearEdit, SW_HIDE); ShowWindow(GetDlgItem(hwnd, 203), SW_HIDE);

        // Search button centered
        CreateWindowW(L"BUTTON", L"Search", WS_VISIBLE | WS_CHILD, 245, 100, 110, 35, hwnd, (HMENU)1, NULL, NULL);

        // Result listbox - wider and taller to use the extra space
        resultList = CreateWindowW(L"LISTBOX", L"",
            WS_VISIBLE | WS_CHILD | WS_BORDER | WS_VSCROLL | LBS_NOINTEGRALHEIGHT,
            15, 150, 595, 350, hwnd, NULL, NULL, NULL);
        break;
    }
    case WM_CTLCOLORSTATIC: {
        HDC hdc = (HDC)wp;
        SetTextColor(hdc, g_darkMode ? DARK_TEXT : LIGHT_TEXT);
        SetBkColor(hdc, g_darkMode ? DARK_BG : LIGHT_BG);
        return (LRESULT)CreateSolidBrush(g_darkMode ? DARK_BG : LIGHT_BG);
    }
    case WM_CTLCOLOREDIT: {
        HDC hdc = (HDC)wp;
        SetTextColor(hdc, g_darkMode ? DARK_TEXT : LIGHT_TEXT);
        SetBkColor(hdc, g_darkMode ? DARK_CARD : LIGHT_CARD);
        return (LRESULT)CreateSolidBrush(g_darkMode ? DARK_CARD : LIGHT_CARD);
    }
    case WM_COMMAND: {
        int cmd = LOWORD(wp);

        if (cmd == 100) {
            // Switch to category search - show text field, hide date fields
            searchType = 1;
            ShowWindow(searchEdit, SW_SHOW); ShowWindow(GetDlgItem(hwnd, 200), SW_SHOW);
            ShowWindow(dayEdit, SW_HIDE); ShowWindow(GetDlgItem(hwnd, 201), SW_HIDE);
            ShowWindow(monthEdit, SW_HIDE); ShowWindow(GetDlgItem(hwnd, 202), SW_HIDE);
            ShowWindow(yearEdit, SW_HIDE); ShowWindow(GetDlgItem(hwnd, 203), SW_HIDE);
        }
        else if (cmd == 101) {
            // Switch to date search - show date fields, hide text field
            searchType = 0;
            ShowWindow(searchEdit, SW_HIDE); ShowWindow(GetDlgItem(hwnd, 200), SW_HIDE);
            ShowWindow(dayEdit, SW_SHOW); ShowWindow(GetDlgItem(hwnd, 201), SW_SHOW);
            ShowWindow(monthEdit, SW_SHOW); ShowWindow(GetDlgItem(hwnd, 202), SW_SHOW);
            ShowWindow(yearEdit, SW_SHOW); ShowWindow(GetDlgItem(hwnd, 203), SW_SHOW);
        }
        else if (cmd == 1) {
            // Search button clicked
            SendMessageA(resultList, LB_RESETCONTENT, 0, 0);

            if (searchType == 1) {
                // ============================================================
                // SEARCH BY CATEGORY - DFS on CategoryBST
                // ============================================================
                char searchStr[100] = { 0 };
                GetWindowTextA(searchEdit, searchStr, 100);

                if (strlen(searchStr) == 0) {
                    SendMessageA(resultList, LB_ADDSTRING, 0, (LPARAM)"Please enter a category name");
                    return 0;
                }

                // Call DFS search on the BST
                // This does inorder DFS (Left->Root->Right) and checks each node
                auto dfsResults = g_fm->categoryTree.searchDFS(searchStr);

                if (dfsResults.empty()) {
                    SendMessageA(resultList, LB_ADDSTRING, 0,
                        (LPARAM)"No matching category found in BST");
                    return 0;
                }

                // Show header
                SendMessageA(resultList, LB_ADDSTRING, 0,
                    (LPARAM)"=== DFS Search Results (Alphabetical from BST) ===");

                // Show each matching category from DFS
                for (auto& cat : dfsResults) {
                    char line[200];
                    sprintf(line, "[DFS-MATCH] Category: %s | Total Spent: $%.2f",
                        cat.first.c_str(), cat.second);
                    SendMessageA(resultList, LB_ADDSTRING, 0, (LPARAM)line);
                }

                // Also walk the linked list to show individual transactions
                // for each matched category
                SendMessageA(resultList, LB_ADDSTRING, 0,
                    (LPARAM)"--- Individual Transactions from Linked List ---");

                string searchLower = searchStr;
                transform(searchLower.begin(), searchLower.end(), searchLower.begin(), ::tolower);

                Transaction* curr = g_fm->transactions.getHead();
                bool found = false;
                while (curr) {
                    string catLower = curr->category;
                    transform(catLower.begin(), catLower.end(), catLower.begin(), ::tolower);
                    if (catLower.find(searchLower) != string::npos) {
                        char line[250];
                        sprintf(line, "  [%s] %s: $%.2f - %02d/%02d/%d | %s",
                            curr->type ? "EXP" : "INC",
                            curr->category.c_str(), curr->amount,
                            curr->date.d, curr->date.m, curr->date.y,
                            curr->note.c_str());
                        SendMessageA(resultList, LB_ADDSTRING, 0, (LPARAM)line);
                        found = true;
                    }
                    curr = curr->next; // linked list traversal
                }
                if (!found)
                    SendMessageA(resultList, LB_ADDSTRING, 0, (LPARAM)"  No individual transactions found");
            }
            else {
                // ============================================================
                // SEARCH BY DATE - BFS on DateTree
                // The DateTree is structured as: Root=Month -> Days -> Transactions
                // BFS uses a Queue to process level by level:
                //   Level 0: Check month match
                //   Level 1: Check day match
                //   Level 2: Collect transaction results
                // ============================================================
                char d[10] = { 0 }, m[10] = { 0 }, y[10] = { 0 };
                GetWindowTextA(dayEdit, d, 10);
                GetWindowTextA(monthEdit, m, 10);
                GetWindowTextA(yearEdit, y, 10);

                int searchDay = atoi(d);
                int searchMonth = atoi(m);
                int searchYear = atoi(y);

                if (searchDay == 0 || searchMonth == 0 || searchYear == 0) {
                    SendMessageA(resultList, LB_ADDSTRING, 0,
                        (LPARAM)"Please enter valid Day, Month and Year");
                    return 0;
                }

                // Show BFS explanation header
                SendMessageA(resultList, LB_ADDSTRING, 0,
                    (LPARAM)"=== BFS Date Search: Date Tree Level-Order ===");
                char header[100];
                sprintf(header, "Tree Structure: Root=Month(%d) -> Days -> Transactions",
                    searchMonth);
                SendMessageA(resultList, LB_ADDSTRING, 0, (LPARAM)header);
                SendMessageA(resultList, LB_ADDSTRING, 0,
                    (LPARAM)"BFS Queue processes: Level0(Month)->Level1(Day)->Level2(Trans)");
                SendMessageA(resultList, LB_ADDSTRING, 0, (LPARAM)"-----");

                // Run BFS on the DateTree
                // Returns only transactions matching the exact date
                auto bfsResults = g_fm->dateTree.searchByDateBFS(
                    searchDay, searchMonth, searchYear
                );

                if (bfsResults.empty()) {
                    char noResult[100];
                    sprintf(noResult, "No transactions found for %02d/%02d/%d",
                        searchDay, searchMonth, searchYear);
                    SendMessageA(resultList, LB_ADDSTRING, 0, (LPARAM)noResult);
                }
                else {
                    char countLine[100];
                    sprintf(countLine, "BFS found %zu transaction(s) on %02d/%02d/%d:",
                        bfsResults.size(), searchDay, searchMonth, searchYear);
                    SendMessageA(resultList, LB_ADDSTRING, 0, (LPARAM)countLine);

                    for (auto& t : bfsResults) {
                        char line[250];
                        sprintf(line, "  [BFS-FOUND] [%s] %s: $%.2f | %s",
                            t.type ? "EXP" : "INC",
                            t.category.c_str(), t.amount,
                            t.note.c_str());
                        SendMessageA(resultList, LB_ADDSTRING, 0, (LPARAM)line);
                    }
                }
            }
        }
        break;
    }
    case WM_CLOSE: DestroyWindow(hwnd); break;
    default: return DefWindowProc(hwnd, msg, wp, lp);
    }
    return 0;
}

// ========================================================================
// ANALYTICS DIALOG - Bar chart using CategoryTree DFS results
// Same DFS inorder traversal as pie chart
// Tallest bar = category with highest spending
// All bars scaled relative to max
// ========================================================================
static LRESULT CALLBACK AnalyticsProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rect;
        GetClientRect(hwnd, &rect);

        HDC      memDC = CreateCompatibleDC(hdc);
        HBITMAP  memBitmap = CreateCompatibleBitmap(hdc, rect.right, rect.bottom);
        SelectObject(memDC, memBitmap);
        Graphics g(memDC);
        g.SetSmoothingMode(SmoothingModeAntiAlias);

        COLORREF   bgC = g_darkMode ? DARK_BG : LIGHT_BG;
        SolidBrush bgBrush(Color(255, GetRValue(bgC), GetGValue(bgC), GetBValue(bgC)));
        g.FillRectangle(&bgBrush, 0.0f, 0.0f, (REAL)rect.right, (REAL)rect.bottom);

        Font       titleFont(L"Segoe UI", 16, FontStyleBold);
        COLORREF   tc = g_darkMode ? DARK_TEXT : LIGHT_TEXT;
        SolidBrush textBrush(Color(255, GetRValue(tc), GetGValue(tc), GetBValue(tc)));
        g.DrawString(L"Expenses by Category (DFS Inorder on BST)", -1, &titleFont,
            PointF(20, 10), &textBrush);

        // Get DFS inorder result from CategoryBST - categories sorted A-Z
        auto   categories = g_fm->categoryTree.getDFSList();
        double maxAmount = 0;
        for (auto& cat : categories)
            if (cat.second > maxAmount) maxAmount = cat.second;
        if (maxAmount == 0) maxAmount = 1;

        Color colors[] = {
            Color(255, 54,162,235), Color(255, 46,204,113),
            Color(255,255,159, 64), Color(255,153,102,255),
            Color(255, 75,192,192), Color(255,255, 99,132),
            Color(255,255,206, 86), Color(255,255,127, 80)
        };

        int barWidth = (rect.right - 100) / max(1, (int)categories.size());
        if (barWidth > 80) barWidth = 80;
        int startX = 50;
        int baseY = rect.bottom - 60;

        Font labelFont(L"Segoe UI", 9);
        Font valueFont(L"Segoe UI", 8);

        for (size_t i = 0; i < categories.size() && i < 8; i++) {
            int barHeight = (int)((categories[i].second / maxAmount) * (rect.bottom - 120));
            if (barHeight < 10) barHeight = 10;

            SolidBrush barBrush(colors[i % 8]);
            g.FillRectangle(&barBrush,
                (REAL)(startX + (int)i * barWidth), (REAL)(baseY - barHeight),
                (REAL)(barWidth - 5), (REAL)barHeight);

            WCHAR catName[50];
            MultiByteToWideChar(CP_ACP, 0, categories[i].first.c_str(), -1, catName, 50);
            g.DrawString(catName, -1, &labelFont,
                PointF((REAL)(startX + i * barWidth), (REAL)(baseY + 5)), &textBrush);

            WCHAR amountStr[50];
            swprintf(amountStr, 50, L"$%.0f", categories[i].second);
            g.DrawString(amountStr, -1, &valueFont,
                PointF((REAL)(startX + i * barWidth), (REAL)(baseY - barHeight - 15)), &textBrush);
        }

        // Grid lines
        COLORREF   bc = g_darkMode ? DARK_BORDER : LIGHT_BORDER;
        Pen        linePen(Color(255, GetRValue(bc), GetGValue(bc), GetBValue(bc)), 1);
        for (int i = 0; i <= 4; i++) {
            double val = (maxAmount / 4) * i;
            int    y = baseY - (int)((val / maxAmount) * (rect.bottom - 120));
            WCHAR  label[20];
            swprintf(label, 20, L"$%.0f", val);
            g.DrawString(label, -1, &valueFont, PointF(10, (REAL)(y - 8)), &textBrush);
            g.DrawLine(&linePen, 40.0f, (REAL)y, (REAL)(rect.right - 20), (REAL)y);
        }

        BitBlt(hdc, 0, 0, rect.right, rect.bottom, memDC, 0, 0, SRCCOPY);
        DeleteObject(memBitmap);
        DeleteDC(memDC);
        EndPaint(hwnd, &ps);
        break;
    }
    case WM_CLOSE: DestroyWindow(hwnd); break;
    default: return DefWindowProc(hwnd, msg, wp, lp);
    }
    return 0;
}

// ========================================================================
// MAIN WINDOW PROCEDURE
// ========================================================================
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        g_mainWnd = hwnd;

        g_nameEdit = CreateWindowW(L"EDIT", L"", WS_VISIBLE | WS_CHILD | WS_BORDER, 20, 20, 180, 30, hwnd, NULL, NULL, NULL);
        g_incomeEdit = CreateWindowW(L"EDIT", L"", WS_VISIBLE | WS_CHILD | WS_BORDER, 220, 20, 150, 30, hwnd, NULL, NULL, NULL);
        g_nextBtn = CreateWindowW(L"BUTTON", L"Next", WS_VISIBLE | WS_CHILD, 390, 20, 80, 30, hwnd, (HMENU)1, NULL, NULL);
        g_darkModeBtn = CreateWindowW(L"BUTTON", L"\U0001F319 Dark", WS_VISIBLE | WS_CHILD, 650, 20, 110, 30, hwnd, (HMENU)9, NULL, NULL);

        // Transaction ListView
        g_transactionList = CreateWindowW(WC_LISTVIEWW, L"",
            WS_VISIBLE | WS_CHILD | WS_BORDER | LVS_REPORT,
            20, 460, 600, 210, hwnd, NULL, NULL, NULL);

        HFONT hFont = CreateFontW(18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        SendMessage(g_transactionList, WM_SETFONT, (WPARAM)hFont, TRUE);

        LVCOLUMNW col = { 0 };
        col.mask = LVCF_TEXT | LVCF_WIDTH;
        col.cx = 95;  col.pszText = (LPWSTR)L"Date";     ListView_InsertColumn(g_transactionList, 0, &col);
        col.cx = 85;  col.pszText = (LPWSTR)L"Type";     ListView_InsertColumn(g_transactionList, 1, &col);
        col.cx = 120; col.pszText = (LPWSTR)L"Category"; ListView_InsertColumn(g_transactionList, 2, &col);
        col.cx = 100; col.pszText = (LPWSTR)L"Amount";   ListView_InsertColumn(g_transactionList, 3, &col);
        col.cx = 190; col.pszText = (LPWSTR)L"Note";     ListView_InsertColumn(g_transactionList, 4, &col);

        // Buttons row 1
        int bW = 110, bH = 40, sX = (620 - (4 * 110 + 3 * 15)) / 2;
        CreateWindowW(L"BUTTON", L"Add Expense", WS_VISIBLE | WS_CHILD, sX, 685, bW, bH, hwnd, (HMENU)2, NULL, NULL);
        CreateWindowW(L"BUTTON", L"Add Income", WS_VISIBLE | WS_CHILD, sX + bW + 15, 685, bW, bH, hwnd, (HMENU)3, NULL, NULL);
        CreateWindowW(L"BUTTON", L"Search", WS_VISIBLE | WS_CHILD, sX + 2 * (bW + 15), 685, bW, bH, hwnd, (HMENU)4, NULL, NULL);
        CreateWindowW(L"BUTTON", L"Analytics", WS_VISIBLE | WS_CHILD, sX + 3 * (bW + 15), 685, bW, bH, hwnd, (HMENU)5, NULL, NULL);

        // Buttons row 2
        int bW2 = 120, sX2 = (620 - (3 * 120 + 2 * 20)) / 2;
        CreateWindowW(L"BUTTON", L"Export", WS_VISIBLE | WS_CHILD, sX2, 740, bW2, bH, hwnd, (HMENU)6, NULL, NULL);
        CreateWindowW(L"BUTTON", L"Reset", WS_VISIBLE | WS_CHILD, sX2 + bW2 + 20, 740, bW2, bH, hwnd, (HMENU)7, NULL, NULL);
        CreateWindowW(L"BUTTON", L"Exit", WS_VISIBLE | WS_CHILD, sX2 + 2 * (bW2 + 20), 740, bW2, bH, hwnd, (HMENU)8, NULL, NULL);

        // ================================================================
        // Buttons row 3 - Undo and Redo
        // These use the undoStack and redoStack (LIFO stacks from Lec 2)
        // Undo: pops last transaction off undoStack, pushes to redoStack
        // Redo: pops from redoStack, re-adds transaction, restores undo
        // ================================================================
        int bW3 = 130, sX3 = (620 - (2 * 130 + 30)) / 2;
        g_undoBtn = CreateWindowW(L"BUTTON", L"\u21A9 Undo", WS_VISIBLE | WS_CHILD,
            sX3 + bW3 + 340, 500, bW3, bH, hwnd, (HMENU)10, NULL, NULL);
        g_redoBtn = CreateWindowW(L"BUTTON", L"\u21AA Redo", WS_VISIBLE | WS_CHILD,
            sX3 + bW3 + 340, 550, bW3, bH, hwnd, (HMENU)11, NULL, NULL);

        UpdateTransactionList(g_transactionList);

        g_darkMode = g_fm->darkModePreference;
        SetWindowTextW(g_darkModeBtn, g_darkMode ? L"\u2600 Light" : L"\U0001F319 Dark");
        ApplyTheme();

        if (g_setupComplete) {
            ShowWindow(g_nameEdit, SW_HIDE);
            ShowWindow(g_incomeEdit, SW_HIDE);
            ShowWindow(g_nextBtn, SW_HIDE);
        }
        break;
    }
    case WM_CTLCOLORSTATIC: {
        HDC hdc = (HDC)wp;
        SetTextColor(hdc, g_darkMode ? DARK_TEXT : LIGHT_TEXT);
        SetBkColor(hdc, g_darkMode ? DARK_BG : LIGHT_BG);
        return (LRESULT)CreateSolidBrush(g_darkMode ? DARK_BG : LIGHT_BG);
    }
    case WM_CTLCOLOREDIT: {
        HDC hdc = (HDC)wp;
        SetTextColor(hdc, g_darkMode ? DARK_TEXT : LIGHT_TEXT);
        SetBkColor(hdc, g_darkMode ? DARK_CARD : LIGHT_CARD);
        return (LRESULT)CreateSolidBrush(g_darkMode ? DARK_CARD : LIGHT_CARD);
    }
    case WM_CTLCOLORBTN: {
        HDC hdc = (HDC)wp;
        SetTextColor(hdc, g_darkMode ? DARK_TEXT : LIGHT_TEXT);
        SetBkColor(hdc, RACING_RED);
        return (LRESULT)CreateSolidBrush(RACING_RED);
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rect;
        GetClientRect(hwnd, &rect);
        HDC     memDC = CreateCompatibleDC(hdc);
        HBITMAP memBitmap = CreateCompatibleBitmap(hdc, rect.right, rect.bottom);
        SelectObject(memDC, memBitmap);
        Graphics graphics(memDC);
        graphics.SetSmoothingMode(SmoothingModeAntiAlias);
        graphics.SetTextRenderingHint(TextRenderingHintAntiAlias);

        // Background
        COLORREF   bgC = g_darkMode ? DARK_BG : LIGHT_BG;
        SolidBrush bgBrush(Color(255, GetRValue(bgC), GetGValue(bgC), GetBValue(bgC)));
        graphics.FillRectangle(&bgBrush, 0.0f, 0.0f, (REAL)rect.right, (REAL)rect.bottom);

        // Header gradient (Racing Red)
        COLORREF hS = g_darkMode ? DARK_HEADER_START : LIGHT_HEADER_START;
        COLORREF hE = g_darkMode ? DARK_HEADER_END : LIGHT_HEADER_END;
        LinearGradientBrush headerBrush(
            Point(0, 0), Point(0, 150),
            Color(255, GetRValue(hS), GetGValue(hS), GetBValue(hS)),
            Color(255, GetRValue(hE), GetGValue(hE), GetBValue(hE)));
        DrawRoundedRect(&graphics, &headerBrush, nullptr, 0, 0, rect.right, 150, 0);

        Font titleFont(L"Segoe UI", 18, FontStyleBold);
        Font labelFont(L"Segoe UI", 12);
        Font valueFont(L"Segoe UI", 18, FontStyleBold);

        // ============================================================
        // DYNAMIC BALANCE / INCOME / EXPENSES
        // Each of these calls traverses the full linked list
        // getTotalExpenses(): sum all type==1 nodes
        // getTotalIncome():   monthlyIncome + sum all type==0 nodes
        // getBalance():       income - expenses
        // All recalculated every repaint = always up to date
        // ============================================================
        double balance = g_fm->getBalance();
        double income = g_fm->getTotalIncome();
        double expenses = g_fm->getTotalExpenses();

        int tabWidth = CalculateTabWidth(balance, income, expenses);
        int tabSpacing = 50;

        COLORREF   tc = g_darkMode ? DARK_TEXT : LIGHT_TEXT;
        SolidBrush welcomeBrush(Color(255, GetRValue(tc), GetGValue(tc), GetBValue(tc)));
        SolidBrush greenBrush(Color(255, 50, 205, 50));
        SolidBrush redBrush(Color(255, 220, 50, 50));
        SolidBrush racingRedBrush(Color(255, 255, 0, 0));

        if (!g_setupComplete) {
            graphics.DrawString(L"Enter your name:", -1, &labelFont, PointF(20, 3), &welcomeBrush);
            graphics.DrawString(L"Monthly income:", -1, &labelFont, PointF(220, 3), &welcomeBrush);
        }
        else {
            WCHAR greeting[100];
            swprintf(greeting, 100, L"Welcome back, %S!", g_fm->userName.c_str());
            graphics.DrawString(greeting, -1, &titleFont, PointF(20, 20), &welcomeBrush);
        }

        // Balance card
        int      startY = 70;
        COLORREF cardC = g_darkMode ? DARK_CARD : LIGHT_CARD;
        SolidBrush cardBrush(Color(255, GetRValue(cardC), GetGValue(cardC), GetBValue(cardC)));
        int cardWidth = (tabWidth + tabSpacing) * 3 + 80;
        if (cardWidth < rect.right - 40) cardWidth = rect.right - 40;
        DrawRoundedRect(&graphics, &cardBrush, nullptr, 20, startY, cardWidth, 120, 15);

        WCHAR balStr[50], incStr[50], expStr[50];
        swprintf(balStr, 50, L"$%.2f", balance);
        swprintf(incStr, 50, L"$%.2f", income);
        swprintf(expStr, 50, L"$%.2f", expenses);

        SolidBrush textBrush(Color(255, GetRValue(tc), GetGValue(tc), GetBValue(tc)));

        int balanceX = 55;
        graphics.DrawString(L"Balance", -1, &labelFont, PointF((REAL)balanceX, (REAL)(startY + 20)), &textBrush);
        graphics.DrawString(balStr, -1, &valueFont, PointF((REAL)balanceX, (REAL)(startY + 50)),
            balance >= 0 ? &greenBrush : &redBrush);

        int incomeX = balanceX + tabWidth + tabSpacing;
        graphics.DrawString(L"Income", -1, &labelFont, PointF((REAL)incomeX, (REAL)(startY + 20)), &textBrush);
        graphics.DrawString(incStr, -1, &valueFont, PointF((REAL)incomeX, (REAL)(startY + 50)), &greenBrush);

        int expenseX = incomeX + tabWidth + tabSpacing;
        graphics.DrawString(L"Expenses", -1, &labelFont, PointF((REAL)expenseX, (REAL)(startY + 20)), &textBrush);
        graphics.DrawString(expStr, -1, &valueFont, PointF((REAL)expenseX, (REAL)(startY + 50)), &racingRedBrush);

        COLORREF   bc = g_darkMode ? DARK_BORDER : LIGHT_BORDER;
        Pen        separatorPen(Color(255, GetRValue(bc), GetGValue(bc), GetBValue(bc)), 1);
        graphics.DrawLine(&separatorPen, (REAL)(balanceX + tabWidth + tabSpacing / 2), (REAL)(startY + 15),
            (REAL)(balanceX + tabWidth + tabSpacing / 2), (REAL)(startY + 100));
        graphics.DrawLine(&separatorPen, (REAL)(incomeX + tabWidth + tabSpacing / 2), (REAL)(startY + 15),
            (REAL)(incomeX + tabWidth + tabSpacing / 2), (REAL)(startY + 100));

        // ============================================================
        // BUDGET PROGRESS BAR
        // percent = (total expenses / monthly income) * 100
        // Red fill width = percent of full bar width (capped at 100%)
        // ============================================================
        startY = 210;
        DrawRoundedRect(&graphics, &cardBrush, nullptr, 20, startY, rect.right - 40, 80, 15);
        double percent = g_fm->monthlyIncome > 0
            ? (g_fm->getTotalExpenses() / g_fm->monthlyIncome) * 100 : 0;

        COLORREF   pgC = g_darkMode ? DARK_PROGRESS_BG : LIGHT_PROGRESS_BG;
        SolidBrush lightGray(Color(255, GetRValue(pgC), GetGValue(pgC), GetBValue(pgC)));
        DrawRoundedRect(&graphics, &lightGray, nullptr, 40, startY + 30, rect.right - 80, 30, 15);
        int fillWidth = (int)((rect.right - 80) * min(percent / 100.0, 1.0));
        if (fillWidth > 0) {
            LinearGradientBrush progBrush(
                Point(0, 0), Point(fillWidth, 0),
                Color(255, 255, 0, 0), Color(255, 180, 0, 0));
            DrawRoundedRect(&graphics, &progBrush, nullptr, 40, startY + 30, fillWidth, 30, 15);
        }
        WCHAR progStr[50];
        swprintf(progStr, 50, L"Budget: %.0f%% used", percent);
        graphics.DrawString(progStr, -1, &labelFont, PointF(40, (REAL)(startY + 7)), &textBrush);

        // ============================================================
        // PIE CHART SECTION
        // Uses CategoryTree DFS inorder to get categories sorted A-Z
        // Each slice = category share of total expenses
        // ============================================================
        startY = 298;
        DrawRoundedRect(&graphics, &cardBrush, nullptr, 20, startY, rect.right - 40, 155, 15);
        graphics.DrawString(L"Category Distribution (DFS on BST)", -1, &labelFont,
            PointF(40, (REAL)(startY + 10)), &textBrush);
        DrawPieChartWithLegend(&graphics, 180, startY + 90, 60, 320, startY + 30);

        BitBlt(hdc, 0, 0, rect.right, rect.bottom, memDC, 0, 0, SRCCOPY);
        DeleteObject(memBitmap);
        DeleteDC(memDC);
        EndPaint(hwnd, &ps);
        break;
    }
    case WM_COMMAND: {
        if (LOWORD(wp) == 1) {
            char name[100] = { 0 }, income[20] = { 0 };
            GetWindowTextA(g_nameEdit, name, 100);
            GetWindowTextA(g_incomeEdit, income, 20);
            if (strlen(name) > 0 && atof(income) > 0) {
                g_fm->userName = name;
                g_fm->monthlyIncome = atof(income);
                g_fm->save("finance.txt");
                g_setupComplete = true;
                ShowWindow(g_nameEdit, SW_HIDE);
                ShowWindow(g_incomeEdit, SW_HIDE);
                ShowWindow(g_nextBtn, SW_HIDE);
                InvalidateRect(hwnd, NULL, TRUE);
            }
            else {
                MessageBoxA(hwnd, "Please enter valid name and income!", "Error", MB_OK | MB_ICONERROR);
            }
        }
        else if (LOWORD(wp) == 9) {
            g_darkMode = !g_darkMode;
            SetWindowTextW(g_darkModeBtn, g_darkMode ? L"\u2600 Light" : L"\U0001F319 Dark");
            ApplyTheme();
        }
        else if (LOWORD(wp) == 2 || LOWORD(wp) == 3) {
            if (g_setupComplete) {
                HWND dlg = CreateWindowW(L"STATIC", L"",
                    WS_OVERLAPPEDWINDOW | WS_CAPTION | WS_SYSMENU, 450, 250, 360, 280, NULL, NULL, NULL, NULL);
                SetWindowLongPtr(dlg, GWLP_WNDPROC, (LONG_PTR)AddTransProc);
                CREATESTRUCT cs = { 0 };
                cs.lpCreateParams = (void*)(INT_PTR)(LOWORD(wp) == 2 ? 1 : 0);
                SendMessage(dlg, WM_CREATE, 0, (LPARAM)&cs);
                ShowWindow(dlg, SW_SHOW);
            }
        }
        else if (LOWORD(wp) == 4) {
            if (g_setupComplete) {
                HWND dlg = CreateWindowW(L"STATIC", L"",
                    WS_OVERLAPPEDWINDOW | WS_CAPTION | WS_SYSMENU, 350, 150, 640, 580, NULL, NULL, NULL, NULL);
                SetWindowLongPtr(dlg, GWLP_WNDPROC, (LONG_PTR)SearchDlgProc);
                SendMessage(dlg, WM_CREATE, 0, 0);
                ShowWindow(dlg, SW_SHOW);
            }
        }
        else if (LOWORD(wp) == 5) {
            if (g_setupComplete) {
                HWND dlg = CreateWindowW(L"STATIC", L"Analytics",
                    WS_OVERLAPPEDWINDOW | WS_CAPTION | WS_SYSMENU, 500, 200, 600, 450, NULL, NULL, NULL, NULL);
                SetWindowLongPtr(dlg, GWLP_WNDPROC, (LONG_PTR)AnalyticsProc);
                ShowWindow(dlg, SW_SHOW);
            }
        }
        else if (LOWORD(wp) == 6) {
            if (g_setupComplete) {
                g_fm->exportMonthlyExpenses();
                char msg[200];
                sprintf(msg, "Data exported to expenses_%02d_%d.txt",
                    g_fm->lastResetMonth, g_fm->lastResetYear);
                MessageBoxA(hwnd, msg, "Success", MB_OK);
            }
        }
        else if (LOWORD(wp) == 7) {
            if (MessageBoxA(hwnd, "Reset all data?", "Confirm", MB_YESNO) == IDYES) {
                delete g_fm;
                g_fm = new FinanceManager();
                g_setupComplete = false;
                ShowWindow(g_nameEdit, SW_SHOW);
                ShowWindow(g_incomeEdit, SW_SHOW);
                ShowWindow(g_nextBtn, SW_SHOW);
                SetWindowTextA(g_nameEdit, "");
                SetWindowTextA(g_incomeEdit, "");
                UpdateTransactionList(g_transactionList);
                InvalidateRect(hwnd, NULL, TRUE);
            }
        }
        else if (LOWORD(wp) == 8) {
            if (MessageBoxA(hwnd, "Exit?", "Exit", MB_YESNO | MB_ICONQUESTION) == IDYES)
                DestroyWindow(hwnd);
        }
        // ================================================================
        // UNDO - pops the undoStack (LIFO) and removes the last transaction
        // Stack pop is O(1) per Lec 2 stack operations
        // ================================================================
        else if (LOWORD(wp) == 10) {
            if (g_setupComplete) {
                if (g_fm->undoTransaction()) {
                    UpdateTransactionList(g_transactionList);
                    InvalidateRect(hwnd, NULL, TRUE);
                    MessageBoxA(hwnd, "Last transaction undone.", "Undo", MB_OK);
                }
                else {
                    MessageBoxA(hwnd, "Nothing to undo - stack is empty.", "Undo", MB_OK | MB_ICONINFORMATION);
                }
            }
        }
        // ================================================================
        // REDO - pops the redoStack (LIFO) and re-adds the transaction
        // Stack pop is O(1) per Lec 2 stack operations
        // ================================================================
        else if (LOWORD(wp) == 11) {
            if (g_setupComplete) {
                if (g_fm->redoTransaction()) {
                    UpdateTransactionList(g_transactionList);
                    InvalidateRect(hwnd, NULL, TRUE);
                    MessageBoxA(hwnd, "Transaction redone.", "Redo", MB_OK);
                }
                else {
                    MessageBoxA(hwnd, "Nothing to redo - stack is empty.", "Redo", MB_OK | MB_ICONINFORMATION);
                }
            }
        }
        break;
    }
    case WM_DESTROY:
        g_fm->save("finance.txt");
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hwnd, msg, wp, lp);
    }
    return 0;
}

// ========================================================================
// ENTRY POINT
// ========================================================================
int WINAPI WinMain(_In_ HINSTANCE hInst, _In_opt_ HINSTANCE,
    _In_ LPSTR, _In_ int) {
    // Initialize GDI+ for graphics
    GdiplusStartupInput gdiplusInput;
    ULONG_PTR           gdiplusToken;
    GdiplusStartup(&gdiplusToken, &gdiplusInput, NULL);

    // Initialize common controls for ListView
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_LISTVIEW_CLASSES;
    InitCommonControlsEx(&icex);

    // Create finance manager and load saved data from txt file
    g_fm = new FinanceManager();
    g_fm->load("finance.txt");

    g_darkMode = g_fm->darkModePreference;

    if (!g_fm->userName.empty() && g_fm->monthlyIncome > 0)
        g_setupComplete = true;

    // Register window class
    WNDCLASSW wc = { 0 };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hbrBackground = CreateSolidBrush(LIGHT_BG);
    wc.lpszClassName = L"FinanceApp";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassW(&wc);

    // Create main window
    g_mainWnd = CreateWindowW(
        L"FinanceApp",
        L"Personal Financial Management Application (PFMA)",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        100, 50, 800, 890, NULL, NULL, hInst, NULL
    );

    // Message loop
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    delete g_fm;
    GdiplusShutdown(gdiplusToken);
    return 0;
}