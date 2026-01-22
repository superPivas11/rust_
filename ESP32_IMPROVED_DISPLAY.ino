// Добавь эти изменения в свой ESP32 код:

// 1. УВЕЛИЧЬ МАССИВ СТРОК (в начале файла, где объявления):
String lines[99];  // Было 20, стало 99

// 2. ЗАМЕНИ функцию splitTextToLines на эту улучшенную версию:

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
    
    // Степени: ^{n} -> ^n
    text.replace("^{", "^");
    text.replace("}", "");
    
    // Математические символы
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
    text.replace("\\infty", "∞");
    text.replace("\\sum", "Σ");
    text.replace("\\prod", "Π");
    text.replace("\\int", "∫");
    text.replace("\\sqrt", "√");
    text.replace("\\pm", "±");
    text.replace("\\times", "×");
    text.replace("\\div", "÷");
    text.replace("\\leq", "≤");
    text.replace("\\geq", "≥");
    text.replace("\\neq", "≠");
    text.replace("\\approx", "≈");
    text.replace("\\equiv", "≡");
    text.replace("\\in", "∈");
    text.replace("\\subset", "⊂");
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
    
    // Убираем оставшиеся LaTeX команды
    text.replace("\\left", "");
    text.replace("\\right", "");
    text.replace("\\begin{", "");
    text.replace("\\end{", "");
    text.replace("\\[", "");
    text.replace("\\]", "");
    text.replace("$$", "");
    text.replace("$", "");
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

// 3. ОБНОВИ функцию showResponse для отображения большего количества строк:

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
        
        // Прогресс-бар прокрутки
        int barHeight = 40;
        int barY = 10;
        int barX = 125;
        
        // Рамка
        u8g2.drawFrame(barX, barY, 2, barHeight);
        
        // Заполнение
        int fillHeight = (barHeight * VISIBLE_LINES) / totalLines;
        int fillY = barY + ((barHeight - fillHeight) * scrollOffset) / (totalLines - VISIBLE_LINES);
        u8g2.drawBox(barX, fillY, 2, fillHeight);
    }
    
    u8g2.drawUTF8(0, 56, "Зажми и говори");
    u8g2.sendBuffer();
}

// ============================================
// ИНСТРУКЦИЯ ПО ИСПОЛЬЗОВАНИЮ:
// ============================================
// 1. Замени в своём коде:
//    - String lines[20]; на String lines[99];
//    - Добавь функцию cleanLatex() перед splitTextToLines()
//    - Замени функцию splitTextToLines() на новую версию
//    - Обнови функцию showResponse() для прогресс-бара
//
// 2. Прошей ESP32
//
// 3. Теперь можно листать до 99 строк, и математические формулы
//    будут отображаться нормально:
//    - \frac{1}{2} -> 1/2
//    - \pi -> π
//    - \sqrt{x} -> √x
//    - \sum -> Σ
//    и т.д.
// ============================================
