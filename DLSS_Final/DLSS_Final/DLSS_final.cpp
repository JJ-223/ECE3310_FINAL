// STL headers used throughout the Sudoku DLX solver //
#include <iostream>        // Input/output (cout, cin)
#include <utility>         // Utility helpers (std::pair, std::move) 
#include <vector>          // Dynamic arrays (used for gride, node storage, etc.)
#include <string>          // String handling for column names and user input
#include <fstream>         // File input when loading Sudoku puzzles from disk
#include <algorithm>       // Algorithms like std::shuffle, std::find, std::transform
#include <cctype>          // Character classification (to lower for user input)
#include <random>          // Random number generation for Sudoku puzzle generator

using namespace std;

struct Column;

// ----------------- Core DLX structs ----------------- //
// A Node represents a single "1" in the exact cover matrix.
// Each node is part of a 4-way doubly linked torodial structure used by Dancing Links
struct Node {
    Node* L, * R, * U, * D;        // Links to neighboring node (left, right, up, down)
    Column* C;                  // Pointer to the column header this node belongs to
    int rowID;                  // Encodes the Sudoku choice (r, c, d) as a single integer

    Node() : L(this), R(this), U(this), D(this), C(nullptr), rowID(-1) {}
    // Constructor initializes the node to a self-loop in all directions.
    // Makes it easy to insert it into the torodial structure later
};

// Column header used by DLX. Inherits Node so it fits in the same
// left-right / up-down linked structure.
struct Column : Node {
    int size;       // Number of data nodes in this column
    string name;    // Optional label (useful for debugging
    explicit Column(string n = "") : Node(), size(0), name(std::move(n)) {
        C = this; // Column header's C points to itself
    }
};

// ----------------- DLX class ----------------- //
// DLX: Dancing Links implmentation for exact-cover problems (e.g: Sudoku)
class DLX {
public:
    Column head;              // Anchor / header of the column list
    vector<Column*> cols;     // Column headers
    vector<Node*>   nodes;    // All data nodes in the matrix
    vector<int>     solution; // rowIDs of chosen rows (partial/full solution)

    // Prevents copyiny or moving (linked structure stores &head, can't be trivially copied)
    DLX(const DLX&) = delete;
    DLX& operator=(const DLX&) = delete;
    DLX(DLX&&) = delete;
    DLX& operator=(DLX&&) = delete;
    // Constructor: create numCols columns and link them into the header
    explicit DLX(int numCols) : head("head") {
        // Explicitly make header a self-loop
        head.L = head.R = head.U = head.D = &head;
        head.size = 0;

        cols.reserve(numCols);
        for (int i = 0; i < numCols; i++) {
            auto* c = new Column(to_string(i));
            cols.push_back(c);
            insertColumn(c);    // link column to header list
        }
    }

    ~DLX() {
        // Clean-up: Delete data nodes first, then column headers
        for (auto* n : nodes) delete n;
        for (auto* c : cols)  delete c;
    }

private:
    // Insert c (a new column) immediately to the right of head
    void insertColumn(Column* c) {    
        c->R = head.R;
        c->L = &head;
        head.R->L = c;
        head.R = c;
    }

public:
    Node* addNode(int colIndex, int rowID) {
        Column* c = cols[colIndex];
        Node* n = new Node();
        nodes.push_back(n);

        // Insert at bottom of column c (before c)
        n->D = c;
        n->U = c->U;
        c->U->D = n;
        c->U = n;

        n->C = c;
        c->size++;
        n->rowID = rowID;
        return n;
    }

    // Make a circular doubly-linked row from the given nodes
    static void linkRow(const std::vector<Node*>& row) {
        if (row.empty()) return;
        int n = (int)row.size();
        for (int i = 0; i < n; i++) {
            Node* a = row[i];
            Node* b = row[(i + 1) % n];
            a->R = b;
            b->L = a;
        }
    }

    // Standard DLX cover / uncover

    static void cover(Column* c) {
        c->R->L = c->L;
        c->L->R = c->R;

        for (Node* row = c->D; row != c; row = row->D) {
            for (Node* node = row->R; node != row; node = node->R) {
                node->D->U = node->U;
                node->U->D = node->D;
                node->C->size--;
            }
        }
    }

    static void uncover(Column* c) {
        for (Node* row = c->U; row != c; row = row->U) {
            for (Node* node = row->L; node != row; node = node->L) {
                node->C->size++;
                node->D->U = node;
                node->U->D = node;
            }
        }
        c->R->L = c;
        c->L->R = c;
    }

    bool search() {
        if (head.R == &head) return true; // all constraints satisfied

        // Choose column with smallest size (heuristic)
        Column* c = nullptr;
        int minSize = 1000000000;
        for (auto* j = (Column*)head.R; j != &head; j = (Column*)j->R) {
            if (j->size < minSize) {
                minSize = j->size;
                c = j;
            }
        }
        if (!c || c->size == 0) return false; // dead end

        cover(c);

        for (Node* r = c->D; r != c; r = r->D) {
            solution.push_back(r->rowID);

            for (Node* j = r->R; j != r; j = j->R)
                cover(j->C);

            if (search()) return true;

            for (Node* j = r->L; j != r; j = j->L)
                uncover(j->C);

            solution.pop_back();
        }

        uncover(c);
        return false;
    }
};
// ----------------- Sudoku Generator -----------------

// Global RNG so both generator functions use the same random engine
static std::mt19937 rng(random_device{}());

// Backtracking generator for a full valid Sudoku grid
bool fillSudoku(vector<vector<int>>& grid, int r = 0, int c = 0) {
    if (r == 9) return true;

    int nr = (c == 8) ? r + 1 : r;
    int nc = (c == 8) ? 0 : c + 1;

    vector<int> nums = { 1,2,3,4,5,6,7,8,9 };
    std::shuffle(nums.begin(), nums.end(), rng);

    auto canPlace = [&](int r, int c, int val) {
        for (int i = 0; i < 9; i++) {
            if (grid[r][i] == val) return false;
            if (grid[i][c] == val) return false;

            int br = 3 * (r / 3) + i / 3;
            int bc = 3 * (c / 3) + i % 3;
            if (grid[br][bc] == val) return false;
        }
        return true;
    };

    for (int val : nums) {
        if (canPlace(r, c, val)) {
            grid[r][c] = val;
            if (fillSudoku(grid, nr, nc)) return true;
            grid[r][c] = 0;
        }
    }
    return false;
}

// Remove <removeCount> random cells to make a puzzle
vector<vector<int>> generateSudokuPuzzle(int removeCount = 40) {
    vector<vector<int>> grid(9, vector<int>(9, 0));
    fillSudoku(grid); // generate a complete valid solution

    vector<pair<int, int>> pos;
    for (int r = 0; r < 9; r++)
        for (int c = 0; c < 9; c++)
            pos.emplace_back(r, c);

    std::shuffle(pos.begin(), pos.end(), rng);

    for (int k = 0; k < removeCount; k++)
        grid[pos[k].first][pos[k].second] = 0;

    return grid;
}

// ----------------- Sudoku-specific helpers -----------------

const int N = 9;
const int N2 = N * N;      // 81
const int COLS = 4 * N2;     // 324 columns

inline int boxIndex(int r, int c) {
    return (r / 3) * 3 + (c / 3);
}

// Fill an existing DLX with the full Sudoku exact-cover matrix
void buildSudokuDLX(DLX& dlx) {
    // For each possible (row, col, digit)
    for (int r = 0; r < N; r++) {
        for (int c = 0; c < N; c++) {
            for (int d = 0; d < N; d++) {
                // Four constraint columns
                int cell_idx = r * N + c;                       // cell
                int rowdig_idx = N2 + r * N + d;                  // row-digit
                int coldig_idx = 2 * N2 + c * N + d;              // col-digit
                int boxdig_idx = 3 * N2 + boxIndex(r, c) * N + d; // box-digit

                vector<int> colIndices = {
                    cell_idx,
                    rowdig_idx,
                    coldig_idx,
                    boxdig_idx
                };

                vector<Node*> rowNodes;
                rowNodes.reserve(4);

                int rowID = r * N2 + c * N + d; // (r,c,d) encoded 0–728
                for (int idx : colIndices) {
                    rowNodes.push_back(dlx.addNode(idx, rowID));
                }

                DLX::linkRow(rowNodes);
            }
        }
    }
}

// Force the given clues into the DLX structure
void applyInitialSudoku(DLX& dlx, const vector<vector<int>>& grid) {
    for (int r = 0; r < 9; r++) {
        for (int c = 0; c < 9; c++) {
            int d = grid[r][c];
            if (d == 0) continue; // skip empty

            int rowID = r * 81 + c * 9 + (d - 1);

            Node* rowNode = nullptr;
            for (auto* n : dlx.nodes) {
                if (n->rowID == rowID) {
                    rowNode = n;
                    break;
                }
            }

            if (!rowNode) {
                cerr << "ERROR: could not find rowID " << rowID
                    << " for given (" << r << "," << c << ")=" << d << endl;
                continue;
            }

            // record given as part of solution
            dlx.solution.push_back(rowID);

            // cover all columns touched by this row
            Node* cur = rowNode;
            do {
                DLX::cover(cur->C);
                cur = cur->R;
            } while (cur != rowNode);
        }
    }
}

bool solveSudoku(DLX& dlx) {
    return dlx.search();
}

// Convert solution rowIDs back into a 9x9 grid
vector<vector<int>> extractSolution(const vector<int>& solution) {
    vector<vector<int>> grid(9, vector<int>(9, 0));

    for (int rowID : solution) {
        int r = rowID / 81;
        int c = (rowID / 9) % 9;
        int d = (rowID % 9) + 1;
        grid[r][c] = d;
    }
    return grid;
}

// ----------------- IO and utility helpers -----------------

bool readSudokuFromStream(istream& in, vector<vector<int>>& grid) {
    grid.assign(9, vector<int>(9, 0));

    for (int r = 0; r < 9; ++r) {
        for (int c = 0; c < 9; ++c) {
            int x;
            if (!(in >> x)) {
                return false;
            }
            if (x < 0 || x > 9) {
                return false;
            }
            grid[r][c] = x;
        }
    }
    return true;
}


// Pretty-print with . and 3x3 box lines
void printSudokuPretty(const vector<vector<int>>& grid, const string& label) {
    cout << label << ":\n";
    for (int r = 0; r < 9; ++r) {
        if (r != 0 && r % 3 == 0)
            cout << "------+-------+------\n";
        for (int c = 0; c < 9; ++c) {
            if (c != 0 && c % 3 == 0)
                cout << "| ";
            int v = grid[r][c];
            if (v == 0) cout << ". ";
            else        cout << v << " ";
        }
        cout << '\n';
    }
}

// Check whether a completed grid is a valid Sudoku solution
bool checkSudoku(const vector<vector<int>>& grid) {
    // Check rows and columns
    for (int i = 0; i < 9; ++i) {
        vector<bool> row(10, false), col(10, false);
        for (int j = 0; j < 9; ++j) {
            int rv = grid[i][j];
            int cv = grid[j][i];
            if (rv < 1 || rv > 9 || cv < 1 || cv > 9) return false;
            if (row[rv] || col[cv]) return false;
            row[rv] = col[cv] = true;
        }
    }

    // Check 3x3 boxes
    for (int br = 0; br < 9; br += 3) {
        for (int bc = 0; bc < 9; bc += 3) {
            vector<bool> box(10, false);
            for (int r = 0; r < 3; ++r) {
                for (int c = 0; c < 3; ++c) {
                    int v = grid[br + r][bc + c];
                    if (v < 1 || v > 9) return false;
                    if (box[v]) return false;
                    box[v] = true;
                }
            }
        }
    }

    return true;
}

bool quit(const string& msg) {
    while (true) {
        cout << msg;
        string inp;
        if (!(cin >> inp)) {
            return true;
        }
        transform(inp.begin(), inp.end(), inp.begin(),
            [](unsigned char ch) { return std::tolower(ch); });

        if (inp == "y" || inp == "yes") return true;
        if (inp == "n" || inp == "no")  return false;

        cerr << "ERROR: Not valid input, enter 'y' or 'n'.\n";
    }
}

bool isInteger(const string& s, int& value) {
    try {
        size_t idx;
        value = std::stoi(s, &idx);
        return idx == s.size();
    }
    catch (...) {
        return false;
    }
}

int getChoice(const string& msg, const vector<int>& options) {
    while (true) {
        cout << msg;
        string inp;
        if (!(cin >> inp)) {
            cerr << "ERROR: Input failure.\n";
            return options.front();
        }

        int choice;
        if (isInteger(inp, choice) &&
            find(options.begin(), options.end(), choice) != options.end())
        {
            return choice;
        }

        cout << "ERROR: Not a valid choice. Valid options are: ";
        for (size_t i = 0; i < options.size(); ++i) {
            cout << options[i];
            if (i + 1 < options.size()) cout << " or ";
        }
        cout << ".\n";
    }
}

// ----------------- main -----------------

int main() {
    cout << "=====================================================\n"
        " Sudoku DLX Solver\n"
        "=====================================================\n";

    do {
        vector<vector<int>> grid;
        bool isValid = true;

        int choice = getChoice(
            "Choose input method:\n"
            "\t1) Enter puzzle manually\n"
            "\t2) Read puzzle from file\n"
            "\t3) Generate a random Sudoku puzzle\n"
            "Enter choice (1, 2, or 3): ",
            { 1, 2, 3 }
        );

        if (choice == 1) {
            cout << "Enter 9 lines, each with 9 numbers (0-9, 0 = empty):\n";
            if (!readSudokuFromStream(cin, grid)) {
                cerr << "ERROR: Failed to read Sudoku.\n";
                isValid = false;
            }

        }
        else if (choice == 2) {
            cout << "Enter filename: ";
            string filename;
            cin >> filename;

            ifstream fin(filename);
            if (!fin) {
                cerr << "ERROR: Could not open file.\n";
                isValid = false;
            }
            else if (!readSudokuFromStream(fin, grid)) {
                cerr << "ERROR: Failed to read Sudoku from file.\n";
                isValid = false;
            }

        }
        else {
            cout << "Generating puzzle...\n";
            grid = generateSudokuPuzzle(40);   // remove 40 cells ? medium difficulty
        }

        if (isValid) {
            cout << "\nInput puzzle:\n";
            printSudokuPretty(grid, "Puzzle");

            // Build fresh DLX structure for this puzzle
            DLX dlx(COLS);
            buildSudokuDLX(dlx);
            applyInitialSudoku(dlx, grid);

            if (solveSudoku(dlx)) {
                auto solvedGrid = extractSolution(dlx.solution);
                cout << "\nSolved Sudoku:\n";
                printSudokuPretty(solvedGrid, "Solution");

                if (checkSudoku(solvedGrid))
                    cout << "\nSolution is valid!\n";
                else
                    cout << "\nSolution is INVALID (something went wrong).\n";
            }
            else {
                cout << "\nNo solution found for this puzzle.\n";
            }
        }
    } while (!quit("Would you like to quit (y or n): "));
    return 0;
}
