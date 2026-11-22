# Zasilanie sieci: odłączniki, odgromniki, połączenia elektryczne, iskierniki – klocki z katalogów (rozeznanie 2026-11-16)

Źródła (dysk użytkownika `E:\SYF\INNE\PDF\Katalog ESN\`): tom 2 `z- KATALOG 2 -z.pdf` (dział 0600–0699 i 0800–0899, strony PDF
w nawiasach), tom 1 `z- KATALOG 1 -z.pdf` (dział 0500: zamocowania odłączników), tom 5 `z- KATALOG 5 -z.pdf` (części). Skany –
tabele czytane okiem / OCR, numery do potwierdzenia przy modelowaniu. Uzupełnienie nr 1: karty 13-0547, 13-0549 (zamocowania
odłączników z napędem silnikowym).

## PLAN „od A do Z” – fizyka prądu nowej sieci (2026-11-21, zastępuje plan z 16.11)

Użytkownik 21.11: „zróbmy to porządnie od A do Z – jak wysięgniki i słupy rozpierdalają system, to niech fizyka prądu też”.
Stan (2026-11-21 wieczór): **A i B zrobione** (solver + budowa obwodu, testy `tp`), **H zrobione** (odgromnik 7310 na słupach,
bez bramek), **E w obwodzie** (odłączniki bez modeli) – w exe NIESPRAWDZONE, szczegóły w PRZEKAZANIE.md. Dalej: C (podstacje
z odpływami, wspólny prostownik, uzależnienie zasilaczy), D (kabiny z wyłącznikami), modele odłączników. Każdy etap z testem na atrapie, w exe sprawdza użytkownik.

### Ustalenia z użytkownikiem (21.11)
1. **Stara sieć (`node traction`) zostaje po staremu** – bez zmian w TTraction::VoltageGet / ResistanceCalc / TTractionPowerSource
   dla starych scenerii (nikt nie będzie przeklikiwał starych scenerii w poszukiwaniu regresji). Nowy solver działa WYŁĄCZNIE na
   przęsłach nowej sieci (łańcuchy z plików `catenary`). Przęsło nowej sieci = dalej obiekt TTraction (pantograf, iskrzenie,
   DynObj bez zmian), tylko jego VoltageGet idzie do nowego solvera (flaga / wskaźnik na węzeł obwodu).
2. **Szyny jako przewód powrotny – tak, jeśli to coś daje i nie zjada FPS.** Daje: szyny to ok. 1/4 oporu pętli (niżej), więc
   bez nich spadki napięcia wychodzą zaniżone. Koszt: przy rozwiązywaniu wysp (niżej) pomijalny. Etap 1 – opór szyn dopisany
   szeregowo do gałęzi przęsła (zero dodatkowych węzłów); pełna sieć szynowa z łącznikami poprzecznymi i dławikami – później,
   tylko jeśli wyjdzie różnica widoczna z kabiny.
3. **Liczby: Głowacki, ewentualnie instrukcje PKP PLK.** Głowacki przeczytany 21.11 (sekcja niżej) – zasady tak, liczb
   elektrycznych podstacji brak.

### Z Głowackiego (przeczytane 21.11: rozdz. 2.1 s. 18–19, rozdz. 9.2 s. 203–212, 10.1 s. 219)
- **Liczb elektrycznych podstacji (napięcie biegu jałowego, opór wewnętrzny, nastawy) Głowacki NIE podaje** – zostają artykuły IK
  (niżej). Podstacje z transformacją jednostopniową (110 kV) mają niższą rezystancję zastępczą – „sztywniejsze” napięcie 3 kV
  (s. 18) → parametr podstacji, nie stała.
- Podstacja: zespół(y) prostownikowe, rozdzielnia 3 kV, odpływy = zasilacze z wyłącznikami szybkimi; zasilacze osobno na każdy tor
  i w każdą stronę (dwutorowa 4, jednotorowa 2); zasilacz przyłączony do sieci przez odłącznik zasilaczowy („OKZ”), numery 10/30
  (tor nieparzysty przed / za granicą zasilania), 20/40 (parzysty), powtórzenia 110, 210…; zasilacze stacyjne: 60 lokomotywownia,
  70 grupa nieparzysta, 80 parzysta, 90 inna grupa (160, 270…); rozdzielnia zasilaczowa na bramce: odłącznik główny + 70-A, 70-B…
  (s. 203–212). Sieć powrotna łączona z podstacją WYŁĄCZNIE kablami, krótkimi, na wprost podstacji (s. 18) → kabel powrotny.
- Granica zasilania (z reguły przęsło naprężenia) ma odłącznik **normalnie otwarty**; zamyka się go awaryjnie (wyłączony
  zasilacz / podstacja) – odłączniki 101 (tor nieparzysty) / 102 (parzysty) między zasilaczami 10/30 i 20/40 (s. 204).
- Kabina sekcyjna: przy jednym pociągu w połowie odcinka spadek napięcia **mniejszy o 50 %**, przy kilku mniej; zwarcie wyłącza
  sieć jednego toru tylko od podstacji do kabiny (s. 209). Nastawy wyłączników kabiny **o 20 % niższe** niż podstacyjnych (s. 211).
- **Uzależnienie (współpraca) zasilaczy**: zwarcie blisko podstacji A – A wyłącza, B (daleko, prąd zwarcia mniejszy od nastawy)
  NIE; dlatego zadziałanie wyłącznika jednego zasilacza automatycznie wyłącza współpracujący (łącze telekomunikacyjne), także
  podstacja–kabina (s. 211) → w solverze: pary/grupy współpracujących wyłączników.
- Na liniach jednotorowych, zwłaszcza w układach awaryjnych, minimalny prąd zwarcia bywa **mniejszy od krótkotrwałego prądu
  rozruchu** – zwarcia nie da się wykryć nawet przy uzależnieniu; długotrwały prąd zwarcia przegrzewa / niszczy sieć i podnosi
  potencjał słupa (porażenie) (s. 211–212) → solver ma to odtworzyć (zwarcie odległe, którego wyłącznik nie widzi).
- Odrębny zasilacz dla torów lokomotywowni (dużo wyłączeń przy pracach na taborze) (s. 212).

### Liczby wstępne (Głowacki ich nie podaje – artykuły IK / instrukcje PKP PLK)
- Napięcie znamionowe na szynach podstacji **3,3 kV** (artykuł Szeląg, Maciołek 2013); bieg jałowy wyżej – DO USTALENIA.
- Opór wewnętrzny podstacji: stary kod przyjmuje 0,2 Ω (prezentacja Kruczek, IK) – na start to samo, DO SPRAWDZENIA.
- Wyłączniki szybkie: w podstacjach np. BWS 3150 A (Dzienis 2017), DCN 1600/2500/3150 A; nastawy (prąd, di/dt) – DO USTALENIA
  (Rojek: koordynacja zabezpieczeń pojazd – podstacja, IK).
- Odległość podstacji: dawniej 20–25 km, po modernizacji maks. ok. 13 km (Białoń i in. 2010) – do kontroli w edytorze (ostrzeżenie).
- Przewody: Djp100 0,182 Ω/km (Głowacki, GLOWACKI.md); liny Cu – z przekroju (ok. 0,0179 Ω·mm²/m → L120 ≈ 0,15 Ω/km), DO SPRAWDZENIA.
  Dopisać kolumnę oporu do `wire` w traction_tables.txt (bez niej – z przekroju i materiału).
- Szyny: WŁASNE WYLICZENIE z przekroju (60E1 76,7 cm², stal szynowa ok. 0,2–0,22 μΩ·m) → ok. 0,03 Ω/km na szynę, tor (dwie
  szyny) ok. 0,015 Ω/km + złącza; dla porównania YC120-2C ≈ 0,058 Ω/km → szyny to ok. 20–25 % pętli. DO SPRAWDZENIA (Głowacki rozdz. 10).

### Model (co jest liczone)
- Obwód prądu stałego: węzły = końce przęseł nowej sieci (+ punkty pośrednie tylko tam, gdzie coś się wpina: pantograf, połączenie,
  izolator sekcyjny); gałęzie = przęsła (opór z typu sieci: wszystkie liny nośne i przewody jezdne równolegle, + szyny szeregowo).
- Pojazd = odbiornik o prądzie z poprzedniego kroku (jak dziś `VoltageGet(u, i)`), rekuperacja = źródło prądu; napięcie na
  pantografie z rozwiązania (interpolacja wzdłuż przęsła).
- Rozwiązanie: obwód dzielony na **wyspy** (części połączone galwanicznie, po otwarciu wyłączników / odłączników / izolatorów);
  każda wyspa – układ rzadki (macierz przewodności), rozwiązywany iteracyjnie z ciepłym startem z poprzedniego kroku. Topologia
  przebudowywana tylko przy zmianie stanu (łącznik, wyłącznik, pantograf mostkujący przerwę). Częstotliwość: co krok fizyki, jeśli
  pomiar pokaże koszt – co n kroków (spadki napięcia i tak zmieniają się wolno).
- Podstacja = prostownik: źródło napięcia z oporem wewnętrznym, **dioda** (bez prądu wstecznego, chyba że `recuperation` –
  falownik/odbiór rekuperacji); jeden zespół prostownikowy, szyna zbiorcza 3 kV i **odpływy (zasilacze)**, każdy z własnym
  wyłącznikiem szybkim (osobno na tor i kierunek – Głowacki s. 205).
- Kabina sekcyjna = szyna zbiorcza bez źródła + odpływy z wyłącznikami (4 na linii dwutorowej, Głowacki s. 210–211) – łączy
  sieci obu torów, zmniejsza spadki, przy zwarciu wyłącza połowę odcinka.
- Wyłącznik szybki: wyzwolenie od prądu (nastawa) i od stromości (di/dt), ponowne załączenie z **próbą linii** (sprawdzenie
  izolacji przed załączeniem – zwarcie trwa → nie załącza), blokada po n próbach; stan widoczny w panelu i w edytorze.
- Odłącznik: zamknięty = gałąź o ~0 Ω między dwiema sekcjami / zasilaczem a siecią; ze **stykiem uszyniającym** – w położeniu
  uszynienia sieć zwarta do szyn (zamknięcie na sieć pod napięciem = zwarcie → wyłączniki).
- Izolator sekcyjny / izolowane przęsło naprężenia: przerwa w obwodzie; pantograf na przerwie mostkuje oba brzegi (prąd
  wyrównawczy przy różnych podstacjach → iskrzenie z catenary_arcing już jest; teraz z prawdziwym prądem).
- Przęsło naprężenia zwarte / łącznik 0640: gałąź ~0 Ω między łańcuchami.
- Przewód wzmacniający: gałąź równoległa do sieci, połączona w punktach połączeń (karty 0650–0653); przewody rozdzielcze 3 kV
  i kable zasilaczy (0655, 0656) – gałęzie z oporem z przekroju.
- Awarie: zwarcie przez uszynienie, zerwany przewód (przerwa + zwarcie do ziemi, jeśli leży), przepalenie (później, z prądem
  i czasem – „rozpierdala system”).

### Etapy
A. **Solver** (`world/catenary_power.h/.cpp`): graf, wyspy, rozwiązanie, źródła z diodą, odbiorniki; test na atrapie (znane obwody
   policzone ręcznie: jedna podstacja, dwie, kabina, rekuperacja bez odbioru).
B. **Mapowanie nowej sieci na obwód**: łańcuchy, przęsła naprężenia zwarte/izolowane, izolatory sekcyjne, łączniki; podpięcie
   TTraction nowych przęseł do solvera (stara sieć bez zmian).
C. **Podstacje i odpływy** w danych i w edytorze (rozbudowa strony „Zasilanie”: podstacja + jej odpływy przypisane do łańcuchów),
   wyłączniki szybkie z próbą linii.
D. **Kabiny sekcyjne** (szyna zbiorcza + odpływy z wyłącznikami), edytor.
E. **Odłączniki** (modele 0661–0677 + zamocowania 0541–0549, połączenia 0641/0643/0651), stan otwarty/zamknięty/uszyniony,
   edytor (stawianie na słupie, stan początkowy), przełączanie eventem.
F. **Przewody wzmacniające / rozdzielcze / kable** jako gałęzie.
G. **Awarie**: zwarcie przez uszynienie, pantograf na przerwie, zerwanie. Do tego (użytkownik 2026-11-22, „dawaj”) **ptak na odgromniku**:
   ptak mostkuje przerwę rożków 7310 (~10 mm) → zwarcie sieć–słup–tok szynowy przez odgromnik; łuk na rożkach (iskry, catenary_arcing),
   wyłącznik szybki odpływu wyłącza sekcję, próba linii / ponowne załączenie (etap C) – zwarcie zniknęło (ptak spadł, łuk zgasł) →
   sekcja wraca, inaczej wyłączona na dobre. Pociąg w sekcji traci na chwilę napięcie. Wyzwalanie: losowo (rzadko, z ustawieniem
   częstości, wyłączalne w eu07.ini) albo eventem na słupie. Bez uszkodzeń: izolatorów nie wypalamy (użytkownik 2026-11-22),
   opalanie rożków tylko szybkim, nieobciążającym shaderem (np. parametr per instancja przyciemniający końce rożków, jak pył
   po normalnej) – inaczej nie robić. Przy zwarciu **błysk** (krótkie światło punktowe na rożkach, jak przy iskrzeniu pantografu)
   i **huk** (dźwięk wyładowania przy słupie) – użytkownik 2026-11-22. Losowy ptak **tylko w zasięgu wzroku** kamery (słupy
   widoczne z miejsca gracza); poza nim nie losujemy. Do ustalenia przy etapie: jak długo pali się łuk przy 3 kV DC
   (w DC brak przejścia przez zero – ile robią rożki, ile wyłącznik).
H. **Odgromniki** – sam model (bez wpływu na prąd), stawiane w edytorze.
I. **Podgląd**: edytor – sekcje kolorami wg zasilacza / napięcia, stan łączników; panel maszynisty / debug – podstacje, odpływy,
   wyłączniki, prądy.
J. **„Opus magnum”**: wyjść z kabiny, podejść do słupa, przełączyć odłącznik (korba / drążek w skrzynce napędu) – też pod
   przyszły multiplayer. Do zaprojektowania: interakcja z obiektem scenerii poza kabiną.

### Otwarte (zapytać użytkownika przy etapie, którego dotyczy)
- Styk starej i nowej sieci w jednej scenerii (przęsło starej sieci połączone z nową): na razie – granica obwodu, stara część
  po staremu; czy tak zostaje? (etap B)
- Gdzie zapisywać podstacje / kabiny / odłączniki: wpisy w plikach `catenary` (nasz format) czy dalej `tractionpowersource`
  z rozszerzeniem (etap C)
- Nastawy wyłączników i napięcie biegu jałowego – z Głowackiego (etap C)

## Odłączniki – zespoły (tom 2, dział 0661–0679)
Ostatnia cyfra karty: nieparzysta – odłącznik prostopadle do osi toru, parzysta – równolegle. 0661–0669 napęd ręczny, 0671–0679
napęd silnikowy. Wszystkie w wykonaniu 11.

| karta | co | części (poz.: szt.) |
|---|---|---|
| 83-0661 (s. 17) | sekcyjny, ręczny, prostopadle | 2: 4820-1 + 4820-2, 3: 6116-1, 4: 7200-1 ×2, 5: 7420-1, 6: 7460-1 |
| 83-0662 (s. 18) | sekcyjny, ręczny, równolegle | jak 0661 |
| 83-0663 (s. 19) | ze stykiem uszyniającym, ręczny, prostopadle | jak 0661, ale 5: 7430-1 |
| 83-0664 (s. 20) | ze stykiem uszyniającym, ręczny, równolegle | 4820-1 ×2, 6116-1, 7200-1 ×2, 7430-1, 7460-1 |
| 83-0667 (s. 21) | pojedynczego punktu zasilania, ręczny | 4820-2, 6116-1, 7200-1 ×1, 7420-1, 7460-1 |
| 83-0671 (s. 22) | sekcyjny, silnikowy, prostopadle | 1: 4811-1, 4820-1 + 4820-2, 6116-1, 7200-1 ×2, 7420-1, 6: 7470-1 |
| 83-0672 (s. 23) | sekcyjny, silnikowy, równolegle | 4811-1, 4820-1 ×2, 6116-1, 7200-1 ×2, 7420-1, 7470-1 |
| 83-0677 (s. 24) | pojedynczego punktu zasilania, silnikowy | 4811-1, 4820-1, 6116-1, 7200-1 ×1, 7420-1, 7460-1 (tak w tabeli) |

Rysunek 0661: na szczycie słupa konstrukcja z odłącznikiem (noże na izolatorach), w dół drążek napędowy 6116, na dole skrzynka napędu.

## Zamocowania odłączników na konstrukcjach (tom 1, dział 0540–0549, s. 157–160)
- 23-0541 (s. 157) ręczny prostopadle, 23-0542 (s. 158) ręczny równolegle, 23-05xx (s. 159) silnikowy prostopadle, 23-0546 (s. 160)
  silnikowy równolegle (+ Uzup. 1: 13-0547, 13-0549).
- Wykonania wg konstrukcji: 1 – słupy z ceowników 120–200; 4 – słupy bramek z ceowników 140–200; 5 – słupy bramek z ceownika 240
  (nie ma w położeniu równoległym – zastępuje wyk. 4); 6 – słupy przestrzenne (pasy 400); 7 – dwuteownik 180/200 na fundamentach
  palowych; dalej 8, 10–12 (dwuteowniki 220/240, ceowniki na palach) – jak w zamocowaniach wysięgników.
- Części: 4750-1…-12 (konstrukcje pod odłącznik, wg wykonania), 4770-1…-12 (prowadnice drążka napędowego), 4790-x (konstrukcje pod
  skrzynkę napędu ręcznego), 4780-x (pod skrzynkę napędu silnikowego).

## Części (tom 5, strona PDF)
- 4750 konstrukcje pod odłącznik (s. 53), 4770 prowadnice drążka napędowego (s. 55), 4780 konstrukcje pod skrzynkę napędu
  silnikowego (s. 56), 4790 – napędu ręcznego (s. 57), 4811 łącznik napędu silnikowego ONS-1 (s. 59), 4820 wsporniki uniwersalne
  izolatora LWP8-24 (s. 60).
- 6116 drążek odłącznika (s. 127), 6861 korba ręczna napędu silnikowego ONS-1 (s. 163).
- 7200 izolator liniowy LWP 8-24 porcelanowy (s. 207), 7420 odłącznik BN-80/9317-105 (s. 214), 7430 odłącznik ze stykiem
  uszyniającym BN-80/9317-106 (s. 215), 7460 skrzynka napędu ręcznego BN-75/9317-88 (s. 216), 7470 napęd silnikowy ONS-1 (s. 217).
- 5880 połączenie odłącznika z uszynieniem linką L50 (s. 114).

## Odgromniki (tom 2, dział 0681–0689, s. 25–27; tom 5 s. 208)
- 93-0681 odgromniki rożkowe z połączeniem elektrycznym, słupy ceownikowe (s. 25); 83-0683, 13-0684 – inne konstrukcje (s. 26–27,
  te same części). Wykonanie wg liny nośnej: 5 – L70, 6 – L95, 7 – L120, 8 – L150.
- Części: 7310-1 odgromnik rożkowy BN-78/9317-87 (dwa rożki „V” z przerwą 10 mm, izolator, belka na ceowniku słupa), uchwyt
  równoległy do dwóch lin na linie nośnej: 5560-5 (wyk. 5), 5500-6 (wyk. 6), 5570-6 (wyk. 7), 5550-5 (wyk. 8); linka 9812-6 4,00 m
  łukiem od odgromnika do liny nośnej; 9981-1, 9986-1 (smar / drobnica, kg).

## Połączenia elektryczne (tom 2, dział 0630–0659, s. 4–16)
- 23-0639 (s. 4) odcinki przewodu rozdzielczego 3 kV (połączenia do odłączników 4820-3).
- 23-0640 (s. 5) międzysekcyjne połączenia sieci (łącznik w „S” – jest w kodzie, `overlap_connector`).
- 23-0641 (s. 6) międzysekcyjne POPRZEZ ODŁĄCZNIK: z każdej sekcji łuk linki 9815-9 od lin nośnych i przewodów (zaciski 6601-1/-2 na
  przewodach, 6605-6 na linach) do zacisków odłącznika; 6316-9 końcówki rurkowe, mostek 9861-1; ilości wg typu sieci (3, 4, 10, 20,
  26, 30, 32, 35): np. sieć 3/4 – 6316-9 ×4, 6601-1 ×4, 6605-6 ×4, 9815-9 20 m, 9861-1 3 m.
- 23-0642 / 0643 / 0644 (s. 7–9) równoległe połączenia sieci szlakowych z siecią stacyjną (0643, 0644 poprzez odłącznik; 0644 z 4820-3,
  7200-1, 9884-1).
- 23-0648 (s. 10) rozjazdowe połączenia sieci szlakowych ze stacyjną.
- 23-0650 … 0653 (s. 11–14) połączenia sieci z przewodem rozdzielczym 3 kV (0651/0653 poprzez odłącznik – pośrednie / końcowe; 6605-8).
- 23-0655 (s. 15) połączenia sieci z odłącznikiem kabla zasilacza (6306-9, 9812-9; w 2CS150 dodatkowa 13-0654).
- 23-0656 (s. 16) połączenia przewodów rozdzielczych 3 kV z kablem zasilacza poprzez odłącznik (4820-3, 6306-6, 7200-1, 9812-6;
  w 2CS150 dodatkowe 657–659).
- Części tom 5: 6601 zaciski zaprasowywane – przewód jezdny (s. 180), 6605 – lina nośna (s. 181), 6316 końcówka rurkowa do liny 185
  (s. 152).

## Iskiernik niskonapięciowy („ogranicznik niskonapięciowy” z notatek uszynienia)
- 53-0831 (tom 2 s. 95) uszynienia z iskiernikiem niskonapięciowym J-250: 7350-1 iskiernik (BN-82/9319-02, tom 5 s. 209) na uchwycie
  6333-2 ×2 przy konstrukcji (wyk. 1 słup betonowy, wyk. 2 stalowy, ścianka max 20 mm), 6306-4, linka 5850-1 (Cu50 w izolacji, tom 5
  s. 112) w dół i łukiem pod szyną do zacisku szynowego 5680-1 (tom 5 s. 106). 6351 słupek do mocowania iskiernika (tom 5 s. 162).
- Dział 0830–0839 – zespoły uszynień z iskiernikiem; tu wchodzi przewód od 5180-07 na kotwieniu uszynienia (notatka 16.11).
