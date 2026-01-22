use std::collections::HashMap;

pub fn decode_morse(morse: &str) -> String {
    let morse_table = get_morse_table();
    
    // Пробуем декодировать с пробелами (если есть)
    if morse.contains(' ') {
        let words: Vec<&str> = morse.split("  ").collect();
        let mut result = String::new();
        
        for (i, word) in words.iter().enumerate() {
            let letters: Vec<&str> = word.split(' ').collect();
            for letter_code in letters {
                if let Some(letter) = morse_table.get(letter_code) {
                    result.push_str(letter);
                } else if !letter_code.is_empty() {
                    result.push('?');
                }
            }
            if i < words.len() - 1 {
                result.push(' ');
            }
        }
        return result;
    }
    
    // Декодируем сплошной текст (без пробелов) - используем динамическое программирование
    decode_continuous_dp(morse, &morse_table)
}

fn decode_continuous_dp(morse: &str, table: &HashMap<&str, &str>) -> String {
    let n = morse.len();
    if n == 0 {
        return String::new();
    }
    
    // dp[i] = (decoded_string, is_valid)
    let mut dp: Vec<Option<String>> = vec![None; n + 1];
    dp[0] = Some(String::new());
    
    for i in 0..n {
        if dp[i].is_none() {
            continue;
        }
        
        // Пробуем все возможные длины кода (1-6 символов)
        for len in 1..=6.min(n - i) {
            let code = &morse[i..i + len];
            if let Some(letter) = table.get(code) {
                let mut new_str = dp[i].as_ref().unwrap().clone();
                new_str.push_str(letter);
                
                // Если уже есть решение для этой позиции, выбираем более короткое
                if dp[i + len].is_none() || dp[i + len].as_ref().unwrap().len() > new_str.len() {
                    dp[i + len] = Some(new_str);
                }
            }
        }
    }
    
    // Возвращаем результат
    if let Some(result) = dp[n].as_ref() {
        result.clone()
    } else {
        // Если не удалось декодировать полностью, пробуем жадный алгоритм
        decode_continuous_greedy(morse, table)
    }
}

fn decode_continuous_greedy(morse: &str, table: &HashMap<&str, &str>) -> String {
    let mut result = String::new();
    let mut i = 0;
    
    while i < morse.len() {
        let mut found = false;
        
        // Пробуем найти самую длинную подходящую последовательность
        for len in (1..=6.min(morse.len() - i)).rev() {
            let code = &morse[i..i + len];
            if let Some(letter) = table.get(code) {
                result.push_str(letter);
                i += len;
                found = true;
                break;
            }
        }
        
        if !found {
            result.push('?');
            i += 1;
        }
    }
    
    result
}

fn get_morse_table() -> HashMap<&'static str, &'static str> {
    let mut table = HashMap::new();
    
    // Латиница
    table.insert(".-", "A");
    table.insert("-...", "B");
    table.insert("-.-.", "C");
    table.insert("-..", "D");
    table.insert(".", "E");
    table.insert("..-.", "F");
    table.insert("--.", "G");
    table.insert("....", "H");
    table.insert("..", "I");
    table.insert(".---", "J");
    table.insert("-.-", "K");
    table.insert(".-..", "L");
    table.insert("--", "M");
    table.insert("-.", "N");
    table.insert("---", "O");
    table.insert(".--.", "P");
    table.insert("--.-", "Q");
    table.insert(".-.", "R");
    table.insert("...", "S");
    table.insert("-", "T");
    table.insert("..-", "U");
    table.insert("...-", "V");
    table.insert(".--", "W");
    table.insert("-..-", "X");
    table.insert("-.--", "Y");
    table.insert("--..", "Z");
    
    // Кириллица
    table.insert(".-", "А");
    table.insert("-...", "Б");
    table.insert(".--", "В");
    table.insert("--.", "Г");
    table.insert("-..", "Д");
    table.insert(".", "Е");
    table.insert("...-", "Ж");
    table.insert("--..", "З");
    table.insert("..", "И");
    table.insert(".---", "Й");
    table.insert("-.-", "К");
    table.insert(".-..", "Л");
    table.insert("--", "М");
    table.insert("-.", "Н");
    table.insert("---", "О");
    table.insert(".--.", "П");
    table.insert(".-.", "Р");
    table.insert("...", "С");
    table.insert("-", "Т");
    table.insert("..-", "У");
    table.insert("..-.", "Ф");
    table.insert("....", "Х");
    table.insert("-.-.", "Ц");
    table.insert("---.", "Ч");
    table.insert("----", "Ш");
    table.insert("--.-", "Щ");
    table.insert("-.--", "Ы");
    table.insert("-..-", "Ь");
    table.insert("..-..", "Э");
    table.insert("..--", "Ю");
    table.insert(".-.-", "Я");
    
    // Цифры
    table.insert(".----", "1");
    table.insert("..---", "2");
    table.insert("...--", "3");
    table.insert("....-", "4");
    table.insert(".....", "5");
    table.insert("-....", "6");
    table.insert("--...", "7");
    table.insert("---..", "8");
    table.insert("----.", "9");
    table.insert("-----", "0");
    
    // Знаки препинания
    table.insert(".-.-.-", ".");
    table.insert("--..--", ",");
    table.insert("..--..", "?");
    table.insert("-.-.--", "!");
    
    table
}
