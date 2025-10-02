# How to Compile and Run

## Prerequisites
- Install a C compiler such as `gcc`
- Open a terminal in the project folder `Product-Order-Manager`
- Ensure the file `products.csv` is in the same directory if the program needs product data

## Compile the Program
Use this command to compile all source files into a single executable
```bash
gcc main.c UnitTests.c E2E.c helpers.c -o ProductOrderManager
```
The command creates an executable named `ProductOrderManager` in the project directory

## Run the Program
```bash
./ProductOrderManager
```
On Windows run `ProductOrderManager.exe`
