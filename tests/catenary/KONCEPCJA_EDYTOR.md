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

## Panel „Konstrukcje wsporcze” (Jan 2026-10-21)

Cel: ktoś ulepi własny słup, wklepie go w edytorze, opisze co trzeba i od razu wstawia – bez notatnika i szukania, co co robi.
Panel w ImGui, część edytora sieci (te same: wstawianie słupa, podgląd, wywołanie solvera), nie osobno.

- Lista słupów i mocowań z polami: model, fundament, top, length, depth góra/dół, mount_max, mocowania strut/stay/steady.
- Podgląd na żywo: słup z mocowaniami na kilku wysokościach i przykładowy wysięgnik.
- Przycisk „sprawdź”: solver na wszystkich kartach na tym słupie – ile bez uwag / z uwagami / nie składa, z listą uwag.
- Zapis przez exe do `data/traction_supports.txt` (plik zostaje jako magazyn; ręczna edycja dalej możliwa).
- Opcjonalnie przeglądarka kart tylko do odczytu (karta + wykonanie → złożony wysięgnik, lista części).

NA KONIEC / może nigdy (Jan: szansa, że ktoś przyjdzie z własnym katalogiem sieci, praktycznie zerowa): definiowanie nowych
części z punktami klikanymi w modelu, edycja kart i przepisów składania, GUI do generatora. Generator (`gen_cantilevers.py`)
zostaje narzędziem deweloperskim, `traction_cantilevers.txt` – częścią paczki jak tekstury.

## Kolejność

1. Gałąź `siec-trakcyjna` od aktualnego mastera, przeniesienie dzisiejszej sieci (pliki `world/catenary_*` bez
   zmian; wspólne pliki scalane ręcznie: Traction, renderery, Globals, Model3d, panel F12, serializer, CMake).
2. Rejestr konstrukcji wsporczych + stawianie słupów przypiętych do toru.
3. Podwieszenia z doborem karty i podglądem, potem przęsła.
4. 102x (dwie sieci), parasole, kontrole.

## Do ustalenia z Janem

- ~~Tabele rozpiętości przęseł i zygzaka~~ – ODCZYTANE 2026-10-21 (niżej).
- ~~Czy słupy mają być osobnym plikiem~~ – USTALONE 2026-10-21: osobny ręczny `data/traction_supports.txt` (słupy i mocowania).
- Radio: osobna gałąź od mastera (PR niezależny od sieci).

## Zrobione 2026-10-21 wieczór – tryb „Sieć trakcyjna” w edytorze (Tory → Sieć trakcyjna, klawisz N)

ZASADA (użytkownik): użytkownik stawia słupy tam, gdzie je widzi na podkładzie (ortofotomapa), często bez profilu
pionowego. ŻADNYCH automatów rozstawiających/drutujących – edytor tylko dobiera kartę i SPRAWDZA (rozpiętości z tablic).

- Klik przy torze stawia słup (typ z `traction_supports.txt`) po stronie kursora; czoło słupa w zadanej odległości od osi
  toru (skrajnia, domyślnie 2,50), oś słupa = skrajnia + ½ głębokości słupa przy główce szyny; przewody na H (5,60) nad
  główką, odsuw z ±z (prosta: na zmianę do/od słupa, „Na zmianę”; łuk R < 4000: na zewnątrz łuku, z ≥ 0,40).
- Karta: dziesiątka z rodziny (przechylne 11xx / nieprzechylne 12xx, prosta 1x/2x, łuk 3x/4x, do słupa x1/x3, od słupa
  x2/x4 – objaśnienia tomu 3), ostatnia cyfra z pasma A (`scheme_for_reach`, A = oś słupa − odsuw − ½ głębokości na
  wysokości H); wykonanie 1–5 wybiera użytkownik. Brak karty = czerwony podgląd i komunikat.
- Zaznaczony słup: suwaki (skrajnia, odsuw, H, wykonanie, typ słupa, izolatory 25 kV) zmieniają go NA ŻYWO – wysięgnik
  składa się od nowa przy każdym ruchu suwaka, karta przeskakuje z pasmem A. Uwagi solvera w panelu. Usuń / Cofnij
  (własne cofanie narzędzia; Ctrl+Z edytora go nie obejmuje).
- Przęsła: pary sąsiednich podwieszeń tej samej linii (cięciwa między drutami odchylona od dwusiecznej kierunków
  wysięgników o ≤ 1,5 m), promień z kąta między kierunkami, limit z `data/traction_layout.txt` (karta 43-0002 + uzupełnienie
  2010, łuk a = 2√(2Rz), stary katalog: 66/72/80); zielone w limicie, czerwone ponad. Typ sieci i strefę wiatrową wybiera
  użytkownik w panelu.
- Zapis: przy zapisie scenerii pliki sieci (`catenary_scene_save`, CRLF, nagłówek z komentarzy zachowany); nowy plik
  dostaje dyrektywę `catenary <plik> endcatenary` w pliku głównym scenerii.
- Części wysięgników są odpięte od warstw scenerii (`catenary_part_detach`) – wcześniej w trybie -edit zapis scenerii
  dopisałby każdą część jako zwykły model (ukryty błąd, poprawiony przy okazji).
- Jeszcze nie ma: przęsła `node traction` stawiane z edytora, parasole z edytora, przesuwanie słupa wzdłuż toru
  przeciąganiem, bramki. Części wysięgników da się zaznaczyć w trybie Otoczenie jak zwykłe modele (do zablokowania).

Tablice (odczytane 2026-10-21): Katalog 1 (`z- KATALOG 1 -z.pdf`) s. 4 karta 43-0002 (rozpiętości), s. 6 karta 23-0007
(pasma A dla skrajni 2,20/2,50, prosta z 0,30/0,15/0, łuk z ±0,40), s. 7 karta 33-0008 (wykres a = 2√(2Rz));
`Kolprojekt_2010-1.PDF` s. 6–7 (0002 z sieciami 36, 37; 0008 także dla z = 0,30); stary `kolprojekt1.pdf` s. 11–13
(13-0002, 13-0005 wykonania 2/3/4, 13-0008: a max sieci 1, 2 – 66 m, 3, 4 – 72 m, 5 – 80 m, potwierdzone przez użytkownika).
Katalog 1 ma też dział 0300 (zespoły podwieszeń przewodu wzmacniającego) – do LPN-ów/linii wzmacniających.
