# Funções

Funções usam `function`, parâmetros tipados e tipo de retorno após `->`:

```argos
function multiply(_ left: int, _ right: int) -> int {
    return left * right
}
```

O `_` torna o parâmetro posicional, portanto a chamada é:

```argos
var result: int = multiply(6, 7)
```

Uma função que não devolve valor usa `void`:

```argos
function greet(_ name: string) -> void {
    print("Hello, ", name)
}
```

Parâmetros e retornos podem usar `int`, `bool`, `float`, `double`, `string`,
tipos de função e classes concretas sem bases. Funções comuns aceitam até seis
parâmetros; métodos, construtores e chamadas indiretas reservam uma posição
interna e aceitam até cinco parâmetros explícitos. Consulte os limites em
[MVP](../mvp.md).

No REPL, uma declaração de função permanece disponível até `:reset`:

```text
>>> function twice(_ value: int) -> int { return value * 2 }
>>> twice(21)
42
```

## Funções como valores e closures

Uma lambda cria uma função sem nome com `=>`. Funções podem ser guardadas em
variáveis, passadas como argumentos e devolvidas por outras funções:

```argos
function apply(_ operation: (int) -> int, _ value: int) -> int {
    return operation(value);
}

function makeAdder(_ amount: int) -> (int) -> int {
    return (value: int) => value + amount;
}

function main() -> int {
    var add = makeAdder(10);
    print(apply(add, 3)); // 13
    return 0;
}
```

Os parâmetros da lambda são tipados. O retorno de um corpo de expressão é
inferido; um corpo de bloco que retorna valor exige `-> Tipo`. Capturas usam
bindings `var` imutáveis e já inicializados. `var mut` não pode ser capturado
nesta etapa. A closure pode escapar da função que a criou. Chamadas por valores
de função usam argumentos posicionais e aceitam até cinco argumentos.
