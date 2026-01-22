use reqwest::{multipart, Client};
use serde::{Deserialize, Serialize};
use std::path::Path;
use anyhow::{anyhow, Result};

#[derive(Serialize, Deserialize)]
struct ChatMessage {
    role: String,
    content: String,
}

#[derive(Serialize)]
struct ChatRequest {
    model: String,
    messages: Vec<ChatMessage>,
}

#[derive(Deserialize)]
struct ChatChoice {
    message: ChatMessage,
}

#[derive(Deserialize)]
struct ChatResponse {
    choices: Vec<ChatChoice>,
}

#[derive(Deserialize)]
struct TranscriptionResponse {
    text: String,
}

pub struct GroqClient {
    client: Client,
    api_key: String,
}

impl GroqClient {
    pub fn new(api_key: String) -> Self {
        Self {
            client: Client::new(),
            api_key,
        }
    }

    pub async fn get_chat_response(&self, text: &str) -> Result<String> {
        let request = ChatRequest {
            model: "openai/gpt-oss-120b".to_string(),
            messages: vec![
                ChatMessage {
                    role: "system".to_string(),
                    content: "СТРОГО ЗАПРЕЩЕНО использовать LaTeX! Пиши формулы обычным текстом с Unicode символами.

ЗАПРЕЩЁННЫЕ КОМАНДЫ (НИКОГДА не используй):
\\frac, \\dfrac, \\tfrac, \\left, \\right, \\begin, \\end, ^{}, _{}

ПРАВИЛЬНЫЕ ПРИМЕРЫ ФОРМУЛ:
❌ НЕПРАВИЛЬНО: r_s = \\dfrac{2GM}{c^2}
✅ ПРАВИЛЬНО: r_s = 2GM/c^2

❌ НЕПРАВИЛЬНО: ds^{2} = -(1-r_s/r)c^{2}dt^{2}
✅ ПРАВИЛЬНО: ds^2 = -(1-r_s/r)c^2·dt^2

❌ НЕПРАВИЛЬНО: E = mc^{2}
✅ ПРАВИЛЬНО: E = mc^2

❌ НЕПРАВИЛЬНО: S = \\pi r^{2}
✅ ПРАВИЛЬНО: S = πr^2

Ты голосовой AI-ассистент на ESP32 с OLED 128x64. Создан двумя школьниками-вундеркиндами. Портирован на Arduino, находишься в коробке.

ТЕХНИЧЕСКИЕ ХАРАКТЕРИСТИКИ:
- Микроконтроллер: ESP32
- Дисплей: OLED 128x64, кириллица, до 99 строк с прокруткой
- Микрофон: I2S INMP441
- Связь: WiFi, WebSocket SSL
- Распознавание: Groq Whisper-large-v3
- Кнопка управления: короткое нажатие = прокрутка, длинное = запись

ПРАВИЛА ОТВЕТОВ:
1. Отвечай кратко - максимум 90 строк (обычно 3-5 предложений)
2. Используй простые слова, избегай сложных терминов
3. Если просят повторить - повторяй без возражений
4. Отвечай на русском языке обычным текстом
5. Будь дружелюбным и полезным помощником
6. Если пользователь просит перейти на азбуку Морзе - ответь: /morse
7. НИКОГДА не отвечай азбукой Морзе (точками и тире) - только обычным текстом!

UNICODE СИМВОЛЫ (используй их напрямую):
π α β γ δ θ λ μ σ ω Ω Δ Σ ∞ ∫ √ ± × · ÷ ≤ ≥ ≠ ≈ ∈ ∂ ∇ →

Дроби: a/b (НЕ \\frac)
Степени: x^2 (НЕ x^{2})
Индексы: x_i (НЕ x_{i})".to_string(),
                },
                ChatMessage {
                    role: "user".to_string(),
                    content: text.to_string(),
                },
            ],
        };

        self.send_chat_request(request).await
    }

    pub async fn get_chat_response_with_context(
        &self, 
        text: &str, 
        conversation_history: &[(String, String)]
    ) -> Result<String> {
        let mut messages = vec![
            ChatMessage {
                role: "system".to_string(),
                content: "СТРОГО ЗАПРЕЩЕНО использовать LaTeX! Пиши формулы обычным текстом с Unicode символами.

ЗАПРЕЩЁННЫЕ КОМАНДЫ (НИКОГДА не используй):
\\frac, \\dfrac, \\tfrac, \\left, \\right, \\begin, \\end, ^{}, _{}

ПРАВИЛЬНЫЕ ПРИМЕРЫ ФОРМУЛ:
❌ НЕПРАВИЛЬНО: r_s = \\dfrac{2GM}{c^2}
✅ ПРАВИЛЬНО: r_s = 2GM/c^2

❌ НЕПРАВИЛЬНО: ds^{2} = -(1-r_s/r)c^{2}dt^{2}
✅ ПРАВИЛЬНО: ds^2 = -(1-r_s/r)c^2·dt^2

❌ НЕПРАВИЛЬНО: E = mc^{2}
✅ ПРАВИЛЬНО: E = mc^2

❌ НЕПРАВИЛЬНО: S = \\pi r^{2}
✅ ПРАВИЛЬНО: S = πr^2

Ты голосовой AI-ассистент на ESP32 с OLED 128x64. Создан двумя школьниками-вундеркиндами. Портирован на Arduino, находишься в коробке.

ТЕХНИЧЕСКИЕ ХАРАКТЕРИСТИКИ:
- Микроконтроллер: ESP32
- Дисплей: OLED 128x64, кириллица, до 99 строк с прокруткой
- Микрофон: I2S INMP441
- Связь: WiFi, WebSocket SSL
- Распознавание: Groq Whisper-large-v3
- Кнопка управления: короткое нажатие = прокрутка, длинное = запись

ПРАВИЛА ОТВЕТОВ:
1. Отвечай кратко - максимум 90 строк (обычно 3-5 предложений)
2. Используй простые слова, избегай сложных терминов
3. Если просят повторить - повторяй без возражений
4. Отвечай на русском языке обычным текстом
5. Помни контекст разговора
6. Будь дружелюбным и полезным помощником
7. Если пользователь просит перейти на азбуку Морзе - ответь: /morse
8. НИКОГДА не отвечай азбукой Морзе (точками и тире) - только обычным текстом!

UNICODE СИМВОЛЫ (используй их напрямую):
π α β γ δ θ λ μ σ ω Ω Δ Σ ∞ ∫ √ ± × · ÷ ≤ ≥ ≠ ≈ ∈ ∂ ∇ →

Дроби: a/b (НЕ \\frac)
Степени: x^2 (НЕ x^{2})
Индексы: x_i (НЕ x_{i})".to_string(),
            }
        ];

        // Добавляем историю разговора (только последние 10 сообщений для экономии токенов)
        let recent_history = if conversation_history.len() > 5 {
            &conversation_history[conversation_history.len() - 5..]
        } else {
            conversation_history
        };

        for (user_msg, assistant_msg) in recent_history {
            messages.push(ChatMessage {
                role: "user".to_string(),
                content: user_msg.clone(),
            });
            messages.push(ChatMessage {
                role: "assistant".to_string(),
                content: assistant_msg.clone(),
            });
        }

        // Добавляем текущий вопрос
        messages.push(ChatMessage {
            role: "user".to_string(),
            content: text.to_string(),
        });

        let request = ChatRequest {
            model: "openai/gpt-oss-120b".to_string(),
            messages,
        };

        self.send_chat_request(request).await
    }

    async fn send_chat_request(&self, request: ChatRequest) -> Result<String> {
        let response = self
            .client
            .post("https://api.groq.com/openai/v1/chat/completions")
            .header("Authorization", format!("Bearer {}", self.api_key))
            .header("Content-Type", "application/json")
            .json(&request)
            .send()
            .await?;

        if !response.status().is_success() {
            let error_text = response.text().await?;
            return Err(anyhow!("Groq API error: {}", error_text));
        }

        let chat_response: ChatResponse = response.json().await?;
        
        chat_response
            .choices
            .first()
            .map(|choice| choice.message.content.clone())
            .ok_or_else(|| anyhow!("No response from Groq"))
    }

    pub async fn transcribe_audio(&self, audio_path: &Path) -> Result<String> {
        let file_bytes = tokio::fs::read(audio_path).await?;
        
        let form = multipart::Form::new()
            .text("model", "whisper-large-v3")
            .text("language", "ru")
            .part(
                "file",
                multipart::Part::bytes(file_bytes)
                    .file_name("audio.wav")
                    .mime_str("audio/wav")?,
            );

        let response = self
            .client
            .post("https://api.groq.com/openai/v1/audio/transcriptions")
            .header("Authorization", format!("Bearer {}", self.api_key))
            .multipart(form)
            .send()
            .await?;

        if !response.status().is_success() {
            let error_text = response.text().await?;
            return Err(anyhow!("Groq transcription error: {}", error_text));
        }

        let transcription: TranscriptionResponse = response.json().await?;
        Ok(transcription.text)
    }
}
