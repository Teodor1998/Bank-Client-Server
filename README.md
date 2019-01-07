# Bank-Client-Server
    	
    Schema arhivei:
    |
    | server.c
    | client.c
    | clienti
    | Makefile
    | README
    |____
      
      
      Unde clienti este fisierul cu baza de date a conturilor.

    	Am implementat rezolvarea temei in limbajul C.
    	In vederea prezentarii algoritmilor folositi pentru rezolvarea
    taskurilor voi prezenta fragmente din server si client/clienti in
    paralel pentru o mai buna intelegere a modului de comunicare dintre
    aceste entitati.
    	Atat clientul cat si serverul folosesc fd_set pentru a stoca
    sursele de unde primesc informatii.
    	In principiu avem trei tipuri de surse:
    		- socketul TCP, pentru majoritatea comenzilor
    		- socketul UDP, pentru functia unlock
    		- stdin, pentru citirea comenzilor atat in server cat si in
    			client
    	In rezolvare am folosit cateva functii auxiliare in server pe care
    le voi descrie cand vom ajunge la apelul acestora.
    	In server totul se intampla in interiorul unei bucle while. Aici
    parcurgem la fiecare iteratie toate elementele setului si le gestionam
    dupa caz.
    	Daca avem mesaj de la stdin, acesta va fi verificat. Daca este
    quit, acesta va trimite quit pe TCP la toti clientii si ii va inchide.
    Daca acesta este orice altceva, se va afisa eroare la server.
    	Daca avem mesaj de la socket-ul UDP, inseamna ca este vorba de o
    deblocare de cont. Am folosit foarte mult functie strtok, pentru
    divizarea mesajelor primite in buffer si gestionarea acestora. Daca
    mesajul primit pe UDP are primul cuvant unlock, putem trage concluzia
    ca este vorba de o deblocare si apelam functia "unlock_req()". Aceasta
    functie primeste baza de date a conturilor (implementata printr-o
    structura ce contine toate campurile, si starea de blocare), bufferul,
    numarul de conturi si vectorul deblocare.
    	Vectorul deblocarae are urmatoare logica de utilizare: Fiecare
    pozitie a acestuia reprezinta un cont. daca la pozitia contului are
    valoare nula inseamna ca nu exista alt client care sa incerce deblocarea
    contului la acel moment de timp. Daca pe pozitia aceea are 1 atunci exista
    un client care a dat deblocare inainte si se afla la propmpt-ul cu parola.
    Functia unlock_req va verifica parametrii primiti: numarul cardului, si
    daca nu cumva altcineva incearca sa il deblocheze. Daca totul este corect,
    se va trimite la client cererea introducerii parolei.
    	Dupa ce clientul a trimis pe UDP cererea de deblocare, acesta va
    seta variabila unlockConf = 1, cu alte cuvinte va stii ca urmatorul mesaj
    pe care il primeste de la tastatura va trebui trimis prin UDP ca si parola.
    Odata trimis, variabila va redeveni 0.
    	Odata ce serverul primeste parola, ii va verifica corectitudiea cu
    functia "unlock()". Daca parola este corecta, contul va fi deblocat si
    cu ajutorul unui switch se va decide ce mesaj se va trimite inapoi
    clientului.
    	Daca serverul a primit o noua conexiune, acesta o va accepta.
    	Altfel (daca nu am primit nimic de la tastatura, nici de la UDP si nici
    conexiune noua) inseamna ca am primit ceva pe socket-ul TCP pentru clientul
    curent.
    	In acest caz putem avea o serie de comenzi pe care le verificam cu
    ajutorul unei insiruiri de if...else.
    	Voi descrie mai in amanunt comenzile mai complexe si anume login si
    transfer.
    	In cazul in care clientul primeste de la tastatura comanda login, acesta
    va verifica daca nu cumva este deja logat, prin interogarea variabilei logged.
    Daca aceasta este nevida, inseamna ca este logat si ca urmare nu va mai
    trimite cerere de login la server. In caz contrar, acesta va seta variabila
    loginSent ca fiind 1 si va astepta confirmarea serverului la mesajul
    proaspat trimis, pentru a stii daca este sau nu logat.
    	Serverul primeste pe TCP comanda de login si ii verifica corectitudinea
    cu apelul functiei login. Aceasta va primi baza de date cu conturi si
    si va verifica numarul de card si parola. In cazul in care totul este corect
    va intoarce indexul contului in "baza noastra de date". Valoarea intoarsa
    va fi gestionata de un switch. Daca este o eroare se va trimite mesajul
    de eroare aferent, altfel se va trimite confirmarea la client.
    	Cand clientul trimite cererea de transfer bancar, acesta va seta
    variabila transfer_sent si va sti ca urmatorul mesaj pe care il va primi pe TCP
    va fi fie o confirmare, fie o eroare. Daca este o confirmare, va seta variabila
    confirm_transfer, si astfel va stii ca urmatoarea comanda primita de la
    tastatura va fi defapt inteleasa ca simplu mesaj de trimis. Astfel am evitat
    confuziile cu celelalte functii pe care le-ar putea cere serverul.
    Exemplu: daca se cere confirmarea si trimitem login <cont> <pin>, se va intelege
    ca n, nu ca login.
    	In server, acesta primeste cererea de transfer si verifica corectitudinea
    parametrilor printr-un apel al functiei cerere_transfer(). In functie de
    rezultatul intors de aceasta se va trimite inapoi la client o confirmare sau
    o eroare. De asemenea pentru a stii despre ce client este vorba (care vrea sa
    faca un transfer), valoarea aceasta in vectorul session va fi inmultita cu un
    numar (130). Daca la primirea confirmarii, clientul curent va fi gasit in vectorul
    session cu valoarea sa * 130, atunci vom sti ca este vorba de o confirmare pentru
    transfer. De asemenea, la indexul acestuia se va adauga in vectorul coada_transfer,
    indexul contului la care se va face transferul si suma ce va fi transferata.
    	La primirea confirmarii se va apela functia "transfer()" care va face transferul
    propriuzis.
    	In client ultimul card la care s-a incercat un login, este salvat in variabila
    last_card. Majoritatea variabilelor declarate la incepu au rolul ajutator pentru a ajuta
    programul sa inteleaga cum sa gestioneze urmatoarele mesaje/comenzi.
    	Pentru listsold, am verificat in client daca acesta este logat si in caz pozitiv,
    se trimite cerere la server. Acesta va intoarce mesajul cu valoarea soldului aferent.
    	Pentru logout, clientul verifica daca este logat sa nu. In cazul in care este,
    acesta trimite apel la server si acesta il va deloga, altfel va afisa ca aceste este
    deja delogat.

    	Timpul necesar rezolvarii temei a fost de aproximativ 48 de ore de lucru.
    	FEEDBACK: Consider ca aceasta tema m-a ajutat sa inteleg mult mai bine modul de
    funcitonare al protocoalelor TCP si UDP.
