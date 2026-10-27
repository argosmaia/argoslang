# Variáveis e tipos

Uma variável pode receber o primeiro valor na declaração ou depois. O
compilador não deixa ler uma variável antes de ela receber um valor:

```argos
var answer: int = 42
var name: string = "Argos"
var enabled: bool = true
var firstValue: int
firstValue = 42
var mut total: int
total = 42
```

O primeiro valor pode ser atribuído uma vez mesmo sem `mut`. Depois disso, a
variável fica imutável; use `var mut` quando precisar alterá-la novamente.

`var` é imutável. Para reatribuir, declare `var mut`:

```argos
var mut count: int = 0
count = count + 1
```

Isto é inválido:

```argos
var count: int = 0
count = count + 1 // erro: variável imutável
```

Os nomes canônicos de tipos são minúsculos: `bool`, `byte`, `char`, `short`,
`int`, `uint`, `long`, `ulong`, `float`, `double`, `string` e `void`. No
subconjunto executável atual, `string` pode ser guardada em variáveis, passada
para funções, devolvida e impressa. `float` também usa representação binary64.
Valores inteiros podem ser convertidos para `float` ou `double`, e valores
`float` e `double` podem ser usados entre esses dois tipos. Por isso, por
exemplo, `var value: float = 2` e `var value: float = 2.0` são válidos no MVP.

## Números

Use `int` para números inteiros, como `7` ou `-12`. O lexer reconhece
inteiros em outras bases: `0b1010` é binário, `0o12` é octal e `0xA` é
hexadecimal. Também reconhece `_` para separar grupos, como em `1_000`.
Por enquanto, a rota executável aceita literais inteiros decimais; as outras
formas ainda não podem ser usadas com `run` ou `compile`.

Use `float` ou `double` para números com parte decimal ou expoente. Estas formas
decimais fazem parte do MVP executável:

```argos
var price: float = 2.5
var distance: double = 1.2e3
var half: double = 1 / 2
```

No MVP atual, `float` e `double` usam a mesma precisão de 64 bits. A divisão
`/` sempre dá um resultado real: `1 / 2` vale `0.5`, não `0`. O operador `%`
calcula o resto da divisão e só aceita inteiros.

O frontend conhece outros tipos numéricos, como `byte`, `short`, `uint`, `long`,
`ulong`, `usize` e `isize`. Mas eles ainda não podem ser usados em todos os
lugares executáveis do MVP. Para programas que rodam agora com `run` ou
`compile`, prefira `int`, `float` e `double`.

Strings são comparadas pelo conteúdo com `==` e `!=`. Ainda não existe
concatenação com `+`.

## Coleções

Uma coleção guardaria vários valores juntos, como uma lista de nomes. A
ArgosLang ainda não tem listas, mapas ou conjuntos disponíveis para programas
executáveis. Por enquanto, use variáveis separadas:

```argos
var first: int = 10
var second: int = 20
```

Não tente usar `var numbers = [1, 2, 3]` como uma lista do MVP: coleções ainda
não estão prontas.

## Opcionais

`T?` representa um valor que pode estar ausente. A ausência explícita é `nil`:

```argos
var nickname: string? = nil
var display: string = nickname ?? "Anonymous"
```

O REPL interpreta esse exemplo. A rota de VM e compilação nativa ainda possui
um subconjunto menor de tipos; consulte a página de ferramentas.
