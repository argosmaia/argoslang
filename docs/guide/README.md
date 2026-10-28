# Guia da Linguagem ArgosLang

Esta é a documentação principal da ArgosLang. Ela é organizada como um guia
de linguagem: cada assunto possui uma página própria, com exemplos simples e
uma explicação do que já funciona e do que ainda está sendo construído. A
referência é da versão `0.0.1`.

## Comece aqui

1. [Instalação, build e primeiros comandos](01-instalacao-e-primeiros-passos.md)
2. [O primeiro programa](02-primeiro-programa.md)
3. [Variáveis, tipos e mutabilidade](03-variaveis-e-tipos.md)
4. [Expressões e operadores](04-expressoes-e-operadores.md)
5. [Funções](05-funcoes.md)
6. [Fluxo de controle](06-fluxo-de-controle.md)
7. [Strings e caracteres](09-strings-e-caracteres.md)
8. [REPL](07-repl.md)
9. [Compilar, executar e diagnosticar](08-ferramentas-e-diagnosticos.md)
10. [Classes e interfaces](10-classes-e-interfaces.md)
11. [Null-safety e tipos opcionais](11-null-safety-e-opcionais.md)
12. [Catálogo de diagnósticos](12-diagnosticos.md)

## O que existe hoje

Já dá para declarar variáveis e funções, fazer contas, comparar valores,
escrever texto com `print`, tomar decisões com `if` e repetir tarefas com
`while`. O guia marca os recursos que ainda não funcionam, para você não perder
tempo tentando usar uma parte que ainda está no desenho.

Arrays homogêneos `[T]` e arrays opcionais já executam. Mapas, conjuntos,
laços `for` e expressões `when` ainda não estão disponíveis. Veja os detalhes em [variáveis e tipos](03-variaveis-e-tipos.md),
[funções](05-funcoes.md) e [fluxo de controle](06-fluxo-de-controle.md).

## Objetos e funções como valores

`this` representa a instância atual em
métodos de classes concretas. Consulte os exemplos e os limites de execução em
[Classes e interfaces](10-classes-e-interfaces.md).

## Mapa da documentação

| Área | Página |
| --- | --- |
| Sintaxe formal e precedência | [Especificação gramatical](../geral.md#gramatica) |
| Compilar, interpretar e executar | [Explicações e exemplos](../teaching.md) |
| Diagnósticos e códigos de erro | [Catálogo](12-diagnosticos.md) |
| Arquitetura do compilador | [Arquitetura](../architecture.md) |
| Escopo e prioridades do MVP | [MVP](../geral.md#mvp) |
| O que falta para consolidar o MVP | [Roteiro de consolidação](../geral.md#roadmap) |
| Referência compacta v0.0.1 | [ArgosLang v0.0.1](../geral.md#referencia) |

> A linguagem usa `function`, `var`, `var mut`, tipos em minúsculas e `nil`.
> `func` não é uma palavra-chave. `null` ainda é aceito como apelido temporário
> de `nil`.

- [Classes, interfaces e closures](10-classes-e-interfaces.md): subconjunto
  executável de POO e limites atuais de herança.


## Opcionais e arrays — 28/10/2026

O [capítulo 11](11-null-safety-e-opcionais.md) documenta a implementação
executável: `T?`, `nil`, `??`, `?.`, `?[`, `?=`, `?==`, `??=`, `!!` e `!!=`,
com tipos rigorosos, curto-circuito, arrays opcionais no estilo Dart,
falhas localizadas e persistência de opcionais numéricos no REPL.
