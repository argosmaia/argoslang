# Referência geral da ArgosLang

Este documento reúne o contrato do MVP, o roteiro, a referência da versão,
a gramática, os planos técnicos, os testes e as decisões históricas.
Para aprender a linguagem, use o [guia](guide/README.md); para construção e
execução passo a passo, use [teaching.md](teaching.md). A arquitetura permanece
em [architecture.md](architecture.md).

O [MVP](#mvp) e o guia descrevem o suporte executável. A gramática e os planos
incluem recursos futuros. As decisões ADR preservam o histórico; em caso de
divergência, prevalecem a arquitetura atual e o contrato executável do MVP.

## Índice

- [ArgosLang MVP](#mvp)
- [Roteiro para um MVP amplo e confiável](#roadmap)
- [ArgosLang v0.0.1](#referencia)
- [Especificação da Gramática e Arquitetura da ArgosLang](#gramatica)
- [Evolução da linguagem após o MVP](#evolucao)
- [Arquitetura de Geração de Código, Interoperabilidade C e Execução Híbrida da ArgosLang](#codegen)
- [Testar a ArgosLang e o compilador](#testes)
- [Diagnóstico de Erros do Parser e Solução Proposta](#parser)
- [ADR-0001: Use a modular monolith](#adr-0001)
- [ADR-0002: Treat LLVM as a backend adapter](#adr-0002)

<a id="mvp"></a>

<a id="mvp-argoslang-mvp"></a>

## ArgosLang MVP

O MVP é uma linguagem compilada e interpretável para programas inteiros em
x86-64/Linux. A sintaxe canônica dos tipos primitivos é sempre minúscula:
`int`, `bool`, `byte`, `char`, `float`, `double`, `string` e `void`. A declaração de função usa
`function`. As grafias antigas `func` e os nomes de tipos com inicial maiúscula
não são aceitos; tipos e palavras-chave seguem a forma minúscula.

Este documento descreve o que o código atual consegue executar. Para ver o que
falta para tornar essa primeira versão mais ampla e confiável, consulte o
[roteiro de consolidação do MVP](#roadmap).

<a id="mvp-subconjunto-executável"></a>

### Subconjunto executável

O núcleo abaixo é complementado pelo suporte a
[POO e funções como valores](#mvp-poo-e-funções-como-valores-26-de-outubro-de-2026).

`run`, `compile` e o REPL usam o mesmo lowering para IR tipada e validada. A VM
executa essa IR diretamente; o backend x86-64/Linux também a consome. `check`
valida recursos adicionais do frontend que ainda não têm lowering executável:

- funções com retorno `int`, `bool`, `float`, `double`, `string` ou `void`, com
  até seis parâmetros `int`/`bool`/`float`/`double`/`string`;
- variáveis locais `var` (imutáveis por padrão) e `var mut` (reatribuíveis);
- declarações tipadas sem inicializador; uma variável imutável pode receber
  seu primeiro valor depois da declaração, mas não pode ser alterada novamente;
- literais inteiros, booleanos, reais e strings;
- aritmética inteira e real (`/` sempre retorna `double`), comparações,
  negação, igualdade de strings por conteúdo, atribuição, `if`, `while`,
  `break`, `continue` e `return`;
- operadores booleanos `&&` e `||` com avaliação de curto-circuito: o lado
  direito só é executado quando necessário;
- chamadas locais posicionais ou nomeadas de até seis argumentos;
- `print` de valores `int`, `bool`, `double` e `string`, incluindo variáveis,
  parâmetros e retornos de função;
- entrada `function main() -> int` ou `function main() -> void`, sem parâmetros.

Strings podem conter UTF-8, escapes reconhecidos pelo lexer e quebras de linha
em literais de três aspas. A igualdade compara os bytes do texto; `print`
escreve os bytes sem adicionar separadores entre argumentos e termina com uma
quebra de linha. Ainda não há concatenação de strings nem suporte a `char`
executável.

`nil` representa ausência em `T?`. Opcionais, inclusive camadas aninhadas,
executam em arquivos na VM e no nativo: `??`, `?==`, `?=`, `??=`, `!!`, `!!=` e
`?.` de classes concretas. Arrays homogêneos `[T]` têm literais, leitura/escrita
`[]` e indexação `?[` de arrays opcionais, inspirada no Dart: ausência omite a
avaliação do índice; índice inválido em array presente é erro. Arrays têm
tamanho fixado na criação, identidade por referência e escrita por base mutável.

`!!`/`!!=` sobre ausência e índices fora dos limites encerram com status 70,
função, linha e coluna. A análise rejeita unwrap implícito, `void?`, alvos
imutáveis, leituras antes da inicialização e inicialização somente em ramos
que podem ser pulados. Consulte o [contrato de null-safety](guide/11-null-safety-e-opcionais.md).

Tipos como `byte`, `char` e os demais inteiros estreitos ou sem sinal ainda não
são executáveis. `float` e `double` usam IEEE-754 binary64.

<a id="mvp-estado-atual-do-pipeline"></a>

### Estado atual do pipeline

- O parser atual é manual, com precedência de operadores; LR(1) continua sendo
  uma direção de arquitetura, não a implementação presente.
- A IR possui tipos de registrador/slot e um validador compartilhado pelas
  fronteiras da VM e do backend nativo.
- A VM executa a IR diretamente. `BytecodeEmitter` ainda é uma representação
  textual de inspeção, não um formato de bytecode carregável.
- O backend nativo gera assembly x86-64 e usa `cc` para montar e ligar.
- O REPL compila cada entrada pela mesma semântica, IR e VM, mantendo valores
  locais `int`/`bool`/`float`/`double` e seus opcionais, inclusive aninhados, entre entradas. Literais string podem ser
  impressos, mas variáveis string ainda não são preservadas entre entradas.
  Não é um interpretador AST paralelo.
- `/` é divisão real, inclusive com operandos `int`; `%` continua inteiro e
  informa erro de runtime por divisor zero ou `int.min % -1`.

<a id="mvp-comandos"></a>

### Comandos

```bash
# validação estática
argosc check examples/minimal_parameters.argos

# execução pela VM, sem criar um arquivo de saída
argosc run examples/minimal_parameters.argos

# executável nativo x86-64/Linux
argosc compile examples/minimal_parameters.argos -o parameters
./parameters

# REPL com a mesma VM e estado entre entradas
argosc repl
```

As saídas de `print` são iguais em `run` e no binário compilado. Sem `print`, a
VM escreve `Resultado da VM: N`; no binário nativo, o retorno de `main` é o
status do processo e o Unix preserva somente seus oito bits inferiores.

<a id="mvp-poo-e-funções-como-valores-26-de-outubro-de-2026"></a>

### POO e funções como valores (26 de outubro de 2026)

Arquivos também podem executar, pela VM e pelo backend x86-64/Linux:

- classes concretas sem bases, campos tipados e identidade por referência;
- campos `var` inicializados uma vez em `construct`, campos `var mut` e valores
  padrão; sem `construct` explícito, todos os campos exigem inicializador;
- construtores `construct(...)`, criação com `new Tipo(...)`, métodos com
  `this` e acesso implícito a campos;
- propriedades computadas tipadas com `getter()` e `setter()`/`setter(nome)`,
  leitura por acesso a membro e escrita por atribuição simples ou composta;
- visibilidade `public`, `private`, `protected` e `internal` (um módulo atual);
- argumentos nomeados, avaliados na ordem escrita e associados pelos rótulos;
- tipos de função `(int) -> int`, funções como argumentos e retornos, lambdas
  `(x: int) => x + 1` e chamadas indiretas;
- closures com captura por valor de bindings locais imutáveis já inicializados,
  incluindo closures retornadas e aninhadas. Uma referência a objeto capturada
  conserva a identidade do objeto; seus campos mutáveis continuam alteráveis.

Funções comuns aceitam até seis argumentos. Métodos e construtores reservam
uma posição para `this`; chamadas indiretas e lambdas reservam uma para o
ambiente, deixando até cinco argumentos explícitos. Há sobrecarga explícita com `overload`; argumentos padrão de parâmetros
ainda não estão disponíveis. Parâmetros de lambdas exigem tipo explícito;
o retorno é inferido em corpos de expressão e assume `void` em corpos de bloco
sem anotação. Não há captura mutável explícita nesta etapa.

Objetos e ambientes usam uma arena com duração da execução, sem GC. No nativo,
a arena é liberada no encerramento do processo. Ainda não há destruição por
escopo, `deinit` ou verificação de ownership; este modelo não é uma implementação
da memória determinística planejada na gramática.

Herança simples, interfaces, classes abstratas, métodos virtuais, `override`,
`final override`, sobrecarga com `overload` e `super(...)`/`super.membro`
executam na VM e no nativo. Uma classe estende no máximo uma superclasse e pode
implementar várias interfaces. Interfaces são públicas por padrão e contêm
contratos de métodos e propriedades, sem implementações padrão.
`data` cria dados transitivamente imutáveis, finais, com construtor canônico
e igualdade estrutural; `protocol` foi removido. As regras e limites atuais
estão no [capítulo de POO](guide/10-classes-e-interfaces.md).
Tipos (`class`, `interface`, `data`) e funções de topo são públicos por padrão;
métodos de classe/data são privados, e requisitos de interface são públicos
com alteração explícita de acesso permitida. `sealed class ... permited ...`
restringe subclasses diretas; campos de `data` são sealed e a lista autoriza
acesso imutável. `:` herda classes; `by` implementa interfaces.
`default class` restringe acesso ao pacote declarado. Tipos privados/protegidos
só podem ser aninhados. `class A with B, C` contém instâncias exclusivas e delega
métodos públicos, sem criar subtipagem; colisões exigem resolução explícita.
`data` também gera `hashCode()` e `toString()`. O guia descreve construção,
permissões e limitações do modelo de memória.
Referências ligadas a métodos, `static` e genéricos continuam pendentes.

O REPL pode chamar funções de ordem superior em uma entrada, mas não preserva
objetos nem closures em variáveis entre entradas e rejeita essas declarações
explicitamente. Classes não são declarações persistentes do REPL.

Exemplos de conformidade: `minimal_objects.argos`, `minimal_closures.argos` e
`minimal_object_closure.argos` e `minimal_properties.argos`, com testes de `check`, VM, executável nativo e
assembly montado externamente.


A partir de 27/10/2026, `init(...)` é rejeitado: o único nome de construtor é
`construct(...)`, sem compatibilidade com a sintaxe antiga. Propriedades exigem
um getter com corpo; setters exigem `var mut` e usam o parâmetro implícito
`value` ou o nome escrito em `setter(nome)`. Não há acessores automáticos nem
visibilidade independente por acessor. Propriedades sem setter são somente
leitura. Elas não exigem inicialização em `construct`, pois não armazenam estado.


<a id="mvp-null-safety-e-arrays-opcionais-28-de-outubro-de-2026"></a>

### Null-safety e arrays opcionais (28 de outubro de 2026)

A implementação cobre presença/ausência na IR, VM e x86-64, valores em
parâmetros/retornos/campos/capturas, curto-circuito e igualdade de camadas.
`?.` e `?[` tornam o resultado opcional conservando membros/elementos já
opcionais. `T??` declarado explicitamente mantém estados distintos de ausência
externa e presença com ausência interna. Funções opcionais precisam de unwrap
antes da chamada; métodos podem ser chamados por `obj?.metodo(...)`.

O REPL preserva opcionais numéricos/bool e o estado anterior após entradas
com erro. Continua rejeitando persistência de strings, objetos, arrays e
closures. Arrays multidimensionais, redimensionamento, slices/views, `[T; N]`
e `for` permanecem pendentes. Herança e interfaces agora integram o lowering;
`protocol` foi substituído por `data`.

Exemplos de conformidade: `minimal_null_safety.argos`,
`minimal_optional_arrays.argos`, `minimal_nil_unwrap.argos` e
`minimal_array_bounds.argos`. Consulte o [roadmap](#roadmap) para o restante
do MVP e o [capítulo 11](guide/11-null-safety-e-opcionais.md) para as regras.


---

<a id="roadmap"></a>

<a id="roadmap-roteiro-para-um-mvp-amplo-e-confiável"></a>

## Roteiro para um MVP amplo e confiável

Este documento explica o que falta para a ArgosLang passar de um protótipo que
executa um subconjunto para uma primeira versão que pessoas consigam usar em
programas pequenos, sem descobrir limites importantes só depois de escrever o
código.

<a id="roadmap-antes-de-tudo-o-que-quer-dizer-mvp-completo"></a>

### Antes de tudo: o que quer dizer “MVP completo”?

Uma linguagem pode ter um MVP útil sem implementar todas as ideias que talvez
entrem nela no futuro. “Completo” aqui quer dizer que o conjunto escolhido para
esta primeira versão tem começo, meio e fim: dá para escrever programas reais
pequenos, receber erros que ajudem a consertá-los, executá-los de forma
previsível e confiar que as rotas suportadas concordam.

Então o objetivo não é fazer tudo que aparece na gramática de referência.
Recursos como classes, genéricos, ponteiros, exceções, concorrência e backends
para outras CPUs podem esperar. O importante é não anunciar um recurso como
pronto enquanto só uma parte dele funciona.

<a id="roadmap-onde-estamos"></a>

### Onde estamos

O [MVP atual](#mvp) já descreve um caminho executável útil:

- `argosc check`, `run`, `compile` e `repl`;
- IR tipada e validada usada pela VM e pelo backend x86-64/Linux;
- variáveis locais e funções tipadas dentro dos limites descritos em
  [ArgosLang MVP](#mvp);
- contas, decisões e laços do subconjunto executável;
- REPL que usa a mesma VM, em vez de um segundo interpretador da linguagem.

Mas isso ainda não é uma versão ampla e confiável da linguagem. O frontend
reconhece coisas que a VM ou o compilador nativo ainda não executam. Alguns
tipos e construções também têm suporte desigual, e não há ainda uma biblioteca
de uso geral para programas comuns. Consulte sempre a lista exata em
[Subconjunto executável](#mvp-subconjunto-executável): exemplos e planos não
substituem testes do recurso em cada rota.

Desde a primeira versão deste roteiro, strings locais já podem ser guardadas,
passadas e devolvidas por funções, comparadas e impressas; `&&` e `||` também
já têm curto-circuito na VM e no nativo. Strings ainda não persistem entre
entradas do REPL, e vários outros itens abaixo continuam pendentes.

<a id="roadmap-o-que-falta-em-ordem"></a>

### O que falta, em ordem

<a id="roadmap-1-definir-o-tamanho-do-mvp"></a>

#### 1. Definir o tamanho do MVP

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

<a id="roadmap-2-fechar-os-tipos-que-os-programas-realmente-usam"></a>

#### 2. Fechar os tipos que os programas realmente usam

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

<a id="roadmap-3-completar-strings-caracteres-e-io"></a>

#### 3. Completar strings, caracteres e I/O

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

<a id="roadmap-4-oferecer-coleções-pequenas-e-úteis"></a>

#### 4. Oferecer coleções pequenas e úteis

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

<a id="roadmap-5-terminar-o-fluxo-de-controle-essencial"></a>

#### 5. Terminar o fluxo de controle essencial

`if`, `while`, `break`, `continue` e `return` formam uma base útil. Um MVP
amplo já tem `&&` e `||` com curto-circuito; também deve decidir e cobrir:

- `for` para percorrer uma sequência, depois que houver uma coleção;
- `else if` e corpos simples com regras claras;
- escopo de variáveis dentro e fora de blocos;
- retorno garantido em funções que prometem devolver um valor;
- laços aninhados e `break`/`continue` sempre associados ao laço correto.

<a id="roadmap-e-when-where-whenever-e-with"></a>

##### E `when`, `where`, `whenever` e `with`?

Esses nomes ainda não formam um recurso executável completo. Antes de ensiná-los
como parte do MVP, a linguagem precisa dizer exatamente o que cada um significa:

- `when`: pode ser uma escolha por condição ou por padrão; definir qual problema
  resolve e como se compara com `if`;
- `where`: normalmente descreve uma restrição de tipo, não um comando de fluxo;
  só é útil quando a linguagem tiver genéricos ou padrões que precisem dessas
  restrições;
- `whenever`: o significado e a sintaxe ainda precisam ser decididos;
- `with`: agora implementa composição exclusiva e delegação múltipla em classes;
  veja o [guia de POO](guide/10-classes-e-interfaces.md).

As três primeiras palavras continuam planejadas; `with` já executa no MVP. É melhor ter `if` e
`for` bem definidos do que palavras extras com comportamento confuso.

<a id="roadmap-6-completar-funções-de-forma-consistente"></a>

#### 6. Completar funções de forma consistente

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

<a id="roadmap-7-unificar-o-significado-de-check-vm-nativo-e-repl"></a>

#### 7. Unificar o significado de `check`, VM, nativo e REPL

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

<a id="roadmap-8-fazer-uma-suíte-de-conformidade-que-prove-o-comportamento"></a>

#### 8. Fazer uma suíte de conformidade que prove o comportamento

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

<a id="roadmap-9-tornar-a-ferramenta-fácil-de-usar"></a>

#### 9. Tornar a ferramenta fácil de usar

Uma primeira versão usável precisa de:

- comandos e opções documentados, com mensagens de uso corretas;
- códigos de saída previsíveis para sucesso, erro de código e erro de runtime;
- caminhos e mensagens de erro que funcionem fora da raiz do repositório;
- build reproduzível e instruções testadas em uma máquina limpa;
- exemplos que usem somente sintaxe canônica e recursos já implementados;
- REPL com estado previsível, ajuda, reset e erros que não estraguem a sessão.

O suporte oficial pode começar somente em Linux x86-64. É melhor declarar um
alvo pequeno e testado do que prometer plataformas que ninguém valida.

<a id="roadmap-10-documentar-limites-sem-vender-planos-como-recursos"></a>

#### 10. Documentar limites sem vender planos como recursos

Cada página da linguagem deve separar:

1. sintaxe que o lexer/parser reconhecem;
2. código que passa na análise semântica;
3. código executado pela VM;
4. código gerado pelo backend nativo;
5. recursos planejados, ainda indisponíveis.

Exemplos precisam ser executados nos comandos que a página recomenda. A
gramática de referência e os planos de evolução devem dizer claramente quando
descrevem o futuro, não o compilador atual.

<a id="roadmap-ordem-recomendada-de-trabalho"></a>

### Ordem recomendada de trabalho

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

<a id="roadmap-critérios-para-dizer-mvp-amplo-e-consolidado"></a>

### Critérios para dizer “MVP amplo e consolidado”

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

<a id="roadmap-o-que-pode-ficar-para-depois"></a>

### O que pode ficar para depois

Não bloqueiam este MVP inicial: classes e herança, interfaces completas,
genéricos, ponteiros raw, FFI, exceções, async/await, otimizações avançadas,
bytecode persistente, cross-compilation e backends ARM64/RISC-V/AVR. Cada um
precisa de uma especificação e testes próprios antes de ser anunciado como
suportado.

<a id="roadmap-documentos-relacionados"></a>

### Documentos relacionados

- [Estado executável atual](#mvp)
- [Plano de evolução da linguagem](#evolucao)
- [Guia da linguagem](guide/README.md)
- [Arquitetura](architecture.md)

<a id="roadmap-primeira-implementação-de-poo-e-programação-funcional-26102026"></a>

### Primeira implementação de POO e programação funcional — 26/10/2026

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
- [x] Lowering de herança, despacho virtual e chamadas por interfaces — 29/10/2026.
- [x] `overload`, `override`, `super` e `data` transitivamente imutável — 29/10/2026.
- [x] Propriedades computadas com `getter()` e `setter()`/`setter(nome)`.
- [x] Construtor `construct(...)`, com rejeição da sintaxe antiga `init(...)`.
- [ ] Acessores automáticos, membros estáticos e genéricos.
- [ ] Referências ligadas a métodos e captura mutável explícita.
- [ ] Ownership, destruição por escopo e persistência de objetos/closures no REPL.

Os itens anteriores sobre coleções, texto, I/O e consolidação continuam abertos.
Esta etapa não declara o roteiro inteiro concluído. A matriz e os limites
executáveis estão em [ArgosLang MVP](#mvp-poo-e-funções-como-valores-26-de-outubro-de-2026).

<a id="roadmap-null-safety-e-arrays-opcionais-implementação-de-28102026"></a>

### Null-safety e arrays opcionais — implementação de 28/10/2026

- [x] `T?`, `nil` e camadas aninhadas com presença explícita, sem sentinelas
      que confundam zero, false ou string vazia com ausência.
- [x] Parser/AST/semântica de `!!=`, inclusive tokenização como operador único.
- [x] `??`, `?=`, `?==`, `??=`, `!!`, `!!=` executáveis na VM e no x86-64/Linux.
- [x] `?.` de campos, propriedades e chamadas de métodos de classes concretas,
      com receptor único e omissão de argumentos/getters quando ausente.
- [x] Arrays homogêneos `[T]`, literais, leitura/escrita `[]` e `?[` inspirado
      no Dart para arrays opcionais; ausência omite o índice.
- [x] Erro localizado de unwrap ausente e índice inválido, com status 70.
- [x] Tipos na IR, wrapping/unwrapping validados, ABI de valores boxed e
      preservação em parâmetros, retornos, campos e capturas.
- [x] Rejeição de unwrap implícito, `void?`, uso antes da inicialização,
      escrita em alvos imutáveis e inicialização garantida por ramo pulável.
- [x] Persistência de opcionais numéricos/bool no REPL, com camadas aninhadas
      e recuperação após erro de runtime.
- [x] Conformidade de saída e efeitos entre VM, binário nativo e assembly.
- [ ] Persistência de opcionais de strings, objetos, arrays e closures no REPL.
- [ ] Arrays multidimensionais, crescimento, slices/views, `[T; N]` e `for`.
- [x] Integração executável com herança, interfaces e tipos-base — 29/10/2026.

O [capítulo de opcionais](guide/11-null-safety-e-opcionais.md) define tipos,
curto-circuito, ordem dos efeitos, aninhamento, propriedades e limitações.
Navegação/indexação segura conserva a opcionalidade de membros/elementos que
já são opcionais; `T??` explicitamente declarado continua tendo duas camadas.

<a id="roadmap-o-que-continua-faltando-no-mvp"></a>

#### O que continua faltando no MVP

Opcionais não concluem o roteiro inteiro. Continuam pendentes texto persistente
no REPL, operações de strings, `char`/Unicode, fechamento das regras numéricas,
coleções além dos arrays mínimos, `for`, entrada básica,
ownership/destruição, persistência de objetos/closures e consolidação de
instalação, diagnósticos e CI. As seções anteriores sobre ausência de coleções
devem ser lidas agora como ausência de uma biblioteca ampla: arrays mínimos
foram implementados nesta etapa.


---

<a id="referencia"></a>

<a id="referencia-argoslang-v001"></a>

## ArgosLang v0.0.1

> Para estudar a linguagem por capítulos, comece no
> [Guia da Linguagem ArgosLang](guide/README.md). Esta seção reúne a
> referência compacta da versão e o mapa de implementação.

ArgosLang v0.0.1 é uma versão inicial de uma linguagem estaticamente tipada,
com compilação nativa x86-64/Linux e execução por VM para o subconjunto
executável. Esta referência descreve o estado implementado, não a linguagem
planejada em sua totalidade.

<a id="referencia-instalação-e-build"></a>

### Instalação e build

No Ubuntu/Pop!_OS, instale CMake, Ninja, um compilador C++23 e ICU:

```bash
sudo apt update
sudo apt install -y build-essential cmake ninja-build g++ libicu-dev libreadline-dev
```

Na raiz do repositório:

```bash
cmake -S . -B build -G Ninja \
  -DARGOS_BUILD_TESTS=OFF \
  -DCMAKE_CXX_COMPILER=g++
cmake --build build
```

O compilador fica em `./build/bin/argosc`.

<a id="referencia-usar-o-compilador-no-terminal"></a>

### Usar o compilador no terminal

Para usar `argosc` sem escrever o caminho completo, adicione a pasta do build
ao `PATH` da sessão atual:

```bash
export PATH="$PATH:$HOME/Projetos/argoslang/build/bin"
```

Para tornar a alteração permanente no Bash:

```bash
echo 'export PATH="$PATH:$HOME/Projetos/argoslang/build/bin"' >> ~/.bashrc
source ~/.bashrc
argosc --help
```

Se o repositório estiver em outro local, substitua o caminho corretamente.
Você também pode continuar usando `./build/bin/argosc` na raiz do projeto.

<a id="referencia-primeiro-programa"></a>

### Primeiro programa

Crie `hello.argos`:

```argos
function main() -> void {
    print("Hello, world!")
}
```

`print(...)` escreve os argumentos em sequência e termina a linha. Ele é a
operação de saída básica da linguagem: combina argumentos como `printf`, mas
adiciona a quebra de linha como `println`.

O próximo exemplo mostra variáveis, cálculo e saída:

```argos
function main() -> void {
    var a: int = 8
    var b: int = 9
    var mut c: int

    c = a + b
    print("O resultado é: ", c)
}
```

Quando `main` tem retorno `int`, use `return` para entregar um valor à função:

```argos
function sum(_ left: int, _ right: int) -> int {
    return left + right
}
```

<a id="referencia-rodar-um-programa"></a>

### Rodar um programa

```bash
argosc run hello.argos
```

<a id="referencia-compilar-um-programa-nativo"></a>

### Compilar um programa nativo

```bash
argosc compile hello.argos -o hello
./hello
echo $?
```

Programas com `main() -> void` terminam com status `0`; programas que retornam
`int` entregam esse valor como status de processo. A compilação nativa atual é
destinada a x86-64/Linux.

<a id="referencia-interpretar-sem-arquivo-repl"></a>

### Interpretar sem arquivo: REPL

```bash
argosc repl
```

```text
>>> 6 * 7
42
>>> print("Olá, Argos!")
Olá, Argos!
>>> :quit
```

O REPL executa expressões e instruções comuns, como `var`, atribuições, `if`,
`while` e `print(...)`, sem que seja necessário escrever `main`. Cada entrada
passa por lexer, parser, AST e análise semântica antes da interpretação.
Variáveis tipadas e inicializadas, bem como declarações de função, ficam
disponíveis para as entradas seguintes na mesma sessão. `:reset` limpa a
sessão inteira.

Para uma build com testes, habilite `-DARGOS_BUILD_TESTS=ON`. Na primeira vez,
o CMake pode precisar obter Catch2. Se o acesso à rede não estiver disponível,
use a configuração sem testes acima.

<a id="referencia-comandos-de-uso-diário"></a>

### Comandos de uso diário

```bash
# Valida um programa sem executá-lo
./build/bin/argosc check examples/control_flow.argos

# Executa na VM, sem criar binário nativo
./build/bin/argosc run examples/control_flow.argos

# Compila para x86-64/Linux
./build/bin/argosc compile examples/control_flow.argos -o control-flow
./control-flow

# REPL
./build/bin/argosc repl
```

`run` imprime o valor retornado por `main` (em uma função `void`, o valor é
`0`). O binário nativo devolve o retorno `int` como status do processo;
sistemas Unix preservam apenas `0..255`.

<a id="referencia-códigos-de-saída-do-cli"></a>

#### Códigos de saída do CLI

| Código | Situação |
| --- | --- |
| `0` | comando concluído sem diagnóstico de erro |
| `64` | uso inválido do CLI |
| `65` | erro léxico, sintático, semântico ou de lowering |
| `66` | arquivo-fonte não pôde ser aberto |
| `70` | erro durante execução da VM |

<a id="referencia-estrutura-de-programa"></a>

### Estrutura de programa

Uma entrada executável precisa de `function main()` sem parâmetros e pode
retornar `void` ou `int`. Use `void` quando o programa só produz saída; use
`int` quando precisar devolver um código calculado:

```argos
function main() -> int {
    return 42
}
```

`function` é a única grafia aceita para declarar funções. `func` não é uma
palavra-chave.

Uma unidade de fonte contém declarações de função, declarações de variável e
instruções permitidas pelo frontend. Para gerar um executável, `main` deve
existir uma única vez e não receber parâmetros.

<a id="referencia-pontuação-blocos-e-terminadores"></a>

### Pontuação, blocos e terminadores

Blocos usam chaves. O lexer pode inserir terminadores em quebras de linha
elegíveis, por isso estes dois estilos são aceitos pelo subconjunto atual:

```argos
var value: int = 1
value = value + 1
```

```argos
var value: int = 1;
value = value + 1;
```

Quando houver dúvida durante a evolução da gramática, prefira `;`, sobretudo
após uma declaração sem inicializador. Ele torna a intenção inequívoca e reduz
diagnósticos em cascata.

<a id="referencia-tipos-e-valores"></a>

### Tipos e valores

Os nomes canônicos são minúsculos: `int`, `bool`, `byte`, `char`, `string`,
`void`, além de `short`, `long`, `uint`, `ulong`, `float`, `double`, `usize` e
`isize`.

O subconjunto executável usa `int`, `bool`, `float`, `double`, `string`,
classes concretas, funções como valores e seus opcionais. Literais básicos:

```argos
var answer: int = 42
var enabled: bool = true
var disabled: bool = false
```

O tipo opcional usa `?`; `nil` representa ausência explícita. Um tipo
não-opcional **nunca** pode conter `nil`: a tentativa é erro semântico
`ARGOS-S0101` em declarações ou `ARGOS-S0331` em reatribuições, detectado em `check`.

```argos
var nome: string? = nil          // válido: string? admite nil
var outro: string = nil          // ARGOS-S0101: tipo incompatível
```

`null` ainda é aceito como apelido temporário de `nil` e será removido em
versão futura. Prefira sempre `nil`.

<a id="referencia-opcionais-e-arrays-opcionais-implementação-de-28102026"></a>

#### Opcionais e arrays opcionais — implementação de 28/10/2026

Todos os operadores abaixo têm parser, semântica, IR, VM e backend x86-64/Linux.
O tipo-base precisa pertencer ao subconjunto executável.

| Operador | Regra | Resultado |
| --- | --- | --- |
| `??` | `T?` à esquerda, fallback compatível com `T`, avaliado apenas na ausência | `T` |
| `?.` | Base com uma camada opcional; omite membro/getter/chamada e argumentos na ausência | Tipo do membro/retorno tornado opcional, conservando opcionalidade existente |
| `?[` | Array `[T]?`; ausência omite índice; presença usa indexação normal | Elemento tornado opcional, conservando opcionalidade existente |
| `?=` | Fonte opcional; escreve uma camada desembrulhada somente na presença | Valor final do alvo |
| `?==` | Ao menos um opcional/nil; tipos-base compatíveis; aceita `T?`/`T` | `bool` |
| `??=` | Alvo opcional; avalia e escreve fonte somente quando alvo ausente | Valor final do alvo |
| `!!` | Remove uma camada; ausência falha com status 70 | Tipo desembrulhado |
| `!!=` | Fonte opcional; unwrap antes da escrita; ausência falha com status 70 | Valor final do alvo |

`null_type` é interno; inferência apenas de `nil` é rejeitada. `void?` e
`never?` são inválidos. `T` nunca recebe `nil` e `T?` nunca vira `T`
implicitamente. Não há conversão de opcional para `bool`, aritmética opcional,
smart cast, ternário ou propagação por `expressão?`. `!` é negação, `!!` é
pós-fixado. Opcionais têm até 4095 camadas.

`T??` mantém ausência externa distinta de presença com ausência interna;
`!!` e `??` removem uma camada. Cada conversão implícita envolve no máximo
uma camada. `?.`/`?[` conservam a opcionalidade de resultados já opcionais
para permitir navegação encadeada; bases com camadas adicionais exigem unwrap.
Chamadas seguras de métodos `void` são omitidas na ausência e não criam `void?`.

Arrays homogêneos usam `[T]`, literais `[1, 2]`, leitura/escrita `[]` e
indexação segura `?[` inspirada no Dart. `[T]?` torna o array opcional;
`[T?]` torna os elementos opcionais. Tipos de array são invariantes. Arrays têm
tamanho fixo e identidade por referência; escrita exige base mutável.
Arrays vazios ou somente com `nil` exigem contexto tipado. Índice exige `int`
não opcional, começa em zero e falha se negativo ou fora dos limites, mesmo
com `?[` quando a base estiver presente. Não há indexação de strings.

As atribuições opcionais exigem alvo mutável já inicializado, resolvido uma
vez antes da fonte. `??=` aceita fonte do tipo do alvo ou uma camada a menos.
Em propriedades, `??=` lê getter uma vez e só chama setter quando ausente;
`?=` lê getter na ausência para produzir o valor final e chama setter na
presença; `!!=` só chama setter após unwrap. Escritas por `?.`/`?[` são
rejeitadas. Falhas de unwrap e limites informam função, linha e coluna.

Igualdade compara camadas recursivamente, strings por conteúdo e
objetos/arrays por identidade. Funções não têm igualdade de valores, mas
funções opcionais podem ser comparadas com `nil`. `==`/`!=` exigem profundidades
iguais, além das comparações com `nil`; `?==` também admite uma camada de
diferença, envolvendo o operando menos opcional.

A precedência segue pós-fixados, prefixos, aritmética, relações, igualdade,
`&&`, `||`, `??` e atribuições. Coalescência e atribuições associam à direita.
Escritas em ramos puláveis não estabelecem inicialização definida.
O REPL executa os operadores e preserva opcionais numéricos/bool, inclusive
aninhados. Persistência de strings, objetos, arrays e closures continua
indisponível, assim como arrays multidimensionais, crescimento, slices/views,
`[T; N]` e `for`.

A representação usa boxes na arena e zero somente para a ausência externa,
sem reservar valores do payload. A IR valida tipos de wrapping, unwrapping,
arrays, chamadas e retornos. O [capítulo 11](guide/11-null-safety-e-opcionais.md)
contém exemplos e a matriz completa de suporte.

<a id="referencia-conversões-e-limites-numéricos"></a>

#### Conversões e limites numéricos

No subconjunto executável, o valor de `int` é tratado como inteiro de 32 bits
com sinal na VM e no backend x86-64. Divisão por zero gera erro de runtime na
VM; overflow e a política de promoção numérica ainda não são contrato estável.
Não use o status de processo para testar retornos maiores que 255.

<a id="referencia-variáveis-e-mutabilidade"></a>

### Variáveis e mutabilidade

`var` declara uma variável **imutável por padrão**. Use `var mut` para uma
variável reatribuível:

```argos
function main() -> int {
    var base: int = 10
    var mut total: int = base
    total = total + 5
    return total
}
```

`let` não faz parte da sintaxe nova. Uma leitura antes da inicialização é erro
semântico; o compilador aponta o identificador e pode apresentar uma sugestão
de inicialização.

Não há inferência de mutabilidade: uma variável declarada com `var` nunca se
torna reatribuível por ter recebido uma atribuição posterior. Declare `var mut`
na origem quando esse for o propósito.

<a id="referencia-funções"></a>

### Funções

Funções têm tipo de retorno após `->`. No subconjunto executável, elas podem
receber até seis parâmetros dos tipos executáveis, incluindo opcionais e arrays:

```argos
function multiply(_ left: int, _ right: int) -> int {
    return left * right
}

function main() -> int {
    return multiply(6, 7)
}
```

O `_` permite chamada posicional. Argumentos nomeados também executam,
preservando a ordem dos efeitos.

Chamadas são resolvidas semanticamente por nome, quantidade de argumentos,
rótulos e tipo. A ABI nativa atual passa até seis argumentos inteiros nos
registradores convencionais do alvo x86-64/Linux; reais usam registradores SSE.
Opcionais, arrays, objetos e closures usam palavras de referência. Métodos,
construtores e closures reservam uma posição implícita, deixando cinco
argumentos explícitos.

<a id="referencia-expressões"></a>

### Expressões

Estão disponíveis no subconjunto executável:

```argos
var result: int = 6 * 7 + 1
var comparison: bool = result >= 42
var negative: int = -result
```

Operadores incluem aritmética (`+`, `-`, `*`, `/`, `%`), comparação (`==`,
`!=`, `<`, `<=`, `>`, `>=`), negação (`!`) e atribuição (`=`).

As precedências seguem o comportamento usual: chamadas e postfixados têm
prioridade alta; multiplicação/divisão vêm antes de soma/subtração; comparações
vêm depois; atribuição associa à direita. Use parênteses quando a intenção não
for óbvia:

```argos
var result: int = (base + offset) * scale
```

<a id="referencia-fluxo-de-controle"></a>

### Fluxo de controle

<a id="referencia-if-else"></a>

#### if / else

```argos
if enabled {
    return 7
} else {
    return 0
}
```

Corpos curtos também são aceitos:

```argos
if enabled return 7 else return 0
```

<a id="referencia-while-break-e-continue"></a>

#### while, break e continue

```argos
var mut index: int = 0
var mut total: int = 0

while index < 5 {
    index = index + 1
    if index == 3 continue
    total = total + index
    if total > 10 break
}
```

`break` sai do laço; `continue` volta para a condição do `while`. Ambos são
erros semânticos fora de um laço.

`for`, `switch`, `when` e `whenever` **não** fazem parte da v0.0.1 executável.
Use `while` e condicionais até que esses recursos atravessem as mesmas fases.

<a id="referencia-diagnósticos"></a>

### Diagnósticos

Cada erro possui categoria e código:

- `ARGOS-Lxxxx`: léxico;
- `ARGOS-Pxxxx`: parsing;
- `ARGOS-Sxxxx`: semântica;
- `ARGOS-Cxxxx`: lowering/compilação.

Os diagnósticos carregam intervalo de origem. O apresentador de diagnósticos
mostra a linha, a coluna e, quando disponível, uma seção `ajuda` com uma
correção sugerida. Veja `examples/error_uninitialized.argos` e
`examples/error_immutable.argos`.

Um diagnóstico bem formado deve conter uma causa primária e evitar mensagens
derivadas da mesma falha. Por exemplo, uma variável não inicializada é apontada
no uso, não na função inteira:

```text
arquivo.argos:4:12: error[ARGOS-S0301]: variável "answer" usada antes da inicialização
  return answer
         ^
  ajuda: inicialize-a antes do uso: var answer: int = 0
```

Para erros de parser, corrija primeiro o diagnóstico de menor linha/coluna;
mensagens seguintes podem ser consequência da estrutura incompleta.

<a id="referencia-exemplos"></a>

### Exemplos

| Arquivo | Uso |
| --- | --- |
| `hello_return.argos` | programa mínimo que retorna 42 |
| `variables.argos` | `var` e `var mut` |
| `functions_parameters.argos` | parâmetros e chamadas |
| `control_flow.argos` | `if`, `while`, `break`, `continue` |
| `minimal_break.argos` | saída antecipada de laço |
| `minimal_null_safety.argos` | operadores opcionais, propriedades e closures — VM e nativo |
| `minimal_optional_arrays.argos` | arrays opcionais e chamadas seguras — VM e nativo |
| `minimal_data.argos` | dados imutáveis e igualdade estrutural — VM e nativo |
| `minimal_polymorphism.argos` | herança, interfaces, overload/override e super — VM e nativo |
| `error_uninitialized.argos` | erro intencional |
| `error_immutable.argos` | erro intencional |

<a id="referencia-limites-desta-versão"></a>

### Limites desta versão

Herança simples, polimorfismo por interfaces, sobrecarga, sobrescrita e `data`
executam na VM e no nativo. Enums, interfaces com implementação padrão, `for`,
`switch`, `when`, genéricos, módulos, exceções, async/await e concorrência
continuam pendentes.

**Opcionais e null-safety** executam na VM e no nativo, incluindo `?[` de
arrays opcionais inspirado no Dart. O REPL preserva opcionais numéricos/bool;
veja os limites no [capítulo 11](guide/11-null-safety-e-opcionais.md).

`byte`, `char` e os demais tipos inteiros estreitos ou sem sinal ainda não são
executáveis. `float` e `double` usam representação binary64.

Também não há biblioteca padrão estável, concatenação de strings, mapas ou conjuntos,
interoperabilidade C exposta ao usuário, geração de objeto própria, cross
compilation ou suporte nativo para outros alvos. Consulte
a seção [Evolução da linguagem](#evolucao) para decisões de desenho e ordem de evolução.


<a id="referencia-avançado-inspeção-do-compilador"></a>

### Avançado: inspeção do compilador

Estes comandos são úteis para estudar a implementação, depurar gramática ou
reportar um erro; não são necessários para escrever e executar programas:

```bash
# sequência de tokens, lexemas e posições
argosc lex examples/control_flow.argos

# confirma que o parser construiu um programa
argosc parse examples/control_flow.argos

# mostra a árvore sintática abstrata
argosc ast examples/control_flow.argos
```

<a id="referencia-avançado-estado-interno-e-arquitetura"></a>

### Avançado: estado interno e arquitetura

Esta é uma versão de bootstrap: o frontend reconhece mais construções do que a
IR executável consegue baixar. Recursos podem ser executáveis, apenas de
frontend ou planejados. O compilador aceita somente `function` e nomes de tipos
primitivos em minúsculas. `null` ainda é aceito como apelido temporário de
`nil`.

```text
arquivo .argos
  -> SourceManager
  -> Lexer
  -> Parser descendente recursivo + Pratt parser de expressões
  -> AST
  -> SemanticAnalyzer
  -> IR executável
  -> VM (`run`) ou x86-64/Linux (`compile`)
```

O parser não é LR/LALR/SLR gerado. Declarações e instruções usam parser manual;
expressões usam Pratt parsing para precedência e associatividade. O frontend
guarda intervalos de origem para alimentar os diagnósticos.


---

<a id="gramatica"></a>

<a id="gramatica-especificação-da-gramática-e-arquitetura-da-argoslang"></a>

## Especificação da Gramática e Arquitetura da ArgosLang

> Especificação de evolução — 14 de setembro de 2026
> Parser-alvo: LR canônico com um símbolo de lookahead, LR(1).

> **Estado do compilador (22 de outubro de 2026):** o parser implementado é
> manual, com precedência de operadores; não é LR(1). Esta especificação
> descreve o desenho futuro e não deve ser usada como lista de recursos
> executáveis. Para o subconjunto que funciona hoje, consulte [ArgosLang MVP](#mvp).
> A grafia canônica é `function`, `var`/`var mut`, `nil` e tipos primitivos em
> minúsculas (`int`, `bool`, `string`). As formas `func` e tipos com inicial
> maiúscula não fazem parte da sintaxe aceita.

<a id="gramatica-1-escopo"></a>

### 1. Escopo

Este documento define a sintaxe, a gramática formal e as regras semânticas centrais da ArgosLang. As palavras **deve**, **não deve**, **pode** e **requer** possuem sentido normativo. Uma regra documentada não constitui prova de implementação.

Termos:

- **CST:** árvore concreta, adequada a ferramentas e preservação de tokens;
- **AST:** árvore abstrata independente do algoritmo de parsing;
- **HIR:** representação de alto nível resolvida e tipada;
- **IR Argos:** representação intermediária própria;
- **Machine IR:** representação específica de uma arquitetura;
- **executor:** fronteira para código C, C++, Rust, Swift ou Assembly.

<a id="gramatica-2-filosofia"></a>

### 2. Filosofia

A ArgosLang é estaticamente e fortemente tipada, multiparadigma e orientada a objetos, com recursos funcionais, null-safety, imutabilidade por padrão e memória determinística sem garbage collector obrigatório.

Ela possui:

1. sintaxe moderna inspirada em Swift;
2. classes como tipos com identidade e herança;
3. interfaces como contratos comportamentais;
4. `data` como agregados imutáveis inspirados nos records do Java e data classes do Kotlin;
5. generics e constraints;
6. perfis hosted, embedded, freestanding e bare metal;
7. compilação nativa direta, sem C, C++ ou LLVM intermediários;
8. bytecode próprio para execução em VM;
9. interoperabilidade com C, C++, Rust, Swift e Assembly.

<a id="gramatica-3-pipeline"></a>

### 3. Pipeline

```text
Fonte → Lexer → tokens → Parser LR(1) → CST/AST → HIR tipada → IR Argos
                                                             ├─→ Bytecode → VM
                                                             └─→ Machine IR → máquina
```

C, C++, Rust e Swift não são destinos da ArgosLang. Seus executores são portas anexas de interoperabilidade.

<a id="gramatica-4-léxico"></a>

### 4. Léxico

Arquivos usam UTF-8. Identificadores aceitam letras Unicode, `_` e dígitos depois do primeiro caractere.

```ebnf
Identifier ::= IdentifierStart IdentifierContinue* ;
IdentifierStart ::= "_" | UnicodeLetter ;
IdentifierContinue ::= IdentifierStart | DecimalDigit ;

IntegerLiteral ::= DecimalInteger | HexInteger | BinaryInteger | OctalInteger ;
FloatLiteral ::= DecimalFloat FloatSuffix? ;
CharLiteral ::= "'" CharContent "'" ;
StringLiteral ::= '"' StringContent* '"' ;
MultilineStringLiteral ::= '"""' MultilineContent* '"""' ;
BooleanLiteral ::= "true" | "false" ;
NullLiteral ::= "nil" ;
```

Comentários de linha começam com `//`; comentários de bloco usam `/* ... */` e podem aninhar. Comentários de documentação usam `///` ou `/** ... */`.

Separadores `_` podem aparecer entre dígitos, nunca nas extremidades ou duplicados.

<a id="gramatica-41-terminadores"></a>

#### 4.1. Terminadores

O lexer produz o token `SEMI` para `;` escrito ou para newline elegível. Newline não gera `SEMI` dentro de `()`, `[]`, listas incompletas ou após operador que exige operando. O terminador também pode ser omitido imediatamente antes de `}` ou `EOF` (e antes de `else` após um corpo de uma instrução). Instruções adjacentes na mesma linha ainda exigem separação.

Isso substitui o predicado `&"}"` da versão antiga, pois ele é sintaxe de PEG, não EBNF/LR.

<a id="gramatica-5-contrato-lr1"></a>

### 5. Contrato LR(1)

O parser é LR(1) canônico. Estados com contextos diferentes não devem ser fundidos como em LALR. Recursão à esquerda é permitida e preferida para operadores e listas associativas à esquerda.

O gerador deve terminar com zero conflitos shift/reduce e reduce/reduce. Precedência só pode resolver um conflito quando a regra for normativa. Recuperação deve sincronizar em `SEMI`, `}`, início de declaração e palavras-chave de fluxo.

A CST pode preservar pontuação e comentários. A AST representa significado estrutural. Não existe “AST LR”: LR caracteriza o parser, não a árvore.

<a id="gramatica-6-unidade-de-compilação"></a>

### 6. Unidade de compilação

```ebnf
CompilationUnit ::= ModuleDecl? ImportDecl* TopLevelDecl* EOF ;
ModuleDecl ::= AttributeList? Visibility? "module" QualifiedName SEMI ;
ImportDecl ::= AttributeList? Visibility? "import" QualifiedName ImportTail? SEMI ;
ImportTail ::= "." "*" | "." "{" ImportItemList "}" | "as" Identifier ;
ImportItemList ::= ImportItem ("," ImportItem)* ","? ;
ImportItem ::= Identifier ("as" Identifier)? ;
QualifiedName ::= Identifier ("." Identifier)* ;

TopLevelDecl ::= ClassDecl | DataDecl | InterfaceDecl | EnumDecl
               | TypeAliasDecl | FunctionDecl | VariableDecl | ForeignDecl ;
TypeAliasDecl ::= AttributeList? Visibility? "type" Identifier
                  GenericParameterClause? "=" Type SEMI ;
```

Instruções no topo são exclusivas de scripts. Módulos compilados contêm declarações e uma entrada válida.

<a id="gramatica-7-atributos-acesso-e-modificadores"></a>

### 7. Atributos, acesso e modificadores

```ebnf
AttributeList ::= Attribute+ ;
Attribute ::= "@" QualifiedName ("(" ArgumentList? ")")? ;
Visibility ::= "public" | "internal" | "protected" | "private" | "default" ;
ClassModifier ::= "abstract" | "sealed" | "final" | "unsafe" ;
MemberModifier ::= "abstract" | "virtual" | "final" | "override" | "overload" | "static"
                 | "mut" | "unsafe" | "async" | "native" ;
```

- `public`: acessível por módulos importadores;
- `internal`: acessível no módulo; tipos e funções de topo são `public` por padrão;
- métodos de classe/data são `private` por padrão;
- `default`: mesmo pacote declarado;
- `protected`: classe e subclasses; classes protegidas somente aninhadas;
- `private`: escopo nominal que declarou o membro;
- `sealed class`: subclasses diretas somente quando listadas em `permited`;
- `final class`: não pode ser estendida;
- `final` em membro: não pode ser sobrescrito;
- `abstract`: exige implementação concreta;
- `override`: obrigatório ao sobrescrever uma implementação herdada de classe;
- `overload`: obrigatório em cada assinatura adicional; retorno e rótulos não diferenciam sobrecargas;
- interfaces e seus membros são públicos por padrão; modificadores usam palavras-chave, sem `@`;
- `sealed final` é redundante e inválido.

<a id="gramatica-8-classes"></a>

### 8. Classes

`class` é o único tipo nominal com identidade, estado e herança de implementação. Não existe `struct`; ArgosLang não replica a duplicidade quase equivalente de `class` e `struct` do C++.

```ebnf
ClassDecl ::= AttributeList? Visibility? ClassModifier*
              "class" Identifier GenericParameterClause?
              ClassInheritanceClause? InterfaceImplementationClause? CompositionClause? PermitedClause? WhereClause? ClassBody ;
ClassInheritanceClause ::= ":" NamedType ;
CompositionClause ::= "with" NamedType ("," NamedType)* ;
InterfaceImplementationClause ::= "by" NamedType ("," NamedType)* ;
PermitedClause ::= "permited" Identifier ("," Identifier)* ;
ClassBody ::= "{" ClassMember* "}" ;

ClassMember ::= FieldDecl | PropertyDecl | ConstructorDecl
              | DeinitializerDecl | MethodDecl | OperatorDecl
              | NestedTypeDecl | ForeignDecl | EmptyDecl ;

FieldDecl ::= AttributeList? Visibility? MemberModifier*
              "var" ("mut")? Identifier ":" Type
              ("=" Expression)? SEMI ;

PropertyDecl ::= AttributeList? Visibility? MemberModifier*
                 "var" ("mut")? Identifier ":" Type PropertyBody ;
PropertyBody ::= "{" PropertyAccessor+ "}" ;
PropertyAccessor ::= "getter" "(" ")" (SEMI | FunctionBody)
                   | "setter" "(" Identifier? ")" (SEMI | FunctionBody) ;

ConstructorDecl ::= AttributeList? Visibility? MemberModifier*
                    "construct" GenericParameterClause?
                    "(" ParameterList? ")" ThrowsClause?
                    WhereClause? FunctionBody ;
DeinitializerDecl ::= AttributeList? "deinit" "(" ")" FunctionBody ;

MethodDecl ::= AttributeList? Visibility? MemberModifier*
               FunctionSignature (FunctionBody | SEMI) ;

ComputedPropertyDecl ::= AttributeList? Visibility? MemberModifier*
                         "var" ("mut")? Identifier ":" Type PropertyBody ;
NestedTypeDecl ::= ClassDecl | DataDecl | InterfaceDecl | EnumDecl
                 | TypeAliasDecl ;
OperatorDecl ::= AttributeList? Visibility? MemberModifier*
                 "operator" OperatorToken "(" ParameterList? ")"
                 ReturnClause? WhereClause? FunctionBody ;
OperatorToken ::= "+" | "-" | "*" | "/" | "%" | "==" | "!="
                | "<" | "<=" | ">" | ">=" ;
EmptyDecl ::= SEMI ;
```

Uma classe estende no máximo uma classe e implementa múltiplas interfaces. Herança usa `:` e implementação usa `by`; por exemplo, `class Funcionario: Pessoa by FormaPagamento`. Todos os campos obrigatórios devem estar inicializados ao final de `construct`. `super(argumentos)` constrói a base antes dos campos da subclasse; `deinit` e destruição por escopo continuam planejados.

Corpos ausentes só são válidos em membro abstrato, requisito de interface ou declaração estrangeira.

<a id="gramatica-9-protocols-de-dados"></a>

### 9. Data imutável

`protocol` foi removido. `interface` representa contratos; `data` representa
agregados transitivamente imutáveis inspirados em data classes do Kotlin e
records do Java.

```ebnf
DataDecl ::= Visibility? "data" "class"? Identifier
             ("(" DataComponentList? ")")?
             ("by" Identifier ("," Identifier)*)? PermitedClause?
             (ClassBody | SEMI) ;
DataComponentList ::= DataComponent ("," DataComponent)* ","? ;
DataComponent ::= Identifier ":" Type ;
```

A forma com componentes gera campos privados e imutáveis com autorização `permited` e `construct`
público com os mesmos nomes como rótulos de parâmetros. Apenas o próprio tipo e
as entidades nominais listadas em `permited` acessam diretamente os campos. Componentes `nome: tipo;` no corpo também geram construtor canônico; campos
explícitos com `var` seguem as regras de inicialização. O corpo permite métodos e conformidade
de interfaces. `data` é sempre final, concreto e não estende classes.

Campos podem conter escalares, strings, opcionais desses tipos e outros `data`.
São rejeitados `var mut`, propriedades computadas, arrays, closures, referências
a classes comuns e grafos recursivos. `==`, `!=` e `?==` com tipo estático `data`
comparam os campos recursivamente. O armazenamento atual ainda usa objetos;
conversão para interface conserva identidade de referência. `hashCode()`, `toString()` e impressão estrutural são gerados automaticamente.
`copy`, `componentN`, decomposição e serialização automática não estão implementados.

```argos
data User(id: int, name: string, email: string?);
```

<a id="gramatica-10-interfaces"></a>

### 10. Interfaces

`interface` define contrato comportamental sem estado armazenado.

```ebnf
InterfaceDecl ::= AttributeList? Visibility? "interface" Identifier
                  GenericParameterClause? InterfaceInheritanceClause?
                  WhereClause? InterfaceBody ;
InterfaceInheritanceClause ::= ":" NamedType ("," NamedType)* ;
InterfaceBody ::= "{" InterfaceMember* "}" ;

InterfaceMember ::= InterfaceMethodRequirement
                  | InterfacePropertyRequirement
                  | AssociatedTypeDecl | DefaultMethodDecl
                  | NestedTypeDecl | EmptyDecl ;
InterfaceMethodRequirement ::= AttributeList? Visibility?
                               FunctionSignature SEMI ;
InterfacePropertyRequirement ::= AttributeList? Visibility?
                                 "var" ("mut")? Identifier ":" Type
                                 "{" AccessorRequirementList "}" ;
AccessorRequirementList ::= GetterRequirement SetterRequirement?
                          | SetterRequirement GetterRequirement ;
GetterRequirement ::= "getter" "(" ")" SEMI ;
SetterRequirement ::= "setter" "(" Identifier? ")" SEMI ;
AssociatedTypeDecl ::= "type" Identifier GenericBoundList? SEMI ;
DefaultMethodDecl ::= AttributeList? Visibility?
                      FunctionSignature FunctionBody ;
```

Interfaces são públicas por padrão e podem estender múltiplas interfaces, sem
estender classes. Conformidade é nominal. A gramática ampliada acima inclui
recursos planejados: implementações padrão e tipos associados ainda não são aceitos.

<a id="gramatica-11-enums"></a>

### 11. Enums

```ebnf
EnumDecl ::= AttributeList? Visibility? "enum" Identifier
             GenericParameterClause? EnumConformanceClause?
             WhereClause? EnumBody ;
EnumConformanceClause ::= ":" NamedType ("," NamedType)* ;
EnumBody ::= "{" EnumMember* "}" ;
EnumMember ::= EnumCaseDecl | MethodDecl | ComputedPropertyDecl
             | NestedTypeDecl | EmptyDecl ;
EnumCaseDecl ::= AttributeList? "case" EnumCaseElementList SEMI ;
EnumCaseElementList ::= EnumCaseElement ("," EnumCaseElement)* ;
EnumCaseElement ::= Identifier EnumPayload? ("=" ConstantExpression)? ;
EnumPayload ::= "(" TypeList? ")" | "(" NamedTypeList ")" ;
NamedTypeList ::= NamedTypeElement ("," NamedTypeElement)* ","? ;
NamedTypeElement ::= Identifier ":" Type ;
```

Enums podem ser conjuntos simples ou variantes com payload.

<a id="gramatica-12-generics"></a>

### 12. Generics

```ebnf
GenericParameterClause ::= "<" GenericParameterList ">" ;
GenericParameterList ::= GenericParameter ("," GenericParameter)* ","? ;
GenericParameter ::= Identifier GenericBoundList? DefaultTypeArgument? ;
GenericBoundList ::= ":" GenericBound ("&" GenericBound)* ;
GenericBound ::= NamedType ;
DefaultTypeArgument ::= "=" Type ;

GenericArgumentClause ::= "<" GenericArgumentList ">" ;
GenericArgumentList ::= Type ("," Type)* ","? ;

WhereClause ::= "where" WherePredicateList ;
WherePredicateList ::= WherePredicate ("," WherePredicate)* ;
WherePredicate ::= Type ":" GenericBound ("&" GenericBound)*
                 | Type "==" Type ;
```

Constraints após `:` expressam conformidade. `&` combina limites. `where` expressa relações adicionais. O compilador pode monomorfizar ou compartilhar código, sem alterar a semântica.

Para evitar o conflito entre argumentos genéricos e operadores `<`/`>` em contexto de expressão, especialização explícita usa `::<...>`:

```argos
let value = parse::<int>(text);
```

<a id="gramatica-13-funções-e-parâmetros"></a>

### 13. Funções e parâmetros

```ebnf
FunctionDecl ::= AttributeList? Visibility? MemberModifier*
                 FunctionSignature SimpleFunctionBody ;
FunctionSignature ::= "function" Identifier GenericParameterClause?
                      "(" ParameterList? ")"
                      ReturnClause? ThrowsClause? WhereClause? ;
ReturnClause ::= "->" Type ;
ThrowsClause ::= "throws" Type? ;
FunctionBody ::= Block ;
SimpleFunctionBody ::= Block | Statement ;

ParameterList ::= Parameter ("," Parameter)* ","? ;
Parameter ::= AttributeList? ParameterLabel? Identifier ":" Type
              ParameterModifier* DefaultArgument? ;
ParameterLabel ::= Identifier | "_" ;
ParameterModifier ::= "var" | "mut" | "inout" ;
DefaultArgument ::= "=" Expression ;
```

Sem rótulo explícito, o nome local é também o rótulo externo. `_` remove o rótulo. Argumentos posicionais devem preceder nomeados.

<a id="gramatica-14-lambdas-e-closures"></a>

### 14. Lambdas e closures

A forma solta `let () => { ... }` é inválida: `let` exige binding e `=`. A forma correta é uma lambda usada como expressão.

```ebnf
LambdaExpression ::= LambdaParameterClause LambdaReturnClause?
                     LambdaEffects? "=>" LambdaBody ;
LambdaParameterClause ::= "(" LambdaParameterList? ")" ;
LambdaParameterList ::= LambdaParameter ("," LambdaParameter)* ","? ;
LambdaParameter ::= Identifier (":" Type)? ;
LambdaReturnClause ::= "->" Type ;
LambdaEffects ::= "async"? ThrowsClause? ;
LambdaBody ::= Expression | Block ;
```

```argos
let increment = (x: int) -> int => x + 1;

let task = () => {
    counter += 1;
};
```

Capturas são imutáveis por padrão. Captura mutável é explícita e deve respeitar ownership. Closures que escapam não podem reter referências locais inválidas.

<a id="gramatica-15-variáveis-e-padrões"></a>

### 15. Variáveis e padrões

```ebnf
VariableDecl ::= AttributeList? Visibility? StorageModifier?
                 BindingKind Pattern VariableType?
                 VariableInitializer? SEMI ;
BindingKind ::= "var" ("mut")? ;
StorageModifier ::= "static" | "thread_local" ;
VariableType ::= ":" Type ;
VariableInitializer ::= "=" Expression ;

Pattern ::= IdentifierPattern | WildcardPattern | TuplePattern
          | DataPattern | EnumPattern | LiteralPattern ;
IdentifierPattern ::= Identifier ;
WildcardPattern ::= "_" ;
TuplePattern ::= "(" PatternList? ")" ;
PatternList ::= Pattern ("," Pattern)* ","? ;
DataPattern ::= QualifiedName "(" NamedPatternList? ")" ;
NamedPatternList ::= NamedPattern ("," NamedPattern)* ","? ;
NamedPattern ::= Identifier ":" Pattern ;
EnumPattern ::= QualifiedName ("(" PatternList? ")")? ;
LiteralPattern ::= Literal ;
```

Sem tipo escrito, a declaração requer inicializador inferível. Sem inicializador, requer tipo e prova de inicialização antes do uso. `var` recebe valor uma vez; `var mut` pode ser reatribuída.

<a id="gramatica-16-sistema-de-tipos"></a>

### 16. Sistema de tipos

| Tipo | Semântica | Largura |
| --- | --- | ---: |
| `bool` | lógico | 1 byte mínimo de armazenamento |
| `byte` | inteiro sem sinal | 8 bits |
| `sbyte` | inteiro com sinal | 8 bits |
| `short` / `ushort` | inteiro com/sem sinal | 16 bits |
| `int` / `uint` | inteiro com/sem sinal | 32 bits |
| `long` / `ulong` | inteiro com/sem sinal | 64 bits |
| `isize` / `usize` | inteiro da palavra-alvo | dependente do alvo |
| `float` | IEEE 754 binary32, se suportado | 32 bits |
| `double` | IEEE 754 binary64, se suportado | 64 bits |
| `char` | escalar Unicode | 32 bits lógicos |
| `void` | ausência de retorno | sem armazenamento |
| `never` | computação que não retorna | inabitável |

`byte` é sem sinal para representar bytes, registradores e buffers. `sbyte` é a contraparte assinada. `string` é tipo nominal intrínseco da biblioteca padrão, logicamente UTF-8.

```ebnf
Type ::= FunctionType OptionalSuffix*
       | PrimaryType OptionalSuffix* ;
PrimaryType ::= PrimitiveType | NamedType | TupleType | ArrayType
              | SliceType | PointerType | ReferenceType
              | InferredType | "(" Type ")" ;
PrimitiveType ::= "bool" | "byte" | "sbyte" | "short" | "ushort"
                | "int" | "uint" | "long" | "ulong"
                | "isize" | "usize" | "float" | "double"
                | "char" | "string" | "void" | "never" ;
NamedType ::= QualifiedName GenericArgumentClause? ;
TupleType ::= "(" TypeList ")" ;
TypeList ::= Type ("," Type)+ ","? ;
ArrayType ::= "[" Type ";" ConstantExpression "]" ;
SliceType ::= "[" Type "]" ;
PointerType ::= "*" ("const" | "mut")? Type ;
ReferenceType ::= "&" "mut"? Type ;
FunctionType ::= "(" FunctionTypeParameterList? ")" "->" Type ;
FunctionTypeParameterList ::= Type ("," Type)* ","? ;
OptionalSuffix ::= "?" ;
InferredType ::= "_" ;
```

Opcionais aninhados são preservados: `T??` não é automaticamente `T?`.

<a id="gramatica-17-null-safety"></a>

### 17. Null-safety

- `T` nunca contém `null`;
- `T?` contém ausência ou `T`;
- `T` promove implicitamente para `T?`;
- `T?` não converte implicitamente para `T`;
- `?.` realiza acesso seguro;
- `??` avalia fallback apenas na ausência;
- `?=` atribui somente quando a origem contém valor;
- `??=` atribui apenas quando o destino está ausente;
- `?==` compara opcionais de modo elevado;
- somente `!!` é force unwrap; `!` é exclusivamente negação lógica.

Para `?==`: duas ausências são iguais; uma ausência e um valor são diferentes; dois valores usam a igualdade do tipo-base. Operandos de short-circuit são avaliados no máximo uma vez.

<a id="gramatica-18-instruções"></a>

### 18. Instruções

```ebnf
Statement ::= Block | VariableDecl | IfStatement | WhileStatement
            | DoWhileStatement | ForStatement | MatchStatement
            | ReturnStatement | BreakStatement | ContinueStatement
            | ThrowStatement | TryStatement | DeferStatement
            | UnsafeStatement | ExpressionStatement | EmptyStatement ;
Block ::= "{" Statement* "}" ;
SimpleBody ::= Block | Statement ;
EmptyStatement ::= SEMI ;
ExpressionStatement ::= Expression SEMI ;

IfStatement ::= "if" Expression SimpleBody ElseClause? ;
ElseClause ::= "else" (IfStatement | SimpleBody) ;
WhileStatement ::= Label? "while" Expression SimpleBody ;
DoWhileStatement ::= Label? "do" Block "while" Expression SEMI ;

ForStatement ::= Label? "for" Pattern "in" Expression Block
               | Label? "for" "(" ForInitializer? SEMI
                 Expression? SEMI ExpressionList? ")" Block ;
ForInitializer ::= VariableDeclNoTerminator | ExpressionList ;
VariableDeclNoTerminator ::= BindingKind Pattern VariableType?
                             VariableInitializer? ;

MatchStatement ::= "match" Expression "{" MatchArm* "}" ;
MatchArm ::= "case" Pattern MatchGuard? "=>" MatchBody
           | "default" "=>" MatchBody ;
MatchGuard ::= "if" Expression ;
MatchBody ::= Expression SEMI | Block ;

ReturnStatement ::= "return" Expression? SEMI ;
BreakStatement ::= "break" Identifier? SEMI ;
ContinueStatement ::= "continue" Identifier? SEMI ;
ThrowStatement ::= "throw" Expression SEMI ;
DeferStatement ::= "defer" Block ;
UnsafeStatement ::= "unsafe" Block ;
Label ::= Identifier ":" ;

TryStatement ::= "try" Block CatchClause+ FinallyClause?
               | "try" Block FinallyClause ;
CatchClause ::= "catch" ("(" Pattern (":" Type)? ")")? Block ;
FinallyClause ::= "finally" Block ;
```

Condições exigem `bool`, não `bool?`. Funções não `void` retornam em todos os caminhos alcançáveis.

<a id="gramatica-19-expressões-e-precedência"></a>

### 19. Expressões e precedência

| Nível | Construção | Associatividade |
| ---: | --- | --- |
| 10 | `=`, compostas, `?=`, `??=` | direita |
| 15 | lambda `=>` | direita |
| 20 | `??` | direita |
| 25 | `||` | esquerda, short-circuit |
| 30 | `&&` | esquerda, short-circuit |
| 35 | `==`, `!=`, `?==` | esquerda |
| 40 | `<`, `<=`, `>`, `>=`, `is`, `as`, `as?` | esquerda |
| 45 | `..`, `..=` | não associativo |
| 50 | `+`, `-` | esquerda |
| 60 | `*`, `/`, `%` | esquerda |
| 70 | prefixos | direita |
| 80 | chamada, índice, membro, `?.`, `!!`, `++`, `--` | esquerda |

```ebnf
Expression ::= AssignmentExpression ;
AssignmentExpression ::= LambdaExpression
                       | NullCoalescingExpression
                       | Assignable AssignmentOperator AssignmentExpression ;
AssignmentOperator ::= "=" | "+=" | "-=" | "*=" | "/=" | "%="
                     | "?=" | "??=" ;

NullCoalescingExpression ::= LogicalOrExpression
                           | LogicalOrExpression "??" NullCoalescingExpression ;
LogicalOrExpression ::= LogicalAndExpression
                      | LogicalOrExpression "||" LogicalAndExpression ;
LogicalAndExpression ::= EqualityExpression
                       | LogicalAndExpression "&&" EqualityExpression ;
EqualityExpression ::= RelationalExpression
                     | EqualityExpression EqualityOperator RelationalExpression ;
EqualityOperator ::= "==" | "!=" | "?==" ;
RelationalExpression ::= RangeExpression
                       | RelationalExpression RelationalOperator RangeExpression ;
RelationalOperator ::= "<" | "<=" | ">" | ">=" | "is" | "as" | "as?" ;
RangeExpression ::= AdditiveExpression
                  | AdditiveExpression (".." | "..=") AdditiveExpression ;
AdditiveExpression ::= MultiplicativeExpression
                     | AdditiveExpression ("+" | "-") MultiplicativeExpression ;
MultiplicativeExpression ::= PrefixExpression
                           | MultiplicativeExpression ("*" | "/" | "%") PrefixExpression ;
PrefixExpression ::= PostfixExpression
                   | PrefixOperator PrefixExpression
                   | "await" PrefixExpression | "try" PrefixExpression ;
PrefixOperator ::= "+" | "-" | "!" | "~" | "&" | "*" ;

PostfixExpression ::= PrimaryExpression
                    | PostfixExpression CallSuffix
                    | PostfixExpression IndexSuffix
                    | PostfixExpression MemberSuffix
                    | PostfixExpression GenericCallSuffix
                    | PostfixExpression PostfixOperator ;
CallSuffix ::= "(" ArgumentList? ")" ;
GenericCallSuffix ::= "::" GenericArgumentClause CallSuffix ;
IndexSuffix ::= "[" ArgumentList "]" ;
MemberSuffix ::= ("." | "?.") Identifier ;
PostfixOperator ::= "!!" | "++" | "--" ;

PrimaryExpression ::= Literal | Identifier | "this" | "super"
                    | ParenthesizedExpression | TupleExpression
                    | ArrayExpression | ObjectCreationExpression
                    | MatchExpression | ForeignExecutorExpression ;
ParenthesizedExpression ::= "(" Expression ")" ;
TupleExpression ::= "(" Expression "," ExpressionListTail? ")" ;
ExpressionListTail ::= Expression ("," Expression)* ","? ;
ArrayExpression ::= "[" ArgumentList? "]" ;
ObjectCreationExpression ::= "new" NamedType "(" ArgumentList? ")" ;

ArgumentList ::= Argument ("," Argument)* ","? ;
Argument ::= (Identifier ":")? Expression ;
ExpressionList ::= Expression ("," Expression)* ;

MatchExpression ::= "match" Expression "{" MatchExpressionArm+ "}" ;
MatchExpressionArm ::= "case" Pattern MatchGuard? "=>" Expression SEMI
                     | "default" "=>" Expression SEMI ;

Assignable ::= Identifier | PostfixExpression "." Identifier
             | PostfixExpression "[" ArgumentList "]"
             | "*" PrefixExpression ;

Literal ::= IntegerLiteral | FloatLiteral | CharLiteral | StringLiteral
          | MultilineStringLiteral | BooleanLiteral | NullLiteral ;
ConstantExpression ::= Expression ;
```

Atribuição exige l-value mutável. Operadores lógicos exigem `bool`. Ranges são não associativos. Comparações encadeadas exigem conjunção. Ordem de avaliação é esquerda para direita, salvo short-circuit.

<a id="gramatica-20-herança-e-despacho"></a>

### 20. Herança e despacho

Classes possuem herança simples de implementação e múltipla conformidade de interfaces.

- membros são não virtuais por padrão;
- `abstract` introduz requisito;
- `override` confirma membro herdado compatível;
- `final override` impede nova sobrescrita;
- assinaturas adicionais exigem `overload`; retorno e rótulos não diferenciam sobrecargas;
- `super()` é a primeira instrução de `construct`, ou a base acessível sem argumentos é construída implicitamente;
- `super.membro` seleciona a implementação da base, conservando visibilidade;
- campos não são sobrescritos;
- casts descendentes exigem teste ou conversão explícita.

<a id="gramatica-21-ownership-e-memória"></a>

### 21. Ownership e memória

- cada recurso possui proprietário;
- saída de escopo destrói em ordem inversa;
- movimento invalida origem não copiável;
- `&T` é referência compartilhada;
- `&mut T` é referência mutável exclusiva;
- `*const T` e `*mut T` são raw e exigem `unsafe`;
- referências não sobrevivem ao valor referenciado;
- `data` possui igualdade por conteúdo; armazenamento inline ainda é planejado;
- alocações ocultas podem ser proibidas por perfil.

<a id="gramatica-22-unsafe-e-bare-metal"></a>

### 22. Unsafe e bare metal

`unsafe` autoriza desreferência raw, aritmética de ponteiros, casts de endereço, MMIO, FFI insegura e Assembly. Tipagem, escopos e mutabilidade continuam ativos.

```ebnf
UnsafeBlock ::= "unsafe" Block ;
```

`@raw` é um atributo aplicado a uma função `unsafe`; não cria uma segunda produção concorrente para funções. MMIO exige acesso volátil e barreiras apropriadas. O perfil hard real-time pode rejeitar heap, recursão, exceptions, runtimes estrangeiros e latência não limitada.

<a id="gramatica-23-interoperabilidade"></a>

### 23. Interoperabilidade

```ebnf
ForeignDecl ::= ForeignFunctionDecl | ForeignBlockDecl ;
ForeignFunctionDecl ::= AttributeList? Visibility? "extern"
                        ForeignLanguage StringLiteral?
                        FunctionSignature SEMI ;
ForeignBlockDecl ::= "extern" ForeignLanguage "{" ForeignFunctionDecl* "}" ;
ForeignLanguage ::= "C" | "C++" | "Rust" | "Swift" | "Assembly" ;

ForeignExecutorExpression ::= ForeignExecutorName
                              "(" ForeignSource ExecutorOptionList? ")" ;
ForeignExecutorName ::= "c_executor" | "cpp_executor" | "rust_executor"
                      | "swift_executor" | "asm_executor" ;
ForeignSource ::= StringLiteral | MultilineStringLiteral ;
ExecutorOptionList ::= "," NamedArgument ("," NamedArgument)* ;
NamedArgument ::= Identifier ":" Expression ;
```

Executores não transformam Argos em linguagem estrangeira. Eles encaminham a unidade estrangeira ao frontend/toolchain apropriado e ligam ou executam o artefato resultante.

Cada executor define alvo, toolchain, modo, símbolo de entrada, ABI, dependências, stdout/stderr, timeout, isolamento, ownership e tratamento de falhas.

Uma unidade com `main` usa modo `program` isolado para não conflitar com a entrada Argos:

```argos
let result = c_executor("""
    #include <stdio.h>

    int main(void) {
        printf("Olá, Mundo!\n");
        return 0;
    }
""", mode: "program");
```

Formas equivalentes existem para C++, Rust e Swift. Assembly requer arquitetura e dialeto.

<a id="gramatica-24-ast-e-hir"></a>

### 24. AST e HIR

A AST separa declarações, instruções, expressões, padrões, tipos, atributos e construções estrangeiras. Todo nó guarda span e identidade estável.

A HIR acrescenta símbolo, tipo, coerções, overload, bindings, capturas, categoria de valor, efeitos, região unsafe e estratégia estrangeira. AST/HIR não armazenam tipos LLVM, registradores físicos ou detalhes da VM.

<a id="gramatica-25-diagnósticos-obrigatórios"></a>

### 25. Diagnósticos obrigatórios

Devem ser diagnosticados:

- nomes e tipos inexistentes;
- duplicatas e ciclos de herança/constraints;
- classe final estendida ou entidade fora da lista `permited` de uma base sealed;
- sobrescrita sem `override`;
- requisito abstrato ausente;
- estado armazenado em interface;
- mutação de campo de data;
- violações de optional e force unwrap;
- caminhos sem retorno;
- patterns não exaustivos;
- l-values inválidos;
- ponteiros fora de `unsafe`;
- executor indisponível;
- ABI incompatível;
- recurso sem lowering no backend escolhido.

Sucesso silencioso é proibido.

<a id="gramatica-26-exemplos-integrados"></a>

### 26. Exemplos integrados

<a id="gramatica-261-classe-e-interface"></a>

#### 26.1. Classe e interface

```argos
public interface Identifiable<TId> {
    public var id: TId { getter(); }
}

sealed class Account<TId> by Identifiable<TId>
where TId: Hashable {
    public var id: TId;
    private var mut balance: long;

    public construct(id: TId, initialBalance: long = 0) {
        this.id = id;
        this.balance = initialBalance;
    }

    public final function deposit(_ amount: long) -> void {
        if amount > 0 {
            balance += amount;
        }
    }
}
```

<a id="gramatica-262-protocol"></a>

#### 26.2. Data

```argos
data SensorReading(sensor: string, value: double, timestamp: int);
```

<a id="gramatica-263-null-safety"></a>

#### 26.3. Null-safety

```argos
function display(user: User?) -> string {
    return user?.profile?.name ?? "Desconhecido";
}
```

<a id="gramatica-264-bare-metal"></a>

#### 26.4. Bare metal

```argos
@raw
unsafe function writeRegister(address: usize, value: byte) -> void {
    let register: *mut byte = address as *mut byte;
    *register = value;
}
```

<a id="gramatica-27-embarcados"></a>

### 27. Embarcados

```text
Argos → Lexer → Parser LR(1) → AST/HIR → IR Argos
                                      ├─→ Bytecode → VM
                                      └─→ Machine IR → objeto/máquina
```

Não existe etapa C, C++ ou LLVM. Cada backend Argos conhece diretamente ISA, ABI, layout e formato do alvo.

<a id="gramatica-28-perfil-mínimo"></a>

### 28. Perfil mínimo

O primeiro perfil completo deve conter módulos simples, funções, variáveis, primitivos, `string` mínima, opcionais, classes finais, data, interfaces, generics monomorfizados, controle de fluxo, lambdas restritas, IR verificada, VM, um backend direto e FFI C com símbolos compilados.

Recursos posteriores falham explicitamente até possuírem semântica e lowering.

<a id="gramatica-29-decisões-abertas"></a>

### 29. Decisões abertas

1. Classes serão finais ou abertas por padrão?
2. Haverá contagem de referências opt-in?
3. Qual será a capture list definitiva?
4. Exceptions usarão unwinding ou resultado tipado por perfil?
5. Qual será o primeiro backend nativo?
6. Bytecode será de registradores ou híbrido?
7. Quais layouts serão ABI-estáveis?
8. Executors rodam em compilação, runtime ou ambos?
9. Toolchains estrangeiras serão plugins ou componentes incorporados?

<a id="gramatica-30-conformidade"></a>

### 30. Conformidade

Uma construção é suportada somente quando possui produção LR(1) sem conflitos, CST/AST, análise semântica, HIR/IR, backend explícito e testes positivos, negativos, de recuperação e de equivalência VM/nativo.

Reconhecer sintaxe não significa implementar a linguagem.

---

<a id="gramatica-31-resumo-da-arquitetura-do-compilador-para-embarcados"></a>

### 31. Resumo da Arquitetura do Compilador para Embarcados

```text
[Código ArgosLang (.argos)]
           ↓
     [Lexer + Parser]
           ↓
   [AST + Type Checker] (Garante Imutabilidade e Null Safety)
           ↓
[Gerador de C / LLVM IR (Zero-GC)]
           ↓
[avr-gcc / xtensa-esp32-elf-gcc / clang]
           ↓
[Binário Firmware (.hex / .bin)] → Gravação na Flash do Microcontrolador (Arduino / ESP32)
```

<a id="gramatica-32-incremento-implementado-em-26102026"></a>

### 32. Incremento implementado em 26/10/2026

O parser manual agora representa classes, interfaces de métodos, `construct`, `new`,
tipos de função e lambdas na AST. A análise valida visibilidade, inicialização,
herança simples, conformidade, `virtual`/`abstract`/`override`/`final` e capturas
imutáveis. Lambdas exigem tipos explícitos nos parâmetros; blocos sem tipo de
retorno assumem `void`. Métodos são não virtuais por padrão.

Classes concretas sem bases e closures com captura imutável têm lowering para
IR, VM e x86-64. Propriedades computadas foram acrescentadas em 27/10/2026.
Herança executável, genéricos e membros estáticos,
captura mutável, ownership e destruição por escopo permanecem planos.
A memória desta etapa é uma arena com duração da execução. Consulte a matriz
atual em [ArgosLang MVP](#mvp-poo-e-funções-como-valores-26-de-outubro-de-2026).


<a id="gramatica-33-construtores-e-acessores-27102026"></a>

### 33. Construtores e acessores — 27/10/2026

O construtor é declarado exclusivamente com `construct(...)`. A sintaxe antiga
`init(...)` foi removida, sem alias ou modo de compatibilidade. A construção
continua sendo `new Tipo(...)`; `construct` não pode ser chamado como método de
uma instância. Sem construtor explícito, todos os campos armazenados precisam
de valor padrão. `deinit` permanece um recurso planejado independente.

Propriedades computadas exigem tipo e `getter()`. Uma propriedade `var mut`
pode declarar `setter()` ou `setter(nome)`. O parâmetro do setter tem o tipo da
propriedade e chama-se `value` quando omitido. O getter tem retorno igual ao
tipo da propriedade; o setter retorna `void`. Propriedades não armazenam um
valor próprio e não aceitam inicializador: estado interno usa campos privados.

A leitura `objeto.propriedade` invoca o getter; a atribuição invoca o setter.
Atribuições compostas leem uma vez, avaliam o operando direito e escrevem uma
vez, sem reavaliar o objeto receptor. Propriedades sem setter são somente
leitura, inclusive em `construct`. Acessores só podem observar uma instância
completamente inicializada. Modificadores de visibilidade pertencem à
propriedade; não há visibilidade independente por acessor nesta etapa.

Interfaces e propriedades `abstract` usam assinaturas `getter();` e `setter();`.
A conformidade valida tipo, visibilidade e presença de setter quando exigido.
A execução de propriedades de classes concretas sem bases funciona na VM e no
x86-64; despacho por interfaces ou herança mantém os limites anteriores.


---

<a id="evolucao"></a>

<a id="evolucao-evolução-da-linguagem-após-o-mvp"></a>

## Evolução da linguagem após o MVP

Este documento fixa as decisões para a expansão solicitada. Uma palavra-chave
somente é anunciada como disponível depois de lexer, parser, AST, semântica, IR
e ao menos uma rota de execução terem cobertura de testes.

Para o trabalho necessário antes dessa expansão — fechar o núcleo do MVP atual,
incluindo VM, compilador nativo, REPL, diagnósticos e conformidade — consulte o
[roteiro para um MVP amplo e confiável](#roadmap).

<a id="evolucao-regras-básicas"></a>

### Regras básicas

- Tipos e palavras-chave são minúsculos: `int`, `string`, `bool`, `nil`, `var`.
- `var` é imutável por padrão; `var mut` permite reatribuição. `let` não faz
  parte da sintaxe nova.
- `int` é assinado. Os equivalentes sem sinal são `uint`, `ushort`, `ulong` e
  `usize`; não haverá o erro de grafia `usigned` como palavra-chave.
- `nil` é a única ausência explícita. `undefined` não é valor: leitura antes
  de inicialização é erro estático.
- `any` é dinâmico e recebe uma tag de runtime; `unknown` exige refinamento por
  `when`/`is` antes de uso específico.

<a id="evolucao-ordem-de-implementação"></a>

### Ordem de implementação

1. Fluxo: `for`, `switch` e `when`. `&&` e `||` já executam com curto-circuito.
   O `while` recebe uma expressão booleana; condições múltiplas são
   combinadas explicitamente, sem uma sintaxe ambígua. `break` e `continue`
   já estão disponíveis em `while`.
2. Tipos nominais: `data` (record imutável com permissões `permited`), `enum`, `class` e
   `interface`, com visibilidade pública padrão de tipos e funções de topo, privada padrão de
   métodos de classe e pública padrão de requisitos de interface.
3. Padrões: `when` como expressão e instrução, incluindo `nil`, variantes de
   `enum`/`data` e refinamento de `unknown`.
4. Genéricos reificados: `T`, `T extends Base`, `where` e wildcards `?`,
   `? extends T`. A reificação precisa chegar à VM e à reflexão de tipos antes
   de ser exposta como recurso estável.
5. Exceções: `try`, `catch`, `finally`, `throw`, `throws`; `except` e `threw`
   não entram por serem redundantes.
6. Módulos e concorrência: `package`, `import`, `export`, `async`, `await` e
   `sync`. `sync` significa exclusivamente região de exclusão mútua.

`static`, `extern`, `inline`, `lazy`, `eager`, `raw` e `unsafe` são
modificadores contextuais, não tipos. `eager` é o padrão; `lazy` cria uma
inicialização sob demanda. `sealed` e `permited` restringem a hierarquia nominal e o acesso aos campos de `data`.


---

<a id="codegen"></a>

<a id="codegen-arquitetura-de-geração-de-código-interoperabilidade-c-e-execução-híbrida-da-argoslang"></a>

## Arquitetura de Geração de Código, Interoperabilidade C e Execução Híbrida da ArgosLang

Este documento especifica a arquitetura de **Geração de Código**, o modelo de **Execução Híbrida** (Interpretada e Compilada), a **Interoperabilidade Nativa com a Linguagem C** e o conjunto de instruções da **Pseudo-Máquina (IR / Bytecode)** da **ArgosLang**.

---

<a id="codegen-1-visão-geral-da-execução-híbrida"></a>

### 1. Visão Geral da Execução Híbrida

A **ArgosLang** é projetada com uma arquitetura híbrida flexível que atende tanto ao prototipamento rápido quanto a ambientes embarcados de alta performance e escassez de recursos (como Arduino AVR, ESP8266, ESP32, STM32 e PCs antigos):

```text
               ┌───────────────────────────────┐
               │    Código ArgosLang (.argos)  │
               └───────────────┬───────────────┘
                               │
               ┌───────────────▼───────────────┐
               │  Lexer + Parser + TypeCheck   │ (Frontend)
               └───────────────┬───────────────┘
                               │
               ┌───────────────▼───────────────┐
               │ AST Validada + Tipada (Argos) │
               └───────────────┬───────────────┘
                               │
               ┌───────────────▼───────────────┐
               │ Lowering / Emissão de IR      │
               │ (Instruções da Pseudo-Máquina)│
               └───────┬───────────────┬───────┘
                       │               │
        ┌──────────────▼───┐       ┌───▼───────────────────────────┐
        │ Modo Interpretado│       │ Modo Compilado (Nativo AOT)   │
        │   (Bytecode VM)  │       │ (Transpilação C / LLVM IR)    │
        └──────────────┬───┘       └───────────────┬───────────────┘
                       │                           │
        ┌──────────────▼───┐       ┌───────────────▼───────────────┐
        │ Execução Direta  │       │  Binário Nativo (.elf / .hex) │
        │ (REPL / Scripts) │       │ (Firmware para Microcontrol.) │
        └──────────────────┘       └───────────────────────────────┘
```

---

<a id="codegen-2-interoperabilidade-nativa-com-a-linguagem-c-c_function"></a>

### 2. Interoperabilidade Nativa com a Linguagem C (`c_function`)

Para permitir acesso direto a registradores de hardware, drivers existentes e código C nativo sem *overhead*, a ArgosLang inclui o construto nativo `c_function("código C")` e blocos de código C embutidos.

<a id="codegen-21-sintaxe-de-inclusão-de-código-c-nativo"></a>

#### 2.1. Sintaxe de Inclusão de Código C Nativo

```swift
// Inclusão de código C direto para manipulação de baixo nível
func delayMs(milliseconds: USize) -> Void {
    c_function("""
        #include <util/delay.h>
        _delay_ms(milliseconds);
    """);
}

func readRegister(address: USize) -> Byte {
    var result: Byte = 0;
    c_function("""
        volatile uint8_t *ptr = (volatile uint8_t *)(uintptr_t)address;
        result = *ptr;
    """);
    return result;
}
```

<a id="codegen-22-regras-de-integração-c"></a>

#### 2.2. Regras de Integração C
1. **Mapeamento de Tipos Nativos:** Os tipos primitivos da ArgosLang mapeiam 1:1 com os tipos C:
   - `Byte` ↔ `int8_t` / `uint8_t`
   - `Short` ↔ `int16_t`
   - `Int` ↔ `int32_t`
   - `Long` ↔ `int64_t`
   - `USize` ↔ `uintptr_t` / `size_t`
   - `Bool` ↔ `bool` (`stdbool.h`)
2. **Acesso a Variáveis ArgosLang em C:** Variáveis declaradas no escopo do código ArgosLang são exportadas como variáveis C com os mesmos nomes dentro do bloco `c_function`.

---

<a id="codegen-3-modelo-de-memória-gerenciamento-manual-sem-gc-estilo-c-raii"></a>

### 3. Modelo de Memória: Gerenciamento Manual Sem GC (Estilo C / RAII)

A ArgosLang **não possui Garbage Collector (Zero-GC)**. Toda a memória é gerenciada deterministicamente através de:

1. **Alocação na Pilha (*Stack Allocation*):** Variáveis locais e structs são alocadas e liberadas automaticamente ao sair do escopo (RAII).
2. **Alocação Manual no Heap (Estilo C):** Para dados de tamanho dinâmico ou tempos de vida estendidos, são fornecidas funções de alocação manual sem GC:

```swift
func allocateBuffer(size: USize) -> Byte? {
    // Alocação de memória manual (equivalente ao malloc do C)
    let ptr: USize = c_function("malloc(size)");
    return ptr;
}

func freeBuffer(ptr: USize) -> Void {
    // Liberação manual explícita (equivalente ao free do C)
    c_function("free((void*)ptr);");
}
```

---

<a id="codegen-4-modelos-de-programação-vm-vs-memória-raw-estilo-c"></a>

### 4. Modelos de Programação: VM vs. Memória Raw (Estilo C)

A ArgosLang oferece ao desenvolvedor **dois modelos de programação** distintos e coexistentes. A escolha pode ser feita por arquivo, por módulo ou mesmo por bloco de código dentro do mesmo programa.

```text
┌─────────────────────────────────────────────────────────────────┐
│                     Código ArgosLang                            │
│                                                                 │
│   ┌──────────────────────────┐   ┌─────────────────────────┐   │
│   │  Modo VM / Pseudo-Máq.  │   │  Modo Raw / Memória C   │   │
│   │  (padrão para scripts   │   │  (bare-metal, drivers,  │   │
│   │   e lógica de alto nível│   │   performance crítica)  │   │
│   └──────────┬───────────────┘   └────────────┬────────────┘   │
└──────────────┼──────────────────────────────── ┼───────────────┘
               │                                 │
       Bytecode / Opcodes             Ponteiros / Endereços
       interpretados pela VM          diretos na memória física
               │                                 │
       Execução portável             Execução nativa imediata
       em qualquer plataforma        (sem camada de VM)
```

---

<a id="codegen-41-modo-vm-programação-com-pseudo-máquina-padrão"></a>

#### 4.1. Modo VM — Programação com Pseudo-Máquina (Padrão)

O modo padrão da ArgosLang opera sobre a **VM de Registradores / Pilha**. O compilador emite *opcodes* da pseudo-máquina que são interpretados em tempo de execução (modo `argosc run`) ou compilados AOT para binário nativo (modo `argosc compile`).

É o modo **recomendado** para lógica de aplicação, algoritmos, tratamento de dados e qualquer código que não precise de controle absoluto da memória física.

```swift
// Modo VM (padrão) — sem nenhuma anotação especial
func soma(a: Int, b: Int) -> Int {
    return a + b;   // emite: ADD r0, r1, r2
}

func loop(n: Int) -> Void {
    var i: Int = 0;
    while i < n {   // emite: CMP_LT + JMP_IF_FALSE
        i = i + 1;  // emite: ADD + STORE_VAR
    }
}
```

**Características do Modo VM:**
- Variáveis alocadas em **slots numerados** (registradores virtuais) — sem endereço físico.
- *Null-safety* e verificações de tipos aplicadas automaticamente pela VM.
- Portável entre x86-64, ARM, RISC-V, AVR e qualquer alvo com a VM compilada.
- GC manual por escopo (RAII) — sem ponteiros expostos ao desenvolvedor.

---

<a id="codegen-42-modo-raw-programação-direta-na-memória-estilo-c"></a>

#### 4.2. Modo Raw — Programação Direta na Memória (Estilo C)

Para casos onde é necessário controle absoluto de desempenho, acesso a endereços físicos de hardware (registradores de periféricos, DMA, MMIO) ou manipulação de ponteiros arbitrários, o desenvolvedor pode optar pelo **Modo Raw**, anotado com o bloco `@raw` ou `unsafe { }`.

No Modo Raw, a ArgosLang se comporta como C: variáveis ocupam posições **físicas** de memória (pilha ou heap), ponteiros são endereços reais e não há camada de VM.

```swift
// Modo Raw — acesso direto à memória física (igual a C)
unsafe {
    @raw
    func writeRegister(address: USize, value: Byte) -> Void {
        // Escreve diretamente em um endereço de memória físico
        // (MMIO, GPIO register, DMA buffer, etc.)
        let ptr: *mut Byte = address as *mut Byte;
        *ptr = value;
    }

    @raw
    func readBuffer(base: USize, offset: USize) -> Byte {
        let ptr: *Byte = (base + offset) as *Byte;
        return *ptr;
    }
}

// Exemplo: controlar um pino GPIO do ESP32 via endereço MMIO
func setGpioHigh() -> Void {
    unsafe {
        writeRegister(0x3FF44004, 1);  // GPIO_OUT_W1TS no ESP32
    }
}
```

**Características do Modo Raw:**
- Variáveis mapeadas diretamente na **pilha do sistema operacional / hardware** (endereço real em bytes).
- Ponteiros tipados (`*T` = ponteiro imutável, `*mut T` = ponteiro mutável) com aritmética explícita.
- Sem verificações de *null-safety* em tempo de execução — **responsabilidade total do desenvolvedor**.
- Necessário estar dentro de `unsafe { }` para garantir rastreabilidade no código.
- Zero *overhead* de VM — código equivalente ao C compilado diretamente.

---

<a id="codegen-43-coexistência-dos-dois-modos-no-mesmo-programa"></a>

#### 4.3. Coexistência dos Dois Modos no Mesmo Programa

Os dois modos podem coexistir no mesmo arquivo `.argos`. A camada `unsafe { }` delimita explicitamente onde o controle passa do modo VM para o Modo Raw:

```swift
// Lógica de alto nível no Modo VM (seguro, portável)
func calcularChecksum(dados: [Byte], tamanho: USize) -> Byte {
    var soma: Byte = 0;
    var i: USize = 0;
    while i < tamanho {
        soma = soma + dados[i];
        i = i + 1;
    }
    return soma;
}

// Gravação do resultado diretamente em hardware via Modo Raw
func enviarParaUart(valor: Byte) -> Void {
    unsafe {
        @raw
        let udr0: *mut Byte = 0xC6 as *mut Byte;  // Registrador UDR0 do ATmega328P
        *udr0 = valor;
    }
}

func main() -> Void {
    let buffer: [Byte] = [0x01, 0x02, 0x03, 0x04];
    let checksum: Byte = calcularChecksum(buffer, 4);  // Modo VM
    enviarParaUart(checksum);                          // Modo Raw
}
```

---

<a id="codegen-44-tabela-comparativa-modo-vm-vs-modo-raw"></a>

#### 4.4. Tabela Comparativa: Modo VM vs. Modo Raw

| Característica | Modo VM (Padrão) | Modo Raw (`unsafe @raw`) |
| :--- | :---: | :---: |
| Endereçamento de memória | Slots virtuais | Endereços físicos reais |
| Aritmética de ponteiros | ❌ Não disponível | ✅ Disponível |
| Acesso a MMIO / Registradores | ❌ Indiretamente (via `c_function`) | ✅ Direto |
| Null-safety em runtime | ✅ Verificado pela VM | ❌ Responsabilidade do dev |
| Portabilidade | ✅ Qualquer alvo com VM | ⚠️ Depende da arquitetura |
| Performance | ⚠️ Overhead da VM (AOT elimina) | ✅ Máxima — zero overhead |
| Adequado para Arduino AVR | ✅ Sim (com AOT) | ✅ Sim (modo nativo) |
| Adequado para Scripts / REPL | ✅ Sim | ❌ Não |

---

<a id="codegen-5-conjunto-de-instruções-da-pseudo-máquina-ir-bytecode"></a>

### 5. Conjunto de Instruções da Pseudo-Máquina (IR / Bytecode)

A representação intermediária (IR) da ArgosLang é baseada em uma **Pseudo-Máquina de Registradores / Pilha** universal. Essas instruções podem ser executadas pela **VM Interpretada** ou convertidas diretamente em código C nativo pelo compilador AOT.

<a id="codegen-41-conjunto-principal-de-opcodes-instruções"></a>

#### 4.1. Conjunto Principal de Opcodes (Instruções)

| Opcode | Operandos | Descrição |
| :--- | :--- | :--- |
| `NOP` | - | Nenhuma operação |
| `LOAD_CONST` | `dst, const_id` | Carrega literal/constante no registrador `dst` |
| `LOAD_VAR` | `dst, slot_id` | Carrega valor da variável local para `dst` |
| `STORE_VAR` | `slot_id, src` | Armazena valor do registrador `src` na variável local |
| `ADD` | `dst, src1, src2` | Soma inteira/flutuante: `dst = src1 + src2` |
| `SUB` | `dst, src1, src2` | Subtração: `dst = src1 - src2` |
| `MUL` | `dst, src1, src2` | Multiplicação: `dst = src1 * src2` |
| `DIV` | `dst, src1, src2` | Divisão: `dst = src1 / src2` |
| `MOD` | `dst, src1, src2` | Módulo: `dst = src1 % src2` |
| `CMP_EQ` | `dst, src1, src2` | Comparação de igualdade: `dst = (src1 == src2)` |
| `CMP_LT` | `dst, src1, src2` | Comparação menor que: `dst = (src1 < src2)` |
| `JMP` | `target_label` | Desvio incondicional para `target_label` |
| `JMP_IF_FALSE`| `cond_reg, target_label` | Desvio se o registrador `cond_reg` for falso |
| `CALL` | `dst, func_id, arg_count` | Chama a função `func_id` repassando `arg_count` argumentos |
| `RET` | `src_reg` | Retorna da função com o valor de `src_reg` |
| `UNWRAP_CHECK`| `dst, src_reg` | Pânico/aborta se `src_reg` for `null`, senão copia valor |
| `C_EMBED` | `c_code_str` | Executa/emite trecho de código C embutido nativo |

---

<a id="codegen-5-pipeline-de-compilação-e-transpilação-c"></a>

### 5. Pipeline de Compilação e Transpilação C

Quando o compilador executa em modo **AOT Nativo**, a representação da Pseudo-Máquina é traduzida para C padrão (C99 / C11) e em seguida compilada pelo utilitário da plataforma (`gcc`, `clang`, `avr-gcc`, `xtensa-esp32-elf-gcc`):

```text
[AST ArgosLang] 
       ↓
[Pseudo-Máquina IR (Opcodes)]
       ↓
[Gerador C (C-Transpiler)] ──► Produz arquivo nativo.c
       ↓
[avr-gcc / xtensa-gcc / gcc]
       ↓
[Binário Executável / Hex Firmware (.bin / .hex)]
```

---

<a id="codegen-6-comandos-futuros-da-cli-argosc"></a>

### 6. Comandos Futuros da CLI `argosc`

| Comando | Descrição | Aplicação |
| :--- | :--- | :--- |
| `argosc run <file.argos>` | Executa o arquivo imediatamente via **Interpretador VM** | Scripts rápidos e testes sem compilar |
| `argosc emit-ir <file.argos>` | Imprime o código de instruções da **Pseudo-Máquina (IR)** | Depuração da geração de bytecode |
| `argosc compile <file.argos> -o <bin>` | Compila nativamente para **C / Binário AOT** | Firmwares para Arduino/ESP e binários nativos |

---

<a id="codegen-7-interoperabilidade-com-outras-linguagens"></a>

### 7. Interoperabilidade com Outras Linguagens

A ArgosLang oferece interoperabilidade com um conjunto amplo de linguagens, permitindo reaproveitar ecossistemas existentes sem abrir mão da segurança de tipos e do modelo Zero-GC.

> **Regra Geral de Nomeação:** Cada linguagem possui seu construto próprio de interop com o padrão `<lang>_function(...)`, `<lang>_block { ... }` ou anotações `@extern_<lang>`, garantindo rastreabilidade e clareza no código-fonte.

---

<a id="codegen-71-assembly-asm_block"></a>

#### 7.1. Assembly (`asm_block`)

Para manipulação direta de registradores, instruções especiais de CPU ou acesso a instruções de hardware sem nenhum nível de abstração.

```swift
// Lê o registrador de status da CPU (flags)
func readStatusRegister() -> Byte {
    var result: Byte = 0;
    asm_block("""
        in %[out], __SREG__
        : [out] "=r" (result)
        :
        : "memory"
    """);
    return result;
}

// Habilita interrupções globais no AVR
func enableInterrupts() -> Void {
    asm_block("sei");
}
```

**Regras:**
- O bloco `asm_block` emite código AT&T ou Intel Syntax dependendo da plataforma alvo.
- Variáveis ArgosLang podem ser referenciadas dentro do bloco usando a sintaxe de *constraint* do GCC Inline Assembly (`[nome] "=r" (var)`).
- Somente permitido dentro de blocos `unsafe { }`.

---

<a id="codegen-72-c-c_function-nativo-já-documentado"></a>

#### 7.2. C (`c_function`) *(Nativo — já documentado)*

Ver **Seção 2** para a documentação completa. Resumo:

```swift
func delay(ms: USize) -> Void {
    c_function("""
        #include <util/delay.h>
        _delay_ms(ms);
    """);
}
```

---

<a id="codegen-73-c-cpp_function"></a>

#### 7.3. C++ (`cpp_function`)

Permite chamar código C++ moderno (RAII, templates, STL, bibliotecas como Eigen, ArduinoJson etc.) sem precisar reescrever bindings manuais.

```swift
// Interop com STL: conversão de string C++ para ArgosLang
func intToString(value: Int) -> String {
    var result: String = "";
    cpp_function("""
        #include <string>
        std::string cpp_str = std::to_string(value);
        result = cpp_str.c_str();
    """);
    return result;
}

// Interop com ArduinoJson
func parseJson(raw: String) -> Void {
    cpp_function("""
        #include <ArduinoJson.h>
        StaticJsonDocument<256> doc;
        deserializeJson(doc, raw);
        int id = doc["id"];
    """);
}
```

**Regras:**
- O bloco `cpp_function` é compilado com `g++` / `clang++` (mínimo C++17).
- O *name mangling* C++ é automaticamente tratado pelo compilador ArgosLang via blocos `extern "C"` gerados.
- O estado dos objetos C++ é de **responsabilidade do desenvolvedor** (Zero-GC — nenhum destrutor é invocado automaticamente ao sair do bloco ArgosLang).

---

<a id="codegen-74-rust-rust_function-e-extern_rust"></a>

#### 7.4. Rust (`rust_function` e `@extern_rust`)

Permite chamar código Rust através da sua **FFI estável** (`extern "C"`). A ArgosLang gera o cabeçalho C de binding automaticamente ou usa anotações `@extern_rust` para importar símbolos.

```swift
// Declara uma função Rust exportada como FFI (extern "C")
@extern_rust("libminha_lib.rlib")
func hashSha256(input: Byte, len: USize) -> USize;

// Uso normal após declaração
func computeHash(data: Byte) -> USize {
    return hashSha256(data, 32);
}
```

**No lado Rust (`src/lib.rs`):**
```rust
#[no_mangle]
pub extern "C" fn hash_sha256(input: *const u8, len: usize) -> usize {
    // Implementação...
    0
}
```

**Regras:**
- Todo binding Rust **deve** ser anotado com `#[no_mangle]` e `extern "C"` no lado Rust.
- Tipos mapeados: `Byte` ↔ `u8`, `Int` ↔ `i32`, `Long` ↔ `i64`, `USize` ↔ `usize`, `Bool` ↔ `bool`.
- O compilador ArgosLang **não** gerencia o ownership do Rust. Dados devem ser passados como ponteiros `USize` quando o ciclo de vida precisa ser controlado pelo Rust.

---

<a id="codegen-75-go-go_function-e-extern_go"></a>

#### 7.5. Go (`go_function` e `@extern_go`)

A ArgosLang se integra com Go via **CGo** e exportação de símbolos com `//export`. Adequado para usar bibliotecas de rede, HTTP ou utilitários Go dentro de um host ArgosLang interpretado.

```swift
// Declara uma função Go exportada via CGo
@extern_go("libmeugo.so")
func httpGet(url: String) -> String;

// Uso
func fetchData() -> Void {
    let response: String = httpGet("http://api.exemplo.com/data");
}
```

**No lado Go (`main.go`):**
```go
package main

import "C"
import "net/http"

//export httpGet
func httpGet(url *C.char) *C.char {
    // Implementação com net/http...
    return C.CString("resposta")
}

func main() {}
```

**Regras:**
- A biblioteca Go deve ser compilada como `shared library` com `go build -buildmode=c-shared`.
- Strings ArgosLang (`String`) são convertidas para `*C.char` automaticamente pelo runtime de interop.
- O Go tem seu próprio GC; objetos Go **não devem** ser armazenados em structs ArgosLang além do tempo de vida da chamada.

---

<a id="codegen-76-python-py_function-e-python_module"></a>

#### 7.6. Python (`py_function` e `@python_module`)

Permite embutir código Python diretamente via **CPython Embedding** (`libpython3`) ou PyPy, útil para scripts de configuração, machine learning (NumPy, TensorFlow) e prototipagem rápida.

```swift
// Bloco de código Python embutido (roda via CPython)
func runPythonScript() -> Void {
    py_function("""
        import json
        data = {"key": "value", "count": 42}
        print(json.dumps(data, indent=2))
    """);
}

// Importa um módulo Python e chama uma função específica
@python_module("numpy")
func numpyMean(values: [Double], len: USize) -> Double;

// Uso
func computeAverage() -> Double {
    return numpyMean([1.0, 2.0, 3.0, 4.0], 4);
}
```

**Regras:**
- O modo `py_function` requer `libpython3.x` linkado e variável `PYTHONPATH` configurada no ambiente.
- Não disponível em alvos bare-metal (Arduino / ESP) por limitação de recursos — apenas em modo interpretado ou em hosts com SO.
- Tipos Python são convertidos automaticamente: `int` ↔ `Int`, `float` ↔ `Double`, `str` ↔ `String`, `bool` ↔ `Bool`.
- Erros Python (`PyException`) são capturados e convertidos em diagnósticos ArgosLang em tempo de execução.

---

<a id="codegen-77-swift-swift_function-e-swift_module"></a>

#### 7.7. Swift (`swift_function` e `@swift_module`)

Como a ArgosLang é fortemente inspirada em Swift, a interoperabilidade com código Swift é uma funcionalidade de primeira classe. Utiliza a **Swift C Interop Layer** e frameworks Swift nativos via `swiftc`.

```swift
// Importa um módulo Swift compilado como framework/dylib
@swift_module("MeuFrameworkSwift")
func formatDate(timestamp: Long) -> String;

// Embutir código Swift diretamente (modo interpretado ou compilado para Darwin/Linux)
func greet(name: String) -> String {
    var result: String = "";
    swift_function("""
        import Foundation
        let greeting = "Olá, \\(name)! Hora atual: \\(Date())"
        result = greeting
    """);
    return result;
}
```

**No lado Swift (para `@swift_module`):**
```swift
// MeuFramework.swift
@_cdecl("formatDate")
public func formatDate(timestamp: Int64) -> UnsafePointer<CChar> {
    let date = Date(timeIntervalSince1970: Double(timestamp))
    return NSString(string: "\(date)").utf8String!
}
```

**Regras:**
- Em macOS/iOS: integração direta via `.framework` e Apple SDKs.
- Em Linux: requer `swift` toolchain instalado; bibliotecas exportadas como `@_cdecl` para garantir compatibilidade com ABI C.
- Strings Swift (`String`) são convertidas para/de `UnsafePointer<CChar>` / `String` ArgosLang automaticamente.
- Tipos opcionais Swift (`T?`) mapeiam para os tipos opcionais ArgosLang (`T?`), preservando a semântica de null-safety.

---

<a id="codegen-8-tabela-resumo-de-interoperabilidade"></a>

### 8. Tabela Resumo de Interoperabilidade

| Linguagem | Construto ArgosLang | Mecanismo Subjacente | Bare-Metal? |
| :--- | :--- | :--- | :---: |
| **Assembly** | `asm_block` | GCC/Clang Inline ASM | ✅ Sim |
| **C** | `c_function` | Geração de código C direto | ✅ Sim |
| **C++** | `cpp_function` | Compilação C++17, `extern "C"` automático | ✅ Sim¹ |
| **Rust** | `@extern_rust` | FFI `extern "C"`, `#[no_mangle]` | ⚠️ Parcial² |
| **Go** | `@extern_go` | CGo `//export`, `c-shared` lib | ❌ Não |
| **Python** | `py_function`, `@python_module` | CPython Embedding (`libpython3`) | ❌ Não |
| **Swift** | `swift_function`, `@swift_module` | `@_cdecl`, ABI C, Apple/Linux SDKs | ❌ Não |

> ¹ C++ requer STL e RTTI desabilitados para alvos bare-metal de baixa memória (AVR, ESP8266).
> ² Rust pode rodar bare-metal via `no_std`, mas o binding ArgosLang requer coordenação manual do ciclo de vida.


---

<a id="testes"></a>

<a id="testes-testar-a-argoslang-e-o-compilador"></a>

## Testar a ArgosLang e o compilador

Este guia descreve o pipeline que existe hoje. A sintaxe canônica usa
`function`, `var`/`var mut`, `nil` e tipos primitivos em minúsculas. O escopo
executável está detalhado em [ArgosLang MVP](#mvp).

<a id="testes-1-construir-e-testar"></a>

### 1. Construir e testar

Na raiz do repositório, configure o projeto, compile o compilador e execute a
suíte:

```bash
cmake -S . -B build -DCMAKE_CXX_COMPILER=/usr/bin/g++
cmake --build build
ctest --test-dir build --output-on-failure
```

`argosc` será criado em `build/bin/argosc`. A primeira configuração com testes
pode baixar Catch2.

<a id="testes-2-inspecionar-um-programa"></a>

### 2. Inspecionar um programa

Os comandos `lex`, `parse`, `ast` e `check` mostram as etapas do frontend:

```bash
./build/bin/argosc lex examples/minimal_arithmetic.argos
./build/bin/argosc parse examples/minimal_arithmetic.argos
./build/bin/argosc ast examples/minimal_arithmetic.argos
./build/bin/argosc check examples/minimal_arithmetic.argos
```

`check` confirma sintaxe e semântica; não executa o programa. Quando um erro
ocorre, a saída informa arquivo, linha, coluna, explicação em linguagem simples
e uma sugestão para corrigir.

<a id="testes-3-executar-e-compilar"></a>

### 3. Executar e compilar

O mesmo exemplo pode ser executado na VM e compilado para x86-64/Linux:

```bash
./build/bin/argosc run examples/minimal_print_int.argos
./build/bin/argosc compile examples/minimal_print_int.argos -o /tmp/argos-print
/tmp/argos-print
```

As duas rotas imprimem `42`. `run` também mostra `Resultado da VM: 0`; no
executável nativo, o retorno de `main()` aparece no status do processo. Esse
status preserva somente os oito bits inferiores em Unix.

Um exemplo mínimo usa a forma canônica:

```argos
function main() -> int {
    var answer: int = 6 * 7
    print(answer)
    return 0
}
```

`var` é imutável por padrão. Use `var mut` quando o valor precisar mudar.
Chamadas, laços, `break` e `continue` funcionam nas duas rotas para o
subconjunto descrito em [ArgosLang MVP](#mvp).

<a id="testes-4-estado-atual-das-fases"></a>

### 4. Estado atual das fases

- O parser em execução é manual, com precedência de operadores; LR(1) é uma
  direção futura documentada, não o parser atual.
- A semântica alimenta uma IR com tipos explícitos e validação compartilhada
  pela VM e pelo backend nativo.
- A VM executa essa IR diretamente. `BytecodeEmitter` ainda imprime texto de
  inspeção; não existe formato de bytecode independente.
- O backend disponível emite assembly x86-64 e usa `cc` para montar e ligar.
- O REPL usa o mesmo frontend, lowering e VM, e preserva variáveis locais
  `int`/`bool` entre entradas.
- Strings armazenadas, opcionais, arrays homogêneos, classes concretas e
  closures já executam na VM e no nativo. Herança, interfaces, `super`,
  overload/override e `data` também executam. Backends ARM64/RISC-V/AVR e
  cross-compilation continuam pendentes.
  A matriz completa está no [MVP](#mvp).

Uma construção aceita por `check` ainda pode não ser executável. Nesses casos,
o compilador informa o diagnóstico `ARGOS-C0001`, em vez de produzir um
executável incompleto.


---

<a id="parser"></a>

<a id="parser-diagnóstico-de-erros-do-parser-e-solução-proposta"></a>

## Diagnóstico de Erros do Parser e Solução Proposta

Este documento contém o diagnóstico detalhado de todos os erros de compilação, incompatibilidades de tipos e falhas de lógica encontrados no módulo do Parser da **ArgosLang**, juntamente com a proposta completa de correção.

---

<a id="parser-1-resumo-executivo"></a>

### 1. Resumo Executivo

Após a análise estática do código-fonte e a tentativa de compilação do projeto via CMake/Ninja (`cmake -B build -S . && cmake --build build`), foram identificados **7 problemas principais** que impedem a compilação e o funcionamento correto do parser.

Os problemas variam desde desencontros de declaração de *namespaces* e convenção de nomes de identificadores até a ausência de operadores essenciais na gramática Pratt e exclusão de arquivos no sistema de *build*.

---

<a id="parser-2-diagnóstico-detalhado-dos-erros"></a>

### 2. Diagnóstico Detalhado dos Erros

<a id="parser-21-incompatibilidade-de-namespaces-em-token_kindhpp"></a>

#### 2.1. Incompatibilidade de Namespaces em `token_kind.hpp`
- **Arquivo:** `compiler/include/argos/lexer/token_kind.hpp`
- **Descrição:** O arquivo abre o namespace com `namespace argos {` na linha 9, mas fecha com `} // namespace argos::lexer` na linha 214.
- **Impacto:** O enum `TokenKind` foi declarado em `argos::TokenKind`, enquanto todo o restante da base de código (`lexer.hpp`, `token.hpp`, `parser.hpp`, `token_kind.cpp`, `lexer_tests.cpp`) espera `argos::lexer::TokenKind`. Isso resulta em erros de compilação como `no member named 'TokenKind' in namespace 'argos::lexer'`.

<a id="parser-22-inconsistência-de-nomenclatura-dos-membros-do-enum-tokenkind"></a>

#### 2.2. Inconsistência de Nomenclatura dos Membros do Enum `TokenKind`
- **Arquivos:** `compiler/include/argos/lexer/token_kind.hpp`, `compiler/src/lexer/token_kind.cpp`, `compiler/src/parser/parser.cpp`, `tests/lexer/lexer_tests.cpp`
- **Descrição:** O enum `TokenKind` foi definido em `token_kind.hpp` usando **PascalCase** (`EndOfFile`, `KwLet`, `KwVar`, `KwFunction`, `KwReturn`, `KwIf`, `KwElse`, `LeftParen`, `RightParen`, `LeftBrace`, `RightBrace`, `Comma`, `Colon`, `Semicolon`, `Dot`, `QuestionDot`, `SafeAssign`, `NullCoalescing`, `ForceUnwrap`, `SafeEqualityAssign`, etc.).
- No entanto, o `parser.cpp`, o `token_kind.cpp` e o `lexer_tests.cpp` utilizaram nomes em **lower_snake_case** inexistentes (`end_of_file`, `keyword_let`, `keyword_var`, `keyword_func`, `keyword_return`, `keyword_if`, `keyword_else`, `keyword_null`, `keyword_true`, `keyword_false`, `left_parenthesis`, `right_parenthesis`, `left_brace`, `right_brace`, `comma`, `colon`, `semicolon`, `dot`, `safe_dot`, `coalesce`, `coalesce_assign`, `conditional_assign`, `double_bang`, `lifted_equal`, etc.).
- **Impacto:** Múltiplos erros de compilação por membro inexistente no enum (ex.: `error: no member named 'keyword_let' in 'argos::TokenKind'`).

<a id="parser-23-redundância-e-conflito-de-estruturas-auxiliares"></a>

#### 2.3. Redundância e Conflito de Estruturas Auxiliares
- **Arquivos:** `compiler/include/argos/lexer/token_kind.hpp` e `compiler/include/argos/lexer/token.hpp`
- **Descrição:** O arquivo `token_kind.hpp` redefiniu `SourceLocation`, `SourceRange` e `Token` de forma duplicada e incompatível com `argos::source::SourceSpan` e `argos::lexer::Token` de `token.hpp`.
- **Impacto:** Ambiguidade e inconsistência entre os tipos de *spans* de código fonte utilizados pelo Parser e Lexer.

<a id="parser-24-incompatibilidade-nos-enums-do-ast-unaryoperator"></a>

#### 2.4. Incompatibilidade nos Enums do AST (`UnaryOperator`)
- **Arquivos:** `compiler/include/argos/ast/ast.hpp` e `compiler/src/parser/parser.cpp`
- **Descrição:** Em `ast.hpp`, o enum `UnaryOperator` define:
  ```cpp
  enum class UnaryOperator : std::uint8_t {
      plus,
      minus,
      not_,
  };
  ```
  No arquivo `parser.cpp`, o método `Parser::unary_operator` tentava retornar identificadores inexistentes: `ast::UnaryOperator::positive`, `ast::UnaryOperator::negative` e `ast::UnaryOperator::logical_not`.
- **Impacto:** Erro de compilação ao tentar instanciar expressões unárias.

<a id="parser-25-operadores-ausentes-na-tabela-de-precedência-pratt-parsing"></a>

#### 2.5. Operadores Ausentes na Tabela de Precedência (Pratt Parsing)
- **Arquivo:** `compiler/src/parser/parser.cpp`
- **Descrição:** Os métodos `Parser::infix_binding_power` e `Parser::binary_operator` não implementaram suporte aos seguintes operadores essenciais presentes em `TokenKind` e `ast::BinaryOperator`:
  - Operadores Relacionais: `<` (`Less`), `<=` (`LessEqual`), `>` (`Greater`), `>=` (`GreaterEqual`)
  - Operadores Lógicos: `&&` (`AmpAmp`), `||` (`PipePipe`)
  - Operador Resto da Divisão: `%` (`Percent`)
- **Impacto:** Expressões contendo comparações, operações lógicas ou módulo falhavam no parsing silenciosamente ou causavam erro de sintaxe.

<a id="parser-26-falha-na-sincronização-e-recuperação-de-erros-de-sintaxe"></a>

#### 2.6. Falha na Sincronização e Recuperação de Erros de Sintaxe
- **Arquivo:** `compiler/src/parser/parser.cpp`
- **Descrição:** Nos loops de `parse_program()` e `parse_block_statement()`, ao encontrar um erro, o parser apenas invocava `advance()`.
- **Impacto:** Ausência de uma rotina de sincronização (`synchronize()`) para descartar tokens até o próximo delimitador de instrução (ex.: `;`, `}`, `let`, `var`, `func`, `if`), gerando uma cascata de diagnósticos redundantes para um único erro.

<a id="parser-27-omissão-do-parser-no-sistema-de-build-cmakeliststxt"></a>

#### 2.7. Omissão do Parser no Sistema de Build (`CMakeLists.txt`)
- **Arquivos:** `compiler/CMakeLists.txt` e `tests/CMakeLists.txt`
- **Descrição:** O arquivo `compiler/src/parser/parser.cpp` não está cadastrado no alvo `argos_frontend` da biblioteca CMake. Adicionalmente, não foram incluídos testes unitários do Parser em `tests/CMakeLists.txt`.
- **Impacto:** O arquivo `parser.cpp` sequer era compilado pela biblioteca do compilador.

---

<a id="parser-3-solução-proposta"></a>

### 3. Solução Proposta

A solução consiste em 6 etapas principais descritas abaixo.

<a id="parser-31-correção-do-header-token_kindhpp"></a>

#### 3.1. Correção do Header `token_kind.hpp`
Ajustar o namespace para `argos::lexer` e reutilizar os tipos de `argos::source`.

```cpp
#pragma once

#include "argos/source/source_location.hpp"
#include "argos/source/source_span.hpp"

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace argos::lexer {

enum class TokenKind : std::uint16_t {
    // Controle
    EndOfFile,
    Error,
    NewLine,

    // Identificadores e literais
    Identifier,
    EscapedIdentifier,
    IntegerLiteral,
    FloatLiteral,
    StringLiteral,
    MultilineStringLiteral,
    CharacterLiteral,
    Invalid,

    // Palavras-chave: declarações
    KwVar,
    KwLet,
    KwConst,
    KwMut,
    KwClass,
    KwStruct,
    KwInterface,
    KwEnum,
    KwRecord,
    KwFunction,
    KwOperator,
    KwImport,
    KwPackage,
    KwNamespace,
    KwUsing,

    // Palavras-chave: modificadores
    KwPublic,
    KwPrivate,
    KwProtected,
    KwInternal,
    KwStatic,
    KwFinal,
    KwSealed,
    KwAbstract,
    KwOverride,
    KwVirtual,
    KwUnsafe,
    KwAsync,
    KwAwait,

    // Palavras-chave: fluxo de controle
    KwIf,
    KwElse,
    KwWhile,
    KwWhen,
    KwFor,
    KwIn,
    KwSwitch,
    KwCase,
    KwDefault,
    KwBreak,
    KwContinue,
    KwReturn,
    KwTry,
    KwCatch,
    KwFinally,
    KwThrow,
    KwThrows,

    // Palavras-chave: expressões e valores
    KwNew,
    KwThis,
    KwSuper,
    KwAs,
    KwIs,
    KwTrue,
    KwFalse,
    KwNull,

    // Tipos primitivos
    KwVoid,
    KwBool,
    KwByte,
    KwShort,
    KwInt,
    KwLong,
    KwFloat,
    KwDouble,
    KwChar,
    KwString,
    KwUSize,
    KwISize,

    // Delimitadores e pontuação
    LeftParen,       // (
    RightParen,      // )
    LeftBrace,       // {
    RightBrace,      // }
    LeftBracket,     // [
    RightBracket,    // ]
    Comma,           // ,
    Colon,           // :
    Semicolon,       // ;
    Dot,             // .
    Range,           // ..
    Ellipsis,        // ...
    At,              // @
    Hash,            // #
    Arrow,           // ->
    FatArrow,        // =>

    // Operadores aritméticos
    Plus,            // +
    Minus,           // -
    Star,            // *
    Slash,           // /
    Percent,         // %
    PlusPlus,        // ++
    MinusMinus,      // --

    // Atribuição
    Equal,           // =
    PlusEqual,       // +=
    MinusEqual,      // -=
    StarEqual,       // *=
    SlashEqual,      // /=
    PercentEqual,    // %=

    // Comparação
    EqualEqual,      // ==
    BangEqual,       // !=
    StrictEqual,     // ===
    StrictBangEqual, // !==
    Less,            // <
    LessEqual,       // <=
    Greater,         // >
    GreaterEqual,    // >=
    Spaceship,       // <=>

    // Lógicos e bit a bit
    Bang,            // !
    Ampersand,       // &
    Pipe,            // |
    Caret,           // ^
    Tilde,           // ~
    AmpAmp,          // &&
    PipePipe,        // ||
    AmpEqual,        // &=
    PipeEqual,       // |=
    CaretEqual,      // ^=
    ShiftLeft,       // <<
    ShiftRight,      // >>
    ShiftLeftEqual,  // <<=
    ShiftRightEqual, // >>=

    // Optional / Null Safety do ArgosLang
    Question,             // T? ou expressão?
    QuestionDot,          // ?.
    QuestionLeftBracket,  // ?[
    SafeAssign,           // ?=
    SafeEqualityAssign,   // ?==
    NullCoalescing,       // ??
    NullCoalescingAssign, // ??=
    ForceUnwrap,          // !!
    ForceUnwrapAssign,    // !!=
};

std::string_view token_kind_name(TokenKind kind) noexcept;

} // namespace argos::lexer
```

---

<a id="parser-32-atualização-do-mapeamento-dos-nomes-dos-tokens-token_kindcpp"></a>

#### 3.2. Atualização do Mapeamento dos Nomes dos Tokens (`token_kind.cpp`)
Atualizar o `switch` em `token_kind.cpp` para corresponder aos identificadores em PascalCase de `TokenKind`.

---

<a id="parser-33-refatoração-e-mapeamento-de-tokens-no-parsercpp"></a>

#### 3.3. Refatoração e Mapeamento de Tokens no `parser.cpp`
Atualizar todas as referências no `parser.cpp` para usar os nomes corretos do `TokenKind` e os enumeradores do `ast`:

1. **Substituição de Nomes de Tokens:**
   - `lexer::TokenKind::end_of_file` $\rightarrow$ `lexer::TokenKind::EndOfFile`
   - `lexer::TokenKind::keyword_let` $\rightarrow$ `lexer::TokenKind::KwLet`
   - `lexer::TokenKind::keyword_var` $\rightarrow$ `lexer::TokenKind::KwVar`
   - `lexer::TokenKind::keyword_func` $\rightarrow$ `lexer::TokenKind::KwFunction`
   - `lexer::TokenKind::keyword_return` $\rightarrow$ `lexer::TokenKind::KwReturn`
   - `lexer::TokenKind::keyword_if` $\rightarrow$ `lexer::TokenKind::KwIf`
   - `lexer::TokenKind::keyword_else` $\rightarrow$ `lexer::TokenKind::KwElse`
   - `lexer::TokenKind::keyword_null` $\rightarrow$ `lexer::TokenKind::KwNull`
   - `lexer::TokenKind::keyword_true` $\rightarrow$ `lexer::TokenKind::KwTrue`
   - `lexer::TokenKind::keyword_false` $\rightarrow$ `lexer::TokenKind::KwFalse`
   - `lexer::TokenKind::identifier` $\rightarrow$ `lexer::TokenKind::Identifier`
   - `lexer::TokenKind::integer_literal` $\rightarrow$ `lexer::TokenKind::IntegerLiteral`
   - `lexer::TokenKind::string_literal` $\rightarrow$ `lexer::TokenKind::StringLiteral`
   - `lexer::TokenKind::left_parenthesis` / `right_parenthesis` $\rightarrow$ `lexer::TokenKind::LeftParen` / `RightParen`
   - `lexer::TokenKind::left_brace` / `right_brace` $\rightarrow$ `lexer::TokenKind::LeftBrace` / `RightBrace`
   - `lexer::TokenKind::colon`, `semicolon`, `comma`, `dot`, `arrow` $\rightarrow$ `Colon`, `Semicolon`, `Comma`, `Dot`, `Arrow`
   - `lexer::TokenKind::safe_dot` $\rightarrow$ `QuestionDot`
   - `lexer::TokenKind::coalesce` $\rightarrow$ `NullCoalescing`
   - `lexer::TokenKind::coalesce_assign` $\rightarrow$ `NullCoalescingAssign`
   - `lexer::TokenKind::conditional_assign` $\rightarrow$ `SafeAssign`
   - `lexer::TokenKind::double_bang` $\rightarrow$ `ForceUnwrap`
   - `lexer::TokenKind::lifted_equal` $\rightarrow$ `SafeEqualityAssign`

2. **Mapeamento de Operadores Unários (`unary_operator`):**
   ```cpp
   ast::UnaryOperator Parser::unary_operator(lexer::TokenKind kind) noexcept {
       switch (kind) {
           case lexer::TokenKind::Plus: return ast::UnaryOperator::plus;
           case lexer::TokenKind::Minus: return ast::UnaryOperator::minus;
           case lexer::TokenKind::Bang: return ast::UnaryOperator::not_;
           default:
               assert(false && "token não representa operador unário");
               return ast::UnaryOperator::plus;
       }
   }
   ```

3. **Inclusão de Operadores Relacionais, Lógicos e Resto na Tabela Pratt (`infix_binding_power` e `binary_operator`):**
   ```cpp
   std::optional<Parser::BindingPower> Parser::infix_binding_power(lexer::TokenKind kind) noexcept {
       switch (kind) {
           case lexer::TokenKind::Equal:
           case lexer::TokenKind::SafeAssign:
           case lexer::TokenKind::NullCoalescingAssign:
               return BindingPower{10, 10};

           case lexer::TokenKind::PipePipe:
               return BindingPower{15, 16};

           case lexer::TokenKind::AmpAmp:
               return BindingPower{18, 19};

           case lexer::TokenKind::NullCoalescing:
               return BindingPower{20, 21};

           case lexer::TokenKind::EqualEqual:
           case lexer::TokenKind::BangEqual:
           case lexer::TokenKind::SafeEqualityAssign:
               return BindingPower{30, 31};

           case lexer::TokenKind::Less:
           case lexer::TokenKind::LessEqual:
           case lexer::TokenKind::Greater:
           case lexer::TokenKind::GreaterEqual:
               return BindingPower{35, 36};

           case lexer::TokenKind::Plus:
           case lexer::TokenKind::Minus:
               return BindingPower{40, 41};

           case lexer::TokenKind::Star:
           case lexer::TokenKind::Slash:
           case lexer::TokenKind::Percent:
               return BindingPower{50, 51};

           default:
               return std::nullopt;
       }
   }
   ```

4. **Adição do Método de Sincronização (`synchronize`):**
   ```cpp
   void Parser::synchronize() {
       advance();
       while (!is_at_end()) {
           if (previous().kind == lexer::TokenKind::Semicolon) return;

           switch (current().kind) {
               case lexer::TokenKind::KwLet:
               case lexer::TokenKind::KwVar:
               case lexer::TokenKind::KwFunction:
               case lexer::TokenKind::KwIf:
               case lexer::TokenKind::KwReturn:
               case lexer::TokenKind::RightBrace:
                   return;
               default:
                   advance();
           }
       }
   }
   ```

---

<a id="parser-34-atualização-dos-alvos-do-cmake-compilercmakeliststxt"></a>

#### 3.4. Atualização dos Alvos do CMake (`compiler/CMakeLists.txt`)

Incluir `src/parser/parser.cpp` no alvo da biblioteca `argos_frontend`:

```cmake
add_library(argos_frontend STATIC
    src/diagnostics/diagnostic_engine.cpp
    src/lexer/lexer.cpp
    src/lexer/token_kind.cpp
    src/parser/parser.cpp
    src/source/source_manager.cpp
)
```

---

<a id="parser-35-adição-da-suíte-de-testes-do-parser-testsparserparser_testscpp"></a>

#### 3.5. Adição da Suíte de Testes do Parser (`tests/parser/parser_tests.cpp`)

Criar o arquivo `tests/parser/parser_tests.cpp` para validar as construções da linguagem:
- Declaração de variáveis (`let x: Int = 10;`, `var y: String?;`)
- Declaração de funções (`func soma(a: Int, b: Int) -> Int { return a + b; }`)
- Expressões condicionais (`if`, `else`)
- Precedência de operadores aritméticos e de *null safety* (`?.`, `??`, `!!`, `?=`)

E atualizar `tests/CMakeLists.txt`:
```cmake
add_executable(argos_tests
    lexer/lexer_tests.cpp
    parser/parser_tests.cpp
)
```

---

<a id="parser-4-passos-para-validação"></a>

### 4. Passos para Validação

Após a aplicação das correções acima:

1. **Configuração e Compilação:**
   ```bash
   cmake -B build -S .
   cmake --build build
   ```
2. **Execução dos Testes Unitários:**
   ```bash
   ctest --test-dir build --output-on-failure
   ```
3. **Resultado Esperado:**
   - Compilação limpa sem erros nem *warnings*.
   - Execução e aprovação de 100% dos testes do Lexer e Parser via Catch2.


---

<a id="adr-0001"></a>

<a id="adr-0001-adr-0001-use-a-modular-monolith"></a>

## ADR-0001: Use a modular monolith

- Status: Accepted
- Date: 2026-07-27

<a id="adr-0001-context"></a>

### Context

A compiler executes a tightly coupled, mostly sequential transformation pipeline.
Network boundaries would add serialization, latency and operational failure modes
without creating useful independence.

<a id="adr-0001-decision"></a>

### Decision

Implement ArgosLang as a single process composed of modules with explicit
compile-time dependency direction.

<a id="adr-0001-consequences"></a>

### Consequences

- Fast in-memory phase communication.
- Simple local debugging.
- One deployment artifact for the compiler driver.
- Module boundaries must be protected through CMake targets and public headers.


---

<a id="adr-0002"></a>

<a id="adr-0002-adr-0002-treat-llvm-as-a-backend-adapter"></a>

## ADR-0002: Treat LLVM as a backend adapter

- Status: Accepted
- Date: 2026-07-27

<a id="adr-0002-context"></a>

### Context

The language frontend must remain independent from a particular machine-code
technology. Direct LLVM types in the parser or semantic analyzer would create
architectural coupling.

<a id="adr-0002-decision"></a>

### Decision

The frontend will lower source code to Argos-owned intermediate representations.
LLVM will later implement a backend port that consumes the lower IR.

<a id="adr-0002-consequences"></a>

### Consequences

- A native x86-64 backend can coexist with LLVM.
- Frontend tests do not need LLVM.
- Language semantics remain owned by ArgosLang.
- Lowering contracts must be carefully specified.

## Atualização implementada: pacotes, composição e records

O [guia de classes](guide/10-classes-e-interfaces.md) é a referência operacional
para `package`, `default`, tipos aninhados, composição múltipla `with`, construção
exclusiva e geração de hash/texto de `data`. A sintaxe permanece `function`, `:`
para superclasse, `by` para interfaces e `permited`; `extends`/`permits` não foram
introduzidos. `argosc check/run/compile` aceita vários arquivos explícitos,
preservando os pacotes e a origem dos diagnósticos. `uint_8` representa 0..255
e possui checagem de faixa. A arena continua tendo duração de execução; o vínculo
exclusivo dos componentes não equivale a um sistema geral de owner/borrow.

### Entregas desta etapa

- Visibilidade pública implícita para tipos e funções de topo, métodos de classe
  privados por padrão e requisitos de interface públicos por padrão.
- `default class` por pacote declarado, tipos aninhados e rejeição de classes
  privadas/protegidas no nível superior, incluindo acesso aos contenedores.
- Herança simples com `:`, contratos com `by`, despacho polimórfico,
  `overload`, `override`, `super` e validação de classes abstratas.
- Hierarquias `sealed` com lista explícita e não vazia de herdeiros diretos
  existentes e acessíveis em `permited`.
- Composição múltipla `with`: instâncias independentes, encaminhamento de métodos
  públicos com o receptor correto, resolução explícita de conflitos e construção
  exclusiva por construtor padrão acessível ou atribuição de uma instância nova.
- `data` final e transitivamente imutável, componentes privados e permissão exata
  de leitura por tipo nominal, sem transferência para subclasses ou serializadores.
  Componentes no corpo geram construtor canônico. Igualdade estrutural, hash e
  texto são automáticos; os métodos gerados cumprem interfaces compatíveis.
- Diagnósticos `ARGOS-S0626` a `ARGOS-S0632` para composição, inicialização, acesso
  e nomes reservados, preservando arquivo e trecho em programas com vários arquivos.

A validação final passou em **239 de 239 testes com GCC e com Clang**, com
execução pela VM e pelo nativo x86-64/Linux. O `build/` local foi reconstruído.
Testes adicionais confirmaram métodos gerados de `data` via interface e erro de
faixa de `uint_8` nos dois backends. Os links e a navegação da documentação foram
verificados. Exemplos completos: `examples/minimal_composition.argos`,
`examples/minimal_records.argos` e `examples/packages/`.

### Trabalho ainda pendente

O vínculo exclusivo de `with` usa a arena existente, liberada ao fim da execução
(VM) ou do processo (`atexit` no nativo). Ainda faltam ownership/borrowing gerais,
lifetimes, destruição por escopo e `deinit`. Também faltam modos de alocação manual
com ponteiros e sua exclusão estática em relação ao modo de ownership; esta etapa
não introduziu esses modos. `with &T` e `with shared T` não estão disponíveis.

Outros limites atuais: métodos ligados como valores, `static`, genéricos, captura
mutável, `copy`/`componentN` e serialização automática de `data`. Não há busca
automática de imports: os arquivos do programa são informados ao driver.
