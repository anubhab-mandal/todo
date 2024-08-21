#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <limits.h>
#include <dirent.h>
#include <ctype.h>
#define MAX_ITEMS 100
#define MAX_LINE_LENGTH 256

void clearScreen() {
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}
void initializeOrLoadItems(char*** items, int** completionStatus, int* itemCount, bool* isFileOpened, char* filePath, int argc, char* argv[]);
void toggleItem(char** items, int* status, int* itemCount, bool* isFileOpened, char* filePath, int argc, char* argv[]);
int terminalWidth;
int getTerminalWidth() {
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    return w.ws_col;
}
void handleResize(int signal) {
    terminalWidth = getTerminalWidth();
}
void loadItemsFromFile(char* items[], int* status, int* itemCount, char* filePath) {
    FILE* file = fopen(filePath, "r");
    if (!file) {
        fprintf(stderr, "Error: File not found or could not be opened.\n");
        return;
    }
    *itemCount = 0;
    char line[MAX_LINE_LENGTH];
    while (fgets(line, MAX_LINE_LENGTH, file)) {
        line[strcspn(line, "\n")] = '\0'; 
        if (strlen(line) >= 4 && (strncmp(line, "[ ]", 3) == 0 || strncmp(line, "[X]", 3) == 0)) {
            status[*itemCount] = (line[1] == 'X');
            strcpy(items[*itemCount], line + 4);
        } else {
            status[*itemCount] = false;
            strcpy(items[*itemCount], line);
        }
        (*itemCount)++;
    }
    fclose(file);
    printf("File loaded successfully.\n");
}
void saveItemsToFile(char* items[], int* status, int itemCount, char* filePath) {
    FILE* file = fopen(filePath, "w");
    if (!file) {
        fprintf(stderr, "Error: Unable to create file in the specified directory.\n");
        return;
    }
    for (int i = 0; i < itemCount; i++) {
        fprintf(file, "%s %s\n", status[i] ? "[X]" : "[ ]", items[i]);
    }
    fclose(file);
    printf("File saved successfully as %s\n", filePath);
}
void autoSave(char* items[], int* status, int itemCount) {
    char homePath[PATH_MAX];
    if (getenv("HOME")) {
        strcpy(homePath, getenv("HOME"));
    } else {
        strcpy(homePath, ".");
    }
    strcat(homePath, "/.tmplist.txt");
    FILE* file = fopen(homePath, "w");
    if (file) {
        for (int i = 0; i < itemCount; i++) {
            fprintf(file, "%s %s\n", status[i] ? "[X]" : "[ ]", items[i]);
        }
        fclose(file);
    }
}
void displayProgressBar(int* status, int itemCount) {
    int completedCount = 0;
    for (int i = 0; i < itemCount; ++i) {
        if (status[i]) {
            completedCount++;
        }
    }
    double progress = (double)completedCount / itemCount;
    printf("\r\n\033[34m");
    int terminalWidth = getTerminalWidth();
    int filledWidth = (int)(progress * terminalWidth);
    for (int i = 0; i < terminalWidth; i++) {
        if (i < filledWidth) {
            printf("█");
        } else {
            printf(" ");
        }
    }
    printf("\033[0m\033[1m%d%%\033[0m\n", (int)(progress * 100));
    fflush(stdout);
}
void displayItems(char* items[], int* status, int itemCount) {
    for (int i = 0; i < itemCount; ++i) {
        if (status[i]) {
            printf("\033[32;9m%d. %s\033[0m\n", i + 1, items[i]);
        } else {
            printf("\033[1;31m%d. %s\033[0m\n", i + 1, items[i]);
        }
    }
}
void toggleItem(char** items, int* status, int* itemCount, bool* isFileOpened, char* filePath, int argc, char* argv[]) {
    char input[MAX_LINE_LENGTH];
    printf("\nEnter the indices of items to toggle completion (separated by spaces), 'add' to add new items, 'change' to reorder items, 'remove' to delete items, 'save' to save the list, 'new' to open a new list, or '0' to exit: ");
    fgets(input, MAX_LINE_LENGTH, stdin);
    input[strcspn(input, "\n")] = '\0';    if (strcmp(input, "0") == 0) {
        if (*isFileOpened) {
            char saveChoice;
            printf("You have an opened list. Do you want to save the current list before exiting? (Y/N): ");
            scanf("%c", &saveChoice);
            getchar(); 
            if (tolower(saveChoice) == 'y') {
                saveItemsToFile(items, status, *itemCount, filePath);
            }
        } else {
            autoSave(items, status, *itemCount);
        }
        exit(0);
    } else if (strcmp(input, "add") == 0) {
        printf("Enter items one by one (Enter 0 to finish):\n");
        while (*itemCount < MAX_ITEMS) {
            fgets(input, MAX_LINE_LENGTH, stdin);
            input[strcspn(input, "\n")] = '\0'; 
            if (strcmp(input, "0") == 0) {
                break;
            }
            strcpy(items[*itemCount], input);
            status[*itemCount] = false;
            (*itemCount)++;
        }
    } else if (strcmp(input, "change") == 0) {
        printf("Enter the changes to the items in the format 'index1>index2,index3>index4,...': ");
        fgets(input, MAX_LINE_LENGTH, stdin);
        input[strcspn(input, "\n")] = '\0'; 
        char* token;
        char* delim = ",";
        token = strtok(input, delim);
        while (token != NULL) {
            char* pair = token;
            char* indexStr1 = strtok(pair, ">");
            char* indexStr2 = strtok(NULL, ">");
            int index1 = atoi(indexStr1) - 1;
            int index2 = atoi(indexStr2) - 1;
            if (index1 >= 0 && index1 < *itemCount && index2 >= 0 && index2 < *itemCount) {
                char temp[MAX_LINE_LENGTH];
                strcpy(temp, items[index1]);
                strcpy(items[index1], items[index2]);
                strcpy(items[index2], temp);
                bool tempStatus = status[index1];
                status[index1] = status[index2];
                status[index2] = tempStatus;
            } else {
                printf("Invalid indices: %d or %d. Skipping.\n", index1 + 1, index2 + 1);
            }
            token = strtok(NULL, delim);
        }
    } else if (strcmp(input, "remove") == 0) {
        printf("Enter the indices of items to remove (separated by spaces): ");
        fgets(input, MAX_LINE_LENGTH, stdin);
        input[strcspn(input, "\n")] = '\0';
        char* token;
        char* delim = " ";
        token = strtok(input, delim);
        while (token != NULL) {
            int index = atoi(token) - 1;
            if (index >= 0 && index < *itemCount) {
                for (int i = index; i < *itemCount - 1; i++) {
                    strcpy(items[i], items[i + 1]);
                    status[i] = status[i + 1];
                }
                (*itemCount)--;
            } else {
                printf("Invalid index: %d. Skipping.\n", index + 1);
            }
            token = strtok(NULL, delim);
        }
    } else if (strcmp(input, "save") == 0) {
        if (*isFileOpened) {
            saveItemsToFile(items, status, *itemCount, filePath);
        } else {
            char directory[MAX_LINE_LENGTH];
            char fileName[MAX_LINE_LENGTH];
            printf("Enter the directory path to save the file: ");
            fgets(directory, MAX_LINE_LENGTH, stdin);
            directory[strcspn(directory, "\n")] = '\0'; 
            printf("Enter the file name: ");
            fgets(fileName, MAX_LINE_LENGTH, stdin);
            fileName[strcspn(fileName, "\n")] = '\0'; 
            snprintf(filePath, MAX_LINE_LENGTH, "%s/%s", directory, fileName);
            saveItemsToFile(items, status, *itemCount, filePath);
            *isFileOpened = true;
        }
    } else if (strcmp(input, "new") == 0) {
        if (*isFileOpened) {
            char saveChoice;
            printf("You have an opened list. Do you want to save the current list before starting a new one? (Y/N): ");
            scanf("%c", &saveChoice);
            getchar();
            if (tolower(saveChoice) == 'y') {
                saveItemsToFile(items, status, *itemCount, filePath);
            }
            clearScreen();
            initializeOrLoadItems(&items, &status, itemCount, isFileOpened, filePath, argc, argv);
            return;
        } else {
            clearScreen();
            initializeOrLoadItems(&items, &status, itemCount, isFileOpened, filePath, argc, argv);
            return;
        }
    } else {
        char* token;
        char* delim = " ";
        token = strtok(input, delim);
        while (token != NULL) {
            int index = atoi(token) - 1;
            if (index >= 0 && index < *itemCount) {
                status[index] = !status[index];
            } else {
                printf("Invalid index: %d. Skipping.\n", index + 1);
            }
            token = strtok(NULL, delim);
        }
    }
}
void initializeOrLoadItems(char*** items, int** completionStatus, int* itemCount, bool* isFileOpened, char* filePath, int argc, char* argv[]) {
    *itemCount = 0;
    *isFileOpened = false;
    for (int i = 0; i < MAX_ITEMS; ++i) {
        free((*items)[i]);
    }
    free(*items);
    free(*completionStatus);
    *items = malloc(MAX_ITEMS * sizeof(char*));
    *completionStatus = malloc(MAX_ITEMS * sizeof(int));
    for (int i = 0; i < MAX_ITEMS; ++i) {
        (*items)[i] = malloc(MAX_LINE_LENGTH * sizeof(char));
    }
    if (argc > 1) {
        loadItemsFromFile(*items, *completionStatus, itemCount, argv[1]);
        *isFileOpened = true;
        strcpy(filePath, argv[1]);
    } else {
        char homePath[PATH_MAX];
        if (getenv("HOME")) {
            strcpy(homePath, getenv("HOME"));
        } else {
            strcpy(homePath, ".");
        }
        strcat(homePath, "/.tmplist.txt");
        if (access(homePath, F_OK) == 0) {
            char loadTemp;
            printf("A temporary saved list was found. Do you want to load it? (Y/N): ");
            scanf("%c", &loadTemp);
            getchar(); // Consume the newline character
            if (tolower(loadTemp) == 'y') {
                loadItemsFromFile(*items, *completionStatus, itemCount, homePath);
                *isFileOpened = true;
                remove(homePath);
            }
        }
        if (*itemCount == 0) {
            char choice;
            printf("Load items from a file? (Y/N): ");
            scanf("%c", &choice);
            getchar(); // Consume the newline character
            if (tolower(choice) == 'y') {
                printf("Enter the file path: ");
                fgets(filePath, MAX_LINE_LENGTH, stdin);
                filePath[strcspn(filePath, "\n")] = '\0'; 
                loadItemsFromFile(*items, *completionStatus, itemCount, filePath);
                *isFileOpened = true;
            } else {
                printf("Enter items one by one (Enter 0 to finish):\n");
                while (*itemCount < MAX_ITEMS) {
                    fgets((*items)[*itemCount], MAX_LINE_LENGTH, stdin);
                    (*items)[*itemCount][strcspn((*items)[*itemCount], "\n")] = '\0'; 
                    if (strcmp((*items)[*itemCount], "0") == 0) {
                        break;
                    }
                    (*completionStatus)[*itemCount] = 0;
                    (*itemCount)++;
                }
            }
        }
    }
}
int main(int argc, char* argv[]) {
    char** items = malloc(MAX_ITEMS * sizeof(char*));
    int* completionStatus = malloc(MAX_ITEMS * sizeof(int));
    int itemCount = 0;
    bool isFileOpened = false;
    char filePath[MAX_LINE_LENGTH] = {0};
    initializeOrLoadItems(&items, &completionStatus, &itemCount, &isFileOpened, filePath, argc, argv);
    int terminalWidth = getTerminalWidth();
    signal(SIGWINCH, handleResize);
    while (true) {
        clearScreen();
        displayItems(items, completionStatus, itemCount);
        displayProgressBar(completionStatus, itemCount);
        if (ferror(stdin)) {
            clearerr(stdin);
        }
        toggleItem(items, completionStatus, &itemCount, &isFileOpened, filePath, argc, argv);
    }
    for (int i = 0; i < itemCount; ++i) {
        free(items[i]);
    }
    free(items);
    free(completionStatus);
    return 0;
}
