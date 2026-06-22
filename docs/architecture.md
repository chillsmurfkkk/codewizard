# Архітектура codewizard

Останнє оновлення: 2026-06-22

`codewizard` - це локальний desktop RAG-застосунок для пошуку і пояснення
вихідного коду. Користувач вибирає папку з проєктом, застосунок індексує
підтримувані файли, зберігає embeddings локально, а потім відповідає на питання
за допомогою найбільш релевантних фрагментів коду.

Документ описує поточну архітектуру для людей, які хочуть зрозуміти, як система
влаштована, де живе кожна відповідальність і яким шляхом проходять дані від UI
до відповіді LLM.

## Коротко про систему

Поточна реалізація - C++20 desktop application з Dear ImGui інтерфейсом.
Застосунок працює локально, але звертається до OpenAI-compatible API для двох
операцій:

- побудова embeddings для коду і питання;
- генерація фінальної відповіді через chat completions.

Локально зберігається тільки індекс проєкту:

```text
<project>/.index/index.json
```

У спрощеному вигляді система виглядає так:

```text
Dear ImGui UI
    |
    v
RAGPipeline
    |
    +-- Indexer
    |     |
    |     +-- FileScanner -> Chunker -> EmbeddingsClient -> VectorStore
    |
    +-- Retriever
    |     |
    |     +-- VectorStore -> EmbeddingsClient -> cosine similarity search
    |
    +-- PromptBuilder
    |
    +-- LlmClient
          |
          +-- HttpClient -> OpenAI-compatible chat completions API
```

## Основні сценарії

У застосунку є два головні сценарії:

1. Індексація проєкту.
2. Відповідь на питання про вже проіндексований проєкт.

Обидва сценарії запускаються з UI, але основна логіка ізольована в
`src/core/`. UI не сканує файли, не рахує схожість і не формує HTTP-запити
напряму. Він передає ці задачі в `RAGPipeline`.

## Технологічний стек

Проєкт використовує:

- C++20;
- CMake 3.24+;
- vcpkg manifest mode;
- Dear ImGui для UI;
- GLFW + OpenGL для вікна і рендерингу;
- `cpr` для HTTP-запитів;
- `nlohmann/json` для JSON;
- `std::filesystem` для роботи з файлами, папками і шляхами.

## Структура джерел

```text
src/
  app/
    window_frame.hpp/.cpp       Рамка, рух, resize і кнопки borderless-вікна

  core/
    app_config.hpp/.cpp         Конфіг API і побудова endpoint URL
    chunker.hpp/.cpp            Розбиття файлів на overlapping line chunks
    embeddings_client.hpp/.cpp  Клієнт OpenAI-compatible embeddings API
    file_scanner.hpp/.cpp       Рекурсивне сканування підтримуваних файлів
    http_client.hpp/.cpp        Спільний JSON POST wrapper на базі cpr
    indexer.hpp/.cpp            Сканування, chunking, embeddings і запис індексу
    llm_client.hpp/.cpp         Клієнт chat completions API і system prompt
    prompt_builder.hpp/.cpp     Форматування знайденого контексту
    rag_pipeline.hpp/.cpp       Високорівнева orchestration індексації і відповіді
    retriever.hpp/.cpp          Embedding питання і пошук top-k chunks
    types.hpp                   Спільні структури даних
    vector_store.hpp/.cpp       JSON persistence і cosine similarity search

  main.cpp                      Dear ImGui app, async tasks, folder picker, UI
```

## Головні типи даних

Базові структури описані в `src/core/types.hpp`.

`SourceFile` представляє файл, який можна індексувати. Він містить абсолютний
шлях, відносний шлях від кореня проєкту і розмір файлу.

`SourceRange` описує місце фрагмента в коді: файл, початковий рядок і кінцевий
рядок.

`CodeChunk` - це текстовий фрагмент коду з унікальним `id`, `SourceRange` і
власне текстом.

`Embedding` - це `std::vector<double>`, тобто числовий вектор, отриманий від
embeddings API.

`EmbeddedChunk` об'єднує `CodeChunk` і його embedding.

`SearchResult` містить знайдений chunk і score схожості.

`ProjectIndex` містить метадані індексу і список усіх embedded chunks.

## Конфігурація

Конфіг описаний у `src/core/app_config.hpp` і читається в
`src/core/app_config.cpp`.

Локальний файл конфігурації називається:

```text
config.json
```

Він ігнорується git-ом. За основу використовується `config.example.json`:

```json
{
  "api_key": "put-your-api-key-here",
  "base_url": "https://openrouter.ai/api/v1",
  "embedding_model": "qwen/qwen3-embedding-8b",
  "llm_model": "google/gemini-3.1-flash-lite"
}
```

`AppConfig` зберігає:

- `api_key`;
- `base_url`;
- `embedding_model`;
- `llm_model`.

Endpoint-и будуються з `base_url`:

```text
embeddings:       <base_url>/embeddings
chat completions: <base_url>/chat/completions
```

## RAGPipeline як фасад системи

`RAGPipeline` живе в:

```text
src/core/rag_pipeline.hpp
src/core/rag_pipeline.cpp
```

Це головна точка входу в core-логіку для UI. Він приховує деталі створення
клієнтів і порядок викликів компонентів.

У нього є два публічні методи:

```cpp
IndexingResult index_project(
    const std::filesystem::path& project_root,
    const IndexingProgressCallback& on_progress = {}
) const;

RAGAnswer answer_question(
    const std::filesystem::path& project_root,
    const std::string& question,
    const AnsweringProgressCallback& on_progress = {}
) const;
```

`RAGPipeline` також створює:

- `EmbeddingsClient` для embeddings endpoint;
- `LlmClient` для chat completions endpoint.

Це важливо: UI не знає, як саме будуються API URL, як створюється retriever або
де лежить індекс. UI працює з одним фасадом.

## Індексація проєкту

Індексація починається з:

```text
RAGPipeline::index_project(...)
```

Далі керування переходить у:

```text
src/core/indexer.cpp
```

Повний шлях даних:

```text
project path
    |
    v
canonical project root
    |
    v
FileScanner
    |
    v
SourceFile[]
    |
    v
Chunker
    |
    v
CodeChunk[]
    |
    v
EmbeddingsClient
    |
    v
EmbeddedChunk[]
    |
    v
VectorStore
    |
    v
<project>/.index/index.json
```

### Крок 1. Нормалізація шляху

Indexer спочатку приводить шлях проєкту до canonical/weakly canonical форми.
Якщо папка не читається, він кидає помилку:

```text
Project path is not a readable directory
```

Це захищає решту pipeline від роботи з невалідним root path.

### Крок 2. Сканування файлів

Сканування реалізоване в:

```text
src/core/file_scanner.hpp
src/core/file_scanner.cpp
```

`FileScanner` рекурсивно проходить по папці проєкту через
`std::filesystem::recursive_directory_iterator`.

Підтримувані розширення:

```text
.c
.cc
.cmake
.cpp
.cppm
.cxx
.h
.hh
.hpp
.hxx
.ixx
.md
.txt
```

Підтримувані точні назви файлів:

```text
CMakeLists.txt
Dockerfile
Makefile
```

Ігноровані директорії:

```text
.git
.vs
.idea
.vscode
build
out
bin
obj
cmake-build-debug
cmake-build-release
```

Максимальний розмір файлу для індексації:

```text
1 MiB
```

Файли сортуються за відносним шляхом. Це робить порядок chunks стабільнішим між
запусками.

### Крок 3. Розбиття на chunks

Chunking реалізований у:

```text
src/core/chunker.hpp
src/core/chunker.cpp
```

Зараз використовується простий line-based підхід:

- максимум 100 рядків у chunk;
- overlap 20 рядків між сусідніми chunks.

Приклад:

```text
Chunk 1: lines 1-100
Chunk 2: lines 81-180
Chunk 3: lines 161-260
```

Overlap потрібен, щоб не втрачати контекст на межі двох chunks. Якщо функція або
клас починається в кінці одного фрагмента і продовжується в наступному, модель
має більше шансів побачити достатній контекст.

Кожен chunk містить:

- `id`;
- відносний шлях до файлу;
- початковий рядок;
- кінцевий рядок;
- текст chunk-а.

Порожні файли не створюють chunks.

### Крок 4. Побудова embeddings

Embeddings API client реалізований у:

```text
src/core/embeddings_client.hpp
src/core/embeddings_client.cpp
```

`EmbeddingsClient::embed(...)` формує JSON:

```json
{
  "model": "...",
  "input": "..."
}
```

Запит відправляється на:

```text
<base_url>/embeddings
```

Відповідь очікується в OpenAI-compatible форматі з `data[0].embedding`.

Під час індексації chunks embedding-яться паралельно. Поточний ліміт:

```text
5 concurrent embedding requests
```

Це реалізовано через `std::async`. Indexer тримає список in-flight запитів,
періодично перевіряє, які з них завершились, і записує результат у позицію
відповідного chunk-а.

### Крок 5. Збереження індексу

Індекс зберігається через:

```text
src/core/vector_store.hpp
src/core/vector_store.cpp
```

Файл індексу:

```text
<project>/.index/index.json
```

Поточна структура JSON:

```json
{
  "project_path": "C:/Projects/MyApp",
  "embedding_model": "qwen/qwen3-embedding-8b",
  "file_count": 8,
  "chunk_count": 42,
  "chunks": [
    {
      "id": 0,
      "file": "src/main.cpp",
      "start_line": 1,
      "end_line": 100,
      "text": "...",
      "embedding": [0.12, -0.45, 0.78]
    }
  ]
}
```

`.index/index.json` є локальним артефактом і не призначений для git.

## Відповідь на питання

Відповідь починається з:

```text
RAGPipeline::answer_question(...)
```

Повний шлях даних:

```text
question
    |
    v
load <project>/.index/index.json
    |
    v
EmbeddingsClient embeds question
    |
    v
VectorStore cosine similarity search
    |
    v
top-k SearchResult[]
    |
    v
PromptBuilder builds retrieved context
    |
    v
LlmClient sends system + user messages
    |
    v
RAGAnswer
```

### Крок 1. Завантаження індексу

Retriever створюється через helper:

```text
load_retriever_from_project(...)
```

Він завантажує:

```text
<project>/.index/index.json
```

Якщо файл відсутній або не читається, система повертає помилку. Тобто перед
питанням проєкт має бути проіндексований.

### Крок 2. Embedding питання

`Retriever::retrieve(...)` перевіряє, що питання не порожнє, і відправляє його в
той самий embeddings endpoint, який використовується для chunks.

Це важлива умова RAG: embeddings коду і embeddings питання мають бути з одного
embedding model space. Саме тому в індексі зберігається `embedding_model`.

Зараз система записує модель в індекс, але ще не блокує answering, якщо індекс
був створений іншою embedding model.

### Крок 3. Пошук релевантних chunks

Пошук виконує `VectorStore::search(...)`.

Алгоритм:

1. Пройти всі chunks в індексі.
2. Пропустити chunks з порожнім embedding або embedding іншої розмірності.
3. Для кожного валідного chunk-а порахувати cosine similarity з embedding питання.
4. Відсортувати найкращі результати через `std::partial_sort`.
5. Повернути top-k результатів.

Поточне значення `top_k`:

```text
6
```

Score в `SearchResult` - це cosine similarity. Чим вищий score, тим ближче chunk
до питання у embedding space.

### Крок 4. Побудова prompt context

Форматування retrieved context реалізоване в:

```text
src/core/prompt_builder.hpp
src/core/prompt_builder.cpp
```

`PromptBuilder::build_context(...)` перетворює `SearchResult[]` у текстовий
контекст для LLM.

Формат одного результату:

````text
[1] File: src/api_client.cpp, lines 20-85, score=0.823
```cpp
...
```
````

PromptBuilder підбирає code fence language для:

- C;
- C++;
- CMake;
- Markdown.

Якщо тип файлу невідомий, code fence лишається без language tag.

У `PromptBuilder` також є метод `build_user_prompt(...)`, який може зібрати
повний user prompt. Але в поточному pipeline використовується тільки
`build_context(...)`; фінальний user message збирає `LlmClient`.

### Крок 5. Запит до LLM

LLM client реалізований у:

```text
src/core/llm_client.hpp
src/core/llm_client.cpp
```

Він відправляє chat completions request на:

```text
<base_url>/chat/completions
```

Тіло запиту має приблизно таку форму:

```json
{
  "model": "...",
  "temperature": 0.2,
  "messages": [
    {
      "role": "system",
      "content": "..."
    },
    {
      "role": "user",
      "content": "Code context:\n\n...\n\nQuestion:\n...\n\nAnswer:"
    }
  ]
}
```

System prompt зараз hardcoded у `src/core/llm_client.cpp` всередині:

```cpp
std::string LlmClient::answer(
    const std::string& question,
    const std::string& context
) const
```

Його роль - змусити модель відповідати тільки на основі наданого code context,
не вигадувати деталі реалізації, пояснювати практично і вказувати файли/рядки,
коли це корисно.

Важливий архітектурний нюанс:

- `LlmClient` володіє фактичним `system` message;
- `PromptBuilder` форматує retrieved code context;
- `RAGPipeline` наразі використовує `PromptBuilder::build_context(...)`, а не
  `PromptBuilder::build_user_prompt(...)`.

Якщо prompt-и стануть частиною регулярного налаштування продукту, логічний
наступний крок - винести system prompt з `LlmClient` у конфіг або в окремий
prompt-related модуль.

## HTTP шар

Спільний HTTP wrapper живе в:

```text
src/core/http_client.hpp
src/core/http_client.cpp
```

`HttpClient::post_json(...)`:

- перевіряє, що URL не порожній;
- перевіряє, що API key не порожній;
- додає `Authorization: Bearer <api_key>`;
- додає `Content-Type: application/json`;
- відправляє `POST` через `cpr`;
- перевіряє transport error;
- перевіряє HTTP status code;
- повертає response body як string.

Цей wrapper використовується і для embeddings, і для chat completions. Завдяки
цьому обробка базових HTTP-помилок не дублюється в клієнтах моделей.

## GUI і асинхронність

Основний UI знаходиться в:

```text
src/main.cpp
```

Кастомна рамка borderless-вікна знаходиться в:

```text
src/app/window_frame.hpp
src/app/window_frame.cpp
```

Поточний UI має:

- borderless resizable GLFW window;
- кастомний title bar;
- перемикач темної і світлої теми;
- Windows font loading з підтримкою кирилиці;
- поле project path;
- кнопку `Browse` для вибору папки на Windows;
- кнопку `Index Project`;
- status line;
- поле для питання;
- кнопку `Ask`;
- панель відповіді;
- панель sources;
- debug panel із retrieved chunks;
- collapsible `Prompt context`.

Індексація і answering запускаються через `std::async`, щоб Dear ImGui продовжував
рендерити вікно під час довгих операцій. Кнопки блокуються, поки задача
виконується, щоб не запускати кілька конфліктних операцій одночасно.

Progress callbacks з core-логіки оновлюють статус, який бачить користувач.

## Обробка помилок

Система вже обробляє основні помилкові ситуації:

- невалідний або недоступний project path;
- порожній API key;
- порожня embeddings model;
- порожня LLM model;
- порожнє питання;
- порожній vector store;
- відсутній або нечитабельний index file;
- HTTP transport error;
- non-2xx HTTP response;
- некоректна embeddings response;
- некоректна LLM response;
- embedding dimension mismatch під час пошуку.

Більшість помилок у core-логіці представлені як `std::runtime_error`. UI ловить
їх і показує користувачу статус.

## Перспективи

Найпростіші точки розвитку:

- винести system prompt з `LlmClient` у `AppConfig`, `config.json` або
  `PromptBuilder`;
- додати підтримку environment variables для API key;
- не дозволяти `Ask`, якщо індекс ще не створений;
- зберігати UI settings, наприклад тему і останній project path;
- додати cancellation для індексації;
- додати retry/backoff для rate limits;
- зробити інкрементальну індексацію незмінених файлів;
- перевіряти сумісність embedding model перед answering;
- зберігати LLM model і prompt version у debug metadata відповіді.

Складніші (нові фічі, нова логіка):
- декілька режимів окрім `Ask`: `Explain`, `Find`, `Trace`, `Review`, `Refactor Plan`, `Tests`
- семантичний/евристичний chunking коду
- "доросла" векторна база даних для індексації
- історія чату