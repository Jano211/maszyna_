# Edytor: co nowego i co można wziąć osobno

Spis zmian w edytorze z branchy Jana (Jano211/maszyna_), dla każdego, kto chce wziąć do siebie tylko część.
Najnowsze na górze. Przy każdej zmianie: co widać w edytorze, czego wymaga i jak ją wziąć.

## Jak brać pojedyncze zmiany

- Od 2026-11-01 do 2026-11-03 nowe funkcje miały osobne branche `funkcja/<nazwa>`: pull request z niego bierze tylko ją
  (plus to, co wpisane w „Wymaga”). Od 2026-11-04 zmiany sieci znowu idą pojedynczymi commitami na `siec-gl46`.
  Wszystko razem jest w `wszystko-gl46`.
- Starsze zmiany (do 2026-10-31) są pojedynczymi commitami na `siec-gl46`: `git cherry-pick <hash>`, w kolejności
  z sekcji „Wymaga”.
- Tłumaczenia (`lang/editor_pl.po`) były dopisywane na końcu pliku: przy cherry-picku nie po kolei konflikt na końcu
  pliku rozwiązuje się, biorąc obie strony. Nowe wpisy idą przy pokrewnych tekstach, więc konfliktów będzie mniej.
- Zmiana pól w `application/editormode.h` oznacza przebudowę prawie całego edytora (ok. 30 min).

## 2026-11-07

Commity na `siec-gl46`.

### Edytor sieci: wykonania wysięgników pod liczbę przewodów jezdnych sieci — commit o tym tytule
- Co widać: lista „Wykonanie” pokazuje tylko wykonania z ramionami na tyle przewodów jezdnych, ile ma sieć łańcucha (albo panelu
  przy nowym łańcuchu); przy zmianie sieci wykonanie panelu przechodzi na pasujące. Słupy już stojące z ramionami na inną liczbę
  przewodów: czerwona uwaga w widoku i w panelu słupa (przycisk „Wykonanie N”), w panelu łańcucha licznik i przycisk „Wykonania
  łańcucha pod jego sieć”. Samo nic się nie zmienia.
- Dane: `traction_catalog.txt`, wpisy `arms <1|2> <zestawy>` (po szerokości modeli ramion wzdłuż toru: wąskie – jeden, szerokie
  i pary różnej długości – dwa). Pary rozjazdowe tej samej długości (6203-1111, 6206-1111, 6206-1212, 6207-0303) jeszcze
  nieopisane – pasują do każdej sieci. Karty bez ramion (120x) też.
- `pick_card(..., Contacts)`: wykonania z ramionami na inną liczbę przewodów pomijane; gdy żadne nie pasuje – komunikat.
- Dobór innego wykonania tej samej karty: najpierw ten sam zestaw ramion (6201-01 ↔ 6201-02), potem ta sama hk – ZAŁOŻENIE.
- Zmienia: `world/catenary_catalog.h/.cpp` (przebudowa kilku plików przez `catenary_scene.h`), `application/editormodecatenary.h/.cpp`,
  `lang/editor_pl.po`, `tests/catenary/data/traction_catalog.txt`, test `tk`. Wymaga: oba pliki na G: (wgrane).

## 2026-11-06

Commity na `siec-gl46`.

### Sieć trakcyjna: mocowania na słupach bramek 311x przy ścianie nogi — commit o tym tytule
- Co widać: wysięgniki na słupach bramek palowych 3111–3113 nie stoją już 2 cm przed ścianą nogi (z obu stron toru).
- Dane `traction_supports.txt`: 311x `offset 0.121` → `0.101` (C200, ściana 0,100 od osi wzdłuż dźwigara, przekrój modelu na
  wysokości mocowań); 312x `width 0.200` → `0.240` (C240, ściana 0,120; offset był dobry). `offset` wyznacza też koniec dźwigara.
- Zmienia: `tests/catenary/data/traction_supports.txt`. Wymaga: kopia na `G:\Maszyna2510\data\` (wgrana 06.11).

### Edytor sieci: słup kotwowy na główce szyny, Shift+A – kotwienie do stojącego słupa — commit o tym tytule
- Co widać: „Zakotwicz koniec/początek łańcucha” stawia słup kotwowy na wysokości główki szyny toru przy nim. Wcześniej brał
  wysokość z ostatniego słupa łańcucha – przy zwisie bramki to spód dźwigara, więc słup lądował kilka metrów w górze. Suwaki
  położenia słupa kotwowego też stawiają go na główce szyny (na spadku idzie za torem). Słup już postawiony za wysoko: wystarczy
  ruszyć suwak.
- Shift+A: koniec łańcucha (ciągniętego albo przy zaznaczonym pierwszym/ostatnim słupie) kotwiony do stojącego słupa – LPM na
  słup, Esc anuluje. Same A i D zostają wolne (pomyłki).
- Zmienia: `application/editormodecatenary.cpp`, `lang/editor_pl.po`. Wymaga: nic.

### Edytor sieci: menu Widok > Sieć trakcyjna – przełączniki podglądów — commit o tym tytule
- Co widać: w górnym menu Widok podmenu „Sieć trakcyjna” z przełącznikami wszystkiego, co rysuje nakładka sieci: słupy, bramki,
  kotwienia, przęsła, kolory przęseł wg katalogu, długości przęseł, uwagi Iet-107, odsuwy w widoku, pantograf, wstawianie (oś
  toru i licznik przęsła); „Pokaż wszystko” / „Ukryj wszystko”. Zaznaczone i wskazane rzeczy rysują się zawsze.
- Checkboxy „Pantograf” i „Odsuwy w widoku” w panelu przełączają to samo co menu. Na czas sesji (bez zapisu w ustawieniach).
- Menu to wolna funkcja `catenary_view_menu()` z `editormodecatenary.cpp` – bez zmiany `editormode.h` (przebudowa tylko dwóch
  plików). Pola `pantograph` i `show_staggers` w `editormodecatenary.h` już nieużywane (do usunięcia przy okazji).
- Zmienia: `application/editormodecatenary.cpp`, `application/editormode.cpp`, `lang/editor_pl.po`. Wymaga: nic.

### Sieć trakcyjna: linie przęsła nie wystają z brył przy linkach Y — commit o tym tytule
- Co widać: z bliska przy niektórych podwieszeniach Y z jednej strony wystawały linie (GL_LINES) obok brył drutów.
- Przyczyna: kształt przęsła (zwis, połówka linki Y) zależy od sąsiada, a połówka Y sąsiada od jego drugiego sąsiada. Po zmianie
  odświeżane były linie tylko najbliższych sąsiadów – przęsło dwa dalej zostawało ze starą połówką Y, a bryły liczone są na świeżo.
- Teraz: linie odświeżane dwa przęsła w każdą stronę (`refresh_around`); gdy przęsło ma bryły (z bliska), linii się nie rysuje.
- Zmienia: `world/catenary_scene_objects.cpp`, `rendering/common/scenewalk.cpp`. Wymaga: nic.

### Edytor sieci: rodzina wybrana ręcznie zostaje, „Karty dla” domyślnie wg toru — commit o tym tytule
- Co widać: zaznaczony słup z rodziną (albo kartą) inną niż ta, którą miejsce przy torze daje samo, pokazuje w panelu swoją
  rodzinę i kartę – zmiana innego ustawienia już ich nie cofa do „wg toru i odsuwu” / „wg A”.
- „Karty dla”: przy zaznaczeniu „wg toru”, chyba że karta słupa jest na drugą wersję (karta prostej na łuku albo odwrotnie).
  Po odznaczeniu i po postawieniu słupa wraca na „wg toru” – ręczny wybór dotyczy tylko tego jednego słupa.
- Zmienia: `application/editormodecatenary.cpp` (catenary_take_settings, catenary_place, render_catenary_ui). Wymaga: nic.

### Edytor sieci: słup kotwowy od -30 do +30 m od osi toru — commit o tym tytule
- Co widać: suwak „Słup kotwowy od osi toru” w panelu kotwienia ma zakres -30…+30 m, także 0 (słup w osi toru). Plus – po
  stronie ostatniego słupa łańcucha, minus – po drugiej stronie toru.
- Odległość liczona od toru, nad którym kończy się łańcuch (najbliższy przewodowi przy ostatnim słupie, albo dołączony do niego,
  gdy słup kotwowy jest za jego końcem), a nie od najbliższego toru – suwak nie skacze, gdy słup przechodzi nad sąsiednimi torami.
- Zmienia: `application/editormodecatenary.cpp`, `lang/editor_pl.po`. Wymaga: nic.

### Edytor sieci: izolatory wysięgnika tylko pasujące — commit o tym tytule
- Co widać: w „Izolatory tego wysięgnika” przy każdym izolatorze z karty tylko te, które tam pasują: teowniki 7010 ↔ 7110,
  7020 ↔ 7120; rurowe 7100 / 7010 / 7020 w wykonaniu -01, -02 (tworzywo) albo -25 (25 kV) – te, które są w katalogu (-02
  pojawi się samo po dopisaniu części). Izolator bez zamiennika nie ma listy.
- Override sprzed zmiany, który tu nie pasuje, widać na liście jako „(tu nie pasuje)” – można wrócić do karty.
- Zmienia: `application/editormodecatenary.cpp`, `lang/editor_pl.po`. Wymaga: nic.

### Sieć trakcyjna: przęsło przebudowywane w miejscu, bez przesyłania całej sekcji na kartę — commit o tym tytule
- Co widać: suwaki kotwienia (wysokość rolek, odległość słupa) i przeciąganie słupów nie zbijają klatek.
- Przyczyna: przebudowa przęsła usuwała je i robiła nowe. Nowy fragment w banku geometrii sekcji (i opróżnienie starego) w
  rendererze GL 4.6 kasuje bufor całej sekcji – co klatkę szła na kartę cała sekcja (tory, druty, linie). Do tego każde nowe
  przęsło zakładało nowy bank brył, a banków nikt nie usuwa.
- Teraz `catenary_span_reload` (world/catenary_scene_objects.cpp) wczytuje istniejące przęsło od nowa: ten sam obiekt, jego
  fragment linii zastąpiony (przy tej samej liczbie wierzchołków na kartę idzie tylko on), bank brył zostaje. Gdy przęsło
  przechodzi do innej sekcji, zmienia nazwę, bieżnię wspólną albo zasilacz – po staremu (usunięcie i nowe).
- Banki brył usuniętych przęseł wracają do puli i są brane przez następne.
- Zmienia: `world/catenary_scene.cpp`, `world/catenary_scene_objects.cpp`, `world/catenary_span.cpp`, test `te`. Wymaga: nic.

## 2026-11-04

Commity na `siec-gl46`.

### Edytor torów: wskazywanie toru nie przelicza całej scenerii co klatkę — commit o tym tytule
- W widoku perspektywicznym wskazywanie toru pod kursorem próbkowało co klatkę każdy tor scenerii (do 64 rzutów na ścieżkę) –
  patrząc wzdłuż dużej scenerii (np. Kielce → Częstochowa) klatki spadały do zera. Teraz najpierw rzut czterech punktów krzywej
  (krzywa leży w ich otoczce): gdy prostokąt otoczki na ekranie z marginesem wskazywania mija kursor, ścieżka jest pomijana.
- Przy okazji podgląd pantografu najpierw odrzuca ścieżki daleko od kamery tanim testem, bez liczenia długości.
- Zmienia: `application/editormodetrackux.cpp`, `application/editormodecatenary.cpp`. Wymaga: nic.

### Edytor sieci: podgląd ślizgu pantografu nad torami — commit o tym tytule
- Strona Druty, przełącznik „Pantograf”: nad torami do 250 m od kamery co 2 m ślizg na wysokości najniższego przewodu jezdnego,
  który by złapał (jak `vehicle_table::update_traction`), w osi toru przechylony z przechyłką. Pole pracy 1,45 m niebieskie, cały
  ślizg 1,95 m szary; gdzie przewód wychodzi poza pole pracy – czerwony odcinek i kropka w miejscu przewodu.
- ZAŁOŻENIE: pole pracy 1,45 m (EN 50367 dla ślizgu 1950 mm). Działa też dla starych drutów (node traction).
- Zmienia: `application/editormodecatenary.h/.cpp`, `lang/editor_pl.po`. Wymaga: nic.

### Edytor sieci: dowolny izolator w każdej roli wysięgnika — commit o tym tytule
- Panel wysięgnika: rozwijane „Izolatory tego wysięgnika” – każdy izolator z karty (ukośnika, odciągu, wysięgu, na teownikach
  7110/7120) można zamienić na dowolny izolator z katalogu (wpis `override <rola> <część>`), także rurowe na teowniku.
- Rura / pręt pod podmienionym izolatorem dobierane z rodziny części jak przy zestawie 25 kV (wcześniej tylko przy zestawie).
- Zmienia: `world/catenary_scene.cpp`, `application/editormodecatenary.cpp`, `lang/editor_pl.po`, test `te`. Wymaga: nic.

### Sieć trakcyjna: obrót wysięgników od brakujących ciężarów — commit o tym tytule
- W łańcuchu z kotwieniem środkowym brakujące ciężary skracają sieć po stronie swojego kotwienia (skrócenie z urządzenia:
  sprężystość i zwis). Wysięgniki po tej stronie obracają się w stronę kotwienia środkowego proporcjonalnie do odległości od
  niego, razem z obrotem od temperatury. Ciężary na gruncie – nie liczymy (sieć bez naciągu).
- Zmiana kotwienia, która zmienia naciąg, składa od nowa wysięgniki łańcucha; przy zmianie temperatury kotwienia łańcucha
  liczone są przed wysięgnikami.
- Zmienia: `world/catenary_scene.cpp`, test `te`. Wymaga: nic.

### Edytor sieci: lista zestawów izolatorów zamiast „Izolatory 25 kV” — commit o tym tytule
- W panelu wysięgnika lista „Izolatory”: zwykłe + zestawy, które katalog wysięgników ma dla co najmniej dwóch rodzin izolatorów
  (części NNNN-<zestaw>; dziś 25 kV). Nowe zestawy (plastikowe) pojawią się same po dopisaniu części.
- Zmienia: `world/catenary_scene.h/.cpp`, `application/editormodecatenary.h/.cpp`, `lang/editor_pl.po`, test `te`. Wymaga: nic.

### Sieć trakcyjna: bieżnia wspólna nad rozjazdami i ręcznie dla przęsła — commit o tym tytule
- Pantograf trzymał się przewodu przęsła, póki ten był pod ślizgiem – nad rozjazdem jechał w górę za przewodem uciekającym
  do kotwienia i dopiero za ślizgiem przeskakiwał na niższy („prawie łamie”, zależnie od kierunku). Przęsło z bieżnią wspólną
  (`parallel`) każe mu co chwilę szukać najniższego przewodu.
- Bieżnia wspólna wykrywana sama także tam, gdzie przewód innego łańcucha zbliża się w planie na 0,98 m (pół ślizgu 1,95 m) przy
  różnicy wysokości do 1 m (rozjazdy, skrzyżowania sieci) – również w przęsłach do kotwień (wcześniej pomijanych).
- Panel łańcucha przy słupie: „Bieżnia wspólna” dla przęsła za słupem (i od kotwienia początkowego przy pierwszym słupie):
  samo / zawsze / nigdy; wpis łańcucha `shared <podwieszenie|start> on|off`. „Zawsze” bez drugiego przęsła obok łączy przęsło
  samo ze sobą (pantograf i tak szuka).
- Zmienia: `world/catenary_suspension.h/.cpp`, `world/catenary_scene.cpp`, `world/catenary_scene_objects.cpp`,
  `application/editormodecatenary.h/.cpp`, `lang/editor_pl.po`, test `te`. Wymaga: nic.

### Edytor sieci: dolne mocowanie przesunięte i obrót ręczny wysięgnika — commit o tym tytule
- Panel słupa: „Dolne mocowanie przesunięte” (tryb ekspert albo gdy ustawione) – ±0,50 m jak „Górne mocowanie przesunięte”,
  solver dobiera ukośnik z tej samej rodziny części; wpis `strut_shift <m>`.
- „Obrót ręczny” – wysięgnik obrócony na zawiasie wokół pionu o ±30°, dodawany do obrotu od temperatury (podpowiedź pokazuje
  łączny kąt); wpis `turn_by_hand <°>`. Bez zawiasu (wpis bez słupa) nic nie robi. Kontrola 12° dotyczy tylko części od temperatury.
- Zmienia: `world/catenary_suspension.h` (przebudowa części sieci), `world/catenary_suspension.cpp`, `world/catenary_scene.cpp`,
  `application/editormodecatenary.h/.cpp`, `lang/editor_pl.po`, test `te`. Wymaga: nic.

### Edytor sieci: przesuwanie kotwienia bez przebudowy całego łańcucha — commit o tym tytule
- Suwaki wysokości kotwienia i przeciąganie słupa kotwowego przebudowywały co klatkę wszystkie przęsła łańcucha (solver
  wieszaków i linek Y), stąd spadek klatek tylko w trakcie przesuwania. Teraz, gdy naciąg od kotwienia się nie zmienia
  (ten sam stos, ciężary nie na gruncie, ten sam zasięg), przebudowuje się tylko przęsło do kotwienia; zmiana naciągu
  (brakujące ciężary, wydłużenie, inne urządzenie) nadal przelicza cały łańcuch.
- Zmienia: `world/catenary_scene.cpp` (`catenary_anchor_update`, `follow_anchor`). Wymaga: nic.

### Sieć trakcyjna: kotwienie środkowe na nodze bramki trafia w ceownik — commit o tym tytule
- Lina kotwienia środkowego (0406) kończyła się w osi nogi bramki (środek kratownicy). Teraz tak jak kotwienie końcowe na
  bramce: uchwyty na zewnętrznej ścianie ceownika bliższego słupowi środkowemu (czoło ½ `length` masztu, oś prostopadle
  do dźwigara), uchwyty obejmują półkę ceownika (75 / 85 mm – ZAŁOŻENIE jak przy kotwieniu końcowym).
- Zmienia: `world/catenary_scene.cpp` (`assemble_middle`). Wymaga: nic.

## 2026-11-01

Branch `funkcja/drobnica-lod` (od `b8f2fc1`).

### Sieć trakcyjna: drobnica nie znika po kilkudziesięciu metrach — commit o tym tytule
- Renderery GL4.6 i Vulkan pomijały modele mniejsze niż 0,003 wysokości ekranu (izolator 4050 znikał po ok. 50 m). Części sieci
  trakcyjnej są z tego zwolnione (flaga `instance_keep_small` w rekordzie instancji), znikają według własnych zasięgów LOD.
- Zmienia: `scene/scene.h` (przebudowa sporej części), `rendering/common/scenewalk.cpp`, `shaders/ogl46/instance_cull.comp`,
  `shaders/vk14/instance_cull.comp` (do `shaders/` obok exe). Wymaga: nic.

Branch `funkcja/polbramki-1901` (od `eb72be7`).

### Edytor sieci: na półbramce 1901 domyślnie zwis 4840 — commit o tym tytule
- Automatyczny dobór zwisu bierze rodziny w kolejności z wpisu dźwigara (`drop 4840,ESN-544` – najpierw 4840), w rodzinie
  najkrótszy, który sięga do mocowań; wcześniej najkrótszy ze wszystkich rodzin (ESN-544-1, 1,65 m). Wybór ręczny bez zmian.
- Zmienia: `application/editormodecatenary.cpp`. Wymaga: nic.

### Sieć trakcyjna: półbramki 1901 – zwisy ESN-544 obrócone, wysięgniki na nodze — commit o tym tytule
- Zwisy ESN-544 stały obrócone o 90° względem 4840 / 4850: dostały `turn 90` jak tamte (modele bez obrotu w t3d, przekrój ułożony
  jak 4840).
- Noga półbramki 1901 (słup przestrzenny, gabaryt 600) ma mocowania wysięgników wyk. 04 (4610-04 / 4620-04 / 4630-04 – karta 0361
  wyk. 4, jak słupy bramek): da się na niej postawić wysięgnik i odciąg. Słupy 1901 w katalogu edytora w grupie g_1901.
- Zmienia: dane `traction_supports.txt`, `traction_catalog.txt` (do `data/`). Wymaga: nic.

Branch `funkcja/odsuw-i-tor` (od `2ccb42f`).

### Edytor sieci: przepięcie wysięgnika na inny tor — commit o tym tytule
- Panel zaznaczonego słupa: „Przepnij na inny tor”, potem LPM przy torze (Esc anuluje). Wysięgnik wstawiony przy rozjeździe do
  złego toru (np. dla toru zasadniczego, a przyklejony do zwrotnego) przechodzi na kliknięty tor: słup zostaje, karta i odsuw
  liczą się od osi tego toru, przeciąganie wzdłuż toru i suwaki panelu też idą już po nim. Ctrl+Z cofa.
- Zmienia: `application/editormodecatenary.*`, `lang/editor_pl.po`. Wymaga: nic.

### Edytor sieci: odsuw ±0,5 nie przełącza krótkiego wysięgnika na długi — commit o tym tytule
- Suwak odsuwu w trybie ±0,5 (tryb eksperta) przesuwa tylko przewody: przejście przez oś toru nie zmienia już karty z krótkiego na
  długi (i odwrotnie). Krótki / długi zmienia się ręcznie: „Przewody do słupa” albo X. Przy zaznaczeniu słupa krótki / długi bierze się
  z rodziny jego karty, a nie ze strony, po której są przewody; przewody po drugiej stronie osi niż karta – suwak od razu w trybie ±0,5.
  Wyłączenie ±0,5 przy przewodach po „złej” stronie przestawia kartę za przewodami.
- Zmienia: `application/editormodecatenary.cpp`, `lang/editor_pl.po`. Wymaga: nic.

Branch `funkcja/wczytywanie-sieci` (od `e0e1eeb`).

### Sieć trakcyjna: łańcuchy z plików scenerii nie ginęły przy wczytaniu — commit o tym tytule
- Sieć zapisana wprost w pliku warstwy (wpisy `node … suspension/chain`, a nie osobny plik `catenary … endcatenary`) po
  ponownym wczytaniu stała bez drutów: słupy i wysięgniki były, łańcuchy odrzucone z błędem „chain refers to suspension … which
  isn't in the file”. Wpisy scenerii są rozbierane po jednym, a parser sprawdzał podwieszenia łańcucha tylko w jego własnym wpisie.
  Teraz sprawdza je dla wszystkich wpisów pliku naraz, przy dodaniu do sceny.
- Uszynienia (`earthing`) z pliku scenerii wczytywanego przed FirstInit nie trafiały do sceny – są.
- Odciągi sieci w łańcuchu przy wczytaniu składały się dwa razy (raz przez łańcuch, raz przy powiązaniu scenerii) – teraz raz.
- Zmienia: `world/catenary_suspension.*` (`check_chain_references`, parametr `References`), `world/catenary_scene.cpp`, test edytora
  (zapis wpisów warstwy i wczytanie drogą scenerii). Wymaga: nic.

### Edytor sieci: słup odciągu do 50 m od osi toru — commit o tym tytule na `funkcja/odciagi-sieci`
- Przy grupie „Odciągi sieci” suwak „Czoło słupa od osi toru” sięga do 50 m (skala logarytmiczna), pasmo A kart odciągów
  1–55 m. Lina odciągowa prosto od słupa; obniżenie uchwytu na słupie dla a > 2,55 m zostaje 0,20 m (koniec tablicy Głowackiego).
- Zmienia: `application/editormodecatenary.cpp`, dane `traction_cantilevers.txt` (do `data/`). Wymaga: `7c1fec5`.

### Sieć trakcyjna: odciągi wielotorowe (0315/0316, 0325/0326) i na bramkach — commit o tym tytule na `funkcja/odciagi-sieci`
- Panel zaznaczonego odciągu: „Odciąg dalej na sąsiedni tor” – LPM przy następnym torze. Ten odciąg staje się pośrednim
  (0311/0313 → 0315, 0312/0314 → 0316, 0321/0323 → 0325, 0322/0324 → 0326), następny staje bez słupa, na wprost niego, z wpisem
  `pull_from <poprzedni>`: pojedynczy ciągnie linę od liny nośnej poprzedniego (zacisk 5640), podwójny – dolną od jego uchwytu ramion,
  górną od liny nośnej (Głowacki rys. 13.8). Z izolacją – izolator 7020 na początku liny, czyli między torami. Klik w przęśle
  łańcucha tamtego toru wstawia odciąg do łańcucha. Przesunięcie poprzedniego przestawia następne.
- Odciągi na bramkach: na słupie bramki (4630-04, karta 0361 wyk. 4) i na zwisie (4630-05, wyk. 5) składają się tak samo jak na słupie.
- Zmienia: `world/catenary_suspension.*` (`pull_from`), `world/catenary_scene.cpp` (`pull_start`), `application/editormodecatenary.*`,
  `lang/editor_pl.po`, dane `traction_cantilevers.txt` i `traction_catalog.txt` (do `data/`), test. Wymaga: poprzedni commit odciągów.

Branch `funkcja/odciagi-sieci` (od `dab19e5`).

### Sieć trakcyjna: odciągi sieci (karty 0311–0314, 0321–0324) — commit o tym tytule
- Nowa kategoria w katalogu edytora „Odciągi sieci”: słup, który sieci nie podwiesza, tylko odciąga ją w bok (łuki, na zewnątrz
  łuku co drugi / co trzeci słup). Wstawia się go jak zwykły słup łańcucha, rodzina = karta (pojedynczy / podwójny, z izolacją
  albo bez, ramiona proste / łukowe), wykonanie 1 – sieć dwuprzewodowa, 2 – jednoprzewodowa.
- Złożenie: lina odciągowa od uchwytu 4630 na słupie (jak wysięgu pomocniczego) przez uchwyt ramion 4270 do liny nośnej, ramiona
  odciągowe (6204/6202, łukowe 6205/6203) od uchwytu do przewodów; z izolacją – izolator 7020 przy słupie. Uchwyt na słupie
  obniżony pod przewód o b z tablicy Głowackiego (rys. 13.7: a 2,20–2,55 m → b 0,14–0,20 m). Podwójny: druga lina z drugiego
  uchwytu 4630 na wysokości liny nośnej, łącznik pionowy między linami.
- Lina nośna w miejscu odciągu wisi wg całej rozpiętości między podwieszeniami (odciąg nie niesie sieci): solver liczy całą
  rozpiętość i odczytuje wysokość liny. Przęsła po obu stronach odciągu dostają dopisek `pull <początek> <całość>`: wieszaki z
  tablicy całej rozpiętości, przy odciągu bez linki Y. Exe bez tej zmiany pominie dopisek i policzy przęsła osobno.
- ZAŁOŻENIA: a = czoło słupa od przewodu + 0,30 m (osi toru solver nie zna); lina załamuje się w uchwycie 4270; model 4270 zerem
  w osi liny, lina wzdłuż Y; lina 9811-3 modelem 4490-00 (jak 9814-3); łącznik w podwójnym 0,9 m od słupa. Bez modeli: końcówka
  liny na linie nośnej 6303-7, zaciski 5270-3, kausze 6301-3. Odciągi pośrednie / wielotorowe (0315, 0316, 0325, 0326) – później.
- Zmienia: `world/catenary_cantilever.*` (`pulloff`, `mount upper`, `rules pulloff_drop`), `world/catenary_scene.cpp`,
  `world/catenary_span.*`, `world/Traction.cpp` (dopisek `pull`), dane `traction_cantilevers.txt` i `traction_catalog.txt`
  (do `data/`), testy. Wymaga: nic.

Branch `funkcja/zastrzal-6506` (od `520072f`).

### Sieć trakcyjna: zastrzał 6506 jako opcja wysięgników rurowych — commit o tym tytule
- Panel słupa (karty 7xx, 10xx, 11xx, 12xx, bez 102x, które mają go same): „Zastrzał 6506 (jak w 102x)” – płaskownik od obejmy
  4210-02 na wysięgu pomocniczym do uchwytu 4760-01 na ukośniku, tam gdzie odstęp między nimi pasuje. Zostaje przy zmianie
  karty; Ctrl+Z cofa.
- We wpisie to trzy nadpisania ról (`override link 6506-01 override link_clamp 4760-01 override link_cord_clamp 4210-02`),
  exe bez tej zmiany składa wysięgnik bez zastrzału. Solver po zwykłym złożeniu dokłada złożenie `brace`
  z `traction_cantilevers.txt`; gdy płaskownik się nie mieści (ukośnik daleko od wysięgu pomocniczego, uchwyt poza rurą),
  wysięgnik zostaje bez niego, z jedną uwagą.
- Wchodzi w 448 z 574 wariantów (karta × wykonanie) z ukośnikiem i wysięgiem pomocniczym; nie wchodzi w 70x, 73x, 74x, 77x,
  78x, 1110, 1120, 1130, 1140, 1210–1240, 1260, 1280.
- Zmienia: `world/catenary_cantilever.cpp`, `application/editormodecatenary.cpp`, `lang/editor_pl.po`, dane
  `traction_cantilevers.txt` (do `data/`), testy. Wymaga: nic.

### Edytor sieci: przeciągane słupy się nie sklejają — commit o tym tytule na `siec-gl46` (poprawka)
- Słup wsunięty przy przeciąganiu w drugi słup tego samego typu (do 5 cm) był brany za jeden słup z nim (jak parasol)
  i dalej ciągnął oba. Teraz to, co stoi na słupie (parasol, rozstawione, słup dzielony), jest ustalane przy chwyceniu,
  a przeciągany słup nie stanie bliżej niż 0,5 m od innego (status „Tam stoi inny słup”). Słupy sklejone wcześniej
  rozjeżdżają się zwykłym przeciągnięciem.
- Zmienia: `application/editormodecatenary.*`, `lang/editor_pl.po`. Wymaga: nic.

### Edytor sieci: przeciągany słup przechodzi na dołączony odcinek toru — commit o tym tytule na `siec-gl46` (poprawka)
- Przy wciśniętym przycisku edytor nie szuka toru pod kursorem, więc słup przeciągany za koniec swojego odcinka stawał na
  jego końcu. Teraz przechodzi na tor dołączony w tym miejscu (`trNext` / `trPrev`, do 8 odcinków dalej), po tej samej stronie
  w terenie (także gdy dołączony odcinek biegnie w drugą stronę). Tory równoległe niedołączone nie są brane – słup między
  torami nie przeskakuje na drugi tor.
- Zmienia: `application/editormodecatenary.cpp`. Wymaga: nic.

Branch `funkcja/plik-sieci` (od `0de40d4`).

### Edytor sieci: plik sieci zapamiętany, widoczny w warstwach, przenoszenie ciągu do innego pliku — commit o tym tytule
- Plik sieci podany w panelu („Plik sieci”) jest zapamiętany dla scenerii w `eu07_editor.ini` (`catenary_file@<sceneria>`);
  „wg warstwy” go kasuje. Po ciągnięciu łańcucha od słupa z innego pliku edytor wraca do podanego pliku (wcześniej zostawał
  w pliku tamtego łańcucha i następne przęsła szły nie tam).
- Panel warstw: pod każdą warstwą jej pliki sieci („sieć: …”, * = zmiany do zapisania), plik, do którego idą nowe słupy,
  z dopiskiem „[tu idzie nowa sieć]” (przy sieci wpisanej wprost w plik warstwy – dopisek przy warstwie). Klik w plik sieci
  robi go docelowym. Pokazuje stan z edytora sieci (zakładka sieci otwarta choć raz).
- Panel zaznaczonego słupa: „Przenieś do innego pliku” – słup razem ze wszystkim, co jest z nim powiązane nazwami w jego pliku
  (łańcuchy, ich podwieszenia, kotwienia, bramki z kotwieniami na nogach, uszynienia), idzie do wybranego / wpisanego pliku;
  nazwy zajęte w pliku docelowym dostają dopisek `_2`, `_3`… Podpowiedź przycisku mówi, co pójdzie. Ctrl+Z tego nie cofa
  (historia cofania jest czyszczona) – przenosi się z powrotem tym samym przyciskiem.
- Zmienia: `editor/editorSettings.*` (nagłówek – przebudowa ok. 20 plików), `world/catenary_scene.*`
  (`catenary_scene_move`, `catenary_scene_target*`), `application/editormodecatenary.*`, `application/editoruipanels.cpp`,
  `lang/editor_pl.po`, test edytora. Wymaga: nic.

### Sieć trakcyjna: kotwienie na nodze bramki na ceowniku — commit o tym tytule na `siec-gl46` (poprawka)
- Uchwyty kotwienia na nodze bramki (4520-4/-5, dół 4560) stały w środku kratownicy. Teraz siedzą na zewnętrznej ścianie
  ceownika bliższego kotwionemu przęsłu: ½ `length` masztu (rozstaw ceowników, 0,30 m) od osi nogi wzdłuż toru.
- Półka ceownika (75 / 85 mm) dobierana z `width` masztu (wysokość ceownika); wcześniej zawsze 75 mm.
- Zmienia: `world/catenary_scene.cpp` (`assemble_anchor`), test edytora. Wymaga: `c9b6761`.

Branch `funkcja/tekstury-sieci` (od `f02b7d2`).

### Sieć trakcyjna: wymienne tekstury wysięgników i słupów — `45fb061`
- Panel słupa: „Tekstura wysięgnika” (textures/siec_trakcyjna/siec*) i „Tekstura słupa” (wg rodziny, np. 161x_*), przycisk
  „Te tekstury na całym ciągu” (Ctrl+Z cofa). Cały wysięgnik (rury, zamocowania, izolatory, siodła, uziemienia) ma jedną
  teksturę. Słup: `pole_skin` z wpisu, inaczej `skin` z katalogu słupów, inaczej „none”; fundamenty zawsze „none”.
- Modele components/, słupy 161x i 1701 muszą mieć „Map: -1” (u Jana przerobione). Sceneria wstawiająca je ręcznie
  z teksturą „none” zobaczy je bez tekstury.
- Zmienia: `world/catenary_cantilever.*`, `world/catenary_suspension.*`, `world/catenary_scene.cpp`,
  `application/editormodecatenary.cpp`, `lang/editor_pl.po`, dane `traction_supports.txt` (do `data/`). Wymaga: `49a3d78`
  (wspólny plik edytora).

### Sieć trakcyjna: stan przewodów „czarne” — `49a3d78`
- Czwarty stan między przejściowym a zaśniedziałym: czarne, matowe przewody. Flaga 4 w pliku sieci (stare exe – jak nowe).
- Zmienia: `world/Traction.*`, `world/catenary_span.cpp`, `application/editormodecatenary.cpp`, `lang/editor_pl.po`,
  dane `traction_materials.txt` (do `data/`). Wymaga: nic.

## 2026-10-31

### Sieć trakcyjna: uchwyty kotwień 4530, 4560, 4570 — `7318b3c`
- Nowe modele Jana w danych: dół prowadnicy na słupie betonowym 1701 – 4530-3 z tyłem 4530-4 (było 4520-3); dół prowadnicy na
  nogach bramek – 4560 wg ceownika (01 C240, 03 C200, 05 C160 i niekatalogowy C140), bez tyłu (śruby hakowe); kotwienie na
  półbramce 1901 (słup przestrzenny) – 4570-1, bez tyłu (w danych „-” zamiast części).
- Zmienia: `traction_tensioners.txt` (do `data/`), `world/catenary_tensioner.cpp` („-” w `brackets`), test. Wymaga: `c9b6761`.

### Edytor sieci: kotwienie na nodze bramki — `c9b6761`
- Kotwienie (ciężarowe albo stałe) można przepiąć na nogę bramki: w panelu kotwienia „Przepnij na istniejący słup”, potem
  klik w bramkę – urządzenie idzie na nogę bliższą kotwieniu, jego własny słup znika (Ctrl+Z cofa). W panelu „Na nodze N bramki X”.
  Przesunięcie, obrót i zmiana szerokości bramki przenoszą kotwienie razem z nogą.
- Uchwyty wg karty 0455: wyk. 4 – nogi z ceowników 160–200 (3111–3123, a także 3032–3038 – ZAŁOŻENIE), 4520-4, śruby 0,107;
  wyk. 5 – C240 (3031–3037), 4520-5, śruby 0,127; tyły 4520-8. Dół (4560-1) do czasu modelu jak góra. Uchwyty chwytają jeden
  ceownik (potwierdzone), szerokość jego półki 75 / 85 mm – ZAŁOŻENIE.
- W pliku sieci wpis kotwienia dostaje `gantry <bramka> <1|2>` (stare exe go nie zna – takie kotwienie pominie).
- Zmienia: `world/catenary_suspension.*`, `world/catenary_scene.cpp`, `application/editormodecatenary.cpp`, `lang/editor_pl.po`,
  dane `traction_tensioners.txt` (do `data/`). Wymaga: nic.

### Edytor sieci: wysięgniki na nogach bramki idą za nogami — `e695949`
- Zmiana dźwigara (czyli szerokości bramki), słupów, obrót i „prostopadle do toru” przesuwają nogi bramki; wysięgniki
  przyczepione do nóg zostawały w starym miejscu. Teraz idą z nogą i składają się od nowa: karta z nowej odległości
  nogi od toru, odsuw przewodów (zygzak) bez zmian. Gdy żadna karta nie sięga – wysięgnik zostaje, jak był, a status
  podaje, ile takich. Jedno Ctrl+Z cofa bramkę razem z wysięgnikami. Wysięgniki na zwisach z dźwigara – bez zmian.
- Zmienia: `application/editormodecatenary.cpp`, `lang/editor_pl.po`. Wymaga: nic.

### Edytor sieci: opis wykonania 2 wysięgników 120x — `5f9d936`
- Lista wykonań 120x pokazywała przy wyk. 2 „hk 1,70”, a karta (i dane złożenia) to hk 1,30. Poprawiony sam opis
  w `traction_catalog.txt` (na dysk do `data/`), geometria bez zmian. Wymaga: nic.

### Edytor sieci: konflikt ID ImGui w kotwieniu środkowym — `7702801`
- Nagłówek sekcji „Kotwienie środkowe” i lista wyboru słupa miały ten sam napis, więc ImGui brał je za jeden element
  (ostrzeżenie o konflikcie ID, lista mogła nie reagować). Lista ma teraz własne ID. Wymaga: nic.

### Sieć trakcyjna: YC120-2C150 na rolkach szeregowych, jeden stos — `d88c33a`
- Kotwienie „ciężary, rolki szeregowe” (karty 0471 Ø400 i 0480 Ø306) działa teraz także dla YC120-2C150 (typ 20) –
  wcześniej ta sieć miała tylko kotwienie rozdzielone (0482/0486: osobne stosy liny i przewodów). Wybór w panelu kotwienia,
  pole „Rodzaj”. Do sieci z lat 2010+ z Y na łukach (np. przęsła 40 m – YC120-2C150 daje Y od 35 m 14 m, krótsze 7 m).
- ZAŁOŻENIE (brak karty): 40 ciężarów × 27 kg × 1:4 = 4238 daN, dźwignia 6406 otwory I/IV wokół III (lina ~1534 daN,
  -3,4 % od 1588; przewody ~2704); 0471: 40 × 6322-1, pręt 2,50; 0480: 31 żeliwnych + 9 polimerobetonowych, pręt 2,80.
- Zmienia tylko dane (`tests/catenary/data/traction_tensioners.txt`, na dysk do `data/`) i test. Wymaga: `62c4b97`
  (dźwignia odwrócona – w typie 20 lina idzie do dolnego otworu).

### Sieć trakcyjna: dźwignia naprężacza w YpC120-2C, YpC95-2C, YwsC120-2C — `62c4b97`
- Dźwignia 6406 (ta ze śrubami rzymskimi i izolatorami) w sieciach, gdzie lina nośna idzie do dolnego otworu dźwigni
  (dźwignia odwrócona), stała tyłem na przód wzdłuż osi naprężacza. Teraz odwraca się o 180° wokół osi uchwytu, tak jak
  w rzeczywistości: otwory zamieniają się góra–dół, a strona z otworami dalej patrzy w stronę drutów. YC95-2C i inne
  nieodwrócone bez zmian. To samo dla drugiej dźwigni w urządzeniach z dwiema dźwigniami.
- Zmienia `world/catenary_tensioner.cpp`. Wymaga: nic.

### Edytor terenu: klawisze pędzla, osobny pędzel dla każdego narzędzia — `a2744c6`
- `[` / `]` – promień pędzla mniejszy / większy, `Shift+[` / `Shift+]` – siła; po 10 %, przytrzymanie powtarza.
- Rzeźbienie, wygładzanie, poziomowanie, malowanie, przywracanie, dziury i trawa mają każde swój promień i siłę,
  zapamiętywane w ustawieniach edytora (`eu07_editor.ini`, `terrain_brush_<nr>`).
- Wymaga: nic. Zmienia `editormode.h`.

### Edytor dróg: przełącznik przeciągania punktów, przechyłka — `ad74309`
- „Przeciąganie punktów” (opcje narzędzia Zaznacz i inspektor punktów) – domyślnie wyłączone: klik tylko zaznacza.
- „Przechyłka [%]” w zaznaczonych punktach (dodatnia – do wnętrza łuku) i „Według łuku” (v²/127R minus
  przyczepność, 2–7 %). Odcinek przechodzi liniowo między przechyłkami końców; przy skrzyżowaniach zero.
  Geometria dróg (Road.cpp) już to obsługiwała – doszło tylko narzędzie.
- Wymaga: `f12d539` (przeciąganie punktów). Zmienia `editormode.h`.

### Teren z NMT: pobocze za stopą podsypki — `e8596c3`
- Płaskie pobocze szerokości co najmniej oczka siatki terenu za stopą podsypki, dopiero potem skarpa (wcześniej teren
  pod stopą siadał o kilka–kilkanaście cm). Na istniejącym terenie: „Ukształtuj torowisko od nowa”.
- Wymaga: `bb0ed8b`.

### Teren: skarpa od krawędzi koryta — `8b982df`
- Poprawka błędu z 2026-10-11 (`c5ac698`): zaokrąglenie skarpy zaczynało się 1,5 m w głąb koryta i przy stopie
  podsypki na nasypie teren był ok. 25 cm za nisko. Zmienia `scene/heightmapterrain.cpp` (modyfikatory terenu).
- Wymaga: nic.

### Teren z NMT: koryto płycej, przebudowa torowiska — `bb0ed8b`
- „Więcej ustawień” w oknie NMT: „Koryto płycej o (m)” (gdy tekstura podsypki nie sięga do jej spodu) i
  „Ukształtuj torowisko od nowa” – przelicza koryta i skarpy na istniejącym terenie, bez robienia kafli od nowa.
- Wymaga: nic.

### Edytor: F7 – wireframe — `d0e05a8`
- F7 w edytorze przełącza widok siatki (jak w trybie jazdy z debugiem). Wymaga: nic.

### Teren z NMT: arkusze .asc z folderu — `22092b7`
- „Teren z arkuszy z folderu” w oknie NMT (pamiętane dla scenerii): arkusze ESRI ASCII pobrane ręcznie z Geoportalu,
  np. starsze do retroscenerii; PUWG 1992 i PUWG 2000 (strefy 5–8) rozpoznawane same, poprawka wysokości; gdzie
  arkuszy brak – z usługi jak zwykle. Teren już zrobiony zmienia się dopiero po zrobieniu od nowa.
- Wymaga: `c47af7b` (pole w `nmt_state`). Zmienia `editormode.h`, `editor/editorNmt.*`, `editor/editorSettings.*`.

### Teren z NMT: limit 10 m, przebudowa wokół kamery — `c47af7b`
- Miejsca torów ponad 10 m nad/pod modelem (tor zostawiony na wysokości 0, błędny punkt) nie ciągną terenu i nie
  dostają koryta; nazwy takich torów w logu.
- „Zrób od nowa teren pod torami wokół kamery” – tylko kafle wokół kamery (liczba z sekcji Kafle).
- Wymaga: nic. Zmienia `editormode.h`.

### Edytor dróg: przeciąganie punktów — `f12d539`
- LPM na punkcie styku/końca odcinków i przeciągnięcie przenosi go w poziomie; odcinki zachowują kierunki; jedno Ctrl+Z.
  Ramion skrzyżowań się nie przeciąga. Zmienia `editormode.h`.

### Edytor dróg: Alt – łagodny łuk bez prostowania — `18636b8`
- Przy budowie drogi z Alt łagodny łuk zostaje łukiem (bez Alt prawie prosty odcinek, poniżej 2 % rozpiętości, jest prostowany). Wymaga: nic.
