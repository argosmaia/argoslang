# Compilar, interpretar e executar ArgosLang

As explicações de construção do compilador, execução pela VM e distribuição
por máquina estão reunidas aqui. Para estudar a sintaxe, use o
[guia](guide/README.md). O [MVP](geral.md#mvp) detalha os recursos executáveis.

## Índice

- [Compilar ArgosLang](#compilar)
- [Interpretar programas ArgosLang](#interpretar)
- [Executar ArgosLang em diferentes máquinas](#maquinas)

<a id="compilar"></a>

<a id="compilar-compilar-argoslang"></a>

## Compilar ArgosLang

Este guia distingue **compilar o compilador** de **compilar um programa `.argos`**. O `argosc` já gera executáveis para um subconjunto inicial em x86-64/Linux. Os demais recursos continuam disponíveis somente para `check` ou ainda estão em desenvolvimento.

<a id="compilar-1-compilar-o-argosc"></a>

### 1. Compilar o `argosc`

Na raiz do repositório, use CMake 3.20 ou superior, um compilador C++23, ICU (`uc`) e Readline. Para gerar um programa nativo também é necessário `cc` com assembler/linker x86-64. Para construir os testes, o CMake precisa obter Catch2; essa etapa pode exigir acesso à rede na primeira configuração.

```bash
cmake -S . -B build -DCMAKE_CXX_COMPILER=/usr/bin/g++
cmake --build build
```

O executável produzido é `build/bin/argosc`. Para validar a construção:

```bash
ctest --test-dir build --output-on-failure
./build/bin/argosc check examples/minimal_arithmetic.argos
```

Se não for possível obter Catch2, construa somente o compilador:

```bash
cmake -S . -B build-no-tests -DARGOS_BUILD_TESTS=OFF
cmake --build build-no-tests
./build-no-tests/bin/argosc check examples/minimal_arithmetic.argos
```

<a id="compilar-2-compilar-e-rodar-um-argos"></a>

### 2. Compilar e rodar um `.argos`

Na raiz do projeto, use um programa dentro do subconjunto, como `minimal_literal.argos`:

```bash
./build/bin/argosc examples/minimal_literal.argos
./minimal_literal
echo $?
```

O primeiro comando gera `minimal_literal` no **diretório de trabalho atual**, não necessariamente ao lado do `.argos`. O segundo executa o binário; `echo $?` mostra `42`, que é o retorno de `main()` visto como status de processo. Um nome simples sem `./` não costuma ser encontrado pelo shell, a menos que o diretório atual esteja no `PATH`.

<a id="compilar-seu-próprio-arquivo"></a>

#### Seu próprio arquivo

Para `arquivo.argos`, a forma curta pedida é:

```bash
./build/bin/argosc arquivo.argos
./arquivo
echo $?
```

O nome da saída é o nome do fonte sem a extensão `.argos`. O arquivo precisa conter uma entrada `function main() -> int`; por exemplo:

```argos
function main() -> int {
    var answer: int = 6 * 7
    return answer
}
```

Se `argosc` estiver instalado no `PATH`, use `argosc arquivo.argos` no lugar de `./build/bin/argosc arquivo.argos`. A forma explícita também funciona: `./build/bin/argosc compile arquivo.argos`.

<a id="compilar-nomear-a-saída"></a>

#### Nomear a saída

Use `-o` para escolher o caminho do binário:

```bash
./build/bin/argosc compile examples/minimal_arithmetic.argos -o /tmp/argos-arithmetic
/tmp/argos-arithmetic
echo $?
```

Nesse caso o status esperado é `14`. O linker escreve uma saída temporária e só instala o executável após sucesso; `-o` não pode apontar para o próprio arquivo-fonte. Em sistemas Unix, o status do processo preserva apenas os oito bits baixos do retorno de `main()`: não use `echo $?` para verificar resultados fora de `0..255`. `print` escreve na saída padrão. Rode `echo $?` **imediatamente** após o executável, antes de outro comando alterar o status do shell.

**Limites atuais:** o backend nativo suporta x86-64/Linux e compartilha a IR
tipada/validada com a VM. Strings, opcionais, arrays homogêneos, classes
concretas e closures fazem parte da execução, dentro dos limites do
[MVP](geral.md#mvp). `/` produz `double`; `%` é inteiro e falhas de módulo
encerram com status 70. `float` e `double` usam binary64. O backend emite
assembly e usa `cc` para montar e ligar, sem transpilar ArgosLang para C.

`examples/null_safety.argos` **ainda não compila para executável**: não define `main()` e usa protocolos e variáveis de topo, que não têm lowering nesse backend. `argosc check examples/null_safety.argos` pode validar o frontend, mas `argosc examples/null_safety.argos` termina com diagnóstico `ARGOS-C0001`/`ARGOS-C0002`, sem criar binário. O exemplo `minimal_optional.argos` já executa na VM e no nativo.

Se a compilação falhar, leia o código e a posição do erro antes de tentar executar a saída. Um erro `ARGOS-C0001` indica uma construção válida para o frontend, mas ainda ausente do backend; `ARGOS-C0002` indica falta de `main()` compilável. Um `check` bem-sucedido não elimina esses erros de compilação. Também não confunda um executável antigo que já exista no disco com uma saída produzida por uma tentativa que falhou.

<a id="compilar-3-verificar-um-argos-sem-gerar-executável"></a>

### 3. Verificar um `.argos` sem gerar executável

Os comandos do frontend são `lex`, `parse`, `ast` e `check`:

```bash
./build/bin/argosc lex examples/minimal_arithmetic.argos
./build/bin/argosc parse examples/minimal_arithmetic.argos
./build/bin/argosc ast examples/minimal_arithmetic.argos
./build/bin/argosc check examples/minimal_arithmetic.argos
```

`check` valida léxico, sintaxe e semântica. Se não houver erros, ele termina com código 0; **não** executa `main()` nem produz código-objeto. Um erro no arquivo termina com código 65. `ctest` inclui testes de execução nativa dos exemplos suportados no host x86-64/Linux.

<a id="compilar-4-ampliar-a-compilação"></a>

### 4. Ampliar a compilação

O caminho executável atual e a arquitetura-alvo estão descritos em
[architecture.md](architecture.md):

```text
.argos -> lexer -> parser manual -> semântica -> IR tipada e validada
       -> VM (direto da IR) ou assembly x86-64 -> cc (montagem/link)
```

Um arquivo de bytecode independente ainda não existe, e passar em `check` não
significa que toda construção do frontend esteja disponível para execução.

Para rodar um programa pela VM, consulte [Interpretar programas ArgosLang](#interpretar). Para distribuição e alvos de hardware, consulte [Executar ArgosLang em diferentes máquinas](#maquinas).

<a id="compilar-gerar-assembly"></a>

#### Gerar assembly

Para salvar o assembly GNU/AT&T x86-64 em vez do executável:

```bash
./build/bin/argosc compile examples/minimal_arithmetic.argos -S -o /tmp/programa.s
cc /tmp/programa.s -o /tmp/programa
/tmp/programa
echo $? # 14
```

A forma curta `argosc -S arquivo.argos` também funciona e gera `arquivo.s`
no diretório atual. A opção `-S` usa o backend existente sobre a IR tipada e
validada, sem chamar `cc`. Montar e ligar o resultado exige uma toolchain
x86-64/Linux. O subconjunto suportado é o mesmo da compilação nativa.


---

<a id="interpretar"></a>

<a id="interpretar-interpretar-programas-argoslang"></a>

## Interpretar programas ArgosLang

**Estado atual:** `argosc run`, `argosc compile` e o REPL compartilham a IR
executável. Arquivos suportam funções, strings, classes concretas, closures,
opcionais e arrays homogêneos. O REPL mantém funções e valores numéricos/bool,
inclusive opcionais; strings, arrays, objetos e closures não persistem entre
entradas. A matriz completa está no [MVP](geral.md#mvp). `/` sempre produz
`double`; `float` e `double` usam binary64 nesta versão.

<a id="interpretar-compilar-não-é-interpretar"></a>

### Compilar não é interpretar

Estes dois fluxos têm saídas diferentes:

| Objetivo | Comando | Disponibilidade |
| --- | --- | --- |
| Validar o fonte | `argosc check arquivo.argos` | Disponível |
| Gerar um binário do host | `argosc arquivo.argos` | Disponível para o subconjunto x86-64/Linux |
| Rodar o binário gerado | `./arquivo` | Disponível após compilação bem-sucedida |
| Executar pela VM | `argosc run arquivo.argos` | Disponível para o subconjunto executável |

`argosc arquivo.argos` **não** interpreta: produz um executável nativo. `./arquivo` executa esse binário sem ler novamente o fonte `.argos`. Já `argosc run arquivo.argos` executa em memória, sem criar executável.

<a id="interpretar-o-que-fazer-hoje"></a>

### O que fazer hoje

Construa o `argosc` conforme [Compilar ArgosLang](#compilar) e verifique um programa:

```bash
./build/bin/argosc check examples/minimal_literal.argos
```

Isso confirma apenas a validade do frontend. `ast` imprime a árvore sintática, não avalia expressões:

```bash
./build/bin/argosc ast examples/minimal_literal.argos
```

Para executar pela VM, use:

```bash
./build/bin/argosc run examples/minimal_literal.argos
```

A saída é `Resultado da VM: 42`; o processo termina com status zero quando a execução é bem-sucedida. O valor de `main()` é impresso, em vez de ser usado como status do processo, para preservar resultados fora de `0..255`.

Para observar o resultado de um programa do subconjunto pela rota nativa, compile e rode como em [Compilar ArgosLang](#compilar):

```bash
./build/bin/argosc examples/minimal_literal.argos
./minimal_literal
echo $?
```

O status esperado é `42`; isso é **execução nativa**, não interpretação. Protocolos e herança ainda podem passar em `check` sem ter execução. Opcionais já executam na VM e no nativo.

<a id="interpretar-como-a-interpretação-funciona"></a>

### Como a interpretação funciona

A arquitetura em [architecture.md](architecture.md) separa o frontend das
rotas de execução:

```text
.argos -> lexer -> parser -> semântica -> IR tipada e validada
       -> VM (direto da IR) ou backend nativo x86-64/Linux
```

No código atual, a VM interpreta os opcodes da IR diretamente. Um validador
compartilhado verifica funções, blocos, registradores, slots, chamadas e tipos
antes da execução ou da geração nativa. Ainda não existe bytecode binário
separado; `BytecodeEmitter` é somente uma visualização textual. O operador
`%` permanece inteiro; divisor zero e `int.min % -1` são erros de runtime.

<a id="interpretar-fluxo-de-run"></a>

### Fluxo de `run`

`argosc run arquivo.argos` valida o fonte, gera a IR executável, inicia a VM e executa a entrada sem produzir um executável nativo no diretório atual. Os testes de ponta a ponta cobrem literal, divisão real, aritmética, desvios, laços, `break`, `continue`, chamadas, `print`, erros de módulo e equivalência VM/nativo. Opcionais também são executados pela VM.

```bash
argosc run examples/minimal_literal.argos
```

<a id="interpretar-repl"></a>

### REPL

`argosc repl` usa a mesma IR e VM, mantendo valores `int`/`bool`/`float`/`double`, seus opcionais e funções
entre entradas. Ele não cria arquivos e aceita `:help`, `:reset`, `:quit` ou
`:q`:

```text
$ argosc repl
>>> 6 * 7
42
>>> :quit
```


---

<a id="maquinas"></a>

<a id="maquinas-executar-argoslang-em-diferentes-máquinas"></a>

## Executar ArgosLang em diferentes máquinas

Hoje você pode executar **o compilador `argosc`** em um host preparado para construí-lo e executar **um subconjunto de programas `.argos`** como binário nativo em x86-64/Linux. A VM já executa a IR no host; os outros backends nativos ainda são esboços.

<a id="maquinas-host-linux-macos-e-windows"></a>

### Host: Linux, macOS e Windows

O projeto usa CMake, C++23, ICU e Readline. Em cada sistema, instale uma toolchain C++23 e o pacote de desenvolvimento ICU apropriado; depois configure e construa como em [Compilar ArgosLang](#compilar). O repositório não fornece binários pré-compilados nem uma matriz de plataformas já validada. Um build que conclui no host comprova apenas que `argosc` roda **naquele host**.

Execute, a partir da raiz do projeto:

```bash
./build/bin/argosc check examples/minimal_loop.argos
```

Em Windows, o caminho e o sufixo do executável dependem do gerador CMake escolhido; localize `argosc.exe` na pasta de saída do build. Caso o CMake não encontre ICU, instale a biblioteca de desenvolvimento e configure novamente. Caso falhe ao obter Catch2, use `-DARGOS_BUILD_TESTS=OFF` para construir somente o CLI.

<a id="maquinas-programa-argoslang-vm-portátil"></a>

### Programa ArgosLang: VM portátil

O caminho planejado é distribuir bytecode ArgosLang e uma VM **implementada para cada host**. O mesmo bytecode só será portátil se o formato, a ABI, o runtime e as dependências do programa forem compatíveis. Para dispositivos restritos, a VM também precisa caber em memória, atender aos requisitos de tempo e expor as operações de hardware exigidas. Nada disso está validado hoje; consulte [Interpretar programas ArgosLang](#interpretar).

<a id="maquinas-programa-argoslang-binário-nativo"></a>

### Programa ArgosLang: binário nativo

O primeiro backend nativo já recebe a IR do subconjunto e emite assembly x86-64/Linux, que `cc` monta e liga. Veja o comando completo em [Compilar ArgosLang](#compilar). Arquitetura de CPU, sistema operacional, formato de objeto, ABI, runtime e linker fazem parte do alvo: esse binário não roda automaticamente em ARM/macOS ou AVR. Os módulos ARM64, RISC-V64 e AVR ainda são esboços.

Para uma placa ou máquina diferente do host, será preciso implementar a toolchain de **cross-compilation**, incluindo geração de objeto, relocação, linkagem, entry point e suporte de runtime adequado. No caso de bare metal, também serão necessários mapa de memória, startup code e interfaces com periféricos. Não há procedimento de flash ou imagem de firmware ArgosLang disponível neste estágio.

No alvo x86-64/Linux atual, `ctest` compila e executa os programas mínimos suportados, comparando os status observados. Para qualquer novo alvo, repita esse teste no hardware ou emulador correspondente. `argosc check` sozinho não substitui execução real.
