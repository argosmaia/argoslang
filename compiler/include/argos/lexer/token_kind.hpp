#pragma once

#include "argos/source/source_location.hpp"
#include "argos/source/source_span.hpp"

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace argos::lexer {

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
    KwProtocol,
    KwModule,
    KwType,
    KwInit,
    KwDeinit,
    KwGet,
    KwSet,
    KwWhere,
    KwExtern,
    KwInOut,
    KwThreadLocal,
    KwNative,
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
    KwDo,
    KwMatch,
    KwDefer,
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
    KwSByte,
    KwUShort,
    KwUInt,
    KwULong,
    KwNever,

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
    RangeInclusive,  // ..=
    ColonColon,      // ::
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

std::string_view token_kind_name(TokenKind kind) noexcept;

} // namespace argos::lexer
