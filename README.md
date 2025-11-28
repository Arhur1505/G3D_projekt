# Model atomu 3D – SFML + OpenGL

Autor: *Artur Jóźwiak*  
Przedmiot: Grafika 3D  

---

## Opis projektu

Projekt przedstawia interaktywną wizualizację atomu w 3D z wykorzystaniem biblioteki **SFML** oraz **OpenGL**.  
Użytkownik może przełączać się między dwoma modelami przedstawienia elektronów:

1. **Model Bohra** – elektrony poruszające się po kołowych orbitach.
2. **Chmura prawdopodobieństwa** – punktowa wizualizacja przestrzennego rozkładu elektronów.

Liczba elektronów może być zmieniana w zakresie **1–18**, a aplikacja na tej podstawie prezentuje odpowiadający pierwiastek (Z, symbol, nazwa).  
Całość osadzona jest na tle kosmicznym (tekstura gwiazd), a w rogu ekranu wyświetlane są podstawowe informacje oraz pomoc kontekstowa.

---

## Zastosowane techniki grafiki 3D

- **OpenGL 2.1 (pipeline mieszany)**
  - klasyczny pipeline (macierze, `glVertex*`, `glRotatef`, itp.),
  - częściowo własne shadery GLSL.

- **Oświetlenie i materiały**
  - klasyczny model oświetlenia **Phonga** (ambient + diffuse + specular) w shaderze fragmentów,
  - zdefiniowane globalne światło punktowe (`GL_LIGHT0`),
  - włączony `GL_NORMALIZE` i `GL_SMOOTH`.

- **Shadery GLSL**
  - **vertex shader** – przekazanie normalnych, pozycji w przestrzeni oka i koloru do fragment shadera,
  - **fragment shader** – własna implementacja Phonga + dodatkowy efekt:
    - **rim lighting** (podkreślenie krawędzi atomu, efekt „świecących obiektów”).

- **Tekstury**
  - tło sceny jako **tekstura 2D** (`resources/stars.png`) renderowane na dużym quadzie za sceną 3D.

- **Geometria**
  - jądro atomu i elektrony jako sfery generowane przez **GLU quadric** (`gluSphere`),
  - kołowe orbity jako okręgi złożone z linii,
  - chmura prawdopodobieństwa – generowana losowo chmura punktów (`GL_POINTS`) w przestrzeni 3D,
  - lokalne układy współrzędnych (osie X/Y/Z) dla wizualizacji orientacji.

- **Przezroczystość i blending**
  - włączony **alpha blending** (`GL_BLEND`, `GL_SRC_ALPHA`, `GL_ONE_MINUS_SRC_ALPHA`) dla półprzezroczystej chmury elektronowej.

- **Interfejs 2D**
  - overlay w **SFML Graphics** (`sf::Text`) z wykorzystaniem czcionki `resources/fonts/arial.ttf`,
  - osobny widok GUI (`sf::View`) niezależny od rozdzielczości.

---

## Sterowanie

Z klawiatury:

- `Strzałki` – obrót sceny (kamery wokół atomu):
  - `←` / `→` – obrót wokół osi Y,
  - `↑` / `↓` – obrót wokół osi X (z ograniczeniem, aby nie „przewrócić” kamery).
- `1` / `Numpad1` – widok **orbit kołowych (model Bohra)**.
- `2` / `Numpad2` – widok **chmury prawdopodobieństwa**.
- `Num +` / `=` – zwiększenie liczby elektronów (max 18).
- `Num -` / `-` – zmniejszenie liczby elektronów (min 1).
- `Spacja` – włączenie/wyłączenie animacji ruchu elektronów.
- `A` – włączenie/wyłączenie **lokalnych osi** przy elektronach.
- `R` – reset widoku do ustawień domyślnych (rotacja, liczba elektronów, tryb).
- `Esc` – wyjście z programu.

---

### Biblioteki:

- **SFML** (co najmniej moduły: `window`, `graphics`, `system`),
- **OpenGL** (`opengl32.lib`),
- **GLU** (`glu32.lib`),
- **GLEW** lub inna biblioteka ładująca funkcje OpenGL (wywołanie `glewInit()` w `main`),
- ewentualnie `pch.h` z konfiguracją projektu (precompiled header).

---

## Film demonstracyjny

[Zobacz film demonstracyjny (MP4)](Film.mp4)


