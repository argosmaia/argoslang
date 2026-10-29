# Briefing arquitetural da ArgosLang

> Documento conceitual e normativo consolidado — 14 de setembro de 2026  
> Este briefing descreve a visão, as decisões e as instruções para evolução da linguagem. Ele não afirma que qualquer recurso já esteja implementado e não contém código, pseudocódigo ou comandos de compilação.

## 1. Propósito do documento

Este documento consolida a direção arquitetural da ArgosLang e deve orientar a gramática, a AST, a análise semântica, a representação intermediária, a máquina virtual, os backends nativos e a interoperabilidade de baixo nível.

As regras aqui descritas são especificações de projeto. Uma construção sintática documentada somente deve ser anunciada como disponível depois de atravessar, com testes, todas as fases necessárias: análise léxica, parsing, validação semântica, lowering e, quando aplicável, execução em ao menos um backend.

## 2. Visão da linguagem

A ArgosLang será uma linguagem fortemente e estaticamente tipada, compilada e interpretável, com sintaxe moderna inspirada em Swift, null-safety, mutabilidade explícita e capacidade de operar desde aplicações convencionais até sistemas embarcados e bare metal.

Seus objetivos centrais são:

- oferecer uma experiência segura e expressiva no modo normal;
- manter desempenho e uso de memória previsíveis;
- permitir execução rápida por VM durante desenvolvimento, scripts e REPL;
- permitir compilação AOT portátil por C;
- permitir geração nativa direta para controle rigoroso da máquina;
- suportar acesso a memória, registradores, MMIO e assembly somente em regiões explicitamente inseguras;
- preservar uma arquitetura modular, apesar de o compilador poder ser distribuído como um único processo.

## 3. Princípios arquiteturais

### 3.1. Separação entre linguagem e implementação

A especificação define o comportamento observável da linguagem. Lexer, parser, analisador semântico, IR e backends são mecanismos substituíveis que devem respeitar esse contrato.

A AST não deve depender do algoritmo de parsing. A adoção de LR não significa que a árvore seja “uma AST LR”; LR é a estratégia usada para reconhecer a gramática. A AST representa o significado estrutural do programa.

### 3.2. Fronteiras de fase

Cada fase deve possuir entrada, saída, invariantes e diagnósticos próprios. Fases internas não devem imprimir diretamente no terminal nem depender da interface de linha de comando.

O fluxo conceitual será:

```text
Fonte → tokens → parser LR → CST opcional → AST → semântica → AST tipada
      → IR Argos validada → backends de execução
```

O lowering somente pode começar após sucesso léxico, sintático e semântico. Informações de origem devem sobreviver até a IR e, quando viável, até o código de máquina, para sustentar diagnósticos e depuração.

### 3.3. Monolito modular

O compilador pode permanecer como monolito modular, mas suas fronteiras devem ser reais. Frontend, semântica, IR, VM, runtime e backends não devem compartilhar detalhes internos por conveniência. Dependências devem apontar para contratos estáveis, nunca circularmente.

## 4. Arquitetura geral do compilador

O frontend transforma texto em uma AST válida e tipada. A IR Argos funciona como representação comum da semântica executável. A arquitetura-alvo possui duas rotas de saída próprias da ArgosLang:

1. **Execução em VM:** IR Argos → bytecode/opcodes → máquina virtual.
2. **Compilação nativa direta:** IR Argos → Machine IR específica do alvo → seleção e alocação de instruções → código de máquina, assembly ou código-objeto.

As duas rotas são irmãs. C, C++, Rust e Swift não são destinos de transpilação nem etapas internas da compilação. Essas linguagens, assim como Assembly, pertencem exclusivamente à camada de interoperabilidade. No desenho-alvo, a VM consumirá bytecode; hoje ela executa a IR diretamente. O backend nativo consome a IR e gera assembly sem transpilar ArgosLang para C, C++, LLVM ou outra linguagem intermediária.

### 4.1. Estado implementado em 22 de outubro de 2026

O estado acima é a direção arquitetural; não descreve tudo o que já existe:

- O parser é manual e usa precedência de operadores. LR(1) é uma meta futura,
  não o algoritmo atual.
- O frontend gera uma IR tipada e validada para o subconjunto do MVP.
- A VM executa essa IR diretamente. Ainda não há bytecode binário separado:
  `BytecodeEmitter` produz apenas texto para inspeção.
- O alvo nativo disponível é x86-64/Linux. O backend emite assembly e chama
  `cc` somente para montar e ligar o executável.
- `run`, `compile` e REPL compartilham frontend, análise semântica e lowering.
  O REPL mantém o estado reconstruindo entradas no mesmo executor VM.
- ARM64, RISC-V e AVR permanecem backends planejados; não são destinos
  funcionais nesta versão.

## 5. Análise léxica

O lexer deve transformar o texto-fonte em tokens preservando, para cada token, sua categoria, lexema e intervalo exato de origem.

O vocabulário léxico deve cobrir:

- identificadores normais e escapados;
- literais inteiros, reais, booleanos, nulos, caracteres e strings;
- strings multilinha adequadas à interoperabilidade;
- palavras-chave de declaração, tipos, fluxo, segurança e módulos;
- delimitadores e terminadores;
- operadores aritméticos, relacionais, lógicos, de atribuição, ponteiros e null-safety;
- anotações destinadas a seleção de modo, alvo ou comportamento.

Deve existir uma única definição canônica para token, localização e intervalo de origem. Nomes de categorias precisam seguir uma convenção uniforme em todo o projeto.

Novas formas sintáticas somente devem entrar na gramática depois de seu vocabulário léxico estar definido sem ambiguidades.

## 6. Parser LR de maior poder

### 6.1. Decisão principal

O parser recomendado é **LR canônico com um símbolo de lookahead, isto é, LR(1)**, gerado a partir de uma gramática declarativa.

Essa escolha oferece poder de reconhecimento maior que LL(1) e, em geral, maior precisão que SLR(1) e LALR(1), porque mantém contextos LR(1) distintos em vez de fundi-los. Ela é adequada a uma linguagem que pretende crescer com declarações complexas, argumentos nomeados, genéricos, padrões, operadores, tipos compostos e construções de baixo nível.

O custo é uma tabela de estados maior e maior disciplina na escrita da gramática. Esse custo é aceitável para o compilador hospedado em PC. Para toolchains extremamente restritas, a tabela pode ser comprimida ou o parser pode ser executado apenas no host que gera o firmware.

### 6.2. Alternativa futura

GLR deve ser reservado para o caso de a linguagem aceitar ambiguidade intencional ou gramáticas que não possam ser resolvidas satisfatoriamente com LR(1). Ele não deve ser o padrão, pois aumenta o custo, pode produzir florestas de parsing e transfere ambiguidades para fases posteriores.

### 6.3. Gramática declarativa

A gramática deve ser escrita em BNF/EBNF como fonte de verdade e transformada em regras compatíveis com o gerador LR. Açúcar EBNF, como repetição e opcionais, deve ser expandido mecanicamente em não terminais auxiliares.

Precedência e associatividade devem ser declaradas de forma explícita. Conflitos shift/reduce e reduce/reduce não devem ser aceitos silenciosamente. Cada conflito precisa ser eliminado por reestruturação da gramática ou justificado por uma regra normativa de precedência.

### 6.4. CST e AST

O parser pode construir uma CST enxuta quando for necessário preservar pontuação, comentários e formatação para ferramentas. A AST deve ser derivada da CST ou construída por ações semânticas controladas.

A CST serve a formatadores, IDEs e refatorações. A AST elimina pontuação irrelevante e expressa apenas construções semânticas. O projeto não deve forçar uma única árvore a cumprir os dois papéis.

### 6.5. Recuperação de erros

O parser LR deve possuir recuperação de erros orientada pela gramática, usando pontos de sincronização como terminadores, fechamento de bloco e início inequívoco de declarações.

Os diagnósticos devem informar:

- o token encontrado;
- as categorias esperadas naquele estado;
- o intervalo de origem;
- uma correção provável quando ela for confiável;
- a causa primária sem gerar cascatas desnecessárias.

Inserções ou remoções sintéticas usadas na recuperação devem ser marcadas na CST/AST para que fases posteriores não tratem nós incompletos como válidos.

## 7. Organização da gramática

### 7.1. Unidade de compilação

Uma unidade de compilação pode conter declarações de módulo, imports, tipos, funções, variáveis e demais itens permitidos no nível superior. A política sobre instruções soltas no topo deve ser decidida separadamente para módulos compilados, scripts e REPL.

### 7.2. Declarações

A gramática deve contemplar, progressivamente:

- variáveis imutáveis e mutáveis;
- funções e parâmetros;
- estruturas, classes, records, enums e interfaces;
- aliases e parâmetros genéricos;
- membros, visibilidade e modificadores;
- módulos, namespaces, imports e exportações;
- operadores definidos pela linguagem, caso esse recurso seja aprovado.

### 7.3. Instruções

O conjunto planejado inclui blocos, retorno, condicionais, repetição, seleção por padrões ou casos, controle de laços, exceções ou resultado tipado, regiões inseguras e blocos de interoperabilidade.

A semântica de terminadores deve ser inequívoca. Se newline funcionar como terminador, o lexer e a gramática devem definir quando ele é significativo e quando é ignorado.

### 7.4. Expressões

As expressões devem cobrir:

- atribuições simples e condicionais;
- operadores lógicos;
- coalescência nula;
- igualdade normal e segura;
- comparação relacional;
- aritmética;
- operadores prefixados e postfixados;
- chamadas, argumentos posicionais e nomeados;
- acesso normal e seguro a membros;
- indexação;
- casts e testes de tipo;
- lambdas e closures;
- construção de valores e tipos;
- operações explícitas de ponteiro dentro de contexto inseguro.

Associatividade deve ser definida semanticamente: atribuições tendem a associar à direita; operações aritméticas, relacionais e lógicas normalmente associam à esquerda; operadores postfixados se encadeiam com maior precedência.

### 7.5. Argumentos nomeados

Argumentos nomeados devem ser uma construção formal, não apenas uma convenção dos exemplos. A linguagem precisa decidir:

- se argumentos nomeados podem ser misturados com posicionais;
- se nomes fazem parte da ABI pública da função;
- se a ordem pode variar;
- como sobrecarga e valores padrão interagem com os rótulos;
- se parâmetro e rótulo externo podem ter nomes diferentes.

### 7.6. Lambdas e closures

Funções anônimas precisam definir parâmetros, tipos inferidos ou explícitos, retorno, captura e mutabilidade das capturas. O modelo de memória deve dizer se uma closure pode escapar do escopo e como seu ambiente é armazenado sem depender de garbage collector.

## 8. Modelo da AST

### 8.1. Objetivo

A AST deve ser uma representação estável, independente de tokens e suficientemente rica para a análise semântica. Todo nó deve manter seu intervalo de origem.

### 8.2. Famílias de nós

A árvore deve distinguir:

- unidade de compilação e módulos;
- declarações;
- instruções;
- expressões;
- sintaxe de tipos;
- padrões;
- atributos e modificadores;
- construções unsafe, raw e de interoperabilidade.

### 8.3. Sintaxe versus significado

Tipos escritos no fonte pertencem inicialmente à sintaxe de tipos. Tipos resolvidos pertencem ao modelo semântico. Um nome como `Usuario` não deve ser convertido prematuramente em um enum fechado.

A AST original pode ser preservada e anotada por tabelas laterais de tipos, símbolos, coerções e constantes. Alternativamente, pode haver uma HIR tipada separada. A decisão deve evitar que o frontend perca a forma original necessária a diagnósticos.

### 8.4. Nós inválidos

Erros recuperáveis devem produzir nós inválidos explícitos. Eles permitem continuar a análise, mas nunca devem ser rebaixados para IR executável.

## 9. Análise semântica

### 9.1. Responsabilidades

A semântica deve abranger:

- resolução de nomes;
- construção e consulta de escopos;
- ligação entre usos e declarações;
- resolução e inferência de tipos;
- validação de mutabilidade e l-values;
- checagem de chamadas e retornos;
- validação de controle de fluxo;
- null-safety e refinamento de tipos;
- verificação de regiões unsafe;
- cálculo de constantes;
- preparação de informações para lowering.

### 9.2. Escopos e símbolos

Escopos devem existir para módulos, tipos, funções, blocos, parâmetros genéricos e closures. Símbolos devem distinguir ao menos variáveis, parâmetros, funções, tipos, membros e módulos.

Declarações duplicadas no mesmo namespace semântico devem ser rejeitadas, enquanto shadowing somente deve ser permitido segundo uma política explícita. Tipos ou identificadores desconhecidos nunca podem resultar em sucesso silencioso.

### 9.3. Tipos nominais

O sistema não deve limitar tipos a um enum de primitivos. Tipos definidos pelo usuário e tipos da biblioteca padrão exigem identidade nominal, parâmetros genéricos e vínculo com símbolos.

`string` deve ter contrato formal como tipo intrínseco ou tipo da biblioteca padrão.

### 9.4. Chamadas

A resolução de chamadas deve considerar quantidade de argumentos, rótulos, tipos, valores padrão, genéricos e eventual sobrecarga. O resultado da resolução deve identificar inequivocamente a função chamada e as conversões autorizadas.

### 9.5. Retornos e fluxo

Funções sem valor devem ter contrato explícito de retorno. Funções que retornam valor precisam garantir retorno em todos os caminhos alcançáveis, e não apenas conter algum comando de retorno textual.

Condições devem aceitar somente valores booleanos não opcionais, salvo se a linguagem definir coerção explícita diferente. Análise de alcançabilidade, inicialização definida e refinamento por fluxo devem fazer parte da evolução semântica.

## 10. Sistema de tipos

### 10.1. Primitivos

A especificação deve definir tipos booleanos, inteiros com largura fixa, inteiros do tamanho da palavra, ponto flutuante, caracteres e ausência de valor. Sinal, largura, overflow, promoção aritmética e disponibilidade por alvo precisam ser normativos.

`byte` não deve ser simultaneamente descrito como inteiro com sinal e mapeado ambiguamente para tipos C com ou sem sinal. A decisão recomendada é separar inteiros assinados e não assinados por largura ou declarar de maneira única o sinal de cada tipo.

`char` precisa representar um escalar Unicode ou um byte; “caractere UTF-8 de um byte” é conceitualmente insuficiente para texto Unicode. Se a prioridade embarcada exigir um byte, deve existir um tipo textual separado para caracteres Unicode.

### 10.2. Tipos opcionais

Um tipo não opcional não aceita `null`. Um tipo opcional representa ausência ou um valor do tipo base.

A linguagem deve decidir se opcionais aninhados são semanticamente distintos. Caso não sejam, a normalização deve impedir profundidades redundantes. Caso sejam, operadores e representação precisam preservar cada nível.

### 10.3. Conversões

Conversões implícitas devem ser poucas, previsíveis e livres de perda silenciosa. Conversões de largura, sinal, ponto flutuante, ponteiros e opcionais precisam de uma matriz normativa.

### 10.4. Tipos compostos

Arrays, slices, tuples, funções, referências, ponteiros, structs, enums e genéricos devem possuir representação, layout e regras de variância explícitas antes de entrarem na IR estável.

## 11. Null-safety

### 11.1. Garantia central

Valores de tipo não opcional devem ser não nulos por construção. A ausência deve ser representada no tipo e tratada antes do uso que exija um valor presente.

### 11.2. Navegação segura

A navegação segura avalia o membro somente quando a base está presente e produz resultado opcional. O encadeamento deve preservar a avaliação única da base e impedir duplicação de efeitos colaterais.

### 11.3. Coalescência

A coalescência retorna o valor presente da esquerda ou avalia o fallback. O fallback deve ser lazy e compatível com o tipo desembrulhado.

### 11.4. Atribuição segura

A atribuição segura modifica o destino somente se a origem opcional contém valor. O destino precisa ser mutável, atribuível e compatível com o tipo desembrulhado.

### 11.5. Atribuição por coalescência

A atribuição por coalescência avalia e atribui o lado direito somente quando o destino opcional está ausente. O destino precisa ser um l-value mutável e deve ser avaliado uma única vez.

### 11.6. Igualdade segura

O significado de igualdade segura precisa ser definido por tabela completa para valor/valor, valor/ausência e ausência/ausência. O nome não basta para distinguir sua semântica da igualdade normal sobre opcionais.

### 11.7. Desempacotamento forçado

Deve existir apenas uma grafia canônica para force unwrap, salvo se duas grafias tiverem semânticas realmente diferentes. A falha precisa ter comportamento definido por modo: pânico controlado, trap ou diagnóstico quando comprovável estaticamente.

### 11.8. Refinamento por fluxo

Depois de testes confiáveis de presença, o analisador pode refinar temporariamente um opcional para não opcional. Esse refinamento deve respeitar mutações, aliases, concorrência e chamadas capazes de invalidar a prova.

## 12. Imutabilidade e efeitos

Declarações imutáveis devem ser o padrão; mutabilidade exige escolha explícita. Imutabilidade da referência e imutabilidade profunda do valor devem ser conceitos distintos.

Funções puras, efeitos e operações unsafe podem receber marcação própria no futuro. Pureza só deve ser prometida se o compilador conseguir verificá-la ou se houver um contrato de efeitos suficientemente preciso.

## 13. Modelo de memória

### 13.1. Decisão geral

A linguagem deve priorizar gerenciamento determinístico sem garbage collector obrigatório. Isso não deve ser chamado de “RAII” até que destrutores, posse, movimento, empréstimos e ordem de destruição estejam especificados.

### 13.2. Regiões de memória

O contrato deve distinguir:

- valores locais e frames de pilha;
- dados estáticos;
- heap explícito ou gerenciado por ownership;
- memória mapeada de hardware;
- memória pertencente a código estrangeiro;
- ambientes de closures;
- armazenamento interno da VM.

### 13.3. Posse e tempo de vida

Antes de suportar dados dinâmicos, a linguagem deve escolher entre ownership estático, contagem de referências opt-in, arenas, alocadores explícitos ou uma combinação controlada. Cópia, movimento, destruição e empréstimo não podem ficar implícitos.

### 13.4. Determinismo

O perfil embarcado deve permitir proibir heap, recursão, exceções, alocações ocultas e operações de latência não limitada. “Adequado a hard real-time” só pode ser afirmado para um perfil com runtime e WCET analisáveis, não para toda a linguagem automaticamente.

## 14. Modo seguro, unsafe e raw

### 14.1. Modo seguro

O código comum deve manter verificações de tipos, inicialização, limites e null-safety segundo o perfil selecionado. Ele não expõe endereços físicos arbitrários.

### 14.2. Região unsafe

`unsafe` delimita operações cuja correção não pode ser provada pelo compilador. Entrar em `unsafe` não deve desativar toda a tipagem; apenas autoriza um conjunto explícito de operações perigosas.

### 14.3. Modo raw

`raw` deve selecionar semântica e lowering nativos para funções ou módulos que exigem endereços reais, ABI da plataforma e acesso direto ao hardware. O contrato precisa especificar se código VM pode chamar raw, como valores atravessam essa fronteira e quais alvos aceitam essa combinação.

### 14.4. Ponteiros

Ponteiros imutáveis e mutáveis precisam de regras para criação, conversão, aritmética, alinhamento, aliasing, validade, volatilidade e desreferenciação. MMIO exige operações voláteis e barreiras de memória apropriadas ao alvo.

## 15. IR Argos

### 15.1. Papel

A IR Argos deve ser a representação intermediária canônica e independente de LLVM, C ou qualquer arquitetura. Ela expressa a semântica já validada e serve como entrada comum para VM e backends.

### 15.2. Estrutura

A IR deve possuir módulos, funções, blocos básicos, valores, constantes, tipos, instruções e terminadores. Cada bloco deve terminar exatamente uma vez. Usos, definições, destinos de salto e assinaturas de chamada precisam ser verificáveis.

### 15.3. Forma da IR

Registradores virtuais e blocos básicos são adequados para otimização e backends. A decisão entre SSA e registradores mutáveis deve ser formal. Se SSA for escolhida, merges de fluxo exigem valores phi ou argumentos de bloco.

### 15.4. Tipos e efeitos

Toda instrução deve declarar operandos, resultado, tipo e efeitos observáveis. A IR deve distinguir operações puras, acesso a memória, chamadas, traps, controle de fluxo e operações voláteis.

### 15.5. Null-safety na IR

Null-safety não pode depender de nomes informais de operadores. A IR precisa representar teste de presença, extração, construção de opcionais, seleção lazy e trap de unwrap. O lowering deve preservar avaliação única e ordem de efeitos.

### 15.6. Chamadas

A ABI lógica de chamadas na IR deve definir onde vivem argumentos e retorno, como chamadas sem retorno são representadas, como variádicos e genéricos são especializados e como chamadas estrangeiras diferem das internas.

### 15.7. Validador

Todo módulo IR deve passar por um verificador antes de chegar a qualquer backend. O verificador precisa rejeitar referências inexistentes, tipos incompatíveis, blocos malformados, registradores usados antes de definição e instruções não suportadas pelo perfil alvo.

## 16. Máquina virtual e bytecode

### 16.1. Finalidade

A VM atende REPL, scripts, testes, portabilidade e ciclo rápido de desenvolvimento. Ela não deve ser confundida com o backend nativo direto nem com os executores de interoperabilidade.

### 16.2. Modelo de execução

Deve ser escolhido um modelo principal: registradores virtuais, pilha de operandos ou híbrido claramente especificado. Uma VM de registradores com slots para variáveis e frames para chamadas é coerente com a IR planejada.

### 16.3. Formato do bytecode

O formato precisa definir versão, endianness, largura dos operandos, pool de constantes, funções, blocos, metadados de debug e validação antes da execução.

### 16.4. Instruções

O conjunto inicial deve cobrir carga e armazenamento, aritmética, comparação, saltos, chamadas, retorno e operações explícitas de opcionais. Instruções posteriores devem incluir memória, agregados, membros, conversões e interoperabilidade conforme os contratos forem fechados.

### 16.5. Segurança

Bytecode externo deve ser verificado antes da execução. Código raw e assembly não devem ser executados diretamente pela VM comum; devem usar stubs nativos autorizados ou ser rejeitados no perfil VM.

## 17. Backend nativo direto

### 17.1. Pipeline

O caminho nativo direto transforma a IR Argos em uma Machine IR específica do alvo. Em seguida executa seleção de instruções, legalização de tipos, alocação de registradores, construção de frame, lowering da ABI, emissão de assembly ou encoding de bytes, relocação e geração de objeto.

### 17.2. Alvos

Os alvos podem incluir x86-64, AArch64, RISC-V, AVR e arquiteturas de microcontroladores. Cada alvo precisa declarar:

- conjunto e classes de registradores;
- calling convention;
- endianness e largura de palavra;
- layout de dados e alinhamento;
- instruções legais;
- formato de objeto ou imagem;
- suporte a ponto flutuante;
- regras para MMIO e atomics.

### 17.3. Assembly e bytes

Gerar assembly textual e gerar diretamente código-objeto são opções distintas. Assembly facilita inspeção e reutiliza o assembler da plataforma. Emissão direta de bytes exige encoder, relocations, símbolos e formato de objeto completos.

## 18. Interoperabilidade nativa

### 18.1. Princípio

Interop permite incorporar, compilar, ligar ou executar unidades escritas em C, C++, Rust, Swift e Assembly sem transformar nenhuma dessas linguagens em representação intermediária da ArgosLang. Cada fronteira precisa declarar linguagem, ABI, entradas, saídas, efeitos, ownership e disponibilidade por plataforma.

### 18.2. Família de executores

A linguagem pode oferecer construções conceituais como `c_executor`, `cpp_executor`, `rust_executor`, `swift_executor` e `asm_executor`. Cada executor recebe uma unidade de código estrangeiro, encaminha essa unidade ao frontend ou à toolchain apropriada e liga o artefato resultante ao programa ArgosLang mediante ABI explícita.

Esses executores não fazem parte do pipeline de compilação do código Argos. Eles são portas de interoperabilidade anexas ao backend nativo. A compilação normal continua sendo Argos → IR Argos → Machine IR → máquina.

Um executor precisa definir dois modos diferentes:

- **declaração estrangeira:** incorpora funções, tipos ou símbolos que poderão ser chamados pela ArgosLang;
- **programa estrangeiro isolado:** compila e executa uma unidade que possui sua própria entrada, como uma função principal, em processo ou módulo separado.

Uma unidade estrangeira com sua própria função principal não deve ser simplesmente inserida no executável Argos principal, pois isso pode produzir duas entradas de programa. O executor deve tratá-la como programa isolado ou exigir a indicação explícita do símbolo que será exportado.

### 18.3. Requisito de toolchain

“Compilação direta para máquina” descreve o código escrito em ArgosLang. Código estrangeiro ainda precisa ser compreendido por alguma implementação da linguagem estrangeira. Portanto, cada executor deve usar uma destas estratégias:

- invocar uma toolchain externa compatível;
- incorporar um frontend/compilador dessa linguagem como biblioteca;
- consumir previamente um objeto, biblioteca ou módulo já compilado.

Sem uma dessas estratégias, uma string contendo C, C++, Rust ou Swift é apenas texto e não pode ser convertida corretamente em código de máquina. Assembly pode ser tratado por um assembler interno ou externo, mas ainda exige validação, relocação e conhecimento do alvo.

### 18.4. C

Integração C deve preferir declarações FFI e funções externas tipadas. Blocos embutidos podem existir para casos especiais, mas precisam de regras de captura, higiene de nomes, includes, retorno, compilação e diagnóstico.

O mapeamento de tipos deve ser exato e dependente do alvo quando necessário. Strings, arrays, structs, opcionais e callbacks exigem ABI explícita; não possuem mapeamento 1:1 automático.

### 18.5. C++, Rust e Swift

Interop com C++ deve passar por uma camada ABI estável ou por compilação coordenada, pois nomes, templates, exceções e layout não são portáveis como C. Rust deve definir `extern`, política de panic, ownership e representação de tipos na fronteira. Swift deve definir calling convention, runtime, ARC, metadados e disponibilidade por alvo. Cada linguagem deve possuir adaptador próprio, mesmo quando todas expõem uma interface uniforme de executor.

### 18.6. Assembly

O executor de Assembly e o assembly inline somente devem ser permitidos em contexto unsafe e em backends nativos compatíveis. A construção precisa declarar arquitetura, dialeto, entradas, saídas, clobbers, memória, volatilidade e símbolos. A simples interpolação de nomes do escopo é insuficiente para garantir correção.

### 18.7. Ligação e ABI comum

Todos os executores devem convergir para objetos, símbolos ou módulos nativos ligáveis, nunca para a IR semântica da linguagem estrangeira. A camada de ligação deve controlar nomes, visibilidade, calling convention, layout, alinhamento, exceções, unwinding, ownership e inicialização de runtimes.

Quando uma linguagem exigir runtime próprio, como Swift ou determinadas configurações de C++ e Rust, esse runtime deve ser declarado como dependência do artefato final. “Interoperabilidade sem overhead” não pode ser prometida universalmente.

## 19. Runtime e biblioteca padrão

Runtime e biblioteca padrão devem ser separados. O runtime contém apenas mecanismos indispensáveis à execução, como entrada do programa, traps, suporte a opcionais, frames da VM e interoperabilidade. A biblioteca padrão oferece strings, coleções, I/O, algoritmos e abstrações de plataforma.

Perfis mínimos devem poder excluir partes do runtime e da biblioteca. Um firmware AVR não deve carregar recursos de desktop por dependência indireta.

## 20. Perfis de compilação

A linguagem deve definir perfis em vez de prometer todas as capacidades em todos os alvos:

- **hosted:** sistema operacional, biblioteca padrão ampla, VM, backend nativo e executores de interoperabilidade;
- **embedded:** runtime reduzido e toolchain cruzada;
- **freestanding:** sem pressupor sistema operacional;
- **hard real-time:** sem operações de latência não limitada;
- **safe:** proíbe raw, assembly e conversões perigosas;
- **development:** metadados, checks e diagnósticos ampliados.

Capacidades indisponíveis devem gerar diagnóstico em compilação.

## 21. Diagnósticos

Diagnósticos devem ser estruturados e possuir código estável, severidade, mensagem, localização primária, notas secundárias e sugestões opcionais.

Cada fase deve emitir erros de seu domínio:

- lexer: texto ou literal inválido;
- parser: construção sintática inesperada;
- semântica: nomes, tipos, mutabilidade e fluxo;
- IR: invariantes internas e incompatibilidade de perfil;
- backend: alvo, ABI ou instrução não suportada;
- runtime: traps que não puderam ser eliminados estaticamente.

Sucesso silencioso diante de tipo desconhecido, construção não suportada ou saída vazia é proibido.

## 22. Interface de ferramentas

A ferramenta principal deve expor operações conceitualmente separadas para inspeção de tokens, parsing, AST, checagem, IR, bytecode, execução em VM, emissão de assembly ou objeto, compilação nativa, seleção de alvo e execução dos adaptadores de interoperabilidade.

Cada operação deve informar com clareza até qual fase o programa avançou. “Parsing bem-sucedido” não significa “programa válido”; “checagem bem-sucedida” não significa “executável produzido”.

Um REPL deve usar o mesmo frontend, semântica e IR do compilador, com suporte incremental planejado em vez de uma implementação paralela da linguagem.

## 23. Estratégia de testes

### 23.1. Gramática e parser

Devem existir testes positivos e negativos para cada produção, precedência, associatividade, ambiguidades, recuperação de erros e término de instruções. O gerador LR deve falhar quando surgirem conflitos não autorizados.

### 23.2. AST

Testes devem validar forma, spans, nós inválidos e estabilidade da árvore independentemente da apresentação textual.

### 23.3. Semântica

Cada regra de nomes, tipos, opcionais, chamadas, retorno, mutabilidade, l-value, unsafe e fluxo deve possuir casos aceitos e rejeitados.

### 23.4. IR

Testes devem verificar lowering, invariantes, controle de fluxo, efeitos, tipos e rejeição de módulos inválidos. Golden tests textuais podem complementar verificações estruturais, não substituí-las.

### 23.5. Modos de execução

Os mesmos programas devem produzir comportamento equivalente na VM e no backend nativo direto. Diferenças autorizadas por perfil ou arquitetura precisam ser documentadas. Cada executor estrangeiro deve possuir testes próprios de compilação, ligação, chamada, retorno, erro e incompatibilidade de ABI.

### 23.6. Embarcados

Testes cruzados devem verificar tamanho de imagem, uso de RAM, layout, acesso volátil, ABI e execução em simulador ou hardware real.

## 24. Roadmap recomendado

### Capítulo I — Constituição da linguagem

Fechar o léxico, a terminologia, os tipos primitivos, `string`, Unicode, terminadores, argumentos nomeados, operadores de opcionais e a divisão entre safe, unsafe e raw.

### Capítulo II — Gramática LR(1)

Converter a EBNF em gramática declarativa canônica, declarar precedência e associatividade, eliminar conflitos e definir recuperação de erros. Adotar LR(1) como padrão e medir o tamanho das tabelas.

### Capítulo III — CST, AST e HIR

Definir as três camadas e seus contratos. Preservar spans, comentários quando necessários às ferramentas, nós inválidos e vínculos semânticos.

### Capítulo IV — Semântica completa do núcleo

Fechar escopos, tipos nominais, chamadas, retornos por caminho, mutabilidade, l-values, opcionais e refinamento por fluxo. Nenhum tipo desconhecido deve ser aceito silenciosamente.

### Capítulo V — IR canônica

Definir uma única IR, seu sistema de tipos, terminadores, chamadas, opcionais, memória, efeitos e verificador. Escolher e documentar SSA ou registradores mutáveis.

### Capítulo VI — Primeira fatia executável

Escolher um subconjunto pequeno, porém completo: funções, valores básicos, variáveis, aritmética, condicionais, laços, chamadas e retornos. Toda construção fora do subconjunto deve falhar explicitamente.

### Capítulo VII — VM

Congelar a primeira versão do bytecode, implementar validação, frames, chamadas, controle de fluxo, traps e execução determinística do subconjunto.

### Capítulo VIII — Memória e agregados

Fechar ownership, movimento, destruição, arrays, slices, strings, structs e closures antes de ampliar a interoperabilidade.

### Capítulo IX — Nativo direto

Selecionar um primeiro alvo, definir Machine IR, ABI, registradores, frames, assembly e objetos. Expandir para novos alvos somente depois da conformidade do primeiro.

### Capítulo X — Bare metal e FFI

Adicionar ponteiros, volatile, MMIO, assembly inline e FFI tipada sob regras unsafe. Definir perfis embedded e freestanding. Integrar os executores de C, C++, Rust, Swift e Assembly como adaptadores independentes, sem usá-los para compilar código Argos.

### Capítulo XI — Otimização e ecossistema

Somente depois da correção ponta a ponta, adicionar otimizações, cache incremental, servidor de linguagem, formatador, package manager, depurador e ferramentas de análise.

## 25. Decisões em aberto

Antes de congelar a versão inicial da linguagem, devem ser respondidas formalmente as seguintes questões:

1. `string` é intrínseca, tipo da biblioteca padrão ou família de tipos?
2. `char` representa byte, code point Unicode ou grapheme?
3. `byte` é assinado ou não assinado?
4. Opcionais aninhados são preservados ou normalizados?
5. Qual é a semântica exata de igualdade segura?
6. Qual grafia única representa force unwrap?
7. Newline é terminador significativo em quais contextos?
8. Argumentos nomeados fazem parte da ABI pública?
9. A IR será SSA, baseada em registradores mutáveis ou terá duas formas?
10. Qual é o modelo de ownership, empréstimo e destruição?
11. Como código VM atravessa a fronteira para código raw?
12. Exceções existirão, ou erros serão representados por tipos de resultado?
13. Qual é o primeiro alvo do backend nativo direto?
14. Qual formato de bytecode e política de versionamento serão usados?
15. Quais garantias pertencem a cada perfil de compilação?
16. Cada executor será embutido, externo ou configurável por toolchain?
17. Código estrangeiro será ligado ao executável, carregado dinamicamente ou executado em processo isolado?
18. Qual ABI comum será usada entre Argos, C, C++, Rust e Swift?

## 26. Critérios de aceite arquitetural

Uma fase somente é considerada pronta quando:

- possui contrato documentado;
- rejeita entradas inválidas com diagnóstico útil;
- não depende de detalhes privados da fase anterior;
- preserva informações de origem relevantes;
- possui testes positivos, negativos e de fronteira;
- não anuncia suporte a construções que apenas reconhece parcialmente.

Uma funcionalidade executável somente é considerada entregue quando percorre o pipeline completo de um backend suportado e produz comportamento observável compatível com a especificação.

## 27. Síntese final

A ArgosLang deve evoluir como uma linguagem híbrida com um único frontend semântico e uma IR própria. O parser LR(1) fornece espaço de crescimento gramatical sem acoplar a AST ao mecanismo de análise. A VM oferece portabilidade e iteração rápida; o backend de máquina produz código nativo diretamente, sem transpilação por C, C++ ou LLVM. C, C++, Rust, Swift e Assembly aparecem somente como linguagens interoperáveis por executores e FFI. `unsafe`, raw, FFI e assembly são extensões controladas do núcleo tipado, não atalhos que anulam seus contratos.

O princípio orientador é simples: primeiro tornar o significado da linguagem inequívoco; depois representá-lo em uma IR verificável; por fim executar a mesma semântica, com fidelidade, em cada backend.

## Implementação de POO — 29/10/2026

O frontend resolve uma superclasse concreta e múltiplos contratos de interface,
valida visibilidade, inicialização, sobrecargas e sobrescritas e registra a
assinatura selecionada de cada chamada. `protocol` foi removido da AST; `data`
usa declarações de classe finais com campos transitivamente imutáveis e
construtor canônico gerado para componentes primários.

O layout de uma subclasse conserva os campos da base como prefixo, inclusive
os privados. Cada layout contém ancestrais e uma tabela de implementações por
assinatura de método. A IR distingue `CALL` de `CALL_VIRTUAL`; suas tabelas,
assinaturas de parâmetros/retorno e prefixos de campos são validados antes da
execução ou emissão nativa. `super` e chamadas privadas usam seleção direta.
Propriedades usam o mesmo mecanismo por meio dos acessores.

A VM seleciona a implementação na tabela do layout real. O backend x86-64
seleciona pelo identificador do layout e emite chamada indireta, preservando
a ABI de inteiros, referências, opcionais e doubles. A construção chama a base
antes dos inicializadores próprios; cada nível inicializa apenas seus campos.
Igualdade de `data` é expandida em comparações recursivas dos campos; classes
comuns continuam com igualdade de identidade.

Objetos e ambientes permanecem na arena de duração da execução. Esta etapa
não implementa ownership/borrow, destruição por escopo ou `deinit`. As regras
observáveis e limitações estão no [guia de POO](guide/10-classes-e-interfaces.md).


### Padrões de acesso e permissões nominais

A AST conserva `:` e `by` em metadados distintos, além da lista `permited`.
O parser atribui acesso público a tipos e funções de topo, privado aos métodos
de classe/data e público aos requisitos de interface, preservando acesso explícito.
A semântica valida relações, subclasses diretas sealed e permissões de leitura
dos campos de data antes do lowering. Essas restrições não alteram o layout
ou a ABI de despacho. `default class` usa o pacote declarado; classes
privadas/protegidas exigem contenedor nominal.

### Pacotes, composição exclusiva e geração de records

O driver reúne os arquivos fornecidos explicitamente em uma análise conjunta.
Cada item conserva seu pacote e cada span conserva o arquivo de origem, inclusive
nos diagnósticos. Nomes nominais são qualificados por pacote e contenedores;
`TypeSyntax.resolved_name` registra a resolução para o lowering. A verificação de
acesso inclui os contenedores e não apenas o tipo final.

Componentes de `with` ficam separados das bases na AST e no grafo de subtipagem.
A semântica gera campos internos e métodos de encaminhamento cuja chamada usa a
instância real do componente; não copia corpos. O resolvedor normal escolhe
sobrecargas e rejeita colisões não resolvidas. Qualificadores de componente são
vinculados aos campos internos, inclusive em closures que capturam o receptor.

Os layouts conservam o prefixo herdado e enumeram `owned_fields`. O validador
confere índices, tipos e preservação do prefixo de ownership. A VM valida dono
único ao anexar componentes. No nativo, o cabeçalho de objeto ocupa 16 bytes:
identificador de layout e ponteiro do dono; os campos começam no deslocamento 16.
A alocação continua na arena; liberação inversa ocorre no encerramento da VM ou
via `atexit` no nativo. Não há destrutores por escopo nem borrow checker.

O lowering de `data` gera igualdade recursiva, `hashCode()` e `toString()`, sem
abrir a visibilidade de componentes ao código usuário. `HASH_SCALAR`,
`STRINGIFY_SCALAR` e `CONCAT_STRINGS` executam na VM e no backend x86-64. Strings
geradas usam armazenamento de duração da execução. `CHECK_UINT8` valida 0..255
antes de armazenar resultados de operações em `byte`/`uint_8`.
