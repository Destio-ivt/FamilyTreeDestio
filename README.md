# Family Tree Viewer - Destio Project

## 👨‍💻 Developer Identity
*   **Name:** Destio Hardiansyah
*   **NIM:** 125103087
*   **Department:** Computer Science
*   **University:** Paramadina University

---

## 📱 Main Preview & Business Purpose
**Family Tree Viewer** is a native Windows desktop application designed to visualize family structures from simple CSV data. 

**Main Business:**
The primary goal is to provide a **privacy-focused, offline** genealogy tool. Unlike web apps that require uploading sensitive family data to the cloud, this application runs entirely locally on your machine, rendering instant visualizations from a simple text file. It transforms raw data into a clean, easy-to-read graphical tree.

**Key Features:**
*   **Instant Visualization:** Converts a standard CSV file into a graphical family tree in milliseconds.
*   **Automatic Layout Engine:** Uses a recursive algorithm to calculate optimal positions, ensuring the tree remains centered and balanced regardless of size.
*   **Hot-Reloading:** The app monitors `family.csv` in real-time. Save your changes in Excel/Notepad, and the tree updates instantly without restarting.
*   **Professional Rendering:** Uses "Manhattan-style" (orthogonal) lines and clear color coding (Pink for Female, Blue/White for Male, Yellow for "Myself").
*   **Robust Relationship Handling:** Capable of handling complex scenarios like multiple marriages and divorces as additional supported features.

---

## 🏗️ Project Architecture (File Structure)
Here is the explanation of the key files in this repository:

*   **`main.cpp`**: The heart of the application. It contains the C++ source code for the Data Model (CSV parsing), Layout Engine (Math & Geometry), Renderer (GDI Graphics), and the Windows GUI (Window management).
*   **`family.csv`**: The database. A simple text file containing all family members and their relationships.
*   **`pseudocode.txt`**: The "Blueprint". A high-level description of the logic and algorithms used to design the C++ code.
*   **`FamilyTreeDestio.cbp`**: The **Code::Blocks Project** file. This configuration file allows you to open the project directly in the Code::Blocks IDE.
*   **`README.md`**: This documentation file.

---

## 📂 Family Data Model (`family.csv`)
The application reads data from `family.csv`. This file acts as the database for the application. It uses a strict 7-column format. Here is the detailed explanation of each column:

1.  **`ID`** (Integer): A unique 5-digit identifier for each person (e.g., `00001`). This is the "Primary Key".
2.  **`Name`** (String): The full name of the family member (e.g., `Asmawi`).
3.  **`Role`** (String): The label displayed under the name to describe their position in the family (e.g., `Grandfather`, `Cousin`, `Myself`).
4.  **`Gender`** (String): Determines the color of the box. `Female` renders as Pink, `Male` renders as White/Blue.
5.  **`FatherID`** (Integer): The ID of this person's biological father. Use `0` if unknown.
6.  **`MotherID`** (Integer): The ID of this person's biological mother. Use `0` if unknown.
7.  **`SpouseID`** (String/Integer): Defines marital connections.
    *   **Single Spouse:** Just the ID (e.g., `15`).
    *   **Multiple Spouses (Feature):** Separated by `|` (e.g., `12|15`).
    *   **Ex-Spouses (Feature):** Marked with `x` (e.g., `12x`). This renders as a dotted line.

---

## 🧠 Algorithm Logic Approach

### 1. The Core Problem: Graph vs. Tree
Family trees are mathematically **Directed Acyclic Graphs (DAGs)**, not simple Hierarchical Trees. Since every child has two parents, naive traversal algorithms often encounter conflicts (infinite loops or duplicate branches).

### 2. Graph Processing: The "Cluster/Leader" Algorithm
The application implements a **"Single Owner" Policy** to linearize the graph. It groups complex relationships into distinct clusters using a priority-based leader selection.

#### Phase A: Leader Selection Matrix
The algorithm scans all "Root" individuals (Generation 0) and uses a comparison logic to decide who initiates a tree.

| Root Candidate | Spouse | Priority Check (`ID < SpouseID`) | Decision |
| :--- | :--- | :--- | :--- |
| **Asmawi (ID 1)** | Nasah (ID 11) | `1 < 11` (**True**) | 👑 **LEADER** (Starts Tree #1) |
| **Nasah (ID 11)** | Asmawi (ID 1) | `11 < 1` (**False**) | 🛑 **FOLLOWER** (Wait) |

#### Phase B: Memory Simulation (BFS Trace)
Once a Leader is elected, the system performs a **Breadth-First Search (BFS)** to "tag" every connected family member. This ensures they are exclusively owned by that Leader's cluster.

| Step | Processing ID | Event / Action | Queue State | Visited Memory | NodeOwner Map |
| :--- | :---: | :--- | :--- | :--- | :--- |
| **0** | **Init** | Asmawi (1) elected Leader. | `[1]` | `{1}` | `1 → 1` |
| **1** | **Asmawi (1)** | Found Spouse **Nasah (11)**. Not visited. **Claim.** | `[11]` | `{1, 11}` | `11 → 1` |
| **2** | **Asmawi (1)** | Found Child **Sugandi (104)**. Not visited. **Claim.** | `[11, 104]` | `{1, 11, 104}` | `104 → 1` |
| **3** | **Nasah (11)** | Found Spouse **Asmawi (1)**. Already Visited. **Skip.** | `[104]` | `{1, 11, 104}` | *(No Change)* |
| **4** | **Sugandi (104)**| Found Wife **Yunita (141)**. Not visited. **Claim.** | `[141]` | `{1, ..., 141}` | `141 → 1` |

#### Phase C: Conflict Resolution
When the loop naturally encounters **Nasah (ID 11)** again, the system checks the `Visited Memory`. Since she is already claimed, the system **ABORTS** creating a new tree for her. **Result:** No duplicates.

### 3. The Rendering Pipeline (From Data to Pixels)
Once the logical graph is solved, the application must draw it to the screen. The **Renderer** acts as a "dumb painter"—it does not calculate geometry; it simply reads the coordinates prepared by the **Layout Engine**.

| Stage | Component | Technical Action | Visual Result |
| :--- | :--- | :--- | :--- |
| **1. Calculation** | `LayoutEngine` | **Math Phase:** Recursively calculates width and assigns exact `(X, Y)` integers to every `Person` object in memory. | Data Model is updated with final coordinates. |
| **2. Data Access** | `Renderer` | **Read Phase:** The Renderer loops through the entire `DataModel`. It specifically looks for people where `x > -9000` (Valid). | Determines *who* is visible and ignores hidden nodes. |
| **3. Buffer** | `CreateCompatibleDC` | Creates an invisible **Virtual Canvas** in RAM (Back Buffer). | A blank, invisible image. |
| **4. Layer 1** | `GDI Pen` | **Painter's Algorithm:** Draws **Connectors** (Lines) first. <br>• Solid Red = Current Spouse<br>• Dotted Gray = Ex-Spouse | Lines appear "behind" where boxes will be. |
| **5. Layer 2** | `GDI Brush` | Draws **Nodes** (Boxes) *on top* of the lines using the `(X, Y)` values read in Step 2. <br>• **Pink:** Female<br>• **White:** Male | Boxes cover the line endpoints cleanly. |
| **6. Transform** | `SetWorldTransform` | Applies a matrix offset `(-ScrollX, -ScrollY)` to the entire image. | The tree moves when the user scrolls. |
| **7. Overlay** | `TextOutW` | Resets transform and draws the **Legend** and **Title** at fixed screen coordinates. | Legend stays "sticky" in the corner. |
| **8. Present** | `BitBlt` | Copies the Virtual Canvas to the Monitor in one atomic operation. | **The user sees the frame instantly.** |

---

## 🚀 How to Run (Code::Blocks)
This project is pre-configured for **Code::Blocks** with the MinGW compiler.

### Step 1: Get the Code
Open your terminal (Git Bash or CMD) and clone/pull the repository:
```bash
git pull origin main
```

### Step 2: Open Project
1.  Launch **Code::Blocks IDE**.
2.  Click **File > Open**.
3.  Select the file `FamilyTreeDestio.cbp` in this folder.

### Step 3: Build & Run
1.  Ensure "Debug" or "Release" target is selected in the toolbar.
2.  Press **F9** (or go to **Build > Build and Run**).
3.  The application window will open, displaying the family tree from `family.csv`.

*(Note: No external libraries are needed. The project uses standard Windows libraries `gdi32` and `comctl32` which are linked automatically in the .cbp file)*

---

## 📝 Summary
This project demonstrates the implementation of advanced Data Structures (Graphs/Trees) and Algorithms (DFS/BFS, Recursion, Heuristic Sorting) in a real-world application. It bridges the gap between theoretical Computer Science concepts taught at **Paramadina University** and practical Software Engineering, resulting in a high-performance tool that solves the complex visualization problem of modern family genealogy.