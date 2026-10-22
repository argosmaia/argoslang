# Guia da Linguagem ArgosLang

Esta é a documentação principal da ArgosLang. Ela é organizada como um guia
de linguagem: cada assunto possui uma página própria, com propósito, regras,
exemplos e limites da implementação atual. A referência é da versão `0.0.1`.

## Comece aqui

1. [Instalação, build e primeiros comandos](01-instalacao-e-primeiros-passos.md)
2. [O primeiro programa](02-primeiro-programa.md)
3. [Variáveis, tipos e mutabilidade](03-variaveis-e-tipos.md)
4. [Expressões e operadores](04-expressoes-e-operadores.md)
5. [Funções](05-funcoes.md)
6. [Fluxo de controle](06-fluxo-de-controle.md)
7. [REPL](07-repl.md)
8. [Compilar, executar e diagnosticar](08-ferramentas-e-diagnosticos.md)

## Palavras reservadas para a evolução

`this` já é uma palavra-chave reservada. Ela representará a instância atual em
métodos quando classes e objetos forem implementados. Na v0.0.1, classes ainda
não existem e o uso de `this` produz um diagnóstico explícito. Caso seja
necessário usar esse nome temporariamente, escreva-o como identificador escapado
entre crases.

## Mapa da documentação

| Área | Página |
| --- | --- |
| Sintaxe formal e precedência | [Especificação gramatical](../grammar-spec.md) |
| Arquitetura do compilador | [Arquitetura](../architecture.md) |
| Escopo e prioridades do MVP | [MVP](../mvp.md) |
| Referência compacta v0.0.1 | [ArgosLang v0.0.1](../argoslang-v0.0.1.md) |

> A linguagem usa `function`, `var`, `var mut`, tipos em minúsculas e `nil`.
> Grafias antigas como `func` e `null` só existem por compatibilidade temporária.
