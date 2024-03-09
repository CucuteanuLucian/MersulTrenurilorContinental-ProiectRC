#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <bits/waitflags.h>
#include <libxml/parser.h>
#include <sys/wait.h>
#include <ctype.h>
#include <sqlite3.h>

#define PORT 2024

#define error_handler(x) \
    {                    \
        perror(x);       \
        exit(0);         \
    }

extern int errno;

typedef struct thData
{
    int idThread; // id thread
    int cl;       // descriptor accept
} thData;

pthread_mutex_t mlock = PTHREAD_MUTEX_INITIALIZER;

void sendNotification(int client, char *idTren)
{
    char msgrasp[1000];
    char bufferClienti[4096];
    bzero(msgrasp, 1000);
    FILE *clientiInteresati;
    clientiInteresati = fopen("clientiInteresati.txt", "r");
    if (clientiInteresati == NULL)
    {
        error_handler("[server]Eroare! Nu s-a putut deschide fisierul text clientiInteresati.txt.\n");
        exit(0);
    }
    int vector[1024] = {0};

    while (fgets(bufferClienti, 4096, clientiInteresati) != NULL)
    {
        char *pUPDT = strtok(bufferClienti, " ");
        char clientID_din_Fisier[10];
        bzero(clientID_din_Fisier, 10);
        strcpy(clientID_din_Fisier, pUPDT);
        if (vector[atoi(clientID_din_Fisier)] == 1)
        {
            continue;
        }
        vector[atoi(clientID_din_Fisier)] = 1;
        char idT[10];
        bzero(idT, 10);
        pUPDT = strtok(NULL, "\n");
        strcpy(idT, pUPDT);
        if (strcmp(idT, idTren) == 0 && client != atoi(clientID_din_Fisier))
        {
            int clientIDupdate;
            clientIDupdate = atoi(clientID_din_Fisier);
            strcpy(msgrasp, "Trenul cu ID-ul: ");
            strcat(msgrasp, idTren);
            strcat(msgrasp, ", de care erati interesat, a fost modificat!\n");
            size_t marimeMesajRaspuns = strlen(msgrasp);
            if (write(clientIDupdate, &marimeMesajRaspuns, sizeof(size_t)) < 0)
            {
                error_handler("[server]Nu s-a putut trimite dimensiunea notificarii catre client.\n");
            }
            if (write(clientIDupdate, msgrasp, marimeMesajRaspuns) < 0)
            {
                error_handler("[server]Nu s-a putut trimite notificarea catre client.\n");
                continue;
            }
            bzero(msgrasp, 1000);
        }
    }
    fclose(clientiInteresati);
}

void timeDifference(char timp1[], char timp2[], char timeDiff[])
{
    int h1, h2, m1, m2;
    sscanf(timp1, "%d:%d", &h1, &m1);
    sscanf(timp2, "%d:%d", &h2, &m2);
    int totalTime1 = h1 * 60 + m1;
    int totalTime2 = h2 * 60 + m2;
    int dif = abs(totalTime1 - totalTime2);
    sprintf(timeDiff, "%d", dif);
}

void cript(char *str)
{
    for (int i = 0; str[i] != '\0'; i++)
        str[i] = str[i] + 1;
}
void decript(char *str)
{
    for (int i = 0; str[i] != '\0'; i++)
        str[i] = str[i] - 1;
}

void trimString(char *str)
{
    int inc = 0;
    int fin = strlen(str) - 1;
    while (str[inc] == ' ')
        inc++;
    while (str[fin] == ' ' && fin > inc)
        fin--;
    for (int i = inc; i <= fin; i++)
        str[i - inc] = str[i];
    str[fin - inc + 1] = '\0';
}

static void *treat(void *); 
void raspunde(void *);

int main()
{
    struct sockaddr_in server; 
    struct sockaddr_in from;
    int nr; 
    int sd; 
    int pid;
    pthread_t th[100]; 
    int i = 0;

    if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        error_handler("[server]Eroare la socket().\n");
        return errno;
    }
    int on = 1;
    setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

    bzero(&server, sizeof(server));
    bzero(&from, sizeof(from));

    server.sin_family = AF_INET;

    server.sin_addr.s_addr = htonl(INADDR_ANY);

    server.sin_port = htons(PORT);

    if (bind(sd, (struct sockaddr *)&server, sizeof(struct sockaddr)) == -1)
    {
        error_handler("[server]Eroare la bind().\n");
        return errno;
    }

    if (listen(sd, 5) == -1)
    {
        error_handler("[server]Eroare la listen().\n");
        return errno;
    }
    
    sqlite3 *db;
    sqlite3_stmt *stmt;
    int rc = sqlite3_open("dataBase.db", &db);

    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "[server]Nu putem deschide baza de date: %s\n", sqlite3_errmsg(db));
    }

    const char *tabelDateClienti = "CREATE TABLE IF NOT EXISTS clienti ("
                                   "id TEXT NOT NULL PRIMARY KEY,"
                                   "parola TEXT NOT NULL"
                                   ");";

    rc = sqlite3_exec(db, tabelDateClienti, 0, 0, 0);

    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "[server]Nu putem crea tabelul: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
    }
    const char *stergereClientiInteresati = "DROP TABLE IF EXISTS clientiInteresati;";

    rc = sqlite3_exec(db, stergereClientiInteresati, 0, 0, 0);

    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "[server]Nu putem sterge tabelul: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
    }

    sqlite3_close(db);
    xmlDoc *document;
    xmlNode *root, *node, *intarziereSosirePlecare, *intarzierePlecare, *intarziereSosireDestinatie, *oraPlecare, *oraSosire;
    char *filename;
    filename = "mersulTrenurilor.xml";
    document = xmlReadFile(filename, NULL, 0);
    root = xmlDocGetRootElement(document);
    for (node = root->children; node != NULL; node = node->next)
    {
        if (node->type == XML_ELEMENT_NODE)
        {
            intarziereSosirePlecare = xmlFirstElementChild(node);
            while (intarziereSosirePlecare != NULL && xmlStrcmp(intarziereSosirePlecare->name, (const xmlChar *)"intarziereSosirePlecare") != 0)
            {
                intarziereSosirePlecare = xmlNextElementSibling(intarziereSosirePlecare);
            }
            xmlNodeSetContent(intarziereSosirePlecare, (const xmlChar *)"0");

            intarzierePlecare = xmlFirstElementChild(node);
            while (intarzierePlecare != NULL && xmlStrcmp(intarzierePlecare->name, (const xmlChar *)"intarzierePlecare") != 0)
            {
                intarzierePlecare = xmlNextElementSibling(intarzierePlecare);
            }
            xmlNodeSetContent(intarzierePlecare, (const xmlChar *)"0");

            intarziereSosireDestinatie = xmlFirstElementChild(node);
            while (intarziereSosireDestinatie != NULL && xmlStrcmp(intarziereSosireDestinatie->name, (const xmlChar *)"intarziereSosireDestinatie") != 0)
            {
                intarziereSosireDestinatie = xmlNextElementSibling(intarziereSosireDestinatie);
            }
            xmlNodeSetContent(intarziereSosireDestinatie, (const xmlChar *)"0");

            if (xmlSaveFormatFile("mersulTrenurilor.xml", document, 1) == -1)
            {
                error_handler("Failed to save the document.\n");
            }
        }
    }
    xmlFreeDoc(document);
    xmlCleanupParser();
    FILE *clientiInteresati;
    clientiInteresati = fopen("clientiInteresati.txt", "w");
    if (clientiInteresati == NULL)
    {
        error_handler("[server]Nu s-a putut deschide fisierul text clientiInteresati.txt. (2)");
    }
    fclose(clientiInteresati);

    printf("[server]Asteptam la portul %d...\n", PORT);
    fflush(stdout);

    while (1)
    {
        pthread_mutex_init(&mlock, NULL);
        int client;
        thData *td; 
        int length = sizeof(from);

        
        pthread_mutex_lock(&mlock);
        if ((client = accept(sd, (struct sockaddr *)&from, &length)) < 0)
        {
            error_handler("[server]Eroare la accept().\n");
            continue;
        }
        pthread_mutex_unlock(&mlock);
        pthread_mutex_destroy(&mlock);

        td = (struct thData *)malloc(sizeof(struct thData));
        td->idThread = i++;
        td->cl = client;

        pthread_create(&th[i], NULL, &treat, td);

    } 
};

static void *treat(void *arg)
{
    struct thData tdL;
    tdL = *((struct thData *)arg);
    printf("[thread]- %d - Asteptam mesajul...\n", tdL.idThread);
    fflush(stdout);
    pthread_detach(pthread_self());
    raspunde((struct thData *)arg);
    close((intptr_t)arg);
    return (NULL);
};

void raspunde(void *arg)
{
    sqlite3 *db;
    sqlite3_stmt *stmt;
    xmlDoc *document;
    xmlNode *root, *node, *intarziereSosirePlecare, *intarzierePlecare, *intarziereSosireDestinatie, *oraPlecare, *oraSosire;
    pthread_mutex_init(&mlock, NULL);
    int rc = sqlite3_open("dataBase.db", &db);

    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "[server]Nu putem deschide baza de date: %s\n", sqlite3_errmsg(db));
    }
    char msgrasp[1000];
    bzero(msgrasp, 1000);
    struct thData tdL = *((struct thData *)arg);
    int esteLogat = 0;
    while (1)
    {
        size_t marimeMsgPrimit;
        int rez = read(tdL.cl, &marimeMsgPrimit, sizeof(size_t));
        if (rez == 0)
        {
            printf("[server]Clientul s-a deconectat fortat! (fara comanda /quit)\n");
            break;
        }
        if (rez == -1)
        {
            error_handler("[server]Nu putem citi marimea mesajului de la client.\n");
        }
        char *msgPrimit = (char *)malloc(marimeMsgPrimit);
        bzero(msgPrimit, marimeMsgPrimit);
        if (read(tdL.cl, msgPrimit, marimeMsgPrimit) < 0)
        {
            error_handler("[server]Nu putem citi mesajul de la client.\n");
            continue;
        }
        int nrParametri = 0;
        for (int count = 0; msgPrimit[count] != '\0'; count++)
            if (msgPrimit[count] == ' ')
                nrParametri++;
        memset(msgrasp, 0, sizeof(msgrasp));
        char comanda[100];
        bzero(comanda, 100);
        char msgCopie[marimeMsgPrimit];
        bzero(msgCopie, marimeMsgPrimit);
        char plecare[100];
        bzero(plecare, 100);
        char destinatie[100];
        bzero(destinatie, 100);

        strcpy(msgCopie, msgPrimit);
        int k;
        for (k = 0; msgCopie[k] != '\0'; k++)
            if (msgCopie[k] == '\n')
                break;
        msgCopie[k] = msgCopie[strlen(msgCopie)];
        char *p = strtok(msgCopie, " ");
        strcpy(comanda, p);
        int esteComadaValida = 0;
        if (strcmp(comanda, "/update") == 0 || strcmp(comanda, "/getInfo") == 0 || strcmp(comanda, "/help") == 0 || strcmp(comanda, "/register") == 0 || strcmp(comanda, "/login") == 0 || strcmp(comanda, "/logout") == 0 || strcmp(comanda, "/quit") == 0)
            esteComadaValida = 1;
        if (esteComadaValida == 0)
            strcpy(msgrasp, "Comanda Invalida!");
        else
        {
            if (strcmp(comanda, "/logout") == 0)
            {
                if (esteLogat == 0)
                {
                    strcpy(msgrasp, "Nu esti conectat!");
                }
                else if (esteLogat == 1)
                {
                    esteLogat = 0;
                    strcpy(msgrasp, "Te-ai delogat..");
                }
            }
            else if (strcmp(comanda, "/quit") == 0)
            {
                strcpy(msgrasp, "quit");
            }
            else if (strcmp(comanda, "/register") == 0)
            {
                if (esteLogat == 0)
                {
                    if (nrParametri == 2)
                    {
                        char idReg[100];
                        bzero(idReg, 100);
                        char parolaReg[100];
                        bzero(parolaReg, 100);
                        p = strtok(NULL, " ");
                        strcpy(idReg, p);
                        p = strtok(NULL, " ");
                        strcpy(parolaReg, p);
                        char registerIns[1000];
                        bzero(registerIns, 1000);
                        strcat(registerIns, "INSERT INTO clienti (id, parola) VALUES ('");
                        strcat(registerIns, idReg);
                        strcat(registerIns, "', '");
                        cript(parolaReg);
                        strcat(registerIns, parolaReg);
                        strcat(registerIns, "');");
                        rc = sqlite3_exec(db, registerIns, 0, 0, 0);

                        if (rc != SQLITE_OK)
                        {
                            strcpy(msgrasp, "Acest ID este deja Inregistrat!");
                            fprintf(stderr, "[server] Nu pot introduce date cu register: %s\n", sqlite3_errmsg(db));
                        }
                        else
                            strcpy(msgrasp, "Te-ai inregistrat cu succes!");
                    }
                    else if (nrParametri < 2)
                    {
                        strcpy(msgrasp, "Ai introdus prea putine date!");
                    }
                    else if (nrParametri > 2)
                    {
                        strcpy(msgrasp, "Ai introdus prea multe date!");
                    }
                }
                else if (esteLogat == 1)
                    strcpy(msgrasp, "Nu te poti inregistra in timp ce esti logat!");
            }
            else if (strcmp(comanda, "/help") == 0)
            {
                strcpy(msgrasp, "Lista Comenzilor:\n");
                strcat(msgrasp, "-/help -> pentru instructiuni comenzi;\n");
                strcat(msgrasp, "-/register id parola -> pentru inregistrarea contului in baza de date;\n");
                strcat(msgrasp, "-/login id parola -> pentru logarea in cont;\n");
                strcat(msgrasp, "-/logout -> pentru delogare din cont;\n");
                strcat(msgrasp, "-/quit -> pentru iesirea din aplicatie;\n");
                strcat(msgrasp, "-/getInfo plecare destinatie -> pentru a primi detalii legate de trenurile pe acea ruta;\n");
                strcat(msgrasp, "-/update idTren statusTren ora -> pentru a actualiza detaliile traseelor feroviare;\n");
            }
            else if (strcmp(comanda, "/login") == 0)
            {
                if (esteLogat == 1)
                {
                    strcpy(msgrasp, "Esti deja logat!");
                }
                else if (esteLogat == 0)
                {
                    if (nrParametri < 2)
                    {
                        strcpy(msgrasp, "Ai introdus prea putine date!");
                    }
                    else if (nrParametri > 2)
                    {
                        strcpy(msgrasp, "Ai introdus prea multe date!");
                    }
                    else if (nrParametri == 2)
                    {
                        int idValid = 0;
                        int parolaValida = 0;
                        char idLog[100];
                        bzero(idLog, 100);
                        char parolaLog[100];
                        bzero(parolaLog, 100);
                        p = strtok(NULL, " ");
                        strcpy(idLog, p);
                        p = strtok(NULL, " ");
                        strcpy(parolaLog, p);
                        char sqlCom[100];
                        bzero(sqlCom, 100);
                        strcpy(sqlCom, "SELECT id FROM clienti WHERE id='");
                        strcat(sqlCom, idLog);
                        strcat(sqlCom, "';");
                        rc = sqlite3_prepare_v2(db, sqlCom, -1, &stmt, NULL);
                        if (rc != SQLITE_OK)
                        {
                            error_handler("[server]Eroare la pregătirea declarației SQL.\n");
                        }
                        rc = sqlite3_step(stmt);
                        if (rc == SQLITE_ROW)
                        {
                            const char *idBazaDate = (const char *)sqlite3_column_text(stmt, 0);
                            if (strcmp(idBazaDate, idLog) == 0)
                                idValid = 1;
                        }
                        else
                            error_handler("[server]Eroare la executarea interogării.\n");

                        bzero(sqlCom, 100);
                        strcpy(sqlCom, "SELECT parola FROM clienti WHERE id='");
                        strcat(sqlCom, idLog);
                        strcat(sqlCom, "';");
                        rc = sqlite3_prepare_v2(db, sqlCom, -1, &stmt, NULL);
                        if (rc != SQLITE_OK)
                        {
                            error_handler("[server]Eroare la pregătirea declarației SQL.\n");
                        }
                        rc = sqlite3_step(stmt);
                        if (rc == SQLITE_ROW)
                        {
                            char *parolaBazaDate = (char *)sqlite3_column_text(stmt, 0);
                            decript(parolaBazaDate);
                            if (strcmp(parolaBazaDate, parolaLog) == 0)
                                parolaValida = 1;
                        }
                        if (idValid == 0)
                            strcpy(msgrasp, "ID-ul introdus este invalid..");
                        if (parolaValida == 0 && idValid == 1)
                            strcpy(msgrasp, "Parola introdusa este invalida..");
                        if (parolaValida == 1 && idValid == 1)
                        {
                            strcpy(msgrasp, "Te-ai conectat cu succes!");
                            esteLogat = 1;
                        }
                        sqlite3_finalize(stmt);
                    }
                }
            }
            else if (strcmp(comanda, "/getInfo") == 0)
            {
                if (nrParametri == 2)
                {
                    p = strtok(NULL, " ");
                    strcpy(plecare, p);
                    p = strtok(NULL, " ");
                    strcpy(destinatie, p);
                    xmlDoc *document;
                    xmlNode *root, *node;
                    char *filename;
                    filename = "mersulTrenurilor.xml";
                    document = xmlReadFile(filename, NULL, 0);
                    root = xmlDocGetRootElement(document);
                    char trenInfo[1000];
                    int numarTrenuriGasite = 0;
                    for (node = root->children; node != NULL; node = node->next)
                    {
                        if (node->type == XML_ELEMENT_NODE)
                        {
                            bzero(trenInfo, 1000);
                            strcpy(trenInfo, xmlNodeGetContent(node));
                            char plecareXml[50];
                            char destinatieXml[50];
                            bzero(plecareXml, 50);
                            bzero(destinatieXml, 50);
                            p = strtok(trenInfo, "\n");
                            trimString(p);
                            strcpy(plecareXml, p);
                            p = strtok(NULL, "\n");
                            trimString(p);
                            strcpy(destinatieXml, p);
                            if (strcmp(plecare, plecareXml) == 0 && strcmp(destinatie, destinatieXml) == 0)
                            {
                                numarTrenuriGasite++;
                                bzero(trenInfo, 1000);
                                strcpy(trenInfo, xmlNodeGetContent(node));
                                if (numarTrenuriGasite > 0)
                                    strcat(msgrasp, "\n");
                                char durataXml[50];
                                char oraPlecareXml[50];
                                char oraSosireXml[50];
                                char intarziereSP[50];
                                char intarziereSD[50];
                                char intarziereIP[50];
                                bzero(oraPlecareXml, 50);
                                bzero(oraSosireXml, 50);
                                bzero(durataXml, 50);
                                bzero(intarziereSP, 50);
                                bzero(intarziereSD, 50);
                                bzero(intarziereIP, 50);
                                p = strtok(trenInfo, "\n"); // plecare
                                p = strtok(NULL, "\n");     // destinatie
                                p = strtok(NULL, "\n");     // ora plecare
                                strcpy(oraPlecareXml, p);
                                trimString(oraPlecareXml);
                                p = strtok(NULL, "\n"); // ora sosire
                                strcpy(oraSosireXml, p);
                                trimString(oraSosireXml);
                                p = strtok(NULL, "\n"); // durata
                                strcpy(durataXml, p);
                                trimString(durataXml);
                                p = strtok(NULL, "\n"); // intarziere sosire plecare
                                strcpy(intarziereSP, p);
                                trimString(intarziereSP);
                                p = strtok(NULL, "\n"); // intarziere plecare
                                strcpy(intarziereIP, p);
                                trimString(intarziereIP);
                                p = strtok(NULL, "\n"); // intarziere sosire destinatie
                                strcpy(intarziereSD, p);
                                trimString(intarziereSD);

                                strcat(msgrasp, "Trenul pleaca din: ");
                                strcat(msgrasp, plecareXml);
                                strcat(msgrasp, ". Merge spre: ");
                                strcat(msgrasp, destinatieXml);
                                strcat(msgrasp, ".\nOra plecare: ");
                                strcat(msgrasp, oraPlecareXml);
                                if (strcmp(intarziereSP, "0") != 0)
                                {
                                    strcat(msgrasp, " (Nu a ajuns in statie. Ajunge in: ");
                                    strcat(msgrasp, intarziereSP);
                                    strcat(msgrasp, " minute)");
                                }
                                else
                                {
                                    if (strcmp(intarziereIP, "0") != 0)
                                    {
                                        strcat(msgrasp, " (Trenul este in statie si pleaca cu ");
                                        strcat(msgrasp, intarziereIP);
                                        strcat(msgrasp, " minute intarziere)");
                                    }
                                    else
                                    {
                                        strcat(msgrasp, " (fara intarziere)");
                                    }
                                }
                                strcat(msgrasp, "\nOra sosire: ");
                                strcat(msgrasp, oraSosireXml);
                                if (strcmp(intarziereSD, "0") != 0)
                                {
                                    strcat(msgrasp, " (Trenul ajunge la destinatie cu ");
                                    strcat(msgrasp, intarziereSD);
                                    strcat(msgrasp, " minute mai tarziu)");
                                }
                                else
                                {
                                    strcat(msgrasp, " (fara intarziere)");
                                }
                                strcat(msgrasp, "\nRuta dureaza ");
                                strcat(msgrasp, durataXml);
                                strcat(msgrasp, ".");
                                char CI[100];
                                bzero(CI, 100);
                                char clientIDc[10];
                                sprintf(clientIDc, "%d", tdL.cl);
                                strcpy(CI, clientIDc);
                                strcat(CI, " ");
                                char idTrenXml[100];
                                strcpy(idTrenXml, xmlGetProp(node, (const xmlChar *)"id"));
                                strcat(CI, idTrenXml);
                                strcat(CI, "\n");
                                FILE *clientiInteresati;
                                clientiInteresati = fopen("clientiInteresati.txt", "a");
                                if (clientiInteresati == NULL)
                                {
                                    error_handler("[server]Eroare! Nu s-a putut deschide fisierul clientiInteresati.txt cu 'a'.\n");
                                }
                                pthread_mutex_lock(&mlock);
                                fprintf(clientiInteresati, "%s", CI);
                                pthread_mutex_unlock(&mlock);
                                fclose(clientiInteresati);
                            }
                        }
                    }
                    xmlFreeDoc(document);
                    xmlCleanupParser();
                    if (numarTrenuriGasite == 0)
                    {
                        strcat(msgrasp, "Nu sunt trenuri pe aceasta ruta!");
                    }
                }
                else if (nrParametri < 2)
                    strcpy(msgrasp, "Ai introdus prea putini parametri!");
                else if (nrParametri > 2)
                    strcpy(msgrasp, "Ai introdus prea multi parametri!");
            }
            else if (strcmp(comanda, "/update") == 0)
            {
                if (esteLogat == 1)
                {
                    if (nrParametri == 3)
                    {
                        char idTren[10];
                        char statusTren[20];
                        char oraSesizarii[10];
                        bzero(idTren, 10);
                        bzero(statusTren, 20);
                        bzero(oraSesizarii, 10);
                        p = strtok(NULL, " ");
                        strcpy(idTren, p);
                        p = strtok(NULL, " ");
                        strcpy(statusTren, p);
                        p = strtok(NULL, " ");
                        strcpy(oraSesizarii, p);
                        int oravalida = 0;
                        if ((isdigit(oraSesizarii[0]) != 0 && oraSesizarii[1] == ':' && isdigit(oraSesizarii[2]) != 0 && isdigit(oraSesizarii[3]) != 0) || ((isdigit(oraSesizarii[0]) != 0 && isdigit(oraSesizarii[1]) != 0 && oraSesizarii[2] == ':' && isdigit(oraSesizarii[3]) != 0 && isdigit(oraSesizarii[4]) != 0)))
                        {
                            oravalida = 1;
                        }
                        if (oravalida == 1)
                        {
                            xmlDoc *document;
                            xmlNode *root, *node, *intarziereSosirePlecare, *intarzierePlecare, *intarziereSosireDestinatie, *oraPlecare, *oraSosire;
                            char intarziereSP[100];
                            bzero(intarziereSP, 100);
                            char intarziereP[100];
                            bzero(intarziereP, 100);
                            char intarziereSD[100];
                            bzero(intarziereSD, 100);
                            char oraPle[100];
                            bzero(oraPle, 100);
                            char oraSos[100];
                            bzero(oraSos, 100);
                            char *filename;
                            filename = "mersulTrenurilor.xml";
                            document = xmlReadFile(filename, NULL, 0);
                            root = xmlDocGetRootElement(document);
                            char trenInfo[1000];
                            int numarTrenuriGasite = 0;
                            int trenGasit = 0;
                            for (node = root->children; node != NULL; node = node->next)
                            {
                                if (node->type == XML_ELEMENT_NODE)
                                {
                                    char idTrenXml[100];
                                    strcpy(idTrenXml, xmlGetProp(node, (const xmlChar *)"id"));
                                    if (strcmp(idTren, idTrenXml) == 0)
                                    {
                                        trenGasit = 1;
                                        strcpy(msgrasp, "Editez trenul cu id: ");
                                        strcat(msgrasp, idTrenXml);
                                        strcat(msgrasp, ".\n");
                                        if (strcmp(statusTren, "IS") == 0)
                                        {
                                            pthread_mutex_lock(&mlock);
                                            oraPlecare = xmlFirstElementChild(node);
                                            while (oraPlecare != NULL && xmlStrcmp(oraPlecare->name, (const xmlChar *)"oraPlecarii") != 0)
                                            {
                                                oraPlecare = xmlNextElementSibling(oraPlecare);
                                            }
                                            strcpy(oraPle, (const char *)xmlNodeGetContent(oraPlecare));

                                            intarziereSosirePlecare = xmlFirstElementChild(node);
                                            while (intarziereSosirePlecare != NULL && xmlStrcmp(intarziereSosirePlecare->name, (const xmlChar *)"intarziereSosirePlecare") != 0)
                                            {
                                                intarziereSosirePlecare = xmlNextElementSibling(intarziereSosirePlecare);
                                            }
                                            strcpy(intarziereSP, (const char *)xmlNodeGetContent(intarziereSosirePlecare));
                                            char diferenta[10];
                                            bzero(diferenta, 10);
                                            timeDifference(oraPle, oraSesizarii, diferenta);
                                            xmlNodeSetContent(intarziereSosirePlecare, (const xmlChar *)diferenta);

                                            intarzierePlecare = xmlFirstElementChild(node);
                                            while (intarzierePlecare != NULL && xmlStrcmp(intarzierePlecare->name, (const xmlChar *)"intarzierePlecare") != 0)
                                            {
                                                intarzierePlecare = xmlNextElementSibling(intarzierePlecare);
                                            }
                                            xmlNodeSetContent(intarzierePlecare, (const xmlChar *)diferenta);

                                            intarziereSosireDestinatie = xmlFirstElementChild(node);
                                            while (intarziereSosireDestinatie != NULL && xmlStrcmp(intarziereSosireDestinatie->name, (const xmlChar *)"intarziereSosireDestinatie") != 0)
                                            {
                                                intarziereSosireDestinatie = xmlNextElementSibling(intarziereSosireDestinatie);
                                            }
                                            xmlNodeSetContent(intarziereSosireDestinatie, (const xmlChar *)diferenta);

                                            if (xmlSaveFormatFile("mersulTrenurilor.xml", document, 1) == -1)
                                            {
                                                error_handler("[server]Eroare la salvarea documentului XML.\n");
                                            }
                                            pthread_mutex_unlock(&mlock);
                                        }
                                        else if (strcmp(statusTren, "IP") == 0)
                                        {
                                            pthread_mutex_lock(&mlock);
                                            oraPlecare = xmlFirstElementChild(node);
                                            while (oraPlecare != NULL && xmlStrcmp(oraPlecare->name, (const xmlChar *)"oraPlecarii") != 0)
                                            {
                                                oraPlecare = xmlNextElementSibling(oraPlecare);
                                            }
                                            strcpy(oraPle, (const char *)xmlNodeGetContent(oraPlecare));

                                            intarziereSosirePlecare = xmlFirstElementChild(node);
                                            while (intarziereSosirePlecare != NULL && xmlStrcmp(intarziereSosirePlecare->name, (const xmlChar *)"intarziereSosirePlecare") != 0)
                                            {
                                                intarziereSosirePlecare = xmlNextElementSibling(intarziereSosirePlecare);
                                            }
                                            xmlNodeSetContent(intarziereSosirePlecare, (const xmlChar *)"0");

                                            intarzierePlecare = xmlFirstElementChild(node);
                                            while (intarzierePlecare != NULL && xmlStrcmp(intarzierePlecare->name, (const xmlChar *)"intarzierePlecare") != 0)
                                            {
                                                intarzierePlecare = xmlNextElementSibling(intarzierePlecare);
                                            }
                                            char diferenta[10];
                                            bzero(diferenta, 10);
                                            timeDifference(oraPle, oraSesizarii, diferenta);
                                            xmlNodeSetContent(intarzierePlecare, (const xmlChar *)diferenta);

                                            intarziereSosireDestinatie = xmlFirstElementChild(node);
                                            while (intarziereSosireDestinatie != NULL && xmlStrcmp(intarziereSosireDestinatie->name, (const xmlChar *)"intarziereSosireDestinatie") != 0)
                                            {
                                                intarziereSosireDestinatie = xmlNextElementSibling(intarziereSosireDestinatie);
                                            }
                                            xmlNodeSetContent(intarziereSosireDestinatie, (const xmlChar *)diferenta);

                                            if (xmlSaveFormatFile("mersulTrenurilor.xml", document, 1) == -1)
                                            {
                                                error_handler("[server]Eroare la salvara documentului XML. (2)\n");
                                            }
                                            pthread_mutex_unlock(&mlock);
                                        }
                                        if (strcmp(statusTren, "IP") != 0 && strcmp(statusTren, "IS") != 0)
                                        {
                                            strcpy(msgrasp, "Status Invalid!");
                                        }
                                        else
                                            sendNotification(tdL.cl, idTren);
                                    }
                                }
                            }
                            xmlFreeDoc(document);
                            xmlCleanupParser();
                            if (trenGasit == 0)
                                strcpy(msgrasp, "Nu am gasit trenul cu acest id!");
                        }
                        else
                        {
                            strcpy(msgrasp, "Ora sesizare invalida!");
                        }
                    }
                    else if (nrParametri < 3)
                        strcpy(msgrasp, "Ai introdus prea putini parametri!");
                    else if (nrParametri > 3)
                        strcpy(msgrasp, "Ai introdus prea multi parametri!");
                }
                else
                {
                    strcpy(msgrasp, "Trebuie sa fii logat pentru a folosi aceasta comanda!");
                }
            }
        }
        printf("[server]Mesajul a fost receptionat...%s\n", msgPrimit);
        printf("[server]Trimitem mesajul inapoi...%s\n", msgrasp);
        size_t marimeMesajRaspuns = strlen(msgrasp);
        if (write(tdL.cl, &marimeMesajRaspuns, sizeof(size_t)) < 0)
        {
            error_handler("[server]Eroare la dimensiune write() catre client.\n");
        }
        if (write(tdL.cl, msgrasp, marimeMesajRaspuns) < 0)
        {
            error_handler("[server]Eroare la write() catre client.\n");
            continue;
        }
        else
            printf("[server]Mesajul a fost trasmis cu succes.\n");
        if (strncmp(msgrasp, "quit", 4) == 0)
        {
            memset(msgrasp, 0, sizeof(msgrasp));
            break;
        }
    }
    sqlite3_close(db);
    pthread_mutex_destroy(&mlock);
}
