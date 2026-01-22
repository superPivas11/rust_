#include <WiFi.h>
#include <WebSocketsClient.h>
#include <driver/i2s.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <EEPROM.h>
#include "animation_data_1.h"
#include "animation_data_2.h"
#include "animation_data_3.h"

const int WIFI_COUNT = 3;
const char* wifiNetworks[WIFI_COUNT][2] = {
    {"sistema", "12345678"},
    {"svaboda", "5422f89a"},
    {"Office", "officepass"}
};

String connectedSSID = "";
int currentWifiIndex = 0;

const char* websocket_host = "voice-assistant-z07p.onrender.com";
const int websocket_port = 443;
const char* websocket_path = "/ws";

#define I2S_WS 25
#define I2S_SD 32
#define I2S_SCK 33
#define I2S_PORT I2S_NUM_0
#define SAMPLE_RATE 16000
#define BUFFER_LEN 1024
#define BUTTON_PIN 4
#define SHORT_PRESS_TIME 300

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, 22, 21);

WebSocketsClient webSocket;

bool isConnected = false;
bool isRecording = false;
bool gotResponse = false;
String lastResponse = "";
int scrollOffset = 0;
int totalLines = 0;

#define MAX_CHARS_PER_LINE 21
#define VISIBLE_LINES 3

unsigned long buttonPressTime = 0;
bool buttonWasPressed = false;

enum State { 
    STATE_ANIMATION, 
    STATE_WIFI_SELECT, 
    STATE_WIFI_CONNECTING, 
    STATE_READY, 
    STATE_RECORDING, 
    STATE_PROCESSING,
    STATE_MORSE  // Новый режим азбуки Морзе
};

State currentState = STATE_ANIMATION;

int currentFrame = 0;
unsigned long lastFrameTime = 0;
#define FRAME_DELAY 100
#define ANIMATION_COUNT 3
int currentAnimation = 0;
#define EEPROM_ANIMATION_ADDR 0

struct AnimationData {
    const uint8_t* frames;
    int frame_count;
    int frame_size;
};

AnimationData animations[ANIMATION_COUNT] = {
    {(const uint8_t*)animation_frames_1, FRAME_COUNT_1, FRAME_SIZE_1},
    {(const uint8_t*)animation_frames_2, FRAME_COUNT_2, FRAME_SIZE_2},
    {(const uint8_t*)animation_frames_3, FRAME_COUNT_3, FRAME_SIZE_3}
};

String lines[99];  // Увеличено с 20 до 99

// Переменные для азбуки Морзе
String morseCode = "";
String morseDisplay = "";
unsigned long morseStartTime = 0;
bool morseButtonPressed = false;
bool morseInputMode = false;  // true = ввод Морзе, false = просмотр ответа
#define MORSE_DOT_TIME 200      // Короткое нажатие < 200ms = точка
#define MORSE_DASH_TIME 400     // Среднее нажатие < 400ms = тире (было 600)
#define MORSE_SEND_TIME 1000    // Длинное нажатие > 1000ms = отправить/новый ввод
#define MORSE_SPACE_TIME 800    // Пауза > 800ms = пробел между буквами (было 500)

void showText(const char* line1, const char* line2 = "", const char* line3 = "", const char* line4 = "") {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x13_t_cyrillic);
    u8g2.drawUTF8(0, 14, line1);
    u8g2.drawUTF8(0, 28, line2);
    u8g2.drawUTF8(0, 42, line3);
    u8g2.drawUTF8(0, 56, line4);
    u8g2.sendBuffer();
}

void showMorse() {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x13_t_cyrillic);
    
    // Заголовок
    u8g2.drawUTF8(0, 10, "РЕЖИМ МОРЗЕ");
    
    // Показываем введённый код построчно (вертикально)
    u8g2.setFont(u8g2_font_10x20_t_cyrillic);
    
    int charsPerLine = 12;  // Символов в строке
    int maxLines = 3;       // Максимум 3 строки (экран 64 пикселя)
    int lineHeight = 18;    // Высота строки
    int startY = 20;        // Начальная позиция Y
    
    // Разбиваем текст на строки
    int totalChars = morseDisplay.length();
    int startChar = 0;
    
    // Если текст длинный - показываем последние строки (прокрутка вверх)
    int totalLines = (totalChars + charsPerLine - 1) / charsPerLine;
    if (totalLines > maxLines) {
        startChar = (totalLines - maxLines) * charsPerLine;
    }
    
    // Рисуем строки сверху вниз
    for (int line = 0; line < maxLines; line++) {
        int charIndex = startChar + (line * charsPerLine);
        if (charIndex >= totalChars) break;
        
        int endIndex = min(charIndex + charsPerLine, totalChars);
        String lineText = morseDisplay.substring(charIndex, endIndex);
        
        u8g2.drawUTF8(0, startY + (line * lineHeight), lineText.c_str());
    }
    
    u8g2.sendBuffer();
}

String cleanLatex(String text) {
    // Убираем LaTeX разметку и заменяем на читаемые символы
    
    // Дроби: \frac{a}{b} -> a/b
    while (text.indexOf("\\frac{") >= 0) {
        int start = text.indexOf("\\frac{");
        int firstClose = text.indexOf("}", start);
        int secondClose = text.indexOf("}", firstClose + 1);
        
        if (firstClose > 0 && secondClose > 0) {
            String numerator = text.substring(start + 6, firstClose);
            String denominator = text.substring(firstClose + 2, secondClose);
            String replacement = numerator + "/" + denominator;
            text = text.substring(0, start) + replacement + text.substring(secondClose + 1);
        } else {
            break;
        }
    }
    
    // Степени и индексы
    text.replace("^{", "^");
    text.replace("_{", "_");
    
    // Убираем все фигурные скобки после обработки
    int braceCount = 0;
    String result = "";
    for (int i = 0; i < text.length(); i++) {
        char c = text[i];
        if (c == '{') {
            braceCount++;
            continue;
        }
        if (c == '}') {
            if (braceCount > 0) braceCount--;
            continue;
        }
        result += c;
    }
    text = result;
    
    // Математические символы (С ОБРАТНЫМ СЛЭШЕМ)
    text.replace("\\pi", "π");
    text.replace("\\alpha", "α");
    text.replace("\\beta", "β");
    text.replace("\\gamma", "γ");
    text.replace("\\delta", "δ");
    text.replace("\\epsilon", "ε");
    text.replace("\\theta", "θ");
    text.replace("\\lambda", "λ");
    text.replace("\\mu", "μ");
    text.replace("\\sigma", "σ");
    text.replace("\\omega", "ω");
    text.replace("\\Omega", "Ω");
    text.replace("\\Delta", "Δ");
    text.replace("\\Gamma", "Γ");
    text.replace("\\Theta", "Θ");
    text.replace("\\Lambda", "Λ");
    text.replace("\\Sigma", "Σ");
    text.replace("\\infty", "∞");
    text.replace("\\sum", "Σ");
    text.replace("\\prod", "Π");
    text.replace("\\int", "∫");
    text.replace("\\sqrt", "√");
    text.replace("\\pm", "±");
    text.replace("\\mp", "∓");
    text.replace("\\times", "×");
    text.replace("\\cdot", "·");
    text.replace("\\div", "÷");
    text.replace("\\leq", "≤");
    text.replace("\\geq", "≥");
    text.replace("\\neq", "≠");
    text.replace("\\approx", "≈");
    text.replace("\\equiv", "≡");
    text.replace("\\in", "∈");
    text.replace("\\notin", "∉");
    text.replace("\\subset", "⊂");
    text.replace("\\supset", "⊃");
    text.replace("\\cup", "∪");
    text.replace("\\cap", "∩");
    text.replace("\\emptyset", "∅");
    text.replace("\\forall", "∀");
    text.replace("\\exists", "∃");
    text.replace("\\nabla", "∇");
    text.replace("\\partial", "∂");
    text.replace("\\rightarrow", "→");
    text.replace("\\leftarrow", "←");
    text.replace("\\Rightarrow", "⇒");
    text.replace("\\Leftarrow", "⇐");
    text.replace("\\leftrightarrow", "↔");
    text.replace("\\Leftrightarrow", "⇔");
    text.replace("\\uparrow", "↑");
    text.replace("\\downarrow", "↓");
    
    // Математические символы (БЕЗ ОБРАТНОГО СЛЭША - для случаев типа /pi)
    text.replace("/pi", "π");
    text.replace("/alpha", "α");
    text.replace("/beta", "β");
    text.replace("/gamma", "γ");
    text.replace("/delta", "δ");
    text.replace("/epsilon", "ε");
    text.replace("/theta", "θ");
    text.replace("/lambda", "λ");
    text.replace("/mu", "μ");
    text.replace("/sigma", "σ");
    text.replace("/omega", "ω");
    text.replace("/Omega", "Ω");
    text.replace("/Delta", "Δ");
    text.replace("/Gamma", "Γ");
    text.replace("/Theta", "Θ");
    text.replace("/Lambda", "Λ");
    text.replace("/Sigma", "Σ");
    text.replace("/infty", "∞");
    text.replace("/sum", "Σ");
    text.replace("/prod", "Π");
    text.replace("/int", "∫");
    text.replace("/sqrt", "√");
    text.replace("/pm", "±");
    text.replace("/mp", "∓");
    text.replace("/times", "×");
    text.replace("/cdot", "·");
    text.replace("/div", "÷");
    text.replace("/leq", "≤");
    text.replace("/geq", "≥");
    text.replace("/neq", "≠");
    text.replace("/approx", "≈");
    text.replace("/equiv", "≡");
    text.replace("/in", "∈");
    text.replace("/notin", "∉");
    text.replace("/subset", "⊂");
    text.replace("/supset", "⊃");
    text.replace("/cup", "∪");
    text.replace("/cap", "∩");
    text.replace("/emptyset", "∅");
    text.replace("/forall", "∀");
    text.replace("/exists", "∃");
    text.replace("/nabla", "∇");
    text.replace("/partial", "∂");
    
    // Убираем оставшиеся LaTeX команды
    text.replace("\\left(", "(");
    text.replace("\\right)", ")");
    text.replace("\\left[", "[");
    text.replace("\\right]", "]");
    text.replace("\\left|", "|");
    text.replace("\\right|", "|");
    text.replace("\\left", "");
    text.replace("\\right", "");
    text.replace("\\begin", "");
    text.replace("\\end", "");
    text.replace("\\[", "");
    text.replace("\\]", "");
    text.replace("$$", "");
    text.replace("$", "");
    text.replace("\\text", "");
    text.replace("\\mathrm", "");
    text.replace("\\mathbf", "");
    text.replace("\\mathit", "");
    text.replace("\\,", " ");
    text.replace("\\;", " ");
    text.replace("\\:", " ");
    text.replace("\\!", "");
    text.replace("\\quad", " ");
    text.replace("\\qquad", "  ");
    
    // Убираем обратные слэши перед обычными символами
    text.replace("\\", "");
    
    return text;
}

int splitTextToLines(String text) {
    // Очищаем LaTeX разметку
    text = cleanLatex(text);
    
    int lineCount = 0;
    String currentLine = "";
    int charCount = 0;
    
    for (int i = 0; i < text.length() && lineCount < 99; i++) {
        char c = text[i];
        
        if (c == '\n') {
            if (currentLine.length() > 0) {
                lines[lineCount++] = currentLine;
            }
            currentLine = "";
            charCount = 0;
            continue;
        }
        
        // UTF-8 обработка (кириллица и спецсимволы)
        if ((c & 0xC0) == 0xC0) {
            if (charCount >= MAX_CHARS_PER_LINE) {
                lines[lineCount++] = currentLine;
                currentLine = "";
                charCount = 0;
            }
            currentLine += c;
            if (i + 1 < text.length()) {
                currentLine += text[++i];
            }
            charCount++;
        } else if ((c & 0x80) == 0) {
            if (charCount >= MAX_CHARS_PER_LINE) {
                lines[lineCount++] = currentLine;
                currentLine = "";
                charCount = 0;
            }
            currentLine += c;
            charCount++;
        } else {
            currentLine += c;
        }
    }
    
    if (currentLine.length() > 0 && lineCount < 99) {
        lines[lineCount++] = currentLine;
    }
    
    return lineCount;
}

void showResponse() {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x13_t_cyrillic);
    
    // Показываем 3 строки из 99 возможных
    for (int i = 0; i < VISIBLE_LINES && (scrollOffset + i) < totalLines; i++) {
        u8g2.drawUTF8(0, 14 + i * 14, lines[scrollOffset + i].c_str());
    }
    
    // Индикатор прокрутки
    if (totalLines > VISIBLE_LINES) {
        String indicator = String(scrollOffset + 1) + "/" + String(totalLines - VISIBLE_LINES + 1);
        u8g2.drawUTF8(80, 56, indicator.c_str());
        
        // Маленький прогресс-бар справа (1 пиксель ширина, 4 пикселя высота)
        int barHeight = 50;  // Высота области для движения
        int barY = 5;        // Начало сверху
        int barX = 127;      // Самый правый край
        
        // Вычисляем позицию маленького индикатора
        if (totalLines > VISIBLE_LINES) {
            int maxScroll = totalLines - VISIBLE_LINES;
            int indicatorY = barY + (barHeight * scrollOffset) / maxScroll;
            
            // Рисуем маленький индикатор 1x4 пикселя
            u8g2.drawBox(barX, indicatorY, 1, 4);
        }
    }
    
    u8g2.drawUTF8(0, 56, "Зажми и говори");
    u8g2.sendBuffer();
}

void showAnimation() {
    u8g2.clearBuffer();
    const uint8_t* frame_data = animations[currentAnimation].frames + (currentFrame * animations[currentAnimation].frame_size);
    u8g2.drawXBM(0, 0, 128, 64, frame_data);
    
    u8g2.setFont(u8g2_font_6x13_t_cyrillic);
    String info = String(currentAnimation + 1) + "/" + String(ANIMATION_COUNT);
    u8g2.drawUTF8(90, 10, info.c_str());
    u8g2.sendBuffer();
}

void showWifiSelect() {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x13_t_cyrillic);
    u8g2.drawUTF8(0, 14, "Выбери WiFi:");
    u8g2.drawUTF8(0, 28, wifiNetworks[currentWifiIndex][0]);
    String nav = "< " + String(currentWifiIndex + 1) + "/" + String(WIFI_COUNT) + " >";
    u8g2.drawUTF8(0, 42, nav.c_str());
    u8g2.drawUTF8(0, 56, "Зажми для подключить");
    u8g2.sendBuffer();
}

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
    switch(type) {
        case WStype_DISCONNECTED:
            Serial.println("WebSocket Disconnected");
            isConnected = false;
            if (!gotResponse && currentState == STATE_READY) {
                showText("Сервер ОФЛАЙН", "Переподключение...");
            }
            break;
            
        case WStype_CONNECTED:
            Serial.println("WebSocket Connected");
            isConnected = true;
            gotResponse = false;
            currentState = STATE_READY;
            if (lastResponse.length() == 0) {
                showText("Сервер ОНЛАЙН!", connectedSSID.c_str(), "", "Зажми и говори");
            }
            break;
            
        case WStype_TEXT: {
            Serial.printf("Response: %s\n", payload);
            
            String response = String((char*)payload);
            
            // Проверяем команду перехода в режим Морзе
            if (response.indexOf("/morse") >= 0 || response.indexOf("/morze") >= 0) {
                currentState = STATE_MORSE;
                morseCode = "";
                morseDisplay = "";
                morseInputMode = true;  // Начинаем с ввода
                showMorse();
                Serial.println("Переход в режим Морзе");
                return;
            }
            
            lastResponse = response;
            gotResponse = true;
            
            // Если мы в режиме Морзе - показываем ответ и переходим в режим просмотра
            if (currentState == STATE_MORSE) {
                morseInputMode = false;  // Переходим в режим просмотра
                scrollOffset = 0;
                totalLines = splitTextToLines(lastResponse);
                showResponse();
            } else {
                currentState = STATE_READY;
                scrollOffset = 0;
                totalLines = splitTextToLines(lastResponse);
                showResponse();
            }
            break;
        }
    }
}

void i2s_install() {
    const i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_I2S,
        .intr_alloc_flags = 0,
        .dma_buf_count = 8,
        .dma_buf_len = BUFFER_LEN,
        .use_apll = false
    };
    i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
}

void i2s_setpin() {
    const i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_SCK,
        .ws_io_num = I2S_WS,
        .data_out_num = -1,
        .data_in_num = I2S_SD
    };
    i2s_set_pin(I2S_PORT, &pin_config);
}

bool tryConnectWifi(int index) {
    showText("Подключение:", wifiNetworks[index][0], "", "Подожди...");
    Serial.printf("Trying %s...\n", wifiNetworks[index][0]);
    WiFi.begin(wifiNetworks[index][0], wifiNetworks[index][1]);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        if (digitalRead(BUTTON_PIN) == LOW) {
            WiFi.disconnect();
            delay(200);
            return false;
        }
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        connectedSSID = String(wifiNetworks[index][0]);
        Serial.printf("\nConnected to %s\n", wifiNetworks[index][0]);
        return true;
    }
    
    WiFi.disconnect();
    return false;
}

void setup() {
    Serial.begin(115200);
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    
    u8g2.begin();
    u8g2.enableUTF8Print();
    
    EEPROM.begin(512);
    currentAnimation = EEPROM.read(EEPROM_ANIMATION_ADDR);
    if (currentAnimation >= ANIMATION_COUNT) {
        currentAnimation = 0;
    }
    
    WiFi.mode(WIFI_STA);
    i2s_install();
    i2s_setpin();
    i2s_start(I2S_PORT);
    
    currentState = STATE_ANIMATION;
    lastFrameTime = millis();
}

void loop() {
    bool buttonPressed = (digitalRead(BUTTON_PIN) == LOW);
    
    // === СОСТОЯНИЕ: АНИМАЦИЯ ===
    if (currentState == STATE_ANIMATION) {
        if (millis() - lastFrameTime > FRAME_DELAY) {
            showAnimation();
            currentFrame++;
            if (currentFrame >= animations[currentAnimation].frame_count) {
                currentFrame = 0;
            }
            lastFrameTime = millis();
        }
        
        if (buttonPressed && !buttonWasPressed) {
            buttonPressTime = millis();
            buttonWasPressed = true;
        }
        
        if (!buttonPressed && buttonWasPressed) {
            unsigned long pressDuration = millis() - buttonPressTime;
            buttonWasPressed = false;
            
            if (pressDuration < SHORT_PRESS_TIME) {
                currentAnimation = (currentAnimation + 1) % ANIMATION_COUNT;
                currentFrame = 0;
                EEPROM.write(EEPROM_ANIMATION_ADDR, currentAnimation);
                EEPROM.commit();
            } else {
                currentState = STATE_WIFI_SELECT;
                showWifiSelect();
            }
        }
        return;
    }
    
    // === СОСТОЯНИЕ: ВЫБОР WIFI ===
    if (currentState == STATE_WIFI_SELECT) {
        if (buttonPressed && !buttonWasPressed) {
            buttonPressTime = millis();
            buttonWasPressed = true;
        }
        
        if (!buttonPressed && buttonWasPressed) {
            unsigned long pressDuration = millis() - buttonPressTime;
            buttonWasPressed = false;
            
            if (pressDuration < SHORT_PRESS_TIME) {
                currentWifiIndex = (currentWifiIndex + 1) % WIFI_COUNT;
                showWifiSelect();
            } else {
                currentState = STATE_WIFI_CONNECTING;
                if (tryConnectWifi(currentWifiIndex)) {
                    showText("WiFi OK!", connectedSSID.c_str(), "", "Подключение к серверу");
                    delay(1000);
                    webSocket.beginSSL(websocket_host, websocket_port, websocket_path);
                    webSocket.onEvent(webSocketEvent);
                    webSocket.setReconnectInterval(5000);
                    showText("Подключение", "к серверу...");
                    currentState = STATE_READY;
                } else {
                    showText("Не удалось!", "Попробуй другую", "", "сеть");
                    delay(1500);
                    currentState = STATE_WIFI_SELECT;
                    showWifiSelect();
                }
            }
        }
        return;
    }
    
    webSocket.loop();
    
    // === СОСТОЯНИЕ: РЕЖИМ МОРЗЕ ===
    if (currentState == STATE_MORSE) {
        static unsigned long lastMorseInput = 0;
        
        // Если в режиме ввода Морзе
        if (morseInputMode) {
            if (buttonPressed && !morseButtonPressed) {
                morseButtonPressed = true;
                morseStartTime = millis();
                lastMorseInput = millis();
            }
            
            if (!buttonPressed && morseButtonPressed) {
                unsigned long pressDuration = millis() - morseStartTime;
                morseButtonPressed = false;
                lastMorseInput = millis();
                
                if (pressDuration >= MORSE_SEND_TIME) {
                    // Длинное нажатие - отправить код
                    if (morseCode.length() > 0) {
                        Serial.println("Отправка кода Морзе: " + morseCode);
                        webSocket.sendTXT("morse:" + morseCode);
                        showText("Отправка...", morseDisplay.c_str());
                        morseCode = "";
                        morseDisplay = "";
                        delay(500);
                    }
                } else if (pressDuration >= MORSE_DASH_TIME) {
                    // Среднее нажатие - тире
                    morseCode += "-";
                    morseDisplay += "-";
                    showMorse();
                    Serial.println("Тире: " + morseCode);
                } else if (pressDuration >= MORSE_DOT_TIME) {
                    // Короткое нажатие - точка
                    morseCode += ".";
                    morseDisplay += ".";
                    showMorse();
                    Serial.println("Точка: " + morseCode);
                }
            }
            
            // Автоматический пробел после паузы 800ms
            if (morseCode.length() > 0 && !buttonPressed) {
                if (millis() - lastMorseInput > MORSE_SPACE_TIME) {
                    if (morseCode[morseCode.length() - 1] != ' ') {
                        morseCode += " ";
                        morseDisplay += " ";
                        showMorse();
                        Serial.println("Пробел: " + morseCode);
                        lastMorseInput = millis();
                    }
                }
            }
        } 
        // Если в режиме просмотра ответа
        else {
            if (buttonPressed && !buttonWasPressed) {
                buttonPressTime = millis();
                buttonWasPressed = true;
            }
            
            if (!buttonPressed && buttonWasPressed) {
                unsigned long pressDuration = millis() - buttonPressTime;
                buttonWasPressed = false;
                
                if (pressDuration >= MORSE_SEND_TIME) {
                    // Длинное нажатие - новый ввод Морзе
                    morseInputMode = true;
                    morseCode = "";
                    morseDisplay = "";
                    showMorse();
                    Serial.println("Новый ввод Морзе");
                } else if (pressDuration < SHORT_PRESS_TIME && totalLines > VISIBLE_LINES) {
                    // Короткое нажатие - прокрутка
                    scrollOffset++;
                    if (scrollOffset > totalLines - VISIBLE_LINES) {
                        scrollOffset = 0;
                    }
                    showResponse();
                }
            }
        }
        
        delay(10);
        return;
    }
    
    if (buttonPressed && !buttonWasPressed) {
        buttonPressTime = millis();
        buttonWasPressed = true;
    }
    
    if (!buttonPressed && buttonWasPressed && !isRecording) {
        unsigned long pressDuration = millis() - buttonPressTime;
        buttonWasPressed = false;
        
        if (pressDuration < SHORT_PRESS_TIME && totalLines > VISIBLE_LINES) {
            scrollOffset++;
            if (scrollOffset > totalLines - VISIBLE_LINES) {
                scrollOffset = 0;
            }
            showResponse();
        }
    }
    
    if (buttonPressed && !isRecording && isConnected) {
        unsigned long pressDuration = millis() - buttonPressTime;
        if (pressDuration >= SHORT_PRESS_TIME) {
            isRecording = true;
            lastResponse = "";
            gotResponse = false;
            scrollOffset = 0;
            totalLines = 0;
            Serial.println("Recording started");
            showText("Запись...", "", "Говори!");
        }
    }
    
    if (isRecording && buttonPressed) {
        char i2s_read_buff[BUFFER_LEN];
        size_t bytes_read;
        i2s_read(I2S_PORT, (void*)i2s_read_buff, BUFFER_LEN, &bytes_read, portMAX_DELAY);
        webSocket.sendBIN((uint8_t*)i2s_read_buff, bytes_read);
    }
    
    if (!buttonPressed && isRecording) {
        isRecording = false;
        buttonWasPressed = false;
        Serial.println("Recording stopped, sending END_STREAM");
        webSocket.sendBIN((uint8_t*)"END_STREAM", 10);
        currentState = STATE_PROCESSING;
        showText("Обработка...", "", "Подожди");
    }
    
    delay(1);
}
