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

## 2026-10-31

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
