#include "argos/lexer/lexer.hpp"
#include "argos/diagnostics/diagnostic_catalog.hpp"

#include <cctype>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <array>
#include <algorithm>
#include <limits>
#include <charconv>
#include <unicode/uchar.h>
#include <unicode/utf8.h>

namespace argos::lexer {
namespace {
constexpr bool is_ascii_alpha(char character) noexcept {
  return (character >= 'a' && character <= 'z') ||
         (character >= 'A' && character <= 'Z');
}

constexpr bool is_ascii_digit(char character) noexcept {
  return character >= '0' && character <= '9';
}

constexpr bool is_ascii_hex_digit(char character) noexcept {
  return is_ascii_digit(character) || (character >= 'a' && character <= 'f') ||
         (character >= 'A' && character <= 'F');
}

constexpr bool is_non_ascii_byte(char character) noexcept {
  return static_cast<unsigned char>(character) >= 0x80;
}

struct CodePoint final { UChar32 value; std::size_t width; };

CodePoint decode_code_point(std::string_view source, std::size_t offset) noexcept {
  if (offset >= source.size()) return {U_SENTINEL, 0};
  if (offset > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()) ||
      source.size() > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()))
    return {U_SENTINEL, 1};
  std::int32_t index = static_cast<std::int32_t>(offset);
  UChar32 value = U_SENTINEL;
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#endif
  U8_NEXT(source.data(), index, static_cast<std::int32_t>(source.size()), value);
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
  return {value, static_cast<std::size_t>(index) - offset};
}

bool is_unicode_identifier_start(UChar32 value) noexcept {
  return value == '_' || u_isalpha(value);
}

bool is_unicode_identifier_continue(UChar32 value) noexcept {
  return is_unicode_identifier_start(value) || u_isdigit(value);
}

constexpr bool is_identifier_start(char character) noexcept {
  return is_ascii_alpha(character) || character == '_' ||
         is_non_ascii_byte(character);
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

bool can_end_statement(TokenKind kind) noexcept {
  switch (kind) {
  case TokenKind::Identifier:
  case TokenKind::EscapedIdentifier:
  case TokenKind::IntegerLiteral:
  case TokenKind::FloatLiteral:
  case TokenKind::CharacterLiteral:
  case TokenKind::StringLiteral:
  case TokenKind::MultilineStringLiteral:
  case TokenKind::KwTrue:
  case TokenKind::KwFalse:
  case TokenKind::KwNull:
  case TokenKind::KwReturn:
  case TokenKind::KwBreak:
  case TokenKind::KwContinue:
  case TokenKind::RightParen:
  case TokenKind::RightBracket:
  case TokenKind::ForceUnwrap:
  case TokenKind::PlusPlus:
  case TokenKind::MinusMinus:
    return true;
  default:
    return false;
  }
}

bool begins_continuation(char current, char next) noexcept {
  if (current == '-' && next == '>') return true;
  if (current == '!' && next == '!')
    return true;
  switch (current) {
  case '.': case '?': case '+': case '-': case '*': case '/': case '%':
  case '=': case '<': case '>': case '&': case '|': case '^': case ':':
  case ',': case '(': case '[':
    return true;
  default: return false;
  }
}
} // namespace

Lexer::Lexer(const source::SourceFile &source_file,
             diagnostics::DiagnosticEngine &diagnostics)
    : source_file_(source_file), diagnostics_(diagnostics) {}

std::vector<Token> Lexer::tokenize() {
  std::vector<Token> tokens;

  std::size_t byte_offset = 0;
  std::size_t source_line = 1;
  std::size_t source_column = 1;
  while (byte_offset < source_file_.content.size()) {
    const auto point = decode_code_point(source_file_.content, byte_offset);
    const auto width = std::max<std::size_t>(point.width, 1);
    if (point.value < 0) {
      diagnostics_.emit(diagnostics::catalog::lexer::invalid_utf8,
                        {{byte_offset, source_line, source_column},
                         {byte_offset + width, source_line, source_column + 1}});
    }
    byte_offset += width;
    if (point.value == '\n') { ++source_line; source_column = 1; }
    else ++source_column;
  }

  while (true) { // loop until EndOfFile token is produced
    skip_trivia();
    if (pending_semicolon_) { // ponto e virgula opcional
      pending_semicolon_ = false;
      tokens.push_back(Token{TokenKind::Semicolon, "", {pending_location_, pending_location_}});
      previous_kind_ = TokenKind::Semicolon;
      continue;
    }
    start_offset_ = offset_;

    auto token = scan_token();
    const auto is_eof = token.kind == TokenKind::EndOfFile;
    switch (token.kind) {
    case TokenKind::LeftParen:
    case TokenKind::QuestionLeftBracket:
    case TokenKind::LeftBracket: ++delimiter_depth_; break;
    case TokenKind::RightParen:
    case TokenKind::RightBracket: if (delimiter_depth_ > 0) --delimiter_depth_; break;
    default: break;
    }
    previous_kind_ = token.kind;
    tokens.push_back(std::move(token));

    if (is_eof) {
      break;
    }
  }

  for (auto &token : tokens) {
    token.span.begin.source_file = source_file_.path.string();
    token.span.end.source_file = source_file_.path.string();
  }
  return tokens;
}

bool Lexer::is_at_end() const noexcept {
  return offset_ >= source_file_.content.size();
}

char Lexer::current() const noexcept {
  return is_at_end() ? '\0' : source_file_.content[offset_];
}

char Lexer::peek(std::size_t distance) const noexcept {
  const auto index = offset_ + distance;
  return index >= source_file_.content.size() ? '\0'
                                              : source_file_.content[index];
}

char Lexer::advance() noexcept {
  if (is_at_end()) {
    return '\0';
  }

  const char character = source_file_.content[offset_++];

  if (character == '\n') {
    ++line_;
    column_ = 1;
  } else if ((static_cast<unsigned char>(character) & 0xC0) != 0x80) {
    ++column_;
  }

  return character;
}

bool Lexer::match(char expected) noexcept {
  if (current() != expected) {
    return false;
  }

  advance();
  return true;
}

void Lexer::skip_trivia() {
  while (!is_at_end()) {
    switch (current()) {
    case ' ':
    case '\t':
    case '\r':
      advance();
      break;
    case '\n': {
      const source::SourceLocation location{offset_, line_, column_};
      advance();
      if (delimiter_depth_ == 0 && can_end_statement(previous_kind_)) {
        pending_semicolon_ = true;
        pending_location_ = location;
      }
      break;
    }

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
      if (pending_semicolon_ && previous_kind_ != TokenKind::KwReturn &&
          begins_continuation(current(), peek()))
        pending_semicolon_ = false;
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
  bool saw_newline = false;
  source::SourceLocation newline_location{};

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
      if (current() == '\n' && !saw_newline) {
        saw_newline = true;
        newline_location = {offset_, line_, column_};
      }
      advance();
    }
  }

  if (nesting_depth != 0) {
    diagnostics_.emit(diagnostics::catalog::lexer::unterminated_block_comment,
                       source::SourceSpan{
                           .begin = begin,
                           .end =
                               source::SourceLocation{
                                   .offset = offset_,
                                   .line = line_,
                                   .column = column_,
                               },
                       });
  }
  if (saw_newline && delimiter_depth_ == 0 &&
      can_end_statement(previous_kind_)) {
    pending_semicolon_ = true;
    pending_location_ = newline_location;
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

  if (is_non_ascii_byte(character)) {
    const auto code_point = decode_code_point(source_file_.content, start_offset_);
    if (code_point.value < 0)
      return make_invalid_token(begin, "sequência UTF-8 inválida");
    if (is_unicode_identifier_start(code_point.value)) {
      for (std::size_t i = 1; i < code_point.width; ++i) advance();
      return scan_identifier_or_keyword();
    }
    for (std::size_t i = 1; i < code_point.width; ++i) advance();
    return make_invalid_token(begin, "caractere Unicode inesperado");
  }

  if (is_ascii_alpha(character) || character == '_') {
    return scan_identifier_or_keyword();
  }

  if (is_ascii_digit(character)) {
    return scan_number();
  }

  switch (character) {
  case '"':
    if (current() == '"' && peek() == '"') {
      advance(); advance();
      return scan_multiline_string();
    }
    return scan_string();
  case '\'': return scan_character();
  case '`': return scan_escaped_identifier();

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
    if (match(':')) return make_token(TokenKind::ColonColon, begin);
    return make_token(TokenKind::Colon, begin);
  case ';':
    return make_token(TokenKind::Semicolon, begin);
  case '.':
    if (match('.')) {
      if (match('=')) return make_token(TokenKind::RangeInclusive, begin);
      if (match('.')) return make_token(TokenKind::Ellipsis, begin);
      return make_token(TokenKind::Range, begin);
    }
    return make_token(TokenKind::Dot, begin);
  case '+':
    if (match('+')) return make_token(TokenKind::PlusPlus, begin);
    if (match('=')) return make_token(TokenKind::PlusEqual, begin);
    return make_token(TokenKind::Plus, begin);
  case '*':
    if (match('=')) return make_token(TokenKind::StarEqual, begin);
    return make_token(TokenKind::Star, begin);
  case '/':
    if (match('=')) return make_token(TokenKind::SlashEqual, begin);
    return make_token(TokenKind::Slash, begin);
  case '%':
    if (match('=')) return make_token(TokenKind::PercentEqual, begin);
    return make_token(TokenKind::Percent, begin);
  case '@': return make_token(TokenKind::At, begin);
  case '[': return make_token(TokenKind::LeftBracket, begin);
  case ']': return make_token(TokenKind::RightBracket, begin);
  case '~': return make_token(TokenKind::Tilde, begin);
  case '^': return make_token(TokenKind::Caret, begin);

  case '-':
    if (match('>')) {
      return make_token(TokenKind::Arrow, begin);
    }
    if (match('-')) return make_token(TokenKind::MinusMinus, begin);
    if (match('=')) return make_token(TokenKind::MinusEqual, begin);
    return make_token(TokenKind::Minus, begin);

  case '=':
    if (match('>')) return make_token(TokenKind::FatArrow, begin);
    if (match('=')) {
      return make_token(TokenKind::EqualEqual, begin);
    }
    return make_token(TokenKind::Equal, begin);

  case '!':
    if (match('!')) {
      if (match('='))
        return make_token(TokenKind::ForceUnwrapAssign, begin);
      return make_token(TokenKind::ForceUnwrap, begin);
    }
    if (match('=')) {
      return make_token(TokenKind::BangEqual, begin);
    }
    return make_token(TokenKind::Bang, begin);

  case '<':
    if (match('=')) {
      return make_token(TokenKind::LessEqual, begin);
    }
    return make_token(TokenKind::Less, begin);

  case '>':
    if (match('=')) {
      return make_token(TokenKind::GreaterEqual, begin);
    }
    return make_token(TokenKind::Greater, begin);

  case '&':
    if (match('&')) {
      return make_token(TokenKind::AmpAmp, begin);
    }
    if (match('=')) return make_token(TokenKind::AmpEqual, begin);
    return make_token(TokenKind::Ampersand, begin);

  case '|':
    if (match('|')) {
      return make_token(TokenKind::PipePipe, begin);
    }
    if (match('=')) return make_token(TokenKind::PipeEqual, begin);
    return make_token(TokenKind::Pipe, begin);

  case '?':
    if (match('?')) {
      if (match('=')) {
        return make_token(TokenKind::NullCoalescingAssign, begin);
      }
      return make_token(TokenKind::NullCoalescing, begin);
    }

    if (match('=')) {
      if (match('=')) {
        return make_token(TokenKind::SafeEqualityAssign, begin);
      }
      return make_token(TokenKind::SafeAssign, begin);
    }

    if (match('.')) {
      return make_token(TokenKind::QuestionDot, begin);
    }
    if (match('[')) return make_token(TokenKind::QuestionLeftBracket, begin);

    return make_token(TokenKind::Question, begin);

  default:
    return make_invalid_token(begin, "caractere inesperado na entrada");
  }
}

Token Lexer::scan_identifier_or_keyword() {
  while (!is_at_end()) {
    const auto code_point = decode_code_point(source_file_.content, offset_);
    if (code_point.value < 0 || !is_unicode_identifier_continue(code_point.value)) break;
    for (std::size_t i = 0; i < code_point.width; ++i) advance();
  }

  const std::string_view lexeme{
      source_file_.content.data() + start_offset_,
      offset_ - start_offset_,
  };

  const auto begin = source::SourceLocation{
      .offset = start_offset_,
      .line = line_,
      .column = column_ - static_cast<std::size_t>(
          std::count_if(source_file_.content.begin() + static_cast<std::ptrdiff_t>(start_offset_),
                        source_file_.content.begin() + static_cast<std::ptrdiff_t>(offset_),
                        [](char byte) { return (static_cast<unsigned char>(byte) & 0xC0) != 0x80; })),
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

  bool is_float = false;
  if (!has_prefix && current() == '.' && is_ascii_digit(peek())) {
    is_float = true;
    advance();
    bool last_separator = false;
    while (is_ascii_digit(current()) || current() == '_') {
      if (current() == '_') {
        if (last_separator) malformed = true;
        last_separator = true;
      } else {
        last_separator = false;
      }
      advance();
    }
    if (last_separator) malformed = true;
  }
  if (!has_prefix && (current() == 'e' || current() == 'E')) {
    is_float = true;
    advance();
    if (current() == '+' || current() == '-') advance();
    if (!is_ascii_digit(current())) malformed = true;
    bool last_separator = false;
    while (is_ascii_digit(current()) || current() == '_') {
      if (current() == '_') {
        if (last_separator) malformed = true;
        last_separator = true;
      } else last_separator = false;
      advance();
    }
    if (last_separator) malformed = true;
    if (source_file_.content[offset_ - 1] == '_') malformed = true;
  }
  if (!has_prefix && is_float && (current() == 'f' || current() == 'F')) advance();

  if (!saw_digit || previous_was_separator || malformed) {
    return make_invalid_token(begin, "literal numérico inválido");
  }

  return make_token(is_float ? TokenKind::FloatLiteral : TokenKind::IntegerLiteral, begin);
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
            begin, "sequência de escape inválida no literal de string");
      }

      return make_token(TokenKind::StringLiteral, begin);
    }

    if (current() == '\n' || current() == '\r') {
      return make_invalid_token(
          begin, "literal de string não terminado antes da quebra de linha");
    }

    if (current() != '\\') {
      advance();
      continue;
    }

    advance(); // '\\'

    if (is_at_end()) {
      return make_invalid_token(begin, "literal de string não terminado");
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

  return make_invalid_token(begin, "literal de string não terminado");
}

Token Lexer::scan_multiline_string() {
  const source::SourceLocation begin{start_offset_, line_, column_ - 3};
  while (!is_at_end()) {
    if (current() == '"' && peek() == '"' && peek(2) == '"') {
      advance(); advance(); advance();
      return make_token(TokenKind::MultilineStringLiteral, begin);
    }
    if (current() == '\\' && peek() == '"') {
      advance(); advance();
    } else {
      advance();
    }
  }
  return make_invalid_token(begin, "string multilinha não terminada");
}

Token Lexer::scan_character() {
  const source::SourceLocation begin{start_offset_, line_, column_ - 1};
  if (is_at_end() || current() == '\n' || current() == '\'')
    return make_invalid_token(begin, "literal de caractere vazio ou não terminado");
  if (current() == '\\') {
    advance();
    if (is_at_end() || current() == '\n')
      return make_invalid_token(begin, "escape de caractere não terminado");
    bool valid = true;
    if (current() == 'u') {
      advance();
      if (!match('{')) valid = false;
      const std::size_t digits_begin = offset_;
      while (is_ascii_hex_digit(current())) advance();
      const std::size_t digits_end = offset_;
      if (!match('}') || digits_end == digits_begin ||
          digits_end - digits_begin > 6) valid = false;
      std::uint32_t scalar = 0;
      const auto *first = source_file_.content.data() + digits_begin;
      const auto [last, error] = std::from_chars(
          first, source_file_.content.data() + digits_end, scalar, 16);
      if (error != std::errc{} || last != source_file_.content.data() + digits_end ||
          !U_IS_UNICODE_CHAR(scalar)) valid = false;
    } else {
      switch (current()) {
      case '0': case 'n': case 'r': case 't': case '\\': case '\'': case '"':
        advance(); break;
      default: valid = false; advance(); break;
      }
    }
    if (!valid) {
      while (!is_at_end() && current() != '\'' && current() != '\n') advance();
      match('\'');
      return make_invalid_token(begin, "escape de caractere inválido");
    }
  } else {
    const auto code_point = decode_code_point(source_file_.content, offset_);
    if (code_point.value < 0 || !U_IS_UNICODE_CHAR(code_point.value))
      return make_invalid_token(begin, "escalar Unicode inválido");
    for (std::size_t i = 0; i < code_point.width; ++i) advance();
  }
  if (!match('\'')) {
    while (!is_at_end() && current() != '\'' && current() != '\n') advance();
    match('\'');
    return make_invalid_token(begin, "literal de caractere deve conter um escalar");
  }
  return make_token(TokenKind::CharacterLiteral, begin);
}

Token Lexer::scan_escaped_identifier() {
  const source::SourceLocation begin{start_offset_, line_, column_ - 1};
  auto code_point = decode_code_point(source_file_.content, offset_);
  if (code_point.value < 0 || !is_unicode_identifier_start(code_point.value))
    return make_invalid_token(begin, "identificador escapado inválido");
  while (!is_at_end()) {
    code_point = decode_code_point(source_file_.content, offset_);
    if (code_point.value < 0 || !is_unicode_identifier_continue(code_point.value)) break;
    for (std::size_t i = 0; i < code_point.width; ++i) advance();
  }
  if (!match('`')) return make_invalid_token(begin, "identificador escapado não terminado");
  return make_token(TokenKind::EscapedIdentifier, begin);
}

Token Lexer::make_token(TokenKind kind, source::SourceLocation begin) const {
  return Token{
      .kind = kind,
      .lexeme =
          source_file_.content.substr(start_offset_, offset_ - start_offset_),
      .span =
          source::SourceSpan{
              .begin = begin,
              .end =
                  source::SourceLocation{
                      .offset = offset_,
                      .line = line_,
                      .column = column_,
                  },
          },
  };
}

Token Lexer::make_invalid_token(source::SourceLocation begin,
                                std::string message) {
  const auto span = source::SourceSpan{
      .begin = begin,
      .end =
          source::SourceLocation{
              .offset = offset_,
              .line = line_,
              .column = column_,
          },
  };

  diagnostics_.emit(diagnostics::catalog::lexer::invalid_token, span,
                    std::move(message));

  return Token{
      .kind = TokenKind::Invalid,
      .lexeme =
          source_file_.content.substr(start_offset_, offset_ - start_offset_),
      .span = span,
  };
}

TokenKind Lexer::keyword_kind(std::string_view lexeme) noexcept {
  constexpr std::array keywords{
      std::pair{"module", TokenKind::KwModule},
      std::pair{"import", TokenKind::KwImport},
      std::pair{"type", TokenKind::KwType},
      std::pair{"construct", TokenKind::KwConstruct},
      std::pair{"deinit", TokenKind::KwDeinit},
      std::pair{"getter", TokenKind::KwGetter},
      std::pair{"setter", TokenKind::KwSetter},
      std::pair{"where", TokenKind::KwWhere},
      std::pair{"extern", TokenKind::KwExtern},
      std::pair{"inout", TokenKind::KwInOut},
      std::pair{"thread_local", TokenKind::KwThreadLocal},
      std::pair{"native", TokenKind::KwNative},
      std::pair{"operator", TokenKind::KwOperator},
      std::pair{"const", TokenKind::KwConst},
      std::pair{"class", TokenKind::KwClass},
      std::pair{"data", TokenKind::KwData},
      std::pair{"overload", TokenKind::KwOverload},
      std::pair{"protocol", TokenKind::KwProtocol},
      std::pair{"interface", TokenKind::KwInterface},
      std::pair{"enum", TokenKind::KwEnum},
      std::pair{"public", TokenKind::KwPublic},
      std::pair{"private", TokenKind::KwPrivate},
      std::pair{"protected", TokenKind::KwProtected},
      std::pair{"internal", TokenKind::KwInternal},
      std::pair{"final", TokenKind::KwFinal},
      std::pair{"sealed", TokenKind::KwSealed},
      std::pair{"permited", TokenKind::KwPermited},
      std::pair{"by", TokenKind::KwBy},
      std::pair{"with", TokenKind::KwWith},
      std::pair{"package", TokenKind::KwPackage},
      std::pair{"abstract", TokenKind::KwAbstract},
      std::pair{"override", TokenKind::KwOverride},
      std::pair{"virtual", TokenKind::KwVirtual},
      std::pair{"static", TokenKind::KwStatic},
      std::pair{"mut", TokenKind::KwMut},
      std::pair{"async", TokenKind::KwAsync},
      std::pair{"await", TokenKind::KwAwait},
      std::pair{"for", TokenKind::KwFor},
      std::pair{"do", TokenKind::KwDo},
      std::pair{"match", TokenKind::KwMatch},
      std::pair{"defer", TokenKind::KwDefer},
      std::pair{"in", TokenKind::KwIn},
      std::pair{"break", TokenKind::KwBreak},
      std::pair{"continue", TokenKind::KwContinue},
      std::pair{"case", TokenKind::KwCase},
      std::pair{"default", TokenKind::KwDefault},
      std::pair{"try", TokenKind::KwTry},
      std::pair{"catch", TokenKind::KwCatch},
      std::pair{"finally", TokenKind::KwFinally},
      std::pair{"throw", TokenKind::KwThrow},
      std::pair{"throws", TokenKind::KwThrows},
      std::pair{"new", TokenKind::KwNew},
      std::pair{"this", TokenKind::KwThis},
      std::pair{"super", TokenKind::KwSuper},
      std::pair{"as", TokenKind::KwAs},
      std::pair{"is", TokenKind::KwIs},
      std::pair{"bool", TokenKind::KwBool},
      std::pair{"byte", TokenKind::KwByte},
      std::pair{"sbyte", TokenKind::KwSByte},
      std::pair{"short", TokenKind::KwShort},
      std::pair{"ushort", TokenKind::KwUShort},
      std::pair{"int", TokenKind::KwInt},
      std::pair{"uint", TokenKind::KwUInt},
      std::pair{"long", TokenKind::KwLong},
      std::pair{"ulong", TokenKind::KwULong},
      std::pair{"isize", TokenKind::KwISize},
      std::pair{"usize", TokenKind::KwUSize},
      std::pair{"float", TokenKind::KwFloat},
      std::pair{"double", TokenKind::KwDouble},
      std::pair{"char", TokenKind::KwChar},
      std::pair{"string", TokenKind::KwString},
      std::pair{"void", TokenKind::KwVoid},
      std::pair{"never", TokenKind::KwNever},
  };
  for (const auto &[word, kind] : keywords) if (lexeme == word) return kind;
  if (lexeme == "var")
    return TokenKind::KwVar;
  if (lexeme == "function")
    return TokenKind::KwFunction;
  if (lexeme == "return")
    return TokenKind::KwReturn;
  if (lexeme == "if")
    return TokenKind::KwIf;
  if (lexeme == "else")
    return TokenKind::KwElse;
  if (lexeme == "while")
    return TokenKind::KwWhile;
  if (lexeme == "unsafe")
    return TokenKind::KwUnsafe;
  if (lexeme == "nil" || lexeme == "null")
    return TokenKind::KwNull;
  if (lexeme == "true")
    return TokenKind::KwTrue;
  if (lexeme == "false")
    return TokenKind::KwFalse;

  return TokenKind::Identifier;
}

} // namespace argos::lexer
