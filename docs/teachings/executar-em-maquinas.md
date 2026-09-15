# Executar ArgosLang em diferentes máquinas

Hoje você pode executar **o compilador `argosc`** em um host preparado para construí-lo e executar **um subconjunto de programas `.argos`** como binário nativo em x86-64/Linux. A VM e outros alvos ainda não executam programas ArgosLang.

## Host: Linux, macOS e Windows

O projeto usa CMake, C++23 e ICU. Em cada sistema, instale uma toolchain C++23 e o pacote de desenvolvimento ICU apropriado; depois configure e construa como em [compilar.md](compilar.md). O repositório não fornece binários pré-compilados nem uma matriz de plataformas já validada. Um build que conclui no host comprova apenas que `argosc` roda **naquele host**.

Execute, a partir da raiz do projeto:

```bash
./build/bin/argosc check examples/minimal_loop.argos
```

Em Windows, o caminho e o sufixo do executável dependem do gerador CMake escolhido; localize `argosc.exe` na pasta de saída do build. Caso o CMake não encontre ICU, instale a biblioteca de desenvolvimento e configure novamente. Caso falhe ao obter Catch2, use `-DARGOS_BUILD_TESTS=OFF` para construir somente o CLI.

## Programa ArgosLang: VM portátil

O caminho planejado é distribuir bytecode ArgosLang e uma VM **implementada para cada host**. O mesmo bytecode só será portátil se o formato, a ABI, o runtime e as dependências do programa forem compatíveis. Para dispositivos restritos, a VM também precisa caber em memória, atender aos requisitos de tempo e expor as operações de hardware exigidas. Nada disso está validado hoje; consulte [interpretar.md](interpretar.md).

## Programa ArgosLang: binário nativo

O primeiro backend nativo já recebe a IR do subconjunto e emite assembly x86-64/Linux, que `cc` monta e liga. Veja o comando completo em [compilar.md](compilar.md). Arquitetura de CPU, sistema operacional, formato de objeto, ABI, runtime e linker fazem parte do alvo: esse binário não roda automaticamente em ARM/macOS ou AVR. Os módulos ARM64, RISC-V64 e AVR ainda são esboços.

Para uma placa ou máquina diferente do host, será preciso implementar a toolchain de **cross-compilation**, incluindo geração de objeto, relocação, linkagem, entry point e suporte de runtime adequado. No caso de bare metal, também serão necessários mapa de memória, startup code e interfaces com periféricos. Não há procedimento de flash ou imagem de firmware ArgosLang disponível neste estágio.

No alvo x86-64/Linux atual, `ctest` compila e executa os programas mínimos suportados, comparando os status observados. Para qualquer novo alvo, repita esse teste no hardware ou emulador correspondente. `argosc check` sozinho não substitui execução real.
