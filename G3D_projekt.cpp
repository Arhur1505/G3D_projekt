#include "pch.h"
#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>
#include <SFML/OpenGL.hpp>
#include <windows.h>
#include <GL/glu.h>
#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "glu32.lib")

#include <iostream>
#include <cmath>
#include <vector>
#include <cstdlib>
#include <sstream>
#include <algorithm>
#include <string>

namespace
{
    constexpr float PI = 3.14159265358979323846f;
    float deg2rad(float d) { return d * PI / 180.f; }
    float clampFloat(float v, float a, float b)
    {
        return (v < a ? a : (v > b ? b : v));
    }

    enum class ViewMode
    {
        BohrOrbits = 0,
        ProbabilityCloud = 1
    };

    struct AppState
    {
        float rotX = 20.f;
        float rotY = -30.f;
        bool  animateElectrons = true;
        float electronAngleDeg = 0.f;
        bool showLocalAxes = true;
        ViewMode viewMode = ViewMode::BohrOrbits;
        int electronCount = 6;
        sf::Vector3f eye{ 2.2f, 1.8f, 4.0f };
        sf::Vector3f center{ 0.0f, 0.2f, 0.0f };
        sf::Vector3f up{ 0.0f, 1.0f, 0.0f };
        float fovDeg = 60.0f;
        float nearP = 0.1f, farP = 100.0f;
    } G;

    struct ElementInfo
    {
        int         Z;
        const char* symbol;
        const char* name;
    };

    static const ElementInfo ELEMENTS[] =
    {
        {  1, "H",  "Wodor"      },
        {  2, "He", "Hel"        },
        {  3, "Li", "Lit"        },
        {  4, "Be", "Beryl"      },
        {  5, "B",  "Bor"        },
        {  6, "C",  "Wegiel"     },
        {  7, "N",  "Azot"       },
        {  8, "O",  "Tlen"       },
        {  9, "F",  "Fluor"      },
        { 10, "Ne", "Neon"       },
        { 11, "Na", "Sod"        },
        { 12, "Mg", "Magnez"     },
        { 13, "Al", "Glin"       },
        { 14, "Si", "Krzem"      },
        { 15, "P",  "Fosfor"     },
        { 16, "S",  "Siarka"     },
        { 17, "Cl", "Chlor"      },
        { 18, "Ar", "Argon"      },
    };

    const ElementInfo* getCurrentElement()
    {
        int Z = G.electronCount;
        if (Z < 1 || Z > 18) return nullptr;
        return &ELEMENTS[Z - 1];
    }

    constexpr int MAX_SHELLS = 3;
    const int SHELL_CAPACITY[MAX_SHELLS] = { 2, 8, 8 };

    float shellRadius(int shellIdx)
    {
        const float baseRadius = 0.7f;
        const float step = 0.55f;
        return baseRadius + step * shellIdx;
    }

    struct CloudPoint
    {
        float x, y, z;
        int   shell;
    };

    std::vector<CloudPoint> gCloudPoints;
    static GLUquadric* gQuad = nullptr;
    static sf::Font gFont;
    static bool gFontLoaded = false;
    static GLuint gBackgroundTex = 0;
    static bool gBackgroundTexLoaded = false;
    static sf::View gGuiView;
    static bool gGuiViewInitialized = false;
    static GLuint gAtomProgram = 0;

    float rand01()
    {
        return std::rand() / static_cast<float>(RAND_MAX);
    }
}

static void initOpenGL()
{
    glClearColor(0.02f, 0.02f, 0.06f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

static void initLighting()
{
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    const GLfloat globalAmbient[] = { 0.05f, 0.05f, 0.08f, 1.0f };
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, globalAmbient);
    glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_FALSE);
    const GLfloat lightAmbient[] = { 0.2f, 0.2f, 0.25f, 1.0f };
    const GLfloat lightDiffuse[] = { 0.8f, 0.8f, 0.9f, 1.0f };
    const GLfloat lightSpecular[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glLightfv(GL_LIGHT0, GL_AMBIENT, lightAmbient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, lightDiffuse);
    glLightfv(GL_LIGHT0, GL_SPECULAR, lightSpecular);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    const GLfloat matSpecular[] = { 0.9f, 0.9f, 0.9f, 1.0f };
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, matSpecular);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 64.0f);
    glShadeModel(GL_SMOOTH);
    glEnable(GL_NORMALIZE);
}

static GLuint compileShader(GLenum type, const char* src)
{
    GLuint shader = glCreateShader(type);
    if (!shader) return 0;
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);
    GLint status = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (status != GL_TRUE)
    {
        GLint logLength = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
        if (logLength > 0)
        {
            std::string log(logLength, '\0');
            glGetShaderInfoLog(shader, logLength, nullptr, &log[0]);
            std::cerr << "Błąd kompilacji shadera: " << log << std::endl;
        }
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

static void initAtomShader()
{
    const char* vsSrc = R"(
        varying vec3 vNormal;
        varying vec3 vPosEye;
        varying vec4 vColor;

        void main()
        {
            vec4 posEye = gl_ModelViewMatrix * gl_Vertex;
            vPosEye = posEye.xyz;
            vNormal = normalize(gl_NormalMatrix * gl_Normal);
            vColor = gl_Color;
            gl_Position = gl_ProjectionMatrix * posEye;
        }
    )";

    const char* fsSrc = R"(
        varying vec3 vNormal;
        varying vec3 vPosEye;
        varying vec4 vColor;

        void main()
        {
            vec3 N = normalize(vNormal);
            vec3 V = normalize(-vPosEye);
            vec3 lightPos = vec3(2.0, 3.0, 4.0);
            vec3 L = normalize(lightPos - vPosEye);
            float NdotL = max(dot(N, L), 0.0);
            vec3 baseColor = vColor.rgb;
            vec3 ambient = 0.15 * baseColor;
            vec3 diffuse = 0.75 * baseColor * NdotL;
            vec3 specular = vec3(0.0);
            if (NdotL > 0.0)
            {
                vec3 R = reflect(-L, N);
                float RdotV = max(dot(R, V), 0.0);
                specular = vec3(0.8) * pow(RdotV, 32.0);
            }
            vec3 color = ambient + diffuse + specular;
            float rim = 1.0 - max(dot(N, V), 0.0);
            float rimFactor = pow(rim, 3.0);
            vec3 rimColor = vec3(0.2, 0.4, 1.0);
            color += rimColor * rimFactor;
            gl_FragColor = vec4(color, vColor.a);
        }
    )";

    GLuint vs = compileShader(GL_VERTEX_SHADER, vsSrc);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fsSrc);
    if (!vs || !fs)
    {
        std::cerr << "Nie udało się skompilować shaderów atomu, używam tylko potoku stałego.\n";
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        return;
    }
    gAtomProgram = glCreateProgram();
    glAttachShader(gAtomProgram, vs);
    glAttachShader(gAtomProgram, fs);
    glLinkProgram(gAtomProgram);
    GLint linkStatus = GL_FALSE;
    glGetProgramiv(gAtomProgram, GL_LINK_STATUS, &linkStatus);
    if (linkStatus != GL_TRUE)
    {
        GLint logLength = 0;
        glGetProgramiv(gAtomProgram, GL_INFO_LOG_LENGTH, &logLength);
        if (logLength > 0)
        {
            std::string log(logLength, '\0');
            glGetProgramInfoLog(gAtomProgram, logLength, nullptr, &log[0]);
            std::cerr << "Błąd linkowania programu shaderów: " << log << std::endl;
        }
        glDeleteProgram(gAtomProgram);
        gAtomProgram = 0;
    }
    glDeleteShader(vs);
    glDeleteShader(fs);
    if (gAtomProgram)
        std::cout << "Shadery atomu (Phong + rim lighting) zainicjalizowane.\n";
}


static bool loadBackgroundTexture(const std::string& path)
{
    sf::Image img;
    if (!img.loadFromFile(path))
    {
        std::cerr << "Nie udalo sie wczytac tekstury tla: " << path << "\n";
        return false;
    }
    img.flipVertically();
    if (gBackgroundTex == 0)
        glGenTextures(1, &gBackgroundTex);
    glBindTexture(GL_TEXTURE_2D, gBackgroundTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(
        GL_TEXTURE_2D, 0, GL_RGBA,
        img.getSize().x, img.getSize().y, 0,
        GL_RGBA, GL_UNSIGNED_BYTE, img.getPixelsPtr()
    );
    glBindTexture(GL_TEXTURE_2D, 0);
    std::cout << "Tekstura tla zaladowana: " << path << "\n";
    return true;
}

static void drawBackgroundQuad()
{
    if (!gBackgroundTexLoaded || gBackgroundTex == 0)
        return;
    GLboolean lighting = glIsEnabled(GL_LIGHTING);
    if (lighting) glDisable(GL_LIGHTING);
    GLfloat prevColor[4];
    glGetFloatv(GL_CURRENT_COLOR, prevColor);
    glUseProgram(0);
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    float w = static_cast<float>(viewport[2]);
    float h = static_cast<float>(viewport[3]);
    if (h == 0.f) h = 1.f;
    float aspect = w / h;
    const float depth = 10.f;
    float halfHeight = depth * std::tan(deg2rad(G.fovDeg * 0.5f));
    float halfWidth = halfHeight * aspect;
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    {
        glLoadIdentity();
        glTranslatef(0.f, 0.f, -depth);
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, gBackgroundTex);
        glColor4f(1.f, 1.f, 1.f, 1.f);
        glBegin(GL_QUADS);
        glTexCoord2f(0.f, 0.f); glVertex3f(-halfWidth, -halfHeight, 0.f);
        glTexCoord2f(1.f, 0.f); glVertex3f(halfWidth, -halfHeight, 0.f);
        glTexCoord2f(1.f, 1.f); glVertex3f(halfWidth, halfHeight, 0.f);
        glTexCoord2f(0.f, 1.f); glVertex3f(-halfWidth, halfHeight, 0.f);
        glEnd();
        glBindTexture(GL_TEXTURE_2D, 0);
        glDisable(GL_TEXTURE_2D);
    }
    glPopMatrix();
    glColor4fv(prevColor);
    if (lighting) glEnable(GL_LIGHTING);
}

static void setupProjection(sf::Vector2u s)
{
    if (!s.y) s.y = 1;
    const double aspect = s.x / static_cast<double>(s.y);
    glViewport(0, 0, (GLsizei)s.x, (GLsizei)s.y);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(G.fovDeg, aspect, G.nearP, G.farP);
    glMatrixMode(GL_MODELVIEW);
}

static void setupView()
{
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    gluLookAt(G.eye.x, G.eye.y, G.eye.z,
        G.center.x, G.center.y, G.center.z,
        G.up.x, G.up.y, G.up.z);
}

static void drawAxes(float len = 0.4f)
{
    GLboolean lighting = glIsEnabled(GL_LIGHTING);
    GLint prevProgram = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &prevProgram);
    GLfloat prevColor[4];
    glGetFloatv(GL_CURRENT_COLOR, prevColor);
    GLfloat prevLineWidth = 1.0f;
    glGetFloatv(GL_LINE_WIDTH, &prevLineWidth);
    glUseProgram(0);
    if (lighting) glDisable(GL_LIGHTING);
    glLineWidth(2.0f);
    glBegin(GL_LINES);
    glColor3f(1, 0, 0); glVertex3f(0, 0, 0); glVertex3f(+len, 0, 0);
    glColor3f(0, 1, 0); glVertex3f(0, 0, 0); glVertex3f(0, +len, 0);
    glColor3f(0, 0, 1); glVertex3f(0, 0, 0); glVertex3f(0, 0, +len);
    glEnd();
    glLineWidth(prevLineWidth);
    glColor4fv(prevColor);
    if (lighting) glEnable(GL_LIGHTING);
    glUseProgram(prevProgram);
}


static void initQuadric()
{
    gQuad = gluNewQuadric();
    if (gQuad)
        gluQuadricNormals(gQuad, GLU_SMOOTH);
}

static void freeQuadric()
{
    if (gQuad)
    {
        gluDeleteQuadric(gQuad);
        gQuad = nullptr;
    }
}

static void drawSphere(float radius, int slices = 32, int stacks = 16)
{
    gluSphere(gQuad, radius, slices, stacks);
}

static void initCloudPoints()
{
    gCloudPoints.clear();
    std::srand(0);
    const int pointsPerShell = 1500;

    for (int s = 0; s < MAX_SHELLS; ++s)
    {
        float R = shellRadius(s);
        for (int i = 0; i < pointsPerShell; ++i)
        {
            float u = rand01();
            float v = rand01();
            float theta = 2.f * PI * u;
            float phi = std::acos(2.f * v - 1.f);
            float sinPhi = std::sin(phi);
            float dirX = sinPhi * std::cos(theta);
            float dirY = std::cos(phi);
            float dirZ = sinPhi * std::sin(theta);
            float rFactor;
            if (s == 0)
            {
                rFactor = rand01();
            }
            else
            {
                rFactor = 0.8f + 0.4f * rand01();
            }
            float r = R * rFactor;
            CloudPoint cp;
            cp.x = dirX * r;
            cp.y = dirY * r;
            cp.z = dirZ * r;
            cp.shell = s;
            gCloudPoints.push_back(cp);
        }
    }
}

static int countActiveShells()
{
    int e = G.electronCount;
    int shells = 0;
    for (int s = 0; s < MAX_SHELLS; ++s)
    {
        if (e <= 0) break;
        ++shells;
        e -= SHELL_CAPACITY[s];
    }
    return shells;
}

static void drawOrbitCircle(float radius)
{
    const int segments = 64;
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < segments; ++i)
    {
        float angle = 2.f * PI * i / segments;
        float x = radius * std::cos(angle);
        float z = radius * std::sin(angle);
        glVertex3f(x, 0.f, z);
    }
    glEnd();
}

static void drawAtomBohrModel()
{
    glPushMatrix();
    {
        glColor3f(1.0f, 0.3f, 0.3f);
        drawSphere(0.25f);
        int remaining = G.electronCount;
        for (int shell = 0; shell < MAX_SHELLS; ++shell)
        {
            if (remaining <= 0) break;
            int capacity = SHELL_CAPACITY[shell];
            int electronsInShell = std::min(remaining, capacity);
            float R = shellRadius(shell);
            {
                GLboolean lighting = glIsEnabled(GL_LIGHTING);
                if (lighting) glDisable(GL_LIGHTING);
                glUseProgram(0);
                glColor3f(0.9f, 0.9f, 0.9f);
                drawOrbitCircle(R);
                if (lighting) glEnable(GL_LIGHTING);
                if (gAtomProgram) glUseProgram(gAtomProgram);
            }
            for (int e = 0; e < electronsInShell; ++e)
            {
                float baseAngle = 360.f * e / electronsInShell;
                float speed = 1.0f + 0.3f * shell;
                float angle = baseAngle + G.electronAngleDeg * speed;
                glPushMatrix();
                glRotatef(angle, 0.f, 1.f, 0.f);
                glTranslatef(R, 0.f, 0.f);
                glColor3f(0.2f, 0.6f, 1.0f);
                if (G.showLocalAxes) drawAxes(0.15f);
                drawSphere(0.08f);
                glPopMatrix();
            }
            remaining -= electronsInShell;
        }
    }
    glPopMatrix();
}

static void drawAtomProbabilityCloud()
{
    glPushMatrix();
    {
        glColor3f(1.0f, 0.3f, 0.3f);
        drawSphere(0.25f);
        int activeShells = countActiveShells();
        GLboolean lighting = glIsEnabled(GL_LIGHTING);
        GLint prevProgram = 0;
        glGetIntegerv(GL_CURRENT_PROGRAM, &prevProgram);
        GLfloat prevColor[4];
        glGetFloatv(GL_CURRENT_COLOR, prevColor);
        GLfloat prevPointSize = 1.0f;
        glGetFloatv(GL_POINT_SIZE, &prevPointSize);
        glUseProgram(0);
        if (lighting) glDisable(GL_LIGHTING);
        glPushMatrix();
        {
            float baseYaw = 18.0f * (G.electronCount - 1);
            float basePitch = 7.0f * (G.electronCount - 1);
            float animAngle = 0.4f * G.electronAngleDeg;
            glRotatef(baseYaw + animAngle, 0.f, 1.f, 0.f);
            glRotatef(basePitch, 1.f, 0.f, 0.f);
            glPointSize(2.5f);
            glBegin(GL_POINTS);
            for (const CloudPoint& cp : gCloudPoints)
            {
                if (cp.shell >= activeShells) continue;
                float alpha = 0.16f + 0.05f * cp.shell;
                float r = 0.3f;
                float g = 0.5f + 0.15f * cp.shell;
                float b = 1.0f;
                glColor4f(r, g, b, alpha);
                glVertex3f(cp.x, cp.y, cp.z);
            }
            glEnd();
        }
        glPopMatrix();
        glPointSize(prevPointSize);
        glColor4fv(prevColor);
        if (lighting) glEnable(GL_LIGHTING);
        glUseProgram(prevProgram);
    }
    glPopMatrix();
}



static void drawAtom()
{
    if (G.showLocalAxes)
        drawAxes(0.5f);
    if (gAtomProgram)
        glUseProgram(gAtomProgram);
    if (G.viewMode == ViewMode::BohrOrbits)
        drawAtomBohrModel();
    else
        drawAtomProbabilityCloud();
    if (gAtomProgram)
        glUseProgram(0);
}

static void drawScene(float dt)
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    drawBackgroundQuad();
    setupView();
    {
        GLfloat lightPos[] = { 2.0f, 3.0f, 4.0f, 1.0f };
        glLightfv(GL_LIGHT0, GL_POSITION, lightPos);
    }
    glRotatef(G.rotX, 1.f, 0.f, 0.f);
    glRotatef(G.rotY, 0.f, 1.f, 0.f);
    drawAtom();
    if (G.animateElectrons)
    {
        G.electronAngleDeg += 40.f * dt;
        if (G.electronAngleDeg >= 360.f) G.electronAngleDeg -= 360.f;
    }
}

static void drawGuiOverlay(sf::RenderWindow& win)
{
    if (!gFontLoaded) return;
    win.pushGLStates();
    {
        if (gGuiViewInitialized)
            win.setView(gGuiView);
        else
            win.setView(win.getDefaultView());
        sf::Text text;
        text.setFont(gFont);
        text.setCharacterSize(18);
        text.setFillColor(sf::Color::White);
        const ElementInfo* el = getCurrentElement();
        std::ostringstream oss;
        if (el)
        {
            oss << "Atom: Z = " << el->Z << "   "
                << el->symbol << " (" << el->name << ")";
        }
        else
        {
            oss << "Atom: (nieznany), e- = " << G.electronCount;
        }
        oss << "\nTryb widoku: ";
        if (G.viewMode == ViewMode::BohrOrbits)
            oss << "orbity kolowe (Bohr)";
        else
            oss << "chmury prawdopodobienstwa";
        text.setString(oss.str());
        text.setPosition(10.f, 10.f);
        win.draw(text);
        text.setFont(gFont);
        text.setCharacterSize(18);
        text.setFillColor(sf::Color::White);
        text.setString(
            "Sterowanie:\n"
            "Strzalki: obrot sceny\n"
            "1 / 2: orbity / chmury\n"
            "Num+/-: liczba elektronow (1..18)\n"
            "Spacja: animacja ON/OFF\n"
            "A: lokalne osie ON/OFF\n"
            "R: reset widoku\n"
            "Esc: wyjscie"
        );
        text.setPosition(10.f, 70.f);
        win.draw(text);
    }
    win.popGLStates();
}

int main()
{
    sf::ContextSettings cs;
    cs.depthBits = 24;
    cs.stencilBits = 8;
    cs.majorVersion = 2;
    cs.minorVersion = 1;
    sf::RenderWindow win(sf::VideoMode(1024, 768),
        "Model atomu - SFML + OpenGL",
        sf::Style::Default, cs);
    win.setVerticalSyncEnabled(true);
    win.setActive(true);
    gGuiView = win.getDefaultView();
    gGuiViewInitialized = true;
    GLenum err = glewInit();
    initOpenGL();
    setupProjection(win.getSize());
    initQuadric();
    initCloudPoints();
    initLighting();
    initAtomShader();
    gBackgroundTexLoaded = loadBackgroundTexture("resources/stars.png");
    gFontLoaded = gFont.loadFromFile("resources/fonts/arial.ttf");
    if (!gFontLoaded)
    {
        std::cout << "UWAGA: Nie udalo sie wczytac czcionki 'resources/fonts/arial.ttf'. "
            << "GUI tekstowe bedzie wylaczone.\n";
    }
    sf::Clock clock;
    std::cout
        << "Sterowanie:\n"
        << "  Strzalki  : obrot sceny\n"
        << "  1 / 2     : orbity kolowe / chmury prawdopodobienstwa\n"
        << "  Num+/-    : zwieksz / zmniejsz liczbe elektronow (1..18)\n"
        << "  Spacja    : animacja elektronow ON/OFF\n"
        << "  A         : lokalne osie ON/OFF\n"
        << "  R         : reset widoku\n"
        << "  Esc       : wyjscie\n";

    bool running = true;
    while (running)
    {
        float dt = clock.restart().asSeconds();
        for (sf::Event e; win.pollEvent(e);)
        {
            if (e.type == sf::Event::Closed)
                running = false;
            if (e.type == sf::Event::KeyPressed &&
                e.key.code == sf::Keyboard::Escape)
                running = false;
            if (e.type == sf::Event::Resized)
            {
                sf::Vector2u size = win.getSize();
                setupProjection(size);
                gGuiView.setSize(static_cast<float>(size.x),
                    static_cast<float>(size.y));
                gGuiView.setCenter(size.x * 0.5f, size.y * 0.5f);
            }
            if (e.type == sf::Event::KeyPressed)
            {
                switch (e.key.code)
                {
                case sf::Keyboard::Left:  G.rotY -= 5.f; break;
                case sf::Keyboard::Right: G.rotY += 5.f; break;
                case sf::Keyboard::Up:    G.rotX += 5.f; break;
                case sf::Keyboard::Down:  G.rotX -= 5.f; break;
                case sf::Keyboard::Num1:
                case sf::Keyboard::Numpad1:
                    G.viewMode = ViewMode::BohrOrbits; break;
                case sf::Keyboard::Num2:
                case sf::Keyboard::Numpad2:
                    G.viewMode = ViewMode::ProbabilityCloud; break;
                case sf::Keyboard::Space:
                    G.animateElectrons = !G.animateElectrons; break;
                case sf::Keyboard::Add:
                case sf::Keyboard::Equal:
                    if (G.electronCount < 18) ++G.electronCount;
                    break;
                case sf::Keyboard::Subtract:
                case sf::Keyboard::Hyphen:
                    if (G.electronCount > 1) --G.electronCount;
                    break;
                case sf::Keyboard::A:
                    G.showLocalAxes = !G.showLocalAxes; break;
                case sf::Keyboard::R:
                    G.rotX = 20.f;
                    G.rotY = -30.f;
                    G.electronAngleDeg = 0.f;
                    G.animateElectrons = true;
                    G.viewMode = ViewMode::BohrOrbits;
                    G.electronCount = 6;
                    break;
                default: break;
                }
                G.rotX = clampFloat(G.rotX, -89.f, +89.f);
            }
        }
        drawScene(dt);
        drawGuiOverlay(win);
        win.display();
    }
     freeQuadric();
    if (gAtomProgram) glDeleteProgram(gAtomProgram);

    return 0;
}
