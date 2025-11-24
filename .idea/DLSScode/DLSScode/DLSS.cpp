#include <string>
#include <vector>
#include <iostream>
using namespace std;
struct Column;

struct Node {
    Node* L, * R, * U, * D;
    Column* C;     // Column this node belongs to
    int rowID;     // For backtracking Sudoku solution

    Node() : L(this), R(this), U(this), D(this), C(nullptr), rowID(-1) {}
};

struct Column : Node {
    int size;       // Number of nodes in this column
    string name;

    Column(const string& n = "") : Node(), size(0), name(n) { C = this; }
};

class DLX {
public:
    Column head;              // Sentinel header
    vector<Column*> cols;
    vector<Node*> nodes;

    vector<int> solution;

    DLX(int numCols) : head() {
        head.L = &head;
        head.R = &head;
        cols.reserve(numCols);
        for (int i = 0; i < numCols; i++) {
            Column* c = new Column(to_string(i));
            cols.push_back(c);
            insertColumn(c);
        }
    }

    ~DLX() {
        for (auto c : cols) delete c;
        for (auto n : nodes) delete n;
    }

private:
    void insertColumn(Column* c) {
        c->R = &head;       // new column points right to head
        c->L = head.L;      // left points to the last column
        head.L->R = c;      // last column's right points to new column
        head.L = c;          // head's left points to new column
    }

public:
    Node* addNode(int colIndex, int rowID) {
        Column* c = cols[colIndex];
        Node* n = new Node();
        nodes.push_back(n);

        // Vertical insert
        n->D = c;
        n->U = c->U;
        c->U->D = n;
        c->U = n;

        n->C = c;
        c->size++;
        n->rowID = rowID;
        return n;
    }

    void linkRow(const std::vector<Node*>& row) {
        for (int i = 0; i < (int)row.size(); i++) {
            Node* a = row[i];
            Node* b = row[(i + 1) % row.size()];

            a->R = b;
            b->L = a;
        }
    }

    void cover(Column* c) {
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

    void uncover(Column* c) {
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
        if (head.R == &head) return true; // success

        // Choose column with fewest nodes
        Column* c = nullptr;
        int minSize = 1e9;
        for (Column* j = (Column*)head.R; j != &head; j = (Column*)j->R) {
            if (j->size < minSize) {
                minSize = j->size;
                c = j;
            }
        }

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
// Assumes DLX class is already defined
const int N = 9;
const int N2 = N * N;       // 81
const int N3 = N * N * N;   // 729
const int COLS = 4 * N2;    // 324

// Helper: compute box index
inline int boxIndex(int r, int c) {
    return (r / 3) * 3 + (c / 3);
}

// Build the DLX matrix for Sudoku
DLX buildSudokuDLX() {
    DLX dlx(COLS);

    // For each possible (row, col, digit)
    for (int r = 0; r < N; r++) {
        for (int c = 0; c < N; c++) {
            for (int d = 0; d < N; d++) {
                vector<int> colIndices(4);
                colIndices[0] = r * N + c;                // cell constraint
                colIndices[1] = N2 + r * N + d;          // row constraint
                colIndices[2] = 2 * N2 + c * N + d;      // column constraint
                colIndices[3] = 3 * N2 + boxIndex(r, c) * N + d; // box constraint

                vector<Node*> rowNodes;
                int rowID = r * N2 + c * N + d; // unique row identifier
                for (int idx : colIndices)
                    rowNodes.push_back(dlx.addNode(idx, rowID));

                dlx.linkRow(rowNodes);
            }
        }
    }

    return dlx;
}


void applyInitialSudoku(DLX& dlx, const vector<vector<int>>& grid) {
    // Step 1: Validate the grid size
    if (grid.size() != 9) {
        throw std::invalid_argument("Grid must have 9 rows");
    }
    for (int r = 0; r < 9; ++r) {
        if (grid[r].size() != 9) {
            throw std::invalid_argument("Each row in the grid must have 9 columns");
        }
    }

    // Now proceed with applying the Sudoku numbers
    for (int r = 0; r < 9; r++) {
        for (int c = 0; c < 9; c++) {
            int d = grid[r][c];
            if (d == 0) continue; // skip empty

            if (d < 1 || d > 9) {
                throw std::invalid_argument("Grid values must be between 0 and 9");
            }

            int rowID = r * 81 + c * 9 + (d - 1); // unique rowID

            Node* rowNode = nullptr;
            for (auto n : dlx.nodes) {
                if (n->rowID == rowID) {
                    rowNode = n;
                    break;
                }
            }

            if (!rowNode) {
                std::cerr << "Warning: RowID not found: " << rowID << "\n";
                continue; // safety check
            }

            // Cover all columns in this row
            Node* cur = rowNode;
            do {
                dlx.cover(cur->C);
                cur = cur->R;
            } while (cur != rowNode);
        }
    }
}

bool solveSudoku(DLX& dlx) {
    return dlx.search(); // returns true if solution exists
}

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

int main() {
    using namespace std;

    vector<vector<int>> puzzle = {
        {5,3,0,0,7,0,0,0,0},
        {6,0,0,1,9,5,0,0,0},
        {0,9,8,0,0,0,0,6,0},
        {8,0,0,0,6,0,0,0,3},
        {4,0,0,8,0,3,0,0,1},
        {7,0,0,0,2,0,0,0,6},
        {0,6,0,0,0,0,2,8,0},
        {0,0,0,4,1,9,0,0,5},
        {0,0,0,0,8,0,0,7,9}
    };

    DLX dlx = buildSudokuDLX();
    int rowID = r * 81 + c * 9 + (d - 1); // unique rowID

    if (rowID < 0 || rowID >= 9 * 9 * 9) { // 9*9*9 = 729 possible rows
        throw std::out_of_range("Computed rowID is out of valid range: " + std::to_string(rowID));
    }
    if (dlx.nodes.empty()) {
        cerr << "Error: DLX nodes not populated!" << endl;
        return;
    }
    applyInitialSudoku(dlx, puzzle);

    if (solveSudoku(dlx)) {
        auto solutionGrid = extractSolution(dlx.solution);
        for (auto& row : solutionGrid) {
            for (int val : row) cout << val << " ";
            cout << endl;
        }
    }
    else {
        cout << "No solution exists!" << endl;
    }
}