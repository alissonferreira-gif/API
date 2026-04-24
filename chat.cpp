
#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cctype>
#include <random>
#include <sstream>
#include <chrono>
#include <immintrin.h>

static constexpr int   VSIZE = 64;
static constexpr int   HSIZE = 32;
static constexpr int   NINT  = 10;
static constexpr float LR    = 0.04f;
static constexpr int   EPOCH = 5000;

static const char* VOCAB[VSIZE] = {
    "oi","ola","hey","bom","boa","tarde","noite","dia",
    "tchau","ate","mais","logo","adeus","falou","xau","fui",
    "qual","seu","nome","como","chama","chamas","quem","voce",
    "e","uma","ia","robo","humano","maquina","programa","bot",
    "faz","pode","fazer","capacidades","funcoes","ajuda","serve","recursos",
    "obrigado","obrigada","valeu","grato","agradeco","muito","top","legal",
    "bem","vai","esta","tudo","otimo","certo","estou","sinto",
    "xor","neural","assembly","machine","code","treino","aprendizado","pesos"
};

struct Sample { std::string text; int intent; };

static std::vector<Sample> TRAIN_DATA = {
    {"oi",0},{"ola",0},{"hey",0},{"bom dia",0},{"boa tarde",0},
    {"boa noite",0},{"oi tudo bem",0},{"ola como vai",0},
    {"salve",0},{"e ai",0},{"boas",0},{"oi oi",0},
    {"tchau",1},{"ate mais",1},{"ate logo",1},{"adeus",1},
    {"falou",1},{"xau",1},{"fui",1},{"ate",1},
    {"ate logo mais",1},{"ja vou",1},{"ate ja",1},
    {"qual seu nome",2},{"como voce se chama",2},{"quem e voce",2},
    {"qual e o seu nome",2},{"me diz seu nome",2},{"como te chama",2},
    {"voce tem nome",2},{"como posso te chamar",2},{"seu nome",2},
    {"voce e uma ia",3},{"voce e um robo",3},{"voce e humano",3},
    {"o que e voce",3},{"voce e uma maquina",3},{"e um bot",3},
    {"voce e um programa",3},{"voce e real",3},{"voce pensa",3},
    {"voce tem sentimentos",3},{"e uma inteligencia artificial",3},
    {"o que voce faz",4},{"o que pode fazer",4},{"quais suas capacidades",4},
    {"como pode me ajudar",4},{"para que serve",4},{"me ajuda",4},
    {"pode me ajudar",4},{"o que sabe fazer",4},{"suas funcoes",4},
    {"o que voce consegue fazer",4},{"voce me ajuda",4},
    {"obrigado",5},{"obrigada",5},{"valeu",5},{"muito obrigado",5},
    {"grato",5},{"agradeco",5},{"top",5},{"legal",5},
    {"muito obrigada",5},{"obrigado mesmo",5},{"valeu mesmo",5},
    {"como voce esta",6},{"tudo bem",6},{"como vai",6},{"esta bem",6},
    {"voce esta bem",6},{"tudo otimo",6},{"como voce vai",6},
    {"esta tudo bem",6},{"voce esta ok",6},{"tudo certo",6},
    {"xor",7},{"rede neural",7},{"assembly",7},{"machine code",7},
    {"treino da rede",7},{"aprendizado",7},{"pesos da rede",7},
    {"como voce foi treinado",7},{"neural",7},{"backprop",7},
    {"voce usa assembly",7},{"avx",7},{"simd",7},
    {"cala boca",8},{"voce e burro",8},{"idiota",8},
    {"voce e pessimo",8},{"horrivel",8},{"inutil",8},
    {"banana",9},{"xablau",9},{"qualquercoisa",9},
    {"sdfjksdf",9},{"zzz",9},{"abc def",9},
};

static const char* RESPONSES[][5] = {
    {"Olá! Como posso te ajudar?",
     "Oi! Tudo bem?",
     "Hey! O que você precisa?",
     "Olá! Que bom te ver por aqui.",
     "Oi! Estou pronto para conversar."},
    {"Até logo! Foi um prazer.",
     "Tchau! Volte sempre.",
     "Falou! Qualquer coisa é só chamar.",
     "Até mais!",
     "Tchau tchau!"},
    {"Meu nome é NeuralChat — uma IA feita em C++ com AVX2.",
     "Pode me chamar de NeuralChat. Sou um chatbot neural.",
     "Sou o NeuralChat, evoluído de um neural_net.asm em Assembly x86-64!",
     "NeuralChat. Uma rede neural que aprendeu a conversar.",
     "Me chamo NeuralChat. 32 neurônios ocultos a seu dispor."},
    {"Sou uma IA — um MLP treinado em C++ com SIMD AVX2.",
     "Sou um programa, não um humano. Mas aprendo com dados!",
     "Definitivamente uma máquina. Um classificador neural de intenções.",
     "Sou um bot neural. Sem consciência, mas com backpropagation!",
     "Uma rede neural artificial. Processo texto em nanossegundos."},
    {"Identifico o que você quer dizer e respondo em português.",
     "Fui treinado para classificar intenções e responder adequadamente.",
     "Converso, identifico perguntas e respondo — tudo com redes neurais!",
     "Classifico texto em 10 categorias de intenção. Rápido e leve.",
     "Entendo saudações, perguntas sobre mim, agradecimentos e mais."},
    {"De nada!",
     "Disponha!",
     "Por nada! É pra isso que existo.",
     "Fico feliz em ajudar!",
     "Sempre! Pode contar comigo."},
    {"Estou ótimo! Rodando em O3 AVX2, sem travamentos.",
     "Tudo bem! Pesos estabilizados, loss baixo.",
     "Funcionando perfeitamente. E você?",
     "Ótimo! Pronto pra processar suas perguntas.",
     "Bem e você? Posso te ajudar em algo?"},
    {"Fui evoluído de um neural_net.asm — Assembly x86-64 puro!",
     "Arquitetura: bag-of-words(64) → sigmoid(32) → softmax(10 intenções).",
     "Treino com cross-entropy + SGD em C++. AVX2 FMA acelera a matemática.",
     "XOR foi meu primeiro problema. Agora classifico texto em português!",
     "Uso FMA (_mm256_fmadd_ps) — 8 produtos acumulados por instrução."},
    {"Tudo bem... sou apenas uma rede neural, não me ofendo.",
     "Ok! Prefiro perguntas mais construtivas.",
     "Registrado. Vamos falar de outra coisa?",
     "Entendido. Posso te ajudar com algo útil?",
     "Sem problema! O que mais posso fazer por você?"},
    {"Não entendi. Pode reformular?",
     "Hmm, não reconheci isso. Tente de outro jeito.",
     "Meu vocabulário é limitado. Pode tentar de outra forma?",
     "Não sei responder isso ainda. Estou aprendendo!",
     "Não captei. Pode ser mais específico?"},
};
static constexpr int RESP_COUNT[] = {5,5,5,5,5,5,5,5,5,5};

alignas(32) static float W1[HSIZE][VSIZE];
alignas(32) static float b1[HSIZE];
alignas(32) static float W2[NINT][HSIZE];
alignas(32) static float b2[NINT];

alignas(32) static float g_h   [HSIZE];
alignas(32) static float g_out [NINT];
alignas(32) static float g_dh  [HSIZE];
alignas(32) static float g_dout[NINT];

static std::unordered_map<std::string, int> vocab_map;

static std::string normalize(const std::string& s) {
    static const std::pair<const char*, const char*> tbl[] = {
        {"á","a"},{"à","a"},{"â","a"},{"ã","a"},{"ä","a"},
        {"é","e"},{"è","e"},{"ê","e"},{"ë","e"},
        {"í","i"},{"ì","i"},{"î","i"},
        {"ó","o"},{"ò","o"},{"ô","o"},{"õ","o"},
        {"ú","u"},{"ù","u"},{"û","u"},{"ü","u"},
        {"ç","c"},{"ñ","n"},
        {"Á","a"},{"Â","a"},{"Ã","a"},{"É","e"},{"Ê","e"},
        {"Í","i"},{"Ó","o"},{"Ô","o"},{"Õ","o"},{"Ú","u"},{"Ç","c"},
    };
    std::string r = s;
    for (auto& [from, to] : tbl) {
        size_t pos;
        while ((pos = r.find(from)) != std::string::npos)
            r.replace(pos, strlen(from), to);
    }
    std::transform(r.begin(), r.end(), r.begin(),
                   [](unsigned char c){ return std::tolower(c); });
    return r;
}

static void encode(const std::string& text, float* __restrict__ vec) {
    memset(vec, 0, VSIZE * sizeof(float));
    std::string norm = normalize(text);
    std::istringstream ss(norm);
    std::string word;
    while (ss >> word) {
        auto it = vocab_map.find(word);
        if (it != vocab_map.end())
            vec[it->second] = 1.0f;
    }
}

static inline float dot_avx2(const float* __restrict__ a,
                              const float* __restrict__ b, int n) {
    __m256 acc = _mm256_setzero_ps();
    for (int i = 0; i < n; i += 8) {
        __m256 va = _mm256_loadu_ps(a + i);
        __m256 vb = _mm256_loadu_ps(b + i);
        acc = _mm256_fmadd_ps(va, vb, acc);
    }
    __m128 lo  = _mm256_castps256_ps128(acc);
    __m128 hi  = _mm256_extractf128_ps(acc, 1);
    lo = _mm_add_ps(lo, hi);
    lo = _mm_hadd_ps(lo, lo);
    lo = _mm_hadd_ps(lo, lo);
    return _mm_cvtss_f32(lo);
}

static inline float sigmoid(float x) {
    return 1.0f / (1.0f + expf(-x));
}

static void forward(const float* __restrict__ input) {
    for (int j = 0; j < HSIZE; j++)
        g_h[j] = sigmoid(dot_avx2(W1[j], input, VSIZE) + b1[j]);

    float maxv = -1e30f;
    for (int k = 0; k < NINT; k++) {
        g_out[k] = dot_avx2(W2[k], g_h, HSIZE) + b2[k];
        if (g_out[k] > maxv) maxv = g_out[k];
    }
    float sum = 0.0f;
    for (int k = 0; k < NINT; k++) { g_out[k] = expf(g_out[k] - maxv); sum += g_out[k]; }
    for (int k = 0; k < NINT; k++) g_out[k] /= sum;
}

static void backward(const float* __restrict__ input, int target) {
    for (int k = 0; k < NINT; k++)
        g_dout[k] = g_out[k] - (k == target ? 1.0f : 0.0f);

    for (int j = 0; j < HSIZE; j++) {
        float grad = 0.0f;
        for (int k = 0; k < NINT; k++) grad += g_dout[k] * W2[k][j];
        g_dh[j] = grad * g_h[j] * (1.0f - g_h[j]);
    }

    for (int k = 0; k < NINT; k++) {
        for (int j = 0; j < HSIZE; j++)
            W2[k][j] -= LR * g_dout[k] * g_h[j];
        b2[k] -= LR * g_dout[k];
    }

    for (int j = 0; j < HSIZE; j++) {
        for (int i = 0; i < VSIZE; i++)
            W1[j][i] -= LR * g_dh[j] * input[i];
        b1[j] -= LR * g_dh[j];
    }
}

static void init_weights() {
    std::mt19937 rng(42);
    float lim1 = sqrtf(6.0f / (VSIZE + HSIZE));
    float lim2 = sqrtf(6.0f / (HSIZE + NINT));
    std::uniform_real_distribution<float> d1(-lim1, lim1);
    std::uniform_real_distribution<float> d2(-lim2, lim2);
    for (int j = 0; j < HSIZE; j++) {
        for (int i = 0; i < VSIZE; i++) W1[j][i] = d1(rng);
        b1[j] = 0.0f;
    }
    for (int k = 0; k < NINT; k++) {
        for (int j = 0; j < HSIZE; j++) W2[k][j] = d2(rng);
        b2[k] = 0.0f;
    }
}

static void train() {
    alignas(32) float input[VSIZE];
    std::mt19937 rng(7);

    auto t0 = std::chrono::high_resolution_clock::now();

    printf("\033[36m  Treinando %d épocas em %zu amostras...\033[0m\n",
           EPOCH, TRAIN_DATA.size());

    for (int ep = 0; ep < EPOCH; ep++) {
        std::shuffle(TRAIN_DATA.begin(), TRAIN_DATA.end(), rng);
        float loss = 0.0f;

        for (auto& s : TRAIN_DATA) {
            encode(s.text, input);
            forward(input);
            loss -= logf(g_out[s.intent] + 1e-9f);
            backward(input, s.intent);
        }

        if ((ep + 1) % 1000 == 0)
            printf("  Epoch %4d | Loss: %.4f\n", ep+1,
                   loss / (float)TRAIN_DATA.size());
    }

    auto t1 = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double,std::milli>(t1-t0).count();
    printf("\033[32m  Pronto! %.1f ms de treinamento.\033[0m\n\n", ms);
}

static int resp_seed = 0;

static std::string predict(const std::string& text) {
    alignas(32) float input[VSIZE];
    encode(text, input);
    forward(input);

    int best = 0;
    for (int k = 1; k < NINT; k++)
        if (g_out[k] > g_out[best]) best = k;

    if (g_out[best] < 0.30f) best = NINT - 1;

    return RESPONSES[best][resp_seed++ % RESP_COUNT[best]];
}

int main() {
    for (int i = 0; i < VSIZE; i++)
        vocab_map[VOCAB[i]] = i;

    printf("\033[1;35m");
    printf("╔════════════════════════════════════════════╗\n");
    printf("║     NeuralChat  —  Assembly → C++ + AVX2  ║\n");
    printf("║  MLP 64→32→10  |  Backprop  |  Português  ║\n");
    printf("╚════════════════════════════════════════════╝\n");
    printf("\033[0m\n");

    init_weights();
    train();

    printf("\033[33m  [Digite 'sair' para encerrar]\033[0m\n\n");

    std::string line;
    while (true) {
        printf("\033[1;37mVocê:\033[0m ");
        fflush(stdout);

        if (!std::getline(std::cin, line)) break;

        std::string trimmed = line;
        trimmed.erase(0, trimmed.find_first_not_of(" \t\r\n"));
        trimmed.erase(trimmed.find_last_not_of(" \t\r\n") + 1);

        if (trimmed.empty()) continue;
        if (normalize(trimmed) == "sair") {
            printf("\033[1;35mNeuralChat:\033[0m Até logo! Foi um prazer conversar.\n");
            break;
        }

        std::string resp = predict(trimmed);
        printf("\033[1;35mNeuralChat:\033[0m %s\n\n", resp.c_str());
    }

    return 0;
}
