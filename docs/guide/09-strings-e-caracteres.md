# Strings e caracteres

Uma string é um texto inteiro. Escreva o texto entre aspas duplas. Ela pode
ficar em uma variável, ser enviada para uma função e voltar como resultado:

```argos
function identity(_ text: string) -> string {
    return text
}

let greeting: string = identity("Oi!")
print(greeting)
```

O MVP consegue imprimir um literal de texto diretamente:

```argos
print("Olá, ArgosLang!")
```

`print` aceita vários argumentos e os escreve em sequência, sem colocar um
separador automático entre eles. Se quiser um espaço, coloque-o no texto:

```argos
print("Oi, ", "Lia")
```

## Quebras de linha e caracteres especiais

Uma string normal não pode atravessar uma linha. Use `\n` para pular uma linha,
`\"` para escrever aspas e `\\` para escrever uma barra invertida. O formato
`\u{...}` permite escrever um caractere Unicode pelo seu código:

```argos
print("Primeira linha\nSegunda linha")
print("Ela disse: \"oi!\"")
print("Ol\u{e1}")
```

O lexer e o parser também reconhecem strings de várias linhas, entre três
aspas:

```argos
print("""Este texto
tem duas linhas.""")
```

## Um caractere

O tipo `char` representa um único caractere. Use aspas simples:

```argos
let letter: char = 'A'
```

Um `char` não é uma palavra inteira. `'A'` é um caractere; `"A"` é uma string
que contém esse caractere. Também são aceitos caracteres Unicode, como `'á'`, e
escapes como `'\n'`.

O MVP executa strings normais e de várias linhas, inclusive escapes Unicode.
`==` e `!=` comparam o conteúdo da string. Ainda não existe operador de
concatenação.

`char` representa um escalar Unicode, executa na VM e no nativo, pode ser
passado/retornado por funções e impresso. Na IR atual ele usa uma string UTF-8
imutável com um único escalar; isso não define uma ABI C para caracteres.
Variáveis `char` e `string` ainda não são preservadas entre entradas do REPL; use-as dentro de uma
mesma entrada ou em um arquivo `.argos`.
