# PCO - Laboratoire 06

###### Rhyan Robertson & Dylan Canton

###### 30.05.2021

---

### Description des fonctionnalités du logiciel

Le but ici est d'implémenter un *Thread Pool*. Ce dernier permet d'allouer dynamiquement des threads lorsque cela devient nécessaire, il est ensuite aussi possible de réutiliser un thread ayant fini son exécution. Un *Thread Pool* permet donc de gérer dynamiquement des threads et donc de répartir la charge de calcul entre plusieurs cœurs et donc plusieurs threads. 

L'implémentation va s'effectuer à l'aide d'**un moniteur de Mesa** pour la synchronisation des threads. 



---

### Implémentation







---

### Tests

Les tests ont été effectués à l'aide du fichier `tst_threadpool.cpp`, 4 tests ont été effectués permettant de tester le lancement, la gestion et la terminaison de plusieurs tailles de *Thread Pool*.

La capture ci-dessous montre le résultat des tests. 

![tests](media/tests.PNG)



---

### Remarques

Nous avons observé des divergences dans les résultats de tests, ces derniers se basant sur un temps exécution afin de valider ou non le test. Même si dans la majorité des cas, les tests passent, il arrive parfois qu'une partie des tests échoue en raison de ce temps exécution. Malgré cela, le comportement du programme est fonctionnel. 
