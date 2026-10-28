# Fluxo de controle

Fluxo de controle é o caminho que o programa segue. Hoje, o MVP tem decisões
com `if` e repetição com `while`.

## Condicionais

`if` faz uma escolha. A condição precisa ser `bool`:

```argos
var temperature: int = 25
if temperature > 20 {
    print("Está quentinho")
} else {
    print("Está frio")
}
```

O `else` é opcional. Você também pode juntar escolhas com `else if`:

```argos
if temperature > 30 {
    print("Muito quente")
} else if temperature > 20 {
    print("Agradável")
} else {
    print("Frio")
}
```

Para um corpo de uma instrução, as chaves podem ser omitidas:

```argos
if temperature > 20 print("Está quentinho")
```

## Laço `while`

`while` repete o bloco enquanto sua condição for verdadeira:

```argos
var mut count: int = 1
while count < 5 {
    print(count)
    count = count + 1
}
```

`break` para o laço agora. `continue` pula o resto desta volta e testa a
condição de novo:

```argos
var mut number: int = 0
while number < 5 {
    number = number + 1
    if number == 2 continue
    if number == 4 break
    print(number)
}
```

`break` e `continue` só podem aparecer dentro de um `while`.

## O que ainda não está disponível

| Construção | Estado na versão atual |
| --- | --- |
| `for` | Ainda não funciona. Use `while` e atualize uma variável contadora. |
| `when` | Ainda não funciona como decisão ou expressão. Use `if` e `else if`. |
| `where` | É uma palavra reservada para regras de tipos no futuro; não use como cláusula de fluxo. |
| `whenever` | Não existe como construção da linguagem ainda. |
| `with` | Não existe como construção da linguagem ainda. |
| `switch` / `match` | Ainda não funciona; use `if` e `else if`. |

Esses nomes podem aparecer em documentos de planejamento ou na gramática de
referência. Isso não quer dizer que o compilador já saiba executá-los. A lista
do subconjunto que funciona está em [MVP](../geral.md#mvp).
