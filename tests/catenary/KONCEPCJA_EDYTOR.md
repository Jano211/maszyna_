# Edytor sieci trakcyjnej w exe – koncepcja (2026-10-16, szkic)

Stan wyjściowy: wbudowany edytor 3D (autor Hirek193, maj00r) jest już w **masterze** MaSzyna-EU07/maszyna
(gałąź `feat/modern-editor` została do niego wciągnięta; master 2026-10-16 = c7f9796). Źródła Jana w `C:\Maszyna`
są z mastera z ok. 2026-09-23 (b1bdd42) – sprzed edytora. Konstrukcji wsporczych kolegi nie ma nigdzie i nie czekamy
na nie (Jan 2026-10-16: kolega robi renderer GL 4.6); konstrukcje wsporcze robimy sami.

## Co edytor już daje

- Tryb **infra** (`editor/editorInfra.*`, `application/editormodeinfra.cpp`): obiekt przypięty do toru przez
  `anchor` = (tor, ścieżka, stacja s w planie, odsunięcie w bok, wysokość nad główką). Po zmianie geometrii toru
  przypięte obiekty jadą za nim. Przypięcia zapisywane w scenerii znacznikiem `//$b`.
- Kategorie po nazwach plików (`default_rules`): `pole` (slup, maszt, wysieg, bramk, konstrukcj, kotw, odciag),
  `catenary` (trakcj, siec_, przewod, lina_nosna). `node traction` = cztery anchory, `move_traction` przesuwa przęsło.
- Pasek trybów pracy (otoczenie, tory, drogi, teren), okno inspektora, bank węzłów z podglądami, cofanie,
  tłumaczenia z `lang/editor_<lang>.po`, autotesty (`editormodeselftest.cpp`).

## Proponowany tryb „Sieć trakcyjna”

1. **Konstrukcje wsporcze** – rejestr typów (słupy pojedyncze, bramki, półbramki): model, wysokość, zbieżność
   (głębokość na wysokości), dostępne mocowania, `mount_max`. Format jak dzisiejszy `pole … endpole` w
   `traction_cantilevers.txt`, z czasem osobny plik. Słup stawiany jako model przypięty anchorem do toru
   (stacja + odsunięcie od osi = gabaryt).
2. **Rozstaw słupów** wzdłuż toru: od punktu do punktu, z maks. rozpiętością przęsła zależną od promienia łuku
   (tabele z katalogu – do odczytania), wybór typu sieci (`type <symbol>`), przęsła naprężenia co ok. 1,5 km.
3. **Przęsła** – `node traction … type <symbol>` między kolejnymi podwieszeniami, nazwy `l<linia>_<tor>_<nr>`
   nadawane automatycznie, przypięte jak dziś (cztery anchory).
4. **Podwieszenia** – dla pary (słup, tor): odsuw przewodu (zygzak ±0,2/±0,3 m, na łukach na zewnątrz), dobór karty
   z odległości czoła słupa od osi toru (pasmo A) i z kierunku odsuwu (do/od słupa), hk z typu sieci. Edytor pisze
   plik `catenary <plik> endcatenary` z wpisami `node … suspension scheme … at … rope … pole … endsuspension`;
   wysięgnik składa solver (ten sam kod co w symulacji), podgląd na żywo, ostrzeżenia solvera przy słupie.
5. **Przęsła naprężenia i 102x** – w miejscu zmiany sekcji dwie sieci na jednym słupie: podwieszenie dostaje dwa
   komplety punktów (`at`/`rope` i `at_b`/`rope_b`) i przęsła obu sieci; solver i parser trzeba przerobić
   (KONCEPCJA.md, punkt 2.1). Edytor wie, które przęsła są „drugą siecią”, więc to robimy razem.
6. **Parasole** – dwa podwieszenia na jednym słupie (`umbrella <nazwa>`), wspólne mocowania.
7. **Kontrole** – rozpiętość przęsła, zygzak, wysokość przewodu, pasmo A, kolizja z gabarytem (moduł gauge jest).

## Kolejność

1. Gałąź `siec-trakcyjna` od aktualnego mastera, przeniesienie dzisiejszej sieci (pliki `world/catenary_*` bez
   zmian; wspólne pliki scalane ręcznie: Traction, renderery, Globals, Model3d, panel F12, serializer, CMake).
2. Rejestr konstrukcji wsporczych + stawianie słupów przypiętych do toru.
3. Podwieszenia z doborem karty i podglądem, potem przęsła.
4. 102x (dwie sieci), parasole, kontrole.

## Do ustalenia z Janem

- Tabele rozpiętości przęseł i zygzaka – z którego katalogu (tom 1?).
- Czy słupy mają być osobnym plikiem rejestru, czy dalej w `traction_cantilevers.txt`.
- Radio: osobna gałąź od mastera (PR niezależny od sieci).
