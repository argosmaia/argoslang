#include "argos/lexer/lexer.hpp"

#include <cctype>
#include <string_view>
#include <utility>
#include <cstddef>
#include <string>

namespace argos::lexer {
namespace {
    constexpr bool is_ascii_alpha(char character) noexcept {
        return(character >= 'a' && character <= 'z') ||
            (character >= 'A' && character <= 'Z');
    }

    constexpr bool is_ascii_digit(char character) noexcept {
        return character >= '0' && character <= '9';
    }

    constexpr bool is_ascii_hex_digit(char character) noexcept {
        return is_ascii_digit(character) ||
            (character >= 'a' && character <= 'f') ||
            (character >= 'A' && character <= 'F');
    }

    constexpr bool is_non_ascii_byte(char character) noexcept {
        return static_cast<unsigned char>(character) >= 0x80;
    }

    constexpr bool is_identifier_start(char character) noexcept {
        // Identificadores podem começar com letras ASCII ou sublinhado.
        return is_ascii_alpha(character) 
            || character == '_'
            || is_non_ascii_byte(character);
    }

    constexpr bool is_identifier_continue(char character) noexcept {
        return is_identifier_start(character) || is_ascii_digit(character);
    }

    constexpr bool is_digit_for_base(char character, unsigned base) noexcept {
        switch (base) {
            case 2:
                return character == '0' || character == '1';
            case 8:
                return character >= '0' && character <= '7';
            case 10:
                return is_ascii_digit(character);
            case 16:
                return is_ascii_hex_digit(character);
            default:
                return false;
        }
    }
} // namespace

// Implementação da classe Lexer, responsável por analisar o código-fonte e gerar tokens.
Lexer::Lexer(
    const source::SourceFile& source_file, // Referência ao arquivo de código-fonte a ser analisado
    diagnostics::DiagnosticEngine& diagnostics // Referência ao mecanismo de diagnóstico para relatar erros e avisos
)
    : source_file_(source_file), // Inicializa o arquivo de código-fonte
      diagnostics_(diagnostics) { // Inicializa o mecanismo de diagnóstico
}

std::vector<Token> Lexer::tokenize() { // Função principal para tokenizar o código-fonte
    std::vector<Token> tokens; // Vetor para armazenar os tokens gerados durante a análise léxica

    while (true) { // Loop principal para percorrer o código-fonte e gerar tokens
        skip_trivia(); // Ignora espaços em branco, comentários e outros elementos triviais do código-fonte
        start_offset_ = offset_; // Atualiza o deslocamento inicial para o próximo token a ser analisado

        auto token = scan_token(); 
        const auto is_eof = token.kind == TokenKind::EndOfFile; // Verifica se o token atual é o final do arquivo (EOF)
        tokens.push_back(std::move(token)); 

        if (is_eof) { // Se o token atual for EOF, interrompe o loop
            break;
        }
    }

    return tokens;
}

// função auxiliar para verificar se o lexer chegou ao final do arquivo
bool Lexer::is_at_end() const noexcept {
    return offset_ >= source_file_.content.size();
}

// função auxiliar para obter o caractere atual no código-fonte
char Lexer::current() const noexcept {
    return is_at_end() ? '\0' : source_file_.content[offset_];
}

// função auxiliar para olhar à frente no código-fonte sem avançar o ponteiro
char Lexer::peek(std::size_t distance) const noexcept {
    const auto index = offset_ + distance;
    return index >= source_file_.content.size()
        ? '\0'
        : source_file_.content[index];
}

// função auxiliar para avançar o ponteiro no código-fonte e retornar o caractere atual
char Lexer::advance() noexcept {
    if (is_at_end()) {
        return '\0';
    }

    const char character = source_file_.content[offset_++];

    if (character == '\n') { // Se o caractere atual for uma nova linha, incrementa a linha e reseta a coluna
        ++line_;
        column_ = 1;
    } else {
        ++column_;
    }

    return character;
}

bool Lexer::match(char expected) noexcept { // Função auxiliar para verificar se o próximo caractere corresponde ao esperado
    if (current() != expected) {
        return false;
    }

    advance();
    return true;
}

void Lexer::skip_trivia() { // Função auxiliar para ignorar elementos triviais do código-fonte
    while (!is_at_end()) {
        switch (current()) {
            case ' ':
            case '\t':
            case '\r':
            case '\n':
                advance();
                break;

            case '/':
                if (peek() == '/') {
                    skip_line_comment();
                } else if (peek() == '*') {
                    skip_block_comment();
                } else {
                    return;
                }
                break;

            default:
                return;
        }
    }
}

void Lexer::skip_line_comment() {
    advance();
    advance();

    while (!is_at_end() && current() != '\n') {
        advance();
    }
}

void Lexer::skip_block_comment() {
    const auto begin = source::SourceLocation{
        .offset = offset_,
        .line = line_,
        .column = column_,
    };

    advance();
    advance();

    std::size_t nesting_depth = 1;

    while (!is_at_end() && nesting_depth > 0) {
        if (current() == '/' && peek() == '*') {
            advance();
            advance();
            ++nesting_depth;
        } else if (current() == '*' && peek() == '/') {
            advance();
            advance();
            --nesting_depth;
        } else {
            advance();
        }
    }

    if (nesting_depth != 0) {
        diagnostics_.error(
            "ARGOS-L0001",
            "comentário de bloco não terminado",
            source::SourceSpan{
                .begin = begin,
                .end = source::SourceLocation{
                    .offset = offset_,
                    .line = line_,
                    .column = column_,
                },
            }
        );
    }
}

Token Lexer::scan_token() {
    const auto begin = source::SourceLocation{
        .offset = offset_,
        .line = line_,
        .column = column_,
    };

    if (is_at_end()) {
        return make_token(TokenKind::EndOfFile, begin);
    }

    const char character = advance();

    if (is_identifier_start(character)) {
        return scan_identifier_or_keyword();
    }

    if (is_ascii_digit(character)) {
        return scan_number();
    }

    switch (character) {
        case '"':
            return scan_string();

        case '(':
            return make_token(TokenKind::LeftParen, begin);
        case ')':
            return make_token(TokenKind::RightParen, begin);
        case '{':
            return make_token(TokenKind::LeftBrace, begin);
        case '}':
            return make_token(TokenKind::RightBrace, begin);
        case ',':
            return make_token(TokenKind::Comma, begin);
        case ':':
            return make_token(TokenKind::Colon, begin);
        case ';':
            return make_token(TokenKind::Semicolon, begin);
        case '.':
            return make_token(TokenKind::Dot, begin);
        case '+':
            return make_token(TokenKind::Plus, begin);
        case '*':
            return make_token(TokenKind::Star, begin);
        case '/':
            return make_token(TokenKind::Slash, begin);

        case '-':
            if (match('>')) {
                return make_token(TokenKind::Arrow, begin);
            }
            return make_token(TokenKind::Minus, begin);

        case '=':
            if (match('=')) {
                return make_token(TokenKind::EqualEqual, begin); // isso é para o operador de igualdade (==)
            }
            return make_token(TokenKind::Equal, begin);

        case '!':
            if (match('!')) {
                return make_token(TokenKind::ForceUnwrap, begin); //  isso é para o operador de desreferenciação forçada (!!)
            }
            if (match('=')) {
                return make_token(TokenKind::BangEqual, begin); // isso é para o operador de desigualdade (!=)
            }
            return make_token(TokenKind::Bang, begin); // isso é para o operador de negação lógica (!)

        case '?':
            // Maximal munch: tenta sempre o operador mais longo primeiro.
            if (match('?')) { // null coalescing isso é para o operador de coalescência nula (??)
                if (match('=')) { 
                    return make_token(TokenKind::NullCoalescingAssign, begin); // isso é para o operador de atribuição de coalescência nula (??=)
                }
                return make_token(TokenKind::NullCoalescing, begin); // isso é para o operador de coalescência nula (??)
            }

            if (match('=')) {
                if (match('=')) { // isso é para o operador de atribuição de igualdade segura (?==)
                    return make_token(TokenKind::SafeEqualityAssign, begin);
                }
                return make_token(TokenKind::SafeAssign, begin); // isso é para o operador de atribuição segura (?=)
            }

            if (match('.')) {
                return make_token(TokenKind::QuestionDot, begin); // isso é para o operador de acesso seguro (?.)
            }

            return make_token(TokenKind::Question, begin); // isso é para o operador de tipo opcional (T?) ou expressão opcional (expressão?)

        default:
            return make_invalid_token(
                begin,
                "caractere inesperado na entrada"
            );
    }
}

Token Lexer::scan_identifier_or_keyword() {
    while (is_identifier_continue(current())) {
        advance();
    }

    const std::string_view lexeme{
        source_file_.content.data() + start_offset_,
        offset_ - start_offset_,
    };

    const auto begin = source::SourceLocation{
        .offset = start_offset_,
        .line = line_,
        .column = column_ - (offset_ - start_offset_),
    };

    return make_token(keyword_kind(lexeme), begin);
}

Token Lexer::scan_number() {
    const auto begin = source::SourceLocation{
        .offset = start_offset_,
        .line = line_,
        .column = column_ - (offset_ - start_offset_),
    };

    unsigned base = 10;
    bool has_prefix = false;

    // O primeiro dígito já foi consumido por scan_token().
    if (source_file_.content[start_offset_] == '0') {
        switch (current()) {
            case 'b':
            case 'B':
                base = 2;
                has_prefix = true;
                advance();
                break;

            case 'o':
            case 'O':
                base = 8;
                has_prefix = true;
                advance();
                break;

            case 'x':
            case 'X':
                base = 16;
                has_prefix = true;
                advance();
                break;

            default:
                break;
        }
    }

    bool saw_digit = !has_prefix;
    bool previous_was_separator = false;
    bool malformed = false;

    while (!is_at_end()) {
        const char character = current();

        if (is_digit_for_base(character, base)) {
            saw_digit = true;
            previous_was_separator = false;
            advance();
            continue;
        }

        if (character == '_') {
            if (!saw_digit || previous_was_separator) {
                malformed = true;
            }

            previous_was_separator = true;
            advance();
            continue;
        }

        // Em literais prefixados, letras ou dígitos inválidos pertencem ao
        // mesmo literal para produzir um único diagnóstico: 0b102, 0xFG etc.
        if (has_prefix && is_identifier_continue(character)) {
            malformed = true;
            advance();
            continue;
        }

        break;
    }

    if (!saw_digit || previous_was_separator || malformed) {
        return make_invalid_token(
            begin,
            "literal inteiro inválido"
        );
    }

    return make_token(TokenKind::IntegerLiteral, begin);
}

Token Lexer::scan_string() {
    const auto begin = source::SourceLocation{
        .offset = start_offset_,
        .line = line_,
        .column = column_ - 1,
    };

    bool malformed_escape = false;

    while (!is_at_end()) {
        if (current() == '"') {
            advance();

            if (malformed_escape) {
                return make_invalid_token(
                    begin,
                    "sequência de escape inválida no literal de string"
                );
            }

            return make_token(TokenKind::StringLiteral, begin);
        }

        // Strings comuns são de uma linha. Strings multilinha devem ganhar
        // uma regra própria quando o TokenKind correspondente for criado.
        if (current() == '\n' || current() == '\r') {
            return make_invalid_token(
                begin,
                "literal de string não terminado antes da quebra de linha"
            );
        }

        if (current() != '\\') {
            advance();
            continue;
        }

        advance(); // '\\'

        if (is_at_end()) {
            return make_invalid_token(
                begin,
                "literal de string não terminado"
            );
        }

        switch (current()) {
            case '0':
            case '\\':
            case 't':
            case 'n':
            case 'r':
            case '"':
                advance();
                break;

            case 'u': {
                advance();

                if (!match('{')) {
                    malformed_escape = true;
                    break;
                }

                std::size_t digit_count = 0;
                while (is_ascii_hex_digit(current())) {
                    advance();
                    ++digit_count;
                }

                const bool has_closing_brace = match('}');
                if (digit_count == 0 || digit_count > 8 || !has_closing_brace) {
                    malformed_escape = true;
                }
                break;
            }

            default:
                malformed_escape = true;
                advance();
                break;
        }
    }

    return make_invalid_token(
        begin,
        "literal de string não terminado"
    );
}

Token Lexer::make_token(
    TokenKind kind,
    source::SourceLocation begin
) const {
    return Token{
        .kind = kind,
        .lexeme = source_file_.content.substr(
            start_offset_,
            offset_ - start_offset_
        ),
        .span = source::SourceSpan{
            .begin = begin,
            .end = source::SourceLocation{
                .offset = offset_,
                .line = line_,
                .column = column_,
            },
        },
    };
}

Token Lexer::make_invalid_token(
    source::SourceLocation begin,
    std::string message
) {
    const auto span = source::SourceSpan{
        .begin = begin,
        .end = source::SourceLocation{
            .offset = offset_,
            .line = line_,
            .column = column_,
        },
    };

    diagnostics_.error(
        "ARGOS-L0002",
        std::move(message),
        span
    );

    return Token{
        .kind = TokenKind::Invalid,
        .lexeme = source_file_.content.substr(
            start_offset_,
            offset_ - start_offset_
        ),
        .span = span,
    };
}

TokenKind Lexer::keyword_kind(std::string_view lexeme) noexcept {
    if (lexeme == "let") return TokenKind::KwLet;
    if (lexeme == "var") return TokenKind::KwVar;
    if (lexeme == "func") return TokenKind::KwFunction;
    if (lexeme == "return") return TokenKind::KwReturn;
    if (lexeme == "if") return TokenKind::KwIf;
    if (lexeme == "else") return TokenKind::KwElse;
    if (lexeme == "null") return TokenKind::KwNull;
    if (lexeme == "true") return TokenKind::KwTrue;
    if (lexeme == "false") return TokenKind::KwFalse;

    return TokenKind::Identifier;
}

} // namespace argos::lexer