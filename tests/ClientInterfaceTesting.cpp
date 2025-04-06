#include <iostream>
#include "../include/client/interface.hpp"

int main() {
    int choice;
    while (true) {
        clearScreen();
        showMenu();
        std::cin >> choice;
        std::cin.ignore(); // clear the newline from the buffer
        handleChoice(choice);

        std::cout << "Press Enter to continue...";
        std::cin.get();
    }
    return 0;
}
