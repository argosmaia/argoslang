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

## O que existe hoje

Já dá para declarar variáveis e funções, fazer contas, comparar valores,
escrever texto com `print`, tomar decisões com `if` e repetir tarefas com
`while`. O guia marca os recursos que ainda não funcionam, para você não perder
tempo tentando usar uma parte que ainda está no desenho.

Coleções como listas e mapas, laços `for` e expressões `when`
ainda não estão disponíveis. Veja os detalhes em [variáveis e tipos](03-variaveis-e-tipos.md),
[funções](05-funcoes.md) e [fluxo de controle](06-fluxo-de-controle.md).

## Objetos e funções como valores

`this` representa a instância atual em
métodos de classes concretas. Consulte os exemplos e os limites de execução em
[Classes e interfaces](10-classes-e-interfaces.md).

## Mapa da documentação

| Área | Página |
| --- | --- |
| Sintaxe formal e precedência | [Especificação gramatical](../grammar-spec.md) |
| Arquitetura do compilador | [Arquitetura](../architecture.md) |
| Escopo e prioridades do MVP | [MVP](../mvp.md) |
| O que falta para consolidar o MVP | [Roteiro de consolidação](../mvp-roadmap.md) |
| Referência compacta v0.0.1 | [ArgosLang v0.0.1](../argoslang-v0.0.1.md) |

> A linguagem usa `function`, `var`, `var mut`, tipos em minúsculas e `nil`.
> `func` não é uma palavra-chave. `null` ainda é aceito como apelido temporário
> de `nil`.

- [Classes, interfaces e closures](10-classes-e-interfaces.md): subconjunto
  executável de POO e limites atuais de herança.
