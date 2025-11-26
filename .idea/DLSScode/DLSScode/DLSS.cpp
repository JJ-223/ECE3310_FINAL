#include <vector>
#include <string>
#include <iostream>
#include <stdexcept>

using namespace std;

struct Column;

struct Node {
    Node* L, * R, * U, * D;
    Column* C;
    int rowID;

    Node() : L(this), R(this), U(this), D(this), C(nullptr), rowID(-1) {}
};

struct Column : Node {
    int size;
    string name;

    Column(string n = "") : Node(), size(0), name(n) { C = this; }
};

class DLX {
public:
    Column head;
    vector<Column*> cols;
    vector<Node*> nodes;

    vector<int> solution;

    // Non-copyable, non-movable (must be constructed in place)
    DLX(const DLX&) = delete;
    DLX& operator=(const DLX&) = delete;
    DLX(DLX&&) = delete;
    DLX& operator=(DLX&&) = delete;

    DLX(int numCols) {
        head.L = head.R = &head;

        cols.reserve(numCols);
        for (int i = 0; i < numCols; i++) {
            Column* c = new Column(to_string(i));
            insertColumn(c);
            cols.push_back(c);
        }
    }

    ~DLX() {
        for (Node* n : nodes) delete n;
        for (Column* c : cols) delete c;
    }

private:
    void insertColumn(Column* c) {
        c->R = &head;
        c->L = head.L;
        head.L->R = c;
        head.L = c;
    }

public:
    Node* addNode(int colIndex, int rowID) {
        Column* c = cols[colIndex];
        Node* n = new Node();
        nodes.push_back(n);

        // vertical insert
        n->D = c;
        n->U = c->U;
        c->U->D = n;
        c->U = n;

        n->C = c;
        c->size++;
        n->rowID = rowID;

        return n;
    }

    void linkRow(const vector<Node*>& row) {
        int k = row.size();
        for (int i = 0; i < k; i++) {
            row[i]->R = row[(i + 1) % k];
            row[(i + 1) % k]->L = row[i];
        }
    }

    void cover(Column* c) {
        c->R->L = c->L;
        c->L->R = c->R;

        for (Node* i = c->D; i != c; i = i->D)
            for (Node* j = i->R; j != i; j = j->R) {
                j->D->U = j->U;
                j->U->D = j->D;
                j->C->size--;
            }
    }

    void uncover(Column* c) {
        for (Node* i = c->U; i != c; i = i->U)
            for (Node* j = i->L; j != i; j = j->L) {
                j->C->size++;
                j->D->U = j;
                j->U->D = j;
            }
        c->R->L = c;
        c->L->R = c;
    }

    bool search() {
        if (head.R == &head) return true;

        Column* c = nullptr;
        int minSize = 999999;

        for (Column* j = (Column*)head.R; j != &head; j = (Column*)j->R)
            if (j->size < minSize) {
                minSize = j->size;
                c = j;
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

const int N = 9;
const int N2 = N * N;        // 81
const int N3 = N * N * N;    // 729 rows
const int COLS = 4 * N2;     // 324 columns

inline int boxIndex(int r, int c) {
    return (r / 3) * 3 + (c / 3);
}

void buildSudokuDLX(DLX& dlx) {
    for (int r = 0; r < 9; r++){
        for (int c = 0; c < 9; c++){
            for (int d = 0; d < 9; d++) {

                int rowID = r * 81 + c * 9 + d;

                vector<int> colIndex = {
                    r * 9 + c,
                    81 + r * 9 + d,
                    162 + c * 9 + d,
                    243 + boxIndex(r,c) * 9 + d
                };

                vector<Node*> row;
                row.reserve(4);

                for (int idx : colIndex)
                    row.push_back(dlx.addNode(idx, rowID));

                dlx.linkRow(row);
            }
        }
    }
}

void applyInitialSudoku(DLX& dlx, const vector<vector<int>>& grid) {
    for (int r = 0; r < 9; r++) {
        for (int c = 0; c < 9; c++) {

            int d = grid[r][c];
            if (d == 0) continue;

            int rowID = r * 81 + c * 9 + (d - 1);

            Node* rowNode = nullptr;
            for (Node* n : dlx.nodes)
                if (n->rowID == rowID) { rowNode = n; break; }

            if (!rowNode)
                throw runtime_error("Bad rowID lookup");

            // --- Add this row to the solution, like Algorithm X ---
            dlx.solution.push_back(rowID);

            // --- Cover all columns in this row ---
            for (Node* cur = rowNode; ; cur = cur->R) {
                dlx.cover(cur->C);
                if (cur->R == rowNode) break;
            }
        }
    }
}

vector<vector<int>> extractSolution(const vector<int>& sol) {
    vector<vector<int>> grid(9, vector<int>(9, 0));

    for (int rowID : sol) {
        int r = rowID / 81;
        int c = (rowID / 9) % 9;
        int d = (rowID % 9);
        grid[r][c] = d + 1;
    }
    return grid;
}

int main() {
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

    DLX dlx(COLS);
    buildSudokuDLX(dlx);
    applyInitialSudoku(dlx, puzzle);

    if (dlx.search()) {
        auto out = extractSolution(dlx.solution);
        for (auto& row : out) {
            for (int v : row) cout << v << " ";
            cout << "\n";
        }
    }
    else {
        cout << "No solution exists\n";
    }
}

