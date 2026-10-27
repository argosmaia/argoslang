# Roteiro para um MVP amplo e confiável

Este documento explica o que falta para a ArgosLang passar de um protótipo que
executa um subconjunto para uma primeira versão que pessoas consigam usar em
programas pequenos, sem descobrir limites importantes só depois de escrever o
código.

## Antes de tudo: o que quer dizer “MVP completo”?

Uma linguagem pode ter um MVP útil sem implementar todas as ideias que talvez
entrem nela no futuro. “Completo” aqui quer dizer que o conjunto escolhido para
esta primeira versão tem começo, meio e fim: dá para escrever programas reais
pequenos, receber erros que ajudem a consertá-los, executá-los de forma
previsível e confiar que as rotas suportadas concordam.

Então o objetivo não é fazer tudo que aparece na gramática de referência.
Recursos como classes, genéricos, ponteiros, exceções, concorrência e backends
para outras CPUs podem esperar. O importante é não anunciar um recurso como
pronto enquanto só uma parte dele funciona.

## Onde estamos

O [MVP atual](mvp.md) já descreve um caminho executável útil:

- `argosc check`, `run`, `compile` e `repl`;
- IR tipada e validada usada pela VM e pelo backend x86-64/Linux;
- variáveis locais e funções tipadas dentro dos limites descritos em
  [mvp.md](mvp.md);
- contas, decisões e laços do subconjunto executável;
- REPL que usa a mesma VM, em vez de um segundo interpretador da linguagem.

Mas isso ainda não é uma versão ampla e confiável da linguagem. O frontend
reconhece coisas que a VM ou o compilador nativo ainda não executam. Alguns
tipos e construções também têm suporte desigual, e não há ainda uma biblioteca
de uso geral para programas comuns. Consulte sempre a lista exata em
[Subconjunto executável](mvp.md#subconjunto-executável): exemplos e planos não
substituem testes do recurso em cada rota.

Desde a primeira versão deste roteiro, strings locais já podem ser guardadas,
passadas e devolvidas por funções, comparadas e impressas; `&&` e `||` também
já têm curto-circuito na VM e no nativo. Strings ainda não persistem entre
entradas do REPL, e vários outros itens abaixo continuam pendentes.

## O que falta, em ordem

### 1. Definir o tamanho do MVP

Antes de adicionar palavras-chave, escolha um conjunto pequeno de programas
que a primeira versão precisa conseguir escrever. Uma boa meta é:

- programas de terminal pequenos, com funções, variáveis, texto, números,
  decisões e repetição;
- coleções simples para guardar vários valores;
- entrada e saída básicas;
- erros de compilação e runtime compreensíveis;
- REPL útil para experimentar as mesmas construções;
- execução consistente pela VM e pelo binário nativo x86-64/Linux.

Escreva essa lista como contrato. Cada item deve dizer se é suportado por
`check`, `run`, `compile` e `repl`. Se uma rota não suporta o recurso, ela deve
avisar claramente, não falhar de um jeito misterioso.

### 2. Fechar os tipos que os programas realmente usam

Hoje, o núcleo numérico executável é pequeno; `string` funciona em arquivos,
mas `char` ainda não funciona e strings não persistem entre entradas do REPL.
Para um MVP amplo, precisamos:

- guardar e passar strings entre variáveis, parâmetros, retornos e chamadas;
- decidir e documentar como `char` se relaciona com Unicode (um byte, um
  escalar Unicode ou outra unidade);
- definir conversões numéricas permitidas e rejeitar as perigosas com um erro
  claro;
- especificar valores mínimos/máximos, overflow, divisão por zero, `NaN` e
  infinito para cada número;
- escolher se `float` será binary32 de verdade ou continuará sendo um nome
  compatível com binary64 no MVP;
- fazer o mesmo valor ter o mesmo tipo e resultado em VM, nativo e REPL.

Tipos que não fazem parte do MVP devem falhar com um diagnóstico que diga qual
parte falta, em vez de parecer que o programa está pronto.

### 3. Completar strings, caracteres e I/O

Texto é necessário para quase todo programa. O MVP já consegue armazenar
strings locais, passá-las e devolvê-las por funções, compará-las por conteúdo,
imprimir UTF-8 e escapar caracteres de controle nos backends VM e nativo.
Continuam faltando regras completas e testadas para:

- strings literais, escapes e strings de várias linhas;
- caracteres e texto Unicode;
- tipo `char` executável e suas regras Unicode;
- estado persistente de strings entre entradas do REPL;
- concatenação e operações básicas de texto;
- uma regra estável para `print` (hoje os argumentos são escritos juntos, sem
  separador automático, e depois recebem uma quebra de linha);
- entrada simples do usuário, além da saída;
- mensagens de erro separadas da saída normal do programa.

O comportamento de `print` precisa ser escolhido e documentado: por exemplo,
se argumentos múltiplos ficam juntos ou recebem espaços. VM e nativo devem
seguir a mesma regra.

### 4. Oferecer coleções pequenas e úteis

Sem coleções, programas precisam inventar muitos nomes para guardar coisas
relacionadas. Para este MVP, escolha pelo menos uma coleção simples, como uma
lista de valores do mesmo tipo. Defina antes:

- criação, leitura e atualização de elementos;
- tamanho e comportamento quando um índice não existe;
- como listas crescem e quem pode alterá-las;
- quais tipos podem entrar nelas;
- comportamento igual na VM e no nativo.

Mapas e conjuntos podem vir depois da lista. Não é necessário adicionar todos os
tipos de coleção ao mesmo tempo.

### 5. Terminar o fluxo de controle essencial

`if`, `while`, `break`, `continue` e `return` formam uma base útil. Um MVP
amplo já tem `&&` e `||` com curto-circuito; também deve decidir e cobrir:

- `for` para percorrer uma sequência, depois que houver uma coleção;
- `else if` e corpos simples com regras claras;
- escopo de variáveis dentro e fora de blocos;
- retorno garantido em funções que prometem devolver um valor;
- laços aninhados e `break`/`continue` sempre associados ao laço correto.

#### E `when`, `where`, `whenever` e `with`?

Esses nomes ainda não formam um recurso executável completo. Antes de ensiná-los
como parte do MVP, a linguagem precisa dizer exatamente o que cada um significa:

- `when`: pode ser uma escolha por condição ou por padrão; definir qual problema
  resolve e como se compara com `if`;
- `where`: normalmente descreve uma restrição de tipo, não um comando de fluxo;
  só é útil quando a linguagem tiver genéricos ou padrões que precisem dessas
  restrições;
- `whenever`: o significado e a sintaxe ainda precisam ser decididos;
- `with`: pode significar várias coisas em linguagens diferentes; não deve ser
  implementado antes de escolher um único significado.

Não é necessário incluir essas quatro palavras no MVP. É melhor ter `if` e
`for` bem definidos do que palavras extras com comportamento confuso.

### 6. Completar funções de forma consistente

Funções nomeadas já são parte do núcleo. Para torná-las confortáveis em
programas pequenos, faltam decisões e testes para:

- chamadas com os tipos do MVP, inclusive strings e coleções quando esses
  tipos estiverem prontos;
- argumentos nomeados e posicionais, ou uma decisão explícita de adiar
  argumentos nomeados;
- funções anônimas/closures, incluindo captura de variáveis, ou uma decisão
  explícita de deixá-las para depois;
- quantidade de argumentos e convenções de chamada sem limites escondidos;
- recursão, retornos e mensagens de erro consistentes.

Funções anônimas não são requisito obrigatório de todo MVP. Se forem adiadas,
funções nomeadas precisam continuar suficientes para ensinar e escrever os
programas escolhidos.

### 7. Unificar o significado de `check`, VM, nativo e REPL

O parser e a semântica podem aceitar uma construção que o backend ainda não
sabe executar. Essa diferença precisa ser visível:

- `check` pode aceitar mais código que a rota executável, mas deve distinguir
  claramente “tipado” de “executável”;
- `run`, `compile` e `repl` devem concordar nos valores e efeitos suportados;
- cada recurso deve ter um diagnóstico específico se ainda não for compilável;
- a IR deve validar tipos, registradores, chamadas, saltos e retornos antes da
  execução;
- a VM deve informar falhas de runtime com localização útil e, quando possível,
  uma pilha de chamadas;
- a representação textual de IR não deve ser chamada de bytecode carregável.

Um bytecode próprio não é pré-requisito para um MVP confiável. Pode ser criado
depois, quando houver uma necessidade real de salvar ou carregar programas
compilados.

### 8. Fazer uma suíte de conformidade que prove o comportamento

Testar só o parser não prova que um programa funciona. Para cada recurso do
MVP, a suíte deve incluir:

- casos aceitos e casos rejeitados com diagnóstico esperado;
- testes da mesma entrada com `check`, `run`, `compile` e `repl`, quando
  aplicável;
- comparação da saída e dos erros entre VM e binário nativo;
- testes para limites, overflow, divisão por zero e inicialização;
- programas de exemplo pequenos, que também rodam em CI;
- execução automatizada nos compiladores C++ oficialmente suportados.

Uma mudança só está pronta quando os testes demonstram a regra escolhida, não
apenas quando o projeto compila.

### 9. Tornar a ferramenta fácil de usar

Uma primeira versão usável precisa de:

- comandos e opções documentados, com mensagens de uso corretas;
- códigos de saída previsíveis para sucesso, erro de código e erro de runtime;
- caminhos e mensagens de erro que funcionem fora da raiz do repositório;
- build reproduzível e instruções testadas em uma máquina limpa;
- exemplos que usem somente sintaxe canônica e recursos já implementados;
- REPL com estado previsível, ajuda, reset e erros que não estraguem a sessão.

O suporte oficial pode começar somente em Linux x86-64. É melhor declarar um
alvo pequeno e testado do que prometer plataformas que ninguém valida.

### 10. Documentar limites sem vender planos como recursos

Cada página da linguagem deve separar:

1. sintaxe que o lexer/parser reconhecem;
2. código que passa na análise semântica;
3. código executado pela VM;
4. código gerado pelo backend nativo;
5. recursos planejados, ainda indisponíveis.

Exemplos precisam ser executados nos comandos que a página recomenda. A
gramática de referência e os planos de evolução devem dizer claramente quando
descrevem o futuro, não o compilador atual.

## Ordem recomendada de trabalho

Para evitar construir muitos recursos em cima de fundamentos instáveis:

1. fixar o contrato do MVP e uma matriz de suporte por comando;
2. fechar valores, caracteres, conversões e erros numéricos; completar strings
   no REPL e definir concatenação;
3. garantir paridade VM/nativo e diagnósticos de recursos não suportados;
4. adicionar coleções mínimas e depois `for`;
5. completar funções e decidir explicitamente o escopo de closures;
6. finalizar I/O básico, erros de runtime e a experiência do REPL;
7. criar a suíte de conformidade e rodá-la em CI;
8. validar instalação, exemplos e documentação em ambiente limpo.

## Critérios para dizer “MVP amplo e consolidado”

Podemos usar essa frase quando todos estes pontos forem verdadeiros:

- existe uma lista curta, pública e estável do que faz parte do MVP;
- cada recurso dessa lista funciona de ponta a ponta, não só no parser;
- `check`, `run`, `compile` e `repl` informam claramente o que suportam;
- os programas de conformidade têm o mesmo resultado observável na VM e no
  nativo;
- textos, números, coleções, funções e fluxo de controle cobrem os exemplos de
  uso escolhidos;
- erros comuns apontam onde está o problema e sugerem como consertá-lo;
- build, testes e exemplos passam em CI a partir de um checkout limpo;
- a documentação não promete recurso que o binário publicado não oferece.

Mesmo então, a ArgosLang continuará sendo um subconjunto da linguagem que se
quer construir no futuro. Isso é normal: um MVP bom é pequeno de propósito,
mas completo dentro das promessas que faz.

## O que pode ficar para depois

Não bloqueiam este MVP inicial: classes e herança, interfaces completas,
genéricos, ponteiros raw, FFI, exceções, async/await, otimizações avançadas,
bytecode persistente, cross-compilation e backends ARM64/RISC-V/AVR. Cada um
precisa de uma especificação e testes próprios antes de ser anunciado como
suportado.

## Documentos relacionados

- [Estado executável atual](mvp.md)
- [Plano de evolução da linguagem](language-evolution.md)
- [Guia da linguagem](guide/README.md)
- [Arquitetura](architecture.md)

## Primeira implementação de POO e programação funcional — 26/10/2026

A prioridade desta etapa foi ampliada para atender ao desenvolvimento de POO
antes da consolidação dos demais itens deste roteiro:

- [x] AST e parser para classes, interfaces, `construct`, `new` e lambdas.
- [x] Tipos de função estruturais, funções de ordem superior e retornos de funções.
- [x] Capturas imutáveis, closures aninhadas e ambientes que sobrevivem ao retorno.
- [x] Classes concretas sem bases com campos, identidade, construtores e métodos.
- [x] Argumentos nomeados executáveis, preservando a ordem dos efeitos.
- [x] Visibilidade, inicialização de campos e rejeição de mutação imutável.
- [x] Análise estática de herança, interfaces e sobrescrita.
- [x] IR verificada e execução VM/x86-64 dos subconjuntos descritos no MVP.
- [ ] Lowering de herança, despacho virtual e chamadas por interfaces.
- [x] Propriedades computadas com `getter()` e `setter()`/`setter(nome)`.
- [x] Construtor `construct(...)`, com rejeição da sintaxe antiga `init(...)`.
- [ ] Acessores automáticos, membros estáticos, sobrecarga e genéricos.
- [ ] Referências ligadas a métodos e captura mutável explícita.
- [ ] Ownership, destruição por escopo e persistência de objetos/closures no REPL.

Os itens anteriores sobre coleções, texto, I/O e consolidação continuam abertos.
Esta etapa não declara o roteiro inteiro concluído. A matriz e os limites
executáveis estão em [mvp.md](mvp.md#poo-e-funções-como-valores-26-de-outubro-de-2026).
