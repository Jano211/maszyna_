# Zasilanie sieci: odłączniki, odgromniki, połączenia elektryczne, iskierniki – klocki z katalogów (rozeznanie 2026-11-16)

Źródła (dysk użytkownika `E:\SYF\INNE\PDF\Katalog ESN\`): tom 2 `z- KATALOG 2 -z.pdf` (dział 0600–0699 i 0800–0899, strony PDF
w nawiasach), tom 1 `z- KATALOG 1 -z.pdf` (dział 0500: zamocowania odłączników), tom 5 `z- KATALOG 5 -z.pdf` (części). Skany –
tabele czytane okiem / OCR, numery do potwierdzenia przy modelowaniu. Uzupełnienie nr 1: karty 13-0547, 13-0549 (zamocowania
odłączników z napędem silnikowym).

## Plan (zaakceptowany przez użytkownika 16.11)
- Podstacja = zasilacz (jak dziś, `Powergrid`) + model w scenerii; stawiana i edytowana w edytorze sieci.
- Odłącznik sekcyjny = model na słupie (zespół 0661…0677 + zamocowanie 0541…0549) ze stanem otwarty/zamknięty; zamknięty zwiera
  dwie sekcje (przęsła po obu stronach dostają jeden zasilacz), otwarty rozdziela.
- Odgromnik = sam model (0681…0684), bez wpływu na prąd.
- „Opus magnum” użytkownika: wyjść z kabiny, podejść do słupa i przełączyć odłącznik (korba / drążek w skrzynce napędu) – też pod
  przyszły multiplayer. Do zaprojektowania: interakcja z obiektem scenerii poza kabiną (co MaSzyna dziś umie w trybie pieszym).
- Użytkownik 17.11 sprawdzi, czy zasilacze za izolatorami sekcyjnymi faktycznie działają (pantograf bierze napięcie z zasilacza przęsła).

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
