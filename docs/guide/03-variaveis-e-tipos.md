# Variáveis e tipos

Uma variável declara nome, tipo e valor inicial. A inicialização explícita
evita que um valor não definido seja lido.

```argos
var answer: int = 42
var name: string = "Argos"
var enabled: bool = true
```

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
`int`, `uint`, `long`, `ulong`, `float`, `double`, `string` e `void`.

## Opcionais

`T?` representa um valor que pode estar ausente. A ausência explícita é `nil`:

```argos
var nickname: string? = nil
var display: string = nickname ?? "Anonymous"
```

O REPL interpreta esse exemplo. A rota de VM e compilação nativa ainda possui
um subconjunto menor de tipos; consulte a página de ferramentas.
