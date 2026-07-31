
### Características a implementar

O lexer adota elementos da estrutura léxica do Swift: *maximal munch*, comentários `/* */` aninháveis, identificadores escapados com crases e um lexer manual. :contentReference[oaicite:1]{index=1}

Ele reconhece:

- Identificadores e palavras-chave;
- Identificadores escapados, como `` `class` ``;
- Inteiros decimais, binários, octais e hexadecimais;
- Separadores numéricos, como `1_000_000`;
- Literais `float` com expoente;
- Strings normais e multilinha com `"""`;
- Caracteres ASCII, UTF-8 e escapes como `'\u{1F600}'`;
- Comentários `//` e comentários de bloco aninháveis;
- Localização completa: offset, linha e coluna;
- Tokens de erro com mensagem diagnóstica;
- Quebras de linha para implementar `;` opcional no parser.

Também inclui os operadores específicos de null-safety:

```text
?
?.
?[
?=
?==
??
??=
!
!!
!!=
```

KwString
Question
Identifier
Equal
KwNull
NullCoalescingAssign
SafeAssign
ForceUnwrap
Dot