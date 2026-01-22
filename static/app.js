class VoiceAssistant {
    constructor() {
        this.ws = null;
        this.mediaRecorder = null;
        this.audioChunks = [];
        this.isRecording = false;
        this.isProcessing = false;
        this.requestCount = 0;
        this.totalRequests = 0;
        this.responseTimes = [];
        this.startTime = null;
        this.lastRequestTime = 0;
        
        this.initElements();
        this.loadStats();
        this.initWebSocket();
        this.initAudio();
        this.bindEvents();
    }

    initElements() {
        this.statusDot = document.getElementById('status').querySelector('.status-dot');
        this.statusText = document.getElementById('status').querySelector('.status-text');
        this.messages = document.getElementById('messages');
        this.recordBtn = document.getElementById('recordBtn');
        this.recordStatus = document.getElementById('recordStatus');
        this.visualizer = document.getElementById('visualizer');
        this.requestCountEl = document.getElementById('requestCount');
        this.responseTimeEl = document.getElementById('responseTime');
        this.serverStatusEl = document.getElementById('serverStatus');
        this.statsGrid = document.getElementById('statsGrid');
        this.heroFeatures = document.getElementById('heroFeatures');
        
        // Text input elements
        this.voiceTab = document.getElementById('voiceTab');
        this.textTab = document.getElementById('textTab');
        this.voiceInput = document.getElementById('voiceInput');
        this.textInput = document.getElementById('textInput');
        this.messageInput = document.getElementById('messageInput');
        this.sendBtn = document.getElementById('sendBtn');
        this.charCounter = document.getElementById('charCounter');
    }

    initWebSocket() {
        const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
        const wsUrl = `${protocol}//${window.location.host}/ws`;
        
        this.connectWebSocket(wsUrl);
    }

    connectWebSocket(wsUrl) {
        this.ws = new WebSocket(wsUrl);
        
        this.ws.onopen = () => {
            this.updateStatus('online', 'Подключено');
            this.serverStatusEl.textContent = 'Online';
            this.addMessage('assistant', '✅ Подключение установлено! Можно начинать разговор.');
            
            // Запускаем ping для поддержания соединения
            this.startPing();
        };
        
        this.ws.onclose = (event) => {
            this.updateStatus('offline', 'Отключено');
            this.serverStatusEl.textContent = 'Offline';
            
            // Останавливаем ping
            this.stopPing();
            
            // Автоматическое переподключение через 3 секунды
            if (!event.wasClean) {
                this.addMessage('assistant', '🔄 Соединение потеряно. Переподключение через 3 секунды...');
                setTimeout(() => {
                    this.connectWebSocket(wsUrl);
                }, 3000);
            }
        };
        
        this.ws.onerror = (error) => {
            console.error('WebSocket error:', error);
            this.updateStatus('offline', 'Ошибка');
            this.serverStatusEl.textContent = 'Error';
        };
        
        this.ws.onmessage = (event) => {
            if (event.data === 'pong') {
                // Игнорируем pong сообщения
                return;
            }
            
            const responseTime = Date.now() - this.startTime;
            
            // Добавляем время ответа в массив для подсчета среднего
            this.responseTimes.push(responseTime);
            this.totalRequests++;
            
            // Вычисляем среднее время ответа
            const averageTime = Math.round(this.responseTimes.reduce((a, b) => a + b, 0) / this.responseTimes.length);
            
            // Обновляем статистику
            this.responseTimeEl.textContent = `${averageTime}ms`;
            this.requestCountEl.textContent = this.totalRequests;
            
            // Сохраняем статистику
            this.saveStats();
            
            this.addMessage('assistant', event.data);
            this.recordStatus.textContent = 'Нажмите и говорите';
            this.visualizer.classList.remove('active');
            
            // Разблокируем интерфейс
            this.isProcessing = false;
            this.recordBtn.disabled = false;
            this.updateCharCounter(); // Обновляем состояние кнопки отправки правильно
        };
    }

    startPing() {
        this.pingInterval = setInterval(() => {
            if (this.ws && this.ws.readyState === WebSocket.OPEN) {
                this.ws.send('ping');
            }
        }, 30000); // Ping каждые 30 секунд
    }

    stopPing() {
        if (this.pingInterval) {
            clearInterval(this.pingInterval);
            this.pingInterval = null;
        }
    }

    async initAudio() {
        try {
            const stream = await navigator.mediaDevices.getUserMedia({ 
                audio: {
                    sampleRate: 16000,
                    channelCount: 1,
                    echoCancellation: true,
                    noiseSuppression: true
                } 
            });
            
            this.mediaRecorder = new MediaRecorder(stream, {
                mimeType: 'audio/webm;codecs=opus'
            });
            
            this.mediaRecorder.ondataavailable = (event) => {
                if (event.data.size > 0) {
                    this.audioChunks.push(event.data);
                }
            };
            
            this.mediaRecorder.onstop = () => {
                this.processAudio();
            };
            
        } catch (error) {
            console.error('Ошибка доступа к микрофону:', error);
            this.addMessage('system', '❌ Не удалось получить доступ к микрофону. Проверьте разрешения.');
        }
    }

    bindEvents() {
        this.recordBtn.addEventListener('mousedown', () => this.startRecording());
        this.recordBtn.addEventListener('mouseup', () => this.stopRecording());
        this.recordBtn.addEventListener('mouseleave', () => this.stopRecording());
        
        // Touch events for mobile
        this.recordBtn.addEventListener('touchstart', (e) => {
            e.preventDefault();
            this.startRecording();
        });
        this.recordBtn.addEventListener('touchend', (e) => {
            e.preventDefault();
            this.stopRecording();
        });





        // Закрываем соединение при закрытии страницы
        window.addEventListener('beforeunload', () => {
            this.stopPing();
            if (this.ws) {
                this.ws.close(1000, 'Page unload');
            }
        });

        // Tab switching
        this.voiceTab.addEventListener('click', () => this.switchTab('voice'));
        this.textTab.addEventListener('click', () => this.switchTab('text'));

        // Text input events
        this.sendBtn.addEventListener('click', () => this.sendTextMessage());
        this.messageInput.addEventListener('input', () => this.updateCharCounter());
        this.messageInput.addEventListener('keydown', (e) => {
            if (e.key === 'Enter' && !e.shiftKey) {
                e.preventDefault();
                this.sendTextMessage();
            }
        });

        // Инициализируем состояние кнопки
        this.updateCharCounter();
    }

    switchTab(tab) {
        if (tab === 'voice') {
            this.voiceTab.classList.add('active');
            this.textTab.classList.remove('active');
            this.voiceInput.classList.add('active');
            this.textInput.classList.remove('active');
        } else {
            this.textTab.classList.add('active');
            this.voiceTab.classList.remove('active');
            this.textInput.classList.add('active');
            this.voiceInput.classList.remove('active');
        }
    }

    updateCharCounter() {
        const length = this.messageInput.value.length;
        this.charCounter.textContent = `${length}/500`;
        
        if (length > 450) {
            this.charCounter.style.color = '#ef4444';
        } else if (length > 400) {
            this.charCounter.style.color = '#f59e0b';
        } else {
            this.charCounter.style.color = '#6b7280';
        }

        // Кнопка активна если есть текст, не превышен лимит и не идет обработка
        this.sendBtn.disabled = length === 0 || length > 500 || this.isProcessing;
    }

    async sendTextMessage() {
        const message = this.messageInput.value.trim();
        if (!message || !this.ws || this.ws.readyState !== WebSocket.OPEN || this.isProcessing) return;

        // Проверяем таймаут 5 секунд
        const now = Date.now();
        if (now - this.lastRequestTime < 5000) {
            const remaining = Math.ceil((5000 - (now - this.lastRequestTime)) / 1000);
            this.addMessage('assistant', `⏱️ Подождите ${remaining} секунд перед следующим запросом`);
            return;
        }

        this.isProcessing = true;
        this.lastRequestTime = now;

        // Добавляем сообщение пользователя
        this.addMessage('user', message);
        
        // Очищаем поле ввода
        this.messageInput.value = '';
        this.updateCharCounter();

        // Блокируем кнопки
        this.sendBtn.disabled = true;
        this.recordBtn.disabled = true;

        // Отправляем текст напрямую через WebSocket
        this.startTime = Date.now();

        // Отправляем как текстовое сообщение с префиксом
        this.ws.send(`text:${message}`);
    }

    loadStats() {
        const saved = localStorage.getItem('voiceAssistantStats');
        if (saved) {
            const stats = JSON.parse(saved);
            this.totalRequests = stats.totalRequests || 0;
            this.responseTimes = stats.responseTimes || [];
            
            // Обновляем интерфейс
            this.requestCountEl.textContent = this.totalRequests;
            if (this.responseTimes.length > 0) {
                const averageTime = Math.round(this.responseTimes.reduce((a, b) => a + b, 0) / this.responseTimes.length);
                this.responseTimeEl.textContent = `${averageTime}ms`;
            }
        }
    }

    saveStats() {
        const stats = {
            totalRequests: this.totalRequests,
            responseTimes: this.responseTimes
        };
        localStorage.setItem('voiceAssistantStats', JSON.stringify(stats));
    }

    getCurrentTime() {
        return new Date().toLocaleTimeString('ru-RU', { 
            hour: '2-digit', 
            minute: '2-digit' 
        });
    }

    startRecording() {
        if (!this.mediaRecorder || this.isRecording || this.isProcessing) return;
        
        // Проверяем таймаут 5 секунд
        const now = Date.now();
        if (now - this.lastRequestTime < 5000) {
            const remaining = Math.ceil((5000 - (now - this.lastRequestTime)) / 1000);
            this.recordStatus.textContent = `Подождите ${remaining} сек.`;
            return;
        }
        
        this.isRecording = true;
        this.audioChunks = [];
        this.recordBtn.classList.add('recording');
        this.recordStatus.textContent = '🎙️ Запись...';
        this.visualizer.classList.add('active');
        
        this.mediaRecorder.start(100); // Collect data every 100ms
        
        this.addMessage('user', '🎤 Запись голосового сообщения...');
    }

    stopRecording() {
        if (!this.isRecording) return;
        
        this.isRecording = false;
        this.recordBtn.classList.remove('recording');
        this.recordStatus.textContent = 'Обработка...';
        
        this.mediaRecorder.stop();
    }

    async processAudio() {
        if (this.audioChunks.length === 0) return;
        
        try {
            // Convert webm to wav
            const audioBlob = new Blob(this.audioChunks, { type: 'audio/webm' });
            const arrayBuffer = await audioBlob.arrayBuffer();
            const audioContext = new AudioContext({ sampleRate: 16000 });
            const audioBuffer = await audioContext.decodeAudioData(arrayBuffer);
            
            // Convert to PCM 16-bit
            const pcmData = this.audioBufferToPCM(audioBuffer);
            
            // Send to server
            this.startTime = Date.now();
            this.requestCount++;
            this.requestCountEl.textContent = this.requestCount;
            
            this.ws.send(pcmData);
            this.ws.send(new TextEncoder().encode('END_STREAM'));
            
            // Update last user message
            const userMessages = this.messages.querySelectorAll('.message.user');
            const lastUserMessage = userMessages[userMessages.length - 1];
            if (lastUserMessage) {
                lastUserMessage.querySelector('.message-content p').textContent = '🎤 Голосовое сообщение отправлено';
            }
            
        } catch (error) {
            console.error('Ошибка обработки аудио:', error);
            this.recordStatus.textContent = 'Ошибка обработки аудио';
            this.addMessage('system', '❌ Ошибка обработки аудио. Попробуйте еще раз.');
        }
    }

    audioBufferToPCM(audioBuffer) {
        const channelData = audioBuffer.getChannelData(0);
        const pcmData = new Int16Array(channelData.length);
        
        for (let i = 0; i < channelData.length; i++) {
            const sample = Math.max(-1, Math.min(1, channelData[i]));
            pcmData[i] = sample < 0 ? sample * 0x8000 : sample * 0x7FFF;
        }
        
        return pcmData.buffer;
    }

    updateStatus(status, text) {
        this.statusDot.className = `status-dot ${status}`;
        this.statusText.textContent = text;
    }

    addMessage(type, content) {
        const messageDiv = document.createElement('div');
        messageDiv.className = `message ${type}`;
        
        // Avatar
        const avatarDiv = document.createElement('div');
        avatarDiv.className = 'message-avatar';
        const avatarIcon = document.createElement('i');
        avatarIcon.className = type === 'user' ? 'fas fa-user' : 'fas fa-robot';
        avatarDiv.appendChild(avatarIcon);
        
        // Content
        const contentDiv = document.createElement('div');
        contentDiv.className = 'message-content';
        
        const bubbleDiv = document.createElement('div');
        bubbleDiv.className = 'message-bubble';
        
        const p = document.createElement('p');
        
        // Обрабатываем математические формулы и химические уравнения
        let processedContent = this.processFormulas(content);
        p.innerHTML = processedContent;
        
        bubbleDiv.appendChild(p);
        
        const timeDiv = document.createElement('div');
        timeDiv.className = 'message-time';
        timeDiv.textContent = this.getCurrentTime();
        
        contentDiv.appendChild(bubbleDiv);
        contentDiv.appendChild(timeDiv);
        
        messageDiv.appendChild(avatarDiv);
        messageDiv.appendChild(contentDiv);
        
        this.messages.appendChild(messageDiv);
        this.messages.scrollTop = this.messages.scrollHeight;
        
        // Обновляем MathJax для новых формул
        if (window.MathJax && window.MathJax.typesetPromise) {
            window.MathJax.typesetPromise([bubbleDiv]).catch((err) => {
                console.log('MathJax typeset error:', err);
            });
        }
    }

    processFormulas(text) {
        // Заменяем специальные символы для безопасности, но сохраняем математические
        let processed = text
            .replace(/&/g, '&amp;')
            .replace(/</g, '&lt;')
            .replace(/>/g, '&gt;')
            .replace(/"/g, '&quot;')
            .replace(/'/g, '&#39;');
        
        // Восстанавливаем математические символы
        processed = processed
            .replace(/&lt;=/g, '≤')
            .replace(/&gt;=/g, '≥')
            .replace(/!=/g, '≠')
            .replace(/-&gt;/g, '→')
            .replace(/&lt;-&gt;/g, '↔');
        
        // Обрабатываем блочные формулы $$...$$
        processed = processed.replace(/\$\$(.*?)\$\$/gs, (match, formula) => {
            return `<div class="formula">$$${formula}$$</div>`;
        });
        
        // Обрабатываем инлайн формулы $...$
        processed = processed.replace(/\$([^$\n]+)\$/g, (match, formula) => {
            return `$${formula}$`;
        });
        
        // Автоматические замены для удобства
        // Простые дроби a/b -> LaTeX
        processed = processed.replace(/\b(\d+)\/(\d+)\b/g, (match, num, den) => {
            return `$\\frac{${num}}{${den}}$`;
        });
        
        // Квадратные корни √n -> LaTeX
        processed = processed.replace(/√\(([^)]+)\)/g, (match, content) => {
            return `$\\sqrt{${content}}$`;
        });
        
        processed = processed.replace(/√(\d+)/g, (match, number) => {
            return `$\\sqrt{${number}}$`;
        });
        
        // Степени x^n -> LaTeX (если не в формуле уже)
        processed = processed.replace(/(?<!\$[^$]*)\b([a-zA-Z])\^(\d+)(?![^$]*\$)/g, (match, base, power) => {
            return `$${base}^{${power}}$`;
        });
        
        // Химические формулы - простые молекулы
        processed = processed.replace(/\b([A-Z][a-z]?)(\d+)(?![^<]*>)/g, (match, element, number) => {
            return `<span class="chemical-formula">${element}<sub>${number}</sub></span>`;
        });
        
        // Ионы с зарядами
        processed = processed.replace(/\b([A-Z][a-z]?)([²³⁴⁵⁶⁷⁸⁹])([⁺⁻])/g, (match, element, power, charge) => {
            return `<span class="chemical-formula">${element}<sup>${power}${charge}</sup></span>`;
        });
        
        // Простые заряды +, -, 2+, 2-
        processed = processed.replace(/\b([A-Z][a-z]?)(\d*)([⁺⁻])/g, (match, element, number, charge) => {
            if (number) {
                return `<span class="chemical-formula">${element}<sup>${number}${charge}</sup></span>`;
            } else {
                return `<span class="chemical-formula">${element}<sup>${charge}</sup></span>`;
            }
        });
        
        // Стрелки в химических реакциях
        processed = processed.replace(/\s*->\s*/g, ' → ');
        processed = processed.replace(/\s*<->\s*/g, ' ↔ ');
        
        // Обрабатываем переводы строк
        processed = processed.replace(/\n/g, '<br>');
        
        return processed;
    }
}

// Initialize when page loads
document.addEventListener('DOMContentLoaded', () => {
    window.voiceAssistantInstance = new VoiceAssistant();
    
    // Инициализируем MathJax для существующих формул
    if (window.MathJax && window.MathJax.typesetPromise) {
        window.MathJax.typesetPromise().catch((err) => {
            console.log('MathJax initial typeset error:', err);
        });
    }
});

// Функции для тестирования формул
function testMathFormula() {
    const voiceAssistant = window.voiceAssistantInstance;
    if (voiceAssistant) {
        voiceAssistant.addMessage('user', 'Реши квадратное уравнение x² + 5x + 6 = 0');
        
        // Симуляция ответа с формулами
        setTimeout(() => {
            const response = `Решение квадратного уравнения $ax^2 + bx + c = 0$:

Для уравнения $x^2 + 5x + 6 = 0$:
- $a = 1$, $b = 5$, $c = 6$

Используем формулу: $$x = \\frac{-b \\pm \\sqrt{b^2 - 4ac}}{2a}$$

Дискриминант: $D = b^2 - 4ac = 25 - 24 = 1$

Корни: $x_1 = \\frac{-5 + 1}{2} = -2$, $x_2 = \\frac{-5 - 1}{2} = -3$

Ответ: $x_1 = -2$, $x_2 = -3$`;
            
            voiceAssistant.addMessage('assistant', response);
        }, 1000);
    }
}

function testChemistryFormula() {
    const voiceAssistant = window.voiceAssistantInstance;
    if (voiceAssistant) {
        voiceAssistant.addMessage('user', 'Покажи реакцию горения метана');
        
        // Симуляция ответа с химическими формулами
        setTimeout(() => {
            const response = `Реакция горения метана:

**Молекулярное уравнение:**
CH₄ + 2O₂ → CO₂ + 2H₂O

**Энергетика:**
- Выделяется энергия: ΔH = -890 кДж/моль
- Это экзотермическая реакция

**Ионы в растворе:**
- Метан: CH₄ (молекула)
- Кислород: O₂ (молекула)  
- Углекислый газ: CO₂ (молекула)
- Вода: H₂O (молекула)

**Применение:** основа природного газа для отопления и энергетики.`;
            
            voiceAssistant.addMessage('assistant', response);
        }, 1000);
    }
}
