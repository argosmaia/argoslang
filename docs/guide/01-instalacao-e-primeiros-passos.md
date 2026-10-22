# Instalação e primeiros passos

## Dependências

No Ubuntu ou Pop!_OS:

```bash
sudo apt update
sudo apt install -y build-essential cmake ninja-build g++ libicu-dev libreadline-dev
```

## Build

Na raiz do repositório:

```bash
cmake -S . -B build -G Ninja \
  -DARGOS_BUILD_TESTS=OFF \
  -DCMAKE_CXX_COMPILER=g++
cmake --build build
```

### Build com Clang

O sistema pode ter um `clang++` do Swift antes do Clang do Ubuntu no `PATH`.
Use o preset do projeto para selecionar explicitamente o Clang do sistema:

```bash
cmake --preset clang-system
cmake --build --preset clang-system
```

Ele gera o compilador em `./build-clang/bin/argosc` usando
`/usr/bin/clang++` (Clang 18), sem depender do `clang++` do Swift.

O executável gerado é `./build/bin/argosc`. A mensagem `ninja: no work to do`
significa apenas que o build já está atualizado.

## Usar no terminal

Durante a sessão atual:

```bash
export PATH="$PATH:$HOME/Projetos/argoslang/build/bin"
```

Para manter no Bash:

```bash
echo 'export PATH="$PATH:$HOME/Projetos/argoslang/build/bin"' >> ~/.bashrc
source ~/.bashrc
```

Depois disso, use `argosc`. Se o repositório estiver em outro diretório, ajuste
o caminho; alternativamente, continue usando `./build/bin/argosc`.
