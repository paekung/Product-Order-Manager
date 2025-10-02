# Product Order Manager

## Overview
Product Order Manager is a terminal-based inventory tool for tracking products, quantities, and pricing. Data is persisted to a CSV file so the catalog survives between runs. The interface is keyboard-driven, supports incremental search, and includes shortcuts to run automated tests that validate critical behaviours.

## Features
- Load and persist product data in `products.csv` with header `ProductID,ProductName,Quantity,UnitPrice`.
- Add products with unique identifiers; the form enforces non-empty names and non-negative inventory values.
- Update or remove existing products through an action menu; edits may step back (Ctrl+Z) or cancel (Ctrl+X).
- Real-time filtering: type any text to narrow the product list by ID or name, use arrows to navigate the matches, and view automatic pagination based on terminal height.
- Keyboard shortcuts: `Ctrl+N` add product, `Ctrl+T` run unit tests, `Ctrl+E` run end-to-end tests, `Ctrl+Q` exit.
- Automated coverage: unit tests stress core add/update helpers, while scripted end-to-end tests replay a full user journey and assert saved results.

## Requirements
- C99-compatible compiler (tested with `gcc`)
- POSIX-like terminal or Windows console (the program enables UTF-8 on Windows automatically)

## Build
```bash
gcc main.c UnitTests.c E2E.c helpers.c -o ProductOrderManager
```
On Windows replace the executable name with `ProductOrderManager.exe` if desired.

## Run
```bash
./ProductOrderManager
```
When the program starts it loads `products.csv` (creating it with a header if missing) and shows the main menu.

## Using the Application
- Use `↑`/`↓` to highlight entries. Press `Enter` to activate the highlighted action or product.
- Type any characters to filter products by ID or name; press `Backspace` to erase the filter.
- Select a product and press `Enter` to open the action menu. Choose update or remove. Removal requires a `y` confirmation.
- During add/update forms: `Ctrl+Z` steps back to the previous field, `Ctrl+X` aborts without changes. Empty product names or duplicate IDs are rejected.
- Press `Ctrl+N` at any point to jump directly to the add-product flow.
- Press `Ctrl+T` to run the unit test suite or `Ctrl+E` to replay the scripted end-to-end scenario. Results are printed inline and the original CSV content is restored afterwards.
- Exit with `Ctrl+Q` or by selecting the exit row.

## Data File
The default catalog resides in `products.csv`. Each line uses comma-separated values with the header shown above. The application rewrites the file after every successful add/update/remove so external changes should be avoided while the program is running.

## Tests
Both test suites are compiled into the executable:
- **Unit tests** (`run_unit_tests`) back up the current heap and CSV, then exercise edge cases for adding and updating products (duplicate IDs, capacity growth, boundary values, CSV persistence).
- **End-to-end test** (`run_e2e_tests`) injects scripted keyboard input to add, update, filter, and remove products, verifying the saved CSV and search results.

Launch the program and trigger the suites via shortcuts or by selecting the corresponding menu rows to validate behaviour after modifying the code.

## Repository Layout
- `main.c` – CLI entrypoint, menus, product CRUD operations.
- `helpers.c/h` – Terminal helpers for keyboard handling, screen control, and test hooks.
- `UnitTests.c` – Unit test harness and scenarios for add/update logic.
- `E2E.c` – Scripted end-to-end scenario support.
- `products.csv` – Sample catalog loaded at startup.
- `HowToCompile.md` – Quick compilation reference.
