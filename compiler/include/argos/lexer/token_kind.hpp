#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace argos {

enum class TokenKind : std::uint16_t {
    // Controle
    EndOfFile,
    Error,
    NewLine,

    // Identificadores e literais
    Identifier,
    EscapedIdentifier,
    IntegerLiteral,
    FloatLiteral,
    StringLiteral,
    MultilineStringLiteral,
    CharacterLiteral,
    Invalid,

    // Palavras-chave: declarações e módulos
    KwVar,
    KwLet,
    KwConst,
    KwMut,
    KwClass,
    KwStruct,
    KwInterface,
    KwEnum,
    KwRecord,
    KwFunction,
    KwOperator,
    KwImport,
    KwPackage,
    KwNamespace,
    KwUsing,

    // Palavras-chave: modificadores
    KwPublic,
    KwPrivate,
    KwProtected,
    KwInternal,
    KwStatic,
    KwFinal,
    KwSealed,
    KwAbstract,
    KwOverride,
    KwVirtual,
    KwUnsafe,
    KwAsync,
    KwAwait,

    // Palavras-chave: fluxo de controle
    KwIf,
    KwElse,
    KwWhile,
    KwWhen,
    KwFor,
    KwIn,
    KwSwitch,
    KwCase,
    KwDefault,
    KwBreak,
    KwContinue,
    KwReturn,
    KwTry,
    KwCatch,
    KwFinally,
    KwThrow,
    KwThrows,

    // Palavras-chave: expressões e valores
    KwNew,
    KwThis,
    KwSuper,
    KwAs,
    KwIs,
    KwTrue,
    KwFalse,
    KwNull,

    // Tipos primitivos
    KwVoid,
    KwBool,
    KwByte,
    KwShort,
    KwInt,
    KwLong,
    KwFloat,
    KwDouble,
    KwChar,
    KwString,
    KwUSize,
    KwISize,

    // Delimitadores e pontuação
    LeftParen,       // (
    RightParen,      // )
    LeftBrace,       // {
    RightBrace,      // }
    LeftBracket,     // [
    RightBracket,    // ]
    Comma,           // ,
    Colon,           // :
    Semicolon,       // ;
    Dot,             // .
    Range,           // ..
    Ellipsis,        // ...
    At,              // @
    Hash,            // #
    Arrow,           // ->
    FatArrow,        // =>

    // Operadores aritméticos
    Plus,            // +
    Minus,           // -
    Star,            // *
    Slash,           // /
    Percent,         // %
    PlusPlus,        // ++
    MinusMinus,      // --

    // Atribuição
    Equal,           // =
    PlusEqual,       // +=
    MinusEqual,      // -=
    StarEqual,       // *=
    SlashEqual,      // /=
    PercentEqual,    // %=

    // Comparação
    EqualEqual,      // ==
    BangEqual,       // !=
    StrictEqual,     // ===
    StrictBangEqual, // !==
    Less,            // <
    LessEqual,       // <=
    Greater,         // >
    GreaterEqual,    // >=
    Spaceship,       // <=>

    // Lógicos e bit a bit
    Bang,            // !
    Ampersand,       // &
    Pipe,            // |
    Caret,           // ^
    Tilde,           // ~
    AmpAmp,          // &&
    PipePipe,        // ||
    AmpEqual,        // &=
    PipeEqual,       // |=
    CaretEqual,      // ^=
    ShiftLeft,       // <<
    ShiftRight,      // >>
    ShiftLeftEqual,  // <<=
    ShiftRightEqual, // >>=

    // Optional / Null Safety do ArgosLang
    Question,             // T? ou expressão?
    QuestionDot,          // ?.
    QuestionLeftBracket,  // ?[
    SafeAssign,           // ?=
    SafeEqualityAssign,   // ?==
    NullCoalescing,       // ??
    NullCoalescingAssign, // ??=
    ForceUnwrap,          // !!
    ForceUnwrapAssign,    // !!=
};

struct SourceLocation {
    std::size_t offset{0};
    std::uint32_t line{1};
    std::uint32_t column{1};
};

struct SourceRange {
    SourceLocation begin{};
    SourceLocation end{};
};

struct Token {
    TokenKind kind{TokenKind::Error};
    std::string lexeme{};
    SourceRange range{};
    std::string diagnostic{};

    [[nodiscard]] bool is(TokenKind expected) const noexcept {
        return kind == expected;
    }

    [[nodiscard]] bool isError() const noexcept {
        return kind == TokenKind::Error;
    }
};

struct LexerOptions {
    // Necessário para que o parser implemente ponto e vírgula opcional.
    bool emitNewLines{true};

    // Assim como Swift, /* ... */ pode conter outro /* ... */.
    bool allowNestedBlockComments{true};

    // Aceita bytes UTF-8 >= 0x80 em identificadores. A validação Unicode
    // completa pode ser adicionada posteriormente com ICU/utf8proc.
    bool allowUtf8Identifiers{true};
};

} // namespace argos::lexer