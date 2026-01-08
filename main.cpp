/**
 * Family Tree Viewer - Professional Edition
 *
 * A clean, Win32-based application to visualize family structures from CSV.
 * Supports multiple spouses, ex-spouses, and automatic layout optimization.
 */

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#define NOMINMAX
#include <windows.h>
#include <tchar.h>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <cmath>

// -----------------------------------------------------------------------------
// 1. CONFIGURATION
// -----------------------------------------------------------------------------
namespace Config {
    // Dimensions
    const int BOX_WIDTH     = 200;
    const int BOX_HEIGHT    = 75;
    const int V_GAP         = 150;  // Vertical distance between generations
    const int H_GAP         = 50;   // Horizontal gap between siblings
    const int SPOUSE_GAP    = 25;   // Gap between spouses
    const int TREE_GAP      = 0;   // Gap between separate family trees

    const wchar_t* DATA_FILE = L"family.csv";

    // Colors
    const COLORREF COL_BG_CANVAS     = RGB(250, 250, 252); // Very light gray/white
    const COLORREF COL_BOX_DEFAULT   = RGB(255, 255, 255);
    const COLORREF COL_BOX_FEMALE    = RGB(255, 245, 248);
    const COLORREF COL_BOX_FOCUS     = RGB(255, 252, 220); // Highlight
    const COLORREF COL_BOX_BORDER    = RGB(180, 180, 180);

    const COLORREF COL_TEXT_NAME     = RGB(30, 30, 30);
    const COLORREF COL_TEXT_ROLE     = RGB(100, 100, 100);

    // Connector Lines
    const COLORREF LINE_CHILD_NORMAL = RGB(180, 180, 180);
    const COLORREF LINE_SPOUSE_CURR  = RGB(220, 80, 80);   // Red/Pink for current
    const COLORREF LINE_SPOUSE_EX    = RGB(220, 80, 80); // Gray for ex
}

// -----------------------------------------------------------------------------
// 2. HELPERS
// -----------------------------------------------------------------------------

// RAII Wrapper for GDI Objects
template <typename T>
class ScopedGDI {
    T handle;
public:
    ScopedGDI(T h) : handle(h) {}
    ~ScopedGDI() { if (handle) DeleteObject(handle); }
    operator T() const { return handle; }
};

// RAII Wrapper for GDI Selection
class AutoSelect {
    HDC hdc;
    HGDIOBJ oldObj;
public:
    AutoSelect(HDC h, HGDIOBJ newObj) : hdc(h), oldObj(SelectObject(h, newObj)) {}
    ~AutoSelect() { SelectObject(hdc, oldObj); }
};

std::wstring ToWString(const std::string& str) {
    if (str.empty()) return L"";
    int size = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    std::wstring wstr(size, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstr[0], size);
    return wstr;
}

// -----------------------------------------------------------------------------
// 3. DATA MODEL
// -----------------------------------------------------------------------------
struct Person {
    int id = 0;
    std::wstring name;
    std::wstring role;
    std::wstring gender;
    int fatherId = 0;
    int motherId = 0;

    // Relationships
    std::vector<int> spouses;
    std::set<int> exSpouses; // IDs marked with 'x' in CSV

    // Visualization state
    int x = -10000;
    int y = -10000;
    int gen = 0;

    bool IsFemale() const { return gender == L"Female"; }
};

class DataModel {
public:
    std::vector<Person> people;
    std::map<int, size_t> idMap;

    void LoadFromFile(const wchar_t* filename) {
        people.clear();
        idMap.clear();

        char fNameMB[MAX_PATH];
        WideCharToMultiByte(CP_ACP, 0, filename, -1, fNameMB, MAX_PATH, NULL, NULL);

        std::ifstream file(fNameMB);
        if (!file.is_open()) return;

        // Skip BOM if present
        if (file.peek() == 0xEF) { file.get(); file.get(); file.get(); }

        std::string line;
        std::getline(file, line); // Skip Header

        while (std::getline(file, line)) {
            if (line.empty()) continue;
            if (line.back() == '\r') line.pop_back();

            std::stringstream ss(line);
            std::string segment;
            std::vector<std::string> parts;
            while (std::getline(ss, segment, ',')) parts.push_back(segment);

            if (parts.size() >= 7) {
                Person p;
                try {
                    p.id = std::stoi(parts[0]);
                    p.name = ToWString(parts[1]);
                    p.role = ToWString(parts[2]);
                    p.gender = ToWString(parts[3]);
                    p.fatherId = std::stoi(parts[4]);
                    p.motherId = std::stoi(parts[5]);

                    // Parse Spouses (e.g., "2x|3")
                    std::stringstream ssSpouse(parts[6]);
                    std::string sId;
                    while(std::getline(ssSpouse, sId, '|')) {
                        bool isEx = false;
                        if (!sId.empty() && (sId.back() == 'x' || sId.back() == 'X')) {
                            isEx = true;
                            sId.pop_back();
                        }
                        try {
                            int id = std::stoi(sId);
                            if (id != 0) {
                                p.spouses.push_back(id);
                                if (isEx) p.exSpouses.insert(id);
                            }
                        } catch(...) {}
                    }
                } catch(...) { continue; }
                people.push_back(p);
            }
        }

        for (size_t i = 0; i < people.size(); ++i) idMap[people[i].id] = i;
    }

    Person* Get(int id) {
        auto it = idMap.find(id);
        if (it != idMap.end()) return &people[it->second];
        return nullptr;
    }
};

// -----------------------------------------------------------------------------
// 4. LAYOUT ENGINE
// -----------------------------------------------------------------------------
class LayoutEngine {
    DataModel* model;
    std::map<int, std::pair<int, int>> subtreeMetrics; // <id, {width, centerOffset}>
    std::map<int, int> nodeOwner; // <person_id, root_id>

public:
    int totalWidth = 1000;
    int totalHeight = 1000;

    LayoutEngine(DataModel* m) : model(m) {}

    void Recalculate() {
        if (model->people.empty()) return;
        ResetState();
        CalculateGenerations();
        AssignOwnership();

        int currentX = 50;
        int currentY = 50;
        std::set<int> placed;

        // Sort roots to ensure consistent order? For now, insertion order (ID order typically)
        // We iterate standard vector order
        for (const auto& p : model->people) {
            // Process Gen 0 Roots
            if (p.gen == 0 && !placed.count(p.id)) {

                // If part of a marriage, only layout the "Canonical" root (smallest ID)
                int minId = p.id;
                for(int sid : p.spouses) minId = std::min(minId, sid);
                if (p.id > minId) continue;

                // Sanity: Do I own myself?
                if (nodeOwner[p.id] != p.id) continue;

                // Smart Gap Logic: Reduce gap if this tree relates to the previous one
                int gap = 0;
                if (!placed.empty()) {
                    gap = IsConnectedToPlaced(p.id, placed) ? Config::H_GAP : Config::TREE_GAP;
                }
                currentX += gap;

                ComputeSubtreeSize(p.id, p.id);
                PositionSubtree(p.id, currentX, currentY, placed, p.id);
                currentX += subtreeMetrics[p.id].first;
            }
        }

        FinalizeBounds();
    }

private:
    void ResetState() {
        for (auto& p : model->people) { p.x = p.y = -10000; p.gen = -1; }
        subtreeMetrics.clear();
        nodeOwner.clear();
    }

    // Determine generation levels relative to roots
    void CalculateGenerations() {
        bool changed = true;
        int limit = 0;
        while(changed && limit++ < 20) {
            changed = false;
            for(auto& p : model->people) {
                if (p.gen != -1) continue;

                Person* f = model->Get(p.fatherId);
                Person* m = model->Get(p.motherId);

                int pGen = -1;
                if(f && f->gen != -1) pGen = std::max(pGen, f->gen);
                if(m && m->gen != -1) pGen = std::max(pGen, m->gen);

                if (pGen != -1) {
                    p.gen = pGen + 1;
                    changed = true;
                } else {
                    // Try to inherit from spouse
                    for(int sid : p.spouses) {
                        Person* sp = model->Get(sid);
                        if (sp && sp->gen != -1) {
                            p.gen = sp->gen;
                            changed = true;
                            break;
                        }
                    }
                    // Roots
                    if (p.gen == -1 && p.fatherId == 0 && p.motherId == 0 && p.spouses.empty()) {
                        p.gen = 0; changed = true;
                    }
                }
            }
        }
        // Fallback
        for(auto& p : model->people) if (p.gen == -1) p.gen = 0;
    }

    // Assign every node to a 'Root Family' to prevent duplicates across trees
    void AssignOwnership() {
        std::set<int> visited;
        for (const auto& p : model->people) {
            if (p.gen == 0) {
                // Canonical root check
                int minId = p.id;
                for(int sid : p.spouses) minId = std::min(minId, sid);
                if (p.id > minId || visited.count(p.id)) continue;

                StartClaiming(p.id, visited);
            }
        }
    }

    void StartClaiming(int rootId, std::set<int>& visited) {
        std::vector<int> q = {rootId};
        visited.insert(rootId);
        nodeOwner[rootId] = rootId;

        size_t head = 0;
        while(head < q.size()) {
            int currId = q[head++];
            Person* curr = model->Get(currId);
            if (!curr) continue;

            // Claim Spouses
            for(int sid : curr->spouses) {
                if (!visited.count(sid)) {
                    visited.insert(sid);
                    nodeOwner[sid] = rootId;
                    q.push_back(sid);
                }
            }
            // Claim Children
            auto kids = GetChildren(currId);
            for(int kidId : kids) {
                if (!visited.count(kidId)) {
                    visited.insert(kidId);
                    nodeOwner[kidId] = rootId;
                    q.push_back(kidId);
                }
            }
        }
    }

    bool IsConnectedToPlaced(int rootId, const std::set<int>& placed) {
        for (const auto& p : model->people) {
            if (nodeOwner[p.id] == rootId) {
                if (placed.count(p.fatherId)) return true;
                if (placed.count(p.motherId)) return true;
                for (int sid : p.spouses) if (placed.count(sid)) return true;
                for (int kid : GetChildren(p.id)) if (placed.count(kid)) return true;
            }
        }
        return false;
    }

    // Returns sorted children for layout: [Left Spouse Kids] [Main Kids] [Right Spouse Kids]
    std::vector<int> GetChildren(int pid) {
        std::set<int> kidSet;
        Person* p = model->Get(pid);
        if(!p) return {};

        // Collect all kids involving this person
        auto addKids = [&](int parentId) {
            for(const auto& k : model->people)
                if(k.fatherId == parentId || k.motherId == parentId)
                    kidSet.insert(k.id);
        };
        addKids(pid);
        for(int sid : p->spouses) addKids(sid);

        std::vector<int> kids(kidSet.begin(), kidSet.end());

        // Sort for visual centering relative to parents
        int numSpouses = (int)p->spouses.size();
        int numLeft = numSpouses / 2;

        // Map SpouseID -> Index
        std::map<int, int> spouseIdx;
        for(int i=0; i<numSpouses; ++i) spouseIdx[p->spouses[i]] = i;

        std::sort(kids.begin(), kids.end(), [&](int a, int b) {
            auto GetKey = [&](int id) -> int {
                Person* k = model->Get(id);
                if(!k) return 999;

                int otherId = (k->fatherId == pid) ? k->motherId : k->fatherId;
                if (otherId == 0) return numLeft; // Center

                auto it = spouseIdx.find(otherId);
                if (it != spouseIdx.end()) {
                    // If left spouse, key < numLeft. If right, key > numLeft.
                    return (it->second < numLeft) ? it->second : (it->second + 1);
                }
                return numLeft;
            };

            int kA = GetKey(a);
            int kB = GetKey(b);
            if (kA != kB) return kA < kB;
            return a < b;
        });

        return kids;
    }

    std::pair<int,int> ComputeSubtreeSize(int pid, int rootId) {
        // If owned by another tree, it takes 0 space here
        if (nodeOwner.count(pid) && nodeOwner[pid] != rootId) return {0, 0};
        if (subtreeMetrics.count(pid)) return subtreeMetrics[pid];

        Person* p = model->Get(pid);
        int numSpouses = (int)p->spouses.size();

        // Width of Parents Cluster ( [Spouse] [Main] [Spouse] )
        int parentsW = Config::BOX_WIDTH;
        if (numSpouses > 0) {
            parentsW = (Config::BOX_WIDTH * (1 + numSpouses)) + (Config::SPOUSE_GAP * numSpouses);
        }

        // Width of Children
        auto kids = GetChildren(pid);
        int kidsW = 0;
        for(int k : kids) kidsW += ComputeSubtreeSize(k, rootId).first;
        if(!kids.empty()) kidsW += (int)((kids.size() - 1) * Config::H_GAP);

        int totalW = std::max(parentsW, kidsW);
        subtreeMetrics[pid] = { totalW, totalW/2 };
        return subtreeMetrics[pid];
    }

    void PositionSubtree(int pid, int x, int y, std::set<int>& placed, int rootId) {
        if (nodeOwner.count(pid) && nodeOwner[pid] != rootId) return;
        if (placed.count(pid)) return;
        placed.insert(pid);

        Person* p = model->Get(pid);
        for(int sid : p->spouses) placed.insert(sid);

        int centerOffset = subtreeMetrics[pid].second;
        int absoluteCenter = x + centerOffset;

        // 1. Position Parents Block
        int numSpouses = (int)p->spouses.size();
        int parentsW = Config::BOX_WIDTH;
        if (numSpouses > 0) parentsW = (Config::BOX_WIDTH * (1 + numSpouses)) + (Config::SPOUSE_GAP * numSpouses);

        int currentX = absoluteCenter - (parentsW/2);

        // A. Left Spouses
        int numLeft = numSpouses / 2;
        for(int i = 0; i < numLeft; ++i) {
            Person* sp = model->Get(p->spouses[i]);
            if(sp) {
                sp->x = currentX; sp->y = y;
                currentX += Config::BOX_WIDTH + Config::SPOUSE_GAP;
            }
        }
        // B. Main Person
        p->x = currentX; p->y = y;
        currentX += Config::BOX_WIDTH + Config::SPOUSE_GAP;

        // C. Right Spouses
        for(int i = numLeft; i < numSpouses; ++i) {
            Person* sp = model->Get(p->spouses[i]);
            if(sp) {
                sp->x = currentX; sp->y = y;
                currentX += Config::BOX_WIDTH + Config::SPOUSE_GAP;
            }
        }

        // 2. Position Children
        auto kids = GetChildren(pid);
        if(kids.empty()) return;

        int kidsTotalW = 0;
        for(int k : kids) kidsTotalW += subtreeMetrics[k].first;
        kidsTotalW += (int)(kids.size()-1) * Config::H_GAP;

        int childX = absoluteCenter - (kidsTotalW/2);
        for(int k : kids) {
            PositionSubtree(k, childX, y + Config::V_GAP, placed, rootId);
            childX += subtreeMetrics[k].first + Config::H_GAP;
        }
    }

    void FinalizeBounds() {
        int mx = 0, my = 0;
        for(const auto& p : model->people) {
            if(p.x > -9000) {
                mx = std::max(mx, p.x + Config::BOX_WIDTH);
                my = std::max(my, p.y + Config::BOX_HEIGHT);
            }
        }
        totalWidth = mx + 100;
        totalHeight = my + 100;
    }
};

// -----------------------------------------------------------------------------
// 5. RENDERER
// -----------------------------------------------------------------------------
class Renderer {
public:
    static void DrawTree(HDC hdc, DataModel* model, int totalWidth) {
        // Draw Header
        DrawHeader(hdc, totalWidth);

        // Draw Lines (Behind boxes)
        for (const auto& p : model->people) {
            if (p.x < -9000) continue;
            if (!p.spouses.empty()) DrawSpouseConnectors(hdc, &p, model);
            else DrawSingleParentChildren(hdc, &p, model);
        }

        // Draw Boxes (On top)
        SetBkMode(hdc, TRANSPARENT);
        for (const auto& p : model->people) {
            if (p.x > -9000) DrawBox(hdc, p);
        }
    }

    static void DrawLegend(HDC hdc, int clientH) {
        int x = 20;
        int h = 135;
        int w = 360;
        int y = clientH - h - 20;

        // Shadow
        RECT rcShadow = {x+4, y+4, x+w+4, y+h+4};
        {
            ScopedGDI<HBRUSH> sh(CreateSolidBrush(RGB(210, 210, 210)));
            FillRect(hdc, &rcShadow, sh);
        }

        // Background
        RECT rcBg = {x, y, x+w, y+h};
        {
            ScopedGDI<HBRUSH> bg(CreateSolidBrush(RGB(255, 255, 255)));
            FillRect(hdc, &rcBg, bg);
            ScopedGDI<HBRUSH> border(CreateSolidBrush(RGB(200, 200, 200)));
            FrameRect(hdc, &rcBg, border);
        }

        ScopedGDI<HFONT> font(CreateFont(17, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, ANTIALIASED_QUALITY, 0, L"Segoe UI"));
        ScopedGDI<HFONT> fontBold(CreateFont(19, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, ANTIALIASED_QUALITY, 0, L"Segoe UI"));

        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(40, 40, 40));

        int startY = y + 20;

        // Header
        {
            AutoSelect sel(hdc, fontBold);
            TextOutW(hdc, x+20, startY, L"Legend", 6);
        }

        int contentY = startY + 35;
        AutoSelect selFont(hdc, font);

        // --- Column 1: Boxes ---
        int col1X = x + 20;
        int currY = contentY;

        auto DrawLegendBox = [&](const wchar_t* txt, COLORREF col) {
            RECT rc = {col1X, currY, col1X+20, currY+16};
            ScopedGDI<HBRUSH> br(CreateSolidBrush(col));
            FillRect(hdc, &rc, br);
            ScopedGDI<HBRUSH> bd(CreateSolidBrush(Config::COL_BOX_BORDER));
            FrameRect(hdc, &rc, bd);
            TextOutW(hdc, col1X+30, currY-1, txt, lstrlenW(txt));
            currY += 26;
        };

        DrawLegendBox(L"Male", Config::COL_BOX_DEFAULT);
        DrawLegendBox(L"Female", Config::COL_BOX_FEMALE);
        DrawLegendBox(L"Myself", Config::COL_BOX_FOCUS);

        // --- Column 2: Lines ---
        int col2X = x + 150;
        currY = contentY; // Reset Y

        auto DrawLegendLine = [&](const wchar_t* txt, COLORREF col, int style, int width) {
            {
                ScopedGDI<HPEN> pen(CreatePen(style, width, col));
                AutoSelect selPen(hdc, pen);
                MoveToEx(hdc, col2X, currY+8, NULL);
                LineTo(hdc, col2X+25, currY+8);
            }
            TextOutW(hdc, col2X+35, currY-1, txt, lstrlenW(txt));
            currY += 26;
        };

        DrawLegendLine(L"Spouse", Config::LINE_SPOUSE_CURR, PS_SOLID, 2);
        DrawLegendLine(L"Ex-Spouse", Config::LINE_SPOUSE_EX, PS_DOT, 1);
        DrawLegendLine(L"Child", Config::LINE_CHILD_NORMAL, PS_SOLID, 2);
    }

private:
    static void DrawHeader(HDC hdc, int width) {
        ScopedGDI<HFONT> hFont(CreateFont(36, 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, ANTIALIASED_QUALITY, 0, L"Segoe UI"));
        AutoSelect sel(hdc, hFont);
        SetTextColor(hdc, RGB(60, 60, 60));
        SetBkMode(hdc, TRANSPARENT);

        RECT rc = {0, 10, width, 70};
        DrawTextW(hdc, L"My Family Tree", -1, &rc, DT_CENTER | DT_NOCLIP);
    }

    static void DrawSpouseConnectors(HDC hdc, const Person* p, DataModel* m) {
        int yC = p->y + Config::BOX_HEIGHT/2;
        int numSpouses = (int)p->spouses.size();

        for(int i=0; i < numSpouses; ++i) {
            int sid = p->spouses[i];
            Person* sp = m->Get(sid);
            if (!sp || sp->x < -9000) continue;

            // Line Style: Ex-Spouse (Dashed) vs Current (Solid)
            bool isEx = p->exSpouses.count(sid);
            COLORREF col = isEx ? Config::LINE_SPOUSE_EX : Config::LINE_SPOUSE_CURR;
            int style = isEx ? PS_DOT : PS_SOLID;
            int width = isEx ? 1 : 2;

            {
                ScopedGDI<HPEN> sPen(CreatePen(style, width, col));
                AutoSelect sel(hdc, sPen);

                // Horizontal connection
                if (sp->x > p->x) {
                    MoveToEx(hdc, p->x + Config::BOX_WIDTH, yC, NULL);
                    LineTo(hdc, sp->x, yC);
                } else {
                    MoveToEx(hdc, p->x, yC, NULL);
                    LineTo(hdc, sp->x + Config::BOX_WIDTH, yC);
                }
            }

            // Child Drop Lines
            DrawPairChildren(hdc, p, sp, m);
        }
    }

    static void DrawPairChildren(HDC hdc, const Person* p1, const Person* p2, DataModel* m) {
        std::vector<int> kids;
        for(const auto& k : m->people) {
            bool match = (k.fatherId == p1->id && k.motherId == p2->id) ||
                         (k.motherId == p1->id && k.fatherId == p2->id);
            if(match) kids.push_back(k.id);
        }

        if(!kids.empty()) {
            ScopedGDI<HPEN> kPen(CreatePen(PS_SOLID, 2, Config::LINE_CHILD_NORMAL));
            AutoSelect sel(hdc, kPen);

            int leftX = std::min(p1->x, p2->x);
            int midX = leftX + Config::BOX_WIDTH + (Config::SPOUSE_GAP/2);
            int yC = p1->y + Config::BOX_HEIGHT/2;

            MoveToEx(hdc, midX, yC, NULL);
            LineTo(hdc, midX, yC + 30); // Drop 30px

            for(int kidId : kids) {
                Person* k = m->Get(kidId);
                if(k && k->x > -9000) {
                    DrawOrthogonalLine(hdc, midX, yC + 30, k->x + Config::BOX_WIDTH/2, k->y);
                }
            }
        }
    }

    static void DrawSingleParentChildren(HDC hdc, const Person* p, DataModel* m) {
        ScopedGDI<HPEN> kPen(CreatePen(PS_SOLID, 2, Config::LINE_CHILD_NORMAL));
        AutoSelect sel(hdc, kPen);

        for(const auto& k : m->people) {
            // Check if this is the ONLY known parent (or other parent is unknown/unlisted)
            bool isP = (k.fatherId == p->id || k.motherId == p->id);
            if(!isP) continue;

            int otherId = (k.fatherId == p->id) ? k.motherId : k.fatherId;

            // If other parent is in the spouse list, it's handled by DrawPairChildren
            bool otherInSpouses = false;
            for(int sid : p->spouses) if(sid == otherId) otherInSpouses = true;

            if(!otherInSpouses && k.x > -9000) {
                int midX = p->x + Config::BOX_WIDTH/2;
                int yC = p->y + Config::BOX_HEIGHT/2;
                // Single parents usually drop lines from bottom center, or slightly lower?
                // Let's drop from center + 30 to match others
                DrawOrthogonalLine(hdc, midX, yC + 30, k.x + Config::BOX_WIDTH/2, k.y);
            }
        }
    }

    static void DrawOrthogonalLine(HDC hdc, int x1, int y1, int x2, int y2) {
        int midY = (y1 + y2) / 2;
        POINT pts[4] = { {x1, y1}, {x1, midY}, {x2, midY}, {x2, y2} };
        Polyline(hdc, pts, 4);
    }

    static void DrawBox(HDC hdc, const Person& p) {
        RECT rc = { p.x, p.y, p.x + Config::BOX_WIDTH, p.y + Config::BOX_HEIGHT };

        // 1. Shadow
        RECT rcShadow = rc; OffsetRect(&rcShadow, 4, 4);
        {
            ScopedGDI<HBRUSH> hShad(CreateSolidBrush(RGB(220, 220, 220)));
            FillRect(hdc, &rcShadow, hShad);
        }

        // 2. Background
        COLORREF bg = Config::COL_BOX_DEFAULT;
        if (p.name.find(L"Destio") != std::string::npos) bg = Config::COL_BOX_FOCUS;
        else if (p.IsFemale()) bg = Config::COL_BOX_FEMALE;

        {
            ScopedGDI<HBRUSH> hBg(CreateSolidBrush(bg));
            FillRect(hdc, &rc, hBg);
        }

        // 3. Border
        {
            ScopedGDI<HBRUSH> hBr(CreateSolidBrush(Config::COL_BOX_BORDER));
            FrameRect(hdc, &rc, hBr);
        }

        // 4. Text
        ScopedGDI<HFONT> fName(CreateFont(19, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, ANTIALIASED_QUALITY, 0, L"Segoe UI"));
        ScopedGDI<HFONT> fRole(CreateFont(15, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, ANTIALIASED_QUALITY, 0, L"Segoe UI"));

        SetBkMode(hdc, TRANSPARENT);

        // Name (Top half)
        RECT rcName = rc; rcName.bottom -= 20; rcName.top += 6;
        {
            AutoSelect sel(hdc, fName);
            SetTextColor(hdc, Config::COL_TEXT_NAME);
            DrawTextW(hdc, p.name.c_str(), -1, &rcName, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }

        // Role (Bottom half)
        RECT rcRole = rc; rcRole.top += 28;
        {
            AutoSelect sel(hdc, fRole);
            SetTextColor(hdc, Config::COL_TEXT_ROLE);
            DrawTextW(hdc, p.role.c_str(), -1, &rcRole, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
    }
};

// -----------------------------------------------------------------------------
// 6. APPLICATION WINDOW
// -----------------------------------------------------------------------------
class FamilyTreeApp {
    HWND hwnd = nullptr;
    DataModel data;
    LayoutEngine layout;
    FILETIME lastModTime = {0};
    int scrollX = 0, scrollY = 0;

public:
    FamilyTreeApp() : layout(&data) {}

    void Init(HWND h) {
        hwnd = h;
        ReloadData(true);
        SetTimer(hwnd, 1, 1000, NULL); // Auto-reload timer
    }

    void OnTimer() { ReloadData(false); }

    void OnPaint() {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        RECT rc;
        GetClientRect(hwnd, &rc);
        HDC hdcMem = CreateCompatibleDC(hdc);
        HBITMAP hbm = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
        HGDIOBJ oldBm = SelectObject(hdcMem, hbm);

        // Clear BG
        ScopedGDI<HBRUSH> hBg(CreateSolidBrush(Config::COL_BG_CANVAS));
        FillRect(hdcMem, &rc, hBg);

        // Draw Content
        SetGraphicsMode(hdcMem, GM_ADVANCED);
        XFORM xform = { 1.0f, 0, 0, 1.0f, (float)-scrollX, (float)-scrollY };
        SetWorldTransform(hdcMem, &xform);

        Renderer::DrawTree(hdcMem, &data, layout.totalWidth);

        // Reset for Overlay (Title, etc)
        ModifyWorldTransform(hdcMem, &xform, MWT_IDENTITY);

        Renderer::DrawLegend(hdcMem, rc.bottom);

        BitBlt(hdc, 0, 0, rc.right, rc.bottom, hdcMem, 0, 0, SRCCOPY);

        SelectObject(hdcMem, oldBm);
        DeleteObject(hbm);
        DeleteDC(hdcMem);
        EndPaint(hwnd, &ps);
    }

    void OnSize() { UpdateScrollBars(); }

    void OnScroll(int bar, WPARAM wParam) {
        int& pos = (bar == SB_HORZ) ? scrollX : scrollY;
        SCROLLINFO si = { sizeof(SCROLLINFO), SIF_ALL };
        GetScrollInfo(hwnd, bar, &si);

        switch(LOWORD(wParam)) {
            case SB_LINELEFT:   pos -= 10; break;
            case SB_LINERIGHT:  pos += 10; break;
            case SB_PAGELEFT:   pos -= si.nPage; break;
            case SB_PAGERIGHT:  pos += si.nPage; break;
            case SB_THUMBTRACK: pos = si.nTrackPos; break;
        }
        pos = std::max(0, std::min(pos, (int)(si.nMax - si.nPage)));

        si.nPos = pos; // Update the struct with the new position
        SetScrollInfo(hwnd, bar, &si, TRUE);
        InvalidateRect(hwnd, NULL, TRUE);
    }

private:
    void ReloadData(bool force) {
        WIN32_FILE_ATTRIBUTE_DATA attrib;
        if (GetFileAttributesExW(Config::DATA_FILE, GetFileExInfoStandard, &attrib)) {
            if (force || CompareFileTime(&lastModTime, &attrib.ftLastWriteTime) != 0) {
                lastModTime = attrib.ftLastWriteTime;
                data.LoadFromFile(Config::DATA_FILE);
                layout.Recalculate();
                UpdateScrollBars();
                InvalidateRect(hwnd, NULL, TRUE);

                if (force && data.people.empty()) {
                    MessageBoxA(hwnd, "Error: family.csv not found or empty.", "Family Tree", MB_ICONWARNING);
                }
            }
        }
    }

    void UpdateScrollBars() {
        RECT rc; GetClientRect(hwnd, &rc);
        SetScrollInfoHelper(SB_HORZ, scrollX, layout.totalWidth, rc.right);
        SetScrollInfoHelper(SB_VERT, scrollY, layout.totalHeight, rc.bottom);
    }

    void SetScrollInfoHelper(int bar, int pos, int maxVal, int page) {
        SCROLLINFO si = { sizeof(SCROLLINFO), SIF_ALL };
        si.nMin = 0; si.nMax = maxVal; si.nPage = page; si.nPos = pos;
        SetScrollInfo(hwnd, bar, &si, TRUE);
    }
};

static FamilyTreeApp g_App;

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch(msg) {
        case WM_CREATE: g_App.Init(hwnd); break;
        case WM_TIMER:  g_App.OnTimer(); break;
        case WM_PAINT:  g_App.OnPaint(); break;
        case WM_SIZE:   g_App.OnSize(); break;
        case WM_HSCROLL: g_App.OnScroll(SB_HORZ, wp); break;
        case WM_VSCROLL: g_App.OnScroll(SB_VERT, wp); break;
        case WM_MOUSEWHEEL: {
            int delta = GET_WHEEL_DELTA_WPARAM(wp);
            g_App.OnScroll(SB_VERT, (delta > 0) ? SB_LINEUP : SB_LINEDOWN);
            g_App.OnScroll(SB_VERT, (delta > 0) ? SB_LINEUP : SB_LINEDOWN);
            return 0;
        }
        case WM_DESTROY: PostQuitMessage(0); break;
        default: return DefWindowProc(hwnd, msg, wp, lp);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nCmdShow) {
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_HREDRAW | CS_VREDRAW, WndProc, 0, 0, hInst, LoadIcon(NULL, IDI_APPLICATION),
                      LoadCursor(NULL, IDC_ARROW), (HBRUSH)GetStockObject(WHITE_BRUSH), NULL, _T("FamilyTreeApp"), NULL };
    RegisterClassEx(&wc);
    HWND hwnd = CreateWindow(_T("FamilyTreeApp"), _T("Family Tree Viewer"), WS_OVERLAPPEDWINDOW | WS_VSCROLL | WS_HSCROLL,
                             CW_USEDEFAULT, CW_USEDEFAULT, 1200, 800, NULL, NULL, hInst, NULL);
    ShowWindow(hwnd, nCmdShow);
    MSG msg;
    while(GetMessage(&msg, NULL, 0, 0)) { TranslateMessage(&msg); DispatchMessage(&msg); }
    return (int)msg.wParam;
}
