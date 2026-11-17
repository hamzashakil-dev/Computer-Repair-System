#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define MAX 100
typedef struct {
    int id;
    char name[50];
    char phone[20];
    char device[50];
    char issue[100];
    int status;
    int month;
    float cost;
} Ticket;

typedef struct {
    int id;
    char name[50];
    int quantity;
    float price;
} Part;

int getInt(char prompt[], int min, int max);
float getFloat(char prompt[], float min);
void getText(char prompt[], char dest[], int size);
void flushLine(char line[]);
int adminLogin(void);
void userMenu(Ticket t[], int *tc, Part p[], int *pc);
void adminMenu(Ticket t[], int tc, Part p[], int pc);
void registerTicket(Ticket t[], int *tc);
void listTickets(Ticket t[], int tc);
void searchTicket(Ticket t[], int tc);
void sortByCost(Ticket t[], int tc);
void updateStatus(Ticket t[], int tc);
void addPart(Part p[], int *pc);
void listParts(Part p[], int pc);
void showStats(Ticket t[], int tc);
void showGraph(Ticket t[], int tc);
int countByStatus(Ticket t[], int tc, int index, int status);
char *statusName(int status);
void saveData(Ticket t[], int tc, Part p[], int pc);
void loadData(Ticket t[], int *tc, Part p[], int *pc);

int main(void) {
    Ticket *tickets = malloc(MAX * sizeof(Ticket));
    Part parts[MAX];
    int ticketCount = 0;
    int partCount = 0;
    int choice;

    if (tickets == NULL) {
        printf("Error: not enough memory.\n");
        return 1;
    }

    loadData(tickets, &ticketCount, parts, &partCount);

    do {
        printf("\n===== COMPUTER REPAIR CENTER =====\n");
        printf("1. User Mode\n");
        printf("2. Admin Mode\n");
        printf("0. Exit\n");
        choice = getInt("Select mode: ", 0, 2);

        switch (choice) {
            case 1:
                userMenu(tickets, &ticketCount, parts, &partCount);
                break;
            case 2:
                if (adminLogin()) {
                    adminMenu(tickets, ticketCount, parts, partCount);
                } else {
                    printf("Access denied! Wrong username or password.\n");
                }
                break;
            case 0:
                printf("Goodbye!\n");
                break;
        }
    } while (choice != 0);

    free(tickets);
    return 0;
}

void flushLine(char line[]) {
    int c;
    if (strchr(line, '\n') == NULL) {
        c = getchar();
        while (c != '\n' && c != EOF) {
            c = getchar();
        }
    }
}

int getInt(char prompt[], int min, int max) {
    char line[100];
    char extra;
    int value;

    while (1) {
        printf("%s", prompt);
        if (fgets(line, 100, stdin) == NULL) {
            exit(0);
        }
        flushLine(line);

        if (sscanf(line, "%d %c", &value, &extra) == 1 && value >= min && value <= max) {
            return value;
        }
        printf("Invalid input! Enter a whole number from %d to %d.\n", min, max);
    }
}

float getFloat(char prompt[], float min) {
    char line[100];
    char extra;
    float value;

    while (1) {
        printf("%s", prompt);
        if (fgets(line, 100, stdin) == NULL) {
            exit(0);
        }
        flushLine(line);
        if (sscanf(line, "%f %c", &value, &extra) == 1 && value >= min && value <= 1000000) {
            return value;
        }
        printf("Invalid input! Enter a number from %.0f to 1000000.\n", min);
    }
}

void getText(char prompt[], char dest[], int size) {
    char line[200];

    while (1) {
        printf("%s", prompt);
        if (fgets(line, 200, stdin) == NULL) {
            exit(0);
        }
        flushLine(line);
        line[strcspn(line, "\n")] = '\0';

        if (strlen(line) == 0) {
            printf("Invalid input! This cannot be empty.\n");
        } else if ((int)strlen(line) >= size) {
            printf("Invalid input! Maximum %d characters.\n", size - 1);
        } else {
            strcpy(dest, line);
            return;
        }
    }
}

int adminLogin(void) {
    char username[30];
    char password[30];

    printf("\n--- ADMIN LOGIN ---\n");
    getText("Username: ", username, 30);
    getText("Password: ", password, 30);

    if (strcmp(username, "admin567") == 0 && strcmp(password, "admin123") == 0) {
        return 1;
    }
    return 0;
}

void userMenu(Ticket t[], int *tc, Part p[], int *pc) {
    int choice;

    do {
        printf("\n--- USER MENU ---\n");
        printf("1. Register repair ticket\n");
        printf("2. List all tickets\n");
        printf("3. Search ticket by ID\n");
        printf("4. Sort tickets by cost\n");
        printf("5. Update ticket status\n");
        printf("6. Add spare part\n");
        printf("7. List spare parts\n");
        printf("0. Back\n");
        choice = getInt("Enter choice: ", 0, 7);

        switch (choice) {
            case 1: registerTicket(t, tc); saveData(t, *tc, p, *pc); break;
            case 2: listTickets(t, *tc); break;
            case 3: searchTicket(t, *tc); break;
            case 4: sortByCost(t, *tc); saveData(t, *tc, p, *pc); break;
            case 5: updateStatus(t, *tc); saveData(t, *tc, p, *pc); break;
            case 6: addPart(p, pc); saveData(t, *tc, p, *pc); break;
            case 7: listParts(p, *pc); break;
            case 0: break;
        }
    } while (choice != 0);
}

void adminMenu(Ticket t[], int tc, Part p[], int pc) {
    int choice;

    do {
        printf("\n--- ADMIN MENU ---\n");
        printf("1. View all tickets\n");
        printf("2. Statistics and revenue\n");
        printf("3. Monthly repair graph\n");
        printf("4. Inventory summary\n");
        printf("0. Logout\n");
        choice = getInt("Enter choice: ", 0, 4);

        switch (choice) {
            case 1: listTickets(t, tc); break;
            case 2: showStats(t, tc); break;
            case 3: showGraph(t, tc); break;
            case 4: listParts(p, pc); break;
            case 0: printf("Logged out.\n"); break;
        }
    } while (choice != 0);
}

void registerTicket(Ticket t[], int *tc) {
    Ticket n;
    int i, ok;

    if (*tc >= MAX) {
        printf("Ticket list is full (%d tickets).\n", MAX);
        return;
    }

    n.id = 101 + *tc;
    getText("Customer name: ", n.name, 50);
    do {
        ok = 1;
        getText("Phone number (digits only): ", n.phone, 20);
        for (i = 0; n.phone[i] != '\0'; i++) {
            if (n.phone[i] < '0' || n.phone[i] > '9') {
                ok = 0;
            }
        }
        if (ok == 0) {
            printf("Invalid input! Use digits only.\n");
        }
    } while (ok == 0);
    getText("Device (e.g. Laptop): ", n.device, 50);
    getText("Issue description: ", n.issue, 100);
    n.month = getInt("Month (1-12): ", 1, 12);
    n.cost = getFloat("Estimated cost: ", 0);
    n.status = 0;

    t[*tc] = n;
    (*tc)++;
    printf("Ticket registered! Ticket ID = %d\n", n.id);
}

void listTickets(Ticket t[], int tc) {
    int i;

    if (tc == 0) {
        printf("No tickets found.\n");
        return;
    }
    printf("\n--- ALL TICKETS ---\n");
    for (i = 0; i < tc; i++) {
        printf("ID: %d | %s | %s | Month: %d | %s | Cost: %.2f\n",
               t[i].id, t[i].name, t[i].device, t[i].month,
               statusName(t[i].status), t[i].cost);
    }
}

void searchTicket(Ticket t[], int tc) {
    int id, i;

    id = getInt("Enter ticket ID: ", 1, 100000);
    for (i = 0; i < tc; i++) {
        if (t[i].id == id) {
            printf("\n--- TICKET FOUND ---\n");
            printf("ID     : %d\n", t[i].id);
            printf("Name   : %s\n", t[i].name);
            printf("Phone  : %s\n", t[i].phone);
            printf("Device : %s\n", t[i].device);
            printf("Issue  : %s\n", t[i].issue);
            printf("Month  : %d\n", t[i].month);
            printf("Status : %s\n", statusName(t[i].status));
            printf("Cost   : %.2f\n", t[i].cost);
            return;
        }
    }
    printf("Ticket %d not found.\n", id);
}

void sortByCost(Ticket t[], int tc) {
    int i, j;
    Ticket temp;

    if (tc < 2) {
        printf("Not enough tickets to sort.\n");
        return;
    }
    for (i = 0; i < tc - 1; i++) {
        for (j = 0; j < tc - i - 1; j++) {
            if (t[j].cost > t[j + 1].cost) {
                temp = t[j];
                t[j] = t[j + 1];
                t[j + 1] = temp;
            }
        }
    }
    printf("Tickets sorted by cost (lowest first).\n");
    listTickets(t, tc);
}

void updateStatus(Ticket t[], int tc) {
    int id, i;

    id = getInt("Enter ticket ID: ", 1, 100000);
    for (i = 0; i < tc; i++) {
        if (t[i].id == id) {
            t[i].status = getInt("New status (0 Received, 1 Repairing, 2 Ready, 3 Collected): ", 0, 3);
            printf("Status updated to %s.\n", statusName(t[i].status));
            return;
        }
    }
    printf("Ticket %d not found.\n", id);
}

char *statusName(int status) {
    switch (status) {
        case 0: return "Received";
        case 1: return "Repairing";
        case 2: return "Ready";
        case 3: return "Collected";
    }
    return "Unknown";
}

void addPart(Part p[], int *pc) {
    Part n;
    int i;

    if (*pc >= MAX) {
        printf("Inventory is full (%d parts).\n", MAX);
        return;
    }
    n.id = *pc + 1;
    getText("Part name: ", n.name, 50);
    for (i = 0; i < *pc; i++) {
        if (strcmp(p[i].name, n.name) == 0) {
            printf("This part already exists. Duplicate not added.\n");
            return;
        }
    }
    n.quantity = getInt("Quantity: ", 0, 100000);
    n.price = getFloat("Unit price: ", 0);

    p[*pc] = n;
    (*pc)++;
    printf("Spare part added!\n");
}

void listParts(Part p[], int pc) {
    int i;

    if (pc == 0) {
        printf("Inventory is empty.\n");
        return;
    }
    printf("\n--- SPARE PARTS ---\n");
    for (i = 0; i < pc; i++) {
        printf("ID: %d | %s | Qty: %d | Price: %.2f\n",
               p[i].id, p[i].name, p[i].quantity, p[i].price);
    }
}

int countByStatus(Ticket t[], int tc, int index, int status) {
    if (index >= tc) {
        return 0;
    }
    if (t[index].status == status) {
        return 1 + countByStatus(t, tc, index + 1, status);
    }
    return countByStatus(t, tc, index + 1, status);
}

void showStats(Ticket t[], int tc) {
    int s, i;
    float revenue;

    printf("\n--- STATISTICS ---\n");
    printf("Total tickets: %d\n", tc);
    for (s = 0; s < 4; s++) {
        revenue = 0;
        for (i = 0; i < tc; i++) {
            if (t[i].status == s) {
                revenue += t[i].cost;
            }
        }
        printf("%-10s : %d tickets | Cost total = %.2f\n",
               statusName(s), countByStatus(t, tc, 0, s), revenue);
    }
    revenue = 0;
    for (i = 0; i < tc; i++) {
        if (t[i].status == 3) {
            revenue += t[i].cost;
        }
    }
    printf("Revenue (Collected tickets only) = %.2f\n", revenue);
}

void showGraph(Ticket t[], int tc) {
    char names[12][4] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                         "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    int counts[12] = {0};
    int i, j;

    for (i = 0; i < tc; i++) {
        counts[t[i].month - 1]++;
    }
    printf("\n--- MONTHLY REPAIRS ---\n");
    for (i = 0; i < 12; i++) {
        printf("%s (%2d) | ", names[i], counts[i]);
        for (j = 0; j < counts[i]; j++) {
            printf("*");
        }
        printf("\n");
    }
}

void saveData(Ticket t[], int tc, Part p[], int pc) {
    FILE *f;

    f = fopen("tickets.dat", "wb");
    if (f == NULL) {
        printf("Error: could not save tickets.\n");
        return;
    }
    fwrite(t, sizeof(Ticket), tc, f);
    fclose(f);

    f = fopen("parts.dat", "wb");
    if (f == NULL) {
        printf("Error: could not save parts.\n");
        return;
    }
    fwrite(p, sizeof(Part), pc, f);
    fclose(f);
}

void loadData(Ticket t[], int *tc, Part p[], int *pc) {
    FILE *f;
    int i;

    f = fopen("tickets.dat", "rb");
    if (f == NULL) {
        printf("No saved tickets found. Starting fresh.\n");
    } else {
        *tc = fread(t, sizeof(Ticket), MAX, f);
        fclose(f);
        for (i = 0; i < *tc; i++) {
            if (t[i].status < 0 || t[i].status > 3 || t[i].month < 1 || t[i].month > 12) {
                printf("tickets.dat is damaged. Starting fresh.\n");
                *tc = 0;
                break;
            }
        }
        printf("Loaded %d ticket(s).\n", *tc);
    }

    f = fopen("parts.dat", "rb");
    if (f == NULL) {
        printf("No saved parts found. Starting fresh.\n");
    } else {
        *pc = fread(p, sizeof(Part), MAX, f);
        fclose(f);
        for (i = 0; i < *pc; i++) {
            if (p[i].quantity < 0 || p[i].price < 0) {
                printf("parts.dat is damaged. Starting fresh.\n");
                *pc = 0;
                break;
            }
        }
        printf("Loaded %d part(s).\n", *pc);
    }
}
