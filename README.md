# Family Tree Viewer - Professional Edition

**Family Tree Viewer** is a high-performance, native Win32 desktop application designed to visualize complex family structures from simple CSV data. It prioritizes clarity, performance, and the ability to handle modern family dynamics—including multiple marriages, divorces, and complex lineage—without the bloat of web-based tools.

## 🎯 Main Business & Purpose

The primary goal of this project is to provide a **lightweight, privacy-focused, and robust visualization tool** for genealogy data. Unlike web apps that require uploading sensitive family data to the cloud, this application runs entirely locally, rendering instant visualizations from a text file.

### Key Capabilities
*   **Complex Relationship Modeling:** Uniquely designed to handle **multiple spouses** and **ex-spouses** gracefully.
*   **Automatic Layout Engine:** Uses a custom recursive algorithm to calculate the optimal position for every family member, ensuring trees are centered, balanced, and readable.
*   **Real-Time Data Reflection:** The application monitors the underlying data file. As soon as you save changes to your CSV, the visualization updates instantly (Hot-Reloading).

## ✨ Key Features

### 1. Advanced Relationship Visualization
*   **Multiple Spouses:** Displays spouses side-by-side with the main family member centered in between.
*   **Status Indicators:**
    *   **Current Spouses:** Connected via a **Solid Red** line.
    *   **Ex-Spouses:** Connected via a **Dashed Gray** line.
*   **Child Centering:** Children are intelligently sorted and positioned directly beneath their specific parents (e.g., children of a first marriage appear under the first spouse).

### 2. Professional Rendering
*   **Orthogonal Lines:** Uses "Manhattan-style" (right-angle) connectors for a clean, professional diagram look.
*   **Color Coding:**
    *   **Blue/White:** Male / Default
    *   **Pink:** Female
    *   **Yellow:** The user ("Me" / Focus)
*   **Interactive Legend:** A floating legend in the bottom-left corner explains all visual cues.

### 3. High Performance
*   **Native Win32 API:** Built directly on the Windows GDI (Graphics Device Interface) for maximum speed and zero dependencies on heavy frameworks like Electron or .NET.
*   **Double Buffering:** Implements a custom rendering pipeline to eliminate screen flickering during scrolling or resizing.

## 📂 Data Format (`family.csv`)

The application reads from a strictly formatted CSV file named `family.csv` located in the same directory.

### Column Structure
The CSV **must** contain the following 7 columns:
`ID, Name, Role, Gender, FatherID, MotherID, SpouseID`

### Handling Spouses
The `SpouseID` column supports special syntax to define complex relationships:

1.  **Multiple Spouses:** Separate IDs with a pipe `|`.
    *   *Example:* `12|15` (Spouses are ID 12 and ID 15).
2.  **Ex-Spouses:** Append an `x` or `X` to the ID.
    *   *Example:* `12x|15` (ID 12 is an ex-spouse, ID 15 is the current spouse).

### Example CSV
```csv
ID,Name,Role,Gender,FatherID,MotherID,SpouseID
1,John Doe,Father,Male,0,0,2x|3
2,Jane Doe,Ex-Wife,Female,0,0,1x
3,Mary Doe,Wife,Female,0,0,1
4,Jimmy Doe,Son (John & Jane),Male,1,2,0
5,Billy Doe,Son (John & Mary),Male,1,3,0
```

## 🛠️ Building & Running

This project is written in standard C++ using the Windows API. It can be compiled with GCC (MinGW) or MSVC.

### Requirements
*   Windows OS (7, 8, 10, 11)
*   C++ Compiler (g++ recommended)

### Compilation Command
```bash
g++ main.cpp -o FamilyTree.exe -lgdi32 -lcomctl32 -static
```

### How to Run
1.  Ensure `family.csv` is in the same folder as the executable.
2.  Run `FamilyTree.exe`.
3.  Edit `family.csv` in Excel or Notepad and save to see instant updates.

## 🏗️ Architecture

The codebase is refactored into modular components for maintainability:

*   **`DataModel`**: Handles parsing, storage, and relationship mapping (Graph structure).
*   **`LayoutEngine`**: A recursive algorithm that calculates `X, Y` coordinates, generation levels, and subtree widths to prevent overlapping.
*   **`Renderer`**: Abstracts GDI drawing commands, handling pens, brushes, fonts, and coordinate transformation.
*   **`FamilyTreeApp`**: Manages the application lifecycle, message loop, scrolling logic, and file watching.

---
*Built with C++ & Win32 API.*
