# Edytor: co nowego i co można wziąć osobno

Spis zmian w edytorze z branchy Jana (Jano211/maszyna_), dla każdego, kto chce wziąć do siebie tylko część.
Najnowsze na górze. Przy każdej zmianie: co widać w edytorze, czego wymaga i jak ją wziąć.

## Jak brać pojedyncze zmiany

- Od 2026-11-01 każda nowa funkcja ma osobny branch `funkcja/<nazwa>`: pull request z niego bierze tylko ją
  (plus to, co wpisane w „Wymaga”). Wszystko razem jest w `wszystko-gl46`.
- Starsze zmiany (do 2026-10-31) są pojedynczymi commitami na `siec-gl46`: `git cherry-pick <hash>`, w kolejności
  z sekcji „Wymaga”.
- Tłumaczenia (`lang/editor_pl.po`) były dopisywane na końcu pliku: przy cherry-picku nie po kolei konflikt na końcu
  pliku rozwiązuje się, biorąc obie strony. Nowe wpisy idą przy pokrewnych tekstach, więc konfliktów będzie mniej.
- Zmiana pól w `application/editormode.h` oznacza przebudowę prawie całego edytora (ok. 30 min).

## 2026-11-01

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
