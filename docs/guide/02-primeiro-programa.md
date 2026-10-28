# O primeiro programa

Crie `hello.argos`:

```argos
function main() -> void {
    print("Hello, world!")
}
```

Execute pela VM:

```bash
argosc run hello.argos
```

`function` começa a declaração de uma função. `main` é a porta de entrada do
programa: é por ela que tudo começa. `-> void` diz que a função não devolve um
valor. `print(...)` escreve o que está entre parênteses e pula para a próxima
linha.

## Variáveis e comandos

Uma variável é uma caixinha com nome. Diga o tipo depois dos dois-pontos:

```argos
var name: string = "Argos"
var answer: int = 42
var ready: bool = true
```

`var` não deixa trocar o valor depois da primeira atribuição. Para uma caixinha
que pode mudar, use `var mut`:

```argos
function main() -> void {
    var mut score: int = 0
    score = score + 1
    print("Pontos: ", score)
}
```

Cada linha pode terminar com `;`, mas normalmente a quebra de linha já separa
os comandos. `{` e `}` juntam comandos em um bloco. Comentários começam com
`//`:

```argos
// Este comentário é para quem está lendo o código.
var number: int = 7
```

Use retorno `int` quando o valor final for importante para outra função ou para
o status do processo. O exemplo abaixo devolve a soma:

```argos
function sum(_ left: int, _ right: int) -> int {
    return left + right
}
```

Os recursos que o compilador executa hoje estão listados em [MVP](../geral.md#mvp).
