/*
 * ESP32 AI Assistant с поддержкой математических формул
 * Пример кода для работы с математическими и химическими формулами
 */

#include <WiFi.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include <SSD1306Wire.h>

// WiFi настройки
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// WebSocket настройки
const char* websocket_server = "your-server.com";
const int websocket_port = 443;
const char* websocket_path = "/ws";

// OLED дисплей
SSD1306Wire display(0x3c, SDA, SCL);

WebSocketsClient webSocket;

void setup() {
    Serial.begin(115200);
    
    // Инициализация дисплея
    display.init();
    display.flipScreenVertically();
    display.setFont(ArialMT_Plain_10);
    
    // Подключение к WiFi
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.println("Подключение к WiFi...");
        display.clear();
        display.drawString(0, 0, "Подключение WiFi...");
        display.display();
    }
    
    Serial.println("WiFi подключен!");
    display.clear();
    display.drawString(0, 0, "WiFi подключен!");
    display.display();
    
    // Настройка WebSocket
    webSocket.beginSSL(websocket_server, websocket_port, websocket_path);
    webSocket.onEvent(webSocketEvent);
    webSocket.setReconnectInterval(5000);
}

void loop() {
    webSocket.loop();
    
    // Проверяем ввод с Serial для тестирования
    if (Serial.available()) {
        String input = Serial.readStringUntil('\n');
        input.trim();
        
        if (input.length() > 0) {
            // Отправляем с префиксом esp32: для идентификации
            String message = "esp32:" + input;
            webSocket.sendTXT(message);
            
            display.clear();
            display.drawString(0, 0, "Отправлено:");
            display.drawString(0, 12, input);
            display.display();
        }
    }
}

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
    switch(type) {
        case WStype_DISCONNECTED:
            Serial.println("WebSocket отключен");
            display.clear();
            display.drawString(0, 0, "Отключен");
            display.display();
            break;
            
        case WStype_CONNECTED:
            Serial.println("WebSocket подключен");
            display.clear();
            display.drawString(0, 0, "Подключен к AI");
            display.display();
            
            // Отправляем идентификацию ESP32
            webSocket.sendTXT("esp32:Привет! Я ESP32");
            break;
            
        case WStype_TEXT: {
            String response = String((char*)payload);
            Serial.println("Ответ AI: " + response);
            
            // Отображаем ответ на OLED с поддержкой формул
            displayMathResponse(response);
            break;
        }
        
        default:
            break;
    }
}

void displayMathResponse(String response) {
    display.clear();
    
    // Простая обработка математических символов для OLED
    response.replace("π", "pi");
    response.replace("α", "alpha");
    response.replace("β", "beta");
    response.replace("γ", "gamma");
    response.replace("θ", "theta");
    response.replace("λ", "lambda");
    response.replace("μ", "mu");
    response.replace("σ", "sigma");
    response.replace("Ω", "Omega");
    response.replace("Δ", "Delta");
    response.replace("∞", "inf");
    response.replace("√", "sqrt");
    response.replace("∫", "integral");
    response.replace("≤", "<=");
    response.replace("≥", ">=");
    response.replace("≠", "!=");
    response.replace("→", "->");
    response.replace("↔", "<->");
    
    // Разбиваем текст на строки для OLED
    int y = 0;
    int maxWidth = 128;
    int lineHeight = 10;
    
    String currentLine = "";
    for (int i = 0; i < response.length(); i++) {
        char c = response.charAt(i);
        
        if (c == '\n' || display.getStringWidth(currentLine + c) > maxWidth) {
            display.drawString(0, y, currentLine);
            y += lineHeight;
            currentLine = "";
            
            if (y > 54) { // Если не помещается, прокручиваем
                break;
            }
        }
        
        if (c != '\n') {
            currentLine += c;
        }
    }
    
    // Отображаем последнюю строку
    if (currentLine.length() > 0 && y <= 54) {
        display.drawString(0, y, currentLine);
    }
    
    display.display();
}

// Функция для отправки математических запросов
void sendMathQuery(String query) {
    String message = "esp32:" + query;
    webSocket.sendTXT(message);
    
    display.clear();
    display.drawString(0, 0, "Математика:");
    display.drawString(0, 12, query);
    display.display();
}

// Примеры использования:
void testMathQueries() {
    // Примеры математических запросов для ESP32
    sendMathQuery("Реши уравнение x^2 + 5x + 6 = 0");
    delay(5000);
    
    sendMathQuery("Найди производную от x^3 + 2x^2 + x");
    delay(5000);
    
    sendMathQuery("Вычисли интеграл от sin(x)");
    delay(5000);
    
    sendMathQuery("Что такое число пи?");
    delay(5000);
    
    sendMathQuery("Формула площади круга");
    delay(5000);
}

// Примеры химических запросов
void testChemistryQueries() {
    sendMathQuery("Молярная масса воды H2O");
    delay(5000);
    
    sendMathQuery("Реакция горения метана CH4");
    delay(5000);
    
    sendMathQuery("Что такое ион кальция Ca(2+)?");
    delay(5000);
    
    sendMathQuery("Формула серной кислоты");
    delay(5000);
}