# Null-safety, opcionais e arrays opcionais

Desde 28/10/2026, opcionais fazem parte da execução pela VM e pelo backend
x86-64/Linux. O REPL executa os mesmos operadores e preserva opcionais numéricos
e booleanos entre entradas. As limitações de persistência de strings, objetos,
arrays e closures no REPL continuam explícitas.

## Ausência e presença

`T` contém um valor; `T?` contém presença de `T` ou ausência, escrita `nil`.
Não há conversão implícita de `T?` para `T`. Converter `T` para `T?` cria
presença e preserva inclusive `0`, `false`, texto vazio e valores reais.

```argos
var number: int? = 0
var missing: int? = nil
var name: string? = ""
var flag: bool? = false
```

`null_type` é o tipo interno do literal `nil`, não um nome de tipo para declarar
variáveis. `var x = nil` exige anotação (`ARGOS-S0102`). Inicializar `int` com
`nil` é `ARGOS-S0101`; reatribuir `nil` a `int` é `ARGOS-S0331`.
`null` permanece um alias temporário com aviso; use `nil`.

`void?` e `never?` são rejeitados. Opcionais não tornam executáveis os tipos-base
que ainda não têm backend, como `char` e inteiros estreitos. A profundidade
máxima representável nesta implementação é de 4095 camadas.

Uma declaração sem inicializador, inclusive `var x: int?`, **não** recebe `nil`
automaticamente. Ela continua não inicializada e não pode ser lida.

## Opcionais aninhados

`T??` significa `(T?)?`. Ausência externa é diferente de presença contendo
uma ausência interna. `!!` e `??` removem exatamente uma camada:

```argos
var inner: int? = nil
var present: int?? = inner
var absent: int?? = nil
print(present ?== nil)       // false
print(present!! ?== nil)     // true
print(absent ?== present)    // false
```

A conversão implícita envolve no máximo uma camada por fronteira de atribuição.
Um valor `T?` pode entrar em `T??`; um `T` não vira `T??` diretamente.
Assinaturas de funções, campos, retornos e capturas preservam a profundidade.

## `??`: valor padrão com curto-circuito

```argos
var value: int? = nil
print(value ?? 9)    // 9
```

O esquerdo exige `T?`; o direito deve ser atribuível a `T`; o resultado é `T`.
O esquerdo é avaliado uma vez. O direito só é avaliado se houver ausência
externa. Em `T?? ?? T?`, o resultado continua sendo `T?`.

## `?.`: campos, propriedades e chamadas seguras

```argos
class Person {
    public var name: string = "Argos"
    public function answer(_ n: int) -> int { return n; }
}
function main() -> int {
    var person: Person? = nil
    print(person?.name ?? "Visitante")
    print(person?.answer(42) ?? 0)
    return 0
}
```

A base exige exatamente uma camada opcional. A base é avaliada uma vez;
base ausente omite leitura, getter ou chamada e seus argumentos.
Se o membro/retorno for `U`, o resultado será `U?`. Se já for opcional,
conserva-se essa opcionalidade: navegação segura não acrescenta camadas
redundantes. Isso permite cadeias `a?.b?.c` com membros opcionais comuns.
Chamadas seguras de métodos `void` simplesmente omitem a chamada na ausência.

`.` sobre opcional é erro (`ARGOS-S0351`). `?.` sobre não opcional ou sobre
base com mais de uma camada é erro (`ARGOS-S0350`); desembrulhe as camadas
adicionais explicitamente. Escrita por `a?.campo = valor` é rejeitada.
Referências ligadas a métodos continuam fora do subconjunto executável.

## `?[`: indexação de arrays opcionais, inspirada no Dart

Arrays homogêneos usam `[T]` e literais `[a, b, c]`. `[T]?` é um array que pode
estar ausente; `[T?]` é um array presente com elementos opcionais.

```argos
var values: [int]? = [10, 20]
var missing: [int]? = nil
print(values?[1] ?? 0)     // 20
print(missing?[100] ?? 0)  // 0: nenhuma indexação acontece
var items: [int?]? = [nil, 4]
print(items?[0] ?? 8)      // 8
```

`a?[i]` avalia `a` uma vez. Se ausente, retorna `nil` e **não avalia `i`**.
Se presente, usa a indexação normal. Índices são `int` não opcionais,
começam em zero e devem estar dentro dos limites. Índice negativo ou fora
dos limites em array presente gera erro de runtime, inclusive com `?[`.
A segurança trata a ausência do array, não oculta erros de índice.

O resultado é o elemento tornado opcional; se ele já for opcional, conserva-se
seu tipo. `?[` exige array com uma camada opcional; `[]` exige array presente.
Não há indexação de strings por esse operador.

```argos
var mut values: [int] = [1, 2]
values[0] = 3
values[1] += 4
```

Arrays têm tamanho fixado na criação, identidade por referência e armazenamento
na arena da execução. A escrita exige base reatribuível (`var mut` ou campo
mutável); aliases conservam identidade. Literais são contextualizados por tipos
de declarações, parâmetros, campos e retornos; `[]` e `[nil]` precisam desse
contexto. Sem anotação, infere-se o tipo dos elementos; presença de `nil`
acrescenta opcionalidade ao elemento. Tipos de arrays são invariantes.
Arrays multidimensionais, redimensionamento, slices/views, `[T; N]` e iteração
`for` continuam pendentes. Não é permitido escrever por `a?[i]`.

## Atribuições opcionais

Todas exigem alvo mutável já inicializado e retornam seu valor final. O alvo
(receptor e índice, quando houver) é resolvido uma vez, antes da fonte.
Funcionam em variáveis locais, campos, propriedades com setter e elementos
acessados por indexação normal.

| Operador | Regra |
| --- | --- |
| `alvo ?= fonte` | Fonte deve ser opcional. Se presente, remove uma camada e escreve valor compatível; se ausente, preserva o alvo. |
| `alvo ??= fonte` | Alvo deve ser opcional. Lê o alvo uma vez e só avalia/escreve a fonte quando ausente. Aceita fonte compatível com o alvo ou com uma camada a menos. |
| `alvo !!= fonte` | Fonte deve ser opcional. Remove uma camada e atribui; ausência gera erro antes da escrita. |

```argos
var mut destination: int = 3
var source: int? = nil
destination ?= source      // mantém 3
source = 7
destination !!= source     // escreve 7
var mut cache: int? = nil
cache ??= 9                // escreve 9
cache ??= 10               // mantém 9
```

Em propriedades, `??=` chama o getter uma vez e o setter somente quando
necessário. `?=` com fonte ausente lê o getter para produzir o valor final;
com fonte presente, chama somente o setter. `!!=` chama somente o setter
após unwrap bem-sucedido. Atribuições condicionais não inicializam um alvo que
antes estava não inicializado (`ARGOS-S0337`).

## `?==`: igualdade com opcionais

Apesar do nome do token `SafeEqualityAssign`, `?==` compara e retorna `bool`;
não realiza atribuição. Avalia os dois operandos uma vez, da esquerda para a
direita. Pelo menos um precisa ser opcional ou `nil`.

- Duas ausências são iguais; ausência e presença são diferentes.
- Presenças comparam os valores internos, recursivamente por camada.
- `T?` pode ser comparado com `T`, envolvendo o segundo como presente.
- Tipos-base incompatíveis são rejeitados, sem conversão numérica implícita.
- Strings comparam conteúdo; objetos e arrays comparam identidade.
- Valores de função não têm igualdade; função opcional pode ser comparada com
  `nil` para testar ausência.

`==` e `!=` mantêm a regra mais estrita de tipos com profundidade igual,
além de comparações com `nil`; também executam igualdade estrutural de camadas.

## `!!`: unwrap explícito

```argos
var number: int? = 5
print(number!!)     // 5
```

É pós-fixado, exige opcional e remove uma camada. Sobre `nil`, a VM e o nativo
encerram com status 70 e mensagem contendo função, linha e coluna.
`!!=` usa o mesmo teste. Não há retorno silencioso de zero nem leitura de
payload ausente.

Não há smart cast: testar `x ?== nil` não altera o tipo estático de `x` no
outro ramo. Não há conversão implícita para `bool`, aritmética opcional nem
chamada direta de função opcional: escreva `f!!(...)` ou use outra operação
explícita. `expressão?` é rejeitado e não propaga ausência. `!` continua
negação lógica; não é unwrap. Não há ternário por `?`.

## Precedência

Da maior para a menor: pós-fixados, prefixos, aritmética, relações,
igualdade (`?==`), `&&`, `||`, `??` e atribuições (`?=`, `??=`, `!!=`).
Coalescência e atribuições associam à direita.

A análise de inicialização considera curto-circuito: escrever numa variável
no direito de `??`/`??=`, num índice seguro ou num argumento de chamada segura
não garante que ela esteja inicializada depois da expressão.

## Matriz de suporte

| Recurso | `check` | VM | Nativo x86-64/Linux | REPL |
| --- | --- | --- | --- | --- |
| `T?`, `nil`, `??`, `?==`, `?=`, `??=`, `!!`, `!!=` | Sim | Sim | Sim | Sim |
| `?.` campos/propriedades/métodos de classes concretas | Sim | Sim | Sim | Classes não são declarações persistentes |
| `[T]`, `[]`, `?[` | Sim | Sim | Sim | Em expressões, sem persistência de arrays |
| Persistência de opcionais numéricos/bool | Sim | Sim | Sim | Sim, inclusive camadas aninhadas |
| Persistência de string/objeto/closure/array | — | Duração da execução | Duração da execução | Não |

O suporte sempre depende do tipo-base executável. Protocolos, interfaces e
herança continuam com os limites descritos no [MVP](../geral.md#mvp).
`print` aceita opcionais dos tipos primitivos imprimíveis, escrevendo `nil`
para ausência; não imprime objetos, funções ou arrays.

## Verificação e mudanças desta etapa

A implementação acrescentou tipos opcionais e arrays tipados à IR, validação
de wrapping/unwrapping, operações da VM e geração nativa. Valores presentes
são boxes na arena; ausência usa handle/ponteiro zero sem reservar qualquer
valor do payload. O nativo libera a arena ao encerrar; a VM libera ao terminar
cada execução. O REPL reconstrói valores numéricos por fonte tipada, preservando
cada camada e mantendo o estado anterior quando uma entrada falha.

Os exemplos `minimal_null_safety.argos` e `minimal_optional_arrays.argos`
cobrem execução e efeitos em VM, nativo e assembly montado externamente.
`minimal_nil_unwrap.argos` e `minimal_array_bounds.argos` cobrem falhas de
runtime. A suíte também verifica rejeições semânticas, IR malformada, o token
único `!!=`, inicialização condicional e persistência/recuperação do REPL.


<a id="diagnosticos-erros-opcionais"></a>

## Diagnósticos principais

| Código | Regra violada |
| --- | --- |
| `ARGOS-S0101` / `ARGOS-S0331` | Inicialização/atribuição incompatível, inclusive nil em tipo não opcional |
| `ARGOS-S0102` | Tipo não inferível apenas de nil |
| `ARGOS-S0301` / `ARGOS-S0337` | Uso antes da inicialização / alvo composto não inicializado |
| `ARGOS-S0323` / `ARGOS-S0329` | Igualdade segura sem opcional ou com tipos incompatíveis |
| `ARGOS-S0324` / `ARGOS-S0325` | Coalescência sem opcional ou fallback incompatível |
| `ARGOS-S0330` | Alvo não reatribuível |
| `ARGOS-S0332` / `ARGOS-S0333` | Fonte de ?= não opcional ou valor desembrulhado incompatível |
| `ARGOS-S0334` / `ARGOS-S0335` | Alvo de ??= não opcional ou fonte incompatível |
| `ARGOS-S0338` / `ARGOS-S0339` | Fonte de !!= não opcional ou valor desembrulhado incompatível |
| `ARGOS-S0350` / `ARGOS-S0351` | Base inadequada de ?. / acesso direto a opcional |
| `ARGOS-S0361` | !! sobre não opcional |
| `ARGOS-S0375` / `ARGOS-S0376` | Base de indexação ou tipo do índice incompatível |
| `ARGOS-S0377` / `ARGOS-S0378` | Tipo de array ou elemento incompatível |
| `ARGOS-S0401` | Tipo opcional inabitável ou profundidade excessiva |
