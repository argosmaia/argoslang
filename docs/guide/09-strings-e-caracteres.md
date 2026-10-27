# Strings e caracteres

Uma string é um texto inteiro. Escreva o texto entre aspas duplas. Ela pode
ficar em uma variável, ser enviada para uma função e voltar como resultado:

```argos
function identity(_ text: string) -> string {
    return text
}

var greeting: string = identity("Oi!")
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
var letter: char = 'A'
```

Um `char` não é uma palavra inteira. `'A'` é um caractere; `"A"` é uma string
que contém esse caractere. Também são aceitos caracteres Unicode, como `'á'`, e
escapes como `'\n'`.

O MVP executa strings normais e de várias linhas, inclusive escapes Unicode.
`==` e `!=` comparam o conteúdo da string. Ainda não existe operador de
concatenação.

O tipo `char` é reconhecido pelo frontend, mas ainda não pode ser guardado e
usado por um programa executável. Além disso, variáveis `string` ainda não
continuam guardadas de uma entrada para outra no REPL; use-as dentro de uma
mesma entrada ou em um arquivo `.argos`.
