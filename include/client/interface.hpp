#include <iostream>
#include <string>
#include <cstdlib>

void clearScreen() {
    // Works on most UNIX terminals; for Windows, you may use system("cls");
    std::system("clear");
}

void showMenu() {
    std::cout << "===== Welcome to FENRIS ====\n";
    std::cout << "=== What would you like to do on the server ? ===\n";
    std::cout << "1. Upload File\n";
    std::cout << "2. Delete File\n";
    std::cout << "3. Download File\n";
    std::cout << "4. Append File\n";
    std::cout << "5. Read File\n";
    std::cout << "6. File Info\n";
    std::cout << "7. Create Directory\n";
    std::cout << "8. List Directory\n";
    std::cout << "9. Delete Directory\n";
    std::cout << "10. Exit\n";
    std::cout << "Choose an option: ";
}

void handleChoice(int choice) {
    std::string InputFilename;
    std::string Content;
    switch (choice) {
        case 1: // for uploading a file
            std::cout << "Please enter the name of the file in the present working directory to be uploaded: ";
            std::cin >> InputFilename;
            std::cout << "Uploading file...\n";
            // Call your upload function here
            break;
        case 2: // for deleting a file
            std::cout << "Please enter the name of the file to be deleted: ";
            std::cin >> InputFilename;
            std::cout << "Deleting "<<InputFilename<<"...\n";
            // Call your delete function here
            break;
        case 3: // downloading a file
            std::cout << "Please enter the name of the file to be downloaded: ";
            std::cin >> InputFilename;
            std::cout << "Downloading "<<InputFilename<<" from the server...\n";
            // Call your download function here
            break;
        case 4: // for appending a file
            std::cout << "Please enter the name of the file to be appended: ";
            std::cin >> InputFilename;
            std::cout << "Please enter the content to be appended to "<<InputFilename<<":\n";
            std::cin.ignore(); // clear the newline from the buffer
            std::getline(std::cin, Content);
            std::cout << "Writing contents to "<<InputFilename<<"...\n";
            // Call your file appending function here
            break;
        case 5: // for reading a file
            std::cout << "Please enter the name of the file to be read: ";
            std::cin >> InputFilename;
            std::cout << "Reading "<<InputFilename<<"...\n";
            // Call your file reading function here
            break;
        case 6: // to get the Info about a file
            std::cout << "Please enter the name of the file to get info: ";
            std::cin >> InputFilename;
            std::cout << "Fetching info about "<<InputFilename<<"...\n";
            // Call your file info function here
            break;
        case 7: // creating a directory in the server
            std::cout << "Please enter the name of the directory to be created on the server: ";
            std::cin >> InputFilename;
            std::cout << "Creating directory "<<InputFilename<<" on the server...\n";
            // Call your create directory function here
            break;
        case 8: // listing the contents of a directory on the server
            std::cout << "Please enter the name of the directory to be listed: ";
            std::cin >> InputFilename;
            std::cout << "Listing contents of "<<InputFilename<<"...\n";
            // Call your list directory function here
            break;
        case 9: // deleting a directory on the server
            std::cout << "Please enter the name of the directory to be deleted: ";
            std::cin >> InputFilename;
            std::cout << "Deleting directory "<<InputFilename<<" on the server...\n";
            // Call your delete directory function here
            break;
        case 10: // exiting the program
            std::cout << "Exiting... \n";
            exit(0);
        default:
            std::cout << "Invalid option. Please try again.\n";
    }
}

