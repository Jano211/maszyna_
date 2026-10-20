# Sieć trakcyjna – jak dokładać nowe rzeczy i nie zepsuć starych

Dla Claude'a w nowym czacie i dla każdego, kto rozwija sieć trakcyjną. Krótko: **co gdzie siedzi, co zmieniać
w którym miejscu, jak sprawdzić, że nic się nie rozjechało.** Stan opisany: 2026-10-21. Bieżące szczegóły i historia
są w `PRZEKAZANIE.md` (sekcja START TUTAJ), koncepcja edytora w `KONCEPCJA_EDYTOR.md`.

## Trzy warstwy

| Warstwa | Plik | Kto pisze | Kiedy działa |
|---|---|---|---|
| Generator | `etap0/gen_cantilevers.py` (+ `etap0/zlozenia.json` ze złożeń z Blendera) | człowiek / Claude | raz, przed grą |
| Katalog wysięgników | `data/traction_cantilevers.txt` | **tylko generator** | czytany przez exe |
| Konstrukcje wsporcze | `data/traction_supports.txt` | **ręcznie** (docelowo panel w edytorze exe) | czytany przez exe |
| Składanie | `world/catenary_cantilever.cpp` (solver), `world/catenary_scene.cpp` (sceneria, 25 kV, dobór rur) | C++ | przy wczytaniu scenerii |

Exe czyta oba pliki danych do jednego katalogu. Python w grze nie bierze udziału.

## Co zmieniać gdzie

- **Nowy słup / mocowanie do słupa** → `data/traction_supports.txt`. Składnia i przykład są w nagłówku pliku.
  Generatora nie ruszać.
- **Wymiar albo punkt części, nowa część, karta, zestaw ramion, przepis składania (`assembly`)** → `gen_cantilevers.py`,
  potem wygenerować. **Nigdy ręcznie w `traction_cantilevers.txt`** – zginie przy następnym generowaniu.
  (Wyjątek: ręczna zmiana w katalogu symulatora na próbę – po potwierdzeniu przenieść wartość do generatora.)
- **Nowy rodzaj kroku w przepisie, nowe słowo w danych, nowe zachowanie** (np. `foundation`, `clearance`) → C++.
  Nowe słowo zawsze z wartością domyślną, przy której stare dane działają jak dotąd.

## Zasady, które już raz się zemściły

1. **Generator musi dawać dokładnie plik z repo.** Przed zmianą: wygenerować i porównać z repo bez CR
   (`tr -d '\r'`). Jeśli różnica jest przed Twoją zmianą – ktoś edytował dane z ręki; najpierw przenieść to do generatora.
   (2026-10-21: generator nie znał słupów 1665–1667 i poprawki `rope_arms` – ponowne generowanie by je skasowało.)
2. **Składanki są budowane z innych składanek** (np. `rope_arms_mount` = treść `rope_arms` z podmianami). Krok dopisany
   do bazowej trafia do pochodnych i odwrotnie. Sprawdzić, w których przepisach kończy się zmiana.
   (2026-10-18: `roll rope_insulator` w `rope_arms` – 40 kart przestało się składać na wszystkich słupach.)
3. **„Bez uwag” w solverze ≠ dobrze w symulatorze.** Reguły pilnują tylko tego, co w nie wpisano. Wymiary z modeli
   (np. całkowita długość izolatora vs punkt `end`) potwierdzać z autorem modelu / użytkownikiem.
4. **Gdy czegoś nie wiadomo – pytać użytkownika, nie zakładać.** Założenia, jeśli już, oznaczać w danych i notatkach słowem ZAŁOŻENIE.
5. **Podglądów PNG nie generować bez prośby** (zjadają kontekst).

## Sprawdzenie przed commitem (każdym, który rusza dane albo solver)

1. Testy poza exe: `sh tests/catenary/build.sh` z katalogu repo, potem `tests/catenary/tc` → ma się kończyć `wszystko OK`.
2. Liczby solvera na wszystkich kartach i słupach przed i po zmianie (bez uwag / z uwagami / nie składa).
   Program: pętla po `catalog.schemes()` i `solve_cantilever_on_pole` na każdym słupie, drut 5,6 m nad główką,
   środek pasma A (wzór: `etap0/klucz_uwag.cpp`). Każda zmiana tych liczb musi mieć wyjaśnienie.
   Stan 2026-10-21: 1701 387/645/4, 1665 1023/13/0, 1666 983/49/4, 1667 365/667/4.
3. Sceneria próbna przez kod sceny z atrapą (`tests/catenary/mock`, wzór: `test_scene.cpp`), np. `td_wys25.scm`:
   69/69 złożonych, 1656 części, zero uwag.

## Dostarczenie

- Commit razem z notatką w `PRZEKAZANIE.md` (jeden krok = jeden commit), push na `siec-trakcyjna`, merge do `wszystko`,
  push `wszystko` (gałąź zbiorcza, z której buduje się exe do testów).
- Dane do katalogu `data\` symulatora: **oba pliki**, w CRLF; po zapisie sprawdzić md5 (pobrać z powrotem i porównać).
  Scenerie do `scenery\`. Cudzych scenerii nie nadpisywać – nowa nazwa pliku (np. `td_wys25`).
- `Traction.h` i `Globals.h` – nie ruszać bez wyraźnej zgody użytkownika (pliki wspólne z resztą exe, konflikty przy merge).
